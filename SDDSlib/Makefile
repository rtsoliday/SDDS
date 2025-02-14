include ../Makefile.rules

LIBRARY = SDDS1

LIBRARY_SRC = SDDS_ascii.c \
          SDDS_binary.c \
          SDDS_copy.c \
          SDDS_data.c \
          SDDS_dataprep.c \
          SDDS_extract.c \
          SDDS_info.c \
          SDDS_input.c \
          SDDS_lzma.c \
          SDDS_mplsupport.c \
          SDDS_output.c \
          SDDS_process.c \
          SDDS_rpn.c \
          SDDS_transfer.c \
          SDDS_utils.c \
          SDDS_write.c

CFLAGS += -DzLib -DALLOW_FILE_LOCKING=1 -DRPN_SUPPORT -I../include

ifeq ($(OS), Windows)
  CFLAGS += -I../zlib -I../lzma -DEXPORT_SDDS /wd4244 /wd4267
  LIBRARY_LIBS = rpnlib.lib mdbmth.lib mdblib.lib lzma.lib z.lib
endif

include ../Makefile.build
