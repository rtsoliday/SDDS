import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSDERIV = BIN_DIR / "sddsderiv"
SDDSPROCESS = BIN_DIR / "sddsprocess"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

EXAMPLE = Path("SDDSlib/demo/example.sdds")


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


@pytest.mark.skipif(not (SDDSDERIV.exists() and SDDSPRINTOUT.exists()), reason="tools not built")
def test_basic_differentiation(tmp_path):
  out = tmp_path / "basic.sdds"
  subprocess.run(
    [
      str(SDDSDERIV),
      str(EXAMPLE),
      str(out),
      "-differentiate=doubleCol",
      "-versus=shortCol",
    ],
    check=True,
  )
  data = run_printout(out, ["shortCol", "doubleColDeriv"])
  for _, deriv in data:
    assert deriv == pytest.approx(10.01, rel=1e-6)


@pytest.mark.skipif(not (SDDSDERIV.exists() and SDDSPRINTOUT.exists()), reason="tools not built")
def test_interval_option(tmp_path):
  out = tmp_path / "interval.sdds"
  subprocess.run(
    [
      str(SDDSDERIV),
      str(EXAMPLE),
      str(out),
      "-differentiate=doubleCol",
      "-versus=shortCol",
      "-interval=1",
    ],
    check=True,
  )
  data = run_printout(out, ["shortCol", "doubleColDeriv"])
  for _, deriv in data:
    assert deriv == pytest.approx(10.01, rel=1e-6)


@pytest.mark.skipif(not (SDDSDERIV.exists() and SDDSPRINTOUT.exists()), reason="tools not built")
def test_savitzky_golay(tmp_path):
  out = tmp_path / "sg.sdds"
  subprocess.run(
    [
      str(SDDSDERIV),
      str(EXAMPLE),
      str(out),
      "-differentiate=doubleCol",
      "-versus=shortCol",
      "-SavitzkyGolay=1,1,2",
    ],
    check=True,
  )
  data = [d for _, d in run_printout(out, ["shortCol", "doubleColDeriv"])]
  expected = [5.005, 10.01, 10.01, 10.01, 5.005, 5.005, 10.01, 5.005]
  assert data == pytest.approx(expected)


@pytest.mark.skipif(not SDDSDERIV.exists(), reason="sddsderiv not built")
def test_main_templates(tmp_path):
  out = tmp_path / "template.sdds"
  subprocess.run(
    [
      str(SDDSDERIV),
      str(EXAMPLE),
      str(out),
      "-differentiate=doubleCol",
      "-versus=shortCol",
      "-mainTemplates=name=%yName_D",
    ],
    check=True,
  )
  header = out.read_bytes()
  assert b"doubleCol_D" in header


@pytest.mark.skipif(not (SDDSDERIV.exists() and SDDSPROCESS.exists()), reason="tools not built")
def test_error_templates(tmp_path):
  temp = tmp_path / "example_sigma.sdds"
  subprocess.run(
    [
      str(SDDSPROCESS),
      str(EXAMPLE),
      str(temp),
      "-define=column,doubleSigma,0.1,type=double",
    ],
    check=True,
  )
  out = tmp_path / "error.sdds"
  subprocess.run(
    [
      str(SDDSDERIV),
      str(temp),
      str(out),
      "-differentiate=doubleCol,doubleSigma",
      "-versus=shortCol",
      "-errorTemplates=name=%yName_Dsig",
    ],
    check=True,
  )
  header = out.read_bytes()
  assert b"doubleCol_Dsig" in header


@pytest.mark.skipif(not SDDSDERIV.exists(), reason="sddsderiv not built")
def test_major_order(tmp_path):
  out = tmp_path / "column_major.sdds"
  subprocess.run(
    [
      str(SDDSDERIV),
      str(EXAMPLE),
      str(out),
      "-differentiate=doubleCol",
      "-versus=shortCol",
      "-majorOrder=column",
    ],
    check=True,
  )
  header = out.read_bytes()
  assert b"column_major_order=1" in header


@pytest.mark.skipif(not SDDSDERIV.exists(), reason="sddsderiv not built")
def test_pipe_option():
  result = subprocess.run(
    [
      str(SDDSDERIV),
      str(EXAMPLE),
      "-differentiate=doubleCol",
      "-versus=shortCol",
      "-pipe=out",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  assert result.stdout.startswith(b"SDDS5")


@pytest.mark.skipif(not SDDSDERIV.exists(), reason="sddsderiv not built")
def test_exclude_option(tmp_path):
  out = tmp_path / "exclude.sdds"
  subprocess.run(
    [
      str(SDDSDERIV),
      str(EXAMPLE),
      str(out),
      "-differentiate=*",
      "-exclude=shortCol",
      "-exclude=floatCol",
      "-exclude=stringCol",
      "-exclude=charCol",
      "-versus=shortCol",
    ],
    check=True,
  )
  header = out.read_bytes()
  assert b"floatColDeriv" not in header
