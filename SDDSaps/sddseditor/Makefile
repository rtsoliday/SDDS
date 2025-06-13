DD  = ../
include ../../Makefile.rules

PROD = sddseditor
sddseditor_SRC = main.cc SDDSEditor.cc

ifeq ($(OS), Linux)
  MOC = $(firstword $(wildcard \
          /usr/lib64/qt6/libexec/moc \
          /usr/lib/qt6/libexec/moc \
          /bin/moc \
          /usr/bin/moc \
          /bin/moc-qt5 \
          /usr/bin/moc-qt5))
  MOC_VER_DIGIT := $(shell $(MOC) -v | cut -d' ' -f2 | cut -c1)
  CCFLAGS += -I../../include -I$(OBJ_DIR) $(shell pkg-config --cflags Qt$(MOC_VER_DIGIT)Widgets) -DH5_USE_16_API -I/usr/include/hdf5/serial
  LDFLAGS += $(shell pkg-config --libs Qt$(MOC_VER_DIGIT)Widgets Qt$(MOC_VER_DIGIT)PrintSupport)
  PROD_LIBS = -lSDDS1 -lrpnlib -lmdbmth -lmdblib
  PROD_SYS_LIBS = $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lm -ldl
endif

ifeq ($(OS), Darwin)
  CCFLAGS += -I../../include -I$(OBJ_DIR) $(shell pkg-config --cflags Qt5Widgets) -DH5_USE_16_API
  LDFLAGS += $(shell pkg-config --libs Qt5Widgets Qt5PrintSupport)
  PROD_LIBS = -lSDDS1 -lrpnlib -lmdbmth -lmdblib
  PROD_SYS_LIBS = $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lm
  MOC = $(wildcard /opt/local/libexec/qt5/bin/moc)
  ifeq ($(MOC),)
    MOC = moc-qt5
  endif
endif

ifeq ($(OS), Windows)
  CCFLAGS += -I../../include -I../../lzma -I$(HDF_DIR)/include -I$(OBJ_DIR) /std:c++17 /Zc:__cplusplus /permissive- -I../../../../include -I"C:\\Qt\\6.8.2\\msvc2022_64\\include" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtCore" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtGui" -I"C:\\Qt\\6.8.2\\msvc2022_64\\include\\QtWidgets"
  LDFLAGS += /LIBPATH:"C:\\Qt\\6.8.2\\msvc2022_64\\lib" Qt6PrintSupport.lib Qt6Widgets.lib Qt6Gui.lib Qt6Core.lib
  PROD_LIBS = SDDS1.lib rpnlib.lib mdbmth.lib mdblib.lib z.lib
  PROD_SYS_LIBS = $(HDF_DIR)/lib/libhdf5.lib $(HDF_DIR)/lib/libszaec.lib $(HDF_DIR)/lib/libaec.lib $(LZMA_LIB) shlwapi.lib advapi32.lib
  MOC = moc.exe
endif

include ../../Makefile.build

$(OBJ_DIR)/SDDSEditor_moc.h:
	$(MOC) SDDSEditor.h -o $(OBJ_DIR)/SDDSEditor_moc.h

SDDSEditor.cc: $(OBJ_DIR)/SDDSEditor_moc.h

$(OBJ_DIR)/sddseditor$(EXEEXT): $(sddseditor_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddseditor_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi
