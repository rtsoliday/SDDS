#if !defined(SDDS3_ENABLE_UNMODERNIZED_API)
#include "SDDS3Legacy.h"
#else

#define __STDC_FORMAT_MACROS 1
#if defined(_WIN32)
#include <stdlib.h>
#include <float.h>
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define PRId16 "d"
#define SCNd16 "hd"
#define PRIu16 "u"
#define SCNu16 "hu"
#define PRId32 "ld"
#define SCNd32 "ld"
#define PRIu32 "lu"
#define SCNu32 "lu"
#define PRId64 "lld"
#define SCNd64 "lld"
#define PRIu64 "llu"
#define SCNu64 "llu"
#define strtoll _strtoi64
#if defined(__BORLANDC__)
#define strcasecmp(s, t) stricmp(s, t)
#define strncasecmp(s, t, n) strnicmp(s, t, n)
#else
#define strcasecmp(s, t) _stricmp(s, t)
#define strncasecmp(s, t, n) _strnicmp(s, t, n)
#endif

#undef epicsShareFuncSDDS3
#undef epicsShareExtern
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_SDDS3)
#define epicsShareFuncSDDS3  __declspec(dllexport)
#define epicsShareExtern extern __declspec(dllexport)
#else
#define epicsShareFuncSDDS3
#define epicsShareExtern extern __declspec(dllimport)
#endif
#else
#define epicsShareFuncSDDS3
#define epicsShareExtern extern
#endif

#else
#include <inttypes.h>
#endif

#if defined(zLib)
#include "zlib.h"
#endif


#if !defined(zLib)
  typedef void *voidp;
  typedef voidp gzFile;
#endif

#define UINT_MAX_HALF UINT_MAX / 2
#define UINT_MAX_QUARTER UINT_MAX / 4
#define UINT_MAX_EIGHTH UINT_MAX / 8

#ifdef __cplusplus

typedef struct {
  char *data, *buffer;
  int32_t bytesLeft, bufferSize;
} SDDS_FILEBUFFER ;

class SDDSArray {
 public:
  SDDSArray();
  ~SDDSArray();

  void      setupArray(char *name, char *symbol, char *units, char *description, 
		       char *format_string, char *group_name, int32_t type, 
		       uint32_t field_length, uint32_t dimensions);
  void      freePage();
  void      informAboutParent(void *parent);
  char*     getDescription();
  char*     getGroupName();
  char*     getFormatString();
  char*     getName();
  uint32_t  getPageCount();
  char*     getSymbol();
  int32_t   getType();
  char*     getUnits();
  uint32_t  getFieldLength();
  uint32_t  getDimensions();
  uint32_t* getDim(uint32_t page);
  void*     getInternalValues(uint32_t page);
  int32_t*  getValuesInInt32(uint32_t page, int32_t *errorcode);
  uint32_t* getValuesInUInt32(uint32_t page, int32_t *errorcode);
  double*   getValuesInDouble(uint32_t page, int32_t *errorcode);
  char**    getValuesInString(uint32_t page, int32_t *errorcode);
  int32_t   readAsciiValues(char **bigBuffer, int32_t *bigBufferSize, uint32_t page);
  int32_t   readBinaryValues(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);
  int32_t   readBinaryValues(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);
  void      setDescription(char *name);
  void      setDim(uint32_t page, uint32_t *dimensions);
  void      setGroupName(char *name);
  void      setFormatString(char *name);
  void      setName(char *name);
  void      setSymbol(char *name);
  void      setType(int32_t type);
  void      setUnits(char *name);
  void      setDimensions(uint32_t dimensions);
  void      setFieldLength(uint32_t field_length);
  int32_t   setValues(int16_t *values, uint32_t page, uint32_t elements);
  int32_t   setValues(uint16_t *values, uint32_t page, uint32_t elements);
  int32_t   setValues(int32_t *values, uint32_t page, uint32_t elements);
  int32_t   setValues(uint32_t *values, uint32_t page, uint32_t elements);
  int32_t   setValues(float *values, uint32_t page, uint32_t elements);
  int32_t   setValues(double *values, uint32_t page, uint32_t elements);
  int32_t   setValues(char **values, uint32_t page, uint32_t elements);
  int32_t   setValues(char *values, uint32_t page, uint32_t elements);
  int32_t   writeDefinition(FILE *fp);
  int32_t   writeDefinition(voidp *gzfp);
  int32_t   writeAsciiValues(FILE *fp, uint32_t page);
  int32_t   writeAsciiValues(voidp *gzfp, uint32_t page);
  int32_t   writeBinaryValues(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);
  int32_t   writeBinaryValues(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);

  char*      arrayName;
  char*      arraySymbol;
  char*      arrayUnits;
  char*      arrayDescription;
  char*      arrayFormatString;
  char*      arrayGroupName;
  int32_t    arrayType;
  uint32_t   arrayFieldLength;
  uint32_t   arrayDimensions;
  uint32_t** arrayDim;
  uint32_t   pageCount;
  uint32_t*  elementCount; 
  uint32_t*  elementsAllocated; 
  uint32_t   elementAllocationIncrement;

  int32_t**  int32Value;
  int16_t**  int16Value;
  uint32_t** uint32Value;
  uint16_t** uint16Value;
  double**   real64Value;
  float**    real32Value;
  char***    stringValue;
  char**     charValue;
  void*      sddsfile;
 private:

 protected:

};

class SDDSParameter {
 public:
  SDDSParameter();
  ~SDDSParameter();

  void      setupParameter(char *name, char *symbol, char *units, char *description, 
			   char *format_string, int32_t type, char *fixed_value);
  void      freePage();
  void      informAboutParent(void *parent);
  char*     getDescription();
  char*     getFixedValue();
  char*     getFormatString();
  char*     getName();
  uint32_t  getPageCount();
  char*     getSymbol();
  int32_t   getType();
  char*     getUnits();
  int32_t   getValueInInt32(uint32_t page, int32_t *errorcode);
  uint32_t  getValueInUInt32(uint32_t page, int32_t *errorcode);
  double    getValueInDouble(uint32_t page, int32_t *errorcode);
  char*     getValueInString(uint32_t page, int32_t *errorcode);
  int32_t*  getValuesInInt32(int32_t *errorcode);
  uint32_t* getValuesInUInt32(int32_t *errorcode);
  double*   getValuesInDouble(int32_t *errorcode);
  char**    getValuesInString(int32_t *errorcode);
  int32_t   readBinaryValue(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);
  int32_t   readBinaryValue(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);
  int32_t   readAsciiValue(char **bigBuffer, int32_t *bigBufferSize, uint32_t page);
  void      setDescription(char *name);
  void      setFixedValue(char *name);
  void      setFormatString(char *name);
  void      setName(char *name);
  void      setSymbol(char *name);
  void      setType(int32_t type);
  void      setUnits(char *name);
  int32_t   setValue(int32_t value, uint32_t page);
  int32_t   setValue(uint32_t value, uint32_t page);
  int32_t   setValue(double value, uint32_t page);
  int32_t   setValue(char *value, uint32_t page);
  int32_t   writeDefinition(FILE *fp);
  int32_t   writeDefinition(voidp *gzfp);
  int32_t   writeAsciiValue(FILE *fp, uint32_t page);
  int32_t   writeAsciiValue(voidp *gzfp, uint32_t page);
  int32_t   writeBinaryValue(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);
  int32_t   writeBinaryValue(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);


