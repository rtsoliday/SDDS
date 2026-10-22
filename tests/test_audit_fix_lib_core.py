"""Regressions for the 2026-10-22 SDDS6/enum audit fixes in the core C library."""
import shutil
import subprocess

import pytest

from sdds_test_utils import BIN_DIR, ROOT_DIR


MAPPING = "0:OFF,10:ON,-1:FAULT"


def run(tool, *args, check=True):
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)], capture_output=True,
                          text=True, encoding="utf8")
  if check:
    assert result.returncode == 0, result.stderr
  return result


def write(path, text):
  path.write_text(text, encoding="utf8", newline="\n")
  return path


def stream(path, *args):
  return run("sdds2stream", path, *args).stdout.split("\n")[:-1]


def numbers(tmp_path):
  return write(tmp_path / "num.sdds", "SDDS1\n&column name=x, type=double &end\n&data mode=ascii &end\n2\n1\n2\n")


def enum_numbers(tmp_path):
  return write(tmp_path / "enum.sdds",
               "SDDS6\n&column name=x, type=double &end\n"
               f"&column name=state, type=enum, enum_values=\"{MAPPING}\" &end\n"
               "&data mode=ascii &end\n2\n1 0\n2 10\n")


def header_field(path, column, field):
  query = run("sddsquery", path, "-sddsOutput=" + str(path) + ".query", "-column")
  assert query.returncode == 0
  names = stream(str(path) + ".query", "-column=Name")
  values = stream(str(path) + ".query", f"-column={field}", "-noquotes")
  return values[names.index(column)]


@pytest.mark.parametrize("make_input,version", [(numbers, "SDDS1"), (enum_numbers, "SDDS6")])
def test_define_strings_keep_literal_backslashes(tmp_path, make_input, version):
  source, output = make_input(tmp_path), tmp_path / "out.sdds"
  # scanargs turns "\\" into "\" before the library parses the definition.
  run("sddsprocess", source, output, r"-define=column,y1,x 2 *,symbol=y\_1",
      r"-define=column,y2,x 3 *,description=C:\\temp\\new ! &end,units=m\\s")
  header = output.read_text(encoding="utf8").split("&data")[0]
  assert header.startswith(version + "\n")
  assert "\t" not in header and "ew" not in header.split("C:")[1].split("\n")[0][:3]
  assert header_field(output, "y1", "Symbol") == r"y\_1"
  assert header_field(output, "y2", "Description") == r"C:\temp\new ! &end"
  assert header_field(output, "y2", "Units") == r"m\s"
  assert stream(output, "-column=y1,y2") == ["2.000000000000000e+00 3.000000000000000e+00",
                                             "4.000000000000000e+00 6.000000000000000e+00"]


def test_string_and_character_units_are_relabelled(tmp_path):
  source = write(tmp_path / "units.sdds",
                 "SDDS1\n&parameter name=P, type=string, units=old &end\n"
                 "&column name=x, type=double, units=m &end\n"
                 "&column name=label, type=string, units=old &end\n"
                 "&column name=ch, type=character, units=old &end\n"
                 "&data mode=ascii &end\nhello\n2\n1 a q\n2 b r\n")
  output = tmp_path / "out.sdds"
  run("sddsconvert", source, output, "-convertUnits=column,label,new", "-convertUnits=column,ch,new2",
      "-convertUnits=parameter,P,newp", "-convertUnits=column,x,mm,m,1000")
  assert header_field(output, "label", "Units") == "new"
  assert header_field(output, "ch", "Units") == "new2"
  assert stream(output, "-column=x,label,ch") == ["1.000000000000000e+03 a q", "2.000000000000000e+03 b r"]
  assert stream(output, "-parameter=P") == ["hello"]


def test_enum_units_conversion_is_still_rejected(tmp_path):
  result = run("sddsconvert", enum_numbers(tmp_path), tmp_path / "out.sdds",
               "-convertUnits=column,state,new", check=False)
  assert result.returncode != 0
  assert "enum" in result.stderr.lower()


