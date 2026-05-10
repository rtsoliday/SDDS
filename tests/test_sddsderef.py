import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSDEREF = BIN_DIR / "sddsderef"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDSMAKE = BIN_DIR / "sddsmakedataset"

pytestmark = pytest.mark.skipif(
  not (SDDSDEREF.exists() and SDDS2STREAM.exists() and SDDSQUERY.exists() and SDDSMAKE.exists()),
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


def stream_parameter(path, parameter):
  result = subprocess.run(
    [str(SDDS2STREAM), str(path), f"-parameters={parameter}"],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout.strip()


def write_input(path):
  subprocess.run(
    [
      str(SDDSMAKE),
      str(path),
      "-ascii",
      "-parameter=indexParam,type=long",
      "-data=1",
      "-parameter=arrayIndexParam,type=long",
      "-data=2",
      "-column=indexCol,type=long",
      "-data=1,2,0",
      "-column=arrayIndex,type=long",
      "-data=0,3,1",
      "-column=source,type=double",
      "-data=10,20,30",
      "-array=grid,type=long",
      "-data=100,200,300,400",
    ],
    check=True,
  )
  return path


def query_columns(path):
  result = subprocess.run(
    [str(SDDSQUERY), str(path), "-columnlist"],
    capture_output=True,
    text=True,
    check=True,
  )
  return {line.strip() for line in result.stdout.splitlines() if line.strip()}


@pytest.mark.parametrize(
  "option,message",
  [
    ("-column", "invalid -column syntax"),
    ("-parameter", "invalid -parameter syntax"),
    ("-constant", "invalid -constant syntax"),
    ("-pipe=foo", "invalid -pipe syntax"),
    ("-outOfBounds", "invalid -outOfBounds syntax/values"),
    ("-majorOrder=foo", "invalid -majorOrder syntax/values"),
  ],
)
def test_sddsderef_option_errors(option, message):
  result = subprocess.run([str(SDDSDEREF), option], capture_output=True, text=True)
  assert result.returncode != 0
  assert result.stdout == ""
  assert result.stderr.strip().splitlines()[-1] == f"Error ({SDDSDEREF}): {message}"


def test_valid_source_reference_errors_are_specific(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  result = subprocess.run(
    [
      str(SDDSDEREF),
      str(input_file),
      str(tmp_path / "out.sdds"),
      "-column=copy,columnSource=missing,indexCol",
      "-majorOrder=column",
      "-outOfBounds=delete",
    ],
    capture_output=True,
    text=True,
  )
  assert result.returncode != 0
  assert "no column missing in input" in result.stderr


def test_parameter_and_constant_dereferences(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSDEREF),
      str(input_file),
      str(output),
      "-parameter=selected,columnSource=source,indexParam",
      "-constant=fixed,columnSource=source,2",
      "-parameter=arrayValue,arraySource=grid,arrayIndexParam",
      "-constant=arrayFixed,arraySource=grid,1",
      "-majorOrder=column",
    ],
    check=True,
  )
  assert stream_parameter(output, "selected") == "2.000000000000000e+01"
  assert stream_parameter(output, "fixed") == "3.000000000000000e+01"
  assert stream_parameter(output, "arrayValue") == "300"
  assert stream_parameter(output, "arrayFixed") == "200"


def test_out_of_bounds_delete_removes_rows_and_zeroes_parameter(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  output = tmp_path / "delete.sdds"
  subprocess.run(
    [
      str(SDDSDEREF),
      str(input_file),
      str(output),
      "-parameter=selected,columnSource=source,indexParam",
      "-constant=fixed,arraySource=grid,9",
      "-outOfBounds=delete",
    ],
    check=True,
  )
  assert stream_columns(output, "indexCol") == [
    "1",
    "2",
    "0",
  ]
  assert stream_parameter(output, "selected") == "2.000000000000000e+01"
  assert stream_parameter(output, "fixed") == "0"


def test_array_column_dereference_and_pipe_output(tmp_path):
  input_file = write_input(tmp_path / "input.sdds")
  result = subprocess.run(
    [
      str(SDDSDEREF),
      str(input_file),
      "-pipe=out",
      "-column=fromArray,arraySource=grid,arrayIndex",
      "-majorOrder=row",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  output = tmp_path / "pipe.sdds"
  output.write_bytes(result.stdout)
  assert "fromArray" in query_columns(output)
  assert stream_columns(output, "fromArray") == [
    "100",
    "400",
    "200",
  ]
