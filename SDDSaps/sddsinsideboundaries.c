/**
 * @file sddsinsideboundaries.c
 * @brief Analyzes data points relative to user-defined geometric boundaries.
 *
 * @details
 * This program reads an SDDS (Self Describing Data Set) file and evaluates whether each data point lies inside 
 * or outside user-defined geometric boundaries specified in a separate file. It supports multithreading for 
 * improved performance and offers options to filter and save points based on their inclusion or exclusion from 
 * the specified boundaries. The input boundaries can span multiple pages.
 *
 * @section Usage
 * ```
 * sddsinsideboundaries [<inputfile>] [<outputfile>] 
 *                      [-pipe=[input][,output]]
 *                       -columns=<x-name>,<y-name>
 *                       -boundary=<filename>,<x-name>,<y-name>
 *                      [-insideColumn=<column_name>]
 *                      [-keep={inside|outside}]
 *                      [-threads=<number>]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Specify the names of the (x, y) columns in the input file.                            |
 * | `-boundary`                           | Provide a file containing boundary data, including x and y columns.                  |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Specify input and/or output via pipe.                                                |
 * | `-insideColumn`                       | Define the name of the output column indicating boundary inclusion.                  |
 * | `-keep`                               | Filter points based on inclusion (inside) or exclusion (outside) from boundaries.    |
 * | `-threads`                            | Set the number of threads for computation (default: 1).                              |
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
#if defined(linux) || (defined(_WIN32) && !defined(_MINGW))
#  include <omp.h>
#else
void omp_set_num_threads(int a) {}
#endif

typedef enum {
  SET_COLUMNS,
  SET_BOUNDARY,
  SET_INSIDE_COLUMN,
  SET_KEEP,
  SET_PIPE,
  SET_THREADS,
  N_OPTIONS
} option_type;

char *option[N_OPTIONS] = {
  "columns",
  "boundary",
  "insideColumn",
  "keep",
  "pipe",
  "threads",
};

static char *USAGE = "\n"                                               \
  "  sddsinsideboundaries [<inputfile>] [<outputfile>]\n"               \
  "                       [-pipe=[input][,output]]\n"                   \
  "                        -columns=<x-name>,<y-name>\n"                \
  "                        -boundary=<filename>,<x-name>,<y-name>\n"    \
  "                       [-insideColumn=<column_name>]\n"              \
  "                       [-keep={inside|outside}]\n"                   \
  "                       [-threads=<number>]\n"                        \
  "Options:\n"                                                          \
  "  -columns=<x-name>,<y-name>\n"                                      \
  "      Specify the names of the (x, y) columns in the input file.\n"  \
  "  -boundary=<filename>,<x-name>,<y-name>\n"                          \
  "      Provide a file with boundary data, including x and y columns.\n" \
  "      The file can have multiple pages.\n"                           \
  "  -insideColumn=<column_name>\n"                                    \
  "      Specify the name of the output column for the count of boundaries\n" \
  "      containing each point (default: InsideSum).\n"                 \
  "  -keep={inside|outside}\n"                                          \
  "      Filter points:\n"                                              \
  "        inside - Keep only points inside any boundary.\n"            \
  "        outside - Keep only points outside all boundaries.\n"        \
  "      By default, all points are kept.\n"                            \
  "  -threads=<number>\n"                                               \
  "      Set the number of threads for computation (default: 1).\n\n"   \
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

typedef enum {
  KEEP_ALL,
  KEEP_INSIDE,
  KEEP_OUTSIDE,
  N_KEEP_OPTIONS
} keep_option_types;

char *keepOption[N_KEEP_OPTIONS] = {
  "all", 
  "inside", 
  "outside"
};

long compute_inside_sum(double x, double y, double **xBoundary, double **yBoundary, int64_t *nValues, long nBoundaries);
long read_boundary_data(char *boundaryInput, char *bxColumn, char *byColumn, double ***xBoundary, double ***yBoundary, int64_t **nValues);

double *xData, *yData;
double **xBoundary = NULL, **yBoundary = NULL;
int64_t *nValues = NULL, rows;
long nBoundaries = 0;
int threads = 1;
int32_t *insideSumData;

int main(int argc, char **argv) {
  int iArg;
  SCANNED_ARG *scanned = NULL;
  char *input = NULL, *output = NULL, *boundaryInput = NULL;
  char *xColumn = NULL, *yColumn = NULL, *bxColumn = NULL, *byColumn = NULL;
  char *insideColumn = "InsideSum";
  SDDS_DATASET SDDSin, SDDSout;
  int32_t *keep;
  long keepCode = KEEP_ALL;
  long readCode, keepSeen = 0;
  int64_t i;
  unsigned long pipeFlags = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case SET_COLUMNS:
        if (xColumn || yColumn)
          SDDS_Bomb("only one -columns option may be given");
        if (scanned[iArg].n_items != 3)
          SDDS_Bomb("invalid -columns syntax");
        xColumn = scanned[iArg].list[1];
        yColumn = scanned[iArg].list[2];
        break;
      case SET_BOUNDARY:
        if (boundaryInput)
          SDDS_Bomb("only one -boundary option may be given");
        if (scanned[iArg].n_items != 4)
          SDDS_Bomb("invalid -boundary syntax");
        boundaryInput = scanned[iArg].list[1];
        bxColumn = scanned[iArg].list[2];
        byColumn = scanned[iArg].list[3];
        break;
      case SET_INSIDE_COLUMN:
        if (scanned[iArg].n_items != 2)
          SDDS_Bomb("invalid -insideColumn syntax");
        insideColumn = scanned[iArg].list[1];
        break;
      case SET_KEEP:
        if (keepSeen)
          SDDS_Bomb("only one -keep option may be given");
        if (scanned[iArg].n_items != 2)
          SDDS_Bomb("invalid -keep syntax");
        if ((keepCode = match_string(scanned[iArg].list[1], keepOption, N_KEEP_OPTIONS, 0)) < 0)
          SDDS_Bomb("invalid -keep value. Supply 'all', 'inside', or 'outside' or a unique abbreviation");
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_THREADS:
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%d", &threads) != 1 || threads < 1)
          SDDS_Bomb("invalid -threads syntax");
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

  processFilenames("sddsinsideboundaries", &input, &output, pipeFlags, 0, NULL);
  if (!pipeFlags && strcmp(input, output) == 0)
    SDDS_Bomb("can't use same file for input and output");

  if (!boundaryInput || !bxColumn || !byColumn)
    SDDS_Bomb("-boundaries option must be given");
  if ((nBoundaries = read_boundary_data(boundaryInput, bxColumn, byColumn, &xBoundary, &yBoundary, &nValues)) <= 0)
    SDDS_Bomb("No valid data in boundary data file");

  if (!SDDS_InitializeInput(&SDDSin, input) ||
      !SDDS_InitializeCopy(&SDDSout, &SDDSin, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_CheckColumn(&SDDSin, xColumn, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK)
    SDDS_Bomb("-xColumn is not present or not numeric");
  if (SDDS_CheckColumn(&SDDSin, yColumn, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK)
    SDDS_Bomb("-yColumn is not present or not numeric");

  if (!SDDS_DefineColumn(&SDDSout, insideColumn, NULL, NULL, "Number of boundaries containing this point", NULL, SDDS_LONG, 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  insideSumData = NULL;
  keep = NULL;
  xData = yData = NULL;
  omp_set_num_threads(threads);
  //fprintf(stderr, "threads=%d\n", threads);
  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    if (!SDDS_CopyPage(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_SetRowFlags(&SDDSout, 1);
    rows = SDDS_CountRowsOfInterest(&SDDSout);
    if (!(xData = SDDS_GetColumnInDoubles(&SDDSin, xColumn)) ||
        !(yData = SDDS_GetColumnInDoubles(&SDDSin, yColumn))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    insideSumData = calloc(rows, sizeof(*insideSumData));
    keep = calloc(rows, sizeof(*keep));

#pragma omp parallel for
    for (i = 0; i < rows; i++) {
      insideSumData[i] = compute_inside_sum(xData[i], yData[i], xBoundary, yBoundary, nValues, nBoundaries);
      switch (keepCode) {
      case KEEP_INSIDE:
        keep[i] = insideSumData[i];
        break;
      case KEEP_OUTSIDE:
        keep[i] = insideSumData[i] == 0;
        break;
      default:
        keep[i] = 1;
        break;
      }
    }
    if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_NAME, insideSumData, rows, insideColumn) ||
        !SDDS_AssertRowFlags(&SDDSout, SDDS_FLAG_ARRAY, keep, rows) ||
        !SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    free(keep);
    free(insideSumData);
    free(xData);
    free(yData);
  }
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 0;
}

long read_boundary_data(char *boundaryInput, char *bxColumn, char *byColumn, double ***xBoundary, double ***yBoundary, int64_t **nValues) {
  SDDS_DATASET SDDSin;
  long nSets = 0;

  if (!SDDS_InitializeInput(&SDDSin, boundaryInput) ||
      SDDS_CheckColumn(&SDDSin, bxColumn, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckColumn(&SDDSin, byColumn, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  while (SDDS_ReadPage(&SDDSin) > 0) {
    if (SDDS_RowCount(&SDDSin) == 0)
      continue;
    *xBoundary = SDDS_Realloc(*xBoundary, sizeof(**xBoundary) * (nSets + 1));
    *yBoundary = SDDS_Realloc(*yBoundary, sizeof(**yBoundary) * (nSets + 1));
    *nValues = SDDS_Realloc(*nValues, sizeof(**nValues) * (nSets + 1));
    (*nValues)[nSets] = SDDS_RowCount(&SDDSin);
    if (!((*xBoundary)[nSets] = SDDS_GetColumnInDoubles(&SDDSin, bxColumn)) ||
        !((*yBoundary)[nSets] = SDDS_GetColumnInDoubles(&SDDSin, byColumn)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    nSets++;
  }
  return nSets;
}

long compute_inside_sum(double x, double y, double **xBoundary, double **yBoundary, int64_t *nValues, long nBoundaries) {
  int64_t ib;
  long insideSum = 0;
  for (ib = 0; ib < nBoundaries; ib++)
    insideSum += pointIsInsideContour(x, y, xBoundary[ib], yBoundary[ib], nValues[ib], NULL, 0.0);
  return insideSum;
}
