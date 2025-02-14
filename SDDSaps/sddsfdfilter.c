/**
 * @file sddsfdfilter.c
 * @brief SDDS-format frequency-domain filter program.
 *
 * @details
 * This program applies various frequency-domain filters to SDDS-formatted data files. It supports multiple filtering 
 * options, including high-pass, low-pass, notch, bandpass, threshold, and file-based filters. Users can specify 
 * input and output files or use pipes for data processing.
 *
 * @section Usage
 * ```
 * sddsfdfilter [<inputfile>] [<outputfile>]
 *              [-pipe=[input][,output]]
 *              [-columns=<indep-variable>[,<depen-quantity>[,...]]]
 *              [-exclude=<depen-quantity>[,...]]
 *              [-clipFrequencies=[high=<number>][,low=<number>]]
 *              [-threshold=level=<value>[,fractional][,start=<freq>][,end=<freq>]]
 *              [-highpass=start=<freq>,end=<freq>]
 *              [-lowpass=start=<freq>,end=<freq>]
 *              [-notch=center=<center>,flatWidth=<value>,fullWidth=<value>]
 *              [-bandpass=center=<center>,flatWidth=<value>,fullWidth=<value>]
 *              [-filterFile=filename=<filename>,frequency=<columnName>{,real=<cName>,imaginary=<cName>|magnitude=<cName>}]
 *              [-cascade]
 *              [-newColumns]
 *              [-differenceColumns]
 *              [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required           | Description                                                                |
 * |--------------------|----------------------------------------------------------------------------|
 * | `-columns`                              | Specifies columns to filter.                                               |
 *
 * | Optional                                  | Description                                                                |
 * |-----------------------------------------|----------------------------------------------------------------------------|
 * | `-pipe`                                 | Specifies input and/or output via pipes.                                   |
 * | `-exclude`                              | Specifies columns to exclude from filtering.                               |
 * | `-clipFrequencies`                      | Clips frequencies outside the specified range.                             |
 * | `-threshold`                            | Filters based on a threshold with optional range.                          |
 * | `-highpass`                             | Applies a high-pass filter with a specified frequency range.               |
 * | `-lowpass`                              | Applies a low-pass filter with a specified frequency range.                |
 * | `-notch`                                | Applies a notch filter with specified characteristics.                     |
 * | `-bandpass`                             | Applies a band-pass filter with specified characteristics.                 |
 * | `-filterFile`                           | Specifies a filter based on external file data.                            |
 * | `-cascade`                              | Specifies cascaded filter stages.                                          |
 * | `-newColumns`                           | Creates new columns for filtered data.                                     |
 * | `-differenceColumns`                    | Creates columns for differences between original and filtered data.        |
 * | `-majorOrder`                           | Specifies major order as either `row` or `column`.                         |
 *
 * @subsection Incompatibilities
 *   - `-cascade` must not precede filter definitions.
 *   - Only one of the following may be specified for `-filterFile`:
 *     - `magnitude=<columnName>`
 *     - Both `real=<columnName>` and `imaginary=<columnName>`
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
 * M. Borland, R. Soliday, Xuesong Jiao, L. Emery, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "fftpackC.h"
#include "SDDSutils.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_PIPE,
  SET_CASCADE,
  SET_CLIPFREQ,
  SET_COLUMNS,
  SET_THRESHOLD,
  SET_HIGHPASS,
  SET_LOWPASS,
  SET_NOTCH,
  SET_BANDPASS,
  SET_FILTERFILE,
  SET_NEWCOLUMNS,
  SET_DIFFERENCECOLUMNS,
  SET_EXCLUDE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "pipe",
  "cascade",
  "clip",
  "columns",
  "threshold",
  "highpass",
  "lowpass",
  "notch",
  "bandpass",
  "filterfile",
  "newcolumns",
  "differencecolumns",
  "exclude",
  "majorOrder",
};

/* Improved and formatted usage message */
static char *USAGE =
  "sddsfdfilter [<inputfile>] [<outputfile>]\n"
  "             [-pipe=[input][,output]]\n"
  "             [-columns=<indep-variable>[,<depen-quantity>[,...]]]\n"
  "             [-exclude=<depen-quantity>[,...]]\n"
  "             [-clipFrequencies=[high=<number>][,low=<number>]]\n"
  "             [-threshold=level=<value>[,fractional][,start=<freq>][,end=<freq>]]\n"
  "             [-highpass=start=<freq>,end=<freq>]\n"
  "             [-lowpass=start=<freq>,end=<freq>]\n"
  "             [-notch=center=<center>,flatWidth=<value>,fullWidth=<value>]\n"
  "             [-bandpass=center=<center>,flatWidth=<value>,fullWidth=<value>]\n"
  "             [-filterFile=filename=<filename>,frequency=<columnName>{,real=<cName>,imaginary=<cName>|magnitude=<cName>}]\n"
  "             [-cascade]\n"
  "             [-newColumns]\n"
  "             [-differenceColumns]\n"
  "             [-majorOrder=row|column]\n\n"
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define FILT_START_GIVEN 0x00000001U
#define FILT_END_GIVEN 0x00000002U
#define FILT_THRES_GIVEN 0x00000004U
#define FILT_CENTER_GIVEN 0x00000008U
#define FILT_FULLWIDTH_GIVEN 0x00000010U
#define FILT_FREQNAME_GIVEN 0x00000020U
#define FILT_REALNAME_GIVEN 0x00000040U
#define FILT_IMAGNAME_GIVEN 0x00000080U
#define FILT_MAGNAME_GIVEN 0x00000100U
#define FILT_FRACTHRES_GIVEN 0x00000200U
#define FILT_LEVEL_GIVEN 0x00000400U
#define FILT_FILENAME_GIVEN 0x00000800U
#define FILT_HIGH_GIVEN 0x00001000U
#define FILT_LOW_GIVEN 0x00002000U
#define FILT_FLATWIDTH_GIVEN 0x00004000U

