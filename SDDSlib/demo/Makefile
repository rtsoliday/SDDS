DD=../
include ../../Makefile.rules

PROD = sdds_write_demo sdds_read_demo


CFLAGS += -DzLib  -I../../include

ifeq ($(OS), Linux)
  PROD_SYS_LIBS := $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += -I../../lzma -I../../zlib
  PROD_LIBS = SDDS1.lib rpnlib.lib mdbmth.lib mdblib.lib lzma.lib z.lib
endif

include ../../Makefile.build


