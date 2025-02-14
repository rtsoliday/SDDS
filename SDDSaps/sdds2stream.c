/**
 * @file sdds2stream.c
 * @brief Utility for streaming SDDS data values to stdout.
 *
 * @details
 * This program reads an SDDS dataset and outputs selected column, parameter, or array values
 * in a delimited format to stdout. It supports filtering by pages and rows, handling multiple
 * input files and multiple data pages, and providing detailed control over output formatting.
 *
 * @section Usage
 * ```
 * sdds2stream [<SDDSinput>...]
 *             [-pipe]
 *             [-columns=<column-name>[,...]]
 *             [-parameters=<parameter-name>[,...]]
 *             [-arrays=<array-name>[,...]]
 *             [-page=<pageNumber>] 
 *             [-delimiter=<delimiting-string>]
 *             [-filenames] 
 *             [-rows[=bare][,total][,scientific]]
 *             [-npages[=<bare>]] 
 *             [-noquotes]
 *             [-ignoreFormats] 
 *             [-description]
 * ```
 *
 * @section Options
 * | Option             | Description                                                                |
 * |--------------------|---------------------------------  -----------------------------------------|
 * | `-pipe`            | Read input from a pipe instead of a file.                                  |
 * | `-columns`         | Stream data values from specified columns.                                 |
 * | `-parameters`      | Stream data values from specified parameters.                              |
 * | `-arrays`          | Stream data values from specified arrays.                                  |
 * | `-page`            | Specify the page number to stream.                                         |
 * | `-delimiter`       | Customize the delimiter for the output text.                               |
 * | `-filenames`       | Include the filenames in the output.                                       |
 * | `-rows`            | Options for row output modes (bare, total, scientific).                    |
 * | `-npages`          | Print the number of pages.                                                 |
 * | `-noquotes`        | Exclude quotation marks around strings.                                    |
 * | `-ignoreFormats`   | Ignore predefined formats for data output.                                 |
 * | `-description`     | Print the dataset description from the SDDS file.                          |
 *
 * @subsection Incompatibilities
 *   - `-columns` is incompatible with:
 *     - `-parameters`
 *     - `-arrays`
 *   - `-parameters` is incompatible with:
 *     - `-columns`
 *     - `-arrays`
 *   - `-arrays` is incompatible with:
 *     - `-columns`
 *     - `-parameters`
 *
 * @subsection SR Specific Requirements
 *   - For `-rows`, the following sub-options are available:
 *     - `bare`
 *     - `total`
 *     - `scientific`
 *     - These sub-options are mutually exclusive.
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
 *  M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#define SET_COLUMNS 0
#define SET_PARAMETERS 1
/* the -table option is retained for backward compatibility */
#define SET_TABLE 2
#define SET_DELIMITER 3
#define SET_FILENAMES 4
#define SET_ROWS 5
#define SET_NOQUOTES 6
#define SET_PIPE 7
#define SET_PAGE 8
#define SET_ARRAYS 9
#define SET_IGNOREFORMATS 10
#define SET_DESCRIPTION 11
#define SET_SHOW_PAGES 12
#define N_OPTIONS 13

char *option[N_OPTIONS] =
  {
    "columns",
    "parameters",
    "table",
    "delimiter",
    "filenames",
    "rows",
    "noquotes",
    "pipe",
    "page",
    "arrays",
    "ignoreformats",
    "description",
    "npages",
  };

char *USAGE = "\n"
  "  sdds2stream [<SDDSinput>...]\n"
  "              [-pipe]\n"
  "              [-columns=<column-name>[,...]]\n"
  "              [-parameters=<parameter-name>[,...]]\n"
  "              [-arrays=<array-name>[,...]]\n"
  "              [-page=<pageNumber>] \n"
  "              [-delimiter=<delimiting-string>]\n"
  "              [-filenames] \n"
  "              [-rows[=bare][,total][,scientific]]\n"
  "              [-npages[=<bare>]] \n"
  "              [-noquotes]\n"
  "              [-ignoreFormats] \n"
  "              [-description]\n"
  "sdds2stream provides stream output to the standard output of data values from "
  "a group of columns or parameters.  Each line of the output contains a different "
  "row of the tabular data or a different parameter.  Values from different columns "
  "are separated by the delimiter string, which by default is a single space. "
  "If -page is not employed, all data pages are output sequentially. "
  "If multiple filenames are given, the files are processed sequentially in the "
  "order given.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

