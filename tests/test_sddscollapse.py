import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCOLLAPSE = BIN_DIR / "sddscollapse"
SDDSCHECK = BIN_DIR / "sddscheck"

@pytest.mark.skipif(not (SDDSCOLLAPSE.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddscollapse(tmp_path):
  output = tmp_path / "collapse.sdds"
  subprocess.run(
    [str(SDDSCOLLAPSE), "SDDSlib/demo/example.sdds", str(output)],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
