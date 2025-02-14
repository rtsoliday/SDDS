include ../Makefile.rules

PROD = editstring replaceText isFileLocked hdf2sdds sdds2hdf

CFLAGS += -DH5_USE_16_API -I../include

ifeq ($(OS), Linux)
  PROD_SYS_LIBS := $(HDF5_LIB) $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbcommon -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbcommon -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += -I../lzma -I$(HDF_DIR)/include -DEXPORT_MDBCOMMON -wd4267
  PROD_LIBS = mdbcommon.lib SDDS1.lib mdbmth.lib mdblib.lib z.lib 
  PROD_SYS_LIBS := $(HDF_DIR)/lib/libhdf5.lib $(HDF_DIR)/lib/libsz.lib $(HDF_DIR)/lib/libaec.lib $(PROD_SYS_LIBS)
endif

include ../Makefile.build
