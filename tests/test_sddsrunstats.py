import subprocess
from pathlib import Path
import math
import pytest

BIN_DIR = Path("bin/Linux-x86_64")
SDDSRUNSTATS = BIN_DIR / "sddsrunstats"
SDDS2STREAM = BIN_DIR / "sdds2stream"

REQUIRED_TOOLS = [SDDSRUNSTATS, SDDS2STREAM]

@pytest.mark.skipif(not all(tool.exists() for tool in REQUIRED_TOOLS), reason="sddsrunstats or sdds2stream not built")
class TestSddsrunstats:
    def create_input(self, tmp_path):
        text = (
            "SDDS2\n"
            "&parameter name=p, type=long &end\n"
            "&column name=x, type=double &end\n"
            "&column name=y, type=double &end\n"
            "&data mode=ascii &end\n"
            "! page 1\n"
            "0\n"  # parameter value
            "6\n"  # row count
            "0 1\n"
            "1 3\n"
            "2 5\n"
            "3 7\n"
            "4 9\n"
            "5 11\n"
        )
        path = tmp_path / "input.sdds"
        path.write_text(text)
        return path

    def create_small_input(self, tmp_path):
        text = (
            "SDDS2\n"
            "&parameter name=p, type=long &end\n"
            "&column name=x, type=double &end\n"
            "&column name=y, type=double &end\n"
            "&data mode=ascii &end\n"
            "! page 1\n"
            "0\n"  # parameter value
            "2\n"  # row count
            "0 1\n"
            "1 3\n"
        )
        path = tmp_path / "small.sdds"
        path.write_text(text)
        return path

    def read_column(self, path, column):
        result = subprocess.run(
            [str(SDDS2STREAM), f"-column={column}", str(path)],
            capture_output=True,
            text=True,
            check=True,
        )
        return [float(x) for x in result.stdout.strip().split()]

    def test_all_statistics(self, tmp_path):
        inp = self.create_input(tmp_path)
        out = tmp_path / "out.sdds"
        subprocess.run(
            [
                str(SDDSRUNSTATS),
                str(inp),
                str(out),
                "-points=3",
                "-maximum=y",
                "-minimum=y",
                "-mean=y",
                "-median=y",
                "-standardDeviation=y",
                "-rms=y",
                "-sum=power=2,y",
                "-sigma=y",
                "-sample=y",
                "-slope=independent=x,y",
            ],
            check=True,
        )
        windows = [
            [1, 3, 5],
            [3, 5, 7],
            [5, 7, 9],
            [7, 9, 11],
        ]
        means = [sum(w) / len(w) for w in windows]
        medians = [sorted(w)[len(w) // 2] for w in windows]
        mins = [min(w) for w in windows]
        maxs = [max(w) for w in windows]
        stdevs = []
        rms = []
        sums2 = []
        sigmas = []
        samples = []
        for w, m in zip(windows, means):
            st = math.sqrt(sum((v - m) ** 2 for v in w) / (len(w) - 1))
            stdevs.append(st)
            rms.append(math.sqrt(sum(v * v for v in w) / len(w)))
            sums2.append(sum(v * v for v in w))
            sigmas.append(st / math.sqrt(len(w)))
            samples.append(w[0])
        slopes = [2.0] * len(windows)

        assert self.read_column(out, "yMax") == pytest.approx(maxs)
        assert self.read_column(out, "yMin") == pytest.approx(mins)
        assert self.read_column(out, "yMean") == pytest.approx(means)
        assert self.read_column(out, "yMedian") == pytest.approx(medians)
        assert self.read_column(out, "yStDev") == pytest.approx(stdevs)
        assert self.read_column(out, "yRMS") == pytest.approx(rms)
        assert self.read_column(out, "ySum") == pytest.approx(sums2)
        assert self.read_column(out, "ySigma") == pytest.approx(sigmas)
        assert self.read_column(out, "y") == pytest.approx(samples)
        assert self.read_column(out, "ySlope") == pytest.approx(slopes)

    def test_no_overlap(self, tmp_path):
        inp = self.create_input(tmp_path)
        out = tmp_path / "nooverlap.sdds"
        subprocess.run(
            [
                str(SDDSRUNSTATS),
                str(inp),
                str(out),
                "-points=3",
                "-noOverlap",
                "-mean=y",
            ],
            check=True,
        )
        assert self.read_column(out, "yMean") == pytest.approx([3.0, 9.0])

    def test_window(self, tmp_path):
        inp = self.create_input(tmp_path)
        out = tmp_path / "window.sdds"
        subprocess.run(
            [
                str(SDDSRUNSTATS),
                str(inp),
                str(out),
                "-window=column=x,width=3",
                "-mean=y",
            ],
            check=True,
        )
        assert self.read_column(out, "yMean") == pytest.approx([4.0, 6.0, 8.0, 9.0, 10.0])

    def test_partial_ok(self, tmp_path):
        inp = self.create_small_input(tmp_path)
        out = tmp_path / "partial.sdds"
        subprocess.run(
            [
                str(SDDSRUNSTATS),
                str(inp),
                str(out),
                "-points=3",
                "-partialOk",
                "-mean=y",
            ],
            check=True,
        )
        assert self.read_column(out, "yMean") == pytest.approx([2.0])

    def test_major_order_row(self, tmp_path):
        inp = self.create_input(tmp_path)
        out = tmp_path / "rowmajor.sdds"
        subprocess.run(
            [
                str(SDDSRUNSTATS),
                str(inp),
                str(out),
                "-points=3",
                "-mean=y",
                "-majorOrder=row",
            ],
            check=True,
        )
        assert self.read_column(out, "yMean") == pytest.approx([3.0, 5.0, 7.0, 9.0])

    def test_pipe(self, tmp_path):
        inp = self.create_input(tmp_path)
        data = inp.read_bytes()
        result = subprocess.run(
            [
                str(SDDSRUNSTATS),
                "-points=3",
                "-mean=y",
                "-pipe=in,out",
            ],
            input=data,
            stdout=subprocess.PIPE,
            check=True,
        )
        out = tmp_path / "pipe.sdds"
        out.write_bytes(result.stdout)
        assert self.read_column(out, "yMean") == pytest.approx([3.0, 5.0, 7.0, 9.0])

    def test_points_zero(self, tmp_path):
        inp = self.create_input(tmp_path)
        out = tmp_path / "points0.sdds"
        subprocess.run(
            [
                str(SDDSRUNSTATS),
                str(inp),
                str(out),
                "-points=0",
                "-mean=y",
            ],
            check=True,
        )
        assert self.read_column(out, "yMean") == pytest.approx([6.0])
