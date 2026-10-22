"""SDDS6 enum plotting, categorical labels, and invalid-input regressions."""

import gzip
import json
import lzma
import struct
import subprocess
import zlib

import pytest

from sdds_test_utils import BIN_DIR


SDDSPLOT = BIN_DIR / "sddsplot"
MAPPING = "-17:FAULT,6:RUN,42:IDLE"
ENCODINGS = ("ascii", "little-row", "little-column", "big-row", "big-column")
pytestmark = pytest.mark.skipif(not SDDSPLOT.exists(), reason="sddsplot not built")


def write_enum_dataset(path, encoding="ascii", pages=(6,), invalid_kind=None,
                       fixed=False, parameter_mapping=MAPPING, column_mapping=MAPPING,
                       column_codes=(42, -17, 42)):
  """Write independently encoded data so a tool's writer cannot hide read bugs."""
  binary = encoding != "ascii"
  endian = ">" if encoding.startswith("big") else "<"
  column_major = encoding.endswith("column")
  header = ("SDDS6\n"
            f'&parameter name=stateParameter, type=enum, units=state, enum_values="{parameter_mapping}"' +
            (", fixed_value=6" if fixed else "") + " &end\n"
            "&parameter name=px, type=double &end\n"
            "&parameter name=py, type=double &end\n"
            f'&array name=stateArray, type=enum, units=state, enum_values="{MAPPING}" &end\n'
            "&array name=ax, type=double &end\n"
            "&array name=ay, type=double &end\n"
            "&column name=x, type=double &end\n"
            "&column name=y, type=double &end\n"
            f'&column name=state, type=enum, units=state, enum_values="{column_mapping}" &end\n'
            f'&column name=otherState, type=enum, units=state, enum_values="{MAPPING}" &end\n'
            "&column name=textState, type=string &end\n" +
            (f'&data mode=binary, endian={"big" if endian == ">" else "little"}, '
             f'column_major_order={int(column_major)} &end\n' if binary else
             "&data mode=ascii &end\n"))
  body = []
  for page, code in enumerate(pages):
    bad_page = invalid_kind is not None and page == len(pages) - 1
    parameter = 999 if bad_page and invalid_kind == "parameter" else code
    states = list(column_codes)
    if bad_page and invalid_kind == "column":
      states[1] = 999
    arrays = [42, 999 if bad_page and invalid_kind == "array" else -17, 42]
    x = [1 + 3 * page, 2 + 3 * page, 3 + 3 * page]
    y = [4 + 3 * page, 8 + 3 * page, 16 + 3 * page]
    columns = [x, y, states, [6, 42, 6], ["IDLE", "FAULT", "IDLE"]]
    if binary:
      pack = lambda fmt, *values: struct.pack(endian + fmt, *values)
      data = pack("i", 3)
      if not fixed:
        data += pack("i", parameter)
      data += pack("dd", page + 1, (page + 1) * 10)
      for values, value_type in ((arrays, "i"), (x, "d"), (y, "d")):
        data += pack("i", 3) + pack(value_type * 3, *values)
      if column_major:
        values = ((value, i) for i, column in enumerate(columns) for value in column)
      else:
        values = ((value, i) for row in zip(*columns) for i, value in enumerate(row))
      for value, i in values:
        if i == 4:
          text = value.encode("utf8")
          data += pack("i", len(text)) + text
        else:
          data += pack("d" if i < 2 else "i", value)
      body.append(data)
    else:
      data = ([] if fixed else [str(parameter)]) + [str(page + 1), str((page + 1) * 10)]
      for values in (arrays, x, y):
        data.extend(("3", " ".join(map(str, values))))
      data.append("3")
      data.extend(" ".join(map(str, row)) for row in zip(*columns))
      body.append(("\n".join(data) + "\n").encode("utf8"))
  path.write_bytes(header.encode("utf8") + b"".join(body))


def run_plot(source, output, *options, check=True):
  args = [str(SDDSPLOT), str(source), *options, "-device=json", f"-output={output}"]
  result = subprocess.run(args, capture_output=True, text=True, timeout=20)
  if check:
    assert result.returncode == 0, f"{args!r}\n{result.stderr}"
    return json.loads(output.read_text())
  return result


