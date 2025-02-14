/**
 * @file sdds2spreadsheet.c
 * @brief Convert an SDDS file to a spreadsheet-readable format.
 *
 * @details
 * This program reads an SDDS (Self Describing Data Set) file and converts it into a format
 * suitable for spreadsheet applications like Excel. It supports customization of output format,
 * delimiter, column selection, and inclusion of additional metadata like units and parameters.
 * The output can be generated as a tab-delimited text file or an Excel file if XLS support is enabled.
 *
 * @section Usage
 * ```
 * sdds2spreadsheet [<SDDSfilename>] [<outputname>]
 *                  [-pipe[=in][,out]]
 *                  [-column=<listOfColumns>]
 *                  [-units]
 *                  [-noParameters]
 *                  [-delimiter=<delimiting-string>]
 *                  [-all]
 *                  [-verbose]
 *                  [-excel]
 *                  [-sheetName=<parameterName>]
 * ```
 *
 * @section Options
 * | Option                              | Description                                                                             |
 * |---------------------------------------|-----------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard SDDS toolkit pipe options.                                                |
 * | `-column`                             | Specify a comma-separated list of columns to include (default is all).                 |
 * | `-units`                              | Include a row of units below the column names.                                         |
 * | `-noParameters`                       | Suppress the output of parameter data.                                                 |
 * | `-delimiter`                          | Define a custom delimiter string (default is `\t`).                                    |
 * | `-all`                                | Include parameter, column, and array information.                                      |
 * | `-verbose`                            | Output detailed header information to the terminal.                                    |
 * | `-excel`                              | Write output in XLS Excel format.                                                      |
 * | `-sheetName`                          | Use the specified parameter to name each Excel sheet.                                  |
 *
 * @subsection Incompatibilities
 *   - `-excel` is incompatible with:
 *     - `-pipe=out`
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
 * Kenneth Evans, C. Saunders, M. Borland, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#ifdef USE_XLS
#  include "common/xlconfig.h"
#endif
#ifdef __GNUC__
#  include <stdbool.h>
#else
typedef enum {
  false = 0,
  true = 1
} bool;
#endif
#ifdef USE_XLS
#  include "xlslib.h"
#  include "common/systype.h"
#endif

/* Enumeration for option types */
enum option_type {
  SET_DELIMITER,
  SET_ALL,
  SET_VERBOSE,
  SET_PIPE,
  SET_EXCEL,
  SET_COLUMNS,
  SET_UNITS,
  SET_SHEET_NAME_PARAMETER,
  SET_NO_PARAMETERS,
  N_OPTIONS
};

#define DELIMITER "\t"

char *option[N_OPTIONS] = {
  "delimiter",
  "all",
  "verbose",
  "pipe",
  "excel",
  "column",
  "units",
  "sheetname",
  "noparameters"};

char *USAGE =
  "\n"
  "  sdds2spreadsheet [<SDDSfilename>] [<outputname>]\n"
  "                   [-pipe[=in][,out]]\n"
  "                   [-column=<listOfColumns>]\n"
  "                   [-units]\n"
  "                   [-noParameters]\n"
  "                   [-delimiter=<delimiting-string>]\n"
  "                   [-all]\n"
  "                   [-verbose]\n"
  "                   [-excel]\n"
  "                   [-sheetName=<parameterName>]\n"
  "\nOptions:\n"
  "  -pipe            Use standard SDDS toolkit pipe option.\n"
  "  -excel           Write output in XLS Excel format.\n"
  "  -column          Specify a comma-separated list of columns to include (default is all).\n"
  "  -units           Include a row of units below the column names.\n"
  "  -noParameters    Suppress the output of parameter data.\n"
  "  -sheetName       Use the specified parameter to name each Excel sheet.\n"
  "  -delimiter       Define a custom delimiter string (default is \"\\t\").\n"
  "  -all             Include parameter, column, and array information.\n"
  "  -verbose         Output detailed header information to the terminal.\n"
  "\nNotes:\n"
  "  - Excel 4.0 lines must be shorter than 255 characters.\n"
  "  - Wingz delimiter can only be \"\\t\"\n"
  "\nProgram by Kenneth Evans.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  FILE *outfile = NULL;
  SDDS_TABLE SDDS_table;
  SDDS_LAYOUT *layout;
  COLUMN_DEFINITION *coldef;
  PARAMETER_DEFINITION *pardef;
  ARRAY_DEFINITION *arraydef;
  char **columnRequestList, **columnList;
  long nColumnsRequested, nColumns;
  char *input, *output;
  long i, i_arg, ntable;
  int64_t j, nrows;
  SCANNED_ARG *s_arg;
  char *text, *contents, *delimiter, *sheetNameParameter;
  long verbose = 0, all = 0, nvariableparms = 0, excel = 0, line = 0, units = 0, includeParameters = 1;
  void *data;
  unsigned long pipeFlags;
