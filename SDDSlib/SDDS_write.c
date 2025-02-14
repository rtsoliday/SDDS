/**
 * @file SDDS_write.c
 * @brief Provides functions for writing SDDS layout headers.
 *
 * This file contains functions that handle writing SDDS (Self Describing Data Sets)
 * layout headers. These functions convert the internal layout structures into the
 * namelist format used for input/output operations.
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

#include "mdb.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDS_internal.h"

/**
 * @brief Converts blank strings to NULL.
 *
 * This utility function checks if a given string is NULL or consists solely of
 * whitespace characters. If so, it returns NULL; otherwise, it returns the original string.
 *
 * @param string The input string to check.
 * @return Returns NULL if the string is blank or NULL; otherwise, returns the original string.
 */
char *SDDS_BlankToNull(char *string) {
  if (!string || SDDS_StringIsBlank(string))
    return NULL;
  return string;
}

/**************************************************************************/
/* This routine writes the protocol version of the file, and should       */
/* never be changed!                                                      */
/**************************************************************************/

/**
 * @brief Writes the SDDS protocol version to a standard file.
 *
 * This function outputs the SDDS protocol version string to the provided
 * file pointer. It is crucial that the protocol version remains unchanged
 * to ensure compatibility.
 *
 * @param version_number The SDDS protocol version number to write.
 * @param fp The file pointer where the version string will be written.
 * @return Returns 1 on successful write, 0 if the file pointer is NULL.
 */
int32_t SDDS_WriteVersion(int32_t version_number, FILE *fp) {
  if (!fp)
    return (0);
  fprintf(fp, "SDDS%" PRId32 "\n", version_number);
  return (1);
}

/**
 * @brief Writes the SDDS protocol version to an LZMA-compressed file.
 *
 * This function outputs the SDDS protocol version string to the provided
 * LZMA-compressed file pointer. Maintaining the protocol version is essential
 * for ensuring the integrity of the SDDS file format.
 *
 * @param version_number The SDDS protocol version number to write.
 * @param lzmafp The LZMA-compressed file pointer where the version string will be written.
 * @return Returns 1 on successful write, 0 if the file pointer is NULL.
 */
