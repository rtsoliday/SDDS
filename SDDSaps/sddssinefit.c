/**
 * @file sddssinefit.c
 * @brief Performs a sinusoidal fit on input data.
 *
 * @details
 * This program reads input data from an SDDS (Self Describing Data Sets) file,
 * fits the data to one of two sinusoidal models:
 * 
 * @f[
  * y(n) = a_0 + a_1 \sin(2\pi a_2 x(n) + a_3)
  * @f]
 * 
 * or
 * 
 * @f[
  * y(n) = a_0 + a_1 \sin(2\pi a_2 x(n) + a_3) + a_4 x(n)
  * @f]
 *
 * Based on user-provided parameters and options, the program performs fitting and outputs
 * the fitted data along with residuals to an output SDDS file.
 *
 * @section Usage
 * ```
 * sddssinefit [<inputfile>] [<outputfile>] 
 *             [-pipe=<input>[,<output>]]
 *             [-fulloutput]
 *              -columns=<x-name>,<y-name>
 *             [-tolerance=<value>]
 *             [-limits=evaluations=<number>,passes=<number>]
 *             [-verbosity=<integer>]
 *             [-guess=constant=<constant>,factor=<factor>,frequency=<freq>,phase=<phase>,slope=<slope>]
 *             [-lockFrequency]
 *             [-addSlope]
 *             [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Specifies the x and y data column names.                                              |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard input/output for data streams.                                           |
 * | `-fulloutput`                         | Includes full output with residuals.                                                 |
 * | `-tolerance`                          | Sets the tolerance for the fitting algorithm (default: 1e-6).                        |
 * | `-limits=evaluations`                 | Sets maximum number of evaluations and passes (default: 5000 evaluations, 25 passes).|
 * | `-verbosity`                          | Sets verbosity level (default: 0).                                                   |
 * | `-guess`                              | Provides initial guesses for fit parameters.    |
 * | `-lockFrequency`                      | Locks the frequency parameter during fitting.                                         |
 * | `-addSlope`                           | Includes a slope term in the fit.                                                    |
 * | `-majorOrder`                         | Specifies the major order for data processing.                                       |
 *
 * @subsection Incompatibilities
 * - `-lockFrequency` cannot be used with `-guess=frequency=<freq>`.
 *
 * @subsection Requirements
 * - For `-guess`, at least one of the guess parameters (`constant`, `factor`, `frequency`, `phase`, `slope`) is required.
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
 * M. Borland, C. Saunders, R. Soliday, L. Emery, H. Shang, N. Kuklev
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
  SET_LIMITS,
  SET_PIPE,
  SET_MAJOR_ORDER,
  SET_LOCK_FREQ,
  SET_ADD_SLOPE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "tolerance",
  "verbosity",
  "clue",
  "guess",
  "columns",
  "fulloutput",
  "limits",
  "pipe",
  "majorOrder",
  "lockFrequency",
  "addSlope",
};

static char *USAGE =
  "sddssinefit [<inputfile>] [<outputfile>] \n"
  "       [-pipe=<input>[,<output>]]\n"
  "       [-fulloutput]\n"
  "       [-columns=<x-name>,<y-name>]\n"
  "       [-tolerance=<value>]\n"
  "       [-limits=evaluations=<number>,passes=<number>]\n"
  "       [-verbosity=<integer>]\n"
  "       [-guess=constant=<constant>,factor=<factor>,frequency=<freq>,phase=<phase>,slope=<slope>]\n"
  "       [-lockFrequency]\n"
  "       [-addSlope]\n"
  "       [-majorOrder=row|column]\n\n"
  "Description:\n"
  "  Performs a sinusoidal fit of the form:\n"
  "    y = <constant> + <factor>*sin(2*PI*<freq>*x + <phase>)\n"
  "  or\n"
  "    y = <constant> + <factor>*sin(2*PI*<freq>*x + <phase>) + <slope>*x\n\n"
  "Options:\n"
  "  <inputfile>                : Path to the input SDDS file.\n"
  "  <outputfile>               : Path to the output SDDS file.\n"
  "  -pipe=<input>,<output>     : Use standard input/output for data streams.\n"
  "  -fulloutput                : Include full output with residuals.\n"
  "  -columns=<x-name>,<y-name> : Specify the names of the x and y data columns.\n"
  "  -tolerance=<value>         : Set the tolerance for the fitting algorithm (default: 1e-6).\n"
  "  -limits=evaluations=<n>,passes=<m> : Set maximum number of evaluations and passes (default: 5000 evaluations, 25 passes).\n"
  "  -verbosity=<integer>       : Set verbosity level (default: 0).\n"
  "  -guess=constant=<c>,factor=<f>,frequency=<freq>,phase=<p>,slope=<s> : Provide initial guesses for fit parameters.\n"
  "  -lockFrequency             : Lock the frequency parameter during fitting.\n"
  "  -addSlope                  : Include a slope term in the fit.\n"
  "  -majorOrder=row|column     : Specify the major order for data processing.\n\n"
  "Author:\n"
  "  Michael Borland\n"
  "  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static double *xData, *yData;
static int64_t nData;
static double yMin, yMax, xMin, xMax;
static double fit[5];

double fitFunction(double *a, long *invalid);
double fitFunctionWithSlope(double *a, long *invalid);
void report(double res, double *a, long pass, long n_eval, long n_dimen);
void setupOutputFile(SDDS_DATASET *OutputTable, int32_t *xIndex, int32_t *yIndex, int32_t *fitIndex,
                     int32_t *residualIndex, char *output, long fullOutput, SDDS_DATASET *InputTable,
                     char *xName, char *yName, short columnMajorOrder, short addSlope);
char *makeInverseUnits(char *units);

long verbosity;

#define GUESS_CONSTANT_GIVEN 0x0001
#define GUESS_FACTOR_GIVEN 0x0002
#define GUESS_FREQ_GIVEN 0x0004
#define GUESS_PHASE_GIVEN 0x0008
#define GUESS_SLOPE_GIVEN 0x0010

int main(int argc, char **argv) {
  double tolerance, result;
  int32_t nEvalMax = 5000, nPassMax = 25;
  double a[5], da[5];
  double alo[5], ahi[5];
  long n_dimen = 4;
  int64_t zeroes;
  SDDS_DATASET InputTable, OutputTable;
  SCANNED_ARG *s_arg;
  long i_arg, fullOutput;
  int64_t i;
  char *input, *output, *xName, *yName;
  int32_t xIndex, yIndex, fitIndex, residualIndex;
  long retval;
  double *fitData, *residualData, rmsResidual;
  unsigned long guessFlags, dummyFlags, pipeFlags;
  double constantGuess, factorGuess, freqGuess, phaseGuess, slopeGuess;
  double firstZero, lastZero;
  unsigned long simplexFlags = 0, majorOrderFlag;
  short columnMajorOrder = -1;
  short lockFreq = 0;
  short addSlope = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2 || argc > (2 + N_OPTIONS))
    bomb(NULL, USAGE);

  input = output = NULL;
  tolerance = 1e-6;
  verbosity = fullOutput = 0;
  xName = yName = NULL;
  guessFlags = 0;
  pipeFlags = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_TOLERANCE:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &tolerance) != 1)
          SDDS_Bomb("incorrect -tolerance syntax");
        break;
      case SET_VERBOSITY:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &verbosity) != 1)
          SDDS_Bomb("incorrect -verbosity syntax");
        break;
      case SET_GUESS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("incorrect -guess syntax");
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&guessFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "constant", SDDS_DOUBLE, &constantGuess, 1, GUESS_CONSTANT_GIVEN,
                          "factor", SDDS_DOUBLE, &factorGuess, 1, GUESS_FACTOR_GIVEN,
                          "frequency", SDDS_DOUBLE, &freqGuess, 1, GUESS_FREQ_GIVEN,
                          "phase", SDDS_DOUBLE, &phaseGuess, 1, GUESS_PHASE_GIVEN,
                          "slope", SDDS_DOUBLE, &slopeGuess, 1, GUESS_SLOPE_GIVEN, NULL))
          SDDS_Bomb("invalid -guess syntax");
        break;
      case SET_COLUMNS:
        if (s_arg[i_arg].n_items != 3)
          SDDS_Bomb("invalid -columns syntax");
        xName = s_arg[i_arg].list[1];
        yName = s_arg[i_arg].list[2];
        break;
      case SET_FULLOUTPUT:
        fullOutput = 1;
        break;
      case SET_LOCK_FREQ:
        lockFreq = 1;
        break;
      case SET_ADD_SLOPE:
        addSlope = 1;
        n_dimen = 5;
        break;
      case SET_LIMITS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("incorrect -limits syntax");
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "evaluations", SDDS_LONG, &nEvalMax, 1, 0,
                          "passes", SDDS_LONG, &nPassMax, 1, 0, NULL) ||
            nEvalMax <= 0 || nPassMax <= 0)
          SDDS_Bomb("invalid -limits syntax");
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      default:
        fprintf(stderr, "Error: Unknown or ambiguous option: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many filenames provided.");
    }
  }

  processFilenames("sddssinefit", &input, &output, pipeFlags, 0, NULL);

  if (!xName || !yName)
    SDDS_Bomb("-columns option must be specified.");

  if (!SDDS_InitializeInput(&InputTable, input) ||
      SDDS_GetColumnIndex(&InputTable, xName) < 0 ||
      SDDS_GetColumnIndex(&InputTable, yName) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  setupOutputFile(&OutputTable, &xIndex, &yIndex, &fitIndex, &residualIndex, output, fullOutput,
                  &InputTable, xName, yName, columnMajorOrder, addSlope);

  fitData = residualData = NULL;

  alo[0] = -(ahi[0] = DBL_MAX);
  alo[1] = alo[2] = 0;
  ahi[1] = ahi[2] = DBL_MAX;
  alo[3] = -(ahi[3] = PIx2);
  if (addSlope) {
    alo[4] = -(ahi[4] = DBL_MAX);
  }
  firstZero = lastZero = 0;
  while ((retval = SDDS_ReadPage(&InputTable)) > 0) {
    if (!(xData = SDDS_GetColumnInDoubles(&InputTable, xName)) ||
        !(yData = SDDS_GetColumnInDoubles(&InputTable, yName)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((nData = SDDS_CountRowsOfInterest(&InputTable)) < 4)
      continue;

    find_min_max(&yMin, &yMax, yData, nData);
    find_min_max(&xMin, &xMax, xData, nData);
    zeroes = 0;
    for (i = 1; i < nData; i++)
      if (yData[i] * yData[i - 1] <= 0) {
        i++;
        if (!zeroes)
          firstZero = (xData[i] + xData[i - 1]) / 2;
        else
          lastZero = (xData[i] + xData[i - 1]) / 2;
        zeroes++;
      }
    a[0] = (yMin + yMax) / 2;
    a[1] = (yMax - yMin) / 2;
    if (!zeroes)
      a[2] = 2 / fabs(xMax - xMin);
    else
      a[2] = zeroes / (2 * fabs(lastZero - firstZero));
    a[3] = 0;
    if (addSlope) {
      a[4] = 0;
    }
    if (guessFlags & GUESS_CONSTANT_GIVEN)
      a[0] = constantGuess;
    if (guessFlags & GUESS_FACTOR_GIVEN)
      a[1] = factorGuess;
    if (guessFlags & GUESS_FREQ_GIVEN)
      a[2] = freqGuess;
    if (guessFlags & GUESS_PHASE_GIVEN)
      a[3] = phaseGuess;
    if (guessFlags & GUESS_SLOPE_GIVEN)
      a[4] = slopeGuess;

    alo[1] = a[1] / 2;
    if (!(da[0] = a[0] * 0.1))
      da[0] = 0.01;
    if (!(da[1] = a[1] * 0.1))
      da[1] = 0.01;
    da[2] = a[2] * 0.25;
    da[3] = 0.01;
    if (addSlope) {
      da[4] = 0.01;
    }

    if (lockFreq) {
      alo[2] = ahi[2] = a[2];
      da[2] = 0;
    }

    if (addSlope) {
      simplexMin(&result, a, da, alo, ahi, NULL, n_dimen, -DBL_MAX, tolerance, fitFunctionWithSlope,
                 (verbosity > 0 ? report : NULL), nEvalMax, nPassMax, 12, 3, 1.0, simplexFlags);
    } else {
      simplexMin(&result, a, da, alo, ahi, NULL, n_dimen, -DBL_MAX, tolerance, fitFunction,
                 (verbosity > 0 ? report : NULL), nEvalMax, nPassMax, 12, 3, 1.0, simplexFlags);
    }
    fitData = trealloc(fitData, sizeof(*fitData) * nData);
    residualData = trealloc(residualData, sizeof(*residualData) * nData);
    if (addSlope) {
      for (i = result = 0; i < nData; i++) {
        fitData[i] = a[0] + a[1] * sin(PIx2 * a[2] * xData[i] + a[3]) + a[4] * xData[i];
        residualData[i] = yData[i] - fitData[i];
        result += sqr(residualData[i]);
      }
    } else {
      for (i = result = 0; i < nData; i++) {
        fitData[i] = a[0] + a[1] * sin(PIx2 * a[2] * xData[i] + a[3]);
        residualData[i] = yData[i] - fitData[i];
        result += sqr(residualData[i]);
      }
    }
    rmsResidual = sqrt(result / nData);
    if (verbosity > 1) {
      fprintf(stderr, "RMS deviation: %.15e\n", rmsResidual);
      fprintf(stderr, "(RMS deviation)/(largest value): %.15e\n", rmsResidual / MAX(fabs(yMin), fabs(yMax)));
    }
    if (verbosity > 0) {
      if (addSlope) {
        fprintf(stderr, "Coefficients of fit to the form y = a0 + a1*sin(2*PI*a2*x + a3) + a4*x, a = \n");
        for (i = 0; i < 5; i++)
          fprintf(stderr, "%.8e ", a[i]);
      } else {
        fprintf(stderr, "Coefficients of fit to the form y = a0 + a1*sin(2*PI*a2*x + a3), a = \n");
        for (i = 0; i < 4; i++)
          fprintf(stderr, "%.8e ", a[i]);
      }
      fprintf(stderr, "\n");
    }

    if (!SDDS_StartPage(&OutputTable, nData) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, xData, nData, xIndex) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, fitData, nData, fitIndex) ||
        !SDDS_SetParameters(&OutputTable, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME,
                            "sinefitConstant", a[0],
                            "sinefitFactor", a[1],
                            "sinefitFrequency", a[2],
                            "sinefitPhase", a[3],
                            "sinefitRmsResidual", rmsResidual,
                            NULL) ||
        (addSlope &&
         (!SDDS_SetParameters(&OutputTable, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME,
                              "sinefitSlope", a[4], NULL))) ||
        (fullOutput && (!SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, yData, nData, yIndex) ||
                        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, residualData, nData, residualIndex))) ||
        !SDDS_WritePage(&OutputTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (SDDS_Terminate(&InputTable) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }
  if (SDDS_Terminate(&OutputTable) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

void setupOutputFile(SDDS_DATASET *OutputTable, int32_t *xIndex, int32_t *yIndex, int32_t *fitIndex,
                     int32_t *residualIndex, char *output, long fullOutput, SDDS_DATASET *InputTable,
                     char *xName, char *yName, short columnMajorOrder, short addSlope) {
  char *name, *yUnits, *description, *xUnits, *inverse_xUnits;
  int32_t typeValue = SDDS_DOUBLE;
  static char *residualNamePart = "Residual";
  static char *residualDescriptionPart = "Residual of sinusoidal fit to ";

  if (!SDDS_InitializeOutput(OutputTable, SDDS_BINARY, 0, NULL, "sddssinefit output", output) ||
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
  description = tmalloc(sizeof(*description) * (strlen(yName) + strlen(residualDescriptionPart) + 1));

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
  sprintf(description, "Sinusoidal fit to %s", yName);
  if ((*fitIndex = SDDS_DefineColumn(OutputTable, name, NULL, yUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  inverse_xUnits = makeInverseUnits(xUnits);

  if (SDDS_DefineParameter(OutputTable, "sinefitConstant", NULL, yUnits, "Constant term from sinusoidal fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "sinefitFactor", NULL, yUnits, "Factor from sinusoidal fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "sinefitFrequency", NULL, inverse_xUnits, "Frequency from sinusoidal fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "sinefitPhase", NULL, xUnits, "Phase from sinusoidal fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "sinefitRmsResidual", NULL, yUnits, "RMS residual from sinusoidal fit",
                           NULL, SDDS_DOUBLE, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (addSlope) {
    if (SDDS_DefineParameter(OutputTable, "sinefitSlope", NULL, yUnits, "Slope term added to sinusoidal fit",
                             NULL, SDDS_DOUBLE, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_WriteLayout(OutputTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  free(name);
  free(description);
}

char *makeInverseUnits(char *units) {
  char *inverseUnits;

  if (!units || SDDS_StringIsBlank(units))
    return NULL;
  inverseUnits = tmalloc(sizeof(*inverseUnits) * (strlen(units) + 5));

  if (strncmp(units, "1/(", 3) == 0 && units[strlen(units) - 1] == ')') {
    /* Special case of "1/(<unit>)" */
    strcpy(inverseUnits, units + 3);
    inverseUnits[strlen(inverseUnits) - 1] = '\0';
  } else if (!strchr(units, ' ')) {
    /* Special case of units string without spaces */
    sprintf(inverseUnits, "1/%s", units);
  } else {
    /* General case */
    sprintf(inverseUnits, "1/(%s)", units);
  }

  return inverseUnits;
}

