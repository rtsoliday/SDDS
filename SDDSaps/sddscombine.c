/**
 * @file sddscombine.c
 * @brief Combines multiple SDDS files into a single SDDS file with options for merging, appending, and modifying data.
 *
 * @details
 * This program processes multiple SDDS files, allowing users to merge data, append new pages, or perform transformations
 * such as deleting or retaining specific columns, parameters, or arrays. It supports various modes for handling sparse data,
 * recovery of corrupted data, and control over the order of the output dataset.
 *
 * @section Usage
 * ```
 * sddscombine [<SDDSinputfilelist>] [<SDDSoutputfile>]
 *             [-pipe=[input][,output]]
 *             [-delete={column|parameter|array},<matching-string>[,...]]
 *             [-retain={column|parameter|array},<matching-string>[,...]]
 *             [-sparse=<integer>[,{average|median|minimum|maximum}]]
 *             [-merge[=<parameter-name>|<npages>]]
 *             [-append]
 *             [-overWrite]
 *             [-collapse]
 *             [-recover[=clip]]
 *             [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Option                               | Description                                                                                     |
 * |--------------------------------------|-------------------------------------------------------------------------------------------------|
 * | `-pipe`                              | Enable piping for input and/or output.                                                         |
 * | `-delete`                            | Delete columns, parameters, or arrays matching the specified pattern.                           |
 * | `-retain`                            | Retain only columns, parameters, or arrays matching the specified pattern.                      |
 * | `-sparse`                            | Sample every nth row, optionally performing statistical analysis (`average`, `median`, etc.).    |
 * | `-merge`                             | Merge pages based on a parameter or number of pages.                                            |
 * | `-append`                            | Append data to the first input file.                                                           |
 * | `-overWrite`                         | Overwrite the output file if it exists.                                                        |
 * | `-collapse`                          | Collapse the output into a single page, as processed through `sddscollapse`.                   |
 * | `-recover`                           | Recover incomplete or corrupted data, optionally clipping incomplete pages.                    |
 * | `-majorOrder`                        | Specify row-major or column-major order for output.                                             |
 *
 * @subsection Incompatibilities
 *   - `-collapse` is incompatible with:
 *     - `-append`
 *   - Only one of the following may be specified:
 *     - `-delete`
 *     - `-retain`
 *   - For `-merge`:
 *     - `<parameter-name>` must exist in all input files if specified.
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"

/* Enumeration for option types */
enum option_type {
  SET_MERGE,
  SET_OVERWRITE,
  SET_PIPE,
  SET_DELETE,
  SET_RETAIN,
  SET_SPARSE,
  SET_COLLAPSE,
  SET_RECOVER,
  SET_MAJOR_ORDER,
  SET_APPEND,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "merge",
  "overwrite",
  "pipe",
  "delete",
  "retain",
  "sparse",
  "collapse",
  "recover",
  "majorOrder",
  "append"
};

char *USAGE =
  "\n"
  "  sddscombine [<SDDSinputfilelist>] [<SDDSoutputfile>]\n"
  "    [-pipe=[input][,output]]\n"
  "    [-delete={column|parameter|array},<matching-string>[,...]]\n"
  "    [-retain={column|parameter|array},<matching-string>[,...]]\n"
  "    [-sparse=<integer>[,{average|median|minimum|maximum}]]\n"
  "    [-merge[=<parameter-name>|<npages>]]\n"
  "    [-append]\n"
  "    [-overWrite]\n"
  "    [-collapse]\n"
  "    [-recover[=clip]]\n"
  "    [-majorOrder=row|column]\n\n"
  "Options:\n"
  "  -pipe=input,output      Enable piping for input and/or output.\n"
  "  -delete=type,pattern    Delete columns, parameters, or arrays matching the pattern.\n"
  "  -retain=type,pattern    Retain only columns, parameters, or arrays matching the pattern.\n"
  "  -sparse=<n>,mode        Sample every nth row with optional mode (average, median, minimum, maximum).\n"
  "  -merge=param|npages     Merge pages based on a parameter or number of pages.\n"
  "  -append                 Append data to the first input file.\n"
  "  -overWrite              Overwrite the output file if it exists.\n"
  "  -collapse               Collapse the output as if processed through sddscollapse.\n"
  "  -recover=clip           Recover incomplete/corrupted data, optionally clipping incomplete pages.\n"
  "  -majorOrder=row|column  Specify data write order: row-major or column-major.\n\n"
  "Description:\n"
  "  sddscombine combines data from a series of SDDS files into a single SDDS file, usually with one page for each page in each file. "
  "Data is added from files in the order that they are listed on the command line. A new parameter ('Filename') is added to show the source of each page.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

