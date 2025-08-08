import subprocess
from pathlib import Path
import re
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSDIGFILTER = BIN_DIR / "sddsdigfilter"
SOURCE_FILE = Path("SDDSaps/sddsdigfilter.c")


def extract_options():
  text = SOURCE_FILE.read_text()
  match = re.search(r'char \*option\[N_OPTIONS\]\s*=\s*\{([^}]+)\};', text, re.MULTILINE)
  return [line.strip().strip('",') for line in match.group(1).splitlines() if line.strip()]


OPTIONS = extract_options()

EXPECTED = {
  'lowpass': lambda out: "Error (bin/Linux-x86_64/sddsdigfilter): Invalid -lowpass option.\n",
  'highpass': lambda out: "Error (bin/Linux-x86_64/sddsdigfilter): Invalid -highpass option.\n",
  'proportional': lambda out: "Error (bin/Linux-x86_64/sddsdigfilter): Invalid -proportional option.\n",
  'analogfilter': lambda out: "Error (bin/Linux-x86_64/sddsdigfilter): Invalid -analogfilter option.\n",
  'digitalfilter': lambda out: "Error (bin/Linux-x86_64/sddsdigfilter): Invalid -digitalfilter option.\n",
  'cascade': lambda out: "no filter or no columns supplied.\n",
  'columns': lambda out: "Error (bin/Linux-x86_64/sddsdigfilter): Invalid -column option.\n",
  'pipe': lambda out: f"error: too many filenames (sddsdigfilter)\n       offending argument is {out}\n",
  'verbose': lambda out: "no filter or no columns supplied.\n",
  'majorOrder': lambda out: "unknown keyword/value given: foo\nError (bin/Linux-x86_64/sddsdigfilter): invalid -majorOrder syntax/values\n",
}


@pytest.mark.skipif(not SDDSDIGFILTER.exists(), reason="sddsdigfilter not built")
@pytest.mark.parametrize("opt", OPTIONS)
def test_sddsdigfilter_option(tmp_path, opt):
  out_file = tmp_path / "out.sdds"
  base_cmd = [str(SDDSDIGFILTER), "SDDSlib/demo/example.sdds", str(out_file)]
  if opt == 'columns':
    cmd = base_cmd + [f"-{opt}"]
  elif opt == 'majorOrder':
    cmd = base_cmd + ["-columns=longCol,shortCol", f"-majorOrder=foo"]
  else:
    cmd = base_cmd + ["-columns=longCol,shortCol", f"-{opt}"]
  result = subprocess.run(cmd, capture_output=True, text=True)
  expected = EXPECTED[opt](str(out_file))
  assert result.stderr == expected
