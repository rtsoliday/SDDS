"""Regressions for the SDDS6 enum audit fixes in combining/selection tools."""

import subprocess

import pytest

from sdds_test_utils import BIN_DIR


TOOLS = ("sddscombine", "sddsselect", "sddscorrelate", "sddsvslopes", "sddsdiff",
         "sddshist", "sddsrowstats", "sddsmpfit", "sddsmatrixmult", "sddsmxref",
         "sddschanges", "sdds2stream", "sddsquery", "sddscheck")
pytestmark = pytest.mark.skipif(not all((BIN_DIR / tool).exists() for tool in TOOLS),
                                reason="tools not built")

MAPPING = "0:OFF,1:ON,2:FAULT"


def run(tool, *args, check=True):
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)], capture_output=True,
                          text=True, encoding="utf8", timeout=60)
  if check:
    assert result.returncode == 0, result.stderr
  return result


def write_ascii(path, parameters=(), columns=(), pages=(), arrays=(), version=None):
  """Write an ASCII SDDS file.

  parameters/columns/arrays are lists of (name, type[, extra]) and each page is a dict with
  optional "parameters" (list of values), "arrays" (list of value lists) and "rows"
  (list of row tuples).
  """
  enum = any(item[1] == "enum" for item in (*parameters, *columns, *arrays))
  lines = [version or ("SDDS6" if enum else "SDDS1")]
  for kind, items in (("parameter", parameters), ("array", arrays), ("column", columns)):
    for item in items:
      extra = f", {item[2]}" if len(item) > 2 else ""
      lines.append(f"&{kind} name={item[0]}, type={item[1]}{extra} &end")
  lines.append("&data mode=ascii, no_row_counts=0 &end")
  for page in pages:
    lines.extend(str(value) for value in page.get("parameters", ()))
    for values in page.get("arrays", ()):
      lines.append(str(len(values)))
      if values:
        lines.append(" ".join(map(str, values)))
    rows = page.get("rows", ())
    lines.append(str(len(rows)))
    lines.extend(" ".join(map(str, row)) for row in rows)
  path.write_text("\n".join(lines) + "\n")
  return path


def stream(path, *selection):
  return run("sdds2stream", path, *selection, "-noquotes").stdout.split()


def values(path, *selection):
  return [float(value) for value in stream(path, *selection)]


def pages(path):
  return int(run("sdds2stream", path, "-npages=bare").stdout.split()[0])


# M4 and the file-boundary -merge comparison (sddscombine)

def make_keyed(tmp_path, name, key, values, ptype="long", extra=""):
  return write_ascii(tmp_path / name, parameters=[("p", ptype, extra)] if extra else [("p", ptype)],
                     columns=[("x", "double")],
                     pages=[{"parameters": [key], "rows": [(v,) for v in values]}])


def test_combine_append_merge_when_destination_lacks_parameter(tmp_path):
  b1 = make_keyed(tmp_path, "b1.sdds", 1, (1.1, 1.2))
  b2 = make_keyed(tmp_path, "b2.sdds", 1, (2.1, 2.2))
  b3 = make_keyed(tmp_path, "b3.sdds", 2, (3.1, 3.2))
  b4 = make_keyed(tmp_path, "b4.sdds", 2, (4.1, 4.2))
  out0 = tmp_path / "out0.sdds"
  run("sddscombine", b1, b2, out0, "-merge=p", "-delete=parameter,p", "-overwrite")
  run("sddscombine", out0, b3, b4, "-append", "-merge=p", "-delete=parameter,p")
  assert run("sddscheck", out0).stdout.strip() == "ok"
  # Legacy behaviour: without the key in the destination, the first appended file
  # continues the destination's last page.
  assert pages(out0) == 1
  assert values(out0, "-column=x") == [1.1, 1.2, 2.1, 2.2, 3.1, 3.2, 4.1, 4.2]


def test_combine_append_merge_enum_key_absent_from_destination(tmp_path):
  extra = f'enum_values="{MAPPING}"'
  b1 = make_keyed(tmp_path, "b1.sdds", 1, (1.0,), "enum", extra)
  b2 = make_keyed(tmp_path, "b2.sdds", 1, (2.0,), "enum", extra)
  b3 = make_keyed(tmp_path, "b3.sdds", 2, (3.0,), "enum", extra)
  b4 = make_keyed(tmp_path, "b4.sdds", 0, (4.0,), "enum", extra)
  out0 = tmp_path / "out0.sdds"
  run("sddscombine", b1, b2, out0, "-merge=p", "-delete=parameter,p", "-overwrite")
  run("sddscombine", out0, b3, b4, "-append", "-merge=p", "-delete=parameter,p")
  assert run("sddscheck", out0).stdout.strip() == "ok"
  assert pages(out0) == 2
  assert values(out0, "-column=x", "-page=1") == [1, 2, 3]
  assert values(out0, "-column=x", "-page=2") == [4]


