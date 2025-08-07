import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCOMBINE = BIN_DIR / "sddscombine"
SDDSCHECK = BIN_DIR / "sddscheck"

@pytest.mark.skipif(not (SDDSCOMBINE.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddscombine(tmp_path):
  output = tmp_path / "combine.sdds"
  subprocess.run(
    [
      str(SDDSCOMBINE),
      "SDDSlib/demo/example.sdds",
      "SDDSlib/demo/example.sdds",
      str(output),
      "-overWrite",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
