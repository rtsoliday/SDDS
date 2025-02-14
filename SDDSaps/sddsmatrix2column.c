/**
 * @file sddsmatrix2column.c
 * @brief Converts a matrix into a single-column SDDS file format.
 *
 * @details
 * This program reads an SDDS file containing an optional string column and multiple numerical columns
 * and outputs an SDDS file with two columns: a string column and a data column. The string column is a 
 * combination of the input string column (or generated row names) and the input data column names. It 
 * supports various options for row names, data column names, and matrix traversal order, making it flexible 
 * for diverse data transformation needs.
 *
 * @section Usage
 * ```
 * sddsmatrix2column [<inputfile>] [<outputfile>]
 *                   [-pipe=[input][,output]]
 *                   [-rowNameColumn=<string>] 
 *                   [-dataColumnName=<string>]
 *                   [-rootnameColumnName=<string>]
 *                   [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Specifies pipe flags for input and/or output.                                         |
 * | `-rowNameColumn`                      | Specifies the column name for row names in the input file.                           |
 * | `-dataColumnName`                     | Specifies the column name for the data column in the output file.                   |
 * | `-rootnameColumnName`                 | Specifies the column name for the string column in the output file.                 |
 * | `-majorOrder`                         | Determines the matrix traversal order (row-major or column-major).                    |
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
 * H. Shang, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_ROW_COLUMN_NAME,
  SET_DATA_COLUMN_NAME,
  SET_ROOTNAME_COLUMN_NAME,
  SET_PIPE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] =
  {
    "rowNameColumn",
    "dataColumnName",
    "rootnameColumnName",
    "pipe",
    "majorOrder"
  };

char *USAGE =
  "Usage: sddsmatrix2column [<inputfile>] [<outputfile>]\n"
  "                         [-pipe=<input>,<output>]\n"
  "                         [-rowNameColumn=<string>]\n"
  "                         [-dataColumnName=<string>]\n"
  "                         [-rootnameColumnName=<string>]\n"
  "                         [-majorOrder=row|column]\n\n"
  "Options:\n"
  "  <inputfile>\n"
  "    - Contains an optional string column and multiple numerical columns.\n"
  "    - If the string column or -rowNameColumn is not provided,\n"
  "      rows will be named as Row<row_index> in the output.\n\n"
  "  <outputfile>\n"
  "    - Contains two columns: a string column and a data column.\n"
  "    - The string column combines the input string column (or Row<row_index>)\n"
  "      with the input data column names.\n\n"
  "  -pipe=<input>,<output>\n"
  "    - Specifies pipe flags for input and/or output.\n\n"
  "  -rowNameColumn=<string>\n"
  "    - Specifies the column name for row names in the input file.\n"
  "    - If not provided, rows will be named as Row<row_index>.\n\n"
  "  -dataColumnName=<string>\n"
  "    - Specifies the column name for data in the output file.\n"
  "    - If not provided, \"Rootname\" will be used.\n\n"
  "  -rootnameColumnName=<string>\n"
  "    - Specifies the column name for the string column in the output file.\n\n"
  "  -majorOrder=row|column\n"
  "    - Determines the order to transfer the matrix into one column.\n"
  "      Choose 'row' for row-major order or 'column' for column-major order.\n\n"
  "Description:\n"
  "  sddsmatrix2column converts a matrix into a single column format.\n\n"
  "Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n\n";

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_dataset, SDDS_orig;
  char tmpName[1024];
  long i_arg, tmpfile_used = 0, j, column_major;
  SCANNED_ARG *s_arg;
  int32_t page, numCols, columns, columnType;
  int64_t i, rows, outputRows, outputRow;
  char *inputfile, *outputfile;
  char *rowColName, *dataColName, *rootnameColName, **rowName, **columnName;
  double data;
  unsigned long pipeFlags, majorOrderFlag;

  inputfile = outputfile = rowColName = dataColName = rootnameColName = NULL;
  pipeFlags = 0;
  rowName = columnName = NULL;
  majorOrderFlag = 0;
  column_major = 1; /* Default as column major */

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s\n", USAGE);
    return 1;
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          column_major = 1;
        if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          column_major = 0;
        break;
      case SET_ROW_COLUMN_NAME:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error (%s): invalid -rowNameColumn syntax\n", argv[0]);
          return 1;
        }
        rowColName = s_arg[i_arg].list[1];
        break;
      case SET_DATA_COLUMN_NAME:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error (%s): invalid -dataColumnName syntax\n", argv[0]);
          return 1;
        }
        dataColName = s_arg[i_arg].list[1];
        break;
      case SET_ROOTNAME_COLUMN_NAME:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error (%s): invalid -rootnameColumnName syntax\n", argv[0]);
          return 1;
        }
        rootnameColName = s_arg[i_arg].list[1];
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "Error (%s): invalid -pipe syntax\n", argv[0]);
          return 1;
        }
        break;
      default:
        fprintf(stderr, "Error: unknown option -- %s provided.\n", s_arg[i_arg].list[0]);
        return 1;
      }
    } else {
      if (inputfile == NULL)
        inputfile = s_arg[i_arg].list[0];
      else if (outputfile == NULL)
        outputfile = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "Error (%s): too many filenames\n", argv[0]);
        return 1;
      }
    }
  }

  if ((!pipeFlags && !outputfile)) {
    fprintf(stderr, "Error: output file not provided.\n");
    return 1;
  }
  processFilenames("sddsmatrix2column", &inputfile, &outputfile, pipeFlags, 0, &tmpfile_used);

  numCols = 0;
  if (!SDDS_InitializeInput(&SDDS_orig, inputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if (!SDDS_InitializeOutput(&SDDS_dataset, SDDS_orig.layout.data_mode.mode, 1, NULL, NULL, outputfile) ||
      !SDDS_DefineSimpleColumn(&SDDS_dataset, rootnameColName ? rootnameColName : "Rootname", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleColumn(&SDDS_dataset, dataColName ? dataColName : "Data", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(&SDDS_dataset, "InputFile", NULL, SDDS_STRING) ||
      !SDDS_WriteLayout(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if (!(columnName = (char **)SDDS_GetColumnNames(&SDDS_orig, &columns))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  numCols = 0;
  for (i = 0; i < columns; i++) {
    if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&SDDS_orig, i))) {
      numCols++;
    }
  }

  while ((page = SDDS_ReadPage(&SDDS_orig)) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&SDDS_orig)) < 0) {
      fprintf(stderr, "Error: problem counting rows in input page\n");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 1;
    }
    outputRows = rows * numCols;
    if (!SDDS_StartPage(&SDDS_dataset, outputRows)) {
      fprintf(stderr, "Error: problem starting output page\n");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 1;
    }
    if (rows > 0) {
      if (rowColName) {
        if (SDDS_CheckColumn(&SDDS_orig, rowColName, NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
          rowName = (char **)SDDS_GetColumn(&SDDS_orig, rowColName);
        } else {
          fprintf(stderr, "Error %s column does not exist or not string type in input file %s\n", rowColName, inputfile);
          return 1;
        }
      }
      outputRow = 0;
      if (!column_major) {
        for (i = 0; i < rows; i++) {
          for (j = 0; j < columns; j++) {
            if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&SDDS_orig, j))) {
              if (rowColName) {
                snprintf(tmpName, sizeof(tmpName), "%s%s", rowName[i], SDDS_orig.layout.column_definition[j].name);
              } else {
                snprintf(tmpName, sizeof(tmpName), "Row%" PRId64 "%s", i, SDDS_orig.layout.column_definition[j].name);
              }
              switch (columnType) {
              case SDDS_LONGDOUBLE:
                data = ((long double *)SDDS_orig.data[j])[i];
                break;
              case SDDS_DOUBLE:
                data = ((double *)SDDS_orig.data[j])[i];
                break;
              case SDDS_FLOAT:
                data = ((float *)SDDS_orig.data[j])[i];
                break;
              case SDDS_LONG64:
                data = ((int64_t *)SDDS_orig.data[j])[i];
                break;
              case SDDS_ULONG64:
                data = ((uint64_t *)SDDS_orig.data[j])[i];
                break;
              case SDDS_LONG:
                data = ((int32_t *)SDDS_orig.data[j])[i];
                break;
              case SDDS_ULONG:
                data = ((uint32_t *)SDDS_orig.data[j])[i];
                break;
              case SDDS_SHORT:
                data = ((short *)SDDS_orig.data[j])[i];
                break;
              case SDDS_USHORT:
                data = ((unsigned short *)SDDS_orig.data[j])[i];
                break;
              default:
                data = 0.0; /* Fallback for unsupported types */
              }

              if (!SDDS_SetRowValues(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, 0, tmpName, 1, data, -1)) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                return 1;
              }
              outputRow++;
            }
          }
        }
      } else {
        for (j = 0; j < columns; j++) {
          if (!SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&SDDS_orig, j)))
            continue;
          for (i = 0; i < rows; i++) {
            if (rowColName) {
              snprintf(tmpName, sizeof(tmpName), "%s%s", SDDS_orig.layout.column_definition[j].name, rowName[i]);
            } else {
              snprintf(tmpName, sizeof(tmpName), "%sRow%" PRId64, SDDS_orig.layout.column_definition[j].name, i);
            }
            switch (columnType) {
            case SDDS_LONGDOUBLE:
              data = ((long double *)SDDS_orig.data[j])[i];
              break;
            case SDDS_DOUBLE:
              data = ((double *)SDDS_orig.data[j])[i];
              break;
            case SDDS_FLOAT:
              data = ((float *)SDDS_orig.data[j])[i];
              break;
            case SDDS_LONG64:
              data = ((int64_t *)SDDS_orig.data[j])[i];
              break;
            case SDDS_ULONG64:
              data = ((uint64_t *)SDDS_orig.data[j])[i];
              break;
            case SDDS_LONG:
              data = ((int32_t *)SDDS_orig.data[j])[i];
              break;
            case SDDS_ULONG:
              data = ((uint32_t *)SDDS_orig.data[j])[i];
              break;
            case SDDS_SHORT:
              data = ((short *)SDDS_orig.data[j])[i];
              break;
            case SDDS_USHORT:
              data = ((unsigned short *)SDDS_orig.data[j])[i];
              break;
            default:
              data = 0.0; /* Fallback for unsupported types */
            }

            if (!SDDS_SetRowValues(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, 0, tmpName, 1, data, -1)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return 1;
            }
            outputRow++;
          }
        }
      }
    }
    if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "InputFile", inputfile ? inputfile : "pipe", NULL) ||
        !SDDS_WritePage(&SDDS_dataset)) {
      fprintf(stderr, "Error: problem writing page to file %s\n", outputfile);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 1;
    }
    if (rowColName) {
      SDDS_FreeStringArray(rowName, rows);
      rowName = NULL;
    }
  }
  if (!SDDS_Terminate(&SDDS_orig) || !SDDS_Terminate(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile)) {
    return 1;
  }

  free_scanargs(&s_arg, argc);
  return 0;
}
