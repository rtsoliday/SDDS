/**
 * @file sddsnaff.c
 * @brief Analyzes signals in SDDS files to extract frequency components using Laskar's NAFF method.
 *
 * @details
 * This program processes SDDS (Self Describing Data Sets) files to determine fundamental frequencies,
 * amplitudes, phases, and significances of signal components. It uses the Numerical Analysis of
 * Fundamental Frequencies (NAFF) method, offering configurable options for truncation, exclusion, paired column analysis,
 * and FFT performance tuning. Outputs can be formatted in row-major or column-major order.
 *
 * @section Usage
 * ```
 * sddsnaff [<inputfile>] [<outputfile>]
 *          [-pipe=[input][,output]]
 *          [-columns=<indep-variable>[,<depen-quantity>[,...]]]
 *          [-pair=<column1>,<column2>]
 *          [-exclude=<depen-quantity>[,...]]
 *          [-terminateSearch={changeLimit=<fraction>[,maxFrequencies=<number>] | frequencies=<number>}]
 *          [-iterateFrequency=[cycleLimit=<number>][,accuracyLimit=<fraction>]]
 *          [-truncate]
 *          [-noWarnings]
 *          [-majorOrder=row|column]
 * ```

 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Specify the independent variable and dependent quantities to analyze. Wildcards are allowed. |

 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use SDDS pipe options for input/output redirection.                                   |
 * | `-pair`                               | Specify a column pair for frequency and phase analysis.                              |
 * | `-exclude`                            | Exclude specified quantities from analysis using wildcards.                          |
 * | `-terminateSearch`                    | Define termination criteria for frequency search, based on RMS change or count.      |
 * | `-iterateFrequency`                   | Configure iteration parameters for frequency computation accuracy and cycles.        |
 * | `-truncate`                           | Truncate data for FFT optimization.                                                  |
 * | `-noWarnings`                         | Suppress warning messages.                                                           |
 * | `-majorOrder`                         | Define the output format: row-major or column-major.                                 |

 * @subsection Incompatibilities
 * - `-columns` and `-pair` cannot be used simultaneously to specify dependent quantities.
 * - Only one of the following may be specified:
 *   - `-terminateSearch`
 *   - `-iterateFrequency`
 *
 * @subsection Requirements
 * - `-terminateSearch` requires either `changeLimit` or `frequencies` to be defined.
 * - `-iterateFrequency` requires at least one of `cycleLimit` or `accuracyLimit`.

 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * M. Borland,
 * R. Soliday,
 * H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "fftpackC.h"
#include "SDDSutils.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_TRUNCATE,
  SET_COLUMN,
  SET_EXCLUDE,
  SET_PIPE,
  SET_NOWARNINGS,
  SET_TERM_SEARCH,
  SET_ITERATE_FREQ,
  SET_PAIR,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "truncate",
  "columns",
  "exclude",
  "pipe",
  "nowarnings",
  "terminatesearch",
  "iteratefrequency",
  "pair",
  "majorOrder",
};

static char *USAGE1 =
  "Usage: sddsnaff [<inputfile>] [<outputfile>]\n"
  "       [-pipe=[input][,output]]\n"
  "       [-columns=<indep-variable>[,<depen-quantity>[,...]]]\n"
  "       [-pair=<column1>,<column2>]\n"
  "       [-exclude=<depen-quantity>[,...]]\n"
  "       [-terminateSearch={changeLimit=<fraction>[,maxFrequencies=<number>] | frequencies=<number>}]\n"
  "       [-iterateFrequency=[cycleLimit=<number>][,accuracyLimit=<fraction>]]\n"
  "       [-truncate]\n"
  "       [-noWarnings]\n"
  "       [-majorOrder=row|column]\n\n"
  "Determines frequency components of signals using Laskar's NAFF method.\n"
  "FFTs are involved in this process, hence some parameters refer to FFT configurations.\n\n"
  "Options:\n"
  "  -pipe             Use standard SDDS Toolkit pipe option for input and/or output.\n"
  "  -columns          Specify the independent variable and dependent quantities to analyze.\n"
  "                    <depen-quantity> entries may include wildcards.\n"
  "  -pair             Specify a pair of columns for frequency and phase analysis.\n"
  "                    Multiple -pair options can be provided.\n"
  "  -exclude          Exclude specified quantities from analysis using wildcard patterns.\n"
  "  -terminateSearch  Terminate the search based on RMS change limit or a specific number of frequencies.\n"
  "  -iterateFrequency Configure iteration parameters for frequency determination.\n"
  "  -truncate         Truncate data to optimize FFT performance.\n"
  "  -noWarnings       Suppress warning messages.\n"
  "  -majorOrder       Specify output file's data order as row-major or column-major.\n\n";

static char *USAGE2 =
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

long SetupNAFFOutput(SDDS_DATASET *SDDSout, char *output, SDDS_DATASET *SDDSin,
                     char *indepQuantity, long depenQuantities, char **depenQuantity, long **frequencyIndex,
                     long **amplitudeIndex, long **phaseIndex, long **significanceIndex, char **depenQuantityPair,
                     long **amplitudeIndex1, long **phaseIndex1, long **significanceIndex1, short columnMajorOrder);

#ifdef DEBUG
double trialFn(double x, long *invalid) {
  *invalid = 0;
  x -= 1;
  if (x == 0)
    return 1;
  return (sin(x) / x + sqr(x - 0.5) * x * 0.5);
}
#endif

int main(int argc, char **argv) {
  char *indepQuantity, **depenQuantity, **exclude, **depenQuantityPair;
  long depenQuantities, excludes;
  char *input, *output;
  long iArg, j, readCode, noWarnings, items;
  int64_t i, rows, rowsToUse;
  unsigned long flags, pairFlags, tmpFlags, pipeFlags, majorOrderFlag;
  SCANNED_ARG *scArg;
  SDDS_DATASET SDDSin, SDDSout;
  double *tdata, *data, t0, dt;
  double fracRMSChangeLimit, fracFreqAccuracyLimit;
  int32_t frequenciesDesired, maxFrequencies, freqCycleLimit;
  short truncate;
  double *frequency, *amplitude = NULL, *phase = NULL, *significance = NULL, *phase1 = NULL, *amplitude1 = NULL, *significance1 = NULL;
  long *frequencyIndex, *amplitudeIndex, *phaseIndex, *significanceIndex, pairs;
  long *amplitudeIndex1, *phaseIndex1, *significanceIndex1;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);

#ifdef DEBUG
  if (1) {
    long code;
    double x, y;
    x = 1.1;
    code = OneDFunctionOptimize(&y, &x, 0.07, -4, 4, trialFn, 50, 1e-6, 0, 1);
    fprintf(stderr, "code: %ld   x=%e,  y=%e\n", code, x, y);

    x = .9;
    code = OneDFunctionOptimize(&y, &x, 0.15, -4, 4, trialFn, 50, 1e-6, 0, 1);
    fprintf(stderr, "code: %ld   x=%e,  y=%e\n", code, x, y);

    x = .999;
    code = OneDFunctionOptimize(&y, &x, 0.11, -4, 4, trialFn, 50, 1e-6, 0, 1);
    fprintf(stderr, "code: %ld   x=%e,  y=%e\n", code, x, y);
    exit(EXIT_SUCCESS);
  }
#endif

  argc = scanargs(&scArg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s%s", USAGE1, USAGE2);
    exit(EXIT_FAILURE);
  }
  output = input = NULL;
  flags = pipeFlags = excludes = truncate = pairFlags = 0;
  indepQuantity = NULL;
  depenQuantity = exclude = depenQuantityPair = NULL;
  depenQuantities = 0;
  noWarnings = 0;
  fracRMSChangeLimit = 0.0;
  fracFreqAccuracyLimit = 0.00001;
  frequenciesDesired = 1;
  maxFrequencies = 4;
  freqCycleLimit = 100;
  pairs = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scArg[iArg].arg_type == OPTION) {
      /* Process options */
      switch (match_string(scArg[iArg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scArg[iArg].n_items--;
        if (scArg[iArg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;

      case SET_TRUNCATE:
        truncate = 1;
        break;

      case SET_PAIR:
        if (depenQuantities)
          SDDS_Bomb("Invalid -pair option, the depen-quantity is provided by -columns option already.");
        if (scArg[iArg].n_items != 3)
          SDDS_Bomb("invalid -pair syntax");
        depenQuantity = SDDS_Realloc(depenQuantity, sizeof(*depenQuantity) * (pairs + 1));
        depenQuantityPair = SDDS_Realloc(depenQuantityPair, sizeof(*depenQuantityPair) * (pairs + 1));
        depenQuantity[pairs] = scArg[iArg].list[1];
        depenQuantityPair[pairs] = scArg[iArg].list[2];
        pairs++;
        break;

      case SET_COLUMN:
        if (indepQuantity)
          SDDS_Bomb("only one -columns option may be given");
        if (scArg[iArg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        indepQuantity = scArg[iArg].list[1];
        if (scArg[iArg].n_items >= 2) {
          if (pairs)
            SDDS_Bomb("Invalid -columns syntax, the depen-quantity is provided by -pair option already.");
          depenQuantity = tmalloc(sizeof(*depenQuantity) * (depenQuantities = scArg[iArg].n_items - 2));
          for (i = 0; i < depenQuantities; i++)
            depenQuantity[i] = scArg[iArg].list[i + 2];
        }
        break;

      case SET_PIPE:
        if (!processPipeOption(scArg[iArg].list + 1, scArg[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;

      case SET_EXCLUDE:
        if (scArg[iArg].n_items < 2)
          SDDS_Bomb("invalid -exclude syntax");
        moveToStringArray(&exclude, &excludes, scArg[iArg].list + 1, scArg[iArg].n_items - 1);
        break;

      case SET_NOWARNINGS:
        noWarnings = 1;
        break;

      case SET_TERM_SEARCH:
        items = scArg[iArg].n_items - 1;
        flags &= ~(NAFF_RMS_CHANGE_LIMIT | NAFF_FREQS_DESIRED | NAFF_MAX_FREQUENCIES);
        fracRMSChangeLimit = 0;
        frequenciesDesired = 0;
        maxFrequencies = 10;
        if (!scanItemList(&tmpFlags, scArg[iArg].list + 1, &items, 0,
                          "changelimit", SDDS_DOUBLE, &fracRMSChangeLimit, 1, NAFF_RMS_CHANGE_LIMIT,
                          "maxfrequencies", SDDS_LONG, &maxFrequencies, 1, NAFF_MAX_FREQUENCIES,
                          "frequencies", SDDS_LONG, &frequenciesDesired, 1, NAFF_FREQS_DESIRED, NULL) ||
            (tmpFlags & NAFF_RMS_CHANGE_LIMIT && tmpFlags & NAFF_FREQS_DESIRED) ||
            maxFrequencies < 1) {
          SDDS_Bomb("invalid -terminateSearch syntax");
        }
        flags |= tmpFlags;
        if (frequenciesDesired)
          maxFrequencies = frequenciesDesired;
        break;

      case SET_ITERATE_FREQ:
        items = scArg[iArg].n_items - 1;
        flags &= ~(NAFF_FREQ_CYCLE_LIMIT | NAFF_FREQ_ACCURACY_LIMIT);
        if (!scanItemList(&tmpFlags, scArg[iArg].list + 1, &items, 0,
                          "cyclelimit", SDDS_LONG, &freqCycleLimit, 1, NAFF_FREQ_CYCLE_LIMIT,
                          "accuracylimit", SDDS_DOUBLE, &fracFreqAccuracyLimit, 1, NAFF_FREQ_ACCURACY_LIMIT, NULL) ||
            !bitsSet(tmpFlags) ||
            freqCycleLimit < 2) {
          SDDS_Bomb("invalid -iterateFrequency syntax");
        }
        flags |= tmpFlags;
        break;

      default:
        fprintf(stderr, "Error: unknown or ambiguous option: %s\n", scArg[iArg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (!input)
        input = scArg[iArg].list[0];
      else if (!output)
        output = scArg[iArg].list[0];
      else
        SDDS_Bomb("too many filenames provided");
    }
  }

  processFilenames("sddsnaff", &input, &output, pipeFlags, 0, NULL);

  if (!indepQuantity)
    SDDS_Bomb("Supply the independent quantity name with the -columns option");

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_CheckColumn(&SDDSin, indepQuantity, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY)
    exit(EXIT_FAILURE);

  excludes = appendToStringArray(&exclude, excludes, indepQuantity);
  if (pairs) {
    pairFlags = flags | NAFF_FREQ_FOUND;
    depenQuantities = pairs;
  }
  if (!depenQuantities)
    depenQuantities = appendToStringArray(&depenQuantity, depenQuantities, "*");
  if (!pairs) {
    if ((depenQuantities = expandColumnPairNames(&SDDSin, &depenQuantity, NULL, depenQuantities, exclude, excludes, FIND_NUMERIC_TYPE, 0)) <= 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_Bomb("No quantities selected to FFT");
    }
  }

  if (!SetupNAFFOutput(&SDDSout, output, &SDDSin, indepQuantity, depenQuantities, depenQuantity,
                       &frequencyIndex, &amplitudeIndex, &phaseIndex, &significanceIndex,
                       depenQuantityPair, &amplitudeIndex1, &phaseIndex1, &significanceIndex1,
                       columnMajorOrder)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!(frequency = SDDS_Malloc(sizeof(*frequency) * maxFrequencies)) ||
      !(amplitude = SDDS_Malloc(sizeof(*amplitude) * maxFrequencies)) ||
      !(phase = SDDS_Malloc(sizeof(*phase) * maxFrequencies)) ||
      !(significance = SDDS_Malloc(sizeof(*significance) * maxFrequencies))) {
    SDDS_Bomb("Memory allocation failure");
  }
  if (pairs) {
    if (!(amplitude1 = SDDS_Malloc(sizeof(*amplitude1) * maxFrequencies)) ||
        !(phase1 = SDDS_Malloc(sizeof(*phase1) * maxFrequencies)) ||
        !(significance1 = SDDS_Malloc(sizeof(*significance1) * maxFrequencies))) {
      SDDS_Bomb("Memory allocation failure");
    }
  }

  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&SDDSin)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (rows) {
      int64_t primeRows;
      rowsToUse = rows;
      primeRows = greatestProductOfSmallPrimes(rows);
      if (rows != primeRows) {
        if (truncate)
          rowsToUse = greatestProductOfSmallPrimes(rows);
        else if (largest_prime_factor(rows) > 100 && !noWarnings)
          fputs("Warning: Number of points has large prime factors.\n"
                "This could take a very long time.\nConsider using the -truncate option.\n",
                stderr);
      }
      if (!SDDS_StartPage(&SDDSout, maxFrequencies) ||
          !SDDS_CopyParameters(&SDDSout, &SDDSin)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!(tdata = SDDS_GetColumnInDoubles(&SDDSin, indepQuantity)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i = 1; i < rowsToUse; i++)
        if (tdata[i] <= tdata[i - 1])
          SDDS_Bomb("Independent data is not monotonically increasing");
      dt = (tdata[rowsToUse - 1] - tdata[0]) / (rowsToUse - 1.0);
      t0 = tdata[0];
      free(tdata);
      tdata = NULL;
      for (i = 0; i < depenQuantities; i++) {
        if (!(data = SDDS_GetColumnInDoubles(&SDDSin, depenQuantity[i])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (j = 0; j < maxFrequencies; j++)
          frequency[j] = amplitude[j] = phase[j] = significance[j] = -1;
        PerformNAFF(frequency, amplitude, phase, significance, t0, dt, data, rowsToUse, flags,
                    fracRMSChangeLimit, maxFrequencies, freqCycleLimit, fracFreqAccuracyLimit, 0, 0);
#ifdef DEBUG
        fprintf(stderr, "Column %s: ", depenQuantity[i]);
        fprintf(stderr, "f=%10.3e  a=%10.3e  p=%10.3e  s=%10.3e\n", frequency[0], amplitude[0], phase[0], significance[0]);
#endif
        free(data);
        data = NULL;
        if (pairs) {
          if (!(data = SDDS_GetColumnInDoubles(&SDDSin, depenQuantityPair[i])))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          PerformNAFF(frequency, amplitude1, phase1, significance1, t0, dt, data, rowsToUse,
                      pairFlags, fracRMSChangeLimit, maxFrequencies, freqCycleLimit, fracFreqAccuracyLimit, 0, 0);

          for (j = 0; j < maxFrequencies; j++)
            if (frequency[j] != -1)
              frequency[j] = adjustFrequencyHalfPlane(frequency[j], phase[j], phase1[j], dt);
          free(data);
          data = NULL;
        }
        if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, frequency, maxFrequencies, frequencyIndex[i]) ||
            !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, amplitude, maxFrequencies, amplitudeIndex[i]) ||
            !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, phase, maxFrequencies, phaseIndex[i]) ||
            !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, significance, maxFrequencies, significanceIndex[i])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (pairs) {
          if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, amplitude1, maxFrequencies, amplitudeIndex1[i]) ||
              !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, phase1, maxFrequencies, phaseIndex1[i]) ||
              !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, significance1, maxFrequencies, significanceIndex1[i])) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
      }
    } else {
      if (!SDDS_StartPage(&SDDSout, 0) || !SDDS_CopyParameters(&SDDSout, &SDDSin)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  free(frequency);
  free(amplitude);
  free(phase);
  free(significance);
  if (pairs) {
    free(amplitude1);
    free(phase1);
    free(significance1);
    free(amplitudeIndex1);
    free(phaseIndex1);
    free(significanceIndex1);
    free(depenQuantityPair);
  }
  free(depenQuantity);
  free(frequencyIndex);
  free(amplitudeIndex);
  free(phaseIndex);
  free(significanceIndex);
  return EXIT_SUCCESS;
}

long SetupNAFFOutput(SDDS_DATASET *SDDSout, char *output, SDDS_DATASET *SDDSin,
                     char *indepQuantity, long depenQuantities, char **depenQuantity, long **frequencyIndex,
                     long **amplitudeIndex, long **phaseIndex, long **significanceIndex, char **depenQuantityPair,
                     long **amplitudeIndex1, long **phaseIndex1, long **significanceIndex1, short columnMajorOrder) {
  char *freqUnits, *buffer, *ampUnits;
  long i, maxBuffer, bufferNeeded;

  if (!(*frequencyIndex = SDDS_Malloc(sizeof(**frequencyIndex) * depenQuantities)) ||
      !(*amplitudeIndex = SDDS_Malloc(sizeof(**amplitudeIndex) * depenQuantities)) ||
      !(*phaseIndex = SDDS_Malloc(sizeof(**phaseIndex) * depenQuantities)) ||
      !(*significanceIndex = SDDS_Malloc(sizeof(**significanceIndex) * depenQuantities)) ||
      !(buffer = SDDS_Malloc(sizeof(*buffer) * (maxBuffer = 1024)))) {
    SDDS_SetError("Memory allocation failure");
    return 0;
  }
  if (!(freqUnits = makeFrequencyUnits(SDDSin, indepQuantity)) ||
      !SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddsnaff output", output)) {
    return 0;
  }
  if (columnMajorOrder != -1)
    SDDSout->layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout->layout.data_mode.column_major = SDDSin->layout.data_mode.column_major;

  for (i = 0; i < depenQuantities; i++) {
    if ((bufferNeeded = strlen(depenQuantity[i]) + 12) > maxBuffer &&
        !(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * bufferNeeded))) {
      SDDS_Bomb("Memory allocation failure");
    }
    sprintf(buffer, "%sFrequency", depenQuantity[i]);
    if (((*frequencyIndex)[i] = SDDS_DefineColumn(SDDSout, buffer, NULL, freqUnits, NULL, NULL, SDDS_DOUBLE, 0)) < 0 ||
        SDDS_GetColumnInformation(SDDSin, "units", &ampUnits, SDDS_GET_BY_NAME, depenQuantity[i]) != SDDS_STRING) {
      return 0;
    }
    sprintf(buffer, "%sAmplitude", depenQuantity[i]);
    if (((*amplitudeIndex)[i] = SDDS_DefineColumn(SDDSout, buffer, NULL, ampUnits, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    sprintf(buffer, "%sPhase", depenQuantity[i]);
    if (((*phaseIndex)[i] = SDDS_DefineColumn(SDDSout, buffer, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    sprintf(buffer, "%sSignificance", depenQuantity[i]);
    if (((*significanceIndex)[i] = SDDS_DefineColumn(SDDSout, buffer, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
  }
  if (depenQuantityPair) {
    if (!(*amplitudeIndex1 = SDDS_Malloc(sizeof(**amplitudeIndex1) * depenQuantities)) ||
        !(*phaseIndex1 = SDDS_Malloc(sizeof(**phaseIndex1) * depenQuantities)) ||
        !(*significanceIndex1 = SDDS_Malloc(sizeof(**significanceIndex1) * depenQuantities))) {
      SDDS_SetError("Memory allocation failure");
      return 0;
    }
    for (i = 0; i < depenQuantities; i++) {
      if ((bufferNeeded = strlen(depenQuantityPair[i]) + 12) > maxBuffer &&
          !(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * bufferNeeded))) {
        SDDS_Bomb("Memory allocation failure");
      }
      sprintf(buffer, "%sAmplitude", depenQuantityPair[i]);
      if (((*amplitudeIndex1)[i] = SDDS_DefineColumn(SDDSout, buffer, NULL, ampUnits, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
        return 0;
      sprintf(buffer, "%sPhase", depenQuantityPair[i]);
      if (((*phaseIndex1)[i] = SDDS_DefineColumn(SDDSout, buffer, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
        return 0;
      sprintf(buffer, "%sSignificance", depenQuantityPair[i]);
      if (((*significanceIndex1)[i] = SDDS_DefineColumn(SDDSout, buffer, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
        return 0;
    }
  }
  free(ampUnits);
  free(freqUnits);

  if (!SDDS_TransferAllParameterDefinitions(SDDSout, SDDSin, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(SDDSout)) {
    return 0;
  }
  free(buffer);
  return 1;
}
