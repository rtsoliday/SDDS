#!/usr/bin/env python3
"""Run the representative C/C++ numeric benchmark and enforce the release gate."""

import json
import statistics
import subprocess
import sys
from pathlib import Path


def run(program: Path, mode: str, path: Path) -> dict:
    completed = subprocess.run(
        [str(program), mode, str(path)], check=True, capture_output=True, text=True
    )
    return json.loads(completed.stdout)


def median_result(runs: list[dict]) -> dict:
    return {
        "seconds": statistics.median(item["seconds"] for item in runs),
        "peak_kib": statistics.median(item["peak_kib"] for item in runs),
    }


def main() -> int:
    if len(sys.argv) != 3:
        return 2
    program = Path(sys.argv[1]).resolve()
    data = Path(sys.argv[2]).resolve()
    c_runs = []
    cpp_runs = []
    for iteration in range(5):
        if iteration % 2:
            cpp_runs.append(run(program, "cpp-full", data))
            c_runs.append(run(program, "c-full", data))
        else:
            c_runs.append(run(program, "c-full", data))
            cpp_runs.append(run(program, "cpp-full", data))
    c_result = median_result(c_runs)
    cpp_result = median_result(cpp_runs)
    c_write_runs = []
    cpp_write_runs = []
    for iteration in range(5):
        if iteration % 2:
            cpp_write_runs.append(run(program, "cpp-write", data.parent / "gate-cpp-write.sdds"))
            c_write_runs.append(run(program, "c-write", data.parent / "gate-c-write.sdds"))
        else:
            c_write_runs.append(run(program, "c-write", data.parent / "gate-c-write.sdds"))
            cpp_write_runs.append(run(program, "cpp-write", data.parent / "gate-cpp-write.sdds"))
    c_write = median_result(c_write_runs)
    cpp_write = median_result(cpp_write_runs)
    read_throughput_ratio = c_result["seconds"] / cpp_result["seconds"]
    read_memory_ratio = cpp_result["peak_kib"] / max(c_result["peak_kib"], 1)
    write_throughput_ratio = c_write["seconds"] / cpp_write["seconds"]
    write_memory_ratio = cpp_write["peak_kib"] / max(c_write["peak_kib"], 1)
    report = {
        "c": c_result,
        "cpp": cpp_result,
        "c_write": c_write,
        "cpp_write": cpp_write,
        "cpp_read_throughput_ratio": read_throughput_ratio,
        "cpp_read_peak_memory_ratio": read_memory_ratio,
        "cpp_write_throughput_ratio": write_throughput_ratio,
        "cpp_write_peak_memory_ratio": write_memory_ratio,
        "throughput_gate": 0.85,
        "memory_gate": 1.25,
    }
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if (read_throughput_ratio >= 0.85 and write_throughput_ratio >= 0.85 and
                 read_memory_ratio <= 1.25 and write_memory_ratio <= 1.25) else 1


if __name__ == "__main__":
    raise SystemExit(main())
