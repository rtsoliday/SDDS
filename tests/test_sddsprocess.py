import math
import struct
import subprocess
from pathlib import Path

import pytest

from sdds_test_utils import BIN_DIR


SDDSPROCESS = BIN_DIR / "sddsprocess"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"

REQUIRED_TOOLS = [SDDSPROCESS, SDDSCHECK, SDDS2STREAM, SDDSQUERY]

requires_process_tools = pytest.mark.skipif(
  not all(tool.exists() for tool in REQUIRED_TOOLS),
  reason="tools not built",
)


def run_process(input_file, output_file, *options, capture_output=False):
  return subprocess.run(
    [str(SDDSPROCESS), str(input_file), str(output_file), *options],
    capture_output=capture_output,
    text=True,
    check=True,
  )


def check_sdds(path):
  result = subprocess.run([str(SDDSCHECK), str(path)], capture_output=True, text=True, check=True)
  assert result.stdout.strip() == "ok"


def stream_lines(path, *, columns=None, parameters=None, page=None, delimiter="|"):
  args = [str(SDDS2STREAM), str(path)]
  if page is not None:
    args.append(f"-page={page}")
  if columns:
    args.append(f"-columns={columns}")
    args.append(f"-delimiter={delimiter}")
  if parameters:
    args.append(f"-parameters={parameters}")
    args.append(f"-delimiter={delimiter}")
  result = subprocess.run(args, capture_output=True, text=True, check=True)
  return result.stdout.strip().splitlines() if result.stdout.strip() else []


def stream_column(path, column):
  return [float(line) for line in stream_lines(path, columns=column)]


def stream_column_strings(path, column):
  return stream_lines(path, columns=column)


def stream_parameter(path, parameter, *, page=None):
  lines = stream_lines(path, parameters=parameter, page=page)
  assert len(lines) == 1
  return lines[0].rstrip("|")


def stream_parameter_float(path, parameter, *, page=None):
  return float(stream_parameter(path, parameter, page=page))


def query_list(path, option):
  result = subprocess.run(
    [str(SDDSQUERY), str(path), option], capture_output=True, text=True, check=True
  )
  return result.stdout.strip().splitlines() if result.stdout.strip() else []


def write_rich_input(path):
  path.write_text(
    """SDDS1
&description text="Base", contents="sddsprocess tests", &end
&parameter name=p, type=double, units=punit, &end
&parameter name=limitLow, type=double, &end
&parameter name=limitHigh, type=double, &end
&parameter name=tag, type=string, &end
&parameter name=exprParam, type=string, &end
&parameter name=numParam, type=string, &end
&column name=x, type=double, units=m, &end
&column name=y, type=double, units=m, &end
&column name=weight, type=double, &end
&column name=label, type=string, &end
&column name=numText, type=string, &end
&column name=expr, type=string, &end
&column name=command, type=string, &end
&column name=time, type=double, &end
&data mode=ascii, &end
10
2
4
keep
"p 3 *"
123
5
1 3 1 alpha 1 "x 2 *" "echo sys1" -2000000000
2 5 1 beta bad "x y +" "echo sys2" 0
3 7 2 gamma 3 "y 1 -" "echo sys3" 1000000000
4 9 2 beta 4x "x y *" "echo sys4" 2000000000
5 11 4 epsilon 5 "x 1 +" "echo sys5" 3000000000
""",
    encoding="ascii",
  )
  return path


def write_multipage_input(path):
  path.write_text(
    """SDDS1
&parameter name=p, type=double, &end
&parameter name=tag, type=string, &end
&parameter name=numParam, type=string, &end
&column name=x, type=double, &end
&data mode=ascii, &end
1
keep
123
2
1
2
9
drop
bad
2
3
4
""",
    encoding="ascii",
  )
  return path


@requires_process_tools
def test_sddsprocess(tmp_path):
  output = tmp_path / "process.sdds"
  run_process(
    "SDDSlib/demo/example.sdds",
    output,
    "-define=column,Index,i_row,type=long",
  )
  check_sdds(output)


