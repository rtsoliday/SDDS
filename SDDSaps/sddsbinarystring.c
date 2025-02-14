/**
 * @file sddsbinarystring.c
 * @brief Converts integer type columns from SDDS files into binary string representations.
 *
 * @details
 * This program reads integer columns from an SDDS (Self-Describing Data Set) file and converts
 * their values into binary string representations. The resulting binary strings are appended as
 * new columns in the output SDDS file. If no column names are specified, all integer columns
 * will be converted. The binary string columns are named as `<originalColumnName>BinaryString`.
 *
 * @section Usage
 * ```
 * sddsbinarystring [<source-file>] [<target-file>] 
 *                  [-pipe=[input][,output]] 
 *                  [-column=<list of column names>]
 * ```
 *
 * @section Options
 * | Required  | Description                                                                 |
 * |-----------|-----------------------------------------------------------------------------|
 * | `-column`                             | Specifies the list of columns to convert. Wildcards are allowed.|
 *
 * | Optional                               | Description                                                   |
 * |---------------------------------------|---------------------------------------------------------------|
 * | `-pipe`                               | Enables piping for input and/or output.                      |
 *
 * @subsection SR Specific Requirements
 * - For `-column`, the specified columns must exist in the source file, and must be of an integer type
 *   (e.g., SHORT, USHORT, LONG, ULONG, LONG64, ULONG64).
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
 * H. Shang, L. Emery, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "SDDSutils.h"
#include "ctype.h"

typedef enum {
  SET_COLUMN,
  SET_PIPE,
  N_OPTIONS
} OptionType;

char *option[N_OPTIONS] =
  {
    "column",
    "pipe",
  };

char *usage =
  "sddsbinarystring [<source-file>] [<target-file>]\n"
  "                 [-pipe=[input][,output]]\n"
  "                  -column=<list of column names>\n"
  "Description:\n"
  "sddsbinarystring converts integer columns into binary string representations.\n"
  "Binary string columns are appended as <oldColumnName>BinaryString.\n\n"
  "Options:\n"
  "-column   List of columns to convert. Wildcards are accepted.\n"
  "-pipe     Use pipes for input and/or output.\n\n"
  "Author: Hairong (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SDDS_DATASET sdds_dataset, sdds_orig;
  long i, j, i_arg;
  SCANNED_ARG *s_arg;
  long tmpfile_used, no_warnings, integer_columns, digits;
  int32_t column_matches;
  long index, type;
  int64_t i_row, rows;
  void *data;
  int32_t columns;
  short *integer_type;
  char *input, *output, **column_name, **column_match, **integer_column;
  char buff[1024], **binary_string;
  unsigned long pipe_flags;

  /* Initialize variables */
  tmpfile_used = 0;
  pipe_flags = 0;
  input = output = NULL;
  data = NULL;
  no_warnings = 0;

  binary_string = NULL;
  buff[0] = 0;

  columns = column_matches = integer_columns = 0;
  column_name = column_match = integer_column = NULL;
  integer_type = NULL;

  /* Register the program name for error handling */
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, usage);

  /* Parse command-line arguments */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_COLUMN:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        column_match =
          tmalloc(sizeof(*column_match) *
                  (column_matches = s_arg[i_arg].n_items - 1));
        for (i = 0; i < column_matches; i++)
          column_match[i] = s_arg[i_arg].list[i + 1];
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1,
                               s_arg[i_arg].n_items - 1, &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      default:
        fprintf(stderr, "Error (%s): unknown switch: %s\n",
                argv[0], s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  /* Process input and output file names */
  processFilenames("sddsbinarystring", &input, &output, pipe_flags,
                   no_warnings, &tmpfile_used);

#ifdef DEBUG
  fprintf(stderr, "Initializing input and output files.\n");
#endif
  /* Initialize the SDDS input */
  if (!SDDS_InitializeInput(&sdds_orig, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  /* Retrieve column names based on user input */
  if (!column_match)
    column_name = SDDS_GetColumnNames(&sdds_orig, &columns);
  else {
    column_name =
      getMatchingSDDSNames(&sdds_orig, column_match, column_matches,
                           &columns, SDDS_MATCH_COLUMN);
    free(column_match);
  }

  /* Identify integer columns for conversion */
  for (i = 0; i < columns; i++) {
    index = SDDS_GetColumnIndex(&sdds_orig, column_name[i]);
    type = SDDS_GetColumnType(&sdds_orig, index);
    if (type == SDDS_SHORT || type == SDDS_USHORT || type == SDDS_LONG || type == SDDS_ULONG || type == SDDS_LONG64 || type == SDDS_ULONG64) {
      integer_column =
        SDDS_Realloc(integer_column,
                     sizeof(*integer_column) * (integer_columns + 1));
      integer_type =
        SDDS_Realloc(integer_type,
                     sizeof(*integer_type) * (integer_columns + 1));
      integer_column[integer_columns] = NULL;

      SDDS_CopyString(&integer_column[integer_columns], column_name[i]);
      integer_type[integer_columns] = type;
      integer_columns++;
    }
  }
  SDDS_FreeStringArray(column_name, columns);
  free(column_name);

  /* Exit if no integer columns are found */
  if (!integer_columns) {
    fprintf(stderr, "There are no integer columns in %s for converting.\n",
            input);
    exit(1);
  }

  /* Initialize the SDDS output */
  if (!SDDS_InitializeCopy(&sdds_dataset, &sdds_orig, output, "w")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  /* Define new columns for binary strings */
  for (i = 0; i < integer_columns; i++) {
    sprintf(buff, "%sBinaryString", integer_column[i]);
    if (!SDDS_DefineSimpleColumn(&sdds_dataset, buff, NULL, SDDS_STRING))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_WriteLayout(&sdds_dataset))
    SDDS_PrintErrors(stderr,
                     SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

#ifdef DEBUG
  fprintf(stderr, "Reading integer data from input file.\n");
#endif
  /* Read and process each page */
  while (SDDS_ReadPage(&sdds_orig) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&sdds_orig)) < 0)
      continue;
    binary_string = malloc(sizeof(*binary_string) * rows);
    if (!SDDS_CopyPage(&sdds_dataset, &sdds_orig))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    /* Process each integer column */
    for (i = 0; i < integer_columns; i++) {
      if (!(data = SDDS_GetInternalColumn(&sdds_orig, integer_column[i])))
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      /* Determine the number of bits for binary conversion */
      if (integer_type[i] == SDDS_SHORT || integer_type[i] == SDDS_USHORT)
        digits = 16;
      else if (integer_type[i] == SDDS_LONG || integer_type[i] == SDDS_ULONG)
        digits = 32;
      else
        digits = 64;

      /* Convert each value to binary string */
      for (i_row = 0; i_row < rows; i_row++) {
        binary_string[i_row] = NULL;
        binary_string[i_row] =
          (char *)malloc(sizeof(**binary_string) * (digits + 1));

        for (j = 0; j < digits; j++) {
          if (integer_type[i] == SDDS_SHORT)
            binary_string[i_row][digits - 1 - j] = (((short*)data)[i_row] >> j & 0x1) ? '1' : '0';
          else if (integer_type[i] == SDDS_USHORT)
            binary_string[i_row][digits - 1 - j] = (((unsigned short*)data)[i_row] >> j & 0x1) ? '1' : '0';
          else if (integer_type[i] == SDDS_LONG)
            binary_string[i_row][digits - 1 - j] = (((int32_t*)data)[i_row] >> j & 0x1) ? '1' : '0';
          else if (integer_type[i] == SDDS_ULONG)
            binary_string[i_row][digits - 1 - j] = (((uint32_t*)data)[i_row] >> j & 0x1) ? '1' : '0';
          else if (integer_type[i] == SDDS_LONG64)
            binary_string[i_row][digits - 1 - j] = (((int64_t*)data)[i_row] >> j & 0x1) ? '1' : '0';
          else if (integer_type[i] == SDDS_ULONG64)
            binary_string[i_row][digits - 1 - j] = (((uint64_t*)data)[i_row] >> j & 0x1) ? '1' : '0';
        }
        binary_string[i_row][digits] = 0;
      }

      /* Add binary string column to the dataset */
      sprintf(buff, "%sBinaryString", integer_column[i]);
      if (!SDDS_SetColumn(&sdds_dataset, SDDS_BY_NAME, binary_string,
                          rows, buff))
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_FreeStringArray(binary_string, rows);
    }
    free(binary_string);
    binary_string = NULL;

    /* Write the processed page */
    if (!SDDS_WritePage(&sdds_dataset))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /* Finalize and clean up */
  if (!SDDS_Terminate(&sdds_orig) || !SDDS_Terminate(&sdds_dataset))
    SDDS_PrintErrors(stderr,
                     SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (tmpfile_used && !replaceFileAndBackUp(input, output))
    exit(1);
  SDDS_FreeStringArray(integer_column, integer_columns);
  free(integer_column);
  free_scanargs(&s_arg, argc);
  return 0;
}
