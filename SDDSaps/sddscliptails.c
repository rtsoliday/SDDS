/**
 * @file sddscliptails.c
 * @brief Clips tails from specified columns in SDDS files based on various criteria.
 *
 * @details
 * This program reads an SDDS (Self Describing Data Sets) file and processes specified columns to
 * clip data tails based on criteria like fractional limits, absolute limits, full-width-half-maximum
 * (FWHM), or separation by zero. The processed data is then written to an output SDDS file.
 * It supports flexible configuration for clipping and output format (row or column major order).
 *
 * @section Usage
 * ```
 * sddscliptails [<input>] [<output>]
 *               [-pipe=[in][,out]]
 *               [-columns=<listOfNames>]
 *               [-fractional=<value>]
 *               [-absolute=<value>]
 *               [-fwhm=<multiplier>]
 *               [-afterzero[=<bufferWidth>]]
 *               [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required  | Description                                                                 |
 * |-----------|-----------------------------------------------------------------------------|
 * | `-columns`            | List of columns to process.                                             |
 *
 * | Optional                | Description                                                              |
 * |-----------------------|--------------------------------------------------------------------------|
 * | `-pipe`               | Use standard input/output for input and/or output.                      |
 * | `-fractional`         | Clip tails below a fraction of the peak value.                          |
 * | `-absolute`           | Clip tails below a specified absolute value.                            |
 * | `-fwhm`               | Clip tails beyond a specified number of FWHMs from the peak.            |
 * | `-afterzero`          | Clip tails separated from the peak by zeros, with optional buffer width. |
 * | `-majorOrder`         | Specify row or column major order for output data.                      |
 *
 * @subsection MEO Mutually Exclusive Options
 *   - Only one of the following may be specified:
 *     - `-fractional`
 *     - `-absolute`
 *   - Only one of the following may be specified:
 *     - `-fwhm`
 *     - `-afterzero`
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
 * M. Borland, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"

/* Enumeration for option types */
typedef enum {
  CLO_FRACTIONAL,
  CLO_ABSOLUTE,
  CLO_FWHM,
  CLO_PIPE,
  CLO_COLUMNS,
  CLO_AFTERZERO,
  CLO_MAJOR_ORDER,
  N_OPTIONS
} option_type;

static char *option[N_OPTIONS] = {
  "fractional",
  "absolute",
  "fwhm",
  "pipe",
  "columns",
  "afterzero",
  "majorOrder",
};

static char *usage =
  "sddscliptails [<input>] [<output>] [-pipe=[input][,output]]\n"
  "    [-columns=<listOfNames>] [-fractional=<value>] [-absolute=<value>] [-fwhm=<multiplier>]\n"
  "    [-afterzero[=<bufferWidth>]] [-majorOrder=row|column]\n\n"
  "-columns      List of columns to process.\n"
  "-fractional   Clip a tail if it falls below this fraction of the peak.\n"
  "-absolute     Clip a tail if it falls below this absolute value.\n"
  "-fwhm         Clip a tail if it is beyond this many FWHM from the peak.\n"
  "-afterzero    Clip a tail if it is separated from the peak by values equal to zero.\n"
  "              If <bufferWidth> is specified, then a region <bufferWidth> wide is kept\n"
  "              on either side of the peak, if possible.\n"
  "-majorOrder   Writes output file in row or column major order.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static long resolve_column_names(SDDS_DATASET *sdds_in, char ***column, int32_t *columns);
static void clip_tail(double *data, int64_t rows, double abs_limit, double frac_limit, short *in_tail);
static void clip_fwhm(double *data, int64_t n_data, double fwhm_limit, double *indep_data, short *in_tail);
static void clip_after_zero(double *data, int64_t rows, long buffer_width, short *in_tail);

