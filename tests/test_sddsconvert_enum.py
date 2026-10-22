"""SDDS6 ENUM preservation, projection, and error handling in sddsconvert."""

import gzip
import lzma
import struct
import subprocess
from pathlib import Path

import pytest

from sdds_test_utils import BIN_DIR


TOOLS = ("sddsconvert", "sdds2stream", "sddscheck")
pytestmark = pytest.mark.skipif(
  not all((BIN_DIR / tool).exists() for tool in TOOLS), reason="tools not built"
)
FIXTURES = Path(__file__).resolve().parents[1] / "SDDSlib/testdata/enums"
MAPPING = "0:OFF,10:ON,-1:FAULT,2147483647:MAX,-2147483648:MIN"
ENCODINGS = ("ascii", "little-row", "little-column", "big-row", "big-column")
PAGES = ((-2147483648, 0, 2147483647), (10, -1), ())


def run(tool, *args, check=True):
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)],
                          capture_output=True, text=True, encoding="utf8", timeout=30)
  if check:
    assert result.returncode == 0, result.stderr
  return result


def stream(path, selection, codes=False):
  options = [selection, "-noquotes"]
  if codes:
    options.append("-enumCodes")
  return run("sdds2stream", path, *options).stdout.split()


def uncompressed(path):
  data = path.read_bytes()
  if path.suffix == ".gz":
    return gzip.decompress(data)
  if path.suffix in (".xz", ".lzma"):
    return lzma.decompress(data)
  return data


def dataset(path, encoding="ascii", invalid_kind=None, invalid_row=0):
  """Write independent ASCII/binary fixtures with changing and empty array shapes."""
  binary = encoding != "ascii"
  endian = ">" if encoding.startswith("big") else "<"
  header = ("SDDS6\n"
            f'&parameter name=mode, type=enum, enum_values="{MAPPING}" &end\n'
            f'&parameter name=fixed, type=enum, enum_values="{MAPPING}", fixed_value=-1 &end\n'
            f'&array name=states, type=enum, enum_values="{MAPPING}", dimensions=2 &end\n'
            '&column name=id, type=long &end\n'
            f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
            '&column name=value, type=double, units=m &end\n' +
            (f'&data mode=binary, endian={"big" if endian == ">" else "little"}, '
             f'column_major_order={int(encoding.endswith("column"))} &end\n' if binary else
             '&data mode=ascii &end\n'))
  body = []
  for page_index, codes in enumerate(PAGES):
    mode = (-1, 10, 0)[page_index]
    shape = ((1, 3), (2, 1), (0, 2))[page_index]
    states = list(codes)
    columns = [list(range(1, len(codes) + 1)), list(codes), [1.5] * len(codes)]
    if page_index == 1:
      if invalid_kind == "parameter":
        mode = 42
      elif invalid_kind == "array":
        states[0] = 42
      elif invalid_kind == "column":
        columns[1][invalid_row] = 42
    if binary:
      pack = lambda fmt, *values: struct.pack(endian + fmt, *values)
      page = pack("ii", len(codes), mode) + pack("ii", *shape) + pack("i" * len(states), *states)
      if encoding.endswith("column"):
        values = ((value, i) for i, column in enumerate(columns) for value in column)
      else:
        values = ((value, i) for row in zip(*columns) for i, value in enumerate(row))
      for value, column_index in values:
        page += pack("d" if column_index == 2 else "i", value)
      body.append(page)
    else:
      page = [str(mode), " ".join(map(str, shape))]
      if states:
        page.append(" ".join(map(str, states)))
      page.append(str(len(codes)))
      page.extend(" ".join(map(str, row)) for row in zip(*columns))
      body.append(("\n".join(page) + "\n").encode("utf8"))
  data = header.encode("utf8") + b"".join(body)
  if path.suffix == ".gz":
    data = gzip.compress(data)
  elif path.suffix in (".xz", ".lzma"):
    data = lzma.compress(data, format=lzma.FORMAT_XZ if path.suffix == ".xz" else lzma.FORMAT_ALONE)
  path.write_bytes(data)
  return path


