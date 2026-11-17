import shutil
import struct
import subprocess
import textwrap
from pathlib import Path

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
# The key changes at the file boundary (SecondPage -> FirstPage), so no pages merge.
MERGE_PARAM_PAGES = [PAGE1, PAGE2, PAGE1, PAGE2]
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
requires_combine_tools = pytest.mark.skipif(not all(p.exists() for p in required), reason="tools not built")


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



@requires_combine_tools
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


@requires_combine_tools
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


@requires_combine_tools
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
  assert _npages(output) == len(MERGE_PARAM_PAGES)
  for page, expected in enumerate(MERGE_PARAM_PAGES, 1):
    _assert_stream_matches(_stream(output, page=page), expected)


@requires_combine_tools
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


@requires_combine_tools
def test_pipe():
  result = subprocess.run([str(SDDSCOMBINE), str(EXAMPLE), str(EXAMPLE), "-pipe=output"], stdout=subprocess.PIPE, check=True)
  stream = subprocess.run([str(SDDS2STREAM), "-pipe", "-columns=shortCol"], input=result.stdout, stdout=subprocess.PIPE, check=True)
  assert stream.stdout.decode().splitlines() == PIPE_SHORTCOL.splitlines()


@requires_combine_tools
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


@requires_combine_tools
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


@requires_combine_tools
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


@requires_combine_tools
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


@requires_combine_tools
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


@requires_combine_tools
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


@requires_combine_tools
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


@requires_combine_tools
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


@requires_combine_tools
@pytest.mark.parametrize("mapping", ["0:OFF,1:ON", "1:ON,0:OFF"])
def test_collapse_accepts_equal_enum_dictionaries(tmp_path, mapping):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first, values=(1, 0))
  _write_collapse_state(second, mapping=mapping, values=(0, 1))
  subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), "-collapse"], check=True)
  assert _sddscheck_ok(output) == "ok"
  assert _stream(output, columns="state").split() == ['ON', 'OFF', 'OFF', 'ON']


@requires_combine_tools
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


@requires_combine_tools
@pytest.mark.parametrize("field_type", ["long", "enum"])
def test_collapse_missing_parameters_keep_zero_default(tmp_path, field_type):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first, field_type=field_type, values=(1,))
  _write_collapse_state(second, field_type=None)
  subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), "-collapse"], check=True)
  expected = ['ON', 'OFF'] if field_type == "enum" else ["1", "0"]
  assert _stream(output, columns="state").split() == expected


@requires_combine_tools
def test_collapse_missing_enum_requires_mapped_zero(tmp_path):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first, mapping="1:ON,2:OFF", values=(1,))
  _write_collapse_state(second, field_type=None)
  result = subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), "-collapse"],
                          capture_output=True, text=True)
  assert result.returncode > 0
  assert "mapped enum code 0" in result.stderr


@requires_combine_tools
def test_collapse_keeps_primitive_type_mixing(tmp_path):
  first, second, output = [tmp_path / name for name in ("first.sdds", "second.sdds", "out.sdds")]
  _write_collapse_state(first, field_type="long", values=(1,))
  _write_collapse_state(second, field_type="ulong", values=(2,))
  subprocess.run([str(SDDSCOMBINE), str(first), str(second), str(output), "-collapse"], check=True)
  assert _stream(output, columns="state").split() == ["1", "2"]


@requires_combine_tools
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
      assert _stream(output, columns="state").split() == ['ON', 'OFF', 'IDLE', 'ON']
  else:
    assert _npages(output) == 4
    for page, rows in enumerate((2, 2, 2, 1), start=1):
      assert _stream(output, page=page, columns="x").split() == ["1"] * rows
    if not delete_key:
      result = subprocess.run([str(SDDS2STREAM), str(output), "-parameters=state"],
                              capture_output=True, text=True, check=True)
      assert result.stdout.split() == ['ON', 'OFF', 'IDLE', 'ON']


