/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/


/* file: rpn.h
 * Michael Borland, 1988
 */
#include "mdb.h"
#include "rpn.h"

#undef epicsShareFuncRPNLIB
#if defined(_WIN32) && !defined(__CYGWIN32__) 
#if defined(EXPORT_RPNLIB)
#define epicsShareFuncRPNLIB  extern __declspec(dllexport)
#else
#define epicsShareFuncRPNLIB  extern __declspec(dllimport)
#endif
#else
#define epicsShareFuncRPNLIB extern
#endif

/* linked-list node structure for User-Defined Functions */
struct UDF {
    char *udf_name;
    char *udf_string;
    long udf_num;
    long start_index;
    long end_index;
    };
short is_udf(char *string);

/* structure of data for each user-callable function */
struct FUNCTION {
    char *keyword;       /* name by which user invokes function */
    char *descrip;       /* description of function (for help)  */
    void (*fn)(void);    /* pointer to the function             */
    long type;            /* type of function                    */
    } ;
/* function types */
#define NUMERIC_FUNC 1
#define LOGICAL_FUNC 2
#define OTHER_FUNC 3

/* array of function structures */
#ifdef USE_GSL
 #ifdef USE_GSL_FRESNEL
  #define NFUNCS (108+2)
 #else
  #define NFUNCS (106+2)
 #endif
#else
 #define NFUNCS (104+2)
#endif
/*it was 88 before, added ssto and streq, strlt, strgt, and strmatch */
epicsShareFuncRPNLIB struct FUNCTION funcRPN[NFUNCS];

epicsShareFuncRPNLIB struct UDF **udf_list;
epicsShareFuncRPNLIB long num_udfs;
epicsShareFuncRPNLIB long max_udfs;
epicsShareFuncRPNLIB long udf_changed;

extern double *memoryData;
extern char **str_memoryData;
epicsShareFuncRPNLIB long memory_added;
epicsShareFuncRPNLIB long n_memories;

/* stack for computations */
epicsShareFuncRPNLIB double stack[STACKSIZE];
epicsShareFuncRPNLIB long stackptr;

/*stack for long numbers */
epicsShareFuncRPNLIB long dstack[STACKSIZE];

/* stack that replaces PCODE */
typedef struct {
    short type;
    short index;
    double data;
    char *keyword;
    } UDF_CODE;
epicsShareFuncRPNLIB UDF_CODE *udf_stack; 
epicsShareFuncRPNLIB long udf_stackptr;
epicsShareFuncRPNLIB long max_udf_stackptr;

/* stack used to replace recursion in execute.c */
typedef struct {
    long udf_start_index;
    long udf_end_index;
    } UDF_INDEX;
epicsShareFuncRPNLIB UDF_INDEX *udf_id;
epicsShareFuncRPNLIB long cycle_counter;
epicsShareFuncRPNLIB long cycle_counter_stop;
extern long max_cycle_counter;

/* stack that is used to locate breakpoints in conditional statements */
typedef struct {
    long cond_colon;
    long cond_dollar;
    } UDF_CONDITIONAL;
epicsShareFuncRPNLIB UDF_CONDITIONAL *udf_cond_stack;
epicsShareFuncRPNLIB long udf_cond_stackptr;
epicsShareFuncRPNLIB long max_udf_cond_stackptr;

/* stack to quickly reference unknown objects in udfs */
typedef struct {
    long index;
    char *keyword;
    } UDF_UNKNOWN;
epicsShareFuncRPNLIB UDF_UNKNOWN *udf_unknown;
extern long udf_unknownptr, max_udf_unknown_counter;

/* stack of pointers to arrays for array implementation */
typedef struct {
    double *data;
    long rows;
    } RPN_ARRAY;
epicsShareFuncRPNLIB RPN_ARRAY *astack;
epicsShareFuncRPNLIB long astackptr;
extern long max_astackptr;

/* stack for strings */
extern char *sstack[STACKSIZE];

/* structure for stack of code strings */
#define CODE_LEN 16384
struct CODE {
    char *text;                /* text of code */
    long position;              /* position of next token in text */
    char *token;               /* pointer to next part of current token */
    long storage_mode;          /* controls free()'ing of *text */
#define STATIC 0
#define VOLATILE 1
    char *buffer;              /* buffer for get_token_rpn() */
#define LBUFFER 256
    struct CODE *pred, *succ;  /* list links */
    } ;
epicsShareFuncRPNLIB struct CODE code;		/* root node */
epicsShareFuncRPNLIB struct CODE *code_ptr;           /* will point to current node */
epicsShareFuncRPNLIB long code_lev;                    /* number of links */

/* stack for logical operations */
#define LOGICSTACKSIZE 500
epicsShareFuncRPNLIB short logicstack[LOGICSTACKSIZE];
epicsShareFuncRPNLIB long lstackptr;


/* structure and stack for command input files */
#define FILESTACKSIZE 10

struct INPUT_FILE {
    FILE *fp;
    long filemode;
    } ;

/* input file modes */
#define ECHO 0
#define NO_ECHO 1

epicsShareFuncRPNLIB struct INPUT_FILE input_stack[FILESTACKSIZE];
epicsShareFuncRPNLIB long istackptr;

/* structure and array (not stack) for user IO files */
struct IO_FILE {
    FILE *fp;
    char *name;
    long mode;
#define INPUT 1
#define OUTPUT 2
    } ;

epicsShareFuncRPNLIB struct IO_FILE io_file[FILESTACKSIZE];

/* values to indicate scientific notation or non-scientific notation output */
#define SCIENTIFIC 0
#define NO_SCIENTIFIC 1
#define USER_SPECIFIED 2
epicsShareFuncRPNLIB long format_flag;

/* flag to indicate trace/notrace mode */
extern long do_trace;

extern char *additional_help;

double rpn_internal(char *expression);

#ifdef USE_GSL
#include "gsl/gsl_errno.h"
#endif