@requires_process_tools
def test_sddsprocess_redefine_array_access_uses_current_page(tmp_path):
  input_file = tmp_path / "two-page-input.sdds"
  input_file.write_text(
    """SDDS1
&column name=Input, type=double, &end
&data mode=ascii, &end
3
1
2
3
3
10
20
30
""",
    encoding="ascii",
  )

  output = tmp_path / "process-array-access.sdds"
  run_process(
    input_file,
    output,
    "-define=column,A,Input,type=double",
    "-define=column,B,1 &A [,type=double",
  )

  check_sdds(output)

  page1 = stream_lines(output, page=1, columns="Input,A,B")
  page2 = stream_lines(output, page=2, columns="Input,A,B")

  assert page1 == [
    "1.000000000000000e+00|1.000000000000000e+00|2.000000000000000e+00",
    "2.000000000000000e+00|2.000000000000000e+00|2.000000000000000e+00",
    "3.000000000000000e+00|3.000000000000000e+00|2.000000000000000e+00",
  ]
  assert page2 == [
    "1.000000000000000e+01|1.000000000000000e+01|2.000000000000000e+01",
    "2.000000000000000e+01|2.000000000000000e+01|2.000000000000000e+01",
    "3.000000000000000e+01|3.000000000000000e+01|2.000000000000000e+01",
  ]


@requires_process_tools
@pytest.mark.parametrize(
  ("option", "expected"),
  [
    ("-filter=column,x,2,4", [2, 3, 4]),
    ("-match=column,label=a*,label=epsilon,|", [1, 5]),
    ("-numberTest=column,numText,strict", [1, 3, 5]),
    ("-test=column,x 3 >", [4, 5]),
    ("-timeFilter=column,time,after=1990/1/1@0:0:0,before=2040/1/1@0:0:0", [3, 4]),
    ("-clip=1,1", [2, 3, 4]),
    ("-clip=1,1,invert", [1, 5]),
    ("-fclip=0.2,0.2", [2, 3, 4]),
    ("-sparse=2,1", [2, 4]),
    ("-sample=1", [1, 2, 3, 4, 5]),
  ],
)
def test_row_winnowing_options(tmp_path, option, expected):
  input_file = write_rich_input(tmp_path / "input.sdds")
  output = tmp_path / "winnowed.sdds"

  run_process(input_file, output, option)

  check_sdds(output)
  assert stream_column(output, "x") == pytest.approx(expected)


@requires_process_tools
@pytest.mark.parametrize(
  "option",
  [
    "-filter=parameter,p,0,5",
    "-match=parameter,tag=keep",
    "-numberTest=parameter,numParam,strict",
    "-test=parameter,p 5 <",
  ],
)
def test_parameter_winnowing_options(tmp_path, option):
  input_file = write_multipage_input(tmp_path / "pages.sdds")
  output = tmp_path / "page-filtered.sdds"

  run_process(input_file, output, option)

  check_sdds(output)
  assert stream_column(output, "x") == pytest.approx([1, 2])
  assert stream_parameter_float(output, "p") == pytest.approx(1)


@requires_process_tools
def test_numeric_definition_evaluation_conversion_and_casting(tmp_path):
  input_file = write_rich_input(tmp_path / "input.sdds")
  rpn_defs = tmp_path / "defs.rpn"
  rpn_defs.write_text("4 sto four\n", encoding="ascii")
  output = tmp_path / "numeric.sdds"

  run_process(
    input_file,
    output,
    f"-rpnDefinitionsFiles={rpn_defs}",
    "-rpnExpression=2 sto two",
    "-define=column,twiceX,x two *,type=double",
    "-define=column,plusFour,x four +,type=double",
    "-define=column,alg,x+y,type=double,algebraic",
    "-evaluate=column,evaluated,expr,type=double",
    "-evaluate=parameter,pFromExpr,exprParam,type=double",
    "-redefine=column,y,y 1 +,type=double",
    "-cast=column,yLong,y,long",
    "-convertUnits=column,x,cm,m,100",
  )

  check_sdds(output)
  assert stream_column(output, "x") == pytest.approx([100, 200, 300, 400, 500])
  assert stream_column(output, "twiceX") == pytest.approx([2, 4, 6, 8, 10])
  assert stream_column(output, "plusFour") == pytest.approx([5, 6, 7, 8, 9])
  assert stream_column(output, "alg") == pytest.approx([4, 7, 10, 13, 16])
  assert stream_column(output, "evaluated") == pytest.approx([2, 7, 6, 36, 6])
  assert stream_column(output, "y") == pytest.approx([4, 6, 8, 10, 12])
  assert stream_column(output, "yLong") == pytest.approx([4, 6, 8, 10, 12])
  assert stream_parameter_float(output, "pFromExpr") == pytest.approx(30)
  assert b"units=cm" in output.read_bytes()


