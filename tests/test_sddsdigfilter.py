import subprocess
from pathlib import Path
import re
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSDIGFILTER = BIN_DIR / "sddsdigfilter"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"
SOURCE_FILE = Path("SDDSaps/sddsdigfilter.c")


def extract_options():
  text = SOURCE_FILE.read_text()
  match = re.search(r'char \*option\[N_OPTIONS\]\s*=\s*\{([^}]+)\};', text, re.MULTILINE)
  return [line.strip().strip('\",') for line in match.group(1).splitlines() if line.strip()]


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

pytestmark = pytest.mark.skipif(
  not (SDDSDIGFILTER.exists() and SDDS2STREAM.exists() and SDDSQUERY.exists()),
  reason="tools not built",
)


def stream_columns(path, columns):
  result = subprocess.run(
    [str(SDDS2STREAM), str(path), f"-columns={columns}", "-delimiter=|"],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout.strip().splitlines() if result.stdout.strip() else []


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


def test_proportional_filter_output(tmp_path):
  output = tmp_path / "digfilter.sdds"
  subprocess.run(
    [
      str(SDDSDIGFILTER),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=shortCol,longCol",
      "-proportional=2",
      "-majorOrder=column",
    ],
    check=True,
  )
  columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
  assert "DigFilteredlongCol" in columns.stdout
  lines = stream_columns(output, "longCol,DigFilteredlongCol")
  assert lines[:3] == [
    "100|2.000000000000000e+02",
    "200|4.000000000000000e+02",
    "300|6.000000000000000e+02",
  ]


def test_pipe_and_verbose(tmp_path):
  result = subprocess.run(
    [
      str(SDDSDIGFILTER),
      "-pipe=in,out",
      "-columns=shortCol,longCol",
      "-proportional=1",
      "-verbose",
    ],
    input=Path("SDDSlib/demo/example.sdds").read_bytes(),
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    check=True,
  )
  output = tmp_path / "pipe-digfilter.sdds"
  output.write_bytes(result.stdout)
  assert b"DigFilteredlongCol" in result.stdout


def test_major_order_proportional_output(tmp_path):
  output = tmp_path / "major.sdds"
  subprocess.run(
    [
      str(SDDSDIGFILTER),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=shortCol,longCol",
      "-proportional=0.5",
      "-majorOrder=row",
    ],
    check=True,
  )
  lines = stream_columns(output, "longCol,DigFilteredlongCol")
  assert lines[0].startswith("100|")

