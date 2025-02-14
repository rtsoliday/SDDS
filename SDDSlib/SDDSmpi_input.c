/**
 * @file SDDSmpi_input.c
 * @brief SDDS MPI Input Initialization and Data Broadcasting Functions
 *
 * This file implements the core functionalities for initializing and reading
 * Self Describing Data Sets (SDDS) in a parallel computing environment using MPI.
 * It includes definitions of essential data structures and functions that handle
 * various data formats, manage layout information, and facilitate the broadcasting
 * of data across multiple MPI processes.
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
 *  H. Shang
 *  R. Soliday
 */

#include "SDDS.h"
#include "SDDS_internal.h"
#include "scan.h"

#if defined(_WIN32)
#  include <fcntl.h>
#  include <io.h>
#  if defined(__BORLANDC__)
#    define _setmode(handle, amode) setmode(handle, amode)
#  endif
#endif

/**
 * @struct ELEMENT_DEF
 * @brief Structure defining an element with various attributes.
 *
 * This structure holds information about an element, including its type,
 * field length, dimensions, and associated strings such as name, symbol,
 * units, description, format string, fixed value, and group name.
 */
typedef struct
{
  int32_t type, field_length, dimensions, definition_mode, memory_number, pointer_number;
  int32_t name_len, symbol_len, units_len, description_len, format_string_len, fixed_value_len, group_name_len;
  char name[256], symbol[256], units[256], description[1024], format_string[256], fixed_value[1024], group_name[256];
} ELEMENT_DEF;

/**
 * @struct ASSOCIATE_DEF
 * @brief Structure defining an associate with various attributes.
 *
 * This structure holds information about an associate, including its SDDS flag,
 * lengths of name, filename, path, description, and contents strings, as well
 * as the actual strings themselves.
 */
typedef struct
{
  int32_t sdds, name_len, filename_len, path_len, description_len, contents_len;
  char name[256], filename[256], path[1024], description[1024], contents[1024];
} ASSOCIATE_DEF;

/**
 * @struct OTHER_DEF
 * @brief Structure defining additional layout information.
 *
 * This structure holds miscellaneous layout information, including the number
 * of columns, parameters, associates, arrays, description and contents lengths,
 * version, layout offset, filename length, mode, lines per row, row count
 * settings, synchronization flags, byte order, depth, command flags, and
 * associated strings.
 */
typedef struct
{
  int32_t n_columns, n_parameters, n_associates, n_arrays, description_len, contents_len, version, layout_offset, filename_len;
  int32_t mode, lines_per_row, no_row_counts, fixed_row_count, fsync_data, additional_header_lines; /*data_mode definition */
  short layout_written, disconnected, gzipFile, lzmaFile, popenUsed, swapByteOrder, column_memory_mode;
  uint32_t byteOrderDeclared;
  int32_t depth;
  int32_t data_command_seen;
  uint32_t commentFlags;

  char description[1024], contents[1024], filename[1024];
} OTHER_DEF;

/*the string length in array or columns should be less than 40 */
/**
 * @def STRING_COL_LENGTH
 * @brief Defines the maximum length of strings in arrays or columns.
 *
 * The string length in arrays or columns should be less than 40 characters.
 */
#define STRING_COL_LENGTH 40
/**
 * @struct STRING_DEF
 * @brief Structure defining a string with fixed maximum length.
 *
 * This structure holds a string value with a maximum length defined by
 * STRING_COL_LENGTH.
 */
typedef struct
{
  char str_value[STRING_COL_LENGTH];
} STRING_DEF;

/**
 * @brief Reads a page from an SDDS dataset using MPI.
 *
 * This function checks the validity of the provided SDDS dataset and ensures
 * that the dataset is connected and in binary mode before attempting to read
 * a page. If the dataset is in ASCII mode or disconnected, appropriate errors
 * are set and the function returns 0. On successful reading of a binary page,
 * the function returns 1.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 *
 * @return 
 *   - `1` on successful page read.
 *   - `0` if the dataset is invalid, disconnected, in ASCII mode, or an unrecognized data mode.
 *
 * @sa SDDS_CheckDataset, SDDS_SetError, SDDS_MPI_ReadBinaryPage
 */
int32_t SDDS_MPI_ReadPage(SDDS_DATASET *SDDS_dataset) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadPageSparse"))
    return (0);
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't read page--file is disconnected (SDDS_ReadPageSparse)");
    return 0;
  }
  if (SDDS_dataset->original_layout.data_mode.mode == SDDS_ASCII) {
    SDDS_SetError("Unable to read ascii file with SDDS_MPI.");
    return 0;
  } else if (SDDS_dataset->original_layout.data_mode.mode == SDDS_BINARY) {
    return SDDS_MPI_ReadBinaryPage(SDDS_dataset);
  } else {
    SDDS_SetError("Unable to read page--unrecognized data mode (SDDS_ReadPageSparse)");
    return (0);
  }
}

/**
 * @brief Broadcasts the layout of an SDDS dataset to all MPI processes.
 *
 * This function creates MPI data types for the layout structures, broadcasts the
 * layout information from the root process to all other processes, and defines
 * columns, parameters, arrays, and associates on non-root processes based on the
 * received layout information. It ensures that all MPI processes have a consistent
 * view of the dataset layout.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 *
 * @return 
 *   - `1` on successful broadcast and layout definition.
 *   - `0` if an error occurs during memory allocation, MPI operations, or layout definition.
 *
 * @details
 * The function performs the following steps:
 * - Checks and commits MPI data types for ELEMENT_DEF and OTHER_DEF structures.
 * - On the root process (MPI rank 0), fills the OTHER_DEF structure with layout information.
 * - Broadcasts the OTHER_DEF structure to all processes.
 * - Allocates memory for columns, parameters, arrays, and associates based on the received counts.
 * - On the root process, populates the ELEMENT_DEF and ASSOCIATE_DEF arrays with layout details.
 * - Broadcasts the ELEMENT_DEF and ASSOCIATE_DEF arrays to all processes.
 * - On non-root processes, defines columns, parameters, arrays, and associates using the received information.
 * - Saves the layout on non-root processes.
 *
 * @sa SDDS_DefineColumn, SDDS_DefineParameter, SDDS_DefineArray, SDDS_DefineAssociate, SDDS_SaveLayout
 */
