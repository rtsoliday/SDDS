/**
 * @file SDDS_process.c
 * @brief Processes SDDS layout headers and translates namelist structures.
 *
 * This file implements a suite of functions responsible for reading and processing
 * SDDS (Self Describing Data Sets) layout headers. The primary purpose of these
 * routines is to facilitate the translation between external namelist structures
 * used for input and the internal data structures that store the SDDS layout.
 *
 * Key functionalities include:
 * - Parsing description sections of SDDS datasets.
 * - Handling column, parameter, array, and associate definitions.
 * - Managing include commands and data modes.
 * - Providing utility functions for tag-value pair extraction and namelist parsing.
 *
 * The implemented functions ensure robust argument checking and support for
 * multiple protocol versions, allowing for future scalability and compatibility.
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

#undef DEBUG


/**************************************************************************/
/* These routines are the "routers" that call the processing routines for */
/* the protocol version of the file being read.  They also do a lot of    */
/* the argument checking.  I've avoided using arrays of function ponters  */
/* here to allow for the possibility that future versions may require     */
/* additional arguments.                                                  */
/**************************************************************************/

/**
 * @brief Process the description section of the SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param s Pointer to the description string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessDescription(SDDS_DATASET *SDDS_dataset, char *s) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessDescription"))
    return (0);
  if (!s) {
    SDDS_SetError("Unable to process description--namelist text pointer is NULL (SDDS_ProcessDescription)");
    return (0);
  }
  switch (SDDS_dataset->layout.version) {
  case 1:
    return (SDDS1_ProcessDescription(SDDS_dataset, s));
  case 2:
    return (SDDS1_ProcessDescription(SDDS_dataset, s));
  case 3:
    return (SDDS1_ProcessDescription(SDDS_dataset, s));
  case 4:
    return (SDDS1_ProcessDescription(SDDS_dataset, s));
  case 5:
    return (SDDS1_ProcessDescription(SDDS_dataset, s));
  default:
    SDDS_SetError("Unable to process description--protocol version number is invalid (SDDS_ProcessDescription)");
    return (0);
  }
}

/**
 * @brief Process the column definition section of the SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param s Pointer to the column definition string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessColumnDefinition(SDDS_DATASET *SDDS_dataset, char *s) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS1_ProcessColumnDefinition"))
    return (0);
  if (!s) {
    SDDS_SetError("Unable to process column definition--namelist text pointer NULL (SDDS1_ProcessColumnDefinition)");
    return (0);
  }
  switch (SDDS_dataset->layout.version) {
  case 1:
    return (SDDS1_ProcessColumnDefinition(SDDS_dataset, s));
  case 2:
    return (SDDS1_ProcessColumnDefinition(SDDS_dataset, s));
  case 3:
    return (SDDS1_ProcessColumnDefinition(SDDS_dataset, s));
  case 4:
    return (SDDS1_ProcessColumnDefinition(SDDS_dataset, s));
  case 5:
    return (SDDS1_ProcessColumnDefinition(SDDS_dataset, s));
  default:
    SDDS_SetError("Unable to process column definition--protocol version number is invalid (SDDS_ProcessColumnDefinition)");
    return (0);
  }
}

/**
 * @brief Process the parameter definition section of the SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param s Pointer to the parameter definition string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessParameterDefinition(SDDS_DATASET *SDDS_dataset, char *s) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessParameterDefinition"))
    return (0);
  if (!s) {
    SDDS_SetError("Unable to process parameter definition--namelist text pointer NULL (SDDS_ProcessParameterDefinition)");
    return (0);
  }
  switch (SDDS_dataset->layout.version) {
  case 1:
    return (SDDS1_ProcessParameterDefinition(SDDS_dataset, s));
  case 2:
    return (SDDS1_ProcessParameterDefinition(SDDS_dataset, s));
  case 3:
    return (SDDS1_ProcessParameterDefinition(SDDS_dataset, s));
  case 4:
    return (SDDS1_ProcessParameterDefinition(SDDS_dataset, s));
  case 5:
    return (SDDS1_ProcessParameterDefinition(SDDS_dataset, s));
  default:
    SDDS_SetError("Unable to process parameter definition--protocol version number is invalid (SDDS_ProcessParameterDefinition)");
    return (0);
  }
}

/**
 * @brief Process the include command within the SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param s Pointer to the include command string.
 * @return Returns a FILE pointer on success, NULL on failure.
 */
