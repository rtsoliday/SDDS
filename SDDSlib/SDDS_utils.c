/**
 * @file SDDS_utils.c
 * @brief This file provides miscellaneous functions for interacting with SDDS objects.
 *
 * This file provides miscellaneous functions for interacting with SDDS objects.
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
#include <ctype.h>
#if !defined(_WIN32)
#  include <unistd.h>
#endif
#if defined(__APPLE__)
/*lockf is not defined by unistd.h like it should be*/
int lockf(int filedes, int function, off_t size);
#endif

/**
 * @brief Prints a data value of a specified type using an optional printf format string.
 *
 * This function prints a single data value from a data array based on the specified type and index. It supports various data types defined by SDDS constants and allows customization of the output format.
 *
 * @param[in]  data    Pointer to the base address of the data array to be printed.
 * @param[in]  index   The index of the item within the data array to be printed.
 * @param[in]  type    The data type of the value to be printed, specified by one of the SDDS constants:
 *                     - `SDDS_LONGDOUBLE`
 *                     - `SDDS_DOUBLE`
 *                     - `SDDS_FLOAT`
 *                     - `SDDS_LONG`
 *                     - `SDDS_ULONG`
 *                     - `SDDS_SHORT`
 *                     - `SDDS_USHORT`
 *                     - `SDDS_CHARACTER`
 *                     - `SDDS_STRING`
 * @param[in]  format  (Optional) NULL-terminated string specifying a `printf` format. If `NULL`, a default format is used based on the data type.
 * @param[in]  fp      Pointer to the `FILE` stream where the data will be printed.
 * @param[in]  mode    Flags controlling the printing behavior. Valid values are:
 *                     - `0`: Default behavior.
 *                     - `SDDS_PRINT_NOQUOTES`: When printing strings, do not enclose them in quotes.
 *
 * @return Returns `1` on success. On failure, returns `0` and records an error message.
 *
 * @note This function assumes that the `data` pointer points to an array of the specified `type`, and `index` is within the bounds of this array.
 *
 * @see SDDS_SetError
 */
int32_t SDDS_PrintTypedValue(void *data, int64_t index, int32_t type, char *format, FILE *fp, uint32_t mode) {
  char buffer[SDDS_PRINT_BUFLEN], *s;

  if (!data) {
    SDDS_SetError("Unable to print value--data pointer is NULL (SDDS_PrintTypedValue)");
    return (0);
  }
  if (!fp) {
    SDDS_SetError("Unable to print value--file pointer is NULL (SDDS_PrintTypedValue)");
    return (0);
  }
  switch (type) {
  case SDDS_SHORT:
    fprintf(fp, format ? format : "%hd", *((short *)data + index));
    break;
  case SDDS_USHORT:
    fprintf(fp, format ? format : "%hu", *((unsigned short *)data + index));
    break;
  case SDDS_LONG:
    fprintf(fp, format ? format : "%" PRId32, *((int32_t *)data + index));
    break;
  case SDDS_ULONG:
    fprintf(fp, format ? format : "%" PRIu32, *((uint32_t *)data + index));
    break;
  case SDDS_LONG64:
    fprintf(fp, format ? format : "%" PRId64, *((int64_t *)data + index));
    break;
  case SDDS_ULONG64:
    fprintf(fp, format ? format : "%" PRIu64, *((uint64_t *)data + index));
    break;
  case SDDS_FLOAT:
    fprintf(fp, format ? format : "%15.8e", *((float *)data + index));
    break;
  case SDDS_DOUBLE:
    fprintf(fp, format ? format : "%21.15e", *((double *)data + index));
    break;
  case SDDS_LONGDOUBLE:
    if (LDBL_DIG == 18) {
      fprintf(fp, format ? format : "%21.18Le", *((long double *)data + index));
    } else {
      fprintf(fp, format ? format : "%21.15Le", *((long double *)data + index));
    }
    break;
  case SDDS_STRING:
    s = *((char **)data + index);
    if ((int32_t)strlen(s) > SDDS_PRINT_BUFLEN - 3) {
      SDDS_SetError("Buffer size overflow (SDDS_PrintTypedValue)");
      return 0;
    }
    SDDS_SprintTypedValue(data, index, type, format, buffer, mode);
    fputs(buffer, fp);
    break;
  case SDDS_CHARACTER:
    fprintf(fp, format ? format : "%c", *((char *)data + index));
    break;
  default:
    SDDS_SetError("Unable to print value--unknown data type (SDDS_PrintTypedValue)");
    return (0);
  }
  return (1);
}

/**
 * @brief Formats a data value of a specified type into a string buffer using an optional printf format string.
 *
 * This function formats a single data value from a data array into a provided buffer. It is a wrapper for `SDDS_SprintTypedValueFactor` with a default scaling factor of 1.0.
 *
 * @param[in]  data    Pointer to the base address of the data array containing the value to be formatted.
 * @param[in]  index   The index of the item within the data array to be formatted.
 * @param[in]  type    The data type of the value, specified by one of the SDDS constants:
 *                     - `SDDS_LONGDOUBLE`
 *                     - `SDDS_DOUBLE`
 *                     - `SDDS_FLOAT`
 *                     - `SDDS_LONG`
 *                     - `SDDS_ULONG`
 *                     - `SDDS_SHORT`
 *                     - `SDDS_USHORT`
 *                     - `SDDS_CHARACTER`
 *                     - `SDDS_STRING`
 * @param[in]  format  (Optional) NULL-terminated string specifying a `printf` format. If `NULL`, a default format is used based on the data type.
 * @param[out] buffer  Pointer to a character array where the formatted string will be stored.
 * @param[in]  mode    Flags controlling the formatting behavior. Valid values are:
 *                     - `0`: Default behavior.
 *                     - `SDDS_PRINT_NOQUOTES`: When formatting strings, do not enclose them in quotes.
 *
 * @return Returns `1` on success. On failure, returns `0` and records an error message.
 *
 * @note This function uses a default scaling factor of 1.0.
 *
 * @see SDDS_SprintTypedValueFactor
 * @see SDDS_SetError
 */
int32_t SDDS_SprintTypedValue(void *data, int64_t index, int32_t type, const char *format, char *buffer, uint32_t mode) {
  return SDDS_SprintTypedValueFactor(data, index, type, format, buffer, mode, 1.0);
}

/**
 * @brief Reallocates memory to a new size and zero-initializes the additional space.
 *
 * This function extends the standard `realloc` functionality by zero-initializing any newly allocated memory beyond the original size. It ensures that memory is consistently reallocated and initialized across different build configurations.
 *
 * @param[in]  data    Pointer to the base address of the data array containing the value to be formatted.
 * @param[in]  index   The index of the item within the data array to be formatted.
 * @param[in]  type    The data type of the value, specified by one of the SDDS constants:
 *                     - `SDDS_LONGDOUBLE`
 *                     - `SDDS_DOUBLE`
 *                     - `SDDS_FLOAT`
 *                     - `SDDS_LONG`
 *                     - `SDDS_ULONG`
 *                     - `SDDS_SHORT`
 *                     - `SDDS_USHORT`
 *                     - `SDDS_CHARACTER`
 *                     - `SDDS_STRING`
 * @param[in]  format  (Optional) NULL-terminated string specifying a `printf` format. If `NULL`, a default format is used based on the data type.
 * @param[out] buffer  Pointer to a character array where the formatted string will be stored.
 * @param[in]  mode    Flags controlling the formatting behavior. Valid values are:
 *                     - `0`: Default behavior.
 *                     - `SDDS_PRINT_NOQUOTES`: When formatting strings, do not enclose them in quotes.
 * @param[in]  factor  Scaling factor to be applied to the value before formatting. The value is multiplied by this factor.
 *
 * @return Returns `1` on success. On failure, returns `0` and records an error message.
 *
 * @note This function handles string types by optionally enclosing them in quotes, unless `SDDS_PRINT_NOQUOTES` is specified in `mode`.
 *
 * @see SDDS_SprintTypedValue
 * @see SDDS_SetError
 */
int32_t SDDS_SprintTypedValueFactor(void *data, int64_t index, int32_t type, const char *format, char *buffer, uint32_t mode, double factor) {
  char buffer2[SDDS_PRINT_BUFLEN], *s;
  short printed;

  if (!data) {
    SDDS_SetError("Unable to print value--data pointer is NULL (SDDS_SprintTypedValueFactor)");
    return (0);
  }
  if (!buffer) {
    SDDS_SetError("Unable to print value--buffer pointer is NULL (SDDS_SprintTypedValueFactor)");
    return (0);
  }
  switch (type) {
  case SDDS_SHORT:
    sprintf(buffer, format ? format : "%hd", (short)(*((short *)data + index) * (factor)));
    break;
  case SDDS_USHORT:
    sprintf(buffer, format ? format : "%hu", (unsigned short)(*((unsigned short *)data + index) * (factor)));
    break;
  case SDDS_LONG:
    sprintf(buffer, format ? format : "%" PRId32, (int32_t)(*((int32_t *)data + index) * (factor)));
    break;
  case SDDS_ULONG:
    sprintf(buffer, format ? format : "%" PRIu32, (uint32_t)(*((uint32_t *)data + index) * (factor)));
    break;
  case SDDS_LONG64:
    sprintf(buffer, format ? format : "%" PRId64, (int64_t)(*((int64_t *)data + index) * (factor)));
    break;
  case SDDS_ULONG64:
    sprintf(buffer, format ? format : "%" PRIu64, (uint64_t)(*((uint64_t *)data + index) * (factor)));
    break;
  case SDDS_FLOAT:
    sprintf(buffer, format ? format : "%15.8e", (float)(*((float *)data + index) * (factor)));
    break;
  case SDDS_DOUBLE:
    sprintf(buffer, format ? format : "%21.15e", (double)(*((double *)data + index) * (factor)));
    break;
  case SDDS_LONGDOUBLE:
    if (LDBL_DIG == 18) {
      sprintf(buffer, format ? format : "%21.18Le", (long double)(*((long double *)data + index) * (factor)));
    } else {
      sprintf(buffer, format ? format : "%21.15Le", (long double)(*((long double *)data + index) * (factor)));
    }
    break;
  case SDDS_STRING:
    s = *((char **)data + index);
    if ((int32_t)strlen(s) > SDDS_PRINT_BUFLEN - 3) {
      SDDS_SetError("Buffer size overflow (SDDS_SprintTypedValue)");
      return (0);
    }
    if (!(mode & SDDS_PRINT_NOQUOTES)) {
      printed = 0;
      if (!s || SDDS_StringIsBlank(s))
        sprintf(buffer, "\"\"");
      else if (strchr(s, '"')) {
        strcpy(buffer2, s);
        SDDS_EscapeQuotes(buffer2, '"');
        if (SDDS_HasWhitespace(buffer2))
          sprintf(buffer, "\"%s\"", buffer2);
        else
          strcpy(buffer, buffer2);
      } else if (SDDS_HasWhitespace(s))
        sprintf(buffer, "\"%s\"", s);
      else {
        sprintf(buffer, format ? format : "%s", s);
        printed = 1;
      }
      if (!printed) {
        sprintf(buffer2, format ? format : "%s", buffer);
        strcpy(buffer, buffer2);
      }
    } else {
      sprintf(buffer, format ? format : "%s", s);
    }
    break;
  case SDDS_CHARACTER:
    sprintf(buffer, format ? format : "%c", *((char *)data + index));
    break;
  default:
    SDDS_SetError("Unable to print value--unknown data type (SDDS_SprintTypedValue)");
    return (0);
  }
  return (1);
}

static int32_t n_errors = 0;
static int32_t n_errors_max = 0;
static char **error_description = NULL;
static char *registeredProgramName = NULL;

/**
 * @brief Registers the executable program name for use in error messages.
 *
 * This function stores the name of the executing program, which is included in various error and warning messages generated by the SDDS library routines.
 *
 * @param[in] name The name of the program. If `NULL`, the registered program name is cleared.
 *
 * @note This function should be called at the beginning of the program to provide context in error messages.
 *
 * @see SDDS_Bomb
 * @see SDDS_Warning
 */
void SDDS_RegisterProgramName(const char *name) {
  if (name)
    SDDS_CopyString(&registeredProgramName, (char *)name);
  else
    registeredProgramName = NULL;
}

/**
 * @brief Retrieves the number of errors recorded by SDDS library routines.
 *
 * This function returns the total number of errors that have been recorded by the SDDS library since the last invocation of `SDDS_PrintErrors`.
 *
 * @return The number of recorded errors.
 *
 * @see SDDS_PrintErrors
 */
int32_t SDDS_NumberOfErrors() {
  return (n_errors);
}

/**
 * @brief Clears all recorded error messages from the SDDS error stack.
 *
 * This function removes all error messages that have been recorded by SDDS library routines, resetting the error count to zero. It should be called after handling or logging the errors to prepare for future error recording.
 *
 * @note After calling this function, `SDDS_NumberOfErrors` will return zero until new errors are recorded.
 *
 * @see SDDS_SetError
 * @see SDDS_PrintErrors
 */
void SDDS_ClearErrors() {
  int32_t i;
  for (i=0; i<n_errors; i++) {
    free(error_description[i]);
    error_description[i] = NULL;
  }
  free(error_description);
  error_description = NULL;
  n_errors = 0;
  n_errors_max = 0;
}

/**
 * @brief Terminates the program after printing an error message and recorded errors.
 *
 * This function prints a termination message to `stderr`, invokes `SDDS_PrintErrors` to display all recorded errors, and then exits the program with a non-zero status.
 *
 * @param[in] message The termination message to be printed. If `NULL`, a default message `"?"` is used.
 *
 * @note This function does not return; it exits the program.
 *
 * @see SDDS_PrintErrors
 * @see SDDS_SetError
 */
void SDDS_Bomb(char *message) {
  if (registeredProgramName)
    fprintf(stderr, "Error (%s): %s\n", registeredProgramName, message ? message : "?");
  else
    fprintf(stderr, "Error: %s\n", message ? message : "?");
  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  exit(1);
}

/**
 * @brief Prints a warning message to `stderr`.
 *
 * This function outputs a warning message to the specified `FILE` stream, typically `stderr`. If a program name has been registered using `SDDS_RegisterProgramName`, it is included in the warning message.
 *
 * @param[in] message The warning message to be printed. If `NULL`, a default message `"?"` is used.
 *
 * @note This function does not record the warning as an error; it only prints the message.
 *
 * @see SDDS_RegisterProgramName
 */
void SDDS_Warning(char *message) {
  if (registeredProgramName)
    fprintf(stderr, "Warning (%s): %s\n", registeredProgramName, message ? message : "?");
  else
    fprintf(stderr, "Warning: %s\n", message ? message : "?");
}

/**
 * @brief Records an error message in the SDDS error stack.
 *
 * This function appends an error message to the internal error stack. These errors can later be retrieved and displayed using `SDDS_PrintErrors`.
 *
 * @param[in] error_text The error message to be recorded. If `NULL`, a warning is printed to `stderr`.
 *
 * @see SDDS_PrintErrors
 * @see SDDS_ClearErrors
 */
void SDDS_SetError(char *error_text) {
  SDDS_SetError0(error_text);
  SDDS_SetError0("\n");
}

/**
 * @brief Internal function to record an error message in the SDDS error stack.
 *
 * This function appends an error message to the internal error stack without adding additional formatting or line breaks. It is typically called by `SDDS_SetError`.
 *
 * @param[in] error_text The error message to be recorded. If `NULL`, a warning is printed to `stderr`.
 *
 * @note This function is intended for internal use within the SDDS library and should not be called directly by user code.
 *
 * @see SDDS_SetError
 */
void SDDS_SetError0(char *error_text) {
  if (n_errors >= n_errors_max) {
    if (!(error_description = SDDS_Realloc(error_description, (n_errors_max += 10) * sizeof(*error_description)))) {
      fputs("Error trying to allocate additional error description string (SDDS_SetError)\n", stderr);
      fprintf(stderr, "Most recent error text:\n%s\n", error_text);
      abort();
    }
  }
  if (!error_text)
    fprintf(stderr, "warning: error text is NULL (SDDS_SetError)\n");
  else {
    if (!SDDS_CopyString(&error_description[n_errors], error_text)) {
      fputs("Error trying to copy additional error description text (SDDS_SetError)\n", stderr);
      fprintf(stderr, "Most recent error text: %s\n", error_text);
      abort();
    }
    n_errors++;
  }
}

/**
 * @brief Prints recorded error messages to a specified file stream.
 *
 * This function outputs the errors that have been recorded by SDDS library routines to the given file stream. Depending on the `mode` parameter, it can print a single error, all recorded errors, and optionally terminate the program after printing.
 *
 * @param[in] fp   Pointer to the `FILE` stream where errors will be printed. Typically `stderr`.
 * @param[in] mode Flags controlling the error printing behavior:
 *                  - `0`: Print only the first recorded error.
 *                  - `SDDS_VERBOSE_PrintErrors`: Print all recorded errors.
 *                  - `SDDS_EXIT_PrintErrors`: After printing errors, terminate the program by calling `exit(1)`.
 *
 * @note After printing, the error stack is cleared. If `mode` includes `SDDS_EXIT_PrintErrors`, the program will terminate.
 *
 * @see SDDS_SetError
 * @see SDDS_NumberOfErrors
 * @see SDDS_ClearErrors
 */
void SDDS_PrintErrors(FILE *fp, int32_t mode) {
  int32_t i, depth;

  if (!n_errors)
    return;
  if (!fp) {
    n_errors = 0;
    return;
  }
  if (mode & SDDS_VERBOSE_PrintErrors)
    depth = n_errors;
  else
    depth = 1;
  if (registeredProgramName)
    fprintf(fp, "Error for %s:\n", registeredProgramName);
  else
    fputs("Error:\n", fp);
  if (!error_description)
    fprintf(stderr, "warning: internal error: error_description pointer is unexpectedly NULL\n");
  else
    for (i = 0; i < depth; i++) {
      if (!error_description[i])
        fprintf(stderr, "warning: internal error: error_description[%" PRId32 "] is unexpectedly NULL\n", i);
      fprintf(fp, "%s", error_description[i]);
    }
  fflush(fp);
  n_errors = 0;
  if (mode & SDDS_EXIT_PrintErrors)
    exit(1);
}

/**
 * @brief Retrieves recorded error messages from the SDDS error stack.
 *
 * This function fetches error messages that have been recorded by SDDS library routines. Depending on the `mode` parameter, it can retrieve a single error message or all recorded errors.
 *
 * @param[out] number Pointer to an `int32_t` variable where the number of retrieved error messages will be stored. If `NULL`, the function returns `NULL`.
 * @param[in]  mode   Flags controlling the retrieval behavior:
 *                     - `0`: Retrieve only the most recent error message.
 *                     - `SDDS_ALL_GetErrorMessages`: Retrieve all recorded error messages.
 *
 * @return A dynamically allocated array of strings containing the error messages. Returns `NULL` if no errors are recorded or if memory allocation fails.
 *
 * @note The caller is responsible for freeing the memory allocated for the returned error messages.
 *
 * @see SDDS_SetError
 * @see SDDS_ClearErrors
 */
char **SDDS_GetErrorMessages(int32_t *number, int32_t mode) {
  int32_t i, depth;
  char **message;

  if (!number)
    return NULL;

  *number = 0;
  if (!n_errors)
    return NULL;

  if (mode & SDDS_ALL_GetErrorMessages)
    depth = n_errors;
  else
    depth = 1;
  if (!(message = (char **)SDDS_Malloc(sizeof(*message) * depth)))
    return NULL;
  if (!error_description) {
    fprintf(stderr, "warning: internal error: error_description pointer is unexpectedly NULL (SDDS_GetErrorMessages)\n");
    return NULL;
  } else {
    for (i = depth - 1; i >= 0; i--) {
      if (!error_description[i]) {
        fprintf(stderr, "internal error: error_description[%" PRId32 "] is unexpectedly NULL (SDDS_GetErrorMessages)\n", i);
        return NULL;
      }
      if (!SDDS_CopyString(message + i, error_description[i])) {
        fprintf(stderr, "unable to copy error message text (SDDS_GetErrorMessages)\n");
        return NULL;
      }
    }
  }
  *number = depth;
  return message;
}

/*static uint32_t AutoCheckMode = TABULAR_DATA_CHECKS ;*/
static uint32_t AutoCheckMode = 0x0000UL;

/**
 * @brief Sets the automatic check mode for SDDS dataset validation.
 *
 * This function updates the auto-check mode, which controls the automatic validation of SDDS datasets during operations. The previous mode is returned.
 *
 * @param[in] newMode The new auto-check mode to be set. It should be a bitwise combination of the following constants:
 *                    - `TABULAR_DATA_CHECKS`: Enables checks for tabular data consistency.
 *                    - (Other mode flags as defined by SDDS)
 *
 * @return The previous auto-check mode before the update.
 *
 * @see SDDS_CheckDataset
 * @see SDDS_CheckTabularData
 */
uint32_t SDDS_SetAutoCheckMode(uint32_t newMode) {
  uint32_t oldMode;
  oldMode = AutoCheckMode;
  AutoCheckMode = newMode;
  return oldMode;
}

/**
 * @brief Validates the SDDS dataset pointer.
 *
 * This function checks whether the provided `SDDS_DATASET` pointer is valid (non-NULL). If the check fails, it records an appropriate error message.
 *
 * @param[in] SDDS_dataset Pointer to the `SDDS_DATASET` structure to be validated.
 * @param[in] caller        Name of the calling function, used for error reporting.
 *
 * @return Returns `1` if the dataset pointer is valid; otherwise, returns `0` and records an error message.
 *
 * @see SDDS_SetError
 */
int32_t SDDS_CheckDataset(SDDS_DATASET *SDDS_dataset, const char *caller) {
  char buffer[100];
  if (!SDDS_dataset) {
    sprintf(buffer, "NULL SDDS_DATASET pointer passed to %s", caller);
    SDDS_SetError(buffer);
    return (0);
  }
  return (1);
}

/**
 * @brief Validates the consistency of tabular data within an SDDS dataset.
 *
 * This function checks the integrity of tabular data in the given `SDDS_DATASET`. It verifies that if columns are defined, corresponding row flags and data arrays exist, and that the number of rows matches the column definitions.
 *
 * @param[in] SDDS_dataset Pointer to the `SDDS_DATASET` structure to be validated.
 * @param[in] caller        Name of the calling function, used for error reporting.
 *
 * @return Returns `1` if the tabular data is consistent and valid; otherwise, returns `0` and records an error message.
 *
 * @note This function performs checks only if `AutoCheckMode` includes `TABULAR_DATA_CHECKS`.
 *
 * @see SDDS_SetAutoCheckMode
 * @see SDDS_SetError
 */
int32_t SDDS_CheckTabularData(SDDS_DATASET *SDDS_dataset, const char *caller) {
  int64_t i;
  char buffer[100];
  if (!(AutoCheckMode & TABULAR_DATA_CHECKS))
    return 1;
  if (SDDS_dataset->layout.n_columns && (!SDDS_dataset->row_flag || !SDDS_dataset->data)) {
    sprintf(buffer, "tabular data is invalid in %s (columns but no row flags or data array)", caller);
    SDDS_SetError(buffer);
    return (0);
  }
  if (SDDS_dataset->layout.n_columns == 0 && SDDS_dataset->n_rows) {
    sprintf(buffer, "tabular data is invalid in %s (no columns present but nonzero row count)", caller);
    SDDS_SetError(buffer);
    return (0);
  }
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
    if (!SDDS_dataset->data[i]) {
      sprintf(buffer, "tabular data is invalid in %s (null data pointer for column %" PRId64 ")", caller, i);
      SDDS_SetError(buffer);
      return (0);
    }
  }
  return (1);
}

/**
 * @brief Allocates zero-initialized memory for an array of elements.
 *
 * This function is a wrapper around the standard `calloc` function, used by SDDS routines to allocate memory. It ensures that even if the requested number of elements or element size is zero or negative, a minimum of 1 element with a size of 4 bytes is allocated.
 *
 * @param[in] nelem     Number of elements to allocate.
 * @param[in] elem_size Size in bytes of each element.
 *
 * @return Pointer to the allocated memory. If allocation fails, returns `NULL`.
 *
 * @note If `nelem` or `elem_size` is less than or equal to zero, the function allocates memory for one element of 4 bytes by default.
 *
 * @see SDDS_Malloc
 * @see SDDS_Free
 */
void *SDDS_Calloc(size_t nelem, size_t elem_size) {
  if (elem_size <= 0)
    elem_size = 4;
  if (nelem <= 0)
    nelem = 1;
  return calloc(nelem, elem_size);
}

/**
 * @brief Allocates memory of a specified size.
 *
 * This function is a wrapper around the standard `malloc` function, used by SDDS routines to allocate memory. It ensures that a minimum allocation size is enforced.
 *
 * @param[in] size Number of bytes to allocate.
 *
 * @return Pointer to the allocated memory. If `size` is less than or equal to zero, it allocates memory for 4 bytes by default. Returns `NULL` if memory allocation fails.
 *
 * @note Users should always check the returned pointer for `NULL` before using it.
 *
 * @see SDDS_Calloc
 * @see SDDS_Free
 */
void *SDDS_Malloc(size_t size) {
  if (size <= 0)
    size = 4;
  return malloc(size);
}

/**
 * @brief Free memory previously allocated by SDDS_Malloc.
 *
 * This function frees memory that wsa previously allocated by SDDS_Malloc.
 *
 * @param[in] mem  Pointer to the memory block.
 *
 * @see SDDS_Malloc
 * @see SDDS_Calloc
 */
void SDDS_Free(void *mem) {
  /* this is required so the free will be consistent with the malloc.
     On WIN32 the release (optimized) version of malloc is different
     from the debug (unoptimized) version, so debug programs freeing
     memory that was allocated by release, library routines encounter
     problems. */
  free(mem);
}

/**
 * @brief Reallocates memory to a new size.
 *
 * This function extends the standard `realloc` functionality by zero-initializing any newly allocated memory beyond the original size. It ensures that memory is consistently reallocated and initialized across different build configurations.
 *
 * @param[in] old_ptr  Pointer to the original memory block. If `NULL`, the function behaves like `SDDS_Malloc`.
 * @param[in] new_size New size in bytes for the memory block.
 *
 * @return Pointer to the reallocated memory block with the new size. If `new_size` is less than or equal to zero, a minimum of 4 bytes is allocated. Returns `NULL` if memory reallocation fails.
 *
 * @see SDDS_Malloc
 * @see SDDS_Free
 */
void *SDDS_Realloc(void *old_ptr, size_t new_size) {
  /* this is required because some realloc's don't behave properly when asked to return a
   * pointer to 0 memory.  They return NULL.
   */
  if (new_size <= 0)
    new_size = 4;
  /* this is required because some realloc's don't behave properly when given a NULL pointer */
  if (!old_ptr)
    return (SDDS_Malloc(new_size));
  else
    return (realloc(old_ptr, new_size));
}

/**
 * @brief Reallocates memory to a new size and zero-initializes the additional space.
 *
 * This function reallocates a memory block to a new size, similar to the standard `realloc` function. Additionally, it ensures that any newly allocated memory beyond the original size is set to zero. This is particularly useful when extending memory blocks to avoid uninitialized memory usage.
 *
 * @param[in]  old_ptr  Pointer to the original memory block. If `NULL`, the function behaves like `SDDS_Calloc`.
 * @param[in]  old_size Size in bytes of the original memory block.
 * @param[in]  new_size New size in bytes for the memory block.
 *
 * @return Pointer to the reallocated memory block with the new size. If `new_size` is less than or equal to zero, a minimum of 4 bytes is allocated. Returns `NULL` if memory reallocation fails.
 *
 * @note After reallocation, the memory from `old_size` to `new_size` bytes is set to zero. If `old_ptr` is `NULL`, the function allocates memory initialized to zero.
 *
 * @see SDDS_Malloc
 * @see SDDS_Calloc
 * @see SDDS_Free
 */
