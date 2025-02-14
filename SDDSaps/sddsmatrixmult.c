/**
 * @file sddsmatrixmult.c
 * @brief Multiplies matrices from SDDS files and outputs the result.
 *
 * @details
 * This program reads two SDDS files containing matrix data, performs matrix multiplication,
 * and writes the resulting product matrix to an output SDDS file. It supports options for 
 * data reuse, matrix commutation, output format selection, and verbosity for debugging and
 * diagnostics.
 *
 * @section Usage
 * ```
 * sddsmatrixmult [<file1>] <file2> [<output>]
 *                [-pipe=[input][,output]]
 *                [-majorOrder=row|column]
 *                [-commute]
 *                [-reuse]
 *                [-verbose]
 *                [-ascii]
 * ```
 *
 * @section Options
 * | Option                 | Description                                                     |
 * |------------------------|-----------------------------------------------------------------|
 * | `-pipe`                | Use input and/or output pipes.                                 |
 * | `-majorOrder`          | Specify output in row-major or column-major order.             |
 * | `-commute`             | Swap file1 and file2 to reverse the order of multiplication.   |
 * | `-reuse`               | Reuse the last data page if a file runs out of pages.          |
 * | `-ascii`               | Output file in ASCII mode.                                     |
 * | `-verbose`             | Enable detailed output for diagnostics.                       |
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
 * M. Borland, C. Saunders, L. Emery, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"

/* Enumeration for option types */
enum option_type {
  CLO_PIPE,
  CLO_VERBOSE,
  CLO_ASCII,
  CLO_REUSE,
  CLO_COMMUTE,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

char *commandline_option[N_OPTIONS] = {
  "pipe",
  "verbose",
  "ascii",
  "reuse",
  "commute",
  "majorOrder",
};

static char *USAGE =
  "sddsmatrixmult [OPTIONS] [<file1>] <file2>\n"
  "               [-pipe=[input][,output]]\n"
  "               [-majorOrder=row|column]\n"
  "               [-commute]\n"
  "               [-reuse]\n"
  "               [-verbose]\n"
  "               [-ascii]\n"
  "Options:\n"
  "  -pipe=[input][,output]       Read input from and/or write output to a pipe.\n"
  "  -majorOrder=row|column       Specify output in row or column major order.\n"
  "  -commute                     Use file1 as the right-hand matrix and file2 as the left-hand matrix.\n"
  "  -reuse                       Reuse the last data page if a file runs out of data pages.\n"
  "  -verbose                     Write diagnostic messages to stderr.\n"
  "  -ascii                       Output the file in ASCII mode.\n\n"
  "Description:\n"
  "  Multiplies matrices from SDDS files file1 and file2.\n"
  "  - file1: SDDS file for the left-hand matrix of the product.\n"
  "  - file2: SDDS file for the right-hand matrix of the product.\n"
  "  - output: SDDS file for the resulting product matrix.\n\n"
  "Author:\n"
  "  L. Emery ANL (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE input1Page, input2Page, outputPage;

  char *inputfile1, *inputfile2, *outputfile;
  char **Input1Column, **Input1DoubleColumn;
  char **Input2Column, **Input2DoubleColumn;
  char **OutputDoubleColumn;
  long Input1Rows, Input1DoubleColumns;
  long Input2Rows, Input2DoubleColumns;
  long OutputRows;
  int32_t Input1Columns, Input2Columns, OutputDoubleColumns;