typedef struct {
  double level, start, end;
  unsigned long flags;
} THRESHOLD_FILTER;

typedef struct {
  double start, end;
  unsigned long flags;
} HILO_FILTER;

typedef struct {
  double center, fullwidth, flatwidth;
  unsigned long flags;
} NHBP_FILTER;

typedef struct {
  char *file, *freqName, *realName, *imagName, *magName;
  double *freqData, *realData, *imagData, *magData;
  long points;
  unsigned long flags;
} FILE_FILTER;

typedef struct {
  int32_t high, low;
  unsigned long flags;
} CLIP_FILTER;

typedef struct {
  void *filter;
  long filterType;
} FILTER;

typedef struct {
  FILTER *filter;
  long filters;
} FILTER_STAGE;

#define FL_NEWCOLUMNS 0x00001UL
#define FL_DIFCOLUMNS 0x00002UL

long applyFilters(double *outputData, double *inputData, double *timeData, int64_t rows, FILTER_STAGE *filterStage, long filterStages);
long applyFilterStage(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, FILTER_STAGE *filterStage);
void addClipFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, CLIP_FILTER *filter);
void addThresholdFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, THRESHOLD_FILTER *filter);
void addHighPassFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, HILO_FILTER *filter);
void addLowPassFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, HILO_FILTER *filter);
void addNotchFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, NHBP_FILTER *filter);
void addBandPassFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, NHBP_FILTER *filter);
void addFileFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, FILE_FILTER *filter);
int64_t computeIndex(double value, double dfrequency, int64_t frequencies);
void addFilter(FILTER_STAGE *filterStage, long optionCode, SCANNED_ARG *scanned);

