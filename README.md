# SDDS C/C++ Library and ToolKit

## Overview
The **SDDS (Self Describing Data Sets) Library** is a C/C++ library designed for handling structured scientific data efficiently. It provides a flexible data format and a set of utilities for writing, reading, and processing SDDS files in both ASCII and binary formats.

## Features
- Read and write **ASCII** and **binary** SDDS files.
- Supports **parameters**, **arrays**, and **column** data.
- Includes utilities for **data compression (LZMA, Zstd, GZIP)**.
- Compatible with **MPI for parallel processing**.
- Provides numerical manipulation and mathematical utilities.

## Prerequisites
To compile and use the SDDS library, ensure you have installed the following dependencies on your system:

### RHEL, Fedora, CentOS, Rocky
```bash
sudo yum install epel-release
sudo yum config-manager --set-enabled crb
sudo yum install gcc gcc-c++ make zlib-devel libzstd-devel xz-devel hdf5-devel libaec-devel gsl-devel libpng-devel gd-devel liblerc-devel libdeflate-devel libtiff-devel qt5-qtbase-devel blas-devel lapack-devel mpich mpich-devel fftw-devel
```

### Ubuntu, Debian
```bash
sudo apt install --ignore-missing gcc g++ make zlib1g-dev libzstd-dev liblzma-dev libhdf5-dev libaec-dev libgsl-dev libpng-dev libgd-dev liblerc-dev libdeflate-dev libtiff-dev qtbase5-dev libblas-dev liblapack-dev liblapacke-dev libfftw3-dev mpich libmpich-dev
```

### OpenSUSE
```bash
sudo zypper install gcc gcc-c++ make zlib-devel libzstd-devel xz-devel hdf5-devel libaec-devel gsl-devel libpng-devel gd-devel lerc-devel libdeflate-devel libtiff-devel qt5-qtbase-devel blas-devel lapack-devel mpich mpich-devel fftw3-devel
```

### MacOS with MacPorts
```bash
sudo port install zlib zstd xz hdf5 libaec gsl libpng gd2 lerc libdeflate tiff qt5 mpich fftw-3
```

### Windows with Cygwin and MSVC
- Install Visual Studio 2022 with the **Desktop development with C++** workload.
- Install 64-bit Cygwin in `C:\cygwin64`; the build uses its `sh`, `uname`,
  `cp`, `rm`, and `mkdir` commands.
- Install GNU Make through Cygwin or Strawberry Perl. The launcher checks
  `C:\cygwin64\bin\make.exe` and `C:\Strawberry\c\bin\make.exe`.