void *SDDS_Recalloc(void *old_ptr, size_t old_size, size_t new_size) {
  /* this is required because some realloc's don't behave properly when asked to return a
   * pointer to 0 memory.  They return NULL.
   * Also, need to clear the memory (in this version).
   */
  void *new_ptr;
  if (new_size <= 0)
    new_size = 4;
  /* this is required because some realloc's don't behave properly when given a NULL pointer */
  if (!old_ptr)
    new_ptr = calloc(new_size, 1);
  else {
    new_ptr = realloc(old_ptr, new_size);
    memset((char *)new_ptr + old_size, 0, new_size - old_size);
  }
  return new_ptr;
}

/**
 * @brief Verifies that a printf format string is compatible with a specified data type.
 *
 * This function checks whether the provided printf format string is appropriate for the given SDDS data type. It ensures that the format specifier matches the type, preventing potential formatting errors during data output.
 *
 * @param[in]  string The printf format string to be verified.
 * @param[in]  type   The data type against which the format string is verified. Must be one of the SDDS type constants:
 *                   - `SDDS_LONGDOUBLE`
 *                   - `SDDS_DOUBLE`
 *                   - `SDDS_FLOAT`
 *                   - `SDDS_LONG`
 *                   - `SDDS_LONG64`
 *                   - `SDDS_ULONG`
 *                   - `SDDS_ULONG64`
 *                   - `SDDS_SHORT`
 *                   - `SDDS_USHORT`
 *                   - `SDDS_STRING`
 *                   - `SDDS_CHARACTER`
 *
 * @return Returns `1` if the format string is valid for the specified type; otherwise, returns `0` and records an error message.
 *
 * @note This function does not modify the format string; it only validates its compatibility with the given type.
 *
 * @see SDDS_SetError
 */
int32_t SDDS_VerifyPrintfFormat(const char *string, int32_t type) {
  char *percent, *s;
  int32_t len, tmp;

  s = (char *)string;
  do {
    if ((percent = strchr(s, '%'))) {
      if (*(percent + 1) != '%')
        break;
      s = percent + 1;
    }
  } while (percent);
  if (!percent || !*++percent)
    return (0);

  s = percent;

  switch (type) {
  case SDDS_LONGDOUBLE:
  case SDDS_DOUBLE:
  case SDDS_FLOAT:
    if ((len = strcspn(s, "fegEG")) == strlen(s))
      return (0);
    if (len == 0)
      return (1);
    if ((tmp = strspn(s, "-+.0123456789 ")) < len)
      return (0);
    break;
  case SDDS_LONG:
  case SDDS_LONG64:
    if ((len = strcspn(s, "d")) == strlen(s))
      return (0);
    /*    if (*(s+len-1)!='l')
            return(0); */
    if (--len == 0)
      return (1);
    if ((tmp = strspn(s, "-+.0123456789 ")) < len)
      return (0);
    break;
  case SDDS_ULONG:
  case SDDS_ULONG64:
    if ((len = strcspn(s, "u")) == strlen(s))
      return (0);
    /*    if (*(s+len-1)!='l')
            return(0); */
    if (--len == 0)
      return (1);
    if ((tmp = strspn(s, "-+.0123456789 ")) < len)
      return (0);
    break;
  case SDDS_SHORT:
    if ((len = strcspn(s, "d")) == strlen(s))
      return (0);
    if (*(s + len - 1) != 'h')
      return (0);
    if (--len == 0)
      return (1);
    if ((tmp = strspn(s, "-+.0123456789 ")) < len)
      return (0);
    break;
  case SDDS_USHORT:
    if ((len = strcspn(s, "u")) == strlen(s))
      return (0);
    if (*(s + len - 1) != 'h')
      return (0);
    if (--len == 0)
      return (1);
    if ((tmp = strspn(s, "-+.0123456789 ")) < len)
      return (0);
    break;
  case SDDS_STRING:
    if ((len = strcspn(s, "s")) == strlen(s))
      return (0);
    if (len == 0)
      return (1);
    if ((tmp = strspn(s, "-0123456789")) < len)
      return (0);
    break;
  case SDDS_CHARACTER:
    if ((len = strcspn(s, "c")) == strlen(s))
      return (0);
    if (len != 0)
      return (0);
    break;
  default:
    return (0);
  }
  /* no errors found--its probably okay */
  return (1);
}

/**
 * @brief Copies a source string to a target string with memory allocation.
 *
 * This function allocates memory for the target string and copies the contents of the source string into it. If the source string is `NULL`, the target string is set to `NULL`.
 *
 * @param[out] target Pointer to a `char*` variable where the copied string will be stored. Memory is allocated within this function and should be freed by the caller to avoid memory leaks.
 * @param[in]  source The source string to be copied. If `NULL`, the target is set to `NULL`.
 *
 * @return Returns `1` on successful copy and memory allocation. Returns `0` on error (e.g., memory allocation failure).
 *
 * @note The caller is responsible for freeing the memory allocated for the target string.
 *
 * @see SDDS_Free
 * @see SDDS_Malloc
 */
int32_t SDDS_CopyString(char **target, const char *source) {
  if (!source)
    *target = NULL;
  else {
    if (!(*target = SDDS_Malloc(sizeof(**target) * (strlen(source) + 1))))
      return (0);
    strcpy(*target, source);
  }
  return (1);
}

/**
 * @brief Retrieves the definition of a specified associate from the SDDS dataset.
 *
 * This function searches for an associate by its name within the provided SDDS dataset. If found, it creates a copy of the associate's definition and returns a pointer to it. The returned pointer should be freed by the caller using `SDDS_FreeAssociateDefinition` to avoid memory leaks.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the associate to retrieve.
 *
 * @return On success, returns a pointer to a newly allocated `ASSOCIATE_DEFINITION` structure containing the associate's information. On failure (e.g., if the associate is not found or a copy fails), returns `NULL` and records an error message.
 *
 * @note The caller is responsible for freeing the returned `ASSOCIATE_DEFINITION` pointer using `SDDS_FreeAssociateDefinition`.
 *
 * @see SDDS_CopyAssociateDefinition
 * @see SDDS_FreeAssociateDefinition
 * @see SDDS_SetError
 */
ASSOCIATE_DEFINITION *SDDS_GetAssociateDefinition(SDDS_DATASET *SDDS_dataset, char *name) {
  int32_t i;
  ASSOCIATE_DEFINITION *assdef;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetAssociateDefinition"))
    return (NULL);
  if (!name) {
    SDDS_SetError("Unable to get associate definition--name is NULL (SDDS_GetAssociateDefinition)");
    return (NULL);
  }
  for (i = 0; i < SDDS_dataset->layout.n_associates; i++) {
    if (strcmp(SDDS_dataset->layout.associate_definition[i].name, name) == 0) {
      if (!SDDS_CopyAssociateDefinition(&assdef, SDDS_dataset->layout.associate_definition + i)) {
        SDDS_SetError("Unable to get associate definition--copy failure  (SDDS_GetAssociateDefinition)");
        return (NULL);
      }
      return (assdef);
    }
  }
  return (NULL);
}

/**
 * @brief Creates a copy of an associate definition.
 *
 * This function allocates memory for a new `ASSOCIATE_DEFINITION` structure and copies the contents from the source associate definition to the target. All string fields are duplicated to ensure independent memory management.
 *
 * @param[out] target Pointer to a `ASSOCIATE_DEFINITION*` where the copied definition will be stored.
 * @param[in]  source Pointer to the `ASSOCIATE_DEFINITION` structure to be copied. If `source` is `NULL`, the target is set to `NULL`.
 *
 * @return Returns a pointer to the copied `ASSOCIATE_DEFINITION` structure on success. Returns `NULL` on failure (e.g., memory allocation failure).
 *
 * @note The caller is responsible for freeing the copied associate definition using `SDDS_FreeAssociateDefinition`.
 *
 * @see SDDS_FreeAssociateDefinition
 * @see SDDS_Malloc
 * @see SDDS_CopyString
 */
ASSOCIATE_DEFINITION *SDDS_CopyAssociateDefinition(ASSOCIATE_DEFINITION **target, ASSOCIATE_DEFINITION *source) {
  if (!source)
    return (*target = NULL);
  if (!(*target = (ASSOCIATE_DEFINITION *)SDDS_Malloc(sizeof(**target))) ||
      !SDDS_CopyString(&(*target)->name, source->name) || !SDDS_CopyString(&(*target)->filename, source->filename) || !SDDS_CopyString(&(*target)->path, source->path) || !SDDS_CopyString(&(*target)->description, source->description) || !SDDS_CopyString(&(*target)->contents, source->contents))
    return (NULL);
  (*target)->sdds = source->sdds;
  return (*target);
}

/**
 * @brief Frees memory allocated for an associate definition.
 *
 * This function deallocates all memory associated with an `ASSOCIATE_DEFINITION` structure, including its string fields. After freeing, the structure is zeroed out to prevent dangling pointers.
 *
 * @param[in] source Pointer to the `ASSOCIATE_DEFINITION` structure to be freed.
 *
 * @return Returns `1` on successful deallocation. Returns `0` if the `source` is `NULL` or if required fields are missing.
 *
 * @note After calling this function, the `source` pointer becomes invalid and should not be used.
 *
 * @see SDDS_CopyAssociateDefinition
 * @see SDDS_Free
 */
int32_t SDDS_FreeAssociateDefinition(ASSOCIATE_DEFINITION *source) {
  if (!source->name)
    return (0);
  free(source->name);
  if (!source->filename)
    return (0);
  free(source->filename);
  if (source->path)
    free(source->path);
  if (source->description)
    free(source->description);
  if (source->contents)
    free(source->contents);
  SDDS_ZeroMemory(source, sizeof(*source));
  free(source);
  return (1);
}

/**
 * @brief Retrieves the definition of a specified column from the SDDS dataset.
 *
 * This function searches for a column by its name within the provided SDDS dataset. If found, it creates a copy of the column's definition and returns a pointer to it. The returned pointer should be freed by the caller using `SDDS_FreeColumnDefinition` to avoid memory leaks.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the column to retrieve.
 *
 * @return On success, returns a pointer to a newly allocated `COLUMN_DEFINITION` structure containing the column's information. On failure (e.g., if the column is not found or a copy fails), returns `NULL` and records an error message.
 *
 * @note The caller is responsible for freeing the returned `COLUMN_DEFINITION` pointer using `SDDS_FreeColumnDefinition`.
 *
 * @see SDDS_CopyColumnDefinition
 * @see SDDS_FreeColumnDefinition
 * @see SDDS_SetError
 */
