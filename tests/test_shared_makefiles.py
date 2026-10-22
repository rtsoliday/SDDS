"""Behavioral checks for the shared Make interface using real native builds.

Generated programs stay under SDDS/tests/artifacts, alongside their build logs.
No compiler wrappers or runtime injection are used.
"""

from pathlib import Path
import filecmp
import os
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile

import pytest


ROOT = Path(__file__).resolve().parents[1]
pytestmark = pytest.mark.skipif(
    platform.system() not in ("Linux", "Darwin"), reason="Native Unix build checks"
)


def shared_source(name):
    """Locate a sibling repository or component; SDDS is always this checkout."""
    repository, _, component = name.partition("/")
    root = ROOT if repository == "SDDS" else ROOT.parent / repository
    return root / component if component else root


def require_current_shared_rules(source, repository):
    """Checks of newer shared behaviour skip siblings not yet synchronized."""
    for name in ("Makefile.rules", "Makefile.build"):
        if not filecmp.cmp(source / name, ROOT / name, shallow=False):
            pytest.skip(f"{repository} has not synchronized the shared {name}")


@pytest.fixture
def build_tree():
    artifacts = ROOT / "tests" / "artifacts"
    artifacts.mkdir(exist_ok=True)
    tree = Path(tempfile.mkdtemp(prefix="shared-make-", dir=artifacts))
    (tree / "main.cc").write_text(
        'extern "C" int answer(void);\n'
        'int bonus();\n'
        'int main() { return answer() == EXPECTED && bonus() == 7 ? 0 : 1; }\n'
    )
    (tree / "extra.cc").write_text("int bonus() { return 7; }\n")
    (tree / "answer.c").write_text("int answer(void) { return EXPECTED; }\n")
    (tree / "Makefile").write_text(
        ".DEFAULT_GOAL := all\n"
        f"include {ROOT}/Makefile.rules\n"
        "PROD = sample\n"
        "sample_SRC = main.cc extra.cc\n"
        "LIBRARY = answer\n"
        "LIBRARY_SRC = answer.c\n"
        "PROD_LIBS = -lanswer\n"
        "LIB_LINK_DIRS = -L$(LIB_DIR)\n"
        "CPPFLAGS += -DEXPECTED=$(VALUE)\n"
        "BUILD_GENERATED += $(OBJ_DIR)/generated.h\n"
        "$(OBJ_DIR)/main.o: $(OBJ_DIR)/generated.h\n"
        "$(OBJ_DIR)/generated.h: | $(OBJ_DIR)\n"
        "\tprintf '%s\\n' '// generated build configuration' > $@\n"
        f"include {ROOT}/Makefile.build\n"
    )
    yield tree
    # Keep failed build inputs and logs available for diagnosis.


def run_make(tree, *arguments, expect_success=True, use_output_defaults=False):
    # macOS ships Make 3.81; these checks use newer GNU Make features.
    command = [*shlex.split(os.environ.get("MAKE", shutil.which("gmake") or "make")), "-j4"]
    if not use_output_defaults:
        command += [
            f"BIN_DIR={tree}/bin", f"LIB_DIR={tree}/lib",
            "LIB_LINK_DIRS=-L" + str(tree / "lib"),
        ]
    command += ["OBJ_DIR=O.test", "VALUE=42", *arguments]
    # Each fixture owns its parallel build; do not inherit a parent's jobserver.
    environment = os.environ.copy()
    for name in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
        environment.pop(name, None)
    result = subprocess.run(command, cwd=tree, text=True, capture_output=True, env=environment)
    with (tree / "build.log").open("a") as log:
        log.write("$ " + " ".join(command) + "\n" + result.stdout + result.stderr)
    if expect_success:
        assert result.returncode == 0, result.stdout + result.stderr
    return result


def query_make_variables(tree, variables, *arguments):
    with (tree / "Makefile").open("a") as makefile:
        makefile.write("\n.PHONY: print-discovery\nprint-discovery:\n")
        for variable in variables:
            makefile.write(f"\t$(info {variable}=$({variable}))\n")
        makefile.write("\t@:\n")
    result = run_make(tree, "print-discovery", *arguments)
    assert not result.stderr, result.stderr
    values = {}
    for line in result.stdout.splitlines():
        name, separator, value = line.partition("=")
        if separator and name in variables:
            values[name] = value
    assert set(values) == set(variables), result.stdout
    return values


def test_product_library_order_and_install_restoration(build_tree):
    """A clean parallel link waits for the local archive's installed copy."""
    tree = build_tree
    run_make(tree)
    program = tree / "O.test" / "sample"
    installed = tree / "bin" / "sample"
    archive = tree / "lib" / "libanswer.a"
    assert installed.read_bytes() == program.read_bytes()
    subprocess.run([str(installed)], check=True)
    installed.unlink()
    archive.unlink()
    run_make(tree)
    assert installed.read_bytes() == program.read_bytes()
    assert archive.is_file()
    # Reinstalling an executable alone must not relink it.
    original_time = program.stat().st_mtime_ns
    installed.unlink()
    run_make(tree)
    assert program.stat().st_mtime_ns == original_time
    assert installed.is_file()
    # Switching object trees must restore the selected library even when its
    # archive is older than the copy installed by the other configuration.
    run_make(tree, "OBJ_DIR=O.other", "VALUE=43")
    subprocess.run([str(installed)], check=True)
    run_make(tree)
    subprocess.run([str(installed)], check=True)


