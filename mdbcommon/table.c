/**
 * @file table.c
 * @brief DPL/MPL format input/output functions.
 * @details Contains functions: get_table(), put_table(), float_array_from_double(),
 * get_table_float(), put_table_float(), double_array_from_float(),
 * delete_trailing_blanks(), fgets_skip().
 * @author Michael Borland
 * @date 1988
 */

#include "mdb.h"
#include <ctype.h>
#include "table.h"

#define SDDS_SUPPORT 1

#if SDDS_SUPPORT
#  include "SDDS.h"

int32_t SDDS_ReadIntoMplTable(TABLE *mpl_data, char *file, int64_t sample_interval, int32_t mpl_flags, char *SDDS_tags);
int32_t SDDS_WriteMplTable(TABLE *mpl_data, char *file);
long SDDS_AddMplDefinition(SDDS_TABLE *SDDS_table, char *label, char *suffix, char *default_name, char *filename);
void SDDS_ExtractNameAndUnit(char **name, char **unit, char *label);
void SDDS_FixMplName(char *name);

#endif

extern void delete_trailing_blanks(char *s);
extern int32_t SDDS_ReadIntoMplTable(TABLE *mpl_data, char *file, int64_t sample_interval, int32_t mpl_flags, char *SDDS_tags);

/* routine: get_table()
 * purpose: get a table from a file (dpl format)
 */
#define OAGBUFSIZ 1024

/**
 * @brief Gets a table from a file in DPL format.
 *
 * @param tab Pointer to a TABLE structure to store the data.
 * @param file Name of the file to read.
 * @param sample_interval Sampling interval for reading data points.
 * @param flags Flags to control the reading behavior.
 * @return Returns 1 on success, 0 on failure.
 */
