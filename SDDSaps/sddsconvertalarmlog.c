/**
 * @file sddsconvertalarmlog.c
 * @brief Convert SDDS alarm log files to an explicit, array-free form.
 *
 * sddsalarmlog normally stores control names, alarm status strings, and
 * alarm severity strings in SDDS arrays, with row data holding integer
 * indices into those arrays.  This program dereferences those indices and
 * writes explicit string columns, which makes the output usable by tools
 * that don't handle SDDS arrays.
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

enum option_type {
  SET_BINARY,
  SET_ASCII,
  SET_FLOAT,
  SET_DOUBLE,
  SET_SNAPSHOT,
  SET_PIPE,
  SET_MININTERVAL,
  SET_TIME,
  SET_DELETE,
  SET_RETAIN,
  N_OPTIONS
};

enum output_column {
  COL_TIME,
  COL_PREVIOUS_ROW,
  COL_TIME_OF_DAY_WS,
  COL_TIME_OF_DAY,
  COL_CONTROL_NAME,
  COL_ALARM_STATUS,
  COL_ALARM_SEVERITY,
  COL_DESCRIPTION,
  COL_RELATED_CONTROL_NAME,
  COL_DURATION,
  COL_RELATED_VALUE_STRING,
  N_OUTPUT_COLUMNS
};

typedef struct {
  int used;
  double time;
  int32_t previousRow;
  float timeOfDayWS;
  float timeOfDay;
  char *controlName;
  char *alarmStatus;
  char *alarmSeverity;
  char *description;
  char *relatedControlName;
  float duration;
  char *relatedValueString;
} ALARM_EVENT;

#if !defined(MAXDOUBLE)
#  define MAXDOUBLE 1.79769313486231570e+308
#endif

static char *option[N_OPTIONS] = {
  "binary",
  "ascii",
  "float",
  "double",
  "snapshot",
  "pipe",
  "minimuminterval",
  "time",
  "delete",
  "retain"};

static const char *outputColumnName[N_OUTPUT_COLUMNS] = {
  "Time",
  "PreviousRow",
  "TimeOfDayWS",
  "TimeOfDay",
  "ControlName",
  "AlarmStatus",
  "AlarmSeverity",
  "Description",
  "RelatedControlName",
  "Duration",
  "RelatedValueString"};

static char *USAGE =
  "Usage: sddsconvertalarmlog [<input-file>] [<output-file>]\n"
  "                           [-pipe=[input][,output]]\n"
  "                           [-binary | -ascii]\n"
  "                           [-float | -double]\n"
  "                           [-minimumInterval=<seconds>]\n"
  "                           [-snapshot=<epochtime>]\n"
  "                           [-time=[start=<epochtime>][,end=<epochtime>]]\n"
  "                           [-delete=<column-names>]\n"
  "                           [-retain=<column-names>]\n"
  "\nOptions:\n"
  "  -pipe=[input][,output]                  Use pipe for input and/or output.\n"
  "  -binary                                 Output in binary format.\n"
  "  -ascii                                  Output in ASCII format.\n"
  "  -double                                 Use double precision for the computed Time column.\n"
  "  -float                                  Use float precision for the computed Time column.\n"
  "  -minimumInterval=<seconds>              Set minimum interval between output rows.\n"
  "  -snapshot=<epochtime>                   Output the latest alarm state for each PV at the epoch time.\n"
  "  -time=[start=<epochtime>][,end=<epochtime>]\n"
  "                                          Filter data by epoch time range.\n"
  "  -delete={<matching-string>[,...]}       Delete output columns matching the pattern.\n"
  "  -retain={<matching-string>[,...]}       Retain only output columns matching the pattern.\n"
  "\nProgram by Robert Soliday. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static int NameMatchesList(const char *name, char **list, long items);
static int NameSelected(const char *name, char **deleteName, long deleteNames, char **retainName, long retainNames);
static int DefineOutputColumns(SDDS_DATASET *outSet, int32_t *outIndex, int *available, long outputType);
static int EnsureRows(SDDS_DATASET *outSet, int64_t *allocatedRows, int64_t neededRows);
static int WriteAlarmEvent(SDDS_DATASET *outSet, int32_t *outIndex, int64_t row, ALARM_EVENT *event);
static double ComputeEpochTime(double timeOfDay, double startTime, double startHour);
static char *ArrayString(SDDS_ARRAY *array, int32_t index);
static char *StringColumnValue(char **column, int64_t row);
static void ClearAlarmEvent(ALARM_EVENT *event);
static int CopyStringOrBlank(char **target, const char *source);
static int SaveSnapshotEvent(ALARM_EVENT **snapshotEvent, int64_t *snapshotEvents, ALARM_EVENT *event);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_input, SDDS_output;
  SCANNED_ARG *s_arg;
  char *input, *output;
  char **retainName, **deleteName;
  long retainNames, deleteNames;
  long i, i_arg, noWarnings, tmpfile_used;
  unsigned long pipeFlags, flags;
  long ascii_output, binary_output, outputType;
  long snapshot;
  double snapshotTime, minimumInterval, previousOutputTime, startTime, endTime;
  short filterTime;
  int32_t outIndex[N_OUTPUT_COLUMNS];
  int available[N_OUTPUT_COLUMNS];
  int hasIndexedData, hasExplicitData, copiedParameters;
  int hasControlNameString, hasAlarmStatusString, hasAlarmSeverityString;
  int hasDescriptionString, hasRelatedControlNameString;
  int hasControlName, hasAlarmStatus, hasAlarmSeverity;
  int hasDescription, hasRelatedControlName, hasDuration, hasRelatedValueString;
  int hasPreviousRow, hasTimeOfDayWS, hasTimeOfDay, hasTime;
  int64_t allocatedRows, outputRows, snapshotEvents;
  ALARM_EVENT *snapshotEvent;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  input = output = NULL;
  retainName = deleteName = NULL;
  retainNames = deleteNames = 0;
  noWarnings = tmpfile_used = 0;
  pipeFlags = 0;
  ascii_output = binary_output = 0;
  outputType = SDDS_DOUBLE;
  snapshot = 0;
  snapshotTime = 0;
  minimumInterval = -1;
  previousOutputTime = -MAXDOUBLE;
  filterTime = 0;
  startTime = -MAXDOUBLE;
  endTime = MAXDOUBLE;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_BINARY:
        binary_output = 1;
        ascii_output = 0;
        break;
      case SET_ASCII:
        ascii_output = 1;
        binary_output = 0;
        break;
      case SET_FLOAT:
        outputType = SDDS_FLOAT;
        break;
      case SET_DOUBLE:
        outputType = SDDS_DOUBLE;
        break;
      case SET_SNAPSHOT:
        if (s_arg[i_arg].n_items < 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &snapshotTime) != 1) {
          fprintf(stderr, "invalid -snapshot syntax or value\n");
          exit(EXIT_FAILURE);
        }
        snapshot = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "invalid -pipe syntax\n");
          exit(EXIT_FAILURE);
        }
        break;
      case SET_MININTERVAL:
        if (s_arg[i_arg].n_items < 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &minimumInterval) != 1) {
          fprintf(stderr, "invalid -minimumInterval syntax or value\n");
          exit(EXIT_FAILURE);
        }
        break;
      case SET_TIME:
        s_arg[i_arg].n_items -= 1;
        filterTime = 1;
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "start", SDDS_DOUBLE, &startTime, 1, 0,
                          "end", SDDS_DOUBLE, &endTime, 1, 0, NULL)) {
          fprintf(stderr, "invalid -time syntax\n");
          exit(EXIT_FAILURE);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case SET_RETAIN:
        retainName = trealloc(retainName, sizeof(*retainName) * (retainNames + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          retainName[i - 1 + retainNames] = s_arg[i_arg].list[i];
        retainNames += s_arg[i_arg].n_items - 1;
        break;
      case SET_DELETE:
        deleteName = trealloc(deleteName, sizeof(*deleteName) * (deleteNames + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          deleteName[i - 1 + deleteNames] = s_arg[i_arg].list[i];
        deleteNames += s_arg[i_arg].n_items - 1;
        break;
      default:
        fprintf(stderr, "Error (%s): unknown switch: %s\n", argv[0], s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    } else {
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "too many filenames\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  if (snapshot && minimumInterval >= 0) {
    fprintf(stderr, "-snapshot and -minimumInterval options cannot be used together\n");
    exit(EXIT_FAILURE);
  }
  if (snapshot && filterTime) {
    fprintf(stderr, "-snapshot and -time options cannot be used together\n");
    exit(EXIT_FAILURE);
  }

  processFilenames("sddsconvertalarmlog", &input, &output, pipeFlags, noWarnings, &tmpfile_used);

  if (!SDDS_InitializeInput(&SDDS_input, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  hasControlNameString = SDDS_CheckArray(&SDDS_input, "ControlNameString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasAlarmStatusString = SDDS_CheckArray(&SDDS_input, "AlarmStatusString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasAlarmSeverityString = SDDS_CheckArray(&SDDS_input, "AlarmSeverityString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasDescriptionString = SDDS_CheckArray(&SDDS_input, "DescriptionString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasRelatedControlNameString = SDDS_CheckArray(&SDDS_input, "RelatedControlNameString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasControlName = SDDS_CheckColumn(&SDDS_input, "ControlName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasAlarmStatus = SDDS_CheckColumn(&SDDS_input, "AlarmStatus", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasAlarmSeverity = SDDS_CheckColumn(&SDDS_input, "AlarmSeverity", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasDescription = SDDS_CheckColumn(&SDDS_input, "Description", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasRelatedControlName = SDDS_CheckColumn(&SDDS_input, "RelatedControlName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasDuration = SDDS_CheckColumn(&SDDS_input, "Duration", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK;
  hasRelatedValueString = SDDS_CheckColumn(&SDDS_input, "RelatedValueString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK;
  hasPreviousRow = SDDS_CheckColumn(&SDDS_input, "PreviousRow", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK;
  hasTimeOfDayWS = SDDS_CheckColumn(&SDDS_input, "TimeOfDayWS", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK;
  hasTimeOfDay = SDDS_CheckColumn(&SDDS_input, "TimeOfDay", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK;
  hasTime = SDDS_CheckColumn(&SDDS_input, "Time", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK;

  hasIndexedData = hasControlNameString && hasAlarmStatusString && hasAlarmSeverityString &&
                   SDDS_CheckColumn(&SDDS_input, "ControlNameIndex", NULL, SDDS_ANY_INTEGER_TYPE, NULL) == SDDS_CHECK_OK &&
                   SDDS_CheckColumn(&SDDS_input, "AlarmStatusIndex", NULL, SDDS_ANY_INTEGER_TYPE, NULL) == SDDS_CHECK_OK &&
                   SDDS_CheckColumn(&SDDS_input, "AlarmSeverityIndex", NULL, SDDS_ANY_INTEGER_TYPE, NULL) == SDDS_CHECK_OK;
  hasExplicitData = hasControlName && hasAlarmStatus && hasAlarmSeverity;
  if (!hasIndexedData && !hasExplicitData) {
    fprintf(stderr, "Error: input file has neither indexed alarm-log data nor explicit alarm-log columns.\n");
    fprintf(stderr, "       Expected ControlNameString/AlarmStatusString/AlarmSeverityString arrays with index columns,\n");
    fprintf(stderr, "       or ControlName/AlarmStatus/AlarmSeverity string columns.\n");
    exit(EXIT_FAILURE);
  }
  if (!hasTime && !hasTimeOfDay) {
    fprintf(stderr, "Error: TimeOfDay column is missing and no Time column is available.\n");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < N_OUTPUT_COLUMNS; i++) {
    available[i] = NameSelected(outputColumnName[i], deleteName, deleteNames, retainName, retainNames);
    outIndex[i] = -1;
  }
  available[COL_PREVIOUS_ROW] = available[COL_PREVIOUS_ROW] && hasPreviousRow;
  available[COL_TIME_OF_DAY_WS] = available[COL_TIME_OF_DAY_WS] && hasTimeOfDayWS;
  available[COL_TIME_OF_DAY] = available[COL_TIME_OF_DAY] && hasTimeOfDay;
  available[COL_DESCRIPTION] = available[COL_DESCRIPTION] && (hasDescription || (hasIndexedData && hasDescriptionString));
  available[COL_RELATED_CONTROL_NAME] = available[COL_RELATED_CONTROL_NAME] && (hasRelatedControlName || (hasIndexedData && hasRelatedControlNameString));
  available[COL_DURATION] = available[COL_DURATION] && hasDuration;
  available[COL_RELATED_VALUE_STRING] = available[COL_RELATED_VALUE_STRING] && hasRelatedValueString;

  if (!SDDS_InitializeOutput(&SDDS_output,
                             ascii_output ? SDDS_ASCII : (binary_output ? SDDS_BINARY : SDDS_input.layout.data_mode.mode),
                             1, NULL, NULL, output) ||
      !SDDS_TransferAllParameterDefinitions(&SDDS_output, &SDDS_input, 0) ||
      !DefineOutputColumns(&SDDS_output, outIndex, available, outputType) ||
      !SDDS_WriteLayout(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  allocatedRows = 0;
  outputRows = 0;
  copiedParameters = 0;
  snapshotEvent = NULL;
  snapshotEvents = 0;

  if (!SDDS_StartTable(&SDDS_output, 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  while (SDDS_ReadTable(&SDDS_input) > 0) {
    int64_t row, rows;
    double pageStartTime, pageStartHour;
    double *timeData, *timeOfDayData, *timeOfDayWSData, *durationData, *previousRowData;
    int32_t *controlNameIndexData, *alarmStatusIndexData, *alarmSeverityIndexData;
    char **controlNameData, **alarmStatusData, **alarmSeverityData;
    char **descriptionData, **relatedControlNameData, **relatedValueStringData;
    SDDS_ARRAY *controlNameArray, *alarmStatusArray, *alarmSeverityArray;
    SDDS_ARRAY *descriptionArray, *relatedControlNameArray;

    rows = SDDS_RowCount(&SDDS_input);
    if (!copiedParameters) {
      if (!SDDS_CopyParameters(&SDDS_output, &SDDS_input)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      copiedParameters = 1;
    }

    pageStartTime = pageStartHour = 0;
    if (!hasTime &&
        (!SDDS_GetParameterAsDouble(&SDDS_input, "StartTime", &pageStartTime) ||
         !SDDS_GetParameterAsDouble(&SDDS_input, "StartHour", &pageStartHour))) {
      fprintf(stderr, "Error: StartTime and StartHour parameters are required when Time is absent.\n");
      exit(EXIT_FAILURE);
    }

    timeData = hasTime ? SDDS_GetColumnInDoubles(&SDDS_input, "Time") : NULL;
    timeOfDayData = hasTimeOfDay ? SDDS_GetColumnInDoubles(&SDDS_input, "TimeOfDay") : NULL;
    timeOfDayWSData = hasTimeOfDayWS ? SDDS_GetColumnInDoubles(&SDDS_input, "TimeOfDayWS") : NULL;
    durationData = hasDuration ? SDDS_GetColumnInDoubles(&SDDS_input, "Duration") : NULL;
    previousRowData = hasPreviousRow ? SDDS_GetColumnInDoubles(&SDDS_input, "PreviousRow") : NULL;
    controlNameIndexData = alarmStatusIndexData = alarmSeverityIndexData = NULL;
    controlNameData = alarmStatusData = alarmSeverityData = NULL;
    descriptionData = relatedControlNameData = relatedValueStringData = NULL;
    controlNameArray = alarmStatusArray = alarmSeverityArray = NULL;
    descriptionArray = relatedControlNameArray = NULL;

    if ((hasTime && !timeData) || (hasTimeOfDay && !timeOfDayData) ||
        (hasTimeOfDayWS && !timeOfDayWSData) || (hasDuration && !durationData) ||
        (hasPreviousRow && !previousRowData)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }

    if (hasIndexedData) {
      controlNameIndexData = SDDS_GetColumnInLong(&SDDS_input, "ControlNameIndex");
      alarmStatusIndexData = SDDS_GetColumnInLong(&SDDS_input, "AlarmStatusIndex");
      alarmSeverityIndexData = SDDS_GetColumnInLong(&SDDS_input, "AlarmSeverityIndex");
      controlNameArray = SDDS_GetArray(&SDDS_input, "ControlNameString", NULL);
      alarmStatusArray = SDDS_GetArray(&SDDS_input, "AlarmStatusString", NULL);
      alarmSeverityArray = SDDS_GetArray(&SDDS_input, "AlarmSeverityString", NULL);
      descriptionArray = hasDescriptionString ? SDDS_GetArray(&SDDS_input, "DescriptionString", NULL) : NULL;
      relatedControlNameArray = hasRelatedControlNameString ? SDDS_GetArray(&SDDS_input, "RelatedControlNameString", NULL) : NULL;
      if (!controlNameIndexData || !alarmStatusIndexData || !alarmSeverityIndexData ||
          !controlNameArray || !alarmStatusArray || !alarmSeverityArray ||
          (hasDescriptionString && !descriptionArray) ||
          (hasRelatedControlNameString && !relatedControlNameArray)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
    if (hasControlName)
      controlNameData = SDDS_GetColumn(&SDDS_input, "ControlName");
    if (hasAlarmStatus)
      alarmStatusData = SDDS_GetColumn(&SDDS_input, "AlarmStatus");
    if (hasAlarmSeverity)
      alarmSeverityData = SDDS_GetColumn(&SDDS_input, "AlarmSeverity");
    if (hasDescription)
      descriptionData = SDDS_GetColumn(&SDDS_input, "Description");
    if (hasRelatedControlName)
      relatedControlNameData = SDDS_GetColumn(&SDDS_input, "RelatedControlName");
    if (hasRelatedValueString)
      relatedValueStringData = SDDS_GetColumn(&SDDS_input, "RelatedValueString");

    if ((hasControlName && !controlNameData) || (hasAlarmStatus && !alarmStatusData) ||
        (hasAlarmSeverity && !alarmSeverityData) || (hasDescription && !descriptionData) ||
        (hasRelatedControlName && !relatedControlNameData) ||
        (hasRelatedValueString && !relatedValueStringData)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }

    for (row = 0; row < rows; row++) {
      ALARM_EVENT event;
      int32_t controlIndex, statusIndex, severityIndex;

      memset(&event, 0, sizeof(event));
      event.previousRow = hasPreviousRow ? (int32_t)previousRowData[row] : -1;
      event.timeOfDay = hasTimeOfDay ? (float)timeOfDayData[row] : 0;
      event.timeOfDayWS = hasTimeOfDayWS ? (float)timeOfDayWSData[row] : 0;
      event.duration = hasDuration ? (float)durationData[row] : 0;
      event.time = hasTime ? timeData[row] : ComputeEpochTime(timeOfDayData[row], pageStartTime, pageStartHour);

      if (filterTime && (event.time < startTime || event.time > endTime))
        continue;

      if (hasIndexedData) {
        controlIndex = controlNameIndexData[row];
        statusIndex = alarmStatusIndexData[row];
        severityIndex = alarmSeverityIndexData[row];
        event.controlName = ArrayString(controlNameArray, controlIndex);
        event.alarmStatus = ArrayString(alarmStatusArray, statusIndex);
        event.alarmSeverity = ArrayString(alarmSeverityArray, severityIndex);
        event.description = hasDescription ? StringColumnValue(descriptionData, row) : ArrayString(descriptionArray, controlIndex);
        event.relatedControlName = hasRelatedControlName ? StringColumnValue(relatedControlNameData, row) : ArrayString(relatedControlNameArray, controlIndex);
      } else {
        event.controlName = StringColumnValue(controlNameData, row);
        event.alarmStatus = StringColumnValue(alarmStatusData, row);
        event.alarmSeverity = StringColumnValue(alarmSeverityData, row);
        event.description = StringColumnValue(descriptionData, row);
        event.relatedControlName = StringColumnValue(relatedControlNameData, row);
      }
      event.relatedValueString = StringColumnValue(relatedValueStringData, row);

      if (!event.controlName || !event.alarmStatus || !event.alarmSeverity)
        continue;

      if (snapshot) {
        if (event.time <= snapshotTime && !SaveSnapshotEvent(&snapshotEvent, &snapshotEvents, &event)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else {
        if (minimumInterval > 0 && event.time - previousOutputTime <= minimumInterval)
          continue;
        if (!EnsureRows(&SDDS_output, &allocatedRows, outputRows + 1) ||
            !WriteAlarmEvent(&SDDS_output, outIndex, outputRows, &event)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
        previousOutputTime = event.time;
        outputRows++;
      }
    }

    if (timeData)
      free(timeData);
    if (timeOfDayData)
      free(timeOfDayData);
    if (timeOfDayWSData)
      free(timeOfDayWSData);
    if (durationData)
      free(durationData);
    if (previousRowData)
      free(previousRowData);
    if (controlNameIndexData)
      free(controlNameIndexData);
    if (alarmStatusIndexData)
      free(alarmStatusIndexData);
    if (alarmSeverityIndexData)
      free(alarmSeverityIndexData);
    if (controlNameData)
      SDDS_FreeStringArray(controlNameData, rows);
    if (alarmStatusData)
      SDDS_FreeStringArray(alarmStatusData, rows);
    if (alarmSeverityData)
      SDDS_FreeStringArray(alarmSeverityData, rows);
    if (descriptionData)
      SDDS_FreeStringArray(descriptionData, rows);
    if (relatedControlNameData)
      SDDS_FreeStringArray(relatedControlNameData, rows);
    if (relatedValueStringData)
      SDDS_FreeStringArray(relatedValueStringData, rows);
    if (controlNameArray)
      SDDS_FreeArray(controlNameArray);
    if (alarmStatusArray)
      SDDS_FreeArray(alarmStatusArray);
    if (alarmSeverityArray)
      SDDS_FreeArray(alarmSeverityArray);
    if (descriptionArray)
      SDDS_FreeArray(descriptionArray);
    if (relatedControlNameArray)
      SDDS_FreeArray(relatedControlNameArray);
  }

  if (snapshot) {
    for (i = 0; i < snapshotEvents; i++) {
      if (!snapshotEvent[i].used)
        continue;
      if (!EnsureRows(&SDDS_output, &allocatedRows, outputRows + 1) ||
          !WriteAlarmEvent(&SDDS_output, outIndex, outputRows, snapshotEvent + i)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      outputRows++;
    }
  }

  if (!SDDS_WriteTable(&SDDS_output) ||
      !SDDS_Terminate(&SDDS_input) ||
      !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (snapshotEvent) {
    for (i = 0; i < snapshotEvents; i++)
      ClearAlarmEvent(snapshotEvent + i);
    free(snapshotEvent);
  }
  if (retainName)
    free(retainName);
  if (deleteName)
    free(deleteName);
  if (tmpfile_used && !replaceFileAndBackUp(input, output))
    exit(EXIT_FAILURE);
  free_scanargs(&s_arg, argc);
  return EXIT_SUCCESS;
}

static int NameMatchesList(const char *name, char **list, long items) {
  long i;
  char *expanded;
  int matched;

  for (i = 0; i < items; i++) {
    expanded = expand_ranges(list[i]);
    matched = wild_match((char *)name, expanded);
    if (expanded)
      free(expanded);
    if (matched)
      return 1;
  }
  return 0;
}

static int NameSelected(const char *name, char **deleteName, long deleteNames, char **retainName, long retainNames) {
  int selected;

  selected = 1;
  if (deleteNames && NameMatchesList(name, deleteName, deleteNames))
    selected = 0;
  if (retainNames) {
    if (!deleteNames)
      selected = 0;
    if (NameMatchesList(name, retainName, retainNames))
      selected = 1;
  }
  return selected;
}

static int DefineOutputColumns(SDDS_DATASET *outSet, int32_t *outIndex, int *available, long outputType) {
  if (available[COL_TIME] &&
      0 > (outIndex[COL_TIME] = SDDS_DefineColumn(outSet, "Time", NULL, "s", "Computed epoch time", NULL, outputType, 0)))
    return 0;
  if (available[COL_PREVIOUS_ROW] &&
      0 > (outIndex[COL_PREVIOUS_ROW] = SDDS_DefineColumn(outSet, "PreviousRow", NULL, NULL, NULL, "%6ld", SDDS_LONG, 0)))
    return 0;
  if (available[COL_TIME_OF_DAY_WS] &&
      0 > (outIndex[COL_TIME_OF_DAY_WS] = SDDS_DefineColumn(outSet, "TimeOfDayWS", NULL, "h", "Time of day in hours per workstation", "%8.5f", SDDS_FLOAT, 0)))
    return 0;
  if (available[COL_TIME_OF_DAY] &&
      0 > (outIndex[COL_TIME_OF_DAY] = SDDS_DefineColumn(outSet, "TimeOfDay", NULL, "h", "Time of day in hours per IOC", "%8.5f", SDDS_FLOAT, 0)))
    return 0;
  if (available[COL_CONTROL_NAME] &&
      0 > (outIndex[COL_CONTROL_NAME] = SDDS_DefineColumn(outSet, "ControlName", NULL, NULL, NULL, "%32s", SDDS_STRING, 0)))
    return 0;
  if (available[COL_ALARM_STATUS] &&
      0 > (outIndex[COL_ALARM_STATUS] = SDDS_DefineColumn(outSet, "AlarmStatus", NULL, NULL, NULL, "%12s", SDDS_STRING, 0)))
    return 0;
  if (available[COL_ALARM_SEVERITY] &&
      0 > (outIndex[COL_ALARM_SEVERITY] = SDDS_DefineColumn(outSet, "AlarmSeverity", NULL, NULL, NULL, "%10s", SDDS_STRING, 0)))
    return 0;
  if (available[COL_DESCRIPTION] &&
      0 > (outIndex[COL_DESCRIPTION] = SDDS_DefineColumn(outSet, "Description", NULL, NULL, NULL, NULL, SDDS_STRING, 0)))
    return 0;
  if (available[COL_RELATED_CONTROL_NAME] &&
      0 > (outIndex[COL_RELATED_CONTROL_NAME] = SDDS_DefineColumn(outSet, "RelatedControlName", NULL, NULL, NULL, NULL, SDDS_STRING, 0)))
    return 0;
  if (available[COL_DURATION] &&
      0 > (outIndex[COL_DURATION] = SDDS_DefineColumn(outSet, "Duration", NULL, "s", NULL, "%8.4f", SDDS_FLOAT, 0)))
    return 0;
  if (available[COL_RELATED_VALUE_STRING] &&
      0 > (outIndex[COL_RELATED_VALUE_STRING] = SDDS_DefineColumn(outSet, "RelatedValueString", NULL, NULL, NULL, NULL, SDDS_STRING, 0)))
    return 0;
  return 1;
}

static int EnsureRows(SDDS_DATASET *outSet, int64_t *allocatedRows, int64_t neededRows) {
  int64_t increment;

  if (neededRows <= *allocatedRows)
    return 1;
  increment = neededRows - *allocatedRows;
  if (increment < 100)
    increment = 100;
  if (!SDDS_LengthenTable(outSet, increment))
    return 0;
  *allocatedRows += increment;
  return 1;
}

static int WriteAlarmEvent(SDDS_DATASET *outSet, int32_t *outIndex, int64_t row, ALARM_EVENT *event) {
  if (outIndex[COL_TIME] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_TIME], event->time, -1))
    return 0;
  if (outIndex[COL_PREVIOUS_ROW] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_PREVIOUS_ROW], event->previousRow, -1))
    return 0;
  if (outIndex[COL_TIME_OF_DAY_WS] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_TIME_OF_DAY_WS], event->timeOfDayWS, -1))
    return 0;
  if (outIndex[COL_TIME_OF_DAY] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_TIME_OF_DAY], event->timeOfDay, -1))
    return 0;
  if (outIndex[COL_CONTROL_NAME] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_CONTROL_NAME], event->controlName ? event->controlName : "", -1))
    return 0;
  if (outIndex[COL_ALARM_STATUS] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_ALARM_STATUS], event->alarmStatus ? event->alarmStatus : "", -1))
    return 0;
  if (outIndex[COL_ALARM_SEVERITY] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_ALARM_SEVERITY], event->alarmSeverity ? event->alarmSeverity : "", -1))
    return 0;
  if (outIndex[COL_DESCRIPTION] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_DESCRIPTION], event->description ? event->description : "", -1))
    return 0;
  if (outIndex[COL_RELATED_CONTROL_NAME] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_RELATED_CONTROL_NAME], event->relatedControlName ? event->relatedControlName : "", -1))
    return 0;
  if (outIndex[COL_DURATION] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_DURATION], event->duration, -1))
    return 0;
  if (outIndex[COL_RELATED_VALUE_STRING] >= 0 &&
      !SDDS_SetRowValues(outSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, row, outIndex[COL_RELATED_VALUE_STRING], event->relatedValueString ? event->relatedValueString : "", -1))
    return 0;
  return 1;
}

static double ComputeEpochTime(double timeOfDay, double startTime, double startHour) {
  double deltaHours;

  deltaHours = timeOfDay - startHour;
  if (deltaHours < -12)
    deltaHours += 24;
  return startTime + 3600 * deltaHours;
}

static char *ArrayString(SDDS_ARRAY *array, int32_t index) {
  if (!array || index < 0 || index >= array->elements)
    return NULL;
  return ((char **)array->data)[index];
}

static char *StringColumnValue(char **column, int64_t row) {
  if (!column)
    return NULL;
  return column[row];
}

static void ClearAlarmEvent(ALARM_EVENT *event) {
  if (!event)
    return;
  if (event->controlName)
    free(event->controlName);
  if (event->alarmStatus)
    free(event->alarmStatus);
  if (event->alarmSeverity)
    free(event->alarmSeverity);
  if (event->description)
    free(event->description);
  if (event->relatedControlName)
    free(event->relatedControlName);
  if (event->relatedValueString)
    free(event->relatedValueString);
  memset(event, 0, sizeof(*event));
}

static int CopyStringOrBlank(char **target, const char *source) {
  if (!source)
    source = "";
  return SDDS_CopyString(target, (char *)source);
}

static int SaveSnapshotEvent(ALARM_EVENT **snapshotEvent, int64_t *snapshotEvents, ALARM_EVENT *event) {
  int64_t i;
  ALARM_EVENT *item;

  for (i = 0; i < *snapshotEvents; i++) {
    if ((*snapshotEvent)[i].used && strcmp((*snapshotEvent)[i].controlName, event->controlName) == 0)
      break;
  }
  if (i == *snapshotEvents) {
    *snapshotEvent = trealloc(*snapshotEvent, sizeof(**snapshotEvent) * (*snapshotEvents + 1));
    memset(*snapshotEvent + *snapshotEvents, 0, sizeof(**snapshotEvent));
    (*snapshotEvents)++;
  } else {
    ClearAlarmEvent(*snapshotEvent + i);
  }

  item = *snapshotEvent + i;
  item->used = 1;
  item->time = event->time;
  item->previousRow = event->previousRow;
  item->timeOfDayWS = event->timeOfDayWS;
  item->timeOfDay = event->timeOfDay;
  item->duration = event->duration;
  if (!CopyStringOrBlank(&item->controlName, event->controlName) ||
      !CopyStringOrBlank(&item->alarmStatus, event->alarmStatus) ||
      !CopyStringOrBlank(&item->alarmSeverity, event->alarmSeverity) ||
      !CopyStringOrBlank(&item->description, event->description) ||
      !CopyStringOrBlank(&item->relatedControlName, event->relatedControlName) ||
      !CopyStringOrBlank(&item->relatedValueString, event->relatedValueString))
    return 0;
  return 1;
}
