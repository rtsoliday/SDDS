/**
 * @file SDDS_copy.c
 * @brief This file contains the functions related to copying SDDS data.
 *
 * The SDDS_copy.c file provides functions for copying data from SDDS files.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDS_internal.h"
#if defined(_WIN32)
#  include <fcntl.h>
#  include <io.h>
#  if defined(__BORLANDC__)
#    define _setmode(handle, amode) setmode(handle, amode)
#  endif
#endif

/**
 * Initializes an SDDS_DATASET structure in preparation for copying a data table from another SDDS_DATASET structure.
 *
 * @param SDDS_target Address of SDDS_DATASET structure into which to copy data.
 * @param SDDS_source Address of SDDS_DATASET structure from which to copy data.
 * @param filename A NULL-terminated character string giving a filename to be associated with the new SDDS_DATASET. Typically, the name of a file to which the copied data will be written after modification. Ignored if NULL.
 * @param filemode A NULL-terminated character string giving the fopen file mode to be used to open the file named by filename. Ignored if filename is NULL.
 *
 * @return 1 on success. On failure, returns 0 and records an error message.
 */
int32_t SDDS_InitializeCopy(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, char *filename, char *filemode) {
  char s[SDDS_MAXLINE];
#if defined(zLib)
  char *extension;
#endif

  if (sizeof(gzFile) != sizeof(void *)) {
    SDDS_SetError("gzFile is not the same size as void *, possible corruption of the SDDS_LAYOUT structure");
    return (0);
  }
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_InitializeCopy"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_target, "SDDS_InitializeCopy"))
    return (0);
  if (!SDDS_ZeroMemory((void *)SDDS_target, sizeof(SDDS_DATASET))) {
    SDDS_SetError("Unable to copy layout--can't zero SDDS_DATASET structure (SDDS_InitializeCopy)");
    return (0);
  }
  if (strcmp(filemode, "r") == 0) {
    filemode = FOPEN_READ_MODE;
    SDDS_target->mode = SDDS_READMODE;
  } else if (strcmp(filemode, "w") == 0) {
    filemode = FOPEN_WRITE_MODE;
    SDDS_target->mode = SDDS_WRITEMODE;
  }
  SDDS_target->pagecount_offset = NULL;
  if (!(strcmp(filemode, "r") == 0 || strcmp(filemode, "w") == 0 || strcmp(filemode, "rb") == 0 || strcmp(filemode, "wb") == 0 || strcmp(filemode, "m") == 0)) {
    SDDS_SetError("Programming error--invalid file mode (SDDS_InitializeCopy)");
    return (0);
  }

  SDDS_target->layout.popenUsed = 0;
  SDDS_target->layout.gzipFile = 0;
  SDDS_target->layout.lzmaFile = 0;
  if (filename) {
    if (SDDS_FileIsLocked(filename)) {
      sprintf(s, "unable to open file %s for copy--file is locked (SDDS_InitializeCopy)", filename);
      SDDS_SetError(s);
      return 0;
    }

    if ((extension = strrchr(filename, '.')) && ((strcmp(extension, ".xz") == 0) || (strcmp(extension, ".lzma") == 0))) {
      SDDS_target->layout.lzmaFile = 1;
      if (!filemode) {
        sprintf(s, "Unable to open file %s (SDDS_InitializeCopy)", filename);
        SDDS_SetError(s);
        return (0);
      }
      if (!(SDDS_target->layout.lzmafp = lzma_open(filename, filemode))) {
        sprintf(s, "Unable to open file %s for writing (SDDS_InitializeCopy)", filename);
        SDDS_SetError(s);
        return 0;
      }
      SDDS_target->layout.fp = SDDS_target->layout.lzmafp->fp;
    } else {
      if (!filemode || !(SDDS_target->layout.fp = fopen(filename, filemode))) {
        sprintf(s, "Unable to open file %s (SDDS_InitializeCopy)", filename);
        SDDS_SetError(s);
        return (0);
      }
    }
    if ((strcmp(filemode, "w") == 0 || strcmp(filemode, "wb") == 0) && !SDDS_LockFile(SDDS_target->layout.fp, filename, "SDDS_InitializeCopy"))
      return 0;
    if (!SDDS_CopyString(&SDDS_target->layout.filename, filename)) {
      SDDS_SetError("Memory allocation failure (SDDS_InitializeCopy)");
      return (0);
    }
#if defined(zLib)
    if ((extension = strrchr(filename, '.')) && strcmp(extension, ".gz") == 0) {
      SDDS_target->layout.gzipFile = 1;
      if ((SDDS_target->layout.gzfp = gzdopen(fileno(SDDS_target->layout.fp), filemode)) == NULL) {
        sprintf(s, "Unable to open compressed file %s for writing (SDDS_InitializeCopy)", filename);
        SDDS_SetError(s);
        return 0;
      }
    }
#endif
  } else {
    SDDS_target->layout.filename = NULL;
    SDDS_target->layout.fp = NULL;
    SDDS_target->mode = SDDS_MEMMODE;
    if (filemode) {
      if (strcmp(filemode, "w") == 0 || strcmp(filemode, "wb") == 0)
        SDDS_target->layout.fp = stdout;
      else if (strcmp(filemode, "r") == 0 || strcmp(filemode, "rb") == 0)
        SDDS_target->layout.fp = stdin;

        /*      else if (strcmp(filemode, "m")!=0) {
                  SDDS_SetError("Unknown filemode (SDDS_InitializeCopy)");
                  return(0);
                  } */
#if defined(_WIN32)
      if (strcmp(filemode, "m") != 0) {
        if (_setmode(_fileno(SDDS_target->layout.fp), _O_BINARY) == -1) {
          sprintf(s, "unable to set stdout or stdin to binary mode");
          SDDS_SetError(s);
          return 0;
        }
      }
#endif
    }
  }
  SDDS_target->page_number = SDDS_target->page_started = 0;
  if (!SDDS_CopyLayout(SDDS_target, SDDS_source))
    return (0);
  return (1);
}

