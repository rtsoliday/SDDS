import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSZEROFIND = BIN_DIR / "sddszerofind"
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"
SDDS2STREAM = BIN_DIR / "sdds2stream"

missing = [p for p in (SDDSZEROFIND, SDDSMAKEDATASET, SDDS2STREAM) if not p.exists()]
pytestmark = pytest.mark.skipif(missing, reason="required binaries not built")

def make_dataset(path, extra=False):
  args = [
    str(SDDSMAKEDATASET),
    str(path),
    "-column=t,type=double",
    "-data=0,1,2,3,4",
    "-column=signal,type=double",
    "-data=-2,-1,1,2,3",
  ]
  if extra:
    args += ["-column=extra,type=double", "-data=10,20,30,40,50"]
  subprocess.run(args, check=True)

def test_zeroesof_option(tmp_path):
  data = tmp_path / "data.sdds"
  make_dataset(data)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSZEROFIND),
      str(data),
      str(out),
      "-zeroesOf=signal",
      "-columns=t",
    ],
    check=True,
  )
  result = subprocess.run(
    [str(SDDS2STREAM), str(out), "-columns=t,signal"],
    capture_output=True,
    text=True,
    check=True,
  )
  t_val, sig_val = map(float, result.stdout.strip().split())
  assert t_val == pytest.approx(1.5)
  assert sig_val == pytest.approx(0.0)

def test_columns_option(tmp_path):
  data = tmp_path / "data.sdds"
  make_dataset(data, extra=True)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSZEROFIND),
      str(data),
      str(out),
      "-zeroesOf=signal",
      "-columns=t",
    ],
    check=True,
  )
  header = out.read_bytes()
  assert b"extra" not in header
  result = subprocess.run(
    [str(SDDS2STREAM), str(out), "-columns=t,signal"],
    capture_output=True,
    text=True,
    check=True,
  )
  t_val, sig_val = map(float, result.stdout.strip().split())
  assert t_val == pytest.approx(1.5)
  assert sig_val == pytest.approx(0.0)

def test_offset_option(tmp_path):
  data = tmp_path / "data.sdds"
  make_dataset(data)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSZEROFIND),
      str(data),
      str(out),
      "-zeroesOf=signal",
      "-columns=t",
      "-offset=0.5",
    ],
    check=True,
  )
  result = subprocess.run(
    [str(SDDS2STREAM), str(out), "-columns=t,signal"],
    capture_output=True,
    text=True,
    check=True,
  )
  t_val, sig_val = map(float, result.stdout.strip().split())
  assert t_val == pytest.approx(1.25)
  assert sig_val == pytest.approx(-0.5)

def test_slope_output_option(tmp_path):
  data = tmp_path / "data.sdds"
  make_dataset(data)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSZEROFIND),
      str(data),
      str(out),
      "-zeroesOf=signal",
      "-columns=t",
      "-slopeOutput",
    ],
    check=True,
  )
  result = subprocess.run(
    [str(SDDS2STREAM), str(out), "-columns=t,tSlope,signal"],
    capture_output=True,
    text=True,
    check=True,
  )
  t_val, slope_val, sig_val = map(float, result.stdout.strip().split())
  assert t_val == pytest.approx(1.5)
  assert sig_val == pytest.approx(0.0)
  assert slope_val == pytest.approx(2.0)

def test_major_order_option(tmp_path):
  data = tmp_path / "data.sdds"
  make_dataset(data)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSZEROFIND),
      str(data),
      str(out),
      "-zeroesOf=signal",
      "-columns=t",
      "-majorOrder=column",
    ],
    check=True,
  )
  header = out.read_bytes()
  assert b"column_major_order=1" in header
  result = subprocess.run(
    [str(SDDS2STREAM), str(out), "-columns=t,signal"],
    capture_output=True,
    text=True,
    check=True,
  )
  t_val, sig_val = map(float, result.stdout.strip().split())
  assert t_val == pytest.approx(1.5)
  assert sig_val == pytest.approx(0.0)

def test_pipe_option(tmp_path):
  data = tmp_path / "data.sdds"
  make_dataset(data)
  data_bytes = data.read_bytes()
  proc = subprocess.run(
    [
      str(SDDSZEROFIND),
      "-zeroesOf=signal",
      "-columns=t",
      "-pipe",
    ],
    input=data_bytes,
    stdout=subprocess.PIPE,
    check=True,
  )
  result = subprocess.run(
    [str(SDDS2STREAM), "-pipe", "-columns=t,signal"],
    input=proc.stdout,
    capture_output=True,
    check=True,
  )
  t_val, sig_val = map(float, result.stdout.decode().strip().split())
  assert t_val == pytest.approx(1.5)
  assert sig_val == pytest.approx(0.0)
