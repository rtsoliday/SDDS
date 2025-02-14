/**
 * @file SDDS_extract.c
 * @brief This file contains routines for getting pointers to SDDS objects like columns and parameters.
 *
 * This file provides functions for extracting data in the
 * Self-Describing Data Sets (SDDS) format. 
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

#include "SDDS.h"
#include "SDDS_internal.h"
#include "mdb.h"

#if defined(_WIN32) && !defined(_MINGW)
/*#define isnan(x) _isnan(x)*/
#endif

/**
 * @brief Sets the acceptance flags for all rows in the current data table of a data set.
 *
 * This function initializes the acceptance flags for each row in the data table. A non-zero flag indicates that the row is "of interest" and should be considered in subsequent operations, while a zero flag marks the row for rejection.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param row_flag_value Integer value to assign to all row flags.
 *                      - Non-zero value: Marks rows as accepted ("of interest").
 *                      - Zero value: Marks rows as rejected.
 *
 * @return 
 *   - **1** on successful update of row flags.
 *   - **0** on failure, with an error message recorded.
 *
 * @note 
 * This function overwrites any existing row flags with the specified `row_flag_value`.
 *
 * @sa SDDS_GetRowFlag, SDDS_GetRowFlags
 */
int32_t SDDS_SetRowFlags(SDDS_DATASET *SDDS_dataset, int32_t row_flag_value) {
  /*  int32_t i; */
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetRowFlags"))
    return (0);
  if (!SDDS_SetMemory(SDDS_dataset->row_flag, SDDS_dataset->n_rows_allocated, SDDS_LONG, (int32_t)row_flag_value, (int32_t)0)) {
    SDDS_SetError("Unable to set row flags--memory filling failed (SDDS_SetRowFlags)");
    return (0);
  }
  return (1);
}

/**
 * @brief Retrieves the acceptance flag of a specific row in the current data table.
 *
 * This function fetches the acceptance flag for a given row. The flag indicates whether the row is "of interest" (non-zero) or rejected (zero).
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param row Index of the row whose flag is to be retrieved. Must be within the range [0, n_rows-1].
 *
 * @return 
 *   - **Non-negative integer** representing the flag value of the specified row.
 *   - **-1** if the dataset is invalid or the row index is out of bounds.
 *
 * @sa SDDS_SetRowFlags, SDDS_GetRowFlags
 */
int32_t SDDS_GetRowFlag(SDDS_DATASET *SDDS_dataset, int64_t row) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetRowFlag"))
    return -1;
  if (row < 0 || row >= SDDS_dataset->n_rows)
    return -1;
  return SDDS_dataset->row_flag[row];
}

/**
 * @brief Retrieves the acceptance flags for all rows in the current data table.
 *
 * This function copies the acceptance flags of each row into a provided array. Each flag indicates whether the corresponding row is "of interest" (non-zero) or rejected (zero).
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param flag Pointer to an integer array where the row flags will be stored. The array must have at least `rows` elements.
 * @param rows Number of rows to retrieve flags for. Typically, this should match the total number of rows in the data table.
 *
 * @return 
 *   - **1** on successful retrieval of all row flags.
 *   - **0** on failure, with an error message recorded (e.g., if row count mismatches).
 *
 * @note 
 * Ensure that the `flag` array is adequately allocated to hold the flags for all specified rows.
 *
 * @sa SDDS_SetRowFlags, SDDS_GetRowFlag
 */
int32_t SDDS_GetRowFlags(SDDS_DATASET *SDDS_dataset, int32_t *flag, int64_t rows) {
  int64_t i;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetRowFlags"))
    return 0;
  if (rows != SDDS_dataset->n_rows) {
    SDDS_SetError("Row count mismatch (SDDS_GetRowFlags)");
    return 0;
  }
  for (i = 0; i < rows; i++)
    flag[i] = SDDS_dataset->row_flag[i];
  return 1;
}

/**
 * @brief Sets acceptance flags for rows based on specified criteria.
 *
 * This function allows setting row flags in two modes:
 * - **SDDS_FLAG_ARRAY**: Sets flags based on an array of flag values.
 * - **SDDS_INDEX_LIMITS**: Sets flags for a range of rows to a specific value.
 *
 * A non-zero flag indicates that a row is "of interest", while a zero flag marks it for rejection.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode Operation mode determining how flags are set. Possible values:
 *             - `SDDS_FLAG_ARRAY`: 
 *               ```c
 *               SDDS_AssertRowFlags(SDDS_DATASET *SDDS_dataset, SDDS_FLAG_ARRAY, int32_t *flagArray, int64_t rowsInArray);
 *               ```
 *             - `SDDS_INDEX_LIMITS`: 
 *               ```c
 *               SDDS_AssertRowFlags(SDDS_DATASET *SDDS_dataset, SDDS_INDEX_LIMITS, int64_t start, int64_t end, int32_t value);
 *               ```
 * @param ... Variable arguments based on the selected mode:
 *             - **SDDS_FLAG_ARRAY**:
 *               - `int32_t *flagArray`: Array of flag values to assign.
 *               - `int64_t rowsInArray`: Number of rows in `flagArray`.
 *             - **SDDS_INDEX_LIMITS**:
 *               - `int64_t start`: Starting row index (inclusive).
 *               - `int64_t end`: Ending row index (inclusive).
 *               - `int32_t value`: Flag value to assign to the specified range.
 *
 * @return 
 *   - **1** on successful assignment of row flags.
 *   - **0** on failure, with an error message recorded (e.g., invalid parameters, memory issues).
 *
 * @note 
 * - For `SDDS_FLAG_ARRAY`, if `rowsInArray` exceeds the number of allocated rows, it is truncated to fit.
 * - For `SDDS_INDEX_LIMITS`, if `end` exceeds the number of rows, it is adjusted to the last valid row index.
 *
 * @sa SDDS_SetRowFlags, SDDS_GetRowFlags, SDDS_GetRowFlag
 */
int32_t SDDS_AssertRowFlags(SDDS_DATASET *SDDS_dataset, uint32_t mode, ...)
/* usage:
   SDDS_AssertRowFlags(&SDDSset, SDDS_FLAG_ARRAY, int32_t *flagArray, int64_t rowsInArray)
   rowsInArray is normally equal to the number of rows in the table
   SDDS_AssertRowFlags(&SDDSset, SDDS_INDEX_LIMITS, int64_t start, int64_t end, int32_t value)
*/
{
  int64_t i, rows, startRow, endRow;
  va_list argptr;
  int32_t retval;
  int32_t *flagArray, flagValue;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_AssertRowFlags"))
    return (0);

  va_start(argptr, mode);
  retval = 0;
  switch (mode) {
  case SDDS_FLAG_ARRAY:
    if (!(flagArray = va_arg(argptr, int32_t *)))
      SDDS_SetError("NULL flag array pointer seen (SDDS_AssertRowFlags)");
    else if ((rows = va_arg(argptr, int64_t)) < 0)
      SDDS_SetError("invalid row count seen (SDDS_AssertRowFlags)");
    else {
      if (rows >= SDDS_dataset->n_rows)
        rows = SDDS_dataset->n_rows;
      for (i = 0; i < rows; i++)
        SDDS_dataset->row_flag[i] = flagArray[i];
      retval = 1;
    }
    break;
  case SDDS_INDEX_LIMITS:
    if ((startRow = va_arg(argptr, int64_t)) < 0 || (endRow = va_arg(argptr, int64_t)) < startRow)
      SDDS_SetError("invalid start and end row values (SDDS_AssertRowFlags)");
    else {
      flagValue = va_arg(argptr, int32_t);
      if (endRow >= SDDS_dataset->n_rows || endRow < 0)
        endRow = SDDS_dataset->n_rows - 1;
      for (i = startRow; i <= endRow; i++)
        SDDS_dataset->row_flag[i] = flagValue;
      retval = 1;
    }
    break;
  default:
    SDDS_SetError("unknown mode passed (SDDS_AssertRowFlags)");
    break;
  }

  va_end(argptr);
  return retval;
}

/**
 * @brief Sets the acceptance flags for all columns in the current data table of a data set.
 *
 * This function initializes the acceptance flags for each column. A non-zero flag indicates that the column is "of interest" and should be considered in subsequent operations, while a zero flag marks the column for rejection.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_flag_value Integer value to assign to all column flags.
 *                           - Non-zero value: Marks columns as accepted ("of interest").
 *                           - Zero value: Marks columns as rejected.
 *
 * @return 
 *   - **1** on successful update of column flags.
 *   - **0** on failure, with an error message recorded (e.g., memory allocation failure).
 *
 * @note 
 * This function overwrites any existing column flags with the specified `column_flag_value`. It also updates the `column_order` array accordingly.
 *
 * @sa SDDS_GetColumnFlags, SDDS_AssertColumnFlags
 */
int32_t SDDS_SetColumnFlags(SDDS_DATASET *SDDS_dataset, int32_t column_flag_value) {
  int64_t i;
  /*  int32_t j; */
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetColumnFlags"))
    return 0;
  if ((!SDDS_dataset->column_flag || !SDDS_dataset->column_order) && !SDDS_AllocateColumnFlags(SDDS_dataset))
    return 0;
  if (!SDDS_SetMemory(SDDS_dataset->column_flag, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)column_flag_value, (int32_t)0)) {
    SDDS_SetError("Unable to set column flags--memory filling failed (SDDS_SetColumnFlags)");
    return (0);
  }
  SDDS_dataset->n_of_interest = column_flag_value ? SDDS_dataset->layout.n_columns : 0;
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
    SDDS_dataset->column_order[i] = column_flag_value ? i : -1;
  return (1);
}

/**
 * @brief Sets acceptance flags for columns based on specified criteria.
 *
 * This function allows setting column flags in two modes:
 * - **SDDS_FLAG_ARRAY**: Sets flags based on an array of flag values.
 * - **SDDS_INDEX_LIMITS**: Sets flags for a range of columns to a specific value.
 *
 * A non-zero flag indicates that a column is "of interest", while a zero flag marks it for rejection.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode Operation mode determining how flags are set. Possible values:
 *             - `SDDS_FLAG_ARRAY`: 
 *               ```c
 *               SDDS_AssertColumnFlags(SDDS_DATASET *SDDS_dataset, SDDS_FLAG_ARRAY, int32_t *flagArray, int32_t columnsInArray);
 *               ```
 *             - `SDDS_INDEX_LIMITS`: 
 *               ```c
 *               SDDS_AssertColumnFlags(SDDS_DATASET *SDDS_dataset, SDDS_INDEX_LIMITS, int32_t start, int32_t end, int32_t value);
 *               ```
 * @param ... Variable arguments based on the selected mode:
 *             - **SDDS_FLAG_ARRAY**:
 *               - `int32_t *flagArray`: Array of flag values to assign.
 *               - `int32_t columnsInArray`: Number of columns in `flagArray`.
 *             - **SDDS_INDEX_LIMITS**:
 *               - `int32_t start`: Starting column index (inclusive).
 *               - `int32_t end`: Ending column index (inclusive).
 *               - `int32_t value`: Flag value to assign to the specified range.
 *
 * @return 
 *   - **1** on successful assignment of column flags.
 *   - **0** on failure, with an error message recorded (e.g., invalid parameters, memory issues).
 *
 * @note 
 * - For `SDDS_FLAG_ARRAY`, if `columnsInArray` exceeds the number of allocated columns, it is truncated to fit.
 * - For `SDDS_INDEX_LIMITS`, if `end` exceeds the number of columns, it is adjusted to the last valid column index.
 *
 * @sa SDDS_SetColumnFlags, SDDS_GetColumnFlags, SDDS_GetColumnFlag
 */
int32_t SDDS_AssertColumnFlags(SDDS_DATASET *SDDS_dataset, uint32_t mode, ...) {
  int64_t i, j;
  int32_t columns, startColumn, endColumn;
  va_list argptr;
  int32_t retval;
  int32_t *flagArray, flagValue;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_AssertColumnFlags"))
    return (0);
  if ((!SDDS_dataset->column_flag || !SDDS_dataset->column_order) && !SDDS_AllocateColumnFlags(SDDS_dataset))
    return 0;

  va_start(argptr, mode);
  retval = 0;
  switch (mode) {
  case SDDS_FLAG_ARRAY:
    if (!(flagArray = va_arg(argptr, int32_t *)))
      SDDS_SetError("NULL flag array pointer seen (SDDS_AssertColumnFlags)");
    else if ((columns = va_arg(argptr, int32_t)) < 0)
      SDDS_SetError("invalid column count seen (SDDS_AssertColumnFlags)");
    else {
      if (columns >= SDDS_dataset->layout.n_columns)
        columns = SDDS_dataset->layout.n_columns - 1;
      for (i = 0; i < columns; i++)
        SDDS_dataset->column_flag[i] = flagArray[i];
      retval = 1;
    }
    break;
  case SDDS_INDEX_LIMITS:
    if ((startColumn = va_arg(argptr, int32_t)) < 0 || (endColumn = va_arg(argptr, int32_t)) < startColumn)
      SDDS_SetError("invalid start and end column values (SDDS_AssertColumnFlags)");
    else {
      flagValue = va_arg(argptr, int32_t);
      if (endColumn >= SDDS_dataset->layout.n_columns || endColumn < 0)
        endColumn = SDDS_dataset->layout.n_columns - 1;
      for (i = startColumn; i <= endColumn; i++)
        SDDS_dataset->column_flag[i] = flagValue;
      retval = 1;
    }
    break;
  default:
    SDDS_SetError("unknown mode passed (SDDS_AssertColumnFlags)");
    break;
  }
  va_end(argptr);

  for (i = j = 0; i < SDDS_dataset->layout.n_columns; i++) {
    if (SDDS_dataset->column_flag[i])
      SDDS_dataset->column_order[j++] = i;
  }

  SDDS_dataset->n_of_interest = j;

  return retval;
}

/**
 * @brief Counts the number of columns marked as "of interest" in the current data table.
 *
 * This function returns the total number of columns that have been flagged as "of interest" based on their acceptance flags.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 *
 * @return 
 *   - **Non-negative integer** representing the number of columns marked as "of interest".
 *   - **-1** if the dataset is invalid.
 *
 * @sa SDDS_CountRowsOfInterest, SDDS_SetColumnFlags, SDDS_GetColumnFlags
 */
int32_t SDDS_CountColumnsOfInterest(SDDS_DATASET *SDDS_dataset) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_CountRowsOfInterest"))
    return (-1);
  return (SDDS_dataset->n_of_interest);
}

/**
 * @brief Counts the number of rows marked as "of interest" in the current data table.
 *
 * This function iterates through the row acceptance flags and tallies the number of rows that are flagged as "of interest" (non-zero).
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 *
 * @return 
 *   - **Number of rows** with non-zero acceptance flags.
 *   - **-1** on error (e.g., invalid dataset or tabular data).
 *
 * @note 
 * Ensure that the dataset contains tabular data before invoking this function.
 *
 * @sa SDDS_SetRowFlags, SDDS_GetRowFlags, SDDS_GetRowFlag
 */
int64_t SDDS_CountRowsOfInterest(SDDS_DATASET *SDDS_dataset) {
  int64_t n_rows, i;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_CountRowsOfInterest"))
    return (-1);
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_CountRowsOfInterest"))
    return (-1);
  if (!SDDS_dataset->layout.n_columns)
    return 0;
  for (i = n_rows = 0; i < SDDS_dataset->n_rows; i++) {
    if (SDDS_dataset->row_flag[i])
      n_rows += 1;
  }
  return (n_rows);
}

/**
 * @brief Sets the acceptance flags for columns based on specified naming criteria.
 *
 * This function allows modifying column acceptance flags using various methods, including specifying column names directly or using pattern matching.
 *
 * Supported modes:
 * - **SDDS_NAME_ARRAY**: Provide an array of column names to mark as "of interest".
 * - **SDDS_NAMES_STRING**: Provide a single string containing comma-separated column names.
 * - **SDDS_NAME_STRINGS**: Provide multiple individual column name strings, terminated by `NULL`.
 * - **SDDS_MATCH_STRING**: Provide a pattern string and a logic mode to match column names.
 *
 * A non-zero flag indicates that a column is "of interest", while a zero flag marks it for rejection.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param mode Operation mode determining how columns are selected. Possible values:
 *             - `SDDS_NAME_ARRAY`: 
 *               ```c
 *               SDDS_SetColumnsOfInterest(SDDS_DATASET *SDDS_dataset, SDDS_NAME_ARRAY, int32_t n_entries, char **nameArray);
 *               ```
 *             - `SDDS_NAMES_STRING`: 
 *               ```c
 *               SDDS_SetColumnsOfInterest(SDDS_DATASET *SDDS_dataset, SDDS_NAMES_STRING, char *names);
 *               ```
 *             - `SDDS_NAME_STRINGS`: 
 *               ```c
 *               SDDS_SetColumnsOfInterest(SDDS_DATASET *SDDS_dataset, SDDS_NAME_STRINGS, char *name1, char *name2, ..., NULL);
 *               ```
 *             - `SDDS_MATCH_STRING`: 
 *               ```c
 *               SDDS_SetColumnsOfInterest(SDDS_DATASET *SDDS_dataset, SDDS_MATCH_STRING, char *pattern, int32_t logic_mode);
 *               ```
 * @param ... Variable arguments based on the selected mode:
 *             - **SDDS_NAME_ARRAY**:
 *               - `int32_t n_entries`: Number of column names in the array.
 *               - `char **nameArray`: Array of column name strings.
 *             - **SDDS_NAMES_STRING**:
 *               - `char *names`: Comma-separated string of column names.
 *             - **SDDS_NAME_STRINGS**:
 *               - `char *name1, char *name2, ..., NULL`: Individual column name strings terminated by `NULL`.
 *             - **SDDS_MATCH_STRING**:
 *               - `char *pattern`: Pattern string to match column names (supports wildcards).
 *               - `int32_t logic_mode`: Logic mode for matching (e.g., AND, OR).
 *
 * @return 
 *   - **1** on successful update of column flags.
 *   - **0** on failure, with an error message recorded (e.g., invalid mode, memory issues, unrecognized column names).
 *
 * @note 
 * - When using `SDDS_MATCH_STRING`, the `pattern` may include wildcards to match multiple column names.
 * - Ensure that column names provided exist within the dataset to avoid errors.
 *
 * @sa SDDS_SetColumnFlags, SDDS_AssertColumnFlags, SDDS_GetColumnFlags
 */
int32_t SDDS_SetColumnsOfInterest(SDDS_DATASET *SDDS_dataset, int32_t mode, ...)
/* This routine has 3 calling modes:
 * SDDS_SetColumnsOfInterest(&SDDS_dataset, SDDS_NAME_ARRAY, int32_t n_entries, char **name)
 * SDDS_SetColumnsOfInterest(&SDDS_dataset, SDDS_NAMES_STRING, char *names)
 * SDDS_SetColumnsOfInterest(&SDDS_dataset, SDDS_NAME_STRINGS, char *name1, char *name2, ..., NULL )
 * SDDS_SetColumnsOfInterest(&SDDS_dataset, SDDS_MATCH_STRING, char *name, int32_t logic_mode)
 */
{
  va_list argptr;
  int32_t i, j, index, n_names;
  int32_t retval;
  /*  int32_t type; */
  char **name, *string, *match_string, *ptr;
  int32_t local_memory; /* (0,1,2) --> (none, pointer array, pointer array + strings) locally allocated */
  char buffer[SDDS_MAXLINE];
  int32_t logic;

  name = NULL;
  n_names = local_memory = logic = 0;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetColumnsOfInterest"))
    return (0);
  if ((!SDDS_dataset->column_flag || !SDDS_dataset->column_order) && !SDDS_AllocateColumnFlags(SDDS_dataset))
    return 0;
  va_start(argptr, mode);
  retval = -1;
  match_string = NULL;
  switch (mode) {
  case SDDS_NAME_ARRAY:
    local_memory = 0;
    n_names = va_arg(argptr, int32_t);
    name = va_arg(argptr, char **);
    break;
  case SDDS_NAMES_STRING:
    local_memory = 2;
    n_names = 0;
    name = NULL;
    ptr = va_arg(argptr, char *);
    SDDS_CopyString(&string, ptr);
    while ((ptr = strchr(string, ',')))
      *ptr = ' ';
    while (SDDS_GetToken(string, buffer, SDDS_MAXLINE) > 0) {
      if (!(name = SDDS_Realloc(name, sizeof(*name) * (n_names + 1))) || !SDDS_CopyString(name + n_names, buffer)) {
        SDDS_SetError("Unable to process column selection--memory allocation failure (SDDS_SetColumnsOfInterest)");
        retval = 0;
        break;
      }
      n_names++;
    }
    free(string);
    break;
  case SDDS_NAME_STRINGS:
    local_memory = 1;
    n_names = 0;
    name = NULL;
    while ((string = va_arg(argptr, char *))) {
      if (!(name = SDDS_Realloc(name, sizeof(*name) * (n_names + 1)))) {
        SDDS_SetError("Unable to process column selection--memory allocation failure (SDDS_SetColumnsOfInterest)");
        retval = 0;
        break;
      }
      name[n_names++] = string;
    }
    break;
  case SDDS_MATCH_STRING:
    local_memory = 0;
    n_names = 1;
    if (!(string = va_arg(argptr, char *))) {
      SDDS_SetError("Unable to process column selection--invalid matching string (SDDS_SetColumnsOfInterest)");
      retval = 0;
      break;
    }
    match_string = expand_ranges(string);
    logic = va_arg(argptr, int32_t);
    break;
  default:
    SDDS_SetError("Unable to process column selection--unknown mode (SDDS_SetColumnsOfInterest)");
    retval = 0;
    break;
  }

  va_end(argptr);
  if (retval != -1)
    return (retval);

  if (n_names == 0) {
    SDDS_SetError("Unable to process column selection--no names in call (SDDS_SetColumnsOfInterest)");
    return (0);
  }
  if (!SDDS_dataset->column_order) {
    SDDS_SetError("Unable to process column selection--'column_order' array in SDDS_DATASET is NULL (SDDS_SetColumnsOfInterest)");
    return (0);
  }

  if (mode != SDDS_MATCH_STRING) {
    for (i = 0; i < n_names; i++) {
      if ((index = SDDS_GetColumnIndex(SDDS_dataset, name[i])) < 0) {
        sprintf(buffer, "Unable to process column selection--unrecognized column name %s seen (SDDS_SetColumnsOfInterest)", name[i]);
        SDDS_SetError(buffer);
        return (0);
      }
      for (j = 0; j < SDDS_dataset->n_of_interest; j++)
        if (index == SDDS_dataset->column_order[j])
          break;
      if (j == SDDS_dataset->n_of_interest) {
        SDDS_dataset->column_flag[index] = 1;
        SDDS_dataset->column_order[j] = index;
        SDDS_dataset->n_of_interest++;
      }
    }
  } else {
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
      if (SDDS_Logic(SDDS_dataset->column_flag[i], wild_match(SDDS_dataset->layout.column_definition[i].name, match_string), logic)) {
#if defined(DEBUG)
        fprintf(stderr, "logic match of %s to %s\n", SDDS_dataset->layout.column_definition[i].name, match_string);
#endif
        for (j = 0; j < SDDS_dataset->n_of_interest; j++)
          if (i == SDDS_dataset->column_order[j])
            break;
        if (j == SDDS_dataset->n_of_interest) {
          SDDS_dataset->column_flag[i] = 1;
          SDDS_dataset->column_order[j] = i;
          SDDS_dataset->n_of_interest++;
        }
      } else {
#if defined(DEBUG)
        fprintf(stderr, "no logic match of %s to %s\n", SDDS_dataset->layout.column_definition[i].name, match_string);
#endif
        SDDS_dataset->column_flag[i] = 0;
        for (j = 0; j < SDDS_dataset->n_of_interest; j++)
          if (i == SDDS_dataset->column_order[j])
            break;
        if (j != SDDS_dataset->n_of_interest) {
          for (j++; j < SDDS_dataset->n_of_interest; j++)
            SDDS_dataset->column_order[j - 1] = SDDS_dataset->column_order[j];
        }
      }
    }
    free(match_string);
  }

#if defined(DEBUG)
  for (i = 0; i < SDDS_dataset->n_of_interest; i++)
    fprintf(stderr, "column %" PRId32 " will be %s\n", i, SDDS_dataset->layout.column_definition[SDDS_dataset->column_order[i]].name);
