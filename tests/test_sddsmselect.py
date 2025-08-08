import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSMSELECT = BIN_DIR / "sddsmselect"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSCOMBINE = BIN_DIR / "sddscombine"

required = [SDDSMSELECT, PLAINDATA2SDDS, SDDS2STREAM, SDDSCOMBINE]
pytestmark = pytest.mark.skipif(not all(p.exists() for p in required), reason="tools not built")

def _create_sdds(path: Path, data: str, columns: list[str]):
  plain = path.with_suffix(".txt")
  plain.write_text(data.strip() + "\n")
  args = [str(PLAINDATA2SDDS), str(plain), str(path)]
  for col in columns:
    args.append(f"-column={col}")
  args.extend(["-inputMode=ascii", "-outputMode=ascii", "-noRowCount"])
  subprocess.run(args, check=True)

def _stream(path: Path, columns: str, page: int | None = None) -> str:
  args = [str(SDDS2STREAM), str(path), f"-columns={columns}"]
  if page is not None:
    args.insert(2, f"-page={page}")
  result = subprocess.run(args, capture_output=True, text=True, check=True)
  return result.stdout.strip()

def _npages(path: Path) -> int:
  result = subprocess.run([str(SDDS2STREAM), str(path), "-npages"], capture_output=True, text=True, check=True)
  return int(result.stdout.split()[0])

def test_match(tmp_path):
  in1 = tmp_path / "in1.sdds"
  _create_sdds(in1, "A 1\nB 2\nC 3\nD 4", ["name,string", "val,long"])
  in2 = tmp_path / "in2.sdds"
  _create_sdds(in2, "B 100\nD 200", ["name,string", "other,long"])
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMSELECT), str(in1), str(in2), str(out), "-match=name"], check=True)
  assert _stream(out, "name,val") == "B 2\nD 4"

def test_equate(tmp_path):
  in1 = tmp_path / "in1.sdds"
  _create_sdds(in1, "1\n2\n3\n4", ["val,long"])
  in2 = tmp_path / "in2.sdds"
  _create_sdds(in2, "2\n4", ["val,long"])
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMSELECT), str(in1), str(in2), str(out), "-equate=val"], check=True)
  assert _stream(out, "val") == "2\n4"

def test_invert(tmp_path):
  in1 = tmp_path / "in1.sdds"
  _create_sdds(in1, "A 1\nB 2\nC 3\nD 4", ["name,string", "val,long"])
  in2 = tmp_path / "in2.sdds"
  _create_sdds(in2, "B 10\nD 20", ["name,string", "other,long"])
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMSELECT), str(in1), str(in2), str(out), "-match=name", "-invert"], check=True)
  assert _stream(out, "name,val") == "A 1\nC 3"

def test_reuse_rows(tmp_path):
  in1 = tmp_path / "in1.sdds"
  _create_sdds(in1, "1\n1", ["val,long"])
  in2 = tmp_path / "in2.sdds"
  _create_sdds(in2, "1", ["val,long"])
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMSELECT), str(in1), str(in2), str(out), "-equate=val", "-reuse"], check=True)
  assert _stream(out, "val") == "1\n1"

def test_reuse_page(tmp_path):
  single = tmp_path / "single.sdds"
  _create_sdds(single, "1", ["val,long"])
  double = tmp_path / "double.sdds"
  subprocess.run([str(SDDSCOMBINE), str(single), str(single), str(double), "-overWrite"], check=True)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSMSELECT), str(double), str(single), str(out), "-equate=val", "-reuse=page"], check=True)
  assert _npages(out) == 2
  assert _stream(out, "val", page=1) == "1"
  assert _stream(out, "val", page=2) == "1"
