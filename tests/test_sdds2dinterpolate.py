import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDS2DINTERPOLATE = BIN_DIR / "sdds2dinterpolate"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

REQUIRED_TOOLS = [SDDS2DINTERPOLATE, PLAINDATA2SDDS, SDDSPRINTOUT]
pytestmark = pytest.mark.skipif(
  not all(tool.exists() for tool in REQUIRED_TOOLS),
  reason="sdds tools not built",
)


def create_input(tmp_path):
  points = [
    (0, 0, 0),
    (0, 2, 2),
    (2, 0, 2),
    (2, 2, 4),
  ]
  plain = tmp_path / "input.txt"
  plain.write_text("\n".join(f"{x} {y} {z}" for x, y, z in points) + "\n")
  sdds = tmp_path / "input.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      "-column=x,double",
      "-column=y,double",
      "-column=z,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return sdds


def create_points(tmp_path):
  pts = [(1, 1), (1.5, 0.5)]
  plain = tmp_path / "points.txt"
  plain.write_text("\n".join(f"{x} {y}" for x, y in pts) + "\n")
  sdds = tmp_path / "points.sdds"
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
      "-columns=z",
      "-noTitle",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  xs, ys, zs = [], [], []
  for line in result.stdout.strip().splitlines():
    parts = line.split()
    if len(parts) == 3:
      try:
        xs.append(float(parts[0]))
        ys.append(float(parts[1]))
        zs.append(float(parts[2]))
      except ValueError:
        pass
  return xs, ys, zs


@pytest.mark.parametrize("algo", ["nn", "csa"])
def test_sparse_interpolation(algo, tmp_path):
  input_sdds = create_input(tmp_path)
  points = create_points(tmp_path)
  output = tmp_path / f"out_{algo}.sdds"
  subprocess.run(
    [
      str(SDDS2DINTERPOLATE),
      str(input_sdds),
      str(output),
      "-independentColumn=xcolumn=x,ycolumn=y",
      "-dependentColumn=z",
      f"-algorithm={algo}",
      f"-file={points}",
    ],
    check=True,
  )
  x, y, z = read_output(output)
  assert x == pytest.approx([1.0, 1.5])
  assert y == pytest.approx([1.0, 0.5])
  assert z == pytest.approx([2.0, 2.0])
