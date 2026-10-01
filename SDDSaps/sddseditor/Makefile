DD  = ../
include ../../Makefile.rules

PROD = sddseditor
sddseditor_SRC = main.cc SDDSEditor.cc ArrayViewer.cc

HDF5_INCLUDE ?= /usr/include/hdf5/serial
USE_H5_16_API ?= 1

H5_API_FLAG :=
ifeq ($(USE_H5_16_API),1)
  H5_API_FLAG := -DH5_USE_16_API
endif

ifneq ($(filter Linux Darwin,$(OS)),)
  PKG_CONFIG ?= pkg-config
  # Retain the macOS Qt 5 default when both versions are installed.
  QT_PREFERENCE = 6 5
  ifeq ($(OS), Darwin)
    QT_PREFERENCE = 5 6
  endif
  ifeq ($(origin QT_MAJOR),undefined)
    ifneq ($(strip $(MOC)),)
      QT_MAJOR := $(shell "$(MOC)" -v 2>&1 | sed -n 's/.* \([56]\)\..*/\1/p')
    else
      QT_MAJOR := $(firstword $(foreach version,$(QT_PREFERENCE),$(shell $(PKG_CONFIG) --exists Qt$(version)Widgets && echo $(version))))
    endif
  endif
  ifeq ($(strip $(QT_MAJOR)),)
    $(warning Qt development packages not found; disabling sddseditor build)
    PROD :=
  else
    ifeq ($(filter 5 6,$(QT_MAJOR)),)
      $(error QT_MAJOR must be 5 or 6)
    endif
    QT_PACKAGES = Qt$(QT_MAJOR)Widgets Qt$(QT_MAJOR)PrintSupport
    ifneq ($(shell $(PKG_CONFIG) --exists $(QT_PACKAGES) && echo yes),yes)
      $(error Missing Qt development packages: $(QT_PACKAGES))
    endif
    QT_HOST_BINS := $(shell $(PKG_CONFIG) --variable=host_bins Qt$(QT_MAJOR)Core)
    QT_LIBEXECDIR := $(shell $(PKG_CONFIG) --variable=libexecdir Qt$(QT_MAJOR)Core)
    MOC ?= $(firstword $(wildcard \
        $(QT_LIBEXECDIR)/moc $(QT_HOST_BINS)/moc \
        /opt/local/libexec/qt$(QT_MAJOR)/libexec/moc \
        /opt/local/libexec/qt$(QT_MAJOR)/bin/moc \
        /usr/lib64/qt$(QT_MAJOR)/libexec/moc \
        /usr/lib/qt$(QT_MAJOR)/libexec/moc \
        /usr/libexec/qt$(QT_MAJOR)-moc /usr/libexec/qt$(QT_MAJOR)/moc \
        /usr/lib64/qt$(QT_MAJOR)/bin/moc /usr/lib/qt$(QT_MAJOR)/bin/moc) \
        $(shell command -v moc-qt$(QT_MAJOR) 2>/dev/null) \
        $(shell command -v moc$(QT_MAJOR) 2>/dev/null) \
        $(shell command -v moc 2>/dev/null))
    ifeq ($(strip $(MOC)),)
      $(error Qt $(QT_MAJOR) moc not found; set MOC to the matching executable)
    endif
    MOC_MAJOR := $(shell "$(MOC)" -v 2>&1 | sed -n 's/.* \([56]\)\..*/\1/p')
    ifneq ($(MOC_MAJOR),$(QT_MAJOR))
      $(error MOC=$(MOC) is not Qt $(QT_MAJOR); select a matching code generator)
    endif
    CCFLAGS += -I../../include -I$(OBJ_DIR) $(shell $(PKG_CONFIG) --cflags $(QT_PACKAGES)) $(H5_API_FLAG)
    ifeq ($(QT_MAJOR),6)
      CCFLAGS += -std=c++17
      LDFLAGS += -std=c++17
    endif
    LDFLAGS += $(shell $(PKG_CONFIG) --libs $(QT_PACKAGES))
    PROD_LIBS = -lSDDS1 -lrpnlib -lmdbmth -lmdblib
    ifeq ($(OS), Linux)
      HDF5_CFLAGS := $(shell $(PKG_CONFIG) --cflags hdf5 2>/dev/null)
      ifeq ($(strip $(HDF5_CFLAGS)),)
        HDF5_CFLAGS := -I$(HDF5_INCLUDE)
      endif
      CCFLAGS += $(HDF5_CFLAGS)
      PROD_SYS_LIBS = $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lm -ldl $(CURL_LIB) $(OPENSSL_COMPAT_RPATH) $(CRYPTO_LIB)
    else
      PROD_SYS_LIBS = $(HDF5_LIB) $(SZ_LIB) $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lm
    endif
  endif
endif