  char*     parameterName;
  char*     parameterSymbol;
  char*     parameterUnits;
  char*     parameterDescription;
  char*     parameterFormatString;
  int32_t   parameterType;
  char*     parameterFixedValue;
  uint32_t  pageCount;
  bool      fixed;
  int32_t*  int32Value;
  int16_t*  int16Value;
  uint32_t* uint32Value;
  uint16_t* uint16Value;
  double*   real64Value;
  float*    real32Value;
  char**    stringValue;
  char*     charValue;
  void*     sddsfile;
 private:

 protected:

};

class SDDSColumn {
 public:
  SDDSColumn();
  ~SDDSColumn();
  void      setupColumn(char *name, char *symbol, char *units, char *description, 
			   char *format_string, int32_t type, uint32_t field_length);
  void      freePage();
  void      informAboutParent(void *parent);
  char*     getDescription();
  uint32_t  getFieldLength();
  char*     getFormatString();
  void*     getInternalValues(uint32_t page);
  char*     getName();
  uint32_t  getPageCount();
  uint32_t  getRowCount(uint32_t page);
  char*     getSymbol();
  int32_t   getType();
  char*     getUnits();
  int32_t*  getValuesInInt32(uint32_t page, int32_t *errorcode);
  uint32_t* getValuesInUInt32(uint32_t page, int32_t *errorcode);
  double*   getValuesInDouble(uint32_t page, int32_t *errorcode);
  char**    getValuesInString(uint32_t page, int32_t *errorcode);
  int32_t   readAsciiValue(char **bigBuffer, int32_t *bigBufferSize, uint32_t page);
  int32_t   readBinaryValue(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);
  int32_t   readBinaryValue(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);
  int32_t   readBinaryValues(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t rows, bool nonNativeEndian);
  int32_t   readBinaryValues(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t rows, bool nonNativeEndian);
  void      setDescription(char *name);
  void      setFieldLength(uint32_t field_length);
  void      setFormatString(char *name);
  void      setName(char *name);
  void      setSymbol(char *name);
  void      setType(int32_t type);
  void      setUnits(char *name);
  int32_t   setValues(int16_t *values, uint32_t page, uint32_t startRow, uint32_t rows);
  int32_t   setValues(uint16_t *values, uint32_t page, uint32_t startRow, uint32_t rows);
  int32_t   setValues(int32_t *values, uint32_t page, uint32_t startRow, uint32_t rows);
  int32_t   setValues(uint32_t *values, uint32_t page, uint32_t startRow, uint32_t rows);
  int32_t   setValues(float *values, uint32_t page, uint32_t startRow, uint32_t rows);
  int32_t   setValues(double *values, uint32_t page, uint32_t startRow, uint32_t rows);
  int32_t   setValues(char **values, uint32_t page, uint32_t startRow, uint32_t rows);
  int32_t   setValues(char *values, uint32_t page, uint32_t startRow, uint32_t rows);
  int32_t   writeDefinition(FILE *fp);
  int32_t   writeDefinition(voidp *gzfp);
  int32_t   writeAsciiValue(FILE *fp, uint32_t page, uint32_t row);
  int32_t   writeAsciiValue(voidp *gzfp, uint32_t page, uint32_t row);
  int32_t   writeBinaryValue(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t row, bool nonNativeEndian);
  int32_t   writeBinaryValue(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, uint32_t row, bool nonNativeEndian);
  int32_t   writeBinaryValues(FILE *fp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);
  int32_t   writeBinaryValues(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, uint32_t page, bool nonNativeEndian);

  char*      columnName;
  char*      columnSymbol;
  char*      columnUnits;
  char*      columnDescription;
  char*      columnFormatString;
  int32_t    columnType;
  uint32_t   columnFieldLength;
  uint32_t   pageCount;
  uint32_t*  rowCount;
  uint32_t*  rowsAllocated;
  uint32_t   rowAllocationIncrement;
  int32_t**  int32Value;
  int16_t**  int16Value;
  uint32_t** uint32Value;
  uint16_t** uint16Value;
  double**   real64Value;
  float**    real32Value;
  char***    stringValue;
  char**     charValue;
  void*      sddsfile;

 private:

 protected:

};


class SDDSFile {
 public:
  SDDSFile();
  SDDSFile(char *filename);
  SDDSFile(bool binary);
  SDDSFile(char *filename, bool binary);
  ~SDDSFile();

  int32_t   checkForErrors();
  void      clearErrors();
  int32_t   closeFile();
  void      freePage();
  uint32_t  getDataMode();
  void      setDataMode(uint32_t mode);
  int32_t   initializeInput(char *filename);
  int32_t   initializeOutput(int32_t data_mode, int32_t lines_per_row, 
                             char *description, char *contents, char *filename);
  int32_t   openInputFile();
  int32_t   openOutputFile();
  uint32_t  pageCount();
  uint32_t  rowCount(uint32_t page);
  void      printErrors(FILE *fp, int32_t mode);
  void      printErrors();
  int32_t   readLayout();
  uint32_t  readAsciiPage();
  int32_t   readAsciiPages();
  uint32_t  readBinaryPage();
  int32_t   readBinaryPages();
  int32_t   readFile();
  int32_t   readPage();
  int32_t   readPages();
  int32_t   readRecoveryPossible(void);
  int32_t   readVersion();
  void      setAsciiMode();
  void      setBinaryMode();
  void      setColumnMajorOrder();
  void      setDescription(char *text, char *contents);
  void      setError(char *error_text);
  void      setFileName(char *filename);
  void      setLayoutVersion(int32_t version);
  void      setNativeEndian();
  void      setNonNativeEndian();
  void      setNoRowCount();
  void      setReadRecoveryMode(int32_t mode);
  void      setRowMajorOrder();
  void      setUseRowCount();
  int32_t   writeDataMode();
  int32_t   writeDescription();
  int32_t   writeFile();
  int32_t   writeLayout();
  int32_t   writeAsciiPage(uint32_t page);
  int32_t   writeBinaryPage(uint32_t page);
  int32_t   writePage(uint32_t page);
  int32_t   writePages();
  int32_t   writePages(uint32_t startPage, uint32_t endPage);
  int32_t   writeArrayDefinition(int32_t arrayIndex);
  int32_t   writeArrayDefinitions();
  int32_t   writeColumnDefinition(int32_t columnIndex);
  int32_t   writeColumnDefinitions();
  int32_t   writeParameterDefinition(int32_t parameterIndex);
  int32_t   writeParameterDefinitions();
  int32_t   writeVersion(int32_t version_number);




