include ../Makefile.rules

ifeq ($(OS), Windows)

  LIBRARY = tiff

  LIBRARY_SRC = tif_aux.c \
	tif_close.c     tif_codec.c     tif_compress.c  tif_dir.c \
	tif_dirinfo.c   tif_dirread.c   tif_dirwrite.c  tif_dumpmode.c \
	tif_error.c     tif_fax3.c      tif_fax3sm.c    tif_flush.c \
	tif_getimage.c  tif_jpeg.c      tif_luv.c       tif_lzw.c \
	tif_next.c      tif_ojpeg.c     tif_open.c \
	tif_packbits.c  tif_pixarlog.c  tif_predict.c   tif_print.c \
	tif_read.c      tif_strip.c     tif_swab.c      tif_thunder.c \
	tif_tile.c      tif_version.c \
	tif_warning.c   tif_write.c \
	tif_zip.c       tif_msdos.c

  NO_DLL=1

  CFLAGS += -wd4244 -wd4267 -wd4311 -wd4312

  include ../Makefile.build

else

all:
	@echo Not building tiff on $(OS)

clean:
	@echo Skipping on $(OS)

.PHONY: all clean

endif

