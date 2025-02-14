/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "SDDStypes.h"
#if defined(_WIN32) && !defined(_MINGW)
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
#define PRId32 "ld"
#define SCNd32 "ld"
#define PRIu32 "lu"
#define SCNu32 "lu"
#if !defined(INT32_MAX)
#define INT32_MAX 2147483647i32
#endif
#else
#if defined(vxWorks)
#define PRId32 "ld"
#define SCNd32 "ld"
#define PRIu32 "lu"
#define SCNu32 "lu"
#define INT32_MAX (2147483647)
#else
#include <inttypes.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


#undef epicsShareFuncRPNLIB
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_RPNLIB)
#define epicsShareFuncRPNLIB  __declspec(dllexport)
#define epicsShareExtern extern __declspec(dllexport)
#else
#define epicsShareFuncRPNLIB
#if !defined(EXPORT_SDDS)
#define epicsShareExtern extern __declspec(dllimport)
#endif
#endif
#else
#undef epicsShareExtern
#define epicsShareFuncRPNLIB
#define epicsShareExtern extern
#endif

/* function call for programs that use rpn: */
epicsShareFuncRPNLIB double rpn(char *expression);

/* prototypes for code in file array.c */
void rpn_alloc(void);
void rref(void);
void sref(void);
epicsShareFuncRPNLIB long rpn_createarray(long size);
epicsShareFuncRPNLIB double *rpn_getarraypointer(long memory_number, int32_t *length);
epicsShareFuncRPNLIB long rpn_resizearray(long arraynum, long size);
void udf_createarray(short type, short index, double data, char *rpn, long i_udf);
void udf_cond_createarray(long colon, long i);
void udf_modarray(short type, short index, double data, long i);
void udf_id_createarray(long start_index_value, long end_index_value);
void udf_create_unknown_array(char *ptr, long index);



/* prototypes for code in file conditional.c */
void conditional(void);
long dissect_conditional(char **branch, long is_true);
void conditional_udf(long udf_current_step);

/* prototypes for code in file execute.c */
epicsShareFuncRPNLIB long execute_code(void);
void set_ptrs(char **text, char **buffer, char **token);
long is_func(char *string);
void quit(void);
void rpn_help(void);
long stack_test(long stackptr, long numneeded, char *stackname, char *caller);
void stop(void);
void ttrace(void);
void rep_stats(void);
void rpn_sleep(void);
long cycle_through_udf(void);

/* long func_compare(struct FUNCTION *f1, struct FUNCTION *f2); */
epicsShareFuncRPNLIB int func_compare(const void *f1v, const void *f2v);

/* prototypes for code in file get_token_rpn.c */
epicsShareFuncRPNLIB char *get_token_rpn(char *s, char *buf, long lbuf, long *spos);

/* prototypes for code in file logical.c */
void greater(void);
void less(void);
void greater_equal(void);
void less_equal(void);
void equal(void);
void not_equal(void);
epicsShareFuncRPNLIB void log_and(void);
epicsShareFuncRPNLIB void log_or(void);
void log_not(void);
void poplog(void);
void lton(void);
void ntol(void);

/* prototypes for code in file math.c */
void rpn_strlen(void);
void rpn_streq(void);
void rpn_strgt(void);
void rpn_strlt(void);
void rpn_strmatch(void);
void rpn_add(void);
void rpn_sumn(void);
void rpn_subtract(void);
void rpn_multiply(void);
void rpn_divide(void);
void rpn_mod(void);
void rpn_sqrt(void);
void rpn_floor(void);
void rpn_ceil(void);
void rpn_round(void);
void rpn_inverseFq(void);
void rpn_square(void);
void rpn_power(void);
void rpn_sin(void);
void rpn_cos(void);
void rpn_atan(void);
void rpn_asin(void);
void rpn_acos(void);
void rpn_ex(void);
void rpn_ln(void);
void rpn_erf(void);
void rpn_erfc(void);
void rpn_JN(void);
void rpn_YN(void);
void rpn_IN(void);
void rpn_KN(void);
void rpn_FresS(void);
void rpn_FresC(void);
void rpn_G1y(void);
void rpn_int(void);
void rpn_cei1(void);
void rpn_cei2(void);
void rpn_lngam(void);
void rpn_betai(void), rpn_gammaP(void), rpn_gammaQ(void);
void rpn_rnd(void);
void rpn_grnd(void);
void rpn_grndlim(void);
void rpn_srnd(void);
void rpn_atan2(void);
void rpn_push_nan(void);
void rpn_isinf(void);
void rpn_isnan(void);
void rpn_poissonSL(void);
void rpn_simpson(void);
void rpn_isort_stack(void);
void rpn_dsort_stack(void);
void rpn_Lambert_W0(void);
void rpn_Lambert_Wm1(void);
void rpn_quantumLifetimeSum(void);
void rpn_bitand(void);
void rpn_bitor(void);
  
