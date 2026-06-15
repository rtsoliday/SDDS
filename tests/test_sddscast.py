import subprocess
from pathlib import Path
import re
import pytest

from sdds_test_utils import BIN_DIR
SDDSCAST = BIN_DIR / "sddscast"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSQUERY = BIN_DIR / "sddsquery"


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


@pytest.mark.skipif(not (SDDSCAST.exists() and SDDSCHECK.exists() and SDDSQUERY.exists()), reason="tools not built")
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
  query = subprocess.run([str(SDDSQUERY), str(output)], capture_output=True, text=True, check=True).stdout
  assert re.search(r"doubleCol\s+NULL\s+NULL\s+NULL\s+float\b", query)
