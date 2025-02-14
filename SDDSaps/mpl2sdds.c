/**
 * @file mpl2sdds.c
 * @brief Converts M. Borland's older `mpl` (Multi-Purpose Library) files into a single SDDS file.
 *
 * @details
 * This program processes multiple `mpl` (Multi-Purpose Library) files and consolidates their data
 * into a single SDDS (Self Describing Data Set) file. It supports both ASCII and binary formats
 * for the output SDDS file and provides options to overwrite existing data or create a new file.
 *
 * The program ensures consistent formatting of the resulting SDDS file, handling cases such as
 * differing numbers of rows in input files or existing data conflicts.
 *
 * @section Usage
 * ```
 * mpl2sdds <mpl-filename> [<mpl-filename>...] -output=<SDDS-filename> [-erase] [-binary]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                 |
 * |---------------------------------------|-----------------------------------------------------------------------------|
 * | `-output`                             | Specifies the output SDDS file.                                             |
 *
 * | Optional                              | Description                                                                 |
 * |---------------------------------------|-----------------------------------------------------------------------------|
 * | `-erase`                              | Erase existing data in the output SDDS file before adding new data.         |
 * | `-binary`                             | Write the SDDS file in binary format instead of ASCII.                      |
 *
 * @subsection Incompatibilities
 *   - `-erase` is incompatible with loading existing data from the specified output file.
 *
 * @subsection Requirements
 *   - Input MPL files must have matching numbers of rows.
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
 * M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include "table.h"
#include "SDDS.h"
#include "scan.h"
#include "match_string.h"

/* Enumeration for option types */
enum option_type {
  SET_ERASE,
  SET_OUTPUT,
  SET_BINARY,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "erase",
  "output",
  "binary"
};

/* Improved usage message */
char *USAGE =
  "mpl2sdds <mpl-filename> [<mpl-filename>...] \n"
  "          -output=<SDDS-filename>\n"
  "         [-erase] [-binary]\n\n"
  "Options:\n"
  "  -output=<SDDS-filename>   Specifies the output SDDS file. This option is mandatory.\n"
  "  -erase                    Erase existing data in the output SDDS file before adding new data.\n"
  "  -binary                   Output the SDDS file in binary format.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ").";

