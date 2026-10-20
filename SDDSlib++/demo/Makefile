DD=../
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../../Makefile.rules

PROD = sddspp_write_demo sddspp_read_demo sddsppdemo

ifeq ($(OS), Windows)
  override CXXFLAGS += /wd4251 /wd4275 /wd4996 -I../../include
  PROD_LIBS = SDDSpp.lib lzma.lib z.lib
else
  override CXXFLAGS += -I../../include
  PROD_LIBS = -lSDDSpp
  PROD_SYS_LIBS := $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
endif

include ../../Makefile.build