/**
 * Appends layout definitions (columns, parameters, associates, arrays) from one SDDS_DATASET to another.
 * Only definitions that do not already exist in the target dataset are added.
 *
 * @param SDDS_target Address of the SDDS_DATASET structure to which layout definitions will be appended.
 * @param SDDS_source Address of the SDDS_DATASET structure from which layout definitions will be taken.
 * @param mode Mode flag (currently unused; can be set to 0).
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_AppendLayout(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, uint32_t mode) {
  SDDS_LAYOUT *source;
  int64_t i;

  if (!SDDS_CheckDataset(SDDS_target, "SDDS_AppendLayout"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_AppendLayout"))
    return (0);
  source = &SDDS_source->layout;
  SDDS_DeferSavingLayout(SDDS_target, 1);

  for (i = 0; i < source->n_columns; i++)
    if (SDDS_GetColumnIndex(SDDS_target, source->column_definition[i].name) < 0 &&
        SDDS_DefineColumn(SDDS_target, source->column_definition[i].name,
                          source->column_definition[i].symbol, source->column_definition[i].units, source->column_definition[i].description, source->column_definition[i].format_string, source->column_definition[i].type, source->column_definition[i].field_length) < 0) {
      SDDS_DeferSavingLayout(SDDS_target, 0);
      SDDS_SetError("Unable to define column (SDDS_AppendLayout)");
      return (0);
    }

  for (i = 0; i < source->n_parameters; i++)
    if (SDDS_GetParameterIndex(SDDS_target, source->parameter_definition[i].name) < 0 &&
        SDDS_DefineParameter(SDDS_target, source->parameter_definition[i].name,
                             source->parameter_definition[i].symbol, source->parameter_definition[i].units, source->parameter_definition[i].description, source->parameter_definition[i].format_string, source->parameter_definition[i].type, source->parameter_definition[i].fixed_value) < 0) {
      SDDS_DeferSavingLayout(SDDS_target, 0);
      SDDS_SetError("Unable to define parameter (SDDS_AppendLayout)");
      return (0);
    }

  for (i = 0; i < source->n_associates; i++)
    if (SDDS_GetAssociateIndex(SDDS_target, source->associate_definition[i].name) < 0 &&
        SDDS_DefineAssociate(SDDS_target, source->associate_definition[i].name, source->associate_definition[i].filename, source->associate_definition[i].path, source->associate_definition[i].description, source->associate_definition[i].contents, source->associate_definition[i].sdds) < 0) {
      SDDS_DeferSavingLayout(SDDS_target, 0);
      SDDS_SetError("Unable to define associate (SDDS_AppendLayout)");
      return (0);
    }

  for (i = 0; i < source->n_arrays; i++)
    if (SDDS_GetArrayIndex(SDDS_target, source->array_definition[i].name) < 0 &&
        SDDS_DefineArray(SDDS_target, source->array_definition[i].name,
                         source->array_definition[i].symbol,
                         source->array_definition[i].units, source->array_definition[i].description,
                         source->array_definition[i].format_string, source->array_definition[i].type, source->array_definition[i].field_length, source->array_definition[i].dimensions, source->array_definition[i].group_name) < 0) {
      SDDS_DeferSavingLayout(SDDS_target, 0);
      SDDS_SetError("Unable to define array (SDDS_AppendLayout)");
      return (0);
    }
  SDDS_DeferSavingLayout(SDDS_target, 0);
  if (!SDDS_SaveLayout(SDDS_target)) {
    SDDS_SetError("Unable to save layout (SDDS_AppendLayout)");
    return (0);
  }
  return (1);
}

/**
 * Copies the entire layout (including version, data mode, description, contents, columns, parameters, associates, and arrays) from one SDDS_DATASET to another.
 * The target dataset's existing layout will be replaced.
 *
 * @param SDDS_target Address of the SDDS_DATASET structure into which the layout will be copied.
 * @param SDDS_source Address of the SDDS_DATASET structure from which the layout will be copied.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyLayout(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source) {
  SDDS_LAYOUT *target, *source;
  int64_t i;

  if (!SDDS_CheckDataset(SDDS_target, "SDDS_CopyLayout"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_CopyLayout"))
    return (0);
  target = &SDDS_target->layout;
  source = &SDDS_source->layout;
  target->version = source->version;
  target->data_mode = source->data_mode;
  target->data_mode.no_row_counts = 0;
  target->data_mode.fixed_row_count = 0;
  target->data_mode.column_memory_mode = DEFAULT_COLUMN_MEMORY_MODE;
  target->layout_written = 0;
  target->byteOrderDeclared = 0;
  if (source->description)
    SDDS_CopyString(&target->description, source->description);
  if (source->contents)
    SDDS_CopyString(&target->contents, source->contents);
  SDDS_DeferSavingLayout(SDDS_target, 1);
  for (i = 0; i < source->n_columns; i++)
    if (SDDS_DefineColumn(SDDS_target, source->column_definition[i].name, source->column_definition[i].symbol,
                          source->column_definition[i].units, source->column_definition[i].description, source->column_definition[i].format_string, source->column_definition[i].type, source->column_definition[i].field_length) < 0) {
      SDDS_SetError("Unable to define column (SDDS_CopyLayout)");
      return (0);
    }
  for (i = 0; i < source->n_parameters; i++)
    if (SDDS_DefineParameter(SDDS_target, source->parameter_definition[i].name, source->parameter_definition[i].symbol,
                             source->parameter_definition[i].units, source->parameter_definition[i].description, source->parameter_definition[i].format_string, source->parameter_definition[i].type, source->parameter_definition[i].fixed_value) < 0) {
      SDDS_SetError("Unable to define parameter (SDDS_CopyLayout)");
      return (0);
    }

  for (i = 0; i < source->n_associates; i++)
    if (SDDS_DefineAssociate(SDDS_target, source->associate_definition[i].name, source->associate_definition[i].filename, source->associate_definition[i].path, source->associate_definition[i].description, source->associate_definition[i].contents, source->associate_definition[i].sdds) < 0) {
      SDDS_SetError("Unable to define associate (SDDS_CopyLayout)");
      return (0);
    }

  for (i = 0; i < source->n_arrays; i++)
    if (SDDS_DefineArray(SDDS_target, source->array_definition[i].name, source->array_definition[i].symbol,
                         source->array_definition[i].units, source->array_definition[i].description,
                         source->array_definition[i].format_string, source->array_definition[i].type, source->array_definition[i].field_length, source->array_definition[i].dimensions, source->array_definition[i].group_name) < 0) {
      SDDS_SetError("Unable to define array (SDDS_CopyLayout)");
      return (0);
    }
  SDDS_DeferSavingLayout(SDDS_target, 0);
  if (!SDDS_SaveLayout(SDDS_target)) {
    SDDS_SetError("Unable to save layout (SDDS_CopyLayout)");
    return (0);
  }
  return (1);
}

/**
 * Copies parameter values from one SDDS_DATASET structure into another for parameters with matching names.
 *
 * @param SDDS_target Address of the SDDS_DATASET structure into which parameter values will be copied.
 * @param SDDS_source Address of the SDDS_DATASET structure from which parameter values will be copied.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyParameters(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source) {
  int32_t i, target_index;
  char *buffer = NULL; /* will be sized to hold any SDDS data type with room to spare */
  char messageBuffer[1024];

  if (!buffer && !(buffer = SDDS_Malloc(sizeof(char) * 16))) {
    SDDS_SetError("Allocation failure (SDDS_CopyParameters)");
    return 0;
  }

  if (!SDDS_CheckDataset(SDDS_target, "SDDS_CopyParameters"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_CopyParameters"))
    return (0);

  for (i = 0; i < SDDS_source->layout.n_parameters; i++) {
    if ((target_index = SDDS_GetParameterIndex(SDDS_target, SDDS_source->layout.parameter_definition[i].name)) < 0)
      continue;
    if (SDDS_source->layout.parameter_definition[i].type != SDDS_target->layout.parameter_definition[target_index].type) {
      if (!SDDS_NUMERIC_TYPE(SDDS_source->layout.parameter_definition[i].type) || !SDDS_NUMERIC_TYPE(SDDS_target->layout.parameter_definition[target_index].type)) {
        sprintf(messageBuffer, "Can't cast between nonnumeric types for parameters %s and %s (SDDS_CopyParameters)", SDDS_source->layout.parameter_definition[i].name, SDDS_target->layout.parameter_definition[target_index].name);
        SDDS_SetError(messageBuffer);
        return 0;
      }
      if (!SDDS_SetParameters(SDDS_target, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, target_index, SDDS_CastValue(SDDS_source->parameter[i], 0, SDDS_source->layout.parameter_definition[i].type, SDDS_target->layout.parameter_definition[target_index].type, buffer), -1)) {
        sprintf(messageBuffer, "Error setting parameter with cast value for parameters %s and %s (SDDS_CopyParameters)", SDDS_source->layout.parameter_definition[i].name, SDDS_target->layout.parameter_definition[target_index].name);
        SDDS_SetError(messageBuffer);
        return 0;
      }
    } else if (!SDDS_SetParameters(SDDS_target, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, target_index, SDDS_source->parameter[i], -1)) {
      sprintf(messageBuffer, "Unable to copy parameters for parameters %s and %s (SDDS_CopyParameters)", SDDS_source->layout.parameter_definition[i].name, SDDS_target->layout.parameter_definition[target_index].name);
      SDDS_SetError(messageBuffer);
      return (0);
    }
  }
  if (buffer)
    free(buffer);
  return (1);
}

/**
 * Copies array data from one SDDS_DATASET structure into another for arrays with matching names.
 *
 * @param SDDS_target Address of the SDDS_DATASET structure into which array data will be copied.
 * @param SDDS_source Address of the SDDS_DATASET structure from which array data will be copied.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyArrays(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source) {
  int32_t i, j, target_index;
  char messageBuffer[1024];

  for (i = 0; i < SDDS_source->layout.n_arrays; i++) {
    if ((target_index = SDDS_GetArrayIndex(SDDS_target, SDDS_source->layout.array_definition[i].name)) < 0)
      continue;
    SDDS_target->array[target_index].definition = SDDS_target->layout.array_definition + target_index;
    SDDS_target->array[target_index].elements = SDDS_source->array[i].elements;
    if (!(SDDS_target->array[target_index].dimension = (int32_t *)SDDS_Malloc(sizeof(*SDDS_target->array[i].dimension) * SDDS_target->array[target_index].definition->dimensions)) ||
        !(SDDS_target->array[target_index].data = SDDS_Realloc(SDDS_target->array[target_index].data, SDDS_type_size[SDDS_target->array[target_index].definition->type - 1] * SDDS_target->array[target_index].elements))) {
      SDDS_SetError("Unable to copy arrays--allocation failure (SDDS_CopyArrays)");
      return (0);
    }

    for (j = 0; j < SDDS_target->array[target_index].definition->dimensions; j++)
      SDDS_target->array[target_index].dimension[j] = SDDS_source->array[i].dimension[j];
    if (!SDDS_source->array[i].data) {
      SDDS_target->array[target_index].data = NULL;
      continue;
    }
    if (SDDS_source->layout.array_definition[i].type != SDDS_target->layout.array_definition[target_index].type) {
      if (!SDDS_NUMERIC_TYPE(SDDS_source->layout.array_definition[i].type) || !SDDS_NUMERIC_TYPE(SDDS_target->layout.array_definition[target_index].type)) {
        sprintf(messageBuffer, "Can't cast between nonnumeric types for parameters %s and %s (SDDS_CopyArrays)", SDDS_source->layout.array_definition[i].name, SDDS_target->layout.array_definition[target_index].name);
        SDDS_SetError(messageBuffer);
        return 0;
      }
      for (j = 0; j < SDDS_source->array[i].elements; j++) {
        if (!SDDS_CastValue(SDDS_source->array[i].data, j, SDDS_source->layout.array_definition[i].type, SDDS_target->layout.array_definition[target_index].type, (char *)(SDDS_target->array[target_index].data) + j * SDDS_type_size[SDDS_target->layout.array_definition[target_index].type - 1])) {
          SDDS_SetError("Problem with cast (SDDS_CopyArrays)");
          return 0;
        }
      }
    } else {
      if (SDDS_target->array[target_index].definition->type != SDDS_STRING)
        memcpy(SDDS_target->array[target_index].data, SDDS_source->array[i].data, SDDS_type_size[SDDS_target->array[target_index].definition->type - 1] * SDDS_target->array[target_index].elements);
      else if (!SDDS_CopyStringArray(SDDS_target->array[target_index].data, SDDS_source->array[i].data, SDDS_target->array[target_index].elements)) {
        SDDS_SetError("Unable to copy arrays (SDDS_CopyArrays)");
        return (0);
      }
    }
  }
  return (1);
}

/**
 * Copies column data from one SDDS_DATASET structure into another for columns with matching names.
 *
 * @param SDDS_target Address of the SDDS_DATASET structure into which column data will be copied.
 * @param SDDS_source Address of the SDDS_DATASET structure from which column data will be copied.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyColumns(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source) {
  int64_t i, j;
  int32_t target_index;
  SDDS_target->n_rows = 0;
  if (SDDS_target->layout.n_columns && SDDS_target->n_rows_allocated < SDDS_source->n_rows) {
    SDDS_SetError("Unable to copy columns--insufficient memory allocated to target table");
    return (0);
  }
  if (!SDDS_target->layout.n_columns)
    return 1;
  for (i = 0; i < SDDS_source->layout.n_columns; i++) {
    if ((target_index = SDDS_GetColumnIndex(SDDS_target, SDDS_source->layout.column_definition[i].name)) < 0)
      continue;
    if (SDDS_source->layout.column_definition[i].type != SDDS_STRING) {
      if (SDDS_source->layout.column_definition[i].type == SDDS_target->layout.column_definition[target_index].type)
        memcpy(SDDS_target->data[target_index], SDDS_source->data[i], SDDS_type_size[SDDS_source->layout.column_definition[i].type - 1] * SDDS_source->n_rows);
      else {
        /* Do a cast between the source and target types, if they are both numeric */
        if (!SDDS_NUMERIC_TYPE(SDDS_source->layout.column_definition[i].type) || !SDDS_NUMERIC_TYPE(SDDS_target->layout.column_definition[target_index].type)) {
          SDDS_SetError("Can't cast between nonnumeric types (SDDS_CopyColumns)");
          return 0;
        }
        for (j = 0; j < SDDS_source->n_rows; j++) {
          if (!SDDS_CastValue(SDDS_source->data[i], j, SDDS_source->layout.column_definition[i].type, SDDS_target->layout.column_definition[target_index].type, (char *)(SDDS_target->data[target_index]) + j * SDDS_type_size[SDDS_target->layout.column_definition[target_index].type - 1])) {
            SDDS_SetError("Problem with cast (SDDS_CopyColumns)");
            return 0;
          }
        }
      }
    } else if (!SDDS_CopyStringArray(SDDS_target->data[target_index], SDDS_source->data[i], SDDS_source->n_rows)) {
      SDDS_SetError("Unable to copy columns (SDDS_CopyColumns)");
      return (0);
    }
    SDDS_target->column_flag[target_index] = 1;
    SDDS_target->column_order[target_index] = target_index;
  }
  SDDS_target->n_rows = SDDS_source->n_rows;
  if (SDDS_target->row_flag)
    for (i = 0; i < SDDS_target->n_rows; i++)
      SDDS_target->row_flag[i] = 1;
  return (1);
}

