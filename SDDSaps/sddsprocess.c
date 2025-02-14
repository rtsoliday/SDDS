/**
 * @file sddsprocess.c
 * @brief Processes SDDS files by applying various transformations to columns and parameters.
 *
 * This program reads an input SDDS (Self Describing Data Sets) file, processes its columns and parameters
 * based on the specified options, and writes the results to a new SDDS output file. It supports a wide
 * range of operations including filtering, matching, editing, defining new parameters or columns,
 * converting units, and more.
 *
 * @section Usage
 * ```
 * sddsprocess [<SDDSinputfile>] [<SDDSoutputfile>]
 *             -pipe=[input][,output]
 *             -ifis={column|parameter},<name>[,...]
 *             -ifnot={column|parameter},<name>[,...]
 *             -match={column|parameter},<match-test>[,<match-test>[,<logic-operation>...]]
 *             -filter={column|parameter},<range-spec>[,<range-spec>[,<logic-operation>...]]
 *             -timeFilter={parameter|column},[,before=YYYY/MM/DD@HH:MM:SS][,after=YYYY/MM/DD@HH:MM:SS][,invert]
 *             -clip=<head>,<tail>,[invert]
 *             -sparse=<interval>[,<offset>]
 *             -sample=<fraction>
 *             -fclip=<head>,<tail>,[invert]
 *             -test={column|parameter},<test>[,autostop][,algebraic]
 *             -numberTest={column|parameter},<name>[,invert]
 *             -rpndefinitionsfiles=<filename>[,...]
 *             -rpnexpression=<expression>[,repeat][,algebraic]
 *             -convertunits={column|parameter},<name>,<new-units>,<old-units>[,<factor>]
 *             -define={column|parameter},<name>,{<equation>|@<parameterName>}[,<definition_entries>][,algebraic]
 *             -redefine={column|parameter},<name>,{<equation>|@<parameterName>}[,<definition_entries>][,algebraic]
 *             -cast={column|parameter},<newName>,<sourceName>,<newType>
 *             -scan={column|parameter},<new-name>,<source-name>,<sscanf-string>[,<definition-entries>][,edit=<string>]
 *             -edit={column|parameter},<new-name>,<source-name>,<edit-string>[,<definition-entries>]
 *             -reedit={column|parameter},<name>,<edit-string>[,<definition-entries>]
 *             -print={column|parameter},<new-name>,<sprintf-string>,<source-name>[,...][,<definition-entries>]
 *             -reprint={column|parameter},<new-name>,<sprintf-string>,<source-name>[,...][,<definition-entries>]
 *             -format={column|parameter},<new-name>,<source-name>[,stringFormat=<sprintfString>][,doubleFormat=<sprintfString>][,longFormat=<sprintfString>]
 *             -system={column|parameter},<new-name>,<command-name>[,<definition_entries>]
 *             -evaluate={column|parameter},<new-name>,<expression-name>[,<definition_entries>]
 *             -process=<column-name>,<analysis-name>,<result-name>[,description=<string>][,symbol=<string>][,weightBy=<column-name>][,functionOf=<column-name>[,lowerLimit=<value>][,upperLimit=<value>][,position]][,head=<number>][,tail=<number>][fhead=<fraction>][ftail=<fraction>][,topLimit=<value>][,bottomLimit=<value>][,offset=<value>][,factor=<value>][match=<column-name>,value=<string>][,overwrite],[default=<value>]
 *             -summarize
 *             -verbose
 *             -nowarnings
 *             -delete={column|parameter|array},<matching-string>[,...]
 *             -retain={column|parameter|array},<matching-string>[,...]
 *             -description=[text=<string>][,contents=<string>]
 *             -majorOrder=row|column
 *             -threads=<number>
 * ```
 *
 * @section Options
 * | Optional                            | Description                                                                      |
 * |-------------------------------------|----------------------------------------------------------------------------------|
 * | `-pipe`                             | Directs input/output via pipes.                                                  |
 * | `-ifis`                             | Include only the specified columns or parameters.                                |
 * | `-ifnot`                            | Exclude the specified columns or parameters.                                     |
 * | `-match`                            | Filter data based on matching criteria for columns or parameters.                |
 * | `-filter`                           | Apply range-based filtering on specified columns or parameters.                  |
 * | `-timeFilter`                       | Filter data based on time constraints.                                           |
 * | `-clip`                             | Clip the dataset by removing a specified number of rows from the head and tail.  |
 * | `-sparse`                           | Retain every `<interval>`-th row starting from `<offset>`.                       |
 * | `-sample`                           | Randomly sample rows based on the specified fraction.                            |
 * | `-fclip`                            | Fractional clipping based on the fraction of rows.                               |
 * | `-test`                             | Apply RPN-based tests on columns or parameters with optional autostop.           |
 * | `-numberTest`                       | Perform numerical tests on specified columns or parameters.                      |
 * | `-rpndefinitionsfiles`              | Specify files containing RPN definitions.                                        |
 * | `-rpnexpression`                    | Define RPN expressions to be evaluated.                                          |
 * | `-convertunits`                     | Convert units of a specified column or parameter.                                |
 * | `-define`                           | Define new columns or parameters based on equations or existing parameters.      |
 * | `-redefine`                         | Redefine existing columns or parameters.                                         |
 * | `-cast`                             | Cast a column or parameter to a new data type.                                   |
 * | `-scan`                             | Scan and parse data from a source column or parameter.                           |
 * | `-edit`                             | Edit values of a column or parameter based on specified rules.                   |
 * | `-reedit`                           | Re-edit existing columns or parameters.                                          |
 * | `-print`                            | Print formatted data from a source to a new column or parameter.                 |
 * | `-reprint`                          | Reprint existing columns or parameters with new formatting.                      |
 * | `-format`                           | Apply formatting to columns or parameters.                                       |
 * | `-system`                           | Execute system commands and capture their output.                                |
 * | `-evaluate`                         | Evaluate expressions to compute new columns or parameters.                       |
 * | `-process`                          | Perform various processing operations on specified columns.                      |
 * | `-summarize`                        | Display a summary of the processing operations performed.                        |
 * | `-verbose`                          | Enable verbose output for detailed processing information.                       |
 * | `-nowarnings`                       | Suppress warning messages.                                                       |
 * | `-delete`                           | Delete specified columns, parameters, or arrays.                                 |
 * | `-retain`                           | Retain only the specified columns, parameters, or arrays.                        |
 * | `-description`                      | Add or modify the description of the output SDDS file.                           |
 * | `-majorOrder`                       | Set the major order of the output data (row-major or column-major).              |
 * | `-threads`                          | Specify the number of threads to use for processing.                             |
 *
 * @section processingModes Processing Modes
 * | Modes          |               |              |                     |
 * |----------------|---------------|--------------|---------------------|
 * | average        | rms           | sum          | standarddeviation   |
 * | mad            | minimum       | maximum      | smallest            |
 * | largest        | first         | last         | count               |
 * | spread         | median        | baselevel    | toplevel            |
 * | amplitude      | risetime      | falltime     | fwhm                |
 * | fwtm           | center        | zerocrossing | fwha                |
 * | fwta           | sigma         | slope        | intercept           |
 * | lfsd           | qrange        | drange       | percentile          |
 * | mode           | integral      | product      | prange              |
 * | signedsmallest | signedlargest | gmintegral   | correlation         |
 *
 * @subsection additonalInformation Additional Information
 * - `<match-test>` is of the form `<name>=<matching-string>[,!]`, where ! signifies logical negation.
 * - `<logic-operation>` is one of & (logical and) or | (logical or), optionally followed by a ! to logically negate the value of the expression.
 * - `<range-spec>` is of the form `<name>,<lower-value>,<upper-value>[,!]`.
 * - `<test>` is an RPN expression leaving a value on the logic stack.
 * - `<definition-entry>` is of the form `<entry-name>=<value>`, where `<entry-name>` is one of "symbol", "units", "description", "format_string", and "type".
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
#include "scan.h"
#include "SDDS.h"
#include "SDDSaps.h"
#include "rpn.h"
#include <sys/types.h>
#if !defined(_WIN32)
#  include <sys/time.h>
#endif

#define COLUMN_MODE 0
#define PARAMETER_MODE 1
#define ARRAY_MODE 2
#define MODES 3
 static char *mode_name[MODES] = {
   "column",
   "parameter",
   "array",
 };

/* Enumeration for option types */
enum option_type {
  SET_PROCESS,
  SET_MATCH,
  SET_FILTER,
  SET_RPN_DEFNS_FILES,
  SET_RPN_EXPRESSION,
  SET_DEFINITION,
  SET_TEST,
  SET_SUMMARIZE,
  SET_VERBOSE,
  SET_CONVERT_UNITS,
  SET_SCAN,
  SET_PRINT,
  SET_EDIT,
  SET_SYSTEM,
  SET_REDEFINITION,
  SET_NOWARNINGS,
  SET_IFNOT,
  SET_IFIS,
  SET_CLIP,
  SET_SPARSE,
  SET_SAMPLE,
  SET_PIPE,
  SET_NUMBERTEST,
  SET_REEDIT,
  SET_FORMAT,
  SET_CAST,
  SET_REPRINT,
  SET_DELETE,
  SET_RETAIN,
  SET_DESCRIPTION,
  SET_FCLIP,
  SET_TIME_FILTER,
  SET_MAJOR_ORDER,
  SET_EVALUATE,
  SET_THREADS,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "process",
  "match",
  "filter",
  "rpndefinitionsfiles",
  "rpnexpression",
  "define",
  "test",
  "summarize",
  "verbose",
  "convertunits",
  "scan",
  "print",
  "edit",
  "system",
  "redefine",
  "nowarnings",
  "ifnot",
  "ifis",
  "clip",
  "sparse",
  "sample",
  "pipe",
  "numbertest",
  "reedit",
  "format",
  "cast",
  "reprint",
  "delete",
  "retain",
  "description",
  "fclip",
  "timeFilter",
  "majorOrder",
  "evaluate",
  "threads",
};