long get_table(TABLE *tab, char *file, int64_t sample_interval, long flags) {
  long i, n, m;
  long sigma_y_present, sigma_x_and_y_present;
  char s[OAGBUFSIZ];
  double tmp;
  FILE *fp;
  char *ptr, *sdds_tags=NULL, *orig_tag = NULL;
  long sdds_expected = 0, sdds_data = 0;

  sigma_y_present = sigma_x_and_y_present = 0;
  tab->c1 = tab->c2 = tab->s1 = tab->s2 = NULL;
  tab->xlab = tab->ylab = tab->topline = tab->title = NULL;
  tab->flags = 0;
  tab->n_data = 0;

#if defined(SDDS_SUPPORT)

  if ((orig_tag = strchr(file, '='))) {
    sdds_tags = orig_tag;
    *sdds_tags = 0;
    if (!fexists(file))
      *sdds_tags = '=';
    else {
      sdds_expected = 1;
      sdds_tags++;
    }
  }

  if (!(fp = fopen_e(file, "r", FOPEN_RETURN_ON_ERROR))) {
    fprintf(stderr, "error: unable to open file %s in mode r (get_table)\n", file);
    exit(1);
  }

  if (!fgets_skip(s, OAGBUFSIZ, fp, '!', 1))
    return (0);
  fclose(fp);
  if (strncmp(s, "SDDS", 4) == 0) {
    if (!SDDS_ReadIntoMplTable(tab, file, sample_interval, flags, sdds_tags)) {
      fprintf(stderr, "error: unable to read requested data from SDDS file %s\n", file);
      exit(1);
    }
    sdds_data = 1;
    sigma_x_and_y_present = (sigma_y_present = tab->flags & SIGMA_Y_PRESENT) && (tab->flags & SIGMA_X_PRESENT);
  }
  if (sdds_expected && !sdds_data)
    *orig_tag = '=';

#endif /* SDDS_SUPPORT */

  if (!sdds_data) {
    fp = fopen_e(file, "r", 0);
#ifdef DEBUG
    fprintf(stderr, "file %s opened: sample_interval=%ld  flags=%x\n", file, sample_interval, flags);
#endif

    /* read in plot labels, skipping comment lines (! lines) */
    fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
    delete_trailing_blanks(s);
    cp_str(&(tab->xlab), s);
    fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
    delete_trailing_blanks(s);
    cp_str(&(tab->ylab), s);
    fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
    delete_trailing_blanks(s);
    cp_str(&(tab->title), s);
    fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
    delete_trailing_blanks(s);
    cp_str(&(tab->topline), s);
#ifdef DEBUG
    fprintf(stderr, "labels:\n<%s>\n<%s>\n<%s>\n<%s>\n", tab->xlab, tab->ylab, tab->title, tab->topline);
#endif
  }

  if (flags & SWAP) {
    ptr = tab->xlab;
    tab->xlab = tab->ylab;
    tab->ylab = ptr;
  }

  if (flags & READ_LABELS_ONLY) {
    if (!sdds_data)
      fclose(fp);
    return (1);
  }

  if (!sdds_data) {
    /* read in the number of data points */
    fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
    if (!*s || 1 != sscanf(s, "%lf", &tmp)) {
      fprintf(stderr, "error in format of file %s--couldn't scan number of points\n", file);
      exit(1);
    }
    n = tmp + 0.5;
#ifdef DEBUG
    fprintf(stderr, "n_pts = %ld\n", n);
#endif

    /* adjust the number of data points for the sampling interval */
    if (sample_interval <= 0)
      sample_interval = 1;
    n = n / sample_interval;

    /* allocate memory for column 1 and 2 data and sigmas */
    tab->c1 = tmalloc(sizeof(*(tab->c1)) * n);
    tab->c2 = tmalloc(sizeof(*(tab->c2)) * n);
    tab->s1 = tmalloc(sizeof(*(tab->s1)) * n);
    tab->s2 = tmalloc(sizeof(*(tab->s2)) * n);
    tab->flags = 0;

#ifdef DEBUG
    fprintf(stderr, "data pointers are: %x, %x, %x, %x\n", (unsigned long)tab->c1, (unsigned long)tab->c2, (unsigned long)tab->s1, (unsigned long)tab->s2);
#endif

    /* read in the data points, one by one */
    for (i = 0; i < n; i++) {
      if (fgets_skip(s, OAGBUFSIZ, fp, '!', (i == 0 ? 1 : sample_interval))) {
#ifdef DEBUG
        fprintf(stderr, "string: <%s>\n", s);
#endif
        if ((ptr = strchr(s, '!')))
          *ptr = 0;
        if (!get_double(tab->c1 + i, s) || !get_double(tab->c2 + i, s)) {
          fprintf(stderr, "warning: error in format of file %s\n", file);
          n = i - 1;
          tab->c1 = trealloc((void *)tab->c1, sizeof(*(tab->c1)) * n);
          tab->c2 = trealloc((void *)tab->c2, sizeof(*(tab->c2)) * n);
          tab->s1 = trealloc((void *)tab->s1, sizeof(*(tab->s1)) * n);
          tab->s2 = trealloc((void *)tab->s2, sizeof(*(tab->s2)) * n);
          break;
        }
        tab->s1[i] = tab->s2[i] = 0;
        if (i == 0) {
          /* First point--establish the number of sigma values in the
		   * data.  If there's only one sigma, interpret it as sigma_y,
		   * otherwise, the sigmas are sigma_x and sigma_y, in that
		   * order.
		   */
          if (get_double(tab->s1 + i, s)) {
            if (get_double(tab->s2 + i, s)) {
              sigma_x_and_y_present = 1;
              tab->flags |= SIGMA_X_PRESENT + SIGMA_Y_PRESENT;
            } else {
              tab->s2[i] = tab->s1[i];
              tab->s1[i] = 0;
              tab->flags |= SIGMA_Y_PRESENT;
              sigma_y_present = 1;
            }
          }
#ifdef DEBUG
          fprintf(stderr, "tab->flags = %x\n", tab->flags);
#endif
        } else {
          /* Scan sigma_y or (sigma_x, sigma_y) values, according 
		   * to what the first line indicated is supposed to be present.
		   */
          if (sigma_y_present && !get_double(tab->s2 + i, s)) {
            fprintf(stderr, "error in format of file %s--expected sigma is missing\n", file);
            exit(1);
          } else if (sigma_x_and_y_present && (!get_double(tab->s1 + i, s) || !get_double(tab->s2 + i, s))) {
            fprintf(stderr, "error in format of file %s--expected sigma is missing\n", file);
            exit(1);
          }
        }
        if (flags & SWAP) {
          /* exchange x and y data points */
          tmp = tab->c1[i];
          tab->c1[i] = tab->c2[i];
          tab->c2[i] = tmp;
          /* exchange the sigmas, too */
          tmp = tab->s1[i];
          tab->s1[i] = tab->s2[i];
          tab->s2[i] = tmp;
        }
      } else {
        fprintf(stderr, "Warning: file %s contains only %ld of %ld expected points.\n", file, i, n);
        n = i;
        tab->c1 = trealloc((void *)tab->c1, sizeof(*(tab->c1)) * n);
        tab->c2 = trealloc((void *)tab->c2, sizeof(*(tab->c2)) * n);
        tab->s1 = trealloc((void *)tab->s1, sizeof(*(tab->s1)) * n);
        tab->s2 = trealloc((void *)tab->s2, sizeof(*(tab->s2)) * n);
        break;
      }
    }
    tab->n_data = n;
  }
  n = tab->n_data;

  /* Check to see if the data needs to be reverse in order, i.e.,
   * if the first point should be the last and the last first. 
   */
  if (flags & REVERSE || ((flags & REORDER_ASCENDING) && tab->c1[0] > tab->c1[n - 1]) || ((flags & REORDER_DESCENDING) && tab->c1[0] < tab->c1[n - 1])) {
    /* Reverse the order of the data. */
    m = n - 1;
    for (i = 0; i < n / 2; i++) {
      if (m - i > n - 1)
        bomb("something impossible happened in get_table()", NULL);
      tmp = tab->c1[i];
      tab->c1[i] = tab->c1[m - i];
      tab->c1[m - i] = tmp;
      tmp = tab->c2[i];
      tab->c2[i] = tab->c2[m - i];
      tab->c2[m - i] = tmp;
      tmp = tab->s1[i];
      tab->s1[i] = tab->s1[m - i];
      tab->s1[m - i] = tmp;
      tmp = tab->s2[i];
      tab->s2[i] = tab->s2[m - i];
      tab->s2[m - i] = tmp;
    }
  }

  if (!sdds_data) {
    /* Check file for extra data. */
    if (fgets_skip(s, OAGBUFSIZ, fp, '!', sample_interval))
      fprintf(stderr, "Warning: file %s contains excess data (which is ignored).\n", file);
    fclose(fp);
  }

  /* If there were no sigmas, free the arrays. */
  if (!(tab->flags & SIGMA_X_PRESENT || tab->flags & SIGMA_Y_PRESENT) && !(flags & SAVE_SIGMA_ARRAYS)) {
    if (tab->s1) {
      tfree(tab->s1);
      tab->s1 = NULL;
    }
    if (tab->s2) {
      tfree(tab->s2);
      tab->s2 = NULL;
    }
  }

#ifdef DEBUG
  for (i = 0; i < n; i++) {
    fprintf(stderr, "%ldth point: c1=%le  c2=%le", i, tab->c1[i], tab->c2[i]);
    if (tab->s1)
      fprintf(stderr, "  s1=%le  s2=%le\n", tab->s1[i], tab->s2[i]);
    else
      fputc('\n', stderr);
  }
#endif

  return (1);
}