def test_mixed_library_sources_and_header_rebuild(build_tree):
    """Nested C, cc and cpp library sources compile and retain header dependencies."""
    tree = build_tree
    nested = tree / "nested"
    nested.mkdir()
    header = nested / "value.h"
    header.write_text("#define HEADER_OFFSET 0\n")
    (nested / "answer.c").write_text(
        '#include "value.h"\n'
        "int library_bonus(void);\nint library_extra(void);\n"
        "int answer(void) { return EXPECTED + HEADER_OFFSET + library_bonus() - 7 + library_extra() - 9; }\n"
    )
    (nested / "bonus.cc").write_text('extern "C" int library_bonus(void) { return 7; }\n')
    (nested / "extra.cpp").write_text('extern "C" int library_extra(void) { return 9; }\n')
    makefile = tree / "Makefile"
    makefile.write_text(makefile.read_text().replace(
        "LIBRARY_SRC = answer.c", "LIBRARY_SRC = nested/answer.c nested/bonus.cc nested/extra.cpp"
    ))
    run_make(tree)
    program = tree / "bin" / "sample"
    subprocess.run([str(program)], check=True)
    objects = [tree / "O.test" / "nested" / name for name in ("answer.o", "bonus.o", "extra.o")]
    assert all(path.is_file() for path in objects)
    times = [path.stat().st_mtime_ns for path in objects]
    run_make(tree)
    assert [path.stat().st_mtime_ns for path in objects] == times
    header.write_text("#define HEADER_OFFSET 1\n")
    run_make(tree)
    assert objects[0].stat().st_mtime_ns > times[0]
    assert [path.stat().st_mtime_ns for path in objects[1:]] == times[1:]
    assert subprocess.run([str(program)]).returncode == 1


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("kind", ("dll", "static", "python"))
def test_windows_library_objects_and_install_recipes(build_tree, repository, kind):
    """Library object names, MSVC links and DLL installation retain their modes."""
    source = shared_source(repository)
    if not (source / "Makefile.build").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    (tree / "nested").mkdir()
    (tree / "nested" / "part.cpp").write_text("int part() { return 1; }\n")
    makefile = tree / "Makefile"
    content = makefile.read_text().replace(str(ROOT), str(source)).replace(
        "LIBRARY_SRC = answer.c", "LIBRARY_SRC = answer.c extra.cc nested/part.cpp"
    )
    if kind == "python":
        content = content.replace("LIBRARY = answer", "LIBRARY =\nPYTHON_SHARED_LIBRARY = answer")
    makefile.write_text(content)
    arguments = [
        "OS=Windows", "ARCH=x86_64", "PROD=", "EPICS_REQUIRED_LIBS=",
        f"NO_DLL={int(kind == 'static')}",
    ]
    values = query_make_variables(
        tree, ("LIBRARY_OBJS", "LIBRARY", "PYTHON_SHARED_LIBRARY", "DLL", "EXP", "DEPFLAGS"),
        *arguments,
    )
    assert values["LIBRARY_OBJS"].split() == ["O.test/answer.obj", "O.test/extra.obj", "O.test/nested/part.obj"]
    assert values["DLL"] == "O.test/answer.dll"
    assert values["EXP"] == "O.test/answer.exp"
    assert values["DEPFLAGS"] == ""
    assert values["PYTHON_SHARED_LIBRARY" if kind == "python" else "LIBRARY"] == "O.test/answer.lib"
    # Inspect MSVC commands without trying to execute Windows tools on Unix.
    result = run_make(tree, "-n", "install-libraries", *arguments)
    linked_output = "O.test/answer.lib" if kind == "static" else "O.test/answer.dll"
    assert f"-out:{linked_output}" in result.stdout
    if kind == "static":
        assert "-implib:" not in result.stdout
        assert "cp -f O.test/answer.dll" not in result.stdout
    else:
        assert "-implib:O.test/answer.lib" in result.stdout
        assert f"cp -f O.test/answer.dll {tree}/bin/" in result.stdout


@pytest.mark.parametrize("goal", ("install-programs", "install-libraries"))
def test_bundled_file_install_restoration(build_tree, goal):
    """Bundled files restore missing/stale copies and leave matching copies untouched."""
    tree = build_tree
    payload = tree / "runtime.data"
    license_file = tree / "LICENSE.runtime.txt"
    payload.write_text("runtime payload fixture\n")
    license_file.write_text("runtime license fixture\n")
    destination = tree / "extensions" / "lib" / "test-arch"
    arguments = [
        "PROD=", "LIBRARY=", f"EPICS_LIB_DIR={destination}",
        f"BUNDLED_RUNTIME_FILES={payload} {license_file}",
    ]
    run_make(tree, goal, *arguments)
    installed = [destination / path.name for path in (payload, license_file)]
    assert [path.read_bytes() for path in installed] == [payload.read_bytes(), license_file.read_bytes()]
    times = [path.stat().st_mtime_ns for path in installed]
    run_make(tree, goal, *arguments)
    assert [path.stat().st_mtime_ns for path in installed] == times
    installed[0].unlink()
    installed[1].write_text("outdated installed license\n")
    run_make(tree, goal, *arguments)
    assert [path.read_bytes() for path in installed] == [payload.read_bytes(), license_file.read_bytes()]
    payload.write_text("updated runtime payload fixture\n")
    license_file.write_text("updated runtime license fixture\n")
    run_make(tree, goal, *arguments)
    assert [path.read_bytes() for path in installed] == [payload.read_bytes(), license_file.read_bytes()]


def test_darwin_program_install_replaces_signed_file(build_tree):
    """macOS program installation keeps the temporary-file replacement sequence."""
    tree = build_tree
    destination = tree / "extensions" / "bin" / "darwin-test"
    result = run_make(
        tree, "-n", "install-programs", "OS=Darwin", "ARCH=x86_64",
        f"EPICS_BIN_DIR={destination}",
    )
    for directory in (tree / "bin", destination):
        assert f"install -m 755 O.test/sample {directory}/sample.new && mv -f {directory}/sample.new {directory}/sample" in result.stdout


def test_configuration_changes_and_noop_build(build_tree):
    """Changed flags invalidate objects/generated files; unchanged flags do not."""
    tree = build_tree
    run_make(tree)
    outputs = [tree / "O.test" / name for name in ("main.o", "answer.o", "generated.h")]
    times = [path.stat().st_mtime_ns for path in outputs]
    run_make(tree)
    assert [path.stat().st_mtime_ns for path in outputs] == times
    run_make(tree, "VALUE=43")
    assert all(path.stat().st_mtime_ns > before for path, before in zip(outputs, times))
    subprocess.run([str(tree / "bin" / "sample")], check=True)


def test_custom_link_and_target_flags(build_tree):
    """A local link recipe and target-specific flags survive shared finalization."""
    tree = build_tree
    makefile = tree / "Makefile"
    content = makefile.read_text().replace(
        f"include {ROOT}/Makefile.build",
        "CUSTOM_LINK_PRODUCTS = sample\n"
        "$(OBJ_DIR)/main.o: CXXFLAGS += -DLOCAL_FLAG=1\n"
        "$(OBJ_DIR)/sample: $(OBJ_DIR)/main.o $(OBJ_DIR)/extra.o $(LIB_DIR)/libanswer.a\n"
        "\t$(CXX) -o $@ $(OBJ_DIR)/main.o $(OBJ_DIR)/extra.o $(LIB_DIR)/libanswer.a\n"
        f"include {ROOT}/Makefile.build",
    )
    makefile.write_text(content)
    source = tree / "main.cc"
    source.write_text("#ifndef LOCAL_FLAG\n#error Missing local flags\n#endif\n" + source.read_text())
    run_make(tree, "DEV_BUILD=1")
    result = subprocess.run([str(tree / "bin" / "sample")])
    assert result.returncode == 0
    assert "overriding recipe" not in (tree / "build.log").read_text()


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("architecture", ("x86_64", "aarch64", "arm64"))
def test_linux_architecture_flags(build_tree, repository, architecture):
    """ARM compilation, linking and Fortran must not inherit Intel options."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    makefile = tree / "Makefile"
    makefile.write_text(makefile.read_text().replace(str(ROOT), str(source)))
    values = query_make_variables(
        tree, ("CFLAGS", "CXXFLAGS", "LDFLAGS", "F77"),
        "OS=Linux", f"ARCH={architecture}", "EPICS_REQUIRED_LIBS=",
        "CUSTOM_GCC_PATH=", "ARM64_CPU_FLAGS=-mcpu=cortex-a72",
    )
    for name, value in values.items():
        assert ("-m64" in value.split()) == (architecture == "x86_64"), (name, value)
    for name in ("CFLAGS", "CXXFLAGS"):
        flags = values[name].split()
        assert ("-mtune=generic" in flags) == (architecture == "x86_64")
        assert ("-mcpu=cortex-a72" in flags) == (architecture != "x86_64")


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("development", (False, True), ids=("release", "development"))
@pytest.mark.parametrize("local_override", (False, True), ids=("shared-qt-flags", "local-qt-flags"))
def test_windows_qt_flags(build_tree, repository, development, local_override):
    """Qt requirements and local replacements survive developer finalization."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    makefile = tree / "Makefile"
    content = makefile.read_text().replace(str(ROOT), str(source))
    if local_override:
        content = content.replace(
            f"include {source}/Makefile.build",
            "CXXFLAGS = /O2 /MD /EHsc /permissive- /Zc:lambda /Zc:__cplusplus /utf-8 /std:c++17 /DLOCAL_QT_FLAGS\n"
            f"include {source}/Makefile.build",
        )
    makefile.write_text(content)
    values = query_make_variables(
        tree, ("CXXFLAGS", "LINKEXE"), "OS=Windows", "ARCH=x86_64",
        "QT_MODULES=Core Widgets", "EPICS_REQUIRED_LIBS=",
        f"DEV_BUILD={int(development)}",
    )
    flags = values["CXXFLAGS"].split()
    assert {"/permissive-", "/Zc:lambda", "/Zc:__cplusplus", "/utf-8", "/std:c++17"} <= set(flags)
    assert ("/DLOCAL_QT_FLAGS" in flags) == local_override
    assert ("/Od" in flags) == development
    assert ("/Zi" in flags) == development
    if development:
        assert not any(flag.startswith(("-O", "/O")) and flag != "/Od" for flag in flags)
        assert "-GL" not in flags
        assert "-LTCG" not in values["LINKEXE"].split()


