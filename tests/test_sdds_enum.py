"""Native SDDS6 enums: format failures, toolkit semantics and legacy rejection."""
import json
import platform
import subprocess
import tempfile
from pathlib import Path

import pytest

from sdds_test_utils import BIN_DIR, PLATFORM_ID, ROOT_DIR


FIXTURES = ROOT_DIR / "SDDSlib/testdata/enums"
MAPPING = "0:OFF,10:ON,-1:FAULT"


def run(tool, *args, check=True):
  command = [str(BIN_DIR / tool), *map(str, args)]
  if platform.system() == "Windows" and len(subprocess.list2cmdline(command)) > 30000:
    # SDDS's existing argument-file syntax avoids CreateProcess's length limit.
    artifacts = ROOT_DIR / "tests/artifacts"
    artifacts.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="enum-arguments-", dir=artifacts) as directory:
      arguments = Path(directory) / "arguments.txt"
      values = [value.replace("\\", "/") if ":\\" in value else value
                for value in command[1:]]
      arguments.write_text("\n".join(values) + "\n", encoding="utf8")
      result = subprocess.run([command[0], f"@@{arguments.as_posix()}"],
                              capture_output=True, text=True, encoding="utf8")
  else:
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf8")
  if check:
    assert result.returncode == 0, result.stderr
  return result


def make_enum(path, mapping=MAPPING, values="0,10,-1", append=None):
  args = [path, "-column=state,type=enum", f"-data={values}",
          f"-enumValues=column,state,{mapping}"]
  if append:
    args.append(append)
  return run("sddsmakedataset", *args)


def test_enum_options_in_argument_files(tmp_path):
  source, remapped = tmp_path / "source.sdds", tmp_path / "remapped.sdds"
  options = tmp_path / "options.txt"
  options.write_text(f"{source.as_posix()}\n-column=state,type=enum\n-data=0,10,-1\n"
                     f"-enumValues=column,state,{MAPPING}\n", encoding="utf8")
  run("sddsmakedataset", f"@@{options.as_posix()}")
  assert labels(source) == ["OFF", "ON", "FAULT"]
  options.write_text(f"{source.as_posix()}\n{remapped.as_posix()}\n"
                     "-cast=column,state,enum,enum\n"
                     "-enumValues=column,state,7:OFF,8:ON,9:FAULT\n", encoding="utf8")
  run("sddscast", f"@@{options.as_posix()}")
  assert labels(remapped) == ["OFF", "ON", "FAULT"]
  assert labels(remapped, True) == ["7", "8", "9"]


def labels(path, codes=False):
  args = [path, "-column=state", "-noquotes"]
  if codes:
    args.append("-enumCodes")
  return run("sdds2stream", *args).stdout.split()


@pytest.mark.parametrize("path", sorted(FIXTURES.glob("*.sdds")), ids=lambda p: p.stem)
def test_shared_enum_fixtures(path):
  result = run("sddscheck", path, check=False)
  assert (result.stdout.strip() == "ok") == path.name.startswith("valid-"), result


def test_enum_display_sort_filter_copy_and_numeric_rejection(tmp_path):
  source, ordered, filtered, copied = [tmp_path / n for n in
                                      ("source.sdds", "sorted.sdds", "filtered.sdds", "copied.sdds")]
  make_enum(source)
  assert labels(source) == ["OFF", "ON", "FAULT"]
  assert labels(source, True) == ["0", "10", "-1"]
  query = run("sddsquery", source).stdout
  assert "enum" in query and MAPPING in query
  printed = run("sddsprintout", source, "-column=state", "-noTitle").stdout
  assert all(label in printed for label in ("OFF", "ON", "FAULT"))
  run("sddssort", source, ordered, "-column=state")
  assert labels(ordered) == ["FAULT", "OFF", "ON"]
  run("sddsprocess", source, filtered, "-match=column,state=ON")
  assert labels(filtered) == ["ON"]
  run("sddsconvert", source, copied, "-ascii")
  assert copied.read_bytes().startswith(b"SDDS6\n")
  assert labels(copied) == labels(source)
  result = run("sddsprocess", source, tmp_path / "mean.sdds",
               "-process=state,average,mean", check=False)
  assert result.returncode != 0
  result = run("sddsprocess", source, tmp_path / "number-test.sdds",
               "-numberTest=column,state", check=False)
  assert result.returncode > 0, "enum numeric scanning must report an error without crashing"


