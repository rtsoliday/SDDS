import platform
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[1]
PLATFORM_ID = f"{platform.system()}-{platform.machine()}"
IS_DARWIN_ARM64 = PLATFORM_ID == "Darwin-arm64"
BIN_DIR = ROOT_DIR / "bin" / PLATFORM_ID

SYSTEM_LIBRARY_DIRS = (
  Path("/usr/lib64"),
  Path("/usr/lib/x86_64-linux-gnu"),
  Path("/lib64"),
  Path("/usr/lib"),
  Path("/lib"),
  Path("/usr/local/lib"),
  Path("/opt/local/lib"),
  Path("/opt/homebrew/lib"),
  Path("/sw/lib"),
  Path("/usr/sfw/lib"),
)

DARWIN_OPENMP_DIRS = (
  Path("/opt/local/lib/libomp"),
  Path("/opt/homebrew/opt/libomp/lib"),
  Path("/usr/local/opt/libomp/lib"),
)


def sdds_binary(name):
  return BIN_DIR / name


def external_library(name):
  """Find an external library using the same common prefixes as Makefile.rules."""
  suffixes = (".dylib", ".so", ".a")
  for directory in SYSTEM_LIBRARY_DIRS:
    for suffix in suffixes:
      candidate = directory / f"lib{name}{suffix}"
      if candidate.exists():
        return candidate
  return None


def external_library_args(*names):
  """Return absolute libraries when found, with -l fallbacks for system libs."""
  return [str(path) if (path := external_library(name)) else f"-l{name}"
          for name in names]


def external_include_args(*library_names):
  """Return include flags for nonstandard prefixes containing dependencies."""
  directories = []
  for name in library_names:
    library = external_library(name)
    if not library:
      continue
    include = library.parent.parent / "include"
    if include.exists() and include not in directories:
      directories.append(include)
  return [item for directory in directories for item in ("-I", str(directory))]


def openmp_link_args():
  """Return the optional macOS libomp link and runtime-search flags."""
  if platform.system() != "Darwin":
    return []
  for directory in DARWIN_OPENMP_DIRS:
    if (directory / "libomp.dylib").exists():
      return [f"-L{directory}", f"-Wl,-rpath,{directory}", "-lomp"]
  return []