def test_local_flags_after_rules_in_development_build(build_tree):
    """A component can replace shared C++ flags before the final developer pass."""
    tree = build_tree
    makefile = tree / "Makefile"
    makefile.write_text(makefile.read_text().replace(
        f"include {ROOT}/Makefile.build",
        "CXXFLAGS = -O3 -DLOCAL_COMPONENT_FLAGS $(CXX_STD_FLAGS)\n"
        f"include {ROOT}/Makefile.build",
    ))
    main = tree / "main.cc"
    main.write_text(
        "#ifndef LOCAL_COMPONENT_FLAGS\n#error Local compiler flags were lost\n#endif\n"
        + main.read_text()
    )
    run_make(tree, "DEV_BUILD=1")
    subprocess.run([str(tree / "bin" / "sample")], check=True)
    log = (tree / "build.log").read_text()
    assert "-Og -g1" in log
    assert "-O3 -DLOCAL_COMPONENT_FLAGS" not in log


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
def test_discovered_dependency_headers(build_tree, repository):
    """A real compile can use discovered dependency headers in every repository."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    dependency = tree / "dependency"
    (dependency / "lib").mkdir(parents=True)
    (dependency / "include").mkdir()
    (dependency / "include" / "discovered.h").write_text("#define DISCOVERED_HEADER 1\n")
    makefile = tree / "Makefile"
    makefile.write_text(
        makefile.read_text().replace(str(ROOT), str(source)).replace(
            "CPPFLAGS += -DEXPECTED=$(VALUE)\n",
            "CPPFLAGS += -DEXPECTED=$(VALUE) $(EXTRA_INC_DIRS)\n",
        )
    )
    main = tree / "main.cc"
    main.write_text(
        '#include <discovered.h>\n'
        '#ifndef DISCOVERED_HEADER\n#error Dependency headers were not found\n#endif\n'
        + main.read_text()
    )
    run_make(
        tree, "EPICS_REQUIRED_LIBS=",
        "AR=ar rcs", "RANLIB=ranlib", f"GSL_LIB={dependency}/lib/libgsl.a",
        "Z_LIB=", "LZMA_LIB=", "HDF5_LIB=", "FFTW3_LIB=", "SZ_LIB=", "GSLCBLAS_LIB=",
    )
    subprocess.run([str(tree / "bin" / "sample")], check=True)


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("windows", (False, True), ids=("native-metadata", "windows-metadata"))
def test_python_metadata(build_tree, repository, windows):
    """The real interpreter supplies native and Windows extension metadata."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    makefile = tree / "Makefile"
    makefile.write_text(makefile.read_text().replace(str(ROOT), str(source)))
    arguments = ["EPICS_REQUIRED_LIBS=", f"PYTHON={sys.executable}"]
    if windows:
        arguments += ["OS=Windows", "ARCH=x86_64"]
    values = query_make_variables(
        tree, ("PYTHON", "PYTHON_PREFIX", "PYTHON_EXEC_PREFIX", "PYTHON_VERSION", "PYTHON_INCLUDE_DIRS"),
        *arguments,
    )
    prefix = sys.prefix.replace("\\", "/") if windows else sys.prefix
    exec_prefix = sys.exec_prefix.replace("\\", "/") if windows else sys.exec_prefix
    version = f"{sys.version_info.major}.{sys.version_info.minor}"
    assert values["PYTHON"] == sys.executable
    assert values["PYTHON_PREFIX"] == prefix
    assert values["PYTHON_EXEC_PREFIX"] == exec_prefix
    assert values["PYTHON_VERSION"] == version
    includes = [f"-I{prefix}/include"] if windows else [
        f"-I{prefix}/include/python{version}", f"-I{prefix}/include/python{version}m",
        f"-I{exec_prefix}/include/python{version}", f"-I{exec_prefix}/include/python{version}m",
    ]
    assert values["PYTHON_INCLUDE_DIRS"].split() == includes


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
def test_windows_mpi_discovery_without_python(build_tree, repository):
    """Windows SDK discovery does not need to run a Python interpreter."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    # Empty SDK markers describe availability only; nothing compiles or links.
    sdk = tree / "C:" / "MicrosoftMPI"
    (sdk / "Include").mkdir(parents=True)
    (sdk / "Lib" / "x64").mkdir(parents=True)
    (sdk / "Include" / "mpi.h").touch()
    (sdk / "Lib" / "x64" / "msmpi.lib").touch()
    makefile = tree / "Makefile"
    makefile.write_text(makefile.read_text().replace(str(ROOT), str(source)))
    values = query_make_variables(
        tree, ("WIN_MPI", "MPI_CC", "WIN_MPI_INC", "WIN_MPI_LIB"),
        "OS=Windows", "ARCH=x86_64",
        "EPICS_REQUIRED_LIBS=", "PYTHON=not-required-for-mpi-discovery",
    )
    assert values["WIN_MPI"] == "C:/MicrosoftMPI"
    assert values["MPI_CC"] == "FOUND"
    assert values["WIN_MPI_INC"] == "-IC:/MicrosoftMPI/Include"
    assert values["WIN_MPI_LIB"] == "C:/MicrosoftMPI/Lib/x64/msmpi.lib"


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("layout", ("runtime-only", "default-sdk", "sdk-environment", "missing-sdk-header"))
def test_windows_mpi_sdk_discovery(build_tree, repository, layout, monkeypatch):
    """MS-MPI needs SDK headers; SDK paths with spaces stay single arguments."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    require_current_shared_rules(source, repository)
    for name in ("MPI_CC", "MSMPI_INC", "MSMPI_LIB64", "WIN_MPI_SEARCH"):
        monkeypatch.delenv(name, raising=False)
    tree = build_tree
    makefile = tree / "Makefile"
    makefile.write_text(makefile.read_text().replace(str(ROOT), str(source)))
    # Empty SDK markers describe availability only; nothing compiles or links.
    (tree / "C:" / "Program Files" / "Microsoft MPI" / "Bin").mkdir(parents=True)
    sdk = tree / "C:" / "Program Files (x86)" / "Microsoft SDKs" / "MPI"
    if layout == "sdk-environment":
        sdk = tree / "MS MPI SDK"
        monkeypatch.setenv("MSMPI_INC", "MS MPI SDK\\Include\\")
        monkeypatch.setenv("MSMPI_LIB64", "MS MPI SDK\\Lib\\x64\\")
    if layout != "runtime-only":
        (sdk / "Include").mkdir(parents=True)
        (sdk / "Lib" / "x64").mkdir(parents=True)
        (sdk / "Lib" / "x64" / "msmpi.lib").touch()
        if layout != "missing-sdk-header":
            (sdk / "Include" / "mpi.h").touch()
    values = query_make_variables(
        tree, ("MPI_CC", "WIN_MPI_INC", "WIN_MPI_LIB"),
        "OS=Windows", "ARCH=x86_64",
        "EPICS_REQUIRED_LIBS=", "PYTHON=not-required-for-mpi-discovery",
    )
    if layout in ("runtime-only", "missing-sdk-header"):
        assert values == {"MPI_CC": "", "WIN_MPI_INC": "", "WIN_MPI_LIB": ""}
        return
    root = "MS MPI SDK" if layout == "sdk-environment" else "C:/Program Files (x86)/Microsoft SDKs/MPI"
    assert values["MPI_CC"] == "FOUND"
    assert values["WIN_MPI_INC"] == f'-I"{root}/Include"'
    assert values["WIN_MPI_LIB"] == f'"{root}/Lib/x64/msmpi.lib"'


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
def test_environment_compiler_settings_are_ignored(build_tree, repository, monkeypatch):
    """Ambient compiler settings (for example Conda's) cannot replace the defaults."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    require_current_shared_rules(source, repository)
    tree = build_tree
    makefile = tree / "Makefile"
    makefile.write_text(makefile.read_text().replace(str(ROOT), str(source)))
    main = tree / "main.cc"
    main.write_text(
        "#ifdef LEAKED_ENVIRONMENT_FLAGS\n#error Environment compiler flags were used\n#endif\n"
        "#ifndef EXTRA_ENVIRONMENT_FLAGS\n#error EXTRA_CXXFLAGS were lost\n#endif\n"
        + main.read_text()
    )
    environment_prefix = tree / "conda-environment"
    monkeypatch.setenv("CC", str(environment_prefix / "bin" / "x86_64-conda-linux-gnu-cc"))
    monkeypatch.setenv("CXX", str(environment_prefix / "bin" / "x86_64-conda-linux-gnu-c++"))
    monkeypatch.setenv("CPPFLAGS", f"-DLEAKED_ENVIRONMENT_FLAGS -isystem {environment_prefix}/include")
    monkeypatch.setenv("CXXFLAGS", "-O0 -DLEAKED_ENVIRONMENT_FLAGS")
    monkeypatch.setenv("LDLIBS", "-lleaked_environment_library")
    # EXTRA_* remains the documented way to add flags.
    monkeypatch.setenv("EXTRA_CXXFLAGS", "-DEXTRA_ENVIRONMENT_FLAGS")
    run_make(tree, "EPICS_REQUIRED_LIBS=", "AR=ar rcs", "RANLIB=ranlib")
    subprocess.run([str(tree / "bin" / "sample")], check=True)
    values = query_make_variables(
        tree, ("CC", "CXX", "CPPFLAGS", "CXXFLAGS", "LDLIBS"),
        "EPICS_REQUIRED_LIBS=", "CC=command-line-cc",
    )
    assert values["CC"] == "command-line-cc"
    assert "conda" not in values["CXX"]
    assert "LEAKED" not in values["CPPFLAGS"] + values["CXXFLAGS"]
    assert {"-O3", "-g", "-Wall"} <= set(values["CXXFLAGS"].split())
    assert values["LDLIBS"] == ""


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
def test_dry_run_and_question_mode_after_build(build_tree, repository):
    """An unchanged configuration leaves objects, archives and links current."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    require_current_shared_rules(source, repository)
    tree = build_tree
    makefile = tree / "Makefile"
    makefile.write_text(makefile.read_text().replace(str(ROOT), str(source)))
    arguments = ("EPICS_REQUIRED_LIBS=", "AR=ar rcs", "RANLIB=ranlib")
    run_make(tree, *arguments)
    outputs = ["O.test/main.o", "O.test/libanswer.a", f"{tree}/lib/libanswer.a", "O.test/sample"]
    result = run_make(tree, "-n", *arguments)
    for command in (" -c ", "printf ", "ar rcs", "-o O.test/sample"):
        assert command not in result.stdout, result.stdout
    assert run_make(tree, "-q", *outputs, *arguments, expect_success=False).returncode == 0
    # A changed configuration is still reported and rebuilt.
    result = run_make(tree, "-n", "VALUE=43", *arguments)
    assert "-c main.cc" in result.stdout
    assert run_make(tree, "-q", *outputs, "VALUE=43", *arguments, expect_success=False).returncode == 1


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("track_makefiles", (True, False), ids=("default", "opt-out"))
def test_makefile_changes_rebuild_custom_outputs(build_tree, repository, track_makefiles):
    """Recipe edits rebuild custom objects/generated headers without flag changes."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    (tree / "main.cc").write_text(
        '#include "generated.h"\n'
        'extern "C" int answer(void);\n'
        'int bonus();\n'
        'int main() { return answer() == EXPECTED && bonus() == EXPECTED_BONUS ? 0 : 1; }\n'
    )
    (tree / "extra.cc").write_text("int bonus() { return RECIPE_BONUS; }\n")
    makefile = tree / "Makefile"
    makefile.write_text(
        ".DEFAULT_GOAL := all\n"
        f"include {source}/Makefile.rules\n"
        "PROD = sample\n"
        "sample_SRC = main.cc extra.cc\n"
        "LIBRARY = answer\n"
        "LIBRARY_SRC = answer.c\n"
        "PROD_LIBS = -lanswer\n"
        "CPPFLAGS += -DEXPECTED=$(VALUE) -I$(OBJ_DIR)\n"
        "BUILD_GENERATED += $(OBJ_DIR)/generated.h\n"
        "$(OBJ_DIR)/main.o: $(OBJ_DIR)/generated.h\n"
        "$(OBJ_DIR)/extra.o: extra.cc | $(OBJ_DIR)\n"
        "\t$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DEPFLAGS) -DRECIPE_BONUS=7 -c $< $(OUTPUT)\n"
        "$(OBJ_DIR)/generated.h: | $(OBJ_DIR)\n"
        "\tprintf '%s\\n' '#define EXPECTED_BONUS 7' > $@\n"
        f"include {source}/Makefile.build\n"
    )
    arguments = ["EPICS_REQUIRED_LIBS=", "AR=ar rcs", "RANLIB=ranlib"]
    if not track_makefiles:
        arguments.append("BUILD_TRACK_MAKEFILES=0")
    outputs = [tree / "O.test" / name for name in ("extra.o", "generated.h", "main.o", "answer.o")]
    program = tree / "bin" / "sample"
    run_make(tree, *arguments)
    subprocess.run([str(program)], check=True)
    times = [path.stat().st_mtime_ns for path in outputs]
    run_make(tree, *arguments)
    assert [path.stat().st_mtime_ns for path in outputs] == times

    # Both edits change recipes only; the recorded configuration stays equal.
    makefile.write_text(
        makefile.read_text().replace("RECIPE_BONUS=7", "RECIPE_BONUS=8")
        .replace("EXPECTED_BONUS 7", "EXPECTED_BONUS 8")
    )
    run_make(tree, *arguments)
    updated = [path.stat().st_mtime_ns for path in outputs]
    if track_makefiles:
        assert all(after > before for after, before in zip(updated, times))
    else:
        assert updated == times
    expected = 8 if track_makefiles else 7
    assert (tree / "O.test" / "generated.h").read_text() == f"#define EXPECTED_BONUS {expected}\n"
    subprocess.run([str(program)], check=True)

    # Configuration changes must still rebuild everything when tracking is off.
    run_make(tree, *arguments, "VALUE=43")
    assert all(path.stat().st_mtime_ns > before for path, before in zip(outputs, updated))
    subprocess.run([str(program)], check=True)


@pytest.mark.skipif(platform.system() != "Linux", reason="ELF runtime path check")
@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
def test_gsl_link_helper_keeps_missing_deployment_path(build_tree, repository):
    """GSL consumers retain the deployment runtime path without a local copy."""
    inspector = shutil.which("readelf")
    if not inspector:
        pytest.skip("readelf is required to inspect the linked ELF runtime path")
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    site_runtime = tree / "site-runtime-not-installed"
    assert not site_runtime.exists()
    makefile = tree / "Makefile"
    makefile.write_text(
        makefile.read_text().replace(str(ROOT), str(source)).replace(
            "PROD_LIBS = -lanswer\n",
            "PROD_LIBS =\nGSL_PROD = sample\n"
            "sample_DEPS = $(LIB_DIR)/libanswer.a\n",
        )
    )
    # The fixture archive supplies the GSL link slot, keeping this test
    # independent of installed GSL versions while exercising the real linker.
    run_make(
        tree, "EPICS_REQUIRED_LIBS=", "AR=ar rcs", "RANLIB=ranlib",
        f"GSL_LIB={tree}/lib/libanswer.a", "GSLCBLAS_LIB=",
        f"GSL_COMPAT_LIB_DIR={site_runtime}",
    )
    program = tree / "bin" / "sample"
    result = subprocess.run([inspector, "-d", str(program)], text=True, capture_output=True, check=True)
    paths = [line for line in result.stdout.splitlines() if "(RUNPATH)" in line or "(RPATH)" in line]
    assert any(str(site_runtime) in line for line in paths), result.stdout
    assert not site_runtime.exists()
    subprocess.run([str(program)], check=True)


@pytest.mark.skipif(platform.system() != "Linux", reason="ELF runtime path check")
@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("helper", ("OPENSSL_LINK_LIBS", "OPENSSL_CRYPTO_LINK_LIBS"))
def test_openssl_link_helpers_keep_missing_deployment_path(build_tree, repository, helper):
    """Both OpenSSL link modes retain the runtime path without installed copies."""
    inspector = shutil.which("readelf")
    if not inspector:
        pytest.skip("readelf is required to inspect the linked ELF runtime path")
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    site_runtime = tree / "openssl-runtime-not-installed"
    assert not site_runtime.exists()
    makefile = tree / "Makefile"
    makefile.write_text(
        makefile.read_text().replace(str(ROOT), str(source)).replace(
            "PROD_LIBS = -lanswer\n",
            f"PROD_LIBS = $({helper})\n"
            "sample_DEPS = $(LIB_DIR)/libanswer.a\n",
        )
    )
    # Supply the fixture archive through the selected link mode, without using
    # installed OpenSSL. Crypto-only links must not consume the SSL library.
    archive = tree / "lib" / "libanswer.a"
    ssl_library = archive if helper == "OPENSSL_LINK_LIBS" else tree / "must-not-link-ssl.a"
    crypto_library = archive if helper == "OPENSSL_CRYPTO_LINK_LIBS" else ""
    run_make(
        tree, "EPICS_REQUIRED_LIBS=", "AR=ar rcs", "RANLIB=ranlib",
        f"SSL_LIB={ssl_library}", f"CRYPTO_LIB={crypto_library}",
        f"OPENSSL_COMPAT_LIB_DIR={site_runtime}",
    )
    program = tree / "bin" / "sample"
    result = subprocess.run([inspector, "-d", str(program)], text=True, capture_output=True, check=True)
    paths = [line for line in result.stdout.splitlines() if "(RUNPATH)" in line or "(RPATH)" in line]
    assert any(str(site_runtime) in line for line in paths), result.stdout
    assert not site_runtime.exists()
    subprocess.run([str(program)], check=True)


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
def test_motif_extension_first_lookup(build_tree, repository):
    """Motif and static-Motif dependencies prefer shared libraries across directories."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    tree = build_tree
    first, second = tree / "first", tree / "second"
    first.mkdir()
    second.mkdir()
    extension = "dylib" if platform.system() == "Darwin" else "so"
    archive = first / "libXm.a"
    shared = second / f"libXm.{extension}"
    dependency = second / f"libXrandr.{extension}"
    archive.touch()
    shared.touch()
    (first / "libXrandr.a").touch()
    dependency.touch()
    (tree / "query.mk").write_text(
        f"include {source}/Makefile.rules\n"
        "$(info selected=$(XM_LIB))\n"
        "$(info static-deps=$(MOTIF_STATIC_DEP_LIBS))\n"
        "query:\n\t@:\n"
    )
    arguments = (
        "-f", "query.mk", "query", "EPICS_REQUIRED_LIBS=",
        f"MOTIF_LIB_DIR={first}", f"X11_LIB_DIR={second}",
        f"MOTIF_INC={tree}/include", f"X11_INC={tree}/include",
        f"LIB_DIRS={first} {second}",
    )
    result = run_make(tree, *arguments)
    assert "selected=" + str(shared) in result.stdout
    shared.unlink()
    result = run_make(tree, *arguments)
    assert "selected=" + str(archive) in result.stdout
    assert "static-deps=" + str(dependency) in result.stdout


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("redirect_install", (False, True), ids=("default", "site-override"))
def test_configured_extensions_install(build_tree, repository, redirect_install):
    """Every repository installs native outputs using the extensions configuration."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    top = build_tree / "extensions"
    configure = top / "configure"
    configure.mkdir(parents=True)
    (configure / "CONFIG").touch()
    (configure / "RELEASE").write_text("EPICS_BASE =\n")
    destination = top
    if redirect_install:
        # The architecture-specific site setting takes precedence over the
        # generic site setting and RELEASE.local.
        (configure / "RELEASE.local").write_text(
            f"INSTALL_LOCATION_EXTENSIONS = {build_tree}/release-install\n"
        )
        (configure / "CONFIG_SITE").write_text(
            f"INSTALL_LOCATION_EXTENSIONS = {build_tree}/site-install\n"
        )
        site_os = configure / "os"
        site_os.mkdir()
        destination = build_tree / "arch-install"
        (site_os / "CONFIG_SITE.shared-test.shared-test").write_text(
            f"INSTALL_LOCATION_EXTENSIONS = {destination}\n"
        )

    package = top / "src" / repository
    component = package / "component"
    component.mkdir(parents=True)
    for name in ("Makefile.rules", "Makefile.build"):
        shutil.copyfile(source / name, package / name)
    for name in ("main.cc", "extra.cc", "answer.c"):
        shutil.copyfile(build_tree / name, component / name)
    (component / "sample.h").write_text("int answer(void);\n")
    makefile = (build_tree / "Makefile").read_text()
    makefile = makefile.replace(str(ROOT), "..")
    makefile = makefile.replace("PROD = sample\n", "PROD = sample\nINC = sample.h\n")
    (component / "Makefile").write_text(makefile)

    # This synthetic program uses no EPICS APIs. Check installation separately
    # from the repository dependency policy, and supply its archive tools.
    run_make(
        component, "EPICS_HOST_ARCH=shared-test", "EPICS_REQUIRED_LIBS=",
        "AR=ar rcs", "RANLIB=ranlib",
        use_output_defaults=True,
    )
    installed = destination / "bin" / "shared-test" / "sample"
    archive = destination / "lib" / "shared-test" / "libanswer.a"
    header = destination / "include" / "sample.h"
    host = f"{platform.system()}-{platform.machine()}"
    assert installed.read_bytes() == (package / "bin" / host / "sample").read_bytes()
    assert archive.read_bytes() == (package / "lib" / host / "libanswer.a").read_bytes()
    assert header.read_text() == (component / "sample.h").read_text()
    subprocess.run([str(installed)], check=True)

    # Matching archives are not recopied; deleted extensions outputs are restored.
    local_archive = package / "lib" / host / "libanswer.a"
    outputs = [component / "O.test" / "sample", local_archive, archive]
    times = [path.stat().st_mtime_ns for path in outputs]
    run_make(
        component, "EPICS_HOST_ARCH=shared-test", "EPICS_REQUIRED_LIBS=",
        "AR=ar rcs", "RANLIB=ranlib", use_output_defaults=True,
    )
    assert [path.stat().st_mtime_ns for path in outputs] == times
    installed.unlink()
    archive.unlink()
    run_make(
        component, "EPICS_HOST_ARCH=shared-test", "EPICS_REQUIRED_LIBS=",
        "AR=ar rcs", "RANLIB=ranlib", use_output_defaults=True,
    )
    assert installed.read_bytes() == outputs[0].read_bytes()
    assert archive.read_bytes() == local_archive.read_bytes()
    assert [path.stat().st_mtime_ns for path in outputs[:2]] == times[:2]


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("scenario", ("missing-base", "missing-libraries", "clean"))
def test_shared_rules_do_not_require_epics(build_tree, repository, scenario):
    """Shared defaults do not impose EPICS on callers that declare no dependency."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    base = build_tree / "epics-base"
    if scenario == "missing-libraries":
        (base / "include").mkdir(parents=True)
        (base / "include" / "cadef.h").touch()
    goal = "clean" if scenario == "clean" else "all"
    result = run_make(
        build_tree, "-f", str(source / "Makefile.rules"),
        f"--eval={goal}: ; @:", goal,
        f"EPICS_BASE={base}", "EPICS_HOST_ARCH=shared-test",
    )
    assert result.returncode == 0


