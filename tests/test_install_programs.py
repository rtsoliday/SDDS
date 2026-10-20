"""Exercise large install lists without compiling or executing fixture programs."""

import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess

import pytest


ROOT = Path(__file__).resolve().parents[1]


def test_large_program_install_restores_outputs(tmp_path):
  """Both install destinations handle lists exceeding Windows shell limits."""
  make = shlex.split(os.environ.get("MAKE", shutil.which("gmake") or "make"),
                     posix=platform.system() != "Windows")
  make = [argument.strip('"') for argument in make]
  if not shutil.which(make[0]):
    pytest.skip("GNU Make is required")
  payloads = tmp_path / "payloads"
  payloads.mkdir()
  for index in range(80):
    (payloads / f"program_{index:03d}.data").write_text(f"payload {index}\n")
  (tmp_path / "Makefile").write_text(
    f"include {ROOT.as_posix()}/Makefile.rules\n"
    "INSTALL_PROGRAMS = $(wildcard payloads/*.data)\n"
    f"include {ROOT.as_posix()}/Makefile.build\n"
  )
  command = [*make, "install-programs", "BIN_DIR=bin", "LIB_DIR=lib",
             "OBJ_DIR=objects", "EPICS_BIN_DIR=extensions/bin"]
  if platform.system() == "Windows":
    cygwin = Path(os.environ.get("SDDS_CYGWIN_ROOT", "C:/cygwin64"))
    command += ["OS=Windows", "ARCH=x86_64",
                f"SHELL={(cygwin / 'bin/sh.exe').as_posix()}"]
  environment = os.environ.copy()
  for name in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
    environment.pop(name, None)

  def install():
    result = subprocess.run(command, cwd=tmp_path, env=environment,
                            text=True, capture_output=True)
    assert result.returncode == 0, result.stdout + result.stderr

  install()
  installed = [tmp_path / directory / source.name
               for directory in ("bin", "extensions/bin")
               for source in sorted(payloads.iterdir())]
  for output in installed:
    assert output.read_bytes() == (payloads / output.name).read_bytes()
  timestamps = [output.stat().st_mtime_ns for output in installed]
  install()
  assert [output.stat().st_mtime_ns for output in installed] == timestamps
  installed[0].unlink()
  installed[-1].write_text("stale installed payload\n")
  install()
  for output in installed:
    assert output.read_bytes() == (payloads / output.name).read_bytes()
