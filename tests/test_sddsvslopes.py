import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSVSLOPES = BIN_DIR / "sddsvslopes"
SDDS2STREAM = BIN_DIR / "sdds2stream"

REQUIRED_TOOLS = [SDDSVSLOPES, SDDS2STREAM]


@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sddsvslopes or sdds2stream not built")
class TestSddsvslopes:
  def create_input(self, tmp_path):
    text = (
      "SDDS2\n"
      "&parameter name=Setpoint, type=double &end\n"
      "&column name=Index, type=long &end\n"
      "&column name=Rootname, type=string &end\n"
      "&column name=Vert, type=double &end\n"
      "&column name=Other, type=double &end\n"
      "&data mode=ascii &end\n"
      "! page 1\n"
      "0\n"
      "2\n"
      "1 r1 1 5\n"
      "2 r2 2 7\n"
      "! page 2\n"
      "1\n"
      "2\n"
      "1 r1 2.5 5\n"
      "2 r2 0 7\n"
      "! page 3\n"
      "2\n"
      "2\n"
      "1 r1 4 5\n"
      "2 r2 -2 7\n"
      "! page 4\n"
      "3\n"
      "2\n"
      "1 r1 5.5 5\n"
      "2 r2 -4 7\n"
    )
    path = tmp_path / "input.sdds"
    path.write_text(text)
    return path

  def read_column(self, path, column):
    result = subprocess.run(
      [str(SDDS2STREAM), f"-column={column}", str(path)],
      capture_output=True,
      text=True,
      check=True,
    )
    return [float(x) for x in result.stdout.strip().split()]

  def test_columns_and_verbose(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSVSLOPES),
        str(inp),
        str(out),
        "-independentVariable=Setpoint",
        "-columns=Vert",
        "-verbose",
      ],
      check=True,
    )
    slopes = self.read_column(out, "VertSlope")
    assert slopes == pytest.approx([1.5, -2.0])
    with pytest.raises(subprocess.CalledProcessError):
      subprocess.run(
        [str(SDDS2STREAM), "-column=OtherSlope", str(out)],
        check=True,
        capture_output=True,
        text=True,
      )

  def test_exclude_columns(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSVSLOPES),
        str(inp),
        str(out),
        "-independentVariable=Setpoint",
        "-excludeColumns=Other",
      ],
      check=True,
    )
    slopes = self.read_column(out, "VertSlope")
    assert slopes == pytest.approx([1.5, -2.0])
    with pytest.raises(subprocess.CalledProcessError):
      subprocess.run(
        [str(SDDS2STREAM), "-column=OtherSlope", str(out)],
        check=True,
        capture_output=True,
        text=True,
      )

  def test_sigma(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSVSLOPES),
        str(inp),
        str(out),
        "-independentVariable=Setpoint",
        "-columns=Vert",
        "-sigma",
      ],
      check=True,
    )
    slopes = self.read_column(out, "VertSlope")
    sigmas = self.read_column(out, "VertSlopeSigma")
    assert slopes == pytest.approx([1.5, -2.0])
    assert sigmas == pytest.approx([0.0, 0.0], abs=1e-6)

  def test_major_order_row(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSVSLOPES),
        str(inp),
        str(out),
        "-independentVariable=Setpoint",
        "-columns=Vert",
        "-majorOrder=row",
      ],
      check=True,
    )
    slopes = self.read_column(out, "VertSlope")
    assert slopes == pytest.approx([1.5, -2.0])

  def test_pipe(self, tmp_path):
    inp = self.create_input(tmp_path)
    data = inp.read_bytes()
    result = subprocess.run(
      [
        str(SDDSVSLOPES),
        "-pipe=in,out",
        "-independentVariable=Setpoint",
        "-columns=Vert",
      ],
      input=data,
      stdout=subprocess.PIPE,
      check=True,
    )
    out = tmp_path / "out.sdds"
    out.write_bytes(result.stdout)
    slopes = self.read_column(out, "VertSlope")
    assert slopes == pytest.approx([1.5, -2.0])
