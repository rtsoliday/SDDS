"""SDDS6 enum preservation, label presentation, and numeric-operation safeguards."""
import struct
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


TOOLS = ("sddsprocess", "sddsconvert", "sddscast", "sdds2stream", "sddscheck", "sddsquery")
pytestmark = pytest.mark.skipif(
  not all((BIN_DIR / tool).exists() for tool in TOOLS), reason="tools not built"
)
MAPPING = "10:ON,-1:FAULT,2147483647:MAX,-2147483648:MIN"


def run(tool, *args, check=True):
  result = subprocess.run(
    [str(BIN_DIR / tool), *map(str, args)], capture_output=True, text=True,
    encoding="utf8", timeout=30,
  )
  if check:
    assert result.returncode == 0, result.stderr
  return result


def enum_input(path):
  path.write_text(
    "SDDS6\n"
    f'&parameter name=mode, type=enum, enum_values="{MAPPING}" &end\n'
    f'&parameter name=fixed, type=enum, enum_values="{MAPPING}", fixed_value=10 &end\n'
    '&parameter name=expression, type=string &end\n'
    f'&array name=states, type=enum, enum_values="{MAPPING}", dimensions=1 &end\n'
    f'&column name=state, type=enum, enum_values="{MAPPING}" &end\n'
    '&column name=x, type=double &end\n'
    '&column name=expr, type=string &end\n'
    '&data mode=ascii &end\n'
    '-1\n"2 3 +"\n3\n-1 2147483647 -2147483648\n'
    '3\n-1 1 "x 2 *"\n2147483647 2 "x 2 *"\n-2147483648 3 "x 2 *"\n'
    '10\n"3 4 +"\n2\n10 -1\n'
    '2\n10 4 "x 2 *"\n-1 5 "x 2 *"\n', encoding="utf8",
  )
  return path


def stream(path, selection, codes=False):
  options = [selection, "-noquotes"]
  if codes:
    options.append("-enumCodes")
  return run("sdds2stream", path, *options).stdout.split()


@pytest.mark.parametrize("storage", ["ascii", "row", "column"])
@pytest.mark.parametrize("suffix", [".sdds", ".sdds.gz", ".sdds.xz", ".sdds.lzma"])
def test_enum_fields_survive_processing_and_deletion(tmp_path, storage, suffix):
  original = enum_input(tmp_path / "original.sdds")
  source, output = tmp_path / f"input{suffix}", tmp_path / f"output{suffix}"
  options = ["-ascii"] if storage == "ascii" else ["-binary", f"-majorOrder={storage}"]
  run("sddsconvert", original, source, *options)
  run("sddsprocess", source, output, "-delete=column,expr",
      "-delete=parameter,expression",
      "-define=column,twice,x 2 *", "-process=x,average,mean")
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, "-column=state", True) == [
    "-1", "2147483647", "-2147483648", "10", "-1"]
  assert stream(output, "-column=state") == ["FAULT", "MAX", "MIN", "ON", "FAULT"]
  assert stream(output, "-parameter=mode", True) == ["-1", "10"]
  assert stream(output, "-parameter=fixed", True) == ["10", "10"]
  assert stream(output, "-array=states", True) == [
    "-1", "2147483647", "-2147483648", "10", "-1"]
  assert list(map(float, stream(output, "-column=twice"))) == [2, 4, 6, 8, 10]
  assert list(map(float, stream(output, "-parameter=mean"))) == [2, 4.5]
  ascii_output = tmp_path / "ascii.sdds"
  run("sddsconvert", output, ascii_output, "-ascii")
  text = ascii_output.read_text(encoding="utf8")
  assert text.startswith("SDDS6\n")
  assert text.count(f'enum_values="{MAPPING}"') == 4


