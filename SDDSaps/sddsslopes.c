/**
 * @file sddsslopes.c
 * @brief Computes straight-line fits of numerical column data from an SDDS experiment output file.
 *
 * @details
 * This program reads an SDDS input file, performs linear fits on specified columns using a defined
 * independent variable, and writes the slopes and intercepts to an output file. It supports options
 * to generate residuals, handle sigma values for error calculations, and configure output formats and ordering.
 * 
 * @section Usage
 * ```
 * sddsslopes [<inputfile>] [<outputfile>]
 *            [-pipe=[input][,output]]
 *             -independentVariable=<columnName> 
 *            [-range=<lower>,<upper>]
 *            [-columns=<list-of-names>] 
 *            [-excludeColumns=<list-of-names>] 
 *            [-sigma[=generate][,minimum=<val>]
 *            [-residual=<file>] 
 *            [-ascii] 
 *            [-verbose] 
 *            [-majorOrder=row|column]
 *            [-nowarnings]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-independentVariable`                | Specifies the independent variable column for the linear fits.                        |
 * 
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Read input or write output through a pipe.                                           |
 * | `-range`                              | Specifies the range of the independent variable for fitting.                         |
 * | `-columns`                            | Comma-separated list of columns to perform fits on.                                  |
 * | `-excludeColumns`                     | Comma-separated list of columns to exclude from fitting.                             |
 * | `-sigma`                              | Calculates errors using sigma columns or generates them.                             |
 * | `-residual`                           | Outputs residuals of the linear fit to a specified file.                              |
 * | `-ascii`                              | Outputs the slopes file in ASCII format (default is binary).                         |
 * | `-verbose`                            | Enables verbose output to stderr.                                                   |
 * | `-majorOrder`                         | Specifies output file ordering.                                                     |
 * | `-nowarnings`                         | Suppresses warning messages.                                                        |
 *
 * @subsection Incompatibilities
 *   - For `-sigma`:
 *     - `generate` and `minimum=<val>` cannot be used together.
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
 * L. Emery, M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"

/* Enumeration for option types */
enum option_type {
  CLO_INDEPENDENT_COLUMN,
  CLO_COLUMNS,
  CLO_EXCLUDE,
  CLO_VERBOSE,
  CLO_SIGMA,
  CLO_ASCII,
  CLO_PIPE,
  CLO_RESIDUAL,
  CLO_RANGE,
  CLO_MAJOR_ORDER,
  CLO_NO_WARNINGS,
  N_OPTIONS
};

char *commandline_option[N_OPTIONS] = {
  "independentVariable",
  "columns",
  "excludeColumns",
  "verbose",
  "sigma",
  "ascii",
  "pipe",
  "residual",
  "range",
  "majorOrder",
  "nowarnings"
};

#define SIGMA_GENERATE 0
#define SIGMA_OPTIONS 1
char *sigma_option[SIGMA_OPTIONS] = {
  "generate"
};

#define DEFAULT_EXCLUDED_COLUMNS 1
char *defaultExcludedColumn[DEFAULT_EXCLUDED_COLUMNS] = {
  "Time",
};

static char *USAGE =
  "sddsslopes [<inputfile>] [<outputfile>]\n"
  "           [-pipe=[input][,output]]\n"
  "            -independentVariable=<columnName> \n"
  "           [-range=<lower>,<upper>]\n"
  "           [-columns=<list-of-names>] \n"
  "           [-excludeColumns=<list-of-names>] \n"
  "           [-sigma[=generate][,minimum=<val>]\n"
  "           [-residual=<file>] \n"
  "           [-ascii] \n"
  "           [-verbose] \n"
  "           [-majorOrder=row|column]\n"
  "           [-nowarnings]\n"
  "Options:\n"
  "  -pipe=[input][,output]             Read input or write output from/to a pipe.\n"
  "  -independentVariable=<columnName>   Specify the independent variable column.\n"
  "  -range=<lower>,<upper>              Specify the range of the independent variable for fitting.\n"
  "  -columns=<list-of-names>            Comma-separated list of columns to perform fits on.\n"
  "  -excludeColumns=<list-of-names>     Comma-separated list of columns to exclude from fitting.\n"
  "  -sigma[=generate][,minimum=<val>]   Calculate errors using sigma columns or generate them.\n"
  "  -residual=<file>                    Output file for residuals of the linear fit.\n"
  "  -ascii                              Output slopes file in ASCII mode (default is binary).\n"
  "  -verbose                            Enable verbose output to stderr.\n"
  "  -majorOrder=<row|column>            Specify output file ordering.\n"
  "  -nowarnings                         Suppress warning messages.\n\n"
  "Description:\n"
  "  Performs straight line fits on numerical columns in the input SDDS file using a specified\n"
  "  independent variable. Outputs the slope and intercept for each selected column.\n"
  "  The independent variable column is excluded from the output but its name is stored as a parameter.\n\n"
  "Example:\n"
  "  sddsslopes data_input.sdds data_output.sdds -independentVariable=Time -columns=X,Y,Z -verbose\n"
  "Program by Louis Emery, ANL (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

