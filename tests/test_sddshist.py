import subprocess
import re
from pathlib import Path
import pytest

SRC_FILE = Path("SDDSaps/sddshist.c")
BIN_DIR = Path("bin/Linux-x86_64")
SDDSHIST = BIN_DIR / "sddshist"


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


@pytest.mark.skipif(not SDDSHIST.exists(), reason="sddshist not built")
@pytest.mark.parametrize("opt", OPTIONS)
def test_sddshist_option(opt):
    result = subprocess.run([str(SDDSHIST), f"-{opt}"], capture_output=True, text=True)
    combined = result.stdout + result.stderr
    output = combined.split("Program by")[0]
    assert output == USAGE_TEXT

