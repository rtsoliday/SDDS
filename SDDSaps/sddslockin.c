/**
 * @file sddslockin.c
 * @brief Performs lock-in (synchronous) detection on SDDS formatted data.
 *
 * @details
 * For each page of the input file, the program computes the amplitude and phase of one or
 * more dependent columns at each of a list of frequencies, using lock-in detection against
 * the independent column.  Dependent columns may be given as a list of names, wildcard
 * patterns, or any combination; every numeric column matching any entry is analyzed once
 * (the independent column is never selected by a wildcard).  For frequency f, with N rows of data (x_i, y_i),
 *
 *   S = (1/N) sum y_i sin(2 pi f x_i),   C = (1/N) sum y_i cos(2 pi f x_i),
 *   Amplitude = 2 sqrt(S^2 + C^2),       Phase = atan2(C, S),
 *
 * so that y = A sin(2 pi f x + phi) gives Amplitude = A and Phase = phi.  The estimate is
 * exact when the data span an integer number of periods with uniform sampling; otherwise
 * it has leakage of order 1/(number of periods).  In that case a nonzero mean of y also
 * leaks into the result, which -removeMean prevents by subtracting the page mean of y first.
 * The frequency is in units of the inverse of the independent column's units (e.g., cycles
 * per pass for an independent column Pass).
 *
 * The output file has one page per input page, with a column Frequency and, for each
 * dependent column <name>, columns <name>Amplitude and <name>Phase and a parameter
 * <name>MeanRemoved.  Parameters of the input file are transferred to the output.
 *
 * @section Usage
 * ```
 * sddslockin [<inputfile>] [<outputfile>]
 *            [-pipe=[input][,output]]
 *             -column=<independentVariable>,<dependentVariable>[,...]
 *            {-atValues=<listOfFrequencies> | -fileValues=<filename>,<columnName> |
 *             -sequence=<points>[,<start>,<end>] | -equispaced=<spacing>[,<start>,<end>]}
 *            [-removeMean]
 *            [-threads=<number>]
 * ```
 *
 * @section Options
 * | Required                       | Description                                                       |
 * |--------------------------------|-------------------------------------------------------------------|
 * | `-column`                      | Independent column, then dependent column names and/or wildcards. |
 * | `-atValues`                    | Compute the amplitude at the listed frequencies.                  |
 * | `-fileValues`                  | Compute the amplitude at the frequencies in a column of a file.   |
 * | `-sequence`                    | Compute the amplitude at <points> equally spaced frequencies.     |
 * | `-equispaced`                  | Compute the amplitude at frequencies with the given spacing.      |
 *
 * | Option                         | Description                                                       |
 * |--------------------------------|-------------------------------------------------------------------|
 * | `-pipe`                        | Use pipe for input and/or output.                                 |
 * | `-removeMean`                  | Subtract the page mean of the dependent column before detection.  |
 * | `-threads`                     | Number of threads (default 1).                                    |
 *
 * @subsection Incompatibilities
 * - One and only one of `-atValues`, `-fileValues`, `-sequence` and `-equispaced` must be specified.
 * - For `-sequence` and `-equispaced` without <start>,<end>, the range is 0 to the Nyquist frequency
 *   of each page, 1/(2 dx), where dx is the mean spacing of the independent column.
 *
 * @copyright
 *   - (c) 2026 The University of Chicago, as Operator of Argonne National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * Claude Code, M. Borland
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "SDDSutils.h"

/* Enumeration for option types */
enum option_type {
  CLO_COLUMN,
  CLO_ATVALUES,
  CLO_FILEVALUES,
  CLO_PIPE,
  CLO_REMOVEMEAN,
  CLO_THREADS,
  CLO_SEQUENCE,
  CLO_EQUISPACED,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "column",
  "atvalues",
  "filevalues",
  "pipe",
  "removemean",
  "threads",
  "sequence",
  "equispaced",
};

