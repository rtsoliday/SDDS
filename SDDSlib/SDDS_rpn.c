/**
 * @file SDDS_rpn.c
 * @brief Implements Reverse Polish Notation (RPN) operations for SDDS datasets.
 *
 * This file provides functions to convert various data types to double and long double,
 * as well as functions to compute parameters and columns using RPN expressions within
 * SDDS datasets. It also includes functionality to filter rows based on RPN tests.
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
 *  M. Borland,
 *  C. Saunders,
 *  R. Soliday
 *  H. Shang
 */

#include "mdb.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDS_internal.h"
#include "rpn.h"

/**
 * @brief Converts a long double value to double.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertLongDoubleToDouble(void *data, int64_t index) {
  return *((long double *)data + index);
}

/**
 * @brief Converts a double value to double (identity function).
 *
 * @param data Pointer to the data array.
 * @param index Index of the element.
 * @return The double value.
 */
double SDDS_ConvertDoubleToDouble(void *data, int64_t index) {
  return *((double *)data + index);
}

/**
 * @brief Converts a float value to double.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertFloatToDouble(void *data, int64_t index) {
  return *((float *)data + index);
}

/**
 * @brief Converts a 64-bit integer to double.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertLong64ToDouble(void *data, int64_t index) {
  return *((int64_t *)data + index);
}

/**
 * @brief Converts an unsigned 64-bit integer to double.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertULong64ToDouble(void *data, int64_t index) {
  return *((uint64_t *)data + index);
}

/**
 * @brief Converts a 32-bit integer to double.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertLongToDouble(void *data, int64_t index) {
  return *((int32_t *)data + index);
}

/**
 * @brief Converts an unsigned 32-bit integer to double.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertULongToDouble(void *data, int64_t index) {
  return *((uint32_t *)data + index);
}

/**
 * @brief Converts a short integer to double.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertShortToDouble(void *data, int64_t index) {
  return *((short *)data + index);
}

/**
 * @brief Converts an unsigned short integer to double.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertUShortToDouble(void *data, int64_t index) {
  return *((unsigned short *)data + index);
}

/**
 * @brief Converts a string to double using atof.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertStringToDouble(void *data, int64_t index) {
  return atof(*((char **)data + index));
}

/**
 * @brief Converts a character to double.
 *
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value.
 */
double SDDS_ConvertCharToDouble(void *data, int64_t index) {
  return *((char *)data + index);
}

/**
 * @brief Converts a value to long double based on its type.
 *
 * @param type The SDDS data type of the value.
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted long double value, or 0.0 on error.
 */
long double SDDS_ConvertToLongDouble(int32_t type, void *data, int64_t index) {
  if (!data) {
    SDDS_SetError("NULL data pointer passed (SDDS_ConvertToLongDouble)");
    return (0.0);
  }
  switch (type) {
  case SDDS_SHORT:
    return ((long double)*((short *)data + index));
  case SDDS_USHORT:
    return ((long double)*((unsigned short *)data + index));
  case SDDS_LONG:
    return ((long double)*((int32_t *)data + index));
  case SDDS_ULONG:
    return ((long double)*((uint32_t *)data + index));
  case SDDS_LONG64:
    return ((long double)*((int64_t *)data + index));
  case SDDS_ULONG64:
    return ((long double)*((uint64_t *)data + index));
  case SDDS_FLOAT:
    return ((long double)*((float *)data + index));
  case SDDS_DOUBLE:
    return ((long double)*((double *)data + index));
  case SDDS_LONGDOUBLE:
    return (*((long double *)data + index));
  case SDDS_CHARACTER:
    return ((long double)*((unsigned char *)data + index));
  default:
    SDDS_SetError("Invalid data type seen (SDDS_ConvertToLongDouble)");
    return (0.0);
  }
}

/**
 * @brief Converts a value to double based on its type.
 *
 * @param type The SDDS data type of the value.
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted double value, or 0.0 on error.
 */