#endif

  if (local_memory == 2) {
    for (i = 0; i < n_names; i++)
      free(name[i]);
  }
  if (local_memory >= 1)
    free(name);

  return (1);
}

/**
 * @brief Retrieves a copy of the data for a specified column, including only rows marked as "of interest".
 *
 * This function returns a newly allocated array containing data from the specified column for all rows that are flagged as "of interest". The data type of the returned array matches the column's data type.
 *
 * For columns of type `SDDS_STRING`, the returned array is of type `char**`, with each element being a dynamically allocated string.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name NULL-terminated string specifying the name of the column to retrieve.
 *
 * @return 
 *   - **Pointer to the data array** on success. The type of the array corresponds to the column's data type.
 *   - **NULL** on failure, with an error message recorded (e.g., unrecognized column name, memory allocation failure, no rows of interest).
 *
 * @warning 
 * The caller is responsible for freeing the allocated memory to avoid memory leaks. For `SDDS_STRING` types, each string within the array should be freed individually, followed by the array itself.
 *
 * @note 
 * - The number of rows in the returned array can be obtained using `SDDS_CountRowsOfInterest`.
 * - If the column's memory mode is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data may be freed after access.
 *
 * @sa SDDS_GetInternalColumn, SDDS_CountRowsOfInterest, SDDS_SetRowFlags
 */
void *SDDS_GetColumn(SDDS_DATASET *SDDS_dataset, char *column_name) {
  int32_t size, type, index;
  int64_t i, j, n_rows;
  void *data;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumn"))
    return (NULL);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_GetColumn)");
    return (NULL);
  }
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get column--no rows left (SDDS_GetColumn)");
    return (NULL);
  }
  if (!(type = SDDS_GetColumnType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get column--data type undefined (SDDS_GetColumn)");
    return (NULL);
  }
  size = SDDS_type_size[type - 1];
  if (!(data = SDDS_Malloc(size * n_rows))) {
    SDDS_SetError("Unable to get column--memory allocation failure (SDDS_GetColumn)");
    return (NULL);
  }
  for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
    if (SDDS_dataset->row_flag[i]) {
      if (type != SDDS_STRING)
        memcpy((char *)data + size * j++, (char *)SDDS_dataset->data[index] + size * i, size);
      else if (!SDDS_CopyString((char **)data + j++, ((char ***)SDDS_dataset->data)[index][i]))
        return (NULL);
    }
  }
  if (j != n_rows) {
    SDDS_SetError("Unable to get column--row number mismatch (SDDS_GetColumn)");
    return (NULL);
  }
  if (SDDS_GetColumnMemoryMode(SDDS_dataset) == DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS) {
    SDDS_dataset->column_track_memory[index] = 0;
    //Free internal copy now under the assumption that the program will not ask for it again.
    if (type == SDDS_STRING) {
      if (0) {
        //FIX this. It currently causes a memory error in SDDS_ScanData2 with multipage files
        char **ptr = (char **)SDDS_dataset->data[index];
        for (i = 0; i < SDDS_dataset->n_rows_allocated; i++, ptr++)
          if (*ptr)
            free(*ptr);
        free(SDDS_dataset->data[index]);
        SDDS_dataset->data[index] = NULL;
      }
    } else {
      free(SDDS_dataset->data[index]);
      SDDS_dataset->data[index] = NULL;
    }
  }
  return (data);
}

/**
 * @brief Retrieves an internal pointer to the data of a specified column, including all rows.
 *
 * This function returns a direct pointer to the internal data array of the specified column. Unlike `SDDS_GetColumn`, it includes all rows, regardless of their acceptance flags.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name NULL-terminated string specifying the name of the column to retrieve.
 *
 * @return 
 *   - **Pointer to the internal data array** on success. The type of the pointer corresponds to the column's data type.
 *   - **NULL** on failure, with an error message recorded (e.g., unrecognized column name).
 *
 * @warning 
 * Modifying the data through the returned pointer affects the internal state of the dataset. Use with caution to avoid unintended side effects.
 *
 * @note 
 * - If the column's memory mode is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data may be freed after access.
 * - This function does not allocate new memory; it provides direct access to the dataset's internal structures.
 *
 * @sa SDDS_GetColumn, SDDS_SetColumnFlags, SDDS_CountColumnsOfInterest
 */
void *SDDS_GetInternalColumn(SDDS_DATASET *SDDS_dataset, char *column_name) {
  int32_t index;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetInternalColumn"))
    return (NULL);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_GetInternalColumn)");
    return (NULL);
  }
  if (SDDS_GetColumnMemoryMode(SDDS_dataset) == DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS) {
    SDDS_dataset->column_track_memory[index] = 0;
  }
  return SDDS_dataset->data[index];
}

/**
 * @brief Retrieves the data of a specified numerical column as an array of long doubles, considering only rows marked as "of interest".
 *
 * This function extracts data from a specified column within the current data table of a dataset. It processes only those rows that are flagged as "of interest" (i.e., have a non-zero acceptance flag). The extracted data is returned as a newly allocated array of `long double` values.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name 
 *   NULL-terminated string specifying the name of the column from which data is to be retrieved.
 *
 * @return 
 *   - **Pointer to an array of `long double`** containing the data from the specified column for all rows marked as "of interest".
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized column name, non-numeric column type, memory allocation failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - The caller is responsible for freeing the allocated memory to prevent memory leaks.
 *   - This function assumes that the specified column contains numerical data. Attempting to retrieve data from a non-numeric column (excluding `SDDS_CHARACTER`) will result in an error.
 *
 * @note 
 *   - The number of elements in the returned array corresponds to the number of rows marked as "of interest", which can be obtained using `SDDS_CountRowsOfInterest`.
 *   - If the dataset's memory mode for the column is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the column may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetColumnInDoubles`
 *   - `SDDS_GetColumnInFloats`
 *   - `SDDS_GetColumn`
 *   - `SDDS_CountRowsOfInterest`
 */
long double *SDDS_GetColumnInLongDoubles(SDDS_DATASET *SDDS_dataset, char *column_name) {
  int32_t size, type, index;
  int64_t i, j, n_rows;
  long double *data;
  void *rawData;

  j = 0;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnInLongDoubles"))
    return (NULL);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_GetColumnInLongDoubles)");
    return (NULL);
  }
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get column--no rows left (SDDS_GetColumnInLongDoubles)");
    return (NULL);
  }
  if ((type = SDDS_GetColumnType(SDDS_dataset, index)) <= 0 || (size = SDDS_GetTypeSize(type)) <= 0 || (!SDDS_NUMERIC_TYPE(type) && type != SDDS_CHARACTER)) {
    SDDS_SetError("Unable to get column--data size or type undefined or non-numeric (SDDS_GetColumnInLongDoubles)");
    return (NULL);
  }
  if (!(data = (long double *)SDDS_Malloc(sizeof(long double) * n_rows))) {
    SDDS_SetError("Unable to get column--memory allocation failure (SDDS_GetColumnInLongDoubles)");
    return (NULL);
  }
  rawData = SDDS_dataset->data[index];
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((long double *)rawData)[i];
    }
    break;
  case SDDS_DOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((double *)rawData)[i];
    }
    break;
  case SDDS_FLOAT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((float *)rawData)[i];
    }
    break;
  case SDDS_LONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int32_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint32_t *)rawData)[i];
    }
    break;
  case SDDS_LONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int64_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint64_t *)rawData)[i];
    }
    break;
  case SDDS_SHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((short *)rawData)[i];
    }
    break;
  case SDDS_USHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((unsigned short *)rawData)[i];
    }
    break;
  case SDDS_CHARACTER:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((char *)rawData)[i];
    }
    break;
  }
  if (j != n_rows) {
    SDDS_SetError("Unable to get column--row number mismatch (SDDS_GetColumnInLongDoubles)");
    return (NULL);
  }
  if (SDDS_GetColumnMemoryMode(SDDS_dataset) == DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS) {
    SDDS_dataset->column_track_memory[index] = 0;
    //Free internal copy now under the assumption that the program will not ask for it again.
    if (type == SDDS_STRING) {
      if (0) {
        //FIX this. It currently causes a memory error in SDDS_ScanData2 with multipage files
        char **ptr = (char **)SDDS_dataset->data[index];
        for (i = 0; i < SDDS_dataset->n_rows_allocated; i++, ptr++)
          if (*ptr)
            free(*ptr);
        free(SDDS_dataset->data[index]);
        SDDS_dataset->data[index] = NULL;
      }
    } else {
      free(SDDS_dataset->data[index]);
      SDDS_dataset->data[index] = NULL;
    }
  }
  return (data);
}

/**
 * @brief Retrieves the data of a specified numerical column as an array of doubles, considering only rows marked as "of interest".
 *
 * This function extracts data from a specified column within the current data table of a dataset. It processes only those rows that are flagged as "of interest" (i.e., have a non-zero acceptance flag). The extracted data is returned as a newly allocated array of `double` values.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name 
 *   NULL-terminated string specifying the name of the column from which data is to be retrieved.
 *
 * @return 
 *   - **Pointer to an array of `double`** containing the data from the specified column for all rows marked as "of interest".
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized column name, non-numeric column type, memory allocation failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - The caller is responsible for freeing the allocated memory to prevent memory leaks.
 *   - This function assumes that the specified column contains numerical data. Attempting to retrieve data from a non-numeric column (excluding `SDDS_CHARACTER`) will result in an error.
 *
 * @note 
 *   - The number of elements in the returned array corresponds to the number of rows marked as "of interest", which can be obtained using `SDDS_CountRowsOfInterest`.
 *   - If the dataset's memory mode for the column is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the column may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetColumnInLongDoubles`
 *   - `SDDS_GetColumnInFloats`
 *   - `SDDS_GetColumn`
 *   - `SDDS_CountRowsOfInterest`
 */
double *SDDS_GetColumnInDoubles(SDDS_DATASET *SDDS_dataset, char *column_name) {
  int32_t size, type, index;
  int64_t i, j, n_rows;
  double *data;
  void *rawData;

  j = 0;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnInDoubles"))
    return (NULL);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_GetColumnInDoubles)");
    return (NULL);
  }
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get column--no rows left (SDDS_GetColumnInDoubles)");
    return (NULL);
  }
  if ((type = SDDS_GetColumnType(SDDS_dataset, index)) <= 0 || (size = SDDS_GetTypeSize(type)) <= 0 || (!SDDS_NUMERIC_TYPE(type) && type != SDDS_CHARACTER)) {
    SDDS_SetError("Unable to get column--data size or type undefined or non-numeric (SDDS_GetColumnInDoubles)");
    return (NULL);
  }
  if (!(data = (double *)SDDS_Malloc(sizeof(double) * n_rows))) {
    SDDS_SetError("Unable to get column--memory allocation failure (SDDS_GetColumnInDoubles)");
    return (NULL);
  }
  rawData = SDDS_dataset->data[index];
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((long double *)rawData)[i];
    }
    break;
  case SDDS_DOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((double *)rawData)[i];
    }
    break;
  case SDDS_FLOAT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((float *)rawData)[i];
    }
    break;
  case SDDS_LONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int32_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint32_t *)rawData)[i];
    }
    break;
  case SDDS_LONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int64_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint64_t *)rawData)[i];
    }
    break;
  case SDDS_SHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((short *)rawData)[i];
    }
    break;
  case SDDS_USHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((unsigned short *)rawData)[i];
    }
    break;
  case SDDS_CHARACTER:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((char *)rawData)[i];
    }
    break;
  }
  if (j != n_rows) {
    SDDS_SetError("Unable to get column--row number mismatch (SDDS_GetColumnInDoubles)");
    return (NULL);
  }
  if (SDDS_GetColumnMemoryMode(SDDS_dataset) == DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS) {
    SDDS_dataset->column_track_memory[index] = 0;
    //Free internal copy now under the assumption that the program will not ask for it again.
    if (type == SDDS_STRING) {
      if (0) {
        //FIX this. It currently causes a memory error in SDDS_ScanData2 with multipage files
        char **ptr = (char **)SDDS_dataset->data[index];
        for (i = 0; i < SDDS_dataset->n_rows_allocated; i++, ptr++)
          if (*ptr)
            free(*ptr);
        free(SDDS_dataset->data[index]);
        SDDS_dataset->data[index] = NULL;
      }
    } else {
      free(SDDS_dataset->data[index]);
      SDDS_dataset->data[index] = NULL;
    }
  }
  return (data);
}

/**
 * @brief Retrieves the data of a specified numerical column as an array of floats, considering only rows marked as "of interest".
 *
 * This function extracts data from a specified column within the current data table of a dataset. It processes only those rows that are flagged as "of interest" (i.e., have a non-zero acceptance flag). The extracted data is returned as a newly allocated array of `float` values.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name 
 *   NULL-terminated string specifying the name of the column from which data is to be retrieved.
 *
 * @return 
 *   - **Pointer to an array of `float`** containing the data from the specified column for all rows marked as "of interest".
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized column name, non-numeric column type, memory allocation failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - The caller is responsible for freeing the allocated memory to prevent memory leaks.
 *   - This function assumes that the specified column contains numerical data. Attempting to retrieve data from a non-numeric column (excluding `SDDS_CHARACTER`) will result in an error.
 *
 * @note 
 *   - The number of elements in the returned array corresponds to the number of rows marked as "of interest", which can be obtained using `SDDS_CountRowsOfInterest`.
 *   - If the dataset's memory mode for the column is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the column may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetColumnInLongDoubles`
 *   - `SDDS_GetColumnInDoubles`
 *   - `SDDS_GetColumn`
 *   - `SDDS_CountRowsOfInterest`
 */
float *SDDS_GetColumnInFloats(SDDS_DATASET *SDDS_dataset, char *column_name) {
  int32_t size, type, index;
  int64_t i, j, n_rows;
  float *data;
  void *rawData;

  j = 0;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnInFloats"))
    return (NULL);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_GetColumnInFloats)");
    return (NULL);
  }
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get column--no rows left (SDDS_GetColumnInFloats)");
    return (NULL);
  }
  if ((type = SDDS_GetColumnType(SDDS_dataset, index)) <= 0 || (size = SDDS_GetTypeSize(type)) <= 0 || (!SDDS_NUMERIC_TYPE(type) && type != SDDS_CHARACTER)) {
    SDDS_SetError("Unable to get column--data size or type undefined or non-numeric (SDDS_GetColumnInFloats)");
    return (NULL);
  }
  if (!(data = (float *)SDDS_Malloc(sizeof(float) * n_rows))) {
    SDDS_SetError("Unable to get column--memory allocation failure (SDDS_GetColumnInFloats)");
    return (NULL);
  }
  rawData = SDDS_dataset->data[index];
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((long double *)rawData)[i];
    }
    break;
  case SDDS_DOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((double *)rawData)[i];
    }
    break;
  case SDDS_FLOAT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((float *)rawData)[i];
    }
    break;
  case SDDS_LONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int32_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint32_t *)rawData)[i];
    }
    break;
  case SDDS_LONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int64_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint64_t *)rawData)[i];
    }
    break;
  case SDDS_SHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((short *)rawData)[i];
    }
    break;
  case SDDS_USHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((unsigned short *)rawData)[i];
    }
    break;
  case SDDS_CHARACTER:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((char *)rawData)[i];
    }
    break;
  }
  if (j != n_rows) {
    SDDS_SetError("Unable to get column--row number mismatch (SDDS_GetColumnInFloats)");
    return (NULL);
  }
  if (SDDS_GetColumnMemoryMode(SDDS_dataset) == DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS) {
    SDDS_dataset->column_track_memory[index] = 0;
    //Free internal copy now under the assumption that the program will not ask for it again.
    if (type == SDDS_STRING) {
      if (0) {
        //FIX this. It currently causes a memory error in SDDS_ScanData2 with multipage files
        char **ptr = (char **)SDDS_dataset->data[index];
        for (i = 0; i < SDDS_dataset->n_rows_allocated; i++, ptr++)
          if (*ptr)
            free(*ptr);
        free(SDDS_dataset->data[index]);
        SDDS_dataset->data[index] = NULL;
      }
    } else {
      free(SDDS_dataset->data[index]);
      SDDS_dataset->data[index] = NULL;
    }
  }
  return (data);
}

/**
 * @brief Retrieves the data of a specified numerical column as an array of 32-bit integers, considering only rows marked as "of interest".
 *
 * This function extracts data from a specified column within the current data table of a dataset. It processes only those rows that are flagged as "of interest" (i.e., have a non-zero acceptance flag). The extracted data is returned as a newly allocated array of `int32_t` values.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name 
 *   NULL-terminated string specifying the name of the column from which data is to be retrieved.
 *
 * @return 
 *   - **Pointer to an array of `int32_t`** containing the data from the specified column for all rows marked as "of interest".
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized column name, non-numeric column type, memory allocation failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - The caller is responsible for freeing the allocated memory to prevent memory leaks.
 *   - This function assumes that the specified column contains numerical data. Attempting to retrieve data from a non-numeric column (excluding `SDDS_CHARACTER`) will result in an error.
 *
 * @note 
 *   - The number of elements in the returned array corresponds to the number of rows marked as "of interest", which can be obtained using `SDDS_CountRowsOfInterest`.
 *   - If the dataset's memory mode for the column is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the column may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetColumnInLongDoubles`
 *   - `SDDS_GetColumnInDoubles`
 *   - `SDDS_GetColumnInFloats`
 *   - `SDDS_GetColumn`
 *   - `SDDS_CountRowsOfInterest`
 */
int32_t *SDDS_GetColumnInLong(SDDS_DATASET *SDDS_dataset, char *column_name) {
  int32_t size, type, index;
  int64_t i, j, n_rows;
  int32_t *data;
  void *rawData;
  j = 0;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnInLong"))
    return (NULL);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_GetColumnInLong)");
    return (NULL);
  }
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get column--no rows left (SDDS_GetColumnInLong)");
    return (NULL);
  }
  if ((type = SDDS_GetColumnType(SDDS_dataset, index)) <= 0 || (size = SDDS_GetTypeSize(type)) <= 0 || (!SDDS_NUMERIC_TYPE(type) && type != SDDS_CHARACTER)) {
    SDDS_SetError("Unable to get column--data size or type undefined or non-numeric (SDDS_GetColumnInLong)");
    return (NULL);
  }
  if (!(data = (int32_t *)SDDS_Malloc(sizeof(int32_t) * n_rows))) {
    SDDS_SetError("Unable to get column--memory allocation failure (SDDS_GetColumnInLong)");
    return (NULL);
  }
  rawData = SDDS_dataset->data[index];
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((long double *)rawData)[i];
    }
    break;
  case SDDS_DOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((double *)rawData)[i];
    }
    break;
  case SDDS_FLOAT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((float *)rawData)[i];
    }
    break;
  case SDDS_LONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int32_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint32_t *)rawData)[i];
    }
    break;
  case SDDS_LONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int64_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint64_t *)rawData)[i];
    }
    break;
  case SDDS_SHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((short *)rawData)[i];
    }
    break;
  case SDDS_USHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((unsigned short *)rawData)[i];
    }
    break;
  case SDDS_CHARACTER:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((char *)rawData)[i];
    }
    break;
  }
  if (j != n_rows) {
    SDDS_SetError("Unable to get column--row number mismatch (SDDS_GetColumnInLong)");
    return (NULL);
  }
  if (SDDS_GetColumnMemoryMode(SDDS_dataset) == DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS) {
    SDDS_dataset->column_track_memory[index] = 0;
    //Free internal copy now under the assumption that the program will not ask for it again.
    if (type == SDDS_STRING) {
      if (0) {
        //FIX this. It currently causes a memory error in SDDS_ScanData2 with multipage files
        char **ptr = (char **)SDDS_dataset->data[index];
        for (i = 0; i < SDDS_dataset->n_rows_allocated; i++, ptr++)
          if (*ptr)
            free(*ptr);
        free(SDDS_dataset->data[index]);
        SDDS_dataset->data[index] = NULL;
      }
    } else {
      free(SDDS_dataset->data[index]);
      SDDS_dataset->data[index] = NULL;
    }
  }
  return (data);
}

/**
 * @brief Retrieves the data of a specified numerical column as an array of short integers, considering only rows marked as "of interest".
 *
 * This function extracts data from a specified column within the current data table of a dataset. It processes only those rows that are flagged as "of interest" (i.e., have a non-zero acceptance flag). The extracted data is returned as a newly allocated array of `short` values.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name 
 *   NULL-terminated string specifying the name of the column from which data is to be retrieved.
 *
 * @return 
 *   - **Pointer to an array of `short`** containing the data from the specified column for all rows marked as "of interest".
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized column name, non-numeric column type, memory allocation failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - The caller is responsible for freeing the allocated memory to prevent memory leaks.
 *   - This function assumes that the specified column contains numerical data. Attempting to retrieve data from a non-numeric column (excluding `SDDS_CHARACTER`) will result in an error.
 *
 * @note 
 *   - The number of elements in the returned array corresponds to the number of rows marked as "of interest", which can be obtained using `SDDS_CountRowsOfInterest`.
 *   - If the dataset's memory mode for the column is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the column may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetColumnInLongDoubles`
 *   - `SDDS_GetColumnInDoubles`
 *   - `SDDS_GetColumnInFloats`
 *   - `SDDS_GetColumn`
 *   - `SDDS_CountRowsOfInterest`
 */
short *SDDS_GetColumnInShort(SDDS_DATASET *SDDS_dataset, char *column_name) {
  int32_t size, type, index;
  int64_t i, j, n_rows;
  short *data;
  void *rawData;
  j = 0;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnInShort"))
    return (NULL);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_GetColumnInShort)");
    return (NULL);
  }
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get column--no rows left (SDDS_GetColumnInShort)");
    return (NULL);
  }
  if ((type = SDDS_GetColumnType(SDDS_dataset, index)) <= 0 || (size = SDDS_GetTypeSize(type)) <= 0 || (!SDDS_NUMERIC_TYPE(type) && type != SDDS_CHARACTER)) {
    SDDS_SetError("Unable to get column--data size or type undefined or non-numeric (SDDS_GetColumnInShort)");
    return (NULL);
  }
  if (!(data = (short *)SDDS_Malloc(sizeof(short) * n_rows))) {
    SDDS_SetError("Unable to get column--memory allocation failure (SDDS_GetColumnInShort)");
    return (NULL);
  }
  rawData = SDDS_dataset->data[index];
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((long double *)rawData)[i];
    }
    break;
  case SDDS_DOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((double *)rawData)[i];
    }
    break;
  case SDDS_FLOAT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((float *)rawData)[i];
    }
    break;
  case SDDS_LONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int32_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint32_t *)rawData)[i];
    }
    break;
  case SDDS_LONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((int64_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((uint64_t *)rawData)[i];
    }
    break;
  case SDDS_SHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((short *)rawData)[i];
    }
    break;
  case SDDS_USHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((unsigned short *)rawData)[i];
    }
    break;
  case SDDS_CHARACTER:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i])
        data[j++] = ((char *)rawData)[i];
    }
    break;
  }
  if (j != n_rows) {
    SDDS_SetError("Unable to get column--row number mismatch (SDDS_GetColumnInShort)");
    return (NULL);
  }
  if (SDDS_GetColumnMemoryMode(SDDS_dataset) == DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS) {
    SDDS_dataset->column_track_memory[index] = 0;
    //Free internal copy now under the assumption that the program will not ask for it again.
    if (type == SDDS_STRING) {
      if (0) {
        //FIX this. It currently causes a memory error in SDDS_ScanData2 with multipage files
        char **ptr = (char **)SDDS_dataset->data[index];
        for (i = 0; i < SDDS_dataset->n_rows_allocated; i++, ptr++)
          if (*ptr)
            free(*ptr);
        free(SDDS_dataset->data[index]);
        SDDS_dataset->data[index] = NULL;
      }
    } else {
      free(SDDS_dataset->data[index]);
      SDDS_dataset->data[index] = NULL;
    }
  }
  return (data);
}

