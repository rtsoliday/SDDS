/**
 * @file sddsexpfit.c
 * @brief Performs exponential fitting for input data using the SDDS library.
 *
 * @details
 * This program fits data to an exponential function of the form:
 * ```
 * y = a0 + a1 * exp(a2 * x)
 * ```
 * It reads input data, applies the fitting model, and writes the results to an output file. The program
 * supports flexible configuration of input/output formats, fitting parameters, and other operational settings.
 *
 * @section Usage
 * ```
 * sddsexpfit [<inputfile>] [<outputfile>]
 *            [-pipe=[input][,output]]
 *            [-fulloutput] 
 *             -columns=<x-name>,<y-name>[,ySigma=<name>]
 *            [-tolerance=<value>] 
 *            [-verbosity=<integer>] 
 *            [-clue={grows|decays}] 
 *            [-guess=<constant>,<factor>,<rate>] 
 *            [-startValues=[constant=<value>][,factor=<value>][,rate=<value>]] 
 *            [-fixValue=[constant=<value>][,factor=<value>][,rate=<value>]] 
 *            [-autoOffset] 
 *            [-limits=[evaluations=<number>][,passes=<number>]] 
 *            [-majorOrder=row|column] 
 * ```
 *
 * @section Options
 * | Required           | Description                                                                |
 * |--------------------|----------------------------------------------------------------------------|
 * | `-columns`                    | Specify column names for x, y, and optional ySigma.     |
 *
 * | Optional                      | Description                                              |
 * |-------------------------------|----------------------------------------------------------|
 * | `-pipe`                       | Use input/output pipes instead of files.                |
 * | `-fulloutput`                 | Include detailed results in the output.                 |
 * | `-tolerance`                  | Set convergence tolerance for the fitting algorithm.    |
 * | `-verbosity`                  | Define verbosity level for logging.                     |
 * | `-clue`                       | Provide a hint for exponential behavior.                |
 * | `-guess`                      | Initial guesses for parameters `a0`, `a1`, `a2`.        |
 * | `-startValues`                | Set specific starting values for fit parameters.        |
 * | `-fixValue`                   | Fix certain fit parameters during optimization.         |
 * | `-autoOffset`                 | Automatically adjust offset for input data.             |
 * | `-limits`                     | Set algorithm limits (e.g., evaluations, passes).       |
 * | `-majorOrder`                 | Specify data processing order (row or column).          |
 *
 * @subsection Incompatibilities
 *   - `-guess` is incompatible with:
 *     - `-startValues`
 *   - `-fixValue` and `-startValues` cannot be applied to the same parameter simultaneously.
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
 * M. Borland, C. Saunders, L. Emery, R. Soliday, H. Shang, N. Kuklev
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_TOLERANCE,
  SET_VERBOSITY,
  SET_CLUE,
  SET_GUESS,
  SET_COLUMNS,
  SET_FULLOUTPUT,
  SET_PIPE,
  SET_LIMITS,
  SET_STARTVALUES,
  SET_FIXVALUE,
  SET_AUTOOFFSET,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "tolerance",
  "verbosity",
  "clue",
  "guess",
  "columns",
  "fulloutput",
  "pipe",
  "limits",
  "startvalues",
  "fixvalue",
  "autooffset",
  "majorOrder",
};

static char *USAGE =
  "sddsexpfit [<inputfile>] [<outputfile>]\n"
  "           [-pipe=[input][,output]]\n"
  "           [-fulloutput]\n"
  "            -columns=<x-name>,<y-name>[,ySigma=<name>]\n"
  "           [-tolerance=<value>]\n"
  "           [-verbosity=<integer>]\n"
  "           [-clue={grows|decays}]\n"
  "           [-guess=<constant>,<factor>,<rate>]\n"
  "           [-startValues=[constant=<value>][,factor=<value>][,rate=<value>]]\n"
  "           [-fixValue=[constant=<value>][,factor=<value>][,rate=<value>]]\n"
  "           [-autoOffset]\n"
  "           [-limits=[evaluations=<number>][,passes=<number>]]\n"
  "           [-majorOrder=row|column]\n\n"
  "Performs an exponential fit of the form y = <constant> + <factor> * exp(<rate> * x).\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static double *xData, *yData, *syData;
static int64_t nData;
static double yMin, yMax, xMin, xMax;
static double fit[3];

double fitFunction(double *a, long *invalid);
void report(double res, double *a, long pass, long n_eval, long n_dimen);
void setupOutputFile(SDDS_DATASET *OutputTable, long *xIndex, long *yIndex, long *fitIndex, long *residualIndex, char *output, long fullOutput, SDDS_DATASET *InputTable, char *xName, char *yName, short columnMajorOrder);
char *makeInverseUnits(char *units);

#define START_CONSTANT_GIVEN 0x0001
#define FIX_CONSTANT_GIVEN (0x0001 << 3)
#define START_FACTOR_GIVEN 0x0002
#define FIX_FACTOR_GIVEN (0x0002 << 3)
#define START_RATE_GIVEN 0x0004
#define FIX_RATE_GIVEN (0x0004 << 3)

#define CLUE_GROWS 0
#define CLUE_DECAYS 1
#define N_CLUE_TYPES 2
char *clue_name[N_CLUE_TYPES] = {
  "grows", 
  "decays"
};

long verbosity;

int main(int argc, char **argv) {
  double tolerance, result, chiSqr, sigLevel;
  int32_t nEvalMax = 5000, nPassMax = 100;
  double guess[3];
  double a[3], da[3];
  double alo[3], ahi[3];
  long n_dimen = 3, guessGiven, startGiven;
  SDDS_DATASET InputTable, OutputTable;
  SCANNED_ARG *s_arg;
  long i_arg, clue, fullOutput;
  int64_t i;
  char *input, *output, *xName, *yName, *syName;
  long xIndex, yIndex, fitIndex, residualIndex, retval;
  double *fitData, *residualData, rmsResidual;
  unsigned long guessFlags, pipeFlags, dummyFlags, majorOrderFlag;
  double constantStart, factorStart, rateStart;
  short disable[3] = {0, 0, 0};
  short autoOffset = 0;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2 || argc > (2 + N_OPTIONS))
    bomb(NULL, USAGE);

  input = output = NULL;
  tolerance = 1e-6;
  verbosity = fullOutput = guessGiven = startGiven = 0;
  clue = -1;
  xName = yName = syName = NULL;
  pipeFlags = guessFlags = 0;
  constantStart = factorStart = rateStart = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_AUTOOFFSET:
        autoOffset = 1;
        break;
      case SET_TOLERANCE:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &tolerance) != 1)
          SDDS_Bomb("incorrect -tolerance syntax");
        break;
      case SET_VERBOSITY:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &verbosity) != 1)
          SDDS_Bomb("incorrect -verbosity syntax");
        break;
      case SET_CLUE:
        if (s_arg[i_arg].n_items != 2 || (clue = match_string(s_arg[i_arg].list[1], clue_name, N_CLUE_TYPES, 0)) < 0)
          SDDS_Bomb("incorrect -clue syntax");
        break;
      case SET_GUESS:
        if (startGiven)
          SDDS_Bomb("can't have -startValues and -guess at once");
        if (s_arg[i_arg].n_items != 4 || sscanf(s_arg[i_arg].list[1], "%lf", guess + 0) != 1 || sscanf(s_arg[i_arg].list[2], "%lf", guess + 1) != 1 || sscanf(s_arg[i_arg].list[3], "%lf", guess + 2) != 1)
          SDDS_Bomb("invalid -guess syntax");
        guessGiven = 1;
        break;
      case SET_STARTVALUES:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("incorrect -startValues syntax");
        if (guessGiven)
          SDDS_Bomb("can't have -startValues and -guess at once");
        s_arg[i_arg].n_items -= 1;
        dummyFlags = guessFlags;
        if (!scanItemList(&guessFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "constant", SDDS_DOUBLE, &constantStart, 1, START_CONSTANT_GIVEN, "factor", SDDS_DOUBLE, &factorStart, 1, START_FACTOR_GIVEN, "rate", SDDS_DOUBLE, &rateStart, 1, START_RATE_GIVEN, NULL))
          SDDS_Bomb("invalid -fixValue syntax");
        if ((dummyFlags >> 3) & (guessFlags))
          SDDS_Bomb("can't have -fixValue and -startValue for the same item");
        guessFlags |= dummyFlags;
        startGiven = 1;
        break;
      case SET_FIXVALUE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("incorrect -fixValue syntax");
        s_arg[i_arg].n_items -= 1;
        dummyFlags = guessFlags;
        if (!scanItemList(&guessFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "constant", SDDS_DOUBLE, &constantStart, 1, FIX_CONSTANT_GIVEN, "factor", SDDS_DOUBLE, &factorStart, 1, FIX_FACTOR_GIVEN, "rate", SDDS_DOUBLE, &rateStart, 1, FIX_RATE_GIVEN, NULL))
          SDDS_Bomb("invalid -fixValue syntax");
        if ((dummyFlags) & (guessFlags >> 3))
          SDDS_Bomb("can't have -fixValue and -startValue for the same item");
        guessFlags |= dummyFlags;
        break;
      case SET_COLUMNS:
        if (s_arg[i_arg].n_items != 3 && s_arg[i_arg].n_items != 4)
          SDDS_Bomb("invalid -columns syntax");
        xName = s_arg[i_arg].list[1];
        yName = s_arg[i_arg].list[2];
        s_arg[i_arg].n_items -= 3;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 3, &s_arg[i_arg].n_items, 0, "ysigma", SDDS_STRING, &syName, 1, 0, NULL))
          SDDS_Bomb("invalid -columns syntax");
        break;
      case SET_FULLOUTPUT:
        fullOutput = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_LIMITS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("incorrect -limits syntax");
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "evaluations", SDDS_LONG, &nEvalMax, 1, 0, "passes", SDDS_LONG, &nPassMax, 1, 0, NULL) || nEvalMax <= 0 || nPassMax <= 0)
          SDDS_Bomb("invalid -limits syntax");
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  processFilenames("sddsexpfit", &input, &output, pipeFlags, 0, NULL);

  for (i = 0; i < 3; i++) {
    if ((guessFlags >> 3) & (1 << i)) {
      disable[i] = 1;
    }
  }

  if (!xName || !yName)
    SDDS_Bomb("-columns option must be given");

  if (!SDDS_InitializeInput(&InputTable, input) || SDDS_GetColumnIndex(&InputTable, xName) < 0 ||
      SDDS_GetColumnIndex(&InputTable, yName) < 0 || (syName && SDDS_GetColumnIndex(&InputTable, syName) < 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  setupOutputFile(&OutputTable, &xIndex, &yIndex, &fitIndex, &residualIndex, output, fullOutput, &InputTable, xName, yName, columnMajorOrder);

  while ((retval = SDDS_ReadPage(&InputTable)) > 0) {
    fitData = residualData = NULL;
    xData = yData = syData = NULL;
    if (!(xData = SDDS_GetColumnInDoubles(&InputTable, xName)) ||
        !(yData = SDDS_GetColumnInDoubles(&InputTable, yName)) ||
        (syName && !(syData = SDDS_GetColumnInDoubles(&InputTable, syName))))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((nData = SDDS_CountRowsOfInterest(&InputTable)) < 4)
      continue;

    if (xData[0] > xData[nData - 1])
      fputs("warning: data reverse-ordered", stderr);

    find_min_max(&yMin, &yMax, yData, nData);
    find_min_max(&xMin, &xMax, xData, nData);
    for (i = 0; i < nData; i++)
      xData[i] -= xMin;

    fill_double_array(alo, 3, -DBL_MAX / 2);
    fill_double_array(ahi, 3, DBL_MAX / 2);

    if (!guessGiven) {
      if (clue == CLUE_GROWS) {
        a[0] = 0.9 * yData[0];
        a[1] = yData[nData - 1] - yData[0];
        a[2] = 1 / (xData[nData - 1] - xData[0]);
        alo[2] = 0;
        if (a[1] > 0)
          alo[1] = 0;
        else
          ahi[1] = 0;
      } else if (clue == CLUE_DECAYS) {
        a[0] = 0.9 * yData[nData - 1];
        a[1] = yData[0] - yData[nData - 1];
        a[2] = 0;
        ahi[2] = 0;
        if (a[1] > 0)
          alo[1] = 0;
        else
          ahi[1] = 0;
      } else {
        a[0] = yMin * 0.9;
        a[1] = yMax - yMin;
        a[2] = 0;
      }
    } else {
      a[0] = guess[0];
      a[1] = guess[1];
      a[2] = guess[2];
    }

    if (guessFlags & (START_CONSTANT_GIVEN + FIX_CONSTANT_GIVEN))
      a[0] = constantStart;
    if (guessFlags & (START_FACTOR_GIVEN + FIX_FACTOR_GIVEN))
      a[1] = factorStart;
    if (guessFlags & (START_RATE_GIVEN + FIX_RATE_GIVEN))
      a[2] = rateStart;

    da[0] = da[1] = fabs(a[1] - a[0]) / 20.0;
    da[2] = 0.1 / (xData[nData - 1] - xData[0]);
    if (verbosity > 3)
      fprintf(stderr, "starting guess: %e, %e, %e\n", a[0], a[1], a[2]);

    simplexMin(&result, a, da, alo, ahi, disable, n_dimen, -DBL_MAX, tolerance, fitFunction, (verbosity > 0 ? report : NULL), nEvalMax, nPassMax, 12, 3, 1.0, 0);

    da[0] = a[0] / 10;
    da[1] = a[1] / 10;
    da[2] = a[2] / 10;
    simplexMin(&result, a, da, alo, ahi, disable, n_dimen, -DBL_MAX, tolerance, fitFunction, (verbosity > 0 ? report : NULL), nEvalMax, nPassMax, 12, 3, 1.0, 0);

    if (!autoOffset) {
      /* user wants the coefficients with the offset removed */
      a[1] *= exp(-a[2] * xMin);
      for (i = 0; i < nData; i++)
        xData[i] += xMin;
    }

    fitData = trealloc(fitData, sizeof(*fitData) * nData);
    residualData = trealloc(residualData, sizeof(*residualData) * nData);
    for (i = result = 0; i < nData; i++) {
      fitData[i] = a[0] + a[1] * exp(a[2] * xData[i]);
      residualData[i] = yData[i] - fitData[i];
      result += sqr(residualData[i]);
    }
    rmsResidual = sqrt(result / nData);
    if (syData) {
      for (i = chiSqr = 0; i < nData; i++)
        chiSqr += sqr(residualData[i] / syData[i]);
    } else {
      double sy2;
      sy2 = result / (nData - 3);
      for (i = chiSqr = 0; i < nData; i++)
        chiSqr += sqr(residualData[i]) / sy2;
    }
    sigLevel = ChiSqrSigLevel(chiSqr, nData - 3);
    if (verbosity > 1) {
      fprintf(stderr, "RMS deviation: %.15e\n", rmsResidual);
      fprintf(stderr, "(RMS deviation)/(largest value): %.15e\n", rmsResidual / MAX(fabs(yMin), fabs(yMax)));
      if (syData)
        fprintf(stderr, "Significance level: %.5e\n", sigLevel);
    }
    if (verbosity > 0) {
      fprintf(stderr, "coefficients of fit to the form y = a0 + a1*exp(a2*x), a = \n");
      for (i = 0; i < 3; i++)
        fprintf(stderr, "%.8e ", a[i]);
      fprintf(stderr, "\n");
    }

    if (!SDDS_StartPage(&OutputTable, nData) ||
        !SDDS_CopyParameters(&OutputTable, &InputTable) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, xData, nData, xIndex) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, fitData, nData, fitIndex) ||
        !SDDS_SetParameters(&OutputTable, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME,
                            "expfitConstant", a[0],
                            "expfitFactor", a[1],
                            "expfitRate", a[2],
                            "expfitRmsResidual", rmsResidual,
                            "expfitSigLevel", sigLevel, NULL) ||
        (fullOutput && (!SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, yData, nData, yIndex) ||
                        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, residualData, nData, residualIndex))) ||
        !SDDS_WritePage(&OutputTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (xData)
      free(xData);
    if (yData)
      free(yData);
    if (syData)
      free(syData);
    if (fitData)
      free(fitData);
    if (residualData)
      free(residualData);
  }
  if (!SDDS_Terminate(&InputTable) || !SDDS_Terminate(&OutputTable)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}

