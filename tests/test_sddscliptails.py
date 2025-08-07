import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCLIPTAILS = BIN_DIR / "sddscliptails"


def extract_options():
  c_path = Path("SDDSaps/sddscliptails.c")
  text = c_path.read_text()
  match = re.search(r"static char \*option\[N_OPTIONS\]\s*=\s*\{([^}]+)\};", text, re.MULTILINE)
  if not match:
    raise ValueError("option array not found")
  items = [s.strip().strip('"') for s in match.group(1).split(',') if s.strip()]
  return items

OPTIONS = extract_options()

INVALID_ARGS = {
  "fractional": (["-fractional"], "invalid -fractional syntax"),
  "absolute": (["-absolute"], "invalid -absolute syntax"),
  "fwhm": (["-fwhm"], "invalid -fwhm syntax"),
  "pipe": (["-pipe=foo"], "invalid -pipe syntax"),
  "columns": (["-columns"], "invalid -columns syntax"),
  "afterzero": (["-afterzero=0"], "invalid -afterZero syntax"),
  "majorOrder": (["-majorOrder=foo"], "invalid -majorOrder syntax/values"),
}

assert set(OPTIONS) == set(INVALID_ARGS)


@pytest.mark.skipif(not SDDSCLIPTAILS.exists(), reason="sddscliptails not built")
@pytest.mark.parametrize("opt", OPTIONS)
def test_invalid_option_invocation(opt):
  args, message = INVALID_ARGS[opt]
  result = subprocess.run([str(SDDSCLIPTAILS)] + args, capture_output=True, text=True)
  assert result.returncode != 0
  stderr_lines = [line.strip() for line in result.stderr.splitlines() if line.strip()]
  assert stderr_lines[-1] == f"Error ({SDDSCLIPTAILS}): {message}"
