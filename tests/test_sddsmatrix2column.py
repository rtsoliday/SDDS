import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSMATRIX2COLUMN = BIN_DIR / "sddsmatrix2column"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"
SDDSQUERY = BIN_DIR / "sddsquery"

REQUIRED = [SDDSMATRIX2COLUMN, PLAINDATA2SDDS, SDDS2PLAINDATA, SDDSQUERY]


def create_input(tmp_path):
  text = tmp_path / "input.txt"
  text.write_text("2\nR1 1 3\nR2 2 4\n")
  sdds = tmp_path / "input.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(text),
      str(sdds),
      "-inputMode=ascii",
      "-separator= ",
      "-column=Row,string",
      "-column=A,double",
      "-column=B,double",
    ],
    check=True,
  )
  return sdds


def create_basic_input(tmp_path):
  text = tmp_path / "basic.txt"
  text.write_text("2\n1 3\n2 4\n")
  sdds = tmp_path / "basic.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(text),
      str(sdds),
      "-inputMode=ascii",
      "-separator= ",
      "-column=A,double",
      "-column=B,double",
    ],
    check=True,
  )
  return sdds


def read_output(path, tmp_path, root_col="Rootname", data_col="Data"):
  out = tmp_path / "out.txt"
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(path),
      str(out),
      f"-column={root_col}",
      f"-column={data_col}",
      "-separator= ",
      "-noRowCount",
      "-outputMode=ascii",
    ],
    check=True,
  )
  names = []
  values = []
  for line in out.read_text().strip().splitlines():
    name, value = line.split()
    names.append(name)
    values.append(float(value))
  return names, values


def column_names(path):
  result = subprocess.run(
    [str(SDDSQUERY), "-columnlist", str(path)],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout.strip().splitlines()


@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sddsmatrix2column not built")
def test_default_transformation(tmp_path):
  inp = create_basic_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMATRIX2COLUMN), str(inp), str(out)], check=True)
  names, values = read_output(out, tmp_path)
  assert names == ["ARow0", "ARow1", "BRow0", "BRow1"]
  assert values == [1.0, 2.0, 3.0, 4.0]


@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sddsmatrix2column not built")
def test_row_name_column(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMATRIX2COLUMN), "-rowNameColumn=Row", str(inp), str(out)], check=True)
  names, values = read_output(out, tmp_path)
  assert names == ["AR1", "AR2", "BR1", "BR2"]
  assert values == [1.0, 2.0, 3.0, 4.0]


@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sddsmatrix2column not built")
def test_custom_column_names(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSMATRIX2COLUMN),
      "-rowNameColumn=Row",
      "-dataColumnName=Value",
      "-rootnameColumnName=Label",
      str(inp),
      str(out),
    ],
    check=True,
  )
  assert column_names(out) == ["Label", "Value"]
  names, values = read_output(out, tmp_path, "Label", "Value")
  assert names == ["AR1", "AR2", "BR1", "BR2"]
  assert values == [1.0, 2.0, 3.0, 4.0]


@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sddsmatrix2column not built")
def test_major_order_row(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMATRIX2COLUMN), "-rowNameColumn=Row", "-majorOrder=row", str(inp), str(out)], check=True)
  names, values = read_output(out, tmp_path)
  assert names == ["R1A", "R1B", "R2A", "R2B"]
  assert values == [1.0, 3.0, 2.0, 4.0]


@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sddsmatrix2column not built")
def test_major_order_column(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMATRIX2COLUMN), "-rowNameColumn=Row", "-majorOrder=column", str(inp), str(out)], check=True)
  names, values = read_output(out, tmp_path)
  assert names == ["AR1", "AR2", "BR1", "BR2"]
  assert values == [1.0, 2.0, 3.0, 4.0]


@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sddsmatrix2column not built")
def test_pipe(tmp_path):
  inp = create_input(tmp_path)
  with open(inp, "rb") as f:
    result = subprocess.run(
      [str(SDDSMATRIX2COLUMN), "-rowNameColumn=Row", "-pipe"],
      input=f.read(),
      stdout=subprocess.PIPE,
      check=True,
    )
  out = tmp_path / "out.sdds"
  out.write_bytes(result.stdout)
  names, values = read_output(out, tmp_path)
  assert names == ["AR1", "AR2", "BR1", "BR2"]
  assert values == [1.0, 2.0, 3.0, 4.0]
