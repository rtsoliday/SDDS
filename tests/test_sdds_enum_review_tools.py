"""Regression coverage for enum grouping, display and toolkit rejection paths."""
import re
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


MAPPING = "0:OFF,1:ON"
TOOLS = ("sddscollect", "sddsseparate", "sddsdiff", "sdds2stream")
pytestmark = pytest.mark.skipif(
    not all((BIN_DIR / tool).exists() for tool in TOOLS), reason="tools not built")


def run(tool, *args, check=True):
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)],
                          capture_output=True, text=True)
  if check:
    assert result.returncode == 0, result.stderr
  return result


def grouped_input(path, second_mapping, second_code=1):
  path.write_text('SDDS6\n'
                  f'&column name=Ax, type=enum, enum_values="{MAPPING}" &end\n'
                  f'&column name=Bx, type=enum, enum_values="{second_mapping}" &end\n'
                  '&data mode=ascii &end\n'
                  f'2\n0 {second_code}\n0 {second_code}\n', encoding="utf8")


def group(tool, source, output, check=True):
  option = "-collect=suffix=x,column=State" if tool == "sddscollect" else "-group=State,Ax,Bx"
  return run(tool, source, output, option, check=check)


@pytest.mark.parametrize("tool", ["sddscollect", "sddsseparate"])
@pytest.mark.parametrize("mapping", [MAPPING, "1:ON,0:OFF"])
def test_enum_groups_accept_equivalent_dictionaries(tmp_path, tool, mapping):
  source, output = tmp_path / "group.sdds", tmp_path / "grouped.sdds"
  grouped_input(source, mapping)
  group(tool, source, output)
  values = run("sdds2stream", output, "-column=State", "-noquotes").stdout.split()
  assert values == (["OFF", "ON", "OFF", "ON"] if tool == "sddscollect" else
                    ["OFF", "OFF", "ON", "ON"])


@pytest.mark.parametrize("tool", ["sddscollect", "sddsseparate"])
@pytest.mark.parametrize("mapping,code", [
    ("0:CLOSED,1:OPEN", 1),
    ("0:ON,1:OFF", 1),
    ("0:OFF,2:ON", 2),
    ("0:OFF,1:ON,2:FAULT", 1),
])
def test_enum_groups_reject_incompatible_dictionaries(tmp_path, tool, mapping, code):
  source, output = tmp_path / "group.sdds", tmp_path / "grouped.sdds"
  grouped_input(source, mapping, code)
  result = group(tool, source, output, check=False)
  assert result.returncode != 0
  assert "Incompatible enum mappings" in result.stderr
  assert "Bx" in result.stderr


@pytest.mark.parametrize("label_source", ["first", "second"])
@pytest.mark.parametrize("value_type,first_values,second_values", [
    ("long", (10, 20), (11, 21)),
    ("double", (1.5, 2.5), (1.6, 2.6)),
    ("string", ("before", "before"), ("after", "after")),
    ("enum", (0, 1), (1, 0)),
])
def test_diff_enum_row_labels_use_dictionary_labels(tmp_path, label_source, value_type,
                                                   first_values, second_values):
  paths = [tmp_path / "first.sdds", tmp_path / "second.sdds"]
  for index, (path, values) in enumerate(zip(paths, (first_values, second_values))):
    has_labels = label_source == "first" or index == 1
    enum_values = f', enum_values="{MAPPING}"' if value_type == "enum" else ""
    path.write_text(("SDDS6\n" if has_labels or enum_values else "SDDS1\n") +
                    f'&column name=value, type={value_type}{enum_values} &end\n' +
                    (f'&column name=State, type=enum, enum_values="{MAPPING}" &end\n' if has_labels else "") +
                    '&data mode=ascii &end\n2\n' +
                    ''.join(f'{value}' + (f' {row}' if has_labels else "") + '\n'
                            for row, value in enumerate(values)), encoding="utf8")
  result = run("sddsdiff", *paths, "-columns=value", "-rowLabel=State,nocomparison")
  rows = [line.split() for line in result.stdout.splitlines()
          if line.split() and line.split()[0] in ("OFF", "ON")]
  assert [row[0] for row in rows] == ["OFF", "ON"]
  if value_type == "enum":
    assert rows == [["OFF", "OFF", "|", "ON"], ["ON", "ON", "|", "OFF"]]


