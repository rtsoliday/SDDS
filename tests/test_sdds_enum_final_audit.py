"""Regressions for remaining ENUM read failures and the legacy name reader."""

import subprocess

import pytest

from sdds_test_utils import BIN_DIR, IS_WINDOWS, PLATFORM_ID, ROOT_DIR
from test_sdds_enum_read_failures import ENCODINGS, input_path, naming_file, page_count, run
from test_sdds_enum_specialized import assert_enum_failure, write_dataset


OPTIONS = {
  "sdds2dfft": ("-columns=x,y",),
  "sddsexpfit": ("-columns=x,y",),
  "sddsgfit": ("-columns=x,y",),
  "sddslorentzianfit": ("-columns=x,y",),
  "sddssinefit": ("-columns=x,y",),
  "sddsnaff": ("-columns=x,y", "-terminateSearch=frequencies=1"),
  "sddsshiftcor": ("-columns=x", "-with=y", "-scan=start=-1,end=1,delta=1"),
  "sddsgenericfit": ("-equation=x a *",
                     "-variable=name=a,lowerLimit=0,upperLimit=5,stepsize=0.1,startingValue=1",
                     "-ycolumn=y"),
  "sddsmultihist": ("-columns=x,y", "-abscissa=bin", "-bins=3"),
  "sddssampledist": ("-columns=independentVariable=x,cdf=y", "-samples=5", "-seed=123"),
  "sddseventhist": ("-dataColumn=x", "-eventIdentifier=event", "-bins=3"),
  "sddsrespmatrixderivative": ("-mode=cor",),
  "elegant2genesis": ("-slices=2", "-totalCharge=1e-9"),
}
SINGLE_PAGE = ("sddseventhist", "sddsrespmatrixderivative")
CONVOLVE_OPTIONS = ("-signalColumns=x,y", "-responseColumns=x,y", "-outputColumns=x,conv")


def configuration(tool):
  if tool == "sddseventhist":
    return [], [], [("event", "string"), ("x", "double")], [("A", 0), ("B", 1), ("A", 2), ("B", 3)]
  if tool == "sddsrespmatrixderivative":
    return [], [], [("BPMName", "string"), ("C1", "double"), ("C2", "double")], [("B1", 1, 2), ("B2", 3, 4)]
  if tool == "elegant2genesis":
    columns = [(n, "double") for n in ("x", "xp", "y", "yp", "t", "p", "particleID")]
    rows = [((i % 7 - 3) * 1e-4, (i % 11 - 5) * 1e-5, (i % 13 - 6) * 1e-4,
             (i % 17 - 8) * 1e-5, i * 1e-12, 1000 + (i % 5) * .01, i + 1) for i in range(40)]
    return [], [], columns, rows
  if tool == "sddssampledist":
    return [], [], [("x", "double"), ("y", "double")], [(0, 0), (1, .25), (2, .5), (3, 1)]
  return [], [], [("x", "double"), ("y", "double")], [(0, 1), (1, 2), (2, 1), (3, 3), (4, 2)]


@pytest.mark.parametrize("tool", OPTIONS)
@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
def test_audited_tools_report_enum_read_errors(tmp_path, tool, encoding, kind, prefix):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  write_dataset(source, encoding, configuration(tool), pages=prefix, invalid=kind)
  assert_enum_failure(run(tool, source, output, *OPTIONS[tool], check=False))
  if tool in ("elegant2genesis", "sddsrespmatrixderivative") and output.exists():
    assert page_count(output) == 0  # Do not publish a result computed from a valid prefix.


@pytest.mark.parametrize("tool", OPTIONS)
@pytest.mark.parametrize("encoding", ENCODINGS)
def test_audited_tools_accept_valid_enum_eof(tmp_path, tool, encoding):
  source, output = input_path(tmp_path, encoding), tmp_path / "output.sdds"
  pages = 1 if tool in SINGLE_PAGE else 2
  write_dataset(source, encoding, configuration(tool), pages=pages)
  result = run(tool, source, output, *OPTIONS[tool])
  assert not result.stderr
  assert run("sddscheck", output).stdout.strip() == "ok"
  assert page_count(output) == (1 if tool == "elegant2genesis" else pages)
  if tool == "sddssampledist":
    assert run("sdds2stream", output, "-parameter=Mode", "-noquotes").stdout.split() == ["ON", "OFF"]


