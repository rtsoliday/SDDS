include ../Makefile.rules

LIBRARY = pgapack

LIBRARY_SRC = binary.c \
          char.c \
          cmdline.c \
          create.c \
          cross.c \
          debug.c \
          duplcate.c \
          evaluate.c \
          fitness.c \
          f2c.c \
          hamming.c \
          heap.c \
          integer.c \
          mutation.c \
          parallel.c \
          pga.c \
          pop.c \
          random.c \
          real.c \
          report.c \
          restart.c \
          select.c \
          stop.c \
          system.c \
          user.c \
          utility.c

CFLAGS += -DWL=32 -DFORTRANUNDERSCORE -DOPTIMIZE -I. -I../include

ifeq ($(OS), Linux)
  CC = $(MPI_CC)
  ifneq ($(IMPROV_BUILD),1)
    CFLAGS += -Wno-maybe-uninitialized
  endif
endif

ifeq ($(OS), Darwin)
  CC = $(MPI_CC)
endif

ifeq ($(OS), Windows)
  CFLAGS += $(WIN_MPI_INC) -wd4018 -wd4244 -wd4311
  LIBRARY_SYS_LIBS = $(WIN_MPI_LIB)
  NO_DLL = 1
endif

include ../Makefile.build
