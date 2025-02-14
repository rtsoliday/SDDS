/**
 * @file SDDSaps.h
 * @brief Header file for routines used by SDDS command-line applications.
 *
 * This header defines the macros, data structures, and function prototypes
 * utilized by SDDS (Self Describing Data Sets) command-line applications.
 * It includes definitions for processing various data types, handling
 * filtering and matching operations, managing output requests, and performing
 * data conversions and formatting.
 *
 * @details
 * The SDDSaps.h header provides essential components for building command-line
 * tools that interact with SDDS datasets. It includes:
 * - Macro definitions for different data classes and processing modes.
 * - Platform-specific adjustments for Windows environments.
 * - Data structures for managing items, parameters, equations, scans, edits,
 *   prints, processing definitions, conversions, filters, matches, and more.
 * - Function prototypes for processing columns, handling definitions, editing
 *   parameters and columns, formatting strings, casting values, and managing
 *   output requests.
 *
 * This header is integral for developers working on SDDS applications, ensuring
 * consistent data handling and processing across various command-line tools.
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

#define COLUMN_BASED 0
#define PARAMETER_BASED 1
#define ARRAY_BASED 2
#define DATA_CLASS_KEYWORDS 3

#if defined(_WIN32)
#  define sleep(sec) Sleep(sec * 1000)
/*#  define popen(x, y) _popen(x, y)*/
#  define pclose(x) _pclose(x)
#endif

extern char *data_class_keyword[DATA_CLASS_KEYWORDS];

typedef struct {
  char **name;
  long *type, items;
} IFITEM_LIST;

typedef struct {
  char *name, *format;
} LABEL_PARAMETER;

typedef struct {
  char *text, *name, *equation, *udf_name, *select, *editSelection, *exclude;
  long is_parameter;
  long redefinition;
  char **argv;
  long argc;
} EQUATION_DEFINITION;
#define IS_EQUATION_DEFINITION 0

typedef struct {
  char *text, *sscanf_string, *source, *new_name, *edit;
  long is_parameter;
} SCAN_DEFINITION;
#define IS_SCAN_DEFINITION 1

typedef struct {
  char *text, *edit_command, *source, *new_name;
  long is_parameter, reedit;
  char **argv;
  long argc;
} EDIT_DEFINITION;
#define IS_EDIT_DEFINITION 2

typedef struct {
  char *text, *printf_string, *new_name, **source;
  long sources, is_parameter, reprint;
  char *select, *editSelection, *exclude;
} PRINT_DEFINITION;
#define IS_PRINT_DEFINITION 3

typedef struct {
  char *parameter_name, *column_name, *description, *symbol, *lower_par, *upper_par;
  char *head_par, *tail_par, *fhead_par, *ftail_par, *offset_par, *factor_par;
  char *functionOf, *weightBy, *match_value, *match_column;
  double lowerLimit, upperLimit, offset, factor, fhead, ftail, topLimit, bottomLimit;
  double percentileLevel, binSize, defaultValue;
  int32_t head, tail;
  long type, outputType;
  long mode, memory_number;
  unsigned long flags;
} PROCESSING_DEFINITION;
#define IS_PROCESSING_DEFINITION 4
#define PROCESSING_LOLIM_GIVEN 0x000001UL
#define PROCESSING_UPLIM_GIVEN 0x000002UL
#define PROCESSING_INVERT_OFFSET 0x000004UL
#define PROCESSING_DESCRIP_GIVEN 0x000008UL
#define PROCESSING_FUNCOF_GIVEN 0x000010UL
#define PROCESSING_TAIL_GIVEN 0x000020UL
#define PROCESSING_HEAD_GIVEN 0x000040UL
#define PROCESSING_SYMBOL_GIVEN 0x000080UL
#define PROCESSING_WEIGHT_GIVEN 0x000100UL
#define PROCESSING_POSITION_GIVEN 0x000200UL
#define PROCESSING_OFFSET_GIVEN 0x000400UL
#define PROCESSING_FACTOR_GIVEN 0x000800UL
#define PROCESSING_FTAIL_GIVEN 0x001000UL
#define PROCESSING_FHEAD_GIVEN 0x002000UL
#define PROCESSING_TOPLIM_GIVEN 0x004000UL
#define PROCESSING_BOTLIM_GIVEN 0x008000UL
#define PROCESSING_PERCLEVEL_GIVEN 0x010000UL
#define PROCESSING_BINSIZE_GIVEN 0x020000UL
#define PROCESSING_MATCHCOLUMN_GIVEN 0x040000UL
#define PROCESSING_MATCHVALUE_GIVEN 0x080000UL
#define PROCESSING_OVERWRITE_GIVEN 0x100000UL
#define PROCESSING_DEFAULTVALUE_GIVEN 0x200000UL
#define PROCESSING_INVERT_FACTOR 0x400000UL

