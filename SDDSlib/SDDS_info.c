/**
 * @file SDDS_info.c
 * @brief This file contains routines for getting the meta data for the SDDS objects.
 *
 * This file provides functions for getting information about the SDDS columns, parameters and arrays.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license 
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDS_internal.h"

/**
 * @brief Retrieves information about a specified column in the SDDS dataset.
 *
 * This function is the preferred alternative to `SDDS_GetColumnDefinition`. It allows you to obtain information about a specific field of a column, either by the column's name or index.
 *
 * @param[in] SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in] field_name   A null-terminated string specifying the name of the field for which information is requested.
 * @param[out] memory      Pointer to a variable where the retrieved information will be stored. The variable should be of type `data_type*`, where `data_type` corresponds to the type of the requested information. For `STRING` information, use `char*`. If `memory` is `NULL`, the function will verify the existence and type of the information, returning the data type without storing any data.
 * @param[in] mode         Specifies how to identify the column. Valid values are:
 *                          - `SDDS_GET_BY_NAME`: Identify the column by its name. Requires an additional argument of type `char*` (column name).
 *                          - `SDDS_GET_BY_INDEX`: Identify the column by its index. Requires an additional argument of type `int32_t` (column index).
 *
 * @return On success, returns the SDDS data type of the requested information. On failure, returns zero and records an error message.
 *
 * @note This function uses variable arguments to accept either the column name or index based on the `mode` parameter.
 *
 * @see SDDS_GetColumnDefinition
 */
