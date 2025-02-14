/**
 * @file SDDS_input.c
 * @brief This file contains the functions related to reading SDDS files.
 *
 * The SDDS_input.c file provides functions for reading data from SDDS files.
 * It includes functions for opening and closing SDDS files, reading headers,
 * and reading data tables.
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

#include "mdb.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDS_internal.h"
#include "namelist.h"
#include "scan.h"

#if defined(_WIN32)
#  include <fcntl.h>
#  include <io.h>
#  if !defined(_MINGW)
#    define pclose(x) _pclose(x)
#  endif
#  if defined(__BORLANDC__)
#    define _setmode(handle, amode) setmode(handle, amode)
#  endif
#endif

#define DEBUG 0

/**
 * Initializes a SDDS_DATASET structure for use in reading data from a SDDS file. This involves opening the file and reading the SDDS header.
 *
 * @param SDDS_dataset Address of the SDDS_DATASET structure for the data set.
 * @param filename A NULL-terminated character string giving the name of the file to set up for input.
 *
 * @return 1 on success. On failure, returns 0 and records an error message.
 *
 */
int32_t SDDS_InitializeInput(SDDS_DATASET *SDDS_dataset, char *filename) {
  /*  char *ptr, *datafile, *headerfile; */
  char s[SDDS_MAXLINE];
#if defined(zLib)
  char *extension;
#endif
  if (sizeof(gzFile) != sizeof(void *)) {
    SDDS_SetError("gzFile is not the same size as void *, possible corruption of the SDDS_LAYOUT structure");
    return (0);
  }
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_InitializeInput"))
    return (0);

  if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET))) {
    sprintf(s, "Unable to initialize input for file %s--can't zero SDDS_DATASET structure (SDDS_InitializeInput)", filename);
    SDDS_SetError(s);
    return (0);
  }
  SDDS_dataset->layout.gzipFile = SDDS_dataset->layout.lzmaFile = SDDS_dataset->layout.disconnected = SDDS_dataset->layout.popenUsed = 0;
  SDDS_dataset->layout.depth = SDDS_dataset->layout.data_command_seen = SDDS_dataset->layout.commentFlags = SDDS_dataset->deferSavingLayout = 0;
  SDDS_dataset->layout.data_mode.column_memory_mode = DEFAULT_COLUMN_MEMORY_MODE;
  if (!filename)
    SDDS_dataset->layout.filename = NULL;
  else if (!SDDS_CopyString(&SDDS_dataset->layout.filename, filename)) {
    sprintf(s, "Memory allocation failure initializing file \"%s\" (SDDS_InitializeInput)", filename);
    SDDS_SetError(s);
    return (0);
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
#if defined(zLib)
    if (!(extension = strrchr(filename, '.')) || strcmp(extension, ".gz") != 0) {
#endif
      if ((extension = strrchr(filename, '.')) && ((strcmp(extension, ".lzma") == 0) || (strcmp(extension, ".xz") == 0))) {
        SDDS_dataset->layout.lzmaFile = 1;
        if (!(SDDS_dataset->layout.lzmafp = UnpackLZMAOpen(filename))) {
          sprintf(s, "Unable to open file \"%s\" for reading (SDDS_InitializeInput)", filename);
          SDDS_SetError(s);
          return (0);
        }
        SDDS_dataset->layout.fp = SDDS_dataset->layout.lzmafp->fp;
      } else {
        if (!(SDDS_dataset->layout.fp = UnpackFopen(filename, UNPACK_REQUIRE_SDDS | UNPACK_USE_PIPE, &SDDS_dataset->layout.popenUsed, NULL))) {
          sprintf(s, "Unable to open file \"%s\" for reading (SDDS_InitializeInput)", filename);
          SDDS_SetError(s);
          return (0);
        }
      }
#if defined(zLib)
    } else {
      SDDS_dataset->layout.gzipFile = 1;
      if (!(SDDS_dataset->layout.gzfp = gzopen(filename, "rb"))) {
        sprintf(s, "Unable to open file \"%s\" for reading (SDDS_InitializeInput)", filename);
        SDDS_SetError(s);
        return (0);
      }
    }
#endif
  }
  SDDS_dataset->page_number = SDDS_dataset->page_started = 0;
  SDDS_dataset->file_had_data = 0;
  SDDS_DeferSavingLayout(SDDS_dataset, 1);
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!SDDS_GZipReadLayout(SDDS_dataset, SDDS_dataset->layout.gzfp))
      return (0);
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!SDDS_LZMAReadLayout(SDDS_dataset, SDDS_dataset->layout.lzmafp))
        return (0);
    } else {
      if (!SDDS_ReadLayout(SDDS_dataset, SDDS_dataset->layout.fp))
        return (0);
    }
#if defined(zLib)
  }
#endif
  SDDS_dataset->layout.layout_written = 0;
  SDDS_DeferSavingLayout(SDDS_dataset, 0);
  if (!SDDS_SaveLayout(SDDS_dataset))
    return 0;
  if (SDDS_dataset->layout.n_columns &&
      ((!(SDDS_dataset->column_flag = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_dataset->layout.n_columns)) ||
        !(SDDS_dataset->column_order = (int32_t *)SDDS_Malloc(sizeof(int32_t) * SDDS_dataset->layout.n_columns))) ||
       (!SDDS_SetMemory(SDDS_dataset->column_flag, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)1, (int32_t)0) ||
        !SDDS_SetMemory(SDDS_dataset->column_order, SDDS_dataset->layout.n_columns, SDDS_LONG, (int32_t)0, (int32_t)1)))) {
    SDDS_SetError("Unable to initialize input--memory allocation failure (SDDS_InitializeInput)");
    return (0);
  }
  SDDS_dataset->mode = SDDS_READMODE; /*reading */
  SDDS_dataset->pagecount_offset = NULL;
  if (!SDDS_dataset->layout.gzipFile && !SDDS_dataset->layout.lzmaFile && !SDDS_dataset->layout.popenUsed && SDDS_dataset->layout.filename) {
    /* Data is not:
         1. from a gzip file
         2. from a file that is being internally decompressed by a command executed with popen()
         3. from a pipe set up externally (e.g., -pipe=in on commandline)
      */
    SDDS_dataset->pages_read = 0;
    SDDS_dataset->pagecount_offset = malloc(sizeof(*SDDS_dataset->pagecount_offset));
    SDDS_dataset->pagecount_offset[0] = ftell(SDDS_dataset->layout.fp);
    fseek(SDDS_dataset->layout.fp, 0, 2); /*point to the end of the file */
    SDDS_dataset->endOfFile_offset = ftell(SDDS_dataset->layout.fp);
    fseek(SDDS_dataset->layout.fp, SDDS_dataset->pagecount_offset[0], 0);
    /*point to the beginning of the first page */
  }
  return (1);
}

/**
 * @brief Initializes the SDDS dataset for headerless input.
 *
 * This function initializes the SDDS dataset structure for reading data from a file without a header.
 *
 * @param SDDS_dataset A pointer to the SDDS_DATASET structure to be initialized.
 * @param filename The name of the file to read data from.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_InitializeHeaderlessInput(SDDS_DATASET *SDDS_dataset, char *filename) {
  /*  char *ptr, *datafile; */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_InitializeInput"))
    return (0);
  if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET))) {
    SDDS_SetError("Unable to initialize input--can't zero SDDS_DATASET structure (SDDS_InitializeInput)");
    return (0);
  }
  SDDS_dataset->layout.gzipFile = SDDS_dataset->layout.lzmaFile = 0;
  SDDS_dataset->layout.depth = SDDS_dataset->layout.data_command_seen = SDDS_dataset->layout.commentFlags = SDDS_dataset->deferSavingLayout = 0;
  if (!(SDDS_dataset->layout.fp = fopen(filename, FOPEN_READ_MODE))) {
    SDDS_SetError("Unable to open file (SDDS_InitializeInput)");
    return (0);
  }
  if (!SDDS_CopyString(&SDDS_dataset->layout.filename, filename)) {
    SDDS_SetError("Memory allocation failure (SDDS_InitializeInput)");
    return (0);
  }
  SDDS_dataset->mode = SDDS_READMODE; /*reading */
  SDDS_dataset->page_number = SDDS_dataset->page_started = 0;
  SDDS_dataset->pages_read = 0;
  SDDS_dataset->pagecount_offset = malloc(sizeof(*SDDS_dataset->pagecount_offset));
  SDDS_dataset->pagecount_offset[0] = ftell(SDDS_dataset->layout.fp);
  fseek(SDDS_dataset->layout.fp, 0, 2); /*point to the end of the file */
  SDDS_dataset->endOfFile_offset = ftell(SDDS_dataset->layout.fp);
  fseek(SDDS_dataset->layout.fp, SDDS_dataset->pagecount_offset[0], 0);
  /*point to the beginning of the first page */
  return (1);
}

/**
 * @brief Checks if a position in a string is within a quoted section.
 *
 * Determines whether the specified position within a string falls inside a quoted section
 * delimited by the given quotation mark.
 *
 * @param string The string to examine.
 * @param position The position within the string to check.
 * @param quotation_mark The character used as the quotation mark.
 * @return Returns 1 if the position is within a quoted section, 0 otherwise.
 */
