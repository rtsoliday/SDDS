import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDS2STREAM = BIN_DIR / "sdds2stream"

@pytest.mark.skipif(not SDDS2STREAM.exists(), reason="sdds2stream not built")
@pytest.mark.parametrize(
  "args, expected",
  [
    (["-columns=longCol", "-page=1"], "100\n200\n300\n400\n500\n"),
    (["-parameters=stringParam"], "FirstPage\nSecondPage\n"),
    (["-arrays=shortArray"], "1 2 3 \n7 8 \n"),
    (["-columns=longCol", "-page=2"], "600\n700\n800\n"),
    (
      ["-columns=longCol,shortCol", "-page=1", "-delimiter=:"],
      "100:1\n200:2\n300:3\n400:4\n500:5\n",
    ),
    (
      ["-columns=longCol", "-page=1", "-filenames"],
      "SDDSlib/demo/example.sdds \n100\n200\n300\n400\n500\n",
    ),
    (
      ["-columns=longCol", "-page=1", "-rows"],
      "5 rows\n100\n200\n300\n400\n500\n",
    ),
    (
      ["-columns=longCol", "-page=1", "-rows=total"],
      "5 rows\n100\n200\n300\n400\n500\n",
    ),
    (
      ["-columns=longCol", "-page=1", "-rows=bare"],
      "5\n100\n200\n300\n400\n500\n",
    ),
    (
      ["-columns=longCol", "-page=1", "-rows=scientific"],
      "5e+00 rows\n100\n200\n300\n400\n500\n",
    ),
    (["-npages"], "2 pages\n"),
    (["-npages=bare"], "2\n"),
    (["-description"], '"Example SDDS Output"\n"SDDS Example"\n'),
    (
      ["-description", "-noquotes"],
      "Example SDDS Output\nSDDS Example\n",
    ),
    (
      ["-columns=doubleCol", "-page=1", "-ignoreFormats"],
      "1.001000000000000e+01\n"
      "2.002000000000000e+01\n"
      "3.003000000000000e+01\n"
      "4.004000000000000e+01\n"
      "5.005000000000000e+01\n",
    ),
    (["-columns=longCol", "-table=1"], "100\n200\n300\n400\n500\n"),
  ],
)
def test_sdds2stream_options(args, expected):
  result = subprocess.run(
    [str(SDDS2STREAM), "SDDSlib/demo/example.sdds", *args],
    capture_output=True,
    text=True,
    check=True,
  )
  assert result.stdout == expected


@pytest.mark.skipif(not SDDS2STREAM.exists(), reason="sdds2stream not built")
def test_sdds2stream_pipe():
  data = Path("SDDSlib/demo/example.sdds").read_bytes()
  result = subprocess.run(
    [str(SDDS2STREAM), "-pipe", "-columns=longCol", "-page=1"],
    input=data,
    capture_output=True,
    check=True,
  )
  assert result.stdout.decode() == "100\n200\n300\n400\n500\n"
