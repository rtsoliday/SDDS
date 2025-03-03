DD = ../
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
        rpn_csh.c \
        rpn_data.c \
        rpn_draw.c \
        rpn_error.c \
        rpn_io.c \
        rpn_sub.c \
        stack.c \
        udf.c

CFLAGS += -I../../include -DUSE_GSL
CCFLAGS += -I../../include -DUSE_GSL

ifeq ($(OS), Linux)
  CFLAGS += -fopenmp
  CCFLAGS += -fopenmp
  PROD_SYS_LIBS := $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lrpnlib -lmdbmth -lmdblib 
endif

ifeq ($(OS), Darwin)
  PROD_SYS_LIBS := $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS = -lrpnlib -lmdbmth -lmdblib 
endif

ifeq ($(OS), Windows)
  CFLAGS += /wd4244 /wd4267 -I../../../gsl
  LIBRARY_CFLAGS = -DEXPORT_RPNLIB
  LIBRARY_LIBS = gsl.lib gslcblas.lib mdbmth.lib mdblib.lib
  PROD_LIBS = rpnlib.lib mdbmth.lib mdblib.lib
endif

include ../../Makefile.build

.NOTPARALLEL: $(LIBRARY)
