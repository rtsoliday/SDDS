"""Regressions for ENUM page failures and categorical names in SDDSaps."""

import gzip
import json
import lzma
import re
import struct

import pytest

from test_sdds_enum_read_failures import (
  ENCODINGS, enum_file, input_path, naming_file, page_count, run,
)


TOOLS = {
  "sddsanalyticsignal": ("-columns=y",),
  "sddscorrelate": ("-columns=x,y",),
  "sddsbaseline": ("-columns=y", "-select=endpoints=1", "-method=average"),
  "sddshist": ("-dataColumn=y", "-bins=3"),
  "sddssnap2grid": ("-column=x,deltaGuess=1",),
  "sddsshift": ("-columns=y", "-shift=1"),
  "sddsmatrix2column": (),
  "sddsseparate": ("-group=selected,state", "-copy=x,y"),
  "sddszerofind": ("-zeroesOf=y", "-columns=x"),
  "sddschanges": ("-changesIn=y",),
  "sddsbinarystring": ("-columns=x",),
  "sddsenvelope": ("-mean=y", "-copy=state"),
}
COPY_TOOLS = ("sddsanalyticsignal", "sddsbaseline", "sddssnap2grid",
              "sddsshift", "sddsbinarystring")


def values(path, columns, codes=False):
  return run("sdds2stream", path, f"-columns={columns}", "-noquotes",
             *(["-enumCodes"] if codes else [])).stdout.split()