long SDDS_CompareParameterValues(void *param1, void *param2, long type);
long keep_element(char *name, char **delete, long deletions, char **retain, long retentions);
void *SDDS_GetParameterMod(SDDS_DATASET *SDDS_dataset, SDDS_DATASET *SDDS_output, char *parameter_name, void *memory);

#define COLUMN_MODE 0
#define PARAMETER_MODE 1
#define ARRAY_MODE 2
#define MODES 3
char *mode_name[MODES] = {
  "column",
  "parameter",
  "array",
};

#define SPARSE_AVERAGE 0
#define SPARSE_MEDIAN 1
#define SPARSE_MINIMUM 2
#define SPARSE_MAXIMUM 3
#define SPARSE_MODES 4
char *sparse_mode[SPARSE_MODES] = {
  "average",
  "median",
  "minimum",
  "maximum"
};

#define ROW_INCREMENT 100

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_input, SDDS_output;
  char **inputfile, *outputfile;
  long inputfiles, i, i_arg, retval = 0, first_page;
  long iFile, first_data, sparse, setPageNumber;
  int32_t sparse_statistics = 0;
  long merge, nMerge, overwrite, collapse, page, append;
  int64_t allocated_rows;
  int32_t columns;
  SCANNED_ARG *s_arg;
  long buffer[16];
  char *param, *last_param, *this_param, *text, *contents, **column;
  long param_index, param_type, param_size, output_pending;
  unsigned long pipeFlags, majorOrderFlag;

  char **retain_column, **delete_column;
  long retain_columns, delete_columns;
  char **retain_parameter, **delete_parameter;
  long retain_parameters, delete_parameters;
  char **retain_array, **delete_array;
  long retain_arrays, delete_arrays;
  long nColumns, recover, recovered;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  setPageNumber = allocated_rows = param_type = param_size = 0;
  last_param = this_param = NULL;
  column = NULL;
  inputfile = NULL;
  outputfile = param = NULL;
  inputfiles = merge = overwrite = collapse = append = nMerge = 0;
  pipeFlags = 0;
  sparse = 1;
  recover = 0;

  retain_column = delete_column = NULL;
  retain_columns = delete_columns = 0;
  retain_parameter = delete_parameter = NULL;
  retain_parameters = delete_parameters = 0;
  retain_array = delete_array = NULL;
  retain_arrays = delete_arrays = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_MERGE:
        if (s_arg[i_arg].n_items > 2)
          bomb("invalid -merge syntax", USAGE);
        merge = 1;
        param = NULL;
        nMerge = -1;
        if (s_arg[i_arg].n_items == 2) {
          if (isdigit(s_arg[i_arg].list[1][0])) {
            if (!sscanf(s_arg[i_arg].list[1], "%ld", &nMerge))
              bomb("invalid -merge syntax (could not scan number of pages)", USAGE);
          } else
            param = s_arg[i_arg].list[1];
        }
        break;
      case SET_APPEND:
        if (s_arg[i_arg].n_items > 1)
          bomb("invalid -append syntax", USAGE);
        append = 1;
        if (collapse) {
          SDDS_Bomb("-collapse and -append options cannot be used together");
          break;
        }
        break;
      case SET_OVERWRITE:
        overwrite = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_RECOVER:
        recover = 1;
        if (s_arg[i_arg].n_items != 1) {
          recover = 2;
          if (s_arg[i_arg].n_items > 2 || strncmp(s_arg[i_arg].list[1], "clip", strlen(s_arg[i_arg].list[1])) != 0)
            SDDS_Bomb("invalid -recover syntax");
        }
        break;
      case SET_DELETE:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -delete syntax");
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case COLUMN_MODE:
          delete_column = trealloc(delete_column, sizeof(*delete_column) * (delete_columns + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            delete_column[i - 2 + delete_columns] = expand_ranges(s_arg[i_arg].list[i]);
          delete_columns += s_arg[i_arg].n_items - 2;
          break;
        case PARAMETER_MODE:
          delete_parameter = trealloc(delete_parameter, sizeof(*delete_parameter) * (delete_parameters + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            delete_parameter[i - 2 + delete_parameters] = expand_ranges(s_arg[i_arg].list[i]);
          delete_parameters += s_arg[i_arg].n_items - 2;
          break;
        case ARRAY_MODE:
          delete_array = trealloc(delete_array, sizeof(*delete_array) * (delete_arrays + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            delete_array[i - 2 + delete_arrays] = expand_ranges(s_arg[i_arg].list[i]);
          delete_arrays += s_arg[i_arg].n_items - 2;
          break;
        default:
          SDDS_Bomb("invalid -delete syntax: specify column or parameter keyword");
          break;
        }
        break;
      case SET_RETAIN:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -retain syntax");
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case COLUMN_MODE:
          retain_column = trealloc(retain_column, sizeof(*retain_column) * (retain_columns + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            retain_column[i - 2 + retain_columns] = expand_ranges(s_arg[i_arg].list[i]);
          retain_columns += s_arg[i_arg].n_items - 2;
          break;
        case PARAMETER_MODE:
          retain_parameter = trealloc(retain_parameter, sizeof(*retain_parameter) * (retain_parameters + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            retain_parameter[i - 2 + retain_parameters] = expand_ranges(s_arg[i_arg].list[i]);
          retain_parameters += s_arg[i_arg].n_items - 2;
          break;
        case ARRAY_MODE:
          retain_array = trealloc(retain_array, sizeof(*retain_array) * (retain_arrays + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            retain_array[i - 2 + retain_arrays] = expand_ranges(s_arg[i_arg].list[i]);
          retain_arrays += s_arg[i_arg].n_items - 2;
          break;
        default:
          SDDS_Bomb("invalid -retain syntax: specify column or parameter keyword");
          break;
        }
        break;
      case SET_SPARSE:
        if ((s_arg[i_arg].n_items >= 2) && (s_arg[i_arg].n_items <= 3)) {
          if (sscanf(s_arg[i_arg].list[1], "%ld", &sparse) != 1) {
            bomb("invalid -sparse syntax", USAGE);
          }
          if (sparse <= 0) {
            bomb("invalid -sparse syntax", USAGE);
          }
          if (s_arg[i_arg].n_items == 3) {
            switch (match_string(s_arg[i_arg].list[2], sparse_mode, SPARSE_MODES, 0)) {
            case SPARSE_AVERAGE:
              sparse_statistics = 1;
              break;
            case SPARSE_MEDIAN:
              sparse_statistics = 2;
              break;
            case SPARSE_MINIMUM:
              sparse_statistics = 3;
              break;
            case SPARSE_MAXIMUM:
              sparse_statistics = 4;
              break;
            default:
              SDDS_Bomb("invalid -sparse syntax");
              break;
            }
          }
        } else {
          bomb("invalid -sparse syntax", USAGE);
        }
        break;
      case SET_COLLAPSE:
        collapse = 1;
        if (append) {
          SDDS_Bomb("-collapse and -append options cannot be used together");
          break;
        }
        break;
      default:
        bomb("unrecognized option", USAGE);
        break;
      }
    } else {
      inputfile = trealloc(inputfile, sizeof(*inputfile) * (inputfiles + 1));
      inputfile[inputfiles++] = s_arg[i_arg].list[0];
    }
  }

  outputfile = NULL;
  if (inputfiles > 1) {
    if (pipeFlags & USE_STDIN)
      SDDS_Bomb("too many input files with -pipe option");
    if (!(pipeFlags & USE_STDOUT)) {
      if (!append) {
        outputfile = inputfile[--inputfiles];
        if (fexists(outputfile) && !overwrite)
          SDDS_Bomb("output file exists already--give -overWrite option to force replacement");
      }
    }
  } else if (inputfiles == 1) {
    if (pipeFlags & USE_STDIN) {
      outputfile = inputfile[0];
      inputfile[0] = NULL;
    }
    if (pipeFlags & USE_STDOUT && outputfile)
      SDDS_Bomb("too many filenames given with -pipe=output");
  } else {
    if (!(pipeFlags & USE_STDIN) || !(pipeFlags & USE_STDOUT))
      SDDS_Bomb("too few filenames given");
    inputfiles = 1;
    inputfile = tmalloc(sizeof(*inputfile) * 1);
    inputfile[0] = outputfile = NULL;
  }

  for (i = 0; i < inputfiles; i++)
    if (inputfile[i] && outputfile && strcmp(inputfile[i], outputfile) == 0)
      SDDS_Bomb("Output file is also an input file.");

  if (append) {
    if (merge) {
      int64_t rowsPresent;
      if (!SDDS_InitializeAppendToPage(&SDDS_output, inputfile[0], 100, &rowsPresent))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else {
      if (!SDDS_InitializeAppend(&SDDS_output, inputfile[0]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    iFile = 1;
  } else {
    if (!SDDS_InitializeInput(&SDDS_input, inputfile[0]) ||
        !SDDS_GetDescription(&SDDS_input, &text, &contents) ||
        !SDDS_InitializeOutput(&SDDS_output, SDDS_BINARY, 0, text, contents, outputfile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (columnMajorOrder != -1)
      SDDS_output.layout.data_mode.column_major = columnMajorOrder;
    else
      SDDS_output.layout.data_mode.column_major = SDDS_input.layout.data_mode.column_major;
    iFile = 0;
  }

  for (; iFile < inputfiles; iFile++) {
    char **name;
    int32_t names;
    if (iFile && !SDDS_InitializeInput(&SDDS_input, inputfile[iFile]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!collapse) {
      if (!(name = SDDS_GetColumnNames(&SDDS_input, &names)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i = 0; i < names; i++) {
        if (append) {
          // We need all files to have the same columns for appending
          if (keep_element(name[i], delete_column, delete_columns, retain_column, retain_columns) &&
              (SDDS_GetColumnIndex(&SDDS_output, name[i]) < 0)) {
            fprintf(stderr, "Error (sddscombine): Problem appending data. Column %s does not exist in first page.\n", name[i]);
            exit(EXIT_FAILURE);
          }
        } else {
          if (keep_element(name[i], delete_column, delete_columns, retain_column, retain_columns) &&
              (SDDS_GetColumnIndex(&SDDS_output, name[i]) < 0 &&
               !SDDS_TransferColumnDefinition(&SDDS_output, &SDDS_input, name[i], name[i])))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        free(name[i]);
      }
      free(name);
    }

    if (!(name = SDDS_GetParameterNames(&SDDS_input, &names)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < names; i++) {
      if (collapse) {
        if (keep_element(name[i], delete_parameter, delete_parameters, retain_parameter, retain_parameters) &&
            (SDDS_GetColumnIndex(&SDDS_output, name[i]) < 0 &&
             !SDDS_DefineColumnLikeParameter(&SDDS_output, &SDDS_input, name[i], name[i])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (append) {
          if (keep_element(name[i], delete_parameter, delete_parameters, retain_parameter, retain_parameters) &&
              (SDDS_GetParameterIndex(&SDDS_output, name[i]) < 0)) {
            fprintf(stderr, "Error (sddscombine): Problem appending data. Parameter %s does not exist in first page.\n", name[i]);
            exit(EXIT_FAILURE);
          }
        } else {
          if (keep_element(name[i], delete_parameter, delete_parameters, retain_parameter, retain_parameters) &&
              (SDDS_GetParameterIndex(&SDDS_output, name[i]) < 0 &&
               !SDDS_TransferParameterDefinition(&SDDS_output, &SDDS_input, name[i], name[i])))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      free(name[i]);
    }
    free(name);

    if (!collapse) {
      if (!(name = SDDS_GetArrayNames(&SDDS_input, &names)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i = 0; i < names; i++) {
        if (append) {
          if (keep_element(name[i], delete_array, delete_arrays, retain_array, retain_arrays) &&
              (SDDS_GetArrayIndex(&SDDS_output, name[i]) < 0)) {
            fprintf(stderr, "Error (sddscombine): Problem appending data. Array %s does not exist in first page.\n", name[i]);
            exit(EXIT_FAILURE);
          }
        } else {
          if (keep_element(name[i], delete_array, delete_arrays, retain_array, retain_arrays) &&
              (SDDS_GetArrayIndex(&SDDS_output, name[i]) < 0 &&
               !SDDS_TransferArrayDefinition(&SDDS_output, &SDDS_input, name[i], name[i])))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        free(name[i]);
      }
      free(name);
    }
    if (inputfiles > 1 && !SDDS_Terminate(&SDDS_input))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (collapse) {
    if (!(column = SDDS_GetColumnNames(&SDDS_output, &columns))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  if (collapse) {
    if (!merge) {
      if (SDDS_GetColumnIndex(&SDDS_output, "Filename") < 0 &&
          SDDS_DefineColumn(&SDDS_output, "Filename", NULL, NULL, "Name of file from which this page came", NULL, SDDS_STRING, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (SDDS_GetColumnIndex(&SDDS_output, "NumberCombined") < 0 &&
        SDDS_DefineColumn(&SDDS_output, "NumberCombined", NULL, NULL, "Number of files combined to make this file", NULL, SDDS_LONG, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!append) {
      if (!SDDS_DeleteParameterFixedValues(&SDDS_output)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      if (!merge) {
        if (SDDS_GetParameterIndex(&SDDS_output, "Filename") < 0 &&
            SDDS_DefineParameter(&SDDS_output, "Filename", NULL, NULL, "Name of file from which this page came", NULL, SDDS_STRING, NULL) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (SDDS_GetParameterIndex(&SDDS_output, "NumberCombined") < 0 &&
          SDDS_DefineParameter(&SDDS_output, "NumberCombined", NULL, NULL, "Number of files combined to make this file", NULL, SDDS_LONG, NULL) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (collapse) {
    if (SDDS_GetColumnIndex(&SDDS_output, "PageNumber") < 0) {
      if (SDDS_DefineColumn(&SDDS_output, "PageNumber", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      setPageNumber = 1;
    } else {
      setPageNumber = 0;
    }
  }
  if (!append) {
    if (!SDDS_WriteLayout(&SDDS_output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
  if (collapse) {
    if (!SDDS_StartPage(&SDDS_output, allocated_rows = ROW_INCREMENT)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
  nColumns = SDDS_ColumnCount(&SDDS_output);
  if (append) {
    iFile = 1;
    first_data = 0;
    output_pending = 1;
  } else {
    iFile = 0;
    first_data = 1; /* indicates no pages copied so far */
    output_pending = 0;
  }

  page = 0;
  for (; iFile < inputfiles; iFile++) {
    if (inputfiles > 1 && !SDDS_InitializeInput(&SDDS_input, inputfile[iFile])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    first_page = 1;
    recovered = 0;
    while (!recovered && (retval = SDDS_ReadPageSparse(&SDDS_input, 0, nColumns ? sparse : INT64_MAX - 1, 0, sparse_statistics)) >= 0) {
      page++;
      if (retval == 0) {
        if (!recover)
          break;
        recovered = 1;
        if (recover == 2 || !SDDS_ReadRecoveryPossible(&SDDS_input))
          /* user doesn't want this page, or it can't be recovered */
          break;
      }
      if (param) {
        if (first_page) {
          if ((param_index = SDDS_GetParameterIndex(&SDDS_input, param)) < 0)
            SDDS_Bomb("-merge parameter not in input file(s)");
          if (param_type) {
            if (param_type != SDDS_GetParameterType(&SDDS_input, param_index))
              SDDS_Bomb("-merge parameter changes type in subsequent files");
          } else {
            param_size = SDDS_GetTypeSize(param_type = SDDS_GetParameterType(&SDDS_input, param_index));
            this_param = tmalloc(param_size);
            last_param = tmalloc(param_size);
            if (!SDDS_GetParameter(&SDDS_input, param, last_param))
              SDDS_Bomb("error getting value for -merge parameter");
          }
        } else {
          memcpy(last_param, this_param, param_size);
        }
        if (!SDDS_GetParameter(&SDDS_input, param, this_param))
          SDDS_Bomb("error getting value for -merge parameter");
      }
#ifdef DEBUG
      if (param) {
        fprintf(stderr, "parameter %s = ", param);
        SDDS_PrintTypedValue(this_param, 0, param_type, NULL, stderr, 0);
        fprintf(stderr, " now (was ");
        SDDS_PrintTypedValue(last_param, 0, param_type, NULL, stderr, 0);
        fprintf(stderr, ")\n");
      }
#endif
      if (collapse) {
        if (merge && param) {
          if (SDDS_CompareParameterValues(this_param, last_param, param_type) != 0 && output_pending) {
            output_pending = 0;
          }
        }
        if (!merge || (!param && first_data && first_page) || (param && !output_pending)) {
          if (page > allocated_rows) {
            if (!SDDS_LengthenTable(&SDDS_output, ROW_INCREMENT)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
            allocated_rows += ROW_INCREMENT;
          }
          for (i = 0; i < columns; i++) {
            if (!SDDS_GetParameterMod(&SDDS_input, &SDDS_output, column[i], buffer)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
            if (!SDDS_SetRowValues(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, page - 1, column[i], buffer, NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          }
          if (!merge) {
            if (!SDDS_SetRowValues(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, page - 1, "Filename", inputfile[iFile] ? inputfile[iFile] : "stdin", "NumberCombined", inputfiles, NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          } else {
            if (!SDDS_SetRowValues(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, page - 1, "NumberCombined", inputfiles, NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          }
          if (setPageNumber && !SDDS_SetRowValues(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, page - 1, "PageNumber", page, NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          first_data = 0;
        } else if (merge && param && output_pending) {
          page--;
        }
      } else {
        if (!merge) {
          if (!SDDS_ClearPage(&SDDS_output) ||
              !SDDS_CopyPage(&SDDS_output, &SDDS_input)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          if (SDDS_GetParameterIndex(&SDDS_output, "Filename") >= 0) {
            if (!SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "Filename", inputfile[iFile] ? inputfile[iFile] : "stdin", NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          }
          if (SDDS_GetParameterIndex(&SDDS_output, "NumberCombined") >= 0) {
            if (!SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "NumberCombined", inputfiles, NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          }
          if (!SDDS_WritePage(&SDDS_output)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        } else if (merge && !param) {
          if (nMerge > 0 && (page - 1) % nMerge == 0 && page != 1) {
            if (!SDDS_WritePage(&SDDS_output)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
            output_pending = 0;
          }
          if ((first_data && first_page) || (nMerge > 0 && (page - 1) % nMerge == 0)) {
            if (!SDDS_CopyPage(&SDDS_output, &SDDS_input)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
            first_data = 0;
          } else {
            if (!SDDS_CopyAdditionalRows(&SDDS_output, &SDDS_input)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          }
        } else {
#ifdef DEBUG
          if (SDDS_CompareParameterValues(this_param, last_param, param_type) != 0)
            fprintf(stderr, "Parameter value has changed\n");
#endif
          if (SDDS_CompareParameterValues(this_param, last_param, param_type) != 0 && output_pending) {
            if (SDDS_GetParameterIndex(&SDDS_output, "NumberCombined") >= 0) {
              if (!SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "NumberCombined", inputfiles, NULL)) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                exit(EXIT_FAILURE);
              }
            }
            if (!SDDS_WritePage(&SDDS_output)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
            output_pending = 0;
          }
          if (!output_pending) {
            if (!SDDS_CopyPage(&SDDS_output, &SDDS_input)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          } else {
            if (!SDDS_CopyAdditionalRows(&SDDS_output, &SDDS_input)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          }
        }
      }
      if (merge) {
        output_pending = 1;
      }
      first_page = 0;
    }
    if (!recovered && (retval == 0 || SDDS_NumberOfErrors() || !SDDS_Terminate(&SDDS_input))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
  if (!collapse && merge && output_pending) {
    if (SDDS_GetParameterIndex(&SDDS_output, "NumberCombined") >= 0) {
      if (!SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "NumberCombined", inputfiles, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
    if (append) {
      if (!SDDS_UpdatePage(&SDDS_output, FLUSH_TABLE)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    } else {
      if (!SDDS_WritePage(&SDDS_output)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }
  if (collapse) {
    if (!SDDS_WritePage(&SDDS_output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (page == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
  if (!SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}

void *SDDS_GetParameterMod(SDDS_DATASET *SDDS_dataset, SDDS_DATASET *SDDS_output, char *parameter_name, void *memory) {
  long index, type, size;
  void *data;
  float floatdata;
  double doubledata;
  uint64_t ulong64data;
  int64_t long64data;
  uint32_t ulongdata;
  int32_t longdata;
  unsigned short ushortdata;
  short shortdata;
  char chardata;
  char *stringdata;
  floatdata = 0.0;
  doubledata = 0.0;
  ulong64data = 0;
  long64data = 0;
  ulongdata = 0;
  longdata = 0;
  ushortdata = 0;
  shortdata = 0;
  chardata = '\000';
  stringdata = "";

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetParameterMod"))
    return (NULL);
  if (!parameter_name) {
    SDDS_SetError("Unable to get parameter value--parameter name pointer is NULL (SDDS_GetParameterMod)");
    return (NULL);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_dataset, parameter_name)) < 0) {
    if ((index = SDDS_GetColumnIndex(SDDS_output, parameter_name)) < 0) {
      SDDS_SetError("Unable to get parameter value--parameter name is unrecognized (SDDS_GetParameterMod)");
      return (NULL);
    }
    if (!(type = SDDS_GetColumnType(SDDS_output, index))) {
      SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameterMod)");
      return (NULL);
    }
    size = SDDS_type_size[type - 1];
    if (memory)
      data = memory;
    else if (!(data = SDDS_Malloc(size))) {
      SDDS_SetError("Unable to get parameter value--parameter data size is invalid (SDDS_GetParameterMod)");
      return (NULL);
    }
    switch (type) {
    case SDDS_FLOAT:
      data = memcpy(data, &floatdata, size);
      break;
    case SDDS_DOUBLE:
      data = memcpy(data, &doubledata, size);
      break;
    case SDDS_ULONG64:
      data = memcpy(data, &ulong64data, size);
      break;
    case SDDS_LONG64:
      data = memcpy(data, &long64data, size);
      break;
    case SDDS_ULONG:
      data = memcpy(data, &ulongdata, size);
      break;
    case SDDS_LONG:
      data = memcpy(data, &longdata, size);
      break;
    case SDDS_USHORT:
      data = memcpy(data, &ushortdata, size);
      break;
    case SDDS_SHORT:
      data = memcpy(data, &shortdata, size);
      break;
    case SDDS_CHARACTER:
      data = memcpy(data, &chardata, size);
      break;
    case SDDS_STRING:
      if (!SDDS_CopyString((char **)data, stringdata))
        break;
    }
  } else {
    if (!(type = SDDS_GetParameterType(SDDS_dataset, index))) {
      SDDS_SetError("Unable to get parameter value--parameter data type is invalid (SDDS_GetParameterMod)");
      return (NULL);
    }
    if (!SDDS_dataset->parameter || !SDDS_dataset->parameter[index]) {
      SDDS_SetError("Unable to get parameter value--parameter data array is NULL (SDDS_GetParameterMod)");
      return (NULL);
    }
    size = SDDS_type_size[type - 1];
    if (memory)
      data = memory;
    else if (!(data = SDDS_Malloc(size))) {
      SDDS_SetError("Unable to get parameter value--parameter data size is invalid (SDDS_GetParameterMod)");
      return (NULL);
    }
    if (type != SDDS_STRING)
      memcpy(data, SDDS_dataset->parameter[index], size);
    else if (!SDDS_CopyString((char **)data, *(char **)SDDS_dataset->parameter[index]))
      return (NULL);
  }
  return (data);
}

long SDDS_CompareParameterValues(void *param1, void *param2, long type) {
  double ddiff;
  int64_t ldiff;
  char cdiff;

  switch (type) {
  case SDDS_FLOAT:
    ddiff = *((float *)param1) - *((float *)param2);
    return ddiff < 0 ? -1 : ddiff > 0 ? 1 : 0;
  case SDDS_DOUBLE:
    ddiff = *((double *)param1) - *((double *)param2);
    return ddiff < 0 ? -1 : ddiff > 0 ? 1 : 0;
  case SDDS_LONG64:
    ldiff = *((int64_t *)param1) - *((int64_t *)param2);
    return ldiff < 0 ? -1 : ldiff > 0 ? 1 : 0;
  case SDDS_ULONG64:
    ldiff = *((uint64_t *)param1) - *((uint64_t *)param2);
    return ldiff < 0 ? -1 : ldiff > 0 ? 1 : 0;
  case SDDS_LONG:
    ldiff = *((int32_t *)param1) - *((int32_t *)param2);
    return ldiff < 0 ? -1 : ldiff > 0 ? 1 : 0;
  case SDDS_ULONG:
    ldiff = *((uint32_t *)param1) - *((uint32_t *)param2);
    return ldiff < 0 ? -1 : ldiff > 0 ? 1 : 0;
  case SDDS_SHORT:
    ldiff = *((short *)param1) - *((short *)param2);
    return ldiff < 0 ? -1 : ldiff > 0 ? 1 : 0;
  case SDDS_USHORT:
    ldiff = *((unsigned short *)param1) - *((unsigned short *)param2);
    return ldiff < 0 ? -1 : ldiff > 0 ? 1 : 0;
  case SDDS_CHARACTER:
    cdiff = (short)*((char *)param1) - (short)*((char *)param2);
    return cdiff < 0 ? -1 : cdiff > 0 ? 1 : 0;
  case SDDS_STRING:
    return strcmp(*(char **)param1, *(char **)param2);
  default:
    SDDS_SetError("Problem doing data comparison--invalid data type (SDDS_CompareParameterValues)");
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
}

long keep_element(char *name, char **delete, long deletions, char **retain, long retentions) {
  long i, flag;

  flag = 1;

  if (deletions) {
    for (i = 0; i < deletions; i++) {
      if (wild_match(name, delete[i])) {
        flag = 0;
        break;
      }
    }
  }

  if (retentions) {
    if (!deletions)
      flag = 0;
    for (i = 0; i < retentions; i++) {
      if (wild_match(name, retain[i])) {
        flag = 1;
        break;
      }
    }
  }

  return flag;
}