FILE *SDDS_ProcessIncludeCommand(SDDS_DATASET *SDDS_dataset, char *s) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessIncludeCommand"))
    return (NULL);
  if (!s) {
    SDDS_SetError("Unable to process parameter definition--namelist text pointer NULL (SDDS_ProcessIncludeCommand)");
    return (NULL);
  }
  switch (SDDS_dataset->layout.version) {
  case 1:
    return (SDDS1_ProcessIncludeCommand(SDDS_dataset, s));
  case 2:
    return (SDDS1_ProcessIncludeCommand(SDDS_dataset, s));
  case 3:
    return (SDDS1_ProcessIncludeCommand(SDDS_dataset, s));
  case 4:
    return (SDDS1_ProcessIncludeCommand(SDDS_dataset, s));
  case 5:
    return (SDDS1_ProcessIncludeCommand(SDDS_dataset, s));
  default:
    SDDS_SetError("Unable to process parameter definition--protocol version number is invalid (SDDS_ProcessIncludeCommand)");
    return (0);
  }
}

/**
 * @brief Process the associate definition section of the SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param s Pointer to the associate definition string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessAssociateDefinition(SDDS_DATASET *SDDS_dataset, char *s) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessAssociateDefinition"))
    return (0);
  if (!s) {
    SDDS_SetError("Unable to process associate definition--namelist text pointer NULL (SDDS_ProcessAssociateDefinition)");
    return (0);
  }
  switch (SDDS_dataset->layout.version) {
  case 1:
    return (SDDS1_ProcessAssociateDefinition(SDDS_dataset, s));
  case 2:
    return (SDDS1_ProcessAssociateDefinition(SDDS_dataset, s));
  case 3:
    return (SDDS1_ProcessAssociateDefinition(SDDS_dataset, s));
  case 4:
    return (SDDS1_ProcessAssociateDefinition(SDDS_dataset, s));
  case 5:
    return (SDDS1_ProcessAssociateDefinition(SDDS_dataset, s));
  default:
    SDDS_SetError("Unable to process associate definition--protocol version number is invalid (SDDS_ProcessAssociateDefinition)");
    return (0);
  }
}

/**
 * @brief Process the data mode section of the SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param s Pointer to the data mode string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessDataMode(SDDS_DATASET *SDDS_dataset, char *s) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessDataMode"))
    return (0);
  if (!s) {
    SDDS_SetError("Unable to process data mode--namelist text pointer is NULL (SDDS_ProcessDataMode)");
    return (0);
  }
  switch (SDDS_dataset->layout.version) {
  case 1:
    return (SDDS1_ProcessDataMode(SDDS_dataset, s));
  case 2:
    return (SDDS1_ProcessDataMode(SDDS_dataset, s));
  case 3:
    return (SDDS1_ProcessDataMode(SDDS_dataset, s));
  case 4:
    return (SDDS1_ProcessDataMode(SDDS_dataset, s));
  case 5:
    return (SDDS1_ProcessDataMode(SDDS_dataset, s));
  default:
    SDDS_SetError("Unable to process data mode--protocol version number is invalid (SDDS_ProcessDataMode)");
    return (0);
  }
}

/**
 * @brief Process the array definition section of the SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param s Pointer to the array definition string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessArrayDefinition(SDDS_DATASET *SDDS_dataset, char *s) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessArrayDefinition"))
    return (0);
  if (!s) {
    SDDS_SetError("Unable to process array definition--namelist text pointer NULL (SDDS_ProcessArrayDefinition)");
    return (0);
  }
  switch (SDDS_dataset->layout.version) {
  case 1:
    return (SDDS1_ProcessArrayDefinition(SDDS_dataset, s));
  case 2:
    return (SDDS1_ProcessArrayDefinition(SDDS_dataset, s));
  case 3:
    return (SDDS1_ProcessArrayDefinition(SDDS_dataset, s));
  case 4:
    return (SDDS1_ProcessArrayDefinition(SDDS_dataset, s));
  case 5:
    return (SDDS1_ProcessArrayDefinition(SDDS_dataset, s));
  default:
    SDDS_SetError("Unable to process array definition--protocol version number is invalid (SDDS_ProcessArrayDefinition)");
    return (0);
  }
}

/***********************************************************
 * The SDDS_ProcessXString routines provide an easy-to-use
 * alternative to the SDDS_DefineX routines for user programs.
 */

