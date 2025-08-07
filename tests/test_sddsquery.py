import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSQUERY = BIN_DIR / "sddsquery"

@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_sddsquery():
  result = subprocess.run(
    [str(SDDSQUERY), "SDDSlib/demo/example.sdds", "-columnlist"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert "shortCol" in result.stdout
