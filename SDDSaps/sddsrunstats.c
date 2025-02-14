/**
 * @file sddsrunstats.c
 * @brief Computes running statistics on SDDS data columns.
 *
 * @details
 * This program calculates various running statistics (e.g., mean, median, minimum, maximum, etc.) 
 * on columns of data from an SDDS (Self-Describing Data Set) file. The program supports multiple
 * options for statistical analysis and allows for processing of sliding window or blocked statistics.
 *
 * @section Usage
 * ```
 * sddsrunstats [<input>] [<output>] 
 *              [-pipe[=input][,output]]
 *              [-points=<integer>]
 *              [-window=column=<column>,width=<value>}]
 *              [-noOverlap]
 *              [-partialOk]
 *              [-mean=[<limitOps>],<columnNameList>]
 *              [-median=[<limitOps>],<columnNameList>]
 *              [-minimum=[<limitOps>],<columnNameList>]
 *              [-maximum=[<limitOps>],<columnNameList>]
 *              [-standardDeviation=[<limitOps>],<columnNameList>]
 *              [-sigma=[<limitOps>],<columnNameList>]
 *              [-sum=[<limitOps>][,power=<integer>],<columnNameList>]
 *              [-sample=[<limitOps>],<columnNameList>]
 *              [-slope=independent=<columnName>,<columnNameList>]
 *              [-majorOrder=row|column]
 *
 *              <limitOps> is of the form [topLimit=<value>,][bottomLimit=<value>]
 * ```
 *
 * @section Options
 * | Option                       | Description                                                                 |
 * |------------------------------|-----------------------------------------------------------------------------|
 * | `-pipe`                      | Use standard input and/or output streams.                                   |
 * | `-points`                    | Number of points for running statistics. Use `0` for entire page statistics.|
 * | `-window`                    | Defines a sliding window based on a column and window width.                |
 * | `-noOverlap`                 | Perform blocked statistics instead of sliding window statistics.            |
 * | `-partialOk`                 | Allow computations even if available rows are fewer than specified points.  |
 * | `-mean`                      | Compute mean of specified columns with optional limits.                     |
 * | `-median`                    | Compute median of specified columns with optional limits.                   |
 * | `-minimum`                   | Compute minimum of specified columns with optional limits.                  |
 * | `-maximum`                   | Compute maximum of specified columns with optional limits.                  |
 * | `-standardDeviation`         | Compute standard deviation with optional limits.                            |
 * | `-sigma`                     | Compute sigma of specified columns with optional limits.                    |
 * | `-sum`                       | Compute sum (optionally raised to a power) with optional limits.            |
 * | `-sample`                    | Sample values from specified columns with optional limits.                  |
 * | `-slope`                     | Compute slope of specified columns with a designated independent column.    |
 * | `-majorOrder`                | Specify the major order for data processing.                                |
 *
 * @subsection Incompatibilities
 * - Only one of the following may be specified:
 *   - `-points`
 *   - `-window`
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
 *   M. Borland,
 *   R. Soliday,
 *   H. Shang
 */


#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "SDDSutils.h"
#include <ctype.h>

/* if statistics are added, they must be added before the SET_POINTS
   item in this list and the following options array
*/
/* Enumeration for option types */
enum option_type {
  SET_MAXIMUM,
  SET_MINIMUM,
  SET_MEAN,
  SET_STANDARDDEVIATION,
  SET_RMS,
  SET_SUM,
  SET_SIGMA,
  SET_SAMPLE,
  SET_MEDIAN,
  SET_SLOPE,
  SET_POINTS,
  SET_NOOVERLAP,
  SET_PIPE,
  SET_WINDOW,
  SET_PARTIALOK,
  SET_MAJOR_ORDER,
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
  "sample",
  "median",
  "slope",
  "points",
  "nooverlap",
  "pipe",
  "window",
  "partialok",
  "majorOrder",
};

