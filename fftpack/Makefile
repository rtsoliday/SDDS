include ../Makefile.rules

LIBRARY = fftpack

LIBRARY_SRC = cfftb.c cfftb1.c cfftf.c \
		cfftf1.c cffti.c cffti1.c \
		cosqb.c cosqb1.c cosqf.c \
		cosqf1.c cosqi.c cost.c \
		costi.c dfilter.cc ezfft1.c \
		ezfftb.c ezfftf.c ezffti.c \
		fftpackC.c naff.c passb.c \
		passb2.c passb3.c passb4.c \
		passb5.c passf.c passf2.c \
		passf3.c passf4.c passf5.c \
		radb2.c radb3.c radb4.c \
		radb5.c radbg.c radf2.c \
		radf3.c radf4.c radf5.c \
		radfg.c rfftb.c rfftb1.c \
		rfftf.c rfftf1.c rffti.c \
		rffti1.c sinqb.c sinqf.c \
		sinqi.c sint.c sint1.c \
		sinti.c

CFLAGS += -I../include
CCFLAGS += -I../include

ifeq ($(OS), Linux)
  CFLAGS += -fopenmp
  CCFLAGS += -fopenmp
endif

ifeq ($(OS), Windows)
  CFLAGS += -openmp -DEXPORT_FFTPACK /wd4244
  CCFLAGS += -openmp -DEXPORT_FFTPACK /wd4244
  LIBRARY_LIBS = mdbmth.lib mdblib.lib
endif

include ../Makefile.build

