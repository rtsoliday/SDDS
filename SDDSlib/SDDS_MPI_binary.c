/**
 * @file SDDS_MPI_binary.c
 * @brief Implementation of SDDS MPI Functions
 *
 * This source file contains the implementation of functions responsible for reading
 * SDDS (Self Describing Data Set) datasets in binary format using MPI (Message Passing Interface).
 * It handles both native and non-native byte orders, ensuring compatibility across different
 * machine architectures. The functions manage buffer operations, memory allocation, and MPI
 * communication to facilitate efficient and accurate data retrieval in parallel processing environments.
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
 *  M. Borland
 *  R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"

static int32_t defaultStringLength = SDDS_MPI_STRING_COLUMN_LEN;
static int32_t number_of_string_truncated = 0;
static int32_t defaultTitleBufferSize = 2400000;
static int32_t defaultReadBufferSize = 4000000;
static int32_t defaultWriteBufferSize = 0;

#if MPI_DEBUG
static FILE *fpdeb = NULL;
#endif

/**
 * @brief Set the default read buffer size for SDDS.
 *
 * This function updates the default buffer size used for reading SDDS binary data.
 *
 * @param newSize The new buffer size to set. Must be greater than zero.
 *                If newSize is less than or equal to zero, the current default
 *                read buffer size is returned without modifying it.
 * @return The previous default read buffer size.
 */
int32_t SDDS_SetDefaultReadBufferSize(int32_t newSize) {
  int32_t previous;
  if (newSize <= 0)
    return defaultReadBufferSize;
  previous = defaultReadBufferSize;
  defaultReadBufferSize = newSize;
  return previous;
}

/**
 * @brief Set the default write buffer size for SDDS.
 *
 * This function updates the default buffer size used for writing SDDS binary data.
 *
 * @param newSize The new buffer size to set. Must be greater than zero.
 *                If newSize is less than or equal to zero, the current default
 *                write buffer size is returned without modifying it.
 * @return The previous default write buffer size.
 */
int32_t SDDS_SetDefaultWriteBufferSize(int32_t newSize) {
  int32_t previous;
  if (newSize <= 0)
    return defaultWriteBufferSize;
  previous = defaultWriteBufferSize;
  defaultWriteBufferSize = newSize;
  return previous;
}

/**
 * @brief Set the default title buffer size for SDDS.
 *
 * This function updates the default buffer size used for storing SDDS titles.
 *
 * @param newSize The new buffer size to set. Must be greater than zero.
 *                If newSize is less than or equal to zero, the current default
 *                title buffer size is returned without modifying it.
 * @return The previous default title buffer size.
 */
int32_t SDDS_SetDefaultTitleBufferSize(int32_t newSize) {
  int32_t previous;
  if (newSize <= 0)
    return defaultTitleBufferSize;
  previous = defaultTitleBufferSize;
  defaultTitleBufferSize = newSize;
  return previous;
}

/**
 * @brief Check the number of truncated strings.
 *
 * This function returns the number of strings that have been truncated
 * due to exceeding the default string length.
 *
 * @return The number of truncated strings.
 */
int32_t SDDS_CheckStringTruncated(void) {
  return number_of_string_truncated;
}

/**
 * @brief Increment the truncated strings counter.
 *
 * This function increments the count of strings that have been truncated.
 */
void SDDS_StringTuncated(void) {
  number_of_string_truncated++;
}

/**
 * @brief Set the default string length for SDDS.
 *
 * This function updates the default maximum length for string columns in SDDS.
 *
 * @param newValue The new string length to set. Must be non-negative.
 *                 If newValue is negative, the current default string length
 *                 is returned without modifying it.
 * @return The previous default string length.
 */
int32_t SDDS_SetDefaultStringLength(int32_t newValue) {
  int32_t previous;
  if (newValue < 0)
    return defaultStringLength;
  previous = defaultStringLength;
  defaultStringLength = newValue;
  return previous;
}

/**
 * @brief Write an SDDS binary page using MPI.
 *
 * This function writes an SDDS dataset as a binary page using MPI.
 * If MPI_DEBUG is enabled, it logs debugging information.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure to write.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_WriteBinaryPage(SDDS_DATASET *SDDS_dataset) {
#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteBinaryPage", SDDS_dataset);
#endif
  /* usleep(1000000); Doesn't solve problem of corrupted data */
  return SDDS_MPI_WriteContinuousBinaryPage(SDDS_dataset);
}

/**
 * @brief Write a binary string to an SDDS dataset using MPI.
 *
 * This function writes a string to the SDDS dataset in binary format using MPI.
 * If the provided string is NULL, a dummy empty string is written instead.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param string The string to write. If NULL, an empty string is written.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_WriteBinaryString(SDDS_DATASET *SDDS_dataset, char *string) {
  static char *dummy_string = "";
  int32_t length;

#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteBinaryString", SDDS_dataset);
#endif

  if (!string)
    string = dummy_string;

  length = strlen(string);
  if (!SDDS_MPI_BufferedWrite(&length, sizeof(length), SDDS_dataset))
    return 0;
  if (length && !SDDS_MPI_BufferedWrite(string, sizeof(*string) * length, SDDS_dataset))
    return 0;
  return 1;
}

/**
 * @brief Write a non-native binary string to an SDDS dataset using MPI.
 *
 * This function writes a string to the SDDS dataset in non-native binary format
 * using MPI, handling byte swapping as necessary. If the provided string is NULL,
 * a dummy empty string is written instead.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param string The string to write. If NULL, an empty string is written.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_WriteNonNativeBinaryString(SDDS_DATASET *SDDS_dataset, char *string) {
  static char *dummy_string = "";
  int32_t length;

#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteNonNativeBinaryString", SDDS_dataset);
#endif

  if (!string)
    string = dummy_string;

  length = strlen(string);
  SDDS_SwapLong(&length);
  if (!SDDS_MPI_BufferedWrite(&length, sizeof(length), SDDS_dataset))
    return 0;
  SDDS_SwapLong(&length);
  if (length && !SDDS_MPI_BufferedWrite(string, sizeof(*string) * length, SDDS_dataset))
    return 0;
  return 1;
}

/**
 * @brief Write binary parameters of an SDDS dataset using MPI.
 *
 * This function writes all non-fixed parameters of the SDDS dataset in binary format
 * using MPI. String parameters are written using SDDS_MPI_WriteBinaryString,
 * and other types are written directly to the buffer.
 *
 * Only the master processor should call this function to write SDDS headers,
 * parameters, and arrays.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_WriteBinaryParameters(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *layout;
  int32_t i;

#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteBinaryParameters", SDDS_dataset);
#endif

  /*should only master processors write SDDS hearder,parameters and arrays */
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_WriteBinaryParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].fixed_value)
      continue;
    if (layout->parameter_definition[i].type == SDDS_STRING) {
      if (!SDDS_MPI_WriteBinaryString(SDDS_dataset, *(char **)SDDS_dataset->parameter[i]))
        return 0;
    } else {
      if (!SDDS_MPI_BufferedWrite(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], SDDS_dataset))
        return 0;
    }
  }
  return (1);
}

/**
 * @brief Write non-native binary parameters of an SDDS dataset using MPI.
 *
 * This function writes all non-fixed parameters of the SDDS dataset in non-native
 * binary format using MPI, handling byte swapping as necessary.
 * String parameters are written using SDDS_MPI_WriteNonNativeBinaryString,
 * and other types are written directly to the buffer.
 *
 * Only the master processor should call this function to write SDDS headers,
 * parameters, and arrays.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_WriteNonNativeBinaryParameters(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *layout;
  int32_t i;

#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteNonNativeBinaryParameters", SDDS_dataset);
#endif

  /*should only master processors write SDDS hearder,parameters and arrays */
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_WriteBinaryParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
  SDDS_SwapEndsParameterData(SDDS_dataset);
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].fixed_value)
      continue;
    if (layout->parameter_definition[i].type == SDDS_STRING) {
      if (!SDDS_MPI_WriteNonNativeBinaryString(SDDS_dataset, *(char **)SDDS_dataset->parameter[i])) {
        SDDS_SwapEndsParameterData(SDDS_dataset);
        return 0;
      }
    } else {
      if (!SDDS_MPI_BufferedWrite(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], SDDS_dataset)) {
        SDDS_SwapEndsParameterData(SDDS_dataset);
        return 0;
      }
    }
  }
  SDDS_SwapEndsParameterData(SDDS_dataset);
  return (1);
}

/**
 * @brief Write binary arrays of an SDDS dataset using MPI.
 *
 * This function writes all arrays of the SDDS dataset in binary format using MPI.
 * It handles different types of arrays, including strings and fixed-size data types.
 *
 * Only the master processor should call this function to write SDDS headers,
 * parameters, and arrays.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_WriteBinaryArrays(SDDS_DATASET *SDDS_dataset) {
  int32_t i, j, zero = 0, writeSize = 0;
  SDDS_LAYOUT *layout;

  /*only the master processor write SDDS header, parameters and arrays */
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_WriteBinaryArray"))
    return (0);
  layout = &SDDS_dataset->layout;
  for (i = 0; i < layout->n_arrays; i++) {
    if (!SDDS_dataset->array[i].dimension) {
      for (j = 0; j < layout->array_definition[i].dimensions; j++) {
        if (!SDDS_MPI_BufferedWrite(&zero, sizeof(zero), SDDS_dataset)) {
          SDDS_SetError("Unable to write null array--failure writing dimensions (SDDS_MPI_WriteBinaryArrays)");
          return 0;
        }
      }
      continue;
    }
    writeSize = sizeof(*(SDDS_dataset->array)[i].dimension) * layout->array_definition[i].dimensions;
    if (!SDDS_MPI_BufferedWrite(SDDS_dataset->array[i].dimension, writeSize, SDDS_dataset)) {
      SDDS_SetError("Unable to write arrays--failure writing dimensions (SDDS_MPI_WriteBinaryArrays)");
      return (0);
    }
    if (layout->array_definition[i].type == SDDS_STRING) {
      for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
        if (!SDDS_MPI_WriteBinaryString(SDDS_dataset, ((char **)SDDS_dataset->array[i].data)[j])) {
          SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteBinaryArrays)");
          return (0);
        }
      }
    } else {
      writeSize = SDDS_type_size[layout->array_definition[i].type - 1] * SDDS_dataset->array[i].elements;
      if (!SDDS_MPI_BufferedWrite(SDDS_dataset->array[i].data, writeSize, SDDS_dataset)) {
        SDDS_SetError("Unable to write arrays--failure writing values (SDDS_MPI_WriteBinaryArrays)");
        return (0);
      }
    }
  }
  return (1);
}

/**
 * @brief Write non-native binary arrays of an SDDS dataset using MPI.
 *
 * This function writes all arrays of the SDDS dataset in non-native binary format
 * using MPI, handling byte swapping as necessary. It handles different types of
 * arrays, including strings and fixed-size data types.
 *
 * Only the master processor should call this function to write SDDS headers,
 * parameters, and arrays.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_WriteNonNativeBinaryArrays(SDDS_DATASET *SDDS_dataset) {
  int32_t i, j, zero = 0, writeSize = 0;
  SDDS_LAYOUT *layout;

  /*only the master processor write SDDS header, parameters and arrays */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_WriteBinaryArray"))
    return (0);
  SDDS_SwapEndsArrayData(SDDS_dataset);
  layout = &SDDS_dataset->layout;
  for (i = 0; i < layout->n_arrays; i++) {
    if (!SDDS_dataset->array[i].dimension) {
      for (j = 0; j < layout->array_definition[i].dimensions; j++) {
        if (!SDDS_MPI_BufferedWrite(&zero, sizeof(zero), SDDS_dataset)) {
          SDDS_SetError("Unable to write null array--failure writing dimensions (SDDS_MPI_WriteBinaryArrays)");
          SDDS_SwapEndsArrayData(SDDS_dataset);
          return 0;
        }
      }
      continue;
    }
    writeSize = sizeof(*(SDDS_dataset->array)[i].dimension) * layout->array_definition[i].dimensions;
    if (!SDDS_MPI_BufferedWrite(SDDS_dataset->array[i].dimension, writeSize, SDDS_dataset)) {
      SDDS_SetError("Unable to write arrays--failure writing dimensions (SDDS_MPI_WriteBinaryArrays)");
      SDDS_SwapEndsArrayData(SDDS_dataset);
      return (0);
    }
    if (layout->array_definition[i].type == SDDS_STRING) {
      for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
        if (!SDDS_MPI_WriteNonNativeBinaryString(SDDS_dataset, ((char **)SDDS_dataset->array[i].data)[j])) {
          SDDS_SetError("Unable to write arrays--failure writing string (SDDS_WriteBinaryArrays)");
          SDDS_SwapEndsArrayData(SDDS_dataset);
          return (0);
        }
      }
    } else {
      writeSize = SDDS_type_size[layout->array_definition[i].type - 1] * SDDS_dataset->array[i].elements;
      if (!SDDS_MPI_BufferedWrite(SDDS_dataset->array[i].data, writeSize, SDDS_dataset)) {
        SDDS_SetError("Unable to write arrays--failure writing values (SDDS_MPI_WriteBinaryArrays)");
        SDDS_SwapEndsArrayData(SDDS_dataset);
        return (0);
      }
    }
  }
  SDDS_SwapEndsArrayData(SDDS_dataset);
  return (1);
}

