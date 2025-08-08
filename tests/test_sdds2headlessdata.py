import subprocess
from pathlib import Path
import struct
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDS2HEADLESSDATA = BIN_DIR / "sdds2headlessdata"
EXAMPLE = Path("SDDSlib/demo/example.sdds")

shorts = [1, 2, 3, 4, 5, 6, 7, 8]
longs = [100, 200, 300, 400, 500, 600, 700, 800]


def row_major_bytes():
  return b"".join(struct.pack("<h", s) + struct.pack("<i", l) for s, l in zip(shorts, longs))


def column_major_bytes():
  return (
    b"".join(struct.pack("<h", s) for s in shorts[:5])
    + b"".join(struct.pack("<i", l) for l in longs[:5])
    + b"".join(struct.pack("<h", s) for s in shorts[5:])
    + b"".join(struct.pack("<i", l) for l in longs[5:])
  )


@pytest.mark.skipif(not SDDS2HEADLESSDATA.exists(), reason="sdds2headlessdata not built")
def test_order_row_major(tmp_path):
  out = tmp_path / "row.bin"
  subprocess.run(
    [
      str(SDDS2HEADLESSDATA),
      str(EXAMPLE),
      str(out),
      "-column=shortCol,longCol",
      "-order=rowMajor",
    ],
    check=True,
  )
  assert out.read_bytes() == row_major_bytes()


@pytest.mark.skipif(not SDDS2HEADLESSDATA.exists(), reason="sdds2headlessdata not built")
def test_order_column_major(tmp_path):
  out = tmp_path / "col.bin"
  subprocess.run(
    [
      str(SDDS2HEADLESSDATA),
      str(EXAMPLE),
      str(out),
      "-column=shortCol,longCol",
      "-order=columnMajor",
    ],
    check=True,
  )
  assert out.read_bytes() == column_major_bytes()


@pytest.mark.skipif(not SDDS2HEADLESSDATA.exists(), reason="sdds2headlessdata not built")
def test_pipe_out():
  result = subprocess.run(
    [
      str(SDDS2HEADLESSDATA),
      str(EXAMPLE),
      "-column=shortCol,longCol",
      "-pipe=out",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  assert result.stdout == row_major_bytes()


@pytest.mark.skipif(not SDDS2HEADLESSDATA.exists(), reason="sdds2headlessdata not built")
def test_pipe_in(tmp_path):
  out = tmp_path / "pipein.bin"
  subprocess.run(
    [
      str(SDDS2HEADLESSDATA),
      str(out),
      "-column=shortCol,longCol",
      "-pipe=in",
    ],
    input=EXAMPLE.read_bytes(),
    check=True,
  )
  assert out.read_bytes() == row_major_bytes()
