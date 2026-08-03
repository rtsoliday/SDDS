/* GUIDELINES
   Do not exit or print to stdout or stderr from any of the functions in this file
*/

/* TODO 
        Ensure that strtod returns HUGE_VAL on all operating systems.
        Modify setType so that it can change the type of an existing parameter.
        Add ability to add pages if only fixedvalue parameters exist.
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
 *                          SDDSParameter                                                      *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to SDDSParameter object                                                   *
 ***********************************************************************************************/
SDDSParameter::SDDSParameter() {
  pageCount = 0;
  parameterType = 0;
  fixed = false;
  parameterName = NULL;
  parameterSymbol = NULL;
  parameterUnits = NULL;
  parameterDescription = NULL;
  parameterFormatString = NULL;
  parameterFixedValue = NULL;
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

/***********************************************************************************************
 *                          ~SDDSParameter                                                     *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
SDDSParameter::~SDDSParameter() {
  uint32_t i;
  if (parameterName != NULL)
    SDDS_free(parameterName);
  if (parameterSymbol != NULL)
    SDDS_free(parameterSymbol);
  if (parameterUnits != NULL)
    SDDS_free(parameterUnits);
  if (parameterDescription != NULL)
    SDDS_free(parameterDescription);
  if (parameterFormatString != NULL)
    SDDS_free(parameterFormatString);
  if (parameterFixedValue != NULL)
    SDDS_free(parameterFixedValue);
  if (int16Value != NULL)
    SDDS_free(int16Value);
  if (uint16Value != NULL)
    SDDS_free(uint16Value);
  if (int32Value != NULL)
    SDDS_free(int32Value);
  if (uint32Value != NULL)
    SDDS_free(uint32Value);
  if (real32Value != NULL)
    SDDS_free(real32Value);
  if (real64Value != NULL)
    SDDS_free(real64Value);
  if (stringValue != NULL) {
    for (i=0; i<pageCount; i++) {
      if (stringValue[i] != NULL) {
	SDDS_free(stringValue[i]);
      }
    }
    SDDS_free(stringValue);
  }
  if (charValue != NULL)
    SDDS_free(charValue);
}

/***********************************************************************************************
 *                             freePage                                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSParameter::freePage() {
  uint32_t i;
  switch (parameterType) {
  case SDDS_INT16:
    if (int16Value != NULL)
      SDDS_free(int16Value);
    int16Value = NULL;
    break;
  case SDDS_UINT16:
    if (uint16Value != NULL)
      SDDS_free(uint16Value);
    uint16Value = NULL;
    break;
  case SDDS_INT32:
    if (int32Value != NULL)
      SDDS_free(int32Value);
    int32Value = NULL;
    break;
  case SDDS_UINT32:
    if (uint32Value != NULL)
      SDDS_free(uint32Value);
    uint32Value = NULL;
    break;
  case SDDS_REAL32:
    if (real32Value != NULL)
      SDDS_free(real32Value);
    real32Value = NULL;
    break;
  case SDDS_REAL64:
    if (real64Value != NULL)
      SDDS_free(real64Value);
    real64Value = NULL;
    break;
  case SDDS_STRING:
    if (stringValue != NULL) {
      for (i=0; i<pageCount; i++) {
        if (stringValue[i] != NULL) {
          SDDS_free(stringValue[i]);
        }
      }
      SDDS_free(stringValue);
      stringValue = NULL;
    }
    break;
  case SDDS_CHARACTER:
    if (charValue != NULL)
      SDDS_free(charValue);
    charValue = NULL;
    break;
  default:
    break;
  }
  pageCount = 0;
}

/***********************************************************************************************
 *                           setupParameter                                                    *
 *                                                                                             *
 * C++ Arguments: char *name, char *symbol, char *units, char *description,                    *
 *                char *format_string, int32_t type, char *fixed_value                         *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSParameter::setupParameter(char *name, char *symbol, char *units, char *description, char *format_string, int32_t type, char *fixed_value) {
  if (parameterName != NULL)
    SDDS_free(parameterName);
  if (parameterSymbol != NULL)
    SDDS_free(parameterSymbol);
  if (parameterUnits != NULL)
    SDDS_free(parameterUnits);
  if (parameterDescription != NULL)
    SDDS_free(parameterDescription);
  if (parameterFormatString != NULL)
    SDDS_free(parameterFormatString);
  if (parameterFixedValue != NULL)
    SDDS_free(parameterFixedValue);

  if (name == NULL)
    parameterName = NULL;
  else {
    parameterName = (char*)SDDS_malloc(sizeof(char) * strlen(name) + 1);
    strcpy(parameterName, name);
  }
  if (symbol == NULL)
    parameterSymbol = NULL;
  else {
    parameterSymbol = (char*)SDDS_malloc(sizeof(char) * strlen(symbol) + 1);
    strcpy(parameterSymbol, symbol);
  }
  if (units == NULL)
    parameterUnits = NULL;
  else {
    parameterUnits = (char*)SDDS_malloc(sizeof(char) * strlen(units) + 1);
    strcpy(parameterUnits, units);
  }
  if (description == NULL)
    parameterDescription = NULL;
  else {
    parameterDescription = (char*)SDDS_malloc(sizeof(char) * strlen(description) + 1);
    strcpy(parameterDescription, description);
  }
  if (format_string == NULL)
    parameterFormatString = NULL;
  else {
    parameterFormatString = (char*)SDDS_malloc(sizeof(char) * strlen(format_string) + 1);
    strcpy(parameterFormatString, format_string);
  }
  if (fixed_value == NULL) {
    parameterFixedValue = NULL;
    fixed = false;
  } else {
    parameterFixedValue = (char*)SDDS_malloc(sizeof(char) * strlen(fixed_value) + 1);
    strcpy(parameterFixedValue, fixed_value);
    fixed = true;
  }
  parameterType = type;
}

/***********************************************************************************************
 *                             informAboutParent                                               *
 *                                                                                             *
 * C++ Arguments: SDDSFile *parent                                                             *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSParameter::informAboutParent(void *parent) {
  sddsfile = parent;
}

/***********************************************************************************************
 *                             getName                                                         *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to name  (do not free the result)                                         *
 ***********************************************************************************************/
char* SDDSParameter::getName() {
  return(parameterName);
}

/***********************************************************************************************
 *                             setName                                                         *
 *                                                                                             *
 * C++ Arguments: char *name                                                                   *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSParameter::setName(char *name) {
  if (parameterName != NULL)
    SDDS_free(parameterName);
  if (name == NULL)
    parameterName = NULL;
  else {
    parameterName = (char*)SDDS_malloc(sizeof(char) * strlen(name) + 1);
    strcpy(parameterName, name);
  }
}

/***********************************************************************************************
 *                             getSymbol                                                       *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to symbol (do not free the result)                                        *
 ***********************************************************************************************/
char* SDDSParameter::getSymbol() {
  return(parameterSymbol);
}

/***********************************************************************************************
 *                             setSymbol                                                       *
 *                                                                                             *
 * C++ Arguments: char *symbol                                                                 *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSParameter::setSymbol(char *symbol) {
  if (parameterSymbol != NULL)
    SDDS_free(parameterSymbol);
  if (symbol == NULL)
    parameterSymbol = NULL;
  else {
    parameterSymbol = (char*)SDDS_malloc(sizeof(char) * strlen(symbol) + 1);
    strcpy(parameterSymbol, symbol);
  }
}

/***********************************************************************************************
 *                             getUnits                                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to the units (do not free the result)                                     *
 ***********************************************************************************************/
char* SDDSParameter::getUnits() {
  return(parameterUnits);
}

/***********************************************************************************************
 *                             setUnits                                                        *
 *                                                                                             *
 * C++ Arguments: char *units                                                                  *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSParameter::setUnits(char *units) {
  if (parameterUnits != NULL)
    SDDS_free(parameterUnits);
  if (units == NULL)
    parameterUnits = NULL;
  else {
    parameterUnits = (char*)SDDS_malloc(sizeof(char) * strlen(units) + 1);
    strcpy(parameterUnits, units);
  }
}

/***********************************************************************************************
 *                             getDescription                                                  *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to the description (do not free the result)                               *
 ***********************************************************************************************/
char* SDDSParameter::getDescription() {
  return(parameterDescription);
}

/***********************************************************************************************
 *                             setDescription                                                  *
 *                                                                                             *
 * C++ Arguments: char *description                                                            *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSParameter::setDescription(char *description) {
  if (parameterDescription != NULL)
    SDDS_free(parameterDescription);
  if (description == NULL)
    parameterDescription = NULL;
  else {
    parameterDescription = (char*)SDDS_malloc(sizeof(char) * strlen(description) + 1);
    strcpy(parameterDescription, description);
  }
}

/***********************************************************************************************
 *                             getFormatString                                                 *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to the format string (do not free the result)                             *
 ***********************************************************************************************/
char* SDDSParameter::getFormatString() {
  return(parameterFormatString);
}

/***********************************************************************************************
 *                             setFormatString                                                 *
 *                                                                                             *
 * C++ Arguments: char *format_string                                                          *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSParameter::setFormatString(char *format_string) {
  if (parameterFormatString != NULL)
    SDDS_free(parameterFormatString);
  if (format_string == NULL)
    parameterFormatString = NULL;
  else {
    parameterFormatString = (char*)SDDS_malloc(sizeof(char) * strlen(format_string) + 1);
    strcpy(parameterFormatString, format_string);
  }
}

/***********************************************************************************************
 *                             getFixedValue                                                   *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results:  pointer to the fixed value   (do not free the result)                             *
 ***********************************************************************************************/
char* SDDSParameter::getFixedValue() {
  return(parameterFixedValue);
}

/***********************************************************************************************
 *                             setFixedValue                                                   *
 *                                                                                             *
 * Note: you can turn off fixed value by sending a NULL value                                  *
 *                                                                                             *
 * C++ Arguments: char *fixed_value                                                            *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSParameter::setFixedValue(char *fixed_value) {
  if (parameterFixedValue != NULL)
    SDDS_free(parameterFixedValue);
  if (fixed_value == NULL) {
    parameterFixedValue = NULL;
    fixed = false;
  } else {
    parameterFixedValue = (char*)SDDS_malloc(sizeof(char) * strlen(fixed_value) + 1);
    strcpy(parameterFixedValue, fixed_value);
    fixed = true;
  }
}

/***********************************************************************************************
 *                             getType                                                         *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: the parameter type in integer form                                                 *
 ***********************************************************************************************/
int32_t SDDSParameter::getType() {
  return(parameterType);
}

/***********************************************************************************************
 *                             setType                                                         *
 *                                                                                             *
 * C++ Arguments:int32_t type                                                                  *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSParameter::setType(int32_t type) {
  parameterType = type;
}

/***********************************************************************************************
 *                             getPageCount                                                    *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: the page count                                                                     *
 ***********************************************************************************************/
uint32_t SDDSParameter::getPageCount() {
  return(pageCount);
}

/***********************************************************************************************
 *                        setValue                                                             *
 *                                                                                             *
 * C++ Arguments: int32_t value, uint32_t page                                                  *
 *                uint32_t value, uint32_t page                                                 *
 *                double value, uint32_t page                                                   *
 *                char *value, uint32_t page                                                    *
 *                                                                                             *
 * Results:  0 for success                                                                     *
 *           1 for invalid page number                                                         *
 *           2 for the case where the value cannot be converted to the proper parameter type   *
 *           3 for invalid parameter type                                                      *
 ***********************************************************************************************/
int32_t SDDSParameter::setValue(int32_t value, uint32_t page) {
  uint32_t n=0, i;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  if (fixed)
    setFixedValue(NULL);
  /* if n > 1 then put 0 values in skipped pages */
  switch (parameterType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t*)SDDS_realloc(int16Value, sizeof(int16_t) * page);
      for (i=2; i<=n; i++)
	int16Value[page - i] = (int16_t)0;
    }
    int16Value[page-1] = (int16_t)value;
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t*)SDDS_realloc(uint16Value, sizeof(uint16_t) * page);
      for (i=2; i<=n; i++)
	uint16Value[page - i] = (uint16_t)0;
    }
    uint16Value[page-1] = (uint16_t)value;
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t*)SDDS_realloc(int32Value, sizeof(int32_t) * page);
      for (i=2; i<=n; i++)
	int32Value[page - i] = (int32_t)0;
    }
    int32Value[page-1] = (int32_t)value;
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t*)SDDS_realloc(uint32Value, sizeof(uint32_t) * page);
      for (i=2; i<=n; i++)
	uint32Value[page - i] = (uint32_t)0;
    }
    uint32Value[page-1] = (uint32_t)value;
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float*)SDDS_realloc(real32Value, sizeof(float) * page);
      for (i=2; i<=n; i++)
	real32Value[page - i] = (float)0;
    }
    real32Value[page-1] = (float)value;
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double*)SDDS_realloc(real64Value, sizeof(double) * page);
      for (i=2; i<=n; i++)
	real64Value[page - i] = (double)0;
    }
    real64Value[page-1] = (double)value;
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char**)SDDS_realloc(stringValue, sizeof(char*) * page);
      for (i=2; i<=n; i++)
	stringValue[page - i] = NULL;
    }
    sprintf(s, "%" PRId32, value);
    stringValue[page-1] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(stringValue[page-1], s);
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char*)SDDS_realloc(charValue, sizeof(char) * page);
      for (i=2; i<=n; i++)
	charValue[page - i] = 0;
    }
    charValue[page-1] = (char)value;
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  return(0);
}

