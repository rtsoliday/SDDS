/**
 * @file sddsderiv.c
 * @brief Calculates derivatives of specified columns in an SDDS data set.
 *
 * @details
 * This program calculates the derivative of specified columns in an SDDS data set. 
 * It supports various options including handling error columns, Savitzky-Golay filtering, 
 * and customizable output templates. The templates allow users to customize names, 
 * symbols, and descriptions for both primary and error derivatives.
 *
 * @section Usage
 * ```
 * sddsderiv [<input>] [<output>]
 *           [-pipe=[input][,output]]
 *            -differentiate=<column-name>[,<sigma-name>]...
 *           [-exclude=<column-name>[,...]]
 *            -versus=<column-name>
 *           [-interval=<integer>]
 *           [-SavitzkyGolay=<left>,<right>,<fitOrder>[,<derivOrder>]]
 *           [-mainTemplates=<item>=<string>[,...]]
 *           [-errorTemplates=<item>=<string>[,...]]
 *           [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                               |
 * |---------------------------------------|---------------------------------------------------------------------------|
 * | `-differentiate`                      | Specifies the columns to differentiate, optionally specifying sigma.      |
 * | `-versus`                             | Specifies the column to differentiate with respect to.                    |
 *
 * | Optional                              | Description                                                               |
 * |---------------------------------------|---------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard input/output.                                                |
 * | `-exclude`                            | Exclude specific columns from differentiation.                            |
 * | `-interval`                           | Set interval for finite difference method.                                |
 * | `-SavitzkyGolay`                      | Apply Savitzky-Golay filter with the specified parameters.  |
 * | `-mainTemplates`                      | Templates for main output columns.                                        |
 * | `-errorTemplates`                     | Templates for error output columns.                                       |
 * | `-majorOrder`                         | Specifies the major order of the data layout.                             |
 *
 * @subsection Incompatibilities
 *   - `-interval` is incompatible with:
 *     - `-SavitzkyGolay`
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "SDDSutils.h"
#include "scan.h"
#include <ctype.h>

static char *USAGE =
  "sddsderiv [<input>] [<output>]\n"
  "          [-pipe=[input][,output]]\n"
  "           -differentiate=<column-name>[,<sigma-name>] ...\n"
  "          [-exclude=<column-name>[,...]]\n"
  "           -versus=<column-name>\n"
  "          [-interval=<integer>]\n"
  "          [-SavitzkyGolay=<left>,<right>,<fitOrder>[,<derivOrder>]]\n"
  "          [-mainTemplates=<item>=<string>[,...]]\n"
  "          [-errorTemplates=<item>=<string>[,...]]\n"
  "          [-majorOrder=row|column]\n\n"
  "Options:\n"
  "  -pipe=[input][,output]                      Use standard input/output.\n"
  "  -differentiate=<col>[,<sigma-col>] ...       Columns to differentiate, optionally specifying sigma columns.\n"
  "  -exclude=<col>[,...]                        Columns to exclude from differentiation.\n"
  "  -versus=<col>                               Column to differentiate with respect to.\n"
  "  -interval=<integer>                         Interval for finite difference.\n"
  "  -SavitzkyGolay=<left>,<right>,<fitOrder>[,<derivOrder>]\n"
  "                                              Apply Savitzky-Golay filter with specified parameters.\n"
  "  -mainTemplates=<item>=<string>[,...]        Templates for main output columns. Items: name, symbol, description.\n"
  "  -errorTemplates=<item>=<string>[,...]       Templates for error output columns. Items: name, symbol, description.\n"
  "  -majorOrder=row|column                      Set major order of data.\n\n"
  "The -templates <item> may be \"name\", \"symbol\" or \"description\".\n"
  "The default main name, description, and symbol templates are \"%yNameDeriv\",\n"
  " \"Derivative w.r.t %xSymbol of %ySymbol\", and \"d[%ySymbol]/d[%xSymbol]\", respectively.\n"
  "The default error name, description, and symbol templates are \"%yNameDerivSigma\",\n"
  " \"Sigma of derivative w.r.t %xSymbol of %ySymbol\", and \"Sigma[d[%ySymbol]/d[%xSymbol]]\", respectively.\n"
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")";

/* Enumeration for option types */
enum option_type {
  CLO_DIFFERENTIATE,
  CLO_VERSUS,
  CLO_INTERVAL,
  CLO_MAINTEMPLATE,
  CLO_ERRORTEMPLATE,
  CLO_PIPE,
  CLO_EXCLUDE,
  CLO_SAVITZKYGOLAY,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "differentiate",
  "versus",
  "interval",
  "maintemplate",
  "errortemplate",
  "pipe",
  "exclude",
  "savitzkygolay",
  "majorOrder",
};