/* routine: put_table()
 * purpose: write a table of numbers to a file in dpl format
 *
 * Michael Borland, 1987.
 */

void put_table(char *file, TABLE *tab, char *format) {
  register long i;
  FILE *fp;

  if (SDDS_WriteMplTable(tab, file))
    return;

  fp = fopen_e(file, "w", FOPEN_SAVE_IF_EXISTS);

  fprintf(fp, "%s\n%s\n%s\n%s\n%-10ld\n", tab->xlab, tab->ylab, tab->title, tab->topline, tab->n_data);

  if (tab->flags & SIGMA_X_PRESENT && tab->flags & SIGMA_Y_PRESENT) {
    if (format == NULL)
      format = "%le  %le  %le  %le\n";
    for (i = 0; i < tab->n_data; i++)
      fprintf(fp, format, tab->c1[i], tab->c2[i], tab->s1[i], tab->s2[i]);
  } else if (tab->flags & SIGMA_X_PRESENT) {
    if (format == NULL)
      format = "%le  %le  %le 0.0\n";
    for (i = 0; i < tab->n_data; i++)
      fprintf(fp, format, tab->c1[i], tab->c2[i], tab->s1[i]);
  } else if (tab->flags & SIGMA_Y_PRESENT) {
    if (format == NULL)
      format = "%le  %le  %le\n";
    for (i = 0; i < tab->n_data; i++)
      fprintf(fp, format, tab->c1[i], tab->c2[i], tab->s2[i]);
  } else {
    if (format == NULL)
      format = "%le  %le\n";
    for (i = 0; i < tab->n_data; i++)
      fprintf(fp, format, tab->c1[i], tab->c2[i]);
  }

  fclose(fp);
}

