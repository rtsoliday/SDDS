import math
import random
import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSNAFF = BIN_DIR / "sddsnaff"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"


def extract_options():
  text = Path("SDDSaps/sddsnaff.c").read_text()
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
  "truncate",
  "columns",
  "exclude",
  "pipe",
  "nowarnings",
  "terminatesearch",
  "iteratefrequency",
  "pair",
  "majorOrder",
}


def create_data(tmp_path):
  N = 100
  data_txt = tmp_path / "data.txt"
  random.seed(0)
  lines = [f"{N}\n"]
  for i in range(N):
    t = float(i)
    x = math.sin(2 * math.pi * 0.05 * t)
    y = math.sin(2 * math.pi * 0.1 * t) + 0.1 * (random.random() - 0.5) * 2
    z = math.cos(2 * math.pi * 0.07 * t)
    lines.append(f"{t} {x} {y} {z}\n")
  data_txt.write_text("".join(lines))
  sdds = tmp_path / "data.sdds"
  subprocess.run(
    [
      str(PLAINDATA2SDDS),
      str(data_txt),
      str(sdds),
      "-inputMode=ascii",
      "-separator= ",
      "-column=t,double",
      "-column=x,double",
      "-column=y,double",
      "-column=z,double",
    ],
    check=True,
  )
  return sdds


@pytest.mark.skipif(not (SDDSNAFF.exists() and SDDSCHECK.exists() and PLAINDATA2SDDS.exists()), reason="tools not built")
def test_option_list():
  assert set(OPTIONS) == EXPECTED_OPTIONS


BASIC_OPTIONS = [
  "-truncate",
  "-terminateSearch=frequencies=2",
  "-noWarnings",
  "-iterateFrequency=cycleLimit=10,accuracyLimit=0.01",
  "-majorOrder=column",
]


@pytest.mark.skipif(
  not (SDDSNAFF.exists() and SDDSCHECK.exists() and PLAINDATA2SDDS.exists()),
  reason="tools not built",
)
@pytest.mark.parametrize("extra", BASIC_OPTIONS)
def test_basic_options(tmp_path, extra):
  sdds = create_data(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSNAFF), str(sdds), str(out), "-columns=t,x", extra],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(out)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"


@pytest.mark.skipif(
  not (SDDSNAFF.exists() and SDDSCHECK.exists() and SDDSQUERY.exists() and PLAINDATA2SDDS.exists()),
  reason="tools not built",
)
def test_exclude(tmp_path):
  sdds = create_data(tmp_path)
  out = tmp_path / "excl.sdds"
  subprocess.run(
    [
      str(SDDSNAFF),
      str(sdds),
      str(out),
      "-columns=t,*",
      "-exclude=y",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(out)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  cols = subprocess.run(
    [str(SDDSQUERY), str(out), "-columnlist"], capture_output=True, text=True
  ).stdout
  assert "yFrequency" not in cols
  assert "xFrequency" in cols


@pytest.mark.skipif(
  not (SDDSNAFF.exists() and SDDSQUERY.exists() and PLAINDATA2SDDS.exists()),
  reason="tools not built",
)
def test_pipe(tmp_path):
  sdds = create_data(tmp_path)
  proc = subprocess.Popen(
    [str(SDDSNAFF), str(sdds), "-columns=t,x", "-pipe=out"],
    stdout=subprocess.PIPE,
  )
  result = subprocess.run(
    [str(SDDSQUERY), "-columnlist", "-pipe"],
    stdin=proc.stdout,
    capture_output=True,
    text=True,
  )
  proc.stdout.close()
  proc.wait()
  assert "xFrequency" in result.stdout


@pytest.mark.skipif(
  not (SDDSNAFF.exists() and SDDSCHECK.exists() and SDDSQUERY.exists() and PLAINDATA2SDDS.exists()),
  reason="tools not built",
)
def test_pair(tmp_path):
  sdds = create_data(tmp_path)
  out = tmp_path / "pair.sdds"
  subprocess.run(
    [
      str(SDDSNAFF),
      str(sdds),
      str(out),
      "-columns=t",
      "-pair=x,y",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(out)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  cols = subprocess.run(
    [str(SDDSQUERY), str(out), "-columnlist"], capture_output=True, text=True
  ).stdout
  assert "xFrequency" in cols
  assert "yAmplitude" in cols


@pytest.mark.skipif(
  not (SDDSNAFF.exists() and SDDS2PLAINDATA.exists() and PLAINDATA2SDDS.exists()),
  reason="tools not built",
)
def test_noise_analysis(tmp_path):
  sdds = create_data(tmp_path)
  out = tmp_path / "noise.sdds"
  subprocess.run(
    [
      str(SDDSNAFF),
      str(sdds),
      str(out),
      "-columns=t,y",
      "-terminateSearch=frequencies=1",
    ],
    check=True,
  )
  ascii_out = tmp_path / "noise.txt"
  subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(out),
      str(ascii_out),
      "-column=yFrequency",
      "-column=ySignificance",
      "-outputMode=ascii",
      "-separator= ",
      "-noRowCount",
    ],
    check=True,
  )
  vals = ascii_out.read_text().strip().split()
  freq = float(vals[0])
  signif = float(vals[1])
  assert abs(freq - 0.1) < 0.01
  assert signif < 0.05
