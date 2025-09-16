DD  = ../
include ../../Makefile.rules

PROD = sddscontour sddscongen sddshist2d sddsimageconvert

sddscontour_SRC = sddscontour.c \
                   draw_contours.c \
                   shade_grid.c \
                   plot_contours.c \
                   fft_interp.c

CFLAGS += -DRPN_SUPPORT -DzLib -I../../include -I../sddsplots

ifeq ($(OS), Linux)
  ifeq ($(words $(GLFW_LIB)),1)
    PROD += sddswaterfall
  endif
  CFLAGS += -fopenmp
  CCFLAGS += -fopenmp
  LDFLAGS += -fopenmp
  PROD_SYS_LIBS := $(GLFW_LIB) $(GL_LIB) $(GLU_LIB) $(GLUT_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(GD_LIB) $(PNG_LIB) $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbplt -lmdbcommon -lfftpack -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(GSL_LIB) $(GSLCBLAS_LIB) $(GD_LIB) $(PNG_LIB) $(LZMA_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lmdbplt -lmdbcommon -lfftpack -lSDDS1 -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += -I../../lzma -I../../zlib -wd4244 -wd4267
  PROD_LIBS = mdbplt.lib gd.lib png.lib mdbcommon.lib fftpack.lib SDDS1.lib rpnlib.lib gsl.lib gslcblas.lib mdbmth.lib mdblib.lib lzma.lib z.lib
  PROD_SYS_LIBS = advapi32.lib
endif

include ../../Makefile.build

$(OBJ_DIR)/sddscontour$(EXEEXT): $(sddscontour_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddscontour_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

