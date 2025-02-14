DD = ../
include ../../Makefile.rules

LIBRARY = csa

LIBRARY_SRC = minell_csa.c svd.c csa.c

CFLAGS += -I../nn

ifeq ($(OS), Windows)
  NO_DLL = 1
endif

include ../../Makefile.build
