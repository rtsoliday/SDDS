import subprocess
from pathlib import Path
import textwrap
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
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
    ],
    check=True,
  )
  assert _sddscheck_ok(output) == "ok"
  for i, expected in enumerate([PAGE1, PAGE2, PAGE1, PAGE2], start=1):
    assert _stream(output, page=i).splitlines() == expected.splitlines()


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
  assert _stream(output).splitlines() == MERGED_ALL.splitlines()


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
  assert _stream(output, page=1).splitlines() == MERGE_PARAM_PAGE1.splitlines()
  assert _stream(output, page=2).splitlines() == MERGE_PARAM_PAGE2.splitlines()
  assert _stream(output, page=3).splitlines() == MERGE_PARAM_PAGE3.splitlines()


def test_append(tmp_path):
  base = tmp_path / "base.sdds"
  subprocess.run(["cp", str(EXAMPLE), str(base)], check=True)
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
    assert _stream(base, page=i).splitlines() == expected.splitlines()


def test_pipe():
  result = subprocess.run([str(SDDSCOMBINE), str(EXAMPLE), str(EXAMPLE), "-pipe=output"], stdout=subprocess.PIPE, check=True)
  stream = subprocess.run([str(SDDS2STREAM), "-pipe", "-columns=shortCol"], input=result.stdout, stdout=subprocess.PIPE, check=True)
  assert stream.stdout.decode().strip() == PIPE_SHORTCOL


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