@pytest.mark.parametrize("kind", ["column", "parameter"])
def test_enum_printout_format_defaults_use_label_formats(tmp_path, kind):
  source = tmp_path / "state.sdds"
  run("sddsmakedataset", source, f"-{kind}=state,type=enum", "-data=10",
      f"-enumValues={kind},state,{MAPPING}")
  default = run("sddsprintout", source, f"-{kind}=state", "-formatDefaults=enum=%30s",
                "-noTitle", "-noLabels")
  explicit = run("sddsprintout", source, f"-{kind}=state,format=%30s", "-noTitle", "-noLabels")
  assert not default.stderr
  assert default.stdout == explicit.stdout
  assert f'{"ON":>30}' in default.stdout


@pytest.mark.parametrize("group_type", ["enum", "string"])
def test_latex_printout_groups_by_labels(tmp_path, group_type):
  source = tmp_path / "grouped.sdds"
  states = "0,0,10,-1,-1,0" if group_type == "enum" else "OFF,OFF,ON,FAULT,FAULT,OFF"
  extra = [f"-enumValues=column,state,{MAPPING}"] if group_type == "enum" else []
  run("sddsmakedataset", source, f"-column=state,type={group_type}", f"-data={states}",
      "-column=x,type=long", "-data=1,2,3,4,5,6", *extra)
  result = run("sddsprintout", source, "-column=x", "-latexFormat=group=state")
  assert "nGroups = 4, maxGroupLength = 2" in result.stdout
  headings = [line for line in result.stdout.splitlines() if line.startswith(r"\multicolumn")]
  assert len(headings) == 4
  assert [heading.split(r"\bf ", 1)[1].split("}", 1)[0] for heading in headings] == [
      "OFF", "ON", "FAULT", "OFF"]


def test_latex_printout_rejects_numeric_group_column(tmp_path):
  source = tmp_path / "numeric.sdds"
  run("sddsmakedataset", source, "-column=x,type=long", "-data=1,2")
  result = run("sddsprintout", source, "-column=x", "-latexFormat=group=x", check=False)
  assert result.returncode > 0
  assert "group column must have string or enum type" in result.stderr


@pytest.mark.parametrize("fixed,label", [("10 ", "ON"), (" 10 ", "ON"), ("-1 ", "FAULT")])
def test_fixed_long_enum_conversion_accepts_reader_whitespace(tmp_path, fixed, label):
  source, output = tmp_path / "fixed-long.sdds", tmp_path / "converted.sdds"
  source.write_text('SDDS1\n'
                    f'&parameter name=state, type=long, fixed_value="{fixed}" &end\n'
                    '&column name=x, type=long &end\n'
                    '&data mode=ascii &end\n1\n1\n', encoding="utf8")
  assert int(run("sdds2stream", source, "-parameter=state").stdout) == int(fixed)
  run("sddscast", source, output, "-cast=parameter,state,long,enum",
      f"-enumValues=parameter,state,{MAPPING}")
  assert run("sdds2stream", output, "-parameter=state", "-noquotes").stdout.strip() == label


