/* GUIDELINES
   Do not exit or print to stdout or stderr from any of the functions in this file
*/

/* TODO
fix nonnative in everything
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <float.h>
#include "SDDS3.h"


/*
 * SDDSArray
 *
 * C++ Arguments: none
 *
 * Results:  pointer to SDDSArray object
 */
SDDSArray::SDDSArray() {
  pageCount = 0;
  elementCount = NULL;
  elementsAllocated = NULL;
  elementAllocationIncrement = 10;
  arrayType = 0;
  arrayName = NULL;
  arraySymbol = NULL;
  arrayUnits = NULL;
  arrayDescription = NULL;
  arrayFormatString = NULL;
  arrayGroupName = NULL;
  arrayFieldLength = 0;
  arrayDimensions = 0;
  arrayDim = NULL;

  int32Value = NULL;
  int16Value = NULL;
  uint32Value = NULL;
  uint16Value = NULL;
  real64Value = NULL;
  real32Value = NULL;
  stringValue = NULL;
  charValue = NULL;
  sddsfile = NULL;
}

/*
 * ~SDDSArray
 *
 * C++ Arguments: none
 *
 * Results:  none
 */
SDDSArray::~SDDSArray() {
  uint32_t i, j;
  if (arrayName != NULL)
    SDDS_free(arrayName);
  if (arraySymbol != NULL)
    SDDS_free(arraySymbol);
  if (arrayUnits != NULL)
    SDDS_free(arrayUnits);
  if (arrayDescription != NULL)
    SDDS_free(arrayDescription);
  if (arrayFormatString != NULL)
    SDDS_free(arrayFormatString);
  if (arrayGroupName != NULL)
    SDDS_free(arrayGroupName);
  if (arrayDim != NULL) {
    for (i=0; i<pageCount; i++) {
      if (arrayDim[i] != NULL) {
	SDDS_free(arrayDim[i]);
      }
    }
    SDDS_free(arrayDim);
  }

  if (int16Value != NULL) {
    for (i=0; i<pageCount; i++) {
      if (int16Value[i] != NULL) {
	SDDS_free(int16Value[i]);
      }
    }
    SDDS_free(int16Value);
  }
  if (uint16Value != NULL) {
    for (i=0; i<pageCount; i++) {
      if (uint16Value[i] != NULL) {
	SDDS_free(uint16Value[i]);
      }
    }
    SDDS_free(uint16Value);
  }
  if (int32Value != NULL) {
    for (i=0; i<pageCount; i++) {
      if (int32Value[i] != NULL) {
	SDDS_free(int32Value[i]);
      }
    }
    SDDS_free(int32Value);
  }
  if (uint32Value != NULL) {
    for (i=0; i<pageCount; i++) {
      if (uint32Value[i] != NULL) {
	SDDS_free(uint32Value[i]);
      }
    }
    SDDS_free(uint32Value);
  }
  if (real32Value != NULL) {
    for (i=0; i<pageCount; i++) {
      if (real32Value[i] != NULL) {
	SDDS_free(real32Value[i]);
      }
    }
    SDDS_free(real32Value);
  }
  if (real64Value != NULL) {
    for (i=0; i<pageCount; i++) {
      if (real64Value[i] != NULL) {
	SDDS_free(real64Value[i]);
      }
    }
    SDDS_free(real64Value);
  }
  if (stringValue != NULL) {
    for (i=0; i<pageCount; i++) {
      if (stringValue[i] != NULL) {
	for (j=0; j<elementCount[i]; j++) {
	  if (stringValue[i][j] != NULL) {
	    SDDS_free(stringValue[i][j]);
	  }
	}
	SDDS_free(stringValue[i]);
      }
    }
    SDDS_free(stringValue);
  }
  if (charValue != NULL) {
    for (i=0; i<pageCount; i++) {
      if (charValue[i] != NULL) {
	SDDS_free(charValue[i]);
      }
    }
    SDDS_free(charValue);
  }
  if (elementCount != NULL) {
    SDDS_free(elementCount);
  }
  if (elementsAllocated != NULL) {
    SDDS_free(elementsAllocated);
  }  
}

/*
 * freePage
 *
 * C++ Arguments: none
 *
 * Results:  none
 */
void SDDSArray::freePage() {
  uint32_t i, j;
  switch (arrayType) {
  case SDDS_INT16:
    if (int16Value != NULL) {
      for (i=0; i<pageCount; i++) {
	if (int16Value[i] != NULL) {
	  SDDS_free(int16Value[i]);
	}
      }
      SDDS_free(int16Value);
      int16Value = NULL;
    }
    break;
  case SDDS_UINT16:
    if (uint16Value != NULL) {
      for (i=0; i<pageCount; i++) {
	if (uint16Value[i] != NULL) {
	  SDDS_free(uint16Value[i]);
	}
      }
      SDDS_free(uint16Value);
      uint16Value = NULL;
    }
    break;
  case SDDS_INT32:
    if (int32Value != NULL) {
      for (i=0; i<pageCount; i++) {
	if (int32Value[i] != NULL) {
	  SDDS_free(int32Value[i]);
	}
      }
      SDDS_free(int32Value);
      int32Value = NULL;
    }
    break;
  case SDDS_UINT32:
    if (uint32Value != NULL) {
      for (i=0; i<pageCount; i++) {
	if (uint32Value[i] != NULL) {
	  SDDS_free(uint32Value[i]);
	}
      }
      SDDS_free(uint32Value);
      uint32Value = NULL;
    }
    break;
  case SDDS_REAL32:
    if (real32Value != NULL) {
      for (i=0; i<pageCount; i++) {
	if (real32Value[i] != NULL) {
	  SDDS_free(real32Value[i]);
	}
      }
      SDDS_free(real32Value);
      real32Value = NULL;
    }
    break;
  case SDDS_REAL64:
    if (real64Value != NULL) {
      for (i=0; i<pageCount; i++) {
	if (real64Value[i] != NULL) {
	  SDDS_free(real64Value[i]);
	}
      }
      SDDS_free(real64Value);
      real64Value = NULL;
    }
    break;
  case SDDS_STRING:
    if (stringValue != NULL) {
      for (i=0; i<pageCount; i++) {
	if (stringValue[i] != NULL) {
	  for (j=0; j<elementCount[i]; j++) {
	    if (stringValue[i][j] != NULL) {
	      SDDS_free(stringValue[i][j]);
	    }
	  }
	  SDDS_free(stringValue[i]);
	}
      }
      SDDS_free(stringValue);
      stringValue = NULL;
    }
    break;
  case SDDS_CHARACTER:
    if (charValue != NULL) {
      for (i=0; i<pageCount; i++) {
	if (charValue[i] != NULL) {
	  SDDS_free(charValue[i]);
	}
      }
      SDDS_free(charValue);
      charValue = NULL;
    }
    break;
  default:
    break;
  }
  if (arrayDim != NULL) {
    for (i=0; i<pageCount; i++) {
      if (arrayDim[i] != NULL) {
	SDDS_free(arrayDim[i]);
      }
    }
    SDDS_free(arrayDim);
  }
  arrayDim = NULL;
  if (elementCount != NULL) {
    SDDS_free(elementCount);
    elementCount = NULL;
  }
  if (elementsAllocated != NULL) {
    SDDS_free(elementsAllocated);
    elementsAllocated = NULL;  
  }
  pageCount = 0;
}