@pytest.mark.parametrize("tool", ["sddseventhist", "sddsrespmatrixderivative", "elegant2genesis"])
def test_empty_input_does_not_clean_up_uninitialized_output(tmp_path, tool):
  source = tmp_path / "empty.sdds"
  write_dataset(source, "ascii", configuration(tool), pages=0)
  result = run(tool, source, tmp_path / "output.sdds", *OPTIONS[tool], check=False)
  assert result.returncode > 0, result.stdout + result.stderr
  assert "No data pages" in result.stderr


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("side", ["signal", "response"])
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
@pytest.mark.parametrize("in_place", [False, True])
def test_convolve_enum_errors_preserve_input(tmp_path, encoding, side, kind, prefix, in_place):
  signal = input_path(tmp_path, encoding)
  response, output = tmp_path / ("response" + ''.join(signal.suffixes)), tmp_path / "output.sdds"
  config = configuration("sddsconvolve")
  for name, source in (("signal", signal), ("response", response)):
    write_dataset(source, encoding, config, pages=prefix if side == name else 2,
                  invalid=kind if side == name else None)
  original = signal.read_bytes()
  result = run("sddsconvolve", signal, response, *([] if in_place else [output]),
               *CONVOLVE_OPTIONS, check=False)
  assert_enum_failure(result)
  assert signal.read_bytes() == original
  assert not signal.with_name(signal.name + "~").exists()


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_convolve_reuse_reports_first_response_error(tmp_path, encoding, kind):
  signal, response, output = tmp_path / "signal.sdds", input_path(tmp_path, encoding), tmp_path / "output.sdds"
  write_dataset(signal, "ascii", configuration("sddsconvolve"), pages=2)
  write_dataset(response, encoding, configuration("sddsconvolve"), pages=0, invalid=kind)
  assert_enum_failure(run("sddsconvolve", signal, response, output, *CONVOLVE_OPTIONS, "-reuse", check=False))


@pytest.mark.parametrize("reuse", [False, True])
@pytest.mark.parametrize("response_pages", [0, 1, 2])
def test_convolve_keeps_normal_response_eof_behavior(tmp_path, reuse, response_pages):
  signal, response, output = tmp_path / "signal.sdds", tmp_path / "response.sdds", tmp_path / "output.sdds"
  for source, pages in ((signal, 2), (response, response_pages)):
    write_dataset(source, "ascii", configuration("sddsconvolve"), pages=pages)
  result = run("sddsconvolve", signal, response, output, *CONVOLVE_OPTIONS,
               *(["-reuse"] if reuse else []), check=False)
  if reuse and not response_pages:
    assert result.returncode > 0
  else:
    assert result.returncode == 0, result.stderr
    assert page_count(output) == (2 if reuse else response_pages)


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
def test_derivative_error_preserves_inplace_input(tmp_path, encoding, kind, prefix):
  source = input_path(tmp_path, encoding)
  write_dataset(source, encoding, configuration("sddsrespmatrixderivative"), pages=prefix, invalid=kind)
  original = source.read_bytes()
  assert_enum_failure(run("sddsrespmatrixderivative", source, "-mode=cor", check=False))
  assert source.read_bytes() == original


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
@pytest.mark.parametrize("threads", [1, 2])
@pytest.mark.parametrize("prefix", [0, 1])
@pytest.mark.parametrize("early_eof", [False, True])
def test_sampledist_worker_errors_override_other_input_eof(tmp_path, encoding, kind, threads, prefix, early_eof):
  source, output = tmp_path / "source.sdds", tmp_path / "output.sdds"
  bad, other = input_path(tmp_path, encoding), tmp_path / "other.sdds"
  config = configuration("sddssampledist")
  write_dataset(source, "ascii", config, pages=2)
  write_dataset(other, "ascii", config, pages=prefix if early_eof else 2)
  write_dataset(bad, encoding, config, pages=prefix, invalid=kind)
  result = run("sddssampledist", source, output, *OPTIONS["sddssampledist"],
               f"-columns=independentVariable=x,cdf=y,output=other,datafile={other}",
               f"-columns=independentVariable=x,cdf=y,output=bad,datafile={bad}",
               f"-threads={threads}", check=False)
  assert_enum_failure(result)
  assert str(bad) in result.stderr
  assert page_count(output) <= prefix


