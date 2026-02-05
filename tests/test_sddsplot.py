import json
import subprocess
from pathlib import Path
import pytest

ROOT_DIR = Path(__file__).resolve().parents[1]
BIN_DIR = ROOT_DIR / "bin/Linux-x86_64"
SDDSPLOT = BIN_DIR / "sddsplot"
EXAMPLE = ROOT_DIR / "SDDSlib/demo/example.sdds"


def run_png_sddsplot(tmp_path, rootname, extra_args=None, device_args=None):
  args = [
    str(SDDSPLOT),
    str(EXAMPLE),
    "-columnnames=shortCol,doubleCol",
  ]
  if extra_args:
    args.extend(extra_args)
  device_arg = f"-device=png,rootname={rootname}"
  if device_args:
    device_arg = f"{device_arg},{','.join(device_args)}"
  args.append(device_arg)
  subprocess.run(args, cwd=tmp_path, check=True)

  pngs = sorted(tmp_path.glob(f"{rootname}*"))
  if not pngs:
    pngs = sorted(tmp_path.iterdir())
  assert pngs, "No output files were generated"
  png_signature = b"\x89PNG\r\n\x1a\n"
  png_candidates = []
  for png_file in pngs:
    if not png_file.is_file():
      continue
    if png_file.stat().st_size <= 0:
      continue
    with png_file.open("rb") as handle:
      if handle.read(8) == png_signature:
        png_candidates.append(png_file)
  assert png_candidates, "No PNG output files were generated"
  return png_candidates


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_json_output(tmp_path):
  out = tmp_path / "plot.json"
  subprocess.run(
    [
      str(SDDSPLOT),
      str(EXAMPLE),
      "-columnnames=shortCol,doubleCol",
      "-device=json",
      f"-output={out}",
    ],
    check=True,
  )
  assert out.exists()
  doc = json.loads(out.read_text())
  assert doc.get("schemaVersion") == "sddsplot-json-1"
  assert isinstance(doc.get("plots"), list)
  assert len(doc["plots"]) > 0


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_png_basic(tmp_path):
  run_png_sddsplot(tmp_path, "basic")


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_png_common_options(tmp_path):
  run_png_sddsplot(
    tmp_path,
    "labels",
    [
      "-title=SDDS Plot",
      "-topline=Example",
      "-xlabel=shortCol",
      "-ylabel=doubleCol",
      "-legend=filename",
    ],
    device_args=["onwhite", "dashes"],
  )
