
import re
import subprocess
from pathlib import Path

import pytest

from sdds_test_utils import BIN_DIR
SDDSCHANGES = BIN_DIR / "sddschanges"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDSCHECK = BIN_DIR / "sddscheck"
PLAINDATA2SDDS = BIN_DIR / "plaindata2sdds"
SDDSCOMBINE = BIN_DIR / "sddscombine"
SDDS2STREAM = BIN_DIR / "sdds2stream"

SRC_FILE = Path("SDDSaps/sddschanges.c")


def extract_options():
  text = SRC_FILE.read_text()
  match = re.search(r'char \*option\[N_OPTIONS\] = \{([^}]*)\};', text, re.MULTILINE)
  return [item.strip().strip('"') for item in match.group(1).split(',') if item.strip()]


@pytest.mark.skipif(not SDDSCHANGES.exists(), reason="sddschanges not built")
def test_option_list():
  assert set(extract_options()) == {
    "copy",
    "changesin",
    "pass",
    "baseline",
    "pipe",
    "parallelpages",
    "keepempties",
    "majorOrder",
  }


def _numeric_lines(output):
  return [line.strip() for line in output.splitlines() if re.match(r'[+-]?\d', line.strip())]


@pytest.fixture
def sample_files(tmp_path):
  data1 = tmp_path / "data1.txt"
  data2 = tmp_path / "data2.txt"
  data1.write_text("1 10\n2 20\n3 30\n")
  data2.write_text("4 40\n5 50\n6 60\n")
  d1 = tmp_path / "data1.sdds"
  d2 = tmp_path / "data2.sdds"
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(data1),
    str(d1),
    "-noRowCount",
    "-column=dataCol,double",
    "-column=extraCol,double",
  ], check=True)
  subprocess.run([
    str(PLAINDATA2SDDS),
    str(data2),
    str(d2),
    "-noRowCount",
    "-column=dataCol,double",
    "-column=extraCol,double",
  ], check=True)
  input_file = tmp_path / "input.sdds"
  subprocess.run([str(SDDSCOMBINE), str(d1), str(d2), str(input_file)], check=True)
  baseline1 = tmp_path / "baseline1.sdds"
  baseline2 = tmp_path / "baseline2.sdds"
  baseline1.write_bytes(d1.read_bytes())
  subprocess.run([str(SDDSCOMBINE), str(d1), str(d1), str(baseline2)], check=True)
  return input_file, baseline1, baseline2


@pytest.mark.skipif(
  not all(p.exists() for p in [SDDSCHANGES, SDDSPRINTOUT, PLAINDATA2SDDS, SDDSCOMBINE]),
  reason="tools not built",
)
def test_changesin(sample_files, tmp_path):
  input_file, _, _ = sample_files
  output = tmp_path / "out.sdds"
  subprocess.run([str(SDDSCHANGES), str(input_file), str(output), "-changesIn=dataCol"], check=True)
  result = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-noTitle", "-columns=ChangeIndataCol"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert _numeric_lines(result.stdout) == ["3.000000e+00"] * 3


@pytest.mark.skipif(
  not all(p.exists() for p in [SDDSCHANGES, SDDSPRINTOUT, PLAINDATA2SDDS, SDDSCOMBINE]),
  reason="tools not built",
)
def test_copy(sample_files, tmp_path):
  input_file, _, _ = sample_files
  output = tmp_path / "out_copy.sdds"
  subprocess.run([
    str(SDDSCHANGES),
    str(input_file),
    str(output),
    "-changesIn=dataCol",
    "-copy=extraCol",
  ], check=True)
  result = subprocess.run([
    str(SDDSPRINTOUT),
    str(output),
    "-noTitle",
    "-columns=extraCol",
    "-columns=ChangeIndataCol",
  ], capture_output=True, text=True, check=True)
  lines = [line.split() for line in _numeric_lines(result.stdout)]
  assert [l[0] for l in lines] == ["1.000000e+01", "2.000000e+01", "3.000000e+01"]
  assert [l[1] for l in lines] == ["3.000000e+00"] * 3


