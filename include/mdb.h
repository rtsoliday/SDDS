/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file   : mdb.h
 * purpose: definitions for general use, for mdblib, and for mdbmth.
 *
 * Michael Borland, 1988
 */
#ifndef _MDB_
#define _MDB_ 1

#include <math.h>
#include <float.h>
#include <limits.h>
#include <time.h>
#include <stdint.h>
#if defined(_WIN32) && !defined(_MINGW)
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#ifndef PRId32
#define PRId32 "ld"
#define SCNd32 "ld"
#define PRIu32 "lu"
#define SCNu32 "lu"
#define PRId64 "I64d"
#define SCNd64 "I64d"
#define PRIu64 "I64u"
#define SCNu64 "I64u"
#define PRIx64 "I64x"
#endif

#if !defined(INT32_MAX)
#define INT32_MAX 2147483647i32
#endif
#if !defined(INT32_MIN)
#define INT32_MIN (-2147483647i32 - 1)
#endif
#if !defined(INT64_MAX)
#define INT64_MAX 9223372036854775807i64
#endif
#else
#if defined(vxWorks)
#define PRId32 "ld"
#define SCNd32 "ld"
#define PRIu32 "lu"
#define SCNu32 "lu"
#define PRId64 "lld"
#define SCNd64 "lld"
#define PRIu64 "llu"
#define SCNu64 "llu"
#define INT32_MAX (2147483647)
#define INT32_MAX (-2147483647 - 1)
#else
#include <inttypes.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SUNOS4)
#define SUN_SPARC 1
#endif

#if defined(linux)
#define LINUX 1
#endif

  /*
#if !(defined(IEEE_MATH) && (defined(SUNOS4) || defined(SOLARIS) || defined(LINUX)))
#define isinf(x) (0)
#endif
  */

#if defined(SOLARIS)
#include <ieeefp.h>
#if defined(__GNUC__)
#define isinf(x) ((x==x) && !finite(x))
#else
#if (SOLARIS < 10)
#define isinf(x) ((x==x) && !finite(x))
#endif
#endif
#endif
#if defined(_WIN32) && !defined(_MINGW) && (_MSC_VER < 1800)
#define isnan(x) _isnan(x)
#define isinf(x) (0)
#endif
#if defined(vxWorks)
#include <private/mathP.h>
#define isinf(x) isInf(x)
#define isnan(x) isNan(x)
#endif

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define FOPEN_WRITE_MODE "wb"
#define FOPEN_READ_MODE  "rb"
#define FOPEN_READ_AND_WRITE_MODE "r+b"

#undef epicsShareFuncMDBLIB
#undef epicsShareFuncMDBMTH
#undef epicsShareFuncMDBCOMMON
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_MDBLIB)
#define epicsShareFuncMDBLIB  __declspec(dllexport)
#else
#define epicsShareFuncMDBLIB
#endif
#if defined(EXPORT_MDBMTH)
#define epicsShareFuncMDBMTH  __declspec(dllexport)
#else
#define epicsShareFuncMDBMTH
#endif
#if defined(EXPORT_MDBCOMMON)
#define epicsShareFuncMDBCOMMON  __declspec(dllexport)
#else
#define epicsShareFuncMDBCOMMON
#endif
#else
#define epicsShareFuncMDBLIB
#define epicsShareFuncMDBMTH
#define epicsShareFuncMDBCOMMON
#endif
  
#if defined(SUNOS4) && defined(GNU_C)
/* prototypes for functions not defined in stdio: */
extern int printf(const char *format_spec, ...);
extern int fprintf(FILE *file_ptr, const char *format_spec, ...);
/* int sprintf(char *str, const char *format_spec, ...); */
extern int scanf(const char *format_spec, ...);
extern int fscanf(FILE *file_ptr, const char *format_spec, ...);
extern int sscanf(char *str, const char *format_spec, ...);
extern int fputs(const char *string, FILE *file_ptr);
extern int puts(const char *string);
extern int fputc(char c, FILE *file_ptr);
extern int fclose(FILE *file_ptr);
extern int close(int file_descriptor);
extern void perror(char *s);
extern int fseek(FILE *file_ptr, long offset, int direction);
extern int fread(void *data, int size, int number, FILE *fp);
extern int fwrite(void *data, int size, int number, FILE *fp);
extern int fflush(FILE *file_ptr);
/* prototypes for functions not fully prototyped in math.h: */
extern double   acos(double x);
extern double   asin(double x);
extern double   atan(double x);
extern double   atan2(double y, double x);
extern double   ceil(double x);
extern double   cos(double x);
extern double   cosh(double x);
extern double   exp(double x);
extern double   fabs(double x);
extern double   floor(double x);
extern double   fmod(double x, double y);
extern double   frexp(double value, int *expo);
extern double   ldexp(double value, int expo);
extern double   log(double x);
extern double   log10(double x);
extern double   modf(double value, double *iptr);
extern double   pow(double x, double y);
extern double   sin(double x);
extern double   sinh(double x);
extern double   sqrt(double x);
extern double   tan(double x);
extern double   tanh(double x);
#endif
#ifndef __cplusplus
epicsShareFuncMDBLIB extern void bombVA(char *template, ...);
#endif

/* double-precision Bessel functions */
#define EPS 1.0e-16
#define FPMIN 1.0e-30
#define MAXIT 10000
#define XMIN 2.0
#define PI 3.141592653589793
epicsShareFuncMDBMTH double dbesi0(double x);
epicsShareFuncMDBMTH double dbesi1(double x);
epicsShareFuncMDBMTH double dbesj0(double x);
epicsShareFuncMDBMTH double dbesj1(double x);
epicsShareFuncMDBMTH double dbesk0(double x);
epicsShareFuncMDBMTH double dbesk1(double x);
epicsShareFuncMDBMTH double dbesy0(double x);
epicsShareFuncMDBMTH double dbesy1(double x);
/*modified bessel function dbeskv */
epicsShareFuncMDBMTH double chebev(double a, double b, double c[], int m, double x);
epicsShareFuncMDBMTH void beschb(double x, double *gam1, double *gam2, double *gampl, double *gammi);


#include <stdlib.h>

epicsShareFuncMDBLIB long PackSuffixType(char *filename, char **unpackedName, unsigned long mode);
epicsShareFuncMDBLIB void *array_1d(uint64_t size_of_elem, uint64_t lower_index, uint64_t upper_index);
epicsShareFuncMDBLIB void **array_2d(uint64_t size_of_elem, uint64_t lower1, uint64_t upper1, uint64_t lower2,
                uint64_t upper2);
epicsShareFuncMDBLIB int free_array_1d(void *array, uint64_t size_of_elem, uint64_t lower_index,
                uint64_t upper_index);
epicsShareFuncMDBLIB int free_array_2d(void **array, uint64_t size_of_elem, uint64_t lower1, uint64_t upper1,
                uint64_t lower2, uint64_t upper2);
epicsShareFuncMDBLIB void **zarray_2d(uint64_t size, uint64_t n1, uint64_t n2);
epicsShareFuncMDBLIB void **resize_zarray_2d(uint64_t size, uint64_t old_n1, uint64_t old_n2,
            void **array, uint64_t n1, uint64_t n2);
