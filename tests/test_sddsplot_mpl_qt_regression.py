"""Regression and compatibility tests for sddsplot and mpl_qt.

The normal test run exercises the binaries in ``bin/<platform>``.  To run the
same tests against older builds as well, set ``SDDS_TEST_COMPAT_BIN_DIRS`` to a
path-separator-delimited list of additional binary directories.  Protocol tests
also cross-feed every sddsplot producer into every mpl_qt consumer.
"""

from dataclasses import dataclass
import json
import os
from pathlib import Path
import subprocess

import pytest

from sdds_test_utils import BIN_DIR, IS_WINDOWS, ROOT_DIR


EXECUTABLE_SUFFIX = ".exe" if IS_WINDOWS else ""
EXAMPLE = ROOT_DIR / "SDDSlib" / "demo" / "example.sdds"
SDDSCONVERT = BIN_DIR / "sddsconvert"
SDDSMAKEDATASET = BIN_DIR / "sddsmakedataset"
SDDSSEQUENCE = BIN_DIR / "sddssequence"


@dataclass(frozen=True)
class Toolchain:
  name: str
  bin_dir: Path

  def executable(self, name):
    return self.bin_dir / f"{name}{EXECUTABLE_SUFFIX}"

  @property
  def sddsplot(self):
    return self.executable("sddsplot")

  @property
  def mpl_qt(self):
    return self.executable("mpl_qt")

  @property
  def mpl_qt_3d(self):
    return self.executable("mpl_qt_3d")


def _discover_toolchains():
  directories = [("current", Path(os.fspath(BIN_DIR)))]
  configured = os.environ.get("SDDS_TEST_COMPAT_BIN_DIRS", "")
  directories.extend(
    (f"compat-{index}", Path(value).expanduser())
    for index, value in enumerate(configured.split(os.pathsep), start=1)
    if value
  )

  toolchains = []
  seen = set()
  for name, directory in directories:
    directory = directory.resolve()
    if directory in seen:
      continue
    seen.add(directory)
    toolchain = Toolchain(name, directory)
    missing = [
      path.name for path in (toolchain.sddsplot, toolchain.mpl_qt)
      if not path.exists()
    ]
    if name != "current" and missing:
      raise RuntimeError(
        f"{name} binary directory {directory} is missing: {', '.join(missing)}"
      )
    toolchains.append(toolchain)
  return toolchains


TOOLCHAINS = _discover_toolchains()
CURRENT_TOOLS_AVAILABLE = all(
  path.exists()
  for path in (
    TOOLCHAINS[0].sddsplot,
    TOOLCHAINS[0].mpl_qt,
    SDDSCONVERT,
    SDDSMAKEDATASET,
    SDDSSEQUENCE,
  )
)
requires_plot_tools = pytest.mark.skipif(
  not CURRENT_TOOLS_AVAILABLE,
  reason="sddsplot, mpl_qt, or SDDS data-generation tools not built",
)


def _run(command, **kwargs):
  return subprocess.run(command, check=True, **kwargs)


def _run_json(toolchain, tmp_path, inputs, plot_args, output_stem="plot"):
  output = tmp_path / f"{output_stem}-{toolchain.name}.json"
  _run([
    str(toolchain.sddsplot),
    *(str(path) for path in inputs),
    *plot_args,
    "-device=json",
    f"-output={output}",
  ])
  document = json.loads(output.read_text())
  assert document["schemaVersion"] == "sddsplot-json-1"
  return document


def _run_qt_producer(toolchain, tmp_path, inputs, plot_args, output_stem="plot"):
  output = tmp_path / f"{output_stem}-{toolchain.name}.mpl"
  _run([
    str(toolchain.sddsplot),
    *(str(path) for path in inputs),
    *plot_args,
    "-device=qt",
    f"-output={output}",
  ])
  stream = output.read_bytes()
  assert stream
  return stream


