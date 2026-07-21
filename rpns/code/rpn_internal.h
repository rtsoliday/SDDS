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

#if defined(_WIN32) && !defined(__CYGWIN32__) && !defined(EXPORT_RPNLIB)
#define RPN_USE_TLS_ACCESSORS 1
#endif

/* linked-list node structure for User-Defined Functions */
struct UDF {
    char *udf_name;
    char *udf_string;
    long udf_num;
    long start_index;
    long end_index;
    };
long is_udf(char *string);

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
  #define NFUNCS (110+2)
 #else
  #define NFUNCS (108+2)
 #endif
#else
 #define NFUNCS (106+2)
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
epicsShareFuncRPNLIB double *rpn_stack_ptr(void);
epicsShareFuncRPNLIB long *rpn_stackptr_ptr(void);
epicsShareFuncRPNLIB long *rpn_dstack_ptr(void);
#if defined(RPN_USE_TLS_ACCESSORS)
#define stack (rpn_stack_ptr())
#define stackptr (*rpn_stackptr_ptr())
#define dstack (rpn_dstack_ptr())
#else
extern RPN_THREAD_LOCAL double stack[STACKSIZE];
extern RPN_THREAD_LOCAL long stackptr;

/*stack for long numbers */
extern RPN_THREAD_LOCAL long dstack[STACKSIZE];
#endif

/* stack that replaces PCODE */
typedef struct {
    long type;
    long index;
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
epicsShareFuncRPNLIB UDF_INDEX **rpn_udf_id_ptr(void);
epicsShareFuncRPNLIB long *rpn_cycle_counter_ptr(void);
epicsShareFuncRPNLIB long *rpn_cycle_counter_stop_ptr(void);
epicsShareFuncRPNLIB long *rpn_max_cycle_counter_ptr(void);
#if defined(RPN_USE_TLS_ACCESSORS)
#define udf_id (*rpn_udf_id_ptr())
#define cycle_counter (*rpn_cycle_counter_ptr())
#define cycle_counter_stop (*rpn_cycle_counter_stop_ptr())
#define max_cycle_counter (*rpn_max_cycle_counter_ptr())
#else
extern RPN_THREAD_LOCAL UDF_INDEX *udf_id;
extern RPN_THREAD_LOCAL long cycle_counter;
extern RPN_THREAD_LOCAL long cycle_counter_stop;
extern RPN_THREAD_LOCAL long max_cycle_counter;
#endif

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
epicsShareFuncRPNLIB char **rpn_sstack_ptr(void);
#if defined(RPN_USE_TLS_ACCESSORS)
#define sstack (rpn_sstack_ptr())
#else
extern RPN_THREAD_LOCAL char *sstack[STACKSIZE];
#endif

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
epicsShareFuncRPNLIB struct CODE *rpn_code_root_ptr(void);
epicsShareFuncRPNLIB struct CODE **rpn_code_current_ptr(void);
epicsShareFuncRPNLIB long *rpn_code_lev_ptr(void);
#if defined(RPN_USE_TLS_ACCESSORS)
#define code (*rpn_code_root_ptr())
#define code_ptr (*rpn_code_current_ptr())
#define code_lev (*rpn_code_lev_ptr())
#else
extern RPN_THREAD_LOCAL struct CODE code;		/* root node */
extern RPN_THREAD_LOCAL struct CODE *code_ptr;           /* will point to current node */
extern RPN_THREAD_LOCAL long code_lev;                    /* number of links */
#endif

/* stack for logical operations */
#define LOGICSTACKSIZE 500
epicsShareFuncRPNLIB long *rpn_logicstack_ptr(void);
epicsShareFuncRPNLIB long *rpn_lstackptr_ptr(void);
#if defined(RPN_USE_TLS_ACCESSORS)
#define logicstack (rpn_logicstack_ptr())
#define lstackptr (*rpn_lstackptr_ptr())
#else
extern RPN_THREAD_LOCAL long logicstack[LOGICSTACKSIZE];
extern RPN_THREAD_LOCAL long lstackptr;
#endif


/* structure and stack for command input files */
#define FILESTACKSIZE 10

struct INPUT_FILE {
    FILE *fp;
    long filemode;
    } ;

/* input file modes */
#define ECHO 0
#define NO_ECHO 1

epicsShareFuncRPNLIB struct INPUT_FILE *rpn_input_stack_ptr(void);
epicsShareFuncRPNLIB long *rpn_istackptr_ptr(void);
#if defined(RPN_USE_TLS_ACCESSORS)
#define input_stack (rpn_input_stack_ptr())
#define istackptr (*rpn_istackptr_ptr())
#else
extern RPN_THREAD_LOCAL struct INPUT_FILE input_stack[FILESTACKSIZE];
extern RPN_THREAD_LOCAL long istackptr;
#endif

/* structure and array (not stack) for user IO files */
struct IO_FILE {
    FILE *fp;
    char *name;
    long mode;
#define INPUT 1
#define OUTPUT 2
    } ;

epicsShareFuncRPNLIB struct IO_FILE *rpn_io_file_ptr(void);
#if defined(RPN_USE_TLS_ACCESSORS)
#define io_file (rpn_io_file_ptr())
#else
extern RPN_THREAD_LOCAL struct IO_FILE io_file[FILESTACKSIZE];
#endif

/* values to indicate scientific notation or non-scientific notation output */
#define SCIENTIFIC 0
#define NO_SCIENTIFIC 1
#define USER_SPECIFIED 2
epicsShareFuncRPNLIB long *rpn_format_flag_ptr(void);
epicsShareFuncRPNLIB long *rpn_do_trace_ptr(void);
#if defined(RPN_USE_TLS_ACCESSORS)
#define format_flag (*rpn_format_flag_ptr())
#define do_trace (*rpn_do_trace_ptr())
#else
extern RPN_THREAD_LOCAL long format_flag;

/* flag to indicate trace/notrace mode */
extern RPN_THREAD_LOCAL long do_trace;
#endif

extern char *additional_help;

double rpn_internal(char *expression);
void rpn_update_legacy_stackptrs(void);

long find_udf_unlocked(char *udf_name);
long find_udf_mod_unlocked(char *udf_name);
long get_udf_unlocked(long number);
void get_udf_indexes_unlocked(long number);
long is_udf_unlocked(char *string);
void revudf_unlocked(void);
long cycle_through_udf_unlocked(void);
void conditional_udf_unlocked(long udf_current_step);
void gen_pcode_unlocked(char *s, long i);
void udf_createarray_unlocked(long type, long index, double data, char *ptr, long i_udf);
void udf_cond_createarray_unlocked(long colon, long i);
void udf_modarray_unlocked(long type, long index, double data, long i);
void udf_id_createarray_unlocked(long start_index_value, long end_index_value);
void udf_create_unknown_array_unlocked(char *ptr, long index);

#ifdef USE_GSL
#include "gsl/gsl_errno.h"
#endif
