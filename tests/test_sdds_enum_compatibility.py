"""ENUM regressions for reshaping, label consumers, and failed page reads."""
import gzip
import json
import lzma
import struct
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


MAPPING = "0:OFF,10:ON,-1:FAULT"
ENCODINGS = ("ascii", "row", "column", "big", "gzip", "xz")
PAGE_TOOLS = {
  "sddssortcolumn": (),
  "sddsexpand": (),
  "sddsduplicate": ("-factor=2",),
  "sddsnormalize": ("-columns=y",),
  "sddssmooth": ("-columns=y", "-points=3"),
  "sddsinteg": ("-integrate=y", "-versus=x"),
  "sddsderiv": ("-differentiate=y", "-versus=x"),
  "sddspeakfind": ("-column=y",),
  "sddsendian": (),
  "sddsarray2column": ("-convert=states,converted",),
}


def run(tool, *args, check=True):
  if not (BIN_DIR / tool).exists():
    pytest.skip(f"{tool} not built")
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)],
                          capture_output=True, text=True)
  if check:
    assert result.returncode == 0, result.stderr
  return result


def stream(path, kind, names, codes=False):
  options = [f"-{kind}={names}", "-noquotes"]
  if codes:
    options.append("-enumCodes")
  return run("sdds2stream", path, *options).stdout.splitlines()


def column_names(path):
  return run("sddsquery", path, "-columnlist").stdout.splitlines()


def dataset(path, encoding="ascii", invalid=None, prefix=False):
  """Write real wire data, including codes that normal SDDS writers reject."""
  major = encoding == "column"
  binary = encoding != "ascii"
  order = ">" if encoding == "big" else "<"
  header = ("SDDS6\n"
            f'&parameter name=mode, type=enum, enum_values="{MAPPING}" &end\n'
            f'&array name=states, type=enum, enum_values="{MAPPING}" &end\n'
            f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
            '&column name=x, type=long &end\n'
            '&column name=y, type=double &end\n'
            f'&data mode={"binary" if binary else "ascii"}, '
            f'endian={"big" if order == ">" else "little"}, '
            f'column_major_order={int(major)} &end\n')

  def page(bad):
    mode = 42 if bad == "parameter" else 10
    states = [0, 10, -1, 10, 0]
    rows = [(0, 0, 0.), (10, 1, 2.), (10, 2, 0.), (-1, 3, 3.), (0, 4, 0.)]
    if bad == "array":
      states[0] = 42
    if bad == "column":
      rows[0] = (42, 0, 0.)
    if not binary:
      return (f'{mode}\n5\n' + ' '.join(map(str, states)) + '\n5\n' +
              ''.join(f'{a} {x} {y}\n' for a, x, y in rows)).encode()
    title = struct.pack(order + "8i", 5, mode, 5, *states)
    if major:
      body = (struct.pack(order + "5i", *(r[0] for r in rows)) +
              struct.pack(order + "5i", *(r[1] for r in rows)) +
              struct.pack(order + "5d", *(r[2] for r in rows)))
    else:
      body = b''.join(struct.pack(order + "iid", *row) for row in rows)
    return title + body

  wire = header.encode() + (page(None) if prefix and invalid else b'') + page(invalid)
  if encoding == "gzip":
    wire = gzip.compress(wire)
  elif encoding == "xz":
    wire = lzma.compress(wire)
  path.write_bytes(wire)


