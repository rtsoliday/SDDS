/**
 * @file raw2sdds.c
 * @brief Converts a binary raw data stream to SDDS format.
 *
 * @details
 * This program reads a binary raw data file and converts it into the SDDS format, widely used for storing and sharing scientific data. 
 * Users can define the structure of the SDDS columns, specify the data size, and choose the major order for the output file.
 *
 * @section Usage
 * ```
 * raw2sdds <inputfile> <outputfile>
 *          -definition=<name>,<definition-entries>
 *         [-size=<horiz-pixels>,<vert-pixels>] 
 *         [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-definition`                         | Defines the SDDS columns. Each definition entry must be in the form `<keyword>=<value>`.   |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-size`                               | Specifies the horizontal and vertical size of the data in pixels. Defaults: 484x512.     |
 * | `-majorOrder`                         | Sets the major order for data storage. Defaults to `row`.                              |
 *
 * @subsection Incompatibilities
 *   - `-definition` requires the keyword `type=character`. Other data types are not yet supported.
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "match_string.h"

#define DEFAULT_HSIZE 484
#define DEFAULT_VSIZE 512

enum option_type {
  SET_DEFINITION,  /* Set the definition of SDDS columns */
  SET_SIZE,        /* Set the size of the data */
  SET_MAJOR_ORDER, /* Set the major order (row or column) */
  N_OPTIONS        /* Number of options */
};

char *option[N_OPTIONS] = {
  "definition",
  "size",
  "majorOrder"
};

char *USAGE =
  "Usage: raw2sdds <inputfile> <outputfile>\n"
  "                -definition=<name>,<definition-entries>\n"
  "               [-size=<horiz-pixels>,<vert-pixels>] \n"
  "               [-majorOrder=row|column]\n"
  "Options:\n"
  "  -definition=<name>,<definition-entries>\n"
  "      Defines the SDDS columns. Each definition entry should be in the form <keyword>=<value>.\n"
  "      Example: -definition=Data,type=character\n\n"
  "  -size=<horiz-pixels>,<vert-pixels>\n"
  "      Specifies the horizontal and vertical size of the data in pixels.\n"
  "      Defaults are 484 horizontally and 512 vertically if not specified.\n"
  "      Example: -size=800,600\n\n"
  "  -majorOrder=row|column\n"
  "      Sets the major order of the output file data. Choose 'row' for row-major order or 'column' for column-major order.\n"
  "      Defaults to row-major if not specified.\n"
  "      Example: -majorOrder=column\n\n"
  "raw2sdds converts a binary data stream to SDDS format. The definition entries are of the form <keyword>=<value>,\n"
  "where the keyword is any valid field name for an SDDS column.\n\n"
  "Program by Michael Borland (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

char *process_column_definition(char **argv, long argc);

int main(int argc, char **argv) {
  SDDS_TABLE SDDS_table;
  SCANNED_ARG *scanned;
  unsigned long majorOrderFlag;
  long i_arg;
  char *input, *output, *definition;
  long hsize, vsize;
  char *data, *data_name;
  char ts1[100], ts2[100];
  FILE *fpi;
  short columnMajorOrder = 0;

  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  input = output = data_name = NULL;
  hsize = DEFAULT_HSIZE;
  vsize = DEFAULT_VSIZE;
  definition = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* Process options here */
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[i_arg].n_items--;
        if (scanned[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[i_arg].list + 1, &scanned[i_arg].n_items,
                           0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          fprintf(stderr, "Error: Invalid -majorOrder syntax or values.\n");
          return EXIT_FAILURE;
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;

      case SET_DEFINITION:
        if (scanned[i_arg].n_items < 2) {
          fprintf(stderr, "Error: -definition requires at least a name and one definition entry.\n");
          fprintf(stderr, "%s", USAGE);
          return EXIT_FAILURE;
        }
        data_name = scanned[i_arg].list[1];
        definition = process_column_definition(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1);
        if (!definition) {
          fprintf(stderr, "Error: Invalid column definition.\n");
          return EXIT_FAILURE;
        }
        if (!strstr(definition, "type=character")) {
          fprintf(stderr, "Error: Data type must be 'character' for now.\n");
          return EXIT_FAILURE;
        }
        break;

      case SET_SIZE:
        if (scanned[i_arg].n_items != 3 ||
            sscanf(scanned[i_arg].list[1], "%ld", &hsize) != 1 || hsize <= 0 ||
            sscanf(scanned[i_arg].list[2], "%ld", &vsize) != 1 || vsize <= 0) {
          fprintf(stderr, "Error: Invalid -size syntax.\n");
          fprintf(stderr, "%s", USAGE);
          return EXIT_FAILURE;
        }
        break;

      default:
        fprintf(stderr, "Error: Invalid option '%s'.\n", scanned[i_arg].list[0]);
        fprintf(stderr, "%s", USAGE);
        return EXIT_FAILURE;
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else {
        fprintf(stderr, "Error: Too many filenames provided.\n");
        fprintf(stderr, "%s", USAGE);
        return EXIT_FAILURE;
      }
    }
  }

  if (!input) {
    fprintf(stderr, "Error: Input file not specified.\n");
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  if (!output) {
    fprintf(stderr, "Error: Output file not specified.\n");
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  if (!definition) {
    fprintf(stderr, "Error: Column definition not specified.\n");
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  snprintf(ts1, sizeof(ts1), "%ld", hsize);
  snprintf(ts2, sizeof(ts2), "%ld", vsize);

  if (!SDDS_InitializeOutput(&SDDS_table, SDDS_BINARY, 0,
                             "Screen image from raw file", "screen image",
                             output) ||
      SDDS_ProcessColumnString(&SDDS_table, definition, 0) < 0 ||
      SDDS_DefineParameter(&SDDS_table, "NumberOfRows", NULL, NULL, "number of rows", NULL, SDDS_LONG, ts1) < 0 ||
      SDDS_DefineParameter(&SDDS_table, "NumberOfColumns", NULL, NULL, "number of columns", NULL, SDDS_LONG, ts2) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  SDDS_table.layout.data_mode.column_major = columnMajorOrder;

  if (!SDDS_WriteLayout(&SDDS_table) || !SDDS_StartTable(&SDDS_table, hsize * vsize)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  data = tmalloc(sizeof(*data) * hsize * vsize);
  fpi = fopen_e(input, "rb", 0);
  if (fread(data, sizeof(*data), hsize * vsize, fpi) != (size_t)(hsize * vsize)) {
    fclose(fpi);
    fprintf(stderr, "Error: Unable to read all data from input file '%s'.\n", input);
    return EXIT_FAILURE;
  }
  fclose(fpi);

  if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, hsize * vsize, data_name) ||
      !SDDS_WriteTable(&SDDS_table) ||
      !SDDS_Terminate(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  free(data);
  return EXIT_SUCCESS;
}

char *process_column_definition(char **argv, long argc) {
  char buffer[SDDS_MAXLINE] = "&column name=";
  char *ptr;
  long i;

  if (argc < 1)
    return NULL;

  strcat(buffer, argv[0]);
  strcat(buffer, ", ");

  for (i = 1; i < argc; i++) {
    if (!strchr(argv[i], '=')) {
      return NULL;
    }
    strcat(buffer, argv[i]);
    strcat(buffer, ", ");
  }

  if (!strstr(buffer, "type=")) {
    strcat(buffer, "type=character, ");
  }

  strcat(buffer, "&end");

  cp_str(&ptr, buffer);
  return ptr;
}