epicsShareFuncMDBLIB int free_zarray_2d(void **array, uint64_t n1, uint64_t n2);
epicsShareFuncMDBLIB void **czarray_2d(const uint64_t size, const uint64_t n1, const uint64_t n2);
epicsShareFuncMDBLIB int free_czarray_2d(void **array, uint64_t n1, uint64_t n2);
epicsShareFuncMDBLIB void **resize_czarray_2d(void **data, uint64_t size, uint64_t n1, uint64_t n2);
epicsShareFuncMDBLIB void zero_memory(void *memory, uint64_t n_bytes);
epicsShareFuncMDBLIB int tfree(void *ptr);
epicsShareFuncMDBLIB void keep_alloc_record(char *filename);

epicsShareFuncMDBLIB void fill_int_array(int *array, long n, int value);
epicsShareFuncMDBLIB void fill_short_array(short *array, long n, short value);
epicsShareFuncMDBLIB void fill_long_array(long *array, long n, long value);
epicsShareFuncMDBLIB void fill_float_array(float *array, long n, float value);
epicsShareFuncMDBLIB void fill_double_array(double *array, long n, double value);

epicsShareFuncMDBLIB void *tmalloc(uint64_t size_of_block);
epicsShareFuncMDBLIB void *trealloc(void *ptr, uint64_t size_of_block);

/* String-related macro definitions: */
#define chop_nl(m_s) ( ((m_s)[strlen(m_s)-1]=='\n') ? (m_s)[strlen(m_s)-1]=0 : 0)

#define queryn(s, t, n) (queryn_func((s), (t), (n)))
#define queryn_e(s, t, n) (queryn_e_func((s), (t), (n)))

#define is_yes(c) ((c)=='y' || (c)=='Y')
#define is_no(c) ((c)=='n' || (c)=='N')

static inline void queryn_func(const char *s, char *t, int n) {
  *t = 0;
  fputs(s, stdout);
  if (fgets(t, n, stdin))
    chop_nl(t);
}

static inline void queryn_e_func(const char *s, char *t, int n) {
  *t = 0;
  fputs(s, stderr);
  if (fgets(t, n, stdin))
    chop_nl(t);
}

/*   -- Data-scanning routines: */
epicsShareFuncMDBLIB extern long   query_long(char *prompt, long default_value);
epicsShareFuncMDBLIB  extern int    query_int(char *prompt, int default_value);
epicsShareFuncMDBLIB  extern short  query_short(char *prompt, short default_value);
epicsShareFuncMDBLIB extern double query_double(char *prompt, double default_value);
epicsShareFuncMDBLIB extern float  query_float(char *prompt, float default_value);
epicsShareFuncMDBLIB extern int   get_double(double *target, char *source);
epicsShareFuncMDBLIB extern int   get_longdouble(long double *target, char *source);
epicsShareFuncMDBLIB extern int   get_double1(double *target, char *source);
epicsShareFuncMDBLIB extern int   get_long(long *target, char *source);
epicsShareFuncMDBLIB extern int   get_long1(long *iptr, char *s);
epicsShareFuncMDBLIB extern int   get_short(short *target, char *source);
epicsShareFuncMDBLIB extern int   get_int(int *target, char *source);
epicsShareFuncMDBLIB extern int   get_float(float *target, char *source);
epicsShareFuncMDBLIB extern char  *get_token(char *source);
epicsShareFuncMDBLIB extern char  *get_token_buf(char *source, char *buffer, long buffer_length);
epicsShareFuncMDBLIB extern char  *get_token_t(char *source, char *token_delimiters);
epicsShareFuncMDBLIB extern char  *get_token_tq(char *source, char *token_start,
                           char *token_end, char *quote_start, char *quote_end);
epicsShareFuncMDBLIB long tokenIsInteger(char *token);
epicsShareFuncMDBLIB long tokenIsNumber(char *token);

/*   -- String routines: */
epicsShareFuncMDBLIB extern char *trim_spaces(char *s);
epicsShareFuncMDBLIB  extern char *replace_chars(char *string, char *from, char *to);
epicsShareFuncMDBLIB  extern char *rcdelete(char *string, char c_lower, char c_upper);
epicsShareFuncMDBLIB extern char *compressString(char *string, char *chars_to_compress);
epicsShareFuncMDBLIB extern char *delete_chars(char *s, char *t);
epicsShareFuncMDBLIB extern char *delete_bounding(char *string, char *chars_to_delete);
epicsShareFuncMDBLIB extern char *str_toupper(char *string);
epicsShareFuncMDBLIB extern char *str_tolower(char *string);
epicsShareFuncMDBLIB extern long is_blank(char *string);
epicsShareFuncMDBLIB extern char *str_in(char *string, char *sub_string);
epicsShareFuncMDBLIB extern char *strcpy_ss(char *dest, const char *src);

epicsShareFuncMDBLIB  extern char *str_inn(char *string, char *sub_string, long n_char_to_check);
epicsShareFuncMDBLIB  extern char *insert(char *place_to_insert, char *string_to_insert);
epicsShareFuncMDBLIB extern char *pad_with_spaces(char *s, int n_spaces);
epicsShareFuncMDBLIB extern char *cp_str(char **target, char *source);
epicsShareFuncMDBLIB  extern char *cpn_str(char **target, char *source, long n_characters);
epicsShareFuncMDBLIB extern long edit_string(char *text, char *edit);
epicsShareFuncMDBLIB extern void edit_strings(char **string, long strings, char *buffer, char *edit);
epicsShareFuncMDBLIB char *strslide(char *s, long distance);

/* ---search path routines-- */
epicsShareFuncMDBLIB extern void setSearchPath(char *input);
epicsShareFuncMDBLIB extern char *findFileInSearchPath(const char *filename);

/* --file stat routines-- */
#include <sys/types.h>
#include <sys/stat.h>
epicsShareFuncMDBLIB extern char *dir_name (const char *path);
epicsShareFuncMDBLIB extern char *read_file_link(const char *filename);
epicsShareFuncMDBLIB extern const char *read_file_lastlink(const char *filename);
epicsShareFuncMDBLIB extern char *read_last_link_to_file(const char *filename);

epicsShareFuncMDBLIB extern long get_file_stat(const char *filename, const char *lastlink, struct stat *filestat);
epicsShareFuncMDBLIB extern long file_is_modified(const char *inputfile, char **final_file, struct stat *input_stat);

/* -- find files routines ---*/
#include <ctype.h>
#if !defined(_WIN32)
#include <dirent.h>
#endif
epicsShareFuncMDBCOMMON extern short make_four_digit_year (short year);
/*epicsShareFuncMDBCOMMON extern long is_leap_year (short year);*/
epicsShareFuncMDBCOMMON extern char **find_files_between_dates(char *directory, char *rootname, char *suffix, 
                            short startYear, short startMonth, short startDay, short startJDay,
                            short endYear, short endMonth, short endDay, short endJDay,
                            char *filter, char **extensionList, long extensions,
                            long tailsOnly, long *files, long increaseOrder);
void sort_files_by_start_time(char *directory, long isTail, char **fileList, long files, long increaseOrder);

/*epicsShareFuncMDBCOMMON extern char **ls_dir (char *path, char *matchstr, long tailsOnly, long *files);*/

#if !defined(__BORLANDC__) || defined(DefineBinaryInsert)
epicsShareFuncMDBLIB long binaryInsert(void **array, long members, void *newMember, 
             int (*compare)(const void *c1, const void *c2), int32_t *duplicate);
#endif
epicsShareFuncMDBLIB long binaryIndexSearch(void **array, long members, void *key, 
                       int (*compare)(const void *c1, const void *c2), long bracket);
epicsShareFuncMDBLIB long binaryArraySearch(void *array, size_t elemSize, long members, void *key, 
                                            int (*compare)(const void *c1, const void *c2), long bracket);