static char *USAGE =
    "sddslockin [<inputfile>] [<outputfile>]\n"
    "           [-pipe=[input][,output]]\n"
    "            -column=<independentVariable>,<dependentVariable>[,...]\n"
    "           {-atValues=<listOfFrequencies> | -fileValues=<filename>,<columnName> |\n"
    "            -sequence=<points>[,<start>,<end>] | -equispaced=<spacing>[,<start>,<end>]}\n"
    "           [-removeMean] [-threads=<number>]\n"
    "  Options:\n"
    "    -pipe=[input][,output]                         Use pipe for input and/or output.\n"
    "    -column=<independent>,<dependent>[,...]        Specify the independent column and one or more dependent\n"
    "                                                   columns, as names and/or wildcard patterns in any\n"
    "                                                   combination.\n"
    "    -atValues=<listOfFrequencies>                  Compute the amplitude at the listed frequencies.\n"
    "    -fileValues=<filename>,<columnName>            Compute the amplitude at the frequencies in the named\n"
    "                                                   column of the first page of the file.\n"
    "    -sequence=<points>[,<start>,<end>]             Compute the amplitude at <points> equally spaced frequencies.\n"
    "    -equispaced=<spacing>[,<start>,<end>]          Compute the amplitude at frequencies with the given spacing.\n"
    "                                                   For -sequence and -equispaced, the default range is 0 to\n"
    "                                                   the Nyquist frequency 1/(2 dx) of each page, where dx is\n"
    "                                                   the mean spacing of the independent column.\n"
    "    -removeMean                                    Subtract the page mean of the dependent column before\n"
    "                                                   detection, so that it cannot leak into the result.\n"
    "    -threads=<number>                              Number of threads (default 1).\n\n"
    "  For each input page, each frequency f and each dependent column y, the output gives\n"
    "    <y>Amplitude = 2 sqrt(S^2+C^2) and <y>Phase = atan2(C,S), where S and C are the averages over\n"
    "    rows of y sin(2 pi f x) and y cos(2 pi f x), so that y = A sin(2 pi f x + phi) yields A and phi.\n"
    "  Frequencies are in units of the inverse of the independent column's units.\n\n"
    "  Example:\n"
    "    sddslockin run.w1 run.lockin -column=Pass,Cdelta -atValues=9.342e-4,1.868e-3\n"
    "    sddslockin run.w1 run.lockin -column=Pass,Cx,C[yt]* -atValues=9.342e-4 -removeMean\n"
    "    sddslockin run.w1 run.lockin -column=Pass,Cdelta -sequence=1001,0,0.02 -threads=16\n\n"
    "  Program by Claude Code, Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static void lockin(double *x, double **y, long columns, int64_t rows, double frequency,
                   double *amplitude, double *phase, int64_t stride, int threads);
static double *makeFrequencySequence(int64_t *points, long sequencePoints, double spacing, short rangeGiven,
                                     double start, double end, double *x, int64_t rows);

