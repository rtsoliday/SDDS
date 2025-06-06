/**
 * @file sddsmakedataset.c
 * @brief Creates an SDDS file from input data provided via the command line.
 *
 * @details
 * This program allows users to define parameters, columns, and arrays with associated data,
 * and write them to an SDDS-formatted file. It supports various data types and provides options
 * for customizing the output, including ASCII and binary formats.
 *
 * @section Usage
 * ```
 * sddsmakedataset [<outputFile> | -pipe=out]
 *                 [-defaultType={double|float|long64|ulong64|long|ulong|short|ushort|string|character}]
 *                 [-parameter=<name>[,type=<string>][,units=<string>][,symbol=<string>][,description=<string>]]
 *                 [-data=<value>] -parameter=.... -data=...
 *                 [-column=<name>[,type=<string>][,units=<string>][,symbol=<string>][,description=<string>]]
 *                 [-data=<listOfCommaSeparatedValue>] -column=... -data=...
 *                 [-array=<name>[,type=string][,units=string>][,symbol=<string>][,description=<string>]]
 *                 [-data=<listOfCommaSeparatedValue>] -array=... -data=...
 *                 [-noWarnings]
 *                 [-ascii]
 *                 [-description=<string>]
 *                 [-contents=<string>]
 *                 [-append[=merge]]
 *                 [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Option                  | Description                                                                                 |
 * |-------------------------|---------------------------------------------------------------------------------------------|
 * | `-pipe`                 | Outputs data to a pipe in SDDS format instead of to a file.                                 |
 * | `-defaultType`          | Specifies the default data type for parameters and columns if not explicitly provided.      |
 * | `-parameter`            | Defines a parameter with optional attributes (type, units, symbol, description).            |
 * | `-data`                 | Provides the data value for the preceding `-parameter`, `-column`, or `-array` option.     |
 * | `-column`               | Defines a column with optional attributes (type, units, symbol, description).               |
 * | `-array`                | Defines an array with optional attributes (type, units, symbol, description).               |
 * | `-noWarnings`           | Suppresses warning messages.                                                               |
 * | `-ascii`                | Outputs the file in ASCII mode (default is binary).                                        |
 * | `-description`          | Specifies a description for the output file.                                               |
 * | `-contents`             | Specifies the contents of the description.                                                 |
 * | `-append`               | Appends data to an existing file; `merge` appends to the current page.                     |
 * | `-majorOrder`           | Specifies the major order of output: `row` or `column`.                                    |
 *
 * @subsection Incompatibilities
 *   - `-data` must follow one of `-parameter`, `-column`, or `-array`.
 *
 * @subsection SR Specific Requirements
 *   - For `-defaultType`, valid values are: `double`, `float`, `long64`, `ulong64`, `long`, `ulong`, `short`, `ushort`, `string`, `character`.
 *   - The `-append` option allows `merge` as an optional value to merge data into the current page.
 *   - If `-contents` is specified, `-description` must also be provided.
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
 * H. Shang, M. Borland, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  CLO_PARAMETER,
  CLO_COLUMN,
  CLO_DATA,
  CLO_PIPE,
  CLO_DEFAULTTYPE,
  CLO_NOWARNINGS,
  CLO_DESCRIPTION,
  CLO_CONTENTS,
  CLO_ASCII,
  CLO_MAJOR_ORDER,
  CLO_APPEND,
  CLO_ARRAY,
  N_OPTIONS
};

/* Note: only -pipe=out is valid */
char *option[N_OPTIONS] = {
  "parameter",
  "column",
  "data",
  "pipe",
  "defaultType",
  "noWarnings",
  "description",
  "contents",
  "ascii",
  "majorOrder",
  "append",
  "array",
};

