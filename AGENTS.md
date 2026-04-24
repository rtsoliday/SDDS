# Agent Instructions

This repository contains the SDDS (Self Describing Data Sets) C/C++ library, utilities and related third‑party code.

## Project Layout
- `SDDSlib/` – Core implementation of the SDDS library written in C, providing routines for reading, writing and processing SDDS files.
- `SDDSaps/` – Utility code for SDDS command‑line applications, including common processing and helper routines.
- `include/` – Public header files for the SDDS library and other utilities.
- `utils/` – Miscellaneous helper programs.
- `fftpack/`, `lzma/`, `png/`, `tiff/`, `zlib/` – Third‑party libraries bundled with this project.
- `matlib/`, `mdbcommon/`, `mdblib/`, `mdbmth/`, `meschach/`, `pgapack/`, `xlslib/`, etc. – Additional numerical and support libraries.
- `rpm/`, `namelist/`, `2d_interpolate/`, `rpns/` – Specialized utilities and mathematical routines.

Refer to `README.md` for build prerequisites and usage examples.

## Coding Conventions
- **Indentation:** Two spaces per level; tabs are avoided.
- **Brace style:** Opening braces on the same line as function or control statements (K&R style).
- **Comments:** Each source file begins with a Doxygen-style block comment (`/** ... */`) documenting the file, authorship, and license. Functions also include Doxygen headers. The file header typically contains:
  - `@file` and `@brief` declarations with optional `@details` text.
  - `@section Usage` and `@section Options` tables documenting command behavior.
  - Optional `@subsection` blocks for incompatibilities or requirements.
  - Copyright and license information using `@copyright` and `@license`.
  - Author lists via `@author` or `@authors`.
  - Functions may include `@brief`, `@param`, and `@return` tags.
- **Naming:**
  - Public API functions use the `SDDS_` prefix followed by PascalCase (e.g., `SDDS_WriteAsciiPage`).
  - Internal helper functions and variables use camelCase (e.g., `bigBuffer`, `compute_average`).
  - Macros and constants are uppercase with underscores (e.g., `SDDS_SHORT`, `INITIAL_BIG_BUFFER_SIZE`).
- **File organization:**
  - Core library code is under `SDDSlib/`, utilities and applications under `SDDSaps/`.
  - C source files use `.c` extension; C++ utilities use `.cc`.
- **Header inclusion order:** Project headers (`"..."`) first, followed by standard library headers (`<...>`), then third-party headers.
- **Line length:** No strict maximum; some lines exceed 200 characters.

These guidelines should be followed when contributing new code or documentation to maintain consistency with the existing project.

## Rebuilding and Verifying Changes

After making changes to the code, beyond just comments and latex files, rebuild the repository and verify there are no compiler warnings by running:

```bash
make clean
make -j
```

## Machine-managed coding quickstart
This supplement is generated from repository evidence and leaves the handwritten guidance above unchanged.

<!-- BEGIN MACHINE:summary -->
## Quick start
- Repository-local guidance is sufficient: start with `AGENTS.md`, `README.md`, `docs/`, build/test/config files, and the source tree.
- The **SDDS (Self Describing Data Sets) Library** is a C/C++ library designed for handling structured scientific data efficiently. It provides a flexible data format and a set of utilities for writing, reading, and processing SDDS files in both ASCII and binary formats.
- Primary work areas: `2d_interpolate`, `fftpack`, `gd`, `include`, `levmar`, `lzma`.

## Read first
- `README.md`: Primary project overview and workflow notes
- `2d_interpolate/nn/README`: Supporting documentation with operational details
- `SDDSaps/doc/README.md`: Supporting documentation with operational details
- `SDDSlib/doc/README.md`: Supporting documentation with operational details
- `fftpack/Makefile`: Build system entry point or dependency manifest

## Build and test
- Documented setup/build commands: `make -f Makefile.MSVC all`, `make -j`, `make`.
- Detected build systems: GNU Make.
- Documented test commands: `pytest -q`, `make tests`.
- Likely run commands or operator entry points: `./sdds_write_demo output.sdds`, `./sdds_read_demo input.sdds`, `./nnphi_test`, `./nnai_test`.

## Operational warnings
- Legacy compatibility paths are still present; confirm which mode is actually in use before changing defaults.
- Platform-specific dependency setup matters; do not assume one platform's build recipe carries over unchanged.
- Large multi-program tree: keep edits scoped to one subtree and watch for wide blast radius from shared code.

## Compatibility constraints
- Legacy components are still present; prefer the documented default path before switching to older modes.
- Cross-platform support exists, but platform-specific dependency setup matters.

## Related knowledge
- Repository-local documentation should be treated as authoritative.
- If a shared `llm-wiki/` directory is present in this workspace or parent folder, consult [the matching repo page](../llm-wiki/repos/SDDS.md) for additional architectural context.
- If no shared wiki is present, continue using repository-local evidence only.
- If available, [the SDDS concept page](../llm-wiki/concepts/sdds.md) adds broader cross-repo context.
- If present in this workspace, [the cross-repo map](../llm-wiki/insights/cross-repo-map.md) helps explain related repositories.
<!-- END MACHINE:summary -->
