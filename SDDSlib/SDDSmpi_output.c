/**
 * @file SDDSmpi_output.c
 * @brief MPI-based Parallel I/O Functions for SDDS Datasets
 *
 * This file implements a suite of functions to facilitate parallel input/output operations
 * for SDDS (Self Describing Data Set) datasets using MPI (Message Passing Interface).
 * It includes functionalities for initializing MPI output, writing dataset layouts and
 * data pages, handling errors, terminating datasets, and managing MPI file connections.
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
 *  H. Shang
 *  M. Borland
 *  R. Soliday
 */

#include "SDDS.h"
#include "stdio.h"

int32_t SDDS_mpi_error_str_len;
char SDDS_mpi_error_str[MPI_MAX_ERROR_STRING];
/*external32 data is written in "big-endian IEEE", use it as default */
char *SDDS_MPI_FILE_TYPE[2] = {"external32", "native"};

static int32_t terminateMode = 0;

#if MPI_DEBUG
/**
 * @brief Opens a debug file for the given SDDS dataset.
 *
 * This function checks if the debug file pointer is `NULL`. If it is, it constructs a filename using the dataset's filename and MPI process ID, opens the file for writing, and writes an initial log message indicating that logging has started.
 *
 * @param sdds Pointer to the `SDDS_DATASET` structure.
 */
void openDebugFile(SDDS_DATASET *sdds) {
  if (sdds->MPI_dataset->fpdeb == NULL) {
    char s[1024];
    sprintf(s, "%s.%04d", sdds->layout.filename, sdds->MPI_dataset->myid);
    sdds->MPI_dataset->fpdeb = fopen(s, "w");
    fprintf(sdds->MPI_dataset->fpdeb, "Started log for file %s, core %d\n", sdds->layout.filename, sdds->MPI_dataset->myid);
  }
}

/**
 * @brief Logs a debug message to the dataset's debug file.
 *
 * If the `MPI_dataset` exists, the function ensures that the debug file is open by calling `openDebugFile` if necessary. Then, it writes the provided string to the debug file.
 *
 * @param string The debug message to log.
 * @param sdds Pointer to the `SDDS_DATASET` structure.
 */
void logDebug(char *string, SDDS_DATASET *sdds) {
  if (sdds->MPI_dataset) {
    if (!sdds->MPI_dataset->fpdeb)
      openDebugFile(sdds);
    if (sdds->MPI_dataset->fpdeb)
      fprintf(sdds->MPI_dataset->fpdeb, "%s\n", string);
  }
}
#endif

/**
 * @brief Converts a blank string to `NULL`.
 *
 * This function checks if the input string is `NULL` or consists only of whitespace characters. If so, it returns `NULL`; otherwise, it returns the original string.
 *
 * @param string The input string to check.
 * @return `NULL` if the string is `NULL` or blank, otherwise returns the original string.
 */
char *BlankToNull(char *string) {
  if (!string || SDDS_StringIsBlank(string))
    return NULL;
  return string;
}

/**
 * @brief Handles MPI errors by printing an error message and optionally exiting.
 *
 * Retrieves the MPI error string corresponding to the given error code. If a string is provided, it prints the string followed by the MPI error message to the specified file pointer. If `exit_code` is non-zero, the program exits with a status of 1.
 *
 * @param fp File pointer to write the error message to.
 * @param str Custom error message to prefix to the MPI error string. Can be `NULL`.
 * @param mpierr The MPI error code.
 * @param exit_code If non-zero, the function will terminate the program.
 */
void SDDS_MPI_GOTO_ERROR(FILE *fp, char *str, int32_t mpierr, int32_t exit_code) {
  MPI_Error_string(mpierr, SDDS_mpi_error_str, &SDDS_mpi_error_str_len);
  if (str)
    fprintf(fp, "%s: ", str);
  if (SDDS_mpi_error_str_len > 0)
    fprintf(fp, "%s\n", SDDS_mpi_error_str);
  if (exit_code)
    exit(1);
}

/**
 * @brief Opens an MPI file with the specified flags.
 *
 * This function opens an MPI file using the given filename and flags, setting the appropriate MPI file mode based on the flags. It also initializes the MPI dataset's debug file pointer and other related fields. If the file is successfully opened, it returns 1; otherwise, it handles the error and may exit the program.
 *
 * @param MPI_dataset Pointer to the `MPI_DATASET` structure to initialize.
 * @param filename The name of the file to open.
 * @param flags Flags indicating the mode in which to open the file (e.g., read-only, write-only, read-write).
 * @return `1` if the file was successfully opened, `0` otherwise.
 */
