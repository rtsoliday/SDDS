/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: match_string.h
 * purpose: flag definitions for use with match_string
 *
 * Michael Borland, 1988
 */

#undef epicsShareFuncMDBLIB
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_MDBLIB)
#define epicsShareFuncMDBLIB  __declspec(dllexport)
#else
#define epicsShareFuncMDBLIB
#endif
#else
#define epicsShareFuncMDBLIB
#endif

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
#define WILDCARD_MATCH 16
#define EXACT_MATCH (CASE_SENSITIVE|MATCH_WHOLE_STRING|RETURN_FIRST_MATCH)

#endif


