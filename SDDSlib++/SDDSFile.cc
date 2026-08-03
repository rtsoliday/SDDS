/* TODO
   Add fixed row count support
   Add ability to open .F and .Z files using freeze and uncompress
   Add better support for different row counts while an SDDS file is being edited
   Add recovery mode support
   Add rowlimit support
   Add lines per row support for output files
   Add array type support
   Add more setparameter types
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#if defined(__BORLANDC__)
#define _setmode(handle, amode) setmode(handle, amode)
#endif
#endif

#include "SDDS3.h"
#include "mdb.h"

SDDSFile::SDDSFile() {
  fileName = NULL;
  bigEndian = (bool)SDDS_isBigEndianMachine();
  n_errors = 0;
  asciiFile = true;
  noRowCount = false;
  linesPerRow = 1;
  additionalHeaderLines = 0;
  isURL = false;
  descriptionText = NULL;
  descriptionContents = NULL;
  arrayCount = 0;
  arrayObject = NULL;
  parameterCount = 0;
  parameterObject = NULL;
  columnCount = 0;
  columnObject = NULL;
  n_errors = 0;
  error_description = NULL;
  gzipFile = false;
  fp = NULL;
  gzfp = NULL;
  layoutVersion = SDDS_VERSION;
  defaultIOBufferSize = 262144;
  recoveryPossible = 0;
  columnMajorOrder = true;
}

SDDSFile::SDDSFile(char *filename) {
  fileName = (char*)SDDS_malloc(sizeof(char) * strlen(filename) + 1);
  strcpy(fileName, filename);
  bigEndian = (bool)SDDS_isBigEndianMachine();
  n_errors = 0;
  asciiFile = true;
  noRowCount = false;
  linesPerRow = 1;
  additionalHeaderLines = 0;
  isURL = false;
  descriptionText = NULL;
  descriptionContents = NULL;
  arrayCount = 0;
  arrayObject = NULL;
  parameterCount = 0;
  parameterObject = NULL;
  columnCount = 0;
  columnObject = NULL;
  n_errors = 0;
  error_description = NULL;
  gzipFile = false;
  fp = NULL;
  gzfp = NULL;
  layoutVersion = SDDS_VERSION;
  defaultIOBufferSize = 262144;
  recoveryPossible = 0;
  columnMajorOrder = true;
}

SDDSFile::SDDSFile(bool binary) {
  fileName = NULL;
  bigEndian = (bool)SDDS_isBigEndianMachine();
  n_errors = 0;
  asciiFile = !binary;
  noRowCount = false;
  linesPerRow = 1;
  additionalHeaderLines = 0;
  isURL = false;
  descriptionText = NULL;
  descriptionContents = NULL;
  arrayCount = 0;
  arrayObject = NULL;
  parameterCount = 0;
  parameterObject = NULL;
  columnCount = 0;
  columnObject = NULL;
  n_errors = 0;
  error_description = NULL;
  gzipFile = false;
  fp = NULL;
  gzfp = NULL;
  layoutVersion = SDDS_VERSION;
  defaultIOBufferSize = 262144;
  recoveryPossible = 0;
  columnMajorOrder = true;
}

SDDSFile::SDDSFile(char *filename, bool binary) {
  fileName = (char*)SDDS_malloc(sizeof(char) * strlen(filename) + 1);
  strcpy(fileName, filename);
  bigEndian = (bool)SDDS_isBigEndianMachine();
  n_errors = 0;
  asciiFile = !binary;
  noRowCount = false;
  linesPerRow = 1;
  additionalHeaderLines = 0;
  isURL = false;
  descriptionText = NULL;
  descriptionContents = NULL;
  arrayCount = 0;
  arrayObject = NULL;
  parameterCount = 0;
  parameterObject = NULL;
  columnCount = 0;
  columnObject = NULL;
  n_errors = 0;
  error_description = NULL;
  gzipFile = false;
  fp = NULL;
  gzfp = NULL;
  layoutVersion = SDDS_VERSION;
  defaultIOBufferSize = 262144;
  recoveryPossible = 0;
  columnMajorOrder = true;
}

/***********************************************************************************************
 *                       SDDSFile_new                                                          *
 *                                                                                             *
 * C Arguments: int32_t mode, ...                                                              *
 *              mode=SDDSFILE_NEW_DEFAULT                                                      *
 *              mode=SDDSFILE_NEW_WITH_FILENAME, char *filename                                *
 *              mode=SDDSFILE_NEW_WITH_MODE, int32_t binary                                    *
 *              mode=SDDSFILE_NEW_WITH_FILENAME+SDDSFILE_NEW_WITH_MODE,                        *
 *                   char *filename, int32_t binary                                            *
 *                                                                                             *
 * Results:  pointer to SDDSFile object                                                        *
 ***********************************************************************************************/
void *SDDSFile_new(int32_t mode, ...) {
  va_list ap;
  char *filename;
  int32_t binary;

  if (mode&SDDSFILE_NEW_WITH_FILENAME) {
    if (mode&SDDSFILE_NEW_WITH_MODE) {
      va_start(ap, mode);
      filename = va_arg(ap, char *);
      binary = va_arg(ap, int32_t);
      if (binary == SDDS_ASCII)
	binary = 0;
      return((void*) new SDDSFile(filename, binary));
    } else {
      va_start(ap, mode);
      filename = va_arg(ap, char *);
      return((void*) new SDDSFile(filename));
    }
  } else if (mode&SDDSFILE_NEW_WITH_MODE) {
    va_start(ap, mode);
    binary = va_arg(ap, int32_t);
    if (binary == SDDS_ASCII)
      binary = 0;
    return((void*) new SDDSFile(binary));
  } else {
    return((void*) new SDDSFile());
  }
}

SDDSFile::~SDDSFile() {
  int32_t i;
  if (fileName != NULL)
    SDDS_free(fileName);
  if (descriptionText != NULL)
    SDDS_free(descriptionText);
  if (descriptionContents != NULL)
    SDDS_free(descriptionContents);
  for (i=0 ; i<arrayCount ; i++) {
    delete arrayObject[i];
  }
  SDDS_free(arrayObject);
  for (i=0 ; i<parameterCount ; i++) {
    delete parameterObject[i];
  }
  SDDS_free(parameterObject);
  for (i=0 ; i<columnCount ; i++) {
    delete columnObject[i];
  }
  SDDS_free(columnObject);
}

/*
 *  SDDSFile_delete used to delete an SDDSFile object.
 *
 */
void SDDSFile_delete(void* sddsfile) {
  delete (SDDSFile*)sddsfile;
}

/***********************************************************************************************
 *                          setFileName                                                        *
 *                                                                                             *
 * C++ Arguments: char *filename                                                               *
 *                                                                                             *
 * C Arguments: void *sddsfile, char *filename                                                 *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setFileName(char *filename) {
  if (fileName != NULL)
    SDDS_free(fileName);
  if (filename == NULL)
    fileName = NULL;
  else {
    fileName = (char*)SDDS_malloc(sizeof(char) * strlen(filename) + 1);
    strcpy(fileName, filename);
  }
}
void SDDSFile_setFileName(void *sddsfile, char *filename) {
  ((SDDSFile*)sddsfile)->setFileName(filename);
}

/***********************************************************************************************
 *                          getDataMode                                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  SDDS_BINARY or SDDS_ASCII                                                         *
 ***********************************************************************************************/
uint32_t SDDSFile::getDataMode() {
  if (asciiFile) {
    return(SDDS_ASCII);
  } else {
    return(SDDS_BINARY);
  }
}
uint32_t SDDSFile_getDataMode(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getDataMode());
}

/***********************************************************************************************
 *                          setDataMode                                                        *
 *                                                                                             *
 * C++ Arguments: uint32_t mode                                                                *
 *                                                                                             *
 * C Arguments: void *sddsfile, uint32_t mode                                                  *
 *                                                                                             *
 * Results:  SDDS_BINARY or SDDS_ASCII                                                         *
 ***********************************************************************************************/
void SDDSFile::setDataMode(uint32_t mode) {
  if (mode == SDDS_ASCII) {
    asciiFile = true;
  } else {
    asciiFile = false;
  }
}
void SDDSFile_setDataMode(void *sddsfile, uint32_t mode) {
  ((SDDSFile*)sddsfile)->setDataMode(mode);
}

/***********************************************************************************************
 *                          setLayoutVersion                                                   *
 *                                                                                             *
 * C++ Arguments: int32_t version                                                              *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t version                                                *
 *                                                                                             *
 * Results:  None                                                                              *
 ***********************************************************************************************/
void SDDSFile::setLayoutVersion(int32_t version) {
  if ((version >= 1) && (version <= SDDS_VERSION)) {
    layoutVersion = version;
  }
}
void SDDSFile_setLayoutVersion(void *sddsfile, int32_t version) {
  ((SDDSFile*)sddsfile)->setLayoutVersion(version);
}

/***********************************************************************************************
 *                          setColumnMajorOrder                                                *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  None                                                                              *
 ***********************************************************************************************/
void SDDSFile::setColumnMajorOrder() {
  columnMajorOrder = true;
}
void SDDSFile_setColumnMajorOrder(void *sddsfile) {
  ((SDDSFile*)sddsfile)->setColumnMajorOrder();
}

/***********************************************************************************************
 *                          setRowMajorOrder                                                   *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  None                                                                              *
 ***********************************************************************************************/
void SDDSFile::setRowMajorOrder() {
  columnMajorOrder = false;
}
void SDDSFile_setRowMajorOrder(void *sddsfile) {
  ((SDDSFile*)sddsfile)->setRowMajorOrder();
}

/***********************************************************************************************
 *                          setBinaryMode                                                      *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setBinaryMode() {
  asciiFile = false;
}
void SDDSFile_setBinaryMode(void *sddsfile) {
  ((SDDSFile*)sddsfile)->setBinaryMode();
}

/***********************************************************************************************
 *                          setAsciiMode                                                      *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setAsciiMode() {
  asciiFile = true;
}
void SDDSFile_setAsciiMode(void *sddsfile) {
  ((SDDSFile*)sddsfile)->setAsciiMode();
}

/***********************************************************************************************
 *                        setNonNativeEndian                                                   *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setNonNativeEndian() {
  bigEndian = (bool)(!SDDS_isBigEndianMachine());
}
void SDDSFile_setNonNativeEndian(void *sddsfile) {
  ((SDDSFile*)sddsfile)->setNonNativeEndian();
}

/***********************************************************************************************
 *                        setNativeEndian                                                   *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setNativeEndian() {
  bigEndian = (bool)(SDDS_isBigEndianMachine());
}
void SDDSFile_setNativeEndian(void *sddsfile) {
  ((SDDSFile*)sddsfile)->setNativeEndian();
}

/***********************************************************************************************
 *                           setNoRowCount                                                     *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setNoRowCount() {
  noRowCount = true;
}
void SDDSFile_setNoRowCount(void *sddsfile) {
  ((SDDSFile*)sddsfile)->setNoRowCount();
}

/***********************************************************************************************
 *                           setUseRowCount                                                    *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setUseRowCount() {
  noRowCount = true;
}
void SDDSFile_setUseRowCount(void *sddsfile) {
  ((SDDSFile*)sddsfile)->setUseRowCount();
}

/***********************************************************************************************
 *                               setDescription                                                *
 *                                                                                             *
 * C++ Arguments: char *text, char *contents                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, char *text, char *contents                                     *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setDescription(char *text, char *contents) {
  if (descriptionText != NULL)
    SDDS_free(descriptionText);
  if (descriptionContents != NULL)
    SDDS_free(descriptionContents);
  if (text == NULL)
    descriptionText = NULL;
  else {
    descriptionText = (char*)SDDS_malloc(sizeof(char) * strlen(text) + 1);
    strcpy(descriptionText, text);
  }
  if (contents == NULL)
    descriptionContents = NULL;
  else {
    descriptionContents = (char*)SDDS_malloc(sizeof(char) * strlen(contents) + 1);
    strcpy(descriptionContents, contents);
  }
}
void SDDSFile_setDescription(void *sddsfile, char *text, char *contents) {
  ((SDDSFile*)sddsfile)->setDescription(text, contents);
}

/***********************************************************************************************
 *                               getDescription                                                *
 *                                                                                             *
 * C++ Arguments: char **text, char **contents                                                 *
 *                                                                                             *
 * C Arguments: void *sddsfile, char **text, char **contents                                   *
 *                                                                                             *
 * Results:  0 on success                                                                      *
 *           1 on failure                                                                      *
 ***********************************************************************************************/
int32_t SDDSFile::getDescription(char **text, char **contents) {
  if (descriptionText != NULL) {
    *text = NULL;
    if (SDDS_copyString(text, descriptionText)) {
      setError((char*)"Unable to retrieve description data (SDDSFile::getDescription)");
      return(1);
    }
  }
  if (descriptionContents != NULL) {
    *contents = NULL;
    if (SDDS_copyString(contents, descriptionContents)) {
      setError((char*)"Unable to retrieve description data (SDDSFile::getDescription)");
      return(1);
    }
  }
  return(0);
}
int32_t SDDSFile_getDescription(void *sddsfile, char **text, char **contents) {
  return(((SDDSFile*)sddsfile)->getDescription(text, contents));
}

/*
 * defineArray
 *
 * C++ Arguments: char *name, char *symbol, char *units, char *description,
 *                  char *format_string, char *group_name, int32_t type, 
 *                  uint32_t field_length, uint32_t dimensions
 *                char *name, char *symbol, char *units, char *description,
 *                  char *format_string, char *group_name, char *type, 
 *                  uint32_t field_length, uint32_t dimensions
 *                char *name, int32_t type, uint32_t dimensions
 *                char *name, char *type, uint32_t dimensions
 *
 * C Arguments: void *sddsfile, int32_t mode, ...
 *              mode=SDDSFILE_DEFINEARRAY_DEFAULT, char *name, int32_t type, uint32_t dimensions
 *              mode=SDDSFILE_DEFINEARRAY_WITH_STRINGTYPE, char *name, char *type, uint32_t dimensions
 *              mode=SDDSFILE_DEFINEARRAY_EXTENSIVE,
 *                   char *name, char *symbol, char *units, char *description,
 *                   char *format_string, char *group_name, int32_t type, 
 *                   uint32_t field_length, uint32_t dimensions
 *              mode=SDDSFILE_DEFINEARRAY_WITH_STRINGTYPE+SDDSFILE_DEFINEARRAY_EXTENSIVE,
 *                   char *name, char *symbol, char *units, char *description,
 *                   char *format_string, char *group_name, char *type, 
 *                   uint32_t field_length, uint32_t dimensions
 *
 * Results: array index on success
 *          -1 on failure (calling program should call printErrors)
 */
int32_t SDDSFile::defineArray(char *name, char *symbol, char *units, char *description, 
			      char *format_string, char *group_name, int32_t type, 
			      uint32_t field_length, uint32_t dimensions) {
  int32_t i, j;
  char s[SDDS_MAXLINE];

  i = arrayCount + 1;
  if (name == NULL) {
    setError((char*)"Warning: NULL name not allowed for array definition (SDDSFile::defineArray)");
    return(-1);
  }
  if (!SDDS_isValidName(name)) {
    sprintf(s, "Warning: The following array name is invalid: >%s< (SDDSFile::defineArray)", name);
    setError(s);
    return(-1);
  }
  for (j=0 ; j<arrayCount ; j++) {
    if (strcmp(name, arrayObject[j]->getName()) == 0) {
      sprintf(s, "Warning: array %s already exists (SDDSFile::defineArray)", name);
      setError(s);
      return(-1);
    }
  }
  if (!SDDS_isValidType(type)) {
    setError((char*)"Warning: unknown data type (SDDSFile::defineArray)");
    return(-1);
  }
  if (format_string != NULL) {
    if (!SDDS_verifyPrintfFormat(format_string, type)) {
      setError((char*)"Warning: invalid format string (SDDSFile::defineArray)");
      return(-1);
    }
  }
  arrayObject = (SDDSArray**)SDDS_realloc(arrayObject, sizeof(SDDSArray*) * (arrayCount + 1));
  arrayObject[arrayCount] = new SDDSArray();
  arrayObject[arrayCount]->setupArray(name, symbol, units, description, format_string, group_name, type, field_length, dimensions);
  arrayObject[arrayCount]->informAboutParent(this);
  arrayCount++;
  return(arrayCount);
}

int32_t SDDSFile::defineArray(char *name, char *symbol, char *units, char *description, 
			      char *format_string, char *group_name,  char *type, 
			      uint32_t field_length, uint32_t dimensions) {
  return(defineArray(name, symbol, units, description, format_string, group_name, SDDS_identifyType(type), field_length, dimensions));
}

int32_t SDDSFile::defineArray(char *name, int32_t type, uint32_t dimensions) {
  return(defineArray(name, NULL, NULL, NULL, NULL, NULL, type, 0, dimensions));
}

int32_t SDDSFile::defineArray(char *name, char *type, uint32_t dimensions) {
  return(defineArray(name, NULL, NULL, NULL, NULL, NULL, SDDS_identifyType(type), 0, dimensions));
}

int32_t SDDSFile_defineArray(void *sddsfile, int32_t mode, ...) {
  va_list ap;
  char *name;
  char *symbol;
  char *units;
  char *description;
  char *format_string;
  char *group_name;
  char *typeChar;
  int32_t typeInt;
  uint32_t field_length;
  uint32_t dimensions;

  va_start(ap, mode);
  if (mode&SDDSFILE_DEFINEARRAY_EXTENSIVE) {
    if (mode&SDDSFILE_DEFINEARRAY_WITH_STRINGTYPE) {
      name = va_arg(ap, char *);
      symbol = va_arg(ap, char *);
      units = va_arg(ap, char *);
      description = va_arg(ap, char *);
      format_string = va_arg(ap, char *);
      group_name = va_arg(ap, char *);
      typeChar = va_arg(ap, char *);
      field_length = va_arg(ap, uint32_t);
      dimensions = va_arg(ap, uint32_t);
      return(((SDDSFile*)sddsfile)->defineArray(name, symbol, units, description, format_string, group_name, typeChar, field_length, dimensions));
    } else {
      name = va_arg(ap, char *);
      symbol = va_arg(ap, char *);
      units = va_arg(ap, char *);
      description = va_arg(ap, char *);
      format_string = va_arg(ap, char *);
      group_name = va_arg(ap, char *);
      typeInt = va_arg(ap, int32_t);
      field_length = va_arg(ap, uint32_t);
      dimensions = va_arg(ap, uint32_t);
      return(((SDDSFile*)sddsfile)->defineArray(name, symbol, units, description, format_string, group_name, typeInt, field_length, dimensions));
    }
  } else if (mode&SDDSFILE_DEFINEARRAY_WITH_STRINGTYPE) {
    name = va_arg(ap, char *);
    typeChar = va_arg(ap, char *);
    dimensions = va_arg(ap, uint32_t);
    return(((SDDSFile*)sddsfile)->defineArray(name, typeChar, dimensions));
  } else {
    name = va_arg(ap, char *);
    typeInt = va_arg(ap, int32_t);
    dimensions = va_arg(ap, uint32_t);
    return(((SDDSFile*)sddsfile)->defineArray(name, typeInt, dimensions));
  }
}

/****************************************************************************************************
 *                           defineParameter                                                        *
 *                                                                                                  *
 * C++ Arguments: char *name, char *symbol, char *units, char *description,                         *
 *                  char *format_string, int32_t type, char *fixed_value                            *
 *                char *name, char *symbol, char *units, char *description,                         *
 *                  char *format_string, char *type, char *fixed_value                              *
 *                char *name, int32_t type                                                          *
 *                char *name, char *type                                                            *
 *                                                                                                  *
 * C Arguments: void *sddsfile, int32_t mode, ...                                                   *
 *              mode=SDDSFILE_DEFINEPARAMETER_DEFAULT, char *name, int32_t type                     *
 *              mode=SDDSFILE_DEFINEPARAMETER_WITH_STRINGTYPE, char *name, char *type               *
 *              mode=SDDSFILE_DEFINEPARAMETER_EXTENSIVE,                                            *
 *                   char *name, char *symbol, char *units, char *description,                      *
 *                   char *format_string, int32_t type, char *fixed_value                           *
 *              mode=SDDSFILE_DEFINEPARAMETER_WITH_STRINGTYPE+SDDSFILE_DEFINEPARAMETER_EXTENSIVE,   *
 *                   char *name, char *symbol, char *units, char *description,                      *
 *                   char *format_string, char *type, char *fixed_value                             *
 *                                                                                                  *
 * Results: parameter index on success                                                              *
 *          -1 on failure (calling program should call printErrors)                                 *
 ****************************************************************************************************/
int32_t SDDSFile::defineParameter(char *name, char *symbol, char *units, char *description, 
				  char *format_string, int32_t type, char *fixed_value) {
  int32_t i, j;
  char s[SDDS_MAXLINE];

  i = parameterCount + 1;
  if (name == NULL) {
    setError((char*)"Warning: NULL name not allowed for parameter definition (SDDSFile::defineParameter)");
    return(-1);
  }
  if (!SDDS_isValidName(name)) {
    sprintf(s, "Warning: The following parameter name is invalid: >%s< (SDDSFile::defineParameter)", name);
    setError(s);
    return(-1);
  }
  for (j=0 ; j<parameterCount ; j++) {
    if (strcmp(name, parameterObject[j]->getName()) == 0) {
      sprintf(s, "Warning: parameter %s already exists (SDDSFile::defineParameter)", name);
      setError(s);
      return(-1);
    }
  }
  if (!SDDS_isValidType(type)) {
    setError((char*)"Warning: unknown data type (SDDSFile::defineParameter)");
    return(-1);
  }
  if (format_string != NULL) {
    if (!SDDS_verifyPrintfFormat(format_string, type)) {
      setError((char*)"Warning: invalid format string (SDDSFile::defineParameter)");
      return(-1);
    }
  }
  parameterObject = (SDDSParameter**)SDDS_realloc(parameterObject, sizeof(SDDSParameter*) * (parameterCount + 1));
  parameterObject[parameterCount] = new SDDSParameter();
  parameterObject[parameterCount]->setupParameter(name, symbol, units, description, format_string, type, fixed_value);
  parameterObject[parameterCount]->informAboutParent(this);
  parameterCount++;
  return(parameterCount);
}

int32_t SDDSFile::defineParameter(char *name, char *symbol, char *units, char *description, 
				  char *format_string, char *type, char *fixed_value) {
  return(defineParameter(name, symbol, units, description, format_string, SDDS_identifyType(type), fixed_value));
}

int32_t SDDSFile::defineParameter(char *name, int32_t type) {
  return(defineParameter(name, NULL, NULL, NULL, NULL, type, NULL));
}

int32_t SDDSFile::defineParameter(char *name, char *type) {
  return(defineParameter(name, NULL, NULL, NULL, NULL, SDDS_identifyType(type), NULL));
}

int32_t SDDSFile_defineParameter(void *sddsfile, int32_t mode, ...) {
  va_list ap;
  char *name;
  char *symbol;
  char *units;
  char *description;
  char *format_string;
  char *typeChar;
  int32_t typeInt;
  char *fixed_value;

  va_start(ap, mode);
  if (mode&SDDSFILE_DEFINEPARAMETER_EXTENSIVE) {
    if (mode&SDDSFILE_DEFINEPARAMETER_WITH_STRINGTYPE) {
      name = va_arg(ap, char *);
      symbol = va_arg(ap, char *);
      units = va_arg(ap, char *);
      description = va_arg(ap, char *);
      format_string = va_arg(ap, char *);
      typeChar = va_arg(ap, char *);
      fixed_value = va_arg(ap, char *);
      return(((SDDSFile*)sddsfile)->defineParameter(name, symbol, units, description, format_string, typeChar, fixed_value));
    } else {
      name = va_arg(ap, char *);
      symbol = va_arg(ap, char *);
      units = va_arg(ap, char *);
      description = va_arg(ap, char *);
      format_string = va_arg(ap, char *);
      typeInt = va_arg(ap, int32_t);
      fixed_value = va_arg(ap, char *);
      return(((SDDSFile*)sddsfile)->defineParameter(name, symbol, units, description, format_string, typeInt, fixed_value));
    }
  } else if (mode&SDDSFILE_DEFINEPARAMETER_WITH_STRINGTYPE) {
    name = va_arg(ap, char *);
    typeChar = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->defineParameter(name, typeChar));
  } else {
    name = va_arg(ap, char *);
    typeInt = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->defineParameter(name, typeInt));
  }
}

/****************************************************************************************************
 *                           defineColumn                                                           *
 *                                                                                                  *
 * C++ Arguments: char *name, char *symbol, char *units, char *description,                         *
 *                  char *format_string, int32_t type, uint32_t field_length                         *
 *                char *name, char *symbol, char *units, char *description,                         *
 *                  char *format_string, char *type, uint32_t field_length                           *
 *                char *name, int32_t type                                                          *
 *                char *name, char *type                                                            *
 *                                                                                                  *
 * C Arguments: void *sddsfile, int32_t mode, ...                                                   *
 *              mode=SDDSFILE_DEFINECOLUMN_DEFAULT, char *name, int32_t type                        *
 *              mode=SDDSFILE_DEFINECOLUMN_WITH_STRINGTYPE, char *name, char *type                  *
 *              mode=SDDSFILE_DEFINECOLUMN_EXTENSIVE,                                               *
 *                   char *name, char *symbol, char *units, char *description,                      *
 *                   char *format_string, int32_t type, uint32_t field_length                        *
 *              mode=SDDSFILE_DEFINECOLUMN_WITH_STRINGTYPE+SDDSFILE_DEFINECOLUMN_EXTENSIVE,         *
 *                   char *name, char *symbol, char *units, char *description,                      *
 *                   char *format_string, char *type, uint32_t field_length                          *
 *                                                                                                  *
 * Results: column index on success                                                                 *
 *          -1 on failure (calling program should call printErrors)                                 *
 ****************************************************************************************************/
int32_t SDDSFile::defineColumn(char *name, char *symbol, char *units, char *description, 
				  char *format_string, int32_t type, uint32_t field_length) {
  int32_t i, j;
  char s[SDDS_MAXLINE];

  i = columnCount + 1;
  if (name == NULL) {
    setError((char*)"Warning: NULL name not allowed for column definition (SDDSFile::defineColumn)");
    return(-1);
  }
  if (!SDDS_isValidName(name)) {
    sprintf(s, "Warning: The following column name is invalid: >%s< (SDDSFile::defineColumn)", name);
    setError(s);
    return(-1);
  }
  for (j=0 ; j<columnCount ; j++) {
    if (strcmp(name, columnObject[j]->getName()) == 0) {
      sprintf(s, "Warning: column %s already exists (SDDSFile::defineColumn)", name);
      setError(s);
      return(-1);
    }
  }
  if (!SDDS_isValidType(type)) {
    setError((char*)"Warning: unknown data type (SDDSFile::defineColumn)");
    return(-1);
  }
  if (format_string != NULL) {
    if (!SDDS_verifyPrintfFormat(format_string, type)) {
      setError((char*)"Warning: invalid format string (SDDSFile::defineColumn)");
      return(-1);
    }
  }
  columnObject = (SDDSColumn**)SDDS_realloc(columnObject, sizeof(SDDSColumn*) * (columnCount + 1));
  columnObject[columnCount] = new SDDSColumn();
  columnObject[columnCount]->setupColumn(name, symbol, units, description, format_string, type, field_length);
  columnObject[columnCount]->informAboutParent(this);
  columnCount++;
  return(columnCount);
}