int32_t SDDS_MPI_File_Open(MPI_DATASET *MPI_dataset, char *filename, unsigned long flags) {
  unsigned long file_opened = 0; /* Flag to indicate that the file was successfully opened */
  int mpi_amode = MPI_MODE_RDWR;
  int mpi_code; /* mpi return code */

  /* Get the MPI rank of this process and the total number of processes */
  /*if (MPI_SUCCESS != (mpi_code=MPI_Comm_rank(MPI_dataset->comm, &MPI_dataset->myid)))
    SDDS_MPI_GOTO_ERROR(stderr,  "MPI_Comm_rank failed", mpi_code, 1);
    if (MPI_SUCCESS != (mpi_code=MPI_Comm_size(MPI_dataset->comm, &MPI_dataset->n_processors)))
    SDDS_MPI_GOTO_ERROR(stderr, "MPI_Comm_size failed", mpi_code, 1); */

  MPI_dataset->fpdeb = NULL;

  if (flags & SDDS_MPI_WRITE_ONLY)
    mpi_amode = MPI_MODE_CREATE | MPI_MODE_WRONLY;
  if (flags & SDDS_MPI_READ_ONLY)
    mpi_amode = MPI_MODE_RDONLY;
  if (flags & SDDS_MPI_READ_WRITE)
    mpi_amode = MPI_MODE_CREATE | MPI_MODE_RDWR;
  if (MPI_SUCCESS != (mpi_code = MPI_File_open(MPI_dataset->comm, filename, mpi_amode, MPI_INFO_NULL, &(MPI_dataset->MPI_file))))
    SDDS_MPI_GOTO_ERROR(stderr, "MPI_File_open failed", mpi_code, 1);
  else
    file_opened = 1;
  MPI_dataset->n_page = 0;
  if (mpi_amode & MPI_MODE_WRONLY)
    MPI_File_set_size(MPI_dataset->MPI_file, 0);
  if (mpi_amode != MPI_MODE_RDONLY) {
    if (MPI_SUCCESS != (mpi_code = MPI_File_sync(MPI_dataset->MPI_file)))
      SDDS_MPI_GOTO_ERROR(stderr, "MPI_File_sync failed", mpi_code, 1);
  }
  /*MPI_dataset->file_offset = 0; */
  return file_opened;
}

/**
 * @brief Creates a namelist field string from a name and value.
 *
 * This function allocates and constructs a string representing a namelist field in the format `name=value` or `name="value"` depending on the content of the value. It handles escaping of double quotes within the value and ensures proper formatting based on the presence of special characters.
 *
 * @param name The name of the field.
 * @param value The value of the field.
 * @return A newly allocated string containing the namelist field, or `NULL` if the name is `NULL` or empty.
 */
char *SDDS_CreateNamelistField(char *name, char *value) {
  char *contents;
  char *buffer = NULL, *bPtr, *vPtr, buf2[2048];

  contents = NULL;
  if (!name)
    return NULL;
  if (!value || !strlen(name))
    return NULL;
  contents = (char *)malloc(sizeof(char) * 2048);
  contents[0] = 0;
  if (!strlen(value))
    sprintf(contents, "%s=\"\", ", name);
  else {
    if (strchr(value, '"')) {
      if (!(buffer = SDDS_Malloc(sizeof(*buffer) * 2 * strlen(value))))
        return 0;
      vPtr = value;
      bPtr = buffer;
      while (*vPtr) {
        if (*vPtr == '"')
          *bPtr++ = '\\';
        *bPtr++ = *vPtr++;
      }
      *bPtr = 0;
      value = buffer;
    }
    if (strpbrk(value, " ,*$\t\n\b")) {
      //sprintf(contents, "%s%s=\"%s\", ", contents, name, value);
      sprintf(buf2, "%s=\"%s\", ", name, value);
      contents = strcat(contents, buf2);
    } else {
      ///sprintf(contents, "%s%s=%s, ", contents, name, value);
      sprintf(buf2, "%s=%s, ", name, value);
      contents = strcat(contents, buf2);
    }
    if (buffer)
      free(buffer);
  }
  return contents;
}

/**
 * @brief Creates a description block for the SDDS layout.
 *
 * This function allocates and constructs a description block containing the provided text and contents. It formats the description in the SDDS namelist format.
 *
 * @param text The descriptive text.
 * @param contents The contents of the description.
 * @return A newly allocated string containing the description block, or `NULL` if both `text` and `contents` are `NULL`.
 */
char *SDDS_CreateDescription(char *text, char *contents) {
  char *desc = NULL;
  if (!text && !contents)
    return NULL;
  desc = (char *)malloc(sizeof(char) * 2048);
  desc[0] = 0;
  sprintf(desc, "&description ");
  if (text)
    strcat(desc, SDDS_CreateNamelistField("text", text));
  if (contents)
    strcat(desc, SDDS_CreateNamelistField("contents", contents));
  //sprintf(desc, "%s&end\n", desc);
  desc = strcat(desc, "&end\n");
  return desc;
}

/**
 * @brief Creates a parameter definition block for the SDDS layout.
 *
 * This function allocates and constructs a parameter definition block based on the provided `PARAMETER_DEFINITION` structure. It includes fields such as `name`, `symbol`, `units`, `description`, `format_string`, `type`, and `fixed_value`.
 *
 * @param parameter_definition Pointer to the `PARAMETER_DEFINITION` structure.
 * @return A newly allocated string containing the parameter definition block.
 */
char *SDDS_CreateParameterDefinition(PARAMETER_DEFINITION *parameter_definition) {
  char *par, *tmpstr;
  par = (char *)malloc(sizeof(char) * 2048);
  par[0] = 0;
  sprintf(par, "&parameter ");
  strcat(par, SDDS_CreateNamelistField("name", parameter_definition->name));
  if ((tmpstr = BlankToNull(parameter_definition->symbol)))
    strcat(par, SDDS_CreateNamelistField("symbol", tmpstr));
  if ((tmpstr = BlankToNull(parameter_definition->units)))
    strcat(par, SDDS_CreateNamelistField("units", tmpstr));
  if ((tmpstr = BlankToNull(parameter_definition->description)))
    strcat(par, SDDS_CreateNamelistField("description", tmpstr));
  if ((tmpstr = BlankToNull(parameter_definition->format_string)))
    strcat(par, SDDS_CreateNamelistField("format_string", tmpstr));
  strcat(par, SDDS_CreateNamelistField("type", SDDS_type_name[parameter_definition->type - 1]));
  /*if ((tmpstr=BlankToNull(parameter_definition->fixed_value)))
    strcat(par, SDDS_CreateNamelistField("fixed_value", tmpstr)); */
  if (parameter_definition->fixed_value)
    strcat(par, SDDS_CreateNamelistField("fixed_value", parameter_definition->fixed_value));
  strcat(par, "&end\n");
  return par;
}

