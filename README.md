# SDDS C/C++ Library and ToolKit

## Overview
The **SDDS (Self Describing Data Sets) Library** is a C/C++ library designed for handling structured scientific data efficiently. It provides a flexible data format and a set of utilities for writing, reading, and processing SDDS files in both ASCII and binary formats.

## Features
- Read and write **ASCII** and **binary** SDDS files.
- Supports **parameters**, **arrays**, and **column** data.
- Includes utilities for **data compression (LZMA, GZIP)**.
- Compatible with **MPI for parallel processing**.
- Provides numerical manipulation and mathematical utilities.

## Prerequisites
To compile and use the SDDS library, ensure you have installed the following dependencies on your system:

### RHEL
```bash
sudo yum install gcc gcc-c++ make zlib-devel zstd-devel xz-devel hdf5-devel libaec-devel gsl-devel libpng-devel gd-devel lerc-devel libdeflate-devel libtiff-devel qt5-qtbase-devel blas-devel lapack-devel mpich mpich-devel
```

### Ubuntu
```bash
sudo apt install --ignore-missing gcc g++ make zlib1g-dev libzstd-dev liblzma-dev libhdf5-dev libaec-dev libgsl-dev libpng-dev libgd-dev liblerc-dev libdeflate-dev libtiff-dev qtbase5-dev libblas-dev liblapack-dev mpich libmpich-dev
```

### OpenSUSE
```bash
sudo zypper install gcc gcc-c++ make zlib-devel libzstd-devel xz-devel hdf5-devel libaec-devel gsl-devel libpng-devel gd-devel lerc-devel libdeflate-devel libtiff-devel qt5-qtbase-devel blas-devel lapack-devel mpich mpich-devel
```

### MacOS with MacPorts
```bash
sudo port install zlib zstd xz hdf5 libaec gsl libpng gd2 lerc libdeflate tiff qt5 mpich
```

### Windows with Cygwin and MSVC
Install [Microsoft MPI](https://learn.microsoft.com/en-us/message-passing-interface/microsoft-mpi)
Install [Qt6](https://doc.qt.io/qt-6/qt-online-installation.html)
Install and build GNU Scientific Library
```bash
git clone https://github.com/rtsoliday/gsl.git
cd gsl
make -f Makefile.MSVC all
cd ..
```
Download the HDF5 libraries
```bash
wget https://github.com/HDFGroup/hdf5/releases/download/hdf5_1.14.6/hdf5-1.14.6-win-vs2022_cl.zip
unzip hdf5-1.14.6-win-vs2022_cl.zip
cd hdf5
unzip HDF5-1.14.6-win64.zip
cd ..
```

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

## ChatGPT Assistant
A custom **[ChatGPT SDDS C/C++ Language Assistant](https://chatgpt.com/g/g-67376bce92308190a01b7056cdd3d74a-sdds-c-c-language-assistant)** is available to help write new SDDS software.

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

