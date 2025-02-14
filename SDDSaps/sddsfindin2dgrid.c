/**
 * @file sddsfindin2dgrid.c
 * @brief Searches a 2D grid to find locations based on specified SDDS input data.
 *
 * @details
 * This program processes 2D grid data from an SDDS input file to locate points where specific variables
 * are closest to given target values. It supports both direct and file-based value inputs, optionally
 * applies interpolation to refine the results, and writes output in SDDS format for further use or analysis.
 *
 * @section Usage
 * ```
 * sddsfindin2dgrid [<input>] [<output>] 
 *                  [-pipe=[input][,output]]
 *                  -gridVariables=<gridColumnName1>,<gridColumnName2>
 *                  -findLocationOf=<columnName1>,<columnName2>
 *                  {
 *                   [-valuesFile=<filename>] | 
 *                   [-atValues=<value1>,<value2>]
 *                  }
 *                  [-presorted]
 *                  [-interpolate]
 *                  [-mode={onePairPerPage|reuseFirstPage|all}]
 *                  [-inverse]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-gridVariables`                      | Specifies the two columns representing the grid variables.                            |
 * | `-findLocationOf`                     | Specifies the two columns whose optimal grid values are to be found.                  |
 * | `-valuesFile` or `-atValues`          | Specifies either a file with values or direct values for location finding.            |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use SDDS pipe for input and/or output.                                                |
 * | `-presorted`                          | Indicates that input data is pre-sorted, improving processing efficiency.             |
 * | `-interpolate`                        | Enables linear interpolation for more precise location determination.                 |
 * | `-mode`                               | Sets the processing mode: onePairPerPage, reuseFirstPage, or all.                     |
 * | `-inverse`                            | Performs an inverse operation, finding data values for specified grid inputs.         |
 *
 * @subsection Incompatibilities
 *   - Only one of `-valuesFile` or `-atValues` may be specified.
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
 * M. Borland
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>

/* Enumeration for option types */
enum option_type {
  CLO_FINDLOCATIONOF,
  CLO_GRIDVARIABLES,
  CLO_VALUESFILE,
  CLO_ATVALUES,
  CLO_PIPE,
  CLO_INTERPOLATE,
  CLO_MODE,
  CLO_PRESORTED,
  CLO_INVERSE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "findlocationof",
  "gridvariables",
  "valuesfile",
  "atvalues",
  "pipe",
  "interpolate",
  "mode",
  "presorted",
  "inverse"
};

/* Improved usage message for better readability */
char *USAGE =
  "Usage: sddsfindin2dgrid [<input>] [<output>]\n"
  "                        [-pipe=[input][,output]]\n"
  "                        -gridVariables=<gridColumnName1>,<gridColumnName2>\n"
  "                        -findLocationOf=<columnName1>,<columnName2>\n"
  "                        {-valuesFile=<filename> | -atValues=<value1>,<value2>}\n"
  "                        [-presorted]\n"
  "                        [-interpolate] [-mode={onePairPerPage|reuseFirstPage|all}]\n"
  "                        [-inverse]\n\n"
  "Description:\n"
  "  This program searches a 2D grid to find the location (gridColumnName1, gridColumnName2)\n"
  "  where columnName1 and columnName2 are closest to the given values.\n\n"
  "Options:\n"
  "  -gridVariables    Names the two columns that are laid out on a grid.\n"
  "  -presorted        Data is sorted by grid variables using 'sddssort'.\n"
  "                    Pre-sorting can save considerable time if data is used repeatedly.\n"
  "  -findLocationOf   Names the two columns to locate on the grid by finding optimal values.\n"
  "  -valuesFile       Specifies a file containing pairs of values to find locations for.\n"
  "  -atValues         Directly provides values to be found. This option may be repeated.\n"
  "  -interpolate      Performs 2D linear interpolation to refine the location.\n"
  "  -mode             Determines processing mode:\n"
  "                      onePairPerPage   - One pair per input page (default).\n"
  "                      reuseFirstPage   - Use all pairs with the first input page.\n"
  "                      all              - Use all pairs with all input pages.\n"
  "  -inverse          Performs the inverse operation, interpolating to find values from grid locations.\n\n"
  "Program Information:\n"
  "  Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define MODE_ONEPAIRPERPAGE 0x01UL