@requires_process_tools
def test_evaluate_column_after_row_filter_uses_selected_rows(tmp_path):
  input_file = write_rich_input(tmp_path / "input.sdds")
  output = tmp_path / "filtered-evaluate.sdds"

  run_process(
    input_file,
    output,
    "-filter=column,x,2,4",
    "-evaluate=column,evaluated,expr,type=double",
  )

  check_sdds(output)
  assert stream_column(output, "x") == pytest.approx([2, 3, 4])
  assert stream_column(output, "evaluated") == pytest.approx([7, 6, 36])


@requires_process_tools
def test_string_scan_edit_print_format_reprint_and_system(tmp_path):
  input_file = write_rich_input(tmp_path / "input.sdds")
  output = tmp_path / "strings.sdds"

  run_process(
    input_file,
    output,
    "-numberTest=column,numText,strict",
    "-scan=column,scanned,numText,%lf,type=double",
    "-edit=column,edited,label,i/pre-/",
    "-reedit=column,edited,ei/-post/",
    "-print=column,printed,%s=%.0f,label,x",
    "-reprint=column,printed,re:%s,printed",
    "-format=column,formatted,numText,longFormat=N:%ld,stringFormat=S:%s",
    "-system=column,systemOut,command",
  )

  check_sdds(output)
  assert stream_column(output, "x") == pytest.approx([1, 3, 5])
  assert stream_column(output, "scanned") == pytest.approx([1, 3, 5])
  assert stream_column_strings(output, "edited") == [
    "pre-alpha-post",
    "pre-gamma-post",
    "pre-epsilon-post",
  ]
  assert stream_column_strings(output, "printed") == [
    "re:alpha=1",
    "re:gamma=3",
    "re:epsilon=5",
  ]
  assert [value.strip() for value in stream_column_strings(output, "formatted")] == [
    "N:1",
    "N:3",
    "N:5",
  ]
  assert stream_column_strings(output, "systemOut") == ["sys1", "sys3", "sys5"]


@requires_process_tools
def test_process_basic_statistics_and_string_results(tmp_path):
  input_file = write_rich_input(tmp_path / "input.sdds")
  output = tmp_path / "process-basic.sdds"

  run_process(
    input_file,
    output,
    "-process=x,average,avgX",
    "-process=x,rms,rmsX",
    "-process=x,sum,sumX",
    "-process=x,minimum,minX",
    "-process=x,maximum,maxX",
    "-process=x,spread,spreadX",
    "-process=x,first,firstX",
    "-process=x,last,lastX",
    "-process=x,count,countX",
    "-process=x,median,medianX",
    "-process=x,product,productX",
    "-process=label,first,firstLabel",
    "-process=label,last,lastLabel",
    "-threads=2",
  )

  check_sdds(output)
  assert stream_parameter_float(output, "avgX") == pytest.approx(3)
  assert stream_parameter_float(output, "rmsX") == pytest.approx(math.sqrt(11))
  assert stream_parameter_float(output, "sumX") == pytest.approx(15)
  assert stream_parameter_float(output, "minX") == pytest.approx(1)
  assert stream_parameter_float(output, "maxX") == pytest.approx(5)
  assert stream_parameter_float(output, "spreadX") == pytest.approx(4)
  assert stream_parameter_float(output, "firstX") == pytest.approx(1)
  assert stream_parameter_float(output, "lastX") == pytest.approx(5)
  assert stream_parameter_float(output, "countX") == pytest.approx(5)
  assert stream_parameter_float(output, "medianX") == pytest.approx(3)
  assert stream_parameter_float(output, "productX") == pytest.approx(120)
  assert stream_parameter(output, "firstLabel") == "alpha"
  assert stream_parameter(output, "lastLabel") == "epsilon"


