"""Exercise production complex-matrix naming and reconstruction without MKL SVD."""

import gzip
import json
import lzma
import struct
import subprocess

import pytest

from sdds_test_utils import BIN_DIR, IS_WINDOWS, PLATFORM_ID, ROOT_DIR
from test_sddscomplexpseudoinverse_enum import ENCODINGS, input_path


CODES = (-2147483648, 2147483647)


@pytest.fixture(scope="module")
def name_driver():
  component = ROOT_DIR / "SDDSaps/pseudoInverse"
  build = subprocess.run(["make", "-C", str(component), "enum-complex-name-test-driver"],
                         capture_output=True, text=True)
  assert build.returncode == 0, build.stdout + build.stderr
  filename = "test_complex_names.exe" if IS_WINDOWS else "test_complex_names"
  return component / f"O.{PLATFORM_ID}" / filename


def write_matrix(path, value_type="enum", encoding="ascii", *, other_string=False,
                 labels=("OFF", "ON"), pages=2, invalid=None):
  """Write wire data with sparse/extreme codes, including an invalid later page."""
  binary = encoding != "ascii"
  order = ">" if encoding.startswith("big") else "<"
  major = encoding.endswith("column")
  mapping = f'{CODES[1]}:{labels[1]},{CODES[0]}:{labels[0]}'
  header = ('SDDS6\n'
            '&parameter name=mode, type=enum, enum_values="-7:IDLE,9:RUN" &end\n'
            '&array name=states, type=enum, enum_values="-7:IDLE,9:RUN" &end\n')
  columns = [("other", "string")] if other_string else []
  if value_type is not None:
    columns.append(("state", value_type))
  columns += [(name, "double") for name in ("RealA", "ImagA", "RealB", "ImagB")]
  for name, kind in columns:
    extra = f', enum_values="{mapping}"' if kind == "enum" else ""
    header += f'&column name={name}, type={kind}{extra} &end\n'
  header += (f'&data mode={"binary" if binary else "ascii"}, '
             f'endian={"big" if order == ">" else "little"}, '
             f'column_major_order={int(major)} &end\n')

  def pack(kind, value):
    if kind == "string":
      data = value.encode("utf8")
      return struct.pack(order + "i", len(data)) + data
    if kind == "character":
      return value.encode("ascii")
    return struct.pack(order + {"enum": "i", "long": "i", "double": "d"}[kind], value)

  wire = header.encode()
  for page in range(pages):
    rows = []
    for row in range(2):
      state_index = (row + page) % 2
      values = [f"unused{row}"] if other_string else []
      if value_type == "enum":
        values.append(0 if invalid == page and row == 1 else CODES[state_index])
      elif value_type == "string":
        values.append(labels[state_index])
      elif value_type is not None:
        values.append(str(row + 1) if value_type == "character" else row + 1)
      rows.append(values + [float(row == 0), 0., float(row == 1), 0.])
    if not binary:
      wire += b'9\n2\n-7 9\n2\n'
      wire += ''.join(' '.join(json.dumps(value) if isinstance(value, str) else str(value)
                               for value in row) + '\n' for row in rows).encode()
    else:
      wire += struct.pack(order + "5i", 2, 9, 2, -7, 9)
      cells = ((kind, row[column]) for column, (_, kind) in enumerate(columns) for row in rows) if major else (
          (kind, value) for row in rows for (_, kind), value in zip(columns, row))
      wire += b''.join(pack(kind, value) for kind, value in cells)
  if encoding == "gzip":
    wire = gzip.compress(wire)
  elif encoding in ("xz", "lzma"):
    wire = lzma.compress(wire, format=lzma.FORMAT_ALONE if encoding == "lzma" else lzma.FORMAT_XZ)
  path.write_bytes(wire)


def run_names(driver, source, tmp_path, requested="state", root="-"):
  return subprocess.run([str(driver), str(source), str(tmp_path / "names.sdds"),
                         str(tmp_path / "reconstructed.sdds"), requested, root],
                        capture_output=True, text=True, timeout=20)


def tool(name, *arguments):
  result = subprocess.run([str(BIN_DIR / name), *map(str, arguments)],
                          capture_output=True, text=True, timeout=20)
  assert result.returncode == 0, result.stderr
  return result.stdout


def values(path, kind, names, codes=False):
  return tool("sdds2stream", path, f"-{kind}={names}", "-noquotes",
              *(["-enumCodes"] if codes else [])).splitlines()


