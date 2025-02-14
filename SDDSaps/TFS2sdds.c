/**
 * @file TFS2sdds.c
 * @brief Converts MAD (Methodical Accelerator Design) TFS (Twiss File Standard) data files to SDDS format.
 *
 * @details
 * This program reads LEP TFS (Twiss File Standard) format files (used by MAD) and converts them to
 * the SDDS (Self Describing Data Sets) format. It supports piping input and output
 * through standard input and output streams. The program processes header and data
 * lines in the input file to generate corresponding SDDS parameters, columns, and data.
 *
 * @section Usage
 * ```
 * TFS2sdds [<inputfile>] [<outputfile>]
 *          [-pipe[=input][,output]]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Enables piping mode. Allows input and/or output through streams.                      |
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
#include "scan.h"
#include "SDDS.h"
#include "match_string.h"

/* Enumeration for option types */
enum option_type {
  SET_PIPE,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "pipe",
};

static const char *USAGE =
  "Usage: TFS2sdds [<inputfile> <outputfile>] [-pipe[=input][,output]]\n"
  "\n"
  "Converts LEP TFS format files (used by MAD) to SDDS.\n"
  "\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ").";

#define SHORT_TYPE 0
#define LONG_TYPE 1
#define DOUBLE_TYPE 2
#define FLOAT_TYPE 3
#define STRING_TYPE 4
#define TYPENAMES 5

static char *typeName[TYPENAMES] = {
  "short",
  "long",
  "double",
  "float",
  "string",
};

#define SDDS_MAXLINE 1024

long identifyType(char *format);

int main(int argc, char **argv) {
  SCANNED_ARG *scanned;
  long i_arg, type, inHeader = 1;
  char *input, *output;
  unsigned long pipeFlags;
  FILE *fpi, *fpo;
  char s1[SDDS_MAXLINE], s2[SDDS_MAXLINE];
  char *name, *format, *value;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s\n", USAGE);
    exit(EXIT_FAILURE);
  }

  input = output = format = value = NULL;
  pipeFlags = 0;
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "Error: Invalid -pipe syntax.\n");
          exit(EXIT_FAILURE);
        }
        break;
      default:
        fprintf(stderr, "Error: Unknown option '%s'.\n", scanned[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (!input) {
        input = scanned[i_arg].list[0];
      } else if (!output) {
        output = scanned[i_arg].list[0];
      } else {
        fprintf(stderr, "Error: Too many filenames provided.\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  processFilenames("TFS2sdds", &input, &output, pipeFlags, 0, NULL);

  if (!input)
    fpi = stdin;
  else if (!(fpi = fopen(input, "r"))) {
    fprintf(stderr, "Error: Unable to open input file '%s'.\n", input);
    exit(EXIT_FAILURE);
  }

  if (!output)
    fpo = stdout;
  else if (!(fpo = fopen(output, "w"))) {
    fprintf(stderr, "Error: Unable to open output file '%s'.\n", output);
    exit(EXIT_FAILURE);
  }

  fprintf(fpo, "SDDS1\n");
  if (!fgets(s1, SDDS_MAXLINE, fpi)) {
    SDDS_Bomb("Input file ends prematurely");
  }

  while (s1[0] == '@') {
    strcpy_ss(s1, s1 + 1);
    if (!(name = get_token(s1)) || !(format = get_token(s1)) || !(value = get_token(s1))) {
      SDDS_Bomb("Missing data for parameter");
    }
    if ((type = identifyType(format)) < 0) {
      fprintf(stderr, "Error (TFS2sdds): Unknown format string: %s\n", format);
      exit(EXIT_FAILURE);
    }
    fprintf(fpo, "&parameter name=%s, type=%s, fixed_value=\"%s\" &end\n", name, typeName[type], value);

    if (!fgets(s1, SDDS_MAXLINE, fpi)) {
      SDDS_Bomb("Input file ends prematurely");
    }
    inHeader = 0;
  }

  if (!fgets(s2, SDDS_MAXLINE, fpi)) {
    SDDS_Bomb("Input file ends prematurely");
  }

  if (s1[0] != '*') {
    SDDS_Bomb("Column name line not seen");
  }
  if (s2[0] != '$') {
    SDDS_Bomb("Column format line not seen");
  }
  strcpy_ss(s1, s1 + 1);
  strcpy_ss(s2, s2 + 1);
  while ((name = get_token(s1))) {
    if (!(format = get_token(s2))) {
      SDDS_Bomb("Missing format for column");
    }
    fprintf(fpo, "&column name=%s, type=", name);
    if ((type = identifyType(format)) < 0) {
      fprintf(stderr, "Error (TFS2sdds): Unknown format string: %s\n", format);
      exit(EXIT_FAILURE);
    }
    fprintf(fpo, "%s &end\n", typeName[type]);
  }

  if (inHeader == 0) {
    fputs("&data mode=ascii, no_row_counts=1 &end\n", fpo);
  }

  while (fgets(s1, SDDS_MAXLINE, fpi)) {
    if (inHeader) {
      if (s1[0] == '@') {
        strcpy_ss(s1, s1 + 1);
        if (!(name = get_token(s1)) || !(format = get_token(s1)) || !(value = get_token(s1))) {
          SDDS_Bomb("Missing data for parameter");
        }
        if ((type = identifyType(format)) < 0) {
          fprintf(stderr, "Error (TFS2sdds): Unknown format string: %s\n", format);
          exit(EXIT_FAILURE);
        }
        fprintf(fpo, "&parameter name=%s, type=%s, fixed_value=\"%s\" &end\n", name, typeName[type], value);
        continue;
      }
      inHeader = 0;
      fputs("&data mode=ascii, no_row_counts=1 &end\n", fpo);
    }
    fputs(s1, fpo);
  }
  if (inHeader) {
    fputs("&data mode=ascii, no_row_counts=1 &end\n\n", fpo);
  }

  return EXIT_SUCCESS;
}

long identifyType(char *format) {
  long length;

  if (!format) {
    SDDS_Bomb("Bad format string seen");
  }

  length = strlen(format);
  if (format[0] != '%') {
    SDDS_Bomb("Bad format string seen");
  }

  if (length >= 2) {
    if (strcmp(format + length - 2, "le") == 0 || strcmp(format + length - 2, "lf") == 0) {
      return DOUBLE_TYPE;
    }
    if (strcmp(format + length - 2, "ld") == 0) {
      return LONG_TYPE;
    }
    if (strcmp(format + length - 2, "hd") == 0) {
      return SHORT_TYPE;
    }
  }

  if (length >= 1) {
    if (format[length - 1] == 'e' || format[length - 1] == 'f') {
      return FLOAT_TYPE;
    }
    if (format[length - 1] == 'd') {
      return LONG_TYPE;
    }
    if (format[length - 1] == 's') {
      return STRING_TYPE;
    }
  }

  return -1;
}
