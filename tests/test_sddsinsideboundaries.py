import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSINSIDEBOUNDARIES = BIN_DIR / "sddsinsideboundaries"
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

missing = [p for p in (SDDSINSIDEBOUNDARIES, SDDSMAKEDATASET, SDDSPRINTOUT) if not p.exists()]
pytestmark = pytest.mark.skipif(missing, reason="required binaries not built")

POINT_X = [5, 25, 15, 5]
POINT_Y = [5, 25, 15, 25]
BOUND1_X = [0, 10, 10, 0, 0]
BOUND1_Y = [0, 0, 10, 10, 0]
BOUND2_X = [20, 30, 30, 20, 20]
BOUND2_Y = [20, 20, 30, 30, 20]
EXPECTED = [1, 1, 0, 0]

def create_input(path: Path):
  subprocess.run(
    [
      str(SDDSMAKEDATASET),
      str(path),
      "-column=x,type=double",
      "-data=" + ",".join(map(str, POINT_X)),
      "-column=y,type=double",
      "-data=" + ",".join(map(str, POINT_Y)),
    ],
    check=True,
  )


def create_boundary(path: Path):
  cmds = [
    [
      str(SDDSMAKEDATASET),
      str(path),
      "-column=xb,type=double",
      "-data=" + ",".join(map(str, BOUND1_X)),
      "-column=yb,type=double",
      "-data=" + ",".join(map(str, BOUND1_Y)),
    ],
    [
      str(SDDSMAKEDATASET),
      str(path),
      "-append",
      "-column=xb,type=double",
      "-data=" + ",".join(map(str, BOUND2_X)),
      "-column=yb,type=double",
      "-data=" + ",".join(map(str, BOUND2_Y)),
    ],
  ]
  for cmd in cmds:
    subprocess.run(cmd, check=True)


def read_column(path: Path, column: str):
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(path),
      f"-columns={column}",
      "-noTitle",
      "-noLabels",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  return [float(x) for x in result.stdout.split()]


def create_files(tmp_path: Path):
  input_file = tmp_path / "input.sdds"
  boundary_file = tmp_path / "bound.sdds"
  create_input(input_file)
  create_boundary(boundary_file)
  return input_file, boundary_file


def test_basic_inside(tmp_path):
  input_file, boundary_file = create_files(tmp_path)
  output_file = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSINSIDEBOUNDARIES),
      str(input_file),
      str(output_file),
      "-columns=x,y",
      f"-boundary={boundary_file},xb,yb",
    ],
    check=True,
  )
  values = read_column(output_file, "InsideSum")
  assert values == EXPECTED


def test_inside_column_option(tmp_path):
  input_file, boundary_file = create_files(tmp_path)
  output_file = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSINSIDEBOUNDARIES),
      str(input_file),
      str(output_file),
      "-columns=x,y",
      f"-boundary={boundary_file},xb,yb",
      "-insideColumn=MyInside",
    ],
    check=True,
  )
  values = read_column(output_file, "MyInside")
  assert values == EXPECTED


def test_keep_inside(tmp_path):
  input_file, boundary_file = create_files(tmp_path)
  output_file = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSINSIDEBOUNDARIES),
      str(input_file),
      str(output_file),
      "-columns=x,y",
      f"-boundary={boundary_file},xb,yb",
      "-keep=inside",
    ],
    check=True,
  )
  xs = read_column(output_file, "x")
  assert xs == [5, 25]


def test_keep_outside(tmp_path):
  input_file, boundary_file = create_files(tmp_path)
  output_file = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSINSIDEBOUNDARIES),
      str(input_file),
      str(output_file),
      "-columns=x,y",
      f"-boundary={boundary_file},xb,yb",
      "-keep=outside",
    ],
    check=True,
  )
  xs = read_column(output_file, "x")
  assert xs == [15, 5]


def test_threads_option(tmp_path):
  input_file, boundary_file = create_files(tmp_path)
  output_file = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSINSIDEBOUNDARIES),
      str(input_file),
      str(output_file),
      "-columns=x,y",
      f"-boundary={boundary_file},xb,yb",
      "-threads=2",
    ],
    check=True,
  )
  values = read_column(output_file, "InsideSum")
  assert values == EXPECTED


def test_pipe_option(tmp_path):
  input_file, boundary_file = create_files(tmp_path)
  data = input_file.read_bytes()
  proc = subprocess.run(
    [
      str(SDDSINSIDEBOUNDARIES),
      "-columns=x,y",
      f"-boundary={boundary_file},xb,yb",
      "-pipe",
    ],
    input=data,
    stdout=subprocess.PIPE,
    check=True,
  )
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      "-pipe",
      "-columns=InsideSum",
      "-noTitle",
      "-noLabels",
    ],
    input=proc.stdout,
    capture_output=True,
    check=True,
  )
  values = [float(x) for x in result.stdout.decode().split()]
  assert values == EXPECTED