ifeq ($(OS), Windows)
  QT_DIR ?= C:/Qt/6.8.2/msvc2022_64
  MOC ?= $(QT_DIR)/bin/moc.exe
  MOC_MAJOR := $(shell "$(MOC)" -v 2>&1 | sed -n 's/.* \([56]\)\..*/\1/p')
  QT_MAJOR ?= $(MOC_MAJOR)
  ifeq ($(filter 5 6,$(QT_MAJOR)),)
    $(error Qt moc not found; set QT_DIR to the installed Qt 5 or Qt 6 MSVC kit)
  endif
  ifneq ($(MOC_MAJOR),$(QT_MAJOR))
    $(error MOC=$(MOC) is not Qt $(QT_MAJOR))
  endif
  # Qt text and test fixtures are UTF-8; Windows SDK macros must not reach Qt/STL.
  CCFLAGS += -I../../include -I../../lzma -I$(HDF_DIR)/include -I$(OBJ_DIR) /std:c++17 /Zc:__cplusplus /permissive- /utf-8 -DNOMINMAX -I"$(QT_DIR)/include" -I"$(QT_DIR)/include/QtCore" -I"$(QT_DIR)/include/QtGui" -I"$(QT_DIR)/include/QtWidgets"
  LDFLAGS += /LIBPATH:"$(QT_DIR)/lib" Qt$(QT_MAJOR)PrintSupport.lib Qt$(QT_MAJOR)Widgets.lib Qt$(QT_MAJOR)Gui.lib Qt$(QT_MAJOR)Core.lib
  PROD_LIBS = SDDS1.lib rpnlib.lib mdbmth.lib mdblib.lib z.lib
  PROD_SYS_LIBS = $(HDF_DIR)/lib/libhdf5.lib $(HDF_DIR)/lib/libszaec.lib $(HDF_DIR)/lib/libaec.lib $(LZMA_LIB) shlwapi.lib advapi32.lib
endif

include ../../Makefile.build

# Switching Qt versions or kits must regenerate moc output and all Qt objects.
.PHONY: force-qt-config
$(OBJ_DIR)/qt-build-config: force-qt-config | $(OBJ_DIR)
	@printf '%s\n' '$(MOC)' '$(CCFLAGS)' '$(LDFLAGS)' > $@.new
	@if cmp -s $@.new $@; then rm -f $@.new; else mv -f $@.new $@; fi

$(sddseditor_OBJS) $(OBJ_DIR)/SDDSEditorTests.$(OBJEXT) $(OBJ_DIR)/PlotSnapshotProbe.$(OBJEXT): $(OBJ_DIR)/qt-build-config

$(OBJ_DIR)/SDDSEditor_moc.h: SDDSEditor.h $(OBJ_DIR)/qt-build-config | $(OBJ_DIR)
	"$(MOC)" SDDSEditor.h -o $(OBJ_DIR)/SDDSEditor_moc.h

SDDSEditor.cc: $(OBJ_DIR)/SDDSEditor_moc.h

# MSVC builds write no .d files, so name the header dependencies explicitly.
$(OBJ_DIR)/main.$(OBJEXT): SDDSEditor.h
$(OBJ_DIR)/SDDSEditor.$(OBJEXT): SDDSEditor.h ArrayViewer.h $(OBJ_DIR)/SDDSEditor_moc.h
$(OBJ_DIR)/ArrayViewer.$(OBJEXT): ArrayViewer.h

$(OBJ_DIR)/sddseditor$(EXEEXT): $(sddseditor_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddseditor_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	# Replace the inode to avoid stale macOS code-signature caches after a rebuild.
	cp -f $@ $(BIN_DIR)/$(@F).new
	mv -f $(BIN_DIR)/$(@F).new $(BIN_DIR)/$(@F)
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/$(@F).new && mv -f $(EPICS_BIN_DIR)/$(@F).new $(EPICS_BIN_DIR)/$(@F); fi

# Checked-in regression programs; all executables and fixtures stay in OBJ_DIR.
$(OBJ_DIR)/SDDSEditorTests.$(OBJEXT): SDDSEditor.cc SDDSEditor.h ArrayViewer.h $(OBJ_DIR)/SDDSEditor_moc.h

$(OBJ_DIR)/sddseditor_tests$(EXEEXT): $(OBJ_DIR)/SDDSEditorTests.$(OBJEXT) $(OBJ_DIR)/ArrayViewer.$(OBJEXT) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(OBJ_DIR)/SDDSEditorTests.$(OBJEXT) $(OBJ_DIR)/ArrayViewer.$(OBJEXT) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)

$(OBJ_DIR)/test-bin/sddsplot$(EXEEXT): $(OBJ_DIR)/PlotSnapshotProbe.$(OBJEXT)
	mkdir -p $(OBJ_DIR)/test-bin
	$(LINKEXE) $(OUTPUTEXE) $< $(LDFLAGS)

.PHONY: tests
tests: all $(OBJ_DIR)/sddseditor_tests$(EXEEXT) $(OBJ_DIR)/test-bin/sddsplot$(EXEEXT)
	QT_QPA_PLATFORM=offscreen ./$(OBJ_DIR)/sddseditor_tests$(EXEEXT)
