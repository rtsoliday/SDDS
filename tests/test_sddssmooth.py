import math
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSMOOTH = BIN_DIR / "sddssmooth"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED_TOOLS = [SDDSSMOOTH, PLAINDATA2SDDS, SDDS2PLAINDATA]

@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
class TestSDDSSmooth:
  def create_sdds(self, values, tmp_path):
    plain = tmp_path / "input.txt"
    plain.write_text("\n".join(str(v) for v in values) + "\n")
    sdds = tmp_path / "input.sdds"
    subprocess.run([
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      "-column=col,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ], check=True)
    return sdds

  def read_sdds(self, path):
    plain = path.with_suffix(".txt")
    subprocess.run([
      str(SDDS2PLAINDATA),
      str(path),
      str(plain),
      "-column=col",
      "-outputMode=ascii",
      "-noRowCount",
    ], check=True)
    return [float(x) for x in plain.read_text().split()]

  def smooth_data(self, values, points, passes):
    data = list(values)
    half = points // 2
    for _ in range(passes):
      smoothed = [0.0] * len(data)
      total = sum(data[:half])
      terms = half
      lower = -half
      upper = half
      for row in range(len(data)):
        if upper < len(data):
          total += data[upper]
          terms += 1
        smoothed[row] = total / terms
        if lower >= 0:
          total -= data[lower]
          terms -= 1
        lower += 1
        upper += 1
      data = smoothed
    return data

  def gaussian_convolution(self, values, sigma):
    ns_per_side = int(6 * sigma)
    exp_factor = [math.exp(-((j / sigma) ** 2) / 2.0) / (sigma * math.sqrt(2 * math.pi))
                  for j in range(-ns_per_side, ns_per_side + 1)]
    rows = len(values)
    out = [0.0] * rows
    for i in range(rows):
      j1 = max(0, i - ns_per_side)
      j2 = min(rows - 1, i + ns_per_side)
      total = 0.0
      for j in range(j1, j2 + 1):
        total += values[j] * exp_factor[j - i + ns_per_side]
      out[i] = total
    return out

  def savitzky_golay(self, values, left, right, order):
    # Supports left=2,right=2,order=2 used in tests
    coef = [-3, 12, 17, 12, -3]
    coef = [c / 35 for c in coef]
    padded = [values[0]] * left + list(values) + [values[-1]] * right
    out = []
    for i in range(left, left + len(values)):
      segment = padded[i - left : i + right + 1]
      out.append(sum(c * s for c, s in zip(coef, segment)))
    return out

  def median_filter(self, values, window):
    if window % 2 == 0:
      window += 1
    half = window // 2
    out = []
    for i in range(len(values)):
      window_vals = []
      for k in range(window):
        idx = i - half + k
        if idx < 0:
          window_vals.append(values[0])
        elif idx >= len(values):
          window_vals.append(values[-1])
        else:
          window_vals.append(values[idx])
      out.append(sorted(window_vals)[half])
    return out

  def test_nearest_neighbor(self, tmp_path):
    values = [0, 0, 10, 0, 0]
    input_sdds = self.create_sdds(values, tmp_path)
    output = tmp_path / "out.sdds"
    subprocess.run([
      str(SDDSSMOOTH),
      str(input_sdds),
      str(output),
      "-columns=col",
      "-points=3",
      "-passes=1",
    ], check=True)
    data = self.read_sdds(output)
    expected = self.smooth_data(values, 3, 1)
    assert data == pytest.approx(expected)

  def test_gaussian(self, tmp_path):
    values = [0] * 13
    values[6] = 1
    input_sdds = self.create_sdds(values, tmp_path)
    output = tmp_path / "gauss.sdds"
    subprocess.run([
      str(SDDSSMOOTH),
      str(input_sdds),
      str(output),
      "-columns=col",
      "-gaussian=1",
      "-passes=0",
    ], check=True)
    data = self.read_sdds(output)
    expected = self.gaussian_convolution(values, 1)
    assert data == pytest.approx(expected)

  def test_savitzky_golay(self, tmp_path):
    values = [i * i for i in range(5)]
    input_sdds = self.create_sdds(values, tmp_path)
    output = tmp_path / "sg.sdds"
    subprocess.run([
      str(SDDSSMOOTH),
      str(input_sdds),
      str(output),
      "-columns=col",
      "-SavitzkyGolay=2,2,2",
    ], check=True)
    data = self.read_sdds(output)
    expected = self.savitzky_golay(values, 2, 2, 2)
    assert data == pytest.approx(expected)

  def test_median_filter(self, tmp_path):
    values = [0, 100, 5, 100, 0]
    input_sdds = self.create_sdds(values, tmp_path)
    output = tmp_path / "median.sdds"
    subprocess.run([
      str(SDDSSMOOTH),
      str(input_sdds),
      str(output),
      "-columns=col",
      "-medianFilter=windowSize=3",
    ], check=True)
    data = self.read_sdds(output)
    expected = self.median_filter(values, 3)
    assert data == pytest.approx(expected)