@pytest.mark.parametrize("tool", PAGE_TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_failed_enum_reads_are_errors(tmp_path, tool, encoding, kind, prefix):
  suffix = ".sdds" + ({"gzip": ".gz", "xz": ".xz"}.get(encoding, ""))
  source, output = tmp_path / ("invalid" + suffix), tmp_path / "out.sdds"
  dataset(source, encoding, kind, prefix)
  result = run(tool, source, output, *PAGE_TOOLS[tool], check=False)
  assert result.returncode > 0, (result.returncode, result.stderr)
  assert "enum" in result.stderr.lower()


@pytest.mark.parametrize("tool", PAGE_TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_valid_enum_files_reach_normal_eof(tmp_path, tool, encoding):
  suffix = ".sdds" + ({"gzip": ".gz", "xz": ".xz"}.get(encoding, ""))
  source, output = tmp_path / ("valid" + suffix), tmp_path / "out.sdds"
  dataset(source, encoding)
  result = run(tool, source, output, *PAGE_TOOLS[tool])
  assert not result.stderr
  assert run("sdds2stream", output, "-npages=bare").stdout.strip() == (
      "5" if tool == "sddsexpand" else "1")


@pytest.mark.parametrize("tool", ["sddssortcolumn", "sddsendian"])
@pytest.mark.parametrize("encoding", ["ascii", "row"])
def test_failed_in_place_read_preserves_input(tmp_path, tool, encoding):
  source = tmp_path / "invalid.sdds"
  dataset(source, encoding, "column", True)
  original = source.read_bytes()
  result = run(tool, source, source, check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
  assert source.read_bytes() == original


def dereference_input(path, zero):
  mapping = MAPPING if zero else "7:OFF,10:ON,-1:FAULT"
  off = 0 if zero else 7
  path.write_text('SDDS6\n&parameter name=index, type=long &end\n'
                  f'&array name=states, type=enum, enum_values="{mapping}" &end\n'
                  f'&column name=state, type=enum, enum_values="{mapping}" &end\n'
                  '&column name=index, type=long &end\n&data mode=ascii &end\n'
                  f'99\n3\n{off} 10 -1\n4\n{off} 1\n10 99\n-1 -1\n{off} 0\n')


@pytest.mark.parametrize("source_kind", ["columnSource=state", "arraySource=states"])
@pytest.mark.parametrize("zero", [False, True])
@pytest.mark.parametrize("target_kind", ["constant", "parameter"])
def test_enum_dereference_fallback_is_mapped_and_deterministic(
    tmp_path, source_kind, zero, target_kind):
  source, output = tmp_path / "source.sdds", tmp_path / "out.sdds"
  dereference_input(source, zero)
  bad_index = "99" if target_kind == "constant" else "index"
  result = run("sddsderef", source, output,
               f"-constant=good,{source_kind},1",
               f"-{target_kind}=bad,{source_kind},{bad_index}",
               "-outOfBounds=delete", check=False)
  if zero:
    assert result.returncode == 0, result.stderr
    assert stream(output, "parameters", "good,bad", codes=True) == ["10", "0"]
    assert stream(output, "parameters", "good,bad") == ["ON", "OFF"]
  else:
    assert result.returncode > 0
    assert "fallback code 0" in result.stderr


@pytest.mark.parametrize("source_kind", ["columnSource=state", "arraySource=states"])
def test_enum_dereference_with_index_column_deletes_only_invalid_rows(tmp_path, source_kind):
  source, output = tmp_path / "source.sdds", tmp_path / "out.sdds"
  dereference_input(source, False)
  run("sddsderef", source, output, f"-column=selected,{source_kind},index",
      "-outOfBounds=delete")
  assert stream(output, "columns", "selected", codes=True) == ["10", "7"]
  assert stream(output, "columns", "selected") == ["ON", "OFF"]


@pytest.mark.parametrize("fixed", [False, True])
@pytest.mark.parametrize("rows", [0, 1, 5])
def test_peakfind_rejects_enum_threshold_even_without_peaks(tmp_path, fixed, rows):
  source, output = tmp_path / "source.sdds", tmp_path / "out.sdds"
  source.write_text('SDDS6\n&parameter name=limit, type=enum, '
                    'enum_values="10:ON"' + (', fixed_value=10' if fixed else '') +
                    ' &end\n&column name=y, type=double &end\n&data mode=ascii &end\n' +
                    ('' if fixed else '10\n') + str(rows) + '\n' +
                    '\n'.join(map(str, [0, 2, 0, 3, 0][:rows])) + '\n')
  result = run("sddspeakfind", source, output, "-column=y", "-threshold=@limit", check=False)
  assert result.returncode > 0
  assert "threshold parameter must have numeric type" in result.stderr
  assert not output.exists()


@pytest.mark.parametrize("value_type", ["short", "long", "float", "double", "longdouble"])
def test_peakfind_converts_numeric_threshold_parameters(tmp_path, value_type):
  source, output = tmp_path / "source.sdds", tmp_path / "out.sdds"
  source.write_text(f'SDDS1\n&parameter name=limit, type={value_type} &end\n'
                    '&column name=y, type=double &end\n&data mode=ascii &end\n'
                    '10\n5\n0\n2\n0\n3\n0\n')
  run("sddspeakfind", source, output, "-column=y", "-threshold=@limit")
  assert stream(output, "columns", "y") == []


@pytest.mark.parametrize("slice_option,expected", [
  ("", [0, 10, -1, 10, -1, 0, 10, -1]),
  (",d0=1", [-1, 0, 10, -1]),
  (",d1=1", [-1, 10, 10, -1]),
  (",d2=1", [10, 10, 0, -1]),
  (",d0=1,d1=0,d2=1", [0]),
])
def test_array2column_preserves_enum_mapping_and_slices(tmp_path, slice_option, expected):
  source, output = tmp_path / "source.sdds", tmp_path / "out.sdds"
  source.write_text('SDDS6\n&array name=states, type=enum, dimensions=3, '
                    f'enum_values="{MAPPING}" &end\n&data mode=ascii &end\n'
                    '2 2 2\n0 10 -1 10 -1 0 10 -1\n')
  run("sddsarray2column", source, output, "-convert=states,selected" + slice_option)
  assert list(map(int, stream(output, "columns", "selected", codes=True))) == expected
  labels = {0: "OFF", 10: "ON", -1: "FAULT"}
  assert stream(output, "columns", "selected") == [labels[code] for code in expected]
  assert output.read_bytes().startswith(b'SDDS6\n')


def test_break_enum_change_compares_categories_and_preserves_pages(tmp_path):
  source, output = tmp_path / "source.sdds", tmp_path / "out.sdds"
  dataset(source)
  run("sddsbreak", source, output, "-changeOf=state")
  assert run("sdds2stream", output, "-rows=bare").stdout.split() == ["1", "2", "1", "1"]
  assert stream(output, "columns", "state") == ["OFF", "ON", "ON", "FAULT", "OFF"]
  assert stream(output, "parameters", "mode") == ["ON"] * 4
  assert [line.split() for line in stream(output, "arrays", "states")] == [
      ["OFF", "ON", "FAULT", "ON", "OFF"]] * 4


@pytest.mark.parametrize("option", ["-changeOf=state,amount=1", "-changeOf=state,base=0",
                                   "-increaseOf=state", "-decreaseOf=state", "-gapIn=state"])
def test_break_enum_numeric_modes_require_explicit_conversion(tmp_path, option):
  source = tmp_path / "source.sdds"
  dataset(source)
  result = run("sddsbreak", source, tmp_path / "out.sdds", option, check=False)
  assert result.returncode > 0
  assert "numeric data" in result.stderr


def names_input(path, value_type, name="Name", labels=("rowA", "rowB")):
  mapping = ', enum_values="7:' + labels[0] + ',10:' + labels[1] + '"' if value_type == "enum" else ""
  path.write_text(('SDDS6' if value_type == 'enum' else 'SDDS1') +
                  f'\n&column name={name}, type={value_type}{mapping} &end\n'
                  '&data mode=ascii &end\n2\n' +
                  ('\n'.join(labels) if value_type == "string" else '7\n10') + '\n')


@pytest.mark.parametrize("value_type", ["string", "enum", "long"])
def test_sortcolumn_uses_labels_for_sortwith(tmp_path, value_type):
  source, names, output = (tmp_path / name for name in ("source.sdds", "names.sdds", "out.sdds"))
  dataset(source)
  names_input(names, value_type, labels=("y", "state"))
  result = run("sddssortcolumn", source, output, f"-sortWith={names},column=Name", check=False)
  if value_type == "long":
    assert result.returncode > 0
    assert "string or enum" in result.stderr
  else:
    assert result.returncode == 0, result.stderr
    assert column_names(output) == ["y", "state", "x"]
    assert stream(output, "columns", "state") == ["OFF", "ON", "ON", "FAULT", "OFF"]


@pytest.mark.parametrize("value_type", ["string", "enum", "long"])
def test_matrixop_uses_labels_for_column_names(tmp_path, value_type):
  source, names, output = (tmp_path / name for name in ("source.sdds", "names.sdds", "out.sdds"))
  source.write_text('SDDS1\n&column name=A, type=double &end\n'
                    '&column name=B, type=double &end\n&data mode=ascii &end\n2\n1 2\n3 4\n')
  names_input(names, value_type)
  result = run("sddsmatrixop", source, output, "-transpose",
               f"-columnNames=filename={names},column=Name", check=False)
  if value_type == "long":
    assert result.returncode > 0
    assert "string or enum" in result.stderr
  else:
    assert result.returncode == 0, result.stderr
    assert "rowA" in column_names(output) and "rowB" in column_names(output)
    assert [list(map(float, line.split())) for line in stream(output, "columns", "rowA,rowB")] == [[1, 3], [2, 4]]


@pytest.mark.parametrize("mode", ["cor", "bpm"])
@pytest.mark.parametrize("value_type", ["string", "enum", "long"])
def test_response_derivative_uses_enum_bpm_names(tmp_path, mode, value_type):
  source, output = tmp_path / "source.sdds", tmp_path / "out.sdds"
  names_input(source, value_type, name="BPMName", labels=("BPM1", "BPM2"))
  text = source.read_text().replace('&data mode=ascii', '&column name=H1, type=double &end\n&data mode=ascii')
  before, body = text.split('&data mode=ascii &end\n', 1)
  values = body.splitlines()
  source.write_text(before + '&data mode=ascii &end\n2\n' + values[1] + ' 1\n' + values[2] + ' 2\n')
  result = run("sddsrespmatrixderivative", source, output, f"-mode={mode}", check=False)
  if value_type == "long":
    assert result.returncode > 0
    assert "string or enum" in result.stderr
  else:
    assert result.returncode == 0, result.stderr
    assert stream(output, "columns", "Rootname") == ["H1BPM1", "H1BPM2"]
    columns = "H1" if mode == "cor" else "BPM1,BPM2"
    expected = [[1], [2]] if mode == "cor" else [[1, 0], [0, 2]]
    assert [list(map(float, line.split())) for line in stream(output, "columns", columns)] == expected


LABEL_FIELDS = [("parameter", "Description", "Tree"), ("parameter", "LogicalTypeDesc", "AND"),
                ("parameter", "TreeName", "Tree1"), ("column", "Description", "element"),
                ("column", "Guidance", "advice"), ("column", "Label", "E1")]


def fault_tree(path, changed=None, value_type="enum"):
  fields = [("parameter", "Description", "string", "Tree"),
            ("parameter", "ID", "long", 1), ("parameter", "LogicalType", "short", 0),
            ("parameter", "LogicalTypeDesc", "string", "AND"),
            ("parameter", "TreeName", "string", "Tree1"),
            ("column", "ID", "long", 1001), ("column", "Probability", "double", 0.1),
            ("column", "Description", "string", "element"),
            ("column", "Guidance", "string", "advice"), ("column", "Label", "string", "E1")]
  header, parameters, columns = ['SDDS6\n'], [], []
  for kind, name, primitive, value in fields:
    change = changed == (kind, name)
    field_type = value_type if change else primitive
    mapping = ', enum_values="7:' + str(value) + '"' if field_type == "enum" else ''
    header.append(f'&{kind} name={name}, type={field_type}{mapping} &end\n')
    wire = '7' if change else json.dumps(value)
    (parameters if kind == "parameter" else columns).append(wire)
  path.write_text(''.join(header) + '&data mode=ascii &end\n' + '\n'.join(parameters) +
                  '\n1\n' + ' '.join(columns) + '\n')


@pytest.mark.parametrize("kind,name,label", LABEL_FIELDS)
def test_fault_tree_uses_enum_labels(tmp_path, kind, name, label):
  source, output = tmp_path / "tree.sdds", tmp_path / "out.sdds"
  fault_tree(source, (kind, name))
  run("convert_to_bdd", source, output)
  assert stream(output, kind + "s", name) == [label]
  assert stream(output, "columns", "Label,Description,Guidance") == ["E1 element advice"]


@pytest.mark.parametrize("kind,name", [("parameter", "ID"), ("parameter", "LogicalType"),
                                       ("column", "ID"), ("column", "Probability")])
def test_fault_tree_rejects_implicit_enum_arithmetic(tmp_path, kind, name):
  source = tmp_path / "tree.sdds"
  fault_tree(source, (kind, name))
  result = run("convert_to_bdd", source, tmp_path / "out.sdds", check=False)
  assert result.returncode > 0
  assert result.stderr
