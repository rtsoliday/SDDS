import subprocess
import re
from pathlib import Path
import pytest

SRC_FILE = Path("SDDSaps/sddshist.c")
BIN_DIR = Path("bin/Linux-x86_64")
SDDSHIST = BIN_DIR / "sddshist"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDS2STREAM = BIN_DIR / "sdds2stream"


def extract_options():
    text = SRC_FILE.read_text()
    match = re.search(r'char \*option\[.*?\] = \{([^}]+)\};', text, re.DOTALL)
    if not match:
        return []
    return re.findall(r"\"([^\"]+)\"", match.group(1))


def extract_usage():
    text = SRC_FILE.read_text()
    start = text.index('char *USAGE =')
    start = text.index('"', start)
    end = text.index('"Program by', start)
    segment = text[start:end]
    lines = re.findall(r'"([^\"]*)"', segment)
    usage = "".join(lines)
    return usage.replace("\\n", "\n")


OPTIONS = extract_options()
USAGE_TEXT = extract_usage()
pytestmark = pytest.mark.skipif(
    not (SDDSHIST.exists() and SDDSQUERY.exists() and SDDS2STREAM.exists()),
    reason="tools not built",
)


def write_input(path):
    path.write_text(
        """SDDS1
&parameter name=tag, type=string, &end
&column name=x, type=double, &end
&column name=gate, type=double, &end
&column name=weight, type=double, &end
&data mode=ascii, &end
hist
4
1 0 1
2 1 2
3 1 3
4 0 4
""",
        encoding="ascii",
    )
    return path


def query_columns(path):
    result = subprocess.run([str(SDDSQUERY), str(path), "-columnlist"], capture_output=True, text=True, check=True)
    return result.stdout.strip().splitlines()


def query_parameters(path):
    result = subprocess.run([str(SDDSQUERY), str(path), "-parameterlist"], capture_output=True, text=True, check=True)
    return result.stdout.strip().splitlines()


def stream_columns(path, columns):
    result = subprocess.run(
        [str(SDDS2STREAM), str(path), f"-columns={columns}", "-delimiter=|"],
        capture_output=True,
        text=True,
        check=True,
    )
    return [line.split("|") for line in result.stdout.strip().splitlines() if line.strip()]


def stream_parameter(path, parameter):
    result = subprocess.run([str(SDDS2STREAM), str(path), f"-parameters={parameter}"], capture_output=True, text=True, check=True)
    return result.stdout.strip()


@pytest.mark.parametrize("opt", OPTIONS)
def test_sddshist_option(opt):
    result = subprocess.run([str(SDDSHIST), f"-{opt}"], capture_output=True, text=True)
    combined = result.stdout + result.stderr
    output = combined.split("Program by")[0]
    assert output == USAGE_TEXT


def test_basic_histogram_bins_and_statistics(tmp_path):
    input_file = write_input(tmp_path / "input.sdds")
    output = tmp_path / "hist.sdds"
    subprocess.run(
        [str(SDDSHIST), str(input_file), str(output), "-dataColumn=x", "-bins=4", "-statistics"],
        check=True,
    )
    assert {"frequency", "x"}.issubset(set(query_columns(output)))
    assert {"sddshistBins", "sddshistBinSize", "sddshistBinned", "xMean", "xStDev", "xRms"}.issubset(set(query_parameters(output)))
    rows = stream_columns(output, "x,frequency")
    assert len(rows) == 4
    histogram = [float(row[1]) for row in rows]
    assert histogram == pytest.approx([1, 1, 1, 1])
    assert stream_parameter(output, "sddshistBinned") == "4"
    assert float(stream_parameter(output, "xMean")) == pytest.approx(2.5)


def test_weighted_histogram_and_filter(tmp_path):
    input_file = write_input(tmp_path / "input.sdds")
    output = tmp_path / "weighted.sdds"
    subprocess.run(
        [
            str(SDDSHIST),
            str(input_file),
            str(output),
            "-dataColumn=x",
            "-bins=2",
            "-weightColumn=weight",
            "-filter=gate,1,1",
        ],
        check=True,
    )
    rows = stream_columns(output, "frequency")
    histogram = [float(row[0]) for row in rows]
    assert histogram == pytest.approx([2, 3])
    assert stream_parameter(output, "sddshistBinned") == "2"


def test_normalize_and_cdf_columns(tmp_path):
    input_file = write_input(tmp_path / "input.sdds")
    output = tmp_path / "cdf.sdds"
    subprocess.run(
        [str(SDDSHIST), str(input_file), str(output), "-dataColumn=x", "-bins=4", "-normalize=sum", "-cdf"],
        check=True,
    )
    assert {"frequency", "xCdf"}.issubset(set(query_columns(output)))
    rows = stream_columns(output, "frequency,xCdf")
    hist = [float(row[0]) for row in rows]
    cdf = [float(row[1]) for row in rows]
    assert hist == pytest.approx([0.25, 0.25, 0.25, 0.25])
    assert cdf == pytest.approx([0.25, 0.5, 0.75, 1.0])


def test_cdf_only_pipe_output_and_major_order(tmp_path):
    input_file = write_input(tmp_path / "input.sdds")
    result = subprocess.run(
        [
            str(SDDSHIST),
            str(input_file),
            "-dataColumn=x",
            "-bins=4",
            "-cdf=only",
            "-majorOrder=column",
            "-pipe=out",
            "-threads=2",
            "-verbose",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    output = tmp_path / "pipe.sdds"
    output.write_bytes(result.stdout)
    columns = set(query_columns(output))
    assert "xCdf" in columns
    assert "frequency" not in columns
    assert b"column_major_order=1" in result.stdout
    assert b"histogrammed" in result.stderr

