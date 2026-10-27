"""Package adapters exercised with native fixtures and real component metadata."""

import os
from pathlib import Path
import platform
import shutil
import subprocess

import pytest

from test_shared_makefiles import ROOT, build_tree, query_make_variables, run_make, shared_source

pytestmark = pytest.mark.skipif(
    platform.system() not in ("Linux", "Darwin"), reason="Native Unix build checks"
)


def test_oag_legacy_declarations_and_resource_install(build_tree):
    source = shared_source("oag-src")
    if not (source / "Makefile.oag-products").is_file():
        pytest.skip("OAG package adapter is unavailable")
    tree = build_tree
    makefile = tree / "Makefile"
    text = makefile.read_text().replace(str(ROOT), str(source))
    text = text.replace(
        f"include {source}/Makefile.rules",
        f"include {source}/Makefile.rules\ninclude {source}/Makefile.oag-rules",
    )
    text = text.replace("PROD = sample", "PROD_HOST = sample")
    text = text.replace("sample_SRC =", "sample_SRCS =")
    text = text.replace("LIBRARY_SRC = answer.c", "LIBSRCS = answer.c")
    text = text.replace(
        f"include {source}/Makefile.build",
        "USR_CXXFLAGS = -DOAG_COMPONENT_FLAGS=1\n"
        "INC = sample.h\nTCLLIBNAME = helper.tcl\nLIB_FILES = palette.data\n"
        f"include {source}/Makefile.oag-products\n"
        f"include {source}/Makefile.build\n"
        f"include {source}/Makefile.oag-install",
    )
    makefile.write_text(text)
    main = tree / "main.cc"
    main.write_text(
        "#ifndef OAG_COMPONENT_FLAGS\n#error OAG component flags were lost\n#endif\n"
        + main.read_text()
    )
    payloads = {"sample.h": "int answer(void);\n", "helper.tcl": "# Tcl library fixture\n",
                "palette.data": "resource fixture\n"}
    for name, text in payloads.items():
        (tree / name).write_text(text)
    arguments = (f"INC_DIR={tree}/include", "EPICS_REQUIRED_LIBS=", "AR=ar rcs", "RANLIB=ranlib")
    run_make(tree, *arguments)
    subprocess.run([str(tree / "bin" / "sample")], check=True)
    outputs = [tree / "include" / "sample.h", tree / "lib" / "helper.tcl", tree / "lib" / "palette.data"]
    assert [p.read_text() for p in outputs] == list(payloads.values())
    timestamps = [p.stat().st_mtime_ns for p in outputs]
    run_make(tree, *arguments)
    assert [p.stat().st_mtime_ns for p in outputs] == timestamps
    for p in outputs:
        p.unlink()
    run_make(tree, *arguments)
    assert [p.read_text() for p in outputs] == list(payloads.values())


def _inspect_elegant(tree, mpi=False, mode="cpu", debug=False, site=""):
    source = shared_source("elegant") / "src"
    if not (source.parent / "Makefile.elegant").is_file():
        pytest.skip("Elegant package adapter is unavailable")
    nvcc = next((str(p) for p in (
        Path(os.environ.get("NVCC", shutil.which("nvcc") or "/nonexistent/nvcc")),
        Path("/local/oagmgr-rhel8/cuda-13.4.1/bin/nvcc"), Path("/usr/local/cuda-12.4/bin/nvcc"),
    ) if p.is_file()), None)
    if mode != "cpu" and nvcc is None:
        pytest.skip("CUDA compiler is unavailable for component discovery")
    variables = ("CFLAGS", "CXXFLAGS", "NVCCFLAGS", "OBJ_DIR", "BIN_DIR", "MKL_LIB",
                 "PROD_SYS_LIBS", "elegant_OBJS", "Pelegant_OBJS", "CUDA_ARCH")
    query = tree / "inspect-elegant.mk"
    query.write_text(
        ".PHONY: inspect-elegant\ninspect-elegant:\n"
        + "".join(f"\t$(info {name}=$({name}))\n" for name in variables) + "\t@:\n"
    )
    command = [shutil.which("gmake") or "make", "-C", str(source), "-f",
               "Makefile.mpi" if mpi else "Makefile", "-f", str(query), "inspect-elegant",
               "OS=Linux", "ARCH=x86_64", f"HAVE_CUDA={int(mode != 'cpu')}",
               f"HAVE_GPU={int(mode != 'cpu')}",
               f"GPU_VERIFY={int(mode == 'verify')}", f"MDEBUG={int(debug)}",
               f"IMPROV_BUILD={int(site == 'improv')}", f"SWING_BUILD={int(site == 'swing')}",
               f"MKLROOT={tree}/mkl"]
    if nvcc:
        command.append(f"NVCC={nvcc}")
    environment = os.environ.copy()
    for name in ("MAKEFLAGS", "MFLAGS", "MAKELEVEL"):
        environment.pop(name, None)
    result = subprocess.run(command, text=True, capture_output=True, env=environment)
    with (tree / "build.log").open("a") as log:
        log.write("$ " + " ".join(command) + "\n" + result.stdout + result.stderr)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "overriding recipe" not in result.stderr
    return dict(line.split("=", 1) for line in result.stdout.splitlines()
                if line.partition("=")[0] in variables)


