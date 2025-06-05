import subprocess
from pathlib import Path
import pytest

SDDSCHECK = Path("bin/Linux-x86_64/sddscheck")
SDDSCONVERT = Path("bin/Linux-x86_64/sddsconvert")

@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_ok():
    result = subprocess.run([str(SDDSCHECK), "SDDSlib/demo/example.sdds"], capture_output=True, text=True)
    assert result.returncode == 0
    assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_print_errors():
    result = subprocess.run([str(SDDSCHECK), "SDDSlib/demo/example.sdds", "-printErrors"], capture_output=True, text=True)
    assert result.returncode == 0
    assert result.stdout.strip() == "ok"

@pytest.mark.skipif(not (SDDSCHECK.exists() and SDDSCONVERT.exists()), reason="tools not built")
def test_sddsconvert(tmp_path):
    binary_file = tmp_path / "binary.sdds"
    ascii_file = tmp_path / "ascii.sdds"

    subprocess.run([str(SDDSCONVERT), "SDDSlib/demo/example.sdds", str(binary_file), "-binary"], check=True)
    result = subprocess.run([str(SDDSCHECK), str(binary_file)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"

    subprocess.run([str(SDDSCONVERT), str(binary_file), str(ascii_file), "-ascii"], check=True)
    result = subprocess.run([str(SDDSCHECK), str(ascii_file)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not (SDDSCHECK.exists() and SDDSCONVERT.exists()), reason="tools not built")
@pytest.mark.parametrize(
    "option",
    [
        ["-binary"],
        ["-ascii"],
        ["-delete=column,shortCol"],
        ["-retain=column,shortCol"],
        ["-rename=column,shortCol=shortCol2"],
        ["-description=test,contents"],
        ["-table=1"],
        ["-editnames=column,*Col,*New"],
        ["-linesperrow=1"],
        ["-nowarnings"],
        ["-recover"],
        ["-pipe=output"],
        ["-fromPage=1"],
        ["-toPage=1"],
        ["-acceptAllNames"],
        ["-removePages=1"],
        ["-keepPages=1"],
        ["-rowlimit=1"],
        ["-majorOrder=column"],
        ["-convertUnits=column,doubleCol,m"],
    ],
)
def test_sddsconvert_options(tmp_path, option):
    output = tmp_path / "out.sdds"
    cmd = [str(SDDSCONVERT), "SDDSlib/demo/example.sdds"]
    if option[0].startswith("-pipe"):
        cmd.append(option[0])
        with output.open("wb") as f:
            subprocess.run(cmd, stdout=f, check=True)
    else:
        cmd += [str(output)] + option
        subprocess.run(cmd, check=True)
    result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"