double fitFunction(double *a, long *invalid) {
  int64_t i;
  double chi;
  static double min_chi;

  min_chi = DBL_MAX;

  *invalid = 0;
  for (i = chi = 0; i < nData; i++)
    chi += sqr(yData[i] - (a[0] + a[1] * sin(PIx2 * a[2] * xData[i] + a[3])));
  if (isnan(chi) || isinf(chi))
    *invalid = 1;
  if (verbosity > 3)
    fprintf(stderr, "Trial: a = %e, %e, %e, %e  --> chi = %e, invalid = %ld\n", a[0], a[1], a[2], a[3], chi, *invalid);
  if (min_chi > chi) {
    min_chi = chi;
    fit[0] = a[0];
    fit[1] = a[1];
    fit[2] = a[2];
    fit[3] = a[3];
    if (verbosity > 2)
      fprintf(stderr, "New best chi = %e:  a = %e, %e, %e, %e\n", chi, fit[0], fit[1], fit[2], fit[3]);
  }
  return chi;
}

double fitFunctionWithSlope(double *a, long *invalid) {
  int64_t i;
  double chi;
  static double min_chi;

  min_chi = DBL_MAX;

  *invalid = 0;
  for (i = chi = 0; i < nData; i++)
    chi += sqr(yData[i] - (a[0] + a[1] * sin(PIx2 * a[2] * xData[i] + a[3]) + a[4] * xData[i]));
  if (isnan(chi) || isinf(chi))
    *invalid = 1;
  if (verbosity > 3)
    fprintf(stderr, "Trial: a = %e, %e, %e, %e, %e  --> chi = %e, invalid = %ld\n",
            a[0], a[1], a[2], a[3], a[4], chi, *invalid);
  if (min_chi > chi) {
    min_chi = chi;
    fit[0] = a[0];
    fit[1] = a[1];
    fit[2] = a[2];
    fit[3] = a[3];
    fit[4] = a[4];
    if (verbosity > 2)
      fprintf(stderr, "New best chi = %e:  a = %e, %e, %e, %e, %e\n",
              chi, fit[0], fit[1], fit[2], fit[3], fit[4]);
  }
  return chi;
}

void report(double y, double *x, long pass, long nEval, long n_dimen) {
  long i;

  fprintf(stderr, "Pass %ld, after %ld evaluations: result = %.16e\n", pass, nEval, y);
  fprintf(stderr, "a = ");
  for (i = 0; i < n_dimen; i++)
    fprintf(stderr, "%.8e ", x[i]);
  fputc('\n', stderr);
}
