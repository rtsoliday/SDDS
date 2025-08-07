import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSORT = BIN_DIR / "sddssort"
SDDSCHECK = BIN_DIR / "sddscheck"

@pytest.mark.skipif(not (SDDSSORT.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddssort(tmp_path):
  output = tmp_path / "sort.sdds"
  subprocess.run(
    [
      str(SDDSSORT),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-column=longCol",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
