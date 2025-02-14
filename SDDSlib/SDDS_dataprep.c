/**
 * @file SDDS_dataprep.c
 * @brief This file provides functions for SDDS dataset preparations.
 *
 * This file provides functions for SDDS dataset preparations.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday. H. Shang
 */

#include "SDDS.h"
#include "SDDS_internal.h"
#include "mdb.h"

#undef DEBUG

/**
 * Allocates memory for column flags and column order arrays in the specified SDDS dataset.
 *
 * This function allocates memory for the `column_flag` and `column_order` arrays based on the number of columns
 * defined in the dataset's layout. It initializes `column_flag` to 1 and `column_order` to 0 and 1 respectively.
 *
 * @param SDDS_target Pointer to the SDDS_DATASET structure where column flags will be allocated.
 *
 * @return Returns 1 on successful allocation and initialization. On failure, returns 0 and records an error message.
 *
 * @sa SDDS_Malloc, SDDS_SetMemory, SDDS_SetError
 */
int32_t SDDS_AllocateColumnFlags(SDDS_DATASET *SDDS_target) {
  if (SDDS_target->layout.n_columns &&
      ((!(SDDS_target->column_flag = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_target->layout.n_columns)) ||
        !(SDDS_target->column_order = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_target->layout.n_columns))) ||
       (!SDDS_SetMemory(SDDS_target->column_flag, SDDS_target->layout.n_columns, SDDS_LONG, (int32_t)1, (int32_t)0) || 
        !SDDS_SetMemory(SDDS_target->column_order, SDDS_target->layout.n_columns, SDDS_LONG, (int32_t)0, (int32_t)1)))) {
    SDDS_SetError("Unable to allocate column flags--memory allocation failure (SDDS_AllocateColumnFlags)");
    return 0;
  }
  return 1;
}

/**
 * Initializes an SDDS_DATASET structure in preparation for inserting data into a new table.
 *
 * This function prepares the specified SDDS dataset for data insertion by initializing necessary data structures
 * and allocating memory based on the expected number of rows. It must be preceded by a call to `SDDS_InitializeOutput`.
 * `SDDS_StartPage` can be called multiple times to begin writing additional tables within the dataset. After initializing
 * a page, `SDDS_WriteTable` should be used to write the table to disk.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure representing the data set.
 * @param expected_n_rows The expected number of rows in the data table. This value is used to preallocate memory for storing data values.
 *                        If `expected_n_rows` is less than or equal to zero, it defaults to 1.
 *
 * @return Returns 1 on successful initialization. On failure, returns 0 and records an error message.
 *
 * @sa SDDS_InitializeOutput, SDDS_WriteTable, SDDS_SetError
 */
int32_t SDDS_StartPage(SDDS_DATASET *SDDS_dataset, int64_t expected_n_rows) {
  SDDS_LAYOUT *layout;
  int64_t i;
  int32_t size;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_StartPage"))
    return (0);
  if ((SDDS_dataset->writing_page) && (SDDS_dataset->layout.data_mode.fixed_row_count)) {
    if (!SDDS_UpdateRowCount(SDDS_dataset))
      return (0);
  }
  if (!SDDS_RestoreLayout(SDDS_dataset)) {
    SDDS_SetError("Unable to start page--couldn't restore layout (SDDS_StartPage)");
    return (0);
  }
  if (expected_n_rows <= 0)
    expected_n_rows = 1;
  SDDS_dataset->n_rows_written = 0;
  SDDS_dataset->last_row_written = -1;
  SDDS_dataset->writing_page = 0;
  SDDS_dataset->first_row_in_mem = 0;
  layout = &SDDS_dataset->layout;
  if (SDDS_dataset->page_started == 0) {
    if (layout->n_parameters) {
      if (!(SDDS_dataset->parameter = (void **)calloc(sizeof(*SDDS_dataset->parameter), layout->n_parameters))) {
        SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
        return (0);
      }
      for (i = 0; i < layout->n_parameters; i++) {
        if (!(SDDS_dataset->parameter[i] = (void *)calloc(SDDS_type_size[layout->parameter_definition[i].type - 1], 1))) {
          SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
          return (0);
        }
      }
    }
    if (layout->n_arrays) {
      if (!(SDDS_dataset->array = (SDDS_ARRAY *)calloc(sizeof(*SDDS_dataset->array), layout->n_arrays))) {
        SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
        return (0);
      }
    }
    if (layout->n_columns) {
      if (!(SDDS_dataset->data = (void **)calloc(sizeof(*SDDS_dataset->data), layout->n_columns))) {
        SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
        return (0);
      }
      SDDS_dataset->row_flag = NULL;
      if (expected_n_rows) {
        if (!(SDDS_dataset->row_flag = (int32_t *)SDDS_Malloc(sizeof(int32_t) * expected_n_rows))) {
          SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
          return (0);
        }
        for (i = 0; i < layout->n_columns; i++) {
          if (!(SDDS_dataset->data[i] = (void *)calloc(expected_n_rows, SDDS_type_size[layout->column_definition[i].type - 1]))) {
            SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
            return (0);
          }
        }
      }
      SDDS_dataset->n_rows_allocated = expected_n_rows;
      if (!(SDDS_dataset->column_flag = (int32_t *)SDDS_Realloc(SDDS_dataset->column_flag, sizeof(int32_t) * layout->n_columns)) || 
          !(SDDS_dataset->column_order = (int32_t *)SDDS_Realloc(SDDS_dataset->column_order, sizeof(int32_t) * layout->n_columns))) {
        SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
        return (0);
      }
    }
  } else if (SDDS_dataset->n_rows_allocated >= expected_n_rows && layout->n_columns) {
    for (i = 0; i < layout->n_columns; i++) {
      if (SDDS_dataset->data[i] && layout->column_definition[i].type == SDDS_STRING)
        SDDS_FreeStringArray(SDDS_dataset->data[i], SDDS_dataset->n_rows_allocated);
    }
  } else if (SDDS_dataset->n_rows_allocated < expected_n_rows && layout->n_columns) {
    if (!SDDS_dataset->data) {
      if (!(SDDS_dataset->column_flag = (int32_t *)SDDS_Realloc(SDDS_dataset->column_flag, sizeof(int32_t) * layout->n_columns)) ||
          !(SDDS_dataset->column_order = (int32_t *)SDDS_Realloc(SDDS_dataset->column_order, sizeof(int32_t) * layout->n_columns)) || 
          !(SDDS_dataset->data = (void **)calloc(layout->n_columns, sizeof(*SDDS_dataset->data)))) {
        SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
        return (0);
      }
    }
    for (i = 0; i < layout->n_columns; i++) {
      size = SDDS_type_size[layout->column_definition[i].type - 1];
      if (SDDS_dataset->data[i] && layout->column_definition[i].type == SDDS_STRING)
        SDDS_FreeStringArray(SDDS_dataset->data[i], SDDS_dataset->n_rows_allocated);
      if (!(SDDS_dataset->data[i] = (void *)SDDS_Realloc(SDDS_dataset->data[i], expected_n_rows * size))) {
        SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
        return (0);
      }
      SDDS_ZeroMemory((char *)SDDS_dataset->data[i] + size * SDDS_dataset->n_rows_allocated, size * (expected_n_rows - SDDS_dataset->n_rows_allocated));
    }
    if (!(SDDS_dataset->row_flag = (int32_t *)SDDS_Realloc(SDDS_dataset->row_flag, sizeof(int32_t) * expected_n_rows))) {
      SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_StartPage)");
      return (0);
    }
    SDDS_dataset->n_rows_allocated = expected_n_rows;
  }
  if (SDDS_dataset->n_rows_allocated && layout->n_columns && !SDDS_SetMemory(SDDS_dataset->row_flag, SDDS_dataset->n_rows_allocated, SDDS_LONG, (int32_t)1, (int32_t)0)) {
    SDDS_SetError("Unable to start page--memory initialization failure (SDDS_StartPage)");
    return (0);
  }
  if (layout->n_columns && (!SDDS_SetMemory(SDDS_dataset->column_flag, layout->n_columns, SDDS_LONG, (int32_t)1, (int32_t)0) || 
                            !SDDS_SetMemory(SDDS_dataset->column_order, layout->n_columns, SDDS_LONG, (int32_t)0, (int32_t)1))) {
    SDDS_SetError("Unable to start page--memory initialization failure (SDDS_StartPage)");
    return (0);
  }
  SDDS_dataset->n_of_interest = layout->n_columns;
  SDDS_dataset->page_number++;
  SDDS_dataset->page_started = 1;
  SDDS_dataset->n_rows = 0;
  return (1);
}

/**
 * Clears the current page in the SDDS dataset, resetting all data and flags.
 *
 * This function resets the current data page in the specified SDDS dataset by reinitializing column flags and order.
 * It frees any allocated string data and zeros out the data arrays, parameters, and arrays in the dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure whose current page will be cleared.
 *
 * @return Returns 1 on successful clearing of the page. On failure, returns 0 and records an error message.
 *
 * @sa SDDS_SetMemory, SDDS_FreeStringData, SDDS_ZeroMemory
 */
int32_t SDDS_ClearPage(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *layout;
  int64_t i;
  int32_t size;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ClearPage"))
    return 0;
  layout = &SDDS_dataset->layout;

  if (layout->n_columns && ((SDDS_dataset->column_flag && !SDDS_SetMemory(SDDS_dataset->column_flag, layout->n_columns, SDDS_LONG, (int32_t)1, (int32_t)0)) || 
                            ((SDDS_dataset->column_order && !SDDS_SetMemory(SDDS_dataset->column_order, layout->n_columns, SDDS_LONG, (int32_t)0, (int32_t)1))))) {
    SDDS_SetError("Unable to start page--memory initialization failure (SDDS_ClearPage)");
    return 0;
  }
  SDDS_FreeStringData(SDDS_dataset);
  if (SDDS_dataset->data) {
    for (i = 0; i < layout->n_columns; i++) {
      size = SDDS_type_size[layout->column_definition[i].type - 1];
      if (SDDS_dataset->data[i])
        SDDS_ZeroMemory(SDDS_dataset->data[i], size * SDDS_dataset->n_rows_allocated);
    }
  }
  if (SDDS_dataset->parameter) {
    for (i = 0; i < layout->n_parameters; i++) {
      size = SDDS_type_size[layout->parameter_definition[i].type - 1];
      SDDS_ZeroMemory(SDDS_dataset->parameter[i], size);
    }
  }
  for (i = 0; i < layout->n_arrays; i++) {
    size = SDDS_type_size[layout->array_definition[i].type - 1];
    if (SDDS_dataset->array && SDDS_dataset->array[i].data && SDDS_dataset->array[i].elements)
      SDDS_ZeroMemory(SDDS_dataset->array[i].data, size * SDDS_dataset->array[i].elements);
  }
  return 1;
}

/**
 * Shortens the data table in the SDDS dataset to a specified number of rows.
 *
 * This function reduces the number of allocated rows in the specified SDDS dataset to the given `rows` count.
 * It reallocates memory for each column's data array and the row flags, freeing existing data as necessary.
 * All data is reset, and the number of rows is set to zero.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure whose table will be shortened.
 * @param rows The new number of rows to allocate in the table. If `rows` is less than or equal to zero, it defaults to 1.
 *
 * @return Returns 1 on successful reallocation and initialization. On failure, returns 0 and records an error message.
 *
 * @sa SDDS_Realloc, SDDS_SetMemory, SDDS_Free, SDDS_SetError
 */
