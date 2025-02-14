/**
 * @file sddstdrpeeling.c
 * @brief Recursive TDR Impedance Peeling Algorithm Implementation.
 *
 * @details
 * This file implements a program to process Time Domain Reflectometry (TDR) data
 * using a recursive peeling algorithm. The algorithm calculates the impedance profile
 * of a non-uniform transmission line based on input voltage and the characteristic
 * line impedance. The program leverages the SDDS (Self Describing Data Sets) library
 * for input and output data handling, supporting flexible data processing options.
 *
 * @section Usage
 * ```
 * sddstdrpeeling [<input>] [<output>]
 *                [-pipe=[input][,output]]
 *                -col=<data-column>
 *                [-inputVoltage=<value|@<parameter>]]
 *                [-z0=<value>]
 *                [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-col`                                | Specifies the data column name in the SDDS file.                                      |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Enables piping for input and/or output streams.                                       |
 * | `-inputVoltage`                       | Sets the input voltage directly or references a parameter for its value.              |
 * | `-z0`                                 | Defines the characteristic line impedance (default is 50 ohms).                       |
 * | `-majorOrder`                         | Specifies the major order of data processing (default is based on input file).        |
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
#include "SDDSutils.h"
#include "scan.h"
#include <ctype.h>

static char *USAGE =
  "sddstdrpeeling [<input>] [<output>]\n"
  "    [-pipe=[input][,output]]\n"
  "    -col=<data-column>\n"
  "    [-inputVoltage=<value|@<parameter>]]\n"
  "    [-z0=<value>]\n\n"
  "Options:\n"
  "  -column           Provide the data column name.\n"
  "  -inputVoltage     Specify the input voltage in volts for TDR (Time Domain Reflectometry).\n"
  "  -z0               Set the line impedance (default is 50 ohms).\n\n"
  "Description:\n"
  "  sddstdrpeeling processes TDR data using a recursive algorithm to determine the impedance of a nonuniform transmission line.\n\n"
  "Program Information:\n"
  "  Program by Hairong Shang. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")";

/* Enumeration for option types */
enum option_type {
  CLO_PIPE,
  CLO_COLUMN,
  CLO_INPUT_VOLTAGE,
  CLO_Z0,
  CLO_MAJOR_ORDER,
  N_OPTIONS
} option_type;

static char *option[N_OPTIONS] = {
  "pipe",
  "column",
  "inputVoltage",
  "z0",
  "majorOrder"};

