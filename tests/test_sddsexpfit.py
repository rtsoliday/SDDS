import subprocess
from pathlib import Path
import pytest
import math

BIN_DIR = Path("bin/Linux-x86_64")
SDDSEXP = BIN_DIR / "sddsexpfit"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

required_bins = [SDDSEXP, PLAINDATA2SDDS, SDDSQUERY, SDDSPRINTOUT]

@pytest.mark.skipif(not all(p.exists() for p in required_bins), reason="required tools not built")
def test_sddsexpfit_options(tmp_path):
  plain = tmp_path / "data.txt"
  lines = []
  for x in range(5):
    y = 1 + 2 * math.exp(0.5 * x)
    lines.append(f"{x} {y}\n")
  plain.write_text("".join(lines))
  input_sdds = tmp_path / "input.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(plain),
    str(input_sdds),
    "-column=x,double",
    "-column=y,double",
    "-inputMode=ascii",
    "-outputMode=ascii",
    "-noRowCount",
  ], check=True)

  expected = {"expfitConstant": 1.0, "expfitFactor": 2.0, "expfitRate": 0.5}

  def query_params(path):
    result = subprocess.run([
      str(SDDSPRINTOUT),
      str(path),
      "-parameters=expfitConstant",
      "-parameters=expfitFactor",
      "-parameters=expfitRate",
      "-noTitle",
    ], capture_output=True, text=True, check=True)
    tokens = result.stdout.strip().split()
    vals = {
      tokens[0]: float(tokens[2]),
      tokens[3]: float(tokens[5]),
      tokens[6]: float(tokens[8]),
    }
    return vals

  def run_and_check(extra, label, use_pipe=False, check_fulloutput=False):
    if use_pipe:
      result = subprocess.run(
        [str(SDDSEXP), "-pipe", "-columns=x,y", *extra],
        input=input_sdds.read_bytes(),
        capture_output=True,
        check=True,
      )
      out = tmp_path / f"{label}.sdds"
      out.write_bytes(result.stdout)
    else:
      out = tmp_path / f"{label}.sdds"
      result = subprocess.run(
        [str(SDDSEXP), str(input_sdds), str(out), "-columns=x,y", *extra],
        capture_output=True,
        text=True,
        check=True,
      )
      if any("-verbosity" in arg for arg in extra):
        assert result.stderr.strip() != ""
    vals = query_params(out)
    for k, v in expected.items():
      assert math.isclose(vals[k], v, rel_tol=1e-3, abs_tol=1e-3)
    if check_fulloutput:
      cols = subprocess.run(
        [str(SDDSQUERY), str(out), "-columnlist"],
        capture_output=True,
        text=True,
        check=True,
      ).stdout
      assert "y" in cols
      assert "yResidual" in cols

  run_and_check([], "base")
  run_and_check(["-tolerance=1e-8"], "tolerance")
  run_and_check(["-verbosity=1"], "verbosity")
  run_and_check(["-clue=grows"], "clue")
  run_and_check(["-guess=1,2,0.5"], "guess")
  run_and_check(["-startValues=constant=1,factor=2,rate=0.5"], "startValues")
  run_and_check(["-fixValue=rate=0.5"], "fixValue")
  run_and_check(["-limits=evaluations=100,passes=10"], "limits")
  run_and_check(["-autoOffset"], "autoOffset")
  run_and_check(["-majorOrder=row"], "majorOrder")
  run_and_check(["-fulloutput"], "fulloutput", check_fulloutput=True)
  run_and_check([], "pipe", use_pipe=True)
