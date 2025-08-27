/**
 * @file csv2sdds.c
 * @brief Converts Comma Separated Values (CSV) data to SDDS format.
 *
 * @details
 * This program reads CSV data from an input file or standard input and converts it into the 
 * SDDS (Self Describing Data Sets) format, writing the output to a specified file or standard output.
 * It provides various options to customize the conversion process, including handling delimiters, 
 * specifying column data types, and more. The tool also validates combinations of options and applies
 * specific requirements to ensure proper operation.
 *
 * @section Usage
 * ```
 * csv2sdds [<inputFile>] [<outputFile>]
 *          [-pipe[=in][,out]]
 *          [-asciiOutput] 
 *          [-spanLines] 
 *          [-maxRows=<number>]
 *          [-schfile=<filename>] 
 *          [-skiplines=<number>]
 *          [-delimiters=start=<start>,end=<char>] 
 *          [-separator=<char>]
 *          [-columnData=name=<name>,type=<type>,units=<units>...]
 *          [-uselabels[=units]] 
 *          [-majorOrder=row|column]
 *          [-fillIn=<zero|last>]
 * ```
 *
 * @section Options
 * | Option                                | Description                                                                          |
 * |---------------------------------------|--------------------------------------------------------------------------------------|
 * | `-pipe`                               | SDDS toolkit pipe option.                                                           |
 * | `-asciiOutput`                        | Requests SDDS ASCII output. Default is binary.                                      |
 * | `-spanLines`                          | Ignore line breaks in parsing the input data.                                       |
 * | `-maxRows`                            | Maximum number of rows to expect in input.                                          |
 * | `-schfile`                            | Specifies the SCH file that describes the columns.                                  |
 * | `-skiplines`                          | Skip the first `<number>` lines of the input file.                                  |
 * | `-delimiters`                         | Specifies the delimiter characters that bracket fields. Default is `"`.            |
 * | `-separator`                          | Specifies the separator character between fields. Default is `,`.                   |
 * | `-columnData`                         | Specifies column data details corresponding to the input file columns.             |
 * | `-uselabels`                          | Defines column names and optionally units from the file headers.                    |
 * | `-majorOrder`                         | Specifies the output file major order: row-major or column-major.                   |
 * | `-fillIn`                             | Use `0` or the last value for empty cells. Default is `0`.                          |
 *
 * @subsection Incompatibilities
 *   - Only one of the following may be specified:
 *     - `-columnData`
 *     - `-schfile`
 *     - `-uselabels`
 *   - For `-separator`:
 *     - Must be a single character.
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
 * M. Borland, R. Soliday, D. Blachowicz, H. Shang, L. Emery
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_ASCIIOUTPUT,
  SET_DELIMITERS,
  SET_SEPARATOR,
  SET_COLUMNDATA,
  SET_SCHFILE,
  SET_PIPE,
  SET_SPANLINES,
  SET_MAXROWS,
  SET_SKIPLINES,
  SET_USELABELS,
  SET_MAJOR_ORDER,
  SET_FILL_IN,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "asciioutput",
  "delimiters",
  "separator",
  "columndata",
  "schfile",
  "pipe",
  "spanlines",
  "maxrows",
  "skiplines",
  "uselabels",
  "majorOrder",
  "fillIn",
};

char *USAGE =
  "\n"
  "  csv2sdds [<inputFile>] [<outputFile>]\n"
  "           [-pipe[=in][,out]]\n"
  "           [-asciiOutput] \n"
  "           [-spanLines] \n"
  "           [-maxRows=<number>]\n"
  "           [-schfile=<filename>] \n"
  "           [-skiplines=<number>]\n"
  "           [-delimiters=start=<start>,end=<char>] \n"
  "           [-separator=<char>]\n"
  "           [-columnData=name=<name>,type=<type>,units=<units>...]\n"
  "           [-uselabels[=units]] \n"
  "           [-majorOrder=row|column]\n"
  "           [-fillIn=<zero|last>]\n"
  "Options:\n"
  "  -pipe[=in][,out]                      SDDS toolkit pipe option.\n"
  "  -asciiOutput                          Requests SDDS ASCII output. Default is binary.\n"
  "  -spanLines                            Ignore line breaks in parsing the input data.\n"
  "  -maxRows=<number>                     Maximum number of rows to expect in input.\n"
  "  -schfile=<filename>                   Specifies the SCH file that describes the columns.\n"
  "  -skiplines=<number>                   Skip the first <number> lines of the input file.\n"
  "  -delimiters=start=<char>,end=<char>   Specifies the delimiter characters that bracket fields.\n"
  "                                        The default is '\"' for both start and end delimiters.\n"
  "  -separator=<char>                     Specifies the separator character between fields. The default is ','.\n"
  "  -columnData=name=<name>,type=<type>,units=<units>...\n"
  "                                        Specifies column data details. Must be provided in the order\n"
  "                                        corresponding to the data columns in the input file.\n"
  "  -uselabels[=units]                    Defines column names and optionally units from the file headers.\n"
  "  -majorOrder=row|column                Specifies the output file major order. Choose between row-major or column-major.\n"
  "  -fillIn=<zero|last>                   Use '0' or the last value for empty cells. The default is '0'.\n\n"
  "Description:\n"
  "  Converts Comma Separated Values (CSV) data to the SDDS format.\n"
  "  Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

typedef struct
{
  char *name, *units;
  long type, index;
} COLUMN_DATA;

long ParseSchFile(char *file, COLUMN_DATA **columnData, char *separator, char *startDelim, char *endDelim);
void SetUpOutputFile(SDDS_DATASET *SDDSout, char *input, char *output, COLUMN_DATA *columnData, long columns,
                     long asciiOutput, short columnMajorOrder);
char *getToken(char *s, char separator, char startDelim, char endDelim, char *buffer);
void writeOneRowToOutputFile(SDDS_DATASET *SDDSout, char *ptr, char separator, char startDelim, char endDelim,
                             long spanLines, COLUMN_DATA *columnData, long columns, int64_t rows, short fillInZero);
void lowerstring(char *ptr);

int main(int argc, char **argv) {
  FILE *fpi;
  char *input, *output, *schFile;
  SDDS_DATASET SDDSout;
  SCANNED_ARG *scanned;
  long i, iArg;
  int64_t rows, maxRows;
  long asciiOutput, columns, spanLines, skipLines = 0, lines;
  short columnlabels = 0, unitlabels = 0, uselabels = 0;
  COLUMN_DATA *columnData;
  char separator, startDelim, endDelim;
  char s[10240], *ptr, *typeName, *unitsName;
  unsigned long dummyFlags, pipeFlags, majorOrderFlag, fillInFlag;
  short columnMajorOrder = 0, fillInZero = 1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    bomb(NULL, USAGE);
  }
  input = output = schFile = NULL;
  asciiOutput = spanLines = columns = 0;
  pipeFlags = 0;
  columnData = NULL;
  separator = ',';
  startDelim = '\"';
  endDelim = '\"';
  maxRows = 10000;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
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
      case SET_ASCIIOUTPUT:
        asciiOutput = 1;
        break;
      case SET_FILL_IN:
        fillInFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 &&
            (!scanItemList(&fillInFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                           "zero", -1, NULL, 0, 0x0001UL,
                           "last", -1, NULL, 0, 0x0002UL, NULL)))
          SDDS_Bomb("invalid -fillIn syntax/values");
        if (fillInFlag & 0x0001UL)
          fillInZero = 1;
        else if (fillInFlag & 0x0002UL)
          fillInZero = 0;
        break;
      case SET_DELIMITERS:
        if (!(scanned[iArg].n_items -= 1) ||
            !scanItemList(&dummyFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "start", SDDS_CHARACTER, &startDelim, 1, 0,
                          "end", SDDS_CHARACTER, &endDelim, 1, 0, NULL)) {
          SDDS_Bomb("invalid -delimiters syntax");
        }
        scanned[iArg].n_items++;
        break;
      case SET_SEPARATOR:
        if (scanned[iArg].n_items != 2 || strlen(scanned[iArg].list[1]) < 1)
          SDDS_Bomb("invalid -separator syntax");
        interpret_escapes(scanned[iArg].list[1]);
        separator = scanned[iArg].list[1][0];
        break;
      case SET_COLUMNDATA:
        columnData = SDDS_Realloc(columnData, sizeof(*columnData) * (columns + 1));
        columnData[columns].name = NULL;
        columnData[columns].units = NULL;
        unitsName = NULL;
        typeName = "string";
        if (!(scanned[iArg].n_items -= 1) ||
            !scanItemList(&dummyFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "name", SDDS_STRING, &(columnData[columns].name), 1, 0,
                          "units", SDDS_STRING, &unitsName, 1, 0,
                          "type", SDDS_STRING, &typeName, 1, 0, NULL) ||
            !columnData[columns].name ||
            !strlen(columnData[columns].name) ||
            !typeName ||
            !(columnData[columns].type = SDDS_IdentifyType(typeName)))
          SDDS_Bomb("invalid -columnData syntax");
        scanned[iArg].n_items++;
        columnData[columns].units = unitsName;
        columns++;
        break;
      case SET_SCHFILE:
        if (scanned[iArg].n_items != 2)
          SDDS_Bomb("invalid -schFile syntax");
        schFile = scanned[iArg].list[1];
        if (!fexists(schFile)) {
          fprintf(stderr, "File not found: %s (csv2sdds)\n", schFile);
          exit(EXIT_FAILURE);
        }
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_SPANLINES:
        spanLines = 1;
        break;
      case SET_MAXROWS:
        if (scanned[iArg].n_items != 2 ||
            strlen(scanned[iArg].list[1]) < 1 ||
            sscanf(scanned[iArg].list[1], "%" SCNd64, &maxRows) != 1 ||
            maxRows < 1)
          SDDS_Bomb("invalid -maxRows syntax");
        break;
      case SET_SKIPLINES:
        if (scanned[iArg].n_items != 2 ||
            strlen(scanned[iArg].list[1]) < 1 ||
            sscanf(scanned[iArg].list[1], "%ld", &skipLines) != 1 ||
            skipLines < 1)
          SDDS_Bomb("invalid -skiplines syntax");
        break;
      case SET_USELABELS:
        if (scanned[iArg].n_items > 2)
          SDDS_Bomb("invalid -uselabels syntax");
        uselabels = 1;
        columnlabels = 1;
        if (scanned[iArg].n_items == 2)
          unitlabels = 1;
        break;
      default:
        bomb("Invalid option encountered.", USAGE);
        break;
      }
    } else {
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else {
        bomb("Too many filenames provided.", USAGE);
      }
    }
  }

  if (!columns && !schFile && !columnlabels)
    SDDS_Bomb("Specify at least one of -columnData, -schFile, or -uselabels options.");
  if (columns && schFile)
    SDDS_Bomb("Specify either -columnData options or -schFile option, not both.");
  if (columns && columnlabels)
    SDDS_Bomb("Specify either -columnData options or -uselabels option, not both.");
  if (schFile && columnlabels)
    SDDS_Bomb("Specify either -schFile option or -uselabels option, not both.");

  processFilenames("csv2sdds", &input, &output, pipeFlags, 0, NULL);
  if (input) {
    if (!fexists(input))
      SDDS_Bomb("Input file not found.");
    if (!(fpi = fopen(input, "r")))
      SDDS_Bomb("Problem opening input file.");
  } else {
    fpi = stdin;
  }

  if (!columnlabels) {
    if (!columns && !(columns = ParseSchFile(schFile, &columnData, &separator, &startDelim, &endDelim)))
      SDDS_Bomb("Problem reading or parsing SCH file.");

    SetUpOutputFile(&SDDSout, input, output, columnData, columns, asciiOutput, columnMajorOrder);

    if (!SDDS_StartPage(&SDDSout, maxRows))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  rows = 0; /* the row index we are storing in */
  lines = 0;

  while (fgets(s, sizeof(s), fpi)) {
    lines++;
    /* Convert unprintable characters to null */
    while ((i = strlen(s)) && s[i - 1] < 27)
      s[i - 1] = 0;
    /* Ignore empty lines after skipping specified number of lines */
    if (strlen(s) == 0 && (skipLines && (lines > skipLines)))
      break;
    ptr = s;
#if defined(DEBUG)
    fprintf(stderr, "line: >%s<\n", ptr);
#endif
    if (columnlabels && (!skipLines || (lines > skipLines))) {
      char t[10240];
      t[0] = 0;
      while (1) {
        ptr = getToken(ptr, separator, startDelim, endDelim, t);
        if (strlen(t) == 0)
          break;
        columnData = SDDS_Realloc(columnData, sizeof(*columnData) * (columns + 1));
        columnData[columns].name = malloc(strlen(t) + 1);
        replace_chars(t, (char *)" ", (char *)"_");
        sprintf(columnData[columns].name, "%s", t);
        columnData[columns].units = NULL;
        columnData[columns].type = SDDS_STRING;
        columns++;
      }
      columnlabels = 0;
      continue;
    } else if (unitlabels && (!skipLines || (lines > skipLines))) {
      char t[10240];
      t[0] = 0;
      for (i = 0; i < columns; i++) {
        ptr = getToken(ptr, separator, startDelim, endDelim, t);
        if (strlen(t) > 0) {
          columnData[i].units = malloc(strlen(t) + 1);
          sprintf(columnData[i].units, "%s", t);
        } else {
          columnData[i].units = NULL;
        }
      }
      unitlabels = 0;
      continue;
    }
    if (uselabels) {
      char *tmpPtr;
      char t[10240];
      double tD;
      t[0] = 0;
      tmpPtr = ptr;
      for (i = 0; i < columns; i++) {
        tmpPtr = getToken(tmpPtr, separator, startDelim, endDelim, t);
        if (strlen(t) == 0)
          break;
        if (sscanf(t, "%lf", &tD) == 1)
          columnData[i].type = SDDS_DOUBLE;
      }
      SetUpOutputFile(&SDDSout, input, output, columnData, columns, asciiOutput, columnMajorOrder);
      if (!SDDS_StartPage(&SDDSout, maxRows))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      uselabels = 0;
    }
    if (!skipLines || (lines > skipLines)) {
      writeOneRowToOutputFile(&SDDSout, ptr, separator, startDelim, endDelim, spanLines, columnData, columns, rows, fillInZero);
      rows++;
    }
    if (rows >= maxRows - 1) {
      if (!SDDS_LengthenTable(&SDDSout, 1000)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      maxRows += 1000;
    }
  }

  fclose(fpi);
  if (!SDDS_WritePage(&SDDSout) || !SDDS_Terminate(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  free_scanargs(&scanned, argc);

  return EXIT_SUCCESS;
}

long ParseSchFile(char *file, COLUMN_DATA **columnData, char *separator, char *startDelim, char *endDelim) {
  FILE *fp;
  char s[10240], *ptr, *ptr0;
  long l, fieldIndex, lastFieldIndex, columns;

  if (!(fp = fopen(file, "r"))) {
    SDDS_Bomb("Unable to open SCH file");
  }

  lastFieldIndex = 0;
  columns = 0;
  while (fgets(s, sizeof(s), fp)) {
    while ((l = strlen(s)) && s[l - 1] < 27)
      s[l - 1] = 0;
    if (strlen(s) == 0)
      continue;
    if (!(ptr = strchr(s, '=')))
      continue;
    *ptr++ = 0;
    if (strcmp(s, "Filetype") == 0) {
      if (strcmp(ptr, "Delimited"))
        SDDS_Bomb("Require Filetype = Delimited in SCH file.");
    } else if (strcmp(s, "Separator") == 0) {
      if (!(*separator = *ptr))
        SDDS_Bomb("Null separator in SCH file.");
    } else if (strcmp(s, "Delimiter") == 0) {
      if (!(*endDelim = *startDelim = *ptr))
        SDDS_Bomb("Null delimiter in SCH file.");
    } else if (strcmp(s, "CharSet") == 0) {
      if (strcmp(ptr, "ascii"))
        SDDS_Bomb("Require CharSet = ascii in SCH file.");
    } else if (strncmp(s, "Field", strlen("Field")) == 0) {
      if (!sscanf(s, "Field%ld", &fieldIndex))
        SDDS_Bomb("Error scanning field index in SCH file.");
      if (fieldIndex - lastFieldIndex != 1)
        SDDS_Bomb("Gap or nonmonotonicity in field index values.");
      lastFieldIndex = fieldIndex;
      *columnData = SDDS_Realloc(*columnData, sizeof(**columnData) * (columns + 1));
      delete_chars(ptr, " ");
      ptr0 = ptr;
      if (!(ptr = strchr(ptr0, ',')))
        SDDS_Bomb("Field name not found.");
      *ptr = 0;
      SDDS_CopyString(&((*columnData)[columns].name), ptr0);
      (*columnData)[columns].units = NULL;
      ptr0 = ptr + 1;
      if (!(ptr = strchr(ptr0, ',')))
        SDDS_Bomb("Field type not found.");
      *ptr = 0;

      lowerstring(ptr0);
      if (strcmp(ptr0, "string") == 0)
        (*columnData)[columns].type = SDDS_STRING;
      else if (strcmp(ptr0, "char") == 0)
        (*columnData)[columns].type = SDDS_STRING;
      else if (strcmp(ptr0, "float") == 0)
        (*columnData)[columns].type = SDDS_FLOAT;
      else if (strcmp(ptr0, "double") == 0)
        (*columnData)[columns].type = SDDS_DOUBLE;
      else {
        fprintf(stderr, "Unknown type '%s' given to '%s'\n", ptr0, (*columnData)[columns].name);
        exit(EXIT_FAILURE);
      }
      columns++;
    } else {
      fprintf(stderr, "Warning: unknown tag value in SCH file: %s\n", s);
    }
  }
  fclose(fp);
  return columns;
}

void SetUpOutputFile(SDDS_DATASET *SDDSout, char *input, char *output, COLUMN_DATA *columnData, long columns, long asciiOutput, short columnMajorOrder) {
  char s[10240];
  long i;

  sprintf(s, "csv2sdds conversion of %s", input ? input : "stdin");

  if (!SDDS_InitializeOutput(SDDSout, asciiOutput ? SDDS_ASCII : SDDS_BINARY, 1, NULL, s, output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  SDDSout->layout.data_mode.column_major = columnMajorOrder;
  for (i = 0; i < columns; i++) {
    if ((columnData[i].index = SDDS_DefineColumn(SDDSout, columnData[i].name, NULL, columnData[i].units, NULL, NULL, columnData[i].type, 0)) < 0) {
      sprintf(s, "Problem defining column %s.", columnData[i].name);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

char *getToken(char *s,         /* the string to be scanned */
               char separator,  /* typically , */
               char startDelim, /* typically " */
               char endDelim,   /* typically " */
               char *buffer     /* place to put the result */
               ) {
  char *ptr;
  if (*s == 0) {
    buffer[0] = 0;
    return s;
  }

  if (*s == separator) {
    /* zero-length token */
    buffer[0] = 0;
    /* advance to next position */
    return s + 1;
  }

  /* Check for quotes. If found, return quote-bounded data. */
  if (*s == startDelim) {
    s++;
    ptr = s;
    while (*ptr) {
      if (*ptr == endDelim && *(ptr - 1) != '\\') {
        ptr++;
        break;
      }
      ptr++;
    }
    strncpy(buffer, s, ptr - s - 1);
    buffer[ptr - s - 1] = 0;
    interpret_escaped_quotes(buffer);
    if (*ptr && *ptr == separator)
      return ptr + 1;
    return ptr;
  }

  /* advance until the next separator is found */
  ptr = s;
  while (*ptr && *ptr != separator)
    ptr++;
  if (*ptr == separator) {
    strncpy(buffer, s, ptr - s);
    buffer[ptr - s] = 0;
    return ptr + 1;
  }
  strcpy(buffer, s);
  buffer[ptr - s] = 0;
  return ptr;
}

void writeOneRowToOutputFile(SDDS_DATASET *SDDSout, char *ptr, char separator, char startDelim, char endDelim, long spanLines, COLUMN_DATA *columnData, long columns, int64_t rows, short fillInZero) {
  int column = 0;
  char t[10240];
  double doubleValue;
  float floatValue;
  short shortValue;
  unsigned short ushortValue;
  long nullData = 0;
  int32_t longValue;
  uint32_t ulongValue;
  int64_t long64Value;
  uint64_t ulong64Value;
  char charValue;
  t[0] = 0;

  for (column = 0; column < columns; column++) {
    ptr = getToken(ptr, separator, startDelim, endDelim, t);
#if defined(DEBUG)
    fprintf(stderr, "token: >%s<\n", t);
#endif
    nullData = 0;
    if (strlen(trim_spaces(t)) == 0)
      nullData = 1;
    if (nullData && spanLines) {
      break;
    }
    switch (columnData[column].type) {
    case SDDS_SHORT:
      if (nullData || sscanf(t, "%hd", &shortValue) != 1) {
        if (fillInZero) {
          shortValue = 0;
        } else {
          if (rows == 0)
            shortValue = 0;
          else
            shortValue = ((short *)SDDSout->data[columnData[column].index])[rows - 1];
        }
      }
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, shortValue, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    case SDDS_USHORT:
      if (nullData || sscanf(t, "%hu", &ushortValue) != 1) {
        if (fillInZero) {
          ushortValue = 0;
        } else {
          if (rows == 0)
            ushortValue = 0;
          else
            ushortValue = ((unsigned short *)SDDSout->data[columnData[column].index])[rows - 1];
        }
      }
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, ushortValue, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    case SDDS_LONG:
      if (nullData || sscanf(t, "%" SCNd32, &longValue) != 1) {
        if (fillInZero) {
          longValue = 0;
        } else {
          if (rows == 0)
            longValue = 0;
          else
            longValue = ((int32_t *)SDDSout->data[columnData[column].index])[rows - 1];
        }
      }
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, longValue, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    case SDDS_ULONG:
      if (nullData || sscanf(t, "%" SCNu32, &ulongValue) != 1) {
        if (fillInZero) {
          ulongValue = 0;
        } else {
          if (rows == 0)
            ulongValue = 0;
          else
            ulongValue = ((uint32_t *)SDDSout->data[columnData[column].index])[rows - 1];
        }
      }
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, ulongValue, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    case SDDS_LONG64:
      if (nullData || sscanf(t, "%" SCNd64, &long64Value) != 1) {
        if (fillInZero) {
          long64Value = 0;
        } else {
          if (rows == 0)
            long64Value = 0;
          else
            long64Value = ((int64_t *)SDDSout->data[columnData[column].index])[rows - 1];
        }
      }
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, long64Value, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    case SDDS_ULONG64:
      if (nullData || sscanf(t, "%" SCNu64, &ulong64Value) != 1) {
        if (fillInZero) {
          ulong64Value = 0;
        } else {
          if (rows == 0)
            ulong64Value = 0;
          else
            ulong64Value = ((uint64_t *)SDDSout->data[columnData[column].index])[rows - 1];
        }
      }
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, ulong64Value, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    case SDDS_FLOAT:
      if (nullData || sscanf(t, "%f", &floatValue) != 1) {
        if (fillInZero) {
          floatValue = 0.0f;
        } else {
          if (rows == 0)
            floatValue = 0.0f;
          else
            floatValue = ((float *)SDDSout->data[columnData[column].index])[rows - 1];
        }
      }
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, floatValue, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    case SDDS_DOUBLE:
      if (nullData || sscanf(t, "%lf", &doubleValue) != 1) {
        if (fillInZero) {
          doubleValue = 0.0;
        } else {
          if (rows == 0)
            doubleValue = 0.0;
          else
            doubleValue = ((double *)SDDSout->data[columnData[column].index])[rows - 1];
        }
      }
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, doubleValue, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    case SDDS_CHARACTER:
      if (nullData || sscanf(t, "%c", &charValue) != 1) {
        if (fillInZero) {
          charValue = 0;
        } else {
          if (rows == 0)
            charValue = 0;
          else
            charValue = ((char *)SDDSout->data[columnData[column].index])[rows - 1];
        }
      }
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, charValue, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    case SDDS_STRING:
      SDDS_InterpretEscapes(t);
      if (!SDDS_SetRowValues(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rows, columnData[column].index, t, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      break;
    default:
      SDDS_Bomb("Unknown or unsupported data type encountered.");
    }
  }
}

void lowerstring(char *ptr) {
  int size, i;
  size = strlen(ptr);
  for (i = 0; i < size; i++)
    ptr[i] = tolower((unsigned char)ptr[i]);
}