int32_t SDDSFile::defineColumn(char *name, char *symbol, char *units, char *description, 
				  char *format_string, char *type, uint32_t field_length) {
  return(defineColumn(name, symbol, units, description, format_string, SDDS_identifyType(type), field_length));
}

int32_t SDDSFile::defineColumn(char *name, int32_t type) {
  return(defineColumn(name, NULL, NULL, NULL, NULL, type, 0));
}

int32_t SDDSFile::defineColumn(char *name, char *type) {
  return(defineColumn(name, NULL, NULL, NULL, NULL, SDDS_identifyType(type), 0));
}

int32_t SDDSFile_defineColumn(void *sddsfile, int32_t mode, ...) {
  va_list ap;
  char *name;
  char *symbol;
  char *units;
  char *description;
  char *format_string;
  char *typeChar;
  int32_t typeInt;
  uint32_t field_length;

  va_start(ap, mode);
  if (mode&SDDSFILE_DEFINECOLUMN_EXTENSIVE) {
    if (mode&SDDSFILE_DEFINECOLUMN_WITH_STRINGTYPE) {
      name = va_arg(ap, char *);
      symbol = va_arg(ap, char *);
      units = va_arg(ap, char *);
      description = va_arg(ap, char *);
      format_string = va_arg(ap, char *);
      typeChar = va_arg(ap, char *);
      field_length = va_arg(ap, uint32_t);
      return(((SDDSFile*)sddsfile)->defineColumn(name, symbol, units, description, format_string, typeChar, field_length));
    } else {
      name = va_arg(ap, char *);
      symbol = va_arg(ap, char *);
      units = va_arg(ap, char *);
      description = va_arg(ap, char *);
      format_string = va_arg(ap, char *);
      typeInt = va_arg(ap, int32_t);
      field_length = va_arg(ap, uint32_t);
      return(((SDDSFile*)sddsfile)->defineColumn(name, symbol, units, description, format_string, typeInt, field_length));
    }
  } else if (mode&SDDSFILE_DEFINECOLUMN_WITH_STRINGTYPE) {
    name = va_arg(ap, char *);
    typeChar = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->defineColumn(name, typeChar));
  } else {
    name = va_arg(ap, char *);
    typeInt = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->defineColumn(name, typeInt));
  }
}

/*
 * defineArrays
 *
 * C++ Arguments: int32_t number, char **name, char **symbol, char **units, 
 *                  char **description, char **format_string, char **group_name, 
 *                  int32_t *type, uint32_t *field_length, uint32_t *dimensions
 *                int32_t number, char **name, char **symbol, char **units, 
 *                  char **description, char **format_string, char **group_name,
 *                  char **type, uint32_t *field_length, uint32_t *dimensions
 *                int32_t number, char **name, int32_t *type, uint32_t *dimensions
 *                int32_t number, char **name, char **type, uint32_t *dimensions
 *
 * Results: 0 on success
 *          1 on failure (calling program should call printErrors)
 */
int32_t SDDSFile::defineArrays(int32_t number, char **name, char **symbol, 
			       char **units, char **description, char **format_string, 
			       char **group_name, int32_t *type, uint32_t *field_length,
			       uint32_t *dimensions) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (symbol == NULL)
    return(1);
  if (units == NULL)
    return(1);
  if (description == NULL)
    return(1);
  if (format_string == NULL)
    return(1);
  if (group_name == NULL)
    return(1);
  if (type == NULL)
    return(1);
  if (field_length == NULL)
    return(1);
  if (dimensions == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineArray(name[i], symbol[i], units[i], description[i], format_string[i], group_name[i], type[i], field_length[i], dimensions[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

int32_t SDDSFile::defineArrays(int32_t number, char **name, char **symbol,
			       char **units, char **description, char **format_string, 
			       char **group_name, char **type, uint32_t *field_length,
			       uint32_t *dimensions) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (symbol == NULL)
    return(1);
  if (units == NULL)
    return(1);
  if (description == NULL)
    return(1);
  if (format_string == NULL)
    return(1);
  if (group_name == NULL)
    return(1);
  if (type == NULL)
    return(1);
  if (field_length == NULL)
    return(1);
  if (dimensions == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineArray(name[i], symbol[i], units[i], description[i], format_string[i], group_name[i], type[i], field_length[i], dimensions[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

int32_t SDDSFile::defineArrays(int32_t number, char **name, int32_t *type, uint32_t *dimensions) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (type == NULL)
    return(1);
  if (dimensions == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineArray(name[i], type[i], dimensions[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

int32_t SDDSFile::defineArrays(int32_t number, char **name, char **type, uint32_t *dimensions) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (type == NULL)
    return(1);
  if (dimensions == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineArray(name[i], type[i], dimensions[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

/*
 * defineParameters
 *
 * C++ Arguments: int32_t number, char **name, char **symbol, char **units, char **description,
 *                  char **format_string, int32_t *type, char **fixed_value
 *                int32_t number, char **name, char **symbol, char **units, char **description,
 *                  char **format_string, char **type, char **fixed_value
 *                int32_t number, char **name, int32_t *type
 *                int32_t number, char **name, char **type
 *
 * Results: 0 on success
 *          1 on failure (calling program should call printErrors)
 */
int32_t SDDSFile::defineParameters(int32_t number, char **name, char **symbol, char **units, 
				   char **description, char **format_string, int32_t *type, char **fixed_value) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (symbol == NULL)
    return(1);
  if (units == NULL)
    return(1);
  if (description == NULL)
    return(1);
  if (format_string == NULL)
    return(1);
  if (type == NULL)
    return(1);
  if (fixed_value == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineParameter(name[i], symbol[i], units[i], description[i], format_string[i], type[i], fixed_value[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

int32_t SDDSFile::defineParameters(int32_t number, char **name, char **symbol, char **units, 
				   char **description, char **format_string, char **type, char **fixed_value) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (symbol == NULL)
    return(1);
  if (units == NULL)
    return(1);
  if (description == NULL)
    return(1);
  if (format_string == NULL)
    return(1);
  if (type == NULL)
    return(1);
  if (fixed_value == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineParameter(name[i], symbol[i], units[i], description[i], format_string[i], type[i], fixed_value[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

int32_t SDDSFile::defineParameters(int32_t number, char **name, int32_t *type) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (type == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineParameter(name[i], type[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

int32_t SDDSFile::defineParameters(int32_t number, char **name, char **type) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (type == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineParameter(name[i], type[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

/*
 * defineColumns
 *
 * C++ Arguments: int32_t number, char **name, char **symbol, char **units, char **description,
 *                  char **format_string, int32_t *type, uint32_t *field_length
 *                int32_t number, char **name, char **symbol, char **units, char **description,
 *                  char **format_string, char *type, uint32_t *field_length
 *                int32_t number, char **name, int32_t *type
 *                int32_t number, char **name, char **type
 *
 * Results: 0 on success
 *          1 on failure (calling program should call printErrors)
 */
int32_t SDDSFile::defineColumns(int32_t number, char **name, char **symbol, char **units, 
				   char **description, char **format_string, int32_t *type, uint32_t *field_length) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (symbol == NULL)
    return(1);
  if (units == NULL)
    return(1);
  if (description == NULL)
    return(1);
  if (format_string == NULL)
    return(1);
  if (type == NULL)
    return(1);
  if (field_length == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineColumn(name[i], symbol[i], units[i], description[i], format_string[i], type[i], field_length[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

int32_t SDDSFile::defineColumns(int32_t number, char **name, char **symbol, char **units, 
				   char **description, char **format_string, char **type, uint32_t *field_length) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (symbol == NULL)
    return(1);
  if (units == NULL)
    return(1);
  if (description == NULL)
    return(1);
  if (format_string == NULL)
    return(1);
  if (type == NULL)
    return(1);
  if (field_length == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineColumn(name[i], symbol[i], units[i], description[i], format_string[i], type[i], field_length[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

int32_t SDDSFile::defineColumns(int32_t number, char **name, int32_t *type) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (type == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineColumn(name[i], type[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

int32_t SDDSFile::defineColumns(int32_t number, char **name, char **type) {
  int32_t i;
  if (number < 1)
    return(0);
  if (name == NULL)
    return(1);
  if (type == NULL)
    return(1);
  for (i=0 ; i<number ; i++) {
    if (defineColumn(name[i], type[i]) < 0) {
      return(1);
    }
  }
  return(0);
}

/*
 * setArraySymbol
 *
 * C++ Arguments: int32_t index, char *symbol
 *                char *name, char *symbol
 *
 * C Arguments: void *sddsfile, int32_t index, char *symbol
 *
 * Results:  none
 */
void SDDSFile::setArraySymbol(int32_t index, char *symbol) {
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setSymbol(symbol);
}
void SDDSFile::setArraySymbol(char *name, char *symbol) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setSymbol(symbol);
}
void SDDSFile_setArraySymbol(void *sddsfile, int32_t index, char *symbol) {
  ((SDDSFile*)sddsfile)->setArraySymbol(index, symbol);
}

/*
 * setParameterSymbol
 *
 * C++ Arguments: int32_t index, char *symbol
 *                char *name, char *symbol
 *
 * C Arguments: void *sddsfile, int32_t index, char *symbol
 *
 * Results:  none
 */
void SDDSFile::setParameterSymbol(int32_t index, char *symbol) {
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setSymbol(symbol);
}
void SDDSFile::setParameterSymbol(char *name, char *symbol) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setSymbol(symbol);
}
void SDDSFile_setParameterSymbol(void *sddsfile, int32_t index, char *symbol) {
  ((SDDSFile*)sddsfile)->setParameterSymbol(index, symbol);
}

/***********************************************************************************************
 *                             setColumnSymbol                                                 *
 *                                                                                             *
 * C++ Arguments: int32_t index, char *symbol                                                  *
 *                char *name, char *symbol                                                     *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index, char *symbol                                    *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setColumnSymbol(int32_t index, char *symbol) {
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setSymbol(symbol);
}
void SDDSFile::setColumnSymbol(char *name, char *symbol) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setSymbol(symbol);
}
void SDDSFile_setColumnSymbol(void *sddsfile, int32_t index, char *symbol) {
  ((SDDSFile*)sddsfile)->setColumnSymbol(index, symbol);
}

/*
 * setArrayUnits
 *
 * C++ Arguments: int32_t index, char *units
 *                char *name, char *units
 *
 * C Arguments: void *sddsfile, int32_t index, char *units
 *
 * Results:  none
 */
void SDDSFile::setArrayUnits(int32_t index, char *units) {
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setUnits(units);
}
void SDDSFile::setArrayUnits(char *name, char *units) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setUnits(units);
}
void SDDSFile_setArrayUnits(void *sddsfile, int32_t index, char *units) {
  ((SDDSFile*)sddsfile)->setArrayUnits(index, units);
}

/*
 * setParameterUnits
 *
 * C++ Arguments: int32_t index, char *units
 *                char *name, char *units
 *
 * C Arguments: void *sddsfile, int32_t index, char *units
 *
 * Results:  none
 */
void SDDSFile::setParameterUnits(int32_t index, char *units) {
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setUnits(units);
}
void SDDSFile::setParameterUnits(char *name, char *units) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setUnits(units);
}
void SDDSFile_setParameterUnits(void *sddsfile, int32_t index, char *units) {
  ((SDDSFile*)sddsfile)->setParameterUnits(index, units);
}

/***********************************************************************************************
 *                             setColumnUnits                                                  *
 *                                                                                             *
 * C++ Arguments: int32_t index, char *units                                                   *
 *                char *name, char *units                                                      *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index, char *units                                     *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setColumnUnits(int32_t index, char *units) {
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setUnits(units);
}
void SDDSFile::setColumnUnits(char *name, char *units) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setUnits(units);
}
void SDDSFile_setColumnUnits(void *sddsfile, int32_t index, char *units) {
  ((SDDSFile*)sddsfile)->setColumnUnits(index, units);
}

/*
 * setArrayDescription
 *
 * C++ Arguments: int32_t index, char *description
 *                char *name, char *description
 *
 * C Arguments: void *sddsfile, int32_t index, char *description
 *
 * Results:  none
 */
void SDDSFile::setArrayDescription(int32_t index, char *description) {
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setDescription(description);
}
void SDDSFile::setArrayDescription(char *name, char *description) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setDescription(description);
}
void SDDSFile_setArrayDescription(void *sddsfile, int32_t index, char *description) {
  ((SDDSFile*)sddsfile)->setArrayDescription(index, description);
}

/*
 * setParameterDescription
 *
 * C++ Arguments: int32_t index, char *description
 *                char *name, char *description
 *
 * C Arguments: void *sddsfile, int32_t index, char *description
 *
 * Results:  none
 */
void SDDSFile::setParameterDescription(int32_t index, char *description) {
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setDescription(description);
}
void SDDSFile::setParameterDescription(char *name, char *description) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setDescription(description);
}
void SDDSFile_setParameterDescription(void *sddsfile, int32_t index, char *description) {
  ((SDDSFile*)sddsfile)->setParameterDescription(index, description);
}

/***********************************************************************************************
 *                             setColumnDescription                                            *
 *                                                                                             *
 * C++ Arguments: int32_t index, char *description                                             *
 *                char *name, char *description                                                *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index, char *description                               *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setColumnDescription(int32_t index, char *description) {
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setDescription(description);
}
void SDDSFile::setColumnDescription(char *name, char *description) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setDescription(description);
}
void SDDSFile_setColumnDescription(void *sddsfile, int32_t index, char *description) {
  ((SDDSFile*)sddsfile)->setColumnDescription(index, description);
}

/*
 * setArrayFormatString
 *
 * C++ Arguments: int32_t index, char *format_string
 *                char *name, char *format_string
 *
 * C Arguments: void *sddsfile, int32_t index, char *format_string
 *
 * Results:  none
 */
void SDDSFile::setArrayFormatString(int32_t index, char *format_string) {
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setFormatString(format_string);
}
void SDDSFile::setArrayFormatString(char *name, char *format_string) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setFormatString(format_string);
}
void SDDSFile_setArrayFormatString(void *sddsfile, int32_t index, char *format_string) {
  ((SDDSFile*)sddsfile)->setArrayFormatString(index, format_string);
}

/*
 * setParameterFormatString
 *
 * C++ Arguments: int32_t index, char *format_string
 *                char *name, char *format_string
 *
 * C Arguments: void *sddsfile, int32_t index, char *format_string
 *
 * Results:  none
 */
void SDDSFile::setParameterFormatString(int32_t index, char *format_string) {
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setFormatString(format_string);
}
void SDDSFile::setParameterFormatString(char *name, char *format_string) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setFormatString(format_string);
}
void SDDSFile_setParameterFormatString(void *sddsfile, int32_t index, char *format_string) {
  ((SDDSFile*)sddsfile)->setParameterFormatString(index, format_string);
}

/***********************************************************************************************
 *                             setColumnFormatString                                           *
 *                                                                                             *
 * C++ Arguments: int32_t index, char *format_string                                           *
 *                char *name, char *format_string                                              *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index, char *format_string                             *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setColumnFormatString(int32_t index, char *format_string) {
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setFormatString(format_string);
}
void SDDSFile::setColumnFormatString(char *name, char *format_string) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setFormatString(format_string);
}
void SDDSFile_setColumnFormatString(void *sddsfile, int32_t index, char *format_string) {
  ((SDDSFile*)sddsfile)->setColumnFormatString(index, format_string);
}

/***********************************************************************************************
 *                             setParameterFixedValue                                          *
 *                                                                                             *
 * C++ Arguments: int32_t index, char *fixed_value                                             *
 *                char *name, char *fixed_value                                                *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index, char *fixed_value                               *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setParameterFixedValue(int32_t index, char *fixed_value) {
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setFixedValue(fixed_value);
}
void SDDSFile::setParameterFixedValue(char *name, char *fixed_value) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return;
  parameterObject[index]->setFixedValue(fixed_value);
}
void SDDSFile_setParameterFixedValue(void *sddsfile, int32_t index, char *fixed_value) {
  ((SDDSFile*)sddsfile)->setParameterFixedValue(index, fixed_value);
}

/*
 * setArrayFieldLength
 *
 * C++ Arguments: int32_t index, uint32_t field_length
 *                char *name, uint32_t field_length
 *
 * C Arguments: void *sddsfile, int32_t index, uint32_t field_length
 *
 * Results:  none
 */
void SDDSFile::setArrayFieldLength(int32_t index, uint32_t field_length) {
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setFieldLength(field_length);
}
void SDDSFile::setArrayFieldLength(char *name, uint32_t field_length) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setFieldLength(field_length);
}
void SDDSFile_setArrayFieldLength(void *sddsfile, int32_t index, uint32_t field_length) {
  ((SDDSFile*)sddsfile)->setArrayFieldLength(index, field_length);
}

/*
 * setColumnFieldLength
 *
 * C++ Arguments: int32_t index, uint32_t field_length
 *                char *name, uint32_t field_length
 *
 * C Arguments: void *sddsfile, int32_t index, uint32_t field_length
 *
 * Results:  none
 */
void SDDSFile::setColumnFieldLength(int32_t index, uint32_t field_length) {
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setFieldLength(field_length);
}
void SDDSFile::setColumnFieldLength(char *name, uint32_t field_length) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return;
  columnObject[index]->setFieldLength(field_length);
}
void SDDSFile_setColumnFieldLength(void *sddsfile, int32_t index, uint32_t field_length) {
  ((SDDSFile*)sddsfile)->setColumnFieldLength(index, field_length);
}

/*
 * setArrayGroupName
 *
 * C++ Arguments: int32_t index, char *group_name
 *                char *name, char *group_name
 *
 * C Arguments: void *sddsfile, int32_t index, char *group_name
 *
 * Results:  none
 */
void SDDSFile::setArrayGroupName(int32_t index, char *group_name) {
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setGroupName(group_name);
}
void SDDSFile::setArrayGroupName(char *name, char *group_name) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setGroupName(group_name);
}
void SDDSFile_setArrayGroupName(void *sddsfile, int32_t index, char *group_name) {
  ((SDDSFile*)sddsfile)->setArrayGroupName(index, group_name);
}

/*
 * setArrayDimensions
 *
 * C++ Arguments: int32_t index, uint32_t dimensions
 *                char *name, uint32_t dimensions
 *
 * C Arguments: void *sddsfile, int32_t index, uint32_t dimensions
 *
 * Results:  none
 */
void SDDSFile::setArrayDimensions(int32_t index, uint32_t dimensions) {
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setDimensions(dimensions);
}
void SDDSFile::setArrayDimensions(char *name, uint32_t dimensions) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setDimensions(dimensions);
}
void SDDSFile_setArrayDimensions(void *sddsfile, int32_t index, uint32_t dimensions) {
  ((SDDSFile*)sddsfile)->setArrayDimensions(index, dimensions);
}

/*
 * setArrayDim
 *
 * C++ Arguments: uint32_t page, int32_t index, uint32_t *dimensions
 *                uint32_t page, char *name, uint32_t *dimensions
 *
 * C Arguments: void *sddsfile, uint32_t page, uint32_t *dimensions, int32_t mode, ...
 *              mode=SDDSFILE_SETARRAY_WITH_INDEX,
 *                    int32_t  arrayIndex
 *              mode=SDDSFILE_SETARRAY_WITH_NAME,
 *                    char *name
 *
 * Results:  none
 */
void SDDSFile::setArrayDim(uint32_t page, int32_t index, uint32_t *dimensions) {
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setDim(page, dimensions);
}
void SDDSFile::setArrayDim(uint32_t page, char *name, uint32_t *dimensions) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return;
  arrayObject[index]->setDim(page, dimensions);
}
void SDDSFile_setArrayDim(void *sddsfile, uint32_t page, uint32_t *dimensions, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_SETARRAY_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    ((SDDSFile*)sddsfile)->setArrayDim(page, index, dimensions);
  } else {
    name = va_arg(ap, char *);
    ((SDDSFile*)sddsfile)->setArrayDim(page, name, dimensions);
  }
}

/*
 * getArrayName
 *
 * C++ Arguments: int32_t index
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to name on success (do not free result)
 *          NULL if name not defined
 */
char* SDDSFile::getArrayName(int32_t index) {
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getName());
}
char* SDDSFile_getArrayName(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getArrayName(index));
}

/*
 * getParameterName
 *
 * C++ Arguments: int32_t index
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to name on success (do not free result)
 *          NULL if name not defined
 */
char* SDDSFile::getParameterName(int32_t index) {
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getName());
}
char* SDDSFile_getParameterName(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getParameterName(index));
}

/***********************************************************************************************
 *                               getColumnName                                                 *
 *                                                                                             *
 * C++ Arguments: int32_t index                                                                *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index                                                  *
 *                                                                                             *
 * Results: pointer to name on success (do not free result)                                    *
 *          NULL if name not defined                                                           *
 ***********************************************************************************************/
char* SDDSFile::getColumnName(int32_t index) {
  if ((index < 0) || (index >= columnCount))
    return(NULL);
  return(columnObject[index]->getName());
}
char* SDDSFile_getColumnName(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getColumnName(index));
}

/*
 * getArrayNames
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of names on success (you must free the pointer but not the
 *                                                elements inside the pointer)
 *          elements inside the array may be NULL if name not defined
 */
char** SDDSFile::getArrayNames() {
  int32_t i;
  char **names;
  names = (char**)SDDS_malloc(sizeof(char*) * arrayCount);
  for (i=0;i<arrayCount;i++) {
    names[i] = getArrayName(i);
  }
  return(names);
}
char** SDDSFile_getArrayNames(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getArrayNames());
}

/*
 * getArrayCount
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: number of arrays
 */
int32_t SDDSFile::getArrayCount() {
  return(arrayCount);
}
int32_t SDDSFile_getArrayCount(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getArrayCount());
}

/*
 * getParameterNames
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of names on success (you must free the pointer but not the
 *                                                elements inside the pointer)
 *          elements inside the array may be NULL if name not defined
 */
char** SDDSFile::getParameterNames() {
  int32_t i;
  char **names;
  names = (char**)SDDS_malloc(sizeof(char*) * parameterCount);
  for (i=0;i<parameterCount;i++) {
    names[i] = getParameterName(i);
  }
  return(names);
}
char** SDDSFile_getParameterNames(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getParameterNames());
}

/*
 * getParameterCount
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: number of parameters
 */
int32_t SDDSFile::getParameterCount() {
  return(parameterCount);
}
int32_t SDDSFile_getParameterCount(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getParameterCount());
}

/***********************************************************************************************
 *                               getColumnNames                                                *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: pointer to array of names on success (you must free the pointer but not the        *
 *                                                elements inside the pointer)                 *
 *          elements inside the array may be NULL if name not defined                          *
 ***********************************************************************************************/
char** SDDSFile::getColumnNames() {
  int32_t i;
  char **names;
  names = (char**)SDDS_malloc(sizeof(char*) * columnCount);
  for (i=0;i<columnCount;i++) {
    names[i] = getColumnName(i);
  }
  return(names);
}
char** SDDSFile_getColumnNames(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getColumnNames());
}

/***********************************************************************************************
 *                               getColumnCount                                                *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: number of columns                                                                  *
 ***********************************************************************************************/
int32_t SDDSFile::getColumnCount() {
  return(columnCount);
}
int32_t SDDSFile_getColumnCount(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getColumnCount());
}

/*
 * getArrayUnits
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to units on success (do not free result)
 *          NULL if units not defined
 */
char* SDDSFile::getArrayUnits(int32_t index) {
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getUnits());
}
char* SDDSFile::getArrayUnits(char *name) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getUnits());
}
char* SDDSFile_getArrayUnits(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getArrayUnits(index));
}

/*
 * getParameterUnits
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to units on success (do not free result)
 *          NULL if units not defined
 */
char* SDDSFile::getParameterUnits(int32_t index) {
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getUnits());
}
char* SDDSFile::getParameterUnits(char *name) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getUnits());
}
char* SDDSFile_getParameterUnits(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getParameterUnits(index));
}

/***********************************************************************************************
 *                               getColumnUnits                                                *
 *                                                                                             *
 * C++ Arguments: int32_t index                                                                *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index                                                  *
 *                                                                                             *
 * Results: pointer to units on success (do not free result)                                   *
 *          NULL if units not defined                                                          *
 ***********************************************************************************************/
char* SDDSFile::getColumnUnits(int32_t index) {
  if ((index < 0) || (index >= columnCount))
    return(NULL);
  return(columnObject[index]->getUnits());
}
char* SDDSFile::getColumnUnits(char *name) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return(NULL);
  return(columnObject[index]->getUnits());
}
char* SDDSFile_getColumnUnits(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getColumnUnits(index));
}

/*
 * getAllArrayUnits
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of units on success (you must free the pointer but not the
 *                                                elements inside the pointer)
 *          elements inside the array may be NULL if unit not defined
 */
char** SDDSFile::getAllArrayUnits() {
  int32_t i;
  char **units;
  units = (char**)SDDS_malloc(sizeof(char*) * arrayCount);
  for (i=0;i<arrayCount;i++) {
    units[i] = getArrayUnits(i);
  }
  return(units);
}
char** SDDSFile_getAllArrayUnits(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getAllArrayUnits());
}

/*
 * getAllParameterUnits
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of units on success (you must free the pointer but not the
 *                                                elements inside the pointer)
 *          elements inside the array may be NULL if unit not defined
 */
char** SDDSFile::getAllParameterUnits() {
  int32_t i;
  char **units;
  units = (char**)SDDS_malloc(sizeof(char*) * parameterCount);
  for (i=0;i<parameterCount;i++) {
    units[i] = getParameterUnits(i);
  }
  return(units);
}
char** SDDSFile_getAllParameterUnits(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getAllParameterUnits());
}

/***********************************************************************************************
 *                               getAllColumnUnits                                             *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: pointer to array of units on success (you must free the pointer but not the        *
 *                                                elements inside the pointer)                 *
 *          elements inside the array may be NULL if unit not defined                          *
 ***********************************************************************************************/
char** SDDSFile::getAllColumnUnits() {
  int32_t i;
  char **units;
  units = (char**)SDDS_malloc(sizeof(char*) * columnCount);
  for (i=0;i<columnCount;i++) {
    units[i] = getColumnUnits(i);
  }
  return(units);
}
char** SDDSFile_getAllColumnUnits(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getAllColumnUnits());
}

/*
 * getArraySymbol
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to symbol on success (do not free result)
 *          NULL if symbol not defined
 */
char* SDDSFile::getArraySymbol(int32_t index) {
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getSymbol());
}
char* SDDSFile::getArraySymbol(char *name) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getSymbol());
}
char* SDDSFile_getArraySymbol(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getArraySymbol(index));
}

/*
 * getParameterSymbol
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to symbol on success (do not free result)
 *          NULL if symbol not defined
 */
char* SDDSFile::getParameterSymbol(int32_t index) {
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getSymbol());
}
char* SDDSFile::getParameterSymbol(char *name) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getSymbol());
}
char* SDDSFile_getParameterSymbol(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getParameterSymbol(index));
}

/***********************************************************************************************
 *                               getColumnSymbol                                               *
 *                                                                                             *
 * C++ Arguments: int32_t index                                                                *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index                                                  *
 *                                                                                             *
 * Results: pointer to symbol on success (do not free result)                                  *
 *          NULL if symbol not defined                                                         *
 ***********************************************************************************************/