def _parse_mpl_stream(stream):
  """Validate record boundaries without depending on native byte order."""
  payload_sizes = {
    ord("V"): 4,
    ord("M"): 4,
    ord("P"): 4,
    ord("L"): 2,
    ord("W"): 2,
    ord("B"): 10,
    ord("U"): 32,
    ord("G"): 0,
    ord("R"): 0,
    ord("E"): 0,
    ord("C"): 6,
    ord("S"): 16,
  }
  commands = []
  offset = 0
  while offset < len(stream):
    command = stream[offset]
    assert command in payload_sizes, (
      f"unknown mpl_qt command 0x{command:02x} at byte {offset}"
    )
    commands.append(chr(command))
    offset += 1 + payload_sizes[command]
    assert offset <= len(stream), (
      f"truncated {chr(command)!r} record ending at byte {offset} "
      f"of {len(stream)}"
    )
  assert offset == len(stream)
  return commands


def _run_mpl_qt(toolchain, stream, extra_args=(), timeout=30):
  environment = os.environ.copy()
  environment.setdefault("QT_QPA_PLATFORM", "offscreen")
  completed = subprocess.run(
    [str(toolchain.mpl_qt), *extra_args],
    input=stream + b"R",
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    env=environment,
    timeout=timeout,
  )
  diagnostics = completed.stderr.decode(errors="replace")
  assert completed.returncode == 0, diagnostics
  assert "Unknown mpl_qt command" not in diagnostics
  assert "Unexpected end of mpl_qt input" not in diagnostics
  return completed


def _traces(document):
  return [
    trace
    for plot in document["plots"]
    for trace in plot["traces"]
  ]


def _create_sequence(path, rows, pages=1, major_order="column"):
  command = [
    str(SDDSSEQUENCE),
    str(path),
    "-define=Time,type=double",
  ]
  if pages > 1:
    command.extend([f"-repeat={pages}", "-break"])
  command.append(f"-sequence=begin=0,delta=0.25,number={rows}")
  command.append("-define=y,type=double")
  if pages > 1:
    command.extend([f"-repeat={pages}", "-break"])
  command.extend([
    f"-sequence=begin=10,delta=2,number={rows}",
    f"-majorOrder={major_order}",
  ])
  _run(command)


@pytest.fixture(scope="module")
def multipage_inputs(tmp_path_factory):
  directory = tmp_path_factory.mktemp("sddsplot-multipage")
  column_major = directory / "column-major.sdds"
  row_major = directory / "row-major.sdds"
  ascii_file = directory / "ascii.sdds"
  _create_sequence(column_major, rows=257, pages=8, major_order="column")
  _create_sequence(row_major, rows=257, pages=8, major_order="row")
  _run([str(SDDSCONVERT), str(column_major), str(ascii_file), "-ascii"])
  return column_major, row_major, ascii_file


@pytest.fixture(scope="module")
def filtered_input(tmp_path_factory):
  path = tmp_path_factory.mktemp("sddsplot-filter") / "filter.sdds"
  _run([
    str(SDDSMAKEDATASET),
    str(path),
    "-ascii",
    "-column=x,type=double",
    "-data=0,1,2,3,4,5,6,7,8,9",
    "-column=y,type=double",
    "-data=0,10,20,30,40,50,60,70,80,90",
    "-column=rank,type=double",
    "-data=9,8,7,6,5,4,3,2,1,0",
  ])
  return path


@pytest.fixture(scope="module")
def ten_large_files(tmp_path_factory):
  directory = tmp_path_factory.mktemp("sddsplot-ten-files")
  files = []
  for index in range(10):
    path = directory / f"trace-{index:02d}.sdds"
    _run([
      str(SDDSSEQUENCE),
      str(path),
      "-define=Time,type=double",
      "-sequence=begin=0,delta=0.1,number=10000",
      "-define=y,type=double",
      f"-sequence=begin={index * 1000},delta={index + 1},number=10000",
      "-majorOrder=column",
    ])
    files.append(path)
  return files


@requires_plot_tools
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_multipage_columns_append_all_rows(toolchain, tmp_path, multipage_inputs):
  document = _run_json(
    toolchain,
    tmp_path,
    [multipage_inputs[0]],
    ["-columnNames=Time,y"],
    "multipage",
  )
  trace, = _traces(document)
  expected_x = [index * 0.25 for index in range(257)] * 8
  expected_y = [10 + index * 2 for index in range(257)] * 8
  assert trace["x"] == pytest.approx(expected_x)
  assert trace["y"] == pytest.approx(expected_y)


