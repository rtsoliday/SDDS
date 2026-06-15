import re
import subprocess
from pathlib import Path
import pytest

from sdds_test_utils import BIN_DIR
SDDS2SPREADSHEET = BIN_DIR / "sdds2spreadsheet"
EXAMPLE = Path("SDDSlib/demo/example.sdds")


def spreadsheet_text(tmp_path, *options):
  output = tmp_path / "spreadsheet.txt"
  subprocess.run(
    [str(SDDS2SPREADSHEET), str(EXAMPLE), str(output), *options],
    check=True,
  )
  return output.read_text()


def normalize_scientific_numbers(text):
  return re.sub(
    r"[-+]?\d+\.\d+e[+-]\d+",
    lambda match: f"{float(match.group(0)):.12e}",
    text,
  )


def assert_spreadsheet_text(text, expected):
  assert normalize_scientific_numbers(text) == normalize_scientific_numbers(expected)


@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_delimiter_option(tmp_path):
  text = spreadsheet_text(tmp_path, "-delimiter=:")
  expected = (
    "Created from SDDS file: SDDSlib/demo/example.sdds\n"
    "Example SDDS Output:\n"
    "SDDS Example:\n"
    "\n"
    "Table 1\n"
    "shortParam::10:\n"
    "ushortParam::11:\n"
    "longParam::1000:\n"
    "ulongParam::1001:\n"
    "long64Param::1002:\n"
    "ulong64Param::1003:\n"
    "floatParam:: 3.14000010e+00:\n"
    "doubleParam::2.718280000000000e+00:\n"
    "longdoubleParam::1.100000000000000000e+00:\n"
    "stringParam::FirstPage:\n"
    "charParam::A:\n"
    "shortCol:ushortCol:longCol:ulongCol:long64Col:ulong64Col:floatCol:doubleCol:longdoubleCol:stringCol:charCol:\n"
    "1:1:100:100:100:100:1.1:10.01:1.001000000000000000e+01:one:a:\n"
    "2:2:200:200:200:200:2.2:20.02:2.002000000000000000e+01:two:b:\n"
    "3:3:300:300:300:300:3.3:30.03:3.003000000000000000e+01:three:c:\n"
    "4:4:400:400:400:400:4.4:40.04:4.004000000000000000e+01:four:d:\n"
    "5:5:500:500:500:500:5.5:50.05:5.005000000000000000e+01:five:e:\n"
    "\n"
    "Table 2\n"
    "shortParam::20:\n"
    "ushortParam::21:\n"
    "longParam::2000:\n"
    "ulongParam::2001:\n"
    "long64Param::2002:\n"
    "ulong64Param::2003:\n"
    "floatParam:: 6.28000021e+00:\n"
    "doubleParam::1.414210000000000e+00:\n"
    "longdoubleParam::2.200000000000000000e+00:\n"
    "stringParam::SecondPage:\n"
    "charParam::B:\n"
    "shortCol:ushortCol:longCol:ulongCol:long64Col:ulong64Col:floatCol:doubleCol:longdoubleCol:stringCol:charCol:\n"
    "6:6:600:600:600:600:6.6:60.06:6.006000000000000000e+01:six:f:\n"
    "7:7:700:700:700:700:7.7:70.07:7.007000000000000000e+01:seven:g:\n"
    "8:8:800:800:800:800:8.8:80.08:8.008000000000000000e+01:eight:h:\n"
  )
  assert_spreadsheet_text(text, expected)


@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_no_parameters_option(tmp_path):
  text = spreadsheet_text(tmp_path, "-noParameters", "-delimiter=:")
  expected = (
    "Created from SDDS file: SDDSlib/demo/example.sdds\n"
    "Example SDDS Output:\n"
    "SDDS Example:\n"
    "\n"
    "Table 1\n"
    "shortCol:ushortCol:longCol:ulongCol:long64Col:ulong64Col:floatCol:doubleCol:longdoubleCol:stringCol:charCol:\n"
    "1:1:100:100:100:100:1.1:10.01:1.001000000000000000e+01:one:a:\n"
    "2:2:200:200:200:200:2.2:20.02:2.002000000000000000e+01:two:b:\n"
    "3:3:300:300:300:300:3.3:30.03:3.003000000000000000e+01:three:c:\n"
    "4:4:400:400:400:400:4.4:40.04:4.004000000000000000e+01:four:d:\n"
    "5:5:500:500:500:500:5.5:50.05:5.005000000000000000e+01:five:e:\n"
    "\n"
    "Table 2\n"
    "shortCol:ushortCol:longCol:ulongCol:long64Col:ulong64Col:floatCol:doubleCol:longdoubleCol:stringCol:charCol:\n"
    "6:6:600:600:600:600:6.6:60.06:6.006000000000000000e+01:six:f:\n"
    "7:7:700:700:700:700:7.7:70.07:7.007000000000000000e+01:seven:g:\n"
    "8:8:800:800:800:800:8.8:80.08:8.008000000000000000e+01:eight:h:\n"
  )
  assert_spreadsheet_text(text, expected)


