/**
 * @file sddsendian.c
 * @brief Converts between big-endian and little-endian formats for SDDS files.
 *
 * @details
 * This program processes Self-Describing Data Sets (SDDS) to convert data
 * between big-endian and little-endian byte orders. It supports both binary
 * and ASCII SDDS files and provides options for handling data piping and
 * specifying row or column major order.
 *
 * @section Usage
 * ```
 * sddsendian [<input>] [<output>]
 *            [-pipe=[input][,output]]
 *            [-nonNative]
 *            [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Option                        | Description                                                   |
 * |-------------------------------|---------------------------------------------------------------|
 * | `-pipe`                       | Use pipe for input and/or output. Requires specifying at least one of `input` or `output`. |
 * | `-nonNative`                  | Handle non-native byte order files.                          |
 * | `-majorOrder`                 | Set the major order to row or column. Must be followed by either `row` or `column`. |
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
 * M. Borland, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_PIPE,
  NONNATIVE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "pipe",
  "nonNative",
  "majorOrder",
};

char *USAGE = "sddsendian [<input>] [<output>]\n\
           [-pipe=[input][,output]]\n\
           [-nonNative]\n\
           [-majorOrder=row|column]\n\
\nOptions:\n\
  -pipe=[input][,output]   Use pipe for input and/or output.\n\
  -majorOrder=row|column   Set the major order to row or column.\n\
  -nonNative               Handle non-native byte order files.\n\
\nDescription:\n\
  Converts between big-endian and little-endian formats.\n\
  This program is designed to handle Self-Describing Data Sets (SDDS)\n\
  efficiently, allowing for platform-independent data sharing.\n\
\nAuthors:\n\
  Michael Borland and Robert Soliday\n\
\nVersion:\n\
  Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

int main(int argc, char **argv) {
  SDDS_DATASET SDDSin, SDDSout;
  long i_arg, tmpfile_used;
  SCANNED_ARG *s_arg;
  char *input, *output, *description_text, *description_contents;
  unsigned long pipe_flags, major_order_flag;
  long page_number, non_native = 0;
  char *output_endianess = NULL;
  char buffer[40];
  short column_major_order = -1;

  // Register the program name for SDDS utilities
  SDDS_RegisterProgramName(argv[0]);

  // Parse command-line arguments
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) // Ensure at least one argument is provided
    bomb(NULL, USAGE);

  // Initialize variables for input/output files and options
  input = output = description_text = description_contents = NULL;
  pipe_flags = 0;

  // Process each command-line argument
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_"); // Normalize option string
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        major_order_flag = 0;
        s_arg[i_arg].n_items--;
        // Parse major order option
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&major_order_flag, s_arg[i_arg].list + 1,
                                                       &s_arg[i_arg].n_items, 0, "row", -1, NULL, 0,
                                                       SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0,
                                                       SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (major_order_flag & SDDS_COLUMN_MAJOR_ORDER)
          column_major_order = 1; // Set column major order
        else if (major_order_flag & SDDS_ROW_MAJOR_ORDER)
          column_major_order = 0; // Set row major order
        break;
      case SET_PIPE:
        // Parse pipe option
        if (!processPipeOption(s_arg[i_arg].list + 1,
                               s_arg[i_arg].n_items - 1, &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case NONNATIVE:
        non_native = 1; // Enable non-native byte order handling
        break;
      default:
        // Handle unknown options
        fprintf(stderr, "Error (%s): unknown switch: %s\n", argv[0],
                s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      // Assign input and output filenames
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  // Process filenames and handle temporary files
  processFilenames("sddsendian", &input, &output, pipe_flags, 0,
                   &tmpfile_used);

  // Check and clear output endianess environment variable
  output_endianess = getenv("SDDS_OUTPUT_ENDIANESS");

  if (output_endianess) {
    putenv("SDDS_OUTPUT_ENDIANESS=");
  }

  // Initialize the input SDDS dataset
  if (!SDDS_InitializeInput(&SDDSin, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  // Retrieve description text if available
  if (!description_text)
    SDDS_GetDescription(&SDDSin, &description_text, &description_contents);

  // Initialize the output SDDS dataset and set data mode
  if (!SDDS_InitializeCopy(&SDDSout, &SDDSin, output, "w") || !SDDS_SetDataMode(&SDDSout, non_native ? SDDS_BINARY : -SDDS_BINARY)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  // Set column/row major order based on parsed options
  if (column_major_order != -1)
    SDDSout.layout.data_mode.column_major = column_major_order;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;

  // Write the layout of the output file
  if (!SDDS_WriteLayout(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  // Process pages in the input file and write to the output file
  if (non_native) {
    while ((page_number = SDDS_ReadNonNativePage(&SDDSin)) >= 0) {
      if (!SDDS_CopyPage(&SDDSout, &SDDSin) || !SDDS_WritePage(&SDDSout)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
  } else {
    while ((page_number = SDDS_ReadPage(&SDDSin)) >= 0) {
      if (!SDDS_CopyPage(&SDDSout, &SDDSin) || !SDDS_WriteNonNativeBinaryPage(&SDDSout)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
  }

  // Terminate the input and output datasets
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  // Replace temporary files if necessary
  if (tmpfile_used && !replaceFileAndBackUp(input, output))
    exit(1);

  // Restore output endianess environment variable if it was set
  if (output_endianess) {
    sprintf(buffer, "SDDS_OUTPUT_ENDIANESS=%s", output_endianess);
    putenv(buffer);
  }

  return 0; // Return success
}
