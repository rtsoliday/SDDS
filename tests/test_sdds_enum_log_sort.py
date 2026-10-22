"""ENUM log assignments and sort-mode validation independent of page contents."""

import gzip
import os
import signal
import socket
import struct
import subprocess
import time

import pytest

from sdds_test_utils import BIN_DIR, IS_WINDOWS


MAPPINGS = (
  "0:OFF,10:ON,7:READY,-3:FAULT,-2147483648:LOW,2147483647:HIGH",
  "7:READY,10:ON,-3:FAULT,-2147483648:LOW,2147483647:HIGH",
)


def run(tool, *args, check=True, **kwargs):
  executable = BIN_DIR / tool
  if not executable.exists():
    pytest.skip(f"{tool} not built")
  result = subprocess.run([str(executable), *map(str, args)],
                          capture_output=True, timeout=20, **kwargs)
  if check:
    assert result.returncode == 0, result.stderr.decode(errors="replace")
  return result


@pytest.fixture
def log_server(tmp_path):
  """Run the existing binary directly; always disconnect its child and stop it."""
  executable = BIN_DIR / "sddslogserver"
  if IS_WINDOWS or not executable.exists():
    pytest.skip("sddslogserver requires a non-Windows build")
  with socket.socket() as reservation:
    reservation.bind(("127.0.0.1", 0))
    port = reservation.getsockname()[1]
  log_path = tmp_path / "server.log"
  connection = None
  with log_path.open("wb") as log:
    server = subprocess.Popen([str(executable), f"-port={port}", f"-root={tmp_path}"],
                              stdout=log, stderr=subprocess.STDOUT)
    try:
      deadline = time.monotonic() + 5
      while b"Waiting for new socket connection" not in log_path.read_bytes():
        assert server.poll() is None, log_path.read_text(errors="replace")
        assert time.monotonic() < deadline, log_path.read_text(errors="replace")
        time.sleep(0.01)
      connection = socket.create_connection(("127.0.0.1", port), timeout=5)

      def submit(channel, value):
        connection.sendall(f"addValue:{channel},{value}\n".encode())
        reply = b""
        while reply != b"ok" and not reply.endswith(b"\n"):
          part = connection.recv(2048)
          assert part, log_path.read_text(errors="replace")
          reply += part
        return reply

      yield submit
    finally:
      if connection is not None:
        try:
          connection.sendall(b"disconnect\n")
          connection.recv(1)  # Wait for the child to close its socket.
        except OSError:
          pass
        finally:
          connection.close()
      if server.poll() is None:
        server.send_signal(signal.SIGINT)
      try:
        server.wait(timeout=5)
      except subprocess.TimeoutExpired:
        server.kill()
        server.wait(timeout=5)


def make_log(path, mapping, encoding, value_type="enum"):
  options = [path, "-column=SampleIDNumber,type=long64", "-data=0",
             "-column=Time,type=double", "-data=0",
             f"-column=State,type={value_type}", "-data=10"]
  if value_type == "enum":
    options.append(f"-enumValues=column,State,{mapping}")
  else:
    # Keep the numeric control in SDDS6, including its declared binary byte order.
    options.extend(["-parameter=Mode,type=enum", "-data=10",
                    f"-enumValues=parameter,Mode,{MAPPINGS[0]}"])
  if encoding == "ascii":
    options.append("-ascii")
  environment = dict(os.environ, SDDS_OUTPUT_ENDIANESS=encoding if encoding != "ascii" else "little")
  run("sddsmakedataset", *options, env=environment)


def column_values(path, name, enum_codes=False):
  return run("sdds2stream", path, f"-column={name}", "-noquotes",
             *(["-enumCodes"] if enum_codes else [])).stdout.decode().split()


@pytest.mark.parametrize("mapping", MAPPINGS)
@pytest.mark.parametrize("encoding", ["ascii", "little", "big"])
def test_logserver_enum_assignments_preserve_codes(log_server, tmp_path, mapping, encoding):
  path = tmp_path / "State.sdds"
  make_log(path, mapping, encoding)
  codes = [10]
  for code in [7, -3, -2147483648, 2147483647] + ([0] if mapping.startswith("0:") else []):
    assert log_server("State", code) == b"ok"
    codes.append(code)
    assert column_values(path, "State", enum_codes=True) == list(map(str, codes))
  assert column_values(path, "SampleIDNumber") == list(map(str, range(len(codes))))
  assert column_values(path, "State") == ["ON", "READY", "FAULT", "LOW", "HIGH"] + (
    ["OFF"] if mapping.startswith("0:") else [])
  assert run("sddscheck", path).stdout.strip() == b"ok"
  assert path.read_bytes().startswith(b"SDDS6\n")
  assert mapping.encode() in path.read_bytes().split(b"&data", 1)[0]


