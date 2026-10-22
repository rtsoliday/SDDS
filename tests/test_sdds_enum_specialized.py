"""ENUM regressions for specialized tools, argument readers, and 3D arrays."""

import gzip
import json
import lzma
import struct
import subprocess
from pathlib import Path

import pytest

from sdds_test_utils import BIN_DIR, IS_WINDOWS, PLATFORM_ID, ROOT_DIR
from test_sdds_enum_read_failures import ENCODINGS, input_path, page_count, run


MAPPING = "7:OFF,10:ON"
FORMATS = {"double": "d", "float": "f", "long": "i",
           "ulong": "I", "long64": "q", "ulong64": "Q", "short": "h",
           "ushort": "H", "enum": "i", "character": "c"}
CASES = (
  ("sddsconvertalarmlog", "alarm", ("-ascii",)),
  ("sddsconvertlogonchange", "log", ("-ascii",)),
  ("sddsmatrixmult", "matrix", ("-ascii",)),
  ("sddsspotanalysis", "spot", ("-imageColumns=row?", "-spotROIsize=xvalue=3,yvalue=3",
                               "-centerOn=xcentroid,ycentroid")),
  ("sddshist2d", "grid", ("-columns=x,y", "-xparameters=4", "-yparameters=4")),
  ("sddsimageconvert", "image", ("-multicolumn=indexName=x,prefix=row", "-ascii")),
  ("sddsimageconvert", "image-single", ("-singlecolumn=imageColumn=pixels", "-ascii")),
  ("sdds2dpfit", "grid", ("-independent=x,y", "-dependent=a", "-maximumOrder=1")),
)


def configuration(name):
  parameters, arrays = [], []
  if name == "alarm":
    parameters = [("StartTime", "double", 1000), ("StartHour", "float", 0)]
    columns = [("Time", "double"), ("ControlName", "string"),
               ("AlarmStatus", "string"), ("AlarmSeverity", "string")]
    rows = [(1000, "PV1", "READ", "MINOR"), (1001, "PV2", "WRITE", "MAJOR")]
  elif name == "log":
    arrays = [("ReadbackName", "string", ["RB1", "RB2"]),
              ("ControlName", "string", ["CN1", "CN2"])]
    columns = [("Time", "double"), ("Value", "double"),
               ("ControlNameIndex", "long"), ("PreviousRow", "long")]
    rows = [(0, 100, 0, -1), (1, 200, 1, 0)]
  elif name == "matrix":
    columns, rows = [("a", "double"), ("b", "double")], [(1, 0), (0, 1)]
  elif name == "spot":
    columns = [(f"row{i}", "double") for i in range(3)]
    rows = [(0, 0, 0), (0, 10, 0), (0, 0, 0)]
  elif name == "image":
    columns = [("x", "double"), ("row0", "double"), ("row1", "double")]
    rows = [(0, 1, 2), (1, 3, 4), (2, 5, 6)]
  elif name == "image-single":
    parameters = [("Variable1Name", "string", "x"), ("Variable2Name", "string", "y")]
    for axis in ("x", "y"):
      parameters += [(axis + "Interval", "double", 1), (axis + "Minimum", "double", 0),
                     (axis + "Dimension", "long", 2)]
    columns, rows = [("pixels", "double")], [(1,), (2,), (3,), (4,)]
  elif name == "grid":
    columns = [(n, "double") for n in ("x", "y", "a", "b")]
    rows = [(0, 0, 0, 0), (0, 1, 0, 1), (1, 0, 1, 0), (1, 1, 1, 1)]
  else:
    raise ValueError(name)
  return parameters, arrays, columns, rows


