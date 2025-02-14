include ../Makefile.rules

LIBRARY = matlib

LIBRARY_SRC = fmat.c m_add.c m_alloc.c \
		m_bench.c m_check.c m_copy.c \
		m_det.c m_error.c m_free.c \
		m_identity.c m_invert.c m_mult.c \
		m_scmul.c m_show.c m_trans.c \
		m_zero.c

CFLAGS += -I../include

ifeq ($(OS), Windows)
  CFLAGS += -DEXPORT_MATLIB /wd4244
  LIBRARY_LIBS = mdbmth.lib mdblib.lib
endif

include ../Makefile.build