def assert_preserved(path):
  assert run("sddscheck", path).stdout.strip() == "ok"
  header = uncompressed(path).split(b"&data", 1)[0]
  assert header.startswith(b"SDDS6\n")
  assert header.count(f'enum_values="{MAPPING}"'.encode()) == 4
  assert stream(path, "-column=state", True) == [str(code) for page in PAGES for code in page]
  assert stream(path, "-column=state") == ["MIN", "OFF", "MAX", "ON", "FAULT"]
  assert stream(path, "-parameter=mode", True) == ["-1", "10", "0"]
  assert stream(path, "-parameter=fixed") == ["FAULT"] * 3
  assert stream(path, "-array=states", True) == [str(code) for page in PAGES for code in page]


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("suffix", [".sdds", ".sdds.gz", ".sdds.xz", ".sdds.lzma"])
@pytest.mark.parametrize("mode", ["ascii", "binary"])
@pytest.mark.parametrize("major_order", ["row", "column"])
def test_enum_format_conversion_preserves_fields(tmp_path, encoding, suffix, mode, major_order):
  source = dataset(tmp_path / f"source{suffix}", encoding)
  output = tmp_path / f"output{suffix}"
  run("sddsconvert", source, output, f"-{mode}", f"-majorOrder={major_order}")
  assert_preserved(output)
  data_header = uncompressed(output).split(b"&data", 1)[1].split(b"&end", 1)[0]
  # The shared writer forces binary output for .xz and .lzma files.
  expected_mode = "binary" if output.suffix in (".xz", ".lzma") else mode
  assert f"mode={expected_mode}".encode() in data_header
  if expected_mode == "binary":
    assert f"column_major_order={int(major_order == 'column')}".encode() in data_header


@pytest.mark.parametrize("endian", ["little", "big"])
@pytest.mark.parametrize("major_order", ["row", "column"])
@pytest.mark.parametrize("suffix", [".sdds", ".sdds.gz", ".sdds.xz", ".sdds.lzma"])
def test_enum_binary_output_uses_requested_byte_order(tmp_path, monkeypatch, endian, major_order, suffix):
  source = dataset(tmp_path / "source.sdds", "big-column" if endian == "little" else "little-row")
  output = tmp_path / f"output{suffix}"
  monkeypatch.setenv("SDDS_OUTPUT_ENDIANESS", endian)
  run("sddsconvert", source, output, "-binary", f"-majorOrder={major_order}")
  assert_preserved(output)
  header, body = uncompressed(output).split(b"&data", 1)[1].split(b"&end\n", 1)
  assert f"endian={endian}".encode() in header
  # Decode the wire data independently, including the materialized fixed parameter.
  offset = 0

  def unpack(fmt):
    nonlocal offset
    values = struct.unpack_from(("<" if endian == "little" else ">") + fmt, body, offset)
    offset += struct.calcsize("=" + fmt)
    return values

  for page_index, codes in enumerate(PAGES):
    assert unpack("iii") == (len(codes), (-1, 10, 0)[page_index], -1)
    assert unpack("ii") == ((1, 3), (2, 1), (0, 2))[page_index]
    assert unpack("i" * len(codes)) == codes
    if major_order == "row":
      for row, code in enumerate(codes, 1):
        assert unpack("iid") == (row, code, 1.5)
    else:
      assert unpack("i" * len(codes)) == tuple(range(1, len(codes) + 1))
      assert unpack("i" * len(codes)) == codes
      assert unpack("d" * len(codes)) == (1.5,) * len(codes)
  assert offset == len(body)


