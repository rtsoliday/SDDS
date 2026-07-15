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

## Operational Security for Automated Tests and Diagnostics

Development and testing occur on monitored laboratory systems. Commands that
combine temporary executable content, runtime injection, wrapper scripts, or
headless process automation may resemble malicious activity and trigger
security alerts even when used for legitimate debugging.

### Default requirements

- Prefer existing repository build and test targets over ad hoc shell workflows.
- Run commands as separate, directly attributable steps. Do not combine
  compilation, permission changes, execution, log collection, and cleanup into
  one `bash -lc` command.
- Place generated executables and shared libraries in the repository's
  documented build or test-artifact directory, not in `/tmp`.
- Do not create executable wrapper scripts dynamically. Prefer checked-in test
  helpers, direct argument-list process execution, or purpose-built test
  binaries.
- Do not apply `chmod +x` to generated content unless the user has explicitly
  approved it. Checked-in scripts should carry their executable bit in Git.
- Prefer supported headless facilities, such as `QT_QPA_PLATFORM=offscreen`,
  over additional display or process wrappers when they satisfy the test.
- Preserve commands, source paths, output paths, and test names in test logs so
  activity can be readily attributed to this repository.

### Security-sensitive techniques

Codex must not introduce or execute any of the following without explicit user
approval in the current conversation:

- `LD_PRELOAD`, `DYLD_INSERT_LIBRARIES`, or other runtime library injection
- `ptrace`, debugger attachment, syscall/API hooking, or symbol interposition
- generated executable content in `/tmp`, `/var/tmp`, or another shared
  temporary directory
- dynamically generated wrapper scripts that execute or replace another program
- privilege changes, capability changes, setuid/setgid behavior, or namespace
  manipulation
- disabling, bypassing, or testing endpoint security controls

Before requesting approval, Codex must explain:

1. why the technique is necessary;
2. which process and files it affects;
3. where generated artifacts will be stored;
4. what safer alternatives were considered;
5. the exact command or repository test target that will run; and
6. how artifacts will be retained or removed.

Approval for one command or test does not authorize unrelated uses of the same
technique.

### Design and test guidance

When a regression test appears to require runtime injection or executable
wrappers, first prefer one of these approaches:

1. Extract the relevant behavior into a directly testable function or component.
2. Add a narrow, documented test interface to the application.
3. Use a checked-in helper program built through the normal build system.
4. Run the security-sensitive integration test only through a named, opt-in
   target on an approved test host or CI runner.

Security-sensitive tests must be clearly named and excluded from routine local
test execution by default. Their documentation should state why the technique
is used and identify the expected processes and artifacts.

If a requested diagnostic cannot be completed without a security-sensitive
technique, stop and ask the user rather than constructing an ad hoc workaround.

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