/**
 * @brief Retrieves the data of a specified column as an array of strings, considering only rows marked as "of interest".
 *
 * This function extracts data from a specified column within the current data table of a dataset. It processes only those rows that are flagged as "of interest" (i.e., have a non-zero acceptance flag). The extracted data is returned as a newly allocated array of `char*` (strings).
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name 
 *   NULL-terminated string specifying the name of the column from which data is to be retrieved.
 *
 * @return 
 *   - **Pointer to an array of `char*`** containing the data from the specified column for all rows marked as "of interest".
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized column name, non-string column type, memory allocation failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - The caller is responsible for freeing the allocated memory to prevent memory leaks. Each string within the array should be freed individually, followed by the array itself.
 *   - This function assumes that the specified column contains string data (`SDDS_STRING` or `SDDS_CHARACTER`). Attempting to retrieve data from a non-string column will result in an error.
 *
 * @note 
 *   - The number of elements in the returned array corresponds to the number of rows marked as "of interest", which can be obtained using `SDDS_CountRowsOfInterest`.
 *   - If the dataset's memory mode for the column is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the column may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetColumnInLongDoubles`
 *   - `SDDS_GetColumnInDoubles`
 *   - `SDDS_GetColumnInFloats`
 *   - `SDDS_GetColumn`
 *   - `SDDS_CountRowsOfInterest`
 */
char **SDDS_GetColumnInString(SDDS_DATASET *SDDS_dataset, char *column_name) {
  int32_t size, type, index;
  int64_t i, j, n_rows;
  char **data;
  char buffer[SDDS_MAXLINE];

  void *rawData;
  j = 0;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnInString"))
    return (NULL);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_GetColumnInString)");
    return (NULL);
  }
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get column--no rows left (SDDS_GetColumnInString)");
    return (NULL);
  }

  if ((type = SDDS_GetColumnType(SDDS_dataset, index)) <= 0 || (size = SDDS_GetTypeSize(type)) <= 0 ||
      (!SDDS_NUMERIC_TYPE(type) && type != SDDS_CHARACTER && type != SDDS_STRING)) {
    SDDS_SetError("Unable to get column--data size or type undefined or non-numeric (SDDS_GetColumnInString)");
    return (NULL);
  }
  if (!(data = (char **)SDDS_Malloc(sizeof(*data) * n_rows))) {
    SDDS_SetError("Unable to get column--memory allocation failure (SDDS_GetColumnInString)");
    return (NULL);
  }
  rawData = SDDS_dataset->data[index];
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        if (LDBL_DIG == 18) {
          sprintf(buffer, "%22.18Le", ((long double *)rawData)[i]);
        } else {
          sprintf(buffer, "%22.15Le", ((long double *)rawData)[i]);
        }
        SDDS_CopyString(&data[j++], buffer);
      }
    }
    break;
  case SDDS_DOUBLE:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        sprintf(buffer, "%22.15le", ((double *)rawData)[i]);
        SDDS_CopyString(&data[j++], buffer);
      }
    }
    break;
  case SDDS_FLOAT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        sprintf(buffer, "%15.8e", ((float *)rawData)[i]);
        SDDS_CopyString(&data[j++], buffer);
      }
    }
    break;
  case SDDS_LONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        sprintf(buffer, "%" PRId64, ((int64_t *)rawData)[i]);
        SDDS_CopyString(&data[j++], buffer);
      }
    }
    break;
  case SDDS_ULONG64:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        sprintf(buffer, "%" PRIu64, ((uint64_t *)rawData)[i]);
        SDDS_CopyString(&data[j++], buffer);
      }
    }
    break;
  case SDDS_LONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        sprintf(buffer, "%" PRId32, ((int32_t *)rawData)[i]);
        SDDS_CopyString(&data[j++], buffer);
      }
    }
    break;
  case SDDS_ULONG:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        sprintf(buffer, "%" PRIu32, ((uint32_t *)rawData)[i]);
        SDDS_CopyString(&data[j++], buffer);
      }
    }
    break;
  case SDDS_SHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        sprintf(buffer, "%hd", ((short *)rawData)[i]);
        SDDS_CopyString(&data[j++], buffer);
      }
    }
    break;
  case SDDS_USHORT:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        sprintf(buffer, "%hu", ((unsigned short *)rawData)[i]);
        SDDS_CopyString(&data[j++], buffer);
      }
    }
    break;
  case SDDS_CHARACTER:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        sprintf(buffer, "%c", ((char *)rawData)[i]);
        SDDS_CopyString(&data[j++], buffer);
      }
    }
  case SDDS_STRING:
    for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
      if (SDDS_dataset->row_flag[i]) {
        SDDS_CopyString(&data[j++], ((char **)rawData)[i]);
      }
    }
    break;
  }
  if (j != n_rows) {
    SDDS_SetError("Unable to get column--row number mismatch (SDDS_GetColumnInString)");
    return (NULL);
  }
  if (SDDS_GetColumnMemoryMode(SDDS_dataset) == DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS) {
    SDDS_dataset->column_track_memory[index] = 0;
    //Free internal copy now under the assumption that the program will not ask for it again.
    if (type == SDDS_STRING) {
      if (0) {
        //FIX this. It currently causes a memory error in SDDS_ScanData2 with multipage files
        char **ptr = (char **)SDDS_dataset->data[index];
        for (i = 0; i < SDDS_dataset->n_rows_allocated; i++, ptr++)
          if (*ptr)
            free(*ptr);
        free(SDDS_dataset->data[index]);
        SDDS_dataset->data[index] = NULL;
      }
    } else {
      free(SDDS_dataset->data[index]);
      SDDS_dataset->data[index] = NULL;
    }
  }
  return (data);
}

/**
 * @brief Retrieves the data of a specified numerical column as an array of a desired numerical type, considering only rows marked as "of interest".
 *
 * This function extracts data from a specified column within the current data table of a dataset. It processes only those rows that are flagged as "of interest" (i.e., have a non-zero acceptance flag). The extracted data is returned as a newly allocated array of the specified numerical type.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name 
 *   NULL-terminated string specifying the name of the column from which data is to be retrieved.
 * @param desiredType 
 *   Integer constant representing the desired data type for the returned array. Must be one of the supported `SDDS` numerical types (e.g., `SDDS_DOUBLE`, `SDDS_FLOAT`, etc.).
 *
 * @return 
 *   - **Pointer to an array of the desired numerical type** containing the data from the specified column for all rows marked as "of interest".
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized column name, non-numeric column type, memory allocation failure, type casting failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - The caller is responsible for freeing the allocated memory to prevent memory leaks.
 *   - This function assumes that the specified column contains numerical data. Attempting to retrieve data from a non-numeric column (excluding `SDDS_CHARACTER`) will result in an error.
 *
 * @note 
 *   - The number of elements in the returned array corresponds to the number of rows marked as "of interest", which can be obtained using `SDDS_CountRowsOfInterest`.
 *   - If the dataset's memory mode for the column is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the column may be freed after access.
 *   - If the `desiredType` matches the column's data type, this function internally calls `SDDS_GetColumn`. Otherwise, it performs type casting using `SDDS_CastValue`.
 *
 * @sa 
 *   - `SDDS_GetColumnInLongDoubles`
 *   - `SDDS_GetColumnInDoubles`
 *   - `SDDS_GetColumnInFloats`
 *   - `SDDS_GetColumn`
 *   - `SDDS_CountRowsOfInterest`
 */
void *SDDS_GetNumericColumn(SDDS_DATASET *SDDS_dataset, char *column_name, int32_t desiredType) {
  int32_t size, type, desiredTypeSize, index;
  int64_t i, j, n_rows;
  void *data;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetNumericColumn"))
    return (NULL);
  if (!SDDS_NUMERIC_TYPE(desiredType) && desiredType != SDDS_CHARACTER) {
    SDDS_SetError("Unable to get column--desired type is nonnumeric (SDDS_GetNumericColumn)");
    return (NULL);
  }
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_GetNumericColumn)");
    return (NULL);
  }
  if ((type = SDDS_GetColumnType(SDDS_dataset, index)) <= 0 || (size = SDDS_GetTypeSize(type)) <= 0 || (!SDDS_NUMERIC_TYPE(type) && type != SDDS_CHARACTER)) {
    SDDS_SetError("Unable to get column--data size or type undefined or non-numeric (SDDS_GetNumericColumn)");
    return (NULL);
  }
  if (type == desiredType)
    return SDDS_GetColumn(SDDS_dataset, column_name);
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get column--no rows left (SDDS_GetNumericColumn)");
    return (NULL);
  }
  if (!(data = (void *)SDDS_Malloc((desiredTypeSize = SDDS_GetTypeSize(desiredType)) * n_rows))) {
    SDDS_SetError("Unable to get column--memory allocation failure (SDDS_GetNumericColumn)");
    return (NULL);
  }
  for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
    if (SDDS_dataset->row_flag[i] && !SDDS_CastValue(SDDS_dataset->data[index], i, type, desiredType, (char *)data + desiredTypeSize * j++)) {
      SDDS_SetError("Unable to get column--cast to double failed (SDDS_GetNumericColumn)");
      return (NULL);
    }
  }
  if (j != n_rows) {
    SDDS_SetError("Unable to get column--row number mismatch (SDDS_GetNumericColumn)");
    return (NULL);
  }
  if (SDDS_GetColumnMemoryMode(SDDS_dataset) == DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS) {
    SDDS_dataset->column_track_memory[index] = 0;
    //Free internal copy now under the assumption that the program will not ask for it again.
    if (type == SDDS_STRING) {
      if (0) {
        //FIX this. It currently causes a memory error in SDDS_ScanData2 with multipage files
        char **ptr = (char **)SDDS_dataset->data[index];
        for (i = 0; i < SDDS_dataset->n_rows_allocated; i++, ptr++)
          if (*ptr)
            free(*ptr);
        free(SDDS_dataset->data[index]);
        SDDS_dataset->data[index] = NULL;
      }
    } else {
      free(SDDS_dataset->data[index]);
      SDDS_dataset->data[index] = NULL;
    }
  }
  return (data);
}

/**
 * @brief Retrieves the actual row index corresponding to a selected row position within the current data table.
 *
 * This function maps a selected row index (i.e., the position among rows marked as "of interest") to its corresponding actual row index within the dataset's data table.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param srow_index 
 *   Zero-based index representing the position of the selected row among all rows marked as "of interest".
 *
 * @return 
 *   - **Non-negative integer** representing the actual row index within the dataset's data table.
 *   - **-1** if an error occurs (e.g., invalid dataset, tabular data not present, `srow_index` out of range).
 *
 * @warning 
 *   - Ensure that `srow_index` is within the valid range [0, `SDDS_CountRowsOfInterest(SDDS_dataset)` - 1] to avoid out-of-range errors.
 *
 * @note 
 *   - This function is useful when iterating over selected rows and needing to access their actual positions within the dataset.
 *
 * @sa 
 *   - `SDDS_CountRowsOfInterest`
 *   - `SDDS_GetValue`
 *   - `SDDS_GetValueAsDouble`
 */
int64_t SDDS_GetSelectedRowIndex(SDDS_DATASET *SDDS_dataset, int64_t srow_index) {
  int64_t i, j;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetSelectedRowIndex"))
    return (-1);
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_GetSelectedRowIndex"))
    return (-1);
  for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
    if (SDDS_dataset->row_flag[i] && j++ == srow_index)
      break;
  }
  if (i == SDDS_dataset->n_rows)
    return (-1);
  return (i);
}

/**
 * @brief Retrieves the value from a specified column and selected row, optionally storing it in provided memory.
 *
 * This function accesses the value of a specific column and selected row within the current data table of a dataset. It returns the value as a pointer to the data, allowing for both direct access and optional storage in user-provided memory.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name 
 *   NULL-terminated string specifying the name of the column from which the value is to be retrieved.
 * @param srow_index 
 *   Zero-based index representing the position of the selected row among all rows marked as "of interest".
 * @param memory 
 *   Pointer to user-allocated memory where the retrieved value will be stored. If `NULL`, the function allocates memory internally, and the caller is responsible for freeing it.
 *
 * @return 
 *   - **Pointer to the retrieved value** stored in `memory` (if provided) or in newly allocated memory.
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized column name, undefined data type, memory allocation failure, invalid row index). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - If `memory` is `NULL`, the function allocates memory that the caller must free to prevent memory leaks.
 *   - The function does not perform type casting. Ensure that the provided `memory` is of the appropriate type matching the column's data type.
 *   - Modifying the data through the returned pointer affects the internal state of the dataset. Use with caution to avoid unintended side effects.
 *
 * @note 
 *   - For columns containing string data (`SDDS_STRING`), the function copies the string into `memory`. A typical usage would involve passing a pointer to a `char*` variable.
 *     ```c
 *     char *string;
 *     SDDS_GetValue(&SDDS_dataset, "name", index, &string);
 *     // or
 *     string = *(char**)SDDS_GetValue(&SDDS_dataset, "name", index, NULL);
 *     ```
 *   - The number of rows marked as "of interest" can be obtained using `SDDS_CountRowsOfInterest`.
 *
 * @sa 
 *   - `SDDS_GetValueAsDouble`
 *   - `SDDS_GetSelectedRowIndex`
 *   - `SDDS_CountRowsOfInterest`
 */