int32_t SDDSParameter::setValue(uint32_t value, uint32_t page) {
  uint32_t n=0, i;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  if (fixed)
    setFixedValue(NULL);
  /* if n > 1 then put 0 values in skipped pages */
  switch (parameterType) {
  case SDDS_INT16:
    if (n > 0) {
      int16Value = (int16_t*)SDDS_realloc(int16Value, sizeof(int16_t) * page);
      for (i=2; i<=n; i++)
	int16Value[page - i] = (int16_t)0;
    }
    int16Value[page-1] = (int16_t)value;
    break;
  case SDDS_UINT16:
    if (n > 0) {
      uint16Value = (uint16_t*)SDDS_realloc(uint16Value, sizeof(uint16_t) * page);
      for (i=2; i<=n; i++)
	uint16Value[page - i] = (uint16_t)0;
    }
    uint16Value[page-1] = (uint16_t)value;
    break;
  case SDDS_INT32:
    if (n > 0) {
      int32Value = (int32_t*)SDDS_realloc(int32Value, sizeof(int32_t) * page);
      for (i=2; i<=n; i++)
	int32Value[page - i] = (int32_t)0;
    }
    int32Value[page-1] = (int32_t)value;
    break;
  case SDDS_UINT32:
    if (n > 0) {
      uint32Value = (uint32_t*)SDDS_realloc(uint32Value, sizeof(uint32_t) * page);
      for (i=2; i<=n; i++)
	uint32Value[page - i] = (uint32_t)0;
    }
    uint32Value[page-1] = (uint32_t)value;
    break;
  case SDDS_REAL32:
    if (n > 0) {
      real32Value = (float*)SDDS_realloc(real32Value, sizeof(float) * page);
      for (i=2; i<=n; i++)
	real32Value[page - i] = (float)0;
    }
    real32Value[page-1] = (float)value;
    break;
  case SDDS_REAL64:
    if (n > 0) {
      real64Value = (double*)SDDS_realloc(real64Value, sizeof(double) * page);
      for (i=2; i<=n; i++)
	real64Value[page - i] = (double)0;
    }
    real64Value[page-1] = (double)value;
    break;
  case SDDS_STRING:
    if (n > 0) {
      stringValue = (char**)SDDS_realloc(stringValue, sizeof(char*) * page);
      for (i=2; i<=n; i++)
	stringValue[page - i] = NULL;
    }
    sprintf(s, "%" PRIu32, value);
    stringValue[page-1] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(stringValue[page-1], s);
    break;
  case SDDS_CHARACTER:
    if (n > 0) {
      charValue = (char*)SDDS_realloc(charValue, sizeof(char) * page);
      for (i=2; i<=n; i++)
	charValue[page - i] = 0;
    }
    charValue[page-1] = (char)value;
    break;
  default:
    return(3);
  }
  if (n > 0)
    pageCount = page;
  return(0);
}

