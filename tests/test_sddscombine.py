import shutil
import subprocess
from pathlib import Path
import textwrap
import pytest

from sdds_test_utils import BIN_DIR
SDDSCOMBINE = BIN_DIR / "sddscombine"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"
EXAMPLE = Path("SDDSlib/demo/example.sdds")
COLUMNS = "shortCol,ushortCol,longCol,ulongCol,long64Col,ulong64Col,floatCol,doubleCol,longdoubleCol,stringCol,charCol"

PAGE1 = textwrap.dedent(
  """
  1 1 100 100 100 100  1.10000002e+00 1.001000000000000e+01 1.001000000000000000e+01 one a
  2 2 200 200 200 200  2.20000005e+00 2.002000000000000e+01 2.002000000000000000e+01 two b
  3 3 300 300 300 300  3.29999995e+00 3.003000000000000e+01 3.003000000000000000e+01 three c
  4 4 400 400 400 400  4.40000010e+00 4.004000000000000e+01 4.004000000000000000e+01 four d
  5 5 500 500 500 500  5.50000000e+00 5.005000000000000e+01 5.005000000000000000e+01 five e
  """
).strip()

PAGE2 = textwrap.dedent(
  """
  6 6 600 600 600 600  6.59999990e+00 6.006000000000000e+01 6.006000000000000000e+01 six f
  7 7 700 700 700 700  7.69999981e+00 7.006999999999999e+01 7.007000000000000000e+01 seven g
  8 8 800 800 800 800  8.80000019e+00 8.008000000000000e+01 8.008000000000000000e+01 eight h
  """
).strip()

MERGED_ALL = "\n".join([PAGE1, PAGE2, PAGE1, PAGE2])
MERGE_PARAM_PAGE1 = PAGE1
MERGE_PARAM_PAGE2 = "\n".join([PAGE2, PAGE1])
MERGE_PARAM_PAGE3 = PAGE2
SPARSE_PAGE1 = "\n".join(["1", "3", "5"])
SPARSE_PAGE2 = "\n".join(["6", "8"])
PIPE_SHORTCOL = "\n".join(["1","2","3","4","5","6","7","8","1","2","3","4","5","6","7","8"])
COLLAPSE_COLUMNS = [
  "shortParam",
  "ushortParam",
  "longParam",
  "ulongParam",
  "long64Param",
  "ulong64Param",
  "floatParam",
  "doubleParam",
  "longdoubleParam",
  "stringParam",
  "charParam",
  "Filename",
  "NumberCombined",
  "PageNumber",
]

required = [SDDSCOMBINE, SDDSCHECK, SDDS2STREAM, SDDSQUERY]
pytestmark = pytest.mark.skipif(not all(p.exists() for p in required), reason="tools not built")


def _sddscheck_ok(path: Path) -> str:
  result = subprocess.run([str(SDDSCHECK), str(path)], capture_output=True, text=True)
  return result.stdout.strip()


def _stream(path: Path, page=None, columns=COLUMNS) -> str:
  args = [str(SDDS2STREAM), str(path), f"-columns={columns}"]
  if page is not None:
    args.insert(2, f"-page={page}")
  result = subprocess.run(args, capture_output=True, text=True, check=True)
  return result.stdout.strip()


def _split_rows(text: str):
  rows = []
  for line in text.splitlines():
    values = line.split()
    rows.append(([float(value) for value in values[:9]], values[9:]))
  return rows


def _assert_stream_matches(actual: str, expected: str):
  actual_rows = _split_rows(actual)
  expected_rows = _split_rows(expected)
  assert len(actual_rows) == len(expected_rows)
  for (actual_numbers, actual_strings), (expected_numbers, expected_strings) in zip(actual_rows, expected_rows):
    assert actual_numbers == pytest.approx(expected_numbers)
    assert actual_strings == expected_strings


def _column_list(path: Path):
  result = subprocess.run([str(SDDSQUERY), str(path), "-columnList"], capture_output=True, text=True, check=True)
  return result.stdout.splitlines()


def _parameter_list(path: Path):
  result = subprocess.run([str(SDDSQUERY), str(path), "-parameterList"], capture_output=True, text=True, check=True)
  return result.stdout.splitlines()


def _npages(path: Path) -> int:
  result = subprocess.run([str(SDDS2STREAM), str(path), "-npages"], capture_output=True, text=True, check=True)
  return int(result.stdout.split()[0])



def test_overwrite(tmp_path):
  output = tmp_path / "combine.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      "-threads=2",
    ],
    check=True,
  )
  assert _sddscheck_ok(output) == "ok"
  for i, expected in enumerate([PAGE1, PAGE2, PAGE1, PAGE2], start=1):
    _assert_stream_matches(_stream(output, page=i), expected)


