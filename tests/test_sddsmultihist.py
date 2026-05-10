import subprocess
import re
from pathlib import Path
import pytest

SRC_FILE = Path("SDDSaps/sddsmultihist.c")
BIN_DIR = Path("bin/Linux-x86_64")
SDDSMULTIHIST = BIN_DIR / "sddsmultihist"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSQUERY = BIN_DIR / "sddsquery"


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
REQUIRED_TOOLS = [SDDSMULTIHIST, PLAINDATA2SDDS, SDDS2STREAM, SDDSQUERY]


def stream_columns(path, columns, page=1):
    result = subprocess.run([
        str(SDDS2STREAM),
        str(path),
        f"-columns={columns}",
        f"-page={page}",
        "-delimiter=|",
    ], capture_output=True, text=True, check=True)
    return result.stdout.strip().splitlines() if result.stdout.strip() else []


@pytest.mark.skipif(not SDDSMULTIHIST.exists(), reason="sddsmultihist not built")
@pytest.mark.parametrize("opt", OPTIONS)
def test_sddsmultihist_option(opt):
    result = subprocess.run([str(SDDSMULTIHIST), f"-{opt}"], capture_output=True, text=True)
    combined = result.stdout + result.stderr
    output = combined.split("Program by")[0]
    assert output == USAGE_TEXT


@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
def test_multiple_histograms(tmp_path):
    plain = tmp_path / "plain.txt"
    plain.write_text("1 1\n2 1\n3 2\n3 2\n")
    data = tmp_path / "data.sdds"
    subprocess.run([
        str(PLAINDATA2SDDS),
        str(plain),
        str(data),
        "-column=x,double",
        "-column=y,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
    ], check=True)
    output = tmp_path / "out.sdds"
    result = subprocess.run([
        str(SDDSMULTIHIST),
        str(data),
        "-pipe=out",
        "-columns=x,y",
        "-bins=2",
        "-separate",
        "-lowerLimit=0,0",
        "-upperLimit=4,3",
    ], stdout=subprocess.PIPE, check=True)
    output.write_bytes(result.stdout)
    values = [float(x) for x in subprocess.run([
        str(SDDS2STREAM),
        str(output),
        "-columns=xFrequency,yFrequency",
        "-page=1",
    ], stdout=subprocess.PIPE, check=True).stdout.split()]
    assert values == [1.0, 2.0, 3.0, 2.0]


@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
def test_cdf_only_weighted_and_major_order(tmp_path):
    plain = tmp_path / "weighted.txt"
    plain.write_text("1 2 1\n2 2 2\n3 4 3\n4 4 4\n")
    data = tmp_path / "weighted.sdds"
    subprocess.run([
        str(PLAINDATA2SDDS),
        str(plain),
        str(data),
        "-column=x,double",
        "-column=y,double",
        "-column=w,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
    ], check=True)
    output = tmp_path / "cdf.sdds"
    subprocess.run([
        str(SDDSMULTIHIST),
        str(data),
        str(output),
        "-columns=x,y",
        "-abscissa=shared",
        "-bins=2",
        "-weightColumn=w",
        "-cdf=only",
        "-majorOrder=column",
        "-normalize=sum",
    ], check=True)
    columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
    assert "xCdf" in columns.stdout
    assert "yCdf" in columns.stdout
    assert "xFrequency" not in columns.stdout
    cdf_lines = stream_columns(output, "xCdf,yCdf")
    assert len(cdf_lines) == 2


@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
def test_normalize_peak_and_pipe_output(tmp_path):
    plain = tmp_path / "peak.txt"
    plain.write_text("1 1\n2 1\n2 1\n3 2\n")
    data = tmp_path / "peak.sdds"
    subprocess.run([
        str(PLAINDATA2SDDS),
        str(plain),
        str(data),
        "-column=x,double",
        "-column=y,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
    ], check=True)
    result = subprocess.run([
        str(SDDSMULTIHIST),
        str(data),
        "-pipe=out",
        "-columns=x,y",
        "-bins=2",
        "-separate",
        "-normalize=peak",
        "-majorOrder=column",
    ], stdout=subprocess.PIPE, check=True)
    output = tmp_path / "pipe.sdds"
    output.write_bytes(result.stdout)
    columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
    assert "x" in columns.stdout
    assert "y" in columns.stdout


@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
def test_autobins_exclude_and_sides(tmp_path):
    plain = tmp_path / "auto.txt"
    plain.write_text("1 10 100\n2 20 200\n3 30 300\n4 40 400\n")
    data = tmp_path / "auto.sdds"
    subprocess.run([
        str(PLAINDATA2SDDS),
        str(plain),
        str(data),
        "-column=x,double",
        "-column=y,double",
        "-column=z,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
    ], check=True)
    output = tmp_path / "auto-out.sdds"
    subprocess.run([
        str(SDDSMULTIHIST),
        str(data),
        str(output),
        "-columns=x,y,z",
        "-exclude=z",
        "-abscissa=shared",
        "-autobins=target=2,minimum=2,maximum=3",
        "-sides=close",
    ], check=True)
    columns = subprocess.run([str(SDDSQUERY), str(output), "-columnlist"], capture_output=True, text=True, check=True)
    assert "xFrequency" in columns.stdout
    assert "yFrequency" in columns.stdout
    assert "zFrequency" not in columns.stdout


@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sdds tools not built")
def test_expand_and_no_normalize(tmp_path):
    plain = tmp_path / "expand.txt"
    plain.write_text("1 5\n2 6\n3 7\n4 8\n")
    data = tmp_path / "expand.sdds"
    subprocess.run([
        str(PLAINDATA2SDDS),
        str(plain),
        str(data),
        "-column=x,double",
        "-column=y,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
    ], check=True)
    output = tmp_path / "expand-out.sdds"
    subprocess.run([
        str(SDDSMULTIHIST),
        str(data),
        str(output),
        "-columns=x,y",
        "-abscissa=shared",
        "-bins=3",
        "-expand=0.2",
        "-normalize=no",
    ], check=True)
    lines = stream_columns(output, "xFrequency,yFrequency")
    assert len(lines) == 3