double SDDS_ConvertToDouble(int32_t type, void *data, int64_t index) {
  if (!data) {
    SDDS_SetError("NULL data pointer passed (SDDS_ConvertToDouble)");
    return (0.0);
  }
  switch (type) {
  case SDDS_SHORT:
    return ((double)*((short *)data + index));
  case SDDS_USHORT:
    return ((double)*((unsigned short *)data + index));
  case SDDS_LONG:
    return ((double)*((int32_t *)data + index));
  case SDDS_ULONG:
    return ((double)*((uint32_t *)data + index));
  case SDDS_LONG64:
    return ((double)*((int64_t *)data + index));
  case SDDS_ULONG64:
    return ((double)*((uint64_t *)data + index));
  case SDDS_FLOAT:
    return ((double)*((float *)data + index));
  case SDDS_DOUBLE:
    return (*((double *)data + index));
  case SDDS_LONGDOUBLE:
    return ((double)*((long double *)data + index));
  case SDDS_CHARACTER:
    return ((double)*((unsigned char *)data + index));
  default:
    SDDS_SetError("Invalid data type seen (SDDS_ConvertToDouble)");
    return (0.0);
  }
}

/**
 * @brief Converts a value to a 64-bit integer based on its type.
 *
 * @param type The SDDS data type of the value.
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted 64-bit integer, or 0 on error.
 */
int64_t SDDS_ConvertToLong64(int32_t type, void *data, int64_t index) {
  if (!data) {
    SDDS_SetError("NULL data pointer passed (SDDS_ConvertToLong64)");
    return (0.0);
  }
  switch (type) {
  case SDDS_LONGDOUBLE:
    return ((int64_t) * ((long double *)data + index));
  case SDDS_DOUBLE:
    return ((int64_t) * ((double *)data + index));
  case SDDS_FLOAT:
    return ((int64_t) * ((float *)data + index));
  case SDDS_SHORT:
    return ((int64_t) * ((short *)data + index));
  case SDDS_USHORT:
    return ((int64_t) * ((unsigned short *)data + index));
  case SDDS_LONG:
    return ((int64_t) * ((int32_t *)data + index));
  case SDDS_ULONG:
    return ((int64_t) * ((uint32_t *)data + index));
  case SDDS_LONG64:
    return (*((int64_t *)data + index));
  case SDDS_ULONG64:
    return ((int64_t) * ((uint64_t *)data + index));
  case SDDS_CHARACTER:
    return ((int64_t) * ((unsigned char *)data + index));
  default:
    SDDS_SetError("Invalid data type seen (SDDS_ConvertToLong64)");
    return (0.0);
  }
}

/**
 * @brief Converts a value to a 32-bit integer based on its type.
 *
 * @param type The SDDS data type of the value.
 * @param data Pointer to the data array.
 * @param index Index of the element to convert.
 * @return The converted 32-bit integer, or 0 on error.
 */
int32_t SDDS_ConvertToLong(int32_t type, void *data, int64_t index) {
  if (!data) {
    SDDS_SetError("NULL data pointer passed (SDDS_ConvertToLong)");
    return (0.0);
  }
  switch (type) {
  case SDDS_LONGDOUBLE:
    return ((int32_t) * ((long double *)data + index));
  case SDDS_DOUBLE:
    return ((int32_t) * ((double *)data + index));
  case SDDS_FLOAT:
    return ((int32_t) * ((float *)data + index));
  case SDDS_SHORT:
    return ((int32_t) * ((short *)data + index));
  case SDDS_USHORT:
    return ((int32_t) * ((unsigned short *)data + index));
  case SDDS_LONG:
    return (*((int32_t *)data + index));
  case SDDS_ULONG:
    return ((int32_t) * ((uint32_t *)data + index));
  case SDDS_LONG64:
    return ((int32_t) * ((int64_t *)data + index));
  case SDDS_ULONG64:
    return ((int32_t) * ((uint64_t *)data + index));
  case SDDS_CHARACTER:
    return ((int32_t) * ((unsigned char *)data + index));
  default:
    SDDS_SetError("Invalid data type seen (SDDS_ConvertToLong)");
    return (0.0);
  }
}

#if defined(RPN_SUPPORT)

static double (*SDDS_ConvertTypeToDouble[SDDS_NUM_TYPES + 1])(void *data, int64_t index) =
  {
    NULL,
    SDDS_ConvertLongDoubleToDouble,
    SDDS_ConvertDoubleToDouble,
    SDDS_ConvertFloatToDouble,
    SDDS_ConvertLong64ToDouble,
    SDDS_ConvertULong64ToDouble,
    SDDS_ConvertLongToDouble,
    SDDS_ConvertULongToDouble,
    SDDS_ConvertShortToDouble,
    SDDS_ConvertUShortToDouble,
    SDDS_ConvertStringToDouble,
    SDDS_ConvertCharToDouble};

