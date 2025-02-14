/**
 * @file sddsxref.c
 * @brief Cross-references and merges SDDS data sets based on column matching and filtering.
 *
 * @details
 * This program merges data from multiple SDDS (Self Describing Data Set) files into a single output file.
 * It performs cross-referencing based on column matching, and supports selective transfer of columns, 
 * parameters, and arrays. The program also includes options for renaming and editing names to customize 
 * the output dataset.
 *
 * @section Usage
 * ```
 * sddsxref [<input1>] <input2> [<input3>...] [<output>]
 *          [-pipe[=input][,output]]
 *          [-ifis={column|parameter|array},<name>[,...]]
 *          [-ifnot={parameter|column|array},<name>[,...]]
 *          [-transfer={parameter|array},<name>[,...]]
 *          [-take=<column-name>[,...]]
 *          [-leave=<column-name>[,...]]
 *          [-replace=column|parameter|array,<name list>]
 *          [-fillIn]
 *          [-reuse[=[rows][,page]]]
 *          [-match=<column-name>[=<column-name>]]
 *          [-wildMatch=<column-name>[=<column-name>]]
 *          [-rename={column|parameter|array},<oldname>=<newname>[,...]]
 *          [-editnames={column|parameter|array},<wildcard-string>,<edit-string>]
 *          [-equate=<column-name>[=<column-name>]]
 *          [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `<input2>`                            | The second input file for cross-referencing.                                          |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard input/output streams.                                                   |
 * | `-ifis`                               | Filters data that must exist in the input files.                                    |
 * | `-ifnot`                              | Filters data that must not exist in the input files.                               |
 * | `-transfer`                           | Specifies parameters or arrays to transfer from reference files into the output.      |
 * | `-take`                               | Specifies columns to be taken from reference files and added to the output dataset.   |
 * | `-leave`                              | Specifies columns not to be taken from reference files. Overrides the `-take` option for these columns. |
 * | `-replace`                            | Replaces data in the output based on the specified types and names.                 |
 * | `-fillIn`                             | Fills in NULL or zero values for unmatched rows in the output.                        |
 * | `-reuse`                              | Allows reusing rows or pages from the reference file during cross-referencing.        |
 * | `-match`                              | Matches columns between input files for data integration.                             |
 * | `-wildMatch`                          | Matches columns using wildcard patterns.                                           |
 * | `-rename`                             | Renames specified columns, parameters, or arrays in the output.                |
 * | `-editnames`                          | Edits names of specified entities.                   |
 * | `-equate`                             | Matches columns based on equality conditions.                                         |
 * | `-majorOrder`                         | Specifies the major order of data in the output. Defaults to the input's order.       |
 *
 * @subsection Incompatibilities
 *   - `-equate` is incompatible with:
 *     - `-match`
 *   - Only one of the following may be specified:
 *     - `-wildMatch`
 *     - `-match`
 *   - For `-reuse`:
 *     - Requires specifying rows, pages, or both.
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
 * M. Borland, C. Saunders, R. Soliday, L. Emery, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "SDDSaps.h"
#include "scan.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_TAKE_COLUMNS,
  SET_LEAVE_COLUMNS,
  SET_MATCH_COLUMN,
  SET_EQUATE_COLUMN,
  SET_TRANSFER,
  SET_REUSE,
  SET_IFNOT,
  SET_NOWARNINGS,
  SET_IFIS,
  SET_PIPE,
  SET_FILLIN,
  SET_RENAME,
  SET_EDIT_NAMES,
  SET_WILD_MATCH,
  SET_MAJOR_ORDER,
  SET_REPLACE,
  N_OPTIONS
};

#define COLUMN_MODE 0
#define PARAMETER_MODE 1
#define ARRAY_MODE 2
#define MODES 3
static char *mode_name[MODES] = {
  "column",
  "parameter",
  "array",
};

#define COLUMN_REPLACE 0
#define PARAMETER_REPLACE 1
#define ARRAY_REPLACE 2
#define REPLACE_TYPES 3
static char *replace_type[REPLACE_TYPES] = {
  "column", "parameter", "array"};

#define PARAMETER_TRANSFER 0
#define ARRAY_TRANSFER 1
#define TRANSFER_TYPES 2
static char *transfer_type[TRANSFER_TYPES] = {
  "parameter", "array"};

typedef struct
{
  char *name;
  long type;
} TRANSFER_DEFINITION;

typedef struct
{
  char **new_column;
  char **new_parameter;
  char **new_array;
  char **orig_column;
  char **orig_parameter;
  char **orig_array;
  int32_t columns;
  int32_t parameters;
  int32_t arrays;
} REFDATA;

/* Structure for getting edit names */
typedef struct
{
  char *match_string;
  char *edit_string;
} EDIT_NAME_REQUEST;

long expandTransferRequests(char ***match, int32_t *matches, long type, TRANSFER_DEFINITION *transfer,
                            long transfers, SDDS_DATASET *inSet);

void add_newnames(SDDS_DATASET *SDDS_dataset, REFDATA *new_data, REFDATA rename_data,
                  EDIT_NAME_REQUEST *edit_column_request, long edit_column_requests,
                  EDIT_NAME_REQUEST *edit_parameter_request, long edit_parameter_requests,
                  EDIT_NAME_REQUEST *edit_array_request, long edit_array_requests, long filenumber);

char **process_editnames(char **orig_name, long **orig_flags, long orig_names, EDIT_NAME_REQUEST *edit_request,
                         long edit_requests, long filenumber);
long CopyRowToNewColumn(SDDS_DATASET *target, int64_t target_row, SDDS_DATASET *source, int64_t source_row,
                        REFDATA new_data, long columns, char *input2);
long CopyParametersFromSecondInput(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, REFDATA new_data);
long CopyArraysFromSecondInput(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, REFDATA new_data);

typedef char *STRING_PAIR[2];

char *option[N_OPTIONS] = {
  "take", "leave", "match", "equate", "transfer", "reuse", "ifnot",
  "nowarnings", "ifis", "pipe", "fillin", "rename", "editnames", "wildmatch", "majorOrder", "replace"};

