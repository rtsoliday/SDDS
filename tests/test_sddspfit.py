import subprocess
from pathlib import Path
import pytest
import math

BIN_DIR = Path("bin/Linux-x86_64")
SDDSPFIT = BIN_DIR / "sddspfit"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDSQUERY = BIN_DIR / "sddsquery"

required_bins = [SDDSPFIT, PLAINDATA2SDDS, SDDSPRINTOUT, SDDSQUERY]

@pytest.mark.skipif(not all(p.exists() for p in required_bins), reason="required tools not built")
def test_sddspfit_flags_and_parameters(tmp_path):
  plain = tmp_path / "data.txt"
  lines = []
  for x in range(1, 6):
    y = 1 + 2 * x + 3 * x * x
    lines.append(f"{x} {y} 0.1 0.1\n")
  plain.write_text("".join(lines))
  input_sdds = tmp_path / "input.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(plain),
    str(input_sdds),
    "-column=x,double",
    "-column=y,double",
    "-column=sx,double",
    "-column=sy,double",
    "-inputMode=ascii",
    "-outputMode=ascii",
    "-noRowCount",
  ], check=True)

  values_txt = tmp_path / "values.txt"
  values_txt.write_text("\n".join(str(x) for x in range(1, 6)) + "\n")
  values_sdds = tmp_path / "values.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(values_txt),
    str(values_sdds),
    "-column=x,double",
    "-inputMode=ascii",
    "-outputMode=ascii",
    "-noRowCount",
  ], check=True)

  plain_even = tmp_path / "even.txt"
  lines_even = []
  for x in range(-2, 3):
    y = 1 + 3 * x * x
    lines_even.append(f"{x} {y} 0.1 0.1\n")
  plain_even.write_text("".join(lines_even))
  input_even = tmp_path / "even.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(plain_even),
    str(input_even),
    "-column=x,double",
    "-column=y,double",
    "-column=sx,double",
    "-column=sy,double",
    "-inputMode=ascii",
    "-outputMode=ascii",
    "-noRowCount",
  ], check=True)

  def run_cmd(extra, label, use_pipe=False, input_path=input_sdds, include_y=True):
    columns = "-columns=x,y,xSigma=sx"
    if include_y:
      columns += ",ySigma=sy"
    if use_pipe:
      result = subprocess.run(
        [str(SDDSPFIT), "-pipe", columns, "-terms=3", *extra],
        input=input_path.read_bytes(),
        capture_output=True,
        check=True,
      )
      out = tmp_path / f"{label}.sdds"
      out.write_bytes(result.stdout)
    else:
      out = tmp_path / f"{label}.sdds"
      subprocess.run([
        str(SDDSPFIT),
        str(input_path),
        str(out),
        columns,
        "-terms=3",
        *extra,
      ], check=True)
    return out

  def get_params(path, names=None):
    if names is None:
      names = [
        "Intercept",
        "Slope",
        "Curvature",
        "xOffset",
        "xScale",
        "Terms",
        "FitIsValid",
      ]
    result = subprocess.run(
      [str(SDDSPRINTOUT), str(path)] + [f"-parameter={n}" for n in names] + ["-noTitle"],
      capture_output=True,
      text=True,
      check=True,
    )
    tokens = result.stdout.strip().split()
    values = {}
    for i in range(0, len(tokens), 3):
      values[tokens[i]] = tokens[i + 2]
    return values

  def check(out, intercept, slope, curvature, offset, scale):
    vals = get_params(out)
    assert vals["FitIsValid"] == "y"
    assert int(float(vals["Terms"])) == 3
    assert math.isclose(float(vals["Intercept"]), intercept, rel_tol=1e-10, abs_tol=1e-10)
    assert math.isclose(float(vals["Slope"]), slope, rel_tol=1e-10, abs_tol=1e-10)
    assert math.isclose(float(vals["Curvature"]), curvature, rel_tol=1e-10, abs_tol=1e-10)
    assert math.isclose(float(vals["xOffset"]), offset, rel_tol=1e-10, abs_tol=1e-10)
    assert math.isclose(float(vals["xScale"]), scale, rel_tol=1e-10, abs_tol=1e-10)

  check(run_cmd([], "base"), 1, 2, 3, 0, 1)
  check(run_cmd(["-orders=0,1,2"], "orders"), 1, 2, 3, 0, 1)
  out = run_cmd(["-chebyshev=convert"], "chebyshev")
  vals = get_params(out, ["xOffset", "xScale", "Terms", "FitIsValid"])
  assert vals["FitIsValid"] == "y"
  assert int(float(vals["Terms"])) == 3
  assert math.isclose(float(vals["xOffset"]), 3, rel_tol=1e-10, abs_tol=1e-10)
  assert math.isclose(float(vals["xScale"]), 2, rel_tol=1e-10, abs_tol=1e-10)
  check(run_cmd(["-normalize=2"], "normalize"), 1, 2, 3, 0, 1)
  check(run_cmd(["-xOffset=1"], "xoffset"), 6, 8, 3, 1, 1)
  check(run_cmd(["-autoOffset"], "autooffset"), 34, 20, 3, 3, 1)
  check(run_cmd(["-xFactor=2"], "xfactor"), 1, 4, 12, 0, 2)
  check(run_cmd(["-sigmas=0.1,absolute"], "sigmas", include_y=False), 1, 2, 3, 0, 1)
  check(run_cmd(["-generateSigmas"], "gensig"), 1, 2, 3, 0, 1)
  check(run_cmd(["-generateSigmas=keepLargest"], "gensig_l"), 1, 2, 3, 0, 1)
  check(run_cmd(["-generateSigmas=keepSmallest"], "gensig_s"), 1, 2, 3, 0, 1)
  check(run_cmd(["-modifySigmas"], "modsig"), 1, 2, 3, 0, 1)
  check(
    run_cmd(["-reviseOrders=threshold=0.1,complete=5,goodEnough=0.1,verbose"], "revise"),
    1,
    2,
    3,
    0,
    1,
  )
  check(run_cmd(["-sparse=2"], "sparse"), 1, 2, 3, 0, 1)
  check(run_cmd(["-range=1,3"], "range"), 1, 2, 3, 0, 1)

  eval1 = tmp_path / "eval1.sdds"
  check(run_cmd([f"-evaluate={eval1},begin=1,end=5,number=5"], "eval_range"), 1, 2, 3, 0, 1)
  cols = subprocess.run(
    [str(SDDSQUERY), str(eval1), "-columnlist"],
    capture_output=True,
    text=True,
    check=True,
  ).stdout
  assert "x" in cols and "y" in cols

  eval2 = tmp_path / "eval2.sdds"
  check(
    run_cmd(
      [f"-evaluate={eval2},valuesFile={values_sdds},valuesColumn=x"],
      "eval_values",
    ),
    1,
    2,
    3,
    0,
    1,
  )
  cols = subprocess.run(
    [str(SDDSQUERY), str(eval2), "-columnlist"],
    capture_output=True,
    text=True,
    check=True,
  ).stdout
  assert "x" in cols and "y" in cols

  check(run_cmd([], "pipe", use_pipe=True), 1, 2, 3, 0, 1)
  check(run_cmd(["-majorOrder=row"], "major"), 1, 2, 3, 0, 1)

  out = run_cmd(["-fitLabelFormat=%.2f"], "label")
  result = subprocess.run(
    [str(SDDSPRINTOUT), str(out), "-parameter=sddspfitLabel", "-noTitle"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert result.stdout.strip().split("=", 1)[1].strip() != ""

  out = run_cmd(["-symmetry=even"], "symmetry", input_path=input_even)
  vals = get_params(out, ["Intercept", "Curvature", "xOffset", "xScale", "Terms", "FitIsValid"])
  assert vals["FitIsValid"] == "y"
  assert int(float(vals["Terms"])) == 3
  assert math.isclose(float(vals["Intercept"]), 1, rel_tol=1e-10, abs_tol=1e-10)
  assert math.isclose(float(vals["Curvature"]), 3, rel_tol=1e-10, abs_tol=1e-10)
  assert math.isclose(float(vals["xOffset"]), 0, rel_tol=1e-10, abs_tol=1e-10)
  assert math.isclose(float(vals["xScale"]), 1, rel_tol=1e-10, abs_tol=1e-10)
