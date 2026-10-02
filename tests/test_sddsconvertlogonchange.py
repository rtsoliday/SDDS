import subprocess
from pathlib import Path
import hashlib
import pytest

from sdds_test_utils import BIN_DIR
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
    # ASCII snapshots include all 17 significant digits of double values.
    ("-ascii", "90c555bf10f90a8c7ae5c9b7d03684bd00b0392c976dcde9260a92c58c7361ff", False),
    ("-double", "90c555bf10f90a8c7ae5c9b7d03684bd00b0392c976dcde9260a92c58c7361ff", False),
    ("-float", "3f8c484b1faa7d71fcfadf425f538aad38781f85c2ca1ba4ebd2c8806c7b9b28", False),
    ("-snapshot=0", "342847524b7099a2f833ea13f49904f1c415c7236d2d9817b1188d04ed3f9134", False),
    ("-minimumInterval=1.1", "342847524b7099a2f833ea13f49904f1c415c7236d2d9817b1188d04ed3f9134", False),
    ("-time=start=1", "78a1d45dd197f15f6673ac6236d36ce511d4a3405d1acce601eb8abf43d9ed82", False),
    ("-delete=RB2", "5e47729aca86d8b654532e314d8964c0be0e0f35f3f6ceac95798f37ab46942d", False),
    ("-retain=RB1", "5e47729aca86d8b654532e314d8964c0be0e0f35f3f6ceac95798f37ab46942d", False),
    ("-pipe=output", "90c555bf10f90a8c7ae5c9b7d03684bd00b0392c976dcde9260a92c58c7361ff", True),
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
