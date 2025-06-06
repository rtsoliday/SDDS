/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution.
\*************************************************************************/

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#ifndef min
#  define min(a, b) ((a) > (b) ? (b) : (a))
#  define max(a, b) ((a) < (b) ? (b) : (a))
#endif

#define SET_ASCII 0
#define SET_BINARY 1
#define SET_PIPE 2
#define SET_NOWARNINGS 3
#define SET_MULTICOLUMN 4
#define SET_SINGLECOLUMN 5
#define OPTIONS 6
static char *option[OPTIONS] = {
  "ascii", "binary", "pipe", "nowarnings", "multicolumn", "singlecolumn"};

char *USAGE = "sddsimageconvert [<inputfile>] [<outputfile>]\n\
[-pipe=[input][,output]]\n\
[-multicolumn=[indexName=<name>][,prefix=<name>]]\n\
[-singlecolumn=[imageColumn=<name>][,xVariableName=<name>][,yVariableName=<name>]\n\
[-ascii] [-binary]\n\
[-noWarnings]\n\n\
Program by Robert Soliday. ("__DATE__")\n";

typedef struct {
  char *prefix;
  double ymin, ymax;
  long ydim;
  char *indexName;
} MULTICOLUMN_DATA;

typedef struct {
  char *imageColumn;
  char *xprefix, *yprefix;
} SINGLECOLUMN_DATA;

long ConvertSDDSsingleColumnImage(SDDS_DATASET *SDDS_orig, SDDS_DATASET *SDDS_dataset, MULTICOLUMN_DATA multicolumnData, SINGLECOLUMN_DATA singlecolumnData);
long ConvertSDDSmultiColumnImage(SDDS_DATASET *SDDS_orig, SDDS_DATASET *SDDS_dataset, MULTICOLUMN_DATA multicolumnData, SINGLECOLUMN_DATA singlecolumnData);
long CheckForImageType(SDDS_DATASET *SDDS_orig, MULTICOLUMN_DATA *multicolumnData, SINGLECOLUMN_DATA *singlecolumnData);