@pytest.mark.parametrize("component,required", (
    ("StripTool/striptool", "ca Com"),
    ("StripTool/qtstriptool", "ca Com"),
    ("probe/probe", "ca Com"),
    ("probe/qtprobe", "ca Com"),
    ("qtalh/alh", "ca Com"),
    ("qtalh/qtalh", "pvAccess pvData ca Com"),
    ("SDDS/SDDSlib", ""),
    ("SDDS-EPICS/logDaemon", "Com"),
    ("SDDS-EPICS/rampload", "ca Com"),
    ("SDDS-EPICS/oagca", "pvaClient nt pvAccessCA pvAccess pvData ca Com"),
    ("SDDS-EPICS/runcontrol", "pvaClient nt pvAccessCA pvAccess pvData ca Com"),
    ("SDDS-EPICS/SDDSepics", "pvaClient nt pvAccessCA pvAccess pvData ca Com"),
    ("medm/printUtils", ""),
    ("medm/xc", "Com"),
    ("medm/medm", "ca Com"),
    ("medm/qtedm", "pvaClient nt pvAccessCA pvAccess pvData ca Com"),
))
@pytest.mark.parametrize("windows", (False, True), ids=("native", "windows"))
@pytest.mark.parametrize("scenario", ("missing-base", "missing-libraries", "minimal-inventory"))
def test_component_epics_requirements(build_tree, component, required, windows, scenario):
    """Real component Makefiles accept minimal SDK inventories and reject missing dependencies."""
    source = shared_source(component)
    if not (source / "Makefile").is_file():
        pytest.skip(f"Sibling component {component} is not available")
    tree = build_tree
    base = tree / "epics-base"
    if scenario != "missing-base":
        (base / "include").mkdir(parents=True)
        (base / "include" / "cadef.h").touch()
    if scenario == "minimal-inventory":
        libdir = base / "lib" / "shared-test"
        libdir.mkdir(parents=True)
        # These empty files describe SDK availability only; nothing links or
        # executes them. No optional PVA or unused pvDatabase library is present.
        for name in required.split():
            filename = f"{name}.lib" if windows else f"lib{name}.a"
            (libdir / filename).touch()
    query = tree / "inspect-epics.mk"
    query.write_text(
        ".PHONY: inspect-component\ninspect-component:\n"
        "\t$(info required=$(EPICS_REQUIRED_LIBS))\n"
        "\t$(info epics=$(EPICS_LIBS))\n"
        "\t$(info product-libs=$(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS))\n"
        "\t@:\n"
    )
    arguments = [
        "-C", str(source), "-f", "Makefile", "-f", str(query), "inspect-component",
        f"EPICS_BASE={base}", "EPICS_HOST_ARCH=shared-test", "QT_BUILD_GOALS=",
        "HAVE_QT=1", "HAVE_MOTIF=1", "SKIP_DEPENDENCY_CHECK=1",
    ]
    if windows:
        arguments += ["OS=Windows", "ARCH=x86_64"]
    succeeds = not required or scenario == "minimal-inventory"
    result = run_make(tree, *arguments, expect_success=succeeds)
    if succeeds:
        assert f"required={required}\n" in result.stdout
        assert "pvDatabase" not in result.stdout
    else:
        assert result.returncode != 0
        diagnostic = (
            "EPICS libraries missing" if scenario == "missing-libraries"
            else "EPICS Base headers not found"
        )
        assert diagnostic in result.stdout + result.stderr


