/**
 * @file sddsexpand.c
 * @brief Converts SDDS column data into parameters in a new SDDS file.
 *
 * @details
 * This program reads an SDDS (Self Describing Data Set) file and creates a new SDDS file where
 * columns in the input file are converted to parameters. Each page in the output file corresponds
 * to a row in the input file. This functionality is effectively the inverse of the sddscollapse
 * program. The program supports various options including piping, warning suppression, and setting
 * the major order for data storage.
 *
 * @section Usage
 * ```
 * sddsexpand [<SDDSinputfile>] [<SDDSoutputfile>]
 *            [-pipe=[input][,output]] 
 *            [-noWarnings] 
 *            [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Option                                | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use piping for input and/or output.                                                   |
 * | `-noWarnings`                         | Suppress warnings during processing.                                                  |
 * | `-majorOrder`                         | Specify the major order for data storage (`row` or `column`).                         |
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
 * M. Borland, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"

/* Enum for command-line options */
typedef enum {
  SET_PIPE,        /* Option for piping input and output */
  SET_NOWARNINGS,  /* Option to suppress warnings */
  SET_MAJOR_ORDER, /* Option to set row/column major order */
  N_OPTIONS        /* Number of options */
} OptionType;

/* Array of command-line option strings */
static char *option[N_OPTIONS] = {
  "pipe",
  "nowarnings",
  "majorOrder",
};

/* Program usage message */
static char *USAGE =
  "sddsexpand [<SDDSinputfile>] [<SDDSoutputfile>]\n"
  "            [-pipe=[input][,output]]\n"
  "            [-noWarnings]\n"
  "            [-majorOrder=row|column]\n\n"
  "sddsexpand is the partial inverse of sddscollapse.\n"
  "All columns of the input file are turned into parameters in the output file.\n"
  "For each row of each page in the input file, sddsexpand emits a new page\n"
  "with parameter values equal to the column values for that page and row.\n\n"
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define ROW_INCREMENT 100 /* Increment size for row allocation */

/* Structure to represent source data (columns or parameters) */
typedef struct {
  char *name;    /* Name of the data source */
  long size;     /* Size of the data type */
  long index;    /* Index in the target data set */
  short do_copy; /* Flag to indicate if this data should be copied */
} SourceData;

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_input, SDDS_output;
  char *inputfile = NULL, *outputfile = NULL;
  int64_t rows, irow;
  long i, no_warnings = 0;
  SCANNED_ARG *s_arg;
  unsigned long pipe_flags = 0, major_order_flag;
  void **data = NULL;
  SourceData *column_source = NULL, *parameter_source = NULL;
  int32_t column_sources, parameter_sources;
  char **name;
  char buffer[32];
  short column_major_order = -1;

  /* Register the program name for error reporting */
  SDDS_RegisterProgramName(argv[0]);

  /* Parse command-line arguments */
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);

  /* Process each argument */
  for (int i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      /* Match the argument to known options */
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        major_order_flag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&major_order_flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                                                       "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                                                       "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("invalid -majorOrder syntax/values");
        }
        column_major_order = (major_order_flag & SDDS_COLUMN_MAJOR_ORDER) ? 1 : 0;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_NOWARNINGS:
        no_warnings = 1;
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    } else {
      /* Handle positional arguments */
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  /* Handle filenames and initialize input and output datasets */
  processFilenames("sddsexpand", &inputfile, &outputfile, pipe_flags, no_warnings, NULL);

  if (!SDDS_InitializeInput(&SDDS_input, inputfile) ||
      !SDDS_InitializeOutput(&SDDS_output, SDDS_BINARY, 1, NULL, NULL, outputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /* Set major order for output dataset */
  SDDS_output.layout.data_mode.column_major = (column_major_order != -1) ? column_major_order : SDDS_input.layout.data_mode.column_major;

  /* Retrieve column names from input dataset */
  if (!(name = SDDS_GetColumnNames(&SDDS_input, &column_sources)) ||
      !(column_source = SDDS_Malloc(sizeof(*column_source) * column_sources))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /* Define parameters based on input columns */
  for (i = 0; i < column_sources; i++) {
    column_source[i].name = name[i];
    if (!SDDS_DefineParameterLikeColumn(&SDDS_output, &SDDS_input, column_source[i].name, NULL) ||
        (column_source[i].index = SDDS_GetParameterIndex(&SDDS_output, column_source[i].name)) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    column_source[i].size = SDDS_GetTypeSize(SDDS_GetParameterType(&SDDS_output, column_source[i].index));
  }

  /* Retrieve parameter names from input dataset */
  if (!(name = SDDS_GetParameterNames(&SDDS_input, &parameter_sources)) ||
      !(parameter_source = SDDS_Malloc(sizeof(*parameter_source) * parameter_sources))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /* Handle parameter definitions and potential name conflicts */
  for (i = 0; i < parameter_sources; i++) {
    parameter_source[i].name = name[i];
    if (SDDS_GetParameterIndex(&SDDS_output, parameter_source[i].name) >= 0) {
      if (!no_warnings)
        fprintf(stderr, "Warning (sddsexpand): name %s used for parameter and column in input file.  Column data used.\n", parameter_source[i].name);
      parameter_source[i].do_copy = 0;
      continue;
    }
    parameter_source[i].do_copy = 1;
    if (!SDDS_TransferParameterDefinition(&SDDS_output, &SDDS_input, parameter_source[i].name, NULL) ||
        (parameter_source[i].index = SDDS_GetParameterIndex(&SDDS_output, parameter_source[i].name)) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    parameter_source[i].size = SDDS_GetTypeSize(SDDS_GetParameterType(&SDDS_output, parameter_source[i].index));
  }

  /* Write layout to output dataset and allocate memory for column data */
  if (!SDDS_WriteLayout(&SDDS_output) ||
      !(data = SDDS_Malloc(sizeof(*data) * column_sources))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /* Process each page of the input dataset */
  while (SDDS_ReadPage(&SDDS_input) > 0) {
    if ((rows = SDDS_RowCount(&SDDS_input)) < 0)
      continue;

    /* Retrieve data for each column */
    for (i = 0; i < column_sources; i++) {
      if (!(data[i] = SDDS_GetInternalColumn(&SDDS_input, column_source[i].name))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    /* Process each row of the current page */
    for (irow = 0; irow < rows; irow++) {
      if (!SDDS_StartPage(&SDDS_output, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      /* Set parameter values in the output dataset */
      for (i = 0; i < parameter_sources; i++) {
        if (!parameter_source[i].do_copy)
          continue;
        if (!SDDS_GetParameter(&SDDS_input, parameter_source[i].name, buffer) ||
            !SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, parameter_source[i].index, buffer, -1)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }

      /* Set column values as parameters in the output dataset */
      for (i = 0; i < column_sources; i++) {
        if (!SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, column_source[i].index, (((char *)data[i]) + irow * column_source[i].size), -1)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }

      /* Write the current page to the output dataset */
      if (!SDDS_WritePage(&SDDS_output)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }

  /* Terminate the input and output datasets */
  if (!SDDS_Terminate(&SDDS_input) || !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  /* Free allocated memory */
  free(column_source);
  free(parameter_source);
  return 0;
}
