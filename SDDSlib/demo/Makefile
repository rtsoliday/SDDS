DD=../
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../../Makefile.rules

PROD = sdds_write_demo sdds_read_demo


CFLAGS += -DzLib  -I../../include

ifeq ($(OS), Linux)
  PROD_SYS_LIBS := $(LZMA_LIB) $(GSL_LINK_LIBS) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(LZMA_LIB) $(GSL_LINK_LIBS) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += -I../../lzma -I../../zlib
  PROD_LIBS = SDDS1.lib rpnlib.lib $(GSL_LINK_LIBS) mdbmth.lib mdblib.lib lzma.lib z.lib
endif

include ../../Makefile.build


