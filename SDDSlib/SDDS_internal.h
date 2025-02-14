/**
 * @file SDDS_internal.h
 * @brief Internal definitions and function declarations for SDDS with LZMA support
 *
 * This header file contains internal macros, structures, and function prototypes
 * used by the SDDS (Self Describing Data Sets) library to handle LZMA-compressed
 * files. It includes definitions for the `lzmafile` structure, buffer sizes,
 * and various binary and ASCII I/O routines tailored for SDDS datasets.
 *
 * The file ensures that internal components are only included once using
 * include guards and conditionally includes the LZMA library when necessary.
 *
 * ### Key Components:
 * - **Structures:**
 *   - `lzmafile`: Represents an LZMA-compressed file with associated stream and buffer.
 *
 * - **Function Prototypes:**
 *   - `lzma_open`, `lzma_close`, `lzma_read`, `lzma_write`: Basic file operations for LZMA files.
 *   - `SDDS_WriteBinaryArrays`, `SDDS_ReadBinaryArrays`, etc.: SDDS-specific binary I/O functions.
 *   - `SDDS_WriteAsciiArrays`, `SDDS_ReadAsciiArrays`, etc.: SDDS-specific ASCII I/O functions.
 *   - Additional utility and processing functions for handling SDDS datasets.
 *
 * - **Macros:**
 *   - `LZMA_BUF_SIZE`: Defines the buffer size for LZMA operations.
 *   - `TABLE_LENGTH_INCREMENT`: Specifies the increment size for table allocations.
 *   - Command definitions for various SDDS operations.
 *
 * ### Dependencies:
 * - Requires the LZMA library (`lzma.h`) for compression support.
 * - Conditionally includes zLib support if defined.
 *
 * ### Usage:
 * This file is intended for internal use within the SDDS library and should not be
 * included directly by external applications. It provides the necessary infrastructure
 * to manage compressed data streams and perform efficient I/O operations on SDDS datasets.
 *
 * @note Ensure that the LZMA and zLib libraries are properly linked when compiling
 *       components that include this header.
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

#include "SDDS.h"

#if !defined(_SDDS_internal_)
#  define _SDDS_internal_ 1

#  if !defined(LZMA_BUF_SIZE)
#    include <lzma.h>
#    define LZMA_BUF_SIZE 4096
struct lzmafile {
  lzma_stream str;                    /* codec stream descriptor */
  FILE *fp;                           /* backing file descriptor */
  char mode;                          /* access mode ('r' or 'w') */
  unsigned char rdbuf[LZMA_BUF_SIZE]; /* read buffer used by lzmaRead */
};
#  endif
void *lzma_open(const char *path, const char *mode);
int lzma_close(struct lzmafile *file);
long lzma_read(struct lzmafile *file, void *buf, size_t count);
long lzma_write(struct lzmafile *file, const void *buf, size_t count);
int lzma_printf(struct lzmafile *file, const char *format, ...);
int lzma_puts(const char *s, struct lzmafile *file);
int lzma_putc(int c, struct lzmafile *file);
char *lzma_gets(char *s, int size, struct lzmafile *file);
int lzma_eof(struct lzmafile *file);
long lzma_tell(struct lzmafile *file);
int lzma_seek(struct lzmafile *file, long offset, int whence);
void *UnpackLZMAOpen(char *filename);
char *fgetsLZMASkipComments(SDDS_DATASET *SDDS_dataset, char *s, int32_t slen, struct lzmafile *lzmafp, char skip_char);
char *fgetsLZMASkipCommentsResize(SDDS_DATASET *SDDS_dataset, char **s, int32_t *slen, struct lzmafile *lzmafp, char skip_char);
int32_t SDDS_LZMAWriteBinaryString(char *string, struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer);
char *SDDS_ReadNonNativeLZMABinaryString(struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer, int32_t skip);
int32_t SDDS_LZMAWriteNonNativeBinaryString(char *string, struct lzmafile *lzmafp, SDDS_FILEBUFFER *fBuffer);

#  define TABLE_LENGTH_INCREMENT 100

