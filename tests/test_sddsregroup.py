import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSREGROUP = BIN_DIR / "sddsregroup"
SDDSCONVERT = BIN_DIR / "sddsconvert"

INPUT_DATA = """SDDS5
&description text="test", contents="test", &end
&parameter name=p_to_col, type=long, &end
&parameter name=keep_param, type=long, &end
&column name=c_to_par, type=long, &end
&column name=keep_col, type=long, &end
&data mode=ascii, &end
10
100
2
1 2
3 4
20
200
2
5 6
7 8
30
300
2
9 10
11 12
"""


def write_input(path):
    path.write_text(INPUT_DATA)


def read_sdds(path, n_params, n_cols):
    lines = Path(path).read_text().splitlines()
    idx = lines.index("&data mode=ascii, &end") + 1
    pages = []
    while idx < len(lines):
        if not lines[idx] or lines[idx].startswith("!"):
            idx += 1
            continue
        params = [int(lines[idx + i].split()[0]) for i in range(n_params)]
        idx += n_params
        if idx >= len(lines):
            break
        rows = int(lines[idx].split()[0])
        idx += 1
        data = []
        for _ in range(rows):
            data.append([int(x) for x in lines[idx].split()[:n_cols]])
            idx += 1
        pages.append((params, data))
    return pages


EXPECTED_DEFAULT = [
    ([], [[1, 2], [5, 6], [9, 10]]),
    ([], [[3, 4], [7, 8], [11, 12]]),
]


@pytest.mark.skipif(not (SDDSREGROUP.exists() and SDDSCONVERT.exists()), reason="sddsregroup not built")
def test_newparameters(tmp_path):
    inp = tmp_path / "input.sdds"
    out = tmp_path / "out.sdds"
    ascii_out = tmp_path / "out_ascii.sdds"
    write_input(inp)
    subprocess.run([str(SDDSREGROUP), str(inp), str(out), "-newparameters=c_to_par"], check=True)
    subprocess.run([str(SDDSCONVERT), str(out), str(ascii_out), "-ascii"], check=True)
    pages = read_sdds(ascii_out, 1, 1)
    assert pages == [([1], [[2], [6], [10]]), ([3], [[4], [8], [12]])]


@pytest.mark.skipif(not (SDDSREGROUP.exists() and SDDSCONVERT.exists()), reason="sddsregroup not built")
def test_newcolumns(tmp_path):
    inp = tmp_path / "input.sdds"
    out = tmp_path / "out.sdds"
    ascii_out = tmp_path / "out_ascii.sdds"
    write_input(inp)
    subprocess.run([str(SDDSREGROUP), str(inp), str(out), "-newcolumns=p_to_col"], check=True)
    subprocess.run([str(SDDSCONVERT), str(out), str(ascii_out), "-ascii"], check=True)
    pages = read_sdds(ascii_out, 0, 3)
    assert pages == [
        ([], [[10, 1, 2], [20, 5, 6], [30, 9, 10]]),
        ([], [[10, 3, 4], [20, 7, 8], [30, 11, 12]]),
    ]


@pytest.mark.skipif(not (SDDSREGROUP.exists() and SDDSCONVERT.exists()), reason="sddsregroup not built")
def test_warning(tmp_path):
    inp = tmp_path / "input.sdds"
    out = tmp_path / "out.sdds"
    ascii_out = tmp_path / "out_ascii.sdds"
    write_input(inp)
    result = subprocess.run([str(SDDSREGROUP), str(inp), str(out), "-warning"], stderr=subprocess.PIPE, check=True)
    assert result.stderr == b""
    subprocess.run([str(SDDSCONVERT), str(out), str(ascii_out), "-ascii"], check=True)
    pages = read_sdds(ascii_out, 0, 2)
    assert pages == EXPECTED_DEFAULT


@pytest.mark.skipif(not (SDDSREGROUP.exists() and SDDSCONVERT.exists()), reason="sddsregroup not built")
def test_verbose(tmp_path):
    inp = tmp_path / "input.sdds"
    out = tmp_path / "out.sdds"
    ascii_out = tmp_path / "out_ascii.sdds"
    write_input(inp)
    result = subprocess.run([str(SDDSREGROUP), str(inp), str(out), "-verbose"], stderr=subprocess.PIPE, check=True)
    assert b"Starting page" in result.stderr
    subprocess.run([str(SDDSCONVERT), str(out), str(ascii_out), "-ascii"], check=True)
    pages = read_sdds(ascii_out, 0, 2)
    assert pages == EXPECTED_DEFAULT


@pytest.mark.skipif(not (SDDSREGROUP.exists() and SDDSCONVERT.exists()), reason="sddsregroup not built")
@pytest.mark.parametrize("order,flag", [("row", False), ("column", True)])
def test_major_order(tmp_path, order, flag):
    inp = tmp_path / "input.sdds"
    out = tmp_path / "out.sdds"
    ascii_out = tmp_path / "out_ascii.sdds"
    write_input(inp)
    subprocess.run([str(SDDSREGROUP), str(inp), str(out), f"-majorOrder={order}"], check=True)
    header = out.read_text()
    has_flag = "column_major_order=1" in header
    assert has_flag == flag
    subprocess.run([str(SDDSCONVERT), str(out), str(ascii_out), "-ascii"], check=True)
    pages = read_sdds(ascii_out, 0, 2)
    assert pages == EXPECTED_DEFAULT


@pytest.mark.skipif(not (SDDSREGROUP.exists() and SDDSCONVERT.exists()), reason="sddsregroup not built")
def test_pipe(tmp_path):
    inp = tmp_path / "input.sdds"
    write_input(inp)
    result = subprocess.run([str(SDDSREGROUP), "-pipe"], input=inp.read_bytes(), stdout=subprocess.PIPE, check=True)
    out = tmp_path / "out.sdds"
    ascii_out = tmp_path / "out_ascii.sdds"
    out.write_bytes(result.stdout)
    subprocess.run([str(SDDSCONVERT), str(out), str(ascii_out), "-ascii"], check=True)
    pages = read_sdds(ascii_out, 0, 2)
    assert pages == EXPECTED_DEFAULT
