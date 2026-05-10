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


def run_json_sddsplot(tmp_path, output_name, extra_args):
  out = tmp_path / output_name
  subprocess.run(
    [str(SDDSPLOT), str(EXAMPLE), *extra_args, "-device=json", f"-output={out}"],
    check=True,
  )
  assert out.exists()
  return json.loads(out.read_text())


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_json_output(tmp_path):
  doc = run_json_sddsplot(tmp_path, "plot.json", ["-columnnames=shortCol,doubleCol"])
  assert doc.get("schemaVersion") == "sddsplot-json-1"
  assert isinstance(doc.get("plots"), list)
  assert len(doc["plots"]) > 0


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_json_parameter_plot(tmp_path):
  doc = run_json_sddsplot(tmp_path, "params.json", ["-parameterNames=shortParam,doubleParam"])
  assert doc.get("schemaVersion") == "sddsplot-json-1"
  assert len(doc["plots"]) >= 1


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_json_array_plot(tmp_path):
  doc = run_json_sddsplot(tmp_path, "arrays.json", ["-arrayNames=shortArray,ushortArray"])
  assert doc.get("schemaVersion") == "sddsplot-json-1"
  assert len(doc["plots"]) >= 1


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


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_page_range_options(tmp_path):
  run_png_sddsplot(
    tmp_path,
    "pagefilter",
    [
      "-fromPage=2",
      "-toPage=2",
    ],
  )


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_separate_and_mode_options(tmp_path):
  run_png_sddsplot(
    tmp_path,
    "separate",
    [
      "-separate=page",
      "-mode=y=log",
      "-sameScale=y",
    ],
    device_args=["onwhite"],
  )


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_layout_and_tick_options(tmp_path):
  pngs = run_png_sddsplot(
    tmp_path,
    "layout",
    [
      "-columnnames=shortCol,longCol",
      "-layout=2,1,limitPerPage=2",
      "-tickSettings=xgrid,ygrid",
      "-subTickSettings=xdivisions=2,ydivisions=2",
      "-axes=x,y",
      "-split=pages",
      "-separate=page",
    ],
    device_args=["onwhite"],
  )
  assert len(pngs) >= 1


@pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")
def test_sddsplot_wildcard_exclude_and_scaling_options(tmp_path):
  pngs = run_png_sddsplot(
    tmp_path,
    "exclude",
    [
      "-columnnames=shortCol,*Col",
      "-yExclude=stringCol,charCol",
      "-factor=xMultiplier=2,yMultiplier=0.5",
      "-offset=xChange=1,yChange=-1",
      "-zoom=xFactor=1.1,yFactor=1.1",
      "-legend=specified=demo",
    ],
    device_args=["onwhite", "dashes"],
  )
  assert len(pngs) >= 1
