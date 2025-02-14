/**
 * @file sdds3daverage.c
 * @brief Averages 3D Vorpal output data, filtering and processing Rho and Jz components.
 *
 * @details
 * This program processes 3D Vorpal simulation outputs containing data for Rho and Jz
 * components in a grid layout. It computes averages over z, y, and Jz for each x-coordinate,
 * allowing for filtering of data ranges in x, y, and z dimensions.
 *
 * @subsection KF Key Features:
 * - Reads Rho and Jz data columns for each grid coordinate.
 * - Filters x, y, and z ranges based on user-provided options.
 * - Computes weighted averages of z, y, and Jz components over Rho values.
 * - Outputs processed data to a file with x, ZAve, YAve, and JzAve columns.
 *
 * @section Usage
 * ```
 * sdds3daverage <inputFile> [<outputRoot>]
 *               [-power=<integer>]
 *               [-xfilter=minimum=<value>,maximum=<value>]
 *               [-yfilter=minimum=<value>,maximum=<value>]
 *               [-zfilter=minimum=<value>,maximum=<value>]
 * ```
 *
 * @section Options
 * | Option                             | Description                                           |
 * |------------------------------------|-------------------------------------------------------|
 * | `-power`                 | Set the power for averaging.                         |
 * | `-xfilter`               | Filter x values within the specified range.           |
 * | `-yfilter`               | Filter y values within the specified range.           |
 * | `-zfilter`               | Filter z values within the specified range.           |
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
 * H. Shang, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_POWER,
  SET_XFILTER,
  SET_YFILTER,
  SET_ZFILTER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "power",
  "xfilter",
  "yfilter",
  "zfilter",
};

char *USAGE =
  "Usage: sdds3daverage <inputFile> [<outputRoot>] [OPTIONS]\n"
  "\n"
  "Options:\n"
  "  -power=<integer>                         Set the power for averaging.\n"
  "  -xfilter=minimum=<value>,maximum=<value> Filter x values within the specified range.\n"
  "  -yfilter=minimum=<value>,maximum=<value> Filter y values within the specified range.\n"
  "  -zfilter=minimum=<value>,maximum=<value> Filter z values within the specified range.\n"
  "\n"
  "Example:\n"
  "  sdds3daverage data.sdds outputRoot -power=2 -xfilter=minimum=0.1,maximum=1.0\n"
  "\n"
  "Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

void SetupOutputFile(SDDS_DATASET *SDDS_out, char *output, long zdim);
void free_data_memory(double ***data, long zdim, long ydim);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_orig, SDDS_out;
  int32_t i, j, k, i_arg, rows, xfilter_provided, yfilter_provided, zfilter_provided, row;
  long power = 1;
  SCANNED_ARG *s_arg;
  char *inputFile, *outputRoot, output[1024], tmpcol[256];
  double xmin, xmax, ymin, ymax, zmin, zmax, xinterval, yinterval, zinterval;
  int32_t xdim, ydim, zdim, columnNames, outputColumns, page = 0;
  char **columnName, **outputColumn;
  double ***Rho, ***Jz, rhoSum, rhoSum1, yRho, zRho, jzRho;
  double x_min, x_max, y_min, y_max, z_min, z_max, x, y, z;
  unsigned long dummyFlags = 0;

  x_min = x_max = y_min = y_max = z_min = z_max = 0;
  xfilter_provided = yfilter_provided = zfilter_provided = 0;

  inputFile = outputRoot = NULL;
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "Error: Insufficient arguments.\n\n%s", USAGE);
    exit(EXIT_FAILURE);
  }

  columnNames = outputColumns = 0;
  columnName = outputColumn = NULL;
  Rho = Jz = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_POWER:
        if (s_arg[i_arg].n_items != 2) {
          SDDS_Bomb("Invalid -power syntax.");
        }
        if (!get_long(&power, s_arg[i_arg].list[1])) {
          SDDS_Bomb("Invalid -power value provided.");
        }
        break;

      case SET_XFILTER:
        if (s_arg[i_arg].n_items < 2) {
          SDDS_Bomb("Invalid -xfilter syntax.");
        }
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "minimum", SDDS_DOUBLE, &x_min, 1, 0,
                          "maximum", SDDS_DOUBLE, &x_max, 1, 0,
                          NULL)) {
          SDDS_Bomb("Invalid -xfilter syntax.");
        }
        s_arg[i_arg].n_items++;
        if (x_max <= x_min) {
          fprintf(stderr, "Error: Invalid -xfilter provided, x_max <= x_min.\n");
          exit(EXIT_FAILURE);
        }
        xfilter_provided = 1;
        break;

      case SET_YFILTER:
        if (s_arg[i_arg].n_items < 2) {
          SDDS_Bomb("Invalid -yfilter syntax.");
        }
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "minimum", SDDS_DOUBLE, &y_min, 1, 0,
                          "maximum", SDDS_DOUBLE, &y_max, 1, 0,
                          NULL)) {
          SDDS_Bomb("Invalid -yfilter syntax.");
        }
        s_arg[i_arg].n_items++;
        if (y_max <= y_min) {
          fprintf(stderr, "Error: Invalid -yfilter provided, y_max <= y_min.\n");
          exit(EXIT_FAILURE);
        }
        yfilter_provided = 1;
        break;

      case SET_ZFILTER:
        if (s_arg[i_arg].n_items < 2) {
          SDDS_Bomb("Invalid -zfilter syntax.");
        }
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "minimum", SDDS_DOUBLE, &z_min, 1, 0,
                          "maximum", SDDS_DOUBLE, &z_max, 1, 0,
                          NULL)) {
          SDDS_Bomb("Invalid -zfilter syntax.");
        }
        s_arg[i_arg].n_items++;
        if (z_max <= z_min) {
          fprintf(stderr, "Error: Invalid -zfilter provided, z_max <= z_min.\n");
          exit(EXIT_FAILURE);
        }
        zfilter_provided = 1;
        break;

      default:
        fprintf(stderr, "Error: Unknown option -%s provided.\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    } else {
      if (!inputFile) {
        inputFile = s_arg[i_arg].list[0];
      } else if (!outputRoot) {
        outputRoot = s_arg[i_arg].list[0];
      } else {
        SDDS_Bomb("Error: Too many file names provided.");
      }
    }
  }

  if (!outputRoot) {
    outputRoot = inputFile;
  }

  snprintf(output, sizeof(output), "%s.ave", outputRoot);

  if (!SDDS_InitializeInput(&SDDS_orig, inputFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  xdim = ydim = zdim = 1;
  zmin = zmax = zinterval = 0;
  while (SDDS_ReadPage(&SDDS_orig) > 0) {
    if (page == 0) {
      if (!SDDS_GetParameterAsDouble(&SDDS_orig, "origin1", &xmin) ||
          !SDDS_GetParameterAsDouble(&SDDS_orig, "max_ext1", &xmax) ||
          !SDDS_GetParameterAsDouble(&SDDS_orig, "delta1", &xinterval) ||
          !SDDS_GetParameterAsLong(&SDDS_orig, "numPhysCells1", &xdim) ||
          !SDDS_GetParameterAsDouble(&SDDS_orig, "origin2", &ymin) ||
          !SDDS_GetParameterAsDouble(&SDDS_orig, "max_ext2", &ymax) ||
          !SDDS_GetParameterAsDouble(&SDDS_orig, "delta2", &yinterval) ||
          !SDDS_GetParameterAsLong(&SDDS_orig, "numPhysCells2", &ydim)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      if (SDDS_CheckParameter(&SDDS_orig, "origin3", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK) {
        if (!SDDS_GetParameterAsDouble(&SDDS_orig, "origin3", &zmin) ||
            !SDDS_GetParameterAsDouble(&SDDS_orig, "max_ext3", &zmax) ||
            !SDDS_GetParameterAsDouble(&SDDS_orig, "delta3", &zinterval) ||
            !SDDS_GetParameterAsLong(&SDDS_orig, "numPhysCells3", &zdim)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }

      if (xfilter_provided) {
        if (x_min > xmax || x_max < xmin) {
          fprintf(stderr, "Error: Invalid xfilter provided, it should be between %le and %le.\n", xmin, xmax);
          if (!SDDS_Terminate(&SDDS_orig))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else {
        x_min = xmin;
        x_max = xmax;
      }

      if (yfilter_provided) {
        if (y_min > ymax || y_max < ymin) {
          fprintf(stderr, "Error: Invalid yfilter provided, it should be between %le and %le.\n", ymin, ymax);
          if (!SDDS_Terminate(&SDDS_orig))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else {
        y_min = ymin;
        y_max = ymax;
      }

      if (zfilter_provided && zdim > 1) {
        if (z_min > zmax || z_max < zmin) {
          fprintf(stderr, "Error: Invalid zfilter provided, it should be between %le and %le.\n", zmin, zmax);
          if (!SDDS_Terminate(&SDDS_orig))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else {
        z_min = zmin;
        z_max = zmax;
      }

      Rho = malloc(sizeof(*Rho) * zdim);
      if (!Rho) {
        fprintf(stderr, "Error: Memory allocation failed for Rho.\n");
        exit(EXIT_FAILURE);
      }

      Jz = malloc(sizeof(*Jz) * zdim);
      if (!Jz) {
        free_data_memory(Rho, zdim, ydim);
        fprintf(stderr, "Error: Memory allocation failed for Jz.\n");
        exit(EXIT_FAILURE);
      }

      for (i = 0; i < zdim; i++) {
        Rho[i] = malloc(sizeof(**Rho) * ydim);
        if (!Rho[i]) {
          free_data_memory(Rho, zdim, ydim);
          free_data_memory(Jz, zdim, ydim);
          fprintf(stderr, "Error: Memory allocation failed for Rho[%d].\n", i);
          exit(EXIT_FAILURE);
        }

        Jz[i] = malloc(sizeof(**Jz) * ydim);
        if (!Jz[i]) {
          free_data_memory(Rho, zdim, ydim);
          free_data_memory(Jz, zdim, ydim);
          fprintf(stderr, "Error: Memory allocation failed for Jz[%d].\n", i);
          exit(EXIT_FAILURE);
        }
      }

      SetupOutputFile(&SDDS_out, output, zdim);
    }

    rows = SDDS_CountRowsOfInterest(&SDDS_orig);
    if (rows != xdim) {
      fprintf(stderr, "Error: Row number does not equal xdim size.\n");
      exit(EXIT_FAILURE);
    }

    for (j = 1; j <= ydim; j++) {
      snprintf(tmpcol, sizeof(tmpcol), "Rho_%d", j);
      Rho[page][j - 1] = NULL;
      if (!(Rho[page][j - 1] = SDDS_GetColumnInDoubles(&SDDS_orig, tmpcol))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      snprintf(tmpcol, sizeof(tmpcol), "Jz_%d", j);
      Jz[page][j - 1] = NULL;
      if (!(Jz[page][j - 1] = SDDS_GetColumnInDoubles(&SDDS_orig, tmpcol))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    page++;
  }

  if (!SDDS_Terminate(&SDDS_orig)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (page != zdim) {
    free_data_memory(Rho, zdim, ydim);
    free_data_memory(Jz, zdim, ydim);
    fprintf(stderr, "Error: The page number does not equal the zdim size.\n");
    exit(EXIT_FAILURE);
  }

  if (!SDDS_StartPage(&SDDS_out, xdim) ||
      !SDDS_SetParameters(&SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "origin1", xmin,
                          "origin2", ymin,
                          "origin3", zmin,
                          "max_ext1", xmax,
                          "max_ext2", ymax,
                          "delta1", xinterval,
                          "delta2", yinterval,
                          "numPhysCells1", xdim,
                          "numPhysCells2", ydim,
                          "xstart", x_min,
                          "xend", x_max,
                          "ystart", y_min,
                          "yend", y_max,
                          NULL)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (zdim > 1) {
    if (!SDDS_SetParameters(&SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "origin3", zmin,
                            "max_ext3", zmax,
                            "delta3", zinterval,
                            "numPhysCells3", zdim,
                            "zstart", z_min,
                            "zend", z_max,
                            NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  row = 0;
  for (i = 0; i < xdim; i++) {
    rhoSum = rhoSum1 = 0;
    yRho = zRho = jzRho = 0;
    x = i * xinterval + xmin;

    if (xfilter_provided && (x < x_min || x > x_max)) {
      continue;
    }

    for (j = 0; j < ydim; j++) {
      y = j * yinterval + ymin;

      if (yfilter_provided && (y < y_min || y > y_max)) {
        continue;
      }

      for (k = 0; k < zdim; k++) {
        z = k * zinterval + zmin;

        if (zfilter_provided && zdim > 1 && (z < z_min || z > z_max)) {
          continue;
        }

        if (power == 1) {
          yRho += fabs(Rho[k][j][i]) * y;
          zRho += fabs(Rho[k][j][i]) * z;
          jzRho += Rho[k][j][i] * Jz[k][j][i];
          rhoSum += fabs(Rho[k][j][i]);
          rhoSum1 += Rho[k][j][i];
        } else {
          yRho += pow(fabs(Rho[k][j][i]), power) * y;
          zRho += pow(fabs(Rho[k][j][i]), power) * z;
          jzRho += pow(Rho[k][j][i] * Jz[k][j][i], power);
          rhoSum += pow(fabs(Rho[k][j][i]), power);
          rhoSum1 += pow(Rho[k][j][i], power);
        }
      }
    }

    /* Set row values */
    if (!SDDS_SetRowValues(&SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row,
                           "x", x,
                           "YAve", yRho / (rhoSum + 1.0e-20),
                           "JzAve", jzRho / (rhoSum1 + 1.0e-20),
                           NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (zdim > 1) {
      if (!SDDS_SetRowValues(&SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row,
                             "ZAve", zRho / (rhoSum + 1.0e-20),
                             NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    row++;
  }

  if (!SDDS_WritePage(&SDDS_out) || !SDDS_Terminate(&SDDS_out)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  free_data_memory(Rho, zdim, ydim);
  free_data_memory(Jz, zdim, ydim);

  return EXIT_SUCCESS;
}

void free_data_memory(double ***data, long zdim, long ydim) {
  long i, j;
  for (i = 0; i < zdim; i++) {
    for (j = 0; j < ydim; j++) {
      free(data[i][j]);
    }
    free(data[i]);
  }
  free(data);
}

void SetupOutputFile(SDDS_DATASET *SDDS_out, char *output, long zdim) {
  if (!SDDS_InitializeOutput(SDDS_out, SDDS_BINARY, 0, NULL, NULL, output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_DefineSimpleParameter(SDDS_out, "origin1", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "origin2", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "max_ext1", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "max_ext2", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "delta1", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "delta2", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "numPhysCells1", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "numPhysCells2", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "xstart", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "xend", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "ystart", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "yend", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleColumn(SDDS_out, "x", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleColumn(SDDS_out, "YAve", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleColumn(SDDS_out, "JzAve", NULL, SDDS_DOUBLE)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (zdim > 1) {
    if (!SDDS_DefineSimpleParameter(SDDS_out, "origin3", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(SDDS_out, "max_ext3", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(SDDS_out, "delta3", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(SDDS_out, "numPhysCells3", NULL, SDDS_LONG) ||
        !SDDS_DefineSimpleParameter(SDDS_out, "zstart", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(SDDS_out, "zend", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(SDDS_out, "ZAve", NULL, SDDS_DOUBLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_WriteLayout(SDDS_out)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}
