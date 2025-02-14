/**
 * @file sddskde2d.c
 * @brief Performs kernel density estimation (KDE) for two-dimensional data using the SDDS library.
 *
 * @details
 * This program processes input data from an SDDS file, applies kernel density estimation
 * to two selected columns, and outputs the resulting density estimates to a new SDDS file.
 * It supports flexible options such as specifying columns, margins, and whether
 * to use the same scales across data pages.
 *
 * @section Usage
 * ```
 * sddskde2d [<inputfile>] [<outputfile>] 
 *           [-pipe=[input][,output]] 
 *            -column=<column1,column2>
 *           [-samescales] 
 *           [-margin=<value>]
 * ```
 *
 * @section Options
 * | Required            | Description                                                                           |
 * |---------------------|---------------------------------------------------------------------------------------|
 * | `-column`              | Specify two column names for KDE, separated by a comma. Wildcards accepted. |
 *
 * | Option                 | Description                                                             |
 * |------------------------|-------------------------------------------------------------------------|
 * | `-pipe`                | Utilize the standard SDDS Toolkit pipe option.                         |
 * | `-samescales`          | Use the same X and Y ranges for all output pages.                      |
 * | `-margin`              | Ratio to extend the original data range (default: 0.05).               |
 *
 * ### Features
 * - Reads input data from SDDS files and writes results in SDDS format.
 * - Allows users to specify the columns for KDE using wildcards.
 * - Handles margins and ensures proper scaling of the data.
 * - Supports generating grids for density estimation.
 * - Implements bandwidth selection and Gaussian kernel functions.
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author
 * Yipeng Sun, M. Borland, R. Soliday
 */

#include <math.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_math.h>
#include "SDDS.h"
#include "mdb.h"
#include "scan.h"
#include "SDDSutils.h"

double bandwidth(double data[], int64_t M);
double gaussiankernelfunction(double sample);
double kerneldensityestimate(double *trainingdata_x, double *trainingdata_y,
                             double sample_x, double sample_y, int64_t n);
double *gridX(double start, double end, int N);
double *gridY(double start, double end, int N);

/* Enumeration for option types */
enum option_type {
  SET_COLUMN,
  SET_PIPE,
  SET_MARGIN,
  SET_SAME_SCALES,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "column",
  "pipe",
  "margin",
  "samescales"};

