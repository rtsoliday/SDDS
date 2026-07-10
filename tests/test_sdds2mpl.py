import shutil
import subprocess
from pathlib import Path

import pytest

from sdds_test_utils import BIN_DIR


SDDS2MPL = BIN_DIR / "sdds2mpl"
EXAMPLE = Path("SDDSlib/demo/example.sdds").resolve()


pytestmark = pytest.mark.skipif(
  not SDDS2MPL.exists(), reason="sdds2mpl not built"
)


def mpl_data(path):
  """Return the declared point count and numeric rows from an MPL file."""
  lines = path.read_text().splitlines()
  count_index = next(
    i for i, line in enumerate(lines) if line.strip().isdigit() and i >= 3
  )
  return int(lines[count_index]), [
    [float(value) for value in line.split()]
    for line in lines[count_index + 1:]
    if line.strip()
  ]


def test_column_and_parameter_output_arities_and_default_rootname(tmp_path):
  source = tmp_path / "input.sdds"
  shutil.copyfile(EXAMPLE, source)

  subprocess.run([
    str(SDDS2MPL),
    str(source),
    "-output=column,longCol,doubleCol",
    "-output=column,shortCol,floatCol,longCol",
    "-output=column,ushortCol,longdoubleCol,ulongCol,doubleCol",
    "-output=parameter,longParam,doubleParam",
  ], check=True)

  count, rows = mpl_data(tmp_path / "input_longCol_doubleCol.out")
  assert count == 8
  assert len(rows) == 8
  assert all(len(row) == 2 for row in rows)
  assert rows[0] == pytest.approx([100, 10.01])
  assert rows[-1] == pytest.approx([800, 80.08])

  count, rows = mpl_data(tmp_path / "input_shortCol_floatCol.out")
  assert count == 8
  assert all(len(row) == 3 for row in rows)
  assert rows[0] == pytest.approx([1, 1.1, 100])

  count, rows = mpl_data(tmp_path / "input_ushortCol_longdoubleCol.out")
  assert count == 8
  assert all(len(row) == 4 for row in rows)
  assert rows[0] == pytest.approx([1, 10.01, 100, 10.01])

  count, rows = mpl_data(tmp_path / "input_longParam_doubleParam.out")
  assert count == 2
  assert rows[0] == pytest.approx([1000, 2.71828])
  assert rows[1] == pytest.approx([2000, 1.41421])


def test_separate_pages_labels_rootname_and_announcements(tmp_path):
  root = tmp_path / "result"
  result = subprocess.run([
    str(SDDS2MPL),
    str(EXAMPLE),
    f"-rootname={root}",
    "-output=column,longCol,doubleCol,floatCol",
    "-output=parameter,longParam,doubleParam",
    "-labelParameters=stringParam=%s,longParam=%06ld",
    "-separatePages",
    "-announceOpenings",
  ], capture_output=True, text=True, check=True)

  first = tmp_path / "result_000_longCol_doubleCol.out"
  second = tmp_path / "result_001_longCol_doubleCol.out"
  parameter = tmp_path / "result_longParam_doubleParam.out"
  assert first.exists() and second.exists() and parameter.exists()
  assert result.stderr.count("file opened:") == 3

  first_text = first.read_text()
  second_text = second.read_text()
  assert "stringParam=FirstPage" in first_text
  assert "longParam=001000" in first_text
  assert "stringParam=SecondPage" in second_text
  assert "longParam=002000" in second_text
  assert mpl_data(first)[0] == 5
  assert mpl_data(second)[0] == 3

  # Parameter output remains one file even when column pages are separated.
  assert mpl_data(parameter)[0] == 2


def test_pipe_input(tmp_path):
  result = subprocess.run([
    str(SDDS2MPL),
    "-pipe=input",
    "-rootname=piped",
    "-output=column,longCol,doubleCol",
  ], input=EXAMPLE.read_bytes(), cwd=tmp_path, capture_output=True, check=True)

  assert result.stderr == b""
  count, rows = mpl_data(tmp_path / "piped_longCol_doubleCol.out")
  assert count == 8
  assert rows[0] == pytest.approx([100, 10.01])

