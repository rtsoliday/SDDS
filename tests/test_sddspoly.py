import subprocess
from pathlib import Path
import re
import pytest

ROOT = Path(__file__).resolve().parent.parent
BIN_DIR = ROOT / "bin/Linux-x86_64"
SDDSPOLY = BIN_DIR / "sddspoly"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"
SDDSCHECK = BIN_DIR / "sddscheck"
SOURCE = ROOT / "SDDSaps/sddspoly.c"


def extract_options():
  text = SOURCE.read_text()
  match = re.search(r"char \*option\[[^\]]+\] = \{([^}]+)\};", text, re.DOTALL)
  opts = []
  if match:
    for line in match.group(1).splitlines():
      line = line.strip().strip(",")
      if line:
        opts.append(line.strip('"'))
  return opts


# Option names expected from the source file
OPTIONS = ["pipe", "evaluate"]


def create_input(tmp_path):
  content = (
    "SDDS1\n"
    "&column name=x, type=double &end\n"
    "&data mode=ascii, no_row_counts=1 &end\n"
    "0\n1\n2\n3\n"
  )
  file = tmp_path / "input.sdds"
  file.write_text(content)
  return file


def create_poly(tmp_path):
  content = (
    "SDDS1\n"
    "&column name=coef, type=double &end\n"
    "&column name=power, type=long &end\n"
    "&data mode=ascii, no_row_counts=1 &end\n"
    "1 0\n2 1\n3 2\n"
  )
  file = tmp_path / "poly.sdds"
  file.write_text(content)
  return file


@pytest.mark.skipif(
  not (SDDSPOLY.exists() and SDDS2PLAINDATA.exists() and SDDSCHECK.exists()),
  reason="required tools not built",
)
@pytest.mark.parametrize("option", OPTIONS)
def test_sddspoly(tmp_path, option):
  assert set(extract_options()) == set(OPTIONS)
  input_file = create_input(tmp_path)
  poly_file = create_poly(tmp_path)
  evaluate = (
    f"-evaluate=filename={poly_file},output=result,coefficients=coef,"
    f"input0=x,power0=power"
  )
  output = tmp_path / "out.sdds"
  if option == "pipe":
    cmd = [str(SDDSPOLY), str(input_file), "-pipe=output", evaluate]
    with output.open("wb") as f:
      subprocess.run(cmd, stdout=f, check=True)
  else:
    cmd = [str(SDDSPOLY), str(input_file), str(output), evaluate]
    subprocess.run(cmd, check=True)

  subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, check=True)
  result = subprocess.run(
    [str(SDDS2PLAINDATA), str(output), "-pipe=out", "-column=result"],
    capture_output=True,
    text=True,
    check=True,
  )
  numbers = [
    float(tok)
    for tok in result.stdout.split()
    if re.fullmatch(r"[+-]?\d+(?:\.\d*)?(?:[eE][+-]?\d+)?", tok)
  ]
  if len(numbers) > 4:
    numbers = numbers[-4:]
  assert numbers == pytest.approx([1.0, 6.0, 17.0, 34.0])
