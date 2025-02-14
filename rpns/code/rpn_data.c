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
 Revision 1.21  2010/02/24 23:50:41  borland
 Added InvFq, which returns q solving F(q)=2*(sqrt(q^2-1)-acos(1/q)).
 See Wiedemann, Particle Accelerator Physics I, 8.2.2

 Revision 1.20  2009/10/29 19:53:20  soliday
 Renamed a few global variables that conflicted with MADX.

 Revision 1.19  2009/03/09 21:27:26  shang
 added strlen function

 Revision 1.18  2005/01/13 16:45:43  shang
 renamed strcmp to streq and added strgt, strlt, strmatch

 Revision 1.17  2005/01/10 21:08:40  shang
 added strcmp and ssto

 Revision 1.16  2003/10/30 21:20:55  soliday
 Changed the function name "store" to store_in_mem to avoid a conflict with a
 system function.

 Revision 1.15  2003/09/29 16:01:16  soliday
 Renamed Kn and In to BesKn and BesIn.

 Revision 1.14  2003/07/09 19:17:38  soliday
 Fixed issues with the dstack and dstackptr

 Revision 1.13  2003/06/15 17:20:38  borland
 Added modified bessel functions Kn and In.

 Revision 1.12  2003/03/17 22:19:01  borland
 Fixed bugs in previous changes. Needs to be checked for memory leaks
 in new code.

 Revision 1.11  2003/03/17 20:10:40  borland
 Added mudf user-callable function to Make UDF from string stack.
 Added simpson user-callable function to do simpson's rule integration.
 Added rpn_internal() to allow evaluating rpn expressions from inside
 interpreter subroutines.

 Revision 1.10  2002/08/14 16:28:59  soliday
 Added Open License

 Revision 1.9  2000/06/15 15:52:51  soliday
 Renamed help to rpn_help so that it does not conflict with the IOC.

 Revision 1.8  2000/05/09 14:27:12  borland
 Added grndl (gaussian RN with limit) function.

 Revision 1.7  2000/05/08 02:38:38  borland
 Added srnd function for seeding random number generators.

 Revision 1.6  2000/03/27 20:29:03  borland
 Added rpn_execn() and rpn_execs() functions and user functions execn and
 execs.  These execute commands from the string stack.  Results are put
 on the numerical or string stack.

 Revision 1.5  1998/08/11 18:27:56  borland
 New version per R. Soliday with true pcoding of UDFs for 4-8 x faster
 evaluation.

 Revision 1.4  1996/10/22 18:47:28  borland
 Added built-in function for poisson statistics significance level.

 * Revision 1.3  1995/10/07  22:45:15  borland
 * Modified UDF code to use a sorted list plus an index.  Unfortunately, this
 * doesn't seem to have improved performance.
 *
 * Revision 1.2  1995/09/05  21:22:28  saunders
 * First test release of the SDDS1.5 package.
 *
*/
/* file   : rpn_data.c
 * purpose: contains allocations of space for global arrays
 * Michael Borland, 1988
 */
#include "rpn_internal.h"

/* first node for user-defined function linked-list */
struct UDF **udf_list = NULL;
long num_udfs = 0, max_udfs = 0;
long udf_changed = 0;

/* flag to indicate that a memory has just been added */
/*
struct MEMORY memory;
long n_memories;
long memory_added;
*/

/* stack for computations */
double stack[STACKSIZE];
long stackptr=0;

/*stack for long numbers */
long dstack[STACKSIZE];
long dstackptr=0;

/* stack for udf code */
UDF_CODE *udf_stack=NULL;
long udf_stackptr=0, max_udf_stackptr=0;

/* stack to replace recursion in execute.c */
UDF_INDEX *udf_id=NULL;
long cycle_counter=0, max_cycle_counter=0;
long cycle_counter_stop=0;

/* stack for udf condtional breakpoints */
UDF_CONDITIONAL *udf_cond_stack=NULL;
long udf_cond_stackptr=0, max_udf_cond_stackptr=0;

/* stack for unknown objects in udfs */
UDF_UNKNOWN *udf_unknown=NULL;
long udf_unknownptr=-1, max_udf_unknown_counter=0;


