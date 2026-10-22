"""ENUM selection failures and parameter sorting of compressed SDDS6 pages."""

import gzip
import lzma
import struct
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


MAPPING = "-2147483648:zeta,2147483647:alpha,-7:mu"
CODES = (-2147483648, 2147483647, -7)
LABELS = dict(zip(CODES, ("zeta", "alpha", "mu")))
COMPRESSIONS = ("plain", "gzip", "xz", "lzma")


def run(tool, *args, check=True):
  executable = BIN_DIR / tool
  if not executable.exists():
    pytest.skip(f"{tool} not built")
  result = subprocess.run([str(executable), *map(str, args)],
                          capture_output=True, text=True, timeout=30)
  if check:
    assert result.returncode == 0, result.stdout + result.stderr
  return result


def selection_input(path, pages=1, state_type="enum"):
  mapping = f', enum_values="{MAPPING}"' if state_type == "enum" else ""
  path.write_text('SDDS6\n&column name=x, type=double &end\n'
                  '&column name=y, type=double &end\n'
                  '&column name=z, type=double &end\n'
                  f'&column name=state, type={state_type}{mapping} &end\n'
                  '&data mode=ascii &end\n' +
                  ('4\n0 0 1 -7\n1 1 0 2147483647\n'
                   '0 1 1 -2147483648\n1 0 0 -7\n') * pages,
                  encoding="utf8")


