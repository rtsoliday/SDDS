/**
 * @file SDDS_ascii.c
 * @brief SDDS ascii data input and output routines
 *
 * This file contains the implementation of the SDDS ascii data input and output routines.
 * It provides functions for writing void pointer data to an ASCII file stream.
 * The functions support different data types such as short, unsigned short, long, unsigned long,
 * long long, unsigned long long, float, double, long double, string, and character.
 *
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
#include <ctype.h>

#undef DEBUG

#define COMMENT_POSITION 40

#if SDDS_VERSION != 5
#  error "SDDS_VERSION does not match the version number of this file"
#endif

/* Define a dynamically expanded text buffer for use by ReadAsciiParameters,
   ReadAsciiArrays, and ReadAsciiPage.  Since these routines never call each
   other, it is okay that they share this buffer.
*/
#define INITIAL_BIG_BUFFER_SIZE SDDS_MAXLINE

/**
 * @brief Writes a typed value to an ASCII file stream.
 *
 * This function writes a value of a specified SDDS data type to an ASCII file stream.
 * The data is provided as a void pointer, and the function handles various data types
 * by casting the pointer appropriately based on the `type` parameter.
 * For string data, special characters are escaped according to SDDS conventions.
 *
 * @param data Pointer to the data to be written. Should be castable to the type specified by `type`.
 * @param index Array index of the data to be printed; use 0 if not an array.
 * @param type The SDDS data type of the data variable. Possible values include SDDS_SHORT, SDDS_LONG, SDDS_FLOAT, etc.
 * @param format Optional printf format string to use; pass NULL to use the default format for the data type.
 * @param fp The FILE pointer to the ASCII file stream where the data will be written.
 *
 * @return Returns 1 on success; 0 on error (e.g., if data or fp is NULL, or an unknown data type is specified).
 */
int32_t SDDS_WriteTypedValue(void *data, int64_t index, int32_t type, char *format, FILE *fp) {
  char c, *s;
  short hasWhitespace;

  if (!data) {
    SDDS_SetError("Unable to write value--data pointer is NULL (SDDS_WriteTypedValue)");
    return (0);
  }
  if (!fp) {
    SDDS_SetError("Unable to print value--file pointer is NULL (SDDS_WriteTypedValue)");
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
    fprintf(fp, format ? format : "%22.15e", *((double *)data + index));
    break;
  case SDDS_LONGDOUBLE:
    if (LDBL_DIG == 18) {
      fprintf(fp, format ? format : "%22.18Le", *((long double *)data + index));
    } else {
      fprintf(fp, format ? format : "%22.15Le", *((long double *)data + index));
    }
    break;
  case SDDS_STRING:
    /* ignores format string */
    s = *((char **)data + index);
    hasWhitespace = 0;
    if (SDDS_HasWhitespace(s) || SDDS_StringIsBlank(s)) {
      fputc('"', fp);
      hasWhitespace = 1;
    }
    while (s && *s) {
      c = *s++;
      if (c == '!')
        fputs("\\!", fp);
      else if (c == '\\')
        fputs("\\\\", fp);
      else if (c == '"')
        fputs("\\\"", fp);
      else if (c == ' ')
        fputc(' ', fp); /* don't escape plain spaces */
      else if (isspace(c) || !isprint(c))
        fprintf(fp, "\\%03o", c);
      else
        fputc(c, fp);
    }
    if (hasWhitespace)
      fputc('"', fp);
    break;
  case SDDS_CHARACTER:
    /* ignores format string */
    c = *((char *)data + index);
    if (c == '!')
      fputs("\\!", fp);
    else if (c == '\\')
      fputs("\\\\", fp);
    else if (c == '"')
      fputs("\\\"", fp);
    else if (!c || isspace(c) || !isprint(c))
      fprintf(fp, "\\%03o", c);
    else
      fputc(c, fp);
    break;
  default:
    SDDS_SetError("Unable to write value--unknown data type (SDDS_WriteTypedValue)");
    return (0);
  }
  return (1);
}

/**
 * @brief Writes a typed value to an LZMA compressed ASCII file stream.
 *
 * This function writes a single value from the provided data pointer to the given LZMA compressed file stream.
 * The value is formatted as an ASCII string according to the specified data type and optional format string.
 * Supports various data types including integers, floating-point numbers, strings, and characters.
 *
 * @param data Pointer to the data to be written. The data should be of the type specified by the @p type parameter.
 *             For array data types, @p data should point to the base of the array.
 * @param index Zero-based index of the element to write if @p data is an array; use 0 if @p data is a single value.
 * @param type The SDDS data type of the value to be written. Determines how the data is interpreted and formatted.
 *             Valid types include:
 *             - SDDS_SHORT
 *             - SDDS_USHORT
 *             - SDDS_LONG
 *             - SDDS_ULONG
 *             - SDDS_LONG64
 *             - SDDS_ULONG64
 *             - SDDS_FLOAT
 *             - SDDS_DOUBLE
 *             - SDDS_LONGDOUBLE
 *             - SDDS_STRING
 *             - SDDS_CHARACTER
 * @param format Optional printf-style format string to specify the output format. If NULL, a default format is used
 *               based on the data type.
 * @param lzmafp Pointer to the LZMA file stream where the data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via SDDS_SetError().
 *
 * @note This function handles special character escaping for strings and characters to ensure correct parsing.
 *       For string and character types, special characters like '!', '\\', and '"' are escaped appropriately.
 */