char* SDDSFile::getColumnSymbol(int32_t index) {
  if ((index < 0) || (index >= columnCount))
    return(NULL);
  return(columnObject[index]->getSymbol());
}
char* SDDSFile::getColumnSymbol(char *name) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return(NULL);
  return(columnObject[index]->getSymbol());
}
char* SDDSFile_getColumnSymbol(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getColumnSymbol(index));
}

/*
 * getArraySymbols
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of symbols on success (you must free the pointer but not the
 *                                                  elements inside the pointer)
 *          elements inside the array may be NULL if unit not defined
 */
char** SDDSFile::getArraySymbols() {
  int32_t i;
  char **symbols;
  symbols = (char**)SDDS_malloc(sizeof(char*) * arrayCount);
  for (i=0;i<arrayCount;i++) {
    symbols[i] = getArraySymbol(i);
  }
  return(symbols);
}
char** SDDSFile_getArraySymbols(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getArraySymbols());
}

/*
 * getParameterSymbols
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of symbols on success (you must free the pointer but not the
 *                                                  elements inside the pointer)
 *          elements inside the array may be NULL if unit not defined
 */
char** SDDSFile::getParameterSymbols() {
  int32_t i;
  char **symbols;
  symbols = (char**)SDDS_malloc(sizeof(char*) * parameterCount);
  for (i=0;i<parameterCount;i++) {
    symbols[i] = getParameterSymbol(i);
  }
  return(symbols);
}
char** SDDSFile_getParameterSymbols(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getParameterSymbols());
}

/***********************************************************************************************
 *                               getColumnSymbols                                              *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: pointer to array of symbols on success (you must free the pointer but not the      *
 *                                                  elements inside the pointer)               *
 *          elements inside the array may be NULL if unit not defined                          *
 ***********************************************************************************************/
char** SDDSFile::getColumnSymbols() {
  int32_t i;
  char **symbols;
  symbols = (char**)SDDS_malloc(sizeof(char*) * columnCount);
  for (i=0;i<columnCount;i++) {
    symbols[i] = getColumnSymbol(i);
  }
  return(symbols);
}
char** SDDSFile_getColumnSymbols(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getColumnSymbols());
}

/*
 * getArrayFormatString
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to format string on success (do not free result)
 *          NULL if format string not defined
 */
char* SDDSFile::getArrayFormatString(int32_t index) {
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getFormatString());
}
char* SDDSFile::getArrayFormatString(char *name) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getFormatString());
}
char* SDDSFile_getArrayFormatString(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getArrayFormatString(index));
}

/*
 * getParameterFormatString
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to format string on success (do not free result)
 *          NULL if format string not defined
 */
char* SDDSFile::getParameterFormatString(int32_t index) {
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getFormatString());
}
char* SDDSFile::getParameterFormatString(char *name) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getFormatString());
}
char* SDDSFile_getParameterFormatString(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getParameterFormatString(index));
}

/***********************************************************************************************
 *                               getColumnFormatString                                         *
 *                                                                                             *
 * C++ Arguments: int32_t index                                                                *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index                                                  *
 *                                                                                             *
 * Results: pointer to format string on success (do not free result)                           *
 *          NULL if format string not defined                                                  *
 ***********************************************************************************************/
char* SDDSFile::getColumnFormatString(int32_t index) {
  if ((index < 0) || (index >= columnCount))
    return(NULL);
  return(columnObject[index]->getFormatString());
}
char* SDDSFile::getColumnFormatString(char *name) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return(NULL);
  return(columnObject[index]->getFormatString());
}
char* SDDSFile_getColumnFormatString(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getColumnFormatString(index));
}

/*
 * getArrayFormatStrings
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of format strings on success (you must free the pointer but not
 *                                                         the elements inside the pointer)
 *          elements inside the array may be NULL if unit not defined
 */
char** SDDSFile::getArrayFormatStrings() {
  int32_t i;
  char **format_strings;
  format_strings = (char**)SDDS_malloc(sizeof(char*) * arrayCount);
  for (i=0;i<arrayCount;i++) {
    format_strings[i] = getArrayFormatString(i);
  }
  return(format_strings);
}
char** SDDSFile_getArrayFormatStrings(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getArrayFormatStrings());
}

/*
 * getParameterFormatStrings
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of format strings on success (you must free the pointer but not
 *                                                         the elements inside the pointer)
 *          elements inside the array may be NULL if unit not defined
 */
char** SDDSFile::getParameterFormatStrings() {
  int32_t i;
  char **format_strings;
  format_strings = (char**)SDDS_malloc(sizeof(char*) * parameterCount);
  for (i=0;i<parameterCount;i++) {
    format_strings[i] = getParameterFormatString(i);
  }
  return(format_strings);
}
char** SDDSFile_getParameterFormatStrings(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getParameterFormatStrings());
}

/***********************************************************************************************
 *                               getColumnFormatStrings                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: pointer to array of format strings on success (you must free the pointer but not   *
 *                                                         the elements inside the pointer)    *
 *          elements inside the array may be NULL if unit not defined                          *
 ***********************************************************************************************/
char** SDDSFile::getColumnFormatStrings() {
  int32_t i;
  char **format_strings;
  format_strings = (char**)SDDS_malloc(sizeof(char*) * columnCount);
  for (i=0;i<columnCount;i++) {
    format_strings[i] = getColumnFormatString(i);
  }
  return(format_strings);
}
char** SDDSFile_getColumnFormatStrings(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getColumnFormatStrings());
}

/*
 * getArrayType
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: int32 representing the array type
 *          0 if array type not defined
 */
int32_t SDDSFile::getArrayType(int32_t index) {
  if ((index < 0) || (index >= arrayCount))
    return(0);
  return(arrayObject[index]->getType());
}
int32_t SDDSFile::getArrayType(char *name) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return(0);
  return(arrayObject[index]->getType());
}
int32_t SDDSFile_getArrayType(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getArrayType(index));
}

/*
 * getParameterType
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: int32 representing the parameter type
 *          0 if parameter type not defined
 */
int32_t SDDSFile::getParameterType(int32_t index) {
  if ((index < 0) || (index >= parameterCount))
    return(0);
  return(parameterObject[index]->getType());
}
int32_t SDDSFile::getParameterType(char *name) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return(0);
  return(parameterObject[index]->getType());
}
int32_t SDDSFile_getParameterType(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getParameterType(index));
}

/***********************************************************************************************
 *                               getColumnType                                                 *
 *                                                                                             *
 * C++ Arguments: int32_t index                                                                *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index                                                  *
 *                                                                                             *
 * Results: int32 representing the column type                                                 *
 *          0 if column type not defined                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::getColumnType(int32_t index) {
  if ((index < 0) || (index >= columnCount))
    return(0);
  return(columnObject[index]->getType());
}
int32_t SDDSFile::getColumnType(char *name) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return(0);
  return(columnObject[index]->getType());
}
int32_t SDDSFile_getColumnType(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getColumnType(index));
}

/*
 * getArrayTypes
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of array types on success (you must free the pointer)
 *          elements inside the array may be 0 if unit not defined
 */
int32_t* SDDSFile::getArrayTypes() {
  int32_t i;
  int32_t *types;
  types = (int32_t*)SDDS_malloc(sizeof(int32_t) * arrayCount);
  for (i=0;i<arrayCount;i++) {
    types[i] = getArrayType(i);
  }
  return(types);
}
int32_t* SDDSFile_getArrayTypes(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getArrayTypes());
}

/*
 * getParameterTypes
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of parameter types on success (you must free the pointer)
 *          elements inside the array may be 0 if unit not defined
 */
int32_t* SDDSFile::getParameterTypes() {
  int32_t i;
  int32_t *types;
  types = (int32_t*)SDDS_malloc(sizeof(int32_t) * parameterCount);
  for (i=0;i<parameterCount;i++) {
    types[i] = getParameterType(i);
  }
  return(types);
}
int32_t* SDDSFile_getParameterTypes(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getParameterTypes());
}

/***********************************************************************************************
 *                               getColumnTypes                                                *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: pointer to array of column types on success (you must free the pointer)            *
 *          elements inside the array may be 0 if unit not defined                             *
 ***********************************************************************************************/
int32_t* SDDSFile::getColumnTypes() {
  int32_t i;
  int32_t *types;
  types = (int32_t*)SDDS_malloc(sizeof(int32_t) * columnCount);
  for (i=0;i<columnCount;i++) {
    types[i] = getColumnType(i);
  }
  return(types);
}
int32_t* SDDSFile_getColumnTypes(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getColumnTypes());
}

/*
 * getArrayDescription
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to description on success (do not free result)
 *          NULL if description not defined
 */
char* SDDSFile::getArrayDescription(int32_t index) {
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getDescription());
}
char* SDDSFile::getArrayDescription(char *name) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getDescription());
}
char* SDDSFile_getArrayDescription(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getArrayDescription(index));
}

/*
 * getParameterDescription
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to description on success (do not free result)
 *          NULL if description not defined
 */
char* SDDSFile::getParameterDescription(int32_t index) {
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getDescription());
}
char* SDDSFile::getParameterDescription(char *name) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getDescription());
}
char* SDDSFile_getParameterDescription(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getParameterDescription(index));
}

/***********************************************************************************************
 *                               getColumnDescription                                          *
 *                                                                                             *
 * C++ Arguments: int32_t index                                                                *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index                                                  *
 *                                                                                             *
 * Results: pointer to description on success (do not free result)                             *
 *          NULL if description not defined                                                    *
 ***********************************************************************************************/
char* SDDSFile::getColumnDescription(int32_t index) {
  if ((index < 0) || (index >= columnCount))
    return(NULL);
  return(columnObject[index]->getDescription());
}
char* SDDSFile::getColumnDescription(char *name) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return(NULL);
  return(columnObject[index]->getDescription());
}
char* SDDSFile_getColumnDescription(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getColumnDescription(index));
}

/*
 * getArrayDescriptions
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of descriptions on success (you must free the pointer but not
 *                                                       the elements inside the pointer)
 *          elements inside the array may be NULL if unit not defined
 */
char** SDDSFile::getArrayDescriptions() {
  int32_t i;
  char **descriptions;
  descriptions = (char**)SDDS_malloc(sizeof(char*) * arrayCount);
  for (i=0;i<arrayCount;i++) {
    descriptions[i] = getArrayDescription(i);
  }
  return(descriptions);
}
char** SDDSFile_getArrayDescriptions(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getArrayDescriptions());
}

/*
 * getParameterDescriptions
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of descriptions on success (you must free the pointer but not
 *                                                       the elements inside the pointer)
 *          elements inside the array may be NULL if unit not defined
 */
char** SDDSFile::getParameterDescriptions() {
  int32_t i;
  char **descriptions;
  descriptions = (char**)SDDS_malloc(sizeof(char*) * parameterCount);
  for (i=0;i<parameterCount;i++) {
    descriptions[i] = getParameterDescription(i);
  }
  return(descriptions);
}
char** SDDSFile_getParameterDescriptions(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getParameterDescriptions());
}

/***********************************************************************************************
 *                               getColumnDescriptions                                         *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: pointer to array of descriptions on success (you must free the pointer but not     *
 *                                                       the elements inside the pointer)      *
 *          elements inside the array may be NULL if unit not defined                          *
 ***********************************************************************************************/
char** SDDSFile::getColumnDescriptions() {
  int32_t i;
  char **descriptions;
  descriptions = (char**)SDDS_malloc(sizeof(char*) * columnCount);
  for (i=0;i<columnCount;i++) {
    descriptions[i] = getColumnDescription(i);
  }
  return(descriptions);
}
char** SDDSFile_getColumnDescriptions(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getColumnDescriptions());
}

/*
 * getArrayGroupName
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: pointer to group name on success (do not free result)
 *          NULL if group name not defined
 */
char* SDDSFile::getArrayGroupName(int32_t index) {
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getGroupName());
}
char* SDDSFile::getArrayGroupName(char *name) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return(NULL);
  return(arrayObject[index]->getGroupName());
}
char* SDDSFile_getArrayGroupName(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getArrayGroupName(index));
}

/*
 * getArrayGroupNames
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of group names on success (you must free the pointer but not
 *                                                      the elements inside the pointer)
 *          elements inside the array may be NULL if unit not defined
 */
char** SDDSFile::getArrayGroupNames() {
  int32_t i;
  char **group_names;
  group_names = (char**)SDDS_malloc(sizeof(char*) * arrayCount);
  for (i=0;i<arrayCount;i++) {
    group_names[i] = getArrayGroupName(i);
  }
  return(group_names);
}
char** SDDSFile_getArrayGroupNames(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getArrayGroupNames());
}

/***********************************************************************************************
 *                               getParameterFixedValue                                        *
 *                                                                                             *
 * C++ Arguments: int32_t index                                                                *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t index                                                  *
 *                                                                                             *
 * Results: pointer to fixed value on success (do not free result)                             *
 *          NULL if fixed value not defined                                                    *
 ***********************************************************************************************/
char* SDDSFile::getParameterFixedValue(int32_t index) {
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getFixedValue());
}
char* SDDSFile::getParameterFixedValue(char *name) {
  int32_t index;
  index = getParameterIndex(name);
  if ((index < 0) || (index >= parameterCount))
    return(NULL);
  return(parameterObject[index]->getFixedValue());
}
char* SDDSFile_getParameterFixedValue(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getParameterFixedValue(index));
}

/***********************************************************************************************
 *                               getParameterFixedValues                                       *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: pointer to array of fixed values on success (you must free the pointer but not     *
 *                                                       the elements inside the pointer)      *
 *          elements inside the array may be NULL if unit not defined                          *
 ***********************************************************************************************/
char** SDDSFile::getParameterFixedValues() {
  int32_t i;
  char **fixed_values;
  fixed_values = (char**)SDDS_malloc(sizeof(char*) * parameterCount);
  for (i=0;i<parameterCount;i++) {
    fixed_values[i] = getParameterFixedValue(i);
  }
  return(fixed_values);
}
char** SDDSFile_getParameterFixedValues(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getParameterFixedValues());
}

/*
 * getArrayFieldLength
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: uint32 representing the array field length
 *          0 if array type not defined
 */
uint32_t SDDSFile::getArrayFieldLength(int32_t index) {
  if ((index < 0) || (index >= arrayCount))
    return(0);
  return(arrayObject[index]->getFieldLength());
}
uint32_t SDDSFile::getArrayFieldLength(char *name) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return(0);
  return(arrayObject[index]->getFieldLength());
}
uint32_t SDDSFile_getArrayFieldLength(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getArrayFieldLength(index));
}

/*
 * getArrayFieldLengths
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of array field lengths on success (you must free the pointer)
 *          elements inside the array may be 0 if unit not defined
 */
uint32_t* SDDSFile::getArrayFieldLengths() {
  int32_t i;
  uint32_t *fls;
  fls = (uint32_t*)SDDS_malloc(sizeof(uint32_t) * arrayCount);
  for (i=0;i<arrayCount;i++) {
    fls[i] = getArrayFieldLength(i);
  }
  return(fls);
}
uint32_t* SDDSFile_getArrayFieldLengths(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getArrayFieldLengths());
}

/*
 * getArrayDimensions
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: uint32 representing the array dimension
 *          0 if array type not defined
 */
uint32_t SDDSFile::getArrayDimensions(int32_t index) {
  if ((index < 0) || (index >= arrayCount))
    return(0);
  return(arrayObject[index]->getDimensions());
}
uint32_t SDDSFile::getArrayDimensions(char *name) {
  int32_t index;
  index = getArrayIndex(name);
  if ((index < 0) || (index >= arrayCount))
    return(0);
  return(arrayObject[index]->getDimensions());
}
uint32_t SDDSFile_getArrayDimensions(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getArrayDimensions(index));
}

/*
 * getAllArrayDimensions
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of array dimensions on success (you must free the pointer)
 *          elements inside the array may be 0 if unit not defined
 */
uint32_t* SDDSFile::getAllArrayDimensions() {
  int32_t i;
  uint32_t *fls;
  fls = (uint32_t*)SDDS_malloc(sizeof(uint32_t) * arrayCount);
  for (i=0;i<arrayCount;i++) {
    fls[i] = getArrayDimensions(i);
  }
  return(fls);
}
uint32_t* SDDSFile_getAllArrayDimensions(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getAllArrayDimensions());
}

/*
 * getColumnFieldLength
 *
 * C++ Arguments: int32_t index
 *                char *name
 *
 * C Arguments: void *sddsfile, int32_t index
 *
 * Results: uint32 representing the column field length
 *          0 if column type not defined
 */
uint32_t SDDSFile::getColumnFieldLength(int32_t index) {
  if ((index < 0) || (index >= columnCount))
    return(0);
  return(columnObject[index]->getFieldLength());
}
uint32_t SDDSFile::getColumnFieldLength(char *name) {
  int32_t index;
  index = getColumnIndex(name);
  if ((index < 0) || (index >= columnCount))
    return(0);
  return(columnObject[index]->getFieldLength());
}
uint32_t SDDSFile_getColumnFieldLength(void *sddsfile, int32_t index) {
  return(((SDDSFile*)sddsfile)->getColumnFieldLength(index));
}

/*
 * getColumnFieldLengths
 *
 * C++ Arguments: none
 *
 * C Arguments: void *sddsfile
 *
 * Results: pointer to array of column field lengths on success (you must free the pointer)
 *          elements inside the array may be 0 if unit not defined
 */
uint32_t* SDDSFile::getColumnFieldLengths() {
  int32_t i;
  uint32_t *fls;
  fls = (uint32_t*)SDDS_malloc(sizeof(uint32_t) * columnCount);
  for (i=0;i<columnCount;i++) {
    fls[i] = getColumnFieldLength(i);
  }
  return(fls);
}
uint32_t* SDDSFile_getColumnFieldLengths(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->getColumnFieldLengths());
}

/*
 * getArrayIndex
 *
 * C++ Arguments: char *name
 *
 * C Arguments: void *sddsfile, char *name
 *
 * Results: array index on success
 *          -1 on failure (calling program should call printErrors)
 */
int32_t SDDSFile::getArrayIndex(char *name) {
  int32_t i;
  char s[SDDS_MAXLINE];
  for ( i=0 ; i<arrayCount ; i++ )
    if (strcmp(name, arrayObject[i]->getName()) == 0)
      return(i);
  sprintf(s, "Warning: array %s does not exist (SDDSFile::getArrayIndex)", name);
  setError(s);
  return(-1);
}
int32_t SDDSFile_getArrayIndex(void *sddsfile, char *name) {
  return(((SDDSFile*)sddsfile)->getArrayIndex(name));
}

/*
 * getParameterIndex
 *
 * C++ Arguments: char *name
 *
 * C Arguments: void *sddsfile, char *name
 *
 * Results: parameter index on success
 *          -1 on failure (calling program should call printErrors)
 */
int32_t SDDSFile::getParameterIndex(char *name) {
  int32_t i;
  char s[SDDS_MAXLINE];
  for ( i=0 ; i<parameterCount ; i++ )
    if (strcmp(name, parameterObject[i]->getName()) == 0)
      return(i);
  sprintf(s, "Warning: parameter %s does not exist (SDDSFile::getParameterIndex)", name);
  setError(s);
  return(-1);
}
int32_t SDDSFile_getParameterIndex(void *sddsfile, char *name) {
  return(((SDDSFile*)sddsfile)->getParameterIndex(name));
}

/***********************************************************************************************
 *                               getColumnIndex                                                *
 *                                                                                             *
 * C++ Arguments: char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, char *name                                                     *
 *                                                                                             *
 * Results: column index on success                                                            *
 *          -1 on failure (calling program should call printErrors)                            *
 ***********************************************************************************************/
int32_t SDDSFile::getColumnIndex(char *name) {
  int32_t i;
  char s[SDDS_MAXLINE];
  for ( i=0 ; i<columnCount ; i++ )
    if (strcmp(name, columnObject[i]->getName()) == 0)
      return(i);
  sprintf(s, "Warning: column %s does not exist (SDDSFile::getColumnIndex)", name);
  setError(s);
  return(-1);
}
int32_t SDDSFile_getColumnIndex(void *sddsfile, char *name) {
  return(((SDDSFile*)sddsfile)->getColumnIndex(name));
}

/*
 * getArrayIndexes
 *
 * C++ Arguments: char **names, int32_t numberOfNames
 *
 * C Arguments: void *sddsfile, char **names, int32_t numberOfNames
 *
 * Results: array of array indexes on success (you must free the pointer)
 *          an element in the array may be -1 if there was a problem (calling program should
 *                                                                   call printErrors)
 */
int32_t* SDDSFile::getArrayIndexes(char** names, int32_t number) {
  int32_t *n;
  int32_t i;
  n = (int32_t*)SDDS_malloc(sizeof(int32_t) * number);
  for (i=0 ; i<number ; i++)
    n[i] = getArrayIndex(names[i]);
  return(n);
}
int32_t* SDDSFile_getArrayIndexes(void *sddsfile, char** names, int32_t number) {
  return(((SDDSFile*)sddsfile)->getArrayIndexes(names, number));
}

/*
 * getParameterIndexes
 *
 * C++ Arguments: char **names, int32_t numberOfNames
 *
 * C Arguments: void *sddsfile, char **names, int32_t numberOfNames
 *
 * Results: array of parameter indexes on success (you must free the pointer)
 *          an element in the array may be -1 if there was a problem (calling program should
 *                                                                   call printErrors)
 */
int32_t* SDDSFile::getParameterIndexes(char** names, int32_t number) {
  int32_t *n;
  int32_t i;
  n = (int32_t*)SDDS_malloc(sizeof(int32_t) * number);
  for (i=0 ; i<number ; i++)
    n[i] = getParameterIndex(names[i]);
  return(n);
}
int32_t* SDDSFile_getParameterIndexes(void *sddsfile, char** names, int32_t number) {
  return(((SDDSFile*)sddsfile)->getParameterIndexes(names, number));
}

/***********************************************************************************************
 *                               getColumnIndexes                                              *
 *                                                                                             *
 * C++ Arguments: char **names, int32_t numberOfNames                                          *
 *                                                                                             *
 * C Arguments: void *sddsfile, char **names, int32_t numberOfNames                            *
 *                                                                                             *
 * Results: array of column indexes on success (you must free the pointer)                     *
 *          an element in the array may be -1 if there was a problem (calling program should   *
 *                                                                   call printErrors)         *
 ***********************************************************************************************/
int32_t* SDDSFile::getColumnIndexes(char** names, int32_t number) {
  int32_t *n;
  int32_t i;
  n = (int32_t*)SDDS_malloc(sizeof(int32_t) * number);
  for (i=0 ; i<number ; i++)
    n[i] = getColumnIndex(names[i]);
  return(n);
}
int32_t* SDDSFile_getColumnIndexes(void *sddsfile, char** names, int32_t number) {
  return(((SDDSFile*)sddsfile)->getColumnIndexes(names, number));
}

/*
 * setArray
 *
 * C++ Arguments: int32_t arrayIndex, uint32_t page, int32_t *values, uint32_t elements
 *                char *name, uint32_t page, int32_t *values, uint32_t elements
 *                int32_t arrayIndex, uint32_t page, uint32_t *values, uint32_t elements
 *                char *name, uint32_t page, uint32_t *values, uint32_t elements
 *                int32_t arrayIndex, uint32_t page, double *values, uint32_t elements
 *                char *name, uint32_t page, double *values, uint32_t elements
 *                int32_t arrayIndex, uint32_t page, char **values, uint32_t elements
 *                char *name, uint32_t page, char **values, uint32_t elements
 *
 * C Arguments: void *sddsfile, uint32_t page, uint32_t elements, int32_t mode, ...
 *              mode=SDDSFILE_SETARRAY_WITH_INDEX+SDDSFILE_SETARRAY_WITH_INT16,
 *                    int32_t  arrayIndex, int16_t *value
 *              mode=SDDSFILE_SETARRAY_WITH_NAME+SDDSFILE_SETARRAY_WITH_INT16,
 *                    char *name, int16_t *value
 *              mode=SDDSFILE_SETARRAY_WITH_INDEX+SDDSFILE_SETARRAY_WITH_UINT16,
 *                    int32_t  arrayIndex, uint16_t *value
 *              mode=SDDSFILE_SETARRAY_WITH_NAME+SDDSFILE_SETARRAY_WITH_UINT16,
 *                    char *name, uint16_t *value
 *              mode=SDDSFILE_SETARRAY_WITH_INDEX+SDDSFILE_SETARRAY_WITH_INT32,
 *                    int32_t  arrayIndex, int32_t *value
 *              mode=SDDSFILE_SETARRAY_WITH_NAME+SDDSFILE_SETARRAY_WITH_INT32,
 *                    char *name, int32_t *value
 *              mode=SDDSFILE_SETARRAY_WITH_INDEX+SDDSFILE_SETARRAY_WITH_UINT32,
 *                    int32_t  arrayIndex, uint32_t *value
 *              mode=SDDSFILE_SETARRAY_WITH_NAME+SDDSFILE_SETARRAY_WITH_UINT32,
 *                    char *name, uint32_t *value
 *              mode=SDDSFILE_SETARRAY_WITH_INDEX+SDDSFILE_SETARRAY_WITH_REAL32,
 *                    int32_t  arrayIndex, float *value
 *              mode=SDDSFILE_SETARRAY_WITH_NAME+SDDSFILE_SETARRAY_WITH_REAL32,
 *                    char *name, float *value
 *              mode=SDDSFILE_SETARRAY_WITH_INDEX+SDDSFILE_SETARRAY_WITH_REAL64,
 *                    int32_t  arrayIndex, double *value
 *              mode=SDDSFILE_SETARRAY_WITH_NAME+SDDSFILE_SETARRAY_WITH_REAL64,
 *                    char *name, double *value
 *              mode=SDDSFILE_SETARRAY_WITH_INDEX+SDDSFILE_SETARRAY_WITH_STRING,
 *                    int32_t  arrayIndex, char **value
 *              mode=SDDSFILE_SETARRAY_WITH_NAME+SDDSFILE_SETARRAY_WITH_STRING,
 *                    char *name, char **value
 *              mode=SDDSFILE_SETARRAY_WITH_INDEX+SDDSFILE_SETARRAY_WITH_CHARACTER,
 *                    int32_t  arrayIndex, char *value
 *              mode=SDDSFILE_SETARRAY_WITH_NAME+SDDSFILE_SETARRAY_WITH_CHARACTER,
 *                    char *name, char *value
 *
 * Results:  0 for success
 *           1 for failure : calling program should call printErrors
 */
int32_t SDDSFile::setArray(int32_t arrayIndex, uint32_t page, int16_t *values, uint32_t elements) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::setArray)", arrayIndex);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(char *name, uint32_t page, int16_t *values, uint32_t elements) {
  int32_t arrayIndex, result;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::setArray)", name);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(int32_t arrayIndex, uint32_t page, uint16_t *values, uint32_t elements) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::setArray)", arrayIndex);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(char *name, uint32_t page, uint16_t *values, uint32_t elements) {
  int32_t arrayIndex, result;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::setArray)", name);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(int32_t arrayIndex, uint32_t page, int32_t *values, uint32_t elements) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::setArray)", arrayIndex);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(char *name, uint32_t page, int32_t *values, uint32_t elements) {
  int32_t arrayIndex, result;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::setArray)", name);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(int32_t arrayIndex, uint32_t page, uint32_t *values, uint32_t elements) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::setArray)", arrayIndex);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(char *name, uint32_t page, uint32_t *values, uint32_t elements) {
  int32_t arrayIndex, result;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::setArray)", name);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(int32_t arrayIndex, uint32_t page, float *values, uint32_t elements) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::setArray)", arrayIndex);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(char *name, uint32_t page, float *values, uint32_t elements) {
  int32_t arrayIndex, result;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::setArray)", name);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(int32_t arrayIndex, uint32_t page, double *values, uint32_t elements) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::setArray)", arrayIndex);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(char *name, uint32_t page, double *values, uint32_t elements) {
  int32_t arrayIndex, result;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::setArray)", name);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(int32_t arrayIndex, uint32_t page, char **values, uint32_t elements) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::setArray)", arrayIndex);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(char *name, uint32_t page, char **values, uint32_t elements) {
  int32_t arrayIndex, result;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::setArray)", name);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(int32_t arrayIndex, uint32_t page, char *values, uint32_t elements) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::setArray)", arrayIndex);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile::setArray(char *name, uint32_t page, char *values, uint32_t elements) {
  int32_t arrayIndex, result;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::setArray)", name);
    setError(s);
    return(1);
  }
  result = arrayObject[arrayIndex]->setValues(values, page, elements);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setArray)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper array type (SDDSFile::setArray)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid array type (SDDSFile::setArray)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setArray)");
  return(1);  
}

