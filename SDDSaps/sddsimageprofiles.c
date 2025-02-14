/**
 * @file sddsimageprofiles.c
 * @brief Analyze images stored as horizontal lines (one per column) based on the ideas of B-X Yang.
 *
 * @details
 * This program processes image data stored in SDDS (Self Describing Data Set) format,
 * allowing for various profile analyses such as center line, integrated, averaged, or peak profiles.
 * It can handle background subtraction and define specific areas of interest within the image.
 *
 * @section Usage
 * ```
 * sddsimageprofiles [<inputfile>] [<outputfile>]
 *                   [-pipe=[input][,output]]
 *                    -columnPrefix=<prefix>
 *                   [-profileType={x|y}]
 *                   [-method={centerLine|integrated|averaged|peak}]
 *                   [-background=<filename>]
 *                   [-areaOfInterest=<rowStart>,<rowEnd>,<columnStart>,<columnEnd>]
 * ```
 *
 * @section Options
 * | Required | Description                                                                 |
 * |----------|-----------------------------------------------------------------------------|
 * | `-columnPrefix`   | Set the column prefix.                                                               |
 *
 * | Option            | Description                                                                          |
 * |-------------------|--------------------------------------------------------------------------------------|
 * | `-pipe`           | Specify input and/or output via pipe.                                                |
 * | `-profileType`    | Choose profile type: `x` or `y`.                                                     |
 * | `-method`         | Select the method for profile analysis: `centerLine`, `integrated`, `averaged`, `peak`.|
 * | `-background`     | Specify a background image file.                                                     |
 * | `-areaOfInterest` | Define the area of interest within the image.                                        |
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
 *   - R. Soliday
 *   - Xuesong Jiao
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_PIPE,
  SET_PROFILETYPE,
  SET_COLPREFIX,
  SET_METHOD,
  SET_AREAOFINTEREST,
  SET_BACKGROUND,
  N_OPTIONS,
};

char *option[N_OPTIONS] = {
  "pipe", "profileType", "columnPrefix",
  "method", "areaOfInterest", "background"};

char *USAGE =
  "sddsimageprofiles [<inputfile>] [<outputfile>]\n"
  "                  [-pipe=[input][,output]]\n"
  "                   -columnPrefix=<prefix>\n"
  "                  [-profileType={x|y}]\n"
  "                  [-method={centerLine|integrated|averaged|peak}]\n"
  "                  [-background=<filename>]\n"
  "                  [-areaOfInterest=<rowStart>,<rowEnd>,<columnStart>,<columnEnd>]\n"
  "Options:\n"
  "  -pipe=[input][,output]                                     Specify input and/or output via pipe.\n"
  "  -columnPrefix=<prefix>                                     Set the column prefix.\n"
  "  -profileType={x|y}                                         Choose profile type: 'x' or 'y'.\n"
  "  -method={centerLine|integrated|averaged|peak}              Select the method for profile analysis.\n"
  "  -background=<filename>                                     Specify a background image file.\n"
  "  -areaOfInterest=<rowStart>,<rowEnd>,<columnStart>,<columnEnd>  Define the area of interest.\n\n"
  "Program by Robert Soliday. (\"" __DATE__ " " __TIME__ "\", SVN revision: " SVN_VERSION ")\n\n"
  "-method:\n"
  "  If this option is not specified, it is a real profile.\n"
  "  If centerLine is specified, it will find the row with the\n"
  "  greatest integrated profile and display that line only.\n"
  "  If integrated is specified, it will sum all the profiles\n"
  "  together. If averaged is specified, it will divide the sum\n"
  "  of all the profiles by the number of profiles. If peak is\n"
  "  specified, it will find the peak point and display the profile\n"
  "  for that row.\n";

typedef struct {
  short *shortData;
  unsigned short *ushortData;
  int32_t *longData;
  uint32_t *ulongData;
  int64_t *long64Data;
  uint64_t *ulong64Data;
  float *floatData;
  double *doubleData;
} IMAGE_DATA;

int xImageProfile(IMAGE_DATA *data, int32_t *type, int64_t rows, SDDS_DATASET *SDDS_dataset,
                  long method, int64_t x1, int64_t x2, long y1, long y2, long *colIndex, double *colIndex2);

int yImageProfile(IMAGE_DATA *data, int32_t *type, int64_t rows, SDDS_DATASET *SDDS_dataset,
                  long method, int64_t x1, int64_t x2, long y1, long y2, long *colIndex, double *colIndex2);

int64_t xPeakLine(IMAGE_DATA *data, int32_t *type, long *colIndex, int64_t x1, int64_t x2, long y1, long y2);

long yPeakLine(IMAGE_DATA *data, int32_t *type, long *colIndex, int64_t x1, int64_t x2, long y1, long y2);

int64_t xCenterLine(IMAGE_DATA *data, int32_t *type, long *colIndex, int64_t x1, int64_t x2, long y1, long y2);

long yCenterLine(IMAGE_DATA *data, int32_t *type, long *colIndex, int64_t x1, int64_t x2, long y1, long y2);

int64_t GetData(SDDS_DATASET *SDDS_orig, char *input, IMAGE_DATA **data, int32_t **type, long **colIndex,
                double **colIndex2, char *colPrefix, long *validColumns);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_dataset, SDDS_orig, SDDS_bg;
  long i_arg;
  SCANNED_ARG *s_arg;
  IMAGE_DATA *data = NULL, *bg_data = NULL;
  char *input = NULL, *output = NULL, *colPrefix = NULL, *background = NULL;
  long profileType = 1, noWarnings = 0, tmpfile_used = 0, method = 0;
  unsigned long pipeFlags = 0;
  int64_t rows, bg_rows, j;
  long i, validColumns = 0, bg_validColumns = 0;
  int32_t *type = NULL, *bg_type = NULL;

  int64_t rowStart = 1, rowEnd = 0;
  long columnStart = 1, columnEnd = 0;

  long *colIndex, *bg_colIndex;
  double *colIndex2, *bg_colIndex2;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);

  if (argc < 3)
    bomb(NULL, USAGE);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_PROFILETYPE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -profileType syntax");
        if (strcmp("x", s_arg[i_arg].list[1]) == 0)
          profileType = 1;
        if (strcmp("y", s_arg[i_arg].list[1]) == 0)
          profileType = 2;
        break;
      case SET_COLPREFIX:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -columnPrefix syntax");
        colPrefix = s_arg[i_arg].list[1];
        break;
      case SET_METHOD:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -method syntax");
        if ((strncasecmp("centralLine", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0) ||
            (strncasecmp("centerLine", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0))
          method = 1;
        if (strncasecmp("integrated", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0)
          method = 2;
        if (strncasecmp("averaged", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0)
          method = 3;
        if (strncasecmp("peak", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0)
          method = 4;
        break;
      case SET_AREAOFINTEREST:
        if (s_arg[i_arg].n_items != 5)
          SDDS_Bomb("invalid -areaOfInterest syntax");
        if (sscanf(s_arg[i_arg].list[1], "%" SCNd64, &rowStart) != 1 || rowStart <= 0)
          SDDS_Bomb("invalid -areaOfInterest syntax or value");
        if (sscanf(s_arg[i_arg].list[2], "%" SCNd64, &rowEnd) != 1 || rowEnd <= 0)
          SDDS_Bomb("invalid -areaOfInterest syntax or value");
        if (sscanf(s_arg[i_arg].list[3], "%ld", &columnStart) != 1 || columnStart <= 0)
          SDDS_Bomb("invalid -areaOfInterest syntax or value");
        if (sscanf(s_arg[i_arg].list[4], "%ld", &columnEnd) != 1 || columnEnd <= 0)
          SDDS_Bomb("invalid -areaOfInterest syntax or value");
        break;
      case SET_BACKGROUND:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -background syntax");
        background = s_arg[i_arg].list[1];
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
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

  if (colPrefix == NULL) {
    fprintf(stderr, "error: missing columnPrefix\n");
    exit(EXIT_FAILURE);
  }

  processFilenames("sddsimageprofiles", &input, &output, pipeFlags, noWarnings, &tmpfile_used);

  /* Read in the image file */
  rows = GetData(&SDDS_orig, input, &data, &type, &colIndex, &colIndex2, colPrefix, &validColumns);

  if (rows < 0) {
    fprintf(stderr, "error: no rows in image file\n");
    exit(EXIT_FAILURE);
  }

  if (background != NULL) {
    /* Read in the background image file */
    bg_rows = GetData(&SDDS_bg, background, &bg_data, &bg_type, &bg_colIndex, &bg_colIndex2, colPrefix, &bg_validColumns);
    if (rows != bg_rows) {
      fprintf(stderr, "error: background has a different number of rows\n");
      exit(EXIT_FAILURE);
    }
    if (validColumns != bg_validColumns) {
      fprintf(stderr, "error: background has a different number of columns\n");
      exit(EXIT_FAILURE);
    }
    /* Subtract the background from the image file */
    for (i = 0; i < validColumns; i++) {
      if (type[colIndex[i]] != bg_type[bg_colIndex[i]]) {
        fprintf(stderr, "error: column types don't match with background image\n");
        exit(EXIT_FAILURE);
      }
      if (colIndex2[i] != bg_colIndex2[i]) {
        fprintf(stderr, "error: image rows don't match with background image\n");
        exit(EXIT_FAILURE);
      }
      switch (type[colIndex[i]]) {
      case SDDS_SHORT:
        for (j = 0; j < rows; j++)
          data[colIndex[i]].shortData[j] -= bg_data[bg_colIndex[i]].shortData[j];
        break;
      case SDDS_USHORT:
        for (j = 0; j < rows; j++)
          data[colIndex[i]].ushortData[j] -= bg_data[bg_colIndex[i]].ushortData[j];
        break;
      case SDDS_LONG:
        for (j = 0; j < rows; j++)
          data[colIndex[i]].longData[j] -= bg_data[bg_colIndex[i]].longData[j];
        break;
      case SDDS_ULONG:
        for (j = 0; j < rows; j++)
          data[colIndex[i]].ulongData[j] -= bg_data[bg_colIndex[i]].ulongData[j];
        break;
      case SDDS_LONG64:
        for (j = 0; j < rows; j++)
          data[colIndex[i]].long64Data[j] -= bg_data[bg_colIndex[i]].long64Data[j];
        break;
      case SDDS_ULONG64:
        for (j = 0; j < rows; j++)
          data[colIndex[i]].ulong64Data[j] -= bg_data[bg_colIndex[i]].ulong64Data[j];
        break;
      case SDDS_FLOAT:
        for (j = 0; j < rows; j++)
          data[colIndex[i]].floatData[j] -= bg_data[bg_colIndex[i]].floatData[j];
        break;
      case SDDS_DOUBLE:
        for (j = 0; j < rows; j++)
          data[colIndex[i]].doubleData[j] -= bg_data[bg_colIndex[i]].doubleData[j];
        break;
      default:
        continue;
      }
    }
  }

  /* Initialize the output file and define the columns */
  if (!SDDS_InitializeOutput(&SDDS_dataset, SDDS_ASCII, 1, NULL, NULL, output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (profileType == 1) {
    if (SDDS_DefineParameter(&SDDS_dataset, "Zone", NULL, NULL, NULL, NULL, SDDS_STRING, 0) == -1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (SDDS_DefineColumn(&SDDS_dataset, "x", NULL, NULL, NULL, NULL, SDDS_LONG64, 0) == -1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (SDDS_DefineColumn(&SDDS_dataset, "y", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  } else {
    if (SDDS_DefineParameter(&SDDS_dataset, "Zone", NULL, NULL, NULL, NULL, SDDS_STRING, 0) == -1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (SDDS_DefineColumn(&SDDS_dataset, "x", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (SDDS_DefineColumn(&SDDS_dataset, "y", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
  if (SDDS_WriteLayout(&SDDS_dataset) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if ((rowEnd > rows) || (rowEnd < rowStart))
    rowEnd = rows;

  if ((columnEnd > validColumns) || (columnEnd < columnStart))
    columnEnd = validColumns;

  if (profileType == 1) {
    xImageProfile(data, type, rows, &SDDS_dataset, method, rowStart - 1, rowEnd, columnStart - 1, columnEnd, colIndex, colIndex2);
  } else {
    yImageProfile(data, type, rows, &SDDS_dataset, method, rowStart - 1, rowEnd, columnStart - 1, columnEnd, colIndex, colIndex2);
  }

  /* Close the output file */
  if (SDDS_Terminate(&SDDS_dataset) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < validColumns; i++) {
    switch (type[colIndex[i]]) {
    case SDDS_SHORT:
      free(data[colIndex[i]].shortData);
      break;
    case SDDS_USHORT:
      free(data[colIndex[i]].ushortData);
      break;
    case SDDS_LONG:
      free(data[colIndex[i]].longData);
      break;
    case SDDS_ULONG:
      free(data[colIndex[i]].ulongData);
      break;
    case SDDS_LONG64:
      free(data[colIndex[i]].long64Data);
      break;
    case SDDS_ULONG64:
      free(data[colIndex[i]].ulong64Data);
      break;
    case SDDS_FLOAT:
      free(data[colIndex[i]].floatData);
      break;
    case SDDS_DOUBLE:
      free(data[colIndex[i]].doubleData);
      break;
    default:
      continue;
    }
  }
  free(data);
  free(type);
  if (background != NULL) {
    for (i = 0; i < validColumns; i++) {
      switch (bg_type[bg_colIndex[i]]) {
      case SDDS_SHORT:
        free(bg_data[bg_colIndex[i]].shortData);
        break;
      case SDDS_USHORT:
        free(bg_data[bg_colIndex[i]].ushortData);
        break;
      case SDDS_LONG:
        free(bg_data[bg_colIndex[i]].longData);
        break;
      case SDDS_ULONG:
        free(bg_data[bg_colIndex[i]].ulongData);
        break;
      case SDDS_LONG64:
        free(bg_data[bg_colIndex[i]].long64Data);
        break;
      case SDDS_ULONG64:
        free(bg_data[bg_colIndex[i]].ulong64Data);
        break;
      case SDDS_FLOAT:
        free(bg_data[bg_colIndex[i]].floatData);
        break;
      case SDDS_DOUBLE:
        free(bg_data[bg_colIndex[i]].doubleData);
        break;
      default:
        continue;
      }
    }
    free(bg_data);
    free(bg_type);
  }

  return EXIT_SUCCESS;
}

int xImageProfile(IMAGE_DATA *data, int32_t *type, int64_t rows, SDDS_DATASET *SDDS_dataset, long method,
                  int64_t x1, int64_t x2, long y1, long y2, long *colIndex, double *colIndex2) {
  int64_t i, k = 0;
  int j;
  double val = 0;
  long center;
  int64_t *index;
  double *values;
  char value[100];
  index = malloc(sizeof(int64_t) * rows);
  values = malloc(sizeof(double) * rows);

  if (method == 0) { /* Highest point */
    for (i = x1; i < x2; i++) {
      switch (type[colIndex[y1]]) {
      case SDDS_SHORT:
        val = data[colIndex[y1]].shortData[i];
        break;
      case SDDS_USHORT:
        val = data[colIndex[y1]].ushortData[i];
        break;
      case SDDS_LONG:
        val = data[colIndex[y1]].longData[i];
        break;
      case SDDS_ULONG:
        val = data[colIndex[y1]].ulongData[i];
        break;
      case SDDS_LONG64:
        val = data[colIndex[y1]].long64Data[i];
        break;
      case SDDS_ULONG64:
        val = data[colIndex[y1]].ulong64Data[i];
        break;
      case SDDS_FLOAT:
        val = data[colIndex[y1]].floatData[i];
        break;
      case SDDS_DOUBLE:
        val = data[colIndex[y1]].doubleData[i];
        break;
      }
      for (j = y1 + 1; j < y2; j++) {
        switch (type[colIndex[j]]) {
        case SDDS_SHORT:
          if (val < data[colIndex[j]].shortData[i])
            val = data[colIndex[j]].shortData[i];
          break;
        case SDDS_USHORT:
          if (val < data[colIndex[j]].ushortData[i])
            val = data[colIndex[j]].ushortData[i];
          break;
        case SDDS_LONG:
          if (val < data[colIndex[j]].longData[i])
            val = data[colIndex[j]].longData[i];
          break;
        case SDDS_ULONG:
          if (val < data[colIndex[j]].ulongData[i])
            val = data[colIndex[j]].ulongData[i];
          break;
        case SDDS_LONG64:
          if (val < data[colIndex[j]].long64Data[i])
            val = data[colIndex[j]].long64Data[i];
          break;
        case SDDS_ULONG64:
          if (val < data[colIndex[j]].ulong64Data[i])
            val = data[colIndex[j]].ulong64Data[i];
          break;
        case SDDS_FLOAT:
          if (val < data[colIndex[j]].floatData[i])
            val = data[colIndex[j]].floatData[i];
          break;
        case SDDS_DOUBLE:
          if (val < data[colIndex[j]].doubleData[i])
            val = data[colIndex[j]].doubleData[i];
          break;
        }
      }
      index[k] = i + 1;
      values[k] = val;
      k++;
    }
  } else if ((method == 1) || (method == 4)) { /* Center line or peak */
    if (method == 4) {
      center = yPeakLine(data, type, colIndex, x1, x2, y1, y2);
    } else {
      center = yCenterLine(data, type, colIndex, x1, x2, y1, y2);
    }
    for (i = x1; i < x2; i++) {
      switch (type[colIndex[center]]) {
      case SDDS_SHORT:
        val = data[colIndex[center]].shortData[i];
        break;
      case SDDS_USHORT:
        val = data[colIndex[center]].ushortData[i];
        break;
      case SDDS_LONG:
        val = data[colIndex[center]].longData[i];
        break;
      case SDDS_ULONG:
        val = data[colIndex[center]].ulongData[i];
        break;
      case SDDS_LONG64:
        val = data[colIndex[center]].long64Data[i];
        break;
      case SDDS_ULONG64:
        val = data[colIndex[center]].ulong64Data[i];
        break;
      case SDDS_FLOAT:
        val = data[colIndex[center]].floatData[i];
        break;
      case SDDS_DOUBLE:
        val = data[colIndex[center]].doubleData[i];
        break;
      }
      index[k] = i + 1;
      values[k] = val;
      k++;
    }
  } else if ((method == 2) || (method == 3)) { /* Integrated or Averaged profile */
    for (i = x1; i < x2; i++) {
      val = 0;
      for (j = y1; j < y2; j++) {
        switch (type[colIndex[j]]) {
        case SDDS_SHORT:
          val += data[colIndex[j]].shortData[i];
          break;
        case SDDS_USHORT:
          val += data[colIndex[j]].ushortData[i];
          break;
        case SDDS_LONG:
          val += data[colIndex[j]].longData[i];
          break;
        case SDDS_ULONG:
          val += data[colIndex[j]].ulongData[i];
          break;
        case SDDS_LONG64:
          val += data[colIndex[j]].long64Data[i];
          break;
        case SDDS_ULONG64:
          val += data[colIndex[j]].ulong64Data[i];
          break;
        case SDDS_FLOAT:
          val += data[colIndex[j]].floatData[i];
          break;
        case SDDS_DOUBLE:
          val += data[colIndex[j]].doubleData[i];
          break;
        }
      }
      index[k] = i + 1;
      if (method == 2)
        values[k] = val;
      else
        values[k] = val / (y2 - y1);
      k++;
    }
  }

  if (SDDS_StartPage(SDDS_dataset, x2 - x1) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  sprintf(value, "(%" PRId64 ",%ld) x (%" PRId64 ",%ld)", x1 + 1, y1 + 1, x2, y2);
  if (SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "Zone", value, NULL) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (SDDS_SetColumn(SDDS_dataset, SDDS_SET_BY_NAME, index, k, "x", NULL) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (SDDS_SetColumn(SDDS_dataset, SDDS_SET_BY_NAME, values, k, "y", NULL) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (SDDS_WritePage(SDDS_dataset) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  free(index);
  free(values);
  return 0;
}

int yImageProfile(IMAGE_DATA *data, int32_t *type, int64_t rows, SDDS_DATASET *SDDS_dataset, long method,
                  int64_t x1, int64_t x2, long y1, long y2, long *colIndex, double *colIndex2) {
  int i, k = 0;
  int64_t j;
  double val;
  int64_t center;

  double *index;
  double *values;
  char value[100];

  index = malloc(sizeof(double) * (y2 - y1));
  values = malloc(sizeof(double) * (y2 - y1));

  if (method == 0) { /* Highest point */
    for (i = y1; i < y2; i++) {
      switch (type[colIndex[i]]) {
      case SDDS_SHORT:
        val = data[colIndex[i]].shortData[x1];
        break;
      case SDDS_USHORT:
        val = data[colIndex[i]].ushortData[x1];
        break;
      case SDDS_LONG:
        val = data[colIndex[i]].longData[x1];
        break;
      case SDDS_ULONG:
        val = data[colIndex[i]].ulongData[x1];
        break;
      case SDDS_LONG64:
        val = data[colIndex[i]].long64Data[x1];
        break;
      case SDDS_ULONG64:
        val = data[colIndex[i]].ulong64Data[x1];
        break;
      case SDDS_FLOAT:
        val = data[colIndex[i]].floatData[x1];
        break;
      case SDDS_DOUBLE:
        val = data[colIndex[i]].doubleData[x1];
        break;
      default:
        continue;
      }
      for (j = x1 + 1; j < x2; j++) {
        switch (type[colIndex[i]]) {
        case SDDS_SHORT:
          if (val < data[colIndex[i]].shortData[j]) {
            val = data[colIndex[i]].shortData[j];
          }
          break;
        case SDDS_USHORT:
          if (val < data[colIndex[i]].ushortData[j]) {
            val = data[colIndex[i]].ushortData[j];
          }
          break;
        case SDDS_LONG:
          if (val < data[colIndex[i]].longData[j]) {
            val = data[colIndex[i]].longData[j];
          }
          break;
        case SDDS_ULONG:
          if (val < data[colIndex[i]].ulongData[j]) {
            val = data[colIndex[i]].ulongData[j];
          }
          break;
        case SDDS_LONG64:
          if (val < data[colIndex[i]].long64Data[j]) {
            val = data[colIndex[i]].long64Data[j];
          }
          break;
        case SDDS_ULONG64:
          if (val < data[colIndex[i]].ulong64Data[j]) {
            val = data[colIndex[i]].ulong64Data[j];
          }
          break;
        case SDDS_FLOAT:
          if (val < data[colIndex[i]].floatData[j]) {
            val = data[colIndex[i]].floatData[j];
          }
          break;
        case SDDS_DOUBLE:
          if (val < data[colIndex[i]].doubleData[j]) {
            val = data[colIndex[i]].doubleData[j];
          }
          break;
        default:
          continue;
        }
      }
      index[k] = colIndex2[i];
      values[k] = val;
      k++;
    }
  } else if ((method == 1) || (method == 4)) { /* Center line or peak */
    if (method == 4) {
      center = xPeakLine(data, type, colIndex, x1, x2, y1, y2);
    } else {
      center = xCenterLine(data, type, colIndex, x1, x2, y1, y2);
    }
    for (i = y1; i < y2; i++) {
      switch (type[colIndex[i]]) {
      case SDDS_SHORT:
        val = data[colIndex[i]].shortData[center];
        break;
      case SDDS_USHORT:
        val = data[colIndex[i]].ushortData[center];
        break;
      case SDDS_LONG:
        val = data[colIndex[i]].longData[center];
        break;
      case SDDS_ULONG:
        val = data[colIndex[i]].ulongData[center];
        break;
      case SDDS_LONG64:
        val = data[colIndex[i]].long64Data[center];
        break;
      case SDDS_ULONG64:
        val = data[colIndex[i]].ulong64Data[center];
        break;
      case SDDS_FLOAT:
        val = data[colIndex[i]].floatData[center];
        break;
      case SDDS_DOUBLE:
        val = data[colIndex[i]].doubleData[center];
        break;
      default:
        continue;
      }
      index[k] = colIndex2[i];
      values[k] = val;
      k++;
    }
  } else if ((method == 2) || (method == 3)) { /* Integrated or Averaged profile */
    for (i = y1; i < y2; i++) {
      val = 0;
      switch (type[colIndex[i]]) {
      case SDDS_SHORT:
        for (j = x1; j < x2; j++)
          val += data[colIndex[i]].shortData[j];
        break;
      case SDDS_USHORT:
        for (j = x1; j < x2; j++)
          val += data[colIndex[i]].ushortData[j];
        break;
      case SDDS_LONG:
        for (j = x1; j < x2; j++)
          val += data[colIndex[i]].longData[j];
        break;
      case SDDS_ULONG:
        for (j = x1; j < x2; j++)
          val += data[colIndex[i]].ulongData[j];
        break;
      case SDDS_LONG64:
        for (j = x1; j < x2; j++)
          val += data[colIndex[i]].long64Data[j];
        break;
      case SDDS_ULONG64:
        for (j = x1; j < x2; j++)
          val += data[colIndex[i]].ulong64Data[j];
        break;
      case SDDS_FLOAT:
        for (j = x1; j < x2; j++)
          val += data[colIndex[i]].floatData[j];
        break;
      case SDDS_DOUBLE:
        for (j = x1; j < x2; j++)
          val += data[colIndex[i]].doubleData[j];
        break;
      default:
        continue;
      }
      index[k] = colIndex2[i];
      if (method == 2)
        values[k] = val;
      else
        values[k] = val / (x2 - x1);
      k++;
    }
  }

  if (SDDS_StartPage(SDDS_dataset, k) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  sprintf(value, "(%" PRId64 ",%ld) x (%" PRId64 ",%ld)", x1 + 1, y1 + 1, x2, y2);
  if (SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "Zone", value, NULL) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (SDDS_SetColumn(SDDS_dataset, SDDS_SET_BY_NAME, index, k, "y", NULL) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (SDDS_SetColumn(SDDS_dataset, SDDS_SET_BY_NAME, values, k, "x", NULL) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (SDDS_WritePage(SDDS_dataset) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  free(index);
  free(values);
  return 0;
}

int64_t xPeakLine(IMAGE_DATA *data, int32_t *type, long *colIndex, int64_t x1, int64_t x2, long y1, long y2) {
  int64_t i, j;
  double maxValue = 0;
  int64_t index = 0;

  index = x1;
  switch (type[colIndex[y1]]) {
  case SDDS_SHORT:
    maxValue = data[colIndex[y1]].shortData[x1];
    break;
  case SDDS_USHORT:
    maxValue = data[colIndex[y1]].ushortData[x1];
    break;
  case SDDS_LONG:
    maxValue = data[colIndex[y1]].longData[x1];
    break;
  case SDDS_ULONG:
    maxValue = data[colIndex[y1]].ulongData[x1];
    break;
  case SDDS_LONG64:
    maxValue = data[colIndex[y1]].long64Data[x1];
    break;
  case SDDS_ULONG64:
    maxValue = data[colIndex[y1]].ulong64Data[x1];
    break;
  case SDDS_FLOAT:
    maxValue = data[colIndex[y1]].floatData[x1];
    break;
  case SDDS_DOUBLE:
    maxValue = data[colIndex[y1]].doubleData[x1];
    break;
  }
  for (i = y1; i < y2; i++) {
    for (j = x1; j < x2; j++) {
      switch (type[colIndex[i]]) {
      case SDDS_SHORT:
        if (maxValue < data[colIndex[i]].shortData[j]) {
          maxValue = data[colIndex[i]].shortData[j];
          index = j;
        }
        break;
      case SDDS_USHORT:
        if (maxValue < data[colIndex[i]].ushortData[j]) {
          maxValue = data[colIndex[i]].ushortData[j];
          index = j;
        }
        break;
      case SDDS_LONG:
        if (maxValue < data[colIndex[i]].longData[j]) {
          maxValue = data[colIndex[i]].longData[j];
          index = j;
        }
        break;
      case SDDS_ULONG:
        if (maxValue < data[colIndex[i]].ulongData[j]) {
          maxValue = data[colIndex[i]].ulongData[j];
          index = j;
        }
        break;
      case SDDS_LONG64:
        if (maxValue < data[colIndex[i]].long64Data[j]) {
          maxValue = data[colIndex[i]].long64Data[j];
          index = j;
        }
        break;
      case SDDS_ULONG64:
        if (maxValue < data[colIndex[i]].ulong64Data[j]) {
          maxValue = data[colIndex[i]].ulong64Data[j];
          index = j;
        }
        break;
      case SDDS_FLOAT:
        if (maxValue < data[colIndex[i]].floatData[j]) {
          maxValue = data[colIndex[i]].floatData[j];
          index = j;
        }
        break;
      case SDDS_DOUBLE:
        if (maxValue < data[colIndex[i]].doubleData[j]) {
          maxValue = data[colIndex[i]].doubleData[j];
          index = j;
        }
        break;
      }
    }
  }
  return index;
}

long yPeakLine(IMAGE_DATA *data, int32_t *type, long *colIndex, int64_t x1, int64_t x2, long y1, long y2) {
  int i;
  int64_t j;
  double maxValue = 0;
  long index = 0;

  index = y1;
  switch (type[colIndex[y1]]) {
  case SDDS_SHORT:
    maxValue = data[colIndex[y1]].shortData[x1];
    break;
  case SDDS_USHORT:
    maxValue = data[colIndex[y1]].ushortData[x1];
    break;
  case SDDS_LONG:
    maxValue = data[colIndex[y1]].longData[x1];
    break;
  case SDDS_ULONG:
    maxValue = data[colIndex[y1]].ulongData[x1];
    break;
  case SDDS_LONG64:
    maxValue = data[colIndex[y1]].long64Data[x1];
    break;
  case SDDS_ULONG64:
    maxValue = data[colIndex[y1]].ulong64Data[x1];
    break;
  case SDDS_FLOAT:
    maxValue = data[colIndex[y1]].floatData[x1];
    break;
  case SDDS_DOUBLE:
    maxValue = data[colIndex[y1]].doubleData[x1];
    break;
  }
  for (i = y1; i < y2; i++) {
    for (j = x1; j < x2; j++) {
      switch (type[colIndex[i]]) {
      case SDDS_SHORT:
        if (maxValue < data[colIndex[i]].shortData[j]) {
          maxValue = data[colIndex[i]].shortData[j];
          index = i;
        }
        break;
      case SDDS_USHORT:
        if (maxValue < data[colIndex[i]].ushortData[j]) {
          maxValue = data[colIndex[i]].ushortData[j];
          index = i;
        }
        break;
      case SDDS_LONG:
        if (maxValue < data[colIndex[i]].longData[j]) {
          maxValue = data[colIndex[i]].longData[j];
          index = i;
        }
        break;
      case SDDS_ULONG:
        if (maxValue < data[colIndex[i]].ulongData[j]) {
          maxValue = data[colIndex[i]].ulongData[j];
          index = i;
        }
        break;
      case SDDS_LONG64:
        if (maxValue < data[colIndex[i]].long64Data[j]) {
          maxValue = data[colIndex[i]].long64Data[j];
          index = i;
        }
        break;
      case SDDS_ULONG64:
        if (maxValue < data[colIndex[i]].ulong64Data[j]) {
          maxValue = data[colIndex[i]].ulong64Data[j];
          index = i;
        }
        break;
      case SDDS_FLOAT:
        if (maxValue < data[colIndex[i]].floatData[j]) {
          maxValue = data[colIndex[i]].floatData[j];
          index = i;
        }
        break;
      case SDDS_DOUBLE:
        if (maxValue < data[colIndex[i]].doubleData[j]) {
          maxValue = data[colIndex[i]].doubleData[j];
          index = i;
        }
        break;
      }
    }
  }
  return index;
}

int64_t xCenterLine(IMAGE_DATA *data, int32_t *type, long *colIndex, int64_t x1, int64_t x2, long y1, long y2) {
  int64_t i, j, start = 1;
  double val = 0, maxValue = 0;
  int64_t index = 0;

  for (i = x1; i < x2; i++) {
    val = 0;
    for (j = y1; j < y2; j++) {
      switch (type[colIndex[j]]) {
      case SDDS_SHORT:
        val += data[colIndex[j]].shortData[i];
        break;
      case SDDS_USHORT:
        val += data[colIndex[j]].ushortData[i];
        break;
      case SDDS_LONG:
        val += data[colIndex[j]].longData[i];
        break;
      case SDDS_ULONG:
        val += data[colIndex[j]].ulongData[i];
        break;
      case SDDS_LONG64:
        val += data[colIndex[j]].long64Data[i];
        break;
      case SDDS_ULONG64:
        val += data[colIndex[j]].ulong64Data[i];
        break;
      case SDDS_FLOAT:
        val += data[colIndex[j]].floatData[i];
        break;
      case SDDS_DOUBLE:
        val += data[colIndex[j]].doubleData[i];
        break;
      }
    }
    if (start == 1) {
      index = i;
      maxValue = val;
      start = 0;
    } else {
      if (val > maxValue) {
        index = i;
        maxValue = val;
      }
    }
  }
  return index;
}

long yCenterLine(IMAGE_DATA *data, int32_t *type, long *colIndex, int64_t x1, int64_t x2, long y1, long y2) {
  int i, start = 1;
  int64_t j;
  double val, maxValue = 0;
  long index = 0;

  for (i = y1; i < y2; i++) {
    val = 0;
    for (j = x1; j < x2; j++)
      switch (type[colIndex[i]]) {
      case SDDS_SHORT:
        val += data[colIndex[i]].shortData[j];
        break;
      case SDDS_USHORT:
        val += data[colIndex[i]].ushortData[j];
        break;
      case SDDS_LONG:
        val += data[colIndex[i]].longData[j];
        break;
      case SDDS_ULONG:
        val += data[colIndex[i]].ulongData[j];
        break;
      case SDDS_LONG64:
        val += data[colIndex[i]].long64Data[j];
        break;
      case SDDS_ULONG64:
        val += data[colIndex[i]].ulong64Data[j];
        break;
      case SDDS_FLOAT:
        val += data[colIndex[i]].floatData[j];
        break;
      case SDDS_DOUBLE:
        val += data[colIndex[i]].doubleData[j];
        break;
      default:
        continue;
      }
    if (start == 1) {
      index = i;
      maxValue = val;
      start = 0;
    } else {
      if (val > maxValue) {
        index = i;
        maxValue = val;
      }
    }
  }
  return index;
}

int64_t GetData(SDDS_DATASET *SDDS_orig, char *input, IMAGE_DATA **data, int32_t **type, long **colIndex,
                double **colIndex2, char *colPrefix, long *validColumns) {
  int64_t rows;
  long i, j, temp;
  double temp2;
  int32_t orig_column_names;
  char **orig_column_name;

  /* Open image file */
  if (!SDDS_InitializeInput(SDDS_orig, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  /* Read column names */
  if (!(orig_column_name = SDDS_GetColumnNames(SDDS_orig, &orig_column_names))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  /* Allocate memory for image file data */
  *data = malloc(sizeof(IMAGE_DATA) * orig_column_names);
  *type = malloc(sizeof(int32_t) * orig_column_names);

  /* Read page */
  if (SDDS_ReadPage(SDDS_orig) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  /* Get number of rows */
  rows = SDDS_RowCount(SDDS_orig);
  *colIndex = malloc(sizeof(long) * orig_column_names);
  *colIndex2 = malloc(sizeof(double) * orig_column_names);

  /* Read all numerical data from file */
  for (i = 0; i < orig_column_names; i++) {
    (*type)[i] = SDDS_GetNamedColumnType(SDDS_orig, orig_column_name[i]);
    if ((*type)[i] == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }

    switch ((*type)[i]) {
    case SDDS_SHORT:
      (*data)[i].shortData = SDDS_GetColumn(SDDS_orig, orig_column_name[i]);
      break;
    case SDDS_USHORT:
      (*data)[i].ushortData = SDDS_GetColumn(SDDS_orig, orig_column_name[i]);
      break;
    case SDDS_LONG:
      (*data)[i].longData = SDDS_GetColumn(SDDS_orig, orig_column_name[i]);
      break;
    case SDDS_ULONG:
      (*data)[i].ulongData = SDDS_GetColumn(SDDS_orig, orig_column_name[i]);
      break;
    case SDDS_LONG64:
      (*data)[i].long64Data = SDDS_GetColumn(SDDS_orig, orig_column_name[i]);
      break;
    case SDDS_ULONG64:
      (*data)[i].ulong64Data = SDDS_GetColumn(SDDS_orig, orig_column_name[i]);
      break;
    case SDDS_FLOAT:
      (*data)[i].floatData = SDDS_GetColumn(SDDS_orig, orig_column_name[i]);
      break;
    case SDDS_DOUBLE:
      (*data)[i].doubleData = SDDS_GetColumn(SDDS_orig, orig_column_name[i]);
      break;
    default:
      continue;
    }
    if (strncmp(colPrefix, orig_column_name[i], strlen(colPrefix)) != 0) {
      continue;
    }
    (*colIndex)[*validColumns] = i;
    *validColumns += 1;
  }

  if (SDDS_Terminate(SDDS_orig) != 1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (*validColumns == 0) {
    fprintf(stderr, "error: no valid columns in image file\n");
    exit(EXIT_FAILURE);
  }
  /* For each valid column read the row from the column name */
  for (i = 0; i < *validColumns; i++) {
    (*colIndex2)[i] = atof(orig_column_name[(*colIndex)[i]] + strlen(colPrefix));
  }
  /* Sort columns by their row value ('row' is not related to the SDDS rows) */
  for (i = 0; i < *validColumns; i++) {
    for (j = i + 1; j < *validColumns; j++) {
      if ((*colIndex2)[j] < (*colIndex2)[i]) {
        temp = (*colIndex)[i];
        temp2 = (*colIndex2)[i];
        (*colIndex)[i] = (*colIndex)[j];
        (*colIndex2)[i] = (*colIndex2)[j];
        (*colIndex)[j] = temp;
        (*colIndex2)[j] = temp2;
      }
    }
  }
  return rows;
}