static long SDDS_MPI_write_kludge_usleep = 0;
/**
 * @brief Set the write kludge usleep duration.
 *
 * This function sets the duration (in microseconds) for the write kludge
 * sleep operation. This is used to fix write issues in certain test cases
 * by introducing a delay after writing a binary row.
 *
 * @param value The number of microseconds to sleep after writing a row.
 */
void SDDS_MPI_SetWriteKludgeUsleep(long value) {
  SDDS_MPI_write_kludge_usleep = value;
}

static short SDDS_MPI_force_file_sync = 0;
/**
 * @brief Set the file synchronization flag.
 *
 * This function enables or disables forced file synchronization after writing
 * binary rows. This can help prevent data corruption by ensuring data is flushed
 * to the file system.
 *
 * @param value A short integer where non-zero enables file synchronization,
 *              and zero disables it.
 */
void SDDS_MPI_SetFileSync(short value) {
  SDDS_MPI_force_file_sync = value;
}

/**
 * @brief Write a binary row to an SDDS dataset using MPI.
 *
 * This function writes a specific row of data from the SDDS dataset in binary
 * format using MPI. It handles different column types, including strings with
 * truncation based on the default string length. Optional write kludge delays
 * and file synchronization can be applied based on configuration.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param row The row index to write.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_WriteBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row) {
  int32_t size, type;
  int64_t i;
  SDDS_LAYOUT *layout;
  /*char buff[defaultStringLength+1], format[256]; */
  char *buff;
  char format[256];

#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteBinaryRow", SDDS_dataset);
#endif

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteBinaryRow"))
    return (0);

  layout = &SDDS_dataset->layout;

  sprintf(format, "%%-%ds", defaultStringLength);
  if (!(buff = malloc(sizeof(*buff) * (defaultStringLength + 1)))) {
    SDDS_SetError("Can not allocate memory in SDDS_MPI_WriteBinaryRow!");
    return 0;
  }
  buff[defaultStringLength] = 0;
  for (i = 0; i < layout->n_columns; i++) {
    type = layout->column_definition[i].type;
    size = SDDS_type_size[type - 1];

    if (type == SDDS_STRING) {
      if (strlen(*((char **)SDDS_dataset->data[i] + row)) <= defaultStringLength)
        sprintf(buff, format, *((char **)SDDS_dataset->data[i] + row));
      else {
        strncpy(buff, *((char **)SDDS_dataset->data[i] + row), defaultStringLength);
        number_of_string_truncated++;
      }
      if (!SDDS_MPI_WriteBinaryString(SDDS_dataset, buff)) {
        free(buff);
        return 0;
      }
    } else {
      size = SDDS_type_size[type - 1];
      if (!SDDS_MPI_BufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, SDDS_dataset)) {
        free(buff);
        return 0;
      }
    }
  }
  free(buff);
  if (SDDS_MPI_write_kludge_usleep)
    usleepSystemIndependent(SDDS_MPI_write_kludge_usleep); /*  This fixes write issue in test case. No data corruption observed. */
  if (SDDS_MPI_force_file_sync)
    MPI_File_sync(SDDS_dataset->MPI_dataset->MPI_file); /* This also seems to fix it. */
  return (1);
}

/**
 * @brief Write a non-native binary row to an SDDS dataset using MPI.
 *
 * This function writes a specific row of data from the SDDS dataset in non-native
 * binary format using MPI, handling byte swapping as necessary. It manages different
 * column types, including strings with truncation based on the default string length.
 * Optional write kludge delays and file synchronization can be applied based on configuration.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param row The row index to write.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_WriteNonNativeBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row) {
  int32_t size, type;
  int64_t i;
  SDDS_LAYOUT *layout;
  /*char buff[defaultStringLength+1], format[256]; */
  char *buff;
  char format[256];
#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteNonNativeBinaryRow", SDDS_dataset);
#endif

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteBinaryRow"))
    return (0);

  layout = &SDDS_dataset->layout;

  sprintf(format, "%%-%ds", defaultStringLength);
  if (!(buff = malloc(sizeof(*buff) * (defaultStringLength + 1)))) {
    SDDS_SetError("Can not allocate memory in SDDS_MPI_WriteNonNativeBinaryRow!");
    return 0;
  }
  buff[defaultStringLength] = 0;
  for (i = 0; i < layout->n_columns; i++) {
    type = layout->column_definition[i].type;
    size = SDDS_type_size[type - 1];

    if (type == SDDS_STRING) {
      if (strlen(*((char **)SDDS_dataset->data[i] + row)) <= defaultStringLength)
        sprintf(buff, format, *((char **)SDDS_dataset->data[i] + row));
      else {
        strncpy(buff, *((char **)SDDS_dataset->data[i] + row), defaultStringLength);
        number_of_string_truncated++;
      }
      if (!SDDS_MPI_WriteNonNativeBinaryString(SDDS_dataset, buff)) {
        free(buff);
        return 0;
      }
    } else {
      size = SDDS_type_size[type - 1];
      if (!SDDS_MPI_BufferedWrite((char *)SDDS_dataset->data[i] + row * size, size, SDDS_dataset)) {
        free(buff);
        return 0;
      }
    }
  }
  free(buff);
  if (SDDS_MPI_write_kludge_usleep)
    usleepSystemIndependent(SDDS_MPI_write_kludge_usleep); /*  This fixes write issue in test case. No data corruption observed. */
  if (SDDS_MPI_force_file_sync)
    MPI_File_sync(SDDS_dataset->MPI_dataset->MPI_file); /* This also seems to fix it. */
  return (1);
}

/**
 * @brief Get the total size of all columns in an SDDS dataset.
 *
 * This function calculates the total size in bytes required to store all columns
 * of the SDDS dataset in binary format. For string columns, it includes space
 * for the string length and the fixed maximum string length.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return The total size of all columns in bytes.
 */
MPI_Offset SDDS_MPI_Get_Column_Size(SDDS_DATASET *SDDS_dataset) {
  int64_t i;
  MPI_Offset column_offset = 0;
  SDDS_LAYOUT *layout;

  layout = &SDDS_dataset->layout;
  for (i = 0; i < layout->n_columns; i++) {
    if (layout->column_definition[i].type == SDDS_STRING)
      /* for string type column, the fixed column size defined by user use SDDS_SetDefaultStringLength(value), 
	   and the length of string is written before the string */
      column_offset += sizeof(int32_t) + defaultStringLength * sizeof(char);
    else
      column_offset += SDDS_type_size[layout->column_definition[i].type - 1];
  }
  return column_offset;
}