/* sort routines (previously sort.h) */
#if !defined(_MDBSORT_INCLUDED_)
#define _MDBSORT_INCLUDED_ 1 

/*following structs and function are moved from sddsxref.c for quick sorting. */
typedef struct {
  char *stringKey;
  double doubleKey;
  long rowIndex;
} KEYED_INDEX;

typedef struct {
  KEYED_INDEX **equivalent;
  long equivalents, nextIndex;
} KEYED_EQUIVALENT;

epicsShareFuncMDBLIB extern int CompareStringKeyedIndex(const void *ki1, const void *ki2);
epicsShareFuncMDBLIB extern int CompareDoubleKeyedIndex(const void *ki1, const void *ki2);
epicsShareFuncMDBLIB extern int CompareStringKeyedGroup(const void *kg1, const void *kg2);
epicsShareFuncMDBLIB extern int CompareDoubleKeyedGroup(const void *kg1, const void *kg2);
epicsShareFuncMDBLIB extern KEYED_EQUIVALENT **MakeSortedKeyGroups(long *keyGroups, long keyType, void *data, long points);
epicsShareFuncMDBLIB extern long FindMatchingKeyGroup(KEYED_EQUIVALENT **keyGroup, long keyGroups, long keyType,void *searchKeyData, long reuse);
epicsShareFuncMDBLIB extern long *sort_and_return_index(void *data, long type, long rows, long increaseOrder);

/* sort routines (previously sort.h) */
epicsShareFuncMDBLIB extern int double_cmpasc(const void *a, const void *b);
epicsShareFuncMDBLIB extern int double_abs_cmpasc(const void *a, const void *b);
epicsShareFuncMDBLIB extern int double_cmpdes(const void *a, const void *b);
epicsShareFuncMDBLIB extern int double_abs_cmpdes(const void *a, const void *b);
extern void double_copy(void *a, void *b);
extern int float_cmpasc(const void *a, const void *b);
extern int float_abs_cmpasc(const void *a, const void *b);
extern int float_cmpdes(const void *a, const void *b);
extern int float_abs_cmpdes(const void *a, const void *b);
extern void float_copy(void *a, void *b);
epicsShareFuncMDBLIB extern int long_cmpasc(const void *a, const void *b);
epicsShareFuncMDBLIB extern int long_abs_cmpasc(const void *a, const void *b);
extern int long_cmpdes(const void *a, const void *b);
extern int long_abs_cmpdes(const void *a, const void *b);
extern void long_copy(void *a, void *b);
epicsShareFuncMDBLIB extern int string_cmpasc(const void *a, const void *b);
extern int string_cmpdes(const void *a, const void *b);
epicsShareFuncMDBLIB extern void string_copy(void *a, void *b);
epicsShareFuncMDBLIB extern int row_compare(const void *a, const void *b);
extern void row_copy(void *a, void *b);
epicsShareFuncMDBLIB extern void set_up_row_sort(int sort_by_column, size_t n_columns,
    size_t element_size,
    int (*compare)(const void *a, const void *b));
epicsShareFuncMDBLIB extern int unique(void *base, size_t n_items, size_t size,
    int (*compare)(const void *a, const void *b),
    void (*copy)(void *a, void *b));
#endif

/* string array matching (previously match_string.h): */
#if !defined(_MATCH_STRING_)
epicsShareFuncMDBLIB extern long match_string(char *string, char **option_list, long n_options,
                        long match_mode_flags);

epicsShareFuncMDBLIB extern int strncmp_case_insensitive(char *s1, char *s2, long n);
epicsShareFuncMDBLIB extern int strcmp_case_insensitive(char *s1, char *s2);
#if defined(_WIN32)
#if defined(__BORLANDC__)
#define strcasecmp(s, t) stricmp(s, t)
#define strncasecmp(s, t, n) strnicmp(s, t, n)
#else
#define strcasecmp(s, t) _stricmp(s, t)
#define strncasecmp(s, t, n) _strnicmp(s, t, n)
#endif
#endif
#define _MATCH_STRING_ 1


#define DCL_STYLE_MATCH 0
#define UNIQUE_MATCH DCL_STYLE_MATCH
#define CASE_SENSITIVE 1
#define MATCH_WHOLE_STRING 2
#define RETURN_FIRST_MATCH 8
#define EXACT_MATCH (CASE_SENSITIVE|MATCH_WHOLE_STRING|RETURN_FIRST_MATCH)
#define WILDCARD_MATCH 16

#endif

epicsShareFuncMDBLIB extern char *clean_filename(char *filename);
epicsShareFuncMDBLIB extern long fexists(const char *filename);
#define RENAME_OVERWRITE 0x0001UL
extern long renameRobust(char *oldName, char *newName, unsigned long flags);

extern char *exp_notation(double x, long n1, long n2);
extern void add_to_headers(char **header, long n_headers, char **item,
    long min_width, long format_index);
long replaceFile(char *file, char *replacement);
epicsShareFuncMDBLIB long replaceFileAndBackUp(char *file, char *replacement);
epicsShareFuncMDBLIB void add_to_standard_headers(char *name_header, char *unit_header,
    char *printf_string, char *new_name, char *new_unit, char *new_format,
    long min_width);
extern long format_length(char *format_specifier);
extern char *sbinary(char *s, int len, long n);
epicsShareFuncMDBLIB extern long bitsSet(unsigned long data);
epicsShareFuncMDBLIB extern void interpret_escapes(char *s);
epicsShareFuncMDBLIB extern int replace_string(char *target, char *source, char *orig, char *newOne);
epicsShareFuncMDBLIB int replace_stringn(char *t, char *s, char *orig, char *repl, long count_limit);
epicsShareFuncMDBLIB void interpret_escaped_quotes(char *s);
epicsShareFuncMDBLIB extern int replaceString(char *t, char *s, char *orig, 
			 char *repl, long count_limit, long here);

extern char **wild_list(int *n_names_ret, int **origin_ret, char **item_list,
        int num_items);
epicsShareFuncMDBLIB int wild_match(char *string, char *tmplate);
epicsShareFuncMDBLIB int wild_match_ci(char *string, char *tmplate);
char *strchr_ci(char *s, char c);
epicsShareFuncMDBLIB int strcmp_ci(const char *s, const char *t);
epicsShareFuncMDBLIB char *expand_ranges(char *tmplate);
epicsShareFuncMDBLIB int has_wildcards(char *tmplate);
epicsShareFuncMDBLIB char *unescape_wildcards(char *tmplate);
epicsShareFuncMDBLIB int strcmp_nh(const char *s, const char *t);
epicsShareFuncMDBLIB int strcmp_skip(const char *s1, const char *s2, const char *skip);

/*   -- Routines for flagging and aborting on errors: */
epicsShareFuncMDBLIB extern void bomb(char *error_message, char *usage_message);
epicsShareFuncMDBLIB extern long bombre(char *error_message, char *usage_message, long return_value);
extern long err_mess(long status, char *routine_name, char *message);
extern long err_mess_sys(long status, char *routine_name, char *message);
extern void fatal_err(long error_code, char *error_message);

/*   -- IO routines: */
extern char *ffgets(char *target, long target_length, FILE *file_pointer);
epicsShareFuncMDBLIB extern FILE *fopen_e(char *file_name, char *open_mode, long error_mode);
#define FOPEN_EXIT_ON_ERROR 0
#define FOPEN_RETURN_ON_ERROR 1
#define FOPEN_INFORM_OF_OPEN  2
#define FOPEN_SAVE_IF_EXISTS  4
epicsShareFuncMDBLIB extern void backspace(long n);