@pytest.mark.parametrize("ptype,keys", [("long", (1, 2, 2, 3)), ("double", (1.5, 2.5, 2.5, 3.5)),
                                        ("string", ("a", "b", "b", "c"))])
def test_combine_merge_compares_across_file_boundaries(tmp_path, ptype, keys):
  inputs = [make_keyed(tmp_path, f"k{i}.sdds", key, (float(i),), ptype)
            for i, key in enumerate(keys)]
  output = tmp_path / "out.sdds"
  run("sddscombine", *inputs, output, "-merge=p")
  assert pages(output) == 3
  assert values(output, "-column=x", "-page=2") == [1, 2]


# M5 (sddsselect)

@pytest.mark.parametrize("second_type,second_extra,second_value",
                         [("enum", 'enum_values="0:A,1:B"', 1), ("string", "", "text")])
def test_select_ignores_input2_parameter_definitions(tmp_path, second_type, second_extra, second_value):
  first = write_ascii(tmp_path / "one.sdds",
                      parameters=[("p", "enum", f'enum_values="{MAPPING}"')],
                      arrays=[("a", "enum", f'enum_values="{MAPPING}"')],
                      columns=[("name", "string")],
                      pages=[{"parameters": [2], "arrays": [[1, 2]], "rows": [("x",), ("y",)]}])
  second = write_ascii(tmp_path / "two.sdds",
                       parameters=[("p", second_type, second_extra)] if second_extra else
                       [("p", second_type)],
                       arrays=[("a", "long")],
                       columns=[("name", "string")],
                       pages=[{"parameters": [second_value], "arrays": [[7]], "rows": [("y",)]}])
  output = tmp_path / "out.sdds"
  run("sddsselect", first, second, output, "-match=name")
  assert stream(output, "-column=name") == ["y"]
  assert stream(output, "-parameter=p") == ["FAULT"]
  assert stream(output, "-array=a") == ["ON", "FAULT"]


# M6 (sddscorrelate)

def correlate_input(tmp_path):
  return write_ascii(tmp_path / "corr.sdds",
                     columns=[("y", "double"), ("z", "double"), ("name", "string"),
                              ("state", "enum", f'enum_values="{MAPPING}"')],
                     pages=[{"rows": [(1, 2, "a", 0), (2, 4.1, "b", 1), (3, 6.3, "c", 2),
                                      (4, 7.9, "d", 1)]}])


@pytest.mark.parametrize("extra", ["name", "absent"])
def test_correlate_drops_missing_and_string_columns(tmp_path, extra):
  output = tmp_path / "out.sdds"
  run("sddscorrelate", correlate_input(tmp_path), output, f"-columns=y,z,{extra}")
  assert stream(output, "-column=CorrelatePair") == ["y.z"]


def test_correlate_rejects_explicit_enum_column(tmp_path):
  result = run("sddscorrelate", correlate_input(tmp_path), tmp_path / "out.sdds",
               "-columns=y,z,state", check=False)
  assert result.returncode != 0
  assert "enum" in result.stderr


def test_correlate_withonly_enum_rejected_missing_dropped(tmp_path):
  source = correlate_input(tmp_path)
  result = run("sddscorrelate", source, tmp_path / "a.sdds", "-withOnly=state", check=False)
  assert result.returncode != 0
  assert "enum" in result.stderr
  run("sddscorrelate", source, tmp_path / "b.sdds", "-withOnly=name")


# M7 (sddsvslopes)

def vslopes_input(tmp_path):
  columns = [("Rootname", "string"), ("a", "double"), ("l", "long"),
             ("state", "enum", f'enum_values="{MAPPING}"')]
  data = []
  for t in (0, 1, 2):
    data.append({"parameters": [float(t)],
                 "rows": [("r1", 1 + 2 * t, 3 * t, t % 3), ("r2", 5 - t, 10 + 4 * t, 1)]})
  return write_ascii(tmp_path / "vslopes.sdds", parameters=[("t", "double")], columns=columns,
                     pages=data)


