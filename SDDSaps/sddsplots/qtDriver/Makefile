DD  = ../../
include ../../../Makefile.rules

PROD = mpl_qt
mpl_qt_SRC = mpl_qt.cc mpl_qt_utils.cc mpl_qt_zoom.cc mpl_qt_print_save.cc mpl_qt_readdata.cc

ifeq ($(OS), Linux)
  MOC = $(firstword $(wildcard \
          /usr/lib64/qt6/libexec/moc \
          /usr/lib/qt6/libexec/moc \
          /bin/moc \
          /usr/bin/moc \
          /bin/moc-qt5 \
          /usr/bin/moc-qt5))
  ifeq ($(strip $(MOC)),)
    $(warning Qt ‘moc’ not found – please install your Qt development packages; disabling mpl_qt build)
    PROD :=
  else
    MOC_VER_DIGIT := $(shell $(MOC) -v | cut -d' ' -f2 | cut -c1)
    CCFLAGS += -I../../../include -I$(OBJ_DIR) $(shell pkg-config --cflags Qt$(MOC_VER_DIGIT)Widgets)
    LDFLAGS += $(shell pkg-config --libs Qt$(MOC_VER_DIGIT)Widgets Qt$(MOC_VER_DIGIT)PrintSupport)
    PROD_LIBS = -lmdbplt -lSDDS1 -lrpnlib -lmdbmth -lmdblib
    PROD_SYS_LIBS = $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(GD_LIB) $(PNG_LIB) -lm
  endif
endif

ifeq ($(OS), Darwin)
  CCFLAGS += -I../../../include -I$(OBJ_DIR) $(shell pkg-config --cflags Qt5Widgets)
  LDFLAGS += $(shell pkg-config --libs Qt5Widgets Qt5PrintSupport)
  PROD_LIBS = -lmdbplt -lSDDS1 -lrpnlib -lmdbmth -lmdblib
  PROD_SYS_LIBS = $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(GD_LIB) $(PNG_LIB) -lm
  MOC = $(wildcard /opt/local/libexec/qt5/bin/moc)
  ifeq ($(MOC),)
    MOC = moc-qt5
  endif
endif

ifeq ($(OS), Windows)
  CCFLAGS += -I../../../include -I$(OBJ_DIR) /std:c++17 /Zc:__cplusplus /permissive- -I../../../../include -I"C:\Qt\6.8.2\msvc2022_64\include" -I"C:\Qt\6.8.2\msvc2022_64\include\QtCore" -I"C:\Qt\6.8.2\msvc2022_64\include\QtGui" -I"C:\Qt\6.8.2\msvc2022_64\include\QtWidgets"
  LDFLAGS += /LIBPATH:"C:\Qt\6.8.2\msvc2022_64\lib" Qt6PrintSupport.lib Qt6Widgets.lib Qt6Gui.lib Qt6Core.lib
  PROD_LIBS = mdbplt.lib gd.lib png.lib mdbcommon.lib SDDS1.lib rpnlib.lib gsl.lib gslcblas.lib mdbmth.lib mdblib.lib lzma.lib z.lib
  PROD_SYS_LIBS = advapi32.lib
  MOC = moc.exe
endif

include ../../../Makefile.build

$(OBJ_DIR)/mpl_qt_moc.h:
	$(MOC) mpl_qt.cc -o $(OBJ_DIR)/mpl_qt_moc.h

mpl_qt.cc: $(OBJ_DIR)/mpl_qt_moc.h

$(OBJ_DIR)/mpl_qt$(EXEEXT): $(mpl_qt_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(mpl_qt_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi
