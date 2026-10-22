"""SDDS6 ENUM preservation and rejection paths in sddsxref."""

import gzip
import lzma
import struct
import subprocess
from pathlib import Path

import pytest

from sdds_test_utils import BIN_DIR


TOOLS = ("sddsxref", "sdds2stream", "sddsconvert", "sddscheck")
pytestmark = pytest.mark.skipif(
  not all((BIN_DIR / tool).exists() for tool in TOOLS), reason="tools not built"
)
MAPPING = "10:ON,-1:FAULT,2147483647:MAX,-2147483648:MIN"
ENCODINGS = ("ascii", "little-row", "little-column", "big-row", "big-column")


def run(tool, *args, check=True):
  result = subprocess.run(
    [str(BIN_DIR / tool), *map(str, args)], capture_output=True, text=True,
    encoding="utf8", timeout=30,
  )
  if check:
    assert result.returncode == 0, result.stderr
  return result


def stream(path, selection, codes=False):
  options = [selection, "-noquotes"]
  if codes:
    options.append("-enumCodes")
  return run("sdds2stream", path, *options).stdout.split()


def enum_dataset(path, prefix, pages, encoding="ascii", invalid_kind=None):
  """Encode inputs independently of the library's writer, including binary order."""
  binary = encoding != "ascii"
  endian = ">" if encoding.startswith("big") else "<"
  header = ("SDDS6\n"
            f'&parameter name={prefix}Mode, type=enum, enum_values="{MAPPING}" &end\n'
            f'&parameter name={prefix}Fixed, type=enum, enum_values="{MAPPING}", fixed_value=10 &end\n'
            f'&array name={prefix}States, type=enum, enum_values="{MAPPING}" &end\n'
            '&column name=id, type=long &end\n'
            '&column name=key, type=string &end\n'
            f'&column name={prefix}State, type=enum, enum_values="{MAPPING}" &end\n'
            f'&column name={prefix}Value, type=double &end\n' +
            (f'&data mode=binary, endian={"big" if endian == ">" else "little"}, '
             f'column_major_order={int(encoding.endswith("column"))} &end\n' if binary else
             '&data mode=ascii &end\n'))
  body = []
  for index, rows in enumerate(pages):
    invalid = invalid_kind if index == len(pages) - 1 else None
    mode = 42 if invalid == "parameter" else 10
    states = [42 if invalid == "array" else -1, 2147483647]
    columns = [[row[0] for row in rows], [f"K{row[0]}" for row in rows],
               [42 if invalid == "column" and i == 0 else row[1]
                for i, row in enumerate(rows)], [row[0] * 10.0 for row in rows]]
    if binary:
      pack = lambda fmt, *values: struct.pack(endian + fmt, *values)
      page = pack("ii", len(rows), mode) + pack("iii", 2, *states)
      if encoding.endswith("column"):
        values = ((value, i) for i, column in enumerate(columns) for value in column)
      else:
        values = ((value, i) for row in zip(*columns) for i, value in enumerate(row))
      for value, i in values:
        if i == 1:
          encoded = value.encode("utf8")
          page += pack("i", len(encoded)) + encoded
        else:
          page += pack("d" if i == 3 else "i", value)
      body.append(page)
    else:
      page = [str(mode), "2", " ".join(map(str, states)), str(len(rows))]
      page.extend(" ".join(map(str, row)) for row in zip(*columns))
      body.append(("\n".join(page) + "\n").encode("utf8"))
  data = header.encode("utf8") + b"".join(body)
  if path.suffix == ".gz":
    data = gzip.compress(data)
  elif path.suffix in (".xz", ".lzma"):
    data = lzma.compress(data, format=lzma.FORMAT_XZ if path.suffix == ".xz" else lzma.FORMAT_ALONE)
  path.write_bytes(data)
  return path


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("suffix", [".sdds", ".sdds.gz", ".sdds.xz", ".sdds.lzma"])
def test_enum_fields_survive_cross_reference(tmp_path, encoding, suffix):
  primary = enum_dataset(tmp_path / f"primary{suffix}", "left",
                         [[(1, -1), (2, 2147483647)], [(2, -2147483648), (1, 10)]], encoding)
  reference = enum_dataset(tmp_path / f"reference{suffix}", "right",
                           [[(2, 10), (1, -2147483648)], [(1, 2147483647), (2, -1)]], encoding)
  output = tmp_path / f"output{suffix}"
  run("sddsxref", primary, reference, output, "-equate=id", "-take=rightState",
      "-transfer=parameter,right*", "-transfer=array,rightStates",
      "-rename=column,rightState=joinedState", "-rename=parameter,rightMode=joinedMode",
      "-rename=array,rightStates=joinedStates")
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, "-column=leftState", True) == ["-1", "2147483647", "-2147483648", "10"]
  assert stream(output, "-column=joinedState", True) == ["-2147483648", "10", "-1", "2147483647"]
  assert stream(output, "-parameter=joinedMode") == ["ON", "ON"]
  assert stream(output, "-parameter=leftFixed") == ["ON", "ON"]
  assert stream(output, "-parameter=rightFixed") == ["ON", "ON"]
  assert stream(output, "-array=joinedStates") == ["FAULT", "MAX", "FAULT", "MAX"]
  ascii_output = tmp_path / "ascii.sdds"
  run("sddsconvert", output, ascii_output, "-ascii")
  text = ascii_output.read_text(encoding="utf8")
  assert text.startswith("SDDS6\n")
  assert text.count(f'enum_values="{MAPPING}"') == 8


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("encoding", ["ascii", "little-row"])
@pytest.mark.parametrize("valid_prefix", [False, True])
@pytest.mark.parametrize("bad_file,threads", [("primary", 1), ("reference", 1), ("second", 2)])
def test_invalid_enum_pages_are_not_end_of_file(tmp_path, kind, encoding, valid_prefix,
                                              bad_file, threads):
  pages = [[(1, 10)]] * (2 if valid_prefix else 1)
  paths = {}
  for name in ("primary", "reference", "second"):
    paths[name] = enum_dataset(tmp_path / f"{name}.sdds", name, pages, encoding,
                               invalid_kind=kind if name == bad_file else None)
  result = run("sddsxref", paths["primary"], paths["reference"], paths["second"],
               tmp_path / "output.sdds", "-equate=id", "-take=*Value",
               f"-threads={threads}", check=False)
  assert result.returncode > 0
  assert str(paths[bad_file]) in result.stderr
  assert "enum" in result.stderr.lower()
  assert "42" in result.stderr


def field_dataset(path, kind, mapping=MAPPING, code=10, fixed=False, rows=1, dimensions=1):
  declaration = f'&{kind} name=state, type=enum, enum_values="{mapping}"'
  if kind == "parameter" and fixed:
    declaration += f', fixed_value={code}'
  if kind == "array":
    declaration += f', dimensions={dimensions}'
  data = ""
  if kind == "parameter" and not fixed:
    data += f"{code}\n"
  if kind == "array":
    data += " ".join(["1"] * dimensions) + f"\n{code}\n"
  data += f"{rows}\n"
  data += (f"1 {code}\n" if kind == "column" else "1\n") * rows
  path.write_text('SDDS6\n&column name=id, type=long &end\n' + declaration +
                  ' &end\n&data mode=ascii &end\n' + data,
                  encoding="utf8")
  return path


@pytest.mark.parametrize("kind,fixed", [("column", False), ("parameter", False),
                                       ("parameter", True), ("array", False)])
def test_enum_replacement_with_equivalent_mapping(tmp_path, kind, fixed):
  primary = field_dataset(tmp_path / "primary.sdds", kind, fixed=fixed)
  reference = field_dataset(tmp_path / "reference.sdds", kind,
                            mapping=",".join(reversed(MAPPING.split(","))), code=-1)
  output = tmp_path / "output.sdds"
  run("sddsxref", primary, reference, output, f"-replace={kind},state")
  assert stream(output, f"-{kind}=state", True) == ["-1"]
  assert stream(output, f"-{kind}=state") == ["FAULT"]


def test_fixed_enum_parameter_replacement_follows_reference_pages(tmp_path):
  primary = field_dataset(tmp_path / "primary.sdds", "parameter", fixed=True)
  primary.write_text(primary.read_text() + "1\n1\n", encoding="utf8")
  reference = field_dataset(tmp_path / "reference.sdds", "parameter", code=-1)
  reference.write_text(reference.read_text() + "2147483647\n1\n1\n", encoding="utf8")
  output = tmp_path / "output.sdds"
  run("sddsxref", primary, reference, output, "-replace=parameter,state")
  assert stream(output, "-parameter=state", True) == ["-1", "2147483647"]


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("rows", [0, 1])
def test_enum_replacement_rejects_incompatible_mapping(tmp_path, kind, rows):
  primary = field_dataset(tmp_path / "primary.sdds", kind, rows=rows)
  reference = field_dataset(tmp_path / "reference.sdds", kind,
                            mapping=MAPPING.replace("ON", "DIFFERENT"), rows=rows)
  result = run("sddsxref", primary, reference, tmp_path / "output.sdds",
               f"-replace={kind},state", check=False)
  assert result.returncode > 0
  assert "Incompatible enum mappings" in result.stderr


@pytest.mark.parametrize("kind", ["parameter", "array"])
@pytest.mark.parametrize("field_type", ["enum", "long", "string"])
def test_replacement_patterns_select_each_requested_field_once(tmp_path, kind, field_type):
  paths = []
  for name, code in (("primary", 10), ("reference", -1)):
    path = tmp_path / f"{name}.sdds"
    mapping = f', enum_values="{MAPPING}"' if field_type == "enum" else ""
    declarations = "".join(f'&{kind} name={field}, type={field_type}{mapping} &end\n'
                           for field in ("state", "untouched"))
    values = (f"{code}\n" if kind == "parameter" else f"1\n{code}\n") * 2
    path.write_text(("SDDS6\n" if field_type == "enum" else "SDDS1\n") + declarations +
                    '&column name=id, type=long &end\n&data mode=ascii &end\n' +
                    values + "1\n1\n", encoding="utf8")
    paths.append(path)
  output = tmp_path / "output.sdds"
  run("sddsxref", *paths, output, f"-replace={kind},state,sta*")
  assert stream(output, f"-{kind}=state", field_type == "enum") == ["-1"]
  assert stream(output, f"-{kind}=untouched", field_type == "enum") == ["10"]


def test_enum_array_replacement_checks_dimension_count(tmp_path):
  primary = field_dataset(tmp_path / "primary.sdds", "array", dimensions=2)
  reference = field_dataset(tmp_path / "reference.sdds", "array", dimensions=1)
  result = run("sddsxref", primary, reference, tmp_path / "output.sdds",
               "-replace=array,state", check=False)
  assert result.returncode > 0
  assert "different numbers of dimensions" in result.stderr


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("primitive", ["long", "string"])
@pytest.mark.parametrize("enum_side", ["primary", "reference"])
def test_replacement_cannot_implicitly_convert_enum_type(tmp_path, kind, primitive, enum_side):
  paths = {}
  for name in ("primary", "reference"):
    path = field_dataset(tmp_path / f"{name}.sdds", kind)
    if name != enum_side:
      text = path.read_text().replace(f'type=enum, enum_values="{MAPPING}"', f'type={primitive}')
      path.write_text(text, encoding="utf8")
    paths[name] = path
  result = run("sddsxref", paths["primary"], paths["reference"], tmp_path / "output.sdds",
               f"-replace={kind},state", check=False)
  assert result.returncode > 0
  assert "Incompatible enum mappings" in result.stderr


@pytest.mark.parametrize("option", ["-equate=state", "-match=state", "-wildMatch=state"])
@pytest.mark.parametrize("enum_side", ["primary", "reference"])
@pytest.mark.parametrize("rows", [0, 1])
def test_enum_keys_require_explicit_conversion(tmp_path, option, enum_side, rows):
  paths = {}
  primitive = "long" if option.startswith("-equate") else "string"
  for name in ("primary", "reference"):
    path = field_dataset(tmp_path / f"{name}.sdds", "column", rows=rows)
    if name != enum_side:
      path.write_text(path.read_text().replace(f'type=enum, enum_values="{MAPPING}"',
                                               f'type={primitive}'), encoding="utf8")
    paths[name] = path
  result = run("sddsxref", paths["primary"], paths["reference"], tmp_path / "output.sdds",
               option, check=False)
  assert result.returncode > 0
  assert ("not numeric type" if primitive == "long" else "not string type") in result.stderr


@pytest.mark.parametrize("option", ["match", "equate"])
@pytest.mark.parametrize("enum_side", ["primary", "reference"])
def test_long_enum_key_errors_include_complete_names(tmp_path, option, enum_side):
  name = "state" * 400
  paths = []
  for filename in ("primary", "reference"):
    path = field_dataset(tmp_path / f"{filename}.sdds", "column")
    text = path.read_text().replace("name=state,", f"name={name},")
    if filename != enum_side:
      text = text.replace(f'type=enum, enum_values="{MAPPING}"',
                          f'type={"string" if option == "match" else "long"}')
    path.write_text(text, encoding="utf8")
    paths.append(path)
  result = run("sddsxref", *paths, tmp_path / "output.sdds", f"-{option}={name}", check=False)
  assert result.returncode > 0
  assert name in result.stderr
  assert ("not string type" if option == "match" else "not numeric type") in result.stderr


@pytest.mark.parametrize("hash_lookup", [False, True])
@pytest.mark.parametrize("reuse_page", [False, True])
@pytest.mark.parametrize("threads", [1, 2])
def test_string_matches_preserve_enum_fields_with_multiple_references(tmp_path, hash_lookup,
                                                                   reuse_page, threads):
  primary = enum_dataset(tmp_path / "primary.sdds", "left", [[(1, 10), (2, -1)]] * 2)
  pages = [[(2, 2147483647), (1, -2147483648)]] * (1 if reuse_page else 2)
  references = [enum_dataset(tmp_path / f"{name}.sdds", name, pages)
                for name in ("right", "third")]
  output = tmp_path / "output.sdds"
  options = ["-hashLookup"] if hash_lookup else []
  if reuse_page:
    options.append("-reuse=rows,page")
  run("sddsxref", primary, *references, output, "-match=key", f"-threads={threads}", *options)
  assert stream(output, "-column=leftState") == ["ON", "FAULT"] * 2
  for name in ("right", "third"):
    assert stream(output, f"-column={name}State", True) == ["-2147483648", "2147483647"] * 2


@pytest.mark.parametrize("zero_mapped", [False, True])
@pytest.mark.parametrize("selection", ["-match=key", "-equate=id"])
def test_fillin_requires_a_mapped_zero_for_missing_enum_columns(tmp_path, zero_mapped, selection):
  primary = enum_dataset(tmp_path / "primary.sdds", "left", [[(1, 10), (2, -1)]])
  reference = enum_dataset(tmp_path / "reference.sdds", "right", [[(1, -1)]])
  if zero_mapped:
    reference.write_text(reference.read_text().replace(MAPPING, MAPPING + ",0:MISSING"),
                         encoding="utf8")
  output = tmp_path / "output.sdds"
  result = run("sddsxref", primary, reference, output, selection, "-take=rightState", "-fillIn",
               check=False)
  if zero_mapped:
    assert result.returncode == 0, result.stderr
    assert stream(output, "-column=rightState") == ["FAULT", "MISSING"]
    assert stream(output, "-column=leftState") == ["ON", "FAULT"]
  else:
    assert result.returncode > 0
    assert "enum" in result.stderr.lower()


def test_normal_reference_eof_still_allows_enum_fillin(tmp_path):
  primary = enum_dataset(tmp_path / "primary.sdds", "left", [[(1, 10)], [(2, -1)]])
  reference = enum_dataset(tmp_path / "reference.sdds", "right", [[(1, -1)]])
  reference.write_text(reference.read_text().replace(MAPPING, MAPPING + ",0:MISSING"),
                       encoding="utf8")
  output = tmp_path / "output.sdds"
  run("sddsxref", primary, reference, output, "-equate=id", "-take=rightState", "-fillIn")
  assert stream(output, "-column=rightState") == ["FAULT", "MISSING"]
  assert stream(output, "-column=leftState") == ["ON", "FAULT"]


@pytest.mark.parametrize("bad_file", ["primary", "reference"])
def test_failed_in_place_cross_reference_keeps_original_input(tmp_path, bad_file):
  paths = {name: enum_dataset(tmp_path / f"{name}.sdds", name, [[(1, 10)], [(1, -1)]],
                              invalid_kind="column" if name == bad_file else None)
           for name in ("primary", "reference")}
  original = paths["primary"].read_bytes()
  result = run("sddsxref", paths["primary"], paths["reference"], "-take=referenceValue",
               "-equate=id", check=False)
  assert result.returncode > 0
  assert paths["primary"].read_bytes() == original


