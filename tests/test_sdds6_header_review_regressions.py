"""SDDS6 header parser failures must not become successful include-file EOF."""

import gzip
import lzma
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


def write_header(path, text, compression):
    wire = text.encode()
    if compression == "gzip":
        wire = gzip.compress(wire)
    elif compression in ("xz", "lzma"):
        wire = lzma.compress(wire, format=lzma.FORMAT_ALONE if compression == "lzma" else lzma.FORMAT_XZ)
    path.write_bytes(wire)


def query(path):
    tool = BIN_DIR / "sddsquery"
    if not tool.exists():
        pytest.skip("sddsquery not built")
    return subprocess.run([str(tool), str(path), "-columnlist"],
                          capture_output=True, text=True, timeout=30)


def run(tool, *args):
    if not (BIN_DIR / tool).exists():
        pytest.skip(f"{tool} not built")
    result = subprocess.run([str(BIN_DIR / tool), *map(str, args)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    return result.stdout


def outer_string(text):
    return (text.replace("\\", "\\\\").replace('"', '\\"').replace("!", "\\!")
            .replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t"))


@pytest.mark.parametrize("compression,suffix", [("plain", ".sdds"), ("gzip", ".sdds.gz"),
                                                 ("xz", ".sdds.xz"), ("lzma", ".sdds.lzma")])
@pytest.mark.parametrize("ending", ["", " &en", ', description="unfinished'])
def test_unterminated_sdds6_include_is_an_error(tmp_path, compression, suffix, ending):
    included = tmp_path / "definitions.inc"
    included.write_text('&column name=state,type=enum,enum_values="10:ON"' + ending)
    source = tmp_path / ("input" + suffix)
    write_header(source, f'SDDS6\n&include filename="{included.as_posix()}" &end\n'
                 '&column name=value,type=long &end\n&data mode=ascii &end\n1\n5\n', compression)
    result = query(source)
    assert result.returncode != 0, result.stdout + result.stderr
    assert "Unterminated SDDS6 namelist" in result.stderr


@pytest.mark.parametrize("version", [1, 5, 6])
@pytest.mark.parametrize("compression,suffix", [("plain", ".sdds"), ("gzip", ".sdds.gz"),
                                                 ("xz", ".sdds.xz"), ("lzma", ".sdds.lzma")])
def test_complete_nested_include_still_accepts_eof(tmp_path, version, compression, suffix):
    inner = tmp_path / "inner.inc"
    inner.write_text('&column name=value,type=long &end\n! normal trailing comment\n')
    outer = tmp_path / "outer.inc"
    outer.write_text(f'&include filename="{inner.as_posix()}" &end\n')
    source = tmp_path / ("input" + suffix)
    write_header(source, f'SDDS{version}\n&include filename="{outer.as_posix()}" &end\n'
                 '&data mode=ascii &end\n1\n5\n', compression)
    result = query(source)
    assert result.returncode == 0, result.stdout + result.stderr
    assert result.stdout.split() == ["value"]


def column_description(path, tmp_path):
    summary = tmp_path / "summary.sdds"
    run("sddsquery", path, "-sddsOutput=" + str(summary), "-column")
    return run("sdds2stream", summary, "-column=Description", "-noquotes")


@pytest.mark.parametrize("compression,suffix", [("plain", ".sdds"), ("gzip", ".sdds.gz"),
                                                 ("xz", ".sdds.xz"), ("lzma", ".sdds.lzma")])
@pytest.mark.parametrize("mode", ["-ascii", "-binary"])
@pytest.mark.parametrize("description", ["state!", "a&endb", "1/m!x", "x&end", r"C:\temp\new ! &end",
                                         "plain text"])
def test_primitive_copy_uses_legacy_header_and_keeps_metadata(tmp_path, compression, suffix, mode, description):
    source, output = tmp_path / ("input" + suffix), tmp_path / ("copy" + suffix)
    write_header(source, 'SDDS6\n&column name=value,type=long,description="' + outer_string(description) +
                 '" &end\n&data mode=ascii &end\n1\n5\n', compression)
    run("sddsconvert", source, output, mode)
    with (gzip.open if compression == "gzip" else lzma.open if compression in ("xz", "lzma") else open)(output, "rb") as stream:
        assert stream.readline() == b"SDDS1\n"
    assert column_description(output, tmp_path) == description + "\n"
    assert run("sdds2stream", output, "-column=value").strip() == "5"


@pytest.mark.parametrize("description", ['a"quoted"value', "line1\nline2", "path ending in a backslash\\",
                                         "x" * 4096, " "])
def test_enum_free_output_never_selects_sdds6(tmp_path, description):
    source, output = tmp_path / "input.sdds", tmp_path / "copy.sdds"
    write_header(source, 'SDDS6\n&column name=value,type=long,description="' + outer_string(description) +
                 '" &end\n&data mode=ascii &end\n1\n5\n', "plain")
    run("sddsconvert", source, output, "-ascii")
    assert output.read_bytes().startswith(b"SDDS1\n")


@pytest.mark.parametrize("units", ['in"', "1/m!", "a&endb"])
def test_new_enum_free_files_stay_legacy(tmp_path, units):
    output = tmp_path / "made.sdds"
    run("sddsmakedataset", output, f"-column=x,type=double,units={units}", "-data=1,2")
    assert output.read_bytes().startswith(b"SDDS1\n")
