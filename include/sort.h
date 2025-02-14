/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sort.h
 * purpose: proto-types for routies in sort.c
 *
 * Michael Borland, 1989
 $Log: not supported by cvs2svn $
 Revision 1.5  2008/03/24 21:09:51  soliday
 Fixed definition for strcmp_skip so that it will work with elegant and
 shared libraries.

 Revision 1.4  2007/10/25 22:36:46  borland
 Added strcmp_skip(), which can compare strings while skipping characters
 from a third string.

 Revision 1.3  2002/08/14 15:40:17  soliday
 Added Open License

 Revision 1.2  1995/09/05 21:15:41  saunders
 First test release of the SDDS1.5 package.

 */
#ifndef _MDBSORT_INCLUDED_
#define _MDBSORT_INCLUDED_ 1
#include <stdio.h>

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

int double_cmpasc(const void *a, const void *b);
int double_abs_cmpasc(const void *a, const void *b);
int double_cmpdes(const void *a, const void *b);
int double_abs_cmpdes(const void *a, const void *b);
void double_copy(void *a, void *b);
int float_cmpasc(const void *a, const void *b);
int float_abs_cmpasc(const void *a, const void *b);
int float_cmpdes(const void *a, const void *b);
int float_abs_cmpdes(const void *a, const void *b);
void float_copy(void *a, void *b);
int long_cmpasc(const void *a, const void *b);
int long_abs_cmpasc(const void *a, const void *b);
int long_cmpdes(const void *a, const void *b);
int long_abs_cmpdes(const void *a, const void *b);
void long_copy(void *a, void *b);
int string_cmpasc(const void *a, const void *b);
int string_cmpdes(const void *a, const void *b);
void string_copy(void *a, void *b);
int row_compare(const void *a, const void *b);
void row_copy(void *a, void *b);
void set_up_row_sort(int sort_by_column, size_t n_columns,
    size_t element_size,
    int (*compare)(const void *a, const void *b));
int unique(void *base, size_t n_items, size_t size,
    int (*compare)(const void *a, const void *b),
    void (*copy)(void *a, void *b));
#ifdef VAX_VMS
void kqsort(char **base, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *));
#endif

#endif