int32_t SDDS_GetColumnInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...) {
  int32_t field_index, type;
  int32_t column_index;
  COLUMN_DEFINITION *columndef;
  char *column_name;
  va_list argptr;
  int32_t retval;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnInformation"))
    return (0);

  if (!field_name) {
    SDDS_SetError("NULL field name passed. (SDDS_GetColumnInformation)");
    return (0);
  }

  va_start(argptr, mode);
  retval = 1;
  if (mode & SDDS_GET_BY_INDEX) {
    if ((column_index = va_arg(argptr, int32_t)) < 0 || column_index >= SDDS_dataset->layout.n_columns) {
      SDDS_SetError("Invalid column index passed. (SDDS_GetColumnInformation)");
      retval = 0;
    }
  } else {
    if (!(column_name = va_arg(argptr, char *))) {
      SDDS_SetError("NULL column name passed. (SDDS_GetColumnInformation)");
      retval = 0;
    }
    if ((column_index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
      SDDS_SetError("Unknown column name given (SDDS_GetColumnInformation)");
      retval = 0;
    }
  }
  columndef = SDDS_dataset->layout.column_definition + column_index;
  va_end(argptr);
  if (!retval)
    return (0);

  for (field_index = 0; field_index < SDDS_COLUMN_FIELDS; field_index++)
    if (strcmp(field_name, SDDS_ColumnFieldInformation[field_index].name) == 0)
      break;
  if (field_index == SDDS_COLUMN_FIELDS) {
    SDDS_SetError("Unknown field name given (SDDS_GetColumnInformation)");
    return (0);
  }
  type = SDDS_ColumnFieldInformation[field_index].type;
  if (!memory)
    return (type);
  if (type == SDDS_STRING) {
    if (!SDDS_CopyString((char **)memory, *((char **)((char *)columndef + SDDS_ColumnFieldInformation[field_index].offset)))) {
      SDDS_SetError("Unable to copy field data (SDDS_GetColumnInformation)");
      return (0);
    }
  } else
    memcpy(memory, (char *)columndef + SDDS_ColumnFieldInformation[field_index].offset, SDDS_type_size[type - 1]);
  return (type);
}

/**
 * @brief Retrieves information about a specified parameter in the SDDS dataset.
 *
 * This function is the preferred alternative to `SDDS_GetParameterDefinition`. It allows you to obtain information about a specific field of a parameter, either by the parameter's name or index.
 *
 * @param[in] SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in] field_name   A null-terminated string specifying the name of the field for which information is requested.
 * @param[out] memory      Pointer to a variable where the retrieved information will be stored. The variable should be of type `data_type*`, where `data_type` corresponds to the type of the requested information. For `STRING` information, use `char*`. If `memory` is `NULL`, the function will verify the existence and type of the information, returning the data type without storing any data.
 * @param[in] mode         Specifies how to identify the parameter. Valid values are:
 *                          - `SDDS_GET_BY_NAME`: Identify the parameter by its name. Requires an additional argument of type `char*` (parameter name).
 *                          - `SDDS_GET_BY_INDEX`: Identify the parameter by its index. Requires an additional argument of type `int32_t` (parameter index).
 *
 * @return On success, returns the SDDS data type of the requested information. On failure, returns zero and records an error message.
 *
 * @note This function uses variable arguments to accept either the parameter name or index based on the `mode` parameter.
 *
 * @see SDDS_GetParameterDefinition
 */
int32_t SDDS_GetParameterInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...) {
  int32_t field_index, type, parameter_index;
  PARAMETER_DEFINITION *parameterdef;
  char *parameter_name;
  va_list argptr;
  int32_t retval;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterInformation"))
    return (0);

  if (!field_name) {
    SDDS_SetError("NULL field name passed. (SDDS_GetParameterInformation)");
    return (0);
  }

  va_start(argptr, mode);
  retval = 1;
  if (mode & SDDS_GET_BY_INDEX) {
    if ((parameter_index = va_arg(argptr, int32_t)) < 0 || parameter_index >= SDDS_dataset->layout.n_parameters) {
      SDDS_SetError("Invalid parameter index passed. (SDDS_GetParameterInformation)");
      retval = 0;
    }
  } else {
    if (!(parameter_name = va_arg(argptr, char *))) {
      SDDS_SetError("NULL parameter name passed. (SDDS_GetParameterInformation)");
      retval = 0;
    }
    if ((parameter_index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
      SDDS_SetError("Unknown parameter name given (SDDS_GetParameterInformation)");
      retval = 0;
    }
  }
  parameterdef = SDDS_dataset->layout.parameter_definition + parameter_index;
  va_end(argptr);
  if (!retval)
    return (0);

  for (field_index = 0; field_index < SDDS_PARAMETER_FIELDS; field_index++)
    if (strcmp(field_name, SDDS_ParameterFieldInformation[field_index].name) == 0)
      break;
  if (field_index == SDDS_PARAMETER_FIELDS) {
    SDDS_SetError("Unknown field name given (SDDS_GetParameterInformation)");
    return (0);
  }
  type = SDDS_ParameterFieldInformation[field_index].type;
  if (!memory)
    return (type);
  if (type == SDDS_STRING) {
    if (!SDDS_CopyString((char **)memory, *((char **)((char *)parameterdef + SDDS_ParameterFieldInformation[field_index].offset)))) {
      SDDS_SetError("Unable to copy field data (SDDS_GetParameterInformation)");
      return (0);
    }
  } else
    memcpy(memory, (char *)parameterdef + SDDS_ParameterFieldInformation[field_index].offset, SDDS_type_size[type - 1]);
  return (type);
}

/**
 * @brief Retrieves information about a specified array in the SDDS dataset.
 *
 * This function is the preferred alternative to `SDDS_GetArrayDefinition`. It allows you to obtain information about a specific field of an array, either by the array's name or index.
 *
 * @param[in] SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in] field_name   A null-terminated string specifying the name of the field for which information is requested.
 * @param[out] memory      Pointer to a variable where the retrieved information will be stored. The variable should be of type `data_type*`, where `data_type` corresponds to the type of the requested information. For `STRING` information, use `char*`. If `memory` is `NULL`, the function will verify the existence and type of the information, returning the data type without storing any data.
 * @param[in] mode         Specifies how to identify the array. Valid values are:
 *                          - `SDDS_GET_BY_NAME`: Identify the array by its name. Requires an additional argument of type `char*` (array name).
 *                          - `SDDS_GET_BY_INDEX`: Identify the array by its index. Requires an additional argument of type `int32_t` (array index).
 *
 * @return On success, returns the SDDS data type of the requested information. On failure, returns zero and records an error message.
 *
 * @note This function uses variable arguments to accept either the array name or index based on the `mode` parameter.
 *
 * @see SDDS_GetArrayDefinition
 */
int32_t SDDS_GetArrayInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...) {
  int32_t field_index, type, array_index;
  ARRAY_DEFINITION *arraydef;
  char *array_name;
  va_list argptr;
  int32_t retval;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetArrayInformation"))
    return (0);

  if (!field_name) {
    SDDS_SetError("NULL field name passed. (SDDS_GetArrayInformation)");
    return (0);
  }

  va_start(argptr, mode);
  retval = 1;
  if (mode & SDDS_GET_BY_INDEX) {
    if ((array_index = va_arg(argptr, int32_t)) < 0 || array_index >= SDDS_dataset->layout.n_arrays) {
      SDDS_SetError("Invalid array index passed. (SDDS_GetArrayInformation)");
      retval = 0;
    }
  } else {
    if (!(array_name = va_arg(argptr, char *))) {
      SDDS_SetError("NULL array name passed. (SDDS_GetArrayInformation)");
      retval = 0;
    }
    if ((array_index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
      SDDS_SetError("Unknown array name given (SDDS_GetArrayInformation)");
      retval = 0;
    }
  }
  arraydef = SDDS_dataset->layout.array_definition + array_index;
  va_end(argptr);
  if (!retval)
    return (0);

  for (field_index = 0; field_index < SDDS_ARRAY_FIELDS; field_index++)
    if (strcmp(field_name, SDDS_ArrayFieldInformation[field_index].name) == 0)
      break;
  if (field_index == SDDS_ARRAY_FIELDS) {
    SDDS_SetError("Unknown field name given (SDDS_GetArrayInformation)");
    return (0);
  }
  type = SDDS_ArrayFieldInformation[field_index].type;
  if (!memory)
    return (type);
  if (type == SDDS_STRING) {
    if (!SDDS_CopyString((char **)memory, *((char **)((char *)arraydef + SDDS_ArrayFieldInformation[field_index].offset)))) {
      SDDS_SetError("Unable to copy field data (SDDS_GetArrayInformation)");
      return (0);
    }
  } else
    memcpy(memory, (char *)arraydef + SDDS_ArrayFieldInformation[field_index].offset, SDDS_type_size[type - 1]);
  return (type);
}

/**
 * @brief Retrieves information about a specified associate in the SDDS dataset.
 *
 * This function allows you to obtain information about a specific field of an associate, either by the associate's name or index.
 *
 * @param[in] SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in] field_name   A null-terminated string specifying the name of the field for which information is requested.
 * @param[out] memory      Pointer to a variable where the retrieved information will be stored. The variable should be of type `data_type*`, where `data_type` corresponds to the type of the requested information. For `STRING` information, use `char*`. If `memory` is `NULL`, the function will verify the existence and type of the information, returning the data type without storing any data.
 * @param[in] mode         Specifies how to identify the associate. Valid values are:
 *                          - `SDDS_GET_BY_NAME`: Identify the associate by its name. Requires an additional argument of type `char*` (associate name).
 *                          - `SDDS_GET_BY_INDEX`: Identify the associate by its index. Requires an additional argument of type `int32_t` (associate index).
 *
 * @return On success, returns the SDDS data type of the requested information. On failure, returns zero and records an error message.
 *
 * @note This function uses variable arguments to accept either the associate name or index based on the `mode` parameter.
 *
 * @see SDDS_GetAssociateDefinition
 */
int32_t SDDS_GetAssociateInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...) {
  int32_t field_index, type, associate_index;
  ASSOCIATE_DEFINITION *associatedef;
  char *associate_name;
  va_list argptr;
  int32_t retval;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetAssociateInformation"))
    return (0);

  if (!field_name) {
    SDDS_SetError("NULL field name passed. (SDDS_GetAssociateInformation)");
    return (0);
  }

  va_start(argptr, mode);
  retval = 1;
  if (mode & SDDS_GET_BY_INDEX) {
    if ((associate_index = va_arg(argptr, int32_t)) < 0 || associate_index >= SDDS_dataset->layout.n_associates) {
      SDDS_SetError("Invalid associate index passed. (SDDS_GetAssociateInformation)");
      retval = 0;
    }
  } else {
    if (!(associate_name = va_arg(argptr, char *))) {
      SDDS_SetError("NULL associate name passed. (SDDS_GetAssociateInformation)");
      retval = 0;
    }
    if ((associate_index = SDDS_GetAssociateIndex(SDDS_dataset, associate_name)) < 0) {
      SDDS_SetError("Unknown associate name given (SDDS_GetAssociateInformation)");
      retval = 0;
    }
  }
  associatedef = SDDS_dataset->layout.associate_definition + associate_index;
  va_end(argptr);
  if (!retval)
    return (0);

  for (field_index = 0; field_index < SDDS_ASSOCIATE_FIELDS; field_index++)
    if (strcmp(field_name, SDDS_AssociateFieldInformation[field_index].name) == 0)
      break;
  if (field_index == SDDS_ASSOCIATE_FIELDS) {
    SDDS_SetError("Unknown field name given (SDDS_GetAssociateInformation)");
    return (0);
  }
  type = SDDS_AssociateFieldInformation[field_index].type;
  if (!memory)
    return (type);
  if (type == SDDS_STRING) {
    if (!SDDS_CopyString((char **)memory, *((char **)((char *)associatedef + SDDS_AssociateFieldInformation[field_index].offset)))) {
      SDDS_SetError("Unable to copy field data (SDDS_GetAssociateInformation)");
      return (0);
    }
  } else
    memcpy(memory, (char *)associatedef + SDDS_AssociateFieldInformation[field_index].offset, SDDS_type_size[type - 1]);
  return (type);
}

