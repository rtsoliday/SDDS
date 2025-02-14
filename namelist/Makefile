include ../Makefile.rules

PROD = nlpp

nlpp_SRC = namelist_pp.c scanargs.c

LIBRARY = namelist

LIBRARY_SRC = escape_quotes.c \
          free_namelist.c \
          free_namelist_text.c \
          get_namelist.c \
          namelist_etc.c \
          print_namelist.c \
          process_namelist.c \
          scan_namelist.c \
          show_namelist.c \
          show_namelist_fields.c \
          zero_namelist.c

CFLAGS += -I../include

ifeq ($(OS), Linux)
  PROD_LIBS = -lnamelist -lrpnlib -lmdblib
endif

ifeq ($(OS), Darwin)
  PROD_LIBS = -lnamelist -lrpnlib -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += /wd4267
  PROD_LIBS = namelist.lib mdblib.lib
  NO_DLL = 1
endif

include ../Makefile.build

$(OBJ_DIR)/nlpp$(EXEEXT): $(nlpp_OBJS) $(LIBRARY) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(nlpp_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/

.NOTPARALLEL: $(LIBRARY)