@pytest.mark.parametrize("kind", ["parameter", "array"])
@pytest.mark.parametrize("threads", [1, 2])
@pytest.mark.parametrize("field_type", ["enum", "long", "string"])
@pytest.mark.parametrize("overlapping", [False, True])
def test_transfer_selection_is_independent_for_each_reference(tmp_path, kind, threads,
                                                            field_type, overlapping):
  primary = field_dataset(tmp_path / "primary.sdds", "column")
  references = []
  mapping = f', enum_values="{MAPPING}"' if field_type == "enum" else ""
  for prefix, fields in (("first", ("Selected", "Other", "Also")),
                         ("second", ("Other", "Also", "Selected"))):
    path = tmp_path / f"{prefix}.sdds"
    declarations = "".join(
      f'&{kind} name={prefix}{field}, type={field_type}{mapping} &end\n'
      for field in fields
    )
    values = "".join(("1\n" if kind == "array" else "") +
                     ("-1\n" if field == "Selected" else "10\n") for field in fields)
    path.write_text("SDDS6\n" + declarations +
                    '&column name=id, type=long &end\n&data mode=ascii &end\n' +
                    values + "1\n1\n", encoding="utf8")
    references.append(path)
  output = tmp_path / "output.sdds"
  options = [f"-transfer={kind},*Also", f"-transfer={kind},*Selected"] if overlapping else []
  run("sddsxref", primary, *references, output, "-leave=*", f"-transfer={kind},*Selected",
      f"-threads={threads}", *options)
  text = output.read_text(encoding="utf8")
  for prefix in ("first", "second"):
    assert f"{prefix}Other" not in text
    assert text.count(f'name="{prefix}Selected"') == 1
    assert stream(output, f"-{kind}={prefix}Selected") == ["FAULT" if field_type == "enum" else "-1"]
    if overlapping:
      assert text.count(f'name="{prefix}Also"') == 1
      assert stream(output, f"-{kind}={prefix}Also") == ["ON" if field_type == "enum" else "10"]
    else:
      assert f"{prefix}Also" not in text


@pytest.mark.parametrize("option", ["take", "replace"])
def test_missing_long_enum_column_errors_include_complete_names(tmp_path, option):
  name = "state" * 400
  primary = field_dataset(tmp_path / "primary.sdds", "column")
  reference = field_dataset(tmp_path / "reference.sdds", "column")
  selection = f"-take={name}" if option == "take" else f"-replace=column,{name}"
  result = run("sddsxref", primary, reference, tmp_path / "output.sdds", selection, check=False)
  assert result.returncode > 0
  assert name in result.stderr
  assert "not found" in result.stderr


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_long_enum_names_can_be_renamed_and_reject_unsafe_editing(tmp_path, kind):
  name = "state" * 400
  primary = field_dataset(tmp_path / "primary.sdds", "column")
  reference = field_dataset(tmp_path / "reference.sdds", kind, code=-1)
  reference.write_text(reference.read_text().replace("name=state,", f"name={name},"),
                       encoding="utf8")
  options = [] if kind == "column" else [f"-transfer={kind},{name}"]
  result = run("sddsxref", primary, reference, tmp_path / "edited.sdds",
               f"-editnames={kind},{name},a", *options, check=False)
  assert result.returncode > 0
  assert "name too long for -editnames" in result.stderr
  assert "-rename" in result.stderr
  output = tmp_path / "renamed.sdds"
  run("sddsxref", primary, reference, output, f"-rename={kind},{name}=shortState", *options)
  assert stream(output, f"-{kind}=shortState") == ["FAULT"]


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_escaped_enum_labels_survive_renamed_transfers(tmp_path, kind):
  primary = field_dataset(tmp_path / "primary.sdds", "column")
  fixture = Path(__file__).resolve().parents[1] / "SDDSlib/testdata/enums/valid-escapes.sdds"
  declaration = fixture.read_text(encoding="utf8").splitlines()[1].replace("&column", f"&{kind}")
  data = {"column": "1\n1 10\n", "parameter": "10\n1\n1\n", "array": "1\n10\n1\n1\n"}
  reference = tmp_path / "reference.sdds"
  reference.write_text('SDDS6\n&column name=id, type=long &end\n' + declaration +
                       '\n&data mode=ascii &end\n' + data[kind], encoding="utf8")
  options = [] if kind == "column" else [f"-transfer={kind},state"]
  output = tmp_path / "output.sdds"
  run("sddsxref", primary, reference, output, f"-rename={kind},state=joinedState", *options)
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, f"-{kind}=joinedState", True) == ["10"]
  expected = run("sdds2stream", reference, f"-{kind}=state", "-noquotes").stdout
  assert "雪 " in expected
  assert run("sdds2stream", output, f"-{kind}=joinedState", "-noquotes").stdout == expected