void *SDDS_GetValue(SDDS_DATASET *SDDS_dataset, char *column_name, int64_t srow_index, void *memory) {
  int32_t type, size, column_index;
  int64_t row_index;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetValue"))
    return (NULL);
  if ((column_index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get value--column name is not recognized (SDDS_GetValue)");
    return (NULL);
  }
  if (!(type = SDDS_GetColumnType(SDDS_dataset, column_index))) {
    SDDS_SetError("Unable to get value--data type undefined (SDDS_GetValue)");
    return (NULL);
  }
  size = SDDS_type_size[type - 1];
  if ((row_index = SDDS_GetSelectedRowIndex(SDDS_dataset, srow_index)) < 0) {
    SDDS_SetError("Unable to get value--row index out of range (SDDS_GetValue)");
    return (NULL);
  }
  if (type != SDDS_STRING) {
    if (!memory && !(memory = SDDS_Malloc(size))) {
      SDDS_SetError("Unable to get value--memory allocation failure (SDDS_GetValue)");
      return (NULL);
    }
    memcpy(memory, (char *)SDDS_dataset->data[column_index] + row_index * size, size);
    return (memory);
  }
  /* for character string data, a typical call would be
   * char *string;
   * SDDS_GetValue(&SDDS_dataset, "name", index, &string)    or
   * string = *(char**)SDDS_GetValue(&SDDS_dataset, "name", index, NULL)
   */
  if (!memory && !(memory = SDDS_Malloc(size))) {
    SDDS_SetError("Unable to get value--memory allocation failure (SDDS_GetValue)");
    return (NULL);
  }
  if (SDDS_CopyString(memory, ((char **)SDDS_dataset->data[column_index])[row_index]))
    return (memory);
  return (NULL);
}

/**
 * @brief Retrieves the value from a specified column and selected row, casting it to a double.
 *
 * This function accesses the value of a specific column and selected row within the current data table of a dataset. It casts the retrieved value to a `double` before returning it.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_name 
 *   NULL-terminated string specifying the name of the column from which the value is to be retrieved.
 * @param srow_index 
 *   Zero-based index representing the position of the selected row among all rows marked as "of interest".
 *
 * @return 
 *   - **Double** representing the casted value from the specified column and row.
 *   - **0** if an error occurs (e.g., invalid dataset, unrecognized column name, undefined data type, invalid row index, non-numeric column type). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - This function only supports numerical data types. Attempting to retrieve and cast data from non-numeric columns (excluding `SDDS_CHARACTER`) will result in an error.
 *
 * @note 
 *   - The function internally allocates temporary memory to store the value before casting. This memory is freed before the function returns.
 *
 * @sa 
 *   - `SDDS_GetValue`
 *   - `SDDS_GetSelectedRowIndex`
 *   - `SDDS_CountRowsOfInterest`
 */
double SDDS_GetValueAsDouble(SDDS_DATASET *SDDS_dataset, char *column_name, int64_t srow_index) {
  int32_t type, size, column_index;
  int64_t row_index;
  void *memory;
  double value = 0;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetValueAsDouble"))
    return (0);
  if ((column_index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get value--column name is not recognized (SDDS_GetValueAsDouble)");
    return (0);
  }
  if (!(type = SDDS_GetColumnType(SDDS_dataset, column_index))) {
    SDDS_SetError("Unable to get value--data type undefined (SDDS_GetValueAsDouble)");
    return (0);
  }
  size = SDDS_type_size[type - 1];
  if ((row_index = SDDS_GetSelectedRowIndex(SDDS_dataset, srow_index)) < 0) {
    SDDS_SetError("Unable to get value--row index out of range (SDDS_GetValueAsDouble)");
    return (0);
  }
  if ((type != SDDS_STRING) && (type != SDDS_CHARACTER)) {
    memory = SDDS_Malloc(size);
    memcpy(memory, (char *)SDDS_dataset->data[column_index] + row_index * size, size);
    switch (type) {
    case SDDS_SHORT:
      value = *(short *)memory;
      break;
    case SDDS_USHORT:
      value = *(unsigned short *)memory;
      break;
    case SDDS_LONG:
      value = *(int32_t *)memory;
      break;
    case SDDS_ULONG:
      value = *(uint32_t *)memory;
      break;
    case SDDS_LONG64:
      value = *(int64_t *)memory;
      break;
    case SDDS_ULONG64:
      value = *(uint64_t *)memory;
      break;
    case SDDS_FLOAT:
      value = *(float *)memory;
      break;
    case SDDS_DOUBLE:
      value = *(double *)memory;
      break;
    case SDDS_LONGDOUBLE:
      value = *(long double *)memory;
      break;
    }
    free(memory);
    return (value);
  }
  SDDS_SetError("Unable to get non-numeric value as double (SDDS_GetValueAsDouble)");
  return (0);
}

/**
 * @brief Retrieves the value from a specified column and selected row, casting it to a double.
 *
 * This function accesses the value of a specific column (identified by its index) and a selected row (identified by its
 * selected row index among rows marked as "of interest") within the current data table of a dataset. It casts the retrieved
 * value to a `double` before returning it.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_index 
 *   Zero-based index of the column from which the value is to be retrieved. Must be within the range [0, n_columns-1].
 * @param srow_index 
 *   Zero-based index representing the position of the selected row among all rows marked as "of interest".
 *
 * @return 
 *   - **Double** representing the casted value from the specified column and row.
 *   - **0.0** if an error occurs (e.g., invalid dataset, column index out of range, undefined data type, row index out of range,
 *     non-numeric column type). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - This function only supports numerical data types. Attempting to retrieve and cast data from non-numeric columns (excluding `SDDS_CHARACTER`)
 *     will result in an error.
 *   - The returned value `0.0` may be ambiguous if it is a valid data value. Always check for errors using `SDDS_CheckError` or similar mechanisms.
 *
 * @note 
 *   - The number of rows marked as "of interest" can be obtained using `SDDS_CountRowsOfInterest`.
 *   - If the dataset's memory mode for the column is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the column may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetValue`
 *   - `SDDS_GetValueAsDouble`
 *   - `SDDS_GetSelectedRowIndex`
 *   - `SDDS_CountRowsOfInterest`
 */
double SDDS_GetValueByIndexAsDouble(SDDS_DATASET *SDDS_dataset, int32_t column_index, int64_t srow_index) {
  int32_t type, size;
  int64_t row_index;
  void *memory;
  double value = 0;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetValueByIndexAsDouble"))
    return (0);
  if (column_index < 0 || column_index >= SDDS_dataset->layout.n_columns) {
    SDDS_SetError("Unable to get value--column index out of range (SDDS_GetValueByIndexAsDouble)");
    return (0);
  }
  if (!(type = SDDS_GetColumnType(SDDS_dataset, column_index))) {
    SDDS_SetError("Unable to get value--data type undefined (SDDS_GetValueByIndexAsDouble)");
    return (0);
  }
  size = SDDS_type_size[type - 1];
  if ((row_index = SDDS_GetSelectedRowIndex(SDDS_dataset, srow_index)) < 0) {
    SDDS_SetError("Unable to get value--row index out of range (SDDS_GetValueByIndexAsDouble)");
    return (0);
  }
  if ((type != SDDS_STRING) && (type != SDDS_CHARACTER)) {
    memory = SDDS_Malloc(size);
    memcpy(memory, (char *)SDDS_dataset->data[column_index] + row_index * size, size);
    switch (type) {
    case SDDS_SHORT:
      value = *(short *)memory;
      break;
    case SDDS_USHORT:
      value = *(unsigned short *)memory;
      break;
    case SDDS_LONG:
      value = *(int32_t *)memory;
      break;
    case SDDS_ULONG:
      value = *(uint32_t *)memory;
      break;
    case SDDS_LONG64:
      value = *(int64_t *)memory;
      break;
    case SDDS_ULONG64:
      value = *(uint64_t *)memory;
      break;
    case SDDS_FLOAT:
      value = *(float *)memory;
      break;
    case SDDS_DOUBLE:
      value = *(double *)memory;
      break;
    case SDDS_LONGDOUBLE:
      value = *(long double *)memory;
      break;
    }
    free(memory);
    return (value);
  }
  SDDS_SetError("Unable to get non-numeric value as double (SDDS_GetValueByIndexAsDouble)");
  return (0);
}

/**
 * @brief Retrieves the value from a specified column and selected row, optionally storing it in provided memory.
 *
 * This function accesses the value of a specific column (identified by its index) and a selected row (identified by its
 * selected row index among rows marked as "of interest") within the current data table of a dataset. The retrieved value is either
 * copied into user-provided memory or returned as a direct pointer to the internal data.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_index 
 *   Zero-based index of the column from which the value is to be retrieved. Must be within the range [0, n_columns-1].
 * @param srow_index 
 *   Zero-based index representing the position of the selected row among all rows marked as "of interest".
 * @param memory 
 *   Pointer to user-allocated memory where the retrieved value will be stored. If `NULL`, the function returns a pointer to the internal data.
 *
 * @return 
 *   - **Pointer to the retrieved value** stored in `memory` (if provided) or to the internal data.
 *   - **NULL** if an error occurs (e.g., invalid dataset, column index out of range, undefined data type, row index out of range, memory allocation failure, non-numeric column type). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - If `memory` is `NULL`, the function returns a direct pointer to the internal data. Modifying the data through this pointer affects the dataset's internal state.
 *   - If `memory` is provided, ensure that it points to sufficient memory to hold the data type of the column.
 *   - This function does not perform type casting. Ensure that the `memory` type matches the column's data type.
 *
 * @note 
 *   - For columns containing string data (`SDDS_STRING`), the function copies the string into `memory`. A typical usage would involve passing a pointer to a `char*` variable.
 *     ```c
 *     char *string;
 *     SDDS_GetValueByIndex(&SDDS_dataset, column_index, index, &string);
 *     // or
 *     string = *(char**)SDDS_GetValueByIndex(&SDDS_dataset, column_index, index, NULL);
 *     ```
 *   - The number of rows marked as "of interest" can be obtained using `SDDS_CountRowsOfInterest`.
 *
 * @sa 
 *   - `SDDS_GetValue`
 *   - `SDDS_GetValueAsDouble`
 *   - `SDDS_GetSelectedRowIndex`
 *   - `SDDS_CountRowsOfInterest`
 */
void *SDDS_GetValueByIndex(SDDS_DATASET *SDDS_dataset, int32_t column_index, int64_t srow_index, void *memory) {
  int32_t type, size;
  int64_t row_index;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetValueByIndex"))
    return (NULL);
  if (column_index < 0 || column_index >= SDDS_dataset->layout.n_columns) {
    SDDS_SetError("Unable to get value--column index out of range (SDDS_GetValueByIndex)");
    return (NULL);
  }
  if (!(type = SDDS_GetColumnType(SDDS_dataset, column_index))) {
    SDDS_SetError("Unable to get value--data type undefined (SDDS_GetValueByIndex)");
    return (NULL);
  }
  size = SDDS_type_size[type - 1];
  if ((row_index = SDDS_GetSelectedRowIndex(SDDS_dataset, srow_index)) < 0) {
    SDDS_SetError("Unable to get value--row index out of range (SDDS_GetValueByIndex)");
    return (NULL);
  }
  if (type != SDDS_STRING) {
    if (memory) {
      memcpy(memory, (char *)SDDS_dataset->data[column_index] + row_index * size, size);
      return (memory);
    }
    return ((char *)SDDS_dataset->data[column_index] + row_index * size);
  }
  /* for character string data, a typical call would be
   * char *string;
   * SDDS_GetValueByIndex(&SDDS_dataset, cindex, index, &string)    or
   * string = *(char**)SDDS_GetValue(&SDDS_dataset, cindex, index, NULL)
   */
  if (!memory)
    memory = SDDS_Malloc(size);
  if (SDDS_CopyString(memory, ((char **)SDDS_dataset->data[column_index])[row_index]))
    return (memory);
  return (NULL);
}

/**
 * @brief Retrieves the value from a specified column and absolute row index, optionally storing it in provided memory.
 *
 * This function accesses the value of a specific column (identified by its index) and an absolute row index within the current
 * data table of a dataset. The retrieved value is either copied into user-provided memory or returned as a direct pointer to the internal data.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param column_index 
 *   Zero-based index of the column from which the value is to be retrieved. Must be within the range [0, n_columns-1].
 * @param row_index 
 *   Absolute zero-based row index within the dataset's data table. Must be within the range [0, n_rows-1].
 * @param memory 
 *   Pointer to user-allocated memory where the retrieved value will be stored. If `NULL`, the function returns a pointer to the internal data.
 *
 * @return 
 *   - **Pointer to the retrieved value** stored in `memory` (if provided) or to the internal data.
 *   - **NULL** if an error occurs (e.g., invalid dataset, column index out of range, row index out of range, undefined data type, memory allocation failure, non-numeric column type). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - If `memory` is `NULL`, the function returns a direct pointer to the internal data. Modifying the data through this pointer affects the dataset's internal state.
 *   - If `memory` is provided, ensure that it points to sufficient memory to hold the data type of the column.
 *   - This function does not perform type casting. Ensure that the `memory` type matches the column's data type.
 *
 * @note 
 *   - Unlike `SDDS_GetValueByIndex`, this function uses an absolute row index rather than a selected row index among rows marked as "of interest".
 *   - For columns containing string data (`SDDS_STRING`), the function copies the string into `memory`. A typical usage would involve passing a pointer to a `char*` variable.
 *     ```c
 *     char *string;
 *     SDDS_GetValueByAbsIndex(&SDDS_dataset, column_index, row_index, &string);
 *     // or
 *     string = *(char**)SDDS_GetValueByAbsIndex(&SDDS_dataset, column_index, row_index, NULL);
 *     ```
 *
 * @sa 
 *   - `SDDS_GetValue`
 *   - `SDDS_GetValueAsDouble`
 *   - `SDDS_CountRowsOfInterest`
 */
void *SDDS_GetValueByAbsIndex(SDDS_DATASET *SDDS_dataset, int32_t column_index, int64_t row_index, void *memory) {
  int32_t type, size;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetValueByAbsIndex"))
    return (NULL);
  if (column_index < 0 || column_index >= SDDS_dataset->layout.n_columns) {
    SDDS_SetError("Unable to get value--column index out of range (SDDS_GetValueByAbsIndex)");
    return (NULL);
  }
  if (row_index < 0 || row_index >= SDDS_dataset->n_rows) {
    SDDS_SetError("Unable to get value--index out of range (SDDS_GetValueByAbsIndex)");
    return (NULL);
  }
  if (!(type = SDDS_GetColumnType(SDDS_dataset, column_index))) {
    SDDS_SetError("Unable to get value--data type undefined (SDDS_GetValueByAbsIndex)");
    return (NULL);
  }
  size = SDDS_type_size[type - 1];
  if (type != SDDS_STRING) {
    if (memory) {
      memcpy(memory, (char *)SDDS_dataset->data[column_index] + row_index * size, size);
      return (memory);
    }
    return ((char *)SDDS_dataset->data[column_index] + row_index * size);
  }
  /* for character string data, a typical call would be
   * char *string;
   * SDDS_GetValueByAbsIndex(&SDDS_dataset, cindex, index, &string)    or
   * string = *(char**)SDDS_GetValue(&SDDS_dataset, cindex, index, NULL)
   */
  if (!memory)
    memory = SDDS_Malloc(size);
  if (SDDS_CopyString(memory, ((char **)SDDS_dataset->data[column_index])[row_index]))
    return (memory);
  return (NULL);
}

/**
 * @brief Determines the data type of the rows based on selected columns in the current data table.
 *
 * This function iterates through all columns marked as "of interest" and verifies that they share the same data type. If all selected columns have a consistent data type, that type is returned. If there is a mismatch in data types among the selected columns, an error is recorded.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 *
 * @return 
 *   - **Integer representing the data type** (as defined by SDDS constants) if all selected columns have the same data type.
 *   - **0** on failure (e.g., invalid dataset, no columns selected, inconsistent data types among selected columns). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - Ensure that at least one column is marked as "of interest" before calling this function to avoid unexpected results.
 *
 * @note 
 *   - This function is useful for operations that require uniform data types across all selected columns.
 *
 * @sa 
 *   - `SDDS_GetRow`
 *   - `SDDS_SetColumnFlags`
 *   - `SDDS_GetColumnType`
 */
int32_t SDDS_GetRowType(SDDS_DATASET *SDDS_dataset) {
  int64_t i;
  int32_t type;
  type = -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetRowType"))
    return (0);
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
    if (!SDDS_dataset->column_flag[i])
      continue;
    if (type == -1)
      type = SDDS_dataset->layout.column_definition[i].type;
    else if (type != SDDS_dataset->layout.column_definition[i].type) {
      SDDS_SetError("Unable to get row type--inconsistent data type for selected columns (SDDS_GetRowType)");
      return (0);
    }
  }
  return (type);
}

/**
 * @brief Retrieves the data of a specific selected row as an array, considering only columns marked as "of interest".
 *
 * This function extracts data from a specific selected row (identified by its selected row index among rows marked as "of interest")
 * within the current data table of a dataset. It processes only those columns that are flagged as "of interest" and returns the row's data
 * as a newly allocated array or stores it in user-provided memory.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param srow_index 
 *   Zero-based index representing the position of the selected row among all rows marked as "of interest".
 * @param memory 
 *   Pointer to user-allocated memory where the retrieved row data will be stored. If `NULL`, the function allocates memory.
 *
 * @return 
 *   - **Pointer to the retrieved row data array** stored in `memory` (if provided) or to newly allocated memory.
 *   - **NULL** if an error occurs (e.g., invalid dataset, row index out of range, inconsistent row types, memory allocation failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - If `memory` is `NULL`, the function allocates memory that the caller must free to prevent memory leaks.
 *   - All selected columns must have the same data type. If there is an inconsistency, the function will fail.
 *   - For columns containing string data (`SDDS_STRING`), each element in the returned array is a dynamically allocated string that must be freed individually, followed by the array itself.
 *
 * @note 
 *   - The number of elements in the returned array corresponds to the number of columns marked as "of interest", which can be obtained using `SDDS_CountColumnsOfInterest`.
 *   - If the dataset's memory mode for the columns is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the columns may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetRowType`
 *   - `SDDS_SetColumnFlags`
 *   - `SDDS_GetValue`
 *   - `SDDS_CountColumnsOfInterest`
 */
void *SDDS_GetRow(SDDS_DATASET *SDDS_dataset, int64_t srow_index, void *memory) {
  void *data;
  int32_t size, type;
  int64_t i, row_index;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetRow"))
    return (NULL);
  if ((row_index = SDDS_GetSelectedRowIndex(SDDS_dataset, srow_index)) < 0) {
    SDDS_SetError("Unable to get row--row index out of range (SDDS_GetRow)");
    return (NULL);
  }
  if (SDDS_dataset->n_of_interest <= 0) {
    SDDS_SetError("Unable to get row--no columns selected (SDDS_GetRow)");
    return (NULL);
  }
  if ((type = SDDS_GetRowType(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get row--inconsistent data type in selected columns (SDDS_GetRow)");
    return (NULL);
  }
  size = SDDS_type_size[type - 1];
  if (memory)
    data = memory;
  else if (!(data = SDDS_Malloc(size * SDDS_dataset->n_of_interest))) {
    SDDS_SetError("Unable to get row--memory allocation failure (SDDS_GetRow)");
    return (NULL);
  }
  if (type != SDDS_STRING)
    for (i = 0; i < SDDS_dataset->n_of_interest; i++)
      memcpy((char *)data + i * size, (char *)SDDS_dataset->data[SDDS_dataset->column_order[i]] + row_index * size, size);
  else
    for (i = 0; i < SDDS_dataset->n_of_interest; i++)
      if (!SDDS_CopyString((char **)data + i, ((char **)SDDS_dataset->data[SDDS_dataset->column_order[i]])[row_index]))
        return (NULL);
  return (data);
}

/**
 * @brief Retrieves all rows marked as "of interest" as a matrix (array of row arrays).
 *
 * This function extracts all rows that are flagged as "of interest" within the current data table of a dataset. It processes only those columns that are flagged as "of interest" and returns the data as a matrix, where each row is an array of values corresponding to the selected columns.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param n_rows 
 *   Pointer to an `int64_t` variable where the number of rows retrieved will be stored.
 *
 * @return 
 *   - **Pointer to an array of pointers**, where each pointer references a row's data array.
 *   - **NULL** if an error occurs (e.g., invalid dataset, no columns selected, inconsistent row types, memory allocation failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - The caller is responsible for freeing the allocated memory to prevent memory leaks. This includes freeing each individual row array followed by the array of pointers itself.
 *   - All selected columns must have the same data type. If there is an inconsistency, the function will fail.
 *
 * @note 
 *   - The number of rows retrieved is stored in the variable pointed to by `n_rows`.
 *   - For columns containing string data (`SDDS_STRING`), each element in the row arrays is a dynamically allocated string that must be freed individually.
 *   - If the dataset's memory mode for the columns is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the columns may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetRowType`
 *   - `SDDS_SetColumnFlags`
 *   - `SDDS_GetRow`
 *   - `SDDS_CountRowsOfInterest`
 */
void *SDDS_GetMatrixOfRows(SDDS_DATASET *SDDS_dataset, int64_t *n_rows) {
  void **data;
  int32_t size, type;
  int64_t i, j, k;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetMatrixOfRows"))
    return (NULL);
  if (SDDS_dataset->n_of_interest <= 0) {
    SDDS_SetError("Unable to get matrix of rows--no columns selected (SDDS_GetMatrixOfRows)");
    return (NULL);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_GetMatrixOfRows"))
    return (NULL);
  if ((type = SDDS_GetRowType(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get row--inconsistent data type in selected columns (SDDS_GetMatrixOfRows)");
    return (NULL);
  }
  size = SDDS_type_size[type - 1];
  if ((*n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get matrix of rows--no rows of interest (SDDS_GetMatrixOfRows)");
    return (NULL);
  }
  if (!(data = (void **)SDDS_Malloc(sizeof(*data) * (*n_rows)))) {
    SDDS_SetError("Unable to get matrix of rows--memory allocation failure (SDDS_GetMatrixOfRows)");
    return (NULL);
  }
  for (j = k = 0; j < SDDS_dataset->n_rows; j++) {
    if (SDDS_dataset->row_flag[j]) {
      if (!(data[k] = SDDS_Malloc(size * SDDS_dataset->n_of_interest))) {
        SDDS_SetError("Unable to get matrix of rows--memory allocation failure (SDDS_GetMatrixOfRows)");
        return (NULL);
      }
      if (type != SDDS_STRING)
        for (i = 0; i < SDDS_dataset->n_of_interest; i++)
          memcpy((char *)data[k] + i * size, (char *)SDDS_dataset->data[SDDS_dataset->column_order[i]] + j * size, size);
      else
        for (i = 0; i < SDDS_dataset->n_of_interest; i++)
          if (!SDDS_CopyString((char **)(data[k]) + i, ((char **)SDDS_dataset->data[SDDS_dataset->column_order[i]])[j]))
            return (NULL);
      k++;
    }
  }
  return (data);
}

/**
 * @brief Retrieves all rows marked as "of interest" as a matrix, casting each value to a specified numerical type.
 *
 * This function extracts all rows that are flagged as "of interest" within the current data table of a dataset and casts each value to a specified numerical type. It processes only those columns that are flagged as "of interest" and returns the data as a matrix, where each row is an array of casted values corresponding to the selected columns.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param n_rows 
 *   Pointer to an `int64_t` variable where the number of rows retrieved will be stored.
 * @param sddsType 
 *   Integer constant representing the desired data type for casting (e.g., `SDDS_DOUBLE`, `SDDS_FLOAT`, etc.). Must be a valid numerical type as defined by SDDS.
 *
 * @return 
 *   - **Pointer to an array of pointers**, where each pointer references a row's data array cast to the specified type.
 *   - **NULL** if an error occurs (e.g., invalid dataset, no columns selected, inconsistent data types among selected columns, non-numeric `sddsType`, memory allocation failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - The caller is responsible for freeing the allocated memory to prevent memory leaks. This includes freeing each individual row array followed by the array of pointers itself.
 *   - All selected columns must have numerical data types. If any selected column is non-numeric, the function will fail.
 *   - Ensure that `sddsType` is a valid numerical type supported by SDDS.
 *
 * @note 
 *   - The number of rows retrieved is stored in the variable pointed to by `n_rows`.
 *   - This function performs type casting using `SDDS_CastValue`. If casting fails for any value, the function will terminate and return `NULL`.
 *   - If the dataset's memory mode for the columns is set to `DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS`, the internal data for the columns may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetMatrixOfRows`
 *   - `SDDS_CastValue`
 *   - `SDDS_GetRowType`
 *   - `SDDS_SetColumnFlags`
 *   - `SDDS_CountRowsOfInterest`
 */
void *SDDS_GetCastMatrixOfRows(SDDS_DATASET *SDDS_dataset, int64_t *n_rows, int32_t sddsType) {
  void **data;
  int32_t size;
  int64_t i, j, k;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetCastMatrixOfRows"))
    return (NULL);
  if (!SDDS_NUMERIC_TYPE(sddsType)) {
    SDDS_SetError("Unable to get matrix of rows--no columns selected (SDDS_GetCastMatrixOfRows)");
    return NULL;
  }
  if (SDDS_dataset->n_of_interest <= 0) {
    SDDS_SetError("Unable to get matrix of rows--no columns selected (SDDS_GetCastMatrixOfRows)");
    return (NULL);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_GetCastMatrixOfRows"))
    return (NULL);
  size = SDDS_type_size[sddsType - 1];
  if ((*n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get matrix of rows--no rows of interest (SDDS_GetCastMatrixOfRows)");
    return (NULL);
  }
  if (!(data = (void **)SDDS_Malloc(sizeof(*data) * (*n_rows)))) {
    SDDS_SetError("Unable to get matrix of rows--memory allocation failure (SDDS_GetCastMatrixOfRows)");
    return (NULL);
  }
  for (i = 0; i < SDDS_dataset->n_of_interest; i++) {
    if (!SDDS_NUMERIC_TYPE(SDDS_dataset->layout.column_definition[SDDS_dataset->column_order[i]].type)) {
      SDDS_SetError("Unable to get matrix of rows--not all columns are numeric (SDDS_GetCastMatrixOfRows)");
      return NULL;
    }
  }
  for (j = k = 0; j < SDDS_dataset->n_rows; j++) {
    if (SDDS_dataset->row_flag[j]) {
      if (!(data[k] = SDDS_Malloc(size * SDDS_dataset->n_of_interest))) {
        SDDS_SetError("Unable to get matrix of rows--memory allocation failure (SDDS_GetCastMatrixOfRows)");
        return (NULL);
      }
      for (i = 0; i < SDDS_dataset->n_of_interest; i++)
        SDDS_CastValue(SDDS_dataset->data[SDDS_dataset->column_order[i]], j, SDDS_dataset->layout.column_definition[SDDS_dataset->column_order[i]].type, sddsType, (char *)data[k] + i * sizeof(double));
      k++;
    }
  }
  return (data);
}

/**
 * @brief Retrieves multiple parameter values from the current data table of a data set.
 *
 * This variadic function allows the retrieval of multiple parameter values in a single call. Each parameter's name and corresponding memory location are provided as pairs of arguments.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param ... 
 *   Variable arguments consisting of pairs of:
 *     - `char *parameter_name`: NULL-terminated string specifying the name of the parameter.
 *     - `void *memory`: Pointer to memory where the parameter's value will be stored. If `NULL`, the function will fail for that parameter.
 *     - The argument list should be terminated when `parameter_name` is `NULL`.
 *
 * @return 
 *   - **1** on successful retrieval of all specified parameters.
 *   - **0** if any parameter retrieval fails. An error message is recorded for the first failure encountered.
 *
 * @warning 
 *   - The function expects an even number of arguments (pairs of parameter names and memory pointers). An odd number may result in undefined behavior.
 *   - Ensure that the `memory` pointers provided are of appropriate types and have sufficient space to hold the parameter values.
 *
 * @note 
 *   - To terminate the argument list, pass a `NULL` as the parameter name.
 *     ```c
 *     SDDS_GetParameters(&SDDS_dataset, "param1", &value1, "param2", &value2, NULL);
 *     ```
 *
 * @sa 
 *   - `SDDS_GetParameter`
 *   - `SDDS_GetParameterByIndex`
 */
int32_t SDDS_GetParameters(SDDS_DATASET *SDDS_dataset, ...) {
  va_list argptr;
  char *name;
  void *data;
  int32_t retval;
  char s[SDDS_MAXLINE];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameters"))
    return 0;
  va_start(argptr, SDDS_dataset);
  retval = 1;
  do {
    if (!(name = va_arg(argptr, char *)))
      break;
    if (!(data = va_arg(argptr, void *)))
      retval = 0;
    if (!SDDS_GetParameter(SDDS_dataset, name, data)) {
      sprintf(s, "Unable to get value of parameter %s (SDDS_GetParameters)", name);
      SDDS_SetError(s);
    }
  } while (retval);
  va_end(argptr);
  return retval;
}

/**
 * @brief Retrieves the value of a specified parameter from the current data table of a data set.
 *
 * This function accesses the value of a specific parameter (identified by its name) within the current data table of a dataset. The retrieved value is either copied into user-provided memory or returned as a direct pointer to the internal data.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param parameter_name 
 *   NULL-terminated string specifying the name of the parameter from which the value is to be retrieved.
 * @param memory 
 *   Pointer to user-allocated memory where the retrieved parameter value will be stored. If `NULL`, the function allocates memory.
 *
 * @return 
 *   - **Pointer to the retrieved parameter value** stored in `memory` (if provided) or to newly allocated memory.
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized parameter name, undefined data type, memory allocation failure, string copy failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - If `memory` is `NULL`, the function allocates memory that the caller must free to prevent memory leaks.
 *   - For parameters containing string data (`SDDS_STRING`), the function copies the string into `memory`. A typical usage would involve passing a pointer to a `char*` variable.
 *     ```c
 *     char *string;
 *     SDDS_GetParameter(&SDDS_dataset, "parameter_name", &string);
 *     // or
 *     string = *(char**)SDDS_GetParameter(&SDDS_dataset, "parameter_name", NULL);
 *     ```
 *
 * @note 
 *   - The size of the allocated memory corresponds to the parameter's data type, which can be obtained using `SDDS_GetParameterType`.
 *   - If the dataset's memory mode for the parameter is set to `DONT_TRACK_PARAMETER_MEMORY_AFTER_ACCESS`, the internal data for the parameter may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetParameters`
 *   - `SDDS_GetParameterByIndex`
 *   - `SDDS_SetParameter`
 *   - `SDDS_GetParameterType`
 */
void *SDDS_GetParameter(SDDS_DATASET *SDDS_dataset, char *parameter_name, void *memory) {
  int32_t index, type, size;
  char s[SDDS_MAXLINE];
  void *data;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameter"))
    return (NULL);
  if (!parameter_name) {
    SDDS_SetError("Unable to get parameter value--parameter name pointer is NULL (SDDS_GetParameter)");
    return (NULL);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    sprintf(s, "Unable to get parameter value--parameter name %s is unrecognized (SDDS_GetParameter)", parameter_name);
    SDDS_SetError(s);
    return (NULL);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameter)");
    return (NULL);
  }
  if (!SDDS_dataset->parameter || !SDDS_dataset->parameter[index]) {
    SDDS_SetError("Unable to get parameter value--parameter data array is NULL (SDDS_GetParameter)");
    return (NULL);
  }
  size = SDDS_type_size[type - 1];
  if (memory)
    data = memory;
  else if (!(data = SDDS_Malloc(size))) {
    SDDS_SetError("Unable to get parameter value--parameter data size is invalid (SDDS_GetParameter)");
    return (NULL);
  }
  if (type != SDDS_STRING)
    memcpy(data, SDDS_dataset->parameter[index], size);
  else if (!SDDS_CopyString((char **)data, *(char **)SDDS_dataset->parameter[index]))
    return (NULL);
  return (data);
}

/**
 * @brief Retrieves the value of a specified parameter by its index from the current data table of a data set.
 *
 * This function accesses the value of a specific parameter (identified by its index) within the current data table of a dataset. The retrieved value is either copied into user-provided memory or returned as a direct pointer to the internal data.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param index 
 *   Zero-based index of the parameter to retrieve. Must be within the range [0, n_parameters-1].
 * @param memory 
 *   Pointer to user-allocated memory where the retrieved parameter value will be stored. If `NULL`, the function allocates memory.
 *
 * @return 
 *   - **Pointer to the retrieved parameter value** stored in `memory` (if provided) or to newly allocated memory.
 *   - **NULL** if an error occurs (e.g., invalid dataset, parameter index out of range, undefined data type, memory allocation failure, string copy failure). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - If `memory` is `NULL`, the function allocates memory that the caller must free to prevent memory leaks.
 *   - For parameters containing string data (`SDDS_STRING`), the function copies the string into `memory`. A typical usage would involve passing a pointer to a `char*` variable.
 *     ```c
 *     char *string;
 *     SDDS_GetParameterByIndex(&SDDS_dataset, index, &string);
 *     // or
 *     string = *(char**)SDDS_GetParameterByIndex(&SDDS_dataset, index, NULL);
 *     ```
 *
 * @note 
 *   - The size of the allocated memory corresponds to the parameter's data type, which can be obtained using `SDDS_GetParameterType`.
 *   - If the dataset's memory mode for the parameter is set to `DONT_TRACK_PARAMETER_MEMORY_AFTER_ACCESS`, the internal data for the parameter may be freed after access.
 *
 * @sa 
 *   - `SDDS_GetParameters`
 *   - `SDDS_GetParameter`
 *   - `SDDS_SetParameter`
 *   - `SDDS_GetParameterType`
 */
void *SDDS_GetParameterByIndex(SDDS_DATASET *SDDS_dataset, int32_t index, void *memory) {
  int32_t type, size;
  void *data;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameter"))
    return (NULL);
  if (index < 0 || index >= SDDS_dataset->layout.n_parameters) {
    SDDS_SetError("Unable to get parameter value--parameter index is invalid (SDDS_GetParameterByIndex)");
    return (NULL);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameterByIndex)");
    return (NULL);
  }
  if (!SDDS_dataset->parameter || !SDDS_dataset->parameter[index]) {
    SDDS_SetError("Unable to get parameter value--parameter data array is NULL (SDDS_GetParameterByIndex)");
    return (NULL);
  }
  size = SDDS_type_size[type - 1];
  if (memory)
    data = memory;
  else if (!(data = SDDS_Malloc(size))) {
    SDDS_SetError("Unable to get parameter value--parameter data size is invalid (SDDS_GetParameterByIndex)");
    return (NULL);
  }
  if (type != SDDS_STRING)
    memcpy(data, SDDS_dataset->parameter[index], size);
  else if (!SDDS_CopyString((char **)data, *(char **)SDDS_dataset->parameter[index]))
    return (NULL);
  return (data);
}

/**
 * @brief Retrieves the value of a specified parameter as a 32-bit integer from the current data table of a data set.
 *
 * This function accesses the value of a specific parameter (identified by its name) within the current data table of a dataset and converts it to a 32-bit integer (`int32_t`). The converted value is either stored in user-provided memory or allocated by the function.
 *
 * @param SDDS_dataset 
 *   Pointer to the `SDDS_DATASET` structure representing the data set.
 * @param parameter_name 
 *   NULL-terminated string specifying the name of the parameter from which the value is to be retrieved.
 * @param memory 
 *   Pointer to a 32-bit integer where the converted parameter value will be stored. If `NULL`, the function allocates memory.
 *
 * @return 
 *   - **Pointer to the `int32_t` value** stored in `memory` (if provided) or to newly allocated memory containing the converted value.
 *   - **NULL** if an error occurs (e.g., invalid dataset, unrecognized parameter name, undefined data type, memory allocation failure, parameter type is `SDDS_STRING`). In this case, an error message is recorded internally.
 *
 * @warning 
 *   - If `memory` is `NULL`, the function allocates memory that the caller must free to prevent memory leaks.
 *   - This function does not support parameters of type `SDDS_STRING`. Attempting to retrieve string parameters as long integers will result in an error.
 *
 * @note 
 *   - The conversion is performed using `SDDS_ConvertToLong`, which handles casting from various numerical types to `int32_t`.
 *   - Ensure that the parameter's data type is compatible with 32-bit integer conversion to avoid data loss or undefined behavior.
 *
 * @sa 
 *   - `SDDS_GetParameter`
 *   - `SDDS_GetParameterByIndex`
 *   - `SDDS_ConvertToLong`
 *   - `SDDS_GetParameterType`
 */
int32_t *SDDS_GetParameterAsLong(SDDS_DATASET *SDDS_dataset, char *parameter_name, int32_t *memory) {
  int32_t index, type;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterAsLong"))
    return (NULL);
  if (!parameter_name) {
    SDDS_SetError("Unable to get parameter value--parameter name pointer is NULL (SDDS_GetParameterAsLong)");
    return (NULL);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    SDDS_SetError("Unable to get parameter value--parameter name is unrecognized (SDDS_GetParameterAsLong)");
    return (NULL);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameterAsLong)");
    return (NULL);
  }
  if (type == SDDS_STRING) {
    SDDS_SetError("Unable to get parameter value--parameter data type is SDDS_STRING (SDDS_GetParameterAsLong)");
    return (NULL);
  }
  if (!SDDS_dataset->parameter || !SDDS_dataset->parameter[index]) {
    SDDS_SetError("Unable to get parameter value--parameter data array is NULL (SDDS_GetParameterAsLong)");
    return (NULL);
  }

  if (!memory && !(memory = (int32_t *)SDDS_Malloc(sizeof(int32_t)))) {
    SDDS_SetError("Unable to get parameter value--memory allocation failure (SDDS_GetParameterAsLong)");
    return (NULL);
  }

  *memory = SDDS_ConvertToLong(type, SDDS_dataset->parameter[index], 0);
  return (memory);
}

/**
 * @brief Retrieves the value of a specified parameter as a 64-bit integer from the current data table of an SDDS dataset.
 *
 * This function looks up the parameter by name within the given SDDS dataset and returns its value as an `int64_t`.
 * If the `memory` pointer is provided, the value is stored at the given memory location. Otherwise, the function
 * allocates memory to store the value, which must be freed by the caller.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param parameter_name A null-terminated string specifying the name of the parameter to retrieve.
 * @param memory Optional pointer to an `int64_t` variable where the parameter value will be stored. If `NULL`,
 *               memory is allocated internally to hold the value.
 *
 * @return On success, returns a pointer to the `int64_t` containing the parameter value. On failure, returns `NULL`
 *         and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, parameter not found, type mismatch, or memory allocation failure).
 * @retval Non-NULL Pointer to the `int64_t` containing the parameter value.
 *
 * @note The caller is responsible for freeing the allocated memory if the `memory` parameter is `NULL`.
 *
 * @sa SDDS_GetParameterAsDouble, SDDS_GetParameterAsString
 */
int64_t *SDDS_GetParameterAsLong64(SDDS_DATASET *SDDS_dataset, char *parameter_name, int64_t *memory) {
  int32_t index, type;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterAsLong64"))
    return (NULL);
  if (!parameter_name) {
    SDDS_SetError("Unable to get parameter value--parameter name pointer is NULL (SDDS_GetParameterAsLong64)");
    return (NULL);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    SDDS_SetError("Unable to get parameter value--parameter name is unrecognized (SDDS_GetParameterAsLong64)");
    return (NULL);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameterAsLong64)");
    return (NULL);
  }
  if (type == SDDS_STRING) {
    SDDS_SetError("Unable to get parameter value--parameter data type is SDDS_STRING (SDDS_GetParameterAsLong64)");
    return (NULL);
  }
  if (!SDDS_dataset->parameter || !SDDS_dataset->parameter[index]) {
    SDDS_SetError("Unable to get parameter value--parameter data array is NULL (SDDS_GetParameterAsLong64)");
    return (NULL);
  }

  if (!memory && !(memory = (int64_t *)SDDS_Malloc(sizeof(int64_t)))) {
    SDDS_SetError("Unable to get parameter value--memory allocation failure (SDDS_GetParameterAsLong64)");
    return (NULL);
  }

  *memory = SDDS_ConvertToLong64(type, SDDS_dataset->parameter[index], 0);
  return (memory);
}

/**
 * @brief Retrieves the value of a specified parameter as a `long double` from the current data table of an SDDS dataset.
 *
 * This function searches for the parameter by name within the provided SDDS dataset and retrieves its value as a `long double`.
 * If the `memory` pointer is supplied, the value is stored at the specified memory location. If `memory` is `NULL`,
 * the function allocates memory for storing the value, which should be freed by the caller.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param parameter_name A null-terminated string specifying the name of the parameter to retrieve.
 * @param memory Optional pointer to a `long double` variable where the parameter value will be stored. If `NULL`,
 *               memory is allocated internally to hold the value.
 *
 * @return On success, returns a pointer to the `long double` containing the parameter value. On failure, returns `NULL`
 *         and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, parameter not found, type mismatch, or memory allocation failure).
 * @retval Non-NULL Pointer to the `long double` containing the parameter value.
 *
 * @note The caller is responsible for freeing the allocated memory if the `memory` parameter is `NULL`.
 *
 * @sa SDDS_GetParameterAsDouble, SDDS_GetParameterAsLong64, SDDS_GetParameterAsString
 */
long double *SDDS_GetParameterAsLongDouble(SDDS_DATASET *SDDS_dataset, char *parameter_name, long double *memory) {
  int32_t index = -1, type = -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterAsLongDouble"))
    return (NULL);
  if (!parameter_name) {
    SDDS_SetError("Unable to get parameter value--parameter name pointer is NULL (SDDS_GetParameterAsLongDouble)");
    return (NULL);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    SDDS_SetError("Unable to get parameter value--parameter name is unrecognized (SDDS_GetParameterAsLongDouble)");
    return (NULL);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameterAsLongDouble)");
    return (NULL);
  }
  if (type == SDDS_STRING) {
    SDDS_SetError("Unable to get parameter value--parameter data type is SDDS_STRING (SDDS_GetParameterAsLongDouble)");
    return (NULL);
  }
  if (!SDDS_dataset->parameter || !SDDS_dataset->parameter[index]) {
    SDDS_SetError("Unable to get parameter value--parameter data array is NULL (SDDS_GetParameterAsLongDouble)");
    return (NULL);
  }

  if (!memory && !(memory = (long double *)SDDS_Malloc(sizeof(long double)))) {
    SDDS_SetError("Unable to get parameter value--memory allocation failure (SDDS_GetParameterAsLongDouble)");
    return (NULL);
  }
  *memory = SDDS_ConvertToLongDouble(type, SDDS_dataset->parameter[index], 0);
  return (memory);
}

/**
 * @brief Retrieves the value of a specified parameter as a `double` from the current data table of an SDDS dataset.
 *
 * This function searches for the parameter by name within the provided SDDS dataset and retrieves its value as a `double`.
 * If the `memory` pointer is supplied, the value is stored at the specified memory location. If `memory` is `NULL`,
 * the function allocates memory for storing the value, which should be freed by the caller.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param parameter_name A null-terminated string specifying the name of the parameter to retrieve.
 * @param memory Optional pointer to a `double` variable where the parameter value will be stored. If `NULL`,
 *               memory is allocated internally to hold the value.
 *
 * @return On success, returns a pointer to the `double` containing the parameter value. On failure, returns `NULL`
 *         and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, parameter not found, type mismatch, or memory allocation failure).
 * @retval Non-NULL Pointer to the `double` containing the parameter value.
 *
 * @note The caller is responsible for freeing the allocated memory if the `memory` parameter is `NULL`.
 *
 * @sa SDDS_GetParameterAsLong64, SDDS_GetParameterAsLongDouble, SDDS_GetParameterAsString
 */
double *SDDS_GetParameterAsDouble(SDDS_DATASET *SDDS_dataset, char *parameter_name, double *memory) {
  int32_t index = -1, type = -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterAsDouble"))
    return (NULL);
  if (!parameter_name) {
    SDDS_SetError("Unable to get parameter value--parameter name pointer is NULL (SDDS_GetParameterAsDouble)");
    return (NULL);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    SDDS_SetError("Unable to get parameter value--parameter name is unrecognized (SDDS_GetParameterAsDouble)");
    return (NULL);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameterAsDouble)");
    return (NULL);
  }
  if (type == SDDS_STRING) {
    SDDS_SetError("Unable to get parameter value--parameter data type is SDDS_STRING (SDDS_GetParameterAsDouble)");
    return (NULL);
  }
  if (!SDDS_dataset->parameter || !SDDS_dataset->parameter[index]) {
    SDDS_SetError("Unable to get parameter value--parameter data array is NULL (SDDS_GetParameterAsDouble)");
    return (NULL);
  }

  if (!memory && !(memory = (double *)SDDS_Malloc(sizeof(double)))) {
    SDDS_SetError("Unable to get parameter value--memory allocation failure (SDDS_GetParameterAsDouble)");
    return (NULL);
  }
  *memory = SDDS_ConvertToDouble(type, SDDS_dataset->parameter[index], 0);
  return (memory);
}

/**
 * @brief Retrieves the value of a specified parameter as a string from the current data table of an SDDS dataset.
 *
 * This function searches for the parameter by name within the provided SDDS dataset and retrieves its value as a string.
 * The function formats the parameter's value based on its data type. If the `memory` pointer is provided, the string
 * is stored at the specified memory location. Otherwise, the function allocates memory to hold the string, which
 * must be freed by the caller.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param parameter_name A null-terminated string specifying the name of the parameter to retrieve.
 * @param memory Optional pointer to a `char*` variable where the string will be stored. If `NULL`, memory is allocated
 *               internally to hold the string.
 *
 * @return On success, returns a pointer to the null-terminated string containing the parameter value. On failure, returns `NULL`
 *         and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, parameter not found, type mismatch, memory allocation failure, or unknown data type).
 * @retval Non-NULL Pointer to the null-terminated string containing the parameter value.
 *
 * @note The caller is responsible for freeing the allocated memory if the `memory` parameter is `NULL`.
 *
 * @sa SDDS_GetParameterAsDouble, SDDS_GetParameterAsLong64, SDDS_GetParameterAsLongDouble
 */
char *SDDS_GetParameterAsString(SDDS_DATASET *SDDS_dataset, char *parameter_name, char **memory) {
  int32_t index, type;
  char buffer[SDDS_MAXLINE], *parValue;
  void *value;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterAsString"))
    return (NULL);
  if (!parameter_name) {
    SDDS_SetError("Unable to get parameter value--parameter name pointer is NULL (SDDS_GetParameterAsString)");
    return (NULL);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    SDDS_SetError("Unable to get parameter value--parameter name is unrecognized (SDDS_GetParameterAsString)");
    return (NULL);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameterAsString)");
    return (NULL);
  }
  value = SDDS_dataset->parameter[index];
  switch (type) {
  case SDDS_LONGDOUBLE:
    if (LDBL_DIG == 18) {
      sprintf(buffer, "%.18Le", *(long double *)value);
    } else {
      sprintf(buffer, "%.15Le", *(long double *)value);
    }
    break;
  case SDDS_DOUBLE:
    sprintf(buffer, "%.15le", *(double *)value);
    break;
  case SDDS_FLOAT:
    sprintf(buffer, "%.8e", *(float *)value);
    break;
  case SDDS_LONG64:
    sprintf(buffer, "%" PRId64, *(int64_t *)value);
    break;
  case SDDS_ULONG64:
    sprintf(buffer, "%" PRIu64, *(uint64_t *)value);
    break;
  case SDDS_LONG:
    sprintf(buffer, "%" PRId32, *(int32_t *)value);
    break;
  case SDDS_ULONG:
    sprintf(buffer, "%" PRIu32, *(uint32_t *)value);
    break;
  case SDDS_SHORT:
    sprintf(buffer, "%hd", *(short *)value);
    break;
  case SDDS_USHORT:
    sprintf(buffer, "%hu", *(unsigned short *)value);
    break;
  case SDDS_CHARACTER:
    sprintf(buffer, "%c", *(char *)value);
    break;
  case SDDS_STRING:
    sprintf(buffer, "%s", *(char **)value);
    break;
  default:
    SDDS_SetError("Unknown data type of parameter (SDDS_GetParameterAsString)");
    return (NULL);
  }
  if (!(parValue = malloc(sizeof(char) * (strlen(buffer) + 1)))) {
    SDDS_SetError("Unable to get parameter value--memory allocation failure (SDDS_GetParameterAsString)");
    return (NULL);
  }
  strcpy(parValue, buffer);
  if (memory)
    *memory = parValue;
  return parValue;
}

/**
 * @brief Retrieves the value of a specified parameter as a formatted string from the current data table of an SDDS dataset.
 *
 * This function searches for the parameter by name within the provided SDDS dataset, formats its value based on the supplied format string,
 * and returns it as a null-terminated string. If `suppliedformat` is `NULL`, the function uses the format string defined in the parameter's
 * definition. If the `memory` pointer is provided, the formatted string is stored at the specified memory location. Otherwise, memory is
 * allocated internally to hold the string, which must be freed by the caller.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param parameter_name A null-terminated string specifying the name of the parameter to retrieve.
 * @param memory Optional pointer to a `char*` variable where the formatted string will be stored. If `NULL`, memory is allocated
 *               internally to hold the string.
 * @param suppliedformat A null-terminated format string (similar to `printf` format specifiers) to format the parameter value. If `NULL`,
 *                       the function uses the format string defined in the parameter's definition within the dataset.
 *
 * @return On success, returns a pointer to the null-terminated formatted string containing the parameter value. On failure, returns `NULL`
 *         and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, parameter not found, invalid format string, type mismatch, memory allocation failure, or unknown data type).
 * @retval Non-NULL Pointer to the null-terminated string containing the formatted parameter value.
 *
 * @note The caller is responsible for freeing the allocated memory if the `memory` parameter is `NULL`.
 *
 * @sa SDDS_GetParameterAsDouble, SDDS_GetParameterAsString, SDDS_GetParameterAsLong64, SDDS_GetParameterAsLongDouble
 */
char *SDDS_GetParameterAsFormattedString(SDDS_DATASET *SDDS_dataset, char *parameter_name, char **memory, char *suppliedformat) {
  int32_t index, type;
  char buffer[SDDS_MAXLINE], *parValue;
  void *value;
  char *format = NULL;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterAsFormattedString"))
    return (NULL);
  if (!parameter_name) {
    SDDS_SetError("Unable to get parameter value--parameter name pointer is NULL (SDDS_GetParameterAsFormattedString)");
    return (NULL);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    SDDS_SetError("Unable to get parameter value--parameter name is unrecognized (SDDS_GetParameterAsFormattedString)");
    return (NULL);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameterAsFormattedString)");
    return (NULL);
  }
  if (suppliedformat != NULL) {
    format = suppliedformat;
    if (!SDDS_VerifyPrintfFormat(format, type)) {
      SDDS_SetError("Unable to get parameter value--given format for parameter is invalid (SDDS_GetParameterAsFormattedString)");
      return (NULL);
    }
  } else {
    if (SDDS_GetParameterInformation(SDDS_dataset, "format_string", &format, SDDS_GET_BY_INDEX, index) != SDDS_STRING) {
      SDDS_SetError("Unable to get parameter value--parameter definition is invalid (SDDS_GetParameterAsFormattedString)");
      return (NULL);
    }
  }
  value = SDDS_dataset->parameter[index];

  if (!SDDS_StringIsBlank(format)) {
    switch (type) {
    case SDDS_LONGDOUBLE:
      sprintf(buffer, format, *(long double *)value);
      break;
    case SDDS_DOUBLE:
      sprintf(buffer, format, *(double *)value);
      break;
    case SDDS_FLOAT:
      sprintf(buffer, format, *(float *)value);
      break;
    case SDDS_LONG64:
      sprintf(buffer, format, *(int64_t *)value);
      break;
    case SDDS_ULONG64:
      sprintf(buffer, format, *(uint64_t *)value);
      break;
    case SDDS_LONG:
      sprintf(buffer, format, *(int32_t *)value);
      break;
    case SDDS_ULONG:
      sprintf(buffer, format, *(uint32_t *)value);
      break;
    case SDDS_SHORT:
      sprintf(buffer, format, *(short *)value);
      break;
    case SDDS_USHORT:
      sprintf(buffer, format, *(unsigned short *)value);
      break;
    case SDDS_CHARACTER:
      sprintf(buffer, format, *(char *)value);
      break;
    case SDDS_STRING:
      sprintf(buffer, format, *(char **)value);
      break;
    default:
      SDDS_SetError("Unknown data type of parameter (SDDS_GetParameterAsFormattedString)");
      return (NULL);
    }
  } else {
    switch (type) {
    case SDDS_LONGDOUBLE:
      if (LDBL_DIG == 18) {
        sprintf(buffer, "%22.18Le", *(long double *)value);
      } else {
        sprintf(buffer, "%22.15Le", *(long double *)value);
      }
      break;
    case SDDS_DOUBLE:
      sprintf(buffer, "%22.15le", *(double *)value);
      break;
    case SDDS_FLOAT:
      sprintf(buffer, "%15.8e", *(float *)value);
      break;
    case SDDS_LONG64:
      sprintf(buffer, "%" PRId64, *(int64_t *)value);
      break;
    case SDDS_ULONG64:
      sprintf(buffer, "%" PRIu64, *(uint64_t *)value);
      break;
    case SDDS_LONG:
      sprintf(buffer, "%" PRId32, *(int32_t *)value);
      break;
    case SDDS_ULONG:
      sprintf(buffer, "%" PRIu32, *(uint32_t *)value);
      break;
    case SDDS_SHORT:
      sprintf(buffer, "%hd", *(short *)value);
      break;
    case SDDS_USHORT:
      sprintf(buffer, "%hu", *(unsigned short *)value);
      break;
    case SDDS_CHARACTER:
      sprintf(buffer, "%c", *(char *)value);
      break;
    case SDDS_STRING:
      sprintf(buffer, "%s", *(char **)value);
      break;
    default:
      SDDS_SetError("Unknown data type of parameter (SDDS_GetParameterAsFormattedString)");
      return (NULL);
    }
  }
  if (!(parValue = malloc(sizeof(char) * (strlen(buffer) + 1)))) {
    SDDS_SetError("Unable to get parameter value--memory allocation failure (SDDS_GetParameterAsFormattedString)");
    return (NULL);
  }
  strcpy(parValue, buffer);
  if (memory)
    *memory = parValue;
  return parValue;
}

/**
 * @brief Retrieves the fixed value of a specified parameter from an SDDS dataset.
 *
 * This function accesses the fixed value defined for a given parameter in the dataset's layout and converts it to the appropriate data type.
 * If the `memory` pointer is provided, the converted value is stored at the specified memory location. Otherwise, memory is allocated
 * internally to hold the value, which must be freed by the caller.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param parameter_name A null-terminated string specifying the name of the parameter whose fixed value is to be retrieved.
 * @param memory Optional pointer to a memory location where the fixed value will be stored. The size of the memory should correspond
 *               to the size of the parameter's data type. If `NULL`, memory is allocated internally to hold the value.
 *
 * @return On success, returns a pointer to the memory containing the fixed value. On failure, returns `NULL` and sets an appropriate
 *         error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, parameter not found, invalid data type, memory allocation failure, or scan failure).
 * @retval Non-NULL Pointer to the memory containing the fixed parameter value.
 *
 * @note The caller is responsible for freeing the allocated memory if the `memory` parameter is `NULL`.
 *
 * @sa SDDS_SetParameterFixedValue, SDDS_GetParameterAsDouble, SDDS_GetParameterAsString
 */
void *SDDS_GetFixedValueParameter(SDDS_DATASET *SDDS_dataset, char *parameter_name, void *memory) {
  int32_t index, type, size;
  void *data;
  char s[SDDS_MAXLINE];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetFixValueParameter"))
    return (NULL);
  if (!parameter_name) {
    SDDS_SetError("Unable to get parameter value--parameter name pointer is NULL (SDDS_GetFixedValueParameter)");
    return (NULL);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    SDDS_SetError("Unable to get parameter value--parameter name is unrecognized (SDDS_GetFixedValueParameter)");
    return (NULL);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetFixedValueParameter)");
    return (NULL);
  }
  size = SDDS_type_size[type - 1];
  if (memory)
    data = memory;
  else if (!(data = SDDS_Malloc(size))) {
    SDDS_SetError("Unable to get parameter value--parameter data size is invalid (SDDS_GetFixedValueParameter)");
    return (NULL);
  }
  strcpy(s, SDDS_dataset->layout.parameter_definition[index].fixed_value);
  if (!SDDS_ScanData(s, type, 0, data, 0, 1)) {
    SDDS_SetError("Unable to retrieve fixed-value paramter--scan failed (SDDS_GetFixedValueParameter)");
    return (NULL);
  }
  return (data);
}

