import math
import subprocess
from pathlib import Path

import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSNORMALIZE = BIN_DIR / "sddsnormalize"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED_TOOLS = [SDDSNORMALIZE, PLAINDATA2SDDS, SDDS2PLAINDATA]

VALUES = [-5, -4, 1, 2]
XVALUES = [0, 1, 2, 3]
MODES = [
  "minimum",
  "maximum",
  "largest",
  "signedlargest",
  "spread",
  "rms",
  "standarddeviation",
  "sum",
  "area",
  "average",
]


def expected_values(mode):
  y = VALUES
  x = XVALUES
  if mode == "minimum":
    factor = min(y)
  elif mode == "maximum":
    factor = max(y)
  elif mode == "largest":
    factor = max(abs(min(y)), abs(max(y)))
  elif mode == "signedlargest":
    factor = min(y) if abs(min(y)) > abs(max(y)) else max(y)
  elif mode == "spread":
    factor = max(y) - min(y)
  elif mode == "rms":
    factor = math.sqrt(sum(v * v for v in y) / len(y))
  elif mode == "standarddeviation":
    mean = sum(y) / len(y)
    factor = math.sqrt(sum((v - mean) ** 2 for v in y) / (len(y) - 1))
  elif mode == "sum":
    factor = sum(y)
  elif mode == "area":
    factor = sum(0.5 * (x[i + 1] - x[i]) * (y[i] + y[i + 1]) for i in range(len(y) - 1))
  elif mode == "average":
    factor = sum(y) / len(y)
  else:
    raise ValueError(mode)
  return [v / factor for v in y]

EXPECTED = {mode: expected_values(mode) for mode in MODES}


@pytest.mark.skipif(
  not all(tool.exists() for tool in REQUIRED_TOOLS),
  reason="sdds tools not built",
)
class TestSDDSNormalize:
  def create_sdds(self, tmp_path):
    plain = tmp_path / "data.txt"
    plain.write_text("\n".join(f"{x} {y}" for x, y in zip(XVALUES, VALUES)) + "\n")
    sdds = tmp_path / "input.sdds"
    subprocess.run(
      [
        str(PLAINDATA2SDDS),
        str(plain),
        str(sdds),
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
        "-column=x,double",
        "-column=y,double",
      ],
      check=True,
    )
    return sdds

  def read_column(self, path):
    plain = path.with_suffix(".txt")
    subprocess.run(
      [
        str(SDDS2PLAINDATA),
        str(path),
        str(plain),
        "-outputMode=ascii",
        "-noRowCount",
        "-column=y",
      ],
      check=True,
    )
    return [float(x) for x in plain.read_text().split()]

  @pytest.mark.parametrize("mode", MODES)
  def test_modes(self, mode, tmp_path):
    input_sdds = self.create_sdds(tmp_path)
    output = tmp_path / f"out_{mode}.sdds"
    if mode == "area":
      columns = f"-columns=mode={mode},functionOf=x,y"
    else:
      columns = f"-columns=mode={mode},y"
    subprocess.run([str(SDDSNORMALIZE), str(input_sdds), str(output), columns], check=True)
    data = self.read_column(output)
    assert data == pytest.approx(EXPECTED[mode])