int32_t SDDS_LZMAWriteTypedValue(void *data, int64_t index, int32_t type, char *format, struct lzmafile *lzmafp) {
  char c, *s;
  short hasWhitespace;

  if (!data) {
    SDDS_SetError("Unable to write value--data pointer is NULL (SDDS_LZMAWriteTypedValue)");
    return (0);
  }
  if (!lzmafp) {
    SDDS_SetError("Unable to print value--file pointer is NULL (SDDS_LZMAWriteTypedValue)");
    return (0);
  }
  switch (type) {
  case SDDS_SHORT:
    lzma_printf(lzmafp, format ? format : "%hd", *((short *)data + index));
    break;
  case SDDS_USHORT:
    lzma_printf(lzmafp, format ? format : "%hu", *((unsigned short *)data + index));
    break;
  case SDDS_LONG:
    lzma_printf(lzmafp, format ? format : "%" PRId32, *((int32_t *)data + index));
    break;
  case SDDS_ULONG:
    lzma_printf(lzmafp, format ? format : "%" PRIu32, *((uint32_t *)data + index));
    break;
  case SDDS_LONG64:
    lzma_printf(lzmafp, format ? format : "%" PRId64, *((int64_t *)data + index));
    break;
  case SDDS_ULONG64:
    lzma_printf(lzmafp, format ? format : "%" PRIu64, *((uint64_t *)data + index));
    break;
  case SDDS_FLOAT:
    lzma_printf(lzmafp, format ? format : "%15.8e", *((float *)data + index));
    break;
  case SDDS_DOUBLE:
    lzma_printf(lzmafp, format ? format : "%22.15e", *((double *)data + index));
    break;
  case SDDS_LONGDOUBLE:
    if (LDBL_DIG == 18) {
      lzma_printf(lzmafp, format ? format : "%22.18Le", *((long double *)data + index));
    } else {
      lzma_printf(lzmafp, format ? format : "%22.15Le", *((long double *)data + index));
    }
    break;
  case SDDS_STRING:
    /* ignores format string */
    s = *((char **)data + index);
    hasWhitespace = 0;
    if (SDDS_HasWhitespace(s) || SDDS_StringIsBlank(s)) {
      lzma_putc('"', lzmafp);
      hasWhitespace = 1;
    }
    while (s && *s) {
      c = *s++;
      if (c == '!')
        lzma_puts("\\!", lzmafp);
      else if (c == '\\')
        lzma_puts("\\\\", lzmafp);
      else if (c == '"')
        lzma_puts("\\\"", lzmafp);
      else if (c == ' ')
        lzma_putc(' ', lzmafp); /* don't escape plain spaces */
      else if (isspace(c) || !isprint(c))
        lzma_printf(lzmafp, "\\%03o", c);
      else
        lzma_putc(c, lzmafp);
    }
    if (hasWhitespace)
      lzma_putc('"', lzmafp);
    break;
  case SDDS_CHARACTER:
    /* ignores format string */
    c = *((char *)data + index);
    if (c == '!')
      lzma_puts("\\!", lzmafp);
    else if (c == '\\')
      lzma_puts("\\\\", lzmafp);
    else if (c == '"')
      lzma_puts("\\\"", lzmafp);
    else if (!c || isspace(c) || !isprint(c))
      lzma_printf(lzmafp, "\\%03o", c);
    else
      lzma_putc(c, lzmafp);
    break;
  default:
    SDDS_SetError("Unable to write value--unknown data type (SDDS_LZMAWriteTypedValue)");
    return (0);
  }
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes a typed value to a GZIP compressed ASCII file stream.
 *
 * This function writes a single value from the provided data pointer to the given GZIP compressed file stream.
 * The value is formatted as an ASCII string according to the specified data type and optional format string.
 * Supports various data types including integers, floating-point numbers, strings, and characters.
 *
 * @param data Pointer to the data to be written. The data should be of the type specified by the @p type parameter.
 *             For array data types, @p data should point to the base of the array.
 * @param index Zero-based index of the element to write if @p data is an array; use 0 if @p data is a single value.
 * @param type The SDDS data type of the value to be written. Determines how the data is interpreted and formatted.
 *             Valid types include:
 *             - SDDS_SHORT
 *             - SDDS_USHORT
 *             - SDDS_LONG
 *             - SDDS_ULONG
 *             - SDDS_LONG64
 *             - SDDS_ULONG64
 *             - SDDS_FLOAT
 *             - SDDS_DOUBLE
 *             - SDDS_LONGDOUBLE
 *             - SDDS_STRING
 *             - SDDS_CHARACTER
 * @param format Optional printf-style format string to specify the output format. If NULL, a default format is used
 *               based on the data type.
 * @param gzfp Pointer to the GZIP file stream where the data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via SDDS_SetError().
 *
 * @note This function handles special character escaping for strings and characters to ensure correct parsing.
 *       For string and character types, special characters like '!', '\\', and '"' are escaped appropriately.
 */
int32_t SDDS_GZipWriteTypedValue(void *data, int64_t index, int32_t type, char *format, gzFile gzfp) {
  char c, *s;
  short hasWhitespace;

  if (!data) {
    SDDS_SetError("Unable to write value--data pointer is NULL (SDDS_GZipWriteTypedValue)");
    return (0);
  }
  if (!gzfp) {
    SDDS_SetError("Unable to print value--file pointer is NULL (SDDS_GZipWriteTypedValue)");
    return (0);
  }
  switch (type) {
  case SDDS_SHORT:
    gzprintf(gzfp, format ? format : "%hd", *((short *)data + index));
    break;
  case SDDS_USHORT:
    gzprintf(gzfp, format ? format : "%hu", *((unsigned short *)data + index));
    break;
  case SDDS_LONG:
    gzprintf(gzfp, format ? format : "%" PRId32, *((int32_t *)data + index));
    break;
  case SDDS_ULONG:
    gzprintf(gzfp, format ? format : "%" PRIu32, *((uint32_t *)data + index));
    break;
  case SDDS_LONG64:
    gzprintf(gzfp, format ? format : "%" PRId64, *((int64_t *)data + index));
    break;
  case SDDS_ULONG64:
    gzprintf(gzfp, format ? format : "%" PRIu64, *((uint64_t *)data + index));
    break;
  case SDDS_FLOAT:
    gzprintf(gzfp, format ? format : "%15.8e", *((float *)data + index));
    break;
  case SDDS_DOUBLE:
    gzprintf(gzfp, format ? format : "%22.15e", *((double *)data + index));
    break;
  case SDDS_LONGDOUBLE:
    if (LDBL_DIG == 18) {
      gzprintf(gzfp, format ? format : "%22.18Le", *((long double *)data + index));
    } else {
      gzprintf(gzfp, format ? format : "%22.15Le", *((long double *)data + index));
    }
    break;
  case SDDS_STRING:
    /* ignores format string */
    s = *((char **)data + index);
    hasWhitespace = 0;
    if (SDDS_HasWhitespace(s) || SDDS_StringIsBlank(s)) {
      gzputc(gzfp, '"');
      hasWhitespace = 1;
    }
    while (s && *s) {
      c = *s++;
      if (c == '!')
        gzputs(gzfp, "\\!");
      else if (c == '\\')
        gzputs(gzfp, "\\\\");
      else if (c == '"')
        gzputs(gzfp, "\\\"");
      else if (c == ' ')
        gzputc(gzfp, ' '); /* don't escape plain spaces */
      else if (isspace(c) || !isprint(c))
        gzprintf(gzfp, "\\%03o", c);
      else
        gzputc(gzfp, c);
    }
    if (hasWhitespace)
      gzputc(gzfp, '"');
    break;
  case SDDS_CHARACTER:
    /* ignores format string */
    c = *((char *)data + index);
    if (c == '!')
      gzputs(gzfp, "\\!");
    else if (c == '\\')
      gzputs(gzfp, "\\\\");
    else if (c == '"')
      gzputs(gzfp, "\\\"");
    else if (!c || isspace(c) || !isprint(c))
      gzprintf(gzfp, "\\%03o", c);
    else
      gzputc(gzfp, c);
    break;
  default:
    SDDS_SetError("Unable to write value--unknown data type (SDDS_GZipWriteTypedValue)");
    return (0);
  }
  return (1);
}
#endif

/**
 * @brief Writes a page of data in ASCII format to the SDDS dataset.
 *
 * This function writes the current page of data from the SDDS dataset to an ASCII file.
 * It handles writing to regular files, as well as LZMA and GZIP compressed files if enabled.
 * The function writes the page number, parameters, arrays, and rows of data according to the SDDS format.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the data to be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via SDDS_SetError().
 *
 * @note This function checks the dataset for consistency before writing. It also updates internal state variables
 *       such as the last row written and the number of rows written.
 */
int32_t SDDS_WriteAsciiPage(SDDS_DATASET *SDDS_dataset) {
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  int64_t i, rows;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteAsciiPage"))
    return (0);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!(gzfp = SDDS_dataset->layout.gzfp)) {
      SDDS_SetError("Unable to write page--file pointer is NULL (SDDS_WriteAsciiPage)");
      return (0);
    }
    if (SDDS_dataset->layout.data_mode.no_row_counts && (SDDS_dataset->page_number > 1 || SDDS_dataset->file_had_data))
      gzputc(gzfp, '\n');
    gzprintf(gzfp, "! page number %" PRId32 "\n", SDDS_dataset->page_number);

    if (!SDDS_GZipWriteAsciiParameters(SDDS_dataset, gzfp) || !SDDS_GZipWriteAsciiArrays(SDDS_dataset, gzfp))
      return 0;
    rows = 0;
    if (SDDS_dataset->layout.n_columns) {
      rows = SDDS_CountRowsOfInterest(SDDS_dataset);
      if (!SDDS_dataset->layout.data_mode.no_row_counts) {
        SDDS_dataset->rowcount_offset = gztell(gzfp);
        if (SDDS_dataset->layout.data_mode.fixed_row_count) {
          gzprintf(gzfp, "%20" PRId64 "\n", ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment);
        } else
          gzprintf(gzfp, "%20" PRId64 "\n", rows);
      }
      for (i = 0; i < SDDS_dataset->n_rows; i++)
        if (SDDS_dataset->row_flag[i] && !SDDS_GZipWriteAsciiRow(SDDS_dataset, i, gzfp))
          return 0;
    }
    SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
    SDDS_dataset->n_rows_written = rows;
    SDDS_dataset->writing_page = 1;
    /*gzflush(gzfp, Z_FULL_FLUSH); */
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!(lzmafp = SDDS_dataset->layout.lzmafp)) {
        SDDS_SetError("Unable to write page--file pointer is NULL (SDDS_WriteAsciiPage)");
        return (0);
      }
      if (SDDS_dataset->layout.data_mode.no_row_counts && (SDDS_dataset->page_number > 1 || SDDS_dataset->file_had_data))
        lzma_putc('\n', lzmafp);
      lzma_printf(lzmafp, "! page number %" PRId32 "\n", SDDS_dataset->page_number);

      if (!SDDS_LZMAWriteAsciiParameters(SDDS_dataset, lzmafp) || !SDDS_LZMAWriteAsciiArrays(SDDS_dataset, lzmafp))
        return 0;
      rows = 0;
      if (SDDS_dataset->layout.n_columns) {
        rows = SDDS_CountRowsOfInterest(SDDS_dataset);
        if (!SDDS_dataset->layout.data_mode.no_row_counts) {
          SDDS_dataset->rowcount_offset = lzma_tell(lzmafp);
          if (SDDS_dataset->layout.data_mode.fixed_row_count)
            lzma_printf(lzmafp, "%20" PRId64 "\n", ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment);
          else
            lzma_printf(lzmafp, "%20" PRId64 "\n", rows);
        }
        for (i = 0; i < SDDS_dataset->n_rows; i++)
          if (SDDS_dataset->row_flag[i] && !SDDS_LZMAWriteAsciiRow(SDDS_dataset, i, lzmafp))
            return 0;
      }
      SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
      SDDS_dataset->n_rows_written = rows;
      SDDS_dataset->writing_page = 1;
    } else {
      if (!(fp = SDDS_dataset->layout.fp)) {
        SDDS_SetError("Unable to write page--file pointer is NULL (SDDS_WriteAsciiPage)");
        return (0);
      }
      if (SDDS_dataset->layout.data_mode.no_row_counts && (SDDS_dataset->page_number > 1 || SDDS_dataset->file_had_data))
        fputc('\n', fp);
      fprintf(fp, "! page number %" PRId32 "\n", SDDS_dataset->page_number);

      if (!SDDS_WriteAsciiParameters(SDDS_dataset, fp) || !SDDS_WriteAsciiArrays(SDDS_dataset, fp))
        return 0;
      rows = 0;
      if (SDDS_dataset->layout.n_columns) {
        rows = SDDS_CountRowsOfInterest(SDDS_dataset);
        if (!SDDS_dataset->layout.data_mode.no_row_counts) {
          SDDS_dataset->rowcount_offset = ftell(fp);
          if (SDDS_dataset->layout.data_mode.fixed_row_count)
            fprintf(fp, "%20" PRId64 "\n", ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment);
          else
            fprintf(fp, "%20" PRId64 "\n", rows);
        }
        for (i = 0; i < SDDS_dataset->n_rows; i++)
          if (SDDS_dataset->row_flag[i] && !SDDS_WriteAsciiRow(SDDS_dataset, i, fp))
            return 0;
      }
      SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
      SDDS_dataset->n_rows_written = rows;
      SDDS_dataset->writing_page = 1;
      fflush(fp);
    }
#if defined(zLib)
  }
#endif

  return (1);
}

/**
 * @brief Writes the parameter data of an SDDS dataset in ASCII format to a file.
 *
 * This function writes all parameters of the SDDS dataset to the provided file pointer in ASCII format.
 * Only parameters that do not have fixed values are written. Each parameter value is written on a new line.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the parameter data.
 * @param fp File pointer to the ASCII file where the parameter data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via SDDS_SetError().
 *
 * @note The function checks the dataset for consistency before writing. It uses SDDS_WriteTypedValue() to write
 *       each parameter value according to its data type.
 */
int32_t SDDS_WriteAsciiParameters(SDDS_DATASET *SDDS_dataset, FILE *fp) {
  int32_t i;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteAsciiParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].fixed_value)
      continue;
    if (!SDDS_WriteTypedValue(SDDS_dataset->parameter[i], 0, layout->parameter_definition[i].type, NULL, fp)) {
      SDDS_SetError("Unable to write ascii parameters (SDDS_WriteAsciiParameters)");
      return 0;
    }
    fputc('\n', fp);
  }
  return (1);
}

/**
 * @brief Writes the parameter data of an SDDS dataset in ASCII format to an LZMA compressed file.
 *
 * This function writes all parameters of the provided SDDS dataset to the specified LZMA compressed file in ASCII format.
 * Only parameters that do not have fixed values are written. Each parameter value is written on a new line.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the parameter data.
 * @param lzmafp Pointer to the LZMA file stream where the ASCII data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note The function checks the dataset for consistency before writing. It uses `SDDS_LZMAWriteTypedValue()` to write
 *       each parameter value according to its data type.
 */
