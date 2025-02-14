/**
 * @file sddsquery.c
 * @brief Reads an SDDS file header and summarizes it.
 *
 * @details
 * This program processes SDDS files and summarizes their headers. It provides
 * lists of arrays, columns, parameters, or associates based on user-specified options.
 *
 * @section Usage
 * ```
 * sddsquery [<SDDSfilename>] [<SDDSfilename>...]
 *           [-pipe[=input]]
 *           [-sddsOutput[=<filename>]]
 *           [-arraylist]
 *           [-associatelist]
 *           [-columnlist]
 *           [-parameterlist]
 *           [-version]
 *           [-delimiter=<delimiting-string>] 
 *           [-appendunits[=bare]] 
 *           [-readAll]
 * ```
 *
 * @section Options
 * | Optional        | Description                                                                           |
 * |-----------------|---------------------------------------------------------------------------------------|
 * | `-pipe`         | Read input from a pipe.                                                              |
 * | `-sddsOutput`   | Write SDDS output to a file.                                              |
 * | `-arraylist`    | List arrays.                                                                         |
 * | `-associatelist`| List associates.                                                                     |
 * | `-columnlist`   | List columns.                                                                        |
 * | `-parameterlist`| List parameters.                                                                     |
 * | `-version`      | Show version information.                                                            |
 * | `-delimiter`    | Use <string> as a delimiter.                                                   |
 * | `-appendunits`  | Append units to the output.                                                    |
 * | `-readAll`      | Read all pages.                                                                      |
 *
 * @subsection Incompatibilities
 *   - Only one of the following may be specified:
 *     - `-arraylist`
 *     - `-associatelist`
 *     - `-columnlist`
 *     - `-parameterlist`
 *     - `-version`
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
 * M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_COLUMN_LIST,
  SET_PARAMETER_LIST,
  SET_ASSOCIATE_LIST,
  SET_ARRAY_LIST,
  SET_DELIMITER,
  SET_APPEND_UNITS,
  SET_VERSION,
  SET_PIPE,
  SET_SDDSOUTPUT,
  SET_READALL,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "columnlist",
  "parameterlist",
  "associatelist",
  "arraylist",
  "delimiter",
  "appendunits",
  "version",
  "pipe",
  "sddsoutput",
  "readall",
};

char *USAGE =
  "sddsquery [<SDDSfilename>...]\n"
  "          [-pipe[=input]]\n"
  "          [-sddsOutput[=<filename>]]\n"
  "          [-arraylist]\n"
  "          [-associatelist]\n"
  "          [-columnlist]\n"
  "          [-parameterlist]\n"
  "          [-version]\n"
  "          [-delimiter=<delimiting-string>] \n"
  "          [-appendunits[=bare]] \n"
  "          [-readAll]\n"
  "Options:\n"
  "  -pipe[=input]                Read input from a pipe.\n"
  "  -sddsOutput[=<filename>]     Write SDDS output to a file.\n"
  "  -arraylist                   List arrays.\n"
  "  -associatelist               List associates.\n"
  "  -columnlist                  List columns.\n"
  "  -parameterlist               List parameters.\n"
  "  -version                     Show version information.\n"
  "  -delimiter=<string>          Use <string> as a delimiter.\n"
  "  -appendunits[=bare]          Append units to the output.\n"
  "  -readAll                     Read all pages.\n"
  "\n"
  "Description:\n"
  "  sddsquery accesses a series of SDDS files and summarizes the file header for each. "
  "It also provides lists of arrays, columns, parameters, or associates.\n"
  "\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

long InitializeSDDSHeaderOutput(SDDS_DATASET *outSet, char *filename);
long MakeSDDSHeaderSummary(SDDS_DATASET *outSet, SDDS_DATASET *inSet, long listRequest, char *inputFile);
long MakeColumnHeaderSummary(SDDS_DATASET *outSet, SDDS_DATASET *inSet, char *inputFile);
long MakeParameterHeaderSummary(SDDS_DATASET *outSet, SDDS_DATASET *inSet, char *inputFile);
long MakeArrayHeaderSummary(SDDS_DATASET *outSet, SDDS_DATASET *inSet, char *inputFile);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_dataset, SDDSout;
  SDDS_LAYOUT *layout;
  COLUMN_DEFINITION *coldef;
  PARAMETER_DEFINITION *pardef;
  ARRAY_DEFINITION *arraydef;
  long list_request;
  char s[SDDS_MAXLINE];
  char **filename, *ptr, *sddsOutputFile;
  long i, i_arg, filenames, file, append_units, sddsOutput;
  SCANNED_ARG *s_arg;
  char *text, *contents, *delimiter, *unitsTemplate;
  FILE *fp;
  unsigned long pipeFlags;
  long readAll;

  list_request = -1;
  filename = NULL;
  sddsOutput = 0;
  filenames = append_units = 0;
  delimiter = sddsOutputFile = unitsTemplate = NULL;
  pipeFlags = 0;
  readAll = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_COLUMN_LIST:
        list_request = SET_COLUMN_LIST;
        break;
      case SET_PARAMETER_LIST:
        list_request = SET_PARAMETER_LIST;
        break;
      case SET_ASSOCIATE_LIST:
        list_request = SET_ASSOCIATE_LIST;
        break;
      case SET_ARRAY_LIST:
        list_request = SET_ARRAY_LIST;
        break;
      case SET_VERSION:
        list_request = SET_VERSION;
        break;
      case SET_DELIMITER:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -delimiter syntax");
        delimiter = s_arg[i_arg].list[1];
        break;
      case SET_APPEND_UNITS:
        append_units = 1;
        if (s_arg[i_arg].n_items == 2) {
          if (strncmp(s_arg[i_arg].list[1], "bare", strlen(s_arg[i_arg].list[1])) == 0)
            unitsTemplate = " %s";
          else
            SDDS_Bomb("invalid -appendUnits syntax");
        } else if (s_arg[i_arg].n_items > 2)
          SDDS_Bomb("invalid -appendUnits syntax");
        else
          unitsTemplate = " (%s)";
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_SDDSOUTPUT:
        sddsOutput = 1;
        sddsOutputFile = NULL;
        if ((s_arg[i_arg].n_items != 1 && s_arg[i_arg].n_items != 2) ||
            (s_arg[i_arg].n_items == 2 && SDDS_StringIsBlank(sddsOutputFile = s_arg[i_arg].list[1])))
          SDDS_Bomb("invalid -sddsOutput syntax");
        break;
      case SET_READALL:
        readAll = 1;
        break;
      default:
        bomb("unknown switch", USAGE);
        break;
      }
    } else {
      filename = trealloc(filename, sizeof(*filename) * (filenames + 1));
      filename[filenames++] = s_arg[i_arg].list[0];
    }
  }

  if (!filenames && !(pipeFlags & USE_STDIN))
    bomb(NULL, USAGE);
  if (pipeFlags & USE_STDIN) {
    char **filename1;
    filename1 = tmalloc(sizeof(*filename1) * (filenames + 1));
    filename1[0] = NULL;
    for (file = 0; file < filenames; file++)
      filename1[file + 1] = filename[file];
    if (filename)
      free(filename);
    filename = filename1;
    filenames++;
  }
#ifdef DEBUG
  fprintf(stderr, "files: ");
  for (file = 0; file < filenames; file++)
    fprintf(stderr, "%s ", filename[file] ? filename[file] : "NULL");
  fprintf(stderr, "\n");
#endif

  if (sddsOutput && !InitializeSDDSHeaderOutput(&SDDSout, sddsOutputFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  for (file = 0; file < filenames; file++) {
#ifdef DEBUG
    fprintf(stderr, "working on file %ld\n", file);
#endif
    if (list_request != SET_VERSION && !SDDS_InitializeInput(&SDDS_dataset, filename[file])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
#ifdef DEBUG
    fprintf(stderr, "file initialized\n");
#endif
    if (sddsOutput) {
#ifdef DEBUG
      fprintf(stderr, "Making SDDS header summary\n");
#endif
      if (!MakeSDDSHeaderSummary(&SDDSout, &SDDS_dataset, list_request, filename[file])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
#ifdef SOLARIS
      if (!SDDS_Terminate(&SDDS_dataset)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
#endif
      continue;
    }
    layout = &SDDS_dataset.layout;
    if (list_request >= 0) {
      switch (list_request) {
      case SET_COLUMN_LIST:
#ifdef DEBUG
        fprintf(stderr, "printing column info\n");
#endif
        for (i = 0; i < layout->n_columns; i++) {
          fputs(layout->column_definition[i].name, stdout);
          if (append_units && layout->column_definition[i].units &&
              !SDDS_StringIsBlank(layout->column_definition[i].units))
            fprintf(stdout, unitsTemplate, layout->column_definition[i].units);
          fputs(delimiter ? delimiter : "\n", stdout);
        }
        break;
      case SET_PARAMETER_LIST:
#ifdef DEBUG
        fprintf(stderr, "printing parameter info\n");
#endif
        for (i = 0; i < layout->n_parameters; i++) {
          fputs(layout->parameter_definition[i].name, stdout);
          if (append_units && layout->parameter_definition[i].units &&
              !SDDS_StringIsBlank(layout->parameter_definition[i].units))
            fprintf(stdout, unitsTemplate, layout->parameter_definition[i].units);
          fputs(delimiter ? delimiter : "\n", stdout);
        }
        break;
      case SET_ASSOCIATE_LIST:
#ifdef DEBUG
        fprintf(stderr, "printing associate info\n");
#endif
        for (i = 0; i < layout->n_associates; i++) {
          fputs(layout->associate_definition[i].filename, stdout);
          fputs(delimiter ? delimiter : "\n", stdout);
        }
        break;
      case SET_ARRAY_LIST:
#ifdef DEBUG
        fprintf(stderr, "printing array info\n");
#endif
        for (i = 0; i < layout->n_arrays; i++) {
          fputs(layout->array_definition[i].name, stdout);
          if (append_units && layout->array_definition[i].units &&
              !SDDS_StringIsBlank(layout->array_definition[i].units))
            fprintf(stdout, unitsTemplate, layout->array_definition[i].units);
          fputs(delimiter ? delimiter : "\n", stdout);
        }
        break;
      case SET_VERSION:
#ifdef DEBUG
        fprintf(stderr, "printing version info\n");
#endif
        s[0] = 0;
        if (!(fp = fopen(filename[file], "r")) || !fgets(s, SDDS_MAXLINE, fp)) {
          puts("-1");
          exit(EXIT_FAILURE);
        }
        if (strncmp(s, "SDDS", 4) != 0)
          puts("0");
        else {
          if (!(ptr = strchr(s, '\n')))
            puts("0");
          else {
            *ptr = 0;
            puts(s + 4);
          }
        }
        fclose(fp);
        break;
      default:
        SDDS_Bomb("something impossible happened!");
        break;
      }
      if (readAll) {
        while (SDDS_ReadPageSparse(&SDDS_dataset, 0, 1000, 0, 0) > 0)
          ;
      }
#ifdef SOLARIS
      if (list_request != SET_VERSION && !SDDS_Terminate(&SDDS_dataset)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
#endif
      exit(EXIT_SUCCESS);
    }

    fprintf(stdout, "\nfile %s is in SDDS protocol version %" PRId32 "\n",
            filename[file] ? filename[file] : "stdin", layout->version);
    if (!SDDS_GetDescription(&SDDS_dataset, &text, &contents))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (text)
      fprintf(stdout, "description: %s\n", text);
    if (contents)
      fprintf(stdout, "contents: %s\n", contents);
    if (layout->data_mode.mode == SDDS_ASCII) {
      fprintf(stdout, "\ndata is ASCII with %" PRId32 " lines per row and %" PRId32 " additional header lines expected.\n",
              layout->data_mode.lines_per_row, layout->data_mode.additional_header_lines);
      fprintf(stdout, "row counts: %s\n", layout->data_mode.no_row_counts ? "no" : "yes");
    } else {
      if (!SDDS_dataset.layout.byteOrderDeclared)
        fprintf(stdout, "data is binary (no byte order declared)\n");
      else if (SDDS_IsBigEndianMachine()) {
        if (SDDS_dataset.swapByteOrder)
          fprintf(stdout, "data is little-endian binary\n");
        else
          fprintf(stdout, "data is big-endian binary\n");
      } else {
        if (SDDS_dataset.swapByteOrder)
          fprintf(stdout, "data is big-endian binary\n");
        else
          fprintf(stdout, "data is little-endian binary\n");
      }
    }

    if (layout->n_columns) {
      fprintf(stdout, "\n%" PRId32 " columns of data:\n", layout->n_columns);
      fprintf(stdout, "NAME            UNITS           SYMBOL          FORMAT          TYPE    FIELD  DESCRIPTION\n");
      fprintf(stdout, "                                                                        LENGTH\n");
      for (i = 0; i < layout->n_columns; i++) {
        coldef = layout->column_definition + i;
        fprintf(stdout, "%-15s %-15s %-15s %-15s %-7s %-7" PRId32 " %s\n",
                coldef->name ? coldef->name : "NULL",
                coldef->units ? coldef->units : "NULL",
                coldef->symbol ? coldef->symbol : "NULL",
                coldef->format_string ? coldef->format_string : "NULL",
                SDDS_type_name[coldef->type - 1] ? SDDS_type_name[coldef->type - 1] : "NULL",
                coldef->field_length, coldef->description ? coldef->description : "NULL");
      }
    }

    if (layout->n_parameters) {
      fprintf(stdout, "\n%" PRId32 " parameters:\n", layout->n_parameters);
      fprintf(stdout, "NAME                UNITS               SYMBOL              TYPE                DESCRIPTION\n");
      for (i = 0; i < layout->n_parameters; i++) {
        pardef = layout->parameter_definition + i;
        fprintf(stdout, "%-19s %-19s %-19s %-19s %s\n",
                pardef->name ? pardef->name : "NULL",
                pardef->units ? pardef->units : "NULL",
                pardef->symbol ? pardef->symbol : "NULL",
                SDDS_type_name[pardef->type - 1] ? SDDS_type_name[pardef->type - 1] : "NULL",
                pardef->description ? pardef->description : "NULL");
      }
    }

    if (layout->n_arrays) {
      fprintf(stdout, "\n%" PRId32 " arrays of data:\n", layout->n_arrays);
      fprintf(stdout, "NAME            UNITS           SYMBOL          FORMAT  TYPE            FIELD   GROUP           DESCRIPTION\n");
      fprintf(stdout, "                                                                        LENGTH  NAME\n");
      for (i = 0; i < layout->n_arrays; i++) {
        arraydef = layout->array_definition + i;
        fprintf(stdout, "%-15s %-15s %-15s %-7s %-8s*^%-5" PRId32 " %-7" PRId32 " %-15s %s\n",
                arraydef->name ? arraydef->name : "NULL",
                arraydef->units ? arraydef->units : "NULL",
                arraydef->symbol ? arraydef->symbol : "NULL",
                arraydef->format_string ? arraydef->format_string : "NULL",
                SDDS_type_name[arraydef->type - 1],
                arraydef->dimensions,
                arraydef->field_length,
                arraydef->group_name ? arraydef->group_name : "NULL",
                arraydef->description ? arraydef->description : "NULL");
      }
    }

    if (layout->n_associates) {
      fprintf(stdout, "\n%" PRId32 " associates:\n", layout->n_associates);
      fprintf(stdout, "SDDS  FILENAME            PATH                          CONTENTS            DESCRIPTION\n");
      for (i = 0; i < layout->n_associates; i++)
        fprintf(stdout, "%-5s %-19s %-29s %-19s %s\n",
                layout->associate_definition[i].sdds ? "yes" : "no",
                layout->associate_definition[i].filename ? layout->associate_definition[i].filename : "NULL",
                layout->associate_definition[i].path ? layout->associate_definition[i].path : "NULL",
                layout->associate_definition[i].contents ? layout->associate_definition[i].contents : "NULL",
                layout->associate_definition[i].description ? layout->associate_definition[i].description : "NULL");
    }
    fflush(stdout);
    if (readAll) {
      while (SDDS_ReadPageSparse(&SDDS_dataset, 0, 1000, 0, 0) > 0)
        ;
    }
    if (!SDDS_Terminate(&SDDS_dataset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
  if (sddsOutput && !SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  return EXIT_SUCCESS;
}

static long indexName, indexUnits, indexSymbol, indexFormat, indexType, indexDescription, indexGroup;

long InitializeSDDSHeaderOutput(SDDS_DATASET *outSet, char *filename) {
  if (!SDDS_InitializeOutput(outSet, SDDS_BINARY, 0, NULL, NULL, filename))
    return 0;
  if (0 > (indexName = SDDS_DefineColumn(outSet, "Name", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) ||
      0 > (indexUnits = SDDS_DefineColumn(outSet, "Units", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) ||
      0 > (indexSymbol = SDDS_DefineColumn(outSet, "Symbol", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) ||
      0 > (indexFormat = SDDS_DefineColumn(outSet, "Format", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) ||
      0 > (indexType = SDDS_DefineColumn(outSet, "Type", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) ||
      0 > (indexDescription = SDDS_DefineColumn(outSet, "Description", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) ||
      0 > (indexGroup = SDDS_DefineColumn(outSet, "Group", NULL, NULL, NULL, NULL, SDDS_STRING, 0)))
    return 0;
  if (0 > SDDS_DefineParameter(outSet, "Class", NULL, NULL, NULL, NULL, SDDS_STRING, 0) ||
      0 > SDDS_DefineParameter(outSet, "Filename", NULL, NULL, NULL, NULL, SDDS_STRING, 0))
    return 0;
  if (!SDDS_WriteLayout(outSet))
    return 0;
  return 1;
}

long MakeSDDSHeaderSummary(SDDS_DATASET *outSet, SDDS_DATASET *inSet, long listRequest, char *inputFile) {
  switch (listRequest) {
  case SET_COLUMN_LIST:
    if (!MakeColumnHeaderSummary(outSet, inSet, inputFile))
      return 0;
    break;
  case SET_PARAMETER_LIST:
    if (!MakeParameterHeaderSummary(outSet, inSet, inputFile))
      return 0;
    break;
  case SET_ARRAY_LIST:
    if (!MakeArrayHeaderSummary(outSet, inSet, inputFile))
      return 0;
    break;
  default:
    if (!MakeColumnHeaderSummary(outSet, inSet, inputFile) ||
        !MakeParameterHeaderSummary(outSet, inSet, inputFile) ||
        !MakeArrayHeaderSummary(outSet, inSet, inputFile))
      return 0;
    break;
  }
  return 1;
}

long MakeColumnHeaderSummary(SDDS_DATASET *outSet, SDDS_DATASET *inSet, char *inputFile) {
  SDDS_LAYOUT *layout;
  COLUMN_DEFINITION *coldef;
  long i;

  layout = &inSet->layout;
  if (!layout->n_columns)
    return 1;
  if (!SDDS_StartPage(outSet, layout->n_columns))
    return 0;
  for (i = 0; i < layout->n_columns; i++) {
    coldef = layout->column_definition + i;
    if (!SDDS_SetRowValues(outSet, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i,
                           indexName, coldef->name,
                           indexUnits, coldef->units,
                           indexSymbol, coldef->symbol,
                           indexFormat, coldef->format_string,
                           indexType, SDDS_type_name[coldef->type - 1],
                           indexDescription, coldef->description,
                           indexGroup, NULL, -1))
      return 0;
  }
  if (!SDDS_SetParameters(outSet, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "Class", "column",
                          "Filename", inputFile,
                          NULL))
    return 0;
  if (!SDDS_WritePage(outSet))
    return 0;
  return 1;
}

long MakeParameterHeaderSummary(SDDS_DATASET *outSet, SDDS_DATASET *inSet, char *inputFile) {
  SDDS_LAYOUT *layout;
  PARAMETER_DEFINITION *pardef;
  long i;

  layout = &inSet->layout;
  if (!layout->n_parameters)
    return 1;
  if (!SDDS_StartPage(outSet, layout->n_parameters))
    return 0;
  for (i = 0; i < layout->n_parameters; i++) {
    pardef = layout->parameter_definition + i;
    if (!SDDS_SetRowValues(outSet, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i,
                           indexName, pardef->name,
                           indexUnits, pardef->units,
                           indexSymbol, pardef->symbol,
                           indexFormat, pardef->format_string,
                           indexType, SDDS_type_name[pardef->type - 1],
                           indexDescription, pardef->description,
                           indexGroup, NULL, -1))
      return 0;
  }
  if (!SDDS_SetParameters(outSet, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "Class", "parameter",
                          "Filename", inputFile,
                          NULL))
    return 0;
  if (!SDDS_WritePage(outSet))
    return 0;
  return 1;
}

long MakeArrayHeaderSummary(SDDS_DATASET *outSet, SDDS_DATASET *inSet, char *inputFile) {
  SDDS_LAYOUT *layout;
  ARRAY_DEFINITION *arrdef;
  long i;

  layout = &inSet->layout;
  if (!layout->n_arrays)
    return 1;
  if (!SDDS_StartPage(outSet, layout->n_arrays))
    return 0;
  for (i = 0; i < layout->n_arrays; i++) {
    arrdef = layout->array_definition + i;
    if (!SDDS_SetRowValues(outSet, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i,
                           indexName, arrdef->name,
                           indexUnits, arrdef->units,
                           indexSymbol, arrdef->symbol,
                           indexFormat, arrdef->format_string,
                           indexType, SDDS_type_name[arrdef->type - 1],
                           indexDescription, arrdef->description,
                           indexGroup, arrdef->group_name,
                           -1))
      return 0;
  }
  if (!SDDS_SetParameters(outSet, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "Class", "array",
                          "Filename", inputFile,
                          NULL))
    return 0;
  if (!SDDS_WritePage(outSet))
    return 0;
  return 1;
}