  int32_t   copyArrayDefinitions(void *sdds_source);
  int32_t   copyArrayDefinition(void *sdds_source, char *orig_array_name, char *new_array_name);
  int32_t   copyColumnDefinitions(void *sdds_source);
  int32_t   copyColumnDefinition(void *sdds_source, char *orig_column_name, char *new_column_name);
  int32_t   copyParameterDefinitions(void *sdds_source);
  int32_t   copyParameterDefinition(void *sdds_source, char *orig_parameter_name, char *new_parameter_name);

  int32_t   defineArray(char *name, char *symbol, char *units, 
			char *description, char *format_string, 
			char *group_name, int32_t type, 
			uint32_t field_length, uint32_t dimensions);
  int32_t   defineArray(char *name, char *symbol, char *units, 
			char *description, char *format_string, 
			char *group_name, char *type, 
			uint32_t field_length, uint32_t dimensions);
  int32_t   defineArray(char *name, int32_t type, uint32_t dimensions);
  int32_t   defineArray(char *name, char *type, uint32_t dimensions);
  int32_t   defineArrays(int32_t number, char **name, char **symbol, 
			 char **units, char **description, 
			 char **format_string, char **group_name, 
			 int32_t *type, uint32_t *field_length,
			 uint32_t *dimensions);
  int32_t   defineArrays(int32_t number, char **name, char **symbol, 
			 char **units, char **description, 
			 char **format_string, char **group_name,
			 char **type, uint32_t *field_length, 
			 uint32_t *dimensions);
  int32_t   defineArrays(int32_t number, char **name, int32_t *type, uint32_t *dimensions);
  int32_t   defineArrays(int32_t number, char **name, char **type, uint32_t *dimensions);

  int32_t   defineColumn(char *name, char *symbol, char *units, 
			 char *description, char *format_string, 
			 int32_t type, uint32_t field_length);
  int32_t   defineColumn(char *name, char *symbol, char *units, 
			 char *description, char *format_string, 
			 char *type, uint32_t field_length);
  int32_t   defineColumn(char *name, int32_t type);
  int32_t   defineColumn(char *name, char *type);
  int32_t   defineColumns(int32_t number, char **name, char **symbol, 
			  char **units, char **description, 
			  char **format_string, int32_t *type, 
			  uint32_t *field_length);
  int32_t   defineColumns(int32_t number, char **name, char **symbol, 
			  char **units, char **description, 
			  char **format_string, char **type, 
			  uint32_t *field_length);
  int32_t   defineColumns(int32_t number, char **name, int32_t *type);
  int32_t   defineColumns(int32_t number, char **name, char **type);


  int32_t   defineParameter(char *name, char *symbol, char *units, 
			    char *description, char *format_string, 
			    int32_t type, char *fixed_value);
  int32_t   defineParameter(char *name, char *symbol, char *units, 
			    char *description, char *format_string, 
			    char *type, char *fixed_value);
  int32_t   defineParameter(char *name, int32_t type);
  int32_t   defineParameter(char *name, char *type);
  int32_t   defineParameters(int32_t number, char **name, char **symbol, 
			     char **units, char **description, 
			     char **format_string, int32_t *type, 
			     char **fixed_value);
  int32_t   defineParameters(int32_t number, char **name, char **symbol, 
			     char **units, char **description, 
			     char **format_string, char **type, 
			     char **fixed_value);
  int32_t   defineParameters(int32_t number, char **name, int32_t *type);
  int32_t   defineParameters(int32_t number, char **name, char **type);

  int32_t   getArrayCount();
  int32_t   getArrayIndex(char *name);
  int32_t*  getArrayIndexes(char** names, int32_t number);
  int32_t*  getArrayInInt32(int32_t arrayIndex, uint32_t page);
  int32_t*  getArrayInInt32(char *name, uint32_t page);
  uint32_t* getArrayInUInt32(int32_t arrayIndex, uint32_t page);
  uint32_t* getArrayInUInt32(char *name, uint32_t page);
  double*   getArrayInDouble(int32_t arrayIndex, uint32_t page);
  double*   getArrayInDouble(char *name, uint32_t page);
  char**    getArrayInString(int32_t arrayIndex, uint32_t page);
  char**    getArrayInString(char *name, uint32_t page);
  char*     getArrayName(int32_t index);
  char**    getArrayNames();
  char*     getArrayUnits(int32_t index);
  char*     getArrayUnits(char *name);
  char**    getAllArrayUnits();
  char*     getArraySymbol(int32_t index);
  char*     getArraySymbol(char *name);
  char**    getArraySymbols();
  char*     getArrayFormatString(int32_t index);
  char*     getArrayFormatString(char *name);
  char**    getArrayFormatStrings();
  int32_t   getArrayType(int32_t index);
  int32_t   getArrayType(char *name);
  int32_t*  getArrayTypes();
  char*     getArrayDescription(int32_t index);
  char*     getArrayDescription(char *name);
  char**    getArrayDescriptions();
  char*     getArrayGroupName(int32_t index);
  char*     getArrayGroupName(char *name);
  char**    getArrayGroupNames();
  uint32_t  getArrayFieldLength(int32_t index);
  uint32_t  getArrayFieldLength(char *name);
  uint32_t* getArrayFieldLengths();
  uint32_t  getArrayDimensions(int32_t index);
  uint32_t  getArrayDimensions(char *name);
  uint32_t* getAllArrayDimensions();
  void*     getInternalArray(int32_t columnIndex, uint32_t page);
  void*     getInternalArray(char *name, uint32_t page);