int32_t SDDSParameter::setValue(double value, uint32_t page) {
  uint32_t n=0, i;
  bool newPage=false;
  char s[SDDS_MAXLINE];
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  if (fixed)
    setFixedValue(NULL);
  /* if n > 1 then put 0 values in skipped pages */
  switch (parameterType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t*)SDDS_realloc(int16Value, sizeof(int16_t) * page);
      for (i=2; i<=n; i++)
	int16Value[page - i] = (int16_t)0;
    }
    int16Value[page-1] = (int16_t)value;
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t*)SDDS_realloc(uint16Value, sizeof(uint16_t) * page);
      for (i=2; i<=n; i++)
	uint16Value[page - i] = (uint16_t)0;
    }
    uint16Value[page-1] = (uint16_t)value;
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t*)SDDS_realloc(int32Value, sizeof(int32_t) * page);
      for (i=2; i<=n; i++)
	int32Value[page - i] = (int32_t)0;
    }
    int32Value[page-1] = (int32_t)value;
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t*)SDDS_realloc(uint32Value, sizeof(uint32_t) * page);
      for (i=2; i<=n; i++)
	uint32Value[page - i] = (uint32_t)0;
    }
    uint32Value[page-1] = (uint32_t)value;
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float*)SDDS_realloc(real32Value, sizeof(float) * page);
      for (i=2; i<=n; i++)
	real32Value[page - i] = (float)0;
    }
    real32Value[page-1] = (float)value;
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double*)SDDS_realloc(real64Value, sizeof(double) * page);
      for (i=2; i<=n; i++)
	real64Value[page - i] = (double)0;
    }
    real64Value[page-1] = (double)value;
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char**)SDDS_realloc(stringValue, sizeof(char*) * page);
      for (i=2; i<=n; i++)
	stringValue[page - i] = NULL;
    }
    sprintf(s, "%lf", value);
    stringValue[page-1] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(stringValue[page-1], s);
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char*)SDDS_realloc(charValue, sizeof(char) * page);
      for (i=2; i<=n; i++)
	charValue[page - i] = 0;
    }
    charValue[page-1] = (char)value;
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  return(0);
}

int32_t SDDSParameter::setValue(char *value, uint32_t page) {
  int32_t returnVal=0;
  uint32_t n=0, i;
  bool newPage=false;
  int64_t iValue=0;
  double dValue=0;
  
  if (page == 0)
    return(1);
  if (page > pageCount) {
    newPage = true;
    n = page - pageCount;
  }
  if (fixed)
    setFixedValue(NULL);
  if (SDDS_INTEGER_TYPE(parameterType)) {
    iValue = strtoll(value, (char**)NULL, 10);
    if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
      /*unable to convert given string to integer parameter*/
      returnVal=2;
      iValue=0;
    }
  } else if (SDDS_FLOATING_TYPE(parameterType)) {
    dValue = strtod(value, (char**)NULL);
    if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
      /*unable to convert given string to floating point parameter*/
      returnVal=2;
      dValue=0;
    }
  }
  /* if n > 1 then put 0 values in skipped pages */
  switch (parameterType) {
  case SDDS_INT16:
    if (newPage) {
      int16Value = (int16_t*)SDDS_realloc(int16Value, sizeof(int16_t) * page);
      for (i=2; i<=n; i++)
	int16Value[page - i] = (int16_t)0;
    }
    int16Value[page-1] = (int16_t)iValue;
    break;
  case SDDS_UINT16:
    if (newPage) {
      uint16Value = (uint16_t*)SDDS_realloc(uint16Value, sizeof(uint16_t) * page);
      for (i=2; i<=n; i++)
	uint16Value[page - i] = (uint16_t)0;
    }
    uint16Value[page-1] = (uint16_t)iValue;
    break;
  case SDDS_INT32:
    if (newPage) {
      int32Value = (int32_t*)SDDS_realloc(int32Value, sizeof(int32_t) * page);
      for (i=2; i<=n; i++)
	int32Value[page - i] = (int32_t)0;
    }
    int32Value[page-1] = (int32_t)iValue;
    break;
  case SDDS_UINT32:
    if (newPage) {
      uint32Value = (uint32_t*)SDDS_realloc(uint32Value, sizeof(uint32_t) * page);
      for (i=2; i<=n; i++)
	uint32Value[page - i] = (uint32_t)0;
    }
    uint32Value[page-1] = (uint32_t)iValue;
    break;
  case SDDS_REAL32:
    if (newPage) {
      real32Value = (float*)SDDS_realloc(real32Value, sizeof(float) * page);
      for (i=2; i<=n; i++)
	real32Value[page - i] = (float)0;
    }
    real32Value[page-1] = (float)dValue;
    break;
  case SDDS_REAL64:
    if (newPage) {
      real64Value = (double*)SDDS_realloc(real64Value, sizeof(double) * page);
      for (i=2; i<=n; i++)
	real64Value[page - i] = (double)0;
    }
    real64Value[page-1] = (double)dValue;
    break;
  case SDDS_STRING:
    if (newPage) {
      stringValue = (char**)SDDS_realloc(stringValue, sizeof(char*) * page);
      for (i=2; i<=n; i++)
	stringValue[page - i] = NULL;
    }
    stringValue[page-1] = (char*)SDDS_malloc(sizeof(char) * strlen(value) + 1);
    strcpy(stringValue[page-1], value);
    break;
  case SDDS_CHARACTER:
    if (newPage) {
      charValue = (char*)SDDS_realloc(charValue, sizeof(char) * page);
      for (i=2; i<=n; i++)
	charValue[page - i] = 0;
    }
    charValue[page-1] = value[0];
    break;
  default:
    return(3);
  }
  if (newPage)
    pageCount = page;
  return(returnVal);
}

/***********************************************************************************************
 *                        readAsciiValue                                                       *
 *                                                                                             *
 * C++ Arguments: char *value, uint32_t page                                                    *
 *                                                                                             *
 * Results:  0 for success                                                                     *
 *           1 for failure                                                                     *
 ***********************************************************************************************/
