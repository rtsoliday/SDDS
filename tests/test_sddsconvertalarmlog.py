import subprocess
from pathlib import Path
import re
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
PROGRAM = BIN_DIR / "sddsconvertalarmlog"
SOURCE = Path("SDDSaps/sddsconvertalarmlog.c")

# Extract option names directly from the C source.
def extract_options():
    text = SOURCE.read_text()
    match = re.search(r"char \*option\[N_OPTIONS\]\s*=\s*\{([^}]+)\};", text, re.MULTILINE)
    if not match:
        raise ValueError("Option array not found")
    return [opt.strip() for opt in re.findall(r"\"([^\"]+)\"", match.group(1))]

# Map each option to a command-line invocation that uses valid syntax.
OPTION_MAP = {
    "binary": "-binary",
    "ascii": "-ascii",
    "float": "-float",
    "double": "-double",
    "pipe": "-pipe",
    "minimuminterval": "-minimumInterval=1",
    "snapshot": "-snapshot=1",
    "time": "-time=start=0,end=1",
    "delete": "-delete=col",
    "retain": "-retain=col",
}

@pytest.mark.skipif(not PROGRAM.exists(), reason="tool not built")
def test_each_option_usage():
    options = extract_options()
    baseline = subprocess.run([str(PROGRAM)], capture_output=True, text=True)
    for opt in options:
        cmd = [str(PROGRAM), OPTION_MAP[opt]]
        result = subprocess.run(cmd, capture_output=True, text=True)
        assert result.stderr == baseline.stderr, f"Option {opt} produced unexpected output"
