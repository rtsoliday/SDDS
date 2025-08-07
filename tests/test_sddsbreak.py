import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSBREAK = BIN_DIR / "sddsbreak"
SDDSCHECK = BIN_DIR / "sddscheck"

@pytest.mark.skipif(not (SDDSBREAK.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddsbreak(tmp_path):
  output = tmp_path / "break.sdds"
  subprocess.run(
    [
      str(SDDSBREAK),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-rowlimit=2",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
