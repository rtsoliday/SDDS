/**
 * @file sdds2math.c
 * @brief Converts SDDS files to a Mathematica-compatible format.
 *
 * @details
 * This program reads an SDDS (Self Describing Data Sets) file and converts it into a format
 * that can be easily imported and used within Mathematica. The output is structured as a
 * single Mathematica variable containing descriptions, column definitions, parameter
 * definitions, array definitions, associations, and tables of data.
 *
 * The output Mathematica variable has the following structure:
 * ```
 * sdds = {description, coldef, pardef, arraydef, associates, tables}
 * ```
 * - **description**: Contains text and contents descriptions.
 * - **coldef**: Defines columns with name, units, symbol, format, type, field length, and description.
 * - **pardef**: Defines parameters with name, fixed value, units, symbol, type, and description.
 * - **arraydef**: Defines arrays with name, units, symbol, format, type, field length, group, and description.
 * - **associates**: Lists associations with SDDS flag, filename, path, contents, and description.
 * - **tables**: Contains data tables with parameters and row data.
 *
 * @section Usage
 * ```
 * sdds2math [<SDDSfilename>] [<outputname>]
 *           [-pipe[=in][,out]]
 *           [-comments]
 *           [-verbose]
 *           [-format=<format-string>]
 * ```
 *
 * @section Options
 * | Option                     | Description                                                                 |
 * |----------------------------|-----------------------------------------------------------------------------|
 * | `-pipe`                    | Standard SDDS Toolkit pipe option.                                          |
 * | `-comments`                | Include helpful Mathematica comments in the output file.                    |
 * | `-format`                  | Specify the format for double precision numbers (Default: `%g`).            |
 * | `-verbose`                 | Display header information to the terminal similar to `sddsquery`.          |
 *
 * @subsection SR Specific Requirements
 *   - If `-format=<format-string>` is used, `<format-string>` must be a valid `printf`-style format.
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
 * K. Evans, C. Saunders, M. Borland, R. Soliday
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_COMMENTS,
  SET_FORMAT,
  SET_VERBOSE,
  SET_PIPE,
  N_OPTIONS
};

#define FORMAT "%g"

/* Option strings corresponding to enum option_type */
static char *option[N_OPTIONS] = {
  "comments",
  "format",
  "verbose",
  "pipe"
};

/* Improved usage message */
static char *USAGE =
  "\nUsage:\n"
  "  sdds2math [<SDDSfilename>] [<outputname>]\n"
  "            [-pipe[=in][,out]]\n"
  "            [-comments]\n"
  "            [-verbose]\n"
  "            [-format=<format-string>]\n"
  "Options:\n"
  "  -pipe[=in][,out]           Standard SDDS Toolkit pipe option.\n"
  "  -comments                  Include helpful Mathematica comments in the output file.\n"
  "  -format=<format-string>    Specify the format for double precision numbers (Default: " FORMAT ").\n"
  "  -verbose                   Display header information to the terminal.\n"
  "\n"
  "Description:\n"
  "  sdds2math converts an SDDS file into a Mathematica-readable format.\n"
  "  The output is a single Mathematica variable with the structure:\n"
  "    sdds = {description, coldef, pardef, arraydef, associates, tables}\n"
  "  where each component contains detailed information about the SDDS data.\n"
  "\n"
  "Author:\n"
  "  Kenneth Evans (Original version: 1994)\n"
  "  Updated: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

