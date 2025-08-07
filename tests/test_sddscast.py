import subprocess
from pathlib import Path
import hashlib
import re
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCAST = BIN_DIR / "sddscast"
SDDSCHECK = BIN_DIR / "sddscheck"


def extract_options():
  text = Path("SDDSaps/sddscast.c").read_text()
  match = re.search(r"char \*option\[.*?\]\s*=\s*{([^}]+)};", text, re.DOTALL)
  opts = []
  if match:
    for line in match.group(1).splitlines():
      line = line.strip().strip(",")
      if line:
        opts.append(line.strip('"'))
  return opts

# Mapping from option identifiers to arguments used for testing
OPTION_CASES = {
  "cast": ["-cast=column,doubleCol,double,float"],
  "nowarnings": ["-noWarnings", "-cast=column,doubleCol,double,float"],
  "pipe": ["-cast=column,doubleCol,double,float", "-pipe=output"],
  "majorOrder": ["-cast=column,doubleCol,double,float", "-majorOrder=column"],
}

OPTIONS = extract_options()
assert set(OPTION_CASES) == set(OPTIONS)
EXPECTED_HASH = "17b9bd8d5282fe3839b5372a5657a43dd59e7c7b7d0bcd5e78da69b85689e178"


@pytest.mark.skipif(not (SDDSCAST.exists() and SDDSCHECK.exists()), reason="tools not built")
@pytest.mark.parametrize("option", OPTIONS)
def test_sddscast_options(tmp_path, option):
  args = OPTION_CASES[option]
  output = tmp_path / "out.sdds"
  cmd = [str(SDDSCAST), "SDDSlib/demo/example.sdds"]
  if "-pipe=output" in args:
    cmd += args
    with output.open("wb") as f:
      subprocess.run(cmd, stdout=f, check=True)
  else:
    cmd += [str(output)] + args
    subprocess.run(cmd, check=True)
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  data = output.read_bytes()
  assert hashlib.sha256(data).hexdigest() == EXPECTED_HASH
