import math
import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSANALYTICSIGNAL = BIN_DIR / "sddsanalyticsignal"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"


def extract_options():
  text = Path("SDDSaps/sddsanalyticsignal.c").read_text()
  match = re.search(
    r"char\s*\*option\s*\[\s*N_OPTIONS\s*\]\s*=\s*\{([^}]*)\};",
    text,
    re.DOTALL,
  )
  if not match:
    return []
  return [opt.strip().strip('"') for opt in match.group(1).split(',') if opt.strip()]


OPTIONS = extract_options()
EXPECTED_OPTIONS = {"columns", "majorOrder", "pipe", "unwrapLimit"}


@pytest.mark.skipif(not (SDDSANALYTICSIGNAL.exists()), reason="tools not built")
def test_option_list():
  assert set(OPTIONS) == EXPECTED_OPTIONS


def create_data(tmp_path):
  N = 100
  data_txt = tmp_path / "data.txt"
  lines = [f"{N}\n"]
  for i in range(N):
    t = i * 0.1
    x = math.sin(t)
    lines.append(f"{t} {x}\n")
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
    ],
    check=True,
  )
  return sdds


@pytest.mark.skipif(
  not (
    SDDSANALYTICSIGNAL.exists()
    and SDDSCHECK.exists()
    and SDDSQUERY.exists()
    and SDDS2PLAINDATA.exists()
    and PLAINDATA2SDDS.exists()
  ),
  reason="tools not built",
)
def test_columns_and_output(tmp_path):
  sdds = create_data(tmp_path)
  out = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSANALYTICSIGNAL), str(sdds), str(out), "-columns=t,x"],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(out)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"
  cols = subprocess.run(
    [str(SDDSQUERY), str(out), "-columnlist"], capture_output=True, text=True
  ).stdout
  for name in ["Realx", "Imagx", "Magx", "Argx"]:
    assert name in cols
  orig = subprocess.run(
    [str(SDDS2PLAINDATA), str(sdds), "/dev/stdout", "-column=x", "-noRowCount"],
    capture_output=True,
    text=True,
  ).stdout.split()
  real = subprocess.run(
    [str(SDDS2PLAINDATA), str(out), "/dev/stdout", "-column=Realx", "-noRowCount"],
    capture_output=True,
    text=True,
  ).stdout.split()
  assert pytest.approx([float(v) for v in orig]) == [float(v) for v in real]


@pytest.mark.skipif(
  not (
    SDDSANALYTICSIGNAL.exists()
    and SDDSCHECK.exists()
    and SDDSQUERY.exists()
    and PLAINDATA2SDDS.exists()
  ),
  reason="tools not built",
)
def test_major_order(tmp_path):
  sdds = create_data(tmp_path)
  out = tmp_path / "mo.sdds"
  subprocess.run(
    [
      str(SDDSANALYTICSIGNAL),
      str(sdds),
      str(out),
      "-columns=t,x",
      "-majorOrder=column",
    ],
    check=True,
  )
  result = subprocess.run([str(SDDSCHECK), str(out)], capture_output=True, text=True)
  assert result.stdout.strip() == "ok"


@pytest.mark.skipif(
  not (
    SDDSANALYTICSIGNAL.exists()
    and SDDSCHECK.exists()
    and SDDSQUERY.exists()
    and SDDS2PLAINDATA.exists()
    and PLAINDATA2SDDS.exists()
  ),
  reason="tools not built",
)
def test_unwrap_limit(tmp_path):
  sdds = create_data(tmp_path)
  out_wrap = tmp_path / "wrap.sdds"
  subprocess.run(
    [
      str(SDDSANALYTICSIGNAL),
      str(sdds),
      str(out_wrap),
      "-columns=t,x",
      "-unwrapLimit=2",
    ],
    check=True,
  )
  out_unwrap = tmp_path / "unwrap.sdds"
  subprocess.run(
    [
      str(SDDSANALYTICSIGNAL),
      str(sdds),
      str(out_unwrap),
      "-columns=t,x",
      "-unwrapLimit=0",
    ],
    check=True,
  )
  wrapped = subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(out_wrap),
      "/dev/stdout",
      "-column=UnwrappedArgx",
      "-noRowCount",
    ],
    capture_output=True,
    text=True,
  ).stdout.split()
  unwrapped = subprocess.run(
    [
      str(SDDS2PLAINDATA),
      str(out_unwrap),
      "/dev/stdout",
      "-column=UnwrappedArgx",
      "-noRowCount",
    ],
    capture_output=True,
    text=True,
  ).stdout.split()
  wrapped_vals = [float(v) for v in wrapped]
  unwrapped_vals = [float(v) for v in unwrapped]
  assert max(wrapped_vals) <= 180
  assert max(unwrapped_vals) > 180


@pytest.mark.skipif(
  not (
    SDDSANALYTICSIGNAL.exists()
    and SDDSQUERY.exists()
    and PLAINDATA2SDDS.exists()
  ),
  reason="tools not built",
)
def test_pipe(tmp_path):
  sdds = create_data(tmp_path)
  proc = subprocess.Popen(
    [
      str(SDDSANALYTICSIGNAL),
      str(sdds),
      "-columns=t,x",
      "-pipe=out",
    ],
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
  assert "Realx" in result.stdout