char *USAGE =
  "Usage: sddsmakedataset [<outputFile> | -pipe=out]\n"
  "                [-defaultType={double|float|long64|ulong64|long|ulong|short|ushort|string|character}]\n"
  "                [-parameter=<name>[,type=<string>][,units=<string>][,symbol=<string>][,description=<string>]]\n"
  "                [-data=<value>] -parameter=.... -data=...\n"
  "                [-column=<name>[,type=<string>][,units=<string>][,symbol=<string>][,description=<string>]]\n"
  "                [-data=<listOfCommaSeparatedValue>] -column=... -data=...\n"
  "                [-array=<name>[,type=string][,units=string>][,symbol=<string>][,description=<string>]]\n"
  "                [-data=<listOfCommaSeparatedValue>] -array=... -data=...\n"
  "                [-noWarnings]\n"
  "                [-ascii]\n"
  "                [-description=<string>]\n"
  "                [-contents=<string>]\n"
  "                [-append[=merge]]\n"
  "                [-majorOrder=row|column]\n"
  "Options:\n"
  "    -defaultType=<type>\n"
  "        Specify the default data type for parameters and columns if not specified.\n"
  "        Available types: double, float, long64, ulong64, long, ulong, short, ushort, string, character.\n\n"
  "    -parameter=<name>[,type=<string>][,units=<string>][,symbol=<string>][,description=<string>]\n"
  "        Define a parameter with optional type, units, symbol, and description.\n"
  "        Must be followed by -data=<value> to provide the parameter's value.\n\n"
  "    -data=<value>\n"
  "        Provide the data value for the preceding -parameter, -column, or -array option.\n\n"
  "    -column=<name>[,type=<string>][,units=<string>][,symbol=<string>][,description=<string>]\n"
  "        Define a column with optional type, units, symbol, and description.\n"
  "        Must be followed by -data=<listOfCommaSeparatedValues> to provide the column's data.\n\n"
  "    -array=<name>[,type=<string>][,units=<string>][,symbol=<string>][,description=<string>]\n"
  "        Define an array with optional type, units, symbol, and description.\n"
  "        Must be followed by -data=<listOfCommaSeparatedValues> to provide the array's data.\n"
  "        Currently supports only one-dimensional arrays.\n\n"
  "    -noWarnings\n"
  "        Do not print out warning messages.\n\n"
  "    -ascii\n"
  "        Output file in ASCII mode. The default is binary.\n\n"
  "    -description=<string>\n"
  "        Provide a description of the output file.\n\n"
  "    -contents=<string>\n"
  "        Provide contents of the description.\n\n"
  "    -append[=merge]\n"
  "        Append data to an existing file. Use 'merge' to append to the current page.\n\n"
  "    -majorOrder=row|column\n"
  "        Specify output file in row or column major order.\n\n"
  "<outputFile>\n"
  "    SDDS output file for writing the data.\n\n"
  "-pipe=out\n"
  "    Output the data in SDDS format to the pipe instead of to a file.\n\n"
  "Program by Hairong Shang. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

/* Structures for parameters and columns */
typedef struct
{
  char *name, *dataString;
  void *data;
  char *description, *symbol, *unit, *typename;
  long type;
} PARAMETER_INFO;

typedef struct
{
  char *name, **dataList;
  void *data;
  char *description, *symbol, *unit, *typename;
  long type, rows;
} COLUMN_INFO;

/* Function prototypes */
COLUMN_INFO *InitializeColumnInfo();
PARAMETER_INFO *InitializeParameteterInfo();
void SetInfoData(PARAMETER_INFO **parameter, long parameters, COLUMN_INFO **column, long columns,
                 COLUMN_INFO **array, long arrays,
                 char *defaultType, long noWarnings, long maxrows);