@pytest.mark.parametrize("component,makefile", (
    ("SDDS/mdbmth", "Makefile"),
    ("SDDS/SDDSlib", "Makefile"),
    ("SDDS/SDDSlib", "Makefile.mpi"),
    ("SDDS/SDDSaps", "Makefile"),
    ("SDDS/SDDSaps/sddseditor", "Makefile"),
    ("SDDS/SDDSaps/sddsplots/motifDriver", "Makefile"),
    ("SDDS/SDDSaps/sddsplots/qtDriver", "Makefile"),
    ("SDDS-EPICS/rampload", "Makefile"),
    ("SDDS-EPICS/SDDSepics", "Makefile"),
))
@pytest.mark.parametrize("mode", ("default", "unbundled", "disabled"))
def test_direct_component_openmp_policy(build_tree, component, makefile, mode):
    """Direct component builds enable bundling and retain command-line overrides."""
    source = shared_source(component)
    if not (source / makefile).is_file():
        pytest.skip(f"Sibling component {component} is not available")
    tree = build_tree
    runtime = tree / "openmp"
    runtime.mkdir()
    # Availability markers are read only as configuration; no macOS binary runs.
    library = runtime / "libomp.dylib"
    library.touch()
    query = tree / "inspect-openmp.mk"
    query.write_text(
        ".PHONY: inspect-component\ninspect-component:\n"
        "\t$(info enabled=$(ENABLE_OPENMP))\n"
        "\t$(info bundled=$(BUNDLE_OPENMP))\n"
        "\t$(info cflags=$(DARWIN_OPENMP_CFLAGS))\n"
        "\t$(info ldflags=$(DARWIN_OPENMP_LDFLAGS))\n"
        "\t$(info product-libs=$(PROD_LIBS) $(PROD_SYS_LIBS) $(LDFLAGS))\n"
        "\t$(info runtime=$(BUNDLED_RUNTIME_LIBS))\n"
        "\t@:\n"
    )
    arguments = [
        "-C", str(source), "-f", makefile, "-f", str(query), "inspect-component",
        "OS=Darwin", "ARCH=aarch64", "EPICS_REQUIRED_LIBS=", "QT_BUILD_GOALS=",
        "HAVE_QT=1", "HAVE_MOTIF=1", "SKIP_DEPENDENCY_CHECK=1",
        f"DARWIN_OPENMP_INCLUDE={runtime}", f"DARWIN_OPENMP_LIBRARY={library}",
        "DARWIN_OPENMP_LICENSE=",
    ]
    if mode == "unbundled":
        arguments += ["BUNDLE_OPENMP=0"]
    elif mode == "disabled":
        arguments += ["ENABLE_OPENMP=0"]
    result = run_make(tree, *arguments)
    values = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    assert values["enabled"] == ("0" if mode == "disabled" else "1")
    assert values["bundled"] == ("0" if mode == "unbundled" else "1")
    if mode == "disabled":
        assert values["cflags"] == values["ldflags"] == values["runtime"] == ""
    else:
        assert "-fopenmp" in values["cflags"]
        assert "-lomp" in values["ldflags"]
        assert "-lomp" in values["product-libs"]
        if mode == "unbundled":
            assert values["runtime"] == ""
            assert f"-Wl,-rpath,{runtime}" in values["ldflags"]
        else:
            assert values["runtime"] == str(tree / "lib" / "libomp.dylib")
            assert "-Wl,-rpath,@loader_path/../../lib/Darwin-aarch64" in values["ldflags"]
            depth = len(Path(component).parts) - 2
            if depth:
                path = "../" * (depth + 2) + "lib/Darwin-aarch64"
                assert f"-Wl,-rpath,@loader_path/{path}" in values["ldflags"]


