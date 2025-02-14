/**
 * @file sddssnap2grid.c
 * @brief Snap data columns to a regular grid in SDDS files.
 *
 * @details
 * This program processes SDDS files to adjust specified data columns
 * so that their values align to a regular grid. The program supports
 * options for controlling the bin size, adjustment factor, and initial grid
 * spacing guess. It also optionally generates parameters describing the grid
 * such as minimum, maximum, interval, and dimension.
 *
 * @section Usage
 * ```
 * sddssnap2grid <inputfile> <outputfile>
 *               [-pipe=[input][,output]]
 *                -column=<name>,[{maximumBins=<value>|binFactor=<value>|deltaGuess=<value>}][,adjustFactor=<value>]
 *               [-makeParameters]
 *               [-verbose]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-column`                             | Specify the column name and bin settings (e.g., binFactor, maximumBins, deltaGuess). |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Standard SDDS Toolkit pipe option for input/output.                                  |
 * | `-makeParameters`                     | Store grid information as parameters in the output file.                             |
 * | `-verbose`                            | Display detailed information during processing.                                      |
 *
 * @subsection Incompatibilities
 *   - `-column`:
 *     - Only one of `maximumBins` or `binFactor` or `deltaGuess` may be specified.
 *
 * @subsection Requirements
 *   - For `-column`:
 *     - `adjustFactor` must be in the range (0, 1).
 *     - `deltaGuess` must be positive.
 *     - If `binFactor` is provided, it must be greater than or equal to 1.
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
 * M. Borland, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "SDDSutils.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  CLO_PIPE,
  CLO_COLUMN,
  CLO_VERBOSE,
  CLO_MAKE_PARAMETERS,
  N_OPTIONS
};

typedef struct {
  unsigned long flags;
  char *name;
  int32_t maximumBins, binFactor;
  double deltaGuess, adjustFactor;
} COLUMN_TO_SNAP;

char *option[N_OPTIONS] = {
  "pipe",
  "column",
  "verbose",
  "makeparameters"
};

#define COLUMN_MAXIMUM_BINS 0x01UL
#define COLUMN_BIN_FACTOR 0x02UL
#define COLUMN_DELTA_GUESS 0x04UL
#define COLUMN_ADJUST_FACTOR 0x08UL

static char *USAGE =
    "sddssnap2grid [<inputfile>] [<outputfile>] [-pipe=[input][,output]]\n"
    "    -column=<name>,[{maximumBins=<value>|binFactor=<value>|deltaGuess=<value>}][,adjustFactor=<value>]\n"
    "    [-column=...]\n"
    "    [-makeParameters] [-verbose]\n"
    "\n"
    "Options:\n"
    "  -pipe        Standard SDDS Toolkit pipe option.\n"
    "  -column      Specify the name of a column to modify for equispaced values.\n"
    "               The default mode uses binFactor = 10, meaning the maximum number\n"
    "               of bins is 10 times the number of data points. The algorithm works as follows:\n"
    "                 1. Bin the data with the maximum number of bins.\n"
    "                 2. If no two adjacent bins are populated, use this grouping to compute\n"
    "                    centroids for each subset, providing delta values.\n"
    "                 3. If two adjacent bins are populated, multiply the number of bins by\n"
    "                    adjustFactor (default: 0.9) and repeat the process.\n"
    "               Alternatively, you can provide a guess for the grid spacing;\n"
    "               the algorithm will use 1/10 of this as the initial bin size.\n"
    "  -makeParameters\n"
    "               Store grid parameters in the output file as parameters named\n"
    "               <name>Minimum, <name>Maximum, <name>Interval, and <name>Dimension.\n"
    "  -verbose     Report the computed deltas and the number of grid points.\n"
    "\n"
    "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

void SnapDataToGrid(double *data, int64_t rows, COLUMN_TO_SNAP *column, int verbose, double *min, double *max, int64_t *points);
void AddParameterDefinitions(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, COLUMN_TO_SNAP *column, long nColumns);
void StoreGridParameters(SDDS_DATASET *SDDSout, char *column, double min, double max, int64_t points);

int main(int argc, char **argv) {
  int iArg, iColumn, verbose;
  char *input, *output;
  unsigned long pipeFlags;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  COLUMN_TO_SNAP *column;
  long nColumns;
  double *data;
  int64_t rows;
  short makeParameters;
  double min, max;
  int64_t points;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  nColumns = 0;
  column = NULL;
  input = output = NULL;
  pipeFlags = 0;
  verbose = 0;
  makeParameters = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_COLUMN:
        column = SDDS_Realloc(column, sizeof(*column) * (nColumns + 1));
        column[nColumns].name = scanned[iArg].list[1];
        column[nColumns].maximumBins = -1;
        column[nColumns].binFactor = 10;
        column[nColumns].deltaGuess = -1;
        column[nColumns].flags = COLUMN_BIN_FACTOR;
        column[nColumns].adjustFactor = 0.9;
        scanned[iArg].n_items -= 2;
        if (scanned[iArg].n_items) {
          if (!scanItemList(&(column[nColumns].flags), scanned[iArg].list + 2, &scanned[iArg].n_items, 0,
                            "maximumbins", SDDS_LONG, &(column[nColumns].maximumBins), 1, COLUMN_MAXIMUM_BINS,
                            "binfactor", SDDS_LONG, &(column[nColumns].binFactor), 1, COLUMN_BIN_FACTOR,
                            "deltaguess", SDDS_DOUBLE, &(column[nColumns].deltaGuess), 1, COLUMN_DELTA_GUESS,
                            "adjustfactor", SDDS_DOUBLE, &(column[nColumns].adjustFactor), 1, COLUMN_ADJUST_FACTOR,
                            NULL)) {
            SDDS_Bomb("invalid -column syntax");
          }
          if (column[nColumns].flags & COLUMN_ADJUST_FACTOR) {
            if (column[nColumns].adjustFactor <= 0 || column[nColumns].adjustFactor >= 1)
              SDDS_Bomb("invalid -column syntax. adjustFactor must be (0,1)");
          }
          if (column[nColumns].flags & COLUMN_DELTA_GUESS) {
            if (column[nColumns].flags & ~(COLUMN_DELTA_GUESS | COLUMN_ADJUST_FACTOR))
              SDDS_Bomb("invalid -column syntax. Can't combine deltaGuess with other options.");
            if (column[nColumns].deltaGuess <= 0)
              SDDS_Bomb("invalid -column syntax. deltaGuess<=0.");
          } else {
            if (column[nColumns].flags & COLUMN_BIN_FACTOR && column[nColumns].flags & COLUMN_MAXIMUM_BINS)
              SDDS_Bomb("invalid -column syntax. Can't give minimumBins and maximumBins with binFactor");
            if (!(column[nColumns].flags & COLUMN_BIN_FACTOR) && !(column[nColumns].flags & COLUMN_MAXIMUM_BINS))
              SDDS_Bomb("invalid -column syntax. Give maximumBins or binFactor");
            if (column[nColumns].flags & COLUMN_BIN_FACTOR && column[nColumns].binFactor < 1)
              SDDS_Bomb("invalid -column syntax. binFactor<1");
          }
        }
        nColumns++;
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_MAKE_PARAMETERS:
        makeParameters = 1;
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

  processFilenames("sddssnap2grid", &input, &output, pipeFlags, 0, NULL);

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_InitializeCopy(&SDDSout, &SDDSin, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (makeParameters)
    AddParameterDefinitions(&SDDSout, &SDDSin, column, nColumns);
  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  while (SDDS_ReadPage(&SDDSin) > 0) {
    rows = SDDS_CountRowsOfInterest(&SDDSin);
    if (!SDDS_CopyPage(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (iColumn = 0; iColumn < nColumns; iColumn++) {
      if (!(data = SDDS_GetColumnInDoubles(&SDDSout, column[iColumn].name)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SnapDataToGrid(data, rows, column + iColumn, verbose, &min, &max, &points);
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, data, rows, column[iColumn].name))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (makeParameters)
        StoreGridParameters(&SDDSout, column[iColumn].name, min, max, points);
      free(data);
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

  return EXIT_SUCCESS;
}

void SnapDataToGrid(double *data, int64_t rows, COLUMN_TO_SNAP *column, int verbose,
                    double *minReturn, double *maxReturn, int64_t *pointsReturn) {
  int64_t i, bins, centroids;
  double min, max, hmin, hmax, span, middle;
  char s[16384];
  double *histogram, *whistogram, *centroid, delta;

  find_min_max(&min, &max, data, rows);
  if ((span = max - min) <= 0)
    return;

  /* Add some buffer space at the ends for histogramming */
  span *= 1 + 2.0 / rows;
  middle = (max + min) / 2;
  hmin = middle - span / 2;
  hmax = middle + span / 2;

  bins = 0; /*  prevent compiler warning */
  if (column->flags & COLUMN_DELTA_GUESS)
    bins = (hmax - hmin) / (column->deltaGuess / 10);
  else if (column->flags & COLUMN_MAXIMUM_BINS)
    bins = column->maximumBins;
  else if (column->flags & COLUMN_BIN_FACTOR)
    bins = rows * column->binFactor;
  else
    SDDS_Bomb("logic error. Missing flags for determination of maximum number of bins.");

  if (verbose) {
    printf("Working on %s with %" PRId64 " bins, span=%le, hmin=%le, hmax=%le\n",
           column->name, bins, span, hmin, hmax);
    fflush(stdout);
  }

  while (bins >= 2) {
    histogram = calloc(bins, sizeof(*histogram));
    whistogram = calloc(bins, sizeof(*whistogram));
    if (verbose) {
      printf("Histogramming %s with %" PRId64 " bins\n", column->name, bins);
      fflush(stdout);
    }
    make_histogram(histogram, bins, hmin, hmax, data, rows, 1);
    for (i = 1; i < bins; i++) {
      if (histogram[i] && histogram[i - 1])
        break;
    }
    if (i != bins) {
      /* indicates that two adjacent bins are occupied, so we need more coarse grouping */
      free(histogram);
      bins *= column->adjustFactor;
      continue;
    }
    if (bins <= 1) {
      snprintf(s, 16384, "Unable to snap data for %s to grid\n", column->name);
      SDDS_Bomb(s);
    }

    /* self-weighted histogram gives centroid*number in each bin */
    make_histogram_weighted(whistogram, bins, hmin, hmax, data, rows, 1, data);
    centroid = tmalloc(sizeof(*centroid) * bins);
    centroids = 0;
    for (i = 0; i < bins; i++) {
      if (histogram[i]) {
        /*
          printf("centroid[%ld] = %le (%le/%le)\n", centroids, whistogram[i]/histogram[i],
          whistogram[i], histogram[i]);
        */
        centroid[centroids++] = whistogram[i] / histogram[i];
      }
    }
    free(histogram);
    free(whistogram);
    delta = (centroid[centroids - 1] - centroid[0]) / (centroids - 1);
    /* assign new value using the computed delta and the first centroid as the first value */
    for (i = 0; i < rows; i++)
      data[i] = ((long)((data[i] - centroid[0]) / delta + 0.5)) * delta + centroid[0];
    if (verbose) {
      printf("Completed work for %s: delta = %le, start = %le, locations = %" PRId64 "\n",
             column->name, delta, centroid[0], centroids);
      fflush(stdout);
    }
    *minReturn = centroid[0];
    *pointsReturn = centroids;
    *maxReturn = centroid[0] + (centroids - 1) * delta;
    free(centroid);
    return;
  }
}