char *usageArray[] = {
  "Usage:\n",
  "  sddsprocess [<SDDSinputfile>] [<SDDSoutputfile>] [OPTIONS]\n",
  "\n",
  "Options:\n",
  "  -pipe=[input][,output]\n",
  "       Use pipe mode with optional input and output streams.\n",
  "  -ifis={column|parameter},<name>[,...]\n",
  "       Include only the specified columns or parameters.\n",
  "  -ifnot={column|parameter},<name>[,...]\n",
  "       Exclude the specified columns or parameters.\n",
  "  -match={column|parameter},<match-test>[,<match-test>,...]\n",
  "       Filter data based on matching criteria for columns or parameters.\n",
  "  -filter={column|parameter},<range-spec>[,<range-spec>,...]\n",
  "       Apply range-based filtering on specified columns or parameters.\n",
  "  -timeFilter={parameter|column},[,before=YYYY/MM/DD@HH:MM:SS][,after=YYYY/MM/DD@HH:MM:SS][,invert]\n",
  "       Filter data based on time constraints.\n",
  "  -clip=<head>,<tail>,[invert]\n",
  "       Clip the dataset by removing a specified number of rows from the head and tail.\n",
  "  -sparse=<interval>[,<offset>]\n",
  "       Retain every <interval>-th row starting from <offset>.\n",
  "  -sample=<fraction>\n",
  "       Randomly sample rows based on the specified fraction.\n",
  "  -fclip=<head>,<tail>,[invert]\n",
  "       Fractional clipping based on the fraction of rows.\n",
  "  -test={column|parameter},<test>[,autostop][,algebraic]\n",
  "       Apply RPN-based tests on columns or parameters with optional autostop.\n",
  "  -numberTest={column|parameter},<name>[,invert]\n",
  "       Perform numerical tests on specified columns or parameters.\n",
  "  -rpndefinitionsfiles=<filename>[,...]\n",
  "       Specify files containing RPN definitions.\n",
  "  -rpnexpression=<expression>[,repeat][,algebraic]\n",
  "       Define RPN expressions to be evaluated.\n",
  "  -convertunits={column|parameter},<name>,<new-units>,<old-units>[,<factor>]\n",
  "       Convert units of a specified column or parameter.\n",
  "  -define={column|parameter},<name>,{<equation>|@<parameterName>}[,<definition_entries>][,algebraic]\n",
  "       Define new columns or parameters based on equations or existing parameters.\n",
  "  -redefine={column|parameter},<name>,{<equation>|@<parameterName>}[,<definition_entries>][,algebraic]\n",
  "       Redefine existing columns or parameters.\n",
  "  -cast={column|parameter},<newName>,<sourceName>,<newType>\n",
  "       Cast a column or parameter to a new data type.\n",
  "  -scan={column|parameter},<new-name>,<source-name>,<sscanf-string>[,<definition-entries>][,edit=<string>]\n",
  "       Scan and parse data from a source column or parameter.\n",
  "  -edit={column|parameter},<new-name>,<source-name>,<edit-string>[,<definition-entries>]\n",
  "       Edit values of a column or parameter based on specified rules.\n",
  "  -reedit={column|parameter},<name>,<edit-string>[,<definition-entries>]\n",
  "       Re-edit existing columns or parameters.\n",
  "  -print={column|parameter},<new-name>,<sprintf-string>,<source-name>[,...][,<definition-entries>]\n",
  "       Print formatted data from a source to a new column or parameter.\n",
  "  -reprint={column|parameter},<new-name>,<sprintf-string>,<source-name>[,...][,<definition-entries>]\n",
  "       Reprint existing columns or parameters with new formatting.\n",
  "  -format={column|parameter},<new-name>,<source-name>[,stringFormat=<sprintfString>][,doubleFormat=<sprintfString>][,longFormat=<sprintfString>]\n",
  "       Apply formatting to columns or parameters.\n",
  "  -system={column|parameter},<new-name>,<command-name>[,<definition_entries>]\n",
  "       Execute system commands and capture their output.\n",
  "  -evaluate={column|parameter},<new-name>,<expression-name>[,<definition_entries>]\n",
  "       Evaluate expressions to compute new columns or parameters.\n",
  "  -process=<column-name>,<analysis-name>,<result-name>[,description=<string>][,symbol=<string>][,weightBy=<column-name>][,functionOf=<column-name>[,lowerLimit=<value>][,upperLimit=<value>][,position]][,head=<number>][,tail=<number>][fhead=<fraction>][ftail=<fraction>][,topLimit=<value>][,bottomLimit=<value>][,offset=<value>][,factor=<value>][match=<column-name>,value=<string>][,overwrite],[default=<value>]\n",
  "       Perform various processing operations on specified columns.\n",
  "  -summarize\n",
  "       Display a summary of the processing operations performed.\n",
  "  -verbose\n",
  "       Enable verbose output for detailed processing information.\n",
  "  -nowarnings\n",
  "       Suppress warning messages.\n",
  "  -delete={column|parameter|array},<matching-string>[,...]\n",
  "       Delete specified columns, parameters, or arrays.\n",
  "  -retain={column|parameter|array},<matching-string>[,...]\n",
  "       Retain only the specified columns, parameters, or arrays.\n",
  "  -description=[text=<string>][,contents=<string>]\n",
  "       Add or modify the description of the output SDDS file.\n",
  "  -majorOrder=row|column\n",
  "       Set the major order of the output data (row-major or column-major).\n",
  "  -threads=<number>\n",
  "       Specify the number of threads to use for processing.\n",
  "\n",
  "Additional Information:\n",
  "  A <match-test> is of the form <name>=<matching-string>[,!], where ! signifies logical negation.\n",
  "  A <logic-operation> is one of & (logical and) or | (logical or), optionally followed by a ! to logically negate the value of the expression.\n",
  "  A <range-spec> is of the form <name>,<lower-value>,<upper-value>[,!].\n",
  "  A <test> is an RPN expression leaving a value on the logic stack.\n",
  "  A <definition-entry> is of the form <entry-name>=<value>, where <entry-name> is one of \"symbol\", \"units\", \"description\", \"format_string\", and \"type\".\n",
  "\n",
  "Description:\n",
  "  sddsprocess reads data from an SDDS file, processes it according to the specified options, and writes the results to a new SDDS file.\n",
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n",
  NULL};

#define PARAMETER_OR_COLUMN(is_param) ((is_param) ? "parameter" : "column")

#define REDEFINABLE_TYPE(type) (SDDS_NUMERIC_TYPE(type) || (type) == SDDS_CHARACTER)

/*
  static char *colpar_choice[2] = {"column", "parameter"};
*/
static long table_number_mem = -1, i_page_mem = -1, n_rows_mem = -1, i_row_mem = -1;

typedef char *STRING_PAIR[2];

typedef struct
{
  char *match_string, *edit_string;
} EDIT_NAME_REQUEST;

char **process_name_options(char **orig_name, long **orig_flag, long orig_names, char **delete, long deletes,
                            char **retain, long retains, STRING_PAIR *rename, long renames,
                            EDIT_NAME_REQUEST *edit_request, long edit_requests);
long SDDS_ComputeSetOfColumns(SDDS_DATASET *SDDS_dataset, long equ_begin, long equ_end, DEFINITION *definition);
void free_processing_definiton(PROCESSING_DEFINITION **processing_definition, long n);
void free_equation_definition(EQUATION_DEFINITION **equation_definition, long n);
long SDDS_EvaluateColumn(SDDS_DATASET *SDDS_dataset, EVALUATE_DEFINITION *definition);
long SDDS_EvaluateParameter(SDDS_DATASET *SDDS_dataset, EVALUATE_DEFINITION *definition);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_input, SDDS_output;
  PARAMETER_DEFINITION *pardefptr;
  //long output_columns;
  char *description_text, *description_contents;
  unsigned long flags;

  PROCESSING_DEFINITION **processing_definition, *processing_ptr;
  long processing_definitions;
  CONVERSION_DEFINITION **conversion, *conversion_ptr;
  long conversions;
  EDIT_DEFINITION **edit_definition, *edit_ptr;
  long edit_definitions;
  EQUATION_DEFINITION **equation_definition, *equation_ptr;
  long equation_definitions;
  EVALUATE_DEFINITION **evaluate_definition, *evaluate_ptr;
  long evaluate_definitions;
  SCAN_DEFINITION **scan_definition, *scan_ptr;
  long scan_definitions;
  PRINT_DEFINITION **print_definition, *print_ptr;
  long print_definitions;
  FORMAT_DEFINITION **format_definition, *format_ptr;
  long format_definitions;
  FILTER_TERM *filter_term;
  FILTER_DEFINITION **filter_definition, *filter_ptr;
  long filter_definitions;
  TIME_FILTER_DEFINITION **time_filter_definition, *time_filter_ptr;
  long time_filter_definitions;
  MATCH_TERM *match_term;
  MATCH_DEFINITION **match_definition, *match_ptr;
  long match_definitions;
  RPNTEST_DEFINITION **rpntest_definition, *rpntest_ptr;
  long rpntest_definitions;
  NUMBERTEST_DEFINITION **numbertest_definition, *numbertest_ptr;
  long numbertest_definitions;
  SYSTEM_DEFINITION **system_definition, *system_ptr;
  long system_definitions;
  RPNEXPRESSION_DEFINITION **rpnexpression_definition, *rpnexpression_ptr;
  long rpnexpression_definitions;
  CLIP_DEFINITION **clip_definition, *clip_ptr;
  FCLIP_DEFINITION **fclip_definition, *fclip_ptr;
  long clip_definitions, fclip_definitions;
  SPARSE_DEFINITION **sparse_definition, *sparse_ptr;
  long sparse_definitions;
  SAMPLE_DEFINITION **sample_definition, *sample_ptr;
  long sample_definitions;
  DEFINITION *definition;
  long definitions;
  CAST_DEFINITION **cast_definition, *cast_ptr;
  long cast_definitions;

  IFITEM_LIST ifnot_item, ifis_item;
  char *inputfile, *outputfile;

  int64_t n_rows, j;
  long i, k, i_arg, summarize, verbose, index = 0, tmpfile_used, nowarnings, udfCounter;
  long page_number, accept;
  SCANNED_ARG *s_arg;
  char s[SDDS_MAXLINE];

  char **rpn_definitions_file, *stringResult;
  long rpn_definitions_files;
  int32_t test_result;
  long original_parameters;
  double result;
  long i_page_memory, table_number_memory, n_rows_memory, row_deletion, skip_page;
  long random_number_seed;
  unsigned long pipeFlags, majorOrderFlag;

  char **retain_column, **delete_column;
  long retain_columns, delete_columns, rename_columns, edit_column_requests;
  EDIT_NAME_REQUEST *edit_column_request;
  STRING_PAIR *rename_column;
  char **orig_column_name, **new_column_name;
  long *orig_column_flag;
  int32_t orig_column_names;

  char **retain_parameter, **delete_parameter;
  long retain_parameters, delete_parameters, rename_parameters, edit_parameter_requests;
  EDIT_NAME_REQUEST *edit_parameter_request;
  STRING_PAIR *rename_parameter;
  char **orig_parameter_name, **new_parameter_name;
  long *orig_parameter_flag;
  int32_t orig_parameter_names;

  char **retain_array, **delete_array;
  long retain_arrays, delete_arrays, rename_arrays, edit_array_requests;
  EDIT_NAME_REQUEST *edit_array_request;
  STRING_PAIR *rename_array;
  char **orig_array_name, **new_array_name;
  long *orig_array_flag;
  int32_t orig_array_names;
  short columnMajorOrder = -1;

  int threads = 1;

  char buffer[SDDS_MAXLINE];

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&s_arg, argc, argv);
  argc = add_sddsfile_arguments(&s_arg, argc);

  random_number_seed = (long)time((time_t *)NULL);
  random_number_seed = 2 * (random_number_seed / 2) + 1;
#if defined(_WIN32)
  random_1(-labs((double)random_number_seed));
#else
  random_1(-FABS((double)random_number_seed));