def write_dataset(path, encoding, config, pages=1, invalid=None):
  """Encode wire data independently, including values a checked writer would reject."""
  parameters, arrays, columns, rows = config
  parameters = [("Mode", "enum", 10)] + parameters
  arrays = arrays + [("StateArray", "enum", [7, 10])]
  columns = columns + [("State", "enum")]
  binary = encoding != "ascii"
  order = ">" if encoding.startswith("big") else "<"
  major = encoding.endswith("column")
  header = "SDDS6\n"
  for kind, fields in (("parameter", parameters), ("array", arrays), ("column", columns)):
    for name, value_type, *_ in fields:
      extra = f', enum_values="{MAPPING}"' if value_type == "enum" else ""
      header += f'&{kind} name={name}, type={value_type}{extra} &end\n'
  header += (f'&data mode={"binary" if binary else "ascii"}, '
             f'endian={"big" if order == ">" else "little"}, '
             f'column_major_order={int(major)} &end\n')

  def value(value_type, data):
    if not binary:
      return (json.dumps(data) if isinstance(data, str) else str(data)).encode() + b"\n"
    if value_type == "string":
      text = data.encode()
      return struct.pack(order + "i", len(text)) + text
    if value_type == "character":
      data = data.encode()
    return struct.pack(order + FORMATS[value_type], data)

  def page(index, bad):
    code = 10 if index % 2 == 0 else 7
    wire = struct.pack(order + "i", len(rows)) if binary else b""
    for name, value_type, data in parameters:
      wire += value(value_type, (99 if bad == "parameter" else code) if name == "Mode" else data)
    for name, value_type, data in arrays:
      data = list(data)
      if name == "StateArray" and bad == "array":
        data[-1] = 99
      wire += struct.pack(order + "i", len(data)) if binary else f"{len(data)}\n".encode()
      wire += b''.join(value(value_type, item) for item in data)
    if not binary:
      wire += f"{len(rows)}\n".encode()
    table = [(*row, 99 if bad == "column" and i == len(rows) - 1 else code)
             for i, row in enumerate(rows)]
    if not binary:
      return wire + b''.join(b' '.join(value(t, item).rstrip(b'\n') for (_, t), item in zip(columns, row)) + b'\n'
                             for row in table)
    cells = ((t, row[c]) for c, (_, t) in enumerate(columns) for row in table) if binary and major else (
      (t, item) for row in table for (_, t), item in zip(columns, row))
    return wire + b''.join(value(t, item) for t, item in cells)

  wire = header.encode() + b''.join(page(i, None) for i in range(pages))
  if invalid:
    wire += page(pages, invalid)
  if encoding == "gzip":
    wire = gzip.compress(wire)
  elif encoding in ("xz", "lzma"):
    wire = lzma.compress(wire, format=lzma.FORMAT_ALONE if encoding == "lzma" else lzma.FORMAT_XZ)
  path.write_bytes(wire)


def command_inputs(tmp_path, case, source):
  if case[0] != "sddsmatrixmult":
    return [source]
  other = tmp_path / "matrix-other.sdds"
  write_dataset(other, "ascii", configuration("matrix"), pages=2)
  return [source, other]


def assert_enum_failure(result):
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower() and "99" in result.stderr


@pytest.mark.parametrize("case", CASES, ids=lambda c: c[0] + "-" + c[1])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
def test_specialized_enum_read_errors(tmp_path, case, encoding, kind, prefix):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  write_dataset(source, encoding, configuration(case[1]), pages=prefix, invalid=kind)
  result = run(case[0], *command_inputs(tmp_path, case, source), output, *case[2], check=False)
  assert_enum_failure(result)


@pytest.mark.parametrize("case", CASES, ids=lambda c: c[0] + "-" + c[1])
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_specialized_valid_enum_eof(tmp_path, case, encoding):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  write_dataset(source, encoding, configuration(case[1]), pages=2)
  run(case[0], *command_inputs(tmp_path, case, source), output, *case[2])
  assert run("sddscheck", output).stdout.strip() == "ok"
  if case[0] == "sddsspotanalysis":
    assert run("sdds2stream", output, "-column=Mode", "-enumCodes", "-noquotes").stdout.split() == ["10", "7"]
  elif case[0] not in ("sddsconvertalarmlog", "sddsconvertlogonchange"):
    assert page_count(output) == 2


@pytest.mark.parametrize("case", [c for c in CASES if c[0] in (
  "sddsconvertalarmlog", "sddsconvertlogonchange", "sddsmatrixmult", "sddsimageconvert")],
  ids=lambda c: c[0] + "-" + c[1])
@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-column", "gzip"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
def test_specialized_enum_failure_preserves_inplace_input(tmp_path, case, encoding, kind, prefix):
  source = input_path(tmp_path, encoding)
  write_dataset(source, encoding, configuration(case[1]), pages=prefix, invalid=kind)
  original = source.read_bytes()
  result = run(case[0], *command_inputs(tmp_path, case, source), *case[2], check=False)
  assert_enum_failure(result)
  assert source.read_bytes() == original
  assert not Path(str(source) + "~").exists()


@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-column", "gzip"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
@pytest.mark.parametrize("side", [0, 1])
@pytest.mark.parametrize("reuse", [False, True])
def test_matrix_enum_error_in_either_input(tmp_path, encoding, kind, prefix, side, reuse):
  inputs = [tmp_path / (name + "-" + input_path(tmp_path, encoding).name) for name in ("left", "right")]
  for i, path in enumerate(inputs):
    write_dataset(path, encoding, configuration("matrix"), pages=prefix if i == side else 2,
                  invalid=kind if i == side else None)
  result = run("sddsmatrixmult", *inputs, tmp_path / "output.sdds",
               *(["-reuse"] if reuse else []), check=False)
  assert_enum_failure(result)