/*
 * setupArray
 *
 * C++ Arguments: char *name, char *symbol, char *units, char *description,
 *                char *format_string, char *group_name, int32_t type, 
 *                uint32_t field_length, uint32_t dimensions
 *
 * Results:  none
 */
void SDDSArray::setupArray(char *name, char *symbol, char *units, char *description, char *format_string, char *group_name, int32_t type, uint32_t field_length, uint32_t dimensions) {
  if (arrayName != NULL)
    SDDS_free(arrayName);
  if (arraySymbol != NULL)
    SDDS_free(arraySymbol);
  if (arrayUnits != NULL)
    SDDS_free(arrayUnits);
  if (arrayDescription != NULL)
    SDDS_free(arrayDescription);
  if (arrayFormatString != NULL)
    SDDS_free(arrayFormatString);
  if (arrayGroupName != NULL)
    SDDS_free(arrayGroupName);

  if (name == NULL)
    arrayName = NULL;
  else {
    arrayName = (char*)SDDS_malloc(sizeof(char) * strlen(name) + 1);
    strcpy(arrayName, name);
  }
  if (symbol == NULL)
    arraySymbol = NULL;
  else {
    arraySymbol = (char*)SDDS_malloc(sizeof(char) * strlen(symbol) + 1);
    strcpy(arraySymbol, symbol);
  }
  if (units == NULL)
    arrayUnits = NULL;
  else {
    arrayUnits = (char*)SDDS_malloc(sizeof(char) * strlen(units) + 1);
    strcpy(arrayUnits, units);
  }
  if (description == NULL)
    arrayDescription = NULL;
  else {
    arrayDescription = (char*)SDDS_malloc(sizeof(char) * strlen(description) + 1);
    strcpy(arrayDescription, description);
  }
  if (format_string == NULL)
    arrayFormatString = NULL;
  else {
    arrayFormatString = (char*)SDDS_malloc(sizeof(char) * strlen(format_string) + 1);
    strcpy(arrayFormatString, format_string);
  }
  if (group_name == NULL)
    arrayGroupName = NULL;
  else {
    arrayGroupName = (char*)SDDS_malloc(sizeof(char) * strlen(group_name) + 1);
    strcpy(arrayGroupName, group_name);
  }
  arrayType = type;
  arrayFieldLength = field_length;
  arrayDimensions = dimensions;
}

/*
 * informAboutParent
 *
 * C++ Arguments: SDDSFile *parent
 *
 * Results:  none
 */
void SDDSArray::informAboutParent(void *parent) {
  sddsfile = parent;
}

/*
 * getName
 *
 * C++ Arguments: none
 *
 * Results:  pointer to name  (do not free the result)
 */
char* SDDSArray::getName() {
  return(arrayName);
}

/*
 * setName
 *
 * C++ Arguments: char *name
 *
 * Results: none
 */
void SDDSArray::setName(char *name) {
  if (arrayName != NULL)
    SDDS_free(arrayName);
  if (name == NULL)
    arrayName = NULL;
  else {
    arrayName = (char*)SDDS_malloc(sizeof(char) * strlen(name) + 1);
    strcpy(arrayName, name);
  }
}

/*
 * getSymbol
 *
 * C++ Arguments: none
 *
 * Results:  pointer to symbol (do not free the result)
 */
char* SDDSArray::getSymbol() {
  return(arraySymbol);
}

/*
 * setSymbol
 *
 * C++ Arguments: char *symbol
 *
 * Results: none
 */
void SDDSArray::setSymbol(char *symbol) {
  if (arraySymbol != NULL)
    SDDS_free(arraySymbol);
  if (symbol == NULL)
    arraySymbol = NULL;
  else {
    arraySymbol = (char*)SDDS_malloc(sizeof(char) * strlen(symbol) + 1);
    strcpy(arraySymbol, symbol);
  }
}

/*
 * getUnits
 *
 * C++ Arguments: none
 * 
 * Results:  pointer to the units (do not free the result)
 */
char* SDDSArray::getUnits() {
  return(arrayUnits);
}

/*
 * setUnits
 *
 * C++ Arguments: char *units
 *
 * Results: none
 */
void SDDSArray::setUnits(char *units) {
  if (arrayUnits != NULL)
    SDDS_free(arrayUnits);
  if (units == NULL)
    arrayUnits = NULL;
  else {
    arrayUnits = (char*)SDDS_malloc(sizeof(char) * strlen(units) + 1);
    strcpy(arrayUnits, units);
  }
}

/*
 * getDescription
 *
 * C++ Arguments: none
 *
 * Results:  pointer to the description (do not free the result)
 */
char* SDDSArray::getDescription() {
  return(arrayDescription);
}

/*
 * setDescription
 *
 * C++ Arguments: char *description
 *
 * Results: none
 */
void SDDSArray::setDescription(char *description) {
  if (arrayDescription != NULL)
    SDDS_free(arrayDescription);
  if (description == NULL)
    arrayDescription = NULL;
  else {
    arrayDescription = (char*)SDDS_malloc(sizeof(char) * strlen(description) + 1);
    strcpy(arrayDescription, description);
  }
}

/*
 * getFormatString
 *
 * C++ Arguments: none
 *
 * Results:  pointer to the format string (do not free the result)
 */
char* SDDSArray::getFormatString() {
  return(arrayFormatString);
}

/*
 * setFormatString
 *
 * C++ Arguments: char *format_string
 *
 * Results: none
 */
void SDDSArray::setFormatString(char *format_string) {
  if (arrayFormatString != NULL)
    SDDS_free(arrayFormatString);
  if (format_string == NULL)
    arrayFormatString = NULL;
  else {
    arrayFormatString = (char*)SDDS_malloc(sizeof(char) * strlen(format_string) + 1);
    strcpy(arrayFormatString, format_string);
  }
}

/*
 * getGroupName
 *
 * C++ Arguments: none
 *
 * Results:  pointer to the group name   (do not free the result)
 */
char* SDDSArray::getGroupName() {
  return(arrayGroupName);
}

/*
 * setGroupName
 *
 * C++ Arguments: char *fixed_value
 *
 * Results: none
 */
void SDDSArray::setGroupName(char *group_name) {
  if (arrayGroupName != NULL)
    SDDS_free(arrayGroupName);
  if (group_name == NULL)
    arrayGroupName = NULL;
  else {
    arrayGroupName = (char*)SDDS_malloc(sizeof(char) * strlen(group_name) + 1);
    strcpy(arrayGroupName, group_name);
  }
}

/*
 * getType
 *
 * C++ Arguments: none
 *
 * Results: the array type in integer form
 */
int32_t SDDSArray::getType() {
  return(arrayType);
}

/*
 * setType
 *
 * C++ Arguments:int32_t type
 *
 * Results: none
 */
