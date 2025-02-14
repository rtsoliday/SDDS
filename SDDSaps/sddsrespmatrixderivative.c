/**
 * @file sddsrespmatrixderivative.c
 * @brief Calculates the response matrix derivative with respect to correctors, BPMs, or quadrupoles.
 *
 * @details
 * This program processes an input SDDS response matrix file to compute its derivative with respect to selected elements
 * such as correctors ("cor"), Beam Position Monitors (BPMs, "bpm"), or quadrupoles ("quad"). Additional options
 * allow users to append zero rows before or after the main data. Supported modes and options are described below.
 *
 * @section Usage
 * ```
 * sddsrespmatrixderivative [<inputfile>] [<outputfile>]
 *                          [-pipe=[input][,output]]
 *                           -mode=<string> 
 *                          [-addRowsBefore=<number>] 
 *                          [-addRowsAfter=<number>] 
 *                          [-verbose]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-mode=<string>`                      | Mode of derivative: `cor`, `bpm` or `quad`                                            |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe=[input][,output]`              | Read input from and/or write output to a pipe.                                        |
 * | `-addRowsBefore=<number>`             | Add zero rows before meaningful rows. Default: 0.                                    |
 * | `-addRowsAfter=<number>`              | Add zero rows after meaningful rows. Default: 0.                                     |
 * | `-verbose`                            | Enable detailed logging to stderr.                                                   |
 *
 * @subsection Incompatibilities
 *   - When `-mode=quad` is specified:
 *     - `-addRowsBefore` is treated as the starting column number for diagonal output.
 *     - `-addRowsAfter` specifies the number of rows of diagonal output.
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
 * R. Soliday
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"

/* Enumeration for option types */
enum option_type {
  CLO_VERBOSE,
  CLO_PIPE,
  CLO_MODE,
  CLO_ADDROWSBEFORE,
  CLO_ADDROWSAFTER,
  N_OPTIONS
};

char *commandline_option[N_OPTIONS] = {
  "verbose",
  "pipe",
  "mode",
  "addRowsBefore",
  "addRowsAfter",
};

static char *USAGE =
  "sddsrespmatrixderivative [<inputfile>] [<outputfile>]\n"
  "                         [-pipe=[input][,output]]\n"
  "                          -mode=<string> \n"
  "                         [-addRowsBefore=<number>] \n"
  "                         [-addRowsAfter=<number>] \n"
  "                         [-verbose]\n"
  "Options:\n"
  "  -pipe=[input][,output]        Read input from and/or write output to a pipe.\n"
  "  -mode=<string>                Specify the mode of derivative:\n"
  "                                 \"cor\"  - derivative with respect to correctors\n"
  "                                 \"bpm\"   - derivative with respect to BPMs\n"
  "                                 \"quad\"  - add rows related to quad constraints\n"
  "  -addRowsBefore=<number>       Number of zero rows to add before the meaningful rows of output.\n"
  "                                 Default is 0.\n"
  "                                 If mode=quad, it specifies the column number where the diagonal output starts.\n"
  "  -addRowsAfter=<number>        Number of zero rows to add after the meaningful rows of output.\n"
  "                                 Default is 0.\n"
  "                                 If mode=quad, it specifies the number of rows of diagonal output.\n"
  "  -verbose                      Print incidental information to stderr.\n";

char **TokenizeString(char *source, long n_items);
char *JoinStrings(char **source, long n_items, long buflen_increment);
int MakeDerivative(char *mode, int addRowsBefore, int addRowsAfter, int addColumnsBefore, MATRIX *B, MATRIX *A);
int MakeRootnameColumn(char *mode, long inputDoubleColumns, int64_t inputRows, long addRowsBefore, long addRowsAfter,
                       char **inputDoubleColumnName, char **stringData, char **rootnameData);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET inputPage, outputPage;

  char *inputfile, *outputfile;
  char **inputColumnName, **inputStringColumnName, **inputDoubleColumnName;
  char **outputStringColumnName, **outputDoubleColumnName;
  int64_t inputRows, outputRows;
  long inputDoubleColumns, inputStringColumns;
  long outputDoubleColumns, outputStringColumns;
  char **inputParameterName;
  int32_t inputParameters, inputColumns;
  char *inputDescription, *inputContents;
  long i, i_arg, col;