@pytest.mark.parametrize("enum_side", ["primary", "reference"])
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_legacy_and_enum_inputs_produce_sdds6_output(tmp_path, enum_side, encoding):
  paths = {}
  for name in ("primary", "reference"):
    path = enum_dataset(tmp_path / f"{name}.sdds", name, [[(1, -1)]], encoding)
    if name != enum_side:
      path.write_bytes(path.read_bytes().replace(
        b"SDDS6\n", b"SDDS1\n" if encoding == "ascii" else b"SDDS5\n").replace(
        f'type=enum, enum_values="{MAPPING}"'.encode(), b"type=long"))
    paths[name] = path
  output = tmp_path / "output.sdds"
  run("sddsxref", paths["primary"], paths["reference"], output,
      "-equate=id", "-transfer=parameter,reference*", "-transfer=array,reference*")
  assert output.read_bytes().startswith(b"SDDS6\n")
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, f"-column={enum_side}State") == ["FAULT"]
  assert stream(output, f"-parameter={enum_side}Fixed") == ["ON"]
  assert stream(output, f"-array={enum_side}States") == ["FAULT", "MAX"]


@pytest.mark.parametrize("selection", [None, "-match=key", "-wildMatch=key", "-equate=id"])
@pytest.mark.parametrize("major_order", ["row", "column"])
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_enum_row_copy_paths_and_output_order(tmp_path, selection, major_order, encoding):
  primary = enum_dataset(tmp_path / "primary.sdds", "left", [[(1, 10), (2, -1)]], encoding)
  reference = enum_dataset(tmp_path / "reference.sdds", "right",
                           [[(2, -2147483648), (1, 2147483647)]], encoding)
  output = tmp_path / "output.sdds"
  options = [selection] if selection else []
  run("sddsxref", primary, reference, output, "-take=rightState",
      "-editnames=column,rightState,%g/right/joined/", f"-majorOrder={major_order}", *options)
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, "-column=leftState", True) == ["10", "-1"]
  expected = ["2147483647", "-2147483648"] if selection else ["-2147483648", "2147483647"]
  assert stream(output, "-column=joinedState", True) == expected
  if encoding != "ascii":
    header = output.read_bytes().split(b"&data", 1)[1].split(b"&end", 1)[0]
    assert f"column_major_order={int(major_order == 'column')}".encode() in header


@pytest.mark.skipif(not (BIN_DIR / "sddscast").exists(), reason="sddscast not built")
@pytest.mark.parametrize("target_type,selection", [("string", "match"), ("long", "equate")])
@pytest.mark.parametrize("hash_lookup", [False, True])
def test_explicit_enum_key_casts_match_labels_or_codes(tmp_path, target_type, selection, hash_lookup):
  primary = enum_dataset(tmp_path / "primary.sdds", "left", [[(1, -1), (2, 10)]])
  reference = enum_dataset(tmp_path / "reference.sdds", "right", [[(7, 10), (8, -1)]])
  reference.write_text(reference.read_text().replace("10:ON,-1:FAULT", "10:FAULT,-1:ON"),
                       encoding="utf8")
  inputs = []
  for path, prefix in ((primary, "left"), (reference, "right")):
    converted = tmp_path / f"{prefix}-cast.sdds"
    run("sddscast", path, converted, f"-cast=column,{prefix}State,enum,{target_type}")
    inputs.append(converted)
  output = tmp_path / "output.sdds"
  options = ["-hashLookup"] if hash_lookup else []
  run("sddsxref", *inputs, output, f"-{selection}=leftState=rightState",
      "-take=rightValue", *options)
  expected = [70, 80] if target_type == "string" else [80, 70]
  assert list(map(float, stream(output, "-column=rightValue"))) == expected
  assert stream(output, "-parameter=leftMode") == ["ON"]


@pytest.mark.parametrize("replace", [False, True])
@pytest.mark.parametrize("binary", [False, True])
def test_enum_array_transfer_preserves_changing_dimensions_and_empty_pages(tmp_path, replace, binary):
  primary = field_dataset(tmp_path / "primary.sdds", "array" if replace else "column")
  primary.write_text(primary.read_text() + ("1\n10\n" if replace else "") +
                     ("1\n1\n" if replace else "1\n1 10\n"), encoding="utf8")
  reference = tmp_path / "reference.sdds"
  reference.write_text(
    f'SDDS6\n&array name=state, type=enum, enum_values="{MAPPING}" &end\n'
    '&column name=id, type=long &end\n&data mode=ascii &end\n'
    '2\n-1 10\n1\n1\n0\n1\n1\n', encoding="utf8")
  if binary:
    converted = tmp_path / "reference-binary.sdds"
    run("sddsconvert", reference, converted, "-binary")
    reference = converted
  output = tmp_path / "output.sdds"
  run("sddsxref", primary, reference, output,
      "-replace=array,state" if replace else "-transfer=array,state", "-leave=*")
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, "-array=state", True) == ["-1", "10"]
