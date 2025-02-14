/**
 * @file sddslocaldensity.c
 * @brief Computes the local density of data points using methods such as fraction, spread, or Kernel Density Estimation (KDE).
 *
 * @details
 * This program processes input data to compute local densities using specified methods such as 
 * `fraction`, `spread`, or `KDE`. KDE supports advanced configuration options for binning, grid 
 * output, sampling, and bandwidth adjustments. The tool supports multithreading for improved 
 * performance and can handle large datasets efficiently.
 *
 * @section Usage
 * ```
 * sddslocaldensity [<inputfile>] [<outputfile>]
 *                  [-pipe=[input][,output]]
 *                   -columns=<normalizationMode>,<name>[,...]
 *                  {
 *                   -fraction=<value> |
 *                   -spread=<value> |
 *                   -kde=bins=<number>[,gridoutput=<filename>][,nsigma=<value>][,explimit=<value>]
 *                     [,sample=<fraction>|use=<number>][,spanPages]
 *                  }
 *                  [-output=<columnName>]
 *                  [-weight=<columnName>]
 *                  [-threads=<number>]
 *                  [-verbose]
 * ```
 *
 * @section Options
 * | Required            | Description                                                                           |
 * |---------------------|---------------------------------------------------------------------------------------|
 * | `-columns`          | Specifies columns to include and their normalization mode (`none`, `range`, or `rms`).              |
 *
 * | Option              | Description                                                                                          |
 * |---------------------|------------------------------------------------------------------------------------------------------|
 * | `-pipe`             | Standard SDDS Toolkit pipe option for input and output redirection.                                 |
 * | `-threads`          | Specifies the number of threads to use for computation.                                             |
 * | `-fraction`         | Fraction of the range to identify "nearby" points.                                                  |
 * | `-spread`           | Standard deviation of the weighting function as a fraction of the range.                            |
 * | `-kde`              | Enables n-dimensional Kernel Density Estimation with options for bins, grid output, and sampling.    |
 * | `-output`           | Specifies the output column name (default: `LocalDensity`).                                         |
 * | `-weight`           | Name of the column for weighting the contributions of each point.                                    |
 * | `-verbose`          | Prints progress information while running.                                                          |
 *
 * @subsection Incompatibilities
 * - `-kde=spanPages` is incompatible with the `-pipe` option.
 * - The `-weight` option is only supported for KDE mode.
 * - One and only one of the following must be specified:
 *   - `-fraction`
 *   - `-spread`
 *   - `-kde`
 *
 * @subsection Requirements
 * - For the `-kde` option:
 *   - The `bins` value must be >= 3.
 *   - `nsigma` must be >= 1.
 *   - The `explimit` must be within (0, 1].
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
 * M. Borland, R. Soliday, N. Kuklev
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include <ctype.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_math.h>

#if defined(linux) || (defined(_WIN32) && !defined(_MINGW))
#  include <omp.h>
#else
#  define NOTHREADS 1
#endif

/* Enumeration for option types */
enum option_type {
  CLO_COLUMNS,
  CLO_PIPE,
  CLO_OUTPUT,
  CLO_FRACTION,
  CLO_SPREAD,
  CLO_KDE,
  CLO_VERBOSE,
  CLO_THREADS,
  CLO_WEIGHT,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "columns",
  "pipe",
  "output",
  "fraction",
  "spread",
  "kde",
  "verbose",
  "threads",
  "weight",
};

#define NORM_NONE 0
#define NORM_RANGE 1
#define NORM_RMS 2
#define NORM_OPTIONS 3
char *normalizationOption[NORM_OPTIONS] = {
  "none", "range", "rms"};

/* Improved usage message for better readability */
static char *USAGE =
  "sddslocaldensity [<inputfile>] [<outputfile>]\n"
  "                 [-pipe=[input][,output]]\n"
  "                  -columns=<normalizationMode>,<name>[,...]\n"
  "                 {\n"
  "                  -fraction=<value> |\n"
  "                  -spread=<value> |\n"
  "                  -kde=bins=<number>[,gridoutput=<filename>][,nsigma=<value>][,explimit=<value>]\n"
  "                    [,sample=<fraction>|use=<number>][,spanPages]\n"
  "                 }\n"
  "                 [-output=<columnName>]\n"
  "                 [-weight=<columnName>]\n"
  "                 [-threads=<number>]\n"
  "                 [-verbose]\n"
  "Options:\n"
  "  -pipe              The standard SDDS Toolkit pipe option.\n"
  "  -threads           The number of threads to use.\n"
  "  -columns           Specifies the names of the columns to include. The names may include wildcards.\n"
  "                     The normalization mode is one of \"none\", \"range\", or \"rms\".\n"
  "                     Note that the normalization mode is irrelevant when fraction or spread options are used.\n"
  "  -weight            Name of the column with which to weight the contributions of each point.\n"
  "  -fraction          Fraction of the range to use to identify \"nearby\" points.\n"
  "  -spread            Standard deviation of the weighting function as a fraction of the range.\n"
  "  -kde               If specified, use n-dimensional Kernel Density Estimation instead of a point-based algorithm.\n"
  "                     Highly recommended when the number of data points is large to avoid N² growth in runtime.\n"
  "                     nsigma gives the number of standard deviations of the bandwidth over which to sum the\n"
  "                     Gaussian factor; smaller numbers can considerably improve performance at the expense of\n"
  "                     lower accuracy in the tails. Using the sample qualifier allows using a randomly-sampled\n"
  "                     fraction of the data to create the density map. The use qualifier allows computing the\n"
  "                     sample fraction so that approximately the indicated number of samples is used.\n"
  "                     If spanPages is given, the KDE density map is created using data from all pages of the file;\n"
  "                     however, the output retains the original page breakdown; useful when processing very large\n"
  "                     quantities of data.\n"
  "  -output            Name of the output column. Defaults to \"LocalDensity\".\n"
  "  -verbose           Print progress information while running.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ "\", SVN revision: " SVN_VERSION ")\n";

