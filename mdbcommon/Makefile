include ../Makefile.rules

LIBRARY = mdbcommon

LIBRARY_SRC = fixcounts.c find_files.c  \
		lsfg.c lsfn.c \
		lsfp.c savitzkyGolay.c scanargs.c \
		table.c hashtab.c lookupa.c recycle.c rcds_powell.c

CFLAGS += -I../include

ifeq ($(OS), Windows)
  CFLAGS += -I../lzma -DEXPORT_MDBCOMMON /wd4244 /wd4267
  LIBRARY_LIBS = matlib.lib fftpack.lib SDDS1.lib mdbmth.lib mdblib.lib
endif

include ../Makefile.build