int main(int argc, char **argv) {
  int iArg;
  char *input, *output, *indepName, **depenName, *valuesFile, *valuesColumn;
  char *frequencyUnits, *indepUnits, *depenUnits, buffer[SDDS_MAXLINE];
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout, SDDSvalues;
  unsigned long pipeFlags;
  double *frequency, *amplitude, *phase, *x, **y, *mean;
  int64_t frequencies, rows, i;
  long readCode, depenNames, j;
  short removeMean, rangeGiven;
  int threads;
  long sequencePoints;
  double sequenceSpacing, sequenceStart, sequenceEnd;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  input = output = indepName = valuesFile = valuesColumn = NULL;
  depenName = NULL;
  depenNames = 0;
  frequency = NULL;
  frequencies = 0;
  pipeFlags = 0;
  removeMean = 0;
  threads = 1;
  sequencePoints = 0;
  sequenceSpacing = sequenceStart = sequenceEnd = 0;
  rangeGiven = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_COLUMN:
        if (scanned[iArg].n_items < 3)
          SDDS_Bomb("invalid -column syntax: give -column=<independentVariable>,<dependentVariable>[,...]");
        if (indepName)
          SDDS_Bomb("give -column only once");
        indepName = scanned[iArg].list[1];
        depenNames = scanned[iArg].n_items - 2;
        depenName = tmalloc(sizeof(*depenName) * depenNames);
        for (j = 0; j < depenNames; j++)
          depenName[j] = scanned[iArg].list[j + 2];
        break;
      case CLO_ATVALUES:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -atValues syntax");
        if (frequency)
          SDDS_Bomb("give -atValues only once");
        frequency = tmalloc(sizeof(*frequency) * (frequencies = scanned[iArg].n_items - 1));
        for (i = 0; i < frequencies; i++)
          if (sscanf(scanned[iArg].list[i + 1], "%lf", &frequency[i]) != 1)
            SDDS_Bomb("invalid -atValues value");
        break;
      case CLO_FILEVALUES:
        if (scanned[iArg].n_items != 3)
          SDDS_Bomb("invalid -fileValues syntax: give -fileValues=<filename>,<columnName>");
        if (valuesFile)
          SDDS_Bomb("give -fileValues only once");
        valuesFile = scanned[iArg].list[1];
        valuesColumn = scanned[iArg].list[2];
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_REMOVEMEAN:
        if (scanned[iArg].n_items != 1)
          SDDS_Bomb("invalid -removeMean syntax");
        removeMean = 1;
        break;
      case CLO_SEQUENCE:
        if ((scanned[iArg].n_items != 2 && scanned[iArg].n_items != 4) ||
            sscanf(scanned[iArg].list[1], "%ld", &sequencePoints) != 1 ||
            sequencePoints < 2)
          SDDS_Bomb("invalid -sequence syntax/value");
        if (scanned[iArg].n_items == 4) {
          if (sscanf(scanned[iArg].list[2], "%lf", &sequenceStart) != 1 ||
              sscanf(scanned[iArg].list[3], "%lf", &sequenceEnd) != 1 ||
              sequenceEnd <= sequenceStart)
            SDDS_Bomb("invalid -sequence syntax/value");
          rangeGiven = 1;
        }
        if (sequenceSpacing)
          SDDS_Bomb("give only one of -sequence and -equispaced");
        break;
      case CLO_EQUISPACED:
        if ((scanned[iArg].n_items != 2 && scanned[iArg].n_items != 4) ||
            sscanf(scanned[iArg].list[1], "%lf", &sequenceSpacing) != 1 ||
            sequenceSpacing <= 0)
          SDDS_Bomb("invalid -equispaced syntax/value");
        if (scanned[iArg].n_items == 4) {
          if (sscanf(scanned[iArg].list[2], "%lf", &sequenceStart) != 1 ||
              sscanf(scanned[iArg].list[3], "%lf", &sequenceEnd) != 1 ||
              sequenceEnd <= sequenceStart)
            SDDS_Bomb("invalid -equispaced syntax/values");
          rangeGiven = 1;
        }
        if (sequencePoints)
          SDDS_Bomb("give only one of -sequence and -equispaced");
        break;
      case CLO_THREADS:
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%d", &threads) != 1 || threads < 1)
          SDDS_Bomb("invalid -threads syntax");
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s\n", scanned[iArg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  processFilenames("sddslockin", &input, &output, pipeFlags, 0, NULL);

  if (!indepName)
    SDDS_Bomb("supply the independent and dependent columns with -column");
  if ((frequency ? 1 : 0) + (valuesFile ? 1 : 0) + (sequencePoints ? 1 : 0) + (sequenceSpacing > 0 ? 1 : 0) != 1)
    SDDS_Bomb("you must give one and only one of -atValues, -fileValues, -sequence and -equispaced");

  if (valuesFile) {
    if (!SDDS_InitializeInput(&SDDSvalues, valuesFile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (SDDS_CheckColumn(&SDDSvalues, valuesColumn, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY)
      SDDS_Bomb("frequency column for -fileValues is missing or not numeric");
    if (SDDS_ReadPage(&SDDSvalues) < 1)
      SDDS_Bomb("unable to read the -fileValues file");
    if ((frequencies = SDDS_RowCount(&SDDSvalues)) < 1)
      SDDS_Bomb("no frequencies in the -fileValues file");
    if (!(frequency = SDDS_GetColumnInDoubles(&SDDSvalues, valuesColumn)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_Terminate(&SDDSvalues))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_CheckColumn(&SDDSin, indepName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY)
    SDDS_Bomb("the independent column must exist and be numeric");
  /* explicitly named (non-wildcard) dependent columns must exist; wildcards may match nothing */
  for (j = 0; j < depenNames; j++)
    if (!has_wildcards(depenName[j]) &&
        SDDS_CheckColumn(&SDDSin, depenName[j], NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY)
      SDDS_Bomb("dependent columns named in -column must exist and be numeric");
  if ((depenNames = expandColumnPairNames(&SDDSin, &depenName, NULL, depenNames, &indepName, 1, FIND_NUMERIC_TYPE, 0)) <= 0)
    SDDS_Bomb("no numeric dependent columns match the -column list");

  indepUnits = frequencyUnits = NULL;
  if (!SDDS_GetColumnInformation(&SDDSin, "units", &indepUnits, SDDS_GET_BY_NAME, indepName))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (indepUnits && strlen(indepUnits)) {
    frequencyUnits = tmalloc(sizeof(*frequencyUnits) * (strlen(indepUnits) + 5));
    sprintf(frequencyUnits, "1/(%s)", indepUnits);
  }

  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 0, NULL, "sddslockin output", output) ||
      !SDDS_TransferAllParameterDefinitions(&SDDSout, &SDDSin, 0) ||
      SDDS_DefineColumn(&SDDSout, "Frequency", NULL, frequencyUnits, "Lock-in frequency", NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineParameter(&SDDSout, "LockinIndependentVariable", NULL, NULL, NULL, NULL, SDDS_STRING, indepName) < 0 ||
      SDDS_DefineParameter(&SDDSout, "LockinRows", NULL, NULL, "Number of data rows used", NULL, SDDS_LONG64, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (j = 0; j < depenNames; j++) {
    depenUnits = NULL;
    if (!SDDS_GetColumnInformation(&SDDSin, "units", &depenUnits, SDDS_GET_BY_NAME, depenName[j]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    sprintf(buffer, "%sAmplitude", depenName[j]);
    if (SDDS_DefineColumn(&SDDSout, buffer, NULL, depenUnits, "Lock-in amplitude", NULL, SDDS_DOUBLE, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    sprintf(buffer, "%sPhase", depenName[j]);
    if (SDDS_DefineColumn(&SDDSout, buffer, NULL, "rad", "Lock-in phase", NULL, SDDS_DOUBLE, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    sprintf(buffer, "%sMeanRemoved", depenName[j]);
    if (SDDS_DefineParameter(&SDDSout, buffer, NULL, depenUnits, "Mean subtracted before detection (0 without -removeMean)",
                             NULL, SDDS_DOUBLE, NULL) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (depenUnits)
      free(depenUnits);
  }
  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  amplitude = phase = NULL;
  y = tmalloc(sizeof(*y) * depenNames);
  mean = tmalloc(sizeof(*mean) * depenNames);

  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    rows = SDDS_RowCount(&SDDSin);
    x = NULL;
    for (j = 0; j < depenNames; j++) {
      y[j] = NULL;
      mean[j] = 0;
    }
    if (rows > 0) {
      if (!(x = SDDS_GetColumnInDoubles(&SDDSin, indepName)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (j = 0; j < depenNames; j++)
        if (!(y[j] = SDDS_GetColumnInDoubles(&SDDSin, depenName[j])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (removeMean) {
        for (j = 0; j < depenNames; j++) {
          double sum = 0, *yj = y[j];
#pragma omp parallel for reduction(+ : sum) if (threads > 1) num_threads(threads)
          for (i = 0; i < rows; i++)
            sum += yj[i];
          mean[j] = sum / rows;
#pragma omp parallel for if (threads > 1) num_threads(threads)
          for (i = 0; i < rows; i++)
            yj[i] -= mean[j];
        }
      }
    }
    if (sequencePoints || sequenceSpacing > 0) {
      /* the default range depends on the page's sampling, so build the list for each page */
      if (frequency)
        free(frequency);
      if (!(frequency = makeFrequencySequence(&frequencies, sequencePoints, sequenceSpacing, rangeGiven,
                                              sequenceStart, sequenceEnd, x, rows)))
        SDDS_Bomb("unable to determine the frequency range: give <start>,<end> for -sequence/-equispaced");
    }
    /* results for column j at frequency i are stored at [j * frequencies + i] */
    amplitude = trealloc(amplitude, sizeof(*amplitude) * frequencies * depenNames);
    phase = trealloc(phase, sizeof(*phase) * frequencies * depenNames);
    if (threads > 1 && frequencies >= threads) {
      /* enough frequencies to keep every thread busy: one frequency per iteration */
#pragma omp parallel for schedule(dynamic) num_threads(threads)
      for (i = 0; i < frequencies; i++)
        lockin(x, y, depenNames, rows, frequency[i], amplitude + i, phase + i, frequencies, 1);
    } else {
      /* few frequencies: thread the sums over rows instead */
      for (i = 0; i < frequencies; i++)
        lockin(x, y, depenNames, rows, frequency[i], amplitude + i, phase + i, frequencies, threads);
    }
    if (!SDDS_StartPage(&SDDSout, frequencies) ||
        !SDDS_CopyParameters(&SDDSout, &SDDSin) ||
        !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "LockinRows", rows, NULL) ||
        !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, frequency, frequencies, "Frequency"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (j = 0; j < depenNames; j++) {
      sprintf(buffer, "%sAmplitude", depenName[j]);
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, amplitude + j * frequencies, frequencies, buffer))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      sprintf(buffer, "%sPhase", depenName[j]);
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, phase + j * frequencies, frequencies, buffer))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      sprintf(buffer, "%sMeanRemoved", depenName[j]);
      if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, buffer, mean[j], NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (x)
      free(x);
    for (j = 0; j < depenNames; j++)
      if (y[j])
        free(y[j]);
  }

  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(frequency);
  free(amplitude);
  free(phase);
  free(y);
  free(mean);
  if (frequencyUnits)
    free(frequencyUnits);
  if (indepUnits)
    free(indepUnits);
  free_scanargs(&scanned, argc);
  return EXIT_SUCCESS;
}

/* Lock-in amplitude and phase of each column y[j](x) at the given frequency, summing over rows
   with the given number of threads.  The sine and cosine are computed once per row and shared
   by all columns.  Results for column j go to amplitude[j * stride] and phase[j * stride].
   Pages with no rows give NaN. */
static void lockin(double *x, double **y, long columns, int64_t rows, double frequency,
                   double *amplitude, double *phase, int64_t stride, int threads) {
  double *sSum, *cSum, omega;
  int64_t i;
  long j;

  if (rows < 1) {
    for (j = 0; j < columns; j++)
      amplitude[j * stride] = phase[j * stride] = NAN;
    return;
  }
  sSum = calloc(columns, sizeof(*sSum));
  cSum = calloc(columns, sizeof(*cSum));
  if (!sSum || !cSum)
    SDDS_Bomb("memory allocation failure");
  omega = PIx2 * frequency;
#pragma omp parallel for private(j) reduction(+ : sSum[:columns], cSum[:columns]) if (threads > 1) num_threads(threads)
  for (i = 0; i < rows; i++) {
    double s = sin(omega * x[i]), c = cos(omega * x[i]);
    for (j = 0; j < columns; j++) {
      sSum[j] += y[j][i] * s;
      cSum[j] += y[j][i] * c;
    }
  }
  for (j = 0; j < columns; j++) {
    sSum[j] /= rows;
    cSum[j] /= rows;
    amplitude[j * stride] = 2 * sqrt(sSum[j] * sSum[j] + cSum[j] * cSum[j]);
    phase[j * stride] = atan2(cSum[j], sSum[j]);
  }
  free(sSum);
  free(cSum);
}

/* Frequency list for -sequence (sequencePoints > 0) or -equispaced (spacing > 0).  Without an
   explicit range, use 0 to the Nyquist frequency 1/(2 dx) of the page, where dx is the mean
   spacing of the independent column.  Returns NULL if the range cannot be determined. */
static double *makeFrequencySequence(int64_t *points, long sequencePoints, double spacing, short rangeGiven,
                                     double start, double end, double *x, int64_t rows) {
  double delta, xMin, xMax, *sequence;
  int64_t i;

  if (!rangeGiven) {
    if (rows < 2 || !find_min_max(&xMin, &xMax, x, rows) || xMax <= xMin)
      return NULL;
    start = 0;
    end = 0.5 * (rows - 1) / (xMax - xMin);
  }
  if (sequencePoints > 1) {
    *points = sequencePoints;
    delta = (end - start) / (sequencePoints - 1);
  } else {
    delta = spacing;
    *points = (int64_t)((end - start) / delta + 1.5);
    if (*points < 1)
      return NULL;
  }
  sequence = tmalloc(sizeof(*sequence) * (*points));
  for (i = 0; i < *points; i++)
    sequence[i] = start + delta * i;
  return sequence;
}
