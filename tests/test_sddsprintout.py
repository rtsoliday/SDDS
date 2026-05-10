import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
EXAMPLE = Path("SDDSlib/demo/example.sdds")

@pytest.mark.skipif(not SDDSPRINTOUT.exists(), reason="sddsprintout not built")
def test_sddsprintout(tmp_path):
  output = tmp_path / "print.txt"
  subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(EXAMPLE),
      str(output),
      "-columns=doubleCol",
      "-parameters=stringParam",
      "-noTitle",
    ],
    check=True,
  )
  text = output.read_text()
  assert "FirstPage" in text
  assert "1.001000e+01" in text


@pytest.mark.skipif(not SDDSPRINTOUT.exists(), reason="sddsprintout not built")
def test_page_range_no_labels_and_title(tmp_path):
  output = tmp_path / "page2.txt"
  subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(EXAMPLE),
      str(output),
      "-columns=longCol",
      "-fromPage=2",
      "-toPage=2",
      "-title=Custom",
      "-noLabels",
    ],
    check=True,
  )
  text = output.read_text()
  assert "Custom" in text
  assert "600" in text
  assert "100" not in text


@pytest.mark.skipif(not SDDSPRINTOUT.exists(), reason="sddsprintout not built")
def test_pipe_and_spreadsheet_output():
  result = subprocess.run(
    [
      str(SDDSPRINTOUT),
      "-pipe=in",
      "-columns=longCol",
      "-columns=shortCol",
      "-spreadsheet=delimiter=:,nolabels",
      "-noTitle",
    ],
    input=EXAMPLE.read_bytes(),
    stdout=subprocess.PIPE,
    check=True,
  )
  text = result.stdout.decode()
  assert "100:" in text
  assert "1" in text
  assert "600:" in text
  assert "8" in text


@pytest.mark.skipif(not SDDSPRINTOUT.exists(), reason="sddsprintout not built")
def test_markdown_output(tmp_path):
  output = tmp_path / "markdown.txt"
  subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(EXAMPLE),
      str(output),
      "-columns=shortCol",
      "-columns=longCol",
      "-markdown",
      "-noTitle",
    ],
    check=True,
  )
  text = output.read_text()
  assert "|" in text
  assert "shortCol" in text or "100" in text


@pytest.mark.skipif(not SDDSPRINTOUT.exists(), reason="sddsprintout not built")
def test_pageadvance_and_postpagelines(tmp_path):
  output = tmp_path / "advanced.txt"
  subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(EXAMPLE),
      str(output),
      "-columns=shortCol",
      "-pageAdvance",
      "-title=Paged",
    ],
    check=True,
  )
  text = output.read_text()
  assert "Paged" in text
  assert "6" in text


@pytest.mark.skipif(not SDDSPRINTOUT.exists(), reason="sddsprintout not built")
def test_paginate_and_post_page_lines(tmp_path):
  output = tmp_path / "paginate.txt"
  subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(EXAMPLE),
      str(output),
      "-columns=shortCol",
      "-paginate=lines=20,nolabels",
    ],
    check=True,
  )
  text = output.read_text()
  assert "shortCol" not in text or "1" in text


@pytest.mark.skipif(not SDDSPRINTOUT.exists(), reason="sddsprintout not built")
def test_html_output(tmp_path):
  output = tmp_path / "table.html"
  subprocess.run(
    [
      str(SDDSPRINTOUT),
      str(EXAMPLE),
      str(output),
      "-columns=shortCol",
      "-columns=longCol",
      "-htmlFormat=caption=Example",
      "-noTitle",
    ],
    check=True,
  )
  text = output.read_text()
  assert "<table" in text.lower() or "Example" in text
