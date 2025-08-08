import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSARRAY2COLUMN = BIN_DIR / "sddsarray2column"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

pytestmark = pytest.mark.skipif(
  not (SDDSARRAY2COLUMN.exists() and SDDS2PLAINDATA.exists()),
  reason="required tools not built",
)

DATASET = """SDDS1
&array name=A, type=long, dimensions=3, &end
&data mode=ascii, &end
! page number 1
2 2 2           ! 3-dimensional array A:
1 2 3 4 5 6 7 8
0
"""

def create_input(tmp_path):
  file = tmp_path / "input.sdds"
  file.write_text(DATASET)
  return file

def read_column(dataset, column="Acol"):
  result = subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(dataset),
      "-pipe=out",
      f"-column={column}",
      "-noRowCount",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  return [int(x) for x in result.stdout.split()]


def test_basic_convert(tmp_path):
  input_file = create_input(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSARRAY2COLUMN),
      str(input_file),
      str(output),
      "-convert=A,Acol",
    ],
    check=True,
  )
  assert read_column(output) == [1, 2, 3, 4, 5, 6, 7, 8]


def test_dimension_option(tmp_path):
  input_file = create_input(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSARRAY2COLUMN),
      str(input_file),
      str(output),
      "-convert=A,Acol,d0=1",
    ],
    check=True,
  )
  assert read_column(output) == [5, 6, 7, 8]


def test_nowarnings_option(tmp_path):
  input_file = create_input(tmp_path)
  output = tmp_path / "out.sdds"
  result = subprocess.run(
    [
      str(SDDSARRAY2COLUMN),
      str(input_file),
      str(output),
      "-convert=A,Acol",
      "-nowarnings",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  assert read_column(output) == [1, 2, 3, 4, 5, 6, 7, 8]


def test_pipe_option(tmp_path):
  input_file = create_input(tmp_path)
  cmd = (
    f"{SDDSARRAY2COLUMN} {input_file} -convert=A,Acol -pipe=out | "
    f"{SDDS2PLAINDATA} -pipe -column=Acol -noRowCount"
  )
  result = subprocess.run(cmd, shell=True, capture_output=True, text=True, check=True)
  values = [int(x) for x in result.stdout.split()]
  assert values == [1, 2, 3, 4, 5, 6, 7, 8]
