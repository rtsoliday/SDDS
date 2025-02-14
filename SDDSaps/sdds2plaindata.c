/**
 * @file sdds2plaindata.c
 * @brief Converts SDDS files to plain data format (ASCII or binary).
 *
 * @details
 * This program reads an SDDS (Self Describing Data Set) file and converts its content 
 * to a plain data file. The output format, ordering, and included data elements can
 * be customized using various options. Both ASCII and binary output modes are supported.
 *
 * @section Usage
 * ```
 * sdds2plaindata [<inputfile>] [<outputfile>]
 *                [-pipe[=input][,output]]
 *                [-outputMode={ascii|binary}]
 *                [-separator=<string>]
 *                [-noRowCount]
 *                [-order={rowMajor|columnMajor}]
 *                [-parameter=<name>[,format=<string>]...]
 *                [-column=<name>[,format=<string>]...]
 *                [-labeled]
 *                [-nowarnings]
 * ```
 *
 * @section Options
 * | Option                  | Description                                                                 |
 * |-------------------------|-----------------------------------------------------------------------------|
 * | `-pipe`                | Use pipes for input and/or output.                                          |
 * | `-outputMode`          | Specify output format as ASCII or binary. Default is ASCII.                |
 * | `-separator`           | Define the column separator string in ASCII mode.                          |
 * | `-noRowCount`          | Exclude the row count from the output.                                      |
 * | `-order`               | Specify data ordering: rowMajor (default) or columnMajor.                  |
 * | `-parameter`           | Include specific parameters in the output. Optionally specify a format.    |
 * | `-column`              | Include specific columns in the output. Supports wildcards and formatting. |
 * | `-labeled`             | Add labels for parameters or columns in ASCII mode.                        |
 * | `-nowarnings`          | Suppress warning messages.                                                 |
 *
 * @subsection Incompatibilities
 *   - `-outputMode=binary` is incompatible with `-noRowCount`.
 *   - `-labeled` is only valid for `-outputMode=ascii`.
 *
 * @subsection SR Specific Requirements
 *   - `-separator` is valid only in `-outputMode=ascii`.
 *   - At least one of `-parameter` or `-column` must be specified.
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
 * R. Soliday, M. Borland, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#if defined(_WIN32)
#  include <fcntl.h>
#  include <io.h>
#  if defined(__BORLANDC__)
#    define _setmode(handle, amode) setmode(handle, amode)
#  endif
#endif

#define ASCII_MODE 0
#define BINARY_MODE 1
#define MODES 2
static char *mode_name[MODES] = {
  "ascii",
  "binary",
};

#define ROW_ORDER 0
#define COLUMN_ORDER 1
#define ORDERS 2
static char *order_names[ORDERS] = {
  "rowMajor",
  "columnMajor",
};

/* Enumeration for option types */
enum option_type {
  SET_OUTPUTMODE,
  SET_SEPARATOR,
  SET_NOROWCOUNT,
  SET_PARAMETER,
  SET_COLUMN,
  SET_PIPE,
  SET_NOWARNINGS,
  SET_ORDER,
  SET_LABELED,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "outputMode",
  "separator",
  "noRowCount",
  "parameter",
  "column",
  "pipe",
  "nowarnings",
  "order",
  "labeled",
};

char *USAGE =
  "sdds2plaindata [<input>] [<output>]\n"
  "               [-pipe=[input][,output]]\n"
  "               [-outputMode={ascii|binary}]\n"
  "               [-separator=<string>]\n"
  "               [-noRowCount]\n"
  "               [-order={rowMajor|columnMajor}]\n"
  "               [-parameter=<name>[,format=<string>]...]\n"
  "               [-column=<name>[,format=<string>]...]\n"
  "               [-labeled]\n"
  "               [-nowarnings]\n\n"
  "Options:\n"
  "  -outputMode       Specify output format: ascii or binary.\n"
  "  -separator        Define the column separator string in ASCII mode.\n"
  "  -noRowCount       Exclude the number of rows from the output file.\n"
  "                     (Note: Binary mode always includes row count.)\n"
  "  -order            Set data ordering: rowMajor (default) or columnMajor.\n"
  "  -parameter        Include specified parameters in the output. Optionally specify a format.\n"
  "  -column           Include specified columns in the output. Supports wildcards and optional format.\n"
  "  -labeled          Add labels for each parameter or column in ASCII mode.\n"
  "  -nowarnings       Suppress warning messages.\n\n"
  "Program by Robert Soliday. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

/* ********** */

