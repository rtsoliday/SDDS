import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSHIFT = BIN_DIR / "sddsshift"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDS2STREAM = BIN_DIR / "sdds2stream"

REQUIRED = [SDDSSHIFT, PLAINDATA2SDDS, SDDSPRINTOUT, SDDS2STREAM]

def shift_py(values, delay, zero=False, circular=False):
  n = len(values)
  out = [0] * n
  if circular:
    for i in range(n):
      j = (i - delay) % n
      out[i] = values[j]
  else:
    if delay < 0:
      delay = -delay
      for i in range(n - delay):
        out[i] = values[i + delay]
      fill = 0 if zero else values[-1]
      for i in range(n - delay, n):
        out[i] = fill
    else:
      for i in range(n - 1, delay - 1, -1):
        out[i] = values[i - delay]
      fill = 0 if zero else values[0]
      for i in range(delay):
        out[i] = fill
  return out

def create_single_column_sdds(tmp_path, values, name="A"):
  plain = tmp_path / "input.txt"
  plain.write_text("\n".join(str(v) for v in values) + "\n")
  sdds = tmp_path / "data.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      f"-column={name},double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return sdds

def create_two_column_sdds(tmp_path, a, b, name1="A", name2="B"):
  plain = tmp_path / "input2.txt"
  plain.write_text("\n".join(f"{x} {y}" for x, y in zip(a, b)) + "\n")
  sdds = tmp_path / "data2.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(sdds),
      f"-column={name1},double",
      f"-column={name2},double",
      "-inputMode=ascii",
      "-outputMode=ascii",
      "-noRowCount",
    ],
    check=True,
  )
  return sdds

def read_column(file, column):
  cmd = [str(SDDSPRINTOUT), str(file), "-pipe=out", f"-columns={column}", "-noTitle"]
  result = subprocess.run(cmd, capture_output=True, text=True, check=True)
  lines = result.stdout.strip().splitlines()[2:]
  return [float(x) for x in lines]

def read_parameter(file, param):
  result = subprocess.run(
    [str(SDDS2STREAM), str(file), f"-parameters={param}"],
    capture_output=True,
    text=True,
    check=True,
  )
  return float(result.stdout.strip())

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_shift(tmp_path):
  values = [1, 2, 3, 4, 5]
  sdds = create_single_column_sdds(tmp_path, values)
  out = tmp_path / "shift.sdds"
  subprocess.run(
    [str(SDDSSHIFT), str(sdds), str(out), "-columns=A", "-shift=2"],
    check=True,
  )
  result = read_column(out, "ShiftedA")
  assert result == shift_py(values, 2)

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_zero(tmp_path):
  values = [1, 2, 3, 4, 5]
  sdds = create_single_column_sdds(tmp_path, values)
  out = tmp_path / "zero.sdds"
  subprocess.run(
    [str(SDDSSHIFT), str(sdds), str(out), "-columns=A", "-shift=2", "-zero"],
    check=True,
  )
  result = read_column(out, "ShiftedA")
  assert result == shift_py(values, 2, zero=True)

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_circular(tmp_path):
  values = [1, 2, 3, 4, 5]
  sdds = create_single_column_sdds(tmp_path, values)
  out = tmp_path / "circ.sdds"
  subprocess.run(
    [str(SDDSSHIFT), str(sdds), str(out), "-columns=A", "-shift=1", "-circular"],
    check=True,
  )
  result = read_column(out, "ShiftedA")
  assert result == shift_py(values, 1, circular=True)

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_match(tmp_path):
  a = [1, 2, 3, 4, 5]
  b = [0, 1, 2, 3, 4]
  sdds = create_two_column_sdds(tmp_path, a, b)
  out = tmp_path / "match.sdds"
  subprocess.run(
    [str(SDDSSHIFT), str(sdds), str(out), "-columns=A", "-match=B", "-zero"],
    check=True,
  )
  result = read_column(out, "ShiftedA")
  assert result == shift_py(a, 1, zero=True)
  shift_value = read_parameter(out, "AShift")
  assert int(shift_value) == 1

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
@pytest.mark.parametrize("order", ["row", "column"])
def test_major_order(tmp_path, order):
  values = [1, 2, 3, 4, 5]
  sdds = create_single_column_sdds(tmp_path, values)
  out = tmp_path / f"order_{order}.sdds"
  subprocess.run(
    [
      str(SDDSSHIFT),
      str(sdds),
      str(out),
      "-columns=A",
      "-shift=1",
      f"-majorOrder={order}",
    ],
    check=True,
  )
  result = read_column(out, "ShiftedA")
  assert result == shift_py(values, 1)

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_pipe(tmp_path):
  values = [1, 2, 3, 4, 5]
  sdds = create_single_column_sdds(tmp_path, values)
  out = tmp_path / "pipe.sdds"
  with open(out, "wb") as fp:
    subprocess.run(
      [str(SDDSSHIFT), str(sdds), "-pipe=out", "-columns=A", "-shift=1"],
      stdout=fp,
      check=True,
    )
  result = read_column(out, "ShiftedA")
  assert result == shift_py(values, 1)