@pytest.mark.parametrize("storage", ["row", "column"])
def test_enum_processing_accepts_opposite_byte_order(tmp_path, storage, monkeypatch):
  original = enum_input(tmp_path / "original.sdds")
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  monkeypatch.setenv("SDDS_OUTPUT_ENDIANESS", "big")
  run("sddsconvert", original, source, "-binary", f"-majorOrder={storage}")
  assert b"endian=big" in source.read_bytes()
  monkeypatch.setenv("SDDS_OUTPUT_ENDIANESS", "little")
  run("sddsprocess", source, output, "-define=column,twice,x 2 *")
  assert b"endian=little" in output.read_bytes()
  assert stream(output, "-column=state", True) == [
    "-1", "2147483647", "-2147483648", "10", "-1"]
  assert stream(output, "-parameter=mode") == ["FAULT", "ON"]
  assert stream(output, "-parameter=fixed") == ["ON", "ON"]
  assert stream(output, "-array=states") == ["FAULT", "MAX", "MIN", "ON", "FAULT"]
  assert list(map(float, stream(output, "-column=twice"))) == [2, 4, 6, 8, 10]


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_prefix", [False, True])
def test_enum_page_read_errors_are_reported(tmp_path, kind, encoding, valid_prefix):
  source, output = tmp_path / "invalid.sdds", tmp_path / "output.sdds"
  header = ('SDDS6\n'
            f'&{kind} name=state, type=enum, enum_values="0:OFF,1:ON" &end\n'
            '&column name=x, type=long &end\n'
            f'&data mode={encoding}, endian=little &end\n')
  pages = []
  for code in ([1] if valid_prefix else []) + [42]:
    if encoding == "binary":
      values = [1] + ([1] if kind == "array" else []) + [code, 5]
      pages.append(struct.pack("<" + "i" * len(values), *values))
    elif kind == "parameter":
      pages.append(f"{code}\n1\n5\n".encode())
    elif kind == "array":
      pages.append(f"1\n{code}\n1\n5\n".encode())
    else:
      pages.append(f"1\n{code} 5\n".encode())
  source.write_bytes(header.encode() + b"".join(pages))
  result = run("sddsprocess", source, output, check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
  assert "42" in result.stderr


def test_enum_print_uses_labels_from_columns_and_parameters(tmp_path):
  source, output = enum_input(tmp_path / "input.sdds"), tmp_path / "output.sdds"
  run("sddsprocess", source, output, "-match=column,state=FAULT,state=ON,|",
      "-print=column,label,state:%s/%s/%.0f,state,mode,x",
      "-print=parameter,label,mode:%s,fixed",
      "-reprint=column,expr,%s,state", "-reprint=parameter,expression,%s,mode", "-verbose")
  assert stream(output, "-column=label") == [
    "state:FAULT/FAULT/1", "state:ON/ON/4", "state:FAULT/ON/5"]
  assert stream(output, "-parameter=label") == ["mode:ON", "mode:ON"]
  assert stream(output, "-column=expr") == ["FAULT", "ON", "FAULT"]
  assert stream(output, "-parameter=expression") == ["FAULT", "ON"]


@pytest.mark.parametrize("kind,source", [("column", "state"), ("parameter", "mode")])
@pytest.mark.parametrize("format_string,error", [
  ("%d", "Enum display format must be a string format"),
  ("%100000s", "Enum display buffer too small"),
])
def test_enum_print_rejects_invalid_or_oversized_formats(tmp_path, kind, source, format_string, error):
  source_file = enum_input(tmp_path / "input.sdds")
  result = run("sddsprocess", source_file, tmp_path / "output.sdds",
               f"-print={kind},label,{format_string},{source}", check=False)
  assert result.returncode > 0
  assert error in result.stderr


@pytest.mark.parametrize("kind,source", [("column", "expr"), ("parameter", "expression")])
@pytest.mark.parametrize("operation", ["define", "evaluate"])
def test_numeric_expressions_cannot_create_enum_outputs(tmp_path, kind, source, operation):
  source_file = enum_input(tmp_path / "input.sdds")
  expression = "10" if operation == "define" else source
  result = run("sddsprocess", source_file, tmp_path / "output.sdds",
               f"-{operation}={kind},result,{expression},type=enum,enum_values=0:OFF",
               check=False)
  assert result.returncode > 0
  assert "non-numeric" in result.stderr


@pytest.mark.parametrize("option", [
  "-test=parameter,1 1 ==", "-rpnExpression=1 1 + pop",
])
def test_rpn_enum_namespace_errors_are_propagated(tmp_path, option):
  source = enum_input(tmp_path / "input.sdds")
  definitions = tmp_path / "definitions.rpn"
  definitions.write_text("1 sto mode\n", encoding="ascii")
  result = run("sddsprocess", source, tmp_path / "output.sdds",
               f"-rpnDefinitionsFiles={definitions}", option, check=False)
  assert result.returncode > 0
  assert "numeric memory with this name" in result.stderr


def test_verbose_prints_enum_parameter_labels_after_deletion(tmp_path):
  source = enum_input(tmp_path / "input.sdds")
  result = run("sddsprocess", source, tmp_path / "output.sdds",
               "-delete=parameter,expression", "-verbose")
  assert "FAULT" in result.stderr and "ON" in result.stderr


@pytest.mark.parametrize("label", ["caf\u00e9,go:run", "line\nnext", "a\\b\"c", "1", "01"])
def test_enum_print_preserves_literal_labels(tmp_path, label):
  escaped = label.replace("\\", "\\\\").replace(",", "\\,").replace(":", "\\:")
  escaped = escaped.replace('"', '\\"').replace("\n", "\\n")
  mapping = ("10:" + escaped).replace("\\", "\\\\").replace('"', '\\"')
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  source.write_text(
    'SDDS6\n'
    f'&parameter name=mode, type=enum, enum_values="{mapping}" &end\n'
    f'&column name=state, type=enum, enum_values="{mapping}" &end\n'
    '&data mode=ascii &end\n10\n1\n10\n', encoding="utf8",
  )
  run("sddsprocess", source, output, "-print=column,label,%s,state",
      "-print=parameter,label,%s,mode")
  for kind in ("column", "parameter"):
    assert run("sdds2stream", output, f"-{kind}=label", "-noquotes").stdout == label + "\n"


@pytest.mark.parametrize("kind,source", [("column", "state"), ("parameter", "mode")])
def test_enum_print_bounds_long_labels(tmp_path, kind, source):
  source_file = enum_input(tmp_path / "input.sdds")
  source_file.write_text(source_file.read_text().replace("FAULT", "a" * 2000), encoding="utf8")
  result = run("sddsprocess", source_file, tmp_path / "output.sdds",
               f"-print={kind},label,%s,{source}", check=False)
  assert result.returncode > 0
  assert "Enum display buffer too small" in result.stderr


@pytest.mark.parametrize("kind,first,second,format_string", [
  ("column", "state", "x", "%s%200f"),
  ("parameter", "mode", "expression", "%s%200s"),
])
def test_enum_print_bounds_mixed_output(tmp_path, kind, first, second, format_string):
  source = enum_input(tmp_path / "source.sdds")
  source.write_text(source.read_text().replace("FAULT", "a" * 1000), encoding="utf8")
  result = run("sddsprocess", source, tmp_path / "output.sdds",
               f"-print={kind},label,{format_string},{first},{second}", check=False)
  assert result.returncode > 0
  assert "Print output buffer too small" in result.stderr


@pytest.mark.parametrize("option", [
  "-process=state,average,result", "-filter=column,state,-10,20",
  "-filter=parameter,mode,-10,20", "-redefine=column,state,10",
  "-redefine=parameter,mode,10", "-cast=column,codes,state,long",
  "-cast=parameter,codes,mode,long", "-process=x,minimum,result,functionOf=state,position",
])
def test_numeric_operations_reject_selected_enums(tmp_path, option):
  source = enum_input(tmp_path / "input.sdds")
  result = run("sddsprocess", source, tmp_path / "output.sdds", option, check=False)
  assert result.returncode > 0


def test_enum_codes_require_explicit_conversion_before_arithmetic(tmp_path):
  source, codes, output = [tmp_path / name for name in ("source.sdds", "codes.sdds", "output.sdds")]
  enum_input(source)
  run("sddscast", source, codes, "-cast=column,state,enum,long")
  run("sddsprocess", codes, output, "-define=column,result,state 1 +")
  assert list(map(float, stream(output, "-column=result"))) == [0, 2147483648, -2147483647, 11, 0]
  assert stream(output, "-parameter=mode") == ["FAULT", "ON"]


@pytest.mark.parametrize("kind,field", [("column", "state"), ("parameter", "mode")])
def test_enum_metadata_can_be_printed(tmp_path, kind, field):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  run("sddsprocess", source, output,
      f"-print={kind},metadata,%s|%s,{field}.type,{field}.enum_values")
  expected = [f"enum|{MAPPING}"] * (5 if kind == "column" else 2)
  assert stream(output, f"-{kind}=metadata") == expected


@pytest.mark.parametrize("selection,expected", [
  ('-test=column,state "FAULT" streq', ["FAULT", "FAULT"]),
  ('-test=parameter,mode "ON" streq', ["ON", "FAULT"]),
])
def test_rpn_tests_compare_enum_labels(tmp_path, selection, expected):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  run("sddsprocess", source, output, selection)
  assert stream(output, "-column=state") == expected


def test_enum_print_column_and_parameter_can_share_a_name(tmp_path):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  text = source.read_text().replace(MAPPING, MAPPING + ",0:OFF")
  text = text.replace('&data mode=ascii &end',
                      f'&parameter name=state, type=enum, enum_values="{MAPPING},0:OFF", fixed_value=10 &end\n'
                      '&data mode=ascii &end').replace('-1 1 "x 2 *"', '0 1 "x 2 *"')
  source.write_text(text, encoding="utf8")
  run("sddsprocess", source, output, "-print=column,label,%s,state")
  assert stream(output, "-column=label") == ["OFF", "MAX", "MIN", "ON", "FAULT"]


@pytest.mark.parametrize("kind,first,second,expected", [
  ("column", "state", "mode", ["FAULT%/FAULT", "MAX%/FAULT", "MIN%/FAULT", "ON%/ON", "FAULT%/ON"]),
  ("parameter", "mode", "fixed", ["FAULT%/ON", "ON%/ON"]),
])
def test_enum_print_allows_literal_percent_between_sources(tmp_path, kind, first, second, expected):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  run("sddsprocess", source, output, f"-print={kind},label,%s%%/%s,{first},{second}")
  assert stream(output, f"-{kind}=label") == expected


@pytest.mark.parametrize("notation", ["@", "@@"])
def test_expression_parameter_uses_output_layout_after_enum_deletion(tmp_path, notation):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  if notation == "@@":
    source.write_text(source.read_text().replace('"2 3 +"', '"2+3"').replace('"3 4 +"', '"3+4"'),
                      encoding="utf8")
  run("sddsprocess", source, output, "-delete=parameter,mode,fixed",
      f"-define=column,result,{notation}expression")
  assert list(map(float, stream(output, "-column=result"))) == [5, 5, 5, 7, 7]


@pytest.mark.parametrize("notation", ["@", "@@"])
def test_enum_parameter_cannot_be_used_as_expression_after_deletion(tmp_path, notation):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  source.write_text(
    'SDDS6\n'
    '&parameter name=discard, type=string &end\n'
    '&parameter name=mode, type=enum, enum_values="10:ON" &end\n'
    '&column name=x, type=double &end\n'
    '&data mode=ascii &end\n"2 3 +"\n10\n1\n5\n', encoding="utf8",
  )
  result = run("sddsprocess", source, output, "-delete=parameter,discard",
               f"-define=column,result,{notation}mode", check=False)
  assert result.returncode > 0
  assert "not string type" in result.stderr


@pytest.mark.parametrize("rows", [0, 1])
@pytest.mark.parametrize("kind", ["column", "parameter"])
def test_enum_unit_conversion_is_rejected_even_on_empty_pages(tmp_path, rows, kind):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  source.write_text(
    'SDDS6\n'
    '&parameter name=mode, type=enum, units=old, enum_values="10:ON" &end\n'
    '&column name=state, type=enum, units=old, enum_values="10:ON" &end\n'
    '&data mode=ascii &end\n10\n' + str(rows) + '\n' + '10\n' * rows,
    encoding="utf8",
  )
  field = "state" if kind == "column" else "mode"
  result = run("sddsprocess", source, output, f"-convertUnits={kind},{field},new,old,2", check=False)
  assert result.returncode > 0
  assert "non-numeric" in result.stderr
