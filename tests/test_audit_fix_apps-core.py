"""Regression tests for the 2026-10-22 SDDS6 audit fixes in the core toolkit apps."""

import os
import re
import subprocess
import time

import pytest

from sdds_test_utils import BIN_DIR


MAPPING = "0:OFF,10:ON,-1:FAULT"


def run(tool, *args, check=True, input=None):
  if not (BIN_DIR / tool).exists():
    pytest.skip(f"{tool} not built")
  env = dict(os.environ)
  env.pop("RPN_DEFNS", None)
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)], input=input,
                          capture_output=True, env=env, timeout=60)
  if check:
    assert result.returncode == 0, (result.stdout + result.stderr).decode(errors="replace")
  return result


def text(result):
  return result.stdout.decode()


def page_count(path):
  return int(text(run("sdds2stream", path, "-npages=bare")).strip())


def column(path, name, *extra):
  return text(run("sdds2stream", path, f"-column={name}", "-noquotes", *extra)).split()


@pytest.fixture
def truncated(tmp_path):
  """A four-page binary file whose last page is cut short."""
  page, intact, cut = tmp_path / "page.sdds", tmp_path / "intact.sdds", tmp_path / "trunc.sdds"
  run("sddsmakedataset", page, "-column=x,type=double", "-data=1,2,3", "-column=y,type=double",
      "-data=4,5,6", "-column=s,type=string", "-data=a,b,c", "-parameter=p,type=long", "-data=7")
  run("sddscombine", page, page, page, page, intact)
  cut.write_bytes(intact.read_bytes()[:-10])
  return cut


TRUNCATED_TOOLS = {
  "sddsconvert": ((), 3),
  "sddscast": (("-cast=column,x,double,float",), 3),
  "sddsprocess": (("-define=column,z,x 2 *",), 3),
  "sddsseparate": (("-group=xy,x,y",), 6),
  "sddscollect": (("-collect=suffix=x",), 9),
}


@pytest.mark.parametrize("tool", TRUNCATED_TOOLS)
def test_truncated_input_fails_but_terminates_output(tmp_path, truncated, tool):
  args, pages = TRUNCATED_TOOLS[tool]
  output = tmp_path / "out.sdds.gz"
  result = run(tool, truncated, output, *args, check=False)
  assert result.returncode != 0
  assert output.stat().st_size > 0
  assert page_count(output) == pages


@pytest.mark.parametrize("tool", ["sddsconvert", "sddscast", "sddsprocess"])
def test_truncated_in_place_input_is_kept_without_temp_files(tmp_path, truncated, tool):
  original = truncated.read_bytes()
  result = run(tool, truncated, *TRUNCATED_TOOLS[tool][0], check=False)
  assert result.returncode != 0
  assert truncated.read_bytes() == original
  assert sorted(p.name for p in tmp_path.iterdir()) == ["intact.sdds", "page.sdds", "trunc.sdds"]


def test_sddsconvert_recover_still_succeeds(tmp_path, truncated):
  output, clipped = tmp_path / "recovered.sdds", tmp_path / "clipped.sdds"
  run("sddsconvert", truncated, output, "-recover")
  assert page_count(output) == 4
  run("sddsconvert", truncated, clipped, "-recover=clip")
  assert page_count(clipped) == 3


def test_sddsquery_read_all_failure_terminates_sdds_output(tmp_path, truncated):
  output = tmp_path / "summary.sdds.gz"
  result = run("sddsquery", tmp_path / "page.sdds", truncated, "-readAll", f"-sddsOutput={output}",
               check=False)
  assert result.returncode != 0
  assert page_count(output) == 4


def test_sddsprintout_stops_after_to_page(tmp_path, truncated):
  result = run("sddsprintout", truncated, "-columns=x", "-toPage=3", "-noTitle")
  assert text(result).split().count("1.000000e+00") == 3
  result = run("sddsprintout", truncated, "-columns=x", "-noTitle", check=False)
  assert result.returncode != 0
  assert text(result).split().count("1.000000e+00") == 3


def test_sddsprintout_many_pages_is_fast(tmp_path):
  sequence = run("sddssequence", "-pipe=out", "-define=x", "-sequence=begin=1,end=20000,delta=1")
  broken = run("sddsbreak", "-pipe", "-rowlimit=1", input=sequence.stdout)
  source = tmp_path / "many.sdds"
  run("sddsprocess", "-pipe=in", source, "-process=x,first,xf", input=broken.stdout)
  start = time.monotonic()
  result = run("sddsprintout", source, "-columns=x", "-parameters=xf", "-noTitle")
  elapsed = time.monotonic() - start
  values = text(result).split()
  assert values.count("x") == 20000 and "2.000000e+04" in values
  # Allocating the enum-sized buffer per page took ~2.7 s here; the fix takes ~0.03 s.
  assert elapsed < 1.5