@pytest.mark.parametrize("match_column", ["state", "stateText"])
@pytest.mark.parametrize("pattern,average,first", [
    ("ON", 3, "second"),
    ("O*", 7 / 3, "first"),
    ("FAULT", 3, "third"),
    ("missing", -7, "_NoResultFound_"),
])
def test_processing_match_qualifiers_use_labels(tmp_path, match_column, pattern, average, first):
  source, output = tmp_path / "mixed.sdds", tmp_path / "processed.sdds"
  run("sddsmakedataset", source, "-column=state,type=enum", "-data=0,10,-1,10",
      f"-enumValues=column,state,{MAPPING}",
      "-column=stateText,type=string", "-data=OFF,ON,FAULT,ON",
      "-column=x,type=double", "-data=1,2,3,4",
      "-column=text,type=string", "-data=first,second,third,fourth")
  run("sddsprocess", source, output,
      f"-process=x,average,xAverage,match={match_column},value={pattern},default=-7",
      f"-process=text,first,textFirst,match={match_column},value={pattern}")
  assert float(run("sdds2stream", output, "-parameter=xAverage").stdout) == pytest.approx(average)
  assert run("sdds2stream", output, "-parameter=textFirst", "-noquotes").stdout.strip() == first
  assert labels(output) == ["OFF", "ON", "FAULT", "ON"]


@pytest.mark.parametrize("source_column,operation", [("x", "average"), ("text", "first")])
def test_processing_match_qualifiers_reject_numeric_columns(tmp_path, source_column, operation):
  source, output = tmp_path / "mixed.sdds", tmp_path / "processed.sdds"
  run("sddsmakedataset", source, "-column=x,type=double", "-data=1,2",
      "-column=text,type=string", "-data=one,two")
  result = run("sddsprocess", source, output,
               f"-process={source_column},{operation},result,match=x,value=1", check=False)
  assert result.returncode > 0
  assert "Processing match column must have string or enum type" in result.stderr


def test_processing_enum_match_qualifiers_respect_selected_rows(tmp_path):
  source, output = tmp_path / "mixed.sdds", tmp_path / "processed.sdds"
  run("sddsmakedataset", source, "-column=state,type=enum", "-data=0,10,-1,10",
      f"-enumValues=column,state,{MAPPING}",
      "-column=x,type=double", "-data=1,2,3,4",
      "-column=text,type=string", "-data=first,second,third,fourth")
  run("sddsprocess", source, output, "-match=column,text=fourth",
      "-process=x,average,xAverage,match=state,value=ON",
      "-process=text,first,textFirst,match=state,value=ON")
  assert float(run("sdds2stream", output, "-parameter=xAverage").stdout) == 4
  assert run("sdds2stream", output, "-parameter=textFirst", "-noquotes").stdout.strip() == "fourth"


@pytest.mark.parametrize("selection", [
    "-filter=parameter,state,0,20",
    "-timeFilter=parameter,state,after=1969/12/31@00:00:00,before=1970/01/02@00:00:00",
])
def test_parameter_numeric_filters_reject_enums(tmp_path, selection):
  for field_type in ("long", "enum"):
    source = tmp_path / f"{field_type}.sdds"
    output = tmp_path / f"{field_type}-filtered.sdds"
    extra = [f"-enumValues=parameter,state,{MAPPING}"] if field_type == "enum" else []
    run("sddsmakedataset", source, f"-parameter=state,type={field_type}", "-data=10",
        "-column=x,type=double", "-data=1,2", *extra)
    result = run("sddsprocess", source, output, selection, check=False)
    if field_type == "enum":
      assert result.returncode > 0
      assert "non-numeric parameter state" in result.stderr
    else:
      assert result.returncode == 0, result.stderr
      assert run("sdds2stream", output, "-rows=bare").stdout.strip() == "2"


