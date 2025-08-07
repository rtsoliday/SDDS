import math
import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSUNWRAP = BIN_DIR / "sddsunwrap"
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"
SDDS2STREAM = BIN_DIR / "sdds2stream"

text = Path("SDDSaps/sddsunwrap.c").read_text()
match = re.search(r"static char \*option\[N_OPTIONS\] = \{(.*?)\};", text, re.S)
option_names = [name.strip().strip('"') for name in match.group(1).split(',') if name.strip()]

expected_base = [0.0, 1.0, 2.0, -2.5 + 2 * math.pi, -1.0 + 2 * math.pi]
expected_threshold = [0.0, 1.0, 2.0, -2.5, -1.0]
expected_modulo = [0.0, 1.0, 2.0, -2.5 + 5.0, -1.0 + 5.0]

option_args = {
    "column": {"args": ["-column=phase"], "expected": expected_base},
    "threshold": {"args": ["-column=phase", "-threshold=5"], "expected": expected_threshold},
    "modulo": {"args": ["-column=phase", "-modulo=5"], "expected": expected_modulo},
    "majorOrder": {"args": ["-column=phase", "-majorOrder=column"], "expected": expected_base},
    "pipe": {"args": ["-column=phase", "-pipe=out"], "expected": expected_base, "pipe": True},
}

missing = set(option_names) - option_args.keys()
assert not missing, f"Missing argument mapping for options: {missing}"

binaries = [SDDSUNWRAP, SDDSMAKEDATASET, SDDS2STREAM]
pytestmark = pytest.mark.skipif(not all(p.exists() for p in binaries), reason="required binaries not built")


def make_phase_file(path: Path):
    subprocess.run(
        [
            str(SDDSMAKEDATASET),
            str(path),
            "-column=phase,type=double",
            "-data=0,1,2,-2.5,-1",
        ],
        check=True,
    )


def read_unwrapped(path: Path | bytes):
    if isinstance(path, Path):
        result = subprocess.run(
            [str(SDDS2STREAM), str(path), "-columns=Unwrapphase"],
            capture_output=True,
            text=True,
            check=True,
        )
        output = result.stdout
    else:
        result = subprocess.run(
            [str(SDDS2STREAM), "-pipe", "-columns=Unwrapphase"],
            input=path,
            capture_output=True,
            check=True,
        )
        output = result.stdout.decode()
    return [float(x) for x in output.strip().splitlines()]


@pytest.mark.parametrize("name", option_names)
def test_sddsunwrap_options(name, tmp_path: Path):
    phase_path = tmp_path / "phase.sdds"
    make_phase_file(phase_path)
    option = option_args[name]
    args = option["args"]
    expected = option["expected"]
    if option.get("pipe"):
        proc = subprocess.run(
            [str(SDDSUNWRAP), str(phase_path), *args],
            capture_output=True,
            check=True,
        )
        values = read_unwrapped(proc.stdout)
    else:
        out_path = tmp_path / "out.sdds"
        subprocess.run(
            [str(SDDSUNWRAP), str(phase_path), str(out_path), *args],
            check=True,
        )
        values = read_unwrapped(out_path)
    assert values == pytest.approx(expected)
