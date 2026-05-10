import struct
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"
EXAMPLE = Path("SDDSlib/demo/example.sdds")

@pytest.mark.skipif(not SDDS2PLAINDATA.exists(), reason="sdds2plaindata not built")
def test_sdds2plaindata(tmp_path):
  output = tmp_path / "plain.txt"
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(EXAMPLE),
      str(output),
      "-column=longCol",
      "-parameter=stringParam",
      "-outputMode=ascii",
    ],
    check=True,
  )
  text = output.read_text()
  assert "FirstPage" in text
  assert "100" in text


@pytest.mark.skipif(not SDDS2PLAINDATA.exists(), reason="sdds2plaindata not built")
def test_ascii_separator_no_rowcount_and_labels(tmp_path):
  output = tmp_path / "labeled.txt"
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(EXAMPLE),
      str(output),
      "-column=longCol,format=%ld",
      "-parameter=stringParam",
      "-outputMode=ascii",
      "-separator=:",
      "-noRowCount",
      "-labeled",
    ],
    check=True,
  )
  text = output.read_text()
  assert "longCol" in text
  assert ":" in text
  assert "100" in text


@pytest.mark.skipif(not SDDS2PLAINDATA.exists(), reason="sdds2plaindata not built")
def test_pipe_output_and_column_order():
  result = subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(EXAMPLE),
      "-pipe=out",
      "-column=shortCol",
      "-column=longCol",
      "-outputMode=ascii",
      "-order=columnMajor",
      "-noRowCount",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  text = result.stdout.decode()
  assert "1" in text
  assert "100" in text


@pytest.mark.skipif(not SDDS2PLAINDATA.exists(), reason="sdds2plaindata not built")
def test_binary_output(tmp_path):
  output = tmp_path / "plain.bin"
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(EXAMPLE),
      str(output),
      "-column=shortCol",
      "-outputMode=binary",
    ],
    check=True,
  )
  data = output.read_bytes()
  assert len(data) > 8
  row_count = struct.unpack("<i", data[:4])[0]
  assert row_count == 5


@pytest.mark.skipif(not SDDS2PLAINDATA.exists(), reason="sdds2plaindata not built")
def test_parameter_only_labeled_output(tmp_path):
  output = tmp_path / "params.txt"
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(EXAMPLE),
      str(output),
      "-parameter=stringParam",
      "-outputMode=ascii",
      "-labeled",
    ],
    check=True,
  )
  text = output.read_text()
  assert "stringParam" in text
  assert "FirstPage" in text
