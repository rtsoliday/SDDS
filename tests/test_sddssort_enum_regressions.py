"""Regression coverage for enum sort read failures and parameter requests."""
import gzip
import os
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


MAPPING = "0:OFF,10:ON,-1:FAULT"
pytestmark = pytest.mark.skipif(
    not all((BIN_DIR / tool).exists() for tool in ("sddssort", "sdds2stream")),
    reason="tools not built")


def run(tool, *args, input=None, check=True):
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)], input=input,
                          capture_output=True, text=True,
                          env={**os.environ, "MALLOC_PERTURB_": "165"})
  if check:
    assert result.returncode == 0, result.stderr
  return result


def invalid_later_page():
  return ('SDDS6\n'
          '&parameter name=first, type=long &end\n'
          f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
          '&data mode=ascii &end\n'
          '3\n1\n0\n'
          '2\n1\n123\n'
          '1\n1\n10\n')


@pytest.mark.parametrize("sort_option", ["-column=state", "-parameter=first"])
@pytest.mark.parametrize("input_mode", ["file", "pipe", "gzip"])
def test_sort_reports_invalid_enum_on_later_page(tmp_path, sort_option, input_mode):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  data = invalid_later_page()
  if input_mode == "pipe":
    result = run("sddssort", output, sort_option, "-pipe=input", input=data,
                 check=False)
  else:
    if input_mode == "gzip":
      source = source.with_suffix(".sdds.gz")
      source.write_bytes(gzip.compress(data.encode()))
    else:
      source.write_text(data, encoding="utf8")
    result = run("sddssort", source, output, sort_option, check=False)
  assert result.returncode != 0, "a page read error must not be treated as EOF"
  assert "enum" in result.stderr.lower()
  assert "Problem sorting data" in result.stderr


@pytest.mark.parametrize("sort_option", ["-column=state", "-parameter=first"])
def test_sort_preserves_input_after_later_page_read_error(tmp_path, sort_option):
  source = tmp_path / "source.sdds"
  original = invalid_later_page().encode()
  source.write_bytes(original)
  result = run("sddssort", source, sort_option, check=False)
  assert result.returncode != 0
  assert source.read_bytes() == original


@pytest.mark.parametrize("sort_rows", [False, True])
@pytest.mark.parametrize("direction", ["increasing", "decreasing"])
@pytest.mark.parametrize("input_mode", ["file", "pipe"])
def test_enum_second_parameter_request_is_initialized(tmp_path, sort_rows, direction,
                                                     input_mode):
  source, output = tmp_path / "source.sdds", tmp_path / "sorted.sdds"
  data = ('SDDS6\n'
          '&parameter name=first, type=long &end\n'
          f'&parameter name=state, type=enum, enum_values="{MAPPING}" &end\n'
          '&column name=x, type=long &end\n'
          '&data mode=ascii &end\n' +
          ''.join(f'5\n{state}\n1\n{x}\n'
                  for state, x in ((0, 10), (10, 20), (-1, 30))))
  options = ["-parameter=first"]
  if sort_rows:
    # Interleave the allocations so the second parameter request grows an
    # existing allocation after another request has been allocated.
    options.append("-column=x")
  options.append(f"-parameter=state,{direction}")
  if input_mode == "pipe":
    run("sddssort", output, *options, "-pipe=input", input=data)
  else:
    source.write_text(data, encoding="utf8")
    run("sddssort", source, output, *options)
  states = run("sdds2stream", output, "-parameter=state", "-noquotes").stdout.split()
  assert states == (["FAULT", "OFF", "ON"] if direction == "increasing" else
                    ["ON", "OFF", "FAULT"])
  rows = run("sdds2stream", output, "-column=x").stdout.split()
  assert rows == (["30", "10", "20"] if direction == "increasing" else
                  ["20", "10", "30"])