void FreeMemory(PARAMETER_INFO **parameter, long parameters, COLUMN_INFO **column, long columns,
                COLUMN_INFO **array, long arrays, long maxrows);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET outTable;
  char *defaultType, *outputFile;
  unsigned long pipeFlags, dummyFlags, majorOrderFlag;
  PARAMETER_INFO **parameter;
  COLUMN_INFO **column;
  COLUMN_INFO **array;
  long parameters, columns, arrays, previousOption, i, j, i_arg, currentOption, tmpfile_used, noWarnings, maxrows = 0, outputMode;
  char *input = "obset";
  char *description, *contents;
  short columnMajorOrder = 0, append = 0;
  int64_t rowsPresent;
  int32_t colIndex, arrayIndex, dsize, startIndex;
  SDDS_ARRAY *sdds_array = NULL;

  description = contents = NULL;
  parameter = NULL;
  column = NULL;
  array = NULL;
  parameters = columns = arrays = 0;
  outputFile = defaultType = NULL;
  pipeFlags = 0;
  tmpfile_used = noWarnings = 0;
  outputMode = SDDS_BINARY;

  SDDS_RegisterProgramName(argv[0]);
  SDDS_CheckDatasetStructureSize(sizeof(SDDS_DATASET));

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "Error: Insufficient arguments provided.\n\n%s", USAGE);
    exit(EXIT_FAILURE);
  }
  previousOption = -1;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      currentOption = match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0);
      if (currentOption == CLO_DATA && previousOption != CLO_PARAMETER && previousOption != CLO_COLUMN && previousOption != CLO_ARRAY) {
        SDDS_Bomb("-data option must follow a -parameter, -column, or -array option.");
      }
      switch (currentOption) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("Invalid -majorOrder syntax or value.");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_NOWARNINGS:
        noWarnings = 1;
        break;
      case CLO_PARAMETER:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -parameter syntax.");
        parameter = SDDS_Realloc(parameter, sizeof(*parameter) * (parameters + 1));
        parameter[parameters] = InitializeParameteterInfo();
        SDDS_CopyString(&(parameter[parameters]->name), s_arg[i_arg].list[1]);
        s_arg[i_arg].list += 2;
        s_arg[i_arg].n_items -= 2;
        if (!(parameter[parameters]->name) || !strlen(parameter[parameters]->name))
          SDDS_Bomb("Invalid -parameter syntax (no name).");
        if (s_arg[i_arg].n_items > 0 &&
            !scanItemList(&dummyFlags, s_arg[i_arg].list, &s_arg[i_arg].n_items, 0,
                          "type", SDDS_STRING, &(parameter[parameters]->typename), 1, 0,
                          "units", SDDS_STRING, &(parameter[parameters]->unit), 1, 0,
                          "symbol", SDDS_STRING, &(parameter[parameters]->symbol), 1, 0,
                          "description", SDDS_STRING, &(parameter[parameters]->description), 1, 0, NULL))
          SDDS_Bomb("Invalid -parameter syntax.");
        parameters++;
        s_arg[i_arg].list -= 2;
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_COLUMN:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -column syntax.");
        column = SDDS_Realloc(column, sizeof(*column) * (columns + 1));
        column[columns] = InitializeColumnInfo();
        SDDS_CopyString(&(column[columns]->name), s_arg[i_arg].list[1]);
        s_arg[i_arg].list += 2;
        s_arg[i_arg].n_items -= 2;
        if (!(column[columns]->name) || !strlen(column[columns]->name))
          SDDS_Bomb("Invalid -column syntax (no name).");
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&dummyFlags, s_arg[i_arg].list, &s_arg[i_arg].n_items, 0,
                           "type", SDDS_STRING, &(column[columns]->typename), 1, 0,
                           "unit", SDDS_STRING, &(column[columns]->unit), 1, 0,
                           "symbol", SDDS_STRING, &(column[columns]->symbol), 1, 0,
                           "description", SDDS_STRING, &(column[columns]->description), 1, 0, NULL)))
          SDDS_Bomb("Invalid -column syntax.");
        columns++;
        s_arg[i_arg].list -= 2;
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_ARRAY:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -array syntax.");
        array = SDDS_Realloc(array, sizeof(*array) * (arrays + 1));
        array[arrays] = InitializeColumnInfo();
        SDDS_CopyString(&(array[arrays]->name), s_arg[i_arg].list[1]);
        s_arg[i_arg].list += 2;
        s_arg[i_arg].n_items -= 2;
        if (!(array[arrays]->name) || !strlen(array[arrays]->name))
          SDDS_Bomb("Invalid -array syntax (no name).");
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&dummyFlags, s_arg[i_arg].list, &s_arg[i_arg].n_items, 0,
                           "type", SDDS_STRING, &(array[arrays]->typename), 1, 0,
                           "unit", SDDS_STRING, &(array[arrays]->unit), 1, 0,
                           "symbol", SDDS_STRING, &(array[arrays]->symbol), 1, 0,
                           "description", SDDS_STRING, &(array[arrays]->description), 1, 0, NULL)))
          SDDS_Bomb("Invalid -array syntax.");
        arrays++;
        s_arg[i_arg].list -= 2;
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_DATA:
        if (previousOption == CLO_PARAMETER) {
          parameter[parameters - 1]->dataString = s_arg[i_arg].list[1];
        }
        if (previousOption == CLO_COLUMN) {
          if (((s_arg[i_arg].n_items - 1) == 1) &&
              (strlen(s_arg[i_arg].list[1]) > 1) &&
              (((column[columns - 1]->typename) && (strcmp(column[columns - 1]->typename, "character") == 0)) ||
               ((defaultType) && (strcmp(defaultType, "character") == 0)))) {
            /* Assume each character is a separate data entry for character type */
            column[columns - 1]->rows = strlen(s_arg[i_arg].list[1]);
            column[columns - 1]->dataList = malloc(sizeof(*(column[columns - 1]->dataList)) * column[columns - 1]->rows);
            for (j = 0; j < column[columns - 1]->rows; j++) {
              char buffer[2] = {s_arg[i_arg].list[1][j], '\0'};
              SDDS_CopyString(&column[columns - 1]->dataList[j], buffer);
            }
          } else {
            column[columns - 1]->rows = s_arg[i_arg].n_items - 1;
            column[columns - 1]->dataList = malloc(sizeof(*(column[columns - 1]->dataList)) * column[columns - 1]->rows);
            for (j = 0; j < column[columns - 1]->rows; j++)
              SDDS_CopyString(&column[columns - 1]->dataList[j], s_arg[i_arg].list[j + 1]);
          }
        }
        if (previousOption == CLO_ARRAY) {
          if (((s_arg[i_arg].n_items - 1) == 1) &&
              (strlen(s_arg[i_arg].list[1]) > 1) &&
              (((array[arrays - 1]->typename) && (strcmp(array[arrays - 1]->typename, "character") == 0)) ||
               ((defaultType) && (strcmp(defaultType, "character") == 0)))) {
            /* Assume each character is a separate data entry for character type */
            array[arrays - 1]->rows = strlen(s_arg[i_arg].list[1]);
            array[arrays - 1]->dataList = malloc(sizeof(*(array[arrays - 1]->dataList)) * array[arrays - 1]->rows);
            for (j = 0; j < array[arrays - 1]->rows; j++) {
              char buffer[2] = {s_arg[i_arg].list[1][j], '\0'};
              SDDS_CopyString(&array[arrays - 1]->dataList[j], buffer);
            }
          } else {
            array[arrays - 1]->rows = s_arg[i_arg].n_items - 1;
            array[arrays - 1]->dataList = malloc(sizeof(*(array[arrays - 1]->dataList)) * array[arrays - 1]->rows);
            for (j = 0; j < array[arrays - 1]->rows; j++)
              SDDS_CopyString(&array[arrays - 1]->dataList[j], s_arg[i_arg].list[j + 1]);
          }
        }
        break;
      case CLO_DEFAULTTYPE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -defaultType option.");
        SDDS_CopyString(&defaultType, s_arg[i_arg].list[1]);
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax.");
        if (pipeFlags != USE_STDOUT)
          SDDS_Bomb("Only -pipe=out syntax is valid.");
        break;
      case CLO_DESCRIPTION:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -description option.");
        SDDS_CopyString(&description, s_arg[i_arg].list[1]);
        break;
      case CLO_CONTENTS:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -contents option.");
        SDDS_CopyString(&contents, s_arg[i_arg].list[1]);
        break;
      case CLO_ASCII:
        outputMode = SDDS_ASCII;
        break;
      case CLO_APPEND:
        append = 1;
        if (s_arg[i_arg].n_items != 1) {
          append = 2;
          if (s_arg[i_arg].n_items > 2 || strncmp(s_arg[i_arg].list[1], "merge", strlen(s_arg[i_arg].list[1])) != 0)
            SDDS_Bomb("Invalid -append syntax.");
        }
        break;
      default:
        fprintf(stderr, "Error: Option %s is invalid.\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
      previousOption = currentOption;
    } else {
      if (outputFile == NULL)
        SDDS_CopyString(&outputFile, s_arg[i_arg].list[0]);
      else {
        fprintf(stderr, "Error: Too many filenames provided (%s).\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    }
  }

  if (!outputFile && !pipeFlags) {
    fprintf(stderr, "Error: Either an output file or -pipe=out must be specified.\n\n%s", USAGE);
    exit(EXIT_FAILURE);
  }
  if (outputFile && pipeFlags) {
    fprintf(stderr, "Error: Only one of output file and -pipe=out can be specified.\n\n%s", USAGE);
    exit(EXIT_FAILURE);
  }

  processFilenames("sddsmakedataset", &input, &outputFile, pipeFlags, 1, &tmpfile_used);
  if (!columns && !parameters && !arrays) {
    fprintf(stderr, "Error: No data provided for writing.\n\n%s", USAGE);
    exit(EXIT_FAILURE);
  }
  if (contents && !description) {
    if (!noWarnings) {
      fprintf(stderr, "Warning: Description text is provided for contents without a description. No description will be written.\n");
      free(contents);
      contents = NULL;
    }
  }
  for (i = 0; i < columns; i++)
    if (maxrows < column[i]->rows)
      maxrows = column[i]->rows;
  SetInfoData(parameter, parameters, column, columns, array, arrays, defaultType, noWarnings, maxrows);

  if (append == 0) {
    /* Write a new file */
    if (!SDDS_InitializeOutput(&outTable, outputMode, 1, description, contents, outputFile))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    outTable.layout.data_mode.column_major = columnMajorOrder;
    for (i = 0; i < parameters; i++) {
      if (parameter[i]->dataString) {
        if (SDDS_DefineParameter(&outTable, parameter[i]->name, parameter[i]->symbol, parameter[i]->unit, parameter[i]->description, NULL, parameter[i]->type, NULL) < 0)
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    for (i = 0; i < columns; i++) {
      if (column[i]->dataList) {
        if (SDDS_DefineColumn(&outTable, column[i]->name, column[i]->symbol, column[i]->unit, column[i]->description, NULL, column[i]->type, 0) < 0)
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    for (i = 0; i < arrays; i++) {
      if (array[i]->dataList) {
        if (SDDS_DefineArray(&outTable, array[i]->name, array[i]->symbol, array[i]->unit, array[i]->description, NULL, array[i]->type, 0, 1, NULL) < 0)
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    if (!SDDS_WriteLayout(&outTable))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (append == 1) {
    /* Append */
    if (!SDDS_InitializeAppend(&outTable, outputFile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (append == 2) {
    /* Append to page, merge */
    if (!SDDS_InitializeAppendToPage(&outTable, outputFile, maxrows, &rowsPresent))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (append) {
    /* Check the parameters, columns, and arrays */
    for (i = 0; i < parameters; i++) {
      if (parameter[i]->dataString) {
        if (SDDS_GetParameterIndex(&outTable, parameter[i]->name) < 0) {
          fprintf(stderr, "Error: Parameter '%s' does not exist in the existing file.\n", parameter[i]->name);
          exit(EXIT_FAILURE);
        }
      }
    }
    if (SDDS_ParameterCount(&outTable) != parameters) {
      fprintf(stderr, "Error: Parameter count does not match the existing file.\n");
      exit(EXIT_FAILURE);
    }
    for (i = 0; i < columns; i++) {
      if (column[i]->dataList) {
        if (SDDS_GetColumnIndex(&outTable, column[i]->name) < 0) {
          fprintf(stderr, "Error: Column '%s' does not exist in the existing file.\n", column[i]->name);
          exit(EXIT_FAILURE);
        }
      }
    }
    if (SDDS_ColumnCount(&outTable) != columns) {
      fprintf(stderr, "Error: Column count does not match the existing file.\n");
      exit(EXIT_FAILURE);
    }
    for (i = 0; i < arrays; i++) {
      if (array[i]->dataList) {
        if (SDDS_GetArrayIndex(&outTable, array[i]->name) < 0) {
          fprintf(stderr, "Error: Array '%s' does not exist in the existing file.\n", array[i]->name);
          exit(EXIT_FAILURE);
        }
      }
    }
    if (SDDS_ArrayCount(&outTable) != arrays) {
      fprintf(stderr, "Error: Array count does not match the existing file.\n");
      exit(EXIT_FAILURE);
    }
  }

  if (append == 0 || append == 1) {
    if (!SDDS_StartPage(&outTable, maxrows))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < parameters; i++) {
      if (parameter[i]->data) {
        if (!SDDS_SetParameters(&outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, parameter[i]->name, parameter[i]->data, NULL))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
    for (i = 0; i < columns; i++) {
      if (column[i]->data) {
        if (!SDDS_SetColumn(&outTable, SDDS_SET_BY_NAME, column[i]->data, maxrows, column[i]->name))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
    for (i = 0; i < arrays; i++) {
      if (array[i]->data) {
        if (!SDDS_SetArrayVararg(&outTable, array[i]->name, SDDS_CONTIGUOUS_DATA, array[i]->data, array[i]->rows))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
  }
  if (append == 2) {
    for (i = 0; i < columns; i++) {
      if (column[i]->data) {
        colIndex = SDDS_GetColumnIndex(&outTable, column[i]->name);
        for (j = 0; j < maxrows; j++) {
          switch (column[i]->type) {
          case SDDS_LONGDOUBLE:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((long double *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_DOUBLE:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((double *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_FLOAT:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((float *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_LONG64:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((int64_t *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_ULONG64:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((uint64_t *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_LONG:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((int32_t *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_ULONG:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((uint32_t *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_SHORT:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((short *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_USHORT:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((unsigned short *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_CHARACTER:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((char *)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          case SDDS_STRING:
            if (SDDS_SetRowValues(&outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowsPresent + j,
                                  colIndex, ((char **)(column[i]->data))[j], -1) == 0)
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            break;
          default:
            SDDS_Bomb("Invalid data type provided.");
            break;
          }
        }
      }
    }
    for (i = 0; i < arrays; i++) {
      if (array[i]->data) {
        arrayIndex = SDDS_GetArrayIndex(&outTable, array[i]->name);
        sdds_array = outTable.array + arrayIndex;
        dsize = SDDS_type_size[sdds_array->definition->type - 1];
        startIndex = sdds_array->elements;
        sdds_array->elements += array[i]->rows;
        sdds_array->data = SDDS_Realloc(sdds_array->data, dsize * sdds_array->elements);
        if (array[i]->type == SDDS_STRING) {
          if (!SDDS_CopyStringArray(((char **)sdds_array->data) + startIndex, array[i]->data, array[i]->rows))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        } else {
          memcpy((char *)sdds_array->data + dsize * startIndex, array[i]->data, dsize * array[i]->rows);
        }
      }
    }
  }

  if (append == 2) {
    if (!SDDS_UpdatePage(&outTable, FLUSH_TABLE) || !SDDS_Terminate(&outTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!SDDS_WritePage(&outTable) || !SDDS_Terminate(&outTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /* Free resources */
  if (tmpfile_used && !replaceFileAndBackUp(input, outputFile))
    exit(EXIT_FAILURE);
  free_scanargs(&s_arg, argc);
  FreeMemory(parameter, parameters, column, columns, array, arrays, maxrows);
  if (defaultType)
    free(defaultType);
  if (outputFile)
    free(outputFile);
  if (description)
    free(description);
  if (contents)
    free(contents);
  return EXIT_SUCCESS;
}

COLUMN_INFO *InitializeColumnInfo() {
  COLUMN_INFO *column = malloc(sizeof(*column));
  if (!column) {
    fprintf(stderr, "Error: Memory allocation failed for COLUMN_INFO.\n");
    exit(EXIT_FAILURE);
  }
  column->name = column->unit = column->description = column->symbol = NULL;
  column->data = NULL;
  column->typename = NULL;
  column->rows = 0;
  column->type = -1;
  column->dataList = NULL;
  return column;
}

PARAMETER_INFO *InitializeParameteterInfo() {
  PARAMETER_INFO *parameter = malloc(sizeof(*parameter));
  if (!parameter) {
    fprintf(stderr, "Error: Memory allocation failed for PARAMETER_INFO.\n");
    exit(EXIT_FAILURE);
  }
  parameter->name = parameter->unit = parameter->description = parameter->symbol = NULL;
  parameter->typename = NULL;
  parameter->data = NULL;
  parameter->type = -1;
  parameter->dataString = NULL;
  return parameter;
}

void SetInfoData(PARAMETER_INFO **parameter, long parameters, COLUMN_INFO **column, long columns, COLUMN_INFO **array, long arrays, char *defaultType,
                 long noWarnings, long maxrows) {
  long i, j;
  PARAMETER_INFO *par;
  COLUMN_INFO *col;
  char *type = NULL;

  for (i = 0; i < parameters; i++) {
    par = parameter[i];
    if (!par->dataString) {
      if (!noWarnings)
        fprintf(stderr, "Warning: No data provided for parameter '%s'. It will not be written to the output file.\n", par->name);
      continue;
    }
    if (par->typename)
      SDDS_CopyString(&type, par->typename);
    else {
      if (defaultType)
        SDDS_CopyString(&type, defaultType);
      else
        SDDS_CopyString(&type, "none");
    }
    if ((par->type = SDDS_IdentifyType(type)) <= 0) {
      fprintf(stderr, "Error: Invalid data type '%s' for parameter '%s'.\n", type, par->name);
      exit(EXIT_FAILURE);
    }
    par->data = malloc(SDDS_type_size[par->type - 1]);
    if (!par->data) {
      fprintf(stderr, "Error: Memory allocation failed for parameter data.\n");
      exit(EXIT_FAILURE);
    }
    switch (par->type) {
    case SDDS_LONGDOUBLE:
      *((long double *)par->data) = strtold(par->dataString, NULL);
      break;
    case SDDS_DOUBLE:
      *((double *)par->data) = atof(par->dataString);
      break;
    case SDDS_FLOAT:
      *((float *)par->data) = (float)atof(par->dataString);
      break;
    case SDDS_LONG64:
      *((int64_t *)par->data) = atoll(par->dataString);
      break;
    case SDDS_ULONG64:
      *((uint64_t *)par->data) = strtoull(par->dataString, NULL, 10);
      break;
    case SDDS_LONG:
      *((int32_t *)par->data) = atol(par->dataString);
      break;
    case SDDS_ULONG:
      *((uint32_t *)par->data) = strtoul(par->dataString, NULL, 10);
      break;
    case SDDS_SHORT:
      *((short *)par->data) = (short)atol(par->dataString);
      break;
    case SDDS_USHORT:
      *((unsigned short *)par->data) = (unsigned short)atol(par->dataString);
      break;
    case SDDS_STRING:
      cp_str((char **)par->data, par->dataString);
      break;
    case SDDS_CHARACTER:
      *((char *)par->data) = par->dataString[0];
      break;
    default:
      SDDS_Bomb("Invalid data type encountered while setting parameter data.");
      break;
    }
    free(type);
  }

  for (i = 0; i < columns; i++) {
    col = column[i];
    if (!col->dataList) {
      if (!noWarnings)
        fprintf(stderr, "Warning: No data provided for column '%s'. It will not be written to the output file.\n", col->name);
      continue;
    }
    if (col->typename)
      SDDS_CopyString(&type, col->typename);
    else {
      if (defaultType)
        SDDS_CopyString(&type, defaultType);
      else
        SDDS_CopyString(&type, "none");
    }
    if ((col->type = SDDS_IdentifyType(type)) <= 0) {
      fprintf(stderr, "Error: Invalid data type '%s' for column '%s'.\n", type, col->name);
      exit(EXIT_FAILURE);
    }
    if (col->rows < maxrows && !noWarnings)
      fprintf(stderr, "Warning: Missing data for column '%s'. Filling with zeros.\n", col->name);
    switch (col->type) {
    case SDDS_LONGDOUBLE:
      col->data = malloc(sizeof(long double) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((long double *)col->data)[j] = strtold(col->dataList[j], NULL);
      for (j = col->rows; j < maxrows; j++)
        ((long double *)col->data)[j] = 0.0L;
      break;
    case SDDS_DOUBLE:
      col->data = malloc(sizeof(double) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((double *)col->data)[j] = atof(col->dataList[j]);
      for (j = col->rows; j < maxrows; j++)
        ((double *)col->data)[j] = 0.0;
      break;
    case SDDS_FLOAT:
      col->data = malloc(sizeof(float) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((float *)col->data)[j] = (float)atof(col->dataList[j]);
      for (j = col->rows; j < maxrows; j++)
        ((float *)col->data)[j] = 0.0f;
      break;
    case SDDS_LONG64:
      col->data = malloc(sizeof(int64_t) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((int64_t *)col->data)[j] = atoll(col->dataList[j]);
      for (j = col->rows; j < maxrows; j++)
        ((int64_t *)col->data)[j] = 0;
      break;
    case SDDS_ULONG64:
      col->data = malloc(sizeof(uint64_t) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((uint64_t *)col->data)[j] = strtoull(col->dataList[j], NULL, 10);
      for (j = col->rows; j < maxrows; j++)
        ((uint64_t *)col->data)[j] = 0;
      break;
    case SDDS_LONG:
      col->data = malloc(sizeof(int32_t) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((int32_t *)col->data)[j] = (int32_t)atol(col->dataList[j]);
      for (j = col->rows; j < maxrows; j++)
        ((int32_t *)col->data)[j] = 0;
      break;
    case SDDS_ULONG:
      col->data = malloc(sizeof(uint32_t) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((uint32_t *)col->data)[j] = strtoul(col->dataList[j], NULL, 10);
      for (j = col->rows; j < maxrows; j++)
        ((uint32_t *)col->data)[j] = 0;
      break;
    case SDDS_SHORT:
      col->data = malloc(sizeof(short) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((short *)col->data)[j] = (short)atol(col->dataList[j]);
      for (j = col->rows; j < maxrows; j++)
        ((short *)col->data)[j] = 0;
      break;
    case SDDS_USHORT:
      col->data = malloc(sizeof(unsigned short) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((unsigned short *)col->data)[j] = (unsigned short)atol(col->dataList[j]);
      for (j = col->rows; j < maxrows; j++)
        ((unsigned short *)col->data)[j] = 0;
      break;
    case SDDS_CHARACTER:
      col->data = malloc(sizeof(char) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((char *)col->data)[j] = col->dataList[j][0];
      for (j = col->rows; j < maxrows; j++)
        ((char *)col->data)[j] = '\0';
      break;
    case SDDS_STRING:
      col->data = malloc(sizeof(char *) * maxrows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for column '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        SDDS_CopyString(&((char **)col->data)[j], col->dataList[j]);
      for (j = col->rows; j < maxrows; j++)
        SDDS_CopyString(&((char **)col->data)[j], "");
      break;
    default:
      SDDS_Bomb("Invalid data type encountered while setting column data.");
      break;
    }
    free(type);
  }

  for (i = 0; i < arrays; i++) {
    col = array[i];
    if (!col->dataList) {
      if (!noWarnings)
        fprintf(stderr, "Warning: No data provided for array '%s'. It will not be written to the output file.\n", col->name);
      continue;
    }
    if (col->typename)
      SDDS_CopyString(&type, col->typename);
    else {
      if (defaultType)
        SDDS_CopyString(&type, defaultType);
      else
        SDDS_CopyString(&type, "none");
    }
    if ((col->type = SDDS_IdentifyType(type)) <= 0) {
      fprintf(stderr, "Error: Invalid data type '%s' for array '%s'.\n", type, col->name);
      exit(EXIT_FAILURE);
    }
    switch (col->type) {
    case SDDS_LONGDOUBLE:
      col->data = malloc(sizeof(long double) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((long double *)col->data)[j] = strtold(col->dataList[j], NULL);
      break;
    case SDDS_DOUBLE:
      col->data = malloc(sizeof(double) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((double *)col->data)[j] = atof(col->dataList[j]);
      break;
    case SDDS_FLOAT:
      col->data = malloc(sizeof(float) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((float *)col->data)[j] = (float)atof(col->dataList[j]);
      break;
    case SDDS_LONG64:
      col->data = malloc(sizeof(int64_t) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((int64_t *)col->data)[j] = atoll(col->dataList[j]);
      break;
    case SDDS_ULONG64:
      col->data = malloc(sizeof(uint64_t) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((uint64_t *)col->data)[j] = strtoull(col->dataList[j], NULL, 10);
      break;
    case SDDS_LONG:
      col->data = malloc(sizeof(int32_t) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((int32_t *)col->data)[j] = (int32_t)atol(col->dataList[j]);
      break;
    case SDDS_ULONG:
      col->data = malloc(sizeof(uint32_t) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((uint32_t *)col->data)[j] = strtoul(col->dataList[j], NULL, 10);
      break;
    case SDDS_SHORT:
      col->data = malloc(sizeof(short) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((short *)col->data)[j] = (short)atol(col->dataList[j]);
      break;
    case SDDS_USHORT:
      col->data = malloc(sizeof(unsigned short) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((unsigned short *)col->data)[j] = (unsigned short)atol(col->dataList[j]);
      break;
    case SDDS_CHARACTER:
      col->data = malloc(sizeof(char) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        ((char *)col->data)[j] = col->dataList[j][0];
      break;
    case SDDS_STRING:
      col->data = malloc(sizeof(char *) * col->rows);
      if (!col->data) {
        fprintf(stderr, "Error: Memory allocation failed for array '%s' data.\n", col->name);
        exit(EXIT_FAILURE);
      }
      for (j = 0; j < col->rows; j++)
        SDDS_CopyString(&((char **)col->data)[j], col->dataList[j]);
      break;
    default:
      SDDS_Bomb("Invalid data type encountered while setting array data.");
      break;
    }
    free(type);
  }
}

void FreeMemory(PARAMETER_INFO **parameter, long parameters, COLUMN_INFO **column, long columns, COLUMN_INFO **array, long arrays, long maxrows) {
  long i, j;

  /* Free parameters */
  for (i = 0; i < parameters; i++) {
    if (parameter[i]->name)
      free(parameter[i]->name);
    if (parameter[i]->data)
      free(parameter[i]->data);
    if (parameter[i]->unit)
      free(parameter[i]->unit);
    if (parameter[i]->description)
      free(parameter[i]->description);
    if (parameter[i]->symbol)
      free(parameter[i]->symbol);
    if (parameter[i]->typename)
      free(parameter[i]->typename);
    free(parameter[i]);
  }
  if (parameters)
    free(parameter);

  /* Free columns */
  for (i = 0; i < columns; i++) {
    if (column[i]->name)
      free(column[i]->name);
    if (column[i]->dataList) {
      for (j = 0; j < column[i]->rows; j++)
        free(column[i]->dataList[j]);
      free(column[i]->dataList);
    }
    if (column[i]->type != SDDS_STRING) {
      if (column[i]->data)
        free(column[i]->data);
    } else {
      if (column[i]->data) {
        for (j = 0; j < maxrows; j++)
          free(((char **)column[i]->data)[j]);
        free(column[i]->data);
      }
    }
    if (column[i]->unit)
      free(column[i]->unit);
    if (column[i]->symbol)
      free(column[i]->symbol);
    if (column[i]->description)
      free(column[i]->description);
    free(column[i]);
  }
  if (columns)
    free(column);

  /* Free arrays */
  for (i = 0; i < arrays; i++) {
    if (array[i]->name)
      free(array[i]->name);
    if (array[i]->dataList) {
      for (j = 0; j < array[i]->rows; j++)
        free(array[i]->dataList[j]);
      free(array[i]->dataList);
    }
    if (array[i]->type != SDDS_STRING) {
      if (array[i]->data)
        free(array[i]->data);
    } else {
      if (array[i]->data) {
        for (j = 0; j < array[i]->rows; j++)
          free(((char **)array[i]->data)[j]);
        free(array[i]->data);
      }
    }
    if (array[i]->unit)
      free(array[i]->unit);
    if (array[i]->symbol)
      free(array[i]->symbol);
    if (array[i]->description)
      free(array[i]->description);
    free(array[i]);
  }
  if (arrays)
    free(array);
}