/**
 * @brief Buffered write to an SDDS dataset using MPI.
 *
 * This function writes data to the SDDS dataset using a buffer. If the buffer has
 * sufficient space, the data is copied into the buffer. Otherwise, the buffer is
 * flushed to the file, and the new data is either written directly or stored in the buffer.
 *
 * @param target Pointer to the data to write.
 * @param targetSize The size of the data to write, in bytes.
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_BufferedWrite(void *target, int64_t targetSize, SDDS_DATASET *SDDS_dataset) {
  SDDS_FILEBUFFER *fBuffer;
  MPI_DATASET *MPI_dataset;
  int32_t mpi_code;

#if MPI_DEBUG
  logDebug("SDDS_MPI_BufferedWrite", SDDS_dataset);
#endif
  MPI_dataset = SDDS_dataset->MPI_dataset;
  fBuffer = &(SDDS_dataset->fBuffer);

  if (!fBuffer->bufferSize) {
    if ((mpi_code = MPI_File_write(MPI_dataset->MPI_file, target, targetSize, MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_WriteBufferedWrite(MPI_File_write_at failed)", mpi_code, 0);
      return 0;
    }
    return 1;
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    memcpy((char *)fBuffer->data, (char *)target, targetSize);
    fBuffer->data += targetSize;
#ifdef DEBUG
    fprintf(stderr, "SDDS_MPI_BufferedWrite of %" PRId64 " bytes done in-memory, %" PRId64 " bytes left\n", targetSize, fBuffer->bytesLeft);
#endif
    return 1;
  } else {
    int64_t lastLeft;
    /* add back what was subtracted in test above.
       * lastLeft is the number of bytes left in the buffer before doing anything 
       * and also the number of bytes from the users data that get copied into the buffer.
       */
    lastLeft = (fBuffer->bytesLeft += targetSize);
    /* copy part of the data into the buffer and write the buffer out */
    memcpy((char *)fBuffer->data, (char *)target, (size_t)fBuffer->bytesLeft);
    if ((mpi_code = MPI_File_write(MPI_dataset->MPI_file, fBuffer->buffer, (int)(fBuffer->bufferSize), MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_WriteBufferedWrite(MPI_File_write_at failed)", mpi_code, 0);
      return 0;
    }

    /* reset the data pointer and the bytesLeft value.
       * also, determine if the remaining data is too large for the buffer.
       * if so, just write it out.
       */
    fBuffer->data = fBuffer->buffer;
    if ((targetSize -= lastLeft) > (fBuffer->bytesLeft = fBuffer->bufferSize)) {
      if ((mpi_code = MPI_File_write_at(MPI_dataset->MPI_file, (MPI_Offset)(MPI_dataset->file_offset), (char *)target + lastLeft, targetSize, MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
        SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_WriteBufferedWrite(MPI_File_write_at failed)", mpi_code, 0);
        return 0;
      }
      return 1;
    }
    /* copy remaining data into the buffer.
       * could do this with a recursive call, but this is more efficient.
       */
    memcpy((char *)fBuffer->data, (char *)target + lastLeft, targetSize);
    fBuffer->data += targetSize;
    fBuffer->bytesLeft -= targetSize;
    return 1;
  }
}

/**
 * @brief Buffered write all to an SDDS dataset using MPI.
 *
 * This function writes all data to the SDDS dataset using a buffer, ensuring that
 * all data is written even if the buffer needs to be flushed multiple times.
 *
 * @param target Pointer to the data to write.
 * @param targetSize The size of the data to write, in bytes.
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_MPI_BufferedWriteAll(void *target, int64_t targetSize, SDDS_DATASET *SDDS_dataset) {
  SDDS_FILEBUFFER *fBuffer;
  MPI_DATASET *MPI_dataset;
  int32_t mpi_code;

#if MPI_DEBUG
  logDebug("SDDS_MPI_BufferedWriteAll", SDDS_dataset);
#endif
  MPI_dataset = SDDS_dataset->MPI_dataset;
  fBuffer = &(SDDS_dataset->fBuffer);

  if (!fBuffer->bufferSize) {
    if ((mpi_code = MPI_File_write_all(MPI_dataset->MPI_file, target, targetSize, MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_WriteBufferedWrite(MPI_File_write_at failed)", mpi_code, 0);
      return 0;
    }
    return 1;
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    memcpy((char *)fBuffer->data, (char *)target, targetSize);
    fBuffer->data += targetSize;
#ifdef DEBUG
    fprintf(stderr, "SDDS_MPI_BufferedWrite of %" PRId64 " bytes done in-memory, %" PRId64 " bytes left\n", targetSize, fBuffer->bytesLeft);
#endif
    return 1;
  } else {
    int64_t lastLeft;
    /* add back what was subtracted in test above.
       * lastLeft is the number of bytes left in the buffer before doing anything 
       * and also the number of bytes from the users data that get copied into the buffer.
       */
    lastLeft = (fBuffer->bytesLeft += targetSize);
    /* copy part of the data into the buffer and write the buffer out */
    memcpy((char *)fBuffer->data, (char *)target, (size_t)fBuffer->bytesLeft);
    if ((mpi_code = MPI_File_write_all(MPI_dataset->MPI_file, fBuffer->buffer, (int)(fBuffer->bufferSize), MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_WriteBufferedWrite(MPI_File_write_at failed)", mpi_code, 0);
      return 0;
    }

    /* reset the data pointer and the bytesLeft value.
       * also, determine if the remaining data is too large for the buffer.
       * if so, just write it out.
       */
    fBuffer->data = fBuffer->buffer;
    if ((targetSize -= lastLeft) > (fBuffer->bytesLeft = fBuffer->bufferSize)) {
      if ((mpi_code = MPI_File_write_all(MPI_dataset->MPI_file, (char *)target + lastLeft, targetSize, MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
        SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_WriteBufferedWrite(MPI_File_write_at failed)", mpi_code, 0);
        return 0;
      }
      return 1;
    }
    /* copy remaining data into the buffer.
       * could do this with a recursive call, but this is more efficient.
       */
    memcpy((char *)fBuffer->data, (char *)target + lastLeft, targetSize);
    fBuffer->data += targetSize;
    fBuffer->bytesLeft -= targetSize;
    return 1;
  }
}

/**
 * @brief Flush the buffer by writing any remaining data to the MPI file.
 *
 * This function ensures that any data remaining in the buffer is written to the MPI file.
 * It handles error checking and resets the buffer pointers upon successful write.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return 
 *   - `1` on successful flush.
 *   - `0` on failure.
 */
int32_t SDDS_MPI_FlushBuffer(SDDS_DATASET *SDDS_dataset) {
  SDDS_FILEBUFFER *fBuffer;
  MPI_DATASET *MPI_dataset;
  int64_t writeBytes;
  int32_t mpi_code;

#if MPI_DEBUG
  logDebug("SDDS_MPI_FlushBuffer", SDDS_dataset);
#endif

  MPI_dataset = SDDS_dataset->MPI_dataset;
  fBuffer = &(SDDS_dataset->fBuffer);

  if (!fBuffer->bufferSize)
    return 1;

  if ((writeBytes = fBuffer->bufferSize - fBuffer->bytesLeft)) {
    if (writeBytes < 0) {
      SDDS_SetError("Unable to flush buffer: negative byte count (SDDS_FlushBuffer).");
      return 0;
    }
#ifdef DEBUG
    fprintf(stderr, "Writing %" PRId64 " bytes to disk\n", writeBytes);
#endif
    if ((mpi_code = MPI_File_write(MPI_dataset->MPI_file, fBuffer->buffer, writeBytes, MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_FlushBuffer(MPI_File_write_at failed)", mpi_code, 0);
      return 0;
    }
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
  }
  return 1;
}

/**
 * @brief Count the number of rows marked as "of interest" within a specified range.
 *
 * This function iterates through the rows of the SDDS dataset from `start_row` to `end_row`
 * and counts how many rows have their `row_flag` set to indicate they are of interest.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param start_row The starting row index (inclusive) to begin counting.
 * @param end_row The ending row index (exclusive) to stop counting.
 * @return The total number of rows marked as of interest within the specified range.
 */
int64_t SDDS_MPI_CountRowsOfInterest(SDDS_DATASET *SDDS_dataset, int64_t start_row, int64_t end_row) {
  int64_t i, rows = 0;
  for (i = start_row; i < end_row; i++) {
    if (i > SDDS_dataset->n_rows - 1)
      break;
    if (SDDS_dataset->row_flag[i])
      rows++;
  }
  return rows;
}

/**
 * @brief Get the total number of rows across all MPI processes.
 *
 * This function uses MPI_Reduce to sum the number of rows (`n_rows`) from all processes
 * and returns the total number of rows to the root process (process 0).
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return The total number of rows across all MPI processes. Only valid on the root process.
 */
int64_t SDDS_MPI_GetTotalRows(SDDS_DATASET *SDDS_dataset) {
  int64_t total_rows;
  MPI_Reduce(&(SDDS_dataset->n_rows), &total_rows, 1, MPI_INT64_T, MPI_SUM, 0, SDDS_dataset->MPI_dataset->comm);
  return total_rows;
}

/**
 * @brief Write a non-native binary page of the SDDS dataset using MPI.
 *
 * This function handles the writing of an entire SDDS dataset page in non-native binary format.
 * It manages buffer allocation, handles byte swapping for non-native formats, and ensures
 * that all parameters and arrays are correctly written. It also coordinates writing across
 * multiple MPI processes.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return 
 *   - `1` on successful write.
 *   - `0` on failure.
 */
int32_t SDDS_MPI_WriteNonNativeBinaryPage(SDDS_DATASET *SDDS_dataset) {
  int64_t row, prev_rows, i, total_rows, fixed_rows, rows;
  int mpi_code, type = 0;
  MPI_Offset column_offset, rowcount_offset, offset;
  int64_t *n_rows = NULL;
  MPI_Status status;
  int32_t min32 = INT32_MIN;

  MPI_DATASET *MPI_dataset = NULL;
  SDDS_FILEBUFFER *fBuffer;

#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteNonNativeBinaryPage", SDDS_dataset);
#endif

  MPI_dataset = SDDS_dataset->MPI_dataset;

#if MPI_DEBUG

#endif

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_WriteNonNativeBinaryPage"))
    return (0);

  fBuffer = &SDDS_dataset->fBuffer;
  if (SDDS_dataset->layout.data_mode.column_major)
    /*write by column ignores the row flag and buffer is not needed for writing data by column */
    rows = SDDS_dataset->n_rows;
  else {
    rows = SDDS_CountRowsOfInterest(SDDS_dataset);
    if (!fBuffer->buffer) {
      fBuffer->bufferSize = defaultWriteBufferSize;
      if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (fBuffer->bufferSize + 1)))) {
        SDDS_SetError("Unable to do buffered read--allocation failure (SDDS_WriteNonNativeBinaryPage)");
        return 0;
      }
      fBuffer->bytesLeft = fBuffer->bufferSize;
      fBuffer->data[0] = 0;
    }
  }
  if (MPI_dataset->n_page >= 1)
    MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL);
  rowcount_offset = MPI_dataset->file_offset + SDDS_MPI_GetTitleOffset(SDDS_dataset); /* the offset before writing rows */
  /*get total number of rows writing */
  column_offset = MPI_dataset->column_offset;
  if (!(n_rows = calloc(sizeof(*n_rows), MPI_dataset->n_processors))) {
    SDDS_SetError("Memory allocation failed!");
    return 0;
  }
  MPI_Allgather(&rows, 1, MPI_INT64_T, n_rows, 1, MPI_INT64_T, MPI_dataset->comm);
  prev_rows = 0;
  for (i = 0; i < MPI_dataset->myid; i++)
    prev_rows += n_rows[i];
  total_rows = 0;
  for (i = 0; i < MPI_dataset->n_processors; i++)
    total_rows += n_rows[i];
  if (MPI_dataset->myid == 0) {
    fixed_rows = total_rows;
    if (!fBuffer->buffer) {
      fBuffer->bufferSize = defaultWriteBufferSize;
      if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (fBuffer->bufferSize + 1)))) {
        SDDS_SetError("Unable to do buffered read--allocation failure (SDDS_WriteNonNativeBinaryPage)");
        return 0;
      }
      fBuffer->bytesLeft = fBuffer->bufferSize;
      fBuffer->data[0] = 0;
    }
    if (fixed_rows > INT32_MAX) {
      SDDS_SwapLong(&min32);
      if (!SDDS_MPI_BufferedWrite(&min32, sizeof(min32), SDDS_dataset))
        return 0;
      SDDS_SwapLong64(&fixed_rows);
      if (!SDDS_MPI_BufferedWrite(&fixed_rows, sizeof(fixed_rows), SDDS_dataset))
        return 0;
    } else {
      int32_t fixed_rows32;
      fixed_rows32 = (int32_t)fixed_rows;
      SDDS_SwapLong(&fixed_rows32);
      if (!SDDS_MPI_BufferedWrite(&fixed_rows32, sizeof(fixed_rows32), SDDS_dataset))
        return 0;
    }
    if (!SDDS_MPI_WriteNonNativeBinaryParameters(SDDS_dataset) || !SDDS_MPI_WriteNonNativeBinaryArrays(SDDS_dataset))
      return 0;
    /* flush buffer, write everything in the buffer to file */
    if (!SDDS_MPI_FlushBuffer(SDDS_dataset))
      return 0;
  }
  SDDS_SwapEndsColumnData(SDDS_dataset);
  if (SDDS_dataset->layout.data_mode.column_major) {
    /*write data by column */
    offset = rowcount_offset;
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
      type = SDDS_dataset->layout.column_definition[i].type;
      MPI_dataset->file_offset = offset + (MPI_Offset)prev_rows * SDDS_type_size[type - 1];
      if (type == SDDS_STRING) {
        SDDS_SetError("Can not write string column to SDDS3 (SDDS_MPI_WriteNonNativeBinaryPage");
        return 0;
      }
      if ((mpi_code = MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL)) != MPI_SUCCESS) {
        SDDS_MPI_GOTO_ERROR(stderr, "Unable to set view for read binary rows", mpi_code, 0);
        SDDS_SetError("Unable to set view for read binary rows");
        return 0;
      }
      if ((mpi_code = MPI_File_write(MPI_dataset->MPI_file, SDDS_dataset->data[i], rows * SDDS_type_size[type - 1], MPI_BYTE, &status)) != MPI_SUCCESS) {
        SDDS_SetError("Unable to write binary columns (SDDS_MPI_WriteNonNativeBinaryPage");
        return 0;
      }

      offset += (MPI_Offset)total_rows * SDDS_type_size[type - 1];
    }
    MPI_dataset->file_offset = offset;
  } else {
    /* now all processors write column data row by row */
    MPI_dataset->file_offset = rowcount_offset + (MPI_Offset)prev_rows * column_offset;
    /* set view to the position where the processor starts writing data */
    MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL);
    if (!MPI_dataset->collective_io) {
      row = 0;
      for (i = 0; i < SDDS_dataset->n_rows; i++) {
        if (SDDS_dataset->row_flag[i] && !SDDS_MPI_WriteNonNativeBinaryRow(SDDS_dataset, i))
          return 0;
        row++;
      }
      /*get current file position until now */
      SDDS_dataset->n_rows = row;
      if (!SDDS_MPI_FlushBuffer(SDDS_dataset))
        return 0;
    } else {
      if (!SDDS_MPI_CollectiveWriteByRow(SDDS_dataset))
        return 0;
      row = SDDS_dataset->n_rows;
    }
    MPI_Allreduce(&row, &total_rows, 1, MPI_INT64_T, MPI_SUM, MPI_dataset->comm);
    MPI_dataset->file_offset = rowcount_offset + total_rows * column_offset;
    rows = row;
  }
  SDDS_SwapEndsColumnData(SDDS_dataset);
  free(n_rows);
  /*skip checking if all data has been written for now */
  SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
  SDDS_dataset->n_rows_written = rows;
  SDDS_dataset->writing_page = 1;
  MPI_dataset->n_page++;
  return 1;
}

/**
 * @brief Write a continuous binary page of the SDDS dataset using MPI.
 *
 * This function writes an SDDS dataset page in binary format, handling both native
 * and non-native byte orders based on the environment variable `SDDS_OUTPUT_ENDIANESS`.
 * It manages buffer allocation, byte swapping, and coordinates writing across MPI processes.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @return 
 *   - `1` on successful write.
 *   - `0` on failure.
 */
int32_t SDDS_MPI_WriteContinuousBinaryPage(SDDS_DATASET *SDDS_dataset) {
  int64_t row, prev_rows, i, total_rows, fixed_rows, rows;
  int mpi_code, type = 0;
  MPI_Offset column_offset, rowcount_offset, offset;
  int64_t *n_rows = NULL;
  int32_t min32 = INT32_MIN;
  MPI_Status status;

  MPI_DATASET *MPI_dataset = NULL;
  SDDS_FILEBUFFER *fBuffer;
  char *outputEndianess = NULL;

#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteContinuousBinaryPage", SDDS_dataset);
#endif

  /* usleep(10000); This doesn't really help */

  if ((outputEndianess = getenv("SDDS_OUTPUT_ENDIANESS"))) {
    if (((strncmp(outputEndianess, "big", 3) == 0) && (SDDS_IsBigEndianMachine() == 0)) || ((strncmp(outputEndianess, "little", 6) == 0) && (SDDS_IsBigEndianMachine() == 1)))
      return SDDS_MPI_WriteNonNativeBinaryPage(SDDS_dataset);
  }

  MPI_dataset = SDDS_dataset->MPI_dataset;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_WriteContinuousBinaryPage"))
    return (0);

  fBuffer = &SDDS_dataset->fBuffer;
  if (SDDS_dataset->layout.data_mode.column_major)
    /*write by column ignores the row flag and buffer is not needed for writing data by column */
    rows = SDDS_dataset->n_rows;
  else {
    rows = SDDS_CountRowsOfInterest(SDDS_dataset);
    if (!fBuffer->buffer) {
      fBuffer->bufferSize = defaultWriteBufferSize;
      if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (fBuffer->bufferSize + 1)))) {
        SDDS_SetError("Unable to do buffered read--allocation failure (SDDS_WriteContinuousBinaryPage)");
        return 0;
      }
      fBuffer->bytesLeft = fBuffer->bufferSize;
      fBuffer->data[0] = 0;
    }
  }
  if (MPI_dataset->n_page >= 1)
    MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL);
  rowcount_offset = MPI_dataset->file_offset + SDDS_MPI_GetTitleOffset(SDDS_dataset); /* the offset before writing rows */
  /*get total number of rows writing */
  column_offset = MPI_dataset->column_offset;
  if (!(n_rows = calloc(sizeof(*n_rows), MPI_dataset->n_processors))) {
    SDDS_SetError("Memory allocation failed!");
    return 0;
  }
  MPI_Allgather(&rows, 1, MPI_INT64_T, n_rows, 1, MPI_INT64_T, MPI_dataset->comm);
  prev_rows = 0;
  for (i = 0; i < MPI_dataset->myid; i++)
    prev_rows += n_rows[i];
  total_rows = 0;
  for (i = 0; i < MPI_dataset->n_processors; i++)
    total_rows += n_rows[i];
  if (MPI_dataset->myid == 0) {
    fixed_rows = total_rows;
    if (!fBuffer->buffer) {
      fBuffer->bufferSize = defaultWriteBufferSize;
      if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (fBuffer->bufferSize + 1)))) {
        SDDS_SetError("Unable to do buffered read--allocation failure (SDDS_WriteContinuousBinaryPage)");
        return 0;
      }
      fBuffer->bytesLeft = fBuffer->bufferSize;
      fBuffer->data[0] = 0;
    }
    if (fixed_rows > INT32_MAX) {
      if (!SDDS_MPI_BufferedWrite(&min32, sizeof(min32), SDDS_dataset))
        return 0;
      if (!SDDS_MPI_BufferedWrite(&fixed_rows, sizeof(fixed_rows), SDDS_dataset))
        return 0;
    } else {
      int32_t fixed_rows32;
      fixed_rows32 = (int32_t)fixed_rows;
      if (!SDDS_MPI_BufferedWrite(&fixed_rows32, sizeof(fixed_rows32), SDDS_dataset))
        return 0;
    }
    if (!SDDS_MPI_WriteBinaryParameters(SDDS_dataset) || !SDDS_MPI_WriteBinaryArrays(SDDS_dataset))
      return 0;
    /* flush buffer, write everything in the buffer to file */
    if (!SDDS_MPI_FlushBuffer(SDDS_dataset))
      return 0;
  }

  if (SDDS_dataset->layout.data_mode.column_major) {
    /*write data by column */
    offset = rowcount_offset;
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
      type = SDDS_dataset->layout.column_definition[i].type;
      MPI_dataset->file_offset = offset + (MPI_Offset)prev_rows * SDDS_type_size[type - 1];
      if (type == SDDS_STRING) {
        SDDS_SetError("Can not write string column to SDDS3 (SDDS_MPI_WriteContinuousBinaryPage");
        return 0;
      }
      if ((mpi_code = MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL)) != MPI_SUCCESS) {
        SDDS_MPI_GOTO_ERROR(stderr, "Unable to set view for read binary rows", mpi_code, 0);
        SDDS_SetError("Unable to set view for read binary rows");
        return 0;
      }
      if ((mpi_code = MPI_File_write(MPI_dataset->MPI_file, SDDS_dataset->data[i], rows * SDDS_type_size[type - 1], MPI_BYTE, &status)) != MPI_SUCCESS) {
        SDDS_SetError("Unable to write binary columns (SDDS_MPI_WriteContinuousBinaryPage");
        return 0;
      }

      offset += (MPI_Offset)total_rows * SDDS_type_size[type - 1];
    }
    MPI_dataset->file_offset = offset;
  } else {
    /* now all processors write column data row by row */
    MPI_dataset->file_offset = rowcount_offset + (MPI_Offset)prev_rows * column_offset;
    /* set view to the position where the processor starts writing data */
    MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL);
    if (!MPI_dataset->collective_io) {
      row = 0;
      for (i = 0; i < SDDS_dataset->n_rows; i++) {
        if (SDDS_dataset->row_flag[i] && !SDDS_MPI_WriteBinaryRow(SDDS_dataset, i))
          return 0;
        row++;
      }
      /*get current file position until now */
      SDDS_dataset->n_rows = row;
      if (!SDDS_MPI_FlushBuffer(SDDS_dataset))
        return 0;
    } else {
      if (!SDDS_MPI_CollectiveWriteByRow(SDDS_dataset))
        return 0;
      row = SDDS_dataset->n_rows;
    }
    MPI_Allreduce(&row, &total_rows, 1, MPI_INT64_T, MPI_SUM, MPI_dataset->comm);
    MPI_dataset->file_offset = rowcount_offset + total_rows * column_offset;
    rows = row;
  }
  free(n_rows);
  /*skip checking if all data has been written for now */
  SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
  SDDS_dataset->n_rows_written = rows;
  SDDS_dataset->writing_page = 1;
  MPI_dataset->n_page++;

  return 1;
}

