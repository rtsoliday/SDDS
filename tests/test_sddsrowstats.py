import subprocess
import csv
from pathlib import Path
import math
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSROWSTATS = BIN_DIR / "sddsrowstats"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"

TOOLS = [PLAINDATA2SDDS, SDDSROWSTATS, SDDSPRINTOUT]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_statistics_options(tmp_path):
    plain = tmp_path / "data.txt"
    plain.write_text(
        "1 2 3 4 5\n" "5 4 3 2 1\n" "-5 -4 -3 -2 -1\n" "-1 0 1 2 3\n"
    )
    input_file = tmp_path / "data.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=c1,double",
            "-column=c2,double",
            "-column=c3,double",
            "-column=c4,double",
            "-column=c5,double",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    output_file = tmp_path / "stats.sdds"
    subprocess.run(
        [
            str(SDDSROWSTATS),
            str(input_file),
            str(output_file),
            "-mean=Mean,c1,c2,c3,c4,c5",
            "-rms=RMS,c1,c2,c3,c4,c5",
            "-median=Median,c1,c2,c3,c4,c5",
            "-minimum=Min,c1,c2,c3,c4,c5",
            "-maximum=Max,c1,c2,c3,c4,c5",
            "-standardDeviation=Std,c1,c2,c3,c4,c5",
            "-sigma=Sigma,c1,c2,c3,c4,c5",
            "-mad=MAD,c1,c2,c3,c4,c5",
            "-sum=Sum,c1,c2,c3,c4,c5",
            "-count=Count,c1,c2,c3,c4,c5",
            "-drange=Drange,c1,c2,c3,c4,c5",
            "-qrange=Qrange,c1,c2,c3,c4,c5",
            "-smallest=Smallest,c1,c2,c3,c4,c5",
            "-largest=Largest,c1,c2,c3,c4,c5",
            "-spread=Spread,c1,c2,c3,c4,c5",
            "-percentile=Percent90,value=90,c1,c2,c3,c4,c5",
        ],
        check=True,
    )
    result = subprocess.run(
        [
            str(SDDSPRINTOUT),
            str(output_file),
            "-columns=Mean",
            "-columns=RMS",
            "-columns=Median",
            "-columns=Min",
            "-columns=Max",
            "-columns=Std",
            "-columns=Sigma",
            "-columns=MAD",
            "-columns=Sum",
            "-columns=Count",
            "-columns=Drange",
            "-columns=Qrange",
            "-columns=Smallest",
            "-columns=Largest",
            "-columns=Spread",
            "-columns=Percent90",
            "-noTitle",
            "-noLabels",
            "-spreadsheet=csv",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    reader = csv.reader(result.stdout.strip().splitlines())
    values = [list(map(float, row)) for row in reader]

    data = [
        [1, 2, 3, 4, 5],
        [5, 4, 3, 2, 1],
        [-5, -4, -3, -2, -1],
        [-1, 0, 1, 2, 3],
    ]

    def expected(row):
        n = len(row)
        mean = sum(row) / n
        rms = math.sqrt(sum(x * x for x in row) / n)
        median = sorted(row)[n // 2]
        minimum = min(row)
        maximum = max(row)
        value2 = sum(x * x for x in row)
        variance = value2 / n - mean * mean
        if variance <= 0:
            std = sigma = 0.0
        else:
            std = math.sqrt(variance * n / (n - 1))
            sigma = math.sqrt(variance / (n - 1))
        mad = sum(abs(x - mean) for x in row) / n
        total = sum(row)
        count = float(n)
        sorted_row = sorted(row)
        drange = (
            sorted_row[int((n - 1) * 0.90)] - sorted_row[int((n - 1) * 0.10)]
        )
        qrange = (
            sorted_row[int((n - 1) * 0.75)] - sorted_row[int((n - 1) * 0.25)]
        )
        smallest = min(abs(x) for x in row)
        largest = max(abs(x) for x in row)
        spread = maximum - minimum
        percentile90 = sorted_row[int((n - 1) * 0.90)]
        return [
            mean,
            rms,
            median,
            minimum,
            maximum,
            std,
            sigma,
            mad,
            total,
            count,
            drange,
            qrange,
            smallest,
            largest,
            spread,
            percentile90,
        ]

    expected_values = [expected(row) for row in data]
    assert len(values) == len(expected_values)
    for result_row, expected_row in zip(values, expected_values):
        assert len(result_row) == len(expected_row)
        for value, exp in zip(result_row, expected_row):
            assert value == pytest.approx(exp, rel=1e-6, abs=1e-6)