def test_legacy_multi_conversion_string_formats_remain_readable(tmp_path):
  source = write(tmp_path / "fmt.sdds",
                 "SDDS1\n&column name=a, type=string, format_string=\"%s (%s)\" &end\n"
                 "&parameter name=p, type=string, format_string=\"%-10s|%s\" &end\n"
                 "&data mode=ascii &end\nhello\n1\nx\n")
  run("sddsquery", source)
  assert stream(source, "-column=a", "-ignoreFormats") == ["x"]
  assert stream(source, "-parameter=p", "-ignoreFormats") == ["hello"]
  # Formatting a string with an unsafe format is an error instead of undefined behaviour.
  result = run("sdds2stream", source, "-column=a", check=False)
  assert result.returncode != 0 and "string conversion" in result.stderr
  converted = tmp_path / "copy.sdds"
  run("sddsconvert", source, converted)
  assert header_field(converted, "a", "Format") == "%s (%s)"


def fixed_enum_file(path, label, keep_enum=False):
  logical = (label.replace("\\", "\\\\").replace(",", "\\,").replace(":", "\\:")
             .replace('"', '\\"').replace("\t", "\\t"))
  outer = logical.replace("\\", "\\\\").replace('"', '\\"').replace("!", "\\!")
  other = (f"&parameter name=other, type=enum, enum_values=\"{MAPPING}\", fixed_value=\"10\" &end\n"
           if keep_enum else "")
  return write(path, f"SDDS6\n&parameter name=fixed, type=enum, enum_values=\"0:{outer}\", "
                     f"fixed_value=\"0\" &end\n{other}&data mode=ascii &end\n\n")


@pytest.mark.parametrize("keep_enum", [False, True])
@pytest.mark.parametrize("label", [
  ' a "q" \\back\\ !bang &end\ttab \u00b5 01 ',
  "two words ending in a backslash\\",
  "E" * 900,
  "F" * 969,
])
def test_fixed_enum_label_casts_to_readable_string_header(tmp_path, label, keep_enum):
  source = fixed_enum_file(tmp_path / "fixed.sdds", label, keep_enum)
  output = tmp_path / "string.sdds"
  run("sddscast", source, output, "-cast=parameter,fixed,enum,string")
  header = output.read_bytes().split(b"\n")
  assert header[0] == (b"SDDS6" if keep_enum else b"SDDS1")
  if not keep_enum:
    assert all(len(line) < 1024 for line in header)
  assert run("sdds2stream", output, "-parameter=fixed", "-noquotes").stdout == label + "\n"


def test_fixed_enum_label_too_long_for_legacy_header_fails_cleanly(tmp_path):
  source = fixed_enum_file(tmp_path / "fixed.sdds", "G" * 970)
  result = run("sddscast", source, tmp_path / "string.sdds", "-cast=parameter,fixed,enum,string",
               check=False)
  assert result.returncode != 0
  assert "too long" in result.stderr


def test_sdds6_comment_inside_command_separates_tokens(tmp_path):
  source = write(tmp_path / "comment.sdds",
                 "SDDS6\n&column name=y! a comment\ntype=double &end\n"
                 "&data mode=ascii &end\n1\n2.5\n")
  assert header_field(source, "y", "Type") == "double"
  assert stream(source, "-column=y") == ["2.500000000000000e+00"]


@pytest.mark.parametrize("token", ['" +10"', '"+10"', "+10"])
@pytest.mark.parametrize("kind", ["column", "parameter"])
def test_ascii_enum_codes_reject_plus_signs(tmp_path, token, kind):
  if kind == "column":
    text = (f"SDDS6\n&column name=state, type=enum, enum_values=\"{MAPPING}\" &end\n"
            f"&data mode=ascii &end\n1\n{token}\n")
  else:
    text = (f"SDDS6\n&parameter name=state, type=enum, enum_values=\"{MAPPING}\" &end\n"
            f"&data mode=ascii &end\n{token}\n")
  source = write(tmp_path / "plus.sdds", text)
  result = run("sdds2stream", source, f"-{kind}=state", check=False)
  assert result.returncode != 0
  space = write(tmp_path / "space.sdds", text.replace(token, '" 10"' if kind == "column" else " 10"))
  assert stream(space, f"-{kind}=state") == ["ON"]


