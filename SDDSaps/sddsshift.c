/**
 * @file sddsshift.c
 * @brief Program for shifting data columns in SDDS files.
 *
 * @details
 * This program provides functionalities for shifting data columns in SDDS files.
 * It supports several shift modes, including zero-padding and circular shifting, and allows
 * matching columns to minimize the least squares error during the shift process.
 *
 * @section Usage
 * ```
 * sddsshift [<inputfile>] [<outputfile>]
 *           [-pipe=[input][,output]] 
 *            -columns=<inputcol>[,...]
 *           [-zero]
 *           [-circular] 
 *           [-shift=<points>]
 *           [-match=<matchcol>]
 *           [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Specify the data columns to shift. Wildcards are accepted.                            |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Define pipe output options.                                                          |
 * | `-zero`                               | Set exposed end-points to zero.                                                      |
 * | `-circular`                           | Perform circular shifting of data.                                                   |
 * | `-shift`                              | Number of rows to shift (positive = later, negative = earlier).                       |
 * | `-match`                              | Specify a column to match for least squares error minimization.                       |
 * | `-majorOrder`                         | Specify the output file order.                                                       |
 *
 * @subsection Incompatibilities
 *   - Either `-shift` or `-match` must be provided, but not both.
 *   - `-zero` and `-circular` options are mutually exclusive.
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

#include "SDDS.h"
#include "mdb.h"
#include "scan.h"

/* Enumeration for option types */
typedef enum {
  CLO_PIPE,
  CLO_COLUMNS,
  CLO_DELAY,
  CLO_MATCH,
  CLO_ZERO,
  CLO_MAJOR_ORDER,
  CLO_CIRCULAR,
  N_OPTIONS
} option_type;

static char *option[N_OPTIONS] = {
  "pipe",
  "columns",
  "shift",
  "match",
  "zero",
  "majorOrder",
  "circular"};

/* Usage information */
static char *usage =
  "sddsshift [<inputfile>] [<outputfile>]\n"
  "       [-pipe=[input][,output]] -columns=<inputcol>[,...]\n"
  "       [-zero | -circular] [-shift=<points> | -match=<matchcol>]\n"
  "       [-majorOrder=row|column]\n\n"
  "-columns     Provide <inputcols>, i.e., the data columns to be shifted.\n"
  "             Wildcards accepted.\n"
  "-shift       Provide number of points to shift in rows.\n"
  "             (positive = later, negative = earlier).\n"
  "-match       Provide <matchcol>. <inputcol> is shifted to\n"
  "             minimize the least squares error relative to <matchcol>.\n"
  "-zero        Set exposed end-points to zero.\n"
  "-circular    Shift the data in a circular fashion.\n"
  "-majorOrder  Specify output file in row or column major order.\n\n"
  "sddsshift shifts specified data columns by rows. A copy of <inputfile> is made with the\n"
  "addition of new columns \"Shifted<inputcol>\". Exposed end-points\n"
  "are set to zero if the zero option is provided or\n"
  "the value of the first/last row in <inputcol> as appropriate.\n"
  "A parameter \"<inputcol>Shift\" contains the number of rows shifted.\n";

/* Function prototypes */
void shift(double *inputcol, double *outputcol, int64_t npoints, long delay, long zero, long circular);
double mse(double *y1, double *y2, long npoints);
double simplex_driver(double *data, long *invalid);
long resolve_column_names(SDDS_DATASET *SDDSin, char ***column, int32_t *columns);

/* Global variables */
double *input_col, *working, *match_col;
int64_t npoints;
long zero, circular;

