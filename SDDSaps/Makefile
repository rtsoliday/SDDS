include ../Makefile.rules

PROD = agilentArb2sdds \
        citi2sdds \
        col2sdds \
        convert_to_bdd \
        csv2sdds \
        elegant2genesis \
        image2sdds \
        lba2sdds \
        mcs2sdds \
        mpl2sdds \
        plaindata2sdds \
        raw2sdds \
        sdds2agilentArb \
        sdds2dfft \
        sdds2dinterpolate \
        sdds2headlessdata \
        sdds2math \
        sdds2mpl \
        sdds2plaindata \
        sdds2spreadsheet \
        sdds2stl \
        sdds2stream \
        sdds2tiff \
        sddsanalyticsignal \
        sddsarray2column \
        sddsbaseline \
        sddsbinarystring \
        sddsbreak \
        sddscast \
        sddschanges \
        sddscheck \
        sddscliptails \
        sddscollapse \
        sddscollect \
        sddscombine \
        sddscombinelogfiles \
        sddsconvert \
        sddsconvertalarmlog \
        sddsconvertlogonchange \
        sddsconvolve \
        sddscorrelate \
        sddsderef \
        sddsderiv \
        sddsdiff \
        sddsdigfilter \
        sddsdistest \
        sddsduplicate \
        sddsendian \
        sddsenvelope \
        sddseventhist \
        sddsexpand \
        sddsexpfit \
        sddsfdfilter \
        sddsfft \
        sddsfindin2dgrid \
        sddsgenericfit \
        sddsgfit \
        sddslorentzianfit \
        sddshist \
        sddsimageprofiles \
        sddsinsideboundaries \
        sddsinteg \
        sddsinterp \
        sddsinterpset \
        sddsmakedataset \
        sddsmatrixmult \
        sddsmatrix2column \
        sddsminterp \
        sddsmpfit \
        sddsmselect \
        sddsmultihist \
        sddsmxref \
        sddsnaff \
        sddsnormalize \
        sddsoutlier \
        sddspeakfind \
        sddspfit \
        sddspoly \
        sddsprintout \
        sddsprocess \
        sddsquery \
        sddsregroup \
        sddsremoveoffsets \
        sddsrespmatrixderivative \
        sddsrowstats \
        sddsrunstats \
        sddssampledist \
        sddsselect \
        sddsseparate \
        sddssequence \
        sddsshift \
        sddsshiftcor \
        sddssinefit \
        sddsslopes \
        sddssmooth \
        sddssnap2grid \
        sddssort \
        sddssortcolumn \
        sddssplit \
        sddsspotanalysis \
        sddstimeconvert \
        sddstranspose \
        sddstdrpeeling \
        sddsunwrap \
        sddsvslopes \
        sddsxref \
        sddszerofind \
        tdms2sdds \
        tek2sdds \
        TFS2sdds \
        tiff2sdds \
        wfm2sdds

# sddssplinefit
ifneq ($(OS), Windows)
  PROD += sddslogserver sddslogclient
  PROD += sddskde sddskde2d sddslocaldensity
  #PROD += agilentwaveform2sdds agilentcomm 
endif

ifeq ($(OS), Linux)
  PROD += stl2sdds
endif

sdds2mpl_SRC = sdds2mpl.c SDDSaps.c
sddsanalyticsignal_SRC = sddsanalyticsignal.c SDDSutils.c
sddscorrelate_SRC = sddscorrelate.c SDDSutils.c
sddsderiv_SRC = sddsderiv.c SDDSutils.c
sddsdistest_SRC = sddsdistest.c SDDSutils.c
sddsfdfilter_SRC = sddsfdfilter.c SDDSutils.c
sddsfft_SRC = sddsfft.c SDDSutils.c
sdds2dfft_SRC = sdds2dfft.c SDDSutils.c
sddsgenericfit_SRC = sddsgenericfit.c SDDSaps.c
sddsinteg_SRC = sddsinteg.c SDDSutils.c
sddsinterp_SRC = sddsinterp.c SDDSutils.c
sddskde_SRC = sddskde.c SDDSutils.c
sddskde2d_SRC = sddskde2d.c SDDSutils.c
sddsmultihist_SRC = sddsmultihist.c SDDSutils.c
sddsmxref_SRC = sddsmxref.c SDDSaps.c
sddsnaff_SRC = sddsnaff.c SDDSutils.c
sddsoutlier_SRC = sddsoutlier.c SDDSutils.c
sddsprocess_SRC = sddsprocess.c SDDSaps.c
sddsrowstats_SRC = sddsrowstats.c SDDSutils.c
sddsrunstats_SRC = sddsrunstats.c SDDSutils.c
sddsshiftcor_SRC = sddsshiftcor.c SDDSutils.c
sddssplinefit_SRC = sddssplinefit.c SDDSutils.c
sddsspotanalysis_SRC = sddsspotanalysis.c SDDSutils.c
sddsxref_SRC = sddsxref.c SDDSaps.c
sdds2tiff_SRC = sdds2tiff.c
tiff2sdds_SRC = tiff2sdds.c