int32_t SDDS_MPI_BroadcastLayout(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *layout = &(SDDS_dataset->layout);
  MPI_DATASET *MPI_dataset = SDDS_dataset->MPI_dataset;
  char *symbol, *units, *description, *format_string, *fixed_value, *filename, *path, *contents;
  int i;
  ELEMENT_DEF *column = NULL, *parameter = NULL, *array = NULL;
  ASSOCIATE_DEF *associate = NULL;
  OTHER_DEF other;
  MPI_Datatype elementType, otherType, oldtypes[4], associateType;
  int blockcounts[4];
  /* MPI_Aint type used to be consistent with syntax of */
  /* MPI_Type_extent routine */
  MPI_Aint offsets[4], int_ext, short_ext, uint_ext;
  MPI_Aint int_lb, short_lb, uint_lb;

  MPI_Type_get_extent(MPI_INT, &int_lb, &int_ext);
  MPI_Type_get_extent(MPI_UNSIGNED, &uint_lb, &uint_ext);
  MPI_Type_get_extent(MPI_SHORT, &short_lb, &short_ext);

  layout = &(SDDS_dataset->layout);
  /*commit element type */
  offsets[0] = 0;
  oldtypes[0] = MPI_INT;
  blockcounts[0] = 13;
  offsets[1] = 13 * int_ext;
  oldtypes[1] = MPI_CHAR;
  blockcounts[1] = 256 + 256 + 256 + 1024 + 256 + 1024 + 256;
  /* Now define structured type and commit it */
  MPI_Type_create_struct(2, blockcounts, offsets, oldtypes, &elementType);
  MPI_Type_commit(&elementType);

  /*commit other type */
  offsets[0] = 0;
  oldtypes[0] = MPI_INT;
  blockcounts[0] = 15;

  offsets[1] = 15 * int_ext;
  oldtypes[1] = MPI_SHORT;
  blockcounts[1] = 4;

  blockcounts[2] = 1;
  oldtypes[2] = MPI_UNSIGNED;
  offsets[2] = offsets[1] + 4 * short_ext;

  blockcounts[3] = 1024 + 1024 + 1024;
  oldtypes[3] = MPI_CHAR;
  offsets[3] = offsets[2] + 1 * uint_ext;

  /* Now define structured type and commit it */
  MPI_Type_create_struct(4, blockcounts, offsets, oldtypes, &otherType);
  MPI_Type_commit(&otherType);

  if (MPI_dataset->myid == 0) {
    /*fill other layout structure */
    other.n_columns = layout->n_columns;
    other.n_parameters = layout->n_parameters;
    other.n_associates = layout->n_associates;
    other.n_arrays = layout->n_arrays;
    other.version = layout->version;
    other.layout_offset = SDDS_dataset->pagecount_offset[0];
    other.layout_written = layout->layout_written;
    other.disconnected = layout->disconnected;
    other.gzipFile = layout->gzipFile;
    other.lzmaFile = layout->lzmaFile;
    other.popenUsed = layout->popenUsed;
    other.depth = layout->depth;
    other.data_command_seen = layout->data_command_seen;
    other.commentFlags = layout->commentFlags;
    other.byteOrderDeclared = layout->byteOrderDeclared;
    other.mode = layout->data_mode.mode;
    other.lines_per_row = layout->data_mode.lines_per_row;
    other.no_row_counts = layout->data_mode.no_row_counts;
    other.fixed_row_count = layout->data_mode.fixed_row_count;
    other.column_memory_mode = layout->data_mode.column_memory_mode;
    other.fsync_data = layout->data_mode.fsync_data;
    other.additional_header_lines = layout->data_mode.additional_header_lines;
    other.description_len = other.contents_len = other.filename_len = 0;
    other.description[0] = other.contents[0] = other.filename[0] = '\0';
    other.swapByteOrder = SDDS_dataset->swapByteOrder;

    if (layout->description) {
      other.description_len = strlen(layout->description);
      sprintf(other.description, "%s", layout->description);
    }
    if (layout->contents) {
      other.contents_len = strlen(layout->contents);
      sprintf(other.contents, "%s", layout->contents);
    }
    if (layout->filename) {
      other.filename_len = strlen(layout->filename);
      sprintf(other.filename, "%s", layout->filename);
    }
  } else {
    /* if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET))) {
         SDDS_SetError("Unable to zero memory for sdds dataset.");
         return 0;
         }
         SDDS_dataset->MPI_dataset = MPI_dataset; */
  }
  /* broadcaset the layout other to other processors */
  MPI_Bcast(&other, 1, otherType, 0, MPI_dataset->comm);
  MPI_Type_free(&otherType);
  if (other.n_columns)
    column = malloc(sizeof(*column) * other.n_columns);
  if (other.n_parameters)
    parameter = malloc(sizeof(*parameter) * other.n_parameters);
  if (other.n_arrays)
    array = malloc(sizeof(*array) * other.n_arrays);
  if (other.n_associates)
    associate = malloc(sizeof(*associate) * other.n_associates);
  if (MPI_dataset->myid == 0) {
    /*fill elements */
    for (i = 0; i < other.n_columns; i++) {
      if (!SDDS_ZeroMemory(&column[i], sizeof(ELEMENT_DEF))) {
        SDDS_SetError("Unable to zero memory for columns(SDDS_MPI_InitializeInput)");
        return 0;
      }
      column[i].type = layout->column_definition[i].type;
      column[i].field_length = layout->column_definition[i].field_length;
      column[i].definition_mode = layout->column_definition[i].definition_mode;
      column[i].memory_number = layout->column_definition[i].memory_number;
      column[i].pointer_number = layout->column_definition[i].pointer_number;
      if (layout->column_definition[i].name) {
        column[i].name_len = strlen(layout->column_definition[i].name);
        sprintf(column[i].name, "%s", layout->column_definition[i].name);
      }
      if (layout->column_definition[i].symbol) {
        column[i].symbol_len = strlen(layout->column_definition[i].symbol);
        sprintf(column[i].symbol, "%s", layout->column_definition[i].symbol);
      }
      if (layout->column_definition[i].units) {
        column[i].units_len = strlen(layout->column_definition[i].units);
        sprintf(column[i].units, "%s", layout->column_definition[i].units);
      }
      if (layout->column_definition[i].description) {
        column[i].description_len = strlen(layout->column_definition[i].description);
        sprintf(column[i].description, "%s", layout->column_definition[i].description);
      }
      if (layout->column_definition[i].format_string) {
        column[i].format_string_len = strlen(layout->column_definition[i].format_string);
        sprintf(column[i].format_string, "%s", layout->column_definition[i].format_string);
      }
    }
    for (i = 0; i < other.n_parameters; i++) {
      if (!SDDS_ZeroMemory(&parameter[i], sizeof(ELEMENT_DEF))) {
        SDDS_SetError("Unable to zero memory for parameters(SDDS_MPI_InitializeInput)");
        return 0;
      }
      parameter[i].type = layout->parameter_definition[i].type;
      parameter[i].definition_mode = layout->parameter_definition[i].definition_mode;
      parameter[i].memory_number = layout->parameter_definition[i].memory_number;
      if (layout->parameter_definition[i].name) {
        parameter[i].name_len = strlen(layout->parameter_definition[i].name);
        sprintf(parameter[i].name, "%s", layout->parameter_definition[i].name);
      }
      if (layout->parameter_definition[i].symbol) {
        parameter[i].symbol_len = strlen(layout->parameter_definition[i].symbol);
        sprintf(parameter[i].symbol, "%s", layout->parameter_definition[i].symbol);
      }
      if (layout->parameter_definition[i].units) {
        parameter[i].units_len = strlen(layout->parameter_definition[i].units);
        sprintf(parameter[i].units, "%s", layout->parameter_definition[i].units);
      }
      if (layout->parameter_definition[i].description) {
        parameter[i].description_len = strlen(layout->parameter_definition[i].description);
        sprintf(parameter[i].description, "%s", layout->parameter_definition[i].description);
      }
      if (layout->parameter_definition[i].format_string) {
        parameter[i].format_string_len = strlen(layout->parameter_definition[i].format_string);
        sprintf(parameter[i].format_string, "%s", layout->parameter_definition[i].format_string);
      }
      if (layout->parameter_definition[i].fixed_value) {
        parameter[i].fixed_value_len = strlen(layout->parameter_definition[i].fixed_value);
        sprintf(parameter[i].fixed_value, "%s", layout->parameter_definition[i].fixed_value);
      } else
        parameter[i].fixed_value_len = -1;
    }
    for (i = 0; i < other.n_arrays; i++) {
      if (!SDDS_ZeroMemory(&array[i], sizeof(ELEMENT_DEF))) {
        SDDS_SetError("Unable to zero memory for arrays(SDDS_MPI_InitializeInput)");
        return 0;
      }
      array[i].type = layout->array_definition[i].type;
      array[i].field_length = layout->array_definition[i].field_length;
      array[i].dimensions = layout->array_definition[i].dimensions;
      if (layout->array_definition[i].name) {
        array[i].name_len = strlen(layout->array_definition[i].name);
        sprintf(array[i].name, "%s", layout->array_definition[i].name);
      }
      if (layout->array_definition[i].symbol) {
        array[i].symbol_len = strlen(layout->array_definition[i].symbol);
        sprintf(array[i].symbol, "%s", layout->array_definition[i].symbol);
      }
      if (layout->array_definition[i].units) {
        array[i].units_len = strlen(layout->array_definition[i].units);
        sprintf(array[i].units, "%s", layout->array_definition[i].units);
      }
      if (layout->array_definition[i].description) {
        array[i].description_len = strlen(layout->array_definition[i].description);
        sprintf(array[i].description, "%s", layout->array_definition[i].description);
      }
      if (layout->array_definition[i].format_string) {
        array[i].format_string_len = strlen(layout->array_definition[i].format_string);
        sprintf(array[i].format_string, "%s", layout->array_definition[i].format_string);
      }
      if (layout->array_definition[i].group_name) {
        array[i].group_name_len = strlen(layout->array_definition[i].group_name);
        sprintf(array[i].group_name, "%s", layout->array_definition[i].group_name);
      }
    }
    for (i = 0; i < other.n_associates; i++) {
      if (!SDDS_ZeroMemory(&associate[i], sizeof(ASSOCIATE_DEF))) {
        SDDS_SetError("Unable to zero memory for associates(SDDS_MPI_InitializeInput)");
        return 0;
      }
      associate[i].sdds = layout->associate_definition[i].sdds;

      if (layout->associate_definition[i].name) {
        associate[i].name_len = strlen(layout->associate_definition[i].name);
        sprintf(associate[i].name, "%s", layout->associate_definition[i].name);
      }
      if (layout->associate_definition[i].filename) {
        associate[i].filename_len = strlen(layout->associate_definition[i].filename);
        sprintf(associate[i].filename, "%s", layout->associate_definition[i].filename);
      }
      if (layout->associate_definition[i].path) {
        associate[i].path_len = strlen(layout->associate_definition[i].path);
        sprintf(associate[i].path, "%s", layout->associate_definition[i].path);
      }
      if (layout->associate_definition[i].description) {
        associate[i].description_len = strlen(layout->associate_definition[i].description);
        sprintf(associate[i].description, "%s", layout->associate_definition[i].description);
      }
      if (layout->associate_definition[i].contents) {
        associate[i].contents_len = strlen(layout->associate_definition[i].contents);
        sprintf(associate[i].contents, "%s", layout->associate_definition[i].contents);
      }
    }
  } else {
    SDDS_dataset->page_number = SDDS_dataset->page_started = 0;
    layout->popenUsed = other.popenUsed;
    layout->gzipFile = other.gzipFile;
    layout->lzmaFile = other.lzmaFile;
    layout->depth = other.depth;
    layout->data_command_seen = other.data_command_seen;
    layout->commentFlags = other.commentFlags;
    layout->disconnected = other.disconnected;
    layout->layout_written = other.layout_written;
    if (other.filename_len)
      SDDS_CopyString(&layout->filename, other.filename);
    layout->version = other.version;
    layout->byteOrderDeclared = other.byteOrderDeclared;
    layout->data_mode.mode = other.mode;
    layout->data_mode.lines_per_row = other.lines_per_row;
    layout->data_mode.no_row_counts = other.no_row_counts;
    layout->data_mode.fixed_row_count = other.fixed_row_count;
    layout->data_mode.fsync_data = other.fsync_data;
    layout->data_mode.column_memory_mode = other.column_memory_mode;
    layout->data_mode.additional_header_lines = other.additional_header_lines;
    if (other.description_len)
      SDDS_CopyString(&layout->description, other.description);
    if (other.contents_len)
      SDDS_CopyString(&layout->contents, other.contents);
    SDDS_dataset->swapByteOrder = other.swapByteOrder;
  }
  if (other.n_columns)
    MPI_Bcast(column, other.n_columns, elementType, 0, MPI_dataset->comm);
  if (other.n_parameters)
    MPI_Bcast(parameter, other.n_parameters, elementType, 0, MPI_dataset->comm);
  if (other.n_arrays)
    MPI_Bcast(array, other.n_arrays, elementType, 0, MPI_dataset->comm);
  MPI_Type_free(&elementType);
  if (other.n_associates) {
    /* create and commit associate type */
    offsets[0] = 0;
    oldtypes[0] = MPI_INT;
    blockcounts[0] = 6;

    offsets[1] = 6 * int_ext;
    oldtypes[1] = MPI_CHAR;
    blockcounts[1] = 256 + 256 + 1024 + 1024 + 1024;

    MPI_Type_create_struct(2, blockcounts, offsets, oldtypes, &associateType);
    MPI_Type_commit(&associateType);
    MPI_Bcast(associate, other.n_associates, associateType, 0, MPI_dataset->comm);
    MPI_Type_free(&associateType);
  }
  if (MPI_dataset->myid) {
    for (i = 0; i < other.n_columns; i++) {
      symbol = units = description = format_string = NULL;
      if (column[i].units_len)
        SDDS_CopyString(&units, column[i].units);
      if (column[i].description_len)
        SDDS_CopyString(&description, column[i].description);
      if (column[i].format_string_len)
        SDDS_CopyString(&format_string, column[i].format_string);
      if (column[i].symbol_len)
        SDDS_CopyString(&symbol, column[i].symbol);
      if (SDDS_DefineColumn(SDDS_dataset, column[i].name, symbol, units, description, format_string, column[i].type, column[i].field_length) < 0) {
        SDDS_SetError("Unable to define column (SDDS_MPI_BroadcastLayout)");
        return (0);
      }
      if (units)
        free(units);
      if (description)
        free(description);
      if (symbol)
        free(symbol);
      if (format_string)
        free(format_string);
    }
    for (i = 0; i < other.n_parameters; i++) {
      symbol = units = description = format_string = fixed_value = NULL;
      if (parameter[i].units_len)
        SDDS_CopyString(&units, parameter[i].units);
      if (parameter[i].description_len)
        SDDS_CopyString(&description, parameter[i].description);
      if (parameter[i].format_string_len)
        SDDS_CopyString(&format_string, parameter[i].format_string);
      if (parameter[i].symbol_len)
        SDDS_CopyString(&symbol, parameter[i].symbol);
      if (parameter[i].fixed_value_len >= 0)
        SDDS_CopyString(&fixed_value, parameter[i].fixed_value);
      if (SDDS_DefineParameter(SDDS_dataset, parameter[i].name, symbol, units, description, format_string, parameter[i].type, fixed_value) < 0) {
        SDDS_SetError("Unable to define parameter (SDDS_MPI_BroadcastLayout)");
        return (0);
      }
      if (units)
        free(units);
      if (description)
        free(description);
      if (symbol)
        free(symbol);
      if (format_string)
        free(format_string);
      if (fixed_value)
        free(fixed_value);
    }
    for (i = 0; i < other.n_arrays; i++) {
      if (SDDS_DefineArray(SDDS_dataset, array[i].name, array[i].symbol, array[i].units, array[i].description, array[i].format_string, array[i].type, array[i].field_length, array[i].dimensions, array[i].group_name) < 0) {
        SDDS_SetError("Unable to define array (SDDS_BroadcastLayout)");
        return (0);
      }
    }
    for (i = 0; i < other.n_associates; i++) {
      filename = path = description = contents = NULL;
      if (associate[i].filename_len)
        SDDS_CopyString(&filename, associate[i].filename);
      if (associate[i].path_len)
        SDDS_CopyString(&path, associate[i].path);
      if (associate[i].description_len)
        SDDS_CopyString(&description, associate[i].description);
      if (associate[i].contents_len)
        SDDS_CopyString(&contents, associate[i].contents);
      if (SDDS_DefineAssociate(SDDS_dataset, associate[i].name, filename, path, description, contents, associate[i].sdds) < 0) {
        SDDS_SetError("Unable to define associate (SDDS_MPI_BroadcastLayout)");
        return (0);
      }
      if (description)
        free(description);
      if (contents)
        free(contents);
      if (path)
        free(path);
      if (filename)
        free(filename);
    }
    if (!SDDS_SaveLayout(SDDS_dataset)) {
      SDDS_SetError("Unable to save layout (SDDS_BroadcastLayout)");
      return (0);
    }
  }
  if (column)
    free(column);
  if (array)
    free(array);
  if (parameter)
    free(parameter);
  if (associate)
    free(associate);
  column = array = parameter = NULL;
  associate = NULL;
  MPI_dataset->file_offset = other.layout_offset;
  return 1;
}

