import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
RESMDERIV = BIN_DIR / "sddsrespmatrixderivative"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

TOOLS = [RESMDERIV, PLAINDATA2SDDS, SDDSPRINTOUT]

def run_printout(path, columns):
  args = [str(SDDSPRINTOUT), str(path)]
  for col in columns:
    args.append(f"-columns={col}")
  args += ["-noTitle", "-noLabels"]
  result = subprocess.run(args, capture_output=True, text=True, check=True)
  data = []
  for line in result.stdout.strip().splitlines():
    parts = line.split()
    if len(parts) == len(columns):
      data.append([float(x) for x in parts])
  return data


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_mode_cor(tmp_path):
  plain = tmp_path / "cor.txt"
  plain.write_text("B1 1 2\nB2 3 4\n")
  src = tmp_path / "cor.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(src),
      "-column=BPMName,string",
      "-column=C1,double",
      "-column=C2,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  out = tmp_path / "out_cor.sdds"
  subprocess.run([str(RESMDERIV), str(src), str(out), "-mode=cor"], check=True)
  data = run_printout(out, ["C1", "C2"])
  assert data == [[1, 0], [0, 2], [3, 0], [0, 4]]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_mode_bpm(tmp_path):
  plain = tmp_path / "bpm.txt"
  plain.write_text("B1 1 2\nB2 3 4\n")
  src = tmp_path / "bpm.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(src),
      "-column=BPMName,string",
      "-column=C1,double",
      "-column=C2,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  out = tmp_path / "out_bpm.sdds"
  subprocess.run([str(RESMDERIV), str(src), str(out), "-mode=bpm"], check=True)
  data = run_printout(out, ["B1", "B2"])
  assert data == [[1, 0], [2, 0], [0, 3], [0, 4]]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_mode_quad(tmp_path):
  plain = tmp_path / "quad.txt"
  plain.write_text("0 0 0\n")
  src = tmp_path / "quad.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(src),
      "-column=Q1,double",
      "-column=Q2,double",
      "-column=Q3,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  out = tmp_path / "out_quad.sdds"
  subprocess.run(
    [
      str(RESMDERIV),
      str(src),
      str(out),
      "-mode=quad",
      "-addRowsBefore=1",
      "-addRowsAfter=2",
    ],
    check=True,
  )
  data = run_printout(out, ["Q1", "Q2", "Q3"])
  assert data == [[0, 1, 0], [0, 0, 1]]