#define BUFLEN 16834
void AddParameterDefinitions(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, COLUMN_TO_SNAP *column, long nColumns) {
  long icol;
  char name1[BUFLEN], name2[BUFLEN], name3[BUFLEN], name4[BUFLEN];

  for (icol = 0; icol < nColumns; icol++) {
    snprintf(name1, BUFLEN, "%sMinimum", column[icol].name);
    snprintf(name2, BUFLEN, "%sMaximum", column[icol].name);
    snprintf(name3, BUFLEN, "%sInterval", column[icol].name);
    snprintf(name4, BUFLEN, "%sDimension", column[icol].name);
    if (!SDDS_DefineParameterLikeColumn(SDDSout, SDDSin, column[icol].name, name1) ||
        !SDDS_DefineParameterLikeColumn(SDDSout, SDDSin, column[icol].name, name2) ||
        !SDDS_DefineParameterLikeColumn(SDDSout, SDDSin, column[icol].name, name3) ||
        !SDDS_DefineSimpleParameter(SDDSout, name4, NULL, SDDS_LONG64))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}

void StoreGridParameters(SDDS_DATASET *SDDSout, char *column, double min, double max, int64_t points) {
  char name1[BUFLEN], name2[BUFLEN], name3[BUFLEN], name4[BUFLEN];

  snprintf(name1, BUFLEN, "%sMinimum", column);
  snprintf(name2, BUFLEN, "%sMaximum", column);
  snprintf(name3, BUFLEN, "%sInterval", column);
  snprintf(name4, BUFLEN, "%sDimension", column);
  if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          name1, min,
                          name2, max,
                          name3, points > 1 ? (max - min) / (points - 1) : -1,
                          name4, points,
                          NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}
