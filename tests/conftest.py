import os
import platform


if platform.system() == "Darwin" and platform.machine() == "arm64":
  os.environ.setdefault("SDDS_LONGDOUBLE_64BITS", "1")
