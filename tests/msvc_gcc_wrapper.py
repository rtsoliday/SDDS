"""Translate the test harnesses' small GCC command line subset to MSVC."""

import os
from pathlib import Path
import shutil
import subprocess
import sys


ROOT_DIR = Path(__file__).resolve().parents[1]
COMPAT_INCLUDE = ROOT_DIR / "tests" / "windows_compat"
IGNORED_LIBRARIES = {"dl", "gcc", "m", "pthread", "rt"}
STATIC_INTERNAL_LIBRARIES = {"rpnlib", "SDDS1", "SDDS1mpi"}
INTERNAL_OBJECTS = {
  "mdbcommon": ("mdbcommon", ("lsfp.obj", "rcds_powell.obj", "savitzkyGolay.obj", "scanargs.obj")),
  "mdblib": ("mdblib", ("timer.obj",)),
  "mdbmth": ("mdbmth", ("moments.obj", "rkODE.obj")),
  "rpnlib": (
    "rpns/code",
    (
      "array.obj", "conditional.obj", "execute.obj", "get_token_rpn.obj",
      "infixtopostfix.obj", "logical.obj", "math.obj", "memory.obj", "pcode.obj",
      "pop_push.obj", "prompt.obj", "rpn_lock.obj", "rpn_csh.obj", "rpn_data.obj",
      "rpn_draw.obj", "rpn_error.obj", "rpn_io.obj", "rpn_sub.obj", "stack.obj", "udf.obj",
    ),
  ),
  "SDDS1": (
    "SDDSlib",
    (
      "SDDS_ascii.obj", "SDDS_binary.obj", "SDDS_copy.obj", "SDDS_data.obj",
      "SDDS_dataprep.obj", "SDDS_extract.obj", "SDDS_info.obj", "SDDS_input.obj",
      "SDDS_lzma.obj", "SDDS_mplsupport.obj", "SDDS_output.obj", "SDDS_process.obj",
      "SDDS_rpn.obj", "SDDS_transfer.obj", "SDDS_utils.obj", "SDDS_write.obj",
    ),
  ),
  "SDDS1mpi": (
    "SDDSlib",
    (
      "SDDS_ascii.mpi.obj", "SDDS_binary.mpi.obj", "SDDS_copy.mpi.obj",
      "SDDS_data.mpi.obj", "SDDS_dataprep.mpi.obj", "SDDS_extract.mpi.obj",
      "SDDS_info.mpi.obj", "SDDS_input.mpi.obj", "SDDS_lzma.mpi.obj",
      "SDDS_mplsupport.mpi.obj", "SDDS_output.mpi.obj", "SDDS_process.mpi.obj",
      "SDDS_rpn.mpi.obj", "SDDS_transfer.mpi.obj", "SDDS_utils.mpi.obj",
      "SDDS_write.mpi.obj", "SDDS_MPI_binary.obj", "SDDSmpi_input.obj", "SDDSmpi_output.obj",
    ),
  ),
  "xls": ("xlslib", ("assert_assist.obj",)),
}


def find_library(name, directories):
  if name in IGNORED_LIBRARIES:
    return None
  for directory in directories:
    candidate = directory / f"{name}.lib"
    if candidate.exists():
      return candidate
  raise SystemExit(f"MSVC test wrapper could not find {name}.lib")


