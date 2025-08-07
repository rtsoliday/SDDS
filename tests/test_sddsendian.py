import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSENDIAN = BIN_DIR / "sddsendian"
EXAMPLE = Path("SDDSlib/demo/example.sdds")

@pytest.mark.skipif(not SDDSENDIAN.exists(), reason="sddsendian not built")
def test_sddsendian_pipe(tmp_path):
  file_output = tmp_path / "out.sdds"
  subprocess.run([str(SDDSENDIAN), str(EXAMPLE), str(file_output)], check=True)
  result = subprocess.run(
    [str(SDDSENDIAN), str(EXAMPLE), "-pipe=out"],
    stdout=subprocess.PIPE,
    check=True,
  )
  assert result.stdout == file_output.read_bytes()

@pytest.mark.skipif(not SDDSENDIAN.exists(), reason="sddsendian not built")
def test_sddsendian_non_native(tmp_path):
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSENDIAN), str(EXAMPLE), str(output), "-nonNative"],
    check=True,
  )
  data_line = output.read_bytes().split(b"&data", 1)[1][:100]
  assert b"endian=little" in data_line

@pytest.mark.skipif(not SDDSENDIAN.exists(), reason="sddsendian not built")
def test_sddsendian_major_order_column(tmp_path):
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSENDIAN), str(EXAMPLE), str(output), "-majorOrder=column"],
    check=True,
  )
  data_line = output.read_bytes().split(b"&data", 1)[1][:100]
  assert b"column_major_order=1" in data_line

@pytest.mark.skipif(not SDDSENDIAN.exists(), reason="sddsendian not built")
def test_sddsendian_major_order_row(tmp_path):
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSENDIAN), str(EXAMPLE), str(output), "-majorOrder=row"],
    check=True,
  )
  data_line = output.read_bytes().split(b"&data", 1)[1][:100]
  assert b"column_major_order" not in data_line
