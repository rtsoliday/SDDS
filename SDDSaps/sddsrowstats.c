/**
 * @file sddsrowstats.c
 * @brief Computes statistics for rows across multiple columns in SDDS datasets.
 *
 * @details
 * The `sddsrowstats` program processes an SDDS dataset and computes specified statistics
 * for each row across selected columns. New columns with the results are added to the
 * output dataset. It supports a variety of statistical operations such as mean, median,
 * standard deviation, and sum, among others. Users can also specify limits and conditions
 * for data selection.
 *
 * @section Usage
 * ```
 * sddsrowstats [<inputfile>] [<outputfile>]
 *              [-pipe[=input][,output]]
 *              [-nowarnings]
 *              [-mean=<newName>,[,<limitOps>],<columnNameList>]
 *              [-rms=<newName>,[,<limitOps>],<columnNameList>]
 *              [-median=<newName>[,<limitOps>],<columnNameList>]
 *              [-minimum=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>]
 *              [-maximum=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>]
 *              [-standardDeviation=<newName>[,<limitOps>],<columnNameList>]
 *              [-sigma=<newName>[,<limitOps>],<columnNameList>]
 *              [-mad=<newName>[,<limitOps>],<columnNameList>]
 *              [-sum=<newName>[,<limitOps>][,power=<integer>],<columnNameList>] 
 *              [-spread=<newName>[,<limitOps>],<columnNameList>]
 *              [-drange=<newName>[,<limitOps>],<columnNameList>]
 *              [-qrange=<newName>[,<limitOps>],<columnNameList>]
 *              [-smallest=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>]
 *              [-largest=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>]
 *              [-count=<newName>[,<limitOps>],<columnNameList>]
 *              [-percentile=<newName>[,<limitOps>],value=<percent>,<columnNameList]
 *              [-majorOrder=row|column]
 *              [-threads=<number>]
 *
 *              <limitOps> is of the form [topLimit=<value>,][bottomLimit=<value>]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                 |
 * |---------------------------------------|-----------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard input/output for data streams.                                 |
 * | `-nowarnings`                         | Suppress warning messages.                                                  |
 * | `-mean`                               | Compute the mean for specified columns.                                     |
 * | `-rms`                                | Compute the root mean square for specified columns.                         |
 * | `-median`                             | Compute the median for specified columns.                                   |
 * | `-minimum`                            | Compute the minimum and optionally store the column of occurrence. |
 * | `-maximum`                            | Compute the maximum and optionally store the column of occurrence. |
 * | `-standardDeviation`                  | Compute the standard deviation for specified columns.             |
 * | `-sigma`                              | Compute the sigma for specified columns.             |
 * | `-mad`                                | Compute the median absolute deviation for specified columns.             |
 * | `-sum`                                | Compute the sum with optional power for specified columns.        |
 * | `-spread`                             | Compute the maximum value minus the minimum value for specified columns.             |
 * | `-drange`                             | Compute the decile range for specified columns.             |
 * | `-qrange`                             | Compute the quartile range for specified columns.             |
 * | `-smallest`                           | Compute the smallest absolute value for specified columns.             |
 * | `-largest`                            | Compute the largest absolute value for specified columns.             |
 * | `-count`                              | Compute the number of values within the limits for specified columns.             |
 * | `-percentile`                         | Compute the specified percentile value for specified columns.             |
 * | `-majorOrder`                         | Specify row or column major order for the output dataset.                   |
 * | `-threads`                            | Specify the number of threads for parallel computations.                    |
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
 *   M. Borland, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "SDDSutils.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_MAXIMUM,
  SET_MINIMUM,
  SET_MEAN,
  SET_STANDARDDEVIATION,
  SET_RMS,
  SET_SUM,
  SET_SIGMA,
  SET_COUNT,
  SET_PIPE,
  SET_MEDIAN,
  SET_MAD,
  SET_NOWARNINGS,
  SET_DRANGE,
  SET_QRANGE,
  SET_LARGEST,
  SET_SMALLEST,
  SET_SPREAD_ARG,
  SET_PERCENTILE,
  SET_MAJOR_ORDER,
  SET_THREADS,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "maximum",
  "minimum",
  "mean",
  "standarddeviation",
  "rms",
  "sum",
  "sigma",
  "count",
  "pipe",
  "median",
  "mad",
  "nowarnings",
  "drange",
  "qrange",
  "largest",
  "smallest",
  "spread",
  "percentile",
  "majorOrder",
  "threads",
};

#define TOPLIMIT_GIVEN 0x0001U
#define BOTTOMLIMIT_GIVEN 0x0002U
#define POSITIONCOLUMN_GIVEN 0x0004U
#define PERCENT_GIVEN 0x0008U

/* this structure stores a command-line request for statistics computation */
/* individual elements of sourceColumn may contain wildcards */
typedef struct
{
  char **sourceColumn, *resultColumn, *positionColumn;
  long sourceColumns, sumPower, optionCode, positionColumnIndex;
  double percent;
  unsigned long flags;
  double topLimit, bottomLimit;
} STAT_REQUEST;