/* routine: get_table_float()
 * purpose: get a table from a file (dpl format) and use float arrays
 */

long get_table_float(TABLE_FLOAT *tab, char *file, long sample_interval, long flags) {
  long i, n, m;
  long sigma_y_present, sigma_x_and_y_present;
  char s[OAGBUFSIZ];
  float tmp;
  FILE *fp;
  char *ptr;

  fp = fopen_e(file, "r", 0);
#ifdef DEBUG
  fprintf(stderr, "file %s opened: sample_interval=%ld  flags=%x\n", file, sample_interval, flags);
#endif

  sigma_y_present = sigma_x_and_y_present = 0;

  /* read in plot labels, skipping comment lines (! lines) */
  fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
  delete_trailing_blanks(s);
  cp_str(&(tab->xlab), s);
  fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
  delete_trailing_blanks(s);
  cp_str(&(tab->ylab), s);
  fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
  delete_trailing_blanks(s);
  cp_str(&(tab->title), s);
  fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
  delete_trailing_blanks(s);
  cp_str(&(tab->topline), s);
  if (flags & SWAP) {
    ptr = tab->xlab;
    tab->xlab = tab->ylab;
    tab->ylab = ptr;
  }
#ifdef DEBUG
  fprintf(stderr, "labels:\n<%s>\n<%s>\n<%s>\n<%s>\n", tab->xlab, tab->ylab, tab->title, tab->topline);
#endif

  if (flags & READ_LABELS_ONLY) {
    fclose(fp);
    return (1);
  }

  /* read in the number of data points */
  fgets_skip(s, OAGBUFSIZ, fp, '!', 1);
  if (!*s || 1 != sscanf(s, "%f", &tmp)) {
    fprintf(stderr, "error in format of file %s--couldn't scan number of points\n", file);
    exit(1);
  }
  n = tmp + 0.5;
#ifdef DEBUG
  fprintf(stderr, "n_pts = %ld\n", n);
#endif

  /* adjust the number of data points for the sampling interval */
  if (sample_interval <= 0)
    sample_interval = 1;
  n = n / sample_interval;

  /* allocate memory for column 1 and 2 data and sigmas */
  tab->c1 = tmalloc(sizeof(*(tab->c1)) * n);
  tab->c2 = tmalloc(sizeof(*(tab->c2)) * n);
  tab->s1 = tmalloc(sizeof(*(tab->s1)) * n);
  tab->s2 = tmalloc(sizeof(*(tab->s2)) * n);
  tab->flags = 0;

#ifdef DEBUG
  fprintf(stderr, "data pointers are: %x, %x, %x, %x\n", (unsigned long)tab->c1, (unsigned long)tab->c2, (unsigned long)tab->s1, (unsigned long)tab->s2);
#endif

  /* read in the data points, one by one */
  for (i = 0; i < n; i++) {
    if (fgets_skip(s, OAGBUFSIZ, fp, '!', (i == 0 ? 1 : sample_interval))) {
#ifdef DEBUG
      fprintf(stderr, "string: <%s>\n", s);
#endif
      if ((ptr = strchr(s, '!')))
        *ptr = 0;
      if (!get_float(tab->c1 + i, s) || !get_float(tab->c2 + i, s)) {
        fprintf(stderr, "warning: error in format of file %s--point %ld\n", file, n);
        n = i - 1;
        tab->c1 = trealloc((void *)tab->c1, sizeof(*(tab->c1)) * n);
        tab->c2 = trealloc((void *)tab->c2, sizeof(*(tab->c2)) * n);
        tab->s1 = trealloc((void *)tab->s1, sizeof(*(tab->s1)) * n);
        tab->s2 = trealloc((void *)tab->s2, sizeof(*(tab->s2)) * n);
        break;
      }
      tab->s1[i] = tab->s2[i] = 0;
      if (i == 0) {
        /* First point--establish the number of sigma values in the
	       * data.  If there's only one sigma, interpret it as sigma_y,
	       * otherwise, the sigmas are sigma_x and sigma_y, in that
	       * order.
	       */
        if (get_float(tab->s1 + i, s)) {
          if (get_float(tab->s2 + i, s)) {
            sigma_x_and_y_present = 1;
            tab->flags |= SIGMA_X_PRESENT + SIGMA_Y_PRESENT;
          } else {
            tab->s2[i] = tab->s1[i];
            tab->s1[i] = 0;
            tab->flags |= SIGMA_Y_PRESENT;
            sigma_y_present = 1;
          }
        }
#ifdef DEBUG
        fprintf(stderr, "tab->flags = %x\n", tab->flags);
#endif
      } else {
        /* Scan sigma_y or (sigma_x, sigma_y) values, according 
	       * to what the first line indicated is supposed to be present.
	       */
        if (sigma_y_present && !get_float(tab->s2 + i, s)) {
          fprintf(stderr, "error in format of file %s--expected sigma is missing\n", file);
          exit(1);
        } else if (sigma_x_and_y_present && (!get_float(tab->s1 + i, s) || !get_float(tab->s2 + i, s))) {
          fprintf(stderr, "error in format of file %s--expected sigma is missing\n", file);
          exit(1);
        }
      }
      if (flags & SWAP) {
        /* exchange x and y data points */
        tmp = tab->c1[i];
        tab->c1[i] = tab->c2[i];
        tab->c2[i] = tmp;
        /* exchange the sigmas, too */
        tmp = tab->s1[i];
        tab->s1[i] = tab->s2[i];
        tab->s2[i] = tmp;
      }
    } else {
      fprintf(stderr, "Warning: file %s contains only %ld of %ld expected points.\n", file, i, n);
      n = i;
      tab->c1 = trealloc((void *)tab->c1, sizeof(*(tab->c1)) * n);
      tab->c2 = trealloc((void *)tab->c2, sizeof(*(tab->c2)) * n);
      tab->s1 = trealloc((void *)tab->s1, sizeof(*(tab->s1)) * n);
      tab->s2 = trealloc((void *)tab->s2, sizeof(*(tab->s2)) * n);
      break;
    }
  }
  tab->n_data = n;

  /* Check to see if the data needs to be reverse in order, i.e.,
   * if the first point should be the last and the last first. 
   */
  if (flags & REVERSE || ((flags & REORDER_ASCENDING) && tab->c1[0] > tab->c1[n - 1]) || ((flags & REORDER_DESCENDING) && tab->c1[0] < tab->c1[n - 1])) {
    /* Reverse the order of the data. */
    m = n - 1;
    for (i = 0; i < n / 2; i++) {
      tmp = tab->c1[i];
      tab->c1[i] = tab->c1[m - i];
      tab->c1[m - i] = tmp;
      tmp = tab->c2[i];
      tab->c2[i] = tab->c2[m - i];
      tab->c2[m - i] = tmp;
      tmp = tab->s1[i];
      tab->s1[i] = tab->s1[m - i];
      tab->s1[m - i] = tmp;
      tmp = tab->s2[i];
      tab->s2[i] = tab->s2[m - i];
      tab->s2[m - i] = tmp;
    }
  }

  /* Check file for extra data. */
  if (fgets_skip(s, OAGBUFSIZ, fp, '!', sample_interval))
    fprintf(stderr, "Warning: file %s contains excess data (which is ignored).\n", file);

  /* If there were no sigmas, free the arrays. */
  if (!(tab->flags & SIGMA_X_PRESENT || tab->flags & SIGMA_Y_PRESENT) && !(flags & SAVE_SIGMA_ARRAYS)) {
    tfree(tab->s1);
    tab->s1 = NULL;
    tfree(tab->s2);
    tab->s2 = NULL;
  }

#ifdef DEBUG
  for (i = 0; i < n; i++) {
    fprintf(stderr, "%ldth point: c1=%le  c2=%le", i, tab->c1[i], tab->c2[i]);
    if (tab->s1)
      fprintf(stderr, "  s1=%le  s2=%le\n", tab->s1[i], tab->s2[i]);
    else
      fputc('\n', stderr);
  }
#endif

  fclose(fp);
  return (1);
}

