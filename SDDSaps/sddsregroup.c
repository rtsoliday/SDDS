/**
 * @file sddsregroup.c
 * @brief Regroups SDDS files by reorganizing rows and pages.
 *
 * @details
 * This program processes an SDDS file where each page contains a fixed number
 * of rows and produces an output file with regrouped pages. Specifically, if the input file
 * contains m pages with n rows each, the output file will contain n pages with m rows each.
 * The user can specify which columns from the input file will become parameters in the output file,
 * and vice versa. Additionally, input parameters can be duplicated as extra columns in the output file.
 *
 * @section Usage
 * ```
 * sddsregroup [<inputfile>] [<outputfile>]
 *             [-pipe=[input][,output]]
 *             [-newparameters=<oldcolumnname>[,...]]
 *             [-newcolumns=<oldparametername>[,...]]
 *             [-warning]
 *             [-verbose]
 *             [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Optional               | Description                                                                 |
 * |------------------------|-----------------------------------------------------------------------------|
 * | `-pipe`                | Read input from and/or write output to a pipe.                            |
 * | `-newparameters`       | Specifies columns from the input file to become parameters in the output file. |
 * | `-newcolumns`          | Specifies parameters from the input file to become columns in the output file. |
 * | `-warning`             | Enables warning messages.                                                  |
 * | `-verbose`             | Enables verbose output for detailed information during execution.          |
 * | `-majorOrder`          | Specifies the data order of the output file (row-major or column-major).   |
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
 *  M. Borland,
 *  C. Saunders,
 *  R. Soliday,
 *  H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"

/* Enumeration for option types */
enum option_type {
  CLO_NEWCOLUMNS,
  CLO_NEWPARAMETERS,
  CLO_WARNING,
  CLO_VERBOSE,
  CLO_PIPE,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

char *commandline_option[N_OPTIONS] = {
  "newcolumns",
  "newparameters",
  "warning",
  "verbose",
  "pipe",
  "majorOrder",
};

static char *USAGE =
  "sddsregroup [<inputfile>] [<outputfile>]\n"
  "            [-pipe=[input][,output]]\n"
  "            [-newparameters=<oldcolumnname>[,...]]\n"
  "            [-newcolumns=<oldparametername>[,...]]\n"
  "            [-warning]\n"
  "            [-verbose]\n"
  "            [-majorOrder=row|column]\n"
  "Reorganizes the data in the input file by taking single rows from each page of the input file\n"
  "to form single pages of the output file.\n\n"
  "Options:\n"
  "  -pipe=[input][,output]          Read input from and/or write output to a pipe.\n"
  "  -newparameters=<col1>[,<col2>,...]\n"
  "                                  Specify which columns of the input file will become\n"
  "                                  parameters in the output file. By default, no new parameters\n"
  "                                  are created, and all columns of the input file are transferred\n"
  "                                  to the output file.\n"
  "  -newcolumns=<param1>[,<param2>,...]\n"
  "                                  Specify which parameters of the input file will become\n"
  "                                  columns in the output file. These columns will be duplicated\n"
  "                                  across all pages. By default, all parameter values are lost.\n"
  "  -majorOrder=row|column          Specify the data order of the output file as row-major or column-major.\n"
  "  -warning                        Enable warning messages.\n"
  "  -verbose                        Enable verbose output.\n\n"
  "Program by Louis Emery, ANL (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE inputPage, *inputPages, outputPage;

  char *inputfile, *outputfile;
  char *InputDescription, *InputContents;
  char *OutputDescription, *OutputContents;
  char **InputParameters, **InputColumns, **ColToParNames, **ParToColNames;
  int32_t NInputParameters, NInputColumns;
  long NColToPar, NParToCol, NColToCol;
  int64_t NInputRows, NOutputRows;
  long NInputPages, NOutputPages;
  long *ColToColInputIndex, *ColToParInputIndex, *ParToColInputIndex;
  long *ColToColOutputIndex, *ColToParOutputIndex, *ParToColOutputIndex;
  long pageIncrement = 20;

  long i, i_arg, j;
  long ipage;
  int64_t row;
  long verbose;
  /* long warning; */
  unsigned long pipeFlags, majorOrderFlag;
  long tmpfile_used, noWarnings;
  short columnMajorOrder = -1;

  inputPages = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  inputfile = outputfile = NULL;
  InputDescription = InputContents = NULL;
  OutputDescription = OutputContents = NULL;
  InputParameters = InputColumns = ColToParNames = ParToColNames = NULL;
  NInputParameters = NInputColumns = NColToPar = NParToCol = NColToCol = 0;
  NInputRows = NOutputRows = NInputPages = NOutputPages = 0;
  ColToColInputIndex = ColToParInputIndex = ParToColInputIndex = NULL;
  ColToColOutputIndex = ColToParOutputIndex = ParToColOutputIndex = NULL;

