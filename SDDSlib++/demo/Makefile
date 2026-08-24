DD=../
include ../../Makefile.rules

PROD = sddspp_write_demo sddspp_read_demo sddsppdemo

ifeq ($(OS), Windows)
  CCFLAGS += /std:c++17 /wd4251 /wd4275 /wd4996 -I../../include
  PROD_LIBS = SDDSpp.lib lzma.lib z.lib
else
  CCFLAGS += -std=c++17 -I../../include
  PROD_LIBS = -lSDDSpp
  PROD_SYS_LIBS := $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
endif

include ../../Makefile.build
