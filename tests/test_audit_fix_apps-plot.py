"""Regressions for the sddsplot, sddscontour and pseudo-inverse audit fixes."""

import json
import struct
import subprocess

import pytest

from sdds_test_utils import BIN_DIR, IS_WINDOWS, PLATFORM_ID, ROOT_DIR


SDDSPLOT = BIN_DIR / "sddsplot"
SDDSCONTOUR = BIN_DIR / "sddscontour"
SDDSPSEUDOINVERSE = BIN_DIR / "sddspseudoinverse"
needs_plot = pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")


def run(*args, check=True):
  result = subprocess.run([str(arg) for arg in args], capture_output=True, text=True, timeout=60)
  if check:
    assert result.returncode == 0, f"{args!r}\n{result.stdout}\n{result.stderr}"
  return result


def stream(path, kind, names, *options):
  return run(BIN_DIR / "sdds2stream", path, f"-{kind}={names}", *options).stdout.split()


def column_names(path):
  return run(BIN_DIR / "sddsquery", path, "-columnlist").stdout.split()


def plot_json(source, output, *options, check=True):
  result = run(SDDSPLOT, *source, *options, "-device=json", f"-output={output}", check=check)
  return (json.loads(output.read_text()) if check else None), result


def traces(doc):
  return [trace for plot in doc["plots"] for trace in plot["traces"]]


def write_xy(path, version=1, pages=2, rows=5):
  """Binary x/y data with one parameter per page, as written by a logging program."""
  data = bytearray((f"SDDS{version}\n!# little-endian\n&parameter name=p, type=long &end\n"
                    "&column name=x, type=double &end\n&column name=y, type=double &end\n"
                    "&data mode=binary, &end\n").encode())
  starts = []
  for page in range(pages):
    starts.append(len(data))
    data += struct.pack("<ii", rows, page + 1)
    for row in range(rows):
      data += struct.pack("<2d", row, 10 * (page + 1) + row)
  path.write_bytes(bytes(data))
  return bytes(data), starts


def write_labels(path):
  path.write_text("SDDS1\n&parameter name=R1, type=string &end\n"
                  "&column name=x, type=double, units=m &end\n&column name=y, type=double &end\n"
                  "&data mode=ascii, &end\nabcabc\n3\n1 2\n2 3\n3 5\n")
  return path


# ---------------------------------------------------------------------------
# M1: sddsplot plots the pages already read from a truncated SDDS1-5 file.

@needs_plot
@pytest.mark.parametrize("cut,points", [("parameter", 5), ("row", 8)])
def test_truncated_legacy_file_plots_pages_already_read(tmp_path, cut, points):
  source = tmp_path / "live.sdds"
  data, starts = write_xy(source)
  # Cut inside page 2's parameter (unrecoverable) or inside its fourth row (recoverable).
  source.write_bytes(data[:starts[1] + 6] if cut == "parameter" else data[:-20])
  doc, result = plot_json([source], tmp_path / "live.json", "-column=x,y")
  assert [len(trace["x"]) for trace in traces(doc)] == [points]


@needs_plot
def test_recovered_partial_page_still_supports_definitions(tmp_path):
  source = tmp_path / "live.sdds"
  data, _ = write_xy(source)
  source.write_bytes(data[:-20])
  doc, _ = plot_json([source], tmp_path / "live.json", "-column=x,y", "-split=page",
                     "-define=parameter,R2,p 2 *,type=long", "-legend=parameter=R2")
  assert [(trace["name"], len(trace["x"])) for trace in traces(doc)] == [("2", 5), ("4", 3)]


@needs_plot
def test_unreadable_first_page_warns_and_plots_other_files(tmp_path):
  broken, good = tmp_path / "broken.sdds", tmp_path / "good.sdds"
  data, starts = write_xy(broken)
  broken.write_bytes(data[:starts[0] + 6])
  write_xy(good, pages=1)
  doc, result = plot_json([broken, good], tmp_path / "files.json", "-column=x,y")
  assert [len(trace["x"]) for trace in traces(doc)] == [5]
  assert f"problem reading data from file {broken}" in result.stderr
  assert "SDDS_ReadBinaryParameters" in result.stderr


@needs_plot
def test_truncated_sdds6_file_still_fails(tmp_path):
  # SDDS6 truncation and invalid categorical data share one failure path in the reader.
  source = tmp_path / "live6.sdds"
  data, starts = write_xy(source, version=6)
  source.write_bytes(data[:starts[1] + 6])
  _, result = plot_json([source], tmp_path / "live6.json", "-column=x,y", check=False)
  assert result.returncode > 0
  assert "parameter" in result.stderr.lower()