int32_t SDDS_LZMAWriteAsciiParameters(SDDS_DATASET *SDDS_dataset, struct lzmafile *lzmafp) {
  int32_t i;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_LZMAWriteAsciiParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].fixed_value)
      continue;
    if (!SDDS_LZMAWriteTypedValue(SDDS_dataset->parameter[i], 0, layout->parameter_definition[i].type, NULL, lzmafp)) {
      SDDS_SetError("Unable to write ascii parameters (SDDS_LZMAWriteAsciiParameters)");
      return 0;
    }
    lzma_putc('\n', lzmafp);
  }
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes the parameter data of an SDDS dataset in ASCII format to a GZIP compressed file.
 *
 * This function writes all parameters of the provided SDDS dataset to the specified GZIP compressed file in ASCII format.
 * Only parameters that do not have fixed values are written. Each parameter value is written on a new line.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the parameter data.
 * @param gzfp Pointer to the GZIP file stream where the ASCII data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note The function checks the dataset for consistency before writing. It uses `SDDS_GZipWriteTypedValue()` to write
 *       each parameter value according to its data type.
 */
int32_t SDDS_GZipWriteAsciiParameters(SDDS_DATASET *SDDS_dataset, gzFile gzfp) {
  int32_t i;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GZipWriteAsciiParameters"))
    return (0);
  layout = &SDDS_dataset->layout;
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].fixed_value)
      continue;
    if (!SDDS_GZipWriteTypedValue(SDDS_dataset->parameter[i], 0, layout->parameter_definition[i].type, NULL, gzfp)) {
      SDDS_SetError("Unable to write ascii parameters (SDDS_GZipWriteAsciiParameters)");
      return 0;
    }
    gzputc(gzfp, '\n');
  }
  return (1);
}
#endif

/**
 * @brief Writes the arrays of an SDDS dataset in ASCII format to a file.
 *
 * This function writes all arrays contained in the provided SDDS dataset to the specified file pointer in ASCII format.
 * For each array, it writes the dimensions, a description line, and the array elements formatted according to their data type.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the arrays to be written.
 * @param fp File pointer to the ASCII file where the array data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note The function checks the dataset for consistency before writing. It uses `SDDS_WriteTypedValue()` to write
 *       each array element according to its data type. Each array element is written, with up to 6 elements per line.
 */
int32_t SDDS_WriteAsciiArrays(SDDS_DATASET *SDDS_dataset, FILE *fp) {
  int32_t i, j;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  ARRAY_DEFINITION *array_definition;
  SDDS_ARRAY *array;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteAsciiArrays"))
    return (0);
  layout = &SDDS_dataset->layout;
  for (j = 0; j < layout->n_arrays; j++) {
    array_definition = layout->array_definition + j;
    array = &SDDS_dataset->array[j];
    for (i = 0; i < array_definition->dimensions; i++)
      fprintf(fp, "%" PRId32 " ", array->dimension[i]);
    fprintf(fp, "          ! %" PRId32 "-dimensional array %s:\n", array_definition->dimensions, array_definition->name);
    for (i = 0; i < array->elements;) {
      if (!SDDS_WriteTypedValue(array->data, i, array_definition->type, NULL, fp)) {
        SDDS_SetError("Unable to write array--couldn't write ASCII data (SDDS_WriteAsciiArrays)");
        return (0);
      }
      i++;
      if (i % 6 == 0 || i == array->elements)
        fputc('\n', fp);
      else
        fputc(' ', fp);
    }
  }
  return (1);
}

/**
 * @brief Writes the arrays of an SDDS dataset in ASCII format to an LZMA compressed file.
 *
 * This function writes all arrays contained in the provided SDDS dataset to the specified LZMA compressed file in ASCII format.
 * For each array, it writes the dimensions, a description line, and the array elements formatted according to their data type.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the arrays to be written.
 * @param lzmafp Pointer to the LZMA file stream where the array data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note The function checks the dataset for consistency before writing. It uses `SDDS_LZMAWriteTypedValue()` to write
 *       each array element according to its data type. Each array element is written, with up to 6 elements per line.
 */
