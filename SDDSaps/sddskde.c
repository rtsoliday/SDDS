/**
 * @file sddskde.c
 * @brief Kernel Density Estimation for SDDS Data.
 *
 * @details
 * This program performs Kernel Density Estimation (KDE) for one-dimensional data
 * read from an SDDS file. It computes probability density functions (PDFs) and cumulative
 * distribution functions (CDFs) for the specified columns and writes the results to
 * an SDDS output file. It uses Gaussian kernel functions for KDE calculations and supports
 * options for selecting columns and customizing the margin for data extension.
 *
 * @section Usage
 * ```
 * sddskde [<inputfile>] [<outputfile>] 
 *         [-pipe=[input][,output]] 
 *          -column=<list of columns>
 *         [-margin=<value>]
 * ```
 *
 * @section Options
 * | Required            | Description                                                                           |
 * |---------------------|---------------------------------------------------------------------------------------|
 * | `-column`           | Specifies the columns for KDE calculations. Comma-separated and supports wildcards.   |
 *
 * | Optional            | Description                                                     |
 * |---------------------|-----------------------------------------------------------------|
 * | `-pipe`             | Enable SDDS Toolkit piping for input/output.                   |
 * | `-margin`           | Set the margin as a ratio to extend data range (default 0.3).   |
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * Yipeng Sun, H. Shang, M. Borland, R. Soliday
 */

#include <math.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_math.h>
#include "SDDS.h"
#include "mdb.h"
#include "scan.h"
#include "SDDSutils.h"

double bandwidth(double data[], int M);
double gaussiankernelfunction(double sample);
double kerneldensityestimate(double *trainingdata, double sample, int64_t n);
double *linearspace(double initial, double final, int N);

/* Enumeration for option types */
enum option_type {
  SET_COLUMN,
  SET_PIPE,
  SET_MARGIN,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "column",
  "pipe",
  "margin",
};

static const char *usage =
  "sddskde [<inputfile>] [<outputfile>]\n"
  "        [-pipe=[input][,output]]\n"
  "         -column=<list of columns>\n"
  "        [-margin=<value>]\n"
  "Options:\n"
  "-column         provide column names separated by commas, wild card accepted.\n"
  "-margin         provide the ratio to extend the original data, default 0.3.\n"
  "-pipe           The standard SDDS Toolkit pipe option.\n\n"
  "sddskde performs kernel density estimation for one-dimensional data.\n"
  "Program by Yipeng Sun and Hairong Shang (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ").\n";