# ---------------------------------------------------------------------------
# M12: malformed label edits keep the legacy partial result instead of aborting.

@needs_plot
def test_malformed_label_edits_keep_legacy_partial_results(tmp_path):
  source = write_labels(tmp_path / "labels.sdds")
  doc, result = plot_json([source], tmp_path / "labels.json", "-column=x,y",
                          "-xlabel=edit=%/x/X", "-ylabel=edit=%/y/Y/%/Y/Z",
                          "-legend=parameter=R1,edit=%/a/A/%/b/B", "-topline=@R1,edit=%/c/C",
                          "-title=@R1,edit=%/a/A/")
  layout = doc["plots"][0]["layout"]
  # Results computed by the legacy edit_string(): edits before the bad command still apply.
  assert layout["xaxis"]["title"] == "x (m)"
  assert layout["yaxis"]["title"] == "Y"
  assert traces(doc)[0]["name"] == "Abcabc"
  assert layout["topline"] == "abcabc"
  assert layout["title"] == "Abcabc"
  assert result.stderr.count("warning: invalid label edit command") == 4


@needs_plot
def test_label_edit_overflow_is_still_fatal(tmp_path):
  source = write_labels(tmp_path / "labels.sdds")
  _, result = plot_json([source], tmp_path / "overflow.json", "-column=x,y",
                        "-xlabel=edit=e1100i/Z/", check=False)
  assert result.returncode > 0
  assert "unable to edit label" in result.stderr.lower() and "too long" in result.stderr.lower()


@pytest.mark.skipif(not SDDSCONTOUR.exists(), reason="sddscontour not built")
@pytest.mark.parametrize("edit,warns", [("%/a/A", True), ("%/a/A/", False)])
def test_contour_parameter_label_edit_tolerates_malformed_command(tmp_path, edit, warns):
  source = tmp_path / "image.sdds"
  source.write_text("SDDS1\n&parameter name=R1, type=string &end\n&column name=Index, type=long &end\n"
                    + "".join(f"&column name=HLine{i}, type=double &end\n" for i in range(3))
                    + "&data mode=ascii &end\nabcabc\n3\n0 1 2 3\n1 2 3 4\n2 3 4 5\n")
  output = tmp_path / "image.ps"
  result = run(SDDSCONTOUR, source, "-columnmatch=Index,HLine*", "-shade",
               f"-xlabel=@R1,edit={edit}", "-device=postscript", f"-output={output}")
  assert output.stat().st_size > 0
  assert ("warning: invalid label edit command" in result.stderr) == warns


@pytest.mark.skipif(not SDDSCONTOUR.exists(), reason="sddscontour not built")
def test_contour_parameter_label_overflow_is_still_fatal(tmp_path):
  source = tmp_path / "image.sdds"
  source.write_text("SDDS1\n&parameter name=R1, type=string &end\n&column name=Index, type=long &end\n"
                    "&column name=HLine0, type=double &end\n&column name=HLine1, type=double &end\n"
                    "&data mode=ascii &end\nabc\n2\n0 1 2\n1 2 3\n")
  result = run(SDDSCONTOUR, source, "-columnmatch=Index,HLine*", "-shade",
               "-xlabel=@R1,edit=e1100i/Z/", "-device=postscript", f"-output={tmp_path / 'x.ps'}",
               check=False)
  assert result.returncode > 0
  assert "too long" in result.stderr


# ---------------------------------------------------------------------------
# Pre-existing: -define=parameter crashed because the new value had no storage.

@needs_plot
def test_defined_parameter_can_label_every_page_of_every_file(tmp_path):
  first, second = tmp_path / "a.sdds", tmp_path / "b.sdds"
  write_xy(first)
  write_xy(second, pages=3)
  doc, _ = plot_json([first, second], tmp_path / "define.json", "-column=x,y", "-split=page",
                     "-define=parameter,R2,p 2 *", "-define=parameter,R3,R2 1 +,type=long",
                     "-legend=parameter=R3")
  assert [trace["name"] for trace in traces(doc)] == ["3", "5", "3", "5", "7"]


@needs_plot
def test_defined_parameter_with_bad_equation_reports_rpn_error(tmp_path):
  source = write_labels(tmp_path / "labels.sdds")
  _, result = plot_json([source], tmp_path / "bad.json", "-column=x,y",
                        "-define=parameter,R2,Run 2 *", "-legend=parameter=R2", check=False)
  assert result.returncode == 1, result.stderr
  assert "rpn" in result.stderr.lower()


# ---------------------------------------------------------------------------
# Low: JSON axis titles are the drawn labels, which include the units.