int32_t SDDSFile_setArray(void *sddsfile, uint32_t page, uint32_t elements, int32_t mode, ...) {
  va_list ap;
  int16_t *int16Val;
  uint16_t *uint16Val;
  int32_t *int32Val;
  uint32_t *uint32Val;
  float *real32Val;
  double *real64Val;
  char **stringVal;
  char *charVal;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_SETARRAY_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    if (mode&SDDSFILE_SETARRAY_WITH_INT16) {
      int16Val = va_arg(ap, int16_t*);
      return(((SDDSFile*)sddsfile)->setArray(index, page, int16Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_UINT16) {
      uint16Val = va_arg(ap, uint16_t*);
      return(((SDDSFile*)sddsfile)->setArray(index, page, uint16Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_INT32) {
      int32Val = va_arg(ap, int32_t*);
      return(((SDDSFile*)sddsfile)->setArray(index, page, int32Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_UINT32) {
      uint32Val = va_arg(ap, uint32_t*);
      return(((SDDSFile*)sddsfile)->setArray(index, page, uint32Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_REAL32) {
      real32Val = va_arg(ap, float*);
      return(((SDDSFile*)sddsfile)->setArray(index, page, real32Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_REAL64) {
      real64Val = va_arg(ap, double*);
      return(((SDDSFile*)sddsfile)->setArray(index, page, real64Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_STRING) {
      stringVal = va_arg(ap, char**);
      return(((SDDSFile*)sddsfile)->setArray(index, page, stringVal, elements));
    } else {
      charVal = va_arg(ap, char*);
      return(((SDDSFile*)sddsfile)->setArray(index, page, charVal, elements));
    }
  } else {
    name = va_arg(ap, char *);
    if (mode&SDDSFILE_SETARRAY_WITH_INT16) {
      int16Val = va_arg(ap, int16_t*);
      return(((SDDSFile*)sddsfile)->setArray(name, page, int16Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_UINT16) {
      uint16Val = va_arg(ap, uint16_t*);
      return(((SDDSFile*)sddsfile)->setArray(name, page, uint16Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_INT32) {
      int32Val = va_arg(ap, int32_t*);
      return(((SDDSFile*)sddsfile)->setArray(name, page, int32Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_UINT32) {
      uint32Val = va_arg(ap, uint32_t*);
      return(((SDDSFile*)sddsfile)->setArray(name, page, uint32Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_REAL32) {
      real32Val = va_arg(ap, float*);
      return(((SDDSFile*)sddsfile)->setArray(name, page, real32Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_REAL64) {
      real64Val = va_arg(ap, double*);
      return(((SDDSFile*)sddsfile)->setArray(name, page, real64Val, elements));
    } else if (mode&SDDSFILE_SETARRAY_WITH_STRING) {
      stringVal = va_arg(ap, char**);
      return(((SDDSFile*)sddsfile)->setArray(name, page, stringVal, elements));
    } else {
      charVal = va_arg(ap, char*);
      return(((SDDSFile*)sddsfile)->setArray(name, page, charVal, elements));
    }
  }
}

/***********************************************************************************************
 *                        setParameter                                                         *
 *                                                                                             *
 * C++ Arguments: uint32_t page, int32_t  parameterIndex, int32_t   value                       *
 *                uint32_t page, char    *name,           int32_t   value                       *
 *                uint32_t page, int32_t  parameterIndex, uint32_t  value                       *
 *                uint32_t page, char    *name,           uint32_t  value                       *
 *                uint32_t page, int32_t  parameterIndex, double    value                       *
 *                uint32_t page, char    *name,           double    value                       *
 *                uint32_t page, int32_t  parameterIndex, char     *value                       *
 *                uint32_t page, char    *name,           char     *value                       *
 *                uint32_t startPage, uint32_t pages, int32_t  parameterIndex, int32_t  *value   *
 *                uint32_t startPage, uint32_t pages, char    *name,           int32_t  *value   *
 *                uint32_t startPage, uint32_t pages, int32_t  parameterIndex, uint32_t *value   *
 *                uint32_t startPage, uint32_t pages, char    *name,           uint32_t *value   *
 *                uint32_t startPage, uint32_t pages, int32_t  parameterIndex, double   *value   *
 *                uint32_t startPage, uint32_t pages, char    *name,           double   *value   *
 *                uint32_t startPage, uint32_t pages, int32_t  parameterIndex, char    **value   *
 *                uint32_t startPage, uint32_t pages, char    *name,           char    **value   *
 *                                                                                             *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...                                *
 *              mode=SDDSFILE_SETPARAMETER_WITH_INDEX+SDDSFILE_SETPARAMETER_WITH_INT,          *
 *                    int32_t  parameterIndex, int32_t value                                   *
 *              mode=SDDSFILE_SETPARAMETER_WITH_NAME+SDDSFILE_SETPARAMETER_WITH_INT,           *
 *                    char *name, int32_t value                                                *
 *              mode=SDDSFILE_SETPARAMETER_WITH_INDEX+SDDSFILE_SETPARAMETER_WITH_UINT,         *
 *                    int32_t  parameterIndex, uint32_t value                                  *
 *              mode=SDDSFILE_SETPARAMETER_WITH_NAME+SDDSFILE_SETPARAMETER_WITH_UINT,          *
 *                    char *name, uint32_t value                                               *
 *              mode=SDDSFILE_SETPARAMETER_WITH_INDEX+SDDSFILE_SETPARAMETER_WITH_FLOAT,        *
 *                    int32_t  parameterIndex, double value                                    *
 *              mode=SDDSFILE_SETPARAMETER_WITH_NAME+SDDSFILE_SETPARAMETER_WITH_FLOAT,         *
 *                    char *name, double value                                                 *
 *              mode=SDDSFILE_SETPARAMETER_WITH_INDEX+SDDSFILE_SETPARAMETER_WITH_STRING,       *
 *                    int32_t  parameterIndex, char *value                                     *
 *              mode=SDDSFILE_SETPARAMETER_WITH_NAME+SDDSFILE_SETPARAMETER_WITH_STRING,        *
 *                    char *name, char *value                                                  *
 *                                                                                             *
 * Results:  0 for success                                                                     *
 *           1 for failure : calling program should call printErrors                           *
 ***********************************************************************************************/
int32_t SDDSFile::setParameter(uint32_t page, int32_t parameterIndex, int32_t value) {
  char s[SDDS_MAXLINE];
  int32_t result;
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::setParameter)", parameterIndex);
    setError(s);
    return(1);
  }
  result = parameterObject[parameterIndex]->setValue(value, page);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
  return(1);
}

int32_t SDDSFile::setParameter(uint32_t startPage, uint32_t pages, int32_t parameterIndex, int32_t *value) {
  int32_t result;
  uint32_t i;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::setParameter)", parameterIndex);
    setError(s);
    return(1);
  }
  for (i=0; i<pages; i++) {
    result = parameterObject[parameterIndex]->setValue(value[i], startPage + i);
    if (result == 0) {
      continue;
    } else if (result == 1) {
      setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
      return(1);
    } else if (result == 2) {
      setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
      return(1);
    } else if (result == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
      return(1);
    }
    setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
    return(1);  
  }
  return(0);
}

int32_t SDDSFile::setParameter(uint32_t page, char *name, int32_t value) {
  int32_t parameterIndex, result;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::setParameter)", name);
    setError(s);
    return(1);
  }
  result = parameterObject[parameterIndex]->setValue(value, page);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
  return(1);
}

int32_t SDDSFile::setParameter(uint32_t startPage, uint32_t pages, char *name, int32_t *value) {
  int32_t parameterIndex, result;
  uint32_t i;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::setParameter)", name);
    setError(s);
    return(1);
  }
  for (i=0; i<pages; i++) {
    result = parameterObject[parameterIndex]->setValue(value[i], startPage + i);
    if (result == 0) {
      continue;
    } else if (result == 1) {
      setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
      return(1);
    } else if (result == 2) {
      setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
      return(1);
    } else if (result == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
      return(1);
    }
    setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
    return(1);  
  }
  return(0);
}

int32_t SDDSFile::setParameter(uint32_t page, int32_t parameterIndex, uint32_t value) {
  char s[SDDS_MAXLINE];
  int32_t result;
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::setParameter)", parameterIndex);
    setError(s);
    return(1);
  }
  result = parameterObject[parameterIndex]->setValue(value, page);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
  return(1);
}

int32_t SDDSFile::setParameter(uint32_t startPage, uint32_t pages, int32_t parameterIndex, uint32_t *value) {
  int32_t result;
  uint32_t i;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::setParameter)", parameterIndex);
    setError(s);
    return(1);
  }
  for (i=0; i<pages; i++) {
    result = parameterObject[parameterIndex]->setValue(value[i], startPage + i);
    if (result == 0) {
      continue;
    } else if (result == 1) {
      setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
      return(1);
    } else if (result == 2) {
      setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
      return(1);
    } else if (result == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
      return(1);
    }
    setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
    return(1);  
  }
  return(0);
}

int32_t SDDSFile::setParameter(uint32_t page, char *name, uint32_t value) {
  int32_t parameterIndex, result;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::setParameter)", name);
    setError(s);
    return(1);
  }
  result = parameterObject[parameterIndex]->setValue(value, page);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
  return(1);
}

int32_t SDDSFile::setParameter(uint32_t startPage, uint32_t pages, char *name, uint32_t *value) {
  int32_t parameterIndex, result;
  uint32_t i;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::setParameter)", name);
    setError(s);
    return(1);
  }
  for (i=0; i<pages; i++) {
    result = parameterObject[parameterIndex]->setValue(value[i], startPage + i);
    if (result == 0) {
      continue;
    } else if (result == 1) {
      setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
      return(1);
    } else if (result == 2) {
      setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
      return(1);
    } else if (result == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
      return(1);
    }
    setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
    return(1);  
  }
  return(0);
}

int32_t SDDSFile::setParameter(uint32_t page, int32_t parameterIndex, double value) {
  char s[SDDS_MAXLINE];
  int32_t result;
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::setParameter)", parameterIndex);
    setError(s);
    return(1);
  }
  result = parameterObject[parameterIndex]->setValue(value, page);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
  return(1);
}

int32_t SDDSFile::setParameter(uint32_t startPage, uint32_t pages, int32_t parameterIndex, double *value) {
  int32_t result;
  uint32_t i;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::setParameter)", parameterIndex);
    setError(s);
    return(1);
  }
  for (i=0; i<pages; i++) {
    result = parameterObject[parameterIndex]->setValue(value[i], startPage + i);
    if (result == 0) {
      continue;
    } else if (result == 1) {
      setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
      return(1);
    } else if (result == 2) {
      setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
      return(1);
    } else if (result == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
      return(1);
    }
    setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
    return(1);  
  }
  return(0);
}

int32_t SDDSFile::setParameter(uint32_t page, char *name, double value) {
  int32_t parameterIndex, result;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::setParameter)", name);
    setError(s);
    return(1);
  }
  result = parameterObject[parameterIndex]->setValue(value, page);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
  return(1);
}

int32_t SDDSFile::setParameter(uint32_t startPage, uint32_t pages, char *name, double *value) {
  int32_t parameterIndex, result;
  uint32_t i;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::setParameter)", name);
    setError(s);
    return(1);
  }
  for (i=0; i<pages; i++) {
    result = parameterObject[parameterIndex]->setValue(value[i], startPage + i);
    if (result == 0) {
      continue;
    } else if (result == 1) {
      setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
      return(1);
    } else if (result == 2) {
      setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
      return(1);
    } else if (result == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
      return(1);
    }
    setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
    return(1);  
  }
  return(0);
}

int32_t SDDSFile::setParameter(uint32_t page, int32_t parameterIndex, char *value) {
  char s[SDDS_MAXLINE];
  int32_t result;
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::setParameter)", parameterIndex);
    setError(s);
    return(1);
  }
  result = parameterObject[parameterIndex]->setValue(value, page);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
  return(1);
}

int32_t SDDSFile::setParameter(uint32_t startPage, uint32_t pages, int32_t parameterIndex, char **value) {
  int32_t result;
  uint32_t i;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32"  does not exist (SDDSFile::setParameter)", parameterIndex);
    setError(s);
    return(1);
  }
  for (i=0; i<pages; i++) {
    result = parameterObject[parameterIndex]->setValue(value[i], startPage + i);
    if (result == 0) {
      continue;
    } else if (result == 1) {
      setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
      return(1);
    } else if (result == 2) {
      setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
      return(1);
    } else if (result == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
      return(1);
    }
    setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
    return(1);  
  }
  return(0);
}

int32_t SDDSFile::setParameter(uint32_t page, char *name, char *value) {
  int32_t parameterIndex, result;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::setParameter)", name);
    setError(s);
    return(1);
  }
  result = parameterObject[parameterIndex]->setValue(value, page);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
  return(1);
}

int32_t SDDSFile::setParameter(uint32_t startPage, uint32_t pages, char *name, char **value) {
  int32_t parameterIndex, result;
  uint32_t i;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::setParameter)", name);
    setError(s);
    return(1);
  }
  for (i=0; i<pages; i++) {
    result = parameterObject[parameterIndex]->setValue(value[i], startPage + i);
    if (result == 0) {
      continue;
    } else if (result == 1) {
      setError((char*)"Warning: invalid page number (SDDSFile::setParameter)");
      return(1);
    } else if (result == 2) {
      setError((char*)"Warning: unable to convert given value to proper parameter type (SDDSFile::setParameter)");
      return(1);
    } else if (result == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::setParameter)");
      return(1);
    }
    setError((char*)"Warning: unexpected error (SDDSFile::setParameter)");
    return(1);  
  }
  return(0);
}

int32_t SDDSFile_setParameter(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t intval;
  uint32_t uintval;
  double doubleval;
  char *stringval;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_SETPARAMETER_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    if (mode&SDDSFILE_SETPARAMETER_WITH_INT) {
      intval = va_arg(ap, int32_t);
      return(((SDDSFile*)sddsfile)->setParameter(page, index, intval));
    } else if (mode&SDDSFILE_SETPARAMETER_WITH_UINT) {
      uintval = va_arg(ap, uint32_t);
      return(((SDDSFile*)sddsfile)->setParameter(page, index, uintval));
    } else if (mode&SDDSFILE_SETPARAMETER_WITH_FLOAT) {
      doubleval = va_arg(ap, double);
      return(((SDDSFile*)sddsfile)->setParameter(page, index, doubleval));
    } else {
      stringval = va_arg(ap, char*);
      return(((SDDSFile*)sddsfile)->setParameter(page, index, stringval));
    }
  } else {
    name = va_arg(ap, char *);
    if (mode&SDDSFILE_SETPARAMETER_WITH_INT) {
      intval = va_arg(ap, int32_t);
      return(((SDDSFile*)sddsfile)->setParameter(page, name, intval));
    } else if (mode&SDDSFILE_SETPARAMETER_WITH_UINT) {
      uintval = va_arg(ap, uint32_t);
      return(((SDDSFile*)sddsfile)->setParameter(page, name, uintval));
    } else if (mode&SDDSFILE_SETPARAMETER_WITH_FLOAT) {
      doubleval = va_arg(ap, double);
      return(((SDDSFile*)sddsfile)->setParameter(page, name, doubleval));
    } else {
      stringval = va_arg(ap, char*);
      return(((SDDSFile*)sddsfile)->setParameter(page, name, stringval));
    }
  }
}

/******************************************************************************************************
 *                        setColumn                                                                   *
 *                                                                                                    *
 * C++ Arguments: int32_t columnIndex, uint32_t page, uint32_t startRow, int32_t *values, uint32_t rows  *
 *                char *name, uint32_t page, uint32_t startRow, int32_t *values, uint32_t rows           *
 *                int32_t columnIndex, uint32_t page, uint32_t startRow, uint32_t *values, uint32_t rows *
 *                char *name, uint32_t page, uint32_t startRow, uint32_t *values, uint32_t rows          *
 *                int32_t columnIndex, uint32_t page, uint32_t startRow, double *values, uint32_t rows   *
 *                char *name, uint32_t page, uint32_t startRow, double *values, uint32_t rows            *
 *                int32_t columnIndex, uint32_t page, uint32_t startRow, char **values, uint32_t rows    *
 *                char *name, uint32_t page, uint32_t startRow, char **values, uint32_t rows             *
 *                                                                                                    *
 * C Arguments: void *sddsfile, uint32_t page, uint32_t startRow, uint32_t rows, int32_t mode, ...       *
 *              mode=SDDSFILE_SETCOLUMN_WITH_INDEX+SDDSFILE_SETCOLUMN_WITH_INT16,                     *
 *                    int32_t  columnIndex, int16_t *value                                            *
 *              mode=SDDSFILE_SETCOLUMN_WITH_NAME+SDDSFILE_SETCOLUMN_WITH_INT16,                      *
 *                    char *name, int16_t *value                                                      *
 *              mode=SDDSFILE_SETCOLUMN_WITH_INDEX+SDDSFILE_SETCOLUMN_WITH_UINT16,                    *
 *                    int32_t  columnIndex, uint16_t *value                                           *
 *              mode=SDDSFILE_SETCOLUMN_WITH_NAME+SDDSFILE_SETCOLUMN_WITH_UINT16,                     *
 *                    char *name, uint16_t *value                                                     *
 *              mode=SDDSFILE_SETCOLUMN_WITH_INDEX+SDDSFILE_SETCOLUMN_WITH_INT32,                     *
 *                    int32_t  columnIndex, int32_t *value                                            *
 *              mode=SDDSFILE_SETCOLUMN_WITH_NAME+SDDSFILE_SETCOLUMN_WITH_INT32,                      *
 *                    char *name, int32_t *value                                                      *
 *              mode=SDDSFILE_SETCOLUMN_WITH_INDEX+SDDSFILE_SETCOLUMN_WITH_UINT32,                    *
 *                    int32_t  columnIndex, uint32_t *value                                           *
 *              mode=SDDSFILE_SETCOLUMN_WITH_NAME+SDDSFILE_SETCOLUMN_WITH_UINT32,                     *
 *                    char *name, uint32_t *value                                                     *
 *              mode=SDDSFILE_SETCOLUMN_WITH_INDEX+SDDSFILE_SETCOLUMN_WITH_REAL32,                    *
 *                    int32_t  columnIndex, float *value                                              *
 *              mode=SDDSFILE_SETCOLUMN_WITH_NAME+SDDSFILE_SETCOLUMN_WITH_REAL32,                     *
 *                    char *name, float *value                                                        *
 *              mode=SDDSFILE_SETCOLUMN_WITH_INDEX+SDDSFILE_SETCOLUMN_WITH_REAL64,                    *
 *                    int32_t  columnIndex, double *value                                             *
 *              mode=SDDSFILE_SETCOLUMN_WITH_NAME+SDDSFILE_SETCOLUMN_WITH_REAL64,                     *
 *                    char *name, double *value                                                       *
 *              mode=SDDSFILE_SETCOLUMN_WITH_INDEX+SDDSFILE_SETCOLUMN_WITH_STRING,                    *
 *                    int32_t  columnIndex, char **value                                              *
 *              mode=SDDSFILE_SETCOLUMN_WITH_NAME+SDDSFILE_SETCOLUMN_WITH_STRING,                     *
 *                    char *name, char **value                                                        *
 *              mode=SDDSFILE_SETCOLUMN_WITH_INDEX+SDDSFILE_SETCOLUMN_WITH_CHARACTER,                 *
 *                    int32_t  columnIndex, char *value                                               *
 *              mode=SDDSFILE_SETCOLUMN_WITH_NAME+SDDSFILE_SETCOLUMN_WITH_CHARACTER,                  *
 *                    char *name, char *value                                                         *
 *                                                                                                    *
 * Results:  0 for success                                                                            *
 *           1 for failure : calling program should call printErrors                                  *
 ******************************************************************************************************/
int32_t SDDSFile::setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, int16_t *values, uint32_t rows) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::setColumn)", columnIndex);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, int16_t *values, uint32_t rows) {
  int32_t columnIndex, result;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::setColumn)", name);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, uint16_t *values, uint32_t rows) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::setColumn)", columnIndex);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, uint16_t *values, uint32_t rows) {
  int32_t columnIndex, result;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::setColumn)", name);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, int32_t *values, uint32_t rows) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::setColumn)", columnIndex);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, int32_t *values, uint32_t rows) {
  int32_t columnIndex, result;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::setColumn)", name);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, uint32_t *values, uint32_t rows) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::setColumn)", columnIndex);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, uint32_t *values, uint32_t rows) {
  int32_t columnIndex, result;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::setColumn)", name);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, float *values, uint32_t rows) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::setColumn)", columnIndex);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, float *values, uint32_t rows) {
  int32_t columnIndex, result;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::setColumn)", name);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, double *values, uint32_t rows) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::setColumn)", columnIndex);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, double *values, uint32_t rows) {
  int32_t columnIndex, result;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::setColumn)", name);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, char **values, uint32_t rows) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::setColumn)", columnIndex);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, char **values, uint32_t rows) {
  int32_t columnIndex, result;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::setColumn)", name);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, char *values, uint32_t rows) {
  int32_t result;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::setColumn)", columnIndex);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile::setColumn(char *name, uint32_t page, uint32_t startRow, char *values, uint32_t rows) {
  int32_t columnIndex, result;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::setColumn)", name);
    setError(s);
    return(1);
  }
  result = columnObject[columnIndex]->setValues(values, page, startRow, rows);
  if (result == 0) {
    return(0);
  } else if (result == 1) {
    setError((char*)"Warning: invalid page number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 2) {
    setError((char*)"Warning: invalid startRow number (SDDSFile::setColumn)");
    return(1);
  } else if (result == 3) {
    setError((char*)"Warning: unable to convert given value to proper column type (SDDSFile::setColumn)");
    return(1);
  } else if (result == 4) {
    setError((char*)"Warning: invalid column type (SDDSFile::setColumn)");
    return(1);
  }
  setError((char*)"Warning: unexpected error (SDDSFile::setColumn)");
  return(1);  
}

int32_t SDDSFile_setColumn(void *sddsfile, uint32_t page, uint32_t startRow, uint32_t rows, int32_t mode, ...) {
  va_list ap;
  int16_t *int16Val;
  uint16_t *uint16Val;
  int32_t *int32Val;
  uint32_t *uint32Val;
  float *real32Val;
  double *real64Val;
  char **stringVal;
  char *charVal;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_SETCOLUMN_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    if (mode&SDDSFILE_SETCOLUMN_WITH_INT16) {
      int16Val = va_arg(ap, int16_t*);
      return(((SDDSFile*)sddsfile)->setColumn(index, page, startRow, int16Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_UINT16) {
      uint16Val = va_arg(ap, uint16_t*);
      return(((SDDSFile*)sddsfile)->setColumn(index, page, startRow, uint16Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_INT32) {
      int32Val = va_arg(ap, int32_t*);
      return(((SDDSFile*)sddsfile)->setColumn(index, page, startRow, int32Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_UINT32) {
      uint32Val = va_arg(ap, uint32_t*);
      return(((SDDSFile*)sddsfile)->setColumn(index, page, startRow, uint32Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_REAL32) {
      real32Val = va_arg(ap, float*);
      return(((SDDSFile*)sddsfile)->setColumn(index, page, startRow, real32Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_REAL64) {
      real64Val = va_arg(ap, double*);
      return(((SDDSFile*)sddsfile)->setColumn(index, page, startRow, real64Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_STRING) {
      stringVal = va_arg(ap, char**);
      return(((SDDSFile*)sddsfile)->setColumn(index, page, startRow, stringVal, rows));
    } else {
      charVal = va_arg(ap, char*);
      return(((SDDSFile*)sddsfile)->setColumn(index, page, startRow, charVal, rows));
    }
  } else {
    name = va_arg(ap, char *);
    if (mode&SDDSFILE_SETCOLUMN_WITH_INT16) {
      int16Val = va_arg(ap, int16_t*);
      return(((SDDSFile*)sddsfile)->setColumn(name, page, startRow, int16Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_UINT16) {
      uint16Val = va_arg(ap, uint16_t*);
      return(((SDDSFile*)sddsfile)->setColumn(name, page, startRow, uint16Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_INT32) {
      int32Val = va_arg(ap, int32_t*);
      return(((SDDSFile*)sddsfile)->setColumn(name, page, startRow, int32Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_UINT32) {
      uint32Val = va_arg(ap, uint32_t*);
      return(((SDDSFile*)sddsfile)->setColumn(name, page, startRow, uint32Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_REAL32) {
      real32Val = va_arg(ap, float*);
      return(((SDDSFile*)sddsfile)->setColumn(name, page, startRow, real32Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_REAL64) {
      real64Val = va_arg(ap, double*);
      return(((SDDSFile*)sddsfile)->setColumn(name, page, startRow, real64Val, rows));
    } else if (mode&SDDSFILE_SETCOLUMN_WITH_STRING) {
      stringVal = va_arg(ap, char**);
      return(((SDDSFile*)sddsfile)->setColumn(name, page, startRow, stringVal, rows));
    } else {
      charVal = va_arg(ap, char*);
      return(((SDDSFile*)sddsfile)->setColumn(name, page, startRow, charVal, rows));
    }
  }
}

/***********************************************************************************************
 *                        getParameterInInt32                                                  *
 *                                                                                             *
 * C++ Arguments: int32_t parameterIndex, uint32_t page                                         *
 *                char *name, uint32_t page                                                     *
 *                                                                                             *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...                                *
 *              mode=SDDSFILE_GETPARAMETER_WITH_INDEX, int32_t  parameterIndex                 *
 *              mode=SDDSFILE_GETPARAMETER_WITH_NAME, char *name                               *
 *                                                                                             *
 * Results:  int32 value of the parameter on success                                           *
 *           0 on failure                                                                      *
 *             You should should call checkForErrors to                                        *
 *             determain if there was a problem getting the value                              *
 ***********************************************************************************************/
int32_t SDDSFile::getParameterInInt32(int32_t parameterIndex, uint32_t page) {
  int64_t result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::getParameterInInt32)", parameterIndex);
    setError(s);
    return((int32_t)0);
  }
  result = parameterObject[parameterIndex]->getValueInInt32(page, &errorcode);
  if (result == LONG_MAX) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInInt32)", getParameterName(parameterIndex));
    setError(s);
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInInt32)");
    }
    return((int32_t)0);
  }
  return((int32_t)result);
}

int32_t SDDSFile::getParameterInInt32(char *name, uint32_t page) {
  int64_t result;
  int32_t errorcode;
  int32_t parameterIndex;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::getParameterInInt32)", name);
    setError(s);
    return((int32_t)0);
  }
  result = parameterObject[parameterIndex]->getValueInInt32(page, &errorcode);
  if (result == LONG_MAX) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInInt32)", name);
    setError(s);
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInInt32)");
    }
    return((int32_t)0);
  }
  return((int32_t)result);
}

int32_t SDDSFile_getParameterInInt32(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETPARAMETER_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getParameterInInt32(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getParameterInInt32(name, page));
  }
}

/***********************************************************************************************
 *                        getParameterInUInt32                                                 *
 *                                                                                             *
 * C++ Arguments: int32_t parameterIndex, uint32_t page                                         *
 *                char *name, uint32_t page                                                     *
 *                                                                                             *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...                                *
 *              mode=SDDSFILE_GETPARAMETER_WITH_INDEX, int32_t  parameterIndex                 *
 *              mode=SDDSFILE_GETPARAMETER_WITH_NAME, char *name                               *
 *                                                                                             *
 * Results:  uint32 value of the parameter on success                                          *
 *           0 on failure                                                                      *
 *             You should should call checkForErrors to                                        *
 *             determain if there was a problem getting the value                              *
 ***********************************************************************************************/
uint32_t SDDSFile::getParameterInUInt32(int32_t parameterIndex, uint32_t page) {
  uint64_t result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::getParameterInUInt32)", parameterIndex);
    setError(s);
    return((uint32_t)0);
  }
  result = parameterObject[parameterIndex]->getValueInUInt32(page, &errorcode);
  if (result == ULONG_MAX) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInUInt32)", getParameterName(parameterIndex));
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInUInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInUInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInUInt32)");
    }
    return((uint32_t)0);
  }
  return((uint32_t)result);
}

uint32_t SDDSFile::getParameterInUInt32(char *name, uint32_t page) {
  uint64_t result;
  int32_t errorcode;
  int32_t parameterIndex;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::getParameterInUInt32)", name);
    setError(s);
    return((uint32_t)0);
  }
  result = parameterObject[parameterIndex]->getValueInUInt32(page, &errorcode);
  if (result == ULONG_MAX) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInUInt32)", name);
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInUInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInUInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInUInt32)");
    }
    return((uint32_t)0);
  }
  return((uint32_t)result);
}