int main(int argc, char *argv[]) {
  SCANNED_ARG *s_arg;
  double *output_col;
  char *input_col_name, *match_col_name, *inputfile, *outputfile;
  long i, i_arg, delay, tmp_file_used;
  unsigned long pipe_flags, major_order_flag;
  char actual_name[256], actual_desc[256], delay_name[256];
  double sim_delay, lower, upper, final_mse;
  SDDS_DATASET SDDS_input, SDDS_output;
  short column_major_order = -1;
  char **input_col_names = NULL;
  int32_t n_input_col_names = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    bomb(NULL, usage);
  }

  /* Initialize flags and defaults */
  tmp_file_used = 0;
  pipe_flags = 0;
  inputfile = outputfile = input_col_name = match_col_name = NULL;
  delay = 0;
  sim_delay = 0.0;
  zero = circular = 0;
  input_col = match_col = output_col = working = NULL;

  /* Process arguments */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        major_order_flag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&major_order_flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("Invalid -majorOrder syntax/values");
        }
        if (major_order_flag & SDDS_COLUMN_MAJOR_ORDER) {
          column_major_order = 1;
        } else if (major_order_flag & SDDS_ROW_MAJOR_ORDER) {
          column_major_order = 0;
        }
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags)) {
          SDDS_Bomb("Invalid -pipe syntax");
        }
        break;
      case CLO_COLUMNS:
        if (s_arg[i_arg].n_items < 2) {
          SDDS_Bomb("Invalid -columns syntax.");
        }
        if (n_input_col_names) {
          SDDS_Bomb("Invalid syntax: specify -columns once only");
        }
        input_col_names = tmalloc(sizeof(*input_col_names) * (s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++) {
          input_col_names[i - 1] = s_arg[i_arg].list[i];
        }
        n_input_col_names = s_arg[i_arg].n_items - 1;
        break;
      case CLO_DELAY:
        if (s_arg[i_arg].n_items != 2) {
          SDDS_Bomb("Invalid -delay option.");
        }
        if (!get_long(&delay, s_arg[i_arg].list[1])) {
          SDDS_Bomb("Invalid delay value provided.");
        }
        break;
      case CLO_MATCH:
        if (s_arg[i_arg].n_items != 2) {
          SDDS_Bomb("Invalid -match option.");
        }
        match_col_name = s_arg[i_arg].list[1];
        break;
      case CLO_ZERO:
        zero = 1;
        break;
      case CLO_CIRCULAR:
        circular = 1;
        break;
      default:
        fprintf(stderr, "Error (%s): unknown switch: %s\n", argv[0], s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    } else {
      if (!inputfile) {
        inputfile = s_arg[i_arg].list[0];
      } else if (!outputfile) {
        outputfile = s_arg[i_arg].list[0];
      } else {
        SDDS_Bomb("Too many files provided.");
      }
    }
  }

  processFilenames("sddsshift", &inputfile, &outputfile, pipe_flags, 1, &tmp_file_used);

  if (zero && circular) {
    SDDS_Bomb("The -zero and -circular options are mutually exclusive.");
  }

  if (n_input_col_names == 0) {
    SDDS_Bomb("A shift column is not given!");
  }
  if (!match_col_name && !delay) {
    SDDS_Bomb("Either match column or shift should be provided.");
  }
  if (match_col_name && delay) {
    SDDS_Bomb("-match column option and -shift option are incompatible.");
  }

  /* Initialize SDDS input */
  if (!SDDS_InitializeInput(&SDDS_input, inputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!resolve_column_names(&SDDS_input, &input_col_names, &n_input_col_names)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_InitializeCopy(&SDDS_output, &SDDS_input, outputfile, "w")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  SDDS_output.layout.data_mode.column_major = column_major_order != -1 ? column_major_order : SDDS_input.layout.data_mode.column_major;

  for (i = 0; i < n_input_col_names; i++) {
    snprintf(actual_name, sizeof(actual_name), "Shifted%s", input_col_names[i]);
    snprintf(actual_desc, sizeof(actual_desc), "Shifted %s", input_col_names[i]);
    snprintf(delay_name, sizeof(delay_name), "%sShift", input_col_names[i]);
    if (SDDS_DefineColumn(&SDDS_output, actual_name, NULL, NULL, actual_desc, NULL, SDDS_DOUBLE, 0) < 0 ||
        SDDS_DefineParameter(&SDDS_output, delay_name, NULL, NULL, NULL, NULL, SDDS_LONG, NULL) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  if (!SDDS_WriteLayout(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  while ((SDDS_ReadPage(&SDDS_input)) > 0) {
    if (!SDDS_CopyPage(&SDDS_output, &SDDS_input)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    npoints = SDDS_CountRowsOfInterest(&SDDS_input);
    if (npoints < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    for (i = 0; i < n_input_col_names; i++) {
      snprintf(actual_name, sizeof(actual_name), "Shifted%s", input_col_names[i]);
      snprintf(delay_name, sizeof(delay_name), "%sShift", input_col_names[i]);
      input_col = SDDS_GetColumnInDoubles(&SDDS_input, input_col_names[i]);
      if (!input_col) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      output_col = calloc(npoints, sizeof(*output_col));
      if (!output_col) {
        SDDS_Bomb("Memory allocation failure");
      }

      if (match_col_name) {
        match_col = SDDS_GetColumnInDoubles(&SDDS_input, match_col_name);
        if (!match_col) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        working = calloc(npoints, sizeof(*working));
        if (!working) {
          SDDS_Bomb("Memory allocation failure");
        }
        lower = -((double)npoints - 1);
        upper = (double)npoints - 1;
        simplexMin(&final_mse, &sim_delay, NULL, &lower, &upper, NULL, 1, 1e-6, 1e-12, simplex_driver, NULL, 2 * npoints, 6, 12, 3, 1.0, 0);
        delay = (long)sim_delay;
        free(working);
        free(match_col);
        match_col = working = NULL;
      }

      shift(input_col, output_col, npoints, delay, zero, circular);
      if (!SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, delay_name, delay, NULL) ||
          !SDDS_SetColumn(&SDDS_output, SDDS_SET_BY_NAME, output_col, npoints, actual_name)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      free(output_col);
      free(input_col);
      output_col = input_col = NULL;
    }

    if (!SDDS_WritePage(&SDDS_output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_Terminate(&SDDS_input) || !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  free_scanargs(&s_arg, argc);
  return EXIT_SUCCESS;
}

long resolve_column_names(SDDS_DATASET *SDDSin, char ***column, int32_t *columns) {
  long i;

  SDDS_SetColumnFlags(SDDSin, 0);
  for (i = 0; i < *columns; i++) {
    if (!SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, (*column)[i], SDDS_OR)) {
      return 0;
    }
  }
  free(*column);
  *column = SDDS_GetColumnNames(SDDSin, columns);
  if (!*column || *columns == 0) {
    SDDS_SetError("No columns found");
    return 0;
  }
  return 1;
}

void shift(double *inputcol, double *outputcol, int64_t npoints, long delay, long zero, long circular) {
  int64_t i, j;

  if (circular) {
    short local_buffer = 0;
    if (inputcol == outputcol) {
      inputcol = tmalloc(sizeof(*inputcol) * npoints);
      memcpy(inputcol, outputcol, sizeof(*inputcol) * npoints);
      local_buffer = 1;
    }
    for (i = 0; i < npoints; i++) {
      j = (i - delay) % npoints;
      if (j < 0) {
        j += npoints;
      }
      outputcol[i] = inputcol[j];
    }
    if (local_buffer) {
      free(inputcol);
    }
  } else {
    if (delay < 0) {
      delay = -delay;
      for (i = 0; i < npoints - delay; i++) {
        outputcol[i] = inputcol[i + delay];
      }
      for (i = npoints - delay; i < npoints; i++) {
        outputcol[i] = zero ? 0 : inputcol[npoints - 1];
      }
    } else {
      for (i = npoints - 1; i >= delay; i--) {
        outputcol[i] = inputcol[i - delay];
      }
      for (i = 0; i < delay; i++) {
        outputcol[i] = zero ? 0 : inputcol[0];
      }
    }
  }
}

double mse(double *y1, double *y2, long npoints) {
  long i;
  double error = 0;

  for (i = 0; i < npoints; i++) {
    error += (y1[i] - y2[i]) * (y1[i] - y2[i]);
  }
  return error / npoints;
}

double simplex_driver(double *data, long *invalid) {
  long delay;

  *invalid = 0;
  delay = (long)*data;
  shift(input_col, working, npoints, delay, zero, circular);
  return mse(match_col, working, npoints);
}
