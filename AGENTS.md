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
After reviewing the source in `SDDSlib` and `SDDSaps`, the following conventions are used:
- **Indentation:** Two spaces per level; tabs are avoided.
- **Brace style:** Opening braces on the same line as function or control statements (K&R style).
- **Comments:** Each source file begins with a Doxygen-style block comment (`/** ... */`) documenting the file, authorship, and license. Functions also include Doxygen headers.
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
