import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSLOPES = BIN_DIR / "sddsslopes"
SDDS2STREAM = BIN_DIR / "sdds2stream"

REQUIRED_TOOLS = [SDDSSLOPES, SDDS2STREAM]


@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sddsslopes or sdds2stream not built")
class TestSddsslopes:
  def create_input(self, tmp_path, sigma_zero=False):
    data_lines = []
    x = [0, 1, 2, 3]
    y1 = [1, 3, 5, 7]
    y2 = [3, 2, 1, 0]
    y1sigma = [0.0 if sigma_zero and i == 0 else 0.1 for i in range(4)]
    y2sigma = [0.1] * 4
    other = [10, 20, 30, 40]
    for i in range(4):
      data_lines.append(f"{x[i]} {y1[i]} {y1sigma[i]} {y2[i]} {y2sigma[i]} {other[i]}\n")
    text = (
      "SDDS2\n"
      "&column name=x, type=double &end\n"
      "&column name=y1, type=double &end\n"
      "&column name=y1Sigma, type=double &end\n"
      "&column name=y2, type=double &end\n"
      "&column name=y2Sigma, type=double &end\n"
      "&column name=Other, type=double &end\n"
      "&data mode=ascii &end\n"
      "! page 1\n"
      "4\n"
      + ''.join(data_lines)
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
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1,y2",
        "-verbose",
      ],
      check=True,
    )
    slopes1 = self.read_column(out, "y1Slope")
    slopes2 = self.read_column(out, "y2Slope")
    ints1 = self.read_column(out, "y1Intercept")
    ints2 = self.read_column(out, "y2Intercept")
    assert slopes1 == pytest.approx([2.0])
    assert slopes2 == pytest.approx([-1.0])
    assert ints1 == pytest.approx([1.0])
    assert ints2 == pytest.approx([3.0])

  def test_exclude_columns(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1,y2,Other",
        "-excludeColumns=Other",
      ],
      check=True,
    )
    self.read_column(out, "y1Slope")
    with pytest.raises(subprocess.CalledProcessError):
      subprocess.run(
        [str(SDDS2STREAM), "-column=OtherSlope", str(out)],
        check=True,
        capture_output=True,
        text=True,
      )

  def test_range(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1",
        "-range=1,2",
      ],
      check=True,
    )
    slope = self.read_column(out, "y1Slope")
    intercept = self.read_column(out, "y1Intercept")
    assert slope == pytest.approx([2.0])
    assert intercept == pytest.approx([1.0])

  def test_sigma(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1,y2",
        "-sigma",
      ],
      check=True,
    )
    sig1 = self.read_column(out, "y1SlopeSigma")
    sig2 = self.read_column(out, "y2SlopeSigma")
    assert sig1 == pytest.approx([0.0447213595], abs=1e-6)
    assert sig2 == pytest.approx([0.0447213595], abs=1e-6)

  def test_sigma_generate_minimum(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1",
        "-sigma=generate,minimum=0.1",
      ],
      check=True,
    )
    sig = self.read_column(out, "y1SlopeSigma")
    assert sig == pytest.approx([0.0447213595], abs=1e-6)

  def test_residual(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    resid = tmp_path / "resid.sdds"
    subprocess.run(
      [
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1,y2",
        f"-residual={resid}",
      ],
      check=True,
    )
    res = self.read_column(resid, "y1")
    assert res == pytest.approx([0.0, 0.0, 0.0, 0.0], abs=1e-6)

  def test_ascii(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1",
        "-ascii",
      ],
      check=True,
    )
    header = out.read_bytes()[:5]
    assert header == b"SDDS1"

  def test_major_order_row(self, tmp_path):
    inp = self.create_input(tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1",
        "-majorOrder=row",
      ],
      check=True,
    )
    slope = self.read_column(out, "y1Slope")
    assert slope == pytest.approx([2.0])

  def test_nowarnings(self, tmp_path):
    inp = self.create_input(tmp_path, sigma_zero=True)
    out = tmp_path / "out.sdds"
    run1 = subprocess.run(
      [
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1",
        "-sigma",
      ],
      capture_output=True,
      text=True,
      check=True,
    )
    assert "Warning" in run1.stderr
    run2 = subprocess.run(
      [
        str(SDDSSLOPES),
        str(inp),
        str(out),
        "-independentVariable=x",
        "-columns=y1",
        "-sigma",
        "-nowarnings",
      ],
      capture_output=True,
      text=True,
      check=True,
    )
    assert "Warning" not in run2.stderr

  def test_pipe(self, tmp_path):
    inp = self.create_input(tmp_path)
    data = inp.read_bytes()
    result = subprocess.run(
      [
        str(SDDSSLOPES),
        "-pipe=in,out",
        "-independentVariable=x",
        "-columns=y1",
      ],
      input=data,
      stdout=subprocess.PIPE,
      check=True,
    )
    out = tmp_path / "out.sdds"
    out.write_bytes(result.stdout)
    slope = self.read_column(out, "y1Slope")
    assert slope == pytest.approx([2.0])