#define N_STATS 10
/*  option[0-9] and statSuffix[0-9] arrays have to line up */
char *statSuffix[N_STATS] = {
  "Max",
  "Min",
  "Mean",
  "StDev",
  "RMS",
  "Sum",
  "Sigma",
  "",
  "Median",
  "Slope"
};

#define TOPLIMIT_GIVEN 0x0001U
#define BOTTOMLIMIT_GIVEN 0x0002U
#define INDEPENDENT_GIVEN 0x0004U

/* this structure stores a command-line request for statistics computation */
/* individual elements of sourceColumn may contain wildcards */
typedef struct
{
  char **sourceColumn, *independentColumn;
  long sourceColumns, sumPower, optionCode;
  unsigned long flags;
  double topLimit, bottomLimit;
} STAT_REQUEST;

/* this structure stores data necessary for accessing/creating SDDS columns and
 * for computing a statistic
 */
typedef struct
{
  char **sourceColumn, **resultColumn, *independentColumn;
  long sourceColumns, optionCode, *resultIndex, sumPower;
  unsigned long flags;
  double topLimit, bottomLimit;
} STAT_DEFINITION;

long addStatRequests(STAT_REQUEST **statRequest, long requests, char **item, long items, long code, unsigned long flag);
STAT_DEFINITION *compileStatDefinitions(SDDS_DATASET *inTable, STAT_REQUEST *request, long requests);
long setupOutputFile(SDDS_DATASET *outTable, char *output, SDDS_DATASET *inTable, STAT_DEFINITION *stat, long stats, short columnMajorOrder);