/**
 * @brief Buffered read from an SDDS dataset using MPI.
 *
 * This function reads data from the SDDS dataset using a buffer. It handles cases
 * where sufficient data is already in the buffer and cases where additional data
 * needs to be read from the MPI file. It also manages end-of-file conditions.
 *
 * @param target Pointer to the buffer where the read data will be stored.
 * @param targetSize The size of the data to read, in bytes.
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param fBuffer Pointer to the SDDS_FILEBUFFER structure managing the buffer.
 * @return 
 *   - `1` on successful read.
 *   - `0` on partial read or failure.
 *   - `-1` if end-of-file is reached.
 */
int32_t SDDS_MPI_BufferedRead(void *target, int64_t targetSize, SDDS_DATASET *SDDS_dataset, SDDS_FILEBUFFER *fBuffer) {
  int32_t mpi_code;
  int32_t bytesRead, count;
  MPI_Status status;
  MPI_DATASET *MPI_dataset = SDDS_dataset->MPI_dataset;

  if (!fBuffer || !fBuffer->bufferSize) {
    /* just read into users buffer or seek if no buffer given */
    if (!target)
      mpi_code = MPI_File_seek(MPI_dataset->MPI_file, targetSize, MPI_SEEK_CUR);
    else {
      mpi_code = MPI_File_read(MPI_dataset->MPI_file, target, targetSize, MPI_BYTE, &status);
      MPI_Get_count(&status, MPI_BYTE, &bytesRead);
      if (!bytesRead) {
        MPI_dataset->end_of_file = 1;
        return -1;
      }
      if (bytesRead < targetSize)
        return 0;
    }
    if (mpi_code != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_BufferedRead(MPI_File_read failed)", mpi_code, 0);
      return 0;
    }
    return 1;
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    /* sufficient data is already in the buffer */
    if (target) {
      memcpy((char *)target, (char *)fBuffer->data, targetSize);
    }
    fBuffer->data += targetSize;
    return 1;
  } else {
    /* need to read additional data into buffer */
    int64_t bytesNeeded, offset;
    fBuffer->bytesLeft += targetSize; /* adds back amount subtracted above */
    /* first, use the data that is already available. this cleans out the buffer */
    if ((offset = fBuffer->bytesLeft)) {
      /* some data is available in the buffer */
      if (target) {
        memcpy((char *)target, (char *)fBuffer->data, offset);
      }
      bytesNeeded = targetSize - offset;
      fBuffer->bytesLeft = 0;
    } else {
      bytesNeeded = targetSize;
    }
    fBuffer->data = fBuffer->buffer;
    if (fBuffer->bufferSize < bytesNeeded) {
      /* just read what is needed directly into user's memory or seek */
      if (!target)
        mpi_code = MPI_File_seek(MPI_dataset->MPI_file, targetSize, MPI_SEEK_CUR);
      else {
        mpi_code = MPI_File_read(MPI_dataset->MPI_file, (char *)target + offset, bytesNeeded, MPI_BYTE, &status);
        MPI_Get_count(&status, MPI_BYTE, &bytesRead);
        if (!bytesRead) {
          MPI_dataset->end_of_file = 1;
          return -1;
        }
        if (bytesRead < bytesNeeded)
          return 0;
      }
      if (mpi_code != MPI_SUCCESS) {
        SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_ReadBufferedRead(MPI_File_read failed)", mpi_code, 0);
        return 0;
      }
      return 1;
    }
    /* fill the buffer */
    mpi_code = MPI_File_read(MPI_dataset->MPI_file, fBuffer->data, (int)(fBuffer->bufferSize), MPI_BYTE, &status);
    MPI_Get_count(&status, MPI_BYTE, &count);
    fBuffer->bytesLeft = count;
    if (!(fBuffer->bytesLeft))
      MPI_dataset->end_of_file = 1;
    if (fBuffer->bytesLeft < bytesNeeded)
      return 0;
    if (target)
      memcpy((char *)target + offset, (char *)fBuffer->data, bytesNeeded);
    fBuffer->data += bytesNeeded;
    fBuffer->bytesLeft -= bytesNeeded;
    return 1;
  }
}

/**
 * @brief Buffered read all from an SDDS dataset using MPI.
 *
 * This function reads all requested data from the SDDS dataset using a buffer,
 * ensuring that the entire requested data is read unless end-of-file is reached.
 * It handles buffer management and MPI collective I/O operations.
 *
 * @param target Pointer to the buffer where the read data will be stored.
 * @param targetSize The size of the data to read, in bytes.
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param fBuffer Pointer to the SDDS_FILEBUFFER structure managing the buffer.
 * @return 
 *   - `1` on successful read of all requested data.
 *   - `0` on partial read or failure.
 *   - `-1` if end-of-file is reached.
 */
int32_t SDDS_MPI_BufferedReadAll(void *target, int64_t targetSize, SDDS_DATASET *SDDS_dataset, SDDS_FILEBUFFER *fBuffer) {
  int32_t mpi_code, bytesRead, count;
  MPI_Status status;
  MPI_DATASET *MPI_dataset = SDDS_dataset->MPI_dataset;

  if (!fBuffer || !fBuffer->bufferSize) {
    /* just read into users buffer or seek if no buffer given */
    if (!target)
      mpi_code = MPI_File_seek(MPI_dataset->MPI_file, targetSize, MPI_SEEK_CUR);
    else {
      mpi_code = MPI_File_read_all(MPI_dataset->MPI_file, target, targetSize, MPI_BYTE, &status);
      MPI_Get_count(&status, MPI_BYTE, &bytesRead);
      if (!bytesRead) {
        MPI_dataset->end_of_file = 1;
        return -1;
      }
      if (bytesRead < targetSize)
        return 0;
    }
    if (mpi_code != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_BufferedRead(MPI_File_read failed)", mpi_code, 0);
      return 0;
    }
    return 1;
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    /* sufficient data is already in the buffer */
    if (target) {
      memcpy((char *)target, (char *)fBuffer->data, targetSize);
    }
    fBuffer->data += targetSize;
    return 1;
  } else {
    /* need to read additional data into buffer */
    int64_t bytesNeeded, offset;
    fBuffer->bytesLeft += targetSize; /* adds back amount subtracted above */
    /* first, use the data that is already available. this cleans out the buffer */
    if ((offset = fBuffer->bytesLeft)) {
      /* some data is available in the buffer */
      if (target) {
        memcpy((char *)target, (char *)fBuffer->data, offset);
      }
      bytesNeeded = targetSize - offset;
      fBuffer->bytesLeft = 0;
    } else {
      bytesNeeded = targetSize;
    }
    fBuffer->data = fBuffer->buffer;
    if (fBuffer->bufferSize < bytesNeeded) {
      /* just read what is needed directly into user's memory or seek */
      if (!target)
        mpi_code = MPI_File_seek(MPI_dataset->MPI_file, targetSize, MPI_SEEK_CUR);
      else {
        mpi_code = MPI_File_read_all(MPI_dataset->MPI_file, (char *)target + offset, bytesNeeded, MPI_BYTE, &status);
        MPI_Get_count(&status, MPI_BYTE, &bytesRead);
        if (!bytesRead) {
          MPI_dataset->end_of_file = 1;
          return -1;
        }
        if (bytesRead < bytesNeeded)
          return 0;
      }
      if (mpi_code != MPI_SUCCESS) {
        SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_ReadBufferedRead(MPI_File_read failed)", mpi_code, 0);
        return 0;
      }
      return 1;
    }
    /* fill the buffer */
    mpi_code = MPI_File_read_all(MPI_dataset->MPI_file, fBuffer->data, (int)(fBuffer->bufferSize), MPI_BYTE, &status);
    MPI_Get_count(&status, MPI_BYTE, &count);
    fBuffer->bytesLeft = count;
    if (!(fBuffer->bytesLeft))
      MPI_dataset->end_of_file = 1;
    if (fBuffer->bytesLeft < bytesNeeded)
      return 0;
    if (target)
      memcpy((char *)target + offset, (char *)fBuffer->data, bytesNeeded);
    fBuffer->data += bytesNeeded;
    fBuffer->bytesLeft -= bytesNeeded;
    return 1;
  }
}

/**
 * @brief Read a binary string from the SDDS dataset using MPI.
 *
 * This function reads a string from the SDDS dataset in binary format using MPI.
 * It first reads the length of the string, allocates memory for the string,
 * and then reads the string data itself. If the `skip` parameter is set,
 * the string data is read and discarded.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param fBuffer Pointer to the SDDS_FILEBUFFER structure managing the buffer.
 * @param skip If non-zero, the string data is read and skipped (not stored).
 * @return 
 *   - Pointer to the allocated string on success.
 *   - `0` if reading the length fails or if the length is negative.
 *   - `NULL` if memory allocation fails or reading the string data fails.
 */
char *SDDS_MPI_ReadBinaryString(SDDS_DATASET *SDDS_dataset, SDDS_FILEBUFFER *fBuffer, int32_t skip) {
  int32_t length;
  char *string;
  if (!SDDS_MPI_BufferedRead(&length, sizeof(length), SDDS_dataset, fBuffer) || length < 0)
    return (0);
  if (!(string = SDDS_Malloc(sizeof(*string) * (length + 1))))
    return (NULL);
  if (length && !SDDS_MPI_BufferedRead(skip ? NULL : string, sizeof(*string) * length, SDDS_dataset, fBuffer))
    return (NULL);
  string[length] = 0;
  return (string);
}

/**
 * @brief Read a non-native binary string from the SDDS dataset using MPI.
 *
 * This function reads a string from the SDDS dataset in non-native binary format using MPI.
 * It handles byte swapping for the string length to match the non-native byte order.
 * After reading the length, it allocates memory for the string and reads the string data.
 * If the `skip` parameter is set, the string data is read and discarded.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param fBuffer Pointer to the SDDS_FILEBUFFER structure managing the buffer.
 * @param skip If non-zero, the string data is read and skipped (not stored).
 * @return 
 *   - Pointer to the allocated string on success.
 *   - `0` if reading the length fails or if the length is negative.
 *   - `NULL` if memory allocation fails or reading the string data fails.
 */
char *SDDS_MPI_ReadNonNativeBinaryString(SDDS_DATASET *SDDS_dataset, SDDS_FILEBUFFER *fBuffer, int32_t skip) {
  int32_t length;
  char *string;

  if (!SDDS_MPI_BufferedRead(&length, sizeof(length), SDDS_dataset, fBuffer))
    return (0);
  SDDS_SwapLong(&length);
  if (length < 0)
    return (0);
  if (!(string = SDDS_Malloc(sizeof(*string) * (length + 1))))
    return (NULL);
  if (length && !SDDS_MPI_BufferedRead(skip ? NULL : string, sizeof(*string) * length, SDDS_dataset, fBuffer))
    return (NULL);
  string[length] = 0;
  return (string);
}

