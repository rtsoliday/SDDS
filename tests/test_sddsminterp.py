import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSMINTERP = BIN_DIR / "sddsminterp"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSCHECK = BIN_DIR / "sddscheck"

@pytest.mark.skipif(
  not (SDDSMINTERP.exists() and SDDS2STREAM.exists() and SDDSCHECK.exists()),
  reason="required tools not built",
)
def test_sddsminterp_all_options(tmp_path):
  model = tmp_path / "model.sdds"
  model.write_text(
    "SDDS1\n"
    "&column name=x, type=double &end\n"
    "&column name=y, type=double &end\n"
    "&data mode=ascii &end\n"
    "! page 1\n"
    "5\n"
    "0 1\n"
    "1 2\n"
    "2 3\n"
    "3 4\n"
    "4 5\n"
  )
  data = tmp_path / "data.sdds"
  data.write_text(
    "SDDS1\n"
    "&column name=x, type=double &end\n"
    "&column name=y, type=double &end\n"
    "&data mode=ascii &end\n"
    "! page 1\n"
    "3\n"
    "0 1\n"
    "2 4.5\n"
    "4 10\n"
  )
  values = tmp_path / "values.sdds"
  values.write_text(
    "SDDS1\n"
    "&column name=x, type=double &end\n"
    "&data mode=ascii &end\n"
    "! page 1\n"
    "5\n"
    "0\n"
    "1\n"
    "2\n"
    "3\n"
    "4\n"
  )
  out = tmp_path / "out.sdds"
  result = subprocess.run(
    [
      str(SDDSMINTERP),
      str(data),
      str(out),
      "-columns=x,y",
      "-order=2",
      f"-model={model},abscissa=x,ordinate=y,interp=1",
      f"-fileValues={values},abscissa=x",
      "-majorOrder=column",
      "-verbose",
      "-ascii",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  assert "Warning: Option -fileValues is not operational yet." in result.stderr
  check = subprocess.run(
    [str(SDDSCHECK), str(out)], capture_output=True, text=True, check=True
  )
  assert check.stdout.strip() == "ok"
  stream = subprocess.run(
    [str(SDDS2STREAM), str(out), "-columns=x,y"],
    capture_output=True,
    text=True,
    check=True,
  )
  xs, ys = [], []
  for line in stream.stdout.strip().splitlines():
    if not line:
      continue
    parts = line.split()
    xs.append(float(parts[0]))
    ys.append(float(parts[1]))
  assert xs == pytest.approx([0, 1, 2, 3, 4])
  assert ys == pytest.approx([1, 2.5, 4.5, 7.0, 10.0])


@pytest.mark.skipif(
  not (SDDSMINTERP.exists() and SDDS2STREAM.exists() and SDDSCHECK.exists()),
  reason="required tools not built",
)
def test_sddsminterp_pipe(tmp_path):
  model = tmp_path / "model.sdds"
  model.write_text(
    "SDDS1\n"
    "&column name=x, type=double &end\n"
    "&column name=y, type=double &end\n"
    "&data mode=ascii &end\n"
    "! page 1\n"
    "5\n"
    "0 1\n"
    "1 2\n"
    "2 3\n"
    "3 4\n"
    "4 5\n"
  )
  data = tmp_path / "data.sdds"
  data.write_text(
    "SDDS1\n"
    "&column name=x, type=double &end\n"
    "&column name=y, type=double &end\n"
    "&data mode=ascii &end\n"
    "! page 1\n"
    "3\n"
    "0 1\n"
    "2 4.5\n"
    "4 10\n"
  )
  with open(data, "rb") as infile:
    proc = subprocess.run(
      [
        str(SDDSMINTERP),
        "-pipe=in,out",
        "-columns=x,y",
        f"-model={model},abscissa=x,ordinate=y,interp=1",
        "-order=2",
      ],
      input=infile.read(),
      stdout=subprocess.PIPE,
      check=True,
    )
  out = tmp_path / "out.sdds"
  out.write_bytes(proc.stdout)
  check = subprocess.run(
    [str(SDDSCHECK), str(out)], capture_output=True, text=True, check=True
  )
  assert check.stdout.strip() == "ok"
  stream = subprocess.run(
    [str(SDDS2STREAM), str(out), "-columns=x,y"],
    capture_output=True,
    text=True,
    check=True,
  )
  xs, ys = [], []
  for line in stream.stdout.strip().splitlines():
    if not line:
      continue
    p = line.split()
    xs.append(float(p[0]))
    ys.append(float(p[1]))
  assert xs == pytest.approx([0, 1, 2, 3, 4])
  assert ys == pytest.approx([1, 2.5, 4.5, 7.0, 10.0])
