import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSDIFF = BIN_DIR / "sddsdiff"
SOURCE = Path("SDDSaps/sddsdiff.c")
EXAMPLE = Path("SDDSlib/demo/example.sdds")

# Extract option names from the source file
text = SOURCE.read_text()
match = re.search(r"char \*option\[N_OPTIONS\] = \{(.*?)\};", text, re.S)
option_names = [name.strip().strip('"') for name in match.group(1).split(',') if name.strip()]

# Map each option to arguments for invocation
option_args = {
  "compareCommon": ["-compareCommon=column"],
  "columns": ["-columns=doubleCol"],
  "parameters": ["-parameters=stringParam"],
  "arrays": ["-arrays=shortArray"],
  "tolerance": ["-tolerance=0.1"],
  "precision": ["-precision=1"],
  "format": ["-format=float=%f,double=%f,string=%s"],
  "exact": ["-exact"],
  "absolute": ["-absolute"],
  "rowlabel": ["-rowlabel=doubleCol"],
  "ignoreUnits": ["-ignoreUnits"],
}

missing = set(option_names) - option_args.keys()
assert not missing, f"Missing argument mapping for options: {missing}"

@pytest.mark.skipif(not SDDSDIFF.exists(), reason="sddsdiff not built")
@pytest.mark.parametrize("name", option_names)
def test_sddsdiff_options(name):
  args = option_args[name]
  result = subprocess.run(
    [str(SDDSDIFF), str(EXAMPLE), str(EXAMPLE), *args],
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  expected = f'"{EXAMPLE}" and "{EXAMPLE}" are identical.\n'
  assert result.stdout == expected