/**
 * Copies rows of interest from the source SDDS_DATASET to the target SDDS_DATASET for columns with matching names.
 * Rows of interest are those that have their row flags set in the source dataset.
 *
 * @param SDDS_target Address of the SDDS_DATASET structure into which rows will be copied.
 * @param SDDS_source Address of the SDDS_DATASET structure from which rows will be copied.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyRowsOfInterest(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source) {
  int64_t i, j, k;
  int32_t size, target_index;
  /*  int32_t rows; */
  char buffer[1024];
  int64_t *rowList, roi;
  k = 0;

  if (!SDDS_target->layout.n_columns)
    return 1;
  roi = SDDS_CountRowsOfInterest(SDDS_source);
  if (roi > SDDS_target->n_rows_allocated) {
    SDDS_SetError("Unable to copy rows of interest--insufficient memory allocated to target page (SDDS_CopyRowsOfInterest)");
    return 0;
  }

  rowList = malloc(sizeof(*rowList) * roi);
  k = 0;
  for (j = 0; j < SDDS_source->n_rows; j++) {
    if (SDDS_source->row_flag[j]) {
      rowList[k] = j;
      k++;
    }
  }

  for (i = 0; i < SDDS_source->layout.n_columns; i++) {
    if ((target_index = SDDS_GetColumnIndex(SDDS_target, SDDS_source->layout.column_definition[i].name)) < 0)
      continue;
    if (SDDS_source->layout.column_definition[i].type != SDDS_STRING) {
      if (SDDS_source->layout.column_definition[i].type == SDDS_target->layout.column_definition[target_index].type) {
        size = SDDS_type_size[SDDS_source->layout.column_definition[i].type - 1];
        for (k = 0; k < roi; k++) {
          memcpy((char *)SDDS_target->data[target_index] + k * size, (char *)SDDS_source->data[i] + rowList[k] * size, SDDS_type_size[SDDS_source->layout.column_definition[i].type - 1]);
        }
      } else {
        for (k = 0; k < roi; k++) {
          if (!SDDS_CastValue(SDDS_source->data[i], rowList[k],
                              SDDS_source->layout.column_definition[i].type, SDDS_target->layout.column_definition[target_index].type, (char *)(SDDS_target->data[target_index]) + k * SDDS_type_size[SDDS_target->layout.column_definition[target_index].type - 1])) {
            sprintf(buffer, "Problem with cast for column %s (SDDS_CopyRowsOfInterest)", SDDS_source->layout.column_definition[i].name);
            SDDS_SetError(buffer);
            return 0;
          }
        }
      }
    } else {
      if (SDDS_source->layout.column_definition[i].type != SDDS_target->layout.column_definition[target_index].type) {
        sprintf(buffer, "Unable to copy columns---inconsistent data types for %s (SDDS_CopyRowsOfInterest)", SDDS_source->layout.column_definition[i].name);
        SDDS_SetError(buffer);
        return (0);
      }
      for (k = 0; k < roi; k++) {
        if (((char **)SDDS_target->data[target_index])[k])
          free(((char **)SDDS_target->data[target_index])[k]);
        if (!SDDS_CopyString(&((char **)SDDS_target->data[target_index])[k], ((char **)SDDS_source->data[i])[rowList[k]])) {
          SDDS_SetError("Unable to copy rows (SDDS_CopyRowsOfInterest)");
          return (0);
        }
      }
    }
    SDDS_target->column_flag[target_index] = 1;
    SDDS_target->column_order[target_index] = target_index;
  }
  free(rowList);
  SDDS_target->n_rows = roi;
  if (SDDS_target->row_flag)
    for (i = 0; i < SDDS_target->n_rows; i++)
      SDDS_target->row_flag[i] = 1;

  return (1);
}

