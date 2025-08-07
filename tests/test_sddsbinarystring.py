import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSBINARYSTRING = BIN_DIR / "sddsbinarystring"
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"
SDDS2STREAM = BIN_DIR / "sdds2stream"

missing = [p for p in (SDDSBINARYSTRING, SDDSMAKEDATASET, SDDS2STREAM) if not p.exists()]
pytestmark = pytest.mark.skipif(missing, reason="required binaries not built")

def test_sddsbinarystring_column(tmp_path):
  input_file = tmp_path / "input.sdds"
  subprocess.run(
    [
      str(SDDSMAKEDATASET),
      str(input_file),
      "-column=val,type=long",
      "-data=5,15",
    ],
    check=True,
  )
  output_file = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSBINARYSTRING), str(input_file), str(output_file), "-column=val"],
    check=True,
  )
  result = subprocess.run(
    [str(SDDS2STREAM), str(output_file), "-columns=valBinaryString"],
    capture_output=True,
    text=True,
    check=True,
  )
  expected = f"{5:032b}\n{15:032b}\n"
  assert result.stdout == expected

def test_sddsbinarystring_pipe(tmp_path):
  input_file = tmp_path / "input.sdds"
  subprocess.run(
    [
      str(SDDSMAKEDATASET),
      str(input_file),
      "-column=val,type=long",
      "-data=5,15",
    ],
    check=True,
  )
  data = input_file.read_bytes()
  proc = subprocess.run(
    [str(SDDSBINARYSTRING), "-column=val", "-pipe"],
    input=data,
    stdout=subprocess.PIPE,
    check=True,
  )
  result = subprocess.run(
    [str(SDDS2STREAM), "-pipe", "-columns=valBinaryString"],
    input=proc.stdout,
    capture_output=True,
    check=True,
  )
  expected = f"{5:032b}\n{15:032b}\n"
  assert result.stdout.decode() == expected
