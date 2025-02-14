/**
 * @file lba2sdds.c
 * @brief Converts a Spiricon LBA (Laser-Beam Analyzer) file to SDDS format.
 *
 * @details
 * This program reads a Laser-Beam Analyzer (LBA) file from Spiricon and converts it into the 
 * Self Describing Data Set (SDDS) format. This conversion facilitates the analysis and visualization 
 * of beam data using SDDS-compatible tools.
 *
 * The program requires that the data type be set to `character`.
 *
 * @section Usage
 * ```
 * lba2sdds [<inputfile>] [<outputfile>]
 *          [-pipe[=input][,output]]
 *          -definition=<name>,<definition-entries>
 *          [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-definition`                         | Defines SDDS columns with the specified name and entries.                             |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use pipe for input and/or output.                                                     |
 * | `-majorOrder`                         | Sets data major order to row or column. Defaults to row order.                        |
 *
 * @subsection SR Specific Requirements
 *   - For `-definition`:
 *     - Must include `type=character` in the definition string.
 *   - Input file must be in a recognizable LBA format (A, B, or C header).
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

/* Enumeration for option types */
enum option_type {
  SET_DEFINITION,
  SET_PIPE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "definition",
  "pipe",
  "majorOrder",
};

/* Improved usage message for better readability */
char *USAGE =
  "lba2sdds [<inputfile>] [<outputfile>]\n"
  "         [-pipe[=input][,output]]\n"
  "          -definition=<name>,<definition-entries>\n"
  "         [-majorOrder=row|column]\n\n"
  "Options:\n"
  "  -pipe[=input][,output]         Use pipe for input and/or output.\n"
  "  -definition=<name>,<entries>   Define SDDS columns with name and entries.\n"
  "  -majorOrder=row|column         Set data major order to row or column.\n\n"
  "Description:\n"
  "  lba2sdds converts a Spiricon LBA file to SDDS format. The definition entries\n"
  "  are specified as <keyword>=<value>, where each keyword is a valid SDDS column field name.\n\n"
  "Program by Michael Borland (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

char *process_column_definition(char **argv, long argc);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_dataset;
  SCANNED_ARG *scanned;
  long i_arg;
  char *input, *output, *definition;
  long hsize, vsize;
  char *data, *data_name;
  char header[200];
  char ts1[100], ts2[100];
  FILE *fpi;
  unsigned long pipeFlags, majorOrderFlag;
  short columnMajorOrder = 0;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scanned, argc, argv);
  if (argc < 4) {
    bomb(NULL, USAGE);
  }

  input = output = data_name = NULL;
  definition = NULL;
  pipeFlags = 0;
  hsize = vsize = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* Process options here */
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[i_arg].n_items--;
        if (scanned[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[i_arg].list + 1, &scanned[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("Invalid -majorOrder syntax or values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;

      case SET_DEFINITION:
        data_name = scanned[i_arg].list[1];
        definition = process_column_definition(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1);
        if (!strstr(definition, "type=character"))
          SDDS_Bomb("Data type must be character for now");
        break;

      case SET_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;

      default:
        bomb("Invalid option seen", USAGE);
        break;
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else
        bomb("Too many filenames provided", USAGE);
    }
  }

  processFilenames("lba2sdds", &input, &output, pipeFlags, 0, NULL);

  if (!definition)
    SDDS_Bomb("Definition not specified");

  if (input)
    fpi = fopen_e(input, "r", 0);
  else
    fpi = stdin;

  if (fread(header, sizeof(*header), 200, fpi) != 200)
    SDDS_Bomb("Unable to read LBA file header");

  switch (header[0]) {
  case 'A':
    hsize = vsize = 120;
    break;
  case 'B':
    vsize = 256;
    hsize = 240;
    break;
  case 'C':
    vsize = 512;
    hsize = 480;
    break;
  default:
    SDDS_Bomb("Data does not appear to be in LBA format--invalid frame type");
    break;
  }

  sprintf(ts1, "%ld", hsize);
  sprintf(ts2, "%ld", vsize);

  if (!SDDS_InitializeOutput(&SDDS_dataset, SDDS_BINARY, 0,
                             "Screen image from LBA file", "Screen Image",
                             output) ||
      SDDS_ProcessColumnString(&SDDS_dataset, definition, 0) < 0 ||
      SDDS_DefineParameter(&SDDS_dataset, "NumberOfRows", NULL, NULL, "Number of rows", NULL, SDDS_LONG, ts1) < 0 ||
      SDDS_DefineParameter(&SDDS_dataset, "NumberOfColumns", NULL, NULL, "Number of columns", NULL, SDDS_LONG, ts2) < 0) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  SDDS_dataset.layout.data_mode.column_major = columnMajorOrder;

  if (!SDDS_WriteLayout(&SDDS_dataset))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  data = tmalloc(sizeof(*data) * hsize * vsize);

  do {
    if (fread(data, sizeof(*data), hsize * vsize, fpi) != hsize * vsize)
      SDDS_Bomb("Unable to read all data from input file");

    if (!SDDS_StartPage(&SDDS_dataset, hsize * vsize) ||
        !SDDS_SetColumn(&SDDS_dataset, SDDS_SET_BY_NAME, data, hsize * vsize, data_name) ||
        !SDDS_WritePage(&SDDS_dataset)) {
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  } while (fread(header, sizeof(*header), 200, fpi) == 200);

  fclose(fpi);

  if (!SDDS_Terminate(&SDDS_dataset))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  return EXIT_SUCCESS;
}

char *process_column_definition(char **argv, long argc) {
  char buffer[SDDS_MAXLINE], *ptr;
  long i;

  if (argc < 1)
    return NULL;

  sprintf(buffer, "&column name=%s, ", argv[0]);

  for (i = 1; i < argc; i++) {
    if (!strchr(argv[i], '=')) {
      return NULL;
    }
    strcat(buffer, argv[i]);
    strcat(buffer, ", ");
  }

  if (!strstr(buffer, "type=")) {
    strcat(buffer, "type=character ");
  }

  strcat(buffer, "&end");
  cp_str(&ptr, buffer);
  return ptr;
}
