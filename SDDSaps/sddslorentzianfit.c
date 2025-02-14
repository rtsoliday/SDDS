/**
 * @file sddslorentzianfit.c
 * @brief Perform a Lorentzian fit on data using the SDDS library.
 *
 * @details
 * This program reads data from an SDDS file, performs a Lorentzian fit, and writes the 
 * results to an SDDS file. The fitting function is:
 * \f[
 * y(x) = baseline + height \cdot \frac{\gamma^2}{\gamma^2 + (x - center)^2}
 * \f]
 * Various options allow the user to customize the fit, including specifying fitting ranges, 
 * initial parameter guesses, and verbosity levels.
 *
 * @section Usage
 * ```
 * sddslorentzianfit [<inputfile>] [<outputfile>]
 *                   [-pipe=[input][,output]]
 *                    -columns=<x-name>,<y-name>[,ySigma=<sy-name>]
 *                   [-fitRange=<lower>|@<parameter-name>,<upper>|@<parameter-name>]
 *                   [-fullOutput]
 *                   [-verbosity=<integer>] 
 *                   [-stepSize=<factor>] 
 *                   [-tolerance=<value>]
 *                   [-guesses=[baseline=<value>|@<parameter-name>][,center=<value>|@<parameter-name>][,height=<value>|@<parameter-name>][,gamma=<value>|@<parameter-name>]] 
 *                   [-fixValue=[baseline=<value>|@<parameter-name>][,center=<value>|@<parameter-name>][,height=<value>|@<parameter-name>][,gamma=<value>|@<parameter-name>]]
 *                   [-limits=[evaluations=<number>][,passes=<number>]] 
 *                   [-majorOrder=row|column] 
 * ```
 *
 * @section Options
 * | Required                              | Description                                             |
 * |---------------------------------------|---------------------------------------------------------|
 * | `-columns`              | Specify x and y column names, and optionally y sigma.                                       |
 *
 * | Optional                | Description                                                                                 |
 * |-------------------------|---------------------------------------------------------------------------------------------|
 * | `-pipe`                 | Use standard input/output pipes.                                                            |
 * | `-fitRange`             | Define fitting range with bounds, which can be constants or parameters.                     |
 * | `-fullOutput`           | Enable detailed output including residuals.                                                |
 * | `-verbosity`            | Set verbosity level.                                                                       |
 * | `-stepSize`             | Step size factor for fitting algorithm.                                                    |
 * | `-tolerance`            | Tolerance for convergence.                                                                 |
 * | `-guesses`              | Provide initial guesses for parameters (baseline, center, height, gamma).                  |
 * | `-fixValue`             | Fix specific parameters to provided values.                                                |
 * | `-limits`               | Set limits on evaluations and passes.                                                      |
 * | `-majorOrder`           | Specify major order for data storage (row or column).                                       |
 *
 * @subsection Incompatibilities
 *   - `-fixValue` is incompatible with `-guesses` for the same parameter.
 *
 * @subsection Requirements
 *   - For `-fitRange`:
 *     - Both lower and upper bounds are required.
 *   - For `-limits`:
 *     - Must specify `evaluations` or `passes` or both.
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
 * M. Borland
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_FITRANGE,
  SET_GUESSES,
  SET_VERBOSITY,
  SET_COLUMNS,
  SET_TOLERANCE,
  SET_FULLOUTPUT,
  SET_STEPSIZE,
  SET_LIMITS,
  SET_PIPE,
  SET_FIXVALUE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "fitrange",
  "guesses",
  "verbosity",
  "columns",
  "tolerance",
  "fulloutput",
  "stepsize",
  "limits",
  "pipe",
  "fixvalue",
  "majorOrder",
};

static char *USAGE =
  "Usage: sddslorentzianfit [<inputfile>] [<outputfile>]\n"
  "                         [-pipe=[input][,output]]\n"
  "                          -columns=<x-name>,<y-name>[,ySigma=<sy-name>]\n"
  "                         [-fitRange=<lower>|@<parameter-name>,<upper>|@<parameter-name>]\n"
  "                         [-fullOutput]\n"
  "                         [-verbosity=<integer>] \n"
  "                         [-stepSize=<factor>] \n"
  "                         [-tolerance=<value>]\n"
  "                         [-guesses=[baseline=<value>|@<parameter-name>][,center=<value>|@<parameter-name>]\n"
  "                                   [,height=<value>|@<parameter-name>][,gamma=<value>|@<parameter-name>]] \n"
  "                         [-fixValue=[baseline=<value>|@<parameter-name>][,center=<value>|@<parameter-name>]\n"
  "                                    [,height=<value>|@<parameter-name>][,gamma=<value>|@<parameter-name>]]\n"
  "                         [-limits=[evaluations=<number>][,passes=<number>]] \n"
  "                         [-majorOrder=row|column] \n"
  "\nOptions:\n"
  "  -pipe=<input>,<output>          Use standard input/output pipes.\n"
  "  -columns=<x-name>,<y-name>[,ySigma=<sy-name>]\n"
  "                                  Specify the names of the x and y data columns,\n"
  "                                  and optionally the y sigma column.\n"
  "  -fitRange=<lower>|@<param>,<upper>|@<param>\n"
  "                                  Define the fitting range with lower and upper bounds.\n"
  "                                  Values can be direct or parameter references.\n"
  "  -guesses=<baseline|@param>,<center|@param>,<height|@param>,<gamma|@param>\n"
  "                                  Provide initial guesses for the fit parameters.\n"
  "  -fixValue=<baseline|@param>,<center|@param>,<height|@param>,<gamma|@param>\n"
  "                                  Fix specific fit parameters to given values or parameters.\n"
  "  -verbosity=<integer>            Set verbosity level (higher for more detail).\n"
  "  -stepSize=<factor>              Define the step size factor for the fitting algorithm.\n"
  "  -tolerance=<value>              Set the tolerance for convergence of the fit.\n"
  "  -limits=<evaluations>,<passes>  Set maximum number of evaluations and passes.\n"
  "  -majorOrder=row|column           Specify the major order for data storage.\n"
  "  -fullOutput                     Enable detailed output including residuals.\n"
  "\nDescription:\n"
  "  Performs a Lorentzian fit of the form:\n"
  "    y = baseline + height * gamma^2 / (gamma^2 + (x - center)^2)\n"
  "\nAuthor:\n"
  "  Michael Borland\n"
  "  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

void report(double res, double *a, long pass, long n_eval, long n_dimen);
void setupOutputFile(SDDS_DATASET *OutputTable, long *xIndex, long *yIndex, long *syIndex, long *fitIndex,
                     long *residualIndex, long fullOutput, char *output, SDDS_DATASET *InputTable,
                     char *xName, char *yName, char *syName, short columnMajorOrder);
long computeStartingPoint(double *a, double *da, double *x, double *y, int64_t n, unsigned long guessFlags,
                          double gammaGuess, double centerGuess, double baselineGuess, double heightGuess,
                          double stepSize);
double fitFunction(double *a, long *invalid);
int64_t makeFilteredCopy(double **xFit, double **yFit, double **syFit, double *x, double *y, double *sy,
                         int64_t n, double lower, double upper);

static double *xDataFit = NULL, *yDataFit = NULL, *syDataFit = NULL;
static int64_t nDataFit = 0;

#define GUESS_BASELINE_GIVEN 0x0001
#define FIX_BASELINE_GIVEN (0x0001 << 4)
#define GUESS_HEIGHT_GIVEN 0x0002
#define FIX_HEIGHT_GIVEN (0x0002 << 4)
#define GUESS_CENTER_GIVEN 0x0004
#define FIX_CENTER_GIVEN (0x0004 << 4)
#define GUESS_GAMMA_GIVEN 0x0008
#define FIX_GAMMA_GIVEN (0x0008 << 4)

#define BASELINE_INDEX 0
#define HEIGHT_INDEX 1
#define CENTER_INDEX 2
#define GAMMA_INDEX 3

int main(int argc, char **argv) {
  double *xData = NULL, *yData = NULL, *syData = NULL;
  int64_t nData = 0;
  SDDS_DATASET InputTable, OutputTable;
  SCANNED_ARG *s_arg;
  long i_arg;
  char *input, *output, *xName, *yName, *syName;
  long xIndex, yIndex, fitIndex, residualIndex, syIndex_col, retval;
  double *fitData, *residualData, rmsResidual, chiSqr, sigLevel;
  unsigned long guessFlags, dummyFlags, pipeFlags, majorOrderFlag;
  double gammaGuess, centerGuess, baselineGuess, heightGuess;
  double tolerance, stepSize;
  double a[4], da[4], lower, upper, result;
  double aLow[4], aHigh[4];
  short disable[4] = {0, 0, 0, 0};
  int32_t nEvalMax = 5000, nPassMax = 100;
  long nEval, verbosity, fullOutput = 0;
  int64_t i;
  short columnMajorOrder = -1;
  char *lowerPar, *upperPar, *baselineGuessPar, *gammaGuessPar, *centerGuessPar, *heightGuessPar;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2 || argc > (2 + N_OPTIONS)) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < 4; i++)
    aLow[i] = -(aHigh[i] = DBL_MAX);
  aLow[GAMMA_INDEX] = 0;
  input = output = NULL;
  stepSize = 1e-2;
  tolerance = 1e-8;
  verbosity = 0;
  guessFlags = gammaGuess = heightGuess = baselineGuess = centerGuess = pipeFlags = 0;
  xName = yName = syName = NULL;
  lower = upper = 0;
  lowerPar = upperPar = gammaGuessPar = heightGuessPar = baselineGuessPar = centerGuessPar = NULL;
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
      case SET_FITRANGE:
        if (s_arg[i_arg].n_items != 3)
          SDDS_Bomb("incorrect -fitRange syntax");
        if (s_arg[i_arg].list[1][0] == '@') {
          cp_str(&lowerPar, s_arg[i_arg].list[1] + 1);
        } else if (sscanf(s_arg[i_arg].list[1], "%lf", &lower) != 1)
          SDDS_Bomb("invalid fitRange lower value provided");
        if (s_arg[i_arg].list[2][0] == '@') {
          cp_str(&upperPar, s_arg[i_arg].list[2] + 1);
        } else if (sscanf(s_arg[i_arg].list[2], "%lf", &upper) != 1)
          SDDS_Bomb("invalid fitRange upper value provided");
        break;
      case SET_TOLERANCE:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &tolerance) != 1 || tolerance <= 0)
          SDDS_Bomb("incorrect -tolerance syntax");
        break;
      case SET_STEPSIZE:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &stepSize) != 1 || stepSize <= 0)
          SDDS_Bomb("incorrect -stepSize syntax");
        break;
      case SET_VERBOSITY:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &verbosity) != 1)
          SDDS_Bomb("incorrect -verbosity syntax");
        break;
      case SET_GUESSES:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("incorrect -guesses syntax");
        s_arg[i_arg].n_items -= 1;
        dummyFlags = guessFlags;
        if (!scanItemList(&guessFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "baseline", SDDS_STRING, &baselineGuessPar, 1, GUESS_BASELINE_GIVEN,
                          "height", SDDS_STRING, &heightGuessPar, 1, GUESS_HEIGHT_GIVEN,
                          "center", SDDS_STRING, &centerGuessPar, 1, GUESS_CENTER_GIVEN,
                          "gamma", SDDS_STRING, &gammaGuessPar, 1, GUESS_GAMMA_GIVEN, NULL))
          SDDS_Bomb("invalid -guesses syntax");
        if (baselineGuessPar) {
          if (baselineGuessPar[0] == '@') {
            baselineGuessPar++;
          } else {
            if (sscanf(baselineGuessPar, "%lf", &baselineGuess) != 1)
              SDDS_Bomb("Invalid baseline guess value provided.");
            free(baselineGuessPar);
            baselineGuessPar = NULL;
          }
        }
        if (heightGuessPar) {
          if (heightGuessPar[0] == '@') {
            heightGuessPar++;
          } else {
            if (sscanf(heightGuessPar, "%lf", &heightGuess) != 1)
              SDDS_Bomb("Invalid height guess value provided.");
            free(heightGuessPar);
            heightGuessPar = NULL;
          }
        }
        if (centerGuessPar) {
          if (centerGuessPar[0] == '@') {
            centerGuessPar++;
          } else {
            if (sscanf(centerGuessPar, "%lf", &centerGuess) != 1)
              SDDS_Bomb("Invalid center guess value provided.");
            free(centerGuessPar);
            centerGuessPar = NULL;
          }
        }
        if (gammaGuessPar) {
          if (gammaGuessPar[0] == '@') {
            gammaGuessPar++;
          } else {
            if (sscanf(gammaGuessPar, "%lf", &gammaGuess) != 1)
              SDDS_Bomb("Invalid gamma guess value provided.");
            free(gammaGuessPar);
            gammaGuessPar = NULL;
          }
        }
        if ((dummyFlags >> 4) & guessFlags)
          SDDS_Bomb("can't have -fixValue and -guesses for the same item");
        guessFlags |= dummyFlags;
        break;
      case SET_FIXVALUE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("incorrect -fixValue syntax");
        s_arg[i_arg].n_items -= 1;
        dummyFlags = guessFlags;
        if (!scanItemList(&guessFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "baseline", SDDS_STRING, &baselineGuessPar, 1, FIX_BASELINE_GIVEN,
                          "height", SDDS_STRING, &heightGuessPar, 1, FIX_HEIGHT_GIVEN,
                          "center", SDDS_STRING, &centerGuessPar, 1, FIX_CENTER_GIVEN,
                          "gamma", SDDS_STRING, &gammaGuessPar, 1, FIX_GAMMA_GIVEN, NULL))
          SDDS_Bomb("invalid -fixValue syntax");
        if (dummyFlags & (guessFlags >> 4))
          SDDS_Bomb("can't have -fixValue and -guesses for the same item");
        guessFlags |= dummyFlags;
        if (baselineGuessPar) {
          if (baselineGuessPar[0] == '@') {
            baselineGuessPar++;
          } else {
            if (sscanf(baselineGuessPar, "%lf", &baselineGuess) != 1)
              SDDS_Bomb("Invalid baseline guess value provided.");
            free(baselineGuessPar);
            baselineGuessPar = NULL;
          }
        }
        if (heightGuessPar) {
          if (heightGuessPar[0] == '@') {
            heightGuessPar++;
          } else {
            if (sscanf(heightGuessPar, "%lf", &heightGuess) != 1)
              SDDS_Bomb("Invalid height guess value provided.");
            free(heightGuessPar);
            heightGuessPar = NULL;
          }
        }
        if (centerGuessPar) {
          if (centerGuessPar[0] == '@') {
            centerGuessPar++;
          } else {
            if (sscanf(centerGuessPar, "%lf", &centerGuess) != 1)
              SDDS_Bomb("Invalid center guess value provided.");
            free(centerGuessPar);
            centerGuessPar = NULL;
          }
        }
        if (gammaGuessPar) {
          if (gammaGuessPar[0] == '@') {
            gammaGuessPar++;
          } else {
            if (sscanf(gammaGuessPar, "%lf", &gammaGuess) != 1)
              SDDS_Bomb("Invalid gamma guess value provided.");
            free(gammaGuessPar);
            gammaGuessPar = NULL;
          }
        }
        break;
      case SET_COLUMNS:
        if (s_arg[i_arg].n_items != 3 && s_arg[i_arg].n_items != 4)
          SDDS_Bomb("invalid -columns syntax");
        xName = s_arg[i_arg].list[1];
        yName = s_arg[i_arg].list[2];
        s_arg[i_arg].n_items -= 3;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 3, &s_arg[i_arg].n_items, 0,
                          "ysigma", SDDS_STRING, &syName, 1, 0, NULL))
          SDDS_Bomb("invalid -columns syntax");
        break;
      case SET_FULLOUTPUT:
        fullOutput = 1;
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
        fprintf(stderr, "%s", USAGE);
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

  processFilenames("sddslorentzianfit", &input, &output, pipeFlags, 0, NULL);

  for (i = 0; i < 4; i++) {
    if ((guessFlags >> 4) & (1 << i)) {
      disable[i] = 1;
    }
  }

  if (!xName || !yName) {
    fprintf(stderr, "Error: -columns option must be specified.\n");
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  if (!SDDS_InitializeInput(&InputTable, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_FindColumn(&InputTable, FIND_NUMERIC_TYPE, xName, NULL) ||
      !SDDS_FindColumn(&InputTable, FIND_NUMERIC_TYPE, yName, NULL) ||
      (syName && !SDDS_FindColumn(&InputTable, FIND_NUMERIC_TYPE, syName, NULL)))
    SDDS_Bomb("One or more of the specified data columns do not exist or are non-numeric.");

  setupOutputFile(&OutputTable, &xIndex, &yIndex, &syIndex_col, &fitIndex, &residualIndex,
                  fullOutput, output, &InputTable, xName, yName, syName, columnMajorOrder);

  while ((retval = SDDS_ReadPage(&InputTable)) > 0) {
    xData = yData = syData = NULL;
    fitData = residualData = NULL;
    if (!(xData = SDDS_GetColumnInDoubles(&InputTable, xName)) ||
        !(yData = SDDS_GetColumnInDoubles(&InputTable, yName)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (syName && !(syData = SDDS_GetColumnInDoubles(&InputTable, syName)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (lowerPar && !SDDS_GetParameterAsDouble(&InputTable, lowerPar, &lower))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (upperPar && !SDDS_GetParameterAsDouble(&InputTable, upperPar, &upper))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (baselineGuessPar && !SDDS_GetParameterAsDouble(&InputTable, baselineGuessPar, &baselineGuess))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (heightGuessPar && !SDDS_GetParameterAsDouble(&InputTable, heightGuessPar, &heightGuess))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (centerGuessPar && !SDDS_GetParameterAsDouble(&InputTable, centerGuessPar, &centerGuess))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (gammaGuessPar && !SDDS_GetParameterAsDouble(&InputTable, gammaGuessPar, &gammaGuess))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if ((nData = SDDS_CountRowsOfInterest(&InputTable)) < 5)
      continue;
    if (lower < upper) {
      if ((nDataFit = makeFilteredCopy(&xDataFit, &yDataFit, &syDataFit, xData, yData, syData, nData, lower, upper)) < 5)
        continue;
    } else {
      xDataFit = xData;
      yDataFit = yData;
      syDataFit = syData;
      nDataFit = nData;
    }

    if (!computeStartingPoint(a, da, xDataFit, yDataFit, nDataFit, guessFlags, gammaGuess, centerGuess, baselineGuess, heightGuess, stepSize)) {
      fprintf(stderr, "Error: Couldn't compute starting point for page %ld--skipping\n", retval);
      continue;
    }
    if (verbosity > 2)
      fprintf(stderr, "Starting values:  gamma=%.6e  center=%.6e  baseline=%.6e  height=%.6e\n", a[GAMMA_INDEX], a[CENTER_INDEX], a[BASELINE_INDEX], a[HEIGHT_INDEX]);
    if (verbosity > 3)
      fprintf(stderr, "Starting steps:   gamma=%.6e  center=%.6e  baseline=%.6e  height=%.6e\n", da[GAMMA_INDEX], da[CENTER_INDEX], da[BASELINE_INDEX], da[HEIGHT_INDEX]);

    nEval = simplexMin(&result, a, da, aLow, aHigh, disable, 4, -DBL_MAX, tolerance, fitFunction, (verbosity > 0 ? report : NULL), nEvalMax, nPassMax, 12, 3, 1.0, 0);
    if (xData != xDataFit) {
      free(xDataFit);
      free(yDataFit);
      if (syDataFit)
        free(syDataFit);
    }

    if (verbosity > 3)
      fprintf(stderr, "%ld evaluations of fit function required, giving result %e\n", nEval, result);

    fitData = trealloc(fitData, sizeof(*fitData) * nData);
    residualData = trealloc(residualData, sizeof(*residualData) * nData);
    for (i = result = 0; i < nData; i++) {
      fitData[i] = a[BASELINE_INDEX] + a[HEIGHT_INDEX] / (1 + sqr((xDataFit[i] - a[CENTER_INDEX]) / a[GAMMA_INDEX]));
      residualData[i] = yData[i] - fitData[i];
      result += sqr(residualData[i]);
    }
    rmsResidual = sqrt(result / nData);
    if (syData) {
      for (i = chiSqr = 0; i < nData; i++)
        chiSqr += sqr(residualData[i] / syData[i]);
    } else {
      double sy2;
      sy2 = result / (nData - 4);
      for (i = chiSqr = 0; i < nData; i++)
        chiSqr += sqr(residualData[i]) / sy2;
    }
    sigLevel = ChiSqrSigLevel(chiSqr, nData - 4);
    if (verbosity > 0) {
      fprintf(stderr, "gamma: %.15e\ncenter: %.15e\nbaseline: %.15e\nheight: %.15e\n", a[GAMMA_INDEX], a[CENTER_INDEX], a[BASELINE_INDEX], a[HEIGHT_INDEX]);
    }
    if (verbosity > 1) {
      if (syData)
        fprintf(stderr, "Significance level: %.5e\n", sigLevel);
      fprintf(stderr, "RMS deviation: %.15e\n", rmsResidual);
    }

    if (!SDDS_StartPage(&OutputTable, nData) ||
        !SDDS_CopyParameters(&OutputTable, &InputTable) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, xData, nData, xIndex) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, fitData, nData, fitIndex) ||
        !SDDS_SetParameters(&OutputTable, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME,
                            "lorentzianfitGamma", a[GAMMA_INDEX],
                            "lorentzianfitCenter", a[CENTER_INDEX],
                            "lorentzianfitBaseline", a[BASELINE_INDEX],
                            "lorentzianfitHeight", a[HEIGHT_INDEX],
                            "lorentzianfitRmsResidual", rmsResidual,
                            "lorentzianfitSigLevel", sigLevel, NULL) ||
        (fullOutput &&
         (!SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, yData, nData, yIndex) ||
          !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, residualData, nData, residualIndex) ||
          (syName && !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, syData, nData, syIndex_col)))) ||
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
  if (lowerPar)
    free(lowerPar);
  if (upperPar)
    free(upperPar);
  if (baselineGuessPar)
    free(baselineGuessPar);
  if (heightGuessPar)
    free(heightGuessPar);
  if (gammaGuessPar)
    free(gammaGuessPar);
  free_scanargs(&s_arg, argc);
  return EXIT_SUCCESS;
}

void setupOutputFile(SDDS_DATASET *OutputTable, long *xIndex, long *yIndex, long *syIndex, long *fitIndex,
                     long *residualIndex, long fullOutput, char *output, SDDS_DATASET *InputTable,
                     char *xName, char *yName, char *syName, short columnMajorOrder) {
  char *name, *yUnits, *description, *xUnits;
  int32_t typeValue = SDDS_DOUBLE;
  static char *residualNamePart = "Residual";
  static char *residualDescriptionPart = "Residual of Lorentzian fit to ";

  if (!SDDS_InitializeOutput(OutputTable, SDDS_BINARY, 0, NULL, "sddslorentzianfit output", output) ||
      !SDDS_TransferColumnDefinition(OutputTable, InputTable, xName, NULL) ||
      !SDDS_ChangeColumnInformation(OutputTable, "type", &typeValue, SDDS_BY_NAME, xName) ||
      (*xIndex = SDDS_GetColumnIndex(OutputTable, xName)) < 0 ||
      !SDDS_GetColumnInformation(InputTable, "units", &xUnits, SDDS_BY_NAME, xName) ||
      !SDDS_GetColumnInformation(InputTable, "units", &yUnits, SDDS_BY_NAME, yName))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
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
    if (syName && (!SDDS_TransferColumnDefinition(OutputTable, InputTable, syName, NULL) ||
                   !SDDS_ChangeColumnInformation(OutputTable, "type", &typeValue, SDDS_BY_NAME, syName) ||
                   (*syIndex = SDDS_GetColumnIndex(OutputTable, syName)) < 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    sprintf(name, "%s%s", yName, residualNamePart);
    sprintf(description, "%s%s", yName, residualDescriptionPart);
    if ((*residualIndex = SDDS_DefineColumn(OutputTable, name, NULL, yUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  sprintf(name, "%sFit", yName);
  sprintf(description, "Lorentzian fit to %s", yName);
  if ((*fitIndex = SDDS_DefineColumn(OutputTable, name, NULL, yUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_DefineParameter(OutputTable, "lorentzianfitBaseline", NULL, yUnits, "Baseline from Lorentzian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "lorentzianfitHeight", NULL, yUnits, "Height from Lorentzian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "lorentzianfitCenter", NULL, xUnits, "Center from Lorentzian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "lorentzianfitGamma", NULL, xUnits, "Gamma from Lorentzian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "lorentzianfitRmsResidual", NULL, yUnits, "RMS residual from Lorentzian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "lorentzianfitSigLevel", NULL, NULL, "Significance level from chi-squared test",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      !SDDS_TransferAllParameterDefinitions(OutputTable, InputTable, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(OutputTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

double fitFunction(double *a, long *invalid) {
  double sum, tmp, center, gamma, base, norm;
  int64_t i;

  *invalid = 0;
  gamma = a[GAMMA_INDEX];
  center = a[CENTER_INDEX];
  base = a[BASELINE_INDEX];
  norm = a[HEIGHT_INDEX];

  if (!syDataFit) {
    for (i = sum = 0; i < nDataFit; i++) {
      tmp = (xDataFit[i] - center) / gamma;
      tmp = norm / (1 + sqr(tmp)) + base;
      sum += sqr(tmp - yDataFit[i]);
    }
    return (sum / nDataFit);
  } else {
    for (i = sum = 0; i < nDataFit; i++) {
      tmp = (xDataFit[i] - center) / gamma;
      tmp = norm / (1 + sqr(tmp)) + base;
      sum += sqr((tmp - yDataFit[i]) / syDataFit[i]);
    }
    return (sum / nDataFit);
  }
}

void report(double y, double *x, long pass, long nEval, long n_dimen) {
  long i;

  fprintf(stderr, "Pass %ld, after %ld evaluations: result = %.16e\na = ", pass, nEval, y);
  for (i = 0; i < n_dimen; i++)
    fprintf(stderr, "%.8e ", x[i]);
  fputc('\n', stderr);
}

long computeStartingPoint(double *a, double *da, double *x, double *y, int64_t n, unsigned long guessFlags,
                          double gammaGuess, double centerGuess, double baselineGuess, double heightGuess,
                          double stepSize) {
  double xhalf, dhalf, ymin, ymax, xcenter, tmp, xmax, xmin;
  int64_t i;

  if (n < 5)
    return 0;

  /* First find maximum y value and corresponding x value, plus max x */
  xcenter = 0;
  ymax = xmax = -DBL_MAX;
  ymin = xmin = DBL_MAX;
  for (i = 0; i < n; i++) {
    if (xmax < (tmp = fabs(x[i])))
      xmax = tmp;
    if (xmin < tmp)
      xmin = tmp;
    if (ymax < y[i]) {
      ymax = y[i];
      xcenter = x[i];
    }
    if (ymin > y[i])
      ymin = y[i];
  }

  /* Now find approximate half-max point */
  xhalf = 0;
  dhalf = DBL_MAX;
  for (i = 0; i < n; i++) {
    tmp = fabs(fabs(y[i] - ymax) / (ymax - ymin) - 0.5);
    if (tmp < dhalf) {
      xhalf = x[i];
      dhalf = tmp;
    }
  }
  if (dhalf != DBL_MAX)
    a[GAMMA_INDEX] = fabs(xhalf - xcenter) / 1.177; /* Starting gamma */
  else
    a[GAMMA_INDEX] = xmax - xmin;
  a[CENTER_INDEX] = xcenter;     /* Starting center */
  a[BASELINE_INDEX] = ymin;      /* Starting baseline */
  a[HEIGHT_INDEX] = ymax - ymin; /* Starting height */

  if (guessFlags & (GUESS_GAMMA_GIVEN + FIX_GAMMA_GIVEN))
    a[GAMMA_INDEX] = gammaGuess;
  if (guessFlags & (GUESS_CENTER_GIVEN + FIX_CENTER_GIVEN))
    a[CENTER_INDEX] = centerGuess;
  if (guessFlags & (GUESS_BASELINE_GIVEN + FIX_BASELINE_GIVEN))
    a[BASELINE_INDEX] = baselineGuess;
  if (guessFlags & (GUESS_HEIGHT_GIVEN + FIX_HEIGHT_GIVEN))
    a[HEIGHT_INDEX] = heightGuess;

  /* Step sizes */
  for (i = 0; i < 4; i++)
    if (!(da[i] = a[i] * stepSize))
      da[i] = stepSize;

  return 1;
}

int64_t makeFilteredCopy(double **xFit, double **yFit, double **syFit, double *x, double *y,
                         double *sy, int64_t n, double lower, double upper) {
  int64_t i, j;

  if (!(*xFit = (double *)malloc(sizeof(**xFit) * n)) ||
      !(*yFit = (double *)malloc(sizeof(**yFit) * n)) ||
      (sy && !(*syFit = (double *)malloc(sizeof(**syFit) * n))))
    return 0;

  for (i = j = 0; i < n; i++) {
    if (x[i] < lower || x[i] > upper)
      continue;
    (*xFit)[j] = x[i];
    (*yFit)[j] = y[i];
    if (sy)
      (*syFit)[j] = sy[i];
    j++;
  }
  return j;
}
