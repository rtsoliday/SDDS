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


BIN_DIR = Path(__file__).resolve().parents[1] / "bin" / "Linux-x86_64"


def sdds_print(file_path, columns):
    args = [BIN_DIR / "sddsprintout", file_path]
    for column in columns:
        args.append(f"-columns={column}")
    args += ["-noTitle", "-noLabels"]
    result = subprocess.run(
        args,
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
    subprocess.run(
        [BIN_DIR / "sddscombinelogfiles", in1, in2, output, "-overwrite"],
        check=True,
    )
    assert sdds_print(output, ["Time", "PV1", "PV2"]) == EXPECTED_OUTPUT


def test_pipe_option(tmp_path):
    in1 = create_input(tmp_path, "in1", "PV1", [(1, 10), (2, 20), (3, 30)])
    in2 = create_input(tmp_path, "in2", "PV2", [(2, 200), (3, 300), (4, 400)])
    result = subprocess.run(
        [BIN_DIR / "sddscombinelogfiles", in1, in2, "-pipe=out"],
        check=True,
        stdout=subprocess.PIPE,
    )
    pipe_file = tmp_path / "pipe.sdds"
    pipe_file.write_bytes(result.stdout)
    assert sdds_print(pipe_file, ["Time", "PV1", "PV2"]) == EXPECTED_OUTPUT


def test_single_pv_concatenates_pages(tmp_path):
    in1 = create_input(tmp_path, "single", "PV1", [(1, 10), (2, 20), (3, 30)])
    output = tmp_path / "single-out.sdds"
    subprocess.run(
        [BIN_DIR / "sddscombinelogfiles", in1, output, "-overwrite"],
        check=True,
    )
    assert sdds_print(output, ["Time", "PV1"]) == (
        "  1.000000e+00   1.000000e+01 \n"
        "  2.000000e+00   2.000000e+01 \n"
        "  3.000000e+00   3.000000e+01 \n"
    )


def test_three_file_common_timestamps(tmp_path):
    in1 = create_input(tmp_path, "in1", "PV1", [(1, 10), (2, 20), (3, 30), (4, 40)])
    in2 = create_input(tmp_path, "in2", "PV2", [(2, 200), (3, 300), (4, 400)])
    in3 = create_input(tmp_path, "in3", "PV3", [(3, 3000), (4, 4000), (5, 5000)])
    output = tmp_path / "three.sdds"
    subprocess.run(
        [BIN_DIR / "sddscombinelogfiles", in1, in2, in3, output, "-overwrite"],
        check=True,
    )
    assert sdds_print(output, ["Time", "PV1", "PV2", "PV3"]) == (
        "  3.000000e+00   3.000000e+01   3.000000e+02   3.000000e+03 \n"
        "  4.000000e+00   4.000000e+01   4.000000e+02   4.000000e+03 \n"
    )