@requires_plot_tools
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_implicit_index_restarts_for_each_page(toolchain, tmp_path, multipage_inputs):
  document = _run_json(
    toolchain,
    tmp_path,
    [multipage_inputs[0]],
    ["-columnNames=y"],
    "implicit-index",
  )
  trace, = _traces(document)
  assert trace["x"] == list(range(257)) * 8
  assert trace["y"] == pytest.approx(
    [10 + index * 2 for index in range(257)] * 8
  )


@requires_plot_tools
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_binary_major_orders_and_ascii_are_equivalent(
  toolchain, tmp_path, multipage_inputs
):
  traces = []
  for index, path in enumerate(multipage_inputs):
    document = _run_json(
      toolchain,
      tmp_path,
      [path],
      ["-columnNames=Time,y"],
      f"encoding-{index}",
    )
    trace, = _traces(document)
    traces.append((trace["x"], trace["y"]))
  assert traces[1] == traces[0]
  assert traces[2] == traces[0]


@requires_plot_tools
@pytest.mark.parametrize(
  ("plot_args", "expected_x", "expected_y"),
  [
    (["-columnNames=x,y", "-filter=column,x,2,8", "-sparse=2"],
     [2, 4, 6, 8], [20, 40, 60, 80]),
    (["-columnNames=x,y", "-sort=rank"],
     list(range(9, -1, -1)), list(range(90, -1, -10))),
  ],
  ids=("filter-and-sparse", "sort"),
)
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_data_selection_transformations(
  toolchain, tmp_path, filtered_input, plot_args, expected_x, expected_y
):
  document = _run_json(
    toolchain, tmp_path, [filtered_input], plot_args, "transform"
  )
  trace, = _traces(document)
  assert trace["x"] == pytest.approx(expected_x)
  assert trace["y"] == pytest.approx(expected_y)


@requires_plot_tools
@pytest.mark.parametrize(
  "plot_args",
  [
    ["-columnNames=shortCol,doubleCol"],
    ["-parameterNames=shortParam,doubleParam"],
    ["-arrayNames=shortArray,ushortArray"],
  ],
  ids=("columns", "parameters", "arrays"),
)
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_columns_parameters_and_arrays_have_finite_paired_data(
  toolchain, tmp_path, plot_args
):
  document = _run_json(toolchain, tmp_path, [EXAMPLE], plot_args, "data-class")
  traces = _traces(document)
  assert traces
  for trace in traces:
    assert trace["x"]
    assert len(trace["x"]) == len(trace["y"])
    assert all(isinstance(value, (int, float)) for value in trace["x"])
    assert all(isinstance(value, (int, float)) for value in trace["y"])


@requires_plot_tools
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_ten_files_with_ten_thousand_rows_each(toolchain, tmp_path, ten_large_files):
  document = _run_json(
    toolchain,
    tmp_path,
    ten_large_files,
    ["-columnNames=Time,y"],
    "ten-large-files",
  )
  traces = _traces(document)
  assert len(traces) == 10
  for index, trace in enumerate(traces):
    assert trace["meta"]["fileIndex"] == index
    assert len(trace["x"]) == 10000
    assert len(trace["y"]) == 10000
    assert trace["x"][0] == pytest.approx(0)
    assert trace["x"][-1] == pytest.approx(999.9)
    assert trace["y"][0] == pytest.approx(index * 1000)
    assert trace["y"][-1] == pytest.approx(
      index * 1000 + 9999 * (index + 1)
    )


@requires_plot_tools
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_repeated_multipage_loads_are_stable(toolchain, tmp_path, multipage_inputs):
  reference = None
  for iteration in range(5):
    document = _run_json(
      toolchain,
      tmp_path,
      [multipage_inputs[0]],
      ["-columnNames=Time,y", "-filter=column,Time,10,40"],
      f"repeat-{iteration}",
    )
    trace, = _traces(document)
    values = (trace["x"], trace["y"])
    if reference is None:
      reference = values
    else:
      assert values == reference


@requires_plot_tools
@pytest.mark.parametrize(
  ("graphic", "required_command"),
  [
    ("line", "V"),
    ("dot", "P"),
    ("symbol", None),
    ("impulse", "V"),
    ("bar", "V"),
  ],
)
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_qt_graphic_modes_produce_valid_consumable_protocol(
  toolchain, tmp_path, multipage_inputs, graphic, required_command
):
  stream = _run_qt_producer(
    toolchain,
    tmp_path,
    [multipage_inputs[0]],
    ["-columnNames=Time,y", f"-graphic={graphic}"],
    f"graphic-{graphic}",
  )
  commands = _parse_mpl_stream(stream)
  assert commands[0] == "G"
  assert "U" in commands
  assert commands[-1] == "E"
  if required_command:
    assert required_command in commands
  _run_mpl_qt(toolchain, stream)


