import subprocess
from pathlib import Path
import re
import pytest

BIN_DIR = Path('bin/Linux-x86_64')
SDDSSHIFTCOR = BIN_DIR / 'sddsshiftcor'
SDDS2STREAM = BIN_DIR / 'sdds2stream'

# extract option names from source
text = Path('SDDSaps/sddsshiftcor.c').read_text()
match = re.search(r"char \*option\[N_OPTIONS\] = \{([^}]*)\};", text, re.S)
option_names = [name.strip().strip('"') for name in match.group(1).split(',') if name.strip()]

OPTION_ARGS = {
    'columns': ['-columns=shifted'],
    'excludecolumns': ['-excludeColumns=junk'],
    'with': [],
    'pipe': ['-pipe=output'],
    'rankorder': ['-rankOrder'],
    'stdevoutlier': ['-columns=shiftedOut', '-stDevOutlier=limit=1.5,passes=1'],
    'scan': [],
    'verbose': ['-verbose'],
    'majorOrder': ['-majorOrder=column'],
}

assert set(option_names) == set(OPTION_ARGS)

REQUIRED_TOOLS = [SDDSSHIFTCOR, SDDS2STREAM]
pytestmark = pytest.mark.skipif(not all(t.exists() for t in REQUIRED_TOOLS), reason='required tools not built')

def create_input(tmp_path: Path) -> Path:
    content = (
        "SDDS1\n"
        "&column name=ref, type=double &end\n"
        "&column name=shifted, type=double &end\n"
        "&column name=shiftedOut, type=double &end\n"
        "&column name=junk, type=double &end\n"
        "&data mode=ascii, no_row_counts=1 &end\n"
        "1 0 0 5\n"
        "3 1 1 5\n"
        "2 3 3 5\n"
        "5 2 2 5\n"
        "4 5 50 5\n"
    )
    path = tmp_path / 'input.sdds'
    path.write_text(content)
    return path

def read_correlation(path: Path, column: str):
    result = subprocess.run(
        [str(SDDS2STREAM), f'-column=ShiftAmount,{column}', str(path)],
        capture_output=True,
        text=True,
        check=True,
    )
    data = []
    for line in result.stdout.strip().splitlines():
        shift, value = line.split()
        data.append((int(float(shift)), float(value)))
    return data

@pytest.mark.parametrize('option', option_names)
def test_sddsshiftcor_options(tmp_path, option):
    inp = create_input(tmp_path)
    out = tmp_path / 'out.sdds'
    base = [str(SDDSSHIFTCOR), str(inp)]
    args = ['-with=ref', '-scan=start=-1,end=1,delta=1'] + OPTION_ARGS[option]

    if option == 'stdevoutlier':
        # baseline without stdev option
        subprocess.run(base + [str(out)] + ['-with=ref', '-scan=start=-1,end=1,delta=1', '-columns=shiftedOut'], check=True)
        baseline = read_correlation(out, 'shiftedOutShiftedCor')
        base_corr = next(c for s, c in baseline if s == 1)
        assert base_corr < 1.0

    if '-pipe=output' in OPTION_ARGS[option]:
        cmd = base + args
        with out.open('wb') as f:
            subprocess.run(cmd, stdout=f, check=True)
    else:
        capture = option == 'verbose'
        result = subprocess.run(base + [str(out)] + args, check=True, capture_output=capture, text=capture)
        if option == 'verbose':
            assert 'Working on shift of 1' in result.stderr

    column = 'shiftedOutShiftedCor' if option == 'stdevoutlier' else 'shiftedShiftedCor'
    data = read_correlation(out, column)
    corr = next(c for s, c in data if s == 1)
    expected = 1.0
    if option == 'rankorder':
        expected = 0.9827076298239908
    assert corr == pytest.approx(expected, rel=1e-12)

    if option == 'columns':
        with pytest.raises(subprocess.CalledProcessError):
            subprocess.run([str(SDDS2STREAM), '-column=shiftedOutShiftedCor', str(out)], check=True, capture_output=True, text=True)
    if option == 'excludecolumns':
        with pytest.raises(subprocess.CalledProcessError):
            subprocess.run([str(SDDS2STREAM), '-column=junkShiftedCor', str(out)], check=True, capture_output=True, text=True)
    if option == 'stdevoutlier':
        assert base_corr == pytest.approx(0.8940578219, rel=1e-6)
        assert corr > base_corr