int32_t SDDS_ShortenTable(SDDS_DATASET *SDDS_dataset, int64_t rows) {
  SDDS_LAYOUT *layout;
  int64_t i, size;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ShortenTable"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!SDDS_dataset->data && !(SDDS_dataset->data = (void **)calloc(layout->n_columns, sizeof(*SDDS_dataset->data)))) {
    SDDS_SetError("Unable to start  page--memory allocation failure (SDDS_ShortenTable)");
    return (0);
  }
  if (rows <= 0)
    rows = 1;
  for (i = 0; i < layout->n_columns; i++) {
    size = SDDS_type_size[layout->column_definition[i].type - 1];
    if (SDDS_dataset->data[i])
      free(SDDS_dataset->data[i]);
    if (!(SDDS_dataset->data[i] = (void *)calloc(rows, size))) {
      SDDS_SetError("Unable to shorten page--memory allocation failure (SDDS_ShortenTable)");
      return (0);
    }
  }
  if (SDDS_dataset->row_flag)
    free(SDDS_dataset->row_flag);
  if (!(SDDS_dataset->row_flag = (int32_t *)malloc(rows * sizeof(int32_t)))) {
    SDDS_SetError("Unable to shorten page--memory allocation failure (SDDS_ShortenTable)");
    return (0);
  }
  SDDS_dataset->n_rows_allocated = rows;
  /* Shorten table is not exactly true. It is really deleting all the rows and then allocating new space.
     if (SDDS_dataset->n_rows > rows) {
     SDDS_dataset->n_rows = rows;
     }
  */
  SDDS_dataset->n_rows = 0;

  if (!SDDS_SetMemory(SDDS_dataset->row_flag, SDDS_dataset->n_rows_allocated, SDDS_LONG, (int32_t)1, (int32_t)0) ||
      !SDDS_SetMemory(SDDS_dataset->column_flag, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)1, (int32_t)0) || 
      !SDDS_SetMemory(SDDS_dataset->column_order, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)0, (int32_t)1)) {
    SDDS_SetError("Unable to shorten page--memory initialization failure (SDDS_ShortenTable)");
    return (0);
  }
  return (1);
}

/**
 * Increases the number of allocated rows in the SDDS dataset's data table.
 *
 * This function extends the allocated memory for the data table in the specified SDDS dataset by adding
 * the specified number of additional rows. It reallocates memory for each column's data array and the row flags,
 * initializing the newly allocated memory to zero.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure whose table will be lengthened.
 * @param n_additional_rows The number of additional rows to allocate. If `n_additional_rows` is less than zero, it is treated as zero.
 *
 * @return Returns 1 on successful reallocation and initialization. On failure, returns 0 and records an error message.
 *
 * @sa SDDS_Realloc, SDDS_SetMemory, SDDS_ZeroMemory, SDDS_SetError
 */
int32_t SDDS_LengthenTable(SDDS_DATASET *SDDS_dataset, int64_t n_additional_rows) {
  SDDS_LAYOUT *layout;
  int64_t i, size;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_LengthenTable"))
    return (0);
  layout = &SDDS_dataset->layout;
#if defined(DEBUG)
  fprintf(stderr, "table size being increased from %" PRId64 " to %" PRId64 " rows\n", SDDS_dataset->n_rows_allocated, SDDS_dataset->n_rows_allocated + n_additional_rows);
#endif
  if (!SDDS_dataset->data && !(SDDS_dataset->data = (void **)calloc(layout->n_columns, sizeof(*SDDS_dataset->data)))) {
    SDDS_SetError("Unable to start page--memory allocation failure1 (SDDS_LengthenTable)");
    return (0);
  }
  if (n_additional_rows < 0)
    n_additional_rows = 0;
  for (i = 0; i < layout->n_columns; i++) {
    size = SDDS_type_size[layout->column_definition[i].type - 1];
    if (!(SDDS_dataset->data[i] = (void *)SDDS_Realloc(SDDS_dataset->data[i], (SDDS_dataset->n_rows_allocated + n_additional_rows) * size))) {
      SDDS_SetError("Unable to lengthen page--memory allocation failure2 (SDDS_LengthenTable)");
      return (0);
    }
    SDDS_ZeroMemory((char *)SDDS_dataset->data[i] + size * SDDS_dataset->n_rows_allocated, size * n_additional_rows);
  }
  if (!(SDDS_dataset->row_flag = (int32_t *)SDDS_Realloc(SDDS_dataset->row_flag, (SDDS_dataset->n_rows_allocated + n_additional_rows) * sizeof(int32_t)))) {
    SDDS_SetError("Unable to lengthen page--memory allocation failure3 (SDDS_LengthenTable)");
    return (0);
  }
  SDDS_dataset->n_rows_allocated += n_additional_rows;

  if (!SDDS_SetMemory(SDDS_dataset->row_flag, SDDS_dataset->n_rows_allocated, SDDS_LONG, (int32_t)1, (int32_t)0) ||
      !SDDS_SetMemory(SDDS_dataset->column_flag, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)1, (int32_t)0) || 
      !SDDS_SetMemory(SDDS_dataset->column_order, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)0, (int32_t)1)) {
    SDDS_SetError("Unable to lengthen page--memory initialization failure4 (SDDS_LengthenTable)");
    return (0);
  }
  return (1);
}

/**
 * Sets the values of one or more parameters for the current data table in an SDDS dataset.
 *
 * This function assigns values to parameters in the current data table of the specified SDDS dataset. It must be preceded by a call
 * to `SDDS_StartPage` to initialize the table. The function can be called multiple times to set parameters for different tables,
 * but `SDDS_WriteTable` should be used to write each table to disk.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure representing the data set.
 * @param mode A bitwise combination of the following constants:
 *             - `SDDS_SET_BY_INDEX`: Specify parameters by their index.
 *             - `SDDS_SET_BY_NAME`: Specify parameters by their name.
 *             - `SDDS_PASS_BY_VALUE`: Pass parameter values by value.
 *             - `SDDS_PASS_BY_REFERENCE`: Pass parameter values by reference.
 *
 *             Exactly one of `SDDS_SET_BY_INDEX` or `SDDS_SET_BY_NAME` must be set to indicate how parameters are identified.
 *             Additionally, exactly one of `SDDS_PASS_BY_VALUE` or `SDDS_PASS_BY_REFERENCE` must be set to indicate how parameter
 *             values are provided.
 *
 *             The syntax for the four possible mode combinations is as follows:
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetParameters(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t index1, value1, int32_t index2, value2, ..., -1)`
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetParameters(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t index1, void *data1, int32_t index2, void *data2, ..., -1)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetParameters(SDDS_DATASET *SDDS_dataset, int32_t mode, char *name1, value1, char *name2, value2, ..., NULL)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetParameters(SDDS_DATASET *SDDS_dataset, int32_t mode, char *name1, void *data1, char *name2, void *data2, ..., NULL)`
 *
 *             **Note:** For parameters of type `SDDS_STRING`, passing by value means passing a `char *`, whereas passing by reference means passing a `char **`.
 *
 * @return Returns 1 on success. On failure, returns 0 and records an error message.
 *
 * @sa SDDS_StartPage, SDDS_WriteTable, SDDS_SetError, SDDS_GetParameterIndex, va_start, va_arg, va_end
 */