  int32_t   getColumnCount();
  int32_t*  getColumnInInt32(int32_t columnIndex, uint32_t page);
  int32_t*  getColumnInInt32(char *name, uint32_t page);
  uint32_t* getColumnInUInt32(int32_t columnIndex, uint32_t page);
  uint32_t* getColumnInUInt32(char *name, uint32_t page);
  double*   getColumnInDouble(int32_t columnIndex, uint32_t page);
  double*   getColumnInDouble(char *name, uint32_t page);
  char**    getColumnInString(int32_t columnIndex, uint32_t page);
  char**    getColumnInString(char *name, uint32_t page);
  int32_t   getColumnIndex(char *name);
  int32_t*  getColumnIndexes(char** names, int32_t number);
  char*     getColumnName(int32_t index);
  char**    getColumnNames();
  char*     getColumnUnits(int32_t index);
  char*     getColumnUnits(char *name);
  char**    getAllColumnUnits();
  char*     getColumnSymbol(int32_t index);
  char*     getColumnSymbol(char *name);
  char**    getColumnSymbols();
  char*     getColumnFormatString(int32_t index);
  char*     getColumnFormatString(char *name);
  char**    getColumnFormatStrings();
  int32_t   getColumnType(int32_t index);
  int32_t   getColumnType(char *name);
  int32_t*  getColumnTypes();
  char*     getColumnDescription(int32_t index);
  char*     getColumnDescription(char *name);
  char**    getColumnDescriptions();
  uint32_t  getColumnFieldLength(int32_t index);
  uint32_t  getColumnFieldLength(char *name);
  uint32_t* getColumnFieldLengths();
  void*     getInternalColumn(int32_t columnIndex, uint32_t page);
  void*     getInternalColumn(char *name, uint32_t page);



  int32_t   getDescription(char **text, char **contents);

  int32_t   getParameterCount();
  int32_t   getParameterIndex(char *name);
  int32_t*  getParameterIndexes(char** names, int32_t number);
  int32_t   getParameterInInt32(int32_t parameterIndex, uint32_t page);
  int32_t   getParameterInInt32(char *name, uint32_t page);
  uint32_t  getParameterInUInt32(int32_t parameterIndex, uint32_t page);
  uint32_t  getParameterInUInt32(char *name, uint32_t page);
  double    getParameterInDouble(int32_t parameterIndex, uint32_t page);
  double    getParameterInDouble(char *name, uint32_t page);
  char*     getParameterInString(int32_t parameterIndex, uint32_t page);
  char*     getParameterInString(char *name, uint32_t page);
  int32_t*  getParameterInInt32s(int32_t parameterIndex);
  int32_t*  getParameterInInt32s(char *name);
  uint32_t* getParameterInUInt32s(int32_t parameterIndex);
  uint32_t* getParameterInUInt32s(char *name);
  double*   getParameterInDoubles(int32_t parameterIndex);
  double*   getParameterInDoubles(char *name);
  char**    getParameterInStrings(int32_t parameterIndex);
  char**    getParameterInStrings(char *name);
  char*     getParameterName(int32_t index);
  char**    getParameterNames();
  char*     getParameterUnits(int32_t index);
  char*     getParameterUnits(char *name);
  char**    getAllParameterUnits();
  char*     getParameterSymbol(int32_t index);
  char*     getParameterSymbol(char *name);
  char**    getParameterSymbols();
  char*     getParameterFormatString(int32_t index);
  char*     getParameterFormatString(char *name);
  char**    getParameterFormatStrings();
  int32_t   getParameterType(int32_t index);
  int32_t   getParameterType(char *name);
  int32_t*  getParameterTypes();
  char*     getParameterDescription(int32_t index);
  char*     getParameterDescription(char *name);
  char**    getParameterDescriptions();
  char*     getParameterFixedValue(int32_t index);
  char*     getParameterFixedValue(char *name);
  char**    getParameterFixedValues();

  int32_t   processArray(char *s);
  int32_t   processColumn(char *s);
  int32_t   processData(char *s);
  int32_t   processDescription(char *s);
  int32_t   processParameter(char *s);

  int32_t   setArray(int32_t columnIndex, uint32_t page, int16_t *values, uint32_t elements);
  int32_t   setArray(char *name, uint32_t page, int16_t *values, uint32_t elements);
  int32_t   setArray(int32_t columnIndex, uint32_t page, uint16_t *values, uint32_t elements);
  int32_t   setArray(char *name, uint32_t page, uint16_t *values, uint32_t elements);
  int32_t   setArray(int32_t columnIndex, uint32_t page, int32_t *values, uint32_t elements);
  int32_t   setArray(char *name, uint32_t page, int32_t *values, uint32_t elements);
  int32_t   setArray(int32_t columnIndex, uint32_t page, uint32_t *values, uint32_t elements);
  int32_t   setArray(char *name, uint32_t page, uint32_t *values, uint32_t elements);
  int32_t   setArray(int32_t columnIndex, uint32_t page, float *values, uint32_t elements);
  int32_t   setArray(char *name, uint32_t page, float *values, uint32_t elements);
  int32_t   setArray(int32_t columnIndex, uint32_t page, double *values, uint32_t elements);
  int32_t   setArray(char *name, uint32_t page, double *values, uint32_t elements);
  int32_t   setArray(int32_t columnIndex, uint32_t page, char **values, uint32_t elements);
  int32_t   setArray(char *name, uint32_t page, char **values, uint32_t elements);
  int32_t   setArray(int32_t columnIndex, uint32_t page, char *values, uint32_t elements);
  int32_t   setArray(char *name, uint32_t page, char *values, uint32_t elements);
  void      setArraySymbol(int32_t index, char *symbol);
  void      setArraySymbol(char *name, char *symbol);
  void      setArrayUnits(int32_t index, char *units);
  void      setArrayUnits(char *name, char *units);
  void      setArrayDescription(int32_t index, char *description);
  void      setArrayDescription(char *name, char *description);
  void      setArrayFormatString(int32_t index, char *format_string);
  void      setArrayFormatString(char *name, char *format_string);
  void      setArrayGroupName(int32_t index, char *group_name);
  void      setArrayGroupName(char *name, char *group_name);
  void      setArrayFieldLength(int32_t index, uint32_t field_length);
  void      setArrayFieldLength(char *name, uint32_t field_length);
  void      setArrayDimensions(int32_t index, uint32_t dimensions);
  void      setArrayDimensions(char *name, uint32_t dimensions);
  void      setArrayDim(uint32_t page, int32_t index, uint32_t *dimensions);
  void      setArrayDim(uint32_t page, char *name, uint32_t *dimensions);


