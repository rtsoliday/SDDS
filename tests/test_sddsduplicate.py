import subprocess
from pathlib import Path
import re
import pytest

from sdds_test_utils import BIN_DIR
SDDSDUPLICATE = BIN_DIR / "sddsduplicate"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDSQUERY = BIN_DIR / "sddsquery"
SRC_PATH = Path("SDDSaps/sddsduplicate.c")


def get_usage():
  result = subprocess.run([str(SDDSDUPLICATE)], capture_output=True, text=True)
  return result.stdout


def get_options():
  text = SRC_PATH.read_text()
  match = re.search(r'static char \*option\[N_OPTIONS\] = {([^}]+)};', text, re.S)
  return re.findall(r'\"(.*?)\"', match.group(1))


pytestmark = pytest.mark.skipif(
  not (SDDSDUPLICATE.exists() and SDDSPRINTOUT.exists() and SDDSQUERY.exists()),
  reason="tools not built",
)


def write_input(path):
  path.write_text(
    """SDDS1
&parameter name=tag, type=string, &end
&column name=id, type=long, &end
&column name=weight, type=double, &end
&data mode=ascii, &end
sample
3
1 1.0
2 2.0
3 2.5
""",
    encoding="ascii",
  )
  return path


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


def ids(path):
  return [int(row[0]) for row in printout_rows(path, ["id"])]


def weights(path):
  return [float(row[0]) for row in printout_rows(path, ["weight"])]


def test_each_option_outputs_usage():
  expected = get_usage()
  for opt in get_options():
    cmd = [str(SDDSDUPLICATE), f"-{opt}=X"] if opt == "pipe" else [str(SDDSDUPLICATE), f"-{opt}"]
    result = subprocess.run(cmd, capture_output=True, text=True)
    assert result.stdout == expected


def test_fixed_factor_duplicates_all_rows(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "factor.sdds"
  subprocess.run([str(SDDSDUPLICATE), str(input_file), str(output), "-factor=2"], check=True)
  assert ids(output) == [1, 1, 2, 2, 3, 3]
  assert weights(output) == pytest.approx([1.0, 1.0, 2.0, 2.0, 2.5, 2.5])


def test_weight_column_and_maxfactor_scale_duplicates(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "maxfactor.sdds"
  subprocess.run(
    [str(SDDSDUPLICATE), str(input_file), str(output), "-weight=weight", "-maxFactor=4"],
    check=True,
  )
  assert ids(output) == [1, 2, 2, 2, 3, 3, 3, 3]


def test_weight_column_and_minfactor_scale_duplicates(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "minfactor.sdds"
  subprocess.run(
    [str(SDDSDUPLICATE), str(input_file), str(output), "-weight=weight", "-minFactor=2"],
    check=True,
  )
  assert ids(output) == [1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3]


def test_probabilistic_seed_is_deterministic(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output_a = tmp_path / "prob-a.sdds"
  output_b = tmp_path / "prob-b.sdds"
  args = ["-weight=weight", "-maxFactor=3", "-probabilistic", "-seed=7"]
  subprocess.run([str(SDDSDUPLICATE), str(input_file), str(output_a), *args], check=True)
  subprocess.run([str(SDDSDUPLICATE), str(input_file), str(output_b), *args], check=True)
  assert ids(output_a) == ids(output_b)


def test_pipe_output_matches_file_output(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  file_output = tmp_path / "file.sdds"
  subprocess.run([str(SDDSDUPLICATE), str(input_file), str(file_output), "-factor=3"], check=True)
  piped = subprocess.run(
    [str(SDDSDUPLICATE), str(input_file), "-factor=3", "-pipe=out"],
    stdout=subprocess.PIPE,
    check=True,
  )
  pipe_output = tmp_path / "pipe.sdds"
  pipe_output.write_bytes(piped.stdout)
  assert ids(pipe_output) == ids(file_output)


def test_pipe_input_and_verbosity(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "pipein.sdds"
  result = subprocess.run(
    [str(SDDSDUPLICATE), str(output), "-pipe=in", "-factor=2", "-verbosity=1"],
    input=input_file.read_bytes(),
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    check=True,
  )
  assert result.stdout == b""
  assert b"output rows" in result.stderr
  assert ids(output) == [1, 1, 2, 2, 3, 3]
