"""Regressions for categorical interpolation, failed page reads, and contour labels."""

import gzip
import json
import lzma
import struct

import pytest

from sdds_test_utils import BIN_DIR
from test_sdds_enum_read_failures import ENCODINGS, input_path, page_count, run


MAPPING = "0:OFF,10:ON,-1:FAULT"
CODES = (10, 0, -1, 10, 0)
TOOLS = ("sddsminterp", "sddsinterpset", "sdds2dinterpolate", "sddsslopes",
         "sddsvslopes", "sddsinsideboundaries", "sddslocaldensity")


def write_input(path, encoding, data_file, pages=2, invalid=None, mapping=MAPPING,
                codes=CODES):
  """Encode SDDS wire data independently so unmapped values reach the readers."""
  binary = encoding != "ascii"
  order = ">" if encoding.startswith("big") else "<"
  major = encoding.endswith("column")
  columns = (("state", "enum"), ("x", "double"), ("y", "double"),
             ("z", "double"), ("Rootname", "string"), ("dataFile", "string"))
  header = ('SDDS6\n&parameter name=Setpoint, type=double &end\n'
            f'&parameter name=mode, type=enum, enum_values="{mapping}" &end\n'
            f'&parameter name=fixed, type=enum, enum_values="{mapping}", fixed_value=10 &end\n'
            f'&array name=states, type=enum, enum_values="{mapping}" &end\n')
  for name, value_type in columns:
    extra = f', enum_values="{mapping}"' if value_type == "enum" else ""
    header += f'&column name={name}, type={value_type}{extra} &end\n'
  header += (f'&data mode={"binary" if binary else "ascii"}, '
             f'endian={"big" if order == ">" else "little"}, '
             f'column_major_order={int(major)} &end\n')

  def pack(value_type, value):
    if value_type == "string":
      encoded = value.encode("utf8")
      return struct.pack(order + "i", len(encoded)) + encoded
    return struct.pack(order + ("d" if value_type == "double" else "i"), value)

  def page(index, bad=None):
    mode = 42 if bad == "parameter" else 10
    states = list(codes)
    if bad == "array":
      states[-1] = 42
    rows = [(42 if bad == "column" and row == 4 else codes[row], float(row),
             y + index, row + y, f"row{row}", data_file.as_posix())
            for row, y in enumerate((0., 2., 0., 3., 0.))]
    if not binary:
      return (f'{index}\n{mode}\n5\n' + ' '.join(map(str, states)) + '\n5\n' +
              ''.join(' '.join(json.dumps(value, ensure_ascii=False) if isinstance(value, str)
                               else str(value) for value in row) + '\n' for row in rows)).encode("utf8")
    wire = struct.pack(order + "idi6i", 5, float(index), mode, 5, *states)
    values = ((value_type, row[column]) for column, (_, value_type) in enumerate(columns)
              for row in rows) if major else (
                  (value_type, value) for row in rows for (_, value_type), value in zip(columns, row))
    return wire + b''.join(pack(value_type, value) for value_type, value in values)

  wire = header.encode("utf8") + b''.join(page(index) for index in range(pages))
  if invalid:
    wire += page(pages, invalid)
  if encoding == "gzip":
    wire = gzip.compress(wire)
  elif encoding in ("xz", "lzma"):
    wire = lzma.compress(wire, format=lzma.FORMAT_ALONE if encoding == "lzma" else lzma.FORMAT_XZ)
  path.write_bytes(wire)


@pytest.fixture
def numeric_files(tmp_path):
  model, boundary = tmp_path / "model.sdds", tmp_path / "boundary.sdds"
  header = ('SDDS1\n&column name=x, type=double &end\n'
            '&column name=y, type=double &end\n&data mode=ascii &end\n')
  model.write_text(header.replace('&data mode=ascii &end\n',
                                 '&column name=u, type=double &end\n'
                                 '&column name=v, type=double &end\n&data mode=ascii &end\n') +
                   '5\n0 1 0 1\n1 2 1 2\n2 3 2 3\n3 4 3 4\n4 5 4 5\n')
  boundary.write_text(header + '5\n-1 -1\n5 -1\n5 5\n-1 5\n-1 -1\n')
  return model, boundary