int32_t SDDS_LZMAWriteAsciiArrays(SDDS_DATASET *SDDS_dataset, struct lzmafile *lzmafp) {
  int32_t i, j;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  ARRAY_DEFINITION *array_definition;
  SDDS_ARRAY *array;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_LZMAWriteAsciiArrays"))
    return (0);
  layout = &SDDS_dataset->layout;
  for (j = 0; j < layout->n_arrays; j++) {
    array_definition = layout->array_definition + j;
    array = &SDDS_dataset->array[j];
    for (i = 0; i < array_definition->dimensions; i++)
      lzma_printf(lzmafp, "%" PRId32 " ", array->dimension[i]);
    lzma_printf(lzmafp, "          ! %" PRId32 "-dimensional array %s:\n", array_definition->dimensions, array_definition->name);
    for (i = 0; i < array->elements;) {
      if (!SDDS_LZMAWriteTypedValue(array->data, i, array_definition->type, NULL, lzmafp)) {
        SDDS_SetError("Unable to write array--couldn't write ASCII data (SDDS_LZMAWriteAsciiArrays)");
        return (0);
      }
      i++;
      if (i % 6 == 0 || i == array->elements)
        lzma_putc('\n', lzmafp);
      else
        lzma_putc(' ', lzmafp);
    }
  }
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes the arrays of an SDDS dataset in ASCII format to a GZIP compressed file.
 *
 * This function writes all arrays contained in the provided SDDS dataset to the specified GZIP compressed file in ASCII format.
 * For each array, it writes the dimensions, a description line, and the array elements formatted according to their data type.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the arrays to be written.
 * @param gzfp Pointer to the GZIP file stream where the array data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note The function checks the dataset for consistency before writing. It uses `SDDS_GZipWriteTypedValue()` to write
 *       each array element according to its data type. Each array element is written, with up to 6 elements per line.
 */
int32_t SDDS_GZipWriteAsciiArrays(SDDS_DATASET *SDDS_dataset, gzFile gzfp) {
  int32_t i, j;
  SDDS_LAYOUT *layout;
  /*  char *predefined_format; */
  ARRAY_DEFINITION *array_definition;
  SDDS_ARRAY *array;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GZipWriteAsciiArrays"))
    return (0);
  layout = &SDDS_dataset->layout;
  for (j = 0; j < layout->n_arrays; j++) {
    array_definition = layout->array_definition + j;
    array = &SDDS_dataset->array[j];
    for (i = 0; i < array_definition->dimensions; i++)
      gzprintf(gzfp, "%" PRId32 " ", array->dimension[i]);
    gzprintf(gzfp, "          ! %" PRId32 "-dimensional array %s:\n", array_definition->dimensions, array_definition->name);
    for (i = 0; i < array->elements;) {
      if (!SDDS_GZipWriteTypedValue(array->data, i, array_definition->type, NULL, gzfp)) {
        SDDS_SetError("Unable to write array--couldn't write ASCII data (SDDS_GZipWriteAsciiArrays)");
        return (0);
      }
      i++;
      if (i % 6 == 0 || i == array->elements)
        gzputc(gzfp, '\n');
      else
        gzputc(gzfp, ' ');
    }
  }
  return (1);
}
#endif

/**
 * @brief Writes a single row of data in ASCII format to a file.
 *
 * This function writes the specified row from the SDDS dataset to the provided file pointer in ASCII format.
 * The data is formatted according to the data types of the columns. Supports multi-line rows as specified by
 * the data mode settings in the dataset layout.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the data.
 * @param row Zero-based index of the row to write.
 * @param fp File pointer to the ASCII file where the row data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note The function checks the dataset for consistency before writing. It uses `SDDS_WriteTypedValue()` to write
 *       each column value according to its data type. The number of values per line is determined by the
 *       `lines_per_row` setting in the dataset layout.
 */
int32_t SDDS_WriteAsciiRow(SDDS_DATASET *SDDS_dataset, int64_t row, FILE *fp) {
  int32_t newline_needed;
  int64_t i, n_per_line, line;
  /*  int32_t  embedded_quotes_present; */
  SDDS_LAYOUT *layout;
  /*  char *predefined_format, *s; */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteAsciiRow"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (SDDS_dataset->layout.data_mode.lines_per_row <= 0)
    SDDS_dataset->layout.data_mode.lines_per_row = 1;
  n_per_line = SDDS_dataset->layout.n_columns / SDDS_dataset->layout.data_mode.lines_per_row;
  line = 1;
  newline_needed = 0;
  for (i = 0; i < layout->n_columns; i++) {
    if (!SDDS_WriteTypedValue(SDDS_dataset->data[i], row, layout->column_definition[i].type, NULL, fp)) {
      SDDS_SetError("Unable to write ascii row (SDDS_WriteAsciiRow)");
      return 0;
    }
    if ((i + 1) % n_per_line == 0 && line != SDDS_dataset->layout.data_mode.lines_per_row) {
      newline_needed = 0;
      fputc('\n', fp);
      line++;
    } else {
      fputc(' ', fp);
      newline_needed = 1;
    }
  }
  if (newline_needed)
    fputc('\n', fp);
  return (1);
}

/**
 * @brief Writes a single row of data in ASCII format to an LZMA compressed file.
 *
 * This function writes the specified row from the SDDS dataset to the provided LZMA compressed file in ASCII format.
 * The data is formatted according to the data types of the columns. Supports multi-line rows as specified by
 * the data mode settings in the dataset layout.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the data.
 * @param row Zero-based index of the row to write.
 * @param lzmafp Pointer to the LZMA file stream where the row data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note The function checks the dataset for consistency before writing. It uses `SDDS_LZMAWriteTypedValue()` to write
 *       each column value according to its data type. The number of values per line is determined by the
 *       `lines_per_row` setting in the dataset layout.
 */
int32_t SDDS_LZMAWriteAsciiRow(SDDS_DATASET *SDDS_dataset, int64_t row, struct lzmafile *lzmafp) {
  int32_t newline_needed;
  int64_t i, n_per_line, line;
  /*  int32_t  embedded_quotes_present; */
  SDDS_LAYOUT *layout;
  /*  char *predefined_format, *s; */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_LZMAWriteAsciiRow"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (SDDS_dataset->layout.data_mode.lines_per_row <= 0)
    SDDS_dataset->layout.data_mode.lines_per_row = 1;
  n_per_line = SDDS_dataset->layout.n_columns / SDDS_dataset->layout.data_mode.lines_per_row;
  line = 1;
  newline_needed = 0;
  for (i = 0; i < layout->n_columns; i++) {
    if (!SDDS_LZMAWriteTypedValue(SDDS_dataset->data[i], row, layout->column_definition[i].type, NULL, lzmafp)) {
      SDDS_SetError("Unable to write ascii row (SDDS_LZMAWriteAsciiRow)");
      return 0;
    }
    if ((i + 1) % n_per_line == 0 && line != SDDS_dataset->layout.data_mode.lines_per_row) {
      newline_needed = 0;
      lzma_putc('\n', lzmafp);
      line++;
    } else {
      lzma_putc(' ', lzmafp);
      newline_needed = 1;
    }
  }
  if (newline_needed)
    lzma_putc('\n', lzmafp);
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes a single row of data in ASCII format to a GZIP compressed file.
 *
 * This function writes the specified row from the SDDS dataset to the provided GZIP compressed file in ASCII format.
 * The data is formatted according to the data types of the columns. Supports multi-line rows as specified by
 * the data mode settings in the dataset layout.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset containing the data.
 * @param row Zero-based index of the row to write.
 * @param gzfp Pointer to the GZIP file stream where the row data will be written.
 *
 * @return Returns 1 on success, or 0 on error. If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note The function checks the dataset for consistency before writing. It uses `SDDS_GZipWriteTypedValue()` to write
 *       each column value according to its data type. The number of values per line is determined by the
 *       `lines_per_row` setting in the dataset layout.
 */
int32_t SDDS_GZipWriteAsciiRow(SDDS_DATASET *SDDS_dataset, int64_t row, gzFile gzfp) {
  int32_t newline_needed;
  int64_t i, n_per_line, line;
  /*  int32_t  embedded_quotes_present; */
  SDDS_LAYOUT *layout;
  /*  char *predefined_format, *s; */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GZipWriteAsciiRow"))
    return (0);
  layout = &SDDS_dataset->layout;
  if (SDDS_dataset->layout.data_mode.lines_per_row <= 0)
    SDDS_dataset->layout.data_mode.lines_per_row = 1;
  n_per_line = SDDS_dataset->layout.n_columns / SDDS_dataset->layout.data_mode.lines_per_row;
  line = 1;
  newline_needed = 0;
  for (i = 0; i < layout->n_columns; i++) {
    if (!SDDS_GZipWriteTypedValue(SDDS_dataset->data[i], row, layout->column_definition[i].type, NULL, gzfp)) {
      SDDS_SetError("Unable to write ascii row (SDDS_GZipWriteAsciiRow)");
      return 0;
    }
    if ((i + 1) % n_per_line == 0 && line != SDDS_dataset->layout.data_mode.lines_per_row) {
      newline_needed = 0;
      gzputc(gzfp, '\n');
      line++;
    } else {
      gzputc(gzfp, ' ');
      newline_needed = 1;
    }
  }
  if (newline_needed)
    gzputc(gzfp, '\n');
  return (1);
}
#endif

/**
 * @brief Reads the parameters from an ASCII file into the SDDS dataset.
 *
 * This function reads parameter data from an ASCII file and stores it in the provided SDDS dataset.
 * It supports reading from regular files, as well as GZIP and LZMA compressed files if enabled.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the parameters will be stored.
 *
 * @return Returns 1 on success, 0 on error, or -1 if end-of-file is reached before any parameters are read.
 *
 * @note The function checks the dataset for consistency before reading. It handles fixed-value parameters appropriately.
 *       The function reads each parameter line by line, skipping comments, and stores the values after scanning them
 *       according to their data type.
 */
int32_t SDDS_ReadAsciiParameters(SDDS_DATASET *SDDS_dataset) {
  int32_t i, first_read;
  SDDS_LAYOUT *layout;
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  char *bigBuffer = NULL;
  int32_t bigBufferSize = 0;

  layout = &SDDS_dataset->layout;
  first_read = 1;
  if (layout->n_parameters > 0) {
    if (!(bigBuffer = SDDS_Malloc(sizeof(*bigBuffer) * (bigBufferSize = INITIAL_BIG_BUFFER_SIZE)))) {
      SDDS_SetError("Unable to read parameters--buffer allocation failure (SDDS_ReadAsciiParameters)");
      return (0);
    }
  }
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
    for (i = 0; i < layout->n_parameters; i++) {
      if (layout->parameter_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
        continue;
      bigBuffer[0] = 0;
      if (!layout->parameter_definition[i].fixed_value) {
        if (!fgetsGZipSkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, gzfp, '!')) {
          if (first_read) {
            if (bigBuffer)
              free(bigBuffer);
            return (-1);
          }
          SDDS_SetError("Unable to read parameters--data ends prematurely (SDDS_ReadAsciiParameters)");
          return (0);
        }
        first_read = 0;
        bigBuffer[strlen(bigBuffer) - 1] = 0;
      } else
        strcpy(bigBuffer, layout->parameter_definition[i].fixed_value);
      if (!SDDS_ScanData(bigBuffer, layout->parameter_definition[i].type, 0, SDDS_dataset->parameter[i], 0, 1)) {
        SDDS_SetError("Unable to read page--parameter scanning error (SDDS_ReadAsciiParameters)");
        return (0);
      }
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
      for (i = 0; i < layout->n_parameters; i++) {
        if (layout->parameter_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
          continue;
        bigBuffer[0] = 0;
        if (!layout->parameter_definition[i].fixed_value) {
          if (!fgetsLZMASkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, lzmafp, '!')) {
            if (first_read) {
              if (bigBuffer)
                free(bigBuffer);
              return (-1);
            }
            SDDS_SetError("Unable to read parameters--data ends prematurely (SDDS_ReadAsciiParameters)");
            return (0);
          }
          first_read = 0;
          bigBuffer[strlen(bigBuffer) - 1] = 0;
        } else
          strcpy(bigBuffer, layout->parameter_definition[i].fixed_value);
        if (!SDDS_ScanData(bigBuffer, layout->parameter_definition[i].type, 0, SDDS_dataset->parameter[i], 0, 1)) {
          SDDS_SetError("Unable to read page--parameter scanning error (SDDS_ReadAsciiParameters)");
          return (0);
        }
      }
    } else {
      fp = layout->fp;
      for (i = 0; i < layout->n_parameters; i++) {
        if (layout->parameter_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
          continue;
        bigBuffer[0] = 0;
        if (!layout->parameter_definition[i].fixed_value) {
          if (!fgetsSkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, fp, '!')) {
            if (first_read) {
              if (bigBuffer)
                free(bigBuffer);
              return (-1);
            }
            SDDS_SetError("Unable to read parameters--data ends prematurely (SDDS_ReadAsciiParameters)");
            return (0);
          }
          first_read = 0;
          bigBuffer[strlen(bigBuffer) - 1] = 0;
        } else
          strcpy(bigBuffer, layout->parameter_definition[i].fixed_value);
        if (!SDDS_ScanData(bigBuffer, layout->parameter_definition[i].type, 0, SDDS_dataset->parameter[i], 0, 1)) {
          SDDS_SetError("Unable to read page--parameter scanning error (SDDS_ReadAsciiParameters)");
          return (0);
        }
      }
    }
#if defined(zLib)
  }
#endif
  if (bigBuffer)
    free(bigBuffer);
  return (1);
}

/**
 * @brief Reads the arrays from an ASCII file into the SDDS dataset.
 *
 * This function reads array data from an ASCII file and stores it in the provided SDDS dataset.
 * It supports reading from regular files, as well as GZIP and LZMA compressed files if enabled.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the arrays will be stored.
 *
 * @return Returns 1 on success, 0 on error, or -1 if end-of-file is reached before any arrays are read.
 *
 * @note The function checks the dataset for consistency before reading. It reads array dimensions and elements,
 *       handling multi-dimensional arrays appropriately. The array elements are scanned and stored according
 *       to their data types.
 */
int32_t SDDS_ReadAsciiArrays(SDDS_DATASET *SDDS_dataset) {
  int32_t i, j, length;
  SDDS_LAYOUT *layout;
  char *buffer = NULL;
  int32_t bufferSize = 0;
  char *bigBuffer = NULL;
  int32_t bigBufferSize = 0;
  char *bigBufferCopy;
  int32_t bigBufferCopySize;
  /*  ARRAY_DEFINITION *array_definition; */
  SDDS_ARRAY *array;
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  struct lzmafile *lzmafp = NULL;

  layout = &SDDS_dataset->layout;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = layout->gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = layout->lzmafp;
    } else {
      fp = layout->fp;
    }
#if defined(zLib)
  }
#endif
  if (layout->n_arrays > 0) {
    if (!(bigBuffer = SDDS_Malloc(sizeof(*bigBuffer) * (bigBufferSize = INITIAL_BIG_BUFFER_SIZE)))) {
      SDDS_SetError("Unable to read parameters--buffer allocation failure (SDDS_ReadAsciiArrays)");
      return (0);
    }
  }
  for (i = 0; i < layout->n_arrays; i++) {
#if defined(zLib)
    if (SDDS_dataset->layout.gzipFile) {
      if (!fgetsGZipSkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, gzfp, '!') || SDDS_StringIsBlank(bigBuffer)) {
        if (i == 0)
          return (-1);
        SDDS_SetError("Unable to read array--dimensions missing (SDDS_ReadAsciiArrays)");
        return (0);
      }
    } else {
#endif
      if (SDDS_dataset->layout.lzmaFile) {
        if (!fgetsLZMASkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, lzmafp, '!') || SDDS_StringIsBlank(bigBuffer)) {
          if (i == 0)
            return (-1);
          SDDS_SetError("Unable to read array--dimensions missing (SDDS_ReadAsciiArrays)");
          return (0);
        }
      } else {
        if (!fgetsSkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, fp, '!') || SDDS_StringIsBlank(bigBuffer)) {
          if (i == 0)
            return (-1);
          SDDS_SetError("Unable to read array--dimensions missing (SDDS_ReadAsciiArrays)");
          return (0);
        }
      }
#if defined(zLib)
    }
