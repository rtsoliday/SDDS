/**
 * @file mcTable.c
 * @brief Read and write routines for Multi-Column Tables.
 *
 * This file contains functions to handle multi-column table operations,
 * including reading from and writing to table files. It provides mechanisms
 * to parse table headers, handle auxiliary quantities, and manage table data.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include "column.h"
#include <ctype.h>

#define DEBUG 0

static char buffer[1024];
static char buffer1[1024];
char *fgets_mc_skip(char *s, long slen, FILE *fp, char skip_char);
void get_name_unit_descrip_format(char **name, char **unit, char **descrip, char **format, char *_buffer);

/**
 * @brief Reads a multi-column table from a file.
 *
 * Parses the specified file to populate the MC_TABLE structure with
 * column information, auxiliary quantities, and table data.
 *
 * @param table Pointer to the MC_TABLE structure to populate.
 * @param file Path to the table file to read.
 * @param flags Flags to control warning messages and behavior.
 * @return Returns 1 on success, 0 on failure.
 */
long get_mc_table(MC_TABLE *table, char *file, long flags) {
  FILE *fpi;
  long i, j, i_col;
  char *ptr;

  if (!(fpi = fopen_e(file, "r", 1)))
    return (0);

  if (!fgets_mc_skip(buffer, 1024, fpi, '!'))
    bomb("unable to read first line of table file", NULL);
  if (!(get_long(&table->n_cols, buffer)))
    bomb("unable to scan number of columns in table file", NULL);
  if (table->n_cols <= 0)
    bomb("number of columns is invalid in table file", NULL);
  table->n_lines_per_row = 1;
  table->n_auxiliaries = 0;
  if (get_long(&table->n_lines_per_row, buffer)) {
    if (table->n_lines_per_row <= 0)
      bomb("number of lines per row is invalid in table file", NULL);
    if (get_long(&table->n_auxiliaries, buffer) &&
        table->n_auxiliaries < 0)
      bomb("number of auxiliary quantities is invalid in table file", NULL);
  }

#if DEBUG
  printf("file %s\nn_cols = %ld  n_lines_per_row = %ld  n_auxiliaries = %ld\n",
         file, table->n_cols, table->n_lines_per_row, table->n_auxiliaries);
#endif

  table->name = tmalloc(sizeof(char *) * table->n_cols);
  table->unit = tmalloc(sizeof(char *) * table->n_cols);
  table->description = tmalloc(sizeof(char *) * table->n_cols);
  table->format = tmalloc(sizeof(char *) * table->n_cols);

  for (i = 0; i < table->n_cols; i++) {
    if (!fgets_mc_skip(buffer, 1024, fpi, '!'))
      bomb("missing quantity name and units line in table file", NULL);
    get_name_unit_descrip_format(table->name + i, table->unit + i,
                                 table->description + i, table->format + i, buffer);
    if (table->format[i] && !strchr(table->format[i], '%'))
      table->format[i] = NULL;
  }

#if DEBUG
  for (i = 0; i < table->n_cols; i++)
    printf("column %ld:  %s in %s--%s in format %s\n",
           i, table->name[i], table->unit[i], table->description[i],
           (table->format[i] ? table->format[i] : "{null}"));
#endif

  if (!fgets_mc_skip(buffer, 1024, fpi, '!'))
    bomb("missing title string in table file", NULL);
  cp_str(&table->title, buffer);

  if (!fgets_mc_skip(buffer, 1024, fpi, '!'))
    bomb("missing label string in table file", NULL);
  cp_str(&table->label, buffer);

#if DEBUG
  printf("title: %s\nlabel: %s\n", table->title, table->label);
#endif

  if (table->n_auxiliaries) {
#if DEBUG
    printf("%ld auxuliary quantities are expected\n", table->n_auxiliaries);
#endif
    /* get auxiliary quantity names\units... */
    table->aux_name = tmalloc(sizeof(char *) * table->n_auxiliaries);
    table->aux_unit = tmalloc(sizeof(char *) * table->n_auxiliaries);
    table->aux_description = tmalloc(sizeof(char *) * table->n_auxiliaries);
    table->aux_value = tmalloc(sizeof(*(table->aux_value)) * table->n_auxiliaries);
    for (i = 0; i < table->n_auxiliaries; i++) {
      if (!fgets_mc_skip(buffer, 1024, fpi, '!'))
        bomb("missing quantity name and units line in table file", NULL);
      get_name_unit_descrip_format(table->aux_name + i, table->aux_unit + i,
                                   table->aux_description + i, &ptr, buffer);
      if ((!ptr || 1 != sscanf(ptr, "%lf", table->aux_value + i)) && flags & GMCT_WARN_MISSING_AUXVAL)
        printf("warning: missing value for auxiliary quantity %s---zero assumed\n", table->aux_name[i]);
    }
  }

#if DEBUG
  for (i = 0; i < table->n_auxiliaries; i++) {
    printf("auxiliary %ld:  %s = %le %s---%s\n",
           i, table->aux_name[i], table->aux_value[i], table->aux_unit[i],
           table->aux_description[i]);
  }
#endif

  if (!fgets_mc_skip(buffer, 1024, fpi, '!'))
    bomb("missing number of data rows in table file", NULL);
  if (!get_long(&table->n_rows, buffer) || table->n_rows < 0)
    bomb("invalid number of data rows specified in table file", NULL);
  if (table->n_rows == 0)
    return (1);

#if DEBUG
  printf("n_rows = %ld\n", table->n_rows);
#endif

  table->value = (double **)zarray_2d(sizeof(double), table->n_cols, table->n_rows);
  table->row_label = (char **)tmalloc(sizeof(char *) * table->n_rows);

  for (i = 0; i < table->n_rows; i++) {
    i_col = 0;
    table->row_label[i] = NULL;
    for (j = 0; j < table->n_lines_per_row && i_col < table->n_cols; j++) {
      if (!fgets_mc_skip(buffer, 1024, fpi, '!'))
        break;
      strcpy(buffer1, buffer);
#if DEBUG
      printf("row %ld, line %ld:\n%s\n", i, j, buffer);
#endif
      while (i_col < table->n_cols && get_double(table->value[i_col] + i, buffer))
        i_col++;
    }

    if (!is_blank(buffer)) {
      cp_str(table->row_label + i, buffer);
      while (isspace(*table->row_label[i]))
        table->row_label[i]++;
    } else
      cp_str(table->row_label + i, "");

    if (i_col == 0) {
      if (flags & GMCT_WARN_WRONG_COUNT)
        printf("warning: fewer data rows than expected in file %s\n", file);
      break;
    } else if (i_col != table->n_cols) {
      if (flags & GMCT_WARN_INCOMPLETE_ROW) {
        printf("warning: incomplete data row in file %s\n", file);
        printf("line in question is:\n%s\n", buffer1);
        printf("skipping to next line\n");
      }
      i--;
      continue;
    }
#if DEBUG
    printf("%ld: ", i);
    for (i_col = 0; i_col < table->n_cols; i_col++)
      printf("%.4le  ", table->value[i_col][i]);
    putchar('\n');
#endif
  }
  if (i == table->n_rows && fgets_mc_skip(buffer, 1024, fpi, '!') && flags & GMCT_WARN_EXTRA_ROWS)
    printf("warning: file %s contains extra rows, which are ignored\n",
           file);
  table->n_rows = i;
  return (1);
}

