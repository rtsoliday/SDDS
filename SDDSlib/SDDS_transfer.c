/**
 * @file SDDS_transfer.c
 * @brief This file provides functions for transferring definitions from one SDDS dataset to another.
 *
 * This file provides functions for transferring definitions from one SDDS dataset to another.
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

#include "mdb.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDS_internal.h"

/**
 * @brief Transfers a column definition from a source dataset to a target dataset.
 *
 * This function defines a column in the target SDDS dataset to match the definition of a column in the source SDDS dataset.
 *
 * @param target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param name The name of the column in the source dataset to be transferred.
 * @param newName The name of the column in the target dataset. If `NULL`, the original `name` is used.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_TransferColumnDefinition(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName) {
  COLUMN_DEFINITION *coldef;

  if (!name || SDDS_StringIsBlank(name)) {
    SDDS_SetError("Unable to transfer column definition--NULL or blank name passed (SDDS_TransferColumnDefinition)");
    return 0;
  }
  if (!newName)
    newName = name;
  if (!(coldef = SDDS_GetColumnDefinition(source, name))) {
    SDDS_SetError("Unable to transfer column definition--unknown column named (SDDS_TransferColumnDefinition)");
    return 0;
  }
  if (SDDS_GetColumnIndex(target, newName) >= 0) {
    SDDS_SetError("Unable to transfer column definition--column already present (SDDS_TransferColumnDefinition)");
    return 0;
  }
  if (SDDS_DefineColumn(target, newName, coldef->symbol, coldef->units, coldef->description, coldef->format_string, coldef->type, coldef->field_length) < 0) {
    SDDS_FreeColumnDefinition(coldef);
    SDDS_SetError("Unable to transfer column definition--call to define column failed (SDDS_TransferColumnDefinition)");
    return 0;
  }
  SDDS_FreeColumnDefinition(coldef);
  return 1;
}

/**
 * @brief Transfers a parameter definition from a source dataset to a target dataset.
 *
 * This function defines a parameter in the target SDDS dataset to match the definition of a parameter in the source SDDS dataset.
 *
 * @param target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param name The name of the parameter in the source dataset to be transferred.
 * @param newName The name of the parameter in the target dataset. If `NULL`, the original `name` is used.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_TransferParameterDefinition(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName) {
  PARAMETER_DEFINITION *pardef;

  if (!name || SDDS_StringIsBlank(name)) {
    SDDS_SetError("Unable to transfer parameter definition--NULL or blank name passed (SDDS_TransferParameterDefinition)");
    return 0;
  }
  if (!newName)
    newName = name;
  if (!(pardef = SDDS_GetParameterDefinition(source, name))) {
    SDDS_SetError("Unable to transfer parameter definition--unknown parameter named (SDDS_TransferParameterDefinition)");
    return 0;
  }
  if (SDDS_GetParameterIndex(target, newName) >= 0) {
    SDDS_SetError("Unable to transfer parameter definition--parameter already present (SDDS_TransferParameterDefinition)");
    return 0;
  }
  if (SDDS_DefineParameter(target, newName, pardef->symbol, pardef->units, pardef->description, pardef->format_string, pardef->type, NULL) < 0) {
    SDDS_FreeParameterDefinition(pardef);
    SDDS_SetError("Unable to transfer parameter definition--call to define parameter failed (SDDS_TransferParameterDefinition)");
    return 0;
  }
  SDDS_FreeParameterDefinition(pardef);
  return 1;
}

/**
 * @brief Transfers an array definition from a source dataset to a target dataset.
 *
 * This function defines an array in the target SDDS dataset to match the definition of an array in the source SDDS dataset.
 *
 * @param target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param name The name of the array in the source dataset to be transferred.
 * @param newName The name of the array in the target dataset. If `NULL`, the original `name` is used.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_TransferArrayDefinition(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName) {
  ARRAY_DEFINITION *ardef;

  if (!name || SDDS_StringIsBlank(name)) {
    SDDS_SetError("Unable to transfer array definition--NULL or blank name passed (SDDS_TransferArrayDefinition)");
    return 0;
  }
  if (!newName)
    newName = name;
  if (!(ardef = SDDS_GetArrayDefinition(source, name))) {
    SDDS_SetError("Unable to transfer array definition--unknown array named (SDDS_TransferArrayDefinition)");
    return 0;
  }
  if (SDDS_GetArrayIndex(target, newName) >= 0) {
    SDDS_SetError("Unable to transfer array definition--array already present (SDDS_TransferArrayDefinition)");
    return 0;
  }
  if (SDDS_DefineArray(target, newName, ardef->symbol, ardef->units, ardef->description, ardef->format_string, ardef->type, ardef->field_length, ardef->dimensions, ardef->group_name) < 0) {
    SDDS_FreeArrayDefinition(ardef);
    SDDS_SetError("Unable to transfer array definition--call to define array failed (SDDS_TransferArrayDefinition)");
    return 0;
  }
  SDDS_FreeArrayDefinition(ardef);
  return 1;
}

/**
 * @brief Transfers an associate definition from a source dataset to a target dataset.
 *
 * This function defines an associate in the target SDDS dataset to match the definition of an associate in the source SDDS dataset.
 *
 * @param target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param name The name of the associate in the source dataset to be transferred.
 * @param newName The name of the associate in the target dataset. If `NULL`, the original `name` is used.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_TransferAssociateDefinition(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName) {
  ASSOCIATE_DEFINITION *asdef;

  if (!name || SDDS_StringIsBlank(name)) {
    SDDS_SetError("Unable to transfer associate definition--NULL or blank name passed (SDDS_TransferAssociateDefinition)");
    return 0;
  }
  if (!newName)
    newName = name;
  if ((asdef = SDDS_GetAssociateDefinition(target, name))) {
    SDDS_FreeAssociateDefinition(asdef);
    SDDS_SetError("Unable to transfer associate definition--associate already present (SDDS_TransferAssociateDefinition)");
    return 0;
  }
  if (!(asdef = SDDS_GetAssociateDefinition(source, newName))) {
    SDDS_SetError("Unable to transfer associate definition--unknown associate named (SDDS_TransferAssociateDefinition)");
    return 0;
  }
  if (SDDS_DefineAssociate(target, newName, asdef->filename, asdef->path, asdef->description, asdef->contents, asdef->sdds) < 0) {
    SDDS_FreeAssociateDefinition(asdef);
    SDDS_SetError("Unable to transfer associate definition--call to define associate failed (SDDS_TransferAssociateDefinition)");
    return 0;
  }
  SDDS_FreeAssociateDefinition(asdef);
  return 1;
}

/**
 * @brief Defines a parameter in the target dataset based on a column definition from the source dataset.
 *
 * This function creates a parameter in the target SDDS dataset with properties matching those of a specified column in the source dataset.
 *
 * @param target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param name The name of the column in the source dataset whose definition is to be used.
 * @param newName The name of the parameter in the target dataset. If `NULL`, the original `name` is used.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_DefineParameterLikeColumn(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName) {
  COLUMN_DEFINITION *coldef;

  if (!name || SDDS_StringIsBlank(name)) {
    SDDS_SetError("Unable to define parameter--NULL or blank name passed (SDDS_DefineParameterLikeColumn)");
    return 0;
  }
  if (!newName)
    newName = name;
  if (!(coldef = SDDS_GetColumnDefinition(source, name))) {
    SDDS_SetError("Unable to define parameter--unknown column named (SDDS_DefineParameterLikeColumn)");
    return 0;
  }
  if (SDDS_GetParameterIndex(target, newName) >= 0) {
    SDDS_SetError("Unable to define parameter--already exists (SDDS_DefineParameterLikeColumn)");
    return 0;
  }
  if (SDDS_DefineParameter(target, newName, coldef->symbol, coldef->units, coldef->description, coldef->format_string, coldef->type, NULL) < 0) {
    SDDS_FreeColumnDefinition(coldef);
    SDDS_SetError("Unable to define parameter--call to define parameter failed (SDDS_DefineParameterLikeColumn)");
    return 0;
  }
  SDDS_FreeColumnDefinition(coldef);
  return 1;
}

/**
 * @brief Defines a parameter in the target dataset based on an array definition from the source dataset.
 *
 * This function creates a parameter in the target SDDS dataset with properties matching those of a specified array in the source dataset.
 *
 * @param target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param name The name of the array in the source dataset whose definition is to be used.
 * @param newName The name of the parameter in the target dataset. If `NULL`, the original `name` is used.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_DefineParameterLikeArray(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName) {
  ARRAY_DEFINITION *arrayDef;

  if (!name || SDDS_StringIsBlank(name)) {
    SDDS_SetError("Unable to define parameter--NULL or blank name passed (SDDS_DefineParameterLikeArray)");
    return 0;
  }
  if (!newName)
    newName = name;
  if (!(arrayDef = SDDS_GetArrayDefinition(source, name))) {
    SDDS_SetError("Unable to define parameter--unknown array named (SDDS_DefineParameterLikeArray)");
    return 0;
  }
  if (SDDS_GetParameterIndex(target, newName) >= 0) {
    SDDS_SetError("Unable to define parameter--already exists (SDDS_DefineParameterLikeArray)");
    return 0;
  }
  if (SDDS_DefineParameter(target, newName, arrayDef->symbol, arrayDef->units, arrayDef->description, arrayDef->format_string, arrayDef->type, NULL) < 0) {
    SDDS_FreeArrayDefinition(arrayDef);
    SDDS_SetError("Unable to define parameter--call to define parameter failed (SDDS_DefineParameterLikeArray)");
    return 0;
  }
  SDDS_FreeArrayDefinition(arrayDef);
  return 1;
}

/**
 * @brief Defines a column in the target dataset based on a parameter definition from the source dataset.
 *
 * This function creates a column in the target SDDS dataset with properties matching those of a specified parameter in the source dataset.
 *
 * @param target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param name The name of the parameter in the source dataset whose definition is to be used.
 * @param newName The name of the column in the target dataset. If `NULL`, the original `name` is used.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_DefineColumnLikeParameter(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName) {
  PARAMETER_DEFINITION *pardef;

  if (!name || SDDS_StringIsBlank(name)) {
    SDDS_SetError("Unable to define column--NULL or blank name passed (SDDS_DefineColumnLikeParameter)");
    return 0;
  }
  if (!newName)
    newName = name;
  if (!(pardef = SDDS_GetParameterDefinition(source, name))) {
    SDDS_SetError("Unable to define column--unknown parameter named (SDDS_DefineColumnLikeParameter)");
    return 0;
  }
  if (SDDS_GetColumnIndex(target, newName) >= 0) {
    SDDS_SetError("Unable to define column--already exists (SDDS_DefineColumnLikeParameter)");
    return 0;
  }
  if (SDDS_DefineColumn(target, newName, pardef->symbol, pardef->units, pardef->description, pardef->format_string, pardef->type, 0) < 0) {
    SDDS_FreeParameterDefinition(pardef);
    SDDS_SetError("Unable to define column--call to define column failed (SDDS_DefineColumnLikeParameter)");
    return 0;
  }
  SDDS_FreeParameterDefinition(pardef);
  return 1;
}

/**
 * @brief Defines a column in the target dataset based on an array definition from the source dataset.
 *
 * This function creates a column in the target SDDS dataset with properties matching those of a specified array in the source dataset.
 *
 * @param target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param name The name of the array in the source dataset whose definition is to be used.
 * @param newName The name of the column in the target dataset. If `NULL`, the original `name` is used.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_DefineColumnLikeArray(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName) {
  ARRAY_DEFINITION *arrayDef;

  if (!name || SDDS_StringIsBlank(name)) {
    SDDS_SetError("Unable to define column--NULL or blank name passed (SDDS_DefineColumnLikeArray)");
    return 0;
  }
  if (!newName)
    newName = name;
  if (!(arrayDef = SDDS_GetArrayDefinition(source, name))) {
    SDDS_SetError("Unable to define column--unknown array named (SDDS_DefineColumnLikeArray)");
    return 0;
  }
  if (SDDS_GetColumnIndex(target, newName) >= 0) {
    SDDS_SetError("Unable to define column--already exists (SDDS_DefineColumnLikeArray)");
    return 0;
  }
  if (SDDS_DefineColumn(target, newName, arrayDef->symbol, arrayDef->units, arrayDef->description, arrayDef->format_string, arrayDef->type, 0) < 0) {
    SDDS_FreeArrayDefinition(arrayDef);
    SDDS_SetError("Unable to define column--call to define column failed (SDDS_DefineColumnLikeArray)");
    return 0;
  }
  SDDS_FreeArrayDefinition(arrayDef);
  return 1;
}

/**
 * @brief Transfers all parameter definitions from a source dataset to a target dataset.
 *
 * This function defines all parameters in the target SDDS dataset to match the parameter definitions in the source SDDS dataset.
 * It handles existing parameters based on the specified mode.
 *
 * @param SDDS_target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param SDDS_source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param mode Flags that determine how to handle existing parameters. Valid flags include:
 *             - `0`: Error if a parameter already exists in the target dataset.
 *             - `SDDS_TRANSFER_KEEPOLD`: Retain existing parameters in the target dataset and skip transferring conflicting parameters.
 *             - `SDDS_TRANSFER_OVERWRITE`: Overwrite existing parameters in the target dataset with definitions from the source dataset.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_TransferAllParameterDefinitions(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, uint32_t mode) {
  SDDS_LAYOUT *target, *source;
  int32_t i, index;
  char messBuffer[1024];

  if (!SDDS_CheckDataset(SDDS_target, "SDDS_TransferAllParameterDefinitions"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_TransferAllParameterDefinitions"))
    return (0);
  if (mode & SDDS_TRANSFER_KEEPOLD && mode & SDDS_TRANSFER_OVERWRITE) {
    SDDS_SetError("Inconsistent mode flags (SDDS_TransferAllParameterDefinitions)");
    return 0;
  }
  target = &SDDS_target->layout;
  source = &SDDS_source->layout;
  SDDS_DeferSavingLayout(SDDS_target, 1);
  for (i = 0; i < source->n_parameters; i++) {
    if ((index = SDDS_GetParameterIndex(SDDS_target, source->parameter_definition[i].name)) >= 0) {
      /* already exists */
      if (mode & SDDS_TRANSFER_KEEPOLD)
        continue;
      if (!(mode & SDDS_TRANSFER_OVERWRITE)) {
        sprintf(messBuffer, "Unable to define parameter %s---already exists (SDDS_TransferAllParameterDefinitions)", source->parameter_definition[i].name);
        SDDS_SetError(messBuffer);
        SDDS_DeferSavingLayout(SDDS_target, 0);
        return 0;
      }
      if (!SDDS_ChangeParameterInformation(SDDS_target, "symbol",
                                           &source->parameter_definition[i].symbol,
                                           SDDS_BY_INDEX, index) ||
          !SDDS_ChangeParameterInformation(SDDS_target, "units",
                                           &source->parameter_definition[i].units,
                                           SDDS_BY_INDEX, index) ||
          !SDDS_ChangeParameterInformation(SDDS_target, "description",
                                           &source->parameter_definition[i].description,
                                           SDDS_BY_INDEX, index) ||
          !SDDS_ChangeParameterInformation(SDDS_target, "format_string",
                                           &source->parameter_definition[i].format_string,
                                           SDDS_BY_INDEX, index) ||
          !SDDS_ChangeParameterInformation(SDDS_target, "type",
                                           &source->parameter_definition[i].type, SDDS_BY_INDEX, index) ||
          (source->parameter_definition[i].fixed_value != NULL && !SDDS_ChangeParameterInformation(SDDS_target, "fixed_value", &source->parameter_definition[i].fixed_value, SDDS_BY_INDEX, index))) {
        SDDS_SetError("Unable to define parameter---problem with overwrite (SDDS_TransferAllParameterDefinitions)");
        SDDS_DeferSavingLayout(SDDS_target, 0);
        return 0;
      }
      if (source->parameter_definition[i].fixed_value == NULL)
        target->parameter_definition[index].fixed_value = NULL;
      target->parameter_definition[index].definition_mode = source->parameter_definition[index].definition_mode;
      if (target->parameter_definition[index].type == SDDS_STRING)
        target->parameter_definition[index].memory_number = SDDS_CreateRpnMemory(source->parameter_definition[i].name, 1);
      else
        target->parameter_definition[index].memory_number = SDDS_CreateRpnMemory(source->parameter_definition[i].name, 0);
    } else {
      if (SDDS_DefineParameter(SDDS_target, source->parameter_definition[i].name,
                               source->parameter_definition[i].symbol, source->parameter_definition[i].units, source->parameter_definition[i].description, source->parameter_definition[i].format_string, source->parameter_definition[i].type, source->parameter_definition[i].fixed_value) < 0) {
        SDDS_SetError("Unable to define parameter (SDDS_TransferAllParameterDefinitions)");
        SDDS_DeferSavingLayout(SDDS_target, 0);
        return 0;
      }
    }
  }
  SDDS_DeferSavingLayout(SDDS_target, 0);
  return 1;
}

