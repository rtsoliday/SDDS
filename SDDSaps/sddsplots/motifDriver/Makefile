DD  = ../../
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../../../Makefile.rules

ifneq ($(OS), Windows)
  PROD = mpl_motif

  mpl_motif_SRC = mpl_motif.cc xwdfunc.c

  CFLAGS += -I../../../include -I$(MOTIF_INC) -I$(X11_INC) -DXREL5 -DzLib

  PROD_LIBS = -lmdbplt -lSDDS1 -lrpnlib -lmdbmth -lmdblib
  PROD_SYS_LIBS = $(LZMA_LIB) $(GSL_LINK_LIBS) $(XM_LIB) $(JPEG_LIB) $(XMU_LIB) $(XT_LIB) $(ICE_LIB) $(SM_LIB) $(XP_LIB) $(XAU_LIB) $(XFT_LIB) $(XEXT_LIB) $(X11_LIB) $(FONTCONFIG_LIB) $(ICONV_LIB) $(Z_LIB) $(GD_LIB) $(PNG_LIB) $(DARWIN_OPENMP_LDFLAGS) -lm
CUSTOM_LINK_PRODUCTS += mpl_motif

  include ../../../Makefile.build

$(OBJ_DIR)/mpl_motif$(EXEEXT): $(mpl_motif_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(mpl_motif_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)


else

all:
	@echo Not building motifDriver on $(OS)

clean:
	@echo Skipping on $(OS)

.PHONY: all clean

endif