/* routine: put_table_float()
 * purpose: write a table of numbers to a file in dpl format, using
 *          float table 
 *
 * Michael Borland, 1987.
 */

void put_table_float(char *file, TABLE_FLOAT *tab, char *format) {
  register long i;
  FILE *fp;

  fp = fopen_e(file, "w", FOPEN_SAVE_IF_EXISTS);

  fprintf(fp, "%s\n%s\n%s\n%s\n%-10ld\n", tab->xlab, tab->ylab, tab->title, tab->topline, tab->n_data);

  if (tab->flags & SIGMA_X_PRESENT && tab->flags & SIGMA_Y_PRESENT) {
    if (format == NULL)
      format = "%e  %e  %e  %e\n";
    for (i = 0; i < tab->n_data; i++)
      fprintf(fp, format, tab->c1[i], tab->c2[i], tab->s1[i], tab->s2[i]);
  } else if (tab->flags & SIGMA_X_PRESENT) {
    if (format == NULL)
      format = "%e  %e  %e   0.0\n";
    for (i = 0; i < tab->n_data; i++)
      fprintf(fp, format, tab->c1[i], tab->c2[i], tab->s1[i]);
  } else if (tab->flags & SIGMA_Y_PRESENT) {
    if (format == NULL)
      format = "%e  %e  %e\n";
    for (i = 0; i < tab->n_data; i++)
      fprintf(fp, format, tab->c1[i], tab->c2[i], tab->s2[i]);
  } else {
    if (format == NULL)
      format = "%e  %e\n";
    for (i = 0; i < tab->n_data; i++)
      fprintf(fp, format, tab->c1[i], tab->c2[i]);
  }

  fclose(fp);
}

