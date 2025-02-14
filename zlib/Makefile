include ../Makefile.rules

ifeq ($(OS), Windows)

  LIBRARY = z

  LIBRARY_SRC = adler32.c \
        compress.c \
        crc32.c \
        gzclose.c \
        gzlib.c \
        gzread.c \
        gzwrite.c \
        uncompr.c \
        deflate.c \
        trees.c \
        zutil.c \
        inflate.c \
        infback.c \
        inftrees.c \
        inffast.c

  NO_DLL = 1
  CFLAGS += -D_LARGEFILE64_SOURCE=1

  include ../Makefile.build

else

all:
	@echo Not building zlib on $(OS)

clean:
	@echo Skipping on $(OS)

.PHONY: all clean

endif

