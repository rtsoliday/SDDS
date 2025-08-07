import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSBASELINE = BIN_DIR / "sddsbaseline"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED_TOOLS = [SDDSBASELINE, PLAINDATA2SDDS, SDDS2PLAINDATA]

@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
class TestSDDSBaseline:
  def create_sdds(self, values, tmp_path):
    plain = tmp_path / "input.txt"
    plain.write_text("\n".join(str(v) for v in values) + "\n")
    sdds = tmp_path / "input.sdds"
    subprocess.run(
      [
        str(PLAINDATA2SDDS),
        str(plain),
        str(sdds),
        "-column=col,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
      ],
      check=True,
    )
    return sdds

  def read_sdds(self, path):
    plain = path.with_suffix(".txt")
    subprocess.run(
      [
        str(SDDS2PLAINDATA),
        str(path),
        str(plain),
        "-column=col",
        "-outputMode=ascii",
        "-noRowCount",
      ],
      check=True,
    )
    return [float(x) for x in plain.read_text().split()]

  @pytest.mark.parametrize(
    "values,options,expected",
    [
      ([1, 2, 3, 4, 5], ["-columns=col", "-select=endpoints=1", "-method=average"], [-2, -1, 0, 1, 2]),
      ([1, 2, 3, 4, 5], ["-columns=col", "-select=endpoints=1", "-method=average", "-nonnegative"], [0, 0, 0, 1, 2]),
      ([1, 2, 3, 4, 5], ["-columns=col", "-select=endpoints=1", "-method=average", "-nonnegative", "-repeats=2"], [0, 0, 0, 0, 1]),
      ([0, 0, 10, 0, 0], ["-columns=col", "-select=endpoints=1", "-method=average", "-nonnegative", "-despike=passes=1,widthlimit=1"], [0, 0, 0, 0, 0]),
      ([0, 0, 10, 0, 0], ["-columns=col", "-select=outsideFWHA=0.5", "-method=average"], [0, 0, 10, 0, 0]),
      ([0, 1, 100, 1, 0], ["-columns=col", "-select=antioutlier=1", "-method=average"], [-20.4, -19.4, 79.6, -19.4, -20.4]),
      ([1, 2, 3, 4, 5], ["-columns=col", "-select=endpoints=2", "-method=fit,terms=2"], [0, 0, 0, 0, 0]),
    ],
  )
  def test_sddsbaseline_options(self, values, options, expected, tmp_path):
    input_sdds = self.create_sdds(values, tmp_path)
    output = tmp_path / "output.sdds"
    cmd = [str(SDDSBASELINE), str(input_sdds), str(output)] + options
    subprocess.run(cmd, check=True)
    data = self.read_sdds(output)
    assert data == pytest.approx(expected)

  def test_major_order_row(self, tmp_path):
    input_sdds = self.create_sdds([1, 2, 3, 4, 5], tmp_path)
    output = tmp_path / "out_row.sdds"
    subprocess.run(
      [
        str(SDDSBASELINE),
        str(input_sdds),
        str(output),
        "-columns=col",
        "-select=endpoints=1",
        "-method=average",
        "-majorOrder=row",
      ],
      check=True,
    )
    data = self.read_sdds(output)
    assert data == pytest.approx([-2, -1, 0, 1, 2])

  def test_pipe_output(self, tmp_path):
    input_sdds = self.create_sdds([1, 2, 3, 4, 5], tmp_path)
    result = subprocess.run(
      [
        str(SDDSBASELINE),
        str(input_sdds),
        "-pipe=out",
        "-columns=col",
        "-select=endpoints=1",
        "-method=average",
      ],
      check=True,
      stdout=subprocess.PIPE,
    )
    out_path = tmp_path / "pipe_out.sdds"
    out_path.write_bytes(result.stdout)
    data = self.read_sdds(out_path)
    assert data == pytest.approx([-2, -1, 0, 1, 2])
