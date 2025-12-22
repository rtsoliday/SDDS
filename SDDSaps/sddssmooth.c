/**
 * @file sddssmooth.c
 * @brief Smooths data columns in SDDS-format files using various techniques.
 *
 * @details
 * This program smooths data columns in SDDS-format files using techniques such as nearest-neighbor averaging, 
 * Gaussian convolution, median filtering, and Savitzky-Golay filtering. It also supports despiking and the 
 * creation of new or difference columns. Users can specify a variety of options for input/output customization 
 * and processing methods.
 *
 * @section Usage
 * ```
 * sddssmooth [<inputfile>] [<outputfile>]
 *            [-pipe=[input][,output]]
 *             -columns=<name>[,...]
 *            [-points=<oddInteger>]
 *            [-passes=<integer>]
 *            [-gaussian=<sigmaValueIn#Rows>]
 *            [-despike[=neighbors=<integer>,passes=<integer>,averageOf=<integer>,threshold=<value>]]
 *            [-SavitzkyGolay=<left>,<right>,<order>[,<derivativeOrder>]]
 *            [-medianFilter=windowSize=<integer>]
 *            [-newColumns]
 *            [-differenceColumns]
 *            [-nowarnings]
 *            [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Specifies column names (wildcards allowed).                                           |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | SDDS pipe input/output.                                                              |
 * | `-points`                             | Sets points for nearest-neighbor averaging (odd integer, default: 3).                 |
 * | `-passes`                             | Number of smoothing passes (default: 1, 0 disables smoothing).                       |
 * | `-gaussian`                           | Gaussian kernel smoothing with specified sigma.                                       |
 * | `-despike`                            | Despike using neighbors, passes, averageOf, and threshold.                           |
 * | `-SavitzkyGolay`                      | Savitzky-Golay filter with specified window and order.                                |
 * | `-medianFilter`                       | Median filtering with specified window size (odd integer, default: 3).               |
 * | `-newColumns`                         | Create new columns with smoothed data.                                               |
 * | `-differenceColumns`                  | Create difference columns (original minus smoothed).                                 |
 * | `-nowarnings`                         | Suppress warnings.                                                                   |
 * | `-majorOrder`                         | Specify row or column major order for data.                                          |
 *
 * @subsection Incompatibilities
 *   - `-SavitzkyGolay` is incompatible with:
 *     - `-passes` (nearest-neighbor smoothing is not done if `-SavitzkyGolay` is specified).
 *   - `-despike` must be applied before other smoothing options.
 *   - `-medianFilter` requires an odd integer window size.
 *   - `-points` requires an odd integer.
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  CLO_POINTS,
  CLO_PASSES,
  CLO_COLUMNS,
  CLO_PIPE,
  CLO_NEWCOLUMNS,
  CLO_DIFFERENCECOLUMNS,
  CLO_DESPIKE,
  CLO_NOWARNINGS,
  CLO_SAVITZKYGOLAY,
  CLO_MAJOR_ORDER,
  CLO_MEDIAN_FILTER,
  CLO_GAUSSIAN,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "points",
  "passes",
  "columns",
  "pipe",
  "newcolumns",
  "differencecolumns",
  "despike",
  "nowarnings",
  "savitzkygolay",
  "majorOrder",
  "medianFilter",
  "gaussian"
};

static char *USAGE =
  "sddssmooth [<inputfile>] [<outputfile>]\n"
  "           [-pipe=[input][,output]]\n"
  "            -columns=<name>[,...]\n"
  "           [-points=<oddInteger>]\n"
  "           [-passes=<integer>]\n"
  "           [-gaussian=<sigmaValueIn#Rows>]\n"
  "           [-despike[=neighbors=<integer>,passes=<integer>,averageOf=<integer>,threshold=<value>]]\n"
  "           [-SavitzkyGolay=<left>,<right>,<order>[,<derivativeOrder>]]\n"
  "           [-medianFilter=windowSize=<integer>]\n"
  "           [-newColumns]\n"
  "           [-differenceColumns]\n"
  "           [-nowarnings]\n"
  "           [-majorOrder=row|column]\n"
  "Options:\n"
  "  -pipe=[input][,output]               The standard SDDS Toolkit pipe option.\n"
  "  -columns=<name>[,...]                Specifies the names of the column(s) to smooth. The names may include wildcards.\n"
  "  -points=<oddInteger>                 Specifies the number of points to average to create a smoothed value for each point.\n"
  "                                       Must be an odd integer. Default is 3.\n"
  "  -passes=<integer>                    Specifies the number of nearest-neighbor-averaging smoothing passes to make over each column of data.\n"
  "                                       Default is 1. If 0, no such smoothing is done. In the limit of an infinite number of passes,\n"
  "                                       every point will tend toward the average value of the original data.\n"
  "                                       If -despike is also given, then despiking occurs first.\n"
  "  -gaussian=<sigmaValueIn#Rows>        Smooths with a Gaussian kernel using the given sigma. Sigma is expressed in terms of the number of rows.\n"
  "  -despike[=neighbors=<integer>,passes=<integer>,averageOf=<integer>,threshold=<value>]\n"
  "                                       Specifies smoothing by despiking. By default, 4 nearest-neighbors are used and 1 pass is done.\n"
  "                                       If this option is not given, no despiking is done.\n"
  "  -SavitzkyGolay=<left>,<right>,<order>[,<derivativeOrder>]\n"
  "                                       Specifies smoothing by using a Savitzky-Golay filter, which involves fitting a polynomial of specified order through left + right + 1 points.\n"
  "                                       Optionally, takes the derivativeOrder-th derivative of the data.\n"
  "                                       If this option is given, nearest-neighbor-averaging smoothing is not done.\n"
  "                                       If -despike is also given, then despiking occurs first.\n"
  "  -medianFilter=windowSize=<integer>   Specifies median-filter-based smoothing with the given window size (must be an odd integer, default is 3).\n"
  "                                       It smooths the original data by finding the median of a data point among the nearest left (W-1)/2 points,\n"
  "                                       the data point itself, and the nearest right (W-1)/2 points.\n"
  "  -newColumns                          Specifies that the smoothed data will be placed in new columns, rather than replacing\n"
  "                                       the data in each column with the smoothed result. The new columns are named columnNameSmoothed,\n"
  "                                       where columnName is the original name of a column.\n"
  "  -differenceColumns                   Specifies that additional columns be created in the output file, containing the difference between\n"
  "                                       the original data and the smoothed data. The new columns are named columnNameUnsmooth,\n"
  "                                       where columnName is the original name of the column.\n"
  "  -nowarnings                          Suppresses warning messages.\n"
  "  -majorOrder=row|column               Specifies the major order for data processing: row or column.\n"
  "\nProgram by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

long resolveColumnNames(SDDS_DATASET *SDDSin, char ***column, int32_t *columns);
void gaussianConvolution(double *data, int64_t rows, double sigma);

#define FL_NEWCOLUMNS 0x0001UL
#define FL_DIFCOLUMNS 0x0002UL

#define DESPIKE_AVERAGEOF 0x0001U

int main(int argc, char **argv) {
  int iArg;
  char **inputColumn, **outputColumn, **difColumn;
  int32_t columns;
  long despike, median, smooth;
  int32_t despikeNeighbors, despikePasses, despikeAverageOf;
  char *input, *output;
  long i, readCode;
  int64_t j, rows;
  int32_t smoothPoints, smoothPasses;
  long noWarnings, medianWindowSize = 3;
  unsigned long pipeFlags, flags, despikeFlags, majorOrderFlag, dummyFlags;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  double *data, despikeThreshold;
  int32_t SGLeft, SGRight, SGOrder, SGDerivOrder;
  short columnMajorOrder = -1;
  double gaussianSigma = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3 || argc > (3 + N_OPTIONS))
    bomb(NULL, USAGE);

  output = input = NULL;
  inputColumn = outputColumn = NULL;
  columns = 0;
  pipeFlags = 0;
  smoothPoints = 3;
  smoothPasses = 1;
  flags = 0;
  despike = 0;
  median = 0;
  smooth = 0;
  noWarnings = 0;
  SGOrder = -1;
  SGDerivOrder = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_PASSES:
        smooth = 1;
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%" SCNd32, &smoothPasses) != 1 ||
            smoothPasses < 0)
          SDDS_Bomb("invalid -passes syntax/value");
        break;
      case CLO_GAUSSIAN:
        gaussianSigma = 0;
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%lf", &gaussianSigma) != 1 ||
            gaussianSigma <= 0)
          SDDS_Bomb("invalid -gaussian syntax/value");
        break;
      case CLO_POINTS:
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%" SCNd32, &smoothPoints) != 1 ||
            smoothPoints < 1 ||
            smoothPoints % 2 == 0)
          SDDS_Bomb("invalid -points syntax/value");
        break;
      case CLO_COLUMNS:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        inputColumn = tmalloc(sizeof(*inputColumn) * (columns = scanned[iArg].n_items - 1));
        for (i = 0; i < columns; i++)
          inputColumn[i] = scanned[iArg].list[i + 1];
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_NEWCOLUMNS:
        flags |= FL_NEWCOLUMNS;
        break;
      case CLO_DIFFERENCECOLUMNS:
        flags |= FL_DIFCOLUMNS;
        break;
      case CLO_DESPIKE:
        scanned[iArg].n_items--;
        despikeNeighbors = 4;
        despikePasses = 1;
        despikeThreshold = 0;
        despikeAverageOf = 2;
        if (scanned[iArg].n_items > 0 &&
            (!scanItemList(&despikeFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                           "neighbors", SDDS_LONG, &despikeNeighbors, 1, 0,
                           "passes", SDDS_LONG, &despikePasses, 1, 0,
                           "averageof", SDDS_LONG, &despikeAverageOf, 1, DESPIKE_AVERAGEOF,
                           "threshold", SDDS_DOUBLE, &despikeThreshold, 1, 0, NULL) ||
             despikeNeighbors < 2 || despikePasses < 1 || despikeAverageOf < 2 || despikeThreshold < 0)) {
          fprintf(stderr, "sddssmooth: Invalid -despike syntax/values: neighbors=%" PRId32 ", passes=%" PRId32 ", averageOf=%" PRId32 ", threshold=%e\n", despikeNeighbors, despikePasses, despikeAverageOf, despikeThreshold);
          exit(EXIT_FAILURE);
        }
        if (!(despikeFlags & DESPIKE_AVERAGEOF))
          despikeAverageOf = despikeNeighbors;
        if (despikeAverageOf > despikeNeighbors)
          SDDS_Bomb("invalid -despike syntax/values: averageOf>neighbors");
        despike = 1;
        break;
      case CLO_MEDIAN_FILTER:
        scanned[iArg].n_items--;
        medianWindowSize = 0;
        if (scanned[iArg].n_items > 0 &&
            (!scanItemList(&dummyFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                           "windowSize", SDDS_LONG, &medianWindowSize, 1, 0, NULL) ||
             medianWindowSize < 0 || (medianWindowSize!=0 && medianWindowSize%2!=1))) {
          fprintf(stderr, "sddssmooth: Invalid -medianFilter syntax/values: windowSize=%ld (0=no median filter, odd value required)\n", medianWindowSize);
          exit(EXIT_FAILURE);
        }
        if (medianWindowSize>1)
          median = 1;
        break;
      case CLO_NOWARNINGS:
        noWarnings = 1;
        break;
      case CLO_SAVITZKYGOLAY:
        if ((scanned[iArg].n_items != 4 && scanned[iArg].n_items != 5) ||
            sscanf(scanned[iArg].list[1], "%" SCNd32, &SGLeft) != 1 ||
            sscanf(scanned[iArg].list[2], "%" SCNd32, &SGRight) != 1 ||
            sscanf(scanned[iArg].list[3], "%" SCNd32, &SGOrder) != 1 ||
            (scanned[iArg].n_items == 5 && sscanf(scanned[iArg].list[4], "%" SCNd32, &SGDerivOrder) != 1) ||
            SGLeft < 0 ||
            SGRight < 0 ||
            (SGLeft + SGRight) < SGOrder ||
            SGOrder < 0 ||
            SGDerivOrder < 0)
          SDDS_Bomb("invalid -SavitzkyGolay syntax/values");
        break;
      case CLO_MAJOR_ORDER:
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

  processFilenames("sddssmooth", &input, &output, pipeFlags, 0, NULL);
  /*do not change the previous setting; before it does smooth by default before adding median */
  if (!median)
    smooth = 1;

  if (!despike && !smoothPasses && !median && !noWarnings)
    fprintf(stderr, "warning: smoothing parameters won't result in any change in data (sddssmooth)\n");

  if (!columns)
    SDDS_Bomb("supply the names of columns to smooth with the -columns option");

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!resolveColumnNames(&SDDSin, &inputColumn, &columns))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!columns)
    SDDS_Bomb("no columns selected for smoothing");

  if (!SDDS_InitializeCopy(&SDDSout, &SDDSin, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  outputColumn = tmalloc(sizeof(*outputColumn) * columns);

  if (flags & FL_NEWCOLUMNS) {
    for (i = 0; i < columns; i++) {
      if (SGDerivOrder <= 0) {
        outputColumn[i] = tmalloc(sizeof(**outputColumn) * (strlen(inputColumn[i]) + 1 + strlen("Smoothed")));
        sprintf(outputColumn[i], "%sSmoothed", inputColumn[i]);
      } else {
        outputColumn[i] = tmalloc(sizeof(**outputColumn) * (strlen(inputColumn[i]) + 1 + strlen("SmoothedDeriv")) + 5);
        sprintf(outputColumn[i], "%sSmoothedDeriv%" PRId32, inputColumn[i], SGDerivOrder);
      }
      if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, inputColumn[i], outputColumn[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  } else
    for (i = 0; i < columns; i++)
      outputColumn[i] = inputColumn[i];

  difColumn = NULL;
  if (flags & FL_DIFCOLUMNS) {
    difColumn = tmalloc(sizeof(*difColumn) * columns);
    for (i = 0; i < columns; i++) {
      difColumn[i] = tmalloc(sizeof(**difColumn) * (strlen(inputColumn[i]) + 1 + strlen("Unsmooth")));
      sprintf(difColumn[i], "%sUnsmooth", inputColumn[i]);
      if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, inputColumn[i], difColumn[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if ((SDDS_GetParameterIndex(&SDDSout, "SmoothPoints") < 0 &&
       SDDS_DefineParameter1(&SDDSout, "SmoothPoints", NULL, NULL, NULL, NULL, SDDS_LONG, &smoothPoints) < 0) ||
      (SDDS_GetParameterIndex(&SDDSout, "SmoothPasses") < 0 &&
       SDDS_DefineParameter1(&SDDSout, "SmoothPasses", NULL, NULL, NULL, NULL, SDDS_LONG, &smoothPasses) < 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;

  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    if (!SDDS_CopyPage(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((rows = SDDS_CountRowsOfInterest(&SDDSin))) {
      for (i = 0; i < columns; i++) {
        if (!(data = SDDS_GetColumnInDoubles(&SDDSin, inputColumn[i])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (despike)
          despikeData(data, rows, despikeNeighbors, despikePasses, despikeAverageOf, despikeThreshold, 0);
        if (gaussianSigma > 0)
          gaussianConvolution(data, rows, gaussianSigma);
        if (median) {
          double *mData = NULL;
          mData = malloc(sizeof(*mData) * rows);
          median_filter(data, mData, rows, medianWindowSize);
          memcpy(data, mData, sizeof(*mData) * rows);
          free(mData);
        }
        if (SGOrder >= 0) {
          long pass = 0;
          for (pass = 0; pass < smoothPasses; pass++)
            SavitzkyGolaySmooth(data, rows, SGOrder, SGLeft, SGRight, SGDerivOrder);
        } else if (smooth && smoothPasses)
          smoothData(data, rows, smoothPoints, smoothPasses);

        if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, data, rows, outputColumn[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (flags & FL_DIFCOLUMNS) {
          double *data0;
          if (!(data0 = SDDS_GetColumnInDoubles(&SDDSin, inputColumn[i])))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          for (j = 0; j < rows; j++)
            data0[j] -= data[j];
          if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, data0, rows, difColumn[i]))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          free(data0);
        }
        free(data);
      }
    }
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}

long resolveColumnNames(SDDS_DATASET *SDDSin, char ***column, int32_t *columns) {
  long i;

  SDDS_SetColumnFlags(SDDSin, 0);
  for (i = 0; i < *columns; i++) {
    if (!SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, (*column)[i], SDDS_OR))
      return 0;
  }
  free(*column);
  if (!(*column = SDDS_GetColumnNames(SDDSin, columns)) || *columns == 0) {
    SDDS_SetError("no columns found");
    return 0;
  }
  return 1;
}

void gaussianConvolution(double *data, int64_t rows, double sigma) {
  double *data1, *expFactor;
  int64_t i, j, j1, j2, nsRows, nsPerSide;

  nsPerSide = 6 * sigma;
  nsRows = 2 * nsPerSide + 1;
  expFactor = tmalloc(sizeof(*expFactor) * nsRows);
  for (j = -nsPerSide; j <= nsPerSide; j++)
    expFactor[j + nsPerSide] = exp(-sqr(j / sigma) / 2.0) / (sigma * sqrt(2 * PI));

  data1 = calloc(rows, sizeof(*data1));
  for (i = 0; i < rows; i++) {
    j1 = i - nsPerSide;
    j2 = i + nsPerSide;
    if (j1 < 0)
      j1 = 0;
    if (j2 >= rows)
      j2 = rows;
    for (j = j1; j <= j2; j++)
      data1[i] += data[j] * expFactor[j - i + nsPerSide];
  }
  memcpy(data, data1, sizeof(*data) * rows);
  free(data1);
}