@requires_plot_tools
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_qt_protocol_output_is_deterministic(toolchain, tmp_path, multipage_inputs):
  streams = [
    _run_qt_producer(
      toolchain,
      tmp_path,
      [multipage_inputs[0]],
      ["-columnNames=Time,y", "-graphic=line"],
      f"deterministic-{iteration}",
    )
    for iteration in range(3)
  ]
  assert streams[1:] == streams[:1] * 2


@requires_plot_tools
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_mpl_qt_keep_and_movie_read_multiple_frames(
  toolchain, tmp_path, multipage_inputs
):
  stream = _run_qt_producer(
    toolchain,
    tmp_path,
    [multipage_inputs[0]],
    ["-columnNames=Time,y"],
    "frames",
  )
  _run_mpl_qt(
    toolchain,
    stream * 3,
    extra_args=("-keep", "3", "-timeoutHours", "1"),
  )
  _run_mpl_qt(
    toolchain,
    stream * 3,
    extra_args=("-movie", "1", "-interval", "0", "-timeoutHours", "1"),
  )


@requires_plot_tools
def test_every_mpl_qt_reads_every_sddsplot_protocol(
  tmp_path, multipage_inputs
):
  streams = [
    _run_qt_producer(
      producer,
      tmp_path,
      [multipage_inputs[0]],
      ["-columnNames=Time,y", "-graphic=dot"],
      f"cross-producer-{index}",
    )
    for index, producer in enumerate(TOOLCHAINS)
  ]
  for stream in streams:
    _parse_mpl_stream(stream)
    for consumer in TOOLCHAINS:
      _run_mpl_qt(consumer, stream)


@requires_plot_tools
def test_current_and_compat_json_results_are_identical(
  tmp_path, multipage_inputs, ten_large_files
):
  scenarios = [
    ([multipage_inputs[0]], ["-columnNames=Time,y"], "multipage"),
    (ten_large_files, ["-columnNames=Time,y"], "ten-files"),
  ]
  for inputs, plot_args, name in scenarios:
    documents = [
      _run_json(toolchain, tmp_path, inputs, plot_args, f"compare-{name}")
      for toolchain in TOOLCHAINS
    ]
    assert documents[1:] == documents[:1] * (len(documents) - 1)


@requires_plot_tools
def test_large_protocol_cross_version_compatibility(tmp_path, ten_large_files):
  streams = [
    _run_qt_producer(
      producer,
      tmp_path,
      ten_large_files,
      ["-columnNames=Time,y", "-graphic=line"],
      f"large-producer-{index}",
    )
    for index, producer in enumerate(TOOLCHAINS)
  ]
  for stream in streams:
    commands = _parse_mpl_stream(stream)
    assert commands.count("G") >= 1
    assert commands.count("E") >= 1
    # The optimized producer coalesces repeated device coordinates, so the
    # protocol intentionally contains fewer vectors than source data points.
    assert commands.count("V") >= 30000
    assert len(stream) >= 150000
    for consumer in TOOLCHAINS:
      _run_mpl_qt(consumer, stream, timeout=60)


@requires_plot_tools
@pytest.mark.parametrize("toolchain", TOOLCHAINS, ids=lambda item: item.name)
def test_3d_dispatch_matches_companion_when_split(toolchain):
  if not toolchain.mpl_qt_3d.exists():
    pytest.skip("this build uses the older integrated mpl_qt 3D implementation")
  environment = os.environ.copy()
  environment.setdefault("QT_QPA_PLATFORM", "offscreen")
  forwarded = subprocess.run(
    [str(toolchain.mpl_qt), "-3d"],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    env=environment,
    timeout=10,
  )
  direct = subprocess.run(
    [str(toolchain.mpl_qt_3d), "-3d"],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    env=environment,
    timeout=10,
  )
  assert forwarded.returncode == direct.returncode == 1
  assert forwarded.stdout == direct.stdout
  assert forwarded.stderr == direct.stderr
