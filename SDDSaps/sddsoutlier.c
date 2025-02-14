/**
 * @file sddsoutlier.c
 * @brief Eliminates statistical outliers from SDDS data files.
 *
 * @details
 * This program processes an input SDDS (Self Describing Data Set) file to identify and eliminate
 * statistical outliers based on various criteria such as standard deviation limits, absolute
 * limits, percentile limits, and more. Outliers can be removed, marked, or replaced with
 * specified values. The processed data is then written to an output SDDS file.
 *
 * @section Usage
 * ```
 * sddsoutlier [<inputfile>] [<outputfile>]
 *             [-pipe=[input][,output]]
 *             [-verbose] 
 *             [-noWarnings] 
 *              -columns=<list-of-names>
 *             [-excludeColumns=<list-of-names>]
 *              -stDevLimit=<value>
 *             [-absLimit=<value>] 
 *             [-absDeviationLimit=<value>[,neighbor=<number>]]
 *             [-maximumLimit=<value>] 
 *             [-minimumLimit=<value>]
 *             [-chanceLimit=<minimumChance>] 
 *             [-passes=<number>]
 *             [-percentileLimit=lower=<lowerPercent>,upper=<upperPercent>]
 *             [-unpopular=bins=<number>]
 *             [-invert] 
 *             [-majorOrder] 
 *             [-markOnly]
 *             [-replaceOnly={lastValue|nextValue|interpolatedValue|value=<number>}]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Specifies a list of column names to process.                                           |
 * | `-stDevLimit`                         | Sets the standard deviation limit for outlier detection.                              |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard input/output as data streams.                                            |
 * | `-verbose`                            | Enables verbose output, displaying processing information.                            |
 * | `-noWarnings`                         | Suppresses warning messages.                                                         |
 * | `-excludeColumns`                     | Specifies a list of column names to exclude from processing.                          |
 * | `-absLimit`                           | Sets an absolute value limit for outlier detection.                                   |
 * | `-absDeviationLimit`                  | Sets the absolute deviation limit from the mean with optional neighbors.            |
 * | `-maximumLimit`                       | Sets a maximum value for outlier detection.                                           |
 * | `-minimumLimit`                       | Sets a minimum value for outlier detection.                                           |
 * | `-chanceLimit`                        | Sets a minimum probability threshold for outlier detection (Gaussian statistics).     |
 * | `-passes`                             | Sets the number of passes for outlier detection.                                      |
 * | `-percentileLimit`                    | Sets percentile limits for outlier detection.       |
 * | `-unpopular`                          | Removes points not in the most populated bin of a histogram.                          |
 * | `-invert`                             | Inverts the outlier selection criteria.                                               |
 * | `-majorOrder`                         | Specifies output file data ordering (row or column major).                            |
 * | `-markOnly`                           | Marks identified outliers without removing them.                                      |
 * | `-replaceOnly`                        | Replaces outliers with specified values.          |
 *
 * @subsection Incompatibilities
 *   - `-markOnly` is incompatible with:
 *     - `-replaceOnly`
 *   - For `-absDeviationLimit`:
 *     - Requires a positive value for the limit.
 *     - Optional `neighbor` parameter must be odd.
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
#include "SDDS.h"
#include "scan.h"
#include "SDDSutils.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_COLUMNS,
  SET_EXCLUDE,
  SET_STDDEV_LIMIT,
  SET_ABS_LIMIT,
  SET_ABSDEV_LIMIT,
  SET_VERBOSE,
  SET_PIPE,
  SET_NOWARNINGS,
  SET_INVERT,
  SET_MARKONLY,
  SET_CHANCELIMIT,
  SET_PASSES,
  SET_REPLACE,
  SET_MAXLIMIT,
  SET_MINLIMIT,
  SET_MAJOR_ORDER,
  SET_PERCENTILE_LIMIT,
  SET_UNPOPULAR,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "columns",
  "excludecolumns",
  "stdevlimit",
  "abslimit",
  "absdeviationlimit",
  "verbose",
  "pipe",
  "nowarnings",
  "invert",
  "markonly",
  "chancelimit",
  "passes",
  "replaceonly",
  "maximumlimit",
  "minimumlimit",
  "majororder",
  "percentilelimit",
  "unpopular",
};

#define USAGE "\n"                                   \
  "sddsoutlier [<inputfile>] [<outputfile>]\n"       \
  "            [-pipe=[input][,output]]\n"           \
  "            [-verbose] \n"                        \
  "            [-noWarnings] \n"                     \
  "             -columns=<list-of-names>\n"          \
  "            [-excludeColumns=<list-of-names>]\n"  \
  "             -stDevLimit=<value>\n"               \
  "            [-absLimit=<value>] \n"               \
  "            [-absDeviationLimit=<value>[,neighbor=<number>]]\n"                      \
  "            [-maximumLimit=<value>] \n"           \
  "            [-minimumLimit=<value>]\n"            \
  "            [-chanceLimit=<minimumChance>] \n"    \
  "            [-passes=<number>]\n"                 \
  "            [-percentileLimit=lower=<lowerPercent>,upper=<upperPercent>]\n"          \
  "            [-unpopular=bins=<number>]\n"         \
  "            [-invert] \n"                         \
  "            [-majorOrder] \n"                     \
  "            [-markOnly]\n"                        \
  "            [-replaceOnly={lastValue|nextValue|interpolatedValue|value=<number>}]\n" \
  "Options:\n"                                                          \
  "  -pipe=[input][,output]\n"                                          \
  "        Use standard input and/or output as data streams.\n"         \
  "  -verbose\n"                                                        \
  "        Enable verbose output, displaying processing information.\n" \
  "  -noWarnings\n"                                                     \
  "        Suppress warning messages.\n"                                \
  "  -columns=<list-of-names>\n"                                        \
  "        Specify a comma-separated list of column names to process.\n" \
  "  -excludeColumns=<list-of-names>\n"                                 \
  "        Specify a comma-separated list of column names to exclude from processing.\n" \
  "  -stDevLimit=<value>\n"                                             \
  "        Point is an outlier if it is more than <value> standard deviations from the mean.\n" \
  "  -absLimit=<value>\n"                                               \
  "        Point is an outlier if it has an absolute value greater than <value>.\n" \
  "  -absDeviationLimit=<value>[,neighbor=<number>]\n"                  \
  "        Point is an outlier if its absolute deviation from the mean exceeds <value>.\n" \
  "        If neighbor is provided, the mean is computed with the neighbors instead of the whole data.\n" \
  "  -minimumLimit=<value>\n"                                           \
  "        Point is an outlier if it is less than <value>.\n"           \
  "  -maximumLimit=<value>\n"                                           \
  "        Point is an outlier if it is greater than <value>.\n"        \
  "  -chanceLimit=<minimumChance>\n"                                    \
  "        Point is an outlier if it has a probability less than <minimumChance> of occurring (Gaussian statistics).\n" \
  "  -percentileLimit=lower=<lowerPercent>,upper=<upperPercent>\n"      \
  "        Point is an outlier if it is below the <lowerPercent> percentile or above the <upperPercent> percentile.\n" \
  "  -unpopular=bins=<number>\n"                                        \
  "        Remove points that are not in the most populated bin based on a histogram with <number> bins.\n" \
  "  -invert\n"                                                         \
  "        Invert the outlier selection criteria.\n"                    \
  "  -majorOrder=row|column\n"                                          \
  "        Specify output file data ordering as row or column major.\n" \
  "  -markOnly\n"                                                       \
  "        Mark identified outliers without removing them.\n"           \
  "  -replaceOnly={lastValue|nextValue|interpolatedValue|value=<number>}\n" \
  "        Replace outliers with specified values or strategies.\n"     \
  "  -passes=<number>\n"                                                \
  "        Define the number of passes for outlier detection.\n\n"      \
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n"

#define OUTLIER_CONTROL_INVOKED 0x00001U
#define OUTLIER_STDEV_GIVEN 0x00002U
#define OUTLIER_FRACTION_GIVEN 0x00004U
#define OUTLIER_STDEVLIMIT_GIVEN 0x00008U
#define OUTLIER_UNPOPULAR_BINS 0x00010U
#define OUTLIER_VERBOSE_GIVEN 0x00020U
#define OUTLIER_ABSLIMIT_GIVEN 0x00040U
#define OUTLIER_ABSDEVLIMIT_GIVEN 0x00080U
#define OUTLIER_INVERT_GIVEN 0x00100U
#define OUTLIER_MARKONLY 0x00200U
#define OUTLIER_CHANCELIMIT_GIVEN 0x00400U
#define OUTLIER_MAXLIMIT_GIVEN 0x00800U
#define OUTLIER_MINLIMIT_GIVEN 0x01000U
#define OUTLIER_REPLACELAST 0x02000U
#define OUTLIER_REPLACENEXT 0x04000U
#define OUTLIER_REPLACEINTERP 0x08000U
#define OUTLIER_REPLACEVALUE 0x10000U
#define OUTLIER_REPLACEFLAGS (OUTLIER_REPLACELAST | OUTLIER_REPLACENEXT | OUTLIER_REPLACEINTERP | OUTLIER_REPLACEVALUE)
#define OUTLIER_PERCENTILE_LOWER 0x20000U
#define OUTLIER_PERCENTILE_UPPER 0x40000U
#define OUTLIER_PERCENTILE_FLAGS (OUTLIER_PERCENTILE_LOWER | OUTLIER_PERCENTILE_UPPER)

typedef struct
{
  double stDevLimit, fractionLimit, absoluteLimit, absDevLimit;
  double chanceLimit, replacementValue, maximumLimit, minimumLimit;
  double percentilePoint[2];
  long passes;
  int32_t unpopularBins;
  int32_t neighbors;
  unsigned long flags;
} OUTLIER_CONTROL;

int64_t removeOutliers(SDDS_DATASET *SDDSin, int64_t rows, char **column, long columns, OUTLIER_CONTROL *outlierControl, int32_t *isOutlier);
long meanStDevForFlaggedData(double *mean, double *stDev, double *data, int32_t *keep, int64_t rows);

int main(int argc, char **argv) {
  int iArg;
  char **column, **excludeColumn;
  long columns, excludeColumns;
  char *input, *output;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  long readCode, dataLimitGiven, tmpfileUsed;
  int64_t i, rows;
  long noWarnings, isOutlierIndex;
  int32_t *isOutlier;
  OUTLIER_CONTROL outlierControl;
  unsigned long pipeFlags, tmpFlags, majorOrderFlag, dummyFlags;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    bomb(NULL, USAGE);
  }

  output = input = NULL;
  columns = excludeColumns = dataLimitGiven = 0;
  column = excludeColumn = NULL;

  outlierControl.flags = 0;
  outlierControl.passes = 1;
  outlierControl.neighbors = 0;
  pipeFlags = tmpfileUsed = noWarnings = isOutlierIndex = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_COLUMNS:
        if (columns)
          SDDS_Bomb("only one -columns option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        column = tmalloc(sizeof(*column) * (columns = scanned[iArg].n_items - 1));
        for (i = 0; i < columns; i++)
          column[i] = scanned[iArg].list[i + 1];
        break;
      case SET_EXCLUDE:
        if (excludeColumns)
          SDDS_Bomb("only one -excludecolumns option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -excludecolumns syntax");
        excludeColumn = tmalloc(sizeof(*excludeColumn) * (excludeColumns = scanned[iArg].n_items - 1));
        for (i = 0; i < excludeColumns; i++)
          excludeColumn[i] = scanned[iArg].list[i + 1];
        break;
      case SET_STDDEV_LIMIT:
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%lf", &outlierControl.stDevLimit) != 1 ||
            outlierControl.stDevLimit <= 0)
          SDDS_Bomb("invalid -stDevLimit syntax");
        outlierControl.flags |= OUTLIER_CONTROL_INVOKED | OUTLIER_STDEV_GIVEN | OUTLIER_STDEVLIMIT_GIVEN;
        break;
      case SET_ABS_LIMIT:
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%lf", &outlierControl.absoluteLimit) != 1 ||
            outlierControl.absoluteLimit <= 0)
          SDDS_Bomb("invalid -absLimit syntax");
        outlierControl.flags |= OUTLIER_CONTROL_INVOKED | OUTLIER_ABSLIMIT_GIVEN;
        break;
      case SET_ABSDEV_LIMIT:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -absDeviationLimit syntax");
        if (scanned[iArg].n_items == 2) {
          if (sscanf(scanned[iArg].list[1], "%lf", &outlierControl.absDevLimit) != 1 || outlierControl.absDevLimit <= 0)
            SDDS_Bomb("invalid -absDeviationLimit syntax");
        } else {
          if (sscanf(scanned[iArg].list[1], "%lf", &outlierControl.absDevLimit) != 1 || outlierControl.absDevLimit <= 0)
            SDDS_Bomb("invalid -absDeviationLimit syntax");
          scanned[iArg].list += 2;
          scanned[iArg].n_items -= 2;
          if (scanned[iArg].n_items > 0 &&
              (!scanItemList(&dummyFlags, scanned[iArg].list, &scanned[iArg].n_items, 0, "neighbors", SDDS_LONG, &(outlierControl.neighbors), 1, 0, NULL)))
            SDDS_Bomb("invalid -absDeviationLimit syntax/value");
          if (outlierControl.neighbors % 2 == 0)
            outlierControl.neighbors += 1;
          /* always make it an odd number */
          scanned[iArg].list -= 2;
          scanned[iArg].n_items += 2;
        }
        outlierControl.flags |= OUTLIER_CONTROL_INVOKED | OUTLIER_ABSDEVLIMIT_GIVEN;
        break;
      case SET_VERBOSE:
        outlierControl.flags |= OUTLIER_VERBOSE_GIVEN;
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_NOWARNINGS:
        noWarnings = 1;
        break;
      case SET_INVERT:
        outlierControl.flags |= OUTLIER_INVERT_GIVEN;
        break;
      case SET_MARKONLY:
        outlierControl.flags |= OUTLIER_MARKONLY;
        break;
      case SET_CHANCELIMIT:
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%lf", &outlierControl.chanceLimit) != 1 ||
            outlierControl.chanceLimit <= 0)
          SDDS_Bomb("invalid -chanceLimit syntax");
        outlierControl.flags |= OUTLIER_CONTROL_INVOKED | OUTLIER_CHANCELIMIT_GIVEN;
        break;
      case SET_PASSES:
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%ld", &outlierControl.passes) != 1 ||
            outlierControl.passes < 1)
          SDDS_Bomb("invalid -passes syntax");
        break;
      case SET_MAXLIMIT:
        outlierControl.flags |= OUTLIER_MAXLIMIT_GIVEN | OUTLIER_CONTROL_INVOKED;
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%lf", &outlierControl.maximumLimit) != 1)
          SDDS_Bomb("invalid -maximumLimit syntax");
        break;
      case SET_MINLIMIT:
        outlierControl.flags |= OUTLIER_MINLIMIT_GIVEN | OUTLIER_CONTROL_INVOKED;
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%lf", &outlierControl.minimumLimit) != 1)
          SDDS_Bomb("invalid -minimumLimit syntax");
        break;
      case SET_REPLACE:
        if (scanned[iArg].n_items != 2)
          SDDS_Bomb("invalid -replace syntax");
        scanned[iArg].n_items -= 1;
        if (!scanItemList(&tmpFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "lastvalue", -1, NULL, 0, OUTLIER_REPLACELAST,
                          "nextvalue", -1, NULL, 0, OUTLIER_REPLACENEXT,
                          "interpolatedvalue", -1, NULL, 0, OUTLIER_REPLACEINTERP,
                          "value", SDDS_DOUBLE, &outlierControl.replacementValue, 1, OUTLIER_REPLACEVALUE, NULL))
          SDDS_Bomb("invalid -replace syntax/values");
        outlierControl.flags |= tmpFlags | OUTLIER_CONTROL_INVOKED;
        break;
      case SET_PERCENTILE_LIMIT:
        if (scanned[iArg].n_items < 3)
          SDDS_Bomb("invalid -percentileLimit syntax");
        scanned[iArg].n_items -= 1;
        if (!scanItemList(&tmpFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "lower", SDDS_DOUBLE, &outlierControl.percentilePoint[0], 1, OUTLIER_PERCENTILE_LOWER,
                          "upper", SDDS_DOUBLE, &outlierControl.percentilePoint[1], 1, OUTLIER_PERCENTILE_UPPER, NULL) ||
            !(tmpFlags & OUTLIER_PERCENTILE_LOWER) || !(tmpFlags & OUTLIER_PERCENTILE_UPPER) ||
            outlierControl.percentilePoint[0] >= outlierControl.percentilePoint[1])
          SDDS_Bomb("invalid -percentileLimit syntax");
        outlierControl.flags |= tmpFlags | OUTLIER_CONTROL_INVOKED;
        break;
      case SET_UNPOPULAR:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -unpopular syntax");
        scanned[iArg].n_items -= 1;
        if (!scanItemList(&tmpFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "bins", SDDS_LONG, &(outlierControl.unpopularBins), 1, OUTLIER_UNPOPULAR_BINS, NULL) ||
            !(tmpFlags & OUTLIER_UNPOPULAR_BINS) || outlierControl.unpopularBins < 2)
          SDDS_Bomb("invalid -unpopular syntax");
        outlierControl.flags |= tmpFlags | OUTLIER_CONTROL_INVOKED;
        break;
      default:
        fprintf(stderr, "Error: Unknown or ambiguous option: %s\n", scanned[iArg].list[0]);
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
  if (outlierControl.flags & OUTLIER_REPLACEFLAGS && outlierControl.flags & OUTLIER_MARKONLY)
    SDDS_Bomb("Cannot use -replaceOnly and -markOnly simultaneously.");

  processFilenames("sddsoutlier", &input, &output, pipeFlags, noWarnings, &tmpfileUsed);

  if (!(outlierControl.flags & OUTLIER_CONTROL_INVOKED)) {
    outlierControl.flags |= OUTLIER_CONTROL_INVOKED | OUTLIER_STDEV_GIVEN | OUTLIER_STDEVLIMIT_GIVEN;
    outlierControl.stDevLimit = 2;
  }

  if (!SDDS_InitializeInput(&SDDSin, input) ||
      !SDDS_InitializeCopy(&SDDSout, &SDDSin, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;

  if (outlierControl.flags & OUTLIER_MARKONLY &&
      (isOutlierIndex = SDDS_GetColumnIndex(&SDDSout, "IsOutlier")) < 0) {
    if (!SDDS_DefineColumn(&SDDSout, "IsOutlier", NULL, NULL, NULL, NULL, SDDS_SHORT, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((columns = expandColumnPairNames(&SDDSout, &column, NULL, columns, excludeColumn, excludeColumns, FIND_NUMERIC_TYPE, 0)) <= 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_Bomb("No columns selected for outlier control.");
  }

  isOutlier = NULL;
  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    if (!SDDS_CopyPage(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if ((rows = SDDS_CountRowsOfInterest(&SDDSout)) < 3) {
      if (!SDDS_WritePage(&SDDSout))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      continue;
    }
    if (outlierControl.flags & OUTLIER_MARKONLY) {
      if (isOutlierIndex >= 0) {
        if (!(isOutlier = SDDS_GetNumericColumn(&SDDSout, "IsOutlier", SDDS_LONG)))
          SDDS_Bomb("Unable to retrieve 'IsOutlier' column from input file despite its existence.");
      } else {
        long i;
        isOutlier = SDDS_Realloc(isOutlier, sizeof(*isOutlier) * rows);
        if (!isOutlier)
          SDDS_Bomb("Memory allocation failure.");
        for (i = 0; i < rows; i++)
          isOutlier[i] = 0;
      }
    }
    if (outlierControl.flags & OUTLIER_VERBOSE_GIVEN)
      fprintf(stderr, "%" PRId64 " rows in page %ld\n", rows, readCode);
    if ((rows = removeOutliers(&SDDSout, rows, column, columns, &outlierControl, isOutlier)) == 0) {
      if (!noWarnings)
        fprintf(stderr, "  No rows left after outlier control--skipping page.\n");
      continue;
    }
    if (outlierControl.flags & OUTLIER_VERBOSE_GIVEN)
      fprintf(stderr, "%" PRId64 " rows left after outlier control\n", rows);
    if (rows != SDDS_CountRowsOfInterest(&SDDSout)) {
      fprintf(stderr, "Problem with row selection:\n  %" PRId64 " expected, %" PRId64 " counted\n", rows, SDDS_CountRowsOfInterest(&SDDSout));
      exit(EXIT_FAILURE);
    }
    if ((isOutlier && !SDDS_SetColumnFromLongs(&SDDSout, SDDS_SET_BY_NAME, isOutlier, rows, "IsOutlier")) ||
        !SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (tmpfileUsed && !replaceFileAndBackUp(input, output))
    exit(EXIT_FAILURE);
  return EXIT_SUCCESS;
}

int64_t removeOutliers(SDDS_DATASET *dataset, int64_t rows, char **column, long columns, OUTLIER_CONTROL *outlierControl, int32_t *isOutlier) {
  long icol, ipass;
  int64_t irow, kept, killed, j, k, summed;
  double *data, sum1, stDev, mean;
  static int32_t *keep = NULL;
  double lastGoodValue = 0;
  int64_t irow0, irow1;

  if (!SDDS_SetRowFlags(dataset, 1))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  /* Allocate or reallocate the keep array */
  keep = SDDS_Realloc(keep, sizeof(*keep) * rows);
  if (!keep)
    SDDS_Bomb("Memory allocation failure.");

  if (!isOutlier) {
    for (irow = 0; irow < rows; irow++)
      keep[irow] = 1;
    kept = rows;
  } else {
    for (irow = kept = 0; irow < rows; irow++)
      if ((keep[irow] = !isOutlier[irow]))
        kept++;
  }

  for (icol = 0; icol < columns; icol++) {
    /* Loop over columns for which outlier control is to be done */
    data = SDDS_GetColumnInDoubles(dataset, column[icol]);
    if (!data)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    for (ipass = 0; ipass < outlierControl->passes; ipass++) {
      if (outlierControl->flags & OUTLIER_UNPOPULAR_BINS && rows > 1) {
        double *hist, lo, hi, delta;
        int64_t imin, imax, ih;
        hist = tmalloc(sizeof(*hist) * outlierControl->unpopularBins);
        find_min_max(&lo, &hi, data, rows);
        make_histogram(hist, outlierControl->unpopularBins, lo, hi, data, rows, 1);
        delta = (hi - lo) / outlierControl->unpopularBins; /* yes, not bins-1 */
        index_min_max(&imin, &imax, hist, outlierControl->unpopularBins);
        for (irow = killed = 0; irow < rows; irow++) {
          ih = (data[irow] - lo) / delta;
          if (ih != imax) {
            killed++;
            kept--;
            keep[irow] = 0;
          }
        }
        free(hist);
        if (killed && (outlierControl->flags & OUTLIER_VERBOSE_GIVEN))
          fprintf(stderr, "%" PRId64 " additional rows killed by column %s unpopular control\n", killed, column[icol]);
      }

      if (outlierControl->flags & OUTLIER_PERCENTILE_FLAGS) {
        double percentileResult[2];
        killed = 0;
        if (compute_percentiles(percentileResult, outlierControl->percentilePoint, 2, data, rows)) {
          for (irow = killed = 0; irow < rows; irow++) {
            if ((data[irow] < percentileResult[0] || data[irow] > percentileResult[1]) && keep[irow]) {
              killed++;
              kept--;
              keep[irow] = 0;
            }
          }
        }
        if (killed && (outlierControl->flags & OUTLIER_VERBOSE_GIVEN))
          fprintf(stderr, "%" PRId64 " additional rows killed by column %s percentile outlier control\n", killed, column[icol]);
      }

      if (outlierControl->flags & OUTLIER_MINLIMIT_GIVEN) {
        /* Limit values to exceed a minimum */
        for (irow = killed = 0; irow < rows; irow++) {
          if (keep[irow] && data[irow] < outlierControl->minimumLimit) {
            kept--;
            keep[irow] = 0;
            killed++;
          }
        }
        if (killed && (outlierControl->flags & OUTLIER_VERBOSE_GIVEN))
          fprintf(stderr, "%" PRId64 " additional rows killed by column %s minimum value outlier control\n", killed, column[icol]);
      }

      if (outlierControl->flags & OUTLIER_MAXLIMIT_GIVEN) {
        /* Limit values to exceed a maximum */
        for (irow = killed = 0; irow < rows; irow++) {
          if (keep[irow] && data[irow] > outlierControl->maximumLimit) {
            kept--;
            keep[irow] = 0;
            killed++;
          }
        }
        if (killed && (outlierControl->flags & OUTLIER_VERBOSE_GIVEN))
          fprintf(stderr, "%" PRId64 " additional rows killed by column %s maximum value outlier control\n", killed, column[icol]);
      }

      if (outlierControl->flags & OUTLIER_ABSLIMIT_GIVEN) {
        /* Limit absolute values */
        for (irow = killed = 0; irow < rows; irow++) {
          if (keep[irow] && fabs(data[irow]) > outlierControl->absoluteLimit) {
            kept--;
            keep[irow] = 0;
            killed++;
          }
        }
        if (killed && (outlierControl->flags & OUTLIER_VERBOSE_GIVEN))
          fprintf(stderr, "%" PRId64 " additional rows killed by column %s absolute value outlier control\n", killed, column[icol]);
      }

      if (outlierControl->flags & OUTLIER_ABSDEVLIMIT_GIVEN) {
        /* Limit absolute deviation from mean */
        if (outlierControl->neighbors > 0) {
          for (irow = killed = 0; irow < rows; irow++) {
            if (!keep[irow])
              continue;
            mean = 0;
            for (j = irow - outlierControl->neighbors / 2; j <= irow + outlierControl->neighbors / 2; j++) {
              if (j < 0)
                k = irow + outlierControl->neighbors / 2 - j;
              else if (j > rows - 1)
                k = irow - outlierControl->neighbors / 2 - (j - rows + 1);
              else
                k = j;
              mean += fabs(data[k]);
            }
            mean = mean / outlierControl->neighbors;
            if (keep[irow] && fabs(data[irow] - mean) > outlierControl->absDevLimit) {
              keep[irow] = 0;
              kept--;
              killed++;
            }
          }
        } else {
          for (irow = sum1 = summed = 0; irow < rows; irow++) {
            if (!keep[irow])
              continue;
            sum1 += data[irow];
            summed++;
          }
          if (summed < 1)
            continue;
          mean = sum1 / summed;
          for (irow = killed = 0; irow < rows; irow++)
            if (keep[irow] && fabs(data[irow] - mean) > outlierControl->absDevLimit) {
              keep[irow] = 0;
              kept--;
              killed++;
            }
        }
        if (killed && (outlierControl->flags & OUTLIER_VERBOSE_GIVEN))
          fprintf(stderr, "%" PRId64 " additional rows killed by column %s absolute deviation outlier control\n", killed, column[icol]);
      }

      if (outlierControl->flags & OUTLIER_STDEV_GIVEN && kept && meanStDevForFlaggedData(&mean, &stDev, data, keep, rows) && stDev) {
        /* Limit deviation from mean in units of standard deviations */
        for (irow = killed = 0; irow < rows; irow++)
          if (keep[irow] && fabs(data[irow] - mean) > outlierControl->stDevLimit * stDev) {
            keep[irow] = 0;
            kept--;
            killed++;
          }
        if (killed && (outlierControl->flags & OUTLIER_VERBOSE_GIVEN))
          fprintf(stderr, "%" PRId64 " additional rows killed by column %s standard deviation outlier control\n", killed, column[icol]);
      }

      if (outlierControl->flags & OUTLIER_CHANCELIMIT_GIVEN) {
        /* Limit improbability of a point occurring based on Gaussian statistics */
        if (kept && meanStDevForFlaggedData(&mean, &stDev, data, keep, rows) && stDev) {
          int64_t lastKept;
          double gProb, probOfSeeing;
          lastKept = kept;
          for (irow = killed = 0; irow < rows; irow++) {
            if (!keep[irow])
              continue;
            /* gProb = (probability of value >= x) */
            /* (1-gProb)^n = probability of not seeing x in n trials */
            gProb = normSigLevel((data[irow] - mean) / stDev, 2);
            probOfSeeing = 1 - ipow(1 - gProb, lastKept);
            if (probOfSeeing < outlierControl->chanceLimit) {
              keep[irow] = 0;
              kept--;
              killed++;
            }
          }
          if (killed && (outlierControl->flags & OUTLIER_VERBOSE_GIVEN))
            fprintf(stderr, "%" PRId64 " additional rows killed by column %s chance limit outlier control\n", killed, column[icol]);
        }
      }
    }

    if (outlierControl->flags & OUTLIER_REPLACEFLAGS && (outlierControl->flags & OUTLIER_INVERT_GIVEN)) {
      for (irow = 0; irow < rows; irow++)
        keep[irow] = !keep[irow];
      kept = rows - kept;
    }

    if (outlierControl->flags & OUTLIER_REPLACELAST) {
      for (irow = 0; irow < rows; irow++) {
        if (keep[irow]) {
          lastGoodValue = data[irow];
          break;
        }
      }
      for (irow = 0; irow < rows; irow++) {
        if (!keep[irow]) {
          keep[irow] = 1;
          data[irow] = lastGoodValue;
        } else
          lastGoodValue = data[irow];
      }
      kept = rows;
      if (!SDDS_SetColumnFromDoubles(dataset, SDDS_SET_BY_NAME, data, rows, column[icol]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (outlierControl->flags & OUTLIER_REPLACENEXT) {
      for (irow = rows - 1; irow >= 0; irow--) {
        if (keep[irow]) {
          lastGoodValue = data[irow];
          break;
        }
      }
      for (irow = rows - 1; irow >= 0; irow--) {
        if (!keep[irow]) {
          data[irow] = lastGoodValue;
          keep[irow] = 1;
        } else
          lastGoodValue = data[irow];
      }
      kept = rows;
      if (!SDDS_SetColumnFromDoubles(dataset, SDDS_SET_BY_NAME, data, rows, column[icol]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (outlierControl->flags & OUTLIER_REPLACEINTERP) {
      irow0 = -1;
      irow1 = -1;
      for (irow = 0; irow < rows; irow++) {
        if (!keep[irow]) {
          if ((irow0 = irow - 1) >= 0) {
            if ((irow1 = irow + 1) < rows) {
              while (irow1 < rows && !keep[irow1])
                irow1++;
            }
            if (irow1 < rows && keep[irow1]) {
              /* irow is bracketed by irow0 and irow1, both of which have good data */
              if (!keep[irow0])
                SDDS_Bomb("Bracketing problem.");
              for (; irow < irow1; irow++)
                data[irow] = data[irow0] + (data[irow1] - data[irow0]) / (1.0 * irow1 - irow0) * (irow - irow0);
              continue;
            } else {
              /* Ran off the end with bad points---just duplicate the last good point */
              for (; irow < rows; irow++)
                data[irow] = data[irow0];
              continue;
            }
          } else {
            /* No good point precedes this point---look for a good point following it */
            for (irow1 = irow + 1; irow1 < rows; irow1++) {
              if (keep[irow1])
                break;
            }
            if (irow1 != rows) {
              for (; irow < irow1; irow++)
                data[irow] = data[irow1];
              continue;
            }
          }
        }
      }
      for (irow = 0; irow < rows; irow++)
        keep[irow] = 1;
      kept = rows;
      if (!SDDS_SetColumnFromDoubles(dataset, SDDS_SET_BY_NAME, data, rows, column[icol]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (outlierControl->flags & OUTLIER_REPLACEVALUE) {
      for (irow = 0; irow < rows; irow++) {
        if (!keep[irow]) {
          data[irow] = outlierControl->replacementValue;
          keep[irow] = 1;
        }
      }
      kept = rows;
      if (!SDDS_SetColumnFromDoubles(dataset, SDDS_SET_BY_NAME, data, rows, column[icol]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    free(data);
  }

  if (outlierControl->flags & OUTLIER_INVERT_GIVEN) {
    for (irow = 0; irow < rows; irow++)
      keep[irow] = !keep[irow];
    kept = rows - kept;
    if (outlierControl->flags & OUTLIER_VERBOSE_GIVEN)
      fprintf(stderr, "%" PRId64 " rows left after inversion\n", kept);
  }

  if (isOutlier && (outlierControl->flags & OUTLIER_MARKONLY)) {
    for (irow = 0; irow < rows; irow++)
      isOutlier[irow] = !keep[irow];
    if (!SDDS_SetRowFlags(dataset, 1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    return rows;
  }

  if (!SDDS_AssertRowFlags(dataset, SDDS_FLAG_ARRAY, keep, rows))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return kept;
}

long meanStDevForFlaggedData(double *mean, double *stDev, double *data, int32_t *keep, int64_t rows) {
  int64_t irow, summed;
  double sum1, sum2, value;

  *mean = *stDev = 0;
  for (irow = sum1 = summed = 0; irow < rows; irow++) {
    if (!keep[irow])
      continue;
    sum1 += data[irow];
    summed++;
  }
  if (summed < 2)
    return 0;
  *mean = sum1 / summed;
  for (irow = sum2 = 0; irow < rows; irow++) {
    if (keep[irow]) {
      value = data[irow] - *mean;
      sum2 += value * value;
    }
  }
  *stDev = sqrt(sum2 / (summed - 1));
  return 1;
}