typedef struct
{
  short normalizationMode;
  long suppliedNames;
  char **suppliedName; /* may include wildcards */
} COLUMN_LIST;

long resolveColumnNames(SDDS_DATASET *SDDSin, COLUMN_LIST *columnList, int32_t nColumnLists,
                        char ***columnName, short **normalizationMode, int32_t *nColumns);
void normalizeData(double *data, int64_t rows, short mode, double min, double max, int threads);

typedef struct bin_layer {
  long bins;   /* i = (0,...,bins-1) */
  double *sum; /* defined only at the lowest layer */
  struct bin_layer *nextLowerLayer;
} BIN_LAYER;

double SilvermansBandwidth(double data[], long dimensions, int64_t M);
void createBinTree(long bins, long inputColumns, BIN_LAYER *layer);
void fillIndexArray(double *point, long bins, long inputColumns, long *indexList, double *min, double *delta);
void addToBinValue(int myid, long bins, long inputColumns, BIN_LAYER *layer, long *indexList, double quantity);
void zeroBinValues(long bins, long inputColumns, BIN_LAYER *layer);
double retrieveBinValue(long bins, long inputColumns, BIN_LAYER *layer, long *indexList);
double interpolateBinValue(long bins, long inputColumns, BIN_LAYER *layer, double *data, double *min, double *delta, long *indexList);
void interpolateBinValue0(int myid);
void addDensityToBinTree(int myid, BIN_LAYER *layer, long bins, long inputColumns,
                         double *min, double *delta, double *bw, uint64_t row, uint64_t rows, double **data,
                         double weight, long nSigmas, double expLimit);
void addDensityToBinTree1(int myid, BIN_LAYER *layer, long bins, long inputColumns,
                          double *min, double *delta, double *bw, double *data, double weight, uint64_t rows, long nSigmas,
                          double expLimit);
int advanceCounter(long *index, long *index1, long *index2, long n);
void dumpBinValues(SDDS_DATASET *SDDSkde, SDDS_DATASET *SDDSin, long bins, char **inputColumn, long inputColumns,
                   char *densityColumn, BIN_LAYER *layer, double *min, double *delta);
void rescaleDensity(long bins, long inputColumns, BIN_LAYER *layer, double factor);
void addDensityToBinTree0(int myid);

#define KDE_BINS_SEEN 0x001UL
#define KDE_GRIDOUTPUT_SEEN 0x002UL
#define KDE_NSIGMAS_SEEN 0x004UL
#define KDE_EXPLIMIT_SEEN 0x008UL
#define KDE_SAMPLE_SEEN 0x010UL
#define KDE_USE_SEEN 0x020UL
#define KDE_SPAN_PAGES 0x040UL

int threads = 1;
BIN_LAYER binTree;
long bins, nKdeSigmas;
double *min, *max, *delta;
int64_t iRow, rows, jRow, rowsSampled;
int64_t *rowsSampledThread = NULL;
double **data;
double kdeExpLimit;
int verbose;
double kdeSampleFraction;
int32_t inputColumns;
double *bw = NULL;
double *density = NULL;
double *weightValue = NULL;