typedef struct {
  char *name, *new_units, *old_units;
  double factor;
  long is_parameter;
} CONVERSION_DEFINITION;
#define IS_CONVERSION_DEFINITION 5

typedef struct {
  char *name, *string;
  unsigned long logic;
} MATCH_TERM;

typedef struct {
  char *name, *upperPar, *lowerPar;
  double lower, upper;
  unsigned long logic;
} FILTER_TERM;

typedef struct {
  FILTER_TERM *filter_term;
  long filter_terms, is_parameter;
} FILTER_DEFINITION;
#define IS_FILTER_DEFINITION 6

typedef struct {
  MATCH_TERM *match_term;
  long match_terms, is_parameter;
} MATCH_DEFINITION;
#define IS_MATCH_DEFINITION 7

typedef struct {
  char *expression;
  long autostop, is_parameter;
} RPNTEST_DEFINITION;
#define IS_RPNTEST_DEFINITION 8

typedef struct {
  char *text, *source, *new_name;
  long is_parameter;
} SYSTEM_DEFINITION;
#define IS_SYSTEM_DEFINITION 9

typedef struct {
  char *expression;
  long repeat;
} RPNEXPRESSION_DEFINITION;
#define IS_RPNEXPRESSION_DEFINITION 10

typedef struct {
  int64_t head, tail;
  short invert;
} CLIP_DEFINITION;
#define IS_CLIP_DEFINITION 11

typedef struct {
  int64_t interval, offset;
} SPARSE_DEFINITION;
#define IS_SPARSE_DEFINITION 12

typedef struct {
  double fraction;
} SAMPLE_DEFINITION;
#define IS_SAMPLE_DEFINITION 13

typedef struct {
  char *name;
  short is_parameter;
  unsigned long flags;
} NUMBERTEST_DEFINITION;
#define IS_NUMBERTEST_DEFINITION 14

typedef struct {
  char *target, *source, *stringFormat, *doubleFormat, *longFormat;
  long is_parameter;
} FORMAT_DEFINITION;
#define IS_FORMAT_DEFINITION 15

typedef struct {
  char *source, *newName, *newTypeName;
  long isParameter;
  int32_t newType;
} CAST_DEFINITION;
#define IS_CAST_DEFINITION 16

typedef struct {
  int64_t head, tail;
  double fhead, ftail;
  short invert;
} FCLIP_DEFINITION;
#define IS_FCLIP_DEFINITION 17

typedef struct {
  char *name;
  double before, after;
  unsigned long flags;
  long is_parameter;
} TIME_FILTER_DEFINITION;
#define TIMEFILTER_BEFORE_GIVEN 0x00001
#define TIMEFILTER_AFTER_GIVEN 0x00002
#define TIMEFILTER_INVERT_GIVEN 0x00004

#define IS_TIME_FILTER_DEFINITION 18

typedef struct {
  char *text, *name, *source, **argv;
  long is_parameter;
} EVALUATE_DEFINITION;
#define IS_EVALUATE_DEFINITION 19

#define DEFINITION_TYPES 20

typedef struct {
  long type;
  void *structure;
} DEFINITION;

typedef struct {
  FILE *fp;
  char *item[4];
  long columns;
  int64_t points;
  long parameter_output;
  void **definitions;
  long counter;
} OUTPUT_REQUEST;

