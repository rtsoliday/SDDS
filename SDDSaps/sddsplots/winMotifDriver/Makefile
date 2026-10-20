DD  = ../../
# Retain the compiler-default C++ mode for legacy Windows builds.
WINDOWS_CXX_STD_FLAGS :=
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../../../Makefile.rules

ifeq ($(OS), Windows)
  PROD = mpl_motif

  mpl_motif_SRC = mpl_motif.cc

  override CXXFLAGS += -I../../../gd -wd4244 -wd4267
  PROD_LIBS = gd.lib png.lib 
  PROD_SYS_LIBS = user32.lib gdi32.lib comdlg32.lib vfw32.lib netapi32.lib
CUSTOM_LINK_PRODUCTS += mpl_motif

  include ../../../Makefile.build

$(OBJ_DIR)/mpl_motif$(EXEEXT): $(mpl_motif_OBJS) $(PROD_DEPS)
	rc -l 0x409 -fo $(OBJ_DIR)/mpl_motif.res mpl_motif.rc
	$(LINKEXE) /SUBSYSTEM:WINDOWS $(OUTPUTEXE) $(mpl_motif_OBJS) $(OBJ_DIR)/mpl_motif.res $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)

else

all:
	@echo Not building winMotifDriver on $(OS)

clean:
	@echo Skipping on $(OS)

.PHONY: all clean

endif

