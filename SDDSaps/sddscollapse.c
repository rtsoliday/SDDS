/**
 * @file sddscollapse.c
 * @brief Converts SDDS file parameters into columns.
 *
 * @details
 * This program processes an input SDDS file to produce an output SDDS file
 * where each data page contains parameters represented as columns of tabular data.
 * The rows in the output file correspond to different pages in the input file.
 * Original file columns are ignored in this transformation.
 *
 * The program dynamically manages memory to handle varying numbers of parameters
 * and data pages efficiently.
 *
 * @section Usage
 * ```
 * sddscollapse [<SDDSinputfile>] [<SDDSoutputfile>]
 *              [-pipe=[input][,output]]
 *              [-majorOrder=row|column]
 *              [-noWarnings]
 * ```
 *
 * @section Options
 * | Option            | Description                                                                 |
 * |-------------------|-----------------------------------------------------------------------------|
 * | `-pipe`           | Enables piping for input/output.                                           |
 * | `-majorOrder`     | Specifies major order for the output file as row or column.                |
 * | `-noWarnings`     | Suppresses warnings during execution.                                      |
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"

typedef enum {
  SET_PIPE,        /* Option for setting up input/output pipes. */
  SET_NOWARNINGS,  /* Option to suppress warnings during execution. */
  SET_MAJOR_ORDER, /* Option to specify row or column major order. */
  N_OPTIONS        /* Total number of options. */
} OptionType;

/* Array of option names corresponding to OptionType. */
char *option[N_OPTIONS] =
  {
    "pipe",
    "nowarnings",
    "majorOrder",
  };

/* Usage message displayed for help. */
char *usage =
  "sddscollapse [<SDDSinputfile>] [<SDDSoutputfile>]\n"
  "[-pipe=[input][,output]] [-majorOrder=row|column] \n"
  "[-noWarnings]\n\n"
  "sddscollapse reads data pages from a SDDS file and writes a new SDDS file \n"
  "containing a single data page.  This data page contains the parameters, \n"
  "with each parameter forming a column of the tabular data.\n\n"
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define ROW_INCREMENT 100 /* Number of rows to increment memory allocation for output. */

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_input, SDDS_output;
  char *inputfile = NULL, *outputfile = NULL, **column;
  long i, i_arg;
  long page_number, no_warnings = 0, set_page_number;
  int64_t allocated_rows;
  int32_t columns;
  SCANNED_ARG *s_arg;
  char s[SDDS_MAXLINE];
  unsigned long pipe_flags = 0, major_order_flag;
  long buffer[16];
  short column_major_order = -1;

  /* Register the program name for error messages. */
  SDDS_RegisterProgramName(argv[0]);

  /* Parse command-line arguments. */
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, usage);

  /* Process each argument. */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      /* Match the option type and process accordingly. */
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        major_order_flag = 0;
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&major_order_flag, s_arg[i_arg].list + 1,
                           &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER,
                           NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (major_order_flag & SDDS_COLUMN_MAJOR_ORDER)
          column_major_order = 1;
        else if (major_order_flag & SDDS_ROW_MAJOR_ORDER)
          column_major_order = 0;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1,
                               &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_NOWARNINGS:
        no_warnings = 1;
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      /* Process input and output filenames. */
      if (inputfile == NULL)
        inputfile = s_arg[i_arg].list[0];
      else if (outputfile == NULL)
        outputfile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  /* Validate and prepare filenames. */
  processFilenames("sddscollapse", &inputfile, &outputfile, pipe_flags, no_warnings, NULL);

  /* Initialize input SDDS file. */
  if (!SDDS_InitializeInput(&SDDS_input, inputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  /* Initialize output SDDS file. */
  if (!SDDS_InitializeOutput(&SDDS_output, SDDS_input.layout.data_mode.mode, 1,
                             NULL, NULL, outputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  /* Set major order for output. */
  if (column_major_order != -1)
    SDDS_output.layout.data_mode.column_major = column_major_order;
  else
    SDDS_output.layout.data_mode.column_major = SDDS_input.layout.data_mode.column_major;

  /* Get parameter names from the input file. */
  if (!(column = SDDS_GetParameterNames(&SDDS_input, &columns))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  /* Define output columns corresponding to input parameters. */
  for (i = 0; i < columns; i++) {
    if (!SDDS_DefineColumnLikeParameter(&SDDS_output, &SDDS_input, column[i], NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }

  /* Define a PageNumber column if it doesn't exist. */
  sprintf(s, "corresponding page number of %s for this row",
          inputfile ? inputfile : "stdin");
  if (SDDS_GetColumnIndex(&SDDS_output, "PageNumber") < 0) {
    if (SDDS_DefineColumn(&SDDS_output, "PageNumber", NULL, NULL, s, NULL,
                          SDDS_LONG, 0) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    set_page_number = 1;
  } else {
    set_page_number = 0;
  }

  /* Start writing the layout and allocate rows for output. */
  if (!SDDS_WriteLayout(&SDDS_output) ||
      !SDDS_StartPage(&SDDS_output, allocated_rows = ROW_INCREMENT)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  /* Process each data page in the input file. */
  while ((page_number = SDDS_ReadPageSparse(&SDDS_input, 0, SDDS_input.layout.data_mode.column_major ? 1 : INT32_MAX - 1, 0, 0)) > 0) {
    /* Expand memory if necessary. */
    if (page_number > allocated_rows) {
      if (!SDDS_LengthenTable(&SDDS_output, ROW_INCREMENT)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      allocated_rows += ROW_INCREMENT;
    }

    /* Copy parameters into corresponding columns. */
    for (i = 0; i < columns; i++) {
      if (!SDDS_GetParameter(&SDDS_input, column[i], buffer)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      if (!SDDS_SetRowValues(&SDDS_output,
                             SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                             page_number - 1, column[i], buffer, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }

    /* Set the PageNumber column if required. */
    if (set_page_number &&
        !SDDS_SetRowValues(&SDDS_output,
                           SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                           page_number - 1, "PageNumber", page_number, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }

  /* Free allocated memory for column names. */
  for (i = 0; i < columns; i++)
    free(column[i]);
  free(column);

  /* Finalize the output SDDS file. */
  if (!SDDS_WritePage(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (page_number == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (!SDDS_Terminate(&SDDS_input) || !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  return 0;
}
