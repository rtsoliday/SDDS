/**
 * @file sddsmselect.c
 * @brief Utility for creating SDDS datasets by matching or equating data between two datasets.
 *
 * @details
 * This utility processes SDDS (Self Describing Data Set) files to match or equate rows of data 
 * between two input datasets. It provides several options to control the selection logic, output 
 * format, and behavior of the program.
 *
 * Key features include:
 * - Matching rows based on specific column values.
 * - Equating rows based on numeric column values.
 * - Reusing rows of the second dataset for multiple matches.
 * - Inverting the selection to choose unmatched rows.
 * - Selecting output data order (row-major or column-major).
 * - Suppressing warnings for a cleaner output.
 *
 * The program integrates with the SDDS library for robust data processing and supports piping 
 * for flexible input/output handling.
 *
 * @section Usage
 * ```
 * sddsmselect [<input1>] <input2> [<output>]
 *             [-pipe[=input][,output]] 
 *             [-match=<column-name>[=<column-name>][,...]]
 *             [-equate=<column-name>[=<column-name>][,...]]
 *             [-invert]
 *             [-reuse[=rows][,page]]
 *             [-majorOrder=row|column]
 *             [-nowarnings]
 * ```
 *
 * @section Options
 * | Option              | Description                                                     |
 * |---------------------|-----------------------------------------------------------------|
 * | `-pipe`             | Use pipe for input and/or output.                              |
 * | `-match`            | Specify columns to match between datasets.                     |
 * | `-equate`           | Specify numeric columns to equate between datasets.            |
 * | `-invert`           | Select rows with no matching rows in the second dataset.       |
 * | `-reuse`            | Allow reuse of rows from the second dataset.                   |
 * | `-majorOrder`       | Set the output file order to row-major or column-major.         |
 * | `-nowarnings`       | Suppress warning messages.                                     |
 *
 * @subsection Incompatibilities
 * - `-reuse` options `rows` and `page` cannot be used together.
 * - One and only one of `-match` or `-equate` must be specified.
 * - `-invert` alters the selection logic and may conflict with specific `-reuse` settings.
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_MATCH_COLUMNS,
  SET_EQUATE_COLUMNS,
  SET_NOWARNINGS,
  SET_INVERT,
  SET_REUSE,
  SET_PIPE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

typedef char *STRING_PAIR[2];

long rows_equate(SDDS_DATASET *SDDS1, int64_t row1, SDDS_DATASET *SDDS2, int64_t row2, long equate_columns, STRING_PAIR *equate_column);

char *option[N_OPTIONS] = {
  "match", 
  "equate", 
  "nowarnings", 
  "invert", 
  "reuse", 
  "pipe", 
  "majorOrder"
};

/* Improved and more readable usage message */
char *USAGE =
  "sddsmselect [<input1>] <input2> [<output>]\n"
  "            [-pipe[=input][,output]] \n"
  "            [-match=<column-name>[=<column-name>][,...]]\n"
  "            [-equate=<column-name>[=<column-name>][,...]]\n"
  "            [-invert]\n"
  "            [-reuse[=rows][,page]]\n"
  "            [-majorOrder=row|column]\n"
  "            [-nowarnings]\n"
  "Options:\n"
  "  -pipe[=input][,output]           Use pipe for input and/or output.\n"
  "  -match=<col1>=<col2>,...         Specify columns to match between input1 and input2.\n"
  "  -equate=<col1>=<col2>,...        Specify columns to equate between input1 and input2.\n"
  "  -invert                           Select rows with no matching rows in input2.\n"
  "  -reuse[=rows|page]                Allow reuse of rows from input2.\n"
  "  -majorOrder=row|column            Set output file order to row or column major.\n"
  "  -nowarnings                       Suppress warning messages.\n"
  "\n"
  "Description:\n"
  "  sddsmselect selects data from <input1> to write to <output>\n"
  "  based on the presence or absence of matching data in <input2>.\n"
  "  If <output> is not specified, <input1> is replaced.\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_1, SDDS_2, SDDS_output;
  long i, i_arg, reuse, reusePage;
  int64_t j, k, rows1, rows2, n, outputRow;
  SCANNED_ARG *s_arg;
  char s[200], *ptr;
  STRING_PAIR *match_column, *equate_column;
  long match_columns, equate_columns;
  char *input1, *input2, *output, *match_value;
  long tmpfile_used, retval1, retval2;
  long *row_used, warnings, invert;
  unsigned long pipeFlags, majorOrderFlag;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  input1 = input2 = output = NULL;
  match_column = equate_column = NULL;
  match_columns = equate_columns = reuse = reusePage = invert = 0;
  tmpfile_used = 0;
  warnings = 1;
  pipeFlags = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items,
                           0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("Invalid -majorOrder syntax or values.");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_MATCH_COLUMNS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -match syntax.");
        match_column = trealloc(match_column, sizeof(*match_column) * (match_columns + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++) {
          if ((ptr = strchr(s_arg[i_arg].list[i], '=')))
            *ptr++ = 0;
          else
            ptr = s_arg[i_arg].list[i];
          match_column[i - 1 + match_columns][0] = s_arg[i_arg].list[i];
          match_column[i - 1 + match_columns][1] = ptr;
        }
        match_columns += s_arg[i_arg].n_items - 1;
        break;
      case SET_EQUATE_COLUMNS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -equate syntax.");
        equate_column = trealloc(equate_column, sizeof(*equate_column) * (equate_columns + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++) {
          if ((ptr = strchr(s_arg[i_arg].list[i], '=')))
            *ptr++ = 0;
          else
            ptr = s_arg[i_arg].list[i];
          equate_column[i - 1 + equate_columns][0] = s_arg[i_arg].list[i];
          equate_column[i - 1 + equate_columns][1] = ptr;
        }
        equate_columns += s_arg[i_arg].n_items - 1;
        break;
      case SET_REUSE:
        if (s_arg[i_arg].n_items == 1)
          reuse = 1;
        else {
          char *reuseOptions[2] = {"rows", "page"};
          for (i = 1; i < s_arg[i_arg].n_items; i++) {
            switch (match_string(s_arg[i_arg].list[i], reuseOptions, 2, 0)) {
            case 0:
              reuse = 1;
              break;
            case 1:
              reusePage = 1;
              break;
            default:
              SDDS_Bomb("Unknown reuse keyword.");
              break;
            }
          }
        }
        break;
      case SET_NOWARNINGS:
        warnings = 0;
        break;
      case SET_INVERT:
        invert = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax.");
        break;
      default:
        fprintf(stderr, "Error: Unknown option: %s\n", s_arg[i_arg].list[0]);
        bomb(NULL, USAGE);
        break;
      }
    } else {
      if (input1 == NULL)
        input1 = s_arg[i_arg].list[0];
      else if (input2 == NULL)
        input2 = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many filenames provided.");
    }
  }

  if (pipeFlags & USE_STDIN && input1) {
    if (output)
      SDDS_Bomb("Too many filenames with -pipe option.");
    output = input2;
    input2 = input1;
    input1 = NULL;
  }
  processFilenames("sddsmselect", &input1, &output, pipeFlags, !warnings, &tmpfile_used);
  if (!input2)
    SDDS_Bomb("Second input file not specified.");

  if (!match_columns && !equate_columns)
    SDDS_Bomb("Either -match or -equate must be specified.");

  if (!SDDS_InitializeInput(&SDDS_1, input1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!SDDS_InitializeInput(&SDDS_2, input2)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < match_columns; i++) {
    if ((j = SDDS_GetColumnIndex(&SDDS_1, match_column[i][0])) < 0 ||
        SDDS_GetColumnType(&SDDS_1, j) != SDDS_STRING) {
      sprintf(s, "Error: Column '%s' not found or not of string type in file '%s'.",
              match_column[i][0], input1 ? input1 : "stdin");
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if ((j = SDDS_GetColumnIndex(&SDDS_2, match_column[i][1])) < 0 ||
        SDDS_GetColumnType(&SDDS_2, j) != SDDS_STRING) {
      sprintf(s, "Error: Column '%s' not found or not of string type in file '%s'.",
              match_column[i][1], input2);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  for (i = 0; i < equate_columns; i++) {
    if ((j = SDDS_GetColumnIndex(&SDDS_1, equate_column[i][0])) < 0 ||
        !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&SDDS_1, j))) {
      sprintf(s, "Error: Column '%s' not found or not of numeric type in file '%s'.",
              equate_column[i][0], input1 ? input1 : "stdin");
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if ((j = SDDS_GetColumnIndex(&SDDS_2, equate_column[i][1])) < 0 ||
        !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&SDDS_2, j))) {
      sprintf(s, "Error: Column '%s' not found or not of numeric type in file '%s'.",
              equate_column[i][1], input2);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (output && (pipeFlags & USE_STDOUT))
    SDDS_Bomb("Too many filenames with -pipe option.");
  if (!output && !(pipeFlags & USE_STDOUT)) {
    if (warnings)
      fprintf(stderr, "Warning: Existing file '%s' will be replaced.\n", input1 ? input1 : "stdin");
    tmpfile_used = 1;
    cp_str(&output, tmpname(NULL));
  }
  if (!SDDS_InitializeCopy(&SDDS_output, &SDDS_1, output, "w")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (columnMajorOrder != -1)
    SDDS_output.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDS_output.layout.data_mode.column_major = SDDS_1.layout.data_mode.column_major;
  if (!SDDS_WriteLayout(&SDDS_output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  row_used = NULL;
  while ((retval1 = SDDS_ReadPage(&SDDS_1)) > 0) {
    if (!reusePage) {
      if ((retval2 = SDDS_ReadPage(&SDDS_2)) <= 0) {
        if (warnings)
          fprintf(stderr, "Warning: <input2> ends before <input1>.\n");
        if (invert) {
          /* Nothing to match, so everything would normally be thrown out */
          if (!SDDS_CopyPage(&SDDS_output, &SDDS_1) ||
              !SDDS_WritePage(&SDDS_output))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          continue;
        } else
          /* Nothing to match, so everything thrown out */
          break;
      }
    } else {
      if (retval1 == 1 && (retval2 = SDDS_ReadPage(&SDDS_2)) <= 0)
        SDDS_Bomb("<input2> has no data.");
      SDDS_SetRowFlags(&SDDS_2, 1);
    }
    SDDS_SetRowFlags(&SDDS_1, 1);
    rows1 = SDDS_CountRowsOfInterest(&SDDS_1);
    if ((rows2 = SDDS_CountRowsOfInterest(&SDDS_2))) {
      row_used = SDDS_Realloc(row_used, sizeof(*row_used) * rows2);
      SDDS_ZeroMemory(row_used, rows2 * sizeof(*row_used));
    }
    if (!SDDS_StartPage(&SDDS_output, rows1)) {
      SDDS_SetError("Problem starting output page.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_CopyParameters(&SDDS_output, &SDDS_1) ||
        !SDDS_CopyArrays(&SDDS_output, &SDDS_1)) {
      SDDS_SetError("Problem copying parameter or array data from first input file.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    outputRow = 0;
    for (j = 0; j < rows1; j++) {
      /* Set up to match all rows of file 2 to row j of file 1 */
      SDDS_SetRowFlags(&SDDS_2, 1);
      for (i = 0; i < match_columns; i++) {
        if (!SDDS_GetValue(&SDDS_1, match_column[i][0], j, &match_value)) {
          sprintf(s, "Problem getting column '%s' from file '%s'.",
                  match_column[i][0], input1 ? input1 : "stdin");
          SDDS_SetError(s);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (SDDS_MatchRowsOfInterest(&SDDS_2, match_column[i][1], match_value, SDDS_AND) < 0) {
          sprintf(s, "Problem setting rows of interest for column '%s'.",
                  match_column[i][1]);
          SDDS_SetError(s);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      n = SDDS_CountRowsOfInterest(&SDDS_2);
      if ((!n && !invert) || (n && invert))
        /* No match in file2 for row j of file1, or unwanted match found--so don't copy it */
        continue;
      for (k = 0; k < rows2; k++) {
        if (SDDS_GetRowFlag(&SDDS_2, k) < 0)
          /* Test if row k of file2 passed string-matches. If not, go to next row */
          continue;
        /* If row k of file2 is not already used, then test it for a match to row j of file1.
           If no -equate options were given, this test is always true.
        */
        if (!row_used[k]) {
          long equal;
          equal = rows_equate(&SDDS_1, j, &SDDS_2, k, equate_columns, equate_column);
          if ((equal && !invert) || (!equal && invert)) {
            row_used[k] = reuse ? 0 : 1;
            break;
          }
        }
      }
      if ((k == rows2 && !invert) || (k != rows2 && invert))
        /* No match in file2 for row j of file1, or unwanted match found--so don't copy it */
        continue;
      if (!SDDS_CopyRowDirect(&SDDS_output, outputRow, &SDDS_1, j)) {
        sprintf(s, "Problem copying to row %" PRId64 " of output from row %" PRId64 " of data set 1.",
                outputRow, j);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      outputRow++;
    }
    if (!SDDS_WritePage(&SDDS_output)) {
      SDDS_SetError("Problem writing data to output file.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_Terminate(&SDDS_1) ||
      !SDDS_Terminate(&SDDS_2) ||
      !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (tmpfile_used && !replaceFileAndBackUp(input1, output))
    exit(EXIT_FAILURE);

  return EXIT_SUCCESS;
}

long rows_equate(SDDS_DATASET *SDDS1, int64_t row1, SDDS_DATASET *SDDS2, int64_t row2, long equate_columns, STRING_PAIR *equate_column) {
  char *data1, *data2;
  long index1, index2, size, type, i;
  char s[SDDS_MAXLINE];
  index2 = 0;
  for (i = 0; i < equate_columns; i++) {
    if ((index1 = SDDS_GetColumnIndex(SDDS1, equate_column[i][0])) < 0 ||
        (index2 = SDDS_GetColumnIndex(SDDS2, equate_column[i][1])) < 0) {
      SDDS_SetError("Problem equating rows.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if ((type = SDDS_GetColumnType(SDDS1, index1)) != SDDS_GetColumnType(SDDS2, index2)) {
      sprintf(s, "Problem equating rows--types don't match for columns '%s' and '%s'.",
              equate_column[i][0], equate_column[i][1]);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    size = SDDS_GetTypeSize(type);
    data1 = (char *)SDDS1->data[index1] + size * row1;
    data2 = (char *)SDDS2->data[index2] + size * row2;
    if (memcmp(data1, data2, size) != 0)
      return 0;
  }
  return 1;
}