def options(tool, numeric_files):
  model, boundary = numeric_files
  return {
      "sddsminterp": ("-columns=x,y", f"-model={model.as_posix()},abscissa=x,ordinate=y,interp=1"),
      "sddsinterpset": ("-data=fileColumn=dataFile,interpolate=v,functionOf=u,atValue=1.5",),
      "sdds2dinterpolate": ("-independentColumn=xcolumn=x,ycolumn=y", "-dependentColumn=z",
                            "-outDimension=xdimension=3,ydimension=3"),
      "sddsslopes": ("-independentVariable=x", "-columns=y"),
      "sddsvslopes": ("-independentVariable=Setpoint", "-columns=y"),
      "sddsinsideboundaries": ("-columns=x,y", f"-boundary={boundary.as_posix()},x,y"),
      "sddslocaldensity": ("-columns=none,x", "-fraction=0.5"),
  }[tool]


def assert_enum_error(result):
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower()
  assert "42" in result.stderr


@pytest.mark.parametrize("tool", TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 2])
def test_unmapped_enum_page_is_an_error(tmp_path, numeric_files, tool, encoding, kind, prefix):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  write_input(source, encoding, numeric_files[0], pages=prefix, invalid=kind)
  assert_enum_error(run(tool, source, output, *options(tool, numeric_files), check=False))


