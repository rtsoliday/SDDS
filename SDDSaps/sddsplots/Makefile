DD  = ../
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../../Makefile.rules

PROD = sddsplot

sddsplot_SRC = SDDSaps.c \
                put_link_date.c \
                sddsplot.c \
                sddsplotAP.c \
                sddsplot3d.c \
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
  override CXXFLAGS += -fopenmp
  LDFLAGS += -fopenmp
  PROD_SYS_LIBS := $(GSL_LINK_LIBS) $(GD_LIB) $(PNG_LIB) $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbplt -lmdbcommon -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(GSL_LINK_LIBS) $(GD_LIB) $(PNG_LIB) $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbplt -lmdbcommon -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += -I../../png -I../../gd -I../../lzma -I../../zlib -DMOTIF_DEVICE=1 -wd4018 -wd4244 -wd4267 -wd4305
  PROD_LIBS = mdbplt.lib gd.lib png.lib mdbcommon.lib SDDS1.lib rpnlib.lib $(GSL_LINK_LIBS) mdbmth.lib mdblib.lib lzma.lib z.lib
  PROD_SYS_LIBS = advapi32.lib
  NO_DLL = 1
endif
CUSTOM_LINK_PRODUCTS += sddsplot

include ../../Makefile.build

# Special compilation rule for sddsplot.c to work around -O3 optimization bug
ifeq ($(OS), Linux)
$(OBJ_DIR)/sddsplot.o: sddsplot.c
	$(CC) $(filter-out -O%,$(CFLAGS)) -O0 $(DEPFLAGS) -c $< $(OUTPUT)
endif

$(OBJ_DIR)/sddsplot$(EXEEXT): $(sddsplot_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsplot_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)

.NOTPARALLEL: $(LIBRARY)

# The test driver exercises production extraction without a GUI or process wrapper.
ARRAY_TEST_DRIVER = $(OBJ_DIR)/test_sddsplot3d$(EXEEXT)
ARRAY_TEST_OBJS = $(OBJ_DIR)/test_sddsplot3d.$(OBJEXT) $(OBJ_DIR)/sddsplot3d.$(OBJEXT)
$(ARRAY_TEST_DRIVER): $(ARRAY_TEST_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(ARRAY_TEST_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)

.PHONY: array-test-driver
array-test-driver: $(ARRAY_TEST_DRIVER)

-include $(ARRAY_TEST_OBJS:.$(OBJEXT)=.d)