/**
 * @brief Creates a column definition block for the SDDS layout.
 *
 * This function allocates and constructs a column definition block based on the provided `COLUMN_DEFINITION` structure. It includes fields such as `name`, `symbol`, `units`, `description`, `format_string`, `type`, and ends the block.
 *
 * @param column_definition Pointer to the `COLUMN_DEFINITION` structure.
 * @return A newly allocated string containing the column definition block.
 */
char *SDDS_CreateColumnDefinition(COLUMN_DEFINITION *column_definition) {
  char *col = NULL, *tmpStr;
  col = (char *)malloc(sizeof(char) * 2048);
  col[0] = 0;
  sprintf(col, "&column ");
  strcat(col, SDDS_CreateNamelistField("name", column_definition->name));
  if ((tmpStr = BlankToNull(column_definition->symbol)))
    strcat(col, SDDS_CreateNamelistField("symbol", tmpStr));
  if ((tmpStr = BlankToNull(column_definition->units)))
    strcat(col, SDDS_CreateNamelistField("units", tmpStr));
  if ((tmpStr = BlankToNull(column_definition->description)))
    strcat(col, SDDS_CreateNamelistField("description", tmpStr));
  if ((tmpStr = BlankToNull(column_definition->format_string)))
    strcat(col, SDDS_CreateNamelistField("format_string", tmpStr));
  strcat(col, SDDS_CreateNamelistField("type", SDDS_type_name[column_definition->type - 1]));
  strcat(col, " &end\n");
  return col;
}

/**
 * @brief Creates an array definition block for the SDDS layout.
 *
 * This function allocates and constructs an array definition block based on the provided `ARRAY_DEFINITION` structure. It includes fields such as `name`, `symbol`, `units`, `description`, `format_string`, `group_name`, `type`, `dimensions`, and ends the block.
 *
 * @param array_definition Pointer to the `ARRAY_DEFINITION` structure.
 * @return A newly allocated string containing the array definition block.
 */
char *SDDS_CreateArrayDefinition(ARRAY_DEFINITION *array_definition) {
  char *array = NULL, *tmpstr;
  char buf[40];
  array = (char *)malloc(sizeof(char) * 2048);
  array[0] = 0;
  sprintf(array, "&array ");
  strcat(array, SDDS_CreateNamelistField("name", array_definition->name));
  if ((tmpstr = BlankToNull(array_definition->symbol)))
    strcat(array, SDDS_CreateNamelistField("symbol", tmpstr));
  if ((tmpstr = BlankToNull(array_definition->units)))
    strcat(array, SDDS_CreateNamelistField("units", tmpstr));
  if ((tmpstr = BlankToNull(array_definition->description)))
    strcat(array, SDDS_CreateNamelistField("description", tmpstr));
  if ((tmpstr = BlankToNull(array_definition->format_string)))
    strcat(array, SDDS_CreateNamelistField("format_string", tmpstr));
  if ((tmpstr = BlankToNull(array_definition->group_name)))
    strcat(array, SDDS_CreateNamelistField("group_name", tmpstr));
  strcat(array, SDDS_CreateNamelistField("type", SDDS_type_name[array_definition->type - 1]));
  if (array_definition->dimensions != 1) /* 1 is default */
  {
    //sprintf(array, "%sdimensions=%" PRId32 ", ", array, array_definition->dimensions);
    sprintf(buf, "dimensions=%" PRId32 ", ", array_definition->dimensions);
    strcat(array, buf);
  }
  strcat(array, " &end\n");
  return array;
}

/**
 * @brief Creates an associate definition block for the SDDS layout.
 *
 * This function allocates and constructs an associate definition block based on the provided `ASSOCIATE_DEFINITION` structure. It includes fields such as `name`, `filename`, `contents`, `path`, `description`, and the associated SDDS index.
 *
 * @param associate_definition Pointer to the `ASSOCIATE_DEFINITION` structure.
 * @return A newly allocated string containing the associate definition block.
 */
char *SDDS_CreateAssociateDefinition(ASSOCIATE_DEFINITION *associate_definition) {
  char *associate = NULL, *tmpstr;
  char buf[40];
  associate = (char *)malloc(sizeof(char) * 2048);
  associate[0] = 0;
  sprintf(associate, "&associate ");
  strcat(associate, SDDS_CreateNamelistField("name", associate_definition->name));
  if ((tmpstr = BlankToNull(associate_definition->filename)))
    strcat(associate, SDDS_CreateNamelistField("filename", tmpstr));
  if ((tmpstr = BlankToNull(associate_definition->contents)))
    strcat(associate, SDDS_CreateNamelistField("contents", tmpstr));
  if ((tmpstr = BlankToNull(associate_definition->path)))
    strcat(associate, SDDS_CreateNamelistField("path", tmpstr));
  if ((tmpstr = BlankToNull(associate_definition->description)))
    strcat(associate, SDDS_CreateNamelistField("description", tmpstr));
  //sprintf(associate, "%ssdds=%" PRId32, associate, associate_definition->sdds);
  sprintf(buf, "sdds=%" PRId32 " &end\n", associate_definition->sdds);
  strcat(associate, buf);
  //strcat(associate, " &end\n");
  return associate;
  ;
}