/**
 * @brief Extracts a matrix from a specified column in the current data table of an SDDS dataset.
 *
 * This function retrieves the data from the specified column and organizes it into a matrix with the given dimensions. The
 * data is arranged in either row-major or column-major order based on the `mode` parameter. The function allocates memory for
 * the matrix, which should be freed by the caller.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param column_name A null-terminated string specifying the name of the column from which to extract the matrix.
 * @param dimension1 The number of rows in the resulting matrix.
 * @param dimension2 The number of columns in the resulting matrix.
 * @param mode Specifies the data layout in the matrix. Use `SDDS_ROW_MAJOR_DATA` for row-major order or `SDDS_COLUMN_MAJOR_DATA`
 *             for column-major order.
 *
 * @return On success, returns a pointer to the allocated matrix. The matrix is an array of pointers, where each pointer refers to a row
 *         (for row-major) or a column (for column-major) in the matrix. On failure, returns `NULL` and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, column not found, dimension mismatch, memory allocation failure).
 * @retval Non-NULL Pointer to the allocated matrix.
 *
 * @note The caller is responsible for freeing the allocated matrix and its contents.
 *
 * @sa SDDS_GetDoubleMatrixFromColumn, SDDS_GetMatrixFromRow, SDDS_AllocateMatrix
 */
void *SDDS_GetMatrixFromColumn(SDDS_DATASET *SDDS_dataset, char *column_name, int64_t dimension1, int64_t dimension2, int32_t mode) {
  int32_t size, type, index;
  int64_t n_rows, i, j;
  void **data, *column;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetMatrixFromColumn"))
    return (NULL);
  if (!column_name) {
    SDDS_SetError("Unable to get matrix--column name is NULL (SDDS_GetMatrixFromColumn)");
    return (NULL);
  }
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get matrix--no rows selected (SDDS_GetMatrixFromColumn)");
    return (NULL);
  }
  if (n_rows != dimension1 * dimension2) {
    char s[1024];
    sprintf(s, "Unable to get matrix--number of rows (%" PRId64 ") doesn't correspond to given dimensions (%" PRId64 " x %" PRId64 ") (SDDS_GetMatrixFromColumn)", n_rows, dimension1, dimension2);
    SDDS_SetError(s);
    return (NULL);
  }
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0 || (type = SDDS_GetColumnType(SDDS_dataset, index)) < 0 || (size = SDDS_GetTypeSize(type)) <= 0) {
    SDDS_SetError("Unable to get matrix--column name is unrecognized (SDDS_GetMatrixFromColumn)");
    return (NULL);
  }
  if (!(column = SDDS_GetColumn(SDDS_dataset, column_name))) {
    SDDS_SetError("Unable to get matrix (SDDS_GetMatrixFromColumn)");
    return (NULL);
  }
  if (!(data = SDDS_AllocateMatrix(size, dimension1, dimension2))) {
    SDDS_SetError("Unable to allocate matrix (SDDS_GetMatrixFromColumn)");
    return (NULL);
  }
  if (mode & SDDS_ROW_MAJOR_DATA || !(mode & SDDS_COLUMN_MAJOR_DATA)) {
    for (i = 0; i < dimension1; i++)
      memcpy(data[i], (char *)column + i * dimension2 * size, dimension2 * size);
  } else {
    for (i = 0; i < dimension1; i++) {
      for (j = 0; j < dimension2; j++) {
        memcpy((char *)data[i] + size * j, (char *)column + (j * dimension1 + i) * size, size);
      }
    }
  }

  free(column);
  return (data);
}

