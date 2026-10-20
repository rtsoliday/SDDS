DD  = ../
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../../Makefile.rules

PROD = sddspseudoinverse sddsmatrixop sdds2dpfit sddsica

sddsmatrixop_SRC = matrix.c sddsmatrixop.c
sdds2dpfit_SRC = matrix.c sdds2dpfit.c
sddsica_SRC = matrix.c sddsica.c

CFLAGS += -DzLib -I../../include
ifneq ($(OS), Windows)
  CFLAGS += -Og
endif

ifeq ($(OS), Linux)
  ifeq ($(MKL_CFLAG),)
    ifeq ($(LAPACK_CFLAG),)
	  USE_MATLIB = 1
    endif
  endif
  ifeq ($(USE_MATLIB),1)
    PROD_SYS_LIBS := $(MKL_LIB) $(LAPACK_LIB) $(GSL_LINK_LIBS) $(GD_LIB) $(PNG_LIB) $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
    PROD_LIBS = -lmdbcommon -lmatlib -lfftpack -lSDDS1 -lrpnlib -lmdbmth -lmdblib -lmeschach
  else
    CFLAGS += -fopenmp $(MKL_CFLAG) $(MKL_INCLUDE) $(LAPACK_CFLAG) $(LAPACK_INCLUDE)
    LDFLAGS += -fopenmp

    PROD_SYS_LIBS := $(MKL_LIB) $(LAPACK_LIB) $(GSL_LINK_LIBS) $(GD_LIB) $(PNG_LIB) $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
    PROD_LIBS = -lmdbcommon -lfftpack -lSDDS1 -lrpnlib -lmdbmth -lmdblib -lmeschach 
  endif
endif

ifeq ($(OS), Darwin)
  CFLAGS += -DCLAPACK
  PROD_SYS_LIBS := -framework Accelerate $(GSL_LINK_LIBS) $(GD_LIB) $(PNG_LIB) $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbcommon -lfftpack -lSDDS1 -lrpnlib -lmdbmth -lmdblib -lmeschach 
endif

ifeq ($(OS), Windows)
  CFLAGS += -I../../lzma -I../../zlib -wd4018 -wd4244 -wd4267 -wd4311 -wd4312 -openmp $(MKL_CFLAG) $(MKL_INCLUDE)
  PROD_SYS_LIBS := $(MKL_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = mdbplt.lib gd.lib png.lib mdbcommon.lib matlib.lib fftpack.lib SDDS1.lib rpnlib.lib $(GSL_LINK_LIBS) mdbmth.lib mdblib.lib lzma.lib z.lib meschach.lib
endif
CUSTOM_LINK_PRODUCTS += sddsmatrixop sdds2dpfit sddsica

include ../../Makefile.build

$(OBJ_DIR)/sddsmatrixop$(EXEEXT): $(sddsmatrixop_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsmatrixop_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)

$(OBJ_DIR)/sdds2dpfit$(EXEEXT): $(sdds2dpfit_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sdds2dpfit_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)

$(OBJ_DIR)/sddsica$(EXEEXT): $(sddsica_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsica_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)

