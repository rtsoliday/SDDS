DD = ../
QT_MODULES := Core Gui Widgets PrintSupport
ifneq ($(origin QT_MAJOR),undefined)
QT_VERSION ?= $(QT_MAJOR)
endif
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../../Makefile.rules
.DEFAULT_GOAL := all
QT_MAJOR = $(QT_VERSION)
PROD = sddseditor
sddseditor_SRC = main.cc SDDSEditor.cc ArrayViewer.cc
HDF5_INCLUDE ?= /usr/include/hdf5/serial
USE_H5_16_API ?= 1

H5_API_FLAG :=
ifeq ($(USE_H5_16_API),1)
  H5_API_FLAG := -DH5_USE_16_API
endif

ifneq ($(HAVE_QT),1)
ifneq ($(BUILD_GOALS),)
$(warning Qt development modules unavailable; disabling sddseditor build)
endif
PROD :=
else
QT_REQUIRED := 1
CPPFLAGS += -I../../include -I$(OBJ_DIR) $(QT_CFLAGS) $(H5_API_FLAG)
override CXXFLAGS += $(if $(filter Windows,$(OS)),/Zc:__cplusplus /permissive- /utf-8 -DNOMINMAX)
LDFLAGS += $(QT_LIBS)
ifeq ($(OS),Windows)
CPPFLAGS += -I../../lzma -I$(HDF_DIR)/include
# Keep project warnings enabled while treating the installed Qt SDK as external.
override CXXFLAGS += /external:I"$(QT_DIR)/include" /external:W0
PROD_LIBS = SDDS1.lib rpnlib.lib mdbmth.lib mdblib.lib z.lib
PROD_SYS_LIBS = $(HDF_DIR)/lib/libhdf5.lib $(HDF_DIR)/lib/libszaec.lib $(HDF_DIR)/lib/libaec.lib $(LZMA_LIB) shlwapi.lib advapi32.lib
else
PROD_LIBS = -lSDDS1 -lrpnlib -lmdbmth -lmdblib
ifeq ($(OS),Linux)
HDF5_CFLAGS := $(shell $(PKG_CONFIG) --cflags hdf5 2>/dev/null)
CPPFLAGS += $(or $(HDF5_CFLAGS),-I$(HDF5_INCLUDE))
PROD_SYS_LIBS = $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LINK_LIBS) $(Z_LIB) -lm -ldl $(CURL_LIB) $(OPENSSL_CRYPTO_LINK_LIBS)
else
PROD_SYS_LIBS = $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LINK_LIBS) $(Z_LIB) $(DARWIN_OPENMP_LDFLAGS) -lm
endif
endif
endif
BUILD_OBJECTS += $(OBJ_DIR)/SDDSEditorTests.$(OBJEXT) $(OBJ_DIR)/PlotSnapshotProbe.$(OBJEXT)
BUILD_GENERATED += $(OBJ_DIR)/SDDSEditor_moc.h
BUILD_CONFIG_VARIABLES += USE_H5_16_API HDF5_INCLUDE
CUSTOM_LINK_PRODUCTS += sddseditor_tests
include ../../Makefile.build
$(OBJ_DIR)/SDDSEditor_moc.h: SDDSEditor.h | $(OBJ_DIR)
	$(MOC) $(QT_CFLAGS) $< -o $@
$(OBJ_DIR)/main.$(OBJEXT): SDDSEditor.h
$(OBJ_DIR)/SDDSEditor.$(OBJEXT): SDDSEditor.h ArrayViewer.h $(OBJ_DIR)/SDDSEditor_moc.h
$(OBJ_DIR)/ArrayViewer.$(OBJEXT): ArrayViewer.h

# Checked-in regression programs; all executables and fixtures stay in OBJ_DIR.
ifeq ($(OS), Windows)
  $(OBJ_DIR)/sddseditor_tests$(EXEEXT): LDFLAGS += Qt$(QT_MAJOR)Test.lib
else
  $(OBJ_DIR)/SDDSEditorTests.$(OBJEXT): CXXFLAGS += $(shell $(PKG_CONFIG) --cflags Qt$(QT_MAJOR)Test)
  $(OBJ_DIR)/sddseditor_tests$(EXEEXT): LDFLAGS += $(shell $(PKG_CONFIG) --libs Qt$(QT_MAJOR)Test)
endif

$(OBJ_DIR)/SDDSEditorTests.$(OBJEXT): SDDSEditor.cc SDDSEditor.h ArrayViewer.h $(OBJ_DIR)/SDDSEditor_moc.h

$(OBJ_DIR)/sddseditor_tests$(EXEEXT): $(OBJ_DIR)/SDDSEditorTests.$(OBJEXT) $(OBJ_DIR)/ArrayViewer.$(OBJEXT) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(OBJ_DIR)/SDDSEditorTests.$(OBJEXT) $(OBJ_DIR)/ArrayViewer.$(OBJEXT) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)

$(OBJ_DIR)/test-bin/sddsplot$(EXEEXT): $(OBJ_DIR)/PlotSnapshotProbe.$(OBJEXT)
	mkdir -p $(OBJ_DIR)/test-bin
	$(LINKEXE) $(OUTPUTEXE) $< $(LDFLAGS)

.PHONY: tests
tests: all $(OBJ_DIR)/sddseditor_tests$(EXEEXT) $(OBJ_DIR)/test-bin/sddsplot$(EXEEXT)
	QT_QPA_PLATFORM=offscreen ./$(OBJ_DIR)/sddseditor_tests$(EXEEXT)
