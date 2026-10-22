"""ENUM regressions for label consumers, failed reads, and input preservation."""

import gzip
import json
import lzma
import struct
import sys

import pytest

from test_sdds_enum_read_failures import ENCODINGS, enum_file, input_path, page_count, run


SINGLE_TOOLS = {
  "sddsdigfilter": ("-columns=x,y", "-proportional=2"),
  "sddspfit": ("-columns=x,y", "-terms=2"),
  "sddsmpfit": ("-independent=x", "-dependent=y", "-terms=2"),
  "sddsica": ("-pca",),
  "sddsremoveoffsets": ("-columns=y",),
  "sddstdrpeeling": ("-column=y", "-inputVoltage=100"),
  "sddstimeconvert": ("-breakdown=column,x,year=Year",),
  "sddspoly": (),
  "sddsdistest": ("-column=y", "-test=ks", "-gaussian"),
}
COPY_TOOLS = ("sddsdigfilter", "sddsremoveoffsets", "sddstdrpeeling",
              "sddstimeconvert", "sddspoly")
JOIN_TOOLS = ("sddsselect", "sddsmselect", "sddsmxref")


def options(tool, directory):
  if tool != "sddspoly":
    return SINGLE_TOOLS[tool]
  poly = directory / "polynomial.sdds"
  poly.write_text('SDDS1\n&column name=coef, type=double &end\n'
                  '&column name=power, type=long &end\n'
                  '&data mode=ascii &end\n2\n1 0\n2 1\n')
  return (f"-evaluate=filename={poly},output=result,coefficients=coef,input0=x,power0=power",)


def assert_enum_error(result):
  # A signal or an allocator abort is not an acceptable validation failure.
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower()
  assert "42" in result.stderr


