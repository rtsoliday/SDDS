import subprocess
from pathlib import Path
from contextlib import nullcontext
import pytest


def run_prog(cmd, expected_stdout=None, *, capsys=None, prefix=None, **kwargs):
    """Run a command while reporting its status."""
    program = Path(cmd[0]).name
    message = prefix if prefix is not None else program
    out_context = capsys.disabled if capsys is not None else nullcontext
    if message:
        with out_context():
            print(message, end="", flush=True)

    # Ensure output is captured when validation is requested
    capture_needed = expected_stdout is not None and "stdout" not in kwargs and "capture_output" not in kwargs
    if capture_needed:
        kwargs["capture_output"] = True
        kwargs.setdefault("text", True)

    result = subprocess.run(cmd, **kwargs)
    try:
        result.check_returncode()
        if expected_stdout is not None:
            assert result.stdout.strip() == expected_stdout
    except Exception:
        with out_context():
            print(" failed")
        raise
    with out_context():
        print(" passed")
    return result

SDDSCHECK = Path("bin/Linux-x86_64/sddscheck")
SDDSCONVERT = Path("bin/Linux-x86_64/sddsconvert")

@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_ok(capsys):
    run_prog([str(SDDSCHECK), "SDDSlib/demo/example.sdds"], expected_stdout="ok", capsys=capsys)


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_print_errors(capsys):
    print("Testing sddscheck options:")
    run_prog([
        str(SDDSCHECK),
        "SDDSlib/demo/example.sdds",
        "-printErrors",
    ], expected_stdout="ok", capsys=capsys, prefix="  -printErrors")

@pytest.mark.skipif(not (SDDSCHECK.exists() and SDDSCONVERT.exists()), reason="tools not built")
def test_sddsconvert(tmp_path, capsys):
    binary_file = tmp_path / "binary.sdds"
    ascii_file = tmp_path / "ascii.sdds"

    run_prog([str(SDDSCONVERT), "SDDSlib/demo/example.sdds", str(binary_file), "-binary"], capsys=capsys)
    run_prog([str(SDDSCHECK), str(binary_file)], expected_stdout="ok", capsys=capsys)

    run_prog([str(SDDSCONVERT), str(binary_file), str(ascii_file), "-ascii"], capsys=capsys)
    run_prog([str(SDDSCHECK), str(ascii_file)], expected_stdout="ok", capsys=capsys)


@pytest.mark.skipif(not (SDDSCHECK.exists() and SDDSCONVERT.exists()), reason="tools not built")
def test_sddsconvert_options(tmp_path, capsys):
    options = [
        "-binary",
        "-ascii",
        "-delete=column,shortCol",
        "-retain=column,shortCol",
        "-rename=column,shortCol=shortCol2",
        "-description=test,contents",
        "-table=1",
        "-editnames=column,*Col,*New",
        "-linesperrow=1",
        "-nowarnings",
        "-recover",
        "-pipe=output",
        "-fromPage=1",
        "-toPage=1",
        "-acceptAllNames",
        "-removePages=1",
        "-keepPages=1",
        "-rowlimit=1",
        "-majorOrder=column",
        "-convertUnits=column,doubleCol,m",
    ]
    print("Testing sddsconvert options:")
    for opt in options:
        output = tmp_path / "out.sdds"
        cmd = [str(SDDSCONVERT), "SDDSlib/demo/example.sdds"]
        if opt.startswith("-pipe"):
            cmd.append(opt)
            with output.open("wb") as f:
                run_prog(cmd, stdout=f, capsys=capsys, prefix=f"  {opt}")
        else:
            cmd += [str(output), opt]
            run_prog(cmd, capsys=capsys, prefix=f"  {opt}")
        run_prog([str(SDDSCHECK), str(output)], expected_stdout="ok", prefix="")

