import subprocess
from pathlib import Path
import re
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSFDFILTER = BIN_DIR / "sddsfdfilter"
SOURCE_FILE = Path("SDDSaps/sddsfdfilter.c")

def extract_options():
  text = SOURCE_FILE.read_text()
  match = re.search(r'char \*option\[N_OPTIONS\]\s*=\s*\{([^}]+)\};', text, re.MULTILINE)
  return [line.strip().strip('",') for line in match.group(1).splitlines() if line.strip()]

OPTIONS = extract_options()

EXPECTED = {
  'pipe': lambda out: f"error: too many filenames (sddsfdfilter)\n       offending argument is {out}\n",
  'cascade': lambda out: "warning: no filters specified (sddsfdfilter)\n",
  'clip': lambda out: "Error (bin/Linux-x86_64/sddsfdfilter): supply at least one of high=<number> or low=<number> with -clipFrequencies\n",
  'columns': lambda out: "Error (bin/Linux-x86_64/sddsfdfilter): invalid -columns syntax\n",
  'threshold': lambda out: "Error (bin/Linux-x86_64/sddsfdfilter): invalid -threshold syntax\n",
  'highpass': lambda out: "Error (bin/Linux-x86_64/sddsfdfilter): supply start=<value> and end=<value> qualifiers with -highpass and -lowpass\n",
  'lowpass': lambda out: "Error (bin/Linux-x86_64/sddsfdfilter): supply start=<value> and end=<value> qualifiers with -highpass and -lowpass\n",
  'notch': lambda out: "Error (bin/Linux-x86_64/sddsfdfilter): supply center=<value> and flatWidth=<value> qualifiers with -notch and -bandpass\n",
  'bandpass': lambda out: "Error (bin/Linux-x86_64/sddsfdfilter): supply center=<value> and flatWidth=<value> qualifiers with -notch and -bandpass\n",
  'filterfile': lambda out: "Error (bin/Linux-x86_64/sddsfdfilter): supply filename=<string> with -filterFile\n",
  'newcolumns': lambda out: "warning: no filters specified (sddsfdfilter)\n",
  'differencecolumns': lambda out: "warning: no filters specified (sddsfdfilter)\n",
  'exclude': lambda out: "Error (bin/Linux-x86_64/sddsfdfilter): invalid -exclude syntax\n",
  'majorOrder': lambda out: "warning: no filters specified (sddsfdfilter)\n",
}

@pytest.mark.skipif(not SDDSFDFILTER.exists(), reason="sddsfdfilter not built")
@pytest.mark.parametrize("opt", OPTIONS)
def test_sddsfdfilter_option(tmp_path, opt):
  out_file = tmp_path / "out.sdds"
  if opt == 'columns':
    cmd = [str(SDDSFDFILTER), "SDDSlib/demo/example.sdds", str(out_file), f"-{opt}"]
  else:
    cmd = [
      str(SDDSFDFILTER),
      "SDDSlib/demo/example.sdds",
      str(out_file),
      "-columns=longCol,shortCol",
      f"-{opt}",
    ]
  result = subprocess.run(cmd, capture_output=True, text=True)
  expected = EXPECTED[opt](str(out_file))
  assert result.stderr == expected
