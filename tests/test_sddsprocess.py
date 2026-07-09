import math
import subprocess
from pathlib import Path

import pytest


from sdds_test_utils import BIN_DIR
SDDSPROCESS = BIN_DIR / "sddsprocess"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"

REQUIRED_TOOLS = [SDDSPROCESS, SDDSCHECK, SDDS2STREAM, SDDSQUERY]

pytestmark = pytest.mark.skipif(
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


def test_sddsprocess(tmp_path):
  output = tmp_path / "process.sdds"
  run_process(
    "SDDSlib/demo/example.sdds",
    output,
    "-define=column,Index,i_row,type=long",
  )
  check_sdds(output)


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