#endif
    if (!(array = SDDS_dataset->array + i)) {
      SDDS_SetError("Unable to read array--pointer to structure storage area is NULL (SDDS_ReadAsciiArrays)");
      return (0);
    }
    if (array->definition && !SDDS_FreeArrayDefinition(array->definition)) {
      SDDS_SetError("Unable to get array--array definition corrupted (SDDS_ReadAsciiArrays)");
      return (0);
    }
    if (!SDDS_CopyArrayDefinition(&array->definition, layout->array_definition + i)) {
      SDDS_SetError("Unable to read array--definition copy failed (SDDS_ReadAsciiArrays)");
      return (0);
    }
    /*if (array->dimension) free(array->dimension); */
    if (!(array->dimension = SDDS_Realloc(array->dimension, sizeof(*array->dimension) * array->definition->dimensions))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_ReadAsciiArrays)");
      return (0);
    }
    array->elements = 1;
    if ((length = strlen(bigBuffer)) >= bufferSize) {
      if (!(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * (bufferSize = 2 * length)))) {
        SDDS_SetError("Unable to scan data--allocation failure (SDDS_ReadAsciiArrays");
        return (0);
      }
    }
    for (j = 0; j < array->definition->dimensions; j++) {
      if (SDDS_GetToken(bigBuffer, buffer, SDDS_MAXLINE) <= 0 || sscanf(buffer, "%" SCNd32, array->dimension + j) != 1 || array->dimension[j] < 0) {
        SDDS_SetError("Unable to read array--dimensions missing or negative (SDDS_ReadAsciiArrays)");
        return (0);
      }
      array->elements *= array->dimension[j];
    }
    if (!array->elements)
      continue;
    if (array->data)
      free(array->data);
    array->data = array->pointer = NULL;
    if (!(array->data = SDDS_Realloc(array->data, array->elements * SDDS_type_size[array->definition->type - 1]))) {
      SDDS_SetError("Unable to read array--allocation failure (SDDS_ReadAsciiArrays)");
      return (0);
    }
    SDDS_ZeroMemory(array->data, array->elements * SDDS_type_size[array->definition->type - 1]);
    j = 0;
    bigBuffer[0] = 0;
    do {
      if (SDDS_StringIsBlank(bigBuffer)) {
        bigBuffer[0] = 0;
#if defined(zLib)
        if (SDDS_dataset->layout.gzipFile) {
          if (!fgetsGZipSkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, gzfp, '!') || SDDS_StringIsBlank(bigBuffer)) {
            SDDS_SetError("Unable to read array--dimensions missing (SDDS_ReadAsciiArrays)");
            return (0);
          }
        } else {
#endif
          if (SDDS_dataset->layout.lzmaFile) {
            if (!fgetsLZMASkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, lzmafp, '!') || SDDS_StringIsBlank(bigBuffer)) {
              SDDS_SetError("Unable to read array--dimensions missing (SDDS_ReadAsciiArrays)");
              return (0);
            }
          } else {
            if (!fgetsSkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, fp, '!') || SDDS_StringIsBlank(bigBuffer)) {
              SDDS_SetError("Unable to read array--dimensions missing (SDDS_ReadAsciiArrays)");
              return (0);
            }
          }
#if defined(zLib)
        }
#endif
      }
      /* copy the bigBuffer because SDDS_ScanData2 will change the bigBufferCopy pointer that SDDS_ScanData
         cannot do becuase bigBuffer is a static variable. This change was implemented to greatly improve
         the speed of reading a very large line of array elements. The previous version just did a lot of
         strcpy commands to these huge lines which was really slow. */
      bigBufferCopy = bigBuffer;
      bigBufferCopySize = strlen(bigBufferCopy);
      do {
        if (!SDDS_ScanData2(bigBufferCopy, &bigBufferCopy, &bigBufferCopySize, array->definition->type, array->definition->field_length, array->data, j, 0)) {
          SDDS_SetError("Unable to read array--error scanning data element (SDDS_ReadAsciiArrays)");
          return (0);
        }
      } while (++j < array->elements && !SDDS_StringIsBlank(bigBufferCopy));
      bigBuffer[0] = 0;
    } while (j < array->elements);
  }
  if (buffer)
    free(buffer);
  if (bigBuffer)
    free(bigBuffer);
  return (1);
}

/**
 * @brief Reads the next SDDS ASCII page into memory with optional data sparsity and statistics.
 *
 * This function reads the next page of data from an ASCII SDDS file into the provided dataset.
 * It supports reading data with specified sparsity (interval and offset) and can compute statistics
 * such as average, median, minimum, or maximum over the sparse data.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the data will be stored.
 * @param sparse_interval Interval for sparsity; read every nth row if greater than 1.
 * @param sparse_offset Offset for sparsity; number of initial rows to skip.
 * @param sparse_statistics Statistic to compute over the sparse data:
 *                          - 0: None
 *                          - 1: Average
 *                          - 2: Median
 *                          - 3: Minimum
 *                          - 4: Maximum
 *
 * @return Returns the page number on success, -1 if end-of-file is reached, or 0 on error.
 *
 * @note The function utilizes `SDDS_ReadAsciiPageDetailed()` to perform the actual reading.
 */
int32_t SDDS_ReadAsciiPage(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset, int32_t sparse_statistics) {
  return SDDS_ReadAsciiPageDetailed(SDDS_dataset, sparse_interval, sparse_offset, 0, sparse_statistics);
}

/**
 * @brief Reads the last specified number of rows from an ASCII page of an SDDS dataset.
 *
 * This function reads only the last specified number of rows from the next ASCII page in the SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the data will be stored.
 * @param last_rows Number of rows to read from the end of the page.
 *
 * @return Returns the page number on success, -1 if end-of-file is reached, or 0 on error.
 *
 * @note The function utilizes `SDDS_ReadAsciiPageDetailed()` to perform the actual reading.
 */
int32_t SDDS_ReadAsciiPageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows) {
  return SDDS_ReadAsciiPageDetailed(SDDS_dataset, 1, 0, last_rows, 0);
}

/**
 * @brief Reads a detailed page of data from an ASCII file into an SDDS dataset with optional sparsity and statistics.
 *
 * This function reads a page of data from an ASCII SDDS file into the provided dataset, supporting various options for data sparsity and statistical processing.
 * It allows specifying a sparse interval and offset to read every nth row starting from a specific offset, as well as reading only the last specified number of rows.
 * Additionally, it can compute statistical measures over the sparse data, such as average, median, minimum, or maximum.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the data will be stored.
 * @param sparse_interval Interval for sparsity; reads every nth row if greater than 1. Use 1 to read every row.
 * @param sparse_offset Offset from the first row to start reading. Rows before this offset are skipped.
 * @param last_rows Number of last rows to read. If greater than 0, only the last @p last_rows rows are read, overriding @p sparse_interval and @p sparse_offset.
 *                  Use 0 to read all rows according to @p sparse_interval and @p sparse_offset.
 * @param sparse_statistics Statistical operation to perform over the sparse data:
 *                          - 0: None (no statistical operation)
 *                          - 1: Average
 *                          - 2: Median
 *                          - 3: Minimum
 *                          - 4: Maximum
 *
 * @return Returns the page number on success,
 *         -1 if end-of-file is reached before any data is read,
 *         0 on error.
 *
 * @note This function handles reading from regular files, as well as GZIP and LZMA compressed files if enabled.
 *       It updates the internal state of the dataset, including the page number and number of rows read.
 *       If @p last_rows is specified and greater than 0, it overrides @p sparse_interval and @p sparse_offset.
 *       In case of errors during reading, the function attempts to recover if @c autoRecover is enabled in the dataset.
 */
int32_t SDDS_ReadAsciiPageDetailed(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset, int64_t last_rows, int32_t sparse_statistics) {
  SDDS_LAYOUT *layout;
  int32_t no_row_counts, end_of_data, retval, lineCount;
  int64_t n_rows, i, j, k, rows_to_store;
  /*  int32_t page_number; */
  char s[SDDS_MAXLINE];
  char *dataRead, *bigBufferCopy;
  int32_t bigBufferCopySize;
  char *bigBuffer = NULL;
  int32_t bigBufferSize = 0;
  /*  char *ptr; */
#if defined(zLib)
  gzFile gzfp = NULL;
#endif
  FILE *fp = NULL;
  void **statData=NULL;
  double statResult;
  struct lzmafile *lzmafp = NULL;
  /*  double value; */

  if (SDDS_dataset->autoRecovered)
    return -1;

  SDDS_SetReadRecoveryMode(SDDS_dataset, 0);

#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    gzfp = SDDS_dataset->layout.gzfp;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      lzmafp = SDDS_dataset->layout.lzmafp;
    } else {
      fp = SDDS_dataset->layout.fp;
    }
#if defined(zLib)
  }
#endif
  if (SDDS_dataset->page_number == -1)
    /* end of file already hit */
    return -1;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (gzeof(gzfp) && SDDS_dataset->page_number > 0)
      /* end of file and not first page about to be read */
      return SDDS_dataset->page_number = -1;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (lzma_eof(lzmafp) && SDDS_dataset->page_number > 0)
        /* end of file and not first page about to be read */
        return SDDS_dataset->page_number = -1;
    } else {
      if (feof(fp) && SDDS_dataset->page_number > 0)
        /* end of file and not first page about to be read */
        return SDDS_dataset->page_number = -1;
    }
#if defined(zLib)
  }
#endif
  if (!SDDS_AsciiDataExpected(SDDS_dataset) && SDDS_dataset->page_number != 0)
    /* if no columns or arrays and only fixed value parameters, then only one "page" allowed */
    return (SDDS_dataset->page_number = -1);

#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (SDDS_dataset->page_number == 0)
      for (i = 0; i < SDDS_dataset->layout.data_mode.additional_header_lines; i++) {
        if (!fgetsGZipSkipComments(SDDS_dataset, s, SDDS_MAXLINE, gzfp, '!'))
          return (SDDS_dataset->page_number = -1); /* indicates end of data */
      }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (SDDS_dataset->page_number == 0)
        for (i = 0; i < SDDS_dataset->layout.data_mode.additional_header_lines; i++) {
          if (!fgetsLZMASkipComments(SDDS_dataset, s, SDDS_MAXLINE, lzmafp, '!'))
            return (SDDS_dataset->page_number = -1); /* indicates end of data */
        }
    } else {
      if (SDDS_dataset->page_number == 0)
        for (i = 0; i < SDDS_dataset->layout.data_mode.additional_header_lines; i++) {
          if (!fgetsSkipComments(SDDS_dataset, s, SDDS_MAXLINE, fp, '!'))
            return (SDDS_dataset->page_number = -1); /* indicates end of data */
        }
    }
#if defined(zLib)
  }