long SDDS_SprintTypedValue2(char *s, char *format, char *buffer, unsigned long mode);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_dataset;
  long i, k, i_arg, retval, description;
  int64_t j, rows;
  SCANNED_ARG *s_arg;
  char **input;
  char **column_name, **parameter_name, **array_name;
  char **parameterFormat, **columnFormat, **arrayFormat;
  long column_names, parameter_names, array_names, page_number, *type, inputs;
  char *delimiter;
  void **data;
  char *buffer;
  long filenames, print_rows, print_pages, noQuotes, pipe, ignoreFormats;
  static char printBuffer[SDDS_MAXLINE * 16];
  long n_rows_bare, n_rows_total, n_pages, n_pages_bare, n_rows_scinotation;
  int64_t n_rows;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  type = NULL;
  data = NULL;
  buffer = tmalloc(sizeof(char) * 16); /* large enough for any data type */
  delimiter = NULL;
  input = parameter_name = column_name = array_name = NULL;
  parameterFormat = columnFormat = arrayFormat = NULL;
  inputs = parameter_names = column_names = array_names = 0;
  page_number = filenames = ignoreFormats = 0;
  n_rows = n_rows_bare = n_rows_total = n_pages = n_pages_bare = 0;
  print_rows = print_pages = noQuotes = pipe = description = 0;
  n_rows_scinotation = 0;

  /* Parse command-line arguments */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      int optIndex = match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0);
      switch (optIndex) {
      case SET_COLUMNS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        if (column_names)
          SDDS_Bomb("invalid syntax: specify -columns once only");
        column_name = tmalloc(sizeof(*column_name) * (s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          column_name[i - 1] = s_arg[i_arg].list[i];
        column_names = s_arg[i_arg].n_items - 1;
        break;

      case SET_PARAMETERS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -parameters syntax");
        if (parameter_names)
          SDDS_Bomb("invalid syntax: specify -parameters once only");
        parameter_name = tmalloc(sizeof(*parameter_name) * (s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          parameter_name[i - 1] = s_arg[i_arg].list[i];
        parameter_names = s_arg[i_arg].n_items - 1;
        break;

      case SET_ARRAYS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -arrays syntax");
        if (array_names)
          SDDS_Bomb("invalid syntax: specify -arrays once only");
        array_name = tmalloc(sizeof(*array_name) * (s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          array_name[i - 1] = s_arg[i_arg].list[i];
        array_names = s_arg[i_arg].n_items - 1;
        break;

      case SET_TABLE:
      case SET_PAGE:
        if (s_arg[i_arg].n_items < 2 || s_arg[i_arg].n_items > 2)
          SDDS_Bomb("invalid -page syntax");
        if (page_number != 0)
          SDDS_Bomb("invalid syntax: specify -page once only");
        if (sscanf(s_arg[i_arg].list[1], "%ld", &page_number) != 1 || page_number <= 0)
          SDDS_Bomb("invalid -page syntax or value");
        break;

      case SET_DELIMITER:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -delimiter syntax");
        delimiter = s_arg[i_arg].list[1];
        break;

      case SET_FILENAMES:
        filenames = 1;
        break;

      case SET_ROWS:
        if (s_arg[i_arg].n_items > 4)
          SDDS_Bomb("invalid -rows syntax");
        else {
          char *rowsOutputMode[3] = {"bare", "total", "scientific"};
          for (i = 1; i < s_arg[i_arg].n_items; i++) {
            int rm = match_string(s_arg[i_arg].list[i],
                                  rowsOutputMode, 3, 0);
            if (rm == 0)
              n_rows_bare = 1;
            else if (rm == 1)
              n_rows_total = 1;
            else if (rm == 2)
              n_rows_scinotation = 1;
            else
              SDDS_Bomb("unknown output mode for -rows option");
          }
        }
        print_rows = 1;
        break;

      case SET_SHOW_PAGES:
        if (s_arg[i_arg].n_items > 2)
          SDDS_Bomb("invalid -pages syntax");
        else {
          char *pagesOutputMode[1] = {"bare"};
          for (i = 1; i < s_arg[i_arg].n_items; i++) {
            if (strcmp(s_arg[i_arg].list[i], "") != 0) {
              int pm = match_string(s_arg[i_arg].list[i],
                                    pagesOutputMode, 1, 0);
              if (pm == 0)
                n_pages_bare = 1;
              else
                SDDS_Bomb("unknown output mode for -npages option");
            } else
              SDDS_Bomb("unknown output mode for -npages option");
          }
        }
        print_pages = 1;
        break;

      case SET_NOQUOTES:
        noQuotes = 1;
        break;

      case SET_PIPE:
        pipe = 1;
        break;

      case SET_IGNOREFORMATS:
        ignoreFormats = 1;
        break;

      case SET_DESCRIPTION:
        description = 1;
        break;

      default:
        fprintf(stderr, "error: unknown switch: %s\n",
                s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      input = trealloc(input, sizeof(*input) * (inputs + 1));
      input[inputs++] = s_arg[i_arg].list[0];
    }
  }

  if (!inputs) {
    if (!pipe)
      SDDS_Bomb("too few filenames");
    inputs = 1;
    input = trealloc(input, sizeof(*input) * (inputs + 1));
    input[0] = NULL;
  }

  if (!column_names && !parameter_names && !array_names && !print_rows && !description && !print_pages)
    SDDS_Bomb("you must specify one of -columns, -parameters, "
              "-arrays, -rows or -description");

  if (!delimiter) {
    if (column_names || array_names)
      cp_str(&delimiter, " ");
    else
      cp_str(&delimiter, "\n");
  }

  SDDS_SetTerminateMode(TERMINATE_DONT_FREE_TABLE_STRINGS + TERMINATE_DONT_FREE_ARRAY_STRINGS);

  /* Process each input file */
  for (k = 0; k < inputs; k++) {
    if (!SDDS_InitializeInput(&SDDS_dataset, input[k])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    n_pages = 0;

    /* Setup columns, parameters, or arrays */
    if (column_names) {
      if (k == 0) {
        type = tmalloc(sizeof(*type) * column_names);
        data = tmalloc(sizeof(*data) * column_names);
        columnFormat = tmalloc(sizeof(*columnFormat) * column_names);
      }
      for (i = 0; i < column_names; i++) {
        j = SDDS_GetColumnIndex(&SDDS_dataset, column_name[i]);
        if (j < 0) {
          fprintf(stderr, "error: column %s does not exist\n",
                  column_name[i]);
          exit(1);
        }
        type[i] = SDDS_GetColumnType(&SDDS_dataset, j);
        if (type[i] <= 0 || !SDDS_GetColumnInformation(&SDDS_dataset,
                                                       "format_string",
                                                       columnFormat + i,
                                                       SDDS_GET_BY_INDEX, j)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
      }
    } else if (array_names) {
      if (k == 0) {
        type = tmalloc(sizeof(*type) * array_names);
        data = tmalloc(sizeof(*data) * array_names);
        arrayFormat = tmalloc(sizeof(*arrayFormat) * array_names);
      }

      for (i = 0; i < array_names; i++) {
        j = SDDS_GetArrayIndex(&SDDS_dataset, array_name[i]);
        if (j < 0) {
          fprintf(stderr, "error: array %s does not exist\n",
                  array_name[i]);
          exit(1);
        }
        type[i] = SDDS_GetArrayType(&SDDS_dataset, j);
        if (type[i] <= 0 || !SDDS_GetArrayInformation(&SDDS_dataset, "format_string",
                                                      arrayFormat + i,
                                                      SDDS_BY_INDEX, j)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
      }
    } else if (parameter_names) {
      if (k == 0) {
        type = tmalloc(sizeof(*type) * parameter_names);
        parameterFormat = tmalloc(sizeof(*parameterFormat) * parameter_names);
      }
      for (i = 0; i < parameter_names; i++) {
        j = SDDS_GetParameterIndex(&SDDS_dataset, parameter_name[i]);
        if (j < 0) {
          fprintf(stderr, "error: parameter %s does not exist\n",
                  parameter_name[i]);
          exit(1);
        }
        type[i] = SDDS_GetParameterType(&SDDS_dataset, j);
        if (type[i] <= 0 || !SDDS_GetParameterInformation(&SDDS_dataset,
                                                          "format_string",
                                                          parameterFormat + i,
                                                          SDDS_BY_INDEX, j)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
      }
    }

    /* Print description if requested */
    if (description) {
      if (!SDDS_SprintTypedValue2(SDDS_dataset.layout.description, NULL,
                                  printBuffer,
                                  noQuotes ? SDDS_PRINT_NOQUOTES : 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }

      fputs(printBuffer, stdout);
      fprintf(stdout, "%s", delimiter);

      if (!SDDS_SprintTypedValue2(SDDS_dataset.layout.contents, NULL,
                                  printBuffer,
                                  noQuotes ? SDDS_PRINT_NOQUOTES : 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      fputs(printBuffer, stdout);
      fprintf(stdout, "%s", delimiter);

      if (!strchr(delimiter, '\n'))
        fputc('\n', stdout);
    }

    retval = -1;
    while (retval != page_number && (retval = SDDS_ReadPage(&SDDS_dataset)) > 0) {
      if (page_number && retval != page_number)
        continue;

      if (print_rows) {
        uint64_t counti = SDDS_CountRowsOfInterest(&SDDS_dataset);
        if (n_rows_total && !page_number) {
          n_rows += counti;
        } else {
          if (!n_rows_scinotation || counti == 0) {
            if (n_rows_bare)
              fprintf(stdout, "%" PRId64 "\n", (int64_t)counti);
            else
              fprintf(stdout, "%" PRId64 " rows\n", (int64_t)counti);
          } else {
            char format[20];
            double count = counti;
            snprintf(format, 20, "%%.%ldle%s",
                     (long)(log10(count)),
                     n_rows_bare ? "" : " rows");
            fprintf(stdout, format, count);
            fputc('\n', stdout);
          }
        }
      }

      if (column_names) {
        rows = SDDS_CountRowsOfInterest(&SDDS_dataset);
        if (rows < 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(1);
        }
        if (rows) {
          if (filenames) {
            fprintf(stdout, "%s%s", input[k], delimiter);
            if (!strchr(delimiter, '\n'))
              fputc('\n', stdout);
          }
          for (i = 0; i < column_names; i++) {
            data[i] = SDDS_GetInternalColumn(&SDDS_dataset,
                                             column_name[i]);
            if (!data[i]) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(1);
            }
          }
          for (j = 0; j < rows; j++) {
            for (i = 0; i < column_names; i++) {
              if (!SDDS_SprintTypedValue(data[i], j, type[i],
                                         ignoreFormats ? NULL : columnFormat[i],
                                         printBuffer,
                                         noQuotes ? SDDS_PRINT_NOQUOTES : 0)) {
                SDDS_PrintErrors(stderr,
                                 SDDS_VERBOSE_PrintErrors);
                exit(1);
              }
              fputs(printBuffer, stdout);
              if (i != column_names - 1)
                fprintf(stdout, "%s", delimiter);
            }
            fputc('\n', stdout);
          }
        }
      } else if (array_names) {
        SDDS_ARRAY *array;
        if (filenames) {
          fprintf(stdout, "%s%s", input[k], delimiter);
          if (!strchr(delimiter, '\n'))
            fputc('\n', stdout);
        }
        for (i = 0; i < array_names; i++) {
          array = SDDS_GetArray(&SDDS_dataset, array_name[i], NULL);
          if (!array) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
          for (j = 0; j < array->elements; j++) {
            if (!SDDS_SprintTypedValue(array->data, j, type[i],
                                       ignoreFormats ? NULL : arrayFormat[i],
                                       printBuffer,
                                       noQuotes ? SDDS_PRINT_NOQUOTES : 0)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(1);
            }
            fputs(printBuffer, stdout);
            fprintf(stdout, "%s", delimiter);
          }
        }
        if (!strchr(delimiter, '\n'))
          fputc('\n', stdout);
      } else if (parameter_names) {
        if (filenames)
          fprintf(stdout, "%s%s", input[k], delimiter);
        for (i = 0; i < parameter_names; i++) {
          if (!SDDS_GetParameter(&SDDS_dataset, parameter_name[i],
                                 buffer)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
          if (!SDDS_SprintTypedValue(buffer, 0, type[i],
                                     ignoreFormats ? NULL : parameterFormat[i],
                                     printBuffer,
                                     noQuotes ? SDDS_PRINT_NOQUOTES : 0)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
          fputs(printBuffer, stdout);
          fprintf(stdout, "%s", delimiter);
        }
        if (!strchr(delimiter, '\n'))
          fputc('\n', stdout);
      }

      n_pages++;
    }

    if (retval == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    } else {
      if (print_rows && (n_rows_total || (retval == -1 && n_pages == 0)) && !page_number) {
        if (!n_rows_scinotation || n_rows == 0) {
          if (n_rows_bare)
            fprintf(stdout, "%" PRId64 "\n", n_rows);
          else
            fprintf(stdout, "%" PRId64 " rows\n", n_rows);
        } else {
          char format[20];
          double count = n_rows;
          snprintf(format, 20, "%%.%ldle%s",
                   (long)(log10(count)),
                   n_rows_bare ? "" : " rows");
          fprintf(stdout, format, count);
          fputc('\n', stdout);
        }
      }
      if (print_pages) {
        if (n_pages_bare)
          fprintf(stdout, "%ld\n", n_pages);
        else
          fprintf(stdout, "%ld pages\n", n_pages);
      }
    }
    if (!SDDS_Terminate(&SDDS_dataset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }
  return 0;
}

long SDDS_SprintTypedValue2(char *s, char *format, char *buffer, unsigned long mode) {
  char buffer2[SDDS_PRINT_BUFLEN];
  short printed;

  if (!s)
    s = "";

  if (!buffer) {
    SDDS_SetError("Unable to print value--buffer pointer is NULL "
                  "(SDDS_SprintTypedValue2)");
    return 0;
  }
  if ((long)strlen(s) > SDDS_PRINT_BUFLEN - 3) {
    SDDS_SetError("Buffer size overflow (SDDS_SprintTypedValue2)");
    return 0;
  }

  if (!(mode & SDDS_PRINT_NOQUOTES)) {
    printed = 0;
    if (!s || SDDS_StringIsBlank(s))
      sprintf(buffer, "\"\"");
    else if (strchr(s, '"')) {
      strcpy(buffer2, s);
      SDDS_EscapeQuotes(buffer2, '"');
      if (SDDS_HasWhitespace(buffer2))
        sprintf(buffer, "\"%s\"", buffer2);
      else
        strcpy(buffer, buffer2);
    } else if (SDDS_HasWhitespace(s))
      sprintf(buffer, "\"%s\"", s);
    else {
      sprintf(buffer, format ? format : "%s", s);
      printed = 1;
    }
    if (!printed) {
      sprintf(buffer2, format ? format : "%s", buffer);
      strcpy(buffer, buffer2);
    }
  } else {
    sprintf(buffer, format ? format : "%s", s);
  }
  return 1;
}
