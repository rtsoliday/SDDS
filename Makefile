# Detect OS and Architecture
OS := $(shell uname -s)
ifeq ($(findstring CYGWIN, $(OS)),CYGWIN)
    OS := Windows
endif

# Check for external gsl repository needed on Windows
ifeq ($(OS), Windows)
  GSL_REPO = $(wildcard ../gsl)
  ifeq ($(GSL_REPO),)
    $(error GSL source code not found. Run 'git clone https://github.com/rtsoliday/gsl.git' next to the SDDS repository)
  endif

  HDF5_REPO = $(wildcard ../hdf5/HDF5-1.14.6-win64)
  ifeq ($(HDF5_REPO),)
    $(info HDF5 source code not found. Run:)
    $(info cd ..)
    $(info wget https://github.com/HDFGroup/hdf5/releases/download/hdf5_1.14.6/hdf5-1.14.6-win-vs2022_cl.zip)
    $(info unzip hdf5-1.14.6-win-vs2022_cl.zip)
    $(info cd hdf5)
    $(info unzip HDF5-1.14.6-win64.zip)
    $(error exiting)
  endif
endif

include Makefile.rules

ifeq ($(OS), Linux)
  GSL_LOCAL = $(wildcard gsl)
  ifneq ($(IMPROV_BUILD),1)
    HDF5_LOCAL = $(wildcard hdf5)
  endif
endif

PLOT_DRIVER = SDDSaps/sddsplots/qtDriver
ifeq ($(IMPROV_BUILD),1)
  PLOT_DRIVER = SDDSaps/sddsplots/motifDriver
endif
ifeq ($(OS), Windows)
  PLOT_DRIVER = SDDSaps/sddsplots/winMotifDriver
endif

DIRS = $(GSL_REPO)
DIRS += $(GSL_LOCAL)
DIRS += $(HDF5_LOCAL)
DIRS += include
DIRS += meschach
DIRS += xlslib
DIRS += zlib
DIRS += lzma
DIRS += mdblib
DIRS += mdbmth
DIRS += rpns/code
DIRS += namelist
DIRS += SDDSlib
DIRS += SDDSlib/demo
DIRS += fftpack
DIRS += matlib
DIRS += mdbcommon
DIRS += utils
DIRS += 2d_interpolate/nn
DIRS += 2d_interpolate/csa
DIRS += png
DIRS += gd
DIRS += tiff
DIRS += SDDSaps
DIRS += SDDSaps/sddsplots
#DIRS += SDDSaps/sddsplots/winMotifDriver
#DIRS += SDDSaps/sddsplots/motifDriver
DIRS += $(PLOT_DRIVER)
DIRS += SDDSaps/sddscontours
DIRS += SDDSaps/pseudoInverse
DIRS += SDDSaps/sddseditor
DIRS += levmar
ifneq ($(MPI_CC),)
DIRS += pgapack
endif

.PHONY: all $(DIRS) clean distclean

all: $(DIRS)

ifneq ($(GSL_REPO),)
  GSL_CLEAN = $(MAKE) -C $(GSL_REPO) -f Makefile.MSVC clean
  $(GSL_REPO):
	$(MAKE) -C $@ -f Makefile.MSVC all
endif
ifneq ($(GSL_LOCAL),)
  GSL_CLEAN = $(MAKE) -C $(GSL_LOCAL) clean
  $(GSL_LOCAL):
	$(MAKE) -C $@ all
endif
ifneq ($(HDF5_LOCAL),)
  HDF5_CLEAN = $(MAKE) -C $(HDF5_LOCAL) clean
  $(HDF5_LOCAL):
	$(MAKE) -C $@ all
endif
include: $(GSL_REPO) $(GSL_LOCAL) $(HDF5_LOCAL)
	$(MAKE) -C $@
meschach: include
	$(MAKE) -C $@
xlslib: meschach
	$(MAKE) -C $@
zlib: xlslib
	$(MAKE) -C $@
lzma: zlib
	$(MAKE) -C $@
mdblib: lzma
	$(MAKE) -C $@
mdbmth: mdblib
	$(MAKE) -C $@
rpns/code: mdbmth $(GSL_REPO) $(GSL_LOCAL)
	$(MAKE) -C $@
namelist: rpns/code
	$(MAKE) -C $@
ifeq ($(MPI_CC),)
SDDSlib: namelist
	$(MAKE) -C $@
else
SDDSlib: namelist
	$(MAKE) -C $@
	$(MAKE) -C $@ -f Makefile.mpi
endif
SDDSlib/demo: SDDSlib
	$(MAKE) -C $@
fftpack: SDDSlib/demo
	$(MAKE) -C $@
matlib: fftpack
	$(MAKE) -C $@
mdbcommon: matlib
	$(MAKE) -C $@
utils: mdbcommon $(HDF5_LOCAL)
	$(MAKE) -C $@
2d_interpolate/nn: utils
	$(MAKE) -C $@
2d_interpolate/csa: 2d_interpolate/nn
	$(MAKE) -C $@
png: 2d_interpolate/csa
	$(MAKE) -C $@
gd: png
	$(MAKE) -C $@
tiff: gd
	$(MAKE) -C $@
SDDSaps: tiff
	$(MAKE) -C $@
SDDSaps/sddsplots: SDDSaps
	$(MAKE) -C $@
SDDSaps/sddsplots/winMotifDriver: SDDSaps/sddsplots
	$(MAKE) -C $@
SDDSaps/sddsplots/motifDriver: SDDSaps/sddsplots
	$(MAKE) -C $@
SDDSaps/sddsplots/qtDriver: SDDSaps/sddsplots
	$(MAKE) -C $@
SDDSaps/sddscontours: $(PLOT_DRIVER) SDDSaps/sddsplots
	$(MAKE) -C $@
SDDSaps/pseudoInverse: SDDSaps/sddscontours
	$(MAKE) -C $@
SDDSaps/sddseditor: SDDSaps/pseudoInverse
	$(MAKE) -C $@
levmar: SDDSaps/sddseditor
	$(MAKE) -C $@
ifneq ($(MPI_CC),)
pgapack: levmar
	$(MAKE) -C $@
endif

clean:
	$(MAKE) -C include clean
	$(MAKE) -C meschach clean
	$(MAKE) -C xlslib clean
	$(MAKE) -C zlib clean
	$(MAKE) -C lzma clean
	$(MAKE) -C mdblib clean
	$(MAKE) -C mdbmth clean
	$(MAKE) -C rpns/code clean
	$(MAKE) -C namelist clean
	$(MAKE) -C SDDSlib clean
	$(MAKE) -C SDDSlib/demo clean
	$(MAKE) -C fftpack clean
	$(MAKE) -C matlib clean
	$(MAKE) -C mdbcommon clean
	$(MAKE) -C utils clean
	$(MAKE) -C 2d_interpolate/nn clean
	$(MAKE) -C 2d_interpolate/csa clean
	$(MAKE) -C png clean
	$(MAKE) -C gd clean
	$(MAKE) -C tiff clean
	$(MAKE) -C SDDSaps clean
	$(MAKE) -C SDDSaps/sddsplots clean
	$(MAKE) -C SDDSaps/sddsplots/winMotifDriver clean
	$(MAKE) -C SDDSaps/sddsplots/motifDriver clean
	$(MAKE) -C SDDSaps/sddsplots/qtDriver clean
	$(MAKE) -C SDDSaps/sddscontours clean
	$(MAKE) -C SDDSaps/pseudoInverse clean
	$(MAKE) -C SDDSaps/sddseditor clean
	$(MAKE) -C levmar clean
ifneq ($(MPI_CC),)
	$(MAKE) -C pgapack clean
endif

distclean: clean
	$(GSL_CLEAN)
	$(HDF5_CLEAN)
	rm -rf bin/$(OS)-$(ARCH)
	rm -rf lib/$(OS)-$(ARCH)