/**
 * @brief Process a column definition string.
 *
 * This function parses a string containing column definition information and
 * updates the SDDS dataset accordingly.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param string Pointer to the column definition string.
 * @param mode Mode indicating how the column is defined.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessColumnString(SDDS_DATASET *SDDS_dataset, char *string, int32_t mode) {
  char *ptr1, *ptr2;
  char *s;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessColumnString"))
    return (0);
  if (!string)
    return (0);
  if (!SDDS_CopyString(&s, string))
    return 0;
  if (!(ptr1 = strchr(s, '&')) || !(ptr2 = strchr(ptr1, ' '))) {
    free(s);
    return 0;
  }
  *ptr2 = 0;
  if (strcmp(ptr1, "&column") != 0) {
    free(s);
    return (0);
  }
  if (SDDS_ProcessColumnDefinition(SDDS_dataset, ptr2 + 1)) {
    free(s);
    SDDS_dataset->layout.column_definition[SDDS_dataset->layout.n_columns - 1].definition_mode = mode;
    if (!SDDS_SaveLayout(SDDS_dataset))
      return (0);
    return (1);
  }
  free(s);
  return (0);
}

/**
 * @brief Process a parameter definition string.
 *
 * This function parses a string containing parameter definition information and
 * updates the SDDS dataset accordingly.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param string Pointer to the parameter definition string.
 * @param mode Mode indicating how the parameter is defined.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessParameterString(SDDS_DATASET *SDDS_dataset, char *string, int32_t mode) {
  char *ptr1, *ptr2;
  char *s;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessParameterString"))
    return (0);
  if (!string)
    return (0);
  if (!SDDS_CopyString(&s, string))
    return 0;
  if (!(ptr1 = strchr(s, '&')) || !(ptr2 = strchr(ptr1, ' '))) {
    free(s);
    return 0;
  }
  *ptr2 = 0;
  if (strcmp(ptr1, "&parameter") != 0) {
    free(s);
    return (0);
  }
  if (SDDS_ProcessParameterDefinition(SDDS_dataset, ptr2 + 1)) {
    free(s);
    SDDS_dataset->layout.parameter_definition[SDDS_dataset->layout.n_parameters - 1].definition_mode = mode;
    if (!SDDS_SaveLayout(SDDS_dataset))
      return (0);
    return (1);
  }
  free(s);
  return (0);
}

/**
 * @brief Process an array definition string.
 *
 * This function parses a string containing array definition information and
 * updates the SDDS dataset accordingly.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param string Pointer to the array definition string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessArrayString(SDDS_DATASET *SDDS_dataset, char *string) {
  char *ptr1, *ptr2;
  char *s;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessArrayString"))
    return (0);
  if (!string)
    return (0);
  if (!SDDS_CopyString(&s, string))
    return 0;
  if (!(ptr1 = strchr(s, '&')) || !(ptr2 = strchr(ptr1, ' '))) {
    free(s);
    return 0;
  }
  *ptr2 = 0;
  if (strcmp(ptr1, "&array") != 0) {
    free(s);
    return (0);
  }
  if (SDDS_ProcessArrayDefinition(SDDS_dataset, ptr2 + 1)) {
    free(s);
    if (!SDDS_SaveLayout(SDDS_dataset))
      return (0);
    return (1);
  }
  free(s);
  return (0);
}

/**
 * @brief Process an associate definition string.
 *
 * This function parses a string containing associate definition information and
 * updates the SDDS dataset accordingly.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param string Pointer to the associate definition string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ProcessAssociateString(SDDS_DATASET *SDDS_dataset, char *string) {
  char *ptr1, *ptr2;
  char *s;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ProcessAssociateString"))
    return (0);
  if (!string)
    return (0);
  if (!SDDS_CopyString(&s, string))
    return 0;
  if (!(ptr1 = strchr(s, '&')) || !(ptr2 = strchr(ptr1, ' '))) {
    free(s);
    return 0;
  }
  *ptr2 = 0;
  if (strcmp(ptr1, "&associate") != 0) {
    free(s);
    return (0);
  }
  *ptr2 = ' ';
  if (SDDS_ProcessAssociateDefinition(SDDS_dataset, ptr2 + 1)) {
    free(s);
    if (!SDDS_SaveLayout(SDDS_dataset))
      return (0);
    return (1);
  }
  free(s);
  return (0);
}

/**
 * @brief Retrieve a tag-value pair from the input string.
 *
 * This function parses a substring to extract a tag and its corresponding value.
 * It handles quoted strings and escaped characters.
 *
 * @param ptr Pointer to the current position in the string.
 * @param tag Double pointer to store the extracted tag.
 * @param value Double pointer to store the extracted value.
 * @return Returns a pointer to the next position in the string after the pair,
 *         or NULL if an error occurs.
 */