char *USAGE =
  "Usage:\n"
  "  sddsxref [<input1>] <input2> [<input3>...] [<output>]\n\n"
  "Options:\n"
  "  -pipe[=input][,output]\n"
  "      Enable piping. Optionally specify input and/or output streams.\n"
  "  -ifis={column|parameter|array},<name>[,...]\n"
  "      Specify names of parameters, arrays, or columns that must exist in <input1>.\n"
  "  -ifnot={parameter|column|array},<name>[,...]\n"
  "      Specify names of parameters, arrays, or columns that must not exist in <input1>.\n"
  "  -transfer={parameter|array},<name>[,...]\n"
  "      Specify parameters or arrays to transfer from <input2>.\n"
  "  -take=<column-name>[,...]\n"
  "      Specify columns to take from <input2>.\n"
  "  -leave=<column-name>[,...]\n"
  "      Specify columns not to take from <input2>. Overrides -take for specified columns.\n"
  "      Use -leave=* to exclude all columns.\n"
  "  -replace=column|parameter|array,<name list>\n"
  "      Replace specified columns, parameters, or arrays in <input1> with those from subsequent input files.\n"
  "  -fillIn\n"
  "      Fill in NULL and 0 values in rows where no match is found. By default, such rows are omitted.\n"
  "  -reuse[=[rows][,page]]\n"
  "      Allow reuse of rows from <input2>. Use -reuse=page to restrict to the first page of <input2>.\n"
  "  -match=<column-name>[=<column-name>]\n"
  "      Specify columns to match between <input1> and <input2> for data selection and placement.\n"
  "  -wildMatch=<column-name>[=<column-name>]\n"
  "      Similar to -match, but allows wildcards in the matching data from <input2>.\n"
  "  -rename={column|parameter|array},<oldname>=<newname>[,...]\n"
  "      Rename specified columns, parameters, or arrays in the output data set.\n"
  "  -editnames={column|parameter|array},<wildcard-string>,<edit-string>\n"
  "      Edit names of specified entities using wildcard patterns and edit commands.\n"
  "  -equate=<column-name>[=<column-name>]\n"
  "      Equate columns between <input1> and <input2> for data matching based on equality.\n"
  "  -majorOrder=row|column\n"
  "      Specify the major order of data in the output (row or column). Defaults to the order of <input1>.\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_1, SDDS_output;
  SDDS_DATASET *SDDS_ref;
  REFDATA *new_data, rename_data, *take_RefData, *replace_RefData;
  long j, i_arg, reuse, reusePage, endWarning, k;
  int64_t i, i1, i2, i3, rows1, rows2, rows2Max;
  SCANNED_ARG *s_arg;
  char s[200], *ptr;

  char **take_column, **leave_column, **replace_column, **replace_parameter, **replace_array, **output_column = NULL;
  char **inputfile, **referfile;
  char **match_column, **equate_column;
  long take_columns, replace_columns, leave_columns, match_columns, equate_columns, leave_all_columns, replace_parameters, replace_arrays;
  int32_t output_columns = 0;
  char *input1, *input2, *output;
  long tmpfile_used, retval1, retval2, inputfiles, referfiles;
  long wildMatch;
  TRANSFER_DEFINITION *transfer;
  long transfers;
  long warnings;
  IFITEM_LIST ifnot_item, ifis_item;
  unsigned long pipeFlags, majorOrderFlag;
  long fillIn, keyGroups = 0;
  KEYED_EQUIVALENT **keyGroup = NULL;
  long outputInitialized;
  int z, it, itm, datatype1, datatype2;
  long col;
  int firstRun, copyInput1Only;
  char **string1, **string2;
  double *value1, *value2;
  long matched;
  short columnMajorOrder = -1;

  EDIT_NAME_REQUEST *edit_column_request, *edit_parameter_request, *edit_array_request;
  long edit_column_requests, edit_parameter_requests, edit_array_requests;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  SDDS_ref = NULL;
  take_RefData = replace_RefData = NULL;
  new_data = NULL;

  rename_data.columns = rename_data.parameters = rename_data.arrays = 0;
  rename_data.new_column = rename_data.orig_column = rename_data.new_parameter = rename_data.orig_parameter = rename_data.new_array = rename_data.orig_array = NULL;
  edit_column_request = edit_parameter_request = edit_array_request = NULL;
  edit_column_requests = edit_parameter_requests = edit_array_requests = 0;

  input1 = input2 = output = NULL;
  take_column = leave_column = replace_column = replace_parameter = replace_array = NULL;
  match_column = equate_column = NULL;
  inputfile = referfile = NULL;
  take_columns = leave_columns = replace_columns = match_columns = equate_columns = reuse = reusePage = replace_parameters = replace_arrays = 0;
  tmpfile_used = inputfiles = referfiles = 0;
  transfer = NULL;
  transfers = 0;
  ifnot_item.items = ifis_item.items = 0;
  warnings = 1;
  pipeFlags = 0;
  fillIn = 0;
  outputInitialized = 0;
  rows1 = rows2 = output_columns = 0;
  string1 = string2 = NULL;
  wildMatch = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
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
      case SET_LEAVE_COLUMNS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -leave syntax");
        leave_column = trealloc(leave_column, sizeof(*leave_column) * (leave_columns + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          leave_column[i - 1 + leave_columns] = s_arg[i_arg].list[i];
        leave_columns += s_arg[i_arg].n_items - 1;
        break;
      case SET_TAKE_COLUMNS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -take syntax");
        take_column = trealloc(take_column, sizeof(*take_column) * (take_columns + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          take_column[i - 1 + take_columns] = s_arg[i_arg].list[i];
        take_columns += s_arg[i_arg].n_items - 1;
        break;
      case SET_WILD_MATCH:
        wildMatch = 1;
        /* fall-through to SET_MATCH_COLUMN */
      case SET_MATCH_COLUMN:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -match or -wildMatch syntax");
        if (match_columns != 0)
          SDDS_Bomb("only one -match or -wildMatch option may be given");
        match_column = tmalloc(sizeof(*match_column) * 2);
        if ((ptr = strchr(s_arg[i_arg].list[1], '=')))
          *ptr++ = 0;
        else
          ptr = s_arg[i_arg].list[1];
        match_column[0] = s_arg[i_arg].list[1];
        match_column[1] = ptr;
        match_columns = 1;
        break;
      case SET_EQUATE_COLUMN:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -equate syntax");
        if (equate_columns != 0)
          SDDS_Bomb("only one -equate option may be given");
        equate_column = tmalloc(sizeof(*equate_column) * 2);
        if ((ptr = strchr(s_arg[i_arg].list[1], '=')))
          *ptr++ = 0;
        else
          ptr = s_arg[i_arg].list[1];
        equate_column[0] = s_arg[i_arg].list[1];
        equate_column[1] = ptr;
        equate_columns = 1;
        break;
      case SET_REPLACE:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -replace syntax");
        switch (match_string(s_arg[i_arg].list[1], replace_type, REPLACE_TYPES, 0)) {
        case COLUMN_REPLACE:
          replace_column = trealloc(replace_column, sizeof(*replace_column) * (replace_columns + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            replace_column[i - 2 + replace_columns] = s_arg[i_arg].list[i];
          replace_columns += s_arg[i_arg].n_items - 2;
          break;
        case PARAMETER_REPLACE:
          replace_parameter = trealloc(replace_parameter, sizeof(*replace_parameter) * (replace_parameters + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            replace_parameter[i - 2 + replace_parameters] = s_arg[i_arg].list[i];
          replace_parameters += s_arg[i_arg].n_items - 2;
          break;
        case ARRAY_REPLACE:
          replace_array = trealloc(replace_array, sizeof(*replace_array) * (replace_arrays + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            replace_array[i - 2 + replace_arrays] = s_arg[i_arg].list[i];
          replace_arrays += s_arg[i_arg].n_items - 2;
          break;
        default:
          SDDS_Bomb("unknown type of transfer");
          break;
        }
        break;
      case SET_TRANSFER:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -transfer syntax");
        transfer = trealloc(transfer, sizeof(*transfer) * (transfers + s_arg[i_arg].n_items - 2));
        switch (match_string(s_arg[i_arg].list[1], transfer_type, TRANSFER_TYPES, 0)) {
        case PARAMETER_TRANSFER:
          for (i = 2; i < s_arg[i_arg].n_items; i++) {
            transfer[i - 2 + transfers].type = PARAMETER_TRANSFER;
            transfer[i - 2 + transfers].name = s_arg[i_arg].list[i];
          }
          break;
        case ARRAY_TRANSFER:
          for (i = 2; i < s_arg[i_arg].n_items; i++) {
            transfer[i - 2 + transfers].type = ARRAY_TRANSFER;
            transfer[i - 2 + transfers].name = s_arg[i_arg].list[i];
          }
          break;
        default:
          SDDS_Bomb("unknown type of transfer");
          break;
        }
        transfers += s_arg[i_arg].n_items - 2;
        break;
      case SET_REUSE:
        if (s_arg[i_arg].n_items == 1)
          reuse = 1;
        else {
          char *reuseOptions[2] = {"rows", "page"};
          for (i = 1; i < s_arg[i_arg].n_items; i++) {
            switch (match_string(s_arg[i_arg].list[i], reuseOptions, 2, 0)) {
            case 0:
              reuse = 1;
              break;
            case 1:
              reusePage = 1;
              break;
            default:
              SDDS_Bomb("unknown reuse keyword");
              break;
            }
          }
        }
        break;
      case SET_IFNOT:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -ifnot usage");
        add_ifitem(&ifnot_item, s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1);
        break;
      case SET_NOWARNINGS:
        warnings = 0;
        break;
      case SET_IFIS:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -ifis usage");
        add_ifitem(&ifis_item, s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1);
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_FILLIN:
        fillIn = 1;
        break;
      case SET_RENAME:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -rename syntax");
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case COLUMN_MODE:
          k = rename_data.columns;
          rename_data.new_column = trealloc(rename_data.new_column, sizeof(char *) * (k + s_arg[i_arg].n_items - 2));
          rename_data.orig_column = trealloc(rename_data.orig_column, sizeof(char *) * (k + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++) {
            if (!(ptr = strchr(s_arg[i_arg].list[i], '=')))
              SDDS_Bomb("invalid -rename syntax");
            *ptr++ = 0;
            rename_data.orig_column[k + i - 2] = s_arg[i_arg].list[i];
            rename_data.new_column[k + i - 2] = ptr;
          }
          rename_data.columns += s_arg[i_arg].n_items - 2;
          break;
        case PARAMETER_MODE:
          k = rename_data.parameters;
          rename_data.new_parameter = trealloc(rename_data.new_parameter, sizeof(char *) * (k + s_arg[i_arg].n_items - 2));
          rename_data.orig_parameter = trealloc(rename_data.orig_parameter, sizeof(char *) * (k + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++) {
            if (!(ptr = strchr(s_arg[i_arg].list[i], '=')))
              SDDS_Bomb("invalid -rename syntax");
            *ptr++ = 0;
            rename_data.orig_parameter[k + i - 2] = s_arg[i_arg].list[i];
            rename_data.new_parameter[k + i - 2] = ptr;
          }
          rename_data.parameters += s_arg[i_arg].n_items - 2;
          break;
        case ARRAY_MODE:
          k = rename_data.arrays;
          rename_data.new_array = trealloc(rename_data.new_array, sizeof(char *) * (k + s_arg[i_arg].n_items - 2));
          rename_data.orig_array = trealloc(rename_data.orig_array, sizeof(char *) * (k + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++) {
            if (!(ptr = strchr(s_arg[i_arg].list[i], '=')))
              SDDS_Bomb("invalid -rename syntax");
            *ptr++ = 0;
            rename_data.orig_array[k + i - 2] = s_arg[i_arg].list[i];
            rename_data.new_array[k + i - 2] = ptr;
          }
          rename_data.arrays += s_arg[i_arg].n_items - 2;
          break;
        default:
          SDDS_Bomb("invalid -rename syntax: specify column, parameter, or array keyword");
          break;
        }
        break;
      case SET_EDIT_NAMES:
        if (s_arg[i_arg].n_items != 4)
          SDDS_Bomb("invalid -editnames syntax");
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case COLUMN_MODE:
          edit_column_request = trealloc(edit_column_request, sizeof(*edit_column_request) * (edit_column_requests + 1));
          edit_column_request[edit_column_requests].match_string = s_arg[i_arg].list[2];
          edit_column_request[edit_column_requests].edit_string = s_arg[i_arg].list[3];
          edit_column_requests++;
          break;
        case PARAMETER_MODE:
          edit_parameter_request = trealloc(edit_parameter_request, sizeof(*edit_parameter_request) * (edit_parameter_requests + 1));
          edit_parameter_request[edit_parameter_requests].match_string = s_arg[i_arg].list[2];
          edit_parameter_request[edit_parameter_requests].edit_string = s_arg[i_arg].list[3];
          edit_parameter_requests++;
          break;
        case ARRAY_MODE:
          edit_array_request = trealloc(edit_array_request, sizeof(*edit_array_request) * (edit_array_requests + 1));
          edit_array_request[edit_array_requests].match_string = s_arg[i_arg].list[2];
          edit_array_request[edit_array_requests].edit_string = s_arg[i_arg].list[3];
          edit_array_requests++;
          break;
        default:
          SDDS_Bomb("invalid -editnames syntax: specify column, parameter, or array keyword");
          break;
        }
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        SDDS_Bomb(NULL);
        break;
      }
    } else {
      inputfile = trealloc(inputfile, sizeof(*inputfile) * (inputfiles + 1));
      inputfile[inputfiles++] = s_arg[i_arg].list[0];
    }
  }

  if (inputfiles == 0) {
    SDDS_Bomb("too few reference files given");
  } else {
    if (!(pipeFlags & USE_STDIN) && !(pipeFlags & USE_STDOUT)) {
      if (inputfiles < 2) {
        SDDS_Bomb("too few reference files given");
      } else if (inputfiles == 2) {
        input1 = output = inputfile[0];
        referfile = trealloc(referfile, sizeof(*referfile) * (referfiles + 1));
        referfile[0] = inputfile[1];
        referfiles++;
      } else {
        input1 = inputfile[0];
        output = inputfile[--inputfiles];
        for (z = 1; z < inputfiles; z++) {
          referfile = trealloc(referfile, sizeof(*referfile) * (referfiles + 1));
          referfile[z - 1] = inputfile[z];
          referfiles++;
        }
      }
    } else if (!(pipeFlags & USE_STDIN) && (pipeFlags & USE_STDOUT)) {
      if (inputfiles < 2) {
        SDDS_Bomb("too few reference files given");
      } else {
        input1 = inputfile[0];
        for (z = 1; z < inputfiles; z++) {
          referfile = trealloc(referfile, sizeof(*referfile) * (referfiles + 1));
          referfile[z - 1] = inputfile[z];
          referfiles++;
        }
      }
    } else if ((pipeFlags & USE_STDIN) && !(pipeFlags & USE_STDOUT)) {
      if (inputfiles < 2) {
        SDDS_Bomb("too few reference files given");
      } else {
        output = inputfile[--inputfiles];
        for (z = 0; z < inputfiles; z++) {
          referfile = trealloc(referfile, sizeof(*referfile) * (referfiles + 1));
          referfile[z] = inputfile[z];
          referfiles++;
        }
      }
    } else {
      for (z = 0; z < inputfiles; z++) {
        referfile = trealloc(referfile, sizeof(*referfile) * (referfiles + 1));
        referfile[z] = inputfile[z];
        referfiles++;
      }
    }
  }

  processFilenames("sddsxref", &input1, &output, pipeFlags, !warnings, &tmpfile_used);

  if (equate_columns && match_columns)
    SDDS_Bomb("only one of -equate or -match may be given");

  if (!SDDS_InitializeInput(&SDDS_1, input1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (!check_ifitems(&SDDS_1, &ifnot_item, 0, warnings) || !check_ifitems(&SDDS_1, &ifis_item, 1, warnings))
    exit(EXIT_SUCCESS);

  for (it = 0; it < ifnot_item.items; it++) {
    switch (ifnot_item.type[it]) {
    case COLUMN_BASED:
      leave_column = trealloc(leave_column, sizeof(*leave_column) * (leave_columns + 1));
      leave_column[leave_columns] = ifnot_item.name[it];
      leave_columns++;
      break;
    case PARAMETER_BASED:
      for (itm = 0; itm < transfers; itm++) {
        if (strcmp(transfer[itm].name, ifnot_item.name[it]) == 0) {
          SDDS_Bomb("Excluded item is a part of -transfer list.");
          exit(EXIT_FAILURE);
        }
      }
      break;
    case ARRAY_BASED:
      for (itm = 0; itm < transfers; itm++) {
        if (strcmp(transfer[itm].name, ifnot_item.name[it]) == 0) {
          SDDS_Bomb("Excluded item is a part of -transfer list.");
          exit(EXIT_FAILURE);
        }
      }
      break;
    default:
      SDDS_Bomb("internal error---unknown ifitem type");
      exit(EXIT_FAILURE);
      break;
    }
  }

  /* Allocate memory for new_data */
  new_data = malloc(sizeof(*new_data) * referfiles);
  for (z = 0; z < referfiles; z++) {
    SDDS_ref = trealloc(SDDS_ref, sizeof(*SDDS_ref) * (z + 1));
    input2 = referfile[z];
    if (!SDDS_InitializeInput(&SDDS_ref[z], input2)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }

    take_RefData = trealloc(take_RefData, sizeof(*take_RefData) * (z + 1));
    take_RefData[z].columns = 0;
    replace_RefData = trealloc(replace_RefData, sizeof(*replace_RefData) * (z + 1));
    replace_RefData[z].columns = replace_RefData[z].parameters = replace_RefData[z].arrays = 0;

    add_newnames(&SDDS_ref[z], &new_data[z], rename_data, edit_column_request, edit_column_requests,
                 edit_parameter_request, edit_parameter_requests, edit_array_request, edit_array_requests, z + 1);

    if (SDDS_ColumnCount(&SDDS_ref[z])) {
      SDDS_SetColumnFlags(&SDDS_ref[z], 1);
      if (take_columns) {
        SDDS_SetColumnFlags(&SDDS_ref[z], 0);
        for (i = 0; i < take_columns; i++) {
          if (!has_wildcards(take_column[i]) && SDDS_GetColumnIndex(&SDDS_ref[z], take_column[i]) < 0) {
            sprintf(s, "error: column %s not found in file %s take_columns %ld SDDS_ref[z] %" PRId64 "\n", take_column[i], input2, take_columns, SDDS_ref[z].n_rows);
            SDDS_SetError(s);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          if (!SDDS_SetColumnsOfInterest(&SDDS_ref[z], SDDS_MATCH_STRING, take_column[i], SDDS_OR))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }

      leave_all_columns = 0;
      if (leave_columns == 1 && strcmp(leave_column[0], "*") == 0)
        leave_all_columns = 1;
      else {
        if (!take_columns)
          SDDS_SetColumnFlags(&SDDS_ref[z], 1);
        for (i = 0; i < leave_columns; i++) {
          if (!has_wildcards(leave_column[i]) &&
              SDDS_GetColumnIndex(&SDDS_ref[z], leave_column[i]) < 0)
            continue;
          if (!SDDS_SetColumnsOfInterest(&SDDS_ref[z], SDDS_MATCH_STRING, leave_column[i], SDDS_AND | SDDS_NEGATE_MATCH))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }

        if (!(take_RefData[z].orig_column = (char **)SDDS_GetColumnNames(&SDDS_ref[z], &take_RefData[z].columns))) {
          SDDS_SetError("error: no columns selected to take from input file");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      if (replace_columns) {
        SDDS_SetColumnFlags(&SDDS_ref[z], 0);
        for (i = 0; i < replace_columns; i++) {
          if (!has_wildcards(replace_column[i]) && SDDS_GetColumnIndex(&SDDS_ref[z], replace_column[i]) < 0) {
            sprintf(s, "error:  column %s not found in file %s replace_columns %ld SDDS_ref[z] %" PRId64 "\n", replace_column[i], input2, replace_columns, SDDS_ref[z].n_rows);
            SDDS_SetError(s);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          if (!SDDS_SetColumnsOfInterest(&SDDS_ref[z], SDDS_MATCH_STRING, replace_column[i], SDDS_OR))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!(replace_RefData[z].orig_column = (char **)SDDS_GetColumnNames(&SDDS_ref[z], &replace_RefData[z].columns))) {
          SDDS_SetError("error: no columns selected to replace from input file");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      if (match_columns) {
        if ((j = SDDS_GetColumnIndex(&SDDS_1, match_column[0])) < 0 ||
            SDDS_GetColumnType(&SDDS_1, j) != SDDS_STRING) {
          sprintf(s, "error: column %s not found or not string type in file %s", match_column[0], input1 ? input1 : "stdin");
          SDDS_SetError(s);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if ((j = SDDS_GetColumnIndex(&SDDS_ref[z], match_column[1])) < 0 ||
            SDDS_GetColumnType(&SDDS_ref[z], j) != SDDS_STRING) {
          sprintf(s, "error: column %s not found or not string type in file %s", match_column[1], input2);
          SDDS_SetError(s);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      if (equate_columns) {
        if ((j = SDDS_GetColumnIndex(&SDDS_1, equate_column[0])) < 0 ||
            !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&SDDS_1, j))) {
          sprintf(s, "error: column %s not found or not numeric type in file %s", equate_column[0], input1 ? input1 : "stdin");
          SDDS_SetError(s);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if ((j = SDDS_GetColumnIndex(&SDDS_ref[z], equate_column[1])) < 0 ||
            !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&SDDS_ref[z], j))) {
          sprintf(s, "error: column %s not found or not numeric type in file %s", equate_column[1], input2);
          SDDS_SetError(s);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    } else {
      take_RefData[z].columns = 0;
      leave_all_columns = 1;
    }
    if (!take_RefData[z].columns && !leave_all_columns && warnings)
      fprintf(stderr, "warning: there are no columns being taken from %s that are not already in %s\n", input2, input1 ? input1 : "stdin");

    if (leave_all_columns)
      take_RefData[z].columns = 0;

    if (!outputInitialized) {
      if (!SDDS_InitializeCopy(&SDDS_output, &SDDS_1, output, "w")) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      outputInitialized = 1;
      if (columnMajorOrder != -1)
        SDDS_output.layout.data_mode.column_major = columnMajorOrder;
      else
        SDDS_output.layout.data_mode.column_major = SDDS_1.layout.data_mode.column_major;
    }

    /* Get the new name for new_data if there is a match of original name */
    if (take_RefData[z].columns)
      take_RefData[z].new_column = (char **)malloc(sizeof(char *) * take_RefData[z].columns);

    for (i = 0; i < take_RefData[z].columns; i++) {
      k = 0;
      /* If there are new names (renamed or edited), find the corresponding original name index,
         and assign the new name to take_RefData */
      if (new_data[z].columns) {
        k = match_string(take_RefData[z].orig_column[i], new_data[z].orig_column, new_data[z].columns, EXACT_MATCH);
        if (k == -1)
          SDDS_CopyString(&take_RefData[z].new_column[i], take_RefData[z].orig_column[i]);
        else
          SDDS_CopyString(&take_RefData[z].new_column[i], new_data[z].new_column[k]);
      } else
        SDDS_CopyString(&take_RefData[z].new_column[i], take_RefData[z].orig_column[i]);
      if (SDDS_GetColumnIndex(&SDDS_output, take_RefData[z].new_column[i]) >= 0) {
        free(take_RefData[z].new_column[i]);
        free(take_RefData[z].orig_column[i]);
        for (j = i; j < take_RefData[z].columns - 1; j++)
          take_RefData[z].orig_column[j] = take_RefData[z].orig_column[j + 1];
        take_RefData[z].columns -= 1;
        i--;
        if (take_RefData[z].columns == 0)
          break;
      } else {
        /* Transfer column definition */
        if (!SDDS_TransferColumnDefinition(&SDDS_output, &SDDS_ref[z], take_RefData[z].orig_column[i], take_RefData[z].new_column[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    if (!take_RefData[z].columns && !leave_all_columns && warnings)
      fprintf(stderr, "warning: there are no columns being taken from %s that are not already in %s\n", input2, input1 ? input1 : "stdin");
    output_columns = 0;
    if (take_RefData[z].columns &&
        (!(output_column = (char **)SDDS_GetColumnNames(&SDDS_output, &output_columns)) || output_columns == 0)) {
      SDDS_SetError("Problem getting output column names");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (output_columns) {
      for (i = 0; i < output_columns; i++)
        free(output_column[i]);
      free(output_column);
    }
    /* Check if the column in replace_column exists in the output file */
    for (i = 0; i < replace_RefData[z].columns; i++) {
      if (SDDS_GetColumnIndex(&SDDS_1, replace_RefData[z].orig_column[i]) < 0) {
        if (warnings) {
          fprintf(stderr, "Warning, %s replace column does not exist in the input1, ignore.\n", replace_RefData[z].orig_column[i]);
        }
      } else {
        /* Check if column types are the same */
        j = SDDS_GetColumnIndex(&SDDS_ref[z], replace_RefData[z].orig_column[i]);
        k = SDDS_GetColumnIndex(&SDDS_output, replace_RefData[z].orig_column[i]);
        datatype1 = SDDS_GetColumnType(&SDDS_ref[z], j);
        datatype2 = SDDS_GetColumnType(&SDDS_output, k);
        if (datatype1 != datatype2 && (datatype1 == SDDS_STRING || datatype2 == SDDS_STRING)) {
          if (warnings) {
            if (datatype1 == SDDS_STRING)
              fprintf(stderr, "Warning: cannot replace a numeric column with a string column, replace %s ignored.\n", replace_RefData[z].orig_column[i]);
            if (datatype2 == SDDS_STRING)
              fprintf(stderr, "Warning: cannot replace a string column with a numeric column, replace %s ignored.\n", replace_RefData[z].orig_column[i]);
          }
        } else {
          if (datatype1 != datatype2) {
            if (warnings)
              fprintf(stderr, "Warning, replace column %s has different data type as the column in input1; redefining the column type\n", replace_RefData[z].orig_column[i]);
            if (!SDDS_ChangeColumnInformation(&SDDS_output, "type", SDDS_type_name[datatype1 - 1], SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, replace_RefData[z].orig_column[i])) {
              fprintf(stderr, "Problem redefining column type for %s\n", replace_RefData[z].orig_column[i]);
              exit(EXIT_FAILURE);
            }
          }
          /* Add replace_column to take_column, while the orig_name and new_name are the same */
          take_RefData[z].orig_column = trealloc(take_RefData[z].orig_column, sizeof(*(take_RefData[z].orig_column)) * (take_RefData[z].columns + 1));
          take_RefData[z].new_column = trealloc(take_RefData[z].new_column, sizeof(*(take_RefData[z].new_column)) * (take_RefData[z].columns + 1));
          SDDS_CopyString(&take_RefData[z].orig_column[take_RefData[z].columns], replace_RefData[z].orig_column[i]);
          SDDS_CopyString(&take_RefData[z].new_column[take_RefData[z].columns], replace_RefData[z].orig_column[i]);
          take_RefData[z].columns++;
        }
      }
      free(replace_RefData[z].orig_column[i]);
    }

    take_RefData[z].parameters = take_RefData[z].arrays = 0;
    if (transfers) {
      if (!expandTransferRequests(&take_RefData[z].orig_parameter, &take_RefData[z].parameters, PARAMETER_TRANSFER, transfer, transfers, &SDDS_ref[z]) ||
          !expandTransferRequests(&take_RefData[z].orig_array, &take_RefData[z].arrays, ARRAY_TRANSFER, transfer, transfers, &SDDS_ref[z]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    /* Get the new name for parameters, define parameters for output */
    if (take_RefData[z].parameters)
      take_RefData[z].new_parameter = (char **)malloc(sizeof(char *) * take_RefData[z].parameters);
    if (take_RefData[z].arrays)
      take_RefData[z].new_array = (char **)malloc(sizeof(char *) * take_RefData[z].arrays);

    for (i = 0; i < take_RefData[z].parameters; i++) {
      k = 0;
      if (new_data[z].parameters) {
        k = match_string(take_RefData[z].orig_parameter[i], new_data[z].orig_parameter, new_data[z].parameters, EXACT_MATCH);
        if (k != -1)
          SDDS_CopyString(&take_RefData[z].new_parameter[i], new_data[z].new_parameter[k]);
        else
          SDDS_CopyString(&take_RefData[z].new_parameter[i], take_RefData[z].orig_parameter[i]);
      } else
        SDDS_CopyString(&take_RefData[z].new_parameter[i], take_RefData[z].orig_parameter[i]);
      if (SDDS_GetParameterIndex(&SDDS_output, take_RefData[z].new_parameter[i]) >= 0) {
        free(take_RefData[z].orig_parameter[i]);
        free(take_RefData[z].new_parameter[i]);
        for (col = i; col < take_RefData[z].parameters - 1; col++)
          take_RefData[z].orig_parameter[col] = take_RefData[z].orig_parameter[col + 1];
        take_RefData[z].parameters -= 1;
        i--;
        if (take_RefData[z].parameters == 0)
          break;
      } else {
        if (!SDDS_TransferParameterDefinition(&SDDS_output, &SDDS_ref[z], take_RefData[z].orig_parameter[i], take_RefData[z].new_parameter[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    /* Get the new name for arrays, and define arrays for output */
    for (i = 0; i < take_RefData[z].arrays; i++) {
      k = 0;
      if (new_data[z].arrays) {
        k = match_string(take_RefData[z].orig_array[i], new_data[z].orig_array, new_data[z].arrays, EXACT_MATCH);
        if (k == -1)
          SDDS_CopyString(&take_RefData[z].new_array[i], take_RefData[z].orig_array[i]);
        else
          SDDS_CopyString(&take_RefData[z].new_array[i], new_data[z].new_array[k]);
      } else
        SDDS_CopyString(&take_RefData[z].new_array[i], take_RefData[z].orig_array[i]);
      if (SDDS_GetArrayIndex(&SDDS_output, take_RefData[z].new_array[i]) >= 0) {
        free(take_RefData[z].orig_array[i]);
        free(take_RefData[z].new_array[i]);
        for (col = i; col < take_RefData[z].arrays - 1; col++)
          take_RefData[z].orig_array[col] = take_RefData[z].orig_array[col + 1];
        take_RefData[z].arrays -= 1;
        i--;
        if (take_RefData[z].arrays == 0)
          break;
      } else {
        if (!SDDS_TransferArrayDefinition(&SDDS_output, &SDDS_ref[z], take_RefData[z].orig_array[i], take_RefData[z].new_array[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    /* Check replace parameters and arrays, adding them to take_refData */
    if (replace_parameters) {
      for (i = 0; i < replace_parameters; i++) {
        replace_RefData[z].parameters += SDDS_MatchParameters(&SDDS_ref[z], &replace_RefData[z].orig_parameter, SDDS_MATCH_STRING, FIND_ANY_TYPE, replace_parameter[i], SDDS_OR | SDDS_1_PREVIOUS);
      }

      /* Check if replace parameters exist in input1 */
      for (i = 0; i < replace_RefData[z].parameters; i++) {
        if (SDDS_GetParameterIndex(&SDDS_1, replace_RefData[z].orig_parameter[i]) < 0) {
          if (warnings) {
            fprintf(stderr, "Warning, parameter %s replace parameter does not exist in the input1, ignore.\n", replace_RefData[z].orig_parameter[i]);
          }
        } else {
          /* Check if parameter types are the same */
          j = SDDS_GetParameterIndex(&SDDS_ref[z], replace_RefData[z].orig_parameter[i]);
          k = SDDS_GetParameterIndex(&SDDS_output, replace_RefData[z].orig_parameter[i]);
          datatype1 = SDDS_GetParameterType(&SDDS_ref[z], j);
          datatype2 = SDDS_GetParameterType(&SDDS_output, k);
          if (datatype1 != datatype2 && (datatype1 == SDDS_STRING || datatype2 == SDDS_STRING)) {
            if (warnings) {
              if (datatype1 == SDDS_STRING)
                fprintf(stderr, "Warning: cannot replace a numeric parameter with a string parameter, replace %s ignored.\n", replace_RefData[z].orig_parameter[i]);
              if (datatype2 == SDDS_STRING)
                fprintf(stderr, "Warning: cannot replace a string parameter with a numeric parameter, replace %s ignored.\n", replace_RefData[z].orig_parameter[i]);
            }
          } else {
            if (datatype1 != datatype2) {
              if (warnings)
                fprintf(stderr, "Warning, replace parameter %s type is different from input1, redefining parameter type.\n", replace_RefData[z].orig_parameter[i]);

              if (!SDDS_ChangeParameterInformation(&SDDS_output, "type", SDDS_type_name[datatype1 - 1], SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, replace_RefData[z].orig_parameter[i])) {
                fprintf(stderr, "Problem redefining parameter type for %s\n", replace_RefData[z].orig_parameter[i]);
                exit(EXIT_FAILURE);
              }
            }
            /* Add replace_parameter to take_parameter, while the orig_name and new_name are the same */
            take_RefData[z].orig_parameter = trealloc(take_RefData[z].orig_parameter, sizeof(*(take_RefData[z].orig_parameter)) * (take_RefData[z].parameters + 1));
            take_RefData[z].new_parameter = trealloc(take_RefData[z].new_parameter, sizeof(*(take_RefData[z].new_parameter)) * (take_RefData[z].parameters + 1));
            SDDS_CopyString(&take_RefData[z].orig_parameter[take_RefData[z].parameters], replace_RefData[z].orig_parameter[i]);
            SDDS_CopyString(&take_RefData[z].new_parameter[take_RefData[z].parameters], replace_RefData[z].orig_parameter[i]);
            take_RefData[z].parameters++;
          }
        }
        free(replace_RefData[z].orig_parameter[i]);
      }

      if (replace_arrays) {
        for (i = 0; i < replace_arrays; i++) {
          replace_RefData[z].arrays += SDDS_MatchArrays(&SDDS_ref[z], &replace_RefData[z].orig_array, SDDS_MATCH_STRING, FIND_ANY_TYPE, replace_array[i], SDDS_OR | SDDS_1_PREVIOUS);
        }
        /* Check if replace arrays exist in input1 */
        for (i = 0; i < replace_RefData[z].arrays; i++) {
          if (SDDS_GetArrayIndex(&SDDS_1, replace_RefData[z].orig_array[i]) < 0) {
            if (warnings) {
              fprintf(stderr, "Warning, array %s replace array does not exist in the input, ignore.\n", replace_RefData[z].orig_array[i]);
            }
          } else {
            /* Check if array types are the same */
            j = SDDS_GetArrayIndex(&SDDS_ref[z], replace_RefData[z].orig_array[i]);
            k = SDDS_GetArrayIndex(&SDDS_output, replace_RefData[z].orig_array[i]);
            datatype1 = SDDS_GetArrayType(&SDDS_ref[z], j);
            datatype2 = SDDS_GetArrayType(&SDDS_output, k);
            if (datatype1 != datatype2 && (datatype1 == SDDS_STRING || datatype2 == SDDS_STRING)) {
              if (warnings) {
                if (datatype1 == SDDS_STRING)
                  fprintf(stderr, "Warning: cannot replace a numeric array with a string array, replace %s ignored.\n", replace_RefData[z].orig_array[i]);
                if (datatype2 == SDDS_STRING)
                  fprintf(stderr, "Warning: cannot replace a string array with a numeric array, replace %s ignored.\n", replace_RefData[z].orig_array[i]);
              }
            } else {
              if (datatype1 != datatype2) {
                if (warnings)
                  fprintf(stderr, "Warning, replace array %s has different data type as the array in input1; redefining\n", replace_RefData[z].orig_array[i]);
                if (!SDDS_ChangeArrayInformation(&SDDS_output, "type", SDDS_type_name[datatype1 - 1], SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, replace_RefData[z].orig_array[i])) {
                  fprintf(stderr, "Problem redefining array type for %s\n", replace_RefData[z].orig_array[i]);
                  exit(EXIT_FAILURE);
                }
              }
              /* Add replace_array to take_array, while the orig_name and new_name are the same */
              take_RefData[z].orig_array = trealloc(take_RefData[z].orig_array, sizeof(*(take_RefData[z].orig_array)) * (take_RefData[z].arrays + 1));
              SDDS_CopyString(&take_RefData[z].orig_array[take_RefData[z].arrays], replace_RefData[z].orig_array[i]);
              SDDS_CopyString(&take_RefData[z].new_array[take_RefData[z].arrays], replace_RefData[z].orig_array[i]);
              take_RefData[z].arrays++;
            }
          }
          free(replace_RefData[z].orig_array[i]);
        }
      }
    }
  }

  if (!SDDS_WriteLayout(&SDDS_output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  free(leave_column);
  if (take_columns) {
    SDDS_FreeStringArray(take_column, take_columns);
    free(take_column);
  }

  endWarning = 0;

  while ((retval1 = SDDS_ReadPage(&SDDS_1)) > 0) {
    copyInput1Only = 0;
    rows1 = SDDS_CountRowsOfInterest(&SDDS_1);
    if (!SDDS_StartPage(&SDDS_output, rows1)) {
      SDDS_SetError("Problem starting output page");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (fillIn && !SDDS_ClearPage(&SDDS_output)) {
      SDDS_SetError("Problem clearing output page");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_CopyParameters(&SDDS_output, &SDDS_1) ||
        !SDDS_CopyArrays(&SDDS_output, &SDDS_1)) {
      SDDS_SetError("Problem copying parameter or array data from first input file");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    for (z = 0; z < referfiles; z++) {
      input2 = referfile[z];
      if (!reusePage) {
        if ((retval2 = SDDS_ReadPage(&SDDS_ref[z])) <= 0 && !endWarning) {
          if (warnings)
            fprintf(stderr, "warning: %s ends prematurely\n", input2 ? input2 : "stdin");
          endWarning = 1;
        }
      } else {
        if (retval1 == 1 && (retval2 = SDDS_ReadPage(&SDDS_ref[z])) <= 0) {
          if (!endWarning && warnings)
            fprintf(stderr, "warning: %s has no data\n", input2 ? input2 : "stdin");
          endWarning = 1;
        } else
          SDDS_SetRowFlags(&SDDS_ref[z], 1);
      }

      if (take_RefData[z].columns &&
          (!SDDS_SetColumnFlags(&SDDS_ref[z], 0) ||
           !SDDS_SetColumnsOfInterest(&SDDS_ref[z], SDDS_NAME_ARRAY, take_RefData[z].columns, take_RefData[z].orig_column)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      /* Copy parameters and arrays */
      if (!CopyParametersFromSecondInput(&SDDS_output, &SDDS_ref[z], take_RefData[z])) {
        SDDS_SetError("Problem copying parameter from second input file");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!CopyArraysFromSecondInput(&SDDS_output, &SDDS_ref[z], take_RefData[z])) {
        SDDS_SetError("Problem copying parameter from second input file");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    firstRun = 1;
    for (z = rows2Max = 0; z < referfiles; z++) {
      input2 = referfile[z];
      rows2 = SDDS_CountRowsOfInterest(&SDDS_ref[z]);
      rows2Max = rows2 > rows2Max ? rows2 : rows2Max;

      if (!firstRun) {
        /* DO NOT USE SDDS_CountRowsOfInterest because
           CopyRowToNewColumn and SDDS_AssertRowFlags expect
           the real row index and not the index of rows of interest */
        rows1 = SDDS_RowCount(&SDDS_output);
      }
      if (take_RefData[z].columns) {
        if (!rows2) {
          if (!SDDS_SetRowFlags(&SDDS_output, fillIn)) {
            SDDS_SetError("Problem setting row flags for output file.");
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        } else if (rows1) {
          if (match_columns) {
            if (firstRun) {
              if (!(string1 = (char **)SDDS_GetColumn(&SDDS_1, match_column[0]))) {
                fprintf(stderr, "Error: problem getting column %s from file %s\n", match_column[0], input1 ? input1 : "stdin");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            } else {
              if (!(string1 = (char **)SDDS_GetColumn(&SDDS_output, match_column[0]))) {
                fprintf(stderr, "Error: problem getting column %s from file %s\n", match_column[0], input1 ? input1 : "stdin");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            if (!(string2 = (char **)SDDS_GetColumn(&SDDS_ref[z], match_column[1]))) {
              fprintf(stderr, "Error: problem getting column %s from file %s\n", match_column[1], input2);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            if (!wildMatch)
              keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_STRING, string2, rows2);
            i3 = 0;
            for (i1 = 0; i1 < rows1; i1++) {
              if (firstRun) {
                if (!SDDS_CopyRowDirect(&SDDS_output, i1, &SDDS_1, i1)) {
                  sprintf(s, "Problem copying row %" PRId64 " of first data set", i1);
                  SDDS_SetError(s);
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                }
              }
              matched = 0;
              if ((&SDDS_output)->row_flag[i1]) {
                if (!wildMatch) {
                  if ((i2 = FindMatchingKeyGroup(keyGroup, keyGroups, SDDS_STRING, string1 + i3, reuse)) >= 0)
                    matched = 1;
                } else {
                  if ((i2 = match_string(string1[i3], string2, rows2, WILDCARD_MATCH)) >= 0)
                    matched = 1;
                }
                if (matched) {
                  if (!CopyRowToNewColumn(&SDDS_output, i1, &SDDS_ref[z], i2, take_RefData[z], take_RefData[z].columns, input2)) {
                    fprintf(stderr, "error in copying data to output!\n");
                    exit(EXIT_FAILURE);
                  }
                } else {
                  if (!fillIn && !SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, i1, i1, (int32_t)0))
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                  if (warnings)
                    fprintf(stderr, "warning: no match for row %" PRId64 " (%s = \"%s\")\n", i3, match_column[0], string1[i3]);
                }
                i3++;
              }
            }
            firstRun = 0;
            if (string1) {
              for (i = 0; i < i3; i++)
                free(string1[i]);
              free(string1);
            }
            if (string2) {
              for (i = 0; i < rows2; i++)
                free(string2[i]);
              free(string2);
            }

            for (i = 0; i < keyGroups; i++) {
              free(keyGroup[i]->equivalent);
              free(keyGroup[i]);
            }
            free(keyGroup);
          } else if (equate_columns) {
            if (firstRun) {
              if (!(value1 = SDDS_GetColumnInDoubles(&SDDS_1, equate_column[0]))) {
                fprintf(stderr, "Error: problem getting column %s from file %s\n", equate_column[0], input1 ? input1 : "stdin");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            } else {
              if (!(value1 = SDDS_GetColumnInDoubles(&SDDS_output, equate_column[0]))) {
                fprintf(stderr, "Error: problem getting column %s from file %s\n", equate_column[0], input1 ? input1 : "stdin");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            if (!(value2 = SDDS_GetColumnInDoubles(&SDDS_ref[z], equate_column[1]))) {
              fprintf(stderr, "Error: problem getting column %s from file %s\n", equate_column[1], input2);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }

            i3 = 0;
            keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_DOUBLE, value2, rows2);
            for (i1 = 0; i1 < rows1; i1++) {
              if (firstRun) {
                if (!SDDS_CopyRowDirect(&SDDS_output, i1, &SDDS_1, i1)) {
                  sprintf(s, "Problem copying row %" PRId64 " of first data set", i1);
                  SDDS_SetError(s);
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                }
              }
              if ((&SDDS_output)->row_flag[i1]) {
                if ((i2 = FindMatchingKeyGroup(keyGroup, keyGroups, SDDS_DOUBLE, value1 + i3, reuse)) >= 0) {
                  if (!CopyRowToNewColumn(&SDDS_output, i1, &SDDS_ref[z], i2, take_RefData[z], take_RefData[z].columns, input2)) {
                    fprintf(stderr, "error in copying data to output!\n");
                    exit(EXIT_FAILURE);
                  }
                } else {
                  if (!fillIn && !SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, i1, i1, (int32_t)0))
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                  if (warnings)
                    fprintf(stderr, "warning: no equal for row %" PRId64 " (%s = %g)\n", i3, equate_column[0], value1[i3]);
                }
                i3++;
              }
            }
            firstRun = 0;
            if (i3 && equate_columns)
              free(value1);
            if (rows2 && equate_columns)
              free(value2);
            for (i = 0; i < keyGroups; i++) {
              free(keyGroup[i]->equivalent);
              free(keyGroup[i]);
            }
            free(keyGroup);
          } else {
            for (i1 = 0; i1 < rows1; i1++) {
              i2 = i1;
              if (i2 >= rows2) {
                if (!reuse) {
                  if (fillIn) {
                    if (!SDDS_CopyRowDirect(&SDDS_output, i1, &SDDS_1, i1)) {
                      sprintf(s, "Problem copying row %" PRId64 " of first data set", i1);
                      SDDS_SetError(s);
                      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                    }
                  }
                  if (warnings)
                    fprintf(stderr, "warning: no row in file 2 for row %" PRId64 " in file 1\n", i1);
                  continue;
                } else
                  i2 = rows2 - 1;
              }
              if (firstRun) {
                if (!SDDS_CopyRowDirect(&SDDS_output, i1, &SDDS_1, i1)) {
                  sprintf(s, "Problem copying row %" PRId64 " of first data set", i1);
                  SDDS_SetError(s);
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                }
              }
              if (take_RefData[z].columns &&
                  !CopyRowToNewColumn(&SDDS_output, i1, &SDDS_ref[z], i2, take_RefData[z], take_RefData[z].columns, input2)) {
                fprintf(stderr, "error in copying data to output!\n");
                exit(EXIT_FAILURE);
              }
            }
            firstRun = 0;
          }
        }
      } else {
        if (rows2) {
          if (rows1) {
            if (match_columns) {
              if (firstRun) {
                if (!(string1 = (char **)SDDS_GetColumn(&SDDS_1, match_column[0]))) {
                  fprintf(stderr, "Error: problem getting column %s from file %s\n", match_column[0], input1 ? input1 : "stdin");
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                }
              } else {
                if (!(string1 = (char **)SDDS_GetColumn(&SDDS_output, match_column[0]))) {
                  fprintf(stderr, "Error: problem getting column %s from file %s\n", match_column[0], input1 ? input1 : "stdin");
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                }
              }
              if (!(string2 = (char **)SDDS_GetColumn(&SDDS_ref[z], match_column[1]))) {
                fprintf(stderr, "Error: problem getting column %s from file %s\n", match_column[1], input2);
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
              keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_STRING, string2, rows2);
              i3 = 0;
              for (i1 = 0; i1 < rows1; i1++) {
                if (firstRun) {
                  if (!SDDS_CopyRowDirect(&SDDS_output, i1, &SDDS_1, i1)) {
                    sprintf(s, "Problem copying row %" PRId64 " of first data set", i1);
                    SDDS_SetError(s);
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                  }
                }
                if ((&SDDS_output)->row_flag[i1]) {
                  if ((FindMatchingKeyGroup(keyGroup, keyGroups, SDDS_STRING, string1 + i3, reuse)) < 0) {
                    if (!fillIn && !SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, i1, i1, (int32_t)0))
                      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                    if (warnings)
                      fprintf(stderr, "warning: no match for row %" PRId64 " (%s = \"%s\")\n", i3, match_column[0], string1[i3]);
                  }
                  i3++;
                }
              }
              firstRun = 0;
              if (string1) {
                for (i = 0; i < i3; i++)
                  free(string1[i]);
                free(string1);
              }
              if (string2) {
                for (i = 0; i < rows2; i++)
                  free(string2[i]);
                free(string2);
              }

              for (i = 0; i < keyGroups; i++) {
                free(keyGroup[i]->equivalent);
                free(keyGroup[i]);
              }
              free(keyGroup);
            } else if (equate_columns) {
              if (firstRun) {
                if (!(value1 = SDDS_GetColumnInDoubles(&SDDS_1, equate_column[0]))) {
                  fprintf(stderr, "Error: problem getting column %s from file %s\n", equate_column[0], input1 ? input1 : "stdin");
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                }
              } else {
                if (!(value1 = SDDS_GetColumnInDoubles(&SDDS_output, equate_column[0]))) {
                  fprintf(stderr, "Error: problem getting column %s from file %s\n", equate_column[0], input1 ? input1 : "stdin");
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                }
              }
              if (!(value2 = SDDS_GetColumnInDoubles(&SDDS_ref[z], equate_column[1]))) {
                fprintf(stderr, "Error: problem getting column %s from file %s\n", equate_column[1], input2);
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
              keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_DOUBLE, value2, rows2);
              i3 = 0;
              for (i1 = 0; i1 < rows1; i1++) {
                if (firstRun) {
                  if (!SDDS_CopyRowDirect(&SDDS_output, i1, &SDDS_1, i1)) {
                    sprintf(s, "Problem copying row %" PRId64 " of first data set", i1);
                    SDDS_SetError(s);
                    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                  }
                }
                if ((&SDDS_output)->row_flag[i1]) {
                  if ((FindMatchingKeyGroup(keyGroup, keyGroups, SDDS_DOUBLE, value1 + i3, reuse)) < 0) {
                    if (!fillIn && !SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, i1, i1, (int32_t)0))
                      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                    if (warnings)
                      fprintf(stderr, "warning: no equal for row %" PRId64 " (%s = %g)\n", i3, equate_column[0], value1[i3]);
                  }
                  i3++;
                }
              }
              firstRun = 0;
              if (i3 && equate_columns)
                free(value1);
              if (rows2 && equate_columns)
                free(value2);
              for (i = 0; i < keyGroups; i++) {
                free(keyGroup[i]->equivalent);
                free(keyGroup[i]);
              }
              free(keyGroup);
            }
          }
        }
        copyInput1Only++;
      }
    }
    if ((rows2Max == 0 && fillIn) || (copyInput1Only == referfiles && !match_columns && !equate_columns)) {
      if (!SDDS_CopyColumns(&SDDS_output, &SDDS_1)) {
        SDDS_SetError("Problem copying tabular data for output file.");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    if (!SDDS_WritePage(&SDDS_output)) {
      SDDS_SetError("Problem writing data to output file");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  for (z = 0; z < referfiles; z++) {
    free(referfile[z]);

    if (take_RefData[z].columns) {
      for (i = 0; i < take_RefData[z].columns; i++) {
        free(take_RefData[z].new_column[i]);
        free(take_RefData[z].orig_column[i]);
      }
      free(take_RefData[z].new_column);
      free(take_RefData[z].orig_column);
    }

    if (take_RefData[z].parameters) {
      for (i = 0; i < take_RefData[z].parameters; i++) {
        free(take_RefData[z].new_parameter[i]);
        free(take_RefData[z].orig_parameter[i]);
      }
      free(take_RefData[z].new_parameter);
      free(take_RefData[z].orig_parameter);
    }

    if (take_RefData[z].arrays) {
      for (i = 0; i < take_RefData[z].arrays; i++) {
        free(take_RefData[z].new_array[i]);
        free(take_RefData[z].orig_array[i]);
      }
      free(take_RefData[z].new_array);
      free(take_RefData[z].orig_array);
    }

    if (new_data[z].columns) {
      for (i = 0; i < new_data[z].columns; i++) {
        free(new_data[z].new_column[i]);
        free(new_data[z].orig_column[i]);
      }
      free(new_data[z].new_column);
      free(new_data[z].orig_column);
    }

    if (new_data[z].parameters) {
      for (i = 0; i < new_data[z].parameters; i++) {
        free(new_data[z].new_parameter[i]);
        free(new_data[z].orig_parameter[i]);
      }
      free(new_data[z].new_parameter);
      free(new_data[z].orig_parameter);
    }
    if (new_data[z].arrays) {
      for (i = 0; i < new_data[z].arrays; i++) {
        free(new_data[z].new_array[i]);
        free(new_data[z].orig_array[i]);
      }
      free(new_data[z].new_array);
      free(new_data[z].orig_array);
    }
  }
  if (new_data)
    free(new_data);

  if (edit_column_requests) {
    for (i = 0; i < edit_column_requests; i++) {
      free(edit_column_request[i].match_string);
      free(edit_column_request[i].edit_string);
    }
    free(edit_column_request);
  }
  if (edit_parameter_requests) {
    for (i = 0; i < edit_parameter_requests; i++) {
      free(edit_parameter_request[i].match_string);
      free(edit_parameter_request[i].edit_string);
    }
    free(edit_parameter_request);
  }

  if (edit_array_requests) {
    for (i = 0; i < edit_array_requests; i++) {
      free(edit_array_request[i].match_string);
      free(edit_array_request[i].edit_string);
    }
    free(edit_array_request);
  }

  free(take_RefData);
  if (replace_RefData)
    free(replace_RefData);
  free(referfile);
  free(inputfile);

  if (match_columns)
    free(match_column);
  if (equate_columns)
    free(equate_column);

  /*#ifdef SOLARIS */
  if (!SDDS_Terminate(&SDDS_output) || !SDDS_Terminate(&SDDS_1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  for (z = 0; z < referfiles; z++) {
    if (!SDDS_Terminate(&SDDS_ref[z])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
  free(SDDS_ref);
  /*#endif */
  if (tmpfile_used && !replaceFileAndBackUp(input1, output))
    exit(EXIT_FAILURE);
  free(input1);
  free(output);
  return EXIT_SUCCESS;
}

long expandTransferRequests(char ***match, int32_t *matches, long type,
                            TRANSFER_DEFINITION *transfer, long transfers, SDDS_DATASET *inSet) {
  long i, first;
  int32_t (*matchRoutine)(SDDS_DATASET *SDDS_dataset, char ***nameReturn, int32_t matchMode, int32_t typeMode, ...);

  *matches = 0;
  *match = NULL;
  if (!transfers)
    return 1;
  switch (type) {
  case PARAMETER_TRANSFER:
    matchRoutine = SDDS_MatchParameters;
    break;
  case ARRAY_TRANSFER:
    matchRoutine = SDDS_MatchArrays;
    break;
  default:
    SDDS_Bomb("invalid transfer type--this shouldn't happen");
    exit(EXIT_FAILURE);
    break;
  }
  first = 0;
  for (i = 0; i < transfers; i++) {
    if (transfer[i].type == type) {
      if ((*matches = (*matchRoutine)(inSet, match, SDDS_MATCH_STRING, FIND_ANY_TYPE, transfer[i].name, SDDS_OR | (first ? SDDS_0_PREVIOUS : 0))) == -1) {
        return 0;
      }
      first = 0;
    }
  }
  return 1;
}

void add_newnames(SDDS_DATASET *SDDS_dataset, REFDATA *new_data, REFDATA rename_data,
                  EDIT_NAME_REQUEST *edit_column_request, long edit_column_requests,
                  EDIT_NAME_REQUEST *edit_parameter_request, long edit_parameter_requests,
                  EDIT_NAME_REQUEST *edit_array_request, long edit_array_requests, long filenumber) {
  long i, k = 0, *orig_columnflags;
  int32_t columns, parameters, arrays;
  long *orig_parameterflags, *orig_arrayflags;
  char **column_names, **parameter_names, **array_names, **new_names;

  columns = parameters = arrays = 0;
  column_names = parameter_names = array_names = new_names = NULL;
  orig_columnflags = orig_parameterflags = orig_arrayflags = NULL;
  new_data->columns = new_data->parameters = new_data->arrays = 0;
  new_data->new_column = new_data->orig_column = NULL;
  new_data->new_parameter = new_data->orig_parameter = NULL;
  new_data->new_array = new_data->orig_array = NULL;

  /* No edit requests at all */
  if (!edit_column_requests && !edit_parameter_requests && !edit_array_requests &&
      !rename_data.columns && !rename_data.parameters && !rename_data.arrays)
    return;

  /* Transfer renames to new_data */
  (*new_data).columns = rename_data.columns;
  (*new_data).parameters = rename_data.parameters;
  (*new_data).arrays = rename_data.arrays;
  if (rename_data.columns) {
    (*new_data).new_column = (char **)malloc(sizeof(char *) * rename_data.columns);
    (*new_data).orig_column = (char **)malloc(sizeof(char *) * rename_data.columns);
    for (i = 0; i < rename_data.columns; i++) {
      SDDS_CopyString(&(*new_data).new_column[i], rename_data.new_column[i]);
      SDDS_CopyString(&(*new_data).orig_column[i], rename_data.orig_column[i]);
    }
  }
  if (rename_data.parameters) {
    (*new_data).new_parameter = (char **)malloc(sizeof(char *) * rename_data.parameters);
    (*new_data).orig_parameter = (char **)malloc(sizeof(char *) * rename_data.parameters);
    for (i = 0; i < rename_data.parameters; i++) {
      SDDS_CopyString(&(*new_data).new_parameter[i], rename_data.new_parameter[i]);
      SDDS_CopyString(&(*new_data).orig_parameter[i], rename_data.orig_parameter[i]);
    }
  }
  if (rename_data.arrays) {
    (*new_data).new_array = (char **)malloc(sizeof(char *) * rename_data.arrays);
    (*new_data).orig_array = (char **)malloc(sizeof(char *) * rename_data.arrays);
    for (i = 0; i < rename_data.arrays; i++) {
      SDDS_CopyString(&(*new_data).new_array[i], rename_data.new_array[i]);
      SDDS_CopyString(&(*new_data).orig_array[i], rename_data.orig_array[i]);
    }
  }

  if (!(column_names = SDDS_GetColumnNames(SDDS_dataset, &columns))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!(parameter_names = SDDS_GetParameterNames(SDDS_dataset, &parameters))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (!(array_names = SDDS_GetArrayNames(SDDS_dataset, &arrays))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  /* Process edit names */
  if (edit_column_requests) {
    if ((new_names = process_editnames(column_names, &orig_columnflags, columns, edit_column_request, edit_column_requests, filenumber))) {
      for (i = 0; i < columns; i++) {
        if (orig_columnflags[i]) {
          k = (*new_data).columns;
          (*new_data).new_column = trealloc((*new_data).new_column, sizeof(char *) * (k + 1));
          (*new_data).orig_column = trealloc((*new_data).orig_column, sizeof(char *) * (k + 1));
          SDDS_CopyString(&(*new_data).new_column[k], new_names[i]);
          SDDS_CopyString(&(*new_data).orig_column[k], column_names[i]);
          (*new_data).columns++;
        }
        free(new_names[i]);
      }
      free(new_names);
    }
  }

  if (edit_parameter_requests) {
    if ((new_names = process_editnames(parameter_names, &orig_parameterflags, parameters, edit_parameter_request, edit_parameter_requests, filenumber))) {
      for (i = 0; i < parameters; i++) {
        if (orig_parameterflags[i]) {
          k = (*new_data).parameters;
          (*new_data).new_parameter = trealloc((*new_data).new_parameter, sizeof(char *) * (k + 1));
          (*new_data).orig_parameter = trealloc((*new_data).orig_parameter, sizeof(char *) * (k + 1));
          SDDS_CopyString(&(*new_data).new_parameter[k], new_names[i]);
          SDDS_CopyString(&(*new_data).orig_parameter[k], parameter_names[i]);
          (*new_data).parameters++;
        }
        free(new_names[i]);
      }
      free(new_names);
    }
  }

  if (edit_array_requests) {
    if ((new_names = process_editnames(array_names, &orig_arrayflags, arrays, edit_array_request, edit_array_requests, filenumber))) {
      for (i = 0; i < arrays; i++) {
        if (orig_arrayflags[i]) {
          k = (*new_data).arrays;
          (*new_data).new_array = trealloc((*new_data).new_array, sizeof(char *) * (k + 1));
          (*new_data).orig_array = trealloc((*new_data).orig_array, sizeof(char *) * (k + 1));
          SDDS_CopyString(&(*new_data).new_array[k], new_names[i]);
          SDDS_CopyString(&(*new_data).orig_array[k], array_names[i]);
          (*new_data).arrays++;
        }
        free(new_names[i]);
      }
      free(new_names);
    }
  }

  if (orig_columnflags)
    free(orig_columnflags);
  if (orig_parameterflags)
    free(orig_parameterflags);
  if (orig_arrayflags)
    free(orig_arrayflags);
  for (i = 0; i < columns; i++)
    free(column_names[i]);
  free(column_names);
  for (i = 0; i < parameters; i++)
    free(parameter_names[i]);
  free(parameter_names);
  for (i = 0; i < arrays; i++)
    free(array_names[i]);
  free(array_names);
}

char **process_editnames(char **orig_name, long **orig_flags, long orig_names, EDIT_NAME_REQUEST *edit_request,
                         long edit_requests, long filenumber) {
  long i, j, i1, i2 = 0, k;
  char **new_name, s[1024], tmpstr[1024];
  char *ptr, **editstr, *pch;
  char edit_buffer[1024];

  *orig_flags = NULL;

  *orig_flags = tmalloc(sizeof(**orig_flags) * orig_names);
  new_name = tmalloc(sizeof(*new_name) * orig_names);

  editstr = (char **)malloc(sizeof(*editstr) * edit_requests);
  ptr = malloc(sizeof(char) * 256);
  sprintf(s, "%ld", filenumber);

  for (i = 0; i < edit_requests; i++) {
    SDDS_CopyString(&editstr[i], edit_request[i].edit_string);
    if (strstr(editstr[i], "%%ld"))
      replace_string(ptr, editstr[i], "%%ld", "%ld");
    else if (strstr(editstr[i], "%ld")) {
      sprintf(s, "%ld", filenumber);
      replace_string(ptr, editstr[i], "%ld", s);
    } else if (wild_match(editstr[i], "*%*ld*")) {
      /* Find the format of %*ld */
      /* Find position of '%' */
      pch = strchr(editstr[i], '%');
      i1 = pch - editstr[i];
      /* Find the position of 'd' after '%' */
      for (k = 0; k < strlen(editstr[i]); k++) {
        if (editstr[i][k] == 'd') {
          i2 = k;
          if (i2 > i1)
            break;
        }
      }
      strncpy(tmpstr, pch, i2 - i1 + 1);
      tmpstr[i2 - i1 + 1] = '\0';
      sprintf(s, tmpstr, filenumber);
      replace_string(ptr, editstr[i], tmpstr, s);
    } else
      continue;
    free(editstr[i]);
    SDDS_CopyString(&editstr[i], ptr);
  }
  free(ptr);
  ptr = NULL;
  for (j = 0; j < orig_names; j++) {
    (*orig_flags)[j] = 0;
    SDDS_CopyString(new_name + j, orig_name[j]);
    for (i = 0; i < edit_requests; i++) {
      ptr = expand_ranges(edit_request[i].match_string);
      free(edit_request[i].match_string);
      edit_request[i].match_string = ptr;
      if (wild_match(new_name[j], edit_request[i].match_string)) {
        strcpy(edit_buffer, new_name[j]);
        if (!edit_string(edit_buffer, editstr[i]))
          SDDS_Bomb("error editing name");
        free(new_name[j]);
        SDDS_CopyString(&new_name[j], edit_buffer);
        (*orig_flags)[j] = 1;
      }
    }
  }

  for (i = 0; i < edit_requests; i++)
    free(editstr[i]);
  free(editstr);
  return new_name;
}

long CopyRowToNewColumn(SDDS_DATASET *target, int64_t target_row, SDDS_DATASET *source, int64_t source_row,
                        REFDATA new_data, long columns, char *input2) {
  long i, j, k, type, size;
  char s[1024];

  if (!columns)
    return 1;

  for (i = 0; i < columns; i++) {
    if ((j = SDDS_GetColumnIndex(source, new_data.orig_column[i])) < 0) {
      sprintf(s, "error: column %s not found in file %s\n", new_data.orig_column[i], input2);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      continue;
    }
    if ((k = SDDS_GetColumnIndex(target, new_data.new_column[i])) < 0) {
      sprintf(s, "error: column %s not defined in output\n", new_data.new_column[i]);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      continue;
    }

    if ((type = SDDS_GetColumnType(target, k)) == SDDS_STRING) {
      if (!SDDS_CopyString(((char ***)target->data)[k] + target_row, ((char ***)source->data)[j][source_row])) {
        SDDS_SetError("Unable to copy row--string copy failed (SDDS_CopyRow)");
        return (0);
      }
    } else {
      size = SDDS_type_size[type - 1];
      memcpy((char *)target->data[k] + size * target_row, (char *)source->data[j] + size * source_row, size);
    }
  }
  return (1);
}

long CopyParametersFromSecondInput(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, REFDATA new_data) {
  long i, j, k;
  char s[1024];

  if (new_data.parameters == 0)
    return 1;
  if (new_data.parameters) {
    for (i = 0; i < new_data.parameters; i++) {
      if ((j = SDDS_GetParameterIndex(SDDS_source, new_data.orig_parameter[i])) < 0) {
        continue;
      }

      if ((k = SDDS_GetParameterIndex(SDDS_target, new_data.new_parameter[i])) < 0) {
        fprintf(stderr, "Warning, parameter %s not defined in output.\n", new_data.new_parameter[i]);
        continue;
      }
      if (!SDDS_SetParameters(SDDS_target, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, k, SDDS_source->parameter[j], -1)) {
        sprintf(s, "Unable to copy parameters for parameter %s", new_data.new_parameter[i]);
        SDDS_SetError(s);
        return (0);
      }
    }
  }
  return 1;
}

long CopyArraysFromSecondInput(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, REFDATA new_data) {
  long i, j, k, m;
  char s[1024];

  if (new_data.arrays == 0)
    return 1;
  for (i = 0; i < new_data.arrays; i++) {
    if ((j = SDDS_GetArrayIndex(SDDS_source, new_data.orig_array[i])) < 0)
      continue;
    if ((k = SDDS_GetArrayIndex(SDDS_target, new_data.new_array[i])) < 0) {
      sprintf(s, "Warning, array %s not defined in output.\n", new_data.new_array[i]);
      SDDS_SetError(s);
      continue;
    }
    if (SDDS_source->layout.array_definition[j].type != SDDS_target->layout.array_definition[k].type) {
      SDDS_SetError("Can't copy arrays between different types (SDDS_CopyArrays)");
      return 0;
    }
    SDDS_target->array[k].definition = SDDS_target->layout.array_definition + k;
    SDDS_target->array[k].elements = SDDS_source->array[j].elements;
    if (!(SDDS_target->array[k].dimension = (int32_t *)SDDS_Malloc(sizeof(*SDDS_target->array[k].dimension) * SDDS_target->array[k].definition->dimensions)) ||
        !(SDDS_target->array[k].data = SDDS_Realloc(SDDS_target->array[k].data, SDDS_type_size[SDDS_target->array[k].definition->type - 1] * SDDS_target->array[k].elements))) {
      SDDS_SetError("Unable to copy arrays--allocation failure (SDDS_CopyArrays)");
      return (0);
    }
    for (m = 0; m < SDDS_target->array[k].definition->dimensions; m++)
      SDDS_target->array[k].dimension[m] = SDDS_source->array[j].dimension[m];

    if (SDDS_target->array[k].definition->type != SDDS_STRING)
      memcpy(SDDS_target->array[k].data, SDDS_source->array[j].data, SDDS_type_size[SDDS_target->array[k].definition->type - 1] * SDDS_target->array[k].elements);
    else if (!SDDS_CopyStringArray(SDDS_target->array[k].data, SDDS_source->array[j].data, SDDS_target->array[k].elements)) {
      SDDS_SetError("Unable to copy arrays (SDDS_CopyArrays)");
      return (0);
    }
  }
  return 1;
}
