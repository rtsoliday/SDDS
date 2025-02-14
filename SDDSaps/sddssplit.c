/**
 * @file sddssplit.c
 * @brief Splits an SDDS file into multiple files, each containing a single page.
 *
 * @details
 * This utility reads an SDDS (Self Describing Data Set) file and splits its contents 
 * into multiple output files, with each page stored in a separate file. The tool allows
 * customization of output filenames, page selection, output format, and more.
 *
 * @section Usage
 * ```
 * sddssplit <inputFile> 
 *          [-pipe[=input]]
 *          [-binary | -ascii]
 *          [-digits=<number>]
 *          [-rootname=<string>]
 *          [-firstPage=<number>]
 *          [-lastPage=<number>]
 *          [-interval=<number>]
 *          [-extension=<string>]
 *          [-groupParameter=<parameterName>]
 *          [-nameParameter=<filenameParameter>]
 *          [-offset=<integer>]
 *          [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Optional                          | Description                                                                            |
 * |-----------------------------------|----------------------------------------------------------------------------------------|
 * | `-pipe`                           | Use standard input instead of an input file.                                           |
 * | `-binary`                         | Specify output format (binary).                                                        |
 * | `-ascii`                          | Specify output format (ASCII).                                                         |
 * | `-digits`                         | Specify number of digits for output file indices.                                      |
 * | `-rootname`                       | Rootname for output filenames. Defaults to input file name.                            |
 * | `-firstPage`                      | First page to include in the output. Defaults to 1.                                    |
 * | `-lastPage`                       | Last page to include in the output. Defaults to the last page in the input file.       |
 * | `-interval`                       | Interval between included pages. Defaults to 1.                                        |
 * | `-extension`                      | Specify file extension for output files. Defaults to "sdds".                           |
 * | `-groupParameter`                 | Group pages into output files based on the specified parameter.                        |
 * | `-nameParameter`                  | Name output files based on the specified parameter.                                 |
 * | `-offset`                         | Offset for page numbering in output filenames.                                         |
 * | `-majorOrder`                     | Specify row- or column-major order for output.                                         |
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
typedef enum {
  SET_BINARY,
  SET_ASCII,
  SET_DIGITS,
  SET_ROOTNAME,
  SET_FIRST_PAGE,
  SET_LAST_PAGE,
  SET_INTERVAL,
  SET_EXTENSION,
  SET_PIPE,
  SET_NAMEPARAMETER,
  SET_OFFSET,
  SET_MAJOR_ORDER,
  SET_GROUPPARAMETER,
  N_OPTIONS
} option_type;

static char *option[N_OPTIONS] = {
  "binary",
  "ascii",
  "digits",
  "rootname",
  "firstpage",
  "lastpage",
  "interval",
  "extension",
  "pipe",
  "nameparameter",
  "offset",
  "majorOrder",
  "groupparameter"};

static char *USAGE =
  "sddssplit <inputFile> -pipe[=input]\n"
  "  [-binary | -ascii]\n"
  "  [-digits=<number>]\n"
  "  [-rootname=<string>]\n"
  "  [-firstPage=<number>]\n"
  "  [-lastPage=<number>]\n"
  "  [-interval=<number>]\n"
  "  [-extension=<string>]\n"
  "  [-groupParameter=<parameterName>]\n"
  "  [-nameParameter=<filenameParameter>]\n"
  "  [-offset=<integer>]\n"
  "  [-majorOrder=row|column]\n\n"

  "sddssplit splits an SDDS file into many SDDS files, with each page going to a separate file.\n"
  "The files are named <rootname><integer>.sdds, where <rootname> is either the filename for\n"
  "the source file or the specified string, and <integer> is by default <page-number>-<offset>\n"
  "and is printed to the number of digits given by -digits (3 is the default).\n\n"

  "-binary, -ascii       Specifies whether binary (default) or ASCII output is desired.\n"
  "-rootname             Rootname to use for output filenames. Defaults to the source filename.\n"
  "-digits               Number of digits to use in the filenames (3 is default).\n"
  "-firstPage            First page of input file to include in output (1 is default).\n"
  "-lastPage             Last page of input file to include in output (EOF is default).\n"
  "-interval             Interval between pages included in output (1 is default).\n"
  "-extension            Extension for output files (sdds is default).\n"
  "-groupParameter       Parameter of input file to use in grouping pages into output files.\n"
  "-nameParameter        Parameter of input file to use for naming the output files.\n"
  "-offset               Offset of page number to compute index for output filename.\n"
  "-majorOrder           Select row- or column-major order output (default is row).\n\n"

  "Program by Michael Borland. ("__DATE__
  " "__TIME__
  ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SDDS_DATASET sdds_dataset, sdds_orig;
  long i_arg, offset = 0;
  SCANNED_ARG *s_arg;
  char *input = NULL, *rootname = NULL, name[500], format[100], *extension = "sdds";
  long ascii_output = 0, binary_output = 0, retval, digits = 3;
  long first_page = 0, last_page = 0, interval = 0;
  unsigned long pipe_flags = 0, major_order_flag = 0;
  char *file_parameter = NULL, *name_from_parameter = NULL, *group_parameter_name = NULL;
  char *last_group_parameter = NULL, *this_group_parameter = NULL;
  short column_major_order = -1, file_active = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s", USAGE);
    return 1;
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        major_order_flag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            !scanItemList(&major_order_flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                          "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)) {
          fprintf(stderr, "Error: Invalid -majorOrder syntax/values\n");
          return 1;
        }
        column_major_order = (major_order_flag & SDDS_COLUMN_MAJOR_ORDER) ? 1 : 0;
        break;
      case SET_BINARY:
        binary_output = 1;
        ascii_output = 0;
        break;
      case SET_ASCII:
        ascii_output = 1;
        binary_output = 0;
        break;
      case SET_DIGITS:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &digits) != 1 || digits <= 0) {
          fprintf(stderr, "Error: Invalid -digits syntax\n");
          return 1;
        }
        break;
      case SET_ROOTNAME:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error: Invalid -rootname syntax\n");
          return 1;
        }
        rootname = s_arg[i_arg].list[1];
        break;
      case SET_FIRST_PAGE:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &first_page) != 1 || first_page <= 0) {
          fprintf(stderr, "Error: Invalid -firstPage syntax\n");
          return 1;
        }
        break;
      case SET_LAST_PAGE:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &last_page) != 1 || last_page <= 0) {
          fprintf(stderr, "Error: Invalid -lastPage syntax\n");
          return 1;
        }
        break;
      case SET_INTERVAL:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &interval) != 1 || interval <= 0) {
          fprintf(stderr, "Error: Invalid -interval syntax\n");
          return 1;
        }
        break;
      case SET_EXTENSION:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error: Invalid -extension syntax\n");
          return 1;
        }
        extension = s_arg[i_arg].list[1];
        break;
      case SET_OFFSET:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &offset) != 1) {
          fprintf(stderr, "Error: Invalid -offset syntax\n");
          return 1;
        }
        break;
      case SET_PIPE:
        pipe_flags = USE_STDIN;
        break;
      case SET_NAMEPARAMETER:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error: Invalid -nameParameter syntax\n");
          return 1;
        }
        file_parameter = s_arg[i_arg].list[1];
        break;
      case SET_GROUPPARAMETER:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error: Invalid -groupParameter syntax\n");
          return 1;
        }
        group_parameter_name = s_arg[i_arg].list[1];
        break;
      default:
        fprintf(stderr, "Error: Unknown switch: %s\n", s_arg[i_arg].list[0]);
        fprintf(stderr, "%s", USAGE);
        return 1;
      }
    } else {
      if (!input) {
        input = s_arg[i_arg].list[0];
      } else {
        fprintf(stderr, "Error: Too many filenames\n");
        return 1;
      }
    }
  }

  if (!input && !(pipe_flags & USE_STDIN)) {
    fprintf(stderr, "Error: Missing input filename\n");
    return 1;
  }

  if (pipe_flags & USE_STDIN && !file_parameter && !rootname) {
    fprintf(stderr, "Error: Provide -rootname or -nameParameter with -pipe\n");
    return 1;
  }

  if (!rootname && !file_parameter) {
    if ((rootname = strrchr(input, '.'))) {
      *rootname = 0;
      SDDS_CopyString(&rootname, input);
      input[strlen(input)] = '.';
    } else {
      SDDS_CopyString(&rootname, input);
    }
  }

  if (first_page && last_page && first_page > last_page) {
    fprintf(stderr, "Error: firstPage > lastPage\n");
    return 1;
  }

  if (!SDDS_InitializeInput(&sdds_orig, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }

  if (!extension || SDDS_StringIsBlank(extension)) {
    extension = NULL;
    snprintf(format, sizeof(format), "%%s%%0%ldld", digits);
  } else {
    snprintf(format, sizeof(format), "%%s%%0%ldld.%s", digits, extension);
  }

  if (file_parameter &&
      SDDS_CheckParameter(&sdds_orig, file_parameter, NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY) {
    fprintf(stderr, "Error: Filename parameter not present or wrong type\n");
    return 1;
  }

  last_group_parameter = NULL;
  while ((retval = SDDS_ReadPage(&sdds_orig)) > 0) {
    if (first_page && retval < first_page) {
      continue;
    }
    if (last_page && retval > last_page) {
      break;
    }
    if (interval) {
      if (first_page) {
        if ((retval - first_page) % interval != 0) {
          continue;
        }
      } else if ((retval - 1) % interval != 0) {
        continue;
      }
    }
    if (file_parameter) {
      if (!SDDS_GetParameter(&sdds_orig, file_parameter, &name_from_parameter)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
      }
      strncpy(name, name_from_parameter, sizeof(name) - 1);
      name[sizeof(name) - 1] = '\0';
      free(name_from_parameter);
    } else {
      snprintf(name, sizeof(name), format, rootname, retval - offset);
    }
    if (group_parameter_name) {
      if (!SDDS_GetParameterAsString(&sdds_orig, group_parameter_name, &this_group_parameter)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
      }
    }
    if (!group_parameter_name || !last_group_parameter ||
        (group_parameter_name && strcmp(this_group_parameter, last_group_parameter))) {
      if (file_active && !SDDS_Terminate(&sdds_dataset)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
      }
      file_active = 0;
      if (!SDDS_InitializeCopy(&sdds_dataset, &sdds_orig, name, "w")) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
      }
      if ((ascii_output && sdds_dataset.layout.data_mode.mode != SDDS_ASCII) ||
          (binary_output && sdds_dataset.layout.data_mode.mode != SDDS_BINARY)) {
        sdds_dataset.layout.data_mode.mode = ascii_output ? SDDS_ASCII : SDDS_BINARY;
      }
      sdds_dataset.layout.data_mode.column_major = (column_major_order != -1) ? column_major_order : sdds_orig.layout.data_mode.column_major;
      if (!SDDS_WriteLayout(&sdds_dataset)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
      }
      file_active = 1;
    }
    if (group_parameter_name) {
      free(last_group_parameter);
      last_group_parameter = this_group_parameter;
      this_group_parameter = NULL;
    }
    if (!SDDS_CopyPage(&sdds_dataset, &sdds_orig) || !SDDS_WritePage(&sdds_dataset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 1;
    }
  }
  if (retval == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  if (!SDDS_Terminate(&sdds_orig)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 1;
  }
  return 0;
}