#define UNKNOWN_IMAGETYPE 0
#define SDDS_SINGLECOLUMN 1
#define SDDS_MULTICOLUMN 2

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_dataset, SDDS_orig;
  SCANNED_ARG *s_arg;
  MULTICOLUMN_DATA multicolumnData;
  SINGLECOLUMN_DATA singlecolumnData;
  long i_arg, binary = 1, noWarnings = 0, tmpfile_used = 0;
  unsigned long pipeFlags = 0;
  char *input = NULL, *output = NULL;
  unsigned long dummyFlags;
  long imageType;

  if (argc < 2) {
    fprintf(stderr, "%s", USAGE);
    return (1);
  }

  multicolumnData.indexName = NULL;
  multicolumnData.prefix = NULL;
  singlecolumnData.imageColumn = NULL;
  singlecolumnData.xprefix = NULL;
  singlecolumnData.yprefix = NULL;

  argc = scanargsg(&s_arg, argc, argv);
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      /* process options here */
      switch (match_string(s_arg[i_arg].list[0], option, OPTIONS, 0)) {
      case SET_ASCII:
        binary = 0;
        break;
      case SET_BINARY:
        binary = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1,
                               s_arg[i_arg].n_items - 1,
                               &pipeFlags)) {
          fprintf(stderr, "Error (%s): invalid -pipe syntax\n", argv[0]);
          return (1);
        }
        break;
      case SET_NOWARNINGS:
        noWarnings = 1;
        break;
      case SET_MULTICOLUMN:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (%s): invalid -multicolumn syntax\n", argv[0]);
          return (1);
        }
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "indexName", SDDS_STRING, &multicolumnData.indexName, 1, 0,
                          "prefix", SDDS_STRING, &multicolumnData.prefix, 1, 0,
                          NULL)) {
          fprintf(stderr, "Error (%s): invalid -multicolumn syntax\n", argv[0]);
          return (1);
        }
        s_arg[i_arg].n_items++;
        break;
      case SET_SINGLECOLUMN:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (%s): invalid -singlecolumn syntax\n", argv[0]);
          return (1);
        }
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "imageColumn", SDDS_STRING, &singlecolumnData.imageColumn, 1, 0,
                          "xVariableName", SDDS_STRING, &singlecolumnData.xprefix, 1, 0,
                          "yVariableName", SDDS_STRING, &singlecolumnData.yprefix, 1, 0,
                          NULL)) {
          fprintf(stderr, "Error (%s): invalid -singlecolumn syntax\n", argv[0]);
          return (1);
        }
        s_arg[i_arg].n_items++;
        break;
      default:
        fprintf(stderr, "Error (%s): unknown switch: %s\n",
                argv[0], s_arg[i_arg].list[0]);
        return (1);
      }
    } else {
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "Error (%s): too many filenames\n", argv[0]);
        return (1);
      }
    }
  }
  if (multicolumnData.indexName == NULL) {
    multicolumnData.indexName = malloc(sizeof(char) * 40);
    sprintf(multicolumnData.indexName, "Index");
  }
  if (multicolumnData.prefix == NULL) {
    multicolumnData.prefix = malloc(sizeof(char) * 40);
    multicolumnData.prefix[0] = 0;
  }
  if (singlecolumnData.imageColumn == NULL) {
    singlecolumnData.imageColumn = malloc(sizeof(char) * 40);
    singlecolumnData.imageColumn[0] = 0;
  }
  if (singlecolumnData.xprefix == NULL) {
    singlecolumnData.xprefix = malloc(sizeof(char) * 40);
    sprintf(singlecolumnData.xprefix, "x");
  }
  if (singlecolumnData.yprefix == NULL) {
    singlecolumnData.yprefix = malloc(sizeof(char) * 40);
    sprintf(singlecolumnData.yprefix, "y");
  }
  processFilenames("sddsimageconvert",
                   &input, &output, pipeFlags,
                   noWarnings, &tmpfile_used);

  if (!SDDS_InitializeInput(&SDDS_orig, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  imageType = CheckForImageType(&SDDS_orig, &multicolumnData, &singlecolumnData);
  if (imageType == UNKNOWN_IMAGETYPE) {
    fprintf(stderr, "Error (%s): Unknown input file type\n", argv[0]);
    if (!SDDS_Terminate(&SDDS_orig)) {
      fprintf(stderr, "Error (%s): Problem closing %s file\n", argv[0], input);
      return (1);
    }
    free_scanargs(&s_arg, argc);
    return (1);
  }
  if (!SDDS_InitializeOutput(&SDDS_dataset, binary ? SDDS_BINARY : SDDS_ASCII, 1, NULL, NULL, output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (imageType == SDDS_MULTICOLUMN) {
    if (ConvertSDDSmultiColumnImage(&SDDS_orig, &SDDS_dataset, multicolumnData, singlecolumnData) == 1) {
      return (1);
    }
  }
  if (imageType == SDDS_SINGLECOLUMN) {
    if (ConvertSDDSsingleColumnImage(&SDDS_orig, &SDDS_dataset, multicolumnData, singlecolumnData) == 1) {
      return (1);
    }
  }
  if (!SDDS_Terminate(&SDDS_orig)) {
    fprintf(stderr, "Problem closing %s file\n", input);
    return (1);
  }
  if (!SDDS_Terminate(&SDDS_dataset)) {
    fprintf(stderr, "Problem closing %s file\n", input);
    return (1);
  }
  if (tmpfile_used && !replaceFileAndBackUp(input, output))
    return (1);
  free_scanargs(&s_arg, argc);
  if (multicolumnData.prefix)
    free(multicolumnData.prefix);
  if (multicolumnData.indexName)
    free(multicolumnData.indexName);
  if (singlecolumnData.imageColumn)
    free(singlecolumnData.imageColumn);
  if (singlecolumnData.xprefix)
    free(singlecolumnData.xprefix);
  if (singlecolumnData.yprefix)
    free(singlecolumnData.yprefix);
  return (0);
}

long CheckForImageType(SDDS_DATASET *SDDS_orig, MULTICOLUMN_DATA *multicolumnData, SINGLECOLUMN_DATA *singlecolumnData) {
  char **columnNames = NULL;
  double buffer;
  char *ptr;
  long n, len = 0, prefixFound = 0;
  int32_t nColumns = 0;
  long sddsMultiColumnImage = 0;
  char charBuffer[40];
  size_t num;

  /*Check for SDDS Single Column Image*/
  if ((SDDS_VerifyParameterExists(SDDS_orig, FIND_SPECIFIED_TYPE, SDDS_STRING, "Variable1Name") >= 0) &&
      (SDDS_VerifyParameterExists(SDDS_orig, FIND_SPECIFIED_TYPE, SDDS_STRING, "Variable2Name") >= 0)) {
    return (SDDS_SINGLECOLUMN);
  }
  /*Check for SDDS Multiple Column Image*/
  if (SDDS_VerifyColumnExists(SDDS_orig, FIND_NUMERIC_TYPE, multicolumnData->indexName) >= 0) {
    columnNames = SDDS_GetColumnNames(SDDS_orig, &nColumns);
    if (multicolumnData->prefix[0] != 0) {
      len = strlen(multicolumnData->prefix);
      prefixFound = 2;
    }
    for (n = 0; n < nColumns; n++) {
      if (strcmp(multicolumnData->indexName, columnNames[n]) == 0)
        continue;
      if (prefixFound == 0) {
        ptr = columnNames[n];
        ptr++;
        while (strlen(ptr) > 0) {
          if (sscanf(ptr, "%lg", &buffer) != 1)
            ptr++;
          else
            break;
        }
        if (strlen(ptr) == 0)
          break;
        /*strlcpy(multicolumnData->prefix, columnNames[n], strlen(columnNames[n]) - strlen(ptr) + 1);*/
        num = strlen(columnNames[n]) - strlen(ptr);
        if (num >= sizeof(charBuffer))
          num = sizeof(charBuffer) - 1;
        memcpy(charBuffer, columnNames[n], num);
        charBuffer[num] = '\0';
        strcpy(multicolumnData->prefix, charBuffer);
        len = strlen(multicolumnData->prefix);
        prefixFound = 1;
        multicolumnData->ydim = 1;
        multicolumnData->ymin = multicolumnData->ymax = buffer;
      } else {
        if (strlen(columnNames[n]) <= len)
          continue;
        if (strncmp(multicolumnData->prefix, columnNames[n], len) != 0)
          continue;
        ptr = columnNames[n];
        ptr += len;
        if (sscanf(ptr, "%lg", &buffer) != 1)
          continue;
        if (prefixFound == 2) {
          prefixFound = 1;
          multicolumnData->ydim = 1;
          multicolumnData->ymin = multicolumnData->ymax = buffer;
        } else {
          sddsMultiColumnImage = 1;
          multicolumnData->ydim++;
          multicolumnData->ymin = min(buffer, multicolumnData->ymin);
          multicolumnData->ymax = max(buffer, multicolumnData->ymax);
        }
      }
    }
    for (n = 0; n < nColumns; n++)
      free(columnNames[n]);
    free(columnNames);
    if (sddsMultiColumnImage)
      return (SDDS_MULTICOLUMN);
  }
  return (UNKNOWN_IMAGETYPE);
}

long ConvertSDDSmultiColumnImage(SDDS_DATASET *SDDS_orig, SDDS_DATASET *SDDS_dataset, MULTICOLUMN_DATA multicolumnData, SINGLECOLUMN_DATA singlecolumnData) {
  double *indexData, *data = NULL, *lineData;
  long n, m, rows, xdim;
  double yinterval, xinterval, xmin, xmax;
  char **dataColumnNames, imageColumn[40];
  char xInterval[40], yInterval[40];
  char xMinimum[40], yMinimum[40];
  char xDimension[40], yDimension[40];

  dataColumnNames = malloc(sizeof(char *) * multicolumnData.ydim);
  yinterval = (multicolumnData.ymax - multicolumnData.ymin) / (multicolumnData.ydim - 1);
  for (n = 0; n < multicolumnData.ydim; n++) {
    dataColumnNames[n] = malloc(sizeof(char) * 40);

    sprintf(dataColumnNames[n], "%s%lg", multicolumnData.prefix, multicolumnData.ymin + yinterval * n);
    if (SDDS_VerifyColumnExists(SDDS_orig, FIND_NUMERIC_TYPE, dataColumnNames[n]) < 0) {
      sprintf(dataColumnNames[n], "%s%02lg", multicolumnData.prefix, multicolumnData.ymin + yinterval * n);
      if (SDDS_VerifyColumnExists(SDDS_orig, FIND_NUMERIC_TYPE, dataColumnNames[n]) < 0) {
        sprintf(dataColumnNames[n], "%s%03lg", multicolumnData.prefix, multicolumnData.ymin + yinterval * n);
        if (SDDS_VerifyColumnExists(SDDS_orig, FIND_NUMERIC_TYPE, dataColumnNames[n]) < 0) {
          sprintf(dataColumnNames[n], "%s%04lg", multicolumnData.prefix, multicolumnData.ymin + yinterval * n);
          if (SDDS_VerifyColumnExists(SDDS_orig, FIND_NUMERIC_TYPE, dataColumnNames[n]) < 0) {
            sprintf(dataColumnNames[n], "%s%05lg", multicolumnData.prefix, multicolumnData.ymin + yinterval * n);
            if (SDDS_VerifyColumnExists(SDDS_orig, FIND_NUMERIC_TYPE, dataColumnNames[n]) < 0) {
              fprintf(stderr, "Error (sddsimageconvert): Unable to find column (%s)\n", dataColumnNames[n]);
              fprintf(stderr, "This is most likely caused by unequal Y intervals\n");
              return (1);
            }
          }
        }
      }
    }
  }
  if (singlecolumnData.imageColumn[0] == 0) {
    sprintf(imageColumn, "%s", multicolumnData.prefix);
  } else {
    sprintf(imageColumn, "%s", singlecolumnData.imageColumn);
  }
  sprintf(xInterval, "%sInterval", singlecolumnData.xprefix);
  sprintf(xMinimum, "%sMinimum", singlecolumnData.xprefix);
  sprintf(xDimension, "%sDimension", singlecolumnData.xprefix);
  sprintf(yInterval, "%sInterval", singlecolumnData.yprefix);
  sprintf(yMinimum, "%sMinimum", singlecolumnData.yprefix);
  sprintf(yDimension, "%sDimension", singlecolumnData.yprefix);
  if ((SDDS_DefineSimpleParameter(SDDS_dataset, "Variable1Name", NULL, SDDS_STRING) == 0) ||
      (SDDS_DefineSimpleParameter(SDDS_dataset, "Variable2Name", NULL, SDDS_STRING) == 0) ||
      (SDDS_DefineSimpleParameter(SDDS_dataset, xInterval, NULL, SDDS_DOUBLE) == 0) ||
      (SDDS_DefineSimpleParameter(SDDS_dataset, xMinimum, NULL, SDDS_DOUBLE) == 0) ||
      (SDDS_DefineSimpleParameter(SDDS_dataset, xDimension, NULL, SDDS_LONG) == 0) ||
      (SDDS_DefineSimpleParameter(SDDS_dataset, yInterval, NULL, SDDS_DOUBLE) == 0) ||
      (SDDS_DefineSimpleParameter(SDDS_dataset, yMinimum, NULL, SDDS_DOUBLE) == 0) ||
      (SDDS_DefineSimpleParameter(SDDS_dataset, yDimension, NULL, SDDS_LONG) == 0) ||
      (SDDS_DefineSimpleColumn(SDDS_dataset, imageColumn, NULL, SDDS_DOUBLE) == 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (SDDS_WriteLayout(SDDS_dataset) == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  while (SDDS_ReadPage(SDDS_orig) > 0) {
    if ((rows = SDDS_RowCount(SDDS_orig)) <= 0)
      continue;
    if (!(indexData = SDDS_GetColumnInDoubles(SDDS_orig, multicolumnData.indexName))) {
      fprintf(stderr, "Error (sddsimageconvert): Problem reading data from %s column\n", multicolumnData.indexName);
      return (1);
    }
    xdim = rows;
    xmin = indexData[0];
    xmax = indexData[xdim - 1];
    for (n = 1; n < xdim; n++) {
      if (xmin > indexData[n]) {
        fprintf(stderr, "Error (sddsimageconvert): %s column is not sorted\n", multicolumnData.indexName);
        return (1);
      }
    }
    for (n = 0; n < xdim - 1; n++) {
      if (xmax < indexData[n]) {
        fprintf(stderr, "Error (sddsimageconvert): %s column is not sorted\n", multicolumnData.indexName);
        return (1);
      }
    }
    xinterval = (xmax - xmin) / (xdim - 1);
    if (SDDS_StartPage(SDDS_dataset, xdim * multicolumnData.ydim) == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    if (SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                           "Variable1Name", singlecolumnData.xprefix,
                           "Variable2Name", singlecolumnData.yprefix,
                           xInterval, xinterval,
                           xMinimum, xmin,
                           xDimension, xdim,
                           yInterval, yinterval,
                           yMinimum, multicolumnData.ymin,
                           yDimension, multicolumnData.ydim, NULL) == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    data = malloc(sizeof(double) * xdim * multicolumnData.ydim);

    for (n = 0; n < multicolumnData.ydim; n++) {
      if (!(lineData = SDDS_GetColumnInDoubles(SDDS_orig, dataColumnNames[n]))) {
        fprintf(stderr, "Error (sddsimageconvert): Problem reading data from %s column\n", dataColumnNames[n]);
        return (1);
      }
      for (m = 0; m < xdim; m++) {
        data[m * multicolumnData.ydim + n] = lineData[m];
      }
      free(lineData);
    }
    if (SDDS_SetColumn(SDDS_dataset, SDDS_SET_BY_NAME, data, xdim * multicolumnData.ydim, imageColumn) == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }

    if (SDDS_WritePage(SDDS_dataset) == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    free(indexData);
    free(data);
  }

  for (n = 0; n < multicolumnData.ydim; n++)
    free(dataColumnNames[n]);
  free(dataColumnNames);

  return (0);
}

long ConvertSDDSsingleColumnImage(SDDS_DATASET *SDDS_orig, SDDS_DATASET *SDDS_dataset, MULTICOLUMN_DATA multicolumnData, SINGLECOLUMN_DATA singlecolumnData) {
  char **columnNames = NULL;
  char imageColumn[40], prefix[40];
  int32_t nColumns = 0;
  char *var1Name = NULL, *var2Name = NULL;
  double xInterval, yInterval, xMinimum, yMinimum;
  int32_t xDimension, yDimension;
  double *data = NULL;
  long rows, definitionsDone = 0;
  double *indexData = NULL, *lineData;
  long n, m, intlength = 0, intindex = 0;
  char **lineColumnNames = NULL, buffer[40];

  if (singlecolumnData.imageColumn[0] == 0) {
    columnNames = SDDS_GetColumnNames(SDDS_orig, &nColumns);
    if (nColumns != 1) {
      fprintf(stderr, "Error (sddsimageconvert): Can't determine which numeric column contains image.\nUse -singlecolumn=imagecolumn=<name>\n");
      return (1);
    }
    sprintf(imageColumn, "%s", columnNames[0]);
    for (n = 0; n < nColumns; n++)
      free(columnNames[n]);
    free(columnNames);
  } else {
    sprintf(imageColumn, "%s", singlecolumnData.imageColumn);
  }
  if (SDDS_VerifyColumnExists(SDDS_orig, FIND_NUMERIC_TYPE,
                              imageColumn) < 0) {
    fprintf(stderr, "Error (sddsimageconvert): Numerical column %s does not exist.\n", imageColumn);
    return (1);
  }
  if (multicolumnData.prefix[0] == 0) {
    sprintf(prefix, "%s", imageColumn);
  } else {
    sprintf(prefix, "%s", multicolumnData.prefix);
  }

  while (SDDS_ReadPage(SDDS_orig) > 0) {
    if ((rows = SDDS_RowCount(SDDS_orig)) <= 0)
      continue;
    if ((SDDS_GetParameterAsString(SDDS_orig, "Variable1Name", &var1Name) == NULL) ||
        (SDDS_GetParameterAsString(SDDS_orig, "Variable2Name", &var2Name) == NULL)) {
      fprintf(stderr, "Error (sddsimageconvert): Problem reading data from parameters Variable1Name and/or Variable2Name\n");
      return (1);
    }
    sprintf(buffer, "%sInterval", var1Name);
    if (SDDS_GetParameterAsDouble(SDDS_orig, buffer, &xInterval) == NULL) {
      fprintf(stderr, "Error (sddsimageconvert): Problem reading data from parameter %s\n", buffer);
      return (1);
    }
    sprintf(buffer, "%sInterval", var2Name);
    if (SDDS_GetParameterAsDouble(SDDS_orig, buffer, &yInterval) == NULL) {
      fprintf(stderr, "Error (sddsimageconvert): Problem reading data from parameter %s\n", buffer);
      return (1);
    }
    sprintf(buffer, "%sMinimum", var1Name);
    if (SDDS_GetParameterAsDouble(SDDS_orig, buffer, &xMinimum) == NULL) {
      fprintf(stderr, "Error (sddsimageconvert): Problem reading data from parameter %s\n", buffer);
      return (1);
    }
    sprintf(buffer, "%sMinimum", var2Name);
    if (SDDS_GetParameterAsDouble(SDDS_orig, buffer, &yMinimum) == NULL) {
      fprintf(stderr, "Error (sddsimageconvert): Problem reading data from parameter %s\n", buffer);
      return (1);
    }
    sprintf(buffer, "%sDimension", var1Name);
    if (SDDS_GetParameterAsLong(SDDS_orig, buffer, &xDimension) == NULL) {
      fprintf(stderr, "Error (sddsimageconvert): Problem reading data from parameter %s\n", buffer);
      return (1);
    }
    sprintf(buffer, "%sDimension", var2Name);
    if (SDDS_GetParameterAsLong(SDDS_orig, buffer, &yDimension) == NULL) {
      fprintf(stderr, "Error (sddsimageconvert): Problem reading data from parameter %s\n", buffer);
      return (1);
    }
    if (definitionsDone == 0) {
      for (n = 0; n < xDimension; n++) {
        if ((int)(n * xInterval + xMinimum) != (n * xInterval + xMinimum)) {
          intindex = 0;
          break;
        }
        intindex = 1;
      }
      if (intindex) {
        if (SDDS_DefineSimpleColumn(SDDS_dataset, multicolumnData.indexName, NULL, SDDS_LONG) == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      } else {
        if (SDDS_DefineSimpleColumn(SDDS_dataset, multicolumnData.indexName, NULL, SDDS_DOUBLE) == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      }
      lineColumnNames = malloc(sizeof(char *) * yDimension);
      for (n = 0; n < yDimension; n++) {
        if (n * yInterval + yMinimum < 0) {
          intlength = 0;
          break;
        }
        if ((int)(n * yInterval + yMinimum) != (n * yInterval + yMinimum)) {
          intlength = 0;
          break;
        }
        intlength = 1;
      }
      if (intlength) {
        sprintf(buffer, "%lg", (yDimension - 1) * yInterval + yMinimum);
        intlength = strlen(buffer);
      }
      for (n = 0; n < yDimension; n++) {
        lineColumnNames[n] = malloc(sizeof(char) * 128);

        if (intlength) {
          snprintf(lineColumnNames[n], 128, "%s%0*ld", prefix, (int)intlength, (long)(n * yInterval + yMinimum));
        } else {
          snprintf(lineColumnNames[n], 128, "%s%g", prefix, n * yInterval + yMinimum);
        }
        if (SDDS_DefineSimpleColumn(SDDS_dataset, lineColumnNames[n], NULL, SDDS_DOUBLE) == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      }
      if (SDDS_WriteLayout(SDDS_dataset) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      definitionsDone = 1;
    }
    if (!(data = SDDS_GetColumnInDoubles(SDDS_orig, imageColumn))) {
      fprintf(stderr, "Error (sddsimageconvert): Problem reading data from %s column\n", imageColumn);
      return (1);
    }
    indexData = malloc(sizeof(double) * xDimension);
    lineData = malloc(sizeof(double) * xDimension);
    if (SDDS_StartPage(SDDS_dataset, xDimension) == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    for (n = 0; n < xDimension; n++) {
      indexData[n] = n * xInterval + xMinimum;
    }
    if (intindex) {
      if (SDDS_SetColumnFromDoubles(SDDS_dataset, SDDS_SET_BY_NAME, indexData, xDimension, multicolumnData.indexName) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
    } else {
      if (SDDS_SetColumn(SDDS_dataset, SDDS_SET_BY_NAME, indexData, xDimension, multicolumnData.indexName) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
    }
    for (n = 0; n < yDimension; n++) {
      for (m = 0; m < xDimension; m++) {
        lineData[m] = data[n + m * yDimension];
      }
      if (SDDS_SetColumn(SDDS_dataset, SDDS_SET_BY_NAME, lineData, xDimension, lineColumnNames[n]) == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
    }
    if (SDDS_WritePage(SDDS_dataset) == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    free(indexData);
    free(lineData);
    free(data);
  }
  if (definitionsDone == 1) {
    for (n = 0; n < yDimension; n++)
      free(lineColumnNames[n]);
    free(lineColumnNames);
  }
  if (var1Name)
    free(var1Name);
  if (var2Name)
    free(var2Name);
  return (0);
}