/**
 * @brief Initializes an SDDS dataset for input using MPI.
 *
 * This function initializes the provided `SDDS_DATASET` structure for reading data
 * from a specified file. It handles various file formats, including plain, gzip,
 * and LZMA-compressed files. The function sets up necessary layout information,
 * allocates memory for columns and related structures, and prepares the dataset
 * for parallel I/O operations using MPI.
 *
 * @param[in,out] SDDS_dataset
 *        Pointer to the `SDDS_DATASET` structure to be initialized.
 *
 * @param[in] filename
 *        The name of the file to be opened for reading. If `filename` is `NULL`,
 *        the function will attempt to read from the standard input (`stdin`).
 *
 * @return 
 *        - `1` on successful initialization.
 *        - `0` if an error occurs during initialization, such as memory
 *          allocation failures, file opening issues, or layout reading errors.
 *
 * @details
 * The function performs the following steps:
 * - Validates the `SDDS_DATASET` structure.
 * - Initializes layout-related flags and variables.
 * - Copies the filename into the dataset's layout if provided.
 * - Opens the specified file, handling different compression formats based on the
 *   file extension (`.gz`, `.lzma`, `.xz`).
 * - Reads the layout information from the file, supporting gzip and LZMA compression.
 * - Allocates memory for column flags and orders if columns are defined.
 * - Sets the dataset mode to read-only and initializes file offsets for sequential reading.
 * - Closes the file pointer after reading the layout.
 * - Broadcasts the layout information to all MPI processes if `MASTER_READTITLE_ONLY`
 *   is defined.
 * - Opens the MPI file for parallel reading and retrieves the file size and column offsets.
 *
 * @note
 * - The function assumes that only one page of data is present in the input file.
 * - If `MASTER_READTITLE_ONLY` is defined, only the root process reads the title.
 *
 * @sa SDDS_CheckDataset, SDDS_SetError, SDDS_CopyString, SDDS_ReadLayout,
 *     SDDS_GZipReadLayout, SDDS_LZMAReadLayout, SDDS_SaveLayout,
 *     SDDS_MPI_BroadcastLayout, SDDS_MPI_File_Open, SDDS_MPI_Get_Column_Size
 */
