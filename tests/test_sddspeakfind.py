import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSPEAKFIND = BIN_DIR / "sddspeakfind"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED = [SDDSPEAKFIND, PLAINDATA2SDDS, SDDS2PLAINDATA]


def create_sdds(data, tmp):
  plain = tmp / "in.txt"
  plain.write_text("\n".join(f"{x} {y}" for x, y in data) + "\n")
  sdds = tmp / "in.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(plain),
    str(sdds),
    "-column=x,double",
    "-column=y,double",
    "-inputMode=ascii",
    "-outputMode=ascii",
    "-noRowCount",
  ], check=True)
  return sdds


def read_peaks(path):
  plain = path.with_suffix(".txt")
  subprocess.run([
    str(SDDS2PLAINDATA),
    str(path),
    str(plain),
    "-column=x",
    "-column=y",
    "-outputMode=ascii",
    "-noRowCount",
    "-separator= ",
  ], check=True)
  text = plain.read_text().strip()
  if not text:
    return []
  return [float(line.split()[0]) for line in text.splitlines()]


cases = [
  (
    "fivepoint",
    [(0, 0), (1, 1), (2, 2), (3, 1), (4, 0)],
    ["-fivePoint"],
    [2],
    False,
  ),
  (
    "sevenpoint",
    [(0, 0), (1, 1), (2, 2), (3, 3), (4, 2), (5, 1), (6, 0)],
    ["-sevenPoint"],
    [3],
    False,
  ),
  (
    "threshold",
    [(0, 0), (1, 2), (2, 0), (3, 1), (4, 0)],
    ["-threshold=1.5"],
    [1],
    False,
  ),
  (
    "exclusion",
    [(0, 0), (1, 3), (2, 0), (3, 2), (4, 0)],
    ["-exclusionZone=1"],
    [1],
    False,
  ),
  (
    "change",
    [(0, 0.95), (1, 1.0), (2, 0.95)],
    ["-changeThreshold=0.1"],
    [],
    False,
  ),
  (
    "curvature",
    [(-1, 0.99), (0, 1.0), (1, 0.99)],
    ["-curvatureLimit=x,0.02"],
    [],
    False,
  ),
  (
    "major",
    [(0, 0), (1, 1), (2, 0)],
    ["-majorOrder=row"],
    [1],
    False,
  ),
  (
    "pipe",
    [(0, 0), (1, 1), (2, 0)],
    ["-pipe=out"],
    [1],
    True,
  ),
]


@pytest.mark.skipif(not all(t.exists() for t in REQUIRED), reason="sdds tools not built")
@pytest.mark.parametrize("name,data,args,expected,use_pipe", cases)
def test_sddspeakfind(name, data, args, expected, use_pipe, tmp_path):
  inp = create_sdds(data, tmp_path)
  out = tmp_path / f"{name}.sdds"
  if use_pipe:
    result = subprocess.run([
      str(SDDSPEAKFIND),
      str(inp),
      "-column=y",
      *args,
    ], stdout=subprocess.PIPE, check=True)
    out.write_bytes(result.stdout)
  else:
    subprocess.run([
      str(SDDSPEAKFIND),
      str(inp),
      str(out),
      "-column=y",
      *args,
    ], check=True)
  peaks = read_peaks(out)
  assert peaks == expected