/* stack of pointers to arrays for array implementation */
RPN_ARRAY *astack=NULL;
long astackptr=0, max_astackptr=0;

/* stack for strings */
char *sstack[STACKSIZE];
long sstackptr=0;

/* stack for logical operations */
short logicstack[LOGICSTACKSIZE];
long lstackptr=0;

/* stack for command input files */
struct INPUT_FILE input_stack[FILESTACKSIZE];
long fstackptr=0;

/* array for regular input/output files (not a stack) */
struct IO_FILE output_file[FILESTACKSIZE];
struct IO_FILE input_file[FILESTACKSIZE];

struct IO_FILE io_file[FILESTACKSIZE];
long istackptr;

/* root structure for code sequences, pointer to current sequence  */
struct CODE code, *code_ptr;
long code_lev;

/* flag to indicate scientific notation or non-scientific notation output */
long format_flag = NO_SCIENTIFIC;

/* flag to indicate trace/notrace mode */
long do_trace;

/* array of function structures */
struct FUNCTION funcRPN[NFUNCS] = {
/* program control and evaluation functions: */
       { "?"   , "conditional (?expr:expr$)           ", conditional, OTHER_FUNC  },
       { "rs"  , "show runtime statistics             ", rep_stats  ,   OTHER_FUNC},
       { "udf" , "make user defined function interactively", make_udf   , OTHER_FUNC  },
       {"mudf" , "make user defined function from string stack (name commands mudf)", rpn_mudf, OTHER_FUNC },
       { "sudf", "show udfs                           ", revudf     , OTHER_FUNC  },
       { "stop", "stop (halt execution)               ", stop       , OTHER_FUNC  },
       { "quit", "quit                                ", quit       , OTHER_FUNC  },
       { "@"   , "push command input file             ", open_cominp, OTHER_FUNC  },
       { "help", "help                                ", rpn_help   , OTHER_FUNC  },
       { "ttr" , "toggle trace mode                   ", ttrace     , OTHER_FUNC  },
       {"sleep", "sleep for number of seconds         ", rpn_sleep  , OTHER_FUNC  },
/* mathematical operations: */
       { "+"   , "add                                 ", rpn_add    , NUMERIC_FUNC},
       { "-"   , "subtract                            ", rpn_subtract, NUMERIC_FUNC},
       { "*"   , "multiply                            ", rpn_multiply, NUMERIC_FUNC},
       { "/"   , "divide                              ", rpn_divide , NUMERIC_FUNC},
       { "fmod", "modulus                             ", rpn_mod, NUMERIC_FUNC},
       { "sqrt", "square-root                         ", rpn_sqrt   , NUMERIC_FUNC},
       { "sqr" , "square                              ", rpn_square , NUMERIC_FUNC},
       { "pow" , "power                               ", rpn_power  , NUMERIC_FUNC},
       { "floor", "floor                              ", rpn_floor  , NUMERIC_FUNC},
       { "ceil", "ceil                                ", rpn_ceil   , NUMERIC_FUNC},
       { "round", "round                              ", rpn_round  , NUMERIC_FUNC},
       { "sin" , "sine function                       ", rpn_sin       , NUMERIC_FUNC},
       { "cos" , "cosine function                     ", rpn_cos       , NUMERIC_FUNC},
       { "asin", "arc-sine function                   ", rpn_asin      , NUMERIC_FUNC},
       { "acos", "arc-cosine function                 ", rpn_acos      , NUMERIC_FUNC},
       { "atan", "arc-tangent function                ", rpn_atan      , NUMERIC_FUNC},
       {"atan2", "two-argument arc-tangent            ", rpn_atan2     , NUMERIC_FUNC},
       {"exp"  , "exponential (base e)                ", rpn_ex        , NUMERIC_FUNC},
       { "ln"  , "logarithm (base e)                  ", rpn_ln        , NUMERIC_FUNC},
       { "erf" , "error function                      ", rpn_erf       , NUMERIC_FUNC},
       { "erfc", "complementary error function        ", rpn_erfc      , NUMERIC_FUNC},
       { "Jn"  , "Bessel functions (1st kind)         ", rpn_JN     , NUMERIC_FUNC},
       { "Yn"  , "Bessel functions (2nd kind)         ", rpn_YN     , NUMERIC_FUNC},
       {"BesIn", "Modified Bessel functions (1st kind)", rpn_IN     , NUMERIC_FUNC},
       {"BesKn", "Modified Bessel functions (2nd kind)", rpn_KN     , NUMERIC_FUNC},
#ifdef USE_GSL_FRESNEL
       {"FresS", "fresnel sine integration function in gsl, note that the argument is (pi/2)t^2 instead of t^2", rpn_FresS  , NUMERIC_FUNC},
       {"FresC", "fresnel cosine integration function in gsl, note that the argument is (pi/2)t^2 instead of t^2 ", rpn_FresC  , NUMERIC_FUNC},
#endif
       {"G1y"  , "G1y (Synch. Rad. Function)"          , rpn_G1y     , NUMERIC_FUNC},
       { "cei1", "1st complete elliptic integral      ", rpn_cei1   , NUMERIC_FUNC},
       { "cei2", "2nd complete elliptic integral      ", rpn_cei2   , NUMERIC_FUNC},
       {"lngam", "ln of gamma function                ", rpn_lngam  , NUMERIC_FUNC},
       {"betai", "incomplete beta fn (x a b betai)    ", rpn_betai  , NUMERIC_FUNC},
       {"gamP",  "incomplete gamma P (x a gamP)       ", rpn_gammaP , NUMERIC_FUNC},
       {"gamQ",  "incomplete gamma Q (x a gamQ)       ", rpn_gammaQ , NUMERIC_FUNC},
       { "int" , "take integer part                   ", rpn_int    , NUMERIC_FUNC},
       { "rnd" , "uniform random number 0->1          ", rpn_rnd    , NUMERIC_FUNC},
       {"grnd" , "norm. dist. random number           ", rpn_grnd   , NUMERIC_FUNC},
       {"grndl" , "norm. dist. random number w/limit  ", rpn_grndlim, NUMERIC_FUNC},
       {"srnd" , "set random number seed              ", rpn_srnd   , NUMERIC_FUNC},
       {"sumn" , "sum of N items from stack           ", rpn_sumn   , NUMERIC_FUNC},
       {"nan"  , "push NaN onto stack                 ", rpn_push_nan  , NUMERIC_FUNC},
       { "bit&", "bitwise and                         ", rpn_bitand , NUMERIC_FUNC},
       { "bit|", "bitwise or                          ", rpn_bitor  , NUMERIC_FUNC},
       { ">"   , "greater than                        ", greater    , LOGICAL_FUNC},
       { "<"   , "less than                           ", less       , LOGICAL_FUNC},
       { "=="  , "equal to                            ", equal      , LOGICAL_FUNC},
       { "&&"  , "logical and                         ", log_and    , LOGICAL_FUNC},
       { "||"  , "logical or                          ", log_or     , LOGICAL_FUNC},
       { "!"   , "logical not                         ", log_not    , LOGICAL_FUNC},
       {"isnan", "test for equality to NaN            ", rpn_isnan  , LOGICAL_FUNC},
       {"isinf", "test for equality to Inf            ", rpn_isinf  , LOGICAL_FUNC},
       {"poiSL", "Poisson dist. sig. level (x mu poiSL)", rpn_poissonSL, NUMERIC_FUNC},
       {"simpson", "Simpsons-rule integration (\"udfName\" low high pts simpson)", rpn_simpson, NUMERIC_FUNC},
       {"InvFq",  "Inverse of F(q)=2*(sqrt(q^2-1)-acos(1/q))", rpn_inverseFq, NUMERIC_FUNC},
       {"qlfun",  "sum of series for quantum lifetime", rpn_quantumLifetimeSum, NUMERIC_FUNC},
/* stack operations: */
       { "view", "view numeric stack                  ", view       , OTHER_FUNC  },
       { "tsci", "toggle scientific notation          ", tsci       , NUMERIC_FUNC},
       { "swap", "swap top two items on stack         ", swap       , NUMERIC_FUNC},
       { "pop" , "remove top item from stack          ", pop        , NUMERIC_FUNC},
       { "cle" , "clear entire stack                  ", rpn_clear      , OTHER_FUNC  },
       { "="   , "duplicate top item on stack         ", duplicate  , OTHER_FUNC  },
       { "n="  , "duplicate top n items on stack      ", nduplicate , OTHER_FUNC  },
       { "rdn" , "rotate stack downward               ", rdn        , NUMERIC_FUNC},
       { "rup" , "rotate stack upward                 ", rup        , NUMERIC_FUNC},
       { "stlv", "push stack level onto stack         ", stack_lev  , NUMERIC_FUNC},
       {"isort", "sort the top n items on the stack in increasing order", rpn_isort_stack, NUMERIC_FUNC},
       {"dsort", "sort the top n items on the stack in decreasing order", rpn_dsort_stack, NUMERIC_FUNC},
       { "vlog", "view logical stack                  ", viewlog    , LOGICAL_FUNC},
       { "popl", "pop logical stack                   ", poplog     , LOGICAL_FUNC},
       { "pops", "pop string stack                    ", pops       , OTHER_FUNC  },
       { "vstr", "view string stack                   ", view_str   , OTHER_FUNC  },
       { "=str", "duplicate top of string stack       ", dup_str    , OTHER_FUNC  },
       { "xstr", "execute top of string stack         ", exe_str    , OTHER_FUNC  },
/* memory operations: */
       { "sto" , "store in memory (store name)        ", store_in_mem , OTHER_FUNC  },
       { "ssto" , "store string in memory (store name) ", store_in_str_mem , OTHER_FUNC  },
       { "streq" , "compare if two strings are the same ", rpn_streq  , LOGICAL_FUNC  },
       { "strgt" , "compare if left string is greater than right string ", rpn_strgt  , LOGICAL_FUNC  },
       { "strlt" , "compare if left string is less than right string ", rpn_strlt  , LOGICAL_FUNC  },
       { "strmatch","compare if left string matches right string pattern",rpn_strmatch, LOGICAL_FUNC  },
       { "strlen" , "return the length of string" , rpn_strlen, NUMERIC_FUNC},
       { "smem", "show contents of memories           ", revmem     , OTHER_FUNC  },
/* array operations: */
       { "mal" , "allocate memory                     ", rpn_alloc     , OTHER_FUNC  },
       { "[" , "recall by reference                 ", rref       , NUMERIC_FUNC},
       { "]" , "store by reference                  ", sref       , OTHER_FUNC  },
/* input/output: */
       { "open", "open input/output file              ", open_io    , OTHER_FUNC  },
       { "clos", "close input/output file             ", close_io   , OTHER_FUNC  },
       { "fprf", "fprintf                             ", fprf       , OTHER_FUNC  },
       { "sprf", "sprintf                             ", sprf       , OTHER_FUNC  },
       { "puts", "put string to file                  ", rpn_puts      , OTHER_FUNC  },
       { "gets", "get string from input file          ", rpn_gets      , OTHER_FUNC  },
       { "scan", "scan number from string             ", scan       , NUMERIC_FUNC},
      {"format", "specify output format               ", format     , OTHER_FUNC},
   {"getformat", "push output format onto string stack", get_format     , OTHER_FUNC},
         {"csh", "start and enter C shell subprocess  ", rpn_csh    , OTHER_FUNC},
        {"cshs", "send top of string stack to C shell", rpn_csh_str, OTHER_FUNC},
        {"execs", "send top of string stack to UNIX and put output on string stack", rpn_execs, OTHER_FUNC},
        {"execn", "send top of string stack to UNIX and put result on numerical stack", rpn_execn, NUMERIC_FUNC},
#ifdef USE_GSL
       {"LambertW0",  "Lambert W_0       ", rpn_Lambert_W0 , NUMERIC_FUNC},
       {"LambertWm1",  "Lambert W_-1       ", rpn_Lambert_Wm1 , NUMERIC_FUNC},
#endif
    };


char *additional_help="\n\
To push an item onto the string stack, place it in double quotes.\n\
For example, to write a number in exponential format to the file\n\
open on unit 3, one would enter 'number 3 \"%le\\n\" fprf'.\n";