COLUMN_DEFINITION *SDDS_GetColumnDefinition(SDDS_DATASET *SDDS_dataset, char *name) {
  int64_t i;
  COLUMN_DEFINITION *coldef;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnDefinition"))
    return (NULL);
  if (!name) {
    SDDS_SetError("Unable to get column definition--name is NULL (SDDS_GetColumnDefinition)");
    return (NULL);
  }
  if ((i = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0)
    return NULL;
  if (!SDDS_CopyColumnDefinition(&coldef, SDDS_dataset->layout.column_definition + i)) {
    SDDS_SetError("Unable to get column definition--copy failure  (SDDS_GetColumnDefinition)");
    return (NULL);
  }
  return (coldef);
}

/**
 * @brief Creates a copy of a column definition.
 *
 * This function allocates memory for a new `COLUMN_DEFINITION` structure and copies the contents from the source column definition to the target. All string fields are duplicated to ensure independent memory management.
 *
 * @param[out] target Pointer to a `COLUMN_DEFINITION*` where the copied definition will be stored.
 * @param[in]  source Pointer to the `COLUMN_DEFINITION` structure to be copied. If `source` is `NULL`, the target is set to `NULL`.
 *
 * @return Returns a pointer to the copied `COLUMN_DEFINITION` structure on success. Returns `NULL` on failure (e.g., memory allocation failure).
 *
 * @note The caller is responsible for freeing the copied column definition using `SDDS_FreeColumnDefinition`.
 *
 * @see SDDS_FreeColumnDefinition
 * @see SDDS_Malloc
 * @see SDDS_CopyString
 */
COLUMN_DEFINITION *SDDS_CopyColumnDefinition(COLUMN_DEFINITION **target, COLUMN_DEFINITION *source) {
  if (!target)
    return NULL;
  if (!source)
    return (*target = NULL);
  if (!(*target = (COLUMN_DEFINITION *)SDDS_Malloc(sizeof(**target))) ||
      !SDDS_CopyString(&(*target)->name, source->name) ||
      !SDDS_CopyString(&(*target)->symbol, source->symbol) || !SDDS_CopyString(&(*target)->units, source->units) || !SDDS_CopyString(&(*target)->description, source->description) || !SDDS_CopyString(&(*target)->format_string, source->format_string))
    return (NULL);
  (*target)->type = source->type;
  (*target)->field_length = source->field_length;
  (*target)->definition_mode = source->definition_mode;
  (*target)->memory_number = source->memory_number;
  return (*target);
}

/**
 * @brief Frees memory allocated for a column definition.
 *
 * This function deallocates all memory associated with a `COLUMN_DEFINITION` structure, including its string fields. After freeing, the structure is zeroed out to prevent dangling pointers.
 *
 * @param[in] source Pointer to the `COLUMN_DEFINITION` structure to be freed.
 *
 * @return Returns `1` on successful deallocation. Returns `0` if the `source` is `NULL` or if required fields are missing.
 *
 * @note After calling this function, the `source` pointer becomes invalid and should not be used.
 *
 * @see SDDS_CopyColumnDefinition
 * @see SDDS_Free
 */
int32_t SDDS_FreeColumnDefinition(COLUMN_DEFINITION *source) {
  if (!source || !source->name)
    return (0);
  free(source->name);
  if (source->symbol)
    free(source->symbol);
  if (source->units)
    free(source->units);
  if (source->description)
    free(source->description);
  if (source->format_string)
    free(source->format_string);
  SDDS_ZeroMemory(source, sizeof(*source));
  free(source);
  return (1);
}

/**
 * @brief Retrieves the definition of a specified parameter from the SDDS dataset.
 *
 * This function searches for a parameter by its name within the provided SDDS dataset. If found, it creates a copy of the parameter's definition and returns a pointer to it. The returned pointer should be freed by the caller using `SDDS_FreeParameterDefinition` to avoid memory leaks.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the parameter to retrieve.
 *
 * @return On success, returns a pointer to a newly allocated `PARAMETER_DEFINITION` structure containing the parameter's information. On failure (e.g., if the parameter is not found or a copy fails), returns `NULL` and records an error message.
 *
 * @note The caller is responsible for freeing the returned `PARAMETER_DEFINITION` pointer using `SDDS_FreeParameterDefinition`.
 *
 * @see SDDS_CopyParameterDefinition
 * @see SDDS_FreeParameterDefinition
 * @see SDDS_SetError
 */
PARAMETER_DEFINITION *SDDS_GetParameterDefinition(SDDS_DATASET *SDDS_dataset, char *name) {
  int32_t i;
  PARAMETER_DEFINITION *pardef;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterDefinition"))
    return (NULL);
  if (!name) {
    SDDS_SetError("Unable to get parameter definition--name is NULL (SDDS_GetParameterDefinition)");
    return (NULL);
  }
  if ((i = SDDS_GetParameterIndex(SDDS_dataset, name)) < 0)
    return NULL;
  if (!SDDS_CopyParameterDefinition(&pardef, SDDS_dataset->layout.parameter_definition + i)) {
    SDDS_SetError("Unable to get parameter definition--copy failure  (SDDS_GetParameterDefinition)");
    return (NULL);
  }
  return (pardef);
}

/**
 * @brief Creates a copy of a parameter definition.
 *
 * This function allocates memory for a new `PARAMETER_DEFINITION` structure and copies the contents from the source parameter definition to the target. All string fields are duplicated to ensure independent memory management.
 *
 * @param[out] target Pointer to a `PARAMETER_DEFINITION*` where the copied definition will be stored.
 * @param[in]  source Pointer to the `PARAMETER_DEFINITION` structure to be copied. If `source` is `NULL`, the target is set to `NULL`.
 *
 * @return Returns a pointer to the copied `PARAMETER_DEFINITION` structure on success. Returns `NULL` on failure (e.g., memory allocation failure).
 *
 * @note The caller is responsible for freeing the copied parameter definition using `SDDS_FreeParameterDefinition`.
 *
 * @see SDDS_FreeParameterDefinition
 * @see SDDS_Malloc
 * @see SDDS_CopyString
 */
PARAMETER_DEFINITION *SDDS_CopyParameterDefinition(PARAMETER_DEFINITION **target, PARAMETER_DEFINITION *source) {
  if (!target)
    return NULL;
  if (!source)
    return (*target = NULL);
  if (!(*target = (PARAMETER_DEFINITION *)SDDS_Malloc(sizeof(**target))) ||
      !SDDS_CopyString(&(*target)->name, source->name) ||
      !SDDS_CopyString(&(*target)->symbol, source->symbol) ||
      !SDDS_CopyString(&(*target)->units, source->units) || !SDDS_CopyString(&(*target)->description, source->description) || !SDDS_CopyString(&(*target)->format_string, source->format_string) || !SDDS_CopyString(&(*target)->fixed_value, source->fixed_value))
    return (NULL);
  (*target)->type = source->type;
  (*target)->definition_mode = source->definition_mode;
  (*target)->memory_number = source->memory_number;
  return (*target);
}

/**
 * @brief Frees memory allocated for a parameter definition.
 *
 * This function deallocates all memory associated with a `PARAMETER_DEFINITION` structure, including its string fields. After freeing, the structure is zeroed out to prevent dangling pointers.
 *
 * @param[in] source Pointer to the `PARAMETER_DEFINITION` structure to be freed.
 *
 * @return Returns `1` on successful deallocation. Returns `0` if the `source` is `NULL` or if required fields are missing.
 *
 * @note After calling this function, the `source` pointer becomes invalid and should not be used.
 *
 * @see SDDS_CopyParameterDefinition
 * @see SDDS_Free
 */
int32_t SDDS_FreeParameterDefinition(PARAMETER_DEFINITION *source) {
  if (!source || !source->name)
    return (0);
  free(source->name);
  if (source->symbol)
    free(source->symbol);
  if (source->units)
    free(source->units);
  if (source->description)
    free(source->description);
  if (source->format_string)
    free(source->format_string);
  if (source->fixed_value)
    free(source->fixed_value);
  SDDS_ZeroMemory(source, sizeof(*source));
  free(source);
  return (1);
}

/**
 * @brief Retrieves the definition of a specified array from the SDDS dataset.
 *
 * This function searches for an array by its name within the provided SDDS dataset. If found, it creates a copy of the array's definition and returns a pointer to it. The returned pointer should be freed by the caller using `SDDS_FreeArrayDefinition` to avoid memory leaks.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the array to retrieve.
 *
 * @return On success, returns a pointer to a newly allocated `ARRAY_DEFINITION` structure containing the array's information. On failure (e.g., if the array is not found or a copy fails), returns `NULL` and records an error message.
 *
 * @note The caller is responsible for freeing the returned `ARRAY_DEFINITION` pointer using `SDDS_FreeArrayDefinition`.
 *
 * @see SDDS_CopyArrayDefinition
 * @see SDDS_FreeArrayDefinition
 * @see SDDS_SetError
 */
ARRAY_DEFINITION *SDDS_GetArrayDefinition(SDDS_DATASET *SDDS_dataset, char *name) {
  int32_t i;
  ARRAY_DEFINITION *arraydef;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetArrayDefinition"))
    return (NULL);
  if (!name) {
    SDDS_SetError("Unable to get array definition--name is NULL (SDDS_GetArrayDefinition)");
    return (NULL);
  }
  if ((i = SDDS_GetArrayIndex(SDDS_dataset, name)) < 0)
    return NULL;
  if (!SDDS_CopyArrayDefinition(&arraydef, SDDS_dataset->layout.array_definition + i)) {
    SDDS_SetError("Unable to get array definition--copy failure  (SDDS_GetArrayDefinition)");
    return (NULL);
  }
  return (arraydef);
}

/**
 * @brief Creates a copy of an array definition.
 *
 * This function allocates memory for a new `ARRAY_DEFINITION` structure and copies the contents from the source array definition to the target. All string fields are duplicated to ensure independent memory management.
 *
 * @param[out] target Pointer to a `ARRAY_DEFINITION*` where the copied definition will be stored.
 * @param[in]  source Pointer to the `ARRAY_DEFINITION` structure to be copied. If `source` is `NULL`, the target is set to `NULL`.
 *
 * @return Returns a pointer to the copied `ARRAY_DEFINITION` structure on success. Returns `NULL` on failure (e.g., memory allocation failure).
 *
 * @note The caller is responsible for freeing the copied array definition using `SDDS_FreeArrayDefinition`.
 *
 * @see SDDS_FreeArrayDefinition
 * @see SDDS_Malloc
 * @see SDDS_CopyString
 */
ARRAY_DEFINITION *SDDS_CopyArrayDefinition(ARRAY_DEFINITION **target, ARRAY_DEFINITION *source) {
  if (!target)
    return NULL;
  if (!source)
    return (*target = NULL);
  if (!(*target = (ARRAY_DEFINITION *)SDDS_Malloc(sizeof(**target))) ||
      !SDDS_CopyString(&(*target)->name, source->name) ||
      !SDDS_CopyString(&(*target)->symbol, source->symbol) ||
      !SDDS_CopyString(&(*target)->units, source->units) || !SDDS_CopyString(&(*target)->description, source->description) || !SDDS_CopyString(&(*target)->format_string, source->format_string) || !SDDS_CopyString(&(*target)->group_name, source->group_name))
    return (NULL);
  (*target)->type = source->type;
  (*target)->field_length = source->field_length;
  (*target)->dimensions = source->dimensions;
  return (*target);
}

/**
 * @brief Frees memory allocated for an array definition.
 *
 * This function deallocates all memory associated with an `ARRAY_DEFINITION` structure, including its string fields. After freeing, the structure is zeroed out to prevent dangling pointers.
 *
 * @param[in] source Pointer to the `ARRAY_DEFINITION` structure to be freed.
 *
 * @return Returns `1` on successful deallocation. Returns `0` if the `source` is `NULL`.
 *
 * @note After calling this function, the `source` pointer becomes invalid and should not be used.
 *
 * @see SDDS_CopyArrayDefinition
 * @see SDDS_Free
 */
int32_t SDDS_FreeArrayDefinition(ARRAY_DEFINITION *source) {
  if (!source)
    return (0);
  if (source->name)
    free(source->name);
  if (source->symbol)
    free(source->symbol);
  if (source->units)
    free(source->units);
  if (source->description)
    free(source->description);
  if (source->format_string)
    free(source->format_string);
  if (source->group_name)
    free(source->group_name);
  SDDS_ZeroMemory(source, sizeof(*source));
  free(source);
  source = NULL;
  return (1);
}

/**
 * @brief Compares two `SORTED_INDEX` structures by their name fields.
 *
 * This function is used as a comparison callback for sorting functions like `qsort`. It compares the `name` fields of two `SORTED_INDEX` structures lexicographically.
 *
 * @param[in] s1 Pointer to the first `SORTED_INDEX` structure.
 * @param[in] s2 Pointer to the second `SORTED_INDEX` structure.
 *
 * @return An integer less than, equal to, or greater than zero if the `name` of `s1` is found, respectively, to be less than, to match, or be greater than the `name` of `s2`.
 *
 * @see qsort
 * @see SORTED_INDEX
 */
int SDDS_CompareIndexedNames(const void *s1, const void *s2) {
  return strcmp(((SORTED_INDEX *)s1)->name, ((SORTED_INDEX *)s2)->name);
}

/* This routine is used with qsort.  Use const void * to avoid warning
 * message from SUN Solaris compiler.
 */
/**
 * @brief Compares two pointers to `SORTED_INDEX` structures by their name fields.
 *
 * This function is used as a comparison callback for sorting functions like `qsort`. It compares the `name` fields of two `SORTED_INDEX` structure pointers lexicographically.
 *
 * @param[in] s1 Pointer to the first `SORTED_INDEX*` structure.
 * @param[in] s2 Pointer to the second `SORTED_INDEX*` structure.
 *
 * @return An integer less than, equal to, or greater than zero if the `name` of `*s1` is found, respectively, to be less than, to match, or be greater than the `name` of `*s2`.
 *
 * @see qsort
 * @see SORTED_INDEX
 */
int SDDS_CompareIndexedNamesPtr(const void *s1, const void *s2) {
  return strcmp((*((SORTED_INDEX **)s1))->name, (*((SORTED_INDEX **)s2))->name);
}

/**
 * @brief Retrieves the index of a named column in the SDDS dataset.
 *
 * This function searches for a column by its name within the provided SDDS dataset and returns its index. The index can then be used with other routines for faster access to the column's data or metadata.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the column whose index is desired.
 *
 * @return On success, returns a non-negative integer representing the index of the column. On failure (e.g., if the column is not found), returns `-1` and records an error message.
 *
 * @see SDDS_GetColumnDefinition
 * @see SDDS_SetError
 */
int32_t SDDS_GetColumnIndex(SDDS_DATASET *SDDS_dataset, char *name) {
  int64_t i;
  SORTED_INDEX key;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnIndex"))
    return (-1);
  if (!name) {
    SDDS_SetError("Unable to get column index--name is NULL (SDDS_GetColumnIndex)");
    return (-1);
  }
  key.name = name;
  if ((i = binaryIndexSearch((void **)SDDS_dataset->layout.column_index, SDDS_dataset->layout.n_columns, &key, SDDS_CompareIndexedNames, 0)) < 0)
    return -1;
  return SDDS_dataset->layout.column_index[i]->index;
}

/**
 * @brief Retrieves the index of a named parameter in the SDDS dataset.
 *
 * This function searches for a parameter by its name within the provided SDDS dataset and returns its index. The index can then be used with other routines for faster access to the parameter's data or metadata.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the parameter whose index is desired.
 *
 * @return On success, returns a non-negative integer representing the index of the parameter. On failure (e.g., if the parameter is not found), returns `-1` and records an error message.
 *
 * @see SDDS_GetParameterDefinition
 * @see SDDS_SetError
 */
int32_t SDDS_GetParameterIndex(SDDS_DATASET *SDDS_dataset, char *name) {
  int32_t i;
  SORTED_INDEX key;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterIndex"))
    return (-1);
  if (!name) {
    SDDS_SetError("Unable to get parameter index--name is NULL (SDDS_GetParameterIndex)");
    return (-1);
  }
  key.name = name;
  if ((i = binaryIndexSearch((void **)SDDS_dataset->layout.parameter_index, SDDS_dataset->layout.n_parameters, &key, SDDS_CompareIndexedNames, 0)) < 0)
    return -1;
  return SDDS_dataset->layout.parameter_index[i]->index;
}

/**
 * @brief Retrieves the index of a named array in the SDDS dataset.
 *
 * This function searches for an array by its name within the provided SDDS dataset and returns its index. The index can then be used with other routines for faster access to the array's data or metadata.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the array whose index is desired.
 *
 * @return On success, returns a non-negative integer representing the index of the array. On failure (e.g., if the array is not found), returns `-1` and records an error message.
 *
 * @see SDDS_GetArrayDefinition
 * @see SDDS_SetError
 */
int32_t SDDS_GetArrayIndex(SDDS_DATASET *SDDS_dataset, char *name) {
  int32_t i;
  SORTED_INDEX key;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetArrayIndex"))
    return (-1);
  if (!name) {
    SDDS_SetError("Unable to get array index--name is NULL (SDDS_GetArrayIndex)");
    return (-1);
  }
  key.name = name;
  if ((i = binaryIndexSearch((void **)SDDS_dataset->layout.array_index, SDDS_dataset->layout.n_arrays, &key, SDDS_CompareIndexedNames, 0)) < 0)
    return -1;
  return SDDS_dataset->layout.array_index[i]->index;
}

/**
 * @brief Retrieves the index of a named associate in the SDDS dataset.
 *
 * This function searches for an associate by its name within the provided SDDS dataset and returns its index. The index can then be used with other routines for faster access to the associate's data or metadata.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the associate whose index is desired.
 *
 * @return On success, returns a non-negative integer representing the index of the associate. On failure (e.g., if the associate is not found), returns `-1` and records an error message.
 *
 * @see SDDS_GetAssociateDefinition
 * @see SDDS_SetError
 */
int32_t SDDS_GetAssociateIndex(SDDS_DATASET *SDDS_dataset, char *name) {
  int32_t i;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetAssociateIndex"))
    return (-1);
  if (!name) {
    SDDS_SetError("Unable to get associate index--name is NULL (SDDS_GetAssociateIndex)");
    return (-1);
  }
  for (i = 0; i < SDDS_dataset->layout.n_associates; i++) {
    if (strcmp(SDDS_dataset->layout.associate_definition[i].name, name) == 0)
      return (i);
  }
  return (-1);
}

/**
 * @brief Checks if a string contains any whitespace characters.
 *
 * This function scans through the provided string to determine if it contains any whitespace characters (e.g., space, tab, newline).
 *
 * @param[in]  string Pointer to the null-terminated string to be checked.
 *
 * @return Returns `1` if the string contains at least one whitespace character. Returns `0` if no whitespace characters are found or if the input string is `NULL`.
 *
 * @see isspace
 */
int32_t SDDS_HasWhitespace(char *string) {
  if (!string)
    return (0);
  while (*string) {
    if (isspace(*string))
      return (1);
    string++;
  }
  return (0);
}

/**
 * @brief Reads a line from a file while skipping comment lines.
 *
 * This function reads lines from the specified file stream, ignoring lines that begin with the specified `skip_char`. It also processes special comment lines that start with `!#` by parsing them using `SDDS_ParseSpecialComments`.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure, used for processing comments.
 * @param[out] s            Pointer to a character array where the read line will be stored.
 * @param[in]  slen         The maximum number of characters to read into `s`.
 * @param[in]  fp           Pointer to the `FILE` stream to read from.
 * @param[in]  skip_char    Character indicating the start of a comment line. Lines beginning with this character will be skipped.
 *
 * @return On success, returns the pointer `s` containing the read line. If the end of the file is reached or an error occurs, returns `NULL`.
 *
 * @note The function modifies the buffer `s` by removing comments as determined by `SDDS_CutOutComments`.
 *
 * @see SDDS_CutOutComments
 * @see SDDS_ParseSpecialComments
 */
char *fgetsSkipComments(SDDS_DATASET *SDDS_dataset, char *s, int32_t slen, FILE *fp, char skip_char /* ignore lines that begin with this character */) {
  while (fgets(s, slen, fp)) {
    if (s[0] != skip_char) {
      SDDS_CutOutComments(SDDS_dataset, s, skip_char);
      return (s);
    } else if (s[1] == '#') {
      SDDS_ParseSpecialComments(SDDS_dataset, s + 2);
    }
  }
  return (NULL);
}

/**
 * @brief Reads a line from a file with dynamic buffer resizing while skipping comment lines.
 *
 * This function reads lines from the specified file stream, ignoring lines that begin with the specified `skip_char`. If a line exceeds the current buffer size, the buffer is dynamically resized to accommodate the entire line. It also processes special comment lines that start with `!#` by parsing them using `SDDS_ParseSpecialComments`.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure, used for processing comments.
 * @param[in,out] s        Pointer to a pointer to a character array where the read line will be stored. This buffer may be resized if necessary.
 * @param[in,out] slen     Pointer to an `int32_t` variable specifying the current size of the buffer `s`. This value may be updated if the buffer is resized.
 * @param[in]  fp           Pointer to the `FILE` stream to read from.
 * @param[in]  skip_char    Character indicating the start of a comment line. Lines beginning with this character will be skipped.
 *
 * @return On success, returns the pointer `*s` containing the read line. If the end of the file is reached or an error occurs, returns `NULL`.
 *
 * @note The caller is responsible for managing the memory of the buffer `*s`, including freeing it when no longer needed.
 *
 * @see SDDS_CutOutComments
 * @see SDDS_ParseSpecialComments
 * @see SDDS_Realloc
 */
char *fgetsSkipCommentsResize(SDDS_DATASET *SDDS_dataset, char **s, int32_t *slen, FILE *fp, char skip_char /* ignore lines that begin with this character */) {
  int32_t spaceLeft, length, newLine;
  char *sInsert, *fgetsReturn;

  sInsert = *s;
  spaceLeft = *slen;
  newLine = 1;
  while ((fgetsReturn = fgets(sInsert, spaceLeft, fp))) {
    if (newLine && sInsert[0] == '!')
      continue;
    SDDS_CutOutComments(SDDS_dataset, sInsert, skip_char);
    length = strlen(sInsert);
    if (sInsert[length - 1] != '\n' && !feof(fp)) {
      /* buffer wasn't long enough to get the whole line.  Resize and add more data. */
      spaceLeft = *slen;
      *slen = *slen * 2;
      *s = SDDS_Realloc(*s, sizeof(**s) * *slen);
      sInsert = *s + strlen(*s);
      newLine = 0;
    } else
      break;
  }
  if (!fgetsReturn)
    return NULL;
  return (*s);
}

/**
 * @brief Reads a line from a LZMA-compressed file while skipping comment lines.
 *
 * This function reads lines from the specified LZMA-compressed file stream, ignoring lines that begin with the specified `skip_char`. It also processes special comment lines that start with `!#` by parsing them using `SDDS_ParseSpecialComments`.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure, used for processing comments.
 * @param[out] s            Pointer to a character array where the read line will be stored.
 * @param[in]  slen         The maximum number of characters to read into `s`.
 * @param[in]  lzmafp       Pointer to the `lzmafile` structure representing the LZMA-compressed file stream.
 * @param[in]  skip_char    Character indicating the start of a comment line. Lines beginning with this character will be skipped.
 *
 * @return On success, returns the pointer `s` containing the read line. If the end of the file is reached or an error occurs, returns `NULL`.
 *
 * @note The function modifies the buffer `s` by removing comments as determined by `SDDS_CutOutComments`.
 *
 * @see SDDS_CutOutComments
 * @see SDDS_ParseSpecialComments
 * @see lzma_gets
 */
char *fgetsLZMASkipComments(SDDS_DATASET *SDDS_dataset, char *s, int32_t slen, struct lzmafile *lzmafp, char skip_char /* ignore lines that begin with this character */) {
  while (lzma_gets(s, slen, lzmafp)) {
    if (s[0] != skip_char) {
      SDDS_CutOutComments(SDDS_dataset, s, skip_char);
      return (s);
    } else if (s[1] == '#') {
      SDDS_ParseSpecialComments(SDDS_dataset, s + 2);
    }
  }
  return (NULL);
}

/**
 * @brief Reads a line from a LZMA-compressed file with dynamic buffer resizing while skipping comment lines.
 *
 * This function reads lines from the specified LZMA-compressed file stream, ignoring lines that begin with the specified `skip_char`. If a line exceeds the current buffer size, the buffer is dynamically resized to accommodate the entire line. It also processes special comment lines that start with `!#` by parsing them using `SDDS_ParseSpecialComments`.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure, used for processing comments.
 * @param[in,out] s        Pointer to a pointer to a character array where the read line will be stored. This buffer may be resized if necessary.
 * @param[in,out] slen     Pointer to an `int32_t` variable specifying the current size of the buffer `*s`. This value may be updated if the buffer is resized.
 * @param[in]  lzmafp       Pointer to the `lzmafile` structure representing the LZMA-compressed file stream.
 * @param[in]  skip_char    Character indicating the start of a comment line. Lines beginning with this character will be skipped.
 *
 * @return On success, returns the pointer `*s` containing the read line. If the end of the file is reached or an error occurs, returns `NULL`.
 *
 * @note The caller is responsible for managing the memory of the buffer `*s`, including freeing it when no longer needed.
 *
 * @see SDDS_CutOutComments
 * @see SDDS_ParseSpecialComments
 * @see SDDS_Realloc
 * @see lzma_gets
 */
char *fgetsLZMASkipCommentsResize(SDDS_DATASET *SDDS_dataset, char **s, int32_t *slen, struct lzmafile *lzmafp, char skip_char /* ignore lines that begin with this character */) {
  int32_t spaceLeft, length, newLine;
  char *sInsert, *fgetsReturn;

  sInsert = *s;
  spaceLeft = *slen;
  newLine = 1;
  while ((fgetsReturn = lzma_gets(sInsert, spaceLeft, lzmafp))) {
    if (newLine && sInsert[0] == '!')
      continue;
    SDDS_CutOutComments(SDDS_dataset, sInsert, skip_char);
    length = strlen(sInsert);
    if (sInsert[length - 1] != '\n' && !lzma_eof(lzmafp)) {
      /* buffer wasn't long enough to get the whole line.  Resize and add more data. */
      spaceLeft = *slen;
      *slen = *slen * 2;
      *s = SDDS_Realloc(*s, sizeof(**s) * *slen);
      sInsert = *s + strlen(*s);
      newLine = 0;
    } else
      break;
  }
  if (!fgetsReturn)
    return NULL;
  return (*s);
}

#if defined(zLib)
/**
 * @brief Reads a line from a GZip-compressed file while skipping comment lines.
 *
 * This function reads lines from a GZip-compressed file stream (`gzfp`), ignoring lines that begin with the specified `skip_char`. It also processes special comment lines that start with `!#` by parsing them using `SDDS_ParseSpecialComments`. Lines that are not skipped have their comments removed using `SDDS_CutOutComments` before being returned.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure used for processing special comments.
 * @param[out] s            Pointer to a character array where the read line will be stored.
 * @param[in]  slen         The maximum number of characters to read into `s`, including the null terminator.
 * @param[in]  gzfp         GZip file pointer from which to read the line.
 * @param[in]  skip_char    Character indicating the start of a comment line. Lines beginning with this character will be skipped.
 *
 * @return On success, returns the pointer `s` containing the read line. If the end of the file is reached or an error occurs, returns `NULL`.
 *
 * @note This function modifies the buffer `s` by removing comments as determined by `SDDS_CutOutComments`.
 *
 * @see SDDS_CutOutComments
 * @see SDDS_ParseSpecialComments
 * @see gzgets
 */
char *fgetsGZipSkipComments(SDDS_DATASET *SDDS_dataset, char *s, int32_t slen, gzFile gzfp, char skip_char /* ignore lines that begin with this character */) {
  while (gzgets(gzfp, s, slen)) {
    if (s[0] != skip_char) {
      SDDS_CutOutComments(SDDS_dataset, s, skip_char);
      return (s);
    } else if (s[1] == '#') {
      SDDS_ParseSpecialComments(SDDS_dataset, s + 2);
    }
  }
  return (NULL);
}

/**
 * @brief Reads a line from a GZip-compressed file with dynamic buffer resizing while skipping comment lines.
 *
 * This function reads lines from a GZip-compressed file stream (`gzfp`), ignoring lines that begin with the specified `skip_char`. If a line exceeds the current buffer size (`slen`), the buffer is dynamically resized to accommodate the entire line. It also processes special comment lines that start with `!#` by parsing them using `SDDS_ParseSpecialComments`. Lines that are not skipped have their comments removed using `SDDS_CutOutComments` before being returned.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure used for processing special comments.
 * @param[in,out] s        Pointer to a pointer to a character array where the read line will be stored. This buffer may be resized if necessary.
 * @param[in,out] slen     Pointer to an `int32_t` variable specifying the current size of the buffer `*s`. This value may be updated if the buffer is resized.
 * @param[in]  gzfp         GZip file pointer from which to read the line.
 * @param[in]  skip_char    Character indicating the start of a comment line. Lines beginning with this character will be skipped.
 *
 * @return On success, returns the pointer `*s` containing the read line. If the end of the file is reached or an error occurs, returns `NULL`.
 *
 * @note The caller is responsible for managing the memory of the buffer `*s`, including freeing it when no longer needed.
 *
 * @see SDDS_CutOutComments
 * @see SDDS_ParseSpecialComments
 * @see SDDS_Realloc
 * @see gzgets
 */
char *fgetsGZipSkipCommentsResize(SDDS_DATASET *SDDS_dataset, char **s, int32_t *slen, gzFile gzfp, char skip_char /* ignore lines that begin with this character */) {
  int32_t spaceLeft, length, newLine;
  char *sInsert, *fgetsReturn;

  sInsert = *s;
  spaceLeft = *slen;
  newLine = 1;
  while ((fgetsReturn = gzgets(gzfp, sInsert, spaceLeft))) {
    if (newLine && sInsert[0] == '!')
      continue;
    SDDS_CutOutComments(SDDS_dataset, sInsert, skip_char);
    length = strlen(sInsert);
    if (sInsert[length - 1] != '\n' && !gzeof(gzfp)) {
      /* buffer wasn't int32_t enough to get the whole line.  Resize and add more data. */
      spaceLeft = *slen;
      *slen = *slen * 2;
      *s = SDDS_Realloc(*s, sizeof(**s) * *slen);
      sInsert = *s + strlen(*s);
      newLine = 0;
    } else
      break;
  }
  if (!fgetsReturn)
    return NULL;
  return (*s);
}
#endif

/**
 * @brief Removes comments from a string based on a specified comment character.
 *
 * This function processes a string `s`, removing any content that follows the comment character `cc`. It also handles special comment lines that start with `!#` by parsing them using `SDDS_ParseSpecialComments`. The function ensures that quoted sections within the string are preserved and not mistakenly identified as comments.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure used for processing special comments.
 * @param[in,out] s         Pointer to the character array containing the string to process. The string will be modified in place.
 * @param[in]  cc           The comment character indicating the start of a comment.
 *
 * @note If the first character of the string is the comment character, the entire line is treated as a comment. Otherwise, only the portion of the string following the first unescaped comment character is removed.
 *
 * @see SDDS_ParseSpecialComments
 */
void SDDS_CutOutComments(SDDS_DATASET *SDDS_dataset, char *s, char cc) {
  int32_t length, hasNewline;
  char *s0;

  if (!cc || !s)
    return;

  hasNewline = 0;
  length = strlen(s);
  if (s[length - 1] == '\n')
    hasNewline = 1;

  if (*s == cc) {
    /* check for special information */
    if (*(s + 1) == '#')
      SDDS_ParseSpecialComments(SDDS_dataset, s + 2);
    *s = 0;
    return;
  }
  s0 = s;
  while (*s) {
    if ((*s == '"') && (s == s0 || *(s - 1) != '\\')) {
      while (*++s && (*s != '"' || *(s - 1) == '\\'))
        ;
      if (!*s)
        return;
      s++;
      continue;
    }
    if (*s == cc) {
      if (s != s0 && *(s - 1) == '\\')
        strcpy_ss(s - 1, s);
      else {
        if (hasNewline) {
          *s = '\n';
          *(s + 1) = 0;
        } else
          *s = 0;
        return;
      }
    }
    s++;
  }
}

/**
 * @brief Extracts the next token from a string, handling quoted substrings and escape characters.
 *
 * This function parses the input string `s` to extract the next token, considering quoted substrings and escape characters. If the token is enclosed in double quotes (`"`), the function ensures that embedded quotes are handled correctly. After extracting the token, the original string `s` is updated to remove the extracted portion.
 *
 * @param[in,out] s        Pointer to the string from which to extract the token. This string will be modified to remove the extracted token.
 * @param[out]    buffer   Pointer to a character array where the extracted token will be stored.
 * @param[in]     buflen   The maximum number of characters to copy into `buffer`, including the null terminator.
 *
 * @return On success, returns the length of the extracted token as an `int32_t`. If no token is found or an error occurs (e.g., buffer overflow), returns `-1`.
 *
 * @note The function assumes that the input string `s` is null-terminated. The caller must ensure that `buffer` has sufficient space to hold the extracted token.
 *
 * @see SDDS_GetToken2
 */
int32_t SDDS_GetToken(char *s, char *buffer, int32_t buflen) {
  char *ptr0, *ptr1, *escptr, *temp;

  /* save the pointer to the head of the string */
  ptr0 = s;

  /* skip leading white-space */
  while (isspace(*s))
    s++;
  if (*s == 0)
    return (-1);
  ptr1 = s;

  if (*s == '"') {
    /* if quoted string, skip to next quotation mark */
    ptr1 = s + 1; /* beginning of actual token */
    do {
      s++;
      escptr = NULL;
      if (*s == '\\' && *(s + 1) == '\\') {
        /* skip and remember literal \ (indicated by \\ in the string) */
        escptr = s + 1;
        s += 2;
      }
    } while (*s && (*s != '"' || (*(s - 1) == '\\' && (s - 1) != escptr)));
    /* replace trailing quotation mark with a space */
    if (*s == '"')
      *s = ' ';
  } else {
    /* skip to first white-space following token */
    do {
      s++;
      /* imbedded quotation marks are handled here */
      if (*s == '"' && *(s - 1) != '\\') {
        while (*++s && !(*s == '"' && *(s - 1) != '\\'))
          ;
      }
    } while (*s && !isspace(*s));
  }

  if ((int32_t)(s - ptr1) >= buflen)
    return (-1);
  strncpy(buffer, ptr1, s - ptr1);
  buffer[s - ptr1] = 0;

  /* update the original string to delete the token */
  temp = malloc(sizeof(char) * (strlen(s) + 1));
  strcpy(temp, s);
  strcpy(ptr0, temp);
  free(temp);

  /* return the string length */
  return ((int32_t)(s - ptr1));
}

/**
 * @brief Extracts the next token from a string, handling quoted substrings and escape characters, with updated string pointers.
 *
 * This function parses the input string `s` to extract the next token, considering quoted substrings and escape characters. If the token is enclosed in double quotes (`"`), the function ensures that embedded quotes are handled correctly. After extracting the token, the original string `s` is updated by adjusting the string pointer `st` and the remaining string length `strlength`.
 *
 * @param[in,out] s           Pointer to the string from which to extract the token. This string will be modified to remove the extracted token.
 * @param[in,out] st          Pointer to the current position in the string `s`. This will be updated to point to the next character after the extracted token.
 * @param[in,out] strlength   Pointer to an `int32_t` variable representing the remaining length of the string `s`. This will be decremented by the length of the extracted token.
 * @param[out]    buffer      Pointer to a character array where the extracted token will be stored.
 * @param[in]     buflen      The maximum number of characters to copy into `buffer`, including the null terminator.
 *
 * @return On success, returns the length of the extracted token as an `int32_t`. If no token is found or an error occurs (e.g., buffer overflow), returns `-1`.
 *
 * @note The caller is responsible for ensuring that `buffer` has sufficient space to hold the extracted token. Additionally, `st` and `strlength` should accurately reflect the current parsing state of the string.
 *
 * @see SDDS_GetToken
 */
int32_t SDDS_GetToken2(char *s, char **st, int32_t *strlength, char *buffer, int32_t buflen) {
  char *ptr0, *ptr1, *escptr;

  /* save the pointer to the head of the string */
  ptr0 = s;

  /* skip leading white-space */
  while (isspace(*s))
    s++;
  if (*s == 0)
    return (-1);
  ptr1 = s;

  if (*s == '"') {
    /* if quoted string, skip to next quotation mark */
    ptr1 = s + 1; /* beginning of actual token */
    do {
      s++;
      escptr = NULL;
      if (*s == '\\' && *(s + 1) == '\\') {
        /* skip and remember literal \ (indicated by \\ in the string) */
        escptr = s + 1;
        s += 2;
      }
    } while (*s && (*s != '"' || (*(s - 1) == '\\' && (s - 1) != escptr)));
    /* replace trailing quotation mark with a space */
    if (*s == '"')
      *s = ' ';
  } else {
    /* skip to first white-space following token */
    do {
      s++;
      /* imbedded quotation marks are handled here */
      if (*s == '"' && *(s - 1) != '\\') {
        while (*++s && !(*s == '"' && *(s - 1) != '\\'))
          ;
      }
    } while (*s && !isspace(*s));
  }

  if ((int32_t)(s - ptr1) >= buflen)
    return (-1);
  strncpy(buffer, ptr1, s - ptr1);
  buffer[s - ptr1] = 0;

  /* update the original string to delete the token */
  *st += s - ptr0;
  *strlength -= s - ptr0;

  /* return the string length including whitespace */
  return ((int32_t)(s - ptr1));
}

/**
 * @brief Pads a string with spaces to reach a specified length.
 *
 * This function appends space characters to the end of the input string `string` until it reaches the desired `length`. If the original string is longer than the specified `length`, the function returns an error without modifying the string.
 *
 * @param[in,out] string Pointer to the null-terminated string to be padded.
 * @param[in]     length The target length for the string after padding.
 *
 * @return Returns `1` on successful padding. Returns `0` if the input string is `NULL` or if the original string length exceeds the specified `length`.
 *
 * @note The function ensures that the padded string is null-terminated. The caller must ensure that the buffer `string` has sufficient space to accommodate the additional padding.
 *
 * @see SDDS_RemovePadding
 */
int32_t SDDS_PadToLength(char *string, int32_t length) {
  int32_t i;
  if (!string || (i = strlen(string)) > length)
    return (0);
  while (i < length)
    string[i++] = ' ';
  string[i] = 0;
  return (1);
}

/**
 * @brief Escapes quote characters within a string by inserting backslashes.
 *
 * This function scans the input string `s` and inserts a backslash (`\`) before each occurrence of the specified `quote_char`, provided it is not already escaped. This is useful for preparing strings for formats that require escaped quotes.
 *
 * @param[in,out] s           Pointer to the string in which quotes will be escaped. The string will be modified in place.
 * @param[in]     quote_char  The quote character to escape (e.g., `"`).
 *
 * @note The function dynamically allocates a temporary buffer to perform the escaping process and ensures that the original string `s` is updated correctly. The caller must ensure that `s` has sufficient space to accommodate the additional backslashes.
 *
 * @see SDDS_UnescapeQuotes
 */
void SDDS_EscapeQuotes(char *s, char quote_char) {
  char *ptr, *bptr;
  char *buffer = NULL;

  ptr = s;
  buffer = trealloc(buffer, sizeof(*buffer) * (4 * (strlen(s) + 1)));
  bptr = buffer;

  while (*ptr) {
    if (*ptr == quote_char && (ptr == s || *(ptr - 1) != '\\'))
      *bptr++ = '\\';
    *bptr++ = *ptr++;
  }
  *bptr = 0;
  strcpy(s, buffer);
  if (buffer)
    free(buffer);
}

/**
 * @brief Removes escape characters from quote characters within a string.
 *
 * This function scans the input string `s` and removes backslashes (`\`) that precede the specified `quote_char`, effectively unescaping the quotes. This is useful for processing strings that have been prepared with escaped quotes.
 *
 * @param[in,out] s           Pointer to the string in which quotes will be unescaped. The string will be modified in place.
 * @param[in]     quote_char  The quote character to unescape (e.g., `"`).
 *
 * @note The function modifies the string `s` by shifting characters to remove the escape backslashes. It assumes that `s` is properly null-terminated.
 *
 * @see SDDS_EscapeQuotes
 */
void SDDS_UnescapeQuotes(char *s, char quote_char) {
  char *ptr;
  ptr = s;
  while (*ptr) {
    if (*ptr == quote_char && ptr != s && *(ptr - 1) == '\\')
      strcpy(ptr - 1, ptr);
    else
      ptr++;
  }
}

/**
 * @brief Escapes comment characters within a string by inserting backslashes.
 *
 * This function scans the input string `string` and inserts a backslash (`\`) before each occurrence of the specified comment character `cc`, provided it is not already escaped. This is useful for preparing strings to include comment characters without them being interpreted as actual comments.
 *
 * @param[in,out] string Pointer to the string in which comment characters will be escaped. The string will be modified in place.
 * @param[in]     cc     The comment character to escape (e.g., `#`).
 *
 * @note The function dynamically allocates a temporary buffer to perform the escaping process and ensures that the original string `string` is updated correctly. The caller must ensure that `string` has sufficient space to accommodate the additional backslashes.
 *
 * @see SDDS_CutOutComments
 */
void SDDS_EscapeCommentCharacters(char *string, char cc) {
  char *ptr, *s0;
  s0 = string;
  while (*string) {
    if (*string == cc && (string == s0 || *(string - 1) != '\\')) {
      ptr = string + strlen(string) + 1;
      while (ptr != string) {
        *ptr = *(ptr - 1);
        ptr--;
      }
      *string++ = '\\';
    }
    string++;
  }
}

/**
 * @brief Sets a block of memory to zero.
 *
 * This function zero-initializes a specified number of bytes in a memory block. It is a wrapper around the standard `memset` function, providing a convenient way to clear memory.
 *
 * @param[in,out] mem      Pointer to the memory block to be zeroed.
 * @param[in]     n_bytes  The number of bytes to set to zero.
 *
 * @return Returns `1` on successful memory zeroing. Returns `0` if the input memory pointer `mem` is `NULL`.
 *
 * @note The function does not perform any bounds checking. It is the caller's responsibility to ensure that the memory block is large enough to accommodate `n_bytes`.
 *
 * @see memset
 */
int32_t SDDS_ZeroMemory(void *mem, int64_t n_bytes) {
  if (mem) {
    memset(mem, 0, n_bytes);
    return 1;
  }
  return 0;

  /*
    char *c;

    if (!(c = (char*)mem))
    return(0);
    while (n_bytes--)
    *c++ = 0;
    return(1);
  */
}

/**
 * @brief Initializes a memory block with a sequence of values based on a specified data type.
 *
 * This function sets a block of memory to a sequence of values, starting from a specified initial value and incrementing by a defined delta. The sequence is determined by the `data_type` parameter, which specifies the type of each element in the memory block. The function supports various SDDS data types and handles the initialization accordingly.
 *
 * @param[in,out] mem         Pointer to the memory block to be initialized.
 * @param[in]     n_elements  The number of elements to initialize in the memory block.
 * @param[in]     data_type   The SDDS data type of each element. Must be one of the following constants:
 *                              - `SDDS_SHORT`
 *                              - `SDDS_USHORT`
 *                              - `SDDS_LONG`
 *                              - `SDDS_ULONG`
 *                              - `SDDS_LONG64`
 *                              - `SDDS_ULONG64`
 *                              - `SDDS_FLOAT`
 *                              - `SDDS_DOUBLE`
 *                              - `SDDS_LONGDOUBLE`
 *                              - `SDDS_CHARACTER`
 * @param[in]     ...         Variable arguments specifying the starting value and increment value. The types of these arguments depend on the `data_type`:
 *                              - For integer types (`SDDS_SHORT`, `SDDS_USHORT`, `SDDS_LONG`, `SDDS_ULONG`, `SDDS_LONG64`, `SDDS_ULONG64`):
 *                                - First argument: initial value (`int`, `unsigned int`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`)
 *                                - Second argument: increment value (`int`, `unsigned int`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`)
 *                              - For floating-point types (`SDDS_FLOAT`, `SDDS_DOUBLE`, `SDDS_LONGDOUBLE`):
 *                                - First argument: initial value (`double` for `float` and `double`, `long double` for `SDDS_LONGDOUBLE`)
 *                                - Second argument: increment value (`double` for `float` and `double`, `long double` for `SDDS_LONGDOUBLE`)
 *                              - For `SDDS_CHARACTER`:
 *                                - First argument: initial value (`char`)
 *                                - Second argument: increment value (`short`)
 *
 * @return Returns `1` on successful memory initialization. Returns `0` if an unknown or invalid `data_type` is provided.
 *
 * @note The function uses variable arguments to accept the starting and increment values. The caller must ensure that the correct types are provided based on the `data_type` parameter.
 *
 * @see SDDS_Malloc
 * @see SDDS_Free
 */
int32_t SDDS_SetMemory(void *mem, int64_t n_elements, int32_t data_type, ...)
/* usage is SDDS_SetMemory(ptr, n_elements, type, start_value, increment_value) */
{
  va_list argptr;
  int32_t retval;
  int64_t i;
  short short_val, short_dval, *short_ptr;
  unsigned short ushort_val, ushort_dval, *ushort_ptr;
  int32_t long_val, long_dval, *long_ptr;
  uint32_t ulong_val, ulong_dval, *ulong_ptr;
  int64_t long64_val, long64_dval, *long64_ptr;
  uint64_t ulong64_val, ulong64_dval, *ulong64_ptr;
  float float_val, float_dval, *float_ptr;
  double double_val, double_dval, *double_ptr;
  long double longdouble_val, longdouble_dval, *longdouble_ptr;
  char char_val, *char_ptr;

  retval = 1;
  va_start(argptr, data_type);
  switch (data_type) {
  case SDDS_SHORT:
    short_val = (short)va_arg(argptr, int);
    short_dval = (short)va_arg(argptr, int);
    short_ptr = (short *)mem;
    for (i = 0; i < n_elements; i++, short_val += short_dval)
      *short_ptr++ = short_val;
    break;
  case SDDS_USHORT:
    ushort_val = (unsigned short)va_arg(argptr, int);
    ushort_dval = (unsigned short)va_arg(argptr, int);
    ushort_ptr = (unsigned short *)mem;
    for (i = 0; i < n_elements; i++, ushort_val += ushort_dval)
      *ushort_ptr++ = ushort_val;
    break;
  case SDDS_LONG:
    long_val = (int32_t)va_arg(argptr, int32_t);
    long_dval = (int32_t)va_arg(argptr, int32_t);
    long_ptr = (int32_t *)mem;
    for (i = 0; i < n_elements; i++, long_val += long_dval)
      *long_ptr++ = long_val;
    break;
  case SDDS_ULONG:
    ulong_val = (uint32_t)va_arg(argptr, uint32_t);
    ulong_dval = (uint32_t)va_arg(argptr, uint32_t);
    ulong_ptr = (uint32_t *)mem;
    for (i = 0; i < n_elements; i++, ulong_val += ulong_dval)
      *ulong_ptr++ = ulong_val;
    break;
  case SDDS_LONG64:
    long64_val = (int64_t)va_arg(argptr, int64_t);
    long64_dval = (int64_t)va_arg(argptr, int64_t);
    long64_ptr = (int64_t *)mem;
    for (i = 0; i < n_elements; i++, long64_val += long64_dval)
      *long64_ptr++ = long64_val;
    break;
  case SDDS_ULONG64:
    ulong64_val = (uint64_t)va_arg(argptr, uint32_t);
    ulong64_dval = (uint64_t)va_arg(argptr, uint32_t);
    ulong64_ptr = (uint64_t *)mem;
    for (i = 0; i < n_elements; i++, ulong64_val += ulong64_dval)
      *ulong64_ptr++ = ulong64_val;
    break;
  case SDDS_FLOAT:
    float_val = (float)va_arg(argptr, double);
    float_dval = (float)va_arg(argptr, double);
    float_ptr = (float *)mem;
    for (i = 0; i < n_elements; i++, float_val += float_dval)
      *float_ptr++ = float_val;
    break;
  case SDDS_DOUBLE:
    double_val = (double)va_arg(argptr, double);
    double_dval = (double)va_arg(argptr, double);
    double_ptr = (double *)mem;
    for (i = 0; i < n_elements; i++, double_val += double_dval)
      *double_ptr++ = double_val;
    break;
  case SDDS_LONGDOUBLE:
    longdouble_val = (long double)va_arg(argptr, long double);
    longdouble_dval = (long double)va_arg(argptr, long double);
    longdouble_ptr = (long double *)mem;
    for (i = 0; i < n_elements; i++, longdouble_val += longdouble_dval)
      *longdouble_ptr++ = longdouble_val;
    break;
  case SDDS_CHARACTER:
    char_val = (char)va_arg(argptr, int);
    short_dval = (short)va_arg(argptr, int);
    char_ptr = (char *)mem;
    for (i = 0; i < n_elements; i++, char_val += short_dval)
      *char_ptr++ = char_val;
    break;
  default:
    SDDS_SetError("Unable to set memory--unknown or invalid data type (SDDS_SetMemory)");
    retval = 0;
    break;
  }
  va_end(argptr);
  return (retval);
}

/**
 * @brief Retrieves the data type of a column in the SDDS dataset by its index.
 *
 * This function returns the SDDS data type of the specified column within the dataset. The data type corresponds to one of the predefined SDDS type constants, such as `SDDS_LONGDOUBLE`, `SDDS_DOUBLE`, `SDDS_FLOAT`, etc.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  index        The zero-based index of the column whose data type is to be retrieved. The index should be obtained from `SDDS_DefineColumn` or `SDDS_GetColumnIndex`.
 *
 * @return On success, returns the SDDS data type of the column as an `int32_t`. On failure (e.g., if the index is out of range or the dataset is invalid), returns `0` and records an error message.
 *
 * @note The function does not perform type validation beyond checking the index range. It assumes that the dataset's column definitions are correctly initialized.
 *
 * @see SDDS_GetColumnIndex
 * @see SDDS_SetError
 */
int32_t SDDS_GetColumnType(SDDS_DATASET *SDDS_dataset, int32_t index) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnType"))
    return (0);
  if (index < 0 || index >= SDDS_dataset->layout.n_columns) {
    SDDS_SetError("Unable to get column type--column index is out of range (SDDS_GetColumnType)");
    return (0);
  }
  return (SDDS_dataset->layout.column_definition[index].type);
}

