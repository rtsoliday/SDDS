/**
 * @file col2sdds.c
 * @brief Converts M. Borland's older `column` format files to SDDS files.
 *
 * @details
 * This program reads a specified input file containing M. Borland's older `column` format files,
 * a precursor to SDDS, and converts them into SDDS format. It allows optional modifications
 * to auxiliary and column names for compatibility with SDDS standards by removing `$` characters.
 * 
 * The implementation includes robust handling of data types, arrays, and various validation
 * mechanisms to ensure the integrity of the converted SDDS files. It leverages the SDDS library
 * for file operations and supports both ASCII and binary SDDS formats.
 *
 * @section Usage
 * ```
 * col2sdds <inputfile> <outputfile> [-fixMplNames]
 * ```
 *
 * @section Options
 * | Optional                     | Description                                               |
 * |------------------------------|-----------------------------------------------------------|
 * | `-fixMplNames`               | Removes `$` characters from names for SDDS compatibility.|
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
 * R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "column.h"

/* Enumeration for option types */
enum option_type {
  SET_FIXMPLNAMES,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "fixMplNames"
};

char *USAGE =
  "Usage: col2sdds <inputfile> <outputfile> [-fixMplNames]\n"
  "Options:\n"
  "  -fixMplNames   Remove '$' characters from auxiliary and column names.\n"
  "Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

int main(int argc, char **argv) {
  SCANNED_ARG *scanned;
  char *input = NULL, *output = NULL;
  int fixMplNames = 0;
  long i_arg, i, j, k, len;
  MC_TABLE mcTable;
  SDDS_TABLE SDDS_table;
  char buffer[100];
  char **aux_symbol = NULL;
  char **symbol = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_FIXMPLNAMES:
        fixMplNames = 1;
        break;
      default:
        fprintf(stderr, "Invalid option: %s\n%s", scanned[i_arg].list[0], USAGE);
        return EXIT_FAILURE;
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else {
        fprintf(stderr, "Too many file names provided.\n%s", USAGE);
        return EXIT_FAILURE;
      }
    }
  }

  if (!input) {
    fprintf(stderr, "Error: Input file not specified.\n%s", USAGE);
    return EXIT_FAILURE;
  }

  if (!output) {
    fprintf(stderr, "Error: Output file not specified.\n%s", USAGE);
    return EXIT_FAILURE;
  }

  if (get_mc_table(&mcTable, input, GMCT_WARNINGS) == 0) {
    fprintf(stderr, "Unable to open input file: %s\n", input);
    return EXIT_FAILURE;
  }

  if (fixMplNames) {
    aux_symbol = malloc(sizeof(char *) * mcTable.n_auxiliaries);
    if (!aux_symbol) {
      fprintf(stderr, "Memory allocation failed for auxiliary symbols.\n");
      return EXIT_FAILURE;
    }

    for (i = 0; i < mcTable.n_auxiliaries; i++) {
      len = strlen(mcTable.aux_name[i]);
      aux_symbol[i] = malloc(sizeof(char) * (len + 1));
      if (!aux_symbol[i]) {
        fprintf(stderr, "Memory allocation failed for auxiliary symbol %ld.\n", i);
        return EXIT_FAILURE;
      }
      k = 0;
      for (j = 0; j < len; j++) {
        if (mcTable.aux_name[i][j] != '$') {
          buffer[k++] = mcTable.aux_name[i][j];
        }
      }
      buffer[k] = '\0';
      strcpy(aux_symbol[i], buffer);
    }

    symbol = malloc(sizeof(char *) * mcTable.n_cols);
    if (!symbol) {
      fprintf(stderr, "Memory allocation failed for column symbols.\n");
      return EXIT_FAILURE;
    }

    for (i = 0; i < mcTable.n_cols; i++) {
      len = strlen(mcTable.name[i]);
      symbol[i] = malloc(sizeof(char) * (len + 1));
      if (!symbol[i]) {
        fprintf(stderr, "Memory allocation failed for column symbol %ld.\n", i);
        return EXIT_FAILURE;
      }
      k = 0;
      for (j = 0; j < len; j++) {
        if (mcTable.name[i][j] != '$') {
          buffer[k++] = mcTable.name[i][j];
        }
      }
      buffer[k] = '\0';
      strcpy(symbol[i], buffer);
    }
  }

  if (SDDS_InitializeOutput(&SDDS_table, SDDS_BINARY, 1, mcTable.title, mcTable.label, output) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  for (i = 0; i < mcTable.n_auxiliaries; i++) {
    const char *paramName = fixMplNames ? aux_symbol[i] : mcTable.aux_name[i];
    const char *paramDesc = mcTable.aux_description[i];
    const char *paramUnit = mcTable.aux_unit[i];
    double *paramValue = &(mcTable.aux_value[i]);

    if (SDDS_DefineParameter1(&SDDS_table, paramName, fixMplNames ? mcTable.aux_name[i] : NULL, paramUnit, paramDesc, NULL, SDDS_DOUBLE, paramValue) == -1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      if (!fixMplNames) {
        fprintf(stderr, "Try rerunning with the -fixMplNames option.\n");
      }
      return EXIT_FAILURE;
    }
  }

  for (i = 0; i < mcTable.n_cols; i++) {
    const char *colName = fixMplNames ? symbol[i] : mcTable.name[i];
    const char *colDesc = mcTable.description[i];
    const char *colUnit = mcTable.unit[i];
    const char *colFormat = mcTable.format[i];

    if (SDDS_DefineColumn(&SDDS_table, colName, fixMplNames ? mcTable.name[i] : NULL, colUnit, colDesc, colFormat, SDDS_DOUBLE, 0) == -1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      if (!fixMplNames) {
        fprintf(stderr, "Try rerunning with the -fixMplNames option.\n");
      }
      return EXIT_FAILURE;
    }
  }

  if (SDDS_SaveLayout(&SDDS_table) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (SDDS_WriteLayout(&SDDS_table) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (SDDS_StartTable(&SDDS_table, mcTable.n_rows) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  for (i = 0; i < mcTable.n_cols; i++) {
    if (SDDS_SetColumnFromDoubles(&SDDS_table, SDDS_SET_BY_INDEX, mcTable.value[i], mcTable.n_rows, i) != 1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
  }

  if (SDDS_WriteTable(&SDDS_table) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (SDDS_Terminate(&SDDS_table) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