int main(int argc, char **argv) {
  int n_test = 100;
  double min_temp, max_temp;
  double gap, lower, upper;
  double margin = 0.3;
  SDDS_DATASET sdds_in, sdds_out;
  char *input_file = NULL, *output_file = NULL, **column = NULL;
  long tmpfile_used = 0, columns = 0, no_warnings = 1;
  unsigned long pipe_flags;
  long i, i_arg, j;
  SCANNED_ARG *s_arg;
  double *column_data = NULL, *pdf = NULL, *cdf = NULL;
  char buffer_pdf[1024], buffer_cdf[1024], buffer_pdf_units[1024];
  int64_t rows;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  pipe_flags = 0;

  if (argc < 2) {
    fprintf(stderr, "%s", usage);
    exit(EXIT_FAILURE);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_COLUMN:
        columns = s_arg[i_arg].n_items - 1;
        column = realloc(column, sizeof(*column) * columns);
        for (i = 1; i < s_arg[i_arg].n_items; i++) {
          column[i - 1] = s_arg[i_arg].list[i];
        }
        break;
      case SET_MARGIN:
        if (s_arg[i_arg].n_items != 2) {
          SDDS_Bomb("Invalid -margin option!");
        }
        if (!get_double(&margin, s_arg[i_arg].list[1])) {
          SDDS_Bomb("Invalid -margin value provided!");
        }
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags)) {
          fprintf(stderr, "Error (%s): invalid -pipe syntax\n", argv[0]);
          return EXIT_FAILURE;
        }
        break;
      default:
        fprintf(stderr, "Unknown option: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    } else {
      if (!input_file) {
        input_file = s_arg[i_arg].list[0];
      } else if (!output_file) {
        output_file = s_arg[i_arg].list[0];
      } else {
        fprintf(stderr, "Error (%s): too many filenames\n", argv[0]);
        return EXIT_FAILURE;
      }
    }
  }

  processFilenames("sddskde", &input_file, &output_file, pipe_flags, no_warnings, &tmpfile_used);

  if (!columns) {
    fprintf(stderr, "%s", usage);
    SDDS_Bomb("No column provided!");
  }

  if (!SDDS_InitializeInput(&sdds_in, input_file)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if ((columns = expandColumnPairNames(&sdds_in, &column, NULL, columns, NULL, 0, FIND_NUMERIC_TYPE, 0)) <= 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_Bomb("No columns selected.");
  }

  if (!SDDS_InitializeOutput(&sdds_out, SDDS_BINARY, 1, NULL, NULL, output_file)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  for (i = 0; i < columns; i++) {
    char *units = NULL;
    if (SDDS_GetColumnInformation(&sdds_in, "units", &units, SDDS_GET_BY_NAME, column[i]) != SDDS_STRING) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (units && strlen(units)) {
      snprintf(buffer_pdf_units, sizeof(buffer_pdf_units), "1/(%s)", units);
    } else {
      buffer_pdf_units[0] = '\0';
    }

    free(units);

    snprintf(buffer_pdf, sizeof(buffer_pdf), "%sPDF", column[i]);
    snprintf(buffer_cdf, sizeof(buffer_cdf), "%sCDF", column[i]);

    if (!SDDS_TransferColumnDefinition(&sdds_out, &sdds_in, column[i], NULL) ||
        SDDS_DefineColumn(&sdds_out, buffer_pdf, NULL, buffer_pdf_units, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
        SDDS_DefineColumn(&sdds_out, buffer_cdf, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_WriteLayout(&sdds_out)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  pdf = malloc(sizeof(*pdf) * n_test);
  cdf = malloc(sizeof(*cdf) * n_test);

  while (SDDS_ReadPage(&sdds_in) >= 0) {
    if ((rows = SDDS_CountRowsOfInterest(&sdds_in))) {
      if (!SDDS_StartPage(&sdds_out, n_test)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      for (i = 0; i < columns; i++) {
        snprintf(buffer_pdf, sizeof(buffer_pdf), "%sPDF", column[i]);
        snprintf(buffer_cdf, sizeof(buffer_cdf), "%sCDF", column[i]);

        if (!(column_data = SDDS_GetColumnInDoubles(&sdds_in, column[i]))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }

        find_min_max(&min_temp, &max_temp, column_data, rows);
        gap = max_temp - min_temp;
        lower = min_temp - gap * margin;
        upper = max_temp + gap * margin;

        double *x_array = linearspace(lower, upper, n_test);

        for (j = 0; j < n_test; j++) {
          pdf[j] = kerneldensityestimate(column_data, x_array[j], rows);
          cdf[j] = pdf[j];
        }

        for (j = 1; j < n_test; j++) {
          cdf[j] += cdf[j - 1];
        }

        for (j = 0; j < n_test; j++) {
          cdf[j] = cdf[j] / cdf[n_test - 1];
        }

        if (!SDDS_SetColumnFromDoubles(&sdds_out, SDDS_SET_BY_NAME, pdf, n_test, buffer_pdf) ||
            !SDDS_SetColumnFromDoubles(&sdds_out, SDDS_SET_BY_NAME, cdf, n_test, buffer_cdf) ||
            !SDDS_SetColumnFromDoubles(&sdds_out, SDDS_SET_BY_NAME, x_array, n_test, column[i])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }

        free(column_data);
        column_data = NULL;

        free(x_array);
        x_array = NULL;
      }
    }

    if (!SDDS_WritePage(&sdds_out)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  free(pdf);
  free(cdf);

  if (!SDDS_Terminate(&sdds_in) || !SDDS_Terminate(&sdds_out)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (tmpfile_used && !replaceFileAndBackUp(input_file, output_file)) {
    return EXIT_FAILURE;
  }

  free(column);

  return EXIT_SUCCESS;
}

double bandwidth(double data[], int M) {
  double sigma, min_val, bwidth, interquartile_range;
  double hspread_three, hspread_one;

  gsl_sort(data, 1, M);
  sigma = gsl_stats_sd(data, 1, M);
  hspread_three = gsl_stats_quantile_from_sorted_data(data, 1, M, 0.750);
  hspread_one = gsl_stats_quantile_from_sorted_data(data, 1, M, 0.250);
  interquartile_range = hspread_three - hspread_one;
  min_val = GSL_MIN(interquartile_range / 1.339999, sigma);
  bwidth = 0.90 * min_val * pow(M, -0.20);

  return bwidth;
}

double gaussiankernelfunction(double sample) {
  double k;
  k = exp(-(gsl_pow_2(sample) / 2.0));
  k = k / (M_SQRT2 * sqrt(M_PI));

  return k;
}

double kerneldensityestimate(double *trainingdata, double sample, int64_t n) {
  int64_t i;
  double pdf = 0.0, h, bwidth;

  bwidth = bandwidth(trainingdata, n);
  h = GSL_MAX(bwidth, 2e-6);

  for (i = 0; i < n; i++) {
    pdf += gaussiankernelfunction((trainingdata[i] - sample) / h);
  }

  pdf = pdf / (n * h);

  return pdf;
}

double *linearspace(double start, double end, int N) {
  double *x;
  int i;
  double step;

  x = calloc(N, sizeof(double));
  step = (end - start) / (double)(N - 1);

  x[0] = start;
  for (i = 1; i < N; i++) {
    x[i] = x[i - 1] + step;
  }
  x[N - 1] = end;

  return x;
}
