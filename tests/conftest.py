import os
import platform
import subprocess
import sys
from pathlib import Path


if platform.system() == "Darwin" and platform.machine() == "arm64":
  os.environ.setdefault("SDDS_LONGDOUBLE_64BITS", "1")


if platform.system() == "Windows":
  os.environ.setdefault("SDDS_LONGDOUBLE_64BITS", "1")
  root_dir = Path(__file__).resolve().parents[1]
  bin_dir = root_dir / "bin" / "Windows-x86_64"
  os.environ["PATH"] = os.fspath(bin_dir) + os.pathsep + os.environ.get("PATH", "")
  test_compiler = Path(__file__).with_name("msvc_gcc_wrapper.py")
  compiler_command = f'"{Path(sys.executable).as_posix()}" "{test_compiler.as_posix()}"'
  os.environ["CC"] = compiler_command
  os.environ["CXX"] = compiler_command
  os.environ["MPI_CC"] = compiler_command
  _native_popen = subprocess.Popen

  def _normalize_windows_arg(arg):
    text = os.fspath(arg)
    if text.lower().endswith(".exe") or f":{chr(92)}" not in text:
      return text
    return text.replace(chr(92), "/")

  def _windows_path_popen(args, *popen_args, **popen_kwargs):
    """Keep SDDS option parsing from treating backslashes as escapes."""
    if isinstance(args, (list, tuple)):
      args = type(args)(
        _normalize_windows_arg(arg) if isinstance(arg, (str, os.PathLike)) else arg
        for arg in args
      )
    return _native_popen(args, *popen_args, **popen_kwargs)

  subprocess.Popen = _windows_path_popen
