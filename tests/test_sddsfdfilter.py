import subprocess
from pathlib import Path
import re
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSFDFILTER = BIN_DIR / "sddsfdfilter"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SOURCE_FILE = Path("SDDSaps/sddsfdfilter.c")

def extract_options():
  text = SOURCE_FILE.read_text()
  match = re.search(r'char \*option\[N_OPTIONS\]\s*=\s*\{([^}]+)\};', text, re.MULTILINE)
  return [line.strip().strip('\",') for line in match.group(1).splitlines() if line.strip()]

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

pytestmark = pytest.mark.skipif(
  not (SDDSFDFILTER.exists() and SDDS2STREAM.exists() and SDDSQUERY.exists() and PLAINDATA2SDDS.exists()),
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


def create_filter_file(tmp_path):
  plain = tmp_path / "filter.txt"
  plain.write_text("0 1\n1 1\n2 0.5\n")
  sdds = tmp_path / "filter.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      "-column=freq,double",
      "-column=mag,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return sdds


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


def test_new_and_difference_columns(tmp_path):
  output = tmp_path / "fdfilter.sdds"
  subprocess.run(
    [
      str(SDDSFDFILTER),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=shortCol,longCol",
      "-newColumns",
      "-differenceColumns",
      "-clip=high=1000",
      "-majorOrder=column",
    ],
    check=True,
  )
  columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
  assert "longColFiltered" in columns.stdout
  assert "longColDifference" in columns.stdout
  lines = stream_columns(output, "longCol,longColFiltered")
  assert lines[0].startswith("100|")


def test_pipe_output(tmp_path):
  result = subprocess.run(
    [
      str(SDDSFDFILTER),
      "SDDSlib/demo/example.sdds",
      "-columns=shortCol,longCol",
      "-clip=high=1000",
      "-pipe=out",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  output = tmp_path / "pipe-fdfilter.sdds"
  output.write_bytes(result.stdout)
  columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
  assert "longCol" in columns.stdout


def test_threshold_and_highpass_success(tmp_path):
  output = tmp_path / "threshold.sdds"
  subprocess.run(
    [
      str(SDDSFDFILTER),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=shortCol,longCol",
      "-threshold=level=0.1",
      "-highpass=start=0,end=1",
    ],
    check=True,
  )
  lines = stream_columns(output, "longCol")
  assert len(lines) >= 3


def test_lowpass_notch_bandpass_and_filterfile_success(tmp_path):
  filter_file = create_filter_file(tmp_path)
  output = tmp_path / "combo.sdds"
  subprocess.run(
    [
      str(SDDSFDFILTER),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=shortCol,longCol",
      "-lowpass=start=0,end=1",
      "-notch=center=1,flatWidth=0.1,fullWidth=0.2",
      "-bandpass=center=1,flatWidth=0.1,fullWidth=0.2",
      f"-filterfile=filename={filter_file},frequency=freq,magnitude=mag",
      "-cascade",
      "-newColumns",
    ],
    check=True,
  )
  columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
  assert "longColFiltered" in columns.stdout


def test_threshold_only_pipe_and_difference_columns(tmp_path):
  result = subprocess.run(
    [
      str(SDDSFDFILTER),
      "SDDSlib/demo/example.sdds",
      "-columns=shortCol,longCol",
      "-threshold=level=0.1,fractional,start=0,end=1",
      "-differenceColumns",
      "-pipe=out",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  output = tmp_path / "threshold_pipe.sdds"
  output.write_bytes(result.stdout)
  columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
  assert "longColDifference" in columns.stdout

