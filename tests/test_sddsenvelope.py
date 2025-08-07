import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSENVELOPE = BIN_DIR / "sddsenvelope"
SDDS2STREAM = BIN_DIR / "sdds2stream"


@pytest.fixture(scope="module")
def input_file(tmp_path_factory):
  infile = tmp_path_factory.mktemp("data") / "in.sdds"
  infile.write_text(
    "SDDS2\n"
    "&description text=\"test\" &end\n"
    "&parameter name=t, type=double &end\n"
    "&column name=x, type=double &end\n"
    "&column name=y, type=double &end\n"
    "&column name=w, type=double &end\n"
    "&data mode=ascii &end\n"
    "! page 1\n"
    "0\n"
    "3\n"
    "1 10 1\n"
    "-2 20 1\n"
    "3 30 1\n"
    "! page 2\n"
    "1\n"
    "3\n"
    "4 40 1\n"
    "-5 50 1\n"
    "6 60 1\n"
  )
  return infile


MEAN = [2.5, -3.5, 4.5]
MAXIMUM = [4, -2, 6]
MINIMUM = [1, -5, 3]
PMAX = [1, 0, 1]
PMIN = [0, 1, 0]
LARGEST = [4, 5, 6]
SIGNED_LARGEST = [4, -5, 6]
SUM = [5, -7, 9]
MEDIAN = [4, -2, 6]
DRANGE = [0.0, 0.0, 0.0]
PERCENTILE = [1, -5, 3]
STD = [2.1213203435596424, 2.1213203435596424, 2.1213203435596424]
RMS = [2.9154759474226504, 3.8078865529319543, 4.743416490252569]
SIGMA = [1.5, 1.5, 1.5]
SLOPE = [3, -3, 3]
INTERCEPT = [1, -2, 3]
COPY_Y = [10, 20, 30]


@pytest.mark.skipif(
  not (SDDSENVELOPE.exists() and SDDS2STREAM.exists()),
  reason="sddsenvelope or sdds2stream not built",
)
@pytest.mark.parametrize(
  "args,column,expected",
  [
    (["-maximum=x"], "xMax", MAXIMUM),
    (["-minimum=x"], "xMin", MINIMUM),
    (["-pmaximum=t,x"], "tPMaximumx", PMAX),
    (["-pminimum=t,x"], "tPMinimumx", PMIN),
    (["-largest=x"], "xLargest", LARGEST),
    (["-signedLargest=x"], "xSignedLargest", SIGNED_LARGEST),
    (["-mean=x"], "xMean", MEAN),
    (["-sum=1,x"], "xSum", SUM),
    (["-median=x"], "xMedian", MEDIAN),
    (["-decilerange=x"], "xDRange", DRANGE),
    (["-percentile=75,x"], "x75Percentile", PERCENTILE),
    (["-standarddeviation=x"], "xStDev", STD),
    (["-rms=x"], "xRms", RMS),
    (["-sigma=x"], "xSigma", SIGMA),
    (["-slope=t,x"], "xSlope", SLOPE),
    (["-intercept=t,x"], "xIntercept", INTERCEPT),
    (["-wmean=w,x"], "xWMean", MEAN),
    (["-wstandarddeviation=w,x"], "xWStDev", STD),
    (["-wrms=w,x"], "xWRms", RMS),
    (["-wsigma=w,x"], "xWSigma", SIGMA),
    (["-copy=y"], "y", COPY_Y),
    (["-mean=x", "-nowarnings"], "xMean", MEAN),
    (["-mean=x", "-majorOrder=column"], "xMean", MEAN),
  ],
)
def test_sddsenvelope_options(input_file, tmp_path, args, column, expected):
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSENVELOPE), str(input_file), str(out), *args], check=True)
  result = subprocess.run(
    [str(SDDS2STREAM), f"-columns={column}", str(out)],
    capture_output=True,
    text=True,
    check=True,
  )
  values = [float(x) for x in result.stdout.strip().split()]
  assert values == pytest.approx(expected)


@pytest.mark.skipif(
  not (SDDSENVELOPE.exists() and SDDS2STREAM.exists()),
  reason="sddsenvelope or sdds2stream not built",
)
def test_sddsenvelope_pipe(input_file, tmp_path):
  data = Path(input_file).read_bytes()
  result = subprocess.run(
    [str(SDDSENVELOPE), "-pipe", "-mean=x"],
    input=data,
    capture_output=True,
    check=True,
  )
  pipe_out = tmp_path / "pipe.sdds"
  pipe_out.write_bytes(result.stdout)
  result = subprocess.run(
    [str(SDDS2STREAM), "-columns=xMean", str(pipe_out)],
    capture_output=True,
    text=True,
    check=True,
  )
  values = [float(x) for x in result.stdout.strip().split()]
  assert values == pytest.approx(MEAN)