/*   -- Run-time statistics: */
epicsShareFuncMDBLIB extern void init_stats(void);
epicsShareFuncMDBLIB extern void report_stats(FILE *fp, char *label);
epicsShareFuncMDBLIB extern double delapsed_time();
epicsShareFuncMDBLIB extern long memory_count(void);
epicsShareFuncMDBLIB extern long cpu_time(void);
epicsShareFuncMDBLIB extern long page_faults(void);

/*   -- Miscellaneous routines: */
extern long log_usage(char *log_file_name, char *program_name);
epicsShareFuncMDBLIB extern char *tmpname(char *target);
epicsShareFuncMDBLIB extern char *mktempOAG(char *tmpl);
epicsShareFuncMDBLIB char *mtime(void);
epicsShareFuncMDBLIB char *mtimes(void);
short IsLeapYear(short year);
short JulianDayFromMonthDay(short month, short day, short year, short *julianDay);
short MonthDayFromJulianDay(short julianDay, short year, short *month, short *day);
epicsShareFuncMDBLIB short TimeEpochToBreakdown(short *year, short *jDay, short *month, short *day, double *hour, double epochTime);
epicsShareFuncMDBLIB short TimeEpochToText(char *text, double epochTime);
epicsShareFuncMDBLIB short TimeBreakdownToEpoch(short year, short jDay, short month, short day, double hour, double *epochTime);
epicsShareFuncMDBLIB int makedir (char *newdir);

/********************* end of mdblib routines *****************************/

/************************ mathematical stuff *******************************/
#ifndef _MDB_MTH_
#define _MDB_MTH_ 1

#define FABS(x) fabs(x)
#define SIGN(x) ((x)<0?-1:((x)>0?1:0))
#define IS_NEGATIVE(x) (SIGN(x)==-1)
#define IS_POSITIVE(x) (SIGN(x)==1)

/* mdbmth routines */
epicsShareFuncMDBMTH long gaussianQuadrature(double (*fn)(double x), double a, double b, long n, double err, double *result);
epicsShareFuncMDBCOMMON extern int fixcount(char *filename, long n_points);
epicsShareFuncMDBMTH extern long factorial(long n);
epicsShareFuncMDBMTH extern double dfactorial(long n);
epicsShareFuncMDBMTH long trapazoidIntegration(double *x, double *y, long n, double *integral);
epicsShareFuncMDBMTH long trapazoidIntegration1(double *x, double *y, long n, double *integral);
epicsShareFuncMDBMTH int GillMillerIntegration(double *integral, double *error, double *f, double *x, long n);
epicsShareFuncMDBMTH extern double ipow(const double base, const int64_t power);
epicsShareFuncMDBMTH extern double zeroInterp(double (*function)(double x), double value,
                                              double x_initial, double x_final,
                                              double x_step, double effective_zero);
epicsShareFuncMDBMTH extern double zeroIntHalve(double (*function)(double x), double value,
                                                double x_initial, double x_final,
                                                double x_step, double effective_zero);
epicsShareFuncMDBMTH extern double zeroNewton(double (*function)(double x), 
                                              double value, double x_initial, double dx_deriv,
                                              long n_passes, double effective_zero);
epicsShareFuncMDBMTH extern int pointIsInsideContour(double x0, double y0, double *x, double *y,
						     int64_t n, double *center, double theta);
#if defined(__BORLANDC__)
epicsShareFuncMDBMTH extern double poly2(double *coef, long n_coefs, double x);
#else
epicsShareFuncMDBMTH extern double poly(double *coef, long n_coefs, double x);
#endif
epicsShareFuncMDBMTH extern double dpoly(double *coef, long n_coefs, double x);
epicsShareFuncMDBMTH extern double polyp(double *coef, long *power, long n_coefs, double x);
epicsShareFuncMDBMTH extern double dpolyp(double *coef, long *power, long n_coefs, double x);
epicsShareFuncMDBMTH extern int solveQuadratic(double a, double b, double c, double *solution);
epicsShareFuncMDBMTH extern double K_cei(double k);
epicsShareFuncMDBMTH extern double E_cei(double k);
epicsShareFuncMDBMTH extern double dK_cei(double k);
epicsShareFuncMDBMTH extern double dE_cei(double k);
epicsShareFuncMDBMTH extern float drand(long dummy);
epicsShareFuncMDBMTH extern double rdrand(double lower_limit, double upper_limit);
epicsShareFuncMDBMTH extern void r_theta_rand(double *r, double *theta, double r_min,
                           double r_max);
epicsShareFuncMDBMTH extern short inhibitRandomSeedPermutation(short state);
epicsShareFuncMDBMTH extern long permuteSeedBitOrder(long iseed);
epicsShareFuncMDBMTH extern double random_1(long iseed);
epicsShareFuncMDBMTH extern double random_2(long iseed);
epicsShareFuncMDBMTH extern double random_3(long iseed);
epicsShareFuncMDBMTH extern double random_4(long iseed);
epicsShareFuncMDBMTH extern double random_5(long iseed);
epicsShareFuncMDBMTH extern double random_6(long iseed);
epicsShareFuncMDBMTH extern double gauss_rn(long iseed, double (*urandom)(long iseed1));
epicsShareFuncMDBMTH extern double gauss_rn_lim(double mean, double sigma, double limit_in_sigmas, double (*urandom)(long iseed));

epicsShareFuncMDBMTH extern double random_oag(long iseed, long increment);
epicsShareFuncMDBMTH extern double gauss_rn_oag(long iseed, long increment, double (*urandom)(long iseed1, long increment));
epicsShareFuncMDBMTH extern double gauss_rn_lim_oag(double mean, double sigma, double limit_in_sigmas, long increment, double (*urandom)(long iseed, long increment));

epicsShareFuncMDBMTH extern long randomizeOrder(char *ptr, long size, long length, long iseed, double (*urandom)(long iseed1));
epicsShareFuncMDBMTH extern double nextHaltonSequencePoint(long ID);
epicsShareFuncMDBMTH extern int32_t startHaltonSequence(int32_t *radix, double value);
epicsShareFuncMDBMTH extern int32_t restartHaltonSequence(long ID, double value);
epicsShareFuncMDBMTH extern double nextModHaltonSequencePoint(long ID);
epicsShareFuncMDBMTH extern int32_t startModHaltonSequence(int32_t *radix, double value);
epicsShareFuncMDBMTH extern int32_t restartModHaltonSequence(long ID, double tiny);
epicsShareFuncMDBMTH extern long convertSequenceToGaussianDistribution(double *data, long points, double limit);
epicsShareFuncMDBMTH extern double KS_Qfunction(double lambda);
extern double twoVariableKStest(double *d1, long n1, double *d2, long n2, double *MaxCDFerror);
epicsShareFuncMDBMTH extern double linearCorrelationCoefficient(double *data1, double *data2, 
                                                                short *accept1, short *accept2, 
                                                                long rows, long *count);
epicsShareFuncMDBMTH extern double linearCorrelationSignificance(double r, long rows);
epicsShareFuncMDBMTH extern double shiftedLinearCorrelationCoefficient(double *data1, double *data2, 
                                                                       short *accept1, short *accept2,
                                                                       long rows, long *count, 
                                                                       long shift);