int main(int argc, char **argv) {
  FILE *fileID;
  SDDS_LAYOUT *layout = NULL;
  SDDS_FILEBUFFER *fBuffer = NULL;

  SDDS_DATASET SDDS_dataset, SDDS_dummy;
  SCANNED_ARG *s_arg;
  long i_arg, retval, page_number = 0, size, columnOrder = 0;
  int64_t i, j;
  int64_t rows = 0;
  int32_t rows32;
  char *input, *output;
  unsigned long pipeFlags = 0, flags = 0;
  long noWarnings = 0, tmpfile_used = 0;
  long labeled = 0;

  long *parameterType, *columnType, *parameterIndex;
  int32_t *columnIndex;
  char **parameterUnits;
  void **columnData;
  char *buffer;
  static char printBuffer[SDDS_MAXLINE * 16];
  static char formatbuffer[100], formatbuffer2[100];

  long binary = 0, noRowCount = 0;
  char *separator, *ptr = NULL;
  char **parameter, **parameterFormat;
  char **column, **columnFormat, **columnUnits, **columnMatch, **columnMatchFormat, **columnName;
  long parameters = 0, columns = 0, columnMatches = 0;
  int32_t columnNames = 0;

  input = output = NULL;
  parameter = column = parameterFormat = columnFormat = columnMatch = columnMatchFormat = columnName = NULL;
  separator = NULL;

  parameterType = columnType = parameterIndex = NULL;
  columnIndex = NULL;
  columnData = NULL;
  parameterUnits = columnUnits = NULL;

  buffer = tmalloc(sizeof(char) * 16);

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_OUTPUTMODE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -outputMode syntax");
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case ASCII_MODE:
          binary = 0;
          break;
        case BINARY_MODE:
          binary = 1;
          break;
        default:
          SDDS_Bomb("invalid -outputMode syntax");
          break;
        }
        break;
      case SET_SEPARATOR:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -separator syntax");
        separator = s_arg[i_arg].list[1];
        break;
      case SET_NOROWCOUNT:
        if (s_arg[i_arg].n_items != 1)
          SDDS_Bomb("invalid -noRowCount syntax");
        noRowCount = 1;
        break;
      case SET_ORDER:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -order syntax");
        switch (match_string(s_arg[i_arg].list[1], order_names, ORDERS, 0)) {
        case ROW_ORDER:
          columnOrder = 0;
          break;
        case COLUMN_ORDER:
          columnOrder = 1;
          break;
        default:
          SDDS_Bomb("invalid -order syntax");
          break;
        }
        break;
      case SET_PARAMETER:
        if ((s_arg[i_arg].n_items != 2) && (s_arg[i_arg].n_items != 4))
          SDDS_Bomb("invalid -parameter syntax");
        parameter = trealloc(parameter, sizeof(*parameter) * (++parameters));
        parameterFormat = trealloc(parameterFormat, sizeof(*parameterFormat) * (parameters));
        parameter[parameters - 1] = s_arg[i_arg].list[1];
        parameterFormat[parameters - 1] = NULL;
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0, "format", SDDS_STRING, &(parameterFormat[parameters - 1]), 1, 0, NULL))
          SDDS_Bomb("invalid -parameter syntax");
        if (parameterFormat[parameters - 1]) {
          replaceString(formatbuffer, parameterFormat[parameters - 1], "ld", PRId32, -1, 0);
          replaceString(formatbuffer2, formatbuffer, "lu", PRIu32, -1, 0);
          parameterFormat[parameters - 1] = malloc(sizeof(*(parameterFormat[parameters - 1])) * (strlen(formatbuffer2) + 1));
          sprintf(parameterFormat[parameters - 1], "%s", formatbuffer2);
        }
        break;
      case SET_COLUMN:
        if ((s_arg[i_arg].n_items < 2))
          SDDS_Bomb("invalid -column syntax");
        if (has_wildcards(s_arg[i_arg].list[1])) {
          columnMatch = trealloc(columnMatch, sizeof(*columnMatch) * (++columnMatches));
          columnMatchFormat = trealloc(columnMatchFormat, sizeof(*columnMatchFormat) * (columnMatches));
          SDDS_CopyString(&columnMatch[columnMatches - 1], s_arg[i_arg].list[1]);
          columnMatchFormat[columnMatches - 1] = NULL;
          s_arg[i_arg].n_items -= 2;
          if (!scanItemList(&flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0, "format", SDDS_STRING, &(columnMatchFormat[columnMatches - 1]), 1, 0, NULL))
            SDDS_Bomb("invalid -columns syntax");
          if (columnMatchFormat[columnMatches - 1]) {
            replaceString(formatbuffer, columnMatchFormat[columnMatches - 1], "ld", PRId32, -1, 0);
            replaceString(formatbuffer2, formatbuffer, "lu", PRIu32, -1, 0);
            columnMatchFormat[columnMatches - 1] = malloc(sizeof(*(columnMatchFormat[columnMatches - 1])) * (strlen(formatbuffer2) + 1));
            sprintf(columnMatchFormat[columnMatches - 1], "%s", formatbuffer2);
          }
        } else {
          column = trealloc(column, sizeof(*column) * (++columns));
          columnFormat = trealloc(columnFormat, sizeof(*columnFormat) * (columns));
          SDDS_CopyString(&column[columns - 1], s_arg[i_arg].list[1]);
          columnFormat[columns - 1] = NULL;
          s_arg[i_arg].n_items -= 2;
          if (!scanItemList(&flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0, "format", SDDS_STRING, &(columnFormat[columns - 1]), 1, 0, NULL))
            SDDS_Bomb("invalid -columns syntax");
          if (columnFormat[columns - 1]) {
            replaceString(formatbuffer, columnFormat[columns - 1], "ld", PRId32, -1, 0);
            replaceString(formatbuffer2, formatbuffer, "lu", PRIu32, -1, 0);
            columnFormat[columns - 1] = malloc(sizeof(*(columnFormat[columns - 1])) * (strlen(formatbuffer2) + 1));
            sprintf(columnFormat[columns - 1], "%s", formatbuffer2);
          }
        }
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_NOWARNINGS:
        if (s_arg[i_arg].n_items != 1)
          SDDS_Bomb("invalid -nowarnings syntax");
        noWarnings = 1;
        break;
      case SET_LABELED:
        labeled = 1;
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (input == NULL) {
        input = s_arg[i_arg].list[0];
      } else if (output == NULL) {
        output = s_arg[i_arg].list[0];
      } else {
        fprintf(stderr, "error: too many filenames provided.\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  processFilenames("sdds2plaindata", &input, &output, pipeFlags, noWarnings, &tmpfile_used);

  if (!columns && !columnMatches && !parameters)
    SDDS_Bomb("error: you must specify at least one of the -column or -parameter options.");
  if (!separator) {
    cp_str(&separator, "");
  }

  if (!SDDS_InitializeInput(&SDDS_dataset, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (columnMatches) {
    if (!(columnName = SDDS_GetColumnNames(&SDDS_dataset, &columnNames)))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    for (i = 0; i < columnMatches; i++) {
      ptr = expand_ranges(columnMatch[i]);
      for (j = 0; j < columnNames; j++) {
        if (wild_match(columnName[j], ptr)) {
          if (columns > 0) {
            if (match_string(columnName[j], column, columns, EXACT_MATCH) < 0) {
              column = trealloc(column, sizeof(*column) * (columns + 1));
              SDDS_CopyString(&column[columns], columnName[j]);
              columnFormat = trealloc(columnFormat, sizeof(*columnFormat) * (columns + 1));
              columnFormat[columns] = NULL;
              if (columnMatchFormat[i])
                SDDS_CopyString(&columnFormat[columns], columnMatchFormat[i]);
              columns++;
            }
          } else {
            column = trealloc(column, sizeof(*column) * (columns + 1));
            SDDS_CopyString(&column[columns], columnName[j]);
            columnFormat = trealloc(columnFormat, sizeof(*columnFormat) * (columns + 1));
            columnFormat[columns] = NULL;
            if (columnMatchFormat[i])
              SDDS_CopyString(&columnFormat[columns], columnMatchFormat[i]);
            columns++;
          }
        }
      }
    }
    SDDS_FreeStringArray(columnName, columnNames);
    free(columnName);
    SDDS_FreeStringArray(columnMatch, columnMatches);
    free(columnMatch);
    SDDS_FreeStringArray(columnMatchFormat, columnMatches);
    free(columnMatchFormat);
  }

  if (parameters) {
    parameterType = tmalloc(sizeof(*parameterType) * parameters);
    parameterIndex = tmalloc(sizeof(*parameterIndex) * parameters);
    parameterUnits = tmalloc(sizeof(*parameterUnits) * parameters);
    for (i = 0; i < parameters; i++) {
      if ((parameterIndex[i] = SDDS_GetParameterIndex(&SDDS_dataset, parameter[i])) < 0) {
        fprintf(stderr, "error: parameter '%s' does not exist.\n", parameter[i]);
        exit(EXIT_FAILURE);
      }
      if ((parameterType[i] = SDDS_GetParameterType(&SDDS_dataset, parameterIndex[i])) <= 0 ||
          SDDS_GetParameterInformation(&SDDS_dataset, "units", &parameterUnits[i], SDDS_GET_BY_INDEX, parameterIndex[i]) != SDDS_STRING) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }
  if (columns) {
    columnType = tmalloc(sizeof(*columnType) * columns);
    columnIndex = tmalloc(sizeof(*columnIndex) * columns);
    columnData = tmalloc(sizeof(*columnData) * columns);
    columnUnits = tmalloc(sizeof(*columnUnits) * columns);
    for (i = 0; i < columns; i++) {
      if ((columnIndex[i] = SDDS_GetColumnIndex(&SDDS_dataset, column[i])) < 0) {
        fprintf(stderr, "error: column '%s' does not exist.\n", column[i]);
        exit(EXIT_FAILURE);
      }
      if ((columnType[i] = SDDS_GetColumnType(&SDDS_dataset, columnIndex[i])) <= 0 ||
          SDDS_GetColumnInformation(&SDDS_dataset, "units", &columnUnits[i], SDDS_GET_BY_INDEX, columnIndex[i]) != SDDS_STRING) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }

  if (!output) {
    if (binary) {
#if defined(_WIN32)
      if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        fprintf(stderr, "error: unable to set stdout to binary mode.\n");
        exit(EXIT_FAILURE);
      }
#endif
    }
    fileID = stdout;
  } else {
    if (binary) {
      fileID = fopen(output, "wb");
    } else {
      fileID = fopen(output, "w");
    }
  }
  if (fileID == NULL) {
    fprintf(stderr, "error: unable to open output file for writing.\n");
    exit(EXIT_FAILURE);
  }

  if (binary) {
    layout = &SDDS_dataset.layout;
    fBuffer = &SDDS_dummy.fBuffer;
    fBuffer->buffer = NULL;
    if (!fBuffer->buffer) {
      if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * SDDS_FILEBUFFER_SIZE))) {
        fprintf(stderr, "error: unable to allocate buffer for binary output.\n");
        exit(EXIT_FAILURE);
      }
      fBuffer->bufferSize = SDDS_FILEBUFFER_SIZE;
      fBuffer->bytesLeft = SDDS_FILEBUFFER_SIZE;
    }
  }

  retval = -1;
  while (retval != page_number && (retval = SDDS_ReadPage(&SDDS_dataset)) > 0) {
    if (page_number && retval != page_number)
      continue;
    if (columns != 0) {
      if ((rows = SDDS_CountRowsOfInterest(&SDDS_dataset)) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
    if ((binary) && (!noRowCount)) {
      if (rows > INT32_MAX) {
        rows32 = INT32_MIN;
        if (!SDDS_BufferedWrite(&rows32, sizeof(rows32), fileID, fBuffer)) {
          fprintf(stderr, "error: failed to write row count (overflow).\n");
          exit(EXIT_FAILURE);
        }
        if (!SDDS_BufferedWrite(&rows, sizeof(rows), fileID, fBuffer)) {
          fprintf(stderr, "error: failed to write row count.\n");
          exit(EXIT_FAILURE);
        }
      } else {
        rows32 = rows;
        if (!SDDS_BufferedWrite(&rows32, sizeof(rows32), fileID, fBuffer)) {
          fprintf(stderr, "error: failed to write row count.\n");
          exit(EXIT_FAILURE);
        }
      }
    }

    for (i = 0; i < parameters; i++) {
      if (binary) {
        /*
          if (layout->parameter_definition[i].fixed_value)
          continue;
        */
        if (parameterType[i] == SDDS_STRING) {
          if (!SDDS_WriteBinaryString(*((char **)SDDS_dataset.parameter[parameterIndex[i]]), fileID, fBuffer)) {
            fprintf(stderr, "error: failed to write string parameter.\n");
            exit(EXIT_FAILURE);
          }
        } else if (!SDDS_BufferedWrite(SDDS_dataset.parameter[parameterIndex[i]], SDDS_type_size[layout->parameter_definition[parameterIndex[i]].type - 1], fileID, fBuffer)) {
          fprintf(stderr, "error: failed to write parameter value.\n");
          exit(EXIT_FAILURE);
        }
      } else {
        if (!SDDS_GetParameter(&SDDS_dataset, parameter[i], buffer)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
        SDDS_SprintTypedValue(buffer, 0, parameterType[i], parameterFormat[i], printBuffer, 0);
        if (labeled) {
          fputs(parameter[i], fileID);
          fputs(separator, fileID);
          if (parameterUnits[i])
            fputs(parameterUnits[i], fileID);
          fputs(separator, fileID);
        }
        fputs(printBuffer, fileID);
        fputc('\n', fileID);
      }
    }

    if (!binary) {
      if (!noRowCount)
        fprintf(fileID, "\t%" PRId64 "\n", rows);
      if (labeled && columns) {
        for (j = 0; j < columns; j++) {
          fputs(column[j], fileID);
          if (j != (columns - 1))
            fputs(separator, fileID);
        }
        fputc('\n', fileID);
        for (j = 0; j < columns; j++) {
          if (columnUnits[j])
            fputs(columnUnits[j], fileID);
          if (j != (columns - 1))
            fputs(separator, fileID);
        }
        fputc('\n', fileID);
      }
    }

    if (columns) {
      if (rows) {
        if (binary) {
          if (columnOrder) {
            for (j = 0; j < columns; j++) {
              if (columnType[j] == SDDS_STRING) {
                for (i = 0; i < rows; i++) {
                  if (!SDDS_WriteBinaryString(*((char **)SDDS_dataset.data[columnIndex[j]] + i), fileID, fBuffer)) {
                    fprintf(stderr, "error: failed to write string data for column '%s'.\n", column[j]);
                    exit(EXIT_FAILURE);
                  }
                }
              } else {
                size = SDDS_type_size[columnType[j] - 1];
                for (i = 0; i < rows; i++) {
                  if (!SDDS_BufferedWrite((char *)SDDS_dataset.data[columnIndex[j]] + i * size, size, fileID, fBuffer)) {
                    fprintf(stderr, "error: failed to write data for column '%s'.\n", column[j]);
                    exit(EXIT_FAILURE);
                  }
                }
              }
            }
          } else {
            for (i = 0; i < rows; i++) {
              for (j = 0; j < columns; j++) {
                if (columnType[j] == SDDS_STRING) {
                  if (!SDDS_WriteBinaryString(*((char **)SDDS_dataset.data[columnIndex[j]] + i), fileID, fBuffer)) {
                    fprintf(stderr, "error: failed to write string data for column '%s'.\n", column[j]);
                    exit(EXIT_FAILURE);
                  }
                } else {
                  size = SDDS_type_size[columnType[j] - 1];
                  if (!SDDS_BufferedWrite((char *)SDDS_dataset.data[columnIndex[j]] + i * size, size, fileID, fBuffer)) {
                    fprintf(stderr, "error: failed to write data for column '%s'.\n", column[j]);
                    exit(EXIT_FAILURE);
                  }
                }
              }
            }
          }
        } else {
          for (i = 0; i < columns; i++) {
            if (!(columnData[i] = SDDS_GetInternalColumn(&SDDS_dataset, column[i]))) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          }
          if (columnOrder) {
            for (i = 0; i < columns; i++) {
              for (j = 0; j < rows; j++) {
                SDDS_SprintTypedValue(columnData[i], j, columnType[i], columnFormat[i], printBuffer, 0);
                fputs(printBuffer, fileID);
                if (j != rows - 1)
                  fprintf(fileID, "%s", separator);
              }
              fputc('\n', fileID);
            }
          } else {
            for (j = 0; j < rows; j++) {
              for (i = 0; i < columns; i++) {
                SDDS_SprintTypedValue(columnData[i], j, columnType[i], columnFormat[i], printBuffer, 0);
                fputs(printBuffer, fileID);
                if (i != columns - 1)
                  fprintf(fileID, "%s", separator);
              }
              fputc('\n', fileID);
            }
          }
        }
      }
    }

    if (retval == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  if (binary) {
    if (!SDDS_FlushBuffer(fileID, fBuffer)) {
      SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteBinaryPage)");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  } else {
    fflush(fileID);
  }
  fclose(fileID);

  if (columns) {
    SDDS_FreeStringArray(column, columns);
    free(column);
    SDDS_FreeStringArray(columnFormat, columns);
    free(columnFormat);
  }
  if (parameters) {
    SDDS_FreeStringArray(parameter, parameters);
    free(parameter);
    SDDS_FreeStringArray(parameterFormat, parameters);
    free(parameterFormat);
  }
  free_scanargs(&s_arg, argc);
  if (!SDDS_Terminate(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
