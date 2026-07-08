import subprocess
import shutil
from pathlib import Path
import pytest

from sdds_test_utils import BIN_DIR
SDDSCHECK = BIN_DIR / "sddscheck"
EXAMPLE = Path("SDDSlib/demo/example.sdds")

@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_ok():
  result = subprocess.run([str(SDDSCHECK), str(EXAMPLE)], capture_output=True, text=True)
  assert result.returncode == 0
  assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_print_errors():
  result = subprocess.run([str(SDDSCHECK), str(EXAMPLE), "-printErrors"], capture_output=True, text=True)
  assert result.returncode == 0
  assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_nonexistent(tmp_path):
  missing = tmp_path / "missing.sdds"
  result = subprocess.run([str(SDDSCHECK), str(missing)], capture_output=True, text=True)
  assert result.returncode == 0
  assert result.stdout.strip() == "nonexistent"


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_bad_header(tmp_path):
  bad = tmp_path / "bad-header.sdds"
  bad.write_text("not an sdds file\n", encoding="ascii")
  result = subprocess.run([str(SDDSCHECK), str(bad), "-printErrors"], capture_output=True, text=True)
  assert result.returncode == 0
  assert result.stdout.strip() == "badHeader"
  assert result.stderr


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_corrupted(tmp_path):
  corrupted = tmp_path / "corrupted.sdds"
  data = EXAMPLE.read_bytes()
  corrupted.write_bytes(data[: max(32, len(data) // 3)])
  result = subprocess.run([str(SDDSCHECK), str(corrupted), "-printErrors"], capture_output=True, text=True)
  assert result.returncode == 0
  assert result.stdout.strip() in {"corrupted", "badHeader"}


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_multiple_files(tmp_path):
  missing = tmp_path / "missing.sdds"
  result = subprocess.run(
    [str(SDDSCHECK), str(EXAMPLE), str(missing)],
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  assert result.stdout.splitlines() == [
    f"{EXAMPLE}: ok",
    f"{missing}: nonexistent",
  ]


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_multiple_files_threaded_preserves_order(tmp_path):
  bad = tmp_path / "bad-header.sdds"
  missing = tmp_path / "missing.sdds"
  corrupted = tmp_path / "corrupted.sdds"
  bad.write_text("not an sdds file\n", encoding="ascii")
  data = EXAMPLE.read_bytes()
  corrupted.write_bytes(data[: max(32, len(data) // 3)])
  result = subprocess.run(
    [
      str(SDDSCHECK),
      "-threads=2",
      str(EXAMPLE),
      str(missing),
      str(bad),
      str(corrupted),
    ],
    capture_output=True,
    text=True,
  )
  lines = result.stdout.splitlines()
  assert result.returncode == 0
  assert lines[:3] == [
    f"{EXAMPLE}: ok",
    f"{missing}: nonexistent",
    f"{bad}: badHeader",
  ]
  assert lines[3] in {f"{corrupted}: corrupted", f"{corrupted}: badHeader"}


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_threads_must_be_positive():
  result = subprocess.run(
    [str(SDDSCHECK), "-threads=0", str(EXAMPLE)],
    capture_output=True,
    text=True,
  )
  assert result.returncode != 0
  assert "invalid -threads syntax" in result.stderr


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_summary(tmp_path):
  missing = tmp_path / "missing.sdds"
  result = subprocess.run(
    [str(SDDSCHECK), "-summary", str(EXAMPLE), str(missing)],
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  assert result.stdout.splitlines()[-1] == "checked: 2 ok: 1 nonexistent: 1 badHeader: 0 corrupted: 0"


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_failures_only(tmp_path):
  missing = tmp_path / "missing.sdds"
  result = subprocess.run(
    [str(SDDSCHECK), "-failuresOnly", str(EXAMPLE), str(missing)],
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  assert result.stdout.splitlines() == [f"{missing}: nonexistent"]


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_fail_on_error(tmp_path):
  missing = tmp_path / "missing.sdds"
  result = subprocess.run(
    [str(SDDSCHECK), "-failOnError", str(EXAMPLE), str(missing)],
    capture_output=True,
    text=True,
  )
  assert result.returncode == 1


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_recursive_pattern(tmp_path):
  root = tmp_path / "inputs"
  nested = root / "nested"
  nested.mkdir(parents=True)
  valid = root / "valid.sdds"
  bad = nested / "bad.sdds"
  ignored = nested / "ignored.txt"
  shutil.copy(EXAMPLE, valid)
  bad.write_text("not an sdds file\n", encoding="ascii")
  ignored.write_text("not checked\n", encoding="ascii")
  result = subprocess.run(
    [str(SDDSCHECK), "-recursive", "-pattern=*.sdds", str(root)],
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  assert sorted(result.stdout.splitlines()) == sorted([
    f"{valid}: ok",
    f"{bad}: badHeader",
  ])


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_max_errors_with_threads(tmp_path):
  missing = [tmp_path / f"missing-{i}.sdds" for i in range(6)]
  result = subprocess.run(
    [str(SDDSCHECK), "-threads=2", "-maxErrors=1", "-summary"] + [str(path) for path in missing],
    capture_output=True,
    text=True,
  )
  lines = result.stdout.splitlines()
  checked_line = lines[-1]
  checked = int(checked_line.split()[1])
  assert result.returncode == 0
  assert 1 <= checked <= len(missing)
  assert len(lines) == checked + 1
  assert f"nonexistent: {checked}" in checked_line


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_show_pages_and_verbose():
  result = subprocess.run(
    [str(SDDSCHECK), "-showPages", "-verbose", str(EXAMPLE)],
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  assert result.stdout.startswith(f"{EXAMPLE}: ok pagesRead=")
  assert "sizeBytes=" in result.stdout
  assert "dataMode=" in result.stdout
  assert "columns=" in result.stdout


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_check_definitions_only(tmp_path):
  header = EXAMPLE.read_text(encoding="ascii").split("! page number", 1)[0]
  bad_body = tmp_path / "bad-body.sdds"
  bad_body.write_text(header + "not valid page data\n", encoding="ascii")
  result = subprocess.run(
    [str(SDDSCHECK), "-checkDefinitionsOnly", str(bad_body)],
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  assert result.stdout.strip() == "ok"


@pytest.mark.skipif(not SDDSCHECK.exists(), reason="sddscheck not built")
def test_sddscheck_stdin(tmp_path):
  missing = tmp_path / "missing.sdds"
  result = subprocess.run(
    [str(SDDSCHECK), "-stdin"],
    input=f"{EXAMPLE}\n{missing}\n",
    capture_output=True,
    text=True,
  )
  assert result.returncode == 0
  assert result.stdout.splitlines() == [
    f"{EXAMPLE}: ok",
    f"{missing}: nonexistent",
  ]
