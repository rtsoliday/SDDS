import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCOLLAPSE = BIN_DIR / "sddscollapse"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDS2STREAM = BIN_DIR / "sdds2stream"
EXAMPLE = Path("SDDSlib/demo/example.sdds")

pytestmark = pytest.mark.skipif(
  not (SDDSCOLLAPSE.exists() and SDDSCHECK.exists() and SDDSQUERY.exists() and SDDS2STREAM.exists()),
  reason="tools not built",
)


def stream_columns(path, columns):
  result = subprocess.run(
    [str(SDDS2STREAM), str(path), f"-columns={columns}", "-delimiter=|"],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout.strip().splitlines() if result.stdout.strip() else []


def test_sddscollapse(tmp_path):
  output = tmp_path / "collapse.sdds"
  subprocess.run(
    [str(SDDSCOLLAPSE), str(EXAMPLE), str(output)],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  lines = stream_columns(output, "PageNumber,stringParam,longParam")
  assert lines == ["1|FirstPage|1000", "2|SecondPage|2000"]


def test_pipe_and_major_order(tmp_path):
  result = subprocess.run(
    [str(SDDSCOLLAPSE), str(EXAMPLE), "-pipe=out", "-majorOrder=column", "-noWarnings"],
    stdout=subprocess.PIPE,
    check=True,
  )
  output = tmp_path / "pipe-collapse.sdds"
  output.write_bytes(result.stdout)
  columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
  assert "PageNumber" in columns.stdout
  assert stream_columns(output, "PageNumber,stringParam") == ["1|FirstPage", "2|SecondPage"]


def test_row_major_output_mode_preserved(tmp_path):
  output = tmp_path / "row-major.sdds"
  subprocess.run(
    [str(SDDSCOLLAPSE), str(EXAMPLE), str(output), "-majorOrder=row"],
    check=True,
  )
  assert stream_columns(output, "PageNumber,doubleParam") == [
    "1|2.718280000000000e+00",
    "2|1.414210000000000e+00",
  ]
