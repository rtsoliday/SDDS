import math
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSGFIT = BIN_DIR / "sddsgfit"
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDS2STREAM = BIN_DIR / "sdds2stream"

missing = [p for p in (SDDSGFIT, SDDSMAKEDATASET, SDDSQUERY, SDDS2STREAM) if not p.exists()]
pytestmark = pytest.mark.skipif(missing, reason="required binaries not built")

BASELINE = 1.0
HEIGHT = 2.0
MEAN = 0.0
SIGMA = 1.0
SY = 0.1

def create_data(tmp_path):
  xs = [i * 0.5 for i in range(-10, 11)]
  ys = [BASELINE + HEIGHT * math.exp(-0.5 * ((x - MEAN) / SIGMA) ** 2) for x in xs]
  sy = [SY for _ in xs]
  sdds = tmp_path / "input.sdds"
  subprocess.run(
    [
      str(SDDSMAKEDATASET),
      str(sdds),
      "-column=x,type=double",
      f"-data={','.join(str(x) for x in xs)}",
      "-column=y,type=double",
      f"-data={','.join(str(y) for y in ys)}",
      "-column=sy,type=double",
      f"-data={','.join(str(v) for v in sy)}",
      "-ascii",
    ],
    check=True,
  )
  return sdds

def read_parameter(path, name):
  result = subprocess.run(
    [str(SDDS2STREAM), str(path), f"-parameters={name}"],
    capture_output=True,
    text=True,
    check=True,
  )
  return float(result.stdout.strip().split()[-1])

def column_list(path):
  result = subprocess.run(
    [str(SDDSQUERY), "-columnlist", str(path)],
    capture_output=True,
    text=True,
    check=True,
  )
  return [line for line in result.stdout.splitlines() if line]

def test_fit_all_options(tmp_path):
  input_sdds = create_data(tmp_path)
  output = tmp_path / "out.sdds"
  result = subprocess.run(
    [
      str(SDDSGFIT),
      str(input_sdds),
      str(output),
      "-columns=x,y,ysigma=sy",
      "-fitRange=-2,2",
      "-fullOutput",
      "-verbosity=1",
      "-stepSize=1e-3",
      "-tolerance=1e-8",
      "-guesses=baseline=0,mean=0,height=1,sigma=0.5",
      "-limits=evaluations=1000,passes=20",
      "-majorOrder=row",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  assert column_list(output) == ["x", "y", "sy", "yResidual", "yFit"]
  baseline = read_parameter(output, "gfitBaseline")
  height = read_parameter(output, "gfitHeight")
  mean = read_parameter(output, "gfitMean")
  sigma = read_parameter(output, "gfitSigma")
  rms = read_parameter(output, "gfitRmsResidual")
  assert baseline == pytest.approx(BASELINE, rel=1e-3)
  assert height == pytest.approx(HEIGHT, rel=1e-3)
  assert mean == pytest.approx(MEAN, abs=1e-3)
  assert sigma == pytest.approx(SIGMA, rel=1e-3)
  assert rms == pytest.approx(0.0, abs=1e-4)

def test_fixvalue_option(tmp_path):
  input_sdds = create_data(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSGFIT),
      str(input_sdds),
      str(output),
      "-columns=x,y",
      "-fixValue=mean=0",
    ],
    check=True,
  )
  mean = read_parameter(output, "gfitMean")
  assert mean == pytest.approx(0.0, abs=1e-12)

def test_pipe_option(tmp_path):
  input_sdds = create_data(tmp_path)
  data = input_sdds.read_bytes()
  proc = subprocess.run(
    [
      str(SDDSGFIT),
      "-pipe=in,out",
      "-columns=x,y",
      "-majorOrder=column",
    ],
    input=data,
    stdout=subprocess.PIPE,
    check=True,
  )
  output = tmp_path / "out.sdds"
  output.write_bytes(proc.stdout)
  baseline = read_parameter(output, "gfitBaseline")
  assert baseline == pytest.approx(BASELINE, rel=1e-3)
