import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSEPARATE = BIN_DIR / "sddsseparate"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDSCHECK = BIN_DIR / "sddscheck"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"

required = [SDDSSEPARATE, SDDSPRINTOUT, SDDSCHECK, PLAINDATA2SDDS]
pytestmark = pytest.mark.skipif(not all(p.exists() for p in required), reason="tools not built")

def make_input(tmp_path):
  txt = tmp_path / "input.txt"
  txt.write_text("1 10 100\n2 20 200\n3 30 300\n")
  sdds = tmp_path / "input.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(txt),
      str(sdds),
      "-noRowCount",
      "-column=colA,double",
      "-column=colB,double",
      "-column=copyCol,double",
    ],
    check=True,
  )
  return sdds

def parse_printout(text, column, caster):
  pages = []
  current = None
  for line in text.splitlines():
    stripped = line.strip()
    if not stripped:
      continue
    if stripped == column:
      if current is not None:
        pages.append(current)
      current = []
    elif set(stripped) == {"-"}:
      continue
    else:
      current.append(caster(stripped))
  if current is not None:
    pages.append(current)
  return pages

def parse_parameters(text, name):
  values = []
  for line in text.splitlines():
    stripped = line.strip()
    if stripped.startswith(name):
      parts = stripped.split("=", 1)
      values.append(parts[1].strip())
  return values

def test_group(tmp_path):
  input_file = make_input(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSSEPARATE), str(input_file), str(output), "-group=Combined,colA,colB"],
    check=True,
  )
  pr = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-noTitle", "-columns=Combined"],
    capture_output=True,
    text=True,
    check=True,
  )
  pages = parse_printout(pr.stdout, "Combined", float)
  assert pages == [[1.0, 2.0, 3.0], [10.0, 20.0, 30.0]]
  prp = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-noTitle", "-parameters=CombinedSourceColumn"],
    capture_output=True,
    text=True,
    check=True,
  )
  params = parse_parameters(prp.stdout, "CombinedSourceColumn")
  assert params == ["colA", "colB"]

def test_copy(tmp_path):
  input_file = make_input(tmp_path)
  output = tmp_path / "out_copy.sdds"
  subprocess.run(
    [
      str(SDDSSEPARATE),
      str(input_file),
      str(output),
      "-group=Combined,colA,colB",
      "-copy=copyCol",
    ],
    check=True,
  )
  pr = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-noTitle", "-columns=Combined"],
    capture_output=True,
    text=True,
    check=True,
  )
  combined = parse_printout(pr.stdout, "Combined", float)
  assert combined == [[1.0, 2.0, 3.0], [10.0, 20.0, 30.0]]
  prc = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-noTitle", "-columns=copyCol"],
    capture_output=True,
    text=True,
    check=True,
  )
  copied = parse_printout(prc.stdout, "copyCol", float)
  assert copied == [[100.0, 200.0, 300.0], [100.0, 200.0, 300.0]]

def test_pipe(tmp_path):
  input_file = make_input(tmp_path)
  file_out = tmp_path / "file.sdds"
  subprocess.run(
    [str(SDDSSEPARATE), str(input_file), str(file_out), "-group=Combined,colA,colB"],
    check=True,
  )
  proc = subprocess.run(
    [str(SDDSSEPARATE), str(input_file), "-pipe=out", "-group=Combined,colA,colB"],
    stdout=subprocess.PIPE,
    check=True,
  )
  pipe_out = tmp_path / "pipe.sdds"
  pipe_out.write_bytes(proc.stdout)
  assert file_out.read_bytes() == pipe_out.read_bytes()
  result = subprocess.run(
    [str(SDDSCHECK), str(pipe_out)], capture_output=True, text=True, check=True
  )
  assert result.stdout.strip() == "ok"
