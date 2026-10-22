"""Exercise both production complex-pseudoinverse weight inputs without MKL."""

import gzip
import json
import lzma
import struct
import subprocess

import pytest

from sdds_test_utils import IS_WINDOWS, PLATFORM_ID, ROOT_DIR


ENCODINGS = ("ascii", "little-row", "little-column", "big-row", "big-column",
             "gzip", "xz", "lzma")
TARGETS = {"bpm": ("BPM1", "BPM2", "BPM3"), "corrector": ("C1", "C2", "C3")}


def input_path(directory, encoding, stem="weights"):
  suffix = {"gzip": ".gz", "xz": ".xz", "lzma": ".lzma"}.get(encoding, "")
  return directory / (stem + ".sdds" + suffix)


@pytest.fixture(scope="module")
def weight_driver():
  component = ROOT_DIR / "SDDSaps/pseudoInverse"
  build = subprocess.run(["make", "-C", str(component), "enum-weight-test-driver"],
                         capture_output=True, text=True)
  assert build.returncode == 0, build.stdout + build.stderr
  return component / f"O.{PLATFORM_ID}" / ("test_complex_weights.exe" if IS_WINDOWS else "test_complex_weights")


def run(driver, kind, source, other=None, page=1, quiet=False):
  unused = source.parent / "unused-file-must-not-be-opened.sdds"
  bpm = source if kind in ("bpm", "both") else unused
  corrector = other if kind == "both" else source if kind == "corrector" else unused
  return subprocess.run([str(driver), kind, str(bpm), str(corrector), str(page), str(int(quiet))],
                        capture_output=True, text=True, timeout=20)


def write_weights(path, name_type, names, values, encoding="ascii", *,
                  invalid=None, name_column="Name", value_type="double", pages=True):
  """Encode wire data independently, including unmapped ENUM codes."""
  binary = encoding != "ascii"
  order = ">" if encoding.startswith("big") else "<"
  major = encoding.endswith("column")
  codes = (-2147483648, 2147483647, -7)
  name_data = list(names)
  name_mapping = ""
  if name_type == "enum":
    # Dictionary declaration order differs from row order, and zero is unmapped.
    name_mapping = ', enum_values="' + ','.join(
        f"{codes[i]}:{names[i]}" for i in reversed(range(len(names)))) + '"'
    name_data = list(codes[:len(names)])
    if invalid is not None:
      name_data[-1] = invalid
  elif name_type != "string":
    name_data = [str(i + 1) if name_type == "character" else i + 1 for i in range(len(names))]
  value_mapping = ', enum_values="2:LOW,4:MEDIUM,8:HIGH"' if value_type == "enum" else ""
  header = ("SDDS6\n" +
            f'&column name={name_column}, type={name_type}{name_mapping} &end\n' +
            f'&column name=Weight, type={value_type}{value_mapping} &end\n' +
            f'&data mode={"binary" if binary else "ascii"}, '
            f'endian={"big" if order == ">" else "little"}, '
            f'column_major_order={int(major)} &end\n')

  def pack(value_type, value):
    if value_type == "string":
      data = value.encode("utf8")
      return struct.pack(order + "i", len(data)) + data
    if value_type == "character":
      return value.encode("ascii")
    return struct.pack(order + {"enum": "i", "long": "i", "double": "d"}[value_type], value)

  wire = header.encode()
  if pages:
    if not binary:
      wire += (str(len(names)) + '\n' + ''.join(
          (json.dumps(name) if name_type == "string" else str(name)) + f" {value}\n"
          for name, value in zip(name_data, values))).encode()
    else:
      wire += struct.pack(order + "i", len(names))
      if major:
        wire += b''.join(pack(name_type, name) for name in name_data)
        wire += b''.join(pack(value_type, value) for value in values)
      else:
        wire += b''.join(pack(name_type, name) + pack(value_type, value)
                         for name, value in zip(name_data, values))
  if encoding == "gzip":
    wire = gzip.compress(wire)
  elif encoding in ("xz", "lzma"):
    wire = lzma.compress(wire, format=lzma.FORMAT_ALONE if encoding == "lzma" else lzma.FORMAT_XZ)
  path.write_bytes(wire)


def decoded(result):
  assert result.returncode == 0, result.stdout + result.stderr
  return {parts[0]: list(map(float, parts[1:]))
          for parts in map(str.split, result.stdout.splitlines())}


