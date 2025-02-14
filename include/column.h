/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file   : column.h
 * purpose: provide definitions for use with routines get_mc_table()
 *          and put_mc_table(), which read and write data in the
 *          new standardized Mutli-Column TABLE format.
 *
 * definition of mc_table format:
 *   -Files are ordinary text format; fortran carraige control is not
 *     recommended.
 *   -Lines in file:
 *     1:  Nc: the number of columns of data   [Nlr:  # of lines per row. [Na: # of auxiliary quantities]]
 *     2:  quantity_name_1\unit_name_1\quantity_description_1\format_1
 *     3:  quantity_name_2\unit_name_2\quantity_description_2\format_2
 *               .
 *               .
 *               .
 *  Nc+1:  quantity_name_Nc\unit_name_Nc\quantity_description_Nc\format_Nc
 *  Nc+2:  title string
 *  Nc+3:  descriptive string
 *  Nc+4:  aux_name_1\aux_unit_1\aux_descrip_1\aux_value
 *  Nc+Na+4 : N: integer number of data points that follow
 *  Nc+Na+5:  q1[0]   q2[1]  q3[1] ... [label1]
 *                      .
 *                      .
 *                      .
 * Nc+Na+2+N*Nlr: q1[N]   q2[N]  q3[N] ...[labelN]
 *     [EOF]
 *   - Nlr defaults to 1.
 *   - The data points are in free format, with no restriction except that
 *     non-data text should not contain the characters ., +, -, or 0-9.
 *   - Any line beginning with '!' will be ignored.
 *   - Lines beyond Nc+2+N*Nlr will be ignored.
 *   - A literal \ can be obtained using the sequence \\ .
 *   - Data is stored by column, then by row; that is, data.value[i_col] is the
 *     array of data in the i_col-th column.
 *   - The label string for each line is optional.
 *
 * Michael Borland, 1989, 1991
 $Log: not supported by cvs2svn $
 Revision 1.2  1995/09/05 21:14:54  saunders
 First test release of the SDDS1.5 package.

 */

typedef struct {
    long n_cols, n_lines_per_row, n_rows;
    double **value;
    char **row_label;
    char **name;
    char **unit;
    char **description;
    char **format;
    char *title;
    char *label;
    long n_auxiliaries;
    char **aux_name;
    char **aux_unit;
    char **aux_description;
    double *aux_value;
    long flags;
    } MC_TABLE;

epicsShareFuncMDBMTH extern long get_mc_table(MC_TABLE *tab, char *file, long flags);
extern long put_mc_table(char *file, MC_TABLE *tab);
extern long put_mc_table_header(FILE *fpo, MC_TABLE *tab);

#define GMCT_WARN_MISSING_AUXVAL 1
#define GMCT_WARN_WRONG_COUNT    2
#define GMCT_WARN_INCOMPLETE_ROW 4
#define GMCT_WARN_EXTRA_ROWS     8
#define GMCT_WARNINGS (GMCT_WARN_MISSING_AUXVAL|GMCT_WARN_WRONG_COUNT|GMCT_WARN_INCOMPLETE_ROW|GMCT_WARN_EXTRA_ROWS)