@needs_plot
def test_json_axis_titles_include_units(tmp_path):
  source = tmp_path / "units.sdds"
  source.write_text("SDDS1\n&column name=x, type=double, units=m &end\n"
                    "&column name=y, type=double, units=s &end\n&data mode=ascii, &end\n2\n1 2\n2 3\n")
  doc, _ = plot_json([source], tmp_path / "units.json", "-column=x,y")
  layout = doc["plots"][0]["layout"]
  assert (layout["xaxis"]["title"], layout["xaxis"]["units"]) == ("x (m)", "m")
  assert (layout["yaxis"]["title"], layout["yaxis"]["units"]) == ("y (s)", "s")


# ---------------------------------------------------------------------------
# H2: complex pseudo-inverse names keep the legacy widened digit count.

@pytest.fixture(scope="module")
def complex_name_driver():
  component = ROOT_DIR / "SDDSaps/pseudoInverse"
  build = subprocess.run(["make", "-C", str(component), "enum-complex-name-test-driver"],
                         capture_output=True, text=True)
  assert build.returncode == 0, build.stdout + build.stderr
  return component / f"O.{PLATFORM_ID}" / ("test_complex_names.exe" if IS_WINDOWS else "test_complex_names")


def write_complex(path, rows):
  lines = ["SDDS1", *(f"&column name={part}{name}, type=double &end"
                      for name in "AB" for part in ("Real", "Imag")),
           "&data mode=ascii &end", str(rows)]
  lines += [f"{1 + (row == 0)} 0 {row % 7} {row % 3}" for row in range(rows)]
  path.write_text("\n".join(lines) + "\n")


@pytest.mark.parametrize("root,digits,first,last", [
    ("-", None, "Column0000", "Column1004"),
    ("R", None, "R0000", "R1004"),
    ("R", "0", "R0000", "R1004"),
    ("R", "-2", "R0000", "R1004"),
    ("R", "6", "R000000", "R001004"),
])
def test_complex_generated_names_share_legacy_width(complex_name_driver, tmp_path, root, digits,
                                                    first, last):
  source = tmp_path / "matrix.sdds"
  write_complex(source, 1005)
  arguments = [complex_name_driver, source, tmp_path / "names.sdds", tmp_path / "rebuilt.sdds", "-", root]
  run(*arguments, *([digits] if digits else []))
  names = stream(tmp_path / "names.sdds", "column", "RowName")
  assert len(names) == 1005 and len({len(name) for name in names}) == 1
  assert (names[0], names[500], names[-1]) == (first, first[:-3] + "500", last)


@pytest.mark.parametrize("rows,expected", [(3, "R002"), (999, "R998"), (1000, "R0999")])
def test_complex_generated_name_width_tracks_row_count(complex_name_driver, tmp_path, rows, expected):
  source = tmp_path / "matrix.sdds"
  write_complex(source, rows)
  run(complex_name_driver, source, tmp_path / "names.sdds", tmp_path / "rebuilt.sdds", "-", "R")
  assert stream(tmp_path / "names.sdds", "column", "RowName")[-1] == expected


# ---------------------------------------------------------------------------
# sddspseudoinverse: multiply matrices with fewer pages, and safe generated names.

needs_pinv = pytest.mark.skipif(not SDDSPSEUDOINVERSE.exists(), reason="sddspseudoinverse not built")


def write_real(path, columns, pages, string_column=None):
  """Write pages as lists of rows; a string column, if any, holds N<row>."""
  lines = ["SDDS1"]
  if string_column:
    lines.append(f"&column name={string_column}, type=string &end")
  lines += [f"&column name={name}, type=double &end" for name in columns]
  lines.append("&data mode=ascii, &end")
  for page in pages:
    lines.append(str(len(page)))
    lines += [" ".join(([f"N{row}"] if string_column else []) + [repr(value) for value in values])
              for row, values in enumerate(page)]
  path.write_text("\n".join(lines) + "\n")


def input_pages():
  return [[[1.0 + (r == c) * 3 + 0.1 * r * c + p for c in range(3)] for r in range(4)] for p in range(2)]


