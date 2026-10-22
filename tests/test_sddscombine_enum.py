"""Regression coverage for ENUM append, merge, metadata, and recovery."""

import struct
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


MAPPING = "0:OFF,10:ON,-1:FAULT,2147483647:MAX,-2147483648:MIN"
ENCODINGS = ("ascii", "little-row", "little-column", "big-row", "big-column")
pytestmark = pytest.mark.skipif(
  not all((BIN_DIR / tool).exists() for tool in ("sddscombine", "sddscheck", "sdds2stream")),
  reason="tools not built",
)


def run(tool, *args, check=True):
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)], capture_output=True,
                          text=True, encoding="utf8", timeout=30)
  if check:
    assert result.returncode == 0, result.stderr
  return result


def codes(path, selection):
  return run("sdds2stream", path, selection, "-enumCodes", "-noquotes").stdout.split()


def dataset(path, encoding="ascii", pages=((10, (10, -1)),), fixed=None,
            name="state", mapping=MAPPING, invalid_kind=None):
  """Encode independent wire fixtures, including appendable ASCII row counts."""
  binary = encoding != "ascii"
  endian = ">" if encoding.startswith("big") else "<"
  fixed_value = f", fixed_value={fixed}" if fixed is not None else ""
  header = ("SDDS6\n"
            f'&parameter name={name}, type=enum, enum_values="{mapping}"{fixed_value} &end\n'
            f'&array name=states, type=enum, enum_values="{MAPPING}" &end\n'
            '&column name=x, type=long &end\n'
            f'&column name=rowState, type=enum, enum_values="{MAPPING}" &end\n' +
            (f'&data mode=binary, endian={"big" if endian == ">" else "little"}, '
             f'column_major_order={int(encoding.endswith("column"))} &end\n' if binary else
             '&data mode=ascii &end\n'))
  body = []
  for page_index, (key, values) in enumerate(pages):
    array, column = list(values), list(values)
    if page_index == 1:
      if invalid_kind == "parameter":
        key = 42
      elif invalid_kind == "array":
        array[0] = 42
      elif invalid_kind == "column":
        column[1] = 42
    ids = list(range(1, len(values) + 1))
    if binary:
      pack = lambda *items: struct.pack(endian + "i" * len(items), *items)
      page = pack(len(values)) + (pack(key) if fixed is None else b"")
      page += pack(len(array), *array)
      if encoding.endswith("column"):
        page += pack(*ids, *column)
      else:
        page += pack(*(item for row in zip(ids, column) for item in row))
      body.append(page)
    else:
      page = ([str(key)] if fixed is None else []) + [str(len(array))]
      if array:
        page.append(" ".join(map(str, array)))
      page.append(f"{len(values):20d}")
      page.extend(f"{row} {code}" for row, code in zip(ids, column))
      body.append(("\n".join(page) + "\n").encode())
  path.write_bytes(header.encode() + b"".join(body))
  return path


@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-row"])
@pytest.mark.parametrize("merge", [False, True])
@pytest.mark.parametrize("source_fixed", [None, -1])
def test_append_rejects_conflicting_fixed_enum(tmp_path, encoding, merge, source_fixed):
  first = dataset(tmp_path / "first.sdds", encoding, fixed=10)
  second = dataset(tmp_path / "second.sdds", pages=((-1, (10,)),), fixed=source_fixed)
  original = first.read_bytes()
  result = run("sddscombine", first, second, "-append", *(["-merge"] if merge else []), check=False)
  assert result.returncode > 0
  assert "fixed enum parameter state" in result.stderr
  assert first.read_bytes() == original


@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-row"])
@pytest.mark.parametrize("merge", [False, True])
def test_append_accepts_matching_fixed_enum(tmp_path, encoding, merge):
  first = dataset(tmp_path / "first.sdds", encoding, fixed=10)
  second = dataset(tmp_path / "second.sdds", pages=((10, (-1,)),))
  run("sddscombine", first, second, "-append", *(["-merge"] if merge else []))
  assert run("sddscheck", first).stdout.strip() == "ok"
  assert codes(first, "-parameter=state") == (["10"] if merge else ["10", "10"])
  assert codes(first, "-column=rowState") == ["10", "-1", "-1"]


@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-row"])
@pytest.mark.parametrize("first_key", [10, -1])
def test_append_merge_checks_destination_key_and_writes_groups(tmp_path, encoding, first_key):
  first = dataset(tmp_path / "first.sdds", encoding, pages=((0, (0,)), (10, (10,))))
  second = dataset(tmp_path / "second.sdds", pages=((first_key, (-1,)), (-1, (10,)),
                                                            (2147483647, (0,))))
  third = dataset(tmp_path / "third.sdds", pages=((2147483647, (-1,)), (-2147483648, (10,))))
  run("sddscombine", first, second, third, "-append", "-merge=state")
  assert run("sddscheck", first).stdout.strip() == "ok"
  assert codes(first, "-parameter=state") == ["0", "10", "-1", "2147483647", "-2147483648"]
  expected_rows = [1, 2, 1, 2, 1] if first_key == 10 else [1, 1, 2, 2, 1]
  for page, count in enumerate(expected_rows, 1):
    values = run("sdds2stream", first, "-column=rowState", "-enumCodes", "-noquotes",
                 f"-page={page}").stdout.split()
    assert len(values) == count
  assert codes(first, "-column=rowState") == ["0", "10", "-1", "10", "0", "-1", "10"]