char *SDDS_GetTagValuePair(char *ptr, char **tag, char **value) {
  /*  char find; */
  char buffer[1024];

  if (!*ptr)
    return NULL;
  *tag = *value = NULL;
  while (*ptr == ' ' || *ptr == ',' || *ptr == '\t')
    ptr++;
  if (!*ptr)
    return ptr;

  *tag = ptr; /* start of field name */
  while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '=')
    ptr++;
  if (*ptr != '=') {
    *ptr++ = 0; /* ends the field name */
    /* skip something of the form <0 or more spaces>=<0 or more spaces> */
    while (*ptr == ' ' || *ptr == '\t')
      ptr++;
    if (*ptr++ != '=' || !*ptr) {
      sprintf(buffer, "Namelist error: Field name %s ends string", *tag);
      SDDS_SetError(buffer);
      return NULL;
    }
  } else {
    *ptr++ = 0;
  }
  while (*ptr == ' ' || *ptr == '\t')
    ptr++;
  if (!*ptr) {
    sprintf(buffer, "Namelist error: Field name %s ends string", *tag);
    SDDS_SetError(buffer);
    return NULL;
  }
  if (!strlen(*tag)) {
    sprintf(buffer, "Namelist error: Field name is blank");
    SDDS_SetError(buffer);
    return NULL;
  }

  if (*ptr == '"' && *(ptr - 1) != '\\') {
    ptr++;
    if (*ptr == '"' && *(ptr - 1) != '\\') {
      *ptr = 0;
      *value = ptr;
      return ptr + 1;
    }
    *value = ptr++;
    while (*ptr && !(*ptr == '"' && *(ptr - 1) != '\\'))
      ptr++;
    if (*ptr == '"' && *(ptr - 1) != '\\')
      *ptr = 0; /* end of value */
  } else {
    *value = ptr; /* start of value */
    while (*ptr != ' ' && *ptr != ',' && *ptr != '\t' && *ptr)
      ptr++;
    if (!*ptr)
      return ptr;
    *ptr = 0; /* end of value */
  }
  return ptr + 1;
}

/**
 * @brief Prepare the string for parsing tag-value pairs.
 *
 * This function removes the trailing '&end' if present and ensures there is no group name.
 *
 * @param s Pointer to the input string.
 * @return Returns a pointer to the modified string ready for parsing.
 */
char *SDDS_PrepareToParseTagValuePairs(char *s) {
  char *ptr;
  int32_t length;

  /* remove the trailing &end if there is one */
  if ((length = strlen(s)) >= 4) {
    ptr = s + length - 4;
    while (1) {
      if (*ptr == '&' && (ptr == s || *(ptr - 1) != '\\') && strncmp(ptr, "&end", 4) == 0 && !SDDS_IsQuoted(s, ptr, '"')) {
        *ptr = 0;
        break;
      }
      if (ptr == s)
        break;
      ptr--;
    }
  }

  /* make sure there is no group name */
  while (*s == ' ')
    s++;
  if (*s == '&') {
    while (*s && *s != ' ')
      s++;
  }
  return s;
}

