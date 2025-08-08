import re
import subprocess
from pathlib import Path

import pytest

BIN_DIR = Path("bin/Linux-x86_64")
IMAGE2SDDS = BIN_DIR / "image2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"


def _numeric_lines(text):
  return [line.strip() for line in text.splitlines() if re.match(r"[+-]?\d", line.strip())]


@pytest.fixture
def sample_image(tmp_path):
  data = bytes(range(6))
  img = tmp_path / "input.img"
  img.write_bytes(data)
  return img


@pytest.mark.skipif(
  not all(p.exists() for p in [IMAGE2SDDS, SDDSPRINTOUT]),
  reason="tools not built",
)
def test_ascii_output(sample_image, tmp_path):
  out = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(IMAGE2SDDS),
      str(sample_image),
      str(out),
      "-xdim",
      "3",
      "-ydim",
      "2",
      "-ascii",
    ],
    check=True,
  )
  assert out.read_text().splitlines()[0] == "SDDS1"
  result = subprocess.run(
    [str(SDDSPRINTOUT), str(out), "-noTitle", "-column=Image"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert _numeric_lines(result.stdout) == ["0", "1", "2", "3", "4", "5"]


@pytest.mark.skipif(
  not all(p.exists() for p in [IMAGE2SDDS, SDDSPRINTOUT]),
  reason="tools not built",
)
def test_transpose(sample_image, tmp_path):
  out = tmp_path / "transpose.sdds"
  subprocess.run(
    [
      str(IMAGE2SDDS),
      str(sample_image),
      str(out),
      "-xdim",
      "3",
      "-ydim",
      "2",
      "-ascii",
      "-transpose",
    ],
    check=True,
  )
  result = subprocess.run(
    [str(SDDSPRINTOUT), str(out), "-noTitle", "-column=Image"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert _numeric_lines(result.stdout) == ["0", "2", "4", "1", "3", "5"]


@pytest.mark.skipif(
  not all(p.exists() for p in [IMAGE2SDDS, SDDSPRINTOUT]),
  reason="tools not built",
)
def test_multicolumn_mode(sample_image, tmp_path):
  out = tmp_path / "multi.sdds"
  subprocess.run(
    [
      str(IMAGE2SDDS),
      str(sample_image),
      str(out),
      "-xdim",
      "3",
      "-ydim",
      "2",
      "-ascii",
      "-multicolumnmode",
      "-xmin",
      "2",
      "-xmax",
      "4",
    ],
    check=True,
  )
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(out),
      "-noTitle",
      "-columns=Index",
      "-columns=Line0",
      "-columns=Line1",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  rows = [list(map(float, line.split())) for line in _numeric_lines(result.stdout)]
  assert rows == [[2.0, 0.0, 1.0], [3.0, 2.0, 3.0], [4.0, 4.0, 5.0]]


@pytest.mark.skipif(not IMAGE2SDDS.exists(), reason="image2sdds not built")
def test_array_mode(sample_image, tmp_path):
  out = tmp_path / "array.sdds"
  subprocess.run(
    [
      str(IMAGE2SDDS),
      str(sample_image),
      str(out),
      "-xdim",
      "3",
      "-ydim",
      "2",
      "-ascii",
      "-2d",
    ],
    check=True,
  )
  lines = out.read_text().splitlines()
  assert lines[-1] == "\\000 \\000 \\001 \\000 \\002 \\000"


@pytest.mark.skipif(
  not all(p.exists() for p in [IMAGE2SDDS, SDDSPRINTOUT]),
  reason="tools not built",
)
def test_contour_parameters(sample_image, tmp_path):
  out = tmp_path / "contour.sdds"
  subprocess.run(
    [
      str(IMAGE2SDDS),
      str(sample_image),
      str(out),
      "-xdim",
      "3",
      "-ydim",
      "2",
      "-ascii",
      "-contour",
      "-xmin",
      "0",
      "-xmax",
      "0.2",
      "-ymin",
      "1",
      "-ymax",
      "1.5",
    ],
    check=True,
  )
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(out),
      "-noTitle",
      "-parameter=Variable1Name",
      "-parameter=Variable2Name",
      "-parameter=xInterval",
      "-parameter=xMinimum",
      "-parameter=xDimension",
      "-parameter=yInterval",
      "-parameter=yMinimum",
      "-parameter=yDimension",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  text = result.stdout
  assert "Variable1Name =                x" in text
  assert "Variable2Name =                y" in text
  assert "xInterval =         1.000000e-01" in text
  assert "xMinimum =          0.000000e+00" in text
  assert "xDimension =                   3" in text
  assert "yInterval =         5.000000e-01" in text
  assert "yMinimum =          1.000000e+00" in text
  assert "yDimension =                   2" in text


@pytest.mark.skipif(
  not all(p.exists() for p in [IMAGE2SDDS, SDDSPRINTOUT]),
  reason="tools not built",
)
def test_debug_option(sample_image, tmp_path):
  out = tmp_path / "debug.sdds"
  subprocess.run(
    [
      str(IMAGE2SDDS),
      str(sample_image),
      str(out),
      "-xdim",
      "3",
      "-ydim",
      "2",
      "-ascii",
      "-debug",
      "2",
    ],
    check=True,
  )
  result = subprocess.run(
    [str(SDDSPRINTOUT), str(out), "-noTitle", "-column=Image"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert _numeric_lines(result.stdout) == ["0", "1", "2", "3", "4", "5"]


@pytest.mark.skipif(not IMAGE2SDDS.exists(), reason="image2sdds not built")
def test_help_option():
  result = subprocess.run(
    [str(IMAGE2SDDS), "in.img", "out.sdds", "-help"], capture_output=True, text=True
  )
  assert "Usage" in result.stderr
  assert result.returncode == 0