@pytest.mark.parametrize("goal", ("install-programs", "install-libraries"))
def test_darwin_openmp_extension_runtime_install(build_tree, goal):
    """Program-only extensions installs include the runtime and license."""
    tree = build_tree
    source = tree / "openmp"
    source.mkdir()
    library = source / "libomp.dylib"
    license_file = source / "LICENSE.TXT"
    library.touch()
    license_file.write_text("OpenMP license fixture\n")
    destination = tree / "extensions" / "lib" / "darwin-aarch64"
    arguments = (
        "OS=Darwin", "ARCH=aarch64", "ENABLE_OPENMP=1", "BUNDLE_OPENMP=1",
        f"DARWIN_OPENMP_INCLUDE={source}", f"DARWIN_OPENMP_LIBRARY={library}",
        f"DARWIN_OPENMP_LICENSE={license_file}",
        f"EPICS_LIB_DIR={destination}", "EPICS_HOST_ARCH=darwin-aarch64",
        "LIBRARY=", "PYTHON_SHARED_LIBRARY=",
    )
    values = query_make_variables(tree, ("DARWIN_OPENMP_LDFLAGS",), *arguments)
    assert "-Wl,-rpath,@loader_path/../../lib/darwin-aarch64" in values["DARWIN_OPENMP_LDFLAGS"]
    # Inspect macOS copy/sign recipes without executing foreign tools on Linux.
    result = run_make(tree, "-n", goal, *arguments)
    lines = result.stdout.splitlines()
    sign = next(i for i, line in enumerate(lines) if line.startswith("codesign "))
    install = next(i for i, line in enumerate(lines) if f"{destination}/libomp.dylib" in line and "cp -f" in line)
    assert sign < install
    assert f"cp -f {library} {tree}/lib/libomp.dylib" in result.stdout
    assert f"install_name_tool -id @rpath/libomp.dylib {tree}/lib/libomp.dylib" in result.stdout
    assert f"cp -f {tree}/lib/LICENSE.libomp.txt {destination}/LICENSE.libomp.txt" in result.stdout