/**
 * @brief Read binary parameters from the SDDS dataset using MPI.
 *
 * This function reads all non-fixed parameters from the SDDS dataset in binary format using MPI.
 * It iterates through each parameter, handling string parameters by reading them as binary strings,
 * and other types by reading their binary representations directly into the dataset's parameter storage.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param fBuffer Pointer to the SDDS_FILEBUFFER structure managing the buffer.
 * @return 
 *   - `1` on successful read of all parameters.
 *   - `0` on failure to read any parameter.
 */
int32_t SDDS_MPI_ReadBinaryParameters(SDDS_DATASET *SDDS_dataset, SDDS_FILEBUFFER *fBuffer) {
  int32_t i;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  static char buffer[SDDS_MAXLINE];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_ReadBinaryParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_parameters)
    return (1);
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
      continue;
    if (layout->parameter_definition[i].fixed_value) {
      strcpy(buffer, layout->parameter_definition[i].fixed_value);
      if (!SDDS_ScanData(buffer, layout->parameter_definition[i].type, 0, SDDS_dataset->parameter[i], 0, 1)) {
        SDDS_SetError("Unable to read page--parameter scanning error (SDDS_MPI_ReadBinaryParameters)");
        return (0);
      }
    } else if (layout->parameter_definition[i].type == SDDS_STRING) {
      if (*(char **)SDDS_dataset->parameter[i])
        free(*(char **)SDDS_dataset->parameter[i]);
      if (!(*((char **)SDDS_dataset->parameter[i]) = SDDS_MPI_ReadBinaryString(SDDS_dataset, fBuffer, 0))) {
        SDDS_SetError("Unable to read parameters--failure reading string (SDDS_MPI_ReadBinaryParameters)");
        return (0);
      }
    } else {
      if (!SDDS_MPI_BufferedRead(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], SDDS_dataset, fBuffer)) {
        SDDS_SetError("Unable to read parameters--failure reading value (SDDS_MPI_ReadBinaryParameters)");
        return (0);
      }
    }
  }
  return (1);
}

/**
 * @brief Read a binary row from the SDDS dataset using MPI.
 *
 * This function reads a specific row of data from the SDDS dataset in binary format using MPI.
 * It handles different column types, including strings and fixed-size data types. If the `skip` parameter
 * is set, string data is read and discarded.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param row The row index to read.
 * @param skip If non-zero, string data in the row is read but not stored.
 * @return 
 *   - `1` on successful read of the row.
 *   - `0` on failure to read any part of the row.
 */
int32_t SDDS_MPI_ReadBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row, int32_t skip) {
  int64_t i;
  int32_t type, size;
  SDDS_LAYOUT *layout;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_ReadBinaryRow"))
    return (0);
  layout = &SDDS_dataset->layout;
  fBuffer = &SDDS_dataset->fBuffer;

  for (i = 0; i < layout->n_columns; i++) {
    if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
      continue;
    if ((type = layout->column_definition[i].type) == SDDS_STRING) {
      if (!skip) {
        if (((char ***)SDDS_dataset->data)[i][row])
          free((((char ***)SDDS_dataset->data)[i][row]));
        if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_MPI_ReadBinaryString(SDDS_dataset, fBuffer, 0))) {
          SDDS_SetError("Unable to read rows--failure reading string (SDDS_MPI_ReadBinaryRows)");
          return (0);
        }
      } else {
        if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_MPI_ReadBinaryString(SDDS_dataset, fBuffer, 1))) {
          SDDS_SetError("Unable to read rows--failure reading string (SDDS_MPI_ReadBinaryRows)");
          return 0;
        }
      }
    } else {
      size = SDDS_type_size[type - 1];
      if (!SDDS_MPI_BufferedRead(skip ? NULL : (char *)SDDS_dataset->data[i] + row * size, size, SDDS_dataset, fBuffer)) {
        SDDS_SetError("Unable to read row--failure reading value (SDDS_MPI_ReadBinaryRow)");
        return (0);
      }
    }
  }
  return (1);
}

/**
 * @brief Read binary arrays from the SDDS dataset using MPI.
 *
 * This function reads all arrays defined in the SDDS dataset in binary format using MPI.
 * It handles reading array dimensions, allocating memory for array data, and reading
 * the actual array data. For string arrays, it reads each string individually.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param fBuffer Pointer to the SDDS_FILEBUFFER structure managing the buffer.
 * @return 
 *   - `1` on successful read of all arrays.
 *   - `0` on failure to read any part of the arrays.
 */
int32_t SDDS_MPI_ReadBinaryArrays(SDDS_DATASET *SDDS_dataset, SDDS_FILEBUFFER *fBuffer) {
  int32_t i, j;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  /*  static char buffer[SDDS_MAXLINE]; */
  SDDS_ARRAY *array;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_ReadBinaryArrays"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_arrays)
    return (1);

  if (!SDDS_dataset->array) {
    SDDS_SetError("Unable to read array--pointer to structure storage area is NULL (SDDS_MPI_ReadBinaryArrays)");
    return (0);
  }
  for (i = 0; i < layout->n_arrays; i++) {
    array = SDDS_dataset->array + i;
    if (array->definition && !SDDS_FreeArrayDefinition(array->definition)) {
      SDDS_SetError("Unable to get array--array definition corrupted (SDDS_MPI_ReadBinaryArrays)");
      return (0);
    }
    if (!SDDS_CopyArrayDefinition(&array->definition, layout->array_definition + i)) {
      SDDS_SetError("Unable to read array--definition copy failed (SDDS_MPI_ReadBinaryArrays)");
      return (0);
    }
    /*if (array->dimension) free(array->dimension); */
    if (!(array->dimension = SDDS_Realloc(array->dimension, sizeof(*array->dimension) * array->definition->dimensions))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_MPI_ReadBinaryArrays)");
      return (0);
    }
    if (!SDDS_MPI_BufferedRead(array->dimension, sizeof(*array->dimension) * array->definition->dimensions, SDDS_dataset, fBuffer)) {
      SDDS_SetError("Unable to read arrays--failure reading dimensions (SDDS_MPI_ReadBinaryArrays)");
      return (0);
    }
    array->elements = 1;
    for (j = 0; j < array->definition->dimensions; j++)
      array->elements *= array->dimension[j];
    if (array->data)
      free(array->data);
    array->data = array->pointer = NULL;
    if (array->elements == 0)
      continue;
    if (array->elements < 0) {
      SDDS_SetError("Unable to read array--number of elements is negative (SDDS_MPI_ReadBinaryArrays)");
      return (0);
    }
    if (!(array->data = SDDS_Realloc(array->data, array->elements * SDDS_type_size[array->definition->type - 1]))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_MPI_ReadBinaryArrays)");
      return (0);
    }
    if (array->definition->type == SDDS_STRING) {
      for (j = 0; j < array->elements; j++) {
        if (!(((char **)(array->data))[j] = SDDS_MPI_ReadBinaryString(SDDS_dataset, fBuffer, 0))) {
          SDDS_SetError("Unable to read arrays--failure reading string (SDDS_MPI_ReadBinaryArrays)");
          return (0);
        }
      }
    } else {
      if (!SDDS_MPI_BufferedRead(array->data, SDDS_type_size[array->definition->type - 1] * array->elements, SDDS_dataset, fBuffer)) {
        SDDS_SetError("Unable to read arrays--failure reading values (SDDS_MPI_ReadBinaryArrays)");
        return (0);
      }
    }
  }
  return (1);
}

/**
 * @brief Read non-native binary parameters from the SDDS dataset using MPI.
 *
 * This function reads all non-fixed parameters from the SDDS dataset in non-native
 * binary format using MPI. It handles byte swapping for parameter data to match
 * the non-native byte order. String parameters are read as non-native binary strings,
 * and other types are read directly into the dataset's parameter storage.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param fBuffer Pointer to the SDDS_FILEBUFFER structure managing the buffer.
 * @return 
 *   - `1` on successful read of all parameters.
 *   - `0` on failure to read any parameter.
 */
int32_t SDDS_MPI_ReadNonNativeBinaryParameters(SDDS_DATASET *SDDS_dataset, SDDS_FILEBUFFER *fBuffer) {
  int32_t i;
  SDDS_LAYOUT *layout;
  static char buffer[SDDS_MAXLINE];

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_ReadNonNativeBinaryParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_parameters)
    return (1);
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
      continue;
    if (layout->parameter_definition[i].fixed_value) {
      strcpy(buffer, layout->parameter_definition[i].fixed_value);
      if (!SDDS_ScanData(buffer, layout->parameter_definition[i].type, 0, SDDS_dataset->parameter[i], 0, 1)) {
        SDDS_SetError("Unable to read page--parameter scanning error (SDDS_MPI_ReadNonNativeBinaryParameters)");
        return (0);
      }
    } else if (layout->parameter_definition[i].type == SDDS_STRING) {
      if (*(char **)SDDS_dataset->parameter[i])
        free(*(char **)SDDS_dataset->parameter[i]);
      if (!(*((char **)SDDS_dataset->parameter[i]) = SDDS_MPI_ReadNonNativeBinaryString(SDDS_dataset, fBuffer, 0))) {
        SDDS_SetError("Unable to read parameters--failure reading string (SDDS_MPI_ReadNonNativeBinaryParameters)");
        return (0);
      }
    } else {
      if (!SDDS_MPI_BufferedRead(SDDS_dataset->parameter[i], SDDS_type_size[layout->parameter_definition[i].type - 1], SDDS_dataset, fBuffer)) {
        SDDS_SetError("Unable to read parameters--failure reading value (SDDS_MPI_ReadNonNativeBinaryParameters)");
        return (0);
      }
    }
  }
  SDDS_SwapEndsParameterData(SDDS_dataset);
  return (1);
}

/**
 * @brief Reads non-native binary arrays from a binary file buffer into the SDDS dataset using MPI.
 *
 * This function validates the provided SDDS dataset and reads array definitions and data from a non-native binary file buffer.
 * It handles byte swapping for endianness, allocates memory for array dimensions and data, and processes string arrays individually.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure where the arrays will be stored.
 * @param[in] fBuffer Pointer to the SDDS_FILEBUFFER structure containing the binary data to be read.
 *
 * @return int32_t Returns 1 on successful read, 0 on failure.
 */
int32_t SDDS_MPI_ReadNonNativeBinaryArrays(SDDS_DATASET *SDDS_dataset, SDDS_FILEBUFFER *fBuffer) {
  int32_t i, j;
  SDDS_LAYOUT *layout;
  SDDS_ARRAY *array;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_ReadNonNativeBinaryArrays"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (!layout->n_arrays)
    return (1);

  if (!SDDS_dataset->array) {
    SDDS_SetError("Unable to read array--pointer to structure storage area is NULL (SDDS_MPI_ReadNonNativeBinaryArrays)");
    return (0);
  }
  for (i = 0; i < layout->n_arrays; i++) {
    array = SDDS_dataset->array + i;
    if (array->definition && !SDDS_FreeArrayDefinition(array->definition)) {
      SDDS_SetError("Unable to get array--array definition corrupted (SDDS_MPI_ReadNonNativeBinaryArrays)");
      return (0);
    }
    if (!SDDS_CopyArrayDefinition(&array->definition, layout->array_definition + i)) {
      SDDS_SetError("Unable to read array--definition copy failed (SDDS_MPI_ReadNonNativeBinaryArrays)");
      return (0);
    }
    /*if (array->dimension) free(array->dimension); */
    if (!(array->dimension = SDDS_Realloc(array->dimension, sizeof(*array->dimension) * array->definition->dimensions))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_MPI_ReadNonNativeBinaryArrays)");
      return (0);
    }
    if (!SDDS_MPI_BufferedRead(array->dimension, sizeof(*array->dimension) * array->definition->dimensions, SDDS_dataset, fBuffer)) {
      SDDS_SetError("Unable to read arrays--failure reading dimensions (SDDS_MPI_ReadNonNativeBinaryArrays)");
      return (0);
    }
    SDDS_SwapLong(array->dimension);
    array->elements = 1;
    for (j = 0; j < array->definition->dimensions; j++)
      array->elements *= array->dimension[j];
    if (array->data)
      free(array->data);
    array->data = array->pointer = NULL;
    if (array->elements == 0)
      continue;
    if (array->elements < 0) {
      SDDS_SetError("Unable to read array--number of elements is negative (SDDS_MPI_ReadNonNativeBinaryArrays)");
      return (0);
    }
    if (!(array->data = SDDS_Realloc(array->data, array->elements * SDDS_type_size[array->definition->type - 1]))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_MPI_ReadNonNativeBinaryArrays)");
      return (0);
    }
    if (array->definition->type == SDDS_STRING) {
      for (j = 0; j < array->elements; j++) {
        if (!(((char **)(array->data))[j] = SDDS_MPI_ReadNonNativeBinaryString(SDDS_dataset, fBuffer, 0))) {
          SDDS_SetError("Unable to read arrays--failure reading string (SDDS_MPI_ReadNonNativeBinaryArrays)");
          return (0);
        }
      }
    } else {
      if (!SDDS_MPI_BufferedRead(array->data, SDDS_type_size[array->definition->type - 1] * array->elements, SDDS_dataset, fBuffer)) {
        SDDS_SetError("Unable to read arrays--failure reading values (SDDS_MPI_ReadNonNativeBinaryArrays)");
        return (0);
      }
    }
  }
  SDDS_SwapEndsArrayData(SDDS_dataset);
  return (1);
}

