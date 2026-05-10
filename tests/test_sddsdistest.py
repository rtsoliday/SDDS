import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSDISTEST = BIN_DIR / "sddsdistest"
SDDSPROCESS = BIN_DIR / "sddsprocess"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
C_SOURCE = Path("SDDSaps/sddsdistest.c")


def extract_options():
  text = C_SOURCE.read_text()
  match = re.search(r'static char \*option\[N_OPTIONS\] = \{([^}]+)\};', text, re.S)
  items = [s.strip().strip('"') for s in match.group(1).split(',') if s.strip()]
  return items

OPTIONS = extract_options()
REQUIRED_TOOLS = [SDDSDISTEST, SDDSPROCESS, SDDSQUERY, SDDS2STREAM, SDDSPRINTOUT, PLAINDATA2SDDS]
pytestmark = pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="tools not built")


def query_columns(path):
  result = subprocess.run([str(SDDSQUERY), str(path), "-columnlist"], capture_output=True, text=True, check=True)
  return result.stdout.strip().splitlines()


def query_parameters(path):
  result = subprocess.run([str(SDDSQUERY), str(path), "-parameterlist"], capture_output=True, text=True, check=True)
  return result.stdout.strip().splitlines()


def stream_columns(path, columns):
  result = subprocess.run(
    [str(SDDS2STREAM), str(path), f"-columns={columns}", "-delimiter=|"],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout.strip().splitlines() if result.stdout.strip() else []


def printout_rows(path, columns):
  args = [str(SDDSPRINTOUT), str(path)]
  for column in columns:
    args.append(f"-columns={column}")
  args += ["-noTitle", "-noLabels"]
  result = subprocess.run(args, capture_output=True, text=True, check=True)
  rows = []
  for line in result.stdout.strip().splitlines():
    parts = line.split()
    if len(parts) == len(columns):
      rows.append(parts)
  return rows


def stream_parameter(path, parameter):
  result = subprocess.run([str(SDDS2STREAM), str(path), f"-parameters={parameter}"], capture_output=True, text=True, check=True)
  return result.stdout.strip()


def write_input(path):
  path.write_text(
    """SDDS1
&parameter name=nu, type=long, &end
&column name=gaussLike, type=double, &end
&column name=uniformLike, type=double, &end
&column name=studentLike, type=double, &end
&column name=studentSigma, type=double, &end
&data mode=ascii, &end
4
5
-1.3 -2.0 -2.2 1.0
-0.2 -0.5 -0.6 1.0
0.1 0.5 0.4 1.0
1.4 2.0 2.5 1.0
""",
    encoding="ascii",
  )
  return path


@pytest.fixture(scope="module")
def usage():
  result = subprocess.run([str(SDDSDISTEST)], capture_output=True, text=True)
  assert result.returncode != 0
  return result.stdout


@pytest.mark.parametrize("opt", OPTIONS)
def test_option_usage(opt, usage):
  result = subprocess.run([str(SDDSDISTEST), f"-{opt}"], capture_output=True, text=True)
  assert result.returncode != 0
  assert result.stdout == usage


def test_ks_gaussian_and_exclude_output(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "gaussian.sdds"
  subprocess.run(
    [
      str(SDDSDISTEST),
      str(input_file),
      str(output),
      "-column=*Like",
      "-exclude=uniformLike",
      "-gaussian",
      "-test=ks",
    ],
    check=True,
  )
  assert set(query_columns(output)) == {"ColumnName", "D", "distestSigLevel"}
  assert {"distestDistribution", "distestTest", "Count"}.issubset(set(query_parameters(output)))
  data = output.read_bytes()
  assert b"distestDistribution" in data
  assert b"gaussian" in data
  assert b"distestTest" in data
  assert b"ks" in data


def test_student_distribution_sigma_and_dof_parameter(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "student.sdds"
  subprocess.run(
    [
      str(SDDSDISTEST),
      str(input_file),
      str(output),
      "-column=studentLike,sigma=studentSigma",
      "-student",
      "-degreesOfFreedom=@nu",
    ],
    check=True,
  )
  assert set(query_columns(output)) == {"ColumnName", "D", "distestSigLevel"}
  data = output.read_bytes()
  assert b"distestDistribution" in data
  assert b"student" in data
  assert b"distestTest" in data
  assert b"ks" in data


def test_major_order_and_pipe_output(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "pipe.sdds"
  result = subprocess.run(
    [
      str(SDDSDISTEST),
      str(input_file),
      "-column=gaussLike",
      "-gaussian",
      "-pipe=out",
      "-majorOrder=column",
      "-threads=2",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  output.write_bytes(result.stdout)
  assert b"column_major_order=1" in result.stdout
  assert set(query_columns(output)) == {"ColumnName", "D", "distestSigLevel"}
  assert b"gaussian" in result.stdout


def test_pipe_input_matches_file_input(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  file_output = tmp_path / "file-output.sdds"
  subprocess.run(
    [str(SDDSDISTEST), str(input_file), str(file_output), "-column=gaussLike", "-gaussian"],
    check=True,
  )
  piped = subprocess.run(
    [str(SDDSDISTEST), "-pipe=in,out", "-column=gaussLike", "-gaussian"],
    input=input_file.read_bytes(),
    stdout=subprocess.PIPE,
    check=True,
  )
  pipe_output = tmp_path / "pipe-output.sdds"
  pipe_output.write_bytes(piped.stdout)
  file_rows = printout_rows(file_output, ["ColumnName", "D", "distestSigLevel"])
  pipe_rows = printout_rows(pipe_output, ["ColumnName", "D", "distestSigLevel"])
  assert pipe_rows == file_rows


def test_poisson_distribution(tmp_path):
  plain = tmp_path / "poisson.txt"
  plain.write_text("0\n1\n2\n3\n")
  data = tmp_path / "poisson.sdds"
  subprocess.run(
    [str(PLAINDATA2SDDS), str(plain), str(data), "-column=p,double", "-inputMode=ascii", "-outputMode=ascii", "-noRowCount"],
    check=True,
  )
  output = tmp_path / "poisson-out.sdds"
  subprocess.run([str(SDDSDISTEST), str(data), str(output), "-column=p", "-poisson"], check=True)
  assert set(query_columns(output)) == {"ColumnName", "D", "distestSigLevel"}
  assert b"poisson" in output.read_bytes()


def test_chisquared_test_reports_unimplemented(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  result = subprocess.run(
    [str(SDDSDISTEST), str(input_file), str(tmp_path / "chi.sdds"), "-column=gaussLike", "-gaussian", "-test=chisquared"],
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  assert (tmp_path / "chi.sdds").exists()
