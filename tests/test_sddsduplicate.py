import subprocess
from pathlib import Path
import re
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSDUPLICATE = BIN_DIR / "sddsduplicate"
SRC_PATH = Path("SDDSaps/sddsduplicate.c")


def get_usage():
  result = subprocess.run([str(SDDSDUPLICATE)], capture_output=True, text=True)
  return result.stdout


def get_options():
  text = SRC_PATH.read_text()
  match = re.search(r'static char \*option\[N_OPTIONS\] = {([^}]+)};', text, re.S)
  return re.findall(r"\"(.*?)\"", match.group(1))


@pytest.mark.skipif(not SDDSDUPLICATE.exists(), reason="sddsduplicate not built")
def test_each_option_outputs_usage():
  expected = get_usage()
  for opt in get_options():
    cmd = [str(SDDSDUPLICATE), f'-{opt}=X'] if opt == 'pipe' else [str(SDDSDUPLICATE), f'-{opt}']
    result = subprocess.run(cmd, capture_output=True, text=True)
    assert result.stdout == expected