ifneq ($(wildcard $(DD)../.svn),)
  SVN_VERSION=$(shell svnversion .)
else
  SVN_VERSION=GIT_$(shell git log --oneline | wc -l | sed 's/^[[:space:]]*//')
endif

CFLAGS += -DRPN_SUPPORT -DzLib -DSVN_VERSION=\"$(SVN_VERSION)\" -I../include -I../2d_interpolate/nn -I../2d_interpolate/csa -I../xlslib -DUSE_XLS
CCFLAGS += -DRPN_SUPPORT -DzLib -DSVN_VERSION=\"$(SVN_VERSION)\" -I../include -I../2d_interpolate/nn -I../2d_interpolate/csa -I../xlslib -DUSE_XLS

ifeq ($(OS), Linux)
  CFLAGS += -fopenmp
  CCFLAGS += -fopenmp
  LDFLAGS += -fopenmp
  PROD_SYS_LIBS := $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lcsa -lnnetwork -lmdbcommon -lmatlib -lfftpack -lSDDS1 -lrpnlib -lmdbmth -lmdblib -lxls
  TIFF_LIBS = $(TIFF_LIB)
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(ZSTD_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lcsa -lnnetwork -lmdbcommon -lmatlib -lfftpack -lSDDS1 -lrpnlib -lmdbmth -lmdblib -lxls
  TIFF_LIBS = $(TIFF_LIB) $(JPEG_LIB) $(LERC_LIB) $(DEFLATE_LIB)
endif

ifeq ($(OS), Windows)
  CFLAGS += -I../tiff -I../lzma -I../zlib -openmp -wd4018 -wd4244 -wd4267 -wd4305 -wd4311 -wd4312
  CCFLAGS += -I../tiff -I../lzma -I../zlib -openmp -wd4018 -wd4244 -wd4267 -wd4305 -wd4311 -wd4312
  PROD_LIBS = csa.lib nnetwork.lib mdbcommon.lib matlib.lib fftpack.lib SDDS1.lib rpnlib.lib gsl.lib gslcblas.lib mdbmth.lib mdblib.lib lzma.lib z.lib xls.lib
  TIFF_LIBS = tiff.lib 
endif

include ../Makefile.build

$(OBJ_DIR)/sdds2tiff$(EXEEXT): $(sdds2tiff_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sdds2tiff_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(TIFF_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/tiff2sdds$(EXEEXT): $(tiff2sdds_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(tiff2sdds_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(TIFF_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sdds2mpl$(EXEEXT): $(sdds2mpl_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sdds2mpl_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsanalyticsignal$(EXEEXT): $(sddsanalyticsignal_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsanalyticsignal_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddscorrelate$(EXEEXT): $(sddscorrelate_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddscorrelate_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsderiv$(EXEEXT): $(sddsderiv_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsderiv_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsdistest$(EXEEXT): $(sddsdistest_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsdistest_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsfdfilter$(EXEEXT): $(sddsfdfilter_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsfdfilter_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsfft$(EXEEXT): $(sddsfft_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsfft_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sdds2dfft$(EXEEXT): $(sdds2dfft_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sdds2dfft_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsgenericfit$(EXEEXT): $(sddsgenericfit_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsgenericfit_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsinteg$(EXEEXT): $(sddsinteg_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsinteg_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsinterp$(EXEEXT): $(sddsinterp_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsinterp_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddskde$(EXEEXT): $(sddskde_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddskde_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddskde2d$(EXEEXT): $(sddskde2d_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddskde2d_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsmultihist$(EXEEXT): $(sddsmultihist_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsmultihist_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsmxref$(EXEEXT): $(sddsmxref_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsmxref_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsnaff$(EXEEXT): $(sddsnaff_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsnaff_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsoutlier$(EXEEXT): $(sddsoutlier_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsoutlier_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsprocess$(EXEEXT): $(sddsprocess_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsprocess_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsrowstats$(EXEEXT): $(sddsrowstats_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsrowstats_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsrunstats$(EXEEXT): $(sddsrunstats_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsrunstats_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsshiftcor$(EXEEXT): $(sddsshiftcor_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsshiftcor_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddssplinefit$(EXEEXT): $(sddssplinefit_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddssplinefit_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsspotanalysis$(EXEEXT): $(sddsspotanalysis_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsspotanalysis_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsxref$(EXEEXT): $(sddsxref_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsxref_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

