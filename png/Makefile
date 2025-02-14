include ../Makefile.rules

ifeq ($(OS), Windows)

  LIBRARY = png

  LIBRARY_SRC = png.c \
          pngerror.c \
          pngget.c \
          pngmem.c \
          pngpread.c \
          pngread.c \
          pngrio.c \
          pngrtran.c \
          pngrutil.c \
          pngset.c \
          pngtrans.c \
          pngwio.c \
          pngwrite.c \
          pngwtran.c \
          pngwutil.c

  NO_DLL=1

  CFLAGS += -I. -I../zlib -DHAVE_CONFIG_H

  include ../Makefile.build

else

all:
	@echo Not building png on $(OS)

clean:
	@echo Skipping on $(OS)

.PHONY: all clean

endif