  int32_t   setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, int16_t *values, uint32_t rows);
  int32_t   setColumn(char *name, uint32_t page, uint32_t startRow, int16_t *values, uint32_t rows);
  int32_t   setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, uint16_t *values, uint32_t rows);
  int32_t   setColumn(char *name, uint32_t page, uint32_t startRow, uint16_t *values, uint32_t rows);
  int32_t   setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, int32_t *values, uint32_t rows);
  int32_t   setColumn(char *name, uint32_t page, uint32_t startRow, int32_t *values, uint32_t rows);
  int32_t   setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, uint32_t *values, uint32_t rows);
  int32_t   setColumn(char *name, uint32_t page, uint32_t startRow, uint32_t *values, uint32_t rows);
  int32_t   setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, float *values, uint32_t rows);
  int32_t   setColumn(char *name, uint32_t page, uint32_t startRow, float *values, uint32_t rows);
  int32_t   setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, double *values, uint32_t rows);
  int32_t   setColumn(char *name, uint32_t page, uint32_t startRow, double *values, uint32_t rows);
  int32_t   setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, char **values, uint32_t rows);
  int32_t   setColumn(char *name, uint32_t page, uint32_t startRow, char **values, uint32_t rows);
  int32_t   setColumn(int32_t columnIndex, uint32_t page, uint32_t startRow, char *values, uint32_t rows);
  int32_t   setColumn(char *name, uint32_t page, uint32_t startRow, char *values, uint32_t rows);
  void      setColumnSymbol(int32_t index, char *symbol);
  void      setColumnSymbol(char *name, char *symbol);
  void      setColumnUnits(int32_t index, char *units);
  void      setColumnUnits(char *name, char *units);
  void      setColumnDescription(int32_t index, char *description);
  void      setColumnDescription(char *name, char *description);
  void      setColumnFormatString(int32_t index, char *format_string);
  void      setColumnFormatString(char *name, char *format_string);
  void      setColumnFieldLength(int32_t index, uint32_t field_length);
  void      setColumnFieldLength(char *name, uint32_t field_length);

  int32_t   setParameter(uint32_t page, int32_t parameterIndex, int32_t value);
  int32_t   setParameter(uint32_t page, int32_t parameterIndex, uint32_t value);
  int32_t   setParameter(uint32_t page, int32_t parameterIndex, double value);
  int32_t   setParameter(uint32_t page, int32_t parameterIndex, char *value);
  int32_t   setParameter(uint32_t page, char *name, int32_t value);
  int32_t   setParameter(uint32_t page, char *name, uint32_t value);
  int32_t   setParameter(uint32_t page, char *name, double value);
  int32_t   setParameter(uint32_t page, char *name, char *value);
  int32_t   setParameter(uint32_t startpage, uint32_t pages, int32_t parameterIndex, int32_t *value);
  int32_t   setParameter(uint32_t startpage, uint32_t pages, char *name, int32_t *value);
  int32_t   setParameter(uint32_t startpage, uint32_t pages, int32_t parameterIndex, uint32_t *value);
  int32_t   setParameter(uint32_t startpage, uint32_t pages, char *name, uint32_t *value);
  int32_t   setParameter(uint32_t startpage, uint32_t pages, int32_t parameterIndex, double *value);
  int32_t   setParameter(uint32_t startpage, uint32_t pages, char *name, double *value);
  int32_t   setParameter(uint32_t startpage, uint32_t pages, int32_t parameterIndex, char **value);
  int32_t   setParameter(uint32_t startpage, uint32_t pages, char *name, char **value);
  void      setParameterSymbol(int32_t index, char *symbol);
  void      setParameterSymbol(char *name, char *symbol);
  void      setParameterUnits(int32_t index, char *units);
  void      setParameterUnits(char *name, char *units);
  void      setParameterDescription(int32_t index, char *description);
  void      setParameterDescription(char *name, char *description);
  void      setParameterFormatString(int32_t index, char *format_string);
  void      setParameterFormatString(char *name, char *format_string);
  void      setParameterFixedValue(int32_t index, char *fixed_value);
  void      setParameterFixedValue(char *name, char *fixed_value);

  char*     getTagValuePair(char *ptr, char **tag, char **value);


  char           *fileName;
  bool            bigEndian;
  bool            asciiFile;
  bool            noRowCount;
  int32_t         linesPerRow;
  int32_t         additionalHeaderLines;
  bool            isURL;
  char           *descriptionText;
  char           *descriptionContents;
  int32_t         arrayCount;
  int32_t         columnCount;
  int32_t         parameterCount;
  SDDSArray     **arrayObject;
  SDDSColumn    **columnObject;
  SDDSParameter **parameterObject;
  FILE           *fp;
  gzFile         *gzfp;
  bool            gzipFile;
  int32_t         layoutVersion;
  int32_t         defaultIOBufferSize;
  int32_t         recoveryPossible;
  bool            columnMajorOrder;

 private:

 protected:
  char    **error_description;
  int32_t   n_errors;
};

/*epicsShareFuncSDDS3 extern int32_t   SDDS_printNamelistField(FILE *fp, char *name, char *value);*/
int32_t   SDDS_printNamelistField(FILE *fp, char *name, char *value);
int32_t   SDDS_printNamelistField(voidp *gzfp, char *name, char *value);
int32_t   SDDS_getNamelist(char *buffer, int32_t buflen, FILE *fp);
int32_t   SDDS_getNamelist(char *buffer, int32_t buflen, voidp *gzfp);
char*     SDDS_fgetsSkipComments(char *s, int32_t slen, FILE *fp, char skip_char);
char*     SDDS_fgetsSkipComments(char *s, int32_t slen, voidp *gzfp, char skip_char);
char*     SDDS_fgetsSkipCommentsResize(char **s, int32_t *slen, FILE *fp, char skip_char);
char*     SDDS_fgetsSkipCommentsResize(char **s, int32_t *slen, voidp *gzfp, char skip_char);
int32_t   SDDS_bufferedRead(void *target, size_t targetSize, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);
int32_t   SDDS_bufferedRead(void *target, size_t targetSize, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);
int32_t   SDDS_bufferedWrite(void *target, size_t targetSize, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);
int32_t   SDDS_bufferedWrite(void *target, size_t targetSize, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);
int32_t   SDDS_flushBuffer(FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);
int32_t   SDDS_flushBuffer(voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);
int32_t   SDDS_writeBinaryString(char *string, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);
int32_t   SDDS_writeBinaryString(char *string, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);
int32_t   SDDS_readBinaryString(char **string, FILE *fp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);
int32_t   SDDS_readBinaryString(char **string, voidp *gzfp, SDDS_FILEBUFFER *fBuffer, void *sddsfile);


