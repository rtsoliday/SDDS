/**
 * @file sdds3dconvert.c
 * @brief Converts 3D Vorpal output data into SDDS format compatible with sddscontour for plotting.
 *
 * @details
 * This program converts 3D Vorpal output data into a format compatible with `sddscontour`. 
 * It supports processing variables such as Ex, Ey, Ez, Rho, Jx, Jy, Jz, etc., contained 
 * in 3D data files with specific x, y, and z coordinates. The program generates output 
 * that can be plotted for yz and xz planes.
 *
 * @section Usage
 * ```
 * sdds3dconvert <inputFile> [<outputRoot>]
 * ```
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
 * H. Shang, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#define USAGE_MESSAGE "Usage: sdds3dconvert <inputFile> [<outputRoot>]\n" \
  "Converts 3D Vorpal output data into SDDS format compatible with sddscontour.\n\n" \
  "Arguments:\n"                                                        \
  "  <inputFile>   Path to the input SDDS file containing 3D Vorpal data.\n" \
  "  <outputRoot>  (Optional) Root name for the output files. Defaults to <inputFile> name if not provided.\n\n" \
  "Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n"

static void setup_output_file(SDDS_DATASET *sdds_out, const char *output, long yz, long output_columns, char **output_column);
static void free_data_memory(double ****data, long output_columns, long pages, long ydim);

int main(int argc, char **argv) {
  SDDS_DATASET sdds_orig, sdds_out1, sdds_out2;
  int32_t i, j, k, row, str_len;
  SCANNED_ARG *s_arg;
  char *input_file, *output_root, output1[1024], output2[1024], tmp_col[256];
  double xmin, xmax, ymin, ymax, zmin, zmax, x_interval, y_interval, z_interval;
  int32_t x_dim, y_dim, z_dim, column_names, output_columns, page = 0;
  char **column_name, **output_column;
  double ****data;

  input_file = output_root = NULL;
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);

  if (argc < 2) {
    fprintf(stderr, "%s", USAGE_MESSAGE);
    exit(EXIT_FAILURE);
  }

  column_names = output_columns = 0;
  column_name = output_column = NULL;
  input_file = s_arg[1].list[0];
  output_root = (argc == 3) ? s_arg[2].list[0] : input_file;

  snprintf(output1, sizeof(output1), "%s.yz", output_root);
  snprintf(output2, sizeof(output2), "%s.xz", output_root);

  data = NULL;
  if (!SDDS_InitializeInput(&sdds_orig, input_file)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  column_name = (char **)SDDS_GetColumnNames(&sdds_orig, &column_names);
  for (i = 0; i < column_names; i++) {
    if (wild_match(column_name[i], "*_1")) {
      output_column = SDDS_Realloc(output_column, (output_columns + 1) * sizeof(*output_column));
      if (!output_column) {
        fprintf(stderr, "Memory allocation failed for output_column.\n");
        exit(EXIT_FAILURE);
      }
      str_len = strlen(column_name[i]);
      output_column[output_columns] = malloc(str_len - 1); // Allocate enough space
      if (!output_column[output_columns]) {
        fprintf(stderr, "Memory allocation failed for output_column[%d].\n", output_columns);
        exit(EXIT_FAILURE);
      }
      strncpy(output_column[output_columns], column_name[i], str_len - 2);
      output_column[output_columns][str_len - 2] = '\0';
      output_columns++;
    }
  }

  data = malloc(sizeof(*data) * output_columns);
  if (!data) {
    fprintf(stderr, "Memory allocation failed for data.\n");
    exit(EXIT_FAILURE);
  }

  while (SDDS_ReadPage(&sdds_orig) > 0) {
    if (page == 0) {
      if (!SDDS_GetParameterAsDouble(&sdds_orig, "origin1", &xmin) ||
          !SDDS_GetParameterAsDouble(&sdds_orig, "max_ext1", &xmax) ||
          !SDDS_GetParameterAsDouble(&sdds_orig, "delta1", &x_interval) ||
          !SDDS_GetParameterAsLong(&sdds_orig, "numPhysCells1", &x_dim) ||
          !SDDS_GetParameterAsDouble(&sdds_orig, "origin2", &ymin) ||
          !SDDS_GetParameterAsDouble(&sdds_orig, "max_ext2", &ymax) ||
          !SDDS_GetParameterAsDouble(&sdds_orig, "delta2", &y_interval) ||
          !SDDS_GetParameterAsLong(&sdds_orig, "numPhysCells2", &y_dim) ||
          !SDDS_GetParameterAsDouble(&sdds_orig, "origin3", &zmin) ||
          !SDDS_GetParameterAsDouble(&sdds_orig, "max_ext3", &zmax) ||
          !SDDS_GetParameterAsDouble(&sdds_orig, "delta3", &z_interval) ||
          !SDDS_GetParameterAsLong(&sdds_orig, "numPhysCells3", &z_dim)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      setup_output_file(&sdds_out1, output1, 1, output_columns, output_column);
      setup_output_file(&sdds_out2, output2, 0, output_columns, output_column);

      for (i = 0; i < output_columns; i++) {
        data[i] = malloc(z_dim * sizeof(**data));
        if (!data[i]) {
          fprintf(stderr, "Memory allocation failed for data[%d].\n", i);
          free_data_memory(data, output_columns, z_dim, y_dim);
          exit(EXIT_FAILURE);
        }
        for (j = 0; j < z_dim; j++) {
          data[i][j] = malloc(y_dim * sizeof(***data));
          if (!data[i][j]) {
            fprintf(stderr, "Memory allocation failed for data[%d][%d].\n", i, j);
            free_data_memory(data, output_columns, z_dim, y_dim);
            exit(EXIT_FAILURE);
          }
        }
      }
    }

    for (i = 0; i < output_columns; i++) {
      for (j = 1; j <= y_dim; j++) {
        snprintf(tmp_col, sizeof(tmp_col), "%s_%d", output_column[i], j);
        data[i][page][j - 1] = SDDS_GetColumnInDoubles(&sdds_orig, tmp_col);
        if (!data[i][page][j - 1]) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          free_data_memory(data, output_columns, z_dim, y_dim);
          exit(EXIT_FAILURE);
        }
      }
    }
    page++;
  }

  if (!SDDS_Terminate(&sdds_orig)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    free_data_memory(data, output_columns, z_dim, y_dim);
    exit(EXIT_FAILURE);
  }

  if (page != z_dim) {
    free_data_memory(data, output_columns, z_dim, y_dim);
    fprintf(stderr, "Error: Page count (%d) does not match z-dimension size (%d).\n", page, z_dim);
    exit(EXIT_FAILURE);
  }

  /* Write yz output */
  for (page = 0; page < x_dim; page++) {
    if (!SDDS_StartPage(&sdds_out1, y_dim * z_dim) ||
        !SDDS_SetParameters(&sdds_out1, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "origin1", xmin, "origin2", ymin, "origin3", zmin,
                            "max_ext1", xmax, "max_ext2", ymax, "max_ext3", zmax,
                            "delta1", x_interval, "delta2", y_interval, "delta3", z_interval,
                            "numPhysCells1", x_dim, "numPhysCells2", y_dim, "numPhysCells3", z_dim,
                            "Variable1Name", "Y", "Variable2Name", "Z",
                            "ZMinimum", zmin, "ZMaximum", zmax, "ZInterval", z_interval, "ZDimension", z_dim,
                            "YMinimum", ymin, "YMaximum", ymax, "YInterval", y_interval, "YDimension", y_dim,
                            NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      free_data_memory(data, output_columns, z_dim, y_dim);
      exit(EXIT_FAILURE);
    }

    for (i = 0; i < output_columns; i++) {
      row = 0;
      for (j = 0; j < y_dim; j++) {
        for (k = 0; k < z_dim; k++) {
          if (!SDDS_SetRowValues(&sdds_out1, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row,
                                 output_column[i], data[i][k][j][page],
                                 "z", k * z_interval + zmin,
                                 "y", j * y_interval + ymin, NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            free_data_memory(data, output_columns, z_dim, y_dim);
            exit(EXIT_FAILURE);
          }
          row++;
        }
      }
    }

    if (!SDDS_WritePage(&sdds_out1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      free_data_memory(data, output_columns, z_dim, y_dim);
      exit(EXIT_FAILURE);
    }
  }

  if (!SDDS_Terminate(&sdds_out1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    free_data_memory(data, output_columns, z_dim, y_dim);
    exit(EXIT_FAILURE);
  }

  /* Write xz output */
  for (page = 0; page < y_dim; page++) {
    if (!SDDS_StartPage(&sdds_out2, x_dim * z_dim) ||
        !SDDS_SetParameters(&sdds_out2, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "origin1", xmin, "origin2", ymin, "origin3", zmin,
                            "max_ext1", xmax, "max_ext2", ymax, "max_ext3", zmax,
                            "delta1", x_interval, "delta2", y_interval, "delta3", z_interval,
                            "numPhysCells1", x_dim, "numPhysCells2", y_dim, "numPhysCells3", z_dim,
                            "Variable1Name", "X", "Variable2Name", "Z",
                            "ZMinimum", zmin, "ZMaximum", zmax, "ZInterval", z_interval, "ZDimension", z_dim,
                            "XMinimum", xmin, "XMaximum", xmax, "XInterval", x_interval, "XDimension", x_dim,
                            NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      free_data_memory(data, output_columns, z_dim, y_dim);
      exit(EXIT_FAILURE);
    }

    for (i = 0; i < output_columns; i++) {
      row = 0;
      for (j = 0; j < x_dim; j++) {
        for (k = 0; k < z_dim; k++) {
          if (!SDDS_SetRowValues(&sdds_out2, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row,
                                 output_column[i], data[i][k][page][j],
                                 "z", k * z_interval + zmin,
                                 "x", j * x_interval + xmin, NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            free_data_memory(data, output_columns, z_dim, y_dim);
            exit(EXIT_FAILURE);
          }
          row++;
        }
      }
    }

    if (!SDDS_WritePage(&sdds_out2)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      free_data_memory(data, output_columns, z_dim, y_dim);
      exit(EXIT_FAILURE);
    }
  }

  free_data_memory(data, output_columns, z_dim, y_dim);

  if (!SDDS_Terminate(&sdds_out2)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}

static void free_data_memory(double ****data, long output_columns, long z_dim, long y_dim) {
  long i, j, k;
  if (!data)
    return;
  for (i = 0; i < output_columns; i++) {
    if (!data[i])
      continue;
    for (j = 0; j < z_dim; j++) {
      if (!data[i][j])
        continue;
      for (k = 0; k < y_dim; k++) {
        free(data[i][j][k]);
      }
      free(data[i][j]);
    }
    free(data[i]);
  }
  free(data);
}

static void setup_output_file(SDDS_DATASET *sdds_out, const char *output, long yz, long output_columns, char **output_column) {
  long i;
  if (!SDDS_InitializeOutput(sdds_out, SDDS_BINARY, 0, NULL, NULL, output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_DefineSimpleParameter(sdds_out, "origin1", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "origin2", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "origin3", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "max_ext1", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "max_ext2", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "max_ext3", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "delta1", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "delta2", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "delta3", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "numPhysCells1", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(sdds_out, "numPhysCells2", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(sdds_out, "numPhysCells3", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(sdds_out, "Variable1Name", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(sdds_out, "Variable2Name", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(sdds_out, "ZMinimum", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "ZMaximum", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "ZInterval", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(sdds_out, "ZDimension", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleColumn(sdds_out, "z", NULL, SDDS_DOUBLE)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (yz) {
    if (!SDDS_DefineSimpleParameter(sdds_out, "YMinimum", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(sdds_out, "YMaximum", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(sdds_out, "YInterval", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(sdds_out, "YDimension", NULL, SDDS_LONG) ||
        !SDDS_DefineSimpleColumn(sdds_out, "y", NULL, SDDS_DOUBLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  } else {
    if (!SDDS_DefineSimpleParameter(sdds_out, "XMinimum", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(sdds_out, "XMaximum", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(sdds_out, "XInterval", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(sdds_out, "XDimension", NULL, SDDS_LONG) ||
        !SDDS_DefineSimpleColumn(sdds_out, "x", NULL, SDDS_DOUBLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  for (i = 0; i < output_columns; i++) {
    if (!SDDS_DefineSimpleColumn(sdds_out, output_column[i], NULL, SDDS_DOUBLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_WriteLayout(sdds_out)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}
