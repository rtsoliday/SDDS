/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file   : table.h
 * purpose: provide definitions for use with routines get_table()
 *	    and put_table(), which read and write data in dpl format
 * definition of dpl format:
 *   -Files are ordinary text format; fortran carraige control is not
 *     recommended.
 *   -Lines in file:
 *     1:  label for x-axis (independent variable)
 *     2:  label for y-axis (dependent variable)
 *     3:  label for plot title
 *     4:  label for top of plot
 *     5:  N: integer number of data points that follow
 *     6:  x[0]       y[0]    {sigma_y[0]  |  {sigma_x[0]  sigma_y[0] } }
 *                      .
 *                      .
 *                      .
 *     N+5:  x[N-1]     y[N-1]  {sigma_y[N-1]  |  {sigma_x[N-1]  sigma_y[N-1] } }
 *     [EOF]
 *   -The data points are in free format, with no restriction except that
 *    non-data text should not contain the characters ., +, -, or 0-9.
 *   -Any line beginning with '!' will be ignored.
 *   -Lines beyond N+5 will be ignored.
 *
 * Michael Borland, 1988
 */
#include <stdio.h>

#ifndef _TABLE_INCLUDED_
#define _TABLE_INCLUDED_ 1

#undef epicsShareFuncMDBCOMMON
#undef epicsShareFuncSDDS
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_MDBCOMMON)
#define epicsShareFuncMDBCOMMON  __declspec(dllexport)
#else
#define epicsShareFuncMDBCOMMON
#endif
#if defined(EXPORT_SDDS)
#define epicsShareFuncSDDS  __declspec(dllexport)
#else
#define epicsShareFuncSDDS
#endif
#else
#define epicsShareFuncMDBCOMMON
#define epicsShareFuncSDDS
#endif

/* control bit-flags for get_table() */
#define SWAP 1
#define REVERSE 2
#define REORDER_ASCENDING 4
#define REORDER_DESCENDING 8
#define SAVE_SIGMA_ARRAYS 16
#define READ_LABELS_ONLY 32
#define SDDS_NOCOMPRESS_NAMES 64

typedef struct {
	double *c1, *c2;           /* arrays of data in cols 1 & 2 */
        double *s1, *s2;           /* sigmas of data in cols 1 & 2 */
    	char *xlab, *ylab;         /* axis labels */
        char  *topline, *title;    /* other plot labels */
        long flags;                 /* data description bit-flags */
#define SIGMA_X_PRESENT 1
#define SIGMA_Y_PRESENT 2
	long n_data;                /* number of data points */
	} TABLE;

typedef struct {
	float *c1, *c2;           /* arrays of data in cols 1 & 2 */
        float *s1, *s2;           /* sigmas of data in cols 1 & 2 */
    	char *xlab, *ylab;         /* axis labels */
        char  *topline, *title;    /* other plot labels */
        long flags;                 /* data description bit-flags */
#define SIGMA_X_PRESENT 1
#define SIGMA_Y_PRESENT 2
	long n_data;                /* number of data points */
	} TABLE_FLOAT;

epicsShareFuncMDBCOMMON extern long get_table(TABLE *tab, char *file, int64_t sample_interval, long flags);
extern void put_table(char *file, TABLE *tab, char *format);
extern long get_table_float(TABLE_FLOAT *tab, char *file, 
                           long sample_interval, long flags);
extern void put_table_float(char *file, TABLE_FLOAT *tab, char *format);
extern double *double_array_from_float(float *f_array, long n_elements);
extern float  *float_array_from_double(double *d_array, long n_elements);
extern char *fgets_skip(char *s, long slen, FILE *fp, char skip_char, long skip_lines);
extern int fixcount(char *filename, long n_points);

epicsShareFuncSDDS int32_t SDDS_ReadIntoMplTable(TABLE *mpl_data, char *file, int64_t sample_interval, int32_t mpl_flags, char *SDDS_tags);
epicsShareFuncSDDS int32_t SDDS_WriteMplTable(TABLE *mpl_data, char *file);

#endif   /* _TABLE_INCLUDED_ */






