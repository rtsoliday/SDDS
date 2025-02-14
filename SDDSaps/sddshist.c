/**
 * @file sddshist.c
 * @brief Command-line tool for generating histograms from SDDS-formatted data.
 *
 * @details
 * This program is a command-line tool designed to generate histograms from data contained in SDDS (Self Describing Data Set) files.
 * It supports various configurations like normalization, filtering, multi-threading for performance, and the ability to define
 * histograms based on specific regions in the data.
 *
 * @section Usage
 * ```
 * sddshist [<inputfile>] [<outputfile>]
 *          [-pipe=[input][,output]]
 *          -dataColumn=<column-name>
 *          [{
 *            -bins=<number> |
 *            -sizeOfBins=<value> |
 *            -regions=filename=<filename>,position=<columnName>,name=<columnName>
 *          }]
 *          [-lowerLimit=<value>]
 *          [-upperLimit=<value>]
 *          [-expand=<factor>]
 *          [-filter=<column-name>,<lower-limit>,<upper-limit>]
 *          [-weightColumn=<column-name>]
 *          [-sides[=<points>]]
 *          [-normalize[={sum|area|peak}]]
 *          [-cdf[=only]]
 *          [-threads=<number>]
 *          [-statistics]
 *          [-verbose]
 *          [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required | Description                                                                 |
 * |----------|-----------------------------------------------------------------------------|
 * | `-dataColumn=<column-name>` | Specifies the column to be histogrammed.                |
 *
 * | Optional | Description                                                                 |
 * |----------|-----------------------------------------------------------------------------|
 * | `-pipe`  | Use pipe for input and/or output.                                           |
 * | `-bins`  | Set the number of bins for the histogram.                                   |
 * | `-sizeOfBins` | Set the size of each bin.                                              |
 * | `-regions` | Define region-based histogramming with file and column information.       |
 * | `-lowerLimit` | Set the lower limit for the histogram.                                 |
 * | `-upperLimit` | Set the upper limit for the histogram.                                 |
 * | `-expand` | Expand the histogram range by a given factor.                              |
 * | `-filter` | Filter data points based on values in a specified column.                 |
 * | `-weightColumn` | Specify a column to weight the histogram.                            |
 * | `-sides` | Extend the histogram to zero level.                                         |
 * | `-normalize` | Normalize the histogram (`sum`, `area`, or `peak`).                     |
 * | `-cdf` | Include the Cumulative Distribution Function (CDF) in the output.            |
 * | `-threads` | Set the number of threads for processing.                                 |
 * | `-statistics` | Include statistical details in the output.                            |
 * | `-verbose` | Print additional processing details.                                      |
 * | `-majorOrder` | Set data major order (row or column).                                  |
 *
 * @subsection Incompatibilities
 *   - `-bins`, `-sizeOfBins`, and `-regions` are mutually exclusive.
 *   - `-normalize` and `-cdf=only` cannot be used together.
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
#include "scan.h"
#include "SDDS.h"

/* Enumeration for option types */
enum option_type {
  SET_BINS,
  SET_LOWERLIMIT,
  SET_UPPERLIMIT,
  SET_DATACOLUMN,
  SET_FILTER,
  SET_BINSIZE,
  SET_WEIGHTCOLUMN,
  SET_NORMALIZE,
  SET_STATISTICS,
  SET_SIDES,
  SET_VERBOSE,
  SET_PIPE,
  SET_CDF,
  SET_EXPAND,
  SET_MAJOR_ORDER,
  SET_REGION_FILE,
  SET_THREADS,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "bins", "lowerlimit", "upperlimit", "datacolumn",
  "filter", "sizeofbins", "weightcolumn",
  "normalize", "statistics", "sides", "verbose", "pipe", "cdf", "expand",
  "majorOrder", "regions", "threads"
};