@pytest.mark.parametrize("mpi", (False, True), ids=("serial", "mpi"))
@pytest.mark.parametrize("mode", ("cpu", "gpu", "verify"))
@pytest.mark.parametrize("debug", (False, True), ids=("normal", "mdebug"))
def test_elegant_cpu_cuda_and_debug_build_modes(build_tree, mpi, mode, debug):
    values = _inspect_elegant(build_tree, mpi=mpi, mode=mode, debug=debug)
    flags = values["CFLAGS"].split()
    expected_optimization = "-O0" if debug else ("-O3" if mpi and mode == "cpu" else "-Og")
    assert [f for f in flags if f.startswith("-O")][-1] == expected_optimization
    assert "-std=c++17" in values["CXXFLAGS"].split()
    objects = values["Pelegant_OBJS" if mpi else "elegant_OBJS"].split()
    assert not any(p.endswith(".cu") for p in objects)
    if mode == "cpu":
        assert "-DHAVE_GPU=1" not in flags
        assert not any("gpu_cuda_runtime" in p for p in objects)
    else:
        assert "-DHAVE_GPU=1" in flags
        assert any("gpu_cuda_runtime" in p and p.endswith(".o") for p in objects)
        assert ".gpu" in values["OBJ_DIR"]
        assert "-gpu" in values["BIN_DIR"]
        assert {"--fmad=false", "--prec-div=true", "--prec-sqrt=true", "-std=c++17"} <= set(values["NVCCFLAGS"].split())
        assert ("-DGPU_VERIFY=1" in flags) == (mode == "verify")
        assert values["OBJ_DIR"].endswith(".verify") == (mode == "verify")


@pytest.mark.parametrize("site,architecture", (("improv", "sm_86"), ("swing", "sm_80")))
def test_elegant_sites_retain_one_openmp_runtime(build_tree, site, architecture):
    values = _inspect_elegant(build_tree, site=site)
    assert "libiomp5" not in values["MKL_LIB"] + values["PROD_SYS_LIBS"]
    assert "-lgomp" in values["MKL_LIB"].split()
    assert values["CUDA_ARCH"] == architecture


@pytest.mark.parametrize("windows", (False, True), ids=("unix", "windows"))
def test_oag_local_headers_keep_platform_path_policy(build_tree, windows):
    source = shared_source("oag-src")
    if not (source / "Makefile.oag-rules").is_file():
        pytest.skip("OAG package adapter is unavailable")
    (build_tree / "Makefile").write_text(
        f"include {source}/Makefile.rules\ninclude {source}/Makefile.oag-rules\n"
    )
    values = query_make_variables(
        build_tree, ("INC_DIR",), f"OS={'Windows' if windows else 'Linux'}",
        "ARCH=x86_64", "DD=../", "EPICS_REQUIRED_LIBS=",
    )
    expected = "../../include" if windows else str(source / "include")
    assert values["INC_DIR"] == expected
