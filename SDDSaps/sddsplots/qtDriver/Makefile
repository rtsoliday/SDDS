DD  = ../../
include ../../../Makefile.rules

PROD = mpl_qt mpl_qt_3d bely_upload
MPL_QT_SRC = mpl_qt.cc mpl_qt_utils.cc mpl_qt_zoom.cc mpl_qt_print_save.cc mpl_qt_readdata.cc
mpl_qt_SRC = $(MPL_QT_SRC)
mpl_qt_3d_SRC =
bely_upload_SRC = bely_upload.cc

ifeq ($(OS), Linux)
  # Preserve the Qt 5 Data Visualization runtime used by RHEL8 builds on RHEL9.
  QT5_DATAVISUALIZATION_COMPAT_LIB = /usr/local/oag/apps/lib/linux-x86_64/libQt5DataVisualization.so.5
  ifneq ($(wildcard $(QT5_DATAVISUALIZATION_COMPAT_LIB)),)
    QT5_DATAVISUALIZATION_COMPAT_RPATH = -Wl,-rpath=/usr/local/oag/apps/lib/linux-x86_64
  endif
  MOC = $(firstword $(wildcard \
          /usr/lib64/qt6/libexec/moc \
          /usr/lib/qt6/libexec/moc \
          /usr/libexec/qt6-moc \
          /usr/libexec/qt6/moc \
          /bin/moc \
          /usr/bin/moc \
          /bin/moc-qt5 \
          /usr/bin/moc-qt5))
  ifeq ($(strip $(MOC)),)
    $(warning Qt ‘moc’ not found – please install your Qt development packages; disabling mpl_qt build)
    PROD :=
  else
    MOC_VER_DIGIT := $(shell $(MOC) -v | cut -d' ' -f2 | cut -c1)
    QT_PKG_LIST := Qt$(MOC_VER_DIGIT)Widgets Qt$(MOC_VER_DIGIT)Network Qt$(MOC_VER_DIGIT)PrintSupport Qt$(MOC_VER_DIGIT)DataVisualization
    PKG_CONFIG_AVAILABLE := $(shell pkg-config --version >/dev/null 2>&1 && echo yes)
    ifneq ($(PKG_CONFIG_AVAILABLE),yes)
      $(warning pkg-config not found - please install your Qt pkg-config files; disabling mpl_qt build)
      PROD :=
    else
      QT_PKGS_PRESENT := $(shell pkg-config --exists $(QT_PKG_LIST) && echo yes)
      ifneq ($(QT_PKGS_PRESENT),yes)
        $(warning Required Qt pkg-config modules ($(QT_PKG_LIST)) not found - disabling mpl_qt build)
        PROD :=
      else
        CCFLAGS += -I../../../include -I$(OBJ_DIR) $(shell pkg-config --cflags Qt$(MOC_VER_DIGIT)Widgets Qt$(MOC_VER_DIGIT)Network Qt$(MOC_VER_DIGIT)DataVisualization)
        LDFLAGS += $(shell pkg-config --libs Qt$(MOC_VER_DIGIT)Widgets Qt$(MOC_VER_DIGIT)PrintSupport Qt$(MOC_VER_DIGIT)Network)
        MPL_QT_3D_LDFLAGS = $(shell pkg-config --libs Qt$(MOC_VER_DIGIT)DataVisualization) $(QT5_DATAVISUALIZATION_COMPAT_RPATH)
        PROD_LIBS = -lmdbplt -lSDDS1 -lrpnlib -lmdbmth -lmdblib
        PROD_SYS_LIBS = $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(GD_LIB) $(PNG_LIB) -lm
      endif
    endif
  endif
endif

