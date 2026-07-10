import subprocess
from pathlib import Path

import pytest

from sdds_test_utils import BIN_DIR


SDDS2MATH = BIN_DIR / "sdds2math"
EXAMPLE = Path("SDDSlib/demo/example.sdds").resolve()


pytestmark = pytest.mark.skipif(
  not SDDS2MATH.exists(), reason="sdds2math not built"
)


def test_file_conversion_includes_schema_types_and_all_pages(tmp_path):
  output = tmp_path / "example.m"
  subprocess.run([str(SDDS2MATH), str(EXAMPLE), str(output)], check=True)

  text = output.read_text()
  assert text.startswith('{{"Example SDDS Output","SDDS Example"}')
  assert '{"longCol","","","","long",0,"No description"}' in text
  assert '{"stringParam","","","","string","No description"}' in text
  assert '{"doubleArray","","","","double*^2",0,"","No description"}' in text
  assert '"FirstPage"' in text
  assert '"SecondPage"' in text
  assert text.rstrip().endswith("}")


def test_comments_format_and_verbose_output(tmp_path):
  output = tmp_path / "formatted.m"
  result = subprocess.run([
    str(SDDS2MATH),
    str(EXAMPLE),
    str(output),
    "-comments",
    "-format=%.3e",
    "-verbose",
  ], capture_output=True, text=True, check=True)

  text = output.read_text()
  assert "(*Table 1*)" in text
  assert "(*Table 2*)" in text
  assert "(* doubleParam *)2.718*10^0" in text
  assert "1.001*10^1" in text
  assert "protocol version 5" in result.stdout
  assert "11 columns of data" in result.stdout
  assert "11 parameters" in result.stdout
  assert "11 arrays of data" in result.stdout


def test_pipe_input_and_output():
  result = subprocess.run([
    str(SDDS2MATH),
    "-pipe=in,out",
  ], input=EXAMPLE.read_bytes(), capture_output=True, check=True)

  text = result.stdout.decode()
  assert text.startswith('{{"Example SDDS Output","SDDS Example"}')
  assert '"FirstPage"' in text
  assert '"SecondPage"' in text
  assert result.stderr == b""
