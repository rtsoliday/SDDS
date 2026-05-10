import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCLIPTAILS = BIN_DIR / "sddscliptails"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"


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

pytestmark = pytest.mark.skipif(
  not (SDDSCLIPTAILS.exists() and SDDS2STREAM.exists() and SDDSQUERY.exists()),
  reason="tools not built",
)


def write_input(path):
  path.write_text(
    """SDDS1
&column name=signal, type=double, &end
&data mode=ascii, &end
7
0
1
10
1
0
0
0
""",
    encoding="ascii",
  )
  return path


def write_fwhm_input(path):
  path.write_text(
    """SDDS1
&column name=signal, type=double, &end
&data mode=ascii, &end
9
0
1
4
8
10
8
4
1
0
""",
    encoding="ascii",
  )
  return path


def stream_columns(path, columns):
  result = subprocess.run(
    [str(SDDS2STREAM), str(path), f"-columns={columns}", "-delimiter=|"],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout.strip().splitlines() if result.stdout.strip() else []


@pytest.mark.parametrize("opt", OPTIONS)
def test_invalid_option_invocation(opt):
  args, message = INVALID_ARGS[opt]
  result = subprocess.run([str(SDDSCLIPTAILS)] + args, capture_output=True, text=True)
  assert result.returncode != 0
  stderr_lines = [line.strip() for line in result.stderr.splitlines() if line.strip()]
  assert stderr_lines[-1] == f"Error ({SDDSCLIPTAILS}): {message}"


def test_fractional_clipping_and_intail(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "fractional.sdds"
  subprocess.run(
    [str(SDDSCLIPTAILS), str(input_file), str(output), "-columns=signal", "-fractional=0.2"],
    check=True,
  )
  lines = stream_columns(output, "signal,InTail")
  assert lines == [
    "0.000000000000000e+00|1",
    "0.000000000000000e+00|1",
    "1.000000000000000e+01|0",
    "0.000000000000000e+00|1",
    "0.000000000000000e+00|1",
    "0.000000000000000e+00|1",
    "0.000000000000000e+00|1",
  ]


def test_afterzero_pipe_and_major_order(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  result = subprocess.run(
    [str(SDDSCLIPTAILS), str(input_file), "-pipe=out", "-columns=signal", "-afterzero=1", "-majorOrder=column"],
    stdout=subprocess.PIPE,
    check=True,
  )
  output = tmp_path / "afterzero.sdds"
  output.write_bytes(result.stdout)
  columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
  assert "InTail" in columns.stdout


def test_absolute_clipping(tmp_path):
  input_file = write_input(tmp_path / "absolute.sdds")
  output = tmp_path / "absolute-out.sdds"
  subprocess.run(
    [str(SDDSCLIPTAILS), str(input_file), str(output), "-columns=signal", "-absolute=2"],
    check=True,
  )
  lines = stream_columns(output, "signal,InTail")
  assert lines[2] == "1.000000000000000e+01|0"
  assert lines[0].endswith("|1")


def test_fwhm_clipping(tmp_path):
  input_file = write_fwhm_input(tmp_path / "fwhm.sdds")
  output = tmp_path / "fwhm-out.sdds"
  subprocess.run(
    [str(SDDSCLIPTAILS), str(input_file), str(output), "-columns=signal", "-fwhm=0.5"],
    check=True,
  )
  lines = stream_columns(output, "signal,InTail")
  assert any(line.endswith("|1") for line in lines)