/**
 * @brief Modifies a specific field in a column definition within the SDDS dataset.
 *
 * This function allows you to change a particular field of a column definition, identified either by its name or index. The new value for the field can be provided either as a direct value or as a string, depending on the field type.
 *
 * @param[in]      SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]      field_name   A null-terminated string specifying the name of the field to be modified.
 * @param[in]      memory       Pointer to the new value for the field. The type of this pointer should correspond to the data type of the field being modified:
 *                                - For non-string fields, provide a pointer to the appropriate data type (e.g., `int32_t*`, `double*`).
 *                                - For string fields, provide a `char*`.
 * @param[in]      mode         A bitwise combination of the following constants to specify how to identify the column and how to pass the new value:
 *                                - `SDDS_SET_BY_INDEX`: Identify the column by its index. Requires an additional argument of type `int32_t` (column index).
 *                                - `SDDS_SET_BY_NAME`: Identify the column by its name. Requires an additional argument of type `char*` (column name).
 *                                - `SDDS_PASS_BY_VALUE`: The new value is provided as a direct value (non-string fields).
 *                                - `SDDS_PASS_BY_STRING`: The new value is provided as a string (string fields).
 *
 * The valid combinations of `mode` are:
 * - `SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE`:
 *   ```c
 *   int32_t SDDS_ChangeColumnInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, int32_t column_index);
 *   ```
 * - `SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE`:
 *   ```c
 *   int32_t SDDS_ChangeColumnInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, char *column_name);
 *   ```
 * - `SDDS_SET_BY_INDEX | SDDS_PASS_BY_STRING`:
 *   ```c
 *   int32_t SDDS_ChangeColumnInformation(SDDS_DATASET *SDDS_dataset, char *field_name, char *memory, int32_t mode, int32_t column_index);
 *   ```
 * - `SDDS_SET_BY_NAME | SDDS_PASS_BY_STRING`:
 *   ```c
 *   int32_t SDDS_ChangeColumnInformation(SDDS_DATASET *SDDS_dataset, char *field_name, char *memory, int32_t mode, char *column_name);
 *   ```
 *
 * @return On success, returns the SDDS data type of the modified information. On failure, returns zero and records an error message.
 *
 * @note This function uses variable arguments to accept either the column name or index based on the `mode` parameter.
 *
 * @see SDDS_GetColumnInformation
 */