@pytest.mark.parametrize("invalid_inputs", [(True, False), (False, True), (True, True)])
@pytest.mark.parametrize("valid_prefix", [False, True])
def test_diff_reports_enum_page_read_errors(tmp_path, invalid_inputs, valid_prefix):
  paths = [tmp_path / "first.sdds", tmp_path / "second.sdds"]
  for path, invalid in zip(paths, invalid_inputs):
    path.write_text('SDDS6\n'
                    f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
                    '&data mode=ascii &end\n' +
                    ('1\n0\n' if valid_prefix else '') +
                    f'1\n{42 if invalid else 0}\n', encoding="utf8")

  result = run("sddsdiff", *paths, check=False)
  assert result.returncode > 0
  assert str(paths[invalid_inputs.index(True)]) in result.stderr
  assert "enum" in result.stderr.lower()
  assert "identical" not in result.stdout
  assert "fewer pages" not in result.stderr


@pytest.mark.parametrize("second_pages,identical", [((0,), True), ((1,), False), ((0, 1), False)])
def test_diff_valid_enum_comparison_keeps_normal_results(tmp_path, second_pages, identical):
  paths = [tmp_path / "first.sdds", tmp_path / "second.sdds"]
  for path, codes in zip(paths, ((0,), second_pages)):
    path.write_text('SDDS6\n'
                    f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
                    '&data mode=ascii &end\n' +
                    ''.join(f'1\n{code}\n' for code in codes), encoding="utf8")

  result = run("sddsdiff", *paths)
  assert ("are identical" in result.stdout) == identical
  assert ("are different" in result.stderr) == (not identical)
  assert "Unable to read page" not in result.stderr


@pytest.mark.parametrize("tool", ["sdds2math", "sdds2spreadsheet"])
@pytest.mark.parametrize("existing_output", [False, True])
def test_enum_export_rejection_preserves_destination(tmp_path, tool, existing_output):
  if not (BIN_DIR / tool).exists():
    pytest.skip(f"{tool} not built")
  source, output = tmp_path / "source.sdds", tmp_path / "export.txt"
  source.write_text('SDDS1\n&column name=state, type=long &end\n'
                    '&data mode=ascii &end\n2\n0\n1\n', encoding="utf8")
  run(tool, source, output)
  original = output.read_bytes()
  assert original
  if not existing_output:
    output.unlink()
  source.write_text('SDDS6\n'
                    f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
                    '&data mode=ascii &end\n2\n0\n1\n', encoding="utf8")

  result = run(tool, source, output, check=False)
  assert result.returncode > 0
  assert "sddscast" in result.stderr
  assert result.stdout == ""
  if existing_output:
    assert output.read_bytes() == original
  else:
    assert not output.exists()


def contour_array_input(path, field_type="long", nonnumeric_array=None):
  definitions = []
  for name in ("z", "x", "y"):
    array_type = field_type if nonnumeric_array in (None, name) else "long"
    mapping = f', enum_values="{MAPPING}"' if array_type == "enum" else ""
    definitions.append(f'&array name={name}, type={array_type}, '
                       f'dimensions={2 if name == "z" else 1}{mapping} &end\n')
  path.write_text(("SDDS6\n" if field_type == "enum" else "SDDS1\n") +
                  ''.join(definitions) + '&data mode=ascii &end\n'
                  '2 2\n0 1 1 0\n2\n0 1\n2\n0 1\n', encoding="utf8")


@pytest.mark.skipif(not (BIN_DIR / "sddscontour").exists(), reason="sddscontour not built")
@pytest.mark.parametrize("array_name", ["z", "x", "y"])
@pytest.mark.parametrize("array_type", ["enum", "string", "character"])
def test_contour_rejects_nonnumeric_selected_arrays(tmp_path, array_name, array_type):
  source, output = tmp_path / "arrays.sdds", tmp_path / "plot.ps"
  contour_array_input(source, array_type, array_name)
  result = run("sddscontour", source, "-array=z,x,y", "-device=postscript",
               f"-output={output}", check=False)
  assert result.returncode > 0
  assert f"array {array_name} must have numeric type" in result.stderr


@pytest.mark.skipif(not (BIN_DIR / "sddscontour").exists(), reason="sddscontour not built")
@pytest.mark.parametrize("array_type", ["long", "double"])
def test_contour_numeric_arrays_retain_intensities(tmp_path, array_type):
  source, output = tmp_path / "arrays.sdds", tmp_path / "plot.ps"
  contour_array_input(source, array_type)
  result = run("sddscontour", source, "-array=z,x,y", "-device=postscript",
               f"-output={output}", "-verbosity=2")
  maximum = re.search(r"maximum value is ([^,\s]+)", result.stdout)
  minimum = re.search(r"minimum value is ([^,\s]+)", result.stdout)
  assert maximum and float(maximum.group(1)) == 1
  assert minimum and float(minimum.group(1)) == 0
  assert output.stat().st_size > 0
