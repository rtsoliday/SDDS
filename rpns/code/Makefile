DD = ../
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../../Makefile.rules

PROD = rpn rpnl if2pf

LIBRARY = rpnlib

LIBRARY_SRC = array.c \
        conditional.c \
        execute.c \
        get_token_rpn.c \
        infixtopostfix.c \
        logical.c \
        math.c \
        memory.c \
        pcode.c \
        pop_push.c \
        prompt.c \
        rpn_readline.c \
        rpn_lock.c \
        rpn_csh.c \
        rpn_data.c \
        rpn_draw.c \
        rpn_error.c \
        rpn_io.c \
        rpn_sub.c \
        stack.c \
        udf.c

CFLAGS += -I../../include -DUSE_GSL
override CXXFLAGS += -I../../include -DUSE_GSL

ifeq ($(OS), Linux)
  # An EPICS extensions build pairs its local libgsl with the local headers.
  GSL_LOCAL_INC = $(if $(and $(filter 1,$(USE_EPICS_DIRS)),$(wildcard $(RULES_DIR)gsl/gsl)),-I$(RULES_DIR)gsl)
  CFLAGS += -fopenmp $(GSL_LOCAL_INC)
  override CXXFLAGS += -fopenmp $(GSL_LOCAL_INC)
  PROD_SYS_LIBS := $(GSL_LINK_LIBS) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lrpnlib -lmdbmth -lmdblib 
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(GSL_LINK_LIBS) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lrpnlib -lmdbmth -lmdblib 
endif

ifeq ($(OS), Windows)
  CFLAGS += /wd4244 /wd4267 -I../../../gsl
  LIBRARY_CFLAGS = -DEXPORT_RPNLIB
  LIBRARY_LIBS = $(GSL_LINK_LIBS) mdbmth.lib mdblib.lib
  PROD_LIBS = rpnlib.lib mdbmth.lib mdblib.lib
endif

include ../../Makefile.build

.NOTPARALLEL:
