import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

@pytest.mark.skipif(not SDDSPRINTOUT.exists(), reason="sddsprintout not built")
def test_sddsprintout(tmp_path):
  output = tmp_path / "print.txt"
  subprocess.run(
    [
      str(SDDSPRINTOUT),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=doubleCol",
      "-parameters=stringParam",
      "-noTitle",
    ],
    check=True,
  )
  assert output.exists()
