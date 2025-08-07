import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSFFT = BIN_DIR / "sddsfft"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSQUERY = BIN_DIR / "sddsquery"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"

def extract_options():
  text = Path("SDDSaps/sddsfft.c").read_text()
  match = re.search(
    r"char\s*\*option\s*\[\s*N_OPTIONS\s*\]\s*=\s*\{([^}]*)\};",
    text,
    re.DOTALL,
  )
  if not match:
    return []
  return [opt.strip().strip('"') for opt in match.group(1).split(',') if opt.strip()]

OPTIONS = extract_options()

EXPECTED_OPTIONS = {
  "window",
  "normalize",
  "padwithzeroes",
  "truncate",
  "suppressaverage",
  "sampleinterval",
  "columns",
  "fulloutput",
  "pipe",
  "psdoutput",
  "exclude",
  "nowarnings",
  "complexinput",
  "inverse",
  "majorOrder",
}

def test_option_list():
  assert set(OPTIONS) == EXPECTED_OPTIONS

BASIC_OPTIONS = [
  "-normalize",
  "-window=welch,correct",
  "-padwithzeroes=2",
  "-truncate",
  "-suppressaverage",
  "-sampleInterval=2",
  "-nowarnings",
  "-majorOrder=column",
]

@pytest.mark.skipif(not (SDDSFFT.exists() and SDDSCHECK.exists()), reason="tools not built")
@pytest.mark.parametrize("extra", BASIC_OPTIONS)
def test_basic_options(tmp_path, extra):
  output = tmp_path / "out.sdds"
  subprocess.run(
    [
      str(SDDSFFT),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=longCol,floatCol",
      extra,
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"

@pytest.mark.skipif(not (SDDSFFT.exists() and SDDSCHECK.exists() and SDDSQUERY.exists()), reason="tools not built")
def test_fulloutput(tmp_path):
  output = tmp_path / "full.sdds"
  subprocess.run(
    [
      str(SDDSFFT),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=longCol,floatCol",
      "-fullOutput=unfolded,unwrapLimit=0.1",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  cols = subprocess.run(
    [str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True
  ).stdout
  assert "RealFFTfloatCol" in cols
  assert "ImagFFTfloatCol" in cols
  assert "ArgFFTfloatCol" in cols

@pytest.mark.skipif(not (SDDSFFT.exists() and SDDSCHECK.exists() and SDDSQUERY.exists()), reason="tools not built")
def test_psdoutput(tmp_path):
  output = tmp_path / "psd.sdds"
  subprocess.run(
    [
      str(SDDSFFT),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=longCol,floatCol",
      "-psdOutput",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  cols = subprocess.run(
    [str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True
  ).stdout
  assert "PSDfloatCol" in cols

@pytest.mark.skipif(not (SDDSFFT.exists() and SDDSCHECK.exists() and SDDSQUERY.exists()), reason="tools not built")
def test_exclude(tmp_path):
  output = tmp_path / "excl.sdds"
  subprocess.run(
    [
      str(SDDSFFT),
      "SDDSlib/demo/example.sdds",
      str(output),
      "-columns=longCol,shortCol,floatCol",
      "-exclude=shortCol",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  cols = subprocess.run(
    [str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True
  ).stdout
  assert "FFTfloatCol" in cols
  assert "FFTshortCol" not in cols

@pytest.mark.skipif(not (SDDSFFT.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_pipe():
  proc = subprocess.Popen(
    [
      str(SDDSFFT),
      "SDDSlib/demo/example.sdds",
      "-columns=longCol,floatCol",
      "-pipe=out",
    ],
    stdout=subprocess.PIPE,
  )
  result = subprocess.run(
    [str(SDDSQUERY), "-pipe", "-columnlist"], stdin=proc.stdout, capture_output=True, text=True
  )
  proc.stdout.close()
  proc.wait()
  assert "FFTfloatCol" in result.stdout

@pytest.mark.skipif(not (SDDSFFT.exists() and SDDSCHECK.exists() and PLAINDATA2SDDS.exists()), reason="tools not built")
def test_complex_input(tmp_path):
  plain = tmp_path / "c.txt"
  plain.write_text("0 1 0\n1 0 1\n2 -1 0\n3 0 -1\n")
  complex_in = tmp_path / "c.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(complex_in),
      "-column=t,double",
      "-column=RealSignal,double",
      "-column=ImagSignal,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
    ],
    check=True,
  )
  output = tmp_path / "cfft.sdds"
  subprocess.run(
    [
      str(SDDSFFT),
      str(complex_in),
      str(output),
      "-columns=t,Signal",
      "-complexInput",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"

@pytest.mark.skipif(not (SDDSFFT.exists() and SDDSCHECK.exists() and PLAINDATA2SDDS.exists()), reason="tools not built")
def test_inverse(tmp_path):
  plain = tmp_path / "ci.txt"
  plain.write_text("0 1 0\n1 0 1\n2 -1 0\n3 0 -1\n")
  complex_in = tmp_path / "ci.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(plain),
      str(complex_in),
      "-column=t,double",
      "-column=RealSignal,double",
      "-column=ImagSignal,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
    ],
    check=True,
  )
  output = tmp_path / "inv.sdds"
  subprocess.run(
    [
      str(SDDSFFT),
      str(complex_in),
      str(output),
      "-columns=t,Signal",
      "-complexInput",
      "-inverse",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