epicsShareFuncMDBMTH extern double betaInc(double x, double a, double b);
epicsShareFuncMDBMTH extern double betaComp(double a, double b);
epicsShareFuncMDBMTH extern double gammaP(double a, double x);
epicsShareFuncMDBMTH extern double gammaQ(double a, double x);
epicsShareFuncMDBMTH extern double tTailSigLevel(double t0, long nu, long tails);
epicsShareFuncMDBMTH extern double FSigLevel(double var1, double var2, long nu1, long nu2);
epicsShareFuncMDBMTH extern double rSigLevel(double r0, long nu);
epicsShareFuncMDBMTH double ChiSqrSigLevel(double ChiSquared0, long nu);
epicsShareFuncMDBMTH double normSigLevel(double z0, long tails);
epicsShareFuncMDBMTH double poissonSigLevel(long n, double n0);

epicsShareFuncMDBMTH extern int64_t is_prime(int64_t number);
epicsShareFuncMDBMTH extern int64_t smallest_factor(int64_t number);
epicsShareFuncMDBMTH extern int64_t next_prime_factor(int64_t *number);
epicsShareFuncMDBMTH int64_t largest_prime_factor(int64_t number);
epicsShareFuncMDBMTH extern int wofz(double *xi, double *yi, double *u, double *v, long *flag);


epicsShareFuncMDBCOMMON long lsfn(double *xd, double *yd, double *sy,
    long nd, long nf, double *coef, double *s_coef,
    double *chi, double *diff);
long lsfp(double *xd, double *yd, double *sy,
    long n_pts, long n_terms, long *power, double *coef, double *s_coef,
    double *chi, double *diff);
epicsShareFuncMDBCOMMON long lsfg(double *xd, double *yd, double *sy,
    long n_pts, long n_terms, int32_t *order,
    double *coef, double *s_coef, double *chi, double *diff,
    double (*fn)(double x, long ord));

/*functions for generation file names, moved from SDDSepics.c, May 8, 2002 */
/*i.e. the declarations of functions in logfile_generation.c */
epicsShareFuncMDBMTH extern double computeYearStartTime(double StartTime);
epicsShareFuncMDBMTH extern void getTimeBreakdown(double *Time, double *Day, 
                                                     double *Hour, double *JulianDay,
                                                     double *Year, double *Month, 
                                                     char **TimeStamp);
epicsShareFuncMDBMTH extern void makeTimeBreakdown(double Time, double *ptrTime,
                                                      double *ptrDay, double *ptrHour,
                                                      double *ptrJulianDay, double *ptrYear,
                                                      double *ptrMonth, char **ptrTimeStamp);
epicsShareFuncMDBMTH extern char *makeTimeStamp(double Time);
epicsShareFuncMDBMTH extern long double getLongDoubleTimeInSecs(void);
epicsShareFuncMDBMTH extern double getTimeInSecs(void);
epicsShareFuncMDBMTH extern double getHourOfDay(void);
epicsShareFuncMDBMTH extern char *getHourMinuteSecond ();
epicsShareFuncMDBMTH extern void checkGenerationFileLocks(char *match_date);
epicsShareFuncMDBMTH extern char *MakeGenerationFilename(char *rootname, long digits, 
                                                            char *delimiter, char *lastFile);
epicsShareFuncMDBMTH extern char *MakeDailyGenerationFilename(char *rootname, long digits,
                                                                 char *delimiter, long timetag);
epicsShareFuncMDBMTH extern char *MakeMonthlyGenerationFilename(char *rootname, long digits,
                                                                char *delimiter, long timetag);
epicsShareFuncMDBMTH extern char *MakeSCRDailyTimeGenerationFilename(char *rootname);
#define DEFAULT_GENERATIONS_DIGITS 4
#define USE_TIMETAG   0x0010U
epicsShareFuncMDBMTH extern void usleepSystemIndependent(long usec);
epicsShareFuncMDBMTH extern double k13(double z);
epicsShareFuncMDBMTH extern double k23(double z);
epicsShareFuncMDBMTH extern double gy(long n, double y);
epicsShareFuncMDBMTH extern double qromb(double (*func)(double x), long maxe,  double a, double b,  double eps);

#define DIFFEQ_EXIT_COND_FAILED -4
#define DIFFEQ_ZERO_STEPSIZE -3
#define DIFFEQ_CANT_TAKE_STEP -2
#define DIFFEQ_OUTSIDE_INTERVAL -1
#define DIFFEQ_XI_GT_XF 0
#define DIFFEQ_SOLVED 1
#define DIFFEQ_SOLVED_ALREADY 1
#define DIFFEQ_ZERO_FOUND 2
#define DIFFEQ_END_OF_INTERVAL 3
epicsShareFuncMDBMTH char *diffeq_result_description(long return_code);

epicsShareFuncMDBMTH extern long rk_odeint(
    double *y_i, void (*derivs)(double *yp, double *y, double x),
    long n_eq, double *accuracy, long *accmode, double *tiny, long *misses,
    double *x0, double xf, double x_accuracy, double h_start, double h_max,
    double *h_rec, double (*exfn)(double *yp, double *y, double x),
    double exit_accuracy, long n_to_skip,
    void (*store_data)(double *yp, double *y, double x, double exval)  );
epicsShareFuncMDBMTH extern long rk_odeint1(
    double *y_i, void (*derivs)(double *yp, double *y, double x),
    long n_eq, double *accuracy, long *accmode,
    double *tiny, long *misses, double *x0, double xf, double x_accuracy,
    double h_start, double h_max, double *h_rec );
epicsShareFuncMDBMTH extern long rk_odeint2(
    double *y_i, void (*derivs)(double *qp, double *q, double t), long n_eq,
    double *accuracy, long *accmode,
    double *tiny, long *misses, double *x0, double xf, double x_accuracy,
    double h_start, double h_max, double *h_rec, double exit_value,
    long i_exit_value, double exit_accuracy, long n_to_skip );
epicsShareFuncMDBMTH extern long rk_odeint3(
    double *y_i, void (*derivs)(double *yp, double *y, double x), long n_eq,
    double *accuracy, long *accmode, double *tiny, long *misses,
    double *x0, double xf, double x_accuracy, double h_start, double h_max,
    double *h_rec, double (*exfn)(double *yp, double *y, double x),
    double exit_accuracy);
epicsShareFuncMDBMTH extern long rk_odeint4(
    double *y_i, void (*derivs)(double *qp, double *q, double t), long n_eq,
    double *accuracy, long *accmode,
    double *tiny, long *misses, double *x0, double xf, double x_accuracy,
    double h_start, double h_max, double *h_rec, double exit_value,
    long i_exit_value, double exit_accuracy, long n_to_skip,
    void (*store_data)(double *qp, double *q, double t, double exfn) );
epicsShareFuncMDBMTH extern long rk_odeint_na(
    double *y_i, void (*derivs)(double *yp, double *y, double x),
    long n_eq, double *null1, long *null2, double *null3, long *null4,
    double *x0, double xf, double dummy1,
    double h, double dummy2, double *dummy3 );
epicsShareFuncMDBMTH extern long rk_odeint3_na(
    double *y_i, void (*derivs)(double *yp, double *y, double x), long n_eq,
    double *accuracy, long *accmode, double *tiny, long *misses,
    double *x0, double xf, double x_accuracy, double h_start, double h_max,
    double *h_rec, double (*exfn)(double *yp, double *y, double x),
    double exit_accuracy,  void (*stochastic)(double *y, double x, double h));
epicsShareFuncMDBMTH extern long bs_odeint(
    double *y_i, void (*derivs)(double *yp, double *y, double x), long n_eq,
    double *accuracy, long *accmode, double *tiny, long *misses,
    double *x0, double xf, double x_accuracy, double h_start, double h_max,
    double *h_rec,
    double (*exfn)(double *yp, double *y, double x), double exit_accuracy, long n_to_skip,
    void (*store_data)(double *qp, double *q, double t, double exfn_value)  );