int32_t SDDS_IsQuoted(char *string, char *position, char quotation_mark) {
  int32_t in_quoted_section;
  char *string0;

  if (*position == quotation_mark)
    return (1);

  in_quoted_section = 0;
  string0 = string;
  while (*string) {
    if (*string == quotation_mark && (string == string0 || *(string - 1) != '\\'))
      in_quoted_section = !in_quoted_section;
    else if (string == position)
      return (in_quoted_section);
    string++;
  }
  return (0);
}

/**
 * @brief Reads a namelist from a file into a buffer.
 *
 * This function reads a namelist from the given file stream into the provided buffer,
 * handling comments and skipping them appropriately.
 *
 * @param SDDS_dataset The SDDS dataset structure.
 * @param buffer The buffer where the namelist will be stored.
 * @param buflen The length of the buffer.
 * @param fp The file stream to read from.
 * @return Returns 1 if a namelist is successfully read, 0 otherwise.
 */
int32_t SDDS_GetNamelist(SDDS_DATASET *SDDS_dataset, char *buffer, int32_t buflen, FILE *fp) {
  char *ptr, *flag, *buffer0;
  /*  char *ptr1 */
  int32_t n, i;
  /*  int32_t namelistStarted; */

  while ((flag = fgetsSkipComments(SDDS_dataset, buffer, buflen, fp, '!'))) {
    if ((ptr = strchr(buffer, '&')) && !SDDS_IsQuoted(buffer, ptr, '"'))
      break;
  }
  if (!flag)
    return 0;
  n = strlen(buffer) - 1;
  if (buffer[n] == '\n') {
    buffer[n] = ' ';
    if ((n - 1 >= 0) && (buffer[n - 1] == '\r'))
      buffer[n - 1] = ' ';
  }

  /* check for the beginning of a namelist (an unquoted &) */
  ptr = buffer;
  while (*ptr) {
    if (*ptr == '"') {
      /* skip quoted section */
      ptr++;
      while (*ptr != '"' && *ptr)
        ptr++;
      if (*ptr)
        ptr++;
      continue;
    }
    if (*ptr == '&') {
      if (strncmp(ptr, "&end", 4) == 0)
        return 0;
      break;
    }
    ptr++;
  }
  if (!*ptr)
    return 0;

  /* remove the trailing &end if there is one */
  if ((n = strlen(buffer)) >= 4) {
    ptr = buffer + n - 4;
    while (1) {
      if (*ptr == '&' && (ptr == buffer || *(ptr - 1) != '\\') && strncmp(ptr, "&end", 4) == 0 && !SDDS_IsQuoted(buffer, ptr, '"')) {
        *ptr = 0;
        return 1;
      }
      if (ptr == buffer)
        break;
      ptr--;
    }
  }

  /* read in the remainder of the namelist */
  buffer0 = buffer;
  buflen -= strlen(buffer);
  buffer += strlen(buffer);
  i = 0;
  while ((flag = fgetsSkipComments(SDDS_dataset, buffer, buflen, fp, '!'))) {
    n = strlen(buffer) - 1;
    if (buffer[n] == '\n') {
      buffer[n] = ' ';
      if ((n - 1 >= 0) && (buffer[n - 1] == '\r'))
        buffer[n - 1] = ' ';
    }
    if ((ptr = strstr(buffer, "&end")) && !SDDS_IsQuoted(buffer0, ptr, '"'))
      return 1;
    buflen -= strlen(buffer);
    buffer += strlen(buffer);
    if (buflen == 0)
      return 0;
    /* this was needed after encountering a file that had binary crap
         dumpted into the header. sddscheck will now report badHeader
         instead of getting stuck in an endless loop here. */
    i++;
    if (i > 10000) {
      return 0;
    }
  }
  return 0;
}

/**
 * @brief Reads a namelist from an LZMA-compressed file into a buffer.
 *
 * This function reads a namelist from an LZMA-compressed file stream into the provided buffer,
 * handling comments and skipping them appropriately.
 *
 * @param SDDS_dataset The SDDS dataset structure.
 * @param buffer The buffer where the namelist will be stored.
 * @param buflen The length of the buffer.
 * @param lzmafp The LZMA file stream to read from.
 * @return Returns 1 if a namelist is successfully read, 0 otherwise.
 */
int32_t SDDS_GetLZMANamelist(SDDS_DATASET *SDDS_dataset, char *buffer, int32_t buflen, struct lzmafile *lzmafp) {
  char *ptr, *flag, *buffer0;
  /*  char *ptr1 */
  int32_t n;
  /*  int32_t namelistStarted; */

  while ((flag = fgetsLZMASkipComments(SDDS_dataset, buffer, buflen, lzmafp, '!'))) {
    if ((ptr = strchr(buffer, '&')) && !SDDS_IsQuoted(buffer, ptr, '"'))
      break;
  }
  if (!flag)
    return 0;
  n = strlen(buffer) - 1;
  if (buffer[n] == '\n') {
    buffer[n] = ' ';
    if ((n - 1 >= 0) && (buffer[n - 1] == '\r'))
      buffer[n - 1] = ' ';
  }

  /* check for the beginning of a namelist (an unquoted &) */
  ptr = buffer;
  while (*ptr) {
    if (*ptr == '"') {
      /* skip quoted section */
      ptr++;
      while (*ptr != '"' && *ptr)
        ptr++;
      if (*ptr)
        ptr++;
      continue;
    }
    if (*ptr == '&') {
      if (strncmp(ptr, "&end", 4) == 0)
        return 0;
      break;
    }
    ptr++;
  }
  if (!*ptr)
    return 0;

  /* remove the trailing &end if there is one */
  if ((n = strlen(buffer)) >= 4) {
    ptr = buffer + n - 4;
    while (1) {
      if (*ptr == '&' && (ptr == buffer || *(ptr - 1) != '\\') && strncmp(ptr, "&end", 4) == 0 && !SDDS_IsQuoted(buffer, ptr, '"')) {
        *ptr = 0;
        return 1;
      }
      if (ptr == buffer)
        break;
      ptr--;
    }
  }

  /* read in the remainder of the namelist */
  buffer0 = buffer;
  buflen -= strlen(buffer);
  buffer += strlen(buffer);
  while ((flag = fgetsLZMASkipComments(SDDS_dataset, buffer, buflen, lzmafp, '!'))) {
    n = strlen(buffer) - 1;
    if (buffer[n] == '\n') {
      buffer[n] = ' ';
      if ((n - 1 >= 0) && (buffer[n - 1] == '\r'))
        buffer[n - 1] = ' ';
    }
    if ((ptr = strstr(buffer, "&end")) && !SDDS_IsQuoted(buffer0, ptr, '"'))
      return 1;
    buflen -= strlen(buffer);
    buffer += strlen(buffer);
    if (buflen == 0)
      return 0;
  }
  return 0;
}

#if defined(zLib)
/**
 * @brief Reads a namelist from a GZip-compressed file into a buffer.
 *
 * This function reads a namelist from a GZip-compressed file stream into the provided buffer,
 * handling comments and skipping them appropriately.
 *
 * @param SDDS_dataset The SDDS dataset structure.
 * @param buffer The buffer where the namelist will be stored.
 * @param buflen The length of the buffer.
 * @param gzfp The GZip file stream to read from.
 * @return Returns 1 if a namelist is successfully read, 0 otherwise.
 */
int32_t SDDS_GetGZipNamelist(SDDS_DATASET *SDDS_dataset, char *buffer, int32_t buflen, gzFile gzfp) {
  char *ptr, *flag, *buffer0;
  /*  char *ptr1 */
  int32_t n;
  /*  int32_t namelistStarted; */

  while ((flag = fgetsGZipSkipComments(SDDS_dataset, buffer, buflen, gzfp, '!'))) {
    if ((ptr = strchr(buffer, '&')) && !SDDS_IsQuoted(buffer, ptr, '"'))
      break;
  }
  if (!flag)
    return 0;
  n = strlen(buffer) - 1;
  if (buffer[n] == '\n') {
    buffer[n] = ' ';
    if ((n - 1 >= 0) && (buffer[n - 1] == '\r'))
      buffer[n - 1] = ' ';
  }

  /* check for the beginning of a namelist (an unquoted &) */
  ptr = buffer;
  while (*ptr) {
    if (*ptr == '"') {
      /* skip quoted section */
      ptr++;
      while (*ptr != '"' && *ptr)
        ptr++;
      if (*ptr)
        ptr++;
      continue;
    }
    if (*ptr == '&') {
      if (strncmp(ptr, "&end", 4) == 0)
        return 0;
      break;
    }
    ptr++;
  }
  if (!*ptr)
    return 0;

  /* remove the trailing &end if there is one */
  if ((n = strlen(buffer)) >= 4) {
    ptr = buffer + n - 4;
    while (1) {
      if (*ptr == '&' && (ptr == buffer || *(ptr - 1) != '\\') && strncmp(ptr, "&end", 4) == 0 && !SDDS_IsQuoted(buffer, ptr, '"')) {
        *ptr = 0;
        return 1;
      }
      if (ptr == buffer)
        break;
      ptr--;
    }
  }

  /* read in the remainder of the namelist */
  buffer0 = buffer;
  buflen -= strlen(buffer);
  buffer += strlen(buffer);
  while ((flag = fgetsGZipSkipComments(SDDS_dataset, buffer, buflen, gzfp, '!'))) {
    n = strlen(buffer) - 1;
    if (buffer[n] == '\n') {
      buffer[n] = ' ';
      if ((n - 1 >= 0) && (buffer[n - 1] == '\r'))
        buffer[n - 1] = ' ';
    }
    if ((ptr = strstr(buffer, "&end")) && !SDDS_IsQuoted(buffer0, ptr, '"'))
      return 1;
    buflen -= strlen(buffer);
    buffer += strlen(buffer);
    if (buflen == 0)
      return 0;
  }
  return 0;
}
#endif

/**
 * Reads the header layout of an SDDS dataset from a file.
 *
 * @param SDDS_dataset The SDDS dataset structure to store the layout information.
 * @param fp The file pointer to the SDDS file.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_ReadLayout(SDDS_DATASET *SDDS_dataset, FILE *fp) {
  char buffer[SDDS_MAXLINE];
  char *groupName, *ptr;
  FILE *fp1;
  int32_t retval, bigEndianMachine;
  uint32_t commentFlags;

  if (!fp) {
    SDDS_SetError("Unable to read layout--NULL file pointer (SDDS_ReadLayout)");
    return (0);
  }
  if (SDDS_dataset->layout.depth == 0) {
    if (SDDS_dataset->layout.disconnected) {
      SDDS_SetError("Can't read layout--file is disconnected (SDDS_ReadLayout)");
      return 0;
    }
    if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadLayout")) {
      fclose(fp);
      return (0);
    }
    SDDS_dataset->layout.layout_written = 1; /* it is already in the file */
    if (!fgets(SDDS_dataset->layout.s, SDDS_MAXLINE, fp)) {
      fclose(fp);
      SDDS_SetError("Unable to read layout--no header lines found (SDDS_ReadLayout)");
      return (0);
    }
    if (strncmp(SDDS_dataset->layout.s, "SDDS", 4) != 0) {
      fclose(fp);
      SDDS_SetError("Unable to read layout--no header lines found (SDDS_ReadLayout)");
      return (0);
    }
    if (sscanf(SDDS_dataset->layout.s + 4, "%" SCNd32, &SDDS_dataset->layout.version) != 1) {
      fclose(fp);
      SDDS_SetError("Unable to read layout--no version number on first line (SDDS_ReadLayout)");
      return (0);
    }
    SDDS_ResetSpecialCommentsModes(SDDS_dataset);
    SDDS_dataset->layout.data_command_seen = 0;
  }
  while (SDDS_GetNamelist(SDDS_dataset, SDDS_dataset->layout.s, SDDS_MAXLINE, fp)) {
#if DEBUG
    strcpy(buffer, SDDS_dataset->layout.s);
#endif
    groupName = SDDS_dataset->layout.s + 1;
    if (!(ptr = strpbrk(SDDS_dataset->layout.s, " \t"))) {
      SDDS_SetError("Unable to read layout---no groupname in namelist (SDDS_ReadLayout)");
      return 0;
    }
    *ptr = 0;
    switch (match_string(groupName, SDDS_command, SDDS_NUM_COMMANDS, EXACT_MATCH)) {
    case SDDS_DESCRIPTION_COMMAND:
      if (!SDDS_ProcessDescription(SDDS_dataset, ptr + 1)) {
        fclose(fp);
        SDDS_SetError("Unable to process description (SDDS_ReadLayout)");
        return (0);
      }
      break;
    case SDDS_COLUMN_COMMAND:
      if (!SDDS_ProcessColumnDefinition(SDDS_dataset, ptr + 1)) {
        fclose(fp);
        SDDS_SetError("Unable to process column definition (SDDS_ReadLayout)");
        return (0);
      }
      break;
    case SDDS_PARAMETER_COMMAND:
      if (!SDDS_ProcessParameterDefinition(SDDS_dataset, ptr + 1)) {
        fclose(fp);
        SDDS_SetError("Unable to process parameter definition (SDDS_ReadLayout)");
        return (0);
      }
      break;
    case SDDS_ASSOCIATE_COMMAND:
#if RW_ASSOCIATES != 0
      if (!SDDS_ProcessAssociateDefinition(SDDS_dataset, ptr + 1)) {
        fclose(fp);
        SDDS_SetError("Unable to process associate definition (SDDS_ReadLayout)");
        return (0);
      }
#endif
      break;
    case SDDS_DATA_COMMAND:
      if (!SDDS_ProcessDataMode(SDDS_dataset, ptr + 1)) {
        fclose(fp);
        SDDS_SetError("Unable to process data mode (SDDS_ReadLayout)");
        return (0);
      }
      if (SDDS_dataset->layout.data_command_seen) {
        /* should never happen */
        fclose(fp);
        SDDS_SetError("Unable to read layout--multiple data commands (SDDS_ReadLayout)");
        return (0);
      }
      if (!SDDS_SaveLayout(SDDS_dataset)) {
        SDDS_SetError("Unable to read layout--couldn't save layout (SDDS_ReadLayout)");
        return (0);
      }
      SDDS_dataset->layout.data_command_seen = 1;
      commentFlags = SDDS_GetSpecialCommentsModes(SDDS_dataset);
      if ((commentFlags & SDDS_BIGENDIAN_SEEN) && (commentFlags & SDDS_LITTLEENDIAN_SEEN)) {
        SDDS_SetError("Unable to read data as it says it is both big and little endian (SDDS_ReadLayout)");
        return (0);
      }
      bigEndianMachine = SDDS_IsBigEndianMachine();
      SDDS_dataset->swapByteOrder = SDDS_dataset->layout.byteOrderDeclared = 0;
      SDDS_dataset->autoRecover = 0;
      if ((commentFlags & SDDS_BIGENDIAN_SEEN) || (SDDS_dataset->layout.data_mode.endian == SDDS_BIGENDIAN)) {
        SDDS_dataset->layout.byteOrderDeclared = SDDS_BIGENDIAN_SEEN;
        if (!bigEndianMachine)
          SDDS_dataset->swapByteOrder = 1;
      }
      if ((commentFlags & SDDS_LITTLEENDIAN_SEEN) || (SDDS_dataset->layout.data_mode.endian == SDDS_LITTLEENDIAN)) {
        SDDS_dataset->layout.byteOrderDeclared = SDDS_LITTLEENDIAN_SEEN;
        if (bigEndianMachine)
          SDDS_dataset->swapByteOrder = 1;
      }
      if ((commentFlags & SDDS_FIXED_ROWCOUNT_SEEN) || (SDDS_dataset->layout.data_mode.fixed_row_count))
        if (!SDDS_SetAutoReadRecovery(SDDS_dataset, SDDS_AUTOREADRECOVER))
          return (0);
      return (1);
    case SDDS_INCLUDE_COMMAND:
      if (!(fp1 = SDDS_ProcessIncludeCommand(SDDS_dataset, ptr + 1))) {
        fclose(fp);
        SDDS_SetError("Unable to process include command (SDDS_ReadLayout)");
        return (0);
      }
      SDDS_dataset->layout.depth += 1;
      retval = SDDS_ReadLayout(SDDS_dataset, fp1);
      SDDS_dataset->layout.depth -= 1;
      fclose(fp1);
      if (retval == 0) {
        return (0);
      }
      if (SDDS_dataset->layout.data_command_seen) {
        return (1);
      }
      break;
    case SDDS_ARRAY_COMMAND:
      if (!SDDS_ProcessArrayDefinition(SDDS_dataset, ptr + 1)) {
        fclose(fp);
        SDDS_SetError("Unable to process array definition (SDDS_ReadLayout)");
        return (0);
      }
      break;
    default:
      fclose(fp);
      sprintf(buffer, "Unknown layout entry %s given (SDDS_ReadLayout)", groupName);
      SDDS_SetError(buffer);
      return (0);
    }
  }
  /* on recursive calls, it's okay to hit EOF */
  if ((feof(fp) && SDDS_dataset->layout.depth != 0) || SDDS_dataset->layout.data_command_seen)
    return (1);
  return (0);
}

/**
 * Reads the header layout of an SDDS dataset from a file with LZMA compression.
 *
 * @param SDDS_dataset The SDDS dataset structure to store the layout information.
 * @param lzmafp The LZMA file pointer to the SDDS file.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_LZMAReadLayout(SDDS_DATASET *SDDS_dataset, struct lzmafile *lzmafp) {
  char buffer[SDDS_MAXLINE];
  char *groupName, *ptr;
  FILE *fp1;
  int32_t retval, bigEndianMachine;
  uint32_t commentFlags;

  if (!lzmafp) {
    SDDS_SetError("Unable to read layout--NULL file pointer (SDDS_LZMAReadLayout)");
    return (0);
  }
  if (SDDS_dataset->layout.depth == 0) {
    if (SDDS_dataset->layout.disconnected) {
      SDDS_SetError("Can't read layout--file is disconnected (SDDS_LZMAReadLayout)");
      return 0;
    }
    if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_LZMAReadLayout")) {
      lzma_close(lzmafp);
      return (0);
    }
    SDDS_dataset->layout.layout_written = 1; /* it is already in the file */
    if (!lzma_gets(SDDS_dataset->layout.s, SDDS_MAXLINE, lzmafp)) {
      lzma_close(lzmafp);
      SDDS_SetError("Unable to read layout--no header lines found (SDDS_LZMAReadLayout)");
      return (0);
    }
    if (strncmp(SDDS_dataset->layout.s, "SDDS", 4) != 0) {
      lzma_close(lzmafp);
      SDDS_SetError("Unable to read layout--no header lines found (SDDS_LZMAReadLayout)");
      return (0);
    }
    if (sscanf(SDDS_dataset->layout.s + 4, "%" SCNd32, &SDDS_dataset->layout.version) != 1) {
      lzma_close(lzmafp);
      SDDS_SetError("Unable to read layout--no version number on first line (SDDS_LZMAReadLayout)");
      return (0);
    }
    SDDS_ResetSpecialCommentsModes(SDDS_dataset);
    SDDS_dataset->layout.data_command_seen = 0;
  }
  while (SDDS_GetLZMANamelist(SDDS_dataset, SDDS_dataset->layout.s, SDDS_MAXLINE, lzmafp)) {
#if DEBUG
    strcpy(buffer, SDDS_dataset->layout.s);
#endif
    groupName = SDDS_dataset->layout.s + 1;
    if (!(ptr = strpbrk(SDDS_dataset->layout.s, " \t"))) {
      SDDS_SetError("Unable to read layout---no groupname in namelist (SDDS_LZMAReadLayout)");
      return 0;
    }
    *ptr = 0;
    switch (match_string(groupName, SDDS_command, SDDS_NUM_COMMANDS, EXACT_MATCH)) {
    case SDDS_DESCRIPTION_COMMAND:
      if (!SDDS_ProcessDescription(SDDS_dataset, ptr + 1)) {
        lzma_close(lzmafp);
        SDDS_SetError("Unable to process description (SDDS_LZMAReadLayout)");
        return (0);
      }
      break;
    case SDDS_COLUMN_COMMAND:
      if (!SDDS_ProcessColumnDefinition(SDDS_dataset, ptr + 1)) {
        lzma_close(lzmafp);
        SDDS_SetError("Unable to process column definition (SDDS_LZMAReadLayout)");
        return (0);
      }
      break;
    case SDDS_PARAMETER_COMMAND:
      if (!SDDS_ProcessParameterDefinition(SDDS_dataset, ptr + 1)) {
        lzma_close(lzmafp);
        SDDS_SetError("Unable to process parameter definition (SDDS_LZMAReadLayout)");
        return (0);
      }
      break;
    case SDDS_ASSOCIATE_COMMAND:
#if RW_ASSOCIATES != 0
      if (!SDDS_ProcessAssociateDefinition(SDDS_dataset, ptr + 1)) {
        lzma_close(lzmafp);
        SDDS_SetError("Unable to process associate definition (SDDS_LZMAReadLayout)");
        return (0);
      }
#endif
      break;
    case SDDS_DATA_COMMAND:
      if (!SDDS_ProcessDataMode(SDDS_dataset, ptr + 1)) {
        lzma_close(lzmafp);
        SDDS_SetError("Unable to process data mode (SDDS_LZMAReadLayout)");
        return (0);
      }
      if (SDDS_dataset->layout.data_command_seen) {
        /* should never happen */
        lzma_close(lzmafp);
        SDDS_SetError("Unable to read layout--multiple data commands (SDDS_LZMAReadLayout)");
        return (0);
      }
      if (!SDDS_SaveLayout(SDDS_dataset)) {
        SDDS_SetError("Unable to read layout--couldn't save layout (SDDS_LZMAReadLayout)");
        return (0);
      }
      SDDS_dataset->layout.data_command_seen = 1;
      commentFlags = SDDS_GetSpecialCommentsModes(SDDS_dataset);
      if ((commentFlags & SDDS_BIGENDIAN_SEEN) && (commentFlags & SDDS_LITTLEENDIAN_SEEN)) {
        SDDS_SetError("Unable to read data as it says it is both big and little endian (SDDS_LZMAReadLayout)");
        return (0);
      }
      bigEndianMachine = SDDS_IsBigEndianMachine();
      SDDS_dataset->swapByteOrder = SDDS_dataset->layout.byteOrderDeclared = 0;
      SDDS_dataset->autoRecover = 0;
      if ((commentFlags & SDDS_BIGENDIAN_SEEN) || (SDDS_dataset->layout.data_mode.endian == SDDS_BIGENDIAN)) {
        SDDS_dataset->layout.byteOrderDeclared = SDDS_BIGENDIAN_SEEN;
        if (!bigEndianMachine)
          SDDS_dataset->swapByteOrder = 1;
      }
      if ((commentFlags & SDDS_LITTLEENDIAN_SEEN) || (SDDS_dataset->layout.data_mode.endian == SDDS_LITTLEENDIAN)) {
        SDDS_dataset->layout.byteOrderDeclared = SDDS_LITTLEENDIAN_SEEN;
        if (bigEndianMachine)
          SDDS_dataset->swapByteOrder = 1;
      }
      if ((commentFlags & SDDS_FIXED_ROWCOUNT_SEEN) || (SDDS_dataset->layout.data_mode.fixed_row_count))
        if (!SDDS_SetAutoReadRecovery(SDDS_dataset, SDDS_AUTOREADRECOVER))
          return (0);
      return (1);
    case SDDS_INCLUDE_COMMAND:
      if (!(fp1 = SDDS_ProcessIncludeCommand(SDDS_dataset, ptr + 1))) {
        lzma_close(lzmafp);
        SDDS_SetError("Unable to process include command (SDDS_LZMAReadLayout)");
        return (0);
      }
      SDDS_dataset->layout.depth += 1;
      retval = SDDS_ReadLayout(SDDS_dataset, fp1);
      SDDS_dataset->layout.depth -= 1;
      fclose(fp1);
      if (retval == 0) {
        return (0);
      }
      if (SDDS_dataset->layout.data_command_seen) {
        return (1);
      }
      break;
    case SDDS_ARRAY_COMMAND:
      if (!SDDS_ProcessArrayDefinition(SDDS_dataset, ptr + 1)) {
        lzma_close(lzmafp);
        SDDS_SetError("Unable to process array definition (SDDS_LZMAReadLayout)");
        return (0);
      }
      break;
    default:
      lzma_close(lzmafp);
      sprintf(buffer, "Unknown layout entry %s given (SDDS_LZMAReadLayout)", groupName);
      SDDS_SetError(buffer);
      return (0);
    }
  }
  /* on recursive calls, it's okay to hit EOF */
  if ((lzma_eof(lzmafp) && SDDS_dataset->layout.depth != 0) || SDDS_dataset->layout.data_command_seen)
    return (1);
  return (0);
}

#if defined(zLib)
/**
 * Reads the header layout of an SDDS dataset from a file with GZIP compression.
 *
 * @param SDDS_dataset The SDDS dataset structure to store the layout information.
 * @param gzfp The GZIP file pointer to the SDDS file.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_GZipReadLayout(SDDS_DATASET *SDDS_dataset, gzFile gzfp) {
  char buffer[SDDS_MAXLINE];
  char *groupName, *ptr;
  FILE *fp1;
  int32_t retval, bigEndianMachine;
  uint32_t commentFlags;

  if (!gzfp) {
    SDDS_SetError("Unable to read layout--NULL file pointer (SDDS_GZipReadLayout)");
    return (0);
  }
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't read layout--file is disconnected (SDDS_GZipReadLayout)");
    return 0;
  }
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GZipReadLayout")) {
    gzclose(gzfp);
    return (0);
  }
  SDDS_dataset->layout.layout_written = 1; /* it is already in the file */
  if (!gzgets(gzfp, SDDS_dataset->layout.s, SDDS_MAXLINE)) {
    gzclose(gzfp);
    SDDS_SetError("Unable to read layout--no header lines found (SDDS_GZipReadLayout)");
    return (0);
  }
  if (strncmp(SDDS_dataset->layout.s, "SDDS", 4) != 0) {
    gzclose(gzfp);
    SDDS_SetError("Unable to read layout--no header lines found (SDDS_GZipReadLayout)");
    return (0);
  }
  if (sscanf(SDDS_dataset->layout.s + 4, "%" SCNd32, &SDDS_dataset->layout.version) != 1) {
    gzclose(gzfp);
    SDDS_SetError("Unable to read layout--no version number on first line (SDDS_GZipReadLayout)");
    return (0);
  }
  SDDS_ResetSpecialCommentsModes(SDDS_dataset);
  if (SDDS_dataset->layout.depth == 0)
    SDDS_dataset->layout.data_command_seen = 0;
  while (SDDS_GetGZipNamelist(SDDS_dataset, SDDS_dataset->layout.s, SDDS_MAXLINE, gzfp)) {
#  if DEBUG
    strcpy(buffer, SDDS_dataset->layout.s);
#  endif
    groupName = SDDS_dataset->layout.s + 1;
    if (!(ptr = strpbrk(SDDS_dataset->layout.s, " \t"))) {
      SDDS_SetError("Unable to read layout---no groupname in namelist (SDDS_GZipReadLayout)");
      return 0;
    }
    *ptr = 0;
    switch (match_string(groupName, SDDS_command, SDDS_NUM_COMMANDS, EXACT_MATCH)) {
    case SDDS_DESCRIPTION_COMMAND:
      if (!SDDS_ProcessDescription(SDDS_dataset, ptr + 1)) {
        gzclose(gzfp);
        SDDS_SetError("Unable to process description (SDDS_GZipReadLayout)");
        return (0);
      }
      break;
    case SDDS_COLUMN_COMMAND:
      if (!SDDS_ProcessColumnDefinition(SDDS_dataset, ptr + 1)) {
        gzclose(gzfp);
        SDDS_SetError("Unable to process column definition (SDDS_GZipReadLayout)");
        return (0);
      }
      break;
    case SDDS_PARAMETER_COMMAND:
      if (!SDDS_ProcessParameterDefinition(SDDS_dataset, ptr + 1)) {
        gzclose(gzfp);
        SDDS_SetError("Unable to process parameter definition (SDDS_GZipReadLayout)");
        return (0);
      }
      break;
    case SDDS_ASSOCIATE_COMMAND:
#  if RW_ASSOCIATES != 0
      if (!SDDS_ProcessAssociateDefinition(SDDS_dataset, ptr + 1)) {
        gzclose(gzfp);
        SDDS_SetError("Unable to process associate definition (SDDS_GZipReadLayout)");
        return (0);
      }
#  endif
      break;
    case SDDS_DATA_COMMAND:
      if (!SDDS_ProcessDataMode(SDDS_dataset, ptr + 1)) {
        gzclose(gzfp);
        SDDS_SetError("Unable to process data mode (SDDS_GZipReadLayout)");
        return (0);
      }
      if (SDDS_dataset->layout.data_command_seen) {
        /* should never happen */
        gzclose(gzfp);
        SDDS_SetError("Unable to read layout--multiple data commands (SDDS_GZipReadLayout)");
        return (0);
      }
      if (!SDDS_SaveLayout(SDDS_dataset)) {
        SDDS_SetError("Unable to read layout--couldn't save layout (SDDS_GZipReadLayout)");
        return (0);
      }
      SDDS_dataset->layout.data_command_seen = 1;
      commentFlags = SDDS_GetSpecialCommentsModes(SDDS_dataset);
      if ((commentFlags & SDDS_BIGENDIAN_SEEN) && (commentFlags & SDDS_LITTLEENDIAN_SEEN)) {
        SDDS_SetError("Unable to read data as it says it is both big and little endian (SDDS_ReadLayout)");
        return (0);
      }
      bigEndianMachine = SDDS_IsBigEndianMachine();
      SDDS_dataset->swapByteOrder = SDDS_dataset->layout.byteOrderDeclared = 0;
      SDDS_dataset->autoRecover = 0;
      if ((commentFlags & SDDS_BIGENDIAN_SEEN) || (SDDS_dataset->layout.data_mode.endian == SDDS_BIGENDIAN)) {
        SDDS_dataset->layout.byteOrderDeclared = SDDS_BIGENDIAN_SEEN;
        if (!bigEndianMachine)
          SDDS_dataset->swapByteOrder = 1;
      }
      if ((commentFlags & SDDS_LITTLEENDIAN_SEEN) || (SDDS_dataset->layout.data_mode.endian == SDDS_LITTLEENDIAN)) {
        SDDS_dataset->layout.byteOrderDeclared = SDDS_LITTLEENDIAN_SEEN;
        if (bigEndianMachine)
          SDDS_dataset->swapByteOrder = 1;
      }
      if ((commentFlags & SDDS_FIXED_ROWCOUNT_SEEN) || (SDDS_dataset->layout.data_mode.fixed_row_count))
        if (!SDDS_SetAutoReadRecovery(SDDS_dataset, SDDS_AUTOREADRECOVER))
          return (0);
      return (1);
    case SDDS_INCLUDE_COMMAND:
      if (!(fp1 = SDDS_ProcessIncludeCommand(SDDS_dataset, ptr + 1))) {
        gzclose(gzfp);
        SDDS_SetError("Unable to process include command (SDDS_GZipReadLayout)");
        return (0);
      }
      SDDS_dataset->layout.depth += 1;
      retval = SDDS_ReadLayout(SDDS_dataset, fp1);
      SDDS_dataset->layout.depth -= 1;
      fclose(fp1);
      if (retval == 0) {
        return (0);
      }
      if (SDDS_dataset->layout.data_command_seen) {
        return (1);
      }
      break;
    case SDDS_ARRAY_COMMAND:
      if (!SDDS_ProcessArrayDefinition(SDDS_dataset, ptr + 1)) {
        gzclose(gzfp);
        SDDS_SetError("Unable to process array definition (SDDS_GZipReadLayout)");
        return (0);
      }
      break;
    default:
      gzclose(gzfp);
      sprintf(buffer, "Unknown layout entry %s given (SDDS_GZipReadLayout)", groupName);
      SDDS_SetError(buffer);
      return (0);
    }
  }
  /* on recursive calls, it's okay to hit EOF */
  if ((gzeof(gzfp) && SDDS_dataset->layout.depth != 0) || SDDS_dataset->layout.data_command_seen)
    return (1);
  return (0);
}
#endif

/**
 * Reads a page of an SDDS file. Usually called after SDDS_InitializeInput.
 *
 * @param SDDS_dataset Address of the SDDS_DATASET structure for the data set.
 * @return Page number on success, -1 if it is the end-of-file, 0 on error.
 */
int32_t SDDS_ReadPage(SDDS_DATASET *SDDS_dataset) {
#if SDDS_MPI_IO
  if (SDDS_dataset->parallel_io)
    return SDDS_MPI_ReadPage(SDDS_dataset);
#endif
  return SDDS_ReadPageSparse(SDDS_dataset, 0, 1, 0, 0);
}

/**
 * Checks if the end of the SDDS dataset file has been reached.
 *
 * @param SDDS_dataset The SDDS dataset structure.
 * @return Returns 1 if the end of file has been reached, 0 if not, and 2 on error.
 */
int32_t SDDS_CheckEndOfFile(SDDS_DATASET *SDDS_dataset) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_EndOfFile"))
    return (0);
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't check status--file is disconnected (SDDS_EndOfFile)");
    return 2;
  }
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!SDDS_dataset->layout.gzfp) {
      SDDS_SetError("Unable to check status--NULL file pointer (SDDS_EndOfFile)");
      return 2;
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!SDDS_dataset->layout.lzmafp) {
        SDDS_SetError("Unable to check status--NULL file pointer (SDDS_EndOfFile)");
        return 2;
      }
    } else {
      if (!SDDS_dataset->layout.fp) {
        SDDS_SetError("Unable to check status--NULL file pointer (SDDS_EndOfFile)");
        return 2;
      }
    }
#if defined(zLib)
  }
#endif
  if (SDDS_dataset->fBuffer.bufferSize && SDDS_dataset->fBuffer.bytesLeft) {
    return 0;
  }

#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (gzeof(SDDS_dataset->layout.gzfp))
      return 1;
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (lzma_eof(SDDS_dataset->layout.lzmafp))
        return 1;
    } else {
      if (feof(SDDS_dataset->layout.fp))
        return 1;
    }
#if defined(zLib)
  }
#endif
  return 0;
}

/**
 * Reads a sparsed page of an SDDS file. Usually called after SDDS_InitializeInput.
 *
 * @param SDDS_dataset       A pointer to an SDDS dataset.
 * @param mode               Not used.
 * @param sparse_interval    The column data can be sparsified over row intervals if this is greater than 1.
 * @param sparse_offset      This is used to skip the initial rows of the column data.
 * @param sparse_statistics  Not used.
 * @return Page number on success, -1 if it is the end-of-file, 0 on error.
 */
int32_t SDDS_ReadPageSparse(SDDS_DATASET *SDDS_dataset, uint32_t mode, int64_t sparse_interval, int64_t sparse_offset, int32_t sparse_statistics)
/* the mode argument is to support future expansion */
{
  int32_t retval;
  /*  SDDS_LAYOUT layout_copy; */

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadPageSparse"))
    return (0);
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't read page--file is disconnected (SDDS_ReadPageSparse)");
    return 0;
  }
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!SDDS_dataset->layout.gzfp) {
      SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadPageSparse)");
      return (0);
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!SDDS_dataset->layout.lzmafp) {
        SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadPageSparse)");
        return (0);
      }
    } else {
      if (!SDDS_dataset->layout.fp) {
        SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadPageSparse)");
        return (0);
      }
    }
#if defined(zLib)
  }
#endif
  if (SDDS_dataset->original_layout.data_mode.mode == SDDS_ASCII) {
    if ((retval = SDDS_ReadAsciiPage(SDDS_dataset, sparse_interval, sparse_offset, sparse_statistics)) < 1) {
      return (retval);
    }
  } else if (SDDS_dataset->original_layout.data_mode.mode == SDDS_BINARY) {
    if ((retval = SDDS_ReadBinaryPage(SDDS_dataset, sparse_interval, sparse_offset, sparse_statistics)) < 1) {
      return (retval);
    }
  } else {
    SDDS_SetError("Unable to read page--unrecognized data mode (SDDS_ReadPageSparse)");
    return (0);
  }
  if (!SDDS_dataset->layout.gzipFile && !SDDS_dataset->layout.lzmaFile && !SDDS_dataset->layout.popenUsed && SDDS_dataset->layout.filename && SDDS_dataset->pagecount_offset) {
    /* Data is not:
         1. from a gzip file
         2. from a file that is being internally decompressed by a command executed with popen()
         3. from a pipe set up externally (e.g., -pipe=in on commandline)
         and pagecount_offset has been allocate memory from SDDS_initializeInput()
      */
    if (SDDS_dataset->pagecount_offset[SDDS_dataset->pages_read] < SDDS_dataset->endOfFile_offset) {
      SDDS_dataset->pages_read++;
      if (!(SDDS_dataset->pagecount_offset = realloc(SDDS_dataset->pagecount_offset, sizeof(int64_t) * (SDDS_dataset->pages_read + 1)))) {
        SDDS_SetError("Unable to allocate memory for pagecount_offset (SDDS_ReadPageSparse)");
        exit(1);
      }
      SDDS_dataset->pagecount_offset[SDDS_dataset->pages_read] = ftell(SDDS_dataset->layout.fp);
    }
  } else {
    SDDS_dataset->pages_read++;
  }
  return (retval);
}

/**
 * Reads the last specified number of rows from the SDDS dataset.
 *
 * @param SDDS_dataset The pointer to the SDDS dataset structure.
 * @param last_rows The number of rows to read from the end of the dataset.
 * @return Page number on success, -1 if it is the end-of-file, 0 on error.
 */
int32_t SDDS_ReadPageLastRows(SDDS_DATASET *SDDS_dataset, int64_t last_rows) {
  int32_t retval;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ReadPageLastRows"))
    return (0);
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't read page--file is disconnected (SDDS_ReadPageLastRows)");
    return 0;
  }
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (!SDDS_dataset->layout.gzfp) {
      SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadPageLastRows)");
      return (0);
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (!SDDS_dataset->layout.lzmafp) {
        SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadPageLastRows)");
        return (0);
      }
    } else {
      if (!SDDS_dataset->layout.fp) {
        SDDS_SetError("Unable to read page--NULL file pointer (SDDS_ReadPageLastRows)");
        return (0);
      }
    }
#if defined(zLib)
  }
#endif
  if (SDDS_dataset->original_layout.data_mode.mode == SDDS_ASCII) {
    if ((retval = SDDS_ReadAsciiPageLastRows(SDDS_dataset, last_rows)) < 1) {
      return (retval);
    }
  } else if (SDDS_dataset->original_layout.data_mode.mode == SDDS_BINARY) {
    if ((retval = SDDS_ReadBinaryPageLastRows(SDDS_dataset, last_rows)) < 1) {
      return (retval);
    }
  } else {
    SDDS_SetError("Unable to read page--unrecognized data mode (SDDS_ReadPageLastRows)");
    return (0);
  }
  if (!SDDS_dataset->layout.gzipFile && !SDDS_dataset->layout.lzmaFile && !SDDS_dataset->layout.popenUsed && SDDS_dataset->layout.filename && SDDS_dataset->pagecount_offset) {
    /* Data is not:
         1. from a gzip file
         2. from a file that is being internally decompressed by a command executed with popen()
         3. from a pipe set up externally (e.g., -pipe=in on commandline)
         and pagecount_offset has been allocate memory from SDDS_initializeInput()
      */
    if (SDDS_dataset->pagecount_offset[SDDS_dataset->pages_read] < SDDS_dataset->endOfFile_offset) {
      SDDS_dataset->pages_read++;
      if (!(SDDS_dataset->pagecount_offset = realloc(SDDS_dataset->pagecount_offset, sizeof(int64_t) * (SDDS_dataset->pages_read + 1)))) {
        SDDS_SetError("Unable to allocate memory for pagecount_offset (SDDS_ReadPageLastRows)");
        exit(1);
      }
      SDDS_dataset->pagecount_offset[SDDS_dataset->pages_read] = ftell(SDDS_dataset->layout.fp);
    }
  } else {
    SDDS_dataset->pages_read++;
  }
  return (retval);
}

/**
 * @brief Global variable to set a limit on the number of rows read.
 *
 * The default value is `INT64_MAX`, indicating no limit.
 */
static int64_t SDDS_RowLimit = INT64_MAX;
/**
 * Sets the row limit for the SDDS dataset.
 *
 * @param limit The maximum number of rows to read. If `limit <= 0`, the row limit is set to `INT64_MAX`.
 * @return The previous row limit value.
 */
int64_t SDDS_SetRowLimit(int64_t limit) {
  int64_t previous;
  previous = SDDS_RowLimit;
  if (limit <= 0)
    SDDS_RowLimit = INT64_MAX;
  else
    SDDS_RowLimit = limit;
  return previous;
}

/**
 * Retrieves the current row limit for the SDDS dataset.
 *
 * @return The current row limit.
 */
int64_t SDDS_GetRowLimit() {
  return SDDS_RowLimit;
}

/**
 * @brief Sets the current page of the SDDS dataset to the specified page number.
 *
 * This function is used to navigate to a specific page of the SDDS dataset. It is only
 * supported for non-zip files and does not work for pipe input.
 *
 * @param SDDS_dataset The SDDS dataset to operate on.
 * @param page_number The page number to navigate to.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_GotoPage(SDDS_DATASET *SDDS_dataset, int32_t page_number) {
  int64_t offset;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GotoPage"))
    return (0);
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't go to page--file is disconnected (SDDS_GotoPage)");
    return 0;
  }
  if (SDDS_dataset->layout.popenUsed || !SDDS_dataset->layout.filename) {
    SDDS_SetError("Can't go to page of pipe is used (SDDS_GotoPage)");
    return 0;
  }
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    SDDS_SetError("Can not go to page of a gzip file (SDDS_GotoPage)");
    return (0);
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      SDDS_SetError("Can not go to page of an .lzma or .xz file (SDDS_GotoPage)");
      return (0);
    } else {
      if (!SDDS_dataset->layout.fp) {
        SDDS_SetError("Unable to go to page--NULL file pointer (SDDS_GotoPage)");
        return (0);
      }
    }
#if defined(zLib)
  }
#endif
  if (!SDDS_dataset->layout.filename) {
    SDDS_SetError("Can't go to page--NULL filename pointer (SDDS_GotoPage)");
    return 0;
  }
  if (SDDS_dataset->mode != SDDS_READMODE) {
    SDDS_SetError("Can't go to page--file mode has to be reading mode (SDDS_GotoPage)");
    return 0;
  }
  if (SDDS_dataset->fBuffer.bufferSize) {
    SDDS_SetError("Can't go to page--file buffering is turned on (SDDS_GotoPage)");
    return 0;
  }
  if (page_number < 1) {
    SDDS_SetError("The page_number can not be less than 1 (SDDS_GotoPage)");
    return (0);
  }
  if (page_number > SDDS_dataset->pages_read) {
    offset = SDDS_dataset->pagecount_offset[SDDS_dataset->pages_read] - ftell(SDDS_dataset->layout.fp);
    fseek(SDDS_dataset->layout.fp, offset, 1);
    SDDS_dataset->page_number = SDDS_dataset->pages_read;
    while (SDDS_dataset->pages_read < page_number) {
      if (SDDS_ReadPageSparse(SDDS_dataset, 0, 10000, 0, 0) <= 0) {
        SDDS_SetError("The page_number is greater than the total pages (SDDS_GotoPage)");
        return (0);
      }
    }
  } else {
    offset = SDDS_dataset->pagecount_offset[page_number - 1] - ftell(SDDS_dataset->layout.fp);
    fseek(SDDS_dataset->layout.fp, offset, 1); /*seek to the position from current offset */
    SDDS_dataset->page_number = page_number - 1;
  }
  return 1;
}

/**
 * @brief Global variable to set the terminate mode for the SDDS dataset.
 *
 * Default value is 0.
 */
static int32_t terminateMode = 0;

/**
 * Sets the terminate mode for the SDDS dataset.
 *
 * @param mode The terminate mode to set.
 */
void SDDS_SetTerminateMode(uint32_t mode) {
  terminateMode = mode;
}

