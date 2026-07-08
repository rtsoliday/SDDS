import math
import subprocess
import pytest

from sdds_test_utils import BIN_DIR

SDDSGROUPEDENVELOPE = BIN_DIR / "sddsgroupedenvelope"
SDDS2STREAM = BIN_DIR / "sdds2stream"


def write_grouped_inputs(input_dir):
  input_dir.mkdir()
  (input_dir / "test-01.sdds").write_text(
    "SDDS2\n"
    "&description text=\"grouped envelope test\" &end\n"
    "&parameter name=Group, type=string &end\n"
    "&parameter name=P, type=double &end\n"
    "&column name=x, type=double &end\n"
    "&column name=y, type=double &end\n"
    "&column name=w, type=double &end\n"
    "&data mode=ascii &end\n"
    "! page 1\n"
    "A\n"
    "1\n"
    "2\n"
    "1 10 1\n"
    "2 20 2\n"
    "! page 2\n"
    "B\n"
    "1\n"
    "2\n"
    "1 100 1\n"
    "2 200 1\n"
  )
  (input_dir / "test-02.sdds").write_text(
    "SDDS2\n"
    "&description text=\"grouped envelope test\" &end\n"
    "&parameter name=Group, type=string &end\n"
    "&parameter name=P, type=double &end\n"
    "&column name=x, type=double &end\n"
    "&column name=y, type=double &end\n"
    "&column name=w, type=double &end\n"
    "&data mode=ascii &end\n"
    "! page 1\n"
    "A\n"
    "3\n"
    "2\n"
    "1.5 14 3\n"
    "2.5 22 4\n"
    "! page 2\n"
    "B\n"
    "3\n"
    "2\n"
    "1.7 110 1\n"
    "2.7 210 1\n"
  )


def stream_columns(path, columns):
  result = subprocess.run(
    [str(SDDS2STREAM), f"-columns={','.join(columns)}", str(path)],
    capture_output=True,
    text=True,
    check=True,
  )
  return [[float(value) for value in line.split()] for line in result.stdout.strip().splitlines()]


@pytest.mark.skipif(
  not (SDDSGROUPEDENVELOPE.exists() and SDDS2STREAM.exists()),
  reason="sddsgroupedenvelope or sdds2stream not built",
)
def test_grouped_statistics_and_group_filter(tmp_path):
  input_dir = tmp_path / "input"
  output = tmp_path / "envelope"
  write_grouped_inputs(input_dir)

  subprocess.run(
    [
      str(SDDSGROUPEDENVELOPE),
      "-inputDir",
      str(input_dir),
      "-pattern",
      "test-*.sdds",
      "-groupBy",
      "Group",
      "-groupValue",
      "A",
      "-copy=x",
      "-maximum=y",
      "-minimum=y",
      "-mean=y",
      "-largest=y",
      "-signedLargest=y",
      "-sum=2,y",
      "-rms=y",
      "-standarddeviations=y",
      "-sigma=y",
      "-wmean=w,y",
      "-wstandarddeviations=w,y",
      "-wrms=w,y",
      "-wsigma=w,y",
      "-cmaximum=x,y",
      "-cminimum=x,y",
      "-pmaximum=P,y",
      "-pminimum=P,y",
      "-slope=P,y",
      "-intercept=P,y",
      "-exmmMean=y",
      "-output",
      str(output),
    ],
    check=True,
  )

  rows = stream_columns(
    output,
    [
      "x",
      "yMax",
      "yMin",
      "yMean",
      "yLargest",
      "ySignedLargest",
      "y2Sum",
      "yRms",
      "yStDev",
      "ySigma",
      "yWMean",
      "yWStDev",
      "yWRms",
      "yWSigma",
      "xCMaximumy",
      "xCMinimumy",
      "PPMaximumy",
      "PPMinimumy",
      "ySlope",
      "yIntercept",
      "yExmmMean",
    ],
  )

  expected = [
    [
      1,
      14,
      10,
      12,
      14,
      14,
      296,
      math.sqrt(148),
      math.sqrt(8),
      2,
      13,
      math.sqrt(6),
      math.sqrt(172),
      math.sqrt(3),
      1.5,
      1,
      3,
      1,
      2,
      8,
      10,
    ],
    [
      2,
      22,
      20,
      21,
      22,
      22,
      884,
      math.sqrt(442),
      math.sqrt(2),
      1,
      128 / 6,
      4 / 3,
      math.sqrt(456),
      math.sqrt(8 / 9),
      2.5,
      2,
      3,
      1,
      1,
      19,
      20,
    ],
  ]
  assert len(rows) == len(expected)
  for row, expected_row in zip(rows, expected):
    assert row == pytest.approx(expected_row)