/**
 * @brief Reads a single non-native binary row from a binary file buffer into the SDDS dataset using MPI.
 *
 * This function reads the data for a specific row from the binary file buffer. It handles different data types, including strings,
 * and can optionally skip reading the row data. If skipping, it advances the file buffer without storing the data.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure where the row data will be stored.
 * @param[in] row The index of the row to read.
 * @param[in] skip If non-zero, the function will skip reading the row data without storing it.
 *
 * @return int32_t Returns 1 on successful read, 0 on failure.
 */
int32_t SDDS_MPI_ReadNonNativeBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row, int32_t skip) {
  int64_t i;
  int32_t type, size;
  SDDS_LAYOUT *layout;
  SDDS_FILEBUFFER *fBuffer;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_ReadNonNativeBinaryRow"))
    return (0);
  layout = &SDDS_dataset->layout;
  fBuffer = &SDDS_dataset->fBuffer;

  for (i = 0; i < layout->n_columns; i++) {
    if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
      continue;
    if ((type = layout->column_definition[i].type) == SDDS_STRING) {
      if (!skip) {
        if (((char ***)SDDS_dataset->data)[i][row])
          free((((char ***)SDDS_dataset->data)[i][row]));
        if (!(((char ***)SDDS_dataset->data)[i][row] = SDDS_MPI_ReadNonNativeBinaryString(SDDS_dataset, fBuffer, 0))) {
          SDDS_SetError("Unable to read rows--failure reading string (SDDS_MPI_ReadNonNativeBinaryRow)");
          return (0);
        }
      } else {
        if (!SDDS_MPI_ReadNonNativeBinaryString(SDDS_dataset, fBuffer, 1)) {
          SDDS_SetError("Unable to read rows--failure reading string (SDDS_MPI_ReadNonNativeBinaryRow)");
          return 0;
        }
      }
    } else {
      size = SDDS_type_size[type - 1];
      if (!SDDS_MPI_BufferedRead(skip ? NULL : (char *)SDDS_dataset->data[i] + row * size, size, SDDS_dataset, fBuffer)) {
        SDDS_SetError("Unable to read row--failure reading value (SDDS_MPI_ReadNonNativeBinaryRow)");
        return (0);
      }
    }
  }
  return (1);
}

/**
 * @brief Broadcasts the title data (parameters and arrays) of the SDDS dataset to all MPI processes.
 *
 * This function gathers the title data from the master process and broadcasts it to all other MPI processes.
 * It handles the total number of rows, parameters, and array definitions, ensuring that each process has consistent dataset metadata.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure containing the title data to broadcast.
 *
 * @return int32_t Returns 1 on successful broadcast, 0 on failure.
 */
int32_t SDDS_MPI_BroadcastTitleData(SDDS_DATASET *SDDS_dataset) {
  char *par_data = NULL;
  int32_t type, size;
  int64_t i, count = 0, *data_len = NULL;
  MPI_DATASET *MPI_dataset;
  SDDS_LAYOUT *layout;
  char *string = NULL;

  MPI_dataset = SDDS_dataset->MPI_dataset;
  layout = &(SDDS_dataset->layout);
  if (!layout->n_parameters && !layout->n_arrays) {
    /* no parameters and arrays, only broadcast total_rows */
    MPI_Bcast(&(MPI_dataset->total_rows), 1, MPI_INT64_T, 0, MPI_dataset->comm);
  } else {
    /* broadcast the total_rows and parameter data */
    data_len = calloc(sizeof(*data_len), 1 + layout->n_parameters);
    if (MPI_dataset->myid == 0) {
      data_len[0] = sizeof(int64_t);
      count = data_len[0];
      for (i = 0; i < layout->n_parameters; i++) {
        type = layout->parameter_definition[i].type;
        if (type == SDDS_STRING)
          data_len[i + 1] = strlen(*((char **)SDDS_dataset->parameter[i])) * sizeof(char);
        else
          data_len[i + 1] = SDDS_type_size[type - 1];
        ;
        count += data_len[i + 1];
      }
      par_data = (char *)malloc(sizeof(char) * count);
      memcpy((char *)par_data, &(MPI_dataset->total_rows), data_len[0]);
      count = data_len[0];
      for (i = 0; i < layout->n_parameters; i++) {
        if (layout->parameter_definition[i].type == SDDS_STRING)
          memcpy((char *)par_data + count, *(char **)SDDS_dataset->parameter[i], data_len[i + 1]);
        else
          memcpy((char *)par_data + count, (char *)SDDS_dataset->parameter[i], data_len[i + 1]);
        count += data_len[i + 1];
      }
    }
    MPI_Bcast(data_len, 1 + layout->n_parameters, MPI_INT64_T, 0, MPI_dataset->comm);
    if (MPI_dataset->myid) {
      count = data_len[0];
      for (i = 0; i < layout->n_parameters; i++)
        count += data_len[i + 1];
      par_data = (char *)malloc(sizeof(char) * count);
    }

    MPI_Bcast(par_data, count, MPI_BYTE, 0, MPI_dataset->comm);
    if (!SDDS_StartPage(SDDS_dataset, 0)) {
      SDDS_SetError("Unable to read page--couldn't start page (SDDS_MPI_BroadcastTitleData)");
      return (0);
    }
    if (MPI_dataset->myid) {
      memcpy(&(MPI_dataset->total_rows), (char *)par_data, data_len[0]);
      count = data_len[0];
      for (i = 0; i < layout->n_parameters; i++) {
        if (layout->parameter_definition[i].type == SDDS_STRING) {
          string = malloc(sizeof(char) * (data_len[i + 1] + 1));
          memcpy((char *)string, (char *)par_data + count, data_len[i + 1]);
          string[data_len[i + 1]] = '\0';
          *(char **)SDDS_dataset->parameter[i] = string;
        } else
          memcpy((char *)(SDDS_dataset->parameter[i]), (char *)par_data + count, data_len[i + 1]);
        count += data_len[i + 1];
      }
    }
  }
  free(data_len);
  free(par_data);
  data_len = NULL;
  par_data = NULL;
  if (layout->n_arrays) {
    data_len = malloc(sizeof(*data_len) * layout->n_arrays);
    if (MPI_dataset->myid == 0) {
      for (i = 0; i < layout->n_arrays; i++)
        data_len[i] = layout->array_definition[i].dimensions;
    }
    MPI_Bcast(data_len, layout->n_arrays, MPI_INT64_T, 0, MPI_dataset->comm);
    for (i = 0; i < layout->n_arrays; i++) {
      type = layout->array_definition[i].type;
      size = SDDS_type_size[type - 1];
      if (data_len[i]) {
        if (type == SDDS_STRING) {
          if (MPI_dataset->myid == 0) {
            /* it is not easy to broad cast string array, will implement it in the future */
          }
        } else
          MPI_Bcast((char *)SDDS_dataset->array[i].data, data_len[i] * size, MPI_BYTE, 0, MPI_dataset->comm);
      }
    }
  }
  /*MPI_dataset->file_offset += SDDS_MPI_GetTitleOffset(MPI_dataset); */
  return 1;
}

/*flag master_read: 1 master processor will read rows; 0: master processor does not read rows.*/
/**
 * @brief Reads a binary page from an SDDS dataset using MPI parallel I/O.
 *
 * This function reads a page of data from a binary SDDS file in parallel using MPI. It handles reading the page title,
 * distributing the rows among MPI processes, and reading the column or row data depending on the dataset's data mode.
 * It also manages error handling and supports read recovery.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure where the page data will be stored.
 *
 * @return int32_t Returns the page number on success, 0 on failure, or -1 if end-of-file is reached.
 */
int32_t SDDS_MPI_ReadBinaryPage(SDDS_DATASET *SDDS_dataset) {
  int32_t mpi_code, type = 0, retval, master_read;
  int64_t i, j, n_rows, prev_rows;
  MPI_DATASET *MPI_dataset;
  SDDS_FILEBUFFER *fBuffer;
  MPI_Status status;
  MPI_Offset offset;
  long ID_offset;

  MPI_dataset = SDDS_dataset->MPI_dataset;
  master_read = MPI_dataset->master_read;

  if (SDDS_dataset->autoRecovered)
    return -1;
  if (SDDS_dataset->swapByteOrder) {
    return SDDS_MPI_ReadNonNativeBinaryPage(SDDS_dataset);
  }
  /*  static char s[SDDS_MAXLINE]; */
  n_rows = 0;
  SDDS_SetReadRecoveryMode(SDDS_dataset, 0);
  if (MPI_dataset->file_offset >= MPI_dataset->file_size)
    return (SDDS_dataset->page_number = -1);
  if ((mpi_code = MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL)) != MPI_SUCCESS) {
    SDDS_MPI_GOTO_ERROR(stderr, "Unable to set view for read binary page", mpi_code, 0);
    SDDS_SetError("Unable to set view for read binary page(1)");
    return 0;
  }
#if defined(MASTER_READTITLE_ONLY)
  if (MPI_dataset->myid == 0)
#endif
    retval = SDDS_MPI_BufferedReadBinaryTitle(SDDS_dataset);
#if defined(MASTER_READTITLE_ONLY)
  MPI_Bcast(&retval, 1, MPI_INT, 0, MPI_dataset->comm);
#endif
  /*end of file reached */
  if (retval < 0)
    return (SDDS_dataset->page_number = -1);
  if (retval == 0) {
    SDDS_SetError("Unable to read the SDDS title (row number, parameter and/or array) data");
    return 0;
  }
#if defined(MASTER_READTITLE_ONLY)
  SDDS_MPI_BroadcastTitleData(SDDS_dataset);
#endif
  MPI_dataset->file_offset += SDDS_MPI_GetTitleOffset(SDDS_dataset);
  if (MPI_dataset->total_rows < 0) {
    SDDS_SetError("Unable to read page--negative number of rows (SDDS_MPI_ReadBinaryPage)");
    return (0);
  }
  if (MPI_dataset->total_rows > SDDS_GetRowLimit()) {
    /* the number of rows is "unreasonably" large---treat like end-of-file */
    return (SDDS_dataset->page_number = -1);
  }
  prev_rows = 0;
  if (master_read) {
    n_rows = MPI_dataset->total_rows / MPI_dataset->n_processors;
    prev_rows = MPI_dataset->myid * n_rows;
    if (MPI_dataset->myid < (ID_offset = MPI_dataset->total_rows % (MPI_dataset->n_processors))) {
      n_rows++;
      prev_rows += MPI_dataset->myid;
    } else
      prev_rows += ID_offset;
  } else {
    if (MPI_dataset->myid == 0)
      n_rows = 0;
    else {
      n_rows = MPI_dataset->total_rows / (MPI_dataset->n_processors - 1);
      prev_rows = (MPI_dataset->myid - 1) * n_rows;
      if (MPI_dataset->myid <= (ID_offset = MPI_dataset->total_rows % (MPI_dataset->n_processors - 1))) {
        n_rows++;
        prev_rows += (MPI_dataset->myid - 1);
      } else
        prev_rows += ID_offset;
    }
  }
  MPI_dataset->start_row = prev_rows; /* This  number will be used as the paritlce ID offset */
  if (!SDDS_StartPage(SDDS_dataset, 0) || !SDDS_LengthenTable(SDDS_dataset, n_rows)) {
    SDDS_SetError("Unable to read page--couldn't start page (SDDS_MPI_ReadBinaryPage)");
    return (0);
  }
  offset = MPI_dataset->file_offset;
  fBuffer = &SDDS_dataset->fBuffer;

  if (SDDS_dataset->layout.data_mode.column_major) {
    /*read by column buffer is not need */
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
      type = SDDS_dataset->layout.column_definition[i].type;
      if (type == SDDS_STRING) {
        SDDS_SetError("Can not read string column from SDDS3 (SDDS_MPI_ReadBinaryPage");
        return 0;
      }
      MPI_dataset->file_offset = offset + (MPI_Offset)prev_rows * SDDS_type_size[type - 1];
      if ((mpi_code = MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL)) != MPI_SUCCESS) {
        SDDS_SetError("Unable to set view for read binary columns");
        return 0;
      }
      if (!MPI_dataset->collective_io) {
        if ((mpi_code = MPI_File_read(MPI_dataset->MPI_file, (char *)SDDS_dataset->data[i], n_rows * SDDS_type_size[type - 1], MPI_BYTE, &status)) != MPI_SUCCESS) {
          SDDS_SetError("Unable to set view for read binary columns");
          return 0;
        }
      } else {
        if ((mpi_code = MPI_File_read_all(MPI_dataset->MPI_file, (char *)SDDS_dataset->data[i], n_rows * SDDS_type_size[type - 1], MPI_BYTE, &status)) != MPI_SUCCESS) {
          SDDS_SetError("Unable to set view for read binary columns");
          return 0;
        }
      }
      offset += (MPI_Offset)MPI_dataset->total_rows * SDDS_type_size[type - 1];
    }
    MPI_dataset->n_rows = SDDS_dataset->n_rows = n_rows;
    MPI_dataset->file_offset = offset;
  } else {
    /* read row by row */
    if (!fBuffer->buffer) {
      fBuffer->bufferSize = defaultReadBufferSize;
      if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (fBuffer->bufferSize + 1)))) {
        SDDS_SetError("Unable to do buffered read--allocation failure");
        return 0;
      }
      fBuffer->bytesLeft = 0;
      fBuffer->data[0] = 0;
    }
    if (fBuffer->bytesLeft > 0) {
      /* discard the extra data for reading next page */
      fBuffer->data[0] = 0;
      fBuffer->bytesLeft = 0;
    }
    MPI_dataset->file_offset += (MPI_Offset)prev_rows * MPI_dataset->column_offset;
    if ((mpi_code = MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "Unable to set view for read binary rows", mpi_code, 0);
      SDDS_SetError("Unable to set view for read binary rows");
      return 0;
    }
    if (!master_read || !MPI_dataset->collective_io) {
      for (j = 0; j < n_rows; j++) {
        if (!SDDS_MPI_ReadBinaryRow(SDDS_dataset, j, 0)) {
          SDDS_dataset->n_rows = j;
          if (SDDS_dataset->autoRecover) {
#if defined(DEBUG)
            fprintf(stderr, "Doing auto-read recovery\n");
#endif
            SDDS_dataset->autoRecovered = 1;
            SDDS_ClearErrors();
            return (SDDS_dataset->page_number = MPI_dataset->n_page);
          }
          SDDS_SetError("Unable to read page--error reading data row (SDDS_MPI_ReadBinaryPage)");
          SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
          return (0);
        }
      }
      MPI_dataset->n_rows = SDDS_dataset->n_rows = j;
    } else {
      MPI_dataset->n_rows = SDDS_dataset->n_rows = n_rows;
      if (!SDDS_MPI_CollectiveReadByRow(SDDS_dataset))
        return 0;
    }
    MPI_dataset->file_offset = offset + MPI_dataset->total_rows * MPI_dataset->column_offset;
  }
  MPI_dataset->n_page++;
  return (SDDS_dataset->page_number = MPI_dataset->n_page);
}