/**
 * @brief Extracts a matrix of doubles from a specified column in the current data table of an SDDS dataset.
 *
 * This function retrieves the data from the specified column as `double` values and organizes it into a matrix with the given dimensions.
 * The data is arranged in either row-major or column-major order based on the `mode` parameter. The function allocates memory for
 * the matrix, which should be freed by the caller.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param column_name A null-terminated string specifying the name of the column from which to extract the matrix.
 * @param dimension1 The number of rows in the resulting matrix.
 * @param dimension2 The number of columns in the resulting matrix.
 * @param mode Specifies the data layout in the matrix. Use `SDDS_ROW_MAJOR_DATA` for row-major order or `SDDS_COLUMN_MAJOR_DATA`
 *             for column-major order.
 *
 * @return On success, returns a pointer to the allocated matrix containing `double` values. The matrix is an array of pointers,
 *         where each pointer refers to a row (for row-major) or a column (for column-major) in the matrix. On failure, returns `NULL`
 *         and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, column not found, dimension mismatch, memory allocation failure).
 * @retval Non-NULL Pointer to the allocated matrix containing `double` values.
 *
 * @note The caller is responsible for freeing the allocated matrix and its contents.
 *
 * @sa SDDS_GetMatrixFromColumn, SDDS_GetDoubleMatrixFromRow, SDDS_AllocateMatrix
 */
void *SDDS_GetDoubleMatrixFromColumn(SDDS_DATASET *SDDS_dataset, char *column_name, int64_t dimension1, int64_t dimension2, int32_t mode) {
  int32_t size, index;
  int64_t n_rows, i, j;
  void **data, *column;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetDoubleMatrixFromColumn"))
    return (NULL);
  if (!column_name) {
    SDDS_SetError("Unable to get matrix--column name is NULL (SDDS_GetDoubleMatrixFromColumn)");
    return (NULL);
  }
  if ((n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get matrix--no rows selected (SDDS_GetDoubleMatrixFromColumn)");
    return (NULL);
  }
  if (n_rows != dimension1 * dimension2) {
    char s[1024];
    sprintf(s, "Unable to get matrix--number of rows (%" PRId64 ") doesn't correspond to given dimensions (%" PRId64 " x %" PRId64 ") (SDDS_GetDoubleMatrixFromColumn)", n_rows, dimension1, dimension2);
    SDDS_SetError(s);
    return (NULL);
  }
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get matrix--column name is unrecognized (SDDS_GetDoubleMatrixFromColumn)");
    return (NULL);
  }
  if (!(column = SDDS_GetColumnInDoubles(SDDS_dataset, column_name))) {
    SDDS_SetError("Unable to get matrix (SDDS_GetDoubleMatrixFromColumn)");
    return (NULL);
  }
  size = sizeof(double);
  if (!(data = SDDS_AllocateMatrix(size, dimension1, dimension2))) {
    SDDS_SetError("Unable to allocate matrix (SDDS_GetDoubleMatrixFromColumn)");
    return (NULL);
  }
  if (mode & SDDS_ROW_MAJOR_DATA || !(mode & SDDS_COLUMN_MAJOR_DATA)) {
    for (i = 0; i < dimension1; i++)
      memcpy(data[i], (char *)column + i * dimension2 * size, dimension2 * size);
  } else {
    for (i = 0; i < dimension1; i++) {
      for (j = 0; j < dimension2; j++) {
        memcpy((char *)data[i] + size * j, (char *)column + (j * dimension1 + i) * size, size);
      }
    }
  }

  free(column);
  return (data);
}

/**
 * @brief Sets the rows of interest in an SDDS dataset based on various selection criteria.
 *
 * This function marks rows in the provided SDDS dataset as "of interest" based on the specified selection criteria.
 * It supports multiple selection modes, allowing users to specify rows by an array of names, a single string containing
 * multiple names, a variadic list of names, or by matching a specific string with logical operations.
 *
 * **Calling Modes:**
 * - **SDDS_NAME_ARRAY**: Specify an array of names.
 *   ```c
 *   SDDS_SetRowsOfInterest(&SDDS_dataset, selection_column, SDDS_NAME_ARRAY, int32_t n_entries, char **name);
 *   ```
 * - **SDDS_NAMES_STRING**: Provide a single string containing multiple names separated by delimiters.
 *   ```c
 *   SDDS_SetRowsOfInterest(&SDDS_dataset, selection_column, SDDS_NAMES_STRING, char *names);
 *   ```
 * - **SDDS_NAME_STRINGS**: Pass multiple name strings, terminated by `NULL`.
 *   ```c
 *   SDDS_SetRowsOfInterest(&SDDS_dataset, selection_column, SDDS_NAME_STRINGS, char *name1, char *name2, ..., NULL);
 *   ```
 * - **SDDS_MATCH_STRING**: Match rows based on a single string and logical operations.
 *   ```c
 *   SDDS_SetRowsOfInterest(&SDDS_dataset, selection_column, SDDS_MATCH_STRING, char *name, int32_t logic_mode);
 *   ```
 *
 * Additionally, each of these modes has a case-insensitive variant prefixed with `SDDS_CI_` (e.g., `SDDS_CI_NAME_ARRAY`).
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param selection_column A null-terminated string specifying the name of the column used for row selection.
 *                         This column must be of string type.
 * @param mode An integer representing the selection mode. Supported modes include:
 *             - `SDDS_NAME_ARRAY`
 *             - `SDDS_NAMES_STRING`
 *             - `SDDS_NAME_STRINGS`
 *             - `SDDS_MATCH_STRING`
 *             - `SDDS_CI_NAME_ARRAY`
 *             - `SDDS_CI_NAMES_STRING`
 *             - `SDDS_CI_NAME_STRINGS`
 *             - `SDDS_CI_MATCH_STRING`
 * @param ... Variable arguments corresponding to the selected mode:
 *            - **SDDS_NAME_ARRAY** and **SDDS_CI_NAME_ARRAY**:
 *              - `int32_t n_entries`: Number of names.
 *              - `char **name`: Array of name strings.
 *            - **SDDS_NAMES_STRING** and **SDDS_CI_NAMES_STRING**:
 *              - `char *names`: Single string containing multiple names separated by delimiters.
 *            - **SDDS_NAME_STRINGS** and **SDDS_CI_NAME_STRINGS**:
 *              - `char *name1, char *name2, ..., NULL`: Multiple name strings terminated by `NULL`.
 *            - **SDDS_MATCH_STRING** and **SDDS_CI_MATCH_STRING**:
 *              - `char *name`: String to match.
 *              - `int32_t logic_mode`: Logical operation mode.
 *
 * @return On success, returns the number of rows marked as "of interest". On failure, returns `-1` and sets an appropriate error message.
 *
 * @retval -1 Indicates that an error occurred (e.g., invalid dataset, unrecognized selection column, memory allocation failure, unknown mode).
 * @retval Non-negative Integer representing the count of rows marked as "of interest".
 *
 * @note
 * - The caller must ensure that the `selection_column` exists and is of string type in the dataset.
 * - For modes that allocate memory internally (e.g., `SDDS_NAMES_STRING`), the function handles memory management internally.
 *
 * @sa SDDS_MatchRowsOfInterest, SDDS_FilterRowsOfInterest, SDDS_DeleteUnsetRows
 */
int64_t SDDS_SetRowsOfInterest(SDDS_DATASET *SDDS_dataset, char *selection_column, int32_t mode, ...)
/* This routine has 4 calling modes:
 * SDDS_SetRowsOfInterest(&SDDS_dataset, selection_column, SDDS_NAME_ARRAY, int32_t n_entries, char **name)
 * SDDS_SetRowsOfInterest(&SDDS_dataset, selection_column, SDDS_NAMES_STRING, char *names)
 * SDDS_SetRowsOfInterest(&SDDS_dataset, selection_column, SDDS_NAME_STRINGS, char *name1, char *name2, ..., NULL )
 * SDDS_SetRowsOfInterest(&SDDS_dataset, selection_column, SDDS_MATCH_STRING, char *name, int32_t logic_mode)
 */
{
  va_list argptr;
  int32_t retval, type, index, n_names;
  int64_t i, j;
  char **name, *string, *match_string, *ptr;
  int32_t local_memory; /* (0,1,2) --> (none, pointer array, pointer array + strings) locally allocated */
  char buffer[SDDS_MAXLINE];
  int32_t logic, caseSensitive;
  int64_t count;

  name = NULL;
  n_names = local_memory = logic = 0;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetRowsOfInterest"))
    return (-1);
  va_start(argptr, mode);
  retval = 1;
  caseSensitive = 1;
  match_string = NULL;
  switch (mode) {
  case SDDS_CI_NAME_ARRAY:
    caseSensitive = 0;
  case SDDS_NAME_ARRAY:
    local_memory = 0;
    n_names = va_arg(argptr, int32_t);
    name = va_arg(argptr, char **);
    break;
  case SDDS_CI_NAMES_STRING:
    caseSensitive = 0;
  case SDDS_NAMES_STRING:
    local_memory = 2;
    n_names = 0;
    name = NULL;
    ptr = va_arg(argptr, char *);
    SDDS_CopyString(&string, ptr);
    while (SDDS_GetToken(string, buffer, SDDS_MAXLINE) > 0) {
      if (!(name = SDDS_Realloc(name, sizeof(*name) * (n_names + 1))) || !SDDS_CopyString(name + n_names, buffer)) {
        SDDS_SetError("Unable to process row selection--memory allocation failure (SDDS_SetRowsOfInterest)");
        retval = -1;
        break;
      }
      n_names++;
    }
    free(string);
    break;
  case SDDS_CI_NAME_STRINGS:
    caseSensitive = 0;
  case SDDS_NAME_STRINGS:
    local_memory = 1;
    n_names = 0;
    name = NULL;
    while ((string = va_arg(argptr, char *))) {
      if (!(name = SDDS_Realloc(name, sizeof(*name) * (n_names + 1)))) {
        SDDS_SetError("Unable to process row selection--memory allocation failure (SDDS_SetRowsOfInterest)");
        retval = -1;
        break;
      }
      name[n_names++] = string;
    }
    break;
  case SDDS_CI_MATCH_STRING:
    caseSensitive = 0;
  case SDDS_MATCH_STRING:
    local_memory = 0;
    n_names = 1;
    if ((string = va_arg(argptr, char *)))
      match_string = expand_ranges(string);
    logic = va_arg(argptr, int32_t);
    if (logic & SDDS_NOCASE_COMPARE)
      caseSensitive = 0;
    break;
  default:
    SDDS_SetError("Unable to process row selection--unknown mode (SDDS_SetRowsOfInterest)");
    retval = -1;
    break;
  }

  va_end(argptr);
  if (retval != 1)
    return (-1);

  if (mode != SDDS_MATCH_STRING && mode != SDDS_CI_MATCH_STRING) {
    int (*stringCompare)(const char *s1, const char *s2);
    if (caseSensitive)
      stringCompare = strcmp;
    else
      stringCompare = strcmp_ci;
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, selection_column)) < 0) {
      SDDS_SetError("Unable to process row selection--unrecognized selection column name (SDDS_SetRowsOfInterest)");
      return (-1);
    }
    if ((type = SDDS_GetColumnType(SDDS_dataset, index)) != SDDS_STRING) {
      SDDS_SetError("Unable to select rows--selection column is not string type (SDDS_SetRowsOfInterest)");
      return (-1);
    }
    if (n_names == 0) {
      SDDS_SetError("Unable to process row selection--no names in call (SDDS_SetRowsOfInterest)");
      return (-1);
    }
    for (j = 0; j < n_names; j++) {
      for (i = 0; i < SDDS_dataset->n_rows; i++) {
        if ((*stringCompare)(*((char **)SDDS_dataset->data[index] + i), name[j]) == 0)
          SDDS_dataset->row_flag[i] = 1;
      }
    }
  } else {
    if (selection_column) {
      int (*wildMatch)(char *string, char *template);
      if (caseSensitive)
        wildMatch = wild_match;
      else
        wildMatch = wild_match_ci;
      if (!match_string) {
        SDDS_SetError("Unable to select rows--no matching string given (SDDS_SetRowsOfInterest)");
        return (-1);
      }
      if ((index = SDDS_GetColumnIndex(SDDS_dataset, selection_column)) < 0) {
        free(match_string);
        SDDS_SetError("Unable to process row selection--unrecognized selection column name (SDDS_SetRowsOfInterest)");
        return (-1);
      }
      if ((type = SDDS_GetColumnType(SDDS_dataset, index)) != SDDS_STRING) {
        free(match_string);
        SDDS_SetError("Unable to select rows--selection column is not string type (SDDS_SetRowsOfInterest)");
        return (-1);
      }
      for (i = 0; i < SDDS_dataset->n_rows; i++)
        SDDS_dataset->row_flag[i] = SDDS_Logic(SDDS_dataset->row_flag[i], (*wildMatch)(*((char **)SDDS_dataset->data[index] + i), match_string), logic);
    } else {
      for (i = 0; i < SDDS_dataset->n_rows; i++)
        SDDS_dataset->row_flag[i] = SDDS_Logic(SDDS_dataset->row_flag[i], 0, logic & ~(SDDS_AND | SDDS_OR));
    }
  }

  if (local_memory == 2) {
    for (i = 0; i < n_names; i++)
      free(name[i]);
  }
  if (match_string)
    free(match_string);
  if (local_memory >= 1)
    free(name);

  for (i = count = 0; i < SDDS_dataset->n_rows; i++)
    if (SDDS_dataset->row_flag[i])
      count++;
  return (count);
}

/**
 * @brief Matches and marks rows of interest in an SDDS dataset based on label matching.
 *
 * This function marks rows in the provided SDDS dataset as "of interest" by matching labels in a specified column against a target label.
 * It supports both direct and indirect matching, as well as case-sensitive and case-insensitive comparisons, based on the provided logic flags.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param selection_column A null-terminated string specifying the name of the column used for label matching.
 *                         This column must be of string or character type.
 * @param label_to_match A null-terminated string specifying the label to match against the entries in the selection column.
 *                       If `logic` includes `SDDS_INDIRECT_MATCH`, this parameter is treated as the name of another column used for indirect matching.
 * @param logic An integer representing logical operation flags. Supported flags include:
 *              - `SDDS_NOCASE_COMPARE`: Perform case-insensitive comparison.
 *              - `SDDS_INDIRECT_MATCH`: Use indirect matching via another column.
 *
 * @return On success, returns the number of rows marked as "of interest". On failure, returns `-1` and sets an appropriate error message.
 *
 * @retval -1 Indicates that an error occurred (e.g., invalid dataset, unrecognized selection column, type mismatch, unrecognized indirect column).
 * @retval Non-negative Integer representing the count of rows marked as "of interest".
 *
 * @note
 * - The selection column must exist and be of string or character type.
 * - If using indirect matching (`SDDS_INDIRECT_MATCH`), the indirect column must exist and be of the same type as the selection column.
 *
 * @sa SDDS_SetRowsOfInterest, SDDS_FilterRowsOfInterest, SDDS_DeleteUnsetRows
 */
int64_t SDDS_MatchRowsOfInterest(SDDS_DATASET *SDDS_dataset, char *selection_column, char *label_to_match, int32_t logic) {
  int32_t match, type, index, indirect_index;
  int64_t i, count;
  char *match_string;
#ifndef tolower
#  if !defined(_MINGW)
  int tolower(int c);
#  endif
#endif

  index = type = indirect_index = 0;

  match_string = NULL;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MatchRowsOfInterest"))
    return (-1);
  if (selection_column) {
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, selection_column)) < 0) {
      SDDS_SetError("Unable to select rows--column name is unrecognized (SDDS_MatchRowsOfInterest)");
      return (-1);
    }
    if ((type = SDDS_GetColumnType(SDDS_dataset, index)) != SDDS_STRING && type != SDDS_CHARACTER) {
      SDDS_SetError("Unable to select rows--selection column is not a string (SDDS_MatchRowsOfInterest)");
      return (-1);
    }
    if (!label_to_match) {
      SDDS_SetError("Unable to select rows--selection label is NULL (SDDS_MatchRowsOfInterest)");
      return (-1);
    }
    if (!(logic & SDDS_INDIRECT_MATCH))
      match_string = expand_ranges(label_to_match);
    else {
      if ((indirect_index = SDDS_GetColumnIndex(SDDS_dataset, label_to_match)) < 0) {
        SDDS_SetError("Unable to select rows--indirect column name is unrecognized (SDDS_MatchRowsOfInterest)");
        return (-1);
      }
      if (SDDS_GetColumnType(SDDS_dataset, indirect_index) != type) {
        SDDS_SetError("Unable to select rows--indirect column is not same type as main column (SDDS_MatchRowsOfInterest)");
        return (-1);
      }
    }
  }
  if (type == SDDS_STRING) {
    int (*stringCompare)(const char *s, const char *t);
    int (*wildMatch)(char *s, char *t);
    if (logic & SDDS_NOCASE_COMPARE) {
      stringCompare = strcmp_ci;
      wildMatch = wild_match_ci;
    } else {
      stringCompare = strcmp;
      wildMatch = wild_match;
    }
    for (i = count = 0; i < SDDS_dataset->n_rows; i++) {
      if (selection_column)
        match = SDDS_Logic(SDDS_dataset->row_flag[i], (logic & SDDS_INDIRECT_MATCH ? (*stringCompare)(*((char **)SDDS_dataset->data[index] + i), *((char **)SDDS_dataset->data[indirect_index] + i)) == 0 : (*wildMatch)(*((char **)SDDS_dataset->data[index] + i), match_string)), logic);
      else
        match = SDDS_Logic(SDDS_dataset->row_flag[i], 0, logic & ~(SDDS_AND | SDDS_OR));
      if ((SDDS_dataset->row_flag[i] = match))
        count++;
    }
  } else {
    char c1, c2;
    c2 = 0;
    if (!(logic & SDDS_INDIRECT_MATCH))
      c2 = *match_string;
    if (logic & SDDS_NOCASE_COMPARE) {
      c2 = tolower(c2);
      for (i = count = 0; i < SDDS_dataset->n_rows; i++) {
        c1 = tolower(*((char *)SDDS_dataset->data[index] + i));
        if (selection_column)
          match = SDDS_Logic(SDDS_dataset->row_flag[i], logic & SDDS_INDIRECT_MATCH ? c1 == tolower(*((char *)SDDS_dataset->data[indirect_index] + i)) : c1 == c2, logic);
        else
          match = SDDS_Logic(SDDS_dataset->row_flag[i], 0, logic & ~(SDDS_AND | SDDS_OR));
        if ((SDDS_dataset->row_flag[i] = match))
          count++;
      }
    } else {
      for (i = count = 0; i < SDDS_dataset->n_rows; i++) {
        c1 = *((char *)SDDS_dataset->data[index] + i);
        if (selection_column)
          match = SDDS_Logic(SDDS_dataset->row_flag[i], logic & SDDS_INDIRECT_MATCH ? c1 == *((char *)SDDS_dataset->data[indirect_index] + i) : c1 == c2, logic);
        else
          match = SDDS_Logic(SDDS_dataset->row_flag[i], 0, logic & ~(SDDS_AND | SDDS_OR));
        if ((SDDS_dataset->row_flag[i] = match))
          count++;
      }
    }
  }
  if (match_string)
    free(match_string);
  return (count);
}

/**
 * @brief Filters rows of interest in an SDDS dataset based on numeric ranges in a specified column.
 *
 * This function marks rows in the provided SDDS dataset as "of interest" if the values in the specified filter column fall
 * within the defined numeric range (`lower_limit` to `upper_limit`). Logical operations specified by the `logic` parameter
 * determine how the filtering interacts with existing row flags.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param filter_column A null-terminated string specifying the name of the column used for numeric filtering.
 *                      This column must be of a numeric type.
 * @param lower_limit The lower bound of the numeric range. Rows with values below this limit are excluded.
 * @param upper_limit The upper bound of the numeric range. Rows with values above this limit are excluded.
 * @param logic An integer representing logical operation flags. Supported flags include:
 *              - `SDDS_NEGATE_PREVIOUS`: Invert the previous row flag.
 *              - `SDDS_NEGATE_MATCH`: Invert the match result.
 *              - `SDDS_AND`: Combine with existing row flags using logical AND.
 *              - `SDDS_OR`: Combine with existing row flags using logical OR.
 *              - `SDDS_NEGATE_EXPRESSION`: Invert the entire logical expression.
 *
 * @return On success, returns the number of rows marked as "of interest" after filtering. On failure, returns `-1` and sets an appropriate error message.
 *
 * @retval -1 Indicates that an error occurred (e.g., invalid dataset, unrecognized filter column, non-numeric filter column).
 * @retval Non-negative Integer representing the count of rows marked as "of interest".
 *
 * @note
 * - The filter column must exist and be of a numeric type (e.g., `SDDS_SHORT`, `SDDS_USHORT`, `SDDS_LONG`, `SDDS_ULONG`, `SDDS_LONG64`, `SDDS_ULONG64`, `SDDS_FLOAT`, `SDDS_DOUBLE`, `SDDS_LONGDOUBLE`).
 * - Logical flags determine how the filtering interacts with existing row flags. Multiple flags can be combined using bitwise OR.
 *
 * @sa SDDS_SetRowsOfInterest, SDDS_MatchRowsOfInterest, SDDS_DeleteUnsetRows
 */
int64_t SDDS_FilterRowsOfInterest(SDDS_DATASET *SDDS_dataset, char *filter_column, double lower_limit, double upper_limit, int32_t logic) {
  int32_t accept, type, index;
  int64_t i, count;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_FilterRowsOfInterest"))
    return (-1);
  if (!filter_column) {
    SDDS_SetError("Unable to filter rows--filter column name not given (SDDS_FilterRowsOfInterest)");
    return (-1);
  }
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, filter_column)) < 0) {
    SDDS_SetError("Unable to filter rows--column name is unrecognized (SDDS_FilterRowsOfInterest)");
    return (-1);
  }
  switch (type = SDDS_GetColumnType(SDDS_dataset, index)) {
  case SDDS_SHORT:
  case SDDS_USHORT:
  case SDDS_LONG:
  case SDDS_ULONG:
  case SDDS_LONG64:
  case SDDS_ULONG64:
  case SDDS_FLOAT:
  case SDDS_DOUBLE:
  case SDDS_LONGDOUBLE:
    break;
  default:
    SDDS_SetError("Unable to filter rows--filter column is not a numeric type (SDDS_FilterRowsOfInterest)");
    return (-1);
  }
  for (i = count = 0; i < SDDS_dataset->n_rows; i++) {
    if (logic & SDDS_NEGATE_PREVIOUS)
      SDDS_dataset->row_flag[i] = !SDDS_dataset->row_flag[i];
    accept = SDDS_ItemInsideWindow(SDDS_dataset->data[index], i, type, lower_limit, upper_limit);
    if (logic & SDDS_NEGATE_MATCH)
      accept = !accept;
    if (logic & SDDS_AND)
      accept = accept && SDDS_dataset->row_flag[i];
    else if (logic & SDDS_OR)
      accept = accept || SDDS_dataset->row_flag[i];
    if (logic & SDDS_NEGATE_EXPRESSION)
      accept = !accept;
    if ((SDDS_dataset->row_flag[i] = accept))
      count++;
  }
  return (count);
}

/**
 * @brief Filters rows of interest in an SDDS dataset based on numeric scanning of a specified column.
 *
 * This function marks rows in the provided SDDS dataset as "of interest" based on whether the entries in the specified filter
 * column can be interpreted as valid numbers. It supports inversion of the filtering criterion through the `mode` parameter.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param filter_column A null-terminated string specifying the name of the column used for numeric scanning.
 *                      This column must not be of string type.
 * @param mode An unsigned integer representing mode flags. Supported flags include:
 *             - `NUMSCANFILTER_INVERT`: Invert the filtering criterion (select rows where the entry is not a number).
 *
 * @return On success, returns the number of rows marked as "of interest" after filtering. On failure, returns `-1` and sets an appropriate error message.
 *
 * @retval -1 Indicates that an error occurred (e.g., invalid dataset, unrecognized filter column, filter column is of string type).
 * @retval Non-negative Integer representing the count of rows marked as "of interest".
 *
 * @note
 * - The filter column must exist and must not be of string type.
 * - The function uses `tokenIsNumber` to determine if an entry is a valid number.
 *
 * @sa SDDS_SetRowsOfInterest, SDDS_MatchRowsOfInterest, SDDS_DeleteUnsetRows
 */