def native_png_data(tmp_path, name, sources, *options):
  """Return PNG pixel data and palette while ignoring command-line metadata."""
  args = [str(SDDSPLOT), *map(str, sources), *options,
          f"-device=png,rootname={name},onwhite"]
  result = subprocess.run(args, cwd=tmp_path, capture_output=True, text=True, timeout=20)
  assert result.returncode == 0, f"{args!r}\n{result.stderr}"
  pngs = [path for path in tmp_path.glob(f"{name}*")
          if path.is_file() and path.read_bytes().startswith(b"\x89PNG\r\n\x1a\n")]
  assert len(pngs) == 1, f"Expected one PNG from {args!r}, found {pngs!r}"
  data = pngs[0].read_bytes()
  offset, image_chunks, compressed = 8, [], []
  while offset < len(data):
    length = struct.unpack_from(">I", data, offset)[0]
    kind = data[offset + 4:offset + 8]
    payload = data[offset + 8:offset + 8 + length]
    if kind in (b"IHDR", b"PLTE", b"tRNS"):
      image_chunks.append((kind, payload))
    elif kind == b"IDAT":
      compressed.append(payload)
    offset += length + 12
  assert compressed
  return image_chunks, zlib.decompress(b"".join(compressed))


def traces(doc):
  return [trace for plot in doc["plots"] for trace in plot["traces"]]


def assert_category_ticks(axis, labels):
  assert axis["tickmode"] == "array"
  assert axis["tickvals"] == list(range(len(labels)))
  assert axis["ticktext"] == labels


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("selection,expected_x,expected_y", [
    ("-column=x,y", [1, 2, 3, 4, 5, 6], [4, 8, 16, 7, 11, 19]),
    ("-parameter=px,py", [1, 2], [10, 20]),
    ("-array=ax,ay", [1, 2, 3, 4, 5, 6], [4, 8, 16, 7, 11, 19]),
])
def test_numeric_plots_accept_unrelated_enum_fields(tmp_path, encoding, selection,
                                                   expected_x, expected_y):
  source = tmp_path / "enums.sdds"
  write_enum_dataset(source, encoding, pages=(6, 42))
  doc = run_plot(source, tmp_path / "numeric.json", selection)
  assert len(traces(doc)) == 1
  assert traces(doc)[0]["x"] == expected_x
  assert traces(doc)[0]["y"] == expected_y
  assert "ticktext" not in doc["plots"][0]["layout"]["xaxis"]
  assert "ticktext" not in doc["plots"][0]["layout"]["yaxis"]


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("axis", ["x", "y"])
def test_enum_column_uses_labels_as_categories(tmp_path, encoding, axis):
  source = tmp_path / "categories.sdds"
  write_enum_dataset(source, encoding)
  selection = "state,y" if axis == "x" else "x,state"
  doc = run_plot(source, tmp_path / "categories.json", f"-column={selection}")
  trace = traces(doc)[0]
  assert trace[axis] == [0, 1, 0]
  assert trace["y" if axis == "x" else "x"] == ([4, 8, 16] if axis == "x" else [1, 2, 3])
  assert_category_ticks(doc["plots"][0]["layout"][f"{axis}axis"], ["IDLE", "FAULT"])
  if axis == "y":
    assert_category_ticks(doc["plots"][0]["layout"]["yaxes"][0], ["IDLE", "FAULT"])

  # A string column with the same labels must produce exactly the same categories.
  selection = "textState,y" if axis == "x" else "x,textState"
  string_doc = run_plot(source, tmp_path / "strings.json", f"-column={selection}")
  assert traces(string_doc)[0]["x"] == trace["x"]
  assert traces(string_doc)[0]["y"] == trace["y"]
  assert_category_ticks(string_doc["plots"][0]["layout"][f"{axis}axis"], ["IDLE", "FAULT"])


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("axis", ["x", "y"])
def test_numeric_sort_and_swap_preserve_enum_categories(tmp_path, encoding, axis):
  source = tmp_path / "categories.sdds"
  write_enum_dataset(source, encoding, column_codes=(-17, 42, 6))
  selection = "state,y" if axis == "x" else "y,state"
  doc = run_plot(source, tmp_path / "sorted.json", f"-column={selection}",
                 "-sort=y,decreasing")
  other_axis = "y" if axis == "x" else "x"
  assert traces(doc)[0][axis] == [0, 1, 2]
  assert traces(doc)[0][other_axis] == [16, 8, 4]
  assert_category_ticks(doc["plots"][0]["layout"][f"{axis}axis"],
                        ["RUN", "IDLE", "FAULT"])

  swapped = run_plot(source, tmp_path / "swapped.json", f"-column={selection}",
                     "-sort=y,decreasing", "-swap")
  assert traces(swapped)[0][axis] == traces(doc)[0][other_axis]
  assert traces(swapped)[0][other_axis] == traces(doc)[0][axis]
  assert_category_ticks(swapped["plots"][0]["layout"][f"{other_axis}axis"],
                        ["RUN", "IDLE", "FAULT"])