/**
 * @brief Transfers all column definitions from a source dataset to a target dataset.
 *
 * This function defines all columns in the target SDDS dataset to match the column definitions in the source SDDS dataset.
 * It handles existing columns based on the specified mode.
 *
 * @param SDDS_target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param SDDS_source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param mode Flags that determine how to handle existing columns. Valid flags include:
 *             - `0`: Error if a column already exists in the target dataset.
 *             - `SDDS_TRANSFER_KEEPOLD`: Retain existing columns in the target dataset and skip transferring conflicting columns.
 *             - `SDDS_TRANSFER_OVERWRITE`: Overwrite existing columns in the target dataset with definitions from the source dataset.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 */
int32_t SDDS_TransferAllColumnDefinitions(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, uint32_t mode) {
  SDDS_LAYOUT *target, *source;
  int32_t i, index;
  char messBuffer[1024];

  if (!SDDS_CheckDataset(SDDS_target, "SDDS_TransferAllColumnDefinitions"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_TransferAllColumnDefinitions"))
    return (0);
  if (mode & SDDS_TRANSFER_KEEPOLD && mode & SDDS_TRANSFER_OVERWRITE) {
    SDDS_SetError("Inconsistent mode flags (SDDS_TransferAllColumnDefinitions)");
    return 0;
  }
  target = &SDDS_target->layout;
  source = &SDDS_source->layout;
  SDDS_DeferSavingLayout(SDDS_target, 1);
  for (i = 0; i < source->n_columns; i++)
    if ((index = SDDS_GetColumnIndex(SDDS_target, source->column_definition[i].name)) >= 0) {
      /* already exists */
      if (mode & SDDS_TRANSFER_KEEPOLD)
        continue;
      if (!(mode & SDDS_TRANSFER_OVERWRITE)) {
        sprintf(messBuffer, "Unable to define column %s---already exists (SDDS_TransferAllColumnDefinitions)", source->column_definition[i].name);
        SDDS_SetError(messBuffer);
        SDDS_DeferSavingLayout(SDDS_target, 0);
        return 0;
      }
      if (source->column_definition[i].type != target->column_definition[index].type && SDDS_target->n_rows_allocated) {
        sprintf(messBuffer, "Unable to define column %s---type mismatch and table already allocated (SDDS_TransferAllColumnDefinitions)", source->column_definition[i].name);
        SDDS_SetError(messBuffer);
        SDDS_DeferSavingLayout(SDDS_target, 0);
        return 0;
      }
      if (!SDDS_ChangeColumnInformation(SDDS_target, "symbol",
                                        &source->column_definition[i].symbol,
                                        SDDS_BY_INDEX, index) ||
          !SDDS_ChangeColumnInformation(SDDS_target, "units",
                                        &source->column_definition[i].units,
                                        SDDS_BY_INDEX, index) ||
          !SDDS_ChangeColumnInformation(SDDS_target, "description",
                                        &source->column_definition[i].description,
                                        SDDS_BY_INDEX, index) ||
          !SDDS_ChangeColumnInformation(SDDS_target, "format_string",
                                        &source->column_definition[i].format_string,
                                        SDDS_BY_INDEX, index) ||
          !SDDS_ChangeColumnInformation(SDDS_target, "type", &source->column_definition[i].type, SDDS_BY_INDEX, index) || !SDDS_ChangeColumnInformation(SDDS_target, "field_length", &source->column_definition[i].field_length, SDDS_BY_INDEX, index)) {
        SDDS_SetError("Unable to define column---problem with overwrite (SDDS_TransferAllColumnDefinitions)");
        SDDS_DeferSavingLayout(SDDS_target, 0);
        return 0;
      }
      target->column_definition[index].definition_mode = source->column_definition[index].definition_mode;
      if (target->column_definition[index].type == SDDS_STRING)
        target->column_definition[index].memory_number = SDDS_CreateRpnMemory(source->column_definition[i].name, 1);
      else
        target->column_definition[index].memory_number = SDDS_CreateRpnMemory(source->column_definition[i].name, 0);
    } else {
      if (SDDS_DefineColumn(SDDS_target, source->column_definition[i].name, source->column_definition[i].symbol,
                            source->column_definition[i].units, source->column_definition[i].description, source->column_definition[i].format_string, source->column_definition[i].type, source->column_definition[i].field_length) < 0) {
        SDDS_SetError("Unable to define column (SDDS_TransferAllColumnDefinitions)");
        SDDS_DeferSavingLayout(SDDS_target, 0);
        return 0;
      }
    }
  SDDS_DeferSavingLayout(SDDS_target, 0);
  return 1;
}

/**
 * @brief Transfers all array definitions from a source dataset to a target dataset.
 *
 * This function defines all arrays in the target SDDS dataset to match the array definitions in the source SDDS dataset.
 * Currently, only mode `0` is supported, which results in an error if any array already exists in the target dataset.
 *
 * @param SDDS_target Pointer to the `SDDS_DATASET` structure representing the target dataset.
 * @param SDDS_source Pointer to the `SDDS_DATASET` structure representing the source dataset.
 * @param mode Flags that determine how to handle existing arrays. Valid value:
 *             - `0`: Error if an array already exists in the target dataset.
 *
 * @return `1` on success; `0` on failure. On failure, an error message is recorded.
 *
 * @note Non-zero mode flags are not supported for array definitions.
 */
int32_t SDDS_TransferAllArrayDefinitions(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, uint32_t mode) {
  SDDS_LAYOUT *source;
  int32_t i;

  if (!SDDS_CheckDataset(SDDS_target, "SDDS_TransferAllArrayDefinitions"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_source, "SDDS_TransferAllArrayDefinitions"))
    return (0);
  if (mode) {
    /* haven't done this one yet */
    SDDS_SetError("Nonzero mode not supported for arrays (SDDS_TransferAllArrayDefinitions)");
    return 0;
  }
  source = &SDDS_source->layout;
  SDDS_DeferSavingLayout(SDDS_target, 1);
  for (i = 0; i < source->n_arrays; i++)
    if (SDDS_DefineArray(SDDS_target, source->array_definition[i].name, source->array_definition[i].symbol,
                         source->array_definition[i].units, source->array_definition[i].description,
                         source->array_definition[i].format_string, source->array_definition[i].type, source->array_definition[i].field_length, source->array_definition[i].dimensions, source->array_definition[i].group_name) < 0) {
      SDDS_SetError("Unable to define array (SDDS_TransferAllArrayDefinitions)");
      SDDS_DeferSavingLayout(SDDS_target, 0);
      return 0;
    }
  SDDS_DeferSavingLayout(SDDS_target, 0);
  return 1;
}
