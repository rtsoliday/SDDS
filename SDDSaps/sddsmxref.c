/**
 * @file sddsmxref.c
 * @brief Merges two SDDS data sets by adding data from the second set to the first based on matching or filtering column data.
 *
 * @details
 * This program merges data from two SDDS files. It transfers parameters, columns, and arrays from the second input
 * file to the first based on user-specified criteria for matching, filtering, and name transformations. The resulting
 * data set can be output to a new file or overwrite the first input file.
 *
 * @section Usage
 * ```
 * sddsmxref [<input1>] <input2> [<output>]
 *           [-pipe[=input][,output]]
 *           [-ifis={column|parameter|array},<name>[,...]]
 *           [-ifnot={column|parameter|array},<name>[,...]]
 *           [-transfer={parameter|array},<name>[,...]]
 *           [-take=<column-name>[,...]]
 *           [-leave=<column-name>[,...]]
 *           [-fillIn]
 *           [-match=<column-name>[=<column-name>][,...]]
 *           [-equate=<column-name>[=<column-name>][,<tol>][,...]]
 *           [-reuse[=[rows][,page]]]
 *           [-rename={column|parameter|array},<old>=<new>[,...]]
 *           [-editnames={column|parameter|array},<wild>,<edit>]
 *           [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Option                                | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard input and/or output instead of files.                                    |
 * | `-ifis`                               | Specifies items that must exist in `<input1>`.                                       |
 * | `-ifnot`                              | Specifies items that must not exist in `<input1>`.                                   |
 * | `-transfer`                           | Specifies parameters or arrays to transfer from `<input2>`.                         |
 * | `-take`                               | Specifies columns to take from `<input2>`.                                           |
 * | `-leave`                              | Specifies columns to exclude from `<input2>` (overrides `-take` for overlapping columns). |
 * | `-fillIn`                             | Fills in NULL or 0 for unmatched rows.                                               |
 * | `-match`                              | Matches rows between `<input1>` and `<input2>` based on column values.               |
 * | `-equate`                             | Equates rows with specified tolerances.                                             |
 * | `-reuse`                              | Allows reuse of rows or pages from `<input2>`.                                       |
 * | `-rename`                             | Renames items in the output data set.                                              |
 * | `-editnames`                          | Edits names matching the wildcard string.                                          |
 * | `-majorOrder`                         | Specifies the output's major order.                                                  |
 *
 * @subsection Incompatibilities
 * - `-take` is incompatible with `-leave` for overlapping column names.
 * - `-reuse=page` cannot be used if `-match` or `-equate` is not specified.
 *
 * @subsection spec_req Specific Requirements
 * - For `-match`, the matching column(s) must exist in both `<input1>` and `<input2>`.
 * - For `-equate`, equated columns must be numeric in both inputs.
 * - For `-transfer`, specified parameters or arrays must exist in `<input2>`.
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
#include "SDDSaps.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_TAKE_COLUMNS,
  SET_LEAVE_COLUMNS,
  SET_MATCH_COLUMNS,
  SET_EQUATE_COLUMNS,
  SET_TRANSFER,
  SET_REUSE,
  SET_IFNOT,
  SET_NOWARNINGS,
  SET_IFIS,
  SET_PIPE,
  SET_FILLIN,
  SET_RENAME,
  SET_EDIT,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "take",
  "leave",
  "match",
  "equate",
  "transfer",
  "reuse",
  "ifnot",
  "nowarnings",
  "ifis",
  "pipe",
  "fillin",
  "rename",
  "editnames",
  "majorOrder",
};

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
  long parameters;
  long arrays;
} REFDATA;

/* Structure for getting edit names */
typedef struct
{
  char *match_string, *edit_string;
} EDIT_NAME_REQUEST;

#define COLUMN_MODE 0
#define PARAMETER_MODE 1
#define ARRAY_MODE 2
#define MODES 3
static char *mode_name[MODES] = {
  "column",
  "parameter",
  "array",
};

long expandTransferRequests(char ***match, long *matches, long type, TRANSFER_DEFINITION *transfer, long transfers, SDDS_DATASET *inSet);
void process_newnames(SDDS_DATASET *SDDS_dataset, REFDATA *take_RefData, REFDATA rename_data,
                      EDIT_NAME_REQUEST *edit_column_request, long edit_column_requests,
                      EDIT_NAME_REQUEST *edit_parameter_request, long edit_parameter_requests,
                      EDIT_NAME_REQUEST *edit_array_request, long edit_array_requests);
char **process_editnames(char **orig_name, long **orig_flags, long orig_names, EDIT_NAME_REQUEST *edit_request, long edit_requests);
long CopyRowToNewColumn(SDDS_DATASET *target, int64_t target_row, SDDS_DATASET *source, int64_t source_row,
                        REFDATA new_data, long columns, char *input2);
long CopyArraysFromSecondInput(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, REFDATA new_data);
long CopyParametersFromSecondInput(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, REFDATA new_data);
void free_refdata(REFDATA *refData, long rename);
void free_edit_request(EDIT_NAME_REQUEST *edit_request, long edit_requests);

typedef char *STRING_PAIR[2];

