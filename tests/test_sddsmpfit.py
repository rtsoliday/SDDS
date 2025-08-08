import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSMPFIT = BIN_DIR / "sddsmpfit"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

TOOLS = [SDDSMPFIT, PLAINDATA2SDDS, SDDSPRINTOUT]

def require_tools():
    return all(tool.exists() for tool in TOOLS)


def make_data(tmp_path):
    plain = tmp_path / "data.txt"
    plain.write_text("0 1\n1 3\n2 5\n3 7\n")
    sdds_path = tmp_path / "data.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(sdds_path),
            "-column=x,double",
            "-column=y,double",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    return sdds_path


def read_column(path, column):
    result = subprocess.run(
        [
            str(SDDSPRINTOUT),
            str(path),
            f"-columns={column}",
            "-noLabel",
            "-noTitle",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    return [float(value) for value in result.stdout.split()]


def read_parameter(path, param):
    result = subprocess.run(
        [
            str(SDDSPRINTOUT),
            str(path),
            f"-parameters={param}",
            "-noLabel",
            "-noTitle",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    return result.stdout.strip()


@pytest.mark.skipif(not require_tools(), reason="required tools not built")
def test_terms_option(tmp_path):
    data = make_data(tmp_path)
    output = tmp_path / "out.sdds"
    info = tmp_path / "info.sdds"
    subprocess.run(
        [
            str(SDDSMPFIT),
            str(data),
            str(output),
            "-terms=2",
            "-independent=x",
            "-dependent=y",
            f"-infoFile={info}",
        ],
        check=True,
    )
    coeffs = read_column(info, "yCoefficient")
    assert abs(coeffs[0] - 1.0) < 1e-12
    assert abs(coeffs[1] - 2.0) < 1e-12
    residuals = read_column(output, "yResidual")
    assert all(abs(r) < 1e-12 for r in residuals)


@pytest.mark.skipif(not require_tools(), reason="required tools not built")
def test_orders_option(tmp_path):
    data = make_data(tmp_path)
    output = tmp_path / "out.sdds"
    info = tmp_path / "info.sdds"
    subprocess.run(
        [
            str(SDDSMPFIT),
            str(data),
            str(output),
            "-orders=0,1",
            "-independent=x",
            "-dependent=y",
            f"-infoFile={info}",
        ],
        check=True,
    )
    coeffs = read_column(info, "yCoefficient")
    assert abs(coeffs[0] - 1.0) < 1e-12
    assert abs(coeffs[1] - 2.0) < 1e-12
    residuals = read_column(output, "yResidual")
    assert all(abs(r) < 1e-12 for r in residuals)


@pytest.mark.skipif(not require_tools(), reason="required tools not built")
def test_chebyshev_option(tmp_path):
    data = make_data(tmp_path)
    output = tmp_path / "out.sdds"
    info = tmp_path / "info.sdds"
    subprocess.run(
        [
            str(SDDSMPFIT),
            str(data),
            str(output),
            "-terms=2",
            "-independent=x",
            "-dependent=y",
            "-chebyshev=convert",
            f"-infoFile={info}",
        ],
        check=True,
    )
    coeffs = read_column(info, "yCoefficient")
    assert abs(coeffs[0] - 4.0) < 1e-12
    assert abs(coeffs[1] - 3.0) < 1e-12
    assert read_parameter(info, "Basis") == "Chebyshev T polynomials"
    residuals = read_column(output, "yResidual")
    assert all(abs(r) < 1e-12 for r in residuals)