@pytest.mark.parametrize("value_type", ["enum", "string"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("other_string", [False, True])
def test_explicit_names_override_automatic_discovery_and_preserve_reconstruction(
    name_driver, tmp_path, value_type, encoding, other_string):
  source = input_path(tmp_path, encoding, "matrix")
  write_matrix(source, value_type, encoding, other_string=other_string)
  result = run_names(name_driver, source, tmp_path)
  assert result.returncode == 0, result.stdout + result.stderr
  assert not result.stderr
  output, rebuilt = tmp_path / "names.sdds", tmp_path / "reconstructed.sdds"
  assert tool("sddsquery", output, "-columnlist").splitlines() == [
      "RowName", "RealOFF", "ImagOFF", "RealON", "ImagON"]
  assert values(output, "column", "RowName") == ["OFF", "ON"]
  assert values(rebuilt, "column", "state") == ["OFF", "ON", "ON", "OFF"]
  if value_type == "enum":
    assert values(rebuilt, "column", "state", codes=True) == list(map(str, (*CODES, *reversed(CODES))))
    assert f'{CODES[1]}:ON,{CODES[0]}:OFF' in tool("sddsquery", rebuilt)
  assert values(rebuilt, "parameter", "mode") == ["RUN", "RUN"]
  assert [line.split() for line in values(rebuilt, "array", "states", codes=True)] == [["-7", "9"]] * 2
  assert values(rebuilt, "column", "RealA,RealB") == values(source, "column", "RealA,RealB")
  assert tool("sddsquery", rebuilt, "-columnlist") == tool("sddsquery", source, "-columnlist")
  if other_string:
    assert values(rebuilt, "column", "other") == ["unused0", "unused1"] * 2
  assert tool("sddscheck", rebuilt).strip() == "ok"


@pytest.mark.parametrize("value_type", [None, "long", "double", "character"])
@pytest.mark.parametrize("other_string", [False, True])
@pytest.mark.parametrize("pages", [0, 1])
def test_missing_and_numeric_naming_columns_fail_even_without_string_columns_or_pages(
    name_driver, tmp_path, value_type, other_string, pages):
  source = tmp_path / "matrix.sdds"
  write_matrix(source, value_type, other_string=other_string, pages=pages)
  result = run_names(name_driver, source, tmp_path)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "-newColumnNames column must exist and have string or enum type" in result.stderr
  assert not (tmp_path / "names.sdds").exists()
  assert not (tmp_path / "reconstructed.sdds").exists()


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("page", [0, 1])
def test_unmapped_enum_names_report_first_and_later_page_failures(name_driver, tmp_path, encoding, page):
  source = input_path(tmp_path, encoding, "matrix")
  write_matrix(source, encoding=encoding, invalid=page)
  result = run_names(name_driver, source, tmp_path)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower() and "0" in result.stderr
  assert not (tmp_path / "names.sdds").exists()


@pytest.mark.parametrize("labels", [("01", "1"), ("A" * 300, "B" * 300)])
@pytest.mark.parametrize("encoding", ["ascii", "big-column"])
def test_enum_labels_remain_exact_and_are_not_limited_by_old_prefix_buffers(
    name_driver, tmp_path, labels, encoding):
  source = input_path(tmp_path, encoding, "matrix")
  write_matrix(source, encoding=encoding, labels=labels, pages=1)
  result = run_names(name_driver, source, tmp_path)
  assert result.returncode == 0, result.stdout + result.stderr
  assert values(tmp_path / "names.sdds", "column", "RowName") == list(labels)
  assert tool("sddsquery", tmp_path / "names.sdds", "-columnlist").splitlines() == [
      "RowName", *(prefix + label for label in labels for prefix in ("Real", "Imag"))]
  for i, label in enumerate(labels):
    for prefix, sign in (("Real", 1), ("Imag", -1)):
      assert list(map(float, values(tmp_path / "names.sdds", "column", prefix + label))) == [
          sign * ((i + 1) * 10 + row) for row in range(2)]


@pytest.mark.parametrize("value_type,other_string,root,expected", [
    ("enum", False, "-", ["Column000", "Column001"]),
    ("string", False, "-", ["OFF", "ON"]),
    ("enum", True, "-", ["unused0", "unused1"]),
    ("string", True, "Row", ["Row000", "Row001"]),
])
def test_automatic_and_generated_names_keep_existing_defaults(
    name_driver, tmp_path, value_type, other_string, root, expected):
  source = tmp_path / "matrix.sdds"
  write_matrix(source, value_type, other_string=other_string, pages=1)
  result = run_names(name_driver, source, tmp_path, requested="-", root=root)
  assert result.returncode == 0, result.stdout + result.stderr
  assert values(tmp_path / "names.sdds", "column", "RowName") == expected


@pytest.mark.skipif(not (BIN_DIR / "sddscomplexpseudoinverse").exists(),
                    reason="optional MKL complex pseudoinverse executable not built")
@pytest.mark.parametrize("value_type", ["enum", "string"])
@pytest.mark.parametrize("other_string", [False, True])
@pytest.mark.parametrize("encoding", ["ascii", "big-column"])
def test_full_complex_pseudoinverse_names_and_reconstruction_when_built(
    tmp_path, value_type, other_string, encoding):
  source = input_path(tmp_path, encoding, "matrix")
  output, rebuilt, u_matrix = (tmp_path / name for name in ("inverse.sdds", "rebuilt.sdds", "u.sdds"))
  write_matrix(source, value_type, encoding, other_string=other_string)
  tool("sddscomplexpseudoinverse", source, output, "-newColumnNames=state",
       f"-reconstruct={rebuilt}", f"-uMatrix={u_matrix}")
  assert tool("sddsquery", output, "-columnlist").splitlines() == [
      "OldColumnNames", "RealOFF", "ImagOFF", "RealON", "ImagON"]
  for name, expected in (("RealOFF", [1., 0.]), ("RealON", [0., 1.]),
                         ("ImagOFF", [0., 0.]), ("ImagON", [0., 0.])):
    assert list(map(float, values(output, "column", name))) == pytest.approx(expected * 2)
  assert values(u_matrix, "column", "state") == ["OFF", "ON"] * 2
  assert values(rebuilt, "column", "state") == ["OFF", "ON", "ON", "OFF"]
  if value_type == "enum":
    assert values(rebuilt, "column", "state", codes=True) == list(map(str, (*CODES, *reversed(CODES))))
    assert f'{CODES[1]}:ON,{CODES[0]}:OFF' in tool("sddsquery", rebuilt)
  for name in ("RealA", "ImagA", "RealB", "ImagB"):
    assert list(map(float, values(rebuilt, "column", name))) == pytest.approx(
        list(map(float, values(source, "column", name))))
  assert tool("sddscheck", rebuilt).strip() == "ok"