long rows_equate(SDDS_DATASET *SDDS1, int64_t row1, SDDS_DATASET *SDDS2, int64_t row2, long equate_columns, STRING_PAIR *equate_column,
                 double *equate_tolerance);

char *USAGE = "Usage:\n"
  "  sddsmxref [<input1>] <input2> [<output>] [options]\n"
  "            [-pipe[=input][,output]]\n"
  "            [-ifis={column|parameter|array},<name>[,...]]\n"
  "            [-ifnot={column|parameter|array},<name>[,...]]\n"
  "            [-transfer={parameter|array},<name>[,...]]\n"
  "            [-take=<column-name>[,...]]\n"
  "            [-leave=<column-name>[,...]]\n"
  "            [-fillIn]\n"
  "            [-match=<column-name>[=<column-name>][,...]]\n"
  "            [-equate=<column-name>[=<column-name>][,<tol>][,...]]\n"
  "            [-reuse[=[rows][,page]]]\n"
  "            [-rename={column|parameter|array},<old>=<new>[,...]]\n"
  "            [-editnames={column|parameter|array},<wild>,<edit>]\n"
  "            [-majorOrder=row|column]\n"
  "Options:\n"
  "  -pipe[=input][,output]                              Use standard input and/or output instead of files.\n"
  "  -ifis={column|parameter|array},<name>[,...]         Specify names that must exist in <input1>.\n"
  "  -ifnot={column|parameter|array},<name>[,...]        Specify names that must not exist in <input1>.\n"
  "  -transfer={parameter|array},<name>[,...]            Specify parameters or arrays to transfer from <input2>.\n"
  "  -take=<column-name>[,...]                            Specify columns to take from <input2>.\n"
  "  -leave=<column-name>[,...]                           Specify columns not to take from <input2>.\n"
  "                                                       Overrides -take if both specify the same column.\n"
  "                                                       Use -leave=* to exclude all columns.\n"
  "  -fillIn                                              Fill in NULL and 0 values for unmatched rows.\n"
  "  -match=<column-name>[=<column-name>][,...]           Specify columns to match between <input1> and <input2>.\n"
  "  -equate=<column-name>[=<column-name>][,<tol>][,...] Specify columns to equate with an optional tolerance.\n"
  "  -reuse[=[rows][,page]]                               Allow reuse of rows from <input2>.\n"
  "  -rename={column|parameter|array},<old>=<new>[,...]   Rename entities in the output data set.\n"
  "  -editnames={column|parameter|array},<wild>,<edit>    Edit names of entities matching the wildcard.\n"
  "  -majorOrder=row|column                               Specify output major order.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_1, SDDS_2, SDDS_output;
  long i_arg, reuse, reusePage;
  int64_t i, j, k, rows1, rows2, n;
  SCANNED_ARG *s_arg;
  char s[200], *ptr;
  char **take_column, **leave_column, **output_column;
  STRING_PAIR *match_column, *equate_column;
  double *equate_tolerance;
  long take_columns, leave_columns, match_columns, equate_columns, leave_all_columns;
  int32_t output_columns;
  char *input1, *input2, *output, *match_value;
  long tmpfile_used, retval1, retval2;
  long *row_used;
  TRANSFER_DEFINITION *transfer;
  long transfers;
  long warnings, fillIn;
  IFITEM_LIST ifnot_item, ifis_item;
  unsigned long pipeFlags, majorOrderFlag;
  REFDATA rename_data, take_RefData;
  EDIT_NAME_REQUEST *edit_column_request, *edit_parameter_request, *edit_array_request;
  long edit_column_requests, edit_parameter_requests, edit_array_requests;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s\n", USAGE);
    exit(EXIT_FAILURE);
  }

  input1 = input2 = output = NULL;
  take_column = leave_column = NULL;
  match_column = equate_column = NULL;
  equate_tolerance = NULL;
  take_columns = leave_columns = match_columns = equate_columns = reuse = reusePage = 0;
  tmpfile_used = 0;
  transfer = NULL;
  transfers = 0;
  ifnot_item.items = ifis_item.items = 0;
  warnings = 1;
  pipeFlags = 0;
  fillIn = 0;
  leave_all_columns = 0;

  rename_data.columns = rename_data.parameters = rename_data.arrays = 0;
  rename_data.new_column = rename_data.orig_column = rename_data.new_parameter = rename_data.orig_parameter = rename_data.new_array = rename_data.orig_array = NULL;
  edit_column_request = edit_parameter_request = edit_array_request = NULL;
  edit_column_requests = edit_parameter_requests = edit_array_requests = 0;
  take_RefData.columns = take_RefData.parameters = take_RefData.arrays = 0;
  take_RefData.orig_column = take_RefData.new_column = take_RefData.orig_parameter = take_RefData.new_parameter = take_RefData.orig_array = take_RefData.new_array = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_LEAVE_COLUMNS:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "%s\n", USAGE);
          exit(EXIT_FAILURE);
        }
        leave_column = trealloc(leave_column, sizeof(*leave_column) * (leave_columns + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          leave_column[i - 1 + leave_columns] = s_arg[i_arg].list[i];
        leave_columns += s_arg[i_arg].n_items - 1;
        break;
      case SET_TAKE_COLUMNS:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "%s\n", USAGE);
          exit(EXIT_FAILURE);
        }
        take_column = trealloc(take_column, sizeof(*take_column) * (take_columns + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          take_column[i - 1 + take_columns] = s_arg[i_arg].list[i];
        take_columns += s_arg[i_arg].n_items - 1;
        break;
      case SET_MATCH_COLUMNS:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "%s\n", USAGE);
          exit(EXIT_FAILURE);
        }
        match_column = trealloc(match_column, sizeof(*match_column) * (match_columns + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++) {
          if ((ptr = strchr(s_arg[i_arg].list[i], '=')))
            *ptr++ = 0;
          else
            ptr = s_arg[i_arg].list[i];
          match_column[i - 1 + match_columns][0] = s_arg[i_arg].list[i];
          match_column[i - 1 + match_columns][1] = ptr;
        }
        match_columns += s_arg[i_arg].n_items - 1;
        break;
      case SET_EQUATE_COLUMNS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -equate syntax");
        equate_column = trealloc(equate_column, sizeof(*equate_column) * (equate_columns + s_arg[i_arg].n_items - 1));
        equate_tolerance = trealloc(equate_tolerance, sizeof(*equate_tolerance) * (equate_columns + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++) {
          if (!tokenIsNumber(s_arg[i_arg].list[i])) {
            if ((ptr = strchr(s_arg[i_arg].list[i], '=')))
              *ptr++ = 0;
            else
              ptr = s_arg[i_arg].list[i];
            equate_column[equate_columns][0] = s_arg[i_arg].list[i];
            equate_column[equate_columns][1] = ptr;
            equate_tolerance[equate_columns] = 0;
            equate_columns += 1;
          } else {
            sscanf(s_arg[i_arg].list[i], "%le", &equate_tolerance[equate_columns - 1]);
          }
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
      case SET_EDIT:
        if (s_arg[i_arg].n_items != 4)
          SDDS_Bomb("invalid -editnames syntax");
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case COLUMN_MODE:
          edit_column_request = trealloc(edit_column_request, sizeof(*edit_column_request) * (edit_column_requests + 1));
          SDDS_CopyString(&edit_column_request[edit_column_requests].match_string, s_arg[i_arg].list[2]);
          SDDS_CopyString(&edit_column_request[edit_column_requests].edit_string, s_arg[i_arg].list[3]);
          edit_column_requests++;
          break;
        case PARAMETER_MODE:
          edit_parameter_request = trealloc(edit_parameter_request, sizeof(*edit_parameter_request) * (edit_parameter_requests + 1));
          SDDS_CopyString(&edit_parameter_request[edit_parameter_requests].match_string, s_arg[i_arg].list[2]);
          SDDS_CopyString(&edit_parameter_request[edit_parameter_requests].edit_string, s_arg[i_arg].list[3]);
          edit_parameter_requests++;
          break;
        case ARRAY_MODE:
          edit_array_request = trealloc(edit_array_request, sizeof(*edit_array_request) * (edit_array_requests + 1));
          SDDS_CopyString(&edit_array_request[edit_array_requests].match_string, s_arg[i_arg].list[2]);
          SDDS_CopyString(&edit_array_request[edit_array_requests].edit_string, s_arg[i_arg].list[3]);
          edit_array_requests++;
          break;
        default:
          SDDS_Bomb("invalid -editnames syntax: specify column, parameter, or array keyword");
          break;
        }
        break;
      default:
        fprintf(stderr, "Error: Unknown switch: %s\n%s\n", s_arg[i_arg].list[0], USAGE);
        SDDS_Bomb(NULL);
        break;
      }
    } else {
      if (input1 == NULL)
        input1 = s_arg[i_arg].list[0];
      else if (input2 == NULL)
        input2 = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "Error: Too many filenames specified.\n%s\n", USAGE);
        SDDS_Bomb("too many filenames");
      }
    }
  }

  if (pipeFlags & USE_STDIN && input1) {
    if (output) {
      fprintf(stderr, "Error: Too many filenames specified with -pipe option.\n%s\n", USAGE);
      SDDS_Bomb("too many filenames (sddsmxref)");
    }
    output = input2;
    input2 = input1;
    input1 = NULL;
  }
  processFilenames("sddsmxref", &input1, &output, pipeFlags, !warnings, &tmpfile_used);
  if (!input2) {
    SDDS_Bomb("Second input file not specified");
    exit(EXIT_FAILURE);
  }

  if (!SDDS_InitializeInput(&SDDS_1, input1)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!check_ifitems(&SDDS_1, &ifnot_item, 0, warnings) || !check_ifitems(&SDDS_1, &ifis_item, 1, warnings))
    exit(EXIT_SUCCESS);
  if (!SDDS_InitializeInput(&SDDS_2, input2)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  /* Get parameter and array names from transfer */
  if (transfers) {
    if (!expandTransferRequests(&take_RefData.orig_parameter, &take_RefData.parameters, PARAMETER_TRANSFER, transfer, transfers, &SDDS_2) ||
        !expandTransferRequests(&take_RefData.orig_array, &take_RefData.arrays, ARRAY_TRANSFER, transfer, transfers, &SDDS_2))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (SDDS_ColumnCount(&SDDS_2)) {
    SDDS_SetColumnFlags(&SDDS_2, 1);
    if (take_columns) {
      SDDS_SetColumnFlags(&SDDS_2, 0);
      for (i = 0; i < take_columns; i++) {
        if (!has_wildcards(take_column[i]) && SDDS_GetColumnIndex(&SDDS_2, take_column[i]) < 0) {
          sprintf(s, "Error: Column '%s' not found in file '%s'", take_column[i], input2);
          SDDS_SetError(s);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetColumnsOfInterest(&SDDS_2, SDDS_MATCH_STRING, take_column[i], SDDS_OR))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    leave_all_columns = 0;
    if (leave_columns == 1 && strcmp(leave_column[0], "*") == 0)
      leave_all_columns = 1;
    else {
      if (!take_columns)
        SDDS_SetColumnFlags(&SDDS_2, 1);
      for (i = 0; i < leave_columns; i++) {
        if (!has_wildcards(leave_column[i]) &&
            SDDS_GetColumnIndex(&SDDS_2, leave_column[i]) < 0)
          continue;
        if (!SDDS_SetColumnsOfInterest(&SDDS_2, SDDS_MATCH_STRING, leave_column[i], SDDS_AND | SDDS_NEGATE_MATCH))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (leave_columns)
        free(leave_column);
      if (take_columns)
        free(take_column);

      if (!(take_RefData.orig_column = SDDS_GetColumnNames(&SDDS_2, &take_RefData.columns))) {
        SDDS_SetError("Error: No columns selected to take from input file.");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    for (i = 0; i < match_columns; i++) {
      if ((j = SDDS_GetColumnIndex(&SDDS_1, match_column[i][0])) < 0 ||
          SDDS_GetColumnType(&SDDS_1, j) != SDDS_STRING) {
        sprintf(s, "Error: Column '%s' not found or not of string type in file '%s'.", match_column[i][0], input1 ? input1 : "stdin");
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if ((j = SDDS_GetColumnIndex(&SDDS_2, match_column[i][1])) < 0 ||
          SDDS_GetColumnType(&SDDS_2, j) != SDDS_STRING) {
        sprintf(s, "Error: Column '%s' not found or not of string type in file '%s'.", match_column[i][1], input2);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    for (i = 0; i < equate_columns; i++) {
      if ((j = SDDS_GetColumnIndex(&SDDS_1, equate_column[i][0])) < 0 ||
          !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&SDDS_1, j))) {
        sprintf(s, "Error: Column '%s' not found or not of numeric type in file '%s'.", equate_column[i][0], input1 ? input1 : "stdin");
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if ((j = SDDS_GetColumnIndex(&SDDS_2, equate_column[i][1])) < 0 ||
          !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(&SDDS_2, j))) {
        sprintf(s, "Error: Column '%s' not found or not of numeric type in file '%s'.", equate_column[i][1], input2);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

  } else {
    take_columns = 0;
    take_RefData.columns = 0;
    leave_all_columns = 1;
  }
  if (!take_RefData.columns && !leave_all_columns && warnings)
    fprintf(stderr, "Warning: No columns being taken from '%s' that are not already in '%s'.\n",
            input1 ? input1 : "stdin", input2);
  if (leave_all_columns) {
    take_RefData.columns = 0;
    take_columns = 0;
  }

  if (output && (pipeFlags & USE_STDOUT))
    SDDS_Bomb("Too many filenames specified with -pipe option.");
  if (!output && !(pipeFlags & USE_STDOUT)) {
    if (warnings)
      fprintf(stderr, "Warning: Existing file '%s' will be replaced.\n", input1 ? input1 : "stdin");
    tmpfile_used = 1;
    cp_str(&output, tmpname(NULL));
  }
  if (!SDDS_InitializeCopy(&SDDS_output, &SDDS_1, output, "w")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (columnMajorOrder != -1)
    SDDS_output.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDS_output.layout.data_mode.column_major = SDDS_1.layout.data_mode.column_major;

  /* Process new names for take RefData */
  process_newnames(&SDDS_2, &take_RefData, rename_data, edit_column_request, edit_column_requests,
                   edit_parameter_request, edit_parameter_requests, edit_array_request, edit_array_requests);

  for (i = 0; i < take_RefData.columns; i++) {
    if (SDDS_GetColumnIndex(&SDDS_output, take_RefData.new_column[i]) >= 0) {
      if (warnings)
        fprintf(stderr, "Warning: Column '%s' already exists in the first input file. No data will be taken from column '%s' of the second input file.\n",
                take_RefData.new_column[i], take_RefData.orig_column[i]);
      free(take_RefData.new_column[i]);
      free(take_RefData.orig_column[i]);
      for (j = i; j < take_RefData.columns - 1; j++) {
        take_RefData.new_column[j] = take_RefData.new_column[j + 1];
        take_RefData.orig_column[j] = take_RefData.orig_column[j + 1];
      }

      take_RefData.columns -= 1;
      i--;
      if (take_RefData.columns == 0)
        break;
    } else {
      if (!SDDS_TransferColumnDefinition(&SDDS_output, &SDDS_2, take_RefData.orig_column[i], take_RefData.new_column[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!(output_column = (char **)SDDS_GetColumnNames(&SDDS_output, &output_columns)) || output_columns == 0) {
    SDDS_SetError("Error: Problem getting output column names.");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  for (i = 0; i < take_RefData.parameters; i++) {
    if (SDDS_GetParameterIndex(&SDDS_output, take_RefData.new_parameter[i]) >= 0) {
      free(take_RefData.orig_parameter[i]);
      free(take_RefData.new_parameter[i]);
      for (j = i; j < take_RefData.parameters - 1; j++)
        take_RefData.orig_parameter[j] = take_RefData.orig_parameter[j + 1];
      take_RefData.parameters -= 1;
      i--;
      if (take_RefData.parameters == 0)
        break;
    } else {
      if (!SDDS_TransferParameterDefinition(&SDDS_output, &SDDS_2, take_RefData.orig_parameter[i], take_RefData.new_parameter[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  for (i = 0; i < take_RefData.arrays; i++) {
    if (SDDS_GetArrayIndex(&SDDS_output, take_RefData.new_array[i]) < 0) {
      free(take_RefData.orig_array[i]);
      free(take_RefData.new_array[i]);
      for (j = i; j < take_RefData.arrays - 1; j++)
        take_RefData.orig_array[j] = take_RefData.orig_array[j + 1];
      take_RefData.arrays -= 1;
      i--;
      if (take_RefData.arrays == 0)
        break;
    } else {
      if (!SDDS_TransferArrayDefinition(&SDDS_output, &SDDS_2, take_RefData.orig_array[i], take_RefData.new_array[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!SDDS_WriteLayout(&SDDS_output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!take_RefData.columns && !leave_all_columns && warnings)
    fprintf(stderr, "Warning: No columns being taken from '%s' that are not already in '%s'.\n", input2, input1 ? input1 : "stdin");
  if (output_columns) {
    for (i = 0; i < output_columns; i++)
      free(output_column[i]);
    free(output_column);
  }
  row_used = NULL;
  while ((retval1 = SDDS_ReadPage(&SDDS_1)) > 0) {
    if (!reusePage) {
      if ((retval2 = SDDS_ReadPage(&SDDS_2)) <= 0) {
        fprintf(stderr, "Warning: <input2> ends before <input1>.\n");
        break;
      }
    } else {
      if (retval1 == 1 && (retval2 = SDDS_ReadPage(&SDDS_2)) <= 0)
        SDDS_Bomb("<input2> has no data");
      SDDS_SetRowFlags(&SDDS_2, 1);
    }
    if (take_RefData.columns &&
        (!SDDS_SetColumnFlags(&SDDS_2, 0) ||
         !SDDS_SetColumnsOfInterest(&SDDS_2, SDDS_NAME_ARRAY, take_RefData.columns, take_RefData.orig_column)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    rows1 = SDDS_CountRowsOfInterest(&SDDS_1);
    if ((rows2 = SDDS_CountRowsOfInterest(&SDDS_2))) {
      row_used = SDDS_Realloc(row_used, sizeof(*row_used) * rows2);
      SDDS_ZeroMemory(row_used, rows2 * sizeof(*row_used));
    }
    if (!SDDS_StartPage(&SDDS_output, rows1)) {
      SDDS_SetError("Error: Problem starting output table.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (fillIn && !SDDS_ClearPage(&SDDS_output)) {
      SDDS_SetError("Error: Problem clearing output table.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    /* Copy parameters and arrays */
    if (!CopyParametersFromSecondInput(&SDDS_output, &SDDS_2, take_RefData)) {
      SDDS_SetError("Error: Problem copying parameters from second input file.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!CopyArraysFromSecondInput(&SDDS_output, &SDDS_2, take_RefData)) {
      SDDS_SetError("Error: Problem copying arrays from second input file.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_CopyParameters(&SDDS_output, &SDDS_1) ||
        !SDDS_CopyArrays(&SDDS_output, &SDDS_1)) {
      SDDS_SetError("Error: Problem copying parameters or arrays from first input file.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    for (j = 0; j < rows1; j++) {
      if (!SDDS_CopyRowDirect(&SDDS_output, j, &SDDS_1, j)) {
        sprintf(s, "Error: Problem copying row %" PRId64 " of first data set.", j);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      SDDS_output.row_flag[j] = 1;
      if (!match_columns && !equate_columns && !leave_all_columns) {
        if (j >= rows2) {
          if (warnings)
            fprintf(stderr, "Warning: No match for row %" PRId64 " (value %s)\n", j, match_value);
          SDDS_output.row_flag[j] = 0;
          continue;
        }
        if (!CopyRowToNewColumn(&SDDS_output, j, &SDDS_2, j, take_RefData, take_RefData.columns, input2)) {
          fprintf(stderr, "Error: Failed to copy data to output.\n");
          exit(EXIT_FAILURE);
        }
        continue;
      }
      if (!leave_all_columns) {
        SDDS_SetRowFlags(&SDDS_2, 1);
        for (i = 0; i < match_columns; i++) {
          if (!SDDS_GetValue(&SDDS_1, match_column[i][0], j, &match_value)) {
            sprintf(s, "Error: Problem getting column '%s' from file '%s'.", match_column[i][0], input1 ? input1 : "stdin");
            SDDS_SetError(s);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          if (SDDS_MatchRowsOfInterest(&SDDS_2, match_column[i][1], match_value, SDDS_AND) < 0) {
            sprintf(s, "Error: Problem setting rows of interest for column '%s'.", match_column[i][1]);
            SDDS_SetError(s);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          free(match_value);
        }
        if (!(n = SDDS_CountRowsOfInterest(&SDDS_2))) {
          if (warnings)
            fprintf(stderr, "Warning: No match for row %" PRId64 "\n", j);
          SDDS_output.row_flag[j] = 0;
          continue;
        }
        i = -1; /* Counter for rows-of-interest in file 2 */
        for (k = 0; k < rows2; k++) {
          if (!SDDS_2.row_flag[k])
            continue;
          i++;
          if (!row_used[k] && rows_equate(&SDDS_1, j, &SDDS_2, k, equate_columns, equate_column, equate_tolerance)) {
            row_used[k] = reuse ? 0 : 1;
            if (!CopyRowToNewColumn(&SDDS_output, j, &SDDS_2, k, take_RefData, take_RefData.columns, input2)) {
              fprintf(stderr, "Error: Failed to copy data to output.\n");
              exit(EXIT_FAILURE);
            }
            break;
          }
        }
        if (k == rows2) {
          if (warnings)
            fprintf(stderr, "Warning: No match for row %" PRId64 "\n", j);
          if (!fillIn)
            SDDS_output.row_flag[j] = 0;
          continue;
        }
      }
    }
    if (!SDDS_WritePage(&SDDS_output)) {
      SDDS_SetError("Error: Problem writing data to output file.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_Terminate(&SDDS_1) || !SDDS_Terminate(&SDDS_2) || !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (tmpfile_used && !replaceFileAndBackUp(input1, output))
    exit(EXIT_FAILURE);
  free_scanargs(&s_arg, argc);
  if (row_used)
    free(row_used);

  free_refdata(&take_RefData, 0);
  free_refdata(&rename_data, 1);

  free_edit_request(edit_column_request, edit_column_requests);
  free_edit_request(edit_parameter_request, edit_parameter_requests);
  free_edit_request(edit_array_request, edit_array_requests);
  if (match_columns)
    free(match_column);
  if (equate_columns)
    free(equate_column);

  return EXIT_SUCCESS;
}

void free_refdata(REFDATA *refData, long rename) {
  long i;
  if (!rename) {
    for (i = 0; i < refData->columns; i++) {
      free(refData->orig_column[i]);
      free(refData->new_column[i]);
    }
    for (i = 0; i < refData->parameters; i++) {
      free(refData->orig_parameter[i]);
      free(refData->new_parameter[i]);
    }
    for (i = 0; i < refData->arrays; i++) {
      free(refData->orig_array[i]);
      free(refData->new_array[i]);
    }
  }
  if (refData->columns) {
    free(refData->orig_column);
    free(refData->new_column);
  }
  if (refData->parameters) {
    free(refData->orig_parameter);
    free(refData->new_parameter);
  }
  if (refData->arrays) {
    free(refData->orig_array);
    free(refData->new_array);
  }
}

long rows_equate(SDDS_DATASET *SDDS1, int64_t row1, SDDS_DATASET *SDDS2, int64_t row2, long equate_columns, STRING_PAIR *equate_column, double *equate_tolerance) {
  char *data1, *data2;
  long index1, index2, size, type1, type2, i;
  char s[SDDS_MAXLINE];
  index2 = 0;
  for (i = 0; i < equate_columns; i++) {
    if ((index1 = SDDS_GetColumnIndex(SDDS1, equate_column[i][0])) < 0 ||
        (index2 = SDDS_GetColumnIndex(SDDS2, equate_column[i][1])) < 0) {
      SDDS_SetError("Problem equating rows");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    type1 = SDDS1->layout.column_definition[index1].type;
    type2 = SDDS2->layout.column_definition[index2].type;
    if (equate_tolerance[i] == 0) {
      if (type1 != type2) {
        sprintf(s, "Problem equating rows--types don't match for column '%s'='%s'", equate_column[i][0], equate_column[i][1]);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      size = SDDS_GetTypeSize(type1);
      data1 = (char *)SDDS1->data[index1] + size * row1;
      data2 = (char *)SDDS2->data[index2] + size * row2;
      if (memcmp(data1, data2, size) != 0)
        return 0;
    } else {
      double d1, d2;
      SDDS_CastValue(SDDS1->data, index1, type1, SDDS_DOUBLE, &d1);
      SDDS_CastValue(SDDS2->data, index2, type2, SDDS_DOUBLE, &d2);
      if (fabs(d1 - d2) > equate_tolerance[i])
        return 0;
    }
  }
  return 1;
}

long expandTransferRequests(char ***match, long *matches, long type, TRANSFER_DEFINITION *transfer, long transfers, SDDS_DATASET *inSet) {
  long i, first;
  int32_t (*matchRoutine)(SDDS_DATASET *SDDS_dataset, char ***nameReturn, int32_t matchMode, int32_t typeMode, ...);

  *matches = 0;
  *match = NULL;
  switch (type) {
  case PARAMETER_TRANSFER:
    matchRoutine = SDDS_MatchParameters;
    break;
  case ARRAY_TRANSFER:
    matchRoutine = SDDS_MatchArrays;
    break;
  default:
    SDDS_Bomb("Invalid transfer type--this shouldn't happen");
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

void process_newnames(SDDS_DATASET *SDDS_dataset, REFDATA *take_RefData, REFDATA rename_data,
                      EDIT_NAME_REQUEST *edit_column_request, long edit_column_requests,
                      EDIT_NAME_REQUEST *edit_parameter_request, long edit_parameter_requests,
                      EDIT_NAME_REQUEST *edit_array_request, long edit_array_requests) {
  long i, k = 0, *orig_columnflags;
  long *orig_parameterflags, *orig_arrayflags;
  char **column_names, **parameter_names, **array_names, **new_names;
  int32_t columns, parameters, arrays;

  columns = parameters = arrays = 0;
  column_names = parameter_names = array_names = new_names = NULL;
  orig_columnflags = orig_parameterflags = orig_arrayflags = NULL;

  if (take_RefData->columns)
    take_RefData->new_column = (char **)malloc(sizeof(*(take_RefData->new_column)) * take_RefData->columns);
  if (take_RefData->parameters)
    take_RefData->new_parameter = (char **)malloc(sizeof(*(take_RefData->new_parameter)) * take_RefData->parameters);
  if (take_RefData->arrays)
    take_RefData->new_array = (char **)malloc(sizeof(*(take_RefData->new_array)) * take_RefData->arrays);

  /* Transfer renames to take_RefData */
  for (i = 0; i < take_RefData->columns; i++)
    if ((k = match_string(take_RefData->orig_column[i], rename_data.orig_column, rename_data.columns, EXACT_MATCH)) >= 0)
      SDDS_CopyString(&take_RefData->new_column[i], rename_data.new_column[k]);
    else
      SDDS_CopyString(&take_RefData->new_column[i], take_RefData->orig_column[i]);

  for (i = 0; i < take_RefData->parameters; i++)
    if ((k = match_string(take_RefData->orig_parameter[i], rename_data.orig_parameter, rename_data.parameters, EXACT_MATCH)) >= 0)
      SDDS_CopyString(&take_RefData->new_parameter[i], rename_data.new_parameter[k]);
    else
      SDDS_CopyString(&take_RefData->new_parameter[i], take_RefData->orig_parameter[i]);
  for (i = 0; i < take_RefData->arrays; i++)
    if ((k = match_string(take_RefData->orig_array[i], rename_data.orig_array, rename_data.arrays, EXACT_MATCH)) >= 0)
      SDDS_CopyString(&take_RefData->new_array[i], rename_data.new_array[k]);
    else
      SDDS_CopyString(&take_RefData->new_array[i], take_RefData->orig_array[i]);

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
    if ((new_names = process_editnames(column_names, &orig_columnflags, columns, edit_column_request, edit_column_requests))) {
      for (i = 0; i < columns; i++) {
        if (orig_columnflags[i]) {
          if ((k = match_string(column_names[i], take_RefData->orig_column, take_RefData->columns, EXACT_MATCH)) >= 0)
            SDDS_CopyString(&take_RefData->new_column[k], new_names[i]);
        }
        free(new_names[i]);
      }
      free(new_names);
    }
  }
  if (edit_parameter_requests) {
    if ((new_names = process_editnames(parameter_names, &orig_parameterflags, parameters, edit_parameter_request, edit_parameter_requests))) {
      for (i = 0; i < parameters; i++) {
        if (orig_parameterflags[i]) {
          if ((k = match_string(parameter_names[i], take_RefData->orig_parameter, take_RefData->parameters, EXACT_MATCH)) >= 0)
            SDDS_CopyString(&take_RefData->new_parameter[k], new_names[i]);
        }
        free(new_names[i]);
      }
      free(new_names);
    }
  }

  if (edit_array_requests) {
    if ((new_names = process_editnames(array_names, &orig_arrayflags, arrays, edit_array_request, edit_array_requests))) {
      for (i = 0; i < arrays; i++) {
        if (orig_arrayflags[i]) {
          if ((k = match_string(array_names[i], take_RefData->orig_array, take_RefData->arrays, EXACT_MATCH)) >= 0)
            SDDS_CopyString(&take_RefData->new_array[k], new_names[i]);
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

char **process_editnames(char **orig_name, long **orig_flags, long orig_names, EDIT_NAME_REQUEST *edit_request, long edit_requests) {
  long i, j;
  char **new_name, s[1024];
  char *ptr, **editstr;
  char edit_buffer[1024];

  *orig_flags = NULL;

  *orig_flags = tmalloc(sizeof(**orig_flags) * orig_names);
  new_name = tmalloc(sizeof(*new_name) * orig_names);

  editstr = (char **)malloc(sizeof(*editstr) * edit_requests);
  ptr = malloc(sizeof(char) * 256);
  sprintf(s, "%" PRId32, 2);

  for (i = 0; i < edit_requests; i++) {
    SDDS_CopyString(&editstr[i], edit_request[i].edit_string);
    if (strstr(editstr[i], "%%ld"))
      replace_string(ptr, editstr[i], "%%ld", "%ld");
    else if (strstr(editstr[i], "%ld"))
      replace_string(ptr, editstr[i], "%ld", s);
    else
      continue;
    free(editstr[i]);
    SDDS_CopyString(&editstr[i], ptr);
  }
  free(ptr);
  ptr = NULL;
  for (j = 0; j < orig_names; j++) {
    (*orig_flags)[j] = 0;
    SDDS_CopyString(&new_name[j], orig_name[j]);
    for (i = 0; i < edit_requests; i++) {
      ptr = expand_ranges(edit_request[i].match_string);
      free(edit_request[i].match_string);
      edit_request[i].match_string = ptr;
      if (wild_match(new_name[j], edit_request[i].match_string)) {
        strcpy(edit_buffer, new_name[j]);
        if (!edit_string(edit_buffer, editstr[i]))
          SDDS_Bomb("Error editing name");
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

long CopyParametersFromSecondInput(SDDS_DATASET *SDDS_target, SDDS_DATASET *SDDS_source, REFDATA new_data) {
  long i, j, k;
  char s[1024];

  if (new_data.parameters == 0)
    return 1;
  for (i = 0; i < new_data.parameters; i++) {
    if ((j = SDDS_GetParameterIndex(SDDS_source, new_data.orig_parameter[i])) < 0) {
      continue;
    }

    if ((k = SDDS_GetParameterIndex(SDDS_target, new_data.new_parameter[i])) < 0) {
      fprintf(stderr, "Warning: Parameter '%s' not defined in output.\n", new_data.new_parameter[i]);
      continue;
    }
    if (!SDDS_SetParameters(SDDS_target, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, k, SDDS_source->parameter[j], -1)) {
      sprintf(s, "Unable to copy parameters for parameter '%s'", new_data.new_parameter[i]);
      SDDS_SetError(s);
      return 0;
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
      sprintf(s, "Warning: Array '%s' not defined in output.\n", new_data.new_array[i]);
      SDDS_SetError(s);
      continue;
    }
    if (SDDS_source->layout.array_definition[j].type != SDDS_target->layout.array_definition[k].type) {
      SDDS_SetError("Error: Cannot copy arrays of different types.");
      return 0;
    }
    SDDS_target->array[k].definition = SDDS_target->layout.array_definition + k;
    SDDS_target->array[k].elements = SDDS_source->array[j].elements;
    if (!(SDDS_target->array[k].dimension = (int32_t *)SDDS_Malloc(sizeof(*SDDS_target->array[k].dimension) * SDDS_target->array[k].definition->dimensions)) ||
        !(SDDS_target->array[k].data = SDDS_Realloc(SDDS_target->array[k].data, SDDS_type_size[SDDS_target->array[k].definition->type - 1] * SDDS_target->array[k].elements))) {
      SDDS_SetError("Error: Unable to copy arrays due to memory allocation failure.");
      return 0;
    }
    for (m = 0; m < SDDS_target->array[k].definition->dimensions; m++)
      SDDS_target->array[k].dimension[m] = SDDS_source->array[j].dimension[m];

    if (SDDS_target->array[k].definition->type != SDDS_STRING)
      memcpy(SDDS_target->array[k].data, SDDS_source->array[j].data, SDDS_type_size[SDDS_target->array[k].definition->type - 1] * SDDS_target->array[k].elements);
    else if (!SDDS_CopyStringArray(SDDS_target->array[k].data, SDDS_source->array[j].data, SDDS_target->array[k].elements)) {
      SDDS_SetError("Error: Unable to copy string arrays.");
      return 0;
    }
  }
  return 1;
}

long CopyRowToNewColumn(SDDS_DATASET *target, int64_t target_row, SDDS_DATASET *source, int64_t source_row, REFDATA new_data, long columns, char *input2) {
  long i, j, k, type, size;
  char s[1024];

  if (!columns)
    return 1;

  for (i = 0; i < columns; i++) {
    if ((j = SDDS_GetColumnIndex(source, new_data.orig_column[i])) < 0) {
      sprintf(s, "Error: Column '%s' not found in file '%s'.\n", new_data.orig_column[i], input2);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      continue;
    }
    if ((k = SDDS_GetColumnIndex(target, new_data.new_column[i])) < 0) {
      sprintf(s, "Error: Column '%s' not defined in output.\n", new_data.new_column[i]);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      continue;
    }

    if ((type = SDDS_GetColumnType(target, k)) == SDDS_STRING) {
      if (!SDDS_CopyString(((char ***)target->data)[k] + target_row, ((char ***)source->data)[j][source_row])) {
        SDDS_SetError("Error: Unable to copy string data.");
        return 0;
      }
    } else {
      size = SDDS_type_size[type - 1];
      memcpy((char *)target->data[k] + size * target_row, (char *)source->data[j] + size * source_row, size);
    }
  }
  return 1;
}

void free_edit_request(EDIT_NAME_REQUEST *edit_request, long edit_requests) {
  long i;
  if (edit_requests) {
    for (i = 0; i < edit_requests; i++) {
      free(edit_request[i].match_string);
      free(edit_request[i].edit_string);
    }
    free(edit_request);
  }
}