uint32_t SDDSFile_getParameterInUInt32(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETPARAMETER_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getParameterInUInt32(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getParameterInUInt32(name, page));
  }
}

/***********************************************************************************************
 *                        getParameterInDouble                                                 *
 *                                                                                             *
 * C++ Arguments: int32_t parameterIndex, uint32_t page                                         *
 *                char *name, uint32_t page                                                     *
 *                                                                                             *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...                                *
 *              mode=SDDSFILE_GETPARAMETER_WITH_INDEX, int32_t  parameterIndex                 *
 *              mode=SDDSFILE_GETPARAMETER_WITH_NAME, char *name                               *
 *                                                                                             *
 * Results:  double value of the parameter on success                                          *
 *           0 on failure                                                                      *
 *             You should should call checkForErrors to                                        *
 *             determain if there was a problem getting the value                              *
 ***********************************************************************************************/
double SDDSFile::getParameterInDouble(int32_t parameterIndex, uint32_t page) {
  double result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::getParameterInDouble)", parameterIndex);
    setError(s);
    return((double)0);
  }
  result = parameterObject[parameterIndex]->getValueInDouble(page, &errorcode);
  if (result == HUGE_VAL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInDouble)", getParameterName(parameterIndex));
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInDouble)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInDouble)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInDouble)");
    }
    return((double)0);
  }
  return((double)result);
}

double SDDSFile::getParameterInDouble(char *name, uint32_t page) {
  double result;
  int32_t errorcode;
  int32_t parameterIndex;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::getParameterInDouble)", name);
    setError(s);
    return((double)0);
  }
  result = parameterObject[parameterIndex]->getValueInDouble(page, &errorcode);
  if (result == HUGE_VAL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInDouble)", name);
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInDouble)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInDouble)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInDouble)");
    }
    return((double)0);
  }
  return((double)result);
}

double SDDSFile_getParameterInDouble(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETPARAMETER_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getParameterInDouble(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getParameterInDouble(name, page));
  }
}

/***********************************************************************************************
 *                        getParameterInString                                                 *
 *                                                                                             *
 * C++ Arguments: int32_t parameterIndex, uint32_t page                                         *
 *                char *name, uint32_t page                                                     *
 *                                                                                             *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...                                *
 *              mode=SDDSFILE_GETPARAMETER_WITH_INDEX, int32_t  parameterIndex                 *
 *              mode=SDDSFILE_GETPARAMETER_WITH_NAME, char *name                               *
 *                                                                                             *
 * Results:  char* value of the parameter on success                                           *
 *             You should free this pointer to avoid a memory leak                             *
 *           NULL on failure                                                                   *
 ***********************************************************************************************/
char* SDDSFile::getParameterInString(int32_t parameterIndex, uint32_t page) {
  char* result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::getParameterInString)", parameterIndex);
    setError(s);
    return((char*)NULL);
  }
  result = parameterObject[parameterIndex]->getValueInString(page, &errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInString)", getParameterName(parameterIndex));
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInString)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInString)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInString)");
    }
    return((char*)NULL);
  }
  return((char*)result);
}

char* SDDSFile::getParameterInString(char *name, uint32_t page) {
  char* result;
  int32_t errorcode;
  int32_t parameterIndex;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if (parameterIndex < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::getParameterInString)", name);
    setError(s);
    return((char*)NULL);
  }
  result = parameterObject[parameterIndex]->getValueInString(page, &errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInString)", name);
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInString)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInString)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInString)");
    }
    return((char*)NULL);
  }
  return((char*)result);
}

char* SDDSFile_getParameterInString(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETPARAMETER_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getParameterInString(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getParameterInString(name, page));
  }
}

/*
 * getInteralArray
 *
 * C++ Arguments: int32_t arrayIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETARRAY_WITH_INDEX, int32_t  arrayIndex
 *              mode=SDDSFILE_GETARRAY_WITH_NAME, char *name
 *
 * Results:  pointer to values of the array on success
 *           NULL on failure
 */
void* SDDSFile::getInternalArray(int32_t arrayIndex, uint32_t page) {
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::getInternalArray)", arrayIndex);
    setError(s);
    return(NULL);
  }
  return(arrayObject[arrayIndex]->getInternalValues(page));
}
void* SDDSFile::getInternalArray(char *name, uint32_t page) {
  int32_t arrayIndex;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::getInternalArray)", name);
    setError(s);
    return(NULL);
  }
  return(arrayObject[arrayIndex]->getInternalValues(page));
}
void* SDDSFile_getInternalArray(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETARRAY_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getInternalArray(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getInternalArray(name, page));
  }
}

/*
 * getArrayInInt32
 *
 * C++ Arguments: int32_t arrayIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETARRAY_WITH_INDEX, int32_t  arrayIndex
 *              mode=SDDSFILE_GETARRAY_WITH_NAME, char *name
 *
 * Results:  pointer to int32 values of the array on success (you must free the pointer)
 *           NULL on failure
 *             You should should call checkForErrors to
 *             determain if there was a problem getting the value because NULL is a valid
 *             return value if there are zero rows
 */
int32_t* SDDSFile::getArrayInInt32(int32_t arrayIndex, uint32_t page) {
  int32_t *result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::getArrayInInt32)", arrayIndex);
    setError(s);
    return((int32_t*)NULL);
  }
  result = arrayObject[arrayIndex]->getValuesInInt32(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: array value undefined (SDDSFile::getArrayInInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getArrayInInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid array type (SDDSFile::getArrayInInt32)");
    }
  }
  return((int32_t*)result);
}

int32_t* SDDSFile::getArrayInInt32(char *name, uint32_t page) {
  int32_t *result;
  int32_t errorcode;
  int32_t arrayIndex;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::getArrayInInt32)", name);
    setError(s);
    return((int32_t*)NULL);
  }
  result = arrayObject[arrayIndex]->getValuesInInt32(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: array value undefined (SDDSFile::getArrayInInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getArrayInInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid array type (SDDSFile::getArrayInInt32)");
    }
  }
  return((int32_t*)result);
}

int32_t* SDDSFile_getArrayInInt32(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETARRAY_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getArrayInInt32(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getArrayInInt32(name, page));
  }
}

/*
 * getArrayInUInt32
 *
 * C++ Arguments: int32_t arrayIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETARRAY_WITH_INDEX, int32_t  arrayIndex
 *              mode=SDDSFILE_GETARRAY_WITH_NAME, char *name
 *
 * Results:  pointer to uint32 values of the array on success (you must free the pointer)
 *           NULL on failure
 *             You should should call checkForErrors to
 *             determain if there was a problem getting the value because NULL is a valid
 *             return value if there are zero elements
 */
uint32_t* SDDSFile::getArrayInUInt32(int32_t arrayIndex, uint32_t page) {
  uint32_t *result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::getArrayInUInt32)", arrayIndex);
    setError(s);
    return((uint32_t*)NULL);
  }
  result = arrayObject[arrayIndex]->getValuesInUInt32(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: array value undefined (SDDSFile::getArrayInUInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getArrayInUInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid array type (SDDSFile::getArrayInUInt32)");
    }
  }
  return((uint32_t*)result);
}

uint32_t* SDDSFile::getArrayInUInt32(char *name, uint32_t page) {
  uint32_t *result;
  int32_t errorcode;
  int32_t arrayIndex;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::getArrayInUInt32)", name);
    setError(s);
    return((uint32_t*)NULL);
  }
  result = arrayObject[arrayIndex]->getValuesInUInt32(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: array value undefined (SDDSFile::getArrayInUInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getArrayInUInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid array type (SDDSFile::getArrayInUInt32)");
    }
  }
  return((uint32_t*)result);
}

uint32_t* SDDSFile_getArrayInUInt32(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETARRAY_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getArrayInUInt32(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getArrayInUInt32(name, page));
  }
}

/*
 * getArrayInDouble
 *
 * C++ Arguments: int32_t arrayIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETARRAY_WITH_INDEX, int32_t  arrayIndex
 *              mode=SDDSFILE_GETARRAY_WITH_NAME, char *name
 *
 * Results:  pointer to double values of the array on success (you must free the pointer)
 *           NULL on failure
 *             You should should call checkForErrors to
 *             determain if there was a problem getting the value because NULL is a valid
 *             return value if there are zero elements
 */
double* SDDSFile::getArrayInDouble(int32_t arrayIndex, uint32_t page) {
  double *result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::getArrayInDouble)", arrayIndex);
    setError(s);
    return((double*)NULL);
  }
  result = arrayObject[arrayIndex]->getValuesInDouble(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: array value undefined (SDDSFile::getArrayInDouble)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getArrayInDouble)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid array type (SDDSFile::getArrayInDouble)");
    }
  }
  return((double*)result);
}

double* SDDSFile::getArrayInDouble(char *name, uint32_t page) {
  double *result;
  int32_t errorcode;
  int32_t arrayIndex;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::getArrayInDouble)", name);
    setError(s);
    return((double*)NULL);
  }
  result = arrayObject[arrayIndex]->getValuesInDouble(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: array value undefined (SDDSFile::getArrayInDouble)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getArrayInDouble)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid array type (SDDSFile::getArrayInDouble)");
    }
  }
  return((double*)result);
}

double* SDDSFile_getArrayInDouble(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETARRAY_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getArrayInDouble(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getArrayInDouble(name, page));
  }
}

/*
 * getArrayInString
 *
 * C++ Arguments: int32_t arrayIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETARRAY_WITH_INDEX, int32_t  arrayIndex
 *              mode=SDDSFILE_GETARRAY_WITH_NAME, char *name
 *
 * Results:  pointer to double values of the array on success (you must free the pointer and
 *                                                              the elements)
 *           NULL on failure
 *             You should should call checkForErrors to
 *             determain if there was a problem getting the value because NULL is a valid
 *             return value if there are zero elements
 */
char** SDDSFile::getArrayInString(int32_t arrayIndex, uint32_t page) {
  char **result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((arrayIndex < 0) || (arrayIndex >= arrayCount)) {
    sprintf(s, "Warning: array index %"PRId32" does not exist (SDDSFile::getArrayInString)", arrayIndex);
    setError(s);
    return((char**)NULL);
  }
  result = arrayObject[arrayIndex]->getValuesInString(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: array value undefined (SDDSFile::getArrayInString)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getArrayInString)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid array type (SDDSFile::getArrayInString)");
    }
  }
  return((char**)result);
}

char** SDDSFile::getArrayInString(char *name, uint32_t page) {
  char **result;
  int32_t errorcode;
  int32_t arrayIndex;
  char s[SDDS_MAXLINE];
  arrayIndex = getArrayIndex(name);
  if (arrayIndex < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::getArrayInString)", name);
    setError(s);
    return((char**)NULL);
  }
  result = arrayObject[arrayIndex]->getValuesInString(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: array value undefined (SDDSFile::getArrayInString)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getArrayInString)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid array type (SDDSFile::getArrayInString)");
    }
  }
  return((char**)result);
}

char** SDDSFile_getArrayInString(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETARRAY_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getArrayInString(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getArrayInString(name, page));
  }
}

/*
 * getInteralColumn
 *
 * C++ Arguments: int32_t columnIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETCOLUMN_WITH_INDEX, int32_t  columnIndex
 *              mode=SDDSFILE_GETCOLUMN_WITH_NAME, char *name
 *
 * Results:  pointer to values of the column on success
 *           NULL on failure
 */
void* SDDSFile::getInternalColumn(int32_t columnIndex, uint32_t page) {
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::getInternalColumn)", columnIndex);
    setError(s);
    return(NULL);
  }
  return(columnObject[columnIndex]->getInternalValues(page));
}
void* SDDSFile::getInternalColumn(char *name, uint32_t page) {
  int32_t columnIndex;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::getInternalColumn)", name);
    setError(s);
    return(NULL);
  }
  return(columnObject[columnIndex]->getInternalValues(page));
}
void* SDDSFile_getInternalColumn(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETCOLUMN_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getInternalColumn(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getInternalColumn(name, page));
  }
}

/*
 * getColumnInInt32
 *
 * C++ Arguments: int32_t columnIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETCOLUMN_WITH_INDEX, int32_t  columnIndex
 *              mode=SDDSFILE_GETCOLUMN_WITH_NAME, char *name
 *
 * Results:  pointer to int32 values of the column on success (you must free the pointer)
 *           NULL on failure
 *             You should should call checkForErrors to
 *             determain if there was a problem getting the value because NULL is a valid
 *             return value if there are zero rows
 */
int32_t* SDDSFile::getColumnInInt32(int32_t columnIndex, uint32_t page) {
  int32_t *result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::getColumnInInt32)", columnIndex);
    setError(s);
    return((int32_t*)NULL);
  }
  result = columnObject[columnIndex]->getValuesInInt32(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: column value undefined (SDDSFile::getColumnInInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getColumnInInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid column type (SDDSFile::getColumnInInt32)");
    }
  }
  return((int32_t*)result);
}

int32_t* SDDSFile::getColumnInInt32(char *name, uint32_t page) {
  int32_t *result;
  int32_t errorcode;
  int32_t columnIndex;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::getColumnInInt32)", name);
    setError(s);
    return((int32_t*)NULL);
  }
  result = columnObject[columnIndex]->getValuesInInt32(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: column value undefined (SDDSFile::getColumnInInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getColumnInInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid column type (SDDSFile::getColumnInInt32)");
    }
  }
  return((int32_t*)result);
}

int32_t* SDDSFile_getColumnInInt32(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETCOLUMN_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getColumnInInt32(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getColumnInInt32(name, page));
  }
}

/*
 * getColumnInUInt32
 *
 * C++ Arguments: int32_t columnIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETCOLUMN_WITH_INDEX, int32_t  columnIndex
 *              mode=SDDSFILE_GETCOLUMN_WITH_NAME, char *name
 *
 * Results:  pointer to uint32 values of the column on success (you must free the pointer)
 *           NULL on failure
 *             You should should call checkForErrors to
 *             determain if there was a problem getting the value because NULL is a valid
 *             return value if there are zero rows
 */
uint32_t* SDDSFile::getColumnInUInt32(int32_t columnIndex, uint32_t page) {
  uint32_t *result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::getColumnInUInt32)", columnIndex);
    setError(s);
    return((uint32_t*)NULL);
  }
  result = columnObject[columnIndex]->getValuesInUInt32(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: column value undefined (SDDSFile::getColumnInUInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getColumnInUInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid column type (SDDSFile::getColumnInUInt32)");
    }
  }
  return((uint32_t*)result);
}

uint32_t* SDDSFile::getColumnInUInt32(char *name, uint32_t page) {
  uint32_t *result;
  int32_t errorcode;
  int32_t columnIndex;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::getColumnInUInt32)", name);
    setError(s);
    return((uint32_t*)NULL);
  }
  result = columnObject[columnIndex]->getValuesInUInt32(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: column value undefined (SDDSFile::getColumnInUInt32)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getColumnInUInt32)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid column type (SDDSFile::getColumnInUInt32)");
    }
  }
  return((uint32_t*)result);
}

uint32_t* SDDSFile_getColumnInUInt32(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETCOLUMN_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getColumnInUInt32(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getColumnInUInt32(name, page));
  }
}

/*
 * getColumnInDouble
 *
 * C++ Arguments: int32_t columnIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETCOLUMN_WITH_INDEX, int32_t  columnIndex
 *              mode=SDDSFILE_GETCOLUMN_WITH_NAME, char *name
 *
 * Results:  pointer to double values of the column on success (you must free the pointer)
 *           NULL on failure
 *             You should should call checkForErrors to
 *             determain if there was a problem getting the value because NULL is a valid
 *             return value if there are zero rows
 */
double* SDDSFile::getColumnInDouble(int32_t columnIndex, uint32_t page) {
  double *result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::getColumnInDouble)", columnIndex);
    setError(s);
    return((double*)NULL);
  }
  result = columnObject[columnIndex]->getValuesInDouble(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: column value undefined (SDDSFile::getColumnInDouble)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getColumnInDouble)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid column type (SDDSFile::getColumnInDouble)");
    }
  }
  return((double*)result);
}

double* SDDSFile::getColumnInDouble(char *name, uint32_t page) {
  double *result;
  int32_t errorcode;
  int32_t columnIndex;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::getColumnInDouble)", name);
    setError(s);
    return((double*)NULL);
  }
  result = columnObject[columnIndex]->getValuesInDouble(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: column value undefined (SDDSFile::getColumnInDouble)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getColumnInDouble)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid column type (SDDSFile::getColumnInDouble)");
    }
  }
  return((double*)result);
}

double* SDDSFile_getColumnInDouble(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETCOLUMN_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getColumnInDouble(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getColumnInDouble(name, page));
  }
}

/*
 * getColumnInString
 *
 * C++ Arguments: int32_t columnIndex, uint32_t page
 *                char *name, uint32_t page
 *
 * C Arguments: void *sddsfile, uint32_t page, int32_t mode, ...
 *              mode=SDDSFILE_GETCOLUMN_WITH_INDEX, int32_t  columnIndex
 *              mode=SDDSFILE_GETCOLUMN_WITH_NAME, char *name
 *
 * Results:  pointer to double values of the column on success (you must free the pointer and
 *                                                              the elements)
 *           NULL on failure
 *             You should should call checkForErrors to
 *             determain if there was a problem getting the value because NULL is a valid
 *             return value if there are zero rows
 */
char** SDDSFile::getColumnInString(int32_t columnIndex, uint32_t page) {
  char **result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((columnIndex < 0) || (columnIndex >= columnCount)) {
    sprintf(s, "Warning: column index %"PRId32" does not exist (SDDSFile::getColumnInString)", columnIndex);
    setError(s);
    return((char**)NULL);
  }
  result = columnObject[columnIndex]->getValuesInString(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: column value undefined (SDDSFile::getColumnInString)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getColumnInString)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid column type (SDDSFile::getColumnInString)");
    }
  }
  return((char**)result);
}

char** SDDSFile::getColumnInString(char *name, uint32_t page) {
  char **result;
  int32_t errorcode;
  int32_t columnIndex;
  char s[SDDS_MAXLINE];
  columnIndex = getColumnIndex(name);
  if (columnIndex < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::getColumnInString)", name);
    setError(s);
    return((char**)NULL);
  }
  result = columnObject[columnIndex]->getValuesInString(page, &errorcode);
  if (result == NULL) {
    if (errorcode == 1) {
      setError((char*)"Warning: column value undefined (SDDSFile::getColumnInString)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getColumnInString)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid column type (SDDSFile::getColumnInString)");
    }
  }
  return((char**)result);
}

char** SDDSFile_getColumnInString(void *sddsfile, uint32_t page, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETCOLUMN_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getColumnInString(index, page));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getColumnInString(name, page));
  }
}

/***********************************************************************************************
 *                        getParameterInInt32s                                                 *
 *                                                                                             *
 * C++ Arguments: int32_t parameterIndex                                                       *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t mode, ...                                              *
 *              mode=SDDSFILE_GETPARAMETER_WITH_INDEX, int32_t  parameterIndex                 *
 *              mode=SDDSFILE_GETPARAMETER_WITH_NAME, char *name                               *
 *                                                                                             *
 * Results:  int32* values of the parameter on success                                         *
 *             You should free this pointer to avoid a memory leak                             *
 *           NULL on failure                                                                   *
 ***********************************************************************************************/
int32_t* SDDSFile::getParameterInInt32s(int32_t parameterIndex) {
  int32_t* result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::getParameterInInt32s)", parameterIndex);
    setError(s);
    return((int32_t*)NULL);
  }
  result = parameterObject[parameterIndex]->getValuesInInt32(&errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInInt32s)", getParameterName(parameterIndex));
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInInt32s)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInInt32s)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInInt32s)");
    }
    return((int32_t*)NULL);
  }
  return((int32_t*)result);
}

int32_t* SDDSFile::getParameterInInt32s(char* name) {
  int32_t* result;
  int32_t errorcode;
  int32_t parameterIndex;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::getParameterInInt32s)", name);
    setError(s);
    return((int32_t*)NULL);
  }
  result = parameterObject[parameterIndex]->getValuesInInt32(&errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInInt32s)", name);
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInInt32s)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInInt32s)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInInt32s)");
    }
    return((int32_t*)NULL);
  }
  return((int32_t*)result);
}

int32_t* SDDSFile_getParameterInInt32s(void *sddsfile, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETPARAMETER_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getParameterInInt32s(index));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getParameterInInt32s(name));
  }
}

/***********************************************************************************************
 *                        getParameterInUInt32s                                                *
 *                                                                                             *
 * C++ Arguments: int32_t parameterIndex                                                       *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t mode, ...                                              *
 *              mode=SDDSFILE_GETPARAMETER_WITH_INDEX, int32_t  parameterIndex                 *
 *              mode=SDDSFILE_GETPARAMETER_WITH_NAME, char *name                               *
 *                                                                                             *
 * Results:  uint32* values of the parameter on success                                        *
 *             You should free this pointer to avoid a memory leak                             *
 *           NULL on failure                                                                   *
 ***********************************************************************************************/
uint32_t* SDDSFile::getParameterInUInt32s(int32_t parameterIndex) {
  uint32_t* result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::getParameterInUInt32s)", parameterIndex);
    setError(s);
    return((uint32_t*)NULL);
  }
  result = parameterObject[parameterIndex]->getValuesInUInt32(&errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInUInt32s)", getParameterName(parameterIndex));
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInUInt32s)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInUInt32s)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInUInt32s)");
    }
    return((uint32_t*)NULL);
  }
  return((uint32_t*)result);
}

uint32_t* SDDSFile::getParameterInUInt32s(char* name) {
  uint32_t* result;
  int32_t errorcode;
  int32_t parameterIndex;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::getParameterInUInt32s)", name);
    setError(s);
    return((uint32_t*)NULL);
  }
  result = parameterObject[parameterIndex]->getValuesInUInt32(&errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInUInt32s)", name);
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInUInt32s)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInUInt32s)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInUInt32s)");
    }
    return((uint32_t*)NULL);
  }
  return((uint32_t*)result);
}

uint32_t* SDDSFile_getParameterInUInt32s(void *sddsfile, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETPARAMETER_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getParameterInUInt32s(index));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getParameterInUInt32s(name));
  }
}

/***********************************************************************************************
 *                        getParameterInDoubles                                                *
 *                                                                                             *
 * C++ Arguments: int32_t parameterIndex                                                       *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t mode, ...                                              *
 *              mode=SDDSFILE_GETPARAMETER_WITH_INDEX, int32_t  parameterIndex                 *
 *              mode=SDDSFILE_GETPARAMETER_WITH_NAME, char *name                               *
 *                                                                                             *
 * Results:  double* values of the parameter on success                                        *
 *             You should free this pointer to avoid a memory leak                             *
 *           NULL on failure                                                                   *
 ***********************************************************************************************/
double* SDDSFile::getParameterInDoubles(int32_t parameterIndex) {
  double* result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::getParameterInDoubles)", parameterIndex);
    setError(s);
    return((double*)NULL);
  }
  result = parameterObject[parameterIndex]->getValuesInDouble(&errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInDoubles)", getParameterName(parameterIndex));
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInDoubles)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInDoubles)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInDoubles)");
    }
    return((double*)NULL);
  }
  return((double*)result);
}

double* SDDSFile::getParameterInDoubles(char* name) {
  double* result;
  int32_t errorcode;
  int32_t parameterIndex;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::getParameterInDoubles)", name);
    setError(s);
    return((double*)NULL);
  }
  result = parameterObject[parameterIndex]->getValuesInDouble(&errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInDoubles)", name);
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInDoubles)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInDoubles)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInDoubles)");
    }
    return((double*)NULL);
  }
  return((double*)result);
}

double* SDDSFile_getParameterInDoubles(void *sddsfile, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETPARAMETER_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getParameterInDoubles(index));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getParameterInDoubles(name));
  }
}

/***********************************************************************************************
 *                        getParameterInStrings                                                *
 *                                                                                             *
 * C++ Arguments: int32_t parameterIndex                                                       *
 *                char *name                                                                   *
 *                                                                                             *
 * C Arguments: void *sddsfile, int32_t mode, ...                                              *
 *              mode=SDDSFILE_GETPARAMETER_WITH_INDEX, int32_t  parameterIndex                 *
 *              mode=SDDSFILE_GETPARAMETER_WITH_NAME, char *name                               *
 *                                                                                             *
 * Results:  char** values of the parameter on success                                         *
 *             You should free this pointer to avoid a memory leak                             *
 *           NULL on failure                                                                   *
 ***********************************************************************************************/
char** SDDSFile::getParameterInStrings(int32_t parameterIndex) {
  char** result;
  int32_t errorcode;
  char s[SDDS_MAXLINE];
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter index %"PRId32" does not exist (SDDSFile::getParameterInStrings)", parameterIndex);
    setError(s);
    return((char**)NULL);
  }
  result = parameterObject[parameterIndex]->getValuesInString(&errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInStrings)", getParameterName(parameterIndex));
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInStrings)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInStrings)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInStrings)");
    }
    return((char**)NULL);
  }
  return((char**)result);
}