char *USAGE = 
  "Usage: sddshist [<inputfile>] [<outputfile>]\n"
  "                [-pipe=[input][,output]]\n"
  "                 -dataColumn=<column-name>\n"
  "                 [{\n"
  "                   -bins=<number> |\n"
  "                   -sizeOfBins=<value> |\n"
  "                   -regions=filename=<filename>,position=<columnName>,name=<columnName>\n"
  "                 }]\n"
  "                 [-lowerLimit=<value>]\n"
  "                 [-upperLimit=<value>]\n"
  "                 [-expand=<factor>]\n"
  "                 [-filter=<column-name>,<lower-limit>,<upper-limit>]\n"
  "                 [-weightColumn=<column-name>]\n"
  "                 [-sides[=<points>]]\n"
  "                 [-normalize[={sum|area|peak}]]\n"
  "                 [-cdf[=only]]\n"
  "                 [-threads=<number>]\n"
  "                 [-statistics]\n"
  "                 [-verbose]\n"
  "                 [-majorOrder=row|column]\n"
  "Options:\n"
  "  -pipe=[input][,output]                        Use pipe for input and/or output.\n"
  "  -dataColumn=<column-name>                     Specify the column to histogram.\n"
  "  -bins=<number>                                Set the number of bins for the histogram.\n"
  "  -sizeOfBins=<value>                           Set the size of each bin.\n"
  "  -regions=filename=<filename>,position=<columnName>,name=<columnName>\n"
  "                                                Define region-based histogramming.\n"
  "  -lowerLimit=<value>                           Set the lower limit of the histogram.\n"
  "  -upperLimit=<value>                           Set the upper limit of the histogram.\n"
  "  -expand=<factor>                              Expand the range of the histogram by the given factor.\n"
  "  -filter=<column-name>,<lower>,<upper>         Filter data points based on column values.\n"
  "  -weightColumn=<column-name>                   Weight the histogram with the specified column.\n"
  "  -sides[=<points>]                             Add sides to the histogram down to zero level.\n"
  "  -normalize[={sum|area|peak}]                  Normalize the histogram.\n"
  "  -cdf[=only]                                   Include the CDF in the output. Use 'only' to exclude the histogram.\n"
  "  -threads=<number>                             Specify the number of threads to use.\n"
  "  -statistics                                   Include statistical information in the output.\n"
  "  -verbose                                      Enable informational printouts during processing.\n"
  "  -majorOrder=row|column                        Set the major order of data.\n\n"
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define NORMALIZE_PEAK 0
#define NORMALIZE_AREA 1
#define NORMALIZE_SUM 2
#define NORMALIZE_NO 3
#define N_NORMALIZE_OPTIONS 4
char *normalize_option[N_NORMALIZE_OPTIONS] = {
  "peak", "area", "sum", "no"
};

static int64_t filter(double *x, double *y, double *filterData, int64_t npts, double lower_filter, double upper_filter);
static long setupOutputFile(SDDS_DATASET *outTable, char *outputfile, SDDS_DATASET *inTable, char *inputfile,
                            char *dataColumn, char *weightColumn, char *filterColumn, double lowerFilter,
                            double upperFilter, SDDS_DATASET *regionTable, char *regionNameColumn, long doStats,
                            int64_t bins, double binSize, long normalizeMode, short columnMajorOrder);

/* Column and parameter indices for output file */
static long iIndep, iFreq, iBins, iBinSize, iLoFilter, iUpFilter, iMean, iRMS, iStDev, iPoints, iCdf;
static short cdfOnly, freOnly;

int64_t readRegionFile(SDDS_DATASET *SDDSin, char *filename, char *positionColumn, char *nameColumn, double **regionPosition, char ***regionName);
void classifyByRegion(double *data, double *weight, int64_t points, double *histogram, double *regionPosition, int64_t bins);