@pytest.mark.parametrize("mode", ["ascii", "binary"])
@pytest.mark.parametrize("suffix", [".sdds", ".sdds.gz", ".sdds.xz", ".sdds.lzma"])
def test_enum_dictionaries_remain_local_to_each_field(tmp_path, mode, suffix):
  source = tmp_path / "source.sdds"
  source.write_text('SDDS6\n'
                    '&parameter name=state, type=enum, enum_values="10:parameter" &end\n'
                    '&parameter name=fixed, type=enum, enum_values="10:fixed", fixed_value=10 &end\n'
                    '&array name=state, type=enum, enum_values="10:array" &end\n'
                    '&column name=state, type=enum, enum_values="10:column" &end\n'
                    '&data mode=ascii &end\n10\n1\n10\n1\n10\n', encoding="utf8")
  output = tmp_path / f"output{suffix}"
  run("sddsconvert", source, output, f"-{mode}",
      "-rename=parameter,state=renamed", "-rename=array,state=renamed", "-rename=column,state=renamed")
  assert run("sddscheck", output).stdout.strip() == "ok"
  for kind in ("parameter", "array", "column"):
    assert stream(output, f"-{kind}=renamed") == [kind]
    assert stream(output, f"-{kind}=renamed", True) == ["10"]
  assert stream(output, "-parameter=fixed") == ["fixed"]


@pytest.mark.parametrize("kind,name", [("column", "state"), ("parameter", "mode"), ("array", "states")])
@pytest.mark.parametrize("method", ["rename", "editnames"])
def test_enum_names_change_without_changing_values(tmp_path, kind, name, method):
  source = dataset(tmp_path / "source.sdds")
  output = tmp_path / "output.sdds"
  option = f"-rename={kind},{name}=newState" if method == "rename" else f"-editnames={kind},{name},%g/{name}/newState/"
  run("sddsconvert", source, output, option)
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, f"-{kind}=newState", True) == stream(source, f"-{kind}={name}", True)
  assert stream(output, f"-{kind}=newState") == stream(source, f"-{kind}={name}")


@pytest.mark.parametrize("kind,name", [("column", "state"), ("parameter", "mode"), ("array", "states")])
@pytest.mark.parametrize("selection", ["delete", "retain"])
def test_enum_field_selection(tmp_path, kind, name, selection):
  source = dataset(tmp_path / "source.sdds")
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, f"-{selection}={kind},{name}")
  assert run("sddscheck", output).stdout.strip() == "ok"
  header = uncompressed(output).split(b"&data", 1)[0]
  assert (f'name="{name}"'.encode() in header) == (selection == "retain")
  if selection == "retain":
    assert stream(output, f"-{kind}={name}", True) == stream(source, f"-{kind}={name}", True)


def test_removing_all_enum_fields_allows_legacy_output(tmp_path):
  source = dataset(tmp_path / "source.sdds")
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, "-ascii", "-delete=column,state",
      "-delete=parameter,*", "-delete=array,*")
  assert output.read_bytes().startswith(b"SDDS1\n")
  assert b"enum_values" not in output.read_bytes()
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert list(map(float, stream(output, "-column=value"))) == [1.5] * 5


@pytest.mark.parametrize("options,pages", [
  (("-fromPage=2",), (1, 2)),
  (("-toPage=1",), (0,)),
  (("-table=2",), (1,)),
  (("-removePages=2",), (0, 2)),
  (("-keepPages=1,3",), (0, 2)),
])
def test_page_selection_preserves_enum_parameters_and_arrays(tmp_path, options, pages):
  source = dataset(tmp_path / "source.sdds", "big-column")
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, "-ascii", *options)
  expected = [str(code) for index in pages for code in PAGES[index]]
  assert stream(output, "-column=state", True) == expected
  assert stream(output, "-array=states", True) == expected
  assert stream(output, "-parameter=mode", True) == [str((-1, 10, 0)[index]) for index in pages]


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("mode", ["ascii", "binary"])
def test_enum_conversion_through_input_and_output_pipes(tmp_path, encoding, mode):
  source = dataset(tmp_path / "source.sdds", encoding)
  result = subprocess.run([str(BIN_DIR / "sddsconvert"), "-pipe=input,output", f"-{mode}"],
                          input=source.read_bytes(), capture_output=True, timeout=30)
  assert result.returncode == 0, result.stderr
  output = tmp_path / "output.sdds"
  output.write_bytes(result.stdout)
  assert_preserved(output)


