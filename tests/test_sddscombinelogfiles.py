import subprocess
from pathlib import Path

def create_input(tmp_path, name, column, rows):
    plain = tmp_path / f"{name}.txt"
    plain.write_text("\n".join(f"{t} {v}" for t, v in rows) + "\n")
    sdds = tmp_path / f"{name}.sdds"
    bin_dir = Path(__file__).resolve().parents[1] / "bin" / "Linux-x86_64"
    subprocess.run(
        [bin_dir / "plaindata2sdds", plain, sdds, "-inputMode=ascii",
         "-column=Time,double", f"-column={column},double", "-noRowCount"],
        check=True,
    )
    return sdds


def sdds_print(file_path):
    bin_dir = Path(__file__).resolve().parents[1] / "bin" / "Linux-x86_64"
    result = subprocess.run(
        [bin_dir / "sddsprintout", file_path,
         "-columns=Time", "-columns=PV1", "-columns=PV2",
         "-noTitle", "-noLabels"],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout

EXPECTED_OUTPUT = (
    "  2.000000e+00   2.000000e+01   2.000000e+02 \n"
    "  3.000000e+00   3.000000e+01   3.000000e+02 \n"
)


def test_overwrite_option(tmp_path):
    in1 = create_input(tmp_path, "in1", "PV1", [(1, 10), (2, 20), (3, 30)])
    in2 = create_input(tmp_path, "in2", "PV2", [(2, 200), (3, 300), (4, 400)])
    output = tmp_path / "out.sdds"
    output.write_text("existing")
    bin_dir = Path(__file__).resolve().parents[1] / "bin" / "Linux-x86_64"
    subprocess.run(
        [bin_dir / "sddscombinelogfiles", in1, in2, output, "-overwrite"],
        check=True,
    )
    assert sdds_print(output) == EXPECTED_OUTPUT


def test_pipe_option(tmp_path):
    in1 = create_input(tmp_path, "in1", "PV1", [(1, 10), (2, 20), (3, 30)])
    in2 = create_input(tmp_path, "in2", "PV2", [(2, 200), (3, 300), (4, 400)])
    bin_dir = Path(__file__).resolve().parents[1] / "bin" / "Linux-x86_64"
    result = subprocess.run(
        [bin_dir / "sddscombinelogfiles", in1, in2, "-pipe=out"],
        check=True,
        stdout=subprocess.PIPE,
    )
    pipe_file = tmp_path / "pipe.sdds"
    pipe_file.write_bytes(result.stdout)
    assert sdds_print(pipe_file) == EXPECTED_OUTPUT