#define MODE_REUSEFIRSTPAGE 0x02UL
#define MODE_ALL 0x04UL

static double *gridValue[2] = {NULL, NULL}, *valueAtLocation[2] = {NULL, NULL};
static uint64_t ng[2] = {0, 0};

/* Function prototypes */
void gridifyData(char *gridVariable[2], uint64_t gridPoints, short presorted);
int findLocationInGrid(double at1, double at2, double *location, double *value, short interpolate);

int main(int argc, char **argv) {
  long iArg;
  SDDS_DATASET SDDSin, SDDSout, SDDSvalues;
  SCANNED_ARG *scanned;
  unsigned long pipeFlags;
  uint64_t gridPoints = 0, atValues = 0, iv = 0, irow = 0;
  char *input = NULL, *output = NULL, *fileForValues = NULL;
  char *findLocationOf[2] = {NULL, NULL}, *gridVariable[2] = {NULL, NULL};
  double *atValue[2] = {NULL, NULL}, value[2] = {0.0, 0.0};
  short interpolate = 0, restarted = 0, needPage = 0, presorted = 0, inverse = 0;
  unsigned long mode = MODE_ALL;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scanned, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* Process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_FINDLOCATIONOF:
        if (scanned[iArg].n_items != 3 ||
            !strlen(findLocationOf[0] = scanned[iArg].list[1]) ||
            !strlen(findLocationOf[1] = scanned[iArg].list[2])) {
          SDDS_Bomb("Invalid -findLocationOf syntax.\n");
        }
        if (strcmp(findLocationOf[0], findLocationOf[1]) == 0) {
          SDDS_Bomb("Invalid -findLocationOf values: two variables are the same.\n");
        }
        break;
      case CLO_GRIDVARIABLES:
        if (scanned[iArg].n_items != 3 ||
            !strlen(gridVariable[0] = scanned[iArg].list[1]) ||
            !strlen(gridVariable[1] = scanned[iArg].list[2])) {
          SDDS_Bomb("Invalid -gridVariables syntax.\n");
        }
        if (strcmp(gridVariable[0], gridVariable[1]) == 0) {
          SDDS_Bomb("Invalid -gridVariables values: two variables are the same.\n");
        }
        break;
      case CLO_VALUESFILE:
        if (scanned[iArg].n_items != 2 ||
            !strlen(fileForValues = scanned[iArg].list[1])) {
          SDDS_Bomb("Invalid -valuesFile syntax.\n");
        }
        if (atValues > 0) {
          SDDS_Bomb("Cannot use -valuesFile and -atValues together.\n");
        }
        break;
      case CLO_ATVALUES:
        atValue[0] = SDDS_Realloc(atValue[0], sizeof(double) * (atValues + 1));
        atValue[1] = SDDS_Realloc(atValue[1], sizeof(double) * (atValues + 1));
        if (scanned[iArg].n_items != 3 ||
            sscanf(scanned[iArg].list[1], "%le", &atValue[0][atValues]) != 1 ||
            sscanf(scanned[iArg].list[2], "%le", &atValue[1][atValues]) != 1) {
          SDDS_Bomb("Invalid -atValues syntax.\n");
        }
        if (fileForValues) {
          SDDS_Bomb("Cannot use -valuesFile and -atValues together.\n");
        }
        atValues++;
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags)) {
          SDDS_Bomb("Invalid -pipe syntax.\n");
        }
        break;
      case CLO_INTERPOLATE:
        interpolate = 1;
        break;
      case CLO_PRESORTED:
        presorted = 1;
        break;
      case CLO_MODE:
        mode = 0;
        if ((scanned[iArg].n_items -= 1) != 1 ||
            !scanItemList(&mode, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "onepairperpage", -1, NULL, 0, MODE_ONEPAIRPERPAGE,
                          "reusefirstpage", -1, NULL, 0, MODE_REUSEFIRSTPAGE,
                          "all", -1, NULL, 0, MODE_ALL,
                          NULL) ||
            bitsSet(mode) != 1) {
          SDDS_Bomb("Invalid -mode syntax.\n");
        }
        break;
      case CLO_INVERSE:
        inverse = 1;
        break;
      default:
        fprintf(stderr, "Invalid option: %s\n", scanned[iArg].list[0]);
        fprintf(stderr, "%s", USAGE);
        exit(EXIT_FAILURE);
      }
    } else {
      if (!input) {
        input = scanned[iArg].list[0];
      } else if (!output) {
        output = scanned[iArg].list[0];
      } else {
        SDDS_Bomb("Too many filenames provided.\n");
      }
    }
  }

  if (!findLocationOf[0] || !findLocationOf[1]) {
    SDDS_Bomb("Must provide -findLocationOf option.\n");
  }
  if (!gridVariable[0] || !gridVariable[1]) {
    SDDS_Bomb("Must provide -gridVariables option.\n");
  }
  if (!atValues && !fileForValues) {
    SDDS_Bomb("Must provide either -atValues or -valuesFile option.\n");
  }

  processFilenames("sddsfindin2dgrid", &input, &output, pipeFlags, 0, NULL);

  if (fileForValues) {
    if (!SDDS_InitializeInput(&SDDSvalues, fileForValues)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (SDDS_ReadPage(&SDDSvalues) <= 0) {
      SDDS_Bomb("Unable to read values file.\n");
    }
    if ((atValues = SDDS_RowCount(&SDDSvalues)) > 0) {
      if (inverse) {
        if (!(atValue[0] = SDDS_GetColumnInDoubles(&SDDSvalues, gridVariable[0]))) {
          SDDS_Bomb("Unable to retrieve values of first grid variable in values file.\n");
        }
        if (!(atValue[1] = SDDS_GetColumnInDoubles(&SDDSvalues, gridVariable[1]))) {
          SDDS_Bomb("Unable to retrieve values of second grid variable in values file.\n");
        }
      } else {
        if (!(atValue[0] = SDDS_GetColumnInDoubles(&SDDSvalues, findLocationOf[0]))) {
          SDDS_Bomb("Unable to retrieve values of first findLocationOf variable in values file.\n");
        }
        if (!(atValue[1] = SDDS_GetColumnInDoubles(&SDDSvalues, findLocationOf[1]))) {
          SDDS_Bomb("Unable to retrieve values of second findLocationOf variable in values file.\n");
        }
      }
    }
    if (SDDS_ReadPage(&SDDSvalues) > 0) {
      SDDS_Bomb("Values file contains multiple pages, which is not supported.\n");
    }
    SDDS_Terminate(&SDDSvalues);
    SDDS_ClearErrors();
  }

  if (!SDDS_InitializeInput(&SDDSin, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 0, NULL, NULL, output) ||
      !SDDS_TransferAllParameterDefinitions(&SDDSout, &SDDSin, 0) ||
      !SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, gridVariable[0], NULL) ||
      !SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, gridVariable[1], NULL) ||
      !SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, findLocationOf[0], NULL) ||
      !SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, findLocationOf[1], NULL) ||
      !SDDS_WriteLayout(&SDDSout) ||
      !SDDS_StartPage(&SDDSout, atValues * 1000)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  SDDS_ClearErrors();

  irow = 0;
  iv = 0;
  restarted = 0;
  while (1) {
    double location[2];
    needPage = (irow == 0) ? 1 : 0;
    if (mode == MODE_ONEPAIRPERPAGE) {
      if (iv == atValues) {
        break;
      }
      needPage = 1;
    } else if (mode == MODE_REUSEFIRSTPAGE) {
      if (iv == atValues) {
        break;
      }
      if (iv == 0) {
        needPage = 1;
      }
    } else if (mode == MODE_ALL) {
      if (iv == atValues) {
        needPage = 1;
        iv = 0;
        restarted = 1; /* Signals that SDDS_ReadPage <= 0 is acceptable */
      }
    }

    if (needPage) {
      if (SDDS_ReadPage(&SDDSin) <= 0) {
        if (!restarted) {
          SDDS_Bomb("Too few pages in input file for number of location requests.\n");
        }
        break;
      }
      if (gridValue[0]) {
        free(gridValue[0]);
        free(gridValue[1]);
        free(valueAtLocation[0]);
        free(valueAtLocation[1]);
        gridValue[0] = gridValue[1] = valueAtLocation[0] = valueAtLocation[1] = NULL;
      }
      if ((gridPoints = SDDS_RowCount(&SDDSin)) <= 0) {
        SDDS_Bomb("First page of input file is empty.\n");
      }
      if (!(gridValue[0] = SDDS_GetColumnInDoubles(&SDDSin, gridVariable[0])) ||
          !(gridValue[1] = SDDS_GetColumnInDoubles(&SDDSin, gridVariable[1]))) {
        SDDS_Bomb("Grid variables are missing from input file.\n");
      }
      if (!(valueAtLocation[0] = SDDS_GetColumnInDoubles(&SDDSin, findLocationOf[0])) ||
          !(valueAtLocation[1] = SDDS_GetColumnInDoubles(&SDDSin, findLocationOf[1]))) {
        SDDS_Bomb("Location variables are missing from input file.\n");
      }

      gridifyData(gridVariable, gridPoints, presorted);
    }

    if (inverse) {
      /* Perform ordinary 2D interpolation on the grid */
      double xmin, ymin, xmax, ymax, dx, dy, v1, v2, fx, fy;
      double x, y;
      int64_t ix, iy, ig;

      xmin = gridValue[0][0];
      ymin = gridValue[1][0];
      xmax = gridValue[0][gridPoints - 1];
      ymax = gridValue[1][gridPoints - 1];
      dx = (xmax - xmin) / (ng[0] - 1);
      dy = (ymax - ymin) / (ng[1] - 1);
      x = atValue[0][iv];
      y = atValue[1][iv];
      ix = (x - xmin) / dx;
      if (ix < 0) {
        ix = 0;
      }
      if (ix >= (int64_t)(ng[0] - 1)) {
        ix = ng[0] - 2;
      }
      iy = (y - ymin) / dy;
      if (iy < 0) {
        iy = 0;
      }
      if (iy >= (int64_t)(ng[1] - 1)) {
        iy = ng[1] - 2;
      }
      fx = (x - (ix * dx + xmin)) / dx;
      fy = (y - (iy * dy + ymin)) / dy;

      ig = ix * ng[1] + iy;
      v1 = valueAtLocation[0][ig] * (1 - fx) + valueAtLocation[0][ig + ng[1]] * fx;
      ig = ix * ng[1] + iy + 1;
      v2 = valueAtLocation[0][ig] * (1 - fx) + valueAtLocation[0][ig + ng[1]] * fx;
      location[0] = v1 * (1 - fy) + v2 * fy;
      value[0] = x;

      ig = ix * ng[1] + iy;
      v1 = valueAtLocation[1][ig] * (1 - fx) + valueAtLocation[1][ig + ng[1]] * fx;
      ig = ix * ng[1] + iy + 1;
      v2 = valueAtLocation[1][ig] * (1 - fx) + valueAtLocation[1][ig + ng[1]] * fx;
      location[1] = v1 * (1 - fy) + v2 * fy;
      value[1] = y;
      if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, irow++,
                             0, value[0], 1, value[1], 2, location[0], 3, location[1],
                             -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    } else {
      if (!findLocationInGrid(atValue[0][iv], atValue[1][iv], &location[0], &value[0], interpolate)) {
        fprintf(stderr, "Couldn't find location for %s=%.6le, %s=%.6le\n",
                findLocationOf[0], atValue[0][iv],
                findLocationOf[1], atValue[1][iv]);
        exit(EXIT_FAILURE);
      }
      if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, irow++,
                             0, location[0], 1, location[1], 2, value[0], 3, value[1],
                             -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    iv++;
  }

  if (!SDDS_WritePage(&SDDSout) || !SDDS_Terminate(&SDDSout) ||
      SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (gridValue[0]) {
    free(gridValue[0]);
    free(gridValue[1]);
    free(valueAtLocation[0]);
    free(valueAtLocation[1]);
    gridValue[0] = gridValue[1] = valueAtLocation[0] = valueAtLocation[1] = NULL;
  }
  return EXIT_SUCCESS;
}

double target[2], achieved[2];

double distance(double *position, long *invalid) {
  uint64_t ix, iy;
  double fx, fy;
  double v[2];
  long i;

  if (position[0] < 0 || position[0] >= ng[0] || position[1] < 0 || position[1] >= ng[1]) {
    *invalid = 1;
    printf("Invalid position: %.6le, %.6le for ng=%" PRIu64 ", %" PRIu64 "\n", position[0], position[1],
           ng[0], ng[1]);
    return DBL_MAX;
  }
  *invalid = 0;

  ix = position[0];
  iy = position[1];
  if (ix == (ng[0] - 1)) {
    ix--;
  }
  if (iy == (ng[1] - 1)) {
    iy--;
  }

  fx = position[0] - ix;
  fy = position[1] - iy;

  for (i = 0; i < 2; i++) {
    double v00, v10, v01, v11, v0, v1;
    v00 = valueAtLocation[i][(ix + 0) * ng[1] + (iy + 0)];
    v10 = valueAtLocation[i][(ix + 1) * ng[1] + (iy + 0)];
    v01 = valueAtLocation[i][(ix + 0) * ng[1] + (iy + 1)];
    v11 = valueAtLocation[i][(ix + 1) * ng[1] + (iy + 1)];
    v0 = v00 + (v10 - v00) * fx;
    v1 = v01 + (v11 - v01) * fx;
    v[i] = v0 + (v1 - v0) * fy;
    achieved[i] = v[i];
  }

  return pow(target[0] - v[0], 2) + pow(target[1] - v[1], 2);
}

int findLocationInGrid(double at1, double at2, double *location, double *value, short interpolate) {
  uint64_t ix, iy, j;
  uint64_t ixBest = ng[0] / 2, iyBest = ng[1] / 2;
  double delta, bestDelta = DBL_MAX;
  long i;

  for (ix = 0; ix < ng[0]; ix++) {
    for (iy = 0; iy < ng[1]; iy++) {
      j = ix * ng[1] + iy;
      delta = pow(valueAtLocation[0][j] - at1, 2) + pow(valueAtLocation[1][j] - at2, 2);
      if (delta < bestDelta) {
        location[0] = gridValue[0][j];
        location[1] = gridValue[1][j];
        value[0] = valueAtLocation[0][j];
        value[1] = valueAtLocation[1][j];
        ixBest = ix;
        iyBest = iy;
        bestDelta = delta;
      }
    }
  }

  if (interpolate) {
    double result, start[2], step[2], lower[2], upper[2];
    start[0] = ixBest;
    start[1] = iyBest;
    step[0] = step[1] = 0.1;
    lower[0] = lower[1] = 0;
    upper[0] = ng[0] - 1;
    upper[1] = ng[1] - 1;
    target[0] = at1;
    target[1] = at2;
    if (simplexMin(&result, start, step, lower, upper, NULL, 2, 0, 1e-14, distance,
                   NULL, 1500, 3, 12, 3, 1, 0) >= 0) {
      double a00, a01, a10, a11, a0, a1, fx, fy;
      ix = start[0];
      iy = start[1];
      if (start[0] < 0)
        ix = 0;
      if (ix >= (int64_t)(ng[0] - 1))
        ix = ng[0] - 2;
      if (start[1] < 0)
        iy = 0;
      if (iy >= (int64_t)(ng[1] - 1))
        iy = ng[1] - 2;
      fx = start[0] - ix;
      fy = start[1] - iy;

      for (i = 0; i < 2; i++) {
        a00 = gridValue[i][(ix + 0) * ng[1] + (iy + 0)];
        a10 = gridValue[i][(ix + 1) * ng[1] + (iy + 0)];
        a01 = gridValue[i][(ix + 0) * ng[1] + (iy + 1)];
        a11 = gridValue[i][(ix + 1) * ng[1] + (iy + 1)];
        a0 = a00 + (a10 - a00) * fx;
        a1 = a01 + (a11 - a01) * fx;
        location[i] = a0 + (a1 - a0) * fy;
        value[i] = achieved[i];
      }
    }
  }

  return 1;
}

int compareGridLocations(const void *data1, const void *data2) {
  uint64_t i1 = *((uint64_t *)data1);
  uint64_t i2 = *((uint64_t *)data2);
  double diff = -(gridValue[0][i1] - gridValue[0][i2]);
  if (diff == 0) {
    diff = -(gridValue[1][i1] - gridValue[1][i2]);
  }

  return (diff < 0 ? 1 : (diff > 0 ? -1 : 0));
}

void gridifyData(char *gridVariable[2], uint64_t gridPoints, short presorted) {
  long i;
  char s[256];
  uint64_t j, *index;
  double *buffer;

  for (i = 0; i < 2; i++) {
    double *copy = tmalloc(sizeof(double) * gridPoints);
    memcpy(copy, gridValue[i], gridPoints * sizeof(double));
    qsort((void *)copy, gridPoints, sizeof(double), double_cmpasc);
    ng[i] = 1;
    for (j = 1; j < gridPoints; j++) {
      if (copy[j - 1] != copy[j]) {
        ng[i] += 1;
      }
    }
    free(copy);
    if (ng[i] == gridPoints) {
      snprintf(s, sizeof(s), "Grid variable %s has only unique values.\n", gridVariable[i]);
      SDDS_Bomb(s);
    }
    if (ng[i] == 1) {
      snprintf(s, sizeof(s), "Grid variable %s has only one unique value.\n", gridVariable[i]);
      SDDS_Bomb(s);
    }
  }
  if (ng[0] * ng[1] != gridPoints) {
    snprintf(s, sizeof(s), "Input data does not form a grid (nx = %" PRIu64 ", ny = %" PRIu64 ", rows = %" PRIu64 ")\n",
             ng[0], ng[1], gridPoints);
    SDDS_Bomb(s);
  }

  if (!presorted) {
    /* Sort indices for points to place data in gridded order */
    index = tmalloc(sizeof(uint64_t) * gridPoints);
    for (j = 0; j < gridPoints; j++) {
      index[j] = j;
    }

    qsort((void *)index, gridPoints, sizeof(uint64_t), compareGridLocations);

    /* Copy data into sorted order in the global variables */
    for (i = 0; i < 2; i++) {
      buffer = tmalloc(sizeof(double) * gridPoints);
      for (j = 0; j < gridPoints; j++) {
        buffer[j] = gridValue[i][index[j]];
      }
      free(gridValue[i]);
      gridValue[i] = buffer;

      buffer = tmalloc(sizeof(double) * gridPoints);
      for (j = 0; j < gridPoints; j++) {
        buffer[j] = valueAtLocation[i][index[j]];
      }
      free(valueAtLocation[i]);
      valueAtLocation[i] = buffer;
    }
    free(index);
  }
}
