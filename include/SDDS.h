/**
 * @file SDDS.h
 * @brief SDDS (Self Describing Data Set) Data Types Definitions and Function Prototypes
 *
 * This header file defines the data types, macros, structures, and function prototypes
 * used for handling SDDS files. SDDS is a file protocol designed to store and transfer
 * scientific data efficiently.
 *
 * @copyright 
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license This file is distributed under the terms of the Software License Agreement
 *          found in the file LICENSE included with this distribution.
 *
 * @authors
 *  M. Borland,
 *  C. Saunders,
 *  R. Soliday,
 *  H. Shang
 *
 */

#if !defined(_SDDS_)
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#define SDDS_READMODE 1
#define SDDS_WRITEMODE 2
#define SDDS_MEMMODE 3

#define SDDS_MPI_READ_ONLY 0x0001UL
#define SDDS_MPI_WRITE_ONLY 0x0002UL
#define SDDS_MPI_READ_WRITE  0x0004UL
#define SDDS_MPI_STRING_COLUMN_LEN 16

#define SDDS_COLUMN_MAJOR_ORDER 0x0001UL
#define SDDS_ROW_MAJOR_ORDER    0x0002UL

#if SDDS_MPI_IO
#include "mpi.h"
#include "mdb.h"

extern char SDDS_mpi_error_str[MPI_MAX_ERROR_STRING];
extern int32_t SDDS_mpi_error_str_len;
extern char *SDDS_MPI_FILE_TYPE[];

typedef struct {
  MPI_File	        MPI_file;	/*MPIO file handle			*/
  MPI_Comm	        comm;		/*communicator				*/
  MPI_Info	        File_info;	/*file information			*/
  int32_t               myid;           /* This process's rank                  */
  int32_t               n_processors;   /* Total number of processes        */
  MPI_Offset            file_offset, file_size, column_offset;  /* number of bytes in one row for row major order*/
  short                 collective_io;
  int32_t               n_page;         /* index of current page*/
  int64_t               n_rows;         /* number of rows that current processor holds */
  int64_t               total_rows;     /* the total number of rows that all processor hold*/
  int32_t               end_of_file;    /* flag for end of MPI_file */
  int32_t               master_read;   /*determine if master processor read the page data or not*/
  int64_t               start_row, end_row; /* the start row and end row that current processor's data that is going to be written to output or read from input */
  FILE *fpdeb;
} MPI_DATASET;
#endif

#if defined(zLib)
#include "zlib.h"
#endif