/**
 * @brief Retrieves the data type of a column in the SDDS dataset by its name.
 *
 * This function searches for a column by its name within the dataset and returns its SDDS data type. The data type corresponds to one of the predefined SDDS type constants, such as `SDDS_LONGDOUBLE`, `SDDS_DOUBLE`, `SDDS_FLOAT`, etc.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the column whose data type is to be retrieved.
 *
 * @return On success, returns the SDDS data type of the column as an `int32_t`. On failure (e.g., if the column name is not found or the dataset is invalid), returns `0` and records an error message.
 *
 * @note The function internally uses `SDDS_GetColumnIndex` to find the column's index before retrieving its type.
 *
 * @see SDDS_GetColumnIndex
 * @see SDDS_SetError
 */
int32_t SDDS_GetNamedColumnType(SDDS_DATASET *SDDS_dataset, char *name) {
  int64_t index;
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0 || index >= SDDS_dataset->layout.n_columns) {
    SDDS_SetError("Unable to get column type--column index is out of range (SDDS_GetNamedColumnType)");
    return (0);
  }
  return (SDDS_dataset->layout.column_definition[index].type);
}

/**
 * @brief Retrieves the data type of an array in the SDDS dataset by its index.
 *
 * This function returns the SDDS data type of the specified array within the dataset. The data type corresponds to one of the predefined SDDS type constants, such as `SDDS_LONGDOUBLE`, `SDDS_DOUBLE`, `SDDS_FLOAT`, etc.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  index        The zero-based index of the array whose data type is to be retrieved. The index should be obtained from `SDDS_DefineArray` or `SDDS_GetArrayIndex`.
 *
 * @return On success, returns the SDDS data type of the array as an `int32_t`. On failure (e.g., if the index is out of range or the dataset is invalid), returns `0` and records an error message.
 *
 * @note The function does not perform type validation beyond checking the index range. It assumes that the dataset's array definitions are correctly initialized.
 *
 * @see SDDS_GetArrayIndex
 * @see SDDS_SetError
 */
int32_t SDDS_GetArrayType(SDDS_DATASET *SDDS_dataset, int32_t index) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetArrayType"))
    return (0);
  if (index < 0 || index >= SDDS_dataset->layout.n_arrays) {
    SDDS_SetError("Unable to get array type--array index is out of range (SDDS_GetArrayType)");
    return (0);
  }
  return (SDDS_dataset->layout.array_definition[index].type);
}

/**
 * @brief Retrieves the data type of an array in the SDDS dataset by its name.
 *
 * This function searches for an array by its name within the dataset and returns its SDDS data type. The data type corresponds to one of the predefined SDDS type constants, such as `SDDS_LONGDOUBLE`, `SDDS_DOUBLE`, `SDDS_FLOAT`, etc.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the array whose data type is to be retrieved.
 *
 * @return On success, returns the SDDS data type of the array as an `int32_t`. On failure (e.g., if the array name is not found or the dataset is invalid), returns `0` and records an error message.
 *
 * @note The function internally uses `SDDS_GetArrayIndex` to find the array's index before retrieving its type.
 *
 * @see SDDS_GetArrayIndex
 * @see SDDS_SetError
 */
int32_t SDDS_GetNamedArrayType(SDDS_DATASET *SDDS_dataset, char *name) {
  int32_t index;
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, name)) < 0 || index >= SDDS_dataset->layout.n_arrays) {
    SDDS_SetError("Unable to get array type--array index is out of range (SDDS_GetNamedArrayType)");
    return (0);
  }
  return (SDDS_dataset->layout.array_definition[index].type);
}

/**
 * @brief Retrieves the data type of a parameter in the SDDS dataset by its index.
 *
 * This function returns the SDDS data type of the specified parameter within the dataset. The data type corresponds to one of the predefined SDDS type constants, such as `SDDS_LONGDOUBLE`, `SDDS_DOUBLE`, `SDDS_FLOAT`, etc.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  index        The zero-based index of the parameter whose data type is to be retrieved. The index should be obtained from `SDDS_DefineParameter` or `SDDS_GetParameterIndex`.
 *
 * @return On success, returns the SDDS data type of the parameter as an `int32_t`. On failure (e.g., if the index is out of range or the dataset is invalid), returns `0` and records an error message.
 *
 * @note The function does not perform type validation beyond checking the index range. It assumes that the dataset's parameter definitions are correctly initialized.
 *
 * @see SDDS_GetParameterIndex
 * @see SDDS_SetError
 */
int32_t SDDS_GetParameterType(SDDS_DATASET *SDDS_dataset, int32_t index) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterType"))
    return (0);
  if (index < 0 || index >= SDDS_dataset->layout.n_parameters) {
    SDDS_SetError("Unable to get parameter type--parameter index is out of range (SDDS_GetParameterType)");
    return (0);
  }
  return (SDDS_dataset->layout.parameter_definition[index].type);
}

/**
 * @brief Retrieves the data type of a parameter in the SDDS dataset by its name.
 *
 * This function searches for a parameter by its name within the dataset and returns its SDDS data type. The data type corresponds to one of the predefined SDDS type constants, such as `SDDS_LONGDOUBLE`, `SDDS_DOUBLE`, `SDDS_FLOAT`, etc.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A null-terminated string specifying the name of the parameter whose data type is to be retrieved.
 *
 * @return On success, returns the SDDS data type of the parameter as an `int32_t`. On failure (e.g., if the parameter name is not found or the dataset is invalid), returns `0` and records an error message.
 *
 * @note The function internally uses `SDDS_GetParameterIndex` to find the parameter's index before retrieving its type.
 *
 * @see SDDS_GetParameterIndex
 * @see SDDS_SetError
 */
int32_t SDDS_GetNamedParameterType(SDDS_DATASET *SDDS_dataset, char *name) {
  int32_t index;
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, name)) < 0 || index >= SDDS_dataset->layout.n_parameters) {
    SDDS_SetError("Unable to get parameter type--parameter index is out of range (SDDS_GetNamedParameterType)");
    return (0);
  }
  return (SDDS_dataset->layout.parameter_definition[index].type);
}

/**
 * @brief Retrieves the size in bytes of a specified SDDS data type.
 *
 * This function returns the size, in bytes, of the specified SDDS data type. The size corresponds to the memory footprint of the data type when stored in the dataset.
 *
 * @param[in] type The SDDS data type for which the size is requested. Must be one of the predefined constants:
 *                 - `SDDS_LONGDOUBLE`
 *                 - `SDDS_DOUBLE`
 *                 - `SDDS_FLOAT`
 *                 - `SDDS_LONG`
 *                 - `SDDS_ULONG`
 *                 - `SDDS_SHORT`
 *                 - `SDDS_USHORT`
 *                 - `SDDS_CHARACTER`
 *                 - `SDDS_STRING`
 *
 * @return On success, returns a positive integer representing the size of the data type in bytes. On failure (e.g., if the type is invalid), returns `-1` and records an error message.
 *
 * @note The function relies on the `SDDS_type_size` array, which should be properly initialized with the sizes of all supported SDDS data types.
 *
 * @see SDDS_GetTypeName
 * @see SDDS_SetError
 */
int32_t SDDS_GetTypeSize(int32_t type) {
  if (!SDDS_VALID_TYPE(type))
    return (-1);
  return (SDDS_type_size[type - 1]);
}

/**
 * @brief Retrieves the name of a specified SDDS data type as a string.
 *
 * This function returns a dynamically allocated string containing the name of the specified SDDS data type. The name corresponds to the textual representation of the data type, such as `"double"`, `"float"`, `"int32"`, etc.
 *
 * @param[in] type The SDDS data type for which the name is requested. Must be one of the predefined constants:
 *                 - `SDDS_LONGDOUBLE`
 *                 - `SDDS_DOUBLE`
 *                 - `SDDS_FLOAT`
 *                 - `SDDS_LONG`
 *                 - `SDDS_ULONG`
 *                 - `SDDS_SHORT`
 *                 - `SDDS_USHORT`
 *                 - `SDDS_CHARACTER`
 *                 - `SDDS_STRING`
 *
 * @return On success, returns a pointer to a newly allocated string containing the name of the data type. On failure (e.g., if the type is invalid or memory allocation fails), returns `NULL`.
 *
 * @note The caller is responsible for freeing the memory allocated for the returned string using `SDDS_Free`.
 *
 * @see SDDS_GetTypeSize
 * @see SDDS_SetError
 */
char *SDDS_GetTypeName(int32_t type) {
  char *name;
  if (!SDDS_VALID_TYPE(type))
    return NULL;
  if (!SDDS_CopyString(&name, SDDS_type_name[type - 1]))
    return NULL;
  return name;
}

/**
 * @brief Identifies the SDDS data type based on its string name.
 *
 * This function searches for the SDDS data type that matches the provided string `typeName`. It returns the corresponding SDDS data type constant if a match is found.
 *
 * @param[in] typeName A null-terminated string representing the name of the SDDS data type to identify.
 *
 * @return On success, returns the SDDS data type constant (`int32_t`) corresponding to `typeName`. On failure (e.g., if `typeName` does not match any known data type), returns `0`.
 *
 * @note The function performs a case-sensitive comparison between `typeName` and the names of supported SDDS data types.
 *
 * @see SDDS_GetTypeName
 * @see SDDS_SetError
 */
int32_t SDDS_IdentifyType(char *typeName) {
  int32_t i;
  for (i = 0; i < SDDS_NUM_TYPES; i++)
    if (strcmp(typeName, SDDS_type_name[i]) == 0)
      return i + 1;
  return 0;
}

/**
 * @brief Removes leading and trailing whitespace from a string.
 *
 * This function trims all leading and trailing whitespace characters from the input string `s`. It modifies the string in place, ensuring that any padding spaces are removed while preserving the internal content.
 *
 * @param[in,out] s Pointer to the null-terminated string to be trimmed. The string will be modified in place.
 *
 * @note The function handles all standard whitespace characters as defined by the `isspace` function. If the string consists entirely of whitespace, the function will result in an empty string.
 *
 * @see isspace
 */
void SDDS_RemovePadding(char *s) {
  char *ptr;
  ptr = s;
  while (isspace(*ptr))
    ptr++;
  if (ptr != s)
    strcpy(s, ptr);
  ptr = s + strlen(s) - 1;
  while (isspace(*ptr))
    *ptr-- = 0;
}

/**
 * @brief Checks if a string is blank (contains only whitespace characters).
 *
 * This function determines whether the provided NULL-terminated string `s` consists solely of whitespace characters. If the string is `NULL` or contains only whitespace, the function returns `1`. If the string contains any non-whitespace characters, it returns `0`.
 *
 * @param[in]  s Pointer to the NULL-terminated string to be checked.
 *
 * @return 
 * - Returns `1` if the string is `NULL` or contains only whitespace characters.
 * - Returns `0` if the string contains any non-whitespace characters.
 *
 * @see isspace
 */
int32_t SDDS_StringIsBlank(char *s) {
  if (!s)
    return 1;
  while (*s)
    if (!isspace(*s++))
      return (0);
  return (1);
}

/**
 * @brief Determines if a specified column is marked as of interest in the dataset.
 *
 * This function checks whether the column with the given `name` is flagged as of interest within the provided `SDDS_dataset`. It verifies the dataset's validity and then iterates through the columns to find a match based on the `column_flag` array.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A NULL-terminated string specifying the name of the column to check.
 *
 * @return 
 * - Returns `1` if the column is marked as of interest.
 * - Returns `0` if the column is not marked as of interest or if `column_flag` is not set.
 * - Returns `-1` if the dataset is invalid.
 *
 * @see SDDS_CheckDataset
 */
int32_t SDDS_ColumnIsOfInterest(SDDS_DATASET *SDDS_dataset, char *name) {
  int64_t i;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ColumnIsOfInterest"))
    return -1;
  if (!SDDS_dataset->column_flag)
    return 0;
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
    if (SDDS_dataset->column_flag[i] && strcmp(name, SDDS_dataset->layout.column_definition[i].name) == 0)
      return 1;
  }
  return 0;
}

/**
 * @brief Retrieves the names of all columns in the SDDS dataset.
 *
 * This function allocates and returns an array of NULL-terminated strings containing the names of the columns in the provided `SDDS_dataset`. It only includes columns that are flagged as of interest if `column_flag` is set.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[out] number        Pointer to an `int32_t` variable where the number of retrieved column names will be stored.
 *
 * @return 
 * - Returns a pointer to an array of NULL-terminated strings containing the column names on success.
 * - Returns `NULL` on failure (e.g., if the dataset is invalid or memory allocation fails) and records an error message.
 *
 * @note The caller is responsible for freeing the memory allocated for the returned array and its strings using `SDDS_FreeStringArray` or similar functions.
 *
 * @see SDDS_CheckDataset
 * @see SDDS_Malloc
 * @see SDDS_CopyString
 * @see SDDS_SetError
 */
char **SDDS_GetColumnNames(SDDS_DATASET *SDDS_dataset, int32_t *number) {
  int64_t i;
  char **name;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetColumnNames"))
    return (NULL);
  *number = 0;
  if (!(name = (char **)SDDS_Malloc(sizeof(*name) * SDDS_dataset->layout.n_columns))) {
    SDDS_SetError("Unable to get column names--allocation failure (SDDS_GetColumnNames)");
    return (NULL);
  }
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
    if (!SDDS_dataset->column_flag || SDDS_dataset->column_flag[i]) {
      if (!SDDS_CopyString(name + *number, SDDS_dataset->layout.column_definition[i].name)) {
        free(name);
        return (NULL);
      }
      *number += 1;
    }
  }
  return (name);
}

/**
 * @brief Retrieves the names of all parameters in the SDDS dataset.
 *
 * This function allocates and returns an array of NULL-terminated strings containing the names of the parameters in the provided `SDDS_dataset`.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[out] number        Pointer to an `int32_t` variable where the number of retrieved parameter names will be stored.
 *
 * @return 
 * - Returns a pointer to an array of NULL-terminated strings containing the parameter names on success.
 * - Returns `NULL` on failure (e.g., if the dataset is invalid or memory allocation fails) and records an error message.
 *
 * @note The caller is responsible for freeing the memory allocated for the returned array and its strings using `SDDS_FreeStringArray` or similar functions.
 *
 * @see SDDS_CheckDataset
 * @see SDDS_Malloc
 * @see SDDS_CopyString
 * @see SDDS_SetError
 */
char **SDDS_GetParameterNames(SDDS_DATASET *SDDS_dataset, int32_t *number) {
  int32_t i;
  char **name;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterNames"))
    return (NULL);
  *number = SDDS_dataset->layout.n_parameters;
  if (!(name = (char **)SDDS_Malloc(sizeof(*name) * SDDS_dataset->layout.n_parameters))) {
    SDDS_SetError("Unable to get parameter names--allocation failure (SDDS_GetParameterNames)");
    return (NULL);
  }
  for (i = 0; i < SDDS_dataset->layout.n_parameters; i++) {
    if (!SDDS_CopyString(name + i, SDDS_dataset->layout.parameter_definition[i].name)) {
      free(name);
      return (NULL);
    }
  }
  return (name);
}

/**
 * @brief Retrieves the names of all arrays in the SDDS dataset.
 *
 * This function allocates and returns an array of NULL-terminated strings containing the names of the arrays in the provided `SDDS_dataset`.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[out] number        Pointer to an `int32_t` variable where the number of retrieved array names will be stored.
 *
 * @return 
 * - Returns a pointer to an array of NULL-terminated strings containing the array names on success.
 * - Returns `NULL` on failure (e.g., if the dataset is invalid or memory allocation fails) and records an error message.
 *
 * @note The caller is responsible for freeing the memory allocated for the returned array and its strings using `SDDS_FreeStringArray` or similar functions.
 *
 * @see SDDS_CheckDataset
 * @see SDDS_Malloc
 * @see SDDS_CopyString
 * @see SDDS_SetError
 */
char **SDDS_GetArrayNames(SDDS_DATASET *SDDS_dataset, int32_t *number) {
  int32_t i;
  char **name;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetArrayNames"))
    return (NULL);
  *number = SDDS_dataset->layout.n_arrays;
  if (!(name = (char **)SDDS_Malloc(sizeof(*name) * SDDS_dataset->layout.n_arrays))) {
    SDDS_SetError("Unable to get array names--allocation failure (SDDS_GetArrayNames)");
    return (NULL);
  }
  for (i = 0; i < SDDS_dataset->layout.n_arrays; i++) {
    if (!SDDS_CopyString(name + i, SDDS_dataset->layout.array_definition[i].name)) {
      free(name);
      return (NULL);
    }
  }
  return (name);
}

/**
 * @brief Retrieves the names of all associates in the SDDS dataset.
 *
 * This function allocates and returns an array of NULL-terminated strings containing the names of the associates in the provided `SDDS_dataset`.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[out] number        Pointer to an `int32_t` variable where the number of retrieved associate names will be stored.
 *
 * @return 
 * - Returns a pointer to an array of NULL-terminated strings containing the associate names on success.
 * - Returns `NULL` on failure (e.g., if the dataset is invalid or memory allocation fails) and records an error message.
 *
 * @note The caller is responsible for freeing the memory allocated for the returned array and its strings using `SDDS_FreeStringArray` or similar functions.
 *
 * @see SDDS_CheckDataset
 * @see SDDS_Malloc
 * @see SDDS_CopyString
 * @see SDDS_SetError
 */
char **SDDS_GetAssociateNames(SDDS_DATASET *SDDS_dataset, int32_t *number) {
  int32_t i;
  char **name;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetAssociateNames"))
    return (NULL);
  if (!(name = (char **)SDDS_Malloc(sizeof(*name) * SDDS_dataset->layout.n_associates))) {
    SDDS_SetError("Unable to get associate names--allocation failure (SDDS_GetAssociateNames)");
    return (NULL);
  }
  *number = SDDS_dataset->layout.n_associates;
  for (i = 0; i < SDDS_dataset->layout.n_associates; i++) {
    if (!SDDS_CopyString(name + i, SDDS_dataset->layout.associate_definition[i].name)) {
      free(name);
      return (NULL);
    }
  }
  return (name);
}

/**
 * @brief Casts a value from one SDDS data type to another.
 *
 * This function converts a value from its original SDDS data type (`data_type`) to a desired SDDS data type (`desired_type`). It retrieves the value at the specified `index` from the `data` array and stores the converted value in the provided `memory` location.
 *
 * @param[in]  data         Pointer to the data array containing the original values.
 * @param[in]  index        The zero-based index of the value to be casted within the `data` array.
 * @param[in]  data_type    The original SDDS data type of the value. Must be one of the SDDS type constants:
 *                           - `SDDS_SHORT`
 *                           - `SDDS_USHORT`
 *                           - `SDDS_LONG`
 *                           - `SDDS_ULONG`
 *                           - `SDDS_LONG64`
 *                           - `SDDS_ULONG64`
 *                           - `SDDS_CHARACTER`
 *                           - `SDDS_FLOAT`
 *                           - `SDDS_DOUBLE`
 *                           - `SDDS_LONGDOUBLE`
 * @param[in]  desired_type The desired SDDS data type to which the value should be casted. Must be one of the SDDS type constants listed above.
 * @param[out] memory       Pointer to the memory location where the casted value will be stored.
 *
 * @return 
 * - Returns a pointer to the `memory` location containing the casted value on success.
 * - Returns `NULL` if the casting fails due to invalid data types or other errors.
 *
 * @note 
 * - The function does not handle casting for `SDDS_STRING` types.
 * - The caller must ensure that the `memory` location has sufficient space to store the casted value.
 *
 * @see SDDS_CopyString
 * @see SDDS_SetError
 */
void *SDDS_CastValue(void *data, int64_t index, int32_t data_type, int32_t desired_type, void *memory) {
  long long integer_value;
  long double fp_value;
  if (!data || !memory || data_type == SDDS_STRING || desired_type == SDDS_STRING)
    return (NULL);
  if (data_type == desired_type) {
    memcpy(memory, (char *)data + SDDS_type_size[data_type - 1] * index, SDDS_type_size[data_type - 1]);
    return (memory);
  }
  switch (data_type) {
  case SDDS_SHORT:
    integer_value = *((short *)data + index);
    fp_value = integer_value;
    break;
  case SDDS_USHORT:
    integer_value = *((unsigned short *)data + index);
    fp_value = integer_value;
    break;
  case SDDS_LONG:
    integer_value = *((int32_t *)data + index);
    fp_value = integer_value;
    break;
  case SDDS_ULONG:
    integer_value = *((uint32_t *)data + index);
    fp_value = integer_value;
    break;
  case SDDS_LONG64:
    integer_value = *((int64_t *)data + index);
    fp_value = integer_value;
    break;
  case SDDS_ULONG64:
    integer_value = *((uint64_t *)data + index);
    fp_value = integer_value;
    break;
  case SDDS_CHARACTER:
    integer_value = *((unsigned char *)data + index);
    fp_value = integer_value;
    break;
  case SDDS_FLOAT:
    fp_value = *((float *)data + index);
    integer_value = fp_value;
    break;
  case SDDS_DOUBLE:
    fp_value = *((double *)data + index);
    integer_value = fp_value;
    break;
  case SDDS_LONGDOUBLE:
    fp_value = *((long double *)data + index);
    integer_value = fp_value;
    break;
  default:
    return (NULL);
  }
  switch (desired_type) {
  case SDDS_CHARACTER:
    *((char *)memory) = integer_value;
    break;
  case SDDS_SHORT:
    *((short *)memory) = integer_value;
    break;
  case SDDS_USHORT:
    *((unsigned short *)memory) = integer_value;
    break;
  case SDDS_LONG:
    *((int32_t *)memory) = integer_value;
    break;
  case SDDS_ULONG:
    *((uint32_t *)memory) = integer_value;
    break;
  case SDDS_LONG64:
    *((int64_t *)memory) = integer_value;
    break;
  case SDDS_ULONG64:
    *((uint64_t *)memory) = integer_value;
    break;
  case SDDS_FLOAT:
    *((float *)memory) = fp_value;
    break;
  case SDDS_DOUBLE:
    *((double *)memory) = fp_value;
    break;
  case SDDS_LONGDOUBLE:
    *((long double *)memory) = fp_value;
    break;
  default:
    SDDS_SetError("The impossible has happened (SDDS_CastValue)");
    return (NULL);
  }
  return (memory);
}

