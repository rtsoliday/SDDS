import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDS2SPREADSHEET = BIN_DIR / "sdds2spreadsheet"
EXAMPLE = Path("SDDSlib/demo/example.sdds")

@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_delimiter_option():
  result = subprocess.run(
    [str(SDDS2SPREADSHEET), str(EXAMPLE), "/dev/stdout", "-delimiter=:"],
    capture_output=True,
    text=True,
    check=True,
  )
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
  assert result.stdout == expected


@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_no_parameters_option():
  result = subprocess.run(
    [
      str(SDDS2SPREADSHEET),
      str(EXAMPLE),
      "/dev/stdout",
      "-noParameters",
      "-delimiter=:",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
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
  assert result.stdout == expected


@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_units_option():
  result = subprocess.run(
    [
      str(SDDS2SPREADSHEET),
      str(EXAMPLE),
      "/dev/stdout",
      "-units",
      "-noParameters",
      "-delimiter=:",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
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
  assert result.stdout == expected


@pytest.mark.skipif(not SDDS2SPREADSHEET.exists(), reason="sdds2spreadsheet not built")
def test_column_option():
  result = subprocess.run(
    [
      str(SDDS2SPREADSHEET),
      str(EXAMPLE),
      "/dev/stdout",
      "-column=longCol,shortCol",
      "-delimiter=:",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
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
  assert result.stdout == expected