#define PROCESS_COLUMN_MEAN 0
#define PROCESS_COLUMN_RMS 1
#define PROCESS_COLUMN_SUM 2
#define PROCESS_COLUMN_STAND_DEV 3
#define PROCESS_COLUMN_MAD 4
#define PROCESS_COLUMN_MINIMUM 5
#define PROCESS_COLUMN_MAXIMUM 6
#define PROCESS_COLUMN_SMALLEST 7
#define PROCESS_COLUMN_LARGEST 8
#define PROCESS_COLUMN_FIRST 9
#define PROCESS_COLUMN_LAST 10
#define PROCESS_COLUMN_COUNT 11
#define PROCESS_COLUMN_SPREAD 12
#define PROCESS_COLUMN_MEDIAN 13
#define PROCESS_COLUMN_BASELEVEL 14
#define PROCESS_COLUMN_TOPLEVEL 15
#define PROCESS_COLUMN_AMPLITUDE 16
#define PROCESS_COLUMN_RISETIME 17
#define PROCESS_COLUMN_FALLTIME 18
#define PROCESS_COLUMN_FWHM 19
#define PROCESS_COLUMN_FWTM 20
#define PROCESS_COLUMN_CENTER 21
#define PROCESS_COLUMN_ZEROCROSSING 22
#define PROCESS_COLUMN_FWHA 23
#define PROCESS_COLUMN_FWTA 24
#define PROCESS_COLUMN_SIGMA 25
#define PROCESS_COLUMN_SLOPE 26
#define PROCESS_COLUMN_INTERCEPT 27
#define PROCESS_COLUMN_LFSD 28
#define PROCESS_COLUMN_QRANGE 29
#define PROCESS_COLUMN_DRANGE 30
#define PROCESS_COLUMN_PERCENTILE 31
#define PROCESS_COLUMN_MODE 32
#define PROCESS_COLUMN_INTEGRAL 33
#define PROCESS_COLUMN_PRODUCT 34
#define PROCESS_COLUMN_PRANGE 35
#define PROCESS_COLUMN_SIGNEDSMALLEST 36
#define PROCESS_COLUMN_SIGNEDLARGEST 37
#define PROCESS_COLUMN_GMINTEGRAL 38
#define PROCESS_COLUMN_CORRELATION 39
#define N_PROCESS_COLUMN_MODES 40
#if 0
extern char *process_column_mode[N_PROCESS_COLUMN_MODES];
extern char *process_column_name[N_PROCESS_COLUMN_MODES];
extern char *process_column_description[N_PROCESS_COLUMN_MODES];
#endif

extern char *addOuterParentheses(char *arg);
extern void show_process_modes(FILE *fp);
extern long process_column(SDDS_DATASET *Table, PROCESSING_DEFINITION *processing_ptr, double *result,
                           char **stringResult, long warnings, int threads);
extern char *process_string_column(SDDS_DATASET *Dataset, PROCESSING_DEFINITION *processing_ptr, long warnings);
extern long process_filter_request(FILTER_TERM **filter, char **argument, long arguments);
extern long process_match_request(MATCH_TERM **match, char **argument, long arguments);
extern void scan_label_parameter(LABEL_PARAMETER *label, char *string);
extern void show_matches(char *type, MATCH_TERM *match, long matches);
extern void show_filters(char *type, FILTER_TERM *filter, long filters);
extern EQUATION_DEFINITION *process_new_equation_definition(char **argument, long arguments);
extern EVALUATE_DEFINITION *process_new_evaluate_definition(char **argument, long arguments);
extern EVALUATE_DEFINITION *process_new_evalute_definition(char **argument, long arguments);
extern SCAN_DEFINITION *process_new_scan_definition(char **argument, long arguments);
extern CAST_DEFINITION *process_new_cast_definition(char **argument, long arguments);
extern EDIT_DEFINITION *process_new_edit_definition(char **argument, long arguments, short reedit);
extern PRINT_DEFINITION *process_new_print_definition(char **argument, long arguments);
extern FORMAT_DEFINITION *process_new_format_definition(char **argument, long arguments);
extern PROCESSING_DEFINITION *record_processing_definition(char **argument, long arguments);
extern PROCESSING_DEFINITION *copyProcessingDefinition(PROCESSING_DEFINITION *source);
extern void expandProcessingDefinitions(DEFINITION **definition, long *definitions, SDDS_DATASET *SDDS_dataset);
extern CONVERSION_DEFINITION *copyConversionDefinition(CONVERSION_DEFINITION *source);
extern void expandConversionDefinitions(DEFINITION **definition, long *definitions, SDDS_DATASET *SDDS_dataset);
extern void expandDefinitions(DEFINITION **definition, long *definitions, SDDS_DATASET *SDDS_dataset);

