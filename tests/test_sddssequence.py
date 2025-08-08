import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSEQUENCE = BIN_DIR / "sddssequence"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"


def run_printout(path, columns, page=None):
  args = [str(SDDSPRINTOUT), str(path)]
  for col in columns:
    args.append(f"-columns={col}")
  if page is not None:
    args.append(f"-fromPage={page}")
    args.append(f"-toPage={page}")
  args += ["-noTitle", "-noLabels"]
  result = subprocess.run(args, capture_output=True, text=True, check=True)
  data = []
  for line in result.stdout.strip().splitlines():
    parts = line.split()
    if len(parts) == len(columns):
      data.append([float(x) for x in parts])
  return data


@pytest.mark.skipif(not (SDDSSEQUENCE.exists() and SDDSPRINTOUT.exists()), reason="tools not built")
def test_basic_sequence_generation(tmp_path):
  out = tmp_path / "basic.sdds"
  subprocess.run(
    [
      str(SDDSSEQUENCE),
      str(out),
      "-define=x,type=double",
      "-sequence=begin=1,end=5,delta=2",
    ],
    check=True,
  )
  data = [d[0] for d in run_printout(out, ["x"])]
  assert data == pytest.approx([1, 3, 5])


@pytest.mark.skipif(not (SDDSSEQUENCE.exists() and SDDSPRINTOUT.exists()), reason="tools not built")
def test_repeat_option(tmp_path):
  out = tmp_path / "repeat.sdds"
  subprocess.run(
    [
      str(SDDSSEQUENCE),
      str(out),
      "-define=x,type=long",
      "-sequence=begin=1,delta=1,number=3",
      "-repeat=2",
    ],
    check=True,
  )
  data = [d[0] for d in run_printout(out, ["x"])]
  assert data == pytest.approx([1, 2, 3, 1, 2, 3])


@pytest.mark.skipif(not (SDDSSEQUENCE.exists() and SDDSPRINTOUT.exists()), reason="tools not built")
def test_break_option(tmp_path):
  out = tmp_path / "break.sdds"
  subprocess.run(
    [
      str(SDDSSEQUENCE),
      str(out),
      "-define=x,type=long",
      "-sequence=begin=0,end=4,number=3",
      "-repeat=2",
      "-break",
    ],
    check=True,
  )
  page1 = [d[0] for d in run_printout(out, ["x"], page=1)]
  page2 = [d[0] for d in run_printout(out, ["x"], page=2)]
  assert page1 == pytest.approx([0, 2, 4])
  assert page2 == pytest.approx([0, 2, 4])
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(out),
      "-columns=x",
      "-fromPage=3",
      "-toPage=3",
      "-noTitle",
      "-noLabels",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  assert result.stdout.strip() == ""


@pytest.mark.skipif(not SDDSSEQUENCE.exists(), reason="sddssequence not built")
def test_major_order(tmp_path):
  out = tmp_path / "major.sdds"
  subprocess.run(
    [
      str(SDDSSEQUENCE),
      str(out),
      "-define=x,type=double",
      "-sequence=begin=0,delta=1,number=2",
      "-majorOrder=column",
    ],
    check=True,
  )
  header = out.read_bytes()
  assert b"column_major_order=1" in header


@pytest.mark.skipif(not SDDSSEQUENCE.exists(), reason="sddssequence not built")
def test_pipe_option():
  result = subprocess.run(
    [
      str(SDDSSEQUENCE),
      "-define=x,type=double",
      "-sequence=begin=0,delta=1,number=2",
      "-pipe=out",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  assert result.stdout.startswith(b"SDDS")