// Static variables to store memory identifiers for RPN operations
static int64_t table_number_mem = -1;
static int64_t i_page_mem = -1;
static int64_t n_rows_mem = -1;
static int64_t i_row_mem = -1;

/**
 * @brief Creates an RPN memory block.
 *
 * @param name Name of the RPN memory.
 * @param is_string Flag indicating if the memory is for string data.
 * @return Identifier of the created RPN memory, or -1 on failure.
 */
int64_t SDDS_CreateRpnMemory(const char *name, short is_string) {
  if (!name)
    return (-1);
  return (rpn_create_mem((char *)name, is_string));
}

/**
 * @brief Creates an RPN array for a given name.
 *
 * @param name Name of the RPN array.
 * @return Identifier of the created RPN array, or -1 on failure.
 */
int64_t SDDS_CreateRpnArray(char *name) {
  int64_t memnum;
  double dummy;
  char *dummy1 = NULL;
  short is_string = 0;

  if (!name)
    return (-1);
  if ((memnum = is_memory(&dummy, &dummy1, &is_string, name)) >= 0)
    return memnum;
  if ((memnum = rpn_create_mem(name, is_string)) >= 0)
    rpn_store((double)rpn_createarray(1), NULL, memnum);
  return memnum;
}

/**
 * @brief Computes a parameter in the SDDS dataset using an RPN equation.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset.
 * @param parameter Index of the parameter to compute.
 * @param equation RPN equation as a string.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_ComputeParameter(SDDS_DATASET *SDDS_dataset, int32_t parameter, char *equation) {
  SDDS_LAYOUT *layout;
  double value;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ComputeParameter"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (parameter < 0 || parameter >= layout->n_parameters)
    return (0);

  if (!equation) {
    SDDS_SetError("Unable to compute defined parameter--no equation for named parameter (SDDS_ComputeParameter)");
    return (0);
  }

  if (!SDDS_StoreParametersInRpnMemories(SDDS_dataset))
    return (0);
  if (!SDDS_StoreColumnsInRpnArrays(SDDS_dataset))
    return 0;

  value = rpn(equation);
  rpn_store(value, NULL, layout->parameter_definition[parameter].memory_number);
  if (rpn_check_error()) {
    SDDS_SetError("Unable to compute rpn expression--rpn error (SDDS_ComputeParameter)");
    return (0);
  }
#  if defined(DEBUG)
  fprintf(stderr, "computed parameter value %s with equation %s: %e\n", layout->parameter_definition[parameter].name, equation, value);
#  endif
  switch (layout->parameter_definition[parameter].type) {
  case SDDS_CHARACTER:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (char)value, -1);
    break;
  case SDDS_SHORT:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (short)value, -1);
    break;
  case SDDS_USHORT:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (unsigned short)value, -1);
    break;
  case SDDS_LONG:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (int32_t)value, -1);
    break;
  case SDDS_ULONG:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (uint32_t)value, -1);
    break;
  case SDDS_LONG64:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (int64_t)value, -1);
    break;
  case SDDS_ULONG64:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (uint64_t)value, -1);
    break;
  case SDDS_FLOAT:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (float)value, -1);
    break;
  case SDDS_DOUBLE:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (double)value, -1);
    break;
  case SDDS_LONGDOUBLE:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (long double)value, -1);
    break;
  }

  return (1);
}

/**
 * @brief Computes a column in the SDDS dataset using an RPN equation.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset.
 * @param column Index of the column to compute.
 * @param equation RPN equation as a string.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_ComputeColumn(SDDS_DATASET *SDDS_dataset, int32_t column, char *equation) {
  int64_t j;
  SDDS_LAYOUT *layout;
  double value;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ComputeColumn"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (column < 0 || column >= layout->n_columns)
    return (0);

  if (!SDDS_StoreParametersInRpnMemories(SDDS_dataset))
    return (0);
  if (!SDDS_StoreColumnsInRpnArrays(SDDS_dataset))
    return 0;

  if (table_number_mem == -1) {
    table_number_mem = rpn_create_mem("table_number", 0);
    i_page_mem = rpn_create_mem("i_page", 0);
    n_rows_mem = rpn_create_mem("n_rows", 0);
    i_row_mem = rpn_create_mem("i_row", 0);
  }

  rpn_store((double)SDDS_dataset->page_number, NULL, table_number_mem);
  rpn_store((double)SDDS_dataset->page_number, NULL, i_page_mem);
  rpn_store((double)SDDS_dataset->n_rows, NULL, n_rows_mem);
#  if defined(DEBUG)
  fprintf(stderr, "computing %s using equation %s\n", layout->column_definition[column].name, equation);
#  endif

  for (j = 0; j < SDDS_dataset->n_rows; j++) {
    rpn_clear();
    if (!SDDS_StoreRowInRpnMemories(SDDS_dataset, j))
      return (0);
    rpn_store((double)j, NULL, i_row_mem);
    value = rpn(equation);
    rpn_store(value, NULL, layout->column_definition[column].memory_number);
    if (rpn_check_error()) {
      SDDS_SetError("Unable to compute rpn expression--rpn error (SDDS_ComputeDefinedColumn)");
      return (0);
    }
#  if defined(DEBUG)
    fprintf(stderr, "computed row value: %s = %e\n", layout->column_definition[column].name, value);
#  endif
    switch (layout->column_definition[column].type) {
    case SDDS_CHARACTER:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (char)value, -1);
      break;
    case SDDS_SHORT:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (short)value, -1);
      break;
    case SDDS_USHORT:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (unsigned short)value, -1);
      break;
    case SDDS_LONG:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (int32_t)value, -1);
      break;
    case SDDS_ULONG:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (uint32_t)value, -1);
      break;
    case SDDS_LONG64:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (int64_t)value, -1);
      break;
    case SDDS_ULONG64:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (uint64_t)value, -1);
      break;
    case SDDS_FLOAT:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (float)value, -1);
      break;
    case SDDS_DOUBLE:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (double)value, -1);
      break;
    case SDDS_LONGDOUBLE:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (long double)value, -1);
      break;
    }
  }

  return (1);
}

/**
 * @brief Filters rows in the SDDS dataset based on an RPN test expression.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset.
 * @param rpn_test RPN test expression as a string.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_FilterRowsWithRpnTest(SDDS_DATASET *SDDS_dataset, char *rpn_test) {
  int64_t i, j;
  int32_t n_columns;
  SDDS_LAYOUT *layout;
  int32_t accept;
  static int64_t table_number_mem = -1, n_rows_mem = -1, i_page_mem = -1;
  COLUMN_DEFINITION *coldef;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ComputeRpnEquations"))
    return (0);
  layout = &SDDS_dataset->layout;

  if (table_number_mem == -1) {
    table_number_mem = rpn_create_mem("table_number", 0);
    i_page_mem = rpn_create_mem("page_number", 0);
    n_rows_mem = rpn_create_mem("n_rows", 0);
    i_row_mem = rpn_create_mem("i_row", 0);
  }

  rpn_store((double)SDDS_dataset->page_number, NULL, table_number_mem);
  rpn_store((double)SDDS_dataset->page_number, NULL, i_page_mem);
  rpn_store((double)SDDS_dataset->n_rows, NULL, n_rows_mem);

  for (i = 0; i < layout->n_columns; i++) {
    if (layout->column_definition[i].memory_number < 0) {
      SDDS_SetError("Unable to compute equations--column lacks rpn memory number (SDDS_FilterRowsWithRpnTest)");
      return (0);
    }
  }

  n_columns = layout->n_columns;

  for (j = 0; j < SDDS_dataset->n_rows; j++) {
    rpn_clear();
    rpn_store((double)j, NULL, i_row_mem);
    /* store values in memories */
    coldef = layout->column_definition;
    for (i = 0; i < n_columns; i++, coldef++) {
      if (coldef->type != SDDS_STRING) {
        rpn_quick_store((*SDDS_ConvertTypeToDouble[coldef->type])(SDDS_dataset->data[i], j), NULL, coldef->memory_number);
      } else {
        rpn_quick_store(0, ((char **)SDDS_dataset->data[i])[j], coldef->memory_number);
      }
    }
    rpn(rpn_test);
    if (rpn_check_error()) {
      SDDS_SetError("Unable to compute rpn expression--rpn error (SDDS_FilterRowsWithRpnTest)");
      return (0);
    }
    if (!pop_log(&accept)) {
      SDDS_SetError("rpn column-based test expression problem");
      return (0);
    }
    if (!accept)
      SDDS_dataset->row_flag[j] = 0;
  }
  rpn_clear();
  return (1);
}