int64_t SDDS_FilterRowsByNumScan(SDDS_DATASET *SDDS_dataset, char *filter_column, uint32_t mode) {
  int32_t accept, index;
  int64_t i, count;
  short invert;
  char *ptr;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_FilterRowsByNumScan"))
    return (-1);
  if (!filter_column) {
    SDDS_SetError("Unable to filter rows--filter column name not given (SDDS_FilterRowsByNumScan)");
    return (-1);
  }
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, filter_column)) < 0) {
    SDDS_SetError("Unable to filter rows--column name is unrecognized (SDDS_FilterRowsByNumScan)");
    return (-1);
  }
  switch (SDDS_GetColumnType(SDDS_dataset, index)) {
  case SDDS_SHORT:
  case SDDS_USHORT:
  case SDDS_LONG:
  case SDDS_ULONG:
  case SDDS_LONG64:
  case SDDS_ULONG64:
  case SDDS_FLOAT:
  case SDDS_DOUBLE:
  case SDDS_LONGDOUBLE:
  case SDDS_CHARACTER:
    SDDS_SetError("Unable to filter rows--filter column is not string type (SDDS_FilterRowsByNumScan)");
    return (-1);
  default:
    break;
  }
  invert = mode & NUMSCANFILTER_INVERT ? 1 : 0;
  for (i = count = 0; i < SDDS_dataset->n_rows; i++) {
    ptr = ((char **)(SDDS_dataset->data[index]))[i];
    accept = !invert;
    if (!tokenIsNumber(ptr))
      accept = invert;
    if ((SDDS_dataset->row_flag[i] = accept))
      count++;
  }
  return (count);
}

/**
 * @brief Deletes rows from an SDDS dataset that are not marked as "of interest".
 *
 * This function removes all rows in the provided SDDS dataset that have not been flagged as "of interest" using prior selection functions.
 * It effectively compacts the dataset by retaining only the desired rows and updating the row count accordingly.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 *
 * @return On success, returns `1`. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that rows were successfully deleted.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, problem copying rows).
 *
 * @note
 * - This operation modifies the dataset in place by removing unset rows.
 * - It is recommended to perform row selection before calling this function to ensure that only desired rows are retained.
 *
 * @sa SDDS_SetRowsOfInterest, SDDS_MatchRowsOfInterest, SDDS_FilterRowsOfInterest
 */
int32_t SDDS_DeleteUnsetRows(SDDS_DATASET *SDDS_dataset) {
  int64_t i, j;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DeleteUnsetRows"))
    return (0);

  for (i = j = 0; i < SDDS_dataset->n_rows; i++) {
    if (SDDS_dataset->row_flag[i]) {
      if (i != j) {
        SDDS_dataset->row_flag[j] = SDDS_dataset->row_flag[i];
        if (!SDDS_TransferRow(SDDS_dataset, j, i)) {
          SDDS_SetError("Unable to delete unset rows--problem copying row (SDDS_DeleteUnsetRows)");
          return (0);
        }
      }
      j++;
    }
  }
  SDDS_dataset->n_rows = j;
  return (1);
}

/**
 * @brief Transfers data from a source row to a target row within an SDDS dataset.
 *
 * This function copies all column data from the specified source row to the target row in the provided SDDS dataset.
 * It handles both string and non-string data types appropriately, ensuring that memory is managed correctly for string entries.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param target The index of the target row where data will be copied to.
 * @param source The index of the source row from which data will be copied.
 *
 * @return On success, returns `1`. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that the row transfer was successful.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, out-of-range indices, memory allocation failure, string copy failure).
 *
 * @note
 * - Both `target` and `source` must be valid row indices within the dataset.
 * - The function does not allocate or deallocate rows; it only copies data between existing rows.
 *
 * @sa SDDS_DeleteUnsetRows, SDDS_CopyColumn
 */
int32_t SDDS_TransferRow(SDDS_DATASET *SDDS_dataset, int64_t target, int64_t source) {
  int32_t size;
  int64_t i;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_TransferRow"))
    return (0);
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
    if (SDDS_dataset->layout.column_definition[i].type != SDDS_STRING) {
      size = SDDS_type_size[SDDS_dataset->layout.column_definition[i].type - 1];
      memcpy((char *)SDDS_dataset->data[i] + target * size, (char *)SDDS_dataset->data[i] + source * size, size);
    } else {
      if (((char ***)SDDS_dataset->data)[i][target])
        free(((char ***)SDDS_dataset->data)[i][target]);
      ((char ***)SDDS_dataset->data)[i][target] = NULL;
      if (!SDDS_CopyString(((char ***)SDDS_dataset->data)[i] + target, ((char ***)SDDS_dataset->data)[i][source]))
        return ((int32_t)0);
    }
  }
  return (1);
}

/**
 * @brief Deletes a specified column from an SDDS dataset.
 *
 * **Note:** This function is currently non-functional and will abort execution if called.
 *
 * This function is intended to remove a column identified by `column_name` from the provided SDDS dataset.
 * It handles the reordering of remaining columns and updates the dataset's layout accordingly. However, as indicated
 * by the current implementation, the function is not operational and will terminate the program when invoked.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param column_name A null-terminated string specifying the name of the column to be deleted.
 *
 * @return This function does not return as it aborts execution. If it were functional, it would return `1` on success
 *         and `0` on failure.
 *
 * @warning
 * - **Currently Non-Functional:** The function will terminate the program with an error message when called.
 *
 * @todo
 * - Implement the functionality to delete a column without aborting.
 *
 * @sa SDDS_DeleteUnsetColumns, SDDS_CopyColumn
 */
int32_t SDDS_DeleteColumn(SDDS_DATASET *SDDS_dataset, char *column_name) {
  int32_t index;
  int64_t i, j;

  SDDS_Bomb("SDDS_DeleteColumn is presently not functional.");

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DeleteColumn"))
    return (0);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to delete column--unrecognized column name (SDDS_DeleteColumn)");
    return (0);
  }
  for (i = index + 1; i < SDDS_dataset->layout.n_columns; i++) {
    if (!SDDS_CopyColumn(SDDS_dataset, i - 1, i)) {
      SDDS_SetError("Unable to delete column--error copying column (SDDS_DeleteColumn)");
      return (0);
    }
    for (j = 0; j < SDDS_dataset->n_of_interest; j++)
      if (SDDS_dataset->column_order[j] == index) {
        memcpy((char *)(SDDS_dataset->column_order + j), (char *)(SDDS_dataset->column_order + j + 1), sizeof(*SDDS_dataset->column_order) * (SDDS_dataset->n_of_interest - j - 1));
        SDDS_dataset->n_of_interest--;
      } else if (SDDS_dataset->column_order[j] > index)
        SDDS_dataset->column_order[j] -= 1;
  }
  if ((SDDS_dataset->layout.n_columns -= 1) == 0)
    SDDS_dataset->n_rows = 0;
  return (1);
}

/**
 * @brief Deletes all columns from an SDDS dataset that are not marked as "of interest".
 *
 * This function iterates through all columns in the provided SDDS dataset and removes those that have not been flagged as "of interest".
 * It ensures that only desired columns are retained, updating the dataset's layout and column order accordingly.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 *
 * @return On success, returns `1`. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that columns were successfully deleted.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, failure to delete a column).
 *
 * @note
 * - This operation modifies the dataset in place by removing unset columns.
 * - It is recommended to perform column selection before calling this function to ensure that only desired columns are retained.
 *
 * @sa SDDS_SetColumnsOfInterest, SDDS_DeleteColumn, SDDS_CopyColumn
 */
int32_t SDDS_DeleteUnsetColumns(SDDS_DATASET *SDDS_dataset) {
  int64_t i;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DeleteUnsetColumns"))
    return (0);
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
    if (!SDDS_dataset->column_flag[i]) {
      if (!SDDS_DeleteColumn(SDDS_dataset, SDDS_dataset->layout.column_definition[i].name))
        return (0);
      else
        i--;
    }
  return (1);
}

/**
 * @brief Copies data from a source column to a target column within an SDDS dataset.
 *
 * This function duplicates the data from the specified source column to the target column in the provided SDDS dataset.
 * It handles both string and non-string data types appropriately, ensuring that memory is managed correctly for string entries.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param target The index of the target column where data will be copied to.
 * @param source The index of the source column from which data will be copied.
 *
 * @return On success, returns `1`. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that the column copy was successful.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, out-of-range indices, memory allocation failure, string copy failure).
 *
 * @note
 * - Both `target` and `source` must be valid column indices within the dataset.
 * - The function does not handle the allocation of new columns; it assumes that the target column already exists.
 *
 * @sa SDDS_DeleteColumn, SDDS_DeleteUnsetColumns, SDDS_TransferRow
 */
int32_t SDDS_CopyColumn(SDDS_DATASET *SDDS_dataset, int32_t target, int32_t source) {
  COLUMN_DEFINITION *cd_target, *cd_source;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_CopyColumn"))
    return (0);
  if (target < 0 || source < 0 || target >= SDDS_dataset->layout.n_columns || source >= SDDS_dataset->layout.n_columns) {
    SDDS_SetError("Unable to copy column--target or source index out of range (SDDS_CopyColumn");
    return (0);
  }
  cd_target = SDDS_dataset->layout.column_definition + target;
  cd_source = SDDS_dataset->layout.column_definition + source;
  SDDS_dataset->column_flag[target] = SDDS_dataset->column_flag[source];
  if (SDDS_dataset->n_rows_allocated) {
    if (cd_target->type != cd_source->type) {
      if (!(SDDS_dataset->data[target] = SDDS_Realloc(SDDS_dataset->data[target], SDDS_type_size[cd_source->type - 1] * SDDS_dataset->n_rows_allocated))) {
        SDDS_SetError("Unable to copy column--memory allocation failure (SDDS_CopyColumn)");
        return (0);
      }
    }
    if (cd_source->type != SDDS_STRING)
      memcpy(SDDS_dataset->data[target], SDDS_dataset->data[source], SDDS_type_size[cd_source->type - 1] * SDDS_dataset->n_rows);
    else if (!SDDS_CopyStringArray(SDDS_dataset->data[target], SDDS_dataset->data[source], SDDS_dataset->n_rows)) {
      SDDS_SetError("Unable to copy column--string copy failure (SDDS_CopyColumn)");
      return (0);
    }
  }
  memcpy((char *)cd_target, (char *)cd_source, sizeof(*cd_target));
  return (1);
}

/**
 * @brief Deletes a specified parameter from an SDDS dataset.
 *
 * This function removes a parameter identified by `parameter_name` from the provided SDDS dataset.
 * It shifts all subsequent parameters up to fill the gap left by the deleted parameter and updates the
 * dataset's parameter count accordingly.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param parameter_name A null-terminated string specifying the name of the parameter to be deleted.
 *
 * @return On success, returns `1`. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that the parameter was successfully deleted.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, unrecognized parameter name, error copying parameters).
 *
 * @note
 * - This operation modifies the dataset by removing the specified parameter.
 * - It is recommended to ensure that the parameter to be deleted is not essential for further operations.
 *
 * @sa SDDS_CopyParameter, SDDS_DeleteUnsetParameters
 */
int32_t SDDS_DeleteParameter(SDDS_DATASET *SDDS_dataset, char *parameter_name) {
  int32_t i, index;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DeleteParameter"))
    return (0);
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    SDDS_SetError("Unable to delete parameter--unrecognized parameter name (SDDS_DeleteParameter)");
    return (0);
  }
  for (i = index + 1; i < SDDS_dataset->layout.n_parameters; i++) {
    if (!SDDS_CopyParameter(SDDS_dataset, i - 1, i)) {
      SDDS_SetError("Unable to delete parameter--error copying parameter (SDDS_DeleteParameter)");
      return (0);
    }
  }
  SDDS_dataset->layout.n_parameters -= 1;
  return (1);
}

/**
 * @brief Copies a parameter from a source index to a target index within an SDDS dataset.
 *
 * This function duplicates the parameter data from the source index to the target index in the provided SDDS dataset.
 * It handles both string and non-string data types appropriately, ensuring that memory is managed correctly for string entries.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param target The index of the target parameter where data will be copied to.
 * @param source The index of the source parameter from which data will be copied.
 *
 * @return On success, returns `1`. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that the parameter copy was successful.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, out-of-range indices, memory allocation failure, string copy failure).
 *
 * @note
 * - Both `target` and `source` must be valid parameter indices within the dataset.
 * - The function assumes that the target parameter already exists and is intended to be overwritten.
 *
 * @sa SDDS_DeleteParameter, SDDS_CopyArray
 */
int32_t SDDS_CopyParameter(SDDS_DATASET *SDDS_dataset, int32_t target, int32_t source) {
  PARAMETER_DEFINITION *cd_target, *cd_source;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_CopyParameter"))
    return (0);
  if (target < 0 || source < 0 || target >= SDDS_dataset->layout.n_parameters || source >= SDDS_dataset->layout.n_parameters) {
    SDDS_SetError("Unable to copy parameter--target or source index out of range (SDDS_CopyParameter");
    return (0);
  }
  cd_target = SDDS_dataset->layout.parameter_definition + target;
  cd_source = SDDS_dataset->layout.parameter_definition + source;
  if (SDDS_dataset->parameter) {
    if (cd_target->type != cd_source->type) {
      if (!(SDDS_dataset->parameter[target] = SDDS_Realloc(SDDS_dataset->data[target], SDDS_type_size[cd_source->type - 1]))) {
        SDDS_SetError("Unable to copy parameter--memory allocation failure (SDDS_CopyParameter)");
        return (0);
      }
    }
    if (cd_source->type != SDDS_STRING)
      memcpy(SDDS_dataset->parameter[target], SDDS_dataset->parameter[source], SDDS_type_size[cd_source->type - 1]);
    else if (!SDDS_CopyStringArray(SDDS_dataset->parameter[target], SDDS_dataset->parameter[source], 1)) {
      SDDS_SetError("Unable to copy parameter--string copy failure (SDDS_CopyParameter)");
      return (0);
    }
  }
  memcpy((char *)cd_target, (char *)cd_source, sizeof(*cd_target));
  return (1);
}

/**
 * @brief Checks whether a data item is within a specified numeric window.
 *
 * This function determines if the data item at the given index within the data array falls within the range defined by
 * `lower_limit` and `upper_limit`. It handles various numeric data types and ensures that the value is neither NaN nor infinity.
 *
 * @param data Pointer to the data array.
 * @param index The index of the item within the data array to be checked.
 * @param type The data type of the item. Supported types include:
 *             - `SDDS_SHORT`
 *             - `SDDS_USHORT`
 *             - `SDDS_LONG`
 *             - `SDDS_ULONG`
 *             - `SDDS_LONG64`
 *             - `SDDS_ULONG64`
 *             - `SDDS_FLOAT`
 *             - `SDDS_DOUBLE`
 *             - `SDDS_LONGDOUBLE`
 * @param lower_limit The lower bound of the numeric window.
 * @param upper_limit The upper bound of the numeric window.
 *
 * @return Returns `1` if the item is within the window and valid, otherwise returns `0`.
 *
 * @retval 1 Indicates that the item is within the specified numeric window and is a valid number.
 * @retval 0 Indicates that the item is outside the specified window, is NaN, is infinite, or the data type is non-numeric.
 *
 * @note
 * - The function sets an error message if the data type is non-numeric.
 * - It is essential to ensure that the `data` array is properly initialized and contains valid data before calling this function.
 *
 * @sa SDDS_FilterRowsOfInterest, SDDS_GetParameterAsDouble
 */
int32_t SDDS_ItemInsideWindow(void *data, int64_t index, int32_t type, double lower_limit, double upper_limit) {
  short short_val;
  unsigned short ushort_val;
  int32_t long_val;
  uint32_t ulong_val;
  int64_t long64_val;
  uint64_t ulong64_val;
  long double ldouble_val;
  double double_val;
  float float_val;

  switch (type) {
  case SDDS_SHORT:
    if ((short_val = *((short *)data + index)) < lower_limit || short_val > upper_limit)
      return (0);
    return (1);
  case SDDS_USHORT:
    if ((ushort_val = *((unsigned short *)data + index)) < lower_limit || ushort_val > upper_limit)
      return (0);
    return (1);
  case SDDS_LONG:
    if ((long_val = *((int32_t *)data + index)) < lower_limit || long_val > upper_limit)
      return (0);
    return (1);
  case SDDS_ULONG:
    if ((ulong_val = *((uint32_t *)data + index)) < lower_limit || ulong_val > upper_limit)
      return (0);
    return (1);
  case SDDS_LONG64:
    if ((long64_val = *((int64_t *)data + index)) < lower_limit || long64_val > upper_limit)
      return (0);
    return (1);
  case SDDS_ULONG64:
    if ((ulong64_val = *((uint64_t *)data + index)) < lower_limit || ulong64_val > upper_limit)
      return (0);
    return (1);
  case SDDS_FLOAT:
    if ((float_val = *((float *)data + index)) < lower_limit || float_val > upper_limit)
      return (0);
    if (isnan(float_val) || isinf(float_val))
      return 0;
    return (1);
  case SDDS_DOUBLE:
    if ((double_val = *((double *)data + index)) < lower_limit || double_val > upper_limit)
      return 0;
    if (isnan(double_val) || isinf(double_val))
      return 0;
    return (1);
  case SDDS_LONGDOUBLE:
    if ((ldouble_val = *((long double *)data + index)) < lower_limit || ldouble_val > upper_limit)
      return 0;
    if (isnan(ldouble_val) || isinf(ldouble_val))
      return 0;
    return (1);
  default:
    SDDS_SetError("Unable to complete window check--item type is non-numeric (SDDS_ItemInsideWindow)");
    return (0);
  }
}

/**
 * @brief Applies logical operations to determine the new state of a row flag based on previous and current match conditions.
 *
 * This function evaluates logical conditions between a previous flag (`previous`) and a current match flag (`match`) based on the
 * provided `logic` flags. It supports various logical operations such as AND, OR, negation of previous flags, and negation of match results.
 *
 * @param previous The previous state of the row flag (typically `0` or `1`).
 * @param match The current match result to be combined with the previous flag.
 * @param logic An unsigned integer representing logical operation flags. Supported flags include:
 *              - `SDDS_0_PREVIOUS`: Set the previous flag to `0`.
 *              - `SDDS_1_PREVIOUS`: Set the previous flag to `1`.
 *              - `SDDS_NEGATE_PREVIOUS`: Negate the previous flag.
 *              - `SDDS_NEGATE_MATCH`: Negate the current match result.
 *              - `SDDS_AND`: Perform a logical AND between the previous flag and the match result.
 *              - `SDDS_OR`: Perform a logical OR between the previous flag and the match result.
 *              - `SDDS_NEGATE_EXPRESSION`: Negate the final logical expression result.
 *
 * @return Returns the result of the logical operation as an integer (`0` or `1`).
 *
 * @retval 1 Indicates that the final logical condition evaluates to true.
 * @retval 0 Indicates that the final logical condition evaluates to false.
 *
 * @note
 * - Multiple logic flags can be combined using bitwise OR to perform complex logical operations.
 * - The order of operations follows the precedence defined within the function implementation.
 *
 * @sa SDDS_SetRowsOfInterest, SDDS_MatchRowsOfInterest
 */
int32_t SDDS_Logic(int32_t previous, int32_t match, uint32_t logic) {
  if (logic & SDDS_0_PREVIOUS)
    previous = 0;
  else if (logic & SDDS_1_PREVIOUS)
    previous = 1;
  if (logic & SDDS_NEGATE_PREVIOUS)
    previous = !previous;
  if (logic & SDDS_NEGATE_MATCH)
    match = !match;
  if (logic & SDDS_AND)
    match = match && previous;
  else if (logic & SDDS_OR)
    match = match || previous;
  else
    match = previous;
  if (logic & SDDS_NEGATE_EXPRESSION)
    match = !match;
  return (match);
}

/**
 * @brief Retrieves an array from the current data table of an SDDS dataset.
 *
 * This function returns a pointer to a `SDDS_ARRAY` structure containing the data and other information about a specified array
 * within the current data table of an SDDS dataset. The function can either populate a provided `SDDS_ARRAY` structure or allocate
 * a new one if `memory` is `NULL`.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param array_name A null-terminated string specifying the name of the SDDS array to retrieve.
 * @param memory Optional pointer to an existing `SDDS_ARRAY` structure where the array information will be stored. If `NULL`,
 *               a new `SDDS_ARRAY` structure is allocated and returned.
 *
 * @return On success, returns a pointer to a `SDDS_ARRAY` structure containing the array data and metadata. If `memory` is not `NULL`,
 *         the function populates the provided structure. On failure, returns `NULL` and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, unrecognized array name, memory allocation failure).
 * @retval Non-NULL Pointer to a `SDDS_ARRAY` structure containing the array data and metadata.
 *
 * @note
 * - The caller is responsible for freeing the allocated memory for the `SDDS_ARRAY` structure if `memory` is `NULL`.
 * - The `definition` field in the returned structure points to the internal copy of the array definition.
 *
 * @sa SDDS_GetArrayInDoubles, SDDS_GetArrayInString, SDDS_GetArrayInLong
 */
SDDS_ARRAY *SDDS_GetArray(SDDS_DATASET *SDDS_dataset, char *array_name, SDDS_ARRAY *memory) {
  int32_t index, type, size;
  SDDS_ARRAY *copy, *original;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetArray"))
    return (NULL);
  if (!array_name) {
    SDDS_SetError("Unable to get array--array name pointer is NULL (SDDS_GetArray)");
    return (NULL);
  }
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
    SDDS_SetError("Unable to get array--array name is unrecognized (SDDS_GetArray)");
    return (NULL);
  }
  if (memory)
    copy = memory;
  else if (!(copy = (SDDS_ARRAY *)calloc(1, sizeof(*copy)))) {
    SDDS_SetError("Unable to get array--allocation failure (SDDS_GetArray)");
    return (NULL);
  }
  original = SDDS_dataset->array + index;
  if (copy->definition && !SDDS_FreeArrayDefinition(copy->definition)) {
    SDDS_SetError("Unable to get array--array definition corrupted (SDDS_GetArray)");
    return (NULL);
  }
  if (!SDDS_CopyArrayDefinition(&copy->definition, original->definition)) {
    SDDS_SetError("Unable to get array--array definition missing (SDDS_GetArray)");
    return (NULL);
  }
  type = copy->definition->type;
  size = SDDS_type_size[copy->definition->type - 1];
  if (!(copy->dimension = SDDS_Realloc(copy->dimension, sizeof(*copy->dimension) * copy->definition->dimensions))) {
    SDDS_SetError("Unable to get array--allocation failure (SDDS_GetArray)");
    return (NULL);
  }
  memcpy((void *)copy->dimension, (void *)original->dimension, sizeof(*copy->dimension) * copy->definition->dimensions);
  if (!(copy->elements = original->elements))
    return (copy);
  if (!(copy->data = SDDS_Realloc(copy->data, size * original->elements))) {
    SDDS_SetError("Unable to get array--allocation failure (SDDS_GetArray)");
    return (NULL);
  }

  if (copy->definition->type != SDDS_STRING)
    memcpy(copy->data, original->data, size * copy->elements);
  else if (!SDDS_CopyStringArray((char **)copy->data, (char **)original->data, original->elements)) {
    SDDS_SetError("Unable to get array--string copy failure (SDDS_GetArray)");
    return (NULL);
  }

  /* should free existing subpointers here, but probably not worth the trouble */
  if (copy->pointer && copy->definition->dimensions != 1)
    free(copy->pointer);
  if (!(copy->pointer = SDDS_MakePointerArray(copy->data, type, copy->definition->dimensions, copy->dimension))) {
    SDDS_SetError("Unable to get array--couldn't make pointer array (SDDS_GetArray)");
    return (NULL);
  }
  return (copy);
}

/**
 * @brief Retrieves an array from the current data table of an SDDS dataset and converts its elements to strings.
 *
 * This function extracts the specified array from the provided SDDS dataset and converts each of its elements into a null-terminated
 * string representation. The conversion respects the data type of the array elements, ensuring accurate string formatting.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param array_name A null-terminated string specifying the name of the SDDS array to retrieve and convert.
 * @param values Pointer to an integer where the number of elements in the array will be stored upon successful completion.
 *
 * @return On success, returns a pointer to an array of null-terminated strings (`char **`). Each string represents an element of the
 *         original SDDS array. On failure, returns `NULL` and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, unrecognized array name, memory allocation failure).
 * @retval Non-NULL Pointer to an array of strings representing the SDDS array elements.
 *
 * @note
 * - The caller is responsible for freeing each string in the returned array as well as the array itself.
 * - The function handles different data types, including numeric types and strings, ensuring proper formatting for each type.
 *
 * @sa SDDS_GetArray, SDDS_GetArrayInDoubles, SDDS_GetArrayInLong
 */
