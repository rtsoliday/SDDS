import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDS2STREAM = BIN_DIR / "sdds2stream"

pytestmark = pytest.mark.skipif(
  not (PLAINDATA2SDDS.exists() and SDDSCHECK.exists() and SDDS2STREAM.exists()),
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


def test_plaindata2sdds(tmp_path):
  plain = tmp_path / "input.txt"
  plain.write_text("1 2\n3 4\n")
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(output),
      "-column=col1,double",
      "-column=col2,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  assert stream_columns(output, "col1,col2") == [
    "1.000000000000000e+00|2.000000000000000e+00",
    "3.000000000000000e+00|4.000000000000000e+00",
  ]


def test_skiplines_comments_and_fillin(tmp_path):
  plain = tmp_path / "input-sep.txt"
  plain.write_text("skip me\n!comment\n5 \n7 8\n")
  output = tmp_path / "out-sep.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(output),
      "-column=col1,double",
      "-column=col2,double",
      "-commentCharacters=!",
      "-fillin",
      "-skiplines=1",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  assert stream_columns(output, "col1,col2") == [
    "5.000000000000000e+00|0.000000000000000e+00",
    "7.000000000000000e+00|8.000000000000000e+00",
  ]


def test_eof_sequence_stops_reading(tmp_path):
  plain = tmp_path / "eof.txt"
  plain.write_text("1 2\nSTOP\n3 4\n")
  output = tmp_path / "eof.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(output),
      "-column=col1,double",
      "-column=col2,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
      "-eofSequence=STOP",
    ],
    check=True,
  )
  assert stream_columns(output, "col1,col2") == ["1.000000000000000e+00|2.000000000000000e+00"]


def test_order_column_major_input(tmp_path):
  plain = tmp_path / "column-major.txt"
  plain.write_text("1\n3\n2\n4\n")
  output = tmp_path / "column-major.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(output),
      "-column=col1,double",
      "-column=col2,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
      "-order=columnMajor",
    ],
    check=True,
  )
  assert stream_columns(output, "col1,col2") == [
    "1.000000000000000e+00|3.000000000000000e+00",
    "2.000000000000000e+00|4.000000000000000e+00",
  ]
