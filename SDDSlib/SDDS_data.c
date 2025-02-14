/**
 * @file SDDS_data.c
 * @brief Defines global data arrays used by SDDS (Self Describing Data Sets) routines.
 *
 * This file declares and initializes global data arrays utilized by the SDDS library's
 * various routines. These arrays include data mode identifiers, type names and sizes,
 * command names, and field information structures for descriptions, data modes,
 * arrays, columns, parameters, associates, and includes.
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
 */

#include "SDDS.h"
#include "SDDS_internal.h"

/**
 * @brief Array of supported data modes.
 *
 * Contains the string representations of the different data modes supported by SDDS,
 * such as "binary" and "ascii".
 */
char *SDDS_data_mode[SDDS_NUM_DATA_MODES] = {
    "binary",
    "ascii"
};

/**
 * @brief Array of supported data type names.
 *
 * Lists the string names corresponding to each data type supported by SDDS.
 */
char *SDDS_type_name[SDDS_NUM_TYPES] = {
    "longdouble",
    "double",
    "float",
    "long64",
    "ulong64",
    "long",
    "ulong",
    "short",
    "ushort",
    "string",
    "character"
};

/**
 * @brief Array of sizes for each supported data type.
 *
 * Stores the size in bytes for each data type defined in SDDS_TYPE.
 */
int32_t SDDS_type_size[SDDS_NUM_TYPES] = {
    sizeof(long double),
    sizeof(double),
    sizeof(float),
    sizeof(int64_t),
    sizeof(uint64_t),
    sizeof(int32_t),
    sizeof(uint32_t),
    sizeof(short),
    sizeof(unsigned short),
    sizeof(char *),
    sizeof(char)
};

/**
 * @brief Array of supported SDDS command names.
 *
 * Lists the command strings that can be used within SDDS files to define structure and data.
 */
char *SDDS_command[SDDS_NUM_COMMANDS] = {
    "description",
    "column",
    "parameter",
    "associate",
    "data",
    "include",
    "array",
};

/* 
 * Field name and type information:
 * This section defines the mapping between field names and their corresponding 
 * offsets and types within various SDDS structures. This approach duplicates
 * some aspects of the namelist description structures but offers easier usage
 * and decouples the routines from the namelist implementation.
 * Eventually, this information may replace the namelist routines entirely.
 */
#include <stddef.h>

/**
 * @brief Field information for SDDS layout descriptions.
 *
 * Maps each description field name to its offset within the SDDS_LAYOUT structure,
 * along with the corresponding data type and any associated enumeration pairs.
 */
SDDS_FIELD_INFORMATION SDDS_DescriptionFieldInformation[SDDS_DESCRIPTION_FIELDS] = {
  {"text", offsetof(SDDS_LAYOUT, description), SDDS_STRING, NULL},
  {"contents", offsetof(SDDS_LAYOUT, contents), SDDS_STRING, NULL},
};

/**
 * @brief Enumeration pairs for data modes.
 *
 * Associates string representations of data modes with their corresponding SDDS enumeration values.
 */
SDDS_ENUM_PAIR dataModeEnumPair[3] = {
  {"binary", SDDS_BINARY},
  {"ascii", SDDS_ASCII},
  {NULL, 0},
};

/**
 * @brief Enumeration pairs for data endianness.
 *
 * Associates string representations of endianness with their corresponding SDDS enumeration values.
 */
SDDS_ENUM_PAIR dataEndianEnumPair[3] = {
  {"big", SDDS_BIGENDIAN},
  {"little", SDDS_LITTLEENDIAN},
  {NULL, 0},
};

/**
 * @brief Enumeration pairs for data types.
 *
 * Associates string representations of data types with their corresponding SDDS enumeration values.
 */
SDDS_ENUM_PAIR typeEnumPair[SDDS_NUM_TYPES + 1] = {
  {"longdouble", SDDS_LONGDOUBLE},
  {"double", SDDS_DOUBLE},
  {"float", SDDS_FLOAT},
  {"long64", SDDS_LONG64},
  {"ulong64", SDDS_ULONG64},
  {"long", SDDS_LONG},
  {"ulong", SDDS_ULONG},
  {"short", SDDS_SHORT},
  {"ushort", SDDS_USHORT},
  {"string", SDDS_STRING},
  {"character", SDDS_CHARACTER},
  {NULL, 0}};

/**
 * @brief Field information for data mode settings.
 *
 * Maps each data mode field name to its offset within the DATA_MODE structure,
 * along with the corresponding data type and any associated enumeration pairs.
 */
SDDS_FIELD_INFORMATION SDDS_DataFieldInformation[SDDS_DATA_FIELDS] = {
  {"mode", offsetof(DATA_MODE, mode), SDDS_LONG, dataModeEnumPair},
  {"lines_per_row", offsetof(DATA_MODE, lines_per_row), SDDS_LONG, NULL},
  {"no_row_counts", offsetof(DATA_MODE, no_row_counts), SDDS_LONG, NULL},
  {"fixed_row_count", offsetof(DATA_MODE, fixed_row_count), SDDS_LONG, NULL},
  {"additional_header_lines", offsetof(DATA_MODE, additional_header_lines), SDDS_LONG, NULL},
  {"column_major_order", offsetof(DATA_MODE, column_major), SDDS_SHORT, NULL},
  {"endian", offsetof(DATA_MODE, endian), SDDS_LONG, dataEndianEnumPair},
};