  verbose = 0;
  /* warning = 0; */
  pipeFlags = 0;
  tmpfile_used = 0;
  noWarnings = 0;
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], commandline_option, N_OPTIONS, UNIQUE_MATCH)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_WARNING:
        /* warning = 1; */
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_NEWCOLUMNS:
        NParToCol = s_arg[i_arg].n_items - 1;
        if (!NParToCol) {
          SDDS_Bomb("No old parameter names given");
        }
        ParToColNames = (char **)malloc(NParToCol * sizeof(char *));
        for (i = 0; i < NParToCol; i++) {
          ParToColNames[i] = s_arg[i_arg].list[i + 1];
        }
        break;
      case CLO_NEWPARAMETERS:
        NColToPar = s_arg[i_arg].n_items - 1;
        if (!NColToPar) {
          SDDS_Bomb("No old column names given");
        }
        ColToParNames = (char **)malloc(NColToPar * sizeof(char *));
        for (i = 0; i < NColToPar; i++) {
          ColToParNames[i] = s_arg[i_arg].list[i + 1];
        }
        break;
      default:
        SDDS_Bomb("unrecognized option given");
      }
    } else {
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames given");
    }
  }

  processFilenames("sddsregroup", &inputfile, &outputfile, pipeFlags, noWarnings, &tmpfile_used);

  if (!SDDS_InitializeInput(&inputPage, inputfile))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (0 < SDDS_ReadTable(&inputPage))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  InputColumns = (char **)SDDS_GetColumnNames(&inputPage, &NInputColumns);
  InputParameters = (char **)SDDS_GetParameterNames(&inputPage, &NInputParameters);
  InputDescription = InputContents = NULL;
  if (!SDDS_GetDescription(&inputPage, &InputDescription, &InputContents))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  NInputRows = SDDS_CountRowsOfInterest(&inputPage);
  ColToParInputIndex = (long *)malloc(NColToPar * sizeof(long));
  ColToParOutputIndex = (long *)malloc(NColToPar * sizeof(long));
  NColToCol = NInputColumns - NColToPar;
  ColToColInputIndex = (long *)malloc(NColToCol * sizeof(long));
  ColToColOutputIndex = (long *)malloc(NColToCol * sizeof(long));
  ParToColInputIndex = (long *)malloc(NParToCol * sizeof(long));
  ParToColOutputIndex = (long *)malloc(NParToCol * sizeof(long));

  /*******************************\
   * Check existence of selected *
   * columns and parameters      *
   \*******************************/
  for (i = 0; i < NColToPar; i++) {
    switch (SDDS_CheckColumn(&inputPage, ColToParNames[i], NULL, 0, NULL)) {
    case SDDS_CHECK_NONEXISTENT:
      fprintf(stderr, "Error: Input file doesn't contain column %s.\n", ColToParNames[i]);
      exit(EXIT_FAILURE);
    }
  }
  for (i = 0; i < NParToCol; i++) {
    switch (SDDS_CheckParameter(&inputPage, ParToColNames[i], NULL, 0, NULL)) {
    case SDDS_CHECK_NONEXISTENT:
      fprintf(stderr, "Error: Input file doesn't contain parameter %s.\n", ParToColNames[i]);
      exit(EXIT_FAILURE);
    }
  }

  /*****************************************\
   * Make copies of pages of the input file *
   \*****************************************/
  NInputPages = 0;
  if (verbose) {
    init_stats();
  }

  do {
    if (!NInputPages) {
      inputPages = (SDDS_TABLE *)malloc(pageIncrement * sizeof(SDDS_TABLE));
    } else if (!(NInputPages % pageIncrement)) {
      inputPages = (SDDS_TABLE *)realloc(inputPages, (NInputPages + pageIncrement) * sizeof(SDDS_TABLE));
    }
    if (NInputRows != SDDS_CountRowsOfInterest(&inputPage)) {
      fprintf(stderr, "Error: Number of rows in pages are not all equal.\n");
      exit(EXIT_FAILURE);
    }
    if (!SDDS_InitializeCopy(&inputPages[NInputPages], &inputPage, NULL, "m"))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (!SDDS_CopyTable(&inputPages[NInputPages], &inputPage))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (verbose) {
      fprintf(stderr, "Reading page %ld...\n", NInputPages);
    }

    NInputPages++;
  } while (0 < SDDS_ReadTable(&inputPage));

  if (!SDDS_Terminate(&inputPage))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (InputDescription) {
    OutputDescription = (char *)malloc((strlen(InputDescription) + strlen(",  regrouped") + 1) * sizeof(char));
    OutputDescription = strcat(strcpy(OutputDescription, InputDescription), ",  regrouped");
  } else {
    OutputDescription = (char *)malloc((strlen("File regrouped") + strlen(inputfile ? inputfile : "from pipe") + 1) * sizeof(char));
    sprintf(OutputDescription, "File %s regrouped", inputfile ? inputfile : "from pipe");
  }
  if (InputContents) {
    OutputContents = (char *)malloc((strlen(InputContents) + strlen(",  regrouped") + 1) * sizeof(char));
    OutputContents = strcat(strcpy(OutputContents, InputContents), ",  regrouped");
  } else {
    OutputContents = (char *)malloc((strlen("File regrouped") + strlen(inputfile ? inputfile : "from pipe") + 1) * sizeof(char));
    sprintf(OutputContents, "File %s regrouped", inputfile ? inputfile : "from pipe");
  }
  if (!SDDS_InitializeOutput(&outputPage, SDDS_BINARY, 0, OutputDescription, OutputContents, outputfile))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (columnMajorOrder != -1)
    outputPage.layout.data_mode.column_major = columnMajorOrder;
  else
    outputPage.layout.data_mode.column_major = inputPage.layout.data_mode.column_major;
  /*************************************************\
   * Define columns and parameters and store indices *
   \*************************************************/

  /******************************************************\
   *  Selected input columns are transferred to parameters *
   \******************************************************/
  for (i = 0; i < NColToPar; i++) {
    if (!SDDS_DefineParameterLikeColumn(&outputPage, &inputPages[0], ColToParNames[i], ColToParNames[i]))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    ColToParInputIndex[i] = SDDS_GetColumnIndex(&inputPages[0], ColToParNames[i]);
    ColToParOutputIndex[i] = SDDS_GetParameterIndex(&outputPage, ColToParNames[i]);
  }
  /****************************************************\
   * Selected input parameters are transferred to columns *
   \****************************************************/
  for (i = 0; i < NParToCol; i++) {
    if (!SDDS_DefineColumnLikeParameter(&outputPage, &inputPages[0], ParToColNames[i], ParToColNames[i]))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    ParToColInputIndex[i] = SDDS_GetParameterIndex(&inputPages[0], ParToColNames[i]);
    ParToColOutputIndex[i] = SDDS_GetColumnIndex(&outputPage, ParToColNames[i]);
  }
  /***********************************\
   * Columns are transferred to columns *
   \***********************************/
  j = 0;
  for (i = 0; i < NInputColumns; i++) {
    if (0 > match_string(InputColumns[i], ColToParNames, NColToPar, EXACT_MATCH)) {
      if (0 > SDDS_TransferColumnDefinition(&outputPage, &inputPages[0], InputColumns[i], InputColumns[i]))
        SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      ColToColInputIndex[j] = SDDS_GetColumnIndex(&inputPages[0], InputColumns[i]);
      ColToColOutputIndex[j] = SDDS_GetColumnIndex(&outputPage, InputColumns[i]);
      j++;
    }
  }
  if (j != NColToCol)
    SDDS_Bomb("Error: Something went wrong with counting the columns. Report to author.");
  if (!SDDS_WriteLayout(&outputPage))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  /*******************************\
   * Fill pages in the output file *
   \*******************************/
  NOutputPages = NInputRows;
  NOutputRows = NInputPages;
  for (ipage = 0; ipage < NOutputPages; ipage++) {
    if (verbose)
      fprintf(stderr, "Starting page %ld...\n", ipage);
    SDDS_StartTable(&outputPage, NOutputRows);
    /* Set parameters */
    for (i = 0; i < NColToPar; i++) {
      if (!SDDS_SetParameters(&outputPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, ColToParOutputIndex[i], SDDS_GetValueByAbsIndex(&inputPages[0], ColToParInputIndex[i], ipage, NULL), -1))
        SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    /* Set columns  */
    for (i = 0; i < NParToCol; i++) {
      /* Transfer parameters of input file to columns of output */
      for (row = 0; row < NOutputRows; row++) {
        if (!SDDS_SetRowValues(&outputPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, row, ParToColOutputIndex[i], SDDS_GetParameter(&inputPages[row], InputParameters[ParToColInputIndex[i]], NULL), -1))
          SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
    for (i = 0; i < NColToCol; i++) {
      for (row = 0; row < NOutputRows; row++) {
        if (!SDDS_SetRowValues(&outputPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, row, ColToColOutputIndex[i], SDDS_GetValueByAbsIndex(&inputPages[row], ColToColInputIndex[i], ipage, NULL), -1))
          SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }

    if (!SDDS_WriteTable(&outputPage))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  for (i = 0; i < NInputPages; i++) {
    if (!SDDS_Terminate(&inputPages[i]))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (inputPages)
    free(inputPages);
  if (!SDDS_Terminate(&outputPage))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile))
    exit(EXIT_FAILURE);
  return EXIT_SUCCESS;
}
