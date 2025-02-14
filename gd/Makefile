include ../Makefile.rules

ifeq ($(OS), Windows)

  LIBRARY = gd

  LIBRARY_SRC = gd.c \
          gd_color.c \
          gd_color_map.c \
          gd_transform.c \
          gdfx.c \
          gd_security.c \
          gd_gd.c \
          gd_gd2.c \
          gd_io.c \
          gd_io_dp.c \
          gd_gif_in.c \
          gd_gif_out.c \
          gd_io_file.c \
          gd_io_ss.c \
          gd_jpeg.c \
          gd_png.c \
          gd_ss.c \
          gd_topal.c \
          gd_wbmp.c \
          gdcache.c \
          gdfontg.c \
          gdfontl.c \
          gdfontmb.c \
          gdfonts.c \
          gdfontt.c \
          gdft.c \
          gdhelpers.c \
          gdkanji.c \
          gdtables.c \
          gdxpm.c \
          wbmp.c \
          gd_filter.c \
          gd_nnquant.c \
          gd_rotate.c \
          gd_matrix.c \
          gd_interpolation.c \
          gd_crop.c \
          webpimg.c \
          gd_webp.c \
          gd_tiff.c \
          gd_tga.c \
          gd_bmp.c \
          gd_xbm.c \
          gd_color_match.c

  CFLAGS += -I../zlib -DHAVE_CONFIG_H -DEXPORTS -DBGDWIN32 -I../png -wd4018 -wd4244 -wd4267
  LIBRARY_LIBS = png.lib z.lib

  include ../Makefile.build

else

all:
	@echo Not building gd on $(OS)

clean:
	@echo Skipping on $(OS)

.PHONY: all clean

endif

