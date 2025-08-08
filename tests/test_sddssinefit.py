import subprocess
from pathlib import Path
import pytest
import math

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSINE = BIN_DIR / "sddssinefit"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

required_bins = [SDDSSINE, PLAINDATA2SDDS, SDDSQUERY, SDDSPRINTOUT]

@pytest.mark.skipif(not all(p.exists() for p in required_bins), reason="required tools not built")
def test_sddssinefit_options(tmp_path):
  def create_input(func, name):
    plain = tmp_path / f"{name}.txt"
    lines = []
    for i in range(1000):
      x = i*0.01
      y = func(x)
      lines.append(f"{x} {y}\n")
    plain.write_text("".join(lines))
    out = tmp_path / f"{name}.sdds"
    subprocess.run([
      str(PLAINDATA2SDDS),
      str(plain),
      str(out),
      "-column=x,double",
      "-column=y,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ], check=True)
    return out
  xs = [i*0.01 for i in range(1000)]
  base = create_input(lambda x: 1 + 2 * math.sin(2 * math.pi * 0.5 * x + 0.1), "base")
  slope = create_input(lambda x: 1 + 2 * math.sin(2 * math.pi * 0.5 * x + 0.1) + 0.3 * x, "slope")
  grow = create_input(lambda x: 1 + 2 * math.sin(2 * math.pi * 0.5 * x + 0.1) * math.exp(0.1 * x), "grow")
  decay = create_input(lambda x: 1 + 2 * math.sin(2 * math.pi * 0.5 * x + 0.1) * math.exp(-0.1 * x), "decay")
  ys_base = [1 + 2 * math.sin(2 * math.pi * 0.5 * x + 0.1) for x in xs]
  def freq_guess(xs, ys):
    zeroes = 0
    first = last = None
    i = 1
    while i < len(xs):
      if ys[i] * ys[i - 1] <= 0:
        mid = (xs[i] + xs[i - 1]) / 2
        if zeroes == 0:
          first = mid
        last = mid
        zeroes += 1
        i += 1
        if zeroes > 5:
          break
      i += 1
    if zeroes == 0:
      return 2 / abs(xs[-1] - xs[0])
    return zeroes / (2 * abs(last - first))
  freq0 = freq_guess(xs, ys_base)

  expected = {
    "sinefitConstant": 1.0,
    "sinefitFactor": 2.0,
    "sinefitFrequency": 0.5,
    "sinefitPhase": 0.1,
    "sinefitRmsResidual": 0.0,
  }

  def query(path, extras=None):
    params = [
      "sinefitConstant",
      "sinefitFactor",
      "sinefitFrequency",
      "sinefitPhase",
      "sinefitRmsResidual",
    ]
    if extras:
      params.extend(extras)
    cmd = [str(SDDSPRINTOUT), str(path)]
    for p in params:
      cmd.append(f"-parameters={p}")
    cmd.append("-noTitle")
    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    tokens = result.stdout.strip().split()
    vals = {}
    for i in range(0, len(tokens), 3):
      vals[tokens[i]] = float(tokens[i + 2])
    return vals

  def run_and_check(extra, label, input_file=base, expected_vals=None, use_pipe=False, check_fulloutput=False):
    if expected_vals is None:
      expected_vals = expected
    if use_pipe:
      proc = subprocess.run([
        str(SDDSSINE),
        "-pipe",
        "-columns=x,y",
        *extra,
      ], input=input_file.read_bytes(), capture_output=True, check=True)
      out = tmp_path / f"{label}_out.sdds"
      out.write_bytes(proc.stdout)
    else:
      out = tmp_path / f"{label}_out.sdds"
      proc = subprocess.run([
        str(SDDSSINE),
        str(input_file),
        str(out),
        "-columns=x,y",
        *extra,
      ], capture_output=True, text=True, check=True)
      if any("verbosity" in a for a in extra):
        assert proc.stderr.strip() != ""
    extras = []
    if any("addSlope" in a for a in extra):
      extras.append("sinefitSlope")
    if any("addExponential" in a for a in extra):
      extras.append("sinefitRate")
    vals = query(out, extras)
    for key, val in expected_vals.items():
      assert math.isclose(vals[key], val, rel_tol=1e-3, abs_tol=1e-2)
    if check_fulloutput:
      cols = subprocess.run([
        str(SDDSQUERY),
        str(out),
        "-columnlist",
      ], capture_output=True, text=True, check=True).stdout
      assert "y" in cols and "yResidual" in cols
      res = subprocess.run([
        str(SDDSPRINTOUT),
        str(out),
        "-column=yResidual",
        "-noTitle",
      ], capture_output=True, text=True, check=True)
      lines = res.stdout.strip().splitlines()
      if len(lines) >= 2:
        lines = lines[2:]
      rvals = [float(v) for v in lines]
      rms = math.sqrt(sum(v*v for v in rvals)/len(rvals))
      assert math.isclose(rms, vals["sinefitRmsResidual"], rel_tol=1e-3, abs_tol=1e-2)
      if "sinefitRmsResidual" in expected_vals:
        assert math.isclose(rms, expected_vals["sinefitRmsResidual"], rel_tol=1e-3, abs_tol=1e-2)

  run_and_check(["-guess=constant=1"], "base")
  run_and_check(["-tolerance=1e-8","-guess=constant=1"], "tolerance")
  run_and_check(["-verbosity=1","-guess=constant=1"], "verbosity")
  run_and_check(["-lockFrequency","-fulloutput","-guess=constant=1"], "lockFrequency", expected_vals={"sinefitFrequency": freq0}, check_fulloutput=True)
  run_and_check(["-limits=evaluations=100,passes=10","-guess=constant=1"], "limits")
  run_and_check(["-majorOrder=row","-guess=constant=1"], "majorOrder")
  run_and_check(["-fulloutput","-guess=constant=1"], "fulloutput", check_fulloutput=True)
  run_and_check(["-guess=constant=1"], "pipe", use_pipe=True)
  run_and_check(["-addSlope","-guess=constant=1,factor=2,frequency=0.5,phase=0.1,slope=0.3","-fulloutput"], "addSlope", input_file=slope, expected_vals={"sinefitSlope": 0.3}, check_fulloutput=True)
  run_and_check(["-addExponential=grow","-guess=constant=1,factor=2,frequency=0.5,phase=0.1,rate=0.1","-fulloutput"], "addExponentialGrow", input_file=grow, expected_vals={"sinefitRate": 0.1}, check_fulloutput=True)
  run_and_check(["-addExponential=decay","-guess=constant=1,factor=2,frequency=0.5,phase=0.1,rate=-0.1","-fulloutput"], "addExponentialDecay", input_file=decay, expected_vals={"sinefitRate": -0.1}, check_fulloutput=True)