@pytest.mark.parametrize("threads", [1, 2])
@pytest.mark.parametrize("bad", [None, "column", "parameter", "array"])
def test_sampledist_datafile_only(tmp_path, threads, bad):
  source, other, output = tmp_path / "distribution.sdds", tmp_path / "other.sdds", tmp_path / "output.sdds"
  config = configuration("sddssampledist")
  write_dataset(source, "ascii", config, pages=0 if bad else 1, invalid=bad)
  write_dataset(other, "ascii", config)
  result = run("sddssampledist", output, "-samples=5", "-seed=123", f"-threads={threads}",
               f"-columns=independentVariable=x,cdf=y,output=a,datafile={other}",
               f"-columns=independentVariable=x,cdf=y,output=b,datafile={source}", check=False)
  if bad:
    assert_enum_failure(result)
  else:
    assert result.returncode == 0, result.stderr
    assert page_count(output) == 1


@pytest.mark.parametrize("kind", [None, "column", "parameter", "array"])
@pytest.mark.parametrize("prefix", [0, 1])
def test_sampledist_reads_main_input_from_pipe(tmp_path, kind, prefix):
  source, output = tmp_path / "input.sdds", tmp_path / "output.sdds"
  write_dataset(source, "ascii", configuration("sddssampledist"), pages=prefix if kind else 2, invalid=kind)
  result = subprocess.run([str(BIN_DIR / "sddssampledist"), str(output), "-pipe=input",
                           *OPTIONS["sddssampledist"]], input=source.read_bytes(), capture_output=True, timeout=30)
  if kind:
    assert result.returncode > 0, result.stderr
    assert b"enum" in result.stderr.lower() and b"99" in result.stderr
  else:
    assert result.returncode == 0, result.stderr
    assert page_count(output) == 2


@pytest.fixture(scope="module")
def feedback_names_driver():
  component = ROOT_DIR / "SDDSaps/pseudoInverse"
  build = subprocess.run(["make", "-C", str(component), "enum-name-test-driver"], capture_output=True, text=True)
  assert build.returncode == 0, build.stdout + build.stderr
  return component / f"O.{PLATFORM_ID}" / ("test_feedback_names.exe" if IS_WINDOWS else "test_feedback_names")


def feedback_names(driver, source, column="state"):
  return subprocess.run([str(driver), str(source), column], capture_output=True, text=True, timeout=30)


@pytest.mark.parametrize("kind", ["string", "enum"])
@pytest.mark.parametrize("encoding", ["ascii", "big-column"])
def test_feedback_names_decode_labels(feedback_names_driver, tmp_path, kind, encoding):
  source = tmp_path / "matrix.sdds"
  naming_file(source, kind, encoding, labels=("BPM1", "BPM2"))
  result = feedback_names(feedback_names_driver, source)
  assert result.returncode == 0, result.stderr
  assert result.stdout.splitlines() == ["BPM1", "BPM2"]


@pytest.mark.parametrize("encoding", ENCODINGS)
@pytest.mark.parametrize("kind", ["column", "parameter", "array"])
def test_feedback_names_reject_invalid_enum(feedback_names_driver, tmp_path, encoding, kind):
  source = input_path(tmp_path, encoding)
  write_dataset(source, encoding, configuration("sddsconvolve"), pages=0, invalid=kind)
  assert_enum_failure(feedback_names(feedback_names_driver, source, "State"))


@pytest.mark.parametrize("kind", ["long", "double", "character"])
def test_feedback_names_reject_numeric_columns(feedback_names_driver, tmp_path, kind):
  source = tmp_path / "matrix.sdds"
  source.write_text(f'SDDS1\n&column name=state, type={kind} &end\n&data mode=ascii &end\n1\n1\n')
  result = feedback_names(feedback_names_driver, source)
  assert result.returncode > 0
  assert "string or enum" in result.stderr


def test_feedback_names_reject_empty_input(feedback_names_driver, tmp_path):
  source = tmp_path / "empty.sdds"
  write_dataset(source, "ascii", configuration("sddsconvolve"), pages=0)
  assert feedback_names(feedback_names_driver, source, "State").returncode > 0
