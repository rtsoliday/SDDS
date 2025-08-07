import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSSORT = BIN_DIR / "sddssort"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDSPROCESS = BIN_DIR / "sddsprocess"
SDDSCOMBINE = BIN_DIR / "sddscombine"
SDDSCHECK = BIN_DIR / "sddscheck"

TOOLS = [
    SDDSSORT,
    PLAINDATA2SDDS,
    SDDSPRINTOUT,
    SDDSPROCESS,
    SDDSCOMBINE,
    SDDSCHECK,
]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_column_sorting(tmp_path):
    plain = tmp_path / "numbers.txt"
    plain.write_text("1\n-2\n1\n3\n-4\n")
    input_file = tmp_path / "numbers.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=a,long",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    cases = [
        (["-column=a"], ["-4", "-2", "1", "1", "3"]),
        (["-column=a,decreasing"], ["3", "1", "1", "-2", "-4"]),
        (["-column=a,absolute"], ["1", "1", "-2", "3", "-4"]),
    ]
    for args, expected in cases:
        output = tmp_path / f"sort_{args[0].split('=')[1].replace(',', '_')}.sdds"
        subprocess.run([str(SDDSSORT), str(input_file), str(output), *args], check=True)
        result = subprocess.run(
            [
                str(SDDSPRINTOUT),
                str(output),
                "-column=a",
                "-noTitle",
                "-noLabels",
                "-spreadsheet=csv",
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        values = [line.strip().strip('"') for line in result.stdout.strip().splitlines()]
        assert values == expected


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_unique_options(tmp_path):
    plain = tmp_path / "numbers.txt"
    plain.write_text("1\n-2\n1\n3\n-4\n")
    input_file = tmp_path / "numbers.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=a,long",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    unique = tmp_path / "unique.sdds"
    subprocess.run(
        [str(SDDSSORT), str(input_file), str(unique), "-column=a", "-unique", "-noWarnings"],
        check=True,
    )
    result = subprocess.run(
        [
            str(SDDSPRINTOUT),
            str(unique),
            "-column=a",
            "-noTitle",
            "-noLabels",
            "-spreadsheet=csv",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    values = [line.strip().strip('"') for line in result.stdout.strip().splitlines()]
    assert values == ["-4", "-2", "1", "3"]

    count_file = tmp_path / "unique_count.sdds"
    subprocess.run(
        [str(SDDSSORT), str(input_file), str(count_file), "-column=a", "-unique=count"],
        check=True,
    )
    result = subprocess.run(
        [
            str(SDDSPRINTOUT),
            str(count_file),
            "-columns=a",
            "-columns=IdenticalCount",
            "-noTitle",
            "-noLabels",
            "-spreadsheet=csv",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    rows = [line.strip().replace('"', '').split(',') for line in result.stdout.strip().splitlines()]
    assert rows == [["-4", "1"], ["-2", "1"], ["1", "2"], ["3", "1"]]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_parameter_sort(tmp_path):
    plain = tmp_path / "numbers.txt"
    plain.write_text("1\n-2\n1\n3\n-4\n")
    base = tmp_path / "base.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(base),
            "-column=a,long",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    page1 = tmp_path / "p1.sdds"
    page2 = tmp_path / "p2.sdds"
    subprocess.run(
        [str(SDDSPROCESS), str(base), str(page1), "-define=parameter,p,2"],
        check=True,
    )
    subprocess.run(
        [str(SDDSPROCESS), str(base), str(page2), "-define=parameter,p,1"],
        check=True,
    )
    combined = tmp_path / "combined.sdds"
    subprocess.run(
        [str(SDDSCOMBINE), str(page1), str(page2), str(combined), "-overWrite"],
        check=True,
    )
    sorted_file = tmp_path / "sorted_param.sdds"
    subprocess.run(
        [str(SDDSSORT), str(combined), str(sorted_file), "-parameter=p"],
        check=True,
    )
    result = subprocess.run(
        [
            str(SDDSPRINTOUT),
            str(sorted_file),
            "-parameters=p",
            "-noTitle",
            "-noLabels",
            "-spreadsheet=csv",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    values = [float(line.split(",")[1].strip().strip('"')) for line in result.stdout.strip().splitlines()]
    assert values == [1.0, 2.0]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_numeric_high(tmp_path):
    plain = tmp_path / "strings.txt"
    plain.write_text("10\n2\nA\nB\n")
    input_file = tmp_path / "strings.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=name,string",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    output = tmp_path / "strings_sorted.sdds"
    subprocess.run(
        [str(SDDSSORT), str(input_file), str(output), "-column=name", "-numericHigh"],
        check=True,
    )
    result = subprocess.run(
        [
            str(SDDSPRINTOUT),
            str(output),
            "-column=name",
            "-noTitle",
            "-noLabels",
            "-spreadsheet=csv",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    values = [line.strip().strip('"') for line in result.stdout.strip().splitlines()]
    assert values == ["A", "B", "2", "10"]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_non_dominate_sort(tmp_path):
    plain = tmp_path / "pareto.txt"
    plain.write_text("1 1\n2 2\n3 3\n")
    input_file = tmp_path / "pareto.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=x,double",
            "-column=y,double",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    output = tmp_path / "pareto_sorted.sdds"
    subprocess.run(
        [
            str(SDDSSORT),
            str(input_file),
            str(output),
            "-column=x,minimize",
            "-column=y,minimize",
            "-nonDominateSort",
        ],
        check=True,
    )
    result = subprocess.run(
        [
            str(SDDSPRINTOUT),
            str(output),
            "-columns=x",
            "-columns=y",
            "-columns=Rank",
            "-noTitle",
            "-noLabels",
            "-spreadsheet=csv",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    ranks = [int(row.split(",")[-1].strip().strip('"')) for row in result.stdout.strip().splitlines()]
    assert ranks == [1, 2, 3]


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_pipe_usage(tmp_path):
    plain = tmp_path / "numbers.txt"
    plain.write_text("1\n-2\n1\n3\n-4\n")
    input_file = tmp_path / "numbers.sdds"
    subprocess.run(
        [
            str(PLAINDATA2SDDS),
            str(plain),
            str(input_file),
            "-column=a,long",
            "-inputMode=ascii",
            "-outputMode=ascii",
            "-noRowCount",
        ],
        check=True,
    )
    out_pipe = tmp_path / "pipe_out.sdds"
    with open(out_pipe, "wb") as f:
        subprocess.run([str(SDDSSORT), str(input_file), "-column=a", "-pipe=out"], stdout=f, check=True)
    with open(out_pipe, "rb") as f:
        result = subprocess.run(
            [str(SDDSPRINTOUT), "-pipe", "-column=a", "-noTitle", "-noLabels", "-spreadsheet=csv"],
            stdin=f,
            capture_output=True,
            text=True,
            check=True,
        )
    values = [line.strip().strip('"') for line in result.stdout.strip().splitlines()]
    assert values == ["-4", "-2", "1", "1", "3"]

    out_in = tmp_path / "pipe_in.sdds"
    with open(input_file, "rb") as fin:
        subprocess.run(
            [str(SDDSSORT), "-pipe=in", str(out_in), "-column=a"],
            stdin=fin,
            check=True,
        )
    result = subprocess.run(
        [
            str(SDDSPRINTOUT),
            str(out_in),
            "-column=a",
            "-noTitle",
            "-noLabels",
            "-spreadsheet=csv",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    values = [line.strip().strip('"') for line in result.stdout.strip().splitlines()]
    assert values == ["-4", "-2", "1", "1", "3"]