#define BUFFER_SIZE_INCREMENT 16384
  MATRIX *R, *Rderiv;
  long OldStringColumnsDefined;
  long verbose;
  long ascii;
  unsigned long pipeFlags;
  long tmpfile_used, noWarnings;
  long ipage, columnType;
  long addRowsBefore, addRowsAfter, addColumnsBefore;
  char *mode;
  char **stringData = NULL;
  char **rootnameData = NULL;

  inputColumnName = outputStringColumnName = outputDoubleColumnName = inputParameterName = NULL;
  outputRows = outputDoubleColumns = outputStringColumns = OldStringColumnsDefined = 0;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  inputfile = outputfile = NULL;
  verbose = 0;
  ascii = 0;
  pipeFlags = 0;
  tmpfile_used = 0;
  noWarnings = 0;
  mode = NULL;
  int modeDefined = 0;
  addRowsBefore = 0;
  addRowsAfter = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, N_OPTIONS, UNIQUE_MATCH)) {
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;
      case CLO_MODE:
        modeDefined = 1;
        if (!(mode = s_arg[i_arg].list[1]))
          SDDS_Bomb("No mode string provided");
        break;
      case CLO_ADDROWSBEFORE:
        if (!(sscanf(s_arg[i_arg].list[1], "%ld", &addRowsBefore)) || (addRowsBefore < 0))
          SDDS_Bomb("Invalid value for addRowsBefore: must be a non-negative number");
        break;
      case CLO_ADDROWSAFTER:
        if (!(sscanf(s_arg[i_arg].list[1], "%ld", &addRowsAfter)) || (addRowsAfter < 0))
          SDDS_Bomb("Invalid value for addRowsAfter: must be a non-negative number");
        break;
      default:
        bomb("Unrecognized option provided", USAGE);
      }
    } else {
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        bomb("Too many filenames provided", USAGE);
    }
  }

  if (modeDefined) {
    if (strcmp(mode, "cor") && strcmp(mode, "bpm") && strcmp(mode, "quad")) {
      bomb("Invalid mode parameter", "Mode must be 'cor', 'bpm', or 'quad'");
    }
  } else {
    bomb("Mode parameter is not defined.", "Mode must be 'cor', 'bpm', or 'quad'");
  }

  processFilenames("sddsrespmatrixderivative", &inputfile, &outputfile, pipeFlags, noWarnings, &tmpfile_used);

  if (!SDDS_InitializeInput(&inputPage, inputfile) ||
      !(inputColumnName = (char **)SDDS_GetColumnNames(&inputPage, &inputColumns)) ||
      !(inputParameterName = (char **)SDDS_GetParameterNames(&inputPage, &inputParameters)) ||
      !SDDS_GetDescription(&inputPage, &inputDescription, &inputContents)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  inputDoubleColumns = 0;
  inputStringColumns = 0;
  inputDoubleColumnName = (char **)malloc(inputColumns * sizeof(char *));
  inputStringColumnName = (char **)malloc(inputColumns * sizeof(char *));
  inputRows = 0;

  if (strcmp(mode, "quad") == 0) {
    addColumnsBefore = addRowsBefore;
    addRowsBefore = 0;
  } else {
    addColumnsBefore = 0;
  }

  /* Read data */
  while (0 < (ipage = SDDS_ReadTable(&inputPage))) {
    if (ipage == 1) {
      if (!SDDS_SetColumnFlags(&inputPage, 0))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

      /* Count the string and numerical columns in the input file */
      for (i = 0; i < inputColumns; i++) {
        if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&inputPage, i))) {
          inputDoubleColumnName[inputDoubleColumns] = inputColumnName[i];
          inputDoubleColumns++;
        } else if (columnType == SDDS_STRING) {
          inputStringColumnName[inputStringColumns] = inputColumnName[i];
          inputStringColumns++;
        }
      }

      if (!(inputRows = SDDS_CountRowsOfInterest(&inputPage)))
        SDDS_Bomb("No rows in dataset.");
    } else {
      SDDS_Bomb("Dataset must be one-page.");
    }

    if (inputDoubleColumns == 0)
      SDDS_Bomb("No numerical columns in file.");

    if ((ipage == 1) && verbose) {
      fprintf(stderr, "Number of numerical columns: %ld.\n", inputDoubleColumns);
      fprintf(stderr, "Number of string columns: %ld.\n", inputStringColumns);
      fprintf(stderr, "Number of rows: %" PRId64 ".\n", inputRows);
    }

    /* Work on data */
    if (inputDoubleColumns) {
      m_alloc(&R, inputDoubleColumns, inputRows);
      outputRows = inputDoubleColumns * inputRows + addRowsBefore + addRowsAfter;

      if (strcmp(mode, "cor") == 0) {
        m_alloc(&Rderiv, inputDoubleColumns, outputRows);
      } else if (strcmp(mode, "bpm") == 0) {
        m_alloc(&Rderiv, inputRows, outputRows);
      } else {
        outputRows = addRowsAfter;
        m_alloc(&Rderiv, inputDoubleColumns, outputRows);
      }

      for (col = 0; col < inputDoubleColumns; col++) {
        if (!(R->a[col] = (double *)SDDS_GetColumnInDoubles(&inputPage, inputDoubleColumnName[col]))) {
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }

      /* Read stringData only in non-quad mode */
      if (strcmp(mode, "quad") != 0) {
        if (!(stringData = SDDS_GetColumn(&inputPage, "BPMName"))) {
          SDDS_SetError("Unable to read specified column: BPMName.");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return EXIT_FAILURE;
        }
      }
    }

    if ((ipage == 1) && verbose) {
      fprintf(stderr, "Starting MakeDerivative...\n");
    }

    MakeDerivative(mode, addRowsBefore, addRowsAfter, addColumnsBefore, Rderiv, R);

    rootnameData = (char **)malloc(outputRows * sizeof(char *));
    if ((ipage == 1) && verbose) {
      fprintf(stderr, "Starting MakeRootnameColumn...\n");
    }

    MakeRootnameColumn(mode, inputDoubleColumns, inputRows, addRowsBefore, addRowsAfter,
                       inputDoubleColumnName, stringData, rootnameData);

    /* Define output page */
    if ((ipage == 1) && verbose) {
      fprintf(stderr, "Starting output...\n");
    }

    if (ipage == 1) {
      if (strcmp(mode, "cor") == 0) {
        outputDoubleColumns = inputDoubleColumns;
        outputDoubleColumnName = inputDoubleColumnName;
      } else if (strcmp(mode, "bpm") == 0) {
        outputDoubleColumns = inputRows;
        outputDoubleColumnName = stringData;
      } else {
        outputDoubleColumns = inputDoubleColumns;
        outputDoubleColumnName = inputDoubleColumnName;
      }

      if (!SDDS_InitializeOutput(&outputPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, NULL, NULL, outputfile))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      /* Define string columns */
      outputStringColumns = 1;
      outputStringColumnName = (char **)malloc(sizeof(char *));
      outputStringColumnName[0] = "Rootname";
      if (SDDS_DefineColumn(&outputPage, outputStringColumnName[0], NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0)
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      /* Define double columns */
      for (i = 0; i < outputDoubleColumns; i++) {
        if (SDDS_DefineColumn(&outputPage, outputDoubleColumnName[i], NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      /* Write layout */
      switch (SDDS_CheckParameter(&outputPage, "InputFile", NULL, SDDS_STRING, NULL)) {
      case SDDS_CHECK_NONEXISTENT:
        if (SDDS_DefineParameter(&outputPage, "InputFile", NULL, NULL, "Original matrix file", NULL, SDDS_STRING, NULL) < 0)
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      default:
        break;
      }

      if (!SDDS_WriteLayout(&outputPage))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!SDDS_StartTable(&outputPage, outputRows))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (ipage == 1) {
      if (!SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "InputFile", inputfile ? inputfile : "pipe", NULL))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    /* Assign data to output table part of data set */

    /* Assign string column data */
    if (!SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, rootnameData, outputRows, outputStringColumnName[0]))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    /* Assign double column data */
    for (i = 0; i < outputDoubleColumns; i++) {
      if (!SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, Rderiv->a[i], outputRows, outputDoubleColumnName[i]))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_WriteTable(&outputPage))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (outputDoubleColumns) {
    m_free(&R);
    m_free(&Rderiv);
    free(rootnameData);
  }

  if (!SDDS_Terminate(&inputPage) || !SDDS_Terminate(&outputPage))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile))
    exit(EXIT_FAILURE);

  return EXIT_SUCCESS;
}