char** SDDSFile::getParameterInStrings(char* name) {
  char** result;
  int32_t errorcode;
  int32_t parameterIndex;
  char s[SDDS_MAXLINE];
  parameterIndex = getParameterIndex(name);
  if ((parameterIndex < 0) || (parameterIndex >= parameterCount)) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::getParameterInStrings)", name);
    setError(s);
    return((char**)NULL);
  }
  result = parameterObject[parameterIndex]->getValuesInString(&errorcode);
  if (result == NULL) {
    sprintf(s, "Warning: unable to get parameter value for %s (SDDSFile::getParameterInStrings)", name);
    setError(s);   
    if (errorcode == 1) {
      setError((char*)"Warning: parameter value undefined (SDDSFile::getParameterInStrings)");
    } else if (errorcode == 2) {
      setError((char*)"Warning: unable to convert value to proper type (SDDSFile::getParameterInStrings)");
    } else if (errorcode == 3) {
      setError((char*)"Warning: invalid parameter type (SDDSFile::getParameterInStrings)");
    }
    return((char**)NULL);
  }
  return((char**)result);
}

char** SDDSFile_getParameterInStrings(void *sddsfile, int32_t mode, ...) {
  va_list ap;
  int32_t index;
  char *name;
  
  va_start(ap, mode);
  if (mode&SDDSFILE_GETPARAMETER_WITH_INDEX) {
    index = va_arg(ap, int32_t);
    return(((SDDSFile*)sddsfile)->getParameterInStrings(index));
  } else {
    name = va_arg(ap, char *);
    return(((SDDSFile*)sddsfile)->getParameterInStrings(name));
  }
}

/***********************************************************************************************
 *                        setError                                                             *
 *                                                                                             *
 * C++ Arguments: char *error_text                                                             *
 *                                                                                             *
 * C Arguments: void *sddsfile, char *error_text                                               *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::setError(char *error_text) {
  if (error_text != NULL) {
    n_errors++;
    error_description = (char**)SDDS_realloc(error_description, sizeof(char*) * n_errors);
    if (error_text == NULL)
      error_description[n_errors - 1] = NULL;
    else {
      error_description[n_errors - 1] = (char*)SDDS_malloc(sizeof(char) * strlen(error_text) + 1);
      strcpy(error_description[n_errors - 1], error_text);
    }
  }
}
void SDDSFile_setError(void *sddsfile, char *error_text) {
  ((SDDSFile*)sddsfile)->setError(error_text);
}

/***********************************************************************************************
 *                        printErrors                                                          *
 *                                                                                             *
 * C++ Arguments: FILE *fp, int32_t mode                                                       *
 *                none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile, FILE *fp, int32_t mode                                         *
 *                                                                                             *
 * Valid mode values: SDDS_EXIT_PrintErrors, SDDS_VERBOSE_PrintErrors                          *
 * Please try to avoid writing code that will exit in a subroutine such as this.               *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::printErrors(FILE *fpointer, int32_t mode) {
  int32_t i;
  char *registeredProgramName;
  if (n_errors == 0) {
    return;
  }
  if (fpointer == NULL) {
    n_errors = 0;
    return;
  }
  registeredProgramName = SDDS_getProgramName();
  if (registeredProgramName) {
    fprintf(fpointer, "Error for %s:\n", registeredProgramName);
  } else {
    fputs("Error:\n", fpointer);
  }
  if (error_description) {
    for (i=0; i<n_errors; i++) {
      if (error_description[i] != NULL) {
	fprintf(fpointer, "%s\n", error_description[i]);
      } else {
        fprintf(stderr, "warning: internal error: error_description[%" PRId32 "] pointer is unexpectedly NULL\n", i);
      }
    }
  } else {
    fprintf(stderr, "warning: internal error: error_description pointer is unexpectedly NULL\n");
  }
  fflush(fpointer);
  clearErrors();
  if (mode&SDDS_EXIT_PrintErrors) {
    exit(1);
  }
}
void SDDSFile::printErrors() {
  int32_t i;
  char *registeredProgramName;
  if (n_errors == 0) {
    return;
  }
  registeredProgramName = SDDS_getProgramName();
  if (registeredProgramName) {
    fprintf(stderr, "Error for %s:\n", registeredProgramName);
  } else {
    fputs("Error:\n", stderr);
  }
  if (error_description) {
    for (i=0; i<n_errors; i++) {
      if (error_description[i] != NULL) {
	fprintf(stderr, "%s\n", error_description[i]);
      }
    }
  } else {
    fprintf(stderr, "warning: internal error: error_description pointer is unexpectedly NULL\n");
  }
  clearErrors();
}
void SDDSFile_printErrors(void *sddsfile) {
  ((SDDSFile*)sddsfile)->printErrors();
}

/***********************************************************************************************
 *                        checkForErrors                                                       *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  number or errors                                                                  *
 ***********************************************************************************************/
int32_t SDDSFile::checkForErrors() {
  return(n_errors);
}
int32_t SDDSFile_checkForErrors(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->checkForErrors());
}

/***********************************************************************************************
 *                        clearErrors                                                          *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results:  none                                                                              *
 ***********************************************************************************************/
void SDDSFile::clearErrors() {
  int32_t i;
  if (n_errors == 0)
    return;
  if (error_description) {
    for (i=0; i<n_errors; i++) {
      if (error_description[i] != NULL) {
	SDDS_free(error_description[i]);
      }
    }
    SDDS_free(error_description);
    error_description = NULL;
  }
  n_errors = 0;
}
void SDDSFile_clearErrors(void *sddsfile) {
  ((SDDSFile*)sddsfile)->clearErrors();
}

/***********************************************************************************************
 *                        readFile                                                             *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::readFile() {
  if (openInputFile()) {
    return(1);
  }
  if (readLayout()) {
    return(1);
  }
  if (readPages()) {
    return(1);
  }
  if (closeFile()) {
    return(1);
  }
  return(0);
}
int32_t SDDSFile_readFile(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->readFile());
}

/***********************************************************************************************
 *                       initializeInput                                                       *
 *                                                                                             *
 * C++ Arguments: char *filename                                                               *
 *                                                                                             *
 * C Arguments: void *sddsfile, char *filename                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::initializeInput(char *filename) {
  setFileName(filename);
  if (openInputFile()) {
    return(1);
  }
  if (readLayout()) {
    return(1);
  }
  return(0);
}
int32_t SDDSFile_initializeInput(void *sddsfile, char *filename) {
  return(((SDDSFile*)sddsfile)->initializeInput(filename));
}

/***********************************************************************************************
 *                        initializeOutput                                                     *
 *                                                                                             *
 * C++ Arguments: char *filename                                                               *
 *                                                                                             *
 * C Arguments: void *sddsfile, char *filename                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::initializeOutput(int32_t data_mode, int32_t lines_per_row, 
                                   char *description, char *contents, char *filename) {
  setFileName(filename);
  if (data_mode == SDDS_BINARY) {
    setBinaryMode();
  } else {
    setAsciiMode();
  }
  linesPerRow = lines_per_row;
  setDescription(description, contents);
  if (openOutputFile()) {
    return(1);
  }
  return(0);
}
int32_t SDDSFile_initializeOutput(void *sddsfile, int32_t data_mode, int32_t lines_per_row, 
                                  char *description, char *contents, char *filename) {
  return(((SDDSFile*)sddsfile)->initializeOutput(data_mode, lines_per_row, description, contents, filename));
}

/***********************************************************************************************
 *                        writeFile                                                            *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writeFile() {
  if (openOutputFile()) {
    return(1);
  }
  if (writeLayout()) {
    return(1);
  }
  if (writePages()) {
    return(1);
  }
  if (closeFile()) {
    return(1);
  }
  return(0);
}
int32_t SDDSFile_writeFile(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->writeFile());
}

/***********************************************************************************************
 *                        openInputFile                                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::openInputFile() {
  char s[SDDS_MAXLINE];
#if defined(zLib)
  char *extension;
#endif
  if (fileName == NULL) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
      setError((char*)"unable to set stdin to binary mode");
      return(1);
    }      
#endif
    fp = stdin;
  } else {
    if (!(fp=fopen(fileName, "rb"))) {
      sprintf(s, "Unable to open file %s for reading (SDDSFile::openInputFile)", fileName);
      setError(s);
      return(1);
    }
    //setvbuf(fp, NULL, _IOFBF, BUFSIZ);
#if defined(zLib)
    if ((extension = strrchr(fileName, '.')) && strcmp(extension, ".gz")==0) {
      gzipFile = true;
      gzfp=(void**)gzdopen(fileno(fp), "rb");
      if (gzfp == NULL) {
	sprintf(s, "Unable to open compressed file %s for reading (SDDSFile::openInputFile)", fileName);
	setError(s);
	return(1);
      }
    }
#endif
  }
  return(0);
}
int32_t SDDSFile_openInputFile(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->openInputFile());
}

/***********************************************************************************************
 *                        openOutputFile                                                       *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::openOutputFile() {
  char s[SDDS_MAXLINE];
#if defined(zLib)
  char *extension;
#endif
  if (fileName == NULL) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
      setError((char*)"unable to set stdout to binary mode");
      return(1);
    }      
#endif
    fp = stdout;
  } else {
    if (SDDS_fileIsLocked(fileName)) {
      sprintf(s, "unable to open file %s for writing--file is locked (SDDSFile::openOutputFile)", fileName);
      setError(s);
      return(1);
    }
    if (!(fp=fopen(fileName, "wb"))) {
      sprintf(s, "Unable to open file %s for writing (SDDSFile::openOutputFile)", fileName);
      setError(s);
      return(1);
    }
    if (!SDDS_lockFile(fp, fileName, (char*)"SDDSFile::openOutputFile"))
      return(1);
#if defined(zLib)
    if ((extension = strrchr(fileName, '.')) && strcmp(extension, ".gz")==0) {
      gzipFile = true;
      gzfp=(void**)gzdopen(fileno(fp), "wb");
      if (gzfp == NULL) {
	sprintf(s, "Unable to open compressed file %s for writing (SDDSFile::openOutputFile)", fileName);
	setError(s);
	return(1);
      }
    }
#endif
  }
  return(0);
}
int32_t SDDSFile_openOutputFile(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->openOutputFile());
}

/***********************************************************************************************
 *                        readLayout                                                           *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::readLayout() {
  char s[SDDS_MAXLINE];
  char *groupName, *ptr;
  int32_t i, result;
  uint32_t commentFlags;
#if defined(zLib)
  if (gzipFile) {
    if (gzfp==NULL) {
      setError((char*)"Can't read SDDS layout--file pointer is NULL (SDDSFile::readLayout)");
      return(1);
    }
  } else {
#endif
    if (fp==NULL) {
      setError((char*)"Can't read SDDS layout--file pointer is NULL (SDDSFile::readLayout)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  if (readVersion()) {
    setError((char*)"Can't read SDDS layout--error writing version (SDDSFile::readLayout)");
    return(1);
  }
  if ((layoutVersion < 1) || (layoutVersion > SDDS_VERSION)) {
    sprintf(s, "Unable to read SDDS version %"PRId32" files", layoutVersion);
    setError(s);
    return(1);
  }
  SDDS_resetSpecialCommentsModes();
#if defined(zLib)
  if (gzipFile) {
    result = SDDS_getNamelist(s, SDDS_MAXLINE, gzfp);
  } else {
#endif
    result = SDDS_getNamelist(s, SDDS_MAXLINE, fp);
#if defined(zLib)
  }
#endif
  while (result) {
    groupName = s+1;
    if ((ptr=strpbrk(s, " \t")) == NULL) {
      setError((char*)"Unable to read layout---no groupname in namelist (SDDSFile::readLayout)");
      return(1);
    }
    *ptr = 0;
    if (strcmp(groupName, (char*)"description") == 0) {
      if (processDescription(ptr+1)) {
        setError((char*)"Unable to process description (SDDSFile::readLayout)");
        return(1);
      }
    } else if (strcmp(groupName, (char*)"array") == 0) {
      if (processArray(ptr+1)) {
        setError((char*)"Unable to process array (SDDSFile::readLayout)");
        return(1);
      }
    } else if (strcmp(groupName, (char*)"column") == 0) {
      if (processColumn(ptr+1)) {
        setError((char*)"Unable to process column (SDDSFile::readLayout)");
        return(1);
      }
    } else if (strcmp(groupName, (char*)"parameter") == 0) {
      if (processParameter(ptr+1)) {
        setError((char*)"Unable to process parameter (SDDSFile::readLayout)");
        return(1);
      }
    } else if (strcmp(groupName, (char*)"data") == 0) {
      if (processData(ptr+1)) {
        setError((char*)"Unable to process parameter (SDDSFile::readLayout)");
        return(1);
      }
      commentFlags = SDDS_getSpecialCommentsModes();
      if ((commentFlags&SDDS_BIGENDIAN_SEEN) && (commentFlags&SDDS_LITTLEENDIAN_SEEN)) {
        setError((char*)"Unable to read data as it says it is both big and little endian (SDDSFile::readLayout)");
        return(1);
      }
      if (commentFlags&SDDS_BIGENDIAN_SEEN) {
        bigEndian = true;
      }
      if (commentFlags&SDDS_LITTLEENDIAN_SEEN) {
        bigEndian = false;
      }
#if defined(zLib)
      if (gzipFile) {
        for (i=0; i<additionalHeaderLines; i++) {
          if (SDDS_fgetsSkipComments(s, SDDS_MAXLINE, gzfp, '!') == NULL) {
            setError((char*)"Unexpected end-of-file while skipping past additional header lines (SDDSFile::readLayout)");
            return(1);
          }
        }
      } else {
#endif
        for (i=0; i<additionalHeaderLines; i++) {
          if (SDDS_fgetsSkipComments(s, SDDS_MAXLINE, fp, '!') == NULL) {
            setError((char*)"Unexpected end-of-file while skipping past additional header lines (SDDSFile::readLayout)");
            return(1);
          }
        }
#if defined(zLib)
      }
#endif
      return(0);
    } else if (strcmp(groupName, (char*)"associate") == 0) {
      fprintf(stderr, "ASSOCIATE\n");
    } else if (strcmp(groupName, (char*)"include") == 0) {
      fprintf(stderr, "INCLUDE\n");
    } else {
      sprintf(s, "Unknown layout entry %s given (SDDSFile::readLayout)", groupName);
      setError(s); 
      return(1);
    }
#if defined(zLib)
    if (gzipFile) {
      result = SDDS_getNamelist(s, SDDS_MAXLINE, gzfp);
    } else {
#endif
      result = SDDS_getNamelist(s, SDDS_MAXLINE, fp);
#if defined(zLib)
    }
#endif
  }
  sprintf(s, "Missing data layout entry (SDDSFile::readLayout)");
  setError(s); 
  return(1);
}
int32_t SDDSFile_readLayout(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->readLayout());
}

/***********************************************************************************************
 *                        writeLayout                                                          *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writeLayout() {
  char *outputEndianess = NULL;
  if ((outputEndianess = getenv("SDDS_OUTPUT_ENDIANESS"))) {
    if (strncmp(outputEndianess, "big", 3) == 0) {
      bigEndian = true;
    } else if (strncmp(outputEndianess, "little", 6) == 0) {
      bigEndian = false;
    }
  }
#if defined(zLib)
  if (gzipFile) {
    if (gzfp==NULL) {
      setError((char*)"Can't write SDDS layout--file pointer is NULL (SDDSFile::writeLayout)");
      return(1);
    }
  } else {
#endif
    if (fp==NULL) {
      setError((char*)"Can't write SDDS layout--file pointer is NULL (SDDSFile::writeLayout)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  if (writeVersion(layoutVersion)) {
    setError((char*)"Can't write SDDS layout--error writing version (SDDSFile::writeLayout)");
    return(1);
  }
  if (writeDescription()) {
    setError((char*)"Can't write SDDS layout--error writing description (SDDSFile::writeLayout)");
    return(1);
  }
  if (writeParameterDefinitions()) {
    setError((char*)"Unable to write layout--error writing parameter definitions (SDDSFile::writeLayout)");
    return(1);
  }
  if (writeArrayDefinitions()) {
    setError((char*)"Unable to write layout--error writing array definitions (SDDSFile::writeLayout)");
    return(1);
  }
  if (writeColumnDefinitions()) {
    setError((char*)"Unable to write layout--error writing column definitions (SDDSFile::writeLayout)");
    return(1);
  }
  if (writeDataMode()) {
    setError((char*)"Unable to write layout--error writing data mode (SDDSFile::writeLayout)");
    return(1);
  }
#if defined(zLib)
  if (!gzipFile) {
#endif
    fflush(fp);
#if defined(zLib)
  }
#endif
  return(0);
}
int32_t SDDSFile_writeLayout(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->writeLayout());
}

/***********************************************************************************************
 *                        readVersion                                                          *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::readVersion() {
  char s[SDDS_MAXLINE];
#if defined(zLib)
  if (gzipFile) {
    if (gzfp==NULL) {
      return(1);
    }
    if (!gzgets(gzfp, s, SDDS_MAXLINE)) {
      setError((char*)"Unable to read layout--no header lines found (SDDSFile::readVersion)");
      return(1);
    }
  } else {
#endif
    if (fp==NULL) {
      return(1);
    }
    if (!fgets(s, SDDS_MAXLINE, fp)) {
      setError((char*)"Unable to read layout--no header lines found (SDDSFile::readVersion)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  if (strncmp(s, "SDDS", 4)!=0) {
    setError((char*)"Unable to read layout--no header lines found (SDDSFile::readVersion)");
    return(1);
  }
  if (sscanf(s+4, "%" SCNd32, &layoutVersion)!=1) {
    setError((char*)"Unable to read layout--no version number on first line (SDDSFile::readVersion)");
    return(1);
  }
  return(0);
}
/***********************************************************************************************
 *                        writeVersion                                                         *
 *                                                                                             *
 * C++ Arguments: int32_t version_number                                                       *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writeVersion(int32_t version_number) {
#if defined(zLib)
  if (gzipFile) {
    if (gzfp==NULL) {
      return(1);
    }
    gzprintf(gzfp, "SDDS%" PRId32 "\n", version_number);
  } else {
#endif
    if (fp==NULL) {
      return(1);
    }
    fprintf(fp, "SDDS%" PRId32 "\n", version_number);
#if defined(zLib)
  }
#endif
  return(0);
}

/***********************************************************************************************
 *                        writeDescription                                                     *
 *                                                                                             *
 * C++ Arguments: int32_t version_number                                                       *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writeDescription() {
  if ((descriptionText == NULL) && (descriptionContents == NULL)) {
    return(0);
  }
#if defined(zLib)
  if (gzipFile) {
    if (gzfp == NULL) {
      return(1);
    }
    gzputs(gzfp, "&description ");
    if (descriptionText != NULL) {
      if (SDDS_printNamelistField(gzfp, (char*)"text", descriptionText)) {
	return(1);
      }
    }
    if (descriptionContents != NULL) {
      if (SDDS_printNamelistField(gzfp, (char*)"contents", descriptionContents)) {
	return(1);
      }
    }
    gzputs(gzfp, "&end\n");
  } else {
#endif
    if (fp == NULL) {
      return(1);
    }
    fputs("&description ", fp);
    if (descriptionText != NULL) {
      if (SDDS_printNamelistField(fp, (char*)"text", descriptionText)) {
	return(1);
      }
    }
    if (descriptionContents != NULL) {
      if (SDDS_printNamelistField(fp, (char*)"contents", descriptionContents)) {
	return(1);
      }
    }
    fputs("&end\n", fp);
#if defined(zLib)
  }
#endif
  return(0);
}

/*
 * writeArrayDefinitions
 *
 * C++ Arguments: none
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::writeArrayDefinitions() {
  int32_t i;
  for (i=0; i<arrayCount; i++) {
    if (writeArrayDefinition(i)) {
      setError((char*)"Error writing array definitions (SDDSFile::writeArrayDefinitions)");
      return(1);
    }
  }
  return(0);
}

/*
 * writeArrayDefinition
 *
 * C++ Arguments: int32_t arrayIndex
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::writeArrayDefinition(int32_t arrayIndex) {
#if defined(zLib)
  if (gzipFile) {
    if (arrayObject[arrayIndex]->writeDefinition(gzfp)) {
      setError((char*)"Error writing array definition (SDDSFile::writeArrayDefinition)");
      return(1);
    }
  } else {
#endif
    if (arrayObject[arrayIndex]->writeDefinition(fp)) {
      setError((char*)"Error writing array definition (SDDSFile::writeArrayDefinition)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  return(0);
}

/*
 * writeColumnDefinitions
 *
 * C++ Arguments: none
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::writeColumnDefinitions() {
  int32_t i;
  for (i=0; i<columnCount; i++) {
    if (writeColumnDefinition(i)) {
      setError((char*)"Error writing column definitions (SDDSFile::writeColumnDefinitions)");
      return(1);
    }
  }
  return(0);
}

/*
 * writeColumnDefinition
 *
 * C++ Arguments: int32_t columnIndex
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::writeColumnDefinition(int32_t columnIndex) {
#if defined(zLib)
  if (gzipFile) {
    if (columnObject[columnIndex]->writeDefinition(gzfp)) {
      setError((char*)"Error writing column definition (SDDSFile::writeColumnDefinition)");
      return(1);
    }
  } else {
#endif
    if (columnObject[columnIndex]->writeDefinition(fp)) {
      setError((char*)"Error writing column definition (SDDSFile::writeColumnDefinition)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  return(0);
}

/***********************************************************************************************
 *                        writeParameterDefinitions                                            *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writeParameterDefinitions() {
  int32_t i;
  for (i=0; i<parameterCount; i++) {
    if (writeParameterDefinition(i)) {
      setError((char*)"Error writing parameter definitions (SDDSFile::writeParameterDefinitions)");
      return(1);
    }
  }
  return(0);
}

/***********************************************************************************************
 *                        writeParameterDefinition                                             *
 *                                                                                             *
 * C++ Arguments: int32_t parameterIndex                                                       *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writeParameterDefinition(int32_t parameterIndex) {
#if defined(zLib)
  if (gzipFile) {
    if (parameterObject[parameterIndex]->writeDefinition(gzfp)) {
      setError((char*)"Error writing parameter definition (SDDSFile::writeParameterDefinition)");
      return(1);
    }
  } else {
#endif
    if (parameterObject[parameterIndex]->writeDefinition(fp)) {
      setError((char*)"Error writing parameter definition (SDDSFile::writeParameterDefinition)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  return(0);
}

/***********************************************************************************************
 *                        writeDataMode                                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writeDataMode() {
#if defined(zLib)
  if (gzipFile) {
    if (gzfp == NULL) {
      return(1);
    }
    gzputs(gzfp, "&data ");
    if (asciiFile) {
      if (SDDS_printNamelistField(gzfp, (char*)"mode", (char*)"ascii")) {
	return(1);
      }
      if (noRowCount) {
        if (layoutVersion >= 3) {
          if (SDDS_printNamelistField(gzfp, (char*)"no_row_counts", (char*)"true")) {
            return(1);
          }
        } else {
          if (SDDS_printNamelistField(gzfp, (char*)"no_row_counts", (char*)"1")) {
            return(1);
          }
        }
      }
    } else {
      if (SDDS_printNamelistField(gzfp, (char*)"mode", (char*)"binary")) {
	return(1);
      }
      if (layoutVersion >= 3) {
        if (bigEndian) {
          if (SDDS_printNamelistField(gzfp, (char*)"endian", (char*)"big")) {
            return(1);
          }
        } else {
          if (SDDS_printNamelistField(gzfp, (char*)"endian", (char*)"little")) {
            return(1);
          }
        }
      }
      if (columnMajorOrder && (layoutVersion >= 3)) {
        if (SDDS_printNamelistField(gzfp, (char*)"column_major_order", (char*)"true")) {
          return(1);
        }
      }
    }

    gzputs(gzfp, "&end\n");
  } else {
#endif
    if (fp == NULL) {
      return(1);
    }
    fputs("&data ", fp);
    if (asciiFile) {
      if (SDDS_printNamelistField(fp, (char*)"mode", (char*)"ascii")) {
	return(1);
      }
      if (noRowCount) {
        if (layoutVersion >= 3) {
          if (SDDS_printNamelistField(fp, (char*)"no_row_counts", (char*)"true")) {
            return(1);
          }
        } else {
          if (SDDS_printNamelistField(fp, (char*)"no_row_counts", (char*)"1")) {
            return(1);
          }
        }
      }
    } else {
      if (SDDS_printNamelistField(fp, (char*)"mode", (char*)"binary")) {
	return(1);
      }
      if (layoutVersion >= 3) {
        if (bigEndian) {
          if (SDDS_printNamelistField(fp, (char*)"endian", (char*)"big")) {
            return(1);
          }
        } else {
          if (SDDS_printNamelistField(fp, (char*)"endian", (char*)"little")) {
            return(1);
          }
        }
      }
      if (columnMajorOrder && (layoutVersion >= 3)) {
        if (SDDS_printNamelistField(fp, (char*)"column_major_order", (char*)"true")) {
          return(1);
        }
      }
    }
    fputs("&end\n", fp);
#if defined(zLib)
  }
#endif
  return(0);
}

/***********************************************************************************************
 *                        writePages                                                           *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                uint32_t startPage, uint32_t endPage                                           *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writePages() {
  return(writePages(1, pageCount()));
}
int32_t SDDSFile::writePages(uint32_t startPage, uint32_t endPage) {
  uint32_t i;
  for (i = startPage; i <= endPage ; i++) {
    if (writePage(i)) {
      return(1);
    }
  }
  return(0);
}
int32_t SDDSFile_writePages(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->writePages());
}

/***********************************************************************************************
 *                        writePage                                                            *
 *                                                                                             *
 * C++ Arguments: uint32_t page                                                                 *
 *                                                                                             *
 * C Arguments: void *sddsfile, uint32_t page                                                   *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writePage(uint32_t page) {
  if (asciiFile) {  
    return(writeAsciiPage(page));
  } else {
    return(writeBinaryPage(page));
  }
}
int32_t SDDSFile_writePage(void *sddsfile, uint32_t page) {
  return(((SDDSFile*)sddsfile)->writePage(page));
}

/***********************************************************************************************
 *                        readPages                                                            *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::readPages() {
  if (asciiFile) {  
    return(readAsciiPages());
  } else {
    return(readBinaryPages());
  }
}
int32_t SDDSFile_readPages(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->readPages());
}

/***********************************************************************************************
 *                        readPage                                                             *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::readPage() {
  freePage();
  if (asciiFile) {  
    return(readAsciiPage());
  } else {
    return(readBinaryPage());
  }
}
int32_t SDDSFile_readPage(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->readPage());
}

/***********************************************************************************************
 *                        freePage                                                             *
 *                                                                                             *
 * This is called by readPage because it is assumed that the program only wants to have        *
 * one page in memory at a time. If this is not the case then the program should call          *
 * readPages instead.                                                                          *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: none                                                                               *
 ***********************************************************************************************/
void SDDSFile::freePage() {
  int32_t i;
  for (i=0 ; i<arrayCount ; i++) {
    arrayObject[i]->freePage();
  }
  for (i=0 ; i<parameterCount ; i++) {
    parameterObject[i]->freePage();
  }
  for (i=0 ; i<columnCount ; i++) {
    columnObject[i]->freePage();
  }
}
void SDDSFile_freePage(void *sddsfile) {
  ((SDDSFile*)sddsfile)->freePage();
}

