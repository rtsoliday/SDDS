DD = ../
include ../../Makefile.rules

LIBRARY = csa

LIBRARY_SRC = minell_csa.c svd.c csa.c

CFLAGS += -I../nn -I../../include

ifeq ($(OS), Windows)
  LIBRARY_LIBS += mdbmth.lib
  NO_DLL = 1
else
  LIBRARY_LIBS += -lmdbmth
endif

include ../../Makefile.build