def main(argv):
  compiler = shutil.which("cl")
  if not compiler:
    raise SystemExit("MSVC cl.exe is not available; run tests from a Visual Studio developer environment")

  include_dirs = [COMPAT_INCLUDE, ROOT_DIR / "lzma", ROOT_DIR / "zlib"]
  library_dirs = [
    ROOT_DIR / "lib" / "Windows-x86_64",
    ROOT_DIR.parent / "gsl" / "lib",
    Path("C:/MicrosoftMPI/Lib/x64"),
  ]
  defines = [
    "__STDC__=0",
    "_CRT_NONSTDC_NO_DEPRECATE",
    "_CRT_SECURE_NO_DEPRECATE",
  ]
  inputs = []
  library_names = []
  compiler_options = ["/nologo", "/MD", "/W3", "/wd5105", "/EHsc"]
  output = None

  index = 0
  while index < len(argv):
    argument = argv[index]
    if argument in ("-I", "-L", "-o"):
      if index + 1 >= len(argv):
        raise SystemExit(f"missing value after {argument}")
      value = argv[index + 1]
      index += 2
      if argument == "-I":
        include_dirs.append(Path(value))
      elif argument == "-L":
        library_dirs.append(Path(value))
      else:
        output = Path(value)
      continue
    if argument.startswith("-I") and len(argument) > 2:
      include_dirs.append(Path(argument[2:]))
    elif argument.startswith("-L") and len(argument) > 2:
      library_dirs.append(Path(argument[2:]))
    elif argument.startswith("-l") and len(argument) > 2:
      library_names.append(argument[2:])
    elif argument.startswith("-D") and len(argument) > 2:
      defines.append(argument[2:])
    elif argument == "-fopenmp":
      compiler_options.append("/openmp")
    elif argument.startswith("-std=c++"):
      compiler_options.append("/std:c++14")
    elif argument.startswith("-std=c"):
      compiler_options.append("/std:c11")
    elif argument.startswith("-Wl,") or argument in ("-Wall", "-Wextra", "-pthread"):
      pass
    elif argument.endswith((".c", ".cc", ".cpp", ".cxx", ".obj")):
      inputs.append(Path(argument))
    elif argument.lower().endswith((".lib", ".a")):
      library = Path(argument)
      if library.suffix.lower() == ".a":
        name = library.stem
        if name.startswith("lib"):
          name = name[3:]
        library_names.append(name)
      else:
        inputs.append(library)
    else:
      raise SystemExit(f"MSVC test wrapper does not understand argument: {argument}")
    index += 1

  if not output:
    raise SystemExit("MSVC test wrapper requires -o <output>")
  if not inputs:
    raise SystemExit("MSVC test wrapper received no source file")

  libraries = []
  for name in library_names:
    library = find_library(name, library_dirs)
    if name not in STATIC_INTERNAL_LIBRARIES and library and library not in libraries:
      libraries.append(library)
    if name in INTERNAL_OBJECTS:
      directory, object_names = INTERNAL_OBJECTS[name]
      object_dir = ROOT_DIR / directory / "O.Windows-x86_64"
      for object_name in object_names:
        object_paths = sorted(object_dir.glob("*.obj")) if object_name == "*" else [object_dir / object_name]
        for object_path in object_paths:
          if not object_path.exists():
            raise SystemExit(f"MSVC test wrapper could not find {object_path}")
          inputs.append(object_path)

  mpi_library = Path("C:/MicrosoftMPI/Lib/x64/msmpi.lib")
  if "SDDS1mpi" in library_names and mpi_library.exists():
    libraries.append(mpi_library)
  mpi_include = Path("C:/MicrosoftMPI/Include")
  if mpi_include.exists():
    include_dirs.append(mpi_include)

  command = [compiler, *compiler_options]
  command.extend(f"/D{item}" for item in defines)
  command.extend(f"/I{os.fspath(path)}" for path in include_dirs if path.exists())
  command.extend(os.fspath(path) for path in inputs)
  command.append(f"/Fe:{os.fspath(output)}")
  command.extend(["/link", "/LTCG"])
  command.extend(f"/LIBPATH:{os.fspath(path)}" for path in library_dirs if path.exists())
  command.extend(os.fspath(path) for path in libraries)

  completed = subprocess.run(command, cwd=output.parent)
  if completed.returncode:
    return completed.returncode

  suffixed_output = Path(f"{output}.exe")
  if not output.exists() and suffixed_output.exists():
    suffixed_output.replace(output)
  return 0


if __name__ == "__main__":
  raise SystemExit(main(sys.argv[1:]))
