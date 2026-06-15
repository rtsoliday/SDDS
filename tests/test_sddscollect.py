import re
import subprocess
from pathlib import Path
import pytest

from sdds_test_utils import BIN_DIR
SDDSCOLLECT = BIN_DIR / "sddscollect"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SOURCE = Path("SDDSaps") / "sddscollect.c"

def extract_options():
    text = SOURCE.read_text()
    match = re.search(r'static char \*option\[[^\]]+\] = \{([^}]+)\};', text, re.DOTALL)
    assert match, "Option array not found"
    return re.findall(r'"([^"\\]+)"', match.group(1))

OPTIONS = extract_options()

OPTION_RUNS = {
    "collect": (["-collect"], f"Error ({SDDSCOLLECT}): Invalid -collect syntax\n"),
    "pipe": (["-pipe"], f"Error ({SDDSCOLLECT}): At least one -collect option must be given\n"),
    "nowarnings": (["-nowarnings"], f"Error ({SDDSCOLLECT}): At least one -collect option must be given\n"),
    "majorOrder": (["-majorOrder=row"], f"Error ({SDDSCOLLECT}): At least one -collect option must be given\n"),
}

pytestmark = pytest.mark.skipif(
    not (SDDSCOLLECT.exists() and SDDSQUERY.exists() and SDDS2STREAM.exists()),
    reason="tools not built",
)


def write_input(path):
    path.write_text(
        """SDDS1
&column name=Q1x, type=double, units=mm, &end
&column name=Q1y, type=double, units=mm, &end
&column name=Q2x, type=double, units=mm, &end
&column name=Q2y, type=double, units=mm, &end
&data mode=ascii, &end
2
1 10 2 20
3 30 4 40
""",
        encoding="ascii",
    )
    return path


def query_columns(path):
    result = subprocess.run([str(SDDSQUERY), str(path), "-columnlist"], capture_output=True, text=True, check=True)
    return result.stdout.strip().splitlines()


def stream_columns(path, columns):
    result = subprocess.run(
        [str(SDDS2STREAM), str(path), f"-columns={columns}", "-delimiter=|"],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip().splitlines()


@pytest.mark.parametrize("option", OPTIONS)
def test_sddscollect_options(option):
    assert option in OPTION_RUNS
    cmd, expected = OPTION_RUNS[option]
    result = subprocess.run([str(SDDSCOLLECT), *cmd], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    assert result.stderr == expected


def test_collect_suffix_creates_grouped_columns(tmp_path):
    input_file = write_input(tmp_path / "input.sdds")
    output = tmp_path / "suffix.sdds"
    subprocess.run(
        [str(SDDSCOLLECT), str(input_file), str(output), "-collect=suffix=x,column=X", "-collect=suffix=y,column=Y"],
        check=True,
    )
    assert {"Rootname", "Units", "X", "Y"}.issubset(set(query_columns(output)))
    rows = stream_columns(output, "Rootname,Units,X,Y")
    assert rows == [
        "Q1|mm|1.000000000000000e+00|1.000000000000000e+01",
        "Q2|mm|2.000000000000000e+00|2.000000000000000e+01",
        "Q1|mm|3.000000000000000e+00|3.000000000000000e+01",
        "Q2|mm|4.000000000000000e+00|4.000000000000000e+01",
    ]


def test_collect_prefix_with_exclude_and_major_order(tmp_path):
    input_file = write_input(tmp_path / "input.sdds")
    output = tmp_path / "prefix.sdds"
    subprocess.run(
        [
            str(SDDSCOLLECT),
            str(input_file),
            str(output),
            "-collect=prefix=Q,column=Value,exclude=*y",
            "-majorOrder=column",
            "-nowarnings",
        ],
        check=True,
    )
    rows = stream_columns(output, "Rootname,Value")
    assert rows == [
        "1x|1.000000000000000e+00",
        "1y|1.000000000000000e+01",
        "2x|2.000000000000000e+00",
        "2y|2.000000000000000e+01",
        "1x|3.000000000000000e+00",
        "1y|3.000000000000000e+01",
        "2x|4.000000000000000e+00",
        "2y|4.000000000000000e+01",
    ]


def test_collect_pipe_output_matches_file_output(tmp_path):
    input_file = write_input(tmp_path / "input.sdds")
    file_output = tmp_path / "file.sdds"
    subprocess.run([str(SDDSCOLLECT), str(input_file), str(file_output), "-collect=suffix=x,column=X"], check=True)
    piped = subprocess.run(
        [str(SDDSCOLLECT), str(input_file), "-collect=suffix=x,column=X", "-pipe=out"],
        stdout=subprocess.PIPE,
        check=True,
    )
    pipe_output = tmp_path / "pipe.sdds"
    pipe_output.write_bytes(piped.stdout)
    assert stream_columns(pipe_output, "Rootname,X") == stream_columns(file_output, "Rootname,X")


def test_collect_match_editcommand_and_original_page(tmp_path):
    input_file = write_input(tmp_path / "match.sdds")
    output = tmp_path / "match_out.sdds"
    subprocess.run(
        [
            str(SDDSCOLLECT),
            str(input_file),
            str(output),
            "-collect=match=Q*x,column=XOnly,editCommand=s/x$//",
        ],
        check=True,
    )
    columns = query_columns(output)
    assert "XOnly" in columns
    rows = stream_columns(output, "Rootname,XOnly")
    assert rows == [
        "Q1x|1.000000000000000e+00",
        "Q2x|2.000000000000000e+00",
        "Q1x|3.000000000000000e+00",
        "Q2x|4.000000000000000e+00",
    ]
