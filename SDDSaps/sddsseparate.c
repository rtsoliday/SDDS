/**
 * @file sddsseparate.c
 * @brief Reorganizes column data from an SDDS file onto separate pages.
 *
 * @details
 * This program reads an SDDS (Self Describing Data Set) file and reorganizes the column data
 * such that data from different columns ends up on separate pages. The user can specify groups
 * of columns to be combined under a new name, or copy columns across pages.
 *
 * @section Usage
 * ```
 * sddsseparate [<inputfile>] [<outputfile>]
 *              [-pipe=[input][,output]]
 *              [-group=<newName>,<listOfOldNames>]
 *              [-copy=<listOfNames>]
 * ```
 *
 * @section Options
 * | Optional | Description |
 * |----------|-------------|
 * | `-pipe`  | Enables piping for input and/or output. |
 * | `-group` | Groups multiple old column names under a new column name. Data from the old columns will appear on sequential pages under the new name. |
 * | `-copy`  | Specifies columns to be copied across all pages. |
 *
 * @subsection Incompatibilities
 *   - The same column cannot appear in both `-group` and `-copy` lists.
 *
 * @subsection spec_req Specific Requirements
 *   - For `-group`:
 *     - All columns in a group must have consistent data types.
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
 *   - M. Borland
 *   - R. Soliday
 *   - H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Structure to define groups for column processing */
typedef struct {
  char *new_name;
  char **users_old_name;
  char **old_name;
  char *parameter_name;
  long users_old_names;
  int32_t old_names;
} groups_t;

/* Enumeration for option types for better readability */
typedef enum {
  SET_GROUP,
  SET_COPY,
  SET_PIPE,
  N_OPTIONS
} option_type_t;

/* List of option strings for argument matching */
static char *option_strings[N_OPTIONS] = {
  "group",
  "copy",
  "pipe",
};

