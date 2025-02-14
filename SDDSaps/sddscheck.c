/**
 * @file sddscheck.c
 * @brief Validates and checks an SDDS file for corruption or issues.
 *
 * @details
 * This program reads an SDDS (Self Describing Data Set) file and determines its validity. 
 * It processes the file by verifying its structure, pages, and data, and outputs the status:
 * - `"ok"` if the file is valid.
 * - `"nonexistent"` if the file does not exist.
 * - `"badHeader"` if the file has an invalid header.
 * - `"corrupted"` if the file contains errors.
 *
 * @section Usage
 * ```
 * sddscheck <filename> [-printErrors]
 * ```
 *
 * @section Options
 * | Option                                | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-printErrors`                        | Outputs detailed error messages to stderr.                                           |
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
 * M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

typedef enum {
  CLO_PRINTERRORS = 0
} OptionType;

#define N_OPTIONS 1

static char *option[N_OPTIONS] = {
  "printErrors"
};

char *usage =
  "sddscheck <filename> [-printErrors]\n\n"
  "This program allows you to determine whether an SDDS file has been\n"
  "corrupted. It reads the entire file and prints a message to stdout.\n"
  "\n"
  "If the file is ok, \"ok\" is printed.\n"
  "If the file has a problem, one of the following will be printed:\n"
  "  - \"nonexistent\": The file does not exist.\n"
  "  - \"badHeader\": The file header is invalid.\n"
  "  - \"corrupted\": The file contains errors.\n"
  "\n"
  "Options:\n"
  "  -printErrors: Deliver error messages to stderr.\n"
  "\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_input;
  char *input;
  long i_arg, retval, print_errors;
  SCANNED_ARG *s_arg;

  /* Register the program name for error reporting. */
  SDDS_RegisterProgramName(argv[0]);

  /* Parse command-line arguments. */
  argc = scanargs(&s_arg, argc, argv);
  if (!s_arg || argc < 2) {
    bomb(NULL, usage); /* Display usage and exit if arguments are insufficient. */
  }

  input = NULL;
  print_errors = 0;

  /* Process each command-line argument. */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      /* Match recognized options. */
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_PRINTERRORS:
        print_errors = 1;
        break;
      default:
        SDDS_Bomb("unknown option given"); /* Handle unrecognized options. */
        break;
      }
    } else {
      /* Assign the first non-option argument as the input file name. */
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames"); /* Ensure only one input file is specified. */
    }
  }

  /* Check if the input file exists. */
  if (!fexists(input)) {
    puts("nonexistent"); /* Indicate file does not exist. */
    exit(0);
  }

  /* Initialize the SDDS input file. */
  if (!SDDS_InitializeInput(&SDDS_input, input)) {
    puts("badHeader"); /* Indicate the file header is invalid. */
    if (print_errors)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors); /* Print detailed errors if enabled. */
    exit(0);
  }

  /* Read and process each page of the SDDS file. */
  while ((retval = SDDS_ReadPage(&SDDS_input)) > 0) {
      /* Loop continues until EOF or an error occurs. */
  }

  if (retval == -1) {
    /* EOF reached  successfully. */
    puts("ok");
  } else {
    /* Handle file corruption or errors during processing. */
    if (print_errors)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    puts("corrupted");
  }

  return (0); /* Exit successfully. */
}