/**
 * @brief Allocates a two-dimensional matrix with zero-initialized elements.
 *
 * This function allocates memory for a two-dimensional matrix based on the specified dimensions and element size. Each row of the matrix is individually allocated and initialized to zero.
 *
 * @param[in]  size   The size in bytes of each element in the matrix.
 * @param[in]  dim1   The number of rows in the matrix.
 * @param[in]  dim2   The number of columns in the matrix.
 *
 * @return 
 * - Returns a pointer to the allocated two-dimensional matrix on success.
 * - Returns `NULL` if memory allocation fails.
 *
 * @note 
 * - The function uses `calloc` to ensure that all elements are zero-initialized.
 * - The caller is responsible for freeing the allocated memory using `SDDS_FreeMatrix`.
 *
 * @see SDDS_FreeMatrix
 * @see calloc
 */
void *SDDS_AllocateMatrix(int32_t size, int64_t dim1, int64_t dim2) {
  int64_t i;
  void **data;

  if (!(data = (void **)SDDS_Malloc(sizeof(*data) * dim1)))
    return (NULL);
  for (i = 0; i < dim1; i++)
    if (!(data[i] = (void *)calloc(dim2, size)))
      return (NULL);
  return (data);
}

/**
 * @brief Frees memory allocated for an SDDS array structure.
 *
 * This function deallocates all memory associated with an `SDDS_ARRAY` structure, including its data and definition. It handles the freeing of string elements if the array type is `SDDS_STRING` and ensures that all pointers are set to `NULL` after deallocation to prevent dangling references.
 *
 * @param[in]  array Pointer to the `SDDS_ARRAY` structure to be freed.
 *
 * @note 
 * - The function assumes that the array's data and definitions were allocated using SDDS memory management functions.
 * - After calling this function, the `array` pointer becomes invalid and should not be used.
 *
 * @see SDDS_FreePointerArray
 * @see SDDS_FreeArrayDefinition
 * @see SDDS_Free
 */
void SDDS_FreeArray(SDDS_ARRAY *array) {
  int i;
  if (!array)
    return;
  if (array->definition) {
    if ((array->definition->type == SDDS_STRING) && (array->data)) {
      char **str = (char **)array->data;
      for (i = 0; i < array->elements; i++) {
        if (str[i])
          free(str[i]);
        str[i] = NULL;
      }
    }
  }
  if (array->definition && array->pointer)
    SDDS_FreePointerArray(array->pointer, array->definition->dimensions, array->dimension);
  if (array->data)
    free(array->data);
  array->pointer = array->data = NULL;
  if (array->dimension)
    free(array->dimension);
  if (array->definition)
    SDDS_FreeArrayDefinition(array->definition);
  array->definition = NULL;
  free(array);
  array = NULL;
}

/**
 * @brief Frees memory allocated for a two-dimensional matrix.
 *
 * This function deallocates a two-dimensional matrix by freeing each row individually followed by the matrix pointer itself.
 *
 * @param[in]  ptr   Pointer to the two-dimensional matrix to be freed.
 * @param[in]  dim1  The number of rows in the matrix.
 *
 * @note 
 * - The function assumes that the matrix was allocated using `SDDS_AllocateMatrix` or similar memory allocation functions.
 *
 * @see SDDS_AllocateMatrix
 * @see free
 */
void SDDS_FreeMatrix(void **ptr, int64_t dim1) {
  int64_t i;
  if (!ptr)
    return;
  for (i = 0; i < dim1; i++)
    free(ptr[i]);
  free(ptr);
}

/**
 * @brief Copies an array of strings from source to target.
 *
 * This function duplicates each string from the `source` array into the `target` array. It handles memory allocation for each individual string using `SDDS_CopyString`.
 *
 * @param[in]  target     Pointer to the destination array of strings where the copied strings will be stored.
 * @param[in]  source     Pointer to the source array of strings to be copied.
 * @param[in]  n_strings  The number of strings to copy from the source to the target.
 *
 * @return 
 * - Returns `1` on successful copying of all strings.
 * - Returns `0` if either `source` or `target` is `NULL`, or if any string copy operation fails.
 *
 * @note 
 * - The caller is responsible for ensuring that the `target` array has sufficient space allocated.
 * - In case of failure, partially copied strings may remain in the `target` array.
 *
 * @see SDDS_CopyString
 * @see SDDS_Malloc
 */
int32_t SDDS_CopyStringArray(char **target, char **source, int64_t n_strings) {
  if (!source || !target)
    return (0);
  while (n_strings--) {
    if (!SDDS_CopyString(target + n_strings, source[n_strings]))
      return (0);
  }
  return (1);
}

/**
 * @brief Frees an array of strings by deallocating each individual string.
 *
 * This function iterates through an array of strings, freeing each non-NULL string and setting its pointer to `NULL` to prevent dangling references.
 *
 * @param[in,out] string  Array of strings to be freed.
 * @param[in]     strings The number of elements in the `string` array.
 *
 * @return 
 * - Returns `1` if the array is successfully freed.
 * - Returns `0` if the `string` pointer is `NULL`.
 *
 * @note 
 * - After calling this function, all string pointers within the array are set to `NULL`.
 *
 * @see SDDS_Free
 * @see free
 */
int32_t SDDS_FreeStringArray(char **string, int64_t strings) {
  int64_t i;
  if (!string)
    return 0;
  for (i = 0; i < strings; i++)
    if (string[i]) {
      free(string[i]);
      string[i] = NULL;
    }
  return 1;
}

/**
 * @brief Recursively creates a multi-dimensional pointer array from a contiguous data block.
 *
 * This internal function is used to build a multi-dimensional pointer array by recursively allocating pointer layers based on the specified dimensions. It maps the contiguous data block to the pointer structure, facilitating easy access to multi-dimensional data.
 *
 * @param[in]  data        Pointer to the data block or intermediate pointer array.
 * @param[in]  size        The size in bytes of each element in the current dimension.
 * @param[in]  dimensions  The number of remaining dimensions to process.
 * @param[in]  dimension   An array specifying the size of each remaining dimension.
 *
 * @return 
 * - Returns a pointer to the next layer of the pointer array on success.
 * - Returns `NULL` if the input data is `NULL`, the `dimension` array is invalid, the `size` is non-positive, or memory allocation fails.
 *
 * @note 
 * - This function maintains a static `depth` variable to track recursion depth for error reporting.
 * - It is intended for internal use within the SDDS library and should not be called directly by user code.
 *
 * @see SDDS_MakePointerArray
 * @see SDDS_SetError
 * @see SDDS_Malloc
 * @see SDDS_type_size
 */
void *SDDS_MakePointerArrayRecursively(void *data, int32_t size, int32_t dimensions, int32_t *dimension) {
  void **pointer;
  int32_t i, elements;
  static int32_t depth = 0;
  static char s[200];

  depth += 1;
  if (!data) {
    sprintf(s, "Unable to make pointer array--NULL data array (SDDS_MakePointerArrayRecursively, recursion %" PRId32 ")", depth);
    SDDS_SetError(s);
    return (NULL);
  }
  if (!dimension || !dimensions) {
    sprintf(s, "Unable to make pointer array--NULL or zero-length dimension array (SDDS_MakePointerArrayRecursively, recursion %" PRId32 ")", depth);
    SDDS_SetError(s);
    return (NULL);
  }
  if (size <= 0) {
    sprintf(s, "Unable to make pointer array--invalid data size (SDDS_MakePointerArrayRecursively, recursion %" PRId32 ")", depth);
    SDDS_SetError(s);
    return (NULL);
  }
  if (dimensions == 1) {
    depth -= 1;
    return (data);
  }
  elements = 1;
  for (i = 0; i < dimensions - 1; i++)
    elements *= dimension[i];
  if (!(pointer = (void **)SDDS_Malloc(sizeof(void *) * elements))) {
    sprintf(s, "Unable to make pointer array--allocation failure (SDDS_MakePointerArrayRecursively, recursion %" PRId32 ")", depth);
    SDDS_SetError(s);
    return (NULL);
  }
  for (i = 0; i < elements; i++)
    pointer[i] = (char *)data + i * size * dimension[dimensions - 1];
  return (SDDS_MakePointerArrayRecursively(pointer, sizeof(*pointer), dimensions - 1, dimension));
}

/**
 * @brief Creates a multi-dimensional pointer array from a contiguous data block.
 *
 * This function generates a multi-dimensional pointer array that maps to a contiguous block of data. It supports arrays with multiple dimensions by recursively creating pointer layers. The `dimensions` parameter specifies the number of dimensions, and the `dimension` array provides the size for each dimension.
 *
 * @param[in]  data        Pointer to the contiguous data block to be mapped.
 * @param[in]  type        The SDDS data type of the elements in the data block. Must be one of the SDDS type constants:
 *                          - `SDDS_SHORT`
 *                          - `SDDS_USHORT`
 *                          - `SDDS_LONG`
 *                          - `SDDS_ULONG`
 *                          - `SDDS_LONG64`
 *                          - `SDDS_ULONG64`
 *                          - `SDDS_FLOAT`
 *                          - `SDDS_DOUBLE`
 *                          - `SDDS_LONGDOUBLE`
 *                          - `SDDS_CHARACTER`
 * @param[in]  dimensions  The number of dimensions for the pointer array.
 * @param[in]  dimension   An array specifying the size of each dimension.
 *
 * @return 
 * - Returns a pointer to the newly created multi-dimensional pointer array on success.
 * - Returns `NULL` if the input data is `NULL`, the `dimension` array is invalid, the `type` is unknown, or memory allocation fails.
 *
 * @note 
 * - The function uses `SDDS_MakePointerArrayRecursively` to handle multi-dimensional allocations.
 * - The caller is responsible for freeing the allocated pointer array using `SDDS_FreePointerArray`.
 *
 * @see SDDS_MakePointerArrayRecursively
 * @see SDDS_FreePointerArray
 * @see SDDS_SetError
 */
void *SDDS_MakePointerArray(void *data, int32_t type, int32_t dimensions, int32_t *dimension) {
  int32_t i;

  if (!data) {
    SDDS_SetError("Unable to make pointer array--NULL data array (SDDS_MakePointerArray)");
    return (NULL);
  }
  if (!dimension || !dimensions) {
    SDDS_SetError("Unable to make pointer array--NULL or zero-length dimension array (SDDS_MakePointerArray)");
    return (NULL);
  }
  if (type <= 0 || type > SDDS_NUM_TYPES) {
    SDDS_SetError("Unable to make pointer array--unknown data type (SDDS_MakePointerArray)");
    return (NULL);
  }
  for (i = 0; i < dimensions; i++)
    if (dimension[i] <= 0) {
      SDDS_SetError("Unable to make pointer array--number of elements invalid (SDDS_MakePointerArray)");
      return (NULL);
    }
  if (dimensions == 1)
    return (data);
  return (SDDS_MakePointerArrayRecursively(data, SDDS_type_size[type - 1], dimensions, dimension));
}

/**
 * @brief Frees a multi-dimensional pointer array created by SDDS_MakePointerArray.
 *
 * This function recursively deallocates a multi-dimensional pointer array that was previously created using `SDDS_MakePointerArray` or `SDDS_MakePointerArrayRecursively`. It ensures that all pointer layers are properly freed to prevent memory leaks.
 *
 * @param[in]  data        Pointer to the multi-dimensional pointer array to be freed.
 * @param[in]  dimensions  The number of dimensions in the pointer array.
 * @param[in]  dimension   An array specifying the size of each dimension.
 *
 * @note 
 * - The function assumes that the pointer array was created using SDDS library functions.
 * - It does not free the actual data block pointed to by the pointer array.
 *
 * @see SDDS_MakePointerArrayRecursively
 * @see free
 */
void SDDS_FreePointerArray(void **data, int32_t dimensions, int32_t *dimension)
/* This procedure is specifically for freeing the pointer arrays made by SDDS_MakePointerArray
 * and *will not* work with general pointer arrays
 */
{
  if (!data || !dimension || !dimensions)
    return;
  if (dimensions > 1) {
    SDDS_FreePointerArray((void **)(data[0]), dimensions - 1, dimension + 1);
    free(data);
  }
}

/**
 * @brief Applies a scaling factor to a specific parameter in the SDDS dataset.
 *
 * This function multiplies the value of a specified parameter by the given `factor`. It first retrieves the parameter's index and verifies that it is of a numeric type. The scaling operation is performed in-place on the parameter's data.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A NULL-terminated string specifying the name of the parameter to scale.
 * @param[in]  factor       The scaling factor to apply to the parameter's value.
 *
 * @return 
 * - Returns `1` on successful application of the factor.
 * - Returns `0` if the parameter is not found, is non-numeric, or if the dataset lacks the necessary data array.
 *
 * @note 
 * - The function modifies the parameter's value directly within the dataset.
 * - It supports various numeric SDDS data types.
 *
 * @see SDDS_GetParameterIndex
 * @see SDDS_NUMERIC_TYPE
 * @see SDDS_SetError
 */
int32_t SDDS_ApplyFactorToParameter(SDDS_DATASET *SDDS_dataset, char *name, double factor) {
  int32_t type, index;
  void *data;

  if ((index = SDDS_GetParameterIndex(SDDS_dataset, name)) < 0)
    return (0);
  type = SDDS_dataset->layout.parameter_definition[index].type;
  if (!SDDS_NUMERIC_TYPE(type)) {
    SDDS_SetError("Unable to apply factor to non-numeric parameter (SDDS_ApplyFactorToParameter)");
    return (0);
  }
  if (!SDDS_dataset->parameter) {
    SDDS_SetError("Unable to apply factor to parameter--no parameter data array (SDDS_ApplyFactorToParameter)");
    return (0);
  }
  if (!(data = SDDS_dataset->parameter[index])) {
    SDDS_SetError("Unable to apply factor to parameter--no data array (SDDS_ApplyFactorToParameter)");
    return (0);
  }
  switch (type) {
  case SDDS_SHORT:
    *((short *)data) *= factor;
    break;
  case SDDS_USHORT:
    *((unsigned short *)data) *= factor;
    break;
  case SDDS_LONG:
    *((int32_t *)data) *= factor;
    break;
  case SDDS_ULONG:
    *((uint32_t *)data) *= factor;
    break;
  case SDDS_LONG64:
    *((int64_t *)data) *= factor;
    break;
  case SDDS_ULONG64:
    *((uint64_t *)data) *= factor;
    break;
  case SDDS_CHARACTER:
    *((char *)data) *= factor;
    break;
  case SDDS_FLOAT:
    *((float *)data) *= factor;
    break;
  case SDDS_DOUBLE:
    *((double *)data) *= factor;
    break;
  case SDDS_LONGDOUBLE:
    *((long double *)data) *= factor;
    break;
  default:
    return (0);
  }
  return (1);
}

/**
 * @brief Applies a scaling factor to all elements of a specific column in the SDDS dataset.
 *
 * This function multiplies each value in the specified column by the given `factor`. It first retrieves the column's index and verifies that it is of a numeric type. The scaling operation is performed in-place on each element of the column's data array.
 *
 * @param[in]  SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset.
 * @param[in]  name         A NULL-terminated string specifying the name of the column to scale.
 * @param[in]  factor       The scaling factor to apply to each element of the column.
 *
 * @return 
 * - Returns `1` on successful application of the factor to all elements.
 * - Returns `0` if the column is not found, is non-numeric, or if the dataset lacks the necessary data array.
 *
 * @note 
 * - The function modifies each element of the column's data array directly within the dataset.
 * - It supports various numeric SDDS data types.
 *
 * @see SDDS_GetColumnIndex
 * @see SDDS_NUMERIC_TYPE
 * @see SDDS_SetError
 */
int32_t SDDS_ApplyFactorToColumn(SDDS_DATASET *SDDS_dataset, char *name, double factor) {
  int32_t type, index;
  int64_t i;
  void *data;

  if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0)
    return (0);
  type = SDDS_dataset->layout.column_definition[index].type;
  if (!SDDS_NUMERIC_TYPE(type)) {
    SDDS_SetError("Unable to apply factor to non-numeric column (SDDS_ApplyFactorToColumn)");
    return (0);
  }
  data = SDDS_dataset->data[index];
  for (i = 0; i < SDDS_dataset->n_rows; i++) {
    switch (type) {
    case SDDS_SHORT:
      *((short *)data + i) *= factor;
      break;
    case SDDS_USHORT:
      *((unsigned short *)data + i) *= factor;
      break;
    case SDDS_LONG:
      *((int32_t *)data + i) *= factor;
      break;
    case SDDS_ULONG:
      *((uint32_t *)data + i) *= factor;
      break;
    case SDDS_LONG64:
      *((int64_t *)data + i) *= factor;
      break;
    case SDDS_ULONG64:
      *((uint64_t *)data + i) *= factor;
      break;
    case SDDS_CHARACTER:
      *((char *)data + i) *= factor;
      break;
    case SDDS_FLOAT:
      *((float *)data + i) *= factor;
      break;
    case SDDS_DOUBLE:
      *((double *)data + i) *= factor;
      break;
    case SDDS_LONGDOUBLE:
      *((long double *)data + i) *= factor;
      break;
    default:
      return (0);
    }
  }
  return (1);
}

/**
 * @brief Escapes newline characters in a string by replacing them with "\\n".
 *
 * This function modifies the input string \p s in place by replacing each newline character (`'\n'`) with the two-character sequence `'\\'` and `'n'`. It shifts the subsequent characters in the string to accommodate the additional character introduced by the escape sequence.
 *
 * @param[in, out] s
 *     Pointer to the null-terminated string to be modified. 
 *     **Important:** The buffer pointed to by \p s must have sufficient space to accommodate the additional characters resulting from the escape sequences. Failure to ensure adequate space may lead to buffer overflows.
 *
 * @warning
 *     This function does not perform bounds checking on the buffer size. Ensure that the buffer is large enough to handle the increased length after escaping newlines.
 *
 * @sa SDDS_UnescapeNewlines
 */
void SDDS_EscapeNewlines(char *s) {
  char *ptr;
  while (*s) {
    if (*s == '\n') {
      ptr = s + strlen(s);
      *(ptr + 1) = 0;
      while (ptr != s) {
        *ptr = *(ptr - 1);
        ptr--;
      }
      *s++ = '\\';
      *s++ = 'n';
    } else
      s++;
  }
}

/**
 * @brief Marks an SDDS dataset as inactive.
 *
 * This function forces the provided SDDS dataset to become inactive by setting its file pointer to `NULL`. An inactive dataset is typically not associated with any open file operations.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure to be marked as inactive.
 *
 * @return
 *     - `1` on successful operation.
 *     - `-1` if a `NULL` pointer is passed, indicating an error.
 *
 * @note
 *     After calling this function, the dataset will be considered inactive, and any subsequent operations that require an active dataset may fail.
 *
 * @sa SDDS_IsActive, SDDS_SetError
 */
int32_t SDDS_ForceInactive(SDDS_DATASET *SDDS_dataset) {
  if (!SDDS_dataset) {
    SDDS_SetError("NULL SDDS_DATASET passed (SDDS_ForceInactive)");
    return (-1);
  }
  SDDS_dataset->layout.fp = NULL;
  return (1);
}

/**
 * @brief Checks whether an SDDS dataset is currently active.
 *
 * This function determines the active status of the provided SDDS dataset by verifying if its file pointer is non-`NULL`.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure to be checked.
 *
 * @return
 *     - `1` if the dataset is active (i.e., the file pointer is non-`NULL`).
 *     - `0` if the dataset is inactive (i.e., the file pointer is `NULL`).
 *     - `-1` if a `NULL` pointer is passed, indicating an error.
 *
 * @note
 *     An inactive dataset does not have an associated open file, and certain operations may not be applicable.
 *
 * @sa SDDS_ForceInactive, SDDS_SetError
 */
int32_t SDDS_IsActive(SDDS_DATASET *SDDS_dataset) {
  if (!SDDS_dataset) {
    SDDS_SetError("NULL SDDS_DATASET passed (SDDS_IsActive)");
    return (-1);
  }
  if (!SDDS_dataset->layout.fp)
    return (0);
  return (1);
}

/**
 * @brief Determines if a specified file is locked.
 *
 * This function checks whether the given file is currently locked. If file locking is enabled through the `F_TEST` and `ALLOW_FILE_LOCKING` macros, it attempts to open the file and apply a test lock using `lockf`. The function returns `1` if the file is locked and `0` otherwise.
 *
 * If file locking is not enabled (i.e., the `F_TEST` and `ALLOW_FILE_LOCKING` macros are not defined), the function always returns `0`, indicating that the file is not locked.
 *
 * @param[in] filename
 *     The path to the file to be checked for a lock.
 *
 * @return
 *     - `1` if the file is locked.
 *     - `0` if the file is not locked or if file locking is not enabled.
 *
 * @note
 *     The effectiveness of this function depends on the platform and the implementation of file locking mechanisms.
 *
 * @warning
 *     Ensure that the `F_TEST` and `ALLOW_FILE_LOCKING` macros are appropriately defined to enable file locking functionality.
 *
 * @sa SDDS_LockFile
 */
int32_t SDDS_FileIsLocked(const char *filename) {
#if defined(F_TEST) && ALLOW_FILE_LOCKING
  FILE *fp;
  if (!(fp = fopen(filename, "rb")))
    return 0;
  if (lockf(fileno(fp), F_TEST, 0) == -1) {
    fclose(fp);
    return 1;
  }
  fclose(fp);
  return 0;
#else
  return 0;
#endif
}

/**
 * @brief Attempts to lock a specified file.
 *
 * This function tries to acquire a lock on the provided file using the given file pointer. If file locking is enabled via the `F_TEST` and `ALLOW_FILE_LOCKING` macros, it first tests whether the file can be locked and then attempts to establish an exclusive lock. If locking fails at any step, an error message is set, and the function returns `0`.
 *
 * If file locking is not enabled, the function assumes that the file is not locked and returns `1`.
 *
 * @param[in] fp
 *     Pointer to the open `FILE` stream associated with the file to be locked.
 *
 * @param[in] filename
 *     The path to the file to be locked. Used primarily for error messaging.
 *
 * @param[in] caller
 *     A string identifying the caller or the context in which the lock is being attempted. This is used in error messages to provide more information about the lock attempt.
 *
 * @return
 *     - `1` if the file lock is successfully acquired or if file locking is not enabled.
 *     - `0` if the file is already locked or if locking fails for another reason.
 *
 * @note
 *     The function relies on the `lockf` system call for file locking, which may not be supported on all platforms.
 *
 * @warning
 *     Proper error handling should be implemented by the caller to handle cases where file locking fails.
 *
 * @sa SDDS_FileIsLocked, SDDS_SetError
 */
int32_t SDDS_LockFile(FILE *fp, const char *filename, const char *caller) {
#if defined(F_TEST) && ALLOW_FILE_LOCKING
  char s[1024];
  if (lockf(fileno(fp), F_TEST, 0) == -1) {
    sprintf(s, "Unable to access file %s--file is locked (%s)", filename, caller);
    SDDS_SetError(s);
    return 0;
  }
  if (lockf(fileno(fp), F_TLOCK, 0) == -1) {
    sprintf(s, "Unable to establish lock on file %s (%s)", filename, caller);
    SDDS_SetError(s);
    return 0;
  }
  return 1;
#else
  return 1;
#endif
}

/**
 * @brief Attempts to override a locked file by creating a temporary copy.
 *
 * This function tries to break into a locked file by creating a temporary backup and replacing the original file with this backup. The process involves:
 * - Generating a temporary filename with a `.blXXX` suffix, where `XXX` ranges from `1000` to `1019`.
 * - Copying the original file to the temporary file while preserving file attributes.
 * - Replacing the original file with the temporary copy.
 *
 * On Windows systems (`_WIN32` defined), the function currently does not support breaking into locked files and will output an error message.
 *
 * @param[in] filename
 *     The path to the locked file that needs to be overridden.
 *
 * @return
 *     - `0` on successful override of the locked file.
 *     - `1` if the operation fails or is not supported on the current platform.
 *
 * @warning
 *     - The function limits the filename length to 500 characters to prevent buffer overflows.
 *     - Ensure that the necessary permissions are available to create and modify files in the target directory.
 *
 * @note
 *     - This function relies on the availability of the `cp` and `mv` system commands on Unix-like systems.
 *     - The function attempts up to 20 different temporary filenames before failing.
 *
 * @sa SDDS_FileIsLocked, SDDS_LockFile
 */
int32_t SDDS_BreakIntoLockedFile(char *filename) {
#if defined(_WIN32)
  fprintf(stderr, "Unable to break into locked file\n");
  return (1);
#else
  char buffer[1024];
  int i = 1000, j = 0;
  FILE *fp;

  /* limit filename length to 500 so we don't overflow the buffer variable */
  if (strlen(filename) > 500) {
    fprintf(stderr, "Unable to break into locked file\n");
    return (1);
  }

  /* find a temporary file name that is not already in use */
  for (i = 1000; i < 1020; i++) {
    sprintf(buffer, "%s.bl%d", filename, i);
    if ((fp = fopen(buffer, "r"))) {
      fclose(fp);
    } else {
      j = i;
      break;
    }
  }

  /* if no temporary file names could be found then return with an error message */
  if (j == 0) {
    fprintf(stderr, "Unable to break into locked file\n");
    return (1);
  }

  /* copy the original file to the temp file name and preserve the attributes */
  /* the temp file name has to be in the same directory to preserve ACL settings */
  sprintf(buffer, "cp -p %s %s.bl%d", filename, filename, j);
  if (system(buffer) == -1) {
    fprintf(stderr, "Unable to break into locked file\n");
    return (1);
  }

  /* move the temp file on top of the original file */
  sprintf(buffer, "mv -f %s.bl%d %s", filename, j, filename);
  if (system(buffer) == -1) {
    fprintf(stderr, "Unable to break into locked file\n");
    return (1);
  }
  return (0);
#endif
}

/**
 * @brief Matches and retrieves column names from an SDDS dataset based on specified criteria.
 *
 * This function selects columns from the provided SDDS dataset according to the specified matching mode and type mode. It supports various calling conventions depending on the matching criteria.
 *
 * The function supports the following matching modes:
 * - **SDDS_NAME_ARRAY**:
 *   - **Parameters**: `int32_t n_entries`, `char **name`
 *   - **Description**: Matches columns whose names are present in the provided array.
 *
 * - **SDDS_NAMES_STRING**:
 *   - **Parameters**: `char *names`
 *   - **Description**: Matches columns whose names are specified in a single comma-separated string.
 *
 * - **SDDS_NAME_STRINGS**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Matches columns whose names are specified as individual string arguments, terminated by a `NULL` pointer.
 *
 * - **SDDS_MATCH_STRING**:
 *   - **Parameters**: `char *name`, `int32_t logic_mode`
 *   - **Description**: Matches columns based on a wildcard pattern provided in `name`, using the specified logical mode.
 *
 * - **SDDS_MATCH_EXCLUDE_STRING**:
 *   - **Parameters**: `char *name`, `char *exclude`, `int32_t logic_mode`
 *   - **Description**: Matches columns based on a wildcard pattern provided in `name`, excluding those that match the `exclude` pattern, using the specified logical mode.
 *
 * Additionally, the `typeMode` parameter allows filtering based on column types, such as numeric, floating, or integer types.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure containing the dataset.
 *
 * @param[out] nameReturn
 *     Pointer to a `char**` that will be allocated and populated with the names of the matched columns. The caller is responsible for freeing the allocated memory.
 *
 * @param[in] matchMode
 *     Specifies the matching mode (e.g., `SDDS_NAME_ARRAY`, `SDDS_NAMES_STRING`, etc.).
 *
 * @param[in] typeMode
 *     Specifies the type matching mode (e.g., `FIND_SPECIFIED_TYPE`, `FIND_NUMERIC_TYPE`, `FIND_FLOATING_TYPE`, `FIND_INTEGER_TYPE`).
 *
 * @param[in] ...
 *     Variable arguments depending on `matchMode`:
 *     - **SDDS_NAME_ARRAY**: `int32_t n_entries`, `char **name`
 *     - **SDDS_NAMES_STRING**: `char *names`
 *     - **SDDS_NAME_STRINGS**: `char *name1, char *name2, ..., NULL`
 *     - **SDDS_MATCH_STRING**: `char *name`, `int32_t logic_mode`
 *     - **SDDS_MATCH_EXCLUDE_STRING**: `char *name`, `char *exclude`, `int32_t logic_mode`
 *
 * @return
 *     - Returns the number of matched columns on success.
 *     - Returns `-1` if an error occurs (e.g., invalid parameters, memory allocation failure).
 *
 * @note
 *     - The function internally manages memory for the matching process and allocates memory for `nameReturn`, which must be freed by the caller using appropriate memory deallocation functions.
 *     - The dataset must be properly initialized and contain a valid layout before calling this function.
 *
 * @warning
 *     - Ensure that the variable arguments match the expected parameters for the specified `matchMode`.
 *     - The caller is responsible for freeing the memory allocated for `nameReturn` to avoid memory leaks.
 *
 * @sa SDDS_MatchParameters, SDDS_SetError
 */