/**
 * Copies additional rows from one SDDS_DATASET to another.
 * The rows from SDDS_source are appended to the existing rows in SDDS_target.
 *
 * @param SDDS_target Pointer to the SDDS_DATASET structure where rows will be appended.
 * @param SDDS_source Pointer to the SDDS_DATASET structure from which rows will be copied.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyAdditionalRows(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source) {
  int64_t i, j, sum;
  int32_t size, target_index;
  char buffer[1024];

  if (SDDS_target->n_rows_allocated < (sum = SDDS_target->n_rows + SDDS_source->n_rows) && !SDDS_LengthenTable(SDDS_target, sum - SDDS_target->n_rows_allocated)) {
    SDDS_SetError("Unable to copy additional rows (SDDS_CopyAdditionalRows)");
    return (0);
  }
  if (SDDS_target->layout.n_columns == 0)
    return 1;
  for (i = 0; i < SDDS_source->layout.n_columns; i++) {
    if ((target_index = SDDS_GetColumnIndex(SDDS_target, SDDS_source->layout.column_definition[i].name)) < 0)
      continue;
    size = SDDS_GetTypeSize(SDDS_source->layout.column_definition[i].type);
    if (SDDS_source->layout.column_definition[i].type != SDDS_STRING) {
      if (SDDS_source->layout.column_definition[i].type == SDDS_target->layout.column_definition[target_index].type) {
        memcpy((char *)SDDS_target->data[target_index] + size * SDDS_target->n_rows, SDDS_source->data[i], SDDS_type_size[SDDS_source->layout.column_definition[i].type - 1] * SDDS_source->n_rows);
      } else {
        for (j = 0; j < SDDS_source->n_rows; j++) {
          if (!SDDS_CastValue(SDDS_source->data[i], j,
                              SDDS_source->layout.column_definition[i].type, SDDS_target->layout.column_definition[target_index].type, (char *)(SDDS_target->data[target_index]) + (j + SDDS_target->n_rows) * SDDS_type_size[SDDS_target->layout.column_definition[target_index].type - 1])) {
            sprintf(buffer, "Problem with cast for column %s (SDDS_CopyAdditionalRows)", SDDS_source->layout.column_definition[i].name);
            SDDS_SetError(buffer);
            return 0;
          }
        }
      }
    } else {
      if (SDDS_source->layout.column_definition[i].type != SDDS_target->layout.column_definition[target_index].type) {
        sprintf(buffer, "Unable to copy columns---inconsistent data types for %s (SDDS_CopyAdditionalRows)", SDDS_source->layout.column_definition[i].name);
        SDDS_SetError(buffer);
        return (0);
      }
      if (!SDDS_CopyStringArray((char **)((char *)SDDS_target->data[target_index] + size * SDDS_target->n_rows), SDDS_source->data[i], SDDS_source->n_rows)) {
        SDDS_SetError("Unable to copy columns (SDDS_CopyAdditionalRows)");
        return (0);
      }
    }
    SDDS_target->column_flag[target_index] = 1;
    SDDS_target->column_order[target_index] = target_index;
  }
  SDDS_target->n_rows += SDDS_source->n_rows;
  if (SDDS_target->row_flag)
    for (i = 0; i < SDDS_target->n_rows; i++)
      SDDS_target->row_flag[i] = 1;

  return (1);
}

/**
 * Copies the data from one SDDS_DATASET structure to another.
 * This includes parameters, arrays, and columns.
 *
 * @param SDDS_target Pointer to the SDDS_DATASET structure where data will be copied to.
 * @param SDDS_source Pointer to the SDDS_DATASET structure from which data will be copied.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyPage(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source) {
  if (!SDDS_CheckDataset(SDDS_target, "SDDS_CopyPage"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_CopyPage"))
    return (0);

  if (!SDDS_StartPage(SDDS_target, SDDS_target->layout.n_columns ? SDDS_source->n_rows : 0)) {
    SDDS_SetError("Unable to copy page (SDDS_CopyPage)");
    return (0);
  }
  if (!SDDS_CopyParameters(SDDS_target, SDDS_source))
    return (0);
  if (!SDDS_CopyArrays(SDDS_target, SDDS_source))
    return (0);
  if (!SDDS_CopyColumns(SDDS_target, SDDS_source))
    return (0);
  return (1);
}

/**
 * Sets the flag to defer or resume saving the layout of an SDDS_DATASET.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param mode Non-zero value to defer saving the layout; zero to resume saving.
 */