extern CONVERSION_DEFINITION *process_conversion_definition(char **argument, long arguments);
extern FILTER_DEFINITION *process_new_filter_definition(char **argument, long arguments);
extern TIME_FILTER_DEFINITION *process_new_time_filter_definition(char **argument, long arguments);
extern MATCH_DEFINITION *process_new_match_definition(char **argument, long arguments);
extern RPNTEST_DEFINITION *process_new_rpntest_definition(char **argument, long arguments);
extern NUMBERTEST_DEFINITION *process_new_numbertest_definition(char **argument, long arguments);
extern RPNEXPRESSION_DEFINITION *process_new_rpnexpression_definition(char **argument, long arguments);
extern CLIP_DEFINITION *process_new_clip_definition(char **argument, long arguments);
extern FCLIP_DEFINITION *process_new_fclip_definition(char **argument, long arguments);
extern SPARSE_DEFINITION *process_new_sparse_definition(char **argument, long arguments);
extern SAMPLE_DEFINITION *process_new_sample_definition(char **argument, long arguments);
extern SYSTEM_DEFINITION *process_new_system_definition(char **argument, long arguments);
extern OUTPUT_REQUEST *process_output_request(char **argument, long arguments, OUTPUT_REQUEST *last_request);
extern char *determine_item_name(char **argument, OUTPUT_REQUEST *last_request, long index);
extern void set_up_output(char *filename, OUTPUT_REQUEST *output, LABEL_PARAMETER *label_parameter, long label_parameters,
                          long separate_tables, long announce_openings, SDDS_DATASET *SDDS_dataset);
extern long complete_processing_definitions(PROCESSING_DEFINITION **processing_definition, long processing_definitions,
                                            SDDS_DATASET *SDDS_dataset);

extern long system_column_value(SDDS_DATASET *SDDS_dataset, char *target, char *source);
extern long system_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char *source);
extern long run_on_pipe(char *command, char *buffer, long buffer_length);

/* used for redefining parameters and columns using sddsprocess-style commandline arguments */
extern long SDDS_RedefineParameterCL(SDDS_DATASET *SDDS_dataset, char *parameter, char **argv, long argc);
extern long SDDS_RedefineColumnCL(SDDS_DATASET *SDDS_dataset, char *column, char **argv, long argc);

extern long edit_string(char *text, char *edit);

extern long reformatString(char *buffer, long bufferSize, char *string, char *stringFormat,
                           char *doubleFormat, char *longFormat);

extern long cast_column_value(SDDS_DATASET *SDDS_dataset, CAST_DEFINITION *cast);
extern long cast_parameter_value(SDDS_DATASET *SDDS_dataset, CAST_DEFINITION *cast);

void add_definition(DEFINITION **definition, long *definitions, void *structure, long type);
long check_ifitems(SDDS_DATASET *SDDS_dataset, IFITEM_LIST *ifitem, long desired, long announce);
long complete_cast_definition(SDDS_DATASET *SDDSout, CAST_DEFINITION *defi,
                              SDDS_DATASET *SDDSin);
long edit_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *edit_command);
long edit_column_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *edit_command);
long scan_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *format,
                          char *edit);
long scan_column_value(SDDS_DATASET *SDDS_dataset, char *target, char *source, char *format,
                       char *edit);
long print_parameter_value(SDDS_DATASET *SDDS_dataset, char *target, char **source, long sources, char *format);
long print_column_value(SDDS_DATASET *SDDS_dataset, char *target, char **source, long sources, char *format);
long format_parameter_value(SDDS_DATASET *SDDS_dataset, FORMAT_DEFINITION *definition);
long format_column_value(SDDS_DATASET *SDDS_dataset, FORMAT_DEFINITION *definition);
long ParameterScansAsNumber(SDDS_DATASET *dataset, char *name, short invert);

/* rpn function for getting logical values */
extern long pop_log(int32_t *logical);

#define is_logic_character(c) ((c) == '|' || (c) == '&' || (c) == '!')

void add_ifitem(IFITEM_LIST *ifitem, char **name, long names);

#include "scan.h"
long add_sddsfile_arguments(SCANNED_ARG **scanned, int argc);
