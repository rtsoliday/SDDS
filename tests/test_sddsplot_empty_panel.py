"""Empty panels must not turn unset scale sentinels into enormous axes."""
import json
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


@pytest.mark.parametrize("empty_first", [False, True])
@pytest.mark.parametrize("fixed_scales", [False, True])
def test_empty_panel_has_usable_scales(tmp_path, empty_first, fixed_scales):
  source = tmp_path / "empty-panel.sdds"
  pages = ["0\n", "4\n1 10\n2 20\n3 30\n4 40\n"]
  if not empty_first:
    pages.reverse()
  source.write_text(
    "SDDS1\n&column name=x type=double &end\n"
    "&column name=y type=double &end\n&data mode=ascii &end\n"
    + "".join(pages)
  )
  output = tmp_path / "plot.json"
  args = [str(BIN_DIR / "sddsplot"), str(source), "-column=x,y",
          "-split=page", "-sep", "-device=json", f"-output={output}"]
  if fixed_scales:
    args.append("-scales=0,5,-10,50")
  subprocess.run(args, check=True)
  plots = json.loads(output.read_text())["plots"]
  assert len(plots) == 2
  empty, populated = plots if empty_first else reversed(plots)
  assert empty["traces"][0]["y"] == []
  bounds = empty["layout"]["yaxis"]["range"]
  assert -100 < bounds[0] < bounds[1] < 100
  assert populated["traces"][0]["y"] == [10, 20, 30, 40]
  if fixed_scales:
    assert bounds == pytest.approx([-10, 50])
    assert populated["layout"]["yaxis"]["range"] == pytest.approx([-10, 50])