void SDDSArray::setType(int32_t type) {
  arrayType = type;
}

/*
 * getFieldLength
 *
 * C++ Arguments: none
 *
 * Results: the array field length
 */
uint32_t SDDSArray::getFieldLength() {
  return(arrayFieldLength);
}

/*
 * setFieldLength
 *
 * C++ Arguments:int32_t field_length
 *
 * Results: none
 */
void SDDSArray::setFieldLength(uint32_t field_length) {
  arrayFieldLength = field_length;
}

/*
 * getDimensions
 *
 * C++ Arguments: none
 *
 * Results: the array dimensions
 */
uint32_t SDDSArray::getDimensions() {
  return(arrayDimensions);
}

/*
 * setDimensions
 *
 * C++ Arguments: uint32_t dimensions
 *
 * Results: none
 */
void SDDSArray::setDimensions(uint32_t dimensions) {
  arrayDimensions = dimensions;
}

/*
 * getDim
 *
 * C++ Arguments: uint32_t page
 *
 * Results: the number of elements in each of the dimensions for this page, do not free the results
 */
uint32_t* SDDSArray::getDim(uint32_t page) {
  if (page > pageCount)
    return(NULL);
  if (arrayDim == NULL)
    return(NULL);
  if (arrayDim[page-1] == NULL)
    return(NULL);
  return arrayDim[page-1];
}

/*
 * setDim
 *
 * C++ Arguments: uint32_t page, uint32_t *dimensions
 *
 * Results: none
 */
void SDDSArray::setDim(uint32_t page, uint32_t *dimensions) {
  uint32_t i;
  if ((page > 0) && (page <= pageCount)) {
    arrayDim[page-1] = (uint32_t*)SDDS_malloc(sizeof(uint32_t) * arrayDimensions);
    for (i=0 ; i<arrayDimensions ; i++)
      arrayDim[page-1][i] = dimensions[i];
  } else if (page == pageCount+1) {
    arrayDim = (uint32_t**)SDDS_realloc(arrayDim, sizeof(uint32_t*) * page);
    arrayDim[page-1] = (uint32_t*)SDDS_malloc(sizeof(uint32_t) * arrayDimensions);
    for (i=0 ; i<arrayDimensions ; i++)
      arrayDim[page-1][i] = dimensions[i];
  }
}

/*
 * getPageCount
 *
 * C++ Arguments: none
 *
 * Results: the page count
 */
uint32_t SDDSArray::getPageCount() {
  return(pageCount);
}

/*
 * writeDefinition
 *
 * C++ Arguments: FILE *fp
 *                voidp *gzfp
 *
 * Results:  0 on success
 *           1 on failure
 */
int32_t SDDSArray::writeDefinition(FILE *fp) {
  if ((arrayType <= 0) || (arrayType > SDDS_NUM_TYPES)) {
    return(1);
  }
  if (fp == NULL) {
    return(1);
  }
  fputs("&array ", fp);
  if (SDDS_printNamelistField(fp, (char*)"name", arrayName)) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"symbol", SDDS_blankToNull(arraySymbol))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"units", SDDS_blankToNull(arrayUnits))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"description", SDDS_blankToNull(arrayDescription))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"format_string", SDDS_blankToNull(arrayFormatString))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"group_name", SDDS_blankToNull(arrayGroupName))) {
    return(1);
  }
  if (((SDDSFile*)sddsfile)->layoutVersion == 1) {
    if (SDDS_printNamelistField(fp, (char*)"type", SDDS_getVer1TypeName(arrayType))) {
      return(1);
    }
  } else if (((SDDSFile*)sddsfile)->layoutVersion == 2) {
    if (SDDS_printNamelistField(fp, (char*)"type", SDDS_getVer2TypeName(arrayType))) {
      return(1);
    }
  } else {
    if (SDDS_printNamelistField(fp, (char*)"type", SDDS_getTypeName(arrayType))) {
      return(1);
    }
  }
  fprintf(fp, "dimensions=%" PRIu32 ", ", arrayDimensions);
  fputs("&end\n", fp);
  return(0);
}

int32_t SDDSArray::writeDefinition(voidp *gzfp) {
  if ((arrayType <= 0) || (arrayType > SDDS_NUM_TYPES)) {
    return(1);
  }
  if (gzfp == NULL) {
    return(1);
  }
  gzputs(gzfp, "&array ");
  if (SDDS_printNamelistField(gzfp, (char*)"name", arrayName)) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"symbol", SDDS_blankToNull(arraySymbol))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"units", SDDS_blankToNull(arrayUnits))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"description", SDDS_blankToNull(arrayDescription))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"format_string", SDDS_blankToNull(arrayFormatString))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"group_name", SDDS_blankToNull(arrayGroupName))) {
    return(1);
  }
  if (((SDDSFile*)sddsfile)->layoutVersion == 1) {
    if (SDDS_printNamelistField(gzfp, (char*)"type", SDDS_getVer1TypeName(arrayType))) {
      return(1);
    }
  } else if (((SDDSFile*)sddsfile)->layoutVersion == 2) {
    if (SDDS_printNamelistField(gzfp, (char*)"type", SDDS_getVer2TypeName(arrayType))) {
      return(1);
    }
  } else {
    if (SDDS_printNamelistField(gzfp, (char*)"type", SDDS_getTypeName(arrayType))) {
      return(1);
    }
  }
  gzprintf(gzfp, "dimensions=%" PRIu32 ", ", arrayDimensions);
  gzputs(gzfp, "&end\n");
  return(0);
}

/*
 * setValues
 *
 * C++ Arguments: int16_t *values, uint32_t page, uint32_t elements
 *                uint16_t *values, uint32_t page, uint32_t elements
 *                int32_t *values, uint32_t page, uint32_t elements
 *                uint32_t *values, uint32_t page, uint32_t elements
 *                float *values, uint32_t page, uint32_t elements
 *                double *values, uint32_t page, uint32_t elements
 *                char **values, uint32_t page, uint32_t elements
 *                char *values, uint32_t page, uint32_t elements
 *
 * Results:  0 for success
 *           1 for invalid page number
 *           2 for the case where the value cannot be converted to the proper array type
 *           3 for invalid array type
 */
int32_t SDDSArray::setValues(int16_t *values, uint32_t page, uint32_t elements) {
  uint32_t i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
    elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      elementCount[p-i] = 0;
      elementsAllocated[p-i] = 0;
    }
  }
  switch (arrayType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p - i] = (int16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementsAllocated[p]);
    }
    memcpy(int16Value[p], values, elements * 2);
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p - i] = (uint16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint16Value[p][i] = (uint16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p - i] = (int32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int32Value[p][i] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p - i] = (uint32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint32Value[p][i] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p - i] = (float*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real32Value[p][i] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p - i] = (double*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real64Value[p][i] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      sprintf(s, "%" PRId16, values[i]);
      stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      charValue[p][i] = (char)(values[i]);
    }
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  if (elementCount[p] < elements) {
    elementCount[p] = elements;
  }
  return(0);
}