@requires_process_tools
def test_process_function_of_modes_and_qualifiers(tmp_path):
  input_file = write_rich_input(tmp_path / "input.sdds")
  output = tmp_path / "process-qualifiers.sdds"

  run_process(
    input_file,
    output,
    "-process=y,slope,slopeY,functionOf=x",
    "-process=y,intercept,interceptY,functionOf=x",
    "-process=y,correlation,corrY,functionOf=x",
    "-process=y,integral,integralY,functionOf=x",
    "-process=x,average,weightedAvg,weightBy=weight",
    "-process=x,average,limitedAvg,functionOf=x,lowerLimit=2,upperLimit=4",
    "-process=x,sum,scaledSum,offset=1,factor=2",
    "-process=x,average,matchedAvg,match=label,value=beta",
    "-process=x,maximum,maxPosition,functionOf=y,position",
    "-process=x,percentile,p50,percentLevel=50",
    "-process=x,average,headAvg,head=3",
    "-process=x,average,tailAvg,tail=2",
    "-process=x,average,fheadAvg,fhead=0.6",
    "-process=x,average,ftailAvg,ftail=0.4",
  )

  check_sdds(output)
  assert stream_parameter_float(output, "slopeY") == pytest.approx(2)
  assert stream_parameter_float(output, "interceptY") == pytest.approx(1)
  assert stream_parameter_float(output, "corrY") == pytest.approx(1)
  assert stream_parameter_float(output, "integralY") == pytest.approx(28)
  assert stream_parameter_float(output, "weightedAvg") == pytest.approx(3.7)
  assert stream_parameter_float(output, "limitedAvg") == pytest.approx(3)
  assert stream_parameter_float(output, "scaledSum") == pytest.approx(40)
  assert stream_parameter_float(output, "matchedAvg") == pytest.approx(3)
  assert stream_parameter_float(output, "maxPosition") == pytest.approx(11)
  assert stream_parameter_float(output, "p50") == pytest.approx(3)
  assert stream_parameter_float(output, "headAvg") == pytest.approx(2)
  assert stream_parameter_float(output, "tailAvg") == pytest.approx(4.5)
  assert stream_parameter_float(output, "fheadAvg") == pytest.approx(2)
  assert stream_parameter_float(output, "ftailAvg") == pytest.approx(4.5)


@requires_process_tools
def test_delete_retain_ifis_ifnot_and_description_controls(tmp_path):
  input_file = write_rich_input(tmp_path / "input.sdds")
  output = tmp_path / "schema.sdds"

  run_process(
    input_file,
    output,
    "-ifis=column,x",
    "-ifnot=column,missingColumn",
    "-retain=column,x,label",
    "-delete=parameter,limit*",
    "-description=text=Updated description,contents=Updated contents",
  )

  check_sdds(output)
  assert query_list(output, "-columnList") == ["x", "label"]
  parameters = query_list(output, "-parameterList")
  assert "p" in parameters
  assert "limitLow" not in parameters
  assert "limitHigh" not in parameters
  header = output.read_bytes()
  assert b"Updated description" in header
  assert b"Updated contents" in header

  skipped_output = tmp_path / "skipped.sdds"
  run_process(input_file, skipped_output, "-ifnot=column,x")
  assert not skipped_output.exists()