#endif

  /* the next call increments the page number */
  if (!SDDS_StartPage(SDDS_dataset, 0)) {
    SDDS_SetError("Unable to read page--couldn't start page (SDDS_ReadAsciiPage)");
    return (0);
  }

  layout = &SDDS_dataset->layout;

  if ((retval = SDDS_ReadAsciiParameters(SDDS_dataset)) < 1) {
    if (retval)
      return (SDDS_dataset->page_number = retval);
    SDDS_SetError("Unable to read page--couldn't read parameters (SDDS_ReadAsciiPage)");
    return (0);
  }
  if ((retval = SDDS_ReadAsciiArrays(SDDS_dataset)) < 1) {
    if (retval)
      return (SDDS_dataset->page_number = retval);
    SDDS_SetError("Unable to read page--couldn't read arrays (SDDS_ReadAsciiPage)");
    return (0);
  }

  if (last_rows < 0)
    last_rows = 0;
  if (sparse_interval <= 0)
    sparse_interval = 1;
  if (sparse_offset < 0)
    sparse_offset = 0;

  SDDS_dataset->rowcount_offset = -1;
  if (layout->n_columns) {
    if (!(bigBuffer = SDDS_Malloc(sizeof(*bigBuffer) * (bigBufferSize = INITIAL_BIG_BUFFER_SIZE)))) {
      SDDS_SetError("Unable to read parameters--buffer allocation failure (SDDS_ReadAsciiPage)");
      return (0);
    }

    n_rows = 0;
    no_row_counts = 0;
    if (!SDDS_dataset->layout.data_mode.no_row_counts) {
      /* read the number of rows */
#if defined(zLib)
      if (SDDS_dataset->layout.gzipFile) {
        if (!fgetsGZipSkipComments(SDDS_dataset, s, SDDS_MAXLINE, gzfp, '!')) {
          if (bigBuffer)
            free(bigBuffer);
          return (SDDS_dataset->page_number = -1); /* indicates end of data */
        }
      } else {
#endif
        if (SDDS_dataset->layout.lzmaFile) {
          if (!fgetsLZMASkipComments(SDDS_dataset, s, SDDS_MAXLINE, lzmafp, '!')) {
            if (bigBuffer)
              free(bigBuffer);
            return (SDDS_dataset->page_number = -1); /* indicates end of data */
          }
        } else {
          do {
            SDDS_dataset->rowcount_offset = ftell(fp);
            if (!fgets(s, SDDS_MAXLINE, fp)) {
              if (bigBuffer)
                free(bigBuffer);
              return (SDDS_dataset->page_number = -1); /* indicates end of data */
            }
          } while (s[0] == '!');
        }
#if defined(zLib)
      }
#endif
      if (sscanf(s, "%" SCNd64, &n_rows) != 1 || n_rows < 0) {
        SDDS_SetError("Unable to read page--file has no (valid) number-of-rows entry (SDDS_ReadAsciiPage)");
        return (0);
      }
      if (n_rows > SDDS_GetRowLimit()) {
        /* number of rows is "unreasonably" large---treat like end-of-file */
        if (bigBuffer)
          free(bigBuffer);
        return (SDDS_dataset->page_number = -1);
      }
      if (last_rows) {
        sparse_interval = 1;
        sparse_offset = n_rows - last_rows;
        if (sparse_offset < 0)
          sparse_offset = 0;
      }
      rows_to_store = (n_rows - sparse_offset) / sparse_interval + 2;
    } else {
      // fix last_rows when no row count is available
      no_row_counts = 1;
      n_rows = TABLE_LENGTH_INCREMENT;
      rows_to_store = n_rows;
    }

    if (rows_to_store >= SDDS_dataset->n_rows_allocated) {
      /* lengthen the page */
      if (!SDDS_LengthenTable(SDDS_dataset, rows_to_store - SDDS_dataset->n_rows_allocated)) {
        SDDS_SetError("Unable to read page--couldn't lengthen data page (SDDS_ReadAsciiPage)");
        return (0);
      }
    }

    /* read the page values */
    j = end_of_data = k = 0;
    s[0] = 0;
    if (!no_row_counts && n_rows == 0) {
      SDDS_dataset->n_rows = 0;
      if (bigBuffer)
        free(bigBuffer);
      return (SDDS_dataset->page_number);
    }
    bigBuffer[0] = 0;
    bigBufferCopy = bigBuffer;
    do {
      if (j >= SDDS_dataset->n_rows_allocated) {
        /* lengthen the page */
        if (!SDDS_LengthenTable(SDDS_dataset, TABLE_LENGTH_INCREMENT)) {
          SDDS_SetError("Unable to read page--couldn't lengthen data page (SDDS_ReadAsciiPage)");
          return (0);
        }
      }
      lineCount = 0;
      dataRead = NULL;
      for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
        if (k == 0) {
          if (sparse_statistics != 0) {
            // Allocate buffer space for statistical sparsing
            if (i == 0) {
              statData = (void**)malloc(SDDS_dataset->layout.n_columns * sizeof(void*));
            }
            if (SDDS_FLOATING_TYPE(layout->column_definition[i].type)) {
              // Not ideal for SDDS_LONGDOUBLE but we may never run across this error
              statData[i] = (double*)calloc(sparse_interval, sizeof(double));
            }
          }
        }
        
        if (layout->column_definition[i].definition_mode & SDDS_WRITEONLY_DEFINITION)
          continue;
        if (SDDS_StringIsBlank(bigBufferCopy)) {
          bigBuffer[0] = 0;
          bigBufferCopy = bigBuffer;
          dataRead = NULL;
#if defined(zLib)
          if (SDDS_dataset->layout.gzipFile) {
            if (!(dataRead = fgetsGZipSkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, gzfp, '!')) || SDDS_StringIsBlank(bigBuffer)) {
              SDDS_dataset->n_rows = j;
              if (no_row_counts) {
                /* legitmate end of data set */
                end_of_data = 1;
                break;
              }
              /* error, but may be recoverable */
              gzseek(gzfp, 0L, SEEK_END);
              if (SDDS_dataset->autoRecover) {
                SDDS_dataset->autoRecovered = 1;
                SDDS_ClearErrors();
                if (bigBuffer)
                  free(bigBuffer);
                return (SDDS_dataset->page_number);
              }
              SDDS_SetError("Unable to read page (SDDS_ReadAsciiPage)");
              SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
              return (0);
            }
          } else {
#endif
            if (SDDS_dataset->layout.lzmaFile) {
              if (!(dataRead = fgetsLZMASkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, lzmafp, '!')) || SDDS_StringIsBlank(bigBuffer)) {
                SDDS_dataset->n_rows = j;
                if (no_row_counts) {
                  /* legitmate end of data set */
                  end_of_data = 1;
                  break;
                }
                /* error, but may be recoverable */
                lzma_seek(lzmafp, 0L, SEEK_END);
                if (SDDS_dataset->autoRecover) {
                  SDDS_dataset->autoRecovered = 1;
                  SDDS_ClearErrors();
                  if (bigBuffer)
                    free(bigBuffer);
                  return (SDDS_dataset->page_number);
                }
                SDDS_SetError("Unable to read page (SDDS_ReadAsciiPage)");
                SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
                return (0);
              }
            } else {
              if (!(dataRead = fgetsSkipCommentsResize(SDDS_dataset, &bigBuffer, &bigBufferSize, fp, '!')) || SDDS_StringIsBlank(bigBuffer)) {
                SDDS_dataset->n_rows = j;
                if (no_row_counts) {
                  /* legitmate end of data set */
                  end_of_data = 1;
                  break;
                }
                /* error, but may be recoverable */
                fseek(fp, 0L, SEEK_END);
                if (SDDS_dataset->autoRecover) {
                  SDDS_dataset->autoRecovered = 1;
                  SDDS_ClearErrors();
                  if (bigBuffer)
                    free(bigBuffer);
                  return (SDDS_dataset->page_number);
                }
                SDDS_SetError("Unable to read page (SDDS_ReadAsciiPage)");
                SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
                return (0);
              }
            }
#if defined(zLib)
          }
#endif
          lineCount++;
          bigBufferCopy = bigBuffer;
          bigBufferCopySize = strlen(bigBufferCopy);
        }
        if (!SDDS_ScanData2(bigBufferCopy, &bigBufferCopy, &bigBufferCopySize, layout->column_definition[i].type, layout->column_definition[i].field_length, SDDS_dataset->data[i], j, 0)) {
          /* error, but may be recoverable */
          SDDS_dataset->n_rows = j;
#if defined(zLib)
          if (SDDS_dataset->layout.gzipFile) {
            gzseek(gzfp, 0L, SEEK_END);
          } else {
#endif
            if (SDDS_dataset->layout.lzmaFile) {
              lzma_seek(lzmafp, 0L, SEEK_END);
            } else {
              fseek(fp, 0L, SEEK_END);
            }
#if defined(zLib)
          }
#endif
          if (SDDS_dataset->autoRecover) {
            SDDS_dataset->autoRecovered = 1;
            SDDS_ClearErrors();
            if (bigBuffer)
              free(bigBuffer);
            return (SDDS_dataset->page_number);
          }
          SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
          SDDS_SetError("Unable to read page--scanning error (SDDS_ReadAsciiPage)");
          return (0);
        }
        if (sparse_statistics != 0) {
          switch (layout->column_definition[i].type) {
          case SDDS_FLOAT:
            ((double*)statData[i])[k % sparse_interval] = (double)(((float*)SDDS_dataset->data[i])[j]);
            break;
          case SDDS_DOUBLE:
            ((double*)statData[i])[k % sparse_interval] = ((double*)SDDS_dataset->data[i])[j];
            break;
          case SDDS_LONGDOUBLE:
            ((double*)statData[i])[k % sparse_interval] = (double)(((long double*)SDDS_dataset->data[i])[j]);
            break;
          }
          if (SDDS_FLOATING_TYPE(layout->column_definition[i].type)) {
            if (sparse_statistics == 1) {
              // Sparse and get average statistics
              compute_average(&statResult, (double*)statData[i], (k % sparse_interval) + 1);
            } else if (sparse_statistics == 2) {
              // Sparse and get median statistics
              compute_median(&statResult, (double*)statData[i], (k % sparse_interval) + 1);
            } else if (sparse_statistics == 3) {
              // Sparse and get minimum statistics
              statResult = min_in_array((double*)statData[i], (k % sparse_interval) + 1);
            } else if (sparse_statistics == 4) {
              // Sparse and get maximum statistics
              statResult = max_in_array((double*)statData[i], (k % sparse_interval) + 1);
            }
          }
          switch (layout->column_definition[i].type) {
          case SDDS_FLOAT:
            ((float*)SDDS_dataset->data[i])[j] = statResult;
            break;
          case SDDS_DOUBLE:
            ((double*)SDDS_dataset->data[i])[j] = statResult;
            break;
          case SDDS_LONGDOUBLE:
            ((long double*)SDDS_dataset->data[i])[j] = statResult;
            break;
          }
        }

#if defined(DEBUG)
        fprintf(stderr, "line remaining = %s\n", bigBuffer);