void extract_name_and_unit(char **name, char **unit, char *label);
long add_definition(SDDS_DATASET *SDDS_dataset, char *label, char *filename);
void fix_mpl_name(char *name);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_dataset;
  TABLE *mpl_data;
  SCANNED_ARG *scanned;
  char **input;
  long i, j, i_arg, inputs;
  char *output;
  long erase, rows, new_columns, SDDS_rows;
  double **data;
  long *index;
  long binary;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    bomb(NULL, USAGE);
  }

  input = NULL;
  output = NULL;
  inputs = erase = binary = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* Process options here */
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_ERASE:
        erase = 1;
        break;
      case SET_OUTPUT:
        if (scanned[i_arg].n_items != 2) {
          bomb("Invalid syntax for -output option.", USAGE);
        }
        output = scanned[i_arg].list[1];
        break;
      case SET_BINARY:
        if (scanned[i_arg].n_items != 1) {
          bomb("Invalid syntax for -binary option.", USAGE);
        }
        binary = 1;
        break;
      default:
        bomb("Unknown option provided.", USAGE);
      }
    } else {
      input = trealloc(input, (inputs + 1) * sizeof(*input));
      input[inputs++] = scanned[i_arg].list[0];
    }
  }

  if (!output) {
    bomb("The -output option must be specified.", USAGE);
  }
  if (!input) {
    bomb("No input MPL files provided.", USAGE);
  }

  if (!erase && fexists(output)) {
    /* Load data from existing file */
    if (!SDDS_InitializeInput(&SDDS_dataset, output)) {
      fprintf(stderr, "Error: Unable to read SDDS layout from %s.\n", output);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
    if (!SDDS_ReadPage(&SDDS_dataset)) {
      fprintf(stderr, "Error: Unable to read data table from %s.\n", output);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
    SDDS_rows = SDDS_CountRowsOfInterest(&SDDS_dataset);
    fclose(SDDS_dataset.layout.fp);
    SDDS_dataset.layout.fp = fopen_e(output, "w", 0);
  } else {
    /* Start a new file */
    if (!SDDS_InitializeOutput(&SDDS_dataset, binary ? SDDS_BINARY : SDDS_ASCII, 1, NULL, NULL, output)) {
      fprintf(stderr, "Error: Unable to initialize output SDDS structure for %s.\n", output);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
    SDDS_rows = -1;
  }

  rows = 0;
  mpl_data = tmalloc(sizeof(*mpl_data) * inputs);
  data = tmalloc(sizeof(*data) * (2 * inputs));
  index = tmalloc(sizeof(*index) * (2 * inputs));
  new_columns = 0;

  for (i = 0; i < inputs; i++) {
    if (!get_table(&mpl_data[i], input[i], 1, 0)) {
      fprintf(stderr, "Warning: Unable to read data from %s. Continuing with other files.\n", input[i]);
      continue;
    }

    if (!rows) {
      if (!(rows = mpl_data[i].n_data)) {
        fprintf(stderr, "Warning: No data in file %s. Continuing with other files.\n", input[i]);
        continue;
      }
    } else if (rows != mpl_data[i].n_data) {
      SDDS_Bomb("All MPL files must have the same number of data points.");
    } else if (SDDS_rows != -1 && rows != SDDS_rows) {
      SDDS_Bomb("Number of data points in MPL files must match the number of rows in the SDDS file.");
    }

    if ((index[new_columns] = add_definition(&SDDS_dataset, mpl_data[i].xlab, input[i])) < 0) {
      free(mpl_data[i].c1);
    } else {
      data[new_columns++] = mpl_data[i].c1;
    }

    if ((index[new_columns] = add_definition(&SDDS_dataset, mpl_data[i].ylab, input[i])) < 0) {
      free(mpl_data[i].c2);
    } else {
      data[new_columns++] = mpl_data[i].c2;
    }
  }

  if (!rows || !new_columns) {
    SDDS_Bomb("All input files are empty or invalid.");
  }

  if (!SDDS_WriteLayout(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }
  if (!SDDS_StartPage(&SDDS_dataset, rows)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  for (i = 0; i < new_columns; i++) {
    for (j = 0; j < rows; j++) {
      SDDS_SetRowValues(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, index[i], data[i][j], -1);
    }
  }

  if (!SDDS_WritePage(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

long add_definition(SDDS_DATASET *SDDS_dataset, char *label, char *filename) {
  char *symbol, *name, *unit;
  long index;

  extract_name_and_unit(&symbol, &unit, label);
  SDDS_CopyString(&name, symbol);
  fix_mpl_name(name);

  if (SDDS_GetColumnIndex(SDDS_dataset, name) >= 0) {
    fprintf(stderr, "Warning: Column name '%s' from file '%s' already exists and will be ignored.\n", name, filename);
    return -1;
  }

  if ((index = SDDS_DefineColumn(SDDS_dataset, name, symbol, unit, NULL, NULL, SDDS_DOUBLE, 0)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return -1;
  }

  return index;
}

void extract_name_and_unit(char **name, char **unit, char *label) {
  char *ptr, *uptr;

  if ((uptr = strchr(label, '('))) {
    *uptr++ = '\0';
    if ((ptr = strchr(uptr, ')'))) {
      *ptr = '\0';
    }
    SDDS_CopyString(unit, uptr);
  } else {
    *unit = NULL;
  }

  /* Trim trailing spaces from label */
  ptr = label + strlen(label) - 1;
  while (ptr != label && *ptr == ' ') {
    *ptr-- = '\0';
  }
  SDDS_CopyString(name, label);
}

void fix_mpl_name(char *name) {
  char *ptr, *ptr1;

  ptr = name;
  while ((ptr = strchr(ptr, '$'))) {
    switch (*(ptr + 1)) {
    case 'a':
    case 'b':
    case 'n':
    case 'g':
    case 'r':
    case 's':
    case 'e':
      strcpy_ss(ptr, ptr + 2);
      break;
    default:
      ptr += 1;
      break;
    }
  }

  ptr = name;
  while ((ptr = strchr(ptr, ' '))) {
    ptr1 = ptr;
    while (*ptr1 == ' ') {
      ptr1++;
    }
    strcpy_ss(ptr, ptr1);
  }
}