@requires_combine_tools
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
  assert "Incompatible enum mapping" in result.stderr


# Regression coverage for ENUM append, merge, metadata, and recovery.

MAPPING = "0:OFF,10:ON,-1:FAULT,2147483647:MAX,-2147483648:MIN"
ENCODINGS = ("ascii", "little-row", "little-column", "big-row", "big-column")
requires_enum_tools = pytest.mark.skipif(
  not all((BIN_DIR / tool).exists() for tool in ("sddscombine", "sddscheck", "sdds2stream")),
  reason="tools not built",
)


def run(tool, *args, check=True):
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)], capture_output=True,
                          text=True, encoding="utf8", timeout=30)
  if check:
    assert result.returncode == 0, result.stderr
  return result


def codes(path, selection):
  return run("sdds2stream", path, selection, "-enumCodes", "-noquotes").stdout.split()


def dataset(path, encoding="ascii", pages=((10, (10, -1)),), fixed=None,
            name="state", mapping=MAPPING, invalid_kind=None):
  """Encode independent wire fixtures, including appendable ASCII row counts."""
  binary = encoding != "ascii"
  endian = ">" if encoding.startswith("big") else "<"
  fixed_value = f", fixed_value={fixed}" if fixed is not None else ""
  header = ("SDDS6\n"
            f'&parameter name={name}, type=enum, enum_values="{mapping}"{fixed_value} &end\n'
            f'&array name=states, type=enum, enum_values="{MAPPING}" &end\n'
            '&column name=x, type=long &end\n'
            f'&column name=rowState, type=enum, enum_values="{MAPPING}" &end\n' +
            (f'&data mode=binary, endian={"big" if endian == ">" else "little"}, '
             f'column_major_order={int(encoding.endswith("column"))} &end\n' if binary else
             '&data mode=ascii &end\n'))
  body = []
  for page_index, (key, values) in enumerate(pages):
    array, column = list(values), list(values)
    if page_index == 1:
      if invalid_kind == "parameter":
        key = 42
      elif invalid_kind == "array":
        array[0] = 42
      elif invalid_kind == "column":
        column[1] = 42
    ids = list(range(1, len(values) + 1))
    if binary:
      pack = lambda *items: struct.pack(endian + "i" * len(items), *items)
      page = pack(len(values)) + (pack(key) if fixed is None else b"")
      page += pack(len(array), *array)
      if encoding.endswith("column"):
        page += pack(*ids, *column)
      else:
        page += pack(*(item for row in zip(ids, column) for item in row))
      body.append(page)
    else:
      page = ([str(key)] if fixed is None else []) + [str(len(array))]
      if array:
        page.append(" ".join(map(str, array)))
      page.append(f"{len(values):20d}")
      page.extend(f"{row} {code}" for row, code in zip(ids, column))
      body.append(("\n".join(page) + "\n").encode())
  path.write_bytes(header.encode() + b"".join(body))
  return path


@requires_enum_tools
@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-row"])
@pytest.mark.parametrize("merge", [False, True])
@pytest.mark.parametrize("source_fixed", [None, -1])
def test_append_rejects_conflicting_fixed_enum(tmp_path, encoding, merge, source_fixed):
  first = dataset(tmp_path / "first.sdds", encoding, fixed=10)
  second = dataset(tmp_path / "second.sdds", pages=((-1, (10,)),), fixed=source_fixed)
  original = first.read_bytes()
  result = run("sddscombine", first, second, "-append", *(["-merge"] if merge else []), check=False)
  assert result.returncode > 0
  assert "fixed enum parameter state" in result.stderr
  assert first.read_bytes() == original


