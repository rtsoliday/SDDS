import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSORTCOLUMN = BIN_DIR / "sddssortcolumn"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSQUERY = BIN_DIR / "sddsquery"

TOOLS = [SDDSSORTCOLUMN, PLAINDATA2SDDS, SDDSQUERY]


def get_column_order(filename: Path):
    result = subprocess.run(
        [str(SDDSQUERY), str(filename), "-columnlist"],
        capture_output=True,
        text=True,
        check=True,
    )
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_decreasing_option(tmp_path: Path):
    plain = tmp_path / "data.txt"
    plain.write_text("1 2 3\n")
    input_file = tmp_path / "input.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=B,long",
            "-column=A,long",
            "-column=C,long",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    inc = tmp_path / "sorted.sdds"
    dec = tmp_path / "sorted_dec.sdds"
    subprocess.run([str(SDDSSORTCOLUMN), str(input_file), str(inc)], check=True)
    subprocess.run([str(SDDSSORTCOLUMN), str(input_file), str(dec), "-decreasing"], check=True)
    assert get_column_order(inc) == ["A", "B", "C"]
    assert get_column_order(dec) == ["C", "B", "A"]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_sortlist_option(tmp_path: Path):
    plain = tmp_path / "data.txt"
    plain.write_text("1 2 3 4\n")
    input_file = tmp_path / "input.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=A,long",
            "-column=B,long",
            "-column=C,long",
            "-column=D,long",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    output = tmp_path / "sorted_list.sdds"
    subprocess.run([str(SDDSSORTCOLUMN), str(input_file), str(output), "-sortList=C,A"], check=True)
    assert get_column_order(output) == ["C", "A", "B", "D"]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_sortwith_option(tmp_path: Path):
    plain = tmp_path / "data.txt"
    plain.write_text("1 2 3 4\n")
    input_file = tmp_path / "input.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=A,long",
            "-column=B,long",
            "-column=C,long",
            "-column=D,long",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    order_plain = tmp_path / "order.txt"
    order_plain.write_text("B\nD\n")
    order_file = tmp_path / "order.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(order_plain),
            str(order_file),
            "-column=name,string",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    output = tmp_path / "sorted_with.sdds"
    subprocess.run(
        [
            str(SDDSSORTCOLUMN),
            str(input_file),
            str(output),
            f"-sortWith={order_file},column=name",
        ],
        check=True,
    )
    assert get_column_order(output) == ["B", "D", "A", "C"]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_bpmorder_option(tmp_path: Path):
    plain = tmp_path / "data.txt"
    plain.write_text("1 2 3 4\n")
    input_file = tmp_path / "input.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=S2A:P1,long",
            "-column=S1A:P2,long",
            "-column=S1A:P0,long",
            "-column=X,long",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    output = tmp_path / "sorted_bpm.sdds"
    subprocess.run([str(SDDSSORTCOLUMN), str(input_file), str(output), "-bpmOrder"], check=True)
    assert get_column_order(output) == ["X", "S1A:P0", "S1A:P2", "S2A:P1"]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_pipe_usage(tmp_path: Path):
    plain = tmp_path / "data.txt"
    plain.write_text("1 2 3\n")
    input_file = tmp_path / "input.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=B,long",
            "-column=A,long",
            "-column=C,long",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    out_pipe = tmp_path / "pipe_out.sdds"
    with open(out_pipe, "wb") as f:
        subprocess.run([str(SDDSSORTCOLUMN), str(input_file), "-pipe=out"], stdout=f, check=True)
    assert get_column_order(out_pipe) == ["A", "B", "C"]

    out_in = tmp_path / "pipe_in.sdds"
    with open(input_file, "rb") as f:
        subprocess.run([str(SDDSSORTCOLUMN), "-pipe=in", str(out_in)], stdin=f, check=True)
    assert get_column_order(out_in) == ["A", "B", "C"]