@needs_pinv
@pytest.mark.parametrize("invert", [False, True])
def test_single_page_multiply_matrix_is_reused_for_later_pages(tmp_path, invert):
  np = pytest.importorskip("numpy")
  source, multiply, output = tmp_path / "r.sdds", tmp_path / "m.sdds", tmp_path / "out.sdds"
  pages = input_pages()
  write_real(source, "ABC", pages)
  if invert:
    matrix = [[1.0 + r + 0.5 * c for c in range(3)] for r in range(2)]
    write_real(multiply, "ABC", [matrix], string_column="Act")
  else:
    matrix = [[1.0 + r * c for c in range(2)] for r in range(4)]
    write_real(multiply, ["M0", "M1"], [matrix])
  run(SDDSPSEUDOINVERSE, source, output, f"-multiplyMatrix={multiply}" + (",invert" if invert else ""))
  names = [name for name in column_names(output) if name not in ("Act", "OldColumnNames")]
  for page, values in enumerate(pages, start=1):
    inverse = np.linalg.pinv(np.array(values))
    expected = np.array(matrix) @ inverse if invert else inverse @ np.array(matrix)
    got = np.array([float(x) for x in stream(output, "column", ",".join(names), f"-page={page}")])
    assert np.allclose(got.reshape(expected.shape), expected, rtol=1e-12, atol=1e-12)


@needs_pinv
def test_paired_multiply_pages_are_used_in_order(tmp_path):
  np = pytest.importorskip("numpy")
  source, multiply, output = tmp_path / "r.sdds", tmp_path / "m.sdds", tmp_path / "out.sdds"
  pages = input_pages()
  write_real(source, "ABC", pages)
  matrices = [[[1.0 + r * c + 2 * p for c in range(2)] for r in range(4)] for p in range(2)]
  write_real(multiply, ["M0", "M1"], matrices)
  run(SDDSPSEUDOINVERSE, source, output, f"-multiplyMatrix={multiply}")
  names = [name for name in column_names(output) if name not in ("Act", "OldColumnNames")]
  for page, (values, matrix) in enumerate(zip(pages, matrices), start=1):
    expected = np.linalg.pinv(np.array(values)) @ np.array(matrix)
    got = np.array([float(x) for x in stream(output, "column", ",".join(names), f"-page={page}")])
    assert np.allclose(got.reshape(expected.shape), expected, rtol=1e-12, atol=1e-12)


@needs_pinv
@pytest.mark.parametrize("invert", [False, True])
def test_multi_page_multiply_matrix_with_too_few_pages_is_an_error(tmp_path, invert):
  source, multiply, output = tmp_path / "r.sdds", tmp_path / "m.sdds", tmp_path / "out.sdds"
  pages = input_pages() + [input_pages()[0]]
  write_real(source, "ABC", pages)
  if invert:
    matrix = [[1.0 + r + 0.5 * c for c in range(3)] for r in range(2)]
    write_real(multiply, "ABC", [matrix, matrix], string_column="Act")
  else:
    matrix = [[1.0 + r * c for c in range(2)] for r in range(4)]
    write_real(multiply, ["M0", "M1"], [matrix, matrix])
  result = run(SDDSPSEUDOINVERSE, source, output, f"-multiplyMatrix={multiply}" + (",invert" if invert else ""),
               check=False)
  assert result.returncode != 0
  assert "fewer pages than the input file" in result.stdout + result.stderr


@needs_pinv
def test_empty_multiply_matrix_file_is_an_error(tmp_path):
  source, multiply, output = tmp_path / "r.sdds", tmp_path / "m.sdds", tmp_path / "out.sdds"
  write_real(source, "ABC", input_pages())
  write_real(multiply, ["M0", "M1"], [])
  result = run(SDDSPSEUDOINVERSE, source, output, f"-multiplyMatrix={multiply}", check=False)
  assert result.returncode != 0
  assert "no data pages" in result.stdout + result.stderr


@needs_pinv
def test_root_names_for_many_multiply_columns(tmp_path):
  source, multiply, output = tmp_path / "r.sdds", tmp_path / "m.sdds", tmp_path / "out.sdds"
  write_real(source, "AB", [[[1.0 + (r == c) * 2 + 0.1 * r for c in range(2)] for r in range(3)]])
  write_real(multiply, [f"M{i}" for i in range(1005)], [[[float(r + c) for c in range(1005)] for r in range(3)]])
  run(SDDSPSEUDOINVERSE, source, output, f"-multiplyMatrix={multiply}", "-root=R")
  names = column_names(output)
  assert names[:3] == ["OldColumnNames", "R000", "R001"] and names[-1] == "R1004"


@needs_pinv
def test_singular_vector_names_for_many_rows(tmp_path):
  source, output, umatrix = tmp_path / "r.sdds", tmp_path / "out.sdds", tmp_path / "u.sdds"
  write_real(source, "AB", [[[float(r == 0) + r * 1e-3, float(r == 1) + (r % 5)] for r in range(1005)]])
  run(SDDSPSEUDOINVERSE, source, output, "-root=Q", "-digits=0", f"-uMatrix={umatrix}")
  assert column_names(output)[1:3] == ["Q0000", "Q0001"]
  assert column_names(umatrix)[:3] == ["OriginalRows", "SV0000", "SV0001"]
