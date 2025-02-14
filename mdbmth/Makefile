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
CCFLAGS += -I../include

ifeq ($(OS), Linux)
  CFLAGS += -fopenmp
  CCFLAGS += -fopenmp
endif

ifeq ($(OS), Windows)
  CFLAGS += -DEXPORT_MDBMTH -openmp /wd4244
  CCFLAGS += -DEXPORT_MDBMTH -openmp /wd4244
  LIBRARY_LIBS = mdblib.lib
endif

include ../Makefile.build