- Install [Microsoft MPI](https://learn.microsoft.com/en-us/message-passing-interface/microsoft-mpi)
  if MPI support is needed.
- Install Qt 6.8.2 for MSVC 2022 in `C:\Qt\6.8.2\msvc2022_64`.
- Install and build GNU Scientific Library:

```bash
git clone https://github.com/rtsoliday/gsl.git
cd gsl
make -f Makefile.MSVC all
cd ..
```

- Download the HDF5 libraries:

```bash
wget https://github.com/HDFGroup/hdf5/releases/download/hdf5_1.14.6/hdf5-1.14.6-win-vs2022_cl.zip
unzip hdf5-1.14.6-win-vs2022_cl.zip
cd hdf5
unzip HDF5-1.14.6-win64.zip
cd ..
```

The repositories should have this sibling layout:

```text
parent\
  SDDS\
  gsl\
  hdf5\HDF5-1.14.6-win64\
```

After installing the prerequisites, build from an ordinary Command Prompt,
PowerShell terminal, IDE terminal, or Explorer without setting environment
variables manually:

```bat
build-windows.bat
```

The launcher discovers Visual Studio, initializes the x64 MSVC environment,
adds Qt and the POSIX tools to the child process's `PATH`, and runs the build
from the repository root. It supports these commands:

```bat
build-windows.bat build
build-windows.bat clean
build-windows.bat rebuild
build-windows.bat test
build-windows.bat build 8
```

The default is an incremental parallel build using the machine's logical
processor count. The optional second argument sets the job count. Set
`SDDS_CYGWIN_ROOT` or `SDDS_MAKE_EXE` before invoking the
launcher to override the default tool locations. Cygwin remains a build-tool
dependency because the existing make recipes use POSIX shell commands, but it
is no longer necessary to enter a configured Cygwin terminal.

## Repository Structure
```
📦 SDDS Repository
├── meschach/       # Meschach library used by sddspseudoinverse
├── xlslib/         # xlslib (Excel) library used by sdds2spreadsheet
├── zlib/           # GZip support for Windows
├── lzma/           # LZMA support for Windows
├── mdblib/         # memory management and utility functions
├── mdbmth/         # mathematical computations and numerical analysis functions
├── rpns/code       # RPN (Reverse Polish Notation) libraries
├── namelist/       # namelist pre-processor for C programs
├── SDDSlib/        # SDDS library functions
│   ├── demo/       # demo SDDS programs
├── fftpack/        # fast Fourier transform library used by sddscontour and sddspseudoinverse
├── matlib/         # matrix manipulation functions
├── mdbcommon/      # general utility functions
├── utils/          # misc utility programs
├── 2d_interpolate/ # 2D interpolation libraries
│   ├── nn/         # Natural Neighbours interpolation library
│   ├── csa/        # Cubic Spline Approximation
├── png/            # PNG support for Windows
├── gd/             # GD support for Windows
├── tiff/           # TIFF support for Windows
├── SDDSaps/        # SDDS ToolKit applications
│   ├── sddsplots/  # SDDS plotting tool (see SDDSaps/sddsplots/sddsplot-json-schema.md for JSON output schema)
│       ├── qtDriver/  # plotting GUI used by sddsplot
│       ├── winMotifDriver/  # Windows plotting tool (deprecated)
│       ├── motifDriver/     # Motif (Linux and MacOS) plotting tool (deprecated)
│   ├── sddscontours/   # SDDS contour plotting tool
│   ├── pseudoInverse/  # SDDS matrix applications
├── levmar/         # Levenberg - Marquardt algorithm
├── pgapack/        # Parallel Genetic Algorithm
├── LICENSE         # Licensing information
└── README.md       # This file
```

## Compilation
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

For more details, refer to the **[SDDS API documentation](https://ops.aps.anl.gov/manuals/SDDSlib/html/files.html)**.

## Thread Safety
The SDDS libraries support concurrent use by multiple threads when each thread
uses its own `SDDS_DATASET` objects and caller-owned data. Internal error stacks
and many former static scratch buffers are thread-local, and process-global
settings are guarded by internal locks.

Shared dataset handles, shared RPN arrays, file streams, and caller-owned
objects are not automatically serialized. Callers must lock around those shared
objects when using them from multiple threads. Process-global settings remain
process-global even though updates are protected against data races.

For details, see [SDDSlib/doc/thread-safety.md](SDDSlib/doc/thread-safety.md).

## Tests
Simple regression tests are provided for many of the SDDS command line tools. After building the project run:
```bash
pytest -q
```
The tests require the binaries in `bin/Linux-x86_64` and use `SDDSlib/demo/example.sdds` as sample data.


## FAQ
For answers to common questions about the SDDS toolkit, see the [FAQ](SDDSaps/doc/FAQ.md).

## ChatGPT Assistant
A custom **[ChatGPT SDDS C/C++ Language Assistant](https://chatgpt.com/g/g-67376bce92308190a01b7056cdd3d74a-sdds-c-c-language-assistant)** is available to help write new SDDS software.

## CODEX Environment Setup
To develop SDDS features using the [chatgpt.com/codex](https://chatgpt.com/codex) environment, initialize the container with the required libraries:

```bash
apt-get update
apt-get install -y zlib1g-dev libzstd-dev liblzma-dev libhdf5-dev libaec-dev libgsl-dev libpng-dev libgd-dev liblerc-dev libdeflate-dev libtiff-dev qtbase5-dev libblas-dev liblapack-dev liblapacke-dev mpich libmpich-dev libqt5datavisualization5-dev
```

## License
This library is distributed under the **Software License Agreement** found in the `LICENSE` file.

## Authors
- M. Borland
- C. Saunders
- R. Soliday
- H. Shang

## Acknowledgments
This project is developed and maintained by **[Accelerator Operations & Physics](https://www.aps.anl.gov/Accelerator-Operations-Physics)** at the **Advanced Photon Source** at **Argonne National Laboratory**.

For more details, visit the official **[SDDS documentation](https://www.aps.anl.gov/Accelerator-Operations-Physics/Documentation)**.