long checkErrorNames(char **yErrorName, long nDerivatives);
void makeSubstitutions(char *buffer1, char *buffer2, char *template, char *nameRoot, char *symbolRoot, char *xName, char *xSymbol);
char *changeInformation(SDDS_DATASET *SDDSout, char *name, char *nameRoot, char *symbolRoot, char *xName, char *xSymbol, char **template, char *newUnits);
long setupOutputFile(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *output,
                     char ***yOutputName, char ***yOutputErrorName, char ***yOutputUnits, char *xName, char *xErrorName, char **yName, char **yErrorName, long yNames, char **mainTemplate, char **errorTemplate, int32_t interval, long order, short columnMajorOrder);
long findDerivIndices(int64_t *i1, int64_t *i2, long interval, int64_t i, int64_t rows);
void takeDerivative(double *x, double *y, double *sy, int64_t rows, double *deriv, double *derivSigma, double *derivPosition, long interval);
void takeSGDerivative(double *x, double *y, int64_t rows, double *deriv, double *derivPosition, long left, long right, long sgOrder, long derivOrder);

int main(int argc, char **argv) {
  double *xData, *yData, *xError, *yError, *derivative, *derivativeError, *derivativePosition;
  char *input, *output, *xName, *xErrorName, **yName, **yErrorName, **yOutputName, **yOutputErrorName, *ptr;
  char **yOutputUnits, **yExcludeName;
  char *mainTemplate[3] = {NULL, NULL, NULL};
  char *errorTemplate[3] = {NULL, NULL, NULL};
  long i, iArg, yNames, yExcludeNames;
  int64_t rows;
  int32_t interval;
  SDDS_DATASET SDDSin, SDDSout;
  SCANNED_ARG *scanned;
  unsigned long flags, pipeFlags, majorOrderFlag;
  long SGLeft, SGRight, SGOrder, SGDerivOrder, intervalGiven, yErrorsSeen;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scanned, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  input = output = xName = xErrorName = NULL;
  yName = yErrorName = yExcludeName = NULL;
  derivative = derivativeError = derivativePosition = yError = yData = xData = xError = NULL;
  yNames = yExcludeNames = 0;
  pipeFlags = 0;
  interval = 2;
  SGOrder = -1;
  SGDerivOrder = 1;
  intervalGiven = 0;
  yErrorsSeen = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 && (!scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_DIFFERENTIATE:
        if (scanned[iArg].n_items != 2 && scanned[iArg].n_items != 3)
          SDDS_Bomb("invalid -differentiate syntax");
        yName = SDDS_Realloc(yName, sizeof(*yName) * (yNames + 1));
        yErrorName = SDDS_Realloc(yErrorName, sizeof(*yErrorName) * (yNames + 1));
        yName[yNames] = scanned[iArg].list[1];
        if (scanned[iArg].n_items == 3) {
          yErrorsSeen = 1;
          yErrorName[yNames] = scanned[iArg].list[2];
        } else
          yErrorName[yNames] = NULL;
        yNames++;
        break;
      case CLO_EXCLUDE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -exclude syntax");
        moveToStringArray(&yExcludeName, &yExcludeNames, scanned[iArg].list + 1, scanned[iArg].n_items - 1);
        break;
      case CLO_VERSUS:
        if (xName)
          SDDS_Bomb("give -versus only once");
        if (scanned[iArg].n_items != 2)
          SDDS_Bomb("invalid -versus syntax");
        xName = scanned[iArg].list[1];
        xErrorName = NULL;
        break;
      case CLO_MAINTEMPLATE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -mainTemplate syntax");
        scanned[iArg].n_items--;
        if (!scanItemList(&flags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "name", SDDS_STRING, mainTemplate + 0, 1, 0, "description", SDDS_STRING, mainTemplate + 1, 1, 0, "symbol", SDDS_STRING, mainTemplate + 2, 1, 0, NULL))
          SDDS_Bomb("invalid -mainTemplate syntax");
        break;
      case CLO_ERRORTEMPLATE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -errorTemplate syntax");
        scanned[iArg].n_items--;
        if (!scanItemList(&flags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "name", SDDS_STRING, errorTemplate + 0, 1, 0, "description", SDDS_STRING, errorTemplate + 1, 1, 0, "symbol", SDDS_STRING, errorTemplate + 2, 1, 0, NULL))
          SDDS_Bomb("invalid -errorTemplate syntax");
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_INTERVAL:
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%" SCNd32, &interval) != 1 || interval <= 0)
          SDDS_Bomb("invalid -interval syntax/value");
        intervalGiven = 1;
        break;
      case CLO_SAVITZKYGOLAY:
        if ((scanned[iArg].n_items != 4 && scanned[iArg].n_items != 5) ||
            sscanf(scanned[iArg].list[1], "%ld", &SGLeft) != 1 ||
            sscanf(scanned[iArg].list[2], "%ld", &SGRight) != 1 ||
            sscanf(scanned[iArg].list[3], "%ld", &SGOrder) != 1 ||
            (scanned[iArg].n_items == 5 && sscanf(scanned[iArg].list[4], "%ld", &SGDerivOrder) != 1) ||
            SGLeft < 0 ||
            SGRight < 0 ||
            (SGLeft + SGRight) < SGOrder ||
            SGOrder < 0 ||
            SGDerivOrder < 0)
          SDDS_Bomb("invalid -SavitzkyGolay syntax/values");
        break;
      default:
        fprintf(stderr, "invalid option seen: %s\n", scanned[iArg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  if (intervalGiven && SGOrder >= 0)
    SDDS_Bomb("-interval and -SavitzkyGolay options are incompatible");
  if (SGOrder >= 0 && (xErrorName || yErrorsSeen))
    SDDS_Bomb("Savitzky-Golay method does not support errors in data");

  processFilenames("sddsderiv", &input, &output, pipeFlags, 0, NULL);

  if (!yNames)
    SDDS_Bomb("-differentiate option must be given at least once");
  if (!checkErrorNames(yErrorName, yNames))
    SDDS_Bomb("either all -differentiate quantities must have errors, or none");

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!(ptr = SDDS_FindColumn(&SDDSin, FIND_NUMERIC_TYPE, xName, NULL))) {
    fprintf(stderr, "error: column %s doesn't exist\n", xName);
    exit(EXIT_FAILURE);
  }
  free(xName);
  xName = ptr;
  if (xErrorName) {
    if (!(ptr = SDDS_FindColumn(&SDDSin, FIND_NUMERIC_TYPE, xErrorName, NULL))) {
      fprintf(stderr, "error: column %s doesn't exist\n", xErrorName);
      exit(EXIT_FAILURE);
    } else {
      free(xErrorName);
      xErrorName = ptr;
    }
  }

  if (!(yNames = expandColumnPairNames(&SDDSin, &yName, &yErrorName, yNames, yExcludeName, yExcludeNames, FIND_NUMERIC_TYPE, 0))) {
    fprintf(stderr, "error: no quantities to differentiate found in file\n");
    exit(EXIT_FAILURE);
  }

  setupOutputFile(&SDDSout, &SDDSin, output, &yOutputName, &yOutputErrorName, &yOutputUnits, xName, xErrorName, yName, yErrorName, yNames, mainTemplate, errorTemplate, interval, SGOrder >= 0 ? SGDerivOrder : 1, columnMajorOrder);

  while (SDDS_ReadPage(&SDDSin) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&SDDSin)) < 2)
      SDDS_Bomb("Can't compute derivatives: too little data.");
    derivative = SDDS_Realloc(derivative, sizeof(*derivative) * rows);
    derivativeError = SDDS_Realloc(derivativeError, sizeof(*derivativeError) * rows);
    derivativePosition = SDDS_Realloc(derivativePosition, sizeof(*derivativePosition) * rows);
    if (!SDDS_StartPage(&SDDSout, rows) || !SDDS_CopyParameters(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    xError = NULL;
    if (!(xData = SDDS_GetColumnInDoubles(&SDDSin, xName)) ||
        (xErrorName && !(xError = SDDS_GetColumnInDoubles(&SDDSin, xErrorName))))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < yNames; i++) {
      yError = NULL;
      if (!(yData = SDDS_GetColumnInDoubles(&SDDSin, yName[i])) ||
          (yErrorName && yErrorName[i] && !(yError = SDDS_GetColumnInDoubles(&SDDSin, yErrorName[i]))))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (SGOrder >= 0)
        takeSGDerivative(xData, yData, rows, derivative, derivativePosition, SGLeft, SGRight, SGOrder, SGDerivOrder);
      else
        takeDerivative(xData, yData, yError, rows, derivative, derivativeError, derivativePosition, interval);
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, derivative, rows, yOutputName[i]) ||
          (yOutputErrorName && yOutputErrorName[i] && !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, derivativeError, rows, yOutputErrorName[i])))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (yData)
        free(yData);
      if (yError)
        free(yError);
      yData = yError = NULL;
    }
    if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, derivativePosition, rows, xName) ||
        (xErrorName && !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, xError, rows, xErrorName)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (xData)
      free(xData);
    if (xError)
      free(xError);
    xData = xError = NULL;
  }
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (derivative)
    free(derivative);
  if (derivativeError)
    free(derivativeError);
  if (derivativePosition)
    free(derivativePosition);
  return EXIT_SUCCESS;
}

