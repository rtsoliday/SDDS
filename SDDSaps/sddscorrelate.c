/**
 * @file sddscorrelate.c
 * @brief Computes and evaluates correlations among columns of data in SDDS files.
 *
 * @details
 * This program reads an input SDDS file, calculates the correlation coefficients
 * between specified or all numeric columns, and outputs the results to a new SDDS file.
 * It supports options for rank-order correlation, standard deviation-based outlier removal,
 * and column/row major data ordering.
 *
 * @section Usage
 * ```
 * sddscorrelate [<inputfile>] [<outputfile>]
 *               [-pipe=[input][,output]]
 *               [-columns=<list-of-names>]
 *               [-excludeColumns=<list-of-names>]
 *               [-withOnly=<name>]
 *               [-rankOrder]
 *               [-stDevOutlier[=limit=<factor>][,passes=<integer>]]
 *               [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Option                            | Description                                                                 |
 * |-----------------------------------|-----------------------------------------------------------------------------|
 * | `-pipe`                           | Use standard input/output for input/output.                                 |
 * | `-columns`                        | Specify columns to include in correlation analysis.                         |
 * | `-excludeColumns`                 | Specify columns to exclude from analysis.                                   |
 * | `-withOnly`                       | Correlate only with the specified column.                                   |
 * | `-rankOrder`                      | Use rank-order (Spearman) correlation instead of linear (Pearson).          |
 * | `-stDevOutlier`                   | Remove outliers based on standard deviation.                                |
 * | `-majorOrder`                     | Set data ordering to row-major or column-major.                             |
 *
 * @subsection Incompatibilities
 *   - `-columns` and `-excludeColumns` cannot be used together.
 *   - `-withOnly` is mutually exclusive with `-excludeColumns`.
 *
 * ### Features
 * - Linear (Pearson) or Rank-Order (Spearman) correlation calculation.
 * - Exclusion of specific columns or correlation with a single column.
 * - Outlier detection and removal using standard deviation thresholds.
 * - Customizable data ordering (row-major or column-major).
 *
 * ### Output
 * The output SDDS file contains:
 * - Correlation coefficients and significance for column pairs.
 * - Number of points used in the correlation.
 * - Parameters summarizing the correlation analysis.
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
 * M. Borland, C. Saunders, R. Soliday
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
  SET_WITHONLY,
  SET_PIPE,
  SET_RANKORDER,
  SET_STDEVOUTLIER,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "columns",
  "excludecolumns",
  "withonly",
  "pipe",
  "rankorder",
  "stdevoutlier",
  "majorOrder",
};

#define USAGE "sddscorrelate [<inputfile>] [<outputfile>]\n\
                     [-pipe=[input][,output]]\n\
                     [-columns=<list-of-names>]\n\
                     [-excludeColumns=<list-of-names>]\n\
                     [-withOnly=<name>]\n\
                     [-rankOrder]\n\
                     [-stDevOutlier[=limit=<factor>][,passes=<integer>]]\n\
                     [-majorOrder=row|column]\n\
\n\
Compute and evaluate correlations among columns of data.\n\
\n\
Options:\n\
  -pipe=[input][,output]          Use standard input/output as input and/or output.\n\
  -columns=<list-of-names>        Specify columns to include in correlation analysis.\n\
  -excludeColumns=<list-of-names> Specify columns to exclude from correlation analysis.\n\
  -withOnly=<name>                Correlate only with the specified column.\n\
  -rankOrder                      Use rank-order (Spearman) correlation instead of linear (Pearson).\n\
  -stDevOutlier[=limit=<factor>][,passes=<integer>]\n\
                                  Remove outliers based on standard deviation.\n\
  -majorOrder=row|column          Set data ordering to row-major or column-major.\n\
\n\
Program by Michael Borland. ("__DATE__ " "__TIME__ ", SVN revision: " SVN_VERSION ")\n"

void replaceWithRank(double *data, int64_t n);
double *findRank(double *data, int64_t n);
void markStDevOutliers(double *data, double limit, long passes, short *keep, int64_t n);

int main(int argc, char **argv) {
  int iArg;
  char **column, **excludeColumn, *withOnly;
  long columns, excludeColumns;
  char *input, *output;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  long i, j, row, count, readCode, rankOrder, iName1, iName2;
  int64_t rows;
  int32_t outlierStDevPasses;
  double **data, correlation, significance, outlierStDevLimit;
  double **rank;
  short **accept;
  char s[SDDS_MAXLINE];
  unsigned long pipeFlags, dummyFlags, majorOrderFlag;
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
      case SET_WITHONLY:
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
      default:
        fprintf(stderr, "Error: unknown or ambiguous option: %s\n", scanned[iArg].list[0]);
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

  processFilenames("sddscorrelate", &input, &output, pipeFlags, 0, NULL);

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

  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 0, NULL, "sddscorrelate output", output) ||
      SDDS_DefineColumn(&SDDSout, "Correlate1Name", NULL, NULL, "Name of correlated quantity 1", NULL, SDDS_STRING, 0) < 0 ||
      SDDS_DefineColumn(&SDDSout, "Correlate2Name", NULL, NULL, "Name of correlated quantity 2", NULL, SDDS_STRING, 0) < 0 ||
      SDDS_DefineColumn(&SDDSout, "CorrelatePair", NULL, NULL, "Names of correlated quantities", NULL, SDDS_STRING, 0) < 0 ||
      SDDS_DefineColumn(&SDDSout, "CorrelationCoefficient", "r", NULL, "Linear correlation coefficient", NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDSout, "CorrelationSignificance", "P$br$n", NULL, "Linear correlation coefficient significance", NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDSout, "CorrelationPoints", NULL, NULL, "Number of points used for correlation", NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineParameter(&SDDSout, "CorrelatedRows", NULL, NULL, "Number of data rows in correlation analysis", NULL, SDDS_LONG, NULL) < 0 ||
      SDDS_DefineParameter(&SDDSout, "sddscorrelateInputFile", NULL, NULL, "Data file processed by sddscorrelate", NULL, SDDS_STRING, input ? input : "stdin") < 0 ||
      SDDS_DefineParameter(&SDDSout, "sddscorrelateMode", NULL, NULL, NULL, NULL, SDDS_STRING, rankOrder ? "Rank-Order (Spearman)" : "Linear (Pearson)") < 0 ||
      SDDS_DefineParameter1(&SDDSout, "sddscorrelateStDevOutlierPasses", NULL, NULL, "Number of passes of standard-deviation outlier elimination applied", NULL, SDDS_LONG, &outlierStDevPasses) < 0 ||
      SDDS_DefineParameter1(&SDDSout, "sddscorrelateStDevOutlierLimit", NULL, NULL, "Standard-deviation outlier limit applied", NULL, SDDS_DOUBLE, &outlierStDevLimit) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;

  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  data = malloc(sizeof(*data) * columns);
  if (!data ||
      (rankOrder && !(rank = malloc(sizeof(*rank) * columns))) ||
      !(accept = malloc(sizeof(*accept) * columns))) {
    SDDS_Bomb("allocation failure");
  }

  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&SDDSin)) < 3)
      continue;
    if (!SDDS_StartPage(&SDDSout, columns * (columns - 1) / 2) ||
        !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "CorrelatedRows", rows, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    for (i = 0; i < columns; i++) {
      data[i] = SDDS_GetColumnInDoubles(&SDDSin, column[i]);
      if (!data[i])
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (rankOrder)
        rank[i] = findRank(data[i], rows);
      if (outlierStDevPasses) {
        accept[i] = malloc(sizeof(**accept) * rows);
        if (!accept[i])
          SDDS_Bomb("allocation failure");
        markStDevOutliers(data[i], outlierStDevLimit, outlierStDevPasses, accept[i], rows);
      } else {
        accept[i] = NULL;
      }
    }
    for (i = row = 0; i < columns; i++) {
      for (j = i + 1; j < columns; j++) {
        iName1 = i;
        iName2 = j;
        if (withOnly) {
          if (strcmp(withOnly, column[i]) == 0) {
            iName1 = j;
            iName2 = i;
          } else if (strcmp(withOnly, column[j]) == 0) {
            iName1 = i;
            iName2 = j;
          } else {
            continue;
          }
        }
        correlation = linearCorrelationCoefficient(rankOrder ? rank[i] : data[i],
                                                   rankOrder ? rank[j] : data[j],
                                                   accept[i], accept[j], rows, &count);
        significance = linearCorrelationSignificance(correlation, count);
        snprintf(s, sizeof(s), "%s.%s", column[iName1], column[iName2]);
        if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, row++,
                               0, column[iName1],
                               1, column[iName2],
                               2, s,
                               3, correlation,
                               4, significance,
                               5, count,
                               -1)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
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

  free(data);
  if (rankOrder)
    free(rank);
  free(accept);

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

void markStDevOutliers(double *data, double limit, long passes, short *keep, int64_t n) {
  double sum1, sum2, variance, mean, absLimit;
  long pass;
  int64_t i, summed, kept;

  for (i = 0; i < n; i++)
    keep[i] = 1;
  kept = n;
  for (pass = 0; pass < passes && kept; pass++) {
    sum1 = 0.0;
    summed = 0;
    for (i = 0; i < n; i++) {
      if (keep[i]) {
        sum1 += data[i];
        summed += 1;
      }
    }
    if (summed < 2)
      return;
    mean = sum1 / summed;
    sum2 = 0.0;
    for (i = 0; i < n; i++) {
      if (keep[i])
        sum2 += sqr(data[i] - mean);
    }
    variance = sum2 / summed;
    if (variance > 0.0) {
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
  double diff = ((DATAnINDEX *)d1)->data - ((DATAnINDEX *)d2)->data;
  if (diff < 0)
    return -1;
  else if (diff > 0)
    return 1;
  else
    return 0;
}

double *findRank(double *data, int64_t n) {
  double *rank = malloc(sizeof(*rank) * n);
  if (!rank)
    return NULL;
  for (int64_t i = 0; i < n; i++)
    rank[i] = data[i];
  replaceWithRank(rank, n);
  return rank;
}

void replaceWithRank(double *data, int64_t n) {
  static DATAnINDEX *indexedData = NULL;
  int64_t i, j, iStart, iEnd;

  indexedData = SDDS_Realloc(indexedData, sizeof(*indexedData) * n);
  for (i = 0; i < n; i++) {
    indexedData[i].data = data[i];
    indexedData[i].originalIndex = i;
  }
  qsort(indexedData, n, sizeof(*indexedData), compareData);
  for (i = 0; i < n; i++)
    data[indexedData[i].originalIndex] = (double)i;
  for (i = 0; i < n - 1; i++) {
    if (data[i] == data[i + 1]) {
      iStart = i;
      for (j = i + 2; j < n; j++) {
        if (data[j] != data[i])
          break;
      }
      iEnd = j - 1;
      double averageRank = (iStart + iEnd) / 2.0;
      for (j = iStart; j <= iEnd; j++)
        data[indexedData[j].originalIndex] = averageRank;
      i = iEnd;
    }
  }
}