/**
 * @brief Find the index of a field based on its tag.
 *
 * @param tag Pointer to the tag string.
 * @param fieldInfo Array of SDDS_FIELD_INFORMATION structures.
 * @param fieldInfos Number of elements in the fieldInfo array.
 * @return Returns the index of the matching field, or -1 if not found.
 */
int32_t SDDS_FindFieldIndex(char *tag, SDDS_FIELD_INFORMATION *fieldInfo, int32_t fieldInfos) {
  int32_t i;
  for (i = 0; i < fieldInfos; i++) {
    if (strcmp(tag, fieldInfo[i].name) == 0)
      return i;
  }
  return -1;
}

/**
 * @brief Match an enumeration string to its corresponding value.
 *
 * @param value Pointer to the enumeration string.
 * @param enumPair Array of SDDS_ENUM_PAIR structures.
 * @return Returns the corresponding integer value if matched, -1 otherwise.
 */
int32_t SDDS_MatchEnum(char *value, SDDS_ENUM_PAIR *enumPair) {
  while (enumPair->name) {
    if (strcmp(value, enumPair->name) == 0)
      return enumPair->value;
    enumPair++;
  }
  return -1;
}

/**
 * @brief Parse a namelist string and populate the corresponding data structure.
 *
 * This function parses a namelist string containing tag-value pairs and assigns
 * the values to the appropriate fields in the provided data structure based on
 * the field information provided.
 *
 * @param data Pointer to the data structure to populate.
 * @param fieldInfo Array of SDDS_FIELD_INFORMATION structures describing the fields.
 * @param fieldInfos Number of elements in the fieldInfo array.
 * @param s Pointer to the namelist string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ParseNamelist(void *data, SDDS_FIELD_INFORMATION *fieldInfo, int32_t fieldInfos, char *s) {
  char buffer[1024], *ptr, *tag, *value;
  int32_t index, longValue;

  ptr = SDDS_PrepareToParseTagValuePairs(s);
  while (*ptr && !SDDS_StringIsBlank(ptr) && (ptr = SDDS_GetTagValuePair(ptr, &tag, &value))) {
    if (!tag)
      break; /* normal termination */
    if ((index = SDDS_FindFieldIndex(tag, fieldInfo, fieldInfos)) < 0) {
      sprintf(buffer, "Unrecognized namelist tag given: %s", tag);
      SDDS_SetError(buffer);
      return 0;
    } else {
      if (fieldInfo[index].enumPair) {
        if (fieldInfo[index].type != SDDS_LONG) {
          fprintf(stderr, "SDDS_ParseNamelist: namelist setup problem---get expert help!\n");
          exit(1);
        }
        /* map a string value into an integer */
        if ((longValue = SDDS_MatchEnum(value, fieldInfo[index].enumPair)) < 0) {
          sprintf(buffer, "Unrecognized value given for %s: %s", tag, value);
          SDDS_SetError(buffer);
          return 0;
        } else
          *((int32_t *)((char *)data + fieldInfo[index].offset)) = longValue;
      } else {
        switch (fieldInfo[index].type) {
        case SDDS_STRING:
          if (!(SDDS_CopyString((char **)((char *)data + fieldInfo[index].offset), value))) {
            SDDS_SetError("Problem setting string value for namelist");
            return 0;
          }
          break;
        case SDDS_CHARACTER:
          *((char *)data + fieldInfo[index].offset) = *value;
          break;
        case SDDS_SHORT:
          if (!sscanf(value, "%hd", (short *)((char *)data + fieldInfo[index].offset))) {
            SDDS_SetError("Problem scanning short value for namelist");
            return 0;
          }
          break;
        case SDDS_USHORT:
          if (!sscanf(value, "%hu", (unsigned short *)((char *)data + fieldInfo[index].offset))) {
            SDDS_SetError("Problem scanning unsigned short value for namelist");
            return 0;
          }
          break;
        case SDDS_LONG:
          if (!sscanf(value, "%" SCNd32, (int32_t *)((char *)data + fieldInfo[index].offset))) {
            SDDS_SetError("Problem scanning int32_t value for namelist");
            return 0;
          }
          break;
        case SDDS_ULONG:
          if (!sscanf(value, "%" SCNu32, (uint32_t *)((char *)data + fieldInfo[index].offset))) {
            SDDS_SetError("Problem scanning uint32_t value for namelist");
            return 0;
          }
          break;
        case SDDS_LONG64:
          if (!sscanf(value, "%" SCNd64, (int64_t *)((char *)data + fieldInfo[index].offset))) {
            SDDS_SetError("Problem scanning int64_t value for namelist");
            return 0;
          }
          break;
        case SDDS_ULONG64:
          if (!sscanf(value, "%" SCNu64, (uint64_t *)((char *)data + fieldInfo[index].offset))) {
            SDDS_SetError("Problem scanning uint64_t value for namelist");
            return 0;
          }
          break;
        case SDDS_LONGDOUBLE:
          if (!sscanf(value, "%Lf", (long double *)((char *)data + fieldInfo[index].offset))) {
            SDDS_SetError("Problem scanning long double value for namelist");
            return 0;
          }
          break;
        case SDDS_DOUBLE:
          if (!sscanf(value, "%lf", (double *)((char *)data + fieldInfo[index].offset))) {
            SDDS_SetError("Problem scanning double value for namelist");
            return 0;
          }
          break;
        case SDDS_FLOAT:
          if (!sscanf(value, "%f", (float *)((char *)data + fieldInfo[index].offset))) {
            SDDS_SetError("Problem scanning float value for namelist");
            return 0;
          }
          break;
        default:
          SDDS_SetError("Internal error: unknown field information type");
          return 0;
        }
      }
    }
  }
  if (!ptr) {
    SDDS_SetError("Problem parsing description namelist");
    return 0;
  }
  return 1;
}

