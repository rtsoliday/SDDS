import subprocess
from pathlib import Path
import hashlib
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSCONVERT = BIN_DIR / "sddsconvert"

@pytest.mark.skipif(not (SDDSCHECK.exists() and SDDSCONVERT.exists()), reason="tools not built")
def test_sddsconvert(tmp_path):
  binary_file = tmp_path / "binary.sdds"
  ascii_file = tmp_path / "ascii.sdds"

  subprocess.run([str(SDDSCONVERT), "SDDSlib/demo/example.sdds", str(binary_file), "-binary"], check=True)
  result = subprocess.run([str(SDDSCHECK), str(binary_file)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"

  subprocess.run([str(SDDSCONVERT), str(binary_file), str(ascii_file), "-ascii"], check=True)
  result = subprocess.run([str(SDDSCHECK), str(ascii_file)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not (SDDSCHECK.exists() and SDDSCONVERT.exists()), reason="tools not built")
@pytest.mark.parametrize(
  "option, expected, extension",
  [
    ("-binary", "ac679ee24199ba7f5d61f8bad3d05ce1d093d44b78c9ec60f0adf7ffd50c0db7", "sdds"),
    ("-ascii", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
    ("-delete=column,shortCol", "dc3302039dec1b6883bc69efb8c1fadfb8ef27b671e612c610f76a6bf28c6a41", "sdds"),
    ("-retain=column,shortCol", "a6f6ed125314ea6e6f7302f360848bebdb65ccb8d397b5052f592cf8ca8599db", "sdds"),
    ("-rename=column,shortCol=shortCol2", "71fcb900a567cf0ee83550df3e0d679ac41123725d96a9de93d3a687e53c9825", "sdds"),
    ("-description=test,contents", "2f39ea37ba4a9235ab34001723c50db8eb1b388d0c1d6f1f29acceb5cd0421f0", "sdds"),
    ("-table=1", "d3fa264796df7f0b431ea4f46bdb9a89a08d10bcfbd6c1e6c58b4dda8b154d1f", "sdds"),
    ("-editnames=column,*Col,%g/Col/New/", "2c6bb35829e95688ba515e86c1e342ecdd1af639cade30cc3094526344906c63", "sdds"),
    ("-linesperrow=1", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
    ("-nowarnings", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
    ("-recover", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
    ("-pipe=output", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
    ("-fromPage=1", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
    ("-toPage=1", "d3fa264796df7f0b431ea4f46bdb9a89a08d10bcfbd6c1e6c58b4dda8b154d1f", "sdds"),
    ("-acceptAllNames", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
    ("-removePages=1", "2d4e6ca23ec22deb07204209c7015b8ff05b4666e93b769410974005aa4cafe7", "sdds"),
    ("-keepPages=1", "d3fa264796df7f0b431ea4f46bdb9a89a08d10bcfbd6c1e6c58b4dda8b154d1f", "sdds"),
    ("-rowlimit=1", "b9a8876206940b40825e9bd900e00b4453d295bcac4021bb031ca06f174e11b9", "sdds"),
    ("-majorOrder=column", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
    ("-convertUnits=column,doubleCol,m", "b56f9ac3cc769dc45947a6103b4c0edad7acd021f7c4219746f60049947609a9", "sdds"),
    ("-xzLevel=0", "eac5eda07d13fe37c0467828d3d34de5239d27ef8901642adc45b33509e2486b", "sdds.xz"),
  ],
)
def test_sddsconvert_options(tmp_path, option, expected, extension):
  output = tmp_path / f"out.{extension}"
  cmd = [str(SDDSCONVERT), "SDDSlib/demo/example.sdds"]
  if option.startswith("-pipe"):
    cmd.append(option)
    with output.open("wb") as f:
      subprocess.run(cmd, stdout=f, check=True)
  else:
    cmd += [str(output), option]
    subprocess.run(cmd, check=True)
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  data = output.read_bytes()
  assert hashlib.sha256(data).hexdigest() == expected