void SDDS_DeferSavingLayout(SDDS_DATASET *SDDS_dataset, int32_t mode) {
  SDDS_dataset->deferSavingLayout = mode;
}

/**
 * Saves the current layout of the SDDS_DATASET.
 * The layout is stored internally for future restoration if needed.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure whose layout is to be saved.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_SaveLayout(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *source, *target;

  if (SDDS_dataset->deferSavingLayout)
    return 1;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SaveLayout"))
    return (0);

  if ((source = &SDDS_dataset->layout) == (target = &SDDS_dataset->original_layout)) {
    SDDS_SetError("\"original\" and working page layouts share memory!");
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  /* copy pointer elements of structure into new memory */
  if (source->n_columns) {
    if (!(target->column_definition = (COLUMN_DEFINITION *)SDDS_Realloc((void *)target->column_definition, sizeof(COLUMN_DEFINITION) * source->n_columns)) || !(target->column_index = (SORTED_INDEX **)SDDS_Realloc((void *)target->column_index, sizeof(SORTED_INDEX *) * source->n_columns))) {
      SDDS_SetError("Unable to save layout--allocation failure (SDDS_SaveLayout)");
      return (0);
    }
    memcpy((char *)target->column_definition, (char *)source->column_definition, sizeof(COLUMN_DEFINITION) * source->n_columns);
    memcpy((char *)target->column_index, (char *)source->column_index, sizeof(SORTED_INDEX *) * source->n_columns);
  }
  if (source->n_parameters) {
    if (!(target->parameter_definition =
            (PARAMETER_DEFINITION *)SDDS_Realloc((void *)target->parameter_definition, sizeof(PARAMETER_DEFINITION) * source->n_parameters)) ||
        !(target->parameter_index = (SORTED_INDEX **)SDDS_Realloc((void *)target->parameter_index, sizeof(SORTED_INDEX *) * source->n_parameters))) {
      SDDS_SetError("Unable to save layout--allocation failure (SDDS_SaveLayout)");
      return (0);
    }
    memcpy((char *)target->parameter_definition, (char *)source->parameter_definition, sizeof(PARAMETER_DEFINITION) * source->n_parameters);
    memcpy((char *)target->parameter_index, (char *)source->parameter_index, sizeof(SORTED_INDEX *) * source->n_parameters);
  }
  if (source->n_arrays) {
    if (!(target->array_definition = (ARRAY_DEFINITION *)SDDS_Realloc((void *)target->array_definition, sizeof(ARRAY_DEFINITION) * source->n_arrays)) || !(target->array_index = (SORTED_INDEX **)SDDS_Realloc((void *)target->array_index, sizeof(SORTED_INDEX *) * source->n_arrays))) {
      SDDS_SetError("Unable to save layout--allocation failure (SDDS_SaveLayout)");
      return (0);
    }
    memcpy((char *)target->array_definition, (char *)source->array_definition, sizeof(ARRAY_DEFINITION) * source->n_arrays);
    memcpy((char *)target->array_index, (char *)source->array_index, sizeof(SORTED_INDEX *) * source->n_arrays);
  }
  if (source->n_associates) {
    if (!(target->associate_definition = (ASSOCIATE_DEFINITION *)SDDS_Realloc((void *)target->associate_definition, sizeof(ASSOCIATE_DEFINITION) * source->n_associates))) {
      SDDS_SetError("Unable to save layout--allocation failure (SDDS_SaveLayout)");
      return (0);
    }
    memcpy((char *)target->associate_definition, (char *)source->associate_definition, sizeof(ASSOCIATE_DEFINITION) * source->n_associates);
  }

  target->n_columns = source->n_columns;
  target->n_parameters = source->n_parameters;
  target->n_associates = source->n_associates;
  target->n_arrays = source->n_arrays;
  target->description = source->description;
  target->contents = source->contents;
  target->version = source->version;
  target->data_mode = source->data_mode;
  target->filename = source->filename;
  target->fp = source->fp;
  target->popenUsed = source->popenUsed;

  if (SDDS_dataset->layout.n_columns) {
    if (!(SDDS_dataset->column_track_memory = (short *)SDDS_Realloc(SDDS_dataset->column_track_memory, sizeof(short) * SDDS_dataset->layout.n_columns))) {
      SDDS_SetError("memory allocation failure (SDDS_SaveLayout)");
      return(0);
    }
    if (!SDDS_SetMemory(SDDS_dataset->column_track_memory, SDDS_dataset->layout.n_columns, SDDS_SHORT, (short)1, (short)0)) {
      SDDS_SetError("Unable to initialize memory (SDDS_SaveLayout)");
      return (0);
    }
  }

  return (1);
}

