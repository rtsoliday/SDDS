import subprocess
from pathlib import Path
import pytest

from sdds_test_utils import BIN_DIR
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSCONVERT = BIN_DIR / "sddsconvert"
SDDSQUERY = BIN_DIR / "sddsquery"


def query(path):
  return subprocess.run([str(SDDSQUERY), str(path)], capture_output=True, text=True, check=True).stdout


def column_list(path):
  result = subprocess.run([str(SDDSQUERY), "-columnlist", str(path)], capture_output=True, text=True, check=True)
  return result.stdout.strip().splitlines()

@pytest.mark.skipif(not (SDDSCHECK.exists() and SDDSCONVERT.exists()), reason="tools not built")
def test_sddsconvert(tmp_path):
  binary_file = tmp_path / "binary.sdds"
  ascii_file = tmp_path / "ascii.sdds"

  subprocess.run([str(SDDSCONVERT), "SDDSlib/demo/example.sdds", str(binary_file), "-binary"], check=True)
  result = subprocess.run([str(SDDSCHECK), str(binary_file)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"

  subprocess.run([str(SDDSCONVERT), str(binary_file), str(ascii_file), "-ascii"], check=True)
  result = subprocess.run([str(SDDSCHECK), str(ascii_file)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not (SDDSCHECK.exists() and SDDSCONVERT.exists() and SDDSQUERY.exists()), reason="tools not built")
@pytest.mark.parametrize(
  "option, extension",
  [
    ("-binary", "sdds"),
    ("-ascii", "sdds"),
    ("-delete=column,shortCol", "sdds"),
    ("-retain=column,shortCol", "sdds"),
    ("-rename=column,shortCol=shortCol2", "sdds"),
    ("-description=test,contents", "sdds"),
    ("-table=1", "sdds"),
    ("-editnames=column,*Col,%g/Col/New/", "sdds"),
    ("-linesperrow=1", "sdds"),
    ("-nowarnings", "sdds"),
    ("-recover", "sdds"),
    ("-pipe=output", "sdds"),
    ("-fromPage=1", "sdds"),
    ("-toPage=1", "sdds"),
    ("-acceptAllNames", "sdds"),
    ("-removePages=1", "sdds"),
    ("-keepPages=1", "sdds"),
    ("-rowlimit=1", "sdds"),
    ("-majorOrder=column", "sdds"),
    ("-convertUnits=column,doubleCol,m", "sdds"),
    ("-xzLevel=0", "sdds.xz"),
  ],
)
def test_sddsconvert_options(tmp_path, option, extension):
  output = tmp_path / f"out.{extension}"
  cmd = [str(SDDSCONVERT), "SDDSlib/demo/example.sdds"]
  if option.startswith("-pipe"):
    cmd.append(option)
    with output.open("wb") as f:
      subprocess.run(cmd, stdout=f, check=True)
  else:
    cmd += [str(output), option]
    subprocess.run(cmd, check=True)
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  assert output.stat().st_size > 0
  if option == "-binary":
    assert "binary" in query(output)
  elif option == "-ascii":
    assert "data is ASCII" in query(output)
  elif option == "-delete=column,shortCol":
    assert "shortCol" not in column_list(output)
  elif option == "-retain=column,shortCol":
    assert column_list(output) == ["shortCol"]
  elif option == "-rename=column,shortCol=shortCol2":
    columns = column_list(output)
    assert "shortCol2" in columns
    assert "shortCol" not in columns
  elif option == "-description=test,contents":
    info = query(output)
    assert "description: test" in info
    assert "contents: contents" in info
