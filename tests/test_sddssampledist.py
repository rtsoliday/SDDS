import statistics
import subprocess
from pathlib import Path
import pytest

from sdds_test_utils import BIN_DIR
SDDSSAMPLEDIST = BIN_DIR / "sddssampledist"
SDDS2STREAM = BIN_DIR / "sdds2stream"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"


@pytest.mark.skipif(
  not SDDSSAMPLEDIST.exists() or not SDDS2STREAM.exists() or not PLAINDATA2SDDS.exists(),
  reason="required tools not built",
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


@pytest.mark.skipif(
  not SDDSSAMPLEDIST.exists() or not SDDS2STREAM.exists(),
  reason="required tools not built",
)
def test_pipe_output_and_optimal_halton(tmp_path):
  result = subprocess.run(
    [
      str(SDDSSAMPLEDIST),
      "-samples=50",
      "-seed=12345",
      "-uniform=columnName=u,minimumValue=0,maximumValue=1",
      "-optimalHalton",
      "-pipe=out",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  out = tmp_path / "pipe.sdds"
  out.write_bytes(result.stdout)
  values = [float(x) for x in subprocess.run([str(SDDS2STREAM), str(out), "-col=u"], capture_output=True, text=True, check=True).stdout.split()]
  assert len(values) == 50


def create_distribution(tmp_path, name, values):
  plain = tmp_path / f"{name}.txt"
  plain.write_text("\n".join(f"{x} {cdf}" for x, cdf in values) + "\n")
  sdds = tmp_path / f"{name}.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      "-column=x,double",
      "-column=cdf,double",
      "-inputMode=ascii",
      "-outputMode=binary",
      "-noRowCount",
    ],
    check=True,
  )
  return sdds


@pytest.mark.skipif(
  not SDDSSAMPLEDIST.exists() or not SDDS2STREAM.exists() or not PLAINDATA2SDDS.exists(),
  reason="required tools not built",
)
def test_threads_match_serial_datafile_distributions(tmp_path):
  dist1 = create_distribution(tmp_path, "dist1", [(0, 0), (1, 0.5), (2, 1)])
  dist2 = create_distribution(tmp_path, "dist2", [(10, 0), (20, 0.25), (30, 1)])
  serial = tmp_path / "serial.sdds"
  threaded = tmp_path / "threaded.sdds"
  args = [
    "-samples=25",
    "-seed=13579",
    f"-columns=independentVariable=x,cdf=cdf,output=a,datafile={dist1}",
    f"-columns=independentVariable=x,cdf=cdf,output=b,datafile={dist2}",
  ]
  subprocess.run([str(SDDSSAMPLEDIST), str(serial), *args, "-threads=1"], check=True)
  subprocess.run([str(SDDSSAMPLEDIST), str(threaded), *args, "-threads=2"], check=True)
  for column in ("a", "b"):
    serial_values = subprocess.run([str(SDDS2STREAM), str(serial), f"-col={column}"], capture_output=True, text=True, check=True).stdout.split()
    threaded_values = subprocess.run([str(SDDS2STREAM), str(threaded), f"-col={column}"], capture_output=True, text=True, check=True).stdout.split()
    assert threaded_values == serial_values
