/* GUIDELINES
   Do not exit or print to stdout or stderr from any of the functions in this file
*/

/* TODO 
        Ensure that strtod returns HUGE_VAL on all operating systems.
        Modify setType so that it can change the type of an existing column.
        Update so that you can not read pages out of order (this may be needed for blank pages)
	fix nonnative binary reads.
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


/***********************************************************************************************
 *                          SDDSColumn                                                         *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to SDDSColumn object                                                      *
 ***********************************************************************************************/
SDDSColumn::SDDSColumn() {
  pageCount = 0;
  rowCount = NULL;
  rowsAllocated = NULL;
  rowAllocationIncrement = 1000;
  columnType = 0;
  columnName = NULL;
  columnSymbol = NULL;
  columnUnits = NULL;
  columnDescription = NULL;
  columnFormatString = NULL;
  columnFieldLength = 0;
  int32Value = NULL;
  int16Value = NULL;
  uint32Value = NULL;
  uint16Value = NULL;
  real64Value = NULL;
  real32Value = NULL;
  stringValue = NULL;
  charValue = NULL;
}

/***********************************************************************************************
 *                          ~SDDSColumn                                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
SDDSColumn::~SDDSColumn() {
  uint32_t i, j;
  if (columnName != NULL)
    SDDS_free(columnName);
  if (columnSymbol != NULL)
    SDDS_free(columnSymbol);
  if (columnUnits != NULL)
    SDDS_free(columnUnits);
  if (columnDescription != NULL)
    SDDS_free(columnDescription);
  if (columnFormatString != NULL)
    SDDS_free(columnFormatString);
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
	for (j=0; j<rowCount[i] ; j++) {
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
  if (rowCount != NULL) {
    SDDS_free(rowCount);
  }
  if (rowsAllocated != NULL) {
    SDDS_free(rowsAllocated);
  }
}

/***********************************************************************************************
 *                             freePage                                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSColumn::freePage() {
  uint32_t i, j;
  switch (columnType) {
  case SDDS_INT16:
    if (int16Value != NULL) {
      for (i=0; i<pageCount; i++) {
        if (int16Value[i] != NULL) {
          SDDS_free(int16Value[i]);
        }
      }
      SDDS_free(int16Value);
    }
    int16Value = NULL;
    break;
  case SDDS_UINT16:
    if (uint16Value != NULL) {
      for (i=0; i<pageCount; i++) {
        if (uint16Value[i] != NULL) {
          SDDS_free(uint16Value[i]);
        }
      }
      SDDS_free(uint16Value);
    }
    uint16Value = NULL;
    break;
  case SDDS_INT32:
    if (int32Value != NULL) {
      for (i=0; i<pageCount; i++) {
        if (int32Value[i] != NULL) {
          SDDS_free(int32Value[i]);
        }
      }
      SDDS_free(int32Value);
    }
    int32Value = NULL;
    break;
  case SDDS_UINT32:
    if (uint32Value != NULL) {
      for (i=0; i<pageCount; i++) {
        if (uint32Value[i] != NULL) {
          SDDS_free(uint32Value[i]);
        }
      }
      SDDS_free(uint32Value);
    }
    uint32Value = NULL;
    break;
  case SDDS_REAL32:
    if (real32Value != NULL) {
      for (i=0; i<pageCount; i++) {
        if (real32Value[i] != NULL) {
          SDDS_free(real32Value[i]);
        }
      }
      SDDS_free(real32Value);
    }
    real32Value = NULL;
    break;
  case SDDS_REAL64:
    if (real64Value != NULL) {
      for (i=0; i<pageCount; i++) {
        if (real64Value[i] != NULL) {
          SDDS_free(real64Value[i]);
        }
      }
      SDDS_free(real64Value);
    }
    real64Value = NULL;
    break;
  case SDDS_STRING:
    if (stringValue != NULL) {
      for (i=0; i<pageCount; i++) {
        if (stringValue[i] != NULL) {
          for (j=0; j<rowCount[i] ; j++) {
            if (stringValue[i][j] != NULL) {
              SDDS_free(stringValue[i][j]);
            }
          }
          SDDS_free(stringValue[i]);
        }
      }
      SDDS_free(stringValue);
    }
    stringValue = NULL;
    break;
  case SDDS_CHARACTER:
    if (charValue != NULL) {
      for (i=0; i<pageCount; i++) {
        if (charValue[i] != NULL) {
          SDDS_free(charValue[i]);
        }
      }
      SDDS_free(charValue);
    }
    charValue = NULL;
    break;
  default:
    break;
  }
  if (rowCount != NULL) {
    SDDS_free(rowCount);
  }
  rowCount = NULL;
  if (rowsAllocated != NULL) {
    SDDS_free(rowsAllocated);
  }
  rowsAllocated = NULL;
  pageCount = 0;
}

/***********************************************************************************************
 *                           setupColumn                                                       *
 *                                                                                             *
 * C++ Arguments: char *name, char *symbol, char *units, char *description,                    *
 *                char *format_string, int32_t type, uint32_t field_length                      *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSColumn::setupColumn(char *name, char *symbol, char *units, char *description, char *format_string, int32_t type, uint32_t field_length) {
  if (columnName != NULL)
    SDDS_free(columnName);
  if (columnSymbol != NULL)
    SDDS_free(columnSymbol);
  if (columnUnits != NULL)
    SDDS_free(columnUnits);
  if (columnDescription != NULL)
    SDDS_free(columnDescription);
  if (columnFormatString != NULL)
    SDDS_free(columnFormatString);

  if (name == NULL)
    columnName = NULL;
  else {
    columnName = (char*)SDDS_malloc(sizeof(char) * strlen(name) + 1);
    strcpy(columnName, name);
  }
  if (symbol == NULL)
    columnSymbol = NULL;
  else {
    columnSymbol = (char*)SDDS_malloc(sizeof(char) * strlen(symbol) + 1);
    strcpy(columnSymbol, symbol);
  }
  if (units == NULL)
    columnUnits = NULL;
  else {
    columnUnits = (char*)SDDS_malloc(sizeof(char) * strlen(units) + 1);
    strcpy(columnUnits, units);
  }
  if (description == NULL)
    columnDescription = NULL;
  else {
    columnDescription = (char*)SDDS_malloc(sizeof(char) * strlen(description) + 1);
    strcpy(columnDescription, description);
  }
  if (format_string == NULL)
    columnFormatString = NULL;
  else {
    columnFormatString = (char*)SDDS_malloc(sizeof(char) * strlen(format_string) + 1);
    strcpy(columnFormatString, format_string);
  }
  columnType = type;
  columnFieldLength = field_length;
}

/***********************************************************************************************
 *                             informAboutParent                                               *
 *                                                                                             *
 * C++ Arguments: SDDSFile *parent                                                             *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSColumn::informAboutParent(void *parent) {
  sddsfile = parent;
}

/***********************************************************************************************
 *                             getName                                                         *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to name  (do not free the result)                                         *
 ***********************************************************************************************/
char* SDDSColumn::getName() {
  return(columnName);
}

/***********************************************************************************************
 *                             setName                                                         *
 *                                                                                             *
 * C++ Arguments: char *name                                                                   *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSColumn::setName(char *name) {
  if (columnName != NULL)
    SDDS_free(columnName);
  if (name == NULL)
    columnName = NULL;
  else {
    columnName = (char*)SDDS_malloc(sizeof(char) * strlen(name) + 1);
    strcpy(columnName, name);
  }
}

/***********************************************************************************************
 *                             getSymbol                                                       *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to symbol (do not free the result)                                        *
 ***********************************************************************************************/
char* SDDSColumn::getSymbol() {
  return(columnSymbol);
}

/***********************************************************************************************
 *                             setSymbol                                                       *
 *                                                                                             *
 * C++ Arguments: char *symbol                                                                 *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSColumn::setSymbol(char *symbol) {
  if (columnSymbol != NULL)
    SDDS_free(columnSymbol);
  if (symbol == NULL)
    columnSymbol = NULL;
  else {
    columnSymbol = (char*)SDDS_malloc(sizeof(char) * strlen(symbol) + 1);
    strcpy(columnSymbol, symbol);
  }
}

/***********************************************************************************************
 *                             getUnits                                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to the units (do not free the result)                                     *
 ***********************************************************************************************/
char* SDDSColumn::getUnits() {
  return(columnUnits);
}

/***********************************************************************************************
 *                             setUnits                                                        *
 *                                                                                             *
 * C++ Arguments: char *units                                                                  *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSColumn::setUnits(char *units) {
  if (columnUnits != NULL)
    SDDS_free(columnUnits);
  if (units == NULL)
    columnUnits = NULL;
  else {
    columnUnits = (char*)SDDS_malloc(sizeof(char) * strlen(units) + 1);
    strcpy(columnUnits, units);
  }
}

/***********************************************************************************************
 *                             getDescription                                                  *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to the description (do not free the result)                               *
 ***********************************************************************************************/
