import statistics
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSAMPLEDIST = BIN_DIR / "sddssampledist"
SDDS2STREAM = BIN_DIR / "sdds2stream"


@pytest.mark.skipif(
  not SDDSSAMPLEDIST.exists() or not SDDS2STREAM.exists(),
  reason="sddssampledist or sdds2stream not built",
)
@pytest.mark.parametrize(
  "option,column,expected_mean,expected_sd,mean_tol,sd_tol",
  [
    (
      "-gaussian=columnName=g,meanValue=5,sigmaValue=2",
      "g",
      5.0,
      2.0,
      0.1,
      0.2,
    ),
    (
      "-uniform=columnName=u,minimumValue=0,maximumValue=1",
      "u",
      0.5,
      0.288675,
      0.05,
      0.02,
    ),
    (
      "-poisson=columnName=p,meanValue=4",
      "p",
      4.0,
      2.0,
      0.2,
      0.3,
    ),
  ],
)
def test_sddssampledist_distributions(tmp_path, option, column, expected_mean, expected_sd, mean_tol, sd_tol):
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSSAMPLEDIST),
      str(out),
      "-samples=1000",
      "-seed=12345",
      option,
    ],
    check=True,
  )
  result = subprocess.run(
    [str(SDDS2STREAM), str(out), f"-col={column}"],
    capture_output=True,
    text=True,
    check=True,
  )
  values = [float(x) for x in result.stdout.split()]
  mean = statistics.mean(values)
  sd = statistics.pstdev(values)
  assert abs(mean - expected_mean) < mean_tol
  assert abs(sd - expected_sd) < sd_tol
