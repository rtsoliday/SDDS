import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

@pytest.mark.skipif(not SDDS2PLAINDATA.exists(), reason="sdds2plaindata not built")
def test_sdds2plaindata(tmp_path):
  output = tmp_path / "plain.txt"
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-column=longCol",
      "-parameter=stringParam",
      "-outputMode=ascii",
    ],
    check=True,
  )
  assert output.exists()
