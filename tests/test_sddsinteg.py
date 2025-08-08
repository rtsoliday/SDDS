import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSINTEG = BIN_DIR / "sddsinteg"
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


@pytest.mark.skipif(
  not (SDDSINTEG.exists() and SDDSPRINTOUT.exists()),
  reason="tools not built",
)
def test_trapazoid_integration(tmp_path):
  out = tmp_path / "trap.sdds"
  subprocess.run(
    [
      str(SDDSINTEG),
      str(EXAMPLE),
      str(out),
      "-integrate=doubleCol",
      "-versus=shortCol",
    ],
    check=True,
  )
  data = run_printout(out, ["doubleColInteg"])
  assert data[4][0] == pytest.approx(120.12, rel=1e-6)


@pytest.mark.skipif(
  not (SDDSINTEG.exists() and SDDSPRINTOUT.exists()),
  reason="tools not built",
)
def test_gillmiller_integration(tmp_path):
  out = tmp_path / "gm.sdds"
  subprocess.run(
    [
      str(SDDSINTEG),
      str(EXAMPLE),
      str(out),
      "-integrate=doubleCol",
      "-versus=shortCol",
      "-method=gillmiller",
    ],
    check=True,
  )
  data = run_printout(out, ["doubleColInteg"])
  assert data[4][0] == pytest.approx(120.12, rel=1e-6)


@pytest.mark.skipif(
  not (SDDSINTEG.exists() and SDDSPRINTOUT.exists() and SDDSPROCESS.exists()),
  reason="tools not built",
)
def test_sigma_propagation(tmp_path):
  temp = tmp_path / "with_sigma.sdds"
  subprocess.run(
    [
      str(SDDSPROCESS),
      str(EXAMPLE),
      str(temp),
      "-define=column,doubleSigma,0.1,type=double",
    ],
    check=True,
  )
  out = tmp_path / "sigma.sdds"
  subprocess.run(
    [
      str(SDDSINTEG),
      str(temp),
      str(out),
      "-integrate=doubleCol,doubleSigma",
      "-versus=shortCol",
    ],
    check=True,
  )
  data = run_printout(out, ["doubleColInteg", "doubleColIntegSigma"])
  assert data[4][0] == pytest.approx(120.12, rel=1e-6)
  assert data[4][1] == pytest.approx(0.141421356, rel=1e-6)


@pytest.mark.skipif(not SDDSINTEG.exists(), reason="sddsinteg not built")
def test_print_final(tmp_path):
  out = tmp_path / "out.sdds"
  result = subprocess.run(
    [
      str(SDDSINTEG),
      str(EXAMPLE),
      str(out),
      "-integrate=doubleCol",
      "-versus=shortCol",
      "-printFinal=bare,stdout",
    ],
    stdout=subprocess.PIPE,
    text=True,
    check=True,
  )
  values = [float(x) for x in result.stdout.strip().splitlines()]
  assert values[0] == pytest.approx(120.12, rel=1e-6)
  assert values[1] == pytest.approx(140.14, rel=1e-6)
