/**
 * @file sdds2headlessdata.c
 * @brief Converts SDDS files into binary data without headers.
 *
 * @details
 * This program reads an SDDS (Self Describing Data Set) file, extracts specified columns, 
 * and writes the data to a binary file in either row-major or column-major order. It supports 
 * flexible options for specifying columns, output order, and piping data. At least one column 
 * must be specified using `-column`. The program ensures compatibility of options and processes 
 * the input accordingly.
 *
 * @section Usage
 * ```
 * sdds2headlessdata [<input>] [<output>]
 *                   [-pipe=in|out]
 *                    -column=<name>
 *                   [-order={rowMajor|columnMajor}] 
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-column`     | Specifies the columns to include in the output.                            |
 *
 * | Optional        | Description                                                                 |
 * |---------------|-----------------------------------------------------------------------------|
 * | `-pipe`       | Allows piping data into or out of the program.                             |
 * | `-order`      | Specifies the output order for column data, either row-major or column-major order. |
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @author
 * R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#ifdef _WIN32
#  include <fcntl.h>
#  include <io.h>
#endif

#define ROW_ORDER 0
#define COLUMN_ORDER 1
#define ORDERS 2
static char *order_names[ORDERS] = {
  "rowMajor",
  "columnMajor",
};

/* Enumeration for option types */
enum option_type {
  SET_COLUMN,
  SET_PIPE,
  SET_ORDER,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "column",
  "pipe",
  "order",
};

static char *usage =
  "sdds2headlessdata [<input>] [<output>]\n"
  "                  [-pipe=in|out]\n"
  "                   -column=<name>\n"
  "                  [-order={rowMajor|columnMajor}] \n"
  "Options:\n"
  "-order:  Row major order is the default. Each row consists of one element\n"
  "         from each column. In column major order, each column is written entirely\n"
  "         on one row.\n"
  "-column: Provide the columns whose data are to be written.\n\n"
  "Program by Hairong Shang.\n"
  "Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

