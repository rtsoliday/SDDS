import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSBREAK = BIN_DIR / "sddsbreak"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"
SDDSCHECK = BIN_DIR / "sddscheck"


def parse_printout(text, column, caster):
  pages = []
  current = None
  for line in text.splitlines():
    stripped = line.strip()
    if not stripped:
      continue
    if stripped == column:
      if current is not None:
        pages.append(current)
      current = []
    elif set(stripped) == {"-"}:
      continue
    else:
      current.append(caster(stripped))
  if current is not None:
    pages.append(current)
  return pages


@pytest.mark.skipif(
  not (SDDSBREAK.exists() and SDDSPRINTOUT.exists() and SDDSCHECK.exists()),
  reason="tools not built",
)
@pytest.mark.parametrize(
  "options,column,caster,expected",
  [
    (
      ["-rowlimit=2,overlap=1"],
      "shortCol",
      int,
      [[1, 2], [2, 3], [3, 4], [4, 5], [6, 7], [7, 8]],
    ),
    (
      ["-pagesPerPage=2"],
      "shortCol",
      int,
      [[1, 2], [3, 4, 5], [6], [7, 8]],
    ),
    (
      ["-gapin=floatCol,amount=1"],
      "shortCol",
      int,
      [[1], [2], [3], [4], [5], [6], [7], [8]],
    ),
    (
      ["-increaseOf=shortCol"],
      "shortCol",
      int,
      [[1], [2], [3], [4], [5], [6], [7], [8]],
    ),
    (
      ["-changeOf=stringCol"],
      "stringCol",
      str,
      [["one"], ["two"], ["three"], ["four"], ["five"], ["six"], ["seven"], ["eight"]],
    ),
    (
      ["-matchTo=stringCol,three"],
      "stringCol",
      str,
      [["one", "two"], ["three", "four", "five"], ["six", "seven", "eight"]],
    ),
    (
      ["-rowlimit=2", "-majorOrder=column"],
      "shortCol",
      int,
      [[1, 2], [3, 4], [5], [6, 7], [8]],
    ),
  ],
)
def test_sddsbreak_example(tmp_path, options, column, caster, expected):
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSBREAK), "SDDSlib/demo/example.sdds", str(output)] + options,
    check=True,
  )
  result = subprocess.run(
    [str(SDDSCHECK), str(output)], capture_output=True, text=True, check=True
  )
  assert result.stdout.strip() == "ok"
  pr = subprocess.run(
    [str(SDDSPRINTOUT), str(output), f"-columns={column}", "-noTitle"],
    capture_output=True,
    text=True,
    check=True,
  )
  pages = parse_printout(pr.stdout, column, caster)
  assert pages == expected


@pytest.mark.skipif(
  not (SDDSBREAK.exists() and SDDSPRINTOUT.exists() and SDDSCHECK.exists()),
  reason="tools not built",
)
def test_sddsbreak_decreaseof(tmp_path):
  input_file = tmp_path / "decrease.sdds"
  input_file.write_text(
    "SDDS1\n"
    "&column name=num, type=double, &end\n"
    "&data mode=ascii, &end\n"
    "! page number 1\n"
    "5\n"
    "5\n"
    "4\n"
    "3\n"
    "2\n"
    "1\n"
  )
  output = tmp_path / "out.sdds"
  subprocess.run(
    [str(SDDSBREAK), str(input_file), str(output), "-decreaseOf=num"],
    check=True,
  )
  result = subprocess.run(
    [str(SDDSCHECK), str(output)], capture_output=True, text=True, check=True
  )
  assert result.stdout.strip() == "ok"
  pr = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-columns=num", "-noTitle"],
    capture_output=True,
    text=True,
    check=True,
  )
  pages = parse_printout(pr.stdout, "num", lambda s: int(float(s)))
  assert pages == [[5], [4], [3], [2], [1]]


@pytest.mark.skipif(
  not (SDDSBREAK.exists() and SDDSPRINTOUT.exists() and SDDSCHECK.exists()),
  reason="tools not built",
)
def test_sddsbreak_pipe(tmp_path):
  with open("SDDSlib/demo/example.sdds", "rb") as infile:
    proc = subprocess.run(
      [str(SDDSBREAK), "-pipe=in,out", "-rowlimit=2"],
      input=infile.read(),
      stdout=subprocess.PIPE,
      check=True,
    )
  output = tmp_path / "out.sdds"
  output.write_bytes(proc.stdout)
  result = subprocess.run(
    [str(SDDSCHECK), str(output)], capture_output=True, text=True, check=True
  )
  assert result.stdout.strip() == "ok"
  pr = subprocess.run(
    [str(SDDSPRINTOUT), str(output), "-columns=shortCol", "-noTitle"],
    capture_output=True,
    text=True,
    check=True,
  )
  pages = parse_printout(pr.stdout, "shortCol", int)
  assert pages == [[1, 2], [3, 4], [5], [6, 7], [8]]