@pytest.mark.parametrize("encoding", ["ascii", "big-column", "gzip"])
@pytest.mark.parametrize("short_side", [0, 1])
def test_matrix_reuses_last_valid_enum_page(tmp_path, encoding, short_side):
  inputs = [tmp_path / (name + "-" + input_path(tmp_path, encoding).name) for name in ("left", "right")]
  for i, path in enumerate(inputs):
    write_dataset(path, encoding, configuration("matrix"), pages=1 if i == short_side else 2)
  output = tmp_path / "output.sdds"
  run("sddsmatrixmult", *inputs, output, "-reuse")
  assert page_count(output) == 2
  values = run("sdds2stream", output, "-columns=a,b", "-noquotes").stdout.split()
  assert list(map(float, values)) == [1, 0, 0, 1] * 2


@pytest.mark.parametrize("empty_side", [0, 1])
def test_matrix_cannot_reuse_an_empty_input(tmp_path, empty_side):
  inputs = [tmp_path / name for name in ("left.sdds", "right.sdds")]
  for i, path in enumerate(inputs):
    write_dataset(path, "ascii", configuration("matrix"), pages=0 if i == empty_side else 1)
  result = run("sddsmatrixmult", *inputs, tmp_path / "out.sdds", "-reuse", check=False)
  assert result.returncode > 0
  assert "empty matrix input" in result.stderr


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
@pytest.mark.parametrize("auxiliary", [False, True])
def test_grid_enum_read_errors(tmp_path, encoding, kind, prefix, auxiliary):
  bad, primary = input_path(tmp_path, encoding), tmp_path / "grid.sdds"
  write_dataset(bad, encoding, configuration("grid"), pages=prefix, invalid=kind)
  write_dataset(primary, "ascii", configuration("grid"))
  options = ["-gridVariables=x,y", "-findLocationOf=a,b", "-mode=all"]
  options += [f"-valuesFile={bad}"] if auxiliary else ["-atValues=0.2,0.8"]
  result = run("sddsfindin2dgrid", primary if auxiliary else bad, tmp_path / "out.sdds",
               *options, check=False)
  assert_enum_failure(result)


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("auxiliary", [False, True])
def test_grid_accepts_valid_enum_eof(tmp_path, encoding, auxiliary):
  primary, output = input_path(tmp_path, encoding), tmp_path / "out.sdds"
  write_dataset(primary, encoding, configuration("grid"), pages=2)
  options = ["-gridVariables=x,y", "-findLocationOf=a,b", "-mode=all"]
  if auxiliary:
    values = tmp_path / "values.sdds"
    write_dataset(values, "ascii", ([], [], [("a", "double"), ("b", "double")], [(0.2, 0.8)]))
    options += [f"-valuesFile={values}"]
  else:
    options += ["-atValues=0.2,0.8"]
  run("sddsfindin2dgrid", primary, output, *options)
  assert len(run("sdds2stream", output, "-column=x", "-noquotes").stdout.split()) == 2
  assert run("sdds2stream", output, "-parameter=Mode", "-enumCodes", "-noquotes").stdout.strip() == "10"


