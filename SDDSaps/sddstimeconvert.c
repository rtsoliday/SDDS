/**
 * @file sddstimeconvert.c
 * @brief Perform time conversions on SDDS data.
 *
 * @details
 * This program enables users to perform various time-related operations on SDDS files.
 * These operations include breaking down epoch times into components, converting date
 * strings to epoch times, and specifying the major order of output data.
 *
 * @section Usage
 * ```
 * sddstimeconvert [<SDDSinput>] [<SDDSoutput>]
 *                 [-pipe=<input>[,<output>]]
 *                 [-majorOrder=row|column]
 *                 [-breakdown={column|parameter},<timeName>[,year=<newName>][,julianDay=<newName>][,month=<newName>][,day=<newName>][,hour=<newName>][,text=<newName>]]
 *                 [-dateToTime={column|parameter},<timeName>,<newName>,<stringName>,format=<formatString>]
 *                 [-epoch={column|parameter},<newName>,year=<name>,[julianDay=<name>|month=<name>,day=<name>],hour=<name>]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Enable pipe-based input/output processing.                                            |
 * | `-majorOrder`                         | Specify output data order: row-major or column-major.                                 |
 * | `-breakdown=`                         | Break down epoch time into components such as year, month, day, hour, etc.            |
 * | `-dateToTime`                         | Convert date strings to epoch times based on a specified format string.               |
 * | `-epoch`                              | Generate a new epoch time column or parameter with optional qualifiers.               |
 *
 * @subsection Incompatibilities
 *   - For `-epoch`, the following requirements must be met:
 *     - Specify either `julianDay` or both `month` and `day` names.
 *   - For `-breakdown`, at least one of the following must be specified:
 *     - `year`, `julianDay`, `month`, `day`, `hour`, or `text` qualifiers.
 *   - For `-dateToTime`, the `format` string is required.
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
 *   - M. Borland
 *   - R. Soliday
 *   - H. Shang
 */

#define _XOPEN_SOURCE
#include <time.h>
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_EPOCH,
  SET_PIPE,
  SET_BREAKDOWN,
  SET_MAJOR_ORDER,
  SET_DATE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "epoch",
  "pipe",
  "breakdown",
  "majorOrder",
  "date",
};

/* Improved and more readable usage message */
char *USAGE =
  "Usage:\n"
  "  sddstimeconvert [<SDDSinput>] [<SDDSoutput>] \n"
  "                  [-pipe=<input>[,<output>]] \n"
  "                  [-majorOrder=row|column]\n"
  "                  [-breakdown={column|parameter},<timeName>[,year=<newName>]\n"
  "                                                           [,julianDay=<newName>]\n"
  "                                                           [,month=<newName>]\n"
  "                                                           [,day=<newName>]\n"
  "                                                           [,hour=<newName>]\n"
  "                                                           [,text=<newName>]]\n"
  "                  [-dateToTime={column|parameter},<timeName>,<newName>,<stringName>,format=<formatString>]\n"
  "                  [-epoch={column|parameter},<newName>,year=<name>,[julianDay=<name>|month=<name>,day=<name>],hour=<name>]\n"
  "Options:\n"
  "  -pipe            Enable standard SDDS Toolkit pipe processing.\n"
  "  -majorOrder      Specify output file order: row or column major.\n"
  "  -breakdown       Break down epoch time into components.\n"
  "  -epoch           Create a new epoch time column or parameter.\n"
  "  -dateToTime      Convert date string to epoch time.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define IS_COLUMN 0x0001U
#define IS_PARAMETER 0x0002U
#define EPOCH_GIVEN 0x0004U
#define YEAR_GIVEN 0x0008U
#define JDAY_GIVEN 0x0010U
#define MONTH_GIVEN 0x0020U
#define DAY_GIVEN 0x0040U
#define HOUR_GIVEN 0x0080U
#define DO_BREAKDOWN 0x0100U
#define DO_EPOCH 0x0200U
#define TEXT_GIVEN 0x0400U
#define FORMAT_GIVEN 0x0800U
#define DO_DATECONVERSION 0x1000U

