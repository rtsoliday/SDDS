import subprocess
from pathlib import Path
import hashlib
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
    "option, expected, extension",
    [
        ("-binary", "ac679ee24199ba7f5d61f8bad3d05ce1d093d44b78c9ec60f0adf7ffd50c0db7", "sdds"),
        ("-ascii", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
        ("-delete=column,shortCol", "dc3302039dec1b6883bc69efb8c1fadfb8ef27b671e612c610f76a6bf28c6a41", "sdds"),
        ("-retain=column,shortCol", "a6f6ed125314ea6e6f7302f360848bebdb65ccb8d397b5052f592cf8ca8599db", "sdds"),
        ("-rename=column,shortCol=shortCol2", "71fcb900a567cf0ee83550df3e0d679ac41123725d96a9de93d3a687e53c9825", "sdds"),
        ("-description=test,contents", "2f39ea37ba4a9235ab34001723c50db8eb1b388d0c1d6f1f29acceb5cd0421f0", "sdds"),
        ("-table=1", "d3fa264796df7f0b431ea4f46bdb9a89a08d10bcfbd6c1e6c58b4dda8b154d1f", "sdds"),
        ("-editnames=column,*Col,%g/Col/New/", "2c6bb35829e95688ba515e86c1e342ecdd1af639cade30cc3094526344906c63", "sdds"),
        ("-linesperrow=1", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
        ("-nowarnings", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
        ("-recover", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
        ("-pipe=output", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
        ("-fromPage=1", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
        ("-toPage=1", "d3fa264796df7f0b431ea4f46bdb9a89a08d10bcfbd6c1e6c58b4dda8b154d1f", "sdds"),
        ("-acceptAllNames", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
        ("-removePages=1", "2d4e6ca23ec22deb07204209c7015b8ff05b4666e93b769410974005aa4cafe7", "sdds"),
        ("-keepPages=1", "d3fa264796df7f0b431ea4f46bdb9a89a08d10bcfbd6c1e6c58b4dda8b154d1f", "sdds"),
        ("-rowlimit=1", "b9a8876206940b40825e9bd900e00b4453d295bcac4021bb031ca06f174e11b9", "sdds"),
        ("-majorOrder=column", "d31aa9cdd0155102d1e48b833d37f9bb32145df43422e8b6b72c1183dce0bfd2", "sdds"),
        ("-convertUnits=column,doubleCol,m", "b56f9ac3cc769dc45947a6103b4c0edad7acd021f7c4219746f60049947609a9", "sdds"),
        ("-xzLevel=0", "eac5eda07d13fe37c0467828d3d34de5239d27ef8901642adc45b33509e2486b", "sdds.xz"),
    ],
)
def test_sddsconvert_options(tmp_path, option, expected, extension):
    output = tmp_path / f"out.{extension}"
    cmd = [str(SDDSCONVERT), "SDDSlib/demo/example.sdds"]
    if option.startswith("-pipe"):
        cmd.append(option)
        with output.open("wb") as f:
            subprocess.run(cmd, stdout=f, check=True)
    else:
        cmd += [str(output), option]
        subprocess.run(cmd, check=True)
    result = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"
    data = output.read_bytes()
    assert hashlib.sha256(data).hexdigest() == expected


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
@pytest.mark.parametrize(
    "args, expected",
    [
        (["-columns=longCol", "-page=1"], "100\n200\n300\n400\n500\n"),
        (["-parameters=stringParam"], "FirstPage\nSecondPage\n"),
        (["-arrays=shortArray"], "1 2 3 \n7 8 \n"),
        (["-columns=longCol", "-page=2"], "600\n700\n800\n"),
        (
            ["-columns=longCol,shortCol", "-page=1", "-delimiter=:"],
            "100:1\n200:2\n300:3\n400:4\n500:5\n",
        ),
        (
            ["-columns=longCol", "-page=1", "-filenames"],
            "SDDSlib/demo/example.sdds \n100\n200\n300\n400\n500\n",
        ),
        (
            ["-columns=longCol", "-page=1", "-rows"],
            "5 rows\n100\n200\n300\n400\n500\n",
        ),
        (
            ["-columns=longCol", "-page=1", "-rows=total"],
            "5 rows\n100\n200\n300\n400\n500\n",
        ),
        (
            ["-columns=longCol", "-page=1", "-rows=bare"],
            "5\n100\n200\n300\n400\n500\n",
        ),
        (
            ["-columns=longCol", "-page=1", "-rows=scientific"],
            "5e+00 rows\n100\n200\n300\n400\n500\n",
        ),
        (["-npages"], "2 pages\n"),
        (["-npages=bare"], "2\n"),
        (["-description"], '"Example SDDS Output"\n"SDDS Example"\n'),
        (
            ["-description", "-noquotes"],
            "Example SDDS Output\nSDDS Example\n",
        ),
        (
            ["-columns=doubleCol", "-page=1", "-ignoreFormats"],
            "1.001000000000000e+01\n"
            "2.002000000000000e+01\n"
            "3.003000000000000e+01\n"
            "4.004000000000000e+01\n"
            "5.005000000000000e+01\n",
        ),
        (["-columns=longCol", "-table=1"], "100\n200\n300\n400\n500\n"),
    ],
)
def test_sdds2stream_options(args, expected):
    result = subprocess.run(
        [str(SDDS2STREAM), "SDDSlib/demo/example.sdds", *args],
        capture_output=True,
        text=True,
        check=True,
    )
    assert result.stdout == expected


@pytest.mark.skipif(not SDDS2STREAM.exists(), reason="sdds2stream not built")
def test_sdds2stream_pipe():
    data = Path("SDDSlib/demo/example.sdds").read_bytes()
    result = subprocess.run(
        [str(SDDS2STREAM), "-pipe", "-columns=longCol", "-page=1"],
        input=data,
        capture_output=True,
        check=True,
    )
    assert result.stdout.decode() == "100\n200\n300\n400\n500\n"


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