@pytest.mark.parametrize("tool", ["sddsprocess", "sddsplot"])
@pytest.mark.parametrize("source_kind", ["column", "parameter"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
def test_getargs_rejects_partial_enum_argument_files(tmp_path, tool, source_kind, encoding, kind, prefix):
  args, primary, output = input_path(tmp_path, encoding), tmp_path / "data.sdds", tmp_path / "output"
  option = "-define=column,extra,1" if tool == "sddsprocess" else "-title=First"
  config = ([("arg", "string", option)], [], [("x", "double")], [(1,)]) if source_kind == "parameter" else (
    [], [], [("arg", "string")], [(option,)])
  write_dataset(args, encoding, config, pages=prefix, invalid=kind)
  write_dataset(primary, "ascii", configuration("grid"))
  output.write_bytes(b"unchanged destination")
  options = [f"-getargs={args},{source_kind}=arg"]
  options += ["-column=x,y", "-device=json", f"-output={output}"] if tool == "sddsplot" else [output]
  result = run(tool, primary, *options, check=False)
  assert_enum_failure(result)
  assert output.read_bytes() == b"unchanged destination"


@pytest.mark.parametrize("tool", ["sddsprocess", "sddsplot"])
@pytest.mark.parametrize("source_kind", ["column", "parameter"])
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_getargs_accepts_valid_enum_eof(tmp_path, tool, source_kind, encoding):
  args, primary, output = input_path(tmp_path, encoding), tmp_path / "data.sdds", tmp_path / "output"
  option = "-define=column,extra,1" if tool == "sddsprocess" else "-title=First"
  config = ([("arg", "string", option)], [], [("x", "double")], [(1,)]) if source_kind == "parameter" else (
    [], [], [("arg", "string")], [(option,)])
  write_dataset(args, encoding, config)
  write_dataset(primary, "ascii", configuration("grid"))
  options = [f"-getargs={args},{source_kind}=arg"]
  options += ["-column=x,y", "-device=json", f"-output={output}"] if tool == "sddsplot" else [output]
  run(tool, primary, *options)
  if tool == "sddsplot":
    assert "First" in json.dumps(json.loads(output.read_text()))
  else:
    assert "extra" in run("sddsquery", output, "-columnList").stdout.split()


@pytest.fixture(scope="module")
def array_driver():
  directory = ROOT_DIR / "SDDSaps/sddsplots"
  subprocess.run(["make", "-C", str(directory), "array-test-driver"], check=True, capture_output=True, text=True)
  return directory / f"O.{PLATFORM_ID}" / ("test_sddsplot3d.exe" if IS_WINDOWS else "test_sddsplot3d")


def read_arrays(driver, source, names):
  return subprocess.run([str(driver), str(source), *names], capture_output=True, text=True, timeout=20)


@pytest.mark.parametrize("value_type,encoding", [
  (t, e) for t in ("double", "float", "long", "ulong", "long64", "ulong64", "short", "ushort")
  for e in ("ascii", "little-row", "big-column", "gzip")] + [("longdouble", "ascii")])
@pytest.mark.parametrize("slot", range(4))
def test_3d_numeric_arrays_keep_values(array_driver, tmp_path, value_type, slot, encoding):
  names = ["x", "y", "z", "intensity"]
  arrays = [(name, value_type if i == slot else "double", [i + 1, i + 2, i + 3])
            for i, name in enumerate(names)]
  source = input_path(tmp_path, encoding)
  write_dataset(source, encoding, ([], arrays, [("unused", "double")], [(0,)]))
  result = read_arrays(array_driver, source, names)
  assert result.returncode == 0, result.stderr
  assert [list(map(float, line.split())) for line in result.stdout.splitlines()] == [
    [3], [1, 2, 3, 4], [2, 3, 4, 5], [3, 4, 5, 6]]


@pytest.mark.parametrize("value_type,values", [("enum", [7, 10, 7]), ("string", ["A", "B", "C"]),
                                              ("character", ["a", "b", "c"])])
@pytest.mark.parametrize("slot", range(4))
@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-column", "gzip"])
def test_3d_rejects_nonnumeric_arrays(array_driver, tmp_path, value_type, values, slot, encoding):
  names = ["x", "y", "z", "intensity"]
  arrays = [(name, value_type if i == slot else "double", values if i == slot else [1, 2, 3])
            for i, name in enumerate(names)]
  source = input_path(tmp_path, encoding)
  write_dataset(source, encoding, ([], arrays, [("unused", "double")], [(0,)]))
  result = read_arrays(array_driver, source, names)
  assert result.returncode > 0
  assert f"array {names[slot]} must have numeric type" in result.stderr
  assert result.stdout == ""


@pytest.mark.parametrize("slot", [1, 2, 3])
def test_3d_rejects_unequal_array_lengths(array_driver, tmp_path, slot):
  names = ["x", "y", "z", "intensity"]
  arrays = [(name, "long", [1, 2] if i == slot else [1, 2, 3]) for i, name in enumerate(names)]
  source = tmp_path / "arrays.sdds"
  write_dataset(source, "ascii", ([], arrays, [("unused", "double")], [(0,)]))
  result = read_arrays(array_driver, source, names)
  assert result.returncode > 0
  assert f"array {names[slot]} has a different element count" in result.stderr
  assert result.stdout == ""


@pytest.mark.parametrize("count", [3, 4])
def test_3d_empty_numeric_arrays(array_driver, tmp_path, count):
  names = ["x", "y", "z", "intensity"][:count]
  source = tmp_path / "arrays.sdds"
  write_dataset(source, "ascii", ([], [(n, "long", []) for n in names], [("unused", "double")], [(0,)]))
  result = read_arrays(array_driver, source, names)
  assert result.returncode == 0, result.stderr
  assert result.stdout == "0\n"


def test_3d_coordinates_without_intensity(array_driver, tmp_path):
  names = ["x", "y", "z"]
  source = tmp_path / "arrays.sdds"
  write_dataset(source, "ascii", ([], [(n, "long", [1, 2]) for n in names], [("unused", "double")], [(0,)]))
  result = read_arrays(array_driver, source, names)
  assert result.returncode == 0, result.stderr
  assert result.stdout == "2\n1 1 1\n2 2 2\n"
