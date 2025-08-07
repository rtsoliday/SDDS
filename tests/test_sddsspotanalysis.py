import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSPOTANALYSIS = BIN_DIR / "sddsspotanalysis"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2STREAM = BIN_DIR / "sdds2stream"

REQUIRED_TOOLS = [SDDSSPOTANALYSIS, PLAINDATA2SDDS, SDDS2STREAM]


@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
class TestSDDSSpotAnalysis:
  def create_xyz(self, tmp_path):
    lines = []
    for y in range(5):
      for x in range(5):
        intensity = 0
        if x == 2 and y == 2:
          intensity = 10
        elif x == 0 and y == 0:
          intensity = -1
        elif x == 4 and y == 4:
          intensity = 5
        lines.append(f"{x} {y} {intensity}")
    plain = tmp_path / "xyz.txt"
    plain.write_text("\n".join(lines) + "\n")
    sdds = tmp_path / "xyz.sdds"
    subprocess.run(
      [
        str(PLAINDATA2SDDS),
        str(plain),
        str(sdds),
        "-column=ix,long",
        "-column=iy,long",
        "-column=intensity,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
      ],
      check=True,
    )
    return sdds

  def create_image_columns(self, tmp_path):
    plain = tmp_path / "img.txt"
    plain.write_text("0 0 0\n0 10 0\n0 0 0\n")
    sdds = tmp_path / "img.sdds"
    subprocess.run(
      [
        str(PLAINDATA2SDDS),
        str(plain),
        str(sdds),
        "-column=row0,double",
        "-column=row1,double",
        "-column=row2,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
      ],
      check=True,
    )
    return sdds

  def read_columns(self, path, columns):
    cmd = [str(SDDS2STREAM), str(path), f"-columns={','.join(columns)}", "-page=1"]
    result = subprocess.run(cmd, check=True, stdout=subprocess.PIPE)
    data = [float(x) for x in result.stdout.split()]
    return data

  def test_all_flags_xyz(self, tmp_path):
    input_sdds = self.create_xyz(tmp_path)
    output = tmp_path / "out.sdds"
    spot = tmp_path / "spot.sdds"
    result = subprocess.run(
      [
        str(SDDSSPOTANALYSIS),
        str(input_sdds),
        "-pipe=out",
        "-xyz=ix,iy,intensity",
        "-ROI=x0value=0,x1value=4,y0value=0,y1value=4",
        "-spotROIsize=xvalue=3,yvalue=3",
        "-centerOn=xcentroid,ycentroid",
        "-levels=intensityLevels=20,saturationLevel=9",
        "-blankOut=x0value=3,x1value=4,y0value=3,y1value=4",
        "-sizeLines=xvalue=1,yvalue=1",
        "-background=halfwidth=1,symmetric,antihalo",
        "-despike=neighbors=2,passes=1,averageOf=2,threshold=100,keep",
        "-hotpixelFilter=fraction=2,distance=1,passes=1",
        "-singleSpot",
        "-spotImage=" + str(spot),
        "-majorOrder=row",
        "-clipNegative",
      ],
      stdout=subprocess.PIPE,
      check=True,
    )
    output.write_bytes(result.stdout)
    values = self.read_columns(
      output,
      [
        "xROILower",
        "xROIUpper",
        "yROILower",
        "yROIUpper",
        "IntegratedSpotIntensity",
        "BackgroundKilledNegative",
        "SaturationCount",
        "xSpotCenter",
        "ySpotCenter",
      ],
    )
    assert values[0:4] == [0, 4, 0, 4]
    assert values[4] == pytest.approx(10)
    assert values[5] >= 1
    assert values[6] >= 1
    assert values[7] == pytest.approx(2)
    assert values[8] == pytest.approx(2)
    assert spot.exists()

  def test_image_columns(self, tmp_path):
    input_sdds = self.create_image_columns(tmp_path)
    output = tmp_path / "out_img.sdds"
    result = subprocess.run(
      [
        str(SDDSSPOTANALYSIS),
        str(input_sdds),
        "-pipe=out",
        "-imageColumns=row?",
        "-spotROIsize=xvalue=3,yvalue=3",
        "-centerOn=xcentroid,ycentroid",
      ],
      stdout=subprocess.PIPE,
      check=True,
    )
    output.write_bytes(result.stdout)
    values = self.read_columns(output, ["IntegratedSpotIntensity"])
    assert values[0] == pytest.approx(10)
