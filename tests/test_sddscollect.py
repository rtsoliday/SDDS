import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCOLLECT = BIN_DIR / "sddscollect"
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

@pytest.mark.skipif(not SDDSCOLLECT.exists(), reason="sddscollect not built")
@pytest.mark.parametrize("option", OPTIONS)
def test_sddscollect_options(option):
    assert option in OPTION_RUNS
    cmd, expected = OPTION_RUNS[option]
    result = subprocess.run([str(SDDSCOLLECT), *cmd], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    assert result.stderr == expected