int32_t SDDSParameter::readAsciiValue(char **bigBuffer, int32_t *bigBufferSize, uint32_t page) {
  int32_t returnVal=0, length;
  uint32_t p;
  int64_t iValue=0;
  double dValue=0;
  static char *buffer=NULL;
  static int32_t bufferSize=0;
  if (fixed) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--parameter is fixed (SDDSParameter::readAsciiValue)");
    return(1);
  }
  if (page == 0) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--invalid page number (SDDSParameter::readAsciiValue)");
    return(1);
  }
  if (page - 1 != pageCount) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--invalid page number (SDDSParameter::readAsciiValue)");
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

  if (SDDS_INTEGER_TYPE(parameterType)) {
    if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter (SDDSParameter::readAsciiValue)");
      returnVal=1;
      iValue=0;
    } else {
      iValue = strtoll(buffer, (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
        ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--not a valid integer (SDDSParameter::readAsciiValue)");
        returnVal=1;
        iValue=0;
      }
    }
  } else if (SDDS_FLOATING_TYPE(parameterType)) {
    if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter (SDDSParameter::readAsciiValue)");
      returnVal=1;
      dValue=0;
    } else {
      dValue = strtod(buffer, (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
        ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--not a valid number (SDDSParameter::readAsciiValue)");
        returnVal=1;
        dValue=0;
      }
    }
  }
  p = page - 1;
  switch (parameterType) {
  case SDDS_INT16:
    int16Value = (int16_t*)SDDS_realloc(int16Value, sizeof(int16_t) * page);
    int16Value[p] = (int16_t)iValue;
    break;
  case SDDS_UINT16:
    uint16Value = (uint16_t*)SDDS_realloc(uint16Value, sizeof(uint16_t) * page);
    uint16Value[p] = (uint16_t)iValue;
    break;
  case SDDS_INT32:
    int32Value = (int32_t*)SDDS_realloc(int32Value, sizeof(int32_t) * page);
    int32Value[p] = (int32_t)iValue;
    break;
  case SDDS_UINT32:
    uint32Value = (uint32_t*)SDDS_realloc(uint32Value, sizeof(uint32_t) * page);
    uint32Value[p] = (uint32_t)iValue;
    break;
  case SDDS_REAL32:
    real32Value = (float*)SDDS_realloc(real32Value, sizeof(float) * page);
    real32Value[p] = (float)dValue;
    break;
  case SDDS_REAL64:
    real64Value = (double*)SDDS_realloc(real64Value, sizeof(double) * page);
    real64Value[p] = (double)dValue;
    break;
  case SDDS_STRING:
    stringValue = (char**)SDDS_realloc(stringValue, sizeof(char*) * page);
    length = strlen(*bigBuffer);
    if (length > 0) {
      if ((*bigBuffer)[length-1] == '\r') {
        (*bigBuffer)[length-1] = 0;
      }
    }
    if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter (SDDSParameter::readAsciiValue)");
      returnVal=1;
      stringValue[p] = (char*)SDDS_malloc(sizeof(char) * 4);
      strcpy(stringValue[p], "\"\"");
    } else {
      SDDS_interpretEscapes(buffer);
      stringValue[p] = (char*)SDDS_malloc(sizeof(char) * strlen(buffer) + 1);
      strcpy(stringValue[p], buffer);
    }
    break;
  case SDDS_CHARACTER:
    charValue = (char*)SDDS_realloc(charValue, sizeof(char) * page);
    if (SDDS_getToken(bigBuffer, bigBufferSize, buffer, bufferSize) < 0) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter (SDDSParameter::readAsciiValue)");
      returnVal=1;
      charValue[p] = '-';
    } else {
      charValue[p] = buffer[0];
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--invalid parameter type (SDDSParameter::readAsciiValue)");
    return(1);
  }
  pageCount = page;
  return(returnVal);
}

/***********************************************************************************************
 *                        getValueInInt32                                                      *
 *                                                                                             *
 * C++ Arguments: uint32_t page, int32_t *errorcode                                             *
 *                                                                                             *
 * Results:  value    and errorcode=0 for success                                              *
 *           LONG_MAX and errorcode=1 for parameter value undefined                            *
 *           LONG_MAX and errorcode=2 for the case where the value cannot be                   *
 *                                    converted to the proper type                             *
 *           LONG_MAX and errorcode=3 for invalid parameter type                               *
 ***********************************************************************************************/