int32_t SDDS_MPI_InitializeInput(SDDS_DATASET *SDDS_dataset, char *filename) {
  /*  startTime = MPI_Wtime(); */
  MPI_DATASET *MPI_dataset = SDDS_dataset->MPI_dataset;

#if defined(MASTER_READTITLE_ONLY)
  if (MPI_dataset->myid == 0)
#endif
  {
    /*  char *ptr, *datafile, *headerfile; */
    static char s[SDDS_MAXLINE];
#if defined(zLib)
    char *extension;
#endif
    if (sizeof(gzFile) != sizeof(void *)) {
      SDDS_SetError("gzFile is not the same size as void *, possible corruption of the SDDS_LAYOUT structure");
      return (0);
    }
    if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_InitializeInput"))
      return (0);
    SDDS_dataset->layout.gzipFile = SDDS_dataset->layout.lzmaFile = SDDS_dataset->layout.disconnected = SDDS_dataset->layout.popenUsed = 0;
    SDDS_dataset->layout.depth = SDDS_dataset->layout.data_command_seen = SDDS_dataset->layout.commentFlags = 0;
    if (!filename)
      SDDS_dataset->layout.filename = NULL;
    else if (!SDDS_CopyString(&SDDS_dataset->layout.filename, filename)) {
      sprintf(s, "Memory allocation failure initializing file \"%s\" (SDDS_InitializeInput)", filename);
      SDDS_SetError(s);
      return (0);
    }
    if (!filename) {
#if defined(_WIN32)
      if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        sprintf(s, "unable to set stdin to binary mode");
        SDDS_SetError(s);
        return 0;
      }
#endif
      SDDS_dataset->layout.fp = stdin;
    } else {
#if defined(zLib)
      if (!(extension = strrchr(filename, '.')) || strcmp(extension, ".gz") != 0) {
#endif
        if ((extension = strrchr(filename, '.')) && ((strcmp(extension, ".lzma") == 0) || (strcmp(extension, ".xz") == 0))) {
          SDDS_dataset->layout.lzmaFile = 1;
          if (!(SDDS_dataset->layout.lzmafp = UnpackLZMAOpen(filename))) {
            sprintf(s, "Unable to open file \"%s\" for reading (SDDS_InitializeInput)", filename);
            SDDS_SetError(s);
            return (0);
          }
          SDDS_dataset->layout.fp = SDDS_dataset->layout.lzmafp->fp;
        } else {
          if (!(SDDS_dataset->layout.fp = UnpackFopen(filename, UNPACK_REQUIRE_SDDS | UNPACK_USE_PIPE, &SDDS_dataset->layout.popenUsed, NULL))) {
            sprintf(s, "Unable to open file \"%s\" for reading (SDDS_InitializeInput)", filename);
            SDDS_SetError(s);
            return (0);
          }
        }
#if defined(zLib)
      } else {
        SDDS_dataset->layout.gzipFile = 1;
        if (!(SDDS_dataset->layout.gzfp = gzopen(filename, "rb"))) {
          sprintf(s, "Unable to open file \"%s\" for reading (SDDS_InitializeInput)", filename);
          SDDS_SetError(s);
          return (0);
        }
      }
#endif
    }
    SDDS_dataset->page_number = SDDS_dataset->page_started = 0;
    SDDS_dataset->file_had_data = 0;
    SDDS_DeferSavingLayout(SDDS_dataset, 1);
#if defined(zLib)
    if (SDDS_dataset->layout.gzipFile) {
      if (!SDDS_GZipReadLayout(SDDS_dataset, SDDS_dataset->layout.gzfp))
        return (0);
    } else {
#endif
      if (SDDS_dataset->layout.lzmaFile) {
        if (!SDDS_LZMAReadLayout(SDDS_dataset, SDDS_dataset->layout.lzmafp))
          return (0);
      } else {
        if (!SDDS_ReadLayout(SDDS_dataset, SDDS_dataset->layout.fp))
          return (0);
      }
#if defined(zLib)
    }
#endif
    SDDS_dataset->layout.layout_written = 0;
    SDDS_DeferSavingLayout(SDDS_dataset, 0);
    if (!SDDS_SaveLayout(SDDS_dataset))
      return 0;
    if (SDDS_dataset->layout.n_columns &&
        ((!(SDDS_dataset->column_flag = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_dataset->layout.n_columns)) ||
          !(SDDS_dataset->column_order = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_dataset->layout.n_columns))) ||
         (!SDDS_SetMemory(SDDS_dataset->column_flag, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)1, (int32_t)0) || !SDDS_SetMemory(SDDS_dataset->column_order, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)0, (int32_t)1)))) {
      SDDS_SetError("Unable to initialize input--memory allocation failure (SDDS_InitializeInput)");
      return (0);
    }
    SDDS_dataset->mode = SDDS_READMODE; /*reading */
    SDDS_dataset->pagecount_offset = NULL;
    if (!SDDS_dataset->layout.gzipFile && !SDDS_dataset->layout.lzmaFile && !SDDS_dataset->layout.popenUsed && SDDS_dataset->layout.filename) {
      /* Data is not:
	     1. from a gzip file 
	     2. from a file that is being internally decompressed by a command executed with popen()
	     3. from a pipe set up externally (e.g., -pipe=in on commandline)
          */
      SDDS_dataset->pages_read = 0;
      SDDS_dataset->pagecount_offset = malloc(sizeof(*SDDS_dataset->pagecount_offset));
      SDDS_dataset->pagecount_offset[0] = ftell(SDDS_dataset->layout.fp);
      fseek(SDDS_dataset->layout.fp, 0, 2); /*point to the end of the file */
      SDDS_dataset->endOfFile_offset = ftell(SDDS_dataset->layout.fp);
      fseek(SDDS_dataset->layout.fp, SDDS_dataset->pagecount_offset[0], 0);
      /*point to the beginning of the first page */
    }
    if (SDDS_dataset->layout.fp)
      fclose(SDDS_dataset->layout.fp);
    /*    SDDS_dataset->MPI_dataset = MPI_dataset; */
  }