@pytest.mark.skipif(
  not all(p.exists() for p in [SDDSCHANGES, SDDSPRINTOUT, PLAINDATA2SDDS, SDDSCOMBINE]),
  reason="tools not built",
)
def test_pass(sample_files, tmp_path):
  input_file, _, _ = sample_files
  output = tmp_path / "out_pass.sdds"
  subprocess.run([
    str(SDDSCHANGES),
    str(input_file),
    str(output),
    "-changesIn=dataCol",
    "-pass=extraCol",
  ], check=True)
  result = subprocess.run([
    str(SDDSPRINTOUT),
    str(output),
    "-noTitle",
    "-columns=extraCol",
    "-columns=ChangeIndataCol",
  ], capture_output=True, text=True, check=True)
  lines = [line.split() for line in _numeric_lines(result.stdout)]
  assert [l[0] for l in lines] == ["4.000000e+01", "5.000000e+01", "6.000000e+01"]
  assert [l[1] for l in lines] == ["3.000000e+00"] * 3


@pytest.mark.skipif(
  not all(p.exists() for p in [SDDSCHANGES, SDDSPRINTOUT, PLAINDATA2SDDS, SDDSCOMBINE]),
  reason="tools not built",
)
def test_baseline(sample_files, tmp_path):
  input_file, baseline1, _ = sample_files
  output = tmp_path / "out_baseline.sdds"
  subprocess.run([
    str(SDDSCHANGES),
    str(input_file),
    str(output),
    "-changesIn=dataCol",
    f"-baseline={baseline1}",
  ], check=True)
  result = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-noTitle", "-columns=ChangeIndataCol"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert _numeric_lines(result.stdout) == ["0.000000e+00"] * 3 + ["3.000000e+00"] * 3


@pytest.mark.skipif(
  not all(p.exists() for p in [SDDSCHANGES, SDDSPRINTOUT, PLAINDATA2SDDS, SDDSCOMBINE]),
  reason="tools not built",
)
def test_parallel_pages(sample_files, tmp_path):
  input_file, _, baseline2 = sample_files
  output = tmp_path / "out_parallel.sdds"
  subprocess.run([
    str(SDDSCHANGES),
    str(input_file),
    str(output),
    "-changesIn=dataCol",
    f"-baseline={baseline2}",
    "-parallelPages",
  ], check=True)
  result = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-noTitle", "-columns=ChangeIndataCol"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert _numeric_lines(result.stdout) == ["0.000000e+00"] * 3 + ["3.000000e+00"] * 3


@pytest.mark.skipif(
  not all(p.exists() for p in [SDDSCHANGES, SDDSPRINTOUT, PLAINDATA2SDDS, SDDSCOMBINE]),
  reason="tools not built",
)
def test_keep_empties(sample_files, tmp_path):
  input_file, _, _ = sample_files
  output = tmp_path / "out_keep.sdds"
  subprocess.run([
    str(SDDSCHANGES),
    str(input_file),
    str(output),
    "-changesIn=dataCol",
    "-keepEmpties",
  ], check=True)
  result = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-noTitle", "-columns=ChangeIndataCol"],
    capture_output=True,
    text=True,
    check=True,
  )
  assert _numeric_lines(result.stdout) == ["3.000000e+00"] * 3


@pytest.mark.skipif(
  not all(p.exists() for p in [SDDSCHANGES, SDDSCHECK, PLAINDATA2SDDS, SDDSCOMBINE]),
  reason="tools not built",
)
def test_major_order(sample_files, tmp_path):
  input_file, _, _ = sample_files
  output = tmp_path / "out_major.sdds"
  subprocess.run([
    str(SDDSCHANGES),
    str(input_file),
    str(output),
    "-changesIn=dataCol",
    "-majorOrder=column",
  ], check=True)
  check = subprocess.run([str(SDDSCHECK), str(output)], capture_output=True, text=True)
  assert check.stdout.strip() == "ok"


@pytest.mark.skipif(
  not all(p.exists() for p in [SDDSCHANGES, SDDSPRINTOUT, PLAINDATA2SDDS, SDDSCOMBINE]),
  reason="tools not built",
)
def test_pipe(sample_files):
  input_file, _, _ = sample_files
  proc = subprocess.run([
    str(SDDSCHANGES),
    str(input_file),
    "-pipe=out",
    "-changesIn=dataCol",
  ], stdout=subprocess.PIPE, check=True)
  result = subprocess.run(
    [str(SDDSPRINTOUT), "-pipe=in", "-noTitle", "-columns=ChangeIndataCol"],
    input=proc.stdout,
    stdout=subprocess.PIPE,
    check=True,
  )
  assert _numeric_lines(result.stdout.decode()) == ["3.000000e+00"] * 3