int main(int argc, char **argv) {
  double *meas_data = NULL, input_voltage = 0.2, z0 = 50;
  char *input = NULL, *output = NULL, *data_column = NULL, *input_vol_param = NULL;
  long i_arg, k;
  double *left = NULL, *right = NULL, *gamma = NULL, *zline = NULL;
  double g_product, vr_temp, d_increase;

  SDDS_DATASET sdds_in, sdds_out;
  SCANNED_ARG *scanned = NULL;
  unsigned long pipe_flags = 0, major_order_flag = 0;
  int64_t i, rows;
  short column_major_order = -1;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scanned, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, &pipe_flags)) {
          SDDS_Bomb("Invalid -pipe syntax");
        }
        break;
      case CLO_MAJOR_ORDER:
        major_order_flag = 0;
        scanned[i_arg].n_items--;
        if (scanned[i_arg].n_items > 0 &&
            (!scanItemList(&major_order_flag, scanned[i_arg].list + 1, &scanned[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("invalid -majorOrder syntax/values");
        }
        if (major_order_flag & SDDS_COLUMN_MAJOR_ORDER)
          column_major_order = 1;
        else if (major_order_flag & SDDS_ROW_MAJOR_ORDER)
          column_major_order = 0;
        break;
      case CLO_INPUT_VOLTAGE:
        if (scanned[i_arg].n_items != 2)
          SDDS_Bomb("invalid -inputVoltage syntax");
        if (scanned[i_arg].list[1][0] == '@') {
          cp_str(&input_vol_param, scanned[i_arg].list[1] + 1);
        } else {
          if (!get_double(&input_voltage, scanned[i_arg].list[1]))
            SDDS_Bomb("invalid -threshold value given");
        }
        break;
      case CLO_COLUMN:
        if (scanned[i_arg].n_items != 2)
          SDDS_Bomb("invalid -column syntax");
        data_column = scanned[i_arg].list[1];
        break;
      case CLO_Z0:
        if (scanned[i_arg].n_items != 2)
          SDDS_Bomb("invalid -z0 syntax");
        if (!get_double(&z0, scanned[i_arg].list[1]))
          SDDS_Bomb("invalid -z0 value given");
        break;
      default:
        fprintf(stderr, "Unknown option %s provided\n", scanned[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  processFilenames("sddstdrpeeling", &input, &output, pipe_flags, 0, NULL);

  if (!SDDS_InitializeInput(&sdds_in, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_InitializeCopy(&sdds_out, &sdds_in, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (column_major_order != -1)
    sdds_out.layout.data_mode.column_major = column_major_order;
  else
    sdds_out.layout.data_mode.column_major = sdds_in.layout.data_mode.column_major;

  if (!SDDS_DefineSimpleColumn(&sdds_out, "PeeledImpedance", "oms", SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_WriteLayout(&sdds_out))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  while (SDDS_ReadPage(&sdds_in) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&sdds_in)) <= 0)
      continue;
    if (!SDDS_StartPage(&sdds_out, rows) || !SDDS_CopyPage(&sdds_out, &sdds_in))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (input_vol_param) {
      if (!SDDS_GetParameterAsDouble(&sdds_in, input_vol_param, &input_voltage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!(meas_data = SDDS_GetColumnInDoubles(&sdds_in, data_column)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    for (i = 0; i < rows; i++)
      meas_data[i] /= input_voltage;

    left = calloc(sizeof(*left), rows + 1);
    right = calloc(sizeof(*right), rows + 1);
    gamma = calloc(sizeof(*gamma), rows + 1);
    zline = calloc(sizeof(*zline), rows + 1);
    g_product = 1;

    if (rows >= 1) {
      gamma[1] = meas_data[0];
      left[1] = 1;
      g_product *= (1 - gamma[1] * gamma[1]);
    }
    if (rows >= 2) {
      vr_temp = (1 - gamma[1]) * right[1] + gamma[1] * left[1];
      gamma[2] = (meas_data[1] - vr_temp) / g_product;
      left[2] = left[1] * (1 + gamma[1]);
      right[1] = left[2] * gamma[2];
      g_product *= (1 - gamma[2] * gamma[2]);
    }
    for (i = 3; i <= rows; i++) {
      left[i] = left[i - 1] * (1 + gamma[i - 1]);
      left[i - 1] = left[i - 2] * (1 + gamma[i - 2]) - right[i - 2] * gamma[i - 2];
      for (k = i - 2; k > 2; k--) {
        right[k] = gamma[k + 1] * left[k + 1] + (1 - gamma[k + 1] * right[k + 1]);
        left[k] = (1 + gamma[k - 1]) * left[k - 1] - gamma[k - 1] * right[k - 1];
      }
      right[1] = left[2] * gamma[2] + right[2] * (1 - gamma[2]);
      vr_temp = (1 - gamma[1]) * right[1] + gamma[1] * left[1];
      gamma[i] = (meas_data[i - 1] - vr_temp) / g_product;
      g_product *= (1 - gamma[i] * gamma[i]);
      d_increase = left[i] * gamma[i];
      right[i - 1] += d_increase;
      for (k = i - 2; k > 1; k--) {
        d_increase *= (1 - gamma[k + 1]);
        right[k] += d_increase;
      }
    }
    zline[0] = z0;
    for (i = 1; i <= rows; i++) {
      zline[i] = (1 + gamma[i]) / (1 - gamma[i]) * zline[i - 1];
    }
    if (!SDDS_SetColumnFromDoubles(&sdds_out, SDDS_BY_NAME, zline + 1, rows, "PeeledImpedance") ||
        !SDDS_WritePage(&sdds_out)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    free(left);
    free(right);
    free(gamma);
    free(zline);
    free(meas_data);
    meas_data = NULL;
  }

  if (!SDDS_Terminate(&sdds_in) || !SDDS_Terminate(&sdds_out)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  free_scanargs(&scanned, argc);
  return 0;
}