#if defined(MASTER_READTITLE_ONLY)
  if (!SDDS_MPI_BroadcastLayout(SDDS_dataset))
    return 0;
#else
  MPI_dataset->file_offset = SDDS_dataset->pagecount_offset[0];
#endif

  if (!SDDS_MPI_File_Open(MPI_dataset, filename, SDDS_MPI_READ_ONLY)) {
    SDDS_SetError("(SDDS_MPI_File_Open)Unablle to open file for reading.");
    return 0;
  }

  MPI_File_get_size(MPI_dataset->MPI_file, &(MPI_dataset->file_size));
  if (MPI_dataset->file_offset >= MPI_dataset->file_size) {
    SDDS_SetError("No data contained in the input file.");
    return 0;
  }
  MPI_dataset->column_offset = SDDS_MPI_Get_Column_Size(SDDS_dataset);
  SDDS_dataset->parallel_io = 1;
  return 1;
}

/**
 * @brief Initializes an SDDS dataset for input by searching the provided search path using MPI.
 *
 * This function searches for the specified file within predefined search paths and
 * initializes the `SDDS_DATASET` structure for reading using MPI. It leverages
 * `SDDS_MPI_InitializeInput` to perform the actual initialization once the file is found.
 *
 * @param[in,out] SDDS_dataset
 *        Pointer to the `SDDS_DATASET` structure to be initialized.
 *
 * @param[in] file
 *        The name of the file to search for and initialize. The function searches for
 *        this file in the configured search paths.
 *
 * @return 
 *        - `1` on successful initialization.
 *        - `0` if the file is not found in the search paths or if initialization fails.
 *
 * @details
 * The function performs the following steps:
 * - Searches for the specified file in the predefined search paths using `findFileInSearchPath`.
 * - If the file is found, it calls `SDDS_MPI_InitializeInput` to initialize the dataset.
 * - Frees the allocated memory for the filename after initialization.
 *
 * @sa SDDS_MPI_InitializeInput, findFileInSearchPath, SDDS_SetError
 */