ifeq ($(OS), Darwin)
  MOC = $(wildcard /opt/local/libexec/qt5/bin/moc)
  ifeq ($(MOC),)
    MOC = moc-qt5
  endif
  PKG_CONFIG_AVAILABLE := $(shell pkg-config --version >/dev/null 2>&1 && echo yes)
  ifneq ($(PKG_CONFIG_AVAILABLE),yes)
    $(warning pkg-config not found - please install your Qt pkg-config files; disabling mpl_qt build)
    PROD :=
  else
    QT_PKG_LIST := Qt5Widgets Qt5Network Qt5PrintSupport Qt5DataVisualization
    QT_PKGS_PRESENT := $(shell pkg-config --exists $(QT_PKG_LIST) && echo yes)
    ifneq ($(QT_PKGS_PRESENT),yes)
      $(warning Required Qt pkg-config modules ($(QT_PKG_LIST)) not found - disabling mpl_qt build)
      PROD :=
    else
      CCFLAGS += -I../../../include -I$(OBJ_DIR) $(shell pkg-config --cflags Qt5Widgets Qt5Network Qt5DataVisualization)
      LDFLAGS += $(shell pkg-config --libs Qt5Widgets Qt5PrintSupport Qt5Network) -framework ApplicationServices
      MPL_QT_3D_LDFLAGS = $(shell pkg-config --libs Qt5DataVisualization)
      PROD_LIBS = -lmdbplt -lSDDS1 -lrpnlib -lmdbmth -lmdblib
      PROD_SYS_LIBS = $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(GD_LIB) $(PNG_LIB) -lm
    endif
  endif
endif

  ifeq ($(OS), Windows)
  CCFLAGS += -I../../../include -I$(OBJ_DIR) /std:c++17 /Zc:__cplusplus /permissive- -I../../../../include -I"C:\Qt\6.8.2\msvc2022_64\include" -I"C:\Qt\6.8.2\msvc2022_64\include\QtCore" -I"C:\Qt\6.8.2\msvc2022_64\include\QtGui" -I"C:\Qt\6.8.2\msvc2022_64\include\QtWidgets" -I"C:\Qt\6.8.2\msvc2022_64\include\QtDataVisualization"
  LDFLAGS += /LIBPATH:"C:\Qt\6.8.2\msvc2022_64\lib" Qt6PrintSupport.lib Qt6Widgets.lib Qt6Network.lib Qt6Gui.lib Qt6Core.lib
  MPL_QT_3D_LDFLAGS = Qt6DataVisualization.lib
  PROD_LIBS = mdbplt.lib gd.lib png.lib mdbcommon.lib SDDS1.lib rpnlib.lib gsl.lib gslcblas.lib mdbmth.lib mdblib.lib lzma.lib z.lib
  PROD_SYS_LIBS = advapi32.lib
  MOC = moc.exe
endif

include ../../../Makefile.build

MPL_QT_3D_OBJS = $(patsubst %.cc,$(OBJ_DIR)/%_3d.$(OBJEXT),$(MPL_QT_SRC))

$(OBJ_DIR)/%_3d.$(OBJEXT): %.cc
	$(CCC) $(CCFLAGS) -DMPL_QT_ENABLE_3D $(DEPFLAGS) -c $< $(OUTPUT)

$(OBJ_DIR)/mpl_qt_moc.h:
	$(MOC) mpl_qt.cc -o $(OBJ_DIR)/mpl_qt_moc.h

$(OBJ_DIR)/mpl_qt_3d_moc.h:
	$(MOC) -DMPL_QT_ENABLE_3D mpl_qt.cc -o $(OBJ_DIR)/mpl_qt_3d_moc.h

mpl_qt.cc: $(OBJ_DIR)/mpl_qt_moc.h $(OBJ_DIR)/mpl_qt_3d_moc.h

$(OBJ_DIR)/mpl_qt$(EXEEXT): $(mpl_qt_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(mpl_qt_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	# Replace the destination inode atomically.  Overwriting a running or
	# previously launched Mach-O in place can leave macOS's signature cache
	# associated with the old contents, causing the rebuilt program to receive
	# SIGKILL at startup.
	cp -f $@ $(BIN_DIR)/$(@F).new
	mv -f $(BIN_DIR)/$(@F).new $(BIN_DIR)/$(@F)
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/$(@F).new; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo mv -f $(EPICS_BIN_DIR)/$(@F).new $(EPICS_BIN_DIR)/$(@F); fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/$(@F).new && mv -f $(EPICS_BIN_DIR)/$(@F).new $(EPICS_BIN_DIR)/$(@F); fi

$(OBJ_DIR)/mpl_qt_3d$(EXEEXT): $(MPL_QT_3D_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(MPL_QT_3D_OBJS) $(LDFLAGS) $(MPL_QT_3D_LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/$(@F).new
	mv -f $(BIN_DIR)/$(@F).new $(BIN_DIR)/$(@F)
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/$(@F).new; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo mv -f $(EPICS_BIN_DIR)/$(@F).new $(EPICS_BIN_DIR)/$(@F); fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/$(@F).new && mv -f $(EPICS_BIN_DIR)/$(@F).new $(EPICS_BIN_DIR)/$(@F); fi
