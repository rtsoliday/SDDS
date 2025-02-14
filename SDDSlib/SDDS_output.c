/**
 * @file SDDS_output.c
 * @brief This file contains the implementation of the SDDS output routines.
 *
 * This file provides functions for outputting data in the
 * Self-Describing Data Sets (SDDS) format. It includes functions for
 * creating and writing SDDS files, as well as functions for defining
 * and appending data to the SDDS files.
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

#if defined(_WIN32)
#  include <fcntl.h>
#  include <io.h>
#  if defined(__BORLANDC__)
#    define _setmode(handle, amode) setmode(handle, amode)
#  endif
#else
#  include <unistd.h>
#endif

#if SDDS_VERSION != 5
#  error "SDDS_VERSION does not match version of this file"
#endif

#undef DEBUG

/* Allows "temporarily" closing a file.  Use SDDS_ReconnectFile() to open it
 * again in the same position.  Updates the present page and flushes the
 * table.
 */

#if SDDS_MPI_IO
int32_t SDDS_MPI_DisconnectFile(SDDS_DATASET *SDDS_dataset);
int32_t SDDS_MPI_ReconnectFile(SDDS_DATASET *SDDS_dataset);
#endif

/**
 * @brief Disconnects the SDDS dataset from its associated file.
 *
 * This function terminates the connection between the SDDS dataset and the file it is currently linked to. It ensures that all pending data is flushed to the file, closes the file handle, and updates the dataset's internal state to reflect that it is no longer connected to any file.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure to be disconnected.
 *
 * @return 
 *   - @c 1 on successful disconnection.
 *   - @c 0 if an error occurred during disconnection. In this case, an error message is set internally.
 *
 * @note 
 *   - If the dataset is already disconnected, this function will return an error.
 *   - This function is not thread-safe if the dataset is being accessed concurrently.
 *
 * @warning 
 *   - Ensure that no further operations are performed on the dataset after disconnection unless it is reconnected.
 */
int32_t SDDS_DisconnectFile(SDDS_DATASET *SDDS_dataset) {
#if SDDS_MPI_IO
  if (SDDS_dataset->parallel_io)
    return SDDS_MPI_DisconnectFile(SDDS_dataset);
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DisconnectFile"))
    return 0;
  if (!SDDS_dataset->layout.filename) {
    SDDS_SetError("Can't disconnect file. No filename given. (SDDS_DisconnectFile)");
    return 0;
  }
  if (SDDS_dataset->layout.gzipFile) {
    SDDS_SetError("Can't disconnect file because it is a gzip file. (SDDS_DisconnectFile)");
    return 0;
  }
  if (SDDS_dataset->layout.lzmaFile) {
    SDDS_SetError("Can't disconnect file because it is a lzma or xz file. (SDDS_DisconnectFile)");
    return 0;
  }
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't disconnect file.  Already disconnected. (SDDS_DisconnectFile)");
    return 0;
  }
  if (SDDS_dataset->page_started && !SDDS_UpdatePage(SDDS_dataset, FLUSH_TABLE)) {
    SDDS_SetError("Can't disconnect file.  Problem updating page. (SDDS_DisconnectFile)");
    return 0;
  }
  if (fclose(SDDS_dataset->layout.fp)) {
    SDDS_SetError("Can't disconnect file.  Problem closing file. (SDDS_DisconnectFile)");
    return 0;
  }
  SDDS_dataset->layout.disconnected = 1;
  return 1;
}

/**
 * @brief Reconnects the SDDS dataset to its previously associated file.
 *
 * This function re-establishes the connection between the SDDS dataset and the file it was previously linked to before being disconnected. It opens the file in read/write mode, seeks to the appropriate position, and updates the dataset's internal state to reflect that it is connected.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure to be reconnected.
 *
 * @return 
 *   - @c 1 on successful reconnection.
 *   - @c 0 if an error occurred during reconnection. In this case, an error message is set internally.
 *
 * @pre
 *   - The dataset must have been previously disconnected using SDDS_DisconnectFile.
 *   - The dataset must have a valid filename set.
 *
 * @note
 *   - Reconnection will fail if the file is not accessible or if the dataset was not properly disconnected.
 *
 * @warning
 *   - Ensure that the file has not been modified externally in a way that could disrupt the dataset's state.
 */
int32_t SDDS_ReconnectFile(SDDS_DATASET *SDDS_dataset) {
#if SDDS_MPI_IO
  if (SDDS_dataset->parallel_io)
    return SDDS_MPI_ReconnectFile(SDDS_dataset);
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReconnectFile"))
    return 0;
  if (!SDDS_dataset->layout.disconnected || !SDDS_dataset->layout.filename) {
    SDDS_SetError("Can't reconnect file.  Not disconnected or missing filename. (SDDS_ReconnectFile)");
    return 0;
  }
  if (!(SDDS_dataset->layout.fp = fopen(SDDS_dataset->layout.filename, FOPEN_READ_AND_WRITE_MODE))) {
    char s[1024];
    sprintf(s, "Unable to open file %s (SDDS_ReconnectFile)", SDDS_dataset->layout.filename);
    SDDS_SetError(s);
    return 0;
  }
  if (fseek(SDDS_dataset->layout.fp, 0, 2) == -1) {
    SDDS_SetError("Can't reconnect file.  Fseek failed. (SDDS_ReconnectFile)");
    return 0;
  }
  SDDS_dataset->original_layout.fp = SDDS_dataset->layout.fp;
  SDDS_dataset->layout.disconnected = 0;
  return 1;
}

/**
 * @brief Disconnects the input file from the SDDS dataset.
 *
 * This function severs the connection between the SDDS dataset and its input file. It closes the file handle, updates the dataset's internal state to indicate disconnection, and returns the current file position before closing. After disconnection, the dataset cannot read further data from the input file until it is reconnected.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure whose input file is to be disconnected.
 *
 * @return 
 *   - On success, returns the current file position (as obtained by @c ftell) before disconnection.
 *   - On failure, returns @c -1 and sets an internal error message.
 *
 * @note
 *   - This function cannot disconnect compressed input files (gzip, lzma, xz).
 *   - Attempting to disconnect an already disconnected dataset will result in an error.
 *
 * @warning
 *   - Ensure that no further read operations are performed on the dataset after disconnection unless it is reconnected.
 */
long SDDS_DisconnectInputFile(SDDS_DATASET *SDDS_dataset) {
  long position;
#if SDDS_MPI_IO
  if (SDDS_dataset->parallel_io) {
    SDDS_SetError("Error: MPI mode not supported yet in SDDS_DisconnectInputFile");
    return -1;
  }
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DisconnectInputFile"))
    return -1;
  if (!SDDS_dataset->layout.filename) {
    SDDS_SetError("Can't disconnect file. No filename given. (SDDS_DisconnectInputFile)");
    return -1;
  }
  if (SDDS_dataset->layout.gzipFile) {
    SDDS_SetError("Can't disconnect file because it is a gzip file. (SDDS_DisconnectInputFile)");
    return -1;
  }
  if (SDDS_dataset->layout.lzmaFile) {
    SDDS_SetError("Can't disconnect file because it is a lzma or xz file. (SDDS_DisconnectInputFile)");
    return -1;
  }
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't disconnect file.  Already disconnected. (SDDS_DisconnectInputFile)");
    return -1;
  }
  position = ftell(SDDS_dataset->layout.fp);
  if (fclose(SDDS_dataset->layout.fp)) {
    SDDS_SetError("Can't disconnect file.  Problem closing file. (SDDS_DisconnectInputFile)");
    return -1;
  }
  SDDS_dataset->layout.disconnected = 1;
  return position;
}

/**
 * @brief Reconnects the input file for the SDDS dataset at a specified position.
 *
 * This function re-establishes the connection between the SDDS dataset and its input file, positioning the file pointer at the specified byte offset. This allows the dataset to resume reading from a specific location within the input file.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure to reconnect.
 * @param[in] position The byte offset position in the input file where reconnection should occur.
 *
 * @return 
 *   - @c 1 on successful reconnection.
 *   - @c 0 on failure. In this case, an internal error message is set.
 *
 * @pre
 *   - The dataset must have been previously disconnected using SDDS_DisconnectInputFile.
 *   - The dataset must have a valid filename set.
 *   - The specified position must be valid within the input file.
 *
 * @note
 *   - Reconnection will fail if the input file is compressed (gzip, lzma, xz).
 *   - The function seeks to the specified position after opening the file.
 *
 * @warning
 *   - Ensure that the specified position does not disrupt the dataset's data integrity.
 */
int32_t SDDS_ReconnectInputFile(SDDS_DATASET *SDDS_dataset, long position) {
#if SDDS_MPI_IO
  if (SDDS_dataset->parallel_io) {
    SDDS_SetError("Error: MPI mode not supported yet in SDDS_ReconnectInputFile");
    return 0;
  }
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReconnectInputFile"))
    return 0;
  if (!SDDS_dataset->layout.disconnected || !SDDS_dataset->layout.filename) {
    SDDS_SetError("Can't reconnect file.  Not disconnected or missing filename. (SDDS_ReconnectInputFile)");
    return 0;
  }
  if (!(SDDS_dataset->layout.fp = fopen(SDDS_dataset->layout.filename, FOPEN_READ_MODE))) {
    char s[1024];
    sprintf(s, "Unable to open file %s (SDDS_ReconnectInputFile)", SDDS_dataset->layout.filename);
    SDDS_SetError(s);
    return 0;
  }
  if (fseek(SDDS_dataset->layout.fp, position, SEEK_SET) == -1) {
    SDDS_SetError("Can't reconnect file.  Fseek failed. (SDDS_ReconnectInputFile)");
    return 0;
  }
  SDDS_dataset->original_layout.fp = SDDS_dataset->layout.fp;
  SDDS_dataset->layout.disconnected = 0;
  return 1;
}

/* appends to a file by adding a new page */

/**
 * @brief Initializes the SDDS dataset for appending data by adding a new page to an existing file.
 *
 * This function prepares the SDDS dataset for appending additional data to an existing SDDS file by initializing necessary data structures, verifying file integrity, and setting up for the addition of a new data page. It ensures that the file is writable, not compressed, and properly locked to prevent concurrent modifications.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure to be initialized for appending.
 * @param[in] filename The name of the existing SDDS file to which data will be appended. If @c NULL, data will be appended to standard input.
 *
 * @return 
 *   - @c 1 on successful initialization.
 *   - @c 0 on error. In this case, an internal error message is set describing the failure.
 *
 * @pre
 *   - The specified file must exist and be a valid SDDS file.
 *   - The file must not be compressed (gzip, lzma, xz) and must be accessible for read and write operations.
 *
 * @post
 *   - The dataset is ready to append data as a new page.
 *   - The file is locked to prevent concurrent writes.
 *
 * @note
 *   - If @c filename is @c NULL, the dataset will append data from standard input.
 *   - The function sets internal flags indicating whether the file was previously empty or had existing data.
 *
 * @warning
 *   - Appending to a compressed file is not supported and will result in an error.
 *   - Ensure that no other processes are accessing the file simultaneously to avoid conflicts.
 */
int32_t SDDS_InitializeAppend(SDDS_DATASET *SDDS_dataset, const char *filename) {
  /*  char *ptr, *datafile, *headerfile; */
  char s[SDDS_MAXLINE];
  int64_t endOfLayoutOffset, endOfFileOffset;
  char *extension;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_InitializeAppend"))
    return 0;
  if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET))) {
    sprintf(s, "Unable to initialize input for file %s--can't zero SDDS_DATASET structure (SDDS_InitializeAppend)", filename);
    SDDS_SetError(s);
    return 0;
  }
  SDDS_dataset->layout.popenUsed = SDDS_dataset->layout.gzipFile = SDDS_dataset->layout.lzmaFile = SDDS_dataset->layout.disconnected = 0;
  SDDS_dataset->layout.depth = SDDS_dataset->layout.data_command_seen = SDDS_dataset->layout.commentFlags = SDDS_dataset->deferSavingLayout = 0;
  if (!filename)
    SDDS_dataset->layout.filename = NULL;
  else if (!SDDS_CopyString(&SDDS_dataset->layout.filename, filename)) {
    sprintf(s, "Memory allocation failure initializing file %s (SDDS_InitializeAppend)", filename);
    SDDS_SetError(s);
    return 0;
  } else if ((extension = strrchr(filename, '.')) && ((strcmp(extension, ".gz") == 0) || (strcmp(extension, ".lzma") == 0) || (strcmp(extension, ".xz") == 0))) {
    sprintf(s, "Cannot append to a compressed file %s (SDDS_InitializeAppend)", filename);
    SDDS_SetError(s);
    return 0;
  }

  SDDS_dataset->layout.popenUsed = 0;
  if (!filename) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
      sprintf(s, "unable to set stdin to binary mode");
      SDDS_SetError(s);
      return 0;
    }
#endif
    SDDS_dataset->layout.fp = stdin;
  } else {
    if (SDDS_FileIsLocked(filename)) {
      sprintf(s, "unable to open file %s for appending--file is locked (SDDS_InitializeAppend)", filename);
      SDDS_SetError(s);
      return 0;
    }
    if (!(SDDS_dataset->layout.fp = fopen(filename, FOPEN_READ_AND_WRITE_MODE))) {
      sprintf(s, "Unable to open file %s for appending (SDDS_InitializeAppend)", filename);
      SDDS_SetError(s);
      return 0;
    }
    if (!SDDS_LockFile(SDDS_dataset->layout.fp, filename, "SDDS_InitializeAppend"))
      return 0;
  }

  if (!SDDS_ReadLayout(SDDS_dataset, SDDS_dataset->layout.fp))
    return 0;
  endOfLayoutOffset = ftell(SDDS_dataset->layout.fp);
  if (SDDS_dataset->layout.n_columns &&
      (!(SDDS_dataset->column_flag = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_dataset->layout.n_columns)) ||
       !(SDDS_dataset->column_order = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_dataset->layout.n_columns)) ||
       !SDDS_SetMemory(SDDS_dataset->column_flag, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)1, (int32_t)0) ||
       !SDDS_SetMemory(SDDS_dataset->column_order, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)0, (int32_t)1))) {
    SDDS_SetError("Unable to initialize input--memory allocation failure (SDDS_InitializeAppend)");
    return 0;
  }
  if (fseek(SDDS_dataset->layout.fp, 0, 2) == -1) {
    SDDS_SetError("Unable to initialize append--seek failure (SDDS_InitializeAppend)");
    return 0;
  }
  endOfFileOffset = ftell(SDDS_dataset->layout.fp);
  if (endOfFileOffset == endOfLayoutOffset)
    SDDS_dataset->file_had_data = 0; /* appending to empty file */
  else
    SDDS_dataset->file_had_data = 1;       /* appending to nonempty file */
  SDDS_dataset->layout.layout_written = 1; /* its already in the file */
  SDDS_dataset->mode = SDDS_WRITEMODE;     /*writing */
  return 1;
}

/**
 * @brief Initializes the SDDS dataset for appending data to the last page of an existing file.
 *
 * This function sets up the SDDS dataset to append additional data rows to the last page of an existing SDDS file. It reads the existing file layout, determines the current state of data (including row counts), and prepares internal data structures to accommodate new data. The function also handles file locking, buffer management, and ensures that the file is ready for efficient data appending based on the specified update interval.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure to be initialized for appending.
 * @param[in] filename The name of the existing SDDS file to which data will be appended. If @c NULL, data will be appended to standard input.
 * @param[in] updateInterval The number of rows to write before the dataset reallocates memory or flushes data. This parameter controls the frequency of memory allocation and disk I/O operations during the append process.
 * @param[out] rowsPresentReturn Pointer to an @c int64_t variable where the function will store the number of rows present in the dataset after initialization. This provides information on the current dataset size.
 *
 * @return 
 *   - @c 1 on successful initialization.
 *   - @c 0 on error. In this case, an internal error message is set detailing the issue.
 *
 * @pre
 *   - The specified file must exist and be a valid SDDS file.
 *   - The file must not be compressed (gzip, lzma, xz) and must be accessible for read and write operations.
 *
 * @post
 *   - The dataset is configured to append data to the last page of the file.
 *   - Internal structures are initialized to track row counts and manage memory efficiently based on the update interval.
 *   - The file is locked to prevent concurrent modifications.
 *   - @c rowsPresentReturn is updated with the current number of rows in the dataset.
 *
 * @note
 *   - If @c filename is @c NULL, data will be appended from standard input.
 *   - The function sets internal flags indicating whether the file already contained data prior to appending.
 *
 * @warning
 *   - Appending to a compressed file is not supported and will result in an error.
 *   - Ensure that no other processes are accessing the file simultaneously to avoid conflicts.
 */
int32_t SDDS_InitializeAppendToPage(SDDS_DATASET *SDDS_dataset, const char *filename, int64_t updateInterval, int64_t *rowsPresentReturn) {
  /*  char *ptr, *datafile, *headerfile; */
  char s[SDDS_MAXLINE];
  int64_t endOfLayoutOffset, endOfFileOffset, rowCountOffset, offset;
  int32_t rowsPresent32;
  int64_t rowsPresent;
  char *extension;
  int32_t previousBufferSize;

  *rowsPresentReturn = -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_InitializeAppendToPage"))
    return 0;
  if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET))) {
    sprintf(s, "Unable to initialize input for file %s--can't zero SDDS_DATASET structure (SDDS_InitializeAppendToPage)", filename);
    SDDS_SetError(s);
    return 0;
  }
  SDDS_dataset->layout.popenUsed = SDDS_dataset->layout.gzipFile = SDDS_dataset->layout.lzmaFile = SDDS_dataset->layout.disconnected = 0;
  SDDS_dataset->layout.depth = SDDS_dataset->layout.data_command_seen = SDDS_dataset->layout.commentFlags = SDDS_dataset->deferSavingLayout = 0;
  if (!filename)
    SDDS_dataset->layout.filename = NULL;
  else if (!SDDS_CopyString(&SDDS_dataset->layout.filename, filename)) {
    sprintf(s, "Memory allocation failure initializing file %s (SDDS_InitializeAppendToPage)", filename);
    SDDS_SetError(s);
    return 0;
  } else if ((extension = strrchr(filename, '.')) && ((strcmp(extension, ".gz") == 0) || (strcmp(extension, ".lzma") == 0) || (strcmp(extension, ".xz") == 0))) {
    sprintf(s, "Cannot append to a compressed file %s (SDDS_InitializeAppendToPage)", filename);
    SDDS_SetError(s);
    return 0;
  }

  if (!filename) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
      sprintf(s, "unable to set stdin to binary mode");
      SDDS_SetError(s);
      return 0;
    }
#endif
    SDDS_dataset->layout.fp = stdin;
  } else {
    if (SDDS_FileIsLocked(filename)) {
      sprintf(s, "unable to open file %s for appending--file is locked (SDDS_InitializeAppendToPage)", filename);
      SDDS_SetError(s);
      return 0;
    }
    if (!(SDDS_dataset->layout.fp = fopen(filename, FOPEN_READ_AND_WRITE_MODE))) {
      sprintf(s, "Unable to open file %s for appending (SDDS_InitializeAppendToPage)", filename);
      SDDS_SetError(s);
      return 0;
    }
    if (!SDDS_LockFile(SDDS_dataset->layout.fp, filename, "SDDS_InitializeAppendToPage")) {
      return 0;
    }
  }

  if (!SDDS_ReadLayout(SDDS_dataset, SDDS_dataset->layout.fp)) {
    return 0;
  }
  endOfLayoutOffset = ftell(SDDS_dataset->layout.fp);
  if (SDDS_dataset->layout.n_columns &&
      (!(SDDS_dataset->column_flag = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_dataset->layout.n_columns)) ||
       !(SDDS_dataset->column_order = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_dataset->layout.n_columns)) ||
       !SDDS_SetMemory(SDDS_dataset->column_flag, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)1, (int32_t)0) ||
       !SDDS_SetMemory(SDDS_dataset->column_order, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)0, (int32_t)1))) {
    SDDS_SetError("Unable to initialize input--memory allocation failure (SDDS_InitializeAppendToPage)");
    return 0;
  }
  rowCountOffset = -1;
  rowsPresent = 0;
#ifdef DEBUG
  fprintf(stderr, "Data mode is %s\n", SDDS_data_mode[SDDS_dataset->layout.data_mode.mode - 1]);
#endif
  SDDS_dataset->pagecount_offset = NULL;
  previousBufferSize = SDDS_SetDefaultIOBufferSize(0);
  if (!SDDS_dataset->layout.data_mode.no_row_counts) {
    /* read pages to get to the last page */
    while (SDDS_ReadPageSparse(SDDS_dataset, 0, 10000, 0, 0) > 0) {
      rowCountOffset = SDDS_dataset->rowcount_offset;
      offset = ftell(SDDS_dataset->layout.fp);
      fseek(SDDS_dataset->layout.fp, rowCountOffset, 0);

      if (SDDS_dataset->layout.data_mode.mode == SDDS_BINARY) {
        if (fread(&rowsPresent32, sizeof(rowsPresent32), 1, SDDS_dataset->layout.fp) == 0) {
          SDDS_SetError("Error: row count not present or not correct length");
          return 0;
        }
        if (SDDS_dataset->swapByteOrder) {
          SDDS_SwapLong(&rowsPresent32);
        }
        if (rowsPresent32 == INT32_MIN) {
          if (fread(&rowsPresent, sizeof(rowsPresent), 1, SDDS_dataset->layout.fp) == 0) {
            SDDS_SetError("Error: row count not present or not correct length");
            return 0;
          }
          if (SDDS_dataset->swapByteOrder) {
            SDDS_SwapLong64(&rowsPresent);
          }
        } else {
          rowsPresent = rowsPresent32;
        }
      } else {
        char buffer[30];
        if (!fgets(buffer, 30, SDDS_dataset->layout.fp) || strlen(buffer) != 21 || sscanf(buffer, "%" SCNd64, &rowsPresent) != 1) {
#ifdef DEBUG
          fprintf(stderr, "buffer for row count data: >%s<\n", buffer);
#endif
          SDDS_SetError("Unable to initialize input--row count not present or not correct length (SDDS_InitializeAppendToPage)");
          SDDS_SetDefaultIOBufferSize(previousBufferSize);
          return 0;
        }
      }
      fseek(SDDS_dataset->layout.fp, offset, 0);
#ifdef DEBUG
      fprintf(stderr, "%" PRId64 " rows present\n", rowsPresent);
#endif
    }
    if (rowCountOffset == -1) {
      SDDS_SetDefaultIOBufferSize(previousBufferSize);
      SDDS_SetError("Unable to initialize input--problem finding row count offset (SDDS_InitializeAppendToPage)");
      return 0;
    }
  }
  SDDS_SetDefaultIOBufferSize(previousBufferSize);
  SDDS_dataset->fBuffer.bytesLeft = SDDS_dataset->fBuffer.bufferSize;

#ifdef DEBUG
  fprintf(stderr, "Starting page with %" PRId64 " rows\n", updateInterval);
#endif
  if (!SDDS_StartPage(SDDS_dataset, updateInterval)) {
    SDDS_SetError("Unable to initialize input--problem starting page (SDDS_InitializeAppendToPage)");
    return 0;
  }

  /* seek to the end of the file */
  if (fseek(SDDS_dataset->layout.fp, 0, 2) == -1) {
    SDDS_SetError("Unable to initialize append--seek failure (SDDS_InitializeAppendToPage)");
    return 0;
  }
  endOfFileOffset = ftell(SDDS_dataset->layout.fp);
  if (endOfFileOffset == endOfLayoutOffset)
    SDDS_dataset->file_had_data = 0; /* appending to empty file */
  else {
    SDDS_dataset->file_had_data = 1; /* appending to nonempty file */
    if (rowCountOffset != -1) {
      SDDS_dataset->rowcount_offset = rowCountOffset;
      SDDS_dataset->n_rows_written = rowsPresent;
      SDDS_dataset->first_row_in_mem = rowsPresent;
      SDDS_dataset->last_row_written = -1;
      *rowsPresentReturn = rowsPresent;
      SDDS_dataset->writing_page = 1;
    }
  }
#ifdef DEBUG
  fprintf(stderr, "rowcount_offset = %" PRId64 ", n_rows_written = %" PRId64 ", first_row_in_mem = %" PRId64 ", last_row_written = %" PRId64 "\n", SDDS_dataset->rowcount_offset, SDDS_dataset->n_rows_written, SDDS_dataset->first_row_in_mem, SDDS_dataset->last_row_written);
#endif
  SDDS_dataset->page_number = 1;
  SDDS_dataset->layout.layout_written = 1; /* its already in the file */
  SDDS_dataset->mode = SDDS_WRITEMODE;     /*writing */
  return 1;
}

/**
 * @brief Initializes the SDDS output dataset.
 *
 * This function sets up the SDDS dataset for output operations by initializing the necessary structures,
 * configuring the data mode (ASCII, Binary, or Parallel), handling file opening (including compressed files),
 * and setting dataset metadata such as description and contents. It ensures that the dataset is ready
 * for writing data according to the specified parameters.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure to be initialized for output.
 * @param[in] data_mode The data mode for the output dataset. Acceptable values are:
 *                     - @c SDDS_ASCII: ASCII text format.
 *                     - @c SDDS_BINARY: Binary format.
 *                     - @c SDDS_PARALLEL: Parallel processing mode.
 * @param[in] lines_per_row The number of lines per row in the output dataset. This parameter is used
 *                           only for ASCII output and is typically set to 1.
 * @param[in] description A string containing the description of the output dataset. Pass @c NULL
 *                        if no description is desired.
 * @param[in] contents A string detailing the contents of the output dataset. Pass @c NULL if no contents are desired.
 * @param[in] filename The name of the file to which the dataset will be written. If @c NULL, the dataset
 *                     will be written to standard output.
 *
 * @return 
 *   - @c 1 on successful initialization.
 *   - @c 0 if an error occurred during initialization. In this case, an error message is set internally.
 *
 * @pre
 *   - The @c SDDS_dataset pointer must be valid and point to a properly allocated SDDS_DATASET structure.
 *
 * @post
 *   - The dataset is configured for output according to the specified parameters.
 *   - The output file is opened and locked if a filename is provided.
 *   - The dataset's internal state reflects the initialization status.
 *
 * @note
 *   - When using compressed file formats (e.g., .gz, .lzma, .xz), the output mode is forced to binary.
 *   - Environment variable @c SDDS_OUTPUT_ENDIANESS can be set to "big" or "little" to declare the byte order.
 *   - For ASCII output, ensure that @c lines_per_row is set appropriately to match the data structure.
 *
 * @warning
 *   - Appending to compressed files is not supported and will result in an error.
 *   - Ensure that the specified file is not locked by another process to avoid initialization failures.
 *   - Changing data mode after initialization is not supported and may lead to undefined behavior.
 */
int32_t SDDS_InitializeOutput(SDDS_DATASET *SDDS_dataset, int32_t data_mode, int32_t lines_per_row, const char *description, const char *contents, const char *filename) {
  char s[SDDS_MAXLINE];
  char *extension;
  char *outputEndianess = NULL;

  if (data_mode == SDDS_PARALLEL)
    return SDDS_Parallel_InitializeOutput(SDDS_dataset, description, contents, filename);

  if (sizeof(gzFile) != sizeof(void *)) {
    SDDS_SetError("gzFile is not the same size as void *, possible corruption of the SDDS_LAYOUT structure");
    return (0);
  }
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_InitializeOutput"))
    return 0;
  if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET))) {
    sprintf(s, "Unable to initialize output for file %s--can't zero SDDS_DATASET structure (SDDS_InitializeOutput)", filename);
    SDDS_SetError(s);
    return 0;
  }
  SDDS_dataset->layout.popenUsed = SDDS_dataset->layout.gzipFile = SDDS_dataset->layout.lzmaFile = SDDS_dataset->layout.disconnected = 0;
  SDDS_dataset->layout.depth = SDDS_dataset->layout.data_command_seen = SDDS_dataset->layout.commentFlags = SDDS_dataset->deferSavingLayout = 0;
  if (!filename) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
      sprintf(s, "unable to set stdout to binary mode");
      SDDS_SetError(s);
      return 0;
    }
#endif
    SDDS_dataset->layout.fp = stdout;
  } else {
    if (SDDS_FileIsLocked(filename)) {
      sprintf(s, "unable to open file %s for writing--file is locked (SDDS_InitializeOutput)", filename);
      SDDS_SetError(s);
      return 0;
    }
    if ((extension = strrchr(filename, '.')) && ((strcmp(extension, ".xz") == 0) || (strcmp(extension, ".lzma") == 0))) {
      SDDS_dataset->layout.lzmaFile = 1;
      data_mode = SDDS_BINARY; /* force binary mode for output lzma files. The reading of ascii lzma files is flaky because of the lzma_gets command, plus the output files will be much smaller */
      if (!(SDDS_dataset->layout.lzmafp = lzma_open(filename, FOPEN_WRITE_MODE))) {
        sprintf(s, "Unable to open file %s for writing (SDDS_InitializeOutput)", filename);
        SDDS_SetError(s);
        return 0;
      }
      SDDS_dataset->layout.fp = SDDS_dataset->layout.lzmafp->fp;
    } else {
      if (!(SDDS_dataset->layout.fp = fopen(filename, FOPEN_WRITE_MODE))) {
        sprintf(s, "Unable to open file %s for writing (SDDS_InitializeOutput)", filename);
        SDDS_SetError(s);
        return 0;
      }
    }
    if (!SDDS_LockFile(SDDS_dataset->layout.fp, filename, "SDDS_InitializeOutput"))
      return 0;
#if defined(zLib)
    if ((extension = strrchr(filename, '.')) && (strcmp(extension, ".gz") == 0)) {
      SDDS_dataset->layout.gzipFile = 1;
      if ((SDDS_dataset->layout.gzfp = gzdopen(fileno(SDDS_dataset->layout.fp), FOPEN_WRITE_MODE)) == NULL) {
        sprintf(s, "Unable to open compressed file %s for writing (SDDS_InitializeOutput)", filename);
        SDDS_SetError(s);
        return 0;
      }
    }
#endif
  }
  SDDS_dataset->page_number = SDDS_dataset->page_started = 0;
  SDDS_dataset->file_had_data = SDDS_dataset->layout.layout_written = 0;
  if (!filename)
    SDDS_dataset->layout.filename = NULL;
  else if (!SDDS_CopyString(&SDDS_dataset->layout.filename, filename)) {
    sprintf(s, "Memory allocation failure initializing file %s (SDDS_InitializeOutput)", filename);
    SDDS_SetError(s);
    return 0;
  }
  if ((outputEndianess = getenv("SDDS_OUTPUT_ENDIANESS"))) {
    if (strncmp(outputEndianess, "big", 3) == 0)
      SDDS_dataset->layout.byteOrderDeclared = SDDS_BIGENDIAN;
    else if (strncmp(outputEndianess, "little", 6) == 0)
      SDDS_dataset->layout.byteOrderDeclared = SDDS_LITTLEENDIAN;
  } else {
    SDDS_dataset->layout.byteOrderDeclared = SDDS_IsBigEndianMachine() ? SDDS_BIGENDIAN : SDDS_LITTLEENDIAN;
  }

  if (data_mode < 0 || data_mode > SDDS_NUM_DATA_MODES) {
    sprintf(s, "Invalid data mode for file %s (SDDS_InitializeOutput)", filename ? filename : "stdout");
    SDDS_SetError(s);
    return 0;
  }
  if (data_mode == SDDS_ASCII && lines_per_row <= 0) {
    sprintf(s, "Invalid number of lines per row for file %s (SDDS_InitializeOutput)", filename ? filename : "stdout");
    SDDS_SetError(s);
    return 0;
  }
  SDDS_dataset->layout.version = SDDS_VERSION;
  SDDS_dataset->layout.data_mode.mode = data_mode;
  SDDS_dataset->layout.data_mode.lines_per_row = lines_per_row;
  SDDS_dataset->layout.data_mode.no_row_counts = 0;
  SDDS_dataset->layout.data_mode.fixed_row_count = 0;
  SDDS_dataset->layout.data_mode.fsync_data = 0;
  SDDS_dataset->layout.data_mode.column_memory_mode = DEFAULT_COLUMN_MEMORY_MODE;
  /*This is only temporary, soon the default will be column major order */
  SDDS_dataset->layout.data_mode.column_major = 0;
  if (description && !SDDS_CopyString(&SDDS_dataset->layout.description, description)) {
    sprintf(s, "Memory allocation failure initializing file %s (SDDS_InitializeOutput)", filename ? filename : "stdout");
    SDDS_SetError(s);
    return 0;
  }
  if (contents && !SDDS_CopyString(&SDDS_dataset->layout.contents, contents)) {
    sprintf(s, "Memory allocation failure initializing file %s (SDDS_InitializeOutput)", filename ? filename : "stdout");
    SDDS_SetError(s);
    return 0;
  }
  SDDS_dataset->mode = SDDS_WRITEMODE; /*writing */
  SDDS_dataset->pagecount_offset = NULL;
  SDDS_dataset->parallel_io = 0;
  return (1);
}

/**
 * @brief Initializes the SDDS output dataset for parallel processing.
 *
 * This function configures the SDDS dataset for parallel output operations. It sets the dataset's
 * description, contents, and filename, ensuring that the output is in binary mode as parallel
 * processing with compressed files is not supported. The function initializes necessary structures
 * and prepares the dataset for efficient parallel data writing.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure to be initialized for parallel output.
 * @param[in] description A string containing the description of the dataset. Pass @c NULL if no description is desired.
 * @param[in] contents A string detailing the contents of the dataset. Pass @c NULL if no contents are desired.
 * @param[in] filename The name of the file to which the dataset will be written. If @c NULL, the dataset
 *                     will be written to standard output.
 *
 * @return 
 *   - @c 1 on successful initialization.
 *   - @c 0 if an error occurred during initialization. In this case, an error message is set internally.
 *
 * @pre
 *   - The @c SDDS_dataset pointer must be valid and point to a properly allocated SDDS_DATASET structure.
 *   - The dataset memory should have been zeroed prior to calling this function (handled externally).
 *
 * @post
 *   - The dataset is configured for parallel binary output.
 *   - The dataset's internal state reflects the initialization status.
 *
 * @note
 *   - Parallel output does not support compressed file formats.
 *   - The output mode is set to binary regardless of the specified data mode.
 *   - Environment variable @c SDDS_OUTPUT_ENDIANESS can be set to "big" or "little" to declare the byte order.
 *
 * @warning
 *   - Attempting to use parallel initialization with compressed files will result in an error.
 *   - Ensure that no other processes are accessing the file simultaneously to prevent initialization failures.
 */
int32_t SDDS_Parallel_InitializeOutput(SDDS_DATASET *SDDS_dataset, const char *description, const char *contents, const char *filename) {
  /*  SDDS_DATASET *SDDS_dataset; */
  char s[SDDS_MAXLINE];
  char *outputEndianess = NULL;

  /* SDDS_dataset = &(MPI_dataset->sdds_dataset); */
  if (sizeof(gzFile) != sizeof(void *)) {
    SDDS_SetError("gzFile is not the same size as void *, possible corruption of the SDDS_LAYOUT structure");
    return (0);
  }
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_InitializeOutput"))
    return 0;
  /* if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET))) {
     sprintf(s,
     "Unable to initialize output for file %s--can't zero SDDS_DATASET structure (SDDS_InitializeOutput)",
     filename);
     SDDS_SetError(s);
     return 0;
     } */
  /*the sdds dataset memory has been zeroed in the SDDS_MPI_Setup */
  SDDS_dataset->layout.popenUsed = SDDS_dataset->layout.gzipFile = SDDS_dataset->layout.lzmaFile = SDDS_dataset->layout.disconnected = 0;
  SDDS_dataset->layout.depth = SDDS_dataset->layout.data_command_seen = SDDS_dataset->layout.commentFlags = SDDS_dataset->deferSavingLayout = 0;
  SDDS_dataset->layout.fp = NULL;

  SDDS_dataset->page_number = SDDS_dataset->page_started = 0;
  SDDS_dataset->file_had_data = SDDS_dataset->layout.layout_written = 0;
  if (!filename)
    SDDS_dataset->layout.filename = NULL;
  else if (!SDDS_CopyString(&SDDS_dataset->layout.filename, filename)) {
    sprintf(s, "Memory allocation failure initializing file %s (SDDS_InitializeOutput)", filename);
    SDDS_SetError(s);
    return 0;
  }
  if ((outputEndianess = getenv("SDDS_OUTPUT_ENDIANESS"))) {
    if (strncmp(outputEndianess, "big", 3) == 0)
      SDDS_dataset->layout.byteOrderDeclared = SDDS_BIGENDIAN;
    else if (strncmp(outputEndianess, "little", 6) == 0)
      SDDS_dataset->layout.byteOrderDeclared = SDDS_LITTLEENDIAN;
  } else {
    SDDS_dataset->layout.byteOrderDeclared = SDDS_IsBigEndianMachine() ? SDDS_BIGENDIAN : SDDS_LITTLEENDIAN;
  }
  /* set big-endian for binary files, since it is the only type of MPI binary file.
     SDDS_dataset->layout.byteOrderDeclared = SDDS_BIGENDIAN; */
  SDDS_dataset->layout.version = SDDS_VERSION;
  /* it turned out that hard to write ascii file in parallel, fixed it as SDDS_BINARY */
  SDDS_dataset->layout.data_mode.mode = SDDS_BINARY;
  SDDS_dataset->layout.data_mode.lines_per_row = 0;
  SDDS_dataset->layout.data_mode.no_row_counts = 0;
  SDDS_dataset->layout.data_mode.fixed_row_count = 0;
  SDDS_dataset->layout.data_mode.fsync_data = 0;
  SDDS_dataset->layout.data_mode.column_memory_mode = DEFAULT_COLUMN_MEMORY_MODE;
  /*This is only temporary, soon the default will be column major order */
  SDDS_dataset->layout.data_mode.column_major = 0;
  if (description && !SDDS_CopyString(&SDDS_dataset->layout.description, description)) {
    sprintf(s, "Memory allocation failure initializing file %s (SDDS_InitializeOutput)", filename ? filename : "stdout");
    SDDS_SetError(s);
    return 0;
  }
  if (contents && !SDDS_CopyString(&SDDS_dataset->layout.contents, contents)) {
    sprintf(s, "Memory allocation failure initializing file %s (SDDS_InitializeOutput)", filename ? filename : "stdout");
    SDDS_SetError(s);
    return 0;
  }
  SDDS_dataset->layout.n_parameters = SDDS_dataset->layout.n_columns = SDDS_dataset->layout.n_arrays = SDDS_dataset->layout.n_associates = 0;
  SDDS_dataset->mode = SDDS_WRITEMODE; /*writing */
  SDDS_dataset->pagecount_offset = NULL;
  SDDS_dataset->parallel_io = 1;
  return (1);
}

/**
 * @brief Sets the flag to enable or disable row counts in the SDDS dataset.
 *
 * This function configures the SDDS dataset to either include or exclude row counts in the output.
 * Row counts provide metadata about the number of rows written, which can be useful for data integrity
 * and validation. Disabling row counts can improve performance when such metadata is unnecessary.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure to be configured.
 * @param[in] value The flag value to set:
 *                  - @c 0: Enable row counts (default behavior).
 *                  - Non-zero: Disable row counts.
 *
 * @return 
 *   - @c 1 on successful configuration.
 *   - @c 0 if an error occurred (e.g., attempting to change the flag after the layout has been written).
 *
 * @pre
 *   - The @c SDDS_dataset must be initialized and not have written the layout yet.
 *
 * @post
 *   - The dataset's configuration reflects the specified row count setting.
 *
 * @note
 *   - Changing the row count setting affects how data rows are managed and stored in the output file.
 *
 * @warning
 *   - This function cannot be called after the dataset layout has been written to the file or if the dataset is in read mode.
 *   - Disabling row counts may complicate data validation and integrity checks.
 */
int32_t SDDS_SetNoRowCounts(SDDS_DATASET *SDDS_dataset, int32_t value) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetNoRowCounts"))
    return 0;
  if (SDDS_dataset->layout.layout_written) {
    SDDS_SetError("Can't change no_row_counts after writing the layout, or for a file you are reading.");
    return 0;
  }
  SDDS_dataset->layout.data_mode.no_row_counts = value ? 1 : 0;
  return 1;
}

/**
 * @brief Writes the SDDS layout header to the output file.
 *
 * This function serializes and writes the layout information of the SDDS dataset to the output file.
 * The layout defines the structure of the data tables, including parameters, arrays, columns, and
 * associates. The function handles different file types, including standard, gzip-compressed, and
 * LZMA-compressed files, and ensures that the layout is written in the correct byte order and format
 * based on the dataset's configuration.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure whose layout is to be written.
 *
 * @return 
 *   - @c 1 on successful writing of the layout.
 *   - @c 0 if an error occurred during the writing process. An internal error message is set in this case.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The layout must have been saved internally using SDDS_SaveLayout before calling this function.
 *   - The dataset must not be disconnected from the output file.
 *   - The layout must not have been previously written to the file.
 *
 * @post
 *   - The layout header is written to the output file in the appropriate format.
 *   - The dataset's internal state is updated to reflect that the layout has been written.
 *
 * @note
 *   - The function automatically determines the layout version based on the data types used in parameters, arrays, and columns.
 *   - Environment variable @c SDDS_OUTPUT_ENDIANESS can influence the byte order declared in the layout.
 *   - The function handles both binary and ASCII modes, adjusting the layout accordingly.
 *
 * @warning
 *   - Attempting to write the layout after it has already been written will result in an error.
 *   - The function does not support writing layouts to disconnected files.
 *   - Ensure that the output file is properly opened and writable before calling this function.
 */
int32_t SDDS_WriteLayout(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *layout;
#if defined(zLib)
  gzFile gzfp;
#endif
  FILE *fp;
  struct lzmafile *lzmafp;
  int64_t i;
  char *outputEndianess = NULL;

#if SDDS_MPI_IO
  if (SDDS_dataset->parallel_io)
    return SDDS_MPI_WriteLayout(SDDS_dataset);
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteLayout"))
    return 0;

  if (!SDDS_SaveLayout(SDDS_dataset))
    return 0;

  layout = &SDDS_dataset->layout;

  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't write layout--file is disconnected (SDDS_WriteLayout)");
    return 0;
  }

  if (layout->layout_written) {
    SDDS_SetError("Can't write layout--already written to file (SDDS_WriteLayout)");
    return 0;
  }

  if ((outputEndianess = getenv("SDDS_OUTPUT_ENDIANESS"))) {
    if (strncmp(outputEndianess, "big", 3) == 0)
      layout->byteOrderDeclared = SDDS_BIGENDIAN;
    else if (strncmp(outputEndianess, "little", 6) == 0)
      layout->byteOrderDeclared = SDDS_LITTLEENDIAN;
  }

  if (!layout->byteOrderDeclared)
    layout->byteOrderDeclared = SDDS_IsBigEndianMachine() ? SDDS_BIGENDIAN : SDDS_LITTLEENDIAN;

  layout->version = 1;
  for (i = 0; i < layout->n_parameters; i++) {
    if ((layout->parameter_definition[i].type == SDDS_ULONG) || (layout->parameter_definition[i].type == SDDS_USHORT)) {
      layout->version = 2;
      break;
    }
  }
  for (i = 0; i < layout->n_arrays; i++) {
    if ((layout->array_definition[i].type == SDDS_ULONG) || (layout->array_definition[i].type == SDDS_USHORT)) {
      layout->version = 2;
      break;
    }
  }
  for (i = 0; i < layout->n_columns; i++) {
    if ((layout->column_definition[i].type == SDDS_ULONG) || (layout->column_definition[i].type == SDDS_USHORT)) {
      layout->version = 2;
      break;
    }
  }
  if ((layout->data_mode.column_major) && (layout->data_mode.mode == SDDS_BINARY)) {
    layout->version = 3;
  }
  for (i = 0; i < layout->n_parameters; i++) {
    if (layout->parameter_definition[i].type == SDDS_LONGDOUBLE) {
      layout->version = 4;
      break;
    }
  }
  for (i = 0; i < layout->n_arrays; i++) {
    if (layout->array_definition[i].type == SDDS_LONGDOUBLE) {
      layout->version = 4;
      break;
    }
  }
  for (i = 0; i < layout->n_columns; i++) {
    if (layout->column_definition[i].type == SDDS_LONGDOUBLE) {
      layout->version = 4;
      break;
    }
  }
  if ((LDBL_DIG != 18) && (layout->version == 4)) {
    if (getenv("SDDS_LONGDOUBLE_64BITS") == NULL) {
      SDDS_SetError("Error: Operating system does not support 80bit float variables used by SDDS_LONGDOUBLE (SDDS_WriteLayout)\nSet SDDS_LONGDOUBLE_64BITS environment variable to read old files that used 64bit float variables for SDDS_LONGDOUBLE");
      return 0;
    }
  }
  for (i = 0; i < layout->n_parameters; i++) {
    if ((layout->parameter_definition[i].type == SDDS_ULONG64) || (layout->parameter_definition[i].type == SDDS_LONG64)) {
      layout->version = 5;
      break;
    }
  }
  for (i = 0; i < layout->n_arrays; i++) {
    if ((layout->array_definition[i].type == SDDS_ULONG64) || (layout->array_definition[i].type == SDDS_LONG64)) {
      layout->version = 5;
      break;
    }
  }
  for (i = 0; i < layout->n_columns; i++) {
    if ((layout->column_definition[i].type == SDDS_ULONG64) || (layout->column_definition[i].type == SDDS_LONG64)) {
      layout->version = 5;
      break;
    }
  }

  // force layout version 5 because the row and column indexes are now 64bit long integers
  // layout->version = 5;

#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!(gzfp = layout->gzfp)) {
      SDDS_SetError("Can't write SDDS layout--file pointer is NULL (SDDS_WriteLayout)");
      return 0;
    }

    /* write out the layout data */
    if (!SDDS_GZipWriteVersion(layout->version, gzfp)) {
      SDDS_SetError("Can't write SDDS layout--error writing version (SDDS_WriteLayout)");
      return 0;
    }
    if (layout->version < 3) {
      if (SDDS_dataset->layout.data_mode.mode == SDDS_BINARY) {
        if (layout->byteOrderDeclared == SDDS_BIGENDIAN)
          gzprintf(gzfp, "!# big-endian\n");
        else
          gzprintf(gzfp, "!# little-endian\n");
      }
      if (SDDS_dataset->layout.data_mode.fixed_row_count) {
        gzprintf(gzfp, "!# fixed-rowcount\n");
      }
    }
    if (!SDDS_GZipWriteDescription(layout->description, layout->contents, gzfp)) {
      SDDS_SetError("Can't write SDDS layout--error writing description (SDDS_WriteLayout)");
      return 0;
    }

    for (i = 0; i < layout->n_parameters; i++)
      if (!SDDS_GZipWriteParameterDefinition(layout->parameter_definition + i, gzfp)) {
        SDDS_SetError("Unable to write layout--error writing parameter definition (SDDS_WriteLayout)");
        return 0;
      }

    for (i = 0; i < layout->n_arrays; i++)
      if (!SDDS_GZipWriteArrayDefinition(layout->array_definition + i, gzfp)) {
        SDDS_SetError("Unable to write layout--error writing array definition (SDDS_WriteLayout)");
        return 0;
      }

    for (i = 0; i < layout->n_columns; i++)
      if (!SDDS_GZipWriteColumnDefinition(layout->column_definition + i, gzfp)) {
        SDDS_SetError("Unable to write layout--error writing column definition (SDDS_WriteLayout)");
        return 0;
      }

#  if RW_ASSOCIATES != 0
    for (i = 0; i < layout->n_associates; i++)
      if (!SDDS_GZipWriteAssociateDefinition(layout->associate_definition + i, gzfp)) {
        SDDS_SetError("Unable to write layout--error writing associated file data (SDDS_WriteLayout)");
        return 0;
      }
#  endif

    if (!SDDS_GZipWriteDataMode(layout, gzfp)) {
      SDDS_SetError("Unable to write layout--error writing data mode (SDDS_WriteLayout)");
      return 0;
    }

    layout->layout_written = 1;
    /*gzflush(gzfp, Z_FULL_FLUSH); */
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!(lzmafp = layout->lzmafp)) {
        SDDS_SetError("Can't write SDDS layout--file pointer is NULL (SDDS_WriteLayout)");
        return 0;
      }

      /* write out the layout data */
      if (!SDDS_LZMAWriteVersion(layout->version, lzmafp)) {
        SDDS_SetError("Can't write SDDS layout--error writing version (SDDS_WriteLayout)");
        return 0;
      }
      if (layout->version < 3) {
        if (SDDS_dataset->layout.data_mode.mode == SDDS_BINARY) {
          if (layout->byteOrderDeclared == SDDS_BIGENDIAN)
            lzma_printf(lzmafp, "!# big-endian\n");
          else
            lzma_printf(lzmafp, "!# little-endian\n");
        }
        if (SDDS_dataset->layout.data_mode.fixed_row_count) {
          lzma_printf(lzmafp, "!# fixed-rowcount\n");
        }
      }
      if (!SDDS_LZMAWriteDescription(layout->description, layout->contents, lzmafp)) {
        SDDS_SetError("Can't write SDDS layout--error writing description (SDDS_WriteLayout)");
        return 0;
      }
      for (i = 0; i < layout->n_parameters; i++)
        if (!SDDS_LZMAWriteParameterDefinition(layout->parameter_definition + i, lzmafp)) {
          SDDS_SetError("Unable to write layout--error writing parameter definition (SDDS_WriteLayout)");
          return 0;
        }
      for (i = 0; i < layout->n_arrays; i++)
        if (!SDDS_LZMAWriteArrayDefinition(layout->array_definition + i, lzmafp)) {
          SDDS_SetError("Unable to write layout--error writing array definition (SDDS_WriteLayout)");
          return 0;
        }
      for (i = 0; i < layout->n_columns; i++)
        if (!SDDS_LZMAWriteColumnDefinition(layout->column_definition + i, lzmafp)) {
          SDDS_SetError("Unable to write layout--error writing column definition (SDDS_WriteLayout)");
          return 0;
        }

#if RW_ASSOCIATES != 0
      for (i = 0; i < layout->n_associates; i++)
        if (!SDDS_LZMAWriteAssociateDefinition(layout->associate_definition + i, lzmafp)) {
          SDDS_SetError("Unable to write layout--error writing associated file data (SDDS_WriteLayout)");
          return 0;
        }
#endif

      if (!SDDS_LZMAWriteDataMode(layout, lzmafp)) {
        SDDS_SetError("Unable to write layout--error writing data mode (SDDS_WriteLayout)");
        return 0;
      }

      layout->layout_written = 1;
    } else {

      if (!(fp = layout->fp)) {
        SDDS_SetError("Can't write SDDS layout--file pointer is NULL (SDDS_WriteLayout)");
        return 0;
      }

      /* write out the layout data */
      if (!SDDS_WriteVersion(layout->version, fp)) {
        SDDS_SetError("Can't write SDDS layout--error writing version (SDDS_WriteLayout)");
        return 0;
      }
      if (layout->version < 3) {
        if (SDDS_dataset->layout.data_mode.mode == SDDS_BINARY) {
          if (layout->byteOrderDeclared == SDDS_BIGENDIAN)
            fprintf(fp, "!# big-endian\n");
          else
            fprintf(fp, "!# little-endian\n");
        }
        if (SDDS_dataset->layout.data_mode.fixed_row_count) {
          fprintf(fp, "!# fixed-rowcount\n");
        }
      }
      if (!SDDS_WriteDescription(layout->description, layout->contents, fp)) {
        SDDS_SetError("Can't write SDDS layout--error writing description (SDDS_WriteLayout)");
        return 0;
      }

      for (i = 0; i < layout->n_parameters; i++)
        if (!SDDS_WriteParameterDefinition(layout->parameter_definition + i, fp)) {
          SDDS_SetError("Unable to write layout--error writing parameter definition (SDDS_WriteLayout)");
          return 0;
        }

      for (i = 0; i < layout->n_arrays; i++)
        if (!SDDS_WriteArrayDefinition(layout->array_definition + i, fp)) {
          SDDS_SetError("Unable to write layout--error writing array definition (SDDS_WriteLayout)");
          return 0;
        }

      for (i = 0; i < layout->n_columns; i++)
        if (!SDDS_WriteColumnDefinition(layout->column_definition + i, fp)) {
          SDDS_SetError("Unable to write layout--error writing column definition (SDDS_WriteLayout)");
          return 0;
        }

#if RW_ASSOCIATES != 0
      for (i = 0; i < layout->n_associates; i++)
        if (!SDDS_WriteAssociateDefinition(layout->associate_definition + i, fp)) {
          SDDS_SetError("Unable to write layout--error writing associated file data (SDDS_WriteLayout)");
          return 0;
        }
#endif

      if (!SDDS_WriteDataMode(layout, fp)) {
        SDDS_SetError("Unable to write layout--error writing data mode (SDDS_WriteLayout)");
        return 0;
      }

      layout->layout_written = 1;
      fflush(fp);
    }
#if defined(zLib)
  }
#endif
  if (SDDS_SyncDataSet(SDDS_dataset) != 0)
    return 0;
  return (1);
}

/**
 * @brief Writes the current data table to the output file.
 *
 * This function serializes and writes the current data table of the SDDS dataset to the output file.
 * It must be preceded by a call to @c SDDS_WriteLayout to ensure that the dataset layout is properly defined
 * in the output file. Depending on the data mode (ASCII or Binary), the function delegates the writing
 * process to the appropriate handler.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 *
 * @return 
 *   - @c 1 on successful writing of the data table.
 *   - @c 0 if an error occurred during the write process. An error message is set internally in this case.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - @c SDDS_WriteLayout must have been called successfully before writing any pages.
 *
 * @post
 *   - The current data table is written to the output file.
 *   - The dataset state is synchronized with the file to ensure data integrity.
 *
 * @note
 *   - The function supports parallel I/O modes if enabled.
 *   - Ensure that the dataset is not disconnected from the output file before calling this function.
 *
 * @warning
 *   - Attempting to write a page without defining the layout first will result in an error.
 *   - Concurrent access to the dataset while writing pages may lead to undefined behavior.
 */
int32_t SDDS_WritePage(SDDS_DATASET *SDDS_dataset) {
  int32_t result;
#if SDDS_MPI_IO
  if (SDDS_dataset->parallel_io)
    return SDDS_MPI_WritePage(SDDS_dataset);
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WritePage"))
    return 0;
  if (!SDDS_dataset->layout.layout_written) {
    SDDS_SetError("Unable to write page--layout not written (SDDS_WritePage)");
    return 0;
  }
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't write page--file is disconnected (SDDS_WritePage)");
    return 0;
  }
  if (SDDS_dataset->layout.data_mode.mode == SDDS_ASCII)
    result = SDDS_WriteAsciiPage(SDDS_dataset);
  else if (SDDS_dataset->layout.data_mode.mode == SDDS_BINARY)
    result = SDDS_WriteBinaryPage(SDDS_dataset);
  else {
    SDDS_SetError("Unable to write page--unknown data mode (SDDS_WritePage)");
    return 0;
  }
  if (result == 1)
    if (SDDS_SyncDataSet(SDDS_dataset) != 0)
      return 0;
  return (result);
}

/**
 * @brief Updates the current page of the SDDS dataset.
 *
 * This function finalizes and writes the current page of the SDDS dataset based on the specified mode.
 * The mode can be either @c FLUSH_TABLE, indicating that the current page is complete and should be written to disk,
 * or @c 0 for other update operations. Depending on the data mode (ASCII or Binary), the function delegates
 * the update process to the appropriate handler.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] mode The update mode, which can be:
 *                 - @c FLUSH_TABLE: Indicates that the current page is complete and should be written to disk.
 *                 - @c 0: Represents a standard update without flushing the table.
 *
 * @return 
 *   - @c 1 on successful update of the current page.
 *   - @c 0 if an error occurred during the update process. An error message is set internally in this case.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - A page must have been started before calling this function.
 *
 * @post
 *   - The current page is updated and, if specified, written to the output file.
 *   - The dataset state is synchronized with the file to ensure data integrity.
 *
 * @note
 *   - The function supports parallel I/O modes if enabled.
 *   - The @c FLUSH_TABLE mode ensures that all buffered data is written to the disk, which can be useful for data integrity.
 *
 * @warning
 *   - Attempting to update a page without starting one will result in an error.
 *   - Concurrent access to the dataset while updating pages may lead to undefined behavior.
 */
int32_t SDDS_UpdatePage(SDDS_DATASET *SDDS_dataset, uint32_t mode) {
  int32_t result;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_UpdatePage"))
    return 0;
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't write page--file is disconnected (SDDS_UpdatePage)");
    return 0;
  }
  if (SDDS_dataset->page_started == 0) {
    SDDS_SetError("Can't update page--no page started (SDDS_UpdatePage)");
    return 0;
  }
  if (SDDS_dataset->layout.data_mode.mode == SDDS_ASCII)
    result = SDDS_UpdateAsciiPage(SDDS_dataset, mode);
  else if (SDDS_dataset->layout.data_mode.mode == SDDS_BINARY)
    result = SDDS_UpdateBinaryPage(SDDS_dataset, mode);
  else {
    SDDS_SetError("Unable to update page--unknown data mode (SDDS_UpdatePage)");
    return 0;
  }
  if (result == 1)
    if (SDDS_SyncDataSet(SDDS_dataset) != 0)
      return 0;
  return (result);
}

/**
 * @brief Synchronizes the SDDS dataset with the disk by flushing buffered data.
 *
 * This function attempts to ensure that any buffered data associated with the SDDS dataset is written
 * to the disk using the @c fsync system call. However, on certain platforms such as VxWorks, Windows,
 * Linux, and macOS, this functionality is not implemented and the function simply returns success.
 * This behavior should be considered when relying on data synchronization across different operating systems.
 *
 * @param[in] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 *
 * @return 
 *   - @c 0 on success, indicating that data synchronization is either not needed or was successful.
 *   - A negative value (e.g., @c -1) on failure to synchronize the data, with an error message set internally.
 *
 * @note
 *   - On unsupported platforms, the function does not perform any synchronization and returns success.
 *   - The synchronization behavior depends on the operating system and its support for the @c fsync system call.
 *
 * @warning
 *   - On platforms where synchronization is not implemented, relying on this function for data integrity is not possible.
 *   - Ensure that critical data is handled appropriately, considering the limitations of the target operating system.
 */
int32_t SDDS_SyncDataSet(SDDS_DATASET *SDDS_dataset) {
#if defined(vxWorks) || defined(_WIN32) || defined(linux) || defined(__APPLE__)
  return (0);
#else
  if (!(SDDS_dataset->layout.fp)) {
    SDDS_SetError("Unable to sync file--file pointer is NULL (SDDS_SyncDataSet)");
    return (-1);
  }
  if (SDDS_dataset->layout.data_mode.fsync_data == 0)
    return (0);
  if (fsync(fileno(SDDS_dataset->layout.fp)) == 0)
    return (0);
  /*
    SDDS_SetError("Unable to sync file (SDDS_SyncDataSet)");
    return(-1);
  */
  /* This error should not be fatal */
  return (0);
#endif
}

/**
 * @brief Defines a data parameter with a fixed numerical value.
 *
 * This function processes the definition of a data parameter within the SDDS dataset. It allows
 * the specification of a fixed numerical value for the parameter, which remains constant across
 * all data entries. The function validates the parameter name, type, and format string before
 * defining the parameter in the dataset.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] name A NULL-terminated string specifying the name of the parameter. This name must be unique within the dataset.
 * @param[in] symbol A NULL-terminated string specifying the symbol for the parameter. Pass @c NULL if no symbol is desired.
 * @param[in] units A NULL-terminated string specifying the units of the parameter. Pass @c NULL if no units are desired.
 * @param[in] description A NULL-terminated string providing a description of the parameter. Pass @c NULL if no description is desired.
 * @param[in] format_string A NULL-terminated string specifying the printf-style format for ASCII output. If @c NULL is passed, a default format is selected based on the parameter type.
 * @param[in] type An integer representing the data type of the parameter. Must be one of the following:
 *                  - @c SDDS_LONGDOUBLE
 *                  - @c SDDS_DOUBLE
 *                  - @c SDDS_FLOAT
 *                  - @c SDDS_LONG
 *                  - @c SDDS_ULONG
 *                  - @c SDDS_SHORT
 *                  - @c SDDS_USHORT
 *                  - @c SDDS_CHARACTER
 *                  - @c SDDS_STRING
 * @param[in] fixed_value A pointer to the numerical value that remains constant for this parameter across all data entries. This value is used to initialize the parameter's fixed value.
 *
 * @return 
 *   - On success, returns the index of the newly defined parameter within the dataset.
 *   - Returns @c -1 on failure, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The parameter name must be unique and valid.
 *   - The fixed value must be non-NULL for numerical types and should be prepared appropriately.
 *
 * @post
 *   - The parameter is defined within the dataset with the specified attributes and fixed value.
 *   - The dataset's internal structures are updated to include the new parameter.
 *
 * @note
 *   - For string-type parameters, the fixed value should be a NULL-terminated string.
 *   - The function internally converts the fixed numerical value to a string representation if the parameter type is not @c SDDS_STRING.
 *
 * @warning
 *   - Defining a parameter with an invalid type or format string will result in an error.
 *   - Passing a NULL fixed value for non-string types will result in an error.
 */
int32_t SDDS_DefineParameter1(SDDS_DATASET *SDDS_dataset, const char *name, const char *symbol, const char *units, const char *description, const char *format_string, int32_t type, void *fixed_value) {
  char buffer[SDDS_MAXLINE];
  if (!SDDS_IsValidName(name, "parameter"))
    return -1;
  if (!fixed_value || type == SDDS_STRING)
    return SDDS_DefineParameter(SDDS_dataset, name, symbol, units, description, format_string, type, fixed_value);
  if (type <= 0 || type > SDDS_NUM_TYPES) {
    SDDS_SetError("Unknown data type (SDDS_DefineParameter1)");
    return (-1);
  }
  buffer[SDDS_MAXLINE - 1] = 0;
  if (!SDDS_SprintTypedValue(fixed_value, 0, type, format_string, buffer, 0) || buffer[SDDS_MAXLINE - 1] != 0) {
    SDDS_SetError("Unable to define fixed value for parameter (SDDS_DefineParameter1)");
    return (-1);
  }
  return SDDS_DefineParameter(SDDS_dataset, name, symbol, units, description, format_string, type, buffer);
}

/**
 * @brief Defines a data parameter with a fixed string value.
 *
 * This function processes the definition of a data parameter within the SDDS dataset. It allows
 * the specification of a fixed string value for the parameter, which remains constant across
 * all data entries. The function validates the parameter name, type, and format string before
 * defining the parameter in the dataset.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] name A NULL-terminated string specifying the name of the parameter. This name must be unique within the dataset.
 * @param[in] symbol A NULL-terminated string specifying the symbol for the parameter. Pass @c NULL if no symbol is desired.
 * @param[in] units A NULL-terminated string specifying the units of the parameter. Pass @c NULL if no units are desired.
 * @param[in] description A NULL-terminated string providing a description of the parameter. Pass @c NULL if no description is desired.
 * @param[in] format_string A NULL-terminated string specifying the printf-style format for ASCII output. If @c NULL is passed, a default format is selected based on the parameter type.
 * @param[in] type An integer representing the data type of the parameter. Must be one of the following:
 *                  - @c SDDS_LONGDOUBLE
 *                  - @c SDDS_DOUBLE
 *                  - @c SDDS_FLOAT
 *                  - @c SDDS_LONG
 *                  - @c SDDS_ULONG
 *                  - @c SDDS_SHORT
 *                  - @c SDDS_USHORT
 *                  - @c SDDS_CHARACTER
 *                  - @c SDDS_STRING
 * @param[in] fixed_value A NULL-terminated string specifying the fixed value of the parameter. For non-string types, this string should be formatted appropriately using functions like @c sprintf.
 *
 * @return 
 *   - On success, returns the index of the newly defined parameter within the dataset.
 *   - Returns @c -1 on failure, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The parameter name must be unique and valid.
 *   - The fixed value must be a valid string representation for the specified parameter type.
 *
 * @post
 *   - The parameter is defined within the dataset with the specified attributes and fixed value.
 *   - The dataset's internal structures are updated to include the new parameter.
 *
 * @note
 *   - For numerical parameter types, the fixed value string should represent the numerical value correctly.
 *   - The function internally handles the conversion of the fixed value string to the appropriate type based on the parameter's data type.
 *
 * @warning
 *   - Defining a parameter with an invalid type or format string will result in an error.
 *   - Passing an improperly formatted fixed value string for the specified type may lead to unexpected behavior.
 */
int32_t SDDS_DefineParameter(SDDS_DATASET *SDDS_dataset, const char *name, const char *symbol, const char *units, const char *description, const char *format_string, int32_t type, char *fixed_value) {
  SDDS_LAYOUT *layout;
  PARAMETER_DEFINITION *definition;
  char s[SDDS_MAXLINE];
  SORTED_INDEX *new_indexed_parameter;
  int32_t index, duplicate;

  if (!SDDS_IsValidName(name, "parameter"))
    return -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DefineParameter"))
    return (-1);
  if (!name) {
    SDDS_SetError("NULL name not allowed for parameter definition");
    return (-1);
  }
  layout = &SDDS_dataset->layout;
  if (!(layout->parameter_definition =
          SDDS_Realloc(layout->parameter_definition, sizeof(*layout->parameter_definition) * (layout->n_parameters + 1))) ||
      !(layout->parameter_index = SDDS_Realloc(layout->parameter_index, sizeof(*layout->parameter_index) * (layout->n_parameters + 1))) || !(new_indexed_parameter = (SORTED_INDEX *)SDDS_Malloc(sizeof(*new_indexed_parameter)))) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineParameter)");
    return (-1);
  }
  if (!SDDS_CopyString(&new_indexed_parameter->name, name))
    return -1;
  index = binaryInsert((void **)layout->parameter_index, layout->n_parameters, new_indexed_parameter, SDDS_CompareIndexedNames, &duplicate);
  if (duplicate) {
    sprintf(s, "Parameter %s already exists (SDDS_DefineParameter)", name);
    SDDS_SetError(s);
    return (-1);
  }
  layout->parameter_index[index]->index = layout->n_parameters;

  if (!SDDS_ZeroMemory(definition = layout->parameter_definition + layout->n_parameters, sizeof(PARAMETER_DEFINITION))) {
    SDDS_SetError("Unable to define parameter--can't zero memory for parameter definition (SDDS_DefineParameter)");
    return (-1);
  }
  definition->name = new_indexed_parameter->name;
  if (symbol && !SDDS_CopyString(&definition->symbol, symbol)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineParameter)");
    return (-1);
  }
  if (units && !SDDS_CopyString(&definition->units, units)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineParameter)");
    return (-1);
  }
  if (description && !SDDS_CopyString(&definition->description, description)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineParameter)");
    return (-1);
  }
  if (type <= 0 || type > SDDS_NUM_TYPES) {
    SDDS_SetError("Unknown data type (SDDS_DefineParameter)");
    return (-1);
  }
  definition->type = type;
  if (format_string) {
    if (!SDDS_VerifyPrintfFormat(format_string, type)) {
      SDDS_SetError("Invalid format string (SDDS_DefineParameter)");
      return (-1);
    }
    if (!SDDS_CopyString(&definition->format_string, format_string)) {
      SDDS_SetError("Memory allocation failure (SDDS_DefineParameter)");
      return (-1);
    }
  }
  if (fixed_value && !SDDS_CopyString(&(definition->fixed_value), fixed_value)) {
    SDDS_SetError("Couldn't copy fixed_value string (SDDS_DefineParameter)");
    return (-1);
  }
  definition->definition_mode = SDDS_NORMAL_DEFINITION;
  if (type == SDDS_STRING)
    definition->memory_number = SDDS_CreateRpnMemory(name, 1);
  else
    definition->memory_number = SDDS_CreateRpnMemory(name, 0);
  layout->n_parameters += 1;
  return (layout->n_parameters - 1);
}

/**
 * @brief Defines a data array within the SDDS dataset.
 *
 * This function processes the definition of a data array in the SDDS dataset. It allows the user
 * to specify the array's name, symbol, units, description, format string, data type, field length,
 * number of dimensions, and associated group name. The function ensures that the array name is valid
 * and unique within the dataset before defining it.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] name A NULL-terminated string specifying the name of the array. This name must be unique within the dataset.
 * @param[in] symbol A NULL-terminated string specifying the symbol for the array. Pass @c NULL if no symbol is desired.
 * @param[in] units A NULL-terminated string specifying the units of the array. Pass @c NULL if no units are desired.
 * @param[in] description A NULL-terminated string providing a description of the array. Pass @c NULL if no description is desired.
 * @param[in] format_string A NULL-terminated string specifying the printf-style format for ASCII output. If @c NULL is passed, a default format is selected based on the array type.
 * @param[in] type An integer representing the data type of the array. Must be one of the following:
 *                  - @c SDDS_LONGDOUBLE
 *                  - @c SDDS_DOUBLE
 *                  - @c SDDS_FLOAT
 *                  - @c SDDS_LONG
 *                  - @c SDDS_ULONG
 *                  - @c SDDS_SHORT
 *                  - @c SDDS_USHORT
 *                  - @c SDDS_CHARACTER
 *                  - @c SDDS_STRING
 * @param[in] field_length An integer specifying the length of the field allotted to the array for ASCII output. If set to @c 0, the field length is ignored. If negative, the field length is set to the absolute value, and leading and trailing white-space are eliminated for @c SDDS_STRING types upon reading.
 * @param[in] dimensions An integer specifying the number of dimensions of the array. Must be greater than @c 0.
 * @param[in] group_name A NULL-terminated string specifying the name of the array group to which this array belongs. This allows related arrays to be grouped together (e.g., parallel arrays).
 *
 * @return 
 *   - On success, returns the index of the newly defined array within the dataset.
 *   - Returns @c -1 on failure, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The array name must be unique and valid.
 *   - The specified data type must be supported by the dataset.
 *
 * @post
 *   - The array is defined within the dataset with the specified attributes.
 *   - The dataset's internal structures are updated to include the new array.
 *
 * @note
 *   - For string-type arrays, the fixed value is managed differently, and leading/trailing white-space is handled based on the field length parameter.
 *   - The function supports multi-dimensional arrays as specified by the @c dimensions parameter.
 *
 * @warning
 *   - Defining an array with an invalid type, field length, or number of dimensions will result in an error.
 *   - Attempting to define an array with a name that already exists within the dataset will result in an error.
 */
int32_t SDDS_DefineArray(SDDS_DATASET *SDDS_dataset, const char *name, const char *symbol, const char *units, const char *description, const char *format_string, int32_t type, int32_t field_length, int32_t dimensions, const char *group_name) {
  SDDS_LAYOUT *layout;
  ARRAY_DEFINITION *definition;
  char s[SDDS_MAXLINE];
  SORTED_INDEX *new_indexed_array;
  int32_t index, duplicate;

  if (!SDDS_IsValidName(name, "array"))
    return -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DefineArray"))
    return (-1);
  if (!name) {
    SDDS_SetError("NULL name not allowed for array definition");
    return (-1);
  }
  layout = &SDDS_dataset->layout;
  if (!(layout->array_definition =
          SDDS_Realloc(layout->array_definition, sizeof(*layout->array_definition) * (layout->n_arrays + 1))) ||
      !(layout->array_index = SDDS_Realloc(layout->array_index, sizeof(*layout->array_index) * (layout->n_arrays + 1))) || !(new_indexed_array = (SORTED_INDEX *)SDDS_Malloc(sizeof(*new_indexed_array)))) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineArray)");
    return (-1);
  }

  if (!SDDS_CopyString(&new_indexed_array->name, name))
    return -1;
  index = binaryInsert((void **)layout->array_index, layout->n_arrays, new_indexed_array, SDDS_CompareIndexedNames, &duplicate);
  if (duplicate) {
    sprintf(s, "Array %s already exists (SDDS_DefineArray)", name);
    SDDS_SetError(s);
    return (-1);
  }
  layout->array_index[index]->index = layout->n_arrays;

  if (!SDDS_ZeroMemory(definition = layout->array_definition + layout->n_arrays, sizeof(ARRAY_DEFINITION))) {
    SDDS_SetError("Unable to define array--can't zero memory for array definition (SDDS_DefineArray)");
    return (-1);
  }
  definition->name = new_indexed_array->name;
  if ((symbol && !SDDS_CopyString(&definition->symbol, symbol)) || (units && !SDDS_CopyString(&definition->units, units)) || (description && !SDDS_CopyString(&definition->description, description)) || (group_name && !SDDS_CopyString(&definition->group_name, group_name))) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineArray)");
    return (-1);
  }
  if (type <= 0 || type > SDDS_NUM_TYPES) {
    SDDS_SetError("Unknown data type (SDDS_DefineArray)");
    return (-1);
  }
  definition->type = type;
  if (format_string) {
    if (!SDDS_VerifyPrintfFormat(format_string, type)) {
      SDDS_SetError("Invalid format string (SDDS_DefineArray)");
      return (-1);
    }
    if (!SDDS_CopyString(&definition->format_string, format_string)) {
      SDDS_SetError("Memory allocation failure (SDDS_DefineArray)");
      return (-1);
    }
  }
  if ((definition->field_length = field_length) < 0 && type != SDDS_STRING) {
    SDDS_SetError("Invalid field length (SDDS_DefineArray)");
    return (-1);
  }
  if ((definition->dimensions = dimensions) < 1) {
    SDDS_SetError("Invalid number of dimensions for array (SDDS_DefineArray)");
    return (-1);
  }
  layout->n_arrays += 1;
  return (layout->n_arrays - 1);
}

/**
 * @brief Defines a data column within the SDDS dataset.
 *
 * This function processes the definition of a data column in the SDDS dataset. It allows the user
 * to specify the column's name, symbol, units, description, format string, data type, and field length.
 * The function ensures that the column name is valid and unique within the dataset before defining it.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] name A NULL-terminated string specifying the name of the column. This name must be unique within the dataset.
 * @param[in] symbol A NULL-terminated string specifying the symbol for the column. Pass @c NULL if no symbol is desired.
 * @param[in] units A NULL-terminated string specifying the units of the column. Pass @c NULL if no units are desired.
 * @param[in] description A NULL-terminated string providing a description of the column. Pass @c NULL if no description is desired.
 * @param[in] format_string A NULL-terminated string specifying the printf-style format for ASCII output. If @c NULL is passed, a default format is selected based on the column type.
 * @param[in] type An integer representing the data type of the column. Must be one of the following:
 *                  - @c SDDS_LONGDOUBLE
 *                  - @c SDDS_DOUBLE
 *                  - @c SDDS_FLOAT
 *                  - @c SDDS_LONG
 *                  - @c SDDS_ULONG
 *                  - @c SDDS_SHORT
 *                  - @c SDDS_USHORT
 *                  - @c SDDS_CHARACTER
 *                  - @c SDDS_STRING
 * @param[in] field_length An integer specifying the length of the field allotted to the column for ASCII output. If set to @c 0, the field length is ignored. If negative, the field length is set to the absolute value, and leading and trailing white-space are eliminated for @c SDDS_STRING types upon reading.
 *
 * @return 
 *   - On success, returns the index of the newly defined column within the dataset.
 *   - Returns @c -1 on failure, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The column name must be unique and valid.
 *   - The specified data type must be supported by the dataset.
 *
 * @post
 *   - The column is defined within the dataset with the specified attributes.
 *   - The dataset's internal structures are updated to include the new column.
 *   - If rows have already been allocated, the data arrays are resized to accommodate the new column.
 *
 * @note
 *   - For string-type columns, the fixed value is managed differently, and leading/trailing white-space is handled based on the field length parameter.
 *   - The function ensures that data arrays are appropriately resized if data has already been allocated.
 *
 * @warning
 *   - Defining a column with an invalid type, field length, or name will result in an error.
 *   - Attempting to define a column with a name that already exists within the dataset will result in an error.
 *   - Memory allocation failures during the definition process will lead to an error.
 */
int32_t SDDS_DefineColumn(SDDS_DATASET *SDDS_dataset, const char *name, const char *symbol, const char *units, const char *description, const char *format_string, int32_t type, int32_t field_length) {
  SDDS_LAYOUT *layout;
  COLUMN_DEFINITION *definition;
  char s[SDDS_MAXLINE];
  SORTED_INDEX *new_indexed_column;
  int32_t index;
  int32_t duplicate;

  if (!SDDS_IsValidName(name, "column"))
    return -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DefineColumn"))
    return (-1);
  if (!name) {
    SDDS_SetError("NULL name not allowed for column definition");
    return (-1);
  }
  layout = &SDDS_dataset->layout;
  if (!(layout->column_definition =
          SDDS_Realloc(layout->column_definition, sizeof(*layout->column_definition) * (layout->n_columns + 1))) ||
      !(layout->column_index = SDDS_Realloc(layout->column_index, sizeof(*layout->column_index) * (layout->n_columns + 1))) || !(new_indexed_column = (SORTED_INDEX *)SDDS_Malloc(sizeof(*new_indexed_column)))) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineColumn)");
    return (-1);
  }
  if (!SDDS_CopyString(&new_indexed_column->name, name))
    return -1;
  index = binaryInsert((void **)layout->column_index, layout->n_columns, new_indexed_column, SDDS_CompareIndexedNames, &duplicate);
  if (duplicate) {
    sprintf(s, "Column %s already exists (SDDS_DefineColumn)", name);
    SDDS_SetError(s);
    return (-1);
  }
  layout->column_index[index]->index = layout->n_columns;
  if (!SDDS_ZeroMemory(definition = layout->column_definition + layout->n_columns, sizeof(COLUMN_DEFINITION))) {
    SDDS_SetError("Unable to define column--can't zero memory for column definition (SDDS_DefineColumn)");
    return (-1);
  }
  definition->name = new_indexed_column->name;
  if (symbol && !SDDS_CopyString(&definition->symbol, symbol)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineColumn)");
    return (-1);
  }
  if (units && !SDDS_CopyString(&definition->units, units)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineColumn)");
    return (-1);
  }
  if (description && !SDDS_CopyString(&definition->description, description)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineColumn)");
    return (-1);
  }
  if (type <= 0 || type > SDDS_NUM_TYPES) {
    SDDS_SetError("Unknown data type (SDDS_DefineColumn)");
    return (-1);
  }
  definition->type = type;
  if (format_string) {
    if (!SDDS_VerifyPrintfFormat(format_string, type)) {
      SDDS_SetError("Invalid format string (SDDS_DefineColumn)");
      return (-1);
    }
    if (!SDDS_CopyString(&definition->format_string, format_string)) {
      SDDS_SetError("Memory allocation failure (SDDS_DefineColumn)");
      return (-1);
    }
  }
  if ((definition->field_length = field_length) < 0 && type != SDDS_STRING) {
    SDDS_SetError("Invalid field length (SDDS_DefineColumn)");
    return (-1);
  }

  if (SDDS_dataset->n_rows_allocated) {
    if (!SDDS_dataset->data) {
      SDDS_SetError("data array NULL but rows have been allocated! (SDDS_DefineColumn)");
      return (-1);
    }
    /* data already present--must resize data and parameter memory */
    if (!(SDDS_dataset->data = SDDS_Realloc(SDDS_dataset->data, sizeof(*SDDS_dataset->data) * (layout->n_columns + 1))) || !(SDDS_dataset->data[layout->n_columns] = calloc(SDDS_dataset->n_rows_allocated, SDDS_type_size[type - 1]))) {
      SDDS_SetError("Memory allocation failure (SDDS_DefineColumn)");
      return (-1);
    }
  }

  /* not part of output: */
  definition->definition_mode = SDDS_NORMAL_DEFINITION;
  if (type == SDDS_STRING)
    definition->memory_number = SDDS_CreateRpnMemory(name, 1);
  else {
    definition->memory_number = SDDS_CreateRpnMemory(name, 0);
  }
  sprintf(s, "&%s", name);
  definition->pointer_number = SDDS_CreateRpnArray(s);

  layout->n_columns += 1;
  return (layout->n_columns - 1);
}

/**
 * @brief Defines a simple data column within the SDDS dataset.
 *
 * This function provides a simplified interface for defining a data column in the SDDS dataset.
 * It allows the user to specify only the column's name, units, and data type, while omitting optional
 * parameters such as symbol, description, format string, and field length. Internally, it calls
 * @c SDDS_DefineColumn with default values for the omitted parameters.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] name A NULL-terminated string specifying the name of the column. This name must be unique within the dataset.
 * @param[in] unit A NULL-terminated string specifying the units of the column. Pass @c NULL if no units are desired.
 * @param[in] type An integer representing the data type of the column. Must be one of the following:
 *                  - @c SDDS_LONGDOUBLE
 *                  - @c SDDS_DOUBLE
 *                  - @c SDDS_FLOAT
 *                  - @c SDDS_LONG
 *                  - @c SDDS_ULONG
 *                  - @c SDDS_SHORT
 *                  - @c SDDS_USHORT
 *                  - @c SDDS_CHARACTER
 *                  - @c SDDS_STRING
 *
 * @return 
 *   - @c 1 on successful definition of the column.
 *   - @c 0 on failure, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The column name must be unique and valid.
 *
 * @post
 *   - The column is defined within the dataset with the specified name, units, and type.
 *   - The dataset's internal structures are updated to include the new column.
 *
 * @note
 *   - This function is intended for scenarios where only basic column attributes are needed.
 *   - Optional parameters such as symbol, description, format string, and field length are set to default values.
 *
 * @warning
 *   - Defining a column with an invalid type or name will result in an error.
 *   - Attempting to define a column with a name that already exists within the dataset will result in an error.
 */
int32_t SDDS_DefineSimpleColumn(SDDS_DATASET *SDDS_dataset, const char *name, const char *unit, int32_t type) {
  if (SDDS_DefineColumn(SDDS_dataset, name, NULL, unit, NULL, NULL, type, 0) < 0)
    return 0;
  return (1);
}

/**
 * @brief Defines a simple data parameter within the SDDS dataset.
 *
 * This function provides a simplified interface for defining a data parameter in the SDDS dataset.
 * It allows the user to specify only the parameter's name, units, and data type, while omitting
 * optional attributes such as symbol, description, format string, and fixed value. Internally,
 * it calls @c SDDS_DefineParameter with default values for the omitted parameters.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] name A NULL-terminated string specifying the name of the parameter. This name must be unique within the dataset.
 * @param[in] unit A NULL-terminated string specifying the units of the parameter. Pass @c NULL if no units are desired.
 * @param[in] type An integer representing the data type of the parameter. Must be one of the following:
 *                  - @c SDDS_LONGDOUBLE
 *                  - @c SDDS_DOUBLE
 *                  - @c SDDS_FLOAT
 *                  - @c SDDS_LONG
 *                  - @c SDDS_ULONG
 *                  - @c SDDS_SHORT
 *                  - @c SDDS_USHORT
 *                  - @c SDDS_CHARACTER
 *                  - @c SDDS_STRING
 *
 * @return 
 *   - @c 1 on successful definition of the parameter.
 *   - @c 0 on failure, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The parameter name must be unique and valid.
 *
 * @post
 *   - The parameter is defined within the dataset with the specified name, units, and type.
 *   - The dataset's internal structures are updated to include the new parameter.
 *
 * @note
 *   - This function is intended for scenarios where only basic parameter attributes are needed.
 *   - Optional parameters such as symbol, description, format string, and fixed value are set to default values.
 *
 * @warning
 *   - Defining a parameter with an invalid type or name will result in an error.
 *   - Attempting to define a parameter with a name that already exists within the dataset will result in an error.
 */
int32_t SDDS_DefineSimpleParameter(SDDS_DATASET *SDDS_dataset, const char *name, const char *unit, int32_t type) {
  if (SDDS_DefineParameter(SDDS_dataset, name, NULL, unit, NULL, NULL, type, NULL) < 0)
    return 0;
  return (1);
}

/**
 * @brief Defines multiple simple data columns of the same data type within the SDDS dataset.
 *
 * This function provides a streamlined way to define multiple data columns in the SDDS dataset that share
 * the same data type. It allows the user to specify the names and units of the columns, while omitting
 * optional attributes such as symbol, description, format string, and field length. Internally, it calls
 * @c SDDS_DefineColumn for each column with default values for the omitted parameters.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] number The number of columns to define. Must be greater than or equal to @c 0.
 * @param[in] name An array of NULL-terminated strings specifying the names of the columns. Each name must be unique within the dataset.
 * @param[in] unit An array of NULL-terminated strings specifying the units of the columns. Pass @c NULL for elements where no units are desired.
 * @param[in] type An integer representing the data type for all the columns. Must be one of the following:
 *                  - @c SDDS_LONGDOUBLE
 *                  - @c SDDS_DOUBLE
 *                  - @c SDDS_FLOAT
 *                  - @c SDDS_LONG
 *                  - @c SDDS_ULONG
 *                  - @c SDDS_SHORT
 *                  - @c SDDS_USHORT
 *                  - @c SDDS_CHARACTER
 *                  - @c SDDS_STRING
 *
 * @return 
 *   - @c 1 on successful definition of all specified columns.
 *   - @c 0 on failure to define any of the columns, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The @c name array must contain unique and valid names for each column.
 *   - The @c type must be a supported data type.
 *
 * @post
 *   - All specified columns are defined within the dataset with the provided names and units.
 *   - The dataset's internal structures are updated to include the new columns.
 *
 * @note
 *   - Passing @c number as @c 0 results in no action and returns success.
 *   - This function is optimized for defining multiple columns of the same type, enhancing code readability and efficiency.
 *
 * @warning
 *   - Defining a column with an invalid type or name will result in an error.
 *   - Attempting to define a column with a name that already exists within the dataset will result in an error.
 *   - Ensure that the @c name and @c unit arrays are properly allocated and contain valid strings.
 */
int32_t SDDS_DefineSimpleColumns(SDDS_DATASET *SDDS_dataset, int32_t number, char **name, char **unit, int32_t type) {
  int32_t i;
  if (!number)
    return (1);
  if (!name)
    return 0;
  for (i = 0; i < number; i++)
    if (SDDS_DefineColumn(SDDS_dataset, name[i], NULL, unit ? unit[i] : NULL, NULL, NULL, type, 0) < 0)
      return 0;
  return (1);
}

/**
 * @brief Defines multiple simple data parameters of the same data type within the SDDS dataset.
 *
 * This function provides a streamlined way to define multiple data parameters in the SDDS dataset that share
 * the same data type. It allows the user to specify the names and units of the parameters, while omitting
 * optional attributes such as symbol, description, format string, and fixed value. Internally, it calls
 * @c SDDS_DefineParameter for each parameter with default values for the omitted parameters.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] number The number of parameters to define. Must be greater than or equal to @c 0.
 * @param[in] name An array of NULL-terminated strings specifying the names of the parameters. Each name must be unique within the dataset.
 * @param[in] unit An array of NULL-terminated strings specifying the units of the parameters. Pass @c NULL for elements where no units are desired.
 * @param[in] type An integer representing the data type for all the parameters. Must be one of the following:
 *                  - @c SDDS_LONGDOUBLE
 *                  - @c SDDS_DOUBLE
 *                  - @c SDDS_FLOAT
 *                  - @c SDDS_LONG
 *                  - @c SDDS_ULONG
 *                  - @c SDDS_SHORT
 *                  - @c SDDS_USHORT
 *                  - @c SDDS_CHARACTER
 *                  - @c SDDS_STRING
 *
 * @return 
 *   - @c 1 on successful definition of all specified parameters.
 *   - @c 0 on failure to define any of the parameters, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The @c name array must contain unique and valid names for each parameter.
 *   - The @c type must be a supported data type.
 *
 * @post
 *   - All specified parameters are defined within the dataset with the provided names and units.
 *   - The dataset's internal structures are updated to include the new parameters.
 *
 * @note
 *   - Passing @c number as @c 0 results in no action and returns success.
 *   - This function is optimized for defining multiple parameters of the same type, enhancing code readability and efficiency.
 *
 * @warning
 *   - Defining a parameter with an invalid type or name will result in an error.
 *   - Attempting to define a parameter with a name that already exists within the dataset will result in an error.
 *   - Ensure that the @c name and @c unit arrays are properly allocated and contain valid strings.
 */
int32_t SDDS_DefineSimpleParameters(SDDS_DATASET *SDDS_dataset, int32_t number, char **name, char **unit, int32_t type) {
  int32_t i;
  if (!number)
    return (1);
  if (!name)
    return 0;
  for (i = 0; i < number; i++)
    if (SDDS_DefineParameter(SDDS_dataset, name[i], NULL, unit ? unit[i] : NULL, NULL, NULL, type, NULL) < 0)
      return 0;
  return (1);
}

static uint32_t nameValidityFlags = 0;
/**
 * @brief Sets the validity flags for parameter and column names in the SDDS dataset.
 *
 * This function allows the user to configure the rules for validating names of parameters and columns
 * within the SDDS dataset. The validity flags determine the set of allowed characters and naming conventions.
 *
 * @param[in] flags A bitmask representing the desired name validity flags. Possible flags include:
 *                  - @c SDDS_ALLOW_ANY_NAME: Allows any name without restrictions.
 *                  - @c SDDS_ALLOW_V15_NAME: Enables compatibility with SDDS version 1.5 naming conventions.
 *                  - Additional flags as defined in the SDDS library.
 *
 * @return 
 *   - The previous name validity flags before the update.
 *
 * @pre
 *   - The function can be called at any time before defining parameters or columns to influence name validation.
 *
 * @post
 *   - The name validity flags are updated to reflect the specified rules.
 *
 * @note
 *   - Changing name validity flags affects how subsequent parameter and column names are validated.
 *   - It is recommended to set the desired validity flags before defining any dataset elements to avoid validation errors.
 *
 * @warning
 *   - Improperly setting validity flags may lead to unintended acceptance or rejection of valid or invalid names.
 *   - Ensure that the flags are set according to the desired naming conventions for your dataset.
 */
int32_t SDDS_SetNameValidityFlags(uint32_t flags) {
  uint32_t oldFlags;
  oldFlags = nameValidityFlags;
  nameValidityFlags = flags;
  return oldFlags;
}

/**
 * @brief Checks if a given name is valid for a specified class within the SDDS dataset.
 *
 * This function validates whether the provided name adheres to the naming conventions and rules
 * defined by the current name validity flags for the specified class (e.g., parameter, column).
 * It ensures that the name contains only allowed characters and follows the required structure.
 *
 * @param[in] name The name to be validated. Must be a NULL-terminated string.
 * @param[in] class The class type to which the name belongs (e.g., "parameter", "column"). This is used
 *                  primarily for error reporting.
 *
 * @return 
 *   - @c 1 if the name is valid for the specified class.
 *   - @c 0 if the name is invalid, with an error message set internally.
 *
 * @pre
 *   - The name must be a valid NULL-terminated string.
 *   - The class must be a valid NULL-terminated string representing a recognized class type.
 *
 * @post
 *   - If the name is invalid, an error message is recorded detailing the reason.
 *
 * @note
 *   - The validation rules are influenced by the current name validity flags set via @c SDDS_SetNameValidityFlags.
 *   - Environment variables or other configuration settings may also affect name validity.
 *
 * @warning
 *   - Using names that do not adhere to the validation rules will result in parameters or columns not being defined.
 *   - Ensure that all names meet the required standards before attempting to define dataset elements.
 */
int32_t SDDS_IsValidName(const char *name, const char *class) {
  char *ptr;
  int32_t isValid = 1;
  char s[SDDS_MAXLINE];
  static char *validChars = "@:#+%-._$&/[]";
  static char *startChars = ".:";

  if (nameValidityFlags & SDDS_ALLOW_ANY_NAME)
    return 1;
  ptr = (char *)name;
  if (strlen(name) == 0)
    isValid = 0;
  else if (!(nameValidityFlags & SDDS_ALLOW_V15_NAME)) {
    /* post V1.5 allows only alpha and startChars members as first character */
    /* V1.5 allows alpha, digits, and any validChars members */
    if (!(isalpha(*ptr) || strchr(startChars, *ptr)))
      isValid = 0;
  }
  while (isValid && *ptr) {
    if (!(isalnum(*ptr) || strchr(validChars, *ptr)))
      isValid = 0;
    ptr++;
  }
  if (!isValid) {
    sprintf(s, "The following %s name is invalid: >%s<\n(sddsconvert may be used to change the name)\n", class, name);
    SDDS_SetError(s);
    return 0;
  }
  return 1;
}

/**
 * @brief Defines an associate for the SDDS dataset.
 *
 * This function defines an associate for the SDDS dataset, allowing the association of additional
 * files or data with the primary dataset. Associates can provide supplementary information or link
 * related datasets together. The function sets up the necessary attributes such as name, filename,
 * path, description, contents, and SDDS flag to describe the associate.
 *
 * **Note:** This function is **NOT USED** in the current implementation and will always return @c 0
 * unless compiled with @c RW_ASSOCIATES defined.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] name A NULL-terminated string specifying the name of the associate. This name must be unique within the dataset.
 * @param[in] filename A NULL-terminated string specifying the filename of the associate. Must be a valid filename.
 * @param[in] path A NULL-terminated string specifying the path to the associate. Pass @c NULL if no path is desired.
 * @param[in] description A NULL-terminated string providing a description of the associate. Pass @c NULL if no description is desired.
 * @param[in] contents A NULL-terminated string detailing the contents of the associate. Pass @c NULL if no contents are desired.
 * @param[in] sdds An integer flag indicating the type of associate. Typically used to specify whether the associate is an SDDS file.
 *
 * @return 
 *   - On success, returns the index of the newly defined associate within the dataset.
 *   - Returns a negative value on failure, with an error message set internally.
 *   - Returns @c 0 if @c RW_ASSOCIATES is not defined.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - @c RW_ASSOCIATES must be defined during compilation to use this feature.
 *   - The associate name and filename must be unique and valid.
 *
 * @post
 *   - The associate is defined within the dataset with the specified attributes.
 *   - The dataset's internal structures are updated to include the new associate.
 *
 * @note
 *   - Associates provide a mechanism to link additional data or files to the primary SDDS dataset.
 *   - Properly defining associates can enhance data organization and accessibility.
 *
 * @warning
 *   - Defining an associate with an invalid type, name, or filename will result in an error.
 *   - Attempting to define an associate with a name that already exists within the dataset will result in an error.
 *   - Ensure that the @c filename and @c path (if provided) are valid and accessible.
 */
int32_t SDDS_DefineAssociate(SDDS_DATASET *SDDS_dataset, const char *name, const char *filename, const char *path, const char *description, const char *contents, int32_t sdds) {

#if RW_ASSOCIATES == 0
  return 0;
#else
  SDDS_LAYOUT *layout;
  ASSOCIATE_DEFINITION *definition;
  char s[SDDS_MAXLINE];
  if (!SDDS_IsValidName(name, "associate"))
    return -1;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_DefineAssociate"))
    return (-1);
  layout = &SDDS_dataset->layout;
  if (!(layout->associate_definition = SDDS_Realloc(layout->associate_definition, sizeof(*layout->associate_definition) * (layout->n_associates + 1)))) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineAssociate)");
    return (-1);
  }
  if (!name) {
    SDDS_SetError("NULL name not allowed for associate file (SDDS_DefineAssociate)");
    return (-1);
  }
  if (!filename) {
    SDDS_SetError("NULL filename not allowed for associate file (SDDS_DefineAssociate)");
    return (-1);
  }
  if (SDDS_GetAssociateIndex(SDDS_dataset, name) >= 0) {
    sprintf(s, "Associate with name %s already exists (SDDS_DefineAssociate)", name);
    SDDS_SetError(s);
    return (-1);
  }
  if (!SDDS_ZeroMemory(definition = layout->associate_definition + layout->n_associates, sizeof(ASSOCIATE_DEFINITION))) {
    SDDS_SetError("Unable to define associate--can't zero memory for associate (SDDS_DefineAssociate)");
    return (-1);
  }

  if (!SDDS_CopyString(&definition->name, name)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineAssociate)");
    return (-1);
  }
  if (!SDDS_CopyString(&definition->filename, filename)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineAssociate)");
    return (-1);
  }
  if (path && !SDDS_CopyString(&definition->path, path)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineAssociate)");
    return (-1);
  }
  if (contents && !SDDS_CopyString(&definition->contents, contents)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineAssociate)");
    return (-1);
  }
  if (description && !SDDS_CopyString(&definition->description, description)) {
    SDDS_SetError("Memory allocation failure (SDDS_DefineAssociate)");
    return (-1);
  }
  definition->sdds = sdds;
  layout->n_associates += 1;
  return (layout->n_associates - 1);
#endif
}

/**
 * @brief Erases all data entries in the SDDS dataset.
 *
 * This function removes all data from the specified SDDS dataset, effectively resetting it to an empty state.
 * It frees any allocated memory associated with data columns, parameters, and arrays, ensuring that
 * all dynamic data is properly cleared. This is useful for reusing the dataset for new data without
 * retaining previous entries.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 *
 * @return 
 *   - @c 1 on successful erasure of all data.
 *   - @c 0 on failure, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured.
 *
 * @post
 *   - All data rows are removed from the dataset.
 *   - Memory allocated for data columns, parameters, and arrays is freed.
 *   - The dataset is ready to accept new data entries.
 *
 * @note
 *   - This function does not alter the dataset's layout definitions; only the data entries are cleared.
 *   - After erasing data, the dataset can be reused to write new data tables without redefining the layout.
 *
 * @warning
 *   - Erasing data is irreversible; ensure that any necessary data is backed up before calling this function.
 *   - Concurrent access to the dataset while erasing data may lead to undefined behavior.
 */
int32_t SDDS_EraseData(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *layout;
  int64_t i, j;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_EraseData"))
    return 0;
  layout = &SDDS_dataset->layout;
  if (SDDS_dataset->data) {
    for (i = 0; i < layout->n_columns; i++) {
      if (!SDDS_dataset->data[i])
        continue;
      if (layout->column_definition[i].type == SDDS_STRING) {
        for (j = 0; j < SDDS_dataset->n_rows; j++) {
          if (((char **)SDDS_dataset->data[i])[j]) {
            free(((char **)SDDS_dataset->data[i])[j]);
            ((char **)SDDS_dataset->data[i])[j] = NULL;
          }
        }
      }
    }
  }
  SDDS_dataset->n_rows = 0;

  if (SDDS_dataset->parameter) {
    for (i = 0; i < layout->n_parameters; i++) {
      if (!SDDS_dataset->parameter[i])
        continue;
      if (layout->parameter_definition[i].type == SDDS_STRING && *(char **)(SDDS_dataset->parameter[i])) {
        free(*(char **)(SDDS_dataset->parameter[i]));
        *(char **)SDDS_dataset->parameter[i] = NULL;
      }
    }
  }

  if (SDDS_dataset->array) {
    for (i = 0; i < layout->n_arrays; i++) {
      if (SDDS_dataset->array[i].definition->type == SDDS_STRING) {
        for (j = 0; j < SDDS_dataset->array[i].elements; j++) {
          if (((char **)SDDS_dataset->array[i].data)[j]) {
            free(((char **)SDDS_dataset->array[i].data)[j]);
            ((char **)SDDS_dataset->array[i].data)[j] = NULL;
          }
        }
      }
    }
  }

  return (1);
}

/**
 * @brief Sets the row count mode for the SDDS dataset.
 *
 * This function configures how row counts are managed within the SDDS dataset. The row count mode
 * determines whether row counts are variable, fixed, or entirely omitted during data writing.
 * Proper configuration of row count modes can enhance data integrity and performance based on
 * specific use cases.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 * @param[in] mode The row count mode to be set. Must be one of the following:
 *                 - @c SDDS_VARIABLEROWCOUNT: Enables variable row counts, allowing the number of rows to vary.
 *                 - @c SDDS_FIXEDROWCOUNT: Sets a fixed row count mode, where the number of rows is constant.
 *                 - @c SDDS_NOROWCOUNT: Disables row counts, omitting them from the dataset.
 *
 * @return 
 *   - @c 1 on successful configuration of the row count mode.
 *   - @c 0 on failure, with an error message set internally.
 *
 * @pre
 *   - The dataset must be initialized and configured for output.
 *   - The layout must not have been written to the file yet.
 *
 * @post
 *   - The dataset's row count mode is updated according to the specified mode.
 *
 * @note
 *   - Changing the row count mode affects how row metadata is handled during data writing.
 *   - The @c SDDS_FIXEDROWCOUNT mode may require specifying additional parameters such as row increment.
 *
 * @warning
 *   - Attempting to change the row count mode after the layout has been written to the file or while reading from a file will result in an error.
 *   - Selecting an invalid row count mode will result in an error.
 */
int32_t SDDS_SetRowCountMode(SDDS_DATASET *SDDS_dataset, uint32_t mode) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetRowCountMode"))
    return 0;
  if (SDDS_dataset->layout.layout_written) {
    SDDS_SetError("Can't change row count mode after writing the layout, or for a file you are reading.");
    return 0;
  }
  if (mode & SDDS_VARIABLEROWCOUNT) {
    SDDS_dataset->layout.data_mode.fixed_row_count = 0;
    SDDS_dataset->layout.data_mode.no_row_counts = 0;
  } else if (mode & SDDS_FIXEDROWCOUNT) {
    SDDS_dataset->layout.data_mode.fixed_row_count = 1;
    SDDS_dataset->layout.data_mode.fixed_row_increment = 500;
    SDDS_dataset->layout.data_mode.no_row_counts = 0;
    SDDS_dataset->layout.data_mode.fsync_data = 0;
  } else if (mode & SDDS_NOROWCOUNT) {
    SDDS_dataset->layout.data_mode.fixed_row_count = 0;
    SDDS_dataset->layout.data_mode.no_row_counts = 1;
  } else {
    SDDS_SetError("Invalid row count mode (SDDS_SetRowCountMode).");
    return 0;
  }
  if (!SDDS_SaveLayout(SDDS_dataset))
    return 0;
  return 1;
}

/**
 * @brief Disables file synchronization for the SDDS dataset.
 *
 * This function disables the file synchronization feature for the specified SDDS dataset. File synchronization
 * ensures that all buffered data is immediately written to disk, enhancing data integrity. By disabling
 * this feature, the dataset will no longer perform synchronous writes, which can improve performance
 * but may risk data loss in the event of a system failure.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 *
 * @post
 *   - File synchronization is disabled, meaning that @c SDDS_SyncDataSet will not call @c fsync.
 *
 * @note
 *   - Disabling file synchronization can lead to improved performance, especially when writing large datasets.
 *   - It is recommended to use this function only when performance is a higher priority than immediate data integrity.
 *
 * @warning
 *   - Without file synchronization, there is a risk of data loss if the system crashes before buffered data is written to disk.
 *   - Ensure that data integrity is managed through other means if synchronization is disabled.
 */
void SDDS_DisableFSync(SDDS_DATASET *SDDS_dataset) {
  SDDS_dataset->layout.data_mode.fsync_data = 0;
}

/**
 * @brief Enables file synchronization for the SDDS dataset.
 *
 * This function enables the file synchronization feature for the specified SDDS dataset. File synchronization
 * ensures that all buffered data is immediately written to disk, enhancing data integrity. Enabling this
 * feature can be crucial for applications where data consistency and reliability are paramount.
 *
 * @param[in,out] SDDS_dataset Pointer to the SDDS_DATASET structure representing the dataset.
 *
 * @post
 *   - File synchronization is enabled, meaning that @c SDDS_SyncDataSet will call @c fsync to flush buffers to disk.
 *
 * @note
 *   - Enabling file synchronization may impact performance due to the increased number of disk write operations.
 *   - It is recommended to enable synchronization when data integrity is critical, such as in transactional systems.
 *
 * @warning
 *   - Frequent synchronization can lead to reduced performance, especially when writing large amounts of data.
 *   - Balance the need for data integrity with performance requirements based on the specific use case.
 */
void SDDS_EnableFSync(SDDS_DATASET *SDDS_dataset) {
  SDDS_dataset->layout.data_mode.fsync_data = 1;
}

/**
 * @brief Synchronizes the SDDS dataset's file to disk.
 *
 * Performs a file synchronization operation on the specified SDDS dataset to ensure that
 * all buffered data is flushed to the storage medium. This is crucial for maintaining
 * data integrity, especially in scenarios where unexpected shutdowns or crashes may occur.
 * 
 * ## Platform-Specific Behavior
 * - **vxWorks, Windows (_WIN32), macOS (__APPLE__)**:
 *   - The function assumes that synchronization is always successful and returns `1`.
 * - **Other Platforms**:
 *   - Attempts to flush the dataset's file buffer to disk using the `fsync` system call.
 *   - Returns `1` if `fsync` succeeds, indicating successful synchronization.
 *   - Returns `0` if `fsync` fails or if the dataset/file pointer is invalid.
 *
 * @param[in] SDDS_dataset Pointer to the `SDDS_DATASET` structure representing the dataset
 *                          to be synchronized.
 * @return int32_t
 *         - `1` on successful synchronization.
 *         - `0` on failure.
 */
int32_t SDDS_DoFSync(SDDS_DATASET *SDDS_dataset) {
#if defined(vxWorks) || defined(_WIN32) || defined(__APPLE__)
  return 1;
#else
  if (SDDS_dataset && SDDS_dataset->layout.fp)
    return fsync(fileno(SDDS_dataset->layout.fp)) == 0;
  return 0;
#endif
}