int32_t SDDS_MPI_InitializeInputFromSearchPath(SDDS_DATASET *SDDS_dataset, char *file) {
  char *filename;
  int32_t value;
  if (!(filename = findFileInSearchPath(file))) {
    SDDS_SetError("file does not exist in searchpath (InitializeInputFromSearchPath)");
    return 0;
  }
  value = SDDS_MPI_InitializeInput(SDDS_dataset, filename);
  free(filename);
  return value;
}

/**
 * @brief Initializes an SDDS dataset for input from a search path on the master MPI process.
 *
 * This function is designed to be called by the master (root) process in an MPI environment.
 * It searches for the specified file within the search paths, initializes the `SDDS_DATASET`
 * structure, and broadcasts the layout information to all other MPI processes. Non-master
 * processes receive the broadcasted layout and prepare their datasets accordingly.
 *
 * @param[in,out] SDDS_dataset
 *        Pointer to the `SDDS_DATASET` structure to be initialized.
 *
 * @param[in] MPI_dataset
 *        Pointer to the `MPI_DATASET` structure containing MPI-related information
 *        such as the communicator and process ID.
 *
 * @param[in] file
 *        The name of the file to search for and initialize. The function searches for
 *        this file in the configured search paths.
 *
 * @return 
 *        - `1` on successful initialization and layout broadcast.
 *        - `0` if the file is not found, initialization fails, or layout broadcasting fails.
 *
 * @details
 * The function performs the following steps:
 * - On the master process (MPI rank 0), it searches for the specified file in the search paths.
 * - If the file is found, it initializes the dataset using `SDDS_InitializeInput`.
 * - Non-master processes zero out their `SDDS_DATASET` structures to prepare for receiving the layout.
 * - Sets the `parallel_io` flag to `0` and assigns the `MPI_dataset`.
 * - Broadcasts the layout information to all MPI processes using `SDDS_MPI_BroadcastLayout`.
 *
 * @note
 * - Only the master process performs file searching and dataset initialization.
 * - Non-master processes rely entirely on the broadcasted layout information.
 *
 * @sa SDDS_MPI_InitializeInputFromSearchPath, SDDS_MPI_BroadcastLayout, SDDS_InitializeInput, SDDS_ZeroMemory
 */
