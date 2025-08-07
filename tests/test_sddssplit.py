import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSPLIT = (BIN_DIR / "sddssplit").resolve()
SDDSMAKEDATASET = (BIN_DIR / "sddsmakedataset").resolve()

missing = [p for p in (SDDSSPLIT, SDDSMAKEDATASET) if not p.exists()]
pytestmark = pytest.mark.skipif(missing, reason="required binaries not built")

def create_source(tmp_path: Path) -> Path:
  input_file = tmp_path / "input.sdds"
  cmds = [
    [
      str(SDDSMAKEDATASET),
      str(input_file),
      "-parameter=groupParam,type=string",
      "-data=A",
      "-parameter=nameParam,type=string",
      "-data=file1",
      "-column=x,type=long",
      "-data=1",
    ],
    [
      str(SDDSMAKEDATASET),
      str(input_file),
      "-append",
      "-parameter=groupParam,type=string",
      "-data=A",
      "-parameter=nameParam,type=string",
      "-data=file2",
      "-column=x,type=long",
      "-data=2",
    ],
    [
      str(SDDSMAKEDATASET),
      str(input_file),
      "-append",
      "-parameter=groupParam,type=string",
      "-data=B",
      "-parameter=nameParam,type=string",
      "-data=file3",
      "-column=x,type=long",
      "-data=3",
    ],
  ]
  for cmd in cmds:
    subprocess.run(cmd, cwd=tmp_path, check=True)
  return input_file


def read_data_line(path: Path) -> bytes:
  return path.read_bytes().split(b"&data", 1)[1][:100]


def test_sddssplit_default_naming(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run([str(SDDSSPLIT), str(input_file)], cwd=tmp_path, check=True)
  for idx in (1, 2, 3):
    assert (tmp_path / f"input{idx:03d}.sdds").exists()


@pytest.mark.parametrize("flag,mode", [("-ascii", b"ascii"), ("-binary", b"binary")])
def test_sddssplit_format(tmp_path, flag, mode):
  input_file = create_source(tmp_path)
  subprocess.run([str(SDDSSPLIT), str(input_file), flag], cwd=tmp_path, check=True)
  data_line = read_data_line(tmp_path / "input001.sdds")
  assert b"mode=" + mode in data_line


def test_sddssplit_digits(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run([str(SDDSSPLIT), str(input_file), "-digits=2"], cwd=tmp_path, check=True)
  assert (tmp_path / "input01.sdds").exists()


def test_sddssplit_rootname(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run([str(SDDSSPLIT), str(input_file), "-rootname=root"], cwd=tmp_path, check=True)
  assert (tmp_path / "root001.sdds").exists()


def test_sddssplit_first_last(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run(
    [str(SDDSSPLIT), str(input_file), "-firstPage=2", "-lastPage=2"],
    cwd=tmp_path,
    check=True,
  )
  assert (tmp_path / "input002.sdds").exists()
  assert not (tmp_path / "input001.sdds").exists()


def test_sddssplit_interval(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run([str(SDDSSPLIT), str(input_file), "-interval=2"], cwd=tmp_path, check=True)
  assert (tmp_path / "input001.sdds").exists()
  assert not (tmp_path / "input002.sdds").exists()
  assert (tmp_path / "input003.sdds").exists()


def test_sddssplit_extension(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run([str(SDDSSPLIT), str(input_file), "-extension=dat"], cwd=tmp_path, check=True)
  assert (tmp_path / "input001.dat").exists()


def test_sddssplit_name_parameter(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run(
    [str(SDDSSPLIT), str(input_file), "-nameParameter=nameParam"],
    cwd=tmp_path,
    check=True,
  )
  for name in ("file1", "file2", "file3"):
    assert (tmp_path / name).exists()


def test_sddssplit_offset(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run([str(SDDSSPLIT), str(input_file), "-offset=1"], cwd=tmp_path, check=True)
  assert (tmp_path / "input000.sdds").exists()


def test_sddssplit_major_order(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run([str(SDDSSPLIT), str(input_file), "-majorOrder=column"], cwd=tmp_path, check=True)
  data_line = read_data_line(tmp_path / "input001.sdds")
  assert b"column_major_order=1" in data_line


def test_sddssplit_group_parameter(tmp_path):
  input_file = create_source(tmp_path)
  subprocess.run(
    [
      str(SDDSSPLIT),
      str(input_file),
      "-ascii",
      "-rootname=group",
      "-groupParameter=groupParam",
    ],
    cwd=tmp_path,
    check=True,
  )
  first = (tmp_path / "group001.sdds").read_text()
  second = (tmp_path / "group003.sdds").read_text()
  assert first.count("! page number") == 2
  assert second.count("! page number") == 1


def test_sddssplit_pipe(tmp_path):
  input_file = create_source(tmp_path)
  data = input_file.read_bytes()
  subprocess.run(
    [str(SDDSSPLIT), "-pipe", "-rootname=pipe"],
    input=data,
    cwd=tmp_path,
    check=True,
  )
  assert (tmp_path / "pipe001.sdds").exists()