@pytest.mark.parametrize("kind,name", [("column", "state"), ("parameter", "mode"), ("array", "states")])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("selection", ["none", "delete", "skip-page"])
def test_invalid_enum_data_cannot_be_hidden_by_projection(tmp_path, kind, name, encoding, selection):
  source = dataset(tmp_path / "source.sdds", encoding, invalid_kind=kind)
  original = source.read_bytes()
  options = [f"-delete={kind},{name}"] if selection == "delete" else []
  if selection == "skip-page":
    options.append("-removePages=2")
  result = run("sddsconvert", source, "-ascii", *options, check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
  assert "42" in result.stderr
  assert source.read_bytes() == original


@pytest.mark.parametrize("kind,name", [("column", "state"), ("parameter", "mode"), ("array", "states")])
@pytest.mark.parametrize("factor", [1, 2])
def test_enum_units_conversion_is_rejected(tmp_path, kind, name, factor):
  source = dataset(tmp_path / "source.sdds")
  result = run("sddsconvert", source, tmp_path / "output.sdds",
               f"-convertUnits={kind},{name},m,,{factor}", check=False)
  assert result.returncode > 0
  assert "nonnumeric" in result.stderr


def test_numeric_units_conversion_leaves_enum_codes_unchanged(tmp_path):
  source = dataset(tmp_path / "source.sdds")
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, "-convertUnits=column,value,cm,m,100")
  assert_preserved(output)
  assert list(map(float, stream(output, "-column=value"))) == [150] * 5


@pytest.mark.parametrize("fixture", ["valid-escapes.sdds", "valid-large.sdds", "valid-multiline.sdds",
                                     "valid-fixed.sdds", "valid-limits.sdds"])
@pytest.mark.parametrize("mode", ["ascii", "binary"])
def test_large_and_escaped_enum_dictionaries_survive_conversion(tmp_path, fixture, mode):
  source = FIXTURES / fixture
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, f"-{mode}")
  assert run("sddscheck", output).stdout.strip() == "ok"
  for codes in (False, True):
    options = ["-enumCodes"] if codes else []
    assert run("sdds2stream", output, "-column=state", "-noquotes", *options).stdout == \
      run("sdds2stream", source, "-column=state", "-noquotes", *options).stdout


@pytest.mark.parametrize("kind", ["parameter", "array"])
@pytest.mark.parametrize("mode", ["ascii", "binary"])
def test_escaped_enum_labels_survive_parameter_and_array_conversion(tmp_path, kind, mode):
  source = tmp_path / "source.sdds"
  declaration = (FIXTURES / "valid-escapes.sdds").read_text(encoding="utf8").splitlines()[1]
  declaration = declaration.replace("&column", f"&{kind}")
  source.write_text("SDDS6\n" + declaration + '\n&data mode=ascii &end\n' +
                    ("1\n10\n" if kind == "array" else "10\n"), encoding="utf8")
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, f"-{mode}", f"-rename={kind},state=renamed")
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert run("sdds2stream", source, f"-{kind}=state", "-noquotes").stdout == \
    run("sdds2stream", output, f"-{kind}=renamed", "-noquotes").stdout


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("recovery", ["-recover", "-recover=clip"])
def test_explicit_recovery_keeps_valid_enum_data(tmp_path, kind, encoding, recovery):
  source = dataset(tmp_path / "source.sdds", encoding, invalid_kind=kind)
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, recovery)
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, "-column=state", True) == [str(code) for code in PAGES[0]]
  partial = recovery == "-recover" and kind == "column" and encoding.endswith("-row")
  assert stream(output, "-parameter=mode", True) == (["-1", "10"] if partial else ["-1"])
  expected = PAGES[0] + PAGES[1] if partial else PAGES[0]
  assert stream(output, "-array=states", True) == [str(code) for code in expected]