int32_t SDDS_Master_InitializeInputFromSearchPath(SDDS_DATASET *SDDS_dataset, MPI_DATASET *MPI_dataset, char *file) {
  char *filename;

  if (MPI_dataset->myid == 0) {
    if (!(filename = findFileInSearchPath(file))) {
      SDDS_SetError("file does not exist in searchpath (InitializeInputFromSearchPath)");
      return 0;
    }
    if (!SDDS_InitializeInput(SDDS_dataset, filename)) {
      free(filename);
      return 0;
    }
    free(filename);
  } else {
    if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET)))
      SDDS_Bomb("Unable to zero memory for SDDS dataset(SDDS_MPI_Setup)");
  }
  SDDS_dataset->parallel_io = 0;
  SDDS_dataset->MPI_dataset = MPI_dataset;
  if (!SDDS_MPI_BroadcastLayout(SDDS_dataset))
    return 0;

  return 1;
}

/**
 * @brief Initializes an SDDS dataset for input on the master MPI process.
 *
 * This function initializes the `SDDS_DATASET` structure for reading from a specified
 * file using MPI. It is intended to be called by the master (root) process, which
 * performs the dataset initialization and then broadcasts the layout to all other
 * MPI processes. Non-master processes prepare their datasets based on the received
 * layout information.
 *
 * @param[in,out] SDDS_dataset
 *        Pointer to the `SDDS_DATASET` structure to be initialized.
 *
 * @param[in] MPI_dataset
 *        Pointer to the `MPI_DATASET` structure containing MPI-related information
 *        such as the communicator and process ID.
 *
 * @param[in] file
 *        The name of the file to be opened and initialized for reading.
 *
 * @return 
 *        - `1` on successful initialization and layout broadcast.
 *        - `0` if initialization fails or layout broadcasting fails.
 *
 * @details
 * The function performs the following steps:
 * - On the master process (MPI rank 0), it initializes the dataset using `SDDS_InitializeInput`.
 * - Non-master processes zero out their `SDDS_DATASET` structures to prepare for receiving the layout.
 * - Sets the `parallel_io` flag to `0` and assigns the `MPI_dataset`.
 * - Broadcasts the layout information to all MPI processes using `SDDS_MPI_BroadcastLayout`.
 *
 * @note
 * - Only the master process performs dataset initialization.
 * - Non-master processes rely entirely on the broadcasted layout information.
 *
 * @sa SDDS_MPI_BroadcastLayout, SDDS_InitializeInput, SDDS_ZeroMemory
 */
int32_t SDDS_Master_InitializeInput(SDDS_DATASET *SDDS_dataset, MPI_DATASET *MPI_dataset, char *file) {
  if (MPI_dataset->myid == 0) {
    if (!SDDS_InitializeInput(SDDS_dataset, file))
      return 0;
  } else {
    if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET)))
      SDDS_Bomb("Unable to zero memory for SDDS dataset(SDDS_MPI_Setup)");
  }
  SDDS_dataset->parallel_io = 0;
  SDDS_dataset->MPI_dataset = MPI_dataset;
  if (!SDDS_MPI_BroadcastLayout(SDDS_dataset))
    return 0;

  return 1;
}

/*master read the file, and broadcast the contents to other processors, assuming only one page data */
/**
 * @brief Reads a single page of data from an SDDS dataset on the master MPI process and broadcasts it.
 *
 * This function is intended to be called by the master (root) MPI process. It reads a single
 * page of data from the SDDS input file, counts the number of rows of interest, and then
 * broadcasts the data to all other MPI processes. Non-master processes allocate memory
 * for the received data and populate their datasets accordingly.
 *
 * @param[in,out] SDDS_dataset
 *        Pointer to the `SDDS_DATASET` structure from which the page will be read and
 *        into which the data will be stored.
 *
 * @return 
 *        - The number of rows read on success.
 *        - A negative value if an error occurs during reading or broadcasting.
 *
 * @details
 * The function performs the following steps:
 * - On the master process (MPI rank 0), it reads a page of data using `SDDS_ReadPage`.
 * - Counts the number of rows of interest using `SDDS_CountRowsOfInterest`.
 * - Broadcasts the number of rows and retrieval status to all MPI processes.
 * - Non-master processes allocate memory for the received data by starting a new page.
 * - Broadcasts each parameter's value to all processes based on its type.
 * - Broadcasts each array's data to all processes based on its type.
 * - Broadcasts the data for each column to all processes based on its type.
 * - Allocates and populates string data where necessary.
 *
 * @note
 * - The function assumes that only one page of data is present in the input file.
 * - String data requires special handling to allocate appropriate memory on non-master processes.
 *
 * @sa SDDS_ReadPage, SDDS_CountRowsOfInterest, SDDS_StartPage, SDDS_DefineColumn,
 *     SDDS_DefineParameter, SDDS_DefineArray, SDDS_DefineAssociate
 */