@pytest.mark.parametrize("tool", TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_remaining_tools_reject_enum_page_errors(tmp_path, tool, encoding, kind, prefix):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  enum_file(source, encoding, invalid=kind, valid_pages=int(prefix))
  result = run(tool, source, output, *TOOLS[tool], check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower()
  assert "42" in result.stderr
  if tool == "sddsenvelope" and output.exists():
    assert page_count(output) == 0  # Never publish statistics from a valid prefix.


@pytest.mark.parametrize("tool", TOOLS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("pages", [1, 2])
def test_remaining_tools_accept_normal_enum_eof(tmp_path, tool, encoding, pages):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  enum_file(source, encoding, valid_pages=pages)
  result = run(tool, source, output, *TOOLS[tool])
  assert not result.stderr
  assert run("sddscheck", output).stdout.strip() == "ok"
  expected_pages = {"sddsenvelope": 1, "sddschanges": pages - 1}.get(tool, pages)
  assert page_count(output) == expected_pages
  if tool in (*COPY_TOOLS, "sddshist", "sddsseparate"):
    assert run("sdds2stream", output, "-parameter=mode", "-noquotes").stdout.split() == ["ON"] * pages
  if tool in COPY_TOOLS:
    assert values(output, "state") == ["OFF", "ON", "ON", "FAULT", "OFF"] * pages
    assert values(output, "state", codes=True) == ["0", "10", "10", "-1", "0"] * pages
    assert run("sdds2stream", output, "-arrays=states", "-noquotes").stdout.split() == [
      "OFF", "ON", "FAULT", "ON", "OFF"] * pages
  if tool in ("sddsseparate", "sddsenvelope"):
    name = "selected" if tool == "sddsseparate" else "state"
    assert values(output, name) == ["OFF", "ON", "ON", "FAULT", "OFF"] * expected_pages


@pytest.mark.parametrize("tool", ["sddsmatrix2column", "sddsbinarystring"])
@pytest.mark.parametrize("encoding", ["ascii", "little-row", "big-column", "gzip"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_remaining_in_place_failures_preserve_input(tmp_path, tool, encoding, kind, prefix):
  source = input_path(tmp_path, encoding)
  enum_file(source, encoding, invalid=kind, valid_pages=int(prefix))
  original = source.read_bytes()
  result = run(tool, source, source, *TOOLS[tool], check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "enum" in result.stderr.lower()
  assert source.read_bytes() == original


def named_data(path, value_type, encoding, labels, columns, rows, metadata=False):
  """Serialize categorical names and primitive numeric data independently."""
  codes = [-2147483648, 2147483647, -7][:len(labels)]
  mapping = ', enum_values="' + ','.join(f"{c}:{v}" for c, v in zip(codes, labels)) + '"' if value_type == "enum" else ""
  attributes = ', symbol="Region", units="category", description="Region labels"' if metadata else ""
  name_values = list(labels) if value_type == "string" else codes
  header = ("SDDS6\n" + f'&column name=Name, type={value_type}{mapping}{attributes} &end\n' +
            ''.join(f'&column name={name}, type=double &end\n' for name in columns))
  binary = encoding != "ascii"
  order = ">" if encoding.startswith("big") else "<"
  major = encoding.endswith("column")
  header += (f'&data mode={"binary" if binary else "ascii"}, '
             f'endian={"big" if order == ">" else "little"}, column_major_order={int(major)} &end\n')
  if not binary:
    wire = (header + str(len(labels)) + '\n' + ''.join(
        (json.dumps(value) if value_type == "string" else str(value)) + ' ' +
        ' '.join(map(str, row)) + '\n' for value, row in zip(name_values, rows))).encode()
  else:
    def name_bytes(value):
      if value_type != "string":
        return struct.pack(order + "i", value)
      raw = value.encode()
      return struct.pack(order + "i", len(raw)) + raw
    wire = header.encode() + struct.pack(order + "i", len(labels))
    if major:
      wire += b''.join(name_bytes(v) for v in name_values)
      for index in range(len(columns)):
        wire += struct.pack(order + "d" * len(rows), *(row[index] for row in rows))
    else:
      wire += b''.join(name_bytes(v) + struct.pack(order + "d" * len(columns), *row)
                       for v, row in zip(name_values, rows))
    if encoding == "gzip":
      wire = gzip.compress(wire)
    elif encoding in ("xz", "lzma"):
      wire = lzma.compress(wire, format=lzma.FORMAT_ALONE if encoding == "lzma" else lzma.FORMAT_XZ)
  path.write_bytes(wire)


@pytest.mark.parametrize("kind", ["weights", "correctorWeights"])
@pytest.mark.parametrize("value_type", ["string", "enum"])
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_pseudoinverse_decodes_weight_names(tmp_path, kind, value_type, encoding):
  source, names, output = tmp_path / "matrix.sdds", input_path(tmp_path, encoding), tmp_path / "output.sdds"
  if kind == "weights":
    labels, columns, data = ("BPM1", "BPM2", "BPM3"), ("A", "B"), ((1, 0), (0, 1), (1, 1))
    weight_labels = ("BPM3", "BPM1", "BPM2")
    expected = [13/49, -36/49, 36/49, -9/49, 40/49, 9/49]
  else:
    labels, columns, data = ("BPM1", "BPM2"), ("A", "B", "C"), ((1, 0, 1), (0, 1, 1))
    weight_labels = ("C", "A", "B")
    expected = [13/49, -9/49, -36/49, 40/49, 36/49, 9/49]
  named_data(source, "string", "ascii", labels, columns, data)
  named_data(names, value_type, encoding, weight_labels, ("Weight",), ((3,), (1,), (2,)))
  result = run("sddspseudoinverse", source, output, f"-{kind}={names},name=Name,value=Weight")
  assert not result.stderr
  assert list(map(float, values(output, ','.join(labels)))) == pytest.approx(expected)


@pytest.mark.parametrize("kind", ["weights", "correctorWeights"])
@pytest.mark.parametrize("value_type", ["long", "double", "character"])
def test_pseudoinverse_rejects_numeric_weight_names(tmp_path, kind, value_type):
  source, names, output = tmp_path / "matrix.sdds", tmp_path / "names.sdds", tmp_path / "out.sdds"
  naming_file(source, "string")
  names.write_text(f'SDDS1\n&column name=Name, type={value_type} &end\n'
                   '&column name=Weight, type=double &end\n&data mode=ascii &end\n2\n7 1\n8 2\n')
  result = run("sddspseudoinverse", source, output, f"-{kind}={names},name=Name,value=Weight", check=False)
  assert result.returncode > 0
  assert "string or enum" in result.stderr


@pytest.mark.parametrize("value_type", ["string", "enum"])
@pytest.mark.parametrize("encoding", ["ascii", "big-column"])
@pytest.mark.parametrize("other_string", [False, True])
def test_pseudoinverse_honors_categorical_names_and_reconstruction(tmp_path, value_type, encoding, other_string):
  source, output = tmp_path / "matrix.sdds", tmp_path / "out.sdds"
  reconstructed, u_matrix = tmp_path / "reconstructed.sdds", tmp_path / "u.sdds"
  naming_file(source, value_type, encoding, other_string)
  run("sddspseudoinverse", source, output, "-newColumnNames=state",
      f"-reconstruct={reconstructed}", f"-uMatrix={u_matrix}")
  assert run("sddsquery", output, "-columnlist").stdout.split() == ["OldColumnNames", "OFF", "ON"]
  assert list(map(float, values(output, "OFF,ON"))) == pytest.approx([-2, 1, 1.5, -0.5])
  assert values(u_matrix, "state") == ["OFF", "ON"]
  assert values(reconstructed, "state") == ["OFF", "ON"]
  assert list(map(float, values(reconstructed, "A,B"))) == pytest.approx([1, 2, 3, 4])
  if value_type == "enum":
    assert values(reconstructed, "state", codes=True) == ["-2147483648", "2147483647"]
    assert reconstructed.read_bytes().startswith(b"SDDS6\n")
  if other_string:
    assert values(reconstructed, "other") == ["unused", "unused"]


@pytest.mark.parametrize("name", ["missing", "A"])
@pytest.mark.parametrize("other_string", [False, True])
def test_pseudoinverse_rejects_invalid_explicit_names(tmp_path, name, other_string):
  source, output = tmp_path / "matrix.sdds", tmp_path / "out.sdds"
  naming_file(source, "enum", other_string=other_string)
  result = run("sddspseudoinverse", source, output, f"-newColumnNames={name}", check=False)
  assert result.returncode > 0
  assert "string or enum" in result.stderr
  assert not output.exists()


@pytest.mark.parametrize("value_type", ["string", "enum"])
def test_pseudoinverse_rejects_duplicate_names(tmp_path, value_type):
  source = tmp_path / "matrix.sdds"
  naming_file(source, value_type, duplicate=True)
  result = run("sddspseudoinverse", source, tmp_path / "out.sdds", "-newColumnNames=state", check=False)
  assert result.returncode > 0
  assert "already exists" in result.stderr.lower() or "duplicate" in result.stderr.lower()


@pytest.mark.parametrize("kind", ["weights", "correctorWeights"])
@pytest.mark.parametrize("field", ["column", "parameter", "array"])
def test_pseudoinverse_rejects_bad_auxiliary_enum_pages(tmp_path, kind, field):
  source, names = tmp_path / "matrix.sdds", tmp_path / "names.sdds"
  naming_file(source, "string")
  enum_file(names, "ascii", invalid=field)
  result = run("sddspseudoinverse", source, tmp_path / "out.sdds",
               f"-{kind}={names},name=state,value=y", check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower()


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("field", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [False, True])
def test_pseudoinverse_rejects_bad_primary_enum_pages(tmp_path, encoding, field, prefix):
  source, output = input_path(tmp_path, encoding), tmp_path / "out.sdds"
  enum_file(source, encoding, invalid=field, valid_pages=int(prefix))
  result = run("sddspseudoinverse", source, output, check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower()


@pytest.mark.parametrize("field", ["column", "parameter", "array"])
def test_pseudoinverse_preserves_failed_in_place_input(tmp_path, field):
  source = tmp_path / "input.sdds"
  enum_file(source, "ascii", invalid=field, valid_pages=1)
  original = source.read_bytes()
  result = run("sddspseudoinverse", source, check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
  assert source.read_bytes() == original


@pytest.mark.parametrize("value_type", ["string", "enum"])
def test_pseudoinverse_reconstructs_categorical_names_across_pages(tmp_path, value_type):
  source, output, reconstructed = (tmp_path / name for name in ("input.sdds", "out.sdds", "reconstructed.sdds"))
  naming_file(source, value_type, other_string=True)
  wire = source.read_bytes()
  source.write_bytes(wire + wire.split(b"&data mode=ascii &end\n", 1)[1])
  run("sddspseudoinverse", source, output, "-newColumnNames=state", f"-reconstruct={reconstructed}")
  assert page_count(output) == page_count(reconstructed) == 2
  assert values(reconstructed, "state") == ["OFF", "ON"] * 2
  assert values(reconstructed, "other") == ["unused"] * 4
  if value_type == "enum":
    assert values(reconstructed, "state", codes=True) == ["-2147483648", "2147483647"] * 2


def test_pseudoinverse_keeps_multiply_matrix_string_names(tmp_path):
  source, other, output = (tmp_path / name for name in ("matrix.sdds", "other.sdds", "out.sdds"))
  named_data(source, "enum", "ascii", ("OFF", "ON"), ("A", "B"), ((1, 0), (0, 1)))
  named_data(other, "string", "ascii", ("Act1", "Act2"), ("A", "B"), ((1, 0), (0, 1)))
  other.write_text(other.read_text().replace("name=Name,", "name=OtherName,"))
  result = run("sddspseudoinverse", source, output, "-newColumnNames=Name", f"-multiplyMatrix={other},invert", check=False)
  if "multiplymatrix is not implemented" in result.stdout.lower():
    pytest.skip("multiplyMatrix requires a LAPACK-based build")
  assert result.returncode == 0, result.stderr
  assert values(output, "Name") == ["Act1", "Act2"]
  assert list(map(float, values(output, "OFF,ON"))) == pytest.approx([1, 0, 0, 1])


@pytest.mark.parametrize("value_type", ["string", "enum"])
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("empty", [False, True])
def test_histogram_projects_region_labels_to_strings(tmp_path, value_type, encoding, empty):
  source, output = tmp_path / "input.sdds", tmp_path / "out.sdds"
  regions = tmp_path / ("regions-" + input_path(tmp_path, encoding).name)
  source.write_text('SDDS1\n&column name=y, type=double &end\n&data mode=ascii &end\n' +
                    ('0\n' if empty else '5\n-1\n0\n1\n2\n3\n'))
  named_data(regions, value_type, encoding, ("Low", "Middle"), ("Position",), ((0,), (2,)), metadata=True)
  result = run("sddshist", source, output, "-dataColumn=y",
               f"-regions=filename={regions},position=Position,name=Name")
  assert not result.stderr
  assert values(output, "Name") == ["Low", "Middle", "Beyond"]
  assert list(map(float, values(output, "frequency"))) == ([0, 0, 0] if empty else [1, 2, 2])
  assert run("sddscheck", output).stdout.strip() == "ok"
  definition = next(line for line in output.read_bytes().split(b"&data", 1)[0].decode().splitlines()
                    if re.search(r'&column name="?Name"?[, ]', line))
  assert "type=string" in definition
  for metadata in ("Region", "category", "Region labels"):
    assert metadata in definition


@pytest.mark.parametrize("name", ["Name", "missing"])
def test_histogram_rejects_non_categorical_region_names(tmp_path, name):
  source, regions = tmp_path / "input.sdds", tmp_path / "regions.sdds"
  source.write_text('SDDS1\n&column name=y, type=double &end\n&data mode=ascii &end\n1\n1\n')
  named_data(regions, "long", "ascii", ("Low", "High"), ("Position",), ((0,), (2,)))
  result = run("sddshist", source, tmp_path / "out.sdds", "-dataColumn=y",
               f"-regions=filename={regions},position=Position,name={name}", check=False)
  assert result.returncode > 0
  assert "string or enum" in result.stderr


@pytest.mark.parametrize("field", ["column", "parameter", "array"])
def test_histogram_rejects_bad_auxiliary_enum_pages(tmp_path, field):
  source, regions = tmp_path / "input.sdds", tmp_path / "regions.sdds"
  source.write_text('SDDS1\n&column name=y, type=double &end\n&data mode=ascii &end\n1\n1\n')
  enum_file(regions, "ascii", invalid=field)
  result = run("sddshist", source, tmp_path / "out.sdds", "-dataColumn=y",
               f"-regions=filename={regions},position=x,name=state", check=False)
  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
