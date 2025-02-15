include ../Makefile.rules

LIBRARY = levmar

LIBRARY_SRC = lm.c Axb.c misc.c lmlec.c lmbc.c

CFLAGS += -DHAVE_LAPACK -DUSE_MPI=1 -DSDDS_MPI_IO=1 -DMASTER_READTITLE_ONLY -I../include

ifeq ($(OS), Linux)
  CFLAGS += -Warray-parameter=1
endif

ifeq ($(OS), Windows)
  CFLAGS += $(WIN_MPI_INC) -wd4244 -wd4305
  LIBRARY_SYS_LIBS = $(WIN_MPI_LIB)
  NO_DLL = 1
endif

include ../Makefile.build

