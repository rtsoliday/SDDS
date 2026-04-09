import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSPROCESS = BIN_DIR / "sddsprocess"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDS2STREAM = BIN_DIR / "sdds2stream"

@pytest.mark.skipif(not (SDDSPROCESS.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddsprocess(tmp_path):
  output = tmp_path / "process.sdds"
  subprocess.run(
    [
      str(SDDSPROCESS),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-define=column,Index,i_row,type=long",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"


@pytest.mark.skipif(
  not (SDDSPROCESS.exists() and SDDSCHECK.exists() and SDDS2STREAM.exists()),
  reason="tools not built",
)
def test_sddsprocess_redefine_array_access_uses_current_page(tmp_path):
  input_file = tmp_path / "two-page-input.sdds"
  input_file.write_text(
    """SDDS1
&column name=Input, type=double, &end
&data mode=ascii, &end
3
1
2
3
3
10
20
30
""",
    encoding="ascii",
  )

  output = tmp_path / "process-array-access.sdds"
  subprocess.run(
    [
      str(SDDSPROCESS),
      str(input_file),
      str(output),
      "-define=column,A,Input,type=double",
      "-define=column,B,1 &A [,type=double",
    ],
    check=True,
  )

  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"

  page1 = subprocess.check_output(
    [str(SDDS2STREAM), str(output), "-page=1", "-columns=Input,A,B", "-delimiter=|"],
    text=True,
  ).strip().splitlines()
  page2 = subprocess.check_output(
    [str(SDDS2STREAM), str(output), "-page=2", "-columns=Input,A,B", "-delimiter=|"],
    text=True,
  ).strip().splitlines()

  assert page1 == [
    "1.000000000000000e+00|1.000000000000000e+00|2.000000000000000e+00",
    "2.000000000000000e+00|2.000000000000000e+00|2.000000000000000e+00",
    "3.000000000000000e+00|3.000000000000000e+00|2.000000000000000e+00",
  ]
  assert page2 == [
    "1.000000000000000e+01|1.000000000000000e+01|2.000000000000000e+01",
    "2.000000000000000e+01|2.000000000000000e+01|2.000000000000000e+01",
    "3.000000000000000e+01|3.000000000000000e+01|2.000000000000000e+01",
  ]