  long i, i_arg, col, commute;
#define BUFFER_SIZE_INCREMENT 20
  MATRIX *R1, *R1Trans, *R2, *R2Trans, *R3, *R3Trans;
  long verbose;
  long ipage, ipage1, ipage2, lastPage1, lastPage2, ascii;
  unsigned long pipeFlags, majorOrderFlag;
  long tmpfile_used, noWarnings;
  long reuse;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  inputfile1 = inputfile2 = outputfile = NULL;
  Input1DoubleColumn = Input2DoubleColumn = OutputDoubleColumn = NULL;
  Input1Rows = Input1DoubleColumns = Input2Rows = Input2DoubleColumns = OutputRows = lastPage1 = lastPage2 = 0;
  tmpfile_used = 0;
  verbose = 0;
  pipeFlags = 0;
  noWarnings = 0;
  reuse = commute = ascii = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], commandline_option, N_OPTIONS, UNIQUE_MATCH)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("Invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_ASCII:
        ascii = 1;
        break;
      case CLO_REUSE:
        reuse = 1;
        break;
      case CLO_COMMUTE:
        commute = 1;
        break;
      default:
        bomb("Unrecognized option given", USAGE);
      }
    } else {
      if (!inputfile1)
        inputfile1 = s_arg[i_arg].list[0];
      else if (!inputfile2)
        inputfile2 = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        bomb("Too many filenames given", USAGE);
    }
  }

  if (pipeFlags & USE_STDIN && inputfile1) {
    if (outputfile)
      SDDS_Bomb("Too many filenames (sddsxref)");
    outputfile = inputfile2;
    inputfile2 = inputfile1;
    inputfile1 = NULL;
  }

  processFilenames("sddsmatrixmult", &inputfile1, &outputfile, pipeFlags, noWarnings, &tmpfile_used);
  if (!inputfile2)
    SDDS_Bomb("Second input file not specified");

  if (commute) {
    char *ptr = inputfile1;
    inputfile1 = inputfile2;
    inputfile2 = ptr;
  }

  /* Initialize input files */
  if (!SDDS_InitializeInput(&input1Page, inputfile1))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  Input1Column = (char **)SDDS_GetColumnNames(&input1Page, &Input1Columns);

  if (!SDDS_InitializeInput(&input2Page, inputfile2))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  Input2Column = (char **)SDDS_GetColumnNames(&input2Page, &Input2Columns);

  if (!SDDS_InitializeOutput(&outputPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, "Matrix product", "Matrix product", outputfile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (columnMajorOrder != -1)
    outputPage.layout.data_mode.column_major = columnMajorOrder;
  else
    outputPage.layout.data_mode.column_major = input1Page.layout.data_mode.column_major;

  /* Process data pages */
  while ((ipage1 = SDDS_ReadTable(&input1Page)) && (ipage2 = SDDS_ReadTable(&input2Page))) {
    if ((reuse && (ipage1 < 0 && ipage2 < 0)) ||
        (!reuse && (ipage1 < 0 || ipage2 < 0)))
      break;

    ipage = MAX(ipage1, ipage2);

    /* Process first input file */
    if (ipage1 == 1) {
      Input1DoubleColumns = 0;
      Input1DoubleColumn = (char **)malloc(Input1Columns * sizeof(char *));
      /* Count the numerical columns in input1 */
      for (i = 0; i < Input1Columns; i++) {
        if (SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&input1Page, i))) {
          Input1DoubleColumn[Input1DoubleColumns] = Input1Column[i];
          Input1DoubleColumns++;
        }
      }
      if (!Input1DoubleColumns) {
        if (verbose) {
          fprintf(stderr, "No numerical columns in page %ld of file %s.\n", ipage, inputfile1 ? inputfile1 : "stdin");
        }
      }
      Input1Rows = SDDS_CountRowsOfInterest(&input1Page);
      if (Input1DoubleColumns && Input1Rows)
        m_alloc(&R1, Input1DoubleColumns, Input1Rows);
      else if (!Input1Rows) {
        if (verbose)
          fprintf(stderr, "No rows in page %ld of file %s.\n", ipage, inputfile1 ? inputfile1 : "stdin");
      }
    }

    if (ipage1 > 0) {
      if (Input1Rows != SDDS_CountRowsOfInterest(&input1Page))
        fprintf(stderr, "Number of rows in page %ld of file %s changed.\n", ipage, inputfile1 ? inputfile1 : "stdin");
      for (col = 0; col < Input1DoubleColumns; col++) {
        if (!(R1->a[col] = (double *)SDDS_GetColumnInDoubles(&input1Page, Input1DoubleColumn[col])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      lastPage1 = ipage1;
      if (verbose)
        fprintf(stderr, "Using page %ld of file %s.\n", lastPage1, inputfile1 ? inputfile1 : "stdin");
    } else if (ipage1 < 0) {
      if (verbose)
        fprintf(stderr, "Reusing page %ld of file %s.\n", lastPage1, inputfile1 ? inputfile1 : "stdin");
    }

    if (verbose && Input1DoubleColumns && Input1Rows) {
      m_alloc(&R1Trans, Input1Rows, Input1DoubleColumns);
      m_trans(R1Trans, R1);
      m_show(R1Trans, "%9.6le ", "Input matrix 1:\n", stderr);
      m_free(&R1Trans);
    }

    /* Process second input file */
    if (ipage2 == 1) {
      Input2DoubleColumns = 0;
      Input2DoubleColumn = (char **)malloc(Input2Columns * sizeof(char *));
      /* Count the numerical columns in input2 */
      for (i = 0; i < Input2Columns; i++) {
        if (SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&input2Page, i))) {
          Input2DoubleColumn[Input2DoubleColumns] = Input2Column[i];
          Input2DoubleColumns++;
        }
      }
      if (!Input2DoubleColumns) {
        if (verbose) {
          fprintf(stderr, "No numerical columns in page %ld of file %s.\n", ipage, inputfile2);
        }
      }
      Input2Rows = SDDS_CountRowsOfInterest(&input2Page);
      if (Input2DoubleColumns && Input2Rows)
        m_alloc(&R2, Input2DoubleColumns, Input2Rows);
      else if (!Input2Rows) {
        if (verbose)
          fprintf(stderr, "No rows in page %ld of file %s.\n", ipage, inputfile2);
      }
    }

    if (ipage2 > 0) {
      if (Input2Rows != SDDS_CountRowsOfInterest(&input2Page))
        fprintf(stderr, "Number of rows in page %ld of file %s changed.\n", ipage, inputfile2);
      for (col = 0; col < Input2DoubleColumns; col++) {
        if (!(R2->a[col] = (double *)SDDS_GetColumnInDoubles(&input2Page, Input2DoubleColumn[col])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      lastPage2 = ipage2;
      if (verbose)
        fprintf(stderr, "Using page %ld of file %s.\n", lastPage2, inputfile2 ? inputfile2 : "stdin");
    } else if (ipage2 < 0) {
      if (verbose)
        fprintf(stderr, "Reusing page %ld of file %s.\n", lastPage2, inputfile2 ? inputfile2 : "stdin");
    }

    if (verbose && Input2DoubleColumns && Input2Rows) {
      m_alloc(&R2Trans, Input2Rows, Input2DoubleColumns);
      m_trans(R2Trans, R2);
      m_show(R2Trans, "%9.6le ", "Input matrix 2:\n", stderr);
      m_free(&R2Trans);
    }

    /* Define output table */
    if (ipage == 1) {
      OutputRows = Input1Rows;
      OutputDoubleColumns = Input2DoubleColumns;
      if (Input1DoubleColumns != Input2Rows) {
        fprintf(stderr, "Error: Dimension mismatch in files.\n");
        fprintf(stderr, "Right-hand matrix (%s) is %ldx%ld.\n",
                inputfile2 ? inputfile2 : "stdin", Input2Rows, Input2DoubleColumns);
        fprintf(stderr, "Left-hand matrix (%s) is %ldx%ld.\n",
                inputfile1 ? inputfile1 : "stdin", Input1Rows, Input1DoubleColumns);
        exit(EXIT_FAILURE);
      }
    }

    /* Perform matrix multiplication */
    if (OutputRows && OutputDoubleColumns) {
      if (ipage == 1)
        m_alloc(&R3, OutputDoubleColumns, OutputRows);
      if (verbose)
        fprintf(stderr, "Multiplying %d x %d matrix by %d x %d matrix\n", R2->m, R2->n, R1->m, R1->n);
      m_mult(R3, R2, R1);
      if (verbose) {
        m_alloc(&R3Trans, OutputRows, OutputDoubleColumns);
        m_trans(R3Trans, R3);
        m_show(R3Trans, "%9.6le ", "Output matrix:\n", stderr);
        m_free(&R3Trans);
      }
    } else {
      if (verbose)
        fprintf(stderr, "Output file will either have no columns or no rows in page %ld.\n", ipage);
    }

    if (ipage == 1) {
      for (i = 0; i < Input2DoubleColumns; i++) {
        if (SDDS_TransferColumnDefinition(&outputPage, &input2Page, Input2DoubleColumn[i], NULL) < 0)
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      OutputDoubleColumn = (char **)SDDS_GetColumnNames(&outputPage, &OutputDoubleColumns);
      if (!SDDS_WriteLayout(&outputPage))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }

    if (!SDDS_StartTable(&outputPage, OutputRows))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    /* Assign values to output table */
    if (OutputRows && OutputDoubleColumns) {
      for (i = 0; i < OutputDoubleColumns; i++) { /* i is the column index */
        if (!SDDS_SetColumnFromDoubles(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                                       R3->a[i], OutputRows, OutputDoubleColumn[i]))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }

    if (!SDDS_WriteTable(&outputPage))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  /* Terminate SDDS tables */
  if (!SDDS_Terminate(&input1Page) || !SDDS_Terminate(&input2Page) || !SDDS_Terminate(&outputPage))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (tmpfile_used && !replaceFileAndBackUp(inputfile1, outputfile))
    return EXIT_FAILURE;

  return EXIT_SUCCESS;
}
