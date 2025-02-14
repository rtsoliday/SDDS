/**
 * @file sddsmultihist.c
 * @brief SDDS-format multi-column histogramming program.
 *
 * @details
 * This program generates histograms for specified columns of an SDDS-formatted input file, offering customization
 * options for bin sizing, normalization, boundaries, and more.
 *
 * @section Usage
 * ```
 * sddsmultihist [<inputfile>] [<outputfile>]
 *               [-pipe=[input][,output]]
 *                -columns=<name>[,...]
 *                -abscissa=<name>[,...]
 *               [-exclude=<name>[,...]]
 *               [-bins=<integer>]
 *               [-sizeOfBins=<value>]
 *               [-autobins=target=<number>[,minimum=<integer>][,maximum=<integer>]]
 *               [-boundaryData=<filename>,<column>]
 *               [-sides[=close|against]]
 *               [-expand=<fraction>]
 *               [-lowerLimit=<value>[,...]]
 *               [-upperLimit=<value>[,...]]
 *               [-separate]
 *               [-cdf=[only]]
 *               [-weightColumn=<name>]
 *               [-majorOrder=row|column]
 *               [-normalize={sum|peak|no}]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Names of columns from the input to be histogrammed. Supports wildcards.               |
 * | `-abscissa`                           | Names of abscissas in the output file.                                                | 
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard SDDS Toolkit pipe for input/output.                                      |
 * | `-exclude`                            | Column names to exclude from histogramming.                                           |
 * | `-bins`                               | Number of bins for the histogram.                                                    |
 * | `-sizeOfBins`                         | Size of each bin for the histogram.                                                  |
 * | `-autobins`                           | Auto-determine bin count based on target samples per bin.                             |
 * | `-boundaryData`                       | Specifies irregular bin boundaries from a file.                                       |
 * | `-sides`                              | Adds zero-height bins at the histogram's ends.                                        |
 * | `-expand`                             | Expands histogram range by the given fraction.                                        |
 * | `-lowerLimit`                         | Sets lower limits for histograms.                                                    |
 * | `-upperLimit`                         | Sets upper limits for histograms.                                                    |
 * | `-separate`                           | Creates separate abscissas for each histogram.                                        |
 * | `-cdf`                                | Includes CDF in the output; "only" includes only CDF.                                 |
 * | `-weightColumn`                       | Specifies a column to weight the histogram.                                           |
 * | `-majorOrder`                         | Sets the output data order to row-major or column-major.                              |
 * | `-normalize`                          | Normalizes the histogram based on sum, peak, or none.                                 |
 *
 * @subsection Incompatibilities
 *   -boundaryData is incompatible with:
 *     - -separate
 *     - -abscissa
 *   - Only one of the following may be specified:
 *     - -bins
 *     - -sizeOfBins
 *     - -autobins
 *   - For -separate:
 *     - The number of -lowerLimit or -upperLimit values must match the number of columns.
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
 * M. Borland, L. Emery, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "SDDSutils.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_COLUMNS,
  SET_PIPE,
  SET_EXCLUDE,
  SET_ABSCISSA,
  SET_BINS,
  SET_SIZEOFBINS,
  SET_LOWERLIMIT,
  SET_UPPERLIMIT,
  SET_SIDES,
  SET_SEPARATE,
  SET_EXPAND,
  SET_CDF,
  SET_AUTOBINS,
  SET_MAJOR_ORDER,
  SET_BOUNDARYDATA,
  SET_WEIGHT,
  SET_NORMALIZE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "columns",
  "pipe",
  "exclude",
  "abscissa",
  "bins",
  "sizeofbins",
  "lowerlimit",
  "upperlimit",
  "sides",
  "separate",
  "expand",
  "cdf",
  "autobins",
  "majorOrder",
  "boundaryData",
  "weightColumn",
  "normalize",
};

static char *USAGE =
  "Usage: sddsmultihist [<inputfile>] [<outputfile>]\n"
  "                     [-pipe=[input][,output]]\n"
  "                     -columns=<name>[,...]\n"
  "                     -abscissa=<name>[,...]\n"
  "                     [-exclude=<name>[,...]]\n"
  "                     [-bins=<integer>]\n"
  "                     [-sizeOfBins=<value>]\n"
  "                     [-autobins=target=<number>[,minimum=<integer>][,maximum=<integer>]]\n"
  "                     [-boundaryData=<filename>,<column>]\n"
  "                     [-sides[=close|against]]\n"
  "                     [-expand=<fraction>]\n"
  "                     [-lowerLimit=<value>[,...]]\n"
  "                     [-upperLimit=<value>[,...]]\n"
  "                     [-separate]\n"
  "                     [-cdf=[only]]\n"
  "                     [-weightColumn=<name>]\n"
  "                     [-majorOrder=row|column]\n"
  "                     [-normalize={sum|peak|no}]\n"
  "Options:\n"
  "  -pipe=[input][,output]             The standard SDDS Toolkit pipe option.\n"
  "  -columns=<name>[,...]              Specifies the names of columns from the input to be histogrammed.\n"
  "                                     Names may contain wildcards.\n"
  "  -abscissa=<name>[,...]             Specifies the names of the abscissas in the output file.\n"
  "                                     When using column names as abscissa names,\n"
  "                                     the -abscissa option is not required (use -separate).\n"
  "                                     At least one abscissa name must be supplied if -separate is not used.\n"
  "  -exclude=<name>[,...]              (Optional) Specifies column names to exclude from histogramming.\n"
  "  -bins=<integer>                    Sets the number of bins for the histogram.\n"
  "  -sizeOfBins=<value>                Sets the size of each bin for the histogram.\n"
  "  -autobins=target=<number>[,minimum=<integer>][,maximum=<integer>]\n"
  "                                     Automatically determines the number of bins based on the target number of samples per bin.\n"
  "                                     Optionally specify minimum and maximum number of bins.\n"
  "  -boundaryData=<filename>,<column>   Specifies irregular bin boundaries from a file.\n"
  "                                     Incompatible with -separate and -abscissa.\n"
  "  -sides[=close|against]             Adds zero-height bins at the ends of the histogram.\n"
  "                                     'close' centers the first and last bins.\n"
  "                                     'against' aligns the first and last bins with the data range.\n"
  "  -expand=<fraction>                 Expands the range of the histogram by the given fraction.\n"
  "  -lowerLimit=<value>[,...]          Sets lower limits for the histograms.\n"
  "  -upperLimit=<value>[,...]          Sets upper limits for the histograms.\n"
  "  -separate                          Creates separate abscissas for each histogram in the output file.\n"
  "  -cdf=[only]                        Includes the Cumulative Distribution Function (CDF) in the output.\n"
  "                                     'only' includes only the CDF, excluding the histogram.\n"
  "  -weightColumn=<name>               Specifies a column to weight the histogram.\n"
  "  -majorOrder=row|column             Sets the output file's data order to row-major or column-major.\n"
  "  -normalize={sum|peak|no}            Normalizes the histogram.\n"
  "                                     'sum' normalizes so that the sum of all bins equals 1.\n"
  "                                     'peak' normalizes so that the peak bin equals 1.\n"
  "                                     'no' applies no normalization.\n"
  "\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define DO_SIDES 1
#define DO_CLOSE_SIDES 2
#define DO_AGAINST_SIDES 3

#define NORMALIZE_PEAK 0
#define NORMALIZE_SUM 1
#define NORMALIZE_NO 2
#define N_NORMALIZE_OPTIONS 3
char *normalize_option[N_NORMALIZE_OPTIONS] = {
  "peak", "sum", "no"};

void SetUpOutput(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, long **abscissaIndex, long **cdfIndex,
                 long **histogramIndex, char *output, char **columnName, long columnNames,
                 char **abscissaName, long abscissaNames, char *boundaryColumn, char *boundaryColumnUnits,
                 short columnMajorOrder, short normMode);
double *ReadBoundaryData(char *file, char *column, int64_t *n, char **units);
void MakeBoundaryHistogram(double *histogram, double *cdf, double *boundaryValue, int64_t nBoundaryValues,
                           double *data, double *weight, int64_t nData);
void NormalizeHistogram(double *hist, int64_t bins, short mode);

static short cdfOnly, freOnly;

int main(int argc, char **argv) {
  int iArg;
  char *boundaryFile, *boundaryColumn, *boundaryColumnUnits;
  double *boundaryValue;
  int64_t nBoundaryValues;
  char **abscissaName, **columnName, **excludeName;
  long columnNames, excludeNames, abscissaNames, column, offset;
  long givenLowerLimits, givenUpperLimits;
  char *input, *output;
  long readCode, binsGiven;
  int64_t i, rows, bins, writeBins;
  long lowerLimitGiven, upperLimitGiven, doSides, doSeparate;
  double autoBinsTarget;
  long autoBinsMinimum, autoBinsMaximum;
  char *weightColumn;
  double *weightData;
  short normMode = NORMALIZE_NO;
  unsigned long pipeFlags;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  double binSize, *lowerLimit = NULL, *upperLimit = NULL, *givenLowerLimit, *givenUpperLimit, *dx = NULL, range, middle;
  double **inputData, *abscissa, *histogram, *minValue, *maxValue, transferLimit, *cdf, sum;
  long *abscissaIndex, *histogramIndex, *cdfIndex;
  double expandRange, maxRange;
  char *CDFONLY;
  unsigned long dummyFlags, majorOrderFlag;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3 || argc > (3 + N_OPTIONS)) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  minValue = maxValue = NULL;
  output = input = NULL;
  pipeFlags = 0;
  abscissaName = NULL;
  boundaryFile = boundaryColumn = boundaryColumnUnits = NULL;
  boundaryValue = NULL;
  offset = 0;
  columnName = excludeName = NULL;
  columnNames = excludeNames = abscissaNames = 0;
  givenLowerLimits = givenUpperLimits = 0;
  givenLowerLimit = givenUpperLimit = NULL;
  bins = binsGiven = binSize = doSides = doSeparate = 0;
  lowerLimitGiven = upperLimitGiven = 0;
  expandRange = 0;
  cdfOnly = 0;
  freOnly = 1;
  autoBinsTarget = 0;
  autoBinsMinimum = autoBinsMaximum = 0;
  weightColumn = NULL;
  weightData = NULL;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 && (!scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_COLUMNS:
        if (columnName)
          SDDS_Bomb("only one -columns option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        if (!(columnName = SDDS_Realloc(columnName, sizeof(*columnName) * (columnNames + scanned[iArg].n_items - 1))))
          SDDS_Bomb("memory allocation failure");
        for (i = 1; i < scanned[iArg].n_items; i++)
          columnName[columnNames + i - 1] = scanned[iArg].list[i];
        columnNames += scanned[iArg].n_items - 1;
        break;
      case SET_ABSCISSA:
        if (abscissaName)
          SDDS_Bomb("only one -abscissa option may be given");
        if (scanned[iArg].n_items >= 2) {
          if (!(abscissaName = SDDS_Realloc(abscissaName, sizeof(*abscissaName) * (abscissaNames + scanned[iArg].n_items - 1))))
            SDDS_Bomb("memory allocation failure");
          for (i = 1; i < scanned[iArg].n_items; i++)
            abscissaName[abscissaNames + i - 1] = scanned[iArg].list[i];
          abscissaNames += scanned[iArg].n_items - 1;
        }
        break;
      case SET_BINS:
        if (binsGiven)
          SDDS_Bomb("-bins specified more than once");
        binsGiven = 1;
        if (sscanf(scanned[iArg].list[1], "%" SCNd64, &bins) != 1 || bins <= 0)
          SDDS_Bomb("invalid value for bins---give a positive value");
        break;
      case SET_SIZEOFBINS:
        if (sscanf(scanned[iArg].list[1], "%le", &binSize) != 1 || binSize <= 0)
          SDDS_Bomb("invalid value for bin size---give a positive value");
        break;
      case SET_AUTOBINS:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("incorrect -autoBins syntax");
        scanned[iArg].n_items -= 1;
        autoBinsTarget = autoBinsMinimum = autoBinsMaximum = 0;
        if (!scanItemList(&dummyFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "target", SDDS_DOUBLE, &autoBinsTarget, 1, 0,
                          "minimum", SDDS_LONG, &autoBinsMinimum, 1, 0,
                          "maximum", SDDS_LONG, &autoBinsMaximum, 1, 0, NULL) ||
            autoBinsTarget <= 0 || autoBinsMinimum < 0 || autoBinsMaximum < 0)
          SDDS_Bomb("incorrect -autoBins syntax or values");
        break;
      case SET_EXCLUDE:
        if (excludeName)
          SDDS_Bomb("only one -exclude option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -exclude syntax");
        if (!(excludeName = SDDS_Realloc(excludeName, sizeof(*excludeName) * (excludeNames + scanned[iArg].n_items - 1))))
          SDDS_Bomb("memory allocation failure");
        for (i = 1; i < scanned[iArg].n_items; i++)
          excludeName[excludeNames + i - 1] = scanned[iArg].list[i];
        excludeNames += scanned[iArg].n_items - 1;
        break;
      case SET_LOWERLIMIT:
        if (lowerLimitGiven)
          SDDS_Bomb("-lowerLimit specified more than once");
        lowerLimitGiven = 1;
        if (!(givenLowerLimit = SDDS_Realloc(givenLowerLimit, sizeof(*givenLowerLimit) * (givenLowerLimits + scanned[iArg].n_items - 1))))
          SDDS_Bomb("SET_LOWERLIMIT: memory allocation failure");
        for (i = 1; i < scanned[iArg].n_items; i++) {
          if (sscanf(scanned[iArg].list[i], "%lf", &transferLimit) != 1)
            SDDS_Bomb("invalid value for -lowerLimit");
          givenLowerLimit[givenLowerLimits + i - 1] = transferLimit;
        }
        givenLowerLimits += scanned[iArg].n_items - 1;
        break;
      case SET_UPPERLIMIT:
        if (upperLimitGiven)
          SDDS_Bomb("-upperLimit specified more than once");
        upperLimitGiven = 1;
        if (!(givenUpperLimit = SDDS_Realloc(givenUpperLimit, sizeof(*givenUpperLimit) * (givenUpperLimits + scanned[iArg].n_items - 1))))
          SDDS_Bomb("SET_UPPERLIMIT: memory allocation failure");
        for (i = 1; i < scanned[iArg].n_items; i++) {
          if (sscanf(scanned[iArg].list[i], "%lf", &transferLimit) != 1)
            SDDS_Bomb("invalid value for -upperLimit");
          givenUpperLimit[givenUpperLimits + i - 1] = transferLimit;
        }
        givenUpperLimits += scanned[iArg].n_items - 1;
        break;
      case SET_SIDES:
        doSides = DO_SIDES;
        if (scanned[iArg].n_items == 2) {
          static char *sideOpt[2] = {"close", "against"};
          switch (match_string(scanned[iArg].list[1], sideOpt, 2, 0)) {
          case 0:
            doSides = DO_CLOSE_SIDES;
            break;
          case 1:
            doSides = DO_AGAINST_SIDES;
            break;
          default:
            SDDS_Bomb("invalid value for -sides");
            break;
          }
        }
        break;
      case SET_SEPARATE:
        doSeparate = 1;
        break;
      case SET_EXPAND:
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%lf", &expandRange) != 1 || expandRange <= 0)
          SDDS_Bomb("invalid -expand syntax");
        break;
      case SET_CDF:
        if (scanned[iArg].n_items == 1)
          cdfOnly = 0;
        else {
          if (scanned[iArg].n_items > 2)
            SDDS_Bomb("invalid -cdf syntax");
          CDFONLY = scanned[iArg].list[1];
          if (strcmp(CDFONLY, "only") != 0)
            SDDS_Bomb("invalid -cdf value, it should be -cdf or -cdf=only");
          cdfOnly = 1;
        }
        freOnly = 0;
        break;
      case SET_BOUNDARYDATA:
        if (scanned[iArg].n_items != 3 ||
            !(boundaryFile = scanned[iArg].list[1]) ||
            !strlen(boundaryFile) ||
            !(boundaryColumn = scanned[iArg].list[2]) ||
            !strlen(boundaryColumn))
          SDDS_Bomb("invalid -boundaryData syntax or values");
        break;
      case SET_WEIGHT:
        if (scanned[iArg].n_items != 2 ||
            !(weightColumn = scanned[iArg].list[1]) ||
            !strlen(weightColumn))
          SDDS_Bomb("invalid -weightColumn syntax or values");
        break;
      case SET_NORMALIZE:
        if (scanned[iArg].n_items == 1)
          normMode = NORMALIZE_SUM;
        else if (scanned[iArg].n_items != 2 ||
                 (normMode = match_string(scanned[iArg].list[1], normalize_option, N_NORMALIZE_OPTIONS, 0)) < 0)
          SDDS_Bomb("invalid -normalize syntax");
        break;
      default:
        fprintf(stderr, "Error: unknown or ambiguous option: %s\n", scanned[iArg].list[0]);
        fprintf(stderr, "%s", USAGE);
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

  if (boundaryColumn && (abscissaNames > 0 || doSeparate))
    SDDS_Bomb("-boundaryData option is incompatible with -abscissa and -separate options");

  if (columnNames <= 0)
    SDDS_Bomb("Supply the names of columns to histogram with -columns");

  processFilenames("sddsmultihist", &input, &output, pipeFlags, 0, NULL);

  if (!SDDS_InitializeInput(&SDDSin, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if ((columnNames = expandColumnPairNames(&SDDSin, &columnName, NULL, columnNames, excludeName, excludeNames, FIND_NUMERIC_TYPE, 0)) <= 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_Bomb("No quantities selected to histogram.");
  }

  if (doSeparate) {
    if (abscissaNames <= 0) {
      if (!(abscissaName = SDDS_Realloc(abscissaName, sizeof(*abscissaName) * (abscissaNames + columnNames))))
        SDDS_Bomb("memory allocation failure");
      abscissaNames = 0;
      for (i = 0; i < columnNames; i++) {
        abscissaName[abscissaNames + i] = columnName[i];
      }
      abscissaNames += columnNames;
    }
    if (columnNames > 1) {
      if (abscissaNames > 0) {
        if (columnNames != abscissaNames)
          SDDS_Bomb("the number of abscissa names must match the number of columns");
      }
      if (givenLowerLimits)
        if (columnNames != givenLowerLimits)
          SDDS_Bomb("the number of lower limits must match the number of columns");
      if (givenUpperLimits)
        if (columnNames != givenUpperLimits)
          SDDS_Bomb("the number of upper limits must match the number of columns");
    } else {
      /* Handle single column with multiple option names */
      if (abscissaNames > 0) {
        if (columnNames != abscissaNames)
          abscissaNames = 1;
      }
      if (givenLowerLimits)
        if (columnNames != givenLowerLimits)
          givenLowerLimits = 1;
      if (givenUpperLimits)
        if (columnNames != givenUpperLimits)
          givenUpperLimits = 1;
    }
  } else if (boundaryFile) {
    if (!(boundaryValue = ReadBoundaryData(boundaryFile, boundaryColumn, &nBoundaryValues, &boundaryColumnUnits))) {
      SDDS_Bomb("Problem reading boundary data");
    }
  } else {
    if (abscissaNames <= 0)
      SDDS_Bomb("Supply the name of the abscissa with -abscissaName");
    abscissaNames = 1;
  }

  SetUpOutput(&SDDSout, &SDDSin, &abscissaIndex, &cdfIndex, &histogramIndex, output, columnName, columnNames, abscissaName, abscissaNames, boundaryColumn, boundaryColumnUnits, columnMajorOrder, normMode);

  if (!(inputData = (double **)malloc(columnNames * sizeof(*inputData))) ||
      !(minValue = (double *)malloc(columnNames * sizeof(*minValue))) ||
      !(maxValue = (double *)malloc(columnNames * sizeof(*maxValue))))
    SDDS_Bomb("memory allocation failure");

  if (((binSize ? 1 : 0) + (binsGiven ? 1 : 0) + (autoBinsTarget ? 1 : 0)) > 1)
    SDDS_Bomb("Specify only one of -binSize, -bins, or -autoBins");
  if (!binSize && !binsGiven && !autoBinsTarget) {
    binsGiven = 1;
    bins = 20;
  }

  abscissa = histogram = cdf = NULL;

  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&SDDSin)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (rows) {
      if (weightColumn && !(weightData = SDDS_GetColumnInDoubles(&SDDSin, weightColumn)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      for (column = 0; column < columnNames; column++) {
        if (!(inputData[column] = SDDS_GetColumnInDoubles(&SDDSin, columnName[column])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      if (!boundaryColumn) {
        if (!(lowerLimit = (double *)malloc(sizeof(*lowerLimit) * columnNames)))
          SDDS_Bomb("memory allocation failure");
        if (!(upperLimit = (double *)malloc(sizeof(*upperLimit) * columnNames)))
          SDDS_Bomb("memory allocation failure");
        if (!(dx = (double *)malloc(sizeof(*dx) * columnNames)))
          SDDS_Bomb("memory allocation failure");

        if (doSeparate) {
          for (column = 0; column < columnNames; column++) {
            find_min_max(&minValue[column], &maxValue[column], inputData[column], rows);
            lowerLimit[column] = givenLowerLimits ? givenLowerLimit[column] : minValue[column];
            upperLimit[column] = givenUpperLimits ? givenUpperLimit[column] : maxValue[column];
          }
        } else {
          for (column = 0; column < columnNames; column++)
            find_min_max(&minValue[column], &maxValue[column], inputData[column], rows);
          lowerLimit[0] = givenLowerLimits ? givenLowerLimit[0] : min_in_array(minValue, columnNames);
          upperLimit[0] = givenUpperLimits ? givenUpperLimit[0] : max_in_array(maxValue, columnNames);
          for (column = 1; column < columnNames; column++) {
            lowerLimit[column] = lowerLimit[0];
            upperLimit[column] = upperLimit[0];
          }
        }

        range = (1 + expandRange) * (upperLimit[0] - lowerLimit[0]);
        if (autoBinsTarget) {
          bins = (int64_t)(rows / autoBinsTarget);
          if (autoBinsMinimum) {
            if (bins < autoBinsMinimum)
              bins = autoBinsMinimum;
          } else if (bins < 5) {
            bins = 5;
          }
          if (autoBinsMaximum) {
            if (bins > autoBinsMaximum)
              bins = autoBinsMaximum;
          } else if (bins > rows)
            bins = rows;
        }

        if (binSize) {
          maxRange = range;
          for (column = 0; column < columnNames; column++) {
            range = (1 + expandRange) * (upperLimit[column] - lowerLimit[column]);
            range = ((range / binSize) + 1) * binSize;
            if (range > maxRange)
              maxRange = range;
            middle = (lowerLimit[column] + upperLimit[column]) / 2;
            lowerLimit[column] = middle - range / 2;
            upperLimit[column] = middle + range / 2;
          }
          range = maxRange;
          if (!doSeparate) {
            lowerLimit[0] = min_in_array(lowerLimit, columnNames);
            upperLimit[0] = max_in_array(upperLimit, columnNames);
            dx[0] = binSize;
          }
          bins = maxRange / binSize + 0.5;
          if (bins < 1 && !doSides)
            bins = 2;
          if (doSeparate)
            for (column = 0; column < columnNames; column++) {
              range = upperLimit[column] - lowerLimit[column];
              upperLimit[column] += (maxRange - range) / 2;
              lowerLimit[column] -= (maxRange - range) / 2;
              dx[column] = binSize;
            }
        } else {
          if (doSeparate) {
            for (column = 0; column < columnNames; column++) {
              range = (1 + expandRange) * (upperLimit[column] - lowerLimit[column]);
              middle = (upperLimit[column] + lowerLimit[column]) / 2;
              upperLimit[column] = middle + range / 2;
              lowerLimit[column] = middle - range / 2;
              if (upperLimit[column] == lowerLimit[column]) {
                if (fabs(upperLimit[column]) < sqrt(DBL_MIN)) {
                  upperLimit[column] = sqrt(DBL_MIN);
                  lowerLimit[column] = -sqrt(DBL_MIN);
                } else {
                  lowerLimit[column] = upperLimit[column] * (1 - 10000 * DBL_EPSILON);
                  upperLimit[column] = upperLimit[column] * (1 + 10000 * DBL_EPSILON);
                }
              }
              dx[column] = (upperLimit[column] - lowerLimit[column]) / bins;
            }
          } else {
            range = (1 + expandRange) * (upperLimit[0] - lowerLimit[0]);
            middle = (upperLimit[0] + lowerLimit[0]) / 2;
            upperLimit[0] = middle + range / 2;
            lowerLimit[0] = middle - range / 2;
            if (upperLimit[0] == lowerLimit[0]) {
              if (fabs(upperLimit[0]) < sqrt(DBL_MIN)) {
                upperLimit[0] = sqrt(DBL_MIN);
                lowerLimit[0] = -sqrt(DBL_MIN);
              } else {
                lowerLimit[0] = upperLimit[0] * (1 - 10000 * DBL_EPSILON);
                upperLimit[0] = upperLimit[0] * (1 + 10000 * DBL_EPSILON);
              }
            }
            dx[0] = (upperLimit[0] - lowerLimit[0]) / bins;
          }
        }
        if (!binsGiven || !abscissa) {
          if (!(abscissa = SDDS_Realloc(abscissa, sizeof(*abscissa) * (bins + 2))) ||
              !(cdf = SDDS_Realloc(cdf, sizeof(*cdf) * (bins + 2))) ||
              !(histogram = SDDS_Realloc(histogram, sizeof(*histogram) * (bins + 2))))
            SDDS_Bomb("memory allocation failure");
        }
        writeBins = bins + (doSides ? 2 : 0);
        offset = doSides ? 0 : 1;
      } else {
        bins = writeBins = nBoundaryValues;
        abscissa = NULL;
        if (!(cdf = SDDS_Realloc(cdf, sizeof(*cdf) * bins)) ||
            !(histogram = SDDS_Realloc(histogram, sizeof(*histogram) * bins)))
          SDDS_Bomb("memory allocation failure");
      }

      if (!SDDS_StartPage(&SDDSout, writeBins) || !SDDS_CopyParameters(&SDDSout, &SDDSin))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      if (boundaryColumn) {
        for (column = 0; column < columnNames; column++) {
          MakeBoundaryHistogram(histogram, cdf, boundaryValue, nBoundaryValues, inputData[column], weightData, rows);
          NormalizeHistogram(histogram, nBoundaryValues, normMode);
          if (!cdfOnly && !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, histogram, writeBins, histogramIndex[column]))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          if (!freOnly && !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, cdf, writeBins, cdfIndex[column]))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          free(inputData[column]);
        }
        if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_NAME, boundaryValue, writeBins, boundaryColumn))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else if (!doSeparate) {
        for (i = -1; i < bins + 1; i++)
          abscissa[i + 1] = (i + 0.5) * dx[0] + lowerLimit[0];
        switch (doSides) {
        case DO_CLOSE_SIDES:
          abscissa[0] = abscissa[1] - dx[0] / 2;
          abscissa[bins + 1] = abscissa[bins] + dx[0] / 2;
          break;
        case DO_AGAINST_SIDES:
          abscissa[0] = abscissa[1];
          abscissa[bins + 1] = abscissa[bins];
          break;
        }
        if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, abscissa + offset, writeBins, abscissaIndex[0]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (column = 0; column < columnNames; column++) {
          histogram[0] = histogram[bins + 1] = 0;
          if (!weightColumn)
            make_histogram(histogram + 1, bins, lowerLimit[0], upperLimit[0], inputData[column], rows, 1);
          else
            make_histogram_weighted(histogram + 1, bins, lowerLimit[0], upperLimit[0], inputData[column], rows, 1, weightData);
          NormalizeHistogram(histogram, bins, normMode);
          sum = 0;
          for (i = 0; i <= bins + 1; i++)
            sum += histogram[i];
          for (i = 0; i <= bins + 1; i++) {
            if (!i)
              cdf[i] = histogram[i] / sum;
            else
              cdf[i] = cdf[i - 1] + histogram[i] / sum;
          }
          if (!cdfOnly) {
            if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, histogram + offset, writeBins, histogramIndex[column]))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          if (!freOnly) {
            if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, cdf + offset, writeBins, cdfIndex[column]))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          free(inputData[column]);
        }
      } else {
        for (column = 0; column < columnNames; column++) {
          for (i = -1; i < bins + 1; i++)
            abscissa[i + 1] = (i + 0.5) * dx[column] + lowerLimit[column];
          switch (doSides) {
          case DO_CLOSE_SIDES:
            abscissa[0] = abscissa[1] - dx[column] / 2;
            abscissa[bins + 1] = abscissa[bins] + dx[column] / 2;
            break;
          case DO_AGAINST_SIDES:
            abscissa[0] = abscissa[1];
            abscissa[bins + 1] = abscissa[bins];
            break;
          }
          if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, abscissa + offset, writeBins, abscissaIndex[column]))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          histogram[0] = histogram[bins + 1] = 0;
          if (!weightColumn)
            make_histogram(histogram + 1, bins, lowerLimit[column], upperLimit[column], inputData[column], rows, 1);
          else
            make_histogram_weighted(histogram + 1, bins, lowerLimit[column], upperLimit[column], inputData[column], rows, 1, weightData);
          NormalizeHistogram(histogram, bins + 2, normMode);
          sum = 0;
          for (i = 0; i <= bins + 1; i++)
            sum += histogram[i];
          for (i = 0; i <= bins + 1; i++) {
            if (!i)
              cdf[i] = histogram[i] / sum;
            else
              cdf[i] = cdf[i - 1] + histogram[i] / sum;
          }
          if (!cdfOnly) {
            if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, histogram + offset, writeBins, histogramIndex[column]))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          if (!freOnly) {
            if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, cdf + offset, writeBins, cdfIndex[column]))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          free(inputData[column]);
        }
      }
    } else {
      if (!SDDS_StartPage(&SDDSout, rows ? bins : 0))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (weightData) {
      free(weightData);
      weightData = NULL;
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

  free(histogram);
  free(cdf);
  free(abscissa);
  if (minValue)
    free(minValue);
  if (maxValue)
    free(maxValue);
  if (lowerLimit)
    free(lowerLimit);
  if (upperLimit)
    free(upperLimit);
  if (dx)
    free(dx);

  return EXIT_SUCCESS;
}

void SetUpOutput(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, long **abscissaIndex, long **cdfIndex,
                 long **histogramIndex, char *output, char **columnName, long columnNames,
                 char **abscissaName, long abscissaNames, char *boundaryColumn, char *boundaryUnits,
                 short columnMajorOrder, short normMode) {
  long column;
  char s[SDDS_MAXLINE];
  int32_t outputType = SDDS_DOUBLE;
  char *blankString = "";

  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddsmultihist output", output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (columnMajorOrder != -1)
    SDDSout->layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout->layout.data_mode.column_major = SDDSin->layout.data_mode.column_major;

  if (!(*cdfIndex = (long *)malloc(sizeof(**cdfIndex) * columnNames)))
    SDDS_Bomb("memory allocation failure");
  if (!cdfOnly) {
    if (!(*histogramIndex = (long *)malloc(sizeof(**histogramIndex) * columnNames)))
      SDDS_Bomb("memory allocation failure");
  }
  if (!boundaryColumn) {
    if (!(*abscissaIndex = (long *)malloc(sizeof(**abscissaIndex) * columnNames)))
      SDDS_Bomb("memory allocation failure");

    for (column = 0; column < abscissaNames; column++) {
      if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, columnName[column], abscissaName[column]) ||
          !SDDS_ChangeColumnInformation(SDDSout, "type", &outputType, SDDS_BY_NAME, abscissaName[column]) ||
          ((*abscissaIndex)[column] = SDDS_GetColumnIndex(SDDSout, abscissaName[column])) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (strcmp(columnName[column], abscissaName[column]) != 0 &&
          (!SDDS_ChangeColumnInformation(SDDSout, "description", blankString, SDDS_BY_NAME, abscissaName[column]) ||
           !SDDS_ChangeColumnInformation(SDDSout, "symbol", blankString, SDDS_BY_NAME, abscissaName[column])))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  } else {
    if (!SDDS_DefineSimpleColumn(SDDSout, boundaryColumn, boundaryUnits, SDDS_DOUBLE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  for (column = 0; column < columnNames; column++) {
    if (!freOnly) {
      sprintf(s, "%sCdf", columnName[column]);
      if (((*cdfIndex)[column] = SDDS_DefineColumn(SDDSout, s, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!cdfOnly) {
      switch (normMode) {
      case NORMALIZE_PEAK:
        sprintf(s, "%sRelativeFrequency", columnName[column]);
        break;
      case NORMALIZE_SUM:
        sprintf(s, "%sFractionalFrequency", columnName[column]);
        break;
      default:
        sprintf(s, "%sFrequency", columnName[column]);
        break;
      }
      if (((*histogramIndex)[column] = SDDS_DefineColumn(SDDSout, s, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!SDDS_TransferAllParameterDefinitions(SDDSout, SDDSin, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

double *ReadBoundaryData(char *file, char *column, int64_t *n, char **units) {
  SDDS_DATASET SDDSin;
  double *data;
  int64_t j;

  if (!SDDS_InitializeInput(&SDDSin, file)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (SDDS_GetColumnInformation(&SDDSin, "units", units, SDDS_GET_BY_NAME, column) != SDDS_STRING)
    return NULL;

  if (SDDS_ReadPage(&SDDSin) <= 0) {
    SDDS_SetError("No pages in boundary data file");
    return NULL;
  }

  *n = SDDS_RowCount(&SDDSin);
  if (*n <= 0)
    return NULL;
  if (!(data = SDDS_GetColumnInDoubles(&SDDSin, column)))
    return NULL;
  for (j = 1; j < (*n); j++) {
    if (data[j] <= data[j - 1]) {
      memmove(data + j, data + j + 1, sizeof(*data) * ((*n) - 1 - j));
      *n -= 1;
      j -= 1;
    }
  }
  return data;
}

void MakeBoundaryHistogram(double *histogram, double *cdf, double *boundaryValue, int64_t nBoundaryValues,
                           double *data, double *weight, int64_t nData) {
  int64_t i, j;
  for (i = 0; i < nBoundaryValues; i++)
    histogram[i] = cdf[i] = 0;
  for (i = 0; i < nData; i++) {
    j = binaryArraySearch(boundaryValue, sizeof(double), nBoundaryValues, &data[i], double_cmpasc, 1) + 1;
    if (j < nBoundaryValues && j >= 0)
      histogram[j] += weight ? fabs(weight[i]) : 1;
  }
  cdf[0] = histogram[0];
  for (j = 1; j < nBoundaryValues; j++) {
    cdf[j] = cdf[j - 1] + histogram[j];
  }
  if (cdf[nBoundaryValues - 1] > 0)
    for (j = 0; j < nBoundaryValues; j++)
      cdf[j] /= cdf[nBoundaryValues - 1];
}

void NormalizeHistogram(double *hist, int64_t bins, short mode) {
  int64_t i;
  double value = 0;
  switch (mode) {
  case NORMALIZE_SUM:
    for (i = 0; i < bins; i++)
      value += hist[i];
    break;
  case NORMALIZE_PEAK:
    for (i = 0; i < bins; i++)
      if (hist[i] > value)
        value = hist[i];
    break;
  default:
    return;
    break;
  }
  if (value)
    for (i = 0; i < bins; i++)
      hist[i] /= value;
}