@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-row"])
def test_append_merge_rejects_destination_mapping_conflict(tmp_path, encoding):
  first = dataset(tmp_path / "first.sdds", encoding)
  second = dataset(tmp_path / "second.sdds", mapping=MAPPING.replace("10:ON", "10:OTHER"))
  original = first.read_bytes()
  result = run("sddscombine", first, second, "-append", "-merge=state", check=False)
  assert result.returncode > 0
  assert "enum mapping" in result.stderr.lower()
  assert first.read_bytes() == original


@pytest.mark.parametrize("name", ["Filename", "NumberCombined"])
@pytest.mark.parametrize("options", [(), ("-threads=2",), ("-collapse",), ("-merge",)])
def test_generated_metadata_rejects_enum_fields(tmp_path, name, options):
  if name == "Filename" and options == ("-merge",):
    pytest.skip("merge does not assign Filename metadata")
  mapping = MAPPING + ",2:TWO"
  first = dataset(tmp_path / "first.sdds", name=name, mapping=mapping)
  second = dataset(tmp_path / "second.sdds", name=name, mapping=mapping)
  output = tmp_path / "out.sdds"
  result = run("sddscombine", first, second, output, *options, check=False)
  assert result.returncode > 0
  assert f"Reserved output {'column' if '-collapse' in options else 'parameter'} {name}" in result.stderr
  assert "enum" in result.stderr


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("recovery", ["-recover", "-recover=clip"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_recovery_clears_handled_enum_errors_before_next_file(tmp_path, encoding, recovery, kind):
  pages = ((10, (10, -1)), (-1, (0, 10)))
  first = dataset(tmp_path / "first.sdds", encoding, pages=pages, invalid_kind=kind)
  second = dataset(tmp_path / "second.sdds", encoding, pages=pages)
  output = tmp_path / "out.sdds"
  run("sddscombine", first, second, output, recovery)
  assert run("sddscheck", output).stdout.strip() == "ok"
  partial = recovery == "-recover" and kind == "column" and encoding.endswith("-row")
  expected = ["10", "-1"] + (["0"] if partial else []) + ["10", "-1", "0", "10"]
  assert codes(output, "-column=rowState") == expected


def change_field(path, kind, name, replacement):
  declaration = f'&{kind} name={name}, type=enum, enum_values="{MAPPING}"'
  path.write_text(path.read_text().replace(declaration,
                                          f'&{kind} name={name}, {replacement}'), encoding="utf8")


@pytest.mark.parametrize("kind,name", [("column", "rowState"), ("parameter", "state"),
                                       ("array", "states")])
@pytest.mark.parametrize("options", [(), ("-threads=2",), ("-merge",), ("-merge=2",),
                                    ("-merge=state",), ("-collapse",),
                                    ("-collapse", "-merge"), ("-collapse", "-merge=state")])
@pytest.mark.parametrize("replacement", [f'type=enum, enum_values="{MAPPING.replace("10:ON", "10:OTHER")}"',
                                         "type=long"])
def test_incompatible_enum_fields_fail_before_writing(tmp_path, kind, name, options, replacement):
  if "-collapse" in options and kind != "parameter":
    pytest.skip("collapse only copies parameters")
  first = dataset(tmp_path / "first.sdds")
  second = dataset(tmp_path / "second.sdds")
  change_field(second, kind, name, replacement)
  output = tmp_path / "out.sdds"
  result = run("sddscombine", first, second, output, *options, check=False)
  assert result.returncode > 0
  assert "enum mapping" in result.stderr.lower()
  assert output.read_bytes() == b""


@pytest.mark.parametrize("kind,name", [("column", "rowState"), ("parameter", "state"),
                                       ("array", "states")])
@pytest.mark.parametrize("options", [(), ("-threads=2",), ("-merge",), ("-merge=state",),
                                    ("-collapse",)])
def test_equivalent_enum_mappings_preserved_and_excluded_conflicts_allowed(tmp_path, kind, name, options):
  first = dataset(tmp_path / "first.sdds")
  second = dataset(tmp_path / "second.sdds")
  reordered = ",".join(reversed(MAPPING.split(",")))
  change_field(second, kind, name, f'type=enum, enum_values="{reordered}"')
  output = tmp_path / "out.sdds"
  run("sddscombine", first, second, output, *options)
  assert run("sddscheck", output).stdout.strip() == "ok"
  if "-collapse" not in options or kind == "parameter":
    output_kind = "column" if "-collapse" in options else kind
    assert f'enum_values="{MAPPING}"'.encode() in output.read_bytes().split(b"&data", 1)[0]
    assert codes(output, f"-{output_kind}={name}")

  change_field(first, kind, name, f'type=enum, enum_values="{MAPPING.replace("10:ON", "10:OTHER")}"')
  # An excluded merge key still determines grouping and must remain compatible.
  if kind == "parameter" and "-merge=state" in options:
    return
  run("sddscombine", first, second, output, *options, f"-delete={kind},{name}", "-overWrite")
  assert run("sddscheck", output).stdout.strip() == "ok"


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("statistic", [None, "average", "median", "minimum", "maximum"])
@pytest.mark.parametrize("threads", [1, 2])
def test_sparse_enum_codes_remain_categorical(tmp_path, encoding, statistic, threads):
  values = (2147483647, -2147483648, 10, -1, 0)
  first = dataset(tmp_path / "first.sdds", encoding, pages=((10, values),))
  second = dataset(tmp_path / "second.sdds", encoding, pages=((-1, values),))
  output = tmp_path / "out.sdds"
  option = "-sparse=2" + (f",{statistic}" if statistic else "")
  run("sddscombine", first, second, output, option, f"-threads={threads}")
  assert run("sddscheck", output).stdout.strip() == "ok"
  # ENUM follows the same selected rows as the integer column; statistics
  # apply only to floating-point data, never to categorical codes.
  selected_rows = codes(output, "-column=x")
  assert codes(output, "-column=rowState") == [str(values[int(row) - 1]) for row in selected_rows]
  assert len(selected_rows) in (4, 6)
  assert codes(output, "-array=states") == list(map(str, values)) * 2


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("collapse", [False, True])
@pytest.mark.parametrize("drop_key", [False, True])
def test_enum_merge_key_groups_across_files_at_int32_limits(tmp_path, encoding, collapse, drop_key):
  first_pages = ((2147483647, (10,)), (-2147483648, (-1,)), (-2147483648, (0,)))
  second_pages = ((-2147483648, (10,)), (-1, (-1,)), (0, (0,)))
  first = dataset(tmp_path / "first.sdds", encoding, pages=first_pages)
  second = dataset(tmp_path / "second.sdds", encoding, pages=second_pages)
  output = tmp_path / "out.sdds"
  options = ["-merge=state"] + (["-collapse"] if collapse else [])
  if drop_key:
    options.append("-delete=parameter,state")
  run("sddscombine", first, second, output, *options)
  assert run("sddscheck", output).stdout.strip() == "ok"
  if not drop_key:
    assert codes(output, "-column=state" if collapse else "-parameter=state") == [
      "2147483647", "-2147483648", "-1", "0"]
  if collapse:
    assert len(codes(output, "-column=PageNumber")) == 4
  else:
    assert codes(output, "-column=rowState") == ["10", "-1", "0", "10", "-1", "0"]
    assert [len(run("sdds2stream", output, "-column=rowState", "-enumCodes", "-noquotes",
                    f"-page={page}").stdout.split()) for page in range(1, 5)] == [1, 3, 1, 1]


@pytest.mark.parametrize("collapse", [False, True])
def test_excluded_enum_merge_key_still_checks_mapping(tmp_path, collapse):
  first = dataset(tmp_path / "first.sdds")
  second = dataset(tmp_path / "second.sdds", mapping=MAPPING.replace("10:ON", "10:OTHER"))
  result = run("sddscombine", first, second, tmp_path / "out.sdds", "-merge=state",
               "-delete=parameter,state", *(["-collapse"] if collapse else []), check=False)
  assert result.returncode > 0
  assert "enum mappings for -merge parameter" in result.stderr


@pytest.mark.parametrize("mapping", [MAPPING, MAPPING.replace("0:OFF,", "")])
@pytest.mark.parametrize("missing_first", [False, True])
def test_collapse_missing_enum_parameter_requires_mapped_zero(tmp_path, mapping, missing_first):
  first = dataset(tmp_path / "first.sdds", name="other" if missing_first else "state", mapping=mapping)
  second = dataset(tmp_path / "second.sdds", name="state" if missing_first else "other", mapping=mapping)
  output = tmp_path / "out.sdds"
  result = run("sddscombine", first, second, output, "-collapse", check=False)
  if "0:OFF" not in mapping:
    assert result.returncode > 0
    assert "requires a mapped enum code 0" in result.stderr
  else:
    assert result.returncode == 0, result.stderr
    assert run("sddscheck", output).stdout.strip() == "ok"
    assert codes(output, "-column=state") == (["0", "10"] if missing_first else ["10", "0"])


@pytest.mark.parametrize("kind,name", [("column", "rowState"), ("parameter", "state"),
                                       ("array", "states")])
def test_empty_input_cannot_hide_conflicting_enum_mapping(tmp_path, kind, name):
  first = dataset(tmp_path / "first.sdds")
  second = dataset(tmp_path / "empty.sdds", pages=())
  change_field(second, kind, name, f'type=enum, enum_values="{MAPPING.replace("10:ON", "10:OTHER")}"')
  output = tmp_path / "out.sdds"
  result = run("sddscombine", first, second, output, "-merge", check=False)
  assert result.returncode > 0
  assert "enum mapping" in result.stderr.lower()
  assert output.read_bytes() == b""