int32_t SDDS_MatchColumns(SDDS_DATASET *SDDS_dataset, char ***nameReturn, int32_t matchMode, int32_t typeMode, ...)
/* This routine has 5 calling modes:
 * SDDS_MatchColumns(&SDDS_dataset, &matchName, SDDS_NAME_ARRAY  , int32_t typeMode [,int32_t type], int32_t n_entries, char **name)
 * SDDS_MatchColumns(&SDDS_dataset, &matchName, SDDS_NAMES_STRING, int32_t typeMode [,int32_t type], char *names)
 * SDDS_MatchColumns(&SDDS_dataset, &matchName, SDDS_NAME_STRINGS, int32_t typeMode [,int32_t type], char *name1, char *name2, ..., NULL )
 * SDDS_MatchColumns(&SDDS_dataset, &matchName, SDDS_MATCH_STRING, int32_t typeMode [,int32_t type], char *name, int32_t logic_mode)
 * SDDS_MatchColumns(&SDDS_dataset, &matchName, SDDS_MATCH_EXCLUDE_STRING, int32_t typeMode [,int32_t type], char *name, char *exclude, int32_t logic_mode)
 */
{
  static int32_t flags = 0;
  static int32_t *flag = NULL;
  char **name, *string, *match_string, *ptr, *exclude_string;
  va_list argptr;
  int32_t retval, requiredType;
  int32_t i, j, n_names, index, matches;
  int32_t local_memory; /* (0,1,2) --> (none, pointer array, pointer array + strings) locally allocated */
  char buffer[SDDS_MAXLINE];
  int32_t logic;

  name = NULL;
  match_string = exclude_string = NULL;
  n_names = requiredType = local_memory = logic = 0;

  matches = -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MatchColumns"))
    return -1;
  if (nameReturn)
    *nameReturn = NULL;

  retval = 1;
  va_start(argptr, typeMode);
  if (typeMode == FIND_SPECIFIED_TYPE)
    requiredType = va_arg(argptr, int32_t);
  switch (matchMode) {
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
        SDDS_SetError("Unable to process column selection--memory allocation failure (SDDS_MatchColumns)");
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
        SDDS_SetError("Unable to process column selection--memory allocation failure (SDDS_MatchColumns)");
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
      SDDS_SetError("Unable to process column selection--invalid matching string (SDDS_MatchColumns)");
      retval = 0;
      break;
    }
    match_string = expand_ranges(string);
    logic = va_arg(argptr, int32_t);
    break;
  case SDDS_MATCH_EXCLUDE_STRING:
    local_memory = 0;
    n_names = 1;
    if (!(string = va_arg(argptr, char *))) {
      SDDS_SetError("Unable to process column selection--invalid matching string (SDDS_MatchColumns)");
      retval = 0;
      break;
    }
    match_string = expand_ranges(string);
    if (!(string = va_arg(argptr, char *))) {
      SDDS_SetError("Unable to process column exclusion--invalid matching string (SDDS_MatchColumns)");
      retval = 0;
      break;
    }
    exclude_string = expand_ranges(string);
    logic = va_arg(argptr, int32_t);
    break;
  default:
    SDDS_SetError("Unable to process column selection--unknown match mode (SDDS_MatchColumns)");
    retval = 0;
    break;
  }
  va_end(argptr);
  if (retval == 0)
    return -1;

  if (n_names == 0) {
    SDDS_SetError("Unable to process column selection--no names in call (SDDS_MatchColumns)");
    return -1;
  }

  if (SDDS_dataset->layout.n_columns != flags) {
    flags = SDDS_dataset->layout.n_columns;
    if (flag)
      free(flag);
    if (!(flag = (int32_t *)calloc(flags, sizeof(*flag)))) {
      SDDS_SetError("Memory allocation failure (SDDS_MatchColumns)");
      return -1;
    }
  }

  if ((matchMode != SDDS_MATCH_STRING) && (matchMode != SDDS_MATCH_EXCLUDE_STRING)) {
    for (i = 0; i < n_names; i++) {
      if ((index = SDDS_GetColumnIndex(SDDS_dataset, name[i])) >= 0)
        flag[index] = 1;
      else
        flag[index] = 0;
    }
  } else {
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
      if (SDDS_Logic(flag[i], wild_match(SDDS_dataset->layout.column_definition[i].name, match_string), logic)) {
        if (exclude_string != NULL) {
          if (SDDS_Logic(flag[i], wild_match(SDDS_dataset->layout.column_definition[i].name, exclude_string), logic))
            flag[i] = 0;
          else
            flag[i] = 1;
        } else {
          flag[i] = 1;
        }
      } else {
#if defined(DEBUG)
        fprintf(stderr, "no logic match of %s to %s\n", SDDS_dataset->layout.column_definition[i].name, match_string);
#endif
        flag[i] = 0;
      }
    }
  }
  if (match_string)
    free(match_string);
  if (exclude_string)
    free(exclude_string);
#if defined(DEBUG)
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
    fprintf(stderr, "flag[%" PRId32 "] = %" PRId32 " : %s\n", i, flag[i], SDDS_dataset->layout.column_definition[i].name);
#endif

  if (local_memory == 2) {
    for (i = 0; i < n_names; i++)
      free(name[i]);
  }
  if (local_memory >= 1)
    free(name);

  switch (typeMode) {
  case FIND_SPECIFIED_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
      if (SDDS_dataset->layout.column_definition[i].type != requiredType)
        flag[i] = 0;
    break;
  case FIND_NUMERIC_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
      if (!SDDS_NUMERIC_TYPE(SDDS_dataset->layout.column_definition[i].type))
        flag[i] = 0;
    break;
  case FIND_FLOATING_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
      if (!SDDS_FLOATING_TYPE(SDDS_dataset->layout.column_definition[i].type))
        flag[i] = 0;
    break;
  case FIND_INTEGER_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
      if (!SDDS_INTEGER_TYPE(SDDS_dataset->layout.column_definition[i].type))
        flag[i] = 0;
    break;
  default:
    break;
  }
#if defined(DEBUG)
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
    if (flag[i])
      fprintf(stderr, "column %s matched\n", SDDS_dataset->layout.column_definition[i].name);
#endif

  for (i = matches = 0; i < SDDS_dataset->layout.n_columns; i++) {
    if (flag[i])
      matches++;
  }
  if (!matches || !nameReturn)
    return matches;
  if (!((*nameReturn) = (char **)SDDS_Malloc(matches * sizeof(**nameReturn)))) {
    SDDS_SetError("Memory allocation failure (SDDS_MatchColumns)");
    return -1;
  }
  for (i = j = 0; i < SDDS_dataset->layout.n_columns; i++) {
    if (flag[i]) {
      if (!SDDS_CopyString((*nameReturn) + j, SDDS_dataset->layout.column_definition[i].name)) {
        SDDS_SetError("String copy failure (SDDS_MatchColumns)");
        return -1;
      }
      j++;
    }
  }
  return matches;
}

/**
 * @brief Matches and retrieves parameter names from an SDDS dataset based on specified criteria.
 *
 * This function selects parameters from the provided SDDS dataset according to the specified matching mode and type mode. It supports various calling conventions depending on the matching criteria.
 *
 * The function supports the following matching modes:
 * - **SDDS_NAME_ARRAY**:
 *   - **Parameters**: `int32_t n_entries`, `char **name`
 *   - **Description**: Matches parameters whose names are present in the provided array.
 *
 * - **SDDS_NAMES_STRING**:
 *   - **Parameters**: `char *names`
 *   - **Description**: Matches parameters whose names are specified in a single comma-separated string.
 *
 * - **SDDS_NAME_STRINGS**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Matches parameters whose names are specified as individual string arguments, terminated by a `NULL` pointer.
 *
 * - **SDDS_MATCH_STRING**:
 *   - **Parameters**: `char *name`, `int32_t logic_mode`
 *   - **Description**: Matches parameters based on a wildcard pattern provided in `name`, using the specified logical mode.
 *
 * - **SDDS_MATCH_EXCLUDE_STRING**:
 *   - **Parameters**: `char *name`, `char *exclude`, `int32_t logic_mode`
 *   - **Description**: Matches parameters based on a wildcard pattern provided in `name`, excluding those that match the `exclude` pattern, using the specified logical mode.
 *
 * Additionally, the `typeMode` parameter allows filtering based on parameter types, such as numeric, floating, or integer types.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure containing the dataset.
 *
 * @param[out] nameReturn
 *     Pointer to a `char**` that will be allocated and populated with the names of the matched parameters. The caller is responsible for freeing the allocated memory.
 *
 * @param[in] matchMode
 *     Specifies the matching mode (e.g., `SDDS_NAME_ARRAY`, `SDDS_NAMES_STRING`, etc.).
 *
 * @param[in] typeMode
 *     Specifies the type matching mode (e.g., `FIND_SPECIFIED_TYPE`, `FIND_NUMERIC_TYPE`, `FIND_FLOATING_TYPE`, `FIND_INTEGER_TYPE`).
 *
 * @param[in] ...
 *     Variable arguments depending on `matchMode`:
 *     - **SDDS_NAME_ARRAY**: `int32_t n_entries`, `char **name`
 *     - **SDDS_NAMES_STRING**: `char *names`
 *     - **SDDS_NAME_STRINGS**: `char *name1, char *name2, ..., NULL`
 *     - **SDDS_MATCH_STRING**: `char *name`, `int32_t logic_mode`
 *     - **SDDS_MATCH_EXCLUDE_STRING**: `char *name`, `char *exclude`, `int32_t logic_mode`
 *
 * @return
 *     - Returns the number of matched parameters on success.
 *     - Returns `-1` if an error occurs (e.g., invalid parameters, memory allocation failure).
 *
 * @note
 *     - The function internally manages memory for the matching process and allocates memory for `nameReturn`, which must be freed by the caller using appropriate memory deallocation functions.
 *     - The dataset must be properly initialized and contain a valid layout before calling this function.
 *
 * @warning
 *     - Ensure that the variable arguments match the expected parameters for the specified `matchMode`.
 *     - The caller is responsible for freeing the memory allocated for `nameReturn` to avoid memory leaks.
 *
 * @sa SDDS_MatchColumns, SDDS_SetError
 */
int32_t SDDS_MatchParameters(SDDS_DATASET *SDDS_dataset, char ***nameReturn, int32_t matchMode, int32_t typeMode, ...)
/* This routine has 4 calling modes:
 * SDDS_MatchParameters(&SDDS_dataset, &matchName, SDDS_NAME_ARRAY  , int32_t typeMode [,long type], int32_t n_entries, char **name)
 * SDDS_MatchParameters(&SDDS_dataset, &matchName, SDDS_NAMES_STRING, int32_t typeMode [,long type], char *names)
 * SDDS_MatchParameters(&SDDS_dataset, &matchName, SDDS_NAME_STRINGS, int32_t typeMode [,long type], char *name1, char *name2, ..., NULL )
 * SDDS_MatchParameters(&SDDS_dataset, &matchName, SDDS_MATCH_STRING, int32_t typeMode [,long type], char *name, int32_t logic_mode)
 * SDDS_MatchParameters(&SDDS_dataset, &matchName, SDDS_MATCH_EXCLUDE_STRING, int32_t typeMode [,long type], char *name, char *exclude, int32_t logic_mode)
 */
{
  static int32_t flags = 0, *flag = NULL;
  char **name, *string, *match_string, *ptr, *exclude_string;
  va_list argptr;
  int32_t i, j, index, n_names, retval, requiredType, matches;
  /*  int32_t type; */
  int32_t local_memory; /* (0,1,2) --> (none, pointer array, pointer array + strings) locally allocated */
  char buffer[SDDS_MAXLINE];
  int32_t logic;

  name = NULL;
  match_string = exclude_string = NULL;
  n_names = requiredType = local_memory = logic = 0;

  matches = -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MatchParameters"))
    return -1;
  if (nameReturn)
    *nameReturn = NULL;

  retval = 1;
  va_start(argptr, typeMode);
  if (typeMode == FIND_SPECIFIED_TYPE)
    requiredType = va_arg(argptr, int32_t);
  switch (matchMode) {
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
        SDDS_SetError("Unable to process parameter selection--memory allocation failure (SDDS_MatchParameters)");
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
        SDDS_SetError("Unable to process parameter selection--memory allocation failure (SDDS_MatchParameters)");
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
      SDDS_SetError("Unable to process parameter selection--invalid matching string (SDDS_MatchParameters)");
      retval = 0;
      break;
    }
    match_string = expand_ranges(string);
    logic = va_arg(argptr, int32_t);
    break;
  case SDDS_MATCH_EXCLUDE_STRING:
    local_memory = 0;
    n_names = 1;
    if (!(string = va_arg(argptr, char *))) {
      SDDS_SetError("Unable to process parameter selection--invalid matching string (SDDS_MatchParameters)");
      retval = 0;
      break;
    }
    match_string = expand_ranges(string);
    if (!(string = va_arg(argptr, char *))) {
      SDDS_SetError("Unable to process parameter exclusion--invalid matching string (SDDS_MatchParameters)");
      retval = 0;
      break;
    }
    exclude_string = expand_ranges(string);
    logic = va_arg(argptr, int32_t);
    break;
  default:
    SDDS_SetError("Unable to process parameter selection--unknown match mode (SDDS_MatchParameters)");
    retval = 0;
    break;
  }
  va_end(argptr);
  if (retval == 0)
    return -1;

  if (n_names == 0) {
    SDDS_SetError("Unable to process parameter selection--no names in call (SDDS_MatchParameters)");
    return -1;
  }

  if (SDDS_dataset->layout.n_parameters != flags) {
    flags = SDDS_dataset->layout.n_parameters;
    if (flag)
      free(flag);
    if (!(flag = (int32_t *)calloc(flags, sizeof(*flag)))) {
      SDDS_SetError("Memory allocation failure (SDDS_MatchParameters)");
      return -1;
    }
  }

  if ((matchMode != SDDS_MATCH_STRING) && (matchMode != SDDS_MATCH_EXCLUDE_STRING)) {
    for (i = 0; i < n_names; i++) {
      if ((index = SDDS_GetParameterIndex(SDDS_dataset, name[i])) >= 0)
        flag[index] = 1;
      else
        flag[index] = 0;
    }
  } else {
    for (i = 0; i < SDDS_dataset->layout.n_parameters; i++) {
      if (SDDS_Logic(flag[i], wild_match(SDDS_dataset->layout.parameter_definition[i].name, match_string), logic)) {
        if (exclude_string != NULL) {
          if (SDDS_Logic(flag[i], wild_match(SDDS_dataset->layout.parameter_definition[i].name, exclude_string), logic))
            flag[i] = 0;
          else
            flag[i] = 1;
        } else {
          flag[i] = 1;
        }
      } else {
#if defined(DEBUG)
        fprintf(stderr, "no logic match of %s to %s\n", SDDS_dataset->layout.parameter_definition[i].name, match_string);
#endif
        flag[i] = 0;
      }
    }
  }
  if (match_string)
    free(match_string);
  if (exclude_string)
    free(exclude_string);
#if defined(DEBUG)
  for (i = 0; i < SDDS_dataset->layout.n_parameters; i++)
    fprintf(stderr, "flag[%" PRId32 "] = %" PRId32 " : %s\n", i, flag[i], SDDS_dataset->layout.parameter_definition[i].name);
#endif

  if (local_memory == 2) {
    for (i = 0; i < n_names; i++)
      free(name[i]);
  }
  if (local_memory >= 1)
    free(name);

  switch (typeMode) {
  case FIND_SPECIFIED_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_parameters; i++)
      if (SDDS_dataset->layout.parameter_definition[i].type != requiredType)
        flag[i] = 0;
    break;
  case FIND_NUMERIC_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_parameters; i++)
      if (!SDDS_NUMERIC_TYPE(SDDS_dataset->layout.parameter_definition[i].type))
        flag[i] = 0;
    break;
  case FIND_FLOATING_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_parameters; i++)
      if (!SDDS_FLOATING_TYPE(SDDS_dataset->layout.parameter_definition[i].type))
        flag[i] = 0;
    break;
  case FIND_INTEGER_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_parameters; i++)
      if (!SDDS_INTEGER_TYPE(SDDS_dataset->layout.parameter_definition[i].type))
        flag[i] = 0;
    break;
  default:
    break;
  }
#if defined(DEBUG)
  for (i = 0; i < SDDS_dataset->layout.n_parameters; i++)
    if (flag[i])
      fprintf(stderr, "parameter %s matched\n", SDDS_dataset->layout.parameter_definition[i].name);
#endif

  for (i = matches = 0; i < SDDS_dataset->layout.n_parameters; i++) {
    if (flag[i])
      matches++;
  }
  if (!matches || !nameReturn)
    return matches;
  if (!((*nameReturn) = (char **)SDDS_Malloc(matches * sizeof(**nameReturn)))) {
    SDDS_SetError("Memory allocation failure (SDDS_MatchParameters)");
    return -1;
  }
  for (i = j = 0; i < SDDS_dataset->layout.n_parameters; i++) {
    if (flag[i]) {
      if (!SDDS_CopyString((*nameReturn) + j, SDDS_dataset->layout.parameter_definition[i].name)) {
        SDDS_SetError("String copy failure (SDDS_MatchParameters)");
        return -1;
      }
      j++;
    }
  }

  return matches;
}

/**
 * @brief Matches and retrieves array names from an SDDS dataset based on specified criteria.
 *
 * This function selects arrays from the provided SDDS dataset according to the specified matching mode and type mode. It supports various calling conventions depending on the matching criteria.
 *
 * The function supports the following matching modes:
 * - **SDDS_NAME_ARRAY**:
 *   - **Parameters**: `int32_t n_entries`, `char **name`
 *   - **Description**: Matches arrays whose names are present in the provided array.
 *
 * - **SDDS_NAMES_STRING**:
 *   - **Parameters**: `char *names`
 *   - **Description**: Matches arrays whose names are specified in a single comma-separated string.
 *
 * - **SDDS_NAME_STRINGS**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Matches arrays whose names are specified as individual string arguments, terminated by a `NULL` pointer.
 *
 * - **SDDS_MATCH_STRING**:
 *   - **Parameters**: `char *name`, `int32_t logic_mode`
 *   - **Description**: Matches arrays based on a wildcard pattern provided in `name`, using the specified logical mode.
 *
 * - **SDDS_MATCH_EXCLUDE_STRING**:
 *   - **Parameters**: `char *name`, `char *exclude`, `int32_t logic_mode`
 *   - **Description**: Matches arrays based on a wildcard pattern provided in `name`, excluding those that match the `exclude` pattern, using the specified logical mode.
 *
 * Additionally, the `typeMode` parameter allows filtering based on array types, such as numeric, floating, or integer types.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure containing the dataset.
 *
 * @param[out] nameReturn
 *     Pointer to a `char**` that will be allocated and populated with the names of the matched arrays. The caller is responsible for freeing the allocated memory.
 *
 * @param[in] matchMode
 *     Specifies the matching mode (e.g., `SDDS_NAME_ARRAY`, `SDDS_NAMES_STRING`, etc.).
 *
 * @param[in] typeMode
 *     Specifies the type matching mode (e.g., `FIND_SPECIFIED_TYPE`, `FIND_NUMERIC_TYPE`, `FIND_FLOATING_TYPE`, `FIND_INTEGER_TYPE`).
 *
 * @param[in] ...
 *     Variable arguments depending on `matchMode`:
 *     - **SDDS_NAME_ARRAY**: `int32_t n_entries`, `char **name`
 *     - **SDDS_NAMES_STRING**: `char *names`
 *     - **SDDS_NAME_STRINGS**: `char *name1, char *name2, ..., NULL`
 *     - **SDDS_MATCH_STRING**: `char *name`, `int32_t logic_mode`
 *     - **SDDS_MATCH_EXCLUDE_STRING**: `char *name`, `char *exclude`, `int32_t logic_mode`
 *
 * @return
 *     - Returns the number of matched arrays on success.
 *     - Returns `-1` if an error occurs (e.g., invalid parameters, memory allocation failure).
 *
 * @note
 *     - The function internally manages memory for the matching process and allocates memory for `nameReturn`, which must be freed by the caller using appropriate memory deallocation functions.
 *     - The dataset must be properly initialized and contain a valid layout before calling this function.
 *
 * @warning
 *     - Ensure that the variable arguments match the expected parameters for the specified `matchMode`.
 *     - The caller is responsible for freeing the memory allocated for `nameReturn` to avoid memory leaks.
 *
 * @sa SDDS_MatchColumns, SDDS_MatchParameters, SDDS_SetError
 */
int32_t SDDS_MatchArrays(SDDS_DATASET *SDDS_dataset, char ***nameReturn, int32_t matchMode, int32_t typeMode, ...)
/* This routine has 4 calling modes:
 * SDDS_MatchArrays(&SDDS_dataset, &matchName, SDDS_NAME_ARRAY  , int32_t typeMode [,long type], int32_t n_entries, char **name)
 * SDDS_MatchArrays(&SDDS_dataset, &matchName, SDDS_NAMES_STRING, int32_t typeMode [,long type], char *names)
 * SDDS_MatchArrays(&SDDS_dataset, &matchName, SDDS_NAME_STRINGS, int32_t typeMode [,long type], char *name1, char *name2, ..., NULL )
 * SDDS_MatchArrays(&SDDS_dataset, &matchName, SDDS_MATCH_STRING, int32_t typeMode [,long type], char *name, int32_t logic_mode)
 * SDDS_MatchArrays(&SDDS_dataset, &matchName, SDDS_MATCH_EXCLUDE_STRING, int32_t typeMode [,long type], char *name, char *exclude, int32_t logic_mode)
 */
{
  static int32_t flags = 0, *flag = NULL;
  char **name, *string, *match_string, *ptr, *exclude_string;
  va_list argptr;
  int32_t i, j, index, n_names, retval, requiredType, matches;
  /*  int32_t type; */
  int32_t local_memory; /* (0,1,2) --> (none, pointer array, pointer array + strings) locally allocated */
  char buffer[SDDS_MAXLINE];
  int32_t logic;

  name = NULL;
  match_string = exclude_string = NULL;
  n_names = requiredType = local_memory = logic = 0;

  matches = -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MatchArrays"))
    return -1;
  if (nameReturn)
    *nameReturn = NULL;

  retval = 1;
  va_start(argptr, typeMode);
  if (typeMode == FIND_SPECIFIED_TYPE)
    requiredType = va_arg(argptr, int32_t);
  switch (matchMode) {
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
    while ((SDDS_GetToken(string, buffer, SDDS_MAXLINE) > 0)) {
      if (!(name = SDDS_Realloc(name, sizeof(*name) * (n_names + 1))) || !SDDS_CopyString(name + n_names, buffer)) {
        SDDS_SetError("Unable to process array selection--memory allocation failure (SDDS_MatchArrays)");
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
        SDDS_SetError("Unable to process array selection--memory allocation failure (SDDS_MatchArrays)");
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
      SDDS_SetError("Unable to process array selection--invalid matching string (SDDS_MatchArrays)");
      retval = 0;
      break;
    }
    match_string = expand_ranges(string);
    logic = va_arg(argptr, int32_t);
    break;
  case SDDS_MATCH_EXCLUDE_STRING:
    local_memory = 0;
    n_names = 1;
    if (!(string = va_arg(argptr, char *))) {
      SDDS_SetError("Unable to process array selection--invalid matching string (SDDS_MatchArrays)");
      retval = 0;
      break;
    }
    match_string = expand_ranges(string);
    if (!(string = va_arg(argptr, char *))) {
      SDDS_SetError("Unable to process array exclusion--invalid matching string (SDDS_MatchArrays)");
      retval = 0;
      break;
    }
    exclude_string = expand_ranges(string);
    logic = va_arg(argptr, int32_t);
    break;
  default:
    SDDS_SetError("Unable to process array selection--unknown match mode (SDDS_MatchArrays)");
    retval = 0;
    break;
  }
  va_end(argptr);
  if (retval == 0)
    return -1;

  if (n_names == 0) {
    SDDS_SetError("Unable to process array selection--no names in call (SDDS_MatchArrays)");
    return -1;
  }

  if (SDDS_dataset->layout.n_arrays != flags) {
    flags = SDDS_dataset->layout.n_arrays;
    if (flag)
      free(flag);
    if (!(flag = (int32_t *)calloc(flags, sizeof(*flag)))) {
      SDDS_SetError("Memory allocation failure (SDDS_MatchArrays)");
      return -1;
    }
  }

  if ((matchMode != SDDS_MATCH_STRING) && (matchMode != SDDS_MATCH_EXCLUDE_STRING)) {
    for (i = 0; i < n_names; i++) {
      if ((index = SDDS_GetArrayIndex(SDDS_dataset, name[i])) >= 0)
        flag[index] = 1;
      else
        flag[index] = 0;
    }
  } else {
    for (i = 0; i < SDDS_dataset->layout.n_arrays; i++) {
      if (SDDS_Logic(flag[i], wild_match(SDDS_dataset->layout.array_definition[i].name, match_string), logic)) {
        if (exclude_string != NULL) {
          if (SDDS_Logic(flag[i], wild_match(SDDS_dataset->layout.array_definition[i].name, exclude_string), logic))
            flag[i] = 0;
          else
            flag[i] = 1;
        } else {
          flag[i] = 1;
        }
      } else {
#if defined(DEBUG)
        fprintf(stderr, "no logic match of %s to %s\n", SDDS_dataset->layout.array_definition[i].name, match_string);
#endif
        flag[i] = 0;
      }
    }
  }
  if (match_string)
    free(match_string);
  if (exclude_string)
    free(exclude_string);
#if defined(DEBUG)
  for (i = 0; i < SDDS_dataset->layout.n_arrays; i++)
    fprintf(stderr, "flag[%" PRId32 "] = %" PRId32 " : %s\n", i, flag[i], SDDS_dataset->layout.array_definition[i].name);
#endif

  if (local_memory == 2) {
    for (i = 0; i < n_names; i++)
      free(name[i]);
  }
  if (local_memory >= 1)
    free(name);

  switch (typeMode) {
  case FIND_SPECIFIED_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_arrays; i++)
      if (SDDS_dataset->layout.array_definition[i].type != requiredType)
        flag[i] = 0;
    break;
  case FIND_NUMERIC_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_arrays; i++)
      if (!SDDS_NUMERIC_TYPE(SDDS_dataset->layout.array_definition[i].type))
        flag[i] = 0;
    break;
  case FIND_FLOATING_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_arrays; i++)
      if (!SDDS_FLOATING_TYPE(SDDS_dataset->layout.array_definition[i].type))
        flag[i] = 0;
    break;
  case FIND_INTEGER_TYPE:
    for (i = 0; i < SDDS_dataset->layout.n_arrays; i++)
      if (!SDDS_INTEGER_TYPE(SDDS_dataset->layout.array_definition[i].type))
        flag[i] = 0;
    break;
  default:
    break;
  }
