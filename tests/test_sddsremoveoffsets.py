import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSREMOVE = BIN_DIR / "sddsremoveoffsets"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED_TOOLS = [SDDSREMOVE, PLAINDATA2SDDS, SDDS2PLAINDATA]

@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
class TestSddsRemoveOffsets:
  def create_sdds(self, values, tmp_path):
    plain = tmp_path / "in.txt"
    plain.write_text("\n".join(str(v) for v in values) + "\n")
    sdds = tmp_path / "in.sdds"
    subprocess.run(
      [
        str(PLAINDATA2SDDS),
        str(plain),
        str(sdds),
        "-column=Wave,double",
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
        "-column=Wave",
        "-outputMode=ascii",
        "-noRowCount",
      ],
      check=True,
    )
    return [float(x) for x in plain.read_text().split()]

  def test_basic(self, tmp_path):
    inp = self.create_sdds([10, 10, 20, 20, 10, 10, 20, 20], tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [str(SDDSREMOVE), str(inp), str(out), "-columns=Wave"],
      check=True,
    )
    assert self.read_sdds(out) == pytest.approx([0] * 8)

  def test_remove_commutation_offset_only(self, tmp_path):
    inp = self.create_sdds([10, 10, 20, 20, 10, 10, 20, 20], tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSREMOVE),
        str(inp),
        str(out),
        "-columns=Wave",
        "-removeCommutationOffsetOnly",
      ],
      check=True,
    )
    assert self.read_sdds(out) == pytest.approx([15] * 8)

  def test_fhead(self, tmp_path):
    inp = self.create_sdds([10, 10, 20, 20, 12, 12, 22, 22], tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [str(SDDSREMOVE), str(inp), str(out), "-columns=Wave", "-fhead=0.5"],
      check=True,
    )
    assert self.read_sdds(out) == pytest.approx([0, 0, 0, 0, 2, 2, 2, 2])

  def test_major_order(self, tmp_path):
    inp = self.create_sdds([10, 10, 20, 20, 10, 10, 20, 20], tmp_path)
    out = tmp_path / "out.sdds"
    subprocess.run(
      [
        str(SDDSREMOVE),
        str(inp),
        str(out),
        "-columns=Wave",
        "-majorOrder=row",
      ],
      check=True,
    )
    assert self.read_sdds(out) == pytest.approx([0] * 8)

  def test_pipe(self, tmp_path):
    inp = self.create_sdds([10, 10, 20, 20, 10, 10, 20, 20], tmp_path)
    data = inp.read_bytes()
    result = subprocess.run(
      [str(SDDSREMOVE), "-pipe=in,out", "-columns=Wave"],
      input=data,
      stdout=subprocess.PIPE,
      check=True,
    )
    out = tmp_path / "out.sdds"
    out.write_bytes(result.stdout)
    assert self.read_sdds(out) == pytest.approx([0] * 8)

  @pytest.mark.parametrize("mode", ["a", "b", "ab1", "ab2"])
  def test_commutation_mode(self, mode, tmp_path):
    inp = self.create_sdds([10, 10, 20, 20, 10, 10, 20, 20], tmp_path)
    out = tmp_path / f"out_{mode}.sdds"
    subprocess.run(
      [
        str(SDDSREMOVE),
        str(inp),
        str(out),
        "-columns=Wave",
        f"-commutationMode={mode}",
      ],
      check=True,
    )
    assert self.read_sdds(out) == pytest.approx([0] * 8)

  def test_verbose(self, tmp_path):
    inp = self.create_sdds([10, 10, 20, 20, 10, 10, 20, 20], tmp_path)
    out = tmp_path / "out.sdds"
    result = subprocess.run(
      [
        str(SDDSREMOVE),
        str(inp),
        str(out),
        "-columns=Wave",
        "-verbose",
      ],
      text=True,
      capture_output=True,
      check=True,
    )
    assert "offset1" in result.stdout
    assert "New average" in result.stdout
    assert self.read_sdds(out) == pytest.approx([0] * 8)