def test_vslopes_long_columns_default_and_explicit(tmp_path):
  source = vslopes_input(tmp_path)
  for options in ((), ("-columns=l",), ("-columns=*",)):
    output = tmp_path / "out.sdds"
    run("sddsvslopes", source, output, "-independentVariable=t", *options)
    assert values(output, "-column=lSlope") == pytest.approx([3.0, 4.0])
    assert [float(v) for v in stream(output, "-column=lIntercept")] == pytest.approx([0.0, 10.0], abs=1e-12)
    names = run("sddsquery", output, "-columnList").stdout.split()
    assert "stateSlope" not in names
  output = tmp_path / "default.sdds"
  run("sddsvslopes", source, output, "-independentVariable=t")
  assert values(output, "-column=aSlope") == pytest.approx([2.0, -1.0])


def test_vslopes_rejects_explicit_enum_column(tmp_path):
  result = run("sddsvslopes", vslopes_input(tmp_path), tmp_path / "out.sdds",
               "-independentVariable=t", "-columns=state", check=False)
  assert result.returncode != 0
  assert "enum" in result.stderr


# sddsdiff

def enum_column_file(path, codes, mapping=MAPPING):
  return write_ascii(path, columns=[("x", "double"), ("state", "enum", f'enum_values="{mapping}"')],
                     pages=[{"rows": [(float(i), code) for i, code in enumerate(codes)]}])


def test_diff_absolute_with_enum_columns(tmp_path):
  one = enum_column_file(tmp_path / "one.sdds", (0, 1, 2))
  same = enum_column_file(tmp_path / "same.sdds", (0, 1, 2))
  other = enum_column_file(tmp_path / "other.sdds", (0, 2, 2))
  result = run("sddsdiff", one, same, "-absolute")
  assert "are identical" in result.stdout
  result = run("sddsdiff", one, other, "-absolute")
  assert "are different" in result.stderr
  assert "ON | FAULT" in result.stdout


def test_diff_compare_common_incompatible_mapping_is_different(tmp_path):
  one = enum_column_file(tmp_path / "one.sdds", (0, 1))
  two = enum_column_file(tmp_path / "two.sdds", (0, 1), mapping="0:OFF,1:OTHER,2:FAULT")
  result = run("sddsdiff", one, two, "-compareCommon")
  assert "Incompatible enum mapping for state" in result.stderr
  assert "are different" in result.stderr
  assert "identical" not in result.stdout


# sddshist abscissa format

@pytest.mark.parametrize("enum", [False, True])
def test_hist_abscissa_has_no_forced_format(tmp_path, enum):
  parameters = [("mode", "enum", f'enum_values="{MAPPING}"')] if enum else []
  source = write_ascii(tmp_path / "hist.sdds", parameters=parameters,
                       columns=[("x", "long", "format_string=%5ld")],
                       pages=[{"parameters": [1] if enum else [], "rows": [(1,), (2,), (2,), (3,)]}])
  output = tmp_path / "out.sdds"
  run("sddshist", source, output, "-dataColumn=x", "-bins=3")
  assert run("sddscheck", output).stdout.strip() == "ok"
  info = run("sddsquery", output).stdout
  assert "%21.15e" not in info
  assert "%5ld" not in info


# sddsrowstats / sddsmpfit explicit enum selections

def stats_input(tmp_path):
  return write_ascii(tmp_path / "stats.sdds",
                     columns=[("x", "double"), ("y", "double"), ("name", "string"),
                              ("state", "enum", f'enum_values="{MAPPING}"')],
                     pages=[{"rows": [(0, 1, "a", 0), (1, 3.1, "b", 1), (2, 4.9, "c", 2),
                                      (3, 7.0, "d", 1)]}])


def test_rowstats_rejects_explicit_enum(tmp_path):
  source = stats_input(tmp_path)
  result = run("sddsrowstats", source, tmp_path / "a.sdds", "-mean=m,state", check=False)
  assert result.returncode != 0
  assert "enum" in result.stderr
  # Wildcards keep selecting numeric columns only, and strings keep the legacy warning.
  run("sddsrowstats", source, tmp_path / "b.sdds", "-mean=m,*")
  assert [float(v) for v in stream(tmp_path / "b.sdds", "-column=m")] == pytest.approx([0.5, 2.05, 3.45, 5.0])
  result = run("sddsrowstats", source, tmp_path / "c.sdds", "-mean=m,name")
  assert "no match" in result.stderr