@pytest.mark.skipif(not (BIN_DIR / "sddsplot").exists(), reason="sddsplot not built")
@pytest.mark.parametrize("selection", [
    "-filter=parameter,state,0,20",
    "-filter=parameter,state,20,30,!",
    "-timeFilter=parameter,state,after=1969/12/31@00:00:00,before=1970/01/02@00:00:00",
    "-timeFilter=parameter,state,after=1970/01/03@00:00:00,before=1970/01/04@00:00:00,invert",
])
def test_plot_parameter_numeric_filters_reject_enums(tmp_path, selection):
  for field_type in ("long", "enum"):
    source, output = tmp_path / f"{field_type}.sdds", tmp_path / f"{field_type}.json"
    extra = [f"-enumValues=parameter,state,{MAPPING}"] if field_type == "enum" else []
    run("sddsmakedataset", source, f"-parameter=state,type={field_type}", "-data=10",
        "-column=x,type=double", "-data=1,2", "-column=y,type=double", "-data=3,4", *extra)
    result = run("sddsplot", source, "-columnNames=x,y", selection,
                 "-device=json", f"-output={output}", check=False)
    if field_type == "enum":
      assert result.returncode > 0
      assert "non-numeric parameter state" in result.stderr
      assert "no datasets to plot" not in result.stderr
    else:
      assert result.returncode == 0, result.stderr
      assert json.loads(output.read_text())["plots"][0]["traces"][0]["y"] == [3, 4]


@pytest.mark.skipif(not (BIN_DIR / "sddsplot").exists(), reason="sddsplot not built")
@pytest.mark.parametrize("kind", ["column", "parameter"])
def test_plot_numeric_definitions_reject_existing_enums(tmp_path, kind):
  for field_type in ("long", "enum"):
    source, output = tmp_path / f"{field_type}.sdds", tmp_path / f"{field_type}.json"
    extra = [f"-enumValues={kind},state,{MAPPING}"] if field_type == "enum" else []
    run("sddsmakedataset", source, f"-{kind}=state,type={field_type}",
        "-data=10,10" if kind == "column" else "-data=10",
        "-column=x,type=double", "-data=1,2", "-column=y,type=double", "-data=3,4", *extra)
    result = run("sddsplot", source, "-columnNames=x,y", f"-define={kind},state,0",
                 "-device=json", f"-output={output}", check=False)
    if field_type == "enum":
      assert result.returncode > 0
      assert "nonnumeric" in result.stderr.lower().replace("-", "")
    else:
      assert result.returncode == 0, result.stderr
      assert json.loads(output.read_text())["plots"][0]["traces"][0]["y"] == [3, 4]


def test_numeric_processing_rejects_enum_parameter_offset(tmp_path):
  source, output = tmp_path / "enum-offset.sdds", tmp_path / "processed.sdds"
  run("sddsmakedataset", source, "-parameter=state,type=enum", "-data=10",
      f"-enumValues=parameter,state,{MAPPING}", "-column=x,type=double", "-data=1,2")
  result = run("sddsprocess", source, output,
               "-process=x,average,xAverage,offset=@state", check=False)
  assert result.returncode > 0
  assert "nonnumeric" in result.stderr.lower()


@pytest.mark.parametrize("kind", ["column", "parameter"])
def test_numeric_rpn_array_access_rejects_enums(tmp_path, kind):
  source, output = tmp_path / "enum-array.sdds", tmp_path / "processed.sdds"
  run("sddsmakedataset", source, "-column=state,type=enum", "-data=10,0",
      f"-enumValues=column,state,{MAPPING}", "-column=current,type=double", "-data=12.5,3.5")
  result = run("sddsprocess", source, output,
               f"-define={kind},result,0 &state [,type=double", check=False)
  assert result.returncode > 0
  assert "array pointer -1 is invalid" in result.stderr
  run("sddsprocess", source, output, f"-define={kind},result,0 &current [,type=double")
  values = run("sdds2stream", output, f"-{kind}=result", "-noquotes").stdout.split()
  assert list(map(float, values)) == ([12.5, 12.5] if kind == "column" else [12.5])
  assert labels(output) == ["ON", "OFF"]