int32_t SDDS_ChangeColumnInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...) {
  int32_t field_index, type, givenType;
  int32_t i, column_index;
  COLUMN_DEFINITION *columndef;
  char *column_name;
  va_list argptr;
  int32_t retval;
  double buffer[4];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ChangeColumnInformation"))
    return (0);

  if (!field_name) {
    SDDS_SetError("NULL field name passed. (SDDS_ChangeColumnInformation)");
    return (0);
  }

  va_start(argptr, mode);
  retval = 1;
  if (mode & SDDS_SET_BY_INDEX) {
    if ((column_index = va_arg(argptr, int32_t)) < 0 || column_index >= SDDS_dataset->layout.n_columns) {
      SDDS_SetError("Invalid column index passed. (SDDS_ChangeColumnInformation)");
      retval = 0;
    }
  } else {
    if (!(column_name = va_arg(argptr, char *))) {
      SDDS_SetError("NULL column name passed. (SDDS_ChangeColumnInformation)");
      retval = 0;
    }
    if ((column_index = SDDS_GetColumnIndex(SDDS_dataset, column_name)) < 0) {
      SDDS_SetError("Unknown column name given (SDDS_ChangeColumnInformation)");
      retval = 0;
    }
  }
  columndef = SDDS_dataset->layout.column_definition + column_index;
  va_end(argptr);
  if (!retval)
    return (0);

  for (field_index = 0; field_index < SDDS_COLUMN_FIELDS; field_index++)
    if (strcmp(field_name, SDDS_ColumnFieldInformation[field_index].name) == 0)
      break;
  if (field_index == SDDS_COLUMN_FIELDS) {
    SDDS_SetError("Unknown field name given (SDDS_ChangeColumnInformation)");
    return (0);
  }
  type = SDDS_ColumnFieldInformation[field_index].type;
  if (!memory)
    return (type);
  if (type == SDDS_STRING) {
    if (!SDDS_CopyString(((char **)((char *)columndef + SDDS_ColumnFieldInformation[field_index].offset)), (char *)memory)) {
      SDDS_SetError("Unable to copy field data (SDDS_ChangeColumnInformation)");
      return (0);
    }
    if (strcmp(field_name, "name") == 0) {
      for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
        if (column_index == SDDS_dataset->layout.column_index[i]->index)
          break;
      if (i == SDDS_dataset->layout.n_columns) {
        SDDS_SetError("Unable to copy field data--column indexing problem (SDDS_ChangeColumnInformation)");
        return (0);
      }
      SDDS_dataset->layout.column_index[i]->name = SDDS_dataset->layout.column_definition[column_index].name;
      qsort((char *)SDDS_dataset->layout.column_index, SDDS_dataset->layout.n_columns, sizeof(*SDDS_dataset->layout.column_index), SDDS_CompareIndexedNamesPtr);
    }
  } else {
    if (mode & SDDS_PASS_BY_STRING) {
      if (strcmp(field_name, "type") == 0 && (givenType = SDDS_IdentifyType((char *)memory)) > 0)
        /* the type has been passed as a string (e.g., "double") */
        memcpy((char *)buffer, (char *)&givenType, sizeof(givenType));
      else if (!SDDS_ScanData((char *)memory, type, 0, (void *)buffer, 0, 0)) {
        SDDS_SetError("Unable to scan string data (SDDS_ChangeColumnInformation)");
        return (0);
      }
      memcpy((char *)columndef + SDDS_ColumnFieldInformation[field_index].offset, (void *)buffer, SDDS_type_size[type - 1]);
    } else
      memcpy((char *)columndef + SDDS_ColumnFieldInformation[field_index].offset, memory, SDDS_type_size[type - 1]);
  }
  return (type);
}