/**
 * @brief Reads a non-native binary page from an SDDS dataset using MPI parallel I/O.
 *
 * This function is a wrapper that calls SDDS_MPI_ReadNonNativePageSparse with mode set to 0,
 * facilitating the reading of non-native binary pages.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure where the page data will be stored.
 *
 * @return int32_t Returns the page number on success, 0 on failure, or -1 if end-of-file is reached.
 */
int32_t SDDS_MPI_ReadNonNativePage(SDDS_DATASET *SDDS_dataset) {
  return SDDS_MPI_ReadNonNativePageSparse(SDDS_dataset, 0);
}

/**
 * @brief Reads a sparse non-native binary page from an SDDS dataset using MPI parallel I/O.
 *
 * This function reads a page of data from a non-native binary SDDS file in parallel using MPI. The `mode` parameter allows for future expansion.
 * It handles reading the title data, broadcasting it to all MPI processes, and reading the column or row data based on the dataset's data mode.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure where the page data will be stored.
 * @param[in] mode Mode flag to support future expansion (currently unused).
 *
 * @return int32_t Returns the page number on success, 0 on failure, or -1 if end-of-file is reached.
 */
int32_t SDDS_MPI_ReadNonNativePageSparse(SDDS_DATASET *SDDS_dataset, uint32_t mode)
/* the mode argument is to support future expansion */
{
  int32_t retval;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_ReadNonNativePageSparse"))
    return (0);
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't read page--file is disconnected (SDDS_MPI_ReadNonNativePageSparse)");
    return 0;
  }

  if (SDDS_dataset->original_layout.data_mode.mode == SDDS_ASCII) {
    SDDS_SetError("Can not read assii file with parallel io.");
    return 0;
  } else if (SDDS_dataset->original_layout.data_mode.mode == SDDS_BINARY) {
    if ((retval = SDDS_MPI_ReadNonNativeBinaryPage(SDDS_dataset)) < 1) {
      return (retval);
    }
  } else {
    SDDS_SetError("Unable to read page--unrecognized data mode (SDDS_MPI_ReadNonNativePageSparse)");
    return (0);
  }
  return (retval);
}

/**
 * @brief Reads a non-native binary page from an SDDS dataset using MPI parallel I/O.
 *
 * This function reads a page of data from a non-native binary SDDS file in parallel using MPI. It manages reading the title data,
 * distributing the rows among MPI processes, and reading the column or row data depending on the dataset's data mode.
 * It also handles byte swapping for endianness and error management.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure where the page data will be stored.
 *
 * @return int32_t Returns the page number on success, 0 on failure, or -1 if end-of-file is reached.
 */
int32_t SDDS_MPI_ReadNonNativeBinaryPage(SDDS_DATASET *SDDS_dataset) {
  int32_t ID_offset, mpi_code, master_read, type, retval;
  int64_t i, j, n_rows, total_rows, prev_rows;
  SDDS_FILEBUFFER *fBuffer;
  MPI_DATASET *MPI_dataset;
  MPI_Offset offset;
  MPI_Status status;

  MPI_dataset = SDDS_dataset->MPI_dataset;
  master_read = MPI_dataset->master_read;
  /*  static char s[SDDS_MAXLINE]; */
  n_rows = 0;
  SDDS_SetReadRecoveryMode(SDDS_dataset, 0);
  if (MPI_dataset->file_offset >= MPI_dataset->file_size)
    return (SDDS_dataset->page_number = -1);

  if ((mpi_code = MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL)) != MPI_SUCCESS) {
    SDDS_MPI_GOTO_ERROR(stderr, "Unable to set view for read binary page", mpi_code, 0);
    SDDS_SetError("Unable to set view for read binary page(1)");
    return 0;
  }
  /* read the number of rows in current page */
#if defined(MASTER_READTITLE_ONLY)
  if (MPI_dataset->myid == 0)
#endif
    retval = SDDS_MPI_BufferedReadNonNativeBinaryTitle(SDDS_dataset);
#if defined(MASTER_READTITLE_ONLY)
  MPI_Bcast(&retval, 1, MPI_INT, 0, MPI_dataset->comm);
#endif
  /*end of file reached */
  if (retval < 0)
    return (SDDS_dataset->page_number = -1);
  if (retval == 0) {
    SDDS_SetError("Unable to read the SDDS title (row number, parameter and/or array) data");
    return 0;
  }
#if defined(MASTER_READTITLE_ONLY)
  SDDS_MPI_BroadcastTitleData(SDDS_dataset);
#endif
  MPI_dataset->file_offset += SDDS_MPI_GetTitleOffset(SDDS_dataset);
  if (MPI_dataset->total_rows < 0) {
    SDDS_SetError("Unable to read page--negative number of rows (SDDS_MPI_ReadBinaryPage)");
    return (0);
  }
  if (MPI_dataset->total_rows > SDDS_GetRowLimit()) {
    /* the number of rows is "unreasonably" large---treat like end-of-file */
    return (SDDS_dataset->page_number = -1);
  }
  total_rows = MPI_dataset->total_rows;
  prev_rows = 0;
  if (master_read) {
    n_rows = total_rows / MPI_dataset->n_processors;
    prev_rows = MPI_dataset->myid * n_rows;
    if (MPI_dataset->myid < (ID_offset = total_rows % (MPI_dataset->n_processors))) {
      n_rows++;
      prev_rows += MPI_dataset->myid;
    } else
      prev_rows += ID_offset;
  } else {
    if (MPI_dataset->myid == 0)
      n_rows = 0;
    else {
      n_rows = total_rows / (MPI_dataset->n_processors - 1);
      prev_rows = (MPI_dataset->myid - 1) * n_rows;
      if (MPI_dataset->myid <= (ID_offset = total_rows % (MPI_dataset->n_processors - 1))) {
        n_rows++;
        prev_rows += (MPI_dataset->myid - 1);
      } else
        prev_rows += ID_offset;
    }
  }
  MPI_dataset->start_row = prev_rows; /* This  number will be used as the paritlce ID offset */
  if (!SDDS_StartPage(SDDS_dataset, 0) || !SDDS_LengthenTable(SDDS_dataset, n_rows)) {
    SDDS_SetError("Unable to read page--couldn't start page (SDDS_MPI_ReadNonNativeBinaryPage)");
    return (0);
  }

  offset = MPI_dataset->file_offset;
  fBuffer = &SDDS_dataset->fBuffer;
  if (SDDS_dataset->layout.data_mode.column_major) {
    /*read by column buffer is not need */
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
      type = SDDS_dataset->layout.column_definition[i].type;
      if (type == SDDS_STRING) {
        SDDS_SetError("Can not read string column from SDDS3 (SDDS_MPI_ReadBinaryPage");
        return 0;
      }
      MPI_dataset->file_offset = offset + (MPI_Offset)prev_rows * SDDS_type_size[type - 1];
      if ((mpi_code = MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL)) != MPI_SUCCESS) {
        SDDS_SetError("Unable to set view for read binary columns");
        return 0;
      }
      if (!MPI_dataset->collective_io) {
        if ((mpi_code = MPI_File_read(MPI_dataset->MPI_file, (char *)SDDS_dataset->data[i], n_rows * SDDS_type_size[type - 1], MPI_BYTE, &status)) != MPI_SUCCESS) {
          SDDS_SetError("Unable to set view for read binary columns");
          return 0;
        }
      } else {
        if ((mpi_code = MPI_File_read_all(MPI_dataset->MPI_file, (char *)SDDS_dataset->data[i], n_rows * SDDS_type_size[type - 1], MPI_BYTE, &status)) != MPI_SUCCESS) {
          SDDS_SetError("Unable to set view for read binary columns");
          return 0;
        }
      }
      offset += (MPI_Offset)MPI_dataset->total_rows * SDDS_type_size[type - 1];
    }
    MPI_dataset->n_rows = SDDS_dataset->n_rows = n_rows;
    MPI_dataset->file_offset = offset;
  } else {
    /* read row by row */
    if (!fBuffer->buffer) {
      fBuffer->bufferSize = defaultReadBufferSize;
      if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (fBuffer->bufferSize + 1)))) {
        SDDS_SetError("Unable to do buffered read--allocation failure");
        return 0;
      }
      fBuffer->bytesLeft = 0;
      fBuffer->data[0] = 0;
    }
    if (fBuffer->bytesLeft > 0) {
      /* discard the extra data for reading next page */
      fBuffer->data[0] = 0;
      fBuffer->bytesLeft = 0;
    }
    MPI_dataset->file_offset += (MPI_Offset)prev_rows * MPI_dataset->column_offset;
    if ((mpi_code = MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "Unable to set view for read binary rows", mpi_code, 0);
      SDDS_SetError("Unable to set view for read binary rows");
      return 0;
    }
    if (!MPI_dataset->collective_io || !master_read) {
      for (j = 0; j < n_rows; j++) {
        if (!SDDS_MPI_ReadBinaryRow(SDDS_dataset, j, 0)) {
          SDDS_dataset->n_rows = j - 1;
          if (SDDS_dataset->autoRecover) {
            SDDS_ClearErrors();
            SDDS_SwapEndsColumnData(SDDS_dataset);
            return (SDDS_dataset->page_number = MPI_dataset->n_page);
          }
          SDDS_SetError("Unable to read page--error reading data row (SDDS_MPI_ReadNonNativeBinaryPage)");
          SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
          return (0);
        }
      }
      SDDS_dataset->n_rows = j;
    } else {
      MPI_dataset->n_rows = SDDS_dataset->n_rows = n_rows;
      if (!SDDS_MPI_CollectiveReadByRow(SDDS_dataset))
        return 0;
    }
    MPI_dataset->file_offset = offset + MPI_dataset->total_rows * MPI_dataset->column_offset;
  }
  SDDS_SwapEndsColumnData(SDDS_dataset);
  MPI_dataset->n_page++;
  MPI_Barrier(MPI_dataset->comm);
  return (SDDS_dataset->page_number = MPI_dataset->n_page);
}

/**
 * @brief Buffers and reads the non-native binary title data from an SDDS dataset using MPI.
 *
 * This function initializes and manages a buffered read for the title section of a non-native binary SDDS dataset.
 * It reads the total number of rows, parameters, and arrays from the binary file buffer. The function handles
 * memory allocation for the buffer, byte swapping for endianness, and initializes the SDDS page structure.
 *
 * @param[in,out] SDDS_dataset Pointer to the `SDDS_DATASET` structure where the title data will be stored.
 *
 * @return int32_t Returns `1` on successful read, `0` on failure, and `-1` if the end of the file is reached.
 */
int32_t SDDS_MPI_BufferedReadNonNativeBinaryTitle(SDDS_DATASET *SDDS_dataset) {
  SDDS_FILEBUFFER *fBuffer = NULL;
  MPI_DATASET *MPI_dataset = NULL;
  int32_t ret_val;
  int32_t total_rows;

  MPI_dataset = SDDS_dataset->MPI_dataset;
  fBuffer = &(SDDS_dataset->titleBuffer);
  if (!fBuffer->buffer) {
    fBuffer->bufferSize = defaultTitleBufferSize;
    if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (fBuffer->bufferSize + 1)))) {
      SDDS_SetError("Unable to do buffered read--allocation failure(SDDS_MPI_ReadNonNativeBinaryTitle)");
      return 0;
    }
    fBuffer->bytesLeft = 0;
  }
  if (fBuffer->bytesLeft > 0) {
    /* discard the extra data for reading next page */
    fBuffer->data[0] = 0;
    fBuffer->bytesLeft = 0;
  }
  if ((ret_val = SDDS_MPI_BufferedRead((void *)&(total_rows), sizeof(int32_t), SDDS_dataset, fBuffer)) < 0)
    return -1;
  SDDS_SwapLong(&(total_rows));
  if (total_rows == INT32_MIN) {
    if ((ret_val = SDDS_MPI_BufferedRead((void *)&(MPI_dataset->total_rows), sizeof(int64_t), SDDS_dataset, fBuffer)) < 0)
      return -1;
  } else {
    MPI_dataset->total_rows = total_rows;
  }
  if (!ret_val)
    return 0;
  if (!SDDS_StartPage(SDDS_dataset, 0)) {
    SDDS_SetError("Unable to read page--couldn't start page (SDDS_MPI_BufferedReadNonNativeBinaryTitle)");
    return (0);
  }
  /*read parameters */
  if (!SDDS_MPI_ReadNonNativeBinaryParameters(SDDS_dataset, fBuffer)) {
    SDDS_SetError("Unable to read page--parameter reading error (SDDS_MPI_BufferedNonNativeReadTitle)");
    return (0);
  }
  /*read arrays */
  if (!SDDS_MPI_ReadNonNativeBinaryArrays(SDDS_dataset, fBuffer)) {
    SDDS_SetError("Unable to read page--array reading error (SDDS_MPI_BufferedNonNativeReadTitle)");
    return (0);
  }

  return 1;
}

