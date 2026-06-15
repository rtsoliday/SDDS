import math
import subprocess
from pathlib import Path
import pytest

from sdds_test_utils import BIN_DIR
SDDSLOCALDENSITY = BIN_DIR / "sddslocaldensity"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"
SDDSQUERY = BIN_DIR / "sddsquery"

REQUIRED_TOOLS = [SDDSLOCALDENSITY, PLAINDATA2SDDS, SDDS2PLAINDATA, SDDSQUERY]

@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
class TestSDDSLocalDensity:
  def create_sdds(self, values, tmp_path, weights=None):
    plain = tmp_path / "input.txt"
    if weights is None:
      plain.write_text("\n".join(str(v) for v in values) + "\n")
      columns = ["-column=x,double"]
    else:
      plain.write_text("\n".join(f"{v} {w}" for v, w in zip(values, weights)) + "\n")
      columns = ["-column=x,double", "-column=w,double"]
    sdds = tmp_path / "input.sdds"
    subprocess.run([
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      *columns,
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ], check=True)
    return sdds

  def read_density(self, path):
    plain = path.with_suffix(".txt")
    subprocess.run([
      str(SDDS2PLAINDATA),
      str(path),
      str(plain),
      "-column=LocalDensity",
      "-outputMode=ascii",
      "-noRowCount",
    ], check=True)
    return [float(x) for x in plain.read_text().split()]

  def fraction_density(self, data, fraction):
    min_val, max_val = min(data), max(data)
    eps = fraction * (max_val - min_val)
    return [
      sum(1 for y in data if abs(x - y) <= eps)
      for x in data
    ]

  def spread_density(self, data, spread_fraction):
    min_val, max_val = min(data), max(data)
    spread = spread_fraction * (max_val - min_val)
    return [
      sum(math.exp(-((x - y) / spread) ** 2 / 2) for y in data)
      for x in data
    ]

  def kde_density(self, data):
    n = len(data)
    mean = sum(data) / n
    variance = sum((x - mean) ** 2 for x in data) / (n - 1)
    bw = (4.0 / (1 + 2.0) / n) ** (2.0 / (1 + 4.0)) * variance
    factor = 1.0 / (math.sqrt(2 * math.pi * bw) * n)
    return [
      sum(math.exp(-((x - y) ** 2) / (2 * bw)) for y in data) * factor
      for x in data
    ]

  def test_fraction(self, tmp_path):
    data = [0, 1, 2]
    input_sdds = self.create_sdds(data, tmp_path)
    output = tmp_path / "out_fraction.sdds"
    subprocess.run([
      str(SDDSLOCALDENSITY),
      str(input_sdds),
      str(output),
      "-columns=none,x",
      "-fraction=0.5",
    ], check=True)
    density = self.read_density(output)
    expected = self.fraction_density(data, 0.5)
    assert density == pytest.approx(expected)

  def test_spread(self, tmp_path):
    data = [0, 1, 2]
    input_sdds = self.create_sdds(data, tmp_path)
    output = tmp_path / "out_spread.sdds"
    subprocess.run([
      str(SDDSLOCALDENSITY),
      str(input_sdds),
      str(output),
      "-columns=none,x",
      "-spread=0.5",
    ], check=True)
    density = self.read_density(output)
    expected = self.spread_density(data, 0.5)
    assert density == pytest.approx(expected)

  def test_kde(self, tmp_path):
    data = [0, 1, 2]
    input_sdds = self.create_sdds(data, tmp_path)
    output = tmp_path / "out_kde.sdds"
    subprocess.run([
      str(SDDSLOCALDENSITY),
      str(input_sdds),
      str(output),
      "-columns=none,x",
      "-kde=bins=1000",
    ], check=True)
    density = self.read_density(output)
    expected = self.kde_density(data)
    assert density == pytest.approx(expected, rel=1e-2)

  def test_weighted_kde_and_output_name(self, tmp_path):
    data = [0, 1, 2]
    weights = [1, 2, 3]
    input_sdds = self.create_sdds(data, tmp_path, weights=weights)
    output = tmp_path / "weighted_kde.sdds"
    subprocess.run([
      str(SDDSLOCALDENSITY),
      str(input_sdds),
      str(output),
      "-columns=none,x",
      "-kde=bins=100",
      "-weight=w",
      "-output=WeightedDensity",
      "-threads=1",
      "-verbose",
    ], check=True)
    plain = output.with_suffix(".txt")
    subprocess.run([
      str(SDDS2PLAINDATA),
      str(output),
      str(plain),
      "-column=WeightedDensity",
      "-outputMode=ascii",
      "-noRowCount",
    ], check=True)
    density = [float(x) for x in plain.read_text().split()]
    assert len(density) == 3

  def test_kde_gridoutput(self, tmp_path):
    data = [0, 1, 2]
    input_sdds = self.create_sdds(data, tmp_path)
    output = tmp_path / "grid_main.sdds"
    grid = tmp_path / "grid_density.sdds"
    subprocess.run([
      str(SDDSLOCALDENSITY),
      str(input_sdds),
      str(output),
      "-columns=none,x",
      f"-kde=bins=20,gridoutput={grid}",
    ], check=True)
    result = subprocess.run([str(SDDSQUERY), str(grid), "-columnlist"], capture_output=True, text=True, check=True)
    assert "x" in result.stdout
    assert "LocalDensity" in result.stdout
