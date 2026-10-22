"""Plot label edits must fit the workspace throughout the edit, including NUL."""

import json
import pytest

from sdds_test_utils import BIN_DIR
from test_sddsplot_enum import native_png_data, run_plot, traces


TARGETS = ("title", "topline", "xlabel", "ylabel", "legend", "string",
           "pointLabel", "enumeratedScales")
pytestmark = pytest.mark.skipif(not (BIN_DIR / "sddsplot").exists(),
                                reason="sddsplot not built")


def label_input(path, value_type, label):
  mapping = (f', enum_values={json.dumps("6:" + label, ensure_ascii=False)}'
             if value_type == "enum" else "")
  value = "6" if value_type == "enum" else json.dumps(label, ensure_ascii=False)
  path.write_text(("SDDS6\n" if value_type == "enum" else "SDDS1\n") +
                  f'&parameter name=stateParameter, type={value_type}{mapping} &end\n'
                  '&column name=x, type=double &end\n'
                  '&column name=y, type=double &end\n'
                  f'&column name=state, type={value_type}{mapping} &end\n'
                  f'&data mode=ascii &end\n{value}\n3\n' +
                  ''.join(f'{x} {y} {value}\n' for x, y in ((1, 4), (2, 8), (3, 16))),
                  encoding="utf8")
  return path


def label_options(target, edit=None):
  selection = "-column=state,y" if target == "enumeratedScales" else "-column=x,y"
  if target == "enumeratedScales":
    return (selection, f"-enumeratedScales=editCommand={edit}") if edit else (selection,)
  if target == "legend":
    option, qualifier = "-legend=parameter=stateParameter", "editCommand"
  elif target == "string":
    option, qualifier = "-string=@stateParameter,xCoordinate=0,yCoordinate=1", "edit"
  elif target == "pointLabel":
    option, qualifier = "-pointLabel=state", "edit"
  else:
    option, qualifier = f"-{target}=@stateParameter", "edit"
  if edit is not None:
    option += f",{qualifier}={edit}"
  return selection, option


@pytest.mark.parametrize("target", TARGETS)
@pytest.mark.parametrize("value_type", ["enum", "string"])
@pytest.mark.parametrize("label,edit", [
    pytest.param("a" * 1000, "e24i/Z/", id="terminating-nul"),
    pytest.param("aa", "e1600i/Z/", id="large-insert"),
    pytest.param("a" * 600, "%g/a/aa/", id="replacement"),
    pytest.param("a" * 600, "a600k2y", id="yank"),
    pytest.param("a" * 1000, "e24(i/Z/)", id="repeated-sequence"),
    pytest.param("a" * 1000, "e24i/Z/a1024d", id="intermediate-overflow"),
    pytest.param("é" * 511, "e2i/Z/", id="utf8-bytes"),
])
def test_expanded_label_edit_is_rejected(tmp_path, target, value_type, label, edit):
  source = label_input(tmp_path / "source.sdds", value_type, label)
  result = run_plot(source, tmp_path / "invalid.json", *label_options(target, edit),
                    check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "unable to edit label" in result.stderr.lower()
  assert "too long" in result.stderr.lower()


@pytest.mark.parametrize("target", TARGETS)
@pytest.mark.parametrize("value_type", ["enum", "string"])
@pytest.mark.parametrize("label,edit,expected", [
    pytest.param("a" * 1000, "e23i/Z/", "a" * 1000 + "Z" * 23, id="insert"),
    pytest.param("a" * 511, "%g/a/aa/ei/Z/", "a" * 1022 + "Z", id="replacement"),
    pytest.param("a" * 341, "a341k3y", "a" * 1023, id="yank"),
    pytest.param("a" * 1000, "e23(i/Z/)", "a" * 1000 + "Z" * 23, id="repeated-sequence"),
    pytest.param("é" * 511, "ei/Z/", "é" * 511 + "Z", id="utf8-bytes"),
])
def test_label_edit_at_workspace_limit_succeeds(tmp_path, target, value_type,
                                               label, edit, expected):
  assert len(expected.encode("utf8")) == 1023
  source = label_input(tmp_path / "source.sdds", value_type, label)
  doc = run_plot(source, tmp_path / "boundary.json", *label_options(target, edit))
  layout = doc["plots"][0]["layout"]
  if target in ("title", "topline"):
    assert layout[target] == expected
  elif target in ("xlabel", "ylabel"):
    assert layout[target[0] + "axis"]["title"] == expected
  elif target == "legend":
    assert traces(doc)[0]["name"] == expected
  elif target == "enumeratedScales":
    assert layout["xaxis"]["ticktext"] == [expected]


@pytest.mark.parametrize("target", ["string", "pointLabel"])
@pytest.mark.parametrize("value_type", ["enum", "string"])
def test_native_label_edit_matches_literal_at_workspace_limit(tmp_path, target, value_type):
  # These labels are rendered natively but are not included by the JSON driver.
  source = label_input(tmp_path / "source.sdds", value_type, "a" * 1000)
  expected = label_input(tmp_path / "expected.sdds", value_type, "a" * 1000 + "Z" * 23)
  fixed_labels = ("-title=Boundary control", "-topline=Boundary control")
  edited = native_png_data(tmp_path, "edited", [source], *fixed_labels,
                           *label_options(target, "e23i/Z/"))
  literal = native_png_data(tmp_path, "literal", [expected], *fixed_labels,
                            *label_options(target))
  assert edited == literal


@pytest.mark.parametrize("target", TARGETS)
@pytest.mark.parametrize("value_type", ["enum", "string"])
def test_failed_label_edit_is_reported(tmp_path, target, value_type):
  source = label_input(tmp_path / "source.sdds", value_type, "aa")
  result = run_plot(source, tmp_path / "invalid.json", *label_options(target, "i"),
                    check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "unable to edit label" in result.stderr.lower()


def test_expanded_filename_label_edit_is_rejected(tmp_path):
  source = label_input(tmp_path / "source.sdds", "enum", "aa")
  result = run_plot(source, tmp_path / "invalid.json", "-column=x,y",
                    "-filenamesOnTopline=editCommand=e1600i/Z/", check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "unable to edit label" in result.stderr.lower()


def test_filename_label_edit_succeeds(tmp_path):
  source = label_input(tmp_path / "source.sdds", "enum", "aa")
  doc = run_plot(source, tmp_path / "filename.json", "-column=x,y",
                 "-filenamesOnTopline=editCommand=ei/_suffix/")
  assert str(source) + "_suffix" in doc["plots"][0]["layout"]["topline"]