@requires_enum_tools
@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-row"])
@pytest.mark.parametrize("merge", [False, True])
def test_append_accepts_matching_fixed_enum(tmp_path, encoding, merge):
  first = dataset(tmp_path / "first.sdds", encoding, fixed=10)
  second = dataset(tmp_path / "second.sdds", pages=((10, (-1,)),))
  run("sddscombine", first, second, "-append", *(["-merge"] if merge else []))
  assert run("sddscheck", first).stdout.strip() == "ok"
  assert codes(first, "-parameter=state") == (["10"] if merge else ["10", "10"])
  assert codes(first, "-column=rowState") == ["10", "-1", "-1"]


@requires_enum_tools
@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-row"])
@pytest.mark.parametrize("first_key", [10, -1])
def test_append_merge_checks_destination_key_and_writes_groups(tmp_path, encoding, first_key):
  first = dataset(tmp_path / "first.sdds", encoding, pages=((0, (0,)), (10, (10,))))
  second = dataset(tmp_path / "second.sdds", pages=((first_key, (-1,)), (-1, (10,)),
                                                            (2147483647, (0,))))
  third = dataset(tmp_path / "third.sdds", pages=((2147483647, (-1,)), (-2147483648, (10,))))
  run("sddscombine", first, second, third, "-append", "-merge=state")
  assert run("sddscheck", first).stdout.strip() == "ok"
  assert codes(first, "-parameter=state") == ["0", "10", "-1", "2147483647", "-2147483648"]
  expected_rows = [1, 2, 1, 2, 1] if first_key == 10 else [1, 1, 2, 2, 1]
  for page, count in enumerate(expected_rows, 1):
    values = run("sdds2stream", first, "-column=rowState", "-enumCodes", "-noquotes",
                 f"-page={page}").stdout.split()
    assert len(values) == count
  assert codes(first, "-column=rowState") == ["0", "10", "-1", "10", "0", "-1", "10"]


@requires_enum_tools
@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-row"])
def test_append_merge_rejects_destination_mapping_conflict(tmp_path, encoding):
  first = dataset(tmp_path / "first.sdds", encoding)
  second = dataset(tmp_path / "second.sdds", mapping=MAPPING.replace("10:ON", "10:OTHER"))
  original = first.read_bytes()
  result = run("sddscombine", first, second, "-append", "-merge=state", check=False)
  assert result.returncode > 0
  assert "enum mapping" in result.stderr.lower()
  assert first.read_bytes() == original


@requires_enum_tools
@pytest.mark.parametrize("name,options", [
  ("Filename", ()),
  ("Filename", ("-threads=2",)),
  ("Filename", ("-collapse",)),
  ("NumberCombined", ()),
  ("NumberCombined", ("-threads=2",)),
  ("NumberCombined", ("-collapse",)),
  ("NumberCombined", ("-merge",)),
])
def test_generated_metadata_rejects_enum_fields(tmp_path, name, options):
  mapping = MAPPING + ",2:TWO"
  first = dataset(tmp_path / "first.sdds", name=name, mapping=mapping)
  second = dataset(tmp_path / "second.sdds", name=name, mapping=mapping)
  output = tmp_path / "out.sdds"
  result = run("sddscombine", first, second, output, *options, check=False)
  assert result.returncode > 0
  assert f"Reserved output {'column' if '-collapse' in options else 'parameter'} {name}" in result.stderr
  assert "enum" in result.stderr


