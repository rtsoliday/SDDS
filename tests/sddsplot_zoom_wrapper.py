"""Log an sddsplot invocation for the opt-in Qt zoom integration test."""

import os
from pathlib import Path
import sys


def main():
  if len(sys.argv) < 3:
    raise SystemExit("usage: sddsplot_zoom_wrapper.py LOG EXECUTABLE [ARG ...]")

  log = Path(sys.argv[1])
  executable = sys.argv[2]
  arguments = sys.argv[3:]
  with log.open("a", encoding="utf-8") as stream:
    stream.write("CALL\n")
    for argument in arguments:
      stream.write(f"ARG={argument}\n")
  os.execv(executable, [executable, *arguments])


if __name__ == "__main__":
  main()
