import math
import subprocess
from pathlib import Path

import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSKDE2D = BIN_DIR / "sddskde2d"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED_TOOLS = [SDDSKDE2D, PLAINDATA2SDDS, SDDS2PLAINDATA]

POINTS = [(0, 0), (1, 1), (0, 1), (1, 0)]


def expected_grid(points):
  xs = [p[0] for p in points]
  ys = [p[1] for p in points]
  n = len(points)
  n_test = 50
  minx, maxx = min(xs), max(xs)
  miny, maxy = min(ys), max(ys)
  stepx = (maxx - minx) / (n_test - 1)
  stepy = (maxy - miny) / (n_test - 1)

  def bandwidth(data):
    mean = sum(data) / n
    var = sum((d - mean) ** 2 for d in data) / (n - 1)
    sigma = math.sqrt(var)
    silver = n ** (-0.16666666)
    return (silver ** 2) * (sigma ** 2)

  def gaussian(z):
    return math.exp(-z / 2.0) / (2.0 * math.pi)

  hx = bandwidth(xs)
  hy = bandwidth(ys)
  x_grid, y_grid, pdf = [], [], []
  for j in range(n_test):
    y = miny + j * stepy
    for i in range(n_test):
      x = minx + i * stepx
      x_grid.append(x)
      y_grid.append(y)
      s = 0.0
      for tx, ty in points:
        z = (tx - x) ** 2 / hx + (ty - y) ** 2 / hy
        s += gaussian(z)
      pdf.append(s / (n * math.sqrt(hx) * math.sqrt(hy)))
  return x_grid, y_grid, pdf

EXPECTED_X, EXPECTED_Y, EXPECTED_PDF = expected_grid(POINTS)


@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
class TestSDDSKDE2D:
  def create_sdds(self, tmp_path):
    plain = tmp_path / "input.txt"
    plain.write_text("\n".join(f"{x} {y}" for x, y in POINTS) + "\n")
    sdds = tmp_path / "input.sdds"
    subprocess.run([
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      "-noRowCount",
      "-column=x,double",
      "-column=y,double",
    ], check=True)
    return sdds

  def read_grid(self, path):
    plain = path.with_suffix(".txt")
    subprocess.run([
      str(SDDS2PLAINDATA),
      str(path),
      str(plain),
      "-column=x",
      "-column=y",
      "-column=PDF",
      "-separator= ",
      "-outputMode=ascii",
      "-noRowCount",
    ], check=True)
    xs, ys, pdf = [], [], []
    for line in plain.read_text().splitlines():
      a, b, c = line.split()
      xs.append(float(a))
      ys.append(float(b))
      pdf.append(float(c))
    return xs, ys, pdf

  def test_basic(self, tmp_path):
    sdds = self.create_sdds(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run([str(SDDSKDE2D), str(sdds), str(out), "-column=x,y"], check=True)
    x, y, pdf = self.read_grid(out)
    assert x == pytest.approx(EXPECTED_X)
    assert y == pytest.approx(EXPECTED_Y)
    assert pdf == pytest.approx(EXPECTED_PDF)

  def test_margin(self, tmp_path):
    sdds = self.create_sdds(tmp_path)
    out = tmp_path / "margin.sdds"
    subprocess.run([str(SDDSKDE2D), str(sdds), str(out), "-column=x,y", "-margin=0.2"], check=True)
    x, y, pdf = self.read_grid(out)
    assert x == pytest.approx(EXPECTED_X)
    assert y == pytest.approx(EXPECTED_Y)
    assert pdf == pytest.approx(EXPECTED_PDF)

  def test_samescales(self, tmp_path):
    sdds = self.create_sdds(tmp_path)
    out = tmp_path / "same.sdds"
    subprocess.run([str(SDDSKDE2D), str(sdds), str(out), "-column=x,y", "-samescales"], check=True)
    x, y, pdf = self.read_grid(out)
    assert x == pytest.approx(EXPECTED_X)
    assert y == pytest.approx(EXPECTED_Y)
    assert pdf == pytest.approx(EXPECTED_PDF)

  def test_pipe(self, tmp_path):
    sdds = self.create_sdds(tmp_path)
    proc1 = subprocess.run(
      [str(SDDSKDE2D), str(sdds), "-column=x,y", "-pipe=out"],
      stdout=subprocess.PIPE,
      check=True,
    )
    proc2 = subprocess.run(
      [
        str(SDDS2PLAINDATA),
        "-pipe",
        "-column=x",
        "-column=y",
        "-column=PDF",
        "-separator= ",
        "-outputMode=ascii",
        "-noRowCount",
      ],
      input=proc1.stdout,
      stdout=subprocess.PIPE,
      check=True,
    )
    text = proc2.stdout.decode()
    xs, ys, pdf = [], [], []
    for line in text.strip().splitlines():
      a, b, c = line.split()
      xs.append(float(a))
      ys.append(float(b))
      pdf.append(float(c))
    assert xs == pytest.approx(EXPECTED_X)
    assert ys == pytest.approx(EXPECTED_Y)
    assert pdf == pytest.approx(EXPECTED_PDF)
