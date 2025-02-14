DD = ../
include ../../Makefile.rules

LIBRARY = nnetwork

LIBRARY_SRC = delaunay.c \
          distribute.c \
          hash.c \
          istack.c \
          lpi.c \
          minell.c \
          nnai.c \
          nncommon.c \
          nncommon-vulnerable.c \
          nnpi.c \
          preader.c \
          triangle.c \
          version.c \
          nn_2d_interpolate.c

CFLAGS += -I../../include -DTRILIBRARY

ifeq ($(OS), Windows)
  CFLAGS += -wd4311 -wd4312
  NO_DLL = 1
endif

include ../../Makefile.build
