/**
 * @file sddsshiftcor.c
 * @brief Performs correlation analysis on shifted data columns in SDDS files.
 *
 * @details
 * This program reads data from an SDDS file, calculates the correlation coefficient
 * between a specified column and other selected numeric columns, with a shifting mechanism.
 * It outputs results in a new SDDS file including the computed correlation values.
 *
 * @section Usage
 * ```
 * sddsshiftcor [<inputfile>] [<outputfile>]
 *              [-pipe=[input][,output]] 
 *               -with=<name> 
 *              [-scan=start=<startShift>,end=<endShift>,delta=<deltaShift>] 
 *              [-columns=<list-of-names>] 
 *              [-excludeColumns=<list-of-names>] 
 *              [-rankOrder] 
 *              [-stDevOutlier[=limit=<factor>][,passes=<integer>]] 
 *              [-verbose] 
 *              [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-with`                               | Specifies the column to correlate with.                                               |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Enables piping for input and/or output files.                                         |
 * | `-scan`                               | Configures the range and step of shifts.                                       |
 * | `-columns`                            | Specifies the columns to process.                                                    |
 * | `-excludeColumns`                     | Excludes specific columns from processing.                                           |
 * | `-rankOrder`                          | Uses rank-order correlation instead of linear correlation.                            |
 * | `-stDevOutlier`                       | Marks and excludes outliers based on standard deviation.                  |
 * | `-verbose`                            | Enables verbose output for detailed progress information.                             |
 * | `-majorOrder`                         | Sets the major data order for output (row or column major).                           |
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author 
 * M. Borland, R. Soliday, H. Shang
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
  SET_WITH,
  SET_PIPE,
  SET_RANKORDER,
  SET_STDEVOUTLIER,
  SET_SCAN,
  SET_VERBOSE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "columns",
  "excludecolumns",
  "with",
  "pipe",
  "rankorder",
  "stdevoutlier",
  "scan",
  "verbose",
  "majorOrder",
};

#define USAGE \
    "sddsshiftcor [-pipe=[input][,output]] [<inputfile>] [<outputfile>] -with=<name>\n" \
    "  [-scan=start=<startShift>,end=<endShift>,delta=<deltaShift>]\n" \
    "  [-columns=<list-of-names>] [-excludeColumns=<list-of-names>]\n" \
    "  [-rankOrder] [-stDevOutlier[=limit=<factor>][,passes=<integer>]]\n" \
    "  [-verbose] [-majorOrder=row|column]\n\n" \
    "Program by Michael Borland. (\"" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n"

void replaceWithRank(double *data, long n);
double *findRank(double *data, int64_t n);
void markStDevOutliers(double *data, double limit, long passes, short *keep, int64_t n);
void setupOutputFile(SDDS_DATASET *SDDSout, char *output, char **column, long columns, short columnMajorOrder);

int main(int argc, char **argv) {
  int iArg;
  char **column, **excludeColumn, *withOnly;
  long columns, excludeColumns;
  char *input, *output, buffer[16384];
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  long i, count, readCode, rankOrder;
  int64_t rows;
  int32_t outlierStDevPasses;
  double **data, correlation, outlierStDevLimit;
  double **rank;
  short **accept;
  unsigned long pipeFlags, dummyFlags, majorOrderFlag;
  int32_t startShift, endShift, deltaShift;
  long outputRows, iWith, outputRow, shiftAmount, verbose;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);

  output = input = withOnly = NULL;
  columns = excludeColumns = 0;
  column = excludeColumn = NULL;
  pipeFlags = 0;
  rankOrder = 0;
  outlierStDevPasses = 0;
  outlierStDevLimit = 1.0;
  rank = NULL;
  accept = NULL;
  startShift = -(endShift = 10);
  deltaShift = 1;
  verbose = 0;

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
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -excludeColumns syntax");
        moveToStringArray(&excludeColumn, &excludeColumns, scanned[iArg].list + 1, scanned[iArg].n_items - 1);
        break;
      case SET_WITH:
        if (withOnly)
          SDDS_Bomb("only one -withOnly option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -withOnly syntax");
        withOnly = scanned[iArg].list[1];
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_RANKORDER:
        rankOrder = 1;
        break;
      case SET_STDEVOUTLIER:
        scanned[iArg].n_items--;
        outlierStDevPasses = 1;
        outlierStDevLimit = 1.0;
        if (!scanItemList(&dummyFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "limit", SDDS_DOUBLE, &outlierStDevLimit, 1, 0,
                          "passes", SDDS_LONG, &outlierStDevPasses, 1, 0, NULL) ||
            outlierStDevPasses <= 0 || outlierStDevLimit <= 0.0)
          SDDS_Bomb("invalid -stdevOutlier syntax/values");
        break;
      case SET_SCAN:
        scanned[iArg].n_items--;
        if (!scanItemList(&dummyFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "start", SDDS_LONG, &startShift, 1, 0,
                          "end", SDDS_LONG, &endShift, 1, 0,
                          "delta", SDDS_LONG, &deltaShift, 1, 0, NULL) ||
            startShift >= endShift || deltaShift <= 0 || (endShift - startShift) < deltaShift)
          SDDS_Bomb("invalid -scan syntax/values");
        break;
      case SET_VERBOSE:
        verbose = 1;
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s\n", scanned[iArg].list[0]);
        exit(1);
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

  processFilenames("sddsshiftcor", &input, &output, pipeFlags, 0, NULL);

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!columns)
    columns = appendToStringArray(&column, columns, "*");
  if (withOnly)
    columns = appendToStringArray(&column, columns, withOnly);

  if ((columns = expandColumnPairNames(&SDDSin, &column, NULL, columns, excludeColumn, excludeColumns, FIND_NUMERIC_TYPE, 0)) <= 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_Bomb("no columns selected for correlation analysis");
  }
  if (columnMajorOrder == -1)
    columnMajorOrder = SDDSin.layout.data_mode.column_major;

  setupOutputFile(&SDDSout, output, column, columns, columnMajorOrder);

  if (!(data = (double **)malloc(sizeof(*data) * columns)) ||
      (rankOrder && !(rank = (double **)malloc(sizeof(*rank) * columns))) ||
      !(accept = (short **)malloc(sizeof(*accept) * columns)))
    SDDS_Bomb("allocation failure");
  outputRows = (endShift - startShift) / deltaShift;
  iWith = -1;
  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&SDDSin)) < (endShift - startShift))
      continue;
    if (!SDDS_StartPage(&SDDSout, outputRows + 1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < columns; i++) {
      if (strcmp(withOnly, column[i]) == 0)
        iWith = i;
      if (!(data[i] = SDDS_GetColumnInDoubles(&SDDSin, column[i])))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (rankOrder)
        rank[i] = findRank(data[i], rows);
      if (outlierStDevPasses) {
        if (!(accept[i] = (short *)malloc(sizeof(**accept) * rows)))
          SDDS_Bomb("allocation failure");
        markStDevOutliers(data[i], outlierStDevLimit, outlierStDevPasses, accept[i], rows);
      } else
        accept[i] = NULL;
    }
    if (iWith < 0)
      SDDS_Bomb("-with column not found");
    outputRow = 0;
    for (shiftAmount = startShift; shiftAmount <= endShift; shiftAmount += deltaShift) {
      if (verbose)
        fprintf(stderr, "Working on shift of %ld\n", shiftAmount);
      if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, outputRow, "ShiftAmount", shiftAmount, NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i = 0; i < columns; i++) {
        correlation = shiftedLinearCorrelationCoefficient(
                                                          rankOrder ? rank[i] : data[i],
                                                          rankOrder ? rank[iWith] : data[iWith],
                                                          accept[i],
                                                          accept[iWith],
                                                          rows,
                                                          &count,
                                                          shiftAmount);
        sprintf(buffer, "%sShiftedCor", column[i]);
        if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, outputRow, buffer, correlation, NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      outputRow++;
    }
    for (i = 0; i < columns; i++) {
      free(data[i]);
      if (rankOrder)
        free(rank[i]);
      if (accept[i])
        free(accept[i]);
    }
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (SDDS_Terminate(&SDDSin) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if (SDDS_Terminate(&SDDSout) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }

  return 0;
}

void setupOutputFile(SDDS_DATASET *SDDSout, char *output, char **column, long columns, short columnMajorOrder) {
  long i;
  char buffer[16384];
  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, NULL, output) ||
      !SDDS_DefineSimpleColumn(SDDSout, "ShiftAmount", NULL, SDDS_LONG))
    SDDS_Bomb("unable to open output file");
  SDDSout->layout.data_mode.column_major = columnMajorOrder;

  for (i = 0; i < columns; i++) {
    sprintf(buffer, "%sShiftedCor", column[i]);
    if (!SDDS_DefineSimpleColumn(SDDSout, buffer, NULL, SDDS_DOUBLE))
      SDDS_Bomb("unable to set up column definitions");
  }
  if (!SDDS_WriteLayout(SDDSout))
    SDDS_Bomb("unable to set up output file");
}

void markStDevOutliers(double *data, double limit, long passes, short *keep, int64_t n) {
  double sum1, sum2, variance, mean, absLimit;
  long pass;
  int64_t i, summed, kept;

  for (i = 0; i < n; i++)
    keep[i] = 1;
  kept = n;
  for (pass = 0; pass < passes && kept; pass++) {
    sum1 = sum2 = 0.0;
    summed = 0;
    for (i = 0; i < n; i++) {
      if (keep[i]) {
        summed++;
        sum1 += data[i];
        sum2 += data[i] * data[i];
      }
    }
    if (summed < 2)
      return;
    mean = sum1 / summed;
    sum2 /= summed;
    if ((variance = sum2 - mean * mean) > 0.0) {
      absLimit = limit * sqrt(variance);
      for (i = 0; i < n; i++) {
        if (keep[i] && fabs(data[i] - mean) > absLimit) {
          keep[i] = 0;
          kept--;
        }
      }
    }
  }
}

typedef struct {
  double data;
  long originalIndex;
} DATAnINDEX;

int compareData(const void *d1, const void *d2) {
  double diff;
  diff = ((DATAnINDEX *)d1)->data - ((DATAnINDEX *)d2)->data;
  return (diff == 0.0) ? 0 : (diff < 0.0 ? -1 : 1);
}

double *findRank(double *data, int64_t n) {
  int64_t i;
  double *rank;

  rank = (double *)malloc(sizeof(*rank) * n);
  if (!rank)
    return NULL;
  for (i = 0; i < n; i++)
    rank[i] = data[i];
  replaceWithRank(rank, n);
  return rank;
}

void replaceWithRank(double *data, long n) {
  static DATAnINDEX *indexedData = NULL;
  long i, j, iStart, iEnd;

  indexedData = SDDS_Realloc(indexedData, sizeof(*indexedData) * n);
  for (i = 0; i < n; i++) {
    indexedData[i].data = data[i];
    indexedData[i].originalIndex = i;
  }
  qsort((void *)indexedData, n, sizeof(*indexedData), compareData);
  for (i = 0; i < n; i++)
    data[indexedData[i].originalIndex] = (double)i;
  for (i = 0; i < n - 1; i++) {
    if (data[i] == data[i + 1]) {
      iStart = i;
      for (j = i + 2; j < n; j++) {
        if (data[j - 1] != data[j])
          break;
      }
      iEnd = j - 1;
      double average = (iStart + iEnd) / 2.0;
      for (j = iStart; j <= iEnd; j++)
        data[indexedData[j].originalIndex] = average;
      i = iEnd;
    }
  }
}
