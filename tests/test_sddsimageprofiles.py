import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSIMAGEPROFILES = BIN_DIR / "sddsimageprofiles"

IMAGE_DATA = """SDDS1
&column name=I1, type=double, &end
&column name=I2, type=double, &end
&column name=I3, type=double, &end
&data mode=ascii, &end
! page number 1
3
1 2 3
4 5 6
7 8 9
"""

BACKGROUND_DATA = """SDDS1
&column name=I1, type=double, &end
&column name=I2, type=double, &end
&column name=I3, type=double, &end
&data mode=ascii, &end
! page number 1
3
0 1 2
1 2 3
2 3 4
"""

def write_image(path):
  path.write_text(IMAGE_DATA)

def write_background(path):
  path.write_text(BACKGROUND_DATA)

def read_sdds(path_or_bytes):
  if isinstance(path_or_bytes, bytes):
    text = path_or_bytes.decode()
  else:
    text = Path(path_or_bytes).read_text()
  lines = [line.strip() for line in text.splitlines()]
  idx = lines.index('! page number 1')
  count = int(lines[idx + 2])
  data = []
  for line in lines[idx + 3 : idx + 3 + count]:
    data.append([float(x) for x in line.split()])
  return data

@pytest.mark.skipif(not SDDSIMAGEPROFILES.exists(), reason="sddsimageprofiles not built")
@pytest.mark.parametrize(
  "profile_type,expected",
  [
    ("x", [[1, 3], [2, 6], [3, 9]]),
    ("y", [[7, 1], [8, 2], [9, 3]]),
  ],
)
def test_profile_types(profile_type, expected, tmp_path):
  image = tmp_path / "image.sdds"
  output = tmp_path / "out.sdds"
  write_image(image)
  subprocess.run(
    [
      str(SDDSIMAGEPROFILES),
      str(image),
      str(output),
      f"-columnPrefix=I",
      f"-profileType={profile_type}",
    ],
    check=True,
  )
  data = read_sdds(output)
  assert data == expected

@pytest.mark.skipif(not SDDSIMAGEPROFILES.exists(), reason="sddsimageprofiles not built")
@pytest.mark.parametrize(
  "method,expected",
  [
    ("centerLine", [[1, 3], [2, 6], [3, 9]]),
    ("integrated", [[1, 6], [2, 15], [3, 24]]),
    ("averaged", [[1, 2], [2, 5], [3, 8]]),
    ("peak", [[1, 3], [2, 6], [3, 9]]),
  ],
)
def test_methods(method, expected, tmp_path):
  image = tmp_path / "image.sdds"
  output = tmp_path / "out.sdds"
  write_image(image)
  subprocess.run(
    [
      str(SDDSIMAGEPROFILES),
      str(image),
      str(output),
      "-columnPrefix=I",
      f"-method={method}",
    ],
    check=True,
  )
  data = read_sdds(output)
  assert data == expected

@pytest.mark.skipif(not SDDSIMAGEPROFILES.exists(), reason="sddsimageprofiles not built")
def test_background(tmp_path):
  image = tmp_path / "image.sdds"
  background = tmp_path / "background.sdds"
  output = tmp_path / "out.sdds"
  write_image(image)
  write_background(background)
  subprocess.run(
    [
      str(SDDSIMAGEPROFILES),
      str(image),
      str(output),
      "-columnPrefix=I",
      f"-background={background}",
    ],
    check=True,
  )
  data = read_sdds(output)
  assert data == [[1, 1], [2, 3], [3, 5]]

@pytest.mark.skipif(not SDDSIMAGEPROFILES.exists(), reason="sddsimageprofiles not built")
def test_area_of_interest(tmp_path):
  image = tmp_path / "image.sdds"
  output = tmp_path / "out.sdds"
  write_image(image)
  subprocess.run(
    [
      str(SDDSIMAGEPROFILES),
      str(image),
      str(output),
      "-columnPrefix=I",
      "-areaOfInterest=2,3,1,2",
    ],
    check=True,
  )
  data = read_sdds(output)
  assert data == [[2, 5], [3, 8]]

@pytest.mark.skipif(not SDDSIMAGEPROFILES.exists(), reason="sddsimageprofiles not built")
def test_pipe_output(tmp_path):
  image = tmp_path / "image.sdds"
  write_image(image)
  result = subprocess.run(
    [
      str(SDDSIMAGEPROFILES),
      str(image),
      "-pipe=out",
      "-columnPrefix=I",
    ],
    check=True,
    stdout=subprocess.PIPE,
  )
  data = read_sdds(result.stdout)
  assert data == [[1, 3], [2, 6], [3, 9]]