@pytest.mark.parametrize("tool", SINGLE_TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_single_input_reports_enum_errors(tmp_path, tool, encoding, kind, prefix):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  enum_file(source, encoding, invalid=kind, valid_pages=int(prefix))
  assert_enum_error(run(tool, source, output, *options(tool, tmp_path), check=False))


@pytest.mark.parametrize("tool", SINGLE_TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("pages", [1, 2])
def test_single_input_normal_eof_preserves_enum_data(tmp_path, tool, encoding, pages):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  enum_file(source, encoding, valid_pages=pages)
  result = run(tool, source, output, *options(tool, tmp_path))
  assert not result.stderr
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert page_count(output) == pages
  if tool in COPY_TOOLS:
    assert run("sdds2stream", output, "-parameter=mode", "-noquotes").stdout.split() == ["ON"] * pages
    assert run("sdds2stream", output, "-column=state", "-noquotes").stdout.split() == [
      "OFF", "ON", "ON", "FAULT", "OFF"] * pages
    assert run("sdds2stream", output, "-column=state", "-enumCodes", "-noquotes").stdout.split() == [
      "0", "10", "10", "-1", "0"] * pages
    assert run("sdds2stream", output, "-array=states", "-noquotes").stdout.split() == [
      "OFF", "ON", "FAULT", "ON", "OFF"] * pages


def test_polynomial_empty_input_cleans_up_safely(tmp_path):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  enum_file(source, "ascii")
  assert not run("sddspoly", source, output, *options("sddspoly", tmp_path)).stderr
  assert page_count(output) == 0


@pytest.mark.parametrize("filter_type", ["analogfilter", "digitalfilter"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_filter_coefficients_report_enum_page_errors(tmp_path, filter_type, encoding, kind):
  primary, coefficients, output = tmp_path / "primary.sdds", input_path(tmp_path, encoding), tmp_path / "output.sdds"
  enum_file(primary, "ascii", valid_pages=1)
  enum_file(coefficients, encoding, invalid=kind)
  assert_enum_error(run("sddsdigfilter", primary, output, "-columns=x,y",
                        f"-{filter_type}={coefficients},x,y", check=False))


def query_options(mode, directory):
  return {"summary": (), "list": ("-columnList",),
          "sdds": ("-columnList", f"-sddsOutput={directory / 'summary.sdds'}")}[mode]


@pytest.mark.parametrize("mode", ["summary", "list", "sdds"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_query_read_all_reports_enum_errors(tmp_path, mode, encoding, kind, prefix):
  source = input_path(tmp_path, encoding)
  enum_file(source, encoding, invalid=kind, valid_pages=int(prefix))
  assert_enum_error(run("sddsquery", source, "-readAll", *query_options(mode, tmp_path), check=False))


@pytest.mark.parametrize("mode", ["summary", "list", "sdds"])
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_query_read_all_accepts_normal_eof(tmp_path, mode, encoding):
  source = input_path(tmp_path, encoding)
  enum_file(source, encoding, valid_pages=2)
  assert not run("sddsquery", source, "-readAll", *query_options(mode, tmp_path)).stderr


def test_query_version_does_not_read_an_uninitialized_dataset(tmp_path):
  source = tmp_path / "source.sdds"
  enum_file(source, "ascii", valid_pages=1)
  assert run("sddsquery", source, "-version", "-readAll").stdout.strip() == "6"


@pytest.mark.parametrize("tool", JOIN_TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("side", ["primary", "secondary"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_two_input_tools_report_enum_errors(tmp_path, tool, encoding, side, kind, prefix):
  primary = input_path(tmp_path, encoding)
  secondary, output = tmp_path / ("reference" + primary.suffix), tmp_path / "output.sdds"
  enum_file(primary, encoding, valid_pages=2)
  enum_file(secondary, encoding, valid_pages=2)
  enum_file(primary if side == "primary" else secondary, encoding, invalid=kind, valid_pages=int(prefix))
  assert_enum_error(run(tool, primary, secondary, output, "-equate=x", check=False))


@pytest.mark.parametrize("tool", JOIN_TOOLS)
@pytest.mark.parametrize("encoding", ["ascii", "big-column"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_reused_secondary_page_reports_enum_error(tmp_path, tool, encoding, kind):
  primary, secondary, output = (tmp_path / name for name in ("input.sdds", "reference.sdds", "out.sdds"))
  enum_file(primary, encoding, valid_pages=2)
  enum_file(secondary, encoding, invalid=kind)
  assert_enum_error(run(tool, primary, secondary, output, "-equate=x", "-reuse=page", check=False))


@pytest.mark.parametrize("tool", ["sddsselect", "sddsmselect"])
@pytest.mark.parametrize("encoding", ["ascii", "big-column"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_invert_does_not_accept_failed_secondary_page(tmp_path, tool, encoding, kind, prefix):
  primary, secondary, output = (tmp_path / name for name in ("input.sdds", "reference.sdds", "out.sdds"))
  enum_file(primary, encoding, valid_pages=2)
  enum_file(secondary, encoding, invalid=kind, valid_pages=int(prefix))
  assert_enum_error(run(tool, primary, secondary, output, "-equate=x", "-invert", check=False))


@pytest.mark.parametrize("tool", JOIN_TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("secondary_pages", [1, 2])
@pytest.mark.parametrize("reuse", [False, True])
def test_two_input_normal_and_early_eof(tmp_path, tool, encoding, secondary_pages, reuse):
  primary = input_path(tmp_path, encoding)
  secondary, output = tmp_path / ("reference" + primary.suffix), tmp_path / "output.sdds"
  enum_file(primary, encoding, valid_pages=2)
  enum_file(secondary, encoding, valid_pages=secondary_pages)
  run(tool, primary, secondary, output, "-equate=x", *(["-reuse=page"] if reuse else []))
  pages = 2 if reuse else secondary_pages
  assert page_count(output) == pages
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert run("sdds2stream", output, "-column=state", "-noquotes").stdout.split() == [
    "OFF", "ON", "ON", "FAULT", "OFF"] * pages
  assert run("sdds2stream", output, "-parameter=mode", "-noquotes").stdout.split() == ["ON"] * pages


@pytest.mark.parametrize("tool", ["sddsselect", "sddsmselect"])
def test_invert_keeps_rows_after_normal_secondary_eof(tmp_path, tool):
  primary, secondary, output = (tmp_path / name for name in ("input.sdds", "reference.sdds", "out.sdds"))
  enum_file(primary, "ascii", valid_pages=2)
  enum_file(secondary, "ascii", valid_pages=1)
  run(tool, primary, secondary, output, "-equate=x", "-invert")
  assert page_count(output) == 2
  assert run("sdds2stream", output, "-rows=bare").stdout.split() == ["0", "5"]


@pytest.mark.parametrize("tool", JOIN_TOOLS)
@pytest.mark.parametrize("encoding", ["ascii", "big-column", "gzip", "xz", "lzma"])
@pytest.mark.parametrize("side", ["primary", "secondary"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_two_input_failures_preserve_in_place_input(tmp_path, tool, encoding, side, kind, prefix):
  primary = input_path(tmp_path, encoding)
  secondary = tmp_path / ("reference" + primary.suffix)
  enum_file(primary, encoding, valid_pages=2)
  enum_file(secondary, encoding, valid_pages=2)
  enum_file(primary if side == "primary" else secondary, encoding, invalid=kind, valid_pages=int(prefix))
  original = primary.read_bytes()
  assert_enum_error(run(tool, primary, secondary, "-equate=x", check=False))
  assert primary.read_bytes() == original


@pytest.mark.parametrize("tool", ["sddsremoveoffsets", "sddsica"])
@pytest.mark.parametrize("encoding", ["ascii", "big-column", "gzip"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_single_input_failures_preserve_in_place_input(tmp_path, tool, encoding, kind, prefix):
  source = input_path(tmp_path, encoding)
  enum_file(source, encoding, invalid=kind, valid_pages=int(prefix))
  original = source.read_bytes()
  assert_enum_error(run(tool, source, *options(tool, tmp_path), check=False))
  assert source.read_bytes() == original


def write_wire(path, encoding, declarations, ascii_body, binary_body):
  order = "big" if encoding.startswith("big") else "little"
  header = ('SDDS6\n' + declarations +
            f'&data mode={"ascii" if encoding == "ascii" else "binary"}, '
            f'endian={order}, column_major_order={int(encoding.endswith("column"))} &end\n')
  wire = header.encode() + (ascii_body.encode() if encoding == "ascii" else binary_body)
  if encoding == "gzip":
    wire = gzip.compress(wire)
  elif encoding in ("xz", "lzma"):
    wire = lzma.compress(wire, format=lzma.FORMAT_ALONE if encoding == "lzma" else lzma.FORMAT_XZ)
  path.write_bytes(wire)


def scalar(value_type, code, label, order):
  if value_type == "string":
    data = label.encode()
    return json.dumps(label, ensure_ascii=False), struct.pack(order + "i", len(data)) + data
  if value_type == "character":
    return "m", b"m"
  return str(code), struct.pack(order + ("d" if value_type == "double" else "i"), code)


def translation_file(path, encoding, old_type="enum", new_type="enum", invalid=None):
  order = ">" if encoding.startswith("big") else "<"
  declarations = ('&parameter name=mode, type=enum, enum_values="0:OFF,10:ON" &end\n'
                  '&array name=states, type=enum, enum_values="0:OFF,10:ON" &end\n')
  text, data = [], []
  for name, value_type, code, label in (("OldName", old_type, -2147483648, "y"),
                                       ("NewName", new_type, 2147483647, "Y_LABEL")):
    mapping = f', enum_values="{code}:{label}"' if value_type == "enum" else ''
    declarations += f'&column name={name}, type={value_type}{mapping} &end\n'
    value, raw = scalar(value_type, 42 if invalid == "column" and name == "OldName" else code, label, order)
    text.append(value)
    data.append(raw)
  mode, array = (42 if invalid == "parameter" else 10), (42 if invalid == "array" else 0)
  write_wire(path, encoding, declarations, f'{mode}\n2\n{array} 10\n1\n' + ' '.join(text) + '\n',
             struct.pack(order + "5i", 1, mode, 2, array, 10) + b''.join(data))


@pytest.mark.skipif(sys.platform == "win32", reason="HTML/LaTeX translation is unavailable on Windows")
@pytest.mark.parametrize("mode", ["latexFormat", "htmlFormat"])
@pytest.mark.parametrize("old_type,new_type", [("string", "string"), ("enum", "string"),
                                              ("string", "enum"), ("enum", "enum")])
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_printout_translation_decodes_enum_labels(tmp_path, mode, old_type, new_type, encoding):
  primary, names = tmp_path / "source.sdds", input_path(tmp_path, encoding)
  primary.write_text('SDDS1\n&column name=y, type=double &end\n&data mode=ascii &end\n2\n1\n2\n1\n3\n')
  translation_file(names, encoding, old_type, new_type)
  result = run("sddsprintout", primary, "-columns=y", f"-{mode}=translate={names}")
  assert not result.stderr
  assert result.stdout.count("Y_LABEL") == 2  # Decoded names survive translation-dataset termination.


@pytest.mark.skipif(sys.platform == "win32", reason="HTML/LaTeX translation is unavailable on Windows")
@pytest.mark.parametrize("mode", ["latexFormat", "htmlFormat"])
@pytest.mark.parametrize("field", ["OldName", "NewName"])
@pytest.mark.parametrize("value_type", ["long", "double", "character"])
def test_printout_translation_rejects_non_categorical_names(tmp_path, mode, field, value_type):
  primary, names = tmp_path / "source.sdds", tmp_path / "names.sdds"
  enum_file(primary, "ascii", valid_pages=1)
  translation_file(names, "ascii", value_type if field == "OldName" else "string",
                   value_type if field == "NewName" else "string")
  result = run("sddsprintout", primary, "-columns=y", f"-{mode}=translate={names}", check=False)
  assert result.returncode > 0
  assert field in result.stderr and "string or enum" in result.stderr


@pytest.mark.skipif(sys.platform == "win32", reason="HTML/LaTeX translation is unavailable on Windows")
@pytest.mark.parametrize("mode", ["latexFormat", "htmlFormat"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_printout_translation_reports_enum_page_errors(tmp_path, mode, encoding, kind):
  primary, names = tmp_path / "source.sdds", input_path(tmp_path, encoding)
  enum_file(primary, "ascii", valid_pages=1)
  translation_file(names, encoding, invalid=kind)
  assert_enum_error(run("sddsprintout", primary, "-columns=y", f"-{mode}=translate={names}", check=False))


def contour_file(path, encoding, axis, value_type, fixed, label="m", metadata=False):
  order = ">" if encoding.startswith("big") else "<"
  declarations = ''.join(f'&parameter name={name}, type=string, fixed_value={value} &end\n'
                         for name, value in (("Variable1Name", "x"), ("Variable2Name", "y")))
  declarations += ''.join(f'&parameter name={name}{field}, type={kind}, fixed_value={value}' +
                          (', units="s"' if metadata and name == axis and field == "Minimum" else '') + ' &end\n'
                          for name in ("x", "y") for field, kind, value in
                          (("Minimum", "double", 0), ("Interval", "double", 1), ("Dimension", "long", 2)))
  mapping = f', enum_values="-2147483648:{label}"' if value_type == "enum" else ''
  value, raw = scalar(value_type, -2147483648, label, order)
  declarations += f'&parameter name={axis}Units, type={value_type}{mapping}' + (
      f', fixed_value={value}' if fixed else '') + ' &end\n&column name=z, type=double &end\n'
  write_wire(path, encoding, declarations, ('' if fixed else value + '\n') + '4\n0\n1\n2\n3\n',
             struct.pack(order + "i", 4) + (b'' if fixed else raw) + struct.pack(order + "4d", 0, 1, 2, 3))


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("axis", ["x", "y"])
@pytest.mark.parametrize("fixed", [False, True])
def test_contour_enum_units_render_as_dictionary_labels(tmp_path, encoding, axis, fixed):
  source, output = input_path(tmp_path, encoding), tmp_path / "contour.png"
  images = []
  for value_type in ("string", "enum"):
    contour_file(source, encoding, axis, value_type, fixed)
    result = run("sddscontour", source, "-quantity=z", "-device=lpng", f"-output={output}")
    assert not result.stderr
    images.append(output.read_bytes())
  assert images[0].startswith(b"\x89PNG\r\n\x1a\n")
  assert images[1] == images[0]


@pytest.mark.parametrize("axis", ["x", "y"])
@pytest.mark.parametrize("value_type", ["long", "double", "character"])
@pytest.mark.parametrize("fixed", [False, True])
def test_contour_rejects_non_categorical_units(tmp_path, axis, value_type, fixed):
  source, output = tmp_path / "source.sdds", tmp_path / "contour.png"
  contour_file(source, "ascii", axis, value_type, fixed)
  result = run("sddscontour", source, "-quantity=z", "-device=lpng", f"-output={output}", check=False)
  assert result.returncode > 0
  assert f"{axis}Units" in result.stderr and "string or enum" in result.stderr


@pytest.mark.parametrize("metadata", [False, True])
def test_contour_handles_long_enum_units_without_overflow(tmp_path, metadata):
  source, output = tmp_path / "source.sdds", tmp_path / "contour.png"
  contour_file(source, "ascii", "x", "enum", True, label="m" * 9000, metadata=metadata)
  result = run("sddscontour", source, "-quantity=z", "-device=lpng", f"-output={output}", check=False)
  if metadata:
    assert result.returncode == 0, result.stderr  # Existing units metadata takes precedence.
  else:
    assert result.returncode > 0
    assert "units label is too long" in result.stderr
