import re
import subprocess
from pathlib import Path
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSEXPAND = BIN_DIR / "sddsexpand"
SDDSPRINTOUT = BIN_DIR / "sddsprintout"


def extract_options():
  text = Path("SDDSaps/sddsexpand.c").read_text()
  match = re.search(r"static char \*option\[N_OPTIONS\] = \{([^}]*)\};", text, re.DOTALL)
  return re.findall(r"\"(.*?)\"", match.group(1))


def print_dataset(path: Path) -> str:
  text_file = path.with_suffix(".txt")
  subprocess.run([str(SDDSPRINTOUT), str(path), str(text_file), "-parameters=*"] , check=True)
  text = text_file.read_text()
  lines = text.splitlines()[1:]
  body = "\n".join(lines)
  body = re.sub(r"longdouble\w* =\s+[0-9eE\+\-.]+", "", body)
  return body


@pytest.mark.skipif(not (SDDSEXPAND.exists() and SDDSPRINTOUT.exists()), reason="tools not built")
def test_sddsexpand_options(tmp_path):
  options = extract_options()
  baseline = tmp_path / "base.sdds"
  subprocess.run([str(SDDSEXPAND), "SDDSlib/demo/example.sdds", str(baseline)], check=True)
  baseline_text = print_dataset(baseline)

  for option in options:
    if option == "pipe":
      out = tmp_path / f"{option}.sdds"
      with open(out, "wb") as f:
        subprocess.run(
          [str(SDDSEXPAND), "SDDSlib/demo/example.sdds", "-pipe=output"],
          stdout=f,
          check=True,
        )
      result = print_dataset(out)
      assert result == baseline_text
    elif option == "majorOrder":
      for value in ("row", "column"):
        out = tmp_path / f"{option}-{value}.sdds"
        subprocess.run(
          [
            str(SDDSEXPAND),
            "SDDSlib/demo/example.sdds",
            str(out),
            f"-majorOrder={value}",
          ],
          check=True,
        )
        result = print_dataset(out)
        assert result == baseline_text
    elif option == "nowarnings":
      out = tmp_path / f"{option}.sdds"
      subprocess.run(
        [str(SDDSEXPAND), "SDDSlib/demo/example.sdds", str(out), "-noWarnings"],
        check=True,
      )
      result = print_dataset(out)
      assert result == baseline_text
