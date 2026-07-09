import subprocess
from pathlib import Path
import pytest

from sdds_test_utils import BIN_DIR
SDDSQUERY = BIN_DIR / "sddsquery"
SDDS2STREAM = BIN_DIR / "sdds2stream"
EXAMPLE = Path("SDDSlib/demo/example.sdds")

pytestmark = pytest.mark.skipif(
  not (SDDSQUERY.exists() and SDDS2STREAM.exists()),
  reason="tools not built",
)


def test_columnlist():
  result = subprocess.run(
    [str(SDDSQUERY), str(EXAMPLE), "-columnlist"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert "shortCol" in result.stdout
  assert "doubleCol" in result.stdout


def test_parameterlist_with_delimiter_and_appendunits():
  result = subprocess.run(
    [str(SDDSQUERY), str(EXAMPLE), "-parameterlist", "-delimiter=|", "-appendunits=bare"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert "stringParam|" in result.stdout
  assert "doubleParam|" in result.stdout


def test_arraylist():
  result = subprocess.run(
    [str(SDDSQUERY), str(EXAMPLE), "-arraylist"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert "shortArray" in result.stdout


def test_version():
  result = subprocess.run(
    [str(SDDSQUERY), str(EXAMPLE), "-version"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert result.stdout.strip() == "5"


def test_default_summary_and_readall():
  result = subprocess.run(
    [str(SDDSQUERY), str(EXAMPLE), "-readAll"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert f"file {EXAMPLE} is in SDDS protocol version 5" in result.stdout
  assert "11 columns of data" in result.stdout
  assert "11 parameters" in result.stdout


def test_pipe_input_columnlist():
  result = subprocess.run(
    [str(SDDSQUERY), "-pipe", "-columnlist"],
    input=EXAMPLE.read_bytes(),
    capture_output=True,
    check=True,
  )
  output = result.stdout.decode()
  assert "shortCol" in output
  assert "doubleCol" in output


def test_sddsoutput_creates_summary_dataset(tmp_path):
  output = tmp_path / "header-summary.sdds"
  subprocess.run(
    [str(SDDSQUERY), str(EXAMPLE), f"-sddsOutput={output}", "-columnlist"],
    check=True,
  )
  result = subprocess.run(
    [str(SDDS2STREAM), str(output), "-columns=Name,Type", "-delimiter=|"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert "shortCol|short" in result.stdout
  assert "doubleCol|double" in result.stdout
