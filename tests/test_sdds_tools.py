import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSCONVERT = BIN_DIR / "sddsconvert"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2PLAINDATA = BIN_DIR / "sdds2plaindata"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSBREAK = BIN_DIR / "sddsbreak"
SDDSCOLLAPSE = BIN_DIR / "sddscollapse"
SDDSCOMBINE = BIN_DIR / "sddscombine"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDSPROCESS = BIN_DIR / "sddsprocess"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDSSORT = BIN_DIR / "sddssort"

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


@pytest.mark.skipif(not (PLAINDATA2SDDS.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_plaindata2sdds(tmp_path):
    plain = tmp_path / "input.txt"
    plain.write_text("1 2\n3 4\n")
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(output),
            "-column=col1,double",
            "-column=col2,double",
            "-inputMode=ascii",
            "-outputMode=ascii",
        ],
        check=True,
    )
    result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not SDDS2PLAINDATA.exists(), reason="sdds2plaindata not built")
def test_sdds2plaindata(tmp_path):
    output = tmp_path / "plain.txt"
    subprocess.run(
        [
            str(SDDS2PLAINDATA),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-column=longCol",
            "-parameter=stringParam",
            "-outputMode=ascii",
        ],
        check=True,
    )
    assert output.exists()


@pytest.mark.skipif(not SDDS2STREAM.exists(), reason="sdds2stream not built")
def test_sdds2stream():
    result = subprocess.run(
        [
            str(SDDS2STREAM),
            "SDDSlib/demo/example.sdds",
            "-columns=longCol",
            "-parameters=stringParam",
            "-page=1",
            "-delimiter=,",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    assert "100\n200\n300\n400\n500\n" in result.stdout


@pytest.mark.skipif(not (SDDSBREAK.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddsbreak(tmp_path):
    output = tmp_path / "break.sdds"
    subprocess.run(
        [
            str(SDDSBREAK),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-rowlimit=2",
        ],
        check=True,
    )
    result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not (SDDSCOLLAPSE.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddscollapse(tmp_path):
    output = tmp_path / "collapse.sdds"
    subprocess.run(
        [str(SDDSCOLLAPSE), "SDDSlib/demo/example.sdds", str(output)],
        check=True,
    )
    result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"



@pytest.mark.skipif(not (SDDSCOMBINE.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddscombine(tmp_path):
    output = tmp_path / "combine.sdds"
    subprocess.run(
        [
            str(SDDSCOMBINE),
            "SDDSlib/demo/example.sdds",
            "SDDSlib/demo/example.sdds",
            str(output),
            "-overWrite",
        ],
        check=True,
    )
    result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not SDDSPRINTOUT.exists(), reason="sddsprintout not built")
def test_sddsprintout(tmp_path):
    output = tmp_path / "print.txt"
    subprocess.run(
        [
            str(SDDSPRINTOUT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-columns=doubleCol",
            "-parameters=stringParam",
            "-noTitle",
        ],
        check=True,
    )
    assert output.exists()


@pytest.mark.skipif(not (SDDSPROCESS.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddsprocess(tmp_path):
    output = tmp_path / "process.sdds"
    subprocess.run(
        [
            str(SDDSPROCESS),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-define=column,Index,i_row,type=long",
        ],
        check=True,
    )
    result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_sddsquery():
    result = subprocess.run(
        [str(SDDSQUERY), "SDDSlib/demo/example.sdds", "-columnlist"],
        capture_output=True,
        text=True,
        check=True,
    )
    assert "shortCol" in result.stdout


@pytest.mark.skipif(not (SDDSSORT.exists() and SDDSCHECK.exists()), reason="tools not built")
def test_sddssort(tmp_path):
    output = tmp_path / "sort.sdds"
    subprocess.run(
        [
            str(SDDSSORT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-column=longCol",
        ],
        check=True,
    )
    result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"




