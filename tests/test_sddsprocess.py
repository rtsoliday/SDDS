import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSPROCESS = BIN_DIR / "sddsprocess"
SDDSCHECK = BIN_DIR / "sddscheck"

@pytest.mark.skipif(not (SDDSPROCESS.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddsprocess(tmp_path):
  output = tmp_path / "process.sdds"
  subprocess.run(
    [
      str(SDDSPROCESS),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-define=column,Index,i_row,type=long",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