@pytest.mark.skipif(
  not (SDDSGROUPEDENVELOPE.exists() and SDDS2STREAM.exists()),
  reason="sddsgroupedenvelope or sdds2stream not built",
)
def test_threads_match_serial_output(tmp_path):
  input_dir = tmp_path / "input"
  serial_output = tmp_path / "serial"
  threaded_output = tmp_path / "threaded"
  write_grouped_inputs(input_dir)

  base_command = [
    str(SDDSGROUPEDENVELOPE),
    "-inputDir",
    str(input_dir),
    "-pattern",
    "test-*.sdds",
    "-groupBy",
    "Group",
    "-copy=x",
    "-maximum=y",
    "-minimum=y",
    "-mean=y",
    "-sum=2,y",
    "-rms=y",
    "-wmean=w,y",
    "-cmaximum=x,y",
    "-pmaximum=P,y",
  ]
  subprocess.run(base_command + ["-output", str(serial_output)], check=True)
  subprocess.run(base_command + ["-threads=2", "-output", str(threaded_output)], check=True)

  columns = [
    "x",
    "yMax",
    "yMin",
    "yMean",
    "y2Sum",
    "yRms",
    "yWMean",
    "xCMaximumy",
    "PPMaximumy",
  ]
  serial_rows = stream_columns(serial_output, columns)
  threaded_rows = stream_columns(threaded_output, columns)

  assert len(threaded_rows) == len(serial_rows)
  for threaded_row, serial_row in zip(threaded_rows, serial_rows):
    assert threaded_row == pytest.approx(serial_row)


@pytest.mark.skipif(
  not (SDDSGROUPEDENVELOPE.exists() and SDDS2STREAM.exists()),
  reason="sddsgroupedenvelope or sdds2stream not built",
)
def test_output_path_is_current_directory_relative_and_skipped_as_input(tmp_path):
  input_dir = tmp_path / "data"
  output = input_dir / "test-output"
  write_grouped_inputs(input_dir)

  command = [
    str(SDDSGROUPEDENVELOPE),
    "-inputDir",
    "data",
    "-pattern",
    "test-*",
    "-groupBy=Group",
    "-copy=x",
    "-maximum=y",
    "-output",
    "data/test-output",
  ]
  subprocess.run(command, cwd=tmp_path, check=True)
  subprocess.run(command, cwd=tmp_path, check=True)

  assert output.exists()
  assert not (input_dir / "data" / "test-output").exists()
  rows = stream_columns(output, ["x", "yMax"])
  expected = [[1, 14], [2, 22], [1, 110], [2, 210]]
  assert len(rows) == len(expected)
  for row, expected_row in zip(rows, expected):
    assert row == pytest.approx(expected_row)


@pytest.mark.skipif(
  not SDDSGROUPEDENVELOPE.exists(),
  reason="sddsgroupedenvelope not built",
)
def test_requires_at_least_one_statistic_option(tmp_path):
  input_dir = tmp_path / "input"
  input_dir.mkdir()

  result = subprocess.run(
    [
      str(SDDSGROUPEDENVELOPE),
      "-inputDir",
      str(input_dir),
      "-pattern",
      "test-*.sdds",
      "-output",
      str(tmp_path / "out"),
    ],
    capture_output=True,
    text=True,
  )

  assert result.returncode != 0
  assert "at least one statistic option is required" in result.stderr


@pytest.mark.skipif(
  not SDDSGROUPEDENVELOPE.exists(),
  reason="sddsgroupedenvelope not built",
)
def test_threads_must_be_positive():
  result = subprocess.run(
    [
      str(SDDSGROUPEDENVELOPE),
      "-threads=0",
      "-mean=y",
    ],
    capture_output=True,
    text=True,
  )

  assert result.returncode != 0
  assert "-threads must be >= 1" in result.stderr