@pytest.mark.parametrize("options", [
  ("-column=x,state",), ("-column=state,x",), ("-column=x",),
  ("-column=state",), ("-column=x,y,z",),
])
@pytest.mark.parametrize("destination", ["new", "existing", "pipe"])
@pytest.mark.parametrize("pages", [0, 1])
def test_kde_requires_two_numeric_columns_before_opening_output(tmp_path, options, destination, pages):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  selection_input(source, pages)
  original = source.read_bytes()
  sentinel = b"existing destination must survive\n"
  if destination == "existing":
    output.write_bytes(sentinel)
  target = ("-pipe=output",) if destination == "pipe" else (output,)
  result = run("sddskde2d", source, *target, *options, check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "numeric columns" in result.stderr.lower()
  assert result.stdout == ""
  assert source.read_bytes() == original
  if destination == "existing":
    assert output.read_bytes() == sentinel
  else:
    assert not output.exists()


@pytest.mark.parametrize("options", [
  ("-columns=x,state",), ("-columns=state,x",), ("-columns=x,y,state",),
  ("-withOnly=state",), ("-columns=x,y", "-withOnly=state"),
])
@pytest.mark.parametrize("destination", ["new", "existing", "pipe"])
@pytest.mark.parametrize("pages", [0, 1])
def test_correlate_rejects_explicit_enum_selection_before_output(tmp_path, options, destination, pages):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  selection_input(source, pages)
  sentinel = b"existing destination must survive\n"
  if destination == "existing":
    output.write_bytes(sentinel)
  target = ("-pipe=output",) if destination == "pipe" else (output,)
  result = run("sddscorrelate", source, *target, *options, check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "state" in result.stderr
  assert "sddscast" in result.stderr
  assert result.stdout == ""
  if destination == "existing":
    assert output.read_bytes() == sentinel
  else:
    assert not output.exists()


@pytest.mark.parametrize("options,expected", [
  ((), ["x.y", "x.z", "y.z"]),
  (("-columns=*",), ["x.y", "x.z", "y.z"]),
  (("-columns=x,y,state", "-excludeColumns=sta*"), ["x.y"]),
  (("-columns=x,y", "-withOnly=x"), ["y.x"]),
])
def test_correlate_keeps_numeric_wildcards_and_exclusions(tmp_path, options, expected):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  selection_input(source)
  run("sddscorrelate", source, output, *options)
  assert run("sdds2stream", output, "-column=CorrelatePair", "-noquotes").stdout.split() == expected


@pytest.mark.parametrize("tool,options,rows", [
  ("sddskde2d", ("-column=[xy]",), 2500),
  ("sddscorrelate", ("-columns=x,state",), 1),
  ("sddscorrelate", ("-columns=x,y", "-withOnly=state"), 2),
])
def test_numeric_controls_remain_supported(tmp_path, tool, options, rows):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  selection_input(source, state_type="long")
  run(tool, source, output, *options)
  assert run("sdds2stream", output, "-rows=bare").stdout.strip() == str(rows)


def sort_input(path, encoding, compression, key_type="enum", pages=3, invalid=None):
  """Encode independently, including codes that normal writers reject."""
  binary = encoding != "ascii"
  order = ">" if encoding == "big-column" else "<"
  major = encoding == "big-column"
  mapping = f', enum_values="{MAPPING}"'
  header = (f'SDDS6\n&parameter name=mode, type={key_type}' +
            (mapping if key_type == "enum" else "") + ' &end\n' +
            f'&array name=states, type=enum{mapping} &end\n' +
            f'&column name=state, type=enum{mapping} &end\n' +
            '&column name=value, type=double &end\n' +
            f'&data mode={"binary" if binary else "ascii"}, '
            f'endian={"big" if order == ">" else "little"}, column_major_order={int(major)} &end\n')

  def page(index, bad=None):
    code = CODES[index]
    mode = 42 if bad == "parameter" else code
    states = [code, 42 if bad == "array" else -7]
    rows = [2, 0, 3][index]
    codes = [42 if bad == "column" and row == 0 else code for row in range(rows)]
    values = [10 * index + row for row in range(rows)]
    if not binary:
      return (f'{mode}\n2\n{states[0]} {states[1]}\n{rows}\n' +
              ''.join(f'{c} {v}\n' for c, v in zip(codes, values))).encode()
    wire = struct.pack(order + "5i", rows, mode, 2, *states)
    if major:
      wire += struct.pack(order + "i" * rows, *codes) + struct.pack(order + "d" * rows, *values)
    else:
      wire += b''.join(struct.pack(order + "id", c, v) for c, v in zip(codes, values))
    return wire

  wire = header.encode() + b''.join(page(index) for index in range(pages))
  if invalid:
    wire += page(0, invalid)
  if compression == "gzip":
    wire = gzip.compress(wire)
  elif compression in ("xz", "lzma"):
    wire = lzma.compress(wire, format=lzma.FORMAT_ALONE if compression == "lzma" else lzma.FORMAT_XZ)
  path.write_bytes(wire)


def compressed_path(tmp_path, compression):
  return tmp_path / ("input.sdds" + {"plain": "", "gzip": ".gz", "xz": ".xz", "lzma": ".lzma"}[compression])


@pytest.mark.parametrize("compression", COMPRESSIONS)
@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-column"])
@pytest.mark.parametrize("key_type", ["enum", "long"])
@pytest.mark.parametrize("decreasing", [False, True])
def test_parameter_sort_preserves_compressed_pages_and_dictionaries(tmp_path, compression, encoding, key_type, decreasing):
  source, output = compressed_path(tmp_path, compression), tmp_path / "sorted.sdds"
  sort_input(source, encoding, compression, key_type)
  original = source.read_bytes()
  result = run("sddssort", source, output, "-parameter=mode" + (",decreasing" if decreasing else ""))
  assert not result.stderr
  assert source.read_bytes() == original
  indices = sorted(range(3), key=lambda i: LABELS[CODES[i]] if key_type == "enum" else CODES[i], reverse=decreasing)
  expected_codes = [CODES[i] for i in indices]
  assert run("sdds2stream", output, "-parameter=mode", "-noquotes").stdout.split() == [
    LABELS[c] if key_type == "enum" else str(c) for c in expected_codes]
  assert run("sdds2stream", output, "-rows=bare").stdout.split() == [str([2, 0, 3][i]) for i in indices]
  assert run("sdds2stream", output, "-array=states", "-enumCodes", "-noquotes").stdout.split() == [
    str(c) for code in expected_codes for c in (code, -7)]
  assert run("sdds2stream", output, "-column=state", "-enumCodes", "-noquotes").stdout.split() == [
    str(CODES[i]) for i in indices for _ in range([2, 0, 3][i])]
  assert list(map(float, run("sdds2stream", output, "-column=value").stdout.split())) == [
    10 * i + row for i in indices for row in range([2, 0, 3][i])]
  assert output.read_bytes().split(b"&data", 1)[0].count(MAPPING.encode()) == (3 if key_type == "enum" else 2)
  assert run("sddscheck", output).stdout.strip() == "ok"


@pytest.mark.parametrize("compression", COMPRESSIONS)
@pytest.mark.parametrize("pages", [0, 1])
def test_parameter_sort_handles_empty_and_single_page_input(tmp_path, compression, pages):
  source, output = compressed_path(tmp_path, compression), tmp_path / "sorted.sdds"
  sort_input(source, "ascii", compression, pages=pages)
  run("sddssort", source, output, "-parameter=mode")
  assert run("sdds2stream", output, "-npages=bare").stdout.strip() == str(pages)


@pytest.mark.parametrize("compression", ["xz", "lzma"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
@pytest.mark.parametrize("in_place", [False, True])
def test_cached_parameter_sort_rejects_invalid_enum_pages(tmp_path, compression, kind, prefix, in_place):
  source, output = compressed_path(tmp_path, compression), tmp_path / "sorted.sdds"
  sort_input(source, "big-column", compression, pages=prefix, invalid=kind)
  original = source.read_bytes()
  result = run("sddssort", source, *([] if in_place else [output]), "-parameter=mode", check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower() and "42" in result.stderr
  assert source.read_bytes() == original