/**
 * Restores a previously saved layout of the SDDS_DATASET.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure whose layout is to be restored.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_RestoreLayout(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *source, *target;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_RestoreLayout"))
    return (0);

  source = &SDDS_dataset->original_layout;
  target = &SDDS_dataset->layout;

  /* copy pointer elements of structure into new memory */
  if (source->n_columns) {
    if (target->column_definition == source->column_definition) {
      SDDS_SetError("Unable to restore layout--column definition pointers are the same  (SDDS_RestoreLayout)");
      return (0);
    }
    if (!(target->column_definition = (COLUMN_DEFINITION *)SDDS_Realloc((void *)target->column_definition, sizeof(COLUMN_DEFINITION) * source->n_columns))) {
      SDDS_SetError("Unable to restore layout--allocation failure (SDDS_RestoreLayout)");
      return (0);
    }
    memcpy((char *)target->column_definition, (char *)source->column_definition, sizeof(COLUMN_DEFINITION) * source->n_columns);
  }
  if (source->n_parameters) {
    if (target->parameter_definition == source->parameter_definition) {
      SDDS_SetError("Unable to restore layout--parameter definition pointers are the same  (SDDS_RestoreLayout)");
      return (0);
    }
    if (!(target->parameter_definition = (PARAMETER_DEFINITION *)SDDS_Realloc((void *)target->parameter_definition, sizeof(PARAMETER_DEFINITION) * source->n_parameters))) {
      SDDS_SetError("Unable to restore layout--allocation failure (SDDS_RestoreLayout)");
      return (0);
    }
    memcpy((char *)target->parameter_definition, (char *)source->parameter_definition, sizeof(PARAMETER_DEFINITION) * source->n_parameters);
  }
  if (source->n_arrays) {
    if (target->array_definition == source->array_definition) {
      SDDS_SetError("Unable to restore layout--array definition pointers are the same  (SDDS_RestoreLayout)");
      return (0);
    }
    if (!(target->array_definition = (ARRAY_DEFINITION *)SDDS_Realloc((void *)target->array_definition, sizeof(ARRAY_DEFINITION) * source->n_arrays))) {
      SDDS_SetError("Unable to restore layout--allocation failure (SDDS_RestoreLayout)");
      return (0);
    }
    memcpy((char *)target->array_definition, (char *)source->array_definition, sizeof(ARRAY_DEFINITION) * source->n_arrays);
  }
  if (source->n_associates) {
    if (target->associate_definition == source->associate_definition) {
      SDDS_SetError("Unable to restore layout--associate definition pointers are the same  (SDDS_RestoreLayout)");
      return (0);
    }
    if (!(target->associate_definition = (ASSOCIATE_DEFINITION *)SDDS_Realloc((void *)target->associate_definition, sizeof(ASSOCIATE_DEFINITION) * source->n_associates))) {
      SDDS_SetError("Unable to restore layout--allocation failure (SDDS_RestoreLayout)");
      return (0);
    }
    memcpy((char *)target->associate_definition, (char *)source->associate_definition, sizeof(ASSOCIATE_DEFINITION) * source->n_associates);
  }

  target->n_columns = source->n_columns;
  target->n_parameters = source->n_parameters;
  target->n_associates = source->n_associates;
  target->n_arrays = source->n_arrays;
  target->description = source->description;
  target->contents = source->contents;
  target->version = source->version;
  target->data_mode = source->data_mode;
  target->filename = source->filename;
  target->fp = source->fp;

  return (1);
}

