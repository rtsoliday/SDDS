"""ENUM page failures must stop processing and preserve in-place inputs."""

import gzip
import lzma
import struct
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


MAPPING = "0:OFF,10:ON,-1:FAULT"
ENCODINGS = ("ascii", "little-row", "little-column", "big-row", "big-column",
             "gzip", "xz", "lzma")
TOOLS = {
  "sddscliptails": ("-columns=y", "-fractional=0.2"),
  "sddsoutlier": ("-columns=y", "-stDevLimit=2"),
  "sddsunwrap": ("-column=y",),
  "sddsrowstats": ("-mean=Mean,x,y",),
  "sddsrunstats": ("-points=3", "-mean=y"),
  "sddsfft": ("-columns=x,y",),
  "sddsregroup": ("-newParameters=state", "-newColumns=mode"),
  "sddstranspose": (),
  "sddskde": ("-column=y",),
  "sddskde2d": ("-column=x,y",),
}
IN_PLACE = ("sddsoutlier", "sddsrowstats", "sddsregroup", "sddstranspose",
            "sddskde", "sddskde2d")


def run(tool, *args, check=True):
  if not (BIN_DIR / tool).exists():
    pytest.skip(f"{tool} not built")
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)],
                          capture_output=True, text=True, timeout=30)
  if check:
    assert result.returncode == 0, result.stdout + result.stderr
  return result


def input_path(tmp_path, encoding):
  suffix = {"gzip": ".gz", "xz": ".xz", "lzma": ".lzma"}.get(encoding, "")
  return tmp_path / ("input.sdds" + suffix)


def enum_file(path, encoding, invalid=None, valid_pages=0):
  """Encode wire data independently, including values SDDS writers reject."""
  binary = encoding != "ascii"
  order = ">" if encoding.startswith("big") else "<"
  major = encoding.endswith("column")
  header = ("SDDS6\n"
            f'&parameter name=mode, type=enum, enum_values="{MAPPING}" &end\n'
            f'&array name=states, type=enum, enum_values="{MAPPING}" &end\n'
            f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
            '&column name=x, type=long &end\n'
            '&column name=y, type=double &end\n'
            f'&data mode={"binary" if binary else "ascii"}, '
            f'endian={"big" if order == ">" else "little"}, '
            f'column_major_order={int(major)} &end\n')

  def page(bad):
    mode = 42 if bad == "parameter" else 10
    states = [0, 10, -1, 10, 0]
    rows = [(0, 0, 0.), (10, 1, 2.), (10, 2, 0.), (-1, 3, 3.), (0, 4, 0.)]
    if bad == "array":
      states[-1] = 42
    if bad == "column":
      rows[-1] = (42, 4, 0.)
    if not binary:
      return (f'{mode}\n5\n' + ' '.join(map(str, states)) + '\n5\n' +
              ''.join(f'{state} {x} {y}\n' for state, x, y in rows)).encode()
    title = struct.pack(order + "8i", 5, mode, 5, *states)
    if major:
      body = (struct.pack(order + "5i", *(r[0] for r in rows)) +
              struct.pack(order + "5i", *(r[1] for r in rows)) +
              struct.pack(order + "5d", *(r[2] for r in rows)))
    else:
      body = b''.join(struct.pack(order + "iid", *row) for row in rows)
    return title + body

  wire = header.encode() + page(None) * valid_pages
  if invalid:
    wire += page(invalid)
  if encoding == "gzip":
    wire = gzip.compress(wire)
  elif encoding in ("xz", "lzma"):
    wire = lzma.compress(wire, format=lzma.FORMAT_ALONE if encoding == "lzma" else lzma.FORMAT_XZ)
  path.write_bytes(wire)


def page_count(path):
  return int(run("sdds2stream", path, "-npages=bare").stdout.strip() or "0")


