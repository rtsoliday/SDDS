/**
 * @file sddsgfit.c
 * @brief A program for performing Gaussian fitting on input data.
 *
 * This program fits input data to a Gaussian function using customizable options. It reads data 
 * from SDDS (Self Describing Data Set) files, processes input columns, and generates fitted results. 
 * Users can specify fit ranges, initial parameter guesses, verbosity levels, and output formats.
 *
 * @section Usage
 * ```
 * sddsgfit [<inputfile>] [<outputfile>]
 *          [-pipe=[input][,output]]
 *           -columns=<x-name>,<y-name>[,ySigma=<sy-name>] 
 *          [-fitRange=<lower>|@<parameter-name>,<upper>|@<parameter-name>] 
 *          [-fullOutput] 
 *          [-verbosity=<integer>] 
 *          [-stepSize=<factor>] 
 *          [-tolerance=<value>] 
 *          [-guesses=[baseline=<value>|@<parameter-name>][,mean=<value>|@<parameter-name>][,height=<value>|@<parameter-name>][,sigma=<value>|@<parameter-name>]] 
 *          [-fixValue=[baseline=<value>|@<parameter-name>][,mean=<value>|@<parameter-name>][,height=<value>|@<parameter-name>][,sigma=<value>|@<parameter-name>]] 
 *          [-limits=[evaluations=<number>][,passes=<number>]] 
 *          [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                                | Description                                                                            |
 * |-----------------------------------------|----------------------------------------------------------------------------------------|
 * | `-columns`                              | Specifies the column names for x and y data.                                           |
 *
 * | Optional                                | Description                                                                            |
 * |-----------------------------------------|----------------------------------------------------------------------------------------|
 * | `-pipe`                                 | Specifies input/output via pipes.                                                     |
 * | `-fitRange`                             | Sets the range for fitting.                                                           |
 * | `-fullOutput`                           | Enables full output, including residuals.                                             |
 * | `-verbosity`                            | Controls verbosity level (higher values give more detailed logs).                     |
 * | `-stepSize`                             | Sets the optimization step size.                                                      |
 * | `-tolerance`                            | Specifies the convergence tolerance.                                                  |
 * | `-guesses`                              | Provides initial parameter guesses (baseline, mean, height, sigma).                   |
 * | `-fixValue`                             | Fixes specific parameter values during fitting.                                       |
 * | `-limits`                               | Sets limits for evaluations and passes.                |
 * | `-majorOrder`                           | Defines the data order for output (row or column major).                               |
 *
 * @subsection Incompatibilities
 * - `-fixValue` is incompatible with `-guesses` for the same parameter.
 *
 * @details
 * The program fits data to the Gaussian function:
 * ```
 * y(n) = baseline + height * exp(-0.5 * (x(n) - mean)^2 / sigma^2)
 * ```
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
  "sddsgfit [<inputfile>] [<outputfile>] [-pipe=[input][,output]]\n"
  "  -columns=<x-name>,<y-name>[,ySigma=<sy-name>]\n"
  "  -fitRange=<lower>|@<parameter-name>,<upper>|@<parameter-name>\n"
  "  -fullOutput\n"
  "  -verbosity=<integer>\n"
  "  -stepSize=<factor>\n"
  "  -tolerance=<value>\n"
  "  -guesses=[baseline=<value>|@<parameter-name>][,mean=<value>|@<parameter-name>]"
  "[,height=<value>|@<parameter-name>][,sigma=<value>|@<parameter-name>]\n"
  "  -fixValue=[baseline=<value>|@<parameter-name>][,mean=<value>|@<parameter-name>]"
  "[,height=<value>|@<parameter-name>][,sigma=<value>|@<parameter-name>]\n"
  "  -limits=[evaluations=<number>][,passes=<number>]\n"
  "  -majorOrder=row|column\n\n"
  "Performs a Gaussian fit of the form:\n"
  "  y = <baseline> + <height> * exp(-0.5 * (x - <mean>)^2 / <sigma>^2)\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

void report(double res, double *a, long pass, long n_eval, long n_dimen);
void setupOutputFile(SDDS_DATASET *OutputTable, long *xIndex, long *yIndex, long *syIndex, long *fitIndex,
                     long *residualIndex, long fullOutput, char *output, SDDS_DATASET *InputTable,
                     char *xName, char *yName, char *syName, short columnMajorOrder);
long computeStartingPoint(double *a, double *da, double *x, double *y, int64_t n, unsigned long guessFlags,
                          double sigmaGuess, double meanGuess, double baselineGuess, double heightGuess,
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
#define GUESS_MEAN_GIVEN 0x0004
#define FIX_MEAN_GIVEN (0x0004 << 4)
#define GUESS_SIGMA_GIVEN 0x0008
#define FIX_SIGMA_GIVEN (0x0008 << 4)

#define BASELINE_INDEX 0
#define HEIGHT_INDEX 1
#define MEAN_INDEX 2
#define SIGMA_INDEX 3

int main(int argc, char **argv) {
  double *xData = NULL, *yData = NULL, *syData = NULL;
  int64_t nData = 0;
  SDDS_DATASET InputTable, OutputTable;
  SCANNED_ARG *s_arg;
  long i_arg;
  char *input, *output, *xName, *yName, *syName;
  long xIndex, yIndex, fitIndex, residualIndex, syIndex_out, retval;
  double *fitData, *residualData, rmsResidual, chiSqr, sigLevel;
  unsigned long guessFlags, dummyFlags, pipeFlags, majorOrderFlag;
  double sigmaGuess, meanGuess, baselineGuess, heightGuess;
  double tolerance, stepSize;
  double a[4], da[4], lower, upper, result;
  double aLow[4], aHigh[4];
  short disable[4] = {0, 0, 0, 0};
  int32_t nEvalMax = 5000, nPassMax = 100;
  long nEval, verbosity, fullOutput = 0;
  int64_t i;
  short columnMajorOrder = -1;
  char *lowerPar, *upperPar, *baselineGuessPar, *sigmaGuessPar, *meanGuessPar, *heightGuessPar;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2 || argc > (2 + N_OPTIONS)) {
    bomb(NULL, USAGE);
  }

  for (i = 0; i < 4; i++) {
    aLow[i] = -(aHigh[i] = DBL_MAX);
  }
  aLow[SIGMA_INDEX] = 0;
  input = output = NULL;
  stepSize = 1e-2;
  tolerance = 1e-8;
  verbosity = 0;
  guessFlags = sigmaGuess = heightGuess = baselineGuess = meanGuess = pipeFlags = 0;
  xName = yName = syName = NULL;
  lower = upper = 0;
  lowerPar = upperPar = sigmaGuessPar = heightGuessPar = baselineGuessPar = meanGuessPar = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1,
                                                       &s_arg[i_arg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                                                       "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER) {
          columnMajorOrder = 1;
        } else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER) {
          columnMajorOrder = 0;
        }
        break;

      case SET_FITRANGE:
        if (s_arg[i_arg].n_items != 3) {
          SDDS_Bomb("incorrect -fitRange syntax");
        }
        if (s_arg[i_arg].list[1][0] == '@') {
          cp_str(&lowerPar, s_arg[i_arg].list[1] + 1);
        } else if (sscanf(s_arg[i_arg].list[1], "%lf", &lower) != 1) {
          SDDS_Bomb("invalid fitRange lower value provided");
        }
        if (s_arg[i_arg].list[2][0] == '@') {
          cp_str(&upperPar, s_arg[i_arg].list[2] + 1);
        } else if (sscanf(s_arg[i_arg].list[2], "%lf", &upper) != 1) {
          SDDS_Bomb("invalid fitRange upper value provided");
        }
        break;

      case SET_TOLERANCE:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &tolerance) != 1 ||
            tolerance <= 0) {
          SDDS_Bomb("incorrect -tolerance syntax");
        }
        break;

      case SET_STEPSIZE:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &stepSize) != 1 ||
            stepSize <= 0) {
          SDDS_Bomb("incorrect -stepSize syntax");
        }
        break;

      case SET_VERBOSITY:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &verbosity) != 1) {
          SDDS_Bomb("incorrect -verbosity syntax");
        }
        break;

      case SET_GUESSES:
        if (s_arg[i_arg].n_items < 2) {
          SDDS_Bomb("incorrect -guesses syntax");
        }
        s_arg[i_arg].n_items -= 1;
        dummyFlags = guessFlags;
        if (!scanItemList(&guessFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "baseline", SDDS_STRING, &baselineGuessPar, 1, GUESS_BASELINE_GIVEN,
                          "height", SDDS_STRING, &heightGuessPar, 1, GUESS_HEIGHT_GIVEN,
                          "mean", SDDS_STRING, &meanGuessPar, 1, GUESS_MEAN_GIVEN,
                          "sigma", SDDS_STRING, &sigmaGuessPar, 1, GUESS_SIGMA_GIVEN, NULL)) {
          SDDS_Bomb("invalid -guesses syntax");
        }
        if (baselineGuessPar) {
          if (baselineGuessPar[0] == '@') {
            baselineGuessPar++;
          } else {
            if (sscanf(baselineGuessPar, "%lf", &baselineGuess) != 1) {
              SDDS_Bomb("Invalid baseline guess value provided.");
            }
            free(baselineGuessPar);
            baselineGuessPar = NULL;
          }
        }
        if (heightGuessPar) {
          if (heightGuessPar[0] == '@') {
            heightGuessPar++;
          } else {
            if (sscanf(heightGuessPar, "%lf", &heightGuess) != 1) {
              SDDS_Bomb("Invalid height guess value provided.");
            }
            free(heightGuessPar);
            heightGuessPar = NULL;
          }
        }
        if (meanGuessPar) {
          if (meanGuessPar[0] == '@') {
            meanGuessPar++;
          } else {
            if (sscanf(meanGuessPar, "%lf", &meanGuess) != 1) {
              SDDS_Bomb("Invalid mean guess value provided.");
            }
            free(meanGuessPar);
            meanGuessPar = NULL;
          }
        }
        if (sigmaGuessPar) {
          if (sigmaGuessPar[0] == '@') {
            sigmaGuessPar++;
          } else {
            if (sscanf(sigmaGuessPar, "%lf", &sigmaGuess) != 1) {
              SDDS_Bomb("Invalid sigma guess value provided.");
            }
            free(sigmaGuessPar);
            sigmaGuessPar = NULL;
          }
        }
        if ((dummyFlags >> 4) & guessFlags) {
          SDDS_Bomb("can't have -fixedValue and -guesses for the same item");
        }
        guessFlags |= dummyFlags;
        break;

      case SET_FIXVALUE:
        if (s_arg[i_arg].n_items < 2) {
          SDDS_Bomb("incorrect -fixValue syntax");
        }
        s_arg[i_arg].n_items -= 1;
        dummyFlags = guessFlags;
        if (!scanItemList(&guessFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "baseline", SDDS_STRING, &baselineGuessPar, 1, FIX_BASELINE_GIVEN,
                          "height", SDDS_STRING, &heightGuessPar, 1, FIX_HEIGHT_GIVEN,
                          "mean", SDDS_STRING, &meanGuessPar, 1, FIX_MEAN_GIVEN,
                          "sigma", SDDS_STRING, &sigmaGuessPar, 1, FIX_SIGMA_GIVEN, NULL)) {
          SDDS_Bomb("invalid -fixValue syntax");
        }
        if (dummyFlags & (guessFlags >> 4)) {
          SDDS_Bomb("can't have -fixValue and -guesses for the same item");
        }
        guessFlags |= dummyFlags;
        if (baselineGuessPar) {
          if (baselineGuessPar[0] == '@') {
            baselineGuessPar++;
          } else {
            if (sscanf(baselineGuessPar, "%lf", &baselineGuess) != 1) {
              SDDS_Bomb("Invalid baseline guess value provided.");
            }
            free(baselineGuessPar);
            baselineGuessPar = NULL;
          }
        }
        if (heightGuessPar) {
          if (heightGuessPar[0] == '@') {
            heightGuessPar++;
          } else {
            if (sscanf(heightGuessPar, "%lf", &heightGuess) != 1) {
              SDDS_Bomb("Invalid height guess value provided.");
            }
            free(heightGuessPar);
            heightGuessPar = NULL;
          }
        }
        if (meanGuessPar) {
          if (meanGuessPar[0] == '@') {
            meanGuessPar++;
          } else {
            if (sscanf(meanGuessPar, "%lf", &meanGuess) != 1) {
              SDDS_Bomb("Invalid mean guess value provided.");
            }
            free(meanGuessPar);
            meanGuessPar = NULL;
          }
        }
        if (sigmaGuessPar) {
          if (sigmaGuessPar[0] == '@') {
            sigmaGuessPar++;
          } else {
            if (sscanf(sigmaGuessPar, "%lf", &sigmaGuess) != 1) {
              SDDS_Bomb("Invalid sigma guess value provided.");
            }
            free(sigmaGuessPar);
            sigmaGuessPar = NULL;
          }
        }
        break;

      case SET_COLUMNS:
        if (s_arg[i_arg].n_items != 3 && s_arg[i_arg].n_items != 4) {
          SDDS_Bomb("invalid -columns syntax");
        }
        xName = s_arg[i_arg].list[1];
        yName = s_arg[i_arg].list[2];
        s_arg[i_arg].n_items -= 3;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 3, &s_arg[i_arg].n_items, 0,
                          "ysigma", SDDS_STRING, &syName, 1, 0, NULL)) {
          SDDS_Bomb("invalid -columns syntax");
        }
        break;

      case SET_FULLOUTPUT:
        fullOutput = 1;
        break;

      case SET_LIMITS:
        if (s_arg[i_arg].n_items < 2) {
          SDDS_Bomb("incorrect -limits syntax");
        }
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "evaluations", SDDS_LONG, &nEvalMax, 1, 0,
                          "passes", SDDS_LONG, &nPassMax, 1, 0, NULL) ||
            nEvalMax <= 0 || nPassMax <= 0) {
          SDDS_Bomb("invalid -limits syntax");
        }
        break;

      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags)) {
          SDDS_Bomb("invalid -pipe syntax");
        }
        break;

      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (input == NULL) {
        input = s_arg[i_arg].list[0];
      } else if (output == NULL) {
        output = s_arg[i_arg].list[0];
      } else {
        SDDS_Bomb("too many filenames");
      }
    }
  }

  processFilenames("sddsgfit", &input, &output, pipeFlags, 0, NULL);

  for (i = 0; i < 4; i++) {
    if ((guessFlags >> 4) & (1 << i)) {
      disable[i] = 1;
    }
  }

  if (!xName || !yName) {
    SDDS_Bomb("-columns option must be given");
  }

  if (!SDDS_InitializeInput(&InputTable, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_FindColumn(&InputTable, FIND_NUMERIC_TYPE, xName, NULL) ||
      !SDDS_FindColumn(&InputTable, FIND_NUMERIC_TYPE, yName, NULL) ||
      (syName && !SDDS_FindColumn(&InputTable, FIND_NUMERIC_TYPE, syName, NULL))) {
    SDDS_Bomb("one or more of the given data columns is nonexistent or nonnumeric");
  }

  setupOutputFile(&OutputTable, &xIndex, &yIndex, &syIndex_out, &fitIndex, &residualIndex,
                  fullOutput, output, &InputTable, xName, yName, syName, columnMajorOrder);

  while ((retval = SDDS_ReadPage(&InputTable)) > 0) {
    xData = yData = syData = NULL;
    fitData = residualData = NULL;
    if (!(xData = SDDS_GetColumnInDoubles(&InputTable, xName)) ||
        !(yData = SDDS_GetColumnInDoubles(&InputTable, yName))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (syName && !(syData = SDDS_GetColumnInDoubles(&InputTable, syName))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (lowerPar && !SDDS_GetParameterAsDouble(&InputTable, lowerPar, &lower)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (upperPar && !SDDS_GetParameterAsDouble(&InputTable, upperPar, &upper)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (baselineGuessPar && !SDDS_GetParameterAsDouble(&InputTable, baselineGuessPar, &baselineGuess)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (heightGuessPar && !SDDS_GetParameterAsDouble(&InputTable, heightGuessPar, &heightGuess)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (meanGuessPar && !SDDS_GetParameterAsDouble(&InputTable, meanGuessPar, &meanGuess)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (sigmaGuessPar && !SDDS_GetParameterAsDouble(&InputTable, sigmaGuessPar, &sigmaGuess)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if ((nData = SDDS_CountRowsOfInterest(&InputTable)) < 5) {
      continue;
    }
    if (lower < upper) {
      if ((nDataFit = makeFilteredCopy(&xDataFit, &yDataFit, &syDataFit, xData, yData, syData, nData, lower, upper)) < 5) {
        continue;
      }
    } else {
      xDataFit = xData;
      yDataFit = yData;
      syDataFit = syData;
      nDataFit = nData;
    }

    if (!computeStartingPoint(a, da, xDataFit, yDataFit, nDataFit, guessFlags, sigmaGuess, meanGuess,
                              baselineGuess, heightGuess, stepSize)) {
      fprintf(stderr, "error: couldn't compute starting point for page %ld--skipping\n", retval);
      continue;
    }
    if (verbosity > 2) {
      fprintf(stderr, "starting values:  sigma=%.6e  mean=%.6e  baseline=%.6e  height=%.6e\n",
              a[SIGMA_INDEX], a[MEAN_INDEX], a[BASELINE_INDEX], a[HEIGHT_INDEX]);
    }
    if (verbosity > 3) {
      fprintf(stderr, "starting steps:   sigma=%.6e  mean=%.6e  baseline=%.6e  height=%.6e\n",
              da[SIGMA_INDEX], da[MEAN_INDEX], da[BASELINE_INDEX], da[HEIGHT_INDEX]);
    }

    nEval = simplexMin(&result, a, da, aLow, aHigh, disable, 4, -DBL_MAX, tolerance,
                       fitFunction, (verbosity > 0 ? report : NULL),
                       nEvalMax, nPassMax, 12, 3, 1.0, 0);
    if (xData != xDataFit) {
      free(xDataFit);
      free(yDataFit);
      if (syDataFit) {
        free(syDataFit);
      }
    }

    if (verbosity > 3) {
      fprintf(stderr, "%ld evaluations of fit function required, giving result %e\n", nEval, result);
    }

    fitData = trealloc(fitData, sizeof(*fitData) * nData);
    residualData = trealloc(residualData, sizeof(*residualData) * nData);
    for (i = 0, result = 0; i < nData; i++) {
      fitData[i] = a[BASELINE_INDEX] + a[HEIGHT_INDEX] * exp(-ipow((xData[i] - a[MEAN_INDEX]) / a[SIGMA_INDEX], 2) / 2);
      residualData[i] = yData[i] - fitData[i];
      result += sqr(residualData[i]);
    }
    rmsResidual = sqrt(result / nData);
    if (syData) {
      for (i = 0, chiSqr = 0; i < nData; i++) {
        chiSqr += sqr(residualData[i] / syData[i]);
      }
    } else {
      double sy2 = result / (nData - 4);
      for (i = 0, chiSqr = 0; i < nData; i++) {
        chiSqr += sqr(residualData[i]) / sy2;
      }
    }
    sigLevel = ChiSqrSigLevel(chiSqr, nData - 4);
    if (verbosity > 0) {
      fprintf(stderr, "sigma: %.15e\nmean: %.15e\nbaseline: %.15e\nheight: %.15e\n",
              a[SIGMA_INDEX], a[MEAN_INDEX], a[BASELINE_INDEX], a[HEIGHT_INDEX]);
    }
    if (verbosity > 1) {
      if (syData) {
        fprintf(stderr, "Significance level: %.5e\n", sigLevel);
      }
      fprintf(stderr, "RMS deviation: %.15e\n", rmsResidual);
    }

    if (!SDDS_StartPage(&OutputTable, nData) ||
        !SDDS_CopyParameters(&OutputTable, &InputTable) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, xData, nData, xIndex) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, fitData, nData, fitIndex) ||
        !SDDS_SetParameters(&OutputTable, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME,
                            "gfitSigma", a[SIGMA_INDEX],
                            "gfitMean", a[MEAN_INDEX],
                            "gfitBaseline", a[BASELINE_INDEX],
                            "gfitHeight", a[HEIGHT_INDEX],
                            "gfitRmsResidual", rmsResidual,
                            "gfitSigLevel", sigLevel, NULL) ||
        (fullOutput &&
         (!SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, yData, nData, yIndex) ||
          !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, residualData, nData, residualIndex) ||
          (syName && !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, syData, nData, syIndex_out)))) ||
        !SDDS_WritePage(&OutputTable)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (xData) {
      free(xData);
    }
    if (yData) {
      free(yData);
    }
    if (syData) {
      free(syData);
    }
    if (fitData) {
      free(fitData);
    }
    if (residualData) {
      free(residualData);
    }
  }

  if (!SDDS_Terminate(&InputTable) || !SDDS_Terminate(&OutputTable)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (lowerPar) {
    free(lowerPar);
  }
  if (upperPar) {
    free(upperPar);
  }
  if (baselineGuessPar) {
    free(baselineGuessPar);
  }
  if (heightGuessPar) {
    free(heightGuessPar);
  }
  if (sigmaGuessPar) {
    free(sigmaGuessPar);
  }
  free_scanargs(&s_arg, argc);
  return EXIT_SUCCESS;
}

void setupOutputFile(SDDS_DATASET *OutputTable, long *xIndex, long *yIndex, long *syIndex, long *fitIndex,
                     long *residualIndex, long fullOutput, char *output, SDDS_DATASET *InputTable,
                     char *xName, char *yName, char *syName, short columnMajorOrder) {
  char *name, *yUnits, *description, *xUnits;
  int32_t typeValue = SDDS_DOUBLE;
  static char *residualNamePart = "Residual";
  static char *residualDescriptionPart = "Residual of Gaussian fit to ";

  if (!SDDS_InitializeOutput(OutputTable, SDDS_BINARY, 0, NULL, "sddsgfit output", output) ||
      !SDDS_TransferColumnDefinition(OutputTable, InputTable, xName, NULL) ||
      !SDDS_ChangeColumnInformation(OutputTable, "type", &typeValue, SDDS_BY_NAME, xName) ||
      (*xIndex = SDDS_GetColumnIndex(OutputTable, xName)) < 0 ||
      !SDDS_GetColumnInformation(InputTable, "units", &xUnits, SDDS_BY_NAME, xName) ||
      !SDDS_GetColumnInformation(InputTable, "units", &yUnits, SDDS_BY_NAME, yName)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (columnMajorOrder != -1) {
    OutputTable->layout.data_mode.column_major = columnMajorOrder;
  } else {
    OutputTable->layout.data_mode.column_major = InputTable->layout.data_mode.column_major;
  }

  name = tmalloc(sizeof(*name) * (strlen(yName) + strlen(residualNamePart) + 1));
  description = tmalloc(sizeof(*description) * (strlen(yName) + strlen(residualDescriptionPart) + 1));

  if (fullOutput) {
    if (!SDDS_TransferColumnDefinition(OutputTable, InputTable, yName, NULL) ||
        !SDDS_ChangeColumnInformation(OutputTable, "type", &typeValue, SDDS_BY_NAME, yName) ||
        (*yIndex = SDDS_GetColumnIndex(OutputTable, yName)) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (syName && (!SDDS_TransferColumnDefinition(OutputTable, InputTable, syName, NULL) ||
                   !SDDS_ChangeColumnInformation(OutputTable, "type", &typeValue, SDDS_BY_NAME, syName) ||
                   (*syIndex = SDDS_GetColumnIndex(OutputTable, syName)) < 0)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    sprintf(name, "%s%s", yName, residualNamePart);
    sprintf(description, "%s%s", yName, residualDescriptionPart);
    if ((*residualIndex = SDDS_DefineColumn(OutputTable, name, NULL, yUnits, description, NULL, SDDS_DOUBLE, 0)) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  sprintf(name, "%sFit", yName);
  sprintf(description, "Gaussian fit to %s", yName);
  if ((*fitIndex = SDDS_DefineColumn(OutputTable, name, NULL, yUnits, description, NULL, SDDS_DOUBLE, 0)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (SDDS_DefineParameter(OutputTable, "gfitBaseline", NULL, yUnits, "Baseline from Gaussian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "gfitHeight", NULL, yUnits, "Height from Gaussian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "gfitMean", "$gm$r", xUnits, "Mean from Gaussian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "gfitSigma", "$gs$r", xUnits, "Sigma from Gaussian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "gfitRmsResidual", NULL, yUnits, "RMS residual from Gaussian fit",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(OutputTable, "gfitSigLevel", NULL, NULL, "Significance level from chi-squared test",
                           NULL, SDDS_DOUBLE, 0) < 0 ||
      !SDDS_TransferAllParameterDefinitions(OutputTable, InputTable, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(OutputTable)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  free(name);
  free(description);
}

double fitFunction(double *a, long *invalid) {
  double sum, tmp, mean, sigma, base, norm;
  int64_t i;

  *invalid = 0;
  sigma = a[SIGMA_INDEX];
  mean = a[MEAN_INDEX];
  base = a[BASELINE_INDEX];
  norm = a[HEIGHT_INDEX];

  if (!syDataFit) {
    for (i = sum = 0; i < nDataFit; i++) {
      tmp = (xDataFit[i] - mean) / sigma;
      tmp = yDataFit[i] - base - norm * exp(-sqr(tmp) / 2);
      sum += sqr(tmp);
    }
    return (sum / nDataFit);
  } else {
    for (i = sum = 0; i < nDataFit; i++) {
      tmp = (xDataFit[i] - mean) / sigma;
      tmp = (yDataFit[i] - base - norm * exp(-sqr(tmp) / 2)) / syDataFit[i];
      sum += sqr(tmp);
    }
    return (sum / nDataFit);
  }
}

void report(double y, double *x, long pass, long nEval, long n_dimen) {
  long i;

  fprintf(stderr, "pass %ld, after %ld evaluations: result = %.16e\na = ", pass, nEval, y);
  for (i = 0; i < n_dimen; i++) {
    fprintf(stderr, "%.8e ", x[i]);
  }
  fputc('\n', stderr);
}

long computeStartingPoint(double *a, double *da, double *x, double *y, int64_t n, unsigned long guessFlags,
                          double sigmaGuess, double meanGuess, double baselineGuess, double heightGuess,
                          double stepSize) {
  double xhalf, dhalf, ymin, ymax, xcenter, tmp, xmax, xmin;
  int64_t i;

  if (n < 5) {
    return 0;
  }

  /* first find maximum y value and corresponding x value, plus max x */
  xcenter = 0;
  ymax = xmax = -DBL_MAX;
  ymin = xmin = DBL_MAX;
  for (i = 0; i < n; i++) {
    if (xmax < fabs(x[i])) {
      xmax = fabs(x[i]);
    }
    if (xmin > fabs(x[i])) { /* Corrected comparison from < to > */
      xmin = fabs(x[i]);
    }
    if (ymax < y[i]) {
      ymax = y[i];
      xcenter = x[i];
    }
    if (ymin > y[i]) {
      ymin = y[i];
    }
  }

  /*  now find approximate half-max point */
  xhalf = 0;
  dhalf = DBL_MAX;
  for (i = 0; i < n; i++) {
    tmp = fabs((fabs(y[i] - ymax) / (ymax - ymin)) - 0.5);
    if (tmp < dhalf) {
      xhalf = x[i];
      dhalf = tmp;
    }
  }
  if (dhalf != DBL_MAX) {
    a[SIGMA_INDEX] = fabs(xhalf - xcenter) / 1.177; /* starting sigma */
  } else {
    a[SIGMA_INDEX] = xmax - xmin;
  }
  a[MEAN_INDEX] = xcenter;       /* starting mean */
  a[BASELINE_INDEX] = ymin;      /* starting baseline */
  a[HEIGHT_INDEX] = ymax - ymin; /* starting height */

  if (guessFlags & (GUESS_SIGMA_GIVEN + FIX_SIGMA_GIVEN)) {
    a[SIGMA_INDEX] = sigmaGuess;
  }
  if (guessFlags & (GUESS_MEAN_GIVEN + FIX_MEAN_GIVEN)) {
    a[MEAN_INDEX] = meanGuess;
  }
  if (guessFlags & (GUESS_BASELINE_GIVEN + FIX_BASELINE_GIVEN)) {
    a[BASELINE_INDEX] = baselineGuess;
  }
  if (guessFlags & (GUESS_HEIGHT_GIVEN + FIX_HEIGHT_GIVEN)) {
    a[HEIGHT_INDEX] = heightGuess;
  }

  /* step sizes */
  for (i = 0; i < 4; i++) {
    if (!(da[i] = a[i] * stepSize))
      da[i] = stepSize;
  }

  return 1;
}

int64_t makeFilteredCopy(double **xFit, double **yFit, double **syFit, double *x, double *y,
                         double *sy, int64_t n, double lower, double upper) {
  int64_t i, j;

  if (!(*xFit = (double *)malloc(sizeof(**xFit) * n)) ||
      !(*yFit = (double *)malloc(sizeof(**yFit) * n)) ||
      (sy && !(*syFit = (double *)malloc(sizeof(**syFit) * n)))) {
    return 0;
  }

  for (i = j = 0; i < n; i++) {
    if (x[i] < lower || x[i] > upper) {
      continue;
    }
    (*xFit)[j] = x[i];
    (*yFit)[j] = y[i];
    if (sy) {
      (*syFit)[j] = sy[i];
    }
    j++;
  }
  return j;
}
