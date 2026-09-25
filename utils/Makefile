include ../Makefile.rules

PROD = editstring replaceText isFileLocked hdf2sdds sdds2hdf
PROD += mecho minpath tmpname token timeconvert tcomp

CFLAGS += -DH5_USE_16_API -I../include

ifeq ($(OS), Linux)
  # The RHEL 8 compatibility build links the repository's static HDF5 library.
  # Put its headers first as well, rather than compiling against the older
  # system headers and producing a binary that aborts during HDF5 startup.
  ifneq ($(wildcard ../hdf5/src/hdf5.h),)
    HDF5_INCLUDE_DIR ?= ../hdf5/src
  else ifneq ($(findstring libhdf5_serial,$(HDF5_LIB)),)
    HDF5_INCLUDE_DIR ?= /usr/include/hdf5/serial
  endif
  ifneq ($(HDF5_INCLUDE_DIR),)
    CFLAGS := -I$(HDF5_INCLUDE_DIR) $(CFLAGS)
    ifneq ($(wildcard $(HDF5_INCLUDE_DIR)/H5FDsubfiling/H5FDsubfiling.h),)
      CFLAGS := -I$(HDF5_INCLUDE_DIR)/H5FDsubfiling $(CFLAGS)
    endif
  endif
  PROD_SYS_LIBS := $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(CURL_LIB) $(OPENSSL_COMPAT_RPATH) $(SSL_LIB) $(CRYPTO_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbcommon -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbcommon -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += -I../lzma -I$(HDF_DIR)/include -DEXPORT_MDBCOMMON -wd4267
  PROD_LIBS = mdbcommon.lib SDDS1.lib gsl.lib gslcblas.lib mdbmth.lib mdblib.lib z.lib 
  PROD_SYS_LIBS := $(HDF_DIR)/lib/libhdf5.lib $(HDF_DIR)/lib/libszaec.lib $(HDF_DIR)/lib/libaec.lib shlwapi.lib $(PROD_SYS_LIBS)
endif

include ../Makefile.build
