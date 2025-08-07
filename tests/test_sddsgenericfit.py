import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSGENERICFIT = BIN_DIR / "sddsgenericfit"
SDDSQUERY = BIN_DIR / "sddsquery"


def column_list(path):
    result = subprocess.run(
        [str(SDDSQUERY), "-columnlist", str(path)],
        capture_output=True,
        text=True,
        check=True,
    )
    return [line for line in result.stdout.splitlines() if line]


def parameter_list(path):
    result = subprocess.run(
        [str(SDDSQUERY), "-parameterlist", str(path)],
        capture_output=True,
        text=True,
        check=True,
    )
    return [line for line in result.stdout.splitlines() if line]


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_basic_ycolumn_and_variable(tmp_path):
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
        ],
        check=True,
    )
    assert column_list(output) == ["doubleCol", "doubleColResidual", "doubleColFit"]
    assert "a" in parameter_list(output)


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_target_option(tmp_path):
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
            "-target=0.1",
        ],
        check=True,
    )
    assert column_list(output) == ["doubleCol", "doubleColResidual", "doubleColFit"]


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_tolerance_option(tmp_path):
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
            "-tolerance=1e-3",
        ],
        check=True,
    )
    assert column_list(output) == ["doubleCol", "doubleColResidual", "doubleColFit"]


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_simplex_option(tmp_path):
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
            "-simplex=restarts=1,cycles=1,evaluations=10",
        ],
        check=True,
    )
    assert column_list(output) == ["doubleCol", "doubleColResidual", "doubleColFit"]


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_verbosity_option(tmp_path):
    output = tmp_path / "out.sdds"
    result = subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
            "-verbosity=1",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    assert "Result of simplex minimization" in result.stderr


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_startfromprevious_option(tmp_path):
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
            "-startFromPrevious",
        ],
        check=True,
    )
    assert column_list(output) == ["doubleCol", "doubleColResidual", "doubleColFit"]


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_expression_option(tmp_path):
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
            "-expression=longCol doubleCol /,ratio",
        ],
        check=True,
    )
    assert column_list(output) == ["doubleCol", "doubleColResidual", "doubleColFit", "ratio"]


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_copycolumns_option(tmp_path):
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
            "-copycolumns=longCol",
        ],
        check=True,
    )
    assert column_list(output) == ["doubleCol", "longCol", "doubleColResidual", "doubleColFit"]


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_logfile_option(tmp_path):
    output = tmp_path / "out.sdds"
    log = tmp_path / "fit.log"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
            f"-logFile={log},1",
        ],
        check=True,
    )
    assert log.exists()


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_major_order_option(tmp_path):
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-ycolumn=doubleCol",
            "-majorOrder=row",
        ],
        check=True,
    )
    assert column_list(output) == ["doubleCol", "doubleColResidual", "doubleColFit"]


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_pipe_option(tmp_path):
    output = tmp_path / "out.sdds"
    with output.open("wb") as out:
        subprocess.run(
            [
                str(SDDSGENERICFIT),
                "SDDSlib/demo/example.sdds",
                "-pipe=out",
                "-equation=doubleCol",
                "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
                "-ycolumn=doubleCol",
            ],
            stdout=out,
            check=True,
        )
    assert column_list(output) == ["doubleCol", "doubleColResidual", "doubleColFit"]


@pytest.mark.skipif(not SDDSGENERICFIT.exists(), reason="sddsgenericfit not built")
@pytest.mark.skipif(not SDDSQUERY.exists(), reason="sddsquery not built")
def test_columns_option(tmp_path):
    output = tmp_path / "out.sdds"
    subprocess.run(
        [
            str(SDDSGENERICFIT),
            "SDDSlib/demo/example.sdds",
            str(output),
            "-equation=doubleCol",
            "-variable=name=a,lowerLimit=0,upperLimit=1,stepsize=0.1,startingValue=0.1",
            "-columns=longCol,doubleCol",
        ],
        check=True,
    )
    assert column_list(output) == ["doubleCol", "longCol", "doubleColResidual", "doubleColFit"]