/**
 * Copies a row from the source SDDS_DATASET to the target SDDS_DATASET.
 * Only columns that exist in both datasets are copied.
 * The source row is determined by its position among the selected rows.
 *
 * @param SDDS_target Pointer to the SDDS_DATASET structure where the row will be copied to.
 * @param target_row Index of the row in the target dataset where data will be placed.
 * @param SDDS_source Pointer to the SDDS_DATASET structure from which the row will be copied.
 * @param source_srow Index of the selected row (among rows of interest) in the source dataset.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyRow(SDDS_DATASET *SDDS_target, int64_t target_row, SDDS_DATASET *SDDS_source, int64_t source_srow) {
  int64_t i, j, source_row;
  int32_t size, type;

  if (!SDDS_CheckDataset(SDDS_target, "SDDS_CopyRow"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_CopyRow"))
    return (0);

  if (target_row >= SDDS_target->n_rows_allocated) {
    SDDS_SetError("Unable to copy row--target page not large enough");
    return (0);
  }
  if (SDDS_target->n_rows <= target_row)
    SDDS_target->n_rows = target_row + 1;

  source_row = -1;
  for (i = j = 0; i < SDDS_source->n_rows; i++)
    if (SDDS_source->row_flag[i] && j++ == source_srow) {
      source_row = i;
      break;
    }

  if (source_row == -1) {
    SDDS_SetError("Unable to copy row--source selected-row does not exist");
    return (0);
  }

  for (i = 0; i < SDDS_target->layout.n_columns; i++) {
    if ((j = SDDS_GetColumnIndex(SDDS_source, SDDS_target->layout.column_definition[i].name)) < 0 || !SDDS_source->column_flag[j])
      continue;
    if ((type = SDDS_GetColumnType(SDDS_target, i)) == SDDS_STRING) {
      if (!SDDS_CopyString(((char ***)SDDS_target->data)[i] + target_row, ((char ***)SDDS_source->data)[j][source_row])) {
        SDDS_SetError("Unable to copy row--string copy failed (SDDS_CopyRow)");
        return (0);
      }
    } else {
      size = SDDS_type_size[type - 1];
      memcpy((char *)SDDS_target->data[i] + size * target_row, (char *)SDDS_source->data[j] + size * source_row, size);
    }
    SDDS_target->row_flag[target_row] = 1;
  }
  return (1);
}

/**
 * Copies a specific row from the source SDDS_DATASET to the target SDDS_DATASET.
 * Only columns that exist in both datasets are copied.
 *
 * @param SDDS_target Pointer to the SDDS_DATASET structure where the row will be copied to.
 * @param target_row Index of the row in the target dataset where data will be placed.
 * @param SDDS_source Pointer to the SDDS_DATASET structure from which the row will be copied.
 * @param source_row Index of the row in the source dataset to be copied.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyRowDirect(SDDS_DATASET *SDDS_target, int64_t target_row, SDDS_DATASET *SDDS_source, int64_t source_row) {
  int64_t i, j;
  int32_t size, type;

  if (!SDDS_CheckDataset(SDDS_target, "SDDS_CopyRow"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_CopyRow"))
    return (0);

  if (target_row >= SDDS_target->n_rows_allocated) {
    SDDS_SetError("Unable to copy row--target page not large enough");
    return (0);
  }
  if (SDDS_target->n_rows <= target_row)
    SDDS_target->n_rows = target_row + 1;
  if (source_row >= SDDS_source->n_rows_allocated) {
    SDDS_SetError("Unable to copy row--source row non-existent");
    return (0);
  }

  for (i = 0; i < SDDS_target->layout.n_columns; i++) {
    if ((j = SDDS_GetColumnIndex(SDDS_source, SDDS_target->layout.column_definition[i].name)) < 0 || !SDDS_source->column_flag[j])
      continue;
    if ((type = SDDS_GetColumnType(SDDS_target, i)) == SDDS_STRING) {
      if (!SDDS_CopyString(((char ***)SDDS_target->data)[i] + target_row, ((char ***)SDDS_source->data)[j][source_row])) {
        SDDS_SetError("Unable to copy row--string copy failed (SDDS_CopyRow)");
        return (0);
      }
    } else {
      size = SDDS_type_size[type - 1];
      memcpy((char *)SDDS_target->data[i] + size * target_row, (char *)SDDS_source->data[j] + size * source_row, size);
    }
    SDDS_target->row_flag[target_row] = 1;
  }
  return (1);
}

/**
 * Copies a range of rows from the source SDDS_DATASET to the target SDDS_DATASET.
 * Only columns that exist in both datasets are copied.
 *
 * @param SDDS_target Pointer to the SDDS_DATASET structure where rows will be copied to.
 * @param SDDS_source Pointer to the SDDS_DATASET structure from which rows will be copied.
 * @param firstRow Index of the first row to copy from the source dataset.
 * @param lastRow Index of the last row to copy from the source dataset.
 *
 * @return Returns 1 on success; 0 on failure, with an error message recorded.
 */