@pytest.mark.parametrize("mapping", MAPPINGS)
@pytest.mark.parametrize("encoding", ["ascii", "little", "big"])
def test_logserver_rejected_enum_samples_preserve_file(log_server, tmp_path, mapping, encoding):
  path = tmp_path / "State.sdds"
  make_log(path, mapping, encoding)
  original = path.read_bytes()
  invalid = ["99", "2147483648", "-2147483649", "bad", "7 junk", "+7", "7,other", ""]
  if not mapping.startswith("0:"):
    invalid.append("0")
  for value in invalid:
    assert log_server("State", value).startswith(b"error:")
    assert path.read_bytes() == original
  # Rejection releases the file lock and leaves the connection usable.
  assert log_server("State", 7) == b"ok"
  assert column_values(path, "State", enum_codes=True) == ["10", "7"]


@pytest.mark.parametrize("encoding", ["ascii", "little", "big"])
def test_logserver_long_control(log_server, tmp_path, encoding):
  path = tmp_path / "State.sdds"
  make_log(path, None, encoding, value_type="long")
  assert log_server("State", 0) == b"ok"
  assert column_values(path, "State") == ["10", "0"]


def sort_input(path, encoding, keys):
  binary = encoding != "ascii"
  order = ">" if encoding == "big" else "<"
  mapping = "10:zeta,-2147483648:alpha,2147483647:beta"
  header = ("SDDS6\n&column name=key, type=long &end\n"
            f'&column name=state, type=enum, enum_values="{mapping}" &end\n'
            f'&data mode={"binary" if binary else "ascii"}, '
            f'endian={"big" if order == ">" else "little"} &end\n').encode()
  wire = header
  if keys is not None:
    codes = [-2147483648, 10, 2147483647][:len(keys)]
    if binary:
      wire += struct.pack(order + "i", len(keys))
      wire += b"".join(struct.pack(order + "ii", key, code) for key, code in zip(keys, codes))
    else:
      wire += (f"{len(keys)}\n" + "".join(f"{key} {code}\n" for key, code in zip(keys, codes))).encode()
  path.write_bytes(gzip.compress(wire) if encoding == "gzip" else wire)


@pytest.mark.parametrize("encoding", ["ascii", "little", "big", "gzip"])
@pytest.mark.parametrize("keys", [None, [], [1], [1, 2], [1, 1]],
                         ids=["no-pages", "empty-page", "one-row", "distinct-keys", "tied-keys"])
@pytest.mark.parametrize("mode", ["absolute-primary", "absolute-secondary", "non-dominated"])
@pytest.mark.parametrize("output_mode", ["file", "in-place", "pipe"])
def test_enum_numeric_sort_modes_reject_before_output(tmp_path, encoding, keys, mode, output_mode):
  source = tmp_path / ("input.sdds.gz" if encoding == "gzip" else "input.sdds")
  output = tmp_path / "output.sdds"
  sort_input(source, encoding, keys)
  original = source.read_bytes()
  output.write_bytes(b"existing output must survive\n")
  options = {
    "absolute-primary": ["-column=state,absolute"],
    "absolute-secondary": ["-column=key", "-column=state,absolute"],
    "non-dominated": ["-column=key", "-column=state", "-nonDominateSort"],
  }[mode]
  destination = {"file": [output], "in-place": [], "pipe": ["-pipe=output"]}[output_mode]
  result = run("sddssort", source, *destination, *options, check=False)
  assert result.returncode > 0
  assert b"enum" in result.stderr.lower() and b"cast" in result.stderr.lower()
  assert result.stdout == b""
  assert source.read_bytes() == original
  assert output.read_bytes() == b"existing output must survive\n"


@pytest.mark.parametrize("encoding", ["ascii", "little", "big", "gzip"])
@pytest.mark.parametrize("options,expected", [
  (["-column=state"], [-2147483648, 2147483647, 10]),
  (["-column=state,decreasing"], [10, 2147483647, -2147483648]),
  (["-column=key,absolute"], [10, 2147483647, -2147483648]),
])
def test_enum_label_sort_and_unrelated_numeric_sort(tmp_path, encoding, options, expected):
  source = tmp_path / ("input.sdds.gz" if encoding == "gzip" else "input.sdds")
  output = tmp_path / "sorted.sdds"
  sort_input(source, encoding, [-3, 1, -2])
  run("sddssort", source, output, *options)
  assert column_values(output, "state", enum_codes=True) == list(map(str, expected))
  assert run("sddscheck", output).stdout.strip() == b"ok"
  assert b'enum_values="10:zeta,-2147483648:alpha,2147483647:beta"' in output.read_bytes()