int main(int argc, char **argv) {
  int i_arg;
  char **input_column;
  char *input, *output;
  long read_code, after_zero, after_zero_buffer_width;
  int64_t i, rows;
  int32_t columns;
  unsigned long pipe_flags, major_order_flag;
  SCANNED_ARG *scanned;
  SDDS_DATASET sdds_in, sdds_out;
  double *data, *indep_data;
  double fractional_limit, absolute_limit, fwhm_limit;
  short *in_tail;
  short column_major_order = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 2)
    bomb(NULL, usage);

  output = input = NULL;
  input_column = NULL;
  columns = 0;
  pipe_flags = 0;
  fractional_limit = fwhm_limit = 0;
  absolute_limit = -1;
  after_zero = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* Process options */
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        major_order_flag = 0;
        scanned[i_arg].n_items--;
        if (scanned[i_arg].n_items > 0 &&
            (!scanItemList(&major_order_flag, scanned[i_arg].list + 1, &scanned[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (major_order_flag & SDDS_COLUMN_MAJOR_ORDER)
          column_major_order = 1;
        else if (major_order_flag & SDDS_ROW_MAJOR_ORDER)
          column_major_order = 0;
        break;
      case CLO_COLUMNS:
        if (scanned[i_arg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        input_column = tmalloc(sizeof(*input_column) * (columns = scanned[i_arg].n_items - 1));
        for (i = 0; i < columns; i++)
          input_column[i] = scanned[i_arg].list[i + 1];
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_FRACTIONAL:
        if (scanned[i_arg].n_items < 2 || sscanf(scanned[i_arg].list[1], "%lf", &fractional_limit) != 1 || fractional_limit < 0)
          SDDS_Bomb("invalid -fractional syntax");
        break;
      case CLO_ABSOLUTE:
        if (scanned[i_arg].n_items < 2 || sscanf(scanned[i_arg].list[1], "%lf", &absolute_limit) != 1 || absolute_limit < 0)
          SDDS_Bomb("invalid -absolute syntax");
        break;
      case CLO_FWHM:
        if (scanned[i_arg].n_items < 2 || sscanf(scanned[i_arg].list[1], "%lf", &fwhm_limit) != 1 || fwhm_limit < 0)
          SDDS_Bomb("invalid -fwhm syntax");
        break;
      case CLO_AFTERZERO:
        after_zero = 1;
        if (scanned[i_arg].n_items > 2 ||
            (scanned[i_arg].n_items == 2 &&
             (sscanf(scanned[i_arg].list[1], "%ld", &after_zero_buffer_width) != 1 ||
              after_zero_buffer_width <= 0)))
          SDDS_Bomb("invalid -afterZero syntax");
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s\n", scanned[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  processFilenames("sddscliptails", &input, &output, pipe_flags, 0, NULL);

  if (!columns)
    SDDS_Bomb("supply the names of columns to process with the -columns option");

  if (!SDDS_InitializeInput(&sdds_in, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!resolve_column_names(&sdds_in, &input_column, &columns))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!columns)
    SDDS_Bomb("no columns selected for processing");

  if (!SDDS_InitializeCopy(&sdds_out, &sdds_in, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (column_major_order != -1)
    sdds_out.layout.data_mode.column_major = column_major_order;
  else
    sdds_out.layout.data_mode.column_major = sdds_in.layout.data_mode.column_major;

  if (!SDDS_DefineColumn(&sdds_out, "InTail", NULL, NULL, NULL, NULL, SDDS_SHORT, 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_WriteLayout(&sdds_out))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  indep_data = NULL;
  in_tail = NULL;
  while ((read_code = SDDS_ReadPage(&sdds_in)) > 0) {
    if (!SDDS_CopyPage(&sdds_out, &sdds_in))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((rows = SDDS_CountRowsOfInterest(&sdds_in))) {
      if (fwhm_limit > 0) {
        indep_data = SDDS_Realloc(indep_data, sizeof(*indep_data) * rows);
        if (!indep_data)
          SDDS_Bomb("memory allocation failure");
        for (i = 0; i < rows; i++)
          indep_data[i] = i;
      }
      in_tail = SDDS_Realloc(in_tail, sizeof(*in_tail) * rows);
      if (!in_tail)
        SDDS_Bomb("memory allocation failure");
      for (i = 0; i < rows; i++)
        in_tail[i] = 0;
      for (i = 0; i < columns; i++) {
        data = SDDS_GetColumnInDoubles(&sdds_in, input_column[i]);
        if (!data)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        clip_tail(data, rows, absolute_limit, fractional_limit, in_tail);
        if (fwhm_limit > 0)
          clip_fwhm(data, rows, fwhm_limit, indep_data, in_tail);
        if (after_zero)
          clip_after_zero(data, rows, after_zero_buffer_width, in_tail);
        if (!SDDS_SetColumnFromDoubles(&sdds_out, SDDS_SET_BY_NAME, data, rows, input_column[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        free(data);
      }
    }
    if (!SDDS_SetColumn(&sdds_out, SDDS_SET_BY_NAME, in_tail, rows, "InTail"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_WritePage(&sdds_out))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (SDDS_Terminate(&sdds_in) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if (SDDS_Terminate(&sdds_out) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if (indep_data)
    free(indep_data);
  if (in_tail)
    free(in_tail);
  return 0;
}

static long resolve_column_names(SDDS_DATASET *sdds_in, char ***column, int32_t *columns) {
  long i;

  SDDS_SetColumnFlags(sdds_in, 0);
  for (i = 0; i < *columns; i++) {
    if (!SDDS_SetColumnsOfInterest(sdds_in, SDDS_MATCH_STRING, (*column)[i], SDDS_OR))
      return 0;
  }
  free(*column);
  *column = SDDS_GetColumnNames(sdds_in, columns);
  if (!*column || *columns == 0) {
    SDDS_SetError("no columns found");
    return 0;
  }
  return 1;
}

static void clip_tail(double *data, int64_t rows, double abs_limit, double frac_limit, short *in_tail) {
  long clip;
  int64_t i, imin, imax;
  double abs_limit2;

  if (abs_limit < 0 && frac_limit <= 0)
    return;
  if (rows < 3)
    return;

  index_min_max(&imin, &imax, data, rows);
  if (!data[imax])
    return;

  abs_limit2 = frac_limit * data[imax];
  if (abs_limit < 0 || (frac_limit && abs_limit2 < abs_limit))
    abs_limit = abs_limit2;

  if (abs_limit < 0)
    return;

  for (i = imax - 1, clip = 0; i >= 0; i--) {
    if (!clip && data[i] < abs_limit)
      clip = 1;
    if (clip) {
      in_tail[i] += 1;
      data[i] = 0;
    }
  }

  for (i = imax + 1, clip = 0; i < rows; i++) {
    if (!clip && data[i] < abs_limit)
      clip = 1;
    if (clip) {
      in_tail[i] += 1;
      data[i] = 0;
    }
  }
}

static void clip_fwhm(double *data, int64_t n_data, double fwhm_limit, double *indep_data, short *in_tail) {
  double top, base, fwhm;
  int64_t i1, i2, i2_save, i, imin, imax;
  double point1, point2;

  if (n_data < 3 || fwhm_limit <= 0)
    return;

  index_min_max(&imin, &imax, data, n_data);
  if (!data[imax])
    return;

  if (!findTopBaseLevels(&top, &base, data, n_data, 50, 2.0))
    return;

  i1 = findCrossingPoint(0, data, n_data, top * 0.5, 1, indep_data, &point1);
  if (i1 < 0)
    return;

  i2 = i2_save = findCrossingPoint(i1, data, n_data, top * 0.9, -1, NULL, NULL);
  if (i2 < 0)
    return;

  i2 = findCrossingPoint(i2, data, n_data, top * 0.5, -1, indep_data, &point2);
  if (i2 < 0)
    return;

  fwhm = point2 - point1;

  for (i = imax + fwhm * fwhm_limit; i < n_data; i++) {
    in_tail[i] += 1;
    data[i] = 0;
  }
  for (i = imax - fwhm * fwhm_limit; i >= 0; i--) {
    in_tail[i] += 1;
    data[i] = 0;
  }
}

static void clip_after_zero(double *data, int64_t rows, long buffer_width, short *in_tail) {
  int64_t imin, imax, i, j;
  index_min_max(&imin, &imax, data, rows);
  if (!data[imax])
    return;
  for (i = imax + 1; i < rows; i++) {
    if (data[i] == 0) {
      for (j = i + buffer_width; j < rows; j++) {
        in_tail[j] += 1;
        data[j] = 0;
      }
      break;
    }
  }

  for (i = imax - 1; i >= 0; i--) {
    if (data[i] == 0) {
      for (j = i - buffer_width; j >= 0; j--) {
        in_tail[j] += 1;
        data[j] = 0;
      }
      break;
    }
  }
}
