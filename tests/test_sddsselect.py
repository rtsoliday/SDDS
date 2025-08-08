import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSELECT = BIN_DIR / "sddsselect"
SDDSMAKE = BIN_DIR / "sddsmakedataset"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

TOOLS = [SDDSSELECT, SDDSMAKE, SDDSPRINTOUT]


def make_dataset(path, column, type_, values):
  subprocess.run(
    [
      str(SDDSMAKE),
      str(path),
      f"-column={column},type={type_}",
      f"-data={','.join(values)}",
      "-ascii",
    ],
    check=True,
  )


def read_column(path, column):
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(path),
      f"-column={column}",
      "-noTitle",
      "-noLabels",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  return result.stdout.strip().split()


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_match(tmp_path):
  in1 = tmp_path / "in1.sdds"
  in2 = tmp_path / "in2.sdds"
  out = tmp_path / "out.sdds"
  make_dataset(in1, "name", "string", ["A", "B", "C"])
  make_dataset(in2, "name", "string", ["A", "C"])
  subprocess.run(
    [
      str(SDDSSELECT),
      str(in1),
      str(in2),
      str(out),
      "-match=name",
    ],
    check=True,
  )
  assert read_column(out, "name") == ["A", "C"]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_equate(tmp_path):
  in1 = tmp_path / "in1n.sdds"
  in2 = tmp_path / "in2n.sdds"
  out = tmp_path / "outn.sdds"
  make_dataset(in1, "value", "long", ["1", "2", "3"])
  make_dataset(in2, "value", "long", ["2", "3"])
  subprocess.run(
    [
      str(SDDSSELECT),
      str(in1),
      str(in2),
      str(out),
      "-equate=value",
    ],
    check=True,
  )
  assert read_column(out, "value") == ["2", "3"]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_invert(tmp_path):
  in1 = tmp_path / "in1.sdds"
  in2 = tmp_path / "in2.sdds"
  out = tmp_path / "out_inv.sdds"
  make_dataset(in1, "name", "string", ["A", "B", "C"])
  make_dataset(in2, "name", "string", ["A", "C"])
  subprocess.run(
    [
      str(SDDSSELECT),
      str(in1),
      str(in2),
      str(out),
      "-match=name",
      "-invert",
    ],
    check=True,
  )
  assert read_column(out, "name") == ["B"]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_reuse(tmp_path):
  in1 = tmp_path / "reuse1.sdds"
  in2 = tmp_path / "reuse2.sdds"
  out1 = tmp_path / "out1.sdds"
  out2 = tmp_path / "out2.sdds"
  make_dataset(in1, "name", "string", ["A", "A", "B"])
  make_dataset(in2, "name", "string", ["A"])
  subprocess.run(
    [str(SDDSSELECT), str(in1), str(in2), str(out1), "-match=name"],
    check=True,
  )
  subprocess.run(
    [
      str(SDDSSELECT),
      str(in1),
      str(in2),
      str(out2),
      "-match=name",
      "-reuse",
    ],
    check=True,
  )
  assert read_column(out1, "name") == ["A"]
  assert read_column(out2, "name") == ["A", "A"]
