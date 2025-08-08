import subprocess
import re
from pathlib import Path
import pytest

SRC_FILE = Path("SDDSaps/sddsmultihist.c")
BIN_DIR = Path("bin/Linux-x86_64")
SDDSMULTIHIST = BIN_DIR / "sddsmultihist"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
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
REQUIRED_TOOLS = [SDDSMULTIHIST, PLAINDATA2SDDS, SDDS2STREAM]


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
    result = subprocess.run([
        str(SDDS2STREAM),
        str(output),
        "-columns=xFrequency,yFrequency",
        "-page=1",
    ], stdout=subprocess.PIPE, check=True)
    values = [float(x) for x in result.stdout.split()]
    assert values == [1.0, 2.0, 3.0, 2.0]