/**
 * @brief Modifies a specific field in a parameter definition within the SDDS dataset.
 *
 * This function allows you to change a particular field of a parameter definition, identified either by its name or index. The new value for the field can be provided either as a direct value or as a string, depending on the field type.
 *
 * @param[in]      SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]      field_name   A null-terminated string specifying the name of the field to be modified.
 * @param[in]      memory       Pointer to the new value for the field. The type of this pointer should correspond to the data type of the field being modified:
 *                                - For non-string fields, provide a pointer to the appropriate data type (e.g., `int32_t*`, `double*`).
 *                                - For string fields, provide a `char*`.
 * @param[in]      mode         A bitwise combination of the following constants to specify how to identify the parameter and how to pass the new value:
 *                                - `SDDS_SET_BY_INDEX`: Identify the parameter by its index. Requires an additional argument of type `int32_t` (parameter index).
 *                                - `SDDS_SET_BY_NAME`: Identify the parameter by its name. Requires an additional argument of type `char*` (parameter name).
 *                                - `SDDS_PASS_BY_VALUE`: The new value is provided as a direct value (non-string fields).
 *                                - `SDDS_PASS_BY_STRING`: The new value is provided as a string (string fields).
 *
 * The valid combinations of `mode` are:
 * - `SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE`:
 *   ```c
 *   int32_t SDDS_ChangeParameterInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, int32_t parameter_index);
 *   ```
 * - `SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE`:
 *   ```c
 *   int32_t SDDS_ChangeParameterInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, char *parameter_name);
 *   ```
 * - `SDDS_SET_BY_INDEX | SDDS_PASS_BY_STRING`:
 *   ```c
 *   int32_t SDDS_ChangeParameterInformation(SDDS_DATASET *SDDS_dataset, char *field_name, char *memory, int32_t mode, int32_t parameter_index);
 *   ```
 * - `SDDS_SET_BY_NAME | SDDS_PASS_BY_STRING`:
 *   ```c
 *   int32_t SDDS_ChangeParameterInformation(SDDS_DATASET *SDDS_dataset, char *field_name, char *memory, int32_t mode, char *parameter_name);
 *   ```
 *
 * @return On success, returns the SDDS data type of the modified information. On failure, returns zero and records an error message.
 *
 * @note This function uses variable arguments to accept either the parameter name or index based on the `mode` parameter.
 *
 * @see SDDS_GetParameterInformation
 */