def test_mpfit_rejects_explicit_enum_and_reports_on_stderr(tmp_path):
  source = stats_input(tmp_path)
  result = run("sddsmpfit", source, tmp_path / "a.sdds", "-independent=x", "-dependent=state",
               "-terms=2", check=False)
  assert result.returncode != 0
  assert "enum" in result.stderr
  result = subprocess.run([str(BIN_DIR / "sddsmpfit"), str(source), "-pipe=out", "-independent=x",
                           "-dependent=y,name", "-terms=2"], capture_output=True, timeout=60)
  assert result.returncode == 0, result.stderr
  assert result.stdout.startswith(b"SDDS")
  assert b"Removing name" in result.stderr
  result = run("sddsmpfit", source, tmp_path / "b.sdds", "-independent=x", "-dependent=y,s*",
               "-terms=2")
  assert "stateFit" not in run("sddsquery", tmp_path / "b.sdds", "-columnList").stdout.split()


# sddsmatrixmult zero-row page

def test_matrixmult_zero_row_page(tmp_path):
  left = write_ascii(tmp_path / "left.sdds", columns=[("a", "double"), ("b", "double")],
                     pages=[{"rows": []}])
  right = write_ascii(tmp_path / "right.sdds", columns=[("c", "double"), ("d", "double")],
                      pages=[{"rows": [(1, 2), (3, 4)]}])
  output = tmp_path / "out.sdds"
  result = run("sddsmatrixmult", left, right, output, check=False)
  assert result.returncode >= 0, "crashed"
  assert result.returncode == 0, result.stderr
  assert run("sddsquery", output, "-columnList").stdout.split() == ["c", "d"]
  assert stream(output, "-rows=bare") == ["0"]


def test_matrixmult_zero_inner_dimension(tmp_path):
  left = write_ascii(tmp_path / "left.sdds", columns=[("s", "string")],
                     pages=[{"rows": [("u",), ("v",)]}])
  right = write_ascii(tmp_path / "right.sdds", columns=[("c", "double")], pages=[{"rows": []}])
  output = tmp_path / "out.sdds"
  result = run("sddsmatrixmult", left, right, output, check=False)
  assert result.returncode >= 0, "crashed"
  assert result.returncode == 0, result.stderr
  assert values(output, "-column=c") == [0, 0]


# sddsmxref take lists

def test_mxref_transfer_parameters_and_arrays(tmp_path):
  first = write_ascii(tmp_path / "one.sdds", parameters=[("p1", "double")],
                      arrays=[("a1", "double")], columns=[("k", "long")],
                      pages=[{"parameters": [1.5], "arrays": [[1, 2]], "rows": [(1,), (2,)]}])
  second = write_ascii(tmp_path / "two.sdds", parameters=[("p1", "double"), ("p2", "double")],
                       arrays=[("a1", "double"), ("a2", "double")],
                       columns=[("k", "long"), ("v", "double")],
                       pages=[{"parameters": [9.5, 2.5], "arrays": [[7], [8, 9]],
                               "rows": [(1, 10), (2, 20)]}])
  output = tmp_path / "out.sdds"
  run("sddsmxref", first, second, output, "-equate=k", "-take=v",
      "-transfer=parameter,p1,p2", "-transfer=array,a1,a2")
  assert values(output, "-parameter=p1") == [1.5]
  assert values(output, "-parameter=p2") == [2.5]
  assert values(output, "-array=a1") == [1, 2]
  assert values(output, "-array=a2") == [8, 9]
  assert values(output, "-column=v") == [10, 20]


# sddschanges wildcard copy over mixed types

def test_changes_wildcard_copy_mixed_types(tmp_path):
  columns = [("s", "string"), ("x", "double"), ("l", "long")]
  rows = lambda base: [("a", base, 1), ("b", base + 1, 2)]
  base = write_ascii(tmp_path / "base.sdds", columns=columns,
                     pages=[{"rows": rows(0)}, {"rows": rows(10)}, {"rows": rows(20)}])
  data = write_ascii(tmp_path / "data.sdds", columns=columns,
                     pages=[{"rows": rows(1)}, {"rows": rows(12)}, {"rows": rows(23)}])
  output = tmp_path / "out.sdds"
  run("sddschanges", data, output, "-changesIn=x", "-copy=*", f"-baseline={base}", "-parallelPages")
  assert stream(output, "-column=s") == ["a", "b"] * 3
  assert values(output, "-column=ChangeInx") == [1, 1, 2, 2, 3, 3]
  numeric_first = write_ascii(tmp_path / "numeric_first.sdds",
                              columns=[("x", "double"), ("s", "string")],
                              pages=[{"rows": [(1, "a")]}, {"rows": [(2, "b")]}])
  result = run("sddschanges", numeric_first, tmp_path / "bad.sdds", "-changesIn=*", check=False)
  assert result.returncode != 0
  assert "non-numeric" in result.stderr