/**
 * Sets the column memory mode for the SDDS dataset.
 *
 * @param SDDS_dataset The SDDS dataset to operate on.
 * @param mode The column memory mode to set.
 */
void SDDS_SetColumnMemoryMode(SDDS_DATASET *SDDS_dataset, uint32_t mode) {
  SDDS_dataset->layout.data_mode.column_memory_mode = mode;
}

/**
 * Retrieves the current column memory mode for the SDDS dataset.
 *
 * @param SDDS_dataset The SDDS dataset to query.
 * @return The current column memory mode.
 */
int32_t SDDS_GetColumnMemoryMode(SDDS_DATASET *SDDS_dataset) {
  return (SDDS_dataset->layout.data_mode.column_memory_mode);
}

#include <signal.h>

/**
 * Frees all allocated string data in the SDDS dataset.
 *
 * This function frees any strings allocated for parameters, arrays, and columns
 * within the SDDS dataset. It is typically called during termination to clean up
 * allocated memory.
 *
 * @param SDDS_dataset The SDDS dataset to free string data from.
 * @return Returns 1 on success, 0 on failure.
 */
int32_t SDDS_FreeStringData(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *layout;
  char **ptr;
  int64_t i, j;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_Terminate"))
    return (0);
  layout = &SDDS_dataset->original_layout;

  if (SDDS_dataset->parameter) {
    for (i = 0; i < layout->n_parameters; i++) {
      if (layout->parameter_definition[i].type == SDDS_STRING) {
        free(*(char **)(SDDS_dataset->parameter[i]));
        *(char **)(SDDS_dataset->parameter[i]) = NULL;
      }
    }
  }
  if (SDDS_dataset->array) {
    for (i = 0; i < layout->n_arrays; i++) {
      if (layout->array_definition[i].type == SDDS_STRING) {
        for (j = 0; j < SDDS_dataset->array[i].elements; j++)
          if (((char **)SDDS_dataset->array[i].data)[j]) {
            free(((char **)SDDS_dataset->array[i].data)[j]);
            ((char **)SDDS_dataset->array[i].data)[j] = NULL;
          }
      }
    }
  }
  if (SDDS_dataset->data) {
    for (i = 0; i < layout->n_columns; i++)
      if (SDDS_dataset->data[i]) {
        if (layout->column_definition[i].type == SDDS_STRING) {
          ptr = (char **)SDDS_dataset->data[i];
          for (j = 0; j < SDDS_dataset->n_rows_allocated; j++, ptr++)
            if (*ptr) {
              free(*ptr);
              *ptr = NULL;
            }
        }
      }
  }
  return (1);
}

/**
 * Frees the strings in the current table of the SDDS dataset.
 *
 * This function frees any strings stored in the data columns of the current table.
 * It does not free strings from parameters or arrays.
 *
 * @param SDDS_dataset The SDDS dataset to free table strings from.
 */
void SDDS_FreeTableStrings(SDDS_DATASET *SDDS_dataset) {
  int64_t i, j;
  char **ptr;
  /* free stored strings */
  if (!SDDS_dataset)
    return;
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++)
    if (SDDS_dataset->layout.column_definition[i].type == SDDS_STRING) {
      ptr = (char **)SDDS_dataset->data[i];
      for (j = 0; j < SDDS_dataset->n_rows; j++, ptr++)
        if (*ptr) {
          free(*ptr);
          *ptr = NULL;
        }
    }
}

/**
 * Closes an SDDS file and frees the related memory.
 *
 * @param SDDS_dataset A pointer to an SDDS dataset.
 * @return 1 on success, 0 on error.
 */
int32_t SDDS_Terminate(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *layout;
  char **ptr;
  int64_t i, j;
  FILE *fp;
  char termBuffer[16384];
#if SDDS_MPI_IO
  if (SDDS_dataset->parallel_io)
    return SDDS_MPI_Terminate(SDDS_dataset);
#endif
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_Terminate"))
    return (0);
  layout = &SDDS_dataset->original_layout;

  fp = SDDS_dataset->layout.fp;
#if defined(zLib)
  if (SDDS_dataset->layout.gzipFile) {
    if (SDDS_dataset->layout.gzfp && layout->filename) {
      if ((SDDS_dataset->writing_page) && (SDDS_dataset->layout.data_mode.fixed_row_count)) {
        if (!SDDS_UpdateRowCount(SDDS_dataset))
          return (0);
      }
      gzclose(SDDS_dataset->layout.gzfp);
    }
  } else {
#endif
    if (SDDS_dataset->layout.lzmaFile) {
      if (SDDS_dataset->layout.lzmafp && layout->filename) {
        if ((SDDS_dataset->writing_page) && (SDDS_dataset->layout.data_mode.fixed_row_count)) {
          if (!SDDS_UpdateRowCount(SDDS_dataset))
            return (0);
        }
        lzma_close(SDDS_dataset->layout.lzmafp);
      }
    } else {
      if (fp && layout->filename) {
        if ((SDDS_dataset->writing_page) && (SDDS_dataset->layout.data_mode.fixed_row_count)) {
          if (!SDDS_UpdateRowCount(SDDS_dataset))
            return (0);
        }
        if (layout->popenUsed) {
          while (fread(termBuffer, sizeof(*termBuffer), 16384, fp)) {
          }
#if defined(vxWorks)
          fprintf(stderr, "pclose is not supported in vxWorks\n");
          exit(1);
#else
        pclose(fp);
#endif
        } else {
          fclose(fp);
        }
      }
    }
#if defined(zLib)
  }
#endif

#if DEBUG
  fprintf(stderr, "Freeing data for file %s\n", SDDS_dataset->layout.filename ? SDDS_dataset->layout.filename : "NULL");
#endif

  if (SDDS_dataset->pagecount_offset)
    free(SDDS_dataset->pagecount_offset);
  if (SDDS_dataset->row_flag)
    free(SDDS_dataset->row_flag);
  if (SDDS_dataset->column_order)
    free(SDDS_dataset->column_order);
  if (SDDS_dataset->column_flag)
    free(SDDS_dataset->column_flag);
  if (SDDS_dataset->fBuffer.buffer)
    free(SDDS_dataset->fBuffer.buffer);
#if DEBUG
  fprintf(stderr, "freeing parameter data...\n");
#endif
  if (SDDS_dataset->parameter) {
    for (i = 0; i < layout->n_parameters; i++) {
      if (layout->parameter_definition[i].type == SDDS_STRING && *(char **)(SDDS_dataset->parameter[i]))
        free(*(char **)(SDDS_dataset->parameter[i]));
      if (SDDS_dataset->parameter[i])
        free(SDDS_dataset->parameter[i]);
    }
    free(SDDS_dataset->parameter);
  }
#if DEBUG
  fprintf(stderr, "freeing array data...\n");
#endif
  if (SDDS_dataset->array) {
    for (i = 0; i < layout->n_arrays; i++) {
      if (layout->array_definition[i].type == SDDS_STRING && !(terminateMode & TERMINATE_DONT_FREE_ARRAY_STRINGS)) {
        for (j = 0; j < SDDS_dataset->array[i].elements; j++)
          if (((char **)SDDS_dataset->array[i].data)[j])
            free(((char **)SDDS_dataset->array[i].data)[j]);
      }
      /*
            if (SDDS_dataset->array[i].definition->type==SDDS_STRING &&
            !(terminateMode&TERMINATE_DONT_FREE_ARRAY_STRINGS)) {
            for (j=0; j<SDDS_dataset->array[i].elements; j++)
            if (((char**)SDDS_dataset->array[i].data)[j])
            free(((char**)SDDS_dataset->array[i].data)[j]);
            }
          */
      if (SDDS_dataset->array[i].data)
        free(SDDS_dataset->array[i].data);
      /* should free the subpointers too, but it would be a lot of trouble for little benefit: */
      if (SDDS_dataset->array[i].pointer && SDDS_dataset->array[i].definition->dimensions != 1)
        free(SDDS_dataset->array[i].pointer);
      if (SDDS_dataset->array[i].dimension)
        free(SDDS_dataset->array[i].dimension);
      /* don't touch this--it's done below */
      if (SDDS_dataset->array[i].definition && SDDS_dataset->array[i].definition->name) {
        if (SDDS_dataset->array[i].definition->name != layout->array_definition[i].name)
          SDDS_FreeArrayDefinition(SDDS_dataset->array[i].definition);
      }
      SDDS_dataset->array[i].definition = NULL;
    }
    free(SDDS_dataset->array);
  }
#if DEBUG
  fprintf(stderr, "freeing tabular data...\n");
#endif
  if (SDDS_dataset->data) {
    for (i = 0; i < layout->n_columns; i++)
      if (SDDS_dataset->data[i]) {
        if ((SDDS_dataset->column_track_memory == NULL) || (SDDS_dataset->column_track_memory[i])) {
          if (layout->column_definition[i].type == SDDS_STRING && !(terminateMode & TERMINATE_DONT_FREE_TABLE_STRINGS)) {
            ptr = (char **)SDDS_dataset->data[i];
            for (j = 0; j < SDDS_dataset->n_rows_allocated; j++, ptr++)
              if (*ptr)
                free(*ptr);
          }
          free(SDDS_dataset->data[i]);
        }
      }
    free(SDDS_dataset->data);
  }
  if (SDDS_dataset->column_track_memory)
    free(SDDS_dataset->column_track_memory);
#if DEBUG
  fprintf(stderr, "freeing layout data...\n");
#endif
  if (layout->description)
    free(layout->description);
  if (layout->contents == (&SDDS_dataset->layout)->contents)
    (&SDDS_dataset->layout)->contents = NULL;
  if (layout->contents)
    free(layout->contents);
  if (layout->filename)
    free(layout->filename);
  if (layout->column_definition) {
    for (i = 0; i < layout->n_columns; i++) {
      if (layout->column_index[i])
        free(layout->column_index[i]);
      if (layout->column_definition[i].name)
        free(layout->column_definition[i].name);
      if (layout->column_definition[i].symbol)
        free(layout->column_definition[i].symbol);
      if (layout->column_definition[i].units)
        free(layout->column_definition[i].units);
      if (layout->column_definition[i].description)
        free(layout->column_definition[i].description);
      if (layout->column_definition[i].format_string)
        free(layout->column_definition[i].format_string);
    }
    free(layout->column_definition);
    free(layout->column_index);
  }
  if (layout->parameter_definition) {
    for (i = 0; i < layout->n_parameters; i++) {
      if (layout->parameter_index[i])
        free(layout->parameter_index[i]);
      if (layout->parameter_definition[i].name)
        free(layout->parameter_definition[i].name);
      if (layout->parameter_definition[i].symbol)
        free(layout->parameter_definition[i].symbol);
      if (layout->parameter_definition[i].units)
        free(layout->parameter_definition[i].units);
      if (layout->parameter_definition[i].description)
        free(layout->parameter_definition[i].description);
      if (layout->parameter_definition[i].format_string)
        free(layout->parameter_definition[i].format_string);
      if (layout->parameter_definition[i].fixed_value)
        free(layout->parameter_definition[i].fixed_value);
    }
    free(layout->parameter_definition);
    free(layout->parameter_index);
  }
  if (layout->array_definition) {
    for (i = 0; i < layout->n_arrays; i++) {
      if (layout->array_index[i])
        free(layout->array_index[i]);
      if (layout->array_definition[i].name)
        free(layout->array_definition[i].name);
      if (layout->array_definition[i].symbol)
        free(layout->array_definition[i].symbol);
      if (layout->array_definition[i].units)
        free(layout->array_definition[i].units);
      if (layout->array_definition[i].description)
        free(layout->array_definition[i].description);
      if (layout->array_definition[i].format_string)
        free(layout->array_definition[i].format_string);
      if (layout->array_definition[i].group_name)
        free(layout->array_definition[i].group_name);
    }
    free(layout->array_definition);
    free(layout->array_index);
  }
  if (layout->associate_definition) {
    for (i = 0; i < layout->n_associates; i++) {
      if (layout->associate_definition[i].name)
        free(layout->associate_definition[i].name);
      if (layout->associate_definition[i].filename)
        free(layout->associate_definition[i].filename);
      if (layout->associate_definition[i].path)
        free(layout->associate_definition[i].path);
      if (layout->associate_definition[i].description)
        free(layout->associate_definition[i].description);
      if (layout->associate_definition[i].contents)
        free(layout->associate_definition[i].contents);
    }
    free(layout->associate_definition);
  }
  SDDS_ZeroMemory(&SDDS_dataset->original_layout, sizeof(SDDS_LAYOUT));
  layout = &SDDS_dataset->layout;
  if (layout->contents)
    free(layout->contents);
  if (layout->column_definition)
    free(layout->column_definition);
  if (layout->array_definition)
    free(layout->array_definition);
  if (layout->associate_definition)
    free(layout->associate_definition);
  if (layout->parameter_definition)
    free(layout->parameter_definition);
  if (layout->column_index)
    free(layout->column_index);
  if (layout->parameter_index)
    free(layout->parameter_index);
  if (layout->array_index)
    free(layout->array_index);
  SDDS_ZeroMemory(&SDDS_dataset->layout, sizeof(SDDS_LAYOUT));
  SDDS_ZeroMemory(SDDS_dataset, sizeof(SDDS_DATASET));
#if DEBUG
  fprintf(stderr, "done\n");
#endif
  return (1);
}