#endif
      }
      if (end_of_data)
        /* ran out of data for no_row_counts=1 */
        break;
      if (layout->data_mode.lines_per_row != 0 && lineCount != layout->data_mode.lines_per_row) {
        sprintf(s, "Unable to read page--line layout error at line %" PRId64 " of page %" PRId32 " (SDDS_ReadAsciiPage)", j + 1, SDDS_dataset->page_number);
        SDDS_SetError(s);
        /* data ends prematurely, which is an error that may be recoverable */
#if defined(zLib)
        if (SDDS_dataset->layout.gzipFile) {
          gzseek(gzfp, 0L, SEEK_END);
        } else {
#endif
          if (SDDS_dataset->layout.lzmaFile) {
            lzma_seek(lzmafp, 0L, SEEK_END);
          } else {
            fseek(fp, 0L, SEEK_END);
          }
#if defined(zLib)
        }
#endif
        if (SDDS_dataset->autoRecover) {
          SDDS_dataset->autoRecovered = 1;
          SDDS_ClearErrors();
          if (bigBuffer)
            free(bigBuffer);
          return (SDDS_dataset->page_number);
        }
        SDDS_SetReadRecoveryMode(SDDS_dataset, 1);
        SDDS_dataset->n_rows = j;
        return (0);
      }
      if (layout->data_mode.lines_per_row != 0) {
        bigBuffer[0] = 0;
        bigBufferCopy = bigBuffer;
      }
      if (--sparse_offset < 0 && 
          (((sparse_statistics == 0) && (k % sparse_interval == 0)) || ((sparse_statistics != 0) && (k % sparse_interval == sparse_interval - 1))))
        j++;
      k++;
    } while (k < n_rows || no_row_counts);

    if (sparse_statistics != 0) {
      for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
        if (SDDS_FLOATING_TYPE(layout->column_definition[i].type)) {
          free(statData[i]);
        }
      }
      free(statData);
    }

    if (end_of_data && !(SDDS_dataset->page_number == 1) && j == 0 && !dataRead) {
      /* For end of data in no_row_counts=1 mode for any page other than the first,
       * an end-of-file is not a valid way to end an empty page (only an incomplete line is)
       */
      if (bigBuffer)
        free(bigBuffer);
      return SDDS_dataset->page_number = -1;
    }
    SDDS_dataset->n_rows = j;
  }
  if (bigBuffer)
    free(bigBuffer);

  return (SDDS_dataset->page_number);
}

/**
 * @brief Scans a string and saves the parsed value into a data pointer according to the specified data type.
 *
 * This function extracts data from a string and stores it in the provided data pointer.
 * It handles various SDDS data types, including integers, floating-point numbers, strings, and characters.
 * The function supports both fixed-field and variable-field formats and can process parameters and column data.
 *
 * @param string Pointer to the input string containing the data to be scanned.
 * @param type The SDDS data type to interpret the scanned data as. Valid types include:
 *             - SDDS_SHORT
 *             - SDDS_USHORT
 *             - SDDS_LONG
 *             - SDDS_ULONG
 *             - SDDS_LONG64
 *             - SDDS_ULONG64
 *             - SDDS_FLOAT
 *             - SDDS_DOUBLE
 *             - SDDS_LONGDOUBLE
 *             - SDDS_STRING
 *             - SDDS_CHARACTER
 * @param field_length Field length for fixed-field formats. Set to 0 for variable-field formats.
 *                     If negative, indicates left-padding should be removed.
 * @param data Void pointer to the data storage where the scanned value will be saved.
 *             Must be pre-allocated and appropriate for the specified data type.
 * @param index The index within the data array where the value should be stored.
 * @param is_parameter Set to 1 if the data is from an SDDS parameter; set to 0 for column data.
 *
 * @return Returns 1 on success, or 0 on error.
 *         If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note The function allocates a buffer internally to process the input string.
 *       It handles escape sequences for strings and characters using `SDDS_InterpretEscapes()`.
 *       For string data types, the function manages memory allocation for the data array elements.
 */
