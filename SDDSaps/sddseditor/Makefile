DD  = ../
include ../../Makefile.rules

PROD = sddseditor
sddseditor_SRC = main.cc SDDSEditor.cc

HDF5_INCLUDE ?= /usr/include/hdf5/serial
USE_H5_16_API ?= 1

H5_API_FLAG :=
ifeq ($(USE_H5_16_API),1)
  H5_API_FLAG := -DH5_USE_16_API
endif

ifeq ($(OS), Linux)
  MOC ?= $(firstword $(wildcard \
          /usr/lib64/qt6/libexec/moc \
          /usr/lib/qt6/libexec/moc \
          /usr/libexec/qt6-moc \
          /bin/moc \
          /usr/bin/moc \
          /bin/moc-qt5 \
          /usr/bin/moc-qt5) \
          $(shell command -v moc 2>/dev/null) \
          $(shell command -v moc-qt5 2>/dev/null))
  ifeq ($(strip $(MOC)),)
    $(warning Qt 'moc' not found - please install your Qt development packages; disabling sddseditor build)
    PROD :=
  else
    MOC_VER_OUTPUT := $(shell $(MOC) -v 2>&1)
    QT_MAJOR_FROM_MOC := $(shell echo "$(MOC_VER_OUTPUT)" | sed -n 's/.*[^0-9]\([0-9][0-9]*\)\..*/\1/p' | head -n 1)
    QT_MAJOR ?= $(QT_MAJOR_FROM_MOC)
    ifeq ($(strip $(QT_MAJOR)),)
      QT_MAJOR := 6
      $(warning Could not parse Qt major version from '$(MOC_VER_OUTPUT)'; defaulting QT_MAJOR=$(QT_MAJOR))
    endif

    QT_PKG_MAJOR := $(shell if pkg-config --exists Qt$(QT_MAJOR)Widgets Qt$(QT_MAJOR)PrintSupport; then \
      echo $(QT_MAJOR); \
    elif pkg-config --exists Qt6Widgets Qt6PrintSupport; then \
      echo 6; \
    elif pkg-config --exists Qt5Widgets Qt5PrintSupport; then \
      echo 5; \
    fi)

    ifeq ($(strip $(QT_PKG_MAJOR)),)
      $(warning Qt pkg-config modules not found (tried Qt$(QT_MAJOR), Qt6, Qt5); disabling sddseditor build)
      PROD :=
    else
      HDF5_CFLAGS := $(shell pkg-config --cflags hdf5 2>/dev/null)
      ifeq ($(strip $(HDF5_CFLAGS)),)
        HDF5_CFLAGS := -I$(HDF5_INCLUDE)
      endif

      CCFLAGS += -I../../include -I$(OBJ_DIR) $(shell pkg-config --cflags Qt$(QT_PKG_MAJOR)Widgets) $(H5_API_FLAG) $(HDF5_CFLAGS)
      LDFLAGS += $(shell pkg-config --libs Qt$(QT_PKG_MAJOR)Widgets Qt$(QT_PKG_MAJOR)PrintSupport)
      PROD_LIBS = -lSDDS1 -lrpnlib -lmdbmth -lmdblib
      PROD_SYS_LIBS = $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lm -ldl $(CURL_LIB) $(CRYPTO_LIB)
    endif
  endif
endif

ifeq ($(OS), Darwin)
  CCFLAGS += -I../../include -I$(OBJ_DIR) $(shell pkg-config --cflags Qt5Widgets) $(H5_API_FLAG)
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
