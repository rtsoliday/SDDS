"""Component build regressions from the 2026-10-22 SDDS6 audit.

Dry runs and variable queries inspect the real component Makefiles without
compiling, linking or running anything.
"""

from pathlib import Path
import os
import platform
import shlex
import shutil
import subprocess

import pytest


ROOT = Path(__file__).resolve().parents[1]
pytestmark = pytest.mark.skipif(
  platform.system() not in ("Linux", "Darwin"), reason="Native Unix make checks"
)


def run_make(directory, *arguments, makefile="Makefile"):
  command = [*shlex.split(os.environ.get("MAKE", shutil.which("gmake") or "make")),
             "-C", str(ROOT / directory), "-f", makefile, *arguments]
  environment = os.environ.copy()
  for name in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
    environment.pop(name, None)
  result = subprocess.run(command, text=True, capture_output=True, env=environment)
  assert result.returncode == 0, result.stdout + result.stderr
  return result.stdout


def query(directory, tmp_path, variables, *arguments, makefile="Makefile"):
  inspect = tmp_path / "inspect.mk"
  inspect.write_text(
    ".PHONY: inspect-component\ninspect-component:\n"
    + "".join(f"\t$(info {name}=$({name}))\n" for name in variables)
    + "\t@:\n"
  )
  output = run_make(directory, "-s", "-f", str(inspect), "inspect-component", *arguments,
                    makefile=makefile)
  values = {}
  for line in output.splitlines():
    name, separator, value = line.partition("=")
    if separator and name in variables:
      values[name] = value
  assert set(values) == set(variables), output
  return values


def command_for(output, marker):
  lines = [line for line in output.splitlines() if marker in line]
  assert lines, output
  return lines[0]


def test_mpi_library_objects_track_headers(tmp_path):
  """libSDDS1mpi objects record header dependencies like the serial library."""
  output = run_make(
    "SDDSlib", "-n", "-B", "MPI_CC=mpicc-dry-run", "OBJ_DIR=O.audit-mpi",
    f"LIB_DIR={tmp_path}/lib", "O.audit-mpi/SDDS_enum.mpi.o", makefile="Makefile.mpi",
  )
  compile_command = command_for(output, "-c SDDS_enum.c")
  assert "-MMD -MP -MF O.audit-mpi/SDDS_enum.mpi.d" in compile_command


@pytest.mark.parametrize("makefile,library,link_marker", (
  ("Makefile", "libSDDS1.a", "test_sdds_enum.c"),
  ("Makefile.mpi", "libSDDS1mpi.a", "test_sdds_enum_mpi.c"),
))
def test_enum_tests_link_the_built_archive(tmp_path, makefile, library, link_marker):
  """Enum tests link this build's archive and refresh the installed copy."""
  output = run_make(
    "SDDSlib", "-n", "-B", "MPI_CC=mpicc-dry-run", "OBJ_DIR=O.audit-enum",
    f"LIB_DIR={tmp_path}/lib", "enum-test", makefile=makefile,
  )
  link = command_for(output, link_marker)
  assert f"O.audit-enum/{library}" in link.split()
  assert f"-l{library[3:-2]}" not in link.split()
  assert f"cp -f O.audit-enum/{library} {tmp_path}/lib/{library}" in output


def test_revision_define_is_not_recorded(tmp_path):
  """A new revision reaches compiled objects without changing the build stamp."""
  output = run_make(
    "SDDSaps", "-n", "-B", "SVN_VERSION=GIT_audit", "OBJ_DIR=O.audit-revision",
    f"BIN_DIR={tmp_path}/bin", "O.audit-revision/sddsquery.o",
  )
  assert "SVN_VERSION" not in command_for(output, "build-config")
  assert '-DSVN_VERSION=\\"GIT_audit\\"' in command_for(output, "-c sddsquery.c")


def test_install_removes_stale_mpl_qt_3d(tmp_path):
  """The former standalone 3D viewer is removed from both install trees."""
  stale = [tmp_path / "bin" / "mpl_qt_3d", tmp_path / "extensions" / "mpl_qt_3d"]
  for path in stale:
    path.parent.mkdir(parents=True)
    path.write_text("stale fixture\n")
  output = run_make(
    "SDDSaps/sddsplots/qtDriver", "-n", "install-programs", "OBJ_DIR=O.audit-qt",
    f"BIN_DIR={stale[0].parent}", f"EPICS_BIN_DIR={stale[1].parent}",
  )
  assert f"rm -f {stale[0]} {stale[1]}" in output.splitlines()


@pytest.mark.parametrize("component", (
  "SDDSaps", "SDDSaps/sddsplots", "SDDSaps/sddscontours", "SDDSaps/pseudoInverse",
  "utils", "rpns/code", "SDDSlib/demo",
))
def test_windows_programs_embed_utf8_manifest(tmp_path, component):
  """Every Windows program accepts UTF-8 command-line labels."""
  values = query(
    component, tmp_path, ("LINKEXE", "PROD_DEPS"),
    "OS=Windows", "ARCH=x86_64", "QT_BUILD_GOALS=", "HAVE_QT=1",
    "EPICS_REQUIRED_LIBS=", "PYTHON=python3",
  )
  flags = values["LINKEXE"].split()
  assert "/MANIFEST:EMBED" in flags
  manifest = next(flag for flag in flags if flag.startswith("/MANIFESTINPUT:"))
  manifest = manifest.partition(":")[2]
  assert (ROOT / component / manifest).resolve() == ROOT / "windows-utf8.manifest"
  assert manifest in values["PROD_DEPS"].split()


@pytest.mark.parametrize("component", ("SDDSaps", "rpns/code"))
@pytest.mark.parametrize("epics_tree", (False, True), ids=("checkout", "extensions-tree"))
def test_local_gsl_headers_in_extensions_tree(tmp_path, component, epics_tree):
  """An extensions build compiles against the local GSL checkout's headers."""
  if platform.system() != "Linux":
    pytest.skip("The local GSL checkout is built on Linux")
  root = tmp_path / "repository"
  (root / "gsl" / "gsl").mkdir(parents=True)
  arguments = [f"RULES_DIR={root}/", f"USE_EPICS_DIRS={int(epics_tree)}"]
  values = query(component, tmp_path, ("CFLAGS", "CXXFLAGS"), *arguments)
  for name in ("CFLAGS", "CXXFLAGS"):
    assert (f"-I{root}/gsl" in values[name].split()) == epics_tree