def _write_baseline_state(path, field_type="enum", mapping="0:OFF,1:ON", values=(0, 1)):
  enum_values = f', enum_values="{mapping}"' if field_type == "enum" else ""
  path.write_text(("SDDS6\n" if field_type == "enum" else "SDDS1\n") +
                  f'&column name=state, type={field_type}{enum_values} &end\n'
                  '&column name=x, type=double &end\n'
                  '&data mode=ascii &end\n' +
                  ''.join(f'1\n{value} {10 + page}\n' for page, value in enumerate(values)),
                  encoding="utf8")


def _changes_with_state_baseline(source, baseline, output, parallel_pages):
  return subprocess.run([str(SDDSCHANGES), str(source), str(output), "-changesIn=x",
                         "-copy=state", f"-baseline={baseline}"] +
                        (["-parallelPages"] if parallel_pages else []),
                        capture_output=True, text=True)


@pytest.mark.skipif(not all(p.exists() for p in [SDDSCHANGES, SDDS2STREAM]), reason="tools not built")
@pytest.mark.parametrize("parallel_pages", [False, True])
@pytest.mark.parametrize("mapping", ["0:OFF,1:ON", "1:ON,0:OFF"])
def test_baseline_copy_accepts_equal_enum_dictionaries(tmp_path, parallel_pages, mapping):
  source, baseline, output = [tmp_path / name for name in ("source.sdds", "baseline.sdds", "out.sdds")]
  _write_baseline_state(source, values=(1, 0))
  _write_baseline_state(baseline, mapping=mapping)
  result = _changes_with_state_baseline(source, baseline, output, parallel_pages)
  assert result.returncode == 0, result.stderr
  stream = subprocess.run([str(SDDS2STREAM), str(output), "-column=state", "-noquotes"],
                          capture_output=True, text=True, check=True)
  assert stream.stdout.split() == (["OFF", "ON"] if parallel_pages else ["OFF", "OFF"])


@pytest.mark.skipif(not SDDSCHANGES.exists(), reason="sddschanges not built")
@pytest.mark.parametrize("parallel_pages", [False, True])
@pytest.mark.parametrize("source_type,baseline_type,mapping,values", [
  ("enum", "enum", "0:ON,1:OFF", (0, 1)),
  ("enum", "enum", "7:OFF,8:ON", (7, 8)),
  ("enum", "enum", "0:OFF,1:ON,2:FAULT", (0, 1)),
  ("enum", "long", None, (0, 1)),
  ("long", "enum", "0:OFF,1:ON", (0, 1)),
])
def test_baseline_copy_rejects_incompatible_enum_definitions(tmp_path, parallel_pages,
                                                            source_type, baseline_type,
                                                            mapping, values):
  source, baseline, output = [tmp_path / name for name in ("source.sdds", "baseline.sdds", "out.sdds")]
  _write_baseline_state(source, field_type=source_type)
  _write_baseline_state(baseline, field_type=baseline_type, mapping=mapping, values=values)
  result = _changes_with_state_baseline(source, baseline, output, parallel_pages)
  assert result.returncode > 0
  assert "Incompatible enum mappings" in result.stderr


@pytest.mark.skipif(not all(p.exists() for p in [SDDSCHANGES, SDDS2STREAM]), reason="tools not built")
@pytest.mark.parametrize("parallel_pages", [False, True])
def test_baseline_copy_keeps_primitive_type_mixing(tmp_path, parallel_pages):
  source, baseline, output = [tmp_path / name for name in ("source.sdds", "baseline.sdds", "out.sdds")]
  _write_baseline_state(source, field_type="long")
  _write_baseline_state(baseline, field_type="ulong", values=(1, 2))
  result = _changes_with_state_baseline(source, baseline, output, parallel_pages)
  assert result.returncode == 0, result.stderr
  stream = subprocess.run([str(SDDS2STREAM), str(output), "-column=state", "-noquotes"],
                          capture_output=True, text=True, check=True)
  assert stream.stdout.split() == (["1", "2"] if parallel_pages else ["1", "1"])
