import math
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSKDE = BIN_DIR / "sddskde"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED_TOOLS = [SDDSKDE, PLAINDATA2SDDS, SDDS2PLAINDATA]


def quantile(sorted_data, f):
  n = len(sorted_data)
  h = (n - 1) * f
  lower = int(math.floor(h))
  upper = int(math.ceil(h))
  if upper >= n:
    return sorted_data[-1]
  if lower == upper:
    return sorted_data[lower]
  weight = h - lower
  return sorted_data[lower] + weight * (sorted_data[upper] - sorted_data[lower])


def bandwidth(data):
  data = sorted(data)
  sigma = math.sqrt(sum((x - sum(data)/len(data))**2 for x in data)/(len(data)-1))
  q3 = quantile(data, 0.75)
  q1 = quantile(data, 0.25)
  iqr = q3 - q1
  min_val = min(iqr / 1.339999, sigma)
  return 0.90 * min_val * (len(data) ** -0.20)


def expected_distribution(data, n_test=100, margin=0.3):
  min_val = min(data)
  max_val = max(data)
  gap = max_val - min_val
  lower = min_val - gap * margin
  upper = max_val + gap * margin
  step = (upper - lower) / (n_test - 1)
  x = [lower + i * step for i in range(n_test)]
  h = max(bandwidth(data), 2e-6)
  pdf = []
  for sample in x:
    total = 0.0
    for d in data:
      z = (d - sample) / h
      total += math.exp(-(z * z) / 2.0) / math.sqrt(2 * math.pi)
    pdf.append(total / (len(data) * h))
  cdf = [pdf[0]]
  for val in pdf[1:]:
    cdf.append(cdf[-1] + val)
  norm = cdf[-1]
  cdf = [v / norm for v in cdf]
  return x, pdf, cdf


def create_sdds(values, tmp_path):
  plain = tmp_path / "input.txt"
  plain.write_text("\n".join(str(v) for v in values) + "\n")
  sdds = tmp_path / "input.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(plain),
    str(sdds),
    "-column=x,double",
    "-inputMode=ascii",
    "-outputMode=ascii",
    "-noRowCount",
  ], check=True)
  return sdds


def read_output(path, tmp_path):
  out = tmp_path / "out.txt"
  subprocess.run([
    str(SDDS2PLAINDATA),
    str(path),
    str(out),
    "-column=x",
    "-column=xPDF",
    "-column=xCDF",
    "-separator= ",
    "-outputMode=ascii",
    "-noRowCount",
  ], check=True)
  xs, pdf, cdf = [], [], []
  for line in out.read_text().strip().splitlines():
    xv, pv, cv = line.split()
    xs.append(float(xv))
    pdf.append(float(pv))
    cdf.append(float(cv))
  return xs, pdf, cdf


@pytest.mark.skipif(not all(t.exists() for t in REQUIRED_TOOLS), reason="sddskde not built")
def test_gaussian_kernel(tmp_path):
  data = [0.0, 1.0, 2.0]
  inp = create_sdds(data, tmp_path)
  out = tmp_path / "output.sdds"
  subprocess.run([str(SDDSKDE), str(inp), str(out), "-column=x"], check=True)
  x, pdf, cdf = read_output(out, tmp_path)
  exp_x, exp_pdf, exp_cdf = expected_distribution(data)
  assert x == pytest.approx(exp_x)
  assert pdf == pytest.approx(exp_pdf, rel=1e-6)
  assert cdf == pytest.approx(exp_cdf, rel=1e-6)
