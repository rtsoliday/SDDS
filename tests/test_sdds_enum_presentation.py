"""Regression coverage for enum printout read errors and plot annotations."""
import json
import struct
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


MAPPING = "0:OFF,1:ON"


def run(tool, *args):
  return subprocess.run([str(BIN_DIR / tool), *map(str, args)],
                        capture_output=True, text=True)


def printout_input(path, kind, encoding, codes):
  header = ('SDDS6\n'
            f'&{kind} name=state, type=enum, enum_values="{MAPPING}" &end\n'
            '&column name=x, type=long &end\n'
            f'&data mode={encoding}, endian=little &end\n')
  pages = []
  for code in codes:
    if encoding == "binary":
      values = [1]  # Row count precedes all binary title data.
      if kind == "array":
        values.append(1)  # One array element.
      values.extend([code, 5])
      pages.append(struct.pack("<" + "i" * len(values), *values))
    else:
      if kind == "parameter":
        page = f'{code}\n1\n5\n'
      elif kind == "array":
        page = f'1\n{code}\n1\n5\n'
      else:
        page = f'1\n{code} 5\n'
      pages.append(page.encode("utf8"))
  path.write_bytes(header.encode("utf8") + b''.join(pages))


def printout_options(kind):
  return [f'-{kind}=state' if kind != "array" else '-column=x', '-noTitle']


@pytest.mark.skipif(not (BIN_DIR / "sddsprintout").exists(), reason="sddsprintout not built")
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
@pytest.mark.parametrize("valid_prefix", [False, True])
def test_printout_reports_enum_page_read_errors(tmp_path, kind, encoding, valid_prefix):
  source = tmp_path / "invalid.sdds"
  printout_input(source, kind, encoding, ([1] if valid_prefix else []) + [42])

  result = run("sddsprintout", source, *printout_options(kind))

  assert result.returncode > 0
  assert "enum" in result.stderr.lower()
  assert "42" in result.stderr
  if valid_prefix:
    assert ("5" if kind == "array" else "ON") in result.stdout
  else:
    assert result.stdout == ""


@pytest.mark.skipif(not (BIN_DIR / "sddsprintout").exists(), reason="sddsprintout not built")
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("encoding", ["ascii", "binary"])
def test_printout_accepts_normal_enum_eof(tmp_path, kind, encoding):
  source = tmp_path / "valid.sdds"
  printout_input(source, kind, encoding, [0, 1])

  result = run("sddsprintout", source, *printout_options(kind))

  assert result.returncode == 0, result.stderr
  assert result.stderr == ""
  if kind != "array":
    assert "OFF" in result.stdout and "ON" in result.stdout


@pytest.mark.skipif(not (BIN_DIR / "sddsplot").exists(), reason="sddsplot not built")
@pytest.mark.parametrize("fixed", [False, True])
@pytest.mark.parametrize("format", [None, "%s", "[%8s]"])
def test_plot_enum_parameter_annotation(tmp_path, fixed, format):
  source, output = tmp_path / "plot.sdds", tmp_path / "plot.json"
  source.write_text('SDDS6\n'
                    f'&parameter name=state, type=enum, enum_values="{MAPPING}"' +
                    (', fixed_value=1' if fixed else '') + ' &end\n'
                    '&column name=x, type=double &end\n'
                    '&column name=y, type=double &end\n'
                    '&data mode=ascii &end\n' +
                    ('' if fixed else '1\n') + '2\n0 1\n1 2\n', encoding="utf8")
  annotation = '-string=@state,xCoordinate=0,yCoordinate=1'
  if format is not None:
    annotation += f',format={format}'

  result = run("sddsplot", source, '-column=x,y', annotation,
               '-device=json', f'-output={output}')

  assert result.returncode == 0, result.stderr
  assert result.stderr == ""
  trace = json.loads(output.read_text())["plots"][0]["traces"][0]
  assert trace["x"] == [0, 1]
  assert trace["y"] == [1, 2]