void setupOutputFile(SDDS_DATASET *OutputTable, long *xIndex, long *yIndex, long *fitIndex, long *residualIndex, char *output, long fullOutput, SDDS_DATASET *InputTable, char *xName, char *yName, short columnMajorOrder) {
  char *name, *yUnits, *description, *xUnits, *inverse_xUnits;
  int32_t typeValue = SDDS_DOUBLE;
  static char *residualNamePart = "Residual";
  static char *residualDescriptionPart = "Residual of exponential fit to ";

  if (!SDDS_InitializeOutput(OutputTable, SDDS_BINARY, 0, NULL, "sddsexpfit output", output) ||
      !SDDS_TransferColumnDefinition(OutputTable, InputTable, xName, NULL) ||
      !SDDS_ChangeColumnInformation(OutputTable, "type", &typeValue, SDDS_BY_NAME, xName) ||
      (*xIndex = SDDS_GetColumnIndex(OutputTable, xName)) < 0 ||
      !SDDS_GetColumnInformation(InputTable, "units", &xUnits, SDDS_BY_NAME, xName) ||
      !SDDS_GetColumnInformation(InputTable, "units", &yUnits, SDDS_BY_NAME, yName)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (columnMajorOrder != -1)
    OutputTable->layout.data_mode.column_major = columnMajorOrder;
  else
    OutputTable->layout.data_mode.column_major = InputTable->layout.data_mode.column_major;

  name = tmalloc(sizeof(*name) * (strlen(yName) + strlen(residualNamePart) + 1));
  description = tmalloc(sizeof(*name) * (strlen(yName) + strlen(residualDescriptionPart) + 1));

  if (fullOutput) {
    if (!SDDS_TransferColumnDefinition(OutputTable, InputTable, yName, NULL) ||
        !SDDS_ChangeColumnInformation(OutputTable, "type", &typeValue, SDDS_BY_NAME, yName) ||
        (*yIndex = SDDS_GetColumnIndex(OutputTable, yName)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    sprintf(name, "%s%s", yName, residualNamePart);
    sprintf(description, "%s%s", yName, residualDescriptionPart);
    if ((*residualIndex = SDDS_DefineColumn(OutputTable, name, NULL, yUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  sprintf(name, "%sFit", yName);
  sprintf(description, "Exponential fit to %s", yName);
  if ((*fitIndex = SDDS_DefineColumn(OutputTable, name, NULL, yUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  inverse_xUnits = makeInverseUnits(xUnits);

  if (SDDS_DefineParameter(OutputTable, "expfitConstant", NULL, yUnits, "Constant term from exponential fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "expfitFactor", NULL, yUnits, "Factor from exponential fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "expfitRate", NULL, inverse_xUnits, "Rate from exponential fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "expfitRmsResidual", NULL, yUnits, "RMS residual from exponential fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "expfitSigLevel", NULL, NULL, "Significance level from chi-squared test", NULL, SDDS_DOUBLE, 0) < 0 ||
      !SDDS_TransferAllParameterDefinitions(OutputTable, InputTable, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(OutputTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

char *makeInverseUnits(char *units) {
  char *inverseUnits;

  if (!units || SDDS_StringIsBlank(units))
    return NULL;
  inverseUnits = tmalloc(sizeof(*inverseUnits) * (strlen(units) + 5));

  if (strncmp(units, "1/(", 3) == 0 && units[strlen(units) - 1] == ')') {
    /* special case of "1/(<unit>)" */
    strcpy(inverseUnits, units + 3);
    inverseUnits[strlen(inverseUnits) - 1] = '\0';
  } else if (!strchr(units, ' ')) {
    /* special case of units string without spaces */
    sprintf(inverseUnits, "1/%s", units);
  } else {
    /* general case */
    sprintf(inverseUnits, "1/(%s)", units);
  }

  return inverseUnits;
}

double fitFunction(double *a, long *invalid) {
  int64_t i;
  double chi = 0.0, diff;
  static double min_chi;

  min_chi = DBL_MAX;

  *invalid = 0;
  for (i = 0; i < nData; i++) {
    diff = yData[i] - (a[0] + a[1] * exp(a[2] * xData[i]));
    if (syData)
      diff /= syData[i];
    chi += sqr(diff);
  }
  if (isnan(chi) || isinf(chi))
    *invalid = 1;
  if (verbosity > 3)
    fprintf(stderr, "trial: a = %e, %e, %e  --> chi = %e, invalid = %ld\n", a[0], a[1], a[2], chi, *invalid);
  if (min_chi > chi) {
    min_chi = chi;
    fit[0] = a[0];
    fit[1] = a[1];
    fit[2] = a[2];
    if (verbosity > 2)
      fprintf(stderr, "new best chi = %e:  a = %e, %e, %e\n", chi, fit[0], fit[1], fit[2]);
  }
  return chi;
}

void report(double y, double *x, long pass, long nEval, long n_dimen) {
  long i;

  fprintf(stderr, "Pass %ld, after %ld evaluations: result = %.16e\na = ", pass, nEval, y);
  for (i = 0; i < n_dimen; i++)
    fprintf(stderr, "%.8e ", x[i]);
  fputc('\n', stderr);
}