@pytest.mark.parametrize("fixed", [False, True])
@pytest.mark.parametrize("pattern,expected", [
    ("ON", ["ON"]),
    ("O*", ["OFF", "ON"]),
    ("+on", ["ON"]),
    ("ON,!", ["OFF", "FAULT"]),
    ("missing", []),
])
def test_enum_parameter_label_matching(tmp_path, fixed, pattern, expected):
  source, output = tmp_path / "parameters.sdds", tmp_path / "matched.sdds"
  header = 'SDDS6\n'
  if not fixed:
    header += f'&parameter name=state,type=enum,enum_values="{MAPPING}" &end\n'
  else:
    header += f'&parameter name=state,type=enum,enum_values="{MAPPING}",fixed_value="10" &end\n'
  header += '&column name=x,type=long &end\n&data mode=ascii &end\n'
  source.write_text(header + ('1\n1\n' if fixed else '0\n1\n1\n10\n1\n2\n-1\n1\n3\n'), encoding="utf8")
  run("sddsprocess", source, output, f"-match=parameter,state={pattern}")
  result = run("sdds2stream", output, "-parameter=state", "-noquotes").stdout.split()
  if fixed:
    expected = ["ON"] if "ON" in expected else []
  assert result == expected
  assert MAPPING in run("sddsquery", output).stdout


def test_enum_parameter_matching_keeps_long_labels(tmp_path):
  source, output = tmp_path / "long-label.sdds", tmp_path / "matched.sdds"
  label = "a" * 20000 + "雪"
  run("sddsmakedataset", source, "-parameter=state,type=enum", "-data=10",
      f"-enumValues=parameter,state,10:{label}", "-column=x,type=long", "-data=1")
  run("sddsprocess", source, output, "-match=parameter,state=a*雪")
  assert run("sdds2stream", output, "-parameter=state", "-noquotes").stdout == label + "\n"


def test_enum_cast_codes_labels_remap_and_downgrade(tmp_path):
  source, codes, strings, remapped, restored = [tmp_path / n for n in
      ("source.sdds", "codes.sdds", "strings.sdds", "remapped.sdds", "restored.sdds")]
  make_enum(source)
  run("sddscast", source, codes, "-cast=column,state,enum,long")
  run("sddscast", source, strings, "-cast=column,state,enum,string")
  assert codes.read_bytes()[:6] != b"SDDS6\n"
  assert labels(codes) == ["0", "10", "-1"]
  assert labels(strings) == ["OFF", "ON", "FAULT"]
  run("sddscast", strings, restored, "-cast=column,state,string,enum",
      "-enumValues=column,state,7:OFF,8:ON,9:FAULT")
  assert labels(restored, True) == ["7", "8", "9"]
  run("sddscast", source, remapped, "-cast=column,state,enum,enum",
      "-enumValues=column,state,7:OFF,8:ON,9:FAULT")
  assert labels(remapped) == labels(source)
  assert labels(remapped, True) == ["7", "8", "9"]
  for extra in ([], ["-enumValues=column,state,7:OFF,8:ON"]):
    result = run("sddscast", strings, tmp_path / "bad.sdds",
                 "-cast=column,state,string,enum", *extra, check=False)
    assert result.returncode != 0
  result = run("sddscast", source, tmp_path / "bad-numeric.sdds",
               "-cast=column,state,enum,double", check=False)
  assert result.returncode != 0