int32_t SDDSParameter::getValueInInt32(uint32_t page, int32_t *errorcode) {
  int64_t iValue;
  double dValue;
  if ((page == 0) || (page > pageCount)) {
    if (!fixed) {
      /*parameter value undefined*/
      *errorcode = 1;
      return((int32_t)LONG_MAX);
    }
  }
  *errorcode = 0;
  if (fixed) {
    if ((SDDS_INTEGER_TYPE(parameterType)) || 
	(parameterType == SDDS_STRING) || 
	(parameterType == SDDS_CHARACTER)) {
      iValue = strtoll(parameterFixedValue, (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	/*unable to convert fixed value string to integer*/
	*errorcode = 2;
	return((int32_t)LONG_MAX);
      }
      return((int32_t)iValue);
    } else if (SDDS_FLOATING_TYPE(parameterType)) {
      dValue = strtod(parameterFixedValue, (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	/*unable to convert fixed value string to floating point*/
	*errorcode = 2;
	return((int32_t)LONG_MAX);
      }
      return((int32_t)dValue);
    }
  }
  page--;
  switch (parameterType) {
  case SDDS_INT16:
    return((int32_t)int16Value[page]);
  case SDDS_UINT16:
    return((int32_t)uint16Value[page]);
  case SDDS_INT32:
    return((int32_t)int32Value[page]);
  case SDDS_UINT32:
    return((int32_t)uint32Value[page]);
  case SDDS_REAL32:
    return((int32_t)real32Value[page]);
  case SDDS_REAL64:
    return((int32_t)real64Value[page]);
  case SDDS_STRING:
    iValue = strtoll(stringValue[page], (char**)NULL, 10);
    if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
      /*unable to convert string to integer*/
      *errorcode = 2;
      return((int32_t)LONG_MAX);
    }
    return((int32_t)iValue);
  case SDDS_CHARACTER:
    return((int32_t)charValue[page]);
  default:
    *errorcode = 3;
    return((int32_t)LONG_MAX);
  }
}

/***********************************************************************************************
 *                        getValuesInInt32                                                     *
 *                                                                                             *
 * C++ Arguments: int32_t *errorcode                                                           *
 *                                                                                             *
 * Results:  pointer to values and errorcode=0 for success                                     *
 *           NULL and errorcode=1 for parameter value undefined                                *
 *           NULL and errorcode=2 for the case where the value cannot be                       *
 *                                    converted to the proper type                             *
 *           NULL and errorcode=3 for invalid parameter type                                   *
 ***********************************************************************************************/
int32_t* SDDSParameter::getValuesInInt32(int32_t *errorcode) {
  int64_t iValue;
  double dValue;
  int32_t* rValues;
  uint32_t i;
  if (pageCount == 0) {
    if (!fixed) {
      /*parameter value undefined*/
      *errorcode = 1;
      return((int32_t*)NULL);
    }
  }
  *errorcode = 0;
  rValues = (int32_t*)SDDS_malloc(sizeof(int32_t) * pageCount);
  if (fixed) {
    if ((SDDS_INTEGER_TYPE(parameterType)) || 
	(parameterType == SDDS_STRING) || 
	(parameterType == SDDS_CHARACTER)) {
      iValue = strtoll(parameterFixedValue, (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	/*unable to convert fixed value string to integer*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((int32_t*)NULL);
      }
      for (i=0; i<pageCount; i++)
	rValues[i] = (int32_t)iValue;
      return((int32_t*)rValues);
    } else if (SDDS_FLOATING_TYPE(parameterType)) {
      dValue = strtod(parameterFixedValue, (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	/*unable to convert fixed value string to floating point*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((int32_t*)NULL);
      }
      for (i=0; i<pageCount; i++)
	rValues[i] = (int32_t)dValue;
      return((int32_t*)rValues);
    }
  }
  switch (parameterType) {
  case SDDS_INT16:
    for (i=0; i<pageCount; i++)
      rValues[i] = (int32_t)int16Value[i];
    return((int32_t*)rValues);
  case SDDS_UINT16:
    for (i=0; i<pageCount; i++)
      rValues[i] = (int32_t)uint16Value[i];
    return((int32_t*)rValues);
  case SDDS_INT32:
    for (i=0; i<pageCount; i++)
      rValues[i] = (int32_t)int32Value[i];
    return((int32_t*)rValues);
  case SDDS_UINT32:
    for (i=0; i<pageCount; i++)
      rValues[i] = (int32_t)uint32Value[i];
    return((int32_t*)rValues);
  case SDDS_REAL32:
    for (i=0; i<pageCount; i++)
      rValues[i] = (int32_t)real32Value[i];
    return((int32_t*)rValues);
  case SDDS_REAL64:
    for (i=0; i<pageCount; i++)
      rValues[i] = (int32_t)real64Value[i];
    return((int32_t*)rValues);
  case SDDS_STRING:
    for (i=0; i<pageCount; i++) {
      iValue = strtoll(stringValue[i], (char**)NULL, 10);
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
    for (i=0; i<pageCount; i++)
      rValues[i] = (int32_t)charValue[i];
    return((int32_t*)rValues);
  default:
    *errorcode = 3;
    return((int32_t*)NULL);
  }
}

/***********************************************************************************************
 *                        getValueInUInt32                                                     *
 *                                                                                             *
 * C++ Arguments: uint32_t page, int32_t *errorcode                                             *
 *                                                                                             *
 * Results:  value    and errorcode=0 for success                                              *
 *           ULONG_MAX and errorcode=1 for parameter value undefined                           *
 *           ULONG_MAX and errorcode=2 for the case where the value cannot be                  *
 *                                    converted to the proper type                             *
 *           ULONG_MAX and errorcode=3 for invalid parameter type                              *
 ***********************************************************************************************/
uint32_t SDDSParameter::getValueInUInt32(uint32_t page, int32_t *errorcode) {
  int64_t iValue;
  double dValue;
  if ((page == 0) || (page > pageCount)) {
    if (!fixed) {
      /*parameter value undefined*/
      *errorcode = 1;
      return((uint32_t)ULONG_MAX);
    }
  }
  *errorcode = 0;
  if (fixed) {
    if ((SDDS_INTEGER_TYPE(parameterType)) || 
	(parameterType == SDDS_STRING) || 
	(parameterType == SDDS_CHARACTER)) {
      iValue = strtoll(parameterFixedValue, (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	/*unable to convert fixed value string to integer*/
	*errorcode = 2;
	return((uint32_t)ULONG_MAX);
      }
      return((uint32_t)iValue);
    } else if (SDDS_FLOATING_TYPE(parameterType)) {
      dValue = strtod(parameterFixedValue, (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	/*unable to convert fixed value string to floating point*/
	*errorcode = 2;
	return((uint32_t)ULONG_MAX);
      }
      return((uint32_t)dValue);
    }
  }
  page--;
  switch (parameterType) {
  case SDDS_INT16:
    return((uint32_t)int16Value[page]);
  case SDDS_UINT16:
    return((uint32_t)uint16Value[page]);
  case SDDS_INT32:
    return((uint32_t)int32Value[page]);
  case SDDS_UINT32:
    return((uint32_t)uint32Value[page]);
  case SDDS_REAL32:
    return((uint32_t)real32Value[page]);
  case SDDS_REAL64:
    return((uint32_t)real64Value[page]);
  case SDDS_STRING:
    iValue = strtoll(stringValue[page], (char**)NULL, 10);
    if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
      /*unable to convert string to integer*/
      *errorcode = 2;
      return((uint32_t)ULONG_MAX);
    }
    return((uint32_t)iValue);
  case SDDS_CHARACTER:
    return((uint32_t)charValue[page]);
  default:
    *errorcode = 3;
    return((uint32_t)ULONG_MAX);
  }
}

/***********************************************************************************************
 *                        getValuesInUInt32                                                    *
 *                                                                                             *
 * C++ Arguments: int32_t *errorcode                                                           *
 *                                                                                             *
 * Results:  pointer to values and errorcode=0 for success                                     *
 *           NULL and errorcode=1 for parameter value undefined                                *
 *           NULL and errorcode=2 for the case where the value cannot be                       *
 *                                    converted to the proper type                             *
 *           NULL and errorcode=3 for invalid parameter type                                   *
 ***********************************************************************************************/
uint32_t* SDDSParameter::getValuesInUInt32(int32_t *errorcode) {
  int64_t iValue;
  double dValue;
  uint32_t* rValues;
  uint32_t i;
  if (pageCount == 0) {
    if (!fixed) {
      /*parameter value undefined*/
      *errorcode = 1;
      return((uint32_t*)NULL);
    }
  }
  *errorcode = 0;
  rValues = (uint32_t*)SDDS_malloc(sizeof(uint32_t) * pageCount);
  if (fixed) {
    if ((SDDS_INTEGER_TYPE(parameterType)) || 
	(parameterType == SDDS_STRING) || 
	(parameterType == SDDS_CHARACTER)) {
      iValue = strtoll(parameterFixedValue, (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	/*unable to convert fixed value string to integer*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((uint32_t*)NULL);
      }
      for (i=0; i<pageCount; i++)
	rValues[i] = (uint32_t)iValue;
      return((uint32_t*)rValues);
    } else if (SDDS_FLOATING_TYPE(parameterType)) {
      dValue = strtod(parameterFixedValue, (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	/*unable to convert fixed value string to floating point*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((uint32_t*)NULL);
      }
      for (i=0; i<pageCount; i++)
	rValues[i] = (uint32_t)dValue;
      return((uint32_t*)rValues);
    }
  }
  switch (parameterType) {
  case SDDS_INT16:
    for (i=0; i<pageCount; i++)
      rValues[i] = (uint32_t)int16Value[i];
    return((uint32_t*)rValues);
  case SDDS_UINT16:
    for (i=0; i<pageCount; i++)
      rValues[i] = (uint32_t)uint16Value[i];
    return((uint32_t*)rValues);
  case SDDS_INT32:
    for (i=0; i<pageCount; i++)
      rValues[i] = (uint32_t)int32Value[i];
    return((uint32_t*)rValues);
  case SDDS_UINT32:
    for (i=0; i<pageCount; i++)
      rValues[i] = (uint32_t)uint32Value[i];
    return((uint32_t*)rValues);
  case SDDS_REAL32:
    for (i=0; i<pageCount; i++)
      rValues[i] = (uint32_t)real32Value[i];
    return((uint32_t*)rValues);
  case SDDS_REAL64:
    for (i=0; i<pageCount; i++)
      rValues[i] = (uint32_t)real64Value[i];
    return((uint32_t*)rValues);
  case SDDS_STRING:
    for (i=0; i<pageCount; i++) {
      iValue = strtoll(stringValue[i], (char**)NULL, 10);
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
    for (i=0; i<pageCount; i++)
      rValues[i] = (uint32_t)charValue[i];
    return((uint32_t*)rValues);
  default:
    *errorcode = 3;
    return((uint32_t*)NULL);
  }
}

/***********************************************************************************************
 *                        getValueInDouble                                                     *
 *                                                                                             *
 * C++ Arguments: uint32_t page, int32_t *errorcode                                             *
 *                                                                                             *
 * Results:  value    and errorcode=0 for success                                              *
 *           HUGE_VAL and errorcode=1 for parameter value undefined                            *
 *           HUGE_VAL and errorcode=2 for the case where the value cannot be                   *
 *                                    converted to the proper type                             *
 *           HUGE_VAL and errorcode=3 for invalid parameter type                               *
 ***********************************************************************************************/
double SDDSParameter::getValueInDouble(uint32_t page, int32_t *errorcode) {
  int64_t iValue;
  double dValue;
  if ((page == 0) || (page > pageCount)) {
    if (!fixed) {
      /*parameter value undefined*/
      *errorcode = 1;
      return((double)HUGE_VAL);
    }
  }
  *errorcode = 0;
  if (fixed) {
    if ((SDDS_INTEGER_TYPE(parameterType)) || 
	(parameterType == SDDS_CHARACTER)) {
      iValue = strtoll(parameterFixedValue, (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	/*unable to convert fixed value string to integer*/
	*errorcode = 2;
	return((double)HUGE_VAL);
      }
      return((double)iValue);
    } else if ((SDDS_FLOATING_TYPE(parameterType)) ||
	       (parameterType == SDDS_STRING)) {
      dValue = strtod(parameterFixedValue, (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	/*unable to convert fixed value string to floating point*/
	*errorcode = 2;
	return((double)HUGE_VAL);
      }
      return((double)dValue);
    }
  }
  page--;
  switch (parameterType) {
  case SDDS_INT16:
    return((double)int16Value[page]);
  case SDDS_UINT16:
    return((double)uint16Value[page]);
  case SDDS_INT32:
    return((double)int32Value[page]);
  case SDDS_UINT32:
    return((double)uint32Value[page]);
  case SDDS_REAL32:
    return((double)real32Value[page]);
  case SDDS_REAL64:
    return((double)real64Value[page]);
  case SDDS_STRING:
    dValue = strtod(stringValue[page], (char**)NULL);
    if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
      /*unable to convert string to floating point*/
      *errorcode = 2;
      return((double)HUGE_VAL);
    }
    return((double)dValue);
  case SDDS_CHARACTER:
    return((double)charValue[page]);
  default:
    *errorcode = 3;
    return((double)HUGE_VAL);
  }
}

/***********************************************************************************************
 *                        getValuesInDouble                                                    *
 *                                                                                             *
 * C++ Arguments: int32_t *errorcode                                                           *
 *                                                                                             *
 * Results:  pointer to values and errorcode=0 for success                                     *
 *           NULL and errorcode=1 for parameter value undefined                                *
 *           NULL and errorcode=2 for the case where the value cannot be                       *
 *                                    converted to the proper type                             *
 *           NULL and errorcode=3 for invalid parameter type                                   *
 ***********************************************************************************************/
double* SDDSParameter::getValuesInDouble(int32_t *errorcode) {
  int64_t iValue;
  double dValue;
  double* rValues;
  uint32_t i;
  if (pageCount == 0) {
    if (!fixed) {
      /*parameter value undefined*/
      *errorcode = 1;
      return((double*)NULL);
    }
  }
  *errorcode = 0;
  rValues = (double*)SDDS_malloc(sizeof(double) * pageCount);
  if (fixed) {
    if ((SDDS_INTEGER_TYPE(parameterType)) || 
	(parameterType == SDDS_CHARACTER)) {
      iValue = strtoll(parameterFixedValue, (char**)NULL, 10);
      if ((iValue == LLONG_MIN) || (iValue == LLONG_MAX)) {
	/*unable to convert fixed value string to integer*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((double*)NULL);
      }
      for (i=0; i<pageCount; i++)
	rValues[i] = (double)iValue;
      return((double*)rValues);
    } else if ((SDDS_FLOATING_TYPE(parameterType)) ||
	       (parameterType == SDDS_STRING)) {
      dValue = strtod(parameterFixedValue, (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	/*unable to convert fixed value string to floating point*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((double*)NULL);
      }
      for (i=0; i<pageCount; i++)
	rValues[i] = (double)dValue;
      return((double*)rValues);
    }
  }
  switch (parameterType) {
  case SDDS_INT16:
    for (i=0; i<pageCount; i++)
      rValues[i] = (double)int16Value[i];
    return((double*)rValues);
  case SDDS_UINT16:
    for (i=0; i<pageCount; i++)
      rValues[i] = (double)uint16Value[i];
    return((double*)rValues);
  case SDDS_INT32:
    for (i=0; i<pageCount; i++)
      rValues[i] = (double)int32Value[i];
    return((double*)rValues);
  case SDDS_UINT32:
    for (i=0; i<pageCount; i++)
      rValues[i] = (double)uint32Value[i];
    return((double*)rValues);
  case SDDS_REAL32:
    for (i=0; i<pageCount; i++)
      rValues[i] = (double)real32Value[i];
    return((double*)rValues);
  case SDDS_REAL64:
    for (i=0; i<pageCount; i++)
      rValues[i] = (double)real64Value[i];
    return((double*)rValues);
  case SDDS_STRING:
    for (i=0; i<pageCount; i++) {
      dValue = strtod(stringValue[i], (char**)NULL);
      if ((dValue == HUGE_VAL) || (dValue == -HUGE_VAL)) {
	/*unable to convert fixed value string to floating point*/
	*errorcode = 2;
	SDDS_free(rValues);
	return((double*)NULL);
      }
      rValues[i] = (double)dValue;
    }
    return((double*)rValues);
  case SDDS_CHARACTER:
    for (i=0; i<pageCount; i++)
      rValues[i] = (double)charValue[i];
    return((double*)rValues);
  default:
    *errorcode = 3;
    return((double*)NULL);
  }
}

/***********************************************************************************************
 *                        getValueInString                                                     *
 *                                                                                             *
 * C++ Arguments: uint32_t page, int32_t *errorcode                                             *
 *                                                                                             *
 * Results:  value and errorcode=0 for success (You will have to free the value yourself)      *
 *           NULL  and errorcode=1 for parameter value undefined                               *
 *           NULL  and errorcode=2 for the case where the value cannot be                      *
 *                                 converted to the proper type                                *
 *           NULL  and errorcode=3 for invalid parameter type                                  *
 ***********************************************************************************************/
char* SDDSParameter::getValueInString(uint32_t page, int32_t *errorcode) {
  char *sValue;
  char s[SDDS_MAXLINE];
  if ((page == 0) || (page > pageCount)) {
    if (!fixed) {
      /*parameter value undefined*/
      *errorcode = 1;
      return((char*)NULL);
    }
  }
  *errorcode = 0;
  if (fixed) {
    sValue = (char*)SDDS_malloc(sizeof(char) * strlen(parameterFixedValue) + 1);
    strcpy(sValue, parameterFixedValue);
    return((char*)sValue);
  }
  page--;
  switch (parameterType) {
  case SDDS_INT16:
    sprintf(s, "%" PRId16, int16Value[page]);
    sValue = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(sValue, s);
    return((char*)sValue);
  case SDDS_UINT16:
    sprintf(s, "%" PRIu16, uint16Value[page]);
    sValue = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(sValue, s);
    return((char*)sValue);
  case SDDS_INT32:
    sprintf(s, "%" PRId32, int32Value[page]);
    sValue = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(sValue, s);
    return((char*)sValue);
  case SDDS_UINT32:
    sprintf(s, "%" PRIu32, uint32Value[page]);
    sValue = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(sValue, s);
    return((char*)sValue);
  case SDDS_REAL32:
    sprintf(s, "%f", real32Value[page]);
    sValue = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(sValue, s);
    return((char*)sValue);
  case SDDS_REAL64:
    sprintf(s, "%lf", real64Value[page]);
    sValue = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(sValue, s);
    return((char*)sValue);
  case SDDS_STRING:
    sValue = (char*)SDDS_malloc(sizeof(char) * strlen(stringValue[page]) + 1);
    strcpy(sValue, stringValue[page]);
    return((char*)sValue);
  case SDDS_CHARACTER:
    sprintf(s, "%c", charValue[page]);
    sValue = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
    strcpy(sValue, s);
    return((char*)sValue);
  default:
    *errorcode = 3;
    return((char*)NULL);
  }
}

/***********************************************************************************************
 *                        getValuesInString                                                    *
 *                                                                                             *
 * C++ Arguments: int32_t *errorcode                                                           *
 *                                                                                             *
 * Results:  pointer to values and errorcode=0 for success                                     *
 *           NULL and errorcode=1 for parameter value undefined                                *
 *           NULL and errorcode=2 for the case where the value cannot be                       *
 *                                    converted to the proper type                             *
 *           NULL and errorcode=3 for invalid parameter type                                   *
 ***********************************************************************************************/
char** SDDSParameter::getValuesInString(int32_t *errorcode) {
  char** rValues;
  uint32_t i;
  char s[SDDS_MAXLINE];
  if (pageCount == 0) {
    if (!fixed) {
      /*parameter value undefined*/
      *errorcode = 1;
      return((char**)NULL);
    }
  }
  *errorcode = 0;
  rValues = (char**)SDDS_malloc(sizeof(char*) * pageCount);
  if (fixed) {
    for (i=0; i<pageCount; i++) {
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(parameterFixedValue) + 1);
      strcpy(rValues[i], parameterFixedValue);
    }
    return((char**)rValues);
  }
  switch (parameterType) {
  case SDDS_INT16:
    for (i=0; i<pageCount; i++) {
      sprintf(s, "%" PRId16, int16Value[i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_UINT16:
    for (i=0; i<pageCount; i++) {
      sprintf(s, "%" PRIu16, uint16Value[i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_INT32:
    for (i=0; i<pageCount; i++) {
      sprintf(s, "%" PRId32, int32Value[i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_UINT32:
    for (i=0; i<pageCount; i++) {
      sprintf(s, "%" PRIu32, uint32Value[i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_REAL32:
    for (i=0; i<pageCount; i++) {
      sprintf(s, "%f", real32Value[i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_REAL64:
    for (i=0; i<pageCount; i++) {
      sprintf(s, "%lf", real64Value[i]);
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(s) + 1);
      strcpy(rValues[i], s);
    }
    return((char**)rValues);
  case SDDS_STRING:
    for (i=0; i<pageCount; i++) {
      rValues[i] = (char*)SDDS_malloc(sizeof(char) * strlen(stringValue[i]) + 1);
      strcpy(rValues[i], stringValue[i]);
    }
    return((char**)rValues);
  case SDDS_CHARACTER:
    for (i=0; i<pageCount; i++) {
      sprintf(s, "%c", charValue[i]);
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
int32_t SDDSParameter::writeDefinition(FILE *fp) {
  if ((parameterType <= 0) || (parameterType > SDDS_NUM_TYPES)) {
    return(1);
  }
  if (fp == NULL) {
    return(1);
  }
  fputs("&parameter ", fp);
  if (SDDS_printNamelistField(fp, (char*)"name", parameterName)) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"symbol", SDDS_blankToNull(parameterSymbol))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"units", SDDS_blankToNull(parameterUnits))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"description", SDDS_blankToNull(parameterDescription))) {
    return(1);
  }
  if (SDDS_printNamelistField(fp, (char*)"format_string", SDDS_blankToNull(parameterFormatString))) {
    return(1);
  }
  if (((SDDSFile*)sddsfile)->layoutVersion == 1) {
    if (SDDS_printNamelistField(fp, (char*)"type", SDDS_getVer1TypeName(parameterType))) {
      return(1);
    }
  } else if (((SDDSFile*)sddsfile)->layoutVersion == 2) {
    if (SDDS_printNamelistField(fp, (char*)"type", SDDS_getVer2TypeName(parameterType))) {
      return(1);
    }
  } else {
    if (SDDS_printNamelistField(fp, (char*)"type", SDDS_getTypeName(parameterType))) {
      return(1);
    }
  }
  if (SDDS_printNamelistField(fp, (char*)"fixed_value", parameterFixedValue)) {
    return(1);
  }
  fputs("&end\n", fp);
  return(0);
}

int32_t SDDSParameter::writeDefinition(voidp *gzfp) {
  if ((parameterType <= 0) || (parameterType > SDDS_NUM_TYPES)) {
    return(1);
  }
  if (gzfp == NULL) {
    return(1);
  }
  gzputs(gzfp, "&parameter ");
  if (SDDS_printNamelistField(gzfp, (char*)"name", parameterName)) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"symbol", SDDS_blankToNull(parameterSymbol))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"units", SDDS_blankToNull(parameterUnits))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"description", SDDS_blankToNull(parameterDescription))) {
    return(1);
  }
  if (SDDS_printNamelistField(gzfp, (char*)"format_string", SDDS_blankToNull(parameterFormatString))) {
    return(1);
  }
  if (((SDDSFile*)sddsfile)->layoutVersion == 1) {
    if (SDDS_printNamelistField(gzfp, (char*)"type", SDDS_getVer1TypeName(parameterType))) {
      return(1);
    }
  } else if (((SDDSFile*)sddsfile)->layoutVersion == 2) {
    if (SDDS_printNamelistField(gzfp, (char*)"type", SDDS_getVer2TypeName(parameterType))) {
      return(1);
    }
  } else {
    if (SDDS_printNamelistField(gzfp, (char*)"type", SDDS_getTypeName(parameterType))) {
      return(1);
    }
  }
  if (SDDS_printNamelistField(gzfp, (char*)"fixed_value", parameterFixedValue)) {
    return(1);
  }
  gzputs(gzfp, "&end\n");
  return(0);
}

/***********************************************************************************************
 *                        writeAsciiValue                                                      *
 *                                                                                             *
 * C++ Arguments: FILE *fp, uint32_t page                                                       *
 * C++ Arguments: voidp *gzfp, uint32_t page                                                    *
 *                                                                                             *
 * Results:  0 on success                                                                      *
 *           1 on failure                                                                      *
 ***********************************************************************************************/
int32_t SDDSParameter::writeAsciiValue(FILE *fp, uint32_t page) {
  char c;
  char *s;
  int32_t hasWhitespace;

  if (fixed) {
    return(0);
  }
  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (parameterType) {
  case SDDS_INT16:
    fprintf(fp, "%" PRId16 "\n", int16Value[page]);
    break;
  case SDDS_UINT16:
    fprintf(fp, "%" PRIu16 "\n", uint16Value[page]);
    break;
  case SDDS_INT32:
    fprintf(fp, "%" PRId32 "\n", int32Value[page]);
    break;
  case SDDS_UINT32:
    fprintf(fp, "%" PRIu32 "\n", uint32Value[page]);
    break;
  case SDDS_REAL32:
    fprintf(fp, "%.*g\n", FLT_DIG, real32Value[page]);
    break;
  case SDDS_REAL64:
    fprintf(fp, "%.*g\n", DBL_DIG, real64Value[page]);
    break;
  case SDDS_STRING:
    s = stringValue[page];
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
    fputc('\n', fp);
    break;
  case SDDS_CHARACTER:
    c = charValue[page];
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
    fputc('\n', fp);
    break;
  default:
    return(1);
  }
  return(0);
}

int32_t SDDSParameter::writeAsciiValue(voidp *gzfp, uint32_t page) {
  char c;
  char *s;
  int32_t hasWhitespace;

  if (fixed) {
    return(0);
  }
  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (parameterType) {
  case SDDS_INT16:
    gzprintf(gzfp, "%" PRId16 "\n", int16Value[page]);
    break;
  case SDDS_UINT16:
    gzprintf(gzfp, "%" PRIu16 "\n", uint16Value[page]);
    break;
  case SDDS_INT32:
    gzprintf(gzfp, "%" PRId32 "\n", int32Value[page]);
    break;
  case SDDS_UINT32:
    gzprintf(gzfp, "%" PRIu32 "\n", uint32Value[page]);
    break;
  case SDDS_REAL32:
    gzprintf(gzfp, "%.*g\n", FLT_DIG, real32Value[page]);
    break;
  case SDDS_REAL64:
    gzprintf(gzfp, "%.*g\n", DBL_DIG, real64Value[page]);
    break;
  case SDDS_STRING:
    s = stringValue[page];
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
    gzputc(gzfp, '\n');
    break;
  case SDDS_CHARACTER:
    c = charValue[page];
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
    gzputc(gzfp, '\n');
    break;
  default:
    return(1);
  }
  return(0);
}

/***********************************************************************************************
 *                        writeBinaryValue                                                     *
 *                                                                                             *
 * C++ Arguments: FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian       *
 * C++ Arguments: voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian    *
 *                                                                                             *
 * Results:  0 on success                                                                      *
 *           1 on failure                                                                      *
 ***********************************************************************************************/
int32_t SDDSParameter::writeBinaryValue(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  if (fixed) {
    return(0);
  }
  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (parameterType) {
  case SDDS_INT16:
    if (SDDS_bufferedWrite(&(int16Value[page]), 2, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    if (SDDS_bufferedWrite(&(uint16Value[page]), 2, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_INT32:
    if (SDDS_bufferedWrite(&(int32Value[page]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    if (SDDS_bufferedWrite(&(uint32Value[page]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    if (SDDS_bufferedWrite(&(real32Value[page]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    if (SDDS_bufferedWrite(&(real64Value[page]), 8, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_STRING:
    if (SDDS_writeBinaryString(stringValue[page], fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing string (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_CHARACTER:
    if (SDDS_bufferedWrite(&(charValue[page]), 1, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--unknown type (SDDSParameter::writeBinaryValue)");
    return(1);
  }
  return(0);
}

int32_t SDDSParameter::writeBinaryValue(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  if (fixed) {
    return(0);
  }
  if ((page == 0) || (page > pageCount)) {
    return(1);
  }
  page--;
  switch (parameterType) {
  case SDDS_INT16:
    if (SDDS_bufferedWrite(&(int16Value[page]), 2, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    if (SDDS_bufferedWrite(&(uint16Value[page]), 2, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_INT32:
    if (SDDS_bufferedWrite(&(int32Value[page]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    if (SDDS_bufferedWrite(&(uint32Value[page]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    if (SDDS_bufferedWrite(&(real32Value[page]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    if (SDDS_bufferedWrite(&(real64Value[page]), 8, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_STRING:
    if (SDDS_writeBinaryString(stringValue[page], gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing string (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  case SDDS_CHARACTER:
    if (SDDS_bufferedWrite(&(charValue[page]), 1, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--failure writing value (SDDSParameter::writeBinaryValue)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to write parameter--unknown type (SDDSParameter::writeBinaryValue)");
    return(1);
  }
  return(0);
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
int32_t SDDSParameter::readBinaryValue(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t p;

  if (fixed) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--parameter is fixed (SDDSParameter::readBinaryValue)");
    return(1);
  }
  if ((page == 0) || (page - 1 != pageCount)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--invalid page number (SDDSParameter::readBinaryValue)");
    return(1);
  }
  p = page - 1;
  switch (parameterType) {
  case SDDS_INT16:
    int16Value = (int16_t*)SDDS_realloc(int16Value, sizeof(int16_t) * page);
    if (SDDS_bufferedRead(&(int16Value[p]), 2, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    uint16Value = (uint16_t*)SDDS_realloc(uint16Value, sizeof(uint16_t) * page);
    if (SDDS_bufferedRead(&(uint16Value[p]), 2, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_INT32:
    int32Value = (int32_t*)SDDS_realloc(int32Value, sizeof(int32_t) * page);
    if (SDDS_bufferedRead(&(int32Value[p]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    uint32Value = (uint32_t*)SDDS_realloc(uint32Value, sizeof(uint32_t) * page);
    if (SDDS_bufferedRead(&(uint32Value[p]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    real32Value = (float*)SDDS_realloc(real32Value, sizeof(float) * page);
    if (SDDS_bufferedRead(&(real32Value[p]), 4, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    real64Value = (double*)SDDS_realloc(real64Value, sizeof(double) * page);
    if (SDDS_bufferedRead(&(real64Value[p]), 8, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_STRING:
    stringValue = (char**)SDDS_realloc(stringValue, sizeof(char*) * page);
    stringValue[p] = NULL;
    if (SDDS_readBinaryString(&(stringValue[p]), fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading string (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_CHARACTER:
    charValue = (char*)SDDS_realloc(charValue, sizeof(char) * page);
    if (SDDS_bufferedRead(&(charValue[p]), 1, fp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--unknown type (SDDSParameter::readBinaryValue)");
    return(1);
  }
  pageCount = page;
  return(0);
}

int32_t SDDSParameter::readBinaryValue(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian) {
  uint32_t p;

  if (fixed) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--parameter is fixed (SDDSParameter::readBinaryValue)");
    return(1);
  }
  if ((page == 0) || (page - 1 != pageCount)) {
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--invalid page number (SDDSParameter::readBinaryValue)");
    return(1);
  }
  p = page - 1;
  switch (parameterType) {
  case SDDS_INT16:
    int16Value = (int16_t*)SDDS_realloc(int16Value, sizeof(int16_t) * page);
    if (SDDS_bufferedRead(&(int16Value[p]), 2, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT16:
    uint16Value = (uint16_t*)SDDS_realloc(uint16Value, sizeof(uint16_t) * page);
    if (SDDS_bufferedRead(&(uint16Value[p]), 2, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_INT32:
    int32Value = (int32_t*)SDDS_realloc(int32Value, sizeof(int32_t) * page);
    if (SDDS_bufferedRead(&(int32Value[p]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_UINT32:
    uint32Value = (uint32_t*)SDDS_realloc(uint32Value, sizeof(uint32_t) * page);
    if (SDDS_bufferedRead(&(uint32Value[p]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL32:
    real32Value = (float*)SDDS_realloc(real32Value, sizeof(float) * page);
    if (SDDS_bufferedRead(&(real32Value[p]), 4, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_REAL64:
    real64Value = (double*)SDDS_realloc(real64Value, sizeof(double) * page);
    if (SDDS_bufferedRead(&(real64Value[p]), 8, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_STRING:
    stringValue = (char**)SDDS_realloc(stringValue, sizeof(char*) * page);
    stringValue[p] = NULL;
    if (SDDS_readBinaryString(&(stringValue[p]), gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading string (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  case SDDS_CHARACTER:
    charValue = (char*)SDDS_realloc(charValue, sizeof(char) * page);
    if (SDDS_bufferedRead(&(charValue[p]), 1, gzfp, fBuffer, sddsfile)) {
      ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--failure reading value (SDDSParameter::readBinaryValue)");
      return(1);
    }
    break;
  default:
    ((SDDSFile*)sddsfile)->setError((char*)"Unable to read parameter--unknown type (SDDSParameter::readBinaryValue)");
    return(1);
  }
  pageCount = page;
  return(0);
}
