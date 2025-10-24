/**
 * @file sddsselect.c
 * @brief Creates an SDDS data set from another data set based on matching data in a third data set.
 *
 * @details
 * This program selects rows from `<input1>` that have (or do not have, if inverted) a matching entry in `<input2>`,
 * based on specified matching or equating columns. The output is written to `<output>`, or `<input1>` is replaced 
 * if `<output>` is not provided. It supports various options to customize the matching behavior, data processing, 
 * and output format.
 *
 * @section Usage
 * ```
 * sddsselect [<input1>] <input2> [<output>]
 *            [-pipe[=input][,output]]
 *            [-match=<column-name>[=<column-name>]]
 *            [-equate=<column-name>[=<column-name>]] 
 *            [-hashLookup]
 *            [-invert]
 *            [-reuse[=rows][,page]] 
 *            [-majorOrder=row|column]
 *            [-nowarnings]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use pipe for input and/or output.                                                     |
 * | `-match`                              | Specify columns to match between `<input1>` and `<input2>`.                           |
 * | `-equate`                             | Specify columns to equate between `<input1>` and `<input2>`.                          |
 * | `-hashLookup`                         | Use a hash table for matching instead of sorted key lists.                            |
 * | `-invert`                             | Invert the selection to keep non-matching rows.                                       |
 * | `-reuse`                              | Allow reusing rows or specify page reuse.                                             |
 * | `-majorOrder`                         | Set the output file to row or column major order.                                     |
 * | `-nowarnings`                         | Suppress warning messages.                                                            |
 *
 * @subsection Incompatibilities
 *   - Only one of `-match` or `-equate` may be specified.
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
#include "SDDSaps.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_MATCH_COLUMN,
  SET_EQUATE_COLUMN,
  SET_NOWARNINGS,
  SET_INVERT,
  SET_REUSE,
  SET_PIPE,
  SET_MAJOR_ORDER,
  SET_HASH_LOOKUP,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "match",
  "equate",
  "nowarnings",
  "invert",
  "reuse",
  "pipe",
  "majorOrder",
  "hashlookup",
};

char *USAGE = "\n\
sddsselect [<input1>] <input2> [<output>]\n\
           [-pipe[=input][,output]]\n\
           [-match=<column-name>[=<column-name>]]\n\
           [-equate=<column-name>[=<column-name>]] \n\
           [-hashLookup]\n\
           [-invert]\n\
           [-reuse[=rows][,page]] \n\
           [-majorOrder=row|column]\n\
           [-nowarnings]\n\
Options:\n\
  -pipe[=input][,output]          Use pipe for input and/or output.\n\
  -match=<column1>[=<column2>]   Specify columns to match between input1 and input2.\n\
  -equate=<column1>[=<column2>]  Specify columns to equate between input1 and input2.\n\
  -hashLookup                     Use a hash table for key lookups (non-wildcard match/equate).\n\
  -invert                         Invert the selection to keep non-matching rows.\n\
  -reuse[=rows][,page]            Allow reusing rows or specify page reuse.\n\
  -majorOrder=row|column          Set the output file to row or column major order.\n\
  -nowarnings                     Suppress warning messages.\n\
\n\
Example:\n\
  sddsselect -match=colA=colB input1.sdds input2.sdds output.sdds\n\
\n\
Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_1, SDDS_2, SDDS_output;
  long i, j, i_arg, reuse, reusePage;
  int64_t rows1, rows2, i1, i2;
  SCANNED_ARG *s_arg;
  char s[200], *ptr;
  char **match_column, **equate_column;
  long match_columns, equate_columns;
  char *input1, *input2, *output;
  long tmpfile_used, retval1, retval2;
  long warnings, invert;
  unsigned long pipeFlags, majorOrderFlag;
  KEYED_EQUIVALENT **keyGroup = NULL;
  long keyGroups = 0;
  short columnMajorOrder = -1;
  long useHashLookup = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  input1 = input2 = output = NULL;
  match_column = equate_column = NULL;
  match_columns = equate_columns = reuse = reusePage = 0;
  tmpfile_used = invert = 0;
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
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_MATCH_COLUMN:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -match syntax");
        if (match_columns != 0)
          SDDS_Bomb("only one -match option may be given");
        match_column = tmalloc(sizeof(*match_column) * 2);
        if ((ptr = strchr(s_arg[i_arg].list[1], '=')))
          *ptr++ = 0;
        else
          ptr = s_arg[i_arg].list[1];
        match_column[0] = s_arg[i_arg].list[1];
        match_column[1] = ptr;
        match_columns = 1;
        break;
      case SET_EQUATE_COLUMN:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -equate syntax");
        if (equate_columns != 0)
          SDDS_Bomb("only one -equate option may be given");
        equate_column = tmalloc(sizeof(*equate_column) * 2);
        if ((ptr = strchr(s_arg[i_arg].list[1], '=')))
          *ptr++ = 0;
        else
          ptr = s_arg[i_arg].list[1];
        equate_column[0] = s_arg[i_arg].list[1];
        equate_column[1] = ptr;
        equate_columns = 1;
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
              SDDS_Bomb("unknown reuse keyword");
              break;
            }
          }
        }
        break;
      case SET_INVERT:
        invert = 1;
        break;
      case SET_NOWARNINGS:
        warnings = 0;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_HASH_LOOKUP:
        useHashLookup = 1;
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        SDDS_Bomb(NULL);
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
        SDDS_Bomb("too many filenames");
    }
  }

  if (pipeFlags & USE_STDIN && input1) {
    if (output)
      SDDS_Bomb("too many filenames (sddsxref)");
    output = input2;
    input2 = input1;
    input1 = NULL;
  }
  processFilenames("sddsselect", &input1, &output, pipeFlags, !warnings, &tmpfile_used);
  if (!input2)
    SDDS_Bomb("second input file not specified (sddsxref)");

  if (equate_columns && match_columns)
    SDDS_Bomb("only one of -equate or -match may be given");
  if (!equate_columns && !match_columns)
    SDDS_Bomb("one of -equate or -match must be given");

  if (!SDDS_InitializeInput(&SDDS_1, input1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!SDDS_InitializeInput(&SDDS_2, input2)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (match_columns) {
    if ((j = SDDS_GetColumnIndex(&SDDS_1, match_column[0])) < 0 ||
        SDDS_GetColumnType(&SDDS_1, j) != SDDS_STRING) {
      sprintf(s, "error: column %s not found or not string type in file %s", match_column[0], input1 ? input1 : "stdin");
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if ((j = SDDS_GetColumnIndex(&SDDS_2, match_column[1])) < 0 ||
        SDDS_GetColumnType(&SDDS_2, j) != SDDS_STRING) {
      sprintf(s, "error: column %s not found or not string type in file %s", match_column[1], input2);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (equate_columns) {
    if ((j = SDDS_GetColumnIndex(&SDDS_1, equate_column[0])) < 0 ||
        !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&SDDS_1, j))) {
      sprintf(s, "error: column %s not found or not numeric type in file %s", equate_column[0], input1 ? input1 : "stdin");
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if ((j = SDDS_GetColumnIndex(&SDDS_2, equate_column[1])) < 0 ||
        !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&SDDS_2, j))) {
      sprintf(s, "error: column %s not found or not numeric type in file %s", equate_column[1], input2);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (output && pipeFlags & USE_STDOUT)
    SDDS_Bomb("too many filenames with -pipe option");
  if (!output && !(pipeFlags & USE_STDOUT)) {
    if (warnings)
      fprintf(stderr, "warning: existing file %s will be replaced (sddsselect)\n", input1 ? input1 : "stdin");
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

  while ((retval1 = SDDS_ReadPage(&SDDS_1)) > 0) {
    if (!reusePage) {
      if ((retval2 = SDDS_ReadPage(&SDDS_2)) <= 0) {
        if (warnings)
          fprintf(stderr, "warning: <input2> ends before <input1>\n");
        if (invert) {
          /* nothing to match, so everything would normally be thrown out */
          if (!SDDS_CopyPage(&SDDS_output, &SDDS_1) ||
              !SDDS_WritePage(&SDDS_output))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          continue;
        } else
          /* nothing to match, so everything thrown out */
          break;
      }
    } else {
      if (retval1 == 1 && (retval2 = SDDS_ReadPage(&SDDS_2)) <= 0)
        SDDS_Bomb("<input2> has no data");
      SDDS_SetRowFlags(&SDDS_2, 1);
    }
    rows1 = SDDS_CountRowsOfInterest(&SDDS_1);
    rows2 = SDDS_CountRowsOfInterest(&SDDS_2);

    if (!SDDS_StartPage(&SDDS_output, rows1)) {
      SDDS_SetError("Problem starting output page");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_CopyParameters(&SDDS_output, &SDDS_2) ||
        !SDDS_CopyArrays(&SDDS_output, &SDDS_2)) {
      SDDS_SetError("Problem copying parameter or array data from second input file");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_CopyParameters(&SDDS_output, &SDDS_1) ||
        !SDDS_CopyArrays(&SDDS_output, &SDDS_1)) {
      SDDS_SetError("Problem copying parameter or array data from first input file");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (rows1) {
      if (match_columns) {
        char **string1, **string2;
        long matched;
        string2 = NULL;
        if (!(string1 = SDDS_GetColumn(&SDDS_1, match_column[0]))) {
          fprintf(stderr, "Error: problem getting column %s from file %s\n", match_column[0], input1 ? input1 : "stdin");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (rows2 && !(string2 = SDDS_GetColumn(&SDDS_2, match_column[1]))) {
          fprintf(stderr, "Error: problem getting column %s from file %s\n", match_column[1], input2);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        StrHash *strHash = NULL;
        if (rows2) {
          if (useHashLookup)
            strHash = SDDS_BuildStrHash(string2, rows2);
          else
            keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_STRING, string2, rows2);
        }
        for (i1 = 0; i1 < rows1; i1++) {
          if (!SDDS_CopyRowDirect(&SDDS_output, i1, &SDDS_1, i1)) {
            sprintf(s, "Problem copying row %" PRId64 " of first data set", i1);
            SDDS_SetError(s);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          matched = 0;
          if (rows2) {
            if (useHashLookup) {
              if (SDDS_LookupStr(strHash, string1[i1], reuse) >= 0)
                matched = 1;
            } else {
              if ((i2 = FindMatchingKeyGroup(keyGroup, keyGroups, SDDS_STRING, string1 + i1, reuse)) >= 0)
                matched = 1;
            }
          }
          if ((!matched && !invert) || (matched && invert)) {
            if (!SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, i1, i1, 0))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
        if (string1) {
          for (i = 0; i < rows1; i++)
            free(string1[i]);
          free(string1);
          string1 = NULL;
        }
        if (string2) {
          for (i = 0; i < rows2; i++)
            free(string2[i]);
          free(string2);
          string2 = NULL;
        }
        if (useHashLookup && strHash) {
          SDDS_FreeStrHash(strHash);
          strHash = NULL;
        }
        for (i = 0; i < keyGroups; i++) {
          if (keyGroup[i]) {
            if (keyGroup[i]->equivalent)
              free(keyGroup[i]->equivalent);
            free(keyGroup[i]);
            keyGroup[i] = NULL;
          }
        }
        if (keyGroups) {
          free(keyGroup);
          keyGroup = NULL;
          keyGroups = 0;
        }
      } else if (equate_columns) {
        double *value1, *value2;
        long equated;
        value2 = NULL;
        if (!(value1 = SDDS_GetColumnInDoubles(&SDDS_1, equate_column[0]))) {
          fprintf(stderr, "Error: problem getting column %s from file %s\n", equate_column[0], input1 ? input1 : "stdin");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (rows2 && !(value2 = SDDS_GetColumnInDoubles(&SDDS_2, equate_column[1]))) {
          fprintf(stderr, "Error: problem getting column %s from file %s\n", equate_column[1], input2);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        NumHash *numHash = NULL;
        if (rows2) {
          if (useHashLookup)
            numHash = SDDS_BuildNumHash(value2, rows2);
          else
            keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_DOUBLE, value2, rows2);
        }
        for (i1 = 0; i1 < rows1; i1++) {
          if (!SDDS_CopyRowDirect(&SDDS_output, i1, &SDDS_1, i1)) {
            sprintf(s, "Problem copying row %" PRId64 " of first data set", i1);
            SDDS_SetError(s);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          equated = 0;
          if (rows2) {
            if (useHashLookup) {
              if (SDDS_LookupNum(numHash, value1[i1], reuse) >= 0)
                equated = 1;
            } else {
              if ((i2 = FindMatchingKeyGroup(keyGroup, keyGroups, SDDS_DOUBLE, value1 + i1, reuse)) >= 0)
                equated = 1;
            }
          }
          if ((!equated && !invert) || (equated && invert)) {
            if (!SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, i1, i1, 0))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
        if (value1)
          free(value1);
        value1 = NULL;
        if (rows2 && value2)
          free(value2);
        value2 = NULL;
        if (useHashLookup && numHash) {
          SDDS_FreeNumHash(numHash);
          numHash = NULL;
        }
        for (i = 0; i < keyGroups; i++) {
          if (keyGroup[i]) {
            if (keyGroup[i]->equivalent)
              free(keyGroup[i]->equivalent);
            free(keyGroup[i]);
            keyGroup[i] = NULL;
          }
        }
        if (keyGroups) {
          free(keyGroup);
          keyGroup = NULL;
          keyGroups = 0;
        }
      }
    }
    if (!SDDS_WritePage(&SDDS_output)) {
      SDDS_SetError("Problem writing data to output file");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_Terminate(&SDDS_1) || !SDDS_Terminate(&SDDS_2) || !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (tmpfile_used && !replaceFileAndBackUp(input1, output))
    exit(EXIT_FAILURE);
  free_scanargs(&s_arg, argc);
  if (match_columns)
    free(match_column);
  return EXIT_SUCCESS;
}