int32_t SDDSArray::setValues(uint16_t *values, uint32_t page, uint32_t elements) {
  uint32_t i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
    elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      elementCount[p-i] = 0;
      elementsAllocated[p-i] = 0;
    }
  }
  switch (arrayType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p - i] = (int16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p - i] = (uint16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementsAllocated[p]);
    }
    memcpy(uint16Value[p], values, elements * 2);
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p - i] = (int32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int32Value[p][i] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p - i] = (uint32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint32Value[p][i] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p - i] = (float*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real32Value[p][i] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p - i] = (double*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real64Value[p][i] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      sprintf(s, "%" PRId16, values[i]);
      stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      charValue[p][i] = (char)(values[i]);
    }
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  if (elementCount[p] < elements) {
    elementCount[p] = elements;
  }
  return(0);
}

int32_t SDDSArray::setValues(int32_t *values, uint32_t page, uint32_t elements) {
  uint32_t i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
    elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      elementCount[p-i] = 0;
      elementsAllocated[p-i] = 0;
    }
  }
  switch (arrayType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p - i] = (int16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p - i] = (uint16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p - i] = (int32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementsAllocated[p]);
    }
    memcpy(int32Value[p], values, elements * 4);
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p - i] = (uint32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint32Value[p][i] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p - i] = (float*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real32Value[p][i] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p - i] = (double*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real64Value[p][i] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      sprintf(s, "%" PRId16, values[i]);
      stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      charValue[p][i] = (char)(values[i]);
    }
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  if (elementCount[p] < elements) {
    elementCount[p] = elements;
  }
  return(0);
}

int32_t SDDSArray::setValues(uint32_t *values, uint32_t page, uint32_t elements) {
  uint32_t i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
    elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      elementCount[p-i] = 0;
      elementsAllocated[p-i] = 0;
    }
  }
  switch (arrayType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p - i] = (int16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p - i] = (uint16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p - i] = (int32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int32Value[p][i] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p - i] = (uint32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementsAllocated[p]);
    }
    memcpy(uint32Value[p], values, elements * 4);
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p - i] = (float*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real32Value[p][i] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p - i] = (double*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real64Value[p][i] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      sprintf(s, "%" PRId16, values[i]);
      stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      charValue[p][i] = (char)(values[i]);
    }
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  if (elementCount[p] < elements) {
    elementCount[p] = elements;
  }
  return(0);
}

int32_t SDDSArray::setValues(float *values, uint32_t page, uint32_t elements) {
  uint32_t i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
    elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      elementCount[p-i] = 0;
      elementsAllocated[p-i] = 0;
    }
  }
  switch (arrayType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p - i] = (int16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p - i] = (uint16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p - i] = (int32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int32Value[p][i] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p - i] = (uint32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint32Value[p][i] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p - i] = (float*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementsAllocated[p]);
    }
    memcpy(real32Value[p], values, elements * 4);
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p - i] = (double*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real64Value[p][i] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      sprintf(s, "%f", values[i]);
      stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      charValue[p][i] = (char)(values[i]);
    }
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  if (elementCount[p] < elements) {
    elementCount[p] = elements;
  }
  return(0);
}

int32_t SDDSArray::setValues(double *values, uint32_t page, uint32_t elements) {
  uint32_t i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
    elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      elementCount[p-i] = 0;
      elementsAllocated[p-i] = 0;
    }
  }
  switch (arrayType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p - i] = (int16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p - i] = (uint16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p - i] = (int32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int32Value[p][i] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p - i] = (uint32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint32Value[p][i] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p - i] = (float*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real32Value[p][i] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p - i] = (double*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementsAllocated[p]);
    }
    memcpy(real64Value[p], values, elements * 8);
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      sprintf(s, "%f", values[i]);
      stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      charValue[p][i] = (char)(values[i]);
    }
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  if (elementCount[p] < elements) {
    elementCount[p] = elements;
  }
  return(0);
}

