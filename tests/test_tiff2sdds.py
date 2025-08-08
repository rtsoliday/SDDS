import subprocess
from pathlib import Path
import pytest
from PIL import Image

BIN_DIR = Path("bin/Linux-x86_64")
TIFF2SDDS = BIN_DIR / "tiff2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

pytestmark = pytest.mark.skipif(
  not (TIFF2SDDS.exists() and SDDS2PLAINDATA.exists()),
  reason="required tools not built",
)

def create_rgb_tiff(tmp_path: Path) -> Path:
  img = Image.new("RGB", (2, 2))
  img.putdata([
    (255, 0, 0), (0, 255, 0),
    (0, 0, 255), (255, 255, 255),
  ])
  path = tmp_path / "rgb.tiff"
  img.save(path, format="TIFF")
  return path

def create_gray8_tiff(tmp_path: Path) -> Path:
  img = Image.new("L", (2, 2))
  img.putdata([0, 64, 128, 255])
  path = tmp_path / "gray8.tiff"
  img.save(path, format="TIFF")
  return path

def create_gray16_tiff(tmp_path: Path) -> Path:
  img = Image.new("I;16", (2, 2))
  img.putdata([0, 65535, 32768, 16384])
  path = tmp_path / "gray16.tiff"
  img.save(path, format="TIFF")
  return path

def read_column(dataset: Path, column: str) -> list[int]:
  result = subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(dataset),
      "-pipe=out",
      f"-column={column}",
      "-noRowCount",
    ],
    capture_output=True,
    text=True,
    check=True,
  )
  return [int(x) for x in result.stdout.split()]

def run_tiff2sdds(input_file: Path, output_file: Path, *options: str) -> None:
  subprocess.run(
    [str(TIFF2SDDS), str(input_file), str(output_file), *options],
    check=True,
  )

@pytest.mark.parametrize(
  "options,expected",
  [
    ([], [[255, 765], [255, 255]]),
    (["-redOnly"], [[0, 255], [255, 0]]),
    (["-greenOnly"], [[0, 255], [0, 255]]),
    (["-blueOnly"], [[255, 255], [0, 0]]),
  ],
)
def test_rgb_options(tmp_path, options, expected):
  tiff = create_rgb_tiff(tmp_path)
  out = tmp_path / "out.sdds"
  run_tiff2sdds(tiff, out, *options)
  assert read_column(out, "Line00000") == expected[0]
  assert read_column(out, "Line00001") == expected[1]

def test_single_column_mode(tmp_path):
  tiff = create_rgb_tiff(tmp_path)
  out = tmp_path / "out.sdds"
  run_tiff2sdds(tiff, out, "-singleColumnMode")
  assert read_column(out, "z") == [255, 255, 765, 255]

def test_gray8(tmp_path):
  tiff = create_gray8_tiff(tmp_path)
  out = tmp_path / "out.sdds"
  run_tiff2sdds(tiff, out, "-redOnly")
  assert read_column(out, "Line00000") == [128, 255]
  assert read_column(out, "Line00001") == [0, 64]

def test_gray16(tmp_path):
  tiff = create_gray16_tiff(tmp_path)
  out = tmp_path / "out.sdds"
  run_tiff2sdds(tiff, out, "-redOnly")
  assert read_column(out, "Line00000") == [128, 64]
  assert read_column(out, "Line00001") == [0, 255]