#endif

  retain_column = delete_column = NULL;
  retain_columns = delete_columns = rename_columns = edit_column_requests = 0;
  rename_column = NULL;
  edit_column_request = NULL;

  retain_parameter = delete_parameter = NULL;
  retain_parameters = delete_parameters = rename_parameters = edit_parameter_requests = 0;
  rename_parameter = NULL;
  edit_parameter_request = NULL;

  retain_array = delete_array = NULL;
  retain_arrays = delete_arrays = rename_arrays = edit_array_requests = 0;
  rename_array = NULL;
  edit_array_request = NULL;

  description_text = description_contents = NULL;
  new_parameter_name = new_column_name = new_array_name = NULL;

  if (argc < 3) {
    char **ptr;
    ptr = usageArray;
    while (*ptr) {
      fprintf(stderr, "%s", *ptr);
      ptr++;
    }
    show_process_modes(stderr);
    exit(EXIT_FAILURE);
  }

  inputfile = outputfile = NULL;
  rpn_definitions_file = NULL;
  rpn_definitions_files = 0;
  summarize = verbose = nowarnings = udfCounter = 0;

  processing_definition = NULL;
  processing_definitions = 0;
  conversion = NULL;
  conversions = 0;
  edit_definition = NULL;
  edit_definitions = 0;
  equation_definition = NULL;
  equation_definitions = 0;
  evaluate_definition = NULL;
  evaluate_definitions = 0;
  scan_definition = NULL;
  scan_definitions = 0;
  print_definition = NULL;
  print_definitions = 0;
  format_definition = NULL;
  format_definitions = 0;
  filter_definition = NULL;
  filter_definitions = 0;
  time_filter_definition = NULL;
  time_filter_definitions = 0;
  match_definition = NULL;
  match_definitions = 0;
  rpntest_definition = NULL;
  rpntest_definitions = 0;
  numbertest_definition = NULL;
  numbertest_definitions = 0;
  system_definition = NULL;
  system_definitions = 0;
  rpnexpression_definition = NULL;
  rpnexpression_definitions = 0;
  clip_definition = NULL;
  fclip_definition = NULL;
  clip_definitions = fclip_definitions = 0;
  sparse_definition = NULL;
  sparse_definitions = 0;
  sample_definition = NULL;
  sample_definitions = 0;
  cast_definition = NULL;
  cast_definitions = 0;
  definition = NULL;
  definitions = 0;
  ifnot_item.items = 0;
  ifis_item.items = 0;
  pipeFlags = 0;

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
      case SET_MATCH:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -match syntax");
        match_definition = (MATCH_DEFINITION **)trealloc(match_definition, sizeof(*match_definition) * (match_definitions + 1));
        if (!(match_definition[match_definitions] = process_new_match_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -match syntax");
        add_definition(&definition, &definitions, (void *)match_definition[match_definitions], IS_MATCH_DEFINITION);
        match_definitions++;
        break;
      case SET_FILTER:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -filter syntax");
        filter_definition = (FILTER_DEFINITION **)trealloc(filter_definition, sizeof(*filter_definition) * (filter_definitions + 1));
        if (!(filter_definition[filter_definitions] = process_new_filter_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -filter syntax");
        add_definition(&definition, &definitions, (void *)filter_definition[filter_definitions], IS_FILTER_DEFINITION);
        filter_definitions++;
        break;
      case SET_TIME_FILTER:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -timeFilter syntax");
        time_filter_definition = (TIME_FILTER_DEFINITION **)trealloc(time_filter_definition, sizeof(*time_filter_definition) * (filter_definitions + 1));
        if (!(time_filter_definition[time_filter_definitions] = process_new_time_filter_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -timeFilter syntax");
        add_definition(&definition, &definitions, (void *)time_filter_definition[time_filter_definitions], IS_TIME_FILTER_DEFINITION);
        time_filter_definitions++;
        break;
      case SET_RPN_DEFNS_FILES:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -rpndefinitionsfiles syntax");
        rpn_definitions_file = trealloc(rpn_definitions_file, sizeof(*rpn_definitions_file) * (rpn_definitions_files + s_arg[i_arg].n_items));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          if (!fexists(rpn_definitions_file[rpn_definitions_files + i - 1] = s_arg[i_arg].list[i]))
            SDDS_Bomb("one or more rpn definitions files do not exist");
        rpn_definitions_files += s_arg[i_arg].n_items - 1;
        break;
      case SET_RPN_EXPRESSION:
        if (s_arg[i_arg].n_items < 2 && s_arg[i_arg].n_items > 4)
          SDDS_Bomb("invalid -rpnexpression syntax");
        rpnexpression_definition = (RPNEXPRESSION_DEFINITION **)trealloc(rpnexpression_definition, sizeof(*rpnexpression_definition) * (rpnexpression_definitions + 1));
        if (!(rpnexpression_definition[rpnexpression_definitions] = process_new_rpnexpression_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -rpnexpression syntax");
        add_definition(&definition, &definitions, (void *)rpnexpression_definition[rpnexpression_definitions], IS_RPNEXPRESSION_DEFINITION);
        rpnexpression_definitions++;
        break;
      case SET_DEFINITION:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("invalid -definition syntax");
        equation_definition = (EQUATION_DEFINITION **)trealloc(equation_definition, sizeof(*equation_definition) * (equation_definitions + 1));
        if (!(equation_definition[equation_definitions] = process_new_equation_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -definition syntax");
        equation_definition[equation_definitions]->redefinition = 0;
        add_definition(&definition, &definitions, (void *)equation_definition[equation_definitions], IS_EQUATION_DEFINITION);
        equation_definitions++;
        break;
      case SET_REDEFINITION:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("invalid -redefinition syntax");
        equation_definition = (EQUATION_DEFINITION **)trealloc(equation_definition, sizeof(*equation_definition) * (equation_definitions + 1));
        if (!(equation_definition[equation_definitions] = process_new_equation_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -redefinition syntax");
        equation_definition[equation_definitions]->redefinition = 1;
        add_definition(&definition, &definitions, (void *)equation_definition[equation_definitions], IS_EQUATION_DEFINITION);
        equation_definitions++;
        break;
      case SET_EVALUATE:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("invalid -evaluate syntax");
        evaluate_definition = (EVALUATE_DEFINITION **)trealloc(evaluate_definition, sizeof(*evaluate_definition) * (evaluate_definitions + 1));
        if (!(evaluate_definition[evaluate_definitions] = process_new_evaluate_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -definition syntax");
        add_definition(&definition, &definitions, (void *)evaluate_definition[evaluate_definitions], IS_EVALUATE_DEFINITION);
        evaluate_definitions++;
        break;
      case SET_TEST:
        if (s_arg[i_arg].n_items < 3 && s_arg[i_arg].n_items > 5)
          SDDS_Bomb("invalid -test syntax");
        rpntest_definition = (RPNTEST_DEFINITION **)trealloc(rpntest_definition, sizeof(*rpntest_definition) * (rpntest_definitions + 1));
        if (!(rpntest_definition[rpntest_definitions] = process_new_rpntest_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -test syntax");
        add_definition(&definition, &definitions, (void *)rpntest_definition[rpntest_definitions], IS_RPNTEST_DEFINITION);
        rpntest_definitions++;
        break;
      case SET_SUMMARIZE:
        summarize = 1;
        break;
      case SET_VERBOSE:
        verbose = 1;
        break;
      case SET_NOWARNINGS:
        nowarnings = 1;
        break;
      case SET_PROCESS:
        processing_definition = (PROCESSING_DEFINITION **)trealloc(processing_definition, sizeof(*processing_definition) * (processing_definitions + 1));
        if (!(processing_definition[processing_definitions] = record_processing_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -process syntax--parsing problem");
        add_definition(&definition, &definitions, (void *)processing_definition[processing_definitions], IS_PROCESSING_DEFINITION);
        processing_definitions++;
        break;
      case SET_CONVERT_UNITS:
        if (s_arg[i_arg].n_items != 6)
          SDDS_Bomb("invalid -convertunits syntax--wrong number of items");
        conversion = (CONVERSION_DEFINITION **)trealloc(conversion, sizeof(*conversion) * (conversions + 1));
        if (!(conversion[conversions] = process_conversion_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -convertunits syntax");
        add_definition(&definition, &definitions, (void *)conversion[conversions], IS_CONVERSION_DEFINITION);
        conversions++;
        break;
      case SET_NUMBERTEST:
        /* -numberTest={col | param},<name>[,invert] */
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -numberTest syntax");
        numbertest_definition = (NUMBERTEST_DEFINITION **)trealloc(numbertest_definition, sizeof(*numbertest_definition) * (numbertest_definitions + 1));
        if (!(numbertest_definition[numbertest_definitions] = process_new_numbertest_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -numberTest syntax");
        add_definition(&definition, &definitions, (void *)numbertest_definition[numbertest_definitions], IS_NUMBERTEST_DEFINITION);
        numbertest_definitions++;
        break;
      case SET_SCAN:
        if (s_arg[i_arg].n_items < 5)
          SDDS_Bomb("invalid -scan syntax");
        scan_definition = (SCAN_DEFINITION **)trealloc(scan_definition, sizeof(*scan_definition) * (scan_definitions + 1));
        if (!(scan_definition[scan_definitions] = process_new_scan_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -scan syntax");
        add_definition(&definition, &definitions, (void *)scan_definition[scan_definitions], IS_SCAN_DEFINITION);
        scan_definitions++;
        break;
      case SET_PRINT:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("invalid -print syntax");
        print_definition = (PRINT_DEFINITION **)trealloc(print_definition, sizeof(*print_definition) * (print_definitions + 1));
        if (!(print_definition[print_definitions] = process_new_print_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -print syntax");
        print_definition[print_definitions]->reprint = 0;
        add_definition(&definition, &definitions, (void *)print_definition[print_definitions], IS_PRINT_DEFINITION);
        print_definitions++;
        break;
      case SET_REPRINT:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("invalid -print syntax");
        print_definition = (PRINT_DEFINITION **)trealloc(print_definition, sizeof(*print_definition) * (print_definitions + 1));
        if (!(print_definition[print_definitions] = process_new_print_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -print syntax");
        print_definition[print_definitions]->reprint = 1;
        add_definition(&definition, &definitions, (void *)print_definition[print_definitions], IS_PRINT_DEFINITION);
        print_definitions++;
        break;
      case SET_EDIT:
        if (s_arg[i_arg].n_items < 5)
          SDDS_Bomb("invalid -edit syntax");
        edit_definition = (EDIT_DEFINITION **)trealloc(edit_definition, sizeof(*edit_definition) * (edit_definitions + 1));
        if (!(edit_definition[edit_definitions] = process_new_edit_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, 0)))
          SDDS_Bomb("invalid -edit syntax");
        add_definition(&definition, &definitions, (void *)edit_definition[edit_definitions], IS_EDIT_DEFINITION);
        edit_definitions++;
        break;
      case SET_REEDIT:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("invalid -reedit syntax");
        edit_definition = (EDIT_DEFINITION **)trealloc(edit_definition, sizeof(*edit_definition) * (edit_definitions + 1));
        if (!(edit_definition[edit_definitions] = process_new_edit_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, 1)))
          SDDS_Bomb("invalid -reedit syntax");
        edit_definition[edit_definitions]->reedit = 1;
        add_definition(&definition, &definitions, (void *)edit_definition[edit_definitions], IS_EDIT_DEFINITION);
        edit_definitions++;
        break;
      case SET_SYSTEM:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("invalid -system syntax");
        system_definition = (SYSTEM_DEFINITION **)trealloc(system_definition, sizeof(*system_definition) * (system_definitions + 1));
        if (!(system_definition[system_definitions] = process_new_system_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -system syntax");
        add_definition(&definition, &definitions, (void *)system_definition[system_definitions], IS_SYSTEM_DEFINITION);
        system_definitions++;
        break;
      case SET_IFNOT:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -ifnot syntax");
        add_ifitem(&ifnot_item, s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1);
        break;
      case SET_IFIS:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -ifis syntax");
        add_ifitem(&ifis_item, s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1);
        break;
      case SET_CLIP:
        clip_definition = (CLIP_DEFINITION **)trealloc(clip_definition, sizeof(*clip_definition) * (clip_definitions + 1));
        if (!(clip_definition[clip_definitions] = process_new_clip_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -clip syntax");
        add_definition(&definition, &definitions, (void *)clip_definition[clip_definitions], IS_CLIP_DEFINITION);
        clip_definitions++;
        break;
      case SET_FCLIP:
        fclip_definition = (FCLIP_DEFINITION **)trealloc(fclip_definition, sizeof(*fclip_definition) * (fclip_definitions + 1));
        if (!(fclip_definition[fclip_definitions] = process_new_fclip_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -fclip syntax");
        add_definition(&definition, &definitions, (void *)fclip_definition[fclip_definitions], IS_FCLIP_DEFINITION);
        fclip_definitions++;
        break;
      case SET_SPARSE:
        sparse_definition = (SPARSE_DEFINITION **)trealloc(sparse_definition, sizeof(*sparse_definition) * (sparse_definitions + 1));
        if (!(sparse_definition[sparse_definitions] = process_new_sparse_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -sparse syntax");
        add_definition(&definition, &definitions, (void *)sparse_definition[sparse_definitions], IS_SPARSE_DEFINITION);
        sparse_definitions++;
        break;
      case SET_SAMPLE:
        sample_definition = (SAMPLE_DEFINITION **)trealloc(sample_definition, sizeof(*sample_definition) * (sample_definitions + 1));
        if (!(sample_definition[sample_definitions] = process_new_sample_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -sample syntax");
        add_definition(&definition, &definitions, (void *)sample_definition[sample_definitions], IS_SAMPLE_DEFINITION);
        sample_definitions++;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_FORMAT:
        if (s_arg[i_arg].n_items < 4)
          SDDS_Bomb("invalid -format syntax");
        format_definition = (FORMAT_DEFINITION **)trealloc(format_definition, sizeof(*format_definition) * (format_definitions + 1));
        if (!(format_definition[format_definitions] = process_new_format_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -format syntax");
        add_definition(&definition, &definitions, (void *)format_definition[format_definitions], IS_FORMAT_DEFINITION);
        format_definitions++;
        break;
      case SET_CAST:
        if (s_arg[i_arg].n_items != 5)
          SDDS_Bomb("invalid -cast syntax");
        cast_definition = (CAST_DEFINITION **)trealloc(cast_definition, sizeof(*cast_definition) * (cast_definitions + 1));
        if (!(cast_definition[cast_definitions] = process_new_cast_definition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)))
          SDDS_Bomb("invalid -cast syntax");
        add_definition(&definition, &definitions, (void *)cast_definition[cast_definitions], IS_CAST_DEFINITION);
        cast_definitions++;
        break;
      case SET_DELETE:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -delete syntax");
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case COLUMN_MODE:
          delete_column = trealloc(delete_column, sizeof(*delete_column) * (delete_columns + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            delete_column[i - 2 + delete_columns] = s_arg[i_arg].list[i];
          delete_columns += s_arg[i_arg].n_items - 2;
          break;
        case PARAMETER_MODE:
          delete_parameter = trealloc(delete_parameter, sizeof(*delete_parameter) * (delete_parameters + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            delete_parameter[i - 2 + delete_parameters] = s_arg[i_arg].list[i];
          delete_parameters += s_arg[i_arg].n_items - 2;
          break;
        case ARRAY_MODE:
          delete_array = trealloc(delete_array, sizeof(*delete_array) * (delete_arrays + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            delete_array[i - 2 + delete_arrays] = s_arg[i_arg].list[i];
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
            retain_column[i - 2 + retain_columns] = s_arg[i_arg].list[i];
          retain_columns += s_arg[i_arg].n_items - 2;
          break;
        case PARAMETER_MODE:
          retain_parameter = trealloc(retain_parameter, sizeof(*retain_parameter) * (retain_parameters + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            retain_parameter[i - 2 + retain_parameters] = s_arg[i_arg].list[i];
          retain_parameters += s_arg[i_arg].n_items - 2;
          break;
        case ARRAY_MODE:
          retain_array = trealloc(retain_array, sizeof(*retain_array) * (retain_arrays + s_arg[i_arg].n_items - 2));
          for (i = 2; i < s_arg[i_arg].n_items; i++)
            retain_array[i - 2 + retain_arrays] = s_arg[i_arg].list[i];
          retain_arrays += s_arg[i_arg].n_items - 2;
          break;
        default:
          SDDS_Bomb("invalid -retain syntax: specify column or parameter keyword");
          break;
        }
        break;
      case SET_DESCRIPTION:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "text", SDDS_STRING, &description_text, 1, 0,
                          "contents", SDDS_STRING, &description_contents, 1, 0, NULL))
          SDDS_Bomb("invalid -description syntax");
        break;
      case SET_THREADS:
        if (s_arg[i_arg].n_items != 2 ||
            !sscanf(s_arg[i_arg].list[1], "%d", &threads) || threads < 1)
          SDDS_Bomb("invalid -threads syntax");
        break;
      default:
        snprintf(buffer, SDDS_MAXLINE, "unknown switch: %s", s_arg[i_arg].list[0]);
        SDDS_Bomb(buffer);
        break;
      }
    } else {
      if (inputfile == NULL)
        inputfile = s_arg[i_arg].list[0];
      else if (outputfile == NULL)
        outputfile = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "argument %s is invalid: too many filenames (sddsprocess)\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    }
  }

  processFilenames("sddsprocess", &inputfile, &outputfile, pipeFlags, nowarnings, &tmpfile_used);

  if (rpn_definitions_files) {
    rpn(rpn_definitions_file[0]);
    if (rpn_check_error())
      exit(EXIT_FAILURE);
    for (i = 1; i < rpn_definitions_files; i++) {
      sprintf(s, "\"%s,s\"  @", rpn_definitions_file[i]);
      rpn(s);
      if (rpn_check_error())
        exit(EXIT_FAILURE);
    }
  } else {
    rpn(getenv("RPN_DEFNS"));
    if (rpn_check_error())
      exit(EXIT_FAILURE);
  }
  i_page_memory = rpn_create_mem("i_page", 0);
  table_number_memory = rpn_create_mem("table_number", 0);
  n_rows_memory = rpn_create_mem("n_rows", 0);

  if (!SDDS_InitializeInput(&SDDS_input, inputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!check_ifitems(&SDDS_input, &ifnot_item, 0, !nowarnings) ||
      !check_ifitems(&SDDS_input, &ifis_item, 1, !nowarnings))
    exit(EXIT_SUCCESS);
  original_parameters = SDDS_input.layout.n_parameters;

  if (!description_text)
    SDDS_GetDescription(&SDDS_input, &description_text, &description_contents);
  if (!SDDS_InitializeOutput(&SDDS_output, SDDS_input.layout.data_mode.mode, 1, description_text, description_contents, outputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (columnMajorOrder != -1)
    SDDS_output.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDS_output.layout.data_mode.column_major = SDDS_input.layout.data_mode.column_major;
  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);

#ifdef DEBUG
  fprintf(stderr, "Getting column, parameter, and array names from input file.\n");
#endif
  if (!(orig_column_name = SDDS_GetColumnNames(&SDDS_input, &orig_column_names))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (orig_column_names && !(new_column_name = process_name_options(orig_column_name, &orig_column_flag, orig_column_names, delete_column, delete_columns, retain_column, retain_columns, rename_column, rename_columns, edit_column_request, edit_column_requests)))
    exit(EXIT_FAILURE);

  if (!(orig_parameter_name = SDDS_GetParameterNames(&SDDS_input, &orig_parameter_names))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

#ifdef DEBUG
  fprintf(stderr, "processing name options\n");
#endif
  if (orig_parameter_names && !(new_parameter_name = process_name_options(orig_parameter_name, &orig_parameter_flag, orig_parameter_names, delete_parameter, delete_parameters, retain_parameter, retain_parameters, rename_parameter, rename_parameters, edit_parameter_request, edit_parameter_requests)))
    exit(EXIT_FAILURE);

  if (!(orig_array_name = SDDS_GetArrayNames(&SDDS_input, &orig_array_names))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (orig_array_names && !(new_array_name = process_name_options(orig_array_name, &orig_array_flag, orig_array_names, delete_array, delete_arrays, retain_array, retain_arrays, rename_array, rename_arrays, edit_array_request, edit_array_requests)))
    exit(EXIT_FAILURE);

#ifdef DEBUG
  for (i = 0; i < orig_parameter_names; i++) {
    fprintf(stderr, "parameter %s goes to %s\n", orig_parameter_name[i], orig_parameter_flag[i] ? new_parameter_name[i] : "<deleted>");
  }
  for (i = 0; i < orig_array_names; i++) {
    fprintf(stderr, "array %s goes to %s\n", orig_array_name[i], orig_array_flag[i] ? new_array_name[i] : "<deleted>");
  }
  for (i = 0; i < orig_column_names; i++) {
    fprintf(stderr, "column %s goes to %s\n", orig_column_name[i], orig_column_flag[i] ? new_column_name[i] : "<deleted>");
  }
#endif

#ifdef DEBUG
  fprintf(stderr, "Transferring definitions to new file.\n");
#endif
  for (i = 0; i < orig_parameter_names; i++) {
    if (orig_parameter_flag[i]) {
      if (!SDDS_TransferParameterDefinition(&SDDS_output, &SDDS_input, orig_parameter_name[i], new_parameter_name[i])) {
        fprintf(stderr, "unable to transfer parameter %s to %s\n", orig_parameter_name[i], new_parameter_name[i]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }
  for (i = 0; i < orig_array_names; i++) {
    if (orig_array_flag[i]) {
      if (!SDDS_TransferArrayDefinition(&SDDS_output, &SDDS_input, orig_array_name[i], new_array_name[i])) {
        fprintf(stderr, "unable to transfer array %s to %s\n", orig_array_name[i], new_array_name[i]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }
  //output_columns = 0;
  for (i = 0; i < orig_column_names; i++) {
    if (orig_column_flag[i]) {
      //output_columns++;
      if (!SDDS_TransferColumnDefinition(&SDDS_output, &SDDS_input, orig_column_name[i], new_column_name[i])) {
        fprintf(stderr, "unable to transfer column %s to %s\n", orig_column_name[i], new_column_name[i]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }
  for (i = 0; i < orig_column_names; i++) {
    free(new_column_name[i]);
    free(orig_column_name[i]);
  }
  free(new_column_name);
  free(orig_column_name);

  expandDefinitions(&definition, &definitions, &SDDS_output);

  for (i = 0; i < definitions; i++) {
    switch (definition[i].type) {
    case IS_EQUATION_DEFINITION:
      equation_ptr = (EQUATION_DEFINITION *)definition[i].structure;
      sprintf(s, "%sUDF%ld", equation_ptr->name, udfCounter++);
      delete_chars(s, " ");
      if (!SDDS_CopyString(&equation_ptr->udf_name, s))
        SDDS_Bomb("string copying failed (making udf name)");
      if (equation_ptr->equation[0] == '@') {
        if (equation_ptr->equation[1] == '@') {
          /* parameter contains algebraic notation */
          if ((index = SDDS_GetParameterIndex(&SDDS_output, equation_ptr->equation + 2)) < 0 ||
              SDDS_GetParameterType(&SDDS_input, index) != SDDS_STRING) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            fprintf(stderr, "Error (sddsprocess): parameter %s does not exist or is not string type\n", equation_ptr->equation + 2);
            exit(EXIT_FAILURE);
          }
        } else {
          if ((index = SDDS_GetParameterIndex(&SDDS_output, equation_ptr->equation + 1)) < 0 ||
              SDDS_GetParameterType(&SDDS_input, index) != SDDS_STRING) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            fprintf(stderr, "Error (sddsprocess): parameter %s does not exist or is not string type\n", equation_ptr->equation + 1);
            exit(EXIT_FAILURE);
          }
        }
      } else {
        if (strstr(equation_ptr->equation, "xstr") == NULL)
          create_udf(equation_ptr->udf_name, equation_ptr->equation);
        else
          /* This works around a bug in rpn that results in a stack overflow for definitions that use the 'xstr' function  */
          equation_ptr->udf_name = equation_ptr->equation;
      }
      if (equation_ptr->redefinition) {
        if (equation_ptr->is_parameter) {
          if ((index = SDDS_GetParameterIndex(&SDDS_output, equation_ptr->name)) < 0)
            equation_ptr->redefinition = 0;
        } else if ((index = SDDS_GetColumnIndex(&SDDS_output, equation_ptr->name)) < 0)
          equation_ptr->redefinition = 0;
      }
      if (equation_ptr->redefinition == 0) {
        if (equation_ptr->is_parameter) {
          if (!SDDS_ProcessParameterString(&SDDS_output, equation_ptr->text, SDDS_WRITEONLY_DEFINITION)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            fprintf(stderr, "Bad parameter string: %s\n", equation_ptr->text);
            exit(EXIT_FAILURE);
          }
        } else if (!SDDS_ProcessColumnString(&SDDS_output, equation_ptr->text, SDDS_WRITEONLY_DEFINITION)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          fprintf(stderr, "Bad column string: %s\n", equation_ptr->text);
          exit(EXIT_FAILURE);
        }
      } else {
        if (equation_ptr->is_parameter) {
          if (!REDEFINABLE_TYPE(SDDS_GetParameterType(&SDDS_output, index))) {
            fprintf(stderr, "can't redefine parameter %s--non-numeric\n", equation_ptr->name);
            exit(EXIT_FAILURE);
          }
          if (equation_ptr->argc && !SDDS_RedefineParameterCL(&SDDS_output, equation_ptr->name, equation_ptr->argv, equation_ptr->argc)) {
            fprintf(stderr, "can't redefine parameter %s as requested\n", equation_ptr->name);
            exit(EXIT_FAILURE);
          }
        } else {
          if (!REDEFINABLE_TYPE(SDDS_GetColumnType(&SDDS_output, index))) {
            fprintf(stderr, "can't redefine column %s--non-numeric\n", equation_ptr->name);
            exit(EXIT_FAILURE);
          }
          if (equation_ptr->argc && !SDDS_RedefineColumnCL(&SDDS_output, equation_ptr->name, equation_ptr->argv, equation_ptr->argc)) {
            fprintf(stderr, "can't redefine column %s as requested\n", equation_ptr->name);
            exit(EXIT_FAILURE);
          }
        }
      }
      break;
    case IS_EDIT_DEFINITION:
      edit_ptr = (EDIT_DEFINITION *)definition[i].structure;
      index = edit_ptr->is_parameter ? SDDS_GetParameterIndex(&SDDS_output, edit_ptr->new_name) : SDDS_GetColumnIndex(&SDDS_output, edit_ptr->new_name);
      if (index < 0)
        edit_ptr->reedit = 0;
      else if (!edit_ptr->reedit) {
        fprintf(stderr, "Can't define new -edit %s %s--already exists (sddsprocess)\n", edit_ptr->is_parameter ? "parameter" : "column", edit_ptr->new_name);
        exit(EXIT_FAILURE);
      }
      if (!edit_ptr->reedit) {
        if (!(edit_ptr->is_parameter ? SDDS_ProcessParameterString(&SDDS_output, edit_ptr->text, SDDS_WRITEONLY_DEFINITION) : SDDS_ProcessColumnString(&SDDS_output, edit_ptr->text, SDDS_WRITEONLY_DEFINITION))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else {
        if ((edit_ptr->is_parameter ? SDDS_GetParameterType(&SDDS_output, index) : SDDS_GetColumnType(&SDDS_output, index)) != SDDS_STRING) {
          fprintf(stderr, "Can't reedit %s %s--not string type (sddsprocess)\n", edit_ptr->is_parameter ? "parameter" : "column", edit_ptr->new_name);
          exit(EXIT_FAILURE);
        }
        if (edit_ptr->argc && !(edit_ptr->is_parameter ? SDDS_RedefineParameterCL(&SDDS_output, edit_ptr->new_name, edit_ptr->argv, edit_ptr->argc) : SDDS_RedefineColumnCL(&SDDS_output, edit_ptr->new_name, edit_ptr->argv, edit_ptr->argc))) {
          fprintf(stderr, "Can't reedit %s %s as requested (sddsprocess)\n", edit_ptr->is_parameter ? "parameter" : "column", edit_ptr->new_name);
          exit(EXIT_FAILURE);
        }
      }
      break;
    case IS_SCAN_DEFINITION:
      scan_ptr = (SCAN_DEFINITION *)definition[i].structure;
      if (scan_ptr->is_parameter) {
        if (!SDDS_ProcessParameterString(&SDDS_output, scan_ptr->text, SDDS_WRITEONLY_DEFINITION)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else if (!SDDS_ProcessColumnString(&SDDS_output, scan_ptr->text, SDDS_WRITEONLY_DEFINITION)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      break;
    case IS_PRINT_DEFINITION:
      print_ptr = (PRINT_DEFINITION *)definition[i].structure;
      index = print_ptr->is_parameter ? SDDS_GetParameterIndex(&SDDS_output, print_ptr->new_name) : SDDS_GetColumnIndex(&SDDS_output, print_ptr->new_name);
      if (index < 0)
        print_ptr->reprint = 0;
      else if (!print_ptr->reprint) {
        fprintf(stderr, "Can't define new -print %s %s--already exists (sddsprocess)\n", print_ptr->is_parameter ? "parameter" : "column", print_ptr->new_name);
        exit(EXIT_FAILURE);
      }
      if (!print_ptr->reprint) {
        if (!(print_ptr->is_parameter ? SDDS_ProcessParameterString(&SDDS_output, print_ptr->text, SDDS_WRITEONLY_DEFINITION) : SDDS_ProcessColumnString(&SDDS_output, print_ptr->text, SDDS_WRITEONLY_DEFINITION))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else {
        if ((print_ptr->is_parameter ? SDDS_GetParameterType(&SDDS_output, index) : SDDS_GetColumnType(&SDDS_output, index)) != SDDS_STRING) {
          fprintf(stderr, "Can't reprint %s %s--not string type (sddsprocess)\n", print_ptr->is_parameter ? "parameter" : "column", print_ptr->new_name);
          exit(EXIT_FAILURE);
        }
      }
      break;
    case IS_PROCESSING_DEFINITION:
      processing_ptr = (PROCESSING_DEFINITION *)definition[i].structure;
      if (!complete_processing_definitions(&processing_ptr, 1, &SDDS_output)) {
        fprintf(stderr, "error: problem completing processing definition\n");
        if (SDDS_NumberOfErrors())
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      break;
    case IS_CONVERSION_DEFINITION:
      conversion_ptr = (CONVERSION_DEFINITION *)definition[i].structure;
      if (conversion_ptr->is_parameter) {
        if ((j = SDDS_GetParameterIndex(&SDDS_output, conversion_ptr->name)) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (SDDS_CheckParameter(&SDDS_output, conversion_ptr->name, conversion_ptr->old_units, SDDS_GetParameterType(&SDDS_output, j), stderr) != SDDS_CHECK_OKAY)
          exit(EXIT_FAILURE);
        if (!SDDS_ChangeParameterInformation(&SDDS_output, "units", conversion_ptr->new_units, SDDS_SET_BY_NAME, conversion_ptr->name)) {
          if (SDDS_NumberOfErrors())
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          else
            fprintf(stderr, "error: unknown error processing conversion for parameter %s\n", conversion_ptr->name);
          exit(EXIT_FAILURE);
        }
      } else {
        if ((j = SDDS_GetColumnIndex(&SDDS_output, conversion_ptr->name)) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (SDDS_CheckColumn(&SDDS_output, conversion_ptr->name, conversion_ptr->old_units, SDDS_GetColumnType(&SDDS_output, j), stderr) != SDDS_CHECK_OKAY)
          exit(EXIT_FAILURE);
        if (!SDDS_ChangeColumnInformation(&SDDS_output, "units", conversion_ptr->new_units, SDDS_SET_BY_NAME, conversion_ptr->name)) {
          if (SDDS_NumberOfErrors())
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          else
            fprintf(stderr, "error: unknown error processing conversion for column %s\n", conversion_ptr->name);
          exit(EXIT_FAILURE);
        }
      }
      break;
    case IS_SYSTEM_DEFINITION:
      system_ptr = (SYSTEM_DEFINITION *)definition[i].structure;
      if (system_ptr->is_parameter) {
        if (!SDDS_ProcessParameterString(&SDDS_output, system_ptr->text, SDDS_WRITEONLY_DEFINITION)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else if (!SDDS_ProcessColumnString(&SDDS_output, system_ptr->text, SDDS_WRITEONLY_DEFINITION)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      break;
    case IS_FORMAT_DEFINITION:
      format_ptr = (FORMAT_DEFINITION *)definition[i].structure;
      if (strcmp(format_ptr->source, format_ptr->target)) {
        if (format_ptr->is_parameter) {
          if (!SDDS_TransferParameterDefinition(&SDDS_output, &SDDS_output, format_ptr->source, format_ptr->target)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        } else if (!SDDS_TransferColumnDefinition(&SDDS_output, &SDDS_output, format_ptr->source, format_ptr->target)) {
          fprintf(stderr, "Problem setting up to format %s from %s\n", format_ptr->target, format_ptr->source);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
      break;
    case IS_EVALUATE_DEFINITION:
      evaluate_ptr = (EVALUATE_DEFINITION *)definition[i].structure;
      index = evaluate_ptr->is_parameter ? SDDS_GetParameterIndex(&SDDS_output, evaluate_ptr->name) : SDDS_GetColumnIndex(&SDDS_output, evaluate_ptr->name);
      if (index >= 0) {
        fprintf(stderr, "Can't define new -evaluate %s %s--already exists (sddsprocess)\n", evaluate_ptr->is_parameter ? "parameter" : "column", evaluate_ptr->name);
        exit(EXIT_FAILURE);
      }
      if ((index = evaluate_ptr->is_parameter ? SDDS_GetParameterIndex(&SDDS_output, evaluate_ptr->source) : SDDS_GetColumnIndex(&SDDS_output, evaluate_ptr->source)) < 0) {
        fprintf(stderr, "Can't evaluate %s %s--doesn't exist (sddsprocess)\n", evaluate_ptr->is_parameter ? "parameter" : "column", evaluate_ptr->source);
        exit(EXIT_FAILURE);
      }
      if ((evaluate_ptr->is_parameter ? SDDS_GetParameterType(&SDDS_output, index) : SDDS_GetColumnType(&SDDS_output, index)) != SDDS_STRING) {
        fprintf(stderr, "Can't evaluate %s %s--not string type (sddsprocess)\n", evaluate_ptr->is_parameter ? "parameter" : "column", evaluate_ptr->source);
        exit(EXIT_FAILURE);
      }
      if (!(evaluate_ptr->is_parameter ? SDDS_ProcessParameterString(&SDDS_output, evaluate_ptr->text, SDDS_WRITEONLY_DEFINITION) : SDDS_ProcessColumnString(&SDDS_output, evaluate_ptr->text, SDDS_WRITEONLY_DEFINITION))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      break;
    case IS_FILTER_DEFINITION:
    case IS_MATCH_DEFINITION:
    case IS_RPNTEST_DEFINITION:
    case IS_RPNEXPRESSION_DEFINITION:
    case IS_CLIP_DEFINITION:
    case IS_FCLIP_DEFINITION:
    case IS_SPARSE_DEFINITION:
    case IS_SAMPLE_DEFINITION:
    case IS_NUMBERTEST_DEFINITION:
    case IS_TIME_FILTER_DEFINITION:
      break;
    case IS_CAST_DEFINITION:
      if (!complete_cast_definition(&SDDS_output, (CAST_DEFINITION *)definition[i].structure, &SDDS_input)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      break;
    default:
      fprintf(stderr, "error: unknown definition type code %ld seen--this shouldn't happen!\n", definition[i].type);
      exit(EXIT_FAILURE);
      break;
    }
  }

  if (!SDDS_DeleteParameterFixedValues(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (!SDDS_WriteLayout(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (summarize) {
    for (i = 0; i < definitions; i++) {
      switch (definition[i].type) {
      case IS_EQUATION_DEFINITION:
        equation_ptr = (EQUATION_DEFINITION *)definition[i].structure;
        fprintf(stderr, "%s %s created from equation %s:\n    %s\n", equation_ptr->is_parameter ? "parameter" : "column", equation_ptr->name, equation_ptr->equation, equation_ptr->text);
        break;
      case IS_EVALUATE_DEFINITION:
        evaluate_ptr = (EVALUATE_DEFINITION *)definition[i].structure;
        fprintf(stderr, "evaluating to %s %s from %s\n", evaluate_ptr->is_parameter ? "parameter" : "column", evaluate_ptr->name, evaluate_ptr->source);
        break;
      case IS_EDIT_DEFINITION:
        edit_ptr = (EDIT_DEFINITION *)definition[i].structure;
        fprintf(stderr, "%s %s edit from %s using %s:\n    %s\n", edit_ptr->is_parameter ? "parameter" : "column", edit_ptr->new_name, edit_ptr->source, edit_ptr->edit_command, edit_ptr->text);
        break;
      case IS_SCAN_DEFINITION:
        scan_ptr = (SCAN_DEFINITION *)definition[i].structure;
        fprintf(stderr, "%s %s scanned from %s using %s:\n    %s\n", scan_ptr->is_parameter ? "parameter" : "column", scan_ptr->new_name, scan_ptr->source, scan_ptr->sscanf_string, scan_ptr->text);
        break;
      case IS_PRINT_DEFINITION:
        print_ptr = (PRINT_DEFINITION *)definition[i].structure;
        fprintf(stderr, "printing to %s %s from ", print_ptr->is_parameter ? "parameter" : "column", print_ptr->new_name);
        for (j = 0; j < print_ptr->sources; j++)
          fprintf(stderr, "%s ", print_ptr->source[j]);
        fprintf(stderr, ":\n    %s\n", print_ptr->text);
        break;
      case IS_PROCESSING_DEFINITION:
        processing_ptr = (PROCESSING_DEFINITION *)definition[i].structure;
        fprintf(stderr, "column %s will be processed to give %s in parameter %s\n", processing_ptr->column_name, processing_ptr->description, processing_ptr->parameter_name);
        break;
      case IS_CONVERSION_DEFINITION:
        conversion_ptr = (CONVERSION_DEFINITION *)definition[i].structure;
        fprintf(stderr, "%s %s will be converted to %s using factor %e\n", conversion_ptr->is_parameter ? "parameter" : "column", conversion_ptr->name, conversion_ptr->new_units, conversion_ptr->factor);
        break;
      case IS_FILTER_DEFINITION:
        filter_ptr = (FILTER_DEFINITION *)definition[i].structure;
        show_filters(filter_ptr->is_parameter ? "parameter" : "column", filter_ptr->filter_term, filter_ptr->filter_terms);
        break;
      case IS_TIME_FILTER_DEFINITION:
        time_filter_ptr = (TIME_FILTER_DEFINITION *)definition[i].structure;
        fprintf(stderr, "Time filter: <%s> %22.15e, %22.15e\n", time_filter_ptr->name, time_filter_ptr->before, time_filter_ptr->after);
      case IS_MATCH_DEFINITION:
        match_ptr = (MATCH_DEFINITION *)definition[i].structure;
        show_matches(match_ptr->is_parameter ? "parameter" : "column", match_ptr->match_term, match_ptr->match_terms);
        break;
      case IS_RPNTEST_DEFINITION:
        rpntest_ptr = (RPNTEST_DEFINITION *)definition[i].structure;
        if (rpntest_ptr->autostop)
          fprintf(stderr, "%s-based autostop rpn test:\n    %s\n", rpntest_ptr->is_parameter ? "parameter" : "column", rpntest_ptr->expression);
        else
          fprintf(stderr, "%s-based rpn test:\n    %s\n", rpntest_ptr->is_parameter ? "parameter" : "column", rpntest_ptr->expression);
        break;
      case IS_NUMBERTEST_DEFINITION:
        numbertest_ptr = (NUMBERTEST_DEFINITION *)definition[i].structure;
        fprintf(stderr, "%s-based number test:\n    %s  %s%s\n", numbertest_ptr->is_parameter ? "parameter" : "column", numbertest_ptr->name, numbertest_ptr->flags & NUMSCANFILTER_INVERT ? " inverted" : "", numbertest_ptr->flags & NUMSCANFILTER_STRICT ? " strict" : "");
        break;
      case IS_RPNEXPRESSION_DEFINITION:
        rpnexpression_ptr = (RPNEXPRESSION_DEFINITION *)definition[i].structure;
        fprintf(stderr, "%srepeated rpn expression:\n    %s\n", (rpnexpression_ptr->repeat ? "" : "un"), rpnexpression_ptr->expression);
        break;
      case IS_SYSTEM_DEFINITION:
        system_ptr = (SYSTEM_DEFINITION *)definition[i].structure;
        fprintf(stderr, "%s %s system-call from %s:\n    %s\n", system_ptr->is_parameter ? "parameter" : "column", system_ptr->new_name, system_ptr->source, system_ptr->text);
        break;
      case IS_FORMAT_DEFINITION:
        format_ptr = (FORMAT_DEFINITION *)definition[i].structure;
        fprintf(stderr, "formating %s %s from %s using ", format_ptr->is_parameter ? "parameter" : "column", format_ptr->target, format_ptr->source);
        fprintf(stderr, "string: %s  double: %s  long: %s\n", format_ptr->stringFormat ? format_ptr->stringFormat : "<none>", format_ptr->doubleFormat ? format_ptr->doubleFormat : "<none>", format_ptr->longFormat ? format_ptr->longFormat : "<none>");
        break;
      case IS_CLIP_DEFINITION:
        clip_ptr = (CLIP_DEFINITION *)definition[i].structure;
        fprintf(stderr, "page clip operation:\n    head = %" PRId64 "  tail = %" PRId64 "  invert = %hd\n", clip_ptr->head, clip_ptr->tail, clip_ptr->invert);
        break;
      case IS_FCLIP_DEFINITION:
        fclip_ptr = (FCLIP_DEFINITION *)definition[i].structure;
        fprintf(stderr, "page clip operation:\n    head = %f  tail = %f  invert = %hd\n", fclip_ptr->fhead, fclip_ptr->ftail, fclip_ptr->invert);
        break;
      case IS_SPARSE_DEFINITION:
        sparse_ptr = (SPARSE_DEFINITION *)definition[i].structure;
        fprintf(stderr, "page sparse operation:\n    interval = %" PRId64 "  offset = %" PRId64 "\n", sparse_ptr->interval, sparse_ptr->offset);
        break;
      case IS_SAMPLE_DEFINITION:
        sample_ptr = (SAMPLE_DEFINITION *)definition[i].structure;
        fprintf(stderr, "page sample operation:\n    fraction = %.8g\n", sample_ptr->fraction);
        break;
      case IS_CAST_DEFINITION:
        cast_ptr = (CAST_DEFINITION *)definition[i].structure;
        fprintf(stderr, "%s %s cast from %s to type %s\n", cast_ptr->isParameter ? "parameter" : "column", cast_ptr->newName, cast_ptr->source, cast_ptr->newTypeName);
        break;
      default:
        fprintf(stderr, "error: unknown definition type code %ld seen--this shouldn't happen!\n", definition[i].type);
        exit(EXIT_FAILURE);
        break;
      }
    }
  }

  while ((page_number = SDDS_ReadPage(&SDDS_input)) > 0) {
    if (!SDDS_CopyPage(&SDDS_output, &SDDS_input)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (!SDDS_SetRowFlags(&SDDS_output, 1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if ((n_rows = SDDS_CountRowsOfInterest(&SDDS_output)) <= 0 && SDDS_ColumnCount(&SDDS_output) != 0) {
      if (!nowarnings)
        fprintf(stderr, "warning: no rows selected for page %" PRId32 "\n", SDDS_output.page_number);
      if (n_rows == 0)
        SDDS_ClearErrors();
      else
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (verbose) {
      fprintf(stderr, "page number %ld read in, with %" PRId64 " rows of data\n", page_number, n_rows);
      for (i = 0; i < original_parameters; i++) {
        SDDS_PrintTypedValue(SDDS_output.parameter[i], 0, SDDS_output.layout.parameter_definition[i].type, NULL, stderr, 0);
        fputc('\n', stderr);
      }
    }
    rpn_store((double)page_number, NULL, table_number_memory);
    rpn_store((double)page_number, NULL, i_page_memory);
    row_deletion = skip_page = 0;
    i = -1;
    while (++i < definitions && !skip_page) {
      rpn_store((double)n_rows, NULL, n_rows_memory);
      switch (definition[i].type) {
      case IS_CONVERSION_DEFINITION:
        conversion_ptr = (CONVERSION_DEFINITION *)definition[i].structure;
        if (conversion_ptr->is_parameter) {
          if (!SDDS_ApplyFactorToParameter(&SDDS_output, conversion_ptr->name, conversion_ptr->factor)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        } else if (n_rows && !SDDS_ApplyFactorToColumn(&SDDS_output, conversion_ptr->name, conversion_ptr->factor)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
        if (verbose)
          fprintf(stderr, "conversion done: %s %s from %s to %s using factor %e\n", conversion_ptr->is_parameter ? "parameter" : "column", conversion_ptr->name, conversion_ptr->old_units, conversion_ptr->new_units, conversion_ptr->factor);
        break;
      case IS_PROCESSING_DEFINITION:
        processing_ptr = (PROCESSING_DEFINITION *)definition[i].structure;
        if (processing_ptr->type != SDDS_STRING) {
          if (!process_column(&SDDS_output, processing_ptr, &result, &stringResult, !nowarnings, threads))
            exit(EXIT_FAILURE);
          if (processing_ptr->outputType != SDDS_STRING) {
            if (!SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, processing_ptr->parameter_name, result, NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
            rpn_store(result, NULL, processing_ptr->memory_number);
            if (verbose)
              fprintf(stderr, "processing result: %s = %e\n", processing_ptr->parameter_name, result);
          } else {
            if (!SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, processing_ptr->parameter_name, stringResult, NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
            if (verbose)
              fprintf(stderr, "processing result: %s = %s\n", processing_ptr->parameter_name, stringResult);
            free(stringResult);
          }
        } else {
          if (!(stringResult = process_string_column(&SDDS_output, processing_ptr, !nowarnings)) ||
              !SDDS_SetParameters(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, processing_ptr->parameter_name, stringResult, NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          free(stringResult);
        }
        break;
      case IS_EDIT_DEFINITION:
        edit_ptr = (EDIT_DEFINITION *)definition[i].structure;
        if (edit_ptr->is_parameter) {
          if (!edit_parameter_value(&SDDS_output, edit_ptr->new_name, edit_ptr->source, edit_ptr->edit_command)) {
            fprintf(stderr, "couldn't edit %s to make %s\n", edit_ptr->source, edit_ptr->new_name);
            exit(EXIT_FAILURE);
          }
        } else if (n_rows && !edit_column_value(&SDDS_output, edit_ptr->new_name, edit_ptr->source, edit_ptr->edit_command)) {
          fprintf(stderr, "couldn't edit %s to make %s\n", edit_ptr->source, edit_ptr->new_name);
          exit(EXIT_FAILURE);
        }
        if (verbose)
          fprintf(stderr, "edited %s %s to produce %s, using edit-command %s\n", edit_ptr->is_parameter ? "parameter" : "column", edit_ptr->source, edit_ptr->new_name, edit_ptr->edit_command);
        break;
      case IS_SCAN_DEFINITION:
        scan_ptr = (SCAN_DEFINITION *)definition[i].structure;
        if (scan_ptr->is_parameter) {
          if (!scan_parameter_value(&SDDS_output, scan_ptr->new_name, scan_ptr->source, scan_ptr->sscanf_string, scan_ptr->edit)) {
            fprintf(stderr, "couldn't scan %s to make %s\n", scan_ptr->source, scan_ptr->new_name);
            exit(EXIT_FAILURE);
          }
        } else if (n_rows && !scan_column_value(&SDDS_output, scan_ptr->new_name, scan_ptr->source, scan_ptr->sscanf_string, scan_ptr->edit)) {
          fprintf(stderr, "couldn't scan %s to make %s\n", scan_ptr->source, scan_ptr->new_name);
          exit(EXIT_FAILURE);
        }
        if (verbose)
          fprintf(stderr, "scanned %s %s to produce %s, using sscanf string %s\n", scan_ptr->is_parameter ? "parameter" : "column", scan_ptr->source, scan_ptr->new_name, scan_ptr->sscanf_string);
        break;
      case IS_PRINT_DEFINITION:
        print_ptr = (PRINT_DEFINITION *)definition[i].structure;
        if (print_ptr->is_parameter) {
          if (!print_parameter_value(&SDDS_output, print_ptr->new_name, print_ptr->source, print_ptr->sources, print_ptr->printf_string)) {
            fprintf(stderr, "couldn't print to make parameter %s\n", print_ptr->new_name);
            exit(EXIT_FAILURE);
          }
        } else if (n_rows && !print_column_value(&SDDS_output, print_ptr->new_name, print_ptr->source, print_ptr->sources, print_ptr->printf_string)) {
          fprintf(stderr, "couldn't print to make column %s\n", print_ptr->new_name);
          exit(EXIT_FAILURE);
        }
        break;
      case IS_EQUATION_DEFINITION:
        equation_ptr = (EQUATION_DEFINITION *)definition[i].structure;
        if (equation_ptr->is_parameter) {
          if (!SDDS_ComputeParameter(&SDDS_output, SDDS_GetParameterIndex(&SDDS_output, equation_ptr->name), equation_ptr->udf_name)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        } else if (n_rows) {
          k = i++;
          while (i < definitions && definition[i].type == IS_EQUATION_DEFINITION) {
            equation_ptr = (EQUATION_DEFINITION *)definition[i].structure;
            if (equation_ptr->is_parameter)
              break;
            else
              i++;
          }
          if (!SDDS_ComputeSetOfColumns(&SDDS_output, k, i, definition)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          i--;
        }
        if (verbose)
          fprintf(stderr, "computed %s %s using expression %s\n", equation_ptr->is_parameter ? "parameter" : "column", equation_ptr->name, equation_ptr->equation);
        break;
      case IS_EVALUATE_DEFINITION:
        evaluate_ptr = (EVALUATE_DEFINITION *)definition[i].structure;
        if (evaluate_ptr->is_parameter) {
          if (!SDDS_EvaluateParameter(&SDDS_output, evaluate_ptr)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        } else if (n_rows) {
          if (!SDDS_EvaluateColumn(&SDDS_output, evaluate_ptr)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        }
        if (verbose)
          fprintf(stderr, "computed %s %s using expressions from %s\n", evaluate_ptr->is_parameter ? "parameter" : "column", evaluate_ptr->name, evaluate_ptr->source);
        break;
      case IS_FORMAT_DEFINITION:
        format_ptr = (FORMAT_DEFINITION *)definition[i].structure;
        if (format_ptr->is_parameter) {
          if (!format_parameter_value(&SDDS_output, format_ptr)) {
            fprintf(stderr, "couldn't format parameter %s\n", format_ptr->target);
            exit(EXIT_FAILURE);
          }
        } else if (n_rows && !format_column_value(&SDDS_output, format_ptr)) {
          fprintf(stderr, "couldn't format column %s\n", format_ptr->target);
          exit(EXIT_FAILURE);
        }
        if (verbose)
          fprintf(stderr, "formatted %s %s\n", format_ptr->is_parameter ? "parameter" : "column", format_ptr->target);
        break;
      case IS_TIME_FILTER_DEFINITION:
        time_filter_ptr = (TIME_FILTER_DEFINITION *)definition[i].structure;
        if (time_filter_ptr->is_parameter) {
          if (!(pardefptr = SDDS_GetParameterDefinition(&SDDS_output, time_filter_ptr->name)) ||
              (pardefptr->type == SDDS_STRING || pardefptr->type == SDDS_CHARACTER)) {
            fprintf(stderr, "error: unknown or non-numeric parameter %s given for time filter\n", time_filter_ptr->name);
            exit(EXIT_FAILURE);
          }
          accept = SDDS_ItemInsideWindow(SDDS_GetParameter(&SDDS_output, time_filter_ptr->name, &result), 0, pardefptr->type, time_filter_ptr->after, time_filter_ptr->before);

          if (!accept)
            skip_page = 1;
          if (time_filter_ptr->flags & TIMEFILTER_INVERT_GIVEN)
            skip_page = !skip_page;
          if (skip_page) {
            if (verbose)
              fputs("    * page outside parameter filter--continuing to next page", stderr);
            continue;
          }
        } else if (n_rows) {
          int64_t nleft;
          if (verbose) {
            fprintf(stderr, "   * applying time filter (column %s)", time_filter_ptr->name);
            fflush(stderr);
          }
          if ((nleft = SDDS_FilterRowsOfInterest(&SDDS_output, time_filter_ptr->name, time_filter_ptr->after, time_filter_ptr->before, time_filter_ptr->flags & TIMEFILTER_INVERT_GIVEN ? SDDS_NEGATE_EXPRESSION : SDDS_AND)) < 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          if (verbose) {
            fprintf(stderr, "---%" PRId64 "  rows left\n", nleft);
            fflush(stderr);
          }
          row_deletion = 1;
        }
        break;
      case IS_FILTER_DEFINITION:
        filter_ptr = (FILTER_DEFINITION *)definition[i].structure;
        filter_term = filter_ptr->filter_term;
        for (j = 0; j < filter_ptr->filter_terms; j++) {
          if (filter_term[j].lowerPar && !SDDS_GetParameterAsDouble(&SDDS_input, filter_term[j].lowerPar, &(filter_term[j].lower))) {
            fprintf(stderr, "Error: unable to get parameter %s value for the lower filter from input file.\n", filter_term[j].lowerPar);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          if (filter_term[j].upperPar && !SDDS_GetParameterAsDouble(&SDDS_input, filter_term[j].upperPar, &(filter_term[j].upper))) {
            fprintf(stderr, "Error: unable to get parameter %s value for the upper filter from input file.\n", filter_term[j].upperPar);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          if (filter_term[j].upper < filter_term[j].lower) {
            fprintf(stderr, "Error: invalid %s filter provided.\n", filter_term[j].name);
            exit(EXIT_FAILURE);
          }
        }
        if (filter_ptr->is_parameter) {
          accept = 1;
          filter_term = filter_ptr->filter_term;
          for (j = 0; j < filter_ptr->filter_terms; j++) {
            if (!(pardefptr = SDDS_GetParameterDefinition(&SDDS_output, filter_term[j].name)) ||
                (pardefptr->type == SDDS_STRING || pardefptr->type == SDDS_CHARACTER)) {
              fprintf(stderr, "error: unknown or non-numeric parameter %s given for filter\n", filter_term[j].name);
              exit(EXIT_FAILURE);
            }
            accept = SDDS_Logic(accept, SDDS_ItemInsideWindow(SDDS_GetParameter(&SDDS_output, filter_term[j].name, &result), 0, pardefptr->type, filter_term[j].lower, filter_term[j].upper), filter_term[j].logic);
          }
          if (!accept) {
            if (verbose)
              fputs("    * page failed parameter matching--continuing to next page", stderr);
            skip_page = 1;
            continue;
          }
        } else if (n_rows) {
          int64_t nleft;
          filter_term = filter_ptr->filter_term;
          for (j = 0; j < filter_ptr->filter_terms; j++) {
            if (verbose) {
              fprintf(stderr, "   * applying filter term %" PRId64 " (column %s)", j, filter_term[j].name);
              fflush(stderr);
            }
            if ((nleft = SDDS_FilterRowsOfInterest(&SDDS_output, filter_term[j].name, filter_term[j].lower, filter_term[j].upper, filter_term[j].logic)) < 0) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
            if (verbose) {
              fprintf(stderr, "---%" PRId64 " rows left\n", nleft);
              fflush(stderr);
            }
          }
          row_deletion = 1;
        }
        if (verbose)
          fprintf(stderr, "applied %s filter\n", filter_ptr->is_parameter ? "parameter" : "column");
        break;
      case IS_MATCH_DEFINITION:
        match_ptr = (MATCH_DEFINITION *)definition[i].structure;
        if (match_ptr->is_parameter) {
          accept = 1;
          match_term = match_ptr->match_term;
          for (j = 0; j < match_ptr->match_terms; j++) {
            if (!(pardefptr = SDDS_GetParameterDefinition(&SDDS_output, match_term[j].name)) ||
                !(pardefptr->type == SDDS_STRING || pardefptr->type == SDDS_CHARACTER)) {
              fprintf(stderr, "error: unknown or numeric parameter %s given for match\n", match_term[j].name);
              exit(EXIT_FAILURE);
            }
            if (pardefptr->type == SDDS_STRING) {
              char **ppc;
              ppc = SDDS_GetParameter(&SDDS_output, match_term[j].name, NULL);
              strcpy(s, *ppc);
            } else {
              char *pc;
              pc = SDDS_GetParameter(&SDDS_output, match_term[j].name, NULL);
              sprintf(s, "%c", *pc);
            }
            accept = SDDS_Logic(accept, (match_term[j].logic & SDDS_NOCASE_COMPARE ? wild_match_ci(s, match_term[j].string) : wild_match(s, match_term[j].string)), match_term[j].logic);
          }
          if (!accept) {
            if (verbose)
              fputs("    * page failed parameter matching--continuing to next page", stderr);
            skip_page = 1;
            continue;
          }
        } else if (n_rows) {
          match_term = match_ptr->match_term;
          for (j = 0; j < match_ptr->match_terms; j++)
            if (SDDS_MatchRowsOfInterest(&SDDS_output, match_term[j].name, match_term[j].string, match_term[j].logic) < 0) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              exit(EXIT_FAILURE);
            }
          row_deletion = 1;
        }
        if (verbose)
          fprintf(stderr, "applied %s match\n", match_ptr->is_parameter ? "parameter" : "column");
        break;
      case IS_RPNTEST_DEFINITION:
        rpntest_ptr = (RPNTEST_DEFINITION *)definition[i].structure;
        SDDS_StoreParametersInRpnMemories(&SDDS_output);
        if (rpntest_ptr->is_parameter) {
          rpn_clear();
          rpn(rpntest_ptr->expression);
          if (rpn_check_error())
            exit(EXIT_FAILURE);
          if (!pop_log(&test_result))
            SDDS_Bomb("aborted due to rpn logical stack/result error for parameter-based test");
          rpn_clear();
          if (!test_result) {
            if (!rpntest_ptr->autostop) {
              if (verbose)
                fputs("    * page failed rpn test--continuing to next page", stderr);
              skip_page = 1;
              continue;
            } else {
              if (verbose)
                fputs("    * page failed rpn test--ignore remainder of file", stderr);
              exit(EXIT_FAILURE);
            }
          }
        } else if (n_rows) {
          if (!SDDS_FilterRowsWithRpnTest(&SDDS_output, rpntest_ptr->expression)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          row_deletion = 1;
        }
        if (verbose)
          fprintf(stderr, "applied %s-based rpn test:\n    %s\n", rpntest_ptr->is_parameter ? "parameter" : "column", rpntest_ptr->expression);
        break;
      case IS_NUMBERTEST_DEFINITION:
        numbertest_ptr = (NUMBERTEST_DEFINITION *)definition[i].structure;
        if (numbertest_ptr->is_parameter) {
          skip_page = !ParameterScansAsNumber(&SDDS_output, numbertest_ptr->name, numbertest_ptr->flags & NUMSCANFILTER_INVERT);
        } else if (n_rows) {
          if (SDDS_FilterRowsByNumScan(&SDDS_output, numbertest_ptr->name, numbertest_ptr->flags) == -1) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          row_deletion = 1;
        }
        if (verbose)
          fprintf(stderr, "applied %s-based %s%snumber test\n", numbertest_ptr->is_parameter ? "parameter" : "column", numbertest_ptr->flags & NUMSCANFILTER_INVERT ? " inverted " : "", numbertest_ptr->flags & NUMSCANFILTER_STRICT ? " strict " : "");
        break;
      case IS_SYSTEM_DEFINITION:
        system_ptr = (SYSTEM_DEFINITION *)definition[i].structure;
        if (system_ptr->is_parameter) {
          if (!system_parameter_value(&SDDS_output, system_ptr->new_name, system_ptr->source)) {
            fprintf(stderr, "couldn't system %s to make %s\n", system_ptr->source, system_ptr->new_name);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        } else if (n_rows && !system_column_value(&SDDS_output, system_ptr->new_name, system_ptr->source)) {
          fprintf(stderr, "couldn't system %s to make %s\n", system_ptr->source, system_ptr->new_name);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
        if (verbose)
          fprintf(stderr, "systemed %s %s to produce %s\n", system_ptr->is_parameter ? "parameter" : "column", system_ptr->source, system_ptr->new_name);
        break;
      case IS_RPNEXPRESSION_DEFINITION:
        rpnexpression_ptr = (RPNEXPRESSION_DEFINITION *)definition[i].structure;
        if (rpnexpression_ptr->repeat == -1)
          continue;
        SDDS_StoreParametersInRpnMemories(&SDDS_output);
        SDDS_StoreColumnsInRpnArrays(&SDDS_output);
        rpn(rpnexpression_ptr->expression);
        if (rpn_check_error())
          exit(EXIT_FAILURE);
        rpn_clear();
        if (verbose)
          fprintf(stderr, "executed rpn expression:\n    %s\n", rpnexpression_ptr->expression);
        if (!rpnexpression_ptr->repeat)
          rpnexpression_ptr->repeat = -1; /* already done, don't repeat */
        break;
      case IS_CLIP_DEFINITION:
        if (n_rows) {
          clip_ptr = (CLIP_DEFINITION *)definition[i].structure;
          if (!SDDS_SetRowFlags(&SDDS_output, clip_ptr->invert ? 0 : 1) ||
              (clip_ptr->head &&
               !SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, (int64_t)0, (int64_t)(clip_ptr->head - 1), clip_ptr->invert ? 1 : 0)) ||
              (n_rows >= clip_ptr->tail && clip_ptr->tail && !SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, (int64_t)(n_rows - clip_ptr->tail), (int64_t)(n_rows - 1), clip_ptr->invert ? 1 : 0))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          row_deletion = 1;
        }
        break;
      case IS_FCLIP_DEFINITION:
        if (n_rows) {
          fclip_ptr = (FCLIP_DEFINITION *)definition[i].structure;
          fclip_ptr->head = fclip_ptr->fhead * n_rows;
          fclip_ptr->tail = fclip_ptr->ftail * n_rows;
          if (!SDDS_SetRowFlags(&SDDS_output, fclip_ptr->invert ? 0 : 1) ||
              (fclip_ptr->head &&
               !SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, (int64_t)0, (int64_t)(fclip_ptr->head - 1), fclip_ptr->invert ? 1 : 0)) ||
              (n_rows >= fclip_ptr->tail && fclip_ptr->tail && !SDDS_AssertRowFlags(&SDDS_output, SDDS_INDEX_LIMITS, (int64_t)(n_rows - fclip_ptr->tail), (int64_t)(n_rows - 1), fclip_ptr->invert ? 1 : 0))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          row_deletion = 1;
        }
        break;
      case IS_SPARSE_DEFINITION:
        if (n_rows) {
          int32_t *sparse_row_flag;
          sparse_ptr = (SPARSE_DEFINITION *)definition[i].structure;
          sparse_row_flag = tmalloc(sizeof(*sparse_row_flag) * n_rows);
          for (j = 0; j < n_rows; j++)
            sparse_row_flag[j] = 0;
          for (j = sparse_ptr->offset; j < n_rows; j += sparse_ptr->interval)
            sparse_row_flag[j] = 1;
          if (!SDDS_AssertRowFlags(&SDDS_output, SDDS_FLAG_ARRAY, sparse_row_flag, n_rows)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          free(sparse_row_flag);
          row_deletion = 1;
        }
        break;
      case IS_SAMPLE_DEFINITION:
        if (n_rows) {
          int32_t *sample_row_flag;
          sample_ptr = (SAMPLE_DEFINITION *)definition[i].structure;
          sample_row_flag = tmalloc(sizeof(*sample_row_flag) * n_rows);
          for (j = 0; j < n_rows; j++)
            sample_row_flag[j] = random_1(1) < sample_ptr->fraction ? 1 : 0;
          if (!SDDS_AssertRowFlags(&SDDS_output, SDDS_FLAG_ARRAY, sample_row_flag, n_rows)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          free(sample_row_flag);
          row_deletion = 1;
        }
        break;
      case IS_CAST_DEFINITION:
        cast_ptr = (CAST_DEFINITION *)definition[i].structure;
        if (cast_ptr->isParameter) {
          if (!cast_parameter_value(&SDDS_output, cast_ptr))
            exit(EXIT_FAILURE);
        } else if (n_rows && !cast_column_value(&SDDS_output, cast_ptr)) {
          exit(EXIT_FAILURE);
        }
        break;
      default:
        fprintf(stderr, "error: unknown definition type code %ld seen--this shouldn't happen!\n", definition[i].type);
        exit(EXIT_FAILURE);
        break;
      }
      if (n_rows && row_deletion) {
        if (i != (definitions - 1) && !SDDS_DeleteUnsetRows(&SDDS_output)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
        if ((n_rows = SDDS_CountRowsOfInterest(&SDDS_output)) <= 0 &&
            SDDS_ColumnCount(&SDDS_output) != 0) {
          if (!nowarnings)
            fprintf(stderr, "warning: no rows selected for page %" PRId32 "\n", SDDS_output.page_number);
          if (SDDS_NumberOfErrors())
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          continue;
        }
        if (verbose)
          fprintf(stderr, "    %" PRId64 " rows left\n", n_rows);
      }
    }
    if (skip_page)
      continue;
    if (verbose)
      fprintf(stderr, "    %" PRId64 " rows selected for output\n", n_rows);
    if (!SDDS_WritePage(&SDDS_output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  if (page_number == 0) {
    fprintf(stderr, "Error: problem reading input file\n");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (!SDDS_Terminate(&SDDS_input) || !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile))
    exit(EXIT_FAILURE);
  free_scanargs(&s_arg, argc);

  if (processing_definition)
    free_processing_definiton(processing_definition, processing_definitions);
  if (equation_definition)
    free_equation_definition(equation_definition, equation_definitions);

  return EXIT_SUCCESS;
}

void free_processing_definiton(PROCESSING_DEFINITION **processing_definition, long n) {
  long i;
  for (i = 0; i < n; i++) {
    if (processing_definition[i]->parameter_name)
      free(processing_definition[i]->parameter_name);
    if (processing_definition[i]->column_name)
      free(processing_definition[i]->column_name);
    if (processing_definition[i]->description)
      free(processing_definition[i]->description);
    if (processing_definition[i]->symbol)
      free(processing_definition[i]->symbol);
    if (processing_definition[i]->functionOf)
      free(processing_definition[i]->functionOf);
    if (processing_definition[i]->weightBy)
      free(processing_definition[i]->weightBy);
    if (processing_definition[i]->match_value)
      free(processing_definition[i]->match_value);
    if (processing_definition[i]->match_column)
      free(processing_definition[i]->match_column);
    free(processing_definition[i]);
  }
  if (processing_definition)
    free(processing_definition);
}

void free_equation_definition(EQUATION_DEFINITION **equation_definition, long n) {
  long i, j;
  for (i = 0; i < n; i++) {
    if (equation_definition[i]->text)
      free(equation_definition[i]->text);
    if (equation_definition[i]->name)
      free(equation_definition[i]->name);
    if (equation_definition[i]->equation)
      free(equation_definition[i]->equation);
    if (equation_definition[i]->udf_name && equation_definition[i]->udf_name != equation_definition[i]->equation)
      free(equation_definition[i]->udf_name);
    if (equation_definition[i]->editSelection)
      free(equation_definition[i]->editSelection);
    if (equation_definition[i]->select)
      free(equation_definition[i]->select);
    if (equation_definition[i]->argv) {
      for (j = 0; j < equation_definition[i]->argc; j++)
        free(equation_definition[i]->argv[j]);
      free(equation_definition[i]->argv);
    }
    free(equation_definition[i]);
  }
  if (equation_definition)
    free(equation_definition);
}

typedef struct
{
  long column;
  char *equation;
} COLUMN_LIST;

long SDDS_ComputeSetOfColumns(SDDS_DATASET *SDDS_dataset, long equ_begin, long equ_end, DEFINITION *definition) {
  EQUATION_DEFINITION *equation_ptr;
  long column;
  SDDS_LAYOUT *layout;
  double value;
  long equations_present;
  COLUMN_LIST *column_list;
  long column_list_ptr, max_column_list_counter;
  int64_t j;

  column_list = NULL;
  column_list_ptr = -1;
  max_column_list_counter = 0;
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ComputeColumn"))
    return (0);
  layout = &SDDS_dataset->layout;
  column_list = NULL;
  column_list_ptr = -1;
  max_column_list_counter = 0;
  for (equations_present = equ_begin; equations_present < equ_end; equations_present++) {
    equation_ptr = (EQUATION_DEFINITION *)definition[equations_present].structure;

    if (++column_list_ptr >= max_column_list_counter || !column_list)
      column_list = trealloc(column_list, sizeof(*column_list) * (max_column_list_counter += 4));
    column_list[column_list_ptr].column = SDDS_GetColumnIndex(SDDS_dataset, equation_ptr->name);
    if (equation_ptr->equation[0] == '@') {
      char **equation, *ptr, pfix[IFPF_BUF_SIZE];
      if (equation_ptr->equation[1] == '@') {
        /* Parameter contains algebraic notation */
        if (!(equation = SDDS_GetParameter(SDDS_dataset, equation_ptr->equation + 2, NULL)))
          SDDS_Bomb("unable to read parameter for equation definition");
        ptr = addOuterParentheses(*equation);
        if2pf(pfix, ptr, sizeof pfix);
        free(ptr);
        if (!SDDS_CopyString(equation, pfix)) {
          fprintf(stderr, "error: problem copying argument string\n");
          return 0;
        }
      } else {
        if (!(equation = SDDS_GetParameter(SDDS_dataset, equation_ptr->equation + 1, NULL)))
          SDDS_Bomb("unable to read parameter for equation definition");
      }

      create_udf(equation_ptr->udf_name, *equation);
      free(*equation);
    }
    cp_str(&column_list[column_list_ptr].equation, equation_ptr->udf_name);
    if (column_list[column_list_ptr].column < 0 || column_list[column_list_ptr].column >= layout->n_columns)
      return (0);
  }

  if (!SDDS_StoreParametersInRpnMemories(SDDS_dataset))
    return (0);
  if (!SDDS_StoreColumnsInRpnArrays(SDDS_dataset))
    return 0;

  if (table_number_mem == -1) {
    table_number_mem = rpn_create_mem("table_number", 0);
    i_page_mem = rpn_create_mem("i_page", 0);
    n_rows_mem = rpn_create_mem("n_rows", 0);
    i_row_mem = rpn_create_mem("i_row", 0);
  }

  rpn_store((double)SDDS_dataset->page_number, NULL, table_number_mem);
  rpn_store((double)SDDS_dataset->page_number, NULL, i_page_mem);
  rpn_store((double)SDDS_dataset->n_rows, NULL, n_rows_mem);

  for (j = 0; j < SDDS_dataset->n_rows; j++) {
#if defined(DEBUG)
    fprintf(stderr, "Working on row %ld\n", j);
#endif
    if (!SDDS_StoreRowInRpnMemories(SDDS_dataset, j))
      return (0);
    column_list_ptr = 0;
    for (equations_present = equ_begin; equations_present < equ_end; equations_present++, column_list_ptr++) {
      column = column_list[column_list_ptr].column;
      rpn_clear();
      rpn_store((double)j, NULL, i_row_mem);
      value = rpn(column_list[column_list_ptr].equation);
      rpn_store(value, NULL, layout->column_definition[column].memory_number);
      if (rpn_check_error()) {
        SDDS_SetError("Unable to compute rpn expression--rpn error (SDDS_ComputeDefinedColumn)");
        return (0);
      }
#if defined(DEBUG)
      fprintf(stderr, "computed row value: %s = %e\n", layout->column_definition[column].name, value);
#endif
      switch (layout->column_definition[column].type) {
      case SDDS_CHARACTER:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (char)value, -1);
        break;
      case SDDS_SHORT:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (short)value, -1);
        break;
      case SDDS_USHORT:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (unsigned short)value, -1);
        break;
      case SDDS_LONG:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (int32_t)value, -1);
        break;
      case SDDS_ULONG:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (uint32_t)value, -1);
        break;
      case SDDS_LONG64:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (int64_t)value, -1);
        break;
      case SDDS_ULONG64:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (uint64_t)value, -1);
        break;
      case SDDS_FLOAT:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (float)value, -1);
        break;
      case SDDS_DOUBLE:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (double)value, -1);
        break;
      case SDDS_LONGDOUBLE:
        SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (long double)value, -1);
        break;
      }
    }
  }

  return (1);
}

long SDDS_EvaluateColumn(SDDS_DATASET *SDDS_dataset, EVALUATE_DEFINITION *definition) {
  double value;
  char **equation;
  long type, column;
  int64_t j;

  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_ComputeColumn"))
    return (0);
  if (!SDDS_StoreParametersInRpnMemories(SDDS_dataset))
    return (0);
  if (!SDDS_StoreColumnsInRpnArrays(SDDS_dataset))
    return 0;

  if (table_number_mem == -1) {
    table_number_mem = rpn_create_mem("table_number", 0);
    i_page_mem = rpn_create_mem("i_page", 0);
    n_rows_mem = rpn_create_mem("n_rows", 0);
    i_row_mem = rpn_create_mem("i_row", 0);
  }

  rpn_store((double)SDDS_dataset->page_number, NULL, table_number_mem);
  rpn_store((double)SDDS_dataset->page_number, NULL, i_page_mem);
  rpn_store((double)SDDS_dataset->n_rows, NULL, n_rows_mem);

  if (!(equation = (char **)SDDS_GetColumn(SDDS_dataset, definition->source)))
    return 0;

  type = SDDS_GetColumnType(SDDS_dataset, column = SDDS_GetColumnIndex(SDDS_dataset, definition->name));

  for (j = 0; j < SDDS_dataset->n_rows; j++) {
    if (!SDDS_StoreRowInRpnMemories(SDDS_dataset, j))
      return (0);
    rpn_clear();
    rpn_store((double)j, NULL, i_row_mem);
    value = rpn(equation[j]);
    if (rpn_check_error()) {
      SDDS_SetError("Unable to compute rpn expression--rpn error (SDDS_EvaluateColumn)");
      return (0);
    }
    switch (type) {
    case SDDS_CHARACTER:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (char)value, -1);
      break;
    case SDDS_SHORT:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (short)value, -1);
      break;
    case SDDS_USHORT:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (unsigned short)value, -1);
      break;
    case SDDS_LONG:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (int32_t)value, -1);
      break;
    case SDDS_ULONG:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (uint32_t)value, -1);
      break;
    case SDDS_LONG64:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (int64_t)value, -1);
      break;
    case SDDS_ULONG64:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (uint64_t)value, -1);
      break;
    case SDDS_FLOAT:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (float)value, -1);
      break;
    case SDDS_DOUBLE:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (double)value, -1);
      break;
    case SDDS_LONGDOUBLE:
      SDDS_SetRowValues(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, j, column, (long double)value, -1);
      break;
    }
  }

  return (1);
}

long SDDS_EvaluateParameter(SDDS_DATASET *SDDS_dataset, EVALUATE_DEFINITION *definition) {
  double value;
  char **equation;
  long parameter;

  if (!SDDS_StoreParametersInRpnMemories(SDDS_dataset))
    return (0);

  if (table_number_mem == -1) {
    table_number_mem = rpn_create_mem("table_number", 0);
    i_page_mem = rpn_create_mem("i_page", 0);
    n_rows_mem = rpn_create_mem("n_rows", 0);
    i_row_mem = rpn_create_mem("i_row", 0);
  }

  rpn_store((double)SDDS_dataset->page_number, NULL, table_number_mem);
  rpn_store((double)SDDS_dataset->page_number, NULL, i_page_mem);
  rpn_store((double)SDDS_dataset->n_rows, NULL, n_rows_mem);

  if (!(equation = (char **)SDDS_GetParameter(SDDS_dataset, definition->source, NULL)))
    return 0;

  rpn_clear();
  value = rpn(*equation);
  fprintf(stderr, "value = %e\n", value);
  free(*equation);
  if (rpn_check_error()) {
    SDDS_SetError("Unable to compute rpn expression--rpn error (SDDS_EvaluateParameter)");
    return (0);
  }
  switch (SDDS_GetParameterType(SDDS_dataset, parameter = SDDS_GetParameterIndex(SDDS_dataset, definition->name))) {
  case SDDS_CHARACTER:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (char)value, -1);
    break;
  case SDDS_SHORT:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (short)value, -1);
    break;
  case SDDS_USHORT:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (unsigned short)value, -1);
    break;
  case SDDS_LONG:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (int32_t)value, -1);
    break;
  case SDDS_ULONG:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (uint32_t)value, -1);
    break;
  case SDDS_LONG64:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (int64_t)value, -1);
    break;
  case SDDS_ULONG64:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (uint64_t)value, -1);
    break;
  case SDDS_FLOAT:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (float)value, -1);
    break;
  case SDDS_DOUBLE:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (double)value, -1);
    break;
  case SDDS_LONGDOUBLE:
    SDDS_SetParameters(SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, parameter, (long double)value, -1);
    break;
  }

  return (1);
}

char **process_name_options(char **orig_name, long **orig_flag, long orig_names, char **delete, long deletes, char **retain,
                            long retains, STRING_PAIR *rename, long renames, EDIT_NAME_REQUEST *edit_request, long edit_requests) {
  long i, j;
  char **new_name;
  char *ptr;

  *orig_flag = tmalloc(sizeof(**orig_flag) * orig_names);
  for (i = 0; i < orig_names; i++)
    (*orig_flag)[i] = 1;

  if (deletes) {
    for (i = 0; i < deletes; i++) {
      ptr = expand_ranges(delete[i]);
      /* free(delete[i]); */
      delete[i] = ptr;
    }
    for (j = 0; j < orig_names; j++) {
      for (i = 0; i < deletes; i++) {
        if (wild_match(orig_name[j], delete[i])) {
          (*orig_flag)[j] = 0;
          break;
        }
      }
    }
  }

  if (retains) {
    for (i = 0; i < retains; i++) {
      ptr = expand_ranges(retain[i]);
      /* free(retain[i]); */
      retain[i] = ptr;
    }
    if (!deletes)
      for (j = 0; j < orig_names; j++)
        (*orig_flag)[j] = 0;
    for (j = 0; j < orig_names; j++) {
      if ((*orig_flag)[j])
        continue;
      for (i = 0; i < retains; i++) {
        if (wild_match(orig_name[j], retain[i])) {
          (*orig_flag)[j] = 1;
          break;
        }
      }
    }
  }

  new_name = tmalloc(sizeof(*new_name) * orig_names);
  for (j = 0; j < orig_names; j++) {
    for (i = 0; i < renames; i++) {
      if (strcmp(rename[i][0], orig_name[j]) == 0) {
        SDDS_CopyString(new_name + j, rename[i][1]);
        break;
      }
    }
    if (i == renames)
      SDDS_CopyString(new_name + j, orig_name[j]);
    for (i = 0; i < edit_requests; i++) {
      char edit_buffer[256];
      ptr = expand_ranges(edit_request[i].match_string);
      /*free(edit_request[i].match_string); */
      edit_request[i].match_string = ptr;
      if (wild_match(new_name[j], edit_request[i].match_string)) {
        strcpy(edit_buffer, new_name[j]);
        if (!edit_string(edit_buffer, edit_request[i].edit_string))
          SDDS_Bomb("error editing name");
        /*free(new_name[j]); */
        SDDS_CopyString(&new_name[j], edit_buffer);
      }
    }
  }

  return (new_name);
}