char* SDDSColumn::getDescription() {
  return(columnDescription);
}

/***********************************************************************************************
 *                             setDescription                                                  *
 *                                                                                             *
 * C++ Arguments: char *description                                                            *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSColumn::setDescription(char *description) {
  if (columnDescription != NULL)
    SDDS_free(columnDescription);
  if (description == NULL)
    columnDescription = NULL;
  else {
    columnDescription = (char*)SDDS_malloc(sizeof(char) * strlen(description) + 1);
    strcpy(columnDescription, description);
  }
}

/***********************************************************************************************
 *                             getFormatString                                                 *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to the format string (do not free the result)                             *
 ***********************************************************************************************/
char* SDDSColumn::getFormatString() {
  return(columnFormatString);
}

/***********************************************************************************************
 *                             setFormatString                                                 *
 *                                                                                             *
 * C++ Arguments: char *format_string                                                          *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSColumn::setFormatString(char *format_string) {
  if (columnFormatString != NULL)
    SDDS_free(columnFormatString);
  if (format_string == NULL)
    columnFormatString = NULL;
  else {
    columnFormatString = (char*)SDDS_malloc(sizeof(char) * strlen(format_string) + 1);
    strcpy(columnFormatString, format_string);
  }
}

/***********************************************************************************************
 *                             getFieldLength                                                  *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: the column field length in integer form                                            *
 ***********************************************************************************************/
uint32_t SDDSColumn::getFieldLength() {
  return(columnFieldLength);
}

/***********************************************************************************************
 *                             setFieldLength                                                  *
 *                                                                                             *
 * C++ Arguments:uint32_t field_length                                                          *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSColumn::setFieldLength(uint32_t field_length) {
  columnFieldLength = field_length;
}

/***********************************************************************************************
 *                             getType                                                         *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: the column type in integer form                                                    *
 ***********************************************************************************************/
int32_t SDDSColumn::getType() {
  return(columnType);
}

/***********************************************************************************************
 *                             setType                                                         *
 *                                                                                             *
 * C++ Arguments:int32_t type                                                                  *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSColumn::setType(int32_t type) {
  columnType = type;
}

/***********************************************************************************************
 *                             getPageCount                                                    *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: the page count                                                                     *
 ***********************************************************************************************/
uint32_t SDDSColumn::getPageCount() {
  return(pageCount);
}

/***********************************************************************************************
 *                             getRowCount                                                     *
 *                                                                                             *
 * C++ Arguments: uint32_t page                                                                 *
 *                                                                                             *
 * Results: the row count on success                                                           *
 *          0 on failure                                                                *
 ***********************************************************************************************/
uint32_t SDDSColumn::getRowCount(uint32_t page) {
  if (rowCount == NULL) {
    return(0);
  }
  if ((page == 0) || (page > pageCount)) {
    return(0);
  }
  return(rowCount[page-1]);
}



/***********************************************************************************************
 *                        setValues                                                            *
 *                                                                                             *
 * C++ Arguments: int16_t *values, uint32_t page, uint32_t startRow, uint32_t rows                *
 *                uint16_t *value, uint32_t page, uint32_t startRow, uint32_t rows                *
 *                int32_t *values, uint32_t page, uint32_t startRow, uint32_t rows                *
 *                uint32_t *value, uint32_t page, uint32_t startRow, uint32_t rows                *
 *                float *value, uint32_t page, uint32_t startRow, uint32_t rows                   *
 *                double *value, uint32_t page, uint32_t startRow, uint32_t rows                  *
 *                char **value, uint32_t page, uint32_t startRow, uint32_t rows                   *
 *                char *value, uint32_t page, uint32_t startRow, uint32_t rows                    *
 *                                                                                             *
 * Results:  0 for success                                                                     *
 *           1 for invalid page number                                                         *
 *           2 for invalid startRow number                                                     *
 *           3 for the case where the value cannot be converted to the proper column type      *
 *           4 for invalid column type                                                         *
 ***********************************************************************************************/