@requires_process_tools
def test_array_delete_major_order_xz_and_pipe_controls(tmp_path):
  array_output = tmp_path / "no-arrays.sdds"
  run_process("SDDSlib/demo/example.sdds", array_output, "-delete=array,*Array")
  check_sdds(array_output)
  assert query_list(array_output, "-arrayList") == []

  major_output = tmp_path / "major.sdds"
  run_process("SDDSlib/demo/example.sdds", major_output, "-majorOrder=column")
  check_sdds(major_output)
  assert stream_column(major_output, "shortCol") == pytest.approx([1, 2, 3, 4, 5, 6, 7, 8])

  compressed_output = tmp_path / "compressed.sdds.xz"
  run_process("SDDSlib/demo/example.sdds", compressed_output, "-xzLevel=1")
  check_sdds(compressed_output)

  pipe_result = subprocess.run(
    [
      str(SDDSPROCESS),
      "SDDSlib/demo/example.sdds",
      "-pipe=out",
      "-define=column,Index,i_row,type=long",
    ],
    stdout=subprocess.PIPE,
    check=True,
  )
  assert pipe_result.stdout.startswith(b"SDDS")


@requires_process_tools
def test_summarize_verbose_and_nowarnings(tmp_path):
  input_file = write_rich_input(tmp_path / "input.sdds")
  output = tmp_path / "diagnostics.sdds"

  result = run_process(
    input_file,
    output,
    "-define=column,z,x,type=double",
    "-summarize",
    "-verbose",
    "-nowarnings",
    capture_output=True,
  )

  check_sdds(output)
  assert "column z created from equation" in result.stderr
  assert "page number 1 read in" in result.stderr


# SDDS6 enum preservation, label presentation, and numeric-operation safeguards.