@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_units_option(tmp_path):
  text = spreadsheet_text(tmp_path, "-units", "-noParameters", "-delimiter=:")
  expected = (
    "Created from SDDS file: SDDSlib/demo/example.sdds\n"
    "Example SDDS Output:\n"
    "SDDS Example:\n"
    "\n"
    "Table 1\n"
    "shortCol:ushortCol:longCol:ulongCol:long64Col:ulong64Col:floatCol:doubleCol:longdoubleCol:stringCol:charCol:\n"
    ":::::::::::\n"
    "1:1:100:100:100:100:1.1:10.01:1.001000000000000000e+01:one:a:\n"
    "2:2:200:200:200:200:2.2:20.02:2.002000000000000000e+01:two:b:\n"
    "3:3:300:300:300:300:3.3:30.03:3.003000000000000000e+01:three:c:\n"
    "4:4:400:400:400:400:4.4:40.04:4.004000000000000000e+01:four:d:\n"
    "5:5:500:500:500:500:5.5:50.05:5.005000000000000000e+01:five:e:\n"
    "\n"
    "Table 2\n"
    "shortCol:ushortCol:longCol:ulongCol:long64Col:ulong64Col:floatCol:doubleCol:longdoubleCol:stringCol:charCol:\n"
    ":::::::::::\n"
    "6:6:600:600:600:600:6.6:60.06:6.006000000000000000e+01:six:f:\n"
    "7:7:700:700:700:700:7.7:70.07:7.007000000000000000e+01:seven:g:\n"
    "8:8:800:800:800:800:8.8:80.08:8.008000000000000000e+01:eight:h:\n"
  )
  assert_spreadsheet_text(text, expected)


@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_column_option(tmp_path):
  text = spreadsheet_text(tmp_path, "-column=longCol,shortCol", "-delimiter=:")
  expected = (
    "Created from SDDS file: SDDSlib/demo/example.sdds\n"
    "Example SDDS Output:\n"
    "SDDS Example:\n"
    "\n"
    "Table 1\n"
    "shortParam::10:\n"
    "ushortParam::11:\n"
    "longParam::1000:\n"
    "ulongParam::1001:\n"
    "long64Param::1002:\n"
    "ulong64Param::1003:\n"
    "floatParam:: 3.14000010e+00:\n"
    "doubleParam::2.718280000000000e+00:\n"
    "longdoubleParam::1.100000000000000000e+00:\n"
    "stringParam::FirstPage:\n"
    "charParam::A:\n"
    "longCol:shortCol:\n"
    "100:1:\n"
    "200:2:\n"
    "300:3:\n"
    "400:4:\n"
    "500:5:\n"
    "\n"
    "Table 2\n"
    "shortParam::20:\n"
    "ushortParam::21:\n"
    "longParam::2000:\n"
    "ulongParam::2001:\n"
    "long64Param::2002:\n"
    "ulong64Param::2003:\n"
    "floatParam:: 6.28000021e+00:\n"
    "doubleParam::1.414210000000000e+00:\n"
    "longdoubleParam::2.200000000000000000e+00:\n"
    "stringParam::SecondPage:\n"
    "charParam::B:\n"
    "longCol:shortCol:\n"
    "600:6:\n"
    "700:7:\n"
    "800:8:\n"
  )
  assert_spreadsheet_text(text, expected)


@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_all_and_verbose_options(tmp_path):
  output = tmp_path / "all.txt"
  result = subprocess.run(
    [
      str(SDDS2SPREADSHEET),
      str(EXAMPLE),
      str(output),
      "-all",
      "-verbose",
      "-delimiter=:",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  text = output.read_text()
  assert "Table 1" in text
  assert "shortCol" in text
  assert "columns of data" in result.stderr


@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_pipe_output():
  result = subprocess.run(
    [str(SDDS2SPREADSHEET), str(EXAMPLE), "-pipe=out", "-delimiter=:", "-noParameters"],
    stdout=subprocess.PIPE,
    check=True,
  )
  text = result.stdout.decode()
  assert "shortCol:ushortCol" in text
  assert "6:6:600" in text
