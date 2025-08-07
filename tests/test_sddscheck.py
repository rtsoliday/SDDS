import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCHECK = BIN_DIR / "sddscheck"

@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_ok():
  result = subprocess.run([str(SDDSCHECK), "SDDSlib/demo/example.sdds"], capture_output=True, text=True)
  assert result.returncode == 0
  assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_print_errors():
  result = subprocess.run([str(SDDSCHECK), "SDDSlib/demo/example.sdds", "-printErrors"], capture_output=True, text=True)
  assert result.returncode == 0
  assert result.stdout.strip() == "ok"
