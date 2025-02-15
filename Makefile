include Makefile.rules

DIRS = meschach
DIRS += xlslib
DIRS += zlib
DIRS += lzma
DIRS += mdblib
DIRS += mdbmth
DIRS += rpns/code
DIRS += namelist
DIRS += SDDSlib
DIRS += SDDSlib/demo
DIRS += fftpack
DIRS += matlib
DIRS += mdbcommon
DIRS += utils
DIRS += 2d_interpolate/nn
DIRS += 2d_interpolate/csa
DIRS += png
DIRS += gd
DIRS += tiff
DIRS += SDDSaps
DIRS += SDDSaps/sddsplots
DIRS += SDDSaps/sddsplots/winMotifDriver
DIRS += SDDSaps/sddsplots/motifDriver
DIRS += SDDSaps/sddscontours
DIRS += SDDSaps/pseudoInverse
DIRS += levmar
ifneq ($(MPI_CC),)
DIRS += pgapack
endif

.PHONY: all $(DIRS) clean distclean

all: $(DIRS)

meschach:
	$(MAKE) -C $@
xlslib: meschach
	$(MAKE) -C $@
zlib: xlslib
	$(MAKE) -C $@
lzma: zlib
	$(MAKE) -C $@
mdblib: lzma
	$(MAKE) -C $@
mdbmth: mdblib
	$(MAKE) -C $@
rpns/code: mdbmth
	$(MAKE) -C $@
namelist: rpns/code
	$(MAKE) -C $@
ifeq ($(MPI_CC),)
SDDSlib: namelist
	$(MAKE) -C $@
else
SDDSlib: namelist
	$(MAKE) -C $@
	$(MAKE) -C $@ -f Makefile.mpi
endif
SDDSlib/demo: SDDSlib
	$(MAKE) -C $@
fftpack: SDDSlib/demo
	$(MAKE) -C $@
matlib: fftpack
	$(MAKE) -C $@
mdbcommon: matlib
	$(MAKE) -C $@
utils: mdbcommon
	$(MAKE) -C $@
2d_interpolate/nn: utils
	$(MAKE) -C $@
2d_interpolate/csa: 2d_interpolate/nn
	$(MAKE) -C $@
png: 2d_interpolate/csa
	$(MAKE) -C $@
gd: png
	$(MAKE) -C $@
tiff: gd
	$(MAKE) -C $@
SDDSaps: tiff
	$(MAKE) -C $@
SDDSaps/sddsplots: SDDSaps
	$(MAKE) -C $@
SDDSaps/sddsplots/winMotifDriver: SDDSaps/sddsplots
	$(MAKE) -C $@
SDDSaps/sddsplots/motifDriver: SDDSaps/sddsplots/winMotifDriver
	$(MAKE) -C $@
SDDSaps/sddscontours: SDDSaps/sddsplots/motifDriver
	$(MAKE) -C $@
SDDSaps/pseudoInverse: SDDSaps/sddscontours
	$(MAKE) -C $@
levmar: SDDSaps/pseudoInverse
	$(MAKE) -C $@
ifneq ($(MPI_CC),)
pgapack: levmar
	$(MAKE) -C $@
endif

clean:
	$(MAKE) -C meschach clean
	$(MAKE) -C xlslib clean
	$(MAKE) -C zlib clean
	$(MAKE) -C lzma clean
	$(MAKE) -C mdblib clean
	$(MAKE) -C mdbmth clean
	$(MAKE) -C rpns/code clean
	$(MAKE) -C namelist clean
	$(MAKE) -C SDDSlib clean
	$(MAKE) -C SDDSlib/demo clean
	$(MAKE) -C fftpack clean
	$(MAKE) -C matlib clean
	$(MAKE) -C mdbcommon clean
	$(MAKE) -C utils clean
	$(MAKE) -C 2d_interpolate/nn clean
	$(MAKE) -C 2d_interpolate/csa clean
	$(MAKE) -C png clean
	$(MAKE) -C gd clean
	$(MAKE) -C tiff clean
	$(MAKE) -C SDDSaps clean
	$(MAKE) -C SDDSaps/sddsplots clean
	$(MAKE) -C SDDSaps/sddsplots/winMotifDriver clean
	$(MAKE) -C SDDSaps/sddsplots/motifDriver clean
	$(MAKE) -C SDDSaps/sddscontours clean
	$(MAKE) -C SDDSaps/pseudoInverse clean
	$(MAKE) -C levmar clean
ifneq ($(MPI_CC),)
	$(MAKE) -C pgapack clean
endif

distclean: clean
	rm -rf bin/$(OS)-$(ARCH)
	rm -rf lib/$(OS)-$(ARCH)