#if defined(DEBUG)
  for (i = 0; i < SDDS_dataset->layout.n_arrays; i++)
    if (flag[i])
      fprintf(stderr, "array %s matched\n", SDDS_dataset->layout.array_definition[i].name);
#endif

  for (i = matches = 0; i < SDDS_dataset->layout.n_arrays; i++) {
    if (flag[i])
      matches++;
  }
  if (!matches || !nameReturn)
    return matches;
  if (!((*nameReturn) = (char **)SDDS_Malloc(matches * sizeof(**nameReturn)))) {
    SDDS_SetError("Memory allocation failure (SDDS_MatchArrays)");
    return -1;
  }
  for (i = j = 0; i < SDDS_dataset->layout.n_arrays; i++) {
    if (flag[i]) {
      if (!SDDS_CopyString((*nameReturn) + j, SDDS_dataset->layout.array_definition[i].name)) {
        SDDS_SetError("String copy failure (SDDS_MatchArrays)");
        return -1;
      }
      j++;
    }
  }

  return matches;
}

/**
 * @brief Finds the first column in the SDDS dataset that matches the specified criteria.
 *
 * This function searches through the columns of the provided SDDS dataset and returns the name of the first column that matches the given criteria based on the specified mode.
 *
 * The function supports the following modes:
 * - **FIND_SPECIFIED_TYPE**:
 *   - **Parameters**: `int32_t type`, `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first column with a specified type among the provided column names.
 *
 * - **FIND_ANY_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first column with any type among the provided column names.
 *
 * - **FIND_NUMERIC_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first column with a numeric type among the provided column names.
 *
 * - **FIND_FLOATING_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first column with a floating type among the provided column names.
 *
 * - **FIND_INTEGER_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first column with an integer type among the provided column names.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure containing the dataset.
 *
 * @param[in] mode
 *     Specifies the mode for matching columns. Valid modes are:
 *     - `FIND_SPECIFIED_TYPE`
 *     - `FIND_ANY_TYPE`
 *     - `FIND_NUMERIC_TYPE`
 *     - `FIND_FLOATING_TYPE`
 *     - `FIND_INTEGER_TYPE`
 *
 * @param[in] ...
 *     Variable arguments depending on `mode`:
 *     - **FIND_SPECIFIED_TYPE**: `int32_t type`, followed by a list of column names (`char *name1, char *name2, ..., NULL`)
 *     - **Other Modes**: A list of column names (`char *name1, char *name2, ..., NULL`)
 *
 * @return
 *     - On success, returns a dynamically allocated string containing the name of the first matched column.
 *     - Returns `NULL` if no matching column is found or if an error occurs (e.g., memory allocation failure).
 *
 * @note
 *     - The caller is responsible for freeing the memory allocated for the returned string using `free()` or an appropriate memory deallocation function.
 *     - Ensure that the SDDS dataset is properly initialized and contains columns before calling this function.
 *
 * @warning
 *     - Ensure that the variable arguments match the expected parameters for the specified `mode`.
 *     - Failure to free the returned string may lead to memory leaks.
 *
 * @sa SDDS_FindParameter, SDDS_FindArray, SDDS_MatchColumns, SDDS_SetError
 */
char *SDDS_FindColumn(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  /*
    SDDS_DATASET *SDDS_dataset, FIND_SPECIFIED_TYPE, int32_t type, char*, ..., NULL)
    SDDS_DATASET *SDDS_dataset, FIND_ANY_TYPE, char*, ..., NULL)
  */
  int32_t index;
  int32_t error, type, thisType;
  va_list argptr;
  char *name, *buffer;

  va_start(argptr, mode);
  buffer = NULL;
  error = type = 0;

  if (mode == FIND_SPECIFIED_TYPE)
    type = va_arg(argptr, int32_t);
  while ((name = va_arg(argptr, char *))) {
    if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) >= 0) {
      thisType = SDDS_GetColumnType(SDDS_dataset, index);
      if (mode == FIND_ANY_TYPE || (mode == FIND_SPECIFIED_TYPE && thisType == type) || (mode == FIND_NUMERIC_TYPE && SDDS_NUMERIC_TYPE(thisType)) || (mode == FIND_FLOATING_TYPE && SDDS_FLOATING_TYPE(thisType)) || (mode == FIND_INTEGER_TYPE && SDDS_INTEGER_TYPE(thisType))) {
        if (!SDDS_CopyString(&buffer, name)) {
          SDDS_SetError("unable to return string from SDDS_FindColumn");
          error = 1;
          break;
        }
        error = 0;
        break;
      }
    }
  }
  va_end(argptr);
  if (error)
    return NULL;
  return buffer;
}

/**
 * @brief Finds the first parameter in the SDDS dataset that matches the specified criteria.
 *
 * This function searches through the parameters of the provided SDDS dataset and returns the name of the first parameter that matches the given criteria based on the specified mode.
 *
 * The function supports the following modes:
 * - **FIND_SPECIFIED_TYPE**:
 *   - **Parameters**: `int32_t type`, `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first parameter with a specified type among the provided parameter names.
 *
 * - **FIND_ANY_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first parameter with any type among the provided parameter names.
 *
 * - **FIND_NUMERIC_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first parameter with a numeric type among the provided parameter names.
 *
 * - **FIND_FLOATING_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first parameter with a floating type among the provided parameter names.
 *
 * - **FIND_INTEGER_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first parameter with an integer type among the provided parameter names.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure containing the dataset.
 *
 * @param[in] mode
 *     Specifies the mode for matching parameters. Valid modes are:
 *     - `FIND_SPECIFIED_TYPE`
 *     - `FIND_ANY_TYPE`
 *     - `FIND_NUMERIC_TYPE`
 *     - `FIND_FLOATING_TYPE`
 *     - `FIND_INTEGER_TYPE`
 *
 * @param[in] ...
 *     Variable arguments depending on `mode`:
 *     - **FIND_SPECIFIED_TYPE**: `int32_t type`, followed by a list of parameter names (`char *name1, char *name2, ..., NULL`)
 *     - **Other Modes**: A list of parameter names (`char *name1, char *name2, ..., NULL`)
 *
 * @return
 *     - On success, returns a dynamically allocated string containing the name of the first matched parameter.
 *     - Returns `NULL` if no matching parameter is found or if an error occurs (e.g., memory allocation failure).
 *
 * @note
 *     - The caller is responsible for freeing the memory allocated for the returned string using `free()` or an appropriate memory deallocation function.
 *     - Ensure that the SDDS dataset is properly initialized and contains parameters before calling this function.
 *
 * @warning
 *     - Ensure that the variable arguments match the expected parameters for the specified `mode`.
 *     - Failure to free the returned string may lead to memory leaks.
 *
 * @sa SDDS_FindColumn, SDDS_FindArray, SDDS_MatchParameters, SDDS_SetError
 */
char *SDDS_FindParameter(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  int32_t index, error, type, thisType;
  va_list argptr;
  char *name, *buffer;

  va_start(argptr, mode);
  buffer = NULL;
  error = type = 0;

  if (mode == FIND_SPECIFIED_TYPE)
    type = va_arg(argptr, int32_t);
  while ((name = va_arg(argptr, char *))) {
    if ((index = SDDS_GetParameterIndex(SDDS_dataset, name)) >= 0) {
      thisType = SDDS_GetParameterType(SDDS_dataset, index);
      if (mode == FIND_ANY_TYPE || (mode == FIND_SPECIFIED_TYPE && thisType == type) || (mode == FIND_NUMERIC_TYPE && SDDS_NUMERIC_TYPE(thisType)) || (mode == FIND_FLOATING_TYPE && SDDS_FLOATING_TYPE(thisType)) || (mode == FIND_INTEGER_TYPE && SDDS_INTEGER_TYPE(thisType))) {
        if (!SDDS_CopyString(&buffer, name)) {
          SDDS_SetError("unable to return string from SDDS_FindParameter");
          error = 1;
          break;
        }
        error = 0;
        break;
      }
    }
  }
  va_end(argptr);
  if (error)
    return NULL;
  return buffer;
}

/**
 * @brief Finds the first array in the SDDS dataset that matches the specified criteria.
 *
 * This function searches through the arrays of the provided SDDS dataset and returns the name of the first array that matches the given criteria based on the specified mode.
 *
 * The function supports the following modes:
 * - **FIND_SPECIFIED_TYPE**:
 *   - **Parameters**: `int32_t type`, `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first array with a specified type among the provided array names.
 *
 * - **FIND_ANY_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first array with any type among the provided array names.
 *
 * - **FIND_NUMERIC_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first array with a numeric type among the provided array names.
 *
 * - **FIND_FLOATING_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first array with a floating type among the provided array names.
 *
 * - **FIND_INTEGER_TYPE**:
 *   - **Parameters**: `char *name1, char *name2, ..., NULL`
 *   - **Description**: Finds the first array with an integer type among the provided array names.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure containing the dataset.
 *
 * @param[in] mode
 *     Specifies the mode for matching arrays. Valid modes are:
 *     - `FIND_SPECIFIED_TYPE`
 *     - `FIND_ANY_TYPE`
 *     - `FIND_NUMERIC_TYPE`
 *     - `FIND_FLOATING_TYPE`
 *     - `FIND_INTEGER_TYPE`
 *
 * @param[in] ...
 *     Variable arguments depending on `mode`:
 *     - **FIND_SPECIFIED_TYPE**: `int32_t type`, followed by a list of array names (`char *name1, char *name2, ..., NULL`)
 *     - **Other Modes**: A list of array names (`char *name1, char *name2, ..., NULL`)
 *
 * @return
 *     - On success, returns a dynamically allocated string containing the name of the first matched array.
 *     - Returns `NULL` if no matching array is found or if an error occurs (e.g., memory allocation failure).
 *
 * @note
 *     - The caller is responsible for freeing the memory allocated for the returned string using `free()` or an appropriate memory deallocation function.
 *     - Ensure that the SDDS dataset is properly initialized and contains arrays before calling this function.
 *
 * @warning
 *     - Ensure that the variable arguments match the expected parameters for the specified `mode`.
 *     - Failure to free the returned string may lead to memory leaks.
 *
 * @sa SDDS_FindColumn, SDDS_FindParameter, SDDS_MatchArrays, SDDS_SetError
 */
char *SDDS_FindArray(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  int32_t index, error, type, thisType;
  va_list argptr;
  char *name, *buffer;

  va_start(argptr, mode);
  buffer = NULL;
  error = type = 0;

  if (mode == FIND_SPECIFIED_TYPE)
    type = va_arg(argptr, int32_t);
  while ((name = va_arg(argptr, char *))) {
    if ((index = SDDS_GetArrayIndex(SDDS_dataset, name)) >= 0) {
      thisType = SDDS_GetArrayType(SDDS_dataset, index);
      if (mode == FIND_ANY_TYPE || (mode == FIND_SPECIFIED_TYPE && thisType == type) || (mode == FIND_NUMERIC_TYPE && SDDS_NUMERIC_TYPE(thisType)) || (mode == FIND_FLOATING_TYPE && SDDS_FLOATING_TYPE(thisType)) || (mode == FIND_INTEGER_TYPE && SDDS_INTEGER_TYPE(thisType))) {
        if (!SDDS_CopyString(&buffer, name)) {
          SDDS_SetError("unable to return string from SDDS_FindArray");
          error = 1;
          break;
        }
        error = 0;
        break;
      }
    }
  }
  va_end(argptr);
  if (error)
    return NULL;
  return buffer;
}

/**
 * @brief Checks if a column exists in the SDDS dataset with the specified name, units, and type.
 *
 * This function verifies whether a column with the given name exists in the SDDS dataset and optionally checks if its units and type match the specified criteria.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset to be checked.
 *
 * @param[in] name
 *     The name of the column to check.
 *
 * @param[in] units
 *     The units of the column. May be `NULL` if units are not to be checked.
 *
 * @param[in] type
 *     Specifies the expected type of the column. Valid values are:
 *     - `SDDS_ANY_NUMERIC_TYPE`
 *     - `SDDS_ANY_FLOATING_TYPE`
 *     - `SDDS_ANY_INTEGER_TYPE`
 *     - `0` (if type is to be ignored)
 *
 * @param[in] fp_message
 *     File pointer where error messages will be sent. Typically, this is `stderr`.
 *
 * @return
 *     - `SDDS_CHECK_OKAY` if the column exists and matches the specified criteria.
 *     - `SDDS_CHECK_NONEXISTENT` if the column does not exist.
 *     - `SDDS_CHECK_WRONGTYPE` if the column exists but does not match the specified type.
 *     - `SDDS_CHECK_WRONGUNITS` if the column exists but does not match the specified units.
 *
 * @note
 *     - If `units` is `NULL`, the function does not check for units.
 *     - The function retrieves the column's units and type using `SDDS_GetColumnInformation` and `SDDS_GetColumnType`.
 *
 * @warning
 *     - Ensure that the SDDS dataset is properly initialized and contains columns before calling this function.
 *     - The function may set error messages using `SDDS_SetError` if it encounters issues accessing column information.
 *
 * @sa SDDS_CheckParameter, SDDS_GetColumnIndex, SDDS_SetError
 */
int32_t SDDS_CheckColumn(SDDS_DATASET *SDDS_dataset, char *name, char *units, int32_t type, FILE *fp_message) {
  char *units1;
  int32_t index;
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) < 0)
    return (SDDS_PrintCheckText(fp_message, name, units, type, "column", SDDS_CHECK_NONEXISTENT));
  if (SDDS_VALID_TYPE(type)) {
    if (type != SDDS_GetColumnType(SDDS_dataset, index))
      return (SDDS_PrintCheckText(fp_message, name, units, type, "column", SDDS_CHECK_WRONGTYPE));
  } else {
    switch (type) {
    case 0:
      break;
    case SDDS_ANY_NUMERIC_TYPE:
      if (!SDDS_NUMERIC_TYPE(SDDS_GetColumnType(SDDS_dataset, index)))
        return (SDDS_PrintCheckText(fp_message, name, units, type, "column", SDDS_CHECK_WRONGTYPE));
      break;
    case SDDS_ANY_FLOATING_TYPE:
      if (!SDDS_FLOATING_TYPE(SDDS_GetColumnType(SDDS_dataset, index))) {
        return (SDDS_PrintCheckText(fp_message, name, units, type, "column", SDDS_CHECK_WRONGTYPE));
      }
      break;
    case SDDS_ANY_INTEGER_TYPE:
      if (!SDDS_INTEGER_TYPE(SDDS_GetColumnType(SDDS_dataset, index))) {
        return (SDDS_PrintCheckText(fp_message, name, units, type, "column", SDDS_CHECK_WRONGTYPE));
      }
      break;
    default:
      return (SDDS_PrintCheckText(fp_message, name, units, type, "column", SDDS_CHECK_WRONGTYPE));
    }
  }
  if (!units) {
    /* don't care about units */
    return SDDS_CHECK_OKAY;
  }
  if (SDDS_GetColumnInformation(SDDS_dataset, "units", &units1, SDDS_GET_BY_NAME, name) != SDDS_STRING) {
    SDDS_SetError("units field of column has wrong data type!");
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (!units1) {
    if (SDDS_StringIsBlank(units))
      return (SDDS_CHECK_OKAY);
    return (SDDS_PrintCheckText(fp_message, name, units, type, "column", SDDS_CHECK_WRONGUNITS));
  }
  if (strcmp(units, units1) == 0) {
    free(units1);
    return (SDDS_CHECK_OKAY);
  }
  free(units1);
  return (SDDS_PrintCheckText(fp_message, name, units, type, "column", SDDS_CHECK_WRONGUNITS));
}

/**
 * @brief Checks if a parameter exists in the SDDS dataset with the specified name, units, and type.
 *
 * This function verifies whether a parameter with the given name exists in the SDDS dataset and optionally checks if its units and type match the specified criteria.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset to be checked.
 *
 * @param[in] name
 *     The name of the parameter to check.
 *
 * @param[in] units
 *     The units of the parameter. May be `NULL` if units are not to be checked.
 *
 * @param[in] type
 *     Specifies the expected type of the parameter. Valid values are:
 *     - `SDDS_ANY_NUMERIC_TYPE`
 *     - `SDDS_ANY_FLOATING_TYPE`
 *     - `SDDS_ANY_INTEGER_TYPE`
 *     - `0` (if type is to be ignored)
 *
 * @param[in] fp_message
 *     File pointer where error messages will be sent. Typically, this is `stderr`.
 *
 * @return
 *     - `SDDS_CHECK_OKAY` if the parameter exists and matches the specified criteria.
 *     - `SDDS_CHECK_NONEXISTENT` if the parameter does not exist.
 *     - `SDDS_CHECK_WRONGTYPE` if the parameter exists but does not match the specified type.
 *     - `SDDS_CHECK_WRONGUNITS` if the parameter exists but does not match the specified units.
 *
 * @note
 *     - If `units` is `NULL`, the function does not check for units.
 *     - The function retrieves the parameter's units and type using `SDDS_GetParameterInformation` and `SDDS_GetParameterType`.
 *
 * @warning
 *     - Ensure that the SDDS dataset is properly initialized and contains parameters before calling this function.
 *     - The function may set error messages using `SDDS_SetError` if it encounters issues accessing parameter information.
 *
 * @sa SDDS_CheckColumn, SDDS_GetParameterIndex, SDDS_SetError
 */
int32_t SDDS_CheckParameter(SDDS_DATASET *SDDS_dataset, char *name, char *units, int32_t type, FILE *fp_message) {
  char *units1;
  int32_t index;
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, name)) < 0)
    return (SDDS_PrintCheckText(fp_message, name, units, type, "parameter", SDDS_CHECK_NONEXISTENT));
  if (SDDS_VALID_TYPE(type)) {
    if (type != SDDS_GetParameterType(SDDS_dataset, index))
      return (SDDS_PrintCheckText(fp_message, name, units, type, "parameter", SDDS_CHECK_WRONGTYPE));
  } else {
    switch (type) {
    case 0:
      break;
    case SDDS_ANY_NUMERIC_TYPE:
      if (!SDDS_NUMERIC_TYPE(SDDS_GetParameterType(SDDS_dataset, index)))
        return (SDDS_PrintCheckText(fp_message, name, units, type, "parameter", SDDS_CHECK_WRONGTYPE));
      break;
    case SDDS_ANY_FLOATING_TYPE:
      if (!SDDS_FLOATING_TYPE(SDDS_GetParameterType(SDDS_dataset, index)))
        return (SDDS_PrintCheckText(fp_message, name, units, type, "parameter", SDDS_CHECK_WRONGTYPE));
      break;
    case SDDS_ANY_INTEGER_TYPE:
      if (!SDDS_INTEGER_TYPE(SDDS_GetParameterType(SDDS_dataset, index)))
        return (SDDS_PrintCheckText(fp_message, name, units, type, "parameter", SDDS_CHECK_WRONGTYPE));
      break;
    default:
      return (SDDS_PrintCheckText(fp_message, name, units, type, "parameter", SDDS_CHECK_WRONGTYPE));
    }
  }
  if (!units) {
    /* don't care about units */
    return (SDDS_CHECK_OKAY);
  }
  if (SDDS_GetParameterInformation(SDDS_dataset, "units", &units1, SDDS_GET_BY_NAME, name) != SDDS_STRING) {
    SDDS_SetError("units field of parameter has wrong data type!");
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (!units1) {
    if (SDDS_StringIsBlank(units))
      return (SDDS_CHECK_OKAY);
    return (SDDS_PrintCheckText(fp_message, name, units, type, "parameter", SDDS_CHECK_WRONGUNITS));
  }
  if (strcmp(units, units1) == 0) {
    free(units1);
    return (SDDS_CHECK_OKAY);
  }
  free(units1);
  return (SDDS_PrintCheckText(fp_message, name, units, type, "parameter", SDDS_CHECK_WRONGUNITS));
}

/**
 * @brief Checks if an array exists in the SDDS dataset with the specified name, units, and type.
 *
 * This function verifies whether an array with the given name exists within the provided SDDS dataset. Additionally, it can check if the array's units and type match the specified criteria.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset to be checked.
 *
 * @param[in] name
 *     The name of the array to check.
 *
 * @param[in] units
 *     The units of the array. This parameter may be `NULL` if units are not to be validated.
 *
 * @param[in] type
 *     Specifies the expected type of the array. Valid values are:
 *     - `SDDS_ANY_NUMERIC_TYPE`
 *     - `SDDS_ANY_FLOATING_TYPE`
 *     - `SDDS_ANY_INTEGER_TYPE`
 *     - `0` (if type is to be ignored)
 *
 * @param[in] fp_message
 *     File pointer where error messages will be sent. Typically, this is `stderr`.
 *
 * @return
 *     - `SDDS_CHECK_OKAY` if the array exists and matches the specified criteria.
 *     - `SDDS_CHECK_NONEXISTENT` if the array does not exist.
 *     - `SDDS_CHECK_WRONGTYPE` if the array exists but does not match the specified type.
 *     - `SDDS_CHECK_WRONGUNITS` if the array exists but does not match the specified units.
 *
 * @note
 *     - If `units` is `NULL`, the function does not perform units validation.
 *     - The function retrieves the array's units and type using `SDDS_GetArrayInformation` and `SDDS_GetArrayType`.
 *
 * @warning
 *     - Ensure that the SDDS dataset is properly initialized and contains arrays before calling this function.
 *     - The function may set error messages using `SDDS_SetError` if it encounters issues accessing array information.
 *
 * @sa SDDS_CheckColumn, SDDS_CheckParameter, SDDS_PrintCheckText, SDDS_SetError
 */
int32_t SDDS_CheckArray(SDDS_DATASET *SDDS_dataset, char *name, char *units, int32_t type, FILE *fp_message) {
  char *units1;
  int32_t index;
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, name)) < 0)
    return (SDDS_PrintCheckText(fp_message, name, units, type, "array", SDDS_CHECK_NONEXISTENT));
  if (SDDS_VALID_TYPE(type)) {
    if (type != SDDS_GetArrayType(SDDS_dataset, index))
      return (SDDS_PrintCheckText(fp_message, name, units, type, "array", SDDS_CHECK_WRONGTYPE));
  } else {
    switch (type) {
    case 0:
      break;
    case SDDS_ANY_NUMERIC_TYPE:
      if (!SDDS_NUMERIC_TYPE(SDDS_GetArrayType(SDDS_dataset, index)))
        return (SDDS_PrintCheckText(fp_message, name, units, type, "array", SDDS_CHECK_WRONGTYPE));
      break;
    case SDDS_ANY_FLOATING_TYPE:
      if (!SDDS_FLOATING_TYPE(SDDS_GetArrayType(SDDS_dataset, index)))
        return (SDDS_PrintCheckText(fp_message, name, units, type, "array", SDDS_CHECK_WRONGTYPE));
      break;
    case SDDS_ANY_INTEGER_TYPE:
      if (!SDDS_INTEGER_TYPE(SDDS_GetArrayType(SDDS_dataset, index)))
        return (SDDS_PrintCheckText(fp_message, name, units, type, "array", SDDS_CHECK_WRONGTYPE));
      break;
    default:
      return (SDDS_PrintCheckText(fp_message, name, units, type, "array", SDDS_CHECK_WRONGTYPE));
    }
  }
  if (SDDS_GetArrayInformation(SDDS_dataset, "units", &units1, SDDS_GET_BY_NAME, name) != SDDS_STRING) {
    SDDS_SetError("units field of array has wrong data type!");
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (!units) {
    /* don't care about units */
    return (SDDS_CHECK_OKAY);
  }
  if (!units1) {
    if (SDDS_StringIsBlank(units))
      return (SDDS_CHECK_OKAY);
    return (SDDS_CHECK_OKAY);
  }
  if (strcmp(units, units1) == 0) {
    free(units1);
    return (SDDS_CHECK_OKAY);
  }
  free(units1);
  return (SDDS_PrintCheckText(fp_message, name, units, type, "array", SDDS_CHECK_WRONGUNITS));
}

/**
 * @brief Prints detailed error messages related to SDDS entity checks.
 *
 * This function outputs error messages to the specified file pointer based on the provided error code. It is primarily used by functions like `SDDS_CheckColumn`, `SDDS_CheckParameter`, and `SDDS_CheckArray` to report issues during validation checks.
 *
 * @param[in] fp
 *     File pointer where the error messages will be printed. Typically, this is `stderr`.
 *
 * @param[in] name
 *     The name of the SDDS entity (e.g., column, parameter, array) being checked.
 *
 * @param[in] units
 *     The expected units of the SDDS entity. May be `NULL` if units are not relevant.
 *
 * @param[in] type
 *     The expected type code of the SDDS entity. This can be a specific type or a general category like `SDDS_ANY_NUMERIC_TYPE`.
 *
 * @param[in] class_name
 *     A string representing the class of the SDDS entity (e.g., "column", "parameter", "array").
 *
 * @param[in] error_code
 *     The specific error code indicating the type of error encountered. Valid values include:
 *     - `SDDS_CHECK_OKAY`
 *     - `SDDS_CHECK_NONEXISTENT`
 *     - `SDDS_CHECK_WRONGTYPE`
 *     - `SDDS_CHECK_WRONGUNITS`
 *
 * @return
 *     Returns the same `error_code` that was passed as an argument.
 *
 * @note
 *     - This function assumes that `registeredProgramName` is a globally accessible string containing the name of the program for contextual error messages.
 *     - Ensure that `fp`, `name`, and `class_name` are not `NULL` to prevent undefined behavior.
 *
 * @warning
 *     - Passing invalid `error_code` values that are not handled in the switch statement will result in a generic error message being printed to `stderr`.
 *
 * @sa SDDS_CheckColumn, SDDS_CheckParameter, SDDS_CheckArray
 */
int32_t SDDS_PrintCheckText(FILE *fp, char *name, char *units, int32_t type, char *class_name, int32_t error_code) {
  if (!fp || !name || !class_name)
    return (error_code);
  switch (error_code) {
  case SDDS_CHECK_OKAY:
    break;
  case SDDS_CHECK_NONEXISTENT:
    fprintf(fp, "Problem with %s %s: nonexistent (%s)\n", class_name, name, registeredProgramName ? registeredProgramName : "?");
    break;
  case SDDS_CHECK_WRONGTYPE:
    if (SDDS_VALID_TYPE(type))
      fprintf(fp, "Problem with %s %s: wrong data type--expected %s (%s)\n", class_name, name, SDDS_type_name[type - 1], registeredProgramName ? registeredProgramName : "?");
    else if (type == SDDS_ANY_NUMERIC_TYPE)
      fprintf(fp, "Problem with %s %s: wrong data type--expected numeric data (%s)\n", class_name, name, registeredProgramName ? registeredProgramName : "?");
    else if (type == SDDS_ANY_FLOATING_TYPE)
      fprintf(fp, "Problem with %s %s: wrong data type--expected floating point data (%s)\n", class_name, name, registeredProgramName ? registeredProgramName : "?");
    else if (type == SDDS_ANY_INTEGER_TYPE)
      fprintf(fp, "Problem with %s %s: wrong data type--expected integer data (%s)\n", class_name, name, registeredProgramName ? registeredProgramName : "?");
    else if (type)
      fprintf(fp, "Problem with %s %s: invalid data type code seen---may be a programming error (%s)\n", class_name, name, registeredProgramName ? registeredProgramName : "?");
    break;
  case SDDS_CHECK_WRONGUNITS:
    fprintf(fp, "Problem with %s %s: wrong units--expected %s (%s)\n", class_name, name, units ? units : "none", registeredProgramName ? registeredProgramName : "?");
    break;
  default:
    fprintf(stderr, "Problem with call to SDDS_PrintCheckText--invalid error code (%s)\n", registeredProgramName ? registeredProgramName : "?");
    return (SDDS_CHECK_OKAY);
  }
  return (error_code);
}