/**********************************************************************************************************/

int MakeRootnameColumn(char *mode, long inputDoubleColumns, int64_t inputRows, long addRowsBefore, long addRowsAfter,
                       char **inputDoubleColumnName, char **stringData, char **rootnameData) {
  int64_t i, j, n;

  n = 0;
  for (i = 0; i < addRowsBefore; i++) {
    rootnameData[n] = (char *)malloc(sizeof(char) * 1);
    rootnameData[n][0] = '\0'; // Initialize to empty string
    n++;
  }

  if (strcmp(mode, "quad") == 0) {
    for (i = 0; i < addRowsAfter; i++) {
      if (addRowsAfter > inputDoubleColumns) {
        SDDS_SetError("Number of addRowsAfter is greater than number of input columns in quad mode.");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return EXIT_FAILURE;
      }
      rootnameData[n] = (char *)malloc(sizeof(char) * (strlen(inputDoubleColumnName[i]) + 1));
      strcpy(rootnameData[n], inputDoubleColumnName[i]);
      n++;
    }
  } else {
    for (i = 0; i < inputRows; i++) {
      for (j = 0; j < inputDoubleColumns; j++) {
        size_t len = strlen(inputDoubleColumnName[j]) + strlen(stringData[i]) + 1;
        rootnameData[n] = (char *)malloc(sizeof(char) * (len + 1));
        strcpy(rootnameData[n], inputDoubleColumnName[j]);
        strcat(rootnameData[n], stringData[i]);
        n++;
      }
    }
    /* Only in non-quad mode, add additional rows after */
    for (i = 0; i < addRowsAfter; i++) {
      rootnameData[n] = (char *)malloc(sizeof(char) * 1);
      rootnameData[n][0] = '\0'; // Initialize to empty string
      n++;
    }
  }

  return EXIT_SUCCESS;
}

/**********************************************************************************************************/

int MakeDerivative(char *mode, int addRowsBefore, int addRowsAfter, int addColumnsBefore, MATRIX *B, MATRIX *A) {
  register long i, j;
  long Ncols, Nrows;

  Ncols = A->n;
  Nrows = A->m;

  /* Initialize matrix B to zero */
  for (i = 0; i < B->n; i++) {
    for (j = 0; j < B->m; j++) {
      B->a[i][j] = 0.0;
    }
  }

  if (strcmp(mode, "cor") == 0) {
    for (i = 0; i < Ncols; i++) {
      for (j = 0; j < Nrows; j++) {
        B->a[i][i + j * Ncols + addRowsBefore] = A->a[i][j];
      }
    }
  } else if (strcmp(mode, "bpm") == 0) {
    for (i = 0; i < Nrows; i++) {
      for (j = 0; j < Ncols; j++) {
        B->a[i][i * Ncols + j + addRowsBefore] = A->a[j][i];
      }
    }
  } else { /* mode == "quad" */
    for (i = 0; i < addRowsAfter; i++) {
      B->a[i + addColumnsBefore][i] = 1.0;
    }
  }

  return EXIT_SUCCESS;
}