@pytest.mark.parametrize("repository", ("StripTool", "probe", "qtalh", "SDDS", "SDDS-EPICS", "medm"))
@pytest.mark.parametrize("environment,mounted,overrides,expected", (
    ("", False, {}, ("0", "0")),
    ("/site/improv.sh", False, {}, ("1", "0")),
    ("/site/swing.sh", False, {}, ("0", "1")),
    ("", True, {}, ("1", "0")),
    ("/site/improv/swing.sh", True, {}, ("0", "1")),
    ("/site/swing.sh", True, {"IMPROV_BUILD": "1"}, ("1", "0")),
    ("/site/improv.sh", True, {"SWING_BUILD": "1"}, ("0", "1")),
    ("/site/improv.sh", True, {"IMPROV_BUILD": "0"}, ("0", "0")),
    ("/site/swing.sh", True, {"SWING_BUILD": "0"}, ("0", "0")),
    ("", True, {"IMPROV_BUILD": "0"}, ("0", "0")),
    ("/site/swing.sh", True, {"IMPROV_BUILD": "0", "SWING_BUILD": "0"}, ("0", "0")),
    ("/site/improv.sh", True, {"IMPROV_BUILD": ""}, ("", "0")),
    # Swing can see the Improv mount; its scheduler or node name selects Swing.
    ("", True, {"SLURM_CLUSTER_NAME": "swing"}, ("0", "1")),
    ("", True, {"LCRC_CLUSTER": "swing-login-01"}, ("0", "1")),
    ("", True, {"SLURM_CLUSTER_NAME": "improv"}, ("1", "0")),
    ("", True, {"SLURM_CLUSTER_NAME": "swing", "IMPROV_BUILD": "1"}, ("1", "0")),
    ("", False, {"SLURM_CLUSTER_NAME": "swing"}, ("0", "0")),
))
def test_shared_linux_site_selection(build_tree, monkeypatch, repository, environment, mounted, overrides, expected):
    """All repositories select the same site libraries and respect explicit choices."""
    source = shared_source(repository)
    if not (source / "Makefile.rules").is_file():
        pytest.skip(f"Sibling repository {repository} is not available")
    if {"SLURM_CLUSTER_NAME", "LCRC_CLUSTER"} & set(overrides):
        require_current_shared_rules(source, repository)
    for name in ("IMPROV_BUILD", "SWING_BUILD", "MPI_BIN", "IOMP5_LIB", "IOMP5_LIB_DIR",
                 "SLURM_CLUSTER_NAME", "LCRC_CLUSTER"):
        monkeypatch.delenv(name, raising=False)
    tree = build_tree
    site = tree / "spack"
    if mounted:
        # Directory availability only; no cluster tools or libraries execute.
        (site / "gcc-13.2.0").mkdir(parents=True)
    (tree / "Makefile").write_text(f"include {source}/Makefile.rules\n")
    values = query_make_variables(
        tree, ("IMPROV_BUILD", "SWING_BUILD", "MPI_BIN", "IOMP5_LIB", "LAPACK_LIB",
               "LZMA_LIB", "HDF5_LIB", "TIFF_LIB", "GD_LIB", "JPEG_LIB", "FFTW3_LIB",
               "PNG_LIB", "EXTRA_INC_DIRS", "PROD_SYS_LIBS"),
        "OS=Linux", "ARCH=x86_64", f"BASH_ENV={environment}", f"LCRC_SPACK={site}",
        f"LIB_DIRS={tree}/no-system-libraries", "MKLROOT=",
        *(f"{name}={value}" for name, value in overrides.items()),
    )
    assert (values["IMPROV_BUILD"], values["SWING_BUILD"]) == expected
    libraries = {
        "LZMA_LIB": "gcc-13.2.0/xz-5.4.6-3zhah5p/lib/liblzma.so",
        "HDF5_LIB": "gcc-13.2.0/hdf5-1.14.3-o6prqns/lib/libhdf5.so",
        "TIFF_LIB": "gcc-12.3.0/libtiff-4.5.1-hmstscv/lib64/libtiff.so",
        "GD_LIB": "gcc-12.3.0/libgd-2.3.3-enxppnl/lib/libgd.so",
        "JPEG_LIB": "gcc-12.3.0/libjpeg-turbo-3.0.0-v7a5c5g/lib64/libjpeg.so",
        "FFTW3_LIB": "gcc-13.2.0/fftw-3.3.10-x5237xr/lib/libfftw3.so",
    }
    active = "1" in expected
    for name, suffix in libraries.items():
        if active:
            assert values[name].split()[-1] == str(site / suffix)
        else:
            assert values[name] == ""
    if active:
        assert f"-I{site}/aocc-4.1.0/xz-5.4.1-gju4bpk/include" in values["EXTRA_INC_DIRS"]
    else:
        assert str(site) not in values["EXTRA_INC_DIRS"]
        assert values["MPI_BIN"] == ""
    if expected[0] == "1":
        assert values["MPI_BIN"] == str(site / "aocc-4.1.0/openmpi-5.0.1-jk5bu46/bin")
        assert "/libiomp5.so" in values["IOMP5_LIB"]
        assert values["IOMP5_LIB"] in values["PROD_SYS_LIBS"]
        assert "nvhpc-24.3-ksebg4i" in values["LAPACK_LIB"]
        assert values["PNG_LIB"] == ""
    else:
        assert values["IOMP5_LIB"] == ""
        assert "libiomp5" not in values["PROD_SYS_LIBS"]
        assert values["LAPACK_LIB"] == "-llapack -lblas"
    if expected[1] == "1":
        assert values["MPI_BIN"] == "/soft/openmpi/4.1.1/swing/bin"
        assert values["PNG_LIB"] == str(site / "gcc-12.3.0/libpng-1.6.39-hsjzalf/lib64/libpng.so")


