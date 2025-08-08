import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSINTERP = BIN_DIR / "sddsinterp"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

REQUIRED_TOOLS = [SDDSINTERP, PLAINDATA2SDDS, SDDSPRINTOUT]
pytestmark = pytest.mark.skipif(
  not all(tool.exists() for tool in REQUIRED_TOOLS),
  reason="sdds tools not built",
)


def create_input_sdds(tmp_path):
  x_vals = [0, 1, 3]
  y_vals = [0, 2, 6]
  plain = tmp_path / "input.txt"
  plain.write_text("\n".join(f"{x} {y}" for x, y in zip(x_vals, y_vals)) + "\n")
  sdds = tmp_path / "input.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      "-column=x,double",
      "-column=y,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return sdds


def read_output(path):
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(path),
      "-columns=x",
      "-columns=y",
      "-noTitle",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  xs, ys = [], []
  for line in result.stdout.strip().splitlines():
    parts = line.split()
    if len(parts) == 2:
      try:
        xs.append(float(parts[0]))
        ys.append(float(parts[1]))
      except ValueError:
        pass
  return xs, ys


def create_values_sdds(values, tmp_path):
  plain = tmp_path / "values.txt"
  plain.write_text("\n".join(str(v) for v in values) + "\n")
  sdds = tmp_path / "values.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      "-column=x,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return sdds


def test_at_values(tmp_path):
  input_sdds = create_input_sdds(tmp_path)
  output = tmp_path / "at.sdds"
  subprocess.run(
    [
      str(SDDSINTERP),
      str(input_sdds),
      str(output),
      "-columns=x,y",
      "-atValues=0.5,2.5",
    ],
    check=True,
  )
  x, y = read_output(output)
  assert x == pytest.approx([0.5, 2.5])
  assert y == pytest.approx([1.0, 5.0])


def test_sequence(tmp_path):
  input_sdds = create_input_sdds(tmp_path)
  output = tmp_path / "seq.sdds"
  subprocess.run(
    [
      str(SDDSINTERP),
      str(input_sdds),
      str(output),
      "-columns=x,y",
      "-sequence=4",
    ],
    check=True,
  )
  x, y = read_output(output)
  assert x == pytest.approx([0, 1, 2, 3])
  assert y == pytest.approx([0, 2, 4, 6])


def test_equispaced(tmp_path):
  input_sdds = create_input_sdds(tmp_path)
  output = tmp_path / "eq.sdds"
  subprocess.run(
    [
      str(SDDSINTERP),
      str(input_sdds),
      str(output),
      "-columns=x,y",
      "-equispaced=0.5",
    ],
    check=True,
  )
  x, y = read_output(output)
  assert x == pytest.approx([0, 0.5, 1, 1.5, 2, 2.5, 3])
  assert y == pytest.approx([0, 1, 2, 3, 4, 5, 6])


def test_fillin(tmp_path):
  input_sdds = create_input_sdds(tmp_path)
  output = tmp_path / "fill.sdds"
  subprocess.run(
    [
      str(SDDSINTERP),
      str(input_sdds),
      str(output),
      "-columns=x,y",
      "-fillIn",
    ],
    check=True,
  )
  x, y = read_output(output)
  assert x == pytest.approx([0, 1, 2, 3])
  assert y == pytest.approx([0, 2, 4, 6])


def test_file_values(tmp_path):
  input_sdds = create_input_sdds(tmp_path)
  values = create_values_sdds([0.5, 2.5], tmp_path)
  output = tmp_path / "file.sdds"
  subprocess.run(
    [
      str(SDDSINTERP),
      str(input_sdds),
      str(output),
      "-columns=x,y",
      f"-fileValues={values},column=x",
    ],
    check=True,
  )
  x, y = read_output(output)
  assert x == pytest.approx([0.5, 2.5])
  assert y == pytest.approx([1.0, 5.0])