int main(int argc, char **argv) {
  FILE *outfile;
  SDDS_TABLE SDDS_table;
  SDDS_LAYOUT *layout;
  COLUMN_DEFINITION *coldef;
  PARAMETER_DEFINITION *pardef;
  ARRAY_DEFINITION *arraydef;
  char s[256];
  char *input, *output;
  long i, i_arg, ntable;
  int64_t nrows, j;
  SCANNED_ARG *s_arg;
  char *text, *contents, *ss, *ptr, *iformat = FORMAT, *format, *rformat;
  long verbose = 0, comments = 0, addquotes = 1;
  short nexp;
  double dd, ddred;
  float ff, ffred;
  void *data;
  unsigned long pipeFlags;

  SDDS_RegisterProgramName(argv[0]);

  input = output = NULL;
  pipeFlags = 0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_COMMENTS:
        comments = 1;
        break;
      case SET_FORMAT:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -format syntax");
        iformat = s_arg[i_arg].list[1]; /* Input format */
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
        fprintf(stderr, "%s", USAGE);
        exit(EXIT_FAILURE);
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

  processFilenames("sdds2math", &input, &output, pipeFlags, 0, NULL);

  if (output) {
    outfile = fopen(output, "w");
    if (!outfile) {
      fprintf(stderr, "Error: Cannot open output file '%s'\n", output);
      exit(EXIT_FAILURE);
    }
  } else {
    outfile = stdout;
  }

  /* Calculate formats for converting to Mathematica convention */
  format = (char *)calloc(256, sizeof(char)); /* Whole format */
  if (!format) {
    fprintf(stderr, "Memory allocation error for format.\n");
    exit(EXIT_FAILURE);
  }

  rformat = (char *)calloc(256, sizeof(char)); /* Part before e */
  if (!rformat) {
    fprintf(stderr, "Memory allocation error for rformat.\n");
    free(format);
    exit(EXIT_FAILURE);
  }

  strcpy(format, iformat);
  if ((ptr = strchr(format, 'E')))
    *ptr = 'e'; /* Convert 'E' to 'e' */
  if ((ptr = strchr(format, 'G')))
    *ptr = 'g'; /* Convert 'G' to 'g' */
  strcpy(rformat, format);
  if ((ptr = strpbrk(rformat, "eg")))
    *ptr = 'f';

  /* Initialize SDDS input */
  if (!SDDS_InitializeInput(&SDDS_table, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  layout = &SDDS_table.layout;

  /* Start top level */
  fprintf(outfile, "{");

  /* Description */
  fprintf(outfile, "{");
  if (verbose)
    printf("\nFile '%s' is in SDDS protocol version %" PRId32 "\n", input, layout->version);

  if (!SDDS_GetDescription(&SDDS_table, &text, &contents))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (text) {
    if (verbose)
      printf("Description: %s\n", text);
    fprintf(outfile, "\"%s\",", text);
  }

  if (contents) {
    if (verbose)
      printf("Contents: %s\n", contents);
    fprintf(outfile, "\"%s\"", contents);
  } else {
    fprintf(outfile, "\"No contents\"");
  }

  if (layout->data_mode.mode == SDDS_ASCII) {
    if (verbose)
      printf("\nData is ASCII with %" PRId32 " lines per row and %" PRId32 " additional header lines expected.\n",
             layout->data_mode.lines_per_row, layout->data_mode.additional_header_lines);
    if (verbose)
      printf("Row counts: %s\n", layout->data_mode.no_row_counts ? "No" : "Yes");
  } else if (verbose) {
    printf("\nData is binary\n");
  }

  fprintf(outfile, "},\n");

  /* Columns */
  fprintf(outfile, " {");
  if (layout->n_columns) {
    if (verbose)
      printf("\n%" PRId32 " columns of data:\n", layout->n_columns);
    if (verbose)
      printf("NAME            UNITS           SYMBOL          FORMAT          TYPE    FIELD  DESCRIPTION\n");
    if (verbose)
      printf("                                                                        LENGTH\n");
    for (i = 0; i < layout->n_columns; i++) {
      if (i > 0)
        fprintf(outfile, ",\n  ");
      coldef = layout->column_definition + i;
      if (verbose)
        printf("%-15s %-15s %-15s %-15s %-7s %-7" PRId32 " %s\n",
               coldef->name ? coldef->name : "No name",
               coldef->units ? coldef->units : "",
               coldef->symbol ? coldef->symbol : "",
               coldef->format_string ? coldef->format_string : "",
               SDDS_type_name[coldef->type - 1],
               coldef->field_length,
               coldef->description ? coldef->description : "No description");
      fprintf(outfile, "{\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%" PRId32 ",\"%s\"}",
              coldef->name ? coldef->name : "No name",
              coldef->units ? coldef->units : "",
              coldef->symbol ? coldef->symbol : "",
              coldef->format_string ? coldef->format_string : "",
              SDDS_type_name[coldef->type - 1],
              coldef->field_length,
              coldef->description ? coldef->description : "No description");
    }
  }
  fprintf(outfile, "},\n");

  /* Parameters */
  fprintf(outfile, " {");
  if (layout->n_parameters) {
    if (verbose)
      printf("\n%" PRId32 " parameters:\n", layout->n_parameters);
    if (verbose)
      printf("NAME                UNITS               SYMBOL              TYPE                DESCRIPTION\n");
    for (i = 0; i < layout->n_parameters; i++) {
      if (i > 0)
        fprintf(outfile, ",\n  ");
      pardef = layout->parameter_definition + i;
      if (verbose)
        printf("%-19s %-19s %-19s %-19s %s\n",
               pardef->name ? pardef->name : "No name",
               pardef->units ? pardef->units : "",
               pardef->symbol ? pardef->symbol : "",
               SDDS_type_name[pardef->type - 1],
               pardef->description ? pardef->description : "No description");
      fprintf(outfile, "{\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"}",
              pardef->name ? pardef->name : "No name",
              pardef->fixed_value ? pardef->fixed_value : "",
              pardef->units ? pardef->units : "",
              pardef->symbol ? pardef->symbol : "",
              SDDS_type_name[pardef->type - 1],
              pardef->description ? pardef->description : "No description");
    }
  }
  fprintf(outfile, "},\n");

  /* Arrays */
  fprintf(outfile, " {");
  if (layout->n_arrays) {
    if (verbose)
      printf("\n%" PRId32 " arrays of data:\n", layout->n_arrays);
    if (verbose)
      printf("NAME            UNITS           SYMBOL          FORMAT  TYPE            FIELD   GROUP           DESCRIPTION\n");
    if (verbose)
      printf("                                                                        LENGTH  NAME\n");
    for (i = 0; i < layout->n_arrays; i++) {
      if (i > 0)
        fprintf(outfile, ",\n  ");
      arraydef = layout->array_definition + i;
      if (verbose)
        printf("%-15s %-15s %-15s %-7s %-8s*^%-5" PRId32 " %-7" PRId32 " %-15s %s\n",
               arraydef->name ? arraydef->name : "No name",
               arraydef->units ? arraydef->units : "",
               arraydef->symbol ? arraydef->symbol : "",
               arraydef->format_string ? arraydef->format_string : "",
               SDDS_type_name[arraydef->type - 1],
               arraydef->dimensions,
               arraydef->field_length,
               arraydef->group_name ? arraydef->group_name : "",
               arraydef->description ? arraydef->description : "No description");
      fprintf(outfile, "{\"%s\",\"%s\",\"%s\",\"%s\",\"%s*^%" PRId32 "\",%" PRId32 ",\"%s\",\"%s\"}",
              arraydef->name ? arraydef->name : "No name",
              arraydef->units ? arraydef->units : "",
              arraydef->symbol ? arraydef->symbol : "",
              arraydef->format_string ? arraydef->format_string : "",
              SDDS_type_name[arraydef->type - 1],
              arraydef->dimensions,
              arraydef->field_length,
              arraydef->group_name ? arraydef->group_name : "",
              arraydef->description ? arraydef->description : "No description");
    }
  }
  fprintf(outfile, "},\n");

  /* Associates */
  fprintf(outfile, " {");
  if (layout->n_associates) {
    if (verbose)
      printf("\n%" PRId32 " associates:\n", layout->n_associates);
    if (verbose)
      printf("SDDS  FILENAME            PATH                          CONTENTS            DESCRIPTION\n");
    for (i = 0; i < layout->n_associates; i++) {
      if (i > 0)
        fprintf(outfile, ",\n  ");
      if (verbose)
        printf("%-5s %-19s %-29s %-19s %s\n",
               layout->associate_definition[i].sdds ? "True" : "False",
               layout->associate_definition[i].filename ? layout->associate_definition[i].filename : "",
               layout->associate_definition[i].path ? layout->associate_definition[i].path : "",
               layout->associate_definition[i].contents ? layout->associate_definition[i].contents : "",
               layout->associate_definition[i].description ? layout->associate_definition[i].description : "No description");
      fprintf(outfile, "{\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"}",
              layout->associate_definition[i].sdds ? "True" : "False",
              layout->associate_definition[i].filename ? layout->associate_definition[i].filename : "",
              layout->associate_definition[i].path ? layout->associate_definition[i].path : "",
              layout->associate_definition[i].contents ? layout->associate_definition[i].contents : "",
              layout->associate_definition[i].description ? layout->associate_definition[i].description : "No description");
    }
  }
  fprintf(outfile, "},\n");

  /* Process tables */
  fprintf(outfile, " {"); /* Start of array of tables */
  while ((ntable = SDDS_ReadTable(&SDDS_table)) > 0) {
    if (ntable > 1)
      fprintf(outfile, ",\n  ");
    if (comments)
      fprintf(outfile, "(*Table %ld*)", ntable);
    fprintf(outfile, "{\n"); /* Start of this table */

    /* Variable parameters */
    fprintf(outfile, "   {"); /* Start of parameters */
    for (i = 0; i < layout->n_parameters; i++) {
      if (i > 0)
        fprintf(outfile, ",\n    ");
      pardef = layout->parameter_definition + i;
      if (!(data = SDDS_GetParameter(&SDDS_table, pardef->name, NULL))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      /* This parameter */
      if (comments)
        fprintf(outfile, "(* %s *)", pardef->name);

      addquotes = 1;
      switch (pardef->type) {
      case SDDS_DOUBLE:
        dd = *(double *)data;
        sprintf(s, format, dd);
        if ((ptr = strchr(s, 'e'))) {
          *ptr = ' ';
          sscanf(s, "%lf %hd", &ddred, &nexp);
          fprintf(outfile, rformat, ddred);
          fprintf(outfile, "*10^%d", nexp);
        } else {
          fprintf(outfile, "%s", s);
        }
        break;
      case SDDS_FLOAT:
        ff = *(float *)data;
        sprintf(s, format, ff);
        if ((ptr = strchr(s, 'e'))) {
          *ptr = ' ';
          sscanf(s, "%f %hd", &ffred, &nexp);
          fprintf(outfile, rformat, ffred);
          fprintf(outfile, "*10^%d", nexp);
        } else {
          fprintf(outfile, "%s", s);
        }
        break;
      case SDDS_STRING:
        ss = *(char **)data;
        if (*ss == '"')
          addquotes = 0;
        else if (SDDS_StringIsBlank(ss) || SDDS_HasWhitespace(ss))
          addquotes = 0;
      case SDDS_CHARACTER:
        if (addquotes)
          fprintf(outfile, "\"");
        SDDS_PrintTypedValue(data, 0, pardef->type, NULL, outfile, 0);
        if (addquotes)
          fprintf(outfile, "\"");
        break;
      default:
        SDDS_PrintTypedValue(data, 0, pardef->type, NULL, outfile, 0);
        break;
      }
    }
    fprintf(outfile, "},\n"); /* End of parameters */

    /* Columns */
    fprintf(outfile, "   {"); /* Start of data array */
    if (layout->n_columns) {
      SDDS_SetColumnFlags(&SDDS_table, 1);
      SDDS_SetRowFlags(&SDDS_table, 1);
      if ((nrows = SDDS_CountRowsOfInterest(&SDDS_table)) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      if (nrows) {
        for (j = 0; j < nrows; j++) {
          if (j > 0)
            fprintf(outfile, ",\n    ");
          fprintf(outfile, "{"); /* Start of row */
          for (i = 0; i < layout->n_columns; i++) {
            if (i > 0)
              fprintf(outfile, ",");
            coldef = layout->column_definition + i;
            if (!(data = SDDS_GetValue(&SDDS_table, coldef->name, j, NULL))) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }

            addquotes = 1;
            switch (coldef->type) {
            case SDDS_DOUBLE:
              dd = *(double *)data;
              sprintf(s, format, dd);
              if ((ptr = strchr(s, 'e'))) {
                *ptr = ' ';
                sscanf(s, "%lf %hd", &ddred, &nexp);
                fprintf(outfile, rformat, ddred);
                fprintf(outfile, "*10^%d", nexp);
              } else {
                fprintf(outfile, "%s", s);
              }
              break;
            case SDDS_FLOAT:
              ff = *(float *)data;
              sprintf(s, format, ff);
              if ((ptr = strchr(s, 'e'))) {
                *ptr = ' ';
                sscanf(s, "%f %hd", &ffred, &nexp);
                fprintf(outfile, rformat, ffred);
                fprintf(outfile, "*10^%d", nexp);
              } else {
                fprintf(outfile, "%s", s);
              }
              break;
            case SDDS_STRING:
              ss = *(char **)data;
              if (*ss == '"')
                addquotes = 0;
              else if (SDDS_StringIsBlank(ss) || SDDS_HasWhitespace(ss))
                addquotes = 0;
            case SDDS_CHARACTER:
              if (addquotes)
                fprintf(outfile, "\"");
              SDDS_PrintTypedValue(data, 0, coldef->type, NULL, outfile, 0);
              if (addquotes)
                fprintf(outfile, "\"");
              break;
            default:
              SDDS_PrintTypedValue(data, 0, coldef->type, NULL, outfile, 0);
              break;
            }
          }
          fprintf(outfile, "}"); /* End of row */
        }
      }
    }
    fprintf(outfile, "}"); /* End of data array */
    fprintf(outfile, "}"); /* End of this table */
  }
  fprintf(outfile, "\n }\n"); /* End of array of tables, last major component */

  /* End top level */
  fprintf(outfile, "}\n"); /* End of top level */

  /* Clean up and exit */
  fflush(stdout);
  if (!SDDS_Terminate(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  free(format);
  free(rformat);

  return EXIT_SUCCESS;
}
