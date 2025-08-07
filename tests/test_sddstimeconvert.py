import subprocess
from pathlib import Path
import datetime
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSTIMECONVERT = BIN_DIR / "sddstimeconvert"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDS2STREAM = BIN_DIR / "sdds2stream"
SDDSCHECK = BIN_DIR / "sddscheck"

TOOLS = [SDDSTIMECONVERT, PLAINDATA2SDDS, SDDS2STREAM, SDDSCHECK]
ENV = {"TZ": "UTC"}


def _epoch(year, month, day, hour, minute=0, second=0):
    dt = datetime.datetime(year, month, day, hour, minute, second, tzinfo=datetime.timezone.utc)
    return dt.timestamp()


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_epoch_conversion(tmp_path):
    plain = tmp_path / "components.txt"
    plain.write_text("2023 3 1 0\n2024 12 31 12.5\n")
    input_file = tmp_path / "components.sdds"
    subprocess.run([
        str(PLAINDATA2SDDS),
        str(plain),
        str(input_file),
        "-column=Year,short",
        "-column=Month,short",
        "-column=Day,short",
        "-column=Hour,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
    ], check=True)
    output = tmp_path / "epoch.sdds"
    subprocess.run([
        str(SDDSTIMECONVERT),
        str(input_file),
        str(output),
        "-epoch=column,Epoch,year=Year,month=Month,day=Day,hour=Hour",
    ], check=True, env=ENV)
    result = subprocess.run([
        str(SDDS2STREAM),
        "-columns=Epoch",
        str(output),
    ], capture_output=True, text=True, check=True)
    values = [float(x) for x in result.stdout.strip().splitlines()]
    expected = [
        _epoch(2023, 3, 1, 0),
        _epoch(2024, 12, 31, 12, 30),
    ]
    assert values == pytest.approx(expected)
    subprocess.run([str(SDDSCHECK), str(output)], check=True, capture_output=True, text=True)


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_breakdown_conversion(tmp_path):
    epochs = [_epoch(2023, 3, 1, 0), _epoch(2024, 12, 31, 12, 30)]
    plain = tmp_path / "epoch.txt"
    plain.write_text("\n".join(str(int(e)) for e in epochs) + "\n")
    input_file = tmp_path / "epoch.sdds"
    subprocess.run([
        str(PLAINDATA2SDDS),
        str(plain),
        str(input_file),
        "-column=Epoch,double",
        "-inputMode=ascii",
        "-outputMode=ascii",
        "-noRowCount",
    ], check=True)
    output = tmp_path / "breakdown.sdds"
    subprocess.run([
        str(SDDSTIMECONVERT),
        str(input_file),
        str(output),
        "-breakdown=column,Epoch,year=Year,julianDay=JDay,month=Month,day=Day,hour=Hour,text=Text",
    ], check=True, env=ENV)
    result = subprocess.run([
        str(SDDS2STREAM),
        "-columns=Year,Month,Day,JDay,Hour,Text",
        str(output),
    ], capture_output=True, text=True, check=True)
    import shlex

    rows = [shlex.split(line.strip()) for line in result.stdout.strip().splitlines()]
    years = [int(r[0]) for r in rows]
    months = [int(r[1]) for r in rows]
    days = [int(r[2]) for r in rows]
    jdays = [int(r[3]) for r in rows]
    hours = [float(r[4]) for r in rows]
    texts = [r[5] for r in rows]
    assert years == [2023, 2024]
    assert months == [3, 12]
    assert days == [1, 31]
    assert jdays == [60, 366]
    assert hours == pytest.approx([0.0, 12.5])
    assert texts == [
        "2023/03/01 00:00:00.0000",
        "2024/12/31 12:30:00.0000",
    ]
    subprocess.run([str(SDDSCHECK), str(output)], check=True, capture_output=True, text=True)


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_date_to_time_conversion(tmp_path):
    input_file = tmp_path / "dates.sdds"
    input_file.write_text(
        "SDDS1\n"
        "&column name=Date, type=string &end\n"
        "&data mode=ascii &end\n"
        "! page 1\n"
        "2\n"
        "\"2023/03/01 00:00:00\"\n"
        "\"2024/12/31 12:30:00\"\n"
    )
    output = tmp_path / "dates_epoch.sdds"
    subprocess.run([
        str(SDDSTIMECONVERT),
        str(input_file),
        str(output),
        "-dateToTime=column,Epoch,Date,format=%Y/%m/%d %H:%M:%S",
    ], check=True, env=ENV)
    result = subprocess.run([
        str(SDDS2STREAM),
        "-columns=Epoch",
        str(output),
    ], capture_output=True, text=True, check=True)
    values = [float(x) for x in result.stdout.strip().splitlines()]
    expected = [_epoch(2023, 3, 1, 0), _epoch(2024, 12, 31, 12, 30)]
    assert values == pytest.approx(expected)
    subprocess.run([str(SDDSCHECK), str(output)], check=True, capture_output=True, text=True)


@pytest.mark.skipif(not all(tool.exists() for tool in TOOLS), reason="tools not built")
def test_parameter_epoch_conversion(tmp_path):
    input_file = tmp_path / "param.sdds"
    input_file.write_text(
        "SDDS1\n"
        "&parameter name=Year, type=short &end\n"
        "&parameter name=Month, type=short &end\n"
        "&parameter name=Day, type=short &end\n"
        "&parameter name=Hour, type=double &end\n"
        "&column name=dummy, type=double &end\n"
        "&data mode=ascii &end\n"
        "! page 1\n"
        "2023\n"
        "3\n"
        "1\n"
        "0\n"
        "1\n"
        "0\n"
    )
    output = tmp_path / "param_epoch.sdds"
    subprocess.run([
        str(SDDSTIMECONVERT),
        str(input_file),
        str(output),
        "-epoch=parameter,Epoch,year=Year,month=Month,day=Day,hour=Hour",
    ], check=True, env=ENV)
    result = subprocess.run([
        str(SDDS2STREAM),
        "-parameters=Epoch",
        str(output),
    ], capture_output=True, text=True, check=True)
    value = float(result.stdout.strip())
    assert value == pytest.approx(_epoch(2023, 3, 1, 0))
    subprocess.run([str(SDDSCHECK), str(output)], check=True, capture_output=True, text=True)