int main(int argc, char **argv) {
  int iArg;
  char **outputColumn, **difColumn;
  char *indepColumn, **depenColumn, **exclude;
  long depenColumns, excludes;
  char *input, *output;
  long i, readCode, optionCode;
  int64_t rows;
  unsigned long flags, pipeFlags, majorOrderFlag;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  double *timeData, *inputData, *outputData;
  FILTER_STAGE *filterStage;
  long filterStages, totalFilters;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3 || argc > (3 + N_OPTIONS))
    bomb(NULL, USAGE);

  output = input = NULL;
  flags = pipeFlags = 0;
  indepColumn = NULL;
  depenColumn = exclude = NULL;
  depenColumns = excludes = 0;
  if (!(filterStage = (FILTER_STAGE *)calloc(1, sizeof(*filterStage))))
    SDDS_Bomb("allocation failure");

  filterStage->filter = NULL;
  filterStage->filters = 0;
  filterStages = 1;
  totalFilters = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* Process options here */
      switch (optionCode = match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 && (!scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                                                        "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                                                        "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
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
        if (indepColumn)
          SDDS_Bomb("only one -columns option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        indepColumn = scanned[iArg].list[1];
        if (scanned[iArg].n_items >= 2) {
          depenColumn = tmalloc(sizeof(*depenColumn) * (depenColumns = scanned[iArg].n_items - 2));
          for (i = 0; i < depenColumns; i++)
            depenColumn[i] = scanned[iArg].list[i + 2];
        }
        break;
      case SET_THRESHOLD:
      case SET_HIGHPASS:
      case SET_LOWPASS:
      case SET_NOTCH:
      case SET_BANDPASS:
      case SET_FILTERFILE:
      case SET_CLIPFREQ:
        addFilter(filterStage + filterStages - 1, optionCode, scanned + iArg);
        totalFilters++;
        break;
      case SET_CASCADE:
        if (filterStages == 0)
          SDDS_Bomb("-cascade option precedes all filter definitions");
        if (!(filterStage = SDDS_Realloc(filterStage, (filterStages + 1) * sizeof(*filterStage))))
          SDDS_Bomb("allocation failure");
        filterStage[filterStages].filter = NULL;
        filterStage[filterStages].filters = 0;
        filterStages++;
        break;
      case SET_NEWCOLUMNS:
        flags |= FL_NEWCOLUMNS;
        break;
      case SET_DIFFERENCECOLUMNS:
        flags |= FL_DIFCOLUMNS;
        break;
      case SET_EXCLUDE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -exclude syntax");
        moveToStringArray(&exclude, &excludes, scanned[iArg].list + 1, scanned[iArg].n_items - 1);
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s (%s)\n", scanned[iArg].list[0], argv[0]);
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

  processFilenames("sddsfdfilter", &input, &output, pipeFlags, 0, NULL);

  if (!totalFilters)
    fputs("warning: no filters specified (sddsfdfilter)\n", stderr);

  if (!indepColumn)
    SDDS_Bomb("supply the independent column name with the -columns option");

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_CheckColumn(&SDDSin, indepColumn, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY)
    exit(EXIT_FAILURE);

  excludes = appendToStringArray(&exclude, excludes, indepColumn);
  if (!depenColumns)
    depenColumns = appendToStringArray(&depenColumn, depenColumns, "*");

  if ((depenColumns = expandColumnPairNames(&SDDSin, &depenColumn, NULL, depenColumns, exclude, excludes, FIND_NUMERIC_TYPE, 0)) <= 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_Bomb("No quantities selected to filter");
  }

  if (!SDDS_InitializeCopy(&SDDSout, &SDDSin, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;

  if (flags & FL_NEWCOLUMNS) {
    outputColumn = tmalloc(sizeof(*outputColumn) * depenColumns);
    for (i = 0; i < depenColumns; i++) {
      outputColumn[i] = tmalloc(sizeof(**outputColumn) * (strlen(depenColumn[i]) + 1 + strlen("Filtered")));
      sprintf(outputColumn[i], "%sFiltered", depenColumn[i]);
      if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, depenColumn[i], outputColumn[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  } else
    outputColumn = depenColumn;

  difColumn = NULL;
  if (flags & FL_DIFCOLUMNS) {
    difColumn = tmalloc(sizeof(*difColumn) * depenColumns);
    for (i = 0; i < depenColumns; i++) {
      difColumn[i] = tmalloc(sizeof(**difColumn) * (strlen(depenColumn[i]) + 1 + strlen("Difference")));
      sprintf(difColumn[i], "%sDifference", depenColumn[i]);
      if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, depenColumn[i], difColumn[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  outputData = NULL;
  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    if (!SDDS_CopyPage(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((rows = SDDS_CountRowsOfInterest(&SDDSin)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (rows) {
      if (!(timeData = SDDS_GetColumnInDoubles(&SDDSin, indepColumn)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(outputData = SDDS_Realloc(outputData, sizeof(*outputData) * rows)))
        SDDS_Bomb("allocation failure");
      for (i = 0; i < depenColumns; i++) {
        if (!(inputData = SDDS_GetColumnInDoubles(&SDDSin, depenColumn[i])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!applyFilters(outputData, inputData, timeData, rows, filterStage, filterStages))
          exit(EXIT_FAILURE);
        if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, outputData, rows, outputColumn[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (flags & FL_DIFCOLUMNS) {
          int64_t j;
          for (j = 0; j < rows; j++)
            outputData[j] = inputData[j] - outputData[j];
          if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, outputData, rows, difColumn[i]))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        free(inputData);
      }
      free(timeData);
    }
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  free(outputData);
  for (i = 0; i < depenColumns; i++) {
    free(depenColumn[i]);
    if (flags & FL_NEWCOLUMNS)
      free(outputColumn[i]);
    if (flags & FL_DIFCOLUMNS)
      free(difColumn[i]);
  }
  for (i = 0; i < excludes; i++)
    free(exclude[i]);

  free(indepColumn);
  if (flags & FL_NEWCOLUMNS)
    free(outputColumn);
  free(depenColumn);
  if (flags & FL_DIFCOLUMNS)
    free(difColumn);
  free(exclude);
  for (i = 0; i < filterStages; i++) {
    long j;
    for (j = 0; j < filterStage[i].filters; j++) {
      switch (filterStage[i].filter[j].filterType) {
      case SET_FILTERFILE:
        free(((FILE_FILTER *)(filterStage[i].filter[j].filter))->freqData);
        free(((FILE_FILTER *)(filterStage[i].filter[j].filter))->magData);
        free(((FILE_FILTER *)(filterStage[i].filter[j].filter))->imagData);
        free(((FILE_FILTER *)(filterStage[i].filter[j].filter))->realData);
        break;
      default:
        break;
      }
    }
  }

  if (!SDDS_Terminate(&SDDSout) || !SDDS_Terminate(&SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  return EXIT_SUCCESS;
}

long applyFilters(double *outputData, double *inputData, double *timeData, int64_t rows, FILTER_STAGE *filterStage, long filterStages) {
  long stage;
  int64_t frequencies, row;
  double *realimagInput, *realimagOutput;
  double length, dfrequency, factor;
  realimagOutput = NULL;
  if (!(realimagInput = (double *)malloc(sizeof(*realimagInput) * (rows + 2))) ||
      !(realimagOutput = (double *)malloc(sizeof(*realimagOutput) * (rows + 2))))
    SDDS_Bomb("allocation failure");

  /* Input data is real and has "rows" rows */
  /* Result is interleaved real and imaginary */
  realFFT2(realimagInput, inputData, rows, 0);
  frequencies = rows / 2 + 1;

  /* Length is the assumed length of the periodic signal,
     which is one interval longer than the actual data entered */
  length = ((double)rows) * (timeData[rows - 1] - timeData[0]) / ((double)rows - 1.0);
  dfrequency = factor = 1.0 / length;

  for (stage = 0; stage < filterStages; stage++) {
    if (!applyFilterStage(realimagOutput, realimagInput, frequencies, dfrequency, filterStage + stage))
      return 0;
    SWAP_PTR(realimagOutput, realimagInput);
  }

  /* Input is still interleaved real and imaginary. The flag
     INVERSE_FFT ensures that the array is interpreted correctly. */
  /* Output is interleaved real and imaginary */
#if DEBUG
  for (row = 0; row < rows; row++) {
    fprintf(stdout, "Real: %f, Imag: %f\n", realimagInput[2 * row], realimagInput[2 * row + 1]);
  }
#endif
  realFFT2(realimagOutput, realimagInput, rows, INVERSE_FFT);

  for (row = 0; row < rows; row++)
    outputData[row] = realimagOutput[row];

  free(realimagOutput);
  free(realimagInput);

  return 1;
}

long applyFilterStage(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, FILTER_STAGE *filterStage) {
  int64_t i, ifilter;

  for (i = 0; i < 2 * frequencies; i++)
    outputRI[i] = 0;

  for (ifilter = 0; ifilter < filterStage->filters; ifilter++) {
    switch (filterStage->filter[ifilter].filterType) {
    case SET_CLIPFREQ:
      addClipFilterOutput(outputRI, inputRI, frequencies, dfrequency, filterStage->filter[ifilter].filter);
      break;
    case SET_THRESHOLD:
      addThresholdFilterOutput(outputRI, inputRI, frequencies, dfrequency, filterStage->filter[ifilter].filter);
      break;
    case SET_HIGHPASS:
      addHighPassFilterOutput(outputRI, inputRI, frequencies, dfrequency, filterStage->filter[ifilter].filter);
      break;
    case SET_LOWPASS:
      addLowPassFilterOutput(outputRI, inputRI, frequencies, dfrequency, filterStage->filter[ifilter].filter);
      break;
    case SET_NOTCH:
      addNotchFilterOutput(outputRI, inputRI, frequencies, dfrequency, filterStage->filter[ifilter].filter);
      break;
    case SET_BANDPASS:
      addBandPassFilterOutput(outputRI, inputRI, frequencies, dfrequency, filterStage->filter[ifilter].filter);
      break;
    case SET_FILTERFILE:
      addFileFilterOutput(outputRI, inputRI, frequencies, dfrequency, filterStage->filter[ifilter].filter);
      break;
    default:
      SDDS_Bomb("unknown filter type in applyFilterStage--this shouldn't happen");
      break;
    }
  }
  return 1;
}

void addClipFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, CLIP_FILTER *filter) {
  int64_t i1, i2;

  i1 = 0;
  i2 = frequencies - 1;
  if (filter->flags & FILT_HIGH_GIVEN)
    if ((i2 = frequencies - filter->high) < 0)
      i2 = 0;
  if (filter->flags & FILT_LOW_GIVEN)
    if ((i1 = filter->low) >= frequencies)
      i1 = frequencies - 1;
  for (; i1 <= i2; i1++) {
    outputRI[2 * i1] += inputRI[2 * i1];
    outputRI[2 * i1 + 1] += inputRI[2 * i1 + 1];
  }
}

void addThresholdFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, THRESHOLD_FILTER *filter) {
  int64_t i, i1, i2;
  double level2, level2q, level2o;
  double max2, mag2;

  i1 = 0;
  if (filter->flags & FILT_START_GIVEN)
    i1 = computeIndex(filter->start, dfrequency, frequencies);
  i2 = frequencies - 1;
  if (filter->flags & FILT_END_GIVEN)
    i2 = computeIndex(filter->end, dfrequency, frequencies);

  if (filter->flags & FILT_FRACTHRES_GIVEN) {
    max2 = -DBL_MAX;
    for (i = i1; i <= i2; i++)
      if ((mag2 = inputRI[2 * i] * inputRI[2 * i] + inputRI[2 * i + 1] * inputRI[2 * i + 1]) > max2)
        max2 = mag2;
    level2o = max2 * sqr(filter->level);
  } else
    level2o = sqr(filter->level);
  level2q = level2o / 4;

  for (i = i1; i <= i2; i++) {
    level2 = level2q; /* only half the amplitude is in positive frequencies */
    if (i == 0 || i == frequencies - 1)
      level2 = level2o;
    if ((inputRI[2 * i] * inputRI[2 * i] + inputRI[2 * i + 1] * inputRI[2 * i + 1]) >= level2) {
      outputRI[2 * i] += inputRI[2 * i];
      outputRI[2 * i + 1] += inputRI[2 * i + 1];
    }
  }
}

void addHighPassFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, HILO_FILTER *filter) {
  int64_t i, i1, i2;
  double fraction, dfraction;

  i1 = computeIndex(filter->start, dfrequency, frequencies);
  i2 = computeIndex(filter->end, dfrequency, frequencies);

  dfraction = i1 == i2 ? 0 : 1.0 / (i2 - i1);
  fraction = 0;
  for (i = i1; i <= i2; i++) {
    outputRI[2 * i] += inputRI[2 * i] * fraction;
    outputRI[2 * i + 1] += inputRI[2 * i + 1] * fraction;
    if ((fraction += dfraction) > 1)
      fraction = 1;
  }
  for (; i < frequencies; i++) {
    outputRI[2 * i] += inputRI[2 * i];
    outputRI[2 * i + 1] += inputRI[2 * i + 1];
  }
}

void addLowPassFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, HILO_FILTER *filter) {
  int64_t i, i1, i2;
  double fraction, dfraction;

  i1 = computeIndex(filter->start, dfrequency, frequencies);
  i2 = computeIndex(filter->end, dfrequency, frequencies);

  dfraction = i1 == i2 ? 0 : 1.0 / (i2 - i1);
  fraction = 1;
  for (i = i1; i <= i2; i++) {
    outputRI[2 * i] += inputRI[2 * i] * fraction;
    outputRI[2 * i + 1] += inputRI[2 * i + 1] * fraction;
    if ((fraction -= dfraction) < 0)
      fraction = 0;
  }
  for (i = 0; i < i1; i++) {
    outputRI[2 * i] += inputRI[2 * i];
    outputRI[2 * i + 1] += inputRI[2 * i + 1];
  }
}

void addNotchFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, NHBP_FILTER *filter) {
  int64_t i1, i2, i;
  double fraction, dfraction;

  i1 = computeIndex(filter->center - filter->fullwidth / 2, dfrequency, frequencies);
  i2 = computeIndex(filter->center - filter->flatwidth / 2, dfrequency, frequencies);
  for (i = 0; i < i1; i++) {
    outputRI[2 * i] += inputRI[2 * i];
    outputRI[2 * i + 1] += inputRI[2 * i + 1];
  }
  dfraction = i1 == i2 ? 0 : 1.0 / (i2 - i1);
  fraction = 1;
  for (i = i1; i <= i2; i++) {
    outputRI[2 * i] += inputRI[2 * i] * fraction;
    outputRI[2 * i + 1] += inputRI[2 * i + 1] * fraction;
    if ((fraction -= dfraction) < 0)
      fraction = 0;
  }

  i1 = computeIndex(filter->center + filter->flatwidth / 2, dfrequency, frequencies);
  i2 = computeIndex(filter->center + filter->fullwidth / 2, dfrequency, frequencies);
  for (i = i2; i < frequencies; i++) {
    outputRI[2 * i] += inputRI[2 * i];
    outputRI[2 * i + 1] += inputRI[2 * i + 1];
  }
  dfraction = i1 == i2 ? 0 : 1.0 / (i2 - i1);
  fraction = 0;
  for (i = i1; i <= i2; i++) {
    outputRI[2 * i] += inputRI[2 * i] * fraction;
    outputRI[2 * i + 1] += inputRI[2 * i + 1] * fraction;
    if ((fraction += dfraction) > 1)
      fraction = 1;
  }
}

void addBandPassFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, NHBP_FILTER *filter) {
  int64_t i1, i2, i;
  double fraction, dfraction;

  i1 = computeIndex(filter->center - filter->fullwidth / 2, dfrequency, frequencies);
  i2 = computeIndex(filter->center - filter->flatwidth / 2, dfrequency, frequencies);
  dfraction = i1 == i2 ? 0 : 1.0 / (i2 - i1);
  fraction = 0;
  for (i = i1; i <= i2; i++) {
    outputRI[2 * i] += inputRI[2 * i] * fraction;
    outputRI[2 * i + 1] += inputRI[2 * i + 1] * fraction;
    if ((fraction += dfraction) > 1)
      fraction = 1;
  }

  i1 = computeIndex(filter->center + filter->flatwidth / 2, dfrequency, frequencies);
  i2 = computeIndex(filter->center + filter->fullwidth / 2, dfrequency, frequencies);
  dfraction = i1 == i2 ? 0 : 1.0 / (i2 - i1);
  fraction = 1;
  for (i = i1; i <= i2; i++) {
    outputRI[2 * i] += inputRI[2 * i] * fraction;
    outputRI[2 * i + 1] += inputRI[2 * i + 1] * fraction;
    if ((fraction -= dfraction) < 0)
      fraction = 0;
  }

  i1 = computeIndex(filter->center - filter->flatwidth / 2, dfrequency, frequencies);
  i2 = computeIndex(filter->center + filter->flatwidth / 2, dfrequency, frequencies);
  for (i = i1; i <= i2; i++) {
    outputRI[2 * i] += inputRI[2 * i];
    outputRI[2 * i + 1] += inputRI[2 * i + 1];
  }
}

void addFileFilterOutput(double *outputRI, double *inputRI, int64_t frequencies, double dfrequency, FILE_FILTER *filter) {
  long code;
  int64_t i;
  double factor, ifactor, rfactor, rdata, idata, f;

  if (!filter->freqData) {
    SDDS_DATASET SDDSin;
    long readCode = 0;

    if (!SDDS_InitializeInput(&SDDSin, filter->file) || (readCode = SDDS_ReadPage(&SDDSin)) == 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (readCode < 0) {
      fprintf(stderr, "error: unable to read filter file %s (sddsfdfilter)\n", filter->file);
      exit(EXIT_FAILURE);
    }
    if ((filter->points = SDDS_CountRowsOfInterest(&SDDSin)) <= 0) {
      fprintf(stderr, "error: file %s has no data on first page (sddsfdfilter)\n", filter->file);
      exit(EXIT_FAILURE);
    }
    if (!(filter->freqData = SDDS_GetColumnInDoubles(&SDDSin, filter->freqName)) ||
        (filter->magName && !(filter->magData = SDDS_GetColumnInDoubles(&SDDSin, filter->magName))) ||
        (filter->imagName && !(filter->imagData = SDDS_GetColumnInDoubles(&SDDSin, filter->imagName))) ||
        (filter->realName && !(filter->realData = SDDS_GetColumnInDoubles(&SDDSin, filter->realName))) ||
        !SDDS_Terminate(&SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 1; i < filter->points; i++)
      if (filter->freqData[i - 1] >= filter->freqData[i]) {
        fprintf(stderr, "error: frequency data not monotonically increasing for %s (sddsfdfilter)\n", filter->file);
        exit(EXIT_FAILURE);
      }
  }
  for (i = 0; i < frequencies; i++) {
    f = i * dfrequency;
    if (filter->freqData[0] > f || filter->freqData[filter->points - 1] < f)
      continue;
    if (filter->magName) {
      factor = interp(filter->magData, filter->freqData, filter->points, f, 0, 1, &code);
      outputRI[2 * i] += factor * inputRI[2 * i];
      outputRI[2 * i + 1] += factor * inputRI[2 * i + 1];
    } else {
      rfactor = interp(filter->realData, filter->freqData, filter->points, f, 0, 1, &code);
      ifactor = interp(filter->imagData, filter->freqData, filter->points, f, 0, 1, &code);
      rdata = inputRI[2 * i];
      idata = inputRI[2 * i + 1];
      outputRI[2 * i] += rdata * rfactor - idata * ifactor;
      outputRI[2 * i + 1] += rdata * ifactor + idata * rfactor;
    }
  }
}

int64_t computeIndex(double value, double dfrequency, int64_t frequencies) {
  int64_t i1;

  i1 = value / dfrequency + 0.5;
  if (i1 < 0)
    i1 = 0;
  if (i1 > frequencies - 1)
    i1 = frequencies - 1;
  return i1;
}

void addFilter(FILTER_STAGE *filterStage, long optionCode, SCANNED_ARG *scanned) {
  THRESHOLD_FILTER *thresholdFilter;
  HILO_FILTER *hiloFilter;
  NHBP_FILTER *nhbpFilter;
  FILE_FILTER *fileFilter;
  CLIP_FILTER *clipFilter;
  FILTER *filter;
  long items, filters;
  char **item;

  if (!(filterStage->filter = SDDS_Realloc(filterStage->filter, sizeof(*filterStage->filter) * (filterStage->filters + 1))))
    SDDS_Bomb("allocation failure adding filter slot");
  filter = filterStage->filter;
  filters = filterStage->filters;
  filterStage->filters++;

  items = scanned->n_items - 1;
  item = scanned->list + 1;
  switch (filter[filters].filterType = optionCode) {
  case SET_THRESHOLD:
    if (!(thresholdFilter = filter[filters].filter = calloc(1, sizeof(THRESHOLD_FILTER))))
      SDDS_Bomb("allocation failure allocating threshold filter");
    if (items < 1)
      SDDS_Bomb("invalid -threshold syntax");
    if (!scanItemList(&thresholdFilter->flags, item, &items, 0,
                      "level", SDDS_DOUBLE, &thresholdFilter->level, 1, FILT_LEVEL_GIVEN,
                      "fractional", -1, NULL, 0, FILT_FRACTHRES_GIVEN,
                      "start", SDDS_DOUBLE, &thresholdFilter->start, 1, FILT_START_GIVEN,
                      "end", SDDS_DOUBLE, &thresholdFilter->end, 1, FILT_END_GIVEN, NULL))
      SDDS_Bomb("invalid -threshold syntax/values");
    if (!(thresholdFilter->flags & FILT_LEVEL_GIVEN))
      SDDS_Bomb("supply level=<value> qualifier for -threshold");
    if ((thresholdFilter->flags & FILT_START_GIVEN && thresholdFilter->flags & FILT_END_GIVEN) && thresholdFilter->start > thresholdFilter->end)
      SDDS_Bomb("start > end for -threshold filter");
    break;
  case SET_HIGHPASS:
  case SET_LOWPASS:
    if (!(hiloFilter = filter[filters].filter = calloc(1, sizeof(HILO_FILTER))))
      SDDS_Bomb("allocation failure allocating low-pass or high-pass filter");
    if (!scanItemList(&hiloFilter->flags, item, &items, 0,
                      "start", SDDS_DOUBLE, &hiloFilter->start, 1, FILT_START_GIVEN,
                      "end", SDDS_DOUBLE, &hiloFilter->end, 1, FILT_END_GIVEN, NULL))
      SDDS_Bomb("invalid -highpass or -lowpass syntax");
    if (!(hiloFilter->flags & FILT_START_GIVEN) || !(hiloFilter->flags & FILT_END_GIVEN))
      SDDS_Bomb("supply start=<value> and end=<value> qualifiers with -highpass and -lowpass");
    break;
  case SET_NOTCH:
  case SET_BANDPASS:
    if (!(nhbpFilter = filter[filters].filter = calloc(1, sizeof(NHBP_FILTER))))
      SDDS_Bomb("allocation failure allocating notch or bandpass filter");
    if (!scanItemList(&nhbpFilter->flags, item, &items, 0,
                      "center", SDDS_DOUBLE, &nhbpFilter->center, 1, FILT_CENTER_GIVEN,
                      "fullwidth", SDDS_DOUBLE, &nhbpFilter->fullwidth, 1, FILT_FULLWIDTH_GIVEN,
                      "flatwidth", SDDS_DOUBLE, &nhbpFilter->flatwidth, 1, FILT_FLATWIDTH_GIVEN, NULL))
      SDDS_Bomb("invalid -notch or -bandpass syntax");
    if (!(nhbpFilter->flags & FILT_CENTER_GIVEN) || !(nhbpFilter->flags & FILT_FLATWIDTH_GIVEN))
      SDDS_Bomb("supply center=<value> and flatWidth=<value> qualifiers with -notch and -bandpass");
    if (!(nhbpFilter->flags & FILT_FULLWIDTH_GIVEN))
      nhbpFilter->fullwidth = nhbpFilter->flatwidth;
    if (nhbpFilter->fullwidth < nhbpFilter->flatwidth)
      SDDS_Bomb("full width may not be less than flat width for notch/bandpass filter");
    break;
  case SET_FILTERFILE:
    if (!(fileFilter = filter[filters].filter = calloc(1, sizeof(FILE_FILTER))))
      SDDS_Bomb("allocation failure allocating file filter");
    if (!scanItemList(&fileFilter->flags, item, &items, 0,
                      "filename", SDDS_STRING, &fileFilter->file, 1, FILT_FILENAME_GIVEN,
                      "frequency", SDDS_STRING, &fileFilter->freqName, 1, FILT_FREQNAME_GIVEN,
                      "real", SDDS_STRING, &fileFilter->realName, 1, FILT_REALNAME_GIVEN,
                      "imaginary", SDDS_STRING, &fileFilter->imagName, 1, FILT_IMAGNAME_GIVEN,
                      "magnitude", SDDS_STRING, &fileFilter->magName, 1, FILT_MAGNAME_GIVEN, NULL))
      SDDS_Bomb("invalid -filterFile syntax");
    if (!(fileFilter->flags & FILT_FILENAME_GIVEN))
      SDDS_Bomb("supply filename=<string> with -filterFile");
    if (!(fileFilter->flags & FILT_FREQNAME_GIVEN))
      SDDS_Bomb("supply frequency=<columnName> with -filterFile");
    if (!(fileFilter->flags & FILT_MAGNAME_GIVEN) && !(fileFilter->flags & FILT_REALNAME_GIVEN && fileFilter->flags & FILT_IMAGNAME_GIVEN))
      SDDS_Bomb("supply either magnitude=<columnName>, or real=<columnName> and imag=<columnName>, with -filterFile");
    if (fileFilter->flags & FILT_MAGNAME_GIVEN && (fileFilter->flags & FILT_REALNAME_GIVEN || fileFilter->flags & FILT_IMAGNAME_GIVEN))
      SDDS_Bomb("magnitude=<columnName> is incompatible with real=<columnName> and imag=<columnName> for -filterFile");
    fileFilter->freqData = NULL; /* Indicates no data present */
    break;
  case SET_CLIPFREQ:
    if (!(clipFilter = filter[filters].filter = calloc(1, sizeof(CLIP_FILTER))))
      SDDS_Bomb("allocation failure allocating clip filter");
    if (!scanItemList(&clipFilter->flags, item, &items, 0,
                      "high", SDDS_LONG, &clipFilter->high, 1, FILT_HIGH_GIVEN,
                      "low", SDDS_LONG, &clipFilter->low, 1, FILT_LOW_GIVEN, NULL))
      SDDS_Bomb("invalid -clipFrequencies syntax");
    if (!(clipFilter->flags & FILT_HIGH_GIVEN) && !(clipFilter->flags & FILT_LOW_GIVEN))
      SDDS_Bomb("supply at least one of high=<number> or low=<number> with -clipFrequencies");
    break;
  default:
    SDDS_Bomb("invalid filter code---this shouldn't happen");
    break;
  }
}
