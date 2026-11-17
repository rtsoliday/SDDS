"""Complex pseudoinverse weights, column naming, and reconstruction tests."""

import gzip
import json
import lzma
import struct
import subprocess

import pytest

from sdds_test_utils import BIN_DIR, IS_WINDOWS, PLATFORM_ID, ROOT_DIR


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


# Exercise production complex-matrix naming and reconstruction without MKL SVD.

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
