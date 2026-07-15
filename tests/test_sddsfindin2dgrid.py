import subprocess
from pathlib import Path
import pytest

from sdds_test_utils import BIN_DIR
SDDSFINDIN2DGRID = BIN_DIR / "sddsfindin2dgrid"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2STREAM = BIN_DIR / "sdds2stream"

pytestmark = pytest.mark.skipif(
  not all(p.exists() for p in [SDDSFINDIN2DGRID, PLAINDATA2SDDS, SDDS2STREAM]),
  reason="tools not built",
)


@pytest.fixture
def grid_file(tmp_path):
  plain = tmp_path / "plain.txt"
  plain.write_text(
    "0 0 0 0\n"
    "0 1 0 1\n"
    "1 0 1 0\n"
    "1 1 1 1\n"
  )
  grid = tmp_path / "grid.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(grid),
      "-column=x,double",
      "-column=y,double",
      "-column=a,double",
      "-column=b,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return grid


@pytest.fixture
def values_file(tmp_path):
  plain = tmp_path / "vals.txt"
  plain.write_text("0.2 0.8\n")
  vals = tmp_path / "vals.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(vals),
      "-column=a,double",
      "-column=b,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return vals


@pytest.fixture
def nonuniform_grid_file(tmp_path):
  plain = tmp_path / "nonuniform-plain.txt"
  plain.write_text(
    "0 0 0 0\n"
    "0 2 2 -4\n"
    "1 0 10 1\n"
    "1 2 12 -3\n"
    "4 0 40 4\n"
    "4 2 42 0\n"
  )
  grid = tmp_path / "nonuniform-grid.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(grid),
      "-column=x,double",
      "-column=y,double",
      "-column=a,double",
      "-column=b,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return grid


def stream_columns(path):
  result = subprocess.run(
    [str(SDDS2STREAM), str(path), "-columns=x,y,a,b", "-page=1"],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout


def test_atvalues(grid_file, tmp_path):
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSFINDIN2DGRID),
      str(grid_file),
      str(out),
      "-gridVariables=x,y",
      "-findLocationOf=a,b",
      "-atValues=0.2,0.8",
    ],
    check=True,
  )
  expected = (
    "0.000000000000000e+00 1.000000000000000e+00 "
    "0.000000000000000e+00 1.000000000000000e+00\n"
  )
  assert stream_columns(out) == expected


def test_values_file(grid_file, values_file, tmp_path):
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSFINDIN2DGRID),
      str(grid_file),
      str(out),
      "-gridVariables=x,y",
      "-findLocationOf=a,b",
      f"-valuesFile={values_file}",
    ],
    check=True,
  )
  expected = (
    "0.000000000000000e+00 1.000000000000000e+00 "
    "0.000000000000000e+00 1.000000000000000e+00\n"
  )
  assert stream_columns(out) == expected


def test_interpolate(grid_file, tmp_path):
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSFINDIN2DGRID),
      str(grid_file),
      str(out),
      "-gridVariables=x,y",
      "-findLocationOf=a,b",
      "-atValues=0.2,0.8",
      "-interpolate",
    ],
    check=True,
  )
  expected = (
    "2.000000000000000e-01 7.999999999999998e-01 "
    "2.000000000000000e-01 8.000000898050477e-01\n"
  )
  assert stream_columns(out) == expected


def test_pipe(grid_file, tmp_path):
  out = tmp_path / "out.sdds"
  data = grid_file.read_bytes()
  subprocess.run(
    [
      str(SDDSFINDIN2DGRID),
      "-pipe=in",
      str(out),
      "-gridVariables=x,y",
      "-findLocationOf=a,b",
      "-atValues=0.2,0.8",
    ],
    input=data,
    check=True,
  )
  expected = (
    "0.000000000000000e+00 1.000000000000000e+00 "
    "0.000000000000000e+00 1.000000000000000e+00\n"
  )
  assert stream_columns(out) == expected


def test_mode(grid_file, tmp_path):
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSFINDIN2DGRID),
      str(grid_file),
      str(out),
      "-gridVariables=x,y",
      "-findLocationOf=a,b",
      "-atValues=0.2,0.8",
      "-mode=onePairPerPage",
    ],
    check=True,
  )
  expected = (
    "0.000000000000000e+00 1.000000000000000e+00 "
    "0.000000000000000e+00 1.000000000000000e+00\n"
  )
  assert stream_columns(out) == expected
  pages = subprocess.run(
    [str(SDDS2STREAM), str(out), "-npages"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert pages.stdout == "1 pages\n"


def test_presorted(grid_file, tmp_path):
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSFINDIN2DGRID),
      str(grid_file),
      str(out),
      "-gridVariables=x,y",
      "-findLocationOf=a,b",
      "-atValues=0.2,0.8",
      "-presorted",
    ],
    check=True,
  )
  expected = (
    "0.000000000000000e+00 1.000000000000000e+00 "
    "0.000000000000000e+00 1.000000000000000e+00\n"
  )
  assert stream_columns(out) == expected


def test_inverse(grid_file, tmp_path):
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSFINDIN2DGRID),
      str(grid_file),
      str(out),
      "-gridVariables=x,y",
      "-findLocationOf=a,b",
      "-atValues=0.2,0.8",
      "-inverse",
    ],
    check=True,
  )
  expected = (
    "2.000000000000000e-01 8.000000000000000e-01 "
    "2.000000000000000e-01 8.000000000000000e-01\n"
  )
  assert stream_columns(out) == expected


def test_inverse_nonuniform_grid(nonuniform_grid_file, tmp_path):
  out = tmp_path / "nonuniform-out.sdds"
  subprocess.run(
    [
      str(SDDSFINDIN2DGRID),
      str(nonuniform_grid_file),
      str(out),
      "-gridVariables=x,y",
      "-findLocationOf=a,b",
      "-atValues=2,1",
      "-inverse",
    ],
    check=True,
  )
  result = [float(value) for value in stream_columns(out).split()]
  assert result == pytest.approx([2, 1, 21, 0])