/**
 * @brief Field information for array definitions.
 *
 * Maps each array field name to its offset within the ARRAY_DEFINITION structure,
 * along with the corresponding data type and any associated enumeration pairs.
 */
SDDS_FIELD_INFORMATION SDDS_ArrayFieldInformation[SDDS_ARRAY_FIELDS] = {
  {"name", offsetof(ARRAY_DEFINITION, name), SDDS_STRING, NULL},
  {"symbol", offsetof(ARRAY_DEFINITION, symbol), SDDS_STRING, NULL},
  {"units", offsetof(ARRAY_DEFINITION, units), SDDS_STRING, NULL},
  {"description", offsetof(ARRAY_DEFINITION, description), SDDS_STRING, NULL},
  {"format_string", offsetof(ARRAY_DEFINITION, format_string), SDDS_STRING, NULL},
  {"group_name", offsetof(ARRAY_DEFINITION, group_name), SDDS_STRING, NULL},
  {"type", offsetof(ARRAY_DEFINITION, type), SDDS_LONG, typeEnumPair},
  {"field_length", offsetof(ARRAY_DEFINITION, field_length), SDDS_LONG, NULL},
  {"dimensions", offsetof(ARRAY_DEFINITION, dimensions), SDDS_LONG, NULL},
};

/**
 * @brief Field information for column definitions.
 *
 * Maps each column field name to its offset within the COLUMN_DEFINITION structure,
 * along with the corresponding data type and any associated enumeration pairs.
 */
SDDS_FIELD_INFORMATION SDDS_ColumnFieldInformation[SDDS_COLUMN_FIELDS] = {
  {"name", offsetof(COLUMN_DEFINITION, name), SDDS_STRING},
  {"symbol", offsetof(COLUMN_DEFINITION, symbol), SDDS_STRING},
  {"units", offsetof(COLUMN_DEFINITION, units), SDDS_STRING},
  {"description", offsetof(COLUMN_DEFINITION, description), SDDS_STRING},
  {"format_string", offsetof(COLUMN_DEFINITION, format_string), SDDS_STRING},
  {"type", offsetof(COLUMN_DEFINITION, type), SDDS_LONG, typeEnumPair},
  {"field_length", offsetof(COLUMN_DEFINITION, field_length), SDDS_LONG},
};

/**
 * @brief Field information for parameter definitions.
 *
 * Maps each parameter field name to its offset within the PARAMETER_DEFINITION structure,
 * along with the corresponding data type and any associated enumeration pairs.
 */
SDDS_FIELD_INFORMATION SDDS_ParameterFieldInformation[SDDS_PARAMETER_FIELDS] = {
  {"name", offsetof(PARAMETER_DEFINITION, name), SDDS_STRING},
  {"symbol", offsetof(PARAMETER_DEFINITION, symbol), SDDS_STRING},
  {"units", offsetof(PARAMETER_DEFINITION, units), SDDS_STRING},
  {"description", offsetof(PARAMETER_DEFINITION, description), SDDS_STRING},
  {"format_string", offsetof(PARAMETER_DEFINITION, format_string), SDDS_STRING},
  {"type", offsetof(PARAMETER_DEFINITION, type), SDDS_LONG, typeEnumPair},
  {"fixed_value", offsetof(PARAMETER_DEFINITION, fixed_value), SDDS_STRING},
};

/**
 * @brief Field information for associate definitions.
 *
 * Maps each associate field name to its offset within the ASSOCIATE_DEFINITION structure,
 * along with the corresponding data type and any associated enumeration pairs.
 */
SDDS_FIELD_INFORMATION SDDS_AssociateFieldInformation[SDDS_ASSOCIATE_FIELDS] = {
  {"name", offsetof(ASSOCIATE_DEFINITION, name), SDDS_STRING},
  {"filename", offsetof(ASSOCIATE_DEFINITION, filename), SDDS_STRING},
  {"path", offsetof(ASSOCIATE_DEFINITION, path), SDDS_STRING},
  {"description", offsetof(ASSOCIATE_DEFINITION, description), SDDS_STRING},
  {"contents", offsetof(ASSOCIATE_DEFINITION, contents), SDDS_STRING},
  {"sdds", offsetof(ASSOCIATE_DEFINITION, sdds), SDDS_LONG},
};

/**
 * @brief Field information for include directives.
 *
 * Maps each include field name to its offset within the INCLUDE_DEFINITION structure,
 * along with the corresponding data type.
 */
SDDS_FIELD_INFORMATION SDDS_IncludeFieldInformation[SDDS_INCLUDE_FIELDS] = {
  {"filename", 0, SDDS_STRING},
};