static char *usage =
  "Usage: sddskde2d [<inputfile>] [<outputfile>] \n"
  "                 [-pipe=[input][,output]]  \n"
  "                  -column=<column1,column2> \n"
  "                 [-samescales] \n"
  "                 [-margin=<value>]\n\n"
  "Options:\n"
  "  -column       Specify two column names separated by a comma. Wildcards are accepted.\n"
  "  -margin       Ratio to extend the original data (default: 0.05).\n"
  "  -samescales   Use the same X and Y ranges for all output pages.\n"
  "  -pipe         Utilize the standard SDDS Toolkit pipe option.\n\n"
  "Description:\n"
  "  sddskde2d performs kernel density estimation for two-dimensional data.\n\n"
  "Author:\n"
  "  Yipeng Sun (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  int n_test, n_total, same_scales;
  double lowerx, upperx;
  double lowery, uppery;
  double margin = 0.05;
  SDDS_DATASET SDDSin, SDDSout;
  char *input_file, *output_file, **column;
  long tmpfile_used = 0, columns, no_warnings = 1;
  unsigned long pipe_flags;
  long i, i_arg, j;
  SCANNED_ARG *s_arg;
  double **column_data_x, **column_data_y, *pdf;
  int64_t *rows, rows0;
  int32_t n_pages, i_page;

  input_file = output_file = NULL;
  pdf = NULL;
  columns = 0;
  column = NULL;
  same_scales = 0;

  n_test = 50;
  n_total = n_test * n_test;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  pipe_flags = 0;
  tmpfile_used = 0;
  if (argc < 2) {
    fprintf(stderr, "%s", usage);
    exit(1);
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_COLUMN:
        columns = s_arg[i_arg].n_items - 1;
        column = trealloc(column, sizeof(*column) * columns);
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          column[i - 1] = s_arg[i_arg].list[i];
        break;
      case SET_MARGIN:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -margin option. Too many qualifiers.");
        if (!get_double(&margin, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -margin value provided.");
        break;
      case SET_SAME_SCALES:
        if (s_arg[i_arg].n_items != 1)
          SDDS_Bomb("Invalid -sameScales option. No qualifiers are accepted.");
        same_scales = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags)) {
          fprintf(stderr, "Error (%s): invalid -pipe syntax\n", argv[0]);
          return 1;
        }
        break;
      }
    } else {
      if (!input_file)
        input_file = s_arg[i_arg].list[0];
      else if (output_file == NULL)
        output_file = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "Error (%s): too many filenames\n", argv[0]);
        return 1;
      }
    }
  }
  processFilenames("sddskde2d", &input_file, &output_file, pipe_flags, no_warnings, &tmpfile_used);
  if (!columns) {
    fprintf(stderr, "%s", usage);
    SDDS_Bomb("No column provided!");
  }
  if (!SDDS_InitializeInput(&SDDSin, input_file)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if ((columns = expandColumnPairNames(&SDDSin, &column, NULL, columns, NULL, 0, FIND_NUMERIC_TYPE, 0)) <= 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_Bomb("no columns selected.");
  }
  if (columns > 2) {
    fprintf(stderr, "%s", usage);
    SDDS_Bomb("Only 2 columns may be accepted.");
  }
  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 1, NULL, NULL, output_file))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, column[0], NULL) ||
      !SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, column[1], NULL) ||
      SDDS_DefineColumn(&SDDSout, "PDF", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  /* Read all the data in case we need to use fixed scales */
  n_pages = 0;
  column_data_x = NULL;
  column_data_y = NULL;
  rows = NULL;
  while (SDDS_ReadPage(&SDDSin) >= 0) {
    if ((rows0 = SDDS_CountRowsOfInterest(&SDDSin))) {
      column_data_x = (double **)SDDS_Realloc(column_data_x, sizeof(*column_data_x) * (n_pages + 1));
      column_data_y = (double **)SDDS_Realloc(column_data_y, sizeof(*column_data_y) * (n_pages + 1));
      rows = (int64_t *)SDDS_Realloc(rows, sizeof(*rows) * (n_pages + 1));
      rows[n_pages] = rows0;
      if (!(column_data_x[n_pages] = SDDS_GetColumnInDoubles(&SDDSin, column[0])))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(column_data_y[n_pages] = SDDS_GetColumnInDoubles(&SDDSin, column[1])))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      n_pages++;
    }
  }
  if (n_pages == 0)
    SDDS_Bomb("No data in file");

  if (same_scales) {
    double llx, lly, uux, uuy;
    lowerx = lowery = DBL_MAX;
    upperx = uppery = -DBL_MAX;
    for (i_page = 0; i_page < n_pages; i_page++) {
      find_min_max(&llx, &uux, column_data_x[i_page], rows[i_page]);
      lowerx = MIN(llx, lowerx);
      upperx = MAX(uux, upperx);
      find_min_max(&lly, &uuy, column_data_y[i_page], rows[i_page]);
      lowery = MIN(lly, lowery);
      uppery = MAX(uuy, uppery);
    }
  }

  pdf = malloc(sizeof(*pdf) * n_total);
  for (i_page = 0; i_page < n_pages; i_page++) {
    if (!SDDS_StartPage(&SDDSout, n_total))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!same_scales) {
      find_min_max(&lowerx, &upperx, column_data_x[i_page], rows[i_page]);
      find_min_max(&lowery, &uppery, column_data_y[i_page], rows[i_page]);
    }
    double *x_array = gridX(lowerx, upperx, n_test);
    double *y_array = gridY(lowery, uppery, n_test);
    for (j = 0; j < n_total; j++) {
      pdf[j] = kerneldensityestimate(column_data_x[i_page], column_data_y[i_page], x_array[j], y_array[j], rows[i_page]);
    }
    if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, x_array, n_total, column[0]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, y_array, n_total, column[1]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, pdf, n_total, "PDF"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    free(column_data_x[i_page]);
    free(column_data_y[i_page]);
    free(x_array);
    free(y_array);
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  free(pdf);
  free(column_data_x);
  free(column_data_y);
  free(rows);
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (tmpfile_used && !replaceFileAndBackUp(input_file, output_file)) {
    return 1;
  }
  free(column);
  return 0;
}

double bandwidth(double data[], int64_t M) {
  double sigma, bwidth, silver_factor;
  sigma = gsl_stats_sd(data, 1, M);
  silver_factor = pow(M, -0.16666666);
  bwidth = pow(silver_factor, 2.0) * pow(sigma, 2.0);
  return bwidth;
}

double gaussiankernelfunction(double sample) {
  double k;
  k = exp(-sample / 2.0);
  k = k / (2.0 * M_PI);
  return k;
}

double kerneldensityestimate(double *trainingdata_x, double *trainingdata_y,
                      double sample_x, double sample_y, int64_t n) {
  int64_t i;
  double pdf, hx, hy, z;

  hx = bandwidth(trainingdata_x, n);
  hy = bandwidth(trainingdata_y, n);
  pdf = 0.0;
  for (i = 0; i < n; i++) {
    z = (trainingdata_x[i] - sample_x) * (trainingdata_x[i] - sample_x) / hx;
    z += (trainingdata_y[i] - sample_y) * (trainingdata_y[i] - sample_y) / hy;
    pdf += gaussiankernelfunction(z);
  }
  pdf = pdf / (n * sqrt(hx) * sqrt(hy));
  return pdf;
}

double *gridX(double start, double end, int N) {
  double *x;
  int i, j, n_grid;
  double step;

  n_grid = N * N;
  x = (double *)calloc(n_grid, sizeof(double));
  step = (end - start) / (double)(N - 1);

  i = 0;
  for (j = 0; j < N; j++) {
    for (i = 0; i < N; i++) {
      x[i + j * N] = start + i * step;
    }
  }
  return x;
}

double *gridY(double start, double end, int N) {
  double *x;
  int i, j, n_grid;
  double step;

  n_grid = N * N;
  x = (double *)calloc(n_grid, sizeof(double));
  step = (end - start) / (double)(N - 1);

  i = 0;
  for (j = 0; j < N; j++) {
    for (i = 0; i < N; i++) {
      x[i + j * N] = start + j * step;
    }
  }
  return x;
}