def test_merge(tmp_path):
  output = tmp_path / "merge.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      "-merge",
    ],
    check=True,
  )
  assert _sddscheck_ok(output) == "ok"
  _assert_stream_matches(_stream(output), MERGED_ALL)


def test_merge_parameter(tmp_path):
  output = tmp_path / "merge_param.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      "-merge=stringParam",
    ],
    check=True,
  )
  assert _sddscheck_ok(output) == "ok"
  assert _npages(output) == 3
  _assert_stream_matches(_stream(output, page=1), MERGE_PARAM_PAGE1)
  _assert_stream_matches(_stream(output, page=2), MERGE_PARAM_PAGE2)
  _assert_stream_matches(_stream(output, page=3), MERGE_PARAM_PAGE3)


def test_append(tmp_path):
  base = tmp_path / "base.sdds"
  shutil.copy(EXAMPLE, base)
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(base),
      str(EXAMPLE),
      "-append",
    ],
    check=True,
  )
  assert _sddscheck_ok(base) == "ok"
  assert _npages(base) == 4
  for i, expected in enumerate([PAGE1, PAGE2, PAGE1, PAGE2], start=1):
    _assert_stream_matches(_stream(base, page=i), expected)


def test_pipe():
  result = subprocess.run([str(SDDSCOMBINE), str(EXAMPLE), str(EXAMPLE), "-pipe=output"], stdout=subprocess.PIPE, check=True)
  stream = subprocess.run([str(SDDS2STREAM), "-pipe", "-columns=shortCol"], input=result.stdout, stdout=subprocess.PIPE, check=True)
  assert stream.stdout.decode().splitlines() == PIPE_SHORTCOL.splitlines()


def test_delete_column(tmp_path):
  output = tmp_path / "delete.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      "-delete=column,longCol",
    ],
    check=True,
  )
  assert "longCol" not in _column_list(output)


def test_retain_column(tmp_path):
  output = tmp_path / "retain.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      "-retain=column,shortCol",
    ],
    check=True,
  )
  assert _column_list(output) == ["shortCol"]


def test_delete_parameter(tmp_path):
  output = tmp_path / "delparam.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      "-delete=parameter,longParam",
    ],
    check=True,
  )
  params = _parameter_list(output)
  assert "longParam" not in params


def test_sparse(tmp_path):
  output = tmp_path / "sparse.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      "-sparse=2",
      "-threads=2",
    ],
    check=True,
  )
  assert _stream(output, page=1, columns="shortCol") == SPARSE_PAGE1
  assert _stream(output, page=2, columns="shortCol") == SPARSE_PAGE2
  assert _stream(output, page=3, columns="shortCol") == SPARSE_PAGE1
  assert _stream(output, page=4, columns="shortCol") == SPARSE_PAGE2


def test_collapse(tmp_path):
  output = tmp_path / "collapse.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      "-collapse",
    ],
    check=True,
  )
  assert _npages(output) == 1
  assert _column_list(output) == COLLAPSE_COLUMNS
  assert _stream(output, columns="shortParam") == "10\n20\n10\n20"


@pytest.mark.parametrize("recover", ["-recover", "-recover=clip"])
def test_recover(tmp_path, recover):
  output = tmp_path / f"{recover.replace('=','_')}.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      recover,
    ],
    check=True,
  )
  assert _npages(output) == 4
  assert _sddscheck_ok(output) == "ok"


def test_major_order(tmp_path):
  col = tmp_path / "colmajor.sdds"
  row = tmp_path / "rowmajor.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(col),
      "-overWrite",
      "-majorOrder=column",
    ],
    check=True,
  )
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(row),
      "-overWrite",
      "-majorOrder=row",
    ],
    check=True,
  )
  strings_col = subprocess.run(["strings", str(col)], capture_output=True, text=True, check=True)
  strings_row = subprocess.run(["strings", str(row)], capture_output=True, text=True, check=True)
  assert "column_major_order=1" in strings_col.stdout
  assert "column_major_order=1" not in strings_row.stdout


def test_xzlevel(tmp_path):
  output = tmp_path / "out.sdds.xz"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      str(EXAMPLE),
      str(EXAMPLE),
      str(output),
      "-overWrite",
      "-xzLevel=1",
    ],
    check=True,
  )
  assert _sddscheck_ok(output) == "ok"


def _write_collapse_state(path, field_type="enum", mapping="0:OFF,1:ON", values=(0,)):
  enum_values = f', enum_values="{mapping}"' if field_type == "enum" else ""
  state = (f'&parameter name=state, type={field_type}{enum_values} &end\n'
           if field_type else "")
  path.write_text(("SDDS6\n" if field_type == "enum" else "SDDS1\n") + state +
                  '&parameter name=other, type=long &end\n'
                  '&column name=x, type=long &end\n'
                  '&data mode=ascii &end\n' +
                  ''.join((f'{value}\n' if field_type else '') + '5\n1\n1\n'
                          for value in values), encoding="utf8")