/**
 * @brief Creates a data mode block for the SDDS layout.
 *
 * This function allocates and constructs a data mode block based on the provided `DATA_MODE` structure. It includes fields such as `mode`, `lines_per_row`, `no_row_counts`, `column_major_order`, and ends the block.
 *
 * @param data_mode Pointer to the `DATA_MODE` structure.
 * @return A newly allocated string containing the data mode block, or `NULL` if the mode is invalid.
 */
char *SDDS_CreateDataMode(DATA_MODE *data_mode) {
  char *mode = NULL;
  char buf[40];
  if (data_mode->mode > SDDS_NUM_DATA_MODES)
    return NULL;
  mode = (char *)malloc(sizeof(char) * 2048);
  mode[0] = 0;
  sprintf(mode, "&data ");
  strcat(mode, SDDS_CreateNamelistField("mode", SDDS_data_mode[data_mode->mode - 1]));
  if (data_mode->lines_per_row > 1) {
    //sprintf(mode, "%slines_per_row=%" PRId32 ", ", mode, data_mode->lines_per_row);
    sprintf(buf, "lines_per_row=%" PRId32 ", ", data_mode->lines_per_row);
    strcat(mode, buf);
  }
  if (data_mode->no_row_counts) {
    //sprintf(mode, "%sno_row_counts=1, ", mode);
    strcat(mode, "no_row_counts=1, ");
  }
  if (data_mode->column_major) {
    strcat(mode, "column_major_order=1, ");
  }
  strcat(mode, "&end\n");
  return mode;
}

/**
 * @brief Writes an ASCII string to the SDDS dataset using MPI.
 *
 * This function writes the provided string to the SDDS dataset's MPI file. It handles buffering to optimize writes and manages buffer overflows by writing buffer contents to the file when necessary. Returns `1` on success, `0` on failure.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure.
 * @param string The ASCII string to write.
 * @return `1` if the string was successfully written, `0` otherwise.
 */