#endif
#ifdef __cplusplus
extern "C" {
#endif

#define SDDS_MAXLINE 1024
#define SDDS_VERSION 3

#define SDDS_BINARY 0x0001UL
#define SDDS_ASCII 0x0002UL

#define SDDSFILE_NEW_DEFAULT       0x0001UL
#define SDDSFILE_NEW_WITH_FILENAME 0x0002UL
#define SDDSFILE_NEW_WITH_MODE     0x0004UL

#define SDDSFILE_DEFINEARRAY_DEFAULT         0x0001UL
#define SDDSFILE_DEFINEARRAY_WITH_STRINGTYPE 0x0002UL
#define SDDSFILE_DEFINEARRAY_EXTENSIVE       0x0004UL

#define SDDSFILE_DEFINECOLUMN_DEFAULT         0x0001UL
#define SDDSFILE_DEFINECOLUMN_WITH_STRINGTYPE 0x0002UL
#define SDDSFILE_DEFINECOLUMN_EXTENSIVE       0x0004UL

#define SDDSFILE_DEFINEPARAMETER_DEFAULT         0x0001UL
#define SDDSFILE_DEFINEPARAMETER_WITH_STRINGTYPE 0x0002UL
#define SDDSFILE_DEFINEPARAMETER_EXTENSIVE       0x0004UL

#define SDDSFILE_SETARRAY_WITH_INDEX   0x0001UL
#define SDDSFILE_SETARRAY_WITH_NAME    0x0002UL
#define SDDSFILE_SETARRAY_WITH_INT16      0x0004UL
#define SDDSFILE_SETARRAY_WITH_UINT16     0x0008UL
#define SDDSFILE_SETARRAY_WITH_INT32      0x0010UL
#define SDDSFILE_SETARRAY_WITH_UINT32     0x0020UL
#define SDDSFILE_SETARRAY_WITH_FLOAT      0x0040UL
#define SDDSFILE_SETARRAY_WITH_REAL32     0x0040UL
#define SDDSFILE_SETARRAY_WITH_DOUBLE     0x0080UL
#define SDDSFILE_SETARRAY_WITH_REAL64     0x0080UL
#define SDDSFILE_SETARRAY_WITH_STRING     0x0100UL
#define SDDSFILE_SETARRAY_WITH_CHARACTER  0x0200UL

#define SDDSFILE_SETCOLUMN_WITH_INDEX      0x0001UL
#define SDDSFILE_SETCOLUMN_WITH_NAME       0x0002UL
#define SDDSFILE_SETCOLUMN_WITH_INT16      0x0004UL
#define SDDSFILE_SETCOLUMN_WITH_UINT16     0x0008UL
#define SDDSFILE_SETCOLUMN_WITH_INT32      0x0010UL
#define SDDSFILE_SETCOLUMN_WITH_UINT32     0x0020UL
#define SDDSFILE_SETCOLUMN_WITH_FLOAT      0x0040UL
#define SDDSFILE_SETCOLUMN_WITH_REAL32     0x0040UL
#define SDDSFILE_SETCOLUMN_WITH_DOUBLE     0x0080UL
#define SDDSFILE_SETCOLUMN_WITH_REAL64     0x0080UL
#define SDDSFILE_SETCOLUMN_WITH_STRING     0x0100UL
#define SDDSFILE_SETCOLUMN_WITH_CHARACTER  0x0200UL

#define SDDSFILE_SETPARAMETER_WITH_INDEX   0x0001UL
#define SDDSFILE_SETPARAMETER_WITH_NAME    0x0002UL
#define SDDSFILE_SETPARAMETER_WITH_INT     0x0004UL
#define SDDSFILE_SETPARAMETER_WITH_UINT    0x0008UL
#define SDDSFILE_SETPARAMETER_WITH_FLOAT   0x0010UL
#define SDDSFILE_SETPARAMETER_WITH_STRING  0x0020UL

#define SDDSFILE_GETARRAY_WITH_INDEX   0x0001UL
#define SDDSFILE_GETARRAY_WITH_NAME    0x0002UL

#define SDDSFILE_GETCOLUMN_WITH_INDEX   0x0001UL
#define SDDSFILE_GETCOLUMN_WITH_NAME    0x0002UL

#define SDDSFILE_GETPARAMETER_WITH_INDEX   0x0001UL
#define SDDSFILE_GETPARAMETER_WITH_NAME    0x0002UL

#define SDDS_BIGENDIAN_SEEN      0x0001UL
#define SDDS_LITTLEENDIAN_SEEN   0x0002UL
#define SDDS_FIXED_ROWCOUNT_SEEN 0x0004UL

#define SDDS_VERBOSE_PrintErrors 0x0001UL
#define SDDS_EXIT_PrintErrors 0x0002UL

#define SDDS_ALLOW_ANY_NAME 0x0001UL
#define SDDS_ALLOW_V15_NAME 0x0002UL

#define SDDS_DOUBLE    1
#define SDDS_REAL64    1
#define SDDS_FLOAT     2
#define SDDS_REAL32    2
#define SDDS_LONG      3
#define SDDS_INT32     3
#define SDDS_ULONG     4
#define SDDS_UINT32    4
#define SDDS_SHORT     5
#define SDDS_INT16     5
#define SDDS_USHORT    6
#define SDDS_UINT16    6
#define SDDS_STRING    7
#define SDDS_CHARACTER 8
#define SDDS_NUM_TYPES 8
#define SDDS_INTEGER_TYPE(type) ((type)==SDDS_INT16 || (type)==SDDS_UINT16 || (type)==SDDS_INT32 || (type)==SDDS_UINT32)
#define SDDS_FLOATING_TYPE(type) ((type)==SDDS_REAL32 || (type)==SDDS_REAL64)


  void*     SDDSFile_new(int32_t mode, ...);
  void      SDDSFile_delete(void *sddsfile);
  int32_t   SDDSFile_copyArrayDefinitions(void *sddsfile, void *sdds_source);
  int32_t   SDDSFile_copyArrayDefinition(void *sddsfile, void *sdds_source, char *orig_array_name, char *new_array_name);
  int32_t   SDDSFile_copyColumnDefinitions(void *sddsfile, void *sdds_source);
  int32_t   SDDSFile_copyColumnDefinition(void *sddsfile, void *sdds_source, char *orig_column_name, char *new_column_name);
  int32_t   SDDSFile_copyParameterDefinitions(void *sddsfile, void *sdds_source);
  int32_t   SDDSFile_copyParameterDefinition(void *sddsfile, void *sdds_source, char *orig_parameter_name, char *new_parameter_name);
  int32_t   SDDSFile_defineArray(void *sddsfile, int32_t mode, ...);
  int32_t   SDDSFile_defineColumn(void *sddsfile, int32_t mode, ...);
  int32_t   SDDSFile_defineParameter(void *sddsfile, int32_t mode, ...);
  void      SDDSFile_freePage(void *sddsfile);
  int32_t   SDDSFile_getArrayCount(void *sddsfile);
  char*     SDDSFile_getArrayName(void *sddsfile, int32_t index);
  char**    SDDSFile_getArrayNames(void *sddsfile);
  char*     SDDSFile_getArrayUnits(void *sddsfile, int32_t index);
  char**    SDDSFile_getAllArrayUnits(void *sddsfile);
  char*     SDDSFile_getArraySymbol(void *sddsfile, int32_t index);
  char**    SDDSFile_getArraySymbols(void *sddsfile);
  char*     SDDSFile_getArrayFormatString(void *sddsfile, int32_t index);
  char**    SDDSFile_getArrayFormatStrings(void *sddsfile);
  int32_t   SDDSFile_getArrayType(void *sddsfile, int32_t index);
  int32_t*  SDDSFile_getArrayTypes(void *sddsfile);
  char*     SDDSFile_getArrayDescription(void *sddsfile, int32_t index);
  char**    SDDSFile_getArrayDescriptions(void *sddsfile);
  char*     SDDSFile_getArrayGroupName(void *sddsfile, int32_t index);
  char**    SDDSFile_getArrayGroupNames(void *sddsfile);
  uint32_t  SDDSFile_getArrayFieldLength(void *sddsfile, int32_t index);
  uint32_t* SDDSFile_getArrayFieldLengths(void *sddsfile);
  uint32_t  SDDSFile_getArrayDimensions(void *sddsfile, int32_t index);
  uint32_t* SDDSFile_getAllArrayDimensions(void *sddsfile);
  int32_t   SDDSFile_getArrayIndex(void *sddsfile, char *name);
  int32_t*  SDDSFile_getArrayIndexes(void *sddsfile, char** names, int32_t number);
  int32_t*  SDDSFile_getArrayInInt32(void *sddsfile, uint32_t page, int32_t mode, ...);
  uint32_t* SDDSFile_getArrayInUInt32(void *sddsfile, uint32_t page, int32_t mode, ...);
  double*   SDDSFile_getArrayInDouble(void *sddsfile, uint32_t page, int32_t mode, ...);
  char**    SDDSFile_getArrayInString(void *sddsfile, uint32_t page, int32_t mode, ...);
  int32_t   SDDSFile_getColumnCount(void *sddsfile);
  int32_t*  SDDSFile_getColumnInInt32(void *sddsfile, uint32_t page, int32_t mode, ...);
  uint32_t* SDDSFile_getColumnInUInt32(void *sddsfile, uint32_t page, int32_t mode, ...);
  double*   SDDSFile_getColumnInDouble(void *sddsfile, uint32_t page, int32_t mode, ...);
  char**    SDDSFile_getColumnInString(void *sddsfile, uint32_t page, int32_t mode, ...);
  char*     SDDSFile_getColumnName(void *sddsfile, int32_t index);
  char**    SDDSFile_getColumnNames(void *sddsfile);
  char*     SDDSFile_getColumnUnits(void *sddsfile, int32_t index);
  char**    SDDSFile_getAllColumnUnits(void *sddsfile);
  char*     SDDSFile_getColumnSymbol(void *sddsfile, int32_t index);
  char**    SDDSFile_getColumnSymbols(void *sddsfile);
  char*     SDDSFile_getColumnFormatString(void *sddsfile, int32_t index);
  char**    SDDSFile_getColumnFormatStrings(void *sddsfile);
  int32_t   SDDSFile_getColumnType(void *sddsfile, int32_t index);
  int32_t*  SDDSFile_getColumnTypes(void *sddsfile);
  char*     SDDSFile_getColumnDescription(void *sddsfile, int32_t index);
  char**    SDDSFile_getColumnDescriptions(void *sddsfile);
  uint32_t  SDDSFile_getColumnFieldLength(void *sddsfile, int32_t index);
  uint32_t* SDDSFile_getColumnFieldLengths(void *sddsfile);
  int32_t   SDDSFile_getColumnIndex(void *sddsfile, char *name);
  int32_t*  SDDSFile_getColumnIndexes(void *sddsfile, char** names, int32_t number);
  uint32_t  SDDSFile_getDataMode(void *sddsfile);
  void      SDDSFile_setDataMode(void *sddsfile, uint32_t mode);
  int32_t   SDDSFile_getDescription(void *sddsfile, char **text, char **contents);
  void*     SDDSFile_getInternalArray(void *sddsfile, uint32_t page, int32_t mode, ...);
  void*     SDDSFile_getInternalColumn(void *sddsfile, uint32_t page, int32_t mode, ...);
  int32_t   SDDSFile_getParameterCount(void *sddsfile);
  char*     SDDSFile_getParameterName(void *sddsfile, int32_t index);
  char**    SDDSFile_getParameterNames(void *sddsfile);
  char*     SDDSFile_getParameterUnits(void *sddsfile, int32_t index);
  char**    SDDSFile_getAllParameterUnits(void *sddsfile);
  char*     SDDSFile_getParameterSymbol(void *sddsfile, int32_t index);
  char**    SDDSFile_getParameterSymbols(void *sddsfile);
  char*     SDDSFile_getParameterFormatString(void *sddsfile, int32_t index);
  char**    SDDSFile_getParameterFormatStrings(void *sddsfile);
  int32_t   SDDSFile_getParameterType(void *sddsfile, int32_t index);
  int32_t*  SDDSFile_getParameterTypes(void *sddsfile);
  char*     SDDSFile_getParameterDescription(void *sddsfile, int32_t index);
  char**    SDDSFile_getParameterDescriptions(void *sddsfile);
  char*     SDDSFile_getParameterFixedValue(void *sddsfile, int32_t index);
  char**    SDDSFile_getParameterFixedValues(void *sddsfile);
  int32_t   SDDSFile_getParameterIndex(void *sddsfile, char *name);
  int32_t*  SDDSFile_getParameterIndexes(void *sddsfile, char** names, int32_t number);
  int32_t   SDDSFile_getParameterInInt32(void *sddsfile, uint32_t page, int32_t mode, ...);
  int32_t*  SDDSFile_getParameterInInt32s(void *sddsfile, int32_t mode, ...);
  uint32_t  SDDSFile_getParameterInUInt32(void *sddsfile, uint32_t page, int32_t mode, ...);
  uint32_t* SDDSFile_getParameterInUInt32s(void *sddsfile, int32_t mode, ...);
  double    SDDSFile_getParameterInDouble(void *sddsfile, uint32_t page, int32_t mode, ...);
  double*   SDDSFile_getParameterInDoubles(void *sddsfile, int32_t mode, ...);
  char*     SDDSFile_getParameterInString(void *sddsfile, uint32_t page, int32_t mode, ...);
  char**    SDDSFile_getParameterInStrings(void *sddsfile, int32_t mode, ...);
  void      SDDSFile_setAsciiMode(void *sddsfile);
  void      SDDSFile_setBinaryMode(void *sddsfile);
  void      SDDSFile_setArrayDescription(void *sddsfile, int32_t index, char *description);
  void      SDDSFile_setArrayGroupName(void *sddsfile, int32_t index, char *group_name);
  void      SDDSFile_setArrayFormatString(void *sddsfile, int32_t index, char *format_string);
  void      SDDSFile_setArraySymbol(void *sddsfile, int32_t index, char *symbol);
  void      SDDSFile_setArrayUnits(void *sddsfile, int32_t index, char *units);
  void      SDDSFile_setArrayFieldLength(void *sddsfile, int32_t index, uint32_t field_length);
  void      SDDSFile_setArrayDimensions(void *sddsfile, int32_t index, uint32_t dimensions);
  void      SDDSFile_setArrayDim(void *sddsfile, uint32_t page, uint32_t *dimensions, int32_t mode);
  int32_t   SDDSFile_setArray(void *sddsfile, uint32_t page, uint32_t elements, int32_t mode, ...);
  int32_t   SDDSFile_setColumn(void *sddsfile, uint32_t page, uint32_t startRow, uint32_t rows, int32_t mode, ...);
  void      SDDSFile_setColumnDescription(void *sddsfile, int32_t index, char *description);
  void      SDDSFile_setColumnFieldLength(void *sddsfile, int32_t index, uint32_t field_length);
  void      SDDSFile_setColumnFormatString(void *sddsfile, int32_t index, char *format_string);
  void      SDDSFile_setColumnSymbol(void *sddsfile, int32_t index, char *symbol);
  void      SDDSFile_setColumnUnits(void *sddsfile, int32_t index, char *units);
  void      SDDSFile_setDescription(void *sddsfile, char *text, char *contents);
  void      SDDSFile_setError(void *sddsfile, char *error_text);
  void      SDDSFile_setFileName(void *sddsfile, char *filename);
  void      SDDSFile_setLayoutVersion(void *sddsfile, int32_t version);
  void      SDDSFile_setMode(void *sddsfile, int32_t binary);
  void      SDDSFile_setNonNativeEndian(void *sddsfile);
  void      SDDSFile_setNoRowCount(void *sddsfile);
  void      SDDSFile_setParameterDescription(void *sddsfile, int32_t index, char *description);
  void      SDDSFile_setParameterFixedValue(void *sddsfile, int32_t index, char *fixed_value);
  void      SDDSFile_setParameterFormatString(void *sddsfile, int32_t index, char *format_string);
  void      SDDSFile_setParameterSymbol(void *sddsfile, int32_t index, char *symbol);
  void      SDDSFile_setParameterUnits(void *sddsfile, int32_t index, char *units);
  int32_t   SDDSFile_setParameter(void *sddsfile, uint32_t page, int32_t mode, ...);
  void      SDDSFile_printErrors(void *sddsfile);
  int32_t   SDDSFile_checkForErrors(void *sddsfile);
  void      SDDSFile_clearErrors(void *sddsfile);
  int32_t   SDDSFile_openInputFile(void *sddsfile);
  int32_t   SDDSFile_openOutputFile(void *sddsfile);
  int32_t   SDDSFile_closeFile(void *sddsfile);
  int32_t   SDDSFile_initializeInput(void *sddsfile, char *filename);
  int32_t   SDDSFile_initializeOutput(void *sddsfile, int32_t data_mode, int32_t lines_per_row, 
                                      char *description, char *contents, char *filename);
  int32_t   SDDSFile_readFile(void *sddsfile);
  int32_t   SDDSFile_readLayout(void *sddsfile);
  int32_t   SDDSFile_readPage(void *sddsfile);
  int32_t   SDDSFile_readPages(void *sddsfile);
  int32_t   SDDSFile_readRecoveryPossible(void *sddsfile);
  int32_t   SDDSFile_writeFile(void *sddsfile);
  int32_t   SDDSFile_writeLayout(void *sddsfile);
  int32_t   SDDSFile_writePages(void *sddsfile);
  int32_t   SDDSFile_writePage(void *sddsfile, uint32_t page);
  uint32_t  SDDSFile_pageCount(void *sddsfile);
  uint32_t  SDDSFile_rowCount(void *sddsfile, uint32_t page);
  void      SDDSFile_setColumnMajorOrder(void *sddsfile);
  void      SDDSFile_setRowMajorOrder(void *sddsfile);


  int32_t   SDDS_isBigEndianMachine();
  int32_t   SDDS_identifyType(char* typeName);
  int32_t   SDDS_verifyPrintfFormat(char *string, int32_t type);
  int32_t   SDDS_isValidType(int32_t type);
  int32_t   SDDS_isValidName(char *name);
  int32_t   SDDS_fileIsLocked(char *filename);
  int32_t   SDDS_lockFile(FILE *fp, char *filename, char *caller);
  void     *SDDS_calloc(size_t nelem, size_t elem_size);
  void     *SDDS_malloc(size_t size);
  void      SDDS_free(void *mem);
  void     *SDDS_realloc(void *old_ptr, size_t new_size);
  char     *SDDS_blankToNull(char *string);
  int32_t   SDDS_stringIsBlank(char *string);
  char     *SDDS_getTypeName(int32_t type);
  char     *SDDS_getVer1TypeName(int32_t type);
  char     *SDDS_getVer2TypeName(int32_t type);
  int32_t   SDDS_hasWhitespace(char *string);
  void      SDDS_cutOutComments(char *s, char cc);
  void      SDDS_parseSpecialComments(char *s);
  int32_t   SDDS_getToken(char **st, int32_t *strlength, char *buffer, int32_t buflen);
  int32_t   SDDS_isQuoted(char *string, char *position, char quotation_mark);
  uint32_t  SDDS_getSpecialCommentsModes();
  void      SDDS_resetSpecialCommentsModes();
  char     *SDDS_prepareToParseTagValuePairs(char *s); 
  int32_t   SDDS_copyString(char **target, char *source);
  void      SDDS_interpretEscapes(char *s);
  void      SDDS_setNameValidityFlags(uint32_t flags);
  void      SDDS_bomb(char *message);
  void      SDDS_registerProgramName(const char *name);
  char     *SDDS_getProgramName();
  int32_t   SDDS_freeStringArray(char **string, int32_t strings);
  void      SDDS_warning(char *message);



#ifdef __cplusplus
}

#endif

#endif /* SDDS3_ENABLE_UNMODERNIZED_API */
