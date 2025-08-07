import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSNAP2GRID = BIN_DIR / "sddssnap2grid"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"

REQUIRED_TOOLS = [SDDSSNAP2GRID, PLAINDATA2SDDS, SDDS2PLAINDATA]
pytestmark = pytest.mark.skipif(
    not all(tool.exists() for tool in REQUIRED_TOOLS),
    reason="sdds tools not built",
)

VALUES = [0, 1.02, 1.97, 3.04, 4.01]

def create_sdds(values, tmp_path):
    plain = tmp_path / "input.txt"
    plain.write_text("\n".join(str(v) for v in values) + "\n")
    sdds = tmp_path / "input.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(sdds),
            "-column=col,double",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    return sdds

def read_sdds(path):
    plain = path.with_suffix(".txt")
    subprocess.run(
        [
            str(SDDS2PLAINDATA),
            str(path),
            str(plain),
            "-column=col",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    return [float(x) for x in plain.read_text().split()]

def assert_grid(values):
    deltas = [values[i + 1] - values[i] for i in range(len(values) - 1)]
    first = deltas[0]
    assert all(abs(d - first) < 1e-6 for d in deltas[1:])

@pytest.mark.parametrize(
    "column_option",
    [
        "-column=col,maximumBins=50,adjustFactor=0.8",
        "-column=col,binFactor=10",
        "-column=col,deltaGuess=1",
    ],
)
def test_column_variants(column_option, tmp_path):
    input_sdds = create_sdds(VALUES, tmp_path)
    output = tmp_path / "out.sdds"
    subprocess.run(
        [str(SDDSSNAP2GRID), str(input_sdds), str(output), column_option],
        check=True,
    )
    data = read_sdds(output)
    assert_grid(data)

def test_make_parameters(tmp_path):
    input_sdds = create_sdds(VALUES, tmp_path)
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSSNAP2GRID),
            str(input_sdds),
            str(output),
            "-column=col,binFactor=10",
            "-makeParameters",
        ],
        check=True,
    )
    data = read_sdds(output)
    delta = data[1] - data[0] if len(data) > 1 else 0
    result = subprocess.run(
        [
            str(SDDS2PLAINDATA),
            str(output),
            "-pipe=out",
            "-parameter=colMinimum",
            "-parameter=colMaximum",
            "-parameter=colInterval",
            "-parameter=colDimension",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    lines = [line.strip() for line in result.stdout.splitlines() if line.strip()]
    read_min = float(lines[0])
    read_max = float(lines[1])
    read_interval = float(lines[2])
    read_dim = int(lines[3])
    assert read_min == pytest.approx(data[0])
    assert read_max == pytest.approx(data[-1])
    assert read_interval == pytest.approx(delta)
    assert read_dim == len(data)

def test_pipe_output(tmp_path):
    input_sdds = create_sdds(VALUES, tmp_path)
    result = subprocess.run(
        [
            str(SDDSSNAP2GRID),
            str(input_sdds),
            "-pipe=out",
            "-column=col,binFactor=10",
        ],
        check=True,
        stdout=subprocess.PIPE,
    )
    out_path = tmp_path / "pipe_out.sdds"
    out_path.write_bytes(result.stdout)
    data = read_sdds(out_path)
    assert_grid(data)

def test_verbose(tmp_path):
    input_sdds = create_sdds(VALUES, tmp_path)
    output = tmp_path / "out.sdds"
    result = subprocess.run(
        [
            str(SDDSSNAP2GRID),
            str(input_sdds),
            str(output),
            "-column=col,binFactor=10",
            "-verbose",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    assert "Completed work for col" in result.stdout
    data = read_sdds(output)
    assert_grid(data)
