/**
 * @file sddssortcolumn.c
 * @brief Rearranges the columns of an SDDS input file into a specified order.
 *
 * @details
 * This program processes an SDDS file, allowing users to reorder its columns based on:
 * - A specified list.
 * - The order of BPMs in a storage ring.
 * - Data from an external file.
 *
 * Sorting options include increasing or decreasing order. The output is written back to an SDDS file. 
 * The tool is useful for preparing SDDS data for analysis or presentation.
 *
 * @section Usage
 * ```
 * sddssortcolumn [<SDDSinput>] [<SDDSoutput>]
 *                [-pipe=[input][,output]]
 *                [-sortList=<list_of_columns>]
 *                [-decreasing]
 *                [-bpmOrder]
 *                [-sortWith=<filename>,column=<column_name>]
 * ```
 *
 * @section Options
 * | Optional     | Description                                                                 |
 * |--------------|-----------------------------------------------------------------------------|
 * | `-pipe`      | Uses pipes for input/output data flow.                                      |
 * | `-sortList`  | A comma-separated list specifying the desired column order.                |
 * | `-decreasing`| Sorts columns in decreasing order. By default, columns are sorted in increasing order. |
 * | `-bpmOrder`  | Orders columns by their BPM position in a storage ring.                    |
 * | `-sortWith`  | Specifies an external file and column for sorting. Overrides other sort orders. |
 *
 * @subsection Incompatibilities
 * - Only one of the following may be specified:
 *   - `-sortWith`
 *   - `-bpmOrder`
 *   - `-sortList`
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
 * H. Shang, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#if defined(_WIN32)
#  include <process.h>
#  define pid_t int
#else
#  if defined(linux)
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

/* Enumeration for option types */
typedef enum {
  SET_PIPE,
  SET_SORTLIST,
  SET_DECREASING,
  SET_BPMORDER,
  SET_SORTWITH,
  N_OPTIONS
} option_type;

static char *option[N_OPTIONS] = {
  "pipe",
  "sortList",
  "decreasing",
  "bpmOrder",
  "sortWith",
};

static char *usage =
  "Usage:\n"
  "  sddssortcolumn [<SDDSinput>] [<SDDSoutput>]\n"
  "                [-pipe=[input][,output]]\n"
  "                [-sortList=<list of columns in order>]\n"
  "                [-decreasing]\n"
  "                [-bpmOrder]\n"
  "                [-sortWith=<filename>,column=<string>]\n\n"
  "Options:\n"
  "  -sortList <list of columns>\n"
  "        Specify the order of column names in a list.\n\n"
  "  -sortWith=<filename>,column=<string>\n"
  "        Sort the columns of the input based on the order defined in the\n"
  "        specified <column> of <filename>. This option overrides any other sorting order.\n\n"
  "  -bpmOrder\n"
  "        Sort the columns by their assumed BPM position in the storage ring.\n\n"
  "  -decreasing\n"
  "        Sort the columns in decreasing order. The default is increasing order.\n\n"
  "Description:\n"
  "  Rearrange the columns of an SDDS input file into the specified order.\n\n"
  "Program Information:\n"
  "  Program by Hairong Shang. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static char **column_name;
static long increasing = 1, bpm_order = 0;
static int32_t columns;

