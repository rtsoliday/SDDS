/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 $Log: not supported by cvs2svn $
 Revision 1.23  2009/10/29 19:53:20  soliday
 Renamed a few global variables that conflicted with MADX.

 Revision 1.22  2009/03/09 21:27:26  shang
 added strlen function

 Revision 1.21  2005/01/13 16:45:12  shang
 changed NFUNCS to 93 since 3 more string related functions were added.

 Revision 1.20  2005/01/10 21:01:47  shang
 added str_memoryData variable and set NFUNCS from 88 to 90

 Revision 1.19  2003/10/03 14:55:59  soliday
 Moved some statements from rpn_interal.h to rpn.h

 Revision 1.18  2003/08/18 20:26:05  soliday
 Fixed issue with OSX

 Revision 1.17  2003/06/19 18:21:35  shang
 replace double by long for long type stack

 Revision 1.16  2003/06/19 18:19:33  shang
 added long type stack

 Revision 1.15  2003/06/15 17:20:38  borland
 Added modified bessel functions Kn and In.

 Revision 1.14  2003/03/17 22:19:02  borland
 Fixed bugs in previous changes. Needs to be checked for memory leaks
 in new code.

 Revision 1.13  2003/03/17 20:10:41  borland
 Added mudf user-callable function to Make UDF from string stack.
 Added simpson user-callable function to do simpson's rule integration.
 Added rpn_internal() to allow evaluating rpn expressions from inside
 interpreter subroutines.

 Revision 1.12  2002/08/14 16:28:59  soliday
 Added Open License

 Revision 1.11  2000/05/09 14:27:12  borland
 Added grndl (gaussian RN with limit) function.

 Revision 1.10  2000/05/08 02:38:38  borland
 Added srnd function for seeding random number generators.

 Revision 1.9  2000/04/13 16:23:05  soliday
 WIN32 can now properly import variables from the DLL

 Revision 1.8  2000/04/06 22:22:01  soliday
 Added support for Borland C.

 Revision 1.7  2000/03/27 20:29:03  borland
 Added rpn_execn() and rpn_execs() functions and user functions execn and
 execs.  These execute commands from the string stack.  Results are put
 on the numerical or string stack.

 Revision 1.6  1999/09/14 18:22:49  soliday
 Added export calls for WIN32 dll files

 Revision 1.5  1998/08/11 18:27:57  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 Revision 1.4  1996/10/22 18:47:28  borland
 Added built-in function for poisson statistics significance level.

 * Revision 1.3  1995/10/07  22:45:16  borland
 * Modified UDF code to use a sorted list plus an index.  Unfortunately, this
 * doesn't seem to have improved performance.
 *
 * Revision 1.2  1995/09/05  21:22:35  saunders
 * First test release of the SDDS1.5 package.
 *
*/
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
