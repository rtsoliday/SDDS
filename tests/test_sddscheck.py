import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
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
