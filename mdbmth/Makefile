# Retain the compiler-default C++ mode for legacy Windows builds.
WINDOWS_CXX_STD_FLAGS :=
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../Makefile.rules

LIBRARY = mdbmth

LIBRARY_SRC = betai.c \
          bsODEp.c \
          complex.cc \
          counter.c \
          dbessel.c \
          diffeqResDescrip.c \
          dlaran.c \
          drand.c \
          elliptic.c \
          factorial.c \
          factorize.c \
          findMinMax.c \
          gammai.c \
          gaussQuad.c \
          GillMillerIntegration.c \
          gridopt.c \
          gy.c \
          halton.c \
          interp.c \
          ipow.c \
          kstests.c \
          k13.c \
          k23.c \
          lincorr.c \
          linfit.c \
          logfile_gener.c \
          lsfBasisFns.c \
          makeHistogram.c \
          mcTable.c \
          median.c \
          mmid.c \
          moments.c \
          onedoptimize.c \
          poly.c \
          powellMin.c \
          qromb.c \
          rkODE.c \
          rowmedian.c \
          sigLevel.c \
          simplex.c \
          smooth.c \
          topbase.c \
          trapInteg.c \
          wofz.c \
          zeroIH.c \
          zeroInterp.c \
          zeroNewton.c \
          medianfilter.c \
          pointInsideContour.c

CFLAGS += -I../include
override CXXFLAGS += -I../include

ifeq ($(OS), Linux)
  CFLAGS += -fopenmp
  override CXXFLAGS += -fopenmp
endif

ifeq ($(OS), Windows)
  CFLAGS += -DEXPORT_MDBMTH -openmp /wd4244
  override CXXFLAGS += -DEXPORT_MDBMTH -openmp /wd4244
  LIBRARY_LIBS = mdblib.lib
endif

include ../Makefile.build