def test_sddsprintout_long_enum_label(tmp_path):
  source = tmp_path / "long.sdds"
  label = "L" * 5000
  run("sddsmakedataset", source, "-column=state,type=enum", "-data=0,1",
      f"-enumValues=column,state,0:OFF,1:{label}", "-parameter=ps,type=enum", "-data=1",
      f"-enumValues=parameter,ps,0:OFF,1:{label}")
  values = text(run("sddsprintout", source, "-columns=state", "-parameters=ps", "-noTitle")).split()
  assert values.count(label) == 2 and "OFF" in values


@pytest.fixture
def mixed(tmp_path):
  source = tmp_path / "mixed.sdds"
  run("sddsmakedataset", source, "-column=state,type=enum", "-data=0,10,-1",
      f"-enumValues=column,state,{MAPPING}", "-column=x,type=double", "-data=1.5,2.5,3.5",
      "-column=s,type=string", "-data=a,b,c")
  return source


def header(path):
  return path.read_bytes().split(b"&data", 1)[0].decode()


def declared_type(path, kind, name):
  match = re.search(rf'&{kind} name="?{name}"?, type="?(\w+)', header(path))
  return match and match.group(1)


def test_sddscast_wildcard_skips_enum_for_numeric_target(tmp_path, mixed):
  output = tmp_path / "out.sdds"
  result = run("sddscast", mixed, output, "-cast=column,*,*,float")
  assert b"state" in result.stderr
  assert declared_type(output, "column", "x") == "float"
  assert f'enum_values="{MAPPING}"' in header(output)
  assert column(output, "state") == ["OFF", "ON", "FAULT"]
  for cast in ("-cast=column,state,*,float", "-cast=column,*,enum,float"):
    result = run("sddscast", mixed, output, cast, check=False)
    assert result.returncode != 0
    assert b"enum conversions require" in result.stderr


def test_sddscast_string_target_requires_enum_source(tmp_path, mixed):
  output = tmp_path / "out.sdds"
  for cast in ("-cast=column,x,double,string", "-cast=column,x,*,string", "-cast=column,*,double,string"):
    result = run("sddscast", mixed, output, cast, check=False)
    assert result.returncode != 0, cast
    assert b"numeric type" in result.stderr
  run("sddscast", mixed, output, "-cast=column,*,*,string")
  assert declared_type(output, "column", "state") == "string"
  assert column(output, "state") == ["OFF", "ON", "FAULT"]
  assert [float(v) for v in column(output, "x")] == [1.5, 2.5, 3.5]


def test_sddsprocess_string_define_creates_empty_element(tmp_path, mixed):
  output = tmp_path / "out.sdds"
  run("sddsprocess", mixed, output, "-define=column,u,x 3 *", "-define=column,t,x 2 *,type=string",
      "-define=column,w,u 1 +", "-define=parameter,P,1 2 +,type=string")
  assert [float(v) for v in column(output, "u")] == [4.5, 7.5, 10.5]
  assert [float(v) for v in column(output, "w")] == [5.5, 8.5, 11.5]
  assert text(run("sdds2stream", output, "-column=t")).split() == ['""'] * 3
  assert text(run("sdds2stream", output, "-parameter=P")).split() == ['""']
  assert declared_type(output, "column", "t") == "string"
  assert declared_type(output, "parameter", "P") == "string"
  result = run("sddsprocess", mixed, output, "-define=column,e,x,type=enum", check=False)
  assert result.returncode != 0


@pytest.mark.parametrize("option", ["-enumValues", "-enum", "-ENUMVALUES", "-enum_values", "-e"])
@pytest.mark.parametrize("kind", ["column", "col"])
def test_enum_values_option_matches_like_other_options(tmp_path, option, kind):
  made, cast = tmp_path / "made.sdds", tmp_path / "cast.sdds"
  run("sddsmakedataset", made, "-column=state,type=enum", "-data=0,10",
      f"{option}={kind},state,0:OFF\\,X,10:ON")
  assert column(made, "state") == ["OFF,X", "ON"]
  run("sddscast", made, cast, "-cast=column,state,enum,enum", f"{option}={kind},state,7:OFF\\,X,8:ON")
  assert column(cast, "state") == ["OFF,X", "ON"]
  assert column(cast, "state", "-enumCodes") == ["7", "8"]


@pytest.mark.parametrize("args", [
    ("-column=state,type=long", "-data=0,10", "-enumValues=column,state,0:OFF,10:ON"),
    ("-column=state,type=long,enum_values=0:OFF", "-data=0,10"),
    ("-parameter=state,type=double", "-data=1", "-enumValues=parameter,state,1:ON"),
    ("-array=state,type=short", "-data=0,1", "-enumValues=array,state,0:OFF,1:ON"),
])
def test_sddsmakedataset_rejects_mapping_for_non_enum_target(tmp_path, args):
  result = run("sddsmakedataset", tmp_path / "out.sdds", *args, check=False)
  assert result.returncode != 0
  assert b"non-enum" in result.stderr