static char *USAGE =
  "sddsrunstats [<input>] [<output>] [-pipe[=input][,output]]\n"
  "  [{-points=<integer> | -window=column=<column>,width=<value>}]\n"
  "  [-noOverlap]\n"
  "  [-partialOk]\n"
  "  [-mean=[<limitOps>],<columnNameList>]\n"
  "  [-median=[<limitOps>],<columnNameList>]\n"
  "  [-minimum=[<limitOps>],<columnNameList>]\n"
  "  [-maximum=[<limitOps>],<columnNameList>]\n"
  "  [-standardDeviation=[<limitOps>],<columnNameList>]\n"
  "  [-sigma=[<limitOps>],<columnNameList>]\n"
  "  [-sum=[<limitOps>][,power=<integer>],<columnNameList>]\n"
  "  [-sample=[<limitOps>],<columnNameList>]\n"
  "  [-slope=independent=<columnName>,<columnNameList>]\n"
  "\n"
  "  <limitOps> is of the form [topLimit=<value>,][bottomLimit=<value>] [-majorOrder=row|column]\n\n"
  "Computes running statistics of columns of data. The <columnNameList> may contain\n"
  "wildcards, in which case an additional output column is produced for every matching\n"
  "column. By default, statistics are done with a sliding window, so the values are\n"
  "running statistics; for blocked statistics, use -noOverlap. For statistics on\n"
  "the entire page, use -points=0.\n"
  "The -partialOk option tells sddsrunstats to do computations even\n"
  "if the number of available rows is less than the number of points\n"
  "specified; by default, such data is simply ignored.\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  STAT_DEFINITION *stat;
  long stats;
  STAT_REQUEST *request;
  long requests;
  int64_t count;
  SCANNED_ARG *scanned;
  SDDS_DATASET inData, outData;
  char *independent;
  int32_t power;
  int64_t pointsToStat;
  long pointsToStat0, overlap;
  int64_t rows, outputRowsMax, outputRows, outputRow;
  long iArg, code, iStat, iColumn;
  int64_t startRow, rowOffset;
  char *input, *output, *windowColumn;
  double *inputData, *outputData, topLimit, bottomLimit, *inputDataOffset, result, sum1, sum2, *newData;
  double windowWidth, *windowData, slope, intercept, variance;
  long windowIndex;
  unsigned long pipeFlags, scanFlags, majorOrderFlag;
  char s[100];
  long lastRegion, region, windowRef, partialOk;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 2) {
    bomb("too few arguments", USAGE);
  }
  newData = NULL;
  result = 0;
  input = output = NULL;
  stat = NULL;
  request = NULL;
  stats = requests = pipeFlags = 0;
  pointsToStat = -1;
  partialOk = 0;
  overlap = 1;
  windowColumn = NULL;
  windowData = NULL;

  for (iArg = 1; iArg < argc; iArg++) {
    scanFlags = 0;
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (code = match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
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
      case SET_POINTS:
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%" SCNd64, &pointsToStat) != 1 ||
            (pointsToStat <= 2 && pointsToStat != 0))
          SDDS_Bomb("invalid -points syntax");
        break;
      case SET_NOOVERLAP:
        overlap = 0;
        break;
      case SET_MAXIMUM:
      case SET_MINIMUM:
      case SET_MEAN:
      case SET_STANDARDDEVIATION:
      case SET_RMS:
      case SET_SIGMA:
      case SET_SAMPLE:
      case SET_MEDIAN:
        if (scanned[iArg].n_items < 2) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        if (!scanItemList(&scanFlags, scanned[iArg].list, &scanned[iArg].n_items,
                          SCANITEMLIST_UNKNOWN_VALUE_OK | SCANITEMLIST_REMOVE_USED_ITEMS | SCANITEMLIST_IGNORE_VALUELESS,
                          "toplimit", SDDS_DOUBLE, &topLimit, 1, TOPLIMIT_GIVEN,
                          "bottomlimit", SDDS_DOUBLE, &bottomLimit, 1, BOTTOMLIMIT_GIVEN, NULL)) {
          sprintf(s, "invalid -%s syntax", scanned[iArg].list[0]);
          SDDS_Bomb(s);
        }
        requests = addStatRequests(&request, requests, scanned[iArg].list + 1, scanned[iArg].n_items - 1, code, scanFlags);
        request[requests - 1].topLimit = topLimit;
        request[requests - 1].bottomLimit = bottomLimit;
        break;
      case SET_SUM:
        if (scanned[iArg].n_items < 2) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        power = 1;
        if (!scanItemList(&scanFlags, scanned[iArg].list, &scanned[iArg].n_items,
                          SCANITEMLIST_UNKNOWN_VALUE_OK | SCANITEMLIST_REMOVE_USED_ITEMS | SCANITEMLIST_IGNORE_VALUELESS,
                          "power", SDDS_LONG, &power, 1, 0,
                          "toplimit", SDDS_DOUBLE, &topLimit, 1, TOPLIMIT_GIVEN,
                          "bottomlimit", SDDS_DOUBLE, &bottomLimit, 1, BOTTOMLIMIT_GIVEN, NULL))
          SDDS_Bomb("invalid -sum syntax");
        requests = addStatRequests(&request, requests, scanned[iArg].list + 1, scanned[iArg].n_items - 1, code, scanFlags);
        request[requests - 1].sumPower = power;
        request[requests - 1].topLimit = topLimit;
        request[requests - 1].bottomLimit = bottomLimit;
        break;
      case SET_SLOPE:
        if (scanned[iArg].n_items < 2) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        if (!scanItemList(&scanFlags, scanned[iArg].list, &scanned[iArg].n_items,
                          SCANITEMLIST_UNKNOWN_VALUE_OK | SCANITEMLIST_REMOVE_USED_ITEMS | SCANITEMLIST_IGNORE_VALUELESS,
                          "independent", SDDS_STRING, &independent, 1, INDEPENDENT_GIVEN,
                          NULL) ||
            !(scanFlags & INDEPENDENT_GIVEN))
          SDDS_Bomb("invalid -slope syntax");
        requests = addStatRequests(&request, requests, scanned[iArg].list + 1, scanned[iArg].n_items - 1, code, scanFlags);
        request[requests - 1].independentColumn = independent;
        request[requests - 1].topLimit = request[requests - 1].bottomLimit = 0;
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_WINDOW:
        windowWidth = -1;
        windowColumn = NULL;
        scanned[iArg].n_items -= 1;
        if (!scanItemList(&scanFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "column", SDDS_STRING, &windowColumn, 1, 0,
                          "width", SDDS_DOUBLE, &windowWidth, 1, 0, NULL) ||
            !windowColumn ||
            !strlen(windowColumn) ||
            windowWidth <= 0)
          SDDS_Bomb("invalid -window syntax/values");
        break;
      case SET_PARTIALOK:
        partialOk = 1;
        break;
      default:
        fprintf(stderr, "error: unknown option '%s' given\n", scanned[iArg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      /* argument is filename */
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  if (pointsToStat < 0 && !windowColumn) {
    pointsToStat = 10;
  }
  processFilenames("sddsrunstats", &input, &output, pipeFlags, 0, NULL);

  if (!requests)
    SDDS_Bomb("no statistics requested");

  if (!SDDS_InitializeInput(&inData, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(stat = compileStatDefinitions(&inData, request, requests)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  stats = requests;
#ifdef DEBUG
  fprintf(stderr, "%ld stats\n", stats);
  for (iStat = 0; iStat < stats; iStat++) {
    for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
      fprintf(stderr, "iStat=%ld  iColumn=%ld  source=%s  result=%s\n", iStat, iColumn, stat[iStat].sourceColumn[iColumn], stat[iStat].resultColumn[iColumn]);
      if (stat[iStat].flags & BOTTOMLIMIT_GIVEN)
        fprintf(stderr, "  bottom = %e\n", stat[iStat].bottomLimit);
      if (stat[iStat].flags & TOPLIMIT_GIVEN)
        fprintf(stderr, "  top = %e\n", stat[iStat].topLimit);
    }
  }
#endif

  if (!setupOutputFile(&outData, output, &inData, stat, stats, columnMajorOrder))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (windowColumn) {
    if ((windowIndex = SDDS_GetColumnIndex(&inData, windowColumn)) < 0)
      SDDS_Bomb("Window column not present");
    if (!SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&inData, windowIndex)))
      SDDS_Bomb("Window column is not numeric");
  }

  outputData = NULL;
  outputRowsMax = 0;
  pointsToStat0 = pointsToStat;
  while ((code = SDDS_ReadPage(&inData)) > 0) {
    rows = SDDS_CountRowsOfInterest(&inData);
    pointsToStat = pointsToStat0;
    if (pointsToStat == 0)
      pointsToStat = rows;
    if (windowColumn && !(windowData = SDDS_GetColumnInDoubles(&inData, windowColumn))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!windowColumn) {
      if (rows < pointsToStat) {
        if (partialOk)
          pointsToStat = rows;
        else
          continue;
      }
      if (overlap) {
        outputRows = rows - pointsToStat + 1;
      } else
        outputRows = rows / pointsToStat;
    } else
      outputRows = rows;

    if (!SDDS_StartPage(&outData, outputRows) ||
        !SDDS_CopyParameters(&outData, &inData) ||
        !SDDS_CopyArrays(&outData, &inData))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (outputRows > outputRowsMax &&
        !(outputData = SDDS_Realloc(outputData, sizeof(*outputData) * (outputRowsMax = outputRows))))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (iStat = 0; iStat < stats; iStat++) {
      for (iColumn = 0; iColumn < stat[iStat].sourceColumns; iColumn++) {
        double *indepData;
        lastRegion = 0;
        windowRef = 0;
        indepData = NULL;
        if (!(inputData = SDDS_GetColumnInDoubles(&inData, stat[iStat].sourceColumn[iColumn])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (stat[iStat].independentColumn &&
            !(indepData = SDDS_GetColumnInDoubles(&inData, stat[iStat].independentColumn)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (outputRow = startRow = 0; outputRow < outputRows; outputRow++, startRow += (overlap ? 1 : pointsToStat)) {
          if (windowColumn) {
            short windowFound = 0;
            if (overlap) {
              windowRef += 1;
              lastRegion = 0;
            }
            for (pointsToStat = 1; pointsToStat < outputRows - startRow; pointsToStat++) {
              region = (windowData[startRow + pointsToStat] - windowData[windowRef]) / windowWidth;
              if (region != lastRegion) {
                lastRegion = region;
                windowFound = 1;
                break;
              }
            }
            if (!windowFound && pointsToStat < 2)
              break;
            if (startRow + pointsToStat > rows) {
              pointsToStat = rows - startRow - 1;
              if (pointsToStat <= 0)
                break;
            }
#ifdef DEBUG
            fprintf(stderr, "row=%" PRId64 " pointsToStat=%" PRId64 "  delta=%.9lf (%.9lf -> %.9lf)\n", startRow, pointsToStat, windowData[startRow + pointsToStat - 1] - windowData[startRow], windowData[startRow], windowData[startRow + pointsToStat - 1]);
#endif
          }
          inputDataOffset = inputData + startRow;
          switch (stat[iStat].optionCode) {
          case SET_MAXIMUM:
            result = -DBL_MAX;
            for (rowOffset = 0; rowOffset < pointsToStat; rowOffset++) {
              if ((stat[iStat].flags & TOPLIMIT_GIVEN && inputDataOffset[rowOffset] > stat[iStat].topLimit) ||
                  (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputDataOffset[rowOffset] < stat[iStat].bottomLimit))
                continue;
              if (inputDataOffset[rowOffset] > result)
                result = inputDataOffset[rowOffset];
            }
            break;
          case SET_MINIMUM:
            result = DBL_MAX;
            for (rowOffset = 0; rowOffset < pointsToStat; rowOffset++) {
              if ((stat[iStat].flags & TOPLIMIT_GIVEN && inputDataOffset[rowOffset] > stat[iStat].topLimit) ||
                  (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputDataOffset[rowOffset] < stat[iStat].bottomLimit))
                continue;
              if (inputDataOffset[rowOffset] < result)
                result = inputDataOffset[rowOffset];
            }
            break;
          case SET_MEAN:
            result = 0;
            count = 0;
            for (rowOffset = 0; rowOffset < pointsToStat; rowOffset++) {
              if ((stat[iStat].flags & TOPLIMIT_GIVEN && inputDataOffset[rowOffset] > stat[iStat].topLimit) ||
                  (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputDataOffset[rowOffset] < stat[iStat].bottomLimit))
                continue;
              result += inputDataOffset[rowOffset];
              count++;
            }
            if (count)
              result /= count;
            else
              result = DBL_MAX;
            break;
          case SET_MEDIAN:
            result = 0;
            count = 0;
            for (rowOffset = 0; rowOffset < pointsToStat; rowOffset++) {
              if ((stat[iStat].flags & TOPLIMIT_GIVEN && inputDataOffset[rowOffset] > stat[iStat].topLimit) ||
                  (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputDataOffset[rowOffset] < stat[iStat].bottomLimit))
                continue;
              newData = SDDS_Realloc(newData, sizeof(*newData) * (count + 1));
              newData[count] = inputDataOffset[rowOffset];
              count++;
            }
            if (count) {
              if (!compute_median(&result, newData, count))
                result = DBL_MAX;
              free(newData);
              newData = NULL;
            } else
              result = DBL_MAX;
            break;
          case SET_STANDARDDEVIATION:
          case SET_SIGMA:
            sum1 = sum2 = count = 0;
            for (rowOffset = 0; rowOffset < pointsToStat; rowOffset++) {
              if ((stat[iStat].flags & TOPLIMIT_GIVEN && inputDataOffset[rowOffset] > stat[iStat].topLimit) ||
                  (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputDataOffset[rowOffset] < stat[iStat].bottomLimit))
                continue;
              sum1 += inputDataOffset[rowOffset];
              sum2 += inputDataOffset[rowOffset] * inputDataOffset[rowOffset];
              count++;
            }
            if (count > 1) {
              if ((result = sum2 / count - sqr(sum1 / count)) <= 0)
                result = 0;
              else
                result = sqrt(result * count / (count - 1.0));
              if (stat[iStat].optionCode == SET_SIGMA)
                result /= sqrt(count);
            }
            break;
          case SET_RMS:
            sum2 = count = 0;
            for (rowOffset = 0; rowOffset < pointsToStat; rowOffset++) {
              if ((stat[iStat].flags & TOPLIMIT_GIVEN && inputDataOffset[rowOffset] > stat[iStat].topLimit) ||
                  (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputDataOffset[rowOffset] < stat[iStat].bottomLimit))
                continue;
              sum2 += inputDataOffset[rowOffset] * inputDataOffset[rowOffset];
              count++;
            }
            if (count > 0)
              result = sqrt(sum2 / count);
            else
              result = DBL_MAX;
            break;
          case SET_SUM:
            sum1 = count = 0;
            for (rowOffset = 0; rowOffset < pointsToStat; rowOffset++) {
              if ((stat[iStat].flags & TOPLIMIT_GIVEN && inputDataOffset[rowOffset] > stat[iStat].topLimit) ||
                  (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputDataOffset[rowOffset] < stat[iStat].bottomLimit))
                continue;
              sum1 += ipow(inputDataOffset[rowOffset], stat[iStat].sumPower);
              count++;
            }
            if (count > 0)
              result = sum1;
            else
              result = DBL_MAX;
            break;
          case SET_SLOPE:
            if (!unweightedLinearFit(indepData + startRow, inputDataOffset, pointsToStat, &slope, &intercept,
                                     &variance)) {
              result = DBL_MAX;
            } else
              result = slope;
            break;
          case SET_SAMPLE:
            result = DBL_MAX;
            for (rowOffset = 0; rowOffset < pointsToStat; rowOffset++) {
              if ((stat[iStat].flags & TOPLIMIT_GIVEN && inputDataOffset[rowOffset] > stat[iStat].topLimit) ||
                  (stat[iStat].flags & BOTTOMLIMIT_GIVEN && inputDataOffset[rowOffset] < stat[iStat].bottomLimit))
                continue;
              result = inputDataOffset[rowOffset];
              break;
            }
            break;
          default:
            fprintf(stderr, "Unknown statistics code %ld in sddsrunave\n", stat[iStat].optionCode);
            exit(EXIT_FAILURE);
            break;
          }
          outputData[outputRow] = result;
        }
        if (!SDDS_SetColumnFromDoubles(&outData, SDDS_SET_BY_INDEX, outputData, outputRow, stat[iStat].resultIndex[iColumn]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        free(inputData);
      }
    }
    if (windowColumn)
      free(windowData);
    if (!SDDS_WritePage(&outData))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (SDDS_Terminate(&inData) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }
  if (SDDS_Terminate(&outData) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }
  if (outputData)
    free(outputData);
  return EXIT_SUCCESS;
}

long addStatRequests(STAT_REQUEST **statRequest, long requests, char **item, long items, long code, unsigned long flags) {
  long i;
  if (!(*statRequest = SDDS_Realloc(*statRequest, sizeof(**statRequest) * (requests + 1))) ||
      !((*statRequest)[requests].sourceColumn = (char **)malloc(sizeof(*(*statRequest)[requests].sourceColumn) * items)))
    SDDS_Bomb("memory allocation failure");
  for (i = 0; i < items; i++) {
    (*statRequest)[requests].sourceColumn[i] = item[i];
  }
  (*statRequest)[requests].sourceColumns = items;
  (*statRequest)[requests].optionCode = code;
  (*statRequest)[requests].sumPower = 1;
  (*statRequest)[requests].flags = flags;
  (*statRequest)[requests].independentColumn = NULL;
  return requests + 1;
}

STAT_DEFINITION *compileStatDefinitions(SDDS_DATASET *inData, STAT_REQUEST *request, long requests) {
  STAT_DEFINITION *stat;
  long iReq, iName;
  char s[SDDS_MAXLINE];

  if (!(stat = (STAT_DEFINITION *)malloc(sizeof(*stat) * requests)))
    SDDS_Bomb("memory allocation failure");
  for (iReq = 0; iReq < requests; iReq++) {
    if ((stat[iReq].sourceColumns = expandColumnPairNames(inData, &request[iReq].sourceColumn, NULL, request[iReq].sourceColumns, NULL, 0, FIND_NUMERIC_TYPE, 0)) <= 0) {
      fprintf(stderr, "Error: no match for column names (sddsrunstats):\n");
      for (iName = 0; iName < request[iReq].sourceColumns; iName++)
        fprintf(stderr, "%s, ", request[iReq].sourceColumn[iReq]);
      fputc('\n', stderr);
      exit(EXIT_FAILURE);
    }
    stat[iReq].sourceColumn = request[iReq].sourceColumn;
    if (!(stat[iReq].resultColumn = malloc(sizeof(*stat[iReq].resultColumn) * stat[iReq].sourceColumns)) ||
        !(stat[iReq].resultIndex = malloc(sizeof(*stat[iReq].resultIndex) * stat[iReq].sourceColumns))) {
      SDDS_Bomb("memory allocation failure");
    }
    for (iName = 0; iName < stat[iReq].sourceColumns; iName++) {
      sprintf(s, "%s%s", stat[iReq].sourceColumn[iName], statSuffix[request[iReq].optionCode]);
      SDDS_CopyString(stat[iReq].resultColumn + iName, s);
    }
    stat[iReq].optionCode = request[iReq].optionCode;
    stat[iReq].sumPower = request[iReq].sumPower;
    stat[iReq].flags = request[iReq].flags;
    stat[iReq].topLimit = request[iReq].topLimit;
    stat[iReq].bottomLimit = request[iReq].bottomLimit;
    stat[iReq].independentColumn = request[iReq].independentColumn;
  }

  return stat;
}

long setupOutputFile(SDDS_DATASET *outData, char *output, SDDS_DATASET *inData, STAT_DEFINITION *stat, long stats, short columnMajorOrder) {
  long column, iStat;
  char s[SDDS_MAXLINE];

  if (!SDDS_InitializeOutput(outData, SDDS_BINARY, 1, NULL, NULL, output))
    return 0;
  if (columnMajorOrder != -1)
    outData->layout.data_mode.column_major = columnMajorOrder;
  else
    outData->layout.data_mode.column_major = inData->layout.data_mode.column_major;
  for (iStat = 0; iStat < stats; iStat++) {
    for (column = 0; column < stat[iStat].sourceColumns; column++) {
      if (!SDDS_TransferColumnDefinition(outData, inData, stat[iStat].sourceColumn[column], stat[iStat].resultColumn[column])) {
        sprintf(s, "Problem transferring definition of column %s to %s\n", stat[iStat].sourceColumn[column], stat[iStat].resultColumn[column]);
        SDDS_SetError(s);
        return 0;
      }
      if ((stat[iStat].resultIndex[column] = SDDS_GetColumnIndex(outData, stat[iStat].resultColumn[column])) < 0) {
        sprintf(s, "Problem creating column %s", stat[iStat].resultColumn[column]);
        SDDS_SetError(s);
        return 0;
      }
      if (!SDDS_ChangeColumnInformation(outData, "description", NULL, SDDS_SET_BY_NAME, stat[iStat].resultColumn[column]) ||
          !SDDS_ChangeColumnInformation(outData, "symbol", NULL, SDDS_SET_BY_NAME, stat[iStat].resultColumn[column]) ||
          !SDDS_ChangeColumnInformation(outData, "type", "double", SDDS_SET_BY_NAME | SDDS_PASS_BY_STRING, stat[iStat].resultColumn[column])) {
        sprintf(s, "Problem changing attributes of new column %s", stat[iStat].resultColumn[column]);
        SDDS_SetError(s);
        return 0;
      }
    }
  }
  if (!SDDS_TransferAllParameterDefinitions(outData, inData, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_TransferAllArrayDefinitions(outData, inData, 0) ||
      !SDDS_WriteLayout(outData))
    return 0;
  return 1;
}