static long get_bpm_suborder(char *bpm_name) {
  long suborder;

  if (wild_match(bpm_name, "*A:P0*"))
    suborder = 1;
  else if (wild_match(bpm_name, "*A:P1*"))
    suborder = 2;
  else if (wild_match(bpm_name, "*A:P2*"))
    suborder = 3;
  else if (wild_match(bpm_name, "*A:P3*"))
    suborder = 4;
  else if (wild_match(bpm_name, "*A:P4*"))
    suborder = 5;
  else if (wild_match(bpm_name, "*A:P5*"))
    suborder = 6;
  else if (wild_match(bpm_name, "*B:P5*"))
    suborder = 7;
  else if (wild_match(bpm_name, "*B:P4*"))
    suborder = 8;
  else if (wild_match(bpm_name, "*B:P3*"))
    suborder = 9;
  else if (wild_match(bpm_name, "*B:P2*"))
    suborder = 10;
  else if (wild_match(bpm_name, "*B:P1*"))
    suborder = 11;
  else if (wild_match(bpm_name, "*B:P0*"))
    suborder = 12;
  else if (wild_match(bpm_name, "*C:P0*"))
    suborder = 13;
  else if (wild_match(bpm_name, "*BM:P1*"))
    suborder = 14;
  else if (wild_match(bpm_name, "*BM:P2*"))
    suborder = 15;
  else if (wild_match(bpm_name, "*ID:P1*"))
    suborder = 16;
  else if (wild_match(bpm_name, "*ID:P2*"))
    suborder = 17;
  else
    suborder = 18;

  return suborder;
}

static int compare_strings(const void *vindex1, const void *vindex2) {
  long index1 = *(long *)vindex1;
  long index2 = *(long *)vindex2;
  long comparison, sector1, sector2, subsector1, subsector2;

  if (bpm_order) {
    if (sscanf(column_name[index1], "S%ld", &sector1) != 1)
      sector1 = 0;
    if (sscanf(column_name[index2], "S%ld", &sector2) != 1)
      sector2 = 0;
    if (sector1 == 0 && sector2 == 0) {
      comparison = strcmp(column_name[index1], column_name[index2]);
    } else {
      if (sector1 > sector2)
        comparison = 1;
      else if (sector1 < sector2)
        comparison = -1;
      else {
        subsector1 = get_bpm_suborder(column_name[index1]);
        subsector2 = get_bpm_suborder(column_name[index2]);
        if (subsector1 > subsector2)
          comparison = 1;
        else if (subsector1 < subsector2)
          comparison = -1;
        else
          comparison = 0;
      }
    }
  } else {
    comparison = strcmp(column_name[index1], column_name[index2]);
  }

  if (!increasing)
    comparison = -comparison;

  return comparison;
}