int32_t SDDSColumn::setValues(int16_t *values, uint32_t page, uint32_t startRow, uint32_t rows) {
  uint32_t rowsNeeded, i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (startRow == 0)
    return(2);
  startRow--;
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  rowsNeeded = rows + startRow;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new pages */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = (int16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    if (startRow > 0) {
      for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
        int16Value[p][i] = (int16_t)0;
      }
      for (i=0; i<rows; i++) {
        int16Value[p][i+startRow] = (int16_t)(values[i]);
      }
    } else {
      memcpy(int16Value[p], values, rows * 2);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new pages */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = (uint16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint16Value[p][i] = (uint16_t)0;
    }
    for (i=0; i<rows; i++) {
      uint16Value[p][i+startRow] = (uint16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new pages */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = (int32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int32Value[p][i] = (int32_t)0;
    }
    for (i=0; i<rows; i++) {
      int32Value[p][i+startRow] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new pages */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = (uint32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint32Value[p][i] = (uint32_t)0;
    }
    for (i=0; i<rows; i++) {
      uint32Value[p][i+startRow] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new pages */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = (float*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real32Value[p][i] = (float)0;
    }
    for (i=0; i<rows; i++) {
      real32Value[p][i+startRow] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new pages */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = (double*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real64Value[p][i] = (double)0;
    }
    for (i=0; i<rows; i++) {
      real64Value[p][i+startRow] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = (char**)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put NULL in skiped rows */
      stringValue[p][i] = (char*)NULL;
    }
    for (i=0; i<rows; i++) {
      sprintf(s, "%" PRId16, values[i]);
      stringValue[p][i+startRow] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i+startRow], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new pages */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = (char*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      charValue[p][i] = (char)0;
    }
    for (i=0; i<rows; i++) {
      charValue[p][i+startRow] = (char)(values[i]);
    }
    break;
  default:
    return(4);
  }
  if (newPage) {
    pageCount = page;
  }
  if (rowCount[p] < rowsNeeded) {
    rowCount[p] = rowsNeeded;
  }
  return(0);
}

int32_t SDDSColumn::setValues(uint16_t *values, uint32_t page, uint32_t startRow, uint32_t rows) {
  uint32_t rowsNeeded, i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page < 1)
    return(1);
  if (startRow == 0)
    return(2);
  startRow--;
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  rowsNeeded = rows + startRow;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new pages */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = (int16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int16Value[p][i] = (int16_t)0;
    }
    for (i=0; i<rows; i++) {
      int16Value[p][i+startRow] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new pages */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = (uint16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    if (startRow > 0) {
      for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
        uint16Value[p][i] = (uint16_t)0;
      }
      for (i=0; i<rows; i++) {
        uint16Value[p][i+startRow] = (uint16_t)(values[i]);
      }
    } else {
      memcpy(uint16Value[p], values, rows * 2);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new pages */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = (int32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int32Value[p][i] = (int32_t)0;
    }
    for (i=0; i<rows; i++) {
      int32Value[p][i+startRow] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new pages */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = (uint32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint32Value[p][i] = (uint32_t)0;
    }
    for (i=0; i<rows; i++) {
      uint32Value[p][i+startRow] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new pages */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = (float*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real32Value[p][i] = (float)0;
    }
    for (i=0; i<rows; i++) {
      real32Value[p][i+startRow] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new pages */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = (double*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real64Value[p][i] = (double)0;
    }
    for (i=0; i<rows; i++) {
      real64Value[p][i+startRow] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = (char**)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put NULL in skiped rows */
      stringValue[p][i] = (char*)NULL;
    }
    for (i=0; i<rows; i++) {
      sprintf(s, "%" PRIu16, values[i]);
      stringValue[p][i+startRow] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i+startRow], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new pages */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = (char*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      charValue[p][i] = (char)0;
    }
    for (i=0; i<rows; i++) {
      charValue[p][i+startRow] = (char)(values[i]);
    }
    break;
  default:
    return(4);
  }
  if (newPage) {
    pageCount = page;
  }
  if (rowCount[p] < rowsNeeded) {
    rowCount[p] = rowsNeeded;
  }
  return(0);
}

int32_t SDDSColumn::setValues(int32_t *values, uint32_t page, uint32_t startRow, uint32_t rows) {
  uint32_t rowsNeeded, i, n=0, p;;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page < 1)
    return(1);
  if (startRow == 0)
    return(2);
  startRow--;
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  rowsNeeded = rows + startRow;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new pages */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = (int16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int16Value[p][i] = (int16_t)0;
    }
    for (i=0; i<rows; i++) {
      int16Value[p][i+startRow] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new pages */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = (uint16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint16Value[p][i] = (uint16_t)0;
    }
    for (i=0; i<rows; i++) {
      uint16Value[p][i+startRow] = (uint16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new pages */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = (int32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    if (startRow > 0) {
      for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
        int32Value[p][i] = (int32_t)0;
      }
      for (i=0; i<rows; i++) {
        int32Value[p][i+startRow] = (int32_t)(values[i]);
      }
    } else {
      memcpy(int32Value[p], values, rows * 4);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new pages */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = (uint32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint32Value[p][i] = (uint32_t)0;
    }
    for (i=0; i<rows; i++) {
      uint32Value[p][i+startRow] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new pages */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = (float*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real32Value[p][i] = (float)0;
    }
    for (i=0; i<rows; i++) {
      real32Value[p][i+startRow] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new pages */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = (double*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real64Value[p][i] = (double)0;
    }
    for (i=0; i<rows; i++) {
      real64Value[p][i+startRow] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = (char**)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put NULL in skiped rows */
      stringValue[p][i] = (char*)NULL;
    }
    for (i=0; i<rows; i++) {
      sprintf(s, "%" PRId32, values[i]);
      stringValue[p][i+startRow] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i+startRow], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new pages */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = (char*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      charValue[p][i] = (char)0;
    }
    for (i=0; i<rows; i++) {
      charValue[p][i+startRow] = (char)(values[i]);
    }
    break;
  default:
    return(4);
  }
  if (newPage) {
    pageCount = page;
  }
  if (rowCount[p] < rowsNeeded) {
    rowCount[p] = rowsNeeded;
  }
  return(0);
}

int32_t SDDSColumn::setValues(uint32_t *values, uint32_t page, uint32_t startRow, uint32_t rows) {
  uint32_t rowsNeeded, i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (startRow == 0)
    return(2);
  startRow--;
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  rowsNeeded = rows + startRow;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new pages */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = (int16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int16Value[p][i] = (int16_t)0;
    }
    for (i=0; i<rows; i++) {
      int16Value[p][i+startRow] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new pages */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = (uint16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint16Value[p][i] = (uint16_t)0;
    }
    for (i=0; i<rows; i++) {
      uint16Value[p][i+startRow] = (uint16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new pages */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = (int32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int32Value[p][i] = (int32_t)0;
    }
    for (i=0; i<rows; i++) {
      int32Value[p][i+startRow] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new pages */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = (uint32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    if (startRow > 0) {
      for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
        uint32Value[p][i] = (uint32_t)0;
      }
      for (i=0; i<rows; i++) {
        uint32Value[p][i+startRow] = (uint32_t)(values[i]);
      }
    } else {
      memcpy(uint32Value[p], values, rows * 4);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new pages */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = (float*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real32Value[p][i] = (float)0;
    }
    for (i=0; i<rows; i++) {
      real32Value[p][i+startRow] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new pages */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = (double*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real64Value[p][i] = (double)0;
    }
    for (i=0; i<rows; i++) {
      real64Value[p][i+startRow] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = (char**)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put NULL in skiped rows */
      stringValue[p][i] = (char*)NULL;
    }
    for (i=0; i<rows; i++) {
      sprintf(s, "%" PRIu32, values[i]);
      stringValue[p][i+startRow] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i+startRow], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new pages */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = (char*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      charValue[p][i] = (char)0;
    }
    for (i=0; i<rows; i++) {
      charValue[p][i+startRow] = (char)(values[i]);
    }
    break;
  default:
    return(4);
  }
  if (newPage) {
    pageCount = page;
  }
  if (rowCount[p] < rowsNeeded) {
    rowCount[p] = rowsNeeded;
  }
  return(0);
}

int32_t SDDSColumn::setValues(float *values, uint32_t page, uint32_t startRow, uint32_t rows) {
  uint32_t rowsNeeded, i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (startRow == 0)
    return(2);
  startRow--;
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  rowsNeeded = rows + startRow;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new pages */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = (int16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int16Value[p][i] = (int16_t)0;
    }
    for (i=0; i<rows; i++) {
      int16Value[p][i+startRow] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new pages */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = (uint16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint16Value[p][i] = (uint16_t)0;
    }
    for (i=0; i<rows; i++) {
      uint16Value[p][i+startRow] = (uint16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new pages */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = (int32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int32Value[p][i] = (int32_t)0;
    }
    for (i=0; i<rows; i++) {
      int32Value[p][i+startRow] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new pages */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = (uint32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint32Value[p][i] = (uint32_t)0;
    }
    for (i=0; i<rows; i++) {
      uint32Value[p][i+startRow] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new pages */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = (float*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    if (startRow > 0) {
      for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
        real32Value[p][i] = (float)0;
      }
      for (i=0; i<rows; i++) {
        real32Value[p][i+startRow] = (float)(values[i]);
      }
    } else {
      memcpy(real32Value[p], values, rows * 4);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new pages */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = (double*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real64Value[p][i] = (double)0;
    }
    for (i=0; i<rows; i++) {
      real64Value[p][i+startRow] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = (char**)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put NULL in skiped rows */
      stringValue[p][i] = (char*)NULL;
    }
    for (i=0; i<rows; i++) {
      sprintf(s, "%lf", values[i]);
      stringValue[p][i+startRow] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i+startRow], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new pages */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = (char*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      charValue[p][i] = (char)0;
    }
    for (i=0; i<rows; i++) {
      charValue[p][i+startRow] = (char)(values[i]);
    }
    break;
  default:
    return(4);
  }
  if (newPage) {
    pageCount = page;
  }
  if (rowCount[p] < rowsNeeded) {
    rowCount[p] = rowsNeeded;
  }
  return(0);
}

int32_t SDDSColumn::setValues(double *values, uint32_t page, uint32_t startRow, uint32_t rows) {
  uint32_t rowsNeeded, i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (startRow == 0)
    return(2);
  startRow--;
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  rowsNeeded = rows + startRow;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new pages */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = (int16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int16Value[p][i] = (int16_t)0;
    }
    for (i=0; i<rows; i++) {
      int16Value[p][i+startRow] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new pages */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = (uint16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint16Value[p][i] = (uint16_t)0;
    }
    for (i=0; i<rows; i++) {
      uint16Value[p][i+startRow] = (uint16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new pages */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = (int32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int32Value[p][i] = (int32_t)0;
    }
    for (i=0; i<rows; i++) {
      int32Value[p][i+startRow] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new pages */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = (uint32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint32Value[p][i] = (uint32_t)0;
    }
    for (i=0; i<rows; i++) {
      uint32Value[p][i+startRow] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new pages */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = (float*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real32Value[p][i] = (float)0;
    }
    for (i=0; i<rows; i++) {
      real32Value[p][i+startRow] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new pages */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = (double*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    if (startRow > 0) {
      for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
        real64Value[p][i] = (double)0;
      }
      for (i=0; i<rows; i++) {
        real64Value[p][i+startRow] = (double)(values[i]);
      }
    } else {
      memcpy(real64Value[p], values, rows * 8);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = (char**)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put NULL in skiped rows */
      stringValue[p][i] = (char*)NULL;
    }
    for (i=0; i<rows; i++) {
      sprintf(s, "%lf", values[i]);
      stringValue[p][i+startRow] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i+startRow], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new pages */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = (char*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      charValue[p][i] = (char)0;
    }
    for (i=0; i<rows; i++) {
      charValue[p][i+startRow] = (char)(values[i]);
    }
    break;
  default:
    return(4);
  }
  if (newPage) {
    pageCount = page;
  }
  if (rowCount[p] < rowsNeeded) {
    rowCount[p] = rowsNeeded;
  }
  return(0);
}

int32_t SDDSColumn::setValues(char **values, uint32_t page, uint32_t startRow, uint32_t rows) {
  int32_t returnVal=0;
  uint32_t rowsNeeded, i, n=0, p;
  bool newPage=false;
  int64_t *iValue=NULL;
  double *dValue=NULL;
  if (page == 0)
    return(1);
  if (startRow == 0)
    return(2);

  if (SDDS_INTEGER_TYPE(columnType)) {
    iValue = (int64_t*)SDDS_malloc(sizeof(int64_t) * rows);
    for (i=0; i<rows; i++) {
      iValue[i] = strtoll(values[i], (char**)NULL, 10);
      if ((iValue[i] == LLONG_MIN) || (iValue[i] == LLONG_MAX)) {
	/*unable to convert given string to integer*/
	returnVal=3;
	iValue[i]=0;
      }
    }
  } else if (SDDS_FLOATING_TYPE(columnType)) {
    dValue = (double*)SDDS_malloc(sizeof(double) * rows);
    for (i=0; i<rows; i++) {
      dValue[i] = strtod(values[i], (char**)NULL);
      if ((dValue[i] == HUGE_VAL) || (dValue[i] == -HUGE_VAL)) {
	/*unable to convert given string to floating point*/
	returnVal=3;
	dValue[i]=0;
      }
    }
  }

  startRow--;
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  rowsNeeded = rows + startRow;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new pages */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = (int16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int16Value[p][i] = (int16_t)0;
    }
    for (i=0; i<rows; i++) {
      int16Value[p][i+startRow] = (int16_t)(iValue[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new pages */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = (uint16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint16Value[p][i] = (uint16_t)0;
    }
    for (i=0; i<rows; i++) {
      uint16Value[p][i+startRow] = (uint16_t)(iValue[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new pages */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = (int32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int32Value[p][i] = (int32_t)0;
    }
    for (i=0; i<rows; i++) {
      int32Value[p][i+startRow] = (int32_t)(iValue[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new pages */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = (uint32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint32Value[p][i] = (uint32_t)0;
    }
    for (i=0; i<rows; i++) {
      uint32Value[p][i+startRow] = (uint32_t)(iValue[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new pages */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = (float*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real32Value[p][i] = (float)0;
    }
    for (i=0; i<rows; i++) {
      real32Value[p][i+startRow] = (float)(dValue[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new pages */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = (double*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real64Value[p][i] = (double)0;
    }
    for (i=0; i<rows; i++) {
      real64Value[p][i+startRow] = (double)(dValue[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = (char**)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put NULL in skiped rows */
      stringValue[p][i] = (char*)NULL;
    }
    for (i=0; i<rows; i++) {
      stringValue[p][i+startRow] = (char*)SDDS_malloc(sizeof(char) * strlen(values[i]) + 1);
      strcpy(stringValue[p][i+startRow], values[i]);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new pages */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = (char*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      charValue[p][i] = (char)0;
    }
    for (i=0; i<rows; i++) {
      charValue[p][i+startRow] = (char)(values[i][0]);
    }
    break;
  default:
    return(4);
  }
  if (newPage) {
    pageCount = page;
  }
  if (rowCount[p] < rowsNeeded) {
    rowCount[p] = rowsNeeded;
  }
  return(returnVal);
}

int32_t SDDSColumn::setValues(char *values, uint32_t page, uint32_t startRow, uint32_t rows) {
  uint32_t rowsNeeded, i, n=0, p;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (startRow == 0)
    return(2);
  startRow--;
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  rowsNeeded = rows + startRow;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new pages */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = (int16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int16Value[p][i] = (int16_t)0;
    }
    for (i=0; i<rows; i++) {
      int16Value[p][i+startRow] = (int16_t)(values[i]);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new pages */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = (uint16_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint16Value[p][i] = (uint16_t)0;
    }
    for (i=0; i<rows; i++) {
      uint16Value[p][i+startRow] = (uint16_t)(values[i]);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new pages */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = (int32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      int32Value[p][i] = (int32_t)0;
    }
    for (i=0; i<rows; i++) {
      int32Value[p][i+startRow] = (int32_t)(values[i]);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new pages */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = (uint32_t*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      uint32Value[p][i] = (uint32_t)0;
    }
    for (i=0; i<rows; i++) {
      uint32Value[p][i+startRow] = (uint32_t)(values[i]);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new pages */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = (float*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real32Value[p][i] = (float)0;
    }
    for (i=0; i<rows; i++) {
      real32Value[p][i+startRow] = (float)(values[i]);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new pages */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = (double*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
      real64Value[p][i] = (double)0;
    }
    for (i=0; i<rows; i++) {
      real64Value[p][i+startRow] = (double)(values[i]);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = (char**)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    for (i=rowCount[p] ; i<startRow; i++) { /* put NULL in skiped rows */
      stringValue[p][i] = (char*)NULL;
    }
    for (i=0; i<rows; i++) {
      sprintf(s, "%" PRId32, values[i]);
      stringValue[p][i+startRow] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(stringValue[p][i+startRow], s);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new pages */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = (char*)NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    if (startRow > 0) {
      for (i=rowCount[p] ; i<startRow; i++) { /* put zeros in skiped rows */
        charValue[p][i] = (char)0;
      }
      for (i=0; i<rows; i++) {
        charValue[p][i+startRow] = (char)(values[i]);
      }
    } else {
      memcpy(charValue[p], values, rows);
    }
    break;
  default:
    return(4);
  }
  if (newPage) {
    pageCount = page;
  }
  if (rowCount[p] < rowsNeeded) {
    rowCount[p] = rowsNeeded;
  }
  return(0);
}

/***********************************************************************************************
 *                        readAsciiValue                                                       *
 *                                                                                             *
 * C++ Arguments: char *value, uint32_t page                                                   *
 *                                                                                             *
 * Results:  0 for success                                                                     *
 *           1 for failure                                                                     *
 ***********************************************************************************************/
int32_t SDDSColumn::readAsciiValue(char **bigBuffer, int32_t *bigBufferSize, uint32_t page) {
  int32_t returnVal=0, length;
  uint32_t rowsNeeded, startRow, i, n=0, p;
  int64_t iValue=0;
  double dValue=0;
  bool newPage=false;
  static char *buffer=NULL;
  static int32_t bufferSize=0;
  if (page == 0) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--invalid page number (SDDSColumn::readAsciiValue)");
    return(1);
  }
  startRow = getRowCount(page);
  if (startRow == UINT_MAX) {
    startRow = 0;
  }
  if (bufferSize==0) {
    bufferSize=SDDS_MAXLINE;
    buffer = (char*)SDDS_malloc(sizeof(*buffer)*(bufferSize));
  }
  length = *bigBufferSize;
  if (length < columnFieldLength) {
    length = columnFieldLength;
  }
  if (bufferSize <= length) {
    bufferSize = 2 * length;
    buffer = (char*)SDDS_realloc(buffer, sizeof(*buffer)*(bufferSize));
  }
  if (columnType != SDDS_STRING) {
    if (columnFieldLength) {
      if (columnFieldLength > *bigBufferSize) {
        strcpy(buffer, *bigBuffer);
        **bigBuffer = 0;
        *bigBufferSize = 0;
      } else {
        strncpy(buffer, *bigBuffer, columnFieldLength);
        buffer[columnFieldLength] = 0;
        *bigBuffer += columnFieldLength;
        *bigBufferSize -= columnFieldLength;
      }
    }
  }
  if (SDDS_INTEGER_TYPE(columnType)) {
    if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column (SDDSColumn::readAsciiValue)");
      returnVal=1;
      iValue=0;
    } else {
      iValue = strtoll(buffer, (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--not a valid integer (SDDSColumn::readAsciiValue)");
	returnVal=1;
	iValue=0;
      }
    }
  } else if (SDDS_FLOATING_TYPE(columnType)) {
    if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column (SDDSColumn::readAsciiValue)");
      returnVal=1;
      dValue=0;
    } else {
      dValue = strtod(buffer, (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
        ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--not a valid number (SDDSColumn::readAsciiValue)");
        returnVal=1;
        dValue=0;
      }
    }
  }
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  rowsNeeded = 1 + startRow;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new page */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    int16Value[p][startRow] = (int16_t)iValue;
    break;
  case SDDS_UINT16:
    if (newPage) { /* new page */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    uint16Value[p][startRow] = (uint16_t)iValue;
    break;
  case SDDS_INT32:
    if (newPage) { /* new page */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    int32Value[p][startRow] = (int32_t)iValue;
    break;
  case SDDS_UINT32:
    if (newPage) { /* new page */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    uint32Value[p][startRow] = (uint32_t)iValue;
    break;
  case SDDS_REAL32:
    if (newPage) { /* new pages */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    real32Value[p][startRow] = (float)dValue;
    break;
  case SDDS_REAL64:
    if (newPage) { /* new pages */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    real64Value[p][startRow] = (double)dValue;
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    length = strlen(*bigBuffer);
    if (length > 0) {
      if ((*bigBuffer)[length-1] == '\r') {
        (*bigBuffer)[length-1] = 0;
      }
    }
    if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column (SDDSColumn::readAsciiValue)");
      returnVal=1;
      stringValue[p][startRow] = (char*)SDDS_malloc(sizeof(char) * 4);
      strcpy(stringValue[p][startRow], "\"\"");
    } else {
      SDDS_interpretEscapes(buffer);
      stringValue[p][startRow] = (char*)SDDS_malloc(sizeof(char) * strlen(buffer) + 1);
      strcpy(stringValue[p][startRow], buffer);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new pages */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column (SDDSColumn::readAsciiValue)");
      returnVal=1;
      charValue[p][startRow] = '-';
    } else {
      charValue[p][startRow] = (char)(buffer[0]);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--invalid column type (SDDSColumn::readAsciiValue)");
    return(1);
  }
  if (newPage) {
    pageCount = page;
    rowCount[p] = 1;
  } else {
    rowCount[p]++;
  }
  return(returnVal);
}

/***********************************************************************************************
 *                        getInteralValues                                                     *
 *                                                                                             *
 * C++ Arguments: uint32_t page                                                                 *
 *                                                                                             *
 * Results:  pointer to values                                                                 *
 *           NULL for column value undefined                                                   *
 ***********************************************************************************************/
void* SDDSColumn::getInternalValues(uint32_t page) {
  uint32_t p;
  if ((page == 0) || (pageCount < page)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Invalid page (SDDSColumn::getInternalValues)");
    return(NULL);
  }
  p = page - 1;
  if (rowCount[p] == 0) {
    return(NULL);
  }
  switch (columnType) {
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
    ((SDDSFile*)sddsfile)->setError((char*)"Invalid data type (SDDSColumn::getInternalValues)");
    return(NULL);
  }
}

/***********************************************************************************************
 *                        getValuesInInt32                                                     *
 *                                                                                             *
 * C++ Arguments: uint32_t page, int32_t *errorcode                                             *
 *                                                                                             *
 * Results:  pointer to values and errorcode=0 for success                                     *
 *           NULL and errorcode=1 for column value undefined                                   *
 *           NULL and errorcode=2 for the case where the value cannot be                       *
 *                                    converted to the proper type                             *
 *           NULL and errorcode=3 for invalid column type                                      *
 ***********************************************************************************************/
int32_t* SDDSColumn::getValuesInInt32(uint32_t page, int32_t *errorcode) {
  int64_t iValue;
  int32_t* rValues;
  uint32_t p;
  uint32_t i;
  if ((page == 0) || (pageCount < page)) {
    *errorcode = 1;
    return((int32_t*)NULL);
  }
  *errorcode = 0;
  p = page - 1;
  if (rowCount[p] == 0) {
    return((int32_t*)NULL);
  }
  rValues = (int32_t*)SDDS_malloc(sizeof(int32_t) * rowCount[p]);
  switch (columnType) {
  case SDDS_INT16:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (int32_t)int16Value[p][i];
    return((int32_t*)rValues);
  case SDDS_UINT16:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (int32_t)uint16Value[p][i];
    return((int32_t*)rValues);
  case SDDS_INT32:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (int32_t)int32Value[p][i];
    return((int32_t*)rValues);
  case SDDS_UINT32:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (int32_t)uint32Value[p][i];
    return((int32_t*)rValues);
  case SDDS_REAL32:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (int32_t)real32Value[p][i];
    return((int32_t*)rValues);
  case SDDS_REAL64:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (int32_t)real64Value[p][i];
    return((int32_t*)rValues);
  case SDDS_STRING:
    for (i=0; i<rowCount[p]; i++) {
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
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (int32_t)charValue[p][i];
    return((int32_t*)rValues);
  default:
    *errorcode = 3;
    return((int32_t*)NULL);
  }
}

/***********************************************************************************************
 *                        getValuesInUInt32                                                    *
 *                                                                                             *
 * C++ Arguments: uint32_t page, int32_t *errorcode                                             *
 *                                                                                             *
 * Results:  pointer to values and errorcode=0 for success                                     *
 *           NULL and errorcode=1 for column value undefined                                   *
 *           NULL and errorcode=2 for the case where the value cannot be                       *
 *                                    converted to the proper type                             *
 *           NULL and errorcode=3 for invalid column type                                      *
 ***********************************************************************************************/
uint32_t* SDDSColumn::getValuesInUInt32(uint32_t page, int32_t *errorcode) {
  int64_t iValue;
  uint32_t* rValues;
  uint32_t i, p;
  if ((page == 0) || (pageCount < page)) {
    *errorcode = 1;
    return((uint32_t*)NULL);
  }
  *errorcode = 0;
  p = page - 1;
  if (rowCount[p] == 0) {
    return((uint32_t*)NULL);
  }
  rValues = (uint32_t*)SDDS_malloc(sizeof(uint32_t) * rowCount[p]);
  switch (columnType) {
  case SDDS_INT16:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (uint32_t)int16Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_UINT16:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (uint32_t)uint16Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_INT32:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (uint32_t)int32Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_UINT32:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (uint32_t)uint32Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_REAL32:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (uint32_t)real32Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_REAL64:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (uint32_t)real64Value[p][i];
    return((uint32_t*)rValues);
  case SDDS_STRING:
    for (i=0; i<rowCount[p]; i++) {
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
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (uint32_t)charValue[p][i];
    return((uint32_t*)rValues);
  default:
    *errorcode = 3;
    return((uint32_t*)NULL);
  }
}

/***********************************************************************************************
 *                        getValuesInDouble                                                    *
 *                                                                                             *
 * C++ Arguments: uint32_t page, int32_t *errorcode                                             *
 *                                                                                             *
 * Results:  pointer to values and errorcode=0 for success                                     *
 *           NULL and errorcode=1 for column value undefined                                   *
 *           NULL and errorcode=2 for the case where the value cannot be                       *
 *                                    converted to the proper type                             *
 *           NULL and errorcode=3 for invalid column type                                      *
 ***********************************************************************************************/
double* SDDSColumn::getValuesInDouble(uint32_t page, int32_t *errorcode) {
  double dValue;
  double* rValues;
  uint32_t i, p;
  if ((page == 0) || (pageCount < page)) {
    *errorcode = 1;
    return((double*)NULL);
  }
  *errorcode = 0;
  p = page - 1;
  if (rowCount[p] == 0) {
    return((double*)NULL);
  }
  rValues = (double*)SDDS_malloc(sizeof(double) * rowCount[p]);
  switch (columnType) {
  case SDDS_INT16:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (double)int16Value[p][i];
    return((double*)rValues);
  case SDDS_UINT16:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (double)uint16Value[p][i];
    return((double*)rValues);
  case SDDS_INT32:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (double)int32Value[p][i];
    return((double*)rValues);
  case SDDS_UINT32:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (double)uint32Value[p][i];
    return((double*)rValues);
  case SDDS_REAL32:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (double)real32Value[p][i];
    return((double*)rValues);
  case SDDS_REAL64:
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (double)real64Value[p][i];
    return((double*)rValues);
  case SDDS_STRING:
    for (i=0; i<rowCount[p]; i++) {
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
    for (i=0; i<rowCount[p]; i++)
      rValues[i] = (double)charValue[p][i];
    return((double*)rValues);
  default:
    *errorcode = 3;
    return((double*)NULL);
  }
}

/***********************************************************************************************
 *                        getValuesInString                                                    *
 *                                                                                             *
 * C++ Arguments: uint32_t page, int32_t *errorcode                                             *
 *                                                                                             *
 * Results:  pointer to values and errorcode=0 for success                                     *
 *           NULL and errorcode=1 for column value undefined                                   *
 *           NULL and errorcode=2 for the case where the value cannot be                       *
 *                                    converted to the proper type                             *
 *           NULL and errorcode=3 for invalid column type                                      *
 ***********************************************************************************************/
char** SDDSColumn::getValuesInString(uint32_t page, int32_t *errorcode) {
  char** rValues;
  uint32_t i, p;
  char s[SDDS_MAXLINE];
  if ((page == 0) || (pageCount < page)) {
    *errorcode = 1;
    return((char**)NULL);
  }
  *errorcode = 0;
  p = page - 1;
  if (rowCount[p] == 0) {
    return((char**)NULL);
  }
  rValues = (char**)SDDS_malloc(sizeof(char*) * rowCount[p]);
  switch (columnType) {
  case SDDS_INT16:
    for (i=0; i<rowCount[p]; i++) {
      sprintf(s, "%" PRId16, int16Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_UINT16:
    for (i=0; i<rowCount[p]; i++) {
      sprintf(s, "%" PRIu16, uint16Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_INT32:
    for (i=0; i<rowCount[p]; i++) {
      sprintf(s, "%" PRId32, int32Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_UINT32:
    for (i=0; i<rowCount[p]; i++) {
      sprintf(s, "%" PRIu32, uint32Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_REAL32:
    for (i=0; i<rowCount[p]; i++) {
      sprintf(s, "%f", real32Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_REAL64:
    for (i=0; i<rowCount[p]; i++) {
      sprintf(s, "%lf", real64Value[p][i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_STRING:
    for (i=0; i<rowCount[p]; i++) {
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(stringValue[p][i]) + 1);
      strcpy(rValues[i], stringValue[p][i]);
    }
    return((char**)rValues);
  case SDDS_CHARACTER:
    for (i=0; i<rowCount[p]; i++) {
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

/***********************************************************************************************
 *                        writeDefinition                                                      *
 *                                                                                             *
 * C++ Arguments: FILE *fp                                                                     *
 *                voidp *gzfp                                                                  *
 *                                                                                             *
 * Results:  0 on success                                                                      *
 *           1 on failure                                                                      *
 ***********************************************************************************************/
int32_t SDDSColumn::writeDefinition(FILE *fp) {
  if ((columnType <= 0) || (columnType > SDDS_NUM_TYPES)) {
    return(1);
  }
  if (fp == NULL) {
    return(1);
  }
  fputs("&column ", fp);
  if (SDDS_printNamelistField(fp, (char*)"name", columnName)) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"symbol", SDDS_blankToNull(columnSymbol))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"units", SDDS_blankToNull(columnUnits))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"description", SDDS_blankToNull(columnDescription))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"format_string", SDDS_blankToNull(columnFormatString))) {
    return(1);
  }
  if (((SDDSFile*)sddsfile)->layoutVersion == 1) {
    if (SDDS_printNamelistField(fp, (char*)"type", SDDS_getVer1TypeName(columnType))) {
      return(1);
    }
  } else if (((SDDSFile*)sddsfile)->layoutVersion == 2) {
    if (SDDS_printNamelistField(fp, (char*)"type", SDDS_getVer2TypeName(columnType))) {
      return(1);
    }
  } else {
    if (SDDS_printNamelistField(fp, (char*)"type", SDDS_getTypeName(columnType))) {
      return(1);
    }
  }
  fputs("&end\n", fp);
  return(0);
}

int32_t SDDSColumn::writeDefinition(voidp *gzfp) {
  if ((columnType <= 0) || (columnType > SDDS_NUM_TYPES)) {
    return(1);
  }
  if (gzfp == NULL) {
    return(1);
  }
  gzputs(gzfp, "&column ");
  if (SDDS_printNamelistField(gzfp, (char*)"name", columnName)) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"symbol", SDDS_blankToNull(columnSymbol))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"units", SDDS_blankToNull(columnUnits))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"description", SDDS_blankToNull(columnDescription))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"format_string", SDDS_blankToNull(columnFormatString))) {
    return(1);
  }
  if (((SDDSFile*)sddsfile)->layoutVersion == 1) {
    if (SDDS_printNamelistField(gzfp, (char*)"type", SDDS_getVer1TypeName(columnType))) {
      return(1);
    }
  } else if (((SDDSFile*)sddsfile)->layoutVersion == 2) {
    if (SDDS_printNamelistField(gzfp, (char*)"type", SDDS_getVer2TypeName(columnType))) {
      return(1);
    }
  } else {
    if (SDDS_printNamelistField(gzfp, (char*)"type", SDDS_getTypeName(columnType))) {
      return(1);
    }
  }
  gzputs(gzfp, "&end\n");
  return(0);
}

/***********************************************************************************************
 *                        writeAsciiValue                                                      *
 *                                                                                             *
 * C++ Arguments: FILE *fp, uint32_t page, uint32_t row                                          *
 * C++ Arguments: voidp *gzfp, uint32_t page, uint32_t row                                       *
 *                                                                                             *
 * Results:  0 on success                                                                      *
 *           1 on failure                                                                      *
 ***********************************************************************************************/
int32_t SDDSColumn::writeAsciiValue(FILE *fp, uint32_t page, uint32_t row) {
  char c;
  char *s;
  int32_t hasWhitespace;

  if ((page == 0) || (page > pageCount) || (row == 0) || (row > rowCount[page-1])) {
    return(1);
  }
  page--;
  row--;
  switch (columnType) {
  case SDDS_INT16:
    fprintf(fp, "%" PRId16 " ", int16Value[page][row]);
    break;
  case SDDS_UINT16:
    fprintf(fp, "%" PRIu16 " ", uint16Value[page][row]);
    break;
  case SDDS_INT32:
    fprintf(fp, "%" PRId32 " ", int32Value[page][row]);
    break;
  case SDDS_UINT32:
    fprintf(fp, "%" PRIu32 " ", uint32Value[page][row]);
    break;
  case SDDS_REAL32:
    fprintf(fp, "%.*g ", FLT_DIG, real32Value[page][row]);
    break;
  case SDDS_REAL64:
    fprintf(fp, "%.*g ", DBL_DIG, real64Value[page][row]);
    break;
  case SDDS_STRING:
    s = stringValue[page][row];
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
    break;
  case SDDS_CHARACTER:
    c = charValue[page][row];
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
    break;
  default:
    return(1);
  }
  return(0);
}

int32_t SDDSColumn::writeAsciiValue(voidp *gzfp, uint32_t page, uint32_t row) {
  char c;
  char *s;
  int32_t hasWhitespace;

  if ((page == 0) || (page > pageCount) || (row == 0) || (row > rowCount[page-1])) {
    return(1);
  }
  page--;
  row--;
  switch (columnType) {
  case SDDS_INT16:
    gzprintf(gzfp, "%" PRId16 " ", int16Value[page][row]);
    break;
  case SDDS_UINT16:
    gzprintf(gzfp, "%" PRIu16 " ", uint16Value[page][row]);
    break;
  case SDDS_INT32:
    gzprintf(gzfp, "%" PRId32 " ", int32Value[page][row]);
    break;
  case SDDS_UINT32:
    gzprintf(gzfp, "%" PRIu32 " ", uint32Value[page][row]);
    break;
  case SDDS_REAL32:
    gzprintf(gzfp, "%.*g ", FLT_DIG, real32Value[page][row]);
    break;
  case SDDS_REAL64:
    gzprintf(gzfp, "%.*g ", DBL_DIG, real64Value[page][row]);
    break;
  case SDDS_STRING:
    s = stringValue[page][row];
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
    break;
  case SDDS_CHARACTER:
    c = charValue[page][row];
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
    break;
  default:
    return(1);
  }
  return(0);
}

/***********************************************************************************************
 *                        writeAsciiValues                                                     *
 *                                                                                             *
 * C++ Arguments: FILE *fp, uint32_t page                                                       *
 * C++ Arguments: voidp *gzfp, uint32_t page                                                    *
 *                                                                                             *
 * Note: column-major-order of ascii files is not implemented yet                              *
 *                                                                                             *
 * Results:  0 on success                                                                      *
 *           1 on failure                                                                      *
 ***********************************************************************************************/
/*
int32_t SDDSColumn::writeAsciiValues(FILE *fp, uint32_t page) {
  char c;
  char *s;
  int32_t hasWhitespace;
  uint32_t i;

  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (columnType) {
  case SDDS_INT16:
    for (i=0; i<rowCount[page]; i++) {
      fprintf(fp, "%" PRId16 " ", int16Value[page][i]);
    }
    break;
  case SDDS_UINT16:
    for (i=0; i<rowCount[page]; i++) {
      fprintf(fp, "%" PRIu16 " ", uint16Value[page][i]);
    }
    break;
  case SDDS_INT32:
    for (i=0; i<rowCount[page]; i++) {
      fprintf(fp, "%" PRId32 " ", int32Value[page][i]);
    }
    break;
  case SDDS_UINT32:
    for (i=0; i<rowCount[page]; i++) {
      fprintf(fp, "%" PRIu32 " ", uint32Value[page][i]);
    }
    break;
  case SDDS_REAL32:
    for (i=0; i<rowCount[page]; i++) {
      fprintf(fp, "%.*g ", FLT_DIG, real32Value[page][i]);
    }
    break;
  case SDDS_REAL64:
    for (i=0; i<rowCount[page]; i++) {
      fprintf(fp, "%.*g ", DBL_DIG, real64Value[page][i]);
    }
    break;
  case SDDS_STRING:
    for (i=0; i<rowCount[page]; i++) {
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
          fputc(' ', fp);
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
    for (i=0; i<rowCount[page]; i++) {
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
  return(0);
}

int32_t SDDSColumn::writeAsciiValues(voidp *gzfp, uint32_t page) {
  char c;
  char *s;
  int32_t hasWhitespace;
  uint32_t i;

  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (columnType) {
  case SDDS_INT16:
    for (i=0; i<rowCount[page]; i++) {
      gzprintf(gzfp, "%" PRId16 " ", int16Value[page][i]);
    }
    break;
  case SDDS_UINT16:
    for (i=0; i<rowCount[page]; i++) {
      gzprintf(gzfp, "%" PRIu16 " ", uint16Value[page][i]);
    }
    break;
  case SDDS_INT32:
    for (i=0; i<rowCount[page]; i++) {
      gzprintf(gzfp, "%" PRId32 " ", int32Value[page][i]);
    }
    break;
  case SDDS_UINT32:
    for (i=0; i<rowCount[page]; i++) {
      gzprintf(gzfp, "%" PRIu32 " ", uint32Value[page][i]);
    }
    break;
  case SDDS_REAL32:
    for (i=0; i<rowCount[page]; i++) {
      gzprintf(gzfp, "%.*g ", FLT_DIG, real32Value[page][i]);
    }
    break;
  case SDDS_REAL64:
    for (i=0; i<rowCount[page]; i++) {
      gzprintf(gzfp, "%.*g ", DBL_DIG, real64Value[page][i]);
    }
    break;
  case SDDS_STRING:
    for (i=0; i<rowCount[page]; i++) {
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
          gzputc(gzfp, ' ');
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
    for (i=0; i<rowCount[page]; i++) {
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
  return(0);
}
*/

/***********************************************************************************************
 *                        writeBinaryValue                                                     *
 *                                                                                             *
 * C++ Arguments: FILE *fp, uint32_t page, uint32_t row                                         *
 * C++ Arguments: voidp *gzfp, uint32_t page, uint32_t row                                      *
 *                                                                                             *
 * Results:  0 on success                                                                      *
 *           1 on failure                                                                      *
 ***********************************************************************************************/
int32_t SDDSColumn::writeBinaryValue(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t row, bool nonNativeEndian) {
  switch (columnType) {
  case SDDS_INT16:
    if (SDDS_bufferedWrite(&(int16Value[page][row]), 2, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_UINT16:
    if (SDDS_bufferedWrite(&(uint16Value[page][row]), 2, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_INT32:
    if (SDDS_bufferedWrite(&(int32Value[page][row]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_UINT32:
    if (SDDS_bufferedWrite(&(uint32Value[page][row]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_REAL32:
    if (SDDS_bufferedWrite(&(real32Value[page][row]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_REAL64:
    if (SDDS_bufferedWrite(&(real64Value[page][row]), 8, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_STRING:
    if (SDDS_writeBinaryString(stringValue[page][row], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing string (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_CHARACTER:
    if (SDDS_bufferedWrite(&(charValue[page][row]), 1, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  }
  ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--unknown type (SDDSColumn::writeBinaryValue)");
  return(1);
}

int32_t SDDSColumn::writeBinaryValue(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t row, bool nonNativeEndian) {
  switch (columnType) {
  case SDDS_INT16:
    if (SDDS_bufferedWrite(&(int16Value[page][row]), 2, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_UINT16:
    if (SDDS_bufferedWrite(&(uint16Value[page][row]), 2, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_INT32:
    if (SDDS_bufferedWrite(&(int32Value[page][row]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_UINT32:
    if (SDDS_bufferedWrite(&(uint32Value[page][row]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_REAL32:
    if (SDDS_bufferedWrite(&(real32Value[page][row]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_REAL64:
    if (SDDS_bufferedWrite(&(real64Value[page][row]), 8, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_STRING:
    if (SDDS_writeBinaryString(stringValue[page][row], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing string (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  case SDDS_CHARACTER:
    if (SDDS_bufferedWrite(&(charValue[page][row]), 1, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValue)");
      return(1);
    }
    return(0);
  }
  ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--unknown type (SDDSColumn::writeBinaryValue)");
  return(1);
}

/***********************************************************************************************
 *                        writeBinaryValues                                                    *
 *                                                                                             *
 * C++ Arguments: FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian       *
 * C++ Arguments: voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian    *
 *                                                                                             *
 * Results:  0 on success                                                                      *
 *           1 on failure                                                                      *
 ***********************************************************************************************/
int32_t SDDSColumn::writeBinaryValues(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t row;
  switch (columnType) {
  case SDDS_INT16:
    if (SDDS_bufferedWrite(&(int16Value[page][0]), 2 * rowCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_UINT16:
    if (SDDS_bufferedWrite(&(uint16Value[page][0]), 2 * rowCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_INT32:
    if (SDDS_bufferedWrite(&(int32Value[page][0]), 4 * rowCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_UINT32:
    if (SDDS_bufferedWrite(&(uint32Value[page][0]), 4 * rowCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_REAL32:
    if (SDDS_bufferedWrite(&(real32Value[page][0]), 4 * rowCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_REAL64:
    if (SDDS_bufferedWrite(&(real64Value[page][0]), 8 * rowCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_STRING:
    for (row=0; row<rowCount[page]; row++) {
      if (SDDS_writeBinaryString(stringValue[page][row], fp, fBuffer, sddsfile)) {
        ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing string (SDDSColumn::writeBinaryValues)");
        return(1);
      }
    }
    return(0);
  case SDDS_CHARACTER:
    if (SDDS_bufferedWrite(&(charValue[page][0]), rowCount[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  }
  ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--unknown type (SDDSColumn::writeBinaryValues)");
  return(1);
}

int32_t SDDSColumn::writeBinaryValues(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t row;
  switch (columnType) {
  case SDDS_INT16:
    if (SDDS_bufferedWrite(&(int16Value[page][0]), 2 * rowCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_UINT16:
    if (SDDS_bufferedWrite(&(uint16Value[page][0]), 2 * rowCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_INT32:
    if (SDDS_bufferedWrite(&(int32Value[page][0]), 4 * rowCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_UINT32:
    if (SDDS_bufferedWrite(&(uint32Value[page][0]), 4 * rowCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_REAL32:
    if (SDDS_bufferedWrite(&(real32Value[page][0]), 4 * rowCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_REAL64:
    if (SDDS_bufferedWrite(&(real64Value[page][0]), 8 * rowCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  case SDDS_STRING:
    for (row=0; row<rowCount[page]; row++) {
      if (SDDS_writeBinaryString(stringValue[page][row], gzfp, fBuffer, sddsfile)) {
        ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing string (SDDSColumn::writeBinaryValues)");
        return(1);
      }
    }
    return(0);
  case SDDS_CHARACTER:
    if (SDDS_bufferedWrite(&(charValue[page][0]), rowCount[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--failure writing value (SDDSColumn::writeBinaryValues)");
      return(1);
    }
    return(0);
  }
  ((SDDSFile*)sddsfile)->setError((char*)"Unable to write column--unknown type (SDDSColumn::writeBinaryValues)");
  return(1);
}

/***********************************************************************************************
 *                        readBinaryValue                                                      *
 *                                                                                             *
 * C++ Arguments: FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian       *
 * C++ Arguments: voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian    *
 *                                                                                             *
 * Results:  0 on success                                                                      *
 *           1 on failure                                                                      *
 ***********************************************************************************************/
int32_t SDDSColumn::readBinaryValue(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t i, n=0, p, rowsNeeded;
  bool newPage=false;

  if (page == 0) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--invalid page number (SDDSColumn::readBinaryValue)");
    return(1);
  }
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  rowsNeeded = 1 + rowCount[p];
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new page */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(int16Value[p][rowCount[p]]), 2, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new page */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(uint16Value[p][rowCount[p]]), 2, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new page */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(int32Value[p][rowCount[p]]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new page */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(uint32Value[p][rowCount[p]]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new page */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(real32Value[p][rowCount[p]]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new page */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(real64Value[p][rowCount[p]]), 8, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    stringValue[p][rowCount[p]] = NULL;
    if (SDDS_readBinaryString(&(stringValue[p][rowCount[p]]), fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading string (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new page */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(charValue[p][rowCount[p]]), 1, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--unknown type (SDDSColumn::readBinaryValue)");
    return(1);
  }
  if (newPage) {
    pageCount = page;
    rowCount[p] = 1;
  } else {
    rowCount[p]++;
  }
  return(0);
}

int32_t SDDSColumn::readBinaryValue(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t i, n=0, p, rowsNeeded;
  bool newPage=false;

  if (page == 0) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--invalid page number (SDDSColumn::readBinaryValue)");
    return(1);
  }
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  p = page - 1;
  if (newPage) {
    rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
    rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
    for (i=0; i<n; i++) {
      rowCount[p-i] = 0;
      rowsAllocated[p-i] = 0;
    }
  }
  rowsNeeded = 1 + rowCount[p];
  switch (columnType) {
  case SDDS_INT16:
    if (newPage) { /* new page */
      int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
      for (i=0; i<n; i++) {
	int16Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(int16Value[p][rowCount[p]]), 2, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    if (newPage) { /* new page */
      uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
      for (i=0; i<n; i++) {
	uint16Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(uint16Value[p][rowCount[p]]), 2, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_INT32:
    if (newPage) { /* new page */
      int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
      for (i=0; i<n; i++) {
	int32Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(int32Value[p][rowCount[p]]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    if (newPage) { /* new page */
      uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
      for (i=0; i<n; i++) {
	uint32Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(uint32Value[p][rowCount[p]]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    if (newPage) { /* new page */
      real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
      for (i=0; i<n; i++) {
	real32Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(real32Value[p][rowCount[p]]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    if (newPage) { /* new page */
      real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
      for (i=0; i<n; i++) {
	real64Value[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(real64Value[p][rowCount[p]]), 8, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_STRING:
    if (newPage) { /* new pages */
      stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
      for (i=0; i<n; i++) {
	stringValue[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rowsAllocated[p]);
    }
    stringValue[p][rowCount[p]] = NULL;
    if (SDDS_readBinaryString(&(stringValue[p][rowCount[p]]), gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading string (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_CHARACTER:
    if (newPage) { /* new page */
      charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
      for (i=0; i<n; i++) {
	charValue[p-i] = NULL;
      }
    }
    if (rowsNeeded > rowsAllocated[p]) {
      rowsAllocated[p] = rowsNeeded + rowAllocationIncrement;
      charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rowsAllocated[p]);
    }
    if (SDDS_bufferedRead(&(charValue[p][rowCount[p]]), 1, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading value (SDDSColumn::readBinaryValue)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--unknown type (SDDSColumn::readBinaryValue)");
    return(1);
  }
  if (newPage) {
    pageCount = page;
    rowCount[p] = 1;
  } else {
    rowCount[p]++;
  }
  return(0);
}

/*************************
 * readBinaryValues

   C++ Arguments: FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t rows, bool nonNativeEndian
                  voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t rows, bool nonNativeEndian

   Note: used for column-major-order

   Results:  0 on success
             1 on failure
 *************************/
int32_t SDDSColumn::readBinaryValues(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t rows,  bool nonNativeEndian) {
  uint32_t i, n, p;

  if (page == 0) {
    ((SDDSFile*)sddsfile)->setError((char*)"Invalid page number--failure reading values (SDDSColumn::readBinaryValues)");
    return(1);
  }
  if (page > pageCount) {
    n = page - pageCount;
  } else {
    ((SDDSFile*)sddsfile)->setError((char*)"Invalid page number--failure reading values (SDDSColumn::readBinaryValues)");
    return(1);
  }
  p = page - 1;
  rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
  rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
  for (i=0; i<n; i++) {
    rowCount[p-i] = 0;
    rowsAllocated[p-i] = 0;
  }
  if (rows == 0) {
    return(0);
  }
  switch (columnType) {
  case SDDS_INT16:
    int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
    for (i=0; i<n; i++) {
      int16Value[p-i] = NULL;
    }
    int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(int16Value[p][0]), 2 * rows, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
    for (i=0; i<n; i++) {
      uint16Value[p-i] = NULL;
    }
    uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(uint16Value[p][0]), 2 * rows, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_INT32:
    int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
    for (i=0; i<n; i++) {
      int32Value[p-i] = NULL;
    }
    int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(int32Value[p][0]), 4 * rows, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
    for (i=0; i<n; i++) {
      uint32Value[p-i] = NULL;
    }
    uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(uint32Value[p][0]), 4 * rows, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
    for (i=0; i<n; i++) {
      real32Value[p-i] = NULL;
    }
    rowsAllocated[p] = rows;
    real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rows);
    if (SDDS_bufferedRead(&(real32Value[p][0]), 4 * rows, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
    for (i=0; i<n; i++) {
      real64Value[p-i] = NULL;
    }
    real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(real64Value[p][0]), 8 * rows, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_STRING:
    stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
    for (i=0; i<n; i++) {
      stringValue[p-i] = NULL;
    }
    stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rows);
    rowsAllocated[p] = rows;
    for (i=0; i<rows; i++) {
      stringValue[p][i] = NULL;
      if (SDDS_readBinaryString(&(stringValue[p][i]), fp, fBuffer, sddsfile)) {
        ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading strings (SDDSColumn::readBinaryValues)");
        return(1);
      }
    }
    break;
  case SDDS_CHARACTER:
    charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
    for (i=0; i<n; i++) {
      charValue[p-i] = NULL;
    }
    charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(charValue[p][0]), rows, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--unknown type (SDDSColumn::readBinaryValues)");
    return(1);
  }
  pageCount = page;
  rowCount[p] = rows;
  return(0);
}

int32_t SDDSColumn::readBinaryValues(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t rows, bool nonNativeEndian) {
  uint32_t i, n, p;

  if (page == 0) {
    ((SDDSFile*)sddsfile)->setError((char*)"Invalid page number--failure reading values (SDDSColumn::readBinaryValues)");
    return(1);
  }
  if (page > pageCount) {
    n = page - pageCount;
  } else {
    ((SDDSFile*)sddsfile)->setError((char*)"Invalid page number--failure reading values (SDDSColumn::readBinaryValues)");
    return(1);
  }
  p = page - 1;
  rowCount = (uint32_t*)SDDS_realloc(rowCount, sizeof(uint32_t) * page);
  rowsAllocated = (uint32_t*)SDDS_realloc(rowsAllocated, sizeof(uint32_t) * page);
  for (i=0; i<n; i++) {
    rowCount[p-i] = 0;
    rowsAllocated[p-i] = 0;
  }
  if (rows == 0) {
    return(0);
  }
  switch (columnType) {
  case SDDS_INT16:
    int16Value = (int16_t**)SDDS_realloc(int16Value, sizeof(int16_t*) * page);
    for (i=0; i<n; i++) {
      int16Value[p-i] = NULL;
    }
    int16Value[p] = (int16_t*)SDDS_realloc(int16Value[p], sizeof(int16_t) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(int16Value[p][0]), 2 * rows, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    uint16Value = (uint16_t**)SDDS_realloc(uint16Value, sizeof(uint16_t*) * page);
    for (i=0; i<n; i++) {
      uint16Value[p-i] = NULL;
    }
    uint16Value[p] = (uint16_t*)SDDS_realloc(uint16Value[p], sizeof(uint16_t) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(uint16Value[p][0]), 2 * rows, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_INT32:
    int32Value = (int32_t**)SDDS_realloc(int32Value, sizeof(int32_t*) * page);
    for (i=0; i<n; i++) {
      int32Value[p-i] = NULL;
    }
    int32Value[p] = (int32_t*)SDDS_realloc(int32Value[p], sizeof(int32_t) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(int32Value[p][0]), 4 * rows, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    uint32Value = (uint32_t**)SDDS_realloc(uint32Value, sizeof(uint32_t*) * page);
    for (i=0; i<n; i++) {
      uint32Value[p-i] = NULL;
    }
    uint32Value[p] = (uint32_t*)SDDS_realloc(uint32Value[p], sizeof(uint32_t) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(uint32Value[p][0]), 4 * rows, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    real32Value = (float**)SDDS_realloc(real32Value, sizeof(float*) * page);
    for (i=0; i<n; i++) {
      real32Value[p-i] = NULL;
    }
    rowsAllocated[p] = rows;
    real32Value[p] = (float*)SDDS_realloc(real32Value[p], sizeof(float) * rows);
    if (SDDS_bufferedRead(&(real32Value[p][0]), 4 * rows, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    real64Value = (double**)SDDS_realloc(real64Value, sizeof(double*) * page);
    for (i=0; i<n; i++) {
      real64Value[p-i] = NULL;
    }
    real64Value[p] = (double*)SDDS_realloc(real64Value[p], sizeof(double) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(real64Value[p][0]), 8 * rows, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  case SDDS_STRING:
    stringValue = (char***)SDDS_realloc(stringValue, sizeof(char**) * page);
    for (i=0; i<n; i++) {
      stringValue[p-i] = NULL;
    }
    stringValue[p] = (char**)SDDS_realloc(stringValue[p], sizeof(char*) * rows);
    rowsAllocated[p] = rows;
    for (i=0; i<rows; i++) {
      stringValue[p][i] = NULL;
      if (SDDS_readBinaryString(&(stringValue[p][i]), gzfp, fBuffer, sddsfile)) {
        ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading strings (SDDSColumn::readBinaryValues)");
        return(1);
      }
    }
    break;
  case SDDS_CHARACTER:
    charValue = (char**)SDDS_realloc(charValue, sizeof(char*) * page);
    for (i=0; i<n; i++) {
      charValue[p-i] = NULL;
    }
    charValue[p] = (char*)SDDS_realloc(charValue[p], sizeof(char) * rows);
    rowsAllocated[p] = rows;
    if (SDDS_bufferedRead(&(charValue[p][0]), rows, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--failure reading values (SDDSColumn::readBinaryValues)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read column--unknown type (SDDSColumn::readBinaryValues)");
    return(1);
  }
  pageCount = page;
  rowCount[p] = rows;
  return(0);
}

