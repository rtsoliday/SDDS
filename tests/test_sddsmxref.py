import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSMREF = BIN_DIR / "sddsmxref"
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"

REQUIRED = [SDDSMREF, SDDSMAKEDATASET, SDDSPRINTOUT, SDDS2STREAM, SDDSQUERY]

def create_dataset(path, params=None, arrays=None, columns=None):
  args = [str(SDDSMAKEDATASET), str(path)]
  params = params or []
  arrays = arrays or []
  columns = columns or []
  for name, typ, value in params:
    args.append(f"-parameter={name},type={typ}")
    args.append(f"-data={value}")
  for name, typ, values in arrays:
    args.append(f"-array={name},type={typ}")
    args.append("-data=" + ",".join(str(v) for v in values))
  for name, typ, values in columns:
    args.append(f"-column={name},type={typ}")
    args.append("-data=" + ",".join(str(v) for v in values))
  subprocess.run(args, check=True)
  return path

def read_columns(file, columns):
  cmd = [str(SDDSPRINTOUT), str(file), "-pipe=out"]
  for col in columns:
    cmd.append(f"-columns={col}")
  cmd.append("-noTitle")
  result = subprocess.run(cmd, capture_output=True, text=True, check=True)
  lines = result.stdout.strip().splitlines()[2:]
  data = []
  for line in lines:
    parts = line.split()
    data.append([float(p) for p in parts])
  return data

def read_parameter(file, name):
  result = subprocess.run(
    [str(SDDS2STREAM), str(file), f"-parameters={name}"],
    capture_output=True,
    text=True,
    check=True,
  )
  return float(result.stdout.strip())

def list_columns(file):
  result = subprocess.run(
    [str(SDDSQUERY), str(file), "-columnList"],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout.strip().split()

@pytest.fixture
def sample_data(tmp_path):
  in1 = create_dataset(
    tmp_path / "in1.sdds",
    params=[("p1", "double", 1)],
    columns=[
      ("id", "long", [1, 1, 2, 4]),
      ("pattern", "string", ["A*", "A*", "B*", "C*"]),
      ("value1", "double", [10, 15, 20, 40]),
      ("matchval", "double", [100, 100, 200, 400]),
    ],
  )
  in2 = create_dataset(
    tmp_path / "in2.sdds",
    params=[("p2", "double", 7)],
    arrays=[("arr1", "double", [5, 6, 7])],
    columns=[
      ("id", "long", [1, 2, 3]),
      ("name", "string", ["Alice", "Beta", "Ceta"]),
      ("value2", "double", [1000, 2000, 3000]),
      ("matchval", "double", [100, 200, 300]),
      ("value1", "double", [11, 22, 33]),
    ],
  )
  return in1, in2

pytestmark = pytest.mark.skipif(
  not all(p.exists() for p in REQUIRED), reason="required binaries not built"
)


def test_match_take_leave_fillin_reuse(tmp_path, sample_data):
  in1, in2 = sample_data
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSMREF),
      str(in1),
      str(in2),
      str(out),
      "-equate=id",
      "-take=value2",
      "-leave=value1",
      "-fillIn",
      "-reuse=rows",
      "-nowarnings",
    ],
    check=True,
  )
  data = read_columns(out, ["id", "value1", "value2"])
  assert data == [
    [1.0, 10.0, 1000.0],
    [1.0, 15.0, 1000.0],
    [2.0, 20.0, 2000.0],
    [4.0, 40.0, 0.0],
  ]

def test_equate(tmp_path, sample_data):
  in1, in2 = sample_data
  out = tmp_path / "equate.sdds"
  subprocess.run(
    [
      str(SDDSMREF),
      str(in1),
      str(in2),
      str(out),
      "-equate=matchval",
      "-take=value2",
      "-fillIn",
      "-reuse=rows",
    ],
    check=True,
  )
  data = read_columns(out, ["id", "value2"])
  assert data == [
    [1.0, 1000.0],
    [1.0, 1000.0],
    [2.0, 2000.0],
    [4.0, 0.0],
  ]

def test_transfer_and_rename(tmp_path, sample_data):
  in1, in2 = sample_data
  out = tmp_path / "transfer.sdds"
  subprocess.run(
    [
      str(SDDSMREF),
      str(in1),
      str(in2),
      str(out),
      "-equate=id",
      "-take=value2",
      "-transfer=parameter,p2",
      "-rename=parameter,p2=p2new",
      "-rename=column,value2=renamed",
      "-fillIn",
      "-reuse=rows",
    ],
    check=True,
  )
  data = read_columns(out, ["id", "renamed"])
  assert data == [
    [1.0, 1000.0],
    [1.0, 1000.0],
    [2.0, 2000.0],
    [4.0, 0.0],
  ]
  assert read_parameter(out, "p2new") == pytest.approx(7.0)

def test_ifis_ifnot(tmp_path, sample_data):
  in1, in2 = sample_data
  out = tmp_path / "ok.sdds"
  subprocess.run(
    [
      str(SDDSMREF),
      str(in1),
      str(in2),
      str(out),
      "-equate=id",
      "-ifis=column,pattern",
      "-ifnot=column,nonexistent",
    ],
    check=True,
  )
  result = subprocess.run(
    [
      str(SDDSMREF),
      str(in1),
      str(in2),
      str(tmp_path / "bad.sdds"),
      "-equate=id",
      "-ifnot=column,pattern",
    ],
    capture_output=True,
    text=True,
  )
  assert "column pattern exists--aborting" in result.stderr

def test_pipe(tmp_path, sample_data):
  in1, in2 = sample_data
  proc = subprocess.run(
    [
      str(SDDSMREF),
      str(in1),
      str(in2),
      "-pipe=out",
      "-equate=id",
      "-take=value2",
      "-fillIn",
      "-reuse=rows",
    ],
    capture_output=True,
    check=True,
  )
  out = tmp_path / "pipe.sdds"
  out.write_bytes(proc.stdout)
  data = read_columns(out, ["id", "value2"])
  assert data == [
    [1.0, 1000.0],
    [1.0, 1000.0],
    [2.0, 2000.0],
    [4.0, 0.0],
  ]


def test_match_strings(tmp_path, sample_data):
  in1, in2 = sample_data
  out = tmp_path / "match.sdds"
  subprocess.run(
    [
      str(SDDSMREF),
      str(in1),
      str(in2),
      str(out),
      "-match=pattern=name",
      "-take=value2",
      "-fillIn",
      "-reuse=rows",
    ],
    check=True,
  )
  data = read_columns(out, ["value2"])
  assert data == [[1000.0], [1000.0], [2000.0], [3000.0]]
