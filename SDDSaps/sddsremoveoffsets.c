/**
 * @file sddsremoveoffsets.c
 * @brief A program to remove offsets from BPM waveform data in SDDS files.
 *
 * @details
 * This program processes SDDS files to remove offsets in specified data columns, ensuring the resulting
 * data has a zero average. Supports advanced options for output format, column selection, and commutation modes.
 *
 * @section Usage
 * ```
 * sddsremoveoffsets [<input-file>] [<output-file>]
 *                   [-pipe=[input],[output]] 
 *                    -columns=<name> 
 *                   [-commutationMode=<string>] 
 *                   [-removeCommutationOffsetOnly] 
 *                   [-fhead=<value>] 
 *                   [-majorOrder=row|column] 
 *                   [-verbose]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Specifies data columns for offset adjustment.                                         |

 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use pipes for input/output.                                                          |
 * | `-commutationMode`                    | Specifies commutation mode (`a`, `b`, `ab1`, `ab2`).                                  |
 * | `-removeCommutationOffsetOnly`        | Removes only commutation offset, leaving other data unaffected.                      |
 * | `-fhead`                              | Fraction of head rows used for offset calculation.                                    |
 * | `-majorOrder=row|column`              | Sets the major order of the output (row or column).                                   |
 * | `-verbose`                            | Enables detailed output during processing.                                            |
 *
 * @subsection spec_req Specific Requirements
 *   - For `-fhead=<value>`:
 *     - Value must be a number greater than 0 and less than or equal to 1.
 *   - For `-commutationMode=<string>`:
 *     - Mode must match one of the predefined modes (`a`, `b`, `ab1`, `ab2`).
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
 * L. Emery, Jialun Luo, R. Soliday
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"

typedef enum {
  CLO_COLUMNS,
  CLO_VERBOSE,
  CLO_PIPE,
  CLO_MAJOR_ORDER,
  CLO_COMMUTATION_MODE,
  CLO_FHEAD,
  CLO_REMOVE_COMMUTATION_OFFSET_ONLY,
  N_OPTIONS
} option_type;

char *commandline_option[N_OPTIONS] = {
  "columns",
  "verbose",
  "pipe",
  "majorOrder",
  "commutationMode",
  "fhead",
  "removeCommutationOffsetOnly",
};

static char *usage = 
  "sddsremoveoffsets [<input-file>] [<output-file>]\n"
  "                  [-pipe=[input],[output]] \n"
  "                   -columns=<name> \n"
  "                  [-commutationMode=<string>] \n"
  "                  [-removeCommutationOffsetOnly] \n"
  "                  [-fhead=<value>] \n"
  "                  [-majorOrder=row|column] \n"
  "                  [-verbose]\n"
  "Options:\n"
  "  -columns=<name>             Specify data columns to adjust\n"
  "  -verbose                    Enable verbose output\n"
  "  -pipe=[input],[output]      Use pipes for input/output\n"
  "  -majorOrder=row|column      Specify output major order\n"
  "  -commutationMode=<string>   Commutation mode (a, b, ab1, ab2)\n"
  "  -fhead=<value>              Fraction of head rows for offset calculation\n"
  "  -removeCommutationOffsetOnly Remove only commutation offset\n\n"
  "Description:\n"
  "  Removes offset from BPM waveform data. Adjusts data such that the resulting file has a zero average.\n"
  "  Supports commutation modes for specific offset handling strategies.\n"
  "Program by Louis Emery and Jialun Luo, ANL (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static long commutation_modes = 4;
static char *commutation_mode_list[4] = {"a", "b", "ab1", "ab2"};

static long pattern_length = 4;

long resolve_column_names(SDDS_DATASET *sdds_in, char ***column, int32_t *columns);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET sdds_in, sdds_out;
  unsigned long major_order_flag;
  char *input = NULL, *output = NULL;
  int64_t j, rows;
  int32_t columns = 0;
  char **input_column = NULL, **output_column = NULL;

  long i, i_arg;
  long verbose = 0;
  long commutation_mode = 0;
  long remove_commutation_offset_only = 0;
  unsigned long pipe_flags = 0;
  long tmpfile_used = 0, no_warnings = 0;
  short column_major_order = -1;
  double *data, offset1 = 0, offset2 = 0, ave_offset = 0;
  double fhead = 1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1)
    bomb(NULL, usage);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], commandline_option, N_OPTIONS, UNIQUE_MATCH)) {
      case CLO_MAJOR_ORDER:
        major_order_flag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&major_order_flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (major_order_flag & SDDS_COLUMN_MAJOR_ORDER)
          column_major_order = 1;
        else if (major_order_flag & SDDS_ROW_MAJOR_ORDER)
          column_major_order = 0;
        break;
      case CLO_COMMUTATION_MODE:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -commutationMode syntax", NULL);
        if ((commutation_mode = match_string(str_tolower(s_arg[i_arg].list[1]), commutation_mode_list, commutation_modes, EXACT_MATCH)) < 0)
          SDDS_Bomb("invalid commutationMode given!");
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_REMOVE_COMMUTATION_OFFSET_ONLY:
        remove_commutation_offset_only = 1;
        break;
      case CLO_FHEAD:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -fhead syntax", NULL);
        if (sscanf(s_arg[i_arg].list[1], "%lf", &fhead) != 1 || fhead <= 0 || fhead > 1)
          fprintf(stderr, "Error: invalid -fhead syntax\n");
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_COLUMNS:
        if (columns)
          SDDS_Bomb("only one -columns option may be given");
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        input_column = tmalloc(sizeof(*input_column) * (columns = s_arg[i_arg].n_items - 1));
        for (i = 0; i < columns; i++)
          input_column[i] = s_arg[i_arg].list[i + 1];
        break;
      default:
        SDDS_Bomb("unrecognized option given");
      }
    } else {
      if (!input)
        input = s_arg[i_arg].list[0];
      else if (!output)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  processFilenames("sddsremoveoffsets", &input, &output, pipe_flags, no_warnings, &tmpfile_used);

  if (!columns)
    SDDS_Bomb("supply the names of columns for offset removal with the -columns option");

  if (!SDDS_InitializeInput(&sdds_in, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!resolve_column_names(&sdds_in, &input_column, &columns))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!columns)
    SDDS_Bomb("no columns selected for offset removal");

  if (!SDDS_InitializeCopy(&sdds_out, &sdds_in, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  output_column = tmalloc(sizeof(*output_column) * columns);
  for (i = 0; i < columns; i++)
    output_column[i] = input_column[i];

  if (!SDDS_InitializeOutput(&sdds_out, SDDS_BINARY, 1, "Remove offset from BPM waveforms", "Modified data", output) ||
      !SDDS_InitializeCopy(&sdds_out, &sdds_in, output, "w"))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (column_major_order != -1)
    sdds_out.layout.data_mode.column_major = column_major_order;
  else
    sdds_out.layout.data_mode.column_major = sdds_in.layout.data_mode.column_major;
  if (!SDDS_WriteLayout(&sdds_out))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  while (SDDS_ReadPage(&sdds_in) > 0) {
    if (!SDDS_CopyPage(&sdds_out, &sdds_in))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((rows = SDDS_CountRowsOfInterest(&sdds_in))) {
      for (i = 0; i < columns; i++) {
        if (!(data = SDDS_GetColumnInDoubles(&sdds_in, input_column[i])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

        long repeat_offset = (data[0] == data[1]) ? 0 : 1;

        double sum1 = 0, sum2 = 0;
        unsigned long count1 = 0, count2 = 0;
        unsigned long half_pattern_length = pattern_length / 2;
        for (j = 0; j < fhead * rows; j++) {
          if ((j - repeat_offset) % pattern_length < half_pattern_length) {
            sum1 += data[j];
            count1++;
          } else {
            sum2 += data[j];
            count2++;
          }
        }
        offset1 = sum1 / count1;
        offset2 = sum2 / count2;
        if (!remove_commutation_offset_only) {
          for (j = 0; j < rows; j++) {
            if ((j - repeat_offset) % pattern_length < half_pattern_length)
              data[j] -= offset1;
            else
              data[j] -= offset2;
          }
        } else {
          ave_offset = (offset1 + offset2) / 2.0;
          for (j = 0; j < rows; j++) {
            if ((j - repeat_offset) % pattern_length < half_pattern_length)
              data[j] = data[j] - offset1 + ave_offset;
            else
              data[j] = data[j] - offset2 + ave_offset;
          }
        }

        if (verbose) {
          printf("offset1 = %f \t offset2 = %f\n", offset1, offset2);
          double new_sum = 0;
          for (j = 0; j < rows; j++) {
            new_sum += data[j];
          }
          double new_mean = new_sum / rows;
          printf("New average: %f\n", new_mean);
        }

        if (!SDDS_SetColumnFromDoubles(&sdds_out, SDDS_SET_BY_NAME, data, rows, output_column[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        free(data);
      }
    }
    if (!SDDS_WritePage(&sdds_out))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  if (!SDDS_Terminate(&sdds_in) || !SDDS_Terminate(&sdds_out))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (tmpfile_used && !replaceFileAndBackUp(input, output))
    exit(EXIT_FAILURE);

  return 0;
}

long resolve_column_names(SDDS_DATASET *sdds_in, char ***column, int32_t *columns) {
  long i;

  SDDS_SetColumnFlags(sdds_in, 0);
  for (i = 0; i < *columns; i++) {
    if (!SDDS_SetColumnsOfInterest(sdds_in, SDDS_MATCH_STRING, (*column)[i], SDDS_OR))
      return 0;
  }
  free(*column);
  if (!(*column = SDDS_GetColumnNames(sdds_in, columns)) || *columns == 0) {
    SDDS_SetError("no columns found");
    return 0;
  }
  return 1;
}