int main(int argc, char **argv) {
  int iArg;
  COLUMN_LIST *columnList;
  int32_t nColumnLists;
  char **inputColumn, *weightColumn;
  short *normalizationMode;
  double fraction, spreadFraction;
  char *input, *output;
  char *outputColumn;
  long i, readCode;
  unsigned long pipeFlags;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout, SDDSkde;
  char *kdeGridOutput;
  int32_t kdeNumberToUse;
  long pass;
  unsigned long kdeFlags;
  double startTime;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3 || argc > (3 + N_OPTIONS))
    bomb(NULL, USAGE);

  output = input = NULL;
  outputColumn = weightColumn = NULL;
  columnList = NULL;
  nColumnLists = 0;
  pipeFlags = 0;
  fraction = spreadFraction = 0;
  bins = 0;
  kdeGridOutput = NULL;
  verbose = 0;
  nKdeSigmas = 5;
  kdeExpLimit = 1e-16;
  kdeSampleFraction = 1;
  kdeNumberToUse = -1;
  kdeFlags = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_COLUMNS:
        if (scanned[iArg].n_items < 3)
          SDDS_Bomb("invalid -columns syntax");
        columnList = SDDS_Realloc(columnList, sizeof(*columnList) * (nColumnLists + 1));
        if ((columnList[nColumnLists].normalizationMode = match_string(scanned[iArg].list[1], normalizationOption,
                                                                       NORM_OPTIONS, 0)) == -1)
          SDDS_Bomb("invalid normalization mode given");
        columnList[nColumnLists].suppliedNames = scanned[iArg].n_items - 2;
        columnList[nColumnLists].suppliedName = tmalloc(sizeof(*(columnList[nColumnLists].suppliedName)) * columnList[nColumnLists].suppliedNames);
        for (i = 0; i < columnList[nColumnLists].suppliedNames; i++) {
          columnList[nColumnLists].suppliedName[i] = scanned[iArg].list[i + 2];
          scanned[iArg].list[i + 2] = NULL;
        }
        nColumnLists++;
        break;
      case CLO_OUTPUT:
        if (scanned[iArg].n_items != 2)
          SDDS_Bomb("invalid -outputColumn syntax: give a name");
        outputColumn = scanned[iArg].list[1];
        scanned[iArg].list[1] = NULL;
        break;
      case CLO_FRACTION:
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%lf", &fraction) != 1 || fraction <= 0)
          SDDS_Bomb("invalid -fraction syntax: give a value greater than 0");
        break;
      case CLO_SPREAD:
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%lf", &spreadFraction) != 1 || spreadFraction <= 0)
          SDDS_Bomb("invalid -spread syntax: give a value greater than 0");
        break;
      case CLO_THREADS:
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%d", &threads) != 1 || threads <= 0)
          SDDS_Bomb("invalid -threads syntax: give a value greater than 0");
        break;
      case CLO_KDE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -kde syntax: give number of bins");
        scanned[iArg].n_items -= 1;
        kdeFlags = 0;
        if (!scanItemList(&kdeFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "bins", SDDS_LONG, &bins, 1, KDE_BINS_SEEN,
                          "gridoutput", SDDS_STRING, &kdeGridOutput, 1, KDE_GRIDOUTPUT_SEEN,
                          "nsigmas", SDDS_LONG, &nKdeSigmas, 1, KDE_NSIGMAS_SEEN,
                          "explimit", SDDS_DOUBLE, &kdeExpLimit, 1, KDE_EXPLIMIT_SEEN,
                          "sample", SDDS_DOUBLE, &kdeSampleFraction, 1, KDE_SAMPLE_SEEN,
                          "use", SDDS_LONG, &kdeNumberToUse, 1, KDE_USE_SEEN,
                          "spanpages", -1, NULL, 0, KDE_SPAN_PAGES,
                          NULL))
          SDDS_Bomb("invalid -kde syntax");
        if (bins < 3)
          SDDS_Bomb("Number of bins should be at least 3 for KDE");
        if (nKdeSigmas < 1)
          SDDS_Bomb("Number of sigmas should be at least 1 for KDE");
        if (kdeExpLimit <= 0 || kdeExpLimit > 1)
          SDDS_Bomb("Exponential limit for KDE must be (0, 1].");
        if (kdeSampleFraction <= 0 || kdeSampleFraction > 1)
          SDDS_Bomb("Sample fraction for KDE must be (0, 1].");
        if (kdeFlags & KDE_USE_SEEN) {
          if (kdeNumberToUse <= 1)
            SDDS_Bomb("Number to use for KDE must be at least 1.");
          if (kdeFlags & KDE_SAMPLE_SEEN)
            SDDS_Bomb("Give sample fraction or number to use for KDE, not both.");
        }
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_WEIGHT:
        if (scanned[iArg].n_items != 2)
          SDDS_Bomb("invalid -weight syntax: give the name of a column");
        weightColumn = scanned[iArg].list[1];
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

  startTime = delapsed_time();
  if (((fraction > 0 ? 1 : 0) + (spreadFraction > 0 ? 1 : 0) + (bins > 0)) > 1)
    SDDS_Bomb("give only one of -fraction, -spread, or -kde");

  processFilenames("sddslocaldensity", &input, &output, pipeFlags, 0, NULL);

  if (kdeFlags & KDE_SPAN_PAGES && pipeFlags)
    SDDS_Bomb("-kde=spanPages is incompatible with -pipe option");
  if (!kdeFlags && weightColumn)
    SDDS_Bomb("-weight is only supported for -kde at present");

  if (!nColumnLists)
    SDDS_Bomb("supply the names of columns to include with the -columns option");

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!resolveColumnNames(&SDDSin, columnList, nColumnLists, &inputColumn, &normalizationMode, &inputColumns))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (weightColumn && !(SDDS_CheckColumn(&SDDSin, weightColumn, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_InitializeCopy(&SDDSout, &SDDSin, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!outputColumn)
    cp_str(&outputColumn, "LocalDensity");

  if (!SDDS_DefineSimpleColumn(&SDDSout, outputColumn, NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(&SDDSout, "sddslocaldensityElapsedTime", "s", SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(&SDDSout, "sddslocaldensityThreads", NULL, SDDS_SHORT))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (kdeGridOutput) {
    if (!SDDS_InitializeOutput(&SDDSkde, SDDS_BINARY, 1, NULL, NULL, kdeGridOutput))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < inputColumns; i++)
      if (!SDDS_TransferColumnDefinition(&SDDSkde, &SDDSin, inputColumn[i], NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_TransferAllParameterDefinitions(&SDDSkde, &SDDSin, SDDS_TRANSFER_OVERWRITE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_DefineSimpleColumn(&SDDSkde, outputColumn, NULL, SDDS_DOUBLE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_WriteLayout(&SDDSkde))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  data = tmalloc(sizeof(*data) * inputColumns);

  if (bins) {
    binTree.sum = NULL;
    binTree.bins = 0;
    binTree.nextLowerLayer = NULL;
    createBinTree(bins, inputColumns, &binTree);
  }

  min = calloc(inputColumns, sizeof(*min));
  max = calloc(inputColumns, sizeof(*max));
  delta = malloc(sizeof(*delta) * inputColumns);
#if !defined(NOTHREADS)
  omp_set_num_threads(threads);
#endif
  bw = malloc(sizeof(*bw) * inputColumns);

  pass = 1;
  while ((kdeFlags & KDE_SPAN_PAGES && pass < 4) || (!(kdeFlags & KDE_SPAN_PAGES) && pass == 1)) {
    while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
      if ((kdeFlags & KDE_SPAN_PAGES && pass == 3) || (!(kdeFlags & KDE_SPAN_PAGES) && pass == 1)) {
        if (!SDDS_CopyPage(&SDDSout, &SDDSin))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if ((rows = SDDS_CountRowsOfInterest(&SDDSin))) {
        if (verbose)
          fprintf(stderr, "Processing page %ld (pass %ld) with %" PRId64 " rows\n", readCode, pass, rows);
        if (weightColumn &&
            !(weightValue = SDDS_GetColumnInDoubles(&SDDSin, weightColumn)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (i = 0; i < inputColumns; i++) {
          if (!(data[i] = SDDS_GetColumnInDoubles(&SDDSin, inputColumn[i])))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          if (pass == 1) {
            update_min_max(&min[i], &max[i], data[i], rows, kdeFlags & KDE_SPAN_PAGES ? readCode == 1 : 1);
            if (bins)
              delta[i] = (max[i] - min[i]) / (bins - 1);
            else
              delta[i] = 0;
            if (verbose)
              fprintf(stderr, "%s:[%le, %le] delta=%le\n", inputColumn[i], min[i], max[i], delta[i]);
          }
          if (pass != 1)
            normalizeData(data[i], rows, normalizationMode[i], min[i], max[i], threads);
        }
        if (bins) {
          if ((!(kdeFlags & KDE_SPAN_PAGES) && pass == 1) ||
              (kdeFlags & KDE_SPAN_PAGES && pass == 2 && readCode == 1)) {
            for (i = 0; i < inputColumns; i++) {
              bw[i] = SilvermansBandwidth(data[i], inputColumns, rows);
              if (verbose)
                fprintf(stderr, "Bandwidth for %s is %le\n", inputColumn[i], bw[i]);
            }
          }

          if ((!(kdeFlags & KDE_SPAN_PAGES) && pass == 1) ||
              (kdeFlags & KDE_SPAN_PAGES && pass == 2)) {
            if (verbose)
              fprintf(stderr, "Summing density over grid.\n");
            rowsSampled = 0;
            if (kdeFlags & KDE_USE_SEEN) {
              if ((kdeSampleFraction = (1.0 * kdeNumberToUse) / rows) > 1)
                kdeSampleFraction = 1;
            }
            rowsSampledThread = calloc(threads, sizeof(*rowsSampledThread));
            rowsSampled = 0;
#pragma omp parallel
            {
#if defined(NOTHREADS)
              addDensityToBinTree0(0);
#else
              addDensityToBinTree0(omp_get_thread_num());
#endif
            }
            for (i = 0; i < threads; i++) {
              rowsSampled += rowsSampledThread[i];
            }
            free(rowsSampledThread);

            if (rowsSampled != rows) {
              if (verbose)
                fprintf(stderr, "%" PRId64 " of %" PRId64 " rows sampled\n", rowsSampled, rows);
              rescaleDensity(bins, inputColumns, &binTree, (1.0 * rows / rowsSampled));
            }
            if (kdeGridOutput) {
              if (verbose)
                fprintf(stderr, "Dumping KDE grid\n");
              dumpBinValues(&SDDSkde, &SDDSin, bins, inputColumn, inputColumns, outputColumn, &binTree, min, delta);
            }
          }

          if ((kdeFlags & KDE_SPAN_PAGES && pass == 3) || (!(kdeFlags & KDE_SPAN_PAGES) && pass == 1)) {

            density = malloc(sizeof(*density) * rows);
            if (verbose)
              fprintf(stderr, "Interpolating density\n");
#pragma omp parallel
            {
#if defined(NOTHREADS)
              interpolateBinValue0(0);
#else
              interpolateBinValue0(omp_get_thread_num());
#endif
            }

            if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, density, rows, outputColumn))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            free(density);
            if (!(kdeFlags & KDE_SPAN_PAGES)) {
              if (verbose)
                fprintf(stderr, "Setting density map to zero\n");
              zeroBinValues(bins, inputColumns, &binTree);
            }
          }
          for (i = 0; i < inputColumns; i++)
            free(data[i]);
        } /* KDE mode */
        else {
          if (fraction > 0) {
            double *count, *epsilon, distance;
            count = tmalloc(rows * sizeof(*count));
            epsilon = tmalloc(inputColumns * sizeof(*epsilon));
            for (i = 0; i < inputColumns; i++)
              epsilon[i] = fraction * (max[i] - min[i]);
            for (iRow = 0; iRow < rows; iRow++) {
              int64_t nInside = 0;
              for (jRow = 0; jRow < rows; jRow++) {
                short inside = 1;
                if (iRow != jRow) {
                  for (i = 0; i < inputColumns; i++) {
                    distance = fabs(data[i][iRow] - data[i][jRow]);
                    if (distance > epsilon[i]) {
                      inside = 0;
                      break;
                    }
                  }
                }
                if (inside)
                  nInside++;
              }
              count[iRow] = nInside;
            }
            if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, count, rows, outputColumn))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            free(count);
            free(epsilon);
          } else if (spreadFraction > 0) {
            double *count, *spread;
            count = tmalloc(rows * sizeof(*count));
            spread = tmalloc(sizeof(*spread) * inputColumns);
            for (i = 0; i < inputColumns; i++) {
              spread[i] = spreadFraction * (max[i] - min[i]);
            }
            for (iRow = 0; iRow < rows; iRow++) {
              count[iRow] = 0;
              for (jRow = 0; jRow < rows; jRow++) {
                double term = 1;
                for (i = 0; i < inputColumns; i++)
                  term *= exp(-sqr((data[i][iRow] - data[i][jRow]) / spread[i]) / 2);
                count[iRow] += term;
              }
            }
            if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, count, rows, outputColumn))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            free(count);
            free(spread);
          } else {
            double *distance;
            distance = tmalloc(rows * sizeof(*distance));
            for (iRow = 0; iRow < rows; iRow++) {
              double sum;
              distance[iRow] = 0;
              for (jRow = 0; jRow < rows; jRow++) {
                sum = 0;
                for (i = 0; i < inputColumns; i++)
                  sum += sqr(data[i][iRow] - data[i][jRow]);
                distance[iRow] += sqrt(sum);
              }
            }
            for (iRow = 0; iRow < rows; iRow++)
              distance[iRow] = rows / distance[iRow];
            if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, distance, rows, outputColumn))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            free(distance);
          }
        }
        if ((kdeFlags & KDE_SPAN_PAGES && pass == 3) || (!(kdeFlags & KDE_SPAN_PAGES) && pass == 1)) {
          if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                  "sddslocaldensityElapsedTime", delapsed_time() - startTime,
                                  "sddslocaldensityThreads", (short)threads,
                                  NULL) ||
              !SDDS_WritePage(&SDDSout))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    }
    if (kdeFlags & KDE_SPAN_PAGES && pass != 3) {
      if (verbose)
        fprintf(stderr, "Closing input file %s and reopening for second pass\n", input);
      if (!SDDS_Terminate(&SDDSin) || !SDDS_InitializeInput(&SDDSin, input)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
    pass++;
  }
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (kdeGridOutput && !SDDS_Terminate(&SDDSkde)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (verbose)
    printf("Execution completed in %le s\n", delapsed_time() - startTime);
  return EXIT_SUCCESS;
}

long resolveColumnNames(SDDS_DATASET *SDDSin, COLUMN_LIST *columnList, int32_t nColumnLists,
                        char ***columnName, short **normalizationMode, int32_t *nColumns) {
  int32_t i, j, k;
  char **newColumnName;
  int32_t newColumns;
  short duplicate;

  *columnName = NULL;
  *normalizationMode = NULL;
  *nColumns = 0;

  for (i = 0; i < nColumnLists; i++) {
    SDDS_SetColumnFlags(SDDSin, 0);
    for (j = 0; j < columnList[i].suppliedNames; j++) {
      if (!SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, columnList[i].suppliedName[j], SDDS_OR))
        return 0;
    }
    if (!(newColumnName = SDDS_GetColumnNames(SDDSin, &newColumns)) || newColumns == 0) {
      SDDS_SetError("no columns found for one or more -column options");
      return 0;
    }
    for (j = 0; j < newColumns; j++) {
      duplicate = 0;
      for (k = 0; k < *nColumns; k++) {
        if (strcmp(newColumnName[j], (*columnName)[k]) == 0) {
          /* overwrite the previous normalization mode */
          (*normalizationMode)[k] = columnList[i].normalizationMode;
          duplicate = 1;
        }
      }
      if (!duplicate) {
        *columnName = SDDS_Realloc(*columnName, sizeof(**columnName) * (*nColumns + 1));
        (*columnName)[*nColumns] = newColumnName[j];
        *normalizationMode = SDDS_Realloc(*normalizationMode, sizeof(**normalizationMode) * (*nColumns + 1));
        (*normalizationMode)[*nColumns] = columnList[i].normalizationMode;
        *nColumns += 1;
      }
    }
  }
  /*
    printf("Computing density using %d columns\n", *nColumns);
    for (i=0; i<*nColumns; i++) {
    printf("%s --- %s normalization\n",
    (*columnName)[i], normalizationOption[(*normalizationMode)[i]]);
    fflush(stdout);
    }
  */
  return 1;
}

void normalizeData(double *data, int64_t rows, short mode, double min_val, double max_val, int threads) {
  int64_t i;
  double offset, rms, divisor;

  switch (mode) {
  case NORM_RANGE:
    if (min_val == max_val)
      SDDS_Bomb("attempt to normalize data with zero range");
    divisor = max_val - min_val;
    offset = max_val - min_val;
    break;
  case NORM_RMS:
    computeMomentsThreaded(&offset, NULL, &rms, NULL, data, rows, threads);
    if (rms == 0)
      SDDS_Bomb("attempt to normalize data with zero rms");
    divisor = rms;
    break;
  default:
    divisor = 1;
    offset = 0;
    break;
  }
  for (i = 0; i < rows; i++)
    data[i] = (data[i] - offset) / divisor;
}

double SilvermansBandwidth(double data[], long dimensions, int64_t M) {
  /* See https://en.wikipedia.org/wiki/Multivariate_kernel_density_estimation#Optimal_bandwidth_matrix_selection */
    return pow(4.0 / (dimensions + 2.0) / M, 2.0 / (dimensions + 4.0)) * ipow2(gsl_stats_sd(data, 1, M));
}

void createBinTree(long bins, long inputColumns, BIN_LAYER *layer) {
  if (inputColumns == 1) {
    layer->sum = calloc(bins, sizeof(*(layer->sum)));
  } else {
    long i;
    layer->nextLowerLayer = calloc(bins, sizeof(BIN_LAYER));
    for (i = 0; i < bins; i++)
      createBinTree(bins, inputColumns - 1, layer->nextLowerLayer + i);
  }
}

void fillIndexArray(double *point, long bins, long inputColumns, long *indexList, double *min_val, double *delta_val) {
  long i, j, bins_m1;

  bins_m1 = bins - 1;
  for (i = 0; i < inputColumns; i++) {
    j = (point[i] - min_val[i]) / delta_val[i];
    if (j < 0)
      j = 0;
    else if (j > bins_m1)
      j = bins_m1;
    indexList[i] = j;
  }
}

void addToBinValue(int myid, long bins, long inputColumns, BIN_LAYER *layer, long *indexList, double quantity) {
  while (inputColumns--) {
    if (inputColumns == 0) {
#pragma omp atomic
      layer->sum[indexList[0]] += quantity;
    } else {
      layer = layer->nextLowerLayer + indexList[0];
      indexList++;
    }
  }
}

void rescaleDensity(long bins, long inputColumns, BIN_LAYER *layer, double factor) {
  long i;
  if (inputColumns == 1) {
    for (i = 0; i < bins; i++)
      layer->sum[i] *= factor;
  } else {
    for (i = 0; i < bins; i++)
      rescaleDensity(bins, inputColumns - 1, layer->nextLowerLayer + i, factor);
  }
}

void zeroBinValues(long bins, long inputColumns, BIN_LAYER *layer) {
  long i;
  if (inputColumns == 1) {
    for (i = 0; i < bins; i++)
      layer->sum[i] = 0;
  } else {
    for (i = 0; i < bins; i++)
      zeroBinValues(bins, inputColumns - 1, layer->nextLowerLayer + i);
  }
}

void dumpBinValues(SDDS_DATASET *SDDSkde, SDDS_DATASET *SDDSin,
                   long bins, char **inputColumn, long inputColumns,
                   char *densityColumn, BIN_LAYER *layer, double *min_val, double *delta_val) {
  long i;
  long *index, *index1, *index2;
  int64_t row;
  long *columnIndex, densityIndex;

  if (!SDDS_StartPage(SDDSkde, (int64_t)ipow(bins, inputColumns)) ||
      !SDDS_CopyParameters(SDDSkde, SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  columnIndex = calloc(inputColumns, sizeof(*columnIndex));
  for (i = 0; i < inputColumns; i++)
    columnIndex[i] = SDDS_GetColumnIndex(SDDSkde, inputColumn[i]);
  densityIndex = SDDS_GetColumnIndex(SDDSkde, densityColumn);

  index1 = calloc(inputColumns, sizeof(*index1));
  index2 = calloc(inputColumns, sizeof(*index2));
  index = calloc(inputColumns, sizeof(*index));
  for (i = 0; i < inputColumns; i++)
    index2[i] = bins - 1;
  row = 0;
  do {
    for (i = 0; i < inputColumns; i++)
      if (!SDDS_SetRowValues(SDDSkde, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                             row, columnIndex[i], index[i] * delta_val[i] + min_val[i], -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_SetRowValues(SDDSkde, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                           row, densityIndex, retrieveBinValue(bins, inputColumns, layer, index), -1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    row++;
  } while (advanceCounter(index, index1, index2, inputColumns));

  if (!SDDS_WritePage(SDDSkde))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  free(columnIndex);
  free(index1);
  free(index2);
  free(index);
}

double retrieveBinValue(long bins, long inputColumns, BIN_LAYER *layer, long *indexList) {
  while (--inputColumns) {
    layer = layer->nextLowerLayer + indexList[0];
    indexList++;
  }
  return layer->sum[indexList[0]];
}

void interpolateBinValue0(int myid) {
  uint64_t iRow, iRow0, iRow1;
  long *indexList, j;
  double *point;
  indexList = malloc(sizeof(*indexList) * inputColumns);
  point = malloc(sizeof(*point) * inputColumns);
  iRow0 = myid * (rows / threads);
  if (myid == (threads - 1))
    iRow1 = rows - 1;
  else
    iRow1 = (myid + 1) * (rows / threads) - 1;
  if (verbose) {
    printf("Thread %d handling interpolation for rows %ld to %ld\n", myid, (long)iRow0, (long)iRow1);
    fflush(stdout);
  }
  for (iRow = iRow0; iRow <= iRow1; iRow++) {
    for (j = 0; j < inputColumns; j++)
      point[j] = data[j][iRow];
    fillIndexArray(point, bins, inputColumns, indexList, min, delta);
    density[iRow] = interpolateBinValue(bins, inputColumns, &binTree, point, min, delta, indexList);
  }
  free(point);
  free(indexList);
  if (verbose) {
    printf("Finished interpolation on thread %d\n", myid);
    fflush(stdout);
  }
  return;
}

double interpolateBinValue(long bins, long inputColumns, BIN_LAYER *layer, double *data_point, double *min_val, double *delta_val, long *indexList) {
  double value1, value2, value;
  if (indexList[0] != (bins - 1)) {
    if (inputColumns > 1) {
      value1 = interpolateBinValue(bins, inputColumns - 1, layer->nextLowerLayer + indexList[0], data_point + 1, min_val + 1, delta_val + 1, indexList + 1);
      value2 = interpolateBinValue(bins, inputColumns - 1, layer->nextLowerLayer + indexList[0] + 1, data_point + 1, min_val + 1, delta_val + 1, indexList + 1);
    } else {
      value1 = layer->sum[indexList[0] + 0];
      value2 = layer->sum[indexList[0] + 1];
    }
    value = value1 + (value2 - value1) / delta_val[0] * (data_point[0] - (min_val[0] + delta_val[0] * indexList[0]));
  } else {
    if (inputColumns > 1) {
      value1 = interpolateBinValue(bins, inputColumns - 1, layer->nextLowerLayer + indexList[0] - 1, data_point + 1, min_val + 1, delta_val + 1, indexList + 1);
      value2 = interpolateBinValue(bins, inputColumns - 1, layer->nextLowerLayer + indexList[0], data_point + 1, min_val + 1, delta_val + 1, indexList + 1);
    } else {
      value1 = layer->sum[indexList[0] - 1];
      value2 = layer->sum[indexList[0] + 0];
    }
    value = value1 + (value2 - value1) / delta_val[0] * (data_point[0] - (min_val[0] + delta_val[0] * (indexList[0] - 1)));
  }
  if (isnan(value) || isinf(value) || value < 0)
    value = 0;
  return value;
}

void addDensityToBinTree0(int myid) {
  uint64_t iRow, iRow0, iRow1;
  uint64_t myRowsSampled;
  double time0, time1;

  iRow0 = myid * (rows / threads);
  if (myid == (threads - 1))
    iRow1 = rows - 1;
  else
    iRow1 = (myid + 1) * (rows / threads) - 1;
  if (verbose) {
    printf("Thread %d handling density addition for rows %ld to %ld\n", myid, (long)iRow0, (long)iRow1);
    fflush(stdout);
  }

  myRowsSampled = 0;
  time0 = delapsed_time();
  for (iRow = iRow0; iRow <= iRow1; iRow++) {
    if ((kdeSampleFraction != 1) && (kdeSampleFraction < drand(-1)))
      continue;
    myRowsSampled++;
    if (verbose && (time1 = delapsed_time()) > (time0 + 10)) {
      time0 = time1;
      fprintf(stderr, "Addition of density %f %% complete after %.0lf s\n", (100.0 * (iRow - iRow0) / (iRow1 - iRow0 + 1.0)), time1);
    }
    addDensityToBinTree(myid, &binTree, bins, inputColumns, min, delta, bw, iRow, rows, data,
                        weightValue ? weightValue[iRow] : 1.0, nKdeSigmas, kdeExpLimit);
  }
  if (verbose) {
    printf("Finished adding density on thread %d\n", myid);
    fflush(stdout);
  }
  rowsSampledThread[myid] = myRowsSampled;
  return;
}

void addDensityToBinTree(int myid, BIN_LAYER *layer, long bins, long inputColumns,
                         double *min_val, double *delta_val, double *bw, uint64_t row, uint64_t rows, double **data,
                         double weight, long nSigmas, double expLimit) {
  double *samplePoint;
  long i;

  samplePoint = malloc(sizeof(*samplePoint) * inputColumns);
  for (i = 0; i < inputColumns; i++)
    samplePoint[i] = data[i][row];

  addDensityToBinTree1(myid, layer, bins, inputColumns, min_val, delta_val, bw, samplePoint, weight, rows, nSigmas, expLimit);

  free(samplePoint);
}

int advanceCounter(long *index, long *index1, long *index2, long n) {
  long i, carry, rolledOver;
  carry = 1;
  rolledOver = 0;
  for (i = 0; i < n; i++) {
    index[i] += carry;
    if (index[i] > index2[i]) {
      carry = 1;
      index[i] = index1[i];
      rolledOver += 1;
    } else
      carry = 0;
  }
  return rolledOver != n;
}

void addDensityToBinTree1(int myid, BIN_LAYER *layer, long bins, long inputColumns,
                          double *min_val, double *delta_val, double *bw, double *data_point, double weight,
                          uint64_t rows, long nSigmas, double expLimit) {
  long i;
  /* index limits in each dimension */
  long *index1 = NULL, *index2 = NULL;
  long *index = NULL;
  /* allows multiplying in the inner loop instead of dividing (for each thread) */
  double *invSqrtBw = NULL, *invBw = NULL;
  long deltaIndex;
  double zLimit, f1;

  index = malloc(sizeof(*index) * inputColumns);
  index1 = malloc(sizeof(*index1) * inputColumns);
  index2 = malloc(sizeof(*index2) * inputColumns);
  invSqrtBw = malloc(sizeof(*invSqrtBw) * inputColumns);
  invBw = malloc(sizeof(*invBw) * inputColumns);

  fillIndexArray(data_point, bins, inputColumns, index1, min_val, delta_val);
  memcpy(index2, index1, sizeof(*index2) * inputColumns);
  for (i = 0; i < inputColumns; i++) {
    invBw[i] = 1. / bw[i];
    invSqrtBw[i] = sqrt(invBw[i]);
    deltaIndex = nSigmas * sqrt(bw[i]) / delta_val[i]; /* sum over +/-nSigmas*sigma */
    if ((index1[i] -= deltaIndex) < 0)
      index1[i] = 0;
    if ((index2[i] += deltaIndex) > (bins - 1))
      index2[i] = bins - 1;
  }

  memcpy(index, index1, sizeof(*index) * inputColumns);
  zLimit = -2 * log(expLimit);
  f1 = 1 / (pow(PIx2, inputColumns / 2.0) * rows);
  do {
    double z, p;
    z = 0;
    p = f1;
    for (i = 0; i < inputColumns; i++) {
      z += sqr(data_point[i] - (index[i] * delta_val[i] + min_val[i])) * invBw[i];
      p *= invSqrtBw[i];
    }
    if (z < zLimit)
      addToBinValue(myid, bins, inputColumns, layer, index, exp(-z / 2) * p * weight);
  } while (advanceCounter(index, index1, index2, inputColumns));

  free(index);
  free(index1);
  free(index2);
  free(invSqrtBw);
  free(invBw);
}