def test_enum_labels_are_quoted_like_strings_and_codes_never(tmp_path):
  source = tmp_path / "states.sdds"
  run("sddsmakedataset", source, "-column=state,type=enum", "-data=0,10,-1,20",
      f"-enumValues=column,state,{MAPPING},20:two words",
      "-column=text,type=string", "-data=OFF,ON,FAULT,two words")
  assert stream(source, "-column=state") == ["OFF", "ON", "FAULT", '"two words"']
  assert stream(source, "-column=state") == stream(source, "-column=text")
  assert stream(source, "-column=state", "-enumCodes") == ["0", "10", "-1", "20"]
  assert stream(source, "-column=state", "-noquotes") == ["OFF", "ON", "FAULT", "two words"]
  printout = run("sddsprintout", source, "-column=state", "-column=text").stdout
  assert '"' not in printout and "two words" in printout


def test_array_to_column_keeps_field_length_in_sdds6(tmp_path):
  source = write(tmp_path / "array.sdds",
                 "SDDS6\n&array name=a, type=double, field_length=7 &end\n"
                 f"&parameter name=state, type=enum, enum_values=\"{MAPPING}\", fixed_value=10 &end\n"
                 "&data mode=ascii &end\n2\n1.5 2.5\n0\n")
  output = tmp_path / "columns.sdds"
  run("sddsarray2column", source, output, "-convert=a")
  assert "field_length=7" in output.read_text(encoding="utf8").split("&data")[0].split("&column")[1]


def test_unscaled_64_bit_integers_print_exactly(tmp_path):
  source = tmp_path / "big.sdds"
  run("sddsmakedataset", source, "-column=u,type=ulong64", "-data=18446744073709551615,9007199254740993",
      "-column=l,type=long64", "-data=-9223372036854775808,9223372036854775807")
  assert stream(source, "-column=u,l") == ["18446744073709551615 -9223372036854775808",
                                          "9007199254740993 9223372036854775807"]
  printout = run("sddsprintout", source, "-column=u", "-column=l").stdout
  assert "18446744073709551615" in printout and "9223372036854775807" in printout


def test_blank_sdds6_format_string_is_no_format(tmp_path):
  source = write(tmp_path / "blank.sdds",
                 "SDDS6\n&column name=x, type=double, format_string=\"\" &end\n"
                 f"&column name=state, type=enum, enum_values=\"{MAPPING}\", format_string=\"\" &end\n"
                 "&data mode=ascii &end\n1\n1.5 10\n")
  run("sddscheck", source)
  assert stream(source, "-column=x,state") == ["1.500000000000000e+00 ON"]
  output = tmp_path / "copy.sdds"
  run("sddsprocess", source, output, "-define=column,y,x 2 *")
  assert "format_string" not in output.read_text(encoding="utf8")
  assert stream(output, "-column=y") == ["3.000000000000000e+00"]


@pytest.mark.skipif(not shutil.which("gfortran"), reason="gfortran not available")
def test_fortran_enum_module_works_with_sddsfortran_header(tmp_path):
  include = ROOT_DIR / "include"
  run_fortran = lambda *args: subprocess.run(["gfortran", "-fsyntax-only", *map(str, args)],
                                             capture_output=True, text=True, cwd=tmp_path)
  result = run_fortran("-J", tmp_path, include / "SDDSenum.f90")
  assert result.returncode == 0, result.stderr
  together = write(tmp_path / "together.f",
                   "      program together\n      use SDDS_enum_interfaces\n      implicit none\n"
                   "      include 'SDDSFortran.h'\n      print *, SDDS_ENUM, SDDS_NUM_TYPES\n      end\n")
  result = run_fortran("-I", tmp_path, "-I", include, together)
  assert result.returncode == 0, result.stderr
  alone = write(tmp_path / "alone.f90",
                "program alone\n  use SDDS_enum_interfaces\n  use SDDS_enum_constants\n  implicit none\n"
                "  print *, SDDS_ENUM, SDDS_NUM_TYPES, SDDS_ANY_NUMERIC_TYPE\nend program alone\n")
  result = run_fortran("-I", tmp_path, alone)
  assert result.returncode == 0, result.stderr