/***********************************************/
/* SDDS protocol version 1 routines begin here */
/***********************************************/

/**
 * @brief Process the description section for SDDS protocol version 1.
 *
 * @param SDDS_dataset Pointer to the SDDS_DATASET structure.
 * @param s Pointer to the description string.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS1_ProcessDescription(SDDS_DATASET *SDDS_dataset, char *s) {
  SDDS_dataset->layout.description = NULL;
  SDDS_dataset->layout.contents = NULL;

  if (!SDDS_ParseNamelist((void *)&SDDS_dataset->layout, SDDS_DescriptionFieldInformation, SDDS_DESCRIPTION_FIELDS, s)) {
    SDDS_SetError("Problem parsing description namelist");
    return 0;
  }
#if DEBUG
  fprintf(stderr, "Description scanned: description=>%s<, contents = >%s<\n", SDDS_dataset->layout.description, SDDS_dataset->layout.contents);
#endif
  return 1;
}

/**
 * @brief Processes and defines a column within an SDDS dataset.
 *
 * This function parses a column definition string, extracts the necessary
 * column information, and defines the column in the given SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the column will be defined.
 * @param s            String containing the column definition in namelist format.
 *
 * @return 
 *   - Returns `1` on successful processing and definition of the column.
 *   - Returns `0` if an error occurs during parsing or definition.
 */
int32_t SDDS1_ProcessColumnDefinition(SDDS_DATASET *SDDS_dataset, char *s) {
  COLUMN_DEFINITION colDef;
  int32_t code;

  colDef.name = colDef.symbol = colDef.units = colDef.description = colDef.format_string = NULL;
  colDef.type = -1;
  colDef.field_length = 0;

  if (!SDDS_ParseNamelist((void *)&colDef, SDDS_ColumnFieldInformation, SDDS_COLUMN_FIELDS, s)) {
    SDDS_SetError("Problem parsing column namelist");
    return 0;
  }
  code = SDDS_DefineColumn(SDDS_dataset, colDef.name, colDef.symbol, colDef.units, colDef.description, colDef.format_string, colDef.type, colDef.field_length);
  if (colDef.name)
    free(colDef.name);
  if (colDef.symbol)
    free(colDef.symbol);
  if (colDef.units)
    free(colDef.units);
  if (colDef.description)
    free(colDef.description);
  if (colDef.format_string)
    free(colDef.format_string);

  if (code < 0) {
    SDDS_SetError("Unable to process column definition--call to define column failed (SDDS1_ProcessColumnDefinition)");
    return (0);
  }
  return (1);
}