#ifdef USE_XLS
  workbook *w = NULL;
  worksheet *ws = NULL;
#endif
  char sheet[256];
  char buffer[5];

  SDDS_RegisterProgramName(argv[0]);

  input = output = NULL;
  delimiter = DELIMITER;
  pipeFlags = 0;
  columnRequestList = columnList = NULL;
  nColumnsRequested = nColumns = 0;
  sheetNameParameter = NULL;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_DELIMITER:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -delimiter syntax");
        delimiter = s_arg[i_arg].list[1];
        break;

      case SET_SHEET_NAME_PARAMETER:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -sheetName syntax");
        sheetNameParameter = s_arg[i_arg].list[1];
        break;

      case SET_NO_PARAMETERS:
        includeParameters = 0;
        break;

      case SET_ALL:
        all = 1;
        break;

      case SET_UNITS:
        units = 1;
        break;

      case SET_COLUMNS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -columns syntax");
        columnRequestList = s_arg[i_arg].list + 1;
        nColumnsRequested = s_arg[i_arg].n_items - 1;
        break;

      case SET_EXCEL:
#ifdef USE_XLS
        excel = 1;
#else
        SDDS_Bomb("-excel option is not available because sdds2spreadsheet was not compiled with xlslib support");
#endif
        break;

      case SET_VERBOSE:
        verbose = 1;
        break;

      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;

      default:
        fprintf(stderr, "Unknown option: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many filenames provided.");
    }
  }

  processFilenames("sdds2spreadsheet", &input, &output, pipeFlags, 0, NULL);

  if (output) {
    if (!excel) {
      outfile = fopen(output, "w");
      if (!outfile) {
        fprintf(stderr, "Cannot open output file %s\n", output);
        exit(EXIT_FAILURE);
      }
    }
  } else {
    if (excel) {
      SDDS_Bomb("-pipe=out and -excel options cannot be used together");
    }
    outfile = stdout;
  }

  if (input && !excel)
    fprintf(outfile, "Created from SDDS file: %s\n", input);

  if (!SDDS_InitializeInput(&SDDS_table, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  layout = &SDDS_table.layout;

  /* Description */
  if (verbose && input)
    fprintf(stderr, "\nFile %s is in SDDS protocol version %" PRId32 "\n", input, layout->version);

  if (!SDDS_GetDescription(&SDDS_table, &text, &contents))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (text) {
    if (verbose)
      fprintf(stderr, "Description: %s\n", text);
  }

  if (!excel)
    fprintf(outfile, "%s%s\n", text ? text : "No description", delimiter);

  if (contents) {
    if (verbose)
      fprintf(stderr, "Contents: %s\n", contents);
  }

  if (!excel)
    fprintf(outfile, "%s%s\n", contents ? contents : "No description", delimiter);

  if (layout->data_mode.mode == SDDS_ASCII) {
    if (verbose) {
      fprintf(stderr, "\nData is ASCII with %" PRId32 " lines per row and %" PRId32 " additional header lines expected.\n",
              layout->data_mode.lines_per_row, layout->data_mode.additional_header_lines);
      fprintf(stderr, "Row counts: %s\n", layout->data_mode.no_row_counts ? "No" : "Yes");
    }
  } else if (verbose) {
    fprintf(stderr, "\nData is binary\n");
  }

  /* Columns */
  if (layout->n_columns) {
    if (nColumnsRequested == 0) {
      nColumnsRequested = 1;
      columnRequestList = tmalloc(sizeof(*columnRequestList) * 1);
      columnRequestList[0] = tmalloc(sizeof(**columnRequestList) * 2);
      strcpy(columnRequestList[0], "*");
    }

    if (verbose) {
      fprintf(stderr, "\n%" PRId32 " columns of data:\n", layout->n_columns);
      fprintf(stderr, "NAME            UNITS           SYMBOL          FORMAT          TYPE    FIELD  DESCRIPTION\n");
      fprintf(stderr, "                                                                        LENGTH\n");
    }

    if (all && !excel)
      fprintf(outfile, "\nColumns%s\nName%sUnits%sSymbol%sFormat%sType%sField Length%sDescription%s\n",
              delimiter, delimiter, delimiter, delimiter, delimiter, delimiter, delimiter, delimiter);

    for (j = 0; j < nColumnsRequested; j++) {
      int32_t nc;
      char **columnName;

      SDDS_SetColumnFlags(&SDDS_table, 0);
      SDDS_SetColumnsOfInterest(&SDDS_table, SDDS_MATCH_STRING, columnRequestList[j], SDDS_OR);

      if ((columnName = SDDS_GetColumnNames(&SDDS_table, &nc)) && nc != 0) {
        columnList = SDDS_Realloc(columnList, sizeof(*columnList) * (nColumns + nc));
        for (i = 0; i < nc; i++) {
          columnList[i + nColumns] = columnName[i];
          coldef = SDDS_GetColumnDefinition(&SDDS_table, columnName[i]);

          if (verbose) {
            fprintf(stderr, "%-15s %-15s %-15s %-15s %-7s %-7" PRId32 " %s\n",
                    coldef->name,
                    coldef->units ? coldef->units : "",
                    coldef->symbol ? coldef->symbol : "",
                    coldef->format_string ? coldef->format_string : "",
                    SDDS_type_name[coldef->type - 1],
                    coldef->field_length,
                    coldef->description ? coldef->description : "");
          }

          if (all && !excel) {
            fprintf(outfile, "%s%s%s%s%s%s%s%s%s%s%-7" PRId32 "%s%s%s\n",
                    coldef->name, delimiter,
                    coldef->units ? coldef->units : "", delimiter,
                    coldef->symbol ? coldef->symbol : "", delimiter,
                    coldef->format_string ? coldef->format_string : "", delimiter,
                    SDDS_type_name[coldef->type - 1], delimiter,
                    coldef->field_length, delimiter,
                    coldef->description ? coldef->description : "", delimiter);
          }
        }
        nColumns += nc;
      }
    }
  }

  /* Parameters */
  if (layout->n_parameters && includeParameters) {
    if (verbose) {
      fprintf(stderr, "\n%" PRId32 " parameters:\n", layout->n_parameters);
      fprintf(stderr, "NAME                UNITS               SYMBOL              TYPE                DESCRIPTION\n");
    }

    if (all && !excel)
      fprintf(outfile, "\nParameters%s\nName%sFixedValue%sUnits%sSymbol%sType%sDescription%s\n",
              delimiter, delimiter, delimiter, delimiter, delimiter, delimiter, delimiter);

    for (i = 0; i < layout->n_parameters; i++) {
      pardef = layout->parameter_definition + i;

      if (!pardef->fixed_value) {
        nvariableparms++;
        if (!all)
          continue;
      }

      if (verbose) {
        fprintf(stderr, "%-19s %-19s %-19s %-19s %s\n",
                pardef->name,
                pardef->units ? pardef->units : "",
                pardef->symbol ? pardef->symbol : "",
                SDDS_type_name[pardef->type - 1],
                pardef->description ? pardef->description : "");
      }

      if (!excel) {
        if (all) {
          fprintf(outfile, "%s%s%s%s%s%s%s%s%s%s%s%s\n",
                  pardef->name, delimiter,
                  pardef->fixed_value ? pardef->fixed_value : "", delimiter,
                  pardef->units ? pardef->units : "", delimiter,
                  pardef->symbol ? pardef->symbol : "", delimiter,
                  SDDS_type_name[pardef->type - 1], delimiter,
                  pardef->description ? pardef->description : "", delimiter);
        } else {
          fprintf(outfile, "%s%s%s%s%s\n",
                  pardef->name, delimiter, delimiter,
                  pardef->fixed_value ? pardef->fixed_value : "", delimiter);
        }
      }
    }
  }

  /* Arrays */
  if (layout->n_arrays && all) {
    if (verbose) {
      fprintf(stderr, "\n%" PRId32 " arrays of data:\n", layout->n_arrays);
      fprintf(stderr, "NAME            UNITS           SYMBOL          FORMAT  TYPE            FIELD   GROUP           DESCRIPTION\n");
      fprintf(stderr, "                                                                        LENGTH  NAME\n");
    }

    if (!excel) {
      fprintf(outfile, "\nArrays%s\nName%sUnits%sSymbol%sFormat%sType%sField Length%sGroup Name%sDescription%s\n",
              delimiter, delimiter, delimiter, delimiter, delimiter, delimiter, delimiter, delimiter, delimiter);
    }

    for (i = 0; i < layout->n_arrays; i++) {
      arraydef = layout->array_definition + i;

      if (verbose) {
        fprintf(stderr, "%-15s %-15s %-15s %-7s %-8s*^%-5" PRId32 " %-7" PRId32 " %-15s %s\n",
                arraydef->name,
                arraydef->units,
                arraydef->symbol,
                arraydef->format_string,
                SDDS_type_name[arraydef->type - 1],
                arraydef->dimensions,
                arraydef->field_length,
                arraydef->group_name,
                arraydef->description);
      }

      if (!excel) {
        fprintf(outfile, "%s%s%s%s%s%s%s%s%s*^%-5" PRId32 "%s%-7" PRId32 "%s%s%s%s%s\n",
                arraydef->name, delimiter,
                arraydef->units, delimiter,
                arraydef->symbol, delimiter,
                arraydef->format_string, delimiter,
                SDDS_type_name[arraydef->type - 1],
                arraydef->dimensions, delimiter,
                arraydef->field_length, delimiter,
                arraydef->group_name, delimiter,
                arraydef->description, delimiter);
      }
    }
  }

#ifdef USE_XLS
  if (excel) {
    w = xlsNewWorkbook();
  }
#  ifdef __APPLE__
  /* xlsWorkbookIconvInType(w, "UCS-4-INTERNAL"); */
#  endif
#endif

  /* Process tables */
  while ((ntable = SDDS_ReadTable(&SDDS_table)) > 0) {
    line = 0;
#ifdef USE_XLS
    if (excel) {
      if (!sheetNameParameter) {
        sprintf(sheet, "Sheet%ld", ntable);
        ws = xlsWorkbookSheet(w, sheet);
      } else {
        char *name;
        if (!(name = SDDS_GetParameterAsString(&SDDS_table, sheetNameParameter, NULL)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        ws = xlsWorkbookSheet(w, name);
        free(name);
      }
    } else {
      fprintf(outfile, "\nTable %ld\n", ntable);
    }
#else
    fprintf(outfile, "\nTable %ld\n", ntable);
#endif

    /* Variable parameters */
    if (nvariableparms && includeParameters) {
      for (i = 0; i < layout->n_parameters; i++) {
        pardef = layout->parameter_definition + i;

        if (pardef->fixed_value)
          continue;

        data = SDDS_GetParameter(&SDDS_table, pardef->name, NULL);
        if (!data) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }

#ifdef USE_XLS
        if (excel) {
          xlsWorksheetLabel(ws, line, 0, pardef->name, NULL);
          switch (pardef->type) {
          case SDDS_LONGDOUBLE:
            xlsWorksheetNumberDbl(ws, line, 1, *((long double *)data), NULL);
            break;
          case SDDS_DOUBLE:
            xlsWorksheetNumberDbl(ws, line, 1, *((double *)data), NULL);
            break;
          case SDDS_FLOAT:
            xlsWorksheetNumberDbl(ws, line, 1, *((float *)data), NULL);
            break;
          case SDDS_ULONG64:
            xlsWorksheetNumberInt(ws, line, 1, *((uint64_t *)data), NULL);
            break;
          case SDDS_LONG64:
            xlsWorksheetNumberInt(ws, line, 1, *((int64_t *)data), NULL);
            break;
          case SDDS_ULONG:
            xlsWorksheetNumberInt(ws, line, 1, *((uint32_t *)data), NULL);
            break;
          case SDDS_LONG:
            xlsWorksheetNumberInt(ws, line, 1, *((int32_t *)data), NULL);
            break;
          case SDDS_USHORT:
            xlsWorksheetNumberInt(ws, line, 1, *((unsigned short *)data), NULL);
            break;
          case SDDS_SHORT:
            xlsWorksheetNumberInt(ws, line, 1, *((short *)data), NULL);
            break;
          case SDDS_STRING:
            xlsWorksheetLabel(ws, line, 1, *((char **)data), NULL);
            break;
          case SDDS_CHARACTER:
            sprintf(buffer, "%c", *((char *)data));
            xlsWorksheetLabel(ws, line, 1, buffer, NULL);
            break;
          default:
            break;
          }
          line++;
        } else {
#endif
          fprintf(outfile, "%s%s%s", pardef->name, delimiter, delimiter);
          SDDS_PrintTypedValue(data, 0, pardef->type, NULL, outfile, 0);
          fprintf(outfile, "%s\n", delimiter);
#ifdef USE_XLS
        }
#endif
      }
      line++;
    }

    /* Columns */
    if (nColumns) {
      SDDS_SetRowFlags(&SDDS_table, 1);
      nrows = SDDS_CountRowsOfInterest(&SDDS_table);
      if (nrows < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      for (i = 0; i < nColumns; i++) {
        coldef = SDDS_GetColumnDefinition(&SDDS_table, columnList[i]);
#ifdef USE_XLS
        if (excel) {
          xlsWorksheetLabel(ws, line, i, coldef->name, NULL);
        } else {
          fprintf(outfile, "%s%s", coldef->name, delimiter);
        }
#else
        fprintf(outfile, "%s%s", coldef->name, delimiter);
#endif
      }
      line++;
      if (!excel)
        fprintf(outfile, "\n");

      if (units) {
        for (i = 0; i < nColumns; i++) {
          coldef = SDDS_GetColumnDefinition(&SDDS_table, columnList[i]);
#ifdef USE_XLS
          if (excel) {
            xlsWorksheetLabel(ws, line, i, coldef->units ? coldef->units : "", NULL);
          } else {
            fprintf(outfile, "%s%s", coldef->units ? coldef->units : "", delimiter);
          }
#else
          fprintf(outfile, "%s%s", coldef->units ? coldef->units : "", delimiter);
#endif
        }
        line++;
        if (!excel)
          fprintf(outfile, "\n");
      }

      if (nrows) {
        for (j = 0; j < nrows; j++) {
          for (i = 0; i < nColumns; i++) {
            coldef = SDDS_GetColumnDefinition(&SDDS_table, columnList[i]);
            data = SDDS_GetValue(&SDDS_table, coldef->name, j, NULL);
            if (!data) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }

#ifdef USE_XLS
            if (excel) {
              switch (coldef->type) {
              case SDDS_LONGDOUBLE:
                xlsWorksheetNumberDbl(ws, line, i, *((long double *)data), NULL);
                break;
              case SDDS_DOUBLE:
                xlsWorksheetNumberDbl(ws, line, i, *((double *)data), NULL);
                break;
              case SDDS_FLOAT:
                xlsWorksheetNumberDbl(ws, line, i, *((float *)data), NULL);
                break;
              case SDDS_ULONG64:
                xlsWorksheetNumberInt(ws, line, i, *((uint64_t *)data), NULL);
                break;
              case SDDS_LONG64:
                xlsWorksheetNumberInt(ws, line, i, *((int64_t *)data), NULL);
                break;
              case SDDS_ULONG:
                xlsWorksheetNumberInt(ws, line, i, *((uint32_t *)data), NULL);
                break;
              case SDDS_LONG:
                xlsWorksheetNumberInt(ws, line, i, *((int32_t *)data), NULL);
                break;
              case SDDS_USHORT:
                xlsWorksheetNumberInt(ws, line, i, *((unsigned short *)data), NULL);
                break;
              case SDDS_SHORT:
                xlsWorksheetNumberInt(ws, line, i, *((short *)data), NULL);
                break;
              case SDDS_STRING:
                xlsWorksheetLabel(ws, line, i, *((char **)data), NULL);
                break;
              case SDDS_CHARACTER:
                sprintf(buffer, "%c", *((char *)data));
                xlsWorksheetLabel(ws, line, i, buffer, NULL);
                break;
              default:
                break;
              }
            } else {
#endif
              switch (coldef->type) {
              case SDDS_DOUBLE:
                fprintf(outfile, "%.*g", DBL_DIG, *((double *)data));
                break;
              case SDDS_FLOAT:
                fprintf(outfile, "%.*g", FLT_DIG, *((float *)data));
                break;
              default:
                SDDS_PrintTypedValue(data, 0, coldef->type, NULL, outfile, 0);
                break;
              }
              fprintf(outfile, "%s", delimiter);
#ifdef USE_XLS
            }
#endif
          }
          if (!excel)
            fprintf(outfile, "\n");
          line++;
        }
      }
    }
  }

#ifdef USE_XLS
  if (excel) {
    xlsWorkbookDump(w, output);
    xlsDeleteWorkbook(w);
  }
#endif

  /* Terminate program */
  fflush(stdout);
  if (!SDDS_Terminate(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