epicsShareFuncMDBMTH extern long bs_odeint1(
    double *y_i, void (*derivs)(double *yp, double *y, double x), long n_eq,
    double *accuracy, long *accmode, double *tiny, long *misses,
    double *x0, double xf, double x_accuracy, double h_start, double h_max,
    double *h_rec );
epicsShareFuncMDBMTH extern long bs_odeint2(
    double *y_i, void (*derivs)(double *qp, double *q, double t), long n_eq,
    double *accuracy, long *accmode, double *tiny, long *misses,
    double *x0, double xf, double x_accuracy, double h_start, double h_max,
    double *h_rec,
    double exit_value, long i_exit_value, double exit_accuracy, long n_to_skip );
epicsShareFuncMDBMTH extern long bs_odeint3(
    double *y_i, void (*derivs)(double *yp, double *y, double x), long n_eq,
    double *accuracy, long *accmode, double *tiny, long *misses,
    double *x0, double xf, double x_accuracy, double h_start, double h_max,
    double *h_rec, double (*exfn)(double *yp, double *y, double x),
    double exit_accuracy);
epicsShareFuncMDBMTH extern long bs_odeint4(
    double *y_i, void (*derivs)(double *qp, double *q, double t), long n_eq,
    double *accuracy, long *accmode,
    double *tiny, long *misses, double *x0, double xf, double x_accuracy,
    double h_start, double h_max, double *h_rec, double exit_value,
    long i_exit_value, double exit_accuracy, long n_to_skip,
    void (*store_data)(double *qp, double *q, double t, double exfn) );
epicsShareFuncMDBMTH void mmid(double *y, double *dydx, long nvar, double xs, double htot,
    long nstep, double *yout, void (*derivs)(double *dydxa, double *ya, double xa)
    );
epicsShareFuncMDBMTH void mmid2(double *y, double *dydx, long nvar, double xs, double htot,
    long nstep, double *yout, void (*derivs)(double *dydxa, double *ya, double xa)
    );
epicsShareFuncMDBMTH extern long mmid_odeint3_na(
    double *y_i, void (*derivs)(double *yp, double *y, double x), long n_eq,
    double *accuracy, long *accmode, double *tiny, long *misses,
    double *x0, double xf, double x_accuracy, double h_start, double h_max,
    double *h_rec, double (*exfn)(double *yp, double *y, double x),
    double exit_accuracy);

epicsShareFuncMDBMTH void smoothData(double *data, long rows, long smoothPoints, long smoothPasses);
epicsShareFuncMDBMTH long despikeData(double *data, long rows, long neighbors, long passes, long averageOf,
    double threshold, long countLimit);
void SavitzkyGolayCoefficients(double *coef, long maxCoefs,
                              long order, long nLeft, long nRight,
                              long derivativeOrder, long wrapAround);
epicsShareFuncMDBCOMMON long SavitzkyGolaySmooth(double *data, long rows,
                                              long order, long nLeft, 
                                              long nRight, long derivativeOrder);
epicsShareFuncMDBMTH void TouchFile(char *filename);

#define SavitzyGolaySmooth(data, rows, order, nLeft, nRight, derivativeOrder) \
SavitzkyGolaySmooth(data, rows, order, nLeft, nRight, derivativeOrder)

epicsShareFuncMDBMTH extern long optimAbort(long abort);
epicsShareFuncMDBMTH extern double minc(double (*fn)(double *param), double *x, double *dx,
    double *dx_lim, double *xlo, double *xhi, long np, long ns_max,
    long p_flag);

epicsShareFuncMDBMTH void set_argument_offset(double offset);
epicsShareFuncMDBMTH double get_argument_offset();
epicsShareFuncMDBMTH double get_argument_scale();
epicsShareFuncMDBMTH void set_argument_scale(double scale);
epicsShareFuncMDBMTH double get_argument_offset();
epicsShareFuncMDBMTH double get_argument_scale();
epicsShareFuncMDBMTH double dtcheby(double x, long n);
epicsShareFuncMDBMTH double tcheby(double x, long n);
epicsShareFuncMDBMTH double ipower(double x, long n);
epicsShareFuncMDBMTH double dipower(double x, long n);
epicsShareFuncMDBMTH double eval_sum(double (*fn)(double x, long ord), double *coef, int32_t *order, long n_coefs, double x0);
epicsShareFuncMDBMTH long powellMin(double *yReturn, double *xGuess, double *dxGuess, double *xLowerLimit,
                                    double *xUpperLimit, long dims, double target, double tolerance,
                                    double (*func)(double *x, long *invalid), 
                                    void (*report)(double ymin, double *xmin, long pass, long evals, long dims),
                                    long maxPasses, long maxEvaluations, long linMinIterations);
#define SIMPLEX_NO_1D_SCANS        0x0001U
#define SIMPLEX_RANDOM_SIGNS       0x0002U
#define SIMPLEX_START_FROM_VERTEX1 0x0004U
#define SIMPLEX_VERBOSE_LEVEL1     0x0008U
#define SIMPLEX_VERBOSE_LEVEL2     0x0010U
#define SIMPLEX_ABORT_ANNOUNCE_STDOUT 0x0002UL
#define SIMPLEX_ABORT_ANNOUNCE_STDERR 0x0004UL
#define RCDS_USE_MIN_FOR_BRACKET 0x0020UL
epicsShareFuncMDBMTH long simplexMinAbort(unsigned long abort);
epicsShareFuncMDBMTH long simplexMin(double *yReturn, double *xGuess, double *dxGuess, double *xLowerLimit,
                double *xUpperLimit, short *disable,
                long dimensions, double target, double tolerance, double (*func)(double *x, long *invalid), 
                void (*report)(double ymin, double *xmin, long pass, long n_evals, long n_dim),
                long maxEvaluations, long maxPasses, long maxDivisions, double divisorFactor, 
                double passRangeFactor, unsigned long flags);
epicsShareFuncMDBMTH long simplexMinimization(double **simplexVector, double *fValue, double *coordLowerLimit,
                         double *coordUpperLimit, short *disable, long dimensions, long activeDimensions,
                         double target, double tolerance, long tolerance_mode,
                         double (*function)(double *x, long *invalid), long maxEvaluations, 
                         long *evaluations, unsigned long flags);
epicsShareFuncMDBMTH void enforceVariableLimits(double *x, double *xlo, double *xhi, long n);
#define ONEDSCANOPTIMIZE_REFRESH 0x0001U
epicsShareFuncMDBMTH long OneDScanOptimize (double *yReturn, double *xGuess,double *dxGuess,
                 double *xLowerLimit, double *xUpperLimit,short *disable,
                 long dimensions, 
                 double target,              /* will return if any value is <= this */
                 double tolerance,           /* <0 means fractional, >0 means absolute */
                 double (*func)(double *x, long *invalid), 
                 void (*report)(double ymin, double *xmin, long pass, long evals, long dims),
                 long maxSteps,
                 long maxDivsions, long maxRepeats,
                 unsigned long flags);                                            

epicsShareFuncMDBMTH long OneDParabolicOptimization
(double *yReturn, double *xGuess, double dx,
 double xLower, double xUpper, 
 double (*func)(double x, long *invalid),
 long maxCycles, double dxLimit, double tolerance,
 long maximize);

epicsShareFuncMDBMTH long grid_search_min(double *best_result, double *best_x, double *lower, double *upper,
    double *step, long n_dimen, double target, double (*func)(double *x, long *invalid));