@pytest.mark.parametrize("kind", TARGETS)
@pytest.mark.parametrize("name_type", ["string", "enum"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("order", [(0, 1, 2), (2, 0, 1)])
def test_weight_readers_match_labels_to_matrix_order(weight_driver, tmp_path, kind,
                                                   name_type, encoding, order):
  source = input_path(tmp_path, encoding)
  expected = [2.5, 4, 8]
  write_weights(source, name_type, [TARGETS[kind][i] for i in order],
                [expected[i] for i in order], encoding)
  result = run(weight_driver, kind, source)
  assert decoded(result) == {kind: expected}
  assert not result.stderr


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("bpm_type,corrector_type", [("enum", "enum"), ("enum", "string"),
                                                  ("string", "enum"), ("string", "string")])
def test_both_weight_inputs_keep_independent_names_and_values(weight_driver, tmp_path, encoding,
                                                            bpm_type, corrector_type):
  bpm, corrector = (input_path(tmp_path, encoding, stem) for stem in ("bpm", "corrector"))
  write_weights(bpm, bpm_type, ["BPM3", "BPM1", "BPM2"], [8, 2, 4], encoding)
  write_weights(corrector, corrector_type, ["C2", "C3", "C1"], [5, 9, 3], encoding)
  result = run(weight_driver, "both", bpm, corrector)
  assert decoded(result) == {"bpm": [2, 4, 8], "corrector": [3, 5, 9]}
  assert not result.stderr


@pytest.mark.parametrize("kind", TARGETS)
@pytest.mark.parametrize("name_type", ["long", "double", "character"])
@pytest.mark.parametrize("encoding", ["ascii", "big-column"])
def test_weight_readers_reject_numeric_name_columns(weight_driver, tmp_path, kind, name_type, encoding):
  source = input_path(tmp_path, encoding)
  write_weights(source, name_type, TARGETS[kind], [2, 4, 8], encoding)
  result = run(weight_driver, kind, source)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "string or enum" in result.stderr
  assert not result.stdout


@pytest.mark.parametrize("kind", TARGETS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("invalid", [0, 99])
def test_weight_readers_report_unmapped_enum_codes(weight_driver, tmp_path, kind, encoding, invalid):
  source = input_path(tmp_path, encoding)
  write_weights(source, "enum", TARGETS[kind], [2, 4, 8], encoding, invalid=invalid)
  result = run(weight_driver, kind, source)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower() and str(invalid) in result.stderr
  assert not result.stdout


@pytest.mark.parametrize("kind", TARGETS)
@pytest.mark.parametrize("name_type", ["string", "enum"])
@pytest.mark.parametrize("quiet", [False, True])
def test_missing_names_keep_unit_weight_and_warning_behavior(weight_driver, tmp_path, kind, name_type, quiet):
  source = tmp_path / "weights.sdds"
  # Prefixes and different case must not match an existing matrix name.
  names = [TARGETS[kind][0] + "0", TARGETS[kind][1], TARGETS[kind][2].lower()]
  write_weights(source, name_type, names, [2, 5, 8])
  result = run(weight_driver, kind, source, quiet=quiet)
  assert decoded(result) == {kind: [1, 5, 1]}
  if quiet:
    assert not result.stderr
  else:
    assert f"Name {TARGETS[kind][0]} doesn't exist" in result.stderr
    assert f"Name {TARGETS[kind][2]} doesn't exist" in result.stderr
    assert str(source) in result.stderr


@pytest.mark.parametrize("kind", TARGETS)
@pytest.mark.parametrize("pages", [False, True], ids=["no-pages", "empty-page"])
def test_weight_readers_reject_empty_inputs(weight_driver, tmp_path, kind, pages):
  source = tmp_path / "weights.sdds"
  write_weights(source, "string", [], [], pages=pages)
  result = run(weight_driver, kind, source)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "weights dataset" in result.stderr.lower()
  assert not result.stdout


@pytest.mark.parametrize("kind", TARGETS)
def test_weight_readers_reject_missing_name_column(weight_driver, tmp_path, kind):
  source = tmp_path / "weights.sdds"
  write_weights(source, "enum", TARGETS[kind], [2, 4, 8], name_column="OtherName")
  result = run(weight_driver, kind, source)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "Weight name column" in result.stderr


@pytest.mark.parametrize("kind", TARGETS)
def test_weight_values_require_numeric_conversion(weight_driver, tmp_path, kind):
  source = tmp_path / "weights.sdds"
  write_weights(source, "enum", TARGETS[kind], [2, 4, 8], value_type="enum")
  result = run(weight_driver, kind, source)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "non-numeric" in result.stderr.lower()
  assert not result.stdout


@pytest.mark.parametrize("kind", ["bpm", "corrector", "both"])
def test_weights_are_only_loaded_for_first_page(weight_driver, tmp_path, kind):
  # A later matrix page must not try to open either missing weights file.
  assert decoded(run(weight_driver, kind, tmp_path / "missing-bpm.sdds",
                     tmp_path / "missing-corrector.sdds", page=2)) == {}