int32_t SDDS_LZMAWriteVersion(int32_t version_number, struct lzmafile *lzmafp) {
  if (!lzmafp)
    return (0);
  lzma_printf(lzmafp, "SDDS%" PRId32 "\n", version_number);
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes the SDDS protocol version to a GZip-compressed file.
 *
 * This function outputs the SDDS protocol version string to the provided
 * GZip-compressed file pointer. The protocol version must remain consistent
 * to maintain SDDS file compatibility.
 *
 * @param version_number The SDDS protocol version number to write.
 * @param gzfp The GZip-compressed file pointer where the version string will be written.
 * @return Returns 1 on successful write, 0 if the file pointer is NULL.
 */
int32_t SDDS_GZipWriteVersion(int32_t version_number, gzFile gzfp) {
  if (!gzfp)
    return (0);
  gzprintf(gzfp, "SDDS%" PRId32 "\n", version_number);
  return (1);
}
#endif

/**************************************************************/
/* SDDS protocol version 1 routines begin here.               */
/* There are no routers for output since only the most recent */
/* protocol will ever be emitted.                             */
/**************************************************************/

/**
 * @brief Writes a namelist field to a standard file.
 *
 * This function formats and writes a single namelist field to the specified
 * file. It handles escaping of double quotes and determines whether to
 * enclose the value in quotes based on its content.
 *
 * @param fp The file pointer where the namelist field will be written.
 * @param name The name of the field.
 * @param value The value of the field.
 * @return Returns 1 on success, 0 if the name is NULL or memory allocation fails.
 */
int32_t SDDS_PrintNamelistField(FILE *fp, char *name, char *value) {
  char *buffer = NULL, *bPtr, *vPtr;
  if (!name)
    return 0;
  if (!value || !strlen(name))
    return 1;
  if (!strlen(value))
    fprintf(fp, "%s=\"\", ", name);
  else {
    if (strchr(value, '"')) {
      if (!(buffer = SDDS_Malloc(sizeof(*buffer) * 2 * strlen(value))))
        return 0;
      vPtr = value;
      bPtr = buffer;
      while (*vPtr) {
        if (*vPtr == '"')
          *bPtr++ = '\\';
        *bPtr++ = *vPtr++;
      }
      *bPtr = 0;
      value = buffer;
    }
    if (strpbrk(value, " ,*$\t\n\b"))
      fprintf(fp, "%s=\"%s\", ", name, value);
    else
      fprintf(fp, "%s=%s, ", name, value);
    if (buffer)
      free(buffer);
  }
  return 1;
}

/**
 * @brief Writes a namelist field to an LZMA-compressed file.
 *
 * This function formats and writes a single namelist field to the specified
 * LZMA-compressed file. It handles escaping of double quotes and determines
 * whether to enclose the value in quotes based on its content.
 *
 * @param lzmafp The LZMA-compressed file pointer where the namelist field will be written.
 * @param name The name of the field.
 * @param value The value of the field.
 * @return Returns 1 on success, 0 if the name is NULL or memory allocation fails.
 */
int32_t SDDS_LZMAPrintNamelistField(struct lzmafile *lzmafp, char *name, char *value) {
  char *buffer = NULL, *bPtr, *vPtr;
  if (!name)
    return 0;
  if (!value || !strlen(name))
    return 1;
  if (!strlen(value))
    lzma_printf(lzmafp, "%s=\"\", ", name);
  else {
    if (strchr(value, '"')) {
      if (!(buffer = SDDS_Malloc(sizeof(*buffer) * 2 * strlen(value))))
        return 0;
      vPtr = value;
      bPtr = buffer;
      while (*vPtr) {
        if (*vPtr == '"')
          *bPtr++ = '\\';
        *bPtr++ = *vPtr++;
      }
      *bPtr = 0;
      value = buffer;
    }
    if (strpbrk(value, " ,*$\t\n\b"))
      lzma_printf(lzmafp, "%s=\"%s\", ", name, value);
    else
      lzma_printf(lzmafp, "%s=%s, ", name, value);
    if (buffer)
      free(buffer);
  }
  return 1;
}

#if defined(zLib)
/**
 * @brief Writes a namelist field to a GZip-compressed file.
 *
 * This function formats and writes a single namelist field to the specified
 * GZip-compressed file. It handles escaping of double quotes and determines
 * whether to enclose the value in quotes based on its content.
 *
 * @param gzfp The GZip-compressed file pointer where the namelist field will be written.
 * @param name The name of the field.
 * @param value The value of the field.
 * @return Returns 1 on success, 0 if the name is NULL or memory allocation fails.
 */
int32_t SDDS_GZipPrintNamelistField(gzFile gzfp, char *name, char *value) {
  char *buffer = NULL, *bPtr, *vPtr;
  if (!name)
    return 0;
  if (!value || !strlen(name))
    return 1;
  if (!strlen(value))
    gzprintf(gzfp, "%s=\"\", ", name);
  else {
    if (strchr(value, '"')) {
      if (!(buffer = SDDS_Malloc(sizeof(*buffer) * 2 * strlen(value))))
        return 0;
      vPtr = value;
      bPtr = buffer;
      while (*vPtr) {
        if (*vPtr == '"')
          *bPtr++ = '\\';
        *bPtr++ = *vPtr++;
      }
      *bPtr = 0;
      value = buffer;
    }
    if (strpbrk(value, " ,*$\t\n\b"))
      gzprintf(gzfp, "%s=\"%s\", ", name, value);
    else
      gzprintf(gzfp, "%s=%s, ", name, value);
    if (buffer)
      free(buffer);
  }
  return 1;
}
#endif

/**
 * @brief Writes the SDDS description section to a standard file.
 *
 * This function writes the description section of the SDDS layout, including
 * optional text and contents fields. It encapsulates the data within
 * &description and &end tags.
 *
 * @param text The descriptive text for the SDDS layout.
 * @param contents The contents description for the SDDS layout.
 * @param fp The file pointer where the description will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL.
 */
int32_t SDDS_WriteDescription(char *text, char *contents, FILE *fp) {
  if (!fp)
    return 0;
  if (!text && !contents)
    return 1;
  fputs("&description ", fp);
  SDDS_PrintNamelistField(fp, "text", text);
  SDDS_PrintNamelistField(fp, "contents", contents);
  fputs("&end\n", fp);
  return 1;
}

/**
 * @brief Writes the SDDS description section to an LZMA-compressed file.
 *
 * This function writes the description section of the SDDS layout, including
 * optional text and contents fields. It encapsulates the data within
 * &description and &end tags in an LZMA-compressed file.
 *
 * @param text The descriptive text for the SDDS layout.
 * @param contents The contents description for the SDDS layout.
 * @param lzmafp The LZMA-compressed file pointer where the description will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL.
 */
int32_t SDDS_LZMAWriteDescription(char *text, char *contents, struct lzmafile *lzmafp) {
  if (!lzmafp)
    return 0;
  if (!text && !contents)
    return 1;
  lzma_puts("&description ", lzmafp);
  SDDS_LZMAPrintNamelistField(lzmafp, "text", text);
  SDDS_LZMAPrintNamelistField(lzmafp, "contents", contents);
  lzma_puts("&end\n", lzmafp);
  return 1;
}

#if defined(zLib)
/**
 * @brief Writes the SDDS description section to a GZip-compressed file.
 *
 * This function writes the description section of the SDDS layout, including
 * optional text and contents fields. It encapsulates the data within
 * &description and &end tags in a GZip-compressed file.
 *
 * @param text The descriptive text for the SDDS layout.
 * @param contents The contents description for the SDDS layout.
 * @param gzfp The GZip-compressed file pointer where the description will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL.
 */
int32_t SDDS_GZipWriteDescription(char *text, char *contents, gzFile gzfp) {
  if (!gzfp)
    return 0;
  if (!text && !contents)
    return 1;
  gzputs(gzfp, "&description ");
  SDDS_GZipPrintNamelistField(gzfp, "text", text);
  SDDS_GZipPrintNamelistField(gzfp, "contents", contents);
  gzputs(gzfp, "&end\n");
  return 1;
}
#endif

/**
 * @brief Writes a column definition to a standard file.
 *
 * This function outputs the definition of a single column in the SDDS layout.
 * It includes fields such as name, symbol, units, description, format string,
 * and data type. The definition is enclosed within &column and &end tags.
 *
 * @param column_definition Pointer to the column definition structure.
 * @param fp The file pointer where the column definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the column type is invalid.
 */
int32_t SDDS_WriteColumnDefinition(COLUMN_DEFINITION *column_definition, FILE *fp) {
  if (!fp || column_definition->type <= 0 || column_definition->type > SDDS_NUM_TYPES)
    return (0);

  fputs("&column ", fp);
  SDDS_PrintNamelistField(fp, "name", column_definition->name);
  SDDS_PrintNamelistField(fp, "symbol", SDDS_BlankToNull(column_definition->symbol));
  SDDS_PrintNamelistField(fp, "units", SDDS_BlankToNull(column_definition->units));
  SDDS_PrintNamelistField(fp, "description", SDDS_BlankToNull(column_definition->description));
  SDDS_PrintNamelistField(fp, "format_string", SDDS_BlankToNull(column_definition->format_string));
  SDDS_PrintNamelistField(fp, "type", SDDS_type_name[column_definition->type - 1]);
  fputs(" &end\n", fp);
  return (1);
}

/**
 * @brief Writes a column definition to an LZMA-compressed file.
 *
 * This function outputs the definition of a single column in the SDDS layout
 * to an LZMA-compressed file. It includes fields such as name, symbol, units,
 * description, format string, and data type. The definition is enclosed within
 * &column and &end tags.
 *
 * @param column_definition Pointer to the column definition structure.
 * @param lzmafp The LZMA-compressed file pointer where the column definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the column type is invalid.
 */
int32_t SDDS_LZMAWriteColumnDefinition(COLUMN_DEFINITION *column_definition, struct lzmafile *lzmafp) {
  if (!lzmafp || column_definition->type <= 0 || column_definition->type > SDDS_NUM_TYPES)
    return (0);

  lzma_puts("&column ", lzmafp);
  SDDS_LZMAPrintNamelistField(lzmafp, "name", column_definition->name);
  SDDS_LZMAPrintNamelistField(lzmafp, "symbol", SDDS_BlankToNull(column_definition->symbol));
  SDDS_LZMAPrintNamelistField(lzmafp, "units", SDDS_BlankToNull(column_definition->units));
  SDDS_LZMAPrintNamelistField(lzmafp, "description", SDDS_BlankToNull(column_definition->description));
  SDDS_LZMAPrintNamelistField(lzmafp, "format_string", SDDS_BlankToNull(column_definition->format_string));
  SDDS_LZMAPrintNamelistField(lzmafp, "type", SDDS_type_name[column_definition->type - 1]);
  lzma_puts(" &end\n", lzmafp);
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes a column definition to a GZip-compressed file.
 *
 * This function outputs the definition of a single column in the SDDS layout
 * to a GZip-compressed file. It includes fields such as name, symbol, units,
 * description, format string, and data type. The definition is enclosed within
 * &column and &end tags.
 *
 * @param column_definition Pointer to the column definition structure.
 * @param gzfp The GZip-compressed file pointer where the column definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the column type is invalid.
 */
int32_t SDDS_GZipWriteColumnDefinition(COLUMN_DEFINITION *column_definition, gzFile gzfp) {
  if (!gzfp || column_definition->type <= 0 || column_definition->type > SDDS_NUM_TYPES)
    return (0);

  gzputs(gzfp, "&column ");
  SDDS_GZipPrintNamelistField(gzfp, "name", column_definition->name);
  SDDS_GZipPrintNamelistField(gzfp, "symbol", SDDS_BlankToNull(column_definition->symbol));
  SDDS_GZipPrintNamelistField(gzfp, "units", SDDS_BlankToNull(column_definition->units));
  SDDS_GZipPrintNamelistField(gzfp, "description", SDDS_BlankToNull(column_definition->description));
  SDDS_GZipPrintNamelistField(gzfp, "format_string", SDDS_BlankToNull(column_definition->format_string));
  SDDS_GZipPrintNamelistField(gzfp, "type", SDDS_type_name[column_definition->type - 1]);
  gzputs(gzfp, " &end\n");
  return (1);
}
#endif

/**
 * @brief Writes a parameter definition to a standard file.
 *
 * This function outputs the definition of a single parameter in the SDDS layout.
 * It includes fields such as name, symbol, units, description, format string,
 * data type, and fixed value. The definition is enclosed within &parameter and &end tags.
 *
 * @param parameter_definition Pointer to the parameter definition structure.
 * @param fp The file pointer where the parameter definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the parameter type is invalid.
 */
int32_t SDDS_WriteParameterDefinition(PARAMETER_DEFINITION *parameter_definition, FILE *fp) {
  if (!fp || parameter_definition->type <= 0 || parameter_definition->type > SDDS_NUM_TYPES)
    return (0);
  fputs("&parameter ", fp);
  SDDS_PrintNamelistField(fp, "name", parameter_definition->name);
  SDDS_PrintNamelistField(fp, "symbol", SDDS_BlankToNull(parameter_definition->symbol));
  SDDS_PrintNamelistField(fp, "units", SDDS_BlankToNull(parameter_definition->units));
  SDDS_PrintNamelistField(fp, "description", SDDS_BlankToNull(parameter_definition->description));
  SDDS_PrintNamelistField(fp, "format_string", SDDS_BlankToNull(parameter_definition->format_string));
  SDDS_PrintNamelistField(fp, "type", SDDS_type_name[parameter_definition->type - 1]);
  SDDS_PrintNamelistField(fp, "fixed_value", parameter_definition->fixed_value);
  fputs("&end\n", fp);
  return (1);
}

/**
 * @brief Writes a parameter definition to an LZMA-compressed file.
 *
 * This function outputs the definition of a single parameter in the SDDS layout
 * to an LZMA-compressed file. It includes fields such as name, symbol, units,
 * description, format string, data type, and fixed value. The definition is
 * enclosed within &parameter and &end tags.
 *
 * @param parameter_definition Pointer to the parameter definition structure.
 * @param lzmafp The LZMA-compressed file pointer where the parameter definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the parameter type is invalid.
 */
int32_t SDDS_LZMAWriteParameterDefinition(PARAMETER_DEFINITION *parameter_definition, struct lzmafile *lzmafp) {
  if (!lzmafp || parameter_definition->type <= 0 || parameter_definition->type > SDDS_NUM_TYPES)
    return (0);
  lzma_puts("&parameter ", lzmafp);
  SDDS_LZMAPrintNamelistField(lzmafp, "name", parameter_definition->name);
  SDDS_LZMAPrintNamelistField(lzmafp, "symbol", SDDS_BlankToNull(parameter_definition->symbol));
  SDDS_LZMAPrintNamelistField(lzmafp, "units", SDDS_BlankToNull(parameter_definition->units));
  SDDS_LZMAPrintNamelistField(lzmafp, "description", SDDS_BlankToNull(parameter_definition->description));
  SDDS_LZMAPrintNamelistField(lzmafp, "format_string", SDDS_BlankToNull(parameter_definition->format_string));
  SDDS_LZMAPrintNamelistField(lzmafp, "type", SDDS_type_name[parameter_definition->type - 1]);
  SDDS_LZMAPrintNamelistField(lzmafp, "fixed_value", parameter_definition->fixed_value);
  lzma_puts("&end\n", lzmafp);
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes a parameter definition to a GZip-compressed file.
 *
 * This function outputs the definition of a single parameter in the SDDS layout
 * to a GZip-compressed file. It includes fields such as name, symbol, units,
 * description, format string, data type, and fixed value. The definition is
 * enclosed within &parameter and &end tags.
 *
 * @param parameter_definition Pointer to the parameter definition structure.
 * @param gzfp The GZip-compressed file pointer where the parameter definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the parameter type is invalid.
 */
int32_t SDDS_GZipWriteParameterDefinition(PARAMETER_DEFINITION *parameter_definition, gzFile gzfp) {
  if (!gzfp || parameter_definition->type <= 0 || parameter_definition->type > SDDS_NUM_TYPES)
    return (0);
  gzputs(gzfp, "&parameter ");
  SDDS_GZipPrintNamelistField(gzfp, "name", parameter_definition->name);
  SDDS_GZipPrintNamelistField(gzfp, "symbol", SDDS_BlankToNull(parameter_definition->symbol));
  SDDS_GZipPrintNamelistField(gzfp, "units", SDDS_BlankToNull(parameter_definition->units));
  SDDS_GZipPrintNamelistField(gzfp, "description", SDDS_BlankToNull(parameter_definition->description));
  SDDS_GZipPrintNamelistField(gzfp, "format_string", SDDS_BlankToNull(parameter_definition->format_string));
  SDDS_GZipPrintNamelistField(gzfp, "type", SDDS_type_name[parameter_definition->type - 1]);
  SDDS_GZipPrintNamelistField(gzfp, "fixed_value", parameter_definition->fixed_value);
  gzputs(gzfp, "&end\n");
  return (1);
}
#endif

/**
 * @brief Writes an associate definition to a standard file.
 *
 * This function outputs the definition of an associate in the SDDS layout.
 * It includes fields such as name, filename, contents, path, description, and
 * an SDDS flag. The definition is enclosed within &associate and &end tags.
 *
 * @param associate_definition Pointer to the associate definition structure.
 * @param fp The file pointer where the associate definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL.
 */
int32_t SDDS_WriteAssociateDefinition(ASSOCIATE_DEFINITION *associate_definition, FILE *fp) {
  if (!fp)
    return (0);

  fputs("&associate ", fp);
  SDDS_PrintNamelistField(fp, "name", associate_definition->name);
  SDDS_PrintNamelistField(fp, "filename", SDDS_BlankToNull(associate_definition->filename));
  SDDS_PrintNamelistField(fp, "contents", SDDS_BlankToNull(associate_definition->contents));
  SDDS_PrintNamelistField(fp, "path", SDDS_BlankToNull(associate_definition->path));
  SDDS_PrintNamelistField(fp, "description", SDDS_BlankToNull(associate_definition->description));
  fprintf(fp, "sdds=%" PRId32, associate_definition->sdds);
  fputs(" &end\n", fp);
  return (1);
}

/**
 * @brief Writes an associate definition to an LZMA-compressed file.
 *
 * This function outputs the definition of an associate in the SDDS layout
 * to an LZMA-compressed file. It includes fields such as name, filename,
 * contents, path, description, and an SDDS flag. The definition is enclosed
 * within &associate and &end tags.
 *
 * @param associate_definition Pointer to the associate definition structure.
 * @param lzmafp The LZMA-compressed file pointer where the associate definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL.
 */
int32_t SDDS_LZMAWriteAssociateDefinition(ASSOCIATE_DEFINITION *associate_definition, struct lzmafile *lzmafp) {
  if (!lzmafp)
    return (0);

  lzma_puts("&associate ", lzmafp);
  SDDS_LZMAPrintNamelistField(lzmafp, "name", associate_definition->name);
  SDDS_LZMAPrintNamelistField(lzmafp, "filename", SDDS_BlankToNull(associate_definition->filename));
  SDDS_LZMAPrintNamelistField(lzmafp, "contents", SDDS_BlankToNull(associate_definition->contents));
  SDDS_LZMAPrintNamelistField(lzmafp, "path", SDDS_BlankToNull(associate_definition->path));
  SDDS_LZMAPrintNamelistField(lzmafp, "description", SDDS_BlankToNull(associate_definition->description));
  lzma_printf(lzmafp, "sdds=%" PRId32, associate_definition->sdds);
  lzma_puts(" &end\n", lzmafp);
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes an associate definition to a GZip-compressed file.
 *
 * This function outputs the definition of an associate in the SDDS layout
 * to a GZip-compressed file. It includes fields such as name, filename,
 * contents, path, description, and an SDDS flag. The definition is enclosed
 * within &associate and &end tags.
 *
 * @param associate_definition Pointer to the associate definition structure.
 * @param gzfp The GZip-compressed file pointer where the associate definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL.
 */
int32_t SDDS_GZipWriteAssociateDefinition(ASSOCIATE_DEFINITION *associate_definition, gzFile gzfp) {
  if (!gzfp)
    return (0);

  gzputs(gzfp, "&associate ");
  SDDS_GZipPrintNamelistField(gzfp, "name", associate_definition->name);
  SDDS_GZipPrintNamelistField(gzfp, "filename", SDDS_BlankToNull(associate_definition->filename));
  SDDS_GZipPrintNamelistField(gzfp, "contents", SDDS_BlankToNull(associate_definition->contents));
  SDDS_GZipPrintNamelistField(gzfp, "path", SDDS_BlankToNull(associate_definition->path));
  SDDS_GZipPrintNamelistField(gzfp, "description", SDDS_BlankToNull(associate_definition->description));
  gzprintf(gzfp, "sdds=%" PRId32, associate_definition->sdds);
  gzputs(gzfp, " &end\n");
  return (1);
}
#endif

/**
 * @brief Writes the data mode section to a standard file.
 *
 * This function outputs the data mode settings of the SDDS layout to the
 * specified file pointer. It includes settings such as mode, lines per row,
 * row counts, endianess, column-major order, and fixed row counts.
 * The section is enclosed within &data and &end tags.
 *
 * @param layout Pointer to the SDDS layout structure containing data mode settings.
 * @param fp The file pointer where the data mode section will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the data mode is invalid.
 */
int32_t SDDS_WriteDataMode(SDDS_LAYOUT *layout, FILE *fp) {
  if (!fp || layout->data_mode.mode < 0 || layout->data_mode.mode > SDDS_NUM_DATA_MODES)
    return (0);

  fputs("&data ", fp);
  SDDS_PrintNamelistField(fp, "mode", SDDS_data_mode[layout->data_mode.mode - 1]);
  if (layout->data_mode.lines_per_row > 1)
    fprintf(fp, "lines_per_row=%" PRId32 ", ", layout->data_mode.lines_per_row);
  if (layout->data_mode.no_row_counts)
    fprintf(fp, "no_row_counts=1, ");
  if (layout->version >= 3) {
    if (layout->data_mode.mode == SDDS_BINARY) {
      if (layout->byteOrderDeclared == SDDS_BIGENDIAN)
        fprintf(fp, "endian=big, ");
      else
        fprintf(fp, "endian=little, ");
      if (layout->data_mode.column_major)
        fprintf(fp, "column_major_order=1, ");
    }
    if (layout->data_mode.fixed_row_count)
      fprintf(fp, "fixed_row_count=1, ");
  }
  fputs("&end\n", fp);
  return (1);
}

/**
 * @brief Writes the data mode section to an LZMA-compressed file.
 *
 * This function outputs the data mode settings of the SDDS layout to the
 * specified LZMA-compressed file pointer. It includes settings such as mode,
 * lines per row, row counts, endianess, column-major order, and fixed row counts.
 * The section is enclosed within &data and &end tags.
 *
 * @param layout Pointer to the SDDS layout structure containing data mode settings.
 * @param lzmafp The LZMA-compressed file pointer where the data mode section will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the data mode is invalid.
 */
int32_t SDDS_LZMAWriteDataMode(SDDS_LAYOUT *layout, struct lzmafile *lzmafp) {
  if (!lzmafp || layout->data_mode.mode < 0 || layout->data_mode.mode > SDDS_NUM_DATA_MODES)
    return (0);

  lzma_puts("&data ", lzmafp);
  SDDS_LZMAPrintNamelistField(lzmafp, "mode", SDDS_data_mode[layout->data_mode.mode - 1]);
  if (layout->data_mode.lines_per_row > 1)
    lzma_printf(lzmafp, "lines_per_row=%" PRId32 ", ", layout->data_mode.lines_per_row);
  if (layout->data_mode.no_row_counts)
    lzma_printf(lzmafp, "no_row_counts=1, ");
  if (layout->version >= 3) {
    if (layout->data_mode.mode == SDDS_BINARY) {
      if (layout->byteOrderDeclared == SDDS_BIGENDIAN)
        lzma_printf(lzmafp, "endian=big, ");
      else
        lzma_printf(lzmafp, "endian=little, ");
      if (layout->data_mode.column_major)
        lzma_printf(lzmafp, "column_major_order=1, ");
    }
    if (layout->data_mode.fixed_row_count)
      lzma_printf(lzmafp, "fixed_row_count=1, ");
  }
  lzma_puts("&end\n", lzmafp);
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes the data mode section to a GZip-compressed file.
 *
 * This function outputs the data mode settings of the SDDS layout to the
 * specified GZip-compressed file pointer. It includes settings such as mode,
 * lines per row, row counts, endianess, column-major order, and fixed row counts.
 * The section is enclosed within &data and &end tags.
 *
 * @param layout Pointer to the SDDS layout structure containing data mode settings.
 * @param gzfp The GZip-compressed file pointer where the data mode section will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the data mode is invalid.
 */
int32_t SDDS_GZipWriteDataMode(SDDS_LAYOUT *layout, gzFile gzfp) {
  if (!gzfp || layout->data_mode.mode < 0 || layout->data_mode.mode > SDDS_NUM_DATA_MODES)
    return (0);

  gzputs(gzfp, "&data ");
  SDDS_GZipPrintNamelistField(gzfp, "mode", SDDS_data_mode[layout->data_mode.mode - 1]);
  if (layout->data_mode.lines_per_row > 1)
    gzprintf(gzfp, "lines_per_row=%" PRId32 ", ", layout->data_mode.lines_per_row);
  if (layout->data_mode.no_row_counts)
    gzprintf(gzfp, "no_row_counts=1, ");
  if (layout->version >= 3) {
    if (layout->data_mode.mode == SDDS_BINARY) {
      if (layout->byteOrderDeclared == SDDS_BIGENDIAN)
        gzprintf(gzfp, "endian=big, ");
      else
        gzprintf(gzfp, "endian=little, ");
      if (layout->data_mode.column_major)
        gzprintf(gzfp, "column_major_order=1, ");
    }
    if (layout->data_mode.fixed_row_count)
      gzprintf(gzfp, "fixed_row_count=1, ");
  }
  gzputs(gzfp, "&end\n");
  return (1);
}
#endif

/**
 * @brief Writes an array definition to a standard file.
 *
 * This function outputs the definition of a single array in the SDDS layout.
 * It includes fields such as name, symbol, units, description, format string,
 * group name, data type, and dimensions. The definition is enclosed within
 * &array and &end tags.
 *
 * @param array_definition Pointer to the array definition structure.
 * @param fp The file pointer where the array definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the array type is invalid.
 */
int32_t SDDS_WriteArrayDefinition(ARRAY_DEFINITION *array_definition, FILE *fp) {
  if (!fp || array_definition->type <= 0 || array_definition->type > SDDS_NUM_TYPES)
    return (0);

  fputs("&array ", fp);
  SDDS_PrintNamelistField(fp, "name", array_definition->name);
  SDDS_PrintNamelistField(fp, "symbol", SDDS_BlankToNull(array_definition->symbol));
  SDDS_PrintNamelistField(fp, "units", SDDS_BlankToNull(array_definition->units));
  SDDS_PrintNamelistField(fp, "description", SDDS_BlankToNull(array_definition->description));
  SDDS_PrintNamelistField(fp, "format_string", SDDS_BlankToNull(array_definition->format_string));
  SDDS_PrintNamelistField(fp, "group_name", SDDS_BlankToNull(array_definition->group_name));
  SDDS_PrintNamelistField(fp, "type", SDDS_type_name[array_definition->type - 1]);
  if (array_definition->dimensions != 1) /* 1 is default */
    fprintf(fp, "dimensions=%" PRId32 ", ", array_definition->dimensions);
  fputs(" &end\n", fp);
  return (1);
}

/**
 * @brief Writes an array definition to an LZMA-compressed file.
 *
 * This function outputs the definition of a single array in the SDDS layout
 * to an LZMA-compressed file. It includes fields such as name, symbol, units,
 * description, format string, group name, data type, and dimensions.
 * The definition is enclosed within &array and &end tags.
 *
 * @param array_definition Pointer to the array definition structure.
 * @param lzmafp The LZMA-compressed file pointer where the array definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the array type is invalid.
 */
int32_t SDDS_LZMAWriteArrayDefinition(ARRAY_DEFINITION *array_definition, struct lzmafile *lzmafp) {
  if (!lzmafp || array_definition->type <= 0 || array_definition->type > SDDS_NUM_TYPES)
    return (0);

  lzma_puts("&array ", lzmafp);
  SDDS_LZMAPrintNamelistField(lzmafp, "name", array_definition->name);
  SDDS_LZMAPrintNamelistField(lzmafp, "symbol", SDDS_BlankToNull(array_definition->symbol));
  SDDS_LZMAPrintNamelistField(lzmafp, "units", SDDS_BlankToNull(array_definition->units));
  SDDS_LZMAPrintNamelistField(lzmafp, "description", SDDS_BlankToNull(array_definition->description));
  SDDS_LZMAPrintNamelistField(lzmafp, "format_string", SDDS_BlankToNull(array_definition->format_string));
  SDDS_LZMAPrintNamelistField(lzmafp, "group_name", SDDS_BlankToNull(array_definition->group_name));
  SDDS_LZMAPrintNamelistField(lzmafp, "type", SDDS_type_name[array_definition->type - 1]);
  if (array_definition->dimensions != 1) /* 1 is default */
    lzma_printf(lzmafp, "dimensions=%" PRId32 ", ", array_definition->dimensions);
  lzma_puts(" &end\n", lzmafp);
  return (1);
}

#if defined(zLib)
/**
 * @brief Writes an array definition to a GZip-compressed file.
 *
 * This function outputs the definition of a single array in the SDDS layout
 * to a GZip-compressed file. It includes fields such as name, symbol, units,
 * description, format string, group name, data type, and dimensions.
 * The definition is enclosed within &array and &end tags.
 *
 * @param array_definition Pointer to the array definition structure.
 * @param gzfp The GZip-compressed file pointer where the array definition will be written.
 * @return Returns 1 on success, 0 if the file pointer is NULL or the array type is invalid.
 */
int32_t SDDS_GZipWriteArrayDefinition(ARRAY_DEFINITION *array_definition, gzFile gzfp) {
  if (!gzfp || array_definition->type <= 0 || array_definition->type > SDDS_NUM_TYPES)
    return (0);

  gzputs(gzfp, "&array ");
  SDDS_GZipPrintNamelistField(gzfp, "name", array_definition->name);
  SDDS_GZipPrintNamelistField(gzfp, "symbol", SDDS_BlankToNull(array_definition->symbol));
  SDDS_GZipPrintNamelistField(gzfp, "units", SDDS_BlankToNull(array_definition->units));
  SDDS_GZipPrintNamelistField(gzfp, "description", SDDS_BlankToNull(array_definition->description));
  SDDS_GZipPrintNamelistField(gzfp, "format_string", SDDS_BlankToNull(array_definition->format_string));
  SDDS_GZipPrintNamelistField(gzfp, "group_name", SDDS_BlankToNull(array_definition->group_name));
  SDDS_GZipPrintNamelistField(gzfp, "type", SDDS_type_name[array_definition->type - 1]);
  if (array_definition->dimensions != 1) /* 1 is default */
    gzprintf(gzfp, "dimensions=%" PRId32 ", ", array_definition->dimensions);
  gzputs(gzfp, " &end\n");
  return (1);
}
#endif

