import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSXREF = BIN_DIR / "sddsxref"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

REQUIRED = [SDDSXREF, PLAINDATA2SDDS, SDDSPRINTOUT]

def create_sdds(tmp_path, name, columns, rows):
  plain = tmp_path / f"{name}.txt"
  plain.write_text("\n".join(" ".join(str(v) for v in row) for row in rows) + "\n")
  sdds = tmp_path / f"{name}.sdds"
  cmd = [str(PLAINDATA2SDDS), str(plain), str(sdds)]
  for col, typ in columns:
    cmd.append(f"-column={col},{typ}")
  cmd.extend(["-inputMode=ascii", "-outputMode=ascii", "-noRowCount"])
  subprocess.run(cmd, check=True)
  return sdds

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

@pytest.fixture
def sample_data(tmp_path):
  cols1 = [("id", "long"), ("pattern", "string"), ("value1", "double"), ("matchval", "double")]
  rows1 = [
    (1, "A*", 10, 100),
    (1, "A*", 15, 100),
    (2, "B*", 20, 200),
    (4, "C*", 40, 400),
  ]
  in1 = create_sdds(tmp_path, "in1", cols1, rows1)
  cols2 = [("id", "long"), ("name", "string"), ("value2", "double"), ("matchval", "double"), ("value1", "double")]
  rows2 = [
    (1, "Alice", 1000, 100, 11),
    (2, "Beta", 2000, 200, 22),
    (3, "Ceta", 3000, 300, 33),
  ]
  in2 = create_sdds(tmp_path, "in2", cols2, rows2)
  return in1, in2

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_basic_flags(tmp_path, sample_data):
  in1, in2 = sample_data
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSXREF),
      str(in1),
      str(in2),
      str(out),
      "-equate=id",
      "-take=value2,value1",
      "-leave=value1",
      "-rename=column,value2=renamed",
      "-replace=column,value1",
      "-fillIn",
      "-reuse=rows",
      "-nowarnings",
    ],
    check=True,
  )
  data = read_columns(out, ["id", "value1", "renamed"])
  assert data == [
    [1.0, 11.0, 1000.0],
    [1.0, 11.0, 1000.0],
    [2.0, 22.0, 2000.0],
    [4.0, 40.0, 0.0],
  ]

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_wildmatch_and_pipe(tmp_path, sample_data):
  cols1 = [("name", "string")]
  rows1 = [("Alice",), ("Beta",), ("Carl",)]
  a = create_sdds(tmp_path, "a", cols1, rows1)
  cols2 = [("pattern", "string"), ("value2", "double")]
  rows2 = [("A*", 1000), ("B*", 2000), ("C*", 3000)]
  b = create_sdds(tmp_path, "b", cols2, rows2)
  proc = subprocess.run(
    [
      str(SDDSXREF),
      str(a),
      str(b),
      "-pipe=out",
      "-wildMatch=name=pattern",
      "-take=value2",
      "-fillIn",
    ],
    capture_output=True,
    check=True,
  )
  out = tmp_path / "wild.sdds"
  out.write_bytes(proc.stdout)
  data = read_columns(out, ["value2"])
  assert data == [[1000.0], [2000.0], [3000.0]]

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_equate(tmp_path, sample_data):
  in1, in2 = sample_data
  out = tmp_path / "equate.sdds"
  subprocess.run(
    [
      str(SDDSXREF),
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

@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="tools not built")
def test_ifis_ifnot(tmp_path, sample_data):
  in1, in2 = sample_data
  out = tmp_path / "ok.sdds"
  subprocess.run(
    [
      str(SDDSXREF),
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
      str(SDDSXREF),
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
