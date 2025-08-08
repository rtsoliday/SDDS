import subprocess
from pathlib import Path
import pytest
import math

BIN_DIR = Path("bin/Linux-x86_64")
SDDSLFT = BIN_DIR / "sddslorentzianfit"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

required_bins = [SDDSLFT, PLAINDATA2SDDS, SDDSQUERY, SDDSPRINTOUT]

@pytest.mark.skipif(not all(p.exists() for p in required_bins), reason="required tools not built")
def test_sddslorentzianfit_options(tmp_path):
  plain = tmp_path / "data.txt"
  baseline, height, center, gamma = 1.0, 5.0, 0.5, 0.3
  lines = []
  for i in range(-50, 51):
    x = i * 0.1
    y = baseline + height * gamma ** 2 / (gamma ** 2 + (x - center) ** 2)
    lines.append(f"{x} {y} 0.1\n")
  plain.write_text("".join(lines))
  input_sdds = tmp_path / "input.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(plain),
    str(input_sdds),
    "-column=x,double",
    "-column=y,double",
    "-column=sy,double",
    "-inputMode=ascii",
    "-outputMode=ascii",
    "-noRowCount",
  ], check=True)

  expected = {
    "lorentzianfitBaseline": baseline,
    "lorentzianfitHeight": height,
    "lorentzianfitCenter": center,
    "lorentzianfitGamma": gamma,
  }

  def query_params(path):
    result = subprocess.run([
      str(SDDSPRINTOUT),
      str(path),
      "-parameters=lorentzianfitBaseline",
      "-parameters=lorentzianfitHeight",
      "-parameters=lorentzianfitCenter",
      "-parameters=lorentzianfitGamma",
      "-noTitle",
    ], capture_output=True, text=True, check=True)
    tokens = result.stdout.strip().split()
    vals = {tokens[i]: float(tokens[i + 2]) for i in range(0, len(tokens), 3)}
    return vals

  def run_and_check(extra, label, use_pipe=False, check_fulloutput=False):
    if use_pipe:
      result = subprocess.run([
        str(SDDSLFT),
        "-pipe",
        "-columns=x,y,ySigma=sy",
        *extra,
      ], input=input_sdds.read_bytes(), capture_output=True, check=True)
      out = tmp_path / f"{label}.sdds"
      out.write_bytes(result.stdout)
    else:
      out = tmp_path / f"{label}.sdds"
      result = subprocess.run([
        str(SDDSLFT),
        str(input_sdds),
        str(out),
        "-columns=x,y,ySigma=sy",
        *extra,
      ], capture_output=True, text=True, check=True)
      if any("-verbosity" in arg for arg in extra):
        assert result.stderr.strip() != ""
    vals = query_params(out)
    for k, v in expected.items():
      assert math.isclose(vals[k], v, rel_tol=1e-3, abs_tol=1e-3)
    if check_fulloutput:
      cols = subprocess.run([
        str(SDDSQUERY),
        str(out),
        "-columnlist",
      ], capture_output=True, text=True, check=True).stdout
      assert "y" in cols
      assert "yResidual" in cols
      assert "yFit" in cols

  run_and_check([], "base")
  run_and_check(["-tolerance=1e-8"], "tolerance")
  run_and_check(["-verbosity=1"], "verbosity")
  run_and_check(["-guesses=baseline=0,center=0,height=1,gamma=1"], "guesses")
  run_and_check(["-stepSize=0.1"], "stepSize")
  run_and_check(["-fixValue=baseline=1"], "fixValue")
  run_and_check(["-limits=evaluations=100,passes=10"], "limits")
  run_and_check(["-majorOrder=row"], "majorOrder")
  run_and_check(["-fullOutput"], "fullOutput", check_fulloutput=True)
  run_and_check(["-fitRange=-1,1"], "fitRange")
  run_and_check([], "pipe", use_pipe=True)
