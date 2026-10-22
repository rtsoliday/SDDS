ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../Makefile.rules

LIBRARY = SDDS1

LIBRARY_SRC = SDDS_ascii.c \
          SDDS_enum.c \
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

ENUM_TEST = $(OBJ_DIR)/test_sdds_enum$(EXEEXT)
ifeq ($(OS), Windows)
# The enum regression also exercises private routines absent from the DLL API.
ENUM_TEST_LIBS = $(LIBRARY_OBJS) $(addprefix $(LIB_DIR)/,$(LIBRARY_LIBS))
ENUM_TEST_OUTPUT = /Fo$@.obj /Fe$@
else
# Link the archive built here, not a previously installed copy.
ENUM_TEST_LIBS = $(LIBRARY) -L$(LIB_DIR) -lrpnlib -lmdbmth -lmdblib $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lm
ENUM_TEST_OUTPUT = -o $@
endif

$(ENUM_TEST): test_sdds_enum.c $(LIBRARY)
	$(CC) $(CFLAGS) $(ENUM_TEST_OUTPUT) test_sdds_enum.c $(ENUM_TEST_LIBS)

.PHONY: enum-test
# Installing keeps later consumers (the SDDSlib++ enum test) on this archive.
enum-test: $(ENUM_TEST) install-libraries
	mkdir -p $(OBJ_DIR)/test-artifacts/enum/edgecases $(OBJ_DIR)/test-artifacts/enum/api
	$(ENUM_TEST) $(OBJ_DIR)/test-artifacts/enum

# Instrumented builds remain in repository artifacts and are never installed.
ifneq ($(OS), Windows)
SANITIZER_CC ?= $(CC)
ifeq ($(OS), Linux)
ifneq ($(findstring gcc,$(SANITIZER_CC)),)
SANITIZER_RUNTIME_FLAGS = -static-libasan -static-libubsan
endif
endif
ENUM_SANITIZER_TEST = $(OBJ_DIR)/test_sdds_enum_sanitized
$(ENUM_SANITIZER_TEST): test_sdds_enum.c $(LIBRARY_SRC) ../include/SDDSenum.h ../include/SDDS.h | $(OBJ_DIR)
	$(SANITIZER_CC) $(CFLAGS) $(SANITIZER_RUNTIME_FLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -o $@ test_sdds_enum.c $(LIBRARY_SRC) $(filter-out $(LIBRARY),$(ENUM_TEST_LIBS))
.PHONY: enum-sanitizer-test
enum-sanitizer-test: $(ENUM_SANITIZER_TEST)
	mkdir -p $(OBJ_DIR)/test-artifacts/enum-sanitized/edgecases $(OBJ_DIR)/test-artifacts/enum-sanitized/api
	ASAN_OPTIONS=detect_leaks=0 $(ENUM_SANITIZER_TEST) $(OBJ_DIR)/test-artifacts/enum-sanitized
endif