#ifdef __cplusplus 
extern "C" {
#endif


#define _SDDS_ 1

#define SDDS_VERSION 5

#ifndef SVN_VERSION
#define SVN_VERSION "unknown"
#endif

#include "SDDStypes.h"
#if defined(_WIN32) && !defined(_MINGW)
  typedef __int32 int32_t;
  typedef unsigned __int32 uint32_t;
  typedef __int64 int64_t;
  typedef unsigned __int64 uint64_t;
#ifndef PRId32
#define PRId32 "ld"
#define SCNd32 "ld"
#define PRIu32 "lu"
#define SCNu32 "lu"
#define PRId64 "I64d"
#define SCNd64 "I64d"
#define PRIu64 "I64u"
#define SCNu64 "I64u"
#endif
#if !defined(INT32_MAX)
#define INT32_MAX 2147483647i32
#endif
#if !defined(INT32_MIN)
#define INT32_MIN (-2147483647i32 - 1)
#endif
#if !defined(INT64_MAX)
#define INT64_MAX 9223372036854775807i64
#endif
#else
#if defined(vxWorks)
#define PRId32 "ld"
#define SCNd32 "ld"
#define PRIu32 "lu"
#define SCNu32 "lu"
#define PRId64 "lld"
#define SCNd64 "lld"
#define PRIu64 "llu"
#define SCNu64 "llu"
#define INT32_MAX (2147483647)
#define INT32_MAX (-2147483647 - 1)
#else
#include <inttypes.h>
#endif
#endif

#undef epicsShareFuncSDDS
#undef epicsShareExtern
#if (defined(_WIN32) && !defined(__CYGWIN32__)) || (defined(__BORLANDC__) && defined(__linux__))
#if defined(EXPORT_SDDS)
#define epicsShareFuncSDDS  __declspec(dllexport)
#define epicsShareExtern extern __declspec(dllexport)
#else
#define epicsShareFuncSDDS
#define epicsShareExtern extern __declspec(dllimport)
#endif
#else
#define epicsShareFuncSDDS
#define epicsShareExtern extern
#endif


#define RW_ASSOCIATES 0

#define SDDS_BINARY 1
#define SDDS_ASCII  2
#define SDDS_PARALLEL 3
#define SDDS_NUM_DATA_MODES 2

  /*
    extern char *SDDS_type_name[SDDS_NUM_TYPES];
    extern int32_t SDDS_type_size[SDDS_NUM_TYPES];
    extern char *SDDS_data_mode[SDDS_NUM_DATA_MODES];
  */

  epicsShareExtern char *SDDS_type_name[SDDS_NUM_TYPES];
  epicsShareExtern int32_t SDDS_type_size[SDDS_NUM_TYPES];
  extern char *SDDS_data_mode[SDDS_NUM_DATA_MODES];

  /* this shouldn't be changed without changing buffer sizes in namelist routines: */
#define SDDS_MAXLINE 1024

#define SDDS_PRINT_BUFLEN 16834

#define SDDS_NORMAL_DEFINITION 0
#define SDDS_WRITEONLY_DEFINITION 1

  typedef struct {
    char *name, *text; 
  } SDDS_DEFINITION;

  typedef struct {
    char *name, *symbol, *units, *description, *format_string;
    int32_t type;
    char *fixed_value;
    /* these are used internally and are not set by the user: */
    int32_t definition_mode, memory_number;
  } PARAMETER_DEFINITION;
#define SDDS_PARAMETER_FIELDS 7

  typedef struct {
    char *name, *symbol, *units, *description, *format_string;
    int32_t type, field_length;
    /* these are used internally and are not set by the user: */
    int32_t definition_mode, memory_number, pointer_number;
  } COLUMN_DEFINITION;
#define SDDS_COLUMN_FIELDS 7

  typedef struct {
    char *name, *symbol, *units, *description, *format_string, *group_name;
    int32_t type, field_length, dimensions;
  } ARRAY_DEFINITION;
#define SDDS_ARRAY_FIELDS 9

  typedef struct {
    char *name, *filename, *path, *description, *contents;
    int32_t sdds;
  } ASSOCIATE_DEFINITION;
#define SDDS_ASSOCIATE_FIELDS 6

  typedef struct {
    int32_t index;
    char *name;
  } SORTED_INDEX;
  int SDDS_CompareIndexedNames(const void *s1, const void *s2);
  int SDDS_CompareIndexedNamesPtr(const void *s1, const void *s2);

  typedef struct {
    int32_t mode, lines_per_row, no_row_counts, fixed_row_count, fixed_row_increment, fsync_data;
    int32_t additional_header_lines, endian;
    short column_major, column_memory_mode;
  } DATA_MODE;

#if !defined(zLib)
  typedef void *voidp;
  typedef voidp gzFile;
#endif

#if !defined(LZMA_BUF_SIZE)
#if defined(_WIN32)
#include <stdint.h>
#endif
#include <lzma.h>
#define LZMA_BUF_SIZE 40960
  struct lzmafile {
    lzma_stream str;        /* codec stream descriptor */
    FILE *fp;               /* backing file descriptor */
    char mode;              /* access mode ('r' or 'w') */
    unsigned char rdbuf[LZMA_BUF_SIZE];  /* read buffer used by lzmaRead */
  };
#endif

  typedef struct {
    char *name;
    int32_t value;
  } SDDS_ENUM_PAIR;


#define SDDS_DESCRIPTION_FIELDS 2
#define SDDS_DATA_FIELDS 7
#define SDDS_INCLUDE_FIELDS 1

  typedef struct {
    char *name;
    int32_t offset;
    int32_t type;
    SDDS_ENUM_PAIR *enumPair;
  } SDDS_FIELD_INFORMATION ;

  extern SDDS_FIELD_INFORMATION SDDS_ArrayFieldInformation[SDDS_ARRAY_FIELDS];
  extern SDDS_FIELD_INFORMATION SDDS_ColumnFieldInformation[SDDS_COLUMN_FIELDS];
  extern SDDS_FIELD_INFORMATION SDDS_ParameterFieldInformation[SDDS_PARAMETER_FIELDS];
  extern SDDS_FIELD_INFORMATION SDDS_AssociateFieldInformation[SDDS_ASSOCIATE_FIELDS];
  extern SDDS_FIELD_INFORMATION SDDS_DescriptionFieldInformation[SDDS_DESCRIPTION_FIELDS];
  extern SDDS_FIELD_INFORMATION SDDS_IncludeFieldInformation[SDDS_INCLUDE_FIELDS];
  extern SDDS_FIELD_INFORMATION SDDS_DataFieldInformation[SDDS_DATA_FIELDS];

  typedef struct {
    int32_t n_columns, n_parameters, n_associates, n_arrays;

    char *description;
    char *contents;
    int32_t version;
    short layout_written;

    DATA_MODE data_mode;
    COLUMN_DEFINITION *column_definition;
    PARAMETER_DEFINITION *parameter_definition;
    ARRAY_DEFINITION *array_definition;
    ASSOCIATE_DEFINITION *associate_definition;
    SORTED_INDEX **column_index, **parameter_index, **array_index;

    char *filename;
    FILE *fp;
    gzFile gzfp;
    struct lzmafile* lzmafp;
    short disconnected;
    short gzipFile;
    short lzmaFile;
    short popenUsed;
    uint32_t byteOrderDeclared;

    /*SDDS_ReadLayout*/
    char s[SDDS_MAXLINE];
    int32_t depth;
    int32_t data_command_seen;

    uint32_t commentFlags;
  } SDDS_LAYOUT;

  typedef struct {
    ARRAY_DEFINITION *definition;
    int32_t *dimension, elements;
    /* Address of array of data values, stored contiguously.
     * For STRING data, the "data values" are actually the addresses of the individual strings.
     */
    void *data;
    void *pointer;
  } SDDS_ARRAY;

  typedef struct {
    char *data, *buffer;
    int64_t bytesLeft;
    int64_t bufferSize;
  } SDDS_FILEBUFFER ;

#define SDDS_FILEBUFFER_SIZE  262144

  typedef struct {
    SDDS_LAYOUT layout, original_layout;
    short swapByteOrder;
    SDDS_FILEBUFFER fBuffer;
    SDDS_FILEBUFFER titleBuffer;
    int32_t page_number;

    short mode; /*file mode*/
    short page_started; /*0 or 1 for page not started or already started*/
    int32_t pages_read; /*the number of pages read so far*/
    int64_t endOfFile_offset;/*the offset in the end of the file*/
    int64_t *pagecount_offset; /*the offset of each read page */ 
    int64_t rowcount_offset;  /* ftell() position of row count */
    int64_t n_rows_written;   /* number of tabular data rows written to disk */
    int64_t last_row_written; /* virtual index of last row written */
    int64_t first_row_in_mem; /* virtual index of first row in memory */
    short writing_page;    /* 1/0 for writing/not-writing page */
    int64_t n_rows_allocated; /* number of tabular data rows for which space is alloc'd */
    int64_t n_rows;           /* number of rows stored in memory */
    int32_t *row_flag;        /* accept/reject flags for rows stored in memory */
    short file_had_data;   /* indicates that file being appended to already had some data in it (i.e.,
                            * more than just a header.  Affects no_row_counts=1 output.
                            */
    short autoRecover;
    short autoRecovered;
    short parallel_io;        /*flag for parallel SDDS */
    int32_t n_of_interest;          /* number of columns of interest */
    int32_t *column_order;          /* column_order[i] = internal index of user's ith column */
    int32_t *column_flag;           /* column_flag[i] indicates whether internal ith column has been selected */
    short *column_track_memory; /*indecates if the column memory should be tracked and eventually freed */

    int32_t readRecoveryPossible;
    int32_t deferSavingLayout;
    /* array of SDDS_ARRAY structures for storing array data */
    SDDS_ARRAY *array;

    /* array for parameter data.  The address of the data for the ith parameter
     * is parameter[i].  So *(<type-name> *)parameter[i] gives the data itself.  For type
     * SDDS_STRING the "data itself" is actually the address of the string, and the type-name
     * is "char *".
     */
    void **parameter;

    /* array for tabular data.  The starting address for the data for the ith column
     * is data[i].  So ((<type-name> *)data[i])[j] gives the jth data value.  
     * For type SDDS_STRING the "data value" is actually the address of the string, and
     * the type-name is "char *".
     */
    void **data;
#if SDDS_MPI_IO
    MPI_DATASET *MPI_dataset;
#endif
  } SDDS_DATASET;

  typedef SDDS_DATASET SDDS_TABLE;

  /* prototypes for routines to prepare and write SDDS files */
  epicsShareFuncSDDS extern int32_t SDDS_InitializeOutput(SDDS_DATASET *SDDS_dataset, int32_t data_mode,
                                                          int32_t lines_per_row, const char *description,
                                                          const char *contents, const char *filename);
  epicsShareFuncSDDS extern int32_t SDDS_Parallel_InitializeOutput(SDDS_DATASET *SDDS_dataset, const char *description,
                                                                   const char *contents, const char *filename);
  epicsShareFuncSDDS extern int32_t SDDS_InitializeAppend(SDDS_DATASET *SDDS_dataset, const char *filename);
  epicsShareFuncSDDS extern int32_t SDDS_InitializeAppendToPage(SDDS_DATASET *SDDS_dataset, const char *filename, 
                                                                int64_t updateInterval,
                                                                int64_t *rowsPresentReturn);
  epicsShareFuncSDDS extern int32_t SDDS_DisconnectFile(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_ReconnectFile(SDDS_DATASET *SDDS_dataset);
#define SDDS_IsDisconnected(SDDSptr) ((SDDSptr)->layout.disconnected)
  epicsShareFuncSDDS extern long SDDS_DisconnectInputFile(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_ReconnectInputFile(SDDS_DATASET *SDDS_dataset, long position);
  epicsShareFuncSDDS extern int32_t SDDS_ReadNewBinaryRows(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_FreeStringData(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_Terminate(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern void SDDS_SetTerminateMode(uint32_t mode);
#define TERMINATE_DONT_FREE_TABLE_STRINGS 0x0001
#define TERMINATE_DONT_FREE_ARRAY_STRINGS 0x0002
  epicsShareFuncSDDS extern void SDDS_SetColumnMemoryMode(SDDS_DATASET *SDDS_dataset, uint32_t mode);
  epicsShareFuncSDDS extern int32_t SDDS_GetColumnMemoryMode(SDDS_DATASET *SDDS_dataset);
#define DEFAULT_COLUMN_MEMORY_MODE 0
#define DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS 1
  epicsShareFuncSDDS extern int32_t SDDS_SetRowCountMode(SDDS_DATASET *SDDS_dataset, uint32_t mode);
#define SDDS_VARIABLEROWCOUNT 0x0001UL
#define SDDS_FIXEDROWCOUNT 0x0002UL
#define SDDS_NOROWCOUNT 0x0004UL
  epicsShareFuncSDDS extern int32_t SDDS_SetAutoReadRecovery(SDDS_DATASET *SDDS_dataset, uint32_t mode);
#define SDDS_NOAUTOREADRECOVER 0x0001UL
#define SDDS_AUTOREADRECOVER 0x0002UL
  epicsShareFuncSDDS extern int32_t SDDS_UpdateRowCount(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern void SDDS_DisableFSync(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern void SDDS_EnableFSync(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_DoFSync(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern void SDDS_SetLZMACompressionLevel(int32_t level);
  epicsShareFuncSDDS extern int32_t SDDS_GetLZMACompressionLevel(void);
  epicsShareFuncSDDS extern int32_t SDDS_DefineParameter(SDDS_DATASET *SDDS_dataset, const char *name, const char *symbol, const char *units, const char *description,
                                                         const char *format_string, int32_t type, char *fixed_value);
  epicsShareFuncSDDS extern int32_t SDDS_DefineParameter1(SDDS_DATASET *SDDS_dataset, const char *name, const char *symbol, const char *units, const char *description, 
                                                          const char *format_string, int32_t type, void *fixed_value);
  epicsShareFuncSDDS extern int32_t SDDS_DefineColumn(SDDS_DATASET *SDDS_dataset, const char *name, const char *symbol, const char *units, const char *description, 
                                                      const char *format_string, int32_t type, int32_t field_length);
  epicsShareFuncSDDS extern int32_t SDDS_DefineArray(SDDS_DATASET *SDDS_dataset, const char *name, const char *symbol, const char *units, const char *description, 
                                                     const char *format_string, int32_t type, int32_t field_length, int32_t dimensions, const char *group_name);
  epicsShareFuncSDDS extern int32_t SDDS_DefineAssociate(SDDS_DATASET *SDDS_dataset, const char *name,
                                                         const char *filename, const char *path, const char *description, const char *contents, int32_t sdds);
  epicsShareFuncSDDS extern int32_t SDDS_IsValidName(const char *name, const char *dataClass);
  epicsShareFuncSDDS extern int32_t SDDS_SetNameValidityFlags(uint32_t flags);
#define SDDS_ALLOW_ANY_NAME 0x0001UL
#define SDDS_ALLOW_V15_NAME 0x0002UL
  epicsShareFuncSDDS extern int32_t SDDS_DefineSimpleColumn(SDDS_DATASET *SDDS_dataset, const char *name, const char *unit, int32_t type);
  epicsShareFuncSDDS extern int32_t SDDS_DefineSimpleParameter(SDDS_DATASET *SDDS_dataset, const char *name, const char *unit, int32_t type);
  epicsShareFuncSDDS extern int32_t SDDS_DefineSimpleColumns(SDDS_DATASET *SDDS_dataset, int32_t number, char **name, char **unit, int32_t type);
  epicsShareFuncSDDS extern int32_t SDDS_DefineSimpleParameters(SDDS_DATASET *SDDS_dataset, int32_t number, char **name, char **unit, int32_t type);
  epicsShareFuncSDDS extern int32_t SDDS_SetNoRowCounts(SDDS_DATASET *SDDS_dataset, int32_t value);
  epicsShareFuncSDDS extern int32_t SDDS_WriteLayout(SDDS_DATASET *SDDS_dataset);
#define SDDS_LayoutWritten(SDDSptr) ((SDDSptr)->layout.layout_written)
  epicsShareFuncSDDS extern int32_t SDDS_EraseData(SDDS_DATASET *SDDS_dataset);

  epicsShareFuncSDDS extern int32_t SDDS_ProcessColumnString(SDDS_DATASET *SDDS_dataset, char *string, int32_t mode);
  epicsShareFuncSDDS extern int32_t SDDS_ProcessParameterString(SDDS_DATASET *SDDS_dataset, char *string, int32_t mode);
  epicsShareFuncSDDS extern int32_t SDDS_ProcessArrayString(SDDS_DATASET *SDDS_dataset, char *string);
  epicsShareFuncSDDS extern int32_t SDDS_ProcessAssociateString(SDDS_DATASET *SDDS_dataset, char *string);

  epicsShareFuncSDDS extern int32_t SDDS_InitializeCopy(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, char *filename, char *filemode);
  epicsShareFuncSDDS extern int32_t SDDS_CopyLayout(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source);
  epicsShareFuncSDDS extern int32_t SDDS_AppendLayout(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, uint32_t mode);
  epicsShareFuncSDDS extern int32_t SDDS_CopyPage(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source);
#define SDDS_CopyTable(a, b) SDDS_CopyPage(a, b)
  epicsShareFuncSDDS extern int32_t SDDS_CopyParameters(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source);
  epicsShareFuncSDDS extern int32_t SDDS_CopyArrays(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source);
  epicsShareFuncSDDS extern int32_t SDDS_CopyColumns(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source);
  epicsShareFuncSDDS extern int32_t SDDS_CopyRowsOfInterest(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source);
  epicsShareFuncSDDS extern int32_t SDDS_CopyRow(SDDS_DATASET *SDDS_target, int64_t target_row, SDDS_DATASET *SDDS_source, int64_t source_srow);
  epicsShareFuncSDDS extern int32_t SDDS_CopyRowDirect(SDDS_DATASET *SDDS_target, int64_t target_row, SDDS_DATASET *SDDS_source, int64_t source_row);
  epicsShareFuncSDDS extern int32_t SDDS_CopyAdditionalRows(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source);
  epicsShareFuncSDDS extern int32_t SDDS_CopyRows(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, int64_t firstRow, int64_t lastRow);

  epicsShareFuncSDDS extern void SDDS_DeferSavingLayout(SDDS_DATASET *SDDS_dataset, int32_t mode);
  epicsShareFuncSDDS extern int32_t SDDS_SaveLayout(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_RestoreLayout(SDDS_DATASET *SDDS_dataset);

#define SDDS_BY_INDEX 1
#define SDDS_BY_NAME  2

  epicsShareFuncSDDS extern int32_t SDDS_StartPage(SDDS_DATASET *SDDS_dataset, int64_t expected_n_rows);
#define SDDS_StartTable(a, b) SDDS_StartPage(a, b)
  epicsShareFuncSDDS extern int32_t SDDS_ClearPage(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_LengthenTable(SDDS_DATASET *SDDS_dataset, int64_t n_additional_rows);
  epicsShareFuncSDDS extern int32_t SDDS_ShortenTable(SDDS_DATASET *SDDS_dataset, int64_t rows);
#define SDDS_SET_BY_INDEX SDDS_BY_INDEX
#define SDDS_SET_BY_NAME SDDS_BY_NAME
#define SDDS_PASS_BY_VALUE 4
#define SDDS_PASS_BY_REFERENCE 8
#define SDDS_PASS_BY_STRING 16
  epicsShareFuncSDDS extern int32_t SDDS_SetParameters(SDDS_DATASET *SDDS_dataset, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_SetParameter(SDDS_DATASET *SDDS_dataset, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_SetRowValues(SDDS_DATASET *SDDS_dataset, int32_t mode, int64_t row, ...);
  epicsShareFuncSDDS extern int32_t SDDS_WritePage(SDDS_DATASET *SDDS_dataset);
#define SDDS_WriteTable(a) SDDS_WritePage(a)
  epicsShareFuncSDDS extern int32_t SDDS_UpdatePage(SDDS_DATASET *SDDS_dataset, uint32_t mode);
#define FLUSH_TABLE 0x1UL
#define SDDS_UpdateTable(a) SDDS_UpdatePage(a, 0)
  epicsShareFuncSDDS extern int32_t SDDS_SyncDataSet(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_SetColumn(SDDS_DATASET *SDDS_dataset, int32_t mode, void *data, int64_t rows, ...);
  epicsShareFuncSDDS extern int32_t SDDS_SetColumnFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, double *data, int64_t rows, ...);
  epicsShareFuncSDDS extern int32_t SDDS_SetColumnFromFloats(SDDS_DATASET *SDDS_dataset, int32_t mode, float *data, int64_t rows, ...);
  epicsShareFuncSDDS extern int32_t SDDS_SetColumnFromLongs(SDDS_DATASET *SDDS_dataset, int32_t mode, int32_t *data, int64_t rows, ...);
  epicsShareFuncSDDS extern int32_t SDDS_SetParametersFromDoubles(SDDS_DATASET *SDDS_dataset, int32_t mode, ...);

#define SDDS_GET_BY_INDEX SDDS_BY_INDEX
#define SDDS_GET_BY_NAME SDDS_BY_NAME
  epicsShareFuncSDDS extern int32_t SDDS_GetColumnInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_GetParameterInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_GetArrayInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_GetAssociateInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_ChangeColumnInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_ChangeParameterInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_ChangeArrayInformation(SDDS_DATASET *SDDS_dataset, char *field_name, void *memory, int32_t mode, ...);

  epicsShareFuncSDDS extern void SDDS_SetReadRecoveryMode(SDDS_DATASET *SDDS_dataset, int32_t mode);
  epicsShareFuncSDDS extern int32_t SDDS_SetDefaultIOBufferSize(int32_t bufferSize);

  /* prototypes for routines to read and use SDDS files  */
  epicsShareFuncSDDS extern int32_t SDDS_InitializeInputFromSearchPath(SDDS_DATASET *SDDSin, char *file);
  epicsShareFuncSDDS extern int32_t SDDS_InitializeInput(SDDS_DATASET *SDDS_dataset, char *filename);
  epicsShareFuncSDDS extern int32_t SDDS_ReadLayout(SDDS_DATASET *SDDS_dataset, FILE *fp);
  epicsShareFuncSDDS extern int32_t SDDS_InitializeHeaderlessInput(SDDS_DATASET *SDDS_dataset, char *filename);
  epicsShareFuncSDDS extern int64_t SDDS_GetRowLimit();
  epicsShareFuncSDDS extern int64_t SDDS_SetRowLimit(int64_t limit);
  epicsShareFuncSDDS extern int32_t SDDS_GotoPage(SDDS_DATASET *SDDS_dataset,int32_t page_number);
  epicsShareFuncSDDS extern int32_t SDDS_CheckEndOfFile(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_ReadPage(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_ReadPageSparse(SDDS_DATASET *SDDS_dataset, uint32_t mode,
                                                        int64_t sparse_interval,
                                                        int64_t sparse_offset, int32_t sparse_statistics);
  epicsShareFuncSDDS extern int32_t SDDS_ReadPageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows);
#define SDDS_ReadTable(a) SDDS_ReadPage(a)
  epicsShareFuncSDDS extern int32_t SDDS_ReadAsciiPage(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval,
                                                       int64_t sparse_offset, int32_t sparse_statistics);
  epicsShareFuncSDDS extern int32_t SDDS_ReadRecoveryPossible(SDDS_DATASET *SDDS_dataset);

  epicsShareFuncSDDS extern int32_t SDDS_SetColumnFlags(SDDS_DATASET *SDDS_dataset, int32_t column_flag_value);
  epicsShareFuncSDDS extern int32_t SDDS_SetRowFlags(SDDS_DATASET *SDDS_dataset, int32_t row_flag_value);
  epicsShareFuncSDDS extern int32_t SDDS_GetRowFlag(SDDS_DATASET *SDDS_dataset, int64_t row);
  epicsShareFuncSDDS extern int32_t SDDS_GetRowFlags(SDDS_DATASET *SDDS_dataset, int32_t *flag, int64_t rows);
  epicsShareFuncSDDS extern int32_t SDDS_BufferedRead(void *target, int64_t targetSize, FILE *fp, SDDS_FILEBUFFER *fBuffer, int32_t type, int32_t byteOrder);
#if defined(zLib)
  epicsShareFuncSDDS extern int32_t SDDS_GZipBufferedRead(void *target, int64_t targetSize, gzFile gzfp, SDDS_FILEBUFFER *fBuffer, int32_t type, int32_t byteOrder);
#endif

  /* logic flags for SDDS_AssertRowFlags and SDDS_AssertColumnFlags */
#define SDDS_FLAG_ARRAY  0x001UL
#define SDDS_INDEX_LIMITS 0x002UL
  epicsShareFuncSDDS extern int32_t SDDS_AssertRowFlags(SDDS_DATASET *SDDS_dataset, uint32_t mode, ...);
  /* modes for SDDS_SetColumnsOfInterest and SDDS_SetRowsOfInterest: */
#define SDDS_NAME_ARRAY 1
#define SDDS_NAMES_STRING 2
#define SDDS_NAME_STRINGS 3
#define SDDS_MATCH_STRING 4
#define SDDS_MATCH_EXCLUDE_STRING 5
#define SDDS_CI_NAME_ARRAY 6
#define SDDS_CI_NAMES_STRING 7
#define SDDS_CI_NAME_STRINGS 8
#define SDDS_CI_MATCH_STRING 9
#define SDDS_CI_MATCH_EXCLUDE_STRING 10

  /* logic flags for SDDS_SetColumnsOfInterest, SDDS_SetRowsOfInterest, and SDDS_MatchRowsOfInterest: */
#define SDDS_AND               0x0001UL
#define SDDS_OR                0x0002UL
#define SDDS_NEGATE_MATCH      0x0004UL
#define SDDS_NEGATE_PREVIOUS   0x0008UL
#define SDDS_NEGATE_EXPRESSION 0x0010UL
#define SDDS_INDIRECT_MATCH    0x0020UL
#define SDDS_1_PREVIOUS        0x0040UL
#define SDDS_0_PREVIOUS        0x0080UL
  /* used by MatchRowsOfInterest only at this point: */
#define SDDS_NOCASE_COMPARE    0x0100UL

  epicsShareFuncSDDS extern int32_t SDDS_MatchColumns(SDDS_DATASET *SDDS_dataset, char ***match, int32_t matchMode, int32_t typeMode, ... );
  epicsShareFuncSDDS extern int32_t SDDS_MatchParameters(SDDS_DATASET *SDDS_dataset, char ***match, int32_t matchMode, int32_t typeMode, ... );
  epicsShareFuncSDDS extern int32_t SDDS_MatchArrays(SDDS_DATASET *SDDS_dataset, char ***match, int32_t matchMode, int32_t typeMode, ... );
  epicsShareFuncSDDS extern int32_t SDDS_Logic(int32_t previous, int32_t match, uint32_t logic);
  epicsShareFuncSDDS extern int32_t SDDS_SetColumnsOfInterest(SDDS_DATASET *SDDS_dataset, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_AssertColumnFlags(SDDS_DATASET *SDDS_dataset, uint32_t mode, ...);
  epicsShareFuncSDDS extern int64_t SDDS_SetRowsOfInterest(SDDS_DATASET *SDDS_dataset, char *selection_column, int32_t mode, ...);
  epicsShareFuncSDDS extern int64_t SDDS_MatchRowsOfInterest(SDDS_DATASET *SDDS_dataset, char *selection_column, char *label_to_match, int32_t logic);
  epicsShareFuncSDDS extern int32_t SDDS_DeleteColumn(SDDS_DATASET *SDDS_dataset, char *column_name);
  epicsShareFuncSDDS extern int32_t SDDS_DeleteParameter(SDDS_DATASET *SDDS_dataset, char *parameter_name);
  epicsShareFuncSDDS extern int32_t SDDS_DeleteUnsetColumns(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_CountColumnsOfInterest(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_ColumnIsOfInterest(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern int32_t SDDS_ColumnCount(SDDS_DATASET *dataset);
  epicsShareFuncSDDS extern int32_t SDDS_ParameterCount(SDDS_DATASET *dataset);
  epicsShareFuncSDDS extern int32_t SDDS_ArrayCount(SDDS_DATASET *dataset);
  epicsShareFuncSDDS extern int64_t SDDS_CountRowsOfInterest(SDDS_DATASET *SDDS_dataset);
#define SDDS_RowCount(SDDS_dataset) ((SDDS_dataset)->n_rows)
  epicsShareFuncSDDS extern int32_t SDDS_DeleteUnsetRows(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int64_t SDDS_FilterRowsOfInterest(SDDS_DATASET *SDDS_dataset, char *filter_column, double lower, double upper, int32_t logic);
  epicsShareFuncSDDS extern int32_t SDDS_ItemInsideWindow(void *data, int64_t index, int32_t type, double lower_limit, double upper_limit);
  epicsShareFuncSDDS extern int64_t SDDS_FilterRowsByNumScan(SDDS_DATASET *SDDS_dataset, char *filter_column, uint32_t mode);
#define NUMSCANFILTER_INVERT 0x0001UL
#define NUMSCANFILTER_STRICT 0x0002UL

  epicsShareFuncSDDS extern void *SDDS_GetColumn(SDDS_DATASET *SDDS_dataset, char *column_name);
  epicsShareFuncSDDS extern void *SDDS_GetInternalColumn(SDDS_DATASET *SDDS_dataset, char *column_name);
  epicsShareFuncSDDS extern double *SDDS_GetColumnInDoubles(SDDS_DATASET *SDDS_dataset, char *column_name);
  epicsShareFuncSDDS extern float *SDDS_GetColumnInFloats(SDDS_DATASET *SDDS_dataset, char *column_name);
  epicsShareFuncSDDS extern int32_t *SDDS_GetColumnInLong(SDDS_DATASET *SDDS_dataset, char *column_name);
  epicsShareFuncSDDS extern short *SDDS_GetColumnInShort(SDDS_DATASET *SDDS_dataset, char *column_name);
  epicsShareFuncSDDS extern char **SDDS_GetColumnInString(SDDS_DATASET *SDDS_dataset, char *column_name);
  epicsShareFuncSDDS extern void *SDDS_GetNumericColumn(SDDS_DATASET *SDDS_dataset, char *column_name, int32_t desiredType);
  epicsShareFuncSDDS extern void *SDDS_GetRow(SDDS_DATASET *SDDS_dataset, int64_t srow_index, void *memory);
  epicsShareFuncSDDS extern void *SDDS_GetValue(SDDS_DATASET *SDDS_dataset, char *column_name, int64_t srow_index, void *memory);
  epicsShareFuncSDDS extern double SDDS_GetValueAsDouble(SDDS_DATASET *SDDS_dataset, char *column_name, int64_t srow_index);
  epicsShareFuncSDDS extern double SDDS_GetValueByIndexAsDouble(SDDS_DATASET *SDDS_dataset, int32_t column_index, int64_t srow_index);
  epicsShareFuncSDDS extern void *SDDS_GetValueByIndex(SDDS_DATASET *SDDS_dataset, int32_t column_index, int64_t srow_index, void *memory);
  epicsShareFuncSDDS extern void *SDDS_GetValueByAbsIndex(SDDS_DATASET *SDDS_dataset, int32_t column_index, int64_t srow_index, void *memory);
  epicsShareFuncSDDS extern void *SDDS_GetParameter(SDDS_DATASET *SDDS_dataset, char *parameter_name, void *memory);
  epicsShareFuncSDDS extern void *SDDS_GetParameterByIndex(SDDS_DATASET *SDDS_dataset, int32_t index, void *memory);
  epicsShareFuncSDDS extern long double *SDDS_GetParameterAsLongDouble(SDDS_DATASET *SDDS_dataset, char *parameter_name, long double *data);
  epicsShareFuncSDDS extern double *SDDS_GetParameterAsDouble(SDDS_DATASET *SDDS_dataset, char *parameter_name, double *data);
  epicsShareFuncSDDS extern int32_t *SDDS_GetParameterAsLong(SDDS_DATASET *SDDS_dataset, char *parameter_name, int32_t *data);
  epicsShareFuncSDDS extern int64_t *SDDS_GetParameterAsLong64(SDDS_DATASET *SDDS_dataset, char *parameter_name, int64_t *data);
  epicsShareFuncSDDS extern char *SDDS_GetParameterAsString(SDDS_DATASET *SDDS_dataset, char *parameter_name, char **memory);
  epicsShareFuncSDDS extern char *SDDS_GetParameterAsFormattedString(SDDS_DATASET *SDDS_dataset, char *parameter_name, char **memory, char *suppliedformat);
  epicsShareFuncSDDS extern int32_t SDDS_GetParameters(SDDS_DATASET *SDDS_dataset, ...);
  epicsShareFuncSDDS extern void *SDDS_GetFixedValueParameter(SDDS_DATASET *SDDS_dataset, char *parameter_name, void *memory);
  epicsShareFuncSDDS extern int32_t SDDS_GetDescription(SDDS_DATASET *SDDS_dataset, char **text, char **contents);

  epicsShareFuncSDDS extern  int32_t SDDS_SetArrayUnitsConversion(SDDS_DATASET *SDDS_dataset, char *column_name, char *new_units, char *old_units, double factor);
  epicsShareFuncSDDS extern  int32_t SDDS_SetColumnUnitsConversion(SDDS_DATASET *SDDS_dataset, char *column_name, char *new_units, char *old_units, double factor);
  epicsShareFuncSDDS extern  int32_t SDDS_SetParameterUnitsConversion(SDDS_DATASET *SDDS_dataset, char *column_name, char *new_units, char *old_units, double factor);

  epicsShareFuncSDDS extern void *SDDS_GetMatrixOfRows(SDDS_DATASET *SDDS_dataset, int64_t *n_rows);
  epicsShareFuncSDDS extern void *SDDS_GetCastMatrixOfRows(SDDS_DATASET *SDDS_dataset, int64_t *n_rows, int32_t sddsType);
#define SDDS_ROW_MAJOR_DATA 1
#define SDDS_COLUMN_MAJOR_DATA 2
  epicsShareFuncSDDS extern void *SDDS_GetMatrixFromColumn(SDDS_DATASET *SDDS_dataset, char *column_name, int64_t dimension1, int64_t dimension2, int32_t mode);
  epicsShareFuncSDDS extern void *SDDS_GetDoubleMatrixFromColumn(SDDS_DATASET *SDDS_dataset, char *column_name, int64_t dimension1, int64_t dimension2, int32_t mode);

  epicsShareFuncSDDS extern SDDS_ARRAY *SDDS_GetArray(SDDS_DATASET *SDDS_dataset, char *array_name, SDDS_ARRAY *memory);
#define SDDS_POINTER_ARRAY 1
#define SDDS_CONTIGUOUS_DATA 2
  epicsShareFuncSDDS extern double *SDDS_GetArrayInDoubles(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t *values);
  epicsShareFuncSDDS extern int32_t *SDDS_GetArrayInLong(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t *values);
  epicsShareFuncSDDS extern char **SDDS_GetArrayInString(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t *values);
  epicsShareFuncSDDS extern int32_t SDDS_SetArrayVararg(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t mode, void *data_pointer, ...);
  epicsShareFuncSDDS extern int32_t SDDS_SetArray(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t mode, void *data_pointer, int32_t *dimension);
  epicsShareFuncSDDS extern int32_t SDDS_AppendToArrayVararg(SDDS_DATASET *SDDS_dataset, char *array_name, int32_t mode, void *data_pointer, int32_t elements, ...);


  /* error-handling and utility routines: */
  epicsShareFuncSDDS extern void *SDDS_Realloc(void *old_ptr, size_t new_size);
  epicsShareFuncSDDS extern void *SDDS_Malloc(size_t size);
  epicsShareFuncSDDS extern void SDDS_Free(void *mem);
  epicsShareFuncSDDS extern void *SDDS_Calloc(size_t nelem, size_t elem_size);
  epicsShareFuncSDDS extern int32_t SDDS_NumberOfErrors(void);
  epicsShareFuncSDDS extern void SDDS_ClearErrors(void);
  epicsShareFuncSDDS extern void SDDS_SetError(char *error_text);
  epicsShareFuncSDDS extern void SDDS_SetError0(char *error_text);
  epicsShareFuncSDDS extern void SDDS_Bomb(char *message);
  epicsShareFuncSDDS extern void SDDS_Warning(char *message);
  epicsShareFuncSDDS extern void SDDS_RegisterProgramName(const char *name);
#define SDDS_VERBOSE_PrintErrors 1
#define SDDS_EXIT_PrintErrors 2
  epicsShareFuncSDDS extern void SDDS_PrintErrors(FILE *fp, int32_t mode);
#define SDDS_LAST_GetErrorMessages 0
#define SDDS_ALL_GetErrorMessages 1
  epicsShareFuncSDDS extern char **SDDS_GetErrorMessages(int32_t *number, int32_t mode);

  epicsShareFuncSDDS extern char **SDDS_GetColumnNames(SDDS_DATASET *SDDS_dataset, int32_t *number);
  epicsShareFuncSDDS extern char **SDDS_GetParameterNames(SDDS_DATASET *SDDS_dataset, int32_t *number);
  epicsShareFuncSDDS extern char **SDDS_GetAssociateNames(SDDS_DATASET *SDDS_dataset, int32_t *number);
  epicsShareFuncSDDS extern char **SDDS_GetArrayNames(SDDS_DATASET *SDDS_dataset, int32_t *number);

  epicsShareFuncSDDS extern COLUMN_DEFINITION *SDDS_GetColumnDefinition(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern COLUMN_DEFINITION *SDDS_CopyColumnDefinition(COLUMN_DEFINITION **target, COLUMN_DEFINITION *source);
  epicsShareFuncSDDS extern int32_t SDDS_FreeColumnDefinition(COLUMN_DEFINITION *source);
  epicsShareFuncSDDS extern int32_t SDDS_TransferColumnDefinition(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName);
  epicsShareFuncSDDS extern int32_t SDDS_DefineColumnLikeParameter(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName);
  epicsShareFuncSDDS extern int32_t SDDS_DefineColumnLikeArray(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName);
  epicsShareFuncSDDS extern int32_t SDDS_TransferAllColumnDefinitions(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source,
                                                                      uint32_t mode);
  epicsShareFuncSDDS extern int32_t SDDS_ParseNamelist(void *data, SDDS_FIELD_INFORMATION *fieldInfo, int32_t fieldInfos, char *s);
  epicsShareFuncSDDS extern PARAMETER_DEFINITION *SDDS_GetParameterDefinition(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern PARAMETER_DEFINITION *SDDS_CopyParameterDefinition(PARAMETER_DEFINITION **target, PARAMETER_DEFINITION *source);
  epicsShareFuncSDDS extern int32_t SDDS_FreeParameterDefinition(PARAMETER_DEFINITION *source);
  epicsShareFuncSDDS extern int32_t SDDS_TransferParameterDefinition(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName);
  epicsShareFuncSDDS extern int32_t SDDS_DefineParameterLikeColumn(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName);
  epicsShareFuncSDDS extern int32_t SDDS_DefineParameterLikeArray(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName);
#define SDDS_TRANSFER_KEEPOLD 0x01UL
#define SDDS_TRANSFER_OVERWRITE 0x02UL
  epicsShareFuncSDDS extern int32_t SDDS_TransferAllParameterDefinitions(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, 
                                                                         uint32_t mode);

  epicsShareFuncSDDS extern ARRAY_DEFINITION *SDDS_GetArrayDefinition(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern ARRAY_DEFINITION *SDDS_CopyArrayDefinition(ARRAY_DEFINITION **target, ARRAY_DEFINITION *source);
  epicsShareFuncSDDS extern int32_t SDDS_FreeArrayDefinition(ARRAY_DEFINITION *source);
  epicsShareFuncSDDS extern int32_t SDDS_TransferArrayDefinition(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName);
  epicsShareFuncSDDS extern int32_t SDDS_TransferAllArrayDefinitions(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source,
                                                                     uint32_t mode);


  epicsShareFuncSDDS extern ASSOCIATE_DEFINITION *SDDS_GetAssociateDefinition(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern ASSOCIATE_DEFINITION *SDDS_CopyAssociateDefinition(ASSOCIATE_DEFINITION **target, ASSOCIATE_DEFINITION *source);
  epicsShareFuncSDDS extern int32_t SDDS_FreeAssociateDefinition(ASSOCIATE_DEFINITION *source);
  epicsShareFuncSDDS extern int32_t SDDS_TransferAssociateDefinition(SDDS_DATASET *target, SDDS_DATASET *source, char *name, char *newName);

  epicsShareFuncSDDS extern int32_t SDDS_GetColumnIndex(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern int32_t SDDS_GetParameterIndex(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern int32_t SDDS_GetArrayIndex(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern int32_t SDDS_GetAssociateIndex(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern int32_t SDDS_GetColumnType(SDDS_DATASET *SDDS_dataset, int32_t index);
  epicsShareFuncSDDS extern int32_t SDDS_GetNamedColumnType(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern int32_t SDDS_GetParameterType(SDDS_DATASET *SDDS_dataset, int32_t index);
  epicsShareFuncSDDS extern int32_t SDDS_GetNamedParameterType(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern int32_t SDDS_GetArrayType(SDDS_DATASET *SDDS_dataset, int32_t index);
  epicsShareFuncSDDS extern int32_t SDDS_GetNamedArrayType(SDDS_DATASET *SDDS_dataset, char *name);
  epicsShareFuncSDDS extern int32_t SDDS_GetTypeSize(int32_t type);
  epicsShareFuncSDDS extern char *SDDS_GetTypeName(int32_t type);
  epicsShareFuncSDDS extern int32_t SDDS_IdentifyType(char *typeName);

#define FIND_ANY_TYPE       0
#define FIND_SPECIFIED_TYPE 1
#define FIND_NUMERIC_TYPE   2
#define FIND_INTEGER_TYPE   3
#define FIND_FLOATING_TYPE  4

  epicsShareFuncSDDS extern char *SDDS_FindColumn(SDDS_DATASET *SDDS_dataset,  int32_t mode, ...);
  epicsShareFuncSDDS extern char *SDDS_FindParameter(SDDS_DATASET *SDDS_dataset,  int32_t mode, ...);
  epicsShareFuncSDDS extern char *SDDS_FindArray(SDDS_DATASET *SDDS_dataset,  int32_t mode, ...);

  epicsShareFuncSDDS extern int32_t SDDS_CheckColumn(SDDS_DATASET *SDDS_dataset, char *name, char *units, int32_t type, FILE *fp_message);
  epicsShareFuncSDDS extern int32_t SDDS_CheckParameter(SDDS_DATASET *SDDS_dataset, char *name, char *units, int32_t type, FILE *fp_message);
  epicsShareFuncSDDS extern int32_t SDDS_CheckArray(SDDS_DATASET *SDDS_dataset, char *name, char *units, int32_t type, FILE *fp_message);
  epicsShareFuncSDDS extern int32_t SDDS_VerifyArrayExists(SDDS_DATASET *SDDS_dataset, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_VerifyColumnExists(SDDS_DATASET *SDDS_dataset, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_VerifyParameterExists(SDDS_DATASET *SDDS_dataset, int32_t mode, ...);
  epicsShareFuncSDDS extern int32_t SDDS_PrintCheckText(FILE *fp, char *name, char *units, int32_t type, char *class_name, int32_t error_code);
#define SDDS_CHECK_OKAY 0
#define SDDS_CHECK_OK SDDS_CHECK_OKAY
#define SDDS_CHECK_NONEXISTENT 1
#define SDDS_CHECK_WRONGTYPE  2
#define SDDS_CHECK_WRONGUNITS  3

  epicsShareFuncSDDS extern int32_t SDDS_IsActive(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_ForceInactive(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_LockFile(FILE *fp, const char *filename, const char *callerName);
  epicsShareFuncSDDS extern int32_t SDDS_FileIsLocked(const char *filename);
  epicsShareFuncSDDS extern int32_t SDDS_BreakIntoLockedFile(char *filename);

  epicsShareFuncSDDS extern int32_t SDDS_CopyString(char **target, const char *source);
  epicsShareFuncSDDS extern int32_t SDDS_CopyStringArray(char **target, char **source, int64_t n_strings);
  epicsShareFuncSDDS extern int32_t SDDS_FreeStringArray(char **string, int64_t strings);
  epicsShareFuncSDDS extern int32_t SDDS_VerifyPrintfFormat(const char *format_string, int32_t type);
  epicsShareFuncSDDS extern int32_t SDDS_HasWhitespace(char *string);
  epicsShareFuncSDDS extern char *fgetsSkipComments(SDDS_DATASET *SDDS_dataset, char *s, int32_t slen, FILE *fp, char skip_char);
  epicsShareFuncSDDS extern char *fgetsSkipCommentsResize(SDDS_DATASET *SDDS_dataset, char **s, int32_t *slen, FILE *fp, char skip_char);
#if defined(zLib)
  epicsShareFuncSDDS extern char *fgetsGZipSkipComments(SDDS_DATASET *SDDS_dataset, char *s, int32_t slen, gzFile gzfp, char skip_char);
  epicsShareFuncSDDS extern char *fgetsGZipSkipCommentsResize(SDDS_DATASET *SDDS_dataset, char **s, int32_t *slen, gzFile gzfp, char skip_char);
#endif
  epicsShareFuncSDDS extern void SDDS_CutOutComments(SDDS_DATASET *SDDS_dataset,char *s, char cc);
  epicsShareFuncSDDS extern void SDDS_EscapeNewlines(char *s);
  epicsShareFuncSDDS extern void SDDS_EscapeQuotes(char *s, char quote_char);
  epicsShareFuncSDDS extern void SDDS_UnescapeQuotes(char *s, char quote_char);
  epicsShareFuncSDDS extern int32_t SDDS_IsQuoted(char *string, char *position, char quotation_mark);
  epicsShareFuncSDDS extern int32_t SDDS_GetToken(char *s, char *buffer, int32_t buflen);
  epicsShareFuncSDDS extern int32_t SDDS_GetToken2(char *s, char **st, int32_t *strlength, char *buffer, int32_t buflen);
  epicsShareFuncSDDS extern int32_t SDDS_PadToLength(char *string, int32_t length);
  epicsShareFuncSDDS extern void SDDS_EscapeCommentCharacters(char *string, char cc);
  epicsShareFuncSDDS extern void SDDS_InterpretEscapes(char *s);

  epicsShareFuncSDDS extern int32_t SDDS_ZeroMemory(void *mem, int64_t n_bytes);
  epicsShareFuncSDDS extern int32_t SDDS_SetMemory(void *mem, int64_t n_elements, int32_t data_type, ...);
#define SDDS_PRINT_NOQUOTES 0x0001UL
  epicsShareFuncSDDS extern int32_t SDDS_SprintTypedValue(void *data, int64_t index, int32_t type, const char *format, char *buffer, uint32_t mode);
  epicsShareFuncSDDS extern int32_t SDDS_SprintTypedValueFactor(void *data, int64_t index, int32_t type, const char *format, char *buffer, uint32_t mode, double factor);
  epicsShareFuncSDDS extern int32_t SDDS_PrintTypedValue(void *data, int64_t index, int32_t type, char *format, FILE *fp, uint32_t mode);
  epicsShareFuncSDDS extern int32_t SDDS_WriteTypedValue(void *data, int64_t index, int32_t type, char *format, FILE *fp);
  epicsShareFuncSDDS extern void *SDDS_CastValue(void *data, int64_t index, int32_t data_type, int32_t desired_type, void *memory);
  epicsShareFuncSDDS extern void SDDS_RemovePadding(char *s);
  epicsShareFuncSDDS extern int32_t SDDS_StringIsBlank(char *s);
  epicsShareFuncSDDS extern void *SDDS_AllocateMatrix(int32_t size, int64_t dim1, int64_t dim2);
  epicsShareFuncSDDS extern void SDDS_FreeMatrix(void **ptr, int64_t dim1);
  epicsShareFuncSDDS extern void SDDS_FreeArray(SDDS_ARRAY *array);
  epicsShareFuncSDDS extern void *SDDS_MakePointerArray(void *data, int32_t type, int32_t dimensions, int32_t *dimension);
  epicsShareFuncSDDS extern int32_t SDDS_ApplyFactorToParameter(SDDS_DATASET *SDDS_dataset, char *name, double factor);
  epicsShareFuncSDDS extern int32_t SDDS_ApplyFactorToColumn(SDDS_DATASET *SDDS_dataset, char *name, double factor);
  epicsShareFuncSDDS extern int32_t SDDS_DeleteParameterFixedValues(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_SetDataMode(SDDS_DATASET *SDDS_dataset, int32_t newmode);
  epicsShareFuncSDDS extern int32_t SDDS_CheckDataset(SDDS_DATASET *SDDS_dataset, const char *caller);
  epicsShareFuncSDDS extern int32_t SDDS_CheckTabularData(SDDS_DATASET *SDDS_dataset, const char *caller);
  epicsShareFuncSDDS extern int32_t SDDS_CheckDatasetStructureSize(int32_t size);
#define SDDS_CheckTableStructureSize(a) SDDS_CheckDatasetStructureSize(a)

#define TABULAR_DATA_CHECKS 0x0001UL
  epicsShareFuncSDDS uint32_t SDDS_SetAutoCheckMode(uint32_t newMode);

  epicsShareFuncSDDS extern int32_t SDDS_FlushBuffer(FILE *fp, SDDS_FILEBUFFER *fBuffer);
  epicsShareFuncSDDS extern int32_t SDDS_BufferedWrite(void *target, int64_t targetSize, FILE *fp, SDDS_FILEBUFFER *fBuffer);

  epicsShareFuncSDDS extern int32_t SDDS_ScanData(char *string, int32_t type, int32_t field_length, void *data, int64_t index, int32_t is_parameter);
  epicsShareFuncSDDS extern int32_t SDDS_ScanData2(char *string, char **pstring, int32_t *strlength, int32_t type, int32_t field_length, void *data, int64_t index, int32_t is_parameter);

  epicsShareFuncSDDS extern long double SDDS_ConvertToLongDouble(int32_t type, void *data, int64_t index);
  epicsShareFuncSDDS extern double SDDS_ConvertToDouble(int32_t type, void *data, int64_t index);
  epicsShareFuncSDDS extern int64_t SDDS_ConvertToLong64(int32_t type, void *data, int64_t index);
  epicsShareFuncSDDS extern int32_t SDDS_ConvertToLong(int32_t type, void *data, int64_t index);

  epicsShareFuncSDDS extern int32_t SDDS_WriteBinaryString(char *string, FILE *fp, SDDS_FILEBUFFER *fBuffer);
#if defined(zLib)
  epicsShareFuncSDDS extern int32_t SDDS_GZipWriteBinaryString(char *string, gzFile gzfp, SDDS_FILEBUFFER *fBuffer);
  epicsShareFuncSDDS extern int32_t SDDS_GZipFlushBuffer(gzFile gzfp, SDDS_FILEBUFFER *fBuffer);
  epicsShareFuncSDDS extern int32_t SDDS_GZipBufferedWrite(void *target, int64_t targetSize, gzFile gzfp, SDDS_FILEBUFFER *fBuffer);
#endif


  epicsShareFuncSDDS extern int64_t SDDS_CreateRpnMemory(const char *name, short is_string);
  epicsShareFuncSDDS extern int64_t SDDS_CreateRpnArray(char *name);

#if defined(RPN_SUPPORT)
  epicsShareFuncSDDS extern int32_t SDDS_FilterRowsWithRpnTest(SDDS_DATASET *SDDS_dataset, char *rpn_test);
  epicsShareFuncSDDS extern int32_t SDDS_StoreParametersInRpnMemories(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_StoreRowInRpnMemories(SDDS_DATASET *SDDS_dataset, int64_t row);
  epicsShareFuncSDDS extern int32_t SDDS_StoreColumnsInRpnArrays(SDDS_DATASET *SDDS_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_ComputeColumn(SDDS_DATASET *SDDS_dataset, int32_t column, char *equation);
  epicsShareFuncSDDS extern int32_t SDDS_ComputeParameter(SDDS_DATASET *SDDS_dataset, int32_t column, char *equation);
#endif

#define SDDS_BIGENDIAN_SEEN      0x0001UL
#define SDDS_LITTLEENDIAN_SEEN   0x0002UL
#define SDDS_FIXED_ROWCOUNT_SEEN 0x0004UL
#define SDDS_BIGENDIAN         SDDS_BIGENDIAN_SEEN
#define SDDS_LITTLEENDIAN      SDDS_LITTLEENDIAN_SEEN
#define SDDS_FIXED_ROWCOUNT    SDDS_FIXED_ROWCOUNT_SEEN
  epicsShareFuncSDDS extern int32_t SDDS_IsBigEndianMachine();
  void SDDS_SwapShort(short *data);
  void SDDS_SwapUShort(unsigned short *data);
  epicsShareFuncSDDS extern void SDDS_SwapLong(int32_t *data);
  epicsShareFuncSDDS extern void SDDS_SwapULong(uint32_t *data);
  epicsShareFuncSDDS extern void SDDS_SwapLong64(int64_t *data);
  epicsShareFuncSDDS extern void SDDS_SwapULong64(uint64_t *data);
  void SDDS_SwapFloat(float *data);
  void SDDS_SwapDouble(double *data);
  void SDDS_SwapLongDouble(long double *data);
  epicsShareFuncSDDS extern int32_t SDDS_SwapEndsArrayData(SDDS_DATASET *SDDSin);
  epicsShareFuncSDDS extern int32_t SDDS_SwapEndsParameterData(SDDS_DATASET *SDDSin) ;
  epicsShareFuncSDDS extern int32_t SDDS_SwapEndsColumnData(SDDS_DATASET *SDDSin);






  epicsShareFuncSDDS extern int32_t SDDS_ReadNonNativePage(SDDS_DATASET *SDDS_dataset);
  int32_t SDDS_ReadNonNativePageSparse(SDDS_DATASET *SDDS_dataset, uint32_t mode,
                                       int64_t sparse_interval,
                                       int64_t sparse_offset);
  int32_t SDDS_ReadNonNativeBinaryPage(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset);
  int32_t SDDS_ReadNonNativeBinaryParameters(SDDS_DATASET *SDDS_dataset);
  int32_t SDDS_ReadNonNativeBinaryArrays(SDDS_DATASET *SDDS_dataset);
  int32_t SDDS_ReadNonNativeBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row, int32_t skip);
  char *SDDS_ReadNonNativeBinaryString(FILE *fp, SDDS_FILEBUFFER *fBuffer, int32_t skip);
#if defined(zLib)
  char *SDDS_ReadNonNativeGZipBinaryString(gzFile gzfp, SDDS_FILEBUFFER *fBuffer, int32_t skip);
#endif



  epicsShareFuncSDDS extern int32_t SDDS_WriteNonNativeBinaryPage(SDDS_DATASET *SDDS_dataset);
  int32_t SDDS_WriteNonNativeBinaryParameters(SDDS_DATASET *SDDS_dataset);
  int32_t SDDS_WriteNonNativeBinaryArrays(SDDS_DATASET *SDDS_dataset);
  int32_t SDDS_WriteNonNativeBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row);

  int32_t SDDS_WriteNonNativeBinaryString(char *string, FILE *fp, SDDS_FILEBUFFER *fBuffer);
#if defined(zLib)
  int32_t SDDS_GZipWriteNonNativeBinaryString(char *string, gzFile gzfp, SDDS_FILEBUFFER *fBuffer);
#endif

#define SDDS_MATCH_COLUMN 0
#define SDDS_MATCH_PARAMETER 1
#define SDDS_MATCH_ARRAY 2
  epicsShareFuncSDDS extern char **getMatchingSDDSNames(SDDS_DATASET *dataset, char **matchName, int32_t matches, int32_t *names,  short match_type);

  epicsShareFuncSDDS extern SDDS_DATASET *SDDS_CreateEmptyDataset(void);

#if SDDS_MPI_IO
  /* SDDSmpi_output.c */
  char *BlankToNull(char *string);
  epicsShareFuncSDDS extern void SDDS_MPI_BOMB(char *text, MPI_File *mpi_file);
  void SDDS_MPI_GOTO_ERROR(FILE *fp, char *str, int32_t mpierror, int32_t exit);
  epicsShareFuncSDDS extern int32_t SDDS_MPI_File_Open(MPI_DATASET *MPI_dataset, char *filename, unsigned long flags);
  char *SDDS_CreateNamelistField(char *name, char *value);
  char *SDDS_CreateDescription(char *text, char *contents);
  char *SDDS_CreateParameterDefinition(PARAMETER_DEFINITION *parameter_definition);
  char *SDDS_CreateColumnDefinition(COLUMN_DEFINITION *column_definition);
  char *SDDS_CreateArrayDefinition(ARRAY_DEFINITION *array_definition);
  char *SDDS_CreateAssociateDefinition(ASSOCIATE_DEFINITION *associate_definition);
  char *SDDS_CreateDataMode(DATA_MODE *data_mode);
#define SDDS_MPI_WriteTable(a) SDDS_MPI_WritePage(a)
  epicsShareFuncSDDS extern int32_t SDDS_MPI_WriteLayout(SDDS_DATASET *MPI_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_MPI_WritePage(SDDS_DATASET *MPI_dataset);
  MPI_Datatype Convert_SDDStype_To_MPItype(int32_t SDDS_type);
  epicsShareFuncSDDS extern int32_t SDDS_MPI_Terminate(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_InitializeOutput(SDDS_DATASET *MPI_dataset, char *description, char *contents, char *filename, unsigned long flags, short column_major);
  int32_t SDDS_MPI_InitializeCopy(SDDS_DATASET *MPI_dataset_target, SDDS_DATASET *SDDS_source, char *filename, short column_major);
  
  /*SDDS_MPI_binary.c writing data*/
  epicsShareFuncSDDS extern int32_t SDDS_SetDefaultWriteBufferSize(int32_t newSize);
  int32_t SDDS_CheckStringTruncated(void);
  void SDDS_StringTuncated(void);
  int32_t SDDS_SetDefaultStringLength(int32_t newValue);
  int32_t SDDS_MPI_WriteBinaryPage(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_WriteBinaryString(SDDS_DATASET *MPI_dataset, char *string);
  int32_t SDDS_MPI_WriteBinaryParameters(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_WriteBinaryArrays(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_WriteBinaryRow(SDDS_DATASET *MPI_dataset, int64_t row);
  int32_t SDDS_MPI_WriteNonNativeBinaryPage(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_WriteNonNativeBinaryString(SDDS_DATASET *MPI_dataset, char *string);
  int32_t SDDS_MPI_WriteNonNativeBinaryParameters(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_WriteNonNativeBinaryArrays(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_WriteNonNativeBinaryRow(SDDS_DATASET *MPI_dataset, int64_t row);
  int32_t SDDS_MPI_BufferedReadNonNativeBinaryTitle(SDDS_DATASET *SDDS_dataset);
  int32_t SDDS_MPI_CollectiveReadByRow(SDDS_DATASET *SDDS_dataset);
  MPI_Offset SDDS_MPI_Get_Column_Size(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_CollectiveWriteByRow(SDDS_DATASET *SDDS_dataset);
  int32_t SDDS_MPI_Get_Title_Size(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_BufferedWrite(void *target, int64_t targetSize, SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_FlushBuffer(SDDS_DATASET *MPI_Dataset);
  int64_t SDDS_MPI_GetTotalRows(SDDS_DATASET *MPI_dataset);
  int64_t SDDS_MPI_CountRowsOfInterest(SDDS_DATASET *SDDS_dataset, int64_t start_row, int64_t end_row);
  int32_t SDDS_MPI_WriteContinuousBinaryPage(SDDS_DATASET *MPI_dataset);
  MPI_Offset SDDS_MPI_GetTitleOffset(SDDS_DATASET *MPI_dataset);
  /*SDDS_MPI_binary.c reading data*/
  epicsShareFuncSDDS extern int32_t SDDS_SetDefaultReadBufferSize(int32_t newSize);
  int32_t SDDS_MPI_BufferedRead(void *target, int64_t targetSize, SDDS_DATASET *MPI_dataset, SDDS_FILEBUFFER *fBuffer);
  int32_t SDDS_MPI_ReadBinaryPage(SDDS_DATASET *MPI_dataset);
  char *SDDS_MPI_ReadNonNativeBinaryString(SDDS_DATASET *MPI_dataset, SDDS_FILEBUFFER *fBuffer, int32_t skip);
  int32_t SDDS_MPI_ReadBinaryParameters(SDDS_DATASET *MPI_dataset, SDDS_FILEBUFFER *fBuffer);
  int32_t SDDS_MPI_ReadBinaryArrays(SDDS_DATASET *MPI_dataset, SDDS_FILEBUFFER *fBuffer);
  int32_t SDDS_MPI_ReadBinaryRow(SDDS_DATASET *MPI_dataset, int64_t row, int32_t skip);
  int32_t SDDS_MPI_ReadNonNativeBinaryParameters(SDDS_DATASET *SDDS_dataset,  SDDS_FILEBUFFER *fBuffer);
  int32_t SDDS_MPI_ReadNonNativeBinaryArrays(SDDS_DATASET *MPI_dataset,  SDDS_FILEBUFFER *fBuffer);
  int32_t SDDS_MPI_ReadNonNativeBinaryRow(SDDS_DATASET *MPI_dataset, int64_t row, int32_t skip);
  int32_t SDDS_MPI_ReadBinaryPage(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_ReadNonNativePage(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_ReadNonNativePageSparse(SDDS_DATASET *MPI_dataset, uint32_t mode);
  int32_t SDDS_MPI_ReadNonNativeBinaryPage(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_MPI_BufferedReadBinaryTitle(SDDS_DATASET *MPI_dataset);
  int32_t SDDS_SetDefaultTitleBufferSize(int32_t newSize);
  int32_t SDDS_MPI_WriteBinaryPageByColumn(SDDS_DATASET *MPI_dataset);
  epicsShareFuncSDDS extern void SDDS_MPI_SetWriteKludgeUsleep(long value);
  epicsShareFuncSDDS extern void SDDS_MPI_SetFileSync(short value);
  epicsShareFuncSDDS extern void SDDS_MPI_Setup(SDDS_DATASET *SDDS_dataset, int32_t parallel_io, int32_t n_processors, int32_t myid, MPI_Comm comm, short master_read);
  
  /*SDDSmpi_input.c */
  epicsShareFuncSDDS extern int32_t SDDS_MPI_ReadPage(SDDS_DATASET *MPI_dataset);
  epicsShareFuncSDDS extern int32_t SDDS_MPI_InitializeInput(SDDS_DATASET *MPI_dataset, char *filename);
  epicsShareFuncSDDS extern int32_t SDDS_MPI_InitializeInputFromSearchPath(SDDS_DATASET *MPI_dataset, char *file);
  epicsShareFuncSDDS extern int32_t SDDS_Master_InitializeInput(SDDS_DATASET *SDDS_dataset, MPI_DATASET *MPI_dataset, char *file);
  epicsShareFuncSDDS extern int32_t SDDS_Master_InitializeInputFromSearchPath(SDDS_DATASET *SDDS_dataset, MPI_DATASET *MPI_dataset, char *file);
  epicsShareFuncSDDS extern int32_t SDDS_Master_ReadPage(SDDS_DATASET *SDDS_dataset);
#define SDDS_MPI_TotalRowCount(SDDS_DATASET) ((SDDS_DATASET)->MPI_dataset->total_rows) 
#endif

#ifdef __cplusplus
}
#endif

#endif