/* Usage message for user reference */
static char *usage_message =
  "sddsseparate [<inputfile>] [<outputfile>]\n"
  "             [-pipe=[input][,output]]\n"
  "             [-group=<newName>,<listOfOldNames>]\n"
  "             [-copy=<listOfNames>]\n"
  "Description:\n"
  "  Reorganizes the column data in the input so that data from different\n"
  "  columns ends up on different pages.\n"
  "  For each -group option, a column is created in the output that contains\n"
  "  data from the columns <listOfOldNames> on sequential pages.\n"
  "  Columns named with the -copy option are duplicated on each page.\n"
  "\n"
  "Examples:\n"
  "  Group columns A, B, C under a new name 'Group1' and copy column D:\n"
  "    sddsseparate input.sdds output.sdds -group=Group1,A,B,C -copy=D\n"
  "\n"
  "Program by Michael Borland.\n"
  "(" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  char *input = NULL;
  char *output = NULL;
  char **copy_column_name = NULL;
  char **users_copy_column_name = NULL;
  groups_t *group = NULL;
  int32_t copy_columns = 0;
  long users_copy_columns = 0;
  long groups_count = 0;
  long i_arg, i, read_code, items;
  int64_t rows;
  unsigned long pipe_flags = 0;
  SCANNED_ARG *sc_arg = NULL;
  SDDS_DATASET sdds_in, sdds_out;

  /* Register the program name for error reporting */
  SDDS_RegisterProgramName(argv[0]);

  /* Parse command-line arguments */
  argc = scanargs(&sc_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, usage_message);

  /* Initialize variables */
  group = NULL;
  copy_column_name = users_copy_column_name = NULL;
  users_copy_columns = copy_columns = groups_count = 0;

  /* Process arguments */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (sc_arg[i_arg].arg_type == OPTION) {
      /* Match options and process accordingly */
      switch (match_string(sc_arg[i_arg].list[0], option_strings, N_OPTIONS, 0)) {
      case SET_PIPE:
        /* Handle pipe options */
        if (!processPipeOption(sc_arg[i_arg].list + 1, sc_arg[i_arg].n_items - 1, &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;

      case SET_GROUP:
        /* Handle group options */
        items = sc_arg[i_arg].n_items - 1;
        if (items < 2)
          SDDS_Bomb("invalid -group syntax");
        group = SDDS_Realloc(group, sizeof(*group) * (groups_count + 1));
        if (!group ||
            !SDDS_CopyString(&group[groups_count].new_name, sc_arg[i_arg].list[1]) ||
            !(group[groups_count].users_old_name = SDDS_Malloc(sizeof(*group[groups_count].users_old_name) * (group[groups_count].users_old_names = items - 1))) ||
            !SDDS_CopyStringArray(group[groups_count].users_old_name, sc_arg[i_arg].list + 2, group[groups_count].users_old_names))
          SDDS_Bomb("memory allocation failure");
        group[groups_count].old_name = NULL;
        group[groups_count].old_names = 0;
        groups_count++;
        break;

      case SET_COPY:
        /* Handle copy options */
        if (users_copy_columns)
          SDDS_Bomb("give -copy only once");
        users_copy_columns = sc_arg[i_arg].n_items - 1;
        if (users_copy_columns < 1)
          SDDS_Bomb("invalid -copy syntax");
        users_copy_column_name = SDDS_Malloc(sizeof(*users_copy_column_name) * users_copy_columns);
        if (!users_copy_column_name ||
            !SDDS_CopyStringArray(users_copy_column_name, sc_arg[i_arg].list + 1, users_copy_columns))
          SDDS_Bomb("memory allocation failure");
        break;

      default:
        /* Handle unknown options */
        fprintf(stderr, "error: unknown/ambiguous option: %s\n", sc_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      /* Process input and output filenames */
      if (!input)
        input = sc_arg[i_arg].list[0];
      else if (!output)
        output = sc_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  if (groups_count == 0)
    SDDS_Bomb("no groups defined");

  /* Process filenames and open input file */
  processFilenames("sddsseparate", &input, &output, pipe_flags, 0, NULL);

  if (!SDDS_InitializeInput(&sdds_in, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  /* Process copy columns */
  if (users_copy_columns) {
    SDDS_SetColumnFlags(&sdds_in, 0);
    for (i = 0; i < users_copy_columns; i++)
      SDDS_SetColumnsOfInterest(&sdds_in, SDDS_MATCH_STRING, users_copy_column_name[i], SDDS_OR);
    copy_column_name = SDDS_GetColumnNames(&sdds_in, &copy_columns);
    if (!copy_column_name || copy_columns == 0)
      SDDS_Bomb("no match for copy columns");
  }

  /* Process groups */
  for (i = 0; i < groups_count; i++) {
    long j, type = 0;
    SDDS_SetColumnFlags(&sdds_in, 0);

    for (j = 0; j < group[i].users_old_names; j++)
      SDDS_SetColumnsOfInterest(&sdds_in, SDDS_MATCH_STRING, group[i].users_old_name[j], SDDS_OR);

    group[i].old_name = SDDS_GetColumnNames(&sdds_in, &group[i].old_names);
    if (!group[i].old_name) {
      fprintf(stderr, "No match for group %s (sddsseparate)\n", group[i].new_name);
      exit(EXIT_FAILURE);
    }

    if (i > 0 && group[i - 1].old_names != group[i].old_names) {
      fprintf(stderr, "Group %s comprises %" PRId32 " columns, whereas the last group comprises %" PRId32 " (sddsseparate)\n",
              group[i].new_name, group[i].old_names, group[i - 1].old_names);
      exit(EXIT_FAILURE);
    }

    type = SDDS_GetColumnType(&sdds_in, SDDS_GetColumnIndex(&sdds_in, group[i].old_name[0]));
    for (j = 1; j < group[i].old_names; j++) {
      if (type != SDDS_GetColumnType(&sdds_in, SDDS_GetColumnIndex(&sdds_in, group[i].old_name[j]))) {
        fprintf(stderr, "Inconsistent data types in group %s (sddsseparate)\n", group[i].new_name);
        fprintf(stderr, "First inconsistent column is %s\n", group[i].old_name[j]);
        exit(EXIT_FAILURE);
      }
    }
  }

  /* Initialize output file */
  if (!SDDS_InitializeOutput(&sdds_out, SDDS_BINARY, 0, NULL, NULL, output) ||
      !SDDS_TransferAllParameterDefinitions(&sdds_out, &sdds_in, 0))
    SDDS_Bomb("problem initializing output file");

  for (i = 0; i < copy_columns; i++) {
    if (!SDDS_TransferColumnDefinition(&sdds_out, &sdds_in, copy_column_name[i], NULL))
      SDDS_Bomb("problem transferring copy column definitions to output file");
  }

  for (i = 0; i < groups_count; i++) {
    char *name;
    if (!SDDS_TransferColumnDefinition(&sdds_out, &sdds_in, group[i].old_name[0], group[i].new_name)) {
      fprintf(stderr, "Problem transferring column %s as %s to output file (sddsseparate)\n",
              group[i].old_name[0], group[i].new_name);
      exit(EXIT_FAILURE);
    }

    group[i].parameter_name = SDDS_Malloc(sizeof(*name) * (strlen(group[i].new_name) + 100));
    if (!group[i].parameter_name)
      SDDS_Bomb("memory allocation failure");

    sprintf(group[i].parameter_name, "%sSourceColumn", group[i].new_name);
    if (!SDDS_DefineSimpleParameter(&sdds_out, group[i].parameter_name, NULL, SDDS_STRING))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_WriteLayout(&sdds_out))
    SDDS_Bomb("problem writing layout to output file");

  /* Process each page */
  while ((read_code = SDDS_ReadPage(&sdds_in)) > 0) {
    rows = SDDS_CountRowsOfInterest(&sdds_in);
    if (rows < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (rows == 0)
      continue;

    for (i = 0; i < group[0].old_names; i++) {
      long ic, ig;

      if (!SDDS_StartPage(&sdds_out, rows) ||
          !SDDS_CopyParameters(&sdds_out, &sdds_in))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      for (ic = 0; ic < copy_columns; ic++) {
        void *data = SDDS_GetInternalColumn(&sdds_in, copy_column_name[ic]);
        if (!data ||
            !SDDS_SetColumn(&sdds_out, SDDS_SET_BY_NAME, data, rows, copy_column_name[ic]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      for (ig = 0; ig < groups_count; ig++) {
        void *data;

        if (!SDDS_SetParameters(&sdds_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                group[ig].parameter_name, group[ig].old_name[i], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

        data = SDDS_GetInternalColumn(&sdds_in, group[ig].old_name[i]);
        if (!data ||
            !SDDS_SetColumn(&sdds_out, SDDS_SET_BY_NAME, data, rows, group[ig].new_name))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      if (!SDDS_WritePage(&sdds_out))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  /* Terminate input and output files */
  if (!SDDS_Terminate(&sdds_in)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (!SDDS_Terminate(&sdds_out)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  return 0;
}