int main(int argc, char **argv) {
  FILE *file_id;
  SDDS_FILEBUFFER *f_buffer = NULL;

  SDDS_DATASET sdds_dataset, sdds_dummy;
  SCANNED_ARG *s_arg;
  long j, i_arg, retval, page_number = 0, size, column_order = 0;
  int64_t i, rows = 0;
  char *input = NULL, *output = NULL;
  unsigned long pipe_flags = 0;
  long no_warnings = 0, tmpfile_used = 0;

  long *column_type = NULL, *column_index = NULL;
  void **column_data = NULL;
  char **column = NULL, **column_match = NULL;
  int32_t column_matches = 0;
  int32_t columns = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    bomb(NULL, usage);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_ORDER:
        if (s_arg[i_arg].n_items != 2) {
          SDDS_Bomb("invalid -order syntax");
        }
        switch (match_string(s_arg[i_arg].list[1], order_names, ORDERS, 0)) {
        case ROW_ORDER:
          column_order = 0;
          break;
        case COLUMN_ORDER:
          column_order = 1;
          break;
        default:
          SDDS_Bomb("invalid -order syntax");
          break;
        }
        break;
      case SET_COLUMN:
        if ((s_arg[i_arg].n_items < 2)) {
          SDDS_Bomb("invalid -column syntax");
        }
        column_matches = s_arg[i_arg].n_items - 1;
        column_match = tmalloc(sizeof(*column_match) * column_matches);
        for (i = 0; i < column_matches; i++) {
          column_match[i] = s_arg[i_arg].list[i + 1];
        }
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags)) {
          fprintf(stderr, "Error (%s): invalid -pipe syntax\n", argv[0]);
          return 1;
        }
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (input == NULL) {
        input = s_arg[i_arg].list[0];
      } else if (output == NULL) {
        output = s_arg[i_arg].list[0];
      } else {
        fprintf(stderr, "too many filenames");
        exit(1);
      }
    }
  }

  processFilenames("sdds2headlessdata", &input, &output, pipe_flags, no_warnings, &tmpfile_used);

  if (!column_matches) {
    SDDS_Bomb("you must specify -column options");
  }

  if (!SDDS_InitializeInput(&sdds_dataset, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  column = getMatchingSDDSNames(&sdds_dataset, column_match, column_matches, &columns, SDDS_MATCH_COLUMN);
  if (!columns) {
    SDDS_Bomb("No columns found in the input file.");
  }

  column_type = tmalloc(sizeof(*column_type) * columns);
  column_index = tmalloc(sizeof(*column_index) * columns);
  column_data = tmalloc(sizeof(*column_data) * columns);
  for (i = 0; i < columns; i++) {
    if ((column_index[i] = SDDS_GetColumnIndex(&sdds_dataset, column[i])) < 0) {
      fprintf(stderr, "error: column %s does not exist\n", column[i]);
      exit(1);
    }
    if ((column_type[i] = SDDS_GetColumnType(&sdds_dataset, column_index[i])) <= 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }

  if (!output) {
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
      fprintf(stderr, "error: unable to set stdout to binary mode\n");
      exit(1);
    }
#endif
    file_id = stdout;
  } else {
    file_id = fopen(output, "wb");
  }
  if (file_id == NULL) {
    fprintf(stderr, "unable to open output file for writing\n");
    exit(1);
  }

  f_buffer = &sdds_dummy.fBuffer;
  f_buffer->buffer = NULL;
  if (!f_buffer->buffer) {
    if (!(f_buffer->buffer = f_buffer->data = SDDS_Malloc(sizeof(char) * SDDS_FILEBUFFER_SIZE))) {
      fprintf(stderr, "Unable to do buffered read--allocation failure\n");
      exit(1);
    }
    f_buffer->bufferSize = SDDS_FILEBUFFER_SIZE;
    f_buffer->bytesLeft = SDDS_FILEBUFFER_SIZE;
  }

  retval = -1;

  while (retval != page_number && (retval = SDDS_ReadPage(&sdds_dataset)) > 0) {
    if (page_number && retval != page_number) {
      continue;
    }
    if ((rows = SDDS_CountRowsOfInterest(&sdds_dataset)) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (rows) {
      if (column_order) {
        for (j = 0; j < columns; j++) {
          if (column_type[j] == SDDS_STRING) {
            for (i = 0; i < rows; i++) {
              if (!SDDS_WriteBinaryString(*((char **)sdds_dataset.data[column_index[j]] + i), file_id, f_buffer)) {
                fprintf(stderr, "Unable to write rows--failure writing string\n");
                exit(1);
              }
            }
          } else {
            size = SDDS_type_size[column_type[j] - 1];
            for (i = 0; i < rows; i++) {
              if (!SDDS_BufferedWrite((char *)sdds_dataset.data[column_index[j]] + i * size, size, file_id, f_buffer)) {
                fprintf(stderr, "Unable to write rows--failure writing string\n");
                exit(1);
              }
            }
          }
        }
      } else {
        for (i = 0; i < rows; i++) {
          for (j = 0; j < columns; j++) {
            if (column_type[j] == SDDS_STRING) {
              if (!SDDS_WriteBinaryString(*((char **)sdds_dataset.data[column_index[j]] + i), file_id, f_buffer)) {
                fprintf(stderr, "Unable to write rows--failure writing string\n");
                exit(1);
              }
            } else {
              size = SDDS_type_size[column_type[j] - 1];
              if (!SDDS_BufferedWrite((char *)sdds_dataset.data[column_index[j]] + i * size, size, file_id, f_buffer)) {
                fprintf(stderr, "Unable to write rows--failure writing string\n");
                exit(1);
              }
            }
          }
        }
      }
    }
    if (retval == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }
  if (!SDDS_FlushBuffer(file_id, f_buffer)) {
    SDDS_SetError("Unable to write page--buffer flushing problem (SDDS_WriteBinaryPage)");
    return 0;
  }
  fclose(file_id);

  if (!SDDS_Terminate(&sdds_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  exit(0);
}