int32_t SDDS_Master_ReadPage(SDDS_DATASET *SDDS_dataset) {
  MPI_DATASET *MPI_dataset = SDDS_dataset->MPI_dataset;
  int32_t rows = 0, i, j, len, retrival = 0;
  STRING_DEF *str_val = NULL;

  /*master read file */
  if (MPI_dataset->myid == 0) {
    if ((retrival = SDDS_ReadPage(SDDS_dataset)) <= 0) {
      SDDS_SetError("SDDS_MPI_ReadParameterFile2: Error in reading input file");
      return (retrival);
    }
    rows = SDDS_CountRowsOfInterest(SDDS_dataset);
  }
  MPI_Bcast(&rows, 1, MPI_INT, 0, MPI_dataset->comm);
  MPI_Bcast(&retrival, 1, MPI_INT, 0, MPI_dataset->comm);
  if (MPI_dataset->myid != 0) {
    /*allocate memory for other processors */
    if (!SDDS_StartPage(SDDS_dataset, rows))
      return 0;
  }
  /*broadcast the parameters to other processors */
  for (i = 0; i < SDDS_dataset->layout.n_parameters; i++) {
    switch (SDDS_dataset->layout.parameter_definition[i].type) {
    case SDDS_LONGDOUBLE:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_LONG_DOUBLE, 0, MPI_dataset->comm);
      break;
    case SDDS_DOUBLE:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_DOUBLE, 0, MPI_dataset->comm);
      break;
    case SDDS_FLOAT:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_FLOAT, 0, MPI_dataset->comm);
      break;
    case SDDS_LONG64:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_INT64_T, 0, MPI_dataset->comm);
      break;
    case SDDS_ULONG64:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_UINT64_T, 0, MPI_dataset->comm);
      break;
    case SDDS_LONG:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_INT, 0, MPI_dataset->comm);
      break;
    case SDDS_ULONG:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_UNSIGNED, 0, MPI_dataset->comm);
      break;
    case SDDS_SHORT:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_SHORT, 0, MPI_dataset->comm);
      break;
    case SDDS_USHORT:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_UNSIGNED_SHORT, 0, MPI_dataset->comm);
      break;
    case SDDS_STRING:
      if (MPI_dataset->myid == 0)
        len = strlen(*(char **)SDDS_dataset->parameter[i]);
      MPI_Bcast(&len, 1, MPI_INT, 0, MPI_dataset->comm);
      if (MPI_dataset->myid != 0)
        SDDS_dataset->parameter[i] = (char *)malloc(sizeof(char) * len);
      MPI_Bcast(SDDS_dataset->parameter[i], len, MPI_BYTE, 0, MPI_dataset->comm);
      break;
    case SDDS_CHARACTER:
      MPI_Bcast(SDDS_dataset->parameter[i], 1, MPI_CHAR, 0, MPI_dataset->comm);
      break;
    }
  }
  /* broadcast arrays to other processros */
  for (i = 0; i < SDDS_dataset->layout.n_arrays; i++) {
    MPI_Bcast(&(SDDS_dataset->layout.array_definition[i].dimensions), 1, MPI_INT, 0, MPI_dataset->comm);
    switch (SDDS_dataset->layout.array_definition[i].type) {
    case SDDS_LONGDOUBLE:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_LONG_DOUBLE, 0, MPI_dataset->comm);
      break;
    case SDDS_DOUBLE:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_DOUBLE, 0, MPI_dataset->comm);
      break;
    case SDDS_FLOAT:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_FLOAT, 0, MPI_dataset->comm);
      break;
    case SDDS_LONG64:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_INT64_T, 0, MPI_dataset->comm);
      break;
    case SDDS_ULONG64:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_UINT64_T, 0, MPI_dataset->comm);
      break;
    case SDDS_LONG:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_INT, 0, MPI_dataset->comm);
      break;
    case SDDS_ULONG:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_UNSIGNED, 0, MPI_dataset->comm);
      break;
    case SDDS_SHORT:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_SHORT, 0, MPI_dataset->comm);
      break;
    case SDDS_USHORT:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_UNSIGNED_SHORT, 0, MPI_dataset->comm);
      break;
    case SDDS_STRING:
      str_val = malloc(sizeof(*str_val) * SDDS_dataset->layout.array_definition[i].dimensions);
      if (MPI_dataset->myid == 0) {
        for (j = 0; j < SDDS_dataset->layout.array_definition[i].dimensions; j++)
          sprintf(str_val[j].str_value, "%s", ((char **)(SDDS_dataset->array[i].data))[j]);
      }
      MPI_Bcast(str_val, SDDS_dataset->layout.array_definition[i].dimensions * 256, MPI_BYTE, 0, MPI_dataset->comm);
      if (MPI_dataset->myid) {
        for (j = 0; j < SDDS_dataset->layout.array_definition[i].dimensions; j++)
          SDDS_CopyString(&(((char **)(SDDS_dataset->array[i].data))[j]), str_val[j].str_value);
      }
      free(str_val);
      str_val = NULL;
      break;
    case SDDS_CHARACTER:
      MPI_Bcast(SDDS_dataset->array[i].data, SDDS_dataset->layout.array_definition[i].dimensions, MPI_CHAR, 0, MPI_dataset->comm);
      break;
    }
  }

  /* broadcast columns to other processors */
  SDDS_dataset->n_rows = SDDS_dataset->n_rows_allocated = rows;
  MPI_Bcast(SDDS_dataset->row_flag, rows, MPI_INT, 0, MPI_dataset->comm);
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
    switch (SDDS_dataset->layout.column_definition[i].type) {
    case SDDS_LONGDOUBLE:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_LONG_DOUBLE, 0, MPI_dataset->comm);
      break;
    case SDDS_DOUBLE:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_DOUBLE, 0, MPI_dataset->comm);
      break;
    case SDDS_FLOAT:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_FLOAT, 0, MPI_dataset->comm);
      break;
    case SDDS_LONG64:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_INT64_T, 0, MPI_dataset->comm);
      break;
    case SDDS_ULONG64:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_UINT64_T, 0, MPI_dataset->comm);
      break;
    case SDDS_LONG:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_INT, 0, MPI_dataset->comm);
      break;
    case SDDS_ULONG:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_UNSIGNED, 0, MPI_dataset->comm);
      break;
    case SDDS_SHORT:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_SHORT, 0, MPI_dataset->comm);
      break;
    case SDDS_USHORT:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_UNSIGNED_SHORT, 0, MPI_dataset->comm);
      break;
    case SDDS_STRING:
      if (!str_val)
        str_val = malloc(sizeof(*str_val) * rows);
      if (MPI_dataset->myid == 0) {
        for (j = 0; j < rows; j++)
          sprintf(str_val[j].str_value, "%s", ((char **)SDDS_dataset->data[i])[j]);
      }
      MPI_Bcast(str_val, rows * STRING_COL_LENGTH, MPI_BYTE, 0, MPI_dataset->comm);
      if (MPI_dataset->myid) {
        for (j = 0; j < rows; j++)
          SDDS_CopyString(&(((char **)SDDS_dataset->data[i])[j]), str_val[j].str_value);
      }
      break;
    case SDDS_CHARACTER:
      MPI_Bcast(SDDS_dataset->data[i], rows, MPI_CHAR, 0, MPI_dataset->comm);
      break;
    }
  }
  if (str_val)
    free(str_val);
  return retrival;
}