/**
 * @brief Processes and defines a parameter within an SDDS dataset.
 *
 * This function parses a parameter definition string, extracts the necessary
 * parameter information, and defines the parameter in the given SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the parameter will be defined.
 * @param s            String containing the parameter definition in namelist format.
 *
 * @return 
 *   - Returns `1` on successful processing and definition of the parameter.
 *   - Returns `0` if an error occurs during parsing or definition.
 */
int32_t SDDS1_ProcessParameterDefinition(SDDS_DATASET *SDDS_dataset, char *s) {
  PARAMETER_DEFINITION paramDef;
  int32_t code;

  paramDef.name = paramDef.symbol = paramDef.units = paramDef.description = paramDef.format_string = paramDef.fixed_value = NULL;
  paramDef.type = -1;

  if (!SDDS_ParseNamelist((void *)&paramDef, SDDS_ParameterFieldInformation, SDDS_PARAMETER_FIELDS, s)) {
    SDDS_SetError("Problem parsing parameter namelist");
    return 0;
  }
  code = SDDS_DefineParameter(SDDS_dataset, paramDef.name, paramDef.symbol, paramDef.units, paramDef.description, paramDef.format_string, paramDef.type, paramDef.fixed_value);
  if (paramDef.name)
    free(paramDef.name);
  if (paramDef.symbol)
    free(paramDef.symbol);
  if (paramDef.units)
    free(paramDef.units);
  if (paramDef.description)
    free(paramDef.description);
  if (paramDef.format_string)
    free(paramDef.format_string);

  if (code < 0) {
    SDDS_SetError("Unable to process parameter definition--call to define parameter failed (SDDS1_ProcessParameterDefinition)");
    return (0);
  }
  return (1);
}

/**
 * @brief Processes an include command by opening the specified file.
 *
 * This function parses an include command string to extract the filename,
 * then attempts to open the file for reading. It returns a file pointer
 * if successful.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset (unused in current implementation).
 * @param s            String containing the include command in namelist format.
 *
 * @return 
 *   - Returns a pointer to the opened `FILE` on success.
 *   - Returns `NULL` if parsing fails or the file cannot be opened.
 */
FILE *SDDS1_ProcessIncludeCommand(SDDS_DATASET *SDDS_dataset, char *s) {
  FILE *fp;
  char *filename;

  filename = NULL;
  if (!SDDS_ParseNamelist((void *)&filename, SDDS_IncludeFieldInformation, SDDS_INCLUDE_FIELDS, s)) {
    SDDS_SetError("Problem parsing include namelist");
    return 0;
  }
  if (!filename || !(fp = fopen(filename, "r"))) {
    SDDS_SetError("Unable to process include command--invalid/nonexistent file (SDDS1_ProcessIncludeCommand)");
    return (NULL);
  }
  return (fp);
}

/**
 * @brief Processes and defines an associate within an SDDS dataset.
 *
 * This function parses an associate definition string, extracts the necessary
 * associate information, and defines the associate in the given SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the associate will be defined.
 * @param s            String containing the associate definition in namelist format.
 *
 * @return 
 *   - Returns `1` on successful processing and definition of the associate.
 *   - Returns `0` if an error occurs during parsing or definition.
 */