/* binary input/output routines */
extern int32_t SDDS_WriteBinaryArrays(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_WriteBinaryColumns(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_WriteNonNativeBinaryColumns(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_WriteBinaryParameters(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_WriteBinaryPage(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_WriteBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row);
extern int32_t SDDS_UpdateBinaryPage(SDDS_DATASET *SDDS_dataset, uint32_t mode);
extern int32_t SDDS_UpdateNonNativeBinaryPage(SDDS_DATASET *SDDS_dataset, uint32_t mode);
extern int32_t SDDS_fseek(FILE *fp, int64_t offset, int32_t dir);
#  if defined(zLib)
extern int32_t SDDS_gzseek(gzFile gzfp, int64_t offset, int32_t dir);
#  endif

extern char *SDDS_ReadBinaryString(FILE *fp, SDDS_FILEBUFFER *fBuffer, int32_t skip);
extern int32_t SDDS_ReadBinaryArrays(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_ReadBinaryColumns(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset);
extern int32_t SDDS_ReadNonNativeBinaryColumns(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_ReadBinaryParameters(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_ReadBinaryPage(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset, int32_t sparse_statistics);
extern int32_t SDDS_ReadBinaryPageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows);
extern int32_t SDDS_ReadBinaryPageDetailed(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset, int64_t last_rows, int32_t sparse_statistics);
extern int32_t SDDS_ReadBinaryRow(SDDS_DATASET *SDDS_dataset, int64_t row, int32_t skip);
extern int32_t SDDS_ReadNonNativePageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows);
extern int32_t SDDS_ReadNonNativeBinaryPageDetailed(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset, int64_t last_rows);
extern int32_t SDDS_ReadNonNativePageDetailed(SDDS_DATASET *SDDS_dataset, uint32_t mode, int64_t sparse_interval, int64_t sparse_offset, int64_t last_rows);
extern int32_t SDDS_ReadNonNativeBinaryPageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows);

extern int32_t SDDS_AllocateColumnFlags(SDDS_DATASET *SDDS_target);

/* ascii input/output routines */
extern int32_t SDDS_WriteAsciiArrays(SDDS_DATASET *SDDS_dataset, FILE *fp);
extern int32_t SDDS_WriteAsciiParameters(SDDS_DATASET *SDDS_dataset, FILE *fp);
extern void SDDS_AppendParameterComment(PARAMETER_DEFINITION *definition, char *text, char *string);
extern int32_t SDDS_WriteAsciiRow(SDDS_DATASET *SDDS_dataset, int64_t row, FILE *fp);
extern int32_t SDDS_WriteAsciiPage(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_ReadAsciiArrays(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_ReadAsciiParameters(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_AsciiDataExpected(SDDS_DATASET *SDDS_dataset);
extern int32_t SDDS_ReadAsciiPageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows);
extern int32_t SDDS_ReadAsciiPageDetailed(SDDS_DATASET *SDDS_dataset, int64_t sparse_interval, int64_t sparse_offset, int64_t last_rows, int32_t sparse_statistics);

extern int32_t SDDS_LZMAWriteAsciiParameters(SDDS_DATASET *SDDS_dataset, struct lzmafile *lzmafp);
extern int32_t SDDS_LZMAWriteAsciiArrays(SDDS_DATASET *SDDS_dataset, struct lzmafile *lzmafp);
extern int32_t SDDS_LZMAWriteAsciiRow(SDDS_DATASET *SDDS_dataset, int64_t row, struct lzmafile *lzmafp);
#  if defined(zLib)
extern int32_t SDDS_GZipWriteAsciiParameters(SDDS_DATASET *SDDS_dataset, gzFile gzfp);
extern int32_t SDDS_GZipWriteAsciiArrays(SDDS_DATASET *SDDS_dataset, gzFile gzfp);
extern int32_t SDDS_GZipWriteAsciiRow(SDDS_DATASET *SDDS_dataset, int64_t row, gzFile gzfp);
#  endif

/* routines from SDDS_extract.c : */
extern int32_t SDDS_CopyColumn(SDDS_DATASET *SDDS_dataset, int32_t target, int32_t source);
extern int32_t SDDS_CopyParameter(SDDS_DATASET *SDDS_dataset, int32_t target, int32_t source);
extern int32_t SDDS_TransferRow(SDDS_DATASET *SDDS_dataset, int64_t target, int64_t source);
extern int64_t SDDS_GetSelectedRowIndex(SDDS_DATASET *SDDS_dataset, int64_t srow_index);

/* routines from SDDS_utils.c : */
extern int32_t SDDS_CheckTable(SDDS_DATASET *SDDS_dataset, const char *caller);
extern int32_t SDDS_AdvanceCounter(int32_t *counter, int32_t *max_count, int32_t n_indices);
extern void SDDS_FreePointerArray(void **data, int32_t dimensions, int32_t *dimension);

/* routines from SDDS_output.c : */
extern int32_t SDDS_WriteVersion(int32_t version_number, FILE *fp);
extern int32_t SDDS_WriteDescription(char *description, char *contents, FILE *fp);
extern int32_t SDDS_WriteColumnDefinition(COLUMN_DEFINITION *column, FILE *fp);
extern int32_t SDDS_WriteParameterDefinition(PARAMETER_DEFINITION *parameter, FILE *fp);
extern int32_t SDDS_WriteAssociateDefinition(ASSOCIATE_DEFINITION *associate, FILE *fp);
extern int32_t SDDS_WriteArrayDefinition(ARRAY_DEFINITION *array_definition, FILE *fp);
extern int32_t SDDS_WriteDataMode(SDDS_LAYOUT *layout, FILE *fp);
extern int32_t SDDS_LZMAWriteVersion(int32_t version_number, struct lzmafile *lzmafp);
extern int32_t SDDS_LZMAWriteDescription(char *description, char *contents, struct lzmafile *lzmafp);
extern int32_t SDDS_LZMAWriteColumnDefinition(COLUMN_DEFINITION *column, struct lzmafile *lzmafp);
extern int32_t SDDS_LZMAWriteParameterDefinition(PARAMETER_DEFINITION *parameter, struct lzmafile *lzmafp);
extern int32_t SDDS_LZMAWriteAssociateDefinition(ASSOCIATE_DEFINITION *associate, struct lzmafile *lzmafp);
extern int32_t SDDS_LZMAWriteArrayDefinition(ARRAY_DEFINITION *array_definition, struct lzmafile *lzmafp);
extern int32_t SDDS_LZMAWriteDataMode(SDDS_LAYOUT *layout, struct lzmafile *lzmafp);
#  if defined(zLib)
extern int32_t SDDS_GZipWriteVersion(int32_t version_number, gzFile gzfp);
extern int32_t SDDS_GZipWriteDescription(char *description, char *contents, gzFile gzfp);
extern int32_t SDDS_GZipWriteColumnDefinition(COLUMN_DEFINITION *column, gzFile gzfp);
extern int32_t SDDS_GZipWriteParameterDefinition(PARAMETER_DEFINITION *parameter, gzFile gzfp);
extern int32_t SDDS_GZipWriteAssociateDefinition(ASSOCIATE_DEFINITION *associate, gzFile gzfp);
extern int32_t SDDS_GZipWriteArrayDefinition(ARRAY_DEFINITION *array_definition, gzFile gzfp);
extern int32_t SDDS_GZipWriteDataMode(SDDS_LAYOUT *layout, gzFile gzfp);
#  endif

/* -- routines for routing data processing to routines for appropriate version */
extern int32_t SDDS_ProcessDescription(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS_ProcessColumnDefinition(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS_ProcessParameterDefinition(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS_ProcessArrayDefinition(SDDS_DATASET *SDDS_dataset, char *s);
extern FILE *SDDS_ProcessIncludeCommand(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS_ProcessAssociateDefinition(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS_ProcessDataMode(SDDS_DATASET *SDDS_dataset, char *s);

/* -- protocol version 1 routines */
extern int32_t SDDS1_ProcessDescription(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS1_ProcessColumnDefinition(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS1_ProcessParameterDefinition(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS1_ProcessArrayDefinition(SDDS_DATASET *SDDS_dataset, char *s);
extern FILE *SDDS1_ProcessIncludeCommand(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS1_ProcessAssociateDefinition(SDDS_DATASET *SDDS_dataset, char *s);
extern int32_t SDDS1_ProcessDataMode(SDDS_DATASET *SDDS_dataset, char *s);

/* internal input routines */
extern int32_t SDDS_ReadLayout(SDDS_DATASET *SDDS_dataset, FILE *fp);
extern int32_t SDDS_LZMAReadLayout(SDDS_DATASET *SDDS_dataset, struct lzmafile *lzmafp);
extern int32_t SDDS_UpdateAsciiPage(SDDS_DATASET *SDDS_dataset, uint32_t mode);
#  if defined(zLib)
extern int32_t SDDS_GZipReadLayout(SDDS_DATASET *SDDS_dataset, gzFile gzfp);
#  endif

#  define SDDS_DESCRIPTION_COMMAND 0
#  define SDDS_COLUMN_COMMAND 1
#  define SDDS_PARAMETER_COMMAND 2
#  define SDDS_ASSOCIATE_COMMAND 3
#  define SDDS_DATA_COMMAND 4
#  define SDDS_INCLUDE_COMMAND 5
#  define SDDS_ARRAY_COMMAND 6
#  define SDDS_NUM_COMMANDS 7

extern char *SDDS_command[SDDS_NUM_COMMANDS];

uint32_t SDDS_GetSpecialCommentsModes(SDDS_DATASET *SDDS_dataset);
void SDDS_ResetSpecialCommentsModes(SDDS_DATASET *SDDS_dataset);
void SDDS_ParseSpecialComments(SDDS_DATASET *SDDS_dataset, char *s);

void SDDS_FreeTableStrings(SDDS_DATASET *SDDS_dataset);

#endif
