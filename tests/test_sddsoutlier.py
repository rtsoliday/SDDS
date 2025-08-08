import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSOUTLIER = BIN_DIR / "sddsoutlier"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

pytestmark = pytest.mark.skipif(
  not (SDDSOUTLIER.exists() and SDDS2PLAINDATA.exists()),
  reason="required tools not built",
)

def write_sdds(tmp_path, columns):
  header = "SDDS1\n"
  for name in columns:
    header += f"&column name={name}, type=double &end\n"
  header += "&data mode=ascii, no_row_counts=1 &end\n"
  rows = len(next(iter(columns.values())))
  for i in range(rows):
    header += " ".join(str(columns[name][i]) for name in columns) + "\n"
  path = tmp_path / "input.sdds"
  path.write_text(header)
  return path

def read_column(path, column):
  plain = path.with_suffix(f".{column}.txt")
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(path),
      str(plain),
      f"-column={column}",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  text = plain.read_text().split()
  return [float(x) for x in text]

def test_stDevLimit(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,100], "y": [10,20,30]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-stDevLimit=1",
    ],
    check=True,
  )
  assert read_column(output, "x") == [1.0, 2.0]
  assert read_column(output, "y") == [10.0, 20.0]

def test_excludeColumns(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,3], "y": [10,20,100]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x,y",
      "-excludeColumns=y",
      "-stDevLimit=1",
    ],
    check=True,
  )
  assert read_column(output, "x") == [1.0,2.0,3.0]
  assert read_column(output, "y") == [10.0,20.0,100.0]

def test_absLimit(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,3,4,5]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-absLimit=4",
    ],
    check=True,
  )
  assert read_column(output, "x") == [1.0,2.0,3.0,4.0]

def test_absDeviationLimit(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [10,10,10,10,100,10,10,10,10]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-absDeviationLimit=45,neighbor=3",
    ],
    check=True,
  )
  assert read_column(output, "x") == [10.0,10.0,10.0,10.0,10.0,10.0,10.0,10.0]

def test_verbose(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,100]})
  output = tmp_path / "out.sdds"
  result = subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-stDevLimit=1",
      "-verbose",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  assert "rows in page" in result.stderr

def test_pipe(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,100], "y": [10,20,30]})
  result = subprocess.run(
    [str(SDDSOUTLIER), str(input_file), "-columns=x", "-stDevLimit=1", "-pipe=out"],
    check=True, stdout=subprocess.PIPE,
  )
  out_path = tmp_path / "pipe_out.sdds"
  out_path.write_bytes(result.stdout)
  assert read_column(out_path, "x") == [1.0,2.0]
  assert read_column(out_path, "y") == [10.0,20.0]

def test_noWarnings(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [100,200]})
  output = tmp_path / "out.sdds"
  result = subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-maximumLimit=50",
      "-noWarnings",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  assert "No rows left" not in result.stderr

def test_invert(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,100]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-stDevLimit=1",
      "-invert",
    ],
    check=True,
  )
  assert read_column(output, "x") == [100.0]

def test_markOnly(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,100]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-stDevLimit=1",
      "-markOnly",
    ],
    check=True,
  )
  assert read_column(output, "x") == [1.0,2.0,100.0]
  assert read_column(output, "IsOutlier") == [0.0,0.0,1.0]

def test_chanceLimit(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [0,0,0,100]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-chanceLimit=0.5",
    ],
    check=True,
  )
  assert read_column(output, "x") == [0.0,0.0,0.0]

def test_passes(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,100,1000]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-stDevLimit=1",
      "-passes=2",
    ],
    check=True,
  )
  assert read_column(output, "x") == [1.0,2.0]

@pytest.mark.parametrize(
  "mode,expected",
  [
    ("lastValue", [1.0,1.0,3.0]),
    ("nextValue", [1.0,3.0,3.0]),
    ("interpolatedValue", [1.0,2.0,3.0]),
    ("value=0", [1.0,0.0,3.0]),
  ],
)
def test_replaceOnly_modes(tmp_path, mode, expected):
  input_file = write_sdds(tmp_path, {"x": [1,100,3]})
  output = tmp_path / f"out_{mode}.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-stDevLimit=1",
      f"-replaceOnly={mode}",
    ],
    check=True,
  )
  assert read_column(output, "x") == expected

def test_maximum_minimum_limit(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [-5,1,10]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-maximumLimit=5",
      "-minimumLimit=0",
    ],
    check=True,
  )
  assert read_column(output, "x") == [1.0]

def test_majorOrder(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,100]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-stDevLimit=1",
      "-majorOrder=row",
    ],
    check=True,
  )
  assert read_column(output, "x") == [1.0,2.0]

def test_percentileLimit(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,2,3,4,5,6,7,8,9,10,100]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-percentileLimit=lower=20,upper=90",
    ],
    check=True,
  )
  assert read_column(output, "x") == [3.0,4.0,5.0,6.0,7.0,8.0,9.0,10.0]

def test_unpopular(tmp_path):
  input_file = write_sdds(tmp_path, {"x": [1,1,1,2,3]})
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSOUTLIER),
      str(input_file),
      str(output),
      "-columns=x",
      "-unpopular=bins=3",
    ],
    check=True,
  )
  assert read_column(output, "x") == [1.0,1.0,1.0]