/* prototypes for code in file memory.c */
epicsShareFuncRPNLIB long rpn_create_mem(char *name, short is_string);
epicsShareFuncRPNLIB long rpn_store(double value, char *str_value, long memory_number);
epicsShareFuncRPNLIB long rpn_quick_store(double value, char *str_value, long memory_number);
epicsShareFuncRPNLIB double rpn_recall(long memory_number);
epicsShareFuncRPNLIB char *rpn_str_recall(long memory_number);
void store_in_mem(void);
void store_in_str_mem(void);
epicsShareFuncRPNLIB long is_memory(double *val, char **str_value, short *is_string, char *string);
void revmem(void);

/* prototypes for code in file pcode.c */
void gen_pcode(char *s, long i);

/* prototypes for code in file pop_push.c */
double pop_num(void);
long pop_long(void);
epicsShareFuncRPNLIB long push_num(double num);
epicsShareFuncRPNLIB long push_long(long num);
epicsShareFuncRPNLIB long pop_log(int32_t *logical);
epicsShareFuncRPNLIB long push_log(long logical);
long pop_file(void);
long push_file(char *filename);
epicsShareFuncRPNLIB char *pop_string(void);
epicsShareFuncRPNLIB void push_string(char *s);
void pop_code(void);
void push_code(char *code, long mode);

/* prototypes for code in file prompt.c */
epicsShareFuncRPNLIB long prompt(char *prompt_s, long do_prompt);

/* prototypes for code in file rpn_io.c */
void open_cominp(void);
void open_io(void);
void close_io(void);
void rpn_gets(void);
void scan(void);
void format(void);
void get_format(void);
epicsShareFuncRPNLIB char *choose_format(long flag, double x);
void view(void);
void view_top(void);
void tsci(void);
void viewlog(void);
void fprf(void);
void view_str(void);
void rpn_puts(void);
void sprf(void);

/* prototypes for code in file stack.c */
void swap(void);
void duplicate(void);
void nduplicate(void);
void stack_lev(void);
void pop(void);
epicsShareFuncRPNLIB void rpn_clear(void);
void pops(void);
void rup(void);
void rdn(void);
void dup_str(void);
void exe_str(void);

/* prototypes for code in file udf.c */
long find_udf(char *udf_name);
long find_udf_mod(char *udf_name);
short get_udf(long number);
void get_udf_indexes(long number);
void make_udf(void);
void rpn_mudf(void);
epicsShareFuncRPNLIB void create_udf(char *name, char *function);
epicsShareFuncRPNLIB void link_udfs(void);
void insert_udf(char *instr, char *udf_string);
void revudf(void);

/* prototypes for code in file rpn_csh.c */
void rpn_csh(void);
void rpn_csh_str(void);
void rpn_execs(void);
void rpn_execn(void);

/* prototypes for code in file rpn_draw.c */
void rpn_draw(void);

/* prototypes for code in file rpn_error.c */
void rpn_set_error(void);
epicsShareFuncRPNLIB long rpn_check_error(void);
epicsShareFuncRPNLIB void rpn_clear_error(void);

/* prototypes for code in file infixtopostfix.c */
#define IFPF_BUF_SIZE    1024
epicsShareFuncRPNLIB int if2pf(char *pfix, char *ifix, size_t size_of_pfix);

#define STACKSIZE 5000
epicsShareExtern long dstackptr;
epicsShareExtern long sstackptr;

#ifdef __cplusplus
}
#endif
