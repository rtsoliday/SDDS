include ../Makefile.rules

LIBRARY = meschach

LIBRARY_SRC = bdfactor.c \
	bkpfacto.c \
	chfactor.c \
	copy.c \
	err.c \
	fft.c \
	givens.c \
	hessen.c \
	hsehldr.c \
	init.c \
	iter0.c \
	iternsym.c \
	itersym.c \
	ivecop.c \
	lufactor.c \
	machine.c \
	matlab.c \
	matop.c \
	matrixio.c \
	meminfo.c \
	memory.c \
	memstat.c \
	mfunc.c	 \
	norm.c \
	otherio.c \
	pxop.c \
	qrfactor.c \
	schur.c \
	solve.c \
	sparse.c \
	sparseio.c \
	spbkp.c \
	spchfctr.c \
	splufctr.c \
	sprow.c \
	spswap.c \
	submat.c \
	svd.c \
	symmeig.c \
	update.c \
	vecop.c \
	version.c \
	zcopy.c \
	zfunc.c \
	zgivens.c \
	zhessen.c \
	zhsehldr.c \
	zlufctr.c \
	zmachine.c \
	zmatio.c \
	zmatlab.c \
	zmatop.c \
	zmemory.c \
	znorm.c \
	zqrfctr.c \
	zschur.c \
	zsolve.c \
	zvecop.c

CFLAGS += -I../include

ifeq ($(OS), Windows)
  NO_DLL = 1
  CFLAGS += /wd4003 /wd4068 /wd4018 /wd4244 /wd4267 /wd4311
endif

include ../Makefile.build