int32_t SDDSArray::setValues(char **values, uint32_t page, uint32_t elements) {
  int32_t returnVal=0;
  uint32_t i, n=0, p;
  bool newPage=false;
  int64_t *iValue=NULL;
  double *dValue=NULL;
  if (page == 0)
    return(1);

  if (SDDS_INTEGER_TYPE(arrayType)) {
    iValue = (int64_t*)SDDS_malloc(sizeof(int64_t) * elements);
    for (i=0; i<elements; i++) {
      iValue[i] = strtoll(values[i], (char**)NULL, 10);
      if ((iValue[i] == LLONG_MIN) || (iValue[i] == LLONG_MAX)) {
	/*unable to convert given string to integer*/
	returnVal=2;
	iValue[i]=0;
      }
    }
  } else if (SDDS_FLOATING_TYPE(arrayType)) {
    dValue = (double*)SDDS_malloc(sizeof(double) * elements);
    for (i=0; i<elements; i++) {
      dValue[i] = strtod(values[i], (char**)NULL);
      if ((dValue[i] == HUGE_VAL) || (dValue[i] == -HUGE_VAL)) {
	/*unable to convert given string to floating point*/
	returnVal=2;
	dValue[i]=0;
      }
    }
  }

  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
    elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      elementCount[p-i] = 0;
      elementsAllocated[p-i] = 0;
    }
  }
  switch (arrayType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p - i] = (int16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int16Value[p][i] = (int16_t)(iValue[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p - i] = (uint16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint16Value[p][i] = (int16_t)(iValue[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p - i] = (int32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int32Value[p][i] = (int32_t)(iValue[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p - i] = (uint32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint32Value[p][i] = (uint32_t)(iValue[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p - i] = (float*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real32Value[p][i] = (float)(dValue[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p - i] = (double*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real64Value[p][i] = (double)(dValue[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * strlen(values[i]) + 1);
      strcpy(stringValue[p][i], values[i]);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      charValue[p][i] = (char)(values[i][0]);
    }
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  if (elementCount[p] < elements) {
    elementCount[p] = elements;
  }
  return(returnVal);
}

int32_t SDDSArray::setValues(char *values, uint32_t page, uint32_t elements) {
  uint32_t i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
    elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      elementCount[p-i] = 0;
      elementsAllocated[p-i] = 0;
    }
  }
  switch (arrayType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p - i] = (int16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p - i] = (uint16_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint16Value[p][i] = (int16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p - i] = (int32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      int32Value[p][i] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p - i] = (uint32_t*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      uint32Value[p][i] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p - i] = (float*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real32Value[p][i] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p - i] = (double*)NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      real64Value[p][i] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementsAllocated[p]);
    }
    for (i=0; i<elements; i++) {
      sprintf(s, "%" PRId16, values[i]);
      stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p - i] = NULL;
      }
    }
    if (elements > elementsAllocated[p]) {
      elementsAllocated[p] = elements + elementAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementsAllocated[p]);
    }
    memcpy(charValue[p], values, elements);
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  if (elementCount[p] < elements) {
    elementCount[p] = elements;
  }
  return(0);
}

/*
 * readAsciiValues
 *
 * C++ Arguments: char *value, uint32_t page
 *
 * Results:  0 for success
 *           1 for failure
 */
int32_t SDDSArray::readAsciiValues(char **bigBuffer, int32_t *bigBufferSize, uint32_t page) {
  int32_t returnVal=0, length;
  uint32_t i, p;
  int64_t iValue;
  double dValue;
  static char *buffer=NULL;
  static int32_t bufferSize=0;
  if ((page == 0) || (page - 1 != pageCount)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid page number (SDDSArray::readAsciiValues)");
    return(1);
  }
  if (arrayDimensions == 0) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array dimensions (SDDSArray::readAsciiValues)");
    return(1);
  }
  if (arrayDim == NULL) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array dimensions (SDDSArray::readAsciiValues)");
    return(1);
  }
  if (arrayDim[page - 1] == NULL) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array dimensions (SDDSArray::readAsciiValues)");
    return(1);
  }
  if (bufferSize==0) {
    bufferSize=SDDS_MAXLINE;
    buffer = (char*)SDDS_malloc(sizeof(*buffer)*(bufferSize));
  }
  if (bufferSize <= *bigBufferSize) {
    bufferSize = 2 * *bigBufferSize;
    buffer = (char*)SDDS_realloc(buffer, sizeof(*buffer)*(bufferSize));
  }

  p = page - 1;
  elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
  elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
  elementCount[p] = arrayDim[page - 1][0];
  for (i=1;i<arrayDimensions;i++) {
    elementCount[p] *= arrayDim[p][i];
  }
  elementsAllocated[p] = elementCount[p];
  switch (arrayType) {
  case SDDS_INT16:
    int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
    int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementsAllocated[p]);
    for (i=0; i<elementCount[p] ; i++) {
      if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array (SDDSArray::readAsciiValues)");
	returnVal=1;
	iValue=0;
      } else {
	iValue = strtoll(buffer, (char**)NULL, 10);
	if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	  ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--not a valid integer (SDDSArray::readAsciiValues)");
	  returnVal=1;
	  iValue=0;
	}
      }
      int16Value[p][i] = (int16_t)iValue;
    }
    break;
  case SDDS_UINT16:
    uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
    uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementsAllocated[p]);
    for (i=0; i<elementCount[p] ; i++) {
      if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array (SDDSArray::readAsciiValues)");
	returnVal=1;
	iValue=0;
      } else {
	iValue = strtoll(buffer, (char**)NULL, 10);
	if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	  ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--not a valid integer (SDDSArray::readAsciiValues)");
	  returnVal=1;
	  iValue=0;
	}
      }
      uint16Value[p][i] = (uint16_t)iValue;
    }
    break;
  case SDDS_INT32:
    int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
    int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementsAllocated[p]);
    for (i=0; i<elementCount[p] ; i++) {
      if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array (SDDSArray::readAsciiValues)");
	returnVal=1;
	iValue=0;
      } else {
	iValue = strtoll(buffer, (char**)NULL, 10);
	if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	  ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--not a valid integer (SDDSArray::readAsciiValues)");
	  returnVal=1;
	  iValue=0;
	}
      }
      int32Value[p][i] = (int32_t)iValue;
    }
    break;
  case SDDS_UINT32:
    uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
    uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementsAllocated[p]);
    for (i=0; i<elementCount[p] ; i++) {
      if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array (SDDSArray::readAsciiValues)");
	returnVal=1;
	iValue=0;
      } else {
	iValue = strtoll(buffer, (char**)NULL, 10);
	if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	  ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--not a valid integer (SDDSArray::readAsciiValues)");
	  returnVal=1;
	  iValue=0;
	}
      }
      uint32Value[p][i] = (uint32_t)iValue;
    }
    break;
  case SDDS_REAL32:
    real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
    real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementsAllocated[p]);
    for (i=0; i<elementCount[p] ; i++) {
      if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array (SDDSArray::readAsciiValues)");
	returnVal=1;
	dValue=0;
      } else {
	dValue = strtod(buffer, (char**)NULL);
	if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	  ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--not a valid number (SDDSArray::readAsciiValues)");
	  returnVal=1;
	  dValue=0;
	}
      }
      real32Value[p][i] = (float)dValue;
    }
    break;
  case SDDS_REAL64:
    real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
    real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementsAllocated[p]);
    for (i=0; i<elementCount[p] ; i++) {
      if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array (SDDSArray::readAsciiValues)");
	returnVal=1;
	dValue=0;
      } else {
	dValue = strtod(buffer, (char**)NULL);
	if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	  ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--not a valid number (SDDSArray::readAsciiValues)");
	  returnVal=1;
	  dValue=0;
	}
      }
      real64Value[p][i] = (double)dValue;
    }
    break;
  case SDDS_STRING:
    stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
    stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementsAllocated[p]);
    length = strlen(*bigBuffer);
    if (length > 0) {
      if ((*bigBuffer)[length-1] == '\r') {
        (*bigBuffer)[length-1] = 0;
      }
    }
    for (i=0; i<elementCount[p] ; i++) {
      if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array (SDDSArray::readAsciiValues)");
	returnVal=1;
	stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * 4);
	strcpy(stringValue[p][i], "\"\"");
      } else {
	SDDS_interpretEscapes(buffer);
	stringValue[p][i] = (char*)SDDS_malloc(sizeof(char) * strlen(buffer) + 1);
	strcpy(stringValue[p][i], buffer);
      }
    }
    break;
  case SDDS_CHARACTER:
    charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
    charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementsAllocated[p]);
    for (i=0; i<elementCount[p] ; i++) {
      if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array (SDDSArray::readAsciiValues)");
	returnVal=1;
	charValue[p][i] = '-';
      } else {
	charValue[p][i] = buffer[0];
      }
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array type (SDDSArray::readAsciiValues)");
    return(1);
  }
  pageCount = page;
  return(returnVal);
}

/*
 * readBinaryValues
 *
 * C++ Arguments: FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian
 * C++ Arguments: voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian
 *
 * Results:  0 on success
 *           1 on failure
 */
