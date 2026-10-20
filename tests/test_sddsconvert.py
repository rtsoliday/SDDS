import gzip
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


def invalid_later_page(field_type):
  mapping = ', enum_values="0:OFF,10:ON,-1:FAULT"' if field_type == "enum" else ""
  invalid = "123" if field_type == "enum" else "invalid"
  version = 6 if field_type == "enum" else 1
  return (f'SDDS{version}\n&column name=state, type={field_type}{mapping} &end\n'
          '&data mode=ascii &end\n'
          f'1\n10\n1\n{invalid}\n1\n-1\n')


@pytest.mark.skipif(not SDDSCONVERT.exists(), reason="tools not built")
@pytest.mark.parametrize("field_type", ["enum", "long"])
@pytest.mark.parametrize("input_mode", ["file", "gzip", "pipe"])
def test_convert_reports_later_page_read_error(tmp_path, field_type, input_mode):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  data = invalid_later_page(field_type)
  cmd = [str(SDDSCONVERT)]
  input_text = None
  if input_mode == "pipe":
    cmd += [str(output), "-pipe=input"]
    input_text = data
  else:
    if input_mode == "gzip":
      source = source.with_suffix(".sdds.gz")
      source.write_bytes(gzip.compress(data.encode()))
    else:
      source.write_text(data, encoding="utf8")
    cmd += [str(source), str(output)]
  result = subprocess.run(cmd + ["-ascii"], input=input_text, capture_output=True, text=True)
  assert result.returncode > 0, "a page read error must not be treated as successful conversion"
  assert "SDDS data garbled" in result.stderr
  if field_type == "enum":
    assert "Unmapped enum code" in result.stderr


@pytest.mark.skipif(not SDDSCONVERT.exists(), reason="tools not built")
@pytest.mark.parametrize("field_type", ["enum", "long"])
def test_convert_preserves_input_after_later_page_read_error(tmp_path, field_type):
  source = tmp_path / "source.sdds"
  original = invalid_later_page(field_type).encode()
  source.write_bytes(original)
  result = subprocess.run([str(SDDSCONVERT), str(source), "-ascii"],
                          capture_output=True, text=True)
  assert result.returncode > 0
  assert source.read_bytes() == original
  assert not source.with_suffix(".sdds.bak").exists()


@pytest.mark.skipif(not all((BIN_DIR / tool).exists() for tool in ("sddscheck", "sddsconvert", "sdds2stream")),
                    reason="tools not built")
@pytest.mark.parametrize("option", ["-recover=clip", "-toPage=1"])
def test_convert_explicitly_keeps_valid_prefix(tmp_path, option):
  source = tmp_path / "source.sdds"
  source.write_text(invalid_later_page("enum"), encoding="utf8")
  subprocess.run([str(SDDSCONVERT), str(source), "-ascii", option],
                 capture_output=True, text=True, check=True)
  result = subprocess.run([str(SDDSCHECK), str(source)], capture_output=True, text=True, check=True)
  assert result.stdout.strip() == "ok"
  result = subprocess.run([str(BIN_DIR / "sdds2stream"), str(source), "-column=state", "-enumCodes", "-noquotes"],
                          capture_output=True, text=True, check=True)
  assert result.stdout.split() == ["10"]