typedef struct
{
  char *epochName, *yearName, *jDayName, *monthName, *dayName, *hourName;
  char *textName, *format;
  long epochIndex, yearIndex, jDayIndex, monthIndex, dayIndex, hourIndex;
  long textIndex;
  unsigned long flags;
} TIME_CONVERSION;

void DoColumnEpochConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion);
void DoParameterEpochConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion);
void DoColumnDateToTimeConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion);
void DoParameterDateToTimeConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion);
void DoColumnBreakdownConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion);
void DoParameterBreakdownConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion);
void InitializeOutput(SDDS_DATASET *SDDSout, char *outputfile, TIME_CONVERSION *conversion, long conversions,
                      SDDS_DATASET *SDDSin, short columnMajorOrder);
void CheckEpochConversionElements(SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion, long conversions);
void CheckBreakdownConversionElements(SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion, long conversions);
void CheckDateConversionElements(SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion, long conversions);

int main(int argc, char **argv) {
  SDDS_DATASET SDDSin, SDDSout;
  long i_arg, iconv;
  SCANNED_ARG *s_arg;
  char *input, *output;
  TIME_CONVERSION *conversion;
  long conversions;
  unsigned long pipeFlags, majorOrderFlag;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  input = output = NULL;
  conversions = 0;
  conversion = NULL;
  pipeFlags = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_EPOCH:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("Invalid -epoch syntax");
        if (!(conversion = SDDS_Realloc(conversion, sizeof(*conversion) * (conversions + 1))))
          SDDS_Bomb("Memory allocation failure");
        memset((char *)(conversion + conversions), 0, sizeof(*conversion));
        conversion[conversions].epochName = s_arg[i_arg].list[2];
        s_arg[i_arg].list[2] = s_arg[i_arg].list[1];
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&conversion[conversions].flags,
                          s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "column", -1, NULL, 0, IS_COLUMN,
                          "parameter", -1, NULL, 0, IS_PARAMETER,
                          "year", SDDS_STRING, &conversion[conversions].yearName, 1, YEAR_GIVEN,
                          "julianday", SDDS_STRING, &conversion[conversions].jDayName, 1, JDAY_GIVEN,
                          "month", SDDS_STRING, &conversion[conversions].monthName, 1, MONTH_GIVEN,
                          "day", SDDS_STRING, &conversion[conversions].dayName, 1, DAY_GIVEN,
                          "hour", SDDS_STRING, &conversion[conversions].hourName, 1, HOUR_GIVEN, NULL))
          SDDS_Bomb("invalid -epoch syntax");
        conversion[conversions].flags |= EPOCH_GIVEN | DO_EPOCH;
        if (!(conversion[conversions].flags & (IS_COLUMN | IS_PARAMETER)))
          SDDS_Bomb("Specify 'column' or 'parameter' qualifier with -epoch");
        if (conversion[conversions].flags & IS_COLUMN && conversion[conversions].flags & IS_PARAMETER)
          SDDS_Bomb("Specify only one of 'column' or 'parameter' qualifier with -epoch");
        if (!(conversion[conversions].flags & YEAR_GIVEN))
          SDDS_Bomb("Specify year name with -epoch");
        if (!(conversion[conversions].flags & JDAY_GIVEN) &&
            (conversion[conversions].flags & (MONTH_GIVEN | DAY_GIVEN)) != (MONTH_GIVEN | DAY_GIVEN))
          SDDS_Bomb("Specify either julianDay name, or both month and day names with -epoch");
        if (conversion[conversions].flags & JDAY_GIVEN && conversion[conversions].flags & (MONTH_GIVEN | DAY_GIVEN))
          SDDS_Bomb("Invalid combination of julianDay name with month or day name for -epoch");
        conversions++;
        break;
      case SET_BREAKDOWN:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("Invalid -breakdown syntax");
        if (!(conversion = SDDS_Realloc(conversion, sizeof(*conversion) * (conversions + 1))))
          SDDS_Bomb("Memory allocation failure");
        memset((char *)(conversion + conversions), 0, sizeof(*conversion));
        conversion[conversions].epochName = s_arg[i_arg].list[2];
        s_arg[i_arg].list[2] = s_arg[i_arg].list[1];
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&conversion[conversions].flags,
                          s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "column", -1, NULL, 0, IS_COLUMN,
                          "parameter", -1, NULL, 0, IS_PARAMETER,
                          "year", SDDS_STRING, &conversion[conversions].yearName, 1, YEAR_GIVEN,
                          "julianday", SDDS_STRING, &conversion[conversions].jDayName, 1, JDAY_GIVEN,
                          "month", SDDS_STRING, &conversion[conversions].monthName, 1, MONTH_GIVEN,
                          "day", SDDS_STRING, &conversion[conversions].dayName, 1, DAY_GIVEN,
                          "hour", SDDS_STRING, &conversion[conversions].hourName, 1, HOUR_GIVEN,
                          "text", SDDS_STRING, &conversion[conversions].textName, 1, TEXT_GIVEN, NULL))
          SDDS_Bomb("invalid -breakdown syntax");
        conversion[conversions].flags |= EPOCH_GIVEN | DO_BREAKDOWN;
        if (!(conversion[conversions].flags & (IS_COLUMN | IS_PARAMETER)))
          SDDS_Bomb("Specify 'column' or 'parameter' qualifier with -breakdown");
        if (conversion[conversions].flags & IS_COLUMN && conversion[conversions].flags & IS_PARAMETER)
          SDDS_Bomb("Specify only one of 'column' or 'parameter' qualifier with -breakdown");
        if (!(conversion[conversions].flags & (YEAR_GIVEN | JDAY_GIVEN | MONTH_GIVEN | DAY_GIVEN | HOUR_GIVEN | TEXT_GIVEN)))
          SDDS_Bomb("Specify at least one of year, julianDay, month, day, hour, or text qualifiers with -breakdown");
        conversions++;
        break;
      case SET_DATE:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("Invalid -dateToTime syntax");
        if (!(conversion = SDDS_Realloc(conversion, sizeof(*conversion) * (conversions + 1))))
          SDDS_Bomb("Memory allocation failure");
        memset((char *)(conversion + conversions), 0, sizeof(*conversion));
        conversion[conversions].textName = s_arg[i_arg].list[3];
        conversion[conversions].epochName = s_arg[i_arg].list[2];
        s_arg[i_arg].list[3] = s_arg[i_arg].list[1];
        s_arg[i_arg].n_items -= 3;
        if (!scanItemList(&conversion[conversions].flags,
                          s_arg[i_arg].list + 3, &s_arg[i_arg].n_items, 0,
                          "column", -1, NULL, 0, IS_COLUMN,
                          "parameter", -1, NULL, 0, IS_PARAMETER,
                          "format", SDDS_STRING, &conversion[conversions].format, 1, FORMAT_GIVEN, NULL))
          SDDS_Bomb("invalid -dateToTime syntax");
        conversion[conversions].flags |= DO_DATECONVERSION;
        if (!(conversion[conversions].flags & (IS_COLUMN | IS_PARAMETER)))
          SDDS_Bomb("Specify 'column' or 'parameter' qualifier with -dateToTime");
        if (conversion[conversions].flags & IS_COLUMN && conversion[conversions].flags & IS_PARAMETER)
          SDDS_Bomb("Specify only one of 'column' or 'parameter' qualifier with -dateToTime");
        if (!(conversion[conversions].flags & FORMAT_GIVEN))
          SDDS_Bomb("Format string not provided for date to time conversion");
        conversions++;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;
      default:
        fprintf(stderr, "Error: Unknown option: %s\n", s_arg[i_arg].list[0]);
        SDDS_Bomb(NULL);
        break;
      }
    } else {
      if (!input)
        input = s_arg[i_arg].list[0];
      else if (!output)
        output = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "Error: Argument '%s' is invalid: too many filenames (sddstimeconvert)\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    }
  }

  processFilenames("sddstimeconvert", &input, &output, pipeFlags, 0, NULL);

  if (!SDDS_InitializeInput(&SDDSin, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  CheckEpochConversionElements(&SDDSin, conversion, conversions);
  CheckBreakdownConversionElements(&SDDSin, conversion, conversions);
  CheckDateConversionElements(&SDDSin, conversion, conversions);

  InitializeOutput(&SDDSout, output, conversion, conversions, &SDDSin, columnMajorOrder);

  while (SDDS_ReadPage(&SDDSin) > 0) {
    if (!SDDS_CopyPage(&SDDSout, &SDDSin)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    for (iconv = 0; iconv < conversions; iconv++) {
      if (conversion[iconv].flags & DO_EPOCH) {
        if (conversion[iconv].flags & IS_PARAMETER)
          DoParameterEpochConversion(&SDDSout, &SDDSin, conversion + iconv);
        else
          DoColumnEpochConversion(&SDDSout, &SDDSin, conversion + iconv);
      } else if (conversion[iconv].flags & DO_BREAKDOWN) {
        if (conversion[iconv].flags & IS_PARAMETER)
          DoParameterBreakdownConversion(&SDDSout, &SDDSin, conversion + iconv);
        else
          DoColumnBreakdownConversion(&SDDSout, &SDDSin, conversion + iconv);
      } else {
        /* Convert date string to double time */
        if (conversion[iconv].flags & IS_PARAMETER)
          DoParameterDateToTimeConversion(&SDDSout, &SDDSin, conversion + iconv);
        else
          DoColumnDateToTimeConversion(&SDDSout, &SDDSin, conversion + iconv);
      }
    }
    if (!SDDS_WritePage(&SDDSout)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}

void CheckEpochConversionElements(SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion, long conversions) {
  while (conversions-- > 0) {
    if (!(conversion[conversions].flags & DO_EPOCH))
      continue;
    if (conversion->flags & IS_PARAMETER) {
      if (SDDS_CheckParameter(SDDSin, conversion->yearName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK ||
          (conversion->jDayName &&
           SDDS_CheckParameter(SDDSin, conversion->jDayName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
          (conversion->dayName &&
           SDDS_CheckParameter(SDDSin, conversion->dayName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
          (conversion->monthName &&
           SDDS_CheckParameter(SDDSin, conversion->monthName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
          SDDS_CheckParameter(SDDSin, conversion->hourName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      conversion->yearIndex = SDDS_GetParameterIndex(SDDSin, conversion->yearName);
      conversion->hourIndex = SDDS_GetParameterIndex(SDDSin, conversion->hourName);
      conversion->dayIndex = conversion->jDayIndex = conversion->monthIndex = -1;
      if (conversion->dayName)
        conversion->dayIndex = SDDS_GetParameterIndex(SDDSin, conversion->dayName);
      if (conversion->jDayName)
        conversion->jDayIndex = SDDS_GetParameterIndex(SDDSin, conversion->jDayName);
      if (conversion->monthName)
        conversion->monthIndex = SDDS_GetParameterIndex(SDDSin, conversion->monthName);
    } else {
      if (SDDS_CheckColumn(SDDSin, conversion->yearName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK ||
          (conversion->jDayName &&
           SDDS_CheckColumn(SDDSin, conversion->jDayName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
          (conversion->dayName &&
           SDDS_CheckColumn(SDDSin, conversion->dayName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
          (conversion->monthName &&
           SDDS_CheckColumn(SDDSin, conversion->monthName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
          SDDS_CheckColumn(SDDSin, conversion->hourName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      conversion->yearIndex = SDDS_GetColumnIndex(SDDSin, conversion->yearName);
      conversion->hourIndex = SDDS_GetColumnIndex(SDDSin, conversion->hourName);
      conversion->dayIndex = conversion->jDayIndex = conversion->monthIndex = -1;
      if (conversion->dayName)
        conversion->dayIndex = SDDS_GetColumnIndex(SDDSin, conversion->dayName);
      if (conversion->jDayName)
        conversion->jDayIndex = SDDS_GetColumnIndex(SDDSin, conversion->jDayName);
      if (conversion->monthName)
        conversion->monthIndex = SDDS_GetColumnIndex(SDDSin, conversion->monthName);
    }
    conversion++;
  }
}

void CheckBreakdownConversionElements(SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion, long conversions) {
  while (conversions-- > 0) {
    if (!(conversion[conversions].flags & DO_BREAKDOWN))
      continue;
    if (conversion->flags & IS_PARAMETER) {
      if (SDDS_CheckParameter(SDDSin, conversion->epochName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      conversion->epochIndex = SDDS_GetParameterIndex(SDDSin, conversion->epochName);
    } else {
      if (SDDS_CheckColumn(SDDSin, conversion->epochName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      conversion->epochIndex = SDDS_GetColumnIndex(SDDSin, conversion->epochName);
    }
    conversion++;
  }
}

void CheckDateConversionElements(SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion, long conversions) {
  while (conversions-- > 0) {
    if (!(conversion[conversions].flags & DO_DATECONVERSION))
      continue;
    if (conversion->flags & IS_PARAMETER) {
      if (SDDS_CheckParameter(SDDSin, conversion->textName, NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      conversion->textIndex = SDDS_GetParameterIndex(SDDSin, conversion->textName);
    } else {
      if (SDDS_CheckColumn(SDDSin, conversion->textName, NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      conversion->textIndex = SDDS_GetColumnIndex(SDDSin, conversion->textName);
    }
    if (conversion->textIndex < 0) {
      fprintf(stderr, "Error: '%s' does not exist in input file.\n", conversion->textName);
      exit(EXIT_FAILURE);
    }
    conversion++;
  }
}

void InitializeOutput(SDDS_DATASET *SDDSout, char *outputfile, TIME_CONVERSION *conversion, long conversions,
                      SDDS_DATASET *SDDSin, short columnMajorOrder) {
  if (!SDDS_InitializeCopy(SDDSout, SDDSin, outputfile, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    SDDSout->layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout->layout.data_mode.column_major = SDDSin->layout.data_mode.column_major;
  while (conversions-- > 0) {
    if (conversion->flags & DO_EPOCH) {
      if (conversion->flags & IS_PARAMETER) {
        if ((conversion->epochIndex = SDDS_DefineParameter(SDDSout, conversion->epochName,
                                                           NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, NULL)) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if ((conversion->epochIndex = SDDS_DefineColumn(SDDSout, conversion->epochName,
                                                        NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0)) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    } else if (conversion->flags & DO_BREAKDOWN) {
      if (conversion->flags & IS_PARAMETER) {
        if ((conversion->yearName &&
             (conversion->yearIndex = SDDS_DefineParameter(SDDSout, conversion->yearName, NULL, NULL, "Year", NULL,
                                                           SDDS_SHORT, NULL)) < 0) ||
            (conversion->dayName &&
             (conversion->dayIndex = SDDS_DefineParameter(SDDSout, conversion->dayName, NULL, NULL, "Day of month",
                                                          NULL, SDDS_SHORT, NULL)) < 0) ||
            (conversion->monthName &&
             (conversion->monthIndex = SDDS_DefineParameter(SDDSout, conversion->monthName, NULL, NULL, "Month", NULL,
                                                            SDDS_SHORT, NULL)) < 0) ||
            (conversion->jDayName &&
             (conversion->jDayIndex = SDDS_DefineParameter(SDDSout, conversion->jDayName, NULL, NULL, "Julian day",
                                                           NULL, SDDS_SHORT, NULL)) < 0) ||
            (conversion->hourName &&
             (conversion->hourIndex = SDDS_DefineParameter(SDDSout, conversion->hourName, NULL, NULL, "Hour of day",
                                                           NULL, SDDS_DOUBLE, NULL)) < 0) ||
            (conversion->textName &&
             (conversion->textIndex = SDDS_DefineParameter(SDDSout, conversion->textName, NULL, NULL, "Timestamp",
                                                           NULL, SDDS_STRING, NULL)) < 0))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if ((conversion->yearName &&
             (conversion->yearIndex = SDDS_DefineColumn(SDDSout, conversion->yearName, NULL, NULL, "Year", NULL,
                                                        SDDS_SHORT, 0)) < 0) ||
            (conversion->dayName &&
             (conversion->dayIndex = SDDS_DefineColumn(SDDSout, conversion->dayName, NULL, NULL, "Day of month",
                                                       NULL, SDDS_SHORT, 0)) < 0) ||
            (conversion->monthName &&
             (conversion->monthIndex = SDDS_DefineColumn(SDDSout, conversion->monthName, NULL, NULL, "Month", NULL,
                                                         SDDS_SHORT, 0)) < 0) ||
            (conversion->jDayName &&
             (conversion->jDayIndex = SDDS_DefineColumn(SDDSout, conversion->jDayName, NULL, NULL, "Julian day",
                                                        NULL, SDDS_SHORT, 0)) < 0) ||
            (conversion->hourName &&
             (conversion->hourIndex = SDDS_DefineColumn(SDDSout, conversion->hourName, NULL, NULL, "Hour of day",
                                                        NULL, SDDS_DOUBLE, 0)) < 0) ||
            (conversion->textName &&
             (conversion->textIndex = SDDS_DefineColumn(SDDSout, conversion->textName, NULL, NULL, "Timestamp",
                                                        NULL, SDDS_STRING, 0)) < 0))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    } else {
      /* Date to time conversion */
      if (conversion->flags & IS_PARAMETER) {
        if ((conversion->epochIndex = SDDS_DefineParameter(SDDSout, conversion->epochName,
                                                           NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, NULL)) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if ((conversion->epochIndex = SDDS_DefineColumn(SDDSout, conversion->epochName,
                                                        NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0)) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    conversion++;
  }
  if (!SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void DoParameterEpochConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion) {
  double hour;
  double month = 0, day = 0, jDay = 0, year, epochTime;

  jDay = 0;
  if (!SDDS_GetParameterAsDouble(SDDSin, conversion->hourName, &hour) ||
      !SDDS_GetParameterAsDouble(SDDSin, conversion->yearName, &year) ||
      (conversion->jDayName && !SDDS_GetParameterAsDouble(SDDSin, conversion->jDayName, &jDay)) ||
      (conversion->monthName && !SDDS_GetParameterAsDouble(SDDSin, conversion->monthName, &month)) ||
      (conversion->dayName && !SDDS_GetParameterAsDouble(SDDSin, conversion->dayName, &day))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  TimeBreakdownToEpoch((short)year, (short)jDay, (short)month, (short)day, hour, &epochTime);
  if (!SDDS_SetParametersFromDoubles(SDDSout, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, conversion->epochName, epochTime, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void DoParameterBreakdownConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion) {
  double hour, epochTime;
  short year, jDay, month, day;
  char text[30];

  if (!SDDS_GetParameterAsDouble(SDDSin, conversion->epochName, &epochTime))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  TimeEpochToBreakdown(&year, &jDay, &month, &day, &hour, epochTime);
  TimeEpochToText(text, epochTime);
  if ((conversion->yearName &&
       !SDDS_SetParametersFromDoubles(SDDSout, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE,
                                      conversion->yearIndex, (double)year, -1)) ||
      (conversion->dayName &&
       !SDDS_SetParametersFromDoubles(SDDSout, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE,
                                      conversion->dayIndex, (double)day, -1)) ||
      (conversion->jDayName &&
       !SDDS_SetParametersFromDoubles(SDDSout, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE,
                                      conversion->jDayIndex, (double)jDay, -1)) ||
      (conversion->monthName &&
       !SDDS_SetParametersFromDoubles(SDDSout, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE,
                                      conversion->monthIndex, (double)month, -1)) ||
      (conversion->hourName &&
       !SDDS_SetParametersFromDoubles(SDDSout, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE,
                                      conversion->hourIndex, (double)hour, -1)) ||
      (conversion->textName &&
       !SDDS_SetParameters(SDDSout, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, conversion->textIndex, text, -1)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void DoParameterDateToTimeConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion) {
#if defined(_WIN32)
  fprintf(stderr, "Error: strptime function needed by DoParameterDateToTimeConversion is not available on Windows\n");
  exit(EXIT_FAILURE);
#else
  double hour;
  double month = 0, day = 0, jDay = 0, year, epochTime;
  char *timestr = NULL;
  struct tm tm = {0};
  /* Note that tm struct:
     tm_year: years since 1900
     tm_mon: months since January (0-11)
  */

  if (!SDDS_GetParameter(SDDSin, conversion->textName, &timestr)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (strptime(timestr, conversion->format, &tm) == NULL) {
    fprintf(stderr, "Error: Failed to parse date string '%s' with format '%s'\n", timestr, conversion->format);
    exit(EXIT_FAILURE);
  }
  year = tm.tm_year + 1900;
  month = tm.tm_mon + 1;
  day = tm.tm_mday;
  hour = tm.tm_hour + tm.tm_min / 60.0 + tm.tm_sec / 3600.0;

  TimeBreakdownToEpoch((short)year, (short)jDay, (short)month, (short)day, hour, &epochTime);
  if (!SDDS_SetParametersFromDoubles(SDDSout, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, conversion->epochName, epochTime, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#endif
}

void DoColumnEpochConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion) {
  double *hour;
  double *month, *day, *jDay, *year, *epochTime;
  int64_t row, rows;

  year = NULL;

  if (!(rows = SDDS_CountRowsOfInterest(SDDSin)))
    return;

  jDay = month = day = NULL;
  if (!(hour = SDDS_GetColumnInDoubles(SDDSin, conversion->hourName)) ||
      !(year = SDDS_GetColumnInDoubles(SDDSin, conversion->yearName)) ||
      (conversion->jDayName && !(jDay = SDDS_GetColumnInDoubles(SDDSin, conversion->jDayName))) ||
      (conversion->monthName && !(month = SDDS_GetColumnInDoubles(SDDSin, conversion->monthName))) ||
      (conversion->dayName && !(day = SDDS_GetColumnInDoubles(SDDSin, conversion->dayName))))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(epochTime = (double *)malloc(sizeof(*epochTime) * rows)))
    SDDS_Bomb("Memory allocation failure");

  for (row = 0; row < rows; row++)
    TimeBreakdownToEpoch((short)year[row], jDay ? (short)jDay[row] : 0, month ? (short)month[row] : 0, day ? (short)day[row] : 0, hour[row], epochTime + row);
  if (!SDDS_SetColumnFromDoubles(SDDSout, SDDS_BY_NAME, epochTime, rows, conversion->epochName))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(hour);
  free(year);
  free(epochTime);
  if (jDay)
    free(jDay);
  if (month)
    free(month);
  if (day)
    free(day);
}

void DoColumnDateToTimeConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion) {
#if defined(_WIN32)
  fprintf(stderr, "Error: strptime function needed by DoColumnDateToTimeConversion is not available on Windows\n");
  exit(EXIT_FAILURE);
#else
  double hour;
  double month = 0, day = 0, year = 0, jDay = 0, *epochTime;
  int64_t row, rows;
  char **timestr = NULL;
  struct tm tm = {0};
  /* Note that tm struct:
     tm_year: years since 1900
     tm_mon: months since January (0-11)
  */
  if (!(rows = SDDS_CountRowsOfInterest(SDDSin)))
    return;

  if (!(timestr = SDDS_GetColumn(SDDSin, conversion->textName)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(epochTime = (double *)malloc(sizeof(*epochTime) * rows)))
    SDDS_Bomb("Memory allocation failure");

  for (row = 0; row < rows; row++) {
    if (strptime(timestr[row], conversion->format, &tm) == NULL) {
      fprintf(stderr, "Error: Failed to parse date string '%s' with format '%s'\n", timestr[row], conversion->format);
      exit(EXIT_FAILURE);
    }
    year = tm.tm_year + 1900;
    month = tm.tm_mon + 1;
    day = tm.tm_mday;
    hour = tm.tm_hour + tm.tm_min / 60.0 + tm.tm_sec / 3600.0;
    TimeBreakdownToEpoch((short)year, (short)jDay, (short)month, (short)day, hour, epochTime + row);
  }
  if (!SDDS_SetColumnFromDoubles(SDDSout, SDDS_BY_NAME, epochTime, rows, conversion->epochName))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  SDDS_FreeStringArray(timestr, rows);
  free(epochTime);
#endif
}

void DoColumnBreakdownConversion(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, TIME_CONVERSION *conversion) {
  double *hour, *epochTime;
  short *month, *day, *jDay, *year;
  char **text;
  int64_t row, rows;

  if (!(rows = SDDS_CountRowsOfInterest(SDDSin)))
    return;

  if (!(epochTime = SDDS_GetColumnInDoubles(SDDSin, conversion->epochName)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  hour = NULL;
  month = day = jDay = year = NULL;
  text = NULL;
  if ((conversion->hourName && !(hour = (double *)malloc(sizeof(*hour) * rows))) ||
      (conversion->monthName && !(month = (short *)malloc(sizeof(*month) * rows))) ||
      (conversion->dayName && !(day = (short *)malloc(sizeof(*day) * rows))) ||
      (conversion->jDayName && !(jDay = (short *)malloc(sizeof(*jDay) * rows))) ||
      (conversion->textName && !(text = (char **)malloc(sizeof(*text) * rows))) ||
      (conversion->yearName && !(year = (short *)malloc(sizeof(*year) * rows))))
    SDDS_Bomb("Memory allocation failure");

  for (row = 0; row < rows; row++) {
    if (text)
      text[row] = malloc(sizeof(**text) * 30);
    if (!TimeEpochToBreakdown(year ? year + row : NULL, jDay ? jDay + row : NULL, month ? month + row : NULL, day ? day + row : NULL, hour ? hour + row : NULL, epochTime[row]) ||
        (text && !TimeEpochToText(text[row], epochTime[row])))
      SDDS_Bomb("Problem performing time breakdown");
  }

  if ((year && !SDDS_SetColumn(SDDSout, SDDS_BY_NAME, year, rows, conversion->yearName)) ||
      (day && !SDDS_SetColumn(SDDSout, SDDS_BY_NAME, day, rows, conversion->dayName)) ||
      (month && !SDDS_SetColumn(SDDSout, SDDS_BY_NAME, month, rows, conversion->monthName)) ||
      (jDay && !SDDS_SetColumn(SDDSout, SDDS_BY_NAME, jDay, rows, conversion->jDayName)) ||
      (hour && !SDDS_SetColumn(SDDSout, SDDS_BY_NAME, hour, rows, conversion->hourName)) ||
      (text && !SDDS_SetColumn(SDDSout, SDDS_BY_NAME, text, rows, conversion->textName)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (hour)
    free(hour);
  if (year)
    free(year);
  if (epochTime)
    free(epochTime);
  if (jDay)
    free(jDay);
  if (month)
    free(month);
  if (day)
    free(day);
  if (text) {
    for (row = 0; row < rows; row++)
      free(text[row]);
    free(text);
  }
}