/* routine: fgets_mc_skip()
 * purpose: fgets() with skipping of lines according to a comment character
 */

char *fgets_mc_skip(
  char *s,       /* buffer for lines */
  long slen,     /* length of lines to read */
  FILE *fp,      /* file to read from */
  char skip_char /* ignore lines that begin with this character */
) {
  do {
    if (!fgets(s, slen, fp))
      return (NULL);
    chop_nl(s);
    if (s[0] != skip_char)
      break;
  } while (1);
  return (s);
}

long put_mc_table(char *file, MC_TABLE *table) {
  FILE *fpo;
  long i, j, wrap;

  if (!(fpo = fopen(file, "w"))) {
    printf("unable to write to file %s\n", file);
    return (0);
  }

  if (table->n_lines_per_row < 1)
    table->n_lines_per_row = 1;

  put_mc_table_header(fpo, table);

  wrap = table->n_cols / table->n_lines_per_row;
  for (i = 0; i < table->n_rows; i++) {
    j = 0;
    while (j < table->n_cols) {
      fprintf(fpo, (table->format && table->format[j] ? table->format[j] : "%le"),
              table->value[j][i]);
      if (j == table->n_cols - 1 && table->row_label && table->row_label[j])
        fprintf(fpo, " %s", table->row_label[j]);
      if (++j % wrap == 0)
        fputc('\n', fpo);
      else
        fputs("  ", fpo);
    }
  }
  fclose(fpo);
  return (1);
}

long put_mc_table_header(FILE *fpo, MC_TABLE *table) {
  long i;

  fprintf(fpo, "%ld  %ld  %ld\n", table->n_cols, table->n_lines_per_row, table->n_auxiliaries);

  for (i = 0; i < table->n_cols; i++)
    fprintf(fpo, "%s\\%s\\%s\\%s\n",
            table->name[i],
            (table->unit[i] && (long)strlen(table->unit[i]) > 1) ? table->unit[i] : " ",
            (table->description[i] && (long)strlen(table->description[i]) > 1) ? table->description[i] : " ",
            (table->format[i] ? table->format[i] : ""));

  fprintf(fpo, "%s\n%s\n",
          (table->title && (long)strlen(table->title) > 1) ? table->title : " ",
          (table->label && (long)strlen(table->label) > 1) ? table->label : " ");

  for (i = 0; i < table->n_auxiliaries; i++)
    fprintf(fpo, "%s\\%s\\%s\\%.16e\n", table->aux_name[i], table->aux_unit[i],
            table->aux_description[i], table->aux_value[i]);

  fprintf(fpo, "%ld\n", table->n_rows);
  return (1);
}

void get_name_unit_descrip_format(char **name, char **unit, char **descrip, char **format, char *buf) {
  char *ptr, *ptrn, *ptru, *ptrd, *ptrf;
  char *blank_string;

  cp_str(&blank_string, " ");

  ptr = ptrn = buf;
  ptru = ptrd = ptrf = NULL;
  while ((ptr = strchr(ptr, '\\'))) {
    if (ptr[1] == '\\')
      ptr += 2;
    else {
      if (!ptru) {
        *ptr = 0;
        ptru = ++ptr;
      } else if (!ptrd) {
        *ptr = 0;
        ptrd = ++ptr;
      } else {
        *ptr = 0;
        ptrf = ++ptr;
        break;
      }
    }
  }
  cp_str(name, ptrn);
  trim_spaces(*name);
  if (ptru) {
    cp_str(unit, ptru);
    trim_spaces(*unit);
  } else
    *unit = blank_string;
  if (ptrd && !is_blank(ptrd)) {
    cp_str(descrip, ptrd);
    trim_spaces(*descrip);
  } else
    cp_str(descrip, *name);
  if (ptrf) {
    cp_str(format, ptrf);
    trim_spaces(*format);
  } else
    *format = NULL;
}

