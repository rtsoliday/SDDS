/**
 * @file sddsunwrap.c
 * @brief Identifies and corrects phase discontinuities in datasets using the SDDS library.
 *
 * @details
 * This program processes datasets to identify and correct phase discontinuities. It allows 
 * users to specify thresholds for detecting discontinuities and applies modular corrections.
 * The input/output files must follow the SDDS format.
 *
 * @section Usage
 * ```
 * sddsunwrap [<input>] [<output>] 
 *            [-pipe=[input][,output]]
 *            [-column=<list>]
 *            [-threshold=<value>]
 *            [-modulo=<value>]
 *            [-majorOrder=<row|column>]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Enables piping for input/output.                                                     |
 * | `-column`                             | Specifies the columns to be unwrapped. Accepts a comma-separated list and wildcards.  |
 * | `-threshold`                          | Sets the discontinuity threshold (default: PI).                                      |
 * | `-modulo`                             | Sets the modulo value for unwrapping (default: 2*PI).                                |
 * | `-majorOrder`                         | Specifies data order (row-major or column-major).                                    |
 *
 * @subsection spec_req Specific Requirements
 *   - If `-column` is not specified, the program defaults to numeric columns.
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
#include "SDDSutils.h"
#include "scan.h"
#include <ctype.h>

typedef enum {
  OPTION_PIPE,
  OPTION_COLUMN,
  OPTION_THRESHOLD,
  OPTION_MAJOR_ORDER,
  OPTION_MODULO,
  N_OPTIONS
} OptionType;

static char *USAGE = "\n"
 "sddsunwrap [<input>] [<output>]\n"
 "            [-pipe=[input][,output]]\n"
 "            [-column=<list>]\n"
 "            [-threshold=<value>]\n"
 "            [-modulo=<value>]\n"
 "            [-majorOrder=<row|column>]\n"
"Options:\n\
  -pipe=[input][,output]   Use pipes for input/output.\n\
  -column=list             Specify columns to be unwrapped, separated by commas.\n\
                           Accepts wildcards.\n\
  -threshold=<value>       Set the discontinuity threshold to identify a wrap.\n\
                           Default: PI.\n\
  -modulo=<value>          Set the value used to unwrap the data.\n\
                           Default: 2*PI.\n\
  -majorOrder=<row|column> Specify the data order (row-major or column-major).\n\
Description:\n\
  sddsunwrap identifies discontinuities greater than the threshold in a set of data\n\
  and adds the appropriate multiple of the modulo to the data set.\n\
Program by Hairong Shang. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")";

static char *option[N_OPTIONS] = {
  "pipe",
  "column",
  "threshold",
  "majorOrder",
  "modulo",
};

int main(int argc, char **argv) {
  double *x_data, *y_data, threshold, modulo = 0;
  char *input = NULL, *output = NULL, **column_match = NULL, **column_name = NULL, **column;
  long i, i_arg;
  char output_column[256];

  SDDS_DATASET sdds_in, sdds_out;
  SCANNED_ARG *scanned;
  unsigned long pipe_flags = 0, major_order_flag = 0;
  int32_t column_type, columns0 = 0;
  int64_t j, k, rows;
  int32_t columns = 0, column_matches = 0;
  short column_major_order = -1, phase = 1, threshold_provided = 0;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scanned, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  input = output = NULL;
  x_data = y_data = NULL;
  column_name = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* process options here */
      OptionType option_type =
        (OptionType)match_string(scanned[i_arg].list[0], option,
                                 N_OPTIONS, 0);
      switch (option_type) {
      case OPTION_MAJOR_ORDER:
        major_order_flag = 0;
        scanned[i_arg].n_items--;
        if (scanned[i_arg].n_items > 0 && (!scanItemList(&major_order_flag, scanned[i_arg].list + 1,
                                                         &scanned[i_arg].n_items, 0, "row", -1,
                                                         NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1,
                                                         NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (major_order_flag & SDDS_COLUMN_MAJOR_ORDER)
          column_major_order = 1;
        else if (major_order_flag & SDDS_ROW_MAJOR_ORDER)
          column_major_order = 0;
        break;
      case OPTION_THRESHOLD:
        if (scanned[i_arg].n_items != 2)
          SDDS_Bomb("invalid -threshold syntax");
        if (!get_double(&threshold, scanned[i_arg].list[1]))
          SDDS_Bomb("invalid -threshold value given");
        threshold_provided = 1;
        break;
      case OPTION_MODULO:
        if (scanned[i_arg].n_items != 2)
          SDDS_Bomb("invalid -modulo syntax");
        if (!get_double(&modulo, scanned[i_arg].list[1]))
          SDDS_Bomb("invalid -modulo value given");
        break;
      case OPTION_COLUMN:
        if ((scanned[i_arg].n_items < 2))
          SDDS_Bomb("invalid -column syntax");
        column_matches = scanned[i_arg].n_items - 1;
        column_match = tmalloc(sizeof(*column_match) * column_matches);
        for (i = 0; i < column_matches; i++)
          column_match[i] = scanned[i_arg].list[i + 1];
        break;
      case OPTION_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1,
                               scanned[i_arg].n_items - 1,
                               &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      default:
        fprintf(stderr, "Unknown option %s provided\n",
                scanned[i_arg].list[0]);
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
  processFilenames("sddsunwrap", &input, &output, pipe_flags, 0, NULL);
  if (phase && !threshold_provided)
    threshold = PI;

  if (!modulo)
    modulo = 2 * threshold;

  if (!SDDS_InitializeInput(&sdds_in, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (column_matches) {
    column = getMatchingSDDSNames(&sdds_in, column_match, column_matches,
                                  &columns, SDDS_MATCH_COLUMN);
  } else {
    column = tmalloc(sizeof(*column) * 1);
    if (!(column_name = (char **)SDDS_GetColumnNames(&sdds_in, &columns0)))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < columns0; i++) {
      if (SDDS_NUMERIC_TYPE(column_type =
                            SDDS_GetColumnType(&sdds_in, i))) {
        SDDS_CopyString(&column[0], column_name[i]);
        break;
      }
    }
    SDDS_FreeStringArray(column_name, columns0);
    columns = 1;
  }

  if (!SDDS_InitializeCopy(&sdds_out, &sdds_in, output, "w"))
    SDDS_PrintErrors(stderr,
                     SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (column_major_order != -1)
    sdds_out.layout.data_mode.column_major = column_major_order;
  else
    sdds_out.layout.data_mode.column_major = sdds_in.layout.data_mode.column_major;

  for (i = 0; i < columns; i++) {
    sprintf(output_column, "Unwrap%s", column[i]);
    if (!SDDS_TransferColumnDefinition(&sdds_out, &sdds_in, column[i],
                                       output_column))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_WriteLayout(&sdds_out))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  while (SDDS_ReadPage(&sdds_in) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&sdds_in)) <= 0)
      continue;
    if (!SDDS_StartPage(&sdds_out, rows) || !SDDS_CopyPage(&sdds_out, &sdds_in))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    y_data = tmalloc(sizeof(*y_data) * rows);
    for (i = 0; i < columns; i++) {
      sprintf(output_column, "Unwrap%s", column[i]);
      if (!(x_data = SDDS_GetColumnInDoubles(&sdds_in, column[i])))
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      k = 0;
      for (j = 0; j < rows - 1; j++) {
        y_data[j] = x_data[j] + modulo * k; /*add 2*Pi*k to input */
        /* check if diff is greater than alpha */
        if (fabs(x_data[j + 1] - x_data[j]) > fabs(threshold)) {
          if (x_data[j + 1] < x_data[j])
            k = k + 1; /* if the pahse jump is negative, increment k */
          else
            k = k - 1; /* if the phase jump is positive, decrement k */
        }
      }
      y_data[rows - 1] = x_data[rows - 1] + modulo * k; /*  add 2*pi*k to the last element of the input */

      if (x_data)
        free(x_data);
      x_data = NULL;

      if (!SDDS_SetColumnFromDoubles(&sdds_out, SDDS_BY_NAME, y_data,
                                     rows, output_column))
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    free(y_data);
    y_data = NULL;
    if (!SDDS_WritePage(&sdds_out))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&sdds_in) || !SDDS_Terminate(&sdds_out)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (column_name) {
    SDDS_FreeStringArray(column_name, columns0);
    free(column_name);
  }
  SDDS_FreeStringArray(column, columns);
  free(column);
  if (column_match)
    free(column_match);
  free_scanargs(&scanned, argc);
  return 0;
}