int32_t SDDS1_ProcessAssociateDefinition(SDDS_DATASET *SDDS_dataset, char *s) {
  ASSOCIATE_DEFINITION assocDef;
  int32_t code;

  assocDef.name = assocDef.filename = assocDef.path = assocDef.description = assocDef.contents = NULL;
  assocDef.sdds = 0;
  if (!SDDS_ParseNamelist((void *)&assocDef, SDDS_AssociateFieldInformation, SDDS_ASSOCIATE_FIELDS, s)) {
    SDDS_SetError("Problem parsing associate namelist");
    return 0;
  }

  code = SDDS_DefineAssociate(SDDS_dataset, assocDef.name, assocDef.filename, assocDef.path, assocDef.description, assocDef.contents, assocDef.sdds);
  if (code < 0) {
    SDDS_SetError("Unable to process associate definition--call to define associate failed (SDDS1_ProcessAssociateDefinition)");
    return (0);
  }
  return (1);
}

/**
 * @brief Processes and sets the data mode for an SDDS dataset.
 *
 * This function parses a data mode definition string, extracts the necessary
 * data mode information, and sets the data mode in the given SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the data mode will be set.
 * @param s            String containing the data mode definition in namelist format.
 *
 * @return 
 *   - Returns `1` on successful processing and setting of the data mode.
 *   - Returns `0` if an error occurs during parsing or if mandatory fields are missing.
 */
int32_t SDDS1_ProcessDataMode(SDDS_DATASET *SDDS_dataset, char *s) {
  DATA_MODE *dataMode;
  dataMode = &(SDDS_dataset->layout.data_mode);
  dataMode->mode = 0;
  dataMode->endian = 0;
  dataMode->lines_per_row = 1;
  dataMode->no_row_counts = dataMode->additional_header_lines = 0;
  dataMode->fixed_row_count = 0;
  dataMode->column_major = 0;
  if (!SDDS_ParseNamelist((void *)dataMode, SDDS_DataFieldInformation, SDDS_DATA_FIELDS, s)) {
    SDDS_SetError("Problem parsing data namelist");
    return 0;
  }
  if (dataMode->mode == 0) {
    SDDS_SetError("Problem with data namelist: mode not given.");
    return 0;
  }
  if (dataMode->mode == SDDS_ASCII && dataMode->lines_per_row < 0) {
    SDDS_SetError("Unable to process data mode--lines_per_row is invalid (SDDS1_ProcessDataMode)");
    return (0);
  }
  return 1;
}

/**
 * @brief Processes and defines an array within an SDDS dataset.
 *
 * This function parses an array definition string, extracts the necessary
 * array information, and defines the array in the given SDDS dataset.
 *
 * @param SDDS_dataset Pointer to the SDDS dataset where the array will be defined.
 * @param s            String containing the array definition in namelist format.
 *
 * @return 
 *   - Returns `1` on successful processing and definition of the array.
 *   - Returns `0` if an error occurs during parsing or definition.
 */
int32_t SDDS1_ProcessArrayDefinition(SDDS_DATASET *SDDS_dataset, char *s) {
  ARRAY_DEFINITION arrayDef;
  int32_t code;

  arrayDef.name = arrayDef.symbol = arrayDef.units = arrayDef.description = arrayDef.format_string = arrayDef.group_name = NULL;
  arrayDef.type = -1;
  arrayDef.field_length = 0;
  arrayDef.dimensions = 1;

  if (!SDDS_ParseNamelist((void *)&arrayDef, SDDS_ArrayFieldInformation, SDDS_ARRAY_FIELDS, s)) {
    SDDS_SetError("Problem parsing array namelist");
    return 0;
  }
  code = SDDS_DefineArray(SDDS_dataset, arrayDef.name, arrayDef.symbol, arrayDef.units, arrayDef.description, arrayDef.format_string, arrayDef.type, arrayDef.field_length, arrayDef.dimensions, arrayDef.group_name);
  if (arrayDef.name)
    free(arrayDef.name);
  if (arrayDef.symbol)
    free(arrayDef.symbol);
  if (arrayDef.units)
    free(arrayDef.units);
  if (arrayDef.description)
    free(arrayDef.description);
  if (arrayDef.format_string)
    free(arrayDef.format_string);
  if (arrayDef.group_name)
    free(arrayDef.group_name);

  if (code < 0) {
    SDDS_SetError("Unable to process array definition--call to define array failed (SDDS1_ProcessArrayDefinition)");
    return (0);
  }
  return (1);
}
