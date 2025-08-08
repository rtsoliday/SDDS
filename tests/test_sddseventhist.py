import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSEVENTHIST = BIN_DIR / "sddseventhist"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2STREAM = BIN_DIR / "sdds2stream"

REQUIRED = [SDDSEVENTHIST, PLAINDATA2SDDS, SDDS2STREAM]
pytestmark = pytest.mark.skipif(not all(t.exists() for t in REQUIRED), reason="required tools not built")

def create_input(tmp_path):
  plain = tmp_path / "input.txt"
  plain.write_text("A 1\nA 2\nA 3\nB 1\nB 2\nB 2\n")
  sdds = tmp_path / "input.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      "-column=event,string",
      "-column=data,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return sdds

def read_column(path, column):
  result = subprocess.run(
    [str(SDDS2STREAM), f"-column={column}", str(path)],
    capture_output=True,
    text=True,
    check=True,
  )
  return [float(x) for x in result.stdout.split()]

def test_bins_limits_sides_peak(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSEVENTHIST),
      str(inp),
      str(out),
      "-dataColumn=data",
      "-eventIdentifier=event",
      "-bins=3",
      "-lowerLimit=1",
      "-upperLimit=4",
      "-sides",
      "-normalize=peak",
      "-majorOrder=row",
    ],
    check=True,
  )
  a_hist = read_column(out, "AFrequency")
  b_hist = read_column(out, "BFrequency")
  assert len(a_hist) == 5
  assert len(b_hist) == 5
  assert max(a_hist) == 1
  assert max(b_hist) == 1
  data_line = out.read_bytes().split(b"&data", 1)[1][:100]
  assert b"column_major_order=1" not in data_line

def test_binsize_overlap_pipe_area_column(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSEVENTHIST),
      str(inp),
      str(out),
      "-dataColumn=data",
      "-eventIdentifier=event",
      "-sizeOfBins=1",
      "-lowerLimit=1",
      "-upperLimit=4",
      "-normalize=area",
      "-overlapEvent=A",
      "-majorOrder=column",
    ],
    check=True,
  )
  result = subprocess.run(
    [
      str(SDDSEVENTHIST),
      str(inp),
      "-pipe=out",
      "-dataColumn=data",
      "-eventIdentifier=event",
      "-sizeOfBins=1",
      "-lowerLimit=1",
      "-upperLimit=4",
      "-normalize=area",
      "-overlapEvent=A",
      "-majorOrder=column",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  assert result.stdout == out.read_bytes()
  data_line = out.read_bytes().split(b"&data", 1)[1][:100]
  assert b"column_major_order=1" in data_line
  a_hist = read_column(out, "AFrequency")
  b_hist = read_column(out, "BFrequency")
  a_overlap = read_column(out, "A.AOverlap")
  b_overlap = read_column(out, "B.AOverlap")
  for seq in (a_hist, b_hist, a_overlap, b_overlap):
    assert len(seq) == 4
    assert all(v >= 0 for v in seq)

def test_normalize_sum(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSEVENTHIST),
      str(inp),
      str(out),
      "-dataColumn=data",
      "-eventIdentifier=event",
      "-bins=3",
      "-lowerLimit=1",
      "-upperLimit=4",
      "-normalize",
    ],
    check=True,
  )
  a_hist = read_column(out, "AFrequency")
  b_hist = read_column(out, "BFrequency")
  assert len(a_hist) == 3
  assert len(b_hist) == 3
  assert max(a_hist) <= 1
  assert max(b_hist) <= 1
