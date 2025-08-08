import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDS2DFFT = BIN_DIR / "sdds2dfft"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSQUERY = BIN_DIR / "sddsquery"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

def extract_options():
  text = Path("SDDSaps/sdds2dfft.c").read_text()
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

def make_matrix(tmp_path):
  plain = tmp_path / "m.txt"
  plain.write_text("""0 0 1 2 3
1 1 2 3 4
2 2 3 4 5
3 3 4 5 6
""")
  out = tmp_path / "m.sdds"
  subprocess.run([
      str(PLAINDATA2SDDS),
      str(plain),
      str(out),
      "-column=t,double",
      "-column=Image1,double",
      "-column=Image2,double",
      "-column=Image3,double",
      "-column=Image4,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
    ], check=True)
  return out

def make_complex(tmp_path):
  plain = tmp_path / "c.txt"
  plain.write_text("""0 1 0 0 1 0 0 1 0
1 0 1 1 0 0 0 1 1
2 -1 0 0 -1 0 0 -1 0
3 0 -1 -1 0 0 0 -1 -1
""")
  out = tmp_path / "c.sdds"
  subprocess.run([
      str(PLAINDATA2SDDS),
      str(plain),
      str(out),
      "-column=t,double",
      "-column=RealSig1,double",
      "-column=ImagSig1,double",
      "-column=RealSig2,double",
      "-column=ImagSig2,double",
      "-column=RealSig3,double",
      "-column=ImagSig3,double",
      "-column=RealSig4,double",
      "-column=ImagSig4,double",
      "-inputMode=ascii",
      "-outputMode=ascii",
    ], check=True)
  return out


def test_option_list():
  assert set(OPTIONS) == EXPECTED_OPTIONS

BASIC_OPTIONS = [
  "-normalize",
  "-padWithZeroes=1",
  "-truncate",
  "-suppressAverage",
  "-nowarnings",
  "-majorOrder=column",
]

@pytest.mark.skipif(
  not (SDDS2DFFT.exists() and SDDSCHECK.exists()),
  reason="tools not built",
)
@pytest.mark.parametrize("extra", BASIC_OPTIONS)
def test_basic_options(tmp_path, extra):
  matrix = make_matrix(tmp_path)
  output = tmp_path / "out.sdds"
  subprocess.run([
      str(SDDS2DFFT),
      str(matrix),
      str(output),
      "-columns=t,Image*",
      extra,
    ], check=True, capture_output=True, text=True)
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"

@pytest.mark.skipif(
  not (SDDS2DFFT.exists() and SDDSCHECK.exists() and SDDSQUERY.exists()),
  reason="tools not built",
)
def test_fulloutput(tmp_path):
  matrix = make_matrix(tmp_path)
  output = tmp_path / "full.sdds"
  subprocess.run([
      str(SDDS2DFFT),
      str(matrix),
      str(output),
      "-columns=t,Image*",
      "-fullOutput=unfolded,unwrapLimit=0.1",
    ], check=True)
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  cols = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True).stdout
  assert "UnwrapArgFFTImage1" in cols

@pytest.mark.skipif(
  not (SDDS2DFFT.exists() and SDDSCHECK.exists() and SDDSQUERY.exists()),
  reason="tools not built",
)
def test_psdoutput(tmp_path):
  matrix = make_matrix(tmp_path)
  output = tmp_path / "psd.sdds"
  subprocess.run([
      str(SDDS2DFFT),
      str(matrix),
      str(output),
      "-columns=t,Image*",
      "-psdOutput",
    ], check=True)
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  cols = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True).stdout
  assert "PSDImage1" in cols

@pytest.mark.skipif(
  not (SDDS2DFFT.exists() and SDDSCHECK.exists() and SDDSQUERY.exists()),
  reason="tools not built",
)
def test_exclude(tmp_path):
  matrix = make_matrix(tmp_path)
  output = tmp_path / "excl.sdds"
  subprocess.run([
      str(SDDS2DFFT),
      str(matrix),
      str(output),
      "-columns=t,Image1,Image2,Image3,Image4",
      "-exclude=Image2",
    ], check=True)
  cols = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True).stdout
  assert "FFTImage2" not in cols and "FFTImage1" in cols

@pytest.mark.skipif(
  not (SDDS2DFFT.exists() and SDDSCHECK.exists() and SDDSQUERY.exists()),
  reason="tools not built",
)
def test_pipe(tmp_path):
  matrix = make_matrix(tmp_path)
  proc = subprocess.Popen([
      str(SDDS2DFFT),
      str(matrix),
      "-columns=t,Image*",
      "-pipe=out",
    ], stdout=subprocess.PIPE)
  result = subprocess.run([str(SDDSQUERY), "-pipe", "-columnlist"], stdin=proc.stdout, capture_output=True, text=True)
  proc.stdout.close()
  proc.wait()
  assert "FFTImage1" in result.stdout

@pytest.mark.skipif(
  not (SDDS2DFFT.exists() and SDDSCHECK.exists() and PLAINDATA2SDDS.exists()),
  reason="tools not built",
)
def test_complex_input(tmp_path):
  comp = make_complex(tmp_path)
  output = tmp_path / "cfft.sdds"
  subprocess.run([
      str(SDDS2DFFT),
      str(comp),
      str(output),
      "-columns=t,Sig*",
      "-complexInput",
    ], check=False)
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"

@pytest.mark.skipif(
  not (SDDS2DFFT.exists() and SDDSCHECK.exists() and PLAINDATA2SDDS.exists()),
  reason="tools not built",
)
def test_inverse(tmp_path):
  comp = make_complex(tmp_path)
  output = tmp_path / "inv.sdds"
  subprocess.run([
      str(SDDS2DFFT),
      str(comp),
      str(output),
      "-columns=t,Sig*",
      "-complexInput",
      "-inverse",
    ], check=False)
  result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"

@pytest.mark.skipif(
  not (SDDS2DFFT.exists() and SDDSCHECK.exists()),
  reason="tools not built",
)
def test_sample_interval(tmp_path):
  matrix = make_matrix(tmp_path)
  output = tmp_path / "sample.sdds"
  subprocess.run([
      str(SDDS2DFFT),
      str(matrix),
      str(output),
      "-columns=t,Image*",
      "-sampleInterval=2",
    ], check=True)
  plain = tmp_path / "plain.txt"
  subprocess.run([str(SDDS2PLAINDATA), str(output), str(plain), "-columns=f"], check=True)
  lines = plain.read_text().strip().splitlines()
  assert len(lines) == 3
