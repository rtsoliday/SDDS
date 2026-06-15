import platform
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[1]
PLATFORM_ID = f"{platform.system()}-{platform.machine()}"
IS_DARWIN_ARM64 = PLATFORM_ID == "Darwin-arm64"
BIN_DIR = ROOT_DIR / "bin" / PLATFORM_ID


def sdds_binary(name):
  return BIN_DIR / name