/**
 * @brief Stores all parameters of the SDDS dataset into RPN memories.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_StoreParametersInRpnMemories(SDDS_DATASET *SDDS_dataset) {
  int32_t i;
  SDDS_LAYOUT *layout;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_StoreParametersInRpnMemories"))
    return (0);
  layout = &SDDS_dataset->layout;

  rpn_clear();
#  if defined(DEBUG)
  fprintf(stderr, "storing parameters: ");
#  endif
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].memory_number < 0) {
      SDDS_SetError("Unable to compute equations--parameter lacks rpn memory number (SDDS_StoreParametersInRpnMemories");
      return (0);
    }
    if (layout->parameter_definition[i].type != SDDS_STRING) {
      rpn_quick_store((*SDDS_ConvertTypeToDouble[layout->parameter_definition[i].type])(SDDS_dataset->parameter[i], 0), NULL, layout->parameter_definition[i].memory_number);
#  if defined(DEBUG)
      fprintf(stderr, "%s = %le ", layout->parameter_definition[i].name, rpn_recall(layout->parameter_definition[i].memory_number));
#  endif
    } else {
      rpn_quick_store(0, ((char **)SDDS_dataset->parameter[i])[0], layout->parameter_definition[i].memory_number);
#  if defined(DEBUG)
      fprintf(stderr, "%s = %s ", layout->parameter_definition[i].name, rpn_str_recall(layout->parameter_definition[i].memory_number));
#  endif
    }
  }
  if (SDDS_NumberOfErrors())
    return (0);
  if (rpn_check_error()) {
    SDDS_SetError("Unable to compute rpn expression--rpn error (SDDS_StoreParametersInRpnMemories)");
    return (0);
  }
#  if defined(DEBUG)
  fputc('\n', stderr);
#  endif
  return (1);
}

/**
 * @brief Stores a specific row's column values into RPN memories.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset.
 * @param row Index of the row to store.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_StoreRowInRpnMemories(SDDS_DATASET *SDDS_dataset, int64_t row) {
  int32_t i, columns;
  COLUMN_DEFINITION *coldef;

  columns = SDDS_dataset->layout.n_columns;
  if (row == 0) {
    coldef = SDDS_dataset->layout.column_definition;
    for (i = 0; i < columns; i++, coldef++) {
      if (coldef->memory_number < 0) {
        SDDS_SetError("Unable to compute equations--column lacks rpn memory number (SDDS_StoreRowInRpnMemories)");
        return (0);
      }
    }
  }
  coldef = SDDS_dataset->layout.column_definition;
  for (i = 0; i < columns; i++, coldef++) {
    if (coldef->type != SDDS_STRING) {
      rpn_quick_store((*SDDS_ConvertTypeToDouble[coldef->type])(SDDS_dataset->data[i], row), NULL, coldef->memory_number);
    } else {
      rpn_quick_store(0, ((char **)SDDS_dataset->data[i])[row], coldef->memory_number);
    }
  }
  return (1);
}

/**
 * @brief Stores all column data into RPN arrays for efficient computation.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_StoreColumnsInRpnArrays(SDDS_DATASET *SDDS_dataset) {
  int64_t i, j;
  int32_t arraysize;
  SDDS_LAYOUT *layout;
  double *arraydata;
  int32_t *longPtr;
  uint32_t *ulongPtr;
  int64_t *long64Ptr;
  uint64_t *ulong64Ptr;
  short *shortPtr;
  unsigned short *ushortPtr;
  long double *ldoublePtr;
  /*  double *doublePtr; */
  float *floatPtr;
  char *charPtr;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_StoreColumnsRpnArrays"))
    return (0);
  layout = &SDDS_dataset->layout;
  rpn_clear();
  for (i = 0; i < layout->n_columns; i++) {
    if (layout->column_definition[i].type != SDDS_STRING) {
      if (layout->column_definition[i].pointer_number < 0) {
        SDDS_SetError("Unable to compute equations--column lacks rpn pointer number (SDDS_StoreColumnsInRpnArrays)");
        return (0);
      }
      if (!rpn_resizearray((int32_t)rpn_recall(layout->column_definition[i].pointer_number), SDDS_dataset->n_rows)) {
        SDDS_SetError("Unable to compute equations--couldn't resize rpn arrays (SDDS_StoreColumnsInRpnArrays)");
        return 0;
      }
      if (!(arraydata = rpn_getarraypointer(layout->column_definition[i].pointer_number, &arraysize)) || arraysize != SDDS_dataset->n_rows) {
        SDDS_SetError("Unable to compute equations--couldn't retrieve rpn arrays (SDDS_StoreColumnsInRpnArrays)");
        return 0;
      }
      switch (layout->column_definition[i].type) {
      case SDDS_LONGDOUBLE:
        ldoublePtr = (long double *)(SDDS_dataset->data[i]);
        for (j = 0; j < SDDS_dataset->n_rows; j++)
          arraydata[j] = ldoublePtr[j];
        break;
      case SDDS_DOUBLE:
        memcpy((char *)arraydata, (char *)(SDDS_dataset->data[i]), SDDS_dataset->n_rows * sizeof(double));
#  if 0
	      doublePtr = (double *)(SDDS_dataset->data[i]);
	      for (j = 0; j < SDDS_dataset->n_rows; j++)
		arraydata[j] = doublePtr[j];
#  endif
        break;
      case SDDS_FLOAT:
        floatPtr = (float *)(SDDS_dataset->data[i]);
        for (j = 0; j < SDDS_dataset->n_rows; j++)
          arraydata[j] = floatPtr[j];
        break;
      case SDDS_LONG64:
        long64Ptr = (int64_t *)(SDDS_dataset->data[i]);
        for (j = 0; j < SDDS_dataset->n_rows; j++)
          arraydata[j] = long64Ptr[j];
        break;
      case SDDS_ULONG64:
        ulong64Ptr = (uint64_t *)(SDDS_dataset->data[i]);
        for (j = 0; j < SDDS_dataset->n_rows; j++)
          arraydata[j] = ulong64Ptr[j];
        break;
      case SDDS_LONG:
        longPtr = (int32_t *)(SDDS_dataset->data[i]);
        for (j = 0; j < SDDS_dataset->n_rows; j++)
          arraydata[j] = longPtr[j];
        break;
      case SDDS_ULONG:
        ulongPtr = (uint32_t *)(SDDS_dataset->data[i]);
        for (j = 0; j < SDDS_dataset->n_rows; j++)
          arraydata[j] = ulongPtr[j];
        break;
      case SDDS_SHORT:
        shortPtr = (short *)(SDDS_dataset->data[i]);
        for (j = 0; j < SDDS_dataset->n_rows; j++)
          arraydata[j] = shortPtr[j];
        break;
      case SDDS_USHORT:
        ushortPtr = (unsigned short *)(SDDS_dataset->data[i]);
        for (j = 0; j < SDDS_dataset->n_rows; j++)
          arraydata[j] = ushortPtr[j];
        break;
      case SDDS_CHARACTER:
        charPtr = (char *)(SDDS_dataset->data[i]);
        for (j = 0; j < SDDS_dataset->n_rows; j++)
          arraydata[j] = charPtr[j];
        break;
      }
    }
  }
  return 1;
}

#else /* end of RPN_SUPPORT section */

/**
 * @brief Stub function for creating RPN memory when RPN_SUPPORT is not enabled.
 *
 * @param name Name of the RPN memory.
 * @param is_string Flag indicating if the memory is for string data.
 * @return Always returns 1.
 */
int64_t SDDS_CreateRpnMemory(const char *name, short is_string) {
  return (1);
}

/**
 * @brief Stub function for creating RPN arrays when RPN_SUPPORT is not enabled.
 *
 * @param name Name of the RPN array.
 * @return Always returns 1.
 */
int64_t SDDS_CreateRpnArray(char *name) {
  return (1);
}

#endif