/***********************************************************************************************
 *                        writeAsciiPage                                                       *
 *                                                                                             *
 * C++ Arguments: uint32_t page                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writeAsciiPage(uint32_t page) {
  char s[SDDS_MAXLINE];
  int32_t i;
  uint32_t j, rows;
  rows = rowCount(page);
#if defined(zLib)
  if (gzipFile) {
    if (gzfp == NULL) {
      return(1);
    }
    if ((noRowCount) && (page > 1) && (columnCount > 0)) {
      gzputc(gzfp, '\n');
    }
    gzprintf(gzfp, "! page number %" PRIu32 "\n", page);
    for (i=0 ; i<parameterCount ; i++) {
      if (parameterObject[i]->writeAsciiValue(gzfp, page)) {
        sprintf(s, "invalid page number (%" PRId32 ") for parameter %s (SDDSFile::writeAsciiPage)", page, parameterObject[i]->getName());
        setError(s);
        return(1);
      }
    }
    for (i=0 ; i<arrayCount ; i++) {
      if (arrayObject[i]->writeAsciiValues(gzfp, page)) {
        sprintf(s, "invalid page number (%" PRId32 ") for array %s (SDDSFile::writeAsciiPage)", page, arrayObject[i]->getName());
        setError(s);
        return(1);
      }
    }
    if ((!noRowCount) && (columnCount > 0)) {
      gzprintf(gzfp, "\t%" PRIu32 "\n", rows);
    }
    if (rows > 0) {
      for (j=1; j<=rows; j++) {
        for (i=0 ; i<columnCount ; i++) {
          if (columnObject[i]->writeAsciiValue(gzfp, page, j)) {
            sprintf(s, "invalid page or row number for column %s (SDDSFile::writeAsciiPage)", columnObject[i]->getName());
            setError(s);
            return(1);
          }
        }
        gzputc(gzfp, '\n');
      }
    }
  } else {
#endif
    if (fp == NULL) {
      return(1);
    }
    if ((noRowCount) && (page > 1) && (columnCount > 0)) {
      fputc('\n', fp);
    }
    fprintf(fp, "! page number %" PRIu32 "\n", page);
    for (i=0 ; i<parameterCount ; i++) {
      if (parameterObject[i]->writeAsciiValue(fp, page)) {
        sprintf(s, "invalid page number (%" PRId32 ") for parameter %s (SDDSFile::writeAsciiPage)", page, parameterObject[i]->getName());
        setError(s);
        return(1);
      }
    }
    for (i=0 ; i<arrayCount ; i++) {
      if (arrayObject[i]->writeAsciiValues(fp, page)) {
        sprintf(s, "invalid page number (%" PRId32 ") for array %s (SDDSFile::writeAsciiPage)", page, arrayObject[i]->getName());
        setError(s);
        return(1);
      }
    }
    if ((!noRowCount) && (columnCount > 0)) {
      fprintf(fp, "\t%" PRIu32 "\n", rows);
    }
    if (rows > 0) {
      for (j=1; j<=rows; j++) {
        for (i=0 ; i<columnCount ; i++) {
          if (columnObject[i]->writeAsciiValue(fp, page, j)) {
            sprintf(s, "invalid page or row number for column %s (SDDSFile::writeAsciiPage)", columnObject[i]->getName());
            setError(s);
            return(1);
          }
        }
        fputc('\n', fp);
      }
    }
    fflush(fp);
#if defined(zLib)
  }
#endif
  return(0);
}

/***********************************************************************************************
 *                        writeBinaryPage                                                      *
 *                                                                                             *
 * C++ Arguments: uint32_t page                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::writeBinaryPage(uint32_t page) {
  SDDS_FILEBUFFER fBuffer;
  int32_t i;
  uint32_t rows, j;
  bool big, nonNativeEndian;
  big = SDDS_isBigEndianMachine();
  if (big == true) {
    if (bigEndian == true) {
      nonNativeEndian = false;
    } else {
      nonNativeEndian = true;
    }
  } else {
    if (bigEndian == true) {
      nonNativeEndian = true;
    } else {
      nonNativeEndian = false;
    }
  }
  if ((fBuffer.buffer = fBuffer.data = (char*)SDDS_malloc(sizeof(char)*(defaultIOBufferSize+1))) == NULL) {
    setError((char*)"Unable to do buffered write--allocation error (SDDSFile::writeBinaryPage)");
    return(1);
  }
  fBuffer.bufferSize = defaultIOBufferSize;
  fBuffer.bytesLeft = defaultIOBufferSize;

  rows = rowCount(page);
#if defined(zLib)
  if (gzipFile) {
    if (gzfp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::writeBinaryPages)");
      return(1);
    }
    if (SDDS_bufferedWrite(&rows, sizeof(rows), gzfp, &fBuffer, this)) {
      setError((char*)"Unable to write page--failure writing number of rows (SDDSFile::writeBinaryPage)");
      return(1);
    }
    for (i=0 ; i<parameterCount ; i++) {
      if (parameterObject[i]->writeBinaryValue(gzfp, &fBuffer, page, nonNativeEndian)) {
        setError((char*)"Unable to write page--failure writing parameter (SDDSFile::writeBinaryPage)");
        return(1);
      }
    }
    for (i=0 ; i<arrayCount ; i++) {
      if (arrayObject[i]->writeBinaryValues(gzfp, &fBuffer, page, nonNativeEndian)) {
        setError((char*)"Unable to write page--failure writing array (SDDSFile::writeBinaryPage)");
        return(1);
      }
    }
    if (columnCount > 0) {
      page--;
      if (columnMajorOrder && (layoutVersion >= 3)) {
        if (rows > 0) {
          for (i=0 ; i<columnCount ; i++) {
            if (columnObject[i]->writeBinaryValues(gzfp, &fBuffer, page, nonNativeEndian)) {
              setError((char*)"Unable to write page--failure writing column (SDDSFile::writeBinaryPage)");
              return(1);
            }
          }
        }
      } else {
        for (j=0; j<rows; j++) {
          for (i=0 ; i<columnCount ; i++) {
            if (columnObject[i]->writeBinaryValue(gzfp, &fBuffer, page, j, nonNativeEndian)) {
              setError((char*)"Unable to write page--failure writing column (SDDSFile::writeBinaryPage)");
              return(1);
            }
          }
        }
      }
      page++;
    }
    if (SDDS_flushBuffer(gzfp, &fBuffer, this)) {
      setError((char*)"Unable to write page--buffer flushing problem (SDDSFile::writeBinaryPage)");
      return(1);
    }
  } else {
#endif
    if (fp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::writeBinaryPages)");
      return(1);
    }
    if (SDDS_bufferedWrite(&rows, sizeof(rows), fp, &fBuffer, this)) {
      setError((char*)"Unable to write page--failure writing number of rows (SDDSFile::writeBinaryPage)");
      return(1);
    }
    for (i=0 ; i<parameterCount ; i++) {
      if (parameterObject[i]->writeBinaryValue(fp, &fBuffer, page, nonNativeEndian)) {
        setError((char*)"Unable to write page--failure writing parameter (SDDSFile::writeBinaryPage)");
        return(1);
      }
    }
    for (i=0 ; i<arrayCount ; i++) {
      if (arrayObject[i]->writeBinaryValues(fp, &fBuffer, page, nonNativeEndian)) {
        setError((char*)"Unable to write page--failure writing array (SDDSFile::writeBinaryPage)");
        return(1);
      }
    }
    if (columnCount > 0) {
      page--;
      if (columnMajorOrder && (layoutVersion >= 3)) {
        if (rows > 0) {
          for (i=0 ; i<columnCount ; i++) {
            if (columnObject[i]->writeBinaryValues(fp, &fBuffer, page, nonNativeEndian)) {
              setError((char*)"Unable to write page--failure writing column (SDDSFile::writeBinaryPage)");
              return(1);
            }
          }
        }
      } else {
        for (j=0; j<rows; j++) {
          for (i=0 ; i<columnCount ; i++) {
            if (columnObject[i]->writeBinaryValue(fp, &fBuffer, page, j, nonNativeEndian)) {
              setError((char*)"Unable to write page--failure writing column (SDDSFile::writeBinaryPage)");
              return(1);
            }
          }
        }
      }
      page++;
    }
    if (SDDS_flushBuffer(fp, &fBuffer, this)) {
      setError((char*)"Unable to write page--buffer flushing problem (SDDSFile::writeBinaryPage)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  SDDS_free(fBuffer.buffer);
  return(0);
}




























































/***********************************************************************************************
 *                        readAsciiPages                                                       *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::readAsciiPages() {
  int32_t i;
  uint32_t page=1;
  bool asciiDataExpected=false, first_read;
  static char *bigBuffer=NULL;
  static int32_t bigBufferSize=0;
  char *bigBufferCopy;
  int32_t bigBufferCopySize;
  int32_t lines, columnIndex;
  uint32_t n_rows, j;
#if defined(zLib)
  if (gzipFile) {
    if (gzfp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::readAsciiPages)");
      return(1);
    }
  } else {
#endif
    if (fp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::readAsciiPages)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  setReadRecoveryMode(0);
  for (i=0; i<parameterCount; i++) {
    if (getParameterFixedValue(i) == NULL) {
      asciiDataExpected = true;
      break;
    }
  }
  if ((columnCount > 0) || (arrayCount > 0)) {
    asciiDataExpected = true;
  }
  if (!asciiDataExpected) {
    return(0);
  }
  if (bigBufferSize == 0) {
    bigBufferSize = SDDS_MAXLINE;
    bigBuffer = (char*)SDDS_malloc(sizeof(char) * bigBufferSize);
  }
  while (true) {
    first_read=true;
#if defined(zLib)
    if (gzipFile) {
      if (gzeof(gzfp)) {
        if (page==1) {
          setError((char*)"No data in file (SDDSFile::readAsciiPages)");
          return(1);
        } else {
          return(0);
        }
      }
    } else {
#endif
      if (feof(fp)) {
        if (page==1) {
          setError((char*)"No data in file (SDDSFile::readAsciiPages)");
          return(1);
        } else {
          return(0);
        }
      }
#if defined(zLib)
    }
#endif
    for (i=0; i<parameterCount; i++) {
      bigBuffer[0] = 0;
      if (getParameterFixedValue(i) == NULL) {
#if defined(zLib)
        if (gzipFile) {
          if (SDDS_fgetsSkipCommentsResize(&bigBuffer, &bigBufferSize, gzfp, '!') == NULL) {
            if (first_read) {
              return(0);
            }
            setError((char*)"Unable to read parameter values (SDDSFile::readAsciiPages)");
            return(1);
          }
        } else {
#endif
          if (SDDS_fgetsSkipCommentsResize(&bigBuffer, &bigBufferSize, fp, '!') == NULL) {
            if (first_read) {
              return(0);
            }
            setError((char*)"Unable to read parameter values (SDDSFile::readAsciiPages)");
            return(1);
          }
#if defined(zLib)
        }
#endif
        first_read = false;
        bigBufferCopy = bigBuffer;
        bigBufferCopySize = strlen(bigBufferCopy);
        if (parameterObject[i]->readAsciiValue(&bigBufferCopy, &bigBufferCopySize, page)) {
          setError((char*)"Unable to read parameter (SDDSFile::readAsciiPages)");
          return(1);
        }
      }
    }
    for (i=0; i<arrayCount; i++) {
      /* ADD CODE HERE */

    }
    if (columnCount > 0) {
      n_rows=0;
      if (noRowCount == false) {
#if defined(zLib)
        if (gzipFile) {
          if (SDDS_fgetsSkipComments(bigBuffer, bigBufferSize, gzfp, '!') == NULL) {
            if (first_read) {
              return(0);
            }
            setError((char*)"Unable to read row count (SDDSFile::readAsciiPages)");
            return(1);
          }
        } else {
#endif
          if (SDDS_fgetsSkipComments(bigBuffer, bigBufferSize, fp, '!') == NULL) {
            if (first_read) {
              return(0);
            }
            setError((char*)"Unable to read row count (SDDSFile::readAsciiPages)");
            return(1);
          }
#if defined(zLib)
        }
#endif
        first_read = false;
        if (sscanf(bigBuffer, "%" SCNu32, &n_rows)!=1 || n_rows<0) {
          setError((char*)"Unable to read page--file has no (valid) number-of-rows entry (SDDSFile::readAsciiPages)");
          return(1);
        }
        if (n_rows == 0) {
          page++;
          continue;
        }
      }
      j=0;
      lines=linesPerRow;
      columnIndex=0;
      while ((j<n_rows) || noRowCount) {
        bigBuffer[0] = 0;
#if defined(zLib)
        if (gzipFile) {
          if (SDDS_fgetsSkipCommentsResize(&bigBuffer, &bigBufferSize, gzfp, '!') == NULL) {
            if (noRowCount) {
              return(0);
            }
            setError((char*)"Unable to read column values (SDDSFile::readAsciiPages)");
            setReadRecoveryMode(1);
            return(1);
          }
        } else {
#endif
          if (SDDS_fgetsSkipCommentsResize(&bigBuffer, &bigBufferSize, fp, '!') == NULL) {
            if (noRowCount) {
              return(0);
            }
            setError((char*)"Unable to read column values (SDDSFile::readAsciiPages)");
            setReadRecoveryMode(1);
            return(1);
          }
#if defined(zLib)
        }
#endif
        if ((noRowCount) && (SDDS_stringIsBlank(bigBuffer))) {
          break;
        }
        bigBufferCopy = bigBuffer;
        bigBufferCopySize = strlen(bigBufferCopy);
        for (i=columnIndex; i<columnCount; i++) {
          if (columnObject[i]->readAsciiValue(&bigBufferCopy, &bigBufferCopySize, page)) {
            setError((char*)"Unable to read column (SDDSFile::readAsciiPages)");
            setReadRecoveryMode(1);
            return(1);
          }
          if ((i+1 != columnCount) && (SDDS_stringIsBlank(bigBufferCopy))) {
            if (lines > 1) {
              lines--;
              columnIndex = i+1;
              j--;
              break;
            } else {
              setError((char*)"Warning: missing element in row (SDDSFile::readAsciiPages)");
              setReadRecoveryMode(1);
              return(1);
            }
          } else if (linesPerRow > 1) {
            lines=linesPerRow;
            columnIndex=0;
          }
        }
        j++;
      }
    }
    page++;
  }
  return(0);
}

/***********************************************************************************************
 *                        readAsciiPage                                                        *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: page number on success                                                             *
 *          0 on failure                                                                       *
 *          -1 on EOF                                                                          *
 ***********************************************************************************************/
uint32_t SDDSFile::readAsciiPage() {
  static uint32_t page=1;
  int32_t i;
  bool asciiDataExpected=false, first_read;
  static char *bigBuffer=NULL;
  static int32_t bigBufferSize=0;
  char *bigBufferCopy;
  int32_t bigBufferCopySize;
  int32_t lines, columnIndex;
  uint32_t n_rows, j;
#if defined(zLib)
  if (gzipFile) {
    if (gzfp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::readAsciiPage)");
      return(0);
    }
  } else {
#endif
    if (fp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::readAsciiPage)");
      return(0);
    }
#if defined(zLib)
  }
#endif
  setReadRecoveryMode(0);
  for (i=0; i<parameterCount; i++) {
    if (getParameterFixedValue(i) == NULL) {
      asciiDataExpected = true;
      break;
    }
  }
  if ((columnCount > 0) || (arrayCount > 0)) {
    asciiDataExpected = true;
  }
  if (!asciiDataExpected) {
    return(-1);
  }
  if (bigBufferSize == 0) {
    bigBufferSize = SDDS_MAXLINE;
    bigBuffer = (char*)SDDS_malloc(sizeof(char) * bigBufferSize);
  }
  /*  while (true) {*/
  first_read=true;
#if defined(zLib)
  if (gzipFile) {
    if (gzeof(gzfp)) {
      if (page==1) {
        setError((char*)"No data in file (SDDSFile::readAsciiPage)");
        return(0);
      } else {
        return(-1);
      }
    }
  } else {
#endif
    if (feof(fp)) {
      if (page==1) {
        setError((char*)"No data in file (SDDSFile::readAsciiPage)");
        return(0);
      } else {
        return(-1);
      }
    }
#if defined(zLib)
  }
#endif
  for (i=0; i<parameterCount; i++) {
    bigBuffer[0] = 0;
    if (getParameterFixedValue(i) == NULL) {
#if defined(zLib)
      if (gzipFile) {
        if (SDDS_fgetsSkipCommentsResize(&bigBuffer, &bigBufferSize, gzfp, '!') == NULL) {
          if (first_read) {
            return(-1);
          }
          setError((char*)"Unable to read parameter values (SDDSFile::readAsciiPage)");
          return(0);
        }
      } else {
#endif
        if (SDDS_fgetsSkipCommentsResize(&bigBuffer, &bigBufferSize, fp, '!') == NULL) {
          if (first_read) {
            return(-1);
          }
          setError((char*)"Unable to read parameter values (SDDSFile::readAsciiPage)");
          return(0);
        }
#if defined(zLib)
      }
#endif
      first_read = false;
      bigBufferCopy = bigBuffer;
      bigBufferCopySize = strlen(bigBufferCopy);
      if (parameterObject[i]->readAsciiValue(&bigBufferCopy, &bigBufferCopySize, 1)) {
        setError((char*)"Unable to read parameter (SDDSFile::readAsciiPage)");
        return(0);
      }
    }
  }
  for (i=0; i<arrayCount; i++) {
    /* ADD CODE HERE */

  }
  if (columnCount > 0) {
    n_rows=0;
    if (noRowCount == false) {
#if defined(zLib)
      if (gzipFile) {
        if (SDDS_fgetsSkipComments(bigBuffer, bigBufferSize, gzfp, '!') == NULL) {
          if (first_read) {
            return(-1);
          }
          setError((char*)"Unable to read row count (SDDSFile::readAsciiPage)");
          return(0);
        }
      } else {
#endif
        if (SDDS_fgetsSkipComments(bigBuffer, bigBufferSize, fp, '!') == NULL) {
          if (first_read) {
            return(-1);
          }
          setError((char*)"Unable to read row count (SDDSFile::readAsciiPage)");
          return(0);
        }
#if defined(zLib)
      }
#endif
      first_read = false;
      if (sscanf(bigBuffer, "%" SCNu32, &n_rows)!=1 || n_rows<0) {
        setError((char*)"Unable to read page--file has no (valid) number-of-rows entry (SDDSFile::readAsciiPage)");
        return(0);
      }
      if (n_rows == 0) {
        page++;
        return(page - 1);
      }
    }
    j=0;
    lines=linesPerRow;
    columnIndex=0;
    while ((j<n_rows) || noRowCount) {
      bigBuffer[0] = 0;
#if defined(zLib)
      if (gzipFile) {
        if (SDDS_fgetsSkipCommentsResize(&bigBuffer, &bigBufferSize, gzfp, '!') == NULL) {
          if (noRowCount) {
            page++;
            return(page - 1);
          }
          setError((char*)"Unable to read column values (SDDSFile::readAsciiPage)");
          setReadRecoveryMode(1);
          return(0);
        }
      } else {
#endif
        if (SDDS_fgetsSkipCommentsResize(&bigBuffer, &bigBufferSize, fp, '!') == NULL) {
          if (noRowCount) {
            page++;
            return(page - 1);
          }
          setError((char*)"Unable to read column values (SDDSFile::readAsciiPage)");
          setReadRecoveryMode(1);
          return(0);
        }
#if defined(zLib)
      }
#endif
      if ((noRowCount) && (SDDS_stringIsBlank(bigBuffer))) {
        break;
      }
      bigBufferCopy = bigBuffer;
      bigBufferCopySize = strlen(bigBufferCopy);
      for (i=columnIndex; i<columnCount; i++) {
        if (columnObject[i]->readAsciiValue(&bigBufferCopy, &bigBufferCopySize, 1)) {
          setError((char*)"Unable to read column (SDDSFile::readAsciiPage)");
          setReadRecoveryMode(1);
          return(0);
        }
        if ((i+1 != columnCount) && (SDDS_stringIsBlank(bigBufferCopy))) {
          if (lines > 1) {
            lines--;
            columnIndex = i+1;
            j--;
            break;
          } else {
            setError((char*)"Warning: missing element in row (SDDSFile::readAsciiPage)");
            setReadRecoveryMode(1);
            return(0);
          }
        } else if (linesPerRow > 1) {
          lines=linesPerRow;
          columnIndex=0;
        }
      }
      j++;
    }
  }
  page++;
  return(page - 1);
}

/***********************************************************************************************
 *                        readBinaryPages                                                      *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::readBinaryPages() {
  SDDS_FILEBUFFER fBuffer;
  int32_t i;
  uint32_t page=1;
  uint32_t n_rows, j;
  char *ptr;
  bool big, nonNativeEndian;
  big = SDDS_isBigEndianMachine();
  if (big == true) {
    if (bigEndian == true) {
      nonNativeEndian = false;
    } else {
      nonNativeEndian = true;
    }
  } else {
    if (bigEndian == true) {
      nonNativeEndian = true;
    } else {
      nonNativeEndian = false;
    }
  }
  if ((ptr = fBuffer.buffer = fBuffer.data = (char*)SDDS_malloc(sizeof(char)*(defaultIOBufferSize+1))) == NULL) {
    setError((char*)"Unable to do buffered read--allocation error (SDDSFile::readBinaryPages)");
    return(1);
  }
  fBuffer.bufferSize = defaultIOBufferSize;
  fBuffer.bytesLeft = 0;

#if defined(zLib)
  if (gzipFile) {
    if (gzfp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::readBinaryPages)");
      return(1);
    }
  } else {
#endif
    if (fp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::readBinaryPages)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  setReadRecoveryMode(0);
  while (true) {
#if defined(zLib)
    if (gzipFile) {
      if ((page==1) && (gzeof(gzfp))) {
        setError((char*)"No data in file (SDDSFile::readBinaryPages)");
        return(1);
      }
      if (SDDS_bufferedRead(&n_rows, sizeof(n_rows), gzfp, &fBuffer, this)) {
        if (gzeof(gzfp)) {
          SDDS_free(ptr);
          return(0);
        }
        setError((char*)"Unable to read page--failure reading number of rows (SDDSFile::readBinaryPages)");
        return(1);
      }
      if (columnCount == 0) {
        setError((char*)"Unable to read page--rows detected but no columns (SDDSFile::readBinaryPages)");
        return(1);
      }
      for (i=0; i<parameterCount; i++) {
        if (getParameterFixedValue(i) == NULL) {
          if (parameterObject[i]->readBinaryValue(gzfp, &fBuffer, page, nonNativeEndian)) {
            setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPages)");
            return(1);
          }
        }
      }
      for (i=0; i<arrayCount; i++) {
	/* ADD CODE HERE */

      }
      if (columnCount > 0) {
        if (columnMajorOrder && (layoutVersion >= 3)) {
            for (i=0; i<columnCount; i++) {
              if (columnObject[i]->readBinaryValues(gzfp, &fBuffer, page, n_rows, nonNativeEndian)) {
                setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPages)");
                setReadRecoveryMode(1);
                return(1);
              }
            }
        } else {
          for (j=1; j <= n_rows; j++) {
            for (i=0; i<columnCount; i++) {
              if (columnObject[i]->readBinaryValue(gzfp, &fBuffer, page, nonNativeEndian)) {
                setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPages)");
                setReadRecoveryMode(1);
                return(1);
              }
            }
          }
        }
      }
    } else {
#endif
      if ((page==1) && (feof(fp))) {
        setError((char*)"No data in file (SDDSFile::readBinaryPages)");
        return(1);
      }
      if (SDDS_bufferedRead(&n_rows, sizeof(n_rows), fp, &fBuffer, this)) {
        if (feof(fp)) {
          SDDS_free(ptr);
          return(0);
        }
        setError((char*)"Unable to read page--failure reading number of rows (SDDSFile::readBinaryPages)");
        return(1);
      }
      if (columnCount == 0) {
        setError((char*)"Unable to read page--rows detected but no columns (SDDSFile::readBinaryPages)");
        return(1);
      }
      for (i=0; i<parameterCount; i++) {
        if (getParameterFixedValue(i) == NULL) {
          if (parameterObject[i]->readBinaryValue(fp, &fBuffer, page, nonNativeEndian)) {
            setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPages)");
            return(1);
          }
        }
      }
      for (i=0; i<arrayCount; i++) {
	/* ADD CODE HERE */

      }
      if (columnCount > 0) {
        if (columnMajorOrder && (layoutVersion >= 3)) {
            for (i=0; i<columnCount; i++) {
              if (columnObject[i]->readBinaryValues(fp, &fBuffer, page, n_rows, nonNativeEndian)) {
                setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPages)");
                setReadRecoveryMode(1);
                return(1);
              }
            }
        } else {
          for (j=1; j <= n_rows; j++) {
            for (i=0; i<columnCount; i++) {
              if (columnObject[i]->readBinaryValue(fp, &fBuffer, page, nonNativeEndian)) {
                setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPages)");
                setReadRecoveryMode(1);
                return(1);
              }
            }
          }
        }
      }
#if defined(zLib)
    }
#endif
    page++;
  }
  SDDS_free(ptr);
  return(0);
}

/***********************************************************************************************
 *                        readBinaryPage                                                       *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * Results: page number on success                                                             *
 *          0 on failure                                                                       *
 *          -1 on EOF                                                                          *
 ***********************************************************************************************/
uint32_t SDDSFile::readBinaryPage() {
  SDDS_FILEBUFFER fBuffer;
  static uint32_t page=1;
  int32_t i;
  uint32_t n_rows, j;
  char *ptr;
  bool big, nonNativeEndian;
  big = SDDS_isBigEndianMachine();
  if (big == true) {
    if (bigEndian == true) {
      nonNativeEndian = false;
    } else {
      nonNativeEndian = true;
    }
  } else {
    if (bigEndian == true) {
      nonNativeEndian = true;
    } else {
      nonNativeEndian = false;
    }
  }
  if ((ptr = fBuffer.buffer = fBuffer.data = (char*)SDDS_malloc(sizeof(char)*(defaultIOBufferSize+1))) == NULL) {
    setError((char*)"Unable to do buffered read--allocation error (SDDSFile::readBinaryPage)");
    return(0);
  }
  fBuffer.bufferSize = defaultIOBufferSize;
  fBuffer.bytesLeft = 0;

#if defined(zLib)
  if (gzipFile) {
    if (gzfp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::readBinaryPage)");
      return(0);
    }
  } else {
#endif
    if (fp == NULL) {
      setError((char*)"Invalid file pointer (SDDSFile::readBinaryPage)");
      return(0);
    }
#if defined(zLib)
  }
#endif
  setReadRecoveryMode(0);
