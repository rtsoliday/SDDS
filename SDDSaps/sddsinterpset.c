/**
 * @file sddsinterpset.c
 * @brief Perform multiple interpolations on SDDS data sets.
 *
 * @details
 * This program reads an input SDDS file containing references to multiple data files.
 * For each referenced data file, it performs interpolation based on specified parameters
 * and writes the results to an output SDDS file. The interpolation behavior can be customized
 * using various command-line options.
 *
 * @section Usage
 * ```
 * sddsinterpset [<input>] [<output>]
 *               [-pipe=[input][,output]]
 *               [-order=<number>]
 *               [-verbose]
 *               [-data=fileColumn=<colName>,interpolate=<colName>,functionof=<colName>,column=<colName> | atValue=<value>]
 *               [-majorOrder=row|column]
 *               [-belowRange={value=<value>|skip|saturate|extrapolate|wrap}[,{abort|warn}]]
 *               [-aboveRange={value=<value>|skip|saturate|extrapolate|wrap}[,{abort|warn}]]
 * ```
 *
 * @section Options
 * | Option              | Description                                                                 |
 * |---------------------|-----------------------------------------------------------------------------|
 * | `-pipe`             | Use standard SDDS Toolkit pipe options for input and output.               |
 * | `-order`            | Specify the order of the polynomials used for interpolation.                |
 * | `-verbose`          | Print detailed processing messages.                                        |
 * | `-data`             | Define data interpolation parameters.                                      |
 * | `-majorOrder`       | Specify data ordering for output: `row` or `column`.                       |
 * | `-belowRange`       | Define behavior for out-of-range points below data range.                  |
 * | `-aboveRange`       | Define behavior for out-of-range points above data range.                  |
 *
 * @subsection Incompatibilities
 * - The `column` and `atValue` options within `-data` are mutually exclusive; only one can be specified per invocation.
 *
 * @copyright
 * - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 * - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * H. Shang, R. Soliday, Xuesong Jiao, M. Borland
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "SDDSutils.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  CLO_ORDER,
  CLO_PIPE,
  CLO_BELOWRANGE,
  CLO_ABOVERANGE,
  CLO_DATA,
  CLO_VERBOSE,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "order",
  "pipe",
  "belowrange",
  "aboverange",
  "data",
  "verbose",
  "majorOrder",
};

static const char *USAGE =
  "Usage: sddsinterpset [<input>] [<output>] \n"
  "                     [-pipe=[input][,output]] \n"
  "                     [-order=<number>] \n"
  "                     [-verbose] \n"
  "                     [-data=fileColumn=<colName>,interpolate=<colName>,functionof=<colName>,\n"
  "                            column=<colName> | atValue=<value>] \n"
  "                     [-majorOrder=row|column] \n"
  "                     [-belowRange={value=<value>|skip|saturate|extrapolate|wrap}[,{abort|warn}]] \n"
  "                     [-aboveRange={value=<value>|skip|saturate|extrapolate|wrap}[,{abort|warn}]]\n\n"
  "Options:\n"
  "  -verbose      Print detailed processing messages.\n"
  "  -pipe         Use standard SDDS Toolkit pipe options for input and output.\n"
  "  -order        Specify the order of the polynomials used for interpolation.\n"
  "                Default is 1 (linear interpolation).\n"
  "  -data         Define data interpolation parameters:\n"
  "                - fileColumn=<colName>   : Column with data file names.\n"
  "                - interpolate=<colName>  : Column to interpolate.\n"
  "                - functionof=<colName>   : Independent variable column name.\n"
  "                - column=<colName>       : Specify interpolation point as a column.\n"
  "                  or\n"
  "                - atValue=<value>        : Specify a fixed interpolation value.\n"
  "  -majorOrder   Specify data ordering for output: 'row' or 'column'.\n"
  "                Default inherits from input file.\n"
  "  -belowRange   Define behavior for interpolation points below data range:\n"
  "                Options: value=<value>, skip, saturate, extrapolate, wrap, abort, warn.\n"
  "  -aboveRange   Define behavior for interpolation points above data range:\n"
  "                Options: value=<value>, skip, saturate, extrapolate, wrap, abort, warn.\n\n"
  "Program by Hairong Shang. ("__DATE__
  " "__TIME__
  ", SVN revision: " SVN_VERSION ")\n";

#define AT_COLUMN 0x00000001
#define AT_VALUE 0x00000002

typedef struct {
  char *fileColumn;
  char *interpCol;
  char *funcOfCol;
  char *atCol;
  char **file;
  int64_t files;
  double atValue;
  double *colValue;
  unsigned long hasdata;
  unsigned long flags;
} DATA_CONTROL;

long checkMonotonicity(double *indepValue, int64_t rows);
void freedatacontrol(DATA_CONTROL *data_control, long dataControls);

int main(int argc, char **argv) {
  int i_arg;
  char *input = NULL, *output = NULL, **interpCol = NULL, **funcOf = NULL;
  long order = 1, dataControls = 0, valid_option = 1, monotonicity;
  //long verbose = 0;
  SCANNED_ARG *s_arg;
  OUTRANGE_CONTROL aboveRange, belowRange;
  DATA_CONTROL *data_control = NULL;
  unsigned long pipeFlags = 0, interpCode = 0, majorOrderFlag;
  SDDS_DATASET SDDSdata, SDDSout, SDDSin;
  double *indepValue = NULL, *depenValue = NULL, **out_depenValue = NULL, atValue = 0;
  int32_t **rowFlag = NULL;
  long valid_data = 0, index, pages = 0;
  int64_t *rows = NULL;
  int64_t i, j, datarows, row;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  aboveRange.flags = belowRange.flags = OUTRANGE_SATURATE;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            !scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                          "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)) {
          SDDS_Bomb("invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;

      case CLO_ORDER:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &order) != 1 ||
            order < 1) {
          SDDS_Bomb("invalid -order syntax/value");
        }
        break;

      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags)) {
          SDDS_Bomb("invalid -pipe syntax");
        }
        break;

      case CLO_ABOVERANGE:
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items < 1 ||
            !scanItemList(&aboveRange.flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "value", SDDS_DOUBLE, &aboveRange.value, 1, OUTRANGE_VALUE,
                          "skip", -1, NULL, 0, OUTRANGE_SKIP,
                          "saturate", -1, NULL, 0, OUTRANGE_SATURATE,
                          "extrapolate", -1, NULL, 0, OUTRANGE_EXTRAPOLATE,
                          "wrap", -1, NULL, 0, OUTRANGE_WRAP,
                          "abort", -1, NULL, 0, OUTRANGE_ABORT,
                          "warn", -1, NULL, 0, OUTRANGE_WARN, NULL)) {
          SDDS_Bomb("invalid -aboveRange syntax/value");
        }
        if ((i = bitsSet(aboveRange.flags & (OUTRANGE_VALUE | OUTRANGE_SKIP | OUTRANGE_SATURATE |
                                             OUTRANGE_EXTRAPOLATE | OUTRANGE_WRAP | OUTRANGE_ABORT))) > 1) {
          SDDS_Bomb("incompatible keywords given for -aboveRange");
        }
        if (i != 1)
          aboveRange.flags |= OUTRANGE_SATURATE;
        break;

      case CLO_VERBOSE:
        //verbose = 1;
        break;

      case CLO_BELOWRANGE:
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items < 1 ||
            !scanItemList(&belowRange.flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "value", SDDS_DOUBLE, &belowRange.value, 1, OUTRANGE_VALUE,
                          "skip", -1, NULL, 0, OUTRANGE_SKIP,
                          "saturate", -1, NULL, 0, OUTRANGE_SATURATE,
                          "extrapolate", -1, NULL, 0, OUTRANGE_EXTRAPOLATE,
                          "wrap", -1, NULL, 0, OUTRANGE_WRAP,
                          "abort", -1, NULL, 0, OUTRANGE_ABORT,
                          "warn", -1, NULL, 0, OUTRANGE_WARN, NULL)) {
          SDDS_Bomb("invalid -belowRange syntax/value");
        }
        if ((i = bitsSet(belowRange.flags & (OUTRANGE_VALUE | OUTRANGE_SKIP | OUTRANGE_SATURATE |
                                             OUTRANGE_EXTRAPOLATE | OUTRANGE_WRAP | OUTRANGE_ABORT))) > 1) {
          SDDS_Bomb("incompatible keywords given for -belowRange");
        }
        if (i != 1)
          belowRange.flags |= OUTRANGE_SATURATE;
        break;

      case CLO_DATA:
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items < 4) {
          SDDS_Bomb("invalid -data syntax");
        }
        data_control = SDDS_Realloc(data_control, sizeof(*data_control) * (dataControls + 1));
        data_control[dataControls].fileColumn = data_control[dataControls].interpCol =
          data_control[dataControls].funcOfCol = data_control[dataControls].atCol = NULL;
        data_control[dataControls].file = NULL;
        data_control[dataControls].files = 0;
        data_control[dataControls].hasdata = 0;
        data_control[dataControls].colValue = NULL;
        data_control[dataControls].flags = 0;

        if (!scanItemList(&data_control[dataControls].flags, s_arg[i_arg].list + 1,
                          &s_arg[i_arg].n_items, 0,
                          "fileColumn", SDDS_STRING, &(data_control[dataControls].fileColumn), 1, 0,
                          "interpolate", SDDS_STRING, &(data_control[dataControls].interpCol), 1, 0,
                          "functionof", SDDS_STRING, &(data_control[dataControls].funcOfCol), 1, 0,
                          "column", SDDS_STRING, &(data_control[dataControls].atCol), 1, AT_COLUMN,
                          "atValue", SDDS_DOUBLE, &(data_control[dataControls].atValue), 1, AT_VALUE, NULL) ||
            !data_control[dataControls].fileColumn ||
            !data_control[dataControls].interpCol ||
            !data_control[dataControls].funcOfCol) {
          SDDS_Bomb("Invalid -data syntax");
        }

        if (!(data_control[dataControls].flags & AT_COLUMN) &&
            !(data_control[dataControls].flags & AT_VALUE)) {
          SDDS_Bomb("Invalid -data syntax: either column or atValue option should be given.");
        }

        if ((data_control[dataControls].flags & AT_COLUMN) &&
            (data_control[dataControls].flags & AT_VALUE)) {
          SDDS_Bomb("Invalid -data syntax: column and atValue options are not compatible.");
        }

        valid_option = 1;
        if (dataControls) {
          if (match_string(data_control[dataControls].funcOfCol, funcOf, dataControls, EXACT_MATCH) > 0) {
            fprintf(stderr, "Multiple independent columns provided!\n");
            exit(EXIT_FAILURE);
          }
          if (match_string(data_control[dataControls].interpCol, interpCol, dataControls, EXACT_MATCH) > 0) {
            fprintf(stderr, "Warning: Interpolate column '%s' has been used.\n", data_control[dataControls].interpCol);
            valid_option = 0;
          }
        }

        if (valid_option) {
          interpCol = SDDS_Realloc(interpCol, sizeof(*interpCol) * (dataControls + 1));
          funcOf = SDDS_Realloc(funcOf, sizeof(*funcOf) * (dataControls + 1));
          interpCol[dataControls] = data_control[dataControls].interpCol;
          funcOf[dataControls] = data_control[dataControls].funcOfCol;
          dataControls++;
        }
        break;

      default:
        fprintf(stderr, "Error: Unknown or ambiguous option '%s'\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (!input)
        input = s_arg[i_arg].list[0];
      else if (!output)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many filenames provided.");
    }
  }

  processFilenames("sddsinterpset", &input, &output, pipeFlags, 0, NULL);

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  for (i = 0; i < dataControls; i++) {
    if ((index = SDDS_GetColumnIndex(&SDDSin, data_control[i].fileColumn)) < 0) {
      fprintf(stderr, "Warning: Column '%s' does not exist in input file '%s'.\n",
              data_control[i].fileColumn, input);
      continue;
    }
    if (SDDS_GetColumnType(&SDDSin, index) != SDDS_STRING) {
      fprintf(stderr, "Error: Column '%s' in input file '%s' is not a string column.\n",
              data_control[i].fileColumn, input);
      continue;
    }
    if (data_control[i].atCol) {
      if ((index = SDDS_GetColumnIndex(&SDDSin, data_control[i].atCol)) < 0) {
        fprintf(stderr, "Warning: Column '%s' does not exist in input file '%s'.\n",
                data_control[i].atCol, input);
        continue;
      }
      if (!SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&SDDSin, index))) {
        fprintf(stderr, "Error: Column '%s' in input file '%s' is not a numeric column.\n",
                data_control[i].atCol, input);
        continue;
      }
    }
    data_control[i].hasdata = 1;
    valid_data++;
  }

  if (!valid_data) {
    fprintf(stderr, "Error: No valid -data options provided for processing.\n");
    exit(EXIT_FAILURE);
  }

  if (!SDDS_InitializeCopy(&SDDSout, &SDDSin, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;

  while (SDDS_ReadPage(&SDDSin) > 0) {
    rows = SDDS_Realloc(rows, sizeof(*rows) * (pages + 1));
    out_depenValue = SDDS_Realloc(out_depenValue, sizeof(*out_depenValue) * (pages + 1));
    rowFlag = SDDS_Realloc(rowFlag, sizeof(*rowFlag) * (pages + 1));

    if (!(rows[pages] = SDDS_CountRowsOfInterest(&SDDSin))) {
      fprintf(stderr, "Error: No data found in input file '%s'.\n", input);
      exit(EXIT_FAILURE);
    }

    rowFlag[pages] = (int32_t *)malloc(sizeof(**rowFlag) * rows[pages]);
    out_depenValue[pages] = (double *)malloc(sizeof(**out_depenValue) * rows[pages]);

    for (i = 0; i < rows[pages]; i++)
      rowFlag[pages][i] = 1;

    for (i = 0; i < dataControls; i++) {
      if (data_control[i].hasdata) {
        data_control[i].files = rows[pages];
        if (!(data_control[i].file = (char **)SDDS_GetColumn(&SDDSin, data_control[i].fileColumn))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }

        if (data_control[i].atCol) {
          if (!(data_control[i].colValue = SDDS_GetColumnInDoubles(&SDDSin, data_control[i].atCol))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }

        if (!data_control[i].atCol)
          atValue = data_control[i].atValue;

        for (j = 0; j < rows[pages]; j++) {
          if (!SDDS_InitializeInput(&SDDSdata, data_control[i].file[j]))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

          switch (SDDS_CheckColumn(&SDDSdata, data_control[i].interpCol, NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
          case SDDS_CHECK_OKAY:
            if (j == (rows[pages] - 1)) {
              if (!pages) {
                if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSdata, data_control[i].interpCol, data_control[i].interpCol))
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            break;
          default:
            fprintf(stderr, "Error: Column '%s' missing or invalid in file '%s'.\n",
                    data_control[i].interpCol, data_control[i].file[j]);
            exit(EXIT_FAILURE);
            break;
          }

          switch (SDDS_CheckColumn(&SDDSdata, data_control[i].funcOfCol, NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
          case SDDS_CHECK_OKAY:
            if (j == (rows[pages] - 1)) {
              if ((!pages) && !(data_control[i].atCol)) {
                if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSdata, data_control[i].funcOfCol, data_control[i].funcOfCol))
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            break;
          default:
            fprintf(stderr, "Error: Column '%s' missing or invalid in file '%s'.\n",
                    data_control[i].funcOfCol, data_control[i].file[j]);
            exit(EXIT_FAILURE);
            break;
          }

          if (SDDS_ReadPage(&SDDSdata) <= 0)
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

          datarows = SDDS_CountRowsOfInterest(&SDDSdata);

          if (!(indepValue = SDDS_GetColumnInDoubles(&SDDSdata, data_control[i].funcOfCol))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          if (!(depenValue = SDDS_GetColumnInDoubles(&SDDSdata, data_control[i].interpCol))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }

          if (!SDDS_Terminate(&SDDSdata))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

          if (!(monotonicity = checkMonotonicity(indepValue, datarows))) {
            fprintf(stderr, "Error: Independent (%s) data in file '%s' is not monotonic.\n",
                    data_control[i].funcOfCol, data_control[i].file[j]);
            exit(EXIT_FAILURE);
          }

          if (data_control[i].atCol)
            atValue = data_control[i].colValue[j];

          out_depenValue[pages][j] = interpolate(depenValue, indepValue, datarows, atValue,
                                                 &belowRange, &aboveRange, order, &interpCode, monotonicity);

          if (interpCode) {
            if (interpCode & OUTRANGE_ABORT) {
              fprintf(stderr, "Error: Value %e out of range for column '%s'.\n",
                      atValue, data_control[i].interpCol);
              exit(EXIT_FAILURE);
            }
            if (interpCode & OUTRANGE_WARN)
              fprintf(stderr, "Warning: Value %e out of range for column '%s'.\n",
                      atValue, data_control[i].interpCol);
            if (interpCode & OUTRANGE_SKIP)
              rowFlag[pages][j] = 0;
          }

          free(depenValue);
          free(indepValue);
        }
      }

      for (j = 0; j < data_control[i].files; j++)
        free(data_control[i].file[j]);
      free(data_control[i].file);
      data_control[i].file = NULL;

      if (data_control[i].colValue) {
        free(data_control[i].colValue);
        data_control[i].colValue = NULL;
      }
    }

    if (!pages) {
      if (!SDDS_WriteLayout(&SDDSout))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!SDDS_StartTable(&SDDSout, rows[pages]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!SDDS_CopyColumns(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!SDDS_CopyParameters(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    for (i = 0; i < dataControls; i++) {
      if (data_control[i].hasdata) {
        if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, out_depenValue[pages],
                                       rows[pages], data_control[i].interpCol)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!(data_control[i].atCol)) {
          for (row = 0; row < rows[pages]; row++) {
            if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row,
                                   data_control[i].funcOfCol, data_control[i].atValue, NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
        }
      }
    }

    if (!SDDS_AssertRowFlags(&SDDSout, SDDS_FLAG_ARRAY, rowFlag[pages], rows[pages]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    pages++;
  }

  if (!SDDS_Terminate(&SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  freedatacontrol(data_control, dataControls);

  if (out_depenValue) {
    for (i = 0; i < pages; i++) {
      free(out_depenValue[i]);
      free(rowFlag[i]);
    }
    free(out_depenValue);
    free(rowFlag);
  }

  if (interpCol)
    free(interpCol);
  if (funcOf)
    free(funcOf);
  if (rows)
    free(rows);

  free_scanargs(&s_arg, argc);
  return EXIT_SUCCESS;
}

long checkMonotonicity(double *indepValue, int64_t rows) {
  if (rows == 1)
    return 1;

  if (indepValue[rows - 1] > indepValue[0]) {
    while (--rows > 0)
      if (indepValue[rows] < indepValue[rows - 1])
        return 0;
    return 1;
  } else {
    while (--rows > 0)
      if (indepValue[rows] > indepValue[rows - 1])
        return 0;
    return -1;
  }
}

void freedatacontrol(DATA_CONTROL *data_control, long dataControls) {
  long i;
  for (i = 0; i < dataControls; i++) {
    free(data_control[i].interpCol);
    free(data_control[i].funcOfCol);
    free(data_control[i].fileColumn);
    if (data_control[i].atCol)
      free(data_control[i].atCol);
  }
  free(data_control);
}
