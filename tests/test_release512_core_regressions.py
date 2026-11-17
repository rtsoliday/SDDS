"""Core API regressions reached by changes after the SDDS-5.12 release."""

import json
import subprocess

import pytest

from sdds_test_utils import BIN_DIR
from test_sdds_enum_compatibility import fault_tree


@pytest.mark.parametrize("parameter,original", [
  ("Description", "Tree"), ("LogicalTypeDesc", "AND"), ("TreeName", "Tree1"),
])
def test_fault_tree_keeps_long_string_parameters(tmp_path, parameter, original):
  source, output, ascii_output = (tmp_path / name for name in
                                 ("tree.sdds", "result.sdds", "result-ascii.sdds"))
  fault_tree(source)
  label = parameter + "_" + "A" * 8192
  # Keep the input in the legacy format supported by SDDS-5.12: the regression
  # is a newly exposed overflow for ordinary string parameters, independent of enums.
  source.write_text(source.read_text().replace("SDDS6\n", "SDDS1\n", 1)
                    .replace(json.dumps(original), json.dumps(label), 1))
  result = subprocess.run([str(BIN_DIR / "convert_to_bdd"), str(source), str(output)],
                          capture_output=True, text=True, timeout=30)
  assert result.returncode == 0, result.stderr
  result = subprocess.run([str(BIN_DIR / "sddsconvert"), str(output), str(ascii_output), "-ascii"],
                          capture_output=True, text=True, timeout=30)
  assert result.returncode == 0, result.stderr
  assert label in ascii_output.read_text().splitlines()