@pytest.mark.parametrize("encoding", ["little-row", "big-row"])
@pytest.mark.parametrize("suffix", [".sdds", ".sdds.gz", ".sdds.xz", ".sdds.lzma"])
@pytest.mark.parametrize("valid_rows", [0, 1])
def test_recovered_enum_row_prefix_has_native_values(tmp_path, encoding, suffix, valid_rows):
  source = dataset(tmp_path / f"source{suffix}", encoding, invalid_kind="column", invalid_row=valid_rows)
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, "-recover", "-ascii")
  assert run("sddscheck", output).stdout.strip() == "ok"
  expected = PAGES[0] + PAGES[1][:valid_rows]
  assert stream(output, "-column=state", True) == [str(code) for code in expected]
  assert list(map(float, stream(output, "-column=value"))) == [1.5] * len(expected)
  assert stream(output, "-column=id") == ["1", "2", "3"] + ["1"] * valid_rows
  assert stream(output, "-parameter=mode", True) == ["-1", "10"]


@pytest.mark.parametrize("encoding", ["little-row", "big-row"])
@pytest.mark.parametrize("suffix", [".sdds", ".sdds.gz", ".sdds.xz", ".sdds.lzma"])
@pytest.mark.parametrize("options,pages", [
  (("-fromPage=2",), (1,)),
  (("-keepPages=2",), (1,)),
  (("-removePages=2",), (0,)),
  (("-keepPages=1,2",), (0, 1)),
])
def test_recovered_enum_page_uses_original_page_number(tmp_path, encoding, suffix, options, pages):
  source = dataset(tmp_path / f"source{suffix}", encoding, invalid_kind="column", invalid_row=1)
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, "-recover", "-ascii", *options)
  assert run("sddscheck", output).stdout.strip() == "ok"
  codes = (PAGES[0], PAGES[1][:1])
  assert stream(output, "-column=state", True) == [str(code) for index in pages for code in codes[index]]
  assert stream(output, "-parameter=mode", True) == [str((-1, 10)[index]) for index in pages]
  assert stream(output, "-parameter=fixed") == ["FAULT"] * len(pages)
  assert stream(output, "-array=states", True) == [str(code) for index in pages for code in PAGES[index]]


@pytest.mark.parametrize("endian", ["little", "big"])
@pytest.mark.parametrize("valid_rows", [0, 1])
def test_recovered_primitive_row_prefix_has_native_values(tmp_path, endian, valid_rows):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  header = ('SDDS5\n&column name=id, type=long &end\n'
            '&column name=value, type=double &end\n'
            f'&data mode=binary, endian={endian} &end\n').encode()
  pack = lambda fmt, *values: struct.pack(("<" if endian == "little" else ">") + fmt, *values)
  source.write_bytes(header + pack("i", 2) + pack("id", 123, 1.5) * valid_rows)
  run("sddsconvert", source, output, "-recover", "-ascii")
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, "-column=id") == ["123"] * valid_rows
  assert list(map(float, stream(output, "-column=value"))) == [1.5] * valid_rows