void takeSGDerivative(double *x, double *y, int64_t rows, double *deriv, double *derivPosition, long left, long right, long sgOrder, long derivOrder) {
  int64_t i;
  double spacing, df;

  spacing = (x[rows - 1] - x[0]) / (rows - 1);
  df = dfactorial(derivOrder) / ipow(spacing, derivOrder);
  for (i = 0; i < rows; i++) {
    derivPosition[i] = x[i];
    deriv[i] = df * y[i];
  }
  SavitzkyGolaySmooth(deriv, rows, sgOrder, left, right, derivOrder);
}

void takeDerivative(double *x, double *y, double *sy, int64_t rows, double *deriv, double *derivSigma, double *derivPosition, long interval) {
  int64_t i, i1, i2;
  double dx;

  if (sy) {
    for (i = 0; i < rows; i++) {
      if (findDerivIndices(&i1, &i2, interval, i, rows) && (dx = x[i2] - x[i1])) {
        deriv[i] = (y[i2] - y[i1]) / dx;
        derivSigma[i] = sqrt(sqr(sy[i1]) + sqr(sy[i2])) / fabs(dx);
        derivPosition[i] = (x[i2] + x[i1]) / 2;
      } else
        deriv[i] = derivSigma[i] = derivPosition[i] = DBL_MAX;
    }
  } else {
    for (i = 0; i < rows; i++) {
      if (findDerivIndices(&i1, &i2, interval, i, rows) && (dx = x[i2] - x[i1])) {
        deriv[i] = (y[i2] - y[i1]) / dx;
        derivPosition[i] = (x[i2] + x[i1]) / 2;
      } else
        deriv[i] = derivPosition[i] = DBL_MAX;
    }
  }
}

