import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSTDRPEELING = BIN_DIR / "sddstdrpeeling"
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

missing = [p for p in (SDDSTDRPEELING, SDDSMAKEDATASET, SDDSPRINTOUT) if not p.exists()]
pytestmark = pytest.mark.skipif(missing, reason="required binaries not built")

MEAS_DATA = [0.1, 0.05, -0.02, 0.03, 0.04]
DATA_STR = ",".join(str(x) for x in MEAS_DATA)

def compute_peeling(data, input_voltage=0.2, z0=50):
  rows = len(data)
  meas = [d / input_voltage for d in data]
  left = [0] * (rows + 1)
  right = [0] * (rows + 1)
  gamma = [0] * (rows + 1)
  zline = [0] * (rows + 1)
  g_product = 1.0
  if rows >= 1:
    gamma[1] = meas[0]
    left[1] = 1
    g_product *= 1 - gamma[1] * gamma[1]
  if rows >= 2:
    vr_temp = (1 - gamma[1]) * right[1] + gamma[1] * left[1]
    gamma[2] = (meas[1] - vr_temp) / g_product
    left[2] = left[1] * (1 + gamma[1])
    right[1] = left[2] * gamma[2]
    g_product *= 1 - gamma[2] * gamma[2]
  for i in range(3, rows + 1):
    left[i] = left[i - 1] * (1 + gamma[i - 1])
    left[i - 1] = left[i - 2] * (1 + gamma[i - 2]) - right[i - 2] * gamma[i - 2]
    for k in range(i - 2, 2, -1):
      right[k] = gamma[k + 1] * left[k + 1] + (1 - gamma[k + 1] * right[k + 1])
      left[k] = (1 + gamma[k - 1]) * left[k - 1] - gamma[k - 1] * right[k - 1]
    right[1] = left[2] * gamma[2] + right[2] * (1 - gamma[2])
    vr_temp = (1 - gamma[1]) * right[1] + gamma[1] * left[1]
    gamma[i] = (meas[i - 1] - vr_temp) / g_product
    g_product *= 1 - gamma[i] * gamma[i]
    d_increase = left[i] * gamma[i]
    right[i - 1] += d_increase
    for k in range(i - 2, 1, -1):
      d_increase *= 1 - gamma[k + 1]
      right[k] += d_increase
  zline[0] = z0
  for i in range(1, rows + 1):
    zline[i] = (1 + gamma[i]) / (1 - gamma[i]) * zline[i - 1]
  return zline[1:]

def create_dataset(path):
  subprocess.run(
    [
      str(SDDSMAKEDATASET),
      str(path),
      "-column=meas,type=double",
      f"-data={DATA_STR}",
      "-parameter=V,type=double",
      "-data=1",
    ],
    check=True,
  )

def read_peeling(path):
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(path),
      "-columns=PeeledImpedance",
      "-noTitle",
      "-noLabels",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  return [float(x) for x in result.stdout.strip().split()]

def test_basic_peeling(tmp_path):
  input_file = tmp_path / "input.sdds"
  output_file = tmp_path / "out.sdds"
  create_dataset(input_file)
  subprocess.run(
    [
      str(SDDSTDRPEELING),
      str(input_file),
      str(output_file),
      "-column=meas",
    ],
    check=True,
  )
  result = read_peeling(output_file)
  assert result == pytest.approx(compute_peeling(MEAS_DATA))

def test_input_voltage_option(tmp_path):
  input_file = tmp_path / "input.sdds"
  output_file = tmp_path / "out.sdds"
  create_dataset(input_file)
  subprocess.run(
    [
      str(SDDSTDRPEELING),
      str(input_file),
      str(output_file),
      "-column=meas",
      "-inputVoltage=1",
    ],
    check=True,
  )
  result = read_peeling(output_file)
  expected = compute_peeling(MEAS_DATA, input_voltage=1)
  assert result == pytest.approx(expected)

def test_input_voltage_parameter(tmp_path):
  input_file = tmp_path / "input.sdds"
  output_file = tmp_path / "out.sdds"
  create_dataset(input_file)
  subprocess.run(
    [
      str(SDDSTDRPEELING),
      str(input_file),
      str(output_file),
      "-column=meas",
      "-inputVoltage=@V",
    ],
    check=True,
  )
  result = read_peeling(output_file)
  expected = compute_peeling(MEAS_DATA, input_voltage=1)
  assert result == pytest.approx(expected)

def test_z0_option(tmp_path):
  input_file = tmp_path / "input.sdds"
  output_file = tmp_path / "out.sdds"
  create_dataset(input_file)
  subprocess.run(
    [
      str(SDDSTDRPEELING),
      str(input_file),
      str(output_file),
      "-column=meas",
      "-z0=75",
    ],
    check=True,
  )
  result = read_peeling(output_file)
  expected = compute_peeling(MEAS_DATA, z0=75)
  assert result == pytest.approx(expected)

def test_major_order(tmp_path):
  input_file = tmp_path / "input.sdds"
  output_file = tmp_path / "out.sdds"
  create_dataset(input_file)
  subprocess.run(
    [
      str(SDDSTDRPEELING),
      str(input_file),
      str(output_file),
      "-column=meas",
      "-majorOrder=column",
    ],
    check=True,
  )
  result = read_peeling(output_file)
  assert result == pytest.approx(compute_peeling(MEAS_DATA))
  header = output_file.read_bytes()
  assert b"column_major_order=1" in header

def test_pipe_option(tmp_path):
  input_file = tmp_path / "input.sdds"
  create_dataset(input_file)
  data = input_file.read_bytes()
  proc = subprocess.run(
    [str(SDDSTDRPEELING), "-column=meas", "-pipe"],
    input=data,
    stdout=subprocess.PIPE,
    check=True,
  )
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      "-pipe",
      "-columns=PeeledImpedance",
      "-noTitle",
      "-noLabels",
    ],
    input=proc.stdout,
    capture_output=True,
    check=True,
  )
  values = [float(x) for x in result.stdout.decode().strip().split()]
  assert values == pytest.approx(compute_peeling(MEAS_DATA))