int main(int argc, char **argv) {
  /* Flags to keep track of what is set in command line */
  long binsGiven, lowerLimitGiven, upperLimitGiven;
  SDDS_DATASET inTable, outTable;
  double *data;                            /* Pointer to the array to histogram */
  double *filterData;                      /* Pointer to the filter data */
  double *weightData;                      /* Pointer to the weight data */
  double *hist, *hist1;                    /* To store the histogram */
  double *CDF, *CDF1;                      /* To store the CDF */
  double sum;                              /* Total of histogram */
  double *indep;                           /* Values of bin centers */
  double lowerLimit, upperLimit;           /* Lower and upper limits in histogram */
  double givenLowerLimit, givenUpperLimit; /* Given lower and upper limits */
  double range, binSize;
  int64_t bins; /* Number of bins in the histogram */
  long doStats; /* Include statistics in output file */
  double mean, rms, standDev, mad;
  char *filterColumn, *dataColumn, *weightColumn;
  double lowerFilter = 0, upperFilter = 0; /* Filter range */
  int64_t points;                          /* Number of data points after filtering */
  SCANNED_ARG *scanned;                    /* Scanned argument structure */
  char *inputfile, *outputfile;            /* Input and output filenames */
  double dx;                               /* Spacing of bins in histogram */
  int64_t i;                               /* Loop variable */
  long pointsBinned;                       /* Number of points in histogram */
  long normalizeMode, doSides, verbose, readCode;
  int64_t rows;
  unsigned long pipeFlags, majorOrderFlag, regionFlags = 0;
  char *cdf;
  double expansionFactor = 0;
  short columnMajorOrder = -1;
  char *regionFilename = NULL, *regionPositionColumn = NULL, *regionNameColumn = NULL;
  double *regionPosition = NULL;
  int64_t nRegions = 0;
  char **regionName = NULL;
  SDDS_DATASET SDDSregion;
  int threads = 1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s\n", USAGE);
    exit(EXIT_FAILURE);
  }

  binsGiven = lowerLimitGiven = upperLimitGiven = 0;
  binSize = doSides = 0;
  inputfile = outputfile = NULL;
  dataColumn = filterColumn = weightColumn = NULL;
  doStats = verbose = 0;
  normalizeMode = NORMALIZE_NO;
  pipeFlags = 0;
  dx = 0;
  cdfOnly = 0;
  freOnly = 1;

  for (i = 1; i < argc; i++) {
    if (scanned[i].arg_type == OPTION) {
      switch (match_string(scanned[i].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[i].n_items--;
        if (scanned[i].n_items > 0 && (!scanItemList(&majorOrderFlag, scanned[i].list + 1, &scanned[i].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_BINS: /* Set number of bins */
        if (binsGiven)
          SDDS_Bomb("-bins specified more than once");
        binsGiven = 1;
        if (sscanf(scanned[i].list[1], "%" SCNd64, &bins) != 1 || bins <= 0)
          SDDS_Bomb("invalid value for bins");
        break;
      case SET_LOWERLIMIT:
        if (lowerLimitGiven)
          SDDS_Bomb("-lowerLimit specified more than once");
        lowerLimitGiven = 1;
        if (sscanf(scanned[i].list[1], "%lf", &givenLowerLimit) != 1)
          SDDS_Bomb("invalid value for lowerLimit");
        break;
      case SET_UPPERLIMIT:
        if (upperLimitGiven)
          SDDS_Bomb("-upperLimit specified more than once");
        upperLimitGiven = 1;
        if (sscanf(scanned[i].list[1], "%lf", &givenUpperLimit) != 1)
          SDDS_Bomb("invalid value for upperLimit");
        break;
      case SET_EXPAND:
        expansionFactor = 0;
        if (sscanf(scanned[i].list[1], "%lf", &expansionFactor) != 1 || expansionFactor <= 0)
          SDDS_Bomb("invalid value for expand");
        break;
      case SET_DATACOLUMN:
        if (dataColumn)
          SDDS_Bomb("-dataColumn specified more than once");
        if (scanned[i].n_items != 2)
          SDDS_Bomb("invalid -dataColumn syntax---supply name");
        dataColumn = scanned[i].list[1];
        break;
      case SET_FILTER:
        if (filterColumn)
          SDDS_Bomb("multiple filter specifications not allowed");
        if (scanned[i].n_items != 4 || sscanf(scanned[i].list[2], "%lf", &lowerFilter) != 1 ||
            sscanf(scanned[i].list[3], "%lf", &upperFilter) != 1 || lowerFilter > upperFilter)
          SDDS_Bomb("invalid -filter syntax/values");
        filterColumn = scanned[i].list[1];
        break;
      case SET_WEIGHTCOLUMN:
        if (weightColumn)
          SDDS_Bomb("multiple weighting columns not allowed");
        if (scanned[i].n_items != 2)
          SDDS_Bomb("-weightColumn requires a column name");
        weightColumn = scanned[i].list[1];
        break;
      case SET_NORMALIZE:
        if (scanned[i].n_items == 1)
          normalizeMode = NORMALIZE_SUM;
        else if (scanned[i].n_items != 2 || (normalizeMode = match_string(scanned[i].list[1], normalize_option, N_NORMALIZE_OPTIONS, 0)) < 0)
          SDDS_Bomb("invalid -normalize syntax");
        break;
      case SET_STATISTICS:
        doStats = 1;
        break;
      case SET_SIDES:
        if (scanned[i].n_items == 1)
          doSides = 1;
        else if (scanned[i].n_items > 2 || (sscanf(scanned[i].list[1], "%ld", &doSides) != 1 || doSides <= 0))
          SDDS_Bomb("invalid -sides syntax");
        break;
      case SET_VERBOSE:
        verbose = 1;
        break;
      case SET_BINSIZE:
        if (sscanf(scanned[i].list[1], "%le", &binSize) != 1 || binSize <= 0)
          SDDS_Bomb("invalid value for bin size");
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[i].list + 1, scanned[i].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_CDF:
        if (scanned[i].n_items == 1)
          cdfOnly = 0;
        else {
          if (scanned[i].n_items != 2)
            SDDS_Bomb("invalid -cdf syntax");
          cdf = scanned[i].list[1];
          if (strcmp(cdf, "only") != 0)
            SDDS_Bomb("invalid -cdf value, it should be -cdf or -cdf=only");
          cdfOnly = 1;
        }
        freOnly = 0;
        break;
      case SET_REGION_FILE:
        if (scanned[i].n_items != 4)
          SDDS_Bomb("invalid -regionFile syntax");
        regionFlags = 0;
        scanned[i].n_items -= 1;
        if (!scanItemList(&regionFlags, scanned[i].list + 1, &scanned[i].n_items, 0,
                          "filename", SDDS_STRING, &regionFilename, 1, 1,
                          "position", SDDS_STRING, &regionPositionColumn, 1, 2,
                          "name", SDDS_STRING, &regionNameColumn, 1, 4, NULL) ||
            regionFlags != (1 + 2 + 4) || !regionFilename || !regionPositionColumn || !regionNameColumn)
          SDDS_Bomb("invalid -regionFile syntax");
        break;
      case SET_THREADS:
        if (scanned[i].n_items != 2 ||
            !sscanf(scanned[i].list[1], "%d", &threads) || threads < 1)
          SDDS_Bomb("invalid -threads syntax");
        break;
      default:
        fprintf(stderr, "Error: option %s not recognized\n", scanned[i].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      /* Argument is filename */
      if (!inputfile)
        inputfile = scanned[i].list[0];
      else if (!outputfile)
        outputfile = scanned[i].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  processFilenames("sddshist", &inputfile, &outputfile, pipeFlags, 0, NULL);

  if (binSize && binsGiven && regionFlags)
    SDDS_Bomb("Provide only one of -bins, -sizeOfBins, or -regions");
  if (!binsGiven)
    bins = 20;
  if (!dataColumn)
    SDDS_Bomb("-dataColumn must be specified");

  if (regionFlags) {
    if (!(nRegions = readRegionFile(&SDDSregion, regionFilename, regionPositionColumn, regionNameColumn, &regionPosition, &regionName)))
      SDDS_Bomb("Problem with region file. Check existence and type of columns");
    doSides = 0;
    bins = nRegions + 1;
  }
  
  hist = tmalloc(sizeof(*hist) * (bins + 2 * doSides));
  CDF = CDF1 = tmalloc(sizeof(*hist) * (bins + 2 * doSides));
  indep = tmalloc(sizeof(*indep) * (bins + 2 * doSides));
  pointsBinned = 0;

  if (!SDDS_InitializeInput(&inTable, inputfile) ||
      SDDS_GetColumnIndex(&inTable, dataColumn) < 0 ||
      (weightColumn && SDDS_GetColumnIndex(&inTable, weightColumn) < 0) ||
      (filterColumn && SDDS_GetColumnIndex(&inTable, filterColumn) < 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!setupOutputFile(&outTable, outputfile, &inTable, inputfile, dataColumn, weightColumn, filterColumn, lowerFilter, upperFilter, &SDDSregion, regionNameColumn, doStats, bins, binSize, normalizeMode, columnMajorOrder))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  data = weightData = filterData = NULL;
  while ((readCode = SDDS_ReadPage(&inTable)) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&inTable)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (rows && (!(data = SDDS_GetColumnInDoubles(&inTable, dataColumn)) ||
                 (weightColumn && !(weightData = SDDS_GetColumnInDoubles(&inTable, weightColumn))) ||
                 (filterColumn && !(filterData = SDDS_GetColumnInDoubles(&inTable, filterColumn)))))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (rows && filterColumn)
      points = filter(data, weightData, filterData, rows, lowerFilter, upperFilter);
    else
      points = rows;

    pointsBinned = 0;
    if (points) {
      if (doStats) {
        if (!weightColumn)
          computeMomentsThreaded(&mean, &rms, &standDev, &mad, data, points, threads);
        else
          computeWeightedMomentsThreaded(&mean, &rms, &standDev, &mad, data, weightData, points, threads);
      }

      if (regionFlags) {
        classifyByRegion(data, weightData, points, hist, regionPosition, bins);
        hist1 = hist;
      } else {
        if (!lowerLimitGiven) {
          lowerLimit = (points > 0) ? data[0] : 0;
          for (i = 0; i < points; i++)
            if (lowerLimit > data[i])
              lowerLimit = data[i];
        } else {
          lowerLimit = givenLowerLimit;
        }
        if (!upperLimitGiven) {
          upperLimit = (points > 0) ? data[0] : 0;
          for (i = 0; i < points; i++)
            if (upperLimit < data[i])
              upperLimit = data[i];
        } else {
          upperLimit = givenUpperLimit;
        }

        range = upperLimit - lowerLimit;
        if (!lowerLimitGiven)
          lowerLimit -= range * 1e-7;
        if (!upperLimitGiven)
          upperLimit += range * 1e-7;
        if (upperLimit == lowerLimit) {
          if (binSize) {
            upperLimit += binSize / 2;
            lowerLimit -= binSize / 2;
          } else if (fabs(upperLimit) < sqrt(DBL_MIN)) {
            upperLimit = sqrt(DBL_MIN);
            lowerLimit = -sqrt(DBL_MIN);
          } else {
            upperLimit += upperLimit * (1 + 2 * DBL_EPSILON);
            lowerLimit -= upperLimit * (1 - 2 * DBL_EPSILON);
          }
        }
        if (expansionFactor > 0) {
          double center = (upperLimit + lowerLimit) / 2;
          range = expansionFactor * (upperLimit - lowerLimit);
          lowerLimit = center - range / 2;
          upperLimit = center + range / 2;
        }
        dx = (upperLimit - lowerLimit) / bins;

        if (binSize) {
          double middle;
          range = ((range / binSize) + 1) * binSize;
          middle = (lowerLimit + upperLimit) / 2;
          lowerLimit = middle - range / 2;
          upperLimit = middle + range / 2;
          dx = binSize;
          bins = range / binSize + 0.5;
          if (bins < 1 && !doSides)
            bins = 2 * doSides;
          indep = trealloc(indep, sizeof(*indep) * (bins + 2 * doSides));
          hist = trealloc(hist, sizeof(*hist) * (bins + 2 * doSides));
          CDF = trealloc(CDF, sizeof(*hist) * (bins + 2 * doSides));
        }

        for (i = -doSides; i < bins + doSides; i++)
          indep[i + doSides] = (i + 0.5) * dx + lowerLimit;
        hist1 = hist + doSides;
        CDF1 = CDF + doSides;
        if (doSides) {
          hist[0] = hist[bins + doSides] = 0;
        }

        if (!weightColumn)
          pointsBinned = make_histogram(hist1, bins, lowerLimit, upperLimit, data, points, 1);
        else
          pointsBinned = make_histogram_weighted(hist1, bins, lowerLimit, upperLimit, data, points, 1, weightData);
      }

      sum = 0;
      for (i = 0; i < bins + doSides; i++) {
        sum += hist1[i];
      }
      CDF1[0] = hist1[0] / sum;
      for (i = 1; i < bins + doSides; i++) {
        CDF1[i] = CDF1[i - 1] + hist1[i] / sum;
      }

      if (verbose)
        fprintf(stderr, "%ld points of %" PRId64 " from page %ld histogrammed in %" PRId64 " bins\n", pointsBinned, rows, readCode, bins);
      if (!cdfOnly) {
        if (normalizeMode != NORMALIZE_NO) {
          double norm = 0;
          switch (normalizeMode) {
          case NORMALIZE_PEAK:
            norm = max_in_array(hist1, bins);
            break;
          case NORMALIZE_AREA:
          case NORMALIZE_SUM:
            for (i = 0; i < bins; i++)
              norm += hist1[i];
            if (normalizeMode == NORMALIZE_AREA)
              norm *= dx;
            break;
          default:
            SDDS_Bomb("invalid normalize mode--consult programmer.");
            break;
          }
          if (norm)
            for (i = 0; i < bins; i++)
              hist1[i] /= norm;
        }
      }
    }

    if (regionFlags) {
      if (!SDDS_StartPage(&outTable, bins) ||
          !SDDS_CopyParameters(&outTable, &inTable) ||
          !SDDS_SetParameters(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iBins, bins, iBinSize, dx, iPoints, pointsBinned, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (points) {
        if (!SDDS_SetColumn(&outTable, SDDS_SET_BY_INDEX, regionPosition, bins, iIndep) ||
            !SDDS_SetColumn(&outTable, SDDS_SET_BY_NAME, regionName, bins, regionNameColumn))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!freOnly && !SDDS_SetColumn(&outTable, SDDS_SET_BY_INDEX, CDF, bins, iCdf))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!cdfOnly && !SDDS_SetColumn(&outTable, SDDS_SET_BY_INDEX, hist, bins, iFreq))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    } else {
      if (!SDDS_StartPage(&outTable, bins + 2 * doSides) ||
          !SDDS_CopyParameters(&outTable, &inTable) ||
          (points && (!SDDS_SetColumn(&outTable, SDDS_SET_BY_INDEX, indep, bins + 2 * doSides, iIndep))) ||
          !SDDS_SetParameters(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iBins, bins, iBinSize, dx, iPoints, pointsBinned, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!freOnly) {
        if (points && !SDDS_SetColumn(&outTable, SDDS_SET_BY_INDEX, CDF, bins + 2 * doSides, iCdf))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!cdfOnly) {
        if (points && !SDDS_SetColumn(&outTable, SDDS_SET_BY_INDEX, hist, bins + 2 * doSides, iFreq))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    if (filterColumn && points &&
        !SDDS_SetParameters(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iLoFilter, lowerFilter, iUpFilter, upperFilter, -1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (doStats && points &&
        !SDDS_SetParameters(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iMean, mean, iRMS, rms, iStDev, standDev, -1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!SDDS_WritePage(&outTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (data)
      free(data);
    if (weightData)
      free(weightData);
    if (filterData)
      free(filterData);
    data = weightData = filterData = NULL;
  }

  if (!SDDS_Terminate(&inTable)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!SDDS_Terminate(&outTable)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}

/* routine: filter()
 * purpose: filter set of points {(x, y)} with a specified
 *          window in one of the variables. Works in place.
 */
static int64_t filter(double *x, double *y, double *filterData, int64_t npts, double lower_filter, double upper_filter) {
  int64_t i, j;
  static char *keep = NULL;
  static int64_t maxPoints = 0;

  if (maxPoints < npts)
    keep = trealloc(keep, sizeof(*keep) * (maxPoints = npts));

  for (i = 0; i < npts; i++) {
    if (filterData[i] < lower_filter || filterData[i] > upper_filter)
      keep[i] = 0;
    else
      keep[i] = 1;
  }

  for (i = j = 0; i < npts; i++)
    if (keep[i]) {
      if (i != j) {
        if (x)
          x[j] = x[i];
        if (y)
          y[j] = y[i];
        filterData[j] = filterData[i];
      }
      j++;
    }

  return j;
}

static long setupOutputFile(SDDS_DATASET *outTable, char *outputfile, SDDS_DATASET *inTable, char *inputfile,
                            char *dataColumn, char *weightColumn, char *filterColumn, double lowerFilter,
                            double upperFilter, SDDS_DATASET *regionTable, char *regionNameColumn, long doStats,
                            int64_t bins, double binSize, long normalizeMode, short columnMajorOrder) {
  char *symbol, *units, *dataUnits, *outputFormat;
  int32_t outputType;
  char s[1024];

  if (!SDDS_InitializeOutput(outTable, SDDS_BINARY, 0, NULL, "sddshist output", outputfile))
    return 0;
  if (columnMajorOrder != -1)
    outTable->layout.data_mode.column_major = columnMajorOrder;
  else
    outTable->layout.data_mode.column_major = inTable->layout.data_mode.column_major;
  if (!SDDS_GetColumnInformation(inTable, "units", &dataUnits, SDDS_GET_BY_NAME, dataColumn))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  /* Define output columns */
  outputType = SDDS_DOUBLE;
  outputFormat = NULL;

  if (!SDDS_TransferColumnDefinition(outTable, inTable, dataColumn, NULL) ||
      !SDDS_ChangeColumnInformation(outTable, "type", &outputType, SDDS_BY_NAME, dataColumn) ||
      !SDDS_ChangeColumnInformation(outTable, "format_string", &outputFormat, SDDS_BY_NAME, dataColumn) ||
      (iIndep = SDDS_GetColumnIndex(outTable, dataColumn)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (regionNameColumn && !SDDS_TransferColumnDefinition(outTable, regionTable, regionNameColumn, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!cdfOnly) {
    switch (normalizeMode) {
    case NORMALIZE_PEAK:
      symbol = "RelativeFrequency";
      units = NULL;
      break;
    case NORMALIZE_AREA:
      symbol = "NormalizedFrequency";
      if (dataUnits && !SDDS_StringIsBlank(dataUnits)) {
        units = tmalloc(sizeof(*units) * (strlen(dataUnits) + 5));
        if (strchr(dataUnits, ' '))
          sprintf(units, "1/(%s)", dataUnits);
        else
          sprintf(units, "1/%s", dataUnits);
      } else
        units = NULL;
      break;
    case NORMALIZE_SUM:
      symbol = "FractionalFrequency";
      units = NULL;
      break;
    default:
      if (weightColumn) {
        char *weightUnits = NULL;
        if (weightColumn && !SDDS_GetColumnInformation(inTable, "units", &weightUnits, SDDS_GET_BY_NAME, weightColumn))
          return 0;
        symbol = "WeightedNumberOfOccurrences";
        units = weightUnits;
      } else {
        symbol = "NumberOfOccurrences";
        units = NULL;
      }
      break;
    }

    if ((iFreq = SDDS_DefineColumn(outTable, "frequency", symbol, units, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    free(units);
    units = NULL;
  }
  if (!freOnly) {
    sprintf(s, "%sCdf", dataColumn);
    if ((iCdf = SDDS_DefineColumn(outTable, s, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /* Define output parameters */
  if (SDDS_DefineParameter(outTable, "sddshistInput", NULL, NULL, NULL, NULL, SDDS_STRING,
                           inputfile) < 0 ||
      (weightColumn && SDDS_DefineParameter(outTable, "sddshistWeight", NULL, NULL, NULL, NULL,
                                            SDDS_STRING, weightColumn) < 0) ||
      (iBins = SDDS_DefineParameter(outTable, "sddshistBins", NULL, NULL, NULL, NULL, SDDS_LONG,
                                    NULL)) < 0 ||
      (iBinSize = SDDS_DefineParameter(outTable, "sddshistBinSize", NULL, NULL, NULL, NULL, SDDS_DOUBLE, NULL)) < 0 ||
      (iPoints = SDDS_DefineParameter(outTable, "sddshistBinned", NULL, NULL, NULL, NULL, SDDS_LONG, NULL)) < 0)
    return 0;
  if (filterColumn) {
    char *filterUnits;
    if (!SDDS_GetColumnInformation(inTable, "units", &filterUnits, SDDS_GET_BY_NAME, filterColumn) ||
        SDDS_DefineParameter(outTable, "sddshistFilter", NULL, NULL, NULL, NULL, SDDS_STRING,
                             filterColumn) < 0 ||
        (iLoFilter = SDDS_DefineParameter(outTable, "sddshistLowerFilter", NULL, filterUnits, NULL, NULL, SDDS_DOUBLE, NULL)) < 0 ||
        (iUpFilter = SDDS_DefineParameter(outTable, "sddshistUpperFilter", NULL, filterUnits, NULL, NULL, SDDS_DOUBLE, NULL)) < 0)
      return 0;
    if (filterUnits)
      free(filterUnits);
  }
  if (doStats) {
    char *buffer;
    buffer = tmalloc(sizeof(*buffer) * (strlen(dataColumn) + 20));
    sprintf(buffer, "%sMean", dataColumn);
    if ((iMean = SDDS_DefineParameter(outTable, buffer, NULL, dataUnits, NULL, NULL, SDDS_DOUBLE, NULL)) < 0)
      return 0;
    sprintf(buffer, "%sRms", dataColumn);
    if ((iRMS = SDDS_DefineParameter(outTable, buffer, NULL, dataUnits, NULL, NULL, SDDS_DOUBLE, NULL)) < 0)
      return 0;
    sprintf(buffer, "%sStDev", dataColumn);
    if ((iStDev = SDDS_DefineParameter(outTable, buffer, NULL, dataUnits, NULL, NULL, SDDS_DOUBLE, NULL)) < 0)
      return 0;
    free(buffer);
  }
  if (SDDS_DefineParameter(outTable, "sddshistNormMode", NULL, NULL, NULL, NULL, SDDS_STRING, normalize_option[normalizeMode]) < 0 ||
      !SDDS_TransferAllParameterDefinitions(outTable, inTable, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(outTable))
    return 0;
  return 1;
}

int64_t readRegionFile(SDDS_DATASET *SDDSin, char *filename, char *positionColumn, char *nameColumn, double **regionPosition, char ***regionName) {
  int64_t i, rows = 0;
  if (!SDDS_InitializeInput(SDDSin, filename) ||
      SDDS_ReadPage(SDDSin) != 1 || (rows = SDDS_RowCount(SDDSin)) < 1 ||
      !(*regionPosition = SDDS_GetColumnInDoubles(SDDSin, positionColumn)) ||
      !(*regionName = (char **)SDDS_GetColumn(SDDSin, nameColumn)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 1; i < rows; i++)
    if ((*regionPosition)[i] <= (*regionPosition)[i - 1]) {
      fprintf(stderr, "sddshist: Error in region position data: row %" PRId64 " is %21.15e while row %" PRId64 " is %21.15e\n", 
              i - 1, (*regionPosition)[i - 1], i, (*regionPosition)[i]);
      exit(EXIT_FAILURE);
    }
  *regionPosition = SDDS_Realloc(*regionPosition, sizeof(**regionPosition) * (rows + 1));
  (*regionPosition)[rows] = DBL_MAX;
  *regionName = SDDS_Realloc(*regionName, sizeof(**regionName) * (rows + 1));
  cp_str(&(*regionName)[rows], "Beyond");
  return rows;
}

void classifyByRegion(double *data, double *weight, int64_t points, double *histogram, double *regionPosition, int64_t bins) {
  int64_t iData, iBin;

  for (iBin = 0; iBin < bins; iBin++)
    histogram[iBin] = 0;

  for (iData = 0; iData < points; iData++) {
    /* Note that bins is 1 greater than the number of positions */
    for (iBin = 0; iBin < bins - 1; iBin++) {
      if (data[iData] < regionPosition[iBin])
        break;
    }
    if (weight) {
      histogram[iBin] += weight[iData];
    } else {
      histogram[iBin] += 1;
    }
  }
}