@pytest.mark.parametrize("fixture", [
  "invalid-missing.sdds", "invalid-duplicate-code.sdds", "invalid-duplicate-label.sdds",
  "invalid-primitive-mapping.sdds", "invalid-version5.sdds", "invalid-nul.sdds", "invalid-utf8.sdds",
])
@pytest.mark.parametrize("recovery", [False, True])
def test_malformed_enum_headers_are_rejected_before_output_is_opened(tmp_path, fixture, recovery):
  output = tmp_path / "output.sdds"
  original = b"existing destination\n"
  output.write_bytes(original)
  options = ["-recover=clip"] if recovery else []
  result = run("sddsconvert", FIXTURES / fixture, output, "-delete=column,*", *options, check=False)
  assert result.returncode > 0
  assert output.read_bytes() == original


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_long_enum_names_allow_renaming_and_reject_unsafe_editing(tmp_path, kind):
  name = "state" * 400
  source = tmp_path / "source.sdds"
  data = {"column": "1\n10\n", "parameter": "10\n", "array": "1\n10\n"}
  source.write_text(f'SDDS6\n&{kind} name={name}, type=enum, enum_values="{MAPPING}" &end\n'
                    '&data mode=ascii &end\n' + data[kind], encoding="utf8")
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, f"-rename={kind},{name}=shortState")
  assert stream(output, f"-{kind}=shortState") == ["ON"]
  result = run("sddsconvert", source, tmp_path / "edited.sdds",
               f"-editnames={kind},*,a", check=False)
  assert result.returncode > 0
  assert "name too long for -editnames" in result.stderr
  assert "-rename" in result.stderr


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("length", [255, 256])
def test_enum_name_editing_input_boundary(tmp_path, kind, length):
  source = tmp_path / "source.sdds"
  name = "s" * length
  data = {"column": "1\n10\n", "parameter": "10\n", "array": "1\n10\n"}
  source.write_text(f'SDDS6\n&{kind} name={name}, type=enum, enum_values="{MAPPING}" &end\n'
                    '&data mode=ascii &end\n' + data[kind], encoding="utf8")
  original = source.read_bytes()
  result = run("sddsconvert", source, f"-editnames={kind},*,a", check=False)
  if length == 255:
    assert result.returncode == 0, result.stderr
    assert stream(source, f"-{kind}={name}") == ["ON"]
  else:
    assert result.returncode > 0
    assert "name too long for -editnames" in result.stderr
    assert source.read_bytes() == original


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("name,edit,expected", [
  ("s" * 254, "ei/x/", "s" * 254 + "x"),
  ("s" * 255, "ei/x/", None),
  ("s" * 128, "%g/s/ss/", None),
  ("s" * 128, "128k2y", None),
  ("s" * 128, "128ky", "s" * 128),
  ("state", "999ki/newState/", "newState"),
  ("state", "(" * 12 + "ei/x/" + ")" * 12, "statex"),
  ("s" * 240, "16(ei/x/)", None),
  ("state", "i", None),
  ("state", "x", None),
  ("state", "%/", None),
  ("state", "%g/s/" + "s" * 5000 + "/", None),
], ids=["insert-fits", "insert-overflows", "replace-overflows", "yank-overflows",
        "yank-fits", "kill-past-end", "nested-edit", "nested-overflow", "invalid-insert",
        "invalid-kill", "invalid-replacement", "replace-scratch-overflow"])
def test_enum_name_editing_checks_output_capacity(tmp_path, kind, name, edit, expected):
  source = tmp_path / "source.sdds"
  data = {"column": "1\n10\n", "parameter": "10\n", "array": "1\n10\n"}
  source.write_text(f'SDDS6\n&{kind} name={name}, type=enum, enum_values="{MAPPING}" &end\n'
                    '&data mode=ascii &end\n' + data[kind], encoding="utf8")
  original = source.read_bytes()
  result = run("sddsconvert", source, f"-editnames={kind},*,{edit}", check=False)
  if expected is None:
    assert result.returncode > 0, result.stderr
    assert "error editing name" in result.stderr
    assert source.read_bytes() == original
  else:
    assert result.returncode == 0, result.stderr
    assert stream(source, f"-{kind}={expected}") == ["ON"]


@pytest.mark.parametrize("selection", ["-removePages=1", "-keepPages=2"])
@pytest.mark.parametrize("invalid", [False, True])
def test_page_limit_still_applies_when_last_page_is_excluded(tmp_path, selection, invalid):
  source = dataset(tmp_path / "source.sdds", invalid_kind="column" if invalid else None)
  output = tmp_path / "output.sdds"
  run("sddsconvert", source, output, "-toPage=1", selection)
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, "-column=state", True) == []
  assert stream(output, "-parameter=mode", True) == []