char **SDDS_GetArrayInString(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t *values) {
  int32_t index, type, i, elements;
  SDDS_ARRAY *original;
  char **data;
  char buffer[SDDS_MAXLINE];
  void *rawData;

  *values = 0;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetArrayInString"))
    return (NULL);
  if (!array_name) {
    SDDS_SetError("Unable to get array--array name pointer is NULL (SDDS_GetArrayInString)");
    return (NULL);
  }
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
    SDDS_SetError("Unable to get array--array name is unrecognized (SDDS_GetArrayInString)");
    return (NULL);
  }
  original = SDDS_dataset->array + index;
  type = original->definition->type;
  elements = original->elements;
  if (!(data = (char **)SDDS_Malloc(sizeof(*data) * elements))) {
    SDDS_SetError("Unable to get array--allocation failure (SDDS_GetArrayInString)");
    return (NULL);
  }
  rawData = original->data;
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = 0; i < elements; i++) {
      if (LDBL_DIG == 18) {
        sprintf(buffer, "%22.18Le", ((long double *)rawData)[i]);
      } else {
        sprintf(buffer, "%22.15Le", ((long double *)rawData)[i]);
      }
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_DOUBLE:
    for (i = 0; i < elements; i++) {
      sprintf(buffer, "%22.15le", ((double *)rawData)[i]);
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_FLOAT:
    for (i = 0; i < elements; i++) {
      sprintf(buffer, "%15.8e", ((float *)rawData)[i]);
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_LONG64:
    for (i = 0; i < elements; i++) {
      sprintf(buffer, "%" PRId64, ((int64_t *)rawData)[i]);
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_ULONG64:
    for (i = 0; i < elements; i++) {
      sprintf(buffer, "%" PRIu64, ((uint64_t *)rawData)[i]);
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_LONG:
    for (i = 0; i < elements; i++) {
      sprintf(buffer, "%" PRId32, ((int32_t *)rawData)[i]);
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_ULONG:
    for (i = 0; i < elements; i++) {
      sprintf(buffer, "%" PRIu32, ((uint32_t *)rawData)[i]);
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_SHORT:
    for (i = 0; i < elements; i++) {
      sprintf(buffer, "%hd", ((short *)rawData)[i]);
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_USHORT:
    for (i = 0; i < elements; i++) {
      sprintf(buffer, "%hu", ((unsigned short *)rawData)[i]);
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_CHARACTER:
    for (i = 0; i < elements; i++) {
      sprintf(buffer, "%c", ((char *)rawData)[i]);
      SDDS_CopyString(&data[i], buffer);
    }
    break;
  case SDDS_STRING:
    for (i = 0; i < elements; i++) {
      SDDS_CopyString(&data[i], ((char **)rawData)[i]);
    }
    break;
  }
  *values = elements;
  return data;
}

/**
 * @brief Retrieves an array from the current data table of an SDDS dataset and converts its elements to doubles.
 *
 * This function extracts the specified array from the provided SDDS dataset and converts each of its elements into a `double` value.
 * It ensures that the array is of a compatible numeric type before performing the conversion.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param array_name A null-terminated string specifying the name of the SDDS array to retrieve and convert.
 * @param values Pointer to an integer where the number of elements in the array will be stored upon successful completion.
 *
 * @return On success, returns a pointer to an array of `double` values representing the SDDS array elements. On failure, returns `NULL` and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, unrecognized array name, incompatible array type, memory allocation failure).
 * @retval Non-NULL Pointer to an array of `double` values representing the SDDS array elements.
 *
 * @note
 * - The caller is responsible for freeing the allocated memory for the returned `double` array.
 * - The function does not handle string-type arrays; attempting to retrieve a string array will result in an error.
 *
 * @sa SDDS_GetArray, SDDS_GetArrayInString, SDDS_GetArrayInLong
 */
double *SDDS_GetArrayInDoubles(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t *values) {
  int32_t index, type, i, elements;
  SDDS_ARRAY *original;
  double *data;
  void *rawData;

  *values = 0;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetArrayInDoubles"))
    return (NULL);
  if (!array_name) {
    SDDS_SetError("Unable to get array--array name pointer is NULL (SDDS_GetArrayInDoubles)");
    return (NULL);
  }
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
    SDDS_SetError("Unable to get array--array name is unrecognized (SDDS_GetArrayInDoubles)");
    return (NULL);
  }
  original = SDDS_dataset->array + index;
  if ((type = original->definition->type) == SDDS_STRING) {
    SDDS_SetError("Unable to get array--string type (SDDS_GetArrayInDoubles)");
    return (NULL);
  }
  elements = original->elements;
  if (!(data = SDDS_Malloc(sizeof(*data) * elements))) {
    SDDS_SetError("Unable to get array--allocation failure (SDDS_GetArrayInDoubles)");
    return (NULL);
  }
  rawData = original->data;
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = 0; i < elements; i++) {
      data[i] = ((long double *)rawData)[i];
    }
    break;
  case SDDS_DOUBLE:
    for (i = 0; i < elements; i++) {
      data[i] = ((double *)rawData)[i];
    }
    break;
  case SDDS_FLOAT:
    for (i = 0; i < elements; i++) {
      data[i] = ((float *)rawData)[i];
    }
    break;
  case SDDS_LONG64:
    for (i = 0; i < elements; i++) {
      data[i] = ((int64_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG64:
    for (i = 0; i < elements; i++) {
      data[i] = ((uint64_t *)rawData)[i];
    }
    break;
  case SDDS_LONG:
    for (i = 0; i < elements; i++) {
      data[i] = ((int32_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG:
    for (i = 0; i < elements; i++) {
      data[i] = ((uint32_t *)rawData)[i];
    }
    break;
  case SDDS_SHORT:
    for (i = 0; i < elements; i++) {
      data[i] = ((short *)rawData)[i];
    }
    break;
  case SDDS_USHORT:
    for (i = 0; i < elements; i++) {
      data[i] = ((unsigned short *)rawData)[i];
    }
    break;
  case SDDS_CHARACTER:
    for (i = 0; i < elements; i++) {
      data[i] = ((char *)rawData)[i];
    }
    break;
  }
  *values = elements;
  return data;
}

/**
 * @brief Retrieves an array from the current data table of an SDDS dataset and converts its elements to 32-bit integers.
 *
 * This function extracts the specified array from the provided SDDS dataset and converts each of its elements into a 32-bit integer (`int32_t`).
 * It ensures that the array is of a compatible numeric type before performing the conversion.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param array_name A null-terminated string specifying the name of the SDDS array to retrieve and convert.
 * @param values Pointer to an integer where the number of elements in the array will be stored upon successful completion.
 *
 * @return On success, returns a pointer to an array of `int32_t` values representing the SDDS array elements. On failure, returns `NULL` and sets an appropriate error message.
 *
 * @retval NULL Indicates that an error occurred (e.g., invalid dataset, unrecognized array name, incompatible array type, memory allocation failure).
 * @retval Non-NULL Pointer to an array of `int32_t` values representing the SDDS array elements.
 *
 * @note
 * - The caller is responsible for freeing the allocated memory for the returned `int32_t` array.
 * - The function does not handle string-type arrays; attempting to retrieve a string array will result in an error.
 *
 * @sa SDDS_GetArray, SDDS_GetArrayInDoubles, SDDS_GetArrayInString
 */
int32_t *SDDS_GetArrayInLong(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t *values) {
  int32_t index, type, i, elements;
  SDDS_ARRAY *original;
  int32_t *data;
  void *rawData;

  *values = 0;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetArrayInLong"))
    return (NULL);
  if (!array_name) {
    SDDS_SetError("Unable to get array--array name pointer is NULL (SDDS_GetArrayInLong)");
    return (NULL);
  }
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
    SDDS_SetError("Unable to get array--array name is unrecognized (SDDS_GetArrayInLong)");
    return (NULL);
  }
  original = SDDS_dataset->array + index;
  if ((type = original->definition->type) == SDDS_STRING) {
    SDDS_SetError("Unable to get array--string type (SDDS_GetArrayInLong)");
    return (NULL);
  }
  elements = original->elements;
  if (!(data = SDDS_Malloc(sizeof(*data) * elements))) {
    SDDS_SetError("Unable to get array--allocation failure (SDDS_GetArrayInLong)");
    return (NULL);
  }
  rawData = original->data;
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = 0; i < elements; i++) {
      data[i] = ((long double *)rawData)[i];
    }
    break;
  case SDDS_DOUBLE:
    for (i = 0; i < elements; i++) {
      data[i] = ((double *)rawData)[i];
    }
    break;
  case SDDS_FLOAT:
    for (i = 0; i < elements; i++) {
      data[i] = ((float *)rawData)[i];
    }
    break;
  case SDDS_LONG64:
    for (i = 0; i < elements; i++) {
      data[i] = ((int64_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG64:
    for (i = 0; i < elements; i++) {
      data[i] = ((uint64_t *)rawData)[i];
    }
    break;
  case SDDS_LONG:
    for (i = 0; i < elements; i++) {
      data[i] = ((int32_t *)rawData)[i];
    }
    break;
  case SDDS_ULONG:
    for (i = 0; i < elements; i++) {
      data[i] = ((uint32_t *)rawData)[i];
    }
    break;
  case SDDS_SHORT:
    for (i = 0; i < elements; i++) {
      data[i] = ((short *)rawData)[i];
    }
    break;
  case SDDS_USHORT:
    for (i = 0; i < elements; i++) {
      data[i] = ((unsigned short *)rawData)[i];
    }
    break;
  case SDDS_CHARACTER:
    for (i = 0; i < elements; i++) {
      data[i] = ((char *)rawData)[i];
    }
    break;
  }
  *values = elements;
  return data;
}

/**
 * @brief Retrieves the text and contents descriptions from an SDDS dataset.
 *
 * This function extracts the text description and contents description from the specified SDDS dataset.
 * The descriptions are copied into the provided pointers if they are not `NULL`. This allows users to
 * obtain metadata information about the dataset's content and purpose.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param text Pointer to a `char*` variable where the text description will be copied.
 *             If `NULL`, the text description is not retrieved.
 * @param contents Pointer to a `char*` variable where the contents description will be copied.
 *                If `NULL`, the contents description is not retrieved.
 *
 * @return Returns `1` on successful retrieval of the descriptions. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that the descriptions were successfully retrieved and copied.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, memory allocation failure).
 *
 * @note
 * - The caller is responsible for freeing the memory allocated for `text` and `contents` if they are not `NULL`.
 * - Ensure that the dataset is properly initialized before calling this function.
 *
 * @sa SDDS_SetDescription, SDDS_GetArray, SDDS_GetParameter
 */
int32_t SDDS_GetDescription(SDDS_DATASET *SDDS_dataset, char **text, char **contents) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetDescription"))
    return (0);
  if (text) {
    *text = NULL;
    if (!SDDS_CopyString(text, SDDS_dataset->layout.description)) {
      SDDS_SetError("Unable to retrieve description data (SDDS_GetDescription)");
      return (0);
    }
  }
  if (contents) {
    *contents = NULL;
    if (!SDDS_CopyString(contents, SDDS_dataset->layout.contents)) {
      SDDS_SetError("Unable to retrieve description data (SDDS_GetDescription)");
      return (0);
    }
  }

  return (1);
}

/**
 * @brief Sets unit conversions for a specified array in an SDDS dataset.
 *
 * This function updates the units of the specified array within the SDDS dataset and applies a conversion factor
 * to all its elements if the dataset has already been read (i.e., `pages_read > 0`). The function ensures that
 * the new units are consistent with the old units if provided.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param array_name A null-terminated string specifying the name of the array to update.
 * @param new_units A null-terminated string specifying the new units to assign to the array.
 *                  This parameter must not be `NULL`.
 * @param old_units A null-terminated string specifying the expected current units of the array.
 *                  If `NULL`, the function does not verify the existing units.
 * @param factor A `double` representing the conversion factor to apply to each element of the array.
 *               Each element will be multiplied by this factor.
 *
 * @return Returns `1` on successful unit conversion and update. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that the unit conversion was successfully applied.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, unrecognized array name, type undefined, memory allocation failure).
 *
 * @note
 * - The `new_units` parameter must not be `NULL`. Passing `NULL` will result in an error.
 * - If the dataset has not been read yet (`pages_read == 0`), the conversion factor is stored but not applied immediately.
 * - The function handles various data types, ensuring that the conversion factor is appropriately applied based on the array's type.
 *
 * @sa SDDS_SetColumnUnitsConversion, SDDS_SetParameterUnitsConversion, SDDS_GetArray
 */
int32_t  SDDS_SetArrayUnitsConversion(SDDS_DATASET *SDDS_dataset, char *array_name, char *new_units, char *old_units, double factor) {
  int32_t index, type;
  int64_t i;
  void *rawData;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetArrayUnitsConversion"))
    return(0);
  if (new_units == NULL) {
    SDDS_SetError("new_units is NULL (SDDS_SetArrayUnitsConversion)");
    return(0);
  }
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
    SDDS_SetError("Unable to get array--name is not recognized (SDDS_SetArrayUnitsConversion)");
    return(0);
  }
  if (!(type = SDDS_GetArrayType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get array--data type undefined (SDDS_SetArrayUnitsConversion)");
    return(0);
  }
  if (SDDS_dataset->layout.array_definition[index].units != NULL) {
    if (strcmp(new_units, SDDS_dataset->layout.array_definition[index].units) != 0) {
      if ((old_units != NULL) && (strcmp(old_units, SDDS_dataset->layout.array_definition[index].units) != 0)) {
        SDDS_SetError("Unexpected units value found (SDDS_SetArrayUnitsConversion)");
        return(0);
      }
      /* free(SDDS_dataset->layout.array_definition[index].units); */
      cp_str(&(SDDS_dataset->layout.array_definition[index].units), new_units);
    }
  } else {
    cp_str(&(SDDS_dataset->layout.array_definition[index].units), new_units);
  }

  if (SDDS_dataset->pages_read == 0) {
    return(1);
  }
  rawData = SDDS_dataset->array[index].data;
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = 0; i < SDDS_dataset->array[index].elements; i++) {
      ((long double *)rawData)[i] *= factor;
    }
    break;
  case SDDS_DOUBLE:
    for (i = 0; i < SDDS_dataset->array[index].elements; i++) {
      ((double *)rawData)[i] *= factor;
    }
    break;
  case SDDS_FLOAT:
    for (i = 0; i < SDDS_dataset->array[index].elements; i++) {
      ((float *)rawData)[i] *= factor;
    }
    break;
  case SDDS_LONG:
    for (i = 0; i < SDDS_dataset->array[index].elements; i++) {
      ((int32_t *)rawData)[i] *= factor;
    }
    break;
  case SDDS_ULONG:
    for (i = 0; i < SDDS_dataset->array[index].elements; i++) {
      ((uint32_t *)rawData)[i] *= factor;
    }
    break;
  case SDDS_LONG64:
    for (i = 0; i < SDDS_dataset->array[index].elements; i++) {
      ((int64_t *)rawData)[i] *= factor;
    }
    break;
  case SDDS_ULONG64:
    for (i = 0; i < SDDS_dataset->array[index].elements; i++) {
      ((uint64_t *)rawData)[i] *= factor;
    }
    break;
  case SDDS_SHORT:
    for (i = 0; i < SDDS_dataset->array[index].elements; i++) {
      ((short *)rawData)[i] *= factor;
    }
    break;
  case SDDS_USHORT:
    for (i = 0; i < SDDS_dataset->array[index].elements; i++) {
      ((unsigned short *)rawData)[i] *= factor;
    }
    break;
  }
  return(1);
}

/**
 * @brief Sets unit conversions for a specified column in an SDDS dataset.
 *
 * This function updates the units of the specified column within the SDDS dataset and applies a conversion factor
 * to all its elements if the dataset has already been read (i.e., `pages_read > 0`). The function ensures that
 * the new units are consistent with the old units if provided.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param column_name A null-terminated string specifying the name of the column to update.
 * @param new_units A null-terminated string specifying the new units to assign to the column.
 *                  This parameter must not be `NULL`.
 * @param old_units A null-terminated string specifying the expected current units of the column.
 *                  If `NULL`, the function does not verify the existing units.
 * @param factor A `double` representing the conversion factor to apply to each element of the column.
 *               Each element will be multiplied by this factor.
 *
 * @return Returns `1` on successful unit conversion and update. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that the unit conversion was successfully applied.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, unrecognized column name, type undefined, memory allocation failure).
 *
 * @note
 * - The `new_units` parameter must not be `NULL`. Passing `NULL` will result in an error.
 * - If the dataset has not been read yet (`pages_read == 0`), the conversion factor is stored but not applied immediately.
 * - The function handles various data types, ensuring that the conversion factor is appropriately applied based on the column's type.
 *
 * @sa SDDS_SetArrayUnitsConversion, SDDS_SetParameterUnitsConversion, SDDS_GetColumn
 */
int32_t  SDDS_SetColumnUnitsConversion(SDDS_DATASET *SDDS_dataset, char *column_name, char *new_units, char *old_units, double factor) {
  int32_t index, type;
  int64_t i;
  void *rawData;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetColumnUnitsConversion"))
    return(0);
  if (new_units == NULL) {
    SDDS_SetError("new_units is NULL (SDDS_SetColumnUnitsConversion)");
    return(0);
  }
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
    SDDS_SetError("Unable to get column--name is not recognized (SDDS_SetColumnUnitsConversion)");
    return(0);
  }
  if (!(type = SDDS_GetColumnType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get column--data type undefined (SDDS_SetColumnUnitsConversion)");
    return(0);
  }
  if (SDDS_dataset->layout.column_definition[index].units != NULL) {
    if (strcmp(new_units, SDDS_dataset->layout.column_definition[index].units) != 0) {
      if ((old_units != NULL) && (strcmp(old_units, SDDS_dataset->layout.column_definition[index].units) != 0)) {
        SDDS_SetError("Unexpected units value found (SDDS_SetColumnUnitsConversion)");
        return(0);
      }
      free(SDDS_dataset->layout.column_definition[index].units);
      cp_str(&(SDDS_dataset->original_layout.column_definition[index].units), new_units);
      cp_str(&(SDDS_dataset->layout.column_definition[index].units), new_units);
    }
  } else {
    cp_str(&(SDDS_dataset->original_layout.column_definition[index].units), new_units);
    cp_str(&(SDDS_dataset->layout.column_definition[index].units), new_units);
  }

  if (SDDS_dataset->pages_read == 0) {
    return(1);
  }
  rawData = SDDS_dataset->data[index];
  switch (type) {
  case SDDS_LONGDOUBLE:
    for (i = 0; i < SDDS_dataset->n_rows; i++) {
      ((long double *)rawData)[i] *= factor;
    }
    break;
  case SDDS_DOUBLE:
    for (i = 0; i < SDDS_dataset->n_rows; i++) {
      ((double *)rawData)[i] *= factor;
    }
    break;
  case SDDS_FLOAT:
    for (i = 0; i < SDDS_dataset->n_rows; i++) {
      ((float *)rawData)[i] *= factor;
    }
    break;
  case SDDS_LONG:
    for (i = 0; i < SDDS_dataset->n_rows; i++) {
      ((int32_t *)rawData)[i] *= factor;
    }
    break;
  case SDDS_ULONG:
    for (i = 0; i < SDDS_dataset->n_rows; i++) {
      ((uint32_t *)rawData)[i] *= factor;
    }
    break;
  case SDDS_LONG64:
    for (i = 0; i < SDDS_dataset->n_rows; i++) {
      ((int64_t *)rawData)[i] *= factor;
    }
    break;
  case SDDS_ULONG64:
    for (i = 0; i < SDDS_dataset->n_rows; i++) {
      ((uint64_t *)rawData)[i] *= factor;
    }
    break;
  case SDDS_SHORT:
    for (i = 0; i < SDDS_dataset->n_rows; i++) {
      ((short *)rawData)[i] *= factor;
    }
    break;
  case SDDS_USHORT:
    for (i = 0; i < SDDS_dataset->n_rows; i++) {
      ((unsigned short *)rawData)[i] *= factor;
    }
    break;
  }
  return(1);
}

/**
 * @brief Sets unit conversions for a specified parameter in an SDDS dataset.
 *
 * This function updates the units of the specified parameter within the SDDS dataset and applies a conversion factor
 * to its value if the dataset has already been read (i.e., `pages_read > 0`). The function ensures that the new units
 * are consistent with the old units if provided.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param parameter_name A null-terminated string specifying the name of the parameter to update.
 * @param new_units A null-terminated string specifying the new units to assign to the parameter.
 *                  This parameter must not be `NULL`.
 * @param old_units A null-terminated string specifying the expected current units of the parameter.
 *                  If `NULL`, the function does not verify the existing units.
 * @param factor A `double` representing the conversion factor to apply to the parameter's value.
 *               The parameter's value will be multiplied by this factor.
 *
 * @return Returns `1` on successful unit conversion and update. On failure, returns `0` and sets an appropriate error message.
 *
 * @retval 1 Indicates that the unit conversion was successfully applied.
 * @retval 0 Indicates that an error occurred (e.g., invalid dataset, unrecognized parameter name, type undefined, memory allocation failure).
 *
 * @note
 * - The `new_units` parameter must not be `NULL`. Passing `NULL` will result in an error.
 * - If the dataset has not been read yet (`pages_read == 0`), the conversion factor is stored but not applied immediately.
 * - The function handles various data types, ensuring that the conversion factor is appropriately applied based on the parameter's type.
 *
 * @sa SDDS_SetArrayUnitsConversion, SDDS_SetColumnUnitsConversion, SDDS_GetParameter
 */
int32_t  SDDS_SetParameterUnitsConversion(SDDS_DATASET *SDDS_dataset, char *parameter_name, char *new_units, char *old_units, double factor) {
  int32_t index, type;
  void *rawData;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetParameterUnitsConversion"))
    return(0);
  if (new_units == NULL) {
    SDDS_SetError("new_units is NULL (SDDS_SetParameterUnitsConversion)");
    return(0);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    SDDS_SetError("Unable to get parameter--name is not recognized (SDDS_SetParameterUnitsConversion)");
    return(0);
  }
  if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
    SDDS_SetError("Unable to get parameter--data type undefined (SDDS_SetParameterUnitsConversion)");
    return(0);
  }
  if (SDDS_dataset->layout.parameter_definition[index].units != NULL) {
    if (strcmp(new_units, SDDS_dataset->layout.parameter_definition[index].units) != 0) {
      if ((old_units != NULL) && (strcmp(old_units, SDDS_dataset->layout.parameter_definition[index].units) != 0)) {
        SDDS_SetError("Unexpected units value found (SDDS_SetParameterUnitsConversion)");
        return(0);
      }
      /* free(SDDS_dataset->layout.parameter_definition[index].units); */
      cp_str(&(SDDS_dataset->layout.parameter_definition[index].units), new_units);
    }
  } else {
    cp_str(&(SDDS_dataset->layout.parameter_definition[index].units), new_units);
  }

  if (SDDS_dataset->pages_read == 0) {
    return(1);
  }
  rawData = SDDS_dataset->parameter[index];
  switch (type) {
  case SDDS_LONGDOUBLE:
    *((long double *)rawData) *= factor;
    break;
  case SDDS_DOUBLE:
    *((double *)rawData) *= factor;
    break;
  case SDDS_FLOAT:
    *((float *)rawData) *= factor;
    break;
  case SDDS_LONG:
    *((int32_t *)rawData) *= factor;
    break;
  case SDDS_ULONG:
    *((uint32_t *)rawData) *= factor;
    break;
  case SDDS_LONG64:
    *((int64_t *)rawData) *= factor;
    break;
  case SDDS_ULONG64:
    *((uint64_t *)rawData) *= factor;
    break;
  case SDDS_SHORT:
    *((short *)rawData) *= factor;
    break;
  case SDDS_USHORT:
    *((unsigned short *)rawData) *= factor;
    break;
  }
  return(1);
}