/**
 * @brief Deletes fixed values from all parameters in the SDDS dataset.
 *
 * This function iterates through all parameters in the provided SDDS dataset and removes any fixed values associated with them. It ensures that both the current layout and the original layout of the dataset have their fixed values cleared.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset from which fixed values will be deleted.
 *
 * @return
 *     - `1` on successful deletion of all fixed values.
 *     - `0` if the dataset check fails or if saving the layout fails.
 *
 * @note
 *     - The function requires that the SDDS dataset is properly initialized and that it contains parameters with fixed values.
 *     - Both the current layout and the original layout are updated to remove fixed values.
 *
 * @warning
 *     - This operation cannot be undone. Ensure that fixed values are no longer needed before calling this function.
 *     - Improper handling of memory allocations related to fixed values may lead to memory leaks or undefined behavior.
 *
 * @sa SDDS_CheckDataset, SDDS_SaveLayout
 */
int32_t SDDS_DeleteParameterFixedValues(SDDS_DATASET *SDDS_dataset) {
  int32_t i;
  SDDS_LAYOUT *layout, *orig_layout;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DeleteFixedValueParameters"))
    return 0;
  if (!SDDS_SaveLayout(SDDS_dataset))
    return 0;
  layout = &SDDS_dataset->layout;
  orig_layout = &SDDS_dataset->original_layout;
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].fixed_value)
      free(layout->parameter_definition[i].fixed_value);
    if (orig_layout->parameter_definition[i].fixed_value && (!layout->parameter_definition[i].fixed_value || orig_layout->parameter_definition[i].fixed_value != layout->parameter_definition[i].fixed_value))
      free(orig_layout->parameter_definition[i].fixed_value);
    orig_layout->parameter_definition[i].fixed_value = NULL;
    layout->parameter_definition[i].fixed_value = NULL;
  }
  return 1;
}

/**
 * @brief Sets the data mode (ASCII or Binary) for the SDDS dataset.
 *
 * This function configures the data mode of the SDDS dataset to either ASCII or Binary. When setting to Binary mode with byte swapping (using `-SDDS_BINARY`), it adjusts the byte order based on the machine's endianness to ensure compatibility.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset whose data mode is to be set.
 *
 * @param[in] newmode
 *     The desired data mode. Valid values are:
 *     - `SDDS_ASCII` for ASCII mode.
 *     - `SDDS_BINARY` for Binary mode.
 *     - `-SDDS_BINARY` for Binary mode with byte swapping (for compatibility with systems like the `sddsendian` program).
 *
 * @return
 *     - `1` on successful mode change.
 *     - `0` if the mode is invalid, if the dataset is `NULL`, or if the dataset has already been written to and the mode cannot be changed.
 *
 * @note
 *     - Changing the data mode is only permitted if no data has been written to the dataset (i.e., `page_number` is `0` and `n_rows_written` is `0`).
 *     - When using `-SDDS_BINARY`, the function automatically determines the appropriate byte order based on the machine's endianness.
 *
 * @warning
 *     - Attempting to change the data mode after writing data to the dataset will result in an error.
 *     - Ensure that the `newmode` parameter is correctly specified to prevent unintended behavior.
 *
 * @sa SDDS_SetError, SDDS_IsBigEndianMachine, SDDS_BINARY, SDDS_ASCII
 */
int32_t SDDS_SetDataMode(SDDS_DATASET *SDDS_dataset, int32_t newmode) {
  if (newmode == -SDDS_BINARY) {
    /* will write with bytes swapped.
       * provided for compatibility with sddsendian program, which writes the
       * data itself 
       */
    SDDS_dataset->layout.byteOrderDeclared = SDDS_IsBigEndianMachine() ? SDDS_LITTLEENDIAN : SDDS_BIGENDIAN;
    newmode = SDDS_BINARY;
  }
  if (newmode != SDDS_ASCII && newmode != SDDS_BINARY) {
    SDDS_SetError("Invalid data mode (SDDS_SetDataMode)");
    return 0;
  }
  if (newmode == SDDS_dataset->layout.data_mode.mode)
    return 1;
  if (!SDDS_dataset) {
    SDDS_SetError("NULL page pointer (SDDS_SetDataMode)");
    return 0;
  }
  if (SDDS_dataset->page_number != 0 && (SDDS_dataset->page_number > 1 || SDDS_dataset->n_rows_written != 0)) {
    SDDS_SetError("Can't change the mode of a file that's been written to (SDDS_SetDataMode)");
    return 0;
  }
  SDDS_dataset->layout.data_mode.mode = SDDS_dataset->original_layout.data_mode.mode = newmode;
  return 1;
}

/**
 * @brief Verifies that the size of the SDDS_DATASET structure matches the expected size.
 *
 * This function ensures that the size of the `SDDS_DATASET` structure used by the program matches the size expected by the SDDS library. This check is crucial to prevent issues related to structure size mismatches, which can occur due to differences in compiler settings or library versions.
 *
 * @param[in] size
 *     The size of the `SDDS_DATASET` structure as determined by the calling program (typically using `sizeof(SDDS_DATASET)`).
 *
 * @return
 *     - `1` if the provided size matches the expected size of the `SDDS_DATASET` structure.
 *     - `0` if there is a size mismatch, indicating potential incompatibility issues.
 *
 * @note
 *     - This function should be called during initialization to ensure structural compatibility between the program and the SDDS library.
 *
 * @warning
 *     - A size mismatch can lead to undefined behavior, including memory corruption and program crashes. Always ensure that both the program and the SDDS library are compiled with compatible settings.
 *
 * @sa SDDS_DATASET, SDDS_SetError
 */
int32_t SDDS_CheckDatasetStructureSize(int32_t size) {
  char buffer[100];
  if (size != sizeof(SDDS_DATASET)) {
    SDDS_SetError("passed size is not equal to expected size for SDDS_DATASET structure");
    sprintf(buffer, "Passed size is %" PRId32 ", library size is %" PRId32 "\n", size, (int32_t)sizeof(SDDS_DATASET));
    SDDS_SetError(buffer);
    return 0;
  }
  return 1;
}

/**
 * @brief Retrieves the number of columns in the SDDS dataset.
 *
 * This function returns the total count of columns defined in the layout of the provided SDDS dataset.
 *
 * @param[in] page
 *     Pointer to the `SDDS_DATASET` structure representing the dataset.
 *
 * @return
 *     - The number of columns (`int32_t`) in the dataset.
 *     - `0` if the provided dataset pointer is `NULL`.
 *
 * @note
 *     - Ensure that the dataset is properly initialized before calling this function.
 *
 * @sa SDDS_GetColumnIndex, SDDS_CheckColumn
 */
int32_t SDDS_ColumnCount(SDDS_DATASET *page) {
  if (!page)
    return 0;
  return page->layout.n_columns;
}

/**
 * @brief Retrieves the number of parameters in the SDDS dataset.
 *
 * This function returns the total count of parameters defined in the layout of the provided SDDS dataset.
 *
 * @param[in] page
 *     Pointer to the `SDDS_DATASET` structure representing the dataset.
 *
 * @return
 *     - The number of parameters (`int32_t`) in the dataset.
 *     - `0` if the provided dataset pointer is `NULL`.
 *
 * @note
 *     - Ensure that the dataset is properly initialized before calling this function.
 *
 * @sa SDDS_GetParameterIndex, SDDS_CheckParameter
 */
int32_t SDDS_ParameterCount(SDDS_DATASET *page) {
  if (!page)
    return 0;
  return page->layout.n_parameters;
}

/**
 * @brief Retrieves the number of arrays in the SDDS dataset.
 *
 * This function returns the total count of arrays defined in the layout of the provided SDDS dataset.
 *
 * @param[in] page
 *     Pointer to the `SDDS_DATASET` structure representing the dataset.
 *
 * @return
 *     - The number of arrays (`int32_t`) in the dataset.
 *     - `0` if the provided dataset pointer is `NULL`.
 *
 * @note
 *     - Ensure that the dataset is properly initialized before calling this function.
 *
 * @sa SDDS_GetArrayIndex, SDDS_CheckArray
 */
int32_t SDDS_ArrayCount(SDDS_DATASET *page) {
  if (!page)
    return 0;
  return page->layout.n_arrays;
}

/*  `\\`, `\\ '`, `\\\"`, `\\a`, `\?` */
/**
 * @brief Interprets and converts escape sequences in a string.
 *
 * This function processes a string containing escape sequences and converts them into their corresponding character representations. Supported escape sequences include:
 * - Standard ANSI escape codes: `\n`, `\t`, `\b`, `\r`, `\f`, `\v`, `\\`, \\', `\"`, `\a`, `\?`
 * - Octal values: `\ddd`
 * - SDDS-specific escape codes: `\!`, `\)`
 *
 * The function modifies the input string `s` in place, replacing escape sequences with their actual character values.
 *
 * @param[in, out] s
 *     Pointer to the null-terminated string to be processed. The string will be modified in place.
 *
 * @note
 *     - Ensure that the input string `s` has sufficient buffer space to accommodate the modified characters, especially when dealing with octal escape sequences that may reduce the overall string length.
 *
 * @warning
 *     - The function does not perform bounds checking. Ensure that the input string is properly null-terminated to prevent undefined behavior.
 *     - Unrecognized escape sequences (other than the ones specified) will result in the backslash being retained in the string.
 *
 * @sa SDDS_EscapeNewlines, SDDS_UnescapeNewlines
 */
void SDDS_InterpretEscapes(char *s)
/* \ddd = octal value
 * ANSI escape codes (n t b r f v \ ' " a ?
 * SDDS escape codes (! )
 */
{
  char *ptr;
  int32_t count;

  ptr = s;
  while (*s) {
    if (*s != '\\')
      *ptr++ = *s++;
    else {
      s++;
      if (!*s) {
        *ptr++ = '\\';
        *ptr++ = 0;
        return;
      }
      switch (*s) {
      case 'n':
        *ptr++ = '\n';
        s++;
        break;
      case 't':
        *ptr++ = '\t';
        s++;
        break;
      case 'b':
        *ptr++ = '\b';
        s++;
        break;
      case 'r':
        *ptr++ = '\r';
        s++;
        break;
      case 'f':
        *ptr++ = '\f';
        s++;
        break;
      case 'v':
        *ptr++ = '\v';
        s++;
        break;
      case '\\':
        *ptr++ = '\\';
        s++;
        break;
      case '\'':
        *ptr++ = '\'';
        s++;
        break;
      case '"':
        *ptr++ = '\"';
        s++;
        break;
      case 'a':
        *ptr++ = '\a';
        s++;
        break;
      case '?':
        *ptr++ = '\?';
        s++;
        break;
      case '!':
        *ptr++ = '!';
        s++;
        break;
      default:
        if (*s >= '0' && *s <= '9') {
          *ptr = 0;
          count = 0;
          while (++count <= 3 && *s >= '0' && *s <= '9')
            *ptr = 8 * (*ptr) + *s++ - '0';
          ptr++;
        } else {
          *ptr++ = '\\';
        }
        break;
      }
    }
  }
  *ptr = 0;
}

#define COMMENT_COMMANDS 3
static char *commentCommandName[COMMENT_COMMANDS] = {
  "big-endian",
  "little-endian",
  "fixed-rowcount",
};

static uint32_t commentCommandFlag[COMMENT_COMMANDS] = {
  SDDS_BIGENDIAN_SEEN,
  SDDS_LITTLEENDIAN_SEEN,
  SDDS_FIXED_ROWCOUNT_SEEN,
};

/**
 * @brief Retrieves the current special comments modes set in the SDDS dataset.
 *
 * This function returns the current set of special comment flags that have been parsed and set within the dataset. These flags indicate the presence of specific configurations such as endianness and fixed row counts.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset.
 *
 * @return
 *     - A `uint32_t` value representing the combined set of special comment flags.
 *     - `0` if no special comments have been set.
 *
 * @note
 *     - Special comment flags are typically set by parsing comment strings using functions like `SDDS_ParseSpecialComments`.
 *
 * @warning
 *     - Ensure that the dataset is properly initialized before calling this function.
 *
 * @sa SDDS_ParseSpecialComments, SDDS_ResetSpecialCommentsModes
 */
uint32_t SDDS_GetSpecialCommentsModes(SDDS_DATASET *SDDS_dataset) {
  return SDDS_dataset->layout.commentFlags;
}

/**
 * @brief Resets the special comments modes in the SDDS dataset.
 *
 * This function clears all special comment flags that have been set within the dataset. After calling this function, the dataset will no longer have any special configurations related to comments.
 *
 * @param[in,out] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset. The `commentFlags` field will be reset to `0`.
 *
 * @note
 *     - Use this function to clear all special configurations before setting new ones or when resetting the dataset's state.
 *
 * @warning
 *     - This operation cannot be undone. Ensure that you no longer require the existing special comment configurations before resetting.
 *
 * @sa SDDS_GetSpecialCommentsModes, SDDS_ParseSpecialComments
 */
void SDDS_ResetSpecialCommentsModes(SDDS_DATASET *SDDS_dataset) {
  SDDS_dataset->layout.commentFlags = 0;
}

/**
 * @brief Parses and processes special comment commands within the SDDS dataset.
 *
 * This function interprets special commands embedded within comment strings of the SDDS dataset. Supported special commands include:
 * - `big-endian`
 * - `little-endian`
 * - `fixed-rowcount`
 *
 * Each recognized command updates the `commentFlags` field within the dataset's layout to reflect the presence of these special configurations.
 *
 * @param[in,out] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset. The `commentFlags` field will be updated based on the parsed commands.
 *
 * @param[in] s
 *     Pointer to the null-terminated string containing special comment commands to be parsed.
 *
 * @note
 *     - This function is intended to be used internally by the SDDS library to handle special configurations specified in comments.
 *     - Only the predefined special commands are recognized and processed.
 *
 * @warning
 *     - Unrecognized commands within the comment string are ignored.
 *     - Ensure that the input string `s` is properly formatted and null-terminated to prevent undefined behavior.
 *
 * @sa SDDS_GetSpecialCommentsModes, SDDS_ResetSpecialCommentsModes
 */
void SDDS_ParseSpecialComments(SDDS_DATASET *SDDS_dataset, char *s) {
  char buffer[SDDS_MAXLINE];
  int32_t i;
  if (SDDS_dataset == NULL)
    return;
  while (SDDS_GetToken(s, buffer, SDDS_MAXLINE) > 0) {
    for (i = 0; i < COMMENT_COMMANDS; i++) {
      if (strcmp(buffer, commentCommandName[i]) == 0) {
        SDDS_dataset->layout.commentFlags |= commentCommandFlag[i];
        break;
      }
    }
  }
}

/**
 * @brief Determines whether the current machine uses big-endian byte ordering.
 *
 * This function checks the byte order of the machine on which the program is running. It returns `1` if the machine is big-endian and `0` if it is little-endian.
 *
 * @return
 *     - `1` if the machine is big-endian.
 *     - `0` if the machine is little-endian.
 *
 * @note
 *     - Endianness detection is based on inspecting the byte order of an integer value.
 *
 * @warning
 *     - This function assumes that `int32_t` is 4 bytes in size. If compiled on a system where `int32_t` differs in size, the behavior may be incorrect.
 *
 * @sa SDDS_SetDataMode
 */
int32_t SDDS_IsBigEndianMachine() {
  int32_t x = 1;
  if (*((char *)&x))
    return 0;
  return 1;
}

/**
 * @brief Verifies the existence of an array in the SDDS dataset based on specified criteria.
 *
 * This function searches for an array within the SDDS dataset that matches the given criteria defined by the `mode`. It returns the index of the first matching array or `-1` if no match is found.
 *
 * The function supports the following modes:
 * - **FIND_SPECIFIED_TYPE**:
 *   - **Parameters**: `int32_t type`, `char *name`
 *   - **Description**: Finds the first array with the specified type.
 *
 * - **FIND_ANY_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first array with any type.
 *
 * - **FIND_NUMERIC_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first array with a numeric type.
 *
 * - **FIND_FLOATING_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first array with a floating type.
 *
 * - **FIND_INTEGER_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first array with an integer type.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset to be searched.
 *
 * @param[in] mode
 *     Specifies the mode for matching arrays. Valid modes are:
 *     - `FIND_SPECIFIED_TYPE`
 *     - `FIND_ANY_TYPE`
 *     - `FIND_NUMERIC_TYPE`
 *     - `FIND_FLOATING_TYPE`
 *     - `FIND_INTEGER_TYPE`
 *
 * @param[in] ...
 *     Variable arguments depending on `mode`:
 *     - **FIND_SPECIFIED_TYPE**: `int32_t type`, followed by `char *name`
 *     - **Other Modes**: `char *name`
 *
 * @return
 *     - Returns the index (`int32_t`) of the first matched array.
 *     - Returns `-1` if no matching array is found.
 *
 * @note
 *     - The caller must ensure that the variable arguments match the expected parameters for the specified `mode`.
 *
 * @warning
 *     - Passing incorrect types or mismatched arguments may lead to undefined behavior.
 *
 * @sa SDDS_GetArrayIndex, SDDS_CheckArray, SDDS_MatchArrays
 */
int32_t SDDS_VerifyArrayExists(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  int32_t index, type, thisType;
  va_list argptr;
  char *name;

  va_start(argptr, mode);
  type = 0;

  if (mode == FIND_SPECIFIED_TYPE)
    type = va_arg(argptr, int32_t);
  name = va_arg(argptr, char *);
  if ((index = SDDS_GetArrayIndex(SDDS_dataset, name)) >= 0) {
    thisType = SDDS_GetArrayType(SDDS_dataset, index);
    if (mode == FIND_ANY_TYPE || (mode == FIND_SPECIFIED_TYPE && thisType == type) || (mode == FIND_NUMERIC_TYPE && SDDS_NUMERIC_TYPE(thisType)) || (mode == FIND_FLOATING_TYPE && SDDS_FLOATING_TYPE(thisType)) || (mode == FIND_INTEGER_TYPE && SDDS_INTEGER_TYPE(thisType))) {
      va_end(argptr);
      return (index);
    }
  }
  va_end(argptr);
  return (-1);
}

/**
 * @brief Verifies the existence of a column in the SDDS dataset based on specified criteria.
 *
 * This function searches for a column within the SDDS dataset that matches the given criteria defined by the `mode`. It returns the index of the first matching column or `-1` if no match is found.
 *
 * The function supports the following modes:
 * - **FIND_SPECIFIED_TYPE**:
 *   - **Parameters**: `int32_t type`, `char *name`
 *   - **Description**: Finds the first column with the specified type.
 *
 * - **FIND_ANY_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first column with any type.
 *
 * - **FIND_NUMERIC_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first column with a numeric type.
 *
 * - **FIND_FLOATING_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first column with a floating type.
 *
 * - **FIND_INTEGER_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first column with an integer type.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset to be searched.
 *
 * @param[in] mode
 *     Specifies the mode for matching columns. Valid modes are:
 *     - `FIND_SPECIFIED_TYPE`
 *     - `FIND_ANY_TYPE`
 *     - `FIND_NUMERIC_TYPE`
 *     - `FIND_FLOATING_TYPE`
 *     - `FIND_INTEGER_TYPE`
 *
 * @param[in] ...
 *     Variable arguments depending on `mode`:
 *     - **FIND_SPECIFIED_TYPE**: `int32_t type`, followed by `char *name`
 *     - **Other Modes**: `char *name`
 *
 * @return
 *     - Returns the index (`int32_t`) of the first matched column.
 *     - Returns `-1` if no matching column is found.
 *
 * @note
 *     - The caller must ensure that the variable arguments match the expected parameters for the specified `mode`.
 *
 * @warning
 *     - Passing incorrect types or mismatched arguments may lead to undefined behavior.
 *
 * @sa SDDS_GetColumnIndex, SDDS_CheckColumn, SDDS_MatchColumns
 */
int32_t SDDS_VerifyColumnExists(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  int32_t index;
  int32_t type, thisType;
  va_list argptr;
  char *name;

  va_start(argptr, mode);
  type = 0;

  if (mode == FIND_SPECIFIED_TYPE)
    type = va_arg(argptr, int32_t);
  name = va_arg(argptr, char *);
  if ((index = SDDS_GetColumnIndex(SDDS_dataset, name)) >= 0) {
    thisType = SDDS_GetColumnType(SDDS_dataset, index);
    if (mode == FIND_ANY_TYPE || (mode == FIND_SPECIFIED_TYPE && thisType == type) || (mode == FIND_NUMERIC_TYPE && SDDS_NUMERIC_TYPE(thisType)) || (mode == FIND_FLOATING_TYPE && SDDS_FLOATING_TYPE(thisType)) || (mode == FIND_INTEGER_TYPE && SDDS_INTEGER_TYPE(thisType))) {
      va_end(argptr);
      return (index);
    }
  }
  va_end(argptr);
  return (-1);
}

/**
 * @brief Verifies the existence of a parameter in the SDDS dataset based on specified criteria.
 *
 * This function searches for a parameter within the SDDS dataset that matches the given criteria defined by the `mode`. It returns the index of the first matching parameter or `-1` if no match is found.
 *
 * The function supports the following modes:
 * - **FIND_SPECIFIED_TYPE**:
 *   - **Parameters**: `int32_t type`, `char *name`
 *   - **Description**: Finds the first parameter with the specified type.
 *
 * - **FIND_ANY_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first parameter with any type.
 *
 * - **FIND_NUMERIC_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first parameter with a numeric type.
 *
 * - **FIND_FLOATING_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first parameter with a floating type.
 *
 * - **FIND_INTEGER_TYPE**:
 *   - **Parameters**: `char *name`
 *   - **Description**: Finds the first parameter with an integer type.
 *
 * @param[in] SDDS_dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset to be searched.
 *
 * @param[in] mode
 *     Specifies the mode for matching parameters. Valid modes are:
 *     - `FIND_SPECIFIED_TYPE`
 *     - `FIND_ANY_TYPE`
 *     - `FIND_NUMERIC_TYPE`
 *     - `FIND_FLOATING_TYPE`
 *     - `FIND_INTEGER_TYPE`
 *
 * @param[in] ...
 *     Variable arguments depending on `mode`:
 *     - **FIND_SPECIFIED_TYPE**: `int32_t type`, followed by `char *name`
 *     - **Other Modes**: `char *name`
 *
 * @return
 *     - Returns the index (`int32_t`) of the first matched parameter.
 *     - Returns `-1` if no matching parameter is found.
 *
 * @note
 *     - The caller must ensure that the variable arguments match the expected parameters for the specified `mode`.
 *
 * @warning
 *     - Passing incorrect types or mismatched arguments may lead to undefined behavior.
 *
 * @sa SDDS_GetParameterIndex, SDDS_CheckParameter, SDDS_MatchParameters
 */
int32_t SDDS_VerifyParameterExists(SDDS_DATASET *SDDS_dataset, int32_t mode, ...) {
  int32_t index, type, thisType;
  va_list argptr;
  char *name;

  va_start(argptr, mode);
  type = 0;

  if (mode == FIND_SPECIFIED_TYPE)
    type = va_arg(argptr, int32_t);
  name = va_arg(argptr, char *);
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, name)) >= 0) {
    thisType = SDDS_GetParameterType(SDDS_dataset, index);
    if (mode == FIND_ANY_TYPE || (mode == FIND_SPECIFIED_TYPE && thisType == type) || (mode == FIND_NUMERIC_TYPE && SDDS_NUMERIC_TYPE(thisType)) || (mode == FIND_FLOATING_TYPE && SDDS_FLOATING_TYPE(thisType)) || (mode == FIND_INTEGER_TYPE && SDDS_INTEGER_TYPE(thisType))) {
      va_end(argptr);
      return (index);
    }
  }
  va_end(argptr);
  return (-1);
}

/**
 * @brief Retrieves an array of matching SDDS entity names based on specified criteria.
 *
 * This function processes a list of SDDS entity names (columns, parameters, or arrays) and selects those that match the provided criteria. It supports wildcard matching and exact matching based on the presence of wildcards in the names.
 *
 * @param[in] dataset
 *     Pointer to the `SDDS_DATASET` structure representing the dataset to be searched.
 *
 * @param[in] matchName
 *     Array of strings containing the names or patterns to match against the dataset's entities.
 *
 * @param[in] matches
 *     The number of names/patterns provided in `matchName`.
 *
 * @param[out] names
 *     Pointer to an `int32_t` that will be set to the number of matched names.
 *
 * @param[in] type
 *     Specifies the type of SDDS entity to match. Valid values are:
 *     - `SDDS_MATCH_COLUMN`
 *     - `SDDS_MATCH_PARAMETER`
 *     - `SDDS_MATCH_ARRAY`
 *
 * @return
 *     - Returns an array of strings (`char **`) containing the names of the matched SDDS entities.
 *     - If no matches are found, returns `NULL`.
 *
 * @note
 *     - The caller is responsible for freeing the memory allocated for the returned array and the individual strings within it.
 *     - The function uses `wild_match` for pattern matching when wildcards are present in the `matchName` entries.
 *
 * @warning
 *     - Ensure that the `type` parameter is correctly specified to match the intended SDDS entity class.
 *     - Passing an invalid `type` value will cause the function to terminate the program with an error message.
 *
 * @sa SDDS_MatchColumns, SDDS_MatchParameters, SDDS_MatchArrays, SDDS_Realloc, SDDS_CopyString
 */
char **getMatchingSDDSNames(SDDS_DATASET *dataset, char **matchName, int32_t matches, int32_t *names, short type) {
  char **name, **selectedName, *ptr = NULL;
  int32_t names0 = 0, selected = 0, i, j;
  int32_t names32 = 0;

  name = selectedName = NULL;
  switch (type) {
  case SDDS_MATCH_COLUMN:
    if (!(name = SDDS_GetColumnNames(dataset, &names0)))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    break;
  case SDDS_MATCH_PARAMETER:
    if (!(name = SDDS_GetParameterNames(dataset, &names32)))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    names0 = names32;
    break;
  case SDDS_MATCH_ARRAY:
    if (!(name = SDDS_GetArrayNames(dataset, &names32)))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    names0 = names32;
    break;
  default:
    SDDS_Bomb("Invalid match type provided.");
    break;
  }
  for (i = 0; i < matches; i++) {
    if (has_wildcards(matchName[i])) {
      ptr = expand_ranges(matchName[i]);
      for (j = 0; j < names0; j++) {
        if (wild_match(name[j], ptr)) {
          selectedName = SDDS_Realloc(selectedName, sizeof(*selectedName) * (selected + 1));
          SDDS_CopyString(&selectedName[selected], name[j]);
          selected++;
        }
      }
      free(ptr);
    } else {
      if (match_string(matchName[i], name, names0, EXACT_MATCH) < 0) {
        fprintf(stderr, "%s not found in input file.\n", matchName[i]);
        exit(1);
      } else {
        selectedName = SDDS_Realloc(selectedName, sizeof(*selectedName) * (selected + 1));
        SDDS_CopyString(&selectedName[selected], matchName[i]);
        selected++;
      }
    }
  }
  SDDS_FreeStringArray(name, names0);
  free(name);
  *names = selected;
  return selectedName;
}

/**
 * @brief Creates an empty SDDS dataset.
 *
 * This function allocates and initializes an empty `SDDS_DATASET` structure. The returned dataset can then be configured and populated with columns, parameters, and arrays as needed.
 *
 * @return
 *     - Pointer to the newly created `SDDS_DATASET` structure.
 *     - `NULL` if memory allocation fails.
 *
 * @note
 *     - The caller is responsible for initializing the dataset's layout and other necessary fields before use.
 *     - Ensure that the returned dataset is properly freed using appropriate memory deallocation functions to prevent memory leaks.
 *
 * @warning
 *     - Failing to initialize the dataset after creation may lead to undefined behavior when performing operations on it.
 *     - Always check if the returned pointer is not `NULL` before using it.
 *
 * @sa SDDS_FreeDataset, SDDS_InitLayout
 */
SDDS_DATASET *SDDS_CreateEmptyDataset(void) {
  SDDS_DATASET *dataset;
  dataset = malloc(sizeof(SDDS_DATASET));
  return dataset;
}