/* this structure stores data necessary for accessing/creating SDDS columns and
 * for computing a statistic
 */
typedef struct
{
  char **sourceColumn, *resultColumn, *positionColumn;
  long sourceColumns, optionCode, resultIndex, sumPower, positionColumnIndex;
  double percent;
  unsigned long flags;
  double topLimit, bottomLimit;
} STAT_DEFINITION;

long addStatRequests(STAT_REQUEST **statRequest, long requests, char **item, long items, long code, unsigned long flag);
STAT_DEFINITION *compileStatDefinitions(SDDS_DATASET *inTable, STAT_REQUEST *request, long requests, long *stats, long noWarnings);
long setupOutputFile(SDDS_DATASET *outTable, char *output, SDDS_DATASET *inTable, STAT_DEFINITION *stat, long stats, short columnMajorOrder);

static char *USAGE =
  "sddsrowstats [<input>] [<output>]\n"
  "             [-pipe[=input][,output]]\n"
  "             [-nowarnings]\n"
  "             [-mean=<newName>,[,<limitOps>],<columnNameList>]\n"
  "             [-rms=<newName>,[,<limitOps>],<columnNameList>]\n"
  "             [-median=<newName>[,<limitOps>],<columnNameList>]\n"
  "             [-minimum=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>]\n"
  "             [-maximum=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>]\n"
  "             [-standardDeviation=<newName>[,<limitOps>],<columnNameList>]\n"
  "             [-sigma=<newName>[,<limitOps>],<columnNameList>]\n"
  "             [-mad=<newName>[,<limitOps>],<columnNameList>]\n"
  "             [-sum=<newName>[,<limitOps>][,power=<integer>],<columnNameList>] \n"
  "             [-spread=<newName>[,<limitOps>],<columnNameList>]\n"
  "             [-drange=<newName>[,<limitOps>],<columnNameList>]\n"
  "             [-qrange=<newName>[,<limitOps>],<columnNameList>]\n"
  "             [-smallest=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>]\n"
  "             [-largest=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>]\n"
  "             [-count=<newName>[,<limitOps>],<columnNameList>]\n"
  "             [-percentile=<newName>[,<limitOps>],value=<percent>,<columnNameList]\n"
  "             [-majorOrder=row|column]\n"
  "             [-threads=<number>]\n"
  "\nOptions:\n"
  "  -pipe[=input][,output]\n"
  "      Use pipe for input and/or output.\n"
  "  -nowarnings\n"
  "      Suppress warning messages.\n"
  "  -mean=<newName>[,<limitOps>],<columnNameList>\n"
  "      Compute the mean of the specified columns.\n"
  "  -rms=<newName>[,<limitOps>],<columnNameList>\n"
  "      Compute the root mean square of the specified columns.\n"
  "  -median=<newName>[,<limitOps>],<columnNameList>\n"
  "      Compute the median of the specified columns.\n"
  "  -minimum=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>\n"
  "      Compute the minimum value among the specified columns.\n"
  "  -maximum=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>\n"
  "      Compute the maximum value among the specified columns.\n"
  "  -standardDeviation=<newName>[,<limitOps>],<columnNameList>\n"
  "      Compute the standard deviation of the specified columns.\n"
  "  -sigma=<newName>[,<limitOps>],<columnNameList>\n"
  "      Compute the sigma (standard deviation) of the specified columns.\n"
  "  -mad=<newName>[,<limitOps>],<columnNameList>\n"
  "      Compute the median absolute deviation (MAD) of the specified columns.\n"
  "  -sum=<newName>[,<limitOps>][,power=<integer>],<columnNameList>\n"
  "      Compute the sum of the specified columns.\n"
  "  -spread=<newName>[,<limitOps>],<columnNameList>\n"
  "      Compute the spread (max - min) of the specified columns.\n"
  "  -drange=<newName>[,<limitOps>],<columnNameList>\n"
  "      Compute the decile range of the specified columns.\n"
  "  -qrange=<newName>[,<limitOps>],<columnNameList>\n"
  "      Compute the quartile range of the specified columns.\n"
  "  -smallest=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>\n"
  "      Compute the smallest absolute value among the specified columns.\n"
  "  -largest=<newName>[,positionColumn=<name>][,<limitOps>],<columnNameList>\n"
  "      Compute the largest absolute value among the specified columns.\n"
  "  -count=<newName>[,<limitOps>],<columnNameList>\n"
  "      Count the number of valid entries in the specified columns.\n"
  "  -percentile=<newName>[,<limitOps>],value=<percent>,<columnNameList>\n"
  "      Compute the specified percentile of the given columns.\n"
  "  -majorOrder=row|column\n"
  "      Set the data ordering to row-major or column-major.\n"
  "  -threads=<number>\n"
  "      Specify the number of threads to use for computations.\n"
  "\n<limitOps> is of the form [topLimit=<value>,][bottomLimit=<value>]\n"
  "\nComputes statistics for each row of each input table, adding new columns to the\n"
  "output table. Each row statistic is done using data from the columns listed in\n"
  "<columnNameList>, which is a comma-separated list of optionally-wildcarded column\n"
  "names. positionColumn=<name> for minimum, maximum, smallest, largest option is to store \n"
  "the corresponding column name of the minimum, maximum, smallest, or largest in each row to \n"
  "the new output column - <name>.\n"
  "\nProgram by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  STAT_DEFINITION *stat;
  long stats;
  STAT_REQUEST *request;
  long requests;
  SCANNED_ARG *scanned;
  SDDS_DATASET inData, outData;

  int32_t power;
  long i_arg, code, iStat, tmpFileUsed, iColumn, posColIndex;
  int64_t rows, row, count;
  long noWarnings, maxSourceColumns;
  char *input, *output, *positionColumn, **posColumnName;
  double **inputData, *outputData, value1, value2, topLimit, bottomLimit;
  unsigned long pipeFlags, scanFlags, majorOrderFlag;
  char s[100];
  double *statWorkArray;
  double quartilePoint[2] = {25.0, 75.0}, quartileResult[2];
  double decilePoint[2] = {10.0, 90.0}, decileResult[2];
  double percent;
  short columnMajorOrder = -1;
  int threads = 1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 2) {
    bomb("too few arguments", USAGE);
  }

  posColumnName = NULL;
  input = output = positionColumn = NULL;
  stat = NULL;
  request = NULL;
  stats = requests = pipeFlags = 0;
  noWarnings = 0;
  outputData = NULL;
  statWorkArray = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    scanFlags = 0;
    if (scanned[i_arg].arg_type == OPTION) {
      /* process options here */
      switch (code = match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAXIMUM:
      case SET_MINIMUM:
      case SET_MEAN:
      case SET_MEDIAN:
      case SET_STANDARDDEVIATION:
      case SET_RMS:
      case SET_SIGMA:
      case SET_MAD:
      case SET_COUNT:
      case SET_DRANGE:
      case SET_QRANGE:
      case SET_SMALLEST:
      case SET_LARGEST:
      case SET_SPREAD_ARG:
        if (scanned[i_arg].n_items < 3) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        if (!scanItemList(&scanFlags, scanned[i_arg].list, &scanned[i_arg].n_items,
                          SCANITEMLIST_UNKNOWN_VALUE_OK | SCANITEMLIST_REMOVE_USED_ITEMS |
                          SCANITEMLIST_IGNORE_VALUELESS,
                          "positionColumn", SDDS_STRING, &positionColumn, 1, POSITIONCOLUMN_GIVEN,
                          "toplimit", SDDS_DOUBLE, &topLimit, 1, TOPLIMIT_GIVEN,
                          "bottomlimit", SDDS_DOUBLE, &bottomLimit, 1, BOTTOMLIMIT_GIVEN, NULL)) {
          sprintf(s, "invalid -%s syntax", scanned[i_arg].list[0]);
          SDDS_Bomb(s);
        }
        requests = addStatRequests(&request, requests, scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, code, scanFlags);
        request[requests - 1].topLimit = topLimit;
        request[requests - 1].bottomLimit = bottomLimit;
        if (positionColumn) {
          if (code == SET_MAXIMUM || code == SET_MINIMUM || code == SET_LARGEST || code == SET_SMALLEST)
            SDDS_CopyString(&request[requests - 1].positionColumn, positionColumn);
          free(positionColumn);
          positionColumn = NULL;
        }
        break;
      case SET_PERCENTILE:
        if (scanned[i_arg].n_items < 3) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        if (!scanItemList(&scanFlags, scanned[i_arg].list, &scanned[i_arg].n_items,
                          SCANITEMLIST_UNKNOWN_VALUE_OK | SCANITEMLIST_REMOVE_USED_ITEMS |
                          SCANITEMLIST_IGNORE_VALUELESS,
                          "value", SDDS_DOUBLE, &percent, 1, PERCENT_GIVEN,
                          "toplimit", SDDS_DOUBLE, &topLimit, 1, TOPLIMIT_GIVEN,
                          "bottomlimit", SDDS_DOUBLE, &bottomLimit, 1, BOTTOMLIMIT_GIVEN, NULL) ||
            !(scanFlags & PERCENT_GIVEN) || percent <= 0 || percent >= 100)
          SDDS_Bomb("invalid -percentile syntax");
        requests = addStatRequests(&request, requests, scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, code, scanFlags);
        request[requests - 1].percent = percent;
        request[requests - 1].topLimit = topLimit;
        request[requests - 1].bottomLimit = bottomLimit;
        break;
      case SET_SUM:
        if (scanned[i_arg].n_items < 3) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        power = 1;
        if (!scanItemList(&scanFlags, scanned[i_arg].list, &scanned[i_arg].n_items,
                          SCANITEMLIST_UNKNOWN_VALUE_OK | SCANITEMLIST_REMOVE_USED_ITEMS | SCANITEMLIST_IGNORE_VALUELESS,
                          "power", SDDS_LONG, &power, 1, 0,
                          "toplimit", SDDS_DOUBLE, &topLimit, 1, TOPLIMIT_GIVEN,
                          "bottomlimit", SDDS_DOUBLE, &bottomLimit, 1, BOTTOMLIMIT_GIVEN, NULL))
          SDDS_Bomb("invalid -sum syntax");
        requests = addStatRequests(&request, requests, scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, code, scanFlags);
        request[requests - 1].sumPower = power;
        request[requests - 1].topLimit = topLimit;
        request[requests - 1].bottomLimit = bottomLimit;
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_NOWARNINGS:
        noWarnings = 1;
        break;
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[i_arg].n_items--;
        if (scanned[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[i_arg].list + 1, &scanned[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_THREADS:
        if (scanned[i_arg].n_items != 2 ||
            !sscanf(scanned[i_arg].list[1], "%d", &threads) || threads < 1)
          SDDS_Bomb("invalid -threads syntax");
        break;
      default:
        fprintf(stderr, "error: unknown option '%s' given\n", scanned[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      /* argument is filename */
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  processFilenames("sddsrowstats", &input, &output, pipeFlags, noWarnings, &tmpFileUsed);

  if (!requests)
    SDDS_Bomb("no statistics requested");

  if (!SDDS_InitializeInput(&inData, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(stat = compileStatDefinitions(&inData, request, requests, &stats, noWarnings))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (stats < 0)
    SDDS_Bomb("No valid statistics requests.");
  for (iStat = maxSourceColumns = 0; iStat < stats; iStat++) {
    if (stat[iStat].sourceColumns > maxSourceColumns)
      maxSourceColumns = stat[iStat].sourceColumns;
  }
  if (!(statWorkArray = malloc(sizeof(*statWorkArray) * maxSourceColumns)))
    SDDS_Bomb("allocation failure (statWorkArray)");

  if (!setupOutputFile(&outData, output, &inData, stat, stats, columnMajorOrder)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  inputData = NULL;

  while ((code = SDDS_ReadPage(&inData)) > 0) {
    if (!SDDS_CopyPage(&outData, &inData)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if ((rows = SDDS_CountRowsOfInterest(&inData))) {
      if (!(outputData = (double *)malloc(sizeof(*outputData) * rows))) {
        SDDS_Bomb("memory allocation failure");
      }
      if (!(posColumnName = (char **)malloc(sizeof(*posColumnName) * rows))) {
        SDDS_Bomb("memory allocation failure");
      }
      for (iStat = 0; iStat < stats; iStat++) {
        if (!(inputData = (double **)malloc(sizeof(*inputData) * stat[iStat].sourceColumns))) {
          SDDS_Bomb("memory allocation failure");
        }
        for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
          if (!(inputData[iColumn] = SDDS_GetColumnInDoubles(&inData, stat[iStat].sourceColumn[iColumn]))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
        for (row = 0; row < rows; row++)
          outputData[row] = DBL_MAX;
        switch (stat[iStat].optionCode) {
        case SET_MINIMUM:
          for (row = 0; row < rows; row++) {
            value1 = DBL_MAX;
            posColIndex = 0;
            posColumnName[row] = NULL;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              if (inputData[iColumn][row] < value1) {
                value1 = inputData[iColumn][row];
                posColIndex = iColumn;
              }
            }
            outputData[row] = value1;
            if (stat[iStat].positionColumn)
              posColumnName[row] = stat[iStat].sourceColumn[posColIndex];
          }
          break;
        case SET_MAXIMUM:
          for (row = 0; row < rows; row++) {
            posColIndex = 0;
            value1 = -DBL_MAX;
            posColumnName[row] = NULL;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              if (inputData[iColumn][row] > value1) {
                posColIndex = iColumn;
                value1 = inputData[iColumn][row];
              }
            }
            outputData[row] = value1;
            if (stat[iStat].positionColumn)
              posColumnName[row] = stat[iStat].sourceColumn[posColIndex];
          }
          break;
        case SET_MEAN:
          for (row = 0; row < rows; row++) {
            value1 = 0;
            count = 0;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              value1 += inputData[iColumn][row];
              count++;
            }
            if (count)
              outputData[row] = value1 / count;
          }
          break;
        case SET_MEDIAN:
          for (row = 0; row < rows; row++) {
            for (iColumn = count = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              statWorkArray[count] = inputData[iColumn][row];
              count++;
            }
            if (count)
              compute_median(outputData + row, statWorkArray, count);
          }
          break;
        case SET_STANDARDDEVIATION:
          for (row = 0; row < rows; row++) {
            value1 = 0;
            value2 = 0;
            count = 0;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              value1 += inputData[iColumn][row];
              value2 += inputData[iColumn][row] * inputData[iColumn][row];
              count++;
            }
            if (count > 1) {
              if ((value1 = value2 / count - sqr(value1 / count)) <= 0)
                outputData[row] = 0;
              else
                outputData[row] = sqrt(value1 * count / (count - 1.0));
            }
          }
          break;
        case SET_SIGMA:
          for (row = 0; row < rows; row++) {
            value1 = 0;
            value2 = 0;
            count = 0;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              value1 += inputData[iColumn][row];
              value2 += inputData[iColumn][row] * inputData[iColumn][row];
              count++;
            }
            if (count > 1) {
              if ((value1 = value2 / count - sqr(value1 / count)) <= 0)
                outputData[row] = 0;
              else
                outputData[row] = sqrt(value1 / (count - 1.0));
            }
          }
          break;
        case SET_RMS:
          for (row = 0; row < rows; row++) {
            value1 = 0;
            count = 0;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              value1 += sqr(inputData[iColumn][row]);
              count++;
            }
            if (count)
              outputData[row] = sqrt(value1 / count);
          }
          break;
        case SET_SUM:
          for (row = 0; row < rows; row++) {
            value1 = 0;
            count = 0;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              value1 += ipow(inputData[iColumn][row], stat[iStat].sumPower);
              count++;
            }
            if (count)
              outputData[row] = value1;
          }
          break;
        case SET_COUNT:
          for (row = 0; row < rows; row++) {
            count = 0;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              count++;
            }
            outputData[row] = count;
          }
          break;
        case SET_MAD:
          for (row = 0; row < rows; row++) {
            for (iColumn = count = value1 = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              statWorkArray[count] = inputData[iColumn][row];
              count++;
            }
            if (count)
              computeMomentsThreaded(NULL, NULL, NULL, &outputData[row], statWorkArray, count, threads);
          }
          break;
        case SET_DRANGE:
          for (row = 0; row < rows; row++) {
            for (iColumn = count = value1 = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              statWorkArray[count] = inputData[iColumn][row];
              count++;
            }
            if (count && compute_percentiles(decileResult, decilePoint, 2, statWorkArray, count))
              outputData[row] = decileResult[1] - decileResult[0];
          }
          break;
        case SET_QRANGE:
          for (row = 0; row < rows; row++) {
            for (iColumn = count = value1 = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              statWorkArray[count] = inputData[iColumn][row];
              count++;
            }
            if (count && compute_percentiles(quartileResult, quartilePoint, 2, statWorkArray, count))
              outputData[row] = quartileResult[1] - quartileResult[0];
          }
          break;
        case SET_SMALLEST:
          for (row = 0; row < rows; row++) {
            value1 = DBL_MAX;
            posColIndex = 0;
            posColumnName[row] = NULL;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              if ((value2 = fabs(inputData[iColumn][row])) < value1) {
                posColIndex = iColumn;
                value1 = value2;
              }
            }
            outputData[row] = value1;
            if (stat[iStat].positionColumn)
              posColumnName[row] = stat[iStat].sourceColumn[posColIndex];
          }
          break;
        case SET_LARGEST:
          for (row = 0; row < rows; row++) {
            value1 = 0;
            posColIndex = 0;
            posColumnName[row] = NULL;
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              if ((value2 = fabs(inputData[iColumn][row])) > value1) {
                posColIndex = iColumn;
                value1 = value2;
              }
            }
            outputData[row] = value1;
            if (stat[iStat].positionColumn)
              posColumnName[row] = stat[iStat].sourceColumn[posColIndex];
          }
          break;
        case SET_SPREAD_ARG:
          for (row = 0; row < rows; row++) {
            value1 = DBL_MAX;  /* min */
            value2 = -DBL_MAX; /* max */
            for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              if (inputData[iColumn][row] < value1)
                value1 = inputData[iColumn][row];
              if (inputData[iColumn][row] > value2)
                value2 = inputData[iColumn][row];
            }
            outputData[row] = value2 - value1;
          }
          break;
        case SET_PERCENTILE:
          for (row = 0; row < rows; row++) {
            for (iColumn = count = value1 = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
              if (stat[iStat].flags & TOPLIMIT_GIVEN && inputData[iColumn][row] > stat[iStat].topLimit)
                continue;
              if (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputData[iColumn][row] < stat[iStat].bottomLimit)
                continue;
              statWorkArray[count] = inputData[iColumn][row];
              count++;
            }
            outputData[row] = HUGE_VAL;
            if (count)
              compute_percentiles(&outputData[row], &stat[iStat].percent, 1, statWorkArray, count);
          }
          break;
        default:
          SDDS_Bomb("invalid statistic code (accumulation loop)");
          break;
        }
        if (!SDDS_SetColumn(&outData, SDDS_SET_BY_INDEX, outputData, rows, stat[iStat].resultIndex))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

        if (stat[iStat].positionColumn) {
          if (!SDDS_SetColumn(&outData, SDDS_SET_BY_INDEX, posColumnName, rows, stat[iStat].positionColumnIndex))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++)
          free(inputData[iColumn]);
        free(inputData);
        inputData = NULL;
      }
      free(outputData);
      outputData = NULL;
      free(posColumnName);
      posColumnName = NULL;
    }
    if (!SDDS_WritePage(&outData))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  free_scanargs(&scanned, argc);
  for (iStat = 0; iStat < stats; iStat++) {
    if (stat[iStat].positionColumn)
      free(stat[iStat].positionColumn);
    for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++)
      free(stat[iStat].sourceColumn[iColumn]);
    free(stat[iStat].sourceColumn);
  }
  free(request);
  free(stat);
  if (statWorkArray)
    free(statWorkArray);

  if (!SDDS_Terminate(&inData) || !SDDS_Terminate(&outData)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (tmpFileUsed && !replaceFileAndBackUp(input, output))
    exit(EXIT_FAILURE);
  return EXIT_SUCCESS;
}

long addStatRequests(STAT_REQUEST **statRequest, long requests, char **item, long items, long code, unsigned long flags) {
  long i;
  if (!(*statRequest = SDDS_Realloc(*statRequest, sizeof(**statRequest) * (requests + 1))) ||
      !((*statRequest)[requests].sourceColumn = (char **)malloc(sizeof(*(*statRequest)[requests].sourceColumn) * (items - 1))))
    SDDS_Bomb("memory allocation failure");
  for (i = 0; i < items - 1; i++)
    SDDS_CopyString(&((*statRequest)[requests].sourceColumn[i]), item[i + 1]);
  (*statRequest)[requests].resultColumn = item[0];
  (*statRequest)[requests].sourceColumns = items - 1;
  (*statRequest)[requests].optionCode = code;
  (*statRequest)[requests].sumPower = 1;
  (*statRequest)[requests].flags = flags;
  (*statRequest)[requests].positionColumn = NULL;

  return requests + 1;
}

STAT_DEFINITION *compileStatDefinitions(SDDS_DATASET *inData, STAT_REQUEST *request, long requests, long *stats, long noWarnings) {
  STAT_DEFINITION *stat;
  long iReq, iName, iStat;

  if (!(stat = (STAT_DEFINITION *)malloc(sizeof(*stat) * requests)))
    SDDS_Bomb("memory allocation failure");
  for (iReq = iStat = 0; iReq < requests; iReq++) {
    if ((stat[iStat].sourceColumns = expandColumnPairNames(inData, &request[iReq].sourceColumn, NULL, request[iReq].sourceColumns, NULL, 0, FIND_NUMERIC_TYPE, 0)) <= 0) {
      if (!noWarnings) {
        fprintf(stderr, "Warning: no match for column names (sddsrowstats):\n");
        for (iName = 0; iName < request[iReq].sourceColumns; iName++)
          fprintf(stderr, "%s, ", request[iReq].sourceColumn[iName]);
        fputc('\n', stderr);
      }
    } else {
      stat[iStat].sourceColumn = request[iReq].sourceColumn;
      stat[iStat].resultColumn = request[iReq].resultColumn;
      stat[iStat].optionCode = request[iReq].optionCode;
      stat[iStat].sumPower = request[iReq].sumPower;
      stat[iStat].flags = request[iReq].flags;
      stat[iStat].topLimit = request[iReq].topLimit;
      stat[iStat].bottomLimit = request[iReq].bottomLimit;
      stat[iStat].positionColumn = request[iReq].positionColumn;
      stat[iStat].percent = request[iReq].percent;
      iStat++;
    }
    *stats = iStat;
  }

  return stat;
}

long setupOutputFile(SDDS_DATASET *outData, char *output, SDDS_DATASET *inData, STAT_DEFINITION *stat, long stats, short columnMajorOrder) {
  long column;
  char s[SDDS_MAXLINE];

  if (!SDDS_InitializeCopy(outData, inData, output, "w"))
    return 0;
  if (columnMajorOrder != -1)
    outData->layout.data_mode.column_major = columnMajorOrder;
  else
    outData->layout.data_mode.column_major = inData->layout.data_mode.column_major;
  for (column = 0; column < stats; column++) {
    if (!SDDS_TransferColumnDefinition(outData, inData, stat[column].sourceColumn[0], stat[column].resultColumn)) {
      sprintf(s, "Problem transferring definition of column %s to %s\n", stat[column].sourceColumn[0], stat[column].resultColumn);
      SDDS_SetError(s);
      return 0;
    }
    if ((stat[column].resultIndex = SDDS_GetColumnIndex(outData, stat[column].resultColumn)) < 0) {
      sprintf(s, "Problem creating column %s", stat[column].resultColumn);
      SDDS_SetError(s);
      return 0;
    }
    if (stat[column].positionColumn) {
      if (!SDDS_DefineSimpleColumn(outData, stat[column].positionColumn, NULL, SDDS_STRING)) {
        sprintf(s, "Problem define column %s\n", stat[column].positionColumn);
        SDDS_SetError(s);
        return 0;
      }
      if ((stat[column].positionColumnIndex = SDDS_GetColumnIndex(outData, stat[column].positionColumn)) < 0) {
        sprintf(s, "Problem creating column %s", stat[column].positionColumn);
        SDDS_SetError(s);
        return 0;
      }
    }
    if (!SDDS_ChangeColumnInformation(outData, "description", "", SDDS_SET_BY_NAME, stat[column].resultColumn) ||
        !SDDS_ChangeColumnInformation(outData, "symbol", "", SDDS_SET_BY_NAME, stat[column].resultColumn) ||
        !SDDS_ChangeColumnInformation(outData, "type", "double", SDDS_SET_BY_NAME | SDDS_PASS_BY_STRING, stat[column].resultColumn)) {
      sprintf(s, "Problem changing attributes of new column %s", stat[column].resultColumn);
      SDDS_SetError(s);
      return 0;
    }
  }
  if (!SDDS_WriteLayout(outData))
    return 0;
  return 1;
}
