/**
 * @file sddspeakfind.c
 * @brief Program to detect and analyze peaks in SDDS data files.
 *
 * @details
 * This program processes an SDDS (Self Describing Data Set) file to identify peaks
 * within a specified column of data. Detection is based on configurable parameters
 * like thresholds, curvature limits, exclusion zones, and more. The program supports
 * input and output through files or pipes and offers compatibility with both row-major
 * and column-major data storage orders.
 *
 * @section Usage
 * ```
 * sddspeakfind [<inputfile>] [<outputfile>] 
 *              [-pipe=[input][,output]]
 *               -column=<columnName>
 *              [-threshold=<value>|@<parametername>]
 *              [-fivePoint]
 *              [-sevenPoint]
 *              [-exclusionZone=<fraction>]
 *              [-changeThreshold=<fractionalChange>]
 *              [-curvatureLimit=<xColumn>,<minValue>]
 *              [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required  | Description                                                                 |
 * |-----------|-----------------------------------------------------------------------------|
 * | `-column` | Specify the column name for peak detection.                                 |
 *
 * | Optional  | Description                                                                 |
 * |-----------|-----------------------------------------------------------------------------|
 * | `-pipe`            | Specify input/output through pipes.                                |
 * | `-threshold`       | Set a threshold for peak detection.                                |
 * | `-fivePoint`       | Use a 5-point criterion for peak detection.                        |
 * | `-sevenPoint`      | Use a 7-point criterion for peak detection.                        |
 * | `-exclusionZone`   | Exclude peaks too close to each other, specified as a fraction.    |
 * | `-changeThreshold` | Apply a fractional change threshold for filtering flat peaks.      |
 * | `-curvatureLimit`  | Apply curvature filtering to exclude shallow peaks.                |
 * | `-majorOrder`      | Specify row or column major order for output.                      |
 *
 * @subsection Incompatibilities
 * - The following options are mutually exclusive:
 *   - `-fivePoint` and `-sevenPoint`
 *
 * @subsection Requirements
 * - For `-curvatureLimit`:
 *   - Requires both `<xColumn>` and `<minValue>` to be specified.
 * - For `-threshold`:
 *   - Accepts either a numeric value or a parameter name prefixed with `@`.
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
 *   M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#define THRESHOLD 0x0001UL
#define FIVEPOINT 0x0002UL
#define CHANGETHRES 0x0004UL
#define EZONEFRAC 0x0008UL
#define PAR_THRESHOLD 0x0010UL
#define SEVENPOINT 0x0020UL
#define CURVATURELIMIT 0x0040UL

/* Enumeration for option types */
enum option_type {
  CLO_THRESHOLD,
  CLO_FIVEPOINT,
  CLO_CHANGETHRESHOLD,
  CLO_PIPE,
  CLO_COLUMN,
  CLO_EXCLUSIONZONE,
  CLO_MAJOR_ORDER,
  CLO_SEVENPOINT,
  CLO_CURVATURE_LIMIT,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "threshold",
  "fivepoint",
  "changethreshold",
  "pipe",
  "column",
  "exclusionzone",
  "majorOrder",
  "sevenpoint",
  "curvatureLimit"};

static char *USAGE =
    "sddspeakfind [<inputfile>] [<outputfile>] [-pipe=[input][,output]] \n"
    "   -column=<columnName> \n"
    "  [-threshold=<value>|@<parametername>] \n"
    "  [{-fivePoint|-sevenPoint}] \n"
    "  [-exclusionZone=<fraction>] \n"
    "  [-changeThreshold=<fractionalChange>] \n"
    "  [-curvatureLimit=<xColumn>,<minValue>] \n"
    "  [-majorOrder=row|column] \n\n"
    "Finds peaks in a column of data as a function of row index.\n"
    "Program by Michael Borland. ("__DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

/* Function prototypes */
void mark_peaks(double *data, int32_t *row_flag, int64_t rows, long n_points);
void unmark_flat_peaks(double *data, int32_t *row_flag, int64_t rows,
                       double change_threshold, long five_point, long seven_point,
                       double *x_data, double curvature_limit);
void unmark_excluded_peaks(double *data, int32_t *row_flag, int64_t rows,
                           double ezone_fraction);