epicsShareFuncMDBMTH long grid_sample_min(double *best_result, double *best_x, double *lower, double *upper,
    double *step, long n_dimen, double target, double (*func)(double *x, long *invalid), double sample_fraction,
    double (*random_f)(long iseed));
epicsShareFuncMDBMTH long randomSampleMin(double *best_result, double *best_x,
                                          double *lower, double *upper, long n_dimen,
                                          double target,
                                          double (*func)(double *x, long *invalid), long nSamples,
					  double (*random_f)(long iseed));
epicsShareFuncMDBMTH long randomWalkMin(double *best_result, double *best_x,
                                          double *lower, double *upper, double *range, long n_dimen,
                                          double target,
					  double (*func)(double *x, long *invalid), long nSamples,
					  double (*random_f)(long iseed));

epicsShareFuncMDBMTH long advance_values(double *value, long *value_index, double *initial, double *step, long n_values,
    long *counter, long *max_count, long n_indices);
epicsShareFuncMDBMTH long advance_counter(long *counter, long *max_count, long n_indices);

epicsShareFuncMDBMTH extern long compute_average(double *value, double *x, int64_t n);
epicsShareFuncMDBMTH extern long compute_middle(double *value, double *x, long n);
epicsShareFuncMDBMTH extern long compute_median(double *value, double *x, long n);
epicsShareFuncMDBMTH extern long compute_percentile(double *value, double *x, long n, double percentile);
epicsShareFuncMDBMTH extern long compute_percentiles(double *value, double *percent, long values, double *x, long n);
epicsShareFuncMDBMTH extern long compute_percentiles_flagged(double *position, double *percent, long positions, double *x,
                                                             int32_t *keep, int64_t n);
epicsShareFuncMDBMTH extern long approximate_percentiles(double *value, double *percent, long values, double *x, long n, long bins);

epicsShareFuncMDBMTH extern long find_average(double *value, double *x, long n);
epicsShareFuncMDBMTH extern long find_middle(double *value, double *x, long n);
epicsShareFuncMDBMTH extern long find_median(double *value, double *x, long n);
epicsShareFuncMDBMTH extern long find_percentile(double *value, double *x, long n, double percentile);
epicsShareFuncMDBMTH extern long find_median_of_row(double *value, double **x, long index, long n);

epicsShareFuncMDBMTH extern long make_histogram(double *hist, long n_bins, double lo, double hi, double *data,
    int64_t n_pts, long new_start);
epicsShareFuncMDBMTH extern long make_histogram_weighted(double *hist, long n_bins, double lo, double hi, double *data,
    long n_pts, long new_start, double *weight);
epicsShareFuncMDBMTH long computeMode(double *result, double *data, long pts, double binSize, long bins);

epicsShareFuncMDBMTH int64_t findCrossingPoint(int64_t start, double *data, int64_t points, double level, long direction,
                       double *interpData, double *result);
epicsShareFuncMDBMTH long findTopBaseLevels(double *top, double *base, double *data, int64_t points,
                       long bins, double sigmasRequired);

epicsShareFuncMDBMTH extern double standardDeviation(double *x, long n);
epicsShareFuncMDBMTH extern double standardDeviationThreaded(double *x, long n, long numThreads);
epicsShareFuncMDBMTH long unweightedLinearFit(double *xData, double *yData, long nData, double *slope, double *intercept, double *variance);
epicsShareFuncMDBMTH long unweightedLinearFitSelect(double *xData, double *yData, short *select, long nData, double *slope, double *intercept, double *variance);
epicsShareFuncMDBMTH extern double rmsValue(double *y, long n);
epicsShareFuncMDBMTH extern double rmsValueThreaded(double *y, long n, long numThreads);
epicsShareFuncMDBMTH extern double arithmeticAverage(double *y, long n);
epicsShareFuncMDBMTH extern double arithmeticAverageThreaded(double *y, long n, long numThreads);
epicsShareFuncMDBMTH extern double meanAbsoluteDeviation(double *y, long n);
epicsShareFuncMDBMTH extern double meanAbsoluteDeviationThreaded(double *y, long n, long numThreads);
epicsShareFuncMDBMTH extern long computeMoments(double *mean, double *rms, double *standardDev,
          double *meanAbsoluteDev, double *x, long n);
epicsShareFuncMDBMTH extern long computeMomentsThreaded(double *mean, double *rms, double *standardDev,
          double *meanAbsoluteDev, double *x, long n, long numThreads);
epicsShareFuncMDBMTH extern long computeCorrelations(double *C11, double *C12, double *C22, double *x, double *y, long n);
epicsShareFuncMDBMTH extern long computeCorrelationsThreaded(double *C11, double *C12, double *C22, double *x, double *y, long n, long numThreads);
epicsShareFuncMDBMTH extern long computeWeightedMoments(double *mean, double *rms, double *standardDev,
          double *meanAbsoluteDev, double *x, double *w, long n);
epicsShareFuncMDBMTH extern long computeWeightedMomentsThreaded(double *mean, double *rms, double *standardDev,
          double *meanAbsoluteDev, double *x, double *w, long n, long numThreads);
extern long accumulateMoments(double *mean, double *rms, double *standardDev,
          double *x, long n, long reset);
extern long accumulateMomentsThreaded(double *mean, double *rms, double *standardDev,
          double *x, long n, long reset, long numThreads);
extern long accumulateWeightedMoments(double *mean, double *rms, double *standardDev,
          double *x, double *w, long n, long reset);
extern long accumulateWeightedMomentsThreaded(double *mean, double *rms, double *standardDev,
          double *x, double *w, long n, long reset, long numThreads);
epicsShareFuncMDBMTH extern double weightedAverage(double *y, double *w, long n);
epicsShareFuncMDBMTH extern double weightedAverageThreaded(double *y, double *w, long n, long numThreads);
epicsShareFuncMDBMTH extern double weightedRMS(double *y, double *w, long n);
epicsShareFuncMDBMTH extern double weightedRMSThreaded(double *y, double *w, long n, long numThreads);
epicsShareFuncMDBMTH extern double weightedMAD(double *y, double *w, long n);
epicsShareFuncMDBMTH extern double weightedMADThreaded(double *y, double *w, long n, long numThreads);
epicsShareFuncMDBMTH extern double weightedStDev(double *y, double *w, long n);
epicsShareFuncMDBMTH extern double weightedStDevThreaded(double *y, double *w, long n, long numThreads);

epicsShareFuncMDBMTH extern int find_min_max(double *min, double *max, double *list, int64_t n);
epicsShareFuncMDBMTH int update_min_max(double *min, double *max, double *list, int64_t n, int32_t reset);
epicsShareFuncMDBMTH extern int index_min_max(int64_t *imin, int64_t *imax, double *list, int64_t n);
epicsShareFuncMDBMTH extern int index_min_max_long(int64_t *imin, int64_t *imax, long *list, int64_t n);
extern int assign_min_max(double *min, double *max, double val);
extern int find_min_max_2d(double *min, double *max, double **value,
            long n1, long n2);
extern int find_min_max_2d_float(float *min, float *max, float **value,
    long n1, long n2);
extern int find_min(double *min, double *loc, double *c1, double *c2, long n);
extern int find_max(double *max, double *loc, double *c1, double *c2, long n);
epicsShareFuncMDBMTH extern double max_double(int num_args, ...);
epicsShareFuncMDBMTH extern double min_double(int num_args, ...);

epicsShareFuncMDBMTH extern double max_in_array(double *array, long n);
epicsShareFuncMDBMTH extern double min_in_array(double *array, long n);
epicsShareFuncMDBMTH extern void median_filter(double *x, double *m, long n, long w);