@pytest.mark.parametrize("os", ("Darwin", "Windows"))
@pytest.mark.parametrize("explicit", (False, True))
def test_site_settings_are_linux_only(build_tree, monkeypatch, os, explicit):
    """Site signals and explicit site flags cannot add Linux libraries elsewhere."""
    for name in ("IMPROV_BUILD", "SWING_BUILD", "MPI_BIN", "IOMP5_LIB", "IOMP5_LIB_DIR"):
        monkeypatch.delenv(name, raising=False)
    tree = build_tree
    site = tree / "spack"
    (site / "gcc-13.2.0").mkdir(parents=True)
    (tree / "Makefile").write_text(f"include {ROOT}/Makefile.rules\n")
    values = query_make_variables(
        tree, ("IMPROV_BUILD", "SWING_BUILD", "MPI_BIN", "IOMP5_LIB", "LAPACK_LIB",
               "LZMA_LIB", "HDF5_LIB", "PNG_LIB", "EXTRA_INC_DIRS", "PROD_SYS_LIBS"),
        f"OS={os}", "ARCH=x86_64", "BASH_ENV=/site/improv/swing.sh", f"LCRC_SPACK={site}",
        f"LIB_DIRS={tree}/no-system-libraries", "MKLROOT=", "PYTHON=python3",
        *(('IMPROV_BUILD=1', 'SWING_BUILD=1') if explicit else ()),
    )
    if not explicit:
        assert values["IMPROV_BUILD"] == values["SWING_BUILD"] == "0"
    assert values["MPI_BIN"] == values["IOMP5_LIB"] == ""
    assert str(site) not in " ".join(values.values())
    assert values["LAPACK_LIB"] == "-llapack -lblas"


def test_conflicting_linux_sites_are_rejected(build_tree):
    """Reject mixed site libraries before attempting any compiler or link recipe."""
    result = run_make(
        build_tree, "-f", str(ROOT / "Makefile.rules"), "clean", "OS=Linux",
        "IMPROV_BUILD=1", "SWING_BUILD=1",
        expect_success=False,
    )
    assert result.returncode != 0
    assert "IMPROV_BUILD and SWING_BUILD cannot both be 1" in result.stderr


@pytest.mark.parametrize("option,value,expected", (
    ("IMPROV_BUILD", "0", ("0", "0")),
    ("SWING_BUILD", "1", ("0", "1")),
))
def test_site_environment_overrides(build_tree, monkeypatch, option, value, expected):
    """Environment choices retain precedence over automatic site detection."""
    monkeypatch.delenv("IMPROV_BUILD", raising=False)
    monkeypatch.delenv("SWING_BUILD", raising=False)
    monkeypatch.setenv(option, value)
    (build_tree / "Makefile").write_text(f"include {ROOT}/Makefile.rules\n")
    values = query_make_variables(
        build_tree, ("IMPROV_BUILD", "SWING_BUILD"), "OS=Linux",
        "BASH_ENV=/site/improv.sh", f"LCRC_SPACK={build_tree}/absent-spack",
    )
    assert (values["IMPROV_BUILD"], values["SWING_BUILD"]) == expected