int32_t SDDSArray::readBinaryValues(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t i, p;

  if ((page == 0) || (page - 1 != pageCount)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid page number (SDDSArray::readBinaryValues)");
    return(1);
  }
  if (arrayDimensions == 0) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array dimensions (SDDSArray::readBinaryValues)");
    return(1);
  }
  if (arrayDim == NULL) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array dimensions (SDDSArray::readBinaryValues)");
    return(1);
  }
  if (arrayDim[page - 1] == NULL) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array dimensions (SDDSArray::readBinaryValues)");
    return(1);
  }
  p = page - 1;
  elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
  elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
  elementCount[p] = arrayDim[page - 1][0];
  for (i=1;i<arrayDimensions;i++) {
    elementCount[p] *= arrayDim[p][i];
  }
  elementsAllocated[p] = elementCount[p];
  switch (arrayType) {
  case SDDS_INT16:
    int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
    int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementCount[p]);
    if (SDDS_bufferedRead(&(int16Value[p][0]), 2 * elementCount[p], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
    uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementCount[p]);
    if (SDDS_bufferedRead(&(uint16Value[p][0]), 2 * elementCount[p], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_INT32:
    int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
    int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementCount[p]);
    if (SDDS_bufferedRead(&(int32Value[p][0]), 4 * elementCount[p], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
    uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementCount[p]);
    if (SDDS_bufferedRead(&(uint32Value[p][0]), 4 * elementCount[p], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
    real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementCount[p]);
    if (SDDS_bufferedRead(&(real32Value[p][0]), 4 * elementCount[p], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
    real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementCount[p]);
    if (SDDS_bufferedRead(&(real64Value[p][0]), 8 * elementCount[p], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_STRING:
    stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
    stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementCount[p]);
    for (i=0; i<elementCount[p]; i++) {
      stringValue[p][i] = NULL;
      if (SDDS_readBinaryString(&(stringValue[p][i]), fp, fBuffer, sddsfile)) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
	return(1);
      }
    }
    break;
  case SDDS_CHARACTER:
    charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
    charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementCount[p]);
    if (SDDS_bufferedRead(&(charValue[p][0]), elementCount[p], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--unknown type (SDDSArray::readBinaryValues)");
    return(1);
  }
  pageCount = page;
  return(0);
}

int32_t SDDSArray::readBinaryValues(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t i, p;

  if ((page == 0) || (page - 1 != pageCount)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid page number (SDDSArray::readBinaryValues)");
    return(1);
  }
  if (arrayDimensions == 0) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array dimensions (SDDSArray::readBinaryValues)");
    return(1);
  }
  if (arrayDim == NULL) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array dimensions (SDDSArray::readBinaryValues)");
    return(1);
  }
  if (arrayDim[page - 1] == NULL) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--invalid array dimensions (SDDSArray::readBinaryValues)");
    return(1);
  }
  p = page - 1;
  elementCount = (uint32_t*)SDDS_realloc(elementCount, sizeof(uint32_t) * page);
  elementsAllocated = (uint32_t*)SDDS_realloc(elementsAllocated, sizeof(uint32_t) * page);
  elementCount[p] = arrayDim[page - 1][0];
  for (i=1;i<arrayDimensions;i++) {
    elementCount[p] *= arrayDim[p][i];
  }
  elementsAllocated[p] = elementCount[p];
  switch (arrayType) {
  case SDDS_INT16:
    int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
    int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * elementCount[p]);
    if (SDDS_bufferedRead(&(int16Value[p][0]), 2 * elementCount[p], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
    uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * elementCount[p]);
    if (SDDS_bufferedRead(&(uint16Value[p][0]), 2 * elementCount[p], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_INT32:
    int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
    int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * elementCount[p]);
    if (SDDS_bufferedRead(&(int32Value[p][0]), 4 * elementCount[p], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
    uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * elementCount[p]);
    if (SDDS_bufferedRead(&(uint32Value[p][0]), 4 * elementCount[p], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
    real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * elementCount[p]);
    if (SDDS_bufferedRead(&(real32Value[p][0]), 4 * elementCount[p], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
    real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * elementCount[p]);
    if (SDDS_bufferedRead(&(real64Value[p][0]), 8 * elementCount[p], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_STRING:
    stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
    stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * elementCount[p]);
    for (i=0; i<elementCount[p]; i++) {
      stringValue[p][i] = NULL;
      if (SDDS_readBinaryString(&(stringValue[p][i]), gzfp, fBuffer, sddsfile)) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
	return(1);
      }
    }
    break;
  case SDDS_CHARACTER:
    charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
    charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * elementCount[p]);
    if (SDDS_bufferedRead(&(charValue[p][0]), elementCount[p], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--failure reading value (SDDSArray::readBinaryValues)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read array--unknown type (SDDSArray::readBinaryValues)");
    return(1);
  }
  pageCount = page;
  return(0);
}

/*
 * getInteralValues
 *
 * C++ Arguments: uint32_t page
 *
 * Results:  pointer to values
 *           NULL for array value undefined
 */
void* SDDSArray::getInternalValues(uint32_t page) {
  uint32_t p;
  if ((page == 0) || (pageCount < page)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Invalid page (SDDSArray::getInternalValues)");
    return(NULL);
  }
  p = page - 1;
  if (elementCount[p] == 0) {
    return(NULL);
  }
  switch (arrayType) {
  case SDDS_INT16:
    return(int16Value[p]);
  case SDDS_UINT16:
    return(uint16Value[p]);
  case SDDS_INT32:
    return(int32Value[p]);
  case SDDS_UINT32:
    return(uint32Value[p]);
  case SDDS_REAL32:
    return(real32Value[p]);
  case SDDS_REAL64:
    return(real64Value[p]);
  case SDDS_STRING:
    return(stringValue[p]);
  case SDDS_CHARACTER:
    return(charValue[p]);
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Invalid data type (SDDSArray::getInternalValues)");
    return(NULL);
  }
}

/*
 * getValuesInInt32
 *
 * C++ Arguments: uint32_t page, int32_t *errorcode
 *
 * Results:  pointer to values and errorcode=0 for success
 *           NULL and errorcode=1 for array value undefined
 *           NULL and errorcode=2 for the case where the value cannot be
 *                                    converted to the proper type
 *           NULL and errorcode=3 for invalid array type
 */
int32_t* SDDSArray::getValuesInInt32(uint32_t page, int32_t *errorcode) {
  int64_t iValue;
  int32_t* rValues;
  uint32_t i, p;
  if ((page == 0) || (pageCount < page)) {
    *errorcode = 1;
    return((int32_t*)NULL);
  }
  *errorcode = 0;
  p = page - 1;
  if (elementCount[p] == 0) {
    return((int32_t*)NULL);
  }
  rValues = (int32_t*)SDDS_malloc(sizeof(int32_t) * elementCount[p]);
  switch (arrayType) {
  case SDDS_INT16:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (int32_t)int16Value[p][i];
    return((int32_t*)rValues);
  case SDDS_UINT16:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (int32_t)uint16Value[p][i];
    return((int32_t*)rValues);
  case SDDS_INT32:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (int32_t)int32Value[p][i];
    return((int32_t*)rValues);
  case SDDS_UINT32:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (int32_t)uint32Value[p][i];
    return((int32_t*)rValues);
  case SDDS_REAL32:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (int32_t)real32Value[p][i];
    return((int32_t*)rValues);
  case SDDS_REAL64:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (int32_t)real64Value[p][i];
    return((int32_t*)rValues);
  case SDDS_STRING:
    for (i=0; i<elementCount[p]; i++) {
      iValue = strtoll(stringValue[p][i], (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	/*unable to convert fixed value string to integer*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((int32_t*)NULL);
      }
      rValues[i] = (int32_t)iValue;
    }
    return((int32_t*)rValues);
  case SDDS_CHARACTER:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (int32_t)charValue[p][i];
    return((int32_t*)rValues);
  default:
    *errorcode = 3;
    return((int32_t*)NULL);
  }
}

/*
 * getValuesInUInt32
 *
 * C++ Arguments: uint32_t page, int32_t *errorcode
 *
 * Results:  pointer to values and errorcode=0 for success
 *           NULL and errorcode=1 for array value undefined
 *           NULL and errorcode=2 for the case where the value cannot be
 *                                    converted to the proper type
 *           NULL and errorcode=3 for invalid array type
 */
uint32_t* SDDSArray::getValuesInUInt32(uint32_t page, int32_t *errorcode) {
  int64_t iValue;
  uint32_t* rValues;
  uint32_t i, p;
  if ((page == 0) || (pageCount < page)) {
    *errorcode = 1;
    return((uint32_t*)NULL);
  }
  *errorcode = 0;
  p = page - 1;
  if (elementCount[p] == 0) {
    return((uint32_t*)NULL);
  }
  rValues = (uint32_t*)SDDS_malloc(sizeof(uint32_t) * elementCount[p]);
  switch (arrayType) {
  case SDDS_INT16:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (uint32_t)int16Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_UINT16:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (uint32_t)uint16Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_INT32:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (uint32_t)int32Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_UINT32:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (uint32_t)uint32Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_REAL32:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (uint32_t)real32Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_REAL64:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (uint32_t)real64Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_STRING:
    for (i=0; i<elementCount[p]; i++) {
      iValue = strtoll(stringValue[p][i], (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	/*unable to convert fixed value string to integer*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((uint32_t*)NULL);
      }
      rValues[i] = (uint32_t)iValue;
    }
    return((uint32_t*)rValues);
  case SDDS_CHARACTER:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (uint32_t)charValue[p][i];
    return((uint32_t*)rValues);
  default:
    *errorcode = 3;
    return((uint32_t*)NULL);
  }
}

/*
 * getValuesInDouble
 *
 * C++ Arguments: uint32_t page, int32_t *errorcode
 *
 * Results:  pointer to values and errorcode=0 for success
 *           NULL and errorcode=1 for array value undefined
 *           NULL and errorcode=2 for the case where the value cannot be
 *                                    converted to the proper type
 *           NULL and errorcode=3 for invalid array type
 */
double* SDDSArray::getValuesInDouble(uint32_t page, int32_t *errorcode) {
  double dValue;
  double* rValues;
  uint32_t i, p;
  if ((page == 0) || (pageCount < page)) {
    *errorcode = 1;
    return((double*)NULL);
  }
  *errorcode = 0;
  p = page - 1;
  if (elementCount[p] == 0) {
    return((double*)NULL);
  }
  rValues = (double*)SDDS_malloc(sizeof(double) * elementCount[p]);
  switch (arrayType) {
  case SDDS_INT16:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (double)int16Value[p][i];
    return((double*)rValues);
  case SDDS_UINT16:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (double)uint16Value[p][i];
    return((double*)rValues);
  case SDDS_INT32:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (double)int32Value[p][i];
    return((double*)rValues);
  case SDDS_UINT32:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (double)uint32Value[p][i];
    return((double*)rValues);
  case SDDS_REAL32:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (double)real32Value[p][i];
    return((double*)rValues);
  case SDDS_REAL64:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (double)real64Value[p][i];
    return((double*)rValues);
  case SDDS_STRING:
    for (i=0; i<elementCount[p]; i++) {
      dValue = strtod(stringValue[p][i], (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	/*unable to convert string to floating point*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((double*)NULL);
      }
      rValues[i] = (double)dValue;
    }
    return((double*)rValues);
  case SDDS_CHARACTER:
    for (i=0; i<elementCount[p]; i++)
      rValues[i] = (double)charValue[p][i];
    return((double*)rValues);
  default:
    *errorcode = 3;
    return((double*)NULL);
  }
}

/*
 * getValuesInString
 *
 * C++ Arguments: uint32_t page, int32_t *errorcode
 *
 * Results:  pointer to values and errorcode=0 for success
 *           NULL and errorcode=1 for array value undefined
 *           NULL and errorcode=2 for the case where the value cannot be
 *                                    converted to the proper type
 *           NULL and errorcode=3 for invalid array type
 */
char** SDDSArray::getValuesInString(uint32_t page, int32_t *errorcode) {
  char** rValues;
  uint32_t i, p;
  char s[SDDS_MAXLINE];
  if ((page == 0) || (pageCount < page)) {
    *errorcode = 1;
    return((char**)NULL);
  }
  *errorcode = 0;
  p = page - 1;
  if (elementCount[p] == 0) {
    return((char**)NULL);
  }
  rValues = (char**)SDDS_malloc(sizeof(char*) * elementCount[p]);
  switch (arrayType) {
  case SDDS_INT16:
    for (i=0; i<elementCount[p]; i++) {
      sprintf(s, "%" PRId16, int16Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_UINT16:
    for (i=0; i<elementCount[p]; i++) {
      sprintf(s, "%" PRIu16, uint16Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_INT32:
    for (i=0; i<elementCount[p]; i++) {
      sprintf(s, "%" PRId32, int32Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_UINT32:
    for (i=0; i<elementCount[p]; i++) {
      sprintf(s, "%" PRIu32, uint32Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_REAL32:
    for (i=0; i<elementCount[p]; i++) {
      sprintf(s, "%f", real32Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_REAL64:
    for (i=0; i<elementCount[p]; i++) {
      sprintf(s, "%lf", real64Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_STRING:
    for (i=0; i<elementCount[p]; i++) {
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(stringValue[p][i]) + 1);
      strcpy(rValues[i], stringValue[p][i]);
    }
    return((char**)rValues);
  case SDDS_CHARACTER:
    for (i=0; i<elementCount[p]; i++) {
      sprintf(s, "%c", charValue[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  default:
    *errorcode = 3;
    return((char**)NULL);
  }
}

/*
 * writeAsciiValues
 *
 * C++ Arguments: FILE *fp, uint32_t page
 * C++ Arguments: voidp *gzfp, uint32_t page
 *
 * Results:  0 on success
 *           1 on failure
 */
int32_t SDDSArray::writeAsciiValues(FILE *fp, uint32_t page) {
  char c;
  char *s;
  int32_t hasWhitespace;
  uint32_t i;

  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (arrayType) {
  case SDDS_INT16:
    for (i=0; i<elementCount[page]; i++) {
      fprintf(fp, "%" PRId16 " ", int16Value[page][i]);
    }
    break;
  case SDDS_UINT16:
    for (i=0; i<elementCount[page]; i++) {
      fprintf(fp, "%" PRIu16 " ", uint16Value[page][i]);
    }
    break;
  case SDDS_INT32:
    for (i=0; i<elementCount[page]; i++) {
      fprintf(fp, "%" PRId32 " ", int32Value[page][i]);
    }
    break;
  case SDDS_UINT32:
    for (i=0; i<elementCount[page]; i++) {
      fprintf(fp, "%" PRIu32 " ", uint32Value[page][i]);
    }
    break;
  case SDDS_REAL32:
    for (i=0; i<elementCount[page]; i++) {
      fprintf(fp, "%.*g ", FLT_DIG, real32Value[page][i]);
    }
    break;
  case SDDS_REAL64:
    for (i=0; i<elementCount[page]; i++) {
      fprintf(fp, "%.*g ", DBL_DIG, real64Value[page][i]);
    }
    break;
  case SDDS_STRING:
    for (i=0; i<elementCount[page]; i++) {
      s = stringValue[page][i];
      hasWhitespace=0;
      if (SDDS_hasWhitespace(s) || SDDS_stringIsBlank(s)) {
	fputc('"', fp);
	hasWhitespace = 1;
      }
      while (s && *s) {
	c = *s++;
	if (c=='!') {
	  fputs("\\!", fp);
	} else if (c=='\\') {
	  fputs("\\\\", fp);
	} else if (c=='"') {
	  fputs("\\\"", fp);
	} else if (c==' ') {
	  fputc(' ', fp);   /* don't escape plain spaces */
	} else if (isspace(c) || !isprint(c)) {
	  fprintf(fp, "\\%03o", c);
	} else {
	  fputc(c, fp);
	}
      }
      if (hasWhitespace) {
	fputc('"', fp);
      }
      fputc(' ', fp);
    }
    break;
  case SDDS_CHARACTER:
    for (i=0; i<elementCount[page]; i++) {
      c = charValue[page][i];
      if (c=='!')
	fputs("\\!", fp);
      else if (c=='\\')
      fputs("\\\\", fp);
      else if (c=='"')
	fputs("\\\"", fp);
      else if (!c || isspace(c) || !isprint(c))
	fprintf(fp, "\\%03o", c);
      else
	fputc(c, fp);
      fputc(' ', fp);
    }
    break;
  default:
    return(1);
  }
  fprintf(fp, "\n");
  return(0);
}

int32_t SDDSArray::writeAsciiValues(voidp *gzfp, uint32_t page) {
  char c;
  char *s;
  int32_t hasWhitespace;
  uint32_t i;

  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (arrayType) {
  case SDDS_INT16:
    for (i=0; i<elementCount[page]; i++) {
      gzprintf(gzfp, "%" PRId16 " ", int16Value[page][i]);
    }
    break;
  case SDDS_UINT16:
    for (i=0; i<elementCount[page]; i++) {
      gzprintf(gzfp, "%" PRIu16 " ", uint16Value[page][i]);
    }
    break;
  case SDDS_INT32:
    for (i=0; i<elementCount[page]; i++) {
      gzprintf(gzfp, "%" PRId32 " ", int32Value[page][i]);
    }
    break;
  case SDDS_UINT32:
    for (i=0; i<elementCount[page]; i++) {
      gzprintf(gzfp, "%" PRIu32 " ", uint32Value[page][i]);
    }
    break;
  case SDDS_REAL32:
    for (i=0; i<elementCount[page]; i++) {
      gzprintf(gzfp, "%.*g ", FLT_DIG, real32Value[page][i]);
    }
    break;
  case SDDS_REAL64:
    for (i=0; i<elementCount[page]; i++) {
      gzprintf(gzfp, "%.*g ", DBL_DIG, real64Value[page][i]);
    }
    break;
  case SDDS_STRING:
    for (i=0; i<elementCount[page]; i++) {
      s = stringValue[page][i];
      hasWhitespace=0;
      if (SDDS_hasWhitespace(s) || SDDS_stringIsBlank(s)) {
	gzputc(gzfp, '"');
	hasWhitespace = 1;
      }
      while (s && *s) {
	c = *s++;
	if (c=='!') {
	  gzputs(gzfp, "\\!");
	} else if (c=='\\') {
	  gzputs(gzfp, "\\\\");
	} else if (c=='"') {
	  gzputs(gzfp, "\\\"");
	} else if (c==' ') {
	  gzputc(gzfp, ' ');   /* don't escape plain spaces */
	} else if (isspace(c) || !isprint(c)) {
	  gzprintf(gzfp, "\\%03o", c);
	} else {
	  gzputc(gzfp, c);
	}
      }
      if (hasWhitespace) {
	gzputc(gzfp, '"');
      }
      gzputc(gzfp, ' ');
    }
    break;
  case SDDS_CHARACTER:
    for (i=0; i<elementCount[page]; i++) {
      c = charValue[page][i];
      if (c=='!')
	gzputs(gzfp, "\\!");
      else if (c=='\\')
	gzputs(gzfp, "\\\\");
      else if (c=='"')
	gzputs(gzfp, "\\\"");
      else if (!c || isspace(c) || !isprint(c))
	gzprintf(gzfp, "\\%03o", c);
      else
	gzputc(gzfp, c);
      gzputc(gzfp, ' ');
    }
    break;
  default:
    return(1);
  }
  gzprintf(gzfp, "\n");
  return(0);
}

/*
 * writeBinaryValues
 *
 * C++ Arguments: FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian
 * C++ Arguments: voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian
 *
 * Results:  0 on success
 *           1 on failure
 */
int32_t SDDSArray::writeBinaryValues(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t i;
  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (arrayType) {
  case SDDS_INT16:
    if (SDDS_bufferedWrite(&(int16Value[page][0]), 2 * elementCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    if (SDDS_bufferedWrite(&(uint16Value[page][0]), 2 * elementCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_INT32:
    if (SDDS_bufferedWrite(&(int32Value[page][0]), 4 * elementCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    if (SDDS_bufferedWrite(&(uint32Value[page][0]), 4 * elementCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    if (SDDS_bufferedWrite(&(real32Value[page][0]), 4 * elementCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    if (SDDS_bufferedWrite(&(real64Value[page][0]), 8 * elementCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_STRING:
    for (i=0; i<elementCount[page]; i++) {
      if (SDDS_writeBinaryString(stringValue[page][i], fp, fBuffer, sddsfile)) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing string (SDDSArray::writeBinaryValues)");
	return(1);
      }
    }
    break;
  case SDDS_CHARACTER:
    if (SDDS_bufferedWrite(&(charValue[page][0]), elementCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--unknown type (SDDSArray::writeBinaryValues)");
    return(1);
  }
  return(0);
}

int32_t SDDSArray::writeBinaryValues(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t i;
  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (arrayType) {
  case SDDS_INT16:
    if (SDDS_bufferedWrite(&(int16Value[page][0]), 2 * elementCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    if (SDDS_bufferedWrite(&(uint16Value[page][0]), 2 * elementCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_INT32:
    if (SDDS_bufferedWrite(&(int32Value[page][0]), 4 * elementCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    if (SDDS_bufferedWrite(&(uint32Value[page][0]), 4 * elementCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    if (SDDS_bufferedWrite(&(real32Value[page][0]), 4 * elementCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    if (SDDS_bufferedWrite(&(real64Value[page][0]), 8 * elementCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  case SDDS_STRING:
    for (i=0; i<elementCount[page]; i++) {
      if (SDDS_writeBinaryString(stringValue[page][i], gzfp, fBuffer, sddsfile)) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing string (SDDSArray::writeBinaryValues)");
	return(1);
      }
    }
    break;
  case SDDS_CHARACTER:
    if (SDDS_bufferedWrite(&(charValue[page][0]), elementCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--failure writing value (SDDSArray::writeBinaryValues)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to write array--unknown type (SDDSArray::writeBinaryValues)");
    return(1);
  }
  return(0);
}
