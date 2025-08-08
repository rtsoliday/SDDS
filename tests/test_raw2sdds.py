import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
RAW2SDDS = BIN_DIR / "raw2sdds"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

REQUIRED_TOOLS = [RAW2SDDS, SDDS2STREAM, SDDSPRINTOUT]
pytestmark = pytest.mark.skipif(
  not all(tool.exists() for tool in REQUIRED_TOOLS),
  reason="sdds tools not built",
)

DEFAULT_HSIZE = 484
DEFAULT_VSIZE = 512


def build_default_data():
  charset = b"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  n = DEFAULT_HSIZE * DEFAULT_VSIZE
  return (charset * (n // len(charset) + 1))[:n]


def check_size_params(path, rows, cols):
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(path),
      "-parameters=NumberOfRows",
      "-parameters=NumberOfColumns",
      "-noTitle",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  match = re.search(
    r"NumberOfRows\s*=\s*(\d+)\s+NumberOfColumns\s*=\s*(\d+)",
    result.stdout,
  )
  assert match
  assert match.group(1) == str(rows)
  assert match.group(2) == str(cols)


def read_data(path):
  result = subprocess.run(
    [str(SDDS2STREAM), str(path), "-columns=Data"],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout


def test_raw2sdds_defaults(tmp_path):
  data = build_default_data()
  raw = tmp_path / "input.bin"
  raw.write_bytes(data)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [str(RAW2SDDS), str(raw), str(out), "-definition=Data,type=character"],
    check=True,
  )
  check_size_params(out, DEFAULT_HSIZE, DEFAULT_VSIZE)
  expected = "".join(chr(b) + "\n" for b in data)
  assert read_data(out) == expected


@pytest.mark.parametrize("major_order", ["row", "column"])
def test_raw2sdds_size_and_major_order(tmp_path, major_order):
  data = b"ABCDEF"
  raw = tmp_path / "input.bin"
  raw.write_bytes(data)
  out = tmp_path / "out.sdds"
  args = [
    str(RAW2SDDS),
    str(raw),
    str(out),
    "-definition=Data,type=character",
    "-size=3,2",
    f"-majorOrder={major_order}",
  ]
  subprocess.run(args, check=True)
  check_size_params(out, 3, 2)
  assert read_data(out) == "A\nB\nC\nD\nE\nF\n"