long findDerivIndices(int64_t *i1, int64_t *i2, long interval, int64_t i, int64_t rows) {
  int64_t index, rows1;

  rows1 = rows - 1;
  *i1 = index = i - interval / 2;
  if (index < 0) {
    *i2 = 2 * i;
    *i1 = 0;
    if (*i1 == *i2)
      *i2 += 1;
    if (*i2 < rows)
      return 1;
    return 0;
  }
  *i2 = i + interval / 2;
  index = rows1 - *i2;
  if (index < 0) {
    *i1 = 2 * i - rows1;
    *i2 = rows1;
    if (*i1 == *i2)
      *i1 -= 1;
    if (*i1 >= 0)
      return 1;
    return 0;
  }
  if (*i1 == *i2) {
    if (*i2 < rows1)
      *i2 = *i1 + 1;
    else
      *i1 = *i1 - 1;
  }
  if (*i2 < rows && *i1 >= 0)
    return 1;
  return 0;
}

long setupOutputFile(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *output,
                     char ***yOutputName, char ***yOutputErrorName, char ***yOutputUnits, char *xName, char *xErrorName, char **yName, char **yErrorName, long yNames, char **mainTemplate0, char **errorTemplate0, int32_t interval, long order, short columnMajorOrder) {
  long i;
  char *xSymbol, *ySymbol;
  char *mainTemplate[3] = {"%yNameDeriv", "Derivative w.r.t. %xSymbol of %ySymbol", "d[%ySymbol]/d[%xSymbol]"};
  char *errorTemplate[3] = {"%yNameDerivSigma", "Sigma of derivative w.r.t. %xSymbol of %ySymbol",
    "Sigma[d[%ySymbol]/d[%xSymbol]]"};
  char buffer[1024];

  for (i = 0; i < 3; i++) {
    if (!mainTemplate0[i]) {
      if (order != 1) {
        switch (i) {
        case 0:
          /* name */
          snprintf(buffer, sizeof(buffer), "%%yNameDeriv%ld", order);
          break;
        case 1:
          /* description */
          snprintf(buffer, sizeof(buffer), "Derivative %ld w.r.t. %%xSymbol of %%ySymbol", order);
          break;
        case 2:
          /* symbol */
          snprintf(buffer, sizeof(buffer), "d[%s%ld]/d[%s]", "ySymbol", order, "xSymbol"); // Adjusted for clarity
          break;
        }
        cp_str(&mainTemplate[i], buffer);
      }
    } else {
      mainTemplate[i] = mainTemplate0[i];
    }
    if (errorTemplate0[i]) {
      errorTemplate[i] = errorTemplate0[i];
    }
  }

  *yOutputName = tmalloc(sizeof(**yOutputName) * yNames);
  *yOutputErrorName = tmalloc(sizeof(**yOutputErrorName) * yNames);
  *yOutputUnits = tmalloc(sizeof(**yOutputUnits) * yNames);
  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddsderiv output", output) ||
      SDDS_DefineParameter1(SDDSout, "derivInterval", NULL, NULL, NULL, NULL, SDDS_LONG, &interval) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, xName, NULL) ||
      (xErrorName && !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xErrorName, NULL)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_GetColumnInformation(SDDSout, "symbol", &xSymbol, SDDS_GET_BY_NAME, xName) != SDDS_STRING) {
    fprintf(stderr, "error: problem getting symbol for column %s\n", xName);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!xSymbol)
    SDDS_CopyString(&xSymbol, xName);
  for (i = 0; i < yNames; i++) {
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName[i], NULL)) {
      fprintf(stderr, "error: problem transferring definition for column %s\n", yName[i]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (SDDS_GetColumnInformation(SDDSout, "symbol", &ySymbol, SDDS_GET_BY_NAME, yName[i]) != SDDS_STRING) {
      fprintf(stderr, "error: problem getting symbol for column %s\n", yName[i]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!ySymbol || SDDS_StringIsBlank(ySymbol))
      SDDS_CopyString(&ySymbol, yName[i]);
    (*yOutputUnits)[i] = divideColumnUnits(SDDSout, yName[i], xName);
    (*yOutputName)[i] = changeInformation(SDDSout, yName[i], yName[i], ySymbol, xName, xSymbol, mainTemplate, (*yOutputUnits)[i]);
    if (yErrorName || xErrorName) {
      if (yErrorName && yErrorName[i]) {
        if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yErrorName[i], NULL)) {
          fprintf(stderr, "error: problem transferring definition for column %s\n", yErrorName[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        (*yOutputErrorName)[i] = changeInformation(SDDSout, yErrorName[i], yName[i], ySymbol, xName, xSymbol, errorTemplate, (*yOutputUnits)[i]);
      } else {
        if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName[i], NULL)) {
          fprintf(stderr, "error: problem transferring error definition for column %s\n", yName[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        (*yOutputErrorName)[i] = changeInformation(SDDSout, yName[i], yName[i], ySymbol, xName, xSymbol, errorTemplate, (*yOutputUnits)[i]);
      }
    } else {
      (*yOutputErrorName)[i] = NULL;
    }
  }
  if (columnMajorOrder != -1)
    SDDSout->layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout->layout.data_mode.column_major = SDDSin->layout.data_mode.column_major;
  if (!SDDS_TransferAllParameterDefinitions(SDDSout, SDDSin, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 1;
}

char *changeInformation(SDDS_DATASET *SDDSout, char *name, char *nameRoot, char *symbolRoot, char *xName, char *xSymbol, char **template, char *newUnits) {
  char buffer1[SDDS_MAXLINE], buffer2[SDDS_MAXLINE], *ptr;

  if (!SDDS_ChangeColumnInformation(SDDSout, "units", newUnits, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, name))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  makeSubstitutions(buffer1, buffer2, template[2], nameRoot, symbolRoot, xName, xSymbol);
  if (!SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer2, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, name))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  makeSubstitutions(buffer1, buffer2, template[1], nameRoot, symbolRoot, xName, xSymbol);
  if (!SDDS_ChangeColumnInformation(SDDSout, "description", buffer2, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, name))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  makeSubstitutions(buffer1, buffer2, template[0], nameRoot, symbolRoot, xName, xSymbol);
  if (!SDDS_ChangeColumnInformation(SDDSout, "name", buffer2, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, name))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  SDDS_CopyString(&ptr, buffer2);
  return ptr;
}

void makeSubstitutions(char *buffer1, char *buffer2, char *template, char *nameRoot, char *symbolRoot, char *xName, char *xSymbol) {
  strcpy(buffer2, template);
  replace_string(buffer1, buffer2, "%ySymbol", symbolRoot);
  replace_string(buffer2, buffer1, "%xSymbol", xSymbol);
  replace_string(buffer1, buffer2, "%yName", nameRoot);
  replace_string(buffer2, buffer1, "%xName", xName);
  strcpy(buffer1, buffer2);
}

long checkErrorNames(char **yErrorName, long yNames) {
  long i;
  if (yErrorName[0]) {
    for (i = 1; i < yNames; i++)
      if (!yErrorName[i])
        return 0;
  } else {
    for (i = 1; i < yNames; i++)
      if (yErrorName[i])
        return 0;
  }
  return 1;
}
