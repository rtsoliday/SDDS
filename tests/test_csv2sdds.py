import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
CSV2SDDS = BIN_DIR / "csv2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"

REQUIRED = [CSV2SDDS, SDDS2PLAINDATA, SDDS2STREAM, SDDSQUERY]

@pytest.mark.skipif(not all(p.exists() for p in REQUIRED), reason="sdds tools not built")
class TestCSV2SDDS:
  def run_plaindata(self, sdds_path, tmp_path, columns):
    out = tmp_path / "plain.txt"
    subprocess.run(
      [
        str(SDDS2PLAINDATA),
        str(sdds_path),
        str(out),
        *[f"-column={c}" for c in columns],
        "-outputMode=ascii",
        "-separator= ",
        "-noRowCount",
      ],
      check=True,
    )
    lines = out.read_text().strip().splitlines()
    return [line.split() for line in lines]

  def test_columndata_ascii_separator_delimiters_maxrows(self, tmp_path):
    csv = tmp_path / "input.csv"
    csv.write_text("[1];[2.5];[foo]\n[3];[4.5];[bar]\n")
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(CSV2SDDS),
        str(csv),
        str(out),
        "-separator=;",
        "-delimiters=start=[,end=]",
        "-columnData=name=c1,type=long,units=m",
        "-columnData=name=c2,type=double,units=s",
        "-columnData=name=c3,type=string",
        "-asciiOutput",
        "-maxRows=1",
      ],
      check=True,
    )
    data = self.run_plaindata(out, tmp_path, ["c1", "c2", "c3"])
    c1 = [float(row[0]) for row in data]
    c2 = [float(row[1]) for row in data]
    c3 = [row[2] for row in data]
    assert c1 == pytest.approx([1, 3])
    assert c2 == pytest.approx([2.5, 4.5])
    assert c3 == ["foo", "bar"]
    content = out.read_bytes()
    assert all(b in b"\n\r\t" or 32 <= b < 127 for b in content)

  def test_schfile(self, tmp_path):
    csv = tmp_path / "input.csv"
    csv.write_text("1,foo\n2,bar\n")
    sch = tmp_path / "input.sch"
    sch.write_text(
      "Filetype=Delimited\nSeparator=,\nField1=col1,float,\nField2=col2,string,\n"
    )
    out = tmp_path / "out.sdds"
    subprocess.run([str(CSV2SDDS), str(csv), str(out), f"-schfile={sch}"], check=True)
    data = self.run_plaindata(out, tmp_path, ["col1", "col2"])
    col1 = [float(row[0]) for row in data]
    col2 = [row[1] for row in data]
    assert col1 == pytest.approx([1, 2])
    assert col2 == ["foo", "bar"]

  def test_uselabels_with_units(self, tmp_path):
    csv = tmp_path / "input.csv"
    csv.write_text("x,y,label\nm,s,-\n1,2,foo\n3,4,bar\n")
    out = tmp_path / "out.sdds"
    subprocess.run([str(CSV2SDDS), str(csv), str(out), "-uselabels=units"], check=True)
    data = self.run_plaindata(out, tmp_path, ["x", "y", "label"])
    x = [float(row[0]) for row in data]
    y = [float(row[1]) for row in data]
    label = [row[2] for row in data]
    assert x == pytest.approx([1, 3])
    assert y == pytest.approx([2, 4])
    assert label == ["foo", "bar"]
    info = subprocess.run([str(SDDSQUERY), str(out)], capture_output=True, text=True, check=True).stdout
    assert "x" in info and "m" in info
    assert "y" in info and "s" in info

  def test_spanlines_and_skiplines(self, tmp_path):
    csv = tmp_path / "input.csv"
    csv.write_text("#skip\n1,2,3\n4,5,6\n")
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(CSV2SDDS),
        str(csv),
        str(out),
        "-skiplines=1",
        "-spanLines",
        "-columnData=name=col1,type=long",
        "-columnData=name=col2,type=long",
        "-columnData=name=col3,type=string",
      ],
      check=True,
    )
    result = subprocess.run(
      [str(SDDS2STREAM), str(out), "-columns=col1"], capture_output=True, text=True, check=True
    )
    assert result.stdout == "1\n4\n"
    result = subprocess.run(
      [str(SDDS2STREAM), str(out), "-columns=col3"], capture_output=True, text=True, check=True
    )
    assert result.stdout == "3\n6\n"

  def test_fillin_zero_and_last(self, tmp_path):
    csv = tmp_path / "input.csv"
    csv.write_text("1,2\n3,\n4,5\n")
    out0 = tmp_path / "out0.sdds"
    subprocess.run(
      [
        str(CSV2SDDS),
        str(csv),
        str(out0),
        "-columnData=name=a,type=long",
        "-columnData=name=b,type=long",
      ],
      check=True,
    )
    data0 = self.run_plaindata(out0, tmp_path, ["a", "b"])
    assert [row[1] for row in data0] == ["2", "0", "5"]
    out1 = tmp_path / "out1.sdds"
    subprocess.run(
      [
        str(CSV2SDDS),
        str(csv),
        str(out1),
        "-columnData=name=a,type=long",
        "-columnData=name=b,type=long",
        "-fillIn=last",
      ],
      check=True,
    )
    data1 = self.run_plaindata(out1, tmp_path, ["a", "b"])
    assert [row[1] for row in data1] == ["2", "2", "5"]

  def test_major_order_row(self, tmp_path):
    csv = tmp_path / "input.csv"
    csv.write_text("1,2\n3,4\n")
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(CSV2SDDS),
        str(csv),
        str(out),
        "-columnData=name=a,type=long",
        "-columnData=name=b,type=long",
        "-majorOrder=row",
      ],
      check=True,
    )
    data = self.run_plaindata(out, tmp_path, ["a", "b"])
    assert data == [["1", "2"], ["3", "4"]]

  def test_pipe_output(self, tmp_path):
    csv = tmp_path / "input.csv"
    csv.write_text("1,2\n3,4\n")
    result = subprocess.run(
      [
        str(CSV2SDDS),
        str(csv),
        "-pipe=out",
        "-columnData=name=a,type=long",
        "-columnData=name=b,type=long",
      ],
      stdout=subprocess.PIPE,
      check=True,
    )
    out = tmp_path / "out.sdds"
    out.write_bytes(result.stdout)
    data = self.run_plaindata(out, tmp_path, ["a", "b"])
    assert data == [["1", "2"], ["3", "4"]]