int main(int argc, char **argv) {
  SDDS_DATASET in_set, out_set;
  SCANNED_ARG *s_arg;
  long i_arg, page_returned;
  int64_t rows, row;
  int32_t *row_flag;
  char *input, *output, *column_name, *par_threshold_name, *x_column_name;
  double *data, *x_data;
  unsigned long pipe_flags, flags, major_order_flag;
  double threshold, ezone_fraction, change_threshold, curvature_limit;
  short column_major_order = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2 || argc > (2 + N_OPTIONS))
    bomb(NULL, USAGE);

  flags = pipe_flags = 0;
  input = output = NULL;
  column_name = NULL;
  ezone_fraction = change_threshold = curvature_limit = 0;
  row_flag = NULL;
  par_threshold_name = NULL;
  x_column_name = NULL;
  x_data = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        major_order_flag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&major_order_flag, s_arg[i_arg].list + 1,
                           &s_arg[i_arg].n_items, 0, "row", -1, NULL, 0,
                           SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0,
                           SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (major_order_flag & SDDS_COLUMN_MAJOR_ORDER)
          column_major_order = 1;
        else if (major_order_flag & SDDS_ROW_MAJOR_ORDER)
          column_major_order = 0;
        break;
      case CLO_THRESHOLD:
        if (s_arg[i_arg].n_items == 2) {
          if (s_arg[i_arg].list[1][0] == '@') {
            SDDS_CopyString(&par_threshold_name, s_arg[i_arg].list[1] + 1);
            flags |= PAR_THRESHOLD;
          } else {
            if (sscanf(s_arg[i_arg].list[1], "%lf", &threshold) != 1)
              SDDS_Bomb("incorrect -threshold syntax");
            flags |= THRESHOLD;
          }
        } else
          SDDS_Bomb("incorrect -threshold syntax");
        break;
      case CLO_FIVEPOINT:
        flags |= FIVEPOINT;
        break;
      case CLO_SEVENPOINT:
        flags |= SEVENPOINT;
        break;
      case CLO_CHANGETHRESHOLD:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &change_threshold) != 1 ||
            change_threshold <= 0)
          SDDS_Bomb("incorrect -changeThreshold syntax or values");
        flags |= CHANGETHRES;
        break;
      case CLO_CURVATURE_LIMIT:
        if (s_arg[i_arg].n_items != 3 ||
            !strlen(x_column_name = s_arg[i_arg].list[1]) ||
            sscanf(s_arg[i_arg].list[2], "%lf", &curvature_limit) != 1 ||
            curvature_limit <= 0)
          SDDS_Bomb("incorrect -curvatureLimit syntax or values");
        flags |= CURVATURELIMIT;
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_COLUMN:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -column syntax");
        column_name = s_arg[i_arg].list[1];
        break;
      case CLO_EXCLUSIONZONE:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &ezone_fraction) != 1 ||
            ezone_fraction <= 0)
          SDDS_Bomb("invalid -exclusionZone syntax or value");
        flags |= EZONEFRAC;
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s\n",
                s_arg[i_arg].list[0]);
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

  processFilenames("sddspeakfind", &input, &output, pipe_flags, 0, NULL);

  if (!column_name)
    SDDS_Bomb("-column option must be given");

  if (!SDDS_InitializeInput(&in_set, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_FindColumn(&in_set, FIND_NUMERIC_TYPE, column_name, NULL))
    SDDS_Bomb("the given column is nonexistent or nonnumeric");

  if (x_column_name &&
      !SDDS_FindColumn(&in_set, FIND_NUMERIC_TYPE, x_column_name, NULL))
    SDDS_Bomb("the given x column is nonexistent or nonnumeric");

  if (!SDDS_InitializeCopy(&out_set, &in_set, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  out_set.layout.data_mode.column_major =
    column_major_order != -1 ? column_major_order : in_set.layout.data_mode.column_major;

  if (!SDDS_WriteLayout(&out_set))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  /* Main loop for processing pages */
  while ((page_returned = SDDS_ReadPage(&in_set)) > 0) {
    if (!SDDS_CopyPage(&out_set, &in_set)) {
      SDDS_SetError("Problem copying data for output file");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if ((rows = SDDS_CountRowsOfInterest(&out_set)) > 1) {
      data = SDDS_GetColumnInDoubles(&in_set, column_name);
      if (!data)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      if (x_column_name)
        x_data = SDDS_GetColumnInDoubles(&in_set, x_column_name);
      if (x_column_name && !x_data)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      row_flag = SDDS_Realloc(row_flag, sizeof(*row_flag) * rows);
      for (row = 0; row < rows; row++)
        row_flag[row] = 0;

      mark_peaks(data, row_flag, rows,
                 flags & SEVENPOINT ? 7 : (flags & FIVEPOINT ? 5 : 3));

      if (flags & PAR_THRESHOLD)
        if (!SDDS_GetParameter(&in_set, par_threshold_name, &threshold))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      if (flags & THRESHOLD || flags & PAR_THRESHOLD)
        for (row = 0; row < rows; row++)
          if (row_flag[row] && data[row] < threshold)
            row_flag[row] = 0;

      if (flags & CHANGETHRES || flags & CURVATURELIMIT)
        unmark_flat_peaks(data, row_flag, rows, change_threshold,
                          flags & FIVEPOINT, flags & SEVENPOINT, x_data, curvature_limit);

      if (flags & EZONEFRAC)
        unmark_excluded_peaks(data, row_flag, rows, ezone_fraction);

      if (!SDDS_AssertRowFlags(&out_set, SDDS_FLAG_ARRAY, row_flag, rows))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      free(data);
    }

    if (!SDDS_WritePage(&out_set))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_Terminate(&in_set) || !SDDS_Terminate(&out_set)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  free_scanargs(&s_arg, argc);
  free(par_threshold_name);
  free(row_flag);

  return 0;
}

void mark_peaks(double *data, int32_t *row_flag, int64_t rows, long n_points) {
  double y0, y1, y2;
  int64_t i, rows_minus;
  short five_point = 0;

  switch (n_points) {
  case 5:
    y0 = data[1];
    y1 = data[2];
    if (rows < 5)
      return;
    five_point = 1;
    break;
  case 7:
    if (rows < 7)
      return;
    for (i = 3; i < (rows - 3); i++) {
      if ((data[i - 3] < data[i - 2] && data[i - 2] < data[i - 1] &&
           data[i - 1] < data[i] && data[i + 1] < data[i] &&
           data[i + 2] < data[i + 1] && data[i + 3] < data[i + 2]))
        row_flag[i] = 1;
    }
    return;
  default:
    if (rows < 3)
      return;
    y0 = data[0];
    y1 = data[1];
  }

  rows_minus = rows - (five_point ? 2 : 1);
  for (i = (five_point ? 2 : 1); i < rows_minus; i++) {
    y2 = data[i + 1];
    if ((y1 > y0 && y1 > y2) || (y1 == y0 && y1 > y2) || (y1 > y0 && y1 == y2)) {
      if (!five_point || (data[i - 2] < y0 && data[i + 2] < y2))
        row_flag[i] = 1;
    }
    y0 = y1;
    y1 = y2;
  }
}

void unmark_flat_peaks(double *data, int32_t *row_flag, int64_t rows,
                       double change_threshold, long five_point, long seven_point,
                       double *x_data, double curvature_limit) {
  long delta;
  int64_t i, rows1;

  delta = five_point ? 2 : (seven_point ? 3 : 1);
  rows1 = rows - delta;

  if (change_threshold > 0) {
    for (i = delta; i < rows1; i++) {
      if (row_flag[i]) {
        if (!data[i] || (data[i] - data[i + delta]) / data[i] > change_threshold)
          continue;
        if (!data[i] || (data[i] - data[i - delta]) / data[i] > change_threshold)
          continue;
        row_flag[i] = 0;
      }
    }
  }

  if (curvature_limit > 0) {
    double chi, coef[3], scoef[3], diff[7];
    int32_t order[3] = {0, 1, 2};
    for (i = delta; i < rows1; i++) {
      if (row_flag[i]) {
        lsfg(x_data + i - delta, data + i - delta, NULL, 2 * delta + 1,
             3, order, coef, scoef, &chi, diff, ipower);
        if (fabs(coef[2]) < curvature_limit)
          row_flag[i] = 0;
      }
    }
  }
}

void unmark_excluded_peaks(double *data, int32_t *row_flag, int64_t rows,
                           double ezone_fraction) {
  int64_t lower, upper, i, j, rows1;
  rows1 = rows - 1;

  for (i = 0; i < rows; i++) {
    if (row_flag[i]) {
      lower = i - ((int64_t)(ezone_fraction / 2.0 * rows + 0.5));
      if (lower < 0)
        lower = 0;
      upper = i + ((int64_t)(ezone_fraction / 2.0 * rows + 0.5));
      if (upper > rows1)
        upper = rows1;
      for (j = lower; j <= upper; j++) {
        if (j != i && row_flag[j] && data[j] <= data[i])
          row_flag[j] = 0;
      }
    }
  }
}
