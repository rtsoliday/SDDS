# SDDS C/C++ Library and ToolKit

## Overview
The **SDDS (Self Describing Data Sets) Library** is a C/C++ library designed for handling structured scientific data efficiently. It provides a flexible data format and a set of utilities for writing, reading, and processing SDDS files in both ASCII and binary formats.

## Features
- Read and write **ASCII** and **binary** SDDS files.
- Supports **parameters**, **arrays**, and **column** data.
- Includes utilities for **data compression (LZMA, GZIP)**.
- Compatible with **MPI for parallel processing**.
- Provides numerical manipulation and mathematical utilities.

## Repository Structure
```
📦 SDDS Repository
├── meschach/       # Meschach library used by sddspseudoinverse
├── xlslib/         # xlslib (Excel) library used by sdds2spreadsheet
├── zlib/           # GZip support for Windows
├── lzma/           # LZMA support for Windows
├── mdblib/         # memory managment and utility functions
├── mdbmth/         # mathematical computations and numerical analysis functions
├── rpns/code       # RPN (Reverse Polish Notation) libraries
├── namelist/       # namelist pre-processor for C programs
├── SDDSlib/        # SDDS library functions
│   ├── demo/       # demo SDDS programs
├── fftpack/        # fast Fourier transform library used by sddscontour and sddspseudoinverse
├── matlib/         # matrix manipulation functions
├── mdbcommon/      # general utility functions
├── utils/          # misc utility programs
├── 2d_interpolate/ #
│   ├── nn/         # Natural Neighbours interpolation library
│   ├── csa/        # Cubic Spline Approximation
├── png/            # PNG support for Windows
├── gd/             # GD support for Windows
├── tiff/           # TIFF support for Windows
├── SDDSaps/        # SDDS ToolKit applications
│   ├── sddsplots/  # SDDS plotting tool
│       ├── winMotifDriver/  # Windows plotting tool
│       ├── motifDriver/     # Motif (Linux and MacOS) plotting tool
│   ├── sddscontours/   # SDDS contour plotting tool
│   ├── pseudoInverse/  # SDDS matrix applications
├── levmar/         # Levenberg - Marquardt algorithm
├── pgapack/        # Parallel Genetic Algorithm
├── LICENSE         # Licensing information
└── README.md       # This file
```

## Getting Started

### Prerequisites
To compile and use the SDDS library, ensure you have:
- A **C/C++ compiler** (e.g., GCC, Clang, MSVC)
- **make** the GNU make utility (for building the project)
- **zlib** (system library used for Linux and MacOS)
- **lzma** (system library used for Linux and MacOS)
- **png** (system library used for Linux and MacOS)
- **gd** (system library used for Linux and MacOS)
- **tiff** (system library used for Linux and MacOS)
- **mpich** or **openmpi** (optional, for parallel SDDS file operations)
- **motif** (for plottig on Linux)
- **XQuartz** (for plotting on MacOS)


### Compilation
1. Clone the repository:
   ```sh
   git clone https://github.com/rtsoliday/SDDS.git
   cd SDDS
   ```
2. Build using `make`:
   ```sh
   make -j
   ```


## Usage

### Writing an SDDS File
The `sdds_write_demo.c` program demonstrates how to write an SDDS file:
```sh
./sdds_write_demo output.sdds
```
It creates a file with various **parameters, arrays, and columns**.

### Reading an SDDS File
To read an SDDS file and print its contents:
```sh
./sdds_read_demo input.sdds
```
This will print the parameters, arrays, and column values for each page in the SDDS file.

## API Reference
The SDDS API provides functions for defining and manipulating SDDS datasets. Some key functions include:
- `SDDS_InitializeOutput(SDDS_TABLE *table, int mode, int pages, const char *description, const char *contents, const char *filename)` - Initializes an SDDS output file.
- `SDDS_DefineParameter(SDDS_TABLE *table, const char *name, ... )` - Defines a parameter.
- `SDDS_DefineArray(SDDS_TABLE *table, const char *name, ... )` - Defines an array.
- `SDDS_DefineColumn(SDDS_TABLE *table, const char *name, ... )` - Defines a column.
- `SDDS_WritePage(SDDS_TABLE *table)` - Writes a page of data to the SDDS file.
- `SDDS_ReadPage(SDDS_DATASET *dataset)` - Reads a page from an SDDS file.

For more details, refer to the [SDDS API documentation](https://ops.aps.anl.gov/manuals/SDDSlib/html/files.html).

## License
This library is distributed under the **Software License Agreement** found in the `LICENSE` file.

## Authors
- M. Borland
- C. Saunders
- R. Soliday
- H. Shang

## Acknowledgments
This project is developed and maintained by **[Acclerator Operations & Physics](https://www.aps.anl.gov/Accelerator-Operations-Physics)** at the **Advanced Photon Source** at **Argonne National Laboratory**.

For more details, visit the official **[SDDS documentation](https://www.aps.anl.gov/Accelerator-Operations-Physics/Documentation)**.

