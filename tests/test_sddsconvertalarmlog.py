import subprocess
import pytest

from sdds_test_utils import BIN_DIR
PROGRAM = BIN_DIR / "sddsconvertalarmlog"
SDDSCHECK = BIN_DIR / "sddscheck"
SDDSQUERY = BIN_DIR / "sddsquery"
SDDS2STREAM = BIN_DIR / "sdds2stream"

ALARM_LOG = """SDDS1
&parameter name=StartTime, type=double, units=s, &end
&parameter name=StartHour, type=float, &end
&array name=ControlNameString, type=string, &end
&array name=AlarmStatusString, type=string, &end
&array name=AlarmSeverityString, type=string, &end
&column name=PreviousRow, type=long, &end
&column name=TimeOfDayWS, type=float, units=h, &end
&column name=TimeOfDay, type=float, units=h, &end
&column name=ControlNameIndex, type=long, &end
&column name=AlarmStatusIndex, type=short, &end
&column name=AlarmSeverityIndex, type=short, &end
&data mode=ascii, &end
1000
0
2
PV1 PV2
3
NO_ALARM READ WRITE
3
NO_ALARM MINOR MAJOR
2
-1 0.0 0.0 0 0 0
0 0.5 0.5 1 2 2
"""

@pytest.mark.skipif(
    not (PROGRAM.exists() and SDDSCHECK.exists() and SDDSQUERY.exists() and SDDS2STREAM.exists()),
    reason="tools not built",
)
def test_converts_indexed_alarm_log_to_explicit_columns(tmp_path):
    input_file = tmp_path / "alarm.sdds"
    output_file = tmp_path / "converted.sdds"
    input_file.write_text(ALARM_LOG)

    subprocess.run([str(PROGRAM), str(input_file), str(output_file), "-ascii"], check=True)

    result = subprocess.run([str(SDDSCHECK), str(output_file)], capture_output=True, text=True)
    assert result.stdout.strip() == "ok"

    arrays = subprocess.run([str(SDDSQUERY), str(output_file), "-arrayList"], capture_output=True, text=True)
    assert arrays.stdout.strip() == ""

    values = subprocess.run(
        [
            str(SDDS2STREAM),
            str(output_file),
            "-columns=Time,ControlName,AlarmStatus,AlarmSeverity",
            "-delimiter=,",
            "-noquotes",
        ],
        capture_output=True,
        text=True,
    )
    rows = [line.split() for line in values.stdout.splitlines()]
    assert rows == [
        ["1.000000000000000e+03", "PV1", "NO_ALARM", "NO_ALARM"],
        ["2.800000000000000e+03", "PV2", "WRITE", "MAJOR"],
    ]


@pytest.mark.skipif(
    not (PROGRAM.exists() and SDDSCHECK.exists() and SDDSQUERY.exists() and SDDS2STREAM.exists()),
    reason="tools not built",
)
def test_time_window_and_retain_option(tmp_path):
    input_file = tmp_path / "alarm.sdds"
    output_file = tmp_path / "filtered.sdds"
    input_file.write_text(ALARM_LOG)
    subprocess.run(
        [
            str(PROGRAM),
            str(input_file),
            str(output_file),
            "-ascii",
            "-time=start=2000,end=3000",
            "-retain=Time,ControlName,AlarmSeverity",
        ],
        check=True,
    )
    result = subprocess.run([str(SDDSQUERY), str(output_file), "-columnlist"], capture_output=True, text=True, check=True)
    assert "ControlName" in result.stdout
    assert "AlarmSeverity" in result.stdout
    assert "AlarmStatus" not in result.stdout


@pytest.mark.skipif(
    not (PROGRAM.exists() and SDDSCHECK.exists() and SDDSQUERY.exists() and SDDS2STREAM.exists()),
    reason="tools not built",
)
def test_snapshot_and_pipe_output(tmp_path):
    input_file = tmp_path / "alarm.sdds"
    input_file.write_text(ALARM_LOG)
    result = subprocess.run(
        [str(PROGRAM), str(input_file), "-snapshot=3000", "-pipe=out", "-double"],
        stdout=subprocess.PIPE,
        check=True,
    )
    output_file = tmp_path / "snapshot.sdds"
    output_file.write_bytes(result.stdout)
    check = subprocess.run([str(SDDSCHECK), str(output_file)], capture_output=True, text=True, check=True)
    assert check.stdout.strip() == "ok"
