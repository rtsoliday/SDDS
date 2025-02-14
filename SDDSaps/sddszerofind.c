/**
 * @file sddszerofind.c
 * @brief A program to identify zero crossings in a specified column of an SDDS file.
 *
 * @details
 * This program processes SDDS files to find zero-crossing points in one column as a function of another column.
 * It performs interpolation to compute zero positions and optionally calculates the slope at each zero.
 * The output can be configured to include additional slope information and different ordering formats.
 *
 * @section Usage
 * ```
 * sddszerofind [<inputfile>] [<outputfile>]
 *              [-pipe=[input][,output]]
 *              -zeroesOf=<columnName>
 *              [-columns=<columnNames>]
 *              [-offset=<value>]
 *              [-slopeOutput]
 *              [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-zeroesOf`                           | Specifies the column to find zero crossings.                                          |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Enables input and/or output through a pipe.                                           |
 * | `-columns`                            | Specifies columns for interpolation (default: all numerical columns).                 |
 * | `-offset`                             | Adjusts the zero-finding threshold by adding an offset.                               |
 * | `-slopeOutput`                        | Includes the slope at zero-crossing points in the output.                             |
 * | `-majorOrder`                         | Configures the output ordering in row or column-major format.                         |
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
#include "scan.h"

typedef enum {
  CLO_PIPE,
  CLO_COLUMNS,
  CLO_SLOPEOUTPUT,
  CLO_ZEROESOF,
  CLO_OFFSET,
  CLO_MAJOR_ORDER,
  N_OPTIONS
} option_type;

char *option[N_OPTIONS] =
  {
    "pipe",
    "columns",
    "slopeoutput",
    "zeroesof",
    "offset",
    "majorOrder",
  };

static char *USAGE =
  "sddszerofind [<inputfile>] [<outputfile>] [-pipe=[input][,output]]\n"
  "-zeroesOf=<columnName> [-columns=<columnNames>] [-offset=<value>] "
  "[-slopeOutput] [-majorOrder=row|column]\n\n"
  "Finds values of columns of data at interpolated zero positions in another\n"
  "column.\n\n"
  "-zeroesOf    Specifies the column for which to find zeroes.\n"
  "-offset      Specifies a value to add to the values of the -zeroesOf column\n"
  "             prior to finding the zeroes. -offset=1 will find places where\n"
  "             the original values are -1.\n"
  "-columns     Specifies the columns to interpolate at the zero positions.\n"
  "             Default is all numerical columns in the file.\n"
  "-majorOrder  Specify output file in row or column order.\n"
  "-slopeOutput Provide output of the slope of each -column column at the zero\n"
  "             position.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define FL_SLOPEOUTPUT 0x00001UL

long resolve_column_names(SDDS_DATASET *SDDSin, char *depen_quantity, char ***indep_quantity, int32_t *indep_quantities);

int main(int argc, char **argv) {
  SDDS_DATASET in_set, out_set;
  SCANNED_ARG *s_arg;
  char *input = NULL, *output = NULL, *zero_name = NULL, **column_name = NULL;
  long i_arg, page_returned;
  int64_t rows, zrow;
  int32_t column_names = 0;
  double **indep_data, *depen_data, **slope_data, slope, offset = 0;
  unsigned long pipe_flags = 0, flags = 0, major_order_flag;
  char s[SDDS_MAXLINE];
  short column_major_order = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2 || argc > (2 + N_OPTIONS)) {
    bomb(NULL, USAGE);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        major_order_flag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&major_order_flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("invalid -majorOrder syntax/values");
        }
        column_major_order = (major_order_flag & SDDS_COLUMN_MAJOR_ORDER) ? 1 : 0;
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags)) {
          SDDS_Bomb("invalid -pipe syntax");
        }
        break;
      case CLO_ZEROESOF:
        if (s_arg[i_arg].n_items != 2) {
          SDDS_Bomb("invalid -zeroesOf syntax");
        }
        zero_name = s_arg[i_arg].list[1];
        break;
      case CLO_COLUMNS:
        if (s_arg[i_arg].n_items < 2) {
          SDDS_Bomb("invalid -columns syntax");
        }
        column_name = tmalloc(sizeof(*column_name) * (column_names = s_arg[i_arg].n_items - 1));
        for (int i = 0; i < column_names; i++) {
          column_name[i] = s_arg[i_arg].list[i + 1];
        }
        break;
      case CLO_SLOPEOUTPUT:
        flags |= FL_SLOPEOUTPUT;
        break;
      case CLO_OFFSET:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%le", &offset) != 1) {
          SDDS_Bomb("invalid -offset syntax");
        }
        break;
      default:
        fprintf(stderr, "Error (%s): unknown/ambiguous option: %s\n", argv[0], s_arg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (input == NULL) {
        input = s_arg[i_arg].list[0];
      } else if (output == NULL) {
        output = s_arg[i_arg].list[0];
      } else {
        SDDS_Bomb("too many filenames");
      }
    }
  }

  processFilenames("sddszerofind", &input, &output, pipe_flags, 0, NULL);

  if (!zero_name) {
    SDDS_Bomb("-zeroesOf option must be given");
  }

  if (!SDDS_InitializeInput(&in_set, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!resolve_column_names(&in_set, zero_name, &column_name, &column_names) ||
      !SDDS_InitializeOutput(&out_set, SDDS_BINARY, 0, NULL, "sddszerofind output", output) ||
      !SDDS_TransferColumnDefinition(&out_set, &in_set, zero_name, NULL)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  out_set.layout.data_mode.column_major = (column_major_order != -1) ? column_major_order : in_set.layout.data_mode.column_major;

  for (int i = 0; i < column_names; i++) {
    snprintf(s, SDDS_MAXLINE, "%sSlope", column_name[i]);
    if (!SDDS_TransferColumnDefinition(&out_set, &in_set, column_name[i], NULL) ||
        (flags & FL_SLOPEOUTPUT && !SDDS_TransferColumnDefinition(&out_set, &in_set, column_name[i], s))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_WriteLayout(&out_set)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  indep_data = tmalloc(sizeof(*indep_data) * column_names);
  slope_data = tmalloc(sizeof(*slope_data) * column_names);

  while ((page_returned = SDDS_ReadPage(&in_set)) > 0) {
    if (!SDDS_StartPage(&out_set, 0)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if ((rows = SDDS_CountRowsOfInterest(&in_set)) > 1) {
      depen_data = SDDS_GetColumnInDoubles(&in_set, zero_name);
      if (!depen_data) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      for (int i = 0; i < column_names; i++) {
        indep_data[i] = SDDS_GetColumnInDoubles(&in_set, column_name[i]);
        if (!indep_data[i]) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }

        if (flags & FL_SLOPEOUTPUT) {
          slope_data[i] = tmalloc(sizeof(**slope_data) * rows);
        }
      }

      if (offset) {
        for (int row = 0; row < rows; row++) {
          depen_data[row] += offset;
        }
      }

      zrow = 0;
      for (int row = 0; row < rows - 1; row++) {
        if ((depen_data[row] <= 0 && depen_data[row + 1] >= 0) ||
            (depen_data[row] >= 0 && depen_data[row + 1] <= 0)) {
          for (int i = 0; i < column_names; i++) {
            if (indep_data[i][row] == indep_data[i][row + 1]) {
              if (flags & FL_SLOPEOUTPUT) {
                slope_data[i][zrow] = DBL_MAX;
              }
              indep_data[i][zrow] = indep_data[i][row];
            } else {
              slope = (depen_data[row + 1] - depen_data[row]) / (indep_data[i][row + 1] - indep_data[i][row]);
              if (flags & FL_SLOPEOUTPUT) {
                slope_data[i][zrow] = slope;
              }
              indep_data[i][zrow] = (slope) ? (indep_data[i][row] - depen_data[row] / slope) : ((indep_data[i][row] + indep_data[i][row + 1]) / 2);
            }
          }
          depen_data[zrow] = -offset;
          zrow++;
        }
      }

      if (zrow) {
        if (!SDDS_LengthenTable(&out_set, zrow) ||
            !SDDS_SetColumnFromDoubles(&out_set, SDDS_SET_BY_NAME, depen_data, zrow, zero_name)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }

        for (int i = 0; i < column_names; i++) {
          snprintf(s, SDDS_MAXLINE, "%sSlope", column_name[i]);
          if (!SDDS_SetColumnFromDoubles(&out_set, SDDS_SET_BY_NAME, indep_data[i], zrow, column_name[i]) ||
              (flags & FL_SLOPEOUTPUT && !SDDS_SetColumnFromDoubles(&out_set, SDDS_SET_BY_NAME, slope_data[i], zrow, s))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
      }

      free(depen_data);
      for (int i = 0; i < column_names; i++) {
        free(indep_data[i]);
      }
      if (flags & FL_SLOPEOUTPUT) {
        for (int i = 0; i < column_names; i++) {
          free(slope_data[i]);
        }
      }
    }

    if (!SDDS_WritePage(&out_set)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_Terminate(&in_set) || !SDDS_Terminate(&out_set)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  return 0;
}

long resolve_column_names(SDDS_DATASET *SDDSin, char *depen_quantity, char ***indep_quantity, int32_t *indep_quantities) {
  long index;
  char s[SDDS_MAXLINE];

  index = SDDS_GetColumnIndex(SDDSin, depen_quantity);
  if (index < 0 || !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(SDDSin, index))) {
    snprintf(s, SDDS_MAXLINE, "column %s is non-existent or non-numeric", depen_quantity);
    SDDS_SetError(s);
    return 0;
  }

  if (*indep_quantities) {
    SDDS_SetColumnFlags(SDDSin, 0);
    for (long i = 0; i < *indep_quantities; i++) {
      if (!SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, (*indep_quantity)[i], SDDS_OR)) {
        return 0;
      }
    }
  } else {
    SDDS_SetColumnFlags(SDDSin, 1);
    if (!SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, depen_quantity, SDDS_NEGATE_MATCH | SDDS_AND)) {
      return 0;
    }
    *indep_quantity = SDDS_GetColumnNames(SDDSin, indep_quantities);
    if (!(*indep_quantity) || *indep_quantities == 0) {
      SDDS_SetError("no independent quantities found");
      return 0;
    }
    for (long i = 0; i < *indep_quantities; i++) {
      index = SDDS_GetColumnIndex(SDDSin, (*indep_quantity)[i]);
      if (!SDDS_NUMERIC_TYPE(SDDS_GetColumnType(SDDSin, index)) &&
          !SDDS_AssertColumnFlags(SDDSin, SDDS_INDEX_LIMITS, index, index, 0)) {
        return 0;
      }
    }
  }

  free(*indep_quantity);
  *indep_quantity = SDDS_GetColumnNames(SDDSin, indep_quantities);
  if (!(*indep_quantity) || *indep_quantities == 0) {
    SDDS_SetError("no independent quantities found");
    return 0;
  }
  return 1;
}