@pytest.mark.parametrize("pipe_input", [False, True], ids=["file-input", "stdin-input"])
@pytest.mark.parametrize("element", ["column", "parameter", "fixed-parameter", "array"])
@pytest.mark.parametrize("source_type,target_type", [
    ("enum", "long"), ("enum", "string"), ("enum", "enum"),
    ("long", "enum"), ("string", "enum"),
])
def test_enum_cast_piped_output(tmp_path, pipe_input, element, source_type, target_type):
  source, output = tmp_path / "source.sdds", tmp_path / "converted.sdds"
  kind = "parameter" if element == "fixed-parameter" else element
  values = ["10"] if kind == "parameter" else ["0", "10", "-1"]
  expected_labels = ["ON"] if kind == "parameter" else ["OFF", "ON", "FAULT"]
  stored_values = expected_labels if source_type == "string" else values
  enum_values = f', enum_values="{MAPPING}"' if source_type == "enum" else ""
  fixed_value = f', fixed_value="{stored_values[0]}"' if element == "fixed-parameter" else ""
  header = ("SDDS6\n" if source_type == "enum" else "SDDS1\n")
  header += f'&{kind} name=state, type={source_type}{enum_values}{fixed_value} &end\n'
  if kind != "column":
    header += '&column name=x, type=long &end\n'
  header += '&data mode=ascii &end\n'
  if kind == "column":
    data = "3\n" + "\n".join(stored_values) + "\n"
  elif kind == "array":
    data = "3\n" + " ".join(stored_values) + "\n1\n1\n"
  else:
    data = ("" if fixed_value else stored_values[0] + "\n") + "1\n1\n"
  source.write_text(header + data, encoding="utf8")

  args = ["-pipe=input,output"] if pipe_input else [str(source), "-pipe=output"]
  args.append(f"-cast={kind},state,{source_type},{target_type}")
  target_mapping = "7:OFF,8:ON,9:FAULT" if source_type == "enum" else MAPPING
  if target_type == "enum":
    args.append(f"-enumValues={kind},state,{target_mapping}")
  result = subprocess.run([str(BIN_DIR / "sddscast"), *args],
                          input=source.read_bytes() if pipe_input else None,
                          capture_output=True)
  assert result.returncode == 0, result.stderr.decode()
  output.write_bytes(result.stdout)
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert run("sdds2stream", output, f"-{kind}=state", "-noquotes").stdout.split() == (
      values if target_type == "long" else expected_labels)
  if target_type == "enum":
    assert result.stdout.startswith(b"SDDS6\n")
    assert target_mapping in run("sddsquery", output).stdout
    expected_codes = values
    if source_type == "enum":
      expected_codes = ["8"] if kind == "parameter" else ["7", "8", "9"]
    assert run("sdds2stream", output, f"-{kind}=state", "-enumCodes", "-noquotes").stdout.split() == expected_codes
  else:
    assert not result.stdout.startswith(b"SDDS6\n")


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_wildcard_cast_filters_types_before_enum_validation(tmp_path, kind):
  source, numeric, codes, restored = [tmp_path / name for name in
      ("mixed.sdds", "numeric.sdds", "codes.sdds", "restored.sdds")]
  states = "10" if kind == "parameter" else "0,10,-1"
  numbers = "1.5" if kind == "parameter" else "1.5,2.5,3.5"
  run("sddsmakedataset", source, f"-{kind}=state,type=enum", f"-data={states}",
      f"-enumValues={kind},state,{MAPPING}", f"-{kind}=x,type=double", f"-data={numbers}")
  run("sddscast", source, numeric, f"-cast={kind},*,double,float")
  header = numeric.read_bytes().split(b"&data", 1)[0]
  assert b"type=float" in header and b"type=double" not in header
  assert MAPPING in run("sddsquery", numeric).stdout
  assert run("sdds2stream", numeric, f"-{kind}=state", "-noquotes").stdout.split() == (
      ["ON"] if kind == "parameter" else ["OFF", "ON", "FAULT"])
  assert [float(value) for value in run("sdds2stream", numeric, f"-{kind}=x").stdout.split()] == (
      [1.5] if kind == "parameter" else [1.5, 2.5, 3.5])

  # A target enum must likewise ignore numeric fields excluded by the old type.
  run("sddscast", source, codes, f"-cast={kind},state,enum,long")
  run("sddscast", codes, restored, f"-cast={kind},*,long,enum",
      f"-enumValues={kind},state,{MAPPING}")
  assert run("sdds2stream", restored, f"-{kind}=state", "-enumCodes", "-noquotes").stdout.split() == states.split(",")