int32_t SDDS_MPI_WriteAsciiString(SDDS_DATASET *SDDS_dataset, char *string) {
  SDDS_FILEBUFFER *fBuffer;
  MPI_DATASET *MPI_dataset = SDDS_dataset->MPI_dataset;
  int32_t mpi_code;
  size_t targetSize;

  fBuffer = &(SDDS_dataset->fBuffer);
  targetSize = strlen(string) * sizeof(char);

  if (!fBuffer->bufferSize) {
    if ((mpi_code = MPI_File_write(MPI_dataset->MPI_file, string, targetSize, MPI_CHAR, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_WriteBufferedWrite(MPI_File_write_at failed)", mpi_code, 0);
      return 0;
    }
    return 1;
  }
  if ((fBuffer->bytesLeft -= targetSize) >= 0) {
    memcpy((char *)fBuffer->data, (char *)string, targetSize);
    fBuffer->data += targetSize;
    return 1;
  } else {
    int32_t lastLeft;
    /* add back what was subtracted in test above.
       * lastLeft is the number of bytes left in the buffer before doing anything 
       * and also the number of bytes from the users data that get copied into the buffer.
       */
    lastLeft = (fBuffer->bytesLeft += targetSize);
    /* copy part of the data into the buffer and write the buffer out */
    memcpy((char *)fBuffer->data, (char *)string, (size_t)fBuffer->bytesLeft);
    if ((mpi_code = MPI_File_write(MPI_dataset->MPI_file, fBuffer->buffer, (int)(fBuffer->bufferSize), MPI_CHAR, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
      SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_WriteBufferedWrite(MPI_File_write_at failed)", mpi_code, 0);
      return 0;
    }

    /* reset the data pointer and the bytesLeft value.
       * also, determine if the remaining data is too large for the buffer.
       * if so, just write it out.
       */
    fBuffer->data = fBuffer->buffer;
    if ((targetSize -= lastLeft) > (fBuffer->bytesLeft = fBuffer->bufferSize)) {
      if ((mpi_code = MPI_File_write_at(MPI_dataset->MPI_file, (MPI_Offset)(MPI_dataset->file_offset), string + lastLeft, targetSize, MPI_BYTE, MPI_STATUS_IGNORE)) != MPI_SUCCESS) {
        SDDS_MPI_GOTO_ERROR(stderr, "SDDS_MPI_WriteBufferedWrite(MPI_File_write_at failed)", mpi_code, 0);
        return 0;
      }
      return 1;
    }
    /* copy remaining data into the buffer.
       * could do this with a recursive call, but this is more efficient.
       */
    memcpy((char *)fBuffer->data, (char *)string + lastLeft, targetSize);
    fBuffer->data += targetSize;
    fBuffer->bytesLeft -= targetSize;
    return 1;
  }
}

/**
 * @brief Writes the layout of the SDDS dataset to the MPI file.
 *
 * This function writes the SDDS dataset layout information to the associated MPI file. It sets the file view, checks and saves the dataset, determines the byte order, allocates and manages the write buffer, and writes various layout components including description, parameters, arrays, columns, associates, and data mode. It ensures proper synchronization and handles errors appropriately. Only the MPI process with rank 0 performs the actual write operations.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure.
 * @return `1` if the layout was successfully written, `0` otherwise.
 */
int32_t SDDS_MPI_WriteLayout(SDDS_DATASET *SDDS_dataset) {
  MPI_DATASET *MPI_dataset;
  SDDS_LAYOUT *layout;
  char buf[2048], *field = NULL;
  int32_t i, data_mode;
  char *outputEndianess = NULL;
  SDDS_FILEBUFFER *fBuffer = NULL;

#if MPI_DEBUG
  logDebug("SDDS_MPI_WriteLayout", SDDS_dataset);
#endif

  MPI_dataset = SDDS_dataset->MPI_dataset;
  MPI_File_set_view(MPI_dataset->MPI_file, 0, MPI_CHAR, MPI_CHAR, "native", MPI_INFO_NULL);
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_WriteLayout"))
    return 0;
  if (!SDDS_SaveLayout(SDDS_dataset))
    return 0;
  layout = &SDDS_dataset->layout;
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't write layout--file is disconnected (SDDS_MPI_WriteLayout)");
    return 0;
  }
  MPI_dataset->file_offset = 0;
  if (layout->layout_written) {
    SDDS_SetError("Can't write layout--already written to file (SDDS_MPI_WriteLayout)");
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

  fBuffer = &(SDDS_dataset->fBuffer);
  /* write out the layout data */
  if (MPI_dataset->myid == 0) {
    if (!fBuffer->buffer) {
      fBuffer->bufferSize = SDDS_SetDefaultWriteBufferSize(-1);
      if (!(fBuffer->buffer = fBuffer->data = SDDS_Malloc(sizeof(char) * (fBuffer->bufferSize + 1)))) {
        SDDS_SetError("Unable to do buffered read--allocation failure (SDDS_MPI_WriteLayout)");
        return 0;
      }
      fBuffer->bytesLeft = fBuffer->bufferSize;
      fBuffer->data[0] = 0;
    }
  }
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
    SDDS_SetError("Operating system does not support 80bit float variables used by SDDS_LONGDOUBLE (SDDS_MPI_WriteLayout)");
    return 0;
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
  //layout->version = 5;
  sprintf(buf, "SDDS%" PRId32 "\n", layout->version);

  MPI_dataset->file_offset += strlen(buf) * sizeof(char);
  data_mode = SDDS_dataset->layout.data_mode.mode;
  if (MPI_dataset->myid == 0 && !SDDS_MPI_WriteAsciiString(SDDS_dataset, buf)) {
    fprintf(stderr, "SDDS_MPI_WriteLayout(error1): Unable to write layout.\n");
    return 0;
  }
  if (data_mode == SDDS_BINARY) {
    if (layout->byteOrderDeclared == SDDS_BIGENDIAN)
      sprintf(buf, "!# big-endian\n");
    else
      sprintf(buf, "!# little-endian\n");
    if (MPI_dataset->myid == 0 && !SDDS_MPI_WriteAsciiString(SDDS_dataset, buf)) {
      fprintf(stderr, "SDDS_MPI_WriteLayout(error1): Unable to write layout.\n");
      return 0;
    }
    MPI_dataset->file_offset += strlen(buf) * sizeof(char);
  }
  if (SDDS_dataset->layout.data_mode.fixed_row_count) {
    sprintf(buf, "!# fixed-rowcount\n");
    if (MPI_dataset->myid == 0 && !SDDS_MPI_WriteAsciiString(SDDS_dataset, buf)) {
      fprintf(stderr, "SDDS_MPI_WriteLayout(error1): Unable to write layout.\n");
      return 0;
    }
    MPI_dataset->file_offset += strlen(buf) * sizeof(char);
  }
  field = NULL;
  if ((field = SDDS_CreateDescription(layout->description, layout->contents)) != NULL) {
    if (MPI_dataset->myid == 0 && !SDDS_MPI_WriteAsciiString(SDDS_dataset, field)) {
      fprintf(stderr, "SDDS_MPI_WriteLayout(error1): Unable to write layout.\n");
      return 0;
    }
    MPI_dataset->file_offset += strlen(field) * sizeof(char);
    free(field);
    field = NULL;
  }

  for (i = 0; i < layout->n_parameters; i++) {
    field = SDDS_CreateParameterDefinition(layout->parameter_definition + i);
    if (MPI_dataset->myid == 0 && !SDDS_MPI_WriteAsciiString(SDDS_dataset, field)) {
      fprintf(stderr, "SDDS_MPI_WriteLayout(error1): Unable to write layout.\n");
      return 0;
    }
    MPI_dataset->file_offset += strlen(field) * sizeof(char);
    free(field);
    field = NULL;
  }
  for (i = 0; i < layout->n_arrays; i++) {
    field = SDDS_CreateArrayDefinition(layout->array_definition + i);
    if (MPI_dataset->myid == 0 && !SDDS_MPI_WriteAsciiString(SDDS_dataset, field)) {
      fprintf(stderr, "SDDS_MPI_WriteLayout(error1): Unable to write layout.\n");
      return 0;
    }
    MPI_dataset->file_offset += strlen(field) * sizeof(char);
    free(field);
    field = NULL;
  }
  for (i = 0; i < layout->n_columns; i++) {
    field = SDDS_CreateColumnDefinition(layout->column_definition + i);
    if (MPI_dataset->myid == 0 && !SDDS_MPI_WriteAsciiString(SDDS_dataset, field)) {
      fprintf(stderr, "SDDS_MPI_WriteLayout(error1): Unable to write layout.\n");
      return 0;
    }
    MPI_dataset->file_offset += strlen(field) * sizeof(char);
    free(field);
    field = NULL;
  }

#if RW_ASSOCIATES != 0
  for (i = 0; i < layout->n_associates; i++) {
    field = SDDS_CreateAssociateDefinition(layout->associate_definition + i);
    if (MPI_dataset->myid == 0 && !SDDS_MPI_WriteAsciiString(SDDS_dataset, field)) {
      fprintf(stderr, "SDDS_MPI_WriteLayout(error1): Unable to write layout.\n");
      return 0;
    }
    MPI_dataset->file_offset += strlen(field) * sizeof(char);
    free(field);
    field = NULL;
  }
#endif
  if ((field = SDDS_CreateDataMode(&layout->data_mode)) != NULL) {
    if (MPI_dataset->myid == 0 && !SDDS_MPI_WriteAsciiString(SDDS_dataset, field)) {
      fprintf(stderr, "SDDS_MPI_WriteLayout(error1): Unable to write layout.\n");
      return 0;
    }
    MPI_dataset->file_offset += strlen(field) * sizeof(char);
    free(field);
    field = NULL;
  }
  MPI_dataset->column_offset = SDDS_MPI_Get_Column_Size(SDDS_dataset);
  layout->layout_written = 1;
  if (MPI_dataset->myid == 0) {
    if (!SDDS_MPI_FlushBuffer(SDDS_dataset))
      return 0;
  }
  SDDS_dataset->original_layout.version = layout->version;
  return (1);
}

/**
 * @brief Terminates the program after handling errors and cleaning up MPI resources.
 *
 * This function prints all accumulated SDDS errors to `stderr` using `SDDS_PrintErrors`.
 * If a custom error message (`text`) is provided, it is printed to `stderr`.
 * If an MPI file pointer (`mpi_file`) is provided, the MPI file is closed.
 * Finally, the MPI environment is finalized, and the program exits with a status of 1.
 *
 * @param text A custom error message to display. Can be `NULL`.
 * @param mpi_file Pointer to an `MPI_File` to be closed. Can be `NULL`.
 */
void SDDS_MPI_BOMB(char *text, MPI_File *mpi_file) {
  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  if (text)
    fprintf(stderr, "Error: %s\n", text);
  if (mpi_file)
    MPI_File_close(mpi_file);
  MPI_Finalize();
  exit(1);
}

/**
 * @brief Writes a page of data to the MPI file associated with the SDDS dataset.
 *
 * This function checks the validity of the dataset and ensures that the layout has been written
 * and that the file is connected. If these conditions are met, it proceeds to write a binary
 * page using `SDDS_MPI_WriteBinaryPage`.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure containing the dataset information.
 * @return `1` if the page was successfully written, `0` otherwise.
 */
int32_t SDDS_MPI_WritePage(SDDS_DATASET *SDDS_dataset) {
  int32_t result;

#if MPI_DEBUG
  logDebug("SDDS_MPI_WritePage", SDDS_dataset);
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
  result = SDDS_MPI_WriteBinaryPage(SDDS_dataset);
  return result;
}

/**
 * @brief Converts an SDDS data type to the corresponding MPI data type.
 *
 * This function maps custom SDDS data types to their equivalent MPI data types using a switch statement.
 * If an unknown SDDS data type is provided, the function calls `SDDS_Bomb` to handle the error.
 *
 * @param SDDS_type The SDDS data type to convert.
 * @return The corresponding `MPI_Datatype`.
 */
MPI_Datatype Convert_SDDStype_To_MPItype(int32_t SDDS_type) {
  switch (SDDS_type) {
  case SDDS_SHORT:
    return MPI_SHORT;
  case SDDS_USHORT:
    return MPI_UNSIGNED_SHORT;
  case SDDS_LONG:
    /* SDDS_LONG type is actually int32_t */
    return MPI_INT;
  case SDDS_ULONG:
    return MPI_UNSIGNED;
  case SDDS_LONG64:
    return MPI_INT64_T;
  case SDDS_ULONG64:
    return MPI_UINT64_T;
  case SDDS_FLOAT:
    return MPI_FLOAT;
  case SDDS_DOUBLE:
    return MPI_DOUBLE;
  case SDDS_LONGDOUBLE:
    return MPI_LONG_DOUBLE;
  case SDDS_STRING:
  case SDDS_CHARACTER:
    return MPI_CHAR;
  default:
    SDDS_Bomb("Unknown SDDS datatype provided to ConvertSDDS_To_MPI.");
    return 0;
  }
}

/**
 * @brief Terminates the SDDS dataset by freeing all allocated resources and closing MPI files.
 *
 * This function performs comprehensive cleanup of the `SDDS_DATASET` structure, freeing all
 * dynamically allocated memory associated with parameters, arrays, columns, and associates.
 * It also closes the MPI file, finalizes MPI, and resets the dataset's layout and data structures.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure to terminate.
 * @return `1` if termination was successful, `0` otherwise.
 */
int32_t SDDS_MPI_Terminate(SDDS_DATASET *SDDS_dataset) {
  MPI_DATASET *MPI_dataset;
  SDDS_LAYOUT *layout;
  char **ptr;
  int32_t i, j;

#if MPI_DEBUG
  logDebug("SDDS_MPI_Terminate", SDDS_dataset);
#endif

  MPI_dataset = SDDS_dataset->MPI_dataset;
  layout = &(SDDS_dataset->original_layout);
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_Terminate"))
    return (0);
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
  if (SDDS_dataset->titleBuffer.buffer)
    free(SDDS_dataset->titleBuffer.buffer);
  if (SDDS_dataset->parameter) {
    for (i = 0; i < layout->n_parameters; i++) {
      if (layout->parameter_definition[i].type == SDDS_STRING && *(char **)(SDDS_dataset->parameter[i]))
        free(*(char **)(SDDS_dataset->parameter[i]));
      if (SDDS_dataset->parameter[i])
        free(SDDS_dataset->parameter[i]);
    }
    free(SDDS_dataset->parameter);
  }
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
  if (SDDS_dataset->data) {
    for (i = 0; i < layout->n_columns; i++)
      if (SDDS_dataset->data[i]) {
        if (layout->column_definition[i].type == SDDS_STRING && !(terminateMode & TERMINATE_DONT_FREE_TABLE_STRINGS)) {
          ptr = (char **)SDDS_dataset->data[i];
          for (j = 0; j < SDDS_dataset->n_rows_allocated; j++, ptr++)
            if (*ptr)
              free(*ptr);
        }
        free(SDDS_dataset->data[i]);
      }
    free(SDDS_dataset->data);
  }
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
  MPI_File_close(&(MPI_dataset->MPI_file));
  free(MPI_dataset);
  MPI_dataset = NULL;
  return (1);
}

/**
 * @brief Initializes the SDDS dataset for MPI output.
 *
 * This function sets up the SDDS dataset for parallel output using MPI. It initializes the dataset with
 * the provided description, contents, and filename, sets the column major order if specified, and opens
 * the MPI file for writing. It also logs the initialization if MPI debugging is enabled.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure to initialize.
 * @param description A description of the dataset.
 * @param contents Additional contents for the dataset description.
 * @param filename The name of the file to initialize for output.
 * @param flags Flags indicating the mode in which to open the file (e.g., read-only, write-only, read-write).
 * @param column_major If non-zero, data will be written in column-major order.
 * @return `1` if initialization was successful, `0` otherwise.
 */
int32_t SDDS_MPI_InitializeOutput(SDDS_DATASET *SDDS_dataset, char *description, char *contents, char *filename, unsigned long flags, short column_major) {
  MPI_DATASET *MPI_dataset = SDDS_dataset->MPI_dataset;

  if (!SDDS_Parallel_InitializeOutput(SDDS_dataset, description, contents, filename))
    return 0;
  if (flags & SDDS_MPI_READ_ONLY) {
    SDDS_SetError("Wrong flags pass (SDDS_MPI_READ_ONLY) passed to SDDS_MPI_InitializeOutput)!");
    return 0;
  }
  /*  SDDS_dataset->MPI_dataset = MPI_dataset; */
  SDDS_dataset->layout.data_mode.column_major = column_major;
  if (!SDDS_MPI_File_Open(MPI_dataset, filename, flags)) {
    SDDS_SetError("Failed in opening file for MPI output!");
    return 0;
  }
#if MPI_DEBUG
  logDebug("SDDS_MPI_InitializeOutput", SDDS_dataset);
#endif
  return 1;
}

/**
 * @brief Initializes a copy of an SDDS dataset for MPI output.
 *
 * This function creates a new SDDS dataset (`SDDS_target`) by copying the layout from an existing
 * dataset (`SDDS_source`). It sets up the target dataset for MPI output, opens the specified file,
 * and writes the layout to the file. The function also sets the data mode to column-major if specified.
 *
 * @param SDDS_target Pointer to the target `SDDS_DATASET` structure to initialize.
 * @param SDDS_source Pointer to the source `SDDS_DATASET` structure to copy from.
 * @param filename The name of the file to initialize for output.
 * @param column_major If non-zero, data will be written in column-major order.
 * @return `1` if the copy and initialization were successful, `0` otherwise.
 */
int32_t SDDS_MPI_InitializeCopy(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, char *filename, short column_major) {
  MPI_DATASET *MPI_target = SDDS_target->MPI_dataset;

  if (!SDDS_CheckDataset(SDDS_source, "SDDS_InitializeCopy"))
    return (0);
  if (!SDDS_CheckDataset(SDDS_target, "SDDS_InitializeCopy"))
    return (0);
  /* if (!SDDS_ZeroMemory((void *)SDDS_target, sizeof(SDDS_DATASET))) {
     SDDS_SetError("Unable to copy layout--can't zero SDDS_DATASET structure (SDDS_InitializeCopy)");
     return(0);
     } */
  /*has been zeroed in the setup */
  SDDS_target->pagecount_offset = NULL;
  SDDS_target->mode = SDDS_WRITEMODE;
  SDDS_target->layout.popenUsed = 0;
  SDDS_target->layout.gzipFile = 0;
  SDDS_target->layout.lzmaFile = 0;
  if (filename) {
    if (!SDDS_CopyString(&SDDS_target->layout.filename, filename)) {
      SDDS_SetError("Memory allocation failure (SDDS_InitializeCopy)");
      return (0);
    }
  }
  SDDS_target->page_number = SDDS_target->page_started = 0;
  if (!SDDS_CopyLayout(SDDS_target, SDDS_source))
    return (0);
  /*  SDDS_target->MPI_dataset = MPI_target; */
  SDDS_target->layout.data_mode.column_major = column_major;
  if (!SDDS_MPI_File_Open(MPI_target, filename, SDDS_MPI_WRITE_ONLY))
    return 0;
  SDDS_target->parallel_io = 1;
  MPI_target->file_offset = 0;
  if (!SDDS_MPI_WriteLayout(SDDS_target))
    return 0;
  return 1;
}

/**
 * @brief Sets up the SDDS dataset for MPI operations.
 *
 * This function initializes the `SDDS_DATASET` structure for MPI-based parallel I/O. It zeroes out the dataset,
 * allocates and initializes the `MPI_DATASET` structure if parallel I/O is enabled, and sets MPI-related fields
 * such as the number of processors, process ID, and MPI communicator.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure to set up.
 * @param parallel_io If non-zero, the dataset will be set up for parallel I/O using MPI.
 * @param n_processors The total number of MPI processes.
 * @param myid The MPI process ID of the current process.
 * @param comm The MPI communicator to use for I/O operations.
 * @param master_read If non-zero, the master process will handle read operations.
 */
void SDDS_MPI_Setup(SDDS_DATASET *SDDS_dataset, int32_t parallel_io, int32_t n_processors, int32_t myid, MPI_Comm comm, short master_read) {
  MPI_DATASET *MPI_dataset;

  if (!SDDS_ZeroMemory((void *)SDDS_dataset, sizeof(SDDS_DATASET)))
    SDDS_Bomb("Unable to zero memory for SDDS dataset(SDDS_MPI_Setup)");
  if (parallel_io) {
    MPI_dataset = malloc(sizeof(*MPI_dataset));
    if (!SDDS_ZeroMemory((void *)MPI_dataset, sizeof(MPI_DATASET)))
      SDDS_Bomb("Unable to zero memory for MPI_DATASEAT (SDDS_MPI_Setup)");
    MPI_dataset->n_processors = n_processors;
    MPI_dataset->myid = myid;
    MPI_dataset->comm = comm;
    MPI_dataset->collective_io = 0;
    MPI_dataset->master_read = master_read;
    MPI_dataset->fpdeb = NULL;
    SDDS_dataset->MPI_dataset = MPI_dataset;
    SDDS_dataset->parallel_io = 1;
  }
}

/**
 * @brief Disconnects the MPI file associated with the SDDS dataset.
 *
 * This function checks the validity of the dataset and ensures that the file is connected and has a valid filename.
 * It then closes the MPI file and marks the dataset as disconnected.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure whose file is to be disconnected.
 * @return `1` if the file was successfully disconnected, `0` otherwise.
 */
int32_t SDDS_MPI_DisconnectFile(SDDS_DATASET *SDDS_dataset) {
  MPI_DATASET *MPI_dataset;

#if MPI_DEBUG
  logDebug("SDDS_MPI_DisconnectFile", SDDS_dataset);
#endif

  MPI_dataset = SDDS_dataset->MPI_dataset;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_DisconnectFile"))
    return 0;
  if (!SDDS_dataset->layout.filename) {
    SDDS_SetError("Can't disconnect file.  No filename or gzip file. (SDDS_MPI_DisconnectFile)");
    return 0;
  }
  if (SDDS_dataset->layout.disconnected) {
    SDDS_SetError("Can't disconnect file.  Already disconnected. (SDDS_MPI_DisconnectFile)");
    return 0;
  }
  /* if (SDDS_dataset->page_started && !SDDS_WritePage(SDDS_dataset)) {
     SDDS_SetError("Can't disconnect file.  Problem updating page. (SDDS_MPI_DisconnectFile)");
     return 0;
     } */
  SDDS_dataset->layout.disconnected = 1;
  MPI_File_close(&(MPI_dataset->MPI_file));
  return 1;
}

/**
 * @brief Reconnects the MPI file associated with the SDDS dataset.
 *
 * This function checks the validity of the dataset and ensures that the file is currently disconnected
 * and has a valid filename. It then reopens the MPI file in read-write mode, updates the file view,
 * and marks the dataset as connected.
 *
 * @param SDDS_dataset Pointer to the `SDDS_DATASET` structure whose file is to be reconnected.
 * @return `1` if the file was successfully reconnected, `0` otherwise.
 */
int32_t SDDS_MPI_ReconnectFile(SDDS_DATASET *SDDS_dataset) {
  MPI_DATASET *MPI_dataset;

#if MPI_DEBUG
  logDebug("SDDS_MPI_ReconnectFile", SDDS_dataset);
#endif
  MPI_dataset = SDDS_dataset->MPI_dataset;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_MPI_ReconnectFile"))
    return 0;
  if (!SDDS_dataset->layout.disconnected || !SDDS_dataset->layout.filename) {
    SDDS_SetError("Can't reconnect file.  Not disconnected or missing filename. (SDDS_MPI_ReconnectFile)");
    return 0;
  }
  if (MPI_File_open(MPI_dataset->comm, SDDS_dataset->layout.filename, MPI_MODE_RDWR, MPI_INFO_NULL, &(MPI_dataset->MPI_file)) != MPI_SUCCESS) {
    SDDS_SetError("Can't reconnect file, MPI_File_open failed. (SDDS_MPI_ReconnectFile)");
    return 0;
  }
  MPI_File_get_size(MPI_dataset->MPI_file, &(MPI_dataset->file_offset));
  MPI_File_set_view(MPI_dataset->MPI_file, MPI_dataset->file_offset, MPI_BYTE, MPI_BYTE, "native", MPI_INFO_NULL);
  /*  MPI_dataset->file_offset=0; */
  SDDS_dataset->layout.disconnected = 0;
  return 1;
}
