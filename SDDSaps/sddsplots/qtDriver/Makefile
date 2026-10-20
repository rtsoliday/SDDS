DD = ../../
QT_MODULES := Core Gui Widgets Network PrintSupport DataVisualization
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../../../Makefile.rules
.DEFAULT_GOAL := all
PROD = mpl_qt bely_upload
mpl_qt_SRC = mpl_qt.cc mpl_qt_utils.cc mpl_qt_zoom.cc mpl_qt_print_save.cc mpl_qt_readdata.cc
bely_upload_SRC = bely_upload.cc
ifneq ($(HAVE_QT),1)
ifneq ($(BUILD_GOALS),)
$(warning Qt development modules unavailable; disabling mpl_qt build)
endif
PROD :=
else
QT_REQUIRED := 1
CPPFLAGS += -I../../../include -I$(OBJ_DIR) $(QT_CFLAGS)
override CXXFLAGS += $(if $(filter Windows,$(OS)),/Zc:__cplusplus /permissive-)
LDFLAGS += $(QT_LIBS)
ifeq ($(OS),Linux)
# Preserve the RHEL 8 Qt 5 Data Visualization compatibility runtime.
QT5_DATAVISUALIZATION_COMPAT_LIB = /usr/local/oag/apps/lib/linux-x86_64/libQt5DataVisualization.so.5
ifneq ($(wildcard $(QT5_DATAVISUALIZATION_COMPAT_LIB)),)
LDFLAGS += -Wl,-rpath=/usr/local/oag/apps/lib/linux-x86_64
endif
endif
ifeq ($(OS),Windows)
PROD_LIBS = mdbplt.lib gd.lib png.lib mdbcommon.lib SDDS1.lib rpnlib.lib $(GSL_LINK_LIBS) mdbmth.lib mdblib.lib lzma.lib z.lib
PROD_SYS_LIBS = advapi32.lib
else
PROD_LIBS = -lmdbplt -lSDDS1 -lrpnlib -lmdbmth -lmdblib
PROD_SYS_LIBS = $(LZMA_LIB) $(GSL_LINK_LIBS) $(Z_LIB) $(GD_LIB) $(PNG_LIB) $(DARWIN_OPENMP_LDFLAGS) -lm
endif
ifeq ($(OS),Darwin)
# Native foreground activation uses ApplicationServices and the Objective-C runtime.
mpl_qt_LIBS = $(PROD_LIBS) $(PROD_SYS_LIBS) -framework ApplicationServices -lobjc
endif
endif
BUILD_GENERATED += $(OBJ_DIR)/mpl_qt_moc.h
include ../../../Makefile.build
$(mpl_qt_OBJS): CPPFLAGS += -DMPL_QT_ENABLE_3D
$(OBJ_DIR)/mpl_qt_moc.h: mpl_qt.cc | $(OBJ_DIR)
	$(MOC) $(QT_CFLAGS) -DMPL_QT_ENABLE_3D $< -o $@
$(OBJ_DIR)/mpl_qt.$(OBJEXT): $(OBJ_DIR)/mpl_qt_moc.h
