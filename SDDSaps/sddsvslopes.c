/**
 * @file sddsvslopes.c
 * @brief Computes straight-line fits (slopes and intercepts) of column data in SDDS experiment output files.
 *
 * @details
 * This program reads an SDDS input file containing multiple datasets with vectorized column data and a defined independent variable.
 * It performs least squares fitting (LSF) for specified columns against the independent variable across rows and pages.
 * The results, including slopes and intercepts (and optionally slope errors), are written to an output SDDS file.
 *
 * - The independent variable must be a defined parameter.
 * - The output file contains one table of slopes and intercepts.
 * - The independent parameter of the input file is removed in the output file, but its name is converted to a parameter string.
 *
 * @section Usage
 * ```
 * sddsvslopes [<inputfile>] [<outputfile>]
 *             [-pipe=[input][,output]]
 *              -independentVariable=<parametername>
 *             [-columns=<list-of-names>] 
 *             [-excludeColumns=<list-of-names>] 
 *             [-sigma] 
 *             [-verbose] 
 *             [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                    |
 * |---------------------------------------|-----------------------------------------------|
 * | `-independentVariable`                | Specifies the name of the independent variable parameter. |
 *
 * | Optional                              | Description                                    |
 * |---------------------------------------|-----------------------------------------------|
 * | `-pipe`                               | Read input or output from a pipe.             |
 * | `-columns`                            | Columns to perform straight-line fitting.     |
 * | `-excludeColumns`                     | Columns to exclude from fitting.              |
 * | `-sigma`                              | Generate errors for slopes using sigma columns.|
 * | `-verbose`                            | Print detailed output to stderr.              |
 * | `-majorOrder`                         | Specify output file in row or column major order. |
 *
 * @subsection spec_req Specific Requirements
 *   - If `-excludeColumns` is not provided, default columns `Index`, `ElapsedTime`, and `Rootname` are excluded.
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
  CLO_INDEPENDENT_PARAMETER,
  CLO_COLUMNS,
  CLO_EXCLUDE,
  CLO_VERBOSE,
  CLO_SDDS_OUTPUT_ROOT,
  CLO_SLOPE_ERRORS,
  CLO_PIPE,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

char *commandline_option[N_OPTIONS] = {
  "independentVariable",
  "columns",
  "excludeColumns",
  "verbose",
  "sddsOutputRoot",
  "sigma",
  "pipe",
  "majorOrder",
};

#define DEFAULT_EXCLUDED_COLUMNS 3
char *defaultExcludedColumn[DEFAULT_EXCLUDED_COLUMNS] = {
  "Index",
  "ElapsedTime",
  "Rootname"};

#define DEFAULT_COPY_COLUMNS 2
char *defaultCopyColumn[DEFAULT_COPY_COLUMNS] = {
  "Index",
  "Rootname"};

static char *USAGE =
  "sddsvslopes [<inputfile>] [<outputfile>]\n"
  "            [-pipe=[input][,output]]\n"
  "             -independentVariable=<parametername>\n"
  "            [-columns=<list-of-names>] \n"
  "            [-excludeColumns=<list-of-names>] \n"
  "            [-sigma] \n"
  "            [-verbose] \n"
  "            [-majorOrder=row|column]\n"
  "Options:\n"
  "  -pipe=[input][,output]            Read input or output from a pipe.\n"
  "  -independentVariable=<name>       Name of the independent variable parameter.\n"
  "  -columns=<list-of-names>          Columns to perform straight line fitting.\n"
  "  -excludeColumns=<list-of-names>   Columns to exclude from fitting.\n"
  "  -sigma                            Generate errors for slopes using sigma columns.\n"
  "  -verbose                          Print detailed output to stderr.\n"
  "  -majorOrder=row|column            Specify output file in row or column major order.\n"
  "\nDescription:\n"
  "  Computes straight line fits of column data in the input SDDS file using a specified\n"
  "  independent variable parameter. The output file contains tables of slopes and intercepts.\n"
  "  The independent parameter is removed from the output file and its name is converted\n"
  "  to a parameter string.\n"
  "\nAuthor:\n"
  "  Louis Emery, ANL (Date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

long set_multicolumn_flags(SDDS_TABLE *SDDSin, char ***column, int32_t *columns, char **exclude, long excludes);

int main(int argc, char **argv) {
  SCANNED_ARG *scanned;
  SDDS_TABLE inputPage, *copiedPage, outputPage;
  long copiedPages;
  char *inputfile, *outputfile;
  char **column, **excludeColumn = NULL;
  int32_t columns;
  long excludeColumns;
  char *indParameterName;
  char **copyColumn;
  int32_t copyColumns;
  long verbose;
  long slopeErrors;
  long iArg, i;
  double *indVar;
  char *indVarUnits;
  char **intColumn, **slopeColumn, **slopeSigmaColumn;
  char *Units, *slopeUnits;
  double *depVar;
  long order;
  double *coef, *coefsigma, *weight, *diff, chi;
  long iCol, iPage;
  int64_t rows, iRow;
  double *slope, slope2, slopeAve, slopeSigma;
  unsigned long pipeFlags, majorOrderFlag;
  long tmpfile_used, noWarnings;
  long generateIndex;
  short columnMajorOrder = -1;

  copiedPage = NULL;
  slopeSigmaColumn = NULL;
  slopeUnits = Units = indVarUnits = NULL;
  rows = 0;
  slope = NULL;
  slope2 = 0;
  coef = coefsigma = weight = diff = slope = NULL;

  argc = scanargs(&scanned, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  inputfile = outputfile = NULL;
  columns = excludeColumns = 0;
  column = excludeColumn = NULL;
  indParameterName = NULL;
  verbose = 0;
  slopeErrors = 0;
  pipeFlags = 0;
  tmpfile_used = 0;
  noWarnings = 0;
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
      case CLO_INDEPENDENT_PARAMETER:
        if (!(indParameterName = scanned[iArg].list[1]))
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
          SDDS_Bomb("only one -excludeColumns option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -excludeColumns syntax");
        excludeColumn = tmalloc(sizeof(*excludeColumn) * (excludeColumns = scanned[iArg].n_items - 1));
        for (i = 0; i < excludeColumns; i++)
          excludeColumn[i] = scanned[iArg].list[i + 1];
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_SLOPE_ERRORS:
        slopeErrors = 1;
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

  processFilenames("sddsvslopes", &inputfile, &outputfile, pipeFlags, noWarnings, &tmpfile_used);

  if (!indParameterName)
    SDDS_Bomb("independentVariable not given");

  if (!excludeColumns) {
    excludeColumn = defaultExcludedColumn;
    excludeColumns = DEFAULT_EXCLUDED_COLUMNS;
  }

  if (verbose)
    fprintf(stderr, "Reading file %s.\n", inputfile);
  SDDS_InitializeInput(&inputPage, inputfile);
  copiedPages = 0;
  while (SDDS_ReadTable(&inputPage) > 0) {
    if (!copiedPages) {
      copiedPage = (SDDS_TABLE *)malloc(sizeof(SDDS_TABLE));
      rows = SDDS_CountRowsOfInterest(&inputPage);
    } else {
      copiedPage = (SDDS_TABLE *)realloc(copiedPage, (copiedPages + 1) * sizeof(SDDS_TABLE));
    }
    if (!SDDS_InitializeCopy(&copiedPage[copiedPages], &inputPage, NULL, "m"))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (!SDDS_CopyTable(&copiedPage[copiedPages], &inputPage))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    copiedPages++;
  }
  if (copiedPages < 2) {
    fprintf(stderr, "Insufficient data (i.e., number of data pages) to fit a straight line.\n");
    exit(EXIT_FAILURE);
  }
  switch (SDDS_CheckColumn(&inputPage, "Rootname", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_WRONGUNITS:
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s.\n", "Rootname");
    exit(EXIT_FAILURE);
  }
  switch (SDDS_CheckColumn(&inputPage, "Index", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
  case SDDS_CHECK_WRONGUNITS:
  case SDDS_CHECK_OKAY:
    generateIndex = 0;
    break;
  case SDDS_CHECK_NONEXISTENT:
    generateIndex = 1;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s.\n", "Index");
    exit(EXIT_FAILURE);
  }
  /****************\
   * make array of independent variable
   \**************/
  indVar = (double *)malloc(sizeof(*indVar) * copiedPages);
  switch (SDDS_CheckParameter(&inputPage, indParameterName, NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_WRONGUNITS:
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with parameter %s.\n", indParameterName);
    exit(EXIT_FAILURE);
  }
  for (iPage = 0; iPage < copiedPages; iPage++) {
    if (!SDDS_GetParameter(&copiedPage[iPage], indParameterName, &indVar[iPage]))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (!SDDS_GetParameterInformation(&inputPage, "units", &indVarUnits, SDDS_GET_BY_NAME, indParameterName))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!indVarUnits) {
    indVarUnits = (char *)malloc(sizeof(*indVarUnits));
    indVarUnits[0] = 0;
  }
  /************************************\
   * get columns of interest. use set_multicolumn_flags to simply
   * return new values for array column.
   \*************************************/
  if (!set_multicolumn_flags(&inputPage, &column, &columns, excludeColumn, excludeColumns)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  /************************************\
   * make column names for the output
   \*************************************/
  intColumn = (char **)malloc((sizeof(char *) * columns));
  slopeColumn = (char **)malloc((sizeof(char *) * columns));
  if (slopeErrors)
    slopeSigmaColumn = (char **)malloc((sizeof(char *) * columns));
  for (i = 0; i < columns; i++) {
    intColumn[i] = (char *)malloc((sizeof(char) * (strlen(column[i]) + strlen("Intercept") + 1)));
    strcat(strcpy(intColumn[i], column[i]), "Intercept");
    slopeColumn[i] = (char *)malloc((sizeof(char) * (strlen(column[i]) + strlen("Slope") + 1)));
    strcat(strcpy(slopeColumn[i], column[i]), "Slope");
    if (slopeErrors) {
      slopeSigmaColumn[i] = (char *)malloc((sizeof(char) * (strlen(column[i]) + strlen("SlopeSigma") + 1)));
      strcat(strcpy(slopeSigmaColumn[i], column[i]), "SlopeSigma");
    }
  }
  /************************************\
   * Write layout for output file
   \*************************************/
  if (verbose)
    fprintf(stderr, "Opening file %s.\n", outputfile);
  if (!SDDS_InitializeOutput(&outputPage, SDDS_BINARY, 1, "lsf of sddsvexperiment", NULL, outputfile) ||
      0 > SDDS_DefineParameter(&outputPage, "InputFile", "InputFile", NULL, "InputFile", NULL, SDDS_STRING, 0) ||
      0 > SDDS_DefineParameter(&outputPage, "IndependentVariable", "IndependentVariable", NULL, "IndependentVariable", NULL, SDDS_STRING, 0) ||
      (0 > SDDS_DefineColumn(&outputPage, "Index", NULL, NULL, "Rootname index", NULL, SDDS_LONG64, 0)) ||
      (0 > SDDS_DefineColumn(&outputPage, "Rootname", NULL, NULL, NULL, NULL, SDDS_STRING, 0)))
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
      strcat(strcat(strcpy(slopeUnits, Units), "/"), indVarUnits);
    }
    if (strlen(indVarUnits) && !strlen(Units)) {
      slopeUnits = (char *)malloc(sizeof(*slopeUnits) * (strlen(indVarUnits) + 2));
      strcat(strcpy(slopeUnits, "1/"), indVarUnits);
    }
    if (!strlen(indVarUnits) && strlen(Units)) {
      slopeUnits = (char *)malloc(sizeof(*slopeUnits) * (strlen(Units) + 2));
      strcpy(slopeUnits, Units);
    }
    if (!strlen(indVarUnits) && !strlen(Units)) {
      slopeUnits = (char *)malloc(sizeof(*slopeUnits));
      slopeUnits[0] = '\0';
    }
    if (0 > SDDS_DefineColumn(&outputPage, slopeColumn[iCol], NULL, slopeUnits, NULL, NULL, SDDS_DOUBLE, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (slopeErrors) {
      if (0 > SDDS_DefineColumn(&outputPage, slopeSigmaColumn[iCol], NULL, slopeUnits, NULL, NULL, SDDS_DOUBLE, 0))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    free(slopeUnits);
  }
  if (!SDDS_WriteLayout(&outputPage) ||
      !SDDS_StartTable(&outputPage, rows) ||
      !SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "InputFile", inputfile ? inputfile : "pipe", NULL) ||
      !SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, 0, "IndependentVariable", indParameterName, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  /************************************\
   * Copy columns to output file (usually columns Index and Rootname)
   \*************************************/
  copyColumns = DEFAULT_COPY_COLUMNS;
  copyColumn = defaultCopyColumn;
  if (!set_multicolumn_flags(&inputPage, &copyColumn, &copyColumns, NULL, 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!SDDS_CopyColumns(&outputPage, &inputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  depVar = (double *)malloc(sizeof(*depVar) * copiedPages);
  weight = (double *)malloc(sizeof(*weight) * copiedPages);
  diff = (double *)malloc(sizeof(*diff) * copiedPages);
  order = 1;
  coef = (double *)malloc(sizeof(*coef) * (order + 1));
  coefsigma = (double *)malloc(sizeof(*coefsigma) * (order + 1));
  if (slopeErrors)
    slope = (double *)malloc(sizeof(*slope) * copiedPages);
  for (iCol = 0; iCol < columns; iCol++) {
    for (iPage = 0; iPage < copiedPages; iPage++)
      weight[iPage] = 1;
    if (verbose)
      fprintf(stderr, "Doing column %s.\n", column[iCol]);
    for (iRow = 0; iRow < rows; iRow++) {
      for (iPage = 0; iPage < copiedPages; iPage++) {
        if (!SDDS_GetValue(&copiedPage[iPage], column[iCol], iRow, &depVar[iPage]))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      if (!(lsfn(indVar, depVar, weight, copiedPages, order, coef, coefsigma, &chi, diff))) {
        fprintf(stderr, "Problem with call to lsfn.\n");
        exit(EXIT_FAILURE);
      }
      if (generateIndex) {
        if (!SDDS_SetRowValues(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, iRow, "Index", iRow, intColumn[iCol], coef[0], slopeColumn[iCol], coef[1], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (!SDDS_SetRowValues(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, iRow, intColumn[iCol], coef[0], slopeColumn[iCol], coef[1], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (slopeErrors) {
        /* recalculate the slope with a subset of points */
        slopeAve = slope2 = 0;
        for (iPage = 0; iPage < copiedPages; iPage++) {
          weight[iPage] = 1e10;
          if (iPage)
            weight[iPage - 1] = 1;
          if (!(lsfn(indVar, depVar, weight, copiedPages, order, coef, coefsigma, &chi, diff))) {
            fprintf(stderr, "Problem with call to lsfn.\n");
            exit(EXIT_FAILURE);
          }
          slope[iPage] = coef[1];
          slopeAve += slope[iPage];
          slope2 += slope[iPage] * slope[iPage];
        }
        slopeSigma = sqrt(slope2 / copiedPages - (slopeAve / copiedPages) * (slopeAve / copiedPages));
        if (!SDDS_SetRowValues(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, iRow, slopeSigmaColumn[iCol], slopeSigma, NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }

  if (!SDDS_WriteTable(&outputPage) || SDDS_Terminate(&inputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (iPage = 0; iPage < copiedPages; iPage++) {
    if (!SDDS_Terminate(&copiedPage[iPage]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (SDDS_Terminate(&outputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile))
    exit(EXIT_FAILURE);
  SDDS_FreeStringArray(column, columns);
  free(column);
  SDDS_FreeStringArray(intColumn, columns);
  SDDS_FreeStringArray(slopeColumn, columns);
  free(intColumn);
  free(slopeColumn);
  if (slopeErrors) {
    SDDS_FreeStringArray(slopeSigmaColumn, columns);
    free(slopeSigmaColumn);
  }

  free(copiedPage);
  free(indVar);
  free(depVar);
  if (weight)
    free(weight);
  if (diff)
    free(diff);
  if (slope)
    free(slope);
  if (coef)
    free(coef);
  if (coefsigma)
    free(coefsigma);
  if (Units)
    free(Units);
  if (indVarUnits)
    free(indVarUnits);

  free_scanargs(&scanned, argc);

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
    free(*column);
    *column = NULL;
  }

  for (i = 0; i < excludes; i++)
    if (!SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, exclude[i], SDDS_NEGATE_MATCH | SDDS_AND))
      return 0;

  if (!(*column = SDDS_GetColumnNames(SDDSin, columns)) || *columns == 0) {
    SDDS_SetError("Selected columns not found.");
    return 0;
  }

  return 1;
}
