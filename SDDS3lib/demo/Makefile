DD=../
include ../../Makefile.rules

PROD = sddspp_write_demo sddspp_read_demo sddsppdemo

CCFLAGS += -std=c++17 -I../../include

ifeq ($(OS), Windows)
  PROD_LIBS = SDDSpp.lib lzma.lib z.lib
else
  PROD_LIBS = -lSDDSpp
  PROD_SYS_LIBS := $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
endif

include ../../Makefile.build