/**
 * Updates the row count in the SDDS file for fixed row count mode.
 *
 * @param SDDS_dataset The SDDS dataset to update.
 * @return 1 on success, 0 on error.
 */
int32_t SDDS_UpdateRowCount(SDDS_DATASET *SDDS_dataset) {
  FILE *fp;
  SDDS_FILEBUFFER *fBuffer;
  int64_t offset, rows;
  int32_t rows32;
  char *outputEndianess = NULL;

  if ((SDDS_dataset->layout.gzipFile) || (SDDS_dataset->layout.lzmaFile))
    return (1);
  if (!(fp = SDDS_dataset->layout.fp)) {
    SDDS_SetError("Unable to update page--file pointer is NULL (SDDS_UpdateRowCount)");
    return (0);
  }
#if DEBUG
  fprintf(stderr, "Updating rowcount in file %s with pointer %p\n", SDDS_dataset->layout.filename ? SDDS_dataset->layout.filename : "NULL", fp);
#endif
  fBuffer = &SDDS_dataset->fBuffer;
  if (!SDDS_FlushBuffer(fp, fBuffer)) {
    SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_UpdateRowCount)");
    return (0);
  }
  offset = ftell(fp);
  if (SDDS_fseek(fp, SDDS_dataset->rowcount_offset, 0) == -1) {
    SDDS_SetError("Unable to update page--failure doing fseek (SDDS_UpdateRowCount)");
    return (0);
  }
  rows = SDDS_CountRowsOfInterest(SDDS_dataset) + SDDS_dataset->first_row_in_mem;
  if (SDDS_dataset->layout.data_mode.mode == SDDS_ASCII) {
    fprintf(fp, "%20" PRId64 "\n", rows);
  } else {

    if (rows > INT32_MAX) {
      // Don't go over this limit because it has a different format
      SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateRowCount)");
      return (0);
    }
    rows32 = (int32_t)rows;
    if ((outputEndianess = getenv("SDDS_OUTPUT_ENDIANESS"))) {
      if (((strncmp(outputEndianess, "big", 3) == 0) && (SDDS_IsBigEndianMachine() == 0)) || ((strncmp(outputEndianess, "little", 6) == 0) && (SDDS_IsBigEndianMachine() == 1)))
        SDDS_SwapLong(&rows32);
    }
    if (fwrite(&rows32, sizeof(rows32), 1, fp) != 1) {
      SDDS_SetError("Unable to update page--failure writing number of rows (SDDS_UpdateRowCount)");
      return (0);
    }
  }
  if (SDDS_fseek(fp, offset, 0) == -1) {
    SDDS_SetError("Unable to update page--failure doing fseek to end of page (SDDS_UpdateRowCount)");
    return (0);
  }
  return (1);
}

/**
 * Sets the auto-read recovery mode for the SDDS dataset.
 *
 * @param SDDS_dataset The SDDS dataset to modify.
 * @param mode The mode to set (SDDS_AUTOREADRECOVER or SDDS_NOAUTOREADRECOVER).
 * @return 1 on success, 0 on error.
 */
int32_t SDDS_SetAutoReadRecovery(SDDS_DATASET *SDDS_dataset, uint32_t mode) {
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_SetAutoReadRecovery"))
    return 0;
  if (mode & SDDS_AUTOREADRECOVER) {
    SDDS_dataset->autoRecover = 1;
  } else if (mode & SDDS_NOAUTOREADRECOVER) {
    SDDS_dataset->autoRecover = 0;
  } else {
    SDDS_SetError("Invalid Auto Read Recovery mode (SDDS_SetAutoReadRecovery).");
    return 0;
  }
  return 1;
}

/**
 * Initializes the SDDS_DATASET structure for input from the search path.
 *
 * The search path is defined by calling `setSearchPath`. This function attempts to find the file
 * in the search path and initializes the SDDS dataset for input.
 *
 * @param SDDSin The SDDS_DATASET structure to be initialized.
 * @param file The name of the file to be opened for input.
 * @return 1 on success, 0 on failure.
 */
int32_t SDDS_InitializeInputFromSearchPath(SDDS_DATASET *SDDSin, char *file) {
  char *filename;
  int32_t value;
  if (!(filename = findFileInSearchPath(file))) {
    char *s;
    if (!(s = SDDS_Malloc(sizeof(*s) * (strlen(file) + 100))))
      SDDS_SetError("file does not exist in search path (InitializeInputFromSearchPath)");
    else {
      sprintf(s, "file %s does not exist in search path (InitializeInputFromSearchPath)", file);
      SDDS_SetError(s);
      free(s);
    }
    return 0;
  }
  value = SDDS_InitializeInput(SDDSin, filename);
  free(filename);
  return value;
}
