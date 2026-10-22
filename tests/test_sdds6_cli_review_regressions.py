"""Regressions for further SDDS6 command-line review findings."""

import json
import subprocess

import pytest

from sdds_test_utils import BIN_DIR


def run(tool, *args, check=True):
  if not (BIN_DIR / tool).exists():
    pytest.skip(f"{tool} not built")
  result = subprocess.run([str(BIN_DIR / tool), *map(str, args)], capture_output=True,
                          text=True, encoding="utf8", timeout=60)
  if check:
    assert result.returncode == 0, result.stderr
  return result


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("source_type,target_type", [("long", "double"), ("enum", "long"),
                                                     ("enum", "string")])
def test_cast_rejects_mapping_for_non_enum_target(tmp_path, kind, source_type, target_type):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  args = [f"-{kind}=state,type={source_type}", "-data=1"]
  if source_type == "enum":
    args.append(f"-enumValues={kind},state,1:ONE")
  run("sddsmakedataset", source, *args)

  result = run("sddscast", source, output, f"-cast={kind},state,{source_type},{target_type}",
               f"-enumValues={kind},state,1:ONE", check=False)
  assert result.returncode != 0
  assert "enum cast target" in result.stderr


@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_cast_applies_mapping_to_enum_target(tmp_path, kind):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  run("sddsmakedataset", source, f"-{kind}=state,type=long", "-data=1")
  run("sddscast", source, output, f"-cast={kind},state,long,enum",
      f"-enumValues={kind},state,1:ONE")
  assert run("sdds2stream", output, f"-{kind}=state", "-noquotes").stdout.split() == ["ONE"]


@pytest.mark.parametrize("version", [1, 6])
def test_plot_defines_first_parameter_on_loaded_page(tmp_path, version):
  source, output = tmp_path / "columns.sdds", tmp_path / "plot.json"
  source.write_text(f"SDDS{version}\n&column name=x, type=double &end\n"
                    "&column name=y, type=double &end\n&data mode=ascii &end\n"
                    "2\n1 2\n2 3\n2\n1 4\n2 5\n", encoding="utf8")
  run("sddsplot", source, "-column=x,y", "-split=page", "-define=parameter,P,2",
      "-legend=parameter=P", "-device=json", f"-output={output}")
  document = json.loads(output.read_text(encoding="utf8"))
  traces = [trace for plot in document["plots"] for trace in plot["traces"]]
  assert len(traces) == 2
  assert all(float(trace["name"]) == 2 for trace in traces)


@pytest.mark.parametrize("label_type", ["enum", "string"])
@pytest.mark.parametrize("value_type", ["longdouble", "double", "float", "long64", "ulong64",
                                       "long", "ulong", "short", "ushort", "character", "string"])
def test_diff_preserves_percent_bytes_in_row_labels(tmp_path, label_type, value_type):
  paths = [tmp_path / "one.sdds", tmp_path / "two.sdds"]
  label = "50%%done"
  for index, path in enumerate(paths):
    value = ("A", "B")[index] if value_type in ("character", "string") else str(index + 1)
    row_args = [f"-column=row,type={label_type}", f"-data={0 if label_type == 'enum' else label}"]
    if label_type == "enum":
      row_args.append(f"-enumValues=column,row,0:{label}")
    run("sddsmakedataset", path, *row_args, f"-column=x,type={value_type}", f"-data={value}")

  result = run("sddsdiff", *paths, "-columns=x", "-rowlabel=row")
  assert "Differences found in column" in result.stdout
  assert label in result.stdout


def write_matrix_pages(path, name, pages):
  lines = ["SDDS1", f"&column name={name}, type=double &end", "&data mode=ascii &end"]
  for page in pages:
    lines.append(str(len(page)))
    lines.extend(map(str, page))
  path.write_text("\n".join(lines) + "\n", encoding="utf8")


@pytest.mark.parametrize("left_pages", [([], [2, 3]), ([1, 2], [3, 4, 5], [6]),
                                        ([1, 2], [], [3])])
@pytest.mark.parametrize("reuse", [False, True])
@pytest.mark.parametrize("threads", [1, 2])
def test_matrix_multiplication_uses_each_pages_row_count(tmp_path, left_pages, reuse, threads):
  left, right, output = [tmp_path / name for name in ("left.sdds", "right.sdds", "product.sdds")]
  right_pages = [[3]] if reuse else [[index + 3] for index in range(len(left_pages))]
  write_matrix_pages(left, "a", left_pages)
  write_matrix_pages(right, "b", right_pages)
  args = ["-reuse"] if reuse else []
  run("sddsmatrixmult", left, right, output, *args, f"-threads={threads}")

  assert run("sdds2stream", output, "-rows=bare").stdout.split() == [str(len(page)) for page in left_pages]
  expected = [value * (3 if reuse else index + 3)
              for index, page in enumerate(left_pages) for value in page]
  assert [float(value) for value in run("sdds2stream", output, "-column=b").stdout.split()] == expected


def test_matrix_multiplication_checks_later_page_dimensions(tmp_path):
  left, right, output = [tmp_path / name for name in ("left.sdds", "right.sdds", "product.sdds")]
  write_matrix_pages(left, "a", [[1], [2]])
  write_matrix_pages(right, "b", [[3], [4, 5]])

  result = run("sddsmatrixmult", left, right, output, check=False)
  assert result.returncode != 0
  assert "Dimension mismatch" in result.stderr