/* routine: float_array_from_double()
 * purpose: copy an array of doubles into a newly-allocated array of floats 
 */

float *float_array_from_double(double *x, long n) {
  register float *ptr;
  register long i;

  if (!(ptr = tmalloc(n * sizeof(*ptr))))
    return (ptr);

  for (i = 0; i < n; i++)
    ptr[i] = x[i];
  return (ptr);
}

/* routine: double_array_from_float()
 * purpose: copy an array of floats into a newly-allocated array of doubles 
 */

double *double_array_from_float(float *x, long n) {
  register double *ptr;
  register long i;

  if (!(ptr = tmalloc(n * sizeof(*ptr))))
    return (ptr);

  for (i = 0; i < n; i++)
    ptr[i] = x[i];
  return (ptr);
}

/* routine: delete_trailing_blanks
 * purpose: delete trailing blanks in a string
 */

void delete_trailing_blanks(char *s) {
  register char *ptr;

  ptr = strlen(s) - 1 + s;
  while (ptr >= s && isspace(*ptr))
    *ptr-- = 0;
}

/* routine: fgets_skip()
 * purpose: fgets() with skipping of lines according to a comment character
 *          and a sampling interval
 */

char *fgets_skip(char *s,        /* buffer for lines */
                 long slen,      /* length of lines to read */
                 FILE *fp,       /* file to read from */
                 char skip_char, /* ignore lines that begin with this character */
                 long skip_lines /* skip this many (non-ignored) lines before returning a line */
) {
  register long i;
  char c;

  i = 0;
  do {
    if (!fgets(s, slen, fp))
      return (NULL);
    if (s[0] != skip_char)
      i++;
  } while (i < skip_lines);
  if ((long)strlen(s) >= slen - 1) {
    while ((c = getc(fp)))
      if (c == '\n')
        break;
  }
  return (s);
}
