import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSMATRIXMULT = BIN_DIR / "sddsmatrixmult"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED = [SDDSMATRIXMULT, PLAINDATA2SDDS, SDDS2PLAINDATA]



def create_matrix(tmp_path, name, data, cols):
  text = tmp_path / f"{name}.txt"
  lines = [str(len(data))]
  for i, row in enumerate(data):
    lines.append("r%d %s" % (i, " ".join(str(x) for x in row)))
  text.write_text("\n".join(lines) + "\n")
  sdds = tmp_path / f"{name}.sdds"
  cmd = [
    str(PLAINDATA2SDDS),
    str(text),
    str(sdds),
    "-inputMode=ascii",
    "-separator= ",
    "-column=row,string",
  ]
  cmd.extend([f"-column={c},double" for c in cols])
  subprocess.run(cmd, check=True)
  return sdds


def read_matrix(path, tmp_path, cols):
  out = tmp_path / "matrix.txt"
  cmd = [
    str(SDDS2PLAINDATA),
    str(path),
    str(out),
    *[f"-column={c}" for c in cols],
    "-outputMode=ascii",
    "-separator= ",
    "-noRowCount",
  ]
  subprocess.run(cmd, check=True)
  lines = out.read_text().strip().splitlines()
  return [[float(x) for x in line.split()] for line in lines]


@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sddsmatrixmult not built")
def test_basic_multiplication(tmp_path):
  a = create_matrix(tmp_path, "a", [[1, 2], [3, 4]], ["a1", "a2"])
  b = create_matrix(tmp_path, "b", [[5, 6], [7, 8]], ["b1", "b2"])
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMATRIXMULT), str(a), str(b), str(out)], check=True)
  result = read_matrix(out, tmp_path, ["b1", "b2"])
  assert result == [[19.0, 22.0], [43.0, 50.0]]


@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sddsmatrixmult not built")
def test_commute(tmp_path):
  a = create_matrix(tmp_path, "a", [[1, 2], [3, 4]], ["a1", "a2"])
  b = create_matrix(tmp_path, "b", [[5, 6], [7, 8]], ["b1", "b2"])
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMATRIXMULT), "-commute", str(a), str(b), str(out)], check=True)
  result = read_matrix(out, tmp_path, ["a1", "a2"])
  assert result == [[23.0, 34.0], [31.0, 46.0]]