int32_t SDDS_CopyRows(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, int64_t firstRow, int64_t lastRow) {
  int64_t i, j, k;
  int32_t size, target_index;
  char buffer[1024];
  int64_t *rowList, roi;
  k = 0;

  if (!SDDS_target->layout.n_columns)
    return 1;
  roi = lastRow - firstRow + 1;
  if (roi > SDDS_target->n_rows_allocated) {
    SDDS_SetError("Unable to copy rows of interest--insufficient memory allocated to target page (SDDS_CopyRows)");
    return 0;
  }
  rowList = malloc(sizeof(*rowList) * roi);
  k = 0;

  for (j = firstRow; j <= lastRow; j++) {
    rowList[k] = j;
    k++;
  }

  for (i = 0; i < SDDS_source->layout.n_columns; i++) {
    if ((target_index = SDDS_GetColumnIndex(SDDS_target, SDDS_source->layout.column_definition[i].name)) < 0)
      continue;
    if (SDDS_source->layout.column_definition[i].type != SDDS_STRING) {
      if (SDDS_source->layout.column_definition[i].type == SDDS_target->layout.column_definition[target_index].type) {
        size = SDDS_type_size[SDDS_source->layout.column_definition[i].type - 1];
        for (k = 0; k < roi; k++) {

          memcpy((char *)SDDS_target->data[target_index] + k * size, (char *)SDDS_source->data[i] + rowList[k] * size, SDDS_type_size[SDDS_source->layout.column_definition[i].type - 1]);
        }
      } else {
        for (k = 0; k < roi; k++) {
          if (!SDDS_CastValue(SDDS_source->data[i], rowList[k],
                              SDDS_source->layout.column_definition[i].type, SDDS_target->layout.column_definition[target_index].type, (char *)(SDDS_target->data[target_index]) + k * SDDS_type_size[SDDS_target->layout.column_definition[target_index].type - 1])) {
            sprintf(buffer, "Problem with cast for column %s (SDDS_CopyRows)", SDDS_source->layout.column_definition[i].name);
            SDDS_SetError(buffer);
            return 0;
          }
        }
      }
    } else {
      if (SDDS_source->layout.column_definition[i].type != SDDS_target->layout.column_definition[target_index].type) {
        sprintf(buffer, "Unable to copy columns---inconsistent data types for %s (SDDS_CopyRows)", SDDS_source->layout.column_definition[i].name);
        SDDS_SetError(buffer);
        return (0);
      }
      for (k = 0; k < roi; k++) {
        if (((char **)SDDS_target->data[target_index])[k])
          free(((char **)SDDS_target->data[target_index])[k]);
        if (!SDDS_CopyString(&((char **)SDDS_target->data[target_index])[k], ((char **)SDDS_source->data[i])[rowList[k]])) {
          SDDS_SetError("Unable to copy rows (SDDS_CopyRows)");
          return (0);
        }
      }
    }
    SDDS_target->column_flag[target_index] = 1;
    SDDS_target->column_order[target_index] = target_index;
  }

  free(rowList);

  SDDS_target->n_rows = roi;
  if (SDDS_target->row_flag) {
    for (i = 0; i < roi; i++) {
      SDDS_target->row_flag[i] = 1;
    }
  }

  return (1);
}
