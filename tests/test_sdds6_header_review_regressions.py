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


@pytest.mark.parametrize("compression,suffix", [("plain", ".sdds"), ("gzip", ".sdds.gz"),
                                                 ("xz", ".sdds.xz"), ("lzma", ".sdds.lzma")])
@pytest.mark.parametrize("mode", ["-ascii", "-binary"])
@pytest.mark.parametrize("description", ["state!", "a&endb", "line1\nline2", 'a"quoted"value',
                                         "path ending in a backslash\\", "x" * 4096])
def test_primitive_copy_preserves_sdds6_metadata(tmp_path, compression, suffix, mode, description):
    source, output = tmp_path / ("input" + suffix), tmp_path / ("copy" + suffix)
    write_header(source, 'SDDS6\n&column name=value,type=long,description="' + outer_string(description) +
                 '" &end\n&data mode=ascii &end\n1\n5\n', compression)
    run("sddsconvert", source, output, mode)
    summary = tmp_path / "summary.sdds"
    run("sddsquery", output, "-sddsOutput=" + str(summary), "-column")
    assert run("sdds2stream", summary, "-column=Description", "-noquotes") == description + "\n"
    assert run("sdds2stream", output, "-column=value").strip() == "5"


def test_safe_primitive_sdds6_metadata_still_uses_legacy_output(tmp_path):
    source, output = tmp_path / "input.sdds", tmp_path / "copy.sdds"
    description = r"C:\temp\new ! &end"
    write_header(source, 'SDDS6\n&column name=value,type=long,symbol="y\\\\_1",description="' +
                 outer_string(description) + '" &end\n&data mode=ascii &end\n1\n5\n', "plain")
    run("sddsconvert", source, output, "-ascii")
    assert output.read_bytes().startswith(b"SDDS1\n")
    summary = tmp_path / "summary.sdds"
    run("sddsquery", output, "-sddsOutput=" + str(summary), "-column")
    assert run("sdds2stream", summary, "-column=Description", "-noquotes") == description + "\n"