@pytest.mark.skipif(not (BIN_DIR / "sddscast").exists(), reason="sddscast not built")
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind,name,selection,expected_x,expected_y", [
    ("column", "state", "-column=state,y", [42, -17, 42] * 2,
     [4, 8, 16, 7, 11, 19]),
    ("parameter", "stateParameter", "-parameter=stateParameter,py", [6, 42], [10, 20]),
    ("array", "stateArray", "-array=stateArray,ay", [42, -17, 42] * 2,
     [4, 8, 16, 7, 11, 19]),
])
def test_explicit_cast_allows_enum_codes_on_numeric_axes(tmp_path, encoding, kind,
                                                        name, selection,
                                                        expected_x, expected_y):
  source, converted = tmp_path / "categories.sdds", tmp_path / "codes.sdds"
  write_enum_dataset(source, encoding, pages=(6, 42))
  result = subprocess.run([str(BIN_DIR / "sddscast"), str(source), str(converted),
                           f"-cast={kind},{name},enum,long"],
                          capture_output=True, text=True, timeout=20)
  assert result.returncode == 0, result.stderr
  doc = run_plot(converted, tmp_path / "codes.json", selection)
  assert traces(doc)[0]["x"] == expected_x
  assert traces(doc)[0]["y"] == expected_y
  assert "ticktext" not in doc["plots"][0]["layout"]["xaxis"]


@pytest.mark.parametrize("second_mapping,second_codes,expected", [
    ("-17:IDLE,6:RUN,42:FAULT", (42, -17, 42), [1, 0, 1]),
    (MAPPING, (6, -17, 6), [2, 1, 2]),
    ("-17:RUN,6:IDLE,42:FAULT", (-17, 42, -17), [2, 1, 2]),
])
def test_enum_categories_merge_by_label_across_files(tmp_path, second_mapping,
                                                    second_codes, expected):
  sources = [tmp_path / "first.sdds", tmp_path / "second.sdds"]
  write_enum_dataset(sources[0])
  write_enum_dataset(sources[1], column_mapping=second_mapping, column_codes=second_codes)
  output = tmp_path / "categories.json"
  args = [str(SDDSPLOT), *map(str, sources), "-column=state,y",
          "-device=json", f"-output={output}"]
  result = subprocess.run(args, capture_output=True, text=True, timeout=20)
  assert result.returncode == 0, result.stderr
  doc = json.loads(output.read_text())
  first, second = traces(doc)
  assert first["x"] == [0, 1, 0]
  assert second["x"] == expected
  assert first["y"] == second["y"] == [4, 8, 16]
  labels = ["IDLE", "FAULT"] + (["RUN"] if 2 in expected else [])
  assert_category_ticks(doc["plots"][0]["layout"]["xaxis"], labels)


def test_enum_json_labels_follow_independent_y_scale_groups(tmp_path):
  source = tmp_path / "categories.sdds"
  write_enum_dataset(source)
  doc = run_plot(source, tmp_path / "scales.json", "-column=x,state",
                 "-yScalesGroup=id=states", "-column=x,otherState",
                 "-yScalesGroup=id=others,right")
  first, second = traces(doc)
  assert first["y"] == second["y"] == [0, 1, 0]
  layout = doc["plots"][0]["layout"]
  assert len(layout["yaxes"]) == 2
  groups = {axis["id"]: axis for axis in layout["yaxes"]}
  assert_category_ticks(groups[first["meta"]["yAxisId"]], ["IDLE", "FAULT"])
  assert_category_ticks(groups[second["meta"]["yAxisId"]], ["RUN", "IDLE"])
  assert_category_ticks(layout["yaxis"], layout["yaxes"][0]["ticktext"])