@requires_enum_tools
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("recovery", ["-recover", "-recover=clip"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_recovery_clears_handled_enum_errors_before_next_file(tmp_path, encoding, recovery, kind):
  pages = ((10, (10, -1)), (-1, (0, 10)))
  first = dataset(tmp_path / "first.sdds", encoding, pages=pages, invalid_kind=kind)
  second = dataset(tmp_path / "second.sdds", encoding, pages=pages)
  output = tmp_path / "out.sdds"
  run("sddscombine", first, second, output, recovery)
  assert run("sddscheck", output).stdout.strip() == "ok"
  partial = recovery == "-recover" and kind == "column" and encoding.endswith("-row")
  expected = ["10", "-1"] + (["0"] if partial else []) + ["10", "-1", "0", "10"]
  assert codes(output, "-column=rowState") == expected


def change_field(path, kind, name, replacement):
  declaration = f'&{kind} name={name}, type=enum, enum_values="{MAPPING}"'
  path.write_text(path.read_text().replace(declaration,
                                          f'&{kind} name={name}, {replacement}'), encoding="utf8")


@requires_enum_tools
@pytest.mark.parametrize("kind,name,options", [
  (kind, name, options)
  for kind, name in [("column", "rowState"), ("parameter", "state"), ("array", "states")]
  for options in [(), ("-threads=2",), ("-merge",), ("-merge=2",), ("-merge=state",)]
] + [
  ("parameter", "state", ("-collapse",)),
  ("parameter", "state", ("-collapse", "-merge")),
  ("parameter", "state", ("-collapse", "-merge=state")),
])
@pytest.mark.parametrize("replacement", [f'type=enum, enum_values="{MAPPING.replace("10:ON", "10:OTHER")}"',
                                         "type=long"])
def test_incompatible_enum_fields_fail_before_writing(tmp_path, kind, name, options, replacement):
  first = dataset(tmp_path / "first.sdds")
  second = dataset(tmp_path / "second.sdds")
  change_field(second, kind, name, replacement)
  output = tmp_path / "out.sdds"
  result = run("sddscombine", first, second, output, *options, check=False)
  assert result.returncode > 0
  assert "enum mapping" in result.stderr.lower()
  assert output.read_bytes() == b""


@requires_enum_tools
@pytest.mark.parametrize("kind,name", [("column", "rowState"), ("parameter", "state"),
                                       ("array", "states")])
@pytest.mark.parametrize("options", [(), ("-threads=2",), ("-merge",), ("-merge=state",),
                                    ("-collapse",)])
def test_equivalent_enum_mappings_preserved_and_excluded_conflicts_allowed(tmp_path, kind, name, options):
  first = dataset(tmp_path / "first.sdds")
  second = dataset(tmp_path / "second.sdds")
  reordered = ",".join(reversed(MAPPING.split(",")))
  change_field(second, kind, name, f'type=enum, enum_values="{reordered}"')
  output = tmp_path / "out.sdds"
  run("sddscombine", first, second, output, *options)
  assert run("sddscheck", output).stdout.strip() == "ok"
  if "-collapse" not in options or kind == "parameter":
    output_kind = "column" if "-collapse" in options else kind
    assert f'enum_values="{MAPPING}"'.encode() in output.read_bytes().split(b"&data", 1)[0]
    assert codes(output, f"-{output_kind}={name}")

  change_field(first, kind, name, f'type=enum, enum_values="{MAPPING.replace("10:ON", "10:OTHER")}"')
  # An excluded merge key still determines grouping and must remain compatible.
  if kind == "parameter" and "-merge=state" in options:
    return
  run("sddscombine", first, second, output, *options, f"-delete={kind},{name}", "-overWrite")
  assert run("sddscheck", output).stdout.strip() == "ok"


@requires_enum_tools
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("statistic", [None, "average", "median", "minimum", "maximum"])
@pytest.mark.parametrize("threads", [1, 2])
def test_sparse_enum_codes_remain_categorical(tmp_path, encoding, statistic, threads):
  values = (2147483647, -2147483648, 10, -1, 0)
  first = dataset(tmp_path / "first.sdds", encoding, pages=((10, values),))
  second = dataset(tmp_path / "second.sdds", encoding, pages=((-1, values),))
  output = tmp_path / "out.sdds"
  option = "-sparse=2" + (f",{statistic}" if statistic else "")
  run("sddscombine", first, second, output, option, f"-threads={threads}")
  assert run("sddscheck", output).stdout.strip() == "ok"
  # ENUM follows the same selected rows as the integer column; statistics
  # apply only to floating-point data, never to categorical codes.
  selected_rows = codes(output, "-column=x")
  assert codes(output, "-column=rowState") == [str(values[int(row) - 1]) for row in selected_rows]
  assert len(selected_rows) in (4, 6)
  assert codes(output, "-array=states") == list(map(str, values)) * 2


@requires_enum_tools
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("collapse", [False, True])
@pytest.mark.parametrize("drop_key", [False, True])
def test_enum_merge_key_groups_across_files_at_int32_limits(tmp_path, encoding, collapse, drop_key):
  first_pages = ((2147483647, (10,)), (-2147483648, (-1,)), (-2147483648, (0,)))
  second_pages = ((-2147483648, (10,)), (-1, (-1,)), (0, (0,)))
  first = dataset(tmp_path / "first.sdds", encoding, pages=first_pages)
  second = dataset(tmp_path / "second.sdds", encoding, pages=second_pages)
  output = tmp_path / "out.sdds"
  options = ["-merge=state"] + (["-collapse"] if collapse else [])
  if drop_key:
    options.append("-delete=parameter,state")
  run("sddscombine", first, second, output, *options)
  assert run("sddscheck", output).stdout.strip() == "ok"
  if not drop_key:
    assert codes(output, "-column=state" if collapse else "-parameter=state") == [
      "2147483647", "-2147483648", "-1", "0"]
  if collapse:
    assert len(codes(output, "-column=PageNumber")) == 4
  else:
    assert codes(output, "-column=rowState") == ["10", "-1", "0", "10", "-1", "0"]
    assert [len(run("sdds2stream", output, "-column=rowState", "-enumCodes", "-noquotes",
                    f"-page={page}").stdout.split()) for page in range(1, 5)] == [1, 3, 1, 1]


@requires_enum_tools
@pytest.mark.parametrize("collapse", [False, True])
def test_excluded_enum_merge_key_still_checks_mapping(tmp_path, collapse):
  first = dataset(tmp_path / "first.sdds")
  second = dataset(tmp_path / "second.sdds", mapping=MAPPING.replace("10:ON", "10:OTHER"))
  result = run("sddscombine", first, second, tmp_path / "out.sdds", "-merge=state",
               "-delete=parameter,state", *(["-collapse"] if collapse else []), check=False)
  assert result.returncode > 0
  assert "enum mappings for -merge parameter" in result.stderr


@requires_enum_tools
@pytest.mark.parametrize("mapping", [MAPPING, MAPPING.replace("0:OFF,", "")])
@pytest.mark.parametrize("missing_first", [False, True])
def test_collapse_missing_enum_parameter_requires_mapped_zero(tmp_path, mapping, missing_first):
  first = dataset(tmp_path / "first.sdds", name="other" if missing_first else "state", mapping=mapping)
  second = dataset(tmp_path / "second.sdds", name="state" if missing_first else "other", mapping=mapping)
  output = tmp_path / "out.sdds"
  result = run("sddscombine", first, second, output, "-collapse", check=False)
  if "0:OFF" not in mapping:
    assert result.returncode > 0
    assert "requires a mapped enum code 0" in result.stderr
  else:
    assert result.returncode == 0, result.stderr
    assert run("sddscheck", output).stdout.strip() == "ok"
    assert codes(output, "-column=state") == (["0", "10"] if missing_first else ["10", "0"])


@requires_enum_tools
@pytest.mark.parametrize("kind,name", [("column", "rowState"), ("parameter", "state"),
                                       ("array", "states")])
def test_empty_input_cannot_hide_conflicting_enum_mapping(tmp_path, kind, name):
  first = dataset(tmp_path / "first.sdds")
  second = dataset(tmp_path / "empty.sdds", pages=())
  change_field(second, kind, name, f'type=enum, enum_values="{MAPPING.replace("10:ON", "10:OTHER")}"')
  output = tmp_path / "out.sdds"
  result = run("sddscombine", first, second, output, "-merge", check=False)
  assert result.returncode > 0
  assert "enum mapping" in result.stderr.lower()
  assert output.read_bytes() == b""
