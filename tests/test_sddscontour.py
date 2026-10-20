"""Check shaded camera crops against independently cropped image data."""

import shlex
import subprocess

import pytest

from sdds_test_utils import BIN_DIR

Image = pytest.importorskip("PIL.Image", reason="Pillow is required for contour image tests")
SDDSCONTOUR = BIN_DIR / "sddscontour"
pytestmark = pytest.mark.skipif(not SDDSCONTOUR.exists(), reason="sddscontour not built")


def write_image(path, rows, columns, pattern):
  """Write camera columns with known horizontal or vertical intensity bands."""
  lines = [
    "SDDS1",
    "&column name=Index, type=long &end",
    "&column name=x, type=double &end",
    *(f"&column name=HLine{column}, type=double &end" for column in columns),
    "&data mode=ascii &end",
    str(len(rows)),
  ]
  for row in rows:
    values = [row if pattern == "vertical" else column for column in columns]
    lines.append(" ".join(str(value) for value in [row, row, *values]))
  path.write_text("\n".join(lines) + "\n")


def render_image(path, output, options):
  command = [
    str(SDDSCONTOUR), str(path),
    "-shade=100,0,8", "-device=lpng", f"-output={output}",
    "-nolabels", "-noscales", "-noborder", "-nocolorbar", "-fillscreen",
    *options,
  ]
  result = subprocess.run(command, capture_output=True, text=True, timeout=10)
  output.with_suffix(".log").write_text(
    f"Command: {shlex.join(command)}\nExit code: {result.returncode}\n"
    f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
  )
  result.check_returncode()
  with Image.open(output) as image:
    image = image.convert("RGB")
    assert any(low != high for low, high in image.getextrema()), "Image must contain intensity variation"
    return image.size, image.tobytes()


@pytest.mark.parametrize("pattern", ["vertical", "horizontal"])
@pytest.mark.parametrize("coordinates", ["range", "column", "range_swapped"])
def test_shaded_columnmatch_crop(tmp_path, pattern, coordinates):
  """A plot-window crop must match the same pixels cropped before plotting."""
  full = tmp_path / "full.sdds"
  cropped = tmp_path / "cropped.sdds"
  write_image(full, range(9), range(8), pattern)
  write_image(cropped, range(2, 6), range(3, 7), pattern)

  options = ["-columnmatch=Index,HLine*", "-yrange=min=0,max=7"]
  if coordinates == "column":
    options[0] = "-columnmatch=x,HLine*"
  else:
    options.append("-xrange=min=0,max=8")
  if coordinates == "range_swapped":
    options.extend(["-swapxy", "-scales=3,6,2,5"])
  else:
    options.append("-scales=2,5,3,6")

  reference_options = [
    "-columnmatch=Index,HLine*", "-xrange=min=2,max=5", "-yrange=min=3,max=6",
  ]
  if coordinates == "range_swapped":
    reference_options.append("-swapxy")

  expected = render_image(cropped, tmp_path / "expected.png", reference_options)
  actual = render_image(full, tmp_path / "actual.png", options)
  assert actual == expected, "Shading does not match the independently cropped image"
