/**
 * @file sddseventhist.c
 * @brief Generates histograms for events from SDDS input files.
 *
 * @details
 * This program processes SDDS files to generate histograms for events identified by a specific column. 
 * It allows for binning, normalization, and overlap analysis between event histograms. 
 * The program supports multiple options for customizing histogram generation, including bin size, limits, 
 * and normalization strategies. Input can be specified as files or through pipes.
 *
 * @section Usage
 * ```
 * sddseventhist [<inputfile>] [<outputfile>]
 *               [-pipe=<input>,<output>]
 *                -dataColumn=<columnName>
 *                -eventIdentifier=<columnName>
 *               [-overlapEvent=<eventValue>]
 *               [-bins=<number> | -sizeOfBins=<value>]
 *               [-lowerLimit=<value>]
 *               [-upperLimit=<value>]
 *               [-sides]
 *               [-normalize[={sum|area|peak}]]
 *               [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required           | Description                                                                |
 * |--------------------|----------------------------------------------------------------------------|
 * | `-dataColumn`      | Specifies the column to generate histograms for.                          |
 * | `-eventIdentifier` | Specifies the column identifying unique events for separate histograms.   |
 *
 * | Option             | Description                                                                |
 * |--------------------|----------------------------------------------------------------------------|
 * | `-pipe`            | Use pipe for input and/or output.                                          |
 * | `-overlapEvent`    | Multiplies histograms with the specified event's histogram.               |
 * | `-bins`            | Number of bins for the histogram.                                         |
 * | `-sizeOfBins`      | Size of each bin.                                                         |
 * | `-lowerLimit`      | Lower limit for the histogram range.                                      |
 * | `-upperLimit`      | Upper limit for the histogram range.                                      |
 * | `-sides`           | Adds sides to the histogram extending to the zero level.                 |
 * | `-normalize`       | Normalizes the histogram by sum, area, or peak value.                    |
 * | `-majorOrder`      | Specifies the major order of data output: `row` or `column`.             |
 *
 * @subsection Incompatibilities
 * - `-bins` and `-sizeOfBins` cannot be used together.
 * - The `-normalize` option defaults to `sum` if no specific value is provided.
 * 
 * @subsection Requirements
 * - `-eventIdentifier` must reference a string or integer column.
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
 * M. Borland, R. Soliday, Xuesong Jiao, H. Shang
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
  SET_BINSIZE,
  SET_NORMALIZE,
  SET_SIDES,
  SET_PIPE,
  SET_EVENTIDENTIFIER,
  SET_OVERLAPEVENT,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "bins",
  "lowerlimit",
  "upperlimit",
  "datacolumn",
  "sizeofbins",
  "normalize",
  "sides",
  "pipe",
  "eventidentifier",
  "overlapevent",
  "majorOrder",
};

/* Improved usage message for better readability */
char *USAGE =
  "sddseventhist [<inputfile>] [<outputfile>]\n"
  "              [-pipe=<input>,<output>]\n"
  "               -dataColumn=<columnName>\n"
  "               -eventIdentifier=<columnName>\n"
  "              [-overlapEvent=<eventValue>]\n"
  "              [-bins=<number> | -sizeOfBins=<value>]\n"
  "              [-lowerLimit=<value>]\n"
  "              [-upperLimit=<value>]\n"
  "              [-sides]\n"
  "              [-normalize[={sum|area|peak}]]\n"
  "              [-majorOrder=row|column]\n";

static char *additional_help = "\n\
dataColumn       : Name of the column to histogram.\n\
eventIdentifier  : Name of the column used to identify events.\n\
                   A separate histogram is created for each unique value in this column.\n\
                   The column must contain string or integer data;\n\
                   if string data, the values must be valid SDDS column names.\n\
overlapEvent     : If specified, histograms are multiplied bin-by-bin with this event's histogram.\n\
bins             : Number of bins for the histogram.\n\
sizeOfBins       : Size of each bin for the histogram.\n\
lowerLimit       : Lower limit of the histogram range.\n\
upperLimit       : Upper limit of the histogram range.\n\
normalize        : Normalize the histogram by sum, area, or peak.\n\
sides            : Adds sides to the histogram down to the zero level.\n\
majorOrder       : Specifies the major order for the output file (row or column).\n\n\
Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define NORMALIZE_PEAK 0
#define NORMALIZE_AREA 1
#define NORMALIZE_SUM 2
#define NORMALIZE_NO 3
#define N_NORMALIZE_OPTIONS 4

char *normalize_option[N_NORMALIZE_OPTIONS] = {
  "peak", "area", "sum", "no"};