#if defined(zLib)
  if (gzipFile) {
    if ((page==1) && (gzeof(gzfp))) {
      setError((char*)"No data in file (SDDSFile::readBinaryPage)");
      return(0);
    }
    if (SDDS_bufferedRead(&n_rows, sizeof(n_rows), gzfp, &fBuffer, this)) {
      if (gzeof(gzfp)) {
        SDDS_free(ptr);
        return(-1);
      }
      setError((char*)"Unable to read page--failure reading number of rows (SDDSFile::readBinaryPage)");
      return(0);
    }
    if (columnCount == 0) {
      setError((char*)"Unable to read page--rows detected but no columns (SDDSFile::readBinaryPage)");
      return(0);
    }
    for (i=0; i<parameterCount; i++) {
      if (getParameterFixedValue(i) == NULL) {
        if (parameterObject[i]->readBinaryValue(gzfp, &fBuffer, page, nonNativeEndian)) {
          setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPage)");
          return(0);
        }
      }
    }
    for (i=0; i<arrayCount; i++) {
      /* ADD CODE HERE */
      
    }
    if (columnCount > 0) {
      if (columnMajorOrder && (layoutVersion >= 3)) {
        for (i=0; i<columnCount; i++) {
          if (columnObject[i]->readBinaryValues(gzfp, &fBuffer, page, n_rows, nonNativeEndian)) {
            setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPage)");
            setReadRecoveryMode(1);
            return(0);
          }
        }
      } else {
        for (j=1; j <= n_rows; j++) {
          for (i=0; i<columnCount; i++) {
            if (columnObject[i]->readBinaryValue(gzfp, &fBuffer, page, nonNativeEndian)) {
              setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPage)");
              setReadRecoveryMode(1);
              return(0);
            }
          }
        }
      }
    }
  } else {
#endif
    if ((page==1) && (feof(fp))) {
      setError((char*)"No data in file (SDDSFile::readBinaryPage)");
      return(0);
    }
    if (SDDS_bufferedRead(&n_rows, sizeof(n_rows), fp, &fBuffer, this)) {
      if (feof(fp)) {
        SDDS_free(ptr);
        return(-1);
      }
      setError((char*)"Unable to read page--failure reading number of rows (SDDSFile::readBinaryPage)");
      return(0);
    }
    if (columnCount == 0) {
      setError((char*)"Unable to read page--rows detected but no columns (SDDSFile::readBinaryPage)");
      return(0);
    }
    for (i=0; i<parameterCount; i++) {
      if (getParameterFixedValue(i) == NULL) {
        if (parameterObject[i]->readBinaryValue(fp, &fBuffer, page, nonNativeEndian)) {
          setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPage)");
          return(0);
        }
      }
    }
    for (i=0; i<arrayCount; i++) {
      /* ADD CODE HERE */
      
    }
    if (columnCount > 0) {
      if (columnMajorOrder && (layoutVersion >= 3)) {
        for (i=0; i<columnCount; i++) {
          if (columnObject[i]->readBinaryValues(fp, &fBuffer, page, n_rows, nonNativeEndian)) {
            setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPage)");
            setReadRecoveryMode(1);
            return(0);
          }
        }
      } else {
        for (j=1; j <= n_rows; j++) {
          for (i=0; i<columnCount; i++) {
            if (columnObject[i]->readBinaryValue(fp, &fBuffer, page, nonNativeEndian)) {
              setError((char*)"Unable to read page--failure reading parameter (SDDSFile::readBinaryPage)");
              setReadRecoveryMode(1);
              return(0);
            }
          }
        }
      }
    }
#if defined(zLib)
  }
#endif
  page++;
  SDDS_free(ptr);
  return(page - 1);
}

/***********************************************************************************************
 *                        pageCount                                                            *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: pages in the SDDS file                                                             *
 ***********************************************************************************************/
uint32_t SDDSFile::pageCount() {
  int32_t i;
  uint32_t endPage, pages;
  endPage = 0;
  for (i=0;i<parameterCount;i++) {
    pages = parameterObject[i]->getPageCount();
    if (pages > endPage)
      endPage = pages;
  }
  for (i=0;i<arrayCount;i++) {
    pages = arrayObject[i]->getPageCount();
    if (pages > endPage)
      endPage = pages;
  }
  for (i=0;i<columnCount;i++) {
    pages = columnObject[i]->getPageCount();
    if (pages > endPage)
      endPage = pages;
  }
  return endPage;	
}
uint32_t SDDSFile_pageCount(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->pageCount());
}

/***********************************************************************************************
 *                        rowCount                                                             *
 *                                                                                             *
 * C++ Arguments: uint32_t page                                                                 *
 *                                                                                             *
 * C Arguments: void *sddsfile, uint32_t page                                                   *
 *                                                                                             *
 * Results: rows in the SDDS file at the given page                                            *
 ***********************************************************************************************/
uint32_t SDDSFile::rowCount(uint32_t page) {
  int32_t i;
  uint32_t endRow, rows;
  endRow = 0;
  for (i=0;i<columnCount;i++) {
    rows = columnObject[i]->getRowCount(page);
    if (rows > endRow) {
      endRow = rows;
      if (i > 0) {
        fprintf(stderr, "Error: inconsistant row count for page %" PRId32 " (SDDSFile::rowCount)\n", page);
        exit(1);
      }
    }
  }
  return endRow;	
}
uint32_t SDDSFile_rowCount(void *sddsfile, uint32_t page) {
  return(((SDDSFile*)sddsfile)->rowCount(page));
}

/***********************************************************************************************
 *                        closeFile                                                            *
 *                                                                                             *
 * C++ Arguments: none                                                                         *
 *                                                                                             *
 * C Arguments: void *sddsfile                                                                 *
 *                                                                                             *
 * Results: 0 on success                                                                       *
 *          1 on failure                                                                       *
 ***********************************************************************************************/
int32_t SDDSFile::closeFile() {
#if defined(zLib)
  if (gzipFile) {
    if (gzfp == NULL) {
      setError((char*)"Warning: problem closing file (SDDSFile::closeFile)");
      return(1);
    }
    if (gzclose(gzfp)) {
      setError((char*)"Warning: problem closing file (SDDSFile::closeFile)");
      return(1);
    }
  } else {
#endif
    if (fp == NULL) {
      setError((char*)"Warning: problem closing file (SDDSFile::closeFile)");
      return(1);
    }
    if (fclose(fp)) {
      setError((char*)"Warning: problem closing file (SDDSFile::closeFile)");
      return(1);
    }
#if defined(zLib)
  }
#endif
  return(0);
}
int32_t SDDSFile_closeFile(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->closeFile());
}

/*
 * processArray
 *
 * C++ Arguments: char *s 
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::processArray(char *s) {
  char *name=NULL, *symbol=NULL, *units=NULL;
  char *description=NULL, *format_string=NULL, *group_name=NULL;
  char *type=NULL;
  uint32_t field_length=0, dimensions=0;
  char *ptr, *tag, *value;
  char buffer[SDDS_MAXLINE];
 
  ptr = SDDS_prepareToParseTagValuePairs(s);
  while (*ptr && !SDDS_stringIsBlank(ptr) && 
         (ptr=getTagValuePair(ptr, &tag, &value))) {
    if (!tag) {
      break;  /* normal termination */
    }
    if (strcmp(tag, (char*)"name") == 0) {
      if (SDDS_copyString(&name, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"type") == 0) {
      if (SDDS_copyString(&type, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"units") == 0) {
      if (SDDS_copyString(&units, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"symbol") == 0) {
      if (SDDS_copyString(&symbol, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"description") == 0) {
      if (SDDS_copyString(&description, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"format_string") == 0) {
      if (SDDS_copyString(&format_string, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"group_name") == 0) {
      if (SDDS_copyString(&group_name, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"field_length") == 0) {
      if (!sscanf(value, "%" SCNu32, &field_length)) {
        setError((char*)"Problem scanning int32_t value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"dimensions") == 0) {
      if (!sscanf(value, "%" SCNu32, &dimensions)) {
        setError((char*)"Problem scanning int32_t value for namelist");
        return(1);
      }
    } else {
      sprintf(buffer, "Unknown array tag %s", tag);
      setError(buffer);
      return(1);
    }
  }
  if (!ptr) {
    setError((char*)"Problem parsing array namelist");
    return(1);
  }
  if (defineArray(name, symbol, units, description, 
		  format_string, group_name, type, field_length, dimensions) == -1) {
    return(1);
  }
  if (name)
    SDDS_free(name);
  if (symbol)
    SDDS_free(symbol);
  if (units)
    SDDS_free(units);
  if (description)
    SDDS_free(description);
  if (format_string)
    SDDS_free(format_string);
  if (group_name)
    SDDS_free(group_name);
  if (type)
    SDDS_free(type);
  return(0);
}

/*
 * processColumn
 *
 * C++ Arguments: char *s 
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::processColumn(char *s) {
  char *name=NULL, *symbol=NULL, *units=NULL;
  char *description=NULL, *format_string=NULL;
  char *type=NULL;
  uint32_t field_length=0;
  char *ptr, *tag, *value;
  char buffer[SDDS_MAXLINE];
 
  ptr = SDDS_prepareToParseTagValuePairs(s);
  while (*ptr && !SDDS_stringIsBlank(ptr) && 
         (ptr=getTagValuePair(ptr, &tag, &value))) {
    if (!tag) {
      break;  /* normal termination */
    }
    if (strcmp(tag, (char*)"name") == 0) {
      if (SDDS_copyString(&name, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"type") == 0) {
      if (SDDS_copyString(&type, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"units") == 0) {
      if (SDDS_copyString(&units, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"symbol") == 0) {
      if (SDDS_copyString(&symbol, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"description") == 0) {
      if (SDDS_copyString(&description, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"format_string") == 0) {
      if (SDDS_copyString(&format_string, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"field_length") == 0) {
      if (!sscanf(value, "%" SCNu32, &field_length)) {
        setError((char*)"Problem scanning int32_t value for namelist");
        return(1);
      }
    } else {
      sprintf(buffer, "Unknown column tag %s", tag);
      setError(buffer);
      return(1);
    }
  }
  if (!ptr) {
    setError((char*)"Problem parsing column namelist");
    return(1);
  }
  if (defineColumn(name, symbol, units, description, 
                   format_string, type, field_length) == -1) {
    return(1);   
  }
  if (name)
    SDDS_free(name);
  if (symbol)
    SDDS_free(symbol);
  if (units)
    SDDS_free(units);
  if (description)
    SDDS_free(description);
  if (format_string)
    SDDS_free(format_string);
  if (type)
    SDDS_free(type);
  return(0);
}

/************************
 processData

 C++ Arguments: char *s 

 Results: 0 on success
          1 on failure
 ************************/
int32_t SDDSFile::processData(char *s) {
  int32_t no_row_counts=0;

  char *ptr, *tag, *value;
  char buffer[SDDS_MAXLINE];
 
  columnMajorOrder = false;
  ptr = SDDS_prepareToParseTagValuePairs(s);
  while (*ptr && !SDDS_stringIsBlank(ptr) && 
         (ptr=getTagValuePair(ptr, &tag, &value))) {
    if (!tag) {
      break;  /* normal termination */
    }
    if (strcmp(tag, (char*)"mode") == 0) {
      if (strcmp(value, (char*)"ascii") == 0) {
        setAsciiMode();
      } else if (strcmp(value, (char*)"binary") == 0) {
        setBinaryMode();
      }
    } else if (strcmp(tag, (char*)"column_major_order") == 0) {
      if (strcmp(value, "true") == 0) {
        columnMajorOrder = true;
      } else {
        columnMajorOrder = false;
      }
    } else if (strcmp(tag, (char*)"no_row_counts") == 0) {
      if (strcmp(value, "true") == 0) {
        setNoRowCount();
      } else if (strcmp(value, "false") == 0) {
        setUseRowCount();
      } else {
        if (!sscanf(value, "%" SCNd32, &no_row_counts)) {
          setError((char*)"Problem scanning int32_t value for namelist");
          return(1);
        }
        if (no_row_counts) {
          setNoRowCount();
        } else {
          setUseRowCount();
        }
      }
    } else if (strcmp(tag, (char*)"endian") == 0) {
      if (strcmp(value, (char*)"big") == 0) {
        bigEndian = true;
      } else if (strcmp(value, (char*)"little") == 0) {
        bigEndian = false;
      }
    } else if (strcmp(tag, (char*)"lines_per_row") == 0) {
      if (!sscanf(value, "%" SCNd32, &linesPerRow)) {
        setError((char*)"Problem scanning int32_t value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"additional_header_lines") == 0) {
      if (!sscanf(value, "%" SCNd32, &additionalHeaderLines)) {
        setError((char*)"Problem scanning int32_t value for namelist");
        return(1);
      }
    } else {
      sprintf(buffer, "Unknown data tag %s", tag);
      setError(buffer);
      return(1);
    }
  }
  if (!ptr) {
    setError((char*)"Problem parsing data namelist");
    return(1);
  }
  return(0);
}

/************************
 processDescription

 C++ Arguments: char *s 

 Results: 0 on success
          1 on failure
 ************************/
int32_t SDDSFile::processDescription(char *s) {
  char *text=NULL, *contents=NULL;
  char *ptr, *tag, *value;
  char buffer[SDDS_MAXLINE];
 
  ptr = SDDS_prepareToParseTagValuePairs(s);
  while (*ptr && !SDDS_stringIsBlank(ptr) && 
         (ptr=getTagValuePair(ptr, &tag, &value))) {
    if (!tag) {
      break;  /* normal termination */
    }
    if (strcmp(tag, (char*)"text") == 0) {
      if (SDDS_copyString(&text, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"contents") == 0) {
      if (SDDS_copyString(&contents, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else {
      sprintf(buffer, "Unknown description tag %s", tag);
      setError(buffer);
      return(1);
    }
  }
  if (!ptr) {
    setError((char*)"Problem parsing description namelist");
    return(1);
  }
  setDescription(text, contents);
  if (text)
    SDDS_free(text);
  if (contents)
    SDDS_free(contents);
  return(0);
}

/************************
 processParameter

 C++ Arguments: char *s 

 Results: 0 on success
          1 on failure
 ************************/
int32_t SDDSFile::processParameter(char *s) {
  char *name=NULL, *symbol=NULL, *units=NULL;
  char *description=NULL, *format_string=NULL, *fixed_value=NULL;
  char *type=NULL;
  char *ptr, *tag, *value;
  char buffer[SDDS_MAXLINE];
 
  ptr = SDDS_prepareToParseTagValuePairs(s);
  while (*ptr && !SDDS_stringIsBlank(ptr) && 
         (ptr=getTagValuePair(ptr, &tag, &value))) {
    if (!tag) {
      break;  /* normal termination */
    }
    if (strcmp(tag, (char*)"name") == 0) {
      if (SDDS_copyString(&name, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"type") == 0) {
      if (SDDS_copyString(&type, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"units") == 0) {
      if (SDDS_copyString(&units, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"symbol") == 0) {
      if (SDDS_copyString(&symbol, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"description") == 0) {
      if (SDDS_copyString(&description, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"format_string") == 0) {
      if (SDDS_copyString(&format_string, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
    } else if (strcmp(tag, (char*)"fixed_value") == 0) {
      if (SDDS_copyString(&fixed_value, value)) {
        setError((char*)"Problem setting string value for namelist");
        return(1);
      }
      SDDS_interpretEscapes(fixed_value);
    } else {
      sprintf(buffer, "Unknown parameter tag %s", tag);
      setError(buffer);
      return(1);
    }
  }
  if (!ptr) {
    setError((char*)"Problem parsing parameter namelist");
    return(1);
  }
  if (defineParameter(name, symbol, units, description, 
                      format_string, type, fixed_value) == -1) {
    return(1);   
  }
  if (name)
    SDDS_free(name);
  if (symbol)
    SDDS_free(symbol);
  if (units)
    SDDS_free(units);
  if (description)
    SDDS_free(description);
  if (format_string)
    SDDS_free(format_string);
  if (fixed_value)
    SDDS_free(fixed_value);
  if (type)
    SDDS_free(type);
  return(0);
}

char* SDDSFile::getTagValuePair(char *ptr, char **tag, char **value) {
  char buffer[1024];

  if (!*ptr) {
    return(NULL);
  }
  *tag = *value = NULL;
  while (*ptr==' ' || *ptr==',' || *ptr=='\t') {
    ptr++;
  }
  if (!*ptr) {
    return(ptr);
  }
    
  *tag = ptr;  /* start of field name */
  while (*ptr && *ptr!=' ' && *ptr!='\t' && *ptr!='=') {
    ptr++;
  }
  if (*ptr!='=') {
    *ptr++ = 0;   /* ends the field name */
    /* skip something of the form <0 or more spaces>=<0 or more spaces> */
    while (*ptr==' ' || *ptr=='\t') {
      ptr++;
    }
    if (*ptr++!='=' || !*ptr) {
      sprintf(buffer, "Namelist error: Field name %s ends string", *tag);
      setError(buffer);
      return(NULL);
    }
  } else {
    *ptr++ = 0;
  }
  while (*ptr==' ' || *ptr=='\t') {
    ptr++;
  }
  if (!*ptr) {
    sprintf(buffer, "Namelist error: Field name %s ends string", *tag);
    setError(buffer);
    return(NULL);
  }
  if (!strlen(*tag)) {
    sprintf(buffer, "Namelist error: Field name is blank");
    setError(buffer);
    return(NULL);
  }
   
  if (*ptr=='"' && *(ptr-1)!='\\') {
    ptr++;
    if (*ptr=='"' && *(ptr-1)!='\\') {
      *ptr = 0;
      *value = ptr;
      return(ptr+1);
    }
    *value = ptr++;
    while (*ptr && !(*ptr=='"' && *(ptr-1)!='\\')) {
      ptr++;
    }
    if (*ptr=='"' && *(ptr-1)!='\\') {
      *ptr = 0; /* end of value */
    }
  } else {
    *value = ptr;   /* start of value */
    while (*ptr!=' ' && *ptr!=',' && *ptr!='\t' && *ptr) {
      ptr++;
    }
    if (!*ptr) {
      return ptr;
    }
    *ptr = 0; /* end of value */
  }
  return(ptr+1);
}

/****************************
 copyParameterDefinitions

 C++ Arguments: void *sdds_source

 C Arguments: void *sddsfile, void *sdds_source

 Results: 0 on success
          1 on failure
*****************************/
int32_t SDDSFile::copyParameterDefinitions(void *sdds_source) {
  int32_t count, i, *type;
  char **name, **symbol, **units, **description, **format_string, **fixed_value;
  
  count = ((SDDSFile*)sdds_source)->getParameterCount();
  name = ((SDDSFile*)sdds_source)->getParameterNames();
  symbol = ((SDDSFile*)sdds_source)->getParameterSymbols();
  units = ((SDDSFile*)sdds_source)->getAllParameterUnits();
  description = ((SDDSFile*)sdds_source)->getParameterDescriptions();
  format_string = ((SDDSFile*)sdds_source)->getParameterFormatStrings();
  fixed_value = ((SDDSFile*)sdds_source)->getParameterFixedValues();
  type = ((SDDSFile*)sdds_source)->getParameterTypes();
  for (i=0 ; i<count ; i++) {
    if (defineParameter(name[i], symbol[i], units[i], description[i], 
                        format_string[i], type[i], fixed_value[i]) == -1) {
      return(1);
    }
  }
  if (count > 0) {
    SDDS_free(name);
    SDDS_free(symbol);
    SDDS_free(units);
    SDDS_free(description);
    SDDS_free(format_string);
    SDDS_free(fixed_value);
    SDDS_free(type);
  }
  return(0);
}
int32_t SDDSFile_copyParameterDefinitions(void *sddsfile, void *sdds_source) {
  return(((SDDSFile*)sddsfile)->copyParameterDefinitions(sdds_source));
}

/****************************
 copyParameterDefinition

 C++ Arguments: void *sdds_source, char *orig_parameter_name, char *new_parameter_name

 C Arguments: void *sddsfile, void *sdds_source, char *orig_parameter_name, char *new_parameter_name

 Results: 0 on success
          1 on failure
*****************************/
int32_t SDDSFile::copyParameterDefinition(void *sdds_source, char *orig_parameter_name, char *new_parameter_name) {
  int32_t i;
  char s[SDDS_MAXLINE];
  i = ((SDDSFile*)sdds_source)->getParameterIndex(orig_parameter_name);
  if (i < 0) {
    sprintf(s, "Warning: parameter %s does not exist (SDDSFile::copyParameterDefinition)", orig_parameter_name);
    setError(s);
    return(1);
  }
  if (defineParameter(new_parameter_name, 
                      ((SDDSFile*)sdds_source)->getParameterSymbol(i),
                      ((SDDSFile*)sdds_source)->getParameterUnits(i),
                      ((SDDSFile*)sdds_source)->getParameterDescription(i),
                      ((SDDSFile*)sdds_source)->getParameterFormatString(i),
                      ((SDDSFile*)sdds_source)->getParameterType(i),
                      ((SDDSFile*)sdds_source)->getParameterFixedValue(i)) == -1) {
    return(1);
  }
  return(0);
}
int32_t SDDSFile_copyParameterDefinition(void *sddsfile, void *sdds_source, char *orig_parameter_name, char *new_parameter_name) {
  return(((SDDSFile*)sddsfile)->copyParameterDefinition(sdds_source, orig_parameter_name, new_parameter_name));
}

/*
 * copyArrayDefinitions
 *
 * C++ Arguments: void *sdds_source
 *
 * C Arguments: void *sddsfile, void *sdds_source
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::copyArrayDefinitions(void *sdds_source) {
  int32_t count, i, *type;
  uint32_t *field_length, *dimensions;
  char **name, **symbol, **units, **description, **format_string, **group_name;
  
  count = ((SDDSFile*)sdds_source)->getArrayCount();
  name = ((SDDSFile*)sdds_source)->getArrayNames();
  symbol = ((SDDSFile*)sdds_source)->getArraySymbols();
  units = ((SDDSFile*)sdds_source)->getAllArrayUnits();
  description = ((SDDSFile*)sdds_source)->getArrayDescriptions();
  format_string = ((SDDSFile*)sdds_source)->getArrayFormatStrings();
  group_name = ((SDDSFile*)sdds_source)->getArrayGroupNames();
  field_length = ((SDDSFile*)sdds_source)->getArrayFieldLengths();
  dimensions = ((SDDSFile*)sdds_source)->getAllArrayDimensions();
  type = ((SDDSFile*)sdds_source)->getArrayTypes();
  for (i=0 ; i<count ; i++) {
    if (defineArray(name[i], symbol[i], units[i], description[i], 
		    format_string[i], group_name[i], type[i], field_length[i], dimensions[i]) == -1) {
      return(1);
    }
  }
  if (count > 0) {
    SDDS_free(name);
    SDDS_free(symbol);
    SDDS_free(units);
    SDDS_free(description);
    SDDS_free(format_string);
    SDDS_free(group_name);
    SDDS_free(field_length);
    SDDS_free(dimensions);
    SDDS_free(type);
  }
  return(0);
}
int32_t SDDSFile_copyArrayDefinitions(void *sddsfile, void *sdds_source) {
  return(((SDDSFile*)sddsfile)->copyArrayDefinitions(sdds_source));
}

/*
 * copyArrayDefinition
 *
 * C++ Arguments: void *sdds_source, char *orig_array_name, char *new_array_name
 *
 * C Arguments: void *sddsfile, void *sdds_source, char *orig_array_name, char *new_array_name
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::copyArrayDefinition(void *sdds_source, char *orig_array_name, char *new_array_name) {
  int32_t i;
  char s[SDDS_MAXLINE];
  i = ((SDDSFile*)sdds_source)->getArrayIndex(orig_array_name);
  if (i < 0) {
    sprintf(s, "Warning: array %s does not exist (SDDSFile::copyArrayDefinition)", orig_array_name);
    setError(s);
    return(1);
  }
  if (defineArray(new_array_name, 
                   ((SDDSFile*)sdds_source)->getArraySymbol(i),
                   ((SDDSFile*)sdds_source)->getArrayUnits(i),
                   ((SDDSFile*)sdds_source)->getArrayDescription(i),
                   ((SDDSFile*)sdds_source)->getArrayFormatString(i),
                   ((SDDSFile*)sdds_source)->getArrayGroupName(i),
                   ((SDDSFile*)sdds_source)->getArrayType(i),
		   ((SDDSFile*)sdds_source)->getArrayFieldLength(i),
                   ((SDDSFile*)sdds_source)->getArrayDimensions(i)) == -1) {
    return(1);
  }
  return(0);
}
int32_t SDDSFile_copyArrayDefinition(void *sddsfile, void *sdds_source, char *orig_array_name, char *new_array_name) {
  return(((SDDSFile*)sddsfile)->copyArrayDefinition(sdds_source, orig_array_name, new_array_name));
}

/*
 * copyColumnDefinitions
 *
 * C++ Arguments: void *sdds_source
 *
 * C Arguments: void *sddsfile, void *sdds_source
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::copyColumnDefinitions(void *sdds_source) {
  int32_t count, i, *type;
  uint32_t *field_length;
  char **name, **symbol, **units, **description, **format_string;
  
  count = ((SDDSFile*)sdds_source)->getColumnCount();
  name = ((SDDSFile*)sdds_source)->getColumnNames();
  symbol = ((SDDSFile*)sdds_source)->getColumnSymbols();
  units = ((SDDSFile*)sdds_source)->getAllColumnUnits();
  description = ((SDDSFile*)sdds_source)->getColumnDescriptions();
  format_string = ((SDDSFile*)sdds_source)->getColumnFormatStrings();
  field_length = ((SDDSFile*)sdds_source)->getColumnFieldLengths();
  type = ((SDDSFile*)sdds_source)->getColumnTypes();
  for (i=0 ; i<count ; i++) {
    if (defineColumn(name[i], symbol[i], units[i], description[i], 
                        format_string[i], type[i], field_length[i]) == -1) {
      return(1);
    }
  }
  if (count > 0) {
    SDDS_free(name);
    SDDS_free(symbol);
    SDDS_free(units);
    SDDS_free(description);
    SDDS_free(format_string);
    SDDS_free(field_length);
    SDDS_free(type);
  }
  return(0);
}
int32_t SDDSFile_copyColumnDefinitions(void *sddsfile, void *sdds_source) {
  return(((SDDSFile*)sddsfile)->copyColumnDefinitions(sdds_source));
}

/*
 * copyColumnDefinition
 *
 * C++ Arguments: void *sdds_source, char *orig_column_name, char *new_column_name
 *
 * C Arguments: void *sddsfile, void *sdds_source, char *orig_column_name, char *new_column_name
 *
 * Results: 0 on success
 *          1 on failure
 */
int32_t SDDSFile::copyColumnDefinition(void *sdds_source, char *orig_column_name, char *new_column_name) {
  int32_t i;
  char s[SDDS_MAXLINE];
  i = ((SDDSFile*)sdds_source)->getColumnIndex(orig_column_name);
  if (i < 0) {
    sprintf(s, "Warning: column %s does not exist (SDDSFile::copyColumnDefinition)", orig_column_name);
    setError(s);
    return(1);
  }
  if (defineColumn(new_column_name, 
                   ((SDDSFile*)sdds_source)->getColumnSymbol(i),
                   ((SDDSFile*)sdds_source)->getColumnUnits(i),
                   ((SDDSFile*)sdds_source)->getColumnDescription(i),
                   ((SDDSFile*)sdds_source)->getColumnFormatString(i),
                   ((SDDSFile*)sdds_source)->getColumnType(i),
                   ((SDDSFile*)sdds_source)->getColumnFieldLength(i)) == -1) {
    return(1);
  }
  return(0);
}
int32_t SDDSFile_copyColumnDefinition(void *sddsfile, void *sdds_source, char *orig_column_name, char *new_column_name) {
  return(((SDDSFile*)sddsfile)->copyColumnDefinition(sdds_source, orig_column_name, new_column_name));
}

/****************************
 readRecoveryPossible

 C++ Arguments: none

 C Arguments: void *sddsfile

 Results: 1 for possible
          0 for not possible
*****************************/
int32_t SDDSFile::readRecoveryPossible(void) {
  int32_t returnValue;
  returnValue = recoveryPossible;
  recoveryPossible = 0;
  return(returnValue);
}

int32_t SDDSFile_readRecoveryPossible(void *sddsfile) {
  return(((SDDSFile*)sddsfile)->readRecoveryPossible());
}

/****************************
 setReadRecoveryMode

 C++ Arguments:int32_t mode 

 Results: none
*****************************/
void SDDSFile::setReadRecoveryMode(int32_t mode) {
  recoveryPossible = mode;
}
