import subprocess
from pathlib import Path
import hashlib
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSCONVERTLOGONCHANGE = BIN_DIR / "sddsconvertlogonchange"

DATASET = """SDDS1
&description text="LogOnChange Example", &end
&array name=ReadbackName, type=string, &end
&array name=ControlName, type=string, &end
&column name=Time, type=double, &end
&column name=Value, type=double, &end
&column name=ControlNameIndex, type=long, &end
&column name=PreviousRow, type=long, &end
&data mode=ascii, &end
! page number 1
2           ! 1-dimensional array ReadbackName:
RB1 RB2
2           ! 1-dimensional array ControlName:
CN1 CN2
2
0 100 0 -1
1 200 1 0
"""

@pytest.mark.skipif(
  not (SDDSCHECK.exists() and SDDSCONVERTLOGONCHANGE.exists()),
  reason="tools not built",
)
@pytest.mark.parametrize(
  "option, expected, pipe",
  [
    ("-binary", "640cd3141b2491489099b451d29f51f699cd388bf221498ba758d2a21a924a7d", False),
    ("-ascii", "8dc06008f96a6e91ccf7b49469afcf8c01328ae4817ace52b4ba5578801cec4b", False),
    ("-double", "8dc06008f96a6e91ccf7b49469afcf8c01328ae4817ace52b4ba5578801cec4b", False),
    ("-float", "a8223ee05e3b25eb6771646c4f76edc71e4efddab1e7cad783fbe8cceb6899ad", False),
    ("-snapshot=0", "febea2138d01248bfe9d9f2da08eb44c632502510646c9729b57097efa6d3f0f", False),
    ("-minimumInterval=1.1", "febea2138d01248bfe9d9f2da08eb44c632502510646c9729b57097efa6d3f0f", False),
    ("-time=start=1", "1afea6b8523e34153528e51aa1c253055786b28018774a6c19d2827df8ba2076", False),
    ("-delete=RB2", "77dfd7838963d64164371a2e8047420a60e8f64b0dee35fad91f94cbf5896939", False),
    ("-retain=RB1", "77dfd7838963d64164371a2e8047420a60e8f64b0dee35fad91f94cbf5896939", False),
    ("-pipe=output", "8dc06008f96a6e91ccf7b49469afcf8c01328ae4817ace52b4ba5578801cec4b", True),
  ],
)
def test_sddsconvertlogonchange(tmp_path, option, expected, pipe):
  input_file = tmp_path / "input.sdds"
  input_file.write_text(DATASET)
  output_file = tmp_path / "out.sdds"
  cmd = [str(SDDSCONVERTLOGONCHANGE), str(input_file)]
  if pipe:
    cmd.append(option)
    with output_file.open("wb") as f:
      subprocess.run(cmd, stdout=f, check=True)
  else:
    cmd += [str(output_file), option]
    subprocess.run(cmd, check=True)
  result = subprocess.run([str(SDDSCHECK), str(output_file)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  data = output_file.read_bytes()
  assert hashlib.sha256(data).hexdigest() == expected