typedef struct
{
  char *string;
  int64_t events;
  double *data;
  /* Indices in output file of histogram and overlap */
  long histogramIndex, overlapIndex;
} EVENT_DATA;

typedef struct
{
  char *string;
  double data;
} EVENT_PAIR;

static long setupOutputFile(SDDS_DATASET *outTable, char *outputfile, SDDS_DATASET *inTable,
                            char *inputfile, char *dataColumn, char *eventIDColumn, char *overlapEventID,
                            EVENT_DATA **eventDataRet, int64_t *eventIDsRet, double **dataRet, long bins,
                            double binSize, long normalizeMode, short columnMajorOrder);
long makeEventHistogram(double *hist, long bins, double lowerLimit, double dx, EVENT_DATA *eventRefData);
void makeEventOverlap(double *overlap, double *hist, double *overlapHist, long bins);
int event_cmpasc(const void *ep1, const void *ep2);

int main(int argc, char **argv) {
  /* Flags to keep track of what is set in command line */
  long binsGiven, lowerLimitGiven, upperLimitGiven;
  SDDS_DATASET inTable, outTable;
  double *data;  /* Pointer to the array to histogram */
  double *hist;  /* To store the histogram */
  double *indep; /* To store values of bin centers */
  double *overlap, *overlapHist;
  double lowerLimit, upperLimit;           /* Lower and upper limits in histogram */
  double givenLowerLimit, givenUpperLimit; /* Given lower and upper limits */
  double range, binSize;
  long bins; /* Number of bins in the histogram */
  char *dataColumn, *eventIDColumn, *overlapEventID;
  SCANNED_ARG *scanned;         /* Scanned argument structure */
  char *inputfile, *outputfile; /* Input and output file names */
  double dx;                    /* Spacing of bins in histogram */
  long pointsBinned;            /* Number of points that are in histogram */
  long normalizeMode, doSides, verbose = 0, readCode;
  unsigned long pipeFlags, majorOrderFlag;
  long eventIDIndex, eventIDType;
  int64_t eventRefIDs = 0;
  EVENT_DATA *eventRefData = NULL;
  /*char **eventID; */
  int64_t i, points, iEvent;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "Usage: %s\n", USAGE);
    fputs(additional_help, stderr);
    exit(EXIT_FAILURE);
  }

  hist = overlap = overlapHist = NULL;
  binsGiven = lowerLimitGiven = upperLimitGiven = 0;
  binSize = doSides = 0;
  inputfile = outputfile = NULL;
  dataColumn = eventIDColumn = overlapEventID = NULL;
  normalizeMode = NORMALIZE_NO;
  pipeFlags = 0;

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
      case SET_BINS: /* set number of bins */
        if (binsGiven)
          SDDS_Bomb("-bins specified more than once");
        binsGiven = 1;
        if (sscanf(scanned[i].list[1], "%ld", &bins) != 1 || bins <= 0)
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
      case SET_DATACOLUMN:
        if (dataColumn)
          SDDS_Bomb("-dataColumn specified more than once");
        if (scanned[i].n_items != 2)
          SDDS_Bomb("invalid -dataColumn syntax---supply name");
        dataColumn = scanned[i].list[1];
        break;
      case SET_EVENTIDENTIFIER:
        if (eventIDColumn)
          SDDS_Bomb("-eventIdentifier specified more than once");
        if (scanned[i].n_items != 2)
          SDDS_Bomb("invalid -eventIdentifier syntax---supply name");
        eventIDColumn = scanned[i].list[1];
        break;
      case SET_OVERLAPEVENT:
        if (overlapEventID)
          SDDS_Bomb("-overlapEvent specified more than once");
        if (scanned[i].n_items != 2)
          SDDS_Bomb("invalid -overlapEvent syntax---supply value");
        overlapEventID = scanned[i].list[1];
        if (!strlen(overlapEventID))
          SDDS_Bomb("invalid -overlapEvent syntax---supply value");
        break;
      case SET_NORMALIZE:
        if (scanned[i].n_items == 1)
          normalizeMode = NORMALIZE_SUM;
        else if (scanned[i].n_items != 2 || (normalizeMode = match_string(scanned[i].list[1], normalize_option, N_NORMALIZE_OPTIONS, 0)) < 0)
          SDDS_Bomb("invalid -normalize syntax");
        break;
      case SET_SIDES:
        doSides = 1;
        break;
      case SET_BINSIZE:
        if (sscanf(scanned[i].list[1], "%le", &binSize) != 1 || binSize <= 0)
          SDDS_Bomb("invalid value for bin size");
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[i].list + 1, scanned[i].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
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
        SDDS_Bomb("Too many filenames provided.");
    }
  }

  processFilenames("sddseventhist", &inputfile, &outputfile, pipeFlags, 0, NULL);

  if (binSize && binsGiven)
    SDDS_Bomb("Specify either -binSize or -bins, not both.");
  if (!binsGiven)
    bins = 20;
  if (!dataColumn)
    SDDS_Bomb("-dataColumn must be specified.");
  if (!eventIDColumn)
    SDDS_Bomb("-eventIdentifier must be specified.");

  if (!(indep = SDDS_Malloc(sizeof(*indep) * (bins + 2))) ||
      !(hist = SDDS_Malloc(sizeof(*hist) * (bins + 2))) ||
      !(overlap = SDDS_Malloc(sizeof(*overlap) * (bins + 2))) ||
      !(overlapHist = SDDS_Malloc(sizeof(*overlapHist) * (bins + 2))))
    SDDS_Bomb("Memory allocation failure.");

  if (!SDDS_InitializeInput(&inTable, inputfile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_GetColumnIndex(&inTable, dataColumn) < 0)
    SDDS_Bomb("Data column not found.");
  if ((eventIDIndex = SDDS_GetColumnIndex(&inTable, eventIDColumn)) < 0)
    SDDS_Bomb("Event ID column not found.");
  if ((eventIDType = SDDS_GetColumnType(&inTable, eventIDIndex)) != SDDS_STRING)
    SDDS_Bomb("Event ID column must be of string type.");
  if (!SDDS_NUMERIC_TYPE(SDDS_GetNamedColumnType(&inTable, dataColumn)))
    SDDS_Bomb("Data column must be of a numeric data type.");

  data = NULL;
  while ((readCode = SDDS_ReadPage(&inTable)) > 0) {
    if (readCode > 1)
      SDDS_Bomb("This program cannot process multi-page files.");
    pointsBinned = 0;
    if ((points = SDDS_CountRowsOfInterest(&inTable)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!points)
      SDDS_Bomb("No data found in the file.");

    if (!setupOutputFile(&outTable, outputfile, &inTable, inputfile, dataColumn, eventIDColumn, overlapEventID, &eventRefData, &eventRefIDs, &data, bins, binSize, normalizeMode, columnMajorOrder))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!lowerLimitGiven) {
      lowerLimit = 0;
      if (points)
        lowerLimit = data[0];
      for (i = 0; i < points; i++)
        if (lowerLimit > data[i])
          lowerLimit = data[i];
    } else {
      lowerLimit = givenLowerLimit;
    }

    if (!upperLimitGiven) {
      upperLimit = 0;
      if (points)
        upperLimit = data[0];
      for (i = 0; i < points; i++)
        if (upperLimit < data[i])
          upperLimit = data[i];
    } else {
      upperLimit = givenUpperLimit;
    }

    free(data);
    data = NULL;
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
        bins = 2;
      if (!(indep = SDDS_Realloc(indep, sizeof(*indep) * (bins + 2))) ||
          !(hist = SDDS_Realloc(hist, sizeof(*hist) * (bins + 2))) ||
          !(overlap = SDDS_Realloc(overlap, sizeof(*overlap) * (bins + 2))) ||
          !(overlapHist = SDDS_Realloc(overlapHist, sizeof(*overlapHist) * (bins + 2))))
        SDDS_Bomb("Memory allocation failure.");
    }

    for (i = -1; i < bins + 1; i++) {
      indep[i + 1] = (i + 0.5) * dx + lowerLimit;
    }
    if (!SDDS_StartPage(&outTable, points ? (doSides ? bins + 2 : bins) : 0) ||
        !SDDS_CopyParameters(&outTable, &inTable) ||
        (points && (!SDDS_SetColumnFromDoubles(&outTable, SDDS_SET_BY_NAME, indep + (doSides ? 0 : 1), doSides ? bins + 2 : bins, dataColumn))))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (overlapEventID) {
      for (iEvent = 0; iEvent < eventRefIDs; iEvent++) {
        if (strcmp(eventRefData[iEvent].string, overlapEventID) == 0)
          break;
      }
      if (iEvent == eventRefIDs)
        SDDS_Bomb("Cannot create overlap as the specified overlap event is not present.");
      makeEventHistogram(overlapHist, bins, lowerLimit, dx, eventRefData + iEvent);
    }

    for (iEvent = pointsBinned = 0; iEvent < eventRefIDs; iEvent++) {
      pointsBinned += makeEventHistogram(hist, bins, lowerLimit, dx, eventRefData + iEvent);
      if (normalizeMode != NORMALIZE_NO) {
        double norm = 0;
        switch (normalizeMode) {
        case NORMALIZE_PEAK:
          norm = max_in_array(hist, bins);
          break;
        case NORMALIZE_AREA:
        case NORMALIZE_SUM:
          for (i = 0; i < bins; i++)
            norm += hist[i];
          if (normalizeMode == NORMALIZE_AREA)
            norm *= dx;
          break;
        default:
          SDDS_Bomb("Invalid normalization mode.");
          break;
        }
        if (norm)
          for (i = 0; i < bins; i++)
            hist[i] /= norm;
      }

      if (!SDDS_SetColumnFromDoubles(&outTable, SDDS_SET_BY_INDEX, hist + (doSides ? 0 : 1), doSides ? bins + 2 : bins, eventRefData[iEvent].histogramIndex))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (overlapEventID) {
        makeEventOverlap(overlap, hist, overlapHist, bins + 2);
        if (!SDDS_SetColumnFromDoubles(&outTable, SDDS_SET_BY_INDEX, overlap + (doSides ? 0 : 1), doSides ? bins + 2 : bins, eventRefData[iEvent].overlapIndex))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      free(eventRefData[iEvent].data);
    }

    if (!SDDS_SetParameters(&outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "sddseventhistBins", bins,
                            "sddseventhistBinSize", dx,
                            "sddseventhistPointsBinned", pointsBinned,
                            NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (verbose)
      fprintf(stderr, "%ld points of %" PRId64 " from page %ld histogrammed in %ld bins\n", pointsBinned, points, readCode, bins);

    if (!SDDS_WritePage(&outTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    free(eventRefData);
  }

  if (!SDDS_Terminate(&inTable)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }
  if (!SDDS_Terminate(&outTable)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  free(hist);
  free(overlapHist);
  free(overlap);
  free(indep);
  return EXIT_SUCCESS;
}

long setupOutputFile(SDDS_DATASET *outTable, char *outputfile, SDDS_DATASET *inTable,
                     char *inputfile, char *dataColumn, char *eventIDColumn, char *overlapEventID,
                     EVENT_DATA **eventDataRet, int64_t *eventIDsRet, double **dataRet, long bins,
                     double binSize, long normalizeMode, short columnMajorOrder) {
  char **eventValue, buffer[SDDS_MAXLINE];
  int64_t eventRows, uniqueRows, row;
  EVENT_DATA *eventData;
  EVENT_PAIR *eventPair;
  double *eventDataValue;
  int64_t row0, iEvent, drow;

  if (!SDDS_InitializeOutput(outTable, SDDS_BINARY, 0, NULL, "sddseventhist output", outputfile) ||
      !SDDS_TransferColumnDefinition(outTable, inTable, dataColumn, NULL) ||
      (eventRows = SDDS_RowCount(inTable)) == 0 ||
      !(eventValue = SDDS_GetColumn(inTable, eventIDColumn)) ||
      !(*dataRet = eventDataValue = SDDS_GetColumnInDoubles(inTable, dataColumn)))
    return 0;
  if (columnMajorOrder != -1)
    outTable->layout.data_mode.column_major = columnMajorOrder;
  else
    outTable->layout.data_mode.column_major = inTable->layout.data_mode.column_major;

  if (!(eventPair = SDDS_Malloc(sizeof(*eventPair) * eventRows)))
    SDDS_Bomb("Memory allocation failure.");

  /* Populate eventPair with event strings and data */
  for (row = 0; row < eventRows; row++) {
    eventPair[row].string = eventValue[row];
    eventPair[row].data = eventDataValue[row];
  }
  qsort(eventPair, eventRows, sizeof(*eventPair), event_cmpasc);

  /* Count unique events */
  uniqueRows = 1;
  for (row = 1; row < eventRows; row++) {
    if (strcmp(eventPair[row - 1].string, eventPair[row].string) != 0)
      uniqueRows++;
  }
  *eventIDsRet = uniqueRows;

  /* Allocate and populate EVENT_DATA structures */
  if (!(eventData = *eventDataRet = SDDS_Malloc(sizeof(**eventDataRet) * uniqueRows)))
    SDDS_Bomb("Memory allocation failure.");
  row0 = 0;
  iEvent = 0;
  for (row = 1; row < eventRows; row++) {
    if (row == (eventRows - 1) || strcmp(eventPair[row].string, eventPair[row0].string) != 0) {
      /* [row0, row-1] have the same event name */
      eventData[iEvent].events = row - row0;
      if (!SDDS_CopyString(&eventData[iEvent].string, eventPair[row0].string) ||
          !(eventData[iEvent].data = SDDS_Malloc(sizeof(*eventData[iEvent].data) * eventData[iEvent].events)))
        SDDS_Bomb("Memory allocation failure");
      for (drow = 0; drow < eventData[iEvent].events; drow++) {
        eventData[iEvent].data[drow] = eventPair[row0 + drow].data;
        /* free(eventPair[row0+drow].string); */
      }
      if (row == (eventRows - 1) && strcmp(eventPair[row].string, eventPair[row0].string) != 0) {
        /* this is necessary to pick up the last event if it is a loner */
        row0 = row;
        row--;
      } else {
        row0 = row;
      }
      iEvent++;
    }
  }

  free(eventPair);
  for (row = 0; row < eventRows; row++)
    free(eventValue[row]);
  free(eventValue);
  if (iEvent != uniqueRows)
    SDDS_Bomb("Unique row count mismatch.");

  if (overlapEventID && strlen(overlapEventID)) {
    for (iEvent = 0; iEvent < uniqueRows; iEvent++) {
      if (strcmp(overlapEventID, eventData[iEvent].string) == 0)
        break;
    }
    if (iEvent == uniqueRows)
      SDDS_Bomb("Overlap event not found.");
  }

  for (row = 0; row < uniqueRows; row++) {
    snprintf(buffer, sizeof(buffer), "%sFrequency", eventData[row].string);
    if ((eventData[row].histogramIndex = SDDS_DefineColumn(outTable, buffer, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    eventData[row].overlapIndex = -1;
    if (overlapEventID) {
      snprintf(buffer, sizeof(buffer), "%s.%sOverlap", eventData[row].string, overlapEventID);
      if ((eventData[row].overlapIndex = SDDS_DefineColumn(outTable, buffer, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
        return 0;
    }
  }

  if (SDDS_DefineParameter(outTable, "sddseventhistInput", NULL, NULL, NULL, NULL, SDDS_STRING,
                           inputfile) < 0 ||
      SDDS_DefineParameter(outTable, "sddseventhistBins", NULL, NULL, NULL, NULL, SDDS_LONG,
                           NULL) < 0 ||
      SDDS_DefineParameter(outTable, "sddseventhistBinSize", NULL, NULL, NULL, NULL,
                           SDDS_DOUBLE, NULL) < 0 ||
      SDDS_DefineParameter(outTable, "sddseventhistPointsBinned", NULL, NULL, NULL, NULL, SDDS_LONG,
                           NULL) < 0 ||
      SDDS_DefineParameter(outTable, "sddseventhistEventIDColumn", NULL, NULL, NULL, NULL,
                           SDDS_STRING, eventIDColumn) < 0 ||
      SDDS_DefineParameter(outTable, "sddshistNormMode", NULL, NULL, NULL, NULL, SDDS_STRING, normalize_option[normalizeMode]) < 0 ||
      !SDDS_TransferAllParameterDefinitions(outTable, inTable, SDDS_TRANSFER_KEEPOLD) || !SDDS_WriteLayout(outTable))
    return 0;
  return 1;
}

long makeEventHistogram(double *hist, long bins, double lowerLimit, double dx, EVENT_DATA *eventRefData) {
  long iBin, pointsBinned;
  int64_t iRow;
  double *hist1;
  hist1 = hist + 1;
  for (iBin = 0; iBin <= bins + 1; iBin++)
    hist[iBin] = 0;
  for (iRow = 0; iRow < eventRefData->events; iRow++) {
    iBin = (eventRefData->data[iRow] - lowerLimit) / dx;
    if (iBin >= 0 && iBin < bins)
      hist1[iBin] += 1;
  }
  for (iBin = pointsBinned = 0; iBin < bins; iBin++)
    pointsBinned += hist1[iBin];
  return pointsBinned;
}

void makeEventOverlap(double *overlap, double *hist, double *overlapHist, long bins) {
  long i;
  for (i = 0; i < bins; i++) {
    overlap[i] = fmin(hist[i], overlapHist[i]);
  }
}

int event_cmpasc(const void *ep1, const void *ep2) {
  EVENT_PAIR *ev1, *ev2;
  int comp;
  ev1 = (EVENT_PAIR *)ep1;
  ev2 = (EVENT_PAIR *)ep2;
  if ((comp = strcmp(ev1->string, ev2->string)) != 0)
    return comp;
  if (ev1->data < ev2->data)
    return -1;
  else if (ev1->data > ev2->data)
    return 1;
  else
    return 0;
}