int main(int argc, char **argv) {
  SDDS_DATASET sdds_input, sdds_output, sdds_sort;
  char *input = NULL, *output = NULL;
  char **sort_list = NULL, **parameter_name = NULL;
  char *sort_file = NULL, *sort_column = NULL, **sorted_column = NULL;
  long i_arg, *sort_column_index = NULL, index;
  int64_t i, sort_lists = 0, rows, count;
  SCANNED_ARG *s_arg;
  long tmpfile_used = 0;
  int32_t parameters = 0;
  unsigned long pipe_flags = 0, dummyflags;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s", usage);
    return EXIT_FAILURE;
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags)) {
          fprintf(stderr, "Invalid -pipe syntax\n");
          return EXIT_FAILURE;
        }
        break;
      case SET_DECREASING:
        increasing = 0;
        break;
      case SET_BPMORDER:
        bpm_order = 1;
        break;
      case SET_SORTLIST:
        sort_lists = s_arg[i_arg].n_items - 1;
        sort_list = malloc(sizeof(*sort_list) * sort_lists);
        for (i = 0; i < sort_lists; i++) {
          SDDS_CopyString(&sort_list[i], s_arg[i_arg].list[i + 1]);
        }
        break;
      case SET_SORTWITH:
        if (s_arg[i_arg].n_items != 3) {
          fprintf(stderr, "Invalid -sortWith option given!\n");
          return EXIT_FAILURE;
        }
        sort_file = s_arg[i_arg].list[1];
        s_arg[i_arg].n_items = 1;
        if (!scanItemList(&dummyflags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "column", SDDS_STRING, &sort_column, 1, 0, NULL) ||
            !sort_column) {
          fprintf(stderr, "Invalid -sortWith syntax/values\n");
          return EXIT_FAILURE;
        }
        s_arg[i_arg].n_items = 3;
        break;
      default:
        fprintf(stderr, "Error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        return EXIT_FAILURE;
      }
    } else {
      if (input == NULL) {
        input = s_arg[i_arg].list[0];
      } else if (output == NULL) {
        output = s_arg[i_arg].list[0];
      } else {
        fprintf(stderr, "Too many filenames\n");
        return EXIT_FAILURE;
      }
    }
  }

  processFilenames("sddssort", &input, &output, pipe_flags, 0, &tmpfile_used);

  if (!SDDS_InitializeInput(&sdds_input, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_InitializeOutput(&sdds_output, SDDS_BINARY, 1, NULL, NULL, output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  column_name = (char **)SDDS_GetColumnNames(&sdds_input, &columns);
  parameter_name = (char **)SDDS_GetParameterNames(&sdds_input, &parameters);

  for (i = 0; i < parameters; i++) {
    if (!SDDS_TransferParameterDefinition(&sdds_output, &sdds_input, parameter_name[i], parameter_name[i])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  sort_column_index = malloc(sizeof(*sort_column_index) * columns);
  for (i = 0; i < columns; i++) {
    sort_column_index[i] = i;
  }

  if (sort_file && sort_column) {
    if (sort_list && sort_lists) {
      for (i = 0; i < sort_lists; i++) {
        free(sort_list[i]);
      }
      free(sort_list);
    }
    sort_lists = 0;
    sort_list = NULL;

    if (!SDDS_InitializeInput(&sdds_sort, sort_file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (SDDS_ReadPage(&sdds_sort) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!(sort_lists = SDDS_CountRowsOfInterest(&sdds_sort))) {
      fprintf(stderr, "Zero rows found in sortWith file.\n");
      return EXIT_FAILURE;
    }

    if (!(sort_list = (char **)SDDS_GetColumn(&sdds_sort, sort_column))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!SDDS_Terminate(&sdds_sort)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  sorted_column = malloc(sizeof(*sorted_column) * columns);
  count = 0;

  if (sort_list) {
    for (i = 0; i < sort_lists; i++) {
      if ((index = match_string(sort_list[i], column_name, columns, EXACT_MATCH)) >= 0) {
        sorted_column[count] = sort_list[i];
        count++;
      }
    }

    for (i = 0; i < columns; i++) {
      if (match_string(column_name[i], sort_list, sort_lists, EXACT_MATCH) < 0) {
        sorted_column[count] = column_name[i];
        count++;
      }
    }
  } else {
    qsort((void *)sort_column_index, columns, sizeof(*sort_column_index), compare_strings);
    for (i = 0; i < columns; i++) {
      index = sort_column_index[i];
      sorted_column[i] = column_name[index];
    }
  }

  for (i = 0; i < columns; i++) {
    if (!SDDS_TransferColumnDefinition(&sdds_output, &sdds_input, sorted_column[i], sorted_column[i])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_WriteLayout(&sdds_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  }

  while (SDDS_ReadPage(&sdds_input) > 0) {
    rows = SDDS_CountRowsOfInterest(&sdds_input);
    if (!SDDS_StartPage(&sdds_output, rows)) {
      fprintf(stderr, "Problem starting output page\n");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!SDDS_CopyParameters(&sdds_output, &sdds_input)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!SDDS_CopyColumns(&sdds_output, &sdds_input)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!SDDS_WritePage(&sdds_output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_Terminate(&sdds_input) || !SDDS_Terminate(&sdds_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (tmpfile_used && !replaceFileAndBackUp(input, output)) {
    return EXIT_FAILURE;
  }

  if (parameters) {
    for (i = 0; i < parameters; i++) {
      free(parameter_name[i]);
    }
    free(parameter_name);
  }

  if (columns) {
    for (i = 0; i < columns; i++) {
      free(column_name[i]);
    }
    free(column_name);
  }

  if (sort_lists) {
    for (i = 0; i < sort_lists; i++) {
      free(sort_list[i]);
    }
    free(sort_list);
  }

  if (sort_column) {
    free(sort_column);
  }

  free(sorted_column);
  free(sort_column_index);
  free_scanargs(&s_arg, argc);

  return EXIT_SUCCESS;
}