int32_t SDDS_ChangeParameterInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...) {
  int32_t field_index, type, parameter_index, givenType;
  PARAMETER_DEFINITION *parameterdef;
  char *parameter_name;
  va_list argptr;
  int32_t retval;
  double buffer[4];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ChangeParameterInformation"))
    return (0);

  if (!field_name) {
    SDDS_SetError("NULL field name passed. (SDDS_ChangeParameterInformation)");
    return (0);
  }

  va_start(argptr, mode);
  retval = 1;
  if (mode & SDDS_SET_BY_INDEX) {
    if ((parameter_index = va_arg(argptr, int32_t)) < 0 || parameter_index >= SDDS_dataset->layout.n_parameters) {
      SDDS_SetError("Invalid parameter index passed. (SDDS_ChangeParameterInformation)");
      retval = 0;
    }
  } else {
    if (!(parameter_name = va_arg(argptr, char *))) {
      SDDS_SetError("NULL parameter name passed. (SDDS_ChangeParameterInformation)");
      retval = 0;
    }
    if ((parameter_index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
      SDDS_SetError("Unknown parameter name given (SDDS_ChangeParameterInformation)");
      retval = 0;
    }
  }
  parameterdef = SDDS_dataset->layout.parameter_definition + parameter_index;
  va_end(argptr);
  if (!retval)
    return (0);

  for (field_index = 0; field_index < SDDS_PARAMETER_FIELDS; field_index++)
    if (strcmp(field_name, SDDS_ParameterFieldInformation[field_index].name) == 0)
      break;
  if (field_index == SDDS_PARAMETER_FIELDS) {
    SDDS_SetError("Unknown field name given (SDDS_ChangeParameterInformation)");
    return (0);
  }
  type = SDDS_ParameterFieldInformation[field_index].type;
  if (!memory)
    return (type);
  if (type == SDDS_STRING) {
    if (!SDDS_CopyString(((char **)((char *)parameterdef + SDDS_ParameterFieldInformation[field_index].offset)), (char *)memory)) {
      SDDS_SetError("Unable to copy field data (SDDS_ChangeParameterInformation)");
      return (0);
    }
    if (strcmp(field_name, "name") == 0)
      qsort((char *)SDDS_dataset->layout.parameter_index, SDDS_dataset->layout.n_parameters, sizeof(*SDDS_dataset->layout.parameter_index), SDDS_CompareIndexedNamesPtr);
  } else {
    if (mode & SDDS_PASS_BY_STRING) {
      if (strcmp(field_name, "type") == 0 && (givenType = SDDS_IdentifyType((char *)memory)) > 0)
        /* the type has been passed as a string (e.g., "double") */
        memcpy((char *)buffer, (char *)&givenType, sizeof(givenType));
      else if (!SDDS_ScanData((char *)memory, type, 0, (void *)buffer, 0, 0)) {
        SDDS_SetError("Unable to scan string data (SDDS_ChangeParameterInformation)");
        return (0);
      }
      memcpy((char *)parameterdef + SDDS_ParameterFieldInformation[field_index].offset, (void *)buffer, SDDS_type_size[type - 1]);
    } else
      memcpy((char *)parameterdef + SDDS_ParameterFieldInformation[field_index].offset, memory, SDDS_type_size[type - 1]);
  }

  return (type);
}

/**
 * @brief Modifies a specific field in an array definition within the SDDS dataset.
 *
 * This function allows you to change a particular field of an array definition, identified either by its name or index. The new value for the field can be provided either as a direct value or as a string, depending on the field type.
 *
 * @param[in]      SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]      field_name   A null-terminated string specifying the name of the field to be modified.
 * @param[in]      memory       Pointer to the new value for the field. The type of this pointer should correspond to the data type of the field being modified:
 *                                - For non-string fields, provide a pointer to the appropriate data type (e.g., `int32_t*`, `double*`).
 *                                - For string fields, provide a `char*`.
 * @param[in]      mode         A bitwise combination of the following constants to specify how to identify the array and how to pass the new value:
 *                                - `SDDS_SET_BY_INDEX`: Identify the array by its index. Requires an additional argument of type `int32_t` (array index).
 *                                - `SDDS_SET_BY_NAME`: Identify the array by its name. Requires an additional argument of type `char*` (array name).
 *                                - `SDDS_PASS_BY_VALUE`: The new value is provided as a direct value (non-string fields).
 *                                - `SDDS_PASS_BY_STRING`: The new value is provided as a string (string fields).
 *
 * The valid combinations of `mode` are:
 * - `SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE`:
 *   ```c
 *   int32_t SDDS_ChangeArrayInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, int32_t array_index);
 *   ```
 * - `SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE`:
 *   ```c
 *   int32_t SDDS_ChangeArrayInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, char *array_name);
 *   ```
 * - `SDDS_SET_BY_INDEX | SDDS_PASS_BY_STRING`:
 *   ```c
 *   int32_t SDDS_ChangeArrayInformation(SDDS_DATASET *SDDS_dataset, char *field_name, char *memory, int32_t mode, int32_t array_index);
 *   ```
 * - `SDDS_SET_BY_NAME | SDDS_PASS_BY_STRING`:
 *   ```c
 *   int32_t SDDS_ChangeArrayInformation(SDDS_DATASET *SDDS_dataset, char *field_name, char *memory, int32_t mode, char *array_name);
 *   ```
 *
 * @return On success, returns the SDDS data type of the modified information. On failure, returns zero and records an error message.
 *
 * @note This function uses variable arguments to accept either the array name or index based on the `mode` parameter.
 *
 * @see SDDS_GetArrayInformation
 */
int32_t SDDS_ChangeArrayInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...) {
  int32_t field_index, type, array_index, givenType;
  ARRAY_DEFINITION *arraydef;
  char *array_name;
  va_list argptr;
  int32_t retval;
  double buffer[4];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ChangeArrayInformation"))
    return (0);

  if (!field_name) {
    SDDS_SetError("NULL field name passed. (SDDS_ChangeArrayInformation)");
    return (0);
  }

  va_start(argptr, mode);
  retval = 1;
  if (mode & SDDS_SET_BY_INDEX) {
    if ((array_index = va_arg(argptr, int32_t)) < 0 || array_index >= SDDS_dataset->layout.n_arrays) {
      SDDS_SetError("Invalid array index passed. (SDDS_ChangeArrayInformation)");
      retval = 0;
    }
  } else {
    if (!(array_name = va_arg(argptr, char *))) {
      SDDS_SetError("NULL array name passed. (SDDS_ChangeArrayInformation)");
      retval = 0;
    }
    if ((array_index = SDDS_GetArrayIndex(SDDS_dataset, array_name)) < 0) {
      SDDS_SetError("Unknown array name given (SDDS_ChangeArrayInformation)");
      retval = 0;
    }
  }
  arraydef = SDDS_dataset->layout.array_definition + array_index;
  va_end(argptr);
  if (!retval)
    return (0);

  for (field_index = 0; field_index < SDDS_ARRAY_FIELDS; field_index++)
    if (strcmp(field_name, SDDS_ArrayFieldInformation[field_index].name) == 0)
      break;
  if (field_index == SDDS_ARRAY_FIELDS) {
    SDDS_SetError("Unknown field name given (SDDS_ChangeArrayInformation)");
    return (0);
  }
  type = SDDS_ArrayFieldInformation[field_index].type;
  if (!memory)
    return (type);
  if (type == SDDS_STRING) {
    if (!SDDS_CopyString(((char **)((char *)arraydef + SDDS_ArrayFieldInformation[field_index].offset)), (char *)memory)) {
      SDDS_SetError("Unable to copy field data (SDDS_ChangeArrayInformation)");
      return (0);
    }
    if (strcmp(field_name, "name") == 0)
      qsort((char *)SDDS_dataset->layout.array_index, SDDS_dataset->layout.n_arrays, sizeof(*SDDS_dataset->layout.array_index), SDDS_CompareIndexedNamesPtr);
  } else {
    if (mode & SDDS_PASS_BY_STRING) {
      if (strcmp(field_name, "type") == 0 && (givenType = SDDS_IdentifyType((char *)memory)) > 0)
        /* the type has been passed as a string (e.g., "double") */
        memcpy((char *)buffer, (char *)&givenType, sizeof(givenType));
      else if (!SDDS_ScanData((char *)memory, type, 0, (void *)buffer, 0, 0)) {
        SDDS_SetError("Unable to scan string data (SDDS_ChangeArrayInformation)");
        return (0);
      }
      memcpy((char *)arraydef + SDDS_ArrayFieldInformation[field_index].offset, (void *)buffer, SDDS_type_size[type - 1]);
    } else
      memcpy((char *)arraydef + SDDS_ArrayFieldInformation[field_index].offset, memory, SDDS_type_size[type - 1]);
  }

  return (type);
}