int32_t SDDS_SetParameters(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  va_list argptr;
  int32_t index, retval;
  SDDS_LAYOUT *layout;
  char *name;
  char s[SDDS_MAXLINE];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetParameters"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME) || !(mode & SDDS_PASS_BY_VALUE || mode & SDDS_PASS_BY_REFERENCE)) {
    SDDS_SetError("Unable to set parameter values--unknown mode (SDDS_SetParameters)");
    return (0);
  }

  va_start(argptr, mode);
  layout = &SDDS_dataset->layout;

  /* variable arguments are pairs of (index, value), where index is a int32_t integer */
  retval = -1;
  do {
    if (mode & SDDS_SET_BY_INDEX) {
      if ((index = va_arg(argptr, int32_t)) == -1) {
        retval = 1;
        break;
      }
      if (index < 0 || index >= layout->n_parameters) {
        SDDS_SetError("Unable to set parameter values--index out of range (SDDS_SetParameters)");
        retval = 0;
        break;
      }
    } else {
      if ((name = va_arg(argptr, char *)) == NULL) {
        retval = 1;
        break;
      }
      if ((index = SDDS_GetParameterIndex(SDDS_dataset, name)) < 0) {
        sprintf(s, "Unable to set parameter values--name %s not recognized (SDDS_SetParameters)", name);
        SDDS_SetError(s);
        retval = 0;
        break;
      }
    }
    switch (layout->parameter_definition[index].type) {
    case SDDS_SHORT:
      if (mode & SDDS_PASS_BY_VALUE)
        *((short *)SDDS_dataset->parameter[index]) = (short)va_arg(argptr, int);
      else
        *((short *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, short *));
      break;
    case SDDS_USHORT:
      if (mode & SDDS_PASS_BY_VALUE)
        *((unsigned short *)SDDS_dataset->parameter[index]) = (unsigned short)va_arg(argptr, unsigned int);
      else
        *((unsigned short *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, unsigned short *));
      break;
    case SDDS_LONG:
      if (mode & SDDS_PASS_BY_VALUE)
        *((int32_t *)SDDS_dataset->parameter[index]) = (int32_t)va_arg(argptr, int32_t);
      else
        *((int32_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, int32_t *));
      break;
    case SDDS_ULONG:
      if (mode & SDDS_PASS_BY_VALUE)
        *((uint32_t *)SDDS_dataset->parameter[index]) = (uint32_t)va_arg(argptr, uint32_t);
      else
        *((uint32_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, uint32_t *));
      break;
    case SDDS_LONG64:
      if (mode & SDDS_PASS_BY_VALUE)
        *((int64_t *)SDDS_dataset->parameter[index]) = (int64_t)va_arg(argptr, int64_t);
      else
        *((int64_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, int64_t *));
      break;
    case SDDS_ULONG64:
      if (mode & SDDS_PASS_BY_VALUE)
        *((uint64_t *)SDDS_dataset->parameter[index]) = (uint64_t)va_arg(argptr, uint64_t);
      else
        *((uint64_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, uint64_t *));
      break;
    case SDDS_FLOAT:
      if (mode & SDDS_PASS_BY_VALUE)
        *((float *)SDDS_dataset->parameter[index]) = (float)va_arg(argptr, double);
      else
        *((float *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, float *));
      break;
    case SDDS_DOUBLE:
      if (mode & SDDS_PASS_BY_VALUE)
        *((double *)SDDS_dataset->parameter[index]) = va_arg(argptr, double);
      else
        *((double *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
      break;
    case SDDS_LONGDOUBLE:
      if (mode & SDDS_PASS_BY_VALUE)
        *((long double *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
      else
        *((long double *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_STRING:
      if (*(char **)SDDS_dataset->parameter[index])
        free(*(char **)SDDS_dataset->parameter[index]);
      if (mode & SDDS_PASS_BY_VALUE) {
        if (!SDDS_CopyString((char **)SDDS_dataset->parameter[index], va_arg(argptr, char *))) {
          SDDS_SetError("Unable to set string parameter value--allocation failure (SDDS_SetParameters)");
          retval = 0;
        }
      } else {
        if (!SDDS_CopyString((char **)SDDS_dataset->parameter[index], *(va_arg(argptr, char **)))) {
          SDDS_SetError("Unable to set string parameter value--allocation failure (SDDS_SetParameters)");
          retval = 0;
        }
      }
      break;
    case SDDS_CHARACTER:
      if (mode & SDDS_PASS_BY_VALUE)
        *((char *)SDDS_dataset->parameter[index]) = (char)va_arg(argptr, int);
      else
        *((char *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, char *));
      break;
    default:
      SDDS_SetError("Unknown data type encountered (SDDS_SetParameters)");
      retval = 0;
    }
  } while (retval == -1);
  va_end(argptr);
  return (retval);
}

/**
 * Sets the value of a single parameter in the current data table of an SDDS dataset.
 *
 * This function assigns a value to a specified parameter in the current data table of the given SDDS dataset.
 * It must be preceded by a call to `SDDS_StartPage` to initialize the table. The parameter to be set can be
 * identified either by its index or by its name. The value can be passed either by value or by reference,
 * depending on the specified mode.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode A bitwise combination of the following constants:
 *             - `SDDS_SET_BY_INDEX`: Identify the parameter by its index.
 *             - `SDDS_SET_BY_NAME`: Identify the parameter by its name.
 *             - `SDDS_PASS_BY_VALUE`: Pass the parameter value by value.
 *             - `SDDS_PASS_BY_REFERENCE`: Pass the parameter value by reference.
 *
 *             **Mode Requirements:**
 *             - Exactly one of `SDDS_SET_BY_INDEX` or `SDDS_SET_BY_NAME` must be set.
 *             - Exactly one of `SDDS_PASS_BY_VALUE` or `SDDS_PASS_BY_REFERENCE` must be set.
 *
 *             **Syntax Based on Mode Combination:**
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetParameter(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t index, value)`
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetParameter(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t index, void *data)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetParameter(SDDS_DATASET *SDDS_dataset, int32_t mode, char *name, value)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetParameter(SDDS_DATASET *SDDS_dataset, int32_t mode, char *name, void *data)`
 *
 *             **Note:** For parameters of type `SDDS_STRING`, passing by value means passing a `char *`,
 *             whereas passing by reference means passing a `char **`.
 *
 * @return Returns `1` on successful assignment of the parameter value.
 *         On failure, returns `0` and records an appropriate error message.
 *
 * @sa SDDS_StartPage, SDDS_SetError, SDDS_GetParameterIndex, SDDS_CopyString
 */
int32_t SDDS_SetParameter(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  va_list argptr;
  int32_t index;
  SDDS_LAYOUT *layout;
  char *name;
  char s[SDDS_MAXLINE];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetParameters"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME) || !(mode & SDDS_PASS_BY_VALUE || mode & SDDS_PASS_BY_REFERENCE)) {
    SDDS_SetError("Unable to set parameter values--unknown mode (SDDS_SetParameters)");
    return (0);
  }

  va_start(argptr, mode);
  layout = &SDDS_dataset->layout;

  /* variable arguments are pairs of (index, value), where index is a int32_t integer */
  if (mode & SDDS_SET_BY_INDEX) {
    if ((index = va_arg(argptr, int32_t)) == -1) {
      SDDS_SetError("Unable to set parameter values--index is null (SDDS_SetParameter)");
      va_end(argptr);
      return (0);
    }
    if (index < 0 || index >= layout->n_parameters) {
      SDDS_SetError("Unable to set parameter values--index out of range (SDDS_SetParameter)");
      va_end(argptr);
      return (0);
    }
  } else {
    if ((name = va_arg(argptr, char *)) == NULL) {
      SDDS_SetError("Unable to set parameter values--name is null (SDDS_SetParameter)");
      va_end(argptr);
      return (0);
    }
    if ((index = SDDS_GetParameterIndex(SDDS_dataset, name)) < 0) {
      sprintf(s, "Unable to set parameter values--name %s not recognized (SDDS_SetParameter)", name);
      SDDS_SetError(s);
      va_end(argptr);
      return (0);
    }
  }
  switch (layout->parameter_definition[index].type) {
  case SDDS_SHORT:
    if (mode & SDDS_PASS_BY_VALUE)
      *((short *)SDDS_dataset->parameter[index]) = (short)va_arg(argptr, int);
    else
      *((short *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, short *));
    break;
  case SDDS_USHORT:
    if (mode & SDDS_PASS_BY_VALUE)
      *((unsigned short *)SDDS_dataset->parameter[index]) = (unsigned short)va_arg(argptr, unsigned int);
    else
      *((unsigned short *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, unsigned short *));
    break;
  case SDDS_LONG:
    if (mode & SDDS_PASS_BY_VALUE)
      *((int32_t *)SDDS_dataset->parameter[index]) = (int32_t)va_arg(argptr, int32_t);
    else
      *((int32_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, int32_t *));
    break;
  case SDDS_ULONG:
    if (mode & SDDS_PASS_BY_VALUE)
      *((uint32_t *)SDDS_dataset->parameter[index]) = (uint32_t)va_arg(argptr, uint32_t);
    else
      *((uint32_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, uint32_t *));
    break;
  case SDDS_LONG64:
    if (mode & SDDS_PASS_BY_VALUE)
      *((int64_t *)SDDS_dataset->parameter[index]) = (int64_t)va_arg(argptr, int64_t);
    else
      *((int64_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, int64_t *));
    break;
  case SDDS_ULONG64:
    if (mode & SDDS_PASS_BY_VALUE)
      *((uint64_t *)SDDS_dataset->parameter[index]) = (uint64_t)va_arg(argptr, uint64_t);
    else
      *((uint64_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, uint64_t *));
    break;
  case SDDS_FLOAT:
    if (mode & SDDS_PASS_BY_VALUE)
      *((float *)SDDS_dataset->parameter[index]) = (float)va_arg(argptr, double);
    else
      *((float *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, float *));
    break;
  case SDDS_DOUBLE:
    if (mode & SDDS_PASS_BY_VALUE)
      *((double *)SDDS_dataset->parameter[index]) = va_arg(argptr, double);
    else
      *((double *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
    break;
  case SDDS_LONGDOUBLE:
    if (mode & SDDS_PASS_BY_VALUE)
      *((long double *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
    else
      *((long double *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
    break;
  case SDDS_STRING:
    if (*(char **)SDDS_dataset->parameter[index])
      free(*(char **)SDDS_dataset->parameter[index]);
    if (mode & SDDS_PASS_BY_VALUE) {
      if (!SDDS_CopyString((char **)SDDS_dataset->parameter[index], va_arg(argptr, char *))) {
        SDDS_SetError("Unable to set string parameter value--allocation failure (SDDS_SetParameters)");
        va_end(argptr);
        return (0);
      }
    } else {
      if (!SDDS_CopyString((char **)SDDS_dataset->parameter[index], *(va_arg(argptr, char **)))) {
        SDDS_SetError("Unable to set string parameter value--allocation failure (SDDS_SetParameters)");
        va_end(argptr);
        return (0);
      }
    }
    break;
  case SDDS_CHARACTER:
    if (mode & SDDS_PASS_BY_VALUE)
      *((char *)SDDS_dataset->parameter[index]) = (char)va_arg(argptr, int);
    else
      *((char *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, char *));
    break;
  default:
    SDDS_SetError("Unknown data type encountered (SDDS_SetParameters)");
    va_end(argptr);
    return (0);
  }
  va_end(argptr);
  return (1);
}

/**
 * Sets the values of one or more parameters in the current data table of an SDDS dataset using double-precision floating-point numbers.
 *
 * This function assigns double-precision floating-point values to specified parameters in the current data table of the given SDDS dataset.
 * It must be preceded by a call to `SDDS_StartPage` to initialize the table. Parameters can be identified either by their index or by
 * their name. The values can be passed either by value or by reference, depending on the specified mode.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode A bitwise combination of the following constants:
 *             - `SDDS_SET_BY_INDEX`: Identify parameters by their indices.
 *             - `SDDS_SET_BY_NAME`: Identify parameters by their names.
 *             - `SDDS_PASS_BY_VALUE`: Pass parameter values by value.
 *             - `SDDS_PASS_BY_REFERENCE`: Pass parameter values by reference.
 *
 *             **Mode Requirements:**
 *             - Exactly one of `SDDS_SET_BY_INDEX` or `SDDS_SET_BY_NAME` must be set.
 *             - Exactly one of `SDDS_PASS_BY_VALUE` or `SDDS_PASS_BY_REFERENCE` must be set.
 *
 *             **Syntax Based on Mode Combination:**
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetParametersFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t index, double value)`
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetParametersFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t index, double *data)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetParametersFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, char *name, double value)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetParametersFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, char *name, double *data)`
 *
 *             **Note:** For parameters of type `SDDS_STRING`, setting values using this function is not supported and will result in an error.
 *
 * @return Returns `1` on successful assignment of all specified parameter values.
 *         On failure, returns `0` and records an appropriate error message.
 *
 * @sa SDDS_StartPage, SDDS_SetError, SDDS_GetParameterIndex
 */
int32_t SDDS_SetParametersFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  va_list argptr;
  int32_t index, retval;
  SDDS_LAYOUT *layout;
  char *name;
  char s[SDDS_MAXLINE];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetParametersFromDoubles"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME) || !(mode & SDDS_PASS_BY_VALUE || mode & SDDS_PASS_BY_REFERENCE)) {
    SDDS_SetError("Unable to set parameter values--unknown mode (SDDS_SetParametersFromDoubles)");
    return (0);
  }

  va_start(argptr, mode);
  layout = &SDDS_dataset->layout;

  /* variable arguments are pairs of (index, value), where index is a int32_t integer */
  retval = -1;
  do {
    if (mode & SDDS_SET_BY_INDEX) {
      if ((index = va_arg(argptr, int32_t)) == -1) {
        retval = 1;
        break;
      }
      if (index < 0 || index >= layout->n_parameters) {
        sprintf(s, "Unable to set parameter values--index %" PRId32 " out of range [%d, %" PRId32 "] (SDDS_SetParametersFromDoubles)", index, 0, layout->n_parameters);
        SDDS_SetError(s);
        retval = 0;
        break;
      }
    } else {
      if ((name = va_arg(argptr, char *)) == NULL) {
        retval = 1;
        break;
      }
      if ((index = SDDS_GetParameterIndex(SDDS_dataset, name)) < 0) {
        sprintf(s, "Unable to set parameter values--name %s not recognized (SDDS_SetParametersFromDoubles)", name);
        SDDS_SetError(s);
        retval = 0;
        break;
      }
    }
    switch (layout->parameter_definition[index].type) {
    case SDDS_SHORT:
      if (mode & SDDS_PASS_BY_VALUE)
        *((short *)SDDS_dataset->parameter[index]) = va_arg(argptr, double);
      else
        *((short *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
      break;
    case SDDS_USHORT:
      if (mode & SDDS_PASS_BY_VALUE)
        *((unsigned short *)SDDS_dataset->parameter[index]) = va_arg(argptr, double);
      else
        *((unsigned short *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
      break;
    case SDDS_LONG:
      if (mode & SDDS_PASS_BY_VALUE)
        *((int32_t *)SDDS_dataset->parameter[index]) = va_arg(argptr, double);
      else
        *((int32_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
      break;
    case SDDS_ULONG:
      if (mode & SDDS_PASS_BY_VALUE)
        *((uint32_t *)SDDS_dataset->parameter[index]) = va_arg(argptr, double);
      else
        *((uint32_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
      break;
    case SDDS_LONG64:
      if (mode & SDDS_PASS_BY_VALUE)
        *((int64_t *)SDDS_dataset->parameter[index]) = va_arg(argptr, double);
      else
        *((int64_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
      break;
    case SDDS_ULONG64:
      if (mode & SDDS_PASS_BY_VALUE)
        *((uint64_t *)SDDS_dataset->parameter[index]) = va_arg(argptr, double);
      else
        *((uint64_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
      break;
    case SDDS_FLOAT:
      if (mode & SDDS_PASS_BY_VALUE)
        *((float *)SDDS_dataset->parameter[index]) = (float)va_arg(argptr, double);
      else
        *((float *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
      break;
    case SDDS_DOUBLE:
      if (mode & SDDS_PASS_BY_VALUE)
        *((double *)SDDS_dataset->parameter[index]) = va_arg(argptr, double);
      else
        *((double *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, double *));
      break;
    case SDDS_STRING:
    case SDDS_CHARACTER:
      SDDS_SetError("Nonnumeric data type encountered (SDDS_SetParametersFromDoubles)");
      retval = 0;
      break;
    default:
      SDDS_SetError("Unknown data type encountered (SDDS_SetParametersFromDoubles)");
      retval = 0;
    }
  } while (retval == -1);
  va_end(argptr);
  return (retval);
}

/**
 * Sets the values of one or more parameters in the current data table of an SDDS dataset using long double-precision floating-point numbers.
 *
 * This function assigns long double-precision floating-point values to specified parameters in the current data table of the given SDDS dataset.
 * It must be preceded by a call to `SDDS_StartPage` to initialize the table. Parameters can be identified either by their index or by
 * their name. The values can be passed either by value or by reference, depending on the specified mode.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode A bitwise combination of the following constants:
 *             - `SDDS_SET_BY_INDEX`: Identify parameters by their indices.
 *             - `SDDS_SET_BY_NAME`: Identify parameters by their names.
 *             - `SDDS_PASS_BY_VALUE`: Pass parameter values by value.
 *             - `SDDS_PASS_BY_REFERENCE`: Pass parameter values by reference.
 *
 *             **Mode Requirements:**
 *             - Exactly one of `SDDS_SET_BY_INDEX` or `SDDS_SET_BY_NAME` must be set.
 *             - Exactly one of `SDDS_PASS_BY_VALUE` or `SDDS_PASS_BY_REFERENCE` must be set.
 *
 *             **Syntax Based on Mode Combination:**
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetParametersFromLongDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t index, long double value)`
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetParametersFromLongDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t index, long double *data)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetParametersFromLongDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, char *name, long double value)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetParametersFromLongDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, char *name, long double *data)`
 *
 *             **Note:** For parameters of type `SDDS_STRING`, setting values using this function is not supported and will result in an error.
 *
 * @return Returns `1` on successful assignment of all specified parameter values.
 *         On failure, returns `0` and records an appropriate error message.
 *
 * @sa SDDS_StartPage, SDDS_SetError, SDDS_GetParameterIndex
 */
int32_t SDDS_SetParametersFromLongDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  va_list argptr;
  int32_t index, retval;
  SDDS_LAYOUT *layout;
  char *name;
  char s[SDDS_MAXLINE];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetParametersFromLongDoubles"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME) || !(mode & SDDS_PASS_BY_VALUE || mode & SDDS_PASS_BY_REFERENCE)) {
    SDDS_SetError("Unable to set parameter values--unknown mode (SDDS_SetParametersFromLongDoubles)");
    return (0);
  }

  va_start(argptr, mode);
  layout = &SDDS_dataset->layout;

  /* variable arguments are pairs of (index, value), where index is a int32_t integer */
  retval = -1;
  do {
    if (mode & SDDS_SET_BY_INDEX) {
      if ((index = va_arg(argptr, int32_t)) == -1) {
        retval = 1;
        break;
      }
      if (index < 0 || index >= layout->n_parameters) {
        sprintf(s, "Unable to set parameter values--index %" PRId32 " out of range [%d, %" PRId32 "] (SDDS_SetParametersFromLongDoubles)", index, 0, layout->n_parameters);
        SDDS_SetError(s);
        retval = 0;
        break;
      }
    } else {
      if ((name = va_arg(argptr, char *)) == NULL) {
        retval = 1;
        break;
      }
      if ((index = SDDS_GetParameterIndex(SDDS_dataset, name)) < 0) {
        sprintf(s, "Unable to set parameter values--name %s not recognized (SDDS_SetParametersFromLongDoubles)", name);
        SDDS_SetError(s);
        retval = 0;
        break;
      }
    }
    switch (layout->parameter_definition[index].type) {
    case SDDS_SHORT:
      if (mode & SDDS_PASS_BY_VALUE)
        *((short *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
      else
        *((short *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_USHORT:
      if (mode & SDDS_PASS_BY_VALUE)
        *((unsigned short *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
      else
        *((unsigned short *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_LONG:
      if (mode & SDDS_PASS_BY_VALUE)
        *((int32_t *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
      else
        *((int32_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_ULONG:
      if (mode & SDDS_PASS_BY_VALUE)
        *((uint32_t *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
      else
        *((uint32_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_LONG64:
      if (mode & SDDS_PASS_BY_VALUE)
        *((int64_t *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
      else
        *((int64_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_ULONG64:
      if (mode & SDDS_PASS_BY_VALUE)
        *((uint64_t *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
      else
        *((uint64_t *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_FLOAT:
      if (mode & SDDS_PASS_BY_VALUE)
        *((float *)SDDS_dataset->parameter[index]) = (float)va_arg(argptr, long double);
      else
        *((float *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_DOUBLE:
      if (mode & SDDS_PASS_BY_VALUE)
        *((double *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
      else
        *((double *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_LONGDOUBLE:
      if (mode & SDDS_PASS_BY_VALUE)
        *((long double *)SDDS_dataset->parameter[index]) = va_arg(argptr, long double);
      else
        *((long double *)SDDS_dataset->parameter[index]) = *(va_arg(argptr, long double *));
      break;
    case SDDS_STRING:
    case SDDS_CHARACTER:
      SDDS_SetError("Nonnumeric data type encountered (SDDS_SetParametersFromLongDoubles)");
      retval = 0;
      break;
    default:
      SDDS_SetError("Unknown data type encountered (SDDS_SetParametersFromLongDoubles)");
      retval = 0;
    }
  } while (retval == -1);
  va_end(argptr);
  return (retval);
}

/**
 * Sets the values of one or more columns in a specified row of the current data table of an SDDS dataset.
 *
 * This function assigns values to specified columns in a particular row of the current data table within the given SDDS dataset.
 * It must be preceded by a call to `SDDS_StartPage` to initialize the table. Columns can be identified either by their index or by
 * their name. The values can be passed either by value or by reference, depending on the specified mode.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode A bitwise combination of the following constants:
 *             - `SDDS_SET_BY_INDEX`: Identify columns by their indices.
 *             - `SDDS_SET_BY_NAME`: Identify columns by their names.
 *             - `SDDS_PASS_BY_VALUE`: Pass column values by value.
 *             - `SDDS_PASS_BY_REFERENCE`: Pass column values by reference.
 *
 *             **Mode Requirements:**
 *             - Exactly one of `SDDS_SET_BY_INDEX` or `SDDS_SET_BY_NAME` must be set.
 *             - Exactly one of `SDDS_PASS_BY_VALUE` or `SDDS_PASS_BY_REFERENCE` must be set.
 *
 *             **Syntax Based on Mode Combination:**
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetRowValues(SDDS_DATASET *SDDS_dataset, int32_t mode, int64_t row, int32_t index, value, ..., -1)`
 *             - `SDDS_SET_BY_INDEX` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetRowValues(SDDS_DATASET *SDDS_dataset, int32_t mode, int64_t row, int32_t index, void *data, ..., -1)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_VALUE`:
 *               `int32_t SDDS_SetRowValues(SDDS_DATASET *SDDS_dataset, int32_t mode, int64_t row, char *name, value, ..., NULL)`
 *             - `SDDS_SET_BY_NAME` + `SDDS_PASS_BY_REFERENCE`:
 *               `int32_t SDDS_SetRowValues(SDDS_DATASET *SDDS_dataset, int32_t mode, int64_t row, char *name, void *data, ..., NULL)`
 *
 *             **Note:** For columns of type `SDDS_STRING`, passing by value means passing a `char *`,
 *             whereas passing by reference means passing a `char **`.
 *
 * @param row The row number in the data table where the column values will be set. Row numbering starts from 1.
 *
 * @return Returns `1` on successful assignment of all specified column values.
 *         On failure, returns `0` and records an appropriate error message.
 *
 * @sa SDDS_StartPage, SDDS_SetError, SDDS_GetColumnIndex, SDDS_CopyString
 */
int32_t SDDS_SetRowValues(SDDS_DATASET *SDDS_dataset, int32_t mode, int64_t row, ...) {
  va_list argptr;
  int32_t index;
  int32_t retval;
  SDDS_LAYOUT *layout;
  char *name;
  char buffer[200];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetRowValues"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME) || !(mode & SDDS_PASS_BY_VALUE || mode & SDDS_PASS_BY_REFERENCE)) {
    SDDS_SetError("Unable to set column values--unknown mode (SDDS_SetRowValues)");
    return (0);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_SetRowValues"))
    return (0);
  row -= SDDS_dataset->first_row_in_mem;
  if (row >= SDDS_dataset->n_rows_allocated) {
    sprintf(buffer, "Unable to set column values--row number (%" PRId64 ") exceeds exceeds allocated memory (%" PRId64 ") (SDDS_SetRowValues)", row, SDDS_dataset->n_rows_allocated);
    SDDS_SetError(buffer);
    return (0);
  }
  if (row > SDDS_dataset->n_rows - 1)
    SDDS_dataset->n_rows = row + 1;

  va_start(argptr, row);
  layout = &SDDS_dataset->layout;

  /* variable arguments are pairs of (index, value), where index is a int32_t integer */
  retval = -1;
#ifdef DEBUG
  fprintf(stderr, "setting row %" PRId64 " (mem slot %" PRId64 ")\n", row + SDDS_dataset->first_row_in_mem, row);
#endif
  do {
    if (mode & SDDS_SET_BY_INDEX) {
      if ((index = va_arg(argptr, int32_t)) == -1) {
        retval = 1;
        break;
      }
      if (index < 0 || index >= layout->n_columns) {
        SDDS_SetError("Unable to set column values--index out of range (SDDS_SetRowValues)");
        retval = 0;
        break;
      }
#ifdef DEBUG
      fprintf(stderr, "Setting values for column #%" PRId32 "\n", index);
#endif
    } else {
      if ((name = va_arg(argptr, char *)) == NULL) {
        retval = 1;
        break;
      }
#ifdef DEBUG
      fprintf(stderr, "Setting values for column %s\n", name);
#endif
      if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0) {
        SDDS_SetError("Unable to set column values--name not recognized (SDDS_SetRowValues)");
        retval = 0;
        break;
      }
    }
    switch (layout->column_definition[index].type) {
    case SDDS_SHORT:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((short *)SDDS_dataset->data[index]) + row) = (short)va_arg(argptr, int);
      else
        *(((short *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, short *));
      break;
    case SDDS_USHORT:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((unsigned short *)SDDS_dataset->data[index]) + row) = (unsigned short)va_arg(argptr, unsigned int);
      else
        *(((unsigned short *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, unsigned short *));
      break;
    case SDDS_LONG:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((int32_t *)SDDS_dataset->data[index]) + row) = va_arg(argptr, int32_t);
      else
        *(((int32_t *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, int32_t *));
      break;
    case SDDS_ULONG:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((uint32_t *)SDDS_dataset->data[index]) + row) = va_arg(argptr, uint32_t);
      else
        *(((uint32_t *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, uint32_t *));
      break;
    case SDDS_LONG64:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((int64_t *)SDDS_dataset->data[index]) + row) = va_arg(argptr, int64_t);
      else
        *(((int64_t *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, int64_t *));
      break;
    case SDDS_ULONG64:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((uint64_t *)SDDS_dataset->data[index]) + row) = va_arg(argptr, uint64_t);
      else
        *(((uint64_t *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, uint64_t *));
      break;
    case SDDS_FLOAT:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((float *)SDDS_dataset->data[index]) + row) = (float)va_arg(argptr, double);
      else
        *(((float *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, float *));
      break;
    case SDDS_DOUBLE:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((double *)SDDS_dataset->data[index]) + row) = va_arg(argptr, double);
      else
        *(((double *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, double *));
      break;
    case SDDS_LONGDOUBLE:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((long double *)SDDS_dataset->data[index]) + row) = va_arg(argptr, long double);
      else
        *(((long double *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, long double *));
      break;
    case SDDS_STRING:
      if (((char **)SDDS_dataset->data[index])[row]) {
        free(((char **)SDDS_dataset->data[index])[row]);
        ((char **)SDDS_dataset->data[index])[row] = NULL;
      }
      if (mode & SDDS_PASS_BY_VALUE) {
        if (!SDDS_CopyString((char **)SDDS_dataset->data[index] + row, va_arg(argptr, char *))) {
          SDDS_SetError("Unable to set string column value--allocation failure (SDDS_SetRowValues)");
          retval = 0;
        }
      } else {
        if (!SDDS_CopyString((char **)SDDS_dataset->data[index] + row, *(va_arg(argptr, char **)))) {
          SDDS_SetError("Unable to set string column value--allocation failure (SDDS_SetRowValues)");
          retval = 0;
        }
      }
      break;
    case SDDS_CHARACTER:
      if (mode & SDDS_PASS_BY_VALUE)
        *(((char *)SDDS_dataset->data[index]) + row) = (char)va_arg(argptr, int);
      else
        *(((char *)SDDS_dataset->data[index]) + row) = *(va_arg(argptr, char *));
      break;
    default:
      SDDS_SetError("Unknown data type encountered (SDDS_SetRowValues");
      retval = 0;
      break;
    }
  } while (retval == -1);
  va_end(argptr);
  return (retval);
}

/**
 * @brief Sets the values of an array variable in the SDDS dataset using variable arguments for dimensions.
 *
 * This function assigns data to a specified array within the current SDDS dataset. The dimensions of the array are
 * provided as variable arguments, allowing for flexible assignment of multi-dimensional arrays. The `mode` parameter
 * controls how the data is interpreted and stored. This function handles both pointer arrays and contiguous data.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param array_name The name of the array to set within the dataset.
 * @param mode Bitwise flags that determine how the array is set. Valid flags include:
 *             - `SDDS_POINTER_ARRAY`: Indicates that the array is a pointer array.
 *             - `SDDS_CONTIGUOUS_DATA`: Indicates that the data is contiguous in memory.
 * @param data_pointer Pointer to the data to be assigned to the array. The data must match the type defined for the array.
 * @param ... Variable arguments specifying the dimensions of the array. The number of dimensions should match the array definition.
 *
 * @return Returns `1` on successful assignment of the array data.
 *         On failure, returns `0` and records an appropriate error message.
 *
 * @sa SDDS_Realloc, SDDS_CopyStringArray, SDDS_SetError, SDDS_GetArrayIndex, SDDS_ZeroMemory, SDDS_AdvanceCounter
 */
int32_t SDDS_SetArrayVararg(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t mode, void *data_pointer, ...) {
  va_list argptr;
  int32_t index, retval, i, size;
  int32_t *counter = NULL;
  SDDS_LAYOUT *layout;
  SDDS_ARRAY *array;
  void *ptr;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetArrayVararg"))
    return (0);
  if (!(mode & SDDS_POINTER_ARRAY) && !(mode & SDDS_CONTIGUOUS_DATA)) {
    SDDS_SetError("Unable to set array--invalid mode (SDDS_SetArrayVararg)");
    return (0);
  }
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
    SDDS_SetError("Unable to set array--unknown array name given (SDDS_SetArrayVararg)");
    return (0);
  }
  if (!SDDS_dataset->array) {
    SDDS_SetError("Unable to set array--internal array pointer is NULL (SDDS_SetArrayVararg)");
    return (0);
  }

  layout = &SDDS_dataset->layout;
  array = SDDS_dataset->array + index;
  if (!layout->array_definition) {
    SDDS_SetError("Unable to set array--internal array definition pointer is NULL (SDDS_SetArrayVararg)");
    return (0);
  }
  array->definition = layout->array_definition + index;
  if (!array->dimension && !(array->dimension = (int32_t *)SDDS_Malloc(sizeof(*array->dimension) * array->definition->dimensions))) {
    SDDS_SetError("Unable to set array--allocation failure (SDDS_SetArrayVararg)");
    return (0);
  }

  va_start(argptr, data_pointer);

  /* variable arguments are dimensions */
  retval = 1;
  index = 0;
  array->elements = 1;
  do {
    if ((array->dimension[index] = va_arg(argptr, int32_t)) < 0) {
      SDDS_SetError("Unable to set array--negative dimension given (SDDS_SetArrayVararg)");
      retval = 0;
      break;
    }
    array->elements *= array->dimension[index];
  } while (retval == 1 && ++index < array->definition->dimensions);
  va_end(argptr);

  if (!retval)
    return (0);
  if (!array->elements)
    return (1);
  if (!data_pointer) {
    SDDS_SetError("Unable to set array--data pointer is NULL (SDDS_SetArrayVararg)");
    return (0);
  }

  size = SDDS_type_size[array->definition->type - 1];
  if (!(array->data = SDDS_Realloc(array->data, size * array->elements))) {
    SDDS_SetError("Unable to set array--allocation failure (SDDS_SetArrayVararg");
    return (0);
  }

  /* handle 1-d arrays and contiguous data as a special case */
  if (array->definition->dimensions == 1 || mode & SDDS_CONTIGUOUS_DATA) {
    if (array->definition->type != SDDS_STRING)
      memcpy(array->data, data_pointer, size * array->elements);
    else if (!SDDS_CopyStringArray(array->data, data_pointer, array->elements)) {
      SDDS_SetError("Unable to set array--string copy failure (SDDS_SetArrayVararg");
      return (0);
    }
    return (1);
  }

  if (!(counter = SDDS_Realloc(counter, sizeof(*counter) * (array->elements - 1)))) {
    SDDS_SetError("Unable to set array--allocation failure (SDDS_SetArrayVararg");
    return (0);
  }
  SDDS_ZeroMemory(counter, sizeof(*counter) * (array->elements - 1));
  index = 0;
  do {
    ptr = data_pointer;
    for (i = 0; i < array->definition->dimensions - 1; i++)
      ptr = ((void **)ptr)[counter[i]];
    if (array->definition->type != SDDS_STRING)
      memcpy((char *)array->data + size * index, ptr, size * array->dimension[i]);
    else if (!SDDS_CopyStringArray(((char **)array->data) + index, ptr, array->dimension[i])) {
      SDDS_SetError("Unable to set array--string copy failure (SDDS_SetArrayVararg");
      return (0);
    }
    index += array->dimension[i];
  } while (SDDS_AdvanceCounter(counter, array->dimension, array->definition->dimensions - 1) != -1);
  if (counter)
    free(counter);
  return (1);
}

/**
 * @brief Sets the values of an array variable in the SDDS dataset using specified dimensions.
 *
 * This function assigns data to a specified array within the current SDDS dataset. The dimensions of the array are
 * provided as an array of integers, allowing for the assignment of multi-dimensional arrays. The `mode` parameter
 * controls how the data is interpreted and stored. This function handles both pointer arrays and contiguous data.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param array_name The name of the array to set within the dataset.
 * @param mode Bitwise flags that determine how the array is set. Valid flags include:
 *             - `SDDS_POINTER_ARRAY`: Indicates that the array is a pointer array.
 *             - `SDDS_CONTIGUOUS_DATA`: Indicates that the data is contiguous in memory.
 * @param data_pointer Pointer to the data to be assigned to the array. The data must match the type defined for the array.
 * @param dimension Pointer to an array of integers specifying the dimensions of the array. The number of dimensions should
 *                  match the array definition.
 *
 * @return Returns `1` on successful assignment of the array data.
 *         On failure, returns `0` and records an appropriate error message.
 *
 * @sa SDDS_Realloc, SDDS_CopyStringArray, SDDS_SetError, SDDS_GetArrayIndex, SDDS_ZeroMemory, SDDS_AdvanceCounter
 */
int32_t SDDS_SetArray(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t mode, void *data_pointer, int32_t *dimension) {
  int32_t index, i, size;
  int32_t *counter = NULL;
  SDDS_LAYOUT *layout;
  SDDS_ARRAY *array;
  void *ptr;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetArray"))
    return (0);
  if (!(mode & SDDS_POINTER_ARRAY) && !(mode & SDDS_CONTIGUOUS_DATA)) {
    SDDS_SetError("Unable to set array--invalid mode (SDDS_SetArray)");
    return (0);
  }
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
    SDDS_SetError("Unable to set array--unknown array name given (SDDS_SetArray)");
    return (0);
  }

  if (!dimension) {
    SDDS_SetError("Unable to set array--dimension pointer is NULL (SDDS_SetArray)");
    return (0);
  }
  if (!SDDS_dataset->array) {
    SDDS_SetError("Unable to set array--internal array pointer is NULL (SDDS_SetArray)");
    return (0);
  }

  layout = &SDDS_dataset->layout;
  array = SDDS_dataset->array + index;
  if (!layout->array_definition) {
    SDDS_SetError("Unable to set array--internal array definition pointer is NULL (SDDS_SetArray)");
    return (0);
  }
  array->definition = layout->array_definition + index;
  if (!array->dimension && !(array->dimension = (int32_t *)SDDS_Malloc(sizeof(*array->dimension) * array->definition->dimensions))) {
    SDDS_SetError("Unable to set array--allocation failure (SDDS_SetArray)");
    return (0);
  }
  array->elements = 1;
  for (i = 0; i < array->definition->dimensions; i++) {
    if ((array->dimension[i] = dimension[i]) < 0) {
      SDDS_SetError("Unable to set array--negative dimension specified (SDDS_SetArray)");
      return (0);
    }
    array->elements *= dimension[i];
    if (array->elements && !data_pointer) {
      SDDS_SetError("Unable to set array--data pointer is NULL (SDDS_SetArray)");
      return (0);
    }
  }
  if (!array->elements)
    return (1);

  size = SDDS_type_size[array->definition->type - 1];
  if (!(array->data = SDDS_Realloc(array->data, size * array->elements))) {
    SDDS_SetError("Unable to set array--allocation failure (SDDS_SetArray)");
    return (0);
  }

  /* handle 1-d arrays and contiguous data as a special case */
  if (array->definition->dimensions == 1 || mode & SDDS_CONTIGUOUS_DATA) {
    if (array->definition->type != SDDS_STRING)
      memcpy(array->data, data_pointer, size * array->elements);
    else if (!SDDS_CopyStringArray(array->data, data_pointer, array->elements)) {
      SDDS_SetError("Unable to set array--string copy failure (SDDS_SetArrayVararg");
      return (0);
    }
    return (1);
  }

  if (!(counter = SDDS_Realloc(counter, sizeof(*counter) * (array->elements - 1)))) {
    SDDS_SetError("Unable to set array--allocation failure (SDDS_SetArray)");
    return (0);
  }
  SDDS_ZeroMemory(counter, sizeof(*counter) * (array->elements - 1));
  index = 0;
  do {
    ptr = data_pointer;
    for (i = 0; i < array->definition->dimensions - 1; i++)
      ptr = ((void **)ptr)[counter[i]];
    if (array->definition->type != SDDS_STRING)
      memcpy((char *)array->data + size * index, ptr, size * array->dimension[i]);
    else if (!SDDS_CopyStringArray(((char **)array->data) + index, ptr, array->dimension[i])) {
      SDDS_SetError("Unable to set array--string copy failure (SDDS_SetArray)");
      return (0);
    }
    index += array->dimension[i];
  } while (SDDS_AdvanceCounter(counter, array->dimension, array->definition->dimensions - 1) != -1);
  if (counter)
    free(counter);
  return (1);
}

/**
 * @brief Appends data to an existing array variable in the SDDS dataset using variable arguments for dimensions.
 *
 * This function appends additional data to a specified array within the current SDDS dataset. The `elements` parameter
 * specifies the number of new elements to append. The `mode` parameter controls how the data is interpreted and stored.
 * The dimensions of the array are provided as variable arguments, allowing for flexible handling of multi-dimensional arrays.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param array_name The name of the array to append data to within the dataset.
 * @param mode Bitwise flags that determine how the array is set. Valid flags include:
 *             - `SDDS_POINTER_ARRAY`: Indicates that the array is a pointer array.
 *             - `SDDS_CONTIGUOUS_DATA`: Indicates that the data is contiguous in memory.
 * @param data_pointer Pointer to the data to be appended to the array. The data must match the type defined for the array.
 * @param elements The number of elements to append to the array.
 * @param ... Variable arguments specifying the dimensions of the array. The number of dimensions should match the array definition.
 *
 * @return Returns `1` on successful appending of the array data.
 *         On failure, returns `0` and records an appropriate error message.
 *
 * @sa SDDS_Realloc, SDDS_CopyStringArray, SDDS_SetError, SDDS_GetArrayIndex
 */
int32_t SDDS_AppendToArrayVararg(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t mode, void *data_pointer, int32_t elements, ...) {
  va_list argptr;
  int32_t index, retval, size, startIndex = 0;
  SDDS_LAYOUT *layout;
  SDDS_ARRAY *array;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_AppendToArrayVararg"))
    return (0);
  if (!(mode & SDDS_POINTER_ARRAY) && !(mode & SDDS_CONTIGUOUS_DATA)) {
    SDDS_SetError("Unable to set array--invalid mode (SDDS_AppendToArrayVararg)");
    return (0);
  }
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
    SDDS_SetError("Unable to set array--unknown array name given (SDDS_AppendToArrayVararg)");
    return (0);
  }
  if (!data_pointer) {
    SDDS_SetError("Unable to set array--data pointer is NULL (SDDS_AppendToArrayVararg)");
    return (0);
  }
  if (!SDDS_dataset->array) {
    SDDS_SetError("Unable to set array--internal array pointer is NULL (SDDS_AppendToArrayVararg)");
    return (0);
  }

  layout = &SDDS_dataset->layout;
  array = SDDS_dataset->array + index;
  if (!layout->array_definition) {
    SDDS_SetError("Unable to set array--internal array definition pointer is NULL (SDDS_AppendToArrayVararg)");
    return (0);
  }
  array->definition = layout->array_definition + index;
  if (!array->dimension && !(array->dimension = (int32_t *)SDDS_Malloc(sizeof(*array->dimension) * array->definition->dimensions))) {
    SDDS_SetError("Unable to set array--allocation failure (SDDS_SetArrayVararg)");
    return (0);
  }
  if (!(array->definition->dimensions == 1 || mode & SDDS_CONTIGUOUS_DATA)) {
    SDDS_SetError("Unable to set array--append operation requires contiguous data (SDDS_AppendToArrayVararg)");
    return (0);
  }

  va_start(argptr, elements);

  /* variable arguments are dimensions */
  retval = 1;
  index = 0;
  array->elements = 1;
  do {
    if ((array->dimension[index] = va_arg(argptr, int32_t)) < 0) {
      SDDS_SetError("Unable to set array--negative dimension given (SDDS_AppendToArrayVararg)");
      retval = 0;
      break;
    }
    array->elements *= array->dimension[index];
  } while (retval == 1 && ++index < array->definition->dimensions);
  va_end(argptr);

  if (!retval)
    return (0);
  if (!array->elements)
    return (1);

  size = SDDS_type_size[array->definition->type - 1];
  if (!(array->data = SDDS_Realloc(array->data, size * array->elements))) {
    SDDS_SetError("Unable to set array--allocation failure (SDDS_AppendToArrayVararg)");
    return (0);
  }

  startIndex = array->elements - elements;

  /* handle 1-d arrays and contiguous data as a special case */
  if (array->definition->dimensions == 1 || mode & SDDS_CONTIGUOUS_DATA) {
    if (array->definition->type != SDDS_STRING)
      memcpy((char *)array->data + size * startIndex, data_pointer, size * elements);
    else if (!SDDS_CopyStringArray(((char **)array->data) + startIndex, data_pointer, elements)) {
      SDDS_SetError("Unable to set array--string copy failure (SDDS_AppendToArrayVararg)");
      return (0);
    }
    return (1);
  }

  return (1);
}

/**
 * @brief Advances a multi-dimensional counter based on maximum counts for each dimension.
 *
 * This helper function increments a multi-dimensional counter array, handling carry-over for each dimension. It is typically
 * used for iterating over multi-dimensional arrays in a nested loop fashion.
 *
 * @param counter Pointer to an array of integers representing the current count in each dimension.
 * @param max_count Pointer to an array of integers representing the maximum count for each dimension.
 * @param n_indices The number of dimensions (indices) in the counter and max_count arrays.
 *
 * @return Returns the index of the dimension that was incremented. If all dimensions have been fully iterated over,
 *         returns `-1` to indicate completion.
 *
 * @sa SDDS_SetArrayVararg, SDDS_SetArray, SDDS_AdvanceCounter
 */
int32_t SDDS_AdvanceCounter(int32_t *counter, int32_t *max_count, int32_t n_indices) {
  int32_t i;

  for (i = n_indices - 1; i >= 0; i--)
    if (counter[i] != (max_count[i] - 1))
      break;
  if (i == -1)
    return (-1);

  for (i = n_indices - 1; i >= 0; i--) {
    if (counter[i] < (max_count[i] - 1)) {
      counter[i]++;
      break;
    } else {
      counter[i] = 0;
    }
  }
  return (i);
}

/**
 * @brief Sets the values for one data column in the current data table of an SDDS dataset.
 *
 * This function assigns data to a specified column within the current data table of the given SDDS dataset. The column
 * can be identified either by its index or by its name. The `mode` parameter determines how the column is identified.
 * The function ensures that the number of rows in the new column matches the existing data table.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode Bitwise flags that determine how the column is identified. Valid flags include:
 *             - `SDDS_SET_BY_INDEX`: Identify the column by its index.
 *             - `SDDS_SET_BY_NAME`: Identify the column by its name.
 * @param data Pointer to an array of data to be assigned to the column. The elements of the array must be of the same type as the column type.
 * @param rows The number of rows in the column. This should match the number of rows in the existing data table.
 * @param ... Variable arguments specifying either the column index or column name, depending on the `mode` parameter:
 *             - If `mode` includes `SDDS_SET_BY_INDEX`: Provide an `int32_t` index.
 *             - If `mode` includes `SDDS_SET_BY_NAME`: Provide a `char*` name.
 *
 * @return Returns `1` on successful assignment of the column data.
 *         On failure, returns `0` and records an appropriate error message.
 *
 * @note The function ensures that the number of rows in the new column matches the existing data table. If the data type of the column is `SDDS_STRING`,
 *       it handles memory allocation and copying of strings appropriately.
 *
 * @sa SDDS_SetRowValues, SDDS_SetError, SDDS_GetColumnIndex, SDDS_CopyStringArray
 */
int32_t SDDS_SetColumn(SDDS_DATASET *SDDS_dataset, int32_t mode, void *data, int64_t rows, ...) {
  va_list argptr;
  int32_t index;
  int32_t retval;
  SDDS_LAYOUT *layout;
  char *name;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetColumn"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME)) {
    SDDS_SetError("Unable to set column values--unknown mode (SDDS_SetColumn)");
    return (0);
  }
  if (rows > SDDS_dataset->n_rows_allocated) {
    SDDS_SetError("Unable to set column values--number of rows exceeds allocated memory (SDDS_SetColumn)");
    return (0);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_SetColumn"))
    return (0);
  if (SDDS_dataset->n_rows != 0 && SDDS_dataset->n_rows != rows) {
    SDDS_SetError("Number of rows in new column unequal to number in other columns (SDDS_SetColumn)");
    return (0);
  }
  SDDS_dataset->n_rows = rows;
  layout = &SDDS_dataset->layout;

  retval = 1;
  va_start(argptr, rows);
  if (mode & SDDS_SET_BY_INDEX) {
    index = va_arg(argptr, int32_t);
    if (index < 0 || index >= layout->n_columns) {
      SDDS_SetError("Unable to set column values--index out of range (SDDS_SetColumn)");
      retval = 0;
    }
  } else {
    name = va_arg(argptr, char *);
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0) {
      SDDS_SetError0("Unable to set column values--name ");
      SDDS_SetError0(name);
      SDDS_SetError(" not recognized (SDDS_SetColumn)");
      retval = 0;
    }
  }
  va_end(argptr);
  if (!retval)
    return 0;

  if (layout->column_definition[index].type == SDDS_STRING) {
    if (SDDS_dataset->data[index]) {
      char *ptr;
      int64_t i;
      for (i = 0; i < rows; i++) {
        ptr = *((char **)SDDS_dataset->data[index] + i);
        if (ptr)
          free(ptr);
        *((char **)SDDS_dataset->data[index] + i) = NULL;
      }
    }
    if (!SDDS_CopyStringArray((char **)(SDDS_dataset->data[index]), (char **)data, rows)) {
      SDDS_SetError("Unable to set column--error copying string data (SDDS_SetColumn)");
      return 0;
    }
  } else
    memcpy(SDDS_dataset->data[index], data, rows * SDDS_type_size[layout->column_definition[index].type - 1]);
  return 1;
}

/**
 * @brief Sets the values for a single data column using double-precision floating-point numbers.
 *
 * This function assigns data to a specified column within the current data table of the given SDDS dataset.
 * The column can be identified either by its index or by its name, based on the provided `mode`. The data
 * provided must be in the form of double-precision floating-point numbers (`double`). If the target column
 * is of a different numeric type, the function will perform the necessary type casting. For string columns,
 * the function converts the double values to strings with appropriate formatting.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode Bitwise flags that determine how the column is identified. Valid flags include:
 *             - `SDDS_SET_BY_INDEX`: Identify the column by its index.
 *             - `SDDS_SET_BY_NAME`: Identify the column by its name.
 *
 *             **Mode Requirements:**
 *             - Exactly one of `SDDS_SET_BY_INDEX` or `SDDS_SET_BY_NAME` must be set.
 *
 *             **Syntax Based on Mode Combination:**
 *             - `SDDS_SET_BY_INDEX`:
 *               `int32_t SDDS_SetColumnFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, double *data, int64_t rows, int32_t index)`
 *             - `SDDS_SET_BY_NAME`:
 *               `int32_t SDDS_SetColumnFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, double *data, int64_t rows, char *name)`
 *
 * @param data Pointer to an array of double-precision floating-point data to be assigned to the column.
 *             The array should contain at least `rows` elements.
 * @param rows The number of rows (elements) in the column to be set. Must not exceed the allocated memory for the dataset.
 * @param ... Variable arguments specifying either the column index (`int32_t`) or column name (`char *`), depending on the `mode`.
 *
 * @return Returns `1` on successful assignment of the column data.
 *         On failure, returns `0` and records an appropriate error message using `SDDS_SetError`.
 *
 * @note
 * - If the target column is of type `SDDS_STRING`, the function converts each double value to a string
 *   with a precision of up to 15 significant digits.
 * - The function ensures that the number of rows in the new column matches the existing data table.
 * - It is required to call `SDDS_StartPage` before setting column values.
 *
 * @sa SDDS_SetColumnFromLongDoubles, SDDS_SetError, SDDS_GetColumnIndex, SDDS_CopyStringArray, SDDS_CastValue
 */
int32_t SDDS_SetColumnFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, double *data, int64_t rows, ...) {
  va_list argptr;
  int64_t i;
  int32_t index, retval, type, size;
  SDDS_LAYOUT *layout;
  char *name;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetColumnFromDoubles"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME)) {
    SDDS_SetError("Unable to set column values--unknown mode (SDDS_SetColumnFromDoubles)");
    return (0);
  }
  if (rows > SDDS_dataset->n_rows_allocated) {
    SDDS_SetError("Unable to set column values--number of rows exceeds allocated memory (SDDS_SetColumnFromDoubles)");
    return (0);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_SetColumnFromDoubles"))
    return (0);
  if (SDDS_dataset->n_rows != 0 && SDDS_dataset->n_rows != rows) {
    SDDS_SetError("Number of rows in new column unequal to number in other columns (SDDS_SetColumnFromDoubles)");
    return (0);
  }
  SDDS_dataset->n_rows = rows;
  layout = &SDDS_dataset->layout;

  retval = 1;
  va_start(argptr, rows);
  if (mode & SDDS_SET_BY_INDEX) {
    index = va_arg(argptr, int32_t);
    if (index < 0 || index >= layout->n_columns) {
      SDDS_SetError("Unable to set column values--index out of range (SDDS_SetColumnFromDoubles)");
      retval = 0;
    }
  } else {
    name = va_arg(argptr, char *);
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0) {
      SDDS_SetError("Unable to set column values--name not recognized (SDDS_SetColumnFromDoubles)");
      retval = 0;
    }
  }
  va_end(argptr);
  if (!retval)
    return 0;

  type = layout->column_definition[index].type;
  if (!SDDS_NUMERIC_TYPE(type)) {
    if (type == SDDS_STRING) {
      char **stringArray;
      if (SDDS_dataset->data[index]) {
        char *ptr;
        int64_t i;
        for (i = 0; i < rows; i++) {
          ptr = *((char **)SDDS_dataset->data[index] + i);
          if (ptr)
            free(ptr);
          *((char **)SDDS_dataset->data[index] + i) = NULL;
        }
      }
      stringArray = (char **)malloc(sizeof(char *) * rows);
      for (i = 0; i < rows; i++) {
        stringArray[i] = (char *)malloc(sizeof(char) * 40);
        sprintf(stringArray[i], "%.15lg", data[i]);
      }
      if (!SDDS_CopyStringArray((char **)(SDDS_dataset->data[index]), (char **)stringArray, rows)) {
        SDDS_SetError("Unable to set column--error copying string data (SDDS_SetColumnFromDoubles)");
        return 0;
      }
      for (i = 0; i < rows; i++) {
        free(stringArray[i]);
      }
      free(stringArray);
      return 1;
    }
    SDDS_SetError("Unable to set column--source type is nonnumeric (SDDS_SetColumnFromDoubles)");
    return 0;
  }

  size = SDDS_type_size[layout->column_definition[index].type - 1];

  if (type == SDDS_DOUBLE) {
    memcpy((char *)SDDS_dataset->data[index], (char *)data, rows * size);
    return 1;
  }

  for (i = 0; i < rows; i++)
    if (!SDDS_CastValue(data, i, SDDS_DOUBLE, type, (char *)(SDDS_dataset->data[index]) + i * size)) {
      SDDS_SetError("Unable to set column--cast error (SDDS_SetColumnFromDoubles)");
      return 0;
    }

  return 1;
}

/**
 * @brief Sets the values for a single data column using long double-precision floating-point numbers.
 *
 * This function assigns data to a specified column within the current data table of the given SDDS dataset.
 * The column can be identified either by its index or by its name, based on the provided `mode`. The data
 * provided must be in the form of long double-precision floating-point numbers (`long double`). If the target
 * column is of a different numeric type, the function will perform the necessary type casting. For string columns,
 * the function converts the long double values to strings with appropriate formatting.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode Bitwise flags that determine how the column is identified. Valid flags include:
 *             - `SDDS_SET_BY_INDEX`: Identify the column by its index.
 *             - `SDDS_SET_BY_NAME`: Identify the column by its name.
 *
 *             **Mode Requirements:**
 *             - Exactly one of `SDDS_SET_BY_INDEX` or `SDDS_SET_BY_NAME` must be set.
 *
 *             **Syntax Based on Mode Combination:**
 *             - `SDDS_SET_BY_INDEX`:
 *               `int32_t SDDS_SetColumnFromLongDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, long double *data, int64_t rows, int32_t index)`
 *             - `SDDS_SET_BY_NAME`:
 *               `int32_t SDDS_SetColumnFromLongDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, long double *data, int64_t rows, char *name)`
 *
 * @param data Pointer to an array of long double-precision floating-point data to be assigned to the column.
 *             The array should contain at least `rows` elements.
 * @param rows The number of rows (elements) in the column to be set. Must not exceed the allocated memory for the dataset.
 * @param ... Variable arguments specifying either the column index (`int32_t`) or column name (`char *`), depending on the `mode`.
 *
 * @return Returns `1` on successful assignment of the column data.
 *         On failure, returns `0` and records an appropriate error message using `SDDS_SetError`.
 *
 * @note
 * - If the target column is of type `SDDS_STRING`, the function converts each long double value to a string
 *   with a precision of up to 18 significant digits if supported, otherwise 15 digits.
 * - The function ensures that the number of rows in the new column matches the existing data table.
 * - It is required to call `SDDS_StartPage` before setting column values.
 *
 * @sa SDDS_SetColumnFromDoubles, SDDS_SetError, SDDS_GetColumnIndex, SDDS_CopyStringArray, SDDS_CastValue
 */
int32_t SDDS_SetColumnFromLongDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, long double *data, int64_t rows, ...) {
  va_list argptr;
  int64_t i;
  int32_t index, retval, type, size;
  SDDS_LAYOUT *layout;
  char *name;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetColumnFromLongDoubles"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME)) {
    SDDS_SetError("Unable to set column values--unknown mode (SDDS_SetColumnFromLongDoubles)");
    return (0);
  }
  if (rows > SDDS_dataset->n_rows_allocated) {
    SDDS_SetError("Unable to set column values--number of rows exceeds allocated memory (SDDS_SetColumnFromLongDoubles)");
    return (0);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_SetColumnFromLongDoubles"))
    return (0);
  if (SDDS_dataset->n_rows != 0 && SDDS_dataset->n_rows != rows) {
    SDDS_SetError("Number of rows in new column unequal to number in other columns (SDDS_SetColumnFromLongDoubles)");
    return (0);
  }
  SDDS_dataset->n_rows = rows;
  layout = &SDDS_dataset->layout;

  retval = 1;
  va_start(argptr, rows);
  if (mode & SDDS_SET_BY_INDEX) {
    index = va_arg(argptr, int32_t);
    if (index < 0 || index >= layout->n_columns) {
      SDDS_SetError("Unable to set column values--index out of range (SDDS_SetColumnFromLongDoubles)");
      retval = 0;
    }
  } else {
    name = va_arg(argptr, char *);
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0) {
      SDDS_SetError("Unable to set column values--name not recognized (SDDS_SetColumnFromLongDoubles)");
      retval = 0;
    }
  }
  va_end(argptr);
  if (!retval)
    return 0;

  type = layout->column_definition[index].type;
  if (!SDDS_NUMERIC_TYPE(type)) {
    if (type == SDDS_STRING) {
      char **stringArray;
      if (SDDS_dataset->data[index]) {
        char *ptr;
        int64_t i;
        for (i = 0; i < rows; i++) {
          ptr = *((char **)SDDS_dataset->data[index] + i);
          if (ptr)
            free(ptr);
          *((char **)SDDS_dataset->data[index] + i) = NULL;
        }
      }
      stringArray = (char **)malloc(sizeof(char *) * rows);
      for (i = 0; i < rows; i++) {
        stringArray[i] = (char *)malloc(sizeof(char) * 40);
        if (LDBL_DIG == 18) {
          sprintf(stringArray[i], "%.18Lg", data[i]);
        } else {
          sprintf(stringArray[i], "%.15Lg", data[i]);
        }
      }
      if (!SDDS_CopyStringArray((char **)(SDDS_dataset->data[index]), (char **)stringArray, rows)) {
        SDDS_SetError("Unable to set column--error copying string data (SDDS_SetColumnFromLongDoubles)");
        return 0;
      }
      for (i = 0; i < rows; i++) {
        free(stringArray[i]);
      }
      free(stringArray);
      return 1;
    }
    SDDS_SetError("Unable to set column--source type is nonnumeric (SDDS_SetColumnFromLongDoubles)");
    return 0;
  }

  size = SDDS_type_size[layout->column_definition[index].type - 1];

  if (type == SDDS_LONGDOUBLE) {
    memcpy((char *)SDDS_dataset->data[index], (char *)data, rows * size);
    return 1;
  }

  for (i = 0; i < rows; i++)
    if (!SDDS_CastValue(data, i, SDDS_LONGDOUBLE, type, (char *)(SDDS_dataset->data[index]) + i * size)) {
      SDDS_SetError("Unable to set column--cast error (SDDS_SetColumnFromLongDoubles)");
      return 0;
    }

  return 1;
}

/**
 * @brief Sets the values for a single data column using single-precision floating-point numbers.
 *
 * This function assigns data to a specified column within the current data table of the given SDDS dataset.
 * The column can be identified either by its index or by its name, based on the provided `mode`. The data
 * provided must be in the form of single-precision floating-point numbers (`float`). If the target column
 * is of a different numeric type, the function will perform the necessary type casting. For string columns,
 * the function converts the float values to strings with appropriate formatting.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode Bitwise flags that determine how the column is identified. Valid flags include:
 *             - `SDDS_SET_BY_INDEX`: Identify the column by its index.
 *             - `SDDS_SET_BY_NAME`: Identify the column by its name.
 *
 *             **Mode Requirements:**
 *             - Exactly one of `SDDS_SET_BY_INDEX` or `SDDS_SET_BY_NAME` must be set.
 *
 *             **Syntax Based on Mode Combination:**
 *             - `SDDS_SET_BY_INDEX`:
 *               `int32_t SDDS_SetColumnFromFloats(SDDS_DATASET *SDDS_dataset, int32_t mode, float *data, int64_t rows, int32_t index)`
 *             - `SDDS_SET_BY_NAME`:
 *               `int32_t SDDS_SetColumnFromFloats(SDDS_DATASET *SDDS_dataset, int32_t mode, float *data, int64_t rows, char *name)`
 *
 * @param data Pointer to an array of single-precision floating-point data to be assigned to the column.
 *             The array should contain at least `rows` elements.
 * @param rows The number of rows (elements) in the column to be set. Must not exceed the allocated memory for the dataset.
 * @param ... Variable arguments specifying either the column index (`int32_t`) or column name (`char *`), depending on the `mode`.
 *
 * @return Returns `1` on successful assignment of the column data.
 *         On failure, returns `0` and records an appropriate error message using `SDDS_SetError`.
 *
 * @note
 * - If the target column is of type `SDDS_STRING`, the function converts each float value to a string
 *   with a precision of up to 8 significant digits.
 * - The function ensures that the number of rows in the new column matches the existing data table.
 * - It is required to call `SDDS_StartPage` before setting column values.
 *
 * @sa SDDS_SetColumnFromDoubles, SDDS_SetError, SDDS_GetColumnIndex, SDDS_CopyStringArray, SDDS_CastValue
 */
int32_t SDDS_SetColumnFromFloats(SDDS_DATASET *SDDS_dataset, int32_t mode, float *data, int64_t rows, ...) {
  va_list argptr;
  int64_t i;
  int32_t index, retval, type, size;
  SDDS_LAYOUT *layout;
  char *name;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetColumnFromFloats"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME)) {
    SDDS_SetError("Unable to set column values--unknown mode (SDDS_SetColumnFromFloats)");
    return (0);
  }
  if (rows > SDDS_dataset->n_rows_allocated) {
    SDDS_SetError("Unable to set column values--number of rows exceeds allocated memory (SDDS_SetColumnFromFloats)");
    return (0);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_SetColumnFromFloats"))
    return (0);
  if (SDDS_dataset->n_rows != 0 && SDDS_dataset->n_rows != rows) {
    SDDS_SetError("Number of rows in new column unequal to number in other columns (SDDS_SetColumnFromFloats)");
    return (0);
  }
  SDDS_dataset->n_rows = rows;
  layout = &SDDS_dataset->layout;

  retval = 1;
  va_start(argptr, rows);
  if (mode & SDDS_SET_BY_INDEX) {
    index = va_arg(argptr, int32_t);
    if (index < 0 || index >= layout->n_columns) {
      SDDS_SetError("Unable to set column values--index out of range (SDDS_SetColumnFromFloats)");
      retval = 0;
    }
  } else {
    name = va_arg(argptr, char *);
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0) {
      SDDS_SetError("Unable to set column values--name not recognized (SDDS_SetColumnFromFloats)");
      retval = 0;
    }
  }
  va_end(argptr);
  if (!retval)
    return 0;

  type = layout->column_definition[index].type;
  if (!SDDS_NUMERIC_TYPE(type)) {
    if (type == SDDS_STRING) {
      char **stringArray;
      if (SDDS_dataset->data[index]) {
        char *ptr;
        int64_t i;
        for (i = 0; i < rows; i++) {
          ptr = *((char **)SDDS_dataset->data[index] + i);
          if (ptr)
            free(ptr);
          *((char **)SDDS_dataset->data[index] + i) = NULL;
        }
      }
      stringArray = (char **)malloc(sizeof(char *) * rows);
      for (i = 0; i < rows; i++) {
        stringArray[i] = (char *)malloc(sizeof(char) * 40);
        sprintf(stringArray[i], "%.8g", data[i]);
      }
      if (!SDDS_CopyStringArray((char **)(SDDS_dataset->data[index]), (char **)stringArray, rows)) {
        SDDS_SetError("Unable to set column--error copying string data (SDDS_SetColumnFromFloats)");
        return 0;
      }
      for (i = 0; i < rows; i++) {
        free(stringArray[i]);
      }
      free(stringArray);
      return 1;
    }
    SDDS_SetError("Unable to set column--source type is nonnumeric (SDDS_SetColumnFromFloats)");
    return 0;
  }

  size = SDDS_type_size[layout->column_definition[index].type - 1];

  if (type == SDDS_FLOAT) {
    memcpy((char *)SDDS_dataset->data[index], (char *)data, rows * size);
    return 1;
  }

  for (i = 0; i < rows; i++)
    if (!SDDS_CastValue(data, i, SDDS_FLOAT, type, (char *)(SDDS_dataset->data[index]) + i * size)) {
      SDDS_SetError("Unable to set column--cast error (SDDS_SetColumnFromFloats)");
      return 0;
    }

  return 1;
}

/**
 * @brief Sets the values for a single data column using long integer numbers.
 *
 * This function assigns data to a specified column within the current data table of the given SDDS dataset.
 * The column can be identified either by its index or by its name, based on the provided `mode`. The data
 * provided must be in the form of long integers (`int32_t`). If the target column is of a different numeric type,
 * the function will perform the necessary type casting. For string columns, the function converts the integer
 * values to strings with appropriate formatting.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode Bitwise flags that determine how the column is identified. Valid flags include:
 *             - `SDDS_SET_BY_INDEX`: Identify the column by its index.
 *             - `SDDS_SET_BY_NAME`: Identify the column by its name.
 *
 *             **Mode Requirements:**
 *             - Exactly one of `SDDS_SET_BY_INDEX` or `SDDS_SET_BY_NAME` must be set.
 *
 *             **Syntax Based on Mode Combination:**
 *             - `SDDS_SET_BY_INDEX`:
 *               `int32_t SDDS_SetColumnFromLongs(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t *data, int64_t rows, int32_t index)`
 *             - `SDDS_SET_BY_NAME`:
 *               `int32_t SDDS_SetColumnFromLongs(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t *data, int64_t rows, char *name)`
 *
 * @param data Pointer to an array of long integer data to be assigned to the column.
 *             The array should contain at least `rows` elements.
 * @param rows The number of rows (elements) in the column to be set. Must not exceed the allocated memory for the dataset.
 * @param ... Variable arguments specifying either the column index (`int32_t`) or column name (`char *`), depending on the `mode`.
 *
 * @return Returns `1` on successful assignment of the column data.
 *         On failure, returns `0` and records an appropriate error message using `SDDS_SetError`.
 *
 * @note
 * - If the target column is of type `SDDS_STRING`, the function converts each long integer value to a string
 *   using the `sprintf` function with the appropriate format specifier.
 * - The function ensures that the number of rows in the new column matches the existing data table.
 * - It is required to call `SDDS_StartPage` before setting column values.
 *
 * @sa SDDS_SetColumnFromDoubles, SDDS_SetError, SDDS_GetColumnIndex, SDDS_CopyStringArray, SDDS_CastValue
 */
int32_t SDDS_SetColumnFromLongs(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t *data, int64_t rows, ...) {
  va_list argptr;
  int64_t i;
  int32_t index, retval, type, size;
  SDDS_LAYOUT *layout;
  char *name;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetColumnFromLongs"))
    return (0);
  if (!(mode & SDDS_SET_BY_INDEX || mode & SDDS_SET_BY_NAME)) {
    SDDS_SetError("Unable to set column values--unknown mode (SDDS_SetColumnFromLongs)");
    return (0);
  }
  if (rows > SDDS_dataset->n_rows_allocated) {
    SDDS_SetError("Unable to set column values--number of rows exceeds allocated memory (SDDS_SetColumnFromLongs)");
    return (0);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_SetColumnFromLongs"))
    return (0);
  if (SDDS_dataset->n_rows != 0 && SDDS_dataset->n_rows != rows) {
    SDDS_SetError("Number of rows in new column unequal to number in other columns (SDDS_SetColumnFromLongs)");
    return (0);
  }
  SDDS_dataset->n_rows = rows;
  layout = &SDDS_dataset->layout;

  retval = 1;
  va_start(argptr, rows);
  if (mode & SDDS_SET_BY_INDEX) {
    index = va_arg(argptr, int32_t);
    if (index < 0 || index >= layout->n_columns) {
      SDDS_SetError("Unable to set column values--index out of range (SDDS_SetColumnFromLongs)");
      retval = 0;
    }
  } else {
    name = va_arg(argptr, char *);
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0) {
      SDDS_SetError("Unable to set column values--name not recognized (SDDS_SetColumnFromLongs)");
      retval = 0;
    }
  }
  va_end(argptr);
  if (!retval)
    return 0;

  type = layout->column_definition[index].type;
  if (!SDDS_NUMERIC_TYPE(type)) {
    if (type == SDDS_STRING) {
      char **stringArray;
      if (SDDS_dataset->data[index]) {
        char *ptr;
        int64_t i;
        for (i = 0; i < rows; i++) {
          ptr = *((char **)SDDS_dataset->data[index] + i);
          if (ptr)
            free(ptr);
          *((char **)SDDS_dataset->data[index] + i) = NULL;
        }
      }
      stringArray = (char **)malloc(sizeof(char *) * rows);
      for (i = 0; i < rows; i++) {
        stringArray[i] = (char *)malloc(sizeof(char) * 40);
        sprintf(stringArray[i], "%" PRId32, data[i]);
      }
      if (!SDDS_CopyStringArray((char **)(SDDS_dataset->data[index]), (char **)stringArray, rows)) {
        SDDS_SetError("Unable to set column--error copying string data (SDDS_SetColumnFromLongs)");
        return 0;
      }
      for (i = 0; i < rows; i++) {
        free(stringArray[i]);
      }
      free(stringArray);
      return 1;
    }
    SDDS_SetError("Unable to set column--source type is nonnumeric (SDDS_SetColumnFromLongs)");
    return 0;
  }

  size = SDDS_type_size[layout->column_definition[index].type - 1];

  if (type == SDDS_LONG) {
    memcpy((char *)SDDS_dataset->data[index], (char *)data, rows * size);
    return 1;
  }

  for (i = 0; i < rows; i++)
    if (!SDDS_CastValue(data, i, SDDS_LONG, type, (char *)(SDDS_dataset->data[index]) + i * size)) {
      SDDS_SetError("Unable to set column--cast error (SDDS_SetColumnFromLongs)");
      return 0;
    }

  return 1;
}