@pytest.mark.parametrize("label_type,label_values", [
    ("string", ("device-A", "device%20B")),
    ("long", ("101", "205")),
])
def test_enum_diff_uses_requested_row_labels(tmp_path, label_type, label_values):
  paths = [tmp_path / "a.sdds", tmp_path / "b.sdds"]
  for path, codes in zip(paths, ((0, 10), (10, 0))):
    path.write_text('SDDS6\n'
                    f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
                    f'&column name=ID, type={label_type} &end\n'
                    '&data mode=ascii &end\n2\n' +
                    ''.join(f'{code} {label}\n' for code, label in zip(codes, label_values)),
                    encoding="utf8")
  output = run("sddsdiff", *paths, "-rowLabel=ID").stdout
  rows = [line.split() for line in output.splitlines() if " | " in line]
  assert rows == [[label_values[0], "OFF", "|", "ON"],
                  [label_values[1], "ON", "|", "OFF"]]
  output = run("sddsdiff", *paths).stdout
  rows = [line.split() for line in output.splitlines() if " | " in line]
  assert rows == [["1", "OFF", "|", "ON"], ["2", "ON", "|", "OFF"]]


def test_mapping_compatibility_combine_append_and_diff(tmp_path):
  a, b, c, merged = [tmp_path / n for n in ("a.sdds", "b.sdds", "c.sdds", "merged.sdds")]
  make_enum(a)
  make_enum(b, "-1:FAULT,10:ON,0:OFF")
  make_enum(c, "0:ON,10:OFF,-1:FAULT")
  run("sddscombine", a, b, merged)
  assert labels(merged) == ["OFF", "ON", "FAULT"] * 2
  result = run("sddscombine", a, c, tmp_path / "bad.sdds", check=False)
  assert result.returncode != 0
  different = run("sddsdiff", a, c, check=False)
  assert "mapping" in (different.stdout + different.stderr).lower()
  failed = run("sddsmakedataset", a, "-column=state,type=enum", "-data=10",
      "-enumValues=column,state,0:ON,10:OFF,-1:FAULT", "-append", check=False)
  assert failed.returncode != 0
  # Failed append must leave the original page readable.
  assert labels(a) == ["OFF", "ON", "FAULT"]


def test_long_mapping_and_explicit_primitive_export(tmp_path):
  source, exported = tmp_path / "large.sdds", tmp_path / "plain.txt"
  mapping = ",".join(f"{i}:state{i:05d}" for i in range(6000))
  make_enum(source, mapping, "0,5999")
  assert labels(source) == ["state00000", "state05999"]
  assert mapping in run("sddsquery", source).stdout
  result = run("sdds2plaindata", source, exported, "-column=state", check=False)
  assert result.returncode != 0 and "sddscast" in result.stderr
  codes = tmp_path / "codes.sdds"
  run("sddscast", source, codes, "-cast=column,state,enum,long")
  run("sdds2plaindata", codes, exported, "-column=state")
  assert exported.exists()


def test_prechange_readers_reject_sdds6(tmp_path):
  baseline = ROOT_DIR / "tests" / f"O.{PLATFORM_ID}" / "enum-baseline"
  if not (baseline / "sddsquery").exists():
    pytest.skip("identifiable pre-change reader is available only on qualification hosts")
  source = tmp_path / "enum.sdds"
  make_enum(source)
  for reader, extra in (("sddsquery", []), ("sdds2stream", ["-column=state"]), ("sddspp_read_demo", [])):
    if (baseline / reader).exists():
      result = subprocess.run([str(baseline / reader), str(source), *extra], capture_output=True, timeout=10)
      assert result.returncode != 0, reader


def test_enum_preserved_during_unrelated_numeric_processing_and_valid_append(tmp_path):
  source, processed = tmp_path / "mixed.sdds", tmp_path / "processed.sdds"
  run("sddsmakedataset", source, "-column=state,type=enum", "-data=0,10,-1",
      f"-enumValues=column,state,{MAPPING}", "-column=x,type=double", "-data=1,2,3")
  run("sddsprocess", source, processed, "-process=x,average,mean")
  assert labels(processed) == ["OFF", "ON", "FAULT"]
  assert MAPPING in run("sddsquery", processed).stdout
  appended = tmp_path / "append.sdds"
  make_enum(appended)
  make_enum(appended, values="10", append="-append")
  assert labels(appended) == ["OFF", "ON", "FAULT", "ON"]
  make_enum(appended, values="-1", append="-append=merge")
  assert labels(appended) == ["OFF", "ON", "FAULT", "ON", "FAULT"]


