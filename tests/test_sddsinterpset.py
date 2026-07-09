import re
import subprocess
from pathlib import Path
import pytest

from sdds_test_utils import BIN_DIR
SDDSINTERPSET = BIN_DIR / "sddsinterpset"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2STREAM = BIN_DIR / "sdds2stream"

REQUIRED = [SDDSINTERPSET, PLAINDATA2SDDS, SDDS2STREAM]

SOURCE = Path("SDDSaps/sddsinterpset.c")
text = SOURCE.read_text()
match = re.search(r"char \*option\[N_OPTIONS\] = \{(.*?)\};", text, re.S)
option_names = [name.strip().strip('"') for name in match.group(1).split(',') if name.strip()]

# Base option strings for reuse
base_data = "-data=fileColumn=dataFile,interpolate=y,functionOf=x,atValue=1.5"
below_data = "-data=fileColumn=dataFile,interpolate=y,functionOf=x,atValue=-1"
above_data = "-data=fileColumn=dataFile,interpolate=y,functionOf=x,atValue=3"

option_args = {
  "order": ["-order=2", base_data],
  "pipe": ["-pipe=out", base_data],
  "belowrange": ["-belowRange=value=999", below_data],
  "aboverange": ["-aboveRange=value=888", above_data],
  "data": [base_data],
  "verbose": ["-verbose", base_data],
  "majorOrder": ["-majorOrder=column", base_data],
  "threads": ["-threads=2", base_data],
}

missing = set(option_names) - option_args.keys()
assert not missing, f"Missing argument mapping for options: {missing}"


def create_input(tmp_path):
  plain = tmp_path / "data.txt"
  plain.write_text("0 0\n1 1\n2 4\n")
  data = tmp_path / "data.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(plain),
    str(data),
    "-column=x,double",
    "-column=y,double",
    "-inputMode=ascii",
    "-outputMode=binary",
    "-noRowCount",
  ], check=True)
  meta_plain = tmp_path / "meta.txt"
  meta_plain.write_text(f"{data}\n")
  meta = tmp_path / "meta.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(meta_plain),
    str(meta),
    "-column=dataFile,string",
    "-inputMode=ascii",
    "-outputMode=binary",
    "-noRowCount",
  ], check=True)
  return meta


def create_threaded_input(tmp_path):
  data_files = []
  for name, rows in (
      ("data1", "0 0 0 100\n1 1 1 200\n2 4 2 300\n"),
      ("data2", "0 0 0 1000\n1 10 1 2000\n2 20 2 3000\n"),
  ):
    plain = tmp_path / f"{name}.txt"
    plain.write_text(rows)
    data = tmp_path / f"{name}.sdds"
    subprocess.run([
      str(PLAINDATA2SDDS),
      str(plain),
      str(data),
      "-column=x,double",
      "-column=y,double",
      "-column=u,double",
      "-column=z,double",
      "-inputMode=ascii",
      "-outputMode=binary",
      "-noRowCount",
    ], check=True)
    data_files.append(data)

  meta_plain = tmp_path / "threaded_meta.txt"
  meta_plain.write_text("".join(f"{path}\n" for path in data_files))
  meta = tmp_path / "threaded_meta.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(meta_plain),
    str(meta),
    "-column=dataFile,string",
    "-inputMode=ascii",
    "-outputMode=binary",
    "-noRowCount",
  ], check=True)
  return meta


def read_column(path, column):
  result = subprocess.run([
    str(SDDS2STREAM),
    f"-column={column}",
    str(path),
  ], capture_output=True, text=True, check=True)
  return [float(x) for x in result.stdout.strip().split()]


@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="required tools not built")
@pytest.mark.parametrize("name", option_names)
def test_sddsinterpset_flags(tmp_path, name):
  meta = create_input(tmp_path)
  args = option_args[name]
  if name == "pipe":
    proc = subprocess.Popen([str(SDDSINTERPSET), str(meta), *args], stdout=subprocess.PIPE)
    result = subprocess.run([
      str(SDDS2STREAM),
      "-pipe",
      "-column=y",
    ], stdin=proc.stdout, capture_output=True, text=True, check=True)
    proc.wait()
    values = [float(x) for x in result.stdout.strip().split()]
    assert values == pytest.approx([2.5])
  else:
    out = tmp_path / "out.sdds"
    subprocess.run([str(SDDSINTERPSET), str(meta), str(out), *args], check=True)
    if name == "order":
      assert read_column(out, "y") == pytest.approx([2.25])
    elif name == "belowrange":
      assert read_column(out, "y") == pytest.approx([999])
    elif name == "aboverange":
      assert read_column(out, "y") == pytest.approx([888])
    elif name == "majorOrder":
      strings_out = subprocess.run(["strings", str(out)], capture_output=True, text=True, check=True).stdout
      assert "column_major_order=1" in strings_out
    else:
      assert read_column(out, "y") == pytest.approx([2.5])


@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="required tools not built")
def test_threads_match_serial_output(tmp_path):
  meta = create_threaded_input(tmp_path)
  serial = tmp_path / "serial.sdds"
  threaded = tmp_path / "threaded.sdds"
  args = [base_data]
  subprocess.run([str(SDDSINTERPSET), str(meta), str(serial), *args, "-threads=1"], check=True)
  subprocess.run([str(SDDSINTERPSET), str(meta), str(threaded), *args, "-threads=2"], check=True)
  assert read_column(threaded, "y") == pytest.approx(read_column(serial, "y"))
  assert read_column(threaded, "y") == pytest.approx([2.5, 15])


@pytest.mark.skipif(not all(x.exists() for x in REQUIRED), reason="required tools not built")
def test_multiple_data_options_keep_independent_results(tmp_path):
  meta = create_threaded_input(tmp_path)
  output = tmp_path / "multiple.sdds"
  subprocess.run([
    str(SDDSINTERPSET),
    str(meta),
    str(output),
    base_data,
    "-data=fileColumn=dataFile,interpolate=z,functionOf=u,atValue=1.5",
    "-threads=2",
  ], check=True)
  assert read_column(output, "y") == pytest.approx([2.5, 15])
  assert read_column(output, "z") == pytest.approx([250, 2500])
