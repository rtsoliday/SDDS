import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSDISTEST = BIN_DIR / "sddsdistest"
C_SOURCE = Path("SDDSaps/sddsdistest.c")


def extract_options():
  text = C_SOURCE.read_text()
  match = re.search(r'static char \*option\[N_OPTIONS\] = \{([^}]+)\};', text, re.S)
  items = [s.strip().strip('"') for s in match.group(1).split(',') if s.strip()]
  return items

OPTIONS = extract_options()


@pytest.fixture(scope="module")
def usage():
  result = subprocess.run([str(SDDSDISTEST)], capture_output=True, text=True)
  assert result.returncode != 0
  return result.stdout


@pytest.mark.skipif(not SDDSDISTEST.exists(), reason="sddsdistest not built")
@pytest.mark.parametrize("opt", OPTIONS)
def test_option_usage(opt, usage):
  result = subprocess.run([str(SDDSDISTEST), f"-{opt}"], capture_output=True, text=True)
  assert result.returncode != 0
  assert result.stdout == usage
