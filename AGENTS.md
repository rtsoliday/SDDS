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
- **Indentation:** Two spaces are used for each indentation level. Tabs are generally avoided.
- **Braces:** Opening braces are placed on the same line as function or control statements.
- **Comments:** Source files begin with Doxygen‑style block comments documenting the file, authorship, and licensing.
- **Naming:** Functions use camelCase with a lowercase initial letter (e.g., `moveToStringArray`), while macros and constants are uppercase with underscores (e.g., `COLUMN_BASED`).
- **Headers:** Standard C headers appear after project headers in `#include` lists.
- **Line Length:** No strict maximum line length is enforced; some lines exceed 200 characters.

These guidelines should be followed when contributing new code or documentation to maintain consistency with the existing project.
