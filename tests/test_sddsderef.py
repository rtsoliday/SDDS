import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSDEREF = BIN_DIR / "sddsderef"

@pytest.mark.skipif(not SDDSDEREF.exists(), reason="sddsderef not built")
@pytest.mark.parametrize(
  "option,message",
  [
    ("-column", "invalid -column syntax"),
    ("-parameter", "invalid -parameter syntax"),
    ("-constant", "invalid -constant syntax"),
    ("-pipe=foo", "invalid -pipe syntax"),
    ("-outOfBounds", "invalid -outOfBounds syntax/values"),
    ("-majorOrder=foo", "invalid -majorOrder syntax/values"),
  ],
)
def test_sddsderef_option_errors(option, message):
  result = subprocess.run([str(SDDSDEREF), option], capture_output=True, text=True)
  assert result.returncode != 0
  assert result.stdout == ""
  assert result.stderr.strip().splitlines()[-1] == f"Error ({SDDSDEREF}): {message}"
