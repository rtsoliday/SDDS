import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSTRANSPOSE = BIN_DIR / "sddstranspose"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"
SDDSQUERY = BIN_DIR / "sddsquery"

REQUIRED = [SDDSTRANSPOSE, PLAINDATA2SDDS, SDDS2PLAINDATA, SDDSQUERY]
EXPECTED = [[1.0, 3.0], [2.0, 4.0]]


def create_input(tmp_path):
  text = tmp_path / "input.txt"
  text.write_text("2\nc1 x1 1 2\nc2 x2 3 4\n")
  sdds = tmp_path / "input.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(text),
      str(sdds),
      "-inputMode=ascii",
      "-separator= ",
      "-column=Names,string",
      "-column=Alt,string",
      "-column=A,double",
      "-column=B,double",
    ],
    check=True,
  )
  return sdds


def create_basic_input(tmp_path):
  text = tmp_path / "basic.txt"
  text.write_text("2\nc1 1 2\nc2 3 4\n")
  sdds = tmp_path / "basic.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(text),
      str(sdds),
      "-inputMode=ascii",
      "-separator= ",
      "-column=Names,string",
      "-column=A,double",
      "-column=B,double",
    ],
    check=True,
  )
  return sdds


def read_matrix(path, tmp_path, columns):
  out = tmp_path / "matrix.txt"
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(path),
      str(out),
      *[f"-column={c}" for c in columns],
      "-outputMode=ascii",
      "-separator= ",
      "-noRowCount",
    ],
    check=True,
  )
  lines = out.read_text().strip().splitlines()
  return [[float(x) for x in line.split()] for line in lines]


def read_strings(path, tmp_path, column):
  out = tmp_path / "strings.txt"
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(path),
      str(out),
      f"-column={column}",
      "-outputMode=ascii",
      "-separator= ",
      "-noRowCount",
    ],
    check=True,
  )
  return out.read_text().strip().splitlines()


def column_names(path):
  result = subprocess.run(
    [str(SDDSQUERY), "-columnlist", str(path)],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout.strip().splitlines()


def header_text(path):
  return Path(path).read_bytes().decode("latin1")


@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sddstranspose not built")
def test_index_column(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-indexColumn", str(inp), str(out)], check=True)
  assert column_names(out) == ["OldColumnNames", "Index", "c1", "c2"]
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED
  index = [r[0] for r in read_matrix(out, tmp_path, ["Index"])]
  assert index == [0.0, 1.0]


def test_no_old_column_names(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-noOldColumnNames", str(inp), str(out)], check=True)
  assert column_names(out) == ["c1", "c2"]
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED


def test_old_column_names_custom(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-oldColumnNames=Prev", str(inp), str(out)], check=True)
  assert column_names(out)[0] == "Prev"
  assert read_strings(out, tmp_path, "Prev") == ["A", "B"]
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED


def test_symbol(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-symbol=SYM", str(inp), str(out)], check=True)
  info = subprocess.run([str(SDDSQUERY), str(out)], capture_output=True, text=True, check=True).stdout
  assert re.search(r"c1\s+NULL\s+SYM", info)
  assert re.search(r"c2\s+NULL\s+SYM", info)
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED


def test_root(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-root=Col", str(inp), str(out)], check=True)
  assert column_names(out) == ["OldColumnNames", "Col000", "Col001"]
  assert read_matrix(out, tmp_path, ["Col000", "Col001"]) == EXPECTED


def test_digits(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-root=Col", "-digits=2", str(inp), str(out)], check=True)
  assert column_names(out) == ["OldColumnNames", "Col00", "Col01"]
  assert read_matrix(out, tmp_path, ["Col00", "Col01"]) == EXPECTED


def test_new_column_names(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-newColumnNames=Alt", str(inp), str(out)], check=True)
  assert column_names(out) == ["OldColumnNames", "x1", "x2"]
  assert read_matrix(out, tmp_path, ["x1", "x2"]) == EXPECTED


def test_match_column(tmp_path):
  inp = create_basic_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-matchColumn=*", str(inp), str(out)], check=True)
  assert column_names(out) == ["OldColumnNames", "c1", "c2"]
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED


def test_ascii(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-ascii", str(inp), str(out)], check=True)
  query = subprocess.run([str(SDDSQUERY), str(out)], capture_output=True, text=True, check=True).stdout
  assert "data is ASCII" in query
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED


def test_major_order_row(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-majorOrder=row", str(inp), str(out)], check=True)
  header = header_text(out)
  assert "column_major_order=1" not in header
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED


def test_major_order_column(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run([str(SDDSTRANSPOSE), "-majorOrder=column", str(inp), str(out)], check=True)
  header = header_text(out)
  assert "column_major_order=1" in header
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED


def test_pipe(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  with open(inp, "rb") as f:
    result = subprocess.run([str(SDDSTRANSPOSE), "-pipe"], input=f.read(), stdout=subprocess.PIPE, check=True)
  out.write_bytes(result.stdout)
  assert column_names(out) == ["OldColumnNames", "c1", "c2"]
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED


def test_verbose(tmp_path):
  inp = create_input(tmp_path)
  out = tmp_path / "out.sdds"
  result = subprocess.run(
    [str(SDDSTRANSPOSE), "-verbose", str(inp), str(out)],
    capture_output=True,
    text=True,
    check=True,
  )
  assert "Number of numerical columns" in result.stderr
  assert read_matrix(out, tmp_path, ["c1", "c2"]) == EXPECTED
