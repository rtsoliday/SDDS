import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSCHECK = BIN_DIR / "sddscheck"

@pytest.mark.skipif(not (PLAINDATA2SDDS.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_plaindata2sdds(tmp_path):
  plain = tmp_path / "input.txt"
  plain.write_text("1 2\n3 4\n")
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(output),
      "-column=col1,double",
      "-column=col2,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