TOOLS = ("sddsprocess", "sddsconvert", "sddscast", "sdds2stream", "sddscheck", "sddsquery")
requires_enum_tools = pytest.mark.skipif(
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


@requires_enum_tools
@pytest.mark.parametrize("storage", ["ascii", "row", "column"])
@pytest.mark.parametrize("suffix", [".sdds", ".sdds.gz", ".sdds.xz", ".sdds.lzma"])
def test_enum_fields_survive_processing_and_deletion(tmp_path, storage, suffix):
  original = enum_input(tmp_path / "original.sdds")
  source, output = tmp_path / f"input{suffix}", tmp_path / f"output{suffix}"
  options = ["-ascii"] if storage == "ascii" else ["-binary", f"-majorOrder={storage}"]
  run("sddsconvert", original, source, *options)
  run("sddsprocess", source, output, "-delete=column,expr",
      "-delete=parameter,expression",
      "-define=column,twice,x 2 *", "-process=x,average,xMean",
      "-define=column,centered,x xMean -")
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert stream(output, "-column=state", True) == [
    "-1", "2147483647", "-2147483648", "10", "-1"]
  assert stream(output, "-column=state") == ["FAULT", "MAX", "MIN", "ON", "FAULT"]
  assert stream(output, "-parameter=mode", True) == ["-1", "10"]
  assert stream(output, "-parameter=fixed", True) == ["10", "10"]
  assert stream(output, "-array=states", True) == [
    "-1", "2147483647", "-2147483648", "10", "-1"]
  assert list(map(float, stream(output, "-column=twice"))) == [2, 4, 6, 8, 10]
  assert list(map(float, stream(output, "-parameter=xMean"))) == [2, 4.5]
  assert list(map(float, stream(output, "-column=centered"))) == [-1, 0, 1, -0.5, 0.5]
  ascii_output = tmp_path / "ascii.sdds"
  run("sddsconvert", output, ascii_output, "-ascii")
  text = ascii_output.read_text(encoding="utf8")
  assert text.startswith("SDDS6\n")
  assert text.count(f'enum_values="{MAPPING}"') == 4


@requires_enum_tools
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


@requires_enum_tools
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


@requires_enum_tools
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


@requires_enum_tools
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


@requires_enum_tools
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


@requires_enum_tools
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


@requires_enum_tools
def test_verbose_prints_enum_parameter_labels_after_deletion(tmp_path):
  source = enum_input(tmp_path / "input.sdds")
  result = run("sddsprocess", source, tmp_path / "output.sdds",
               "-delete=parameter,expression", "-verbose")
  assert "FAULT" in result.stderr and "ON" in result.stderr


@requires_enum_tools
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


@requires_enum_tools
@pytest.mark.parametrize("kind,source", [("column", "state"), ("parameter", "mode")])
def test_enum_print_bounds_long_labels(tmp_path, kind, source):
  source_file = enum_input(tmp_path / "input.sdds")
  source_file.write_text(source_file.read_text().replace("FAULT", "a" * 2000), encoding="utf8")
  result = run("sddsprocess", source_file, tmp_path / "output.sdds",
               f"-print={kind},label,%s,{source}", check=False)
  assert result.returncode > 0
  assert "Enum display buffer too small" in result.stderr


@requires_enum_tools
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


@requires_enum_tools
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


@requires_enum_tools
def test_enum_codes_require_explicit_conversion_before_arithmetic(tmp_path):
  source, codes, output = [tmp_path / name for name in ("source.sdds", "codes.sdds", "output.sdds")]
  enum_input(source)
  run("sddscast", source, codes, "-cast=column,state,enum,long")
  run("sddsprocess", codes, output, "-define=column,result,state 1 +")
  assert list(map(float, stream(output, "-column=result"))) == [0, 2147483648, -2147483647, 11, 0]
  assert stream(output, "-parameter=mode") == ["FAULT", "ON"]


@requires_enum_tools
@pytest.mark.parametrize("kind,field", [("column", "state"), ("parameter", "mode")])
def test_enum_metadata_can_be_printed(tmp_path, kind, field):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  run("sddsprocess", source, output,
      f"-print={kind},metadata,%s|%s,{field}.type,{field}.enum_values")
  expected = [f"enum|{MAPPING}"] * (5 if kind == "column" else 2)
  assert stream(output, f"-{kind}=metadata") == expected


@requires_enum_tools
@pytest.mark.parametrize("selection,expected", [
  ('-test=column,state "FAULT" streq', ["FAULT", "FAULT"]),
  ('-test=parameter,mode "ON" streq', ["ON", "FAULT"]),
])
def test_rpn_tests_compare_enum_labels(tmp_path, selection, expected):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  run("sddsprocess", source, output, selection)
  assert stream(output, "-column=state") == expected


@requires_enum_tools
def test_enum_print_column_and_parameter_can_share_a_name(tmp_path):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  text = source.read_text().replace(MAPPING, MAPPING + ",0:OFF")
  text = text.replace('&data mode=ascii &end',
                      f'&parameter name=state, type=enum, enum_values="{MAPPING},0:OFF", fixed_value=10 &end\n'
                      '&data mode=ascii &end').replace('-1 1 "x 2 *"', '0 1 "x 2 *"')
  source.write_text(text, encoding="utf8")
  run("sddsprocess", source, output, "-print=column,label,%s,state")
  assert stream(output, "-column=label") == ["OFF", "MAX", "MIN", "ON", "FAULT"]


@requires_enum_tools
@pytest.mark.parametrize("kind,first,second,expected", [
  ("column", "state", "mode", ["FAULT%/FAULT", "MAX%/FAULT", "MIN%/FAULT", "ON%/ON", "FAULT%/ON"]),
  ("parameter", "mode", "fixed", ["FAULT%/ON", "ON%/ON"]),
])
def test_enum_print_allows_literal_percent_between_sources(tmp_path, kind, first, second, expected):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  run("sddsprocess", source, output, f"-print={kind},label,%s%%/%s,{first},{second}")
  assert stream(output, f"-{kind}=label") == expected


@requires_enum_tools
@pytest.mark.parametrize("notation", ["@", "@@"])
def test_expression_parameter_uses_output_layout_after_enum_deletion(tmp_path, notation):
  source, output = enum_input(tmp_path / "source.sdds"), tmp_path / "output.sdds"
  if notation == "@@":
    source.write_text(source.read_text().replace('"2 3 +"', '"2+3"').replace('"3 4 +"', '"3+4"'),
                      encoding="utf8")
  run("sddsprocess", source, output, "-delete=parameter,mode,fixed",
      f"-define=column,result,{notation}expression")
  assert list(map(float, stream(output, "-column=result"))) == [5, 5, 5, 7, 7]


@requires_enum_tools
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


@requires_enum_tools
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