def test_fixed_enum_string_conversion_preserves_literal_escapes(tmp_path):
  mapping = r'0:OFF,20: a\,b\:c\\d\"雪 '
  outer = mapping.replace('\\', '\\\\').replace('"', '\\"')
  source, strings, restored = [tmp_path / name for name in
                                ("fixed.sdds", "strings.sdds", "restored.sdds")]
  source.write_text('SDDS6\n'
                    f'&parameter name=fixed, type=enum, enum_values="{outer}", fixed_value="20" &end\n'
                    '&column name=state, type=enum, enum_values="0:OFF" &end\n'
                    '&data mode=ascii &end\n1\n0\n', encoding="utf8")
  run("sddscast", source, strings, "-cast=parameter,fixed,enum,string")
  assert run("sdds2stream", strings, "-parameter=fixed", "-noquotes").stdout == ' a,b:c\\d"雪 \n'
  run("sddscast", strings, restored, "-cast=parameter,fixed,string,enum",
      f"-enumValues=parameter,fixed,{mapping}")
  assert run("sdds2stream", restored, "-parameter=fixed", "-enumCodes", "-noquotes").stdout == "20\n"


@pytest.mark.parametrize("fixed,code,label", [("10 ", 10, "ON"), (" \t-1\r\n ", -1, "FAULT"), (" 0 ", 0, "OFF")])
@pytest.mark.parametrize("target", ["long", "string", "enum"])
def test_fixed_enum_conversion_accepts_reader_whitespace(tmp_path, fixed, code, label, target):
  source, output = tmp_path / "fixed.sdds", tmp_path / "converted.sdds"
  escaped = fixed.replace("\t", r"\t").replace("\r", r"\r").replace("\n", r"\n")
  source.write_text('SDDS6\n'
                    f'&parameter name=fixed, type=enum, enum_values="{MAPPING}", fixed_value="{escaped}" &end\n'
                    '&column name=state, type=enum, enum_values="0:OFF" &end\n'
                    '&data mode=ascii &end\n1\n0\n', encoding="utf8")
  assert run("sdds2stream", source, "-parameter=fixed", "-noquotes").stdout.strip() == label
  extra = ["-enumValues=parameter,fixed,7:OFF,8:ON,9:FAULT"] if target == "enum" else []
  run("sddscast", source, output, f"-cast=parameter,fixed,enum,{target}", *extra)
  assert run("sdds2stream", output, "-parameter=fixed", "-noquotes").stdout.strip() == (
      str(code) if target == "long" else label)
  if target == "enum":
    assert run("sdds2stream", output, "-parameter=fixed", "-noquotes", "-enumCodes").stdout.strip() == str(
        {"OFF": 7, "ON": 8, "FAULT": 9}[label])


@pytest.mark.parametrize("kind", ["column", "parameter"])
def test_long_enum_names_print_and_label_editing_is_bounded(tmp_path, kind):
  source = tmp_path / "long-name.sdds"
  name = "state" + "A" * 20000
  run("sddsmakedataset", source, f"-{kind}={name},type=enum", "-data=10",
      f"-enumValues={kind},{name},{MAPPING}")
  result = run("sddsprintout", source, f"-{kind}=*", "-noTitle")
  assert name in result.stdout and "ON" in result.stdout
  result = subprocess.run([str(BIN_DIR / "sddsprintout"), str(source),
                           f"-{kind}=*,editLabel=1f", "-noTitle"],
                          capture_output=True, text=True, timeout=10)
  assert result.returncode > 0
  assert "editLabel requires a label shorter than 1024 bytes" in result.stderr
  result = run("sddsprintout", source, f"-{kind}=*,label=State,editLabel=1f", "-noTitle")
  assert "State" in result.stdout and "ON" in result.stdout
