import subprocess
from pathlib import Path
import pytest
from PIL import Image

BIN_DIR = Path("bin/Linux-x86_64")
SDDS2TIFF = BIN_DIR / "sdds2tiff"
TIFF2SDDS = BIN_DIR / "tiff2sdds"
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"

pytestmark = pytest.mark.skipif(
    not (SDDS2TIFF.exists() and TIFF2SDDS.exists() and SDDSMAKEDATASET.exists()),
    reason="required tools not built",
)

def run_tiff2sdds(tiff: Path, sdds: Path, *opts: str) -> None:
    subprocess.run([str(TIFF2SDDS), str(tiff), str(sdds), *opts], check=True)

def run_sdds2tiff(sdds: Path, out_prefix: Path, *opts: str, stdin=None) -> None:
    args = [str(SDDS2TIFF), str(sdds), str(out_prefix), *opts]
    subprocess.run(args, check=True, stdin=stdin)

def read_pixels(path: Path) -> list[int]:
    with Image.open(path) as img:
        return list(img.getdata())

def create_gray8(tmp_path: Path, values) -> Path:
    img = Image.new("L", (2, 2))
    img.putdata(values)
    p = tmp_path / "gray8.tiff"
    img.save(p, format="TIFF")
    return p

def create_gray16(tmp_path: Path) -> Path:
    img = Image.new("I;16", (2, 2))
    img.putdata([0, 65535, 32768, 16384])
    p = tmp_path / "gray16.tiff"
    img.save(p, format="TIFF")
    return p

def scale_to_max(values: list[int]) -> list[int]:
    m = max(values)
    return [int(v * 255 / m + 0.5) for v in values]

def test_column_prefix(tmp_path):
    dataset = tmp_path / "data.sdds"
    subprocess.run(
        [
            str(SDDSMAKEDATASET),
            str(dataset),
            "-column=Img00000,type=short",
            "-data=30,40",
            "-column=Img00001,type=short",
            "-data=10,20",
        ],
        check=True,
    )
    out_prefix = tmp_path / "out"
    run_sdds2tiff(dataset, out_prefix, "-columnPrefix=Img")
    out = Path(f"{out_prefix}.0001")
    assert read_pixels(out) == [10, 20, 30, 40]

def test_max_contrast(tmp_path):
    vals = [10, 20, 30, 40]
    tiff = create_gray8(tmp_path, vals)
    sdds = tmp_path / "data.sdds"
    run_tiff2sdds(tiff, sdds, "-redOnly")
    out_prefix = tmp_path / "out"
    run_sdds2tiff(sdds, out_prefix, "-maxContrast")
    out = Path(f"{out_prefix}.0001")
    assert read_pixels(out) == scale_to_max(vals)

def test_sixteen_bit(tmp_path):
    tiff = create_gray16(tmp_path)
    sdds = tmp_path / "data.sdds"
    run_tiff2sdds(tiff, sdds, "-redOnly")
    out_prefix = tmp_path / "out"
    run_sdds2tiff(sdds, out_prefix, "-16bit")
    out = Path(f"{out_prefix}.0001")
    with Image.open(out) as img:
        assert img.mode == "I;16"
        expected = [v >> 8 for v in [0, 65535, 32768, 16384]]
        assert list(img.getdata()) == expected

def test_page_range(tmp_path):
    sdds = tmp_path / "data.sdds"
    subprocess.run(
        [
            str(SDDSMAKEDATASET),
            str(sdds),
            "-column=Line00000,type=short",
            "-data=0,255",
            "-column=Line00001,type=short",
            "-data=255,0",
        ],
        check=True,
    )
    out_prefix = tmp_path / "out"
    run_sdds2tiff(sdds, out_prefix, "-fromPage=1", "-toPage=1")
    out1 = Path(f"{out_prefix}.0001")
    assert out1.exists()
    assert read_pixels(out1) == [255, 0, 0, 255]

def test_pipe(tmp_path):
    tiff = create_gray8(tmp_path, [1, 2, 3, 4])
    sdds = tmp_path / "data.sdds"
    run_tiff2sdds(tiff, sdds, "-redOnly")
    out_prefix = tmp_path / "out"
    with open(sdds, "rb") as f:
        subprocess.run([str(SDDS2TIFF), "-pipe=in", str(out_prefix)], stdin=f, check=True)
    out = Path(f"{out_prefix}.0001")
    assert read_pixels(out) == [1, 2, 3, 4]
