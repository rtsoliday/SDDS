import subprocess
from pathlib import Path
import math
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCORRELATE = BIN_DIR / "sddscorrelate"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

pytestmark = pytest.mark.skipif(
  not (SDDSCORRELATE.exists() and SDDS2PLAINDATA.exists()),
  reason="required tools not built",
)

def create_input(tmp_path):
  content = (
    "SDDS1\n"
    "&column name=x, type=double &end\n"
    "&column name=y, type=double &end\n"
    "&column name=z, type=double &end\n"
    "&data mode=ascii, no_row_counts=1 &end\n"
    "1 1 2\n2 4 3\n3 9 4\n4 16 5\n"
  )
  file = tmp_path / "input.sdds"
  file.write_text(content)
  return file

def parse_pairs(dataset):
  result = subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(dataset),
      "-pipe=out",
      "-column=CorrelatePair",
      "-column=CorrelationCoefficient",
      "-labeled",
      "-separator= ",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  return parse_pairs_text(result.stdout)

def parse_pairs_text(text):
  lines = [line.strip() for line in text.splitlines() if line.strip()]
  data_lines = lines[2:]
  pairs = {}
  for line in data_lines:
    parts = line.split()
    if len(parts) >= 2:
      pairs[parts[0]] = float(parts[1])
  return pairs

def corr(a, b):
  mean_a = sum(a) / len(a)
  mean_b = sum(b) / len(b)
  num = sum((ai - mean_a) * (bi - mean_b) for ai, bi in zip(a, b))
  den = math.sqrt(
    sum((ai - mean_a) ** 2 for ai in a) * sum((bi - mean_b) ** 2 for bi in b)
  )
  return num / den


def test_columns_option(tmp_path):
  input_file = create_input(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSCORRELATE), str(input_file), str(output), "-columns=x,y"],
    check=True,
  )
  pairs = parse_pairs(output)
  expected = corr([1,2,3,4], [1,4,9,16])
  assert list(pairs.keys()) == ["x.y"]
  assert pairs["x.y"] == pytest.approx(expected)

def test_excludeColumns_option(tmp_path):
  input_file = create_input(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSCORRELATE), str(input_file), str(output), "-excludeColumns=z"],
    check=True,
  )
  pairs = parse_pairs(output)
  expected = corr([1,2,3,4], [1,4,9,16])
  assert list(pairs.keys()) == ["x.y"]
  assert pairs["x.y"] == pytest.approx(expected)

def test_withOnly_option(tmp_path):
  input_file = create_input(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSCORRELATE), str(input_file), str(output), "-withOnly=x"],
    check=True,
  )
  pairs = parse_pairs(output)
  expected_xy = corr([1,2,3,4], [1,4,9,16])
  expected_xz = 1.0
  assert set(pairs.keys()) == {"y.x", "z.x"}
  assert pairs["y.x"] == pytest.approx(expected_xy)
  assert pairs["z.x"] == pytest.approx(expected_xz)

def test_rankOrder_option(tmp_path):
  input_file = create_input(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSCORRELATE), str(input_file), str(output), "-rankOrder"],
    check=True,
  )
  result = subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(output),
      "-pipe=out",
      "-parameter=sddscorrelateMode",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  mode = result.stdout.splitlines()[0].strip().strip("\"")
  assert mode == "Rank-Order (Spearman)"

def test_stDevOutlier_option(tmp_path):
  input_file = create_input(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSCORRELATE),
      str(input_file),
      str(output),
      "-stDevOutlier=limit=1.0,passes=2",
    ],
    check=True,
  )
  result = subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(output),
      "-pipe=out",
      "-parameter=sddscorrelateStDevOutlierLimit",
      "-parameter=sddscorrelateStDevOutlierPasses",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
  assert float(lines[0]) == pytest.approx(1.0)
  assert int(lines[1]) == 2

def test_majorOrder_option(tmp_path):
  input_file = create_input(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSCORRELATE), str(input_file), str(output), "-majorOrder=row"],
    check=True,
  )
  pairs = parse_pairs(output)
  assert set(pairs.keys()) == {"x.y", "x.z", "y.z"}

def test_pipe_option(tmp_path):
  input_file = create_input(tmp_path)
  cmd = (
    f"{SDDSCORRELATE} {input_file} -pipe=out | "
    f"{SDDS2PLAINDATA} -pipe -column=CorrelatePair -column=CorrelationCoefficient "
    f"\"-separator= \" -labeled"
  )
  result = subprocess.run(cmd, shell=True, capture_output=True, text=True, check=True)
  pairs = parse_pairs_text(result.stdout)
  assert set(pairs.keys()) == {"x.y", "x.z", "y.z"}