/*interpolate functions from interp.c */
typedef struct {
  double value;
  unsigned long flags;
} OUTRANGE_CONTROL;
#define OUTRANGE_VALUE       0x00000001
#define OUTRANGE_SKIP        0x00000002
#define OUTRANGE_SATURATE    0x00000004
#define OUTRANGE_EXTRAPOLATE 0x00000008
#define OUTRANGE_ABORT       0x00000010
#define OUTRANGE_WARN        0x00000020
#define OUTRANGE_WRAP        0x00000040
epicsShareFuncMDBMTH extern double interpolate(double *f, double *x, int64_t n, double xo, 
                                               OUTRANGE_CONTROL *belowRange, 
                                               OUTRANGE_CONTROL *aboveRange, 
                                               long order, unsigned long *returnCode, long M);
epicsShareFuncMDBMTH extern double interp(double *y, double *x, long n, double x0, long warn, long order, long *returnCode);
int interpolate_minimum(double *fmin, double *zmin, double *value, double z_lo,
    double z_hi, long n);
epicsShareFuncMDBMTH double LagrangeInterp(double *x, double *f, long order, double x0, long *returnCode);


epicsShareFuncMDBLIB extern void substituteTagValue(char *input, long buflen, 
                        char **macroTag, char **macroValue, long macros); 

epicsShareFuncMDBMTH short interp_short(short *f, double *x, int64_t n, double xo, long warnings,
                                        short order, unsigned long *returnCode, long *next_start_pos);

epicsShareFuncMDBCOMMON long rcdsMinAbort(long abort);
epicsShareFuncMDBCOMMON long rcdsMin(double *yReturn, double *xBest, double *xGuess, double *dxGuess, double *xLowerLimit, double *xUpperLimit, double **dmat0, long dimensions, double target, double tolerance, double (*func)(double *x, long *invalid), void (*report)(double ymin, double *xmin, long pass, long evals, long dims), long maxEvaluations, long maxPasses, double noise, double rcdsStep, unsigned long flags);

#define iceil(x) ((int)ceil(x))
#define roundMDB(x) ( x < 0.0 ? ((int)((x)-.5)) : ((int)((x)+.5)) )

#ifndef MIN
#define MIN(x,y) ( ((x)>(y)) ? (y) : (x))
#endif
#ifndef MAX
#define MAX(x,y) ( ((x)<(y)) ? (y) : (x))
#endif

#define SWAP_LONG(x, y) {long tmp_swap_long; tmp_swap_long=(x); (x)=(y); (y)=tmp_swap_long; }
#define SWAP_INT(x, y) {int tmp_swap_int; tmp_swap_int=(x); (x)=(y); (y)=tmp_swap_int; }
#define SWAP_SHORT(x, y) {short tmp_swap_short; tmp_swap_short=(x); (x)=(y); (y)=tmp_swap_short; }
#define SWAP_DOUBLE(x, y) {double tmp_swap_double; tmp_swap_double=(x); (x)=(y); (y)=tmp_swap_double; }
#define SWAP_FLOAT(x, y) {float tmp_swap_float; tmp_swap_float=(x); (x)=(y); (y)=tmp_swap_float; }
#define SWAP_PTR(x, y) {void *tmp_swap_ptr; tmp_swap_ptr=(x); (x)=(y); (y)=tmp_swap_ptr; }

#define INTERPOLATE(y1, y2, x1, x2, x0) (((y2)-(y1))/((x2)-(x1))*((x0)-(x1)) + (y1))

#ifndef USE_OLD_IPOW
#define USE_OLD_IPOW 0
#endif

#if !(USE_OLD_IPOW)
/*
// These are header functions that will be compiled and inlined in
// every translation unit instead of previous linking/call overhead

// Branchless multiplication a bit faster than if check
// Need more microbenchmarking
//#define _IPOW_CHECK_ZERO if (x==0.) {return 0.;}
*/
#define _IPOW_CHECK_ZERO

# define ipow1(x) (x)
# define ipow2(x) ((x)*(x))
# define ipow3(x) ((x)*(x)*(x))
# define ipow4(x) (((x)*(x))*((x)*(x)))
static inline double ipow5(const double x) {
  _IPOW_CHECK_ZERO
  double y = x*x;
  return y*y*x;
}
static inline double ipow6(const double x) {
  _IPOW_CHECK_ZERO
  double y = x*x;
  return y*y*y;
}
static inline double ipow7(const double x) {
  _IPOW_CHECK_ZERO
  double y = x*x;
  return y*y*y*x;
}
static inline double ipow8(const double x) {
  _IPOW_CHECK_ZERO
  double y = x*x;
  y = y*y;
  return y*y;
}
static inline double ipow9(const double x) {
  _IPOW_CHECK_ZERO
  double y = x*x*x;
  return y*y*y;
}
static inline double ipow10(const double x) {
  _IPOW_CHECK_ZERO
  double y2 = x*x;
  double y4 = y2*y2;
  return y4*y4*y2;
}

#else

#define ipow1(x) ipow(x,1)
#define ipow2(x) ipow(x,2)
#define ipow3(x) ipow(x,3)
#define ipow4(x) ipow(x,4)
#define ipow5(x) ipow(x,5)
#define ipow6(x) ipow(x,6)
#define ipow7(x) ipow(x,7)
#define ipow8(x) ipow(x,8)
#define ipow9(x) ipow(x,9)
#define ipow10(x) ipow(x,10)

#endif

#define sqr(x) ipow2(x)
#define pow2(x) sqr(x)
#define pow3(x) ipow3(x)
#define pow4(x) ipow4(x)
#define pow5(x) ipow5(x)
#define pow6(x) ipow6(x)
#define pow7(x) ipow7(x)
#define pow8(x) ipow8(x)

#define SQR(x)  pow2(x)
#define POW2(x) pow2(x)
#define POW3(x) pow3(x)
#define POW4(x) pow4(x)
#define POW5(x) pow5(x)
#define POW6(x) pow6(x)
#define POW7(x) pow7(x)
#define POW8(x) pow8(x)

#include "constants.h"

#endif  /* _MDB_MTH_ */



epicsShareFuncMDBMTH void complex_multiply(
                      double *r0, double *i0,    /* result */
                      double  r1, double  i1,
                      double  r2, double  i2
                      );
epicsShareFuncMDBMTH void complex_divide(
                    double *r0, double *i0,    /* result */
                    double  r1, double  i1,
                    double  r2, double  i2,
                    double threshold
                    );


#if defined(_WIN32)
#include <windows.h>
#define sleep(sec) Sleep(sec * 1000)
#if !defined(_MINGW)
#define popen(x,y) _popen(x,y)
#define pclose(x) _pclose(x)
#endif
#endif

#if !defined(SVN_VERSION)
#define SVN_VERSION "unknown"
#endif

/* machine-specific include file: */
#ifdef VAX_VMS
#include "mdbvax.h"
#endif
#ifdef SUNOS4
#include "mdbsunos4.h"
#endif
#if defined(__TURBOC__) && !defined(__BORLANDC__)
#include "mdbtc.h"
#endif

#ifdef __cplusplus
}


#include <complex>
epicsShareFuncMDBMTH std::complex <double> complexErf(std::complex <double> z, long *flag);
epicsShareFuncMDBMTH std::complex <double> cexpi(double p);
epicsShareFuncMDBMTH std::complex <double> cipowr(std::complex <double> a, int n);


#endif 

#endif /* _MDB_ */