@pytest.mark.parametrize("tool", TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_valid_enum_input_reaches_normal_eof(tmp_path, numeric_files, tool, encoding):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  write_input(source, encoding, numeric_files[0])
  result = run(tool, source, output, *options(tool, numeric_files))
  assert not result.stderr
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert page_count(output) == (1 if tool == "sddsvslopes" else 2)
  if tool in ("sddsminterp", "sddsinterpset", "sddsinsideboundaries", "sddslocaldensity"):
    assert run("sdds2stream", output, "-column=state", "-enumCodes", "-noquotes").stdout.split() == list(map(str, CODES)) * 2
    assert run("sdds2stream", output, "-parameter=mode", "-noquotes").stdout.split() == ["ON", "ON"]
    assert run("sdds2stream", output, "-array=states", "-enumCodes", "-noquotes").stdout.split() == list(map(str, CODES)) * 2


@pytest.mark.parametrize("tool", ["sddsminterp", "sddsslopes", "sddsvslopes"])
@pytest.mark.parametrize("encoding", ["ascii", "big-column", "gzip"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 2])
def test_failed_enum_read_preserves_in_place_input(tmp_path, numeric_files, tool, encoding, kind, prefix):
  source = input_path(tmp_path, encoding)
  write_input(source, encoding, numeric_files[0], pages=prefix, invalid=kind)
  original = source.read_bytes()
  assert_enum_error(run(tool, source, *options(tool, numeric_files), check=False))
  assert source.read_bytes() == original


@pytest.mark.parametrize("tool", ["sdds2dinterpolate", "sddsinsideboundaries"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 2])
def test_auxiliary_enum_page_errors_are_not_eof(tmp_path, numeric_files, tool, encoding, kind, prefix):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  auxiliary = input_path(tmp_path, encoding)
  write_input(source, "ascii", numeric_files[0])
  write_input(auxiliary, encoding, numeric_files[0], pages=prefix, invalid=kind)
  if tool == "sdds2dinterpolate":
    extra = (*options(tool, numeric_files), f"-file={auxiliary.as_posix()}")
  else:
    extra = ("-columns=x,y", f"-boundary={auxiliary.as_posix()},x,y")
  assert_enum_error(run(tool, source, output, *extra, check=False))


@pytest.mark.parametrize("auxiliary", ["model", "values"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_minterp_rejects_failed_auxiliary_first_page(tmp_path, numeric_files, auxiliary, encoding, kind):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  bad = input_path(tmp_path, encoding)
  write_input(source, "ascii", numeric_files[0])
  write_input(bad, encoding, numeric_files[0], pages=0, invalid=kind)
  model = bad if auxiliary == "model" else numeric_files[0]
  extra = ["-columns=x,y", f"-model={model.as_posix()},abscissa=x,ordinate=y,interp=1"]
  if auxiliary == "values":
    extra.append(f"-fileValues={bad.as_posix()},abscissa=x")
  assert_enum_error(run("sddsminterp", source, output, *extra, check=False))


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("mapping,codes", [
    (MAPPING, CODES),
    ("7:OFF,10:ON,-2147483648:FAULT", (10, 7, -2147483648, 10, 7)),
])
@pytest.mark.parametrize("mode", ["ascii", "binary"])
def test_minterp_preserves_categories_on_matching_grid(tmp_path, numeric_files, encoding, mapping, codes, mode):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  write_input(source, encoding, numeric_files[0], mapping=mapping, codes=codes)
  run("sddsminterp", source, output, *options("sddsminterp", numeric_files),
      *(["-ascii"] if mode == "ascii" else []))
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert output.read_bytes().startswith(b"SDDS6\n")
  assert output.read_bytes().count(f'enum_values="{mapping}"'.encode()) == 4
  assert run("sdds2stream", output, "-column=state", "-enumCodes", "-noquotes").stdout.split() == list(map(str, codes)) * 2
  assert run("sdds2stream", output, "-column=state", "-noquotes").stdout.split() == [
      "ON", "OFF", "FAULT", "ON", "OFF"] * 2
  assert run("sdds2stream", output, "-array=states", "-enumCodes", "-noquotes").stdout.split() == list(map(str, codes)) * 2
  for name in ("mode", "fixed"):
    assert run("sdds2stream", output, f"-parameter={name}", "-noquotes").stdout.split() == ["ON", "ON"]
  assert list(map(float, run("sdds2stream", output, "-column=y").stdout.split())) == pytest.approx(
      [0, 2, 0, 3, 0, 1, 3, 1, 4, 1])


@pytest.mark.parametrize("grid", [(0, 2, 4), (0, 1, 2, 3, 5), (4, 3, 2, 1, 0)])
@pytest.mark.parametrize("in_place", [False, True])
def test_minterp_rejects_changed_categorical_grid(tmp_path, numeric_files, grid, in_place):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  write_input(source, "ascii", numeric_files[0])
  original = source.read_bytes()
  model = numeric_files[0]
  model.write_text('SDDS1\n&column name=x, type=double &end\n'
                   '&column name=y, type=double &end\n&data mode=ascii &end\n' +
                   f'{len(grid)}\n' + ''.join(f'{x} {x + 1}\n' for x in grid))
  result = run("sddsminterp", source, *([] if in_place else [output]),
               *options("sddsminterp", numeric_files), check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum column state" in result.stderr
  assert "changed interpolation grid" in result.stderr
  assert source.read_bytes() == original


def test_minterp_keeps_dictionaries_local_to_each_column(tmp_path, numeric_files):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  source.write_text('SDDS6\n'
                    '&column name=state, type=enum, enum_values="7:OFF,10:ON" &end\n'
                    '&column name=other, type=enum, enum_values="7:IDLE,10:READY" &end\n'
                    '&column name=x, type=double &end\n'
                    '&column name=y, type=double &end\n&data mode=ascii &end\n5\n' +
                    ''.join(f'{code} {code} {row} {row + 1}\n'
                            for row, code in enumerate((10, 7, 10, 7, 10))))
  run("sddsminterp", source, output, *options("sddsminterp", numeric_files))
  assert run("sdds2stream", output, "-column=state", "-noquotes").stdout.split() == [
      "ON", "OFF", "ON", "OFF", "ON"]
  assert run("sdds2stream", output, "-column=other", "-noquotes").stdout.split() == [
      "READY", "IDLE", "READY", "IDLE", "READY"]


@pytest.mark.parametrize("encoding", ["ascii", "big-column", "gzip"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 2])
def test_density_checks_errors_before_reopening_for_next_pass(tmp_path, numeric_files, encoding, kind, prefix):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  write_input(source, encoding, numeric_files[0], pages=prefix, invalid=kind)
  assert_enum_error(run("sddslocaldensity", source, output, "-columns=none,x",
                        "-kde=bins=20,spanPages", check=False))


@pytest.mark.parametrize("encoding", ["ascii", "big-column", "gzip"])
def test_density_span_pages_accepts_normal_enum_eof(tmp_path, numeric_files, encoding):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  write_input(source, encoding, numeric_files[0])
  run("sddslocaldensity", source, output, "-columns=none,x", "-kde=bins=20,spanPages")
  assert page_count(output) == 2
  assert run("sdds2stream", output, "-column=state", "-enumCodes", "-noquotes").stdout.split() == list(map(str, CODES)) * 2


def write_contour_input(path, fixed=False, value_type="enum"):
  mapping = f', enum_values="{MAPPING}"' if value_type == "enum" else ""
  value = "10" if value_type == "enum" else "ON"
  path.write_text('SDDS6\n' + f'&parameter name=mode, type={value_type}{mapping}' +
                  (f', fixed_value={value}' if fixed else '') + ' &end\n'
                  '&array name=z, type=double, dimensions=2 &end\n'
                  '&array name=x, type=double &end\n'
                  '&array name=y, type=double &end\n&data mode=ascii &end\n' +
                  ('' if fixed else value + '\n') + '2 2\n0 1 1 0\n2\n0 1\n2\n0 1\n')


@pytest.mark.skipif(not (BIN_DIR / "sddscontour").exists(), reason="sddscontour not built")
@pytest.mark.parametrize("fixed", [False, True])
@pytest.mark.parametrize("value_type", ["enum", "string"])
@pytest.mark.parametrize("format", [None, "%s", "[%8s]", "%.1s"])
@pytest.mark.parametrize("edit", [False, True])
def test_contour_formats_categorical_parameter_labels(tmp_path, fixed, value_type, format, edit):
  source, output = tmp_path / "source.sdds", tmp_path / "contour.ps"
  write_contour_input(source, fixed, value_type)
  label = '-topline=@mode' + (f',format={format}' if format is not None else '')
  if edit:
    label += ',edit=ei/_suffix/'
  run("sddscontour", source, '-array=z,x,y', label, '-device=postscript', f'-output={output}')
  expected = 'O' if format == '%.1s' else '[      ON]' if format == '[%8s]' else 'ON'
  if edit:
    expected += '_suffix'
  reference = tmp_path / "reference.ps"
  run("sddscontour", source, '-array=z,x,y', f'-topline={expected}',
      '-device=postscript', f'-output={reference}')
  # Text is drawn with vector strokes. Compare the drawing to a literal label.
  def drawing(path):
    return [line for line in path.read_text().splitlines() if not line.startswith('%%Creator:')]
  assert drawing(output) == drawing(reference)


@pytest.mark.skipif(not (BIN_DIR / "sddscontour").exists(), reason="sddscontour not built")
@pytest.mark.parametrize("format", ["%d", "%n", "%s%s"])
def test_contour_rejects_invalid_enum_label_formats(tmp_path, format):
  source = tmp_path / "source.sdds"
  write_contour_input(source, fixed=True)
  result = run("sddscontour", source, '-array=z,x,y', f'-topline=@mode,format={format}',
               '-device=postscript', f'-output={tmp_path / "plot.ps"}', check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "format" in result.stderr.lower()


@pytest.mark.skipif(not (BIN_DIR / "sddscontour").exists(), reason="sddscontour not built")
@pytest.mark.parametrize("width", [1023, 2048])
def test_contour_rejects_oversized_edited_enum_labels(tmp_path, width):
  source = tmp_path / "source.sdds"
  write_contour_input(source)
  result = run("sddscontour", source, '-array=z,x,y',
               f'-topline=@mode,format=%{width}s,edit=ei/_suffix/', '-device=postscript',
               f'-output={tmp_path / "plot.ps"}', check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "label" in result.stderr.lower() and "too long" in result.stderr.lower()
