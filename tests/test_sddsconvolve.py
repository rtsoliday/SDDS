import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCONVOLVE = BIN_DIR / "sddsconvolve"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDSCONVERT = BIN_DIR / "sddsconvert"

REQUIRED = [SDDSCONVOLVE, PLAINDATA2SDDS, SDDSPRINTOUT, SDDSCONVERT]

def create_sdds(tmp_path, name, x_vals, y_vals, y_name):
  plain = tmp_path / f"{name}.txt"
  plain.write_text("\n".join(f"{x} {y}" for x, y in zip(x_vals, y_vals)) + "\n")
  sdds = tmp_path / f"{name}.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      "-column=t,double",
      f"-column={y_name},double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return sdds

def read_column(file, column):
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(file),
      "-pipe=out",
      f"-columns={column}",
      "-noTitle",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  lines = result.stdout.strip().splitlines()
  return [float(l.strip()) for l in lines[2:]]

def run_convolve(signal, response, output, out_name, extra=None):
  cmd = [
    str(SDDSCONVOLVE),
    str(signal),
    str(response),
    str(output),
    "-signalColumns=t,signal",
    "-responseColumns=t,response",
    f"-outputColumns=t,{out_name}",
  ]
  if extra:
    cmd.extend(extra)
  subprocess.run(cmd, check=True)

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_convolution_and_reuse(tmp_path):
  sig = create_sdds(tmp_path, "sig", [0, 1, 2, 3], [1, 2, 3, 4], "signal")
  res = create_sdds(tmp_path, "res", [0, 1, 2, 3], [1, 1, 1, 1], "response")
  out = tmp_path / "out.sdds"
  run_convolve(sig, res, out, "conv")
  assert read_column(out, "conv") == [1.0, 3.0, 6.0, 10.0]
  out_reuse = tmp_path / "out_reuse.sdds"
  run_convolve(sig, res, out_reuse, "conv", extra=["-reuse"])
  assert read_column(out_reuse, "conv") == [1.0, 3.0, 6.0, 10.0]

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_pipe(tmp_path):
  sig = create_sdds(tmp_path, "sig", [0, 1, 2, 3], [1, 2, 3, 4], "signal")
  res = create_sdds(tmp_path, "res", [0, 1, 2, 3], [1, 1, 1, 1], "response")
  proc = subprocess.Popen(
    [
      str(SDDSCONVOLVE),
      str(sig),
      str(res),
      "-signalColumns=t,signal",
      "-responseColumns=t,response",
      "-outputColumns=t,conv",
      "-pipe=out",
    ],
    stdout=subprocess.PIPE,
  )
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      "-pipe=input,output",
      "-columns=conv",
      "-noTitle",
    ],
    stdin=proc.stdout,
    capture_output=True,
    text=True,
    check=True,
  )
  lines = result.stdout.strip().splitlines()
  values = [float(l.strip()) for l in lines[2:]]
  assert values == [1.0, 3.0, 6.0, 10.0]

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_major_order(tmp_path):
  sig = create_sdds(tmp_path, "sig", [0, 1, 2, 3], [1, 2, 3, 4], "signal")
  res = create_sdds(tmp_path, "res", [0, 1, 2, 3], [1, 1, 1, 1], "response")
  out_row = tmp_path / "out_row.sdds"
  run_convolve(sig, res, out_row, "conv", extra=["-majorOrder=row"])
  strings_row = subprocess.run(["strings", str(out_row)], capture_output=True, text=True, check=True).stdout
  assert "column_major_order=1" not in strings_row
  out_col = tmp_path / "out_col.sdds"
  run_convolve(sig, res, out_col, "conv", extra=["-majorOrder=column"])
  strings_col = subprocess.run(["strings", str(out_col)], capture_output=True, text=True, check=True).stdout
  assert "column_major_order=1" in strings_col

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_deconvolve_noise_fraction(tmp_path):
  sig = create_sdds(tmp_path, "sig", [0, 1, 2, 3], [1, 2, 3, 4], "signal")
  res = create_sdds(tmp_path, "res", [0, 1, 2, 3], [1, 0, 0, 0], "response")
  out = tmp_path / "out_deconv.sdds"
  run_convolve(sig, res, out, "deconv", extra=["-deconvolve", "-noiseFraction=1e-6"])
  assert read_column(out, "deconv") == [1.0, 2.0, 3.0, 4.0]

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_deconvolve_wiener_filter(tmp_path):
  sig = create_sdds(tmp_path, "sig", [0, 1, 2, 3], [1, 2, 3, 4], "signal")
  res = create_sdds(tmp_path, "res", [0, 1, 2, 3], [1, 0, 0, 0], "response")
  out = tmp_path / "out_wiener.sdds"
  run_convolve(sig, res, out, "deconv", extra=["-deconvolve", "-wienerFilter=0.5"])
  assert read_column(out, "deconv") == [0.5, 1.0, 1.5, 2.0]

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_correlate(tmp_path):
  sig = create_sdds(tmp_path, "sig", [0, 1, 2, 3], [1, 2, 3, 4], "signal")
  res = create_sdds(tmp_path, "res", [0, 1, 2, 3], [1, 0, 0, 0], "response")
  out = tmp_path / "out_corr.sdds"
  run_convolve(sig, res, out, "corr", extra=["-correlate"])
  assert read_column(out, "corr") == [1.0, 2.0, 3.0, 4.0]
