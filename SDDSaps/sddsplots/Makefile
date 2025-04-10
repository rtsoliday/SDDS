DD  = ../
include ../../Makefile.rules

PROD = sddsplot

sddsplot_SRC = SDDSaps.c \
                put_link_date.c \
                sddsplot.c \
                sddsplotAP.c \
                sddsplotConvert.c \
                sddsplotFilter.c \
                sddsplotLabels.c \
                sddsplotMapping.c \
                sddsplotRead.c \
                sddsplotRecord.c \
                sddsplotXfer.c \
                sddsplotGroup.c

LIBRARY = mdbplt

LIBRARY_SRC = term.c arrows.c \
          plot_data_sets.c \
          make_scales.c \
          graphics.c \
          psymbol.c \
          hersheyfont.c

CFLAGS += -DUSE_GD_LIBRARY -DzLib -I../../include

ifeq ($(OS), Linux)
  CFLAGS += -fopenmp
  CCFLAGS += -fopenmp
  ifeq ($(IMPROV_BUILD),1)
    CFLAGS += "-DMOTIF_DEVICE=1"
  endif
  LDFLAGS += -fopenmp
  PROD_SYS_LIBS := $(GSL_LIB) $(GSLCBLAS_LIB) $(GD_LIB) $(PNG_LIB) $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbplt -lmdbcommon -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(GSL_LIB) $(GSLCBLAS_LIB) $(GD_LIB) $(PNG_LIB) $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbplt -lmdbcommon -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += -I../../png -I../../gd -I../../lzma -I../../zlib -DMOTIF_DEVICE=1 -wd4018 -wd4244 -wd4267 -wd4305
  PROD_LIBS = mdbplt.lib gd.lib png.lib mdbcommon.lib SDDS1.lib rpnlib.lib gsl.lib gslcblas.lib mdbmth.lib mdblib.lib lzma.lib z.lib
  PROD_SYS_LIBS = advapi32.lib
  NO_DLL = 1
endif

include ../../Makefile.build

$(OBJ_DIR)/sddsplot$(EXEEXT): $(sddsplot_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsplot_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

.NOTPARALLEL: $(LIBRARY)