int32_t SDDS_ScanData(char *string, int32_t type, int32_t field_length, void *data, int64_t index, int32_t is_parameter) {
  char *buffer = NULL;
  int32_t abs_field_length, length;
  int32_t bufferSize = 0;

  abs_field_length = abs(field_length);
  if (!string) {
    SDDS_SetError("Unable to scan data--input string is NULL (SDDS_ScanData)");
    return (0);
  }
  if (!data) {
    SDDS_SetError("Unable to scan data--data pointer is NULL (SDDS_ScanData)");
    return (0);
  }
  if (!(buffer = SDDS_Malloc(sizeof(*buffer) * (bufferSize = SDDS_MAXLINE)))) {
    SDDS_SetError("Unable to scan data--allocation failure (SDDS_ScanData)");
    return (0);
  }
  if ((length = strlen(string)) < abs_field_length)
    length = abs_field_length;
  if (bufferSize <= length) {
    if (!(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * (bufferSize = 2 * length)))) {
      /* I allocate 2*length in the hopes that I won't need to realloc too often if I do this */
      SDDS_SetError("Unable to scan data--allocation failure (SDDS_ScanData)");
      return (0);
    }
  }
  if (type != SDDS_STRING) {
    /* for non-string data, fill buffer with string to be scanned */
    if (field_length) {
      /* fill with fixed number of characters */
      if (abs_field_length > (int32_t)strlen(string)) {
        strcpy(buffer, string);
        *string = 0;
      } else {
        strncpy(buffer, string, abs_field_length);
        buffer[field_length] = 0;
        memmove(string, string + abs_field_length, strlen(string + abs_field_length) + 1);
      }
    } else if (SDDS_GetToken(string, buffer, bufferSize) < 0) {
      SDDS_SetError("Unable to scan data--tokenizing error (SDDS_ScanData)");
      return (0);
    }
  }
  switch (type) {
  case SDDS_SHORT:
    if (sscanf(buffer, "%hd", ((short *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_USHORT:
    if (sscanf(buffer, "%hu", ((unsigned short *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_LONG:
    if (sscanf(buffer, "%" SCNd32, ((int32_t *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_ULONG:
    if (sscanf(buffer, "%" SCNu32, ((uint32_t *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_LONG64:
    if (sscanf(buffer, "%" SCNd64, ((int64_t *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_ULONG64:
    if (sscanf(buffer, "%" SCNu64, ((uint64_t *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_FLOAT:
    if (sscanf(buffer, "%f", ((float *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_DOUBLE:
    if (sscanf(buffer, "%lf", ((double *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_LONGDOUBLE:
    if (sscanf(buffer, "%Lf", ((long double *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_STRING:
    if (is_parameter) {
      int32_t len;
      if (((char **)data)[index]) {
        free(((char **)data)[index]);
        ((char **)data)[index] = NULL;
      }
      if ((len = strlen(string)) > 0) {
        if (string[len - 1] == '\r')
          string[len - 1] = 0;
      }
      if (string[0] == '"')
        SDDS_GetToken(string, buffer, bufferSize);
      else
        strcpy(buffer, string);
      SDDS_InterpretEscapes(buffer);
      if (SDDS_CopyString(((char **)data) + index, buffer)) {
        if (buffer)
          free(buffer);
        return (1);
      }
    } else {
      if (field_length) {
        if (abs_field_length > (int32_t)strlen(string)) {
          strcpy(buffer, string);
          *string = 0;
        } else {
          strncpy(buffer, string, abs_field_length);
          buffer[abs_field_length] = 0;
          memmove(string, string + abs_field_length, strlen(string + abs_field_length) + 1);
        }
        if (field_length < 0)
          SDDS_RemovePadding(buffer);
      } else if (SDDS_GetToken(string, buffer, bufferSize) < 0)
        break;
      if (((char **)data)[index]) {
        free(((char **)data)[index]);
        ((char **)data)[index] = NULL;
      }
      SDDS_InterpretEscapes(buffer);
      if (SDDS_CopyString(((char **)data) + index, buffer)) {
        if (buffer)
          free(buffer);
        return (1);
      }
    }
    break;
  case SDDS_CHARACTER:
    SDDS_InterpretEscapes(buffer);
    *(((char *)data) + index) = buffer[0];
    if (buffer)
      free(buffer);
    return (1);
  default:
    SDDS_SetError("Unknown data type encountered (SDDS_ScanData)");
    return (0);
  }
  SDDS_SetError("Unable to scan data--scanning or allocation error (SDDS_ScanData)");
  return (0);
}

/**
 * @brief Scans a string and saves the parsed value into a data pointer, optimized for long strings.
 *
 * This function is similar to `SDDS_ScanData` but optimized for very long strings.
 * It modifies the input string by advancing the string pointer and reducing its length after each call,
 * which can improve performance when processing large amounts of data.
 *
 * @param string Pointer to the input string containing the data to be scanned.
 * @param pstring Pointer to the string pointer; this is updated to point to the next unread character.
 * @param strlength Pointer to the length of the string; this is updated as the string is consumed.
 * @param type The SDDS data type to interpret the scanned data as. Valid types include:
 *             - SDDS_SHORT
 *             - SDDS_USHORT
 *             - SDDS_LONG
 *             - SDDS_ULONG
 *             - SDDS_LONG64
 *             - SDDS_ULONG64
 *             - SDDS_FLOAT
 *             - SDDS_DOUBLE
 *             - SDDS_LONGDOUBLE
 *             - SDDS_STRING
 *             - SDDS_CHARACTER
 * @param field_length Field length for fixed-field formats. Set to 0 for variable-field formats.
 *                     If negative, indicates left-padding should be removed.
 * @param data Void pointer to the data storage where the scanned value will be saved.
 *             Must be pre-allocated and appropriate for the specified data type.
 * @param index The index within the data array where the value should be stored.
 * @param is_parameter Set to 1 if the data is from an SDDS parameter; set to 0 for column data.
 *
 * @return Returns 1 on success, or 0 on error.
 *         If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note This function modifies the input string by advancing the pointer and reducing the length,
 *       which can lead to the original string being altered after each call.
 *       It is more efficient for processing very long strings compared to `SDDS_ScanData`.
 */
int32_t SDDS_ScanData2(char *string, char **pstring, int32_t *strlength, int32_t type, int32_t field_length, void *data, int64_t index, int32_t is_parameter) {
  char *buffer = NULL;
  int32_t abs_field_length, length;
  int32_t bufferSize = 0;

  abs_field_length = abs(field_length);
  if (!string) {
    SDDS_SetError("Unable to scan data--input string is NULL (SDDS_ScanData2)");
    return (0);
  }
  if (!data) {
    SDDS_SetError("Unable to scan data--data pointer is NULL (SDDS_ScanData2)");
    return (0);
  }
  if (!(buffer = SDDS_Malloc(sizeof(*buffer) * (bufferSize = SDDS_MAXLINE)))) {
    SDDS_SetError("Unable to scan data--allocation failure (SDDS_ScanData2)");
    return (0);
  }
  length = *strlength;
  if (length < abs_field_length)
    length = abs_field_length;
  if (bufferSize <= length) {
    if (!(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * (bufferSize = 2 * length)))) {
      /* I allocate 2*length in the hopes that I won't need to realloc too often if I do this */
      SDDS_SetError("Unable to scan data--allocation failure (SDDS_ScanData2)");
      return (0);
    }
  }
  if (type != SDDS_STRING) {
    /* for non-string data, fill buffer with string to be scanned */
    if (field_length) {
      /* fill with fixed number of characters */
      if (abs_field_length > *strlength) {
        strcpy(buffer, string);
        **pstring = 0;
        *strlength = 0;
      } else {
        strncpy(buffer, string, abs_field_length);
        buffer[abs_field_length] = 0;
        *pstring += abs_field_length;
        *strlength -= abs_field_length;
      }
    } else if (SDDS_GetToken2(string, pstring, strlength, buffer, bufferSize) < 0) {
      SDDS_SetError("Unable to scan data--tokenizing error (SDDS_ScanData2)");
      return (0);
    }
  }
  switch (type) {
  case SDDS_SHORT:
    if (sscanf(buffer, "%hd", ((short *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_USHORT:
    if (sscanf(buffer, "%hu", ((unsigned short *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_LONG:
    if (sscanf(buffer, "%" SCNd32, ((int32_t *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_ULONG:
    if (sscanf(buffer, "%" SCNu32, ((uint32_t *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_LONG64:
    if (sscanf(buffer, "%" SCNd64, ((int64_t *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_ULONG64:
    if (sscanf(buffer, "%" SCNu64, ((uint64_t *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_FLOAT:
    if (sscanf(buffer, "%f", ((float *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_DOUBLE:
    if (sscanf(buffer, "%lf", ((double *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_LONGDOUBLE:
    if (sscanf(buffer, "%Lf", ((long double *)data) + index) == 1) {
      if (buffer)
        free(buffer);
      return (1);
    }
    break;
  case SDDS_STRING:
    if (is_parameter) {
      int32_t len;
      if (((char **)data)[index]) {
        free(((char **)data)[index]);
        ((char **)data)[index] = NULL;
      }
      if ((len = *strlength) > 0) {
        if (*pstring[len - 1] == '\r') {
          *pstring[len - 1] = 0;
          *strlength -= 1;
        }
      }
      if (*pstring[0] == '"')
        SDDS_GetToken2(*pstring, pstring, strlength, buffer, bufferSize);
      else
        strcpy(buffer, string);
      SDDS_InterpretEscapes(buffer);
      if (SDDS_CopyString(((char **)data) + index, buffer)) {
        if (buffer)
          free(buffer);
        return (1);
      }
    } else {
      if (field_length) {
        if (abs_field_length > *strlength) {
          strcpy(buffer, string);
          **pstring = 0;
          *strlength = 0;
        } else {
          strncpy(buffer, string, abs_field_length);
          buffer[abs_field_length] = 0;
          *pstring += abs_field_length;
          *strlength -= abs_field_length;
        }
        if (field_length < 0)
          SDDS_RemovePadding(buffer);
      } else if (SDDS_GetToken2(string, pstring, strlength, buffer, bufferSize) < 0)
        break;
      if (((char **)data)[index]) {
        free(((char **)data)[index]);
        ((char **)data)[index] = NULL;
      }
      SDDS_InterpretEscapes(buffer);
      if (SDDS_CopyString(((char **)data) + index, buffer)) {
        if (buffer)
          free(buffer);
        return (1);
      }
    }
    break;
  case SDDS_CHARACTER:
    SDDS_InterpretEscapes(buffer);
    *(((char *)data) + index) = buffer[0];
    if (buffer)
      free(buffer);
    return 1;
  default:
    SDDS_SetError("Unknown data type encountered (SDDS_ScanData2)");
    return (0);
  }
  SDDS_SetError("Unable to scan data--scanning or allocation error (SDDS_ScanData2)");
  return (0);
}

/**
 * @brief Checks whether the SDDS dataset expects ASCII data input.
 *
 * This function determines if the provided SDDS dataset is expecting ASCII data.
 * The dataset expects ASCII data if it has columns, arrays, or parameters without fixed values.
 * If the dataset only contains parameters with fixed values and no columns or arrays, it does not expect ASCII data.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset to check.
 *
 * @return Returns 1 if ASCII data is expected, or 0 if ASCII data is not expected.
 */
int32_t SDDS_AsciiDataExpected(SDDS_DATASET *SDDS_dataset) {
  int32_t i;
  if (SDDS_dataset->layout.n_columns || SDDS_dataset->layout.n_arrays)
    return (1);
  for (i = 0; i < SDDS_dataset->layout.n_parameters; i++)
    if (!SDDS_dataset->layout.parameter_definition[i].fixed_value)
      return (1);
  return (0);
}

/**
 * @brief Updates the current ASCII page of an SDDS dataset with new data.
 *
 * This function updates the ASCII page of the provided SDDS dataset, appending any new rows that have been added since the last write.
 * It handles updating the row count in the file and ensures data consistency.
 * If the dataset is not currently writing a page, it initiates a new page write.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset to update.
 * @param mode Mode flags that control the update behavior. Common modes include:
 *             - `FLUSH_TABLE`: Flushes the data table after writing.
 *
 * @return Returns 1 on success, or 0 on error.
 *         If an error occurs, an error message is set via `SDDS_SetError()`.
 *
 * @note This function cannot be used with compressed files (GZIP or LZMA). It requires a regular ASCII file.
 *       It also handles updating internal state variables such as the number of rows written and the last row written.
 */
int32_t SDDS_UpdateAsciiPage(SDDS_DATASET *SDDS_dataset, uint32_t mode) {
  FILE *fp;
  int32_t code;
  int64_t i, rows, offset;
  SDDS_FILEBUFFER *fBuffer;

#ifdef DEBUG
  fprintf(stderr, "%" PRId64 " virtual rows present, first=%" PRId32 "\n", SDDS_CountRowsOfInterest(SDDS_dataset), SDDS_dataset->first_row_in_mem);
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_UpdateAsciiPage"))
    return (0);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    SDDS_SetError("Unable to perform page updates on a gzip file (SDDS_UpdateAsciiPage)");
    return 0;
  }
#endif
  if (SDDS_dataset->layout.lzmaFile) {
    SDDS_SetError("Unable to perform page updates on an .lzma or .xz file (SDDS_UpdateAsciiPage)");
    return 0;
  }
  if (!SDDS_dataset->writing_page) {
    if (!(code = SDDS_WriteAsciiPage(SDDS_dataset)))
      return 0;
    if (mode & FLUSH_TABLE) {
      SDDS_FreeTableStrings(SDDS_dataset);
      SDDS_dataset->first_row_in_mem = SDDS_CountRowsOfInterest(SDDS_dataset);
      SDDS_dataset->last_row_written = -1;
      SDDS_dataset->n_rows = 0;
    }
    return code;
  }
  if (!(fp = SDDS_dataset->layout.fp)) {
    SDDS_SetError("Unable to update page--file pointer is NULL (SDDS_UpdateAsciiPage)");
    return (0);
  }
  fBuffer = &SDDS_dataset->fBuffer;
  if (!SDDS_FlushBuffer(fp, fBuffer)) {
    SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_UpdateAsciiPage)");
    return 0;
  }
  offset = ftell(fp);

  rows = SDDS_CountRowsOfInterest(SDDS_dataset) + SDDS_dataset->first_row_in_mem;
  if (rows == SDDS_dataset->n_rows_written)
    return (1);
  if (rows < SDDS_dataset->n_rows_written) {
    SDDS_SetError("Unable to update page--new number of rows less than previous number (SDDS_UpdateAsciiPage)");
    return (0);
  }
  if ((!SDDS_dataset->layout.data_mode.fixed_row_count) || (((rows + rows - SDDS_dataset->n_rows_written) / SDDS_dataset->layout.data_mode.fixed_row_increment) != (rows / SDDS_dataset->layout.data_mode.fixed_row_increment))) {
    if (!SDDS_dataset->layout.data_mode.no_row_counts) {
      if (SDDS_fseek(fp, SDDS_dataset->rowcount_offset, SEEK_SET) == -1) {
        SDDS_SetError("Unable to update page--failure doing fseek (SDDS_UpdateAsciiPage)");
        return (0);
      }
      /* overwrite the existing row count */
      if (SDDS_dataset->layout.data_mode.fixed_row_count) {
        if ((rows - SDDS_dataset->n_rows_written) + 1 > SDDS_dataset->layout.data_mode.fixed_row_increment) {
          SDDS_dataset->layout.data_mode.fixed_row_increment = (rows - SDDS_dataset->n_rows_written) + 1;
        }
        fprintf(fp, "%20" PRId64 "\n", ((rows / SDDS_dataset->layout.data_mode.fixed_row_increment) + 2) * SDDS_dataset->layout.data_mode.fixed_row_increment);
      } else
        fprintf(fp, "%20" PRId64 "\n", rows);
      if (SDDS_fseek(fp, offset, SEEK_SET) == -1) {
        SDDS_SetError("Unable to update page--failure doing fseek to end of page (SDDS_UpdateAsciiPage)");
        return (0);
      }
    }
  }
  for (i = SDDS_dataset->last_row_written + 1; i < SDDS_dataset->n_rows; i++)
    if (SDDS_dataset->row_flag[i])
      SDDS_WriteAsciiRow(SDDS_dataset, i, fp);
  if (!SDDS_FlushBuffer(fp, fBuffer)) {
    SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_UpdateAsciiPage)");
    return 0;
  }
  SDDS_dataset->last_row_written = SDDS_dataset->n_rows - 1;
  SDDS_dataset->n_rows_written = rows;
  if (mode & FLUSH_TABLE) {
    SDDS_FreeTableStrings(SDDS_dataset);
    SDDS_dataset->first_row_in_mem = rows;
    SDDS_dataset->last_row_written = -1;
    SDDS_dataset->n_rows = 0;
  }
  return (1);
}
