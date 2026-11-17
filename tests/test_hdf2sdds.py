"""Round-trip numeric HDF5 columns through the toolkit converters."""

import subprocess

import pytest

from sdds_test_utils import BIN_DIR


@pytest.mark.parametrize("reduction,expected", [
  ((), ["0 0 -1", "1 2147483648 -2", "2 4294967295 -3"]),
  (("-reduceFactor=2,keep=2",), ["1 2147483648 -2"]),
])
def test_rank_one_unsigned_columns(tmp_path, reduction, expected):
  source, hdf, output = (tmp_path / name for name in ("input.sdds", "input.h5", "output.sdds"))
  source.write_text("SDDS1\n&column name=counts,type=ulong &end\n"
                    "&column name=signed,type=long &end\n&data mode=ascii &end\n"
                    "3\n0 -1\n2147483648 -2\n4294967295 -3\n")

  for tool, arguments in [
    ("sdds2hdf", (source, hdf)),
    ("hdf2sdds", (hdf, output, "-group=/page1/columns", "-withIndex", *reduction)),
    ("sdds2stream", (output, "-columns=Index,counts,signed")),
  ]:
    result = subprocess.run([str(BIN_DIR / tool), *map(str, arguments)],
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
  assert [" ".join(line.split()) for line in result.stdout.splitlines()] == expected