@pytest.mark.parametrize("tool", TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_enum_read_failure_is_not_eof(tmp_path, tool, encoding, kind, prefix):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  enum_file(source, encoding, kind, int(prefix))
  result = run(tool, source, output, *TOOLS[tool], check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower()
  assert "42" in result.stderr
  if tool in ("sddskde", "sddskde2d"):
    assert page_count(output) <= int(prefix)


@pytest.mark.parametrize("tool", TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("pages", [1, 2])
def test_normal_enum_eof_and_dictionary_preservation(tmp_path, tool, encoding, pages):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  enum_file(source, encoding, valid_pages=pages)
  result = run(tool, source, output, *TOOLS[tool])
  assert not result.stderr
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert page_count(output) == (5 if tool == "sddsregroup" else pages)
  if tool not in ("sddskde", "sddskde2d", "sddsregroup"):
    assert run("sdds2stream", output, "-parameter=mode", "-noquotes").stdout.split() == ["ON"] * pages
  if tool in ("sddscliptails", "sddsoutlier", "sddsunwrap", "sddsrowstats"):
    assert run("sdds2stream", output, "-column=state", "-noquotes").stdout.split() == [
        "OFF", "ON", "ON", "FAULT", "OFF"] * pages
    assert run("sdds2stream", output, "-column=state", "-enumCodes", "-noquotes").stdout.split() == [
        "0", "10", "10", "-1", "0"] * pages


@pytest.mark.parametrize("tool", IN_PLACE)
@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-column", "gzip"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_failed_enum_read_preserves_in_place_input(tmp_path, tool, encoding, kind, prefix):
  source = input_path(tmp_path, encoding)
  enum_file(source, encoding, kind, int(prefix))
  original = source.read_bytes()
  result = run(tool, source, *TOOLS[tool], check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower()
  assert source.read_bytes() == original


def naming_file(path, value_type, encoding="ascii", other_string=False, labels=("OFF", "ON"), duplicate=False):
  codes = (-2147483648, 2147483647)
  mapping = f', enum_values="{codes[0]}:{labels[0]},{codes[1]}:{labels[1]}"' if value_type == "enum" else ""
  header = (f'SDDS6\n&column name=state, type={value_type}{mapping} &end\n' +
            ('&column name=other, type=string &end\n' if other_string else '') +
            '&column name=A, type=double &end\n&column name=B, type=double &end\n')
  values = [labels[0], labels[0] if duplicate else labels[1]] if value_type == "string" else [
      codes[0], codes[0] if duplicate else codes[1]]
  rows = [(values[0], 1., 2.), (values[1], 3., 4.)]
  if encoding == "ascii":
    wire = (header + '&data mode=ascii &end\n2\n' + ''.join(
        f'{state} ' + ('unused ' if other_string else '') + f'{a} {b}\n' for state, a, b in rows)).encode()
  else:
    wire = (header + '&data mode=binary, endian=big, column_major_order=1 &end\n').encode() + struct.pack(">i", 2)
    if value_type == "string":
      wire += b''.join(struct.pack(">i", len(v.encode())) + v.encode() for v in values)
    else:
      wire += struct.pack(">2i", *values)
    if other_string:
      wire += (struct.pack(">i", 6) + b'unused') * 2
    wire += struct.pack(">2d", 1., 3.) + struct.pack(">2d", 2., 4.)
  path.write_bytes(wire)


@pytest.mark.parametrize("value_type", ["enum", "string"])
@pytest.mark.parametrize("encoding", ["ascii", "big-column"])
@pytest.mark.parametrize("other_string", [False, True])
def test_transpose_uses_requested_categorical_names(tmp_path, value_type, encoding, other_string):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  naming_file(source, value_type, encoding, other_string)
  run("sddstranspose", source, output, "-newColumnNames=state")
  assert run("sddsquery", output, "-columnlist").stdout.split() == ["OldColumnNames", "OFF", "ON"]
  assert list(map(float, run("sdds2stream", output, "-column=OFF,ON").stdout.split())) == [1., 3., 2., 4.]


@pytest.mark.parametrize("value_type", ["enum", "string"])
def test_transpose_rejects_duplicate_categorical_names(tmp_path, value_type):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  naming_file(source, value_type, duplicate=True)
  result = run("sddstranspose", source, output, "-newColumnNames=state", check=False)
  assert result.returncode > 0
  assert "Duplicate column name" in result.stderr


@pytest.mark.parametrize("name", ["A", "missing"])
@pytest.mark.parametrize("other_string", [False, True])
def test_transpose_rejects_invalid_explicit_naming_column(tmp_path, name, other_string):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  naming_file(source, "enum", other_string=other_string)
  result = run("sddstranspose", source, output, f"-newColumnNames={name}", check=False)
  assert result.returncode > 0
  assert "string or enum" in result.stderr


def test_transpose_keeps_long_enum_labels(tmp_path):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  labels = ("OFF" + "A" * 700, "ON" + "B" * 700)
  naming_file(source, "enum", labels=labels)
  run("sddstranspose", source, output, "-newColumnNames=state")
  assert run("sddsquery", output, "-columnlist").stdout.split() == ["OldColumnNames", *labels]