long set_multicolumn_flags(SDDS_TABLE *SDDSin, char ***column, int32_t *columns, char **exclude, long excludes);

int main(int argc, char **argv) {
  SCANNED_ARG *scanned;
  SDDS_TABLE inputPage, outputPage, residualPage;
  char *inputfile, *outputfile;
  char **column, **excludeColumn;
  int32_t columns;
  long excludeColumns;
  char *indColumnName;
  long verbose;
  long iArg, ipage;
  double *indVar, *indVarOrig;
  char *indVarUnits;
  char **intColumn, **slopeColumn, **slopeSigmaColumn, **interceptSigmaColumn;
  char *Units, *slopeUnits;
  double *depVar, *depVarOrig;
  long order;
  double *coef, *coefsigma, *weight, *diff, *diffOrig, chi;
  long iCol;
  int64_t i, j, rows, rowsOrig, iRow;
  double rmsResidual;
  double slopeSigma, interceptSigma;
  char **sigmaColumn, **chiSquaredColumn;
  long *sigmaColumnExists;
  long doSlopeSigma, generateSigma, doPreliminaryFit;
  int64_t validSigmas;
  double sigmaSum, averageSigma, minSigma;
  long ascii;
  char *residualFile;
  unsigned long pipeFlags;
  long tmpfile_used, noWarnings;
  long unsigned int majorOrderFlag;
  double xMin, xMax;
  short columnMajorOrder = -1;

  indVar = indVarOrig = depVar = depVarOrig = coef = coefsigma = weight = diff = NULL;
  intColumn = slopeColumn = slopeSigmaColumn = interceptSigmaColumn = sigmaColumn = chiSquaredColumn = NULL;
  slopeUnits = NULL;
  sigmaColumnExists = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  inputfile = outputfile = NULL;
  columns = excludeColumns = 0;
  column = excludeColumn = NULL;
  indColumnName = NULL;
  verbose = 0;
  doSlopeSigma = 0;
  generateSigma = 0;
  minSigma = 0;
  doPreliminaryFit = 0;
  ascii = 0;
  pipeFlags = 0;
  tmpfile_used = 0;
  noWarnings = 0;
  residualFile = NULL;
  xMin = xMax = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      delete_chars(scanned[iArg].list[0], "_");
      switch (match_string(scanned[iArg].list[0], commandline_option, N_OPTIONS, UNIQUE_MATCH)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_INDEPENDENT_COLUMN:
        if (!(indColumnName = scanned[iArg].list[1]))
          SDDS_Bomb("no string given for option -independentVariable");
        break;
      case CLO_COLUMNS:
        if (columns)
          SDDS_Bomb("only one -columns option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        column = tmalloc(sizeof(*column) * (columns = scanned[iArg].n_items - 1));
        for (i = 0; i < columns; i++)
          column[i] = scanned[iArg].list[i + 1];
        break;
      case CLO_EXCLUDE:
        if (excludeColumns)
          SDDS_Bomb("only one -excludecolumns option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -excludecolumns syntax");
        excludeColumn = tmalloc(sizeof(*excludeColumn) * (excludeColumns = scanned[iArg].n_items - 1));
        for (i = 0; i < excludeColumns; i++)
          excludeColumn[i] = scanned[iArg].list[i + 1];
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_ASCII:
        ascii = 1;
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_SIGMA:
        doSlopeSigma = 1;
        if (scanned[iArg].n_items > 1) {
          unsigned long sigmaFlags = 0;
          scanned[iArg].n_items--;
          if (!scanItemList(&sigmaFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                            "generate", -1, NULL, 0, 1,
                            "minimum", SDDS_DOUBLE, &minSigma, 1, 0,
                            NULL) ||
              minSigma < 0)
            SDDS_Bomb("invalid -sigma syntax");
          if (sigmaFlags & 1)
            generateSigma = 1;
        }
        break;
      case CLO_RESIDUAL:
        if (!(residualFile = scanned[iArg].list[1])) {
          fprintf(stderr, "No file specified in -residual option.\n");
          exit(EXIT_FAILURE);
        }
        break;
      case CLO_RANGE:
        if (scanned[iArg].n_items != 3 ||
            1 != sscanf(scanned[iArg].list[1], "%lf", &xMin) ||
            1 != sscanf(scanned[iArg].list[2], "%lf", &xMax) ||
            xMin >= xMax)
          SDDS_Bomb("incorrect -range syntax");
        break;
      case CLO_NO_WARNINGS:
        noWarnings = 1;
        break;
      default:
        SDDS_Bomb("unrecognized option given");
        break;
      }
    } else {
      if (!inputfile)
        inputfile = scanned[iArg].list[0];
      else if (!outputfile)
        outputfile = scanned[iArg].list[0];
      else
        SDDS_Bomb("too many filenames given");
    }
  }

  if (residualFile && outputfile) {
    if (!strcmp(residualFile, outputfile)) {
      fprintf(stderr, "Residual file can't be the same as the output file.\n");
      exit(EXIT_FAILURE);
    }
  }

  processFilenames("sddsslopes", &inputfile, &outputfile, pipeFlags, noWarnings, &tmpfile_used);

  if (!indColumnName) {
    fprintf(stderr, "independentVariable not given\n");
    exit(EXIT_FAILURE);
  }

  if (!excludeColumns) {
    excludeColumn = defaultExcludedColumn;
    excludeColumns = DEFAULT_EXCLUDED_COLUMNS;
  }

  if (verbose)
    fprintf(stderr, "Reading file %s.\n", inputfile);
  if (!SDDS_InitializeInput(&inputPage, inputfile))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  while (0 < (ipage = SDDS_ReadTable(&inputPage))) {
    if (verbose) {
      fprintf(stderr, "working on page %ld\n", ipage);
    }
    rows = SDDS_CountRowsOfInterest(&inputPage);
    rowsOrig = rows;
    /*************************************\
     * make array of independent variable
     \*************************************/
    if (ipage == 1) {
      indVar = (double *)malloc(sizeof(*indVar) * rows);
      if (!indVar) {
        fprintf(stderr, "Memory allocation failed for indVar.\n");
        exit(EXIT_FAILURE);
      }
    } else {
      indVar = (double *)realloc(indVar, sizeof(*indVar) * rows);
      if (!indVar) {
        fprintf(stderr, "Memory reallocation failed for indVar.\n");
        exit(EXIT_FAILURE);
      }
    }
    if (ipage == 1) {
      if (!SDDS_FindColumn(&inputPage, FIND_NUMERIC_TYPE, indColumnName, NULL)) {
        fprintf(stderr, "Something wrong with column %s.\n", indColumnName);
        SDDS_CheckColumn(&inputPage, indColumnName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr);
        exit(EXIT_FAILURE);
      }
    }
    /* filter out the specified range in independent variable */
    if (xMin != xMax) {
      if (!(indVarOrig = SDDS_GetColumnInDoubles(&inputPage, indColumnName)))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      for (i = j = 0; i < rowsOrig; i++) {
        if (indVarOrig[i] <= xMax && indVarOrig[i] >= xMin) {
          indVar[j] = indVarOrig[i];
          j++;
        }
      }
    } else {
      if (!(indVar = SDDS_GetColumnInDoubles(&inputPage, indColumnName)))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (ipage == 1) {
      if (!SDDS_GetColumnInformation(&inputPage, "units", &indVarUnits, SDDS_GET_BY_NAME, indColumnName))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!indVarUnits) {
        indVarUnits = (char *)malloc(sizeof(*indVarUnits));
        indVarUnits[0] = 0;
      }
    }
    /************************************\
     * initialize residual file
     \************************************/
    if (residualFile) {
      if (ipage == 1) {
        if (!SDDS_InitializeOutput(&residualPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, "Residual of 2-term fit", NULL, outputfile) ||
            !SDDS_InitializeCopy(&residualPage, &inputPage, residualFile, "w"))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (columnMajorOrder != -1)
          residualPage.layout.data_mode.column_major = columnMajorOrder;
        else
          residualPage.layout.data_mode.column_major = inputPage.layout.data_mode.column_major;
        if (!SDDS_WriteLayout(&residualPage))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!SDDS_CopyPage(&residualPage, &inputPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    /************************************\
     * get columns of interest. use set_multicolumn_flags to simply
     * return new values for array column.
     \************************************/
    if (!set_multicolumn_flags(&inputPage, &column, &columns, excludeColumn, excludeColumns)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    /************************************\
     * make  column names for the output
     \************************************/
    if (ipage == 1) {
      intColumn = (char **)malloc((sizeof(char *) * columns));
      slopeColumn = (char **)malloc((sizeof(char *) * columns));
      if (doSlopeSigma) {
        slopeSigmaColumn = (char **)malloc((sizeof(char *) * columns));
        interceptSigmaColumn = (char **)malloc((sizeof(char *) * columns));
        chiSquaredColumn = (char **)malloc((sizeof(char *) * columns));
      }
      for (i = 0; i < columns; i++) {
        intColumn[i] = (char *)malloc((sizeof(char) * (strlen(column[i]) + strlen("Intercept") + 1)));
        snprintf(intColumn[i], strlen(column[i]) + strlen("Intercept") + 1, "%sIntercept", column[i]);
        
        slopeColumn[i] = (char *)malloc((sizeof(char) * (strlen(column[i]) + strlen("Slope") + 1)));
        snprintf(slopeColumn[i], strlen(column[i]) + strlen("Slope") + 1, "%sSlope", column[i]);
        
        if (doSlopeSigma) {
          slopeSigmaColumn[i] = (char *)malloc((sizeof(char) * (strlen(column[i]) + strlen("SlopeSigma") + 1)));
          snprintf(slopeSigmaColumn[i], strlen(column[i]) + strlen("SlopeSigma") + 1, "%sSlopeSigma", column[i]);
          
          interceptSigmaColumn[i] = (char *)malloc((sizeof(char) * (strlen(column[i]) + strlen("InterceptSigma") + 1)));
          snprintf(interceptSigmaColumn[i], strlen(column[i]) + strlen("InterceptSigma") + 1, "%sInterceptSigma", column[i]);
          
          chiSquaredColumn[i] = (char *)malloc((sizeof(char) * (strlen(column[i]) + strlen("ChiSquared") + 1)));
          snprintf(chiSquaredColumn[i], strlen(column[i]) + strlen("ChiSquared") + 1, "%sChiSquared", column[i]);
        }
      }
    }
    /************************************\
     * Write layout for output file
     \************************************/
    if (ipage == 1) {
      if (verbose)
        fprintf(stderr, "Opening file %s.\n", outputfile);
      if (!SDDS_InitializeOutput(&outputPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, "2-term fit", NULL, outputfile) ||
          0 > SDDS_DefineParameter(&outputPage, "InputFile", "InputFile", NULL, "InputFile", NULL, SDDS_STRING, 0) ||
          0 > SDDS_DefineColumn(&outputPage, "IndependentVariable", NULL, NULL, NULL, NULL, SDDS_STRING, 0))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (columnMajorOrder != -1)
        outputPage.layout.data_mode.column_major = columnMajorOrder;
      else
        outputPage.layout.data_mode.column_major = inputPage.layout.data_mode.column_major;
      for (iCol = 0; iCol < columns; iCol++) {
        if (!SDDS_GetColumnInformation(&inputPage, "units", &Units, SDDS_GET_BY_NAME, column[iCol]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!Units) {
          Units = (char *)malloc(sizeof(*Units));
          Units[0] = 0;
        }
        if (0 > SDDS_DefineColumn(&outputPage, intColumn[iCol], NULL, Units, NULL, NULL, SDDS_DOUBLE, 0))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        /* units for slopes columns */
        if (strlen(indVarUnits) && strlen(Units)) {
          slopeUnits = (char *)malloc(sizeof(*slopeUnits) * (strlen(Units) + strlen(indVarUnits) + 2));
          snprintf(slopeUnits, strlen(Units) + strlen(indVarUnits) + 2, "%s/%s", Units, indVarUnits);
        }
        if (strlen(indVarUnits) && !strlen(Units)) {
          slopeUnits = (char *)malloc(sizeof(*slopeUnits) * (strlen(indVarUnits) + 2));
          snprintf(slopeUnits, strlen(indVarUnits) + 2, "1/%s", indVarUnits);
        }
        if (!strlen(indVarUnits) && strlen(Units)) {
          slopeUnits = (char *)malloc(sizeof(*slopeUnits) * (strlen(Units) + 1));
          strcpy(slopeUnits, indVarUnits); // Fix this
        }
        if (!strlen(indVarUnits) && !strlen(Units)) {
          slopeUnits = (char *)malloc(sizeof(*slopeUnits));
          strcpy(slopeUnits, "");
        }
        if (0 > SDDS_DefineColumn(&outputPage, slopeColumn[iCol], NULL, slopeUnits, NULL, NULL, SDDS_DOUBLE, 0))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (doSlopeSigma) {
          if (0 > SDDS_DefineColumn(&outputPage, interceptSigmaColumn[iCol], NULL, Units, NULL, NULL, SDDS_DOUBLE, 0) ||
              0 > SDDS_DefineColumn(&outputPage, slopeSigmaColumn[iCol], NULL, slopeUnits, NULL, NULL, SDDS_DOUBLE, 0) ||
              0 > SDDS_DefineColumn(&outputPage, chiSquaredColumn[iCol], NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        free(slopeUnits);
      }
      if (!SDDS_WriteLayout(&outputPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!SDDS_StartTable(&outputPage, 1) ||
        !SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "InputFile", inputfile ? inputfile : "pipe", NULL) ||
        !SDDS_SetRowValues(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, 0, "IndependentVariable", indColumnName, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    /* determine which included columns has a Sigma column defined in the input file */
    if (ipage == 1) {
      sigmaColumn = (char **)malloc(sizeof(*sigmaColumn) * columns);
      sigmaColumnExists = (long *)malloc(columns * sizeof(*sigmaColumnExists));
      for (iCol = 0; iCol < columns; iCol++) {
        sigmaColumn[iCol] = (char *)malloc(sizeof(**sigmaColumn) * (strlen(column[iCol]) + strlen("Sigma") + 1));
        snprintf(sigmaColumn[iCol], strlen(column[iCol]) + strlen("Sigma") + 1, "%sSigma", column[iCol]);
        switch (SDDS_CheckColumn(&inputPage, sigmaColumn[iCol], NULL, SDDS_DOUBLE, NULL)) {
        case SDDS_CHECK_WRONGUNITS:
        case SDDS_CHECK_OKAY:
          sigmaColumnExists[iCol] = 1;
          break;
        default:
          /* try other possible spelling */
          snprintf(sigmaColumn[iCol], strlen("Sigma") + strlen(column[iCol]) + 1, "Sigma%s", column[iCol]);
          switch (SDDS_CheckColumn(&inputPage, sigmaColumn[iCol], NULL, SDDS_DOUBLE, NULL)) {
          case SDDS_CHECK_WRONGUNITS:
          case SDDS_CHECK_OKAY:
            sigmaColumnExists[iCol] = 1;
            break;
          default:
            sigmaColumnExists[iCol] = 0;
          }
          break;
        }
      }
    }

    if (ipage == 1) {
      weight = (double *)malloc(sizeof(*weight) * rows);
      diff = (double *)malloc(sizeof(*diff) * rows);
      order = 1;
      coef = (double *)malloc(sizeof(*coef) * (order + 1));
      coefsigma = (double *)malloc(sizeof(*coefsigma) * (order + 1));
      if (!weight || !diff || !coef || !coefsigma) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
      }
    } else {
      weight = (double *)realloc(weight, sizeof(*weight) * rows);
      diff = (double *)realloc(diff, sizeof(*diff) * rows);
      coef = (double *)realloc(coef, sizeof(*coef) * (order + 1));
      coefsigma = (double *)realloc(coefsigma, sizeof(*coefsigma) * (order + 1));
      if (!weight || !diff || !coef || !coefsigma) {
        fprintf(stderr, "Memory reallocation failed.\n");
        exit(EXIT_FAILURE);
      }
    }

    if (ipage == 1) {
      depVar = (double *)malloc(sizeof(*depVar) * rows);
      if (!depVar) {
        fprintf(stderr, "Memory allocation failed for depVar.\n");
        exit(EXIT_FAILURE);
      }
    } else {
      depVar = (double *)realloc(depVar, sizeof(*depVar) * rows);
      if (!depVar) {
        fprintf(stderr, "Memory reallocation failed for depVar.\n");
        exit(EXIT_FAILURE);
      }
    }
    for (iCol = 0; iCol < columns; iCol++) {
      if (verbose)
        fprintf(stderr, "Doing column %s.\n", column[iCol]);
      /* filter out the specified range in independent variable */
      if (xMin != xMax) {
        if (!(depVarOrig = (double *)SDDS_GetColumnInDoubles(&inputPage, column[iCol])))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        for (i = j = 0; i < rowsOrig; i++) {
          if (xMin <= indVarOrig[i] && indVarOrig[i] <= xMax) {
            depVar[j] = depVarOrig[i];
            j++;
          }
        }
        rows = j;
      } else {
        if (!(depVar = SDDS_GetColumnInDoubles(&inputPage, column[iCol])))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }

      /*********************
        three possibilities:
        1) don't do or write slope errors. (doSlopeSigma=0)
        do one lsf call with all weights = 1
        2) calculated slope errors from sigma columns in the input file.
        (doSlopeSigma=1 && generateSigma=0 && sigmaColumnExists[iCol]=1 )
        do one lsf call with weights from sigma columns
        3) calculate slope errors from generated sigma from a preliminary fit.
        (doSlopeSigma=1 && (generateSigma=1 || sigmaColumnExists[iCol]=NULL)
        do preliminary fit to generate sigma
      *********************/

      for (iRow = 0; iRow < rows; iRow++)
        weight[iRow] = 1;
      if (doSlopeSigma) {
        /*********************
          check validity of sigma column values
        *********************/
        if (!generateSigma && sigmaColumnExists[iCol]) {
          if (verbose)
            fprintf(stderr, "\tUsing column %s for sigma.\n", sigmaColumn[iCol]);
          if (!(weight = SDDS_GetColumnInDoubles(&inputPage, sigmaColumn[iCol])))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          if (minSigma > 0) {
            for (iRow = 0; iRow < rows; iRow++) {
              if (weight[iRow] < minSigma)
                weight[iRow] = minSigma;
            }
          }
          /* check for zero weight values which will give lsfn problems */
          validSigmas = rows;
          sigmaSum = 0;
          for (iRow = 0; iRow < rows; iRow++) {
            sigmaSum += weight[iRow];
            if (!weight[iRow]) {
              validSigmas--;
              /*
                fprintf(stderr,"Warning: %s of row number %ld is zero. Using average sigma.\n",sigmaColumn[iCol],iRow); */
            }
          }
          if (!validSigmas) {
            if (!noWarnings)
              fprintf(stderr, "Warning: All sigmas are zero.\n");
            doPreliminaryFit = 1;
          } else if (validSigmas != rows) {
            /* fix some sigmas */
            averageSigma = sigmaSum / validSigmas;
            if (!noWarnings)
              fprintf(stderr, "Warning: replacing %" PRId64 " invalid sigmas with average (%e)\n", rows - validSigmas, averageSigma);
            for (iRow = 0; iRow < rows; iRow++) {
              if (!weight[iRow]) {
                weight[iRow] = averageSigma;
              }
            }
          }
        } else {
          doPreliminaryFit = 1;
        }
      }

      if (doPreliminaryFit) {
        if (verbose)
          fprintf(stderr, "\tGenerating sigmas from rms residual of a preliminary fit.\n");
        if (!(lsfn(indVar, depVar, weight, rows, order, coef, coefsigma, &chi, diff))) {
          fprintf(stderr, "Problem with call to lsfn\n.");
          exit(EXIT_FAILURE);
        }
        rmsResidual = 0;
        /* calculate rms residual */
        for (iRow = 0; iRow < rows; iRow++) {
          rmsResidual += sqr(diff[iRow]);
        }
        rmsResidual = sqrt(rmsResidual / (rows));
        for (iRow = 0; iRow < rows; iRow++) {
          weight[iRow] = rmsResidual;
        }
        if (minSigma > 0) {
          for (iRow = 0; iRow < rows; iRow++) {
            if (weight[iRow] < minSigma)
              weight[iRow] = minSigma;
          }
        }
      }

      if (!(lsfn(indVar, depVar, weight, rows, order, coef, coefsigma, &chi, diff))) {
        fprintf(stderr, "Problem with call to lsfn\n.");
        exit(EXIT_FAILURE);
      }
      if (!SDDS_SetRowValues(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, 0,
                             intColumn[iCol], coef[0], slopeColumn[iCol], coef[1], NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (doSlopeSigma) {
        interceptSigma = coefsigma[0];
        slopeSigma = coefsigma[1];
        if (!SDDS_SetRowValues(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, 0,
                               chiSquaredColumn[iCol], chi, interceptSigmaColumn[iCol],
                               interceptSigma, slopeSigmaColumn[iCol], slopeSigma, NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (residualFile) {
        if (xMin != xMax) {
          /* calculate the residuals for the whole column explicitly since there
             are points outside the range of which the lsf call did not calculate
             the difference. */
          diffOrig = (double *)malloc(rowsOrig * sizeof(double));
          if (!diffOrig) {
            fprintf(stderr, "Memory allocation failed for diffOrig.\n");
            exit(EXIT_FAILURE);
          }
          for (i = 0; i < rowsOrig; i++) {
            diffOrig[i] = depVarOrig[i] - coef[0] - coef[1] * indVarOrig[i];
          }
          if (!SDDS_SetColumnFromDoubles(&residualPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                                         diffOrig, rowsOrig, column[iCol]))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          free(diffOrig);
        } else {
          if (!SDDS_SetColumnFromDoubles(&residualPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                                         diff, rows, column[iCol]))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    }

    if (residualFile) {
      if (!SDDS_WriteTable(&residualPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!SDDS_WriteTable(&outputPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (residualFile) {
    if (!SDDS_Terminate(&residualPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&inputPage) || !SDDS_Terminate(&outputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile))
    exit(EXIT_FAILURE);

  /* Free allocated memory */
  free(indVar);
  free(indVarOrig);
  free(depVar);
  free(depVarOrig);
  free(coef);
  free(coefsigma);
  free(weight);
  free(diff);
  if (intColumn) {
    for (i = 0; i < columns; i++) {
      free(intColumn[i]);
      free(slopeColumn[i]);
      if (doSlopeSigma) {
        free(slopeSigmaColumn[i]);
        free(interceptSigmaColumn[i]);
        free(chiSquaredColumn[i]);
      }
      free(sigmaColumn[i]);
    }
    free(intColumn);
    free(slopeColumn);
    if (doSlopeSigma) {
      free(slopeSigmaColumn);
      free(interceptSigmaColumn);
      free(chiSquaredColumn);
    }
    free(sigmaColumn);
    free(sigmaColumnExists);
  }
  free(column);
  if (excludeColumn && excludeColumn != defaultExcludedColumn)
    free(excludeColumn);

  return EXIT_SUCCESS;
}

long set_multicolumn_flags(SDDS_TABLE *SDDSin, char ***column, int32_t *columns, char **exclude, long excludes) {
  long index, i;

  if (*column) {
    SDDS_SetColumnFlags(SDDSin, 0);
    for (i = 0; i < *columns; i++) {
      if (!SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, (*column)[i], SDDS_OR))
        return 0;
    }
  } else {
    SDDS_SetColumnFlags(SDDSin, 1);
    if (!(*column = SDDS_GetColumnNames(SDDSin, columns)) || *columns == 0) {
      SDDS_SetError("no columns found");
      return 0;
    }
    for (i = 0; i < *columns; i++) {
      index = SDDS_GetColumnIndex(SDDSin, (*column)[i]);
      if (!SDDS_NUMERIC_TYPE(SDDS_GetColumnType(SDDSin, index)) &&
          !SDDS_AssertColumnFlags(SDDSin, SDDS_INDEX_LIMITS, index, index, 0))
        return 0;
    }
  }
  free(*column);

  for (i = 0; i < excludes; i++)
    if (!SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, exclude[i], SDDS_NEGATE_MATCH | SDDS_AND))
      return 0;

  if (!(*column = SDDS_GetColumnNames(SDDSin, columns)) || *columns == 0) {
    SDDS_SetError("Selected columns not found.");
    return 0;
  }

  return 1;
}