@pytest.mark.parametrize("mapping", ["0:OFF,1:ON", "1:ON,0:OFF"])
def test_collapse_accepts_equal_enum_dictionaries(tmp_path, mapping):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first, values=(1, 0))
  _write_collapse_state(second, mapping=mapping, values=(0, 1))
  subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), "-collapse"], check=True)
  assert _sddscheck_ok(output) == "ok"
  assert _stream(output, columns="state").split() == ['"ON"', '"OFF"', '"OFF"', '"ON"']


@pytest.mark.parametrize("first_type,second_type,second_mapping,second_value", [
  ("enum", "enum", "0:ON,1:OFF", 0),
  ("enum", "enum", "7:OFF,8:ON", 7),
  ("enum", "enum", "0:OFF,1:ON,2:FAULT", 0),
  ("enum", "long", None, 0),
  ("long", "enum", "0:OFF,1:ON", 0),
])
def test_collapse_rejects_incompatible_enum_definitions(tmp_path, first_type,
                                                       second_type, second_mapping,
                                                       second_value):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first, field_type=first_type)
  _write_collapse_state(second, field_type=second_type, mapping=second_mapping,
                        values=(second_value,))
  result = subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), "-collapse"],
                          capture_output=True, text=True)
  assert result.returncode > 0
  assert "Incompatible enum mappings" in result.stderr


@pytest.mark.parametrize("field_type", ["long", "enum"])
def test_collapse_missing_parameters_keep_zero_default(tmp_path, field_type):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first, field_type=field_type, values=(1,))
  _write_collapse_state(second, field_type=None)
  subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), "-collapse"], check=True)
  expected = ['"ON"', '"OFF"'] if field_type == "enum" else ["1", "0"]
  assert _stream(output, columns="state").split() == expected


def test_collapse_missing_enum_requires_mapped_zero(tmp_path):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first, mapping="1:ON,2:OFF", values=(1,))
  _write_collapse_state(second, field_type=None)
  result = subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), "-collapse"],
                          capture_output=True, text=True)
  assert result.returncode > 0
  assert "mapped enum code 0" in result.stderr


def test_collapse_keeps_primitive_type_mixing(tmp_path):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first, field_type="long", values=(1,))
  _write_collapse_state(second, field_type="ulong", values=(2,))
  subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), "-collapse"], check=True)
  assert _stream(output, columns="state").split() == ["1", "2"]


@pytest.mark.parametrize("collapse", [False, True])
@pytest.mark.parametrize("delete_key", [False, True])
@pytest.mark.parametrize("reorder_mapping", [False, True])
def test_merge_enum_parameter(tmp_path, collapse, delete_key, reorder_mapping):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  mapping = "-2147483648:OFF,2147483647:ON,0:IDLE"
  second_mapping = "0:IDLE,2147483647:ON,-2147483648:OFF" if reorder_mapping else mapping
  _write_collapse_state(first, mapping=mapping, values=(2147483647, 2147483647, -2147483648))
  _write_collapse_state(second, mapping=second_mapping, values=(-2147483648, 0, 0, 2147483647))
  options = ["-merge=state"]
  if collapse:
    options.append("-collapse")
  if delete_key:
    options.append("-delete=parameter,state")
  subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), *options], check=True)
  assert _sddscheck_ok(output) == "ok"
  if collapse:
    assert _npages(output) == 1
    assert _stream(output, columns="other").split() == ["5"] * 4
    if not delete_key:
      assert _stream(output, columns="state").split() == ['"ON"', '"OFF"', '"IDLE"', '"ON"']
  else:
    assert _npages(output) == 4
    for page, rows in enumerate((2, 2, 2, 1), start=1):
      assert _stream(output, page=page, columns="x").split() == ["1"] * rows
    if not delete_key:
      result = subprocess.run([str(SDDS2STREAM), str(output), "-parameters=state"],
                              capture_output=True, text=True, check=True)
      assert result.stdout.split() == ['"ON"', '"OFF"', '"IDLE"', '"ON"']


@pytest.mark.parametrize("collapse", [False, True])
@pytest.mark.parametrize("delete_key", [False, True])
@pytest.mark.parametrize("mapping,value", [
  ("0:ON,1:OFF", 0),
  ("7:OFF,8:ON", 7),
  ("0:OFF,1:ON,2:FAULT", 0),
])
def test_merge_enum_parameter_rejects_incompatible_mappings(tmp_path, collapse, delete_key,
                                                          mapping, value):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first)
  _write_collapse_state(second, mapping=mapping, values=(value,))
  options = ["-merge=state"]
  if collapse:
    options.append("-collapse")
  if delete_key:
    options.append("-delete=parameter,state")
  result = subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), *options],
                          capture_output=True, text=True)
  assert result.returncode > 0
  assert "Incompatible enum mappings for -merge parameter" in result.stderr
