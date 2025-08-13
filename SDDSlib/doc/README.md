# SDDS Library Documentation

This directory contains documentation for the core SDDS C/C++ library.
The materials here correspond to Version 5 of the SDDS library and file protocol.

## Building

From the repository root or within `SDDSlib`:

```bash
make -j
```

The build generates the static library at `lib/<OS>-<ARCH>/libSDDS1.a`.

## Usage

Include headers from the `include/` directory and link against `libSDDS1.a`.
See the sample programs in `SDDSlib/demo` for basic reading and writing
examples.

Source files contain Doxygen-style comments that can be processed with tools
like `doxygen` to produce API references. Additional documents can be added
here to expand the library documentation.