@pytest.mark.parametrize("axis", ["x", "y"])
@pytest.mark.parametrize("field_type", ["enum", "string"])
def test_native_category_labels_include_all_files(tmp_path, axis, field_type):
  sources = [tmp_path / name for name in ("first.sdds", "second.sdds", "combined.sdds")]
  first, second = [42, -17, 42], [6, -17, 6]
  labels = {42: "IDLE", -17: "FAULT", 6: "RUN"}
  for source, codes in zip(sources, (first, second, first + second)):
    definition = (f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
                  if field_type == "enum" else '&column name=state, type=string &end\n')
    values = (4, 8, 16) * (len(codes) // 3)
    rows = "".join(f"{code if field_type == 'enum' else labels[code]} {value}\n"
                   for code, value in zip(codes, values))
    source.write_text(("SDDS6\n" if field_type == "enum" else "SDDS1\n") +
                      definition + "&column name=value, type=double &end\n"
                      "&data mode=ascii &end\n" + f"{len(codes)}\n" + rows,
                      encoding="utf8")
  selection = "state,value" if axis == "x" else "value,state"
  options = (f"-column={selection}", "-graphic=symbol", "-title=CategoryUnion",
             "-topline=", "-xlabel=State", "-ylabel=Value")
  multiple = native_png_data(tmp_path, "multiple", sources[:2], *options)
  combined = native_png_data(tmp_path, "combined", sources[2:], *options)
  assert multiple == combined


@pytest.mark.parametrize("axis", ["x", "y"])
def test_native_enum_labels_match_string_and_literal_labels(tmp_path, axis):
  source = tmp_path / "labels.sdds"
  source.write_text("SDDS6\n" +
                    f'&parameter name=stateParameter, type=enum, enum_values="{MAPPING}" &end\n' +
                    "&parameter name=textParameter, type=string &end\n" +
                    f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n' +
                    "&column name=textState, type=string &end\n"
                    "&column name=value, type=double &end\n"
                    "&data mode=ascii &end\n6\nRUN\n3\n42 IDLE 4\n-17 FAULT 8\n42 IDLE 16\n",
                    encoding="utf8")
  images = []
  for kind in ("enum", "string", "literal"):
    field = "state" if kind == "enum" else "textState"
    selection = f"{field},value" if axis == "x" else f"value,{field}"
    options = [f"-column={selection}", "-graphic=symbol", "-topline=",
               "-xlabel=State", "-ylabel=Value", f"-pointLabel={field}"]
    if kind == "literal":
      options.extend(("-title=[RUN]", "-legend=specified=State:RUN",
                      "-string=Annotation:RUN,pCoordinate=0.4,qCoordinate=0.5"))
    else:
      parameter = "stateParameter" if kind == "enum" else "textParameter"
      options.extend((f"-title=@{parameter},format=[%s]",
                      f"-legend=parameter={parameter},format=State:%s",
                      f"-string=@{parameter},pCoordinate=0.4,qCoordinate=0.5,format=Annotation:%s"))
    images.append(native_png_data(tmp_path, kind, [source], *options))
  assert images[0] == images[1] == images[2]


@pytest.mark.parametrize("axis", ["x", "y"])
@pytest.mark.parametrize("categorical_first", [False, True])
@pytest.mark.parametrize("field", ["state", "textState"])
def test_mixed_categorical_and_numeric_scale_is_rejected(tmp_path, axis,
                                                         categorical_first, field):
  source = tmp_path / "categories.sdds"
  write_enum_dataset(source)
  numeric = "-column=x,y"
  categorical = f"-column={field},y" if axis == "x" else f"-column=x,{field}"
  selections = (categorical, numeric) if categorical_first else (numeric, categorical)
  result = run_plot(source, tmp_path / "invalid.json", *selections, check=False)
  assert result.returncode > 0
  assert "enumerated and non-enumerated" in result.stderr.lower()


@pytest.mark.parametrize("encoding", ("ascii", "little-column"))
@pytest.mark.parametrize("compression", ("gz", "xz"))
def test_compressed_enum_columns_are_categorical(tmp_path, encoding, compression):
  source = tmp_path / "categories.sdds"
  write_enum_dataset(source, encoding)
  compressed = source.with_suffix(f".sdds.{compression}")
  compress = gzip.compress if compression == "gz" else lzma.compress
  compressed.write_bytes(compress(source.read_bytes()))
  doc = run_plot(compressed, tmp_path / "compressed.json", "-column=state,y")
  assert traces(doc)[0]["x"] == [0, 1, 0]
  assert traces(doc)[0]["y"] == [4, 8, 16]
  assert_category_ticks(doc["plots"][0]["layout"]["xaxis"], ["IDLE", "FAULT"])


@pytest.mark.parametrize("axis", ("x", "y"))
def test_enum_png_render_with_parameter_and_point_labels(tmp_path, axis):
  source = tmp_path / "categories.sdds"
  write_enum_dataset(source, "little-row")
  selection = "state,y" if axis == "x" else "x,state"
  args = [str(SDDSPLOT), str(source), f"-column={selection}",
          "-graphic=symbol", "-pointLabel=state", "-title=@stateParameter,format=[%s]",
          "-legend=parameter=stateParameter,format=State:%s",
          "-device=png,rootname=enum,onwhite"]
  result = subprocess.run(args, cwd=tmp_path, capture_output=True, text=True, timeout=20)
  assert result.returncode == 0, result.stderr
  pngs = [path for path in tmp_path.glob("enum*")
          if path.is_file() and path.read_bytes().startswith(b"\x89PNG\r\n\x1a\n")]
  assert pngs, "No enum plot PNG was generated"
  assert all(all(size > 0 for size in struct.unpack(">II", path.read_bytes()[16:24]))
             for path in pngs)


@pytest.mark.parametrize("encoding", ["ascii", "little-row"])
def test_numeric_parameter_error_bars_match_column_values(tmp_path, encoding):
  source, reference = tmp_path / "parameters.sdds", tmp_path / "columns.sdds"
  write_enum_dataset(source, encoding, pages=(6, 42))
  reference.write_text("SDDS1\n&column name=px, type=double &end\n"
                       "&column name=py, type=double &end\n"
                       "&data mode=ascii &end\n2\n1 10\n2 20\n", encoding="utf8")
  options = ("-graphic=errorbar", "-title=ParameterErrors", "-topline=",
             "-xlabel=X", "-ylabel=Y")
  parameters = native_png_data(tmp_path, "parameters", [source],
                               "-parameter=px,py,px,py", *options)
  columns = native_png_data(tmp_path, "columns", [reference],
                            "-column=px,py,px,py", *options)
  assert parameters == columns


@pytest.mark.parametrize("selection", ["state,y", "x,state", "textState,y", "x,textState"])
def test_categorical_transpose_requires_explicit_conversion(tmp_path, selection):
  source = tmp_path / "categories.sdds"
  write_enum_dataset(source)
  result = run_plot(source, tmp_path / "invalid.json", f"-column={selection}",
                    "-transpose", check=False)
  assert result.returncode > 0
  assert "categorical" in result.stderr.lower()
  assert "transpose" in result.stderr.lower()


@pytest.mark.parametrize("selection", ["state,otherState", "state,textState", "textState,state"])
def test_two_categorical_axes_are_rejected(tmp_path, selection):
  source = tmp_path / "categories.sdds"
  write_enum_dataset(source)
  result = run_plot(source, tmp_path / "invalid.json", f"-column={selection}", check=False)
  assert result.returncode > 0
  assert "both" in result.stderr.lower()


@pytest.mark.parametrize("encoding", ("ascii", "little-row"))
@pytest.mark.parametrize("selection", [
    "-parameter=stateParameter,py", "-parameter=px,stateParameter",
    "-array=stateArray,ay", "-array=ax,stateArray",
])
def test_numeric_parameter_and_array_plots_reject_enums(tmp_path, encoding, selection):
  source = tmp_path / "enums.sdds"
  write_enum_dataset(source, encoding, pages=(6, 42))
  result = run_plot(source, tmp_path / "invalid.json", selection, check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower() or "numeric" in result.stderr.lower()


@pytest.mark.parametrize("encoding", ("ascii", "little-row"))
@pytest.mark.parametrize("options", [
    ("-column=x,y", "-sort=state"),
    ("-column=x,y,state", "-graphic=errorbar"),
    ("-column=x,y,state,state", "-graphic=errorbar"),
    ("-parameter=px,py,stateParameter", "-graphic=errorbar"),
    ("-parameter=px,py,stateParameter,px", "-graphic=errorbar"),
    ("-parameter=px,py,px,stateParameter", "-graphic=errorbar"),
    ("-array=ax,ay,stateArray", "-graphic=errorbar"),
    ("-array=ax,ay,stateArray,ax", "-graphic=errorbar"),
    ("-array=ax,ay,ax,stateArray", "-graphic=errorbar"),
    ("-column=x,y", "-filter=column,state,-20,50"),
    ("-column=x,y", "-split=parameterChange=stateParameter,width=1"),
    ("-column=x,y", "-split=columnBin=state,width=1"),
    ("-column=x,y", "-factor=xparameter=stateParameter"),
    ("-column=x,y", "-offset=yparameter=stateParameter"),
    ("-column=x,y", "-modulus=xparameter=stateParameter"),
    ("-column=x,y", "-graphic=symbol,type=@state"),
    ("-column=x,y", "-graphic=symbol,subtype=@state"),
    ("-column=x,y", "-string=State,xCoordinate=@stateParameter,yCoordinate=1"),
    ("-column=x,y", "-drawLine=x0parameter=stateParameter,y0value=0,x1value=1,y1value=1"),
])
def test_numeric_operations_reject_enums(tmp_path, encoding, options):
  source = tmp_path / "enums.sdds"
  write_enum_dataset(source, encoding)
  result = run_plot(source, tmp_path / "invalid.json", *options, check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower() or "numeric" in result.stderr.lower()


@pytest.mark.parametrize("encoding", ("ascii", "little-row"))
@pytest.mark.parametrize("kind,name", [
    ("column", "state"), ("parameter", "stateParameter"), ("array", "stateArray"),
])
@pytest.mark.parametrize("wildcard", [False, True])
def test_unit_conversion_rejects_enums(tmp_path, encoding, kind, name, wildcard):
  source = tmp_path / "enums.sdds"
  write_enum_dataset(source, encoding)
  selection = "state*" if wildcard else name
  result = run_plot(source, tmp_path / "invalid.json", "-column=x,y",
                    f"-convertUnits={kind},{selection},scaled,state,2", check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower() or "numeric" in result.stderr.lower()


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ("column", "parameter"))
def test_enum_matching_uses_labels(tmp_path, encoding, kind):
  source = tmp_path / "enums.sdds"
  write_enum_dataset(source, encoding, pages=(6, 42))
  match = "-match=column,state=I*" if kind == "column" else "-match=parameter,stateParameter=I*"
  doc = run_plot(source, tmp_path / "matched.json", "-column=x,y", match)
  trace = traces(doc)[0]
  assert trace["x"] == ([1, 3, 4, 6] if kind == "column" else [4, 5, 6])
  assert trace["y"] == ([4, 16, 7, 19] if kind == "column" else [7, 11, 19])


@pytest.mark.parametrize("encoding", ("ascii", "little-row"))
@pytest.mark.parametrize("pattern,expected_x", [
    ("01", [1, 2, 3]), ("0*", [1, 2, 3]), ("01,!", [4, 5, 6]),
])
def test_enum_parameter_match_keeps_numeric_looking_labels_distinct(tmp_path, encoding,
                                                                   pattern, expected_x):
  source = tmp_path / "numeric_labels.sdds"
  write_enum_dataset(source, encoding, pages=(-17, 6),
                     parameter_mapping="-17:01,6:1,42:IDLE")
  doc = run_plot(source, tmp_path / "matched.json", "-column=x,y",
                 f"-match=parameter,stateParameter={pattern}")
  assert traces(doc)[0]["x"] == expected_x


@pytest.mark.parametrize("encoding", ("ascii", "little-row"))
def test_enum_parameter_match_accepts_long_labels(tmp_path, encoding):
  source = tmp_path / "long_label.sdds"
  label = "long-label-" + "a" * 4096
  write_enum_dataset(source, encoding, parameter_mapping=f"6:{label}")
  doc = run_plot(source, tmp_path / "matched.json", "-column=x,y",
                 "-match=parameter,stateParameter=long-label-*")
  assert traces(doc)[0]["x"] == [1, 2, 3]


@pytest.mark.parametrize("encoding", ("ascii", "little-row"))
@pytest.mark.parametrize("fixed", [False, True])
@pytest.mark.parametrize("format,expected", [(None, "RUN"), ("[%8s]", "[     RUN]")])
def test_enum_parameter_title_and_legend_display_labels(tmp_path, encoding, fixed,
                                                       format, expected):
  source = tmp_path / "enums.sdds"
  write_enum_dataset(source, encoding, fixed=fixed)
  suffix = f",format={format}" if format else ""
  doc = run_plot(source, tmp_path / "labels.json", "-column=x,y",
                 f"-title=@stateParameter{suffix}",
                 f"-legend=parameter=stateParameter{suffix}")
  assert doc["plots"][0]["layout"]["title"] == expected
  assert traces(doc)[0]["name"] == expected


@pytest.mark.parametrize("target", ("title", "legend"))
@pytest.mark.parametrize("format,expected", [
    ("%.2s", "RU"), ("[%-5.2s]", "[RU   ]"), ("%% %.2s %%", "% RU %"),
])
def test_enum_display_string_format_accepts_precision_and_literal_percent(tmp_path,
                                                                         target,
                                                                         format,
                                                                         expected):
  source = tmp_path / "enums.sdds"
  write_enum_dataset(source)
  option = (f"-title=@stateParameter,format={format}" if target == "title" else
            f"-legend=parameter=stateParameter,format={format}")
  doc = run_plot(source, tmp_path / "formatted.json", "-column=x,y", option)
  actual = (doc["plots"][0]["layout"]["title"] if target == "title" else
            traces(doc)[0]["name"])
  assert actual == expected


@pytest.mark.parametrize("target", ("title", "legend"))
@pytest.mark.parametrize("format,expected", [
    ("%s", "RUN"), ("[%8s]", "[     RUN]"), ("[%-5.2s]", "[RU   ]"),
    ("%% %.2s %%", "% RU %"), ("%s%s", None), ("%s%d", None),
    ("%s%", None), ("%*s", None), ("%.*s", None), ("%ls", None),
])
def test_legacy_string_parameter_display_formats(tmp_path, target, format, expected):
  source = tmp_path / "strings.sdds"
  source.write_text("SDDS1\n"
                    "&parameter name=state, type=string, fixed_value=RUN &end\n"
                    "&column name=x, type=double &end\n"
                    "&column name=y, type=double &end\n"
                    "&data mode=ascii &end\n2\n1 4\n2 8\n", encoding="utf8")
  option = (f"-title=@state,format={format}" if target == "title" else
            f"-legend=parameter=state,format={format}")
  output = tmp_path / "formatted.json"
  if expected is None:
    result = run_plot(source, output, "-column=x,y", option, check=False)
    assert result.returncode > 0
    assert "format" in result.stderr.lower()
  else:
    doc = run_plot(source, output, "-column=x,y", option)
    actual = (doc["plots"][0]["layout"]["title"] if target == "title" else
              traces(doc)[0]["name"])
    assert actual == expected


@pytest.mark.parametrize("target", ("title", "legend"))
@pytest.mark.parametrize("format", ("%s%s", "%d", "%*s", "%.*s", "%ls"))
def test_enum_display_string_format_rejects_unsupported_conversions(tmp_path, target,
                                                                   format):
  source = tmp_path / "enums.sdds"
  write_enum_dataset(source)
  option = (f"-title=@stateParameter,format={format}" if target == "title" else
            f"-legend=parameter=stateParameter,format={format}")
  result = run_plot(source, tmp_path / "invalid.json", "-column=x,y", option, check=False)
  assert result.returncode > 0
  assert "format" in result.stderr.lower()


def test_long_enum_labels_display_without_editing(tmp_path):
  source = tmp_path / "long_labels.sdds"
  label = "a" * 4096
  write_enum_dataset(source, parameter_mapping=f"6:{label}",
                     column_mapping=f"-17:{label},6:RUN,42:IDLE")
  doc = run_plot(source, tmp_path / "long_labels.json", "-column=state,y",
                 "-title=@stateParameter", "-legend=parameter=stateParameter",
                 "-string=@stateParameter,xCoordinate=0,yCoordinate=1")
  assert doc["plots"][0]["layout"]["title"] == label
  assert traces(doc)[0]["name"] == label
  assert_category_ticks(doc["plots"][0]["layout"]["xaxis"], ["IDLE", label])


@pytest.mark.parametrize("target", [
    "title", "topline", "xlabel", "ylabel", "legend", "string",
    "pointLabel", "enumeratedScales",
])
def test_long_enum_label_edit_is_rejected(tmp_path, target):
  source = tmp_path / "long_labels.sdds"
  label = "a" * 4096
  write_enum_dataset(source, parameter_mapping=f"6:{label}",
                     column_mapping=f"-17:{label},6:RUN,42:IDLE")
  selection = "-column=x,y"
  if target == "legend":
    option = "-legend=parameter=stateParameter,editCommand=u"
  elif target == "string":
    option = "-string=@stateParameter,xCoordinate=0,yCoordinate=1,edit=u"
  elif target == "pointLabel":
    option = "-pointLabel=state,edit=u"
  elif target == "enumeratedScales":
    selection = "-column=state,y"
    option = "-enumeratedScales=editCommand=u"
  else:
    option = f"-{target}=@stateParameter,edit=u"
  result = run_plot(source, tmp_path / "invalid.json", selection, option, check=False)
  assert result.returncode > 0
  assert "label is too long to edit" in result.stderr


def test_short_enum_label_format_can_be_edited(tmp_path):
  source = tmp_path / "long_label.sdds"
  write_enum_dataset(source, parameter_mapping=f"6:{'a' * 4096}")
  doc = run_plot(source, tmp_path / "short_labels.json", "-column=x,y",
                 "-title=@stateParameter,format=%.16s,edit=u",
                 "-topline=@stateParameter,format=%.16s,edit=u",
                 "-xlabel=@stateParameter,format=%.16s,edit=u",
                 "-ylabel=@stateParameter,format=%.16s,edit=u",
                 "-legend=parameter=stateParameter,format=%.16s,editCommand=u",
                 "-string=@stateParameter,xCoordinate=0,yCoordinate=1,format=%.16s,edit=u")
  expected = "A" + "a" * 15
  layout = doc["plots"][0]["layout"]
  assert layout["title"] == layout["topline"] == expected
  assert layout["xaxis"]["title"] == layout["yaxis"]["title"] == expected
  assert traces(doc)[0]["name"] == expected


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ("column", "parameter", "array"))
@pytest.mark.parametrize("valid_prefix", [False, True])
def test_invalid_enum_page_is_not_silent_end_of_file(tmp_path, encoding, kind,
                                                    valid_prefix):
  source, output = tmp_path / "invalid.sdds", tmp_path / "invalid.json"
  write_enum_dataset(source, encoding, pages=(6, 42) if valid_prefix else (6,),
                     invalid_kind=kind)
  result = run_plot(source, output, "-column=x,y", check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
  assert "999" in result.stderr


@pytest.mark.parametrize("encoding", ENCODINGS)
def test_sparse_read_still_rejects_unselected_invalid_enum_row(tmp_path, encoding):
  source = tmp_path / "invalid.sdds"
  write_enum_dataset(source, encoding, invalid_kind="column")
  result = run_plot(source, tmp_path / "invalid.json", "-column=x,y",
                    "-presparse=2", check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
  assert "999" in result.stderr


@pytest.mark.parametrize("valid_prefix", [False, True])
def test_malformed_enum_token_is_not_recovered_as_partial_page(tmp_path, valid_prefix):
  source = tmp_path / "malformed.sdds"
  write_enum_dataset(source, pages=(6, 42) if valid_prefix else (6,),
                     invalid_kind="column")
  source.write_text(source.read_text().replace("999", "not-an-integer"))
  result = run_plot(source, tmp_path / "invalid.json", "-column=x,y", check=False)
  assert result.returncode > 0
  assert "scanning" in result.stderr.lower() or "enum" in result.stderr.lower()
