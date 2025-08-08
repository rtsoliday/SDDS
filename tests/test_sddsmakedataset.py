import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED_TOOLS = [SDDSMAKEDATASET, SDDS2PLAINDATA]

@pytest.mark.skipif(not all(t.exists() for t in REQUIRED_TOOLS), reason="sdds tools not built")
class TestSDDSMakeDataset:
  def read_column(self, dataset, column, tmp_path):
    out = tmp_path / f"{column}.txt"
    subprocess.run([
      str(SDDS2PLAINDATA),
      str(dataset),
      str(out),
      f"-column={column}",
      "-outputMode=ascii",
      "-noRowCount",
    ], check=True)
    return [float(x) for x in out.read_text().split()]

  def read_parameter(self, dataset, parameter, tmp_path):
    out = tmp_path / f"{parameter}.txt"
    subprocess.run([
      str(SDDS2PLAINDATA),
      str(dataset),
      str(out),
      f"-parameter={parameter}",
      "-outputMode=ascii",
      "-noRowCount",
    ], check=True)
    return float(out.read_text().strip())

  def test_all_options(self, tmp_path):
    out = tmp_path / "out.sdds"
    subprocess.run([
      str(SDDSMAKEDATASET),
      str(out),
      "-defaultType=double",
      "-parameter=P1", "-data=1.5",
      "-column=C1", "-data=1,2,3",
      "-array=A1", "-data=4,5",
      "-noWarnings",
      "-ascii",
      "-description=sample",
      "-contents=test",
    ], check=True)

    header = out.read_text().split("&data")[0]
    assert "description text=sample" in header
    assert "contents=test" in header
    assert "&parameter name=P1, type=double" in header
    assert "&column name=C1, type=double" in header
    assert "&array name=A1, type=double" in header

    data_line = out.read_bytes().split(b"&data", 1)[1][:100]
    assert b"mode=ascii" in data_line

    pval = self.read_parameter(out, "P1", tmp_path)
    assert pval == pytest.approx(1.5)

    cvals = self.read_column(out, "C1", tmp_path)
    assert cvals == pytest.approx([1, 2, 3])

    lines = out.read_text().splitlines()
    idx = next(i for i, line in enumerate(lines) if "array A1:" in line)
    arr_vals = [float(x) for x in lines[idx + 1].split()]
    assert arr_vals == pytest.approx([4, 5])

  def test_pipe_output(self, tmp_path):
    result = subprocess.run([
      str(SDDSMAKEDATASET),
      "-pipe=out",
      "-defaultType=double",
      "-parameter=P1", "-data=2.5",
      "-column=C1", "-data=1,2,3",
      "-majorOrder=column",
    ], stdout=subprocess.PIPE, check=True)
    out = tmp_path / "pipe.sdds"
    out.write_bytes(result.stdout)
    cvals = self.read_column(out, "C1", tmp_path)
    assert cvals == pytest.approx([1, 2, 3])
    data_line = out.read_bytes().split(b"&data", 1)[1][:100]
    assert b"column_major_order=1" in data_line

  def test_append_new_page(self, tmp_path):
    out = tmp_path / "append.sdds"
    subprocess.run([
      str(SDDSMAKEDATASET),
      str(out),
      "-column=C,type=double", "-data=1,2",
      "-majorOrder=row",
      "-ascii",
    ], check=True)

    data_line = out.read_bytes().split(b"&data", 1)[1][:100]
    assert b"column_major_order" not in data_line

    subprocess.run([
      str(SDDSMAKEDATASET),
      str(out),
      "-column=C,type=double", "-data=3,4",
      "-append",
    ], check=True)

    assert out.read_text().count("! page number") == 2
    cvals = self.read_column(out, "C", tmp_path)
    assert cvals == pytest.approx([1, 2, 3, 4])

  def test_append_merge(self, tmp_path):
    out = tmp_path / "merge.sdds"
    subprocess.run([
      str(SDDSMAKEDATASET),
      str(out),
      "-column=C,type=double", "-data=1,2",
    ], check=True)

    subprocess.run([
      str(SDDSMAKEDATASET),
      str(out),
      "-column=C,type=double", "-data=3,4",
      "-append=merge",
    ], check=True)

    assert out.read_bytes().count(b"&data") == 1
    cvals = self.read_column(out, "C", tmp_path)
    assert cvals == pytest.approx([1, 2, 3, 4])