/*obtain the offset of n_rows, parameters and arrays which are written by master processor */
/**
 * @brief Calculates the byte offset for the title section in a non-native binary SDDS dataset.
 *
 * This function computes the total byte offset required to read the number of rows, parameters, and arrays
 * defined in the SDDS dataset. It accounts for different data types, including strings, and variable-length
 * parameters and arrays. The calculated offset is used to position the MPI file view correctly for reading.
 *
 * @param[in] SDDS_dataset Pointer to the `SDDS_DATASET` structure containing the dataset layout and data.
 *
 * @return MPI_Offset The total byte offset of the title section in the binary file.
 */
MPI_Offset SDDS_MPI_GetTitleOffset(SDDS_DATASET *SDDS_dataset) {
  int64_t i, j;
  MPI_Offset offset = 0;
  SDDS_LAYOUT *layout;

  layout = &(SDDS_dataset->layout);
  offset += sizeof(int32_t);
  if (SDDS_dataset->n_rows > INT32_MAX) {
    offset += sizeof(int64_t);
  }
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].fixed_value)
      continue;
    if (layout->parameter_definition[i].type == SDDS_STRING) {
      if (*(char **)SDDS_dataset->parameter[i])
        offset += sizeof(int32_t) + sizeof(char) * strlen(*(char **)SDDS_dataset->parameter[i]);
      else
        offset += sizeof(int32_t); /*write length 0 for NULL string */
    } else {
      offset += SDDS_type_size[layout->parameter_definition[i].type - 1];
    }
  }
  for (i = 0; i < layout->n_arrays; i++) {
    if (!(SDDS_dataset->array[i].dimension)) {
      offset += layout->array_definition[i].dimensions * sizeof(int32_t);
      continue;
    }
    offset += sizeof(*(SDDS_dataset->array)[i].dimension) * layout->array_definition[i].dimensions;
    if (layout->array_definition[i].type == SDDS_STRING) {
      for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
        if (((char **)SDDS_dataset->array[i].data)[j])
          offset += sizeof(int32_t) + sizeof(char) * strlen(((char **)SDDS_dataset->array[i].data)[j]);
        else
          offset += sizeof(int32_t);
      }
    } else {
      offset += SDDS_type_size[layout->array_definition[i].type - 1] * SDDS_dataset->array[i].elements;
    }
  }
  return offset;
}

/*use a small size of buffer to read the total_rows, parameters and arrays of each page */
/**
 * @brief Buffers and reads the binary title data from an SDDS dataset using MPI.
 *
 * This function initializes and manages a buffered read for the title section of a native binary SDDS dataset.
 * It reads the total number of rows, parameters, and arrays from the binary file buffer. The function handles
 * memory allocation for the buffer and initializes the SDDS page structure.
 *
 * @param[in,out] SDDS_dataset Pointer to the `SDDS_DATASET` structure where the title data will be stored.
 *
 * @return int32_t Returns `1` on successful read, `0` on failure, and `-1` if the end of the file is reached.
 */
int32_t SDDS_MPI_BufferedReadBinaryTitle(SDDS_DATASET *SDDS_dataset) {
  SDDS_FILEBUFFER *fBuffer = NULL;
  MPI_DATASET *MPI_dataset = NULL;
  int32_t ret_val;
  int32_t total_rows;

  MPI_dataset = SDDS_dataset->MPI_dataset;
  fBuffer = &(SDDS_dataset->titleBuffer);
  if (!fBuffer->buffer) {
    fBuffer->bufferSize = defaultTitleBufferSize;
    if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (fBuffer->bufferSize + 1)))) {
      SDDS_SetError("Unable to do buffered read--allocation failure(SDDS_MPI_ReadBinaryTitle)");
      return 0;
    }
    fBuffer->bytesLeft = 0;
  }
  if (fBuffer->bytesLeft > 0) {
    /* discard the extra data for reading next page */
    fBuffer->data[0] = 0;
    fBuffer->bytesLeft = 0;
  }
  if ((ret_val = SDDS_MPI_BufferedRead((void *)&(total_rows), sizeof(int32_t), SDDS_dataset, fBuffer)) < 0)
    return -1;
  if (total_rows == INT32_MIN) {
    if ((ret_val = SDDS_MPI_BufferedRead((void *)&(MPI_dataset->total_rows), sizeof(int64_t), SDDS_dataset, fBuffer)) < 0)
      return -1;
  } else {
    MPI_dataset->total_rows = total_rows;
  }
  if (!ret_val)
    return 0;
  if (!SDDS_StartPage(SDDS_dataset, 0)) {
    SDDS_SetError("Unable to read page--couldn't start page (SDDS_MPI_BufferedReadBinaryTitle)");
    return (0);
  }
  /*read parameters */
  if (!SDDS_MPI_ReadBinaryParameters(SDDS_dataset, fBuffer)) {
    SDDS_SetError("Unable to read page--parameter reading error (SDDS_MPI_BufferedReadTitle)");
    return (0);
  }
  /*read arrays */
  if (!SDDS_MPI_ReadBinaryArrays(SDDS_dataset, fBuffer)) {
    SDDS_SetError("Unable to read page--array reading error (SDDS_MPI_BufferedReadTitle)");
    return (0);
  }

  return 1;
}

/**
 * @brief Writes SDDS dataset rows collectively by row using MPI parallel I/O.
 *
 * This function performs a collective write operation where each MPI process writes its assigned rows of the SDDS dataset.
 * It handles buffering of row data, ensures synchronization among MPI processes, and manages error handling.
 * The function supports only binary string types for non-string data and flushes the buffer upon completion.
 *
 * @param[in,out] SDDS_dataset Pointer to the `SDDS_DATASET` structure containing the data to be written.
 *
 * @return int32_t Returns `1` on successful write, `0` on failure.
 */
int32_t SDDS_MPI_CollectiveWriteByRow(SDDS_DATASET *SDDS_dataset) {
  MPI_DATASET *MPI_dataset;
  SDDS_LAYOUT *layout;
  int32_t mpi_code, type, size;
  int64_t i, j, n_rows, min_rows, writeBytes;
  SDDS_FILEBUFFER *fBuffer;

#if MPI_DEBUG
  logDebug("SDDS_MPI_CollectiveWriteByRow", SDDS_dataset);
#endif

  MPI_dataset = SDDS_dataset->MPI_dataset;
  layout = &(SDDS_dataset->layout);
  fBuffer = &(SDDS_dataset->fBuffer);
  n_rows = SDDS_dataset->n_rows;
  MPI_Allreduce(&n_rows, &min_rows, 1, MPI_INT64_T, MPI_MIN, MPI_dataset->comm);
  for (i = 0; i < min_rows; i++) {
    for (j = 0; j < layout->n_columns; j++) {
      type = layout->column_definition[j].type;
      size = SDDS_type_size[type - 1];
      if (type == SDDS_STRING) {
        SDDS_SetError("Can not write binary string in collective io.");
        return 0;
      }
      if (!SDDS_MPI_BufferedWriteAll((char *)SDDS_dataset->data[j] + i * size, size, SDDS_dataset))
        return 0;
    }
  }

  if ((writeBytes = fBuffer->bufferSize - fBuffer->bytesLeft)) {
    if (writeBytes < 0) {
      SDDS_SetError("Unable to flush buffer: negative byte count (SDDS_FlushBuffer).");
      return 0;
    }
    if ((mpi_code = MPI_File_write_all(MPI_dataset->MPI_file, fBuffer->buffer, writeBytes, MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_FlushBuffer(MPI_File_write_at failed)", mpi_code, 0);
      return 0;
    }
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
  }

  for (i = min_rows; i < n_rows; i++)
    if (!SDDS_MPI_WriteBinaryRow(SDDS_dataset, i))
      return 0;
  if (!SDDS_MPI_FlushBuffer(SDDS_dataset))
    return 0;
  return 1;
}

/**
 * @brief Writes non-native binary SDDS dataset rows collectively by row using MPI parallel I/O.
 *
 * This function performs a collective write operation for non-native binary SDDS datasets, where each MPI process writes
 * its assigned rows. It handles buffering of row data, ensures synchronization among MPI processes, and manages error handling.
 * The function supports only binary string types for non-string data and flushes the buffer upon completion.
 *
 * @param[in,out] SDDS_dataset Pointer to the `SDDS_DATASET` structure containing the data to be written.
 *
 * @return int32_t Returns `1` on successful write, `0` on failure.
 */
int32_t SDDS_MPI_CollectiveWriteNonNativeByRow(SDDS_DATASET *SDDS_dataset) {
  MPI_DATASET *MPI_dataset;
  SDDS_LAYOUT *layout;
  int32_t mpi_code, type, size;
  int64_t i, j, n_rows, min_rows, writeBytes;
  SDDS_FILEBUFFER *fBuffer;

#if MPI_DEBUG
  logDebug("SDDS_MPI_CollectiveWriteNonNativeByRow", SDDS_dataset);
#endif

  MPI_dataset = SDDS_dataset->MPI_dataset;
  layout = &(SDDS_dataset->layout);
  fBuffer = &(SDDS_dataset->fBuffer);
  n_rows = SDDS_dataset->n_rows;
  MPI_Allreduce(&n_rows, &min_rows, 1, MPI_INT64_T, MPI_MIN, MPI_dataset->comm);
  for (i = 0; i < min_rows; i++) {
    for (j = 0; j < layout->n_columns; j++) {
      type = layout->column_definition[j].type;
      size = SDDS_type_size[type - 1];
      if (type == SDDS_STRING) {
        SDDS_SetError("Can not write binary string in collective io.");
        return 0;
      }
      if (!SDDS_MPI_BufferedWriteAll((char *)SDDS_dataset->data[j] + i * size, size, SDDS_dataset))
        return 0;
    }
  }

  if ((writeBytes = fBuffer->bufferSize - fBuffer->bytesLeft)) {
    if (writeBytes < 0) {
      SDDS_SetError("Unable to flush buffer: negative byte count (SDDS_FlushBuffer).");
      return 0;
    }
    if ((mpi_code = MPI_File_write_all(MPI_dataset->MPI_file, fBuffer->buffer, writeBytes, MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_FlushBuffer(MPI_File_write_at failed)", mpi_code, 0);
      return 0;
    }
    fBuffer->bytesLeft = fBuffer->bufferSize;
    fBuffer->data = fBuffer->buffer;
  }

  for (i = min_rows; i < n_rows; i++)
    if (!SDDS_MPI_WriteNonNativeBinaryRow(SDDS_dataset, i))
      return 0;
  if (!SDDS_MPI_FlushBuffer(SDDS_dataset))
    return 0;
  return 1;
}

/**
 * @brief Reads SDDS dataset rows collectively by row using MPI parallel I/O.
 *
 * This function performs a collective read operation where each MPI process reads its assigned rows of the SDDS dataset.
 * It handles buffering of row data, ensures synchronization among MPI processes, and manages error handling.
 * The function supports only binary string types for non-string data and flushes the buffer upon completion.
 *
 * @param[in,out] SDDS_dataset Pointer to the `SDDS_DATASET` structure where the data will be stored.
 *
 * @return int32_t Returns `1` on successful read, `0` on failure.
 */
int32_t SDDS_MPI_CollectiveReadByRow(SDDS_DATASET *SDDS_dataset) {

  SDDS_FILEBUFFER *fBuffer;
  MPI_DATASET *MPI_dataset;
  int32_t type, size;
  int64_t min_rows, i, j;
  SDDS_LAYOUT *layout;

  MPI_dataset = SDDS_dataset->MPI_dataset;
  fBuffer = &(SDDS_dataset->fBuffer);
  layout = &(SDDS_dataset->layout);

  if (!MPI_dataset->master_read) {
    SDDS_SetError("Cannot read row with collective io when master is not reading the data.");
    return 0;
  }
  min_rows = MPI_dataset->total_rows / MPI_dataset->n_processors;
  for (i = 0; i < min_rows; i++) {
    for (j = 0; j < layout->n_columns; j++) {
      type = layout->column_definition[j].type;
      size = SDDS_type_size[type - 1];
      if (type == SDDS_STRING) {
        SDDS_SetError("Can not write binary string in collective io.");
        return 0;
      }
      if (!SDDS_MPI_BufferedReadAll((char *)SDDS_dataset->data[j] + i * size, size, SDDS_dataset, fBuffer))
        return 0;
    }
  }
  for (i = min_rows; i < MPI_dataset->n_rows; i++)
    if (!SDDS_MPI_ReadBinaryRow(SDDS_dataset, i, 0))
      return 0;

  return 1;
}
