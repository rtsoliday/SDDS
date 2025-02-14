/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: SDDSaps.h
 * contents: header file for routines used by SDDS command-line applications
 * 
 * M. Borland, 1993.
 $Log: not supported by cvs2svn $
 Revision 1.7  2003/04/14 14:44:56  shang
 added TIME_FILTER_DEFINITION structure

 Revision 1.6  2002/08/14 17:24:50  soliday
 Added Open License

 Revision 1.5  1999/07/22 18:34:24  soliday
 Added WIN32 support

 Revision 1.4  1999/06/03 16:04:07  soliday
 Removed compiler warnings under linux.

 Revision 1.3  1998/08/25 15:05:23  borland
 New version with major changes to allow multiple scales on x and y axes.

 * Revision 1.2  1995/09/05  21:09:15  saunders
 * First test release of the SDDS1.5 package.
 *
 */

#define COLUMN_BASED 0
#define PARAMETER_BASED 1
#define ARRAY_BASED 2
#define DATA_CLASS_KEYWORDS 3
extern char *data_class_keyword[DATA_CLASS_KEYWORDS];

typedef struct {
    char **name;
    long *type, items;
    } IFITEM_LIST ;


typedef struct {
    char *name, *format;
    } LABEL_PARAMETER;

typedef struct {
    char *text, *name, *equation, *udf_name;
    long is_parameter;
    long redefinition;
    char **argv;
    long argc;
    } EQUATION_DEFINITION;
#define IS_EQUATION_DEFINITION 0

typedef struct {
    char *text, *sscanf_string, *source, *new_name;
    long is_parameter;
    } SCAN_DEFINITION;
#define IS_SCAN_DEFINITION 1

typedef struct {
    char *text, *edit_command, *source, *new_name;
    long is_parameter;
    } EDIT_DEFINITION;
#define IS_EDIT_DEFINITION 2

typedef struct {
    char *text, *printf_string, *new_name, **source;
    long sources, is_parameter;
    } PRINT_DEFINITION;
#define IS_PRINT_DEFINITION 3

typedef struct {
    char *parameter_name, *column_name, *description, *symbol; 
    char *functionOf, *weightBy;
    double lowerLimit, upperLimit, offset, factor, fhead, ftail;
    int32_t  head, tail;
    long type;
    long mode, memory_number;
    unsigned long flags;
    } PROCESSING_DEFINITION;
#define IS_PROCESSING_DEFINITION 4
#define PROCESSING_LOLIM_GIVEN   0x0001
#define PROCESSING_UPLIM_GIVEN   0x0002
#define PROCESSING_INVERT        0x0004
#define PROCESSING_DESCRIP_GIVEN 0x0008
#define PROCESSING_FUNCOF_GIVEN  0x0010
#define PROCESSING_TAIL_GIVEN    0x0020
#define PROCESSING_HEAD_GIVEN    0x0040
#define PROCESSING_SYMBOL_GIVEN  0x0080
#define PROCESSING_WEIGHT_GIVEN  0x0100
#define PROCESSING_POSITION_GIVEN 0x0200
#define PROCESSING_OFFSET_GIVEN   0x0400
#define PROCESSING_FACTOR_GIVEN   0x0800
#define PROCESSING_FTAIL_GIVEN    0x1000
#define PROCESSING_FHEAD_GIVEN    0x2000

typedef struct {
    char *name, *new_units, *old_units;
    double factor;
    long is_array;
    long is_column;
    long is_parameter;
    } CONVERSION_DEFINITION;
#define IS_CONVERSION_DEFINITION 5

typedef struct {
    char *name, *string;
    long logic;
    } MATCH_TERM;

typedef struct {
    char *name;
    double lower, upper;
    long logic;
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
    } RPNEXPRESSION_DEFINITION ;
#define IS_RPNEXPRESSION_DEFINITION 10

typedef struct {
    long head, tail;
    short invert;
    } CLIP_DEFINITION;
#define IS_CLIP_DEFINITION 11

typedef struct {
    long interval, offset;
    } SPARSE_DEFINITION;
#define IS_SPARSE_DEFINITION 12

typedef struct {
    double fraction;
    } SAMPLE_DEFINITION;
#define IS_SAMPLE_DEFINITION 13

typedef struct {
  char *name;
  double before, after;
  unsigned long flags;
  long is_parameter;
} TIME_FILTER_DEFINITION;
#define TIMEFILTER_BEFORE_GIVEN   0x00001
#define TIMEFILTER_AFTER_GIVEN   0x00002
#define TIMEFILTER_INVERT_GIVEN  0x00004

#define IS_TIME_FILTER_DEFINITION 14

#define DEFINITION_TYPES 15
/*
static char *definition_type_name[DEFINITION_TYPES] = {
    "define", "scan", "edit", "print", "process", "convert", "filter", "match", "test", "system", 
    "rpn_expression", "clip", "sparse", "sample"
        } ;
*/
typedef struct {
    long type;
    void *structure;
    } DEFINITION;

typedef struct {
    FILE *fp;
    char *item[4];
    long columns;
    long points;
    long parameter_output;
    void **definitions;
    long counter;
    } OUTPUT_REQUEST;


#define PROCESS_COLUMN_MEAN 0
#define PROCESS_COLUMN_RMS  1
#define PROCESS_COLUMN_SUM  2
#define PROCESS_COLUMN_STAND_DEV  3
#define PROCESS_COLUMN_MAD  4
#define PROCESS_COLUMN_MINIMUM  5
#define PROCESS_COLUMN_MAXIMUM  6
#define PROCESS_COLUMN_SMALLEST  7
#define PROCESS_COLUMN_LARGEST  8
#define PROCESS_COLUMN_FIRST  9
#define PROCESS_COLUMN_LAST  10
#define PROCESS_COLUMN_COUNT  11
#define PROCESS_COLUMN_SPREAD  12
#define PROCESS_COLUMN_MEDIAN  13
#define PROCESS_COLUMN_BASELEVEL  14
#define PROCESS_COLUMN_TOPLEVEL  15
#define PROCESS_COLUMN_AMPLITUDE  16
#define PROCESS_COLUMN_RISETIME 17
#define PROCESS_COLUMN_FALLTIME 18
#define PROCESS_COLUMN_FWHM 19
#define PROCESS_COLUMN_FWTM 20
#define PROCESS_COLUMN_CENTER 21
#define PROCESS_COLUMN_ZEROCROSSING 22
#define PROCESS_COLUMN_FWHA 23
#define PROCESS_COLUMN_FWTA 24
#define N_PROCESS_COLUMN_MODES  25
#if 0
extern char *process_column_mode[N_PROCESS_COLUMN_MODES];
extern char *process_column_name[N_PROCESS_COLUMN_MODES];
extern char *process_column_description[N_PROCESS_COLUMN_MODES];
#endif

extern void show_process_modes(FILE *fp);
extern long process_column(SDDS_DATASET *Table, PROCESSING_DEFINITION *processing_ptr, double *result);
extern char *process_string_column(SDDS_DATASET *Dataset, PROCESSING_DEFINITION *processing_ptr);
extern long process_filter_request(FILTER_TERM **filter, char **argument, long arguments);
extern TIME_FILTER_DEFINITION *process_new_time_filter_definition(char **argument, long arguments);
extern long process_match_request(MATCH_TERM **match, char **argument, long arguments);
extern void scan_label_parameter(LABEL_PARAMETER *label, char *string);
extern void show_matches(char *type, MATCH_TERM *match, long matches);
extern void show_filters(char *type, FILTER_TERM *filter, long filters);
extern EQUATION_DEFINITION *process_new_equation_definition(char **argument, long arguments);
extern SCAN_DEFINITION *process_new_scan_definition(char **argument, long arguments);
extern EDIT_DEFINITION *process_new_edit_definition(char **argument, long arguments);
extern PRINT_DEFINITION *process_new_print_definition(char **argument, long arguments);
extern PROCESSING_DEFINITION *record_processing_definition(char **argument, long arguments);
extern PROCESSING_DEFINITION *copyProcessingDefinition(PROCESSING_DEFINITION *source);
extern void expandProcessingDefinitions(DEFINITION **definition, long *definitions, SDDS_DATASET *SDDS_dataset);
extern CONVERSION_DEFINITION *process_conversion_definition(char **argument, long arguments);
extern FILTER_DEFINITION *process_new_filter_definition(char **argument, long arguments);
extern MATCH_DEFINITION *process_new_match_definition(char **argument, long arguments);
extern RPNTEST_DEFINITION *process_new_rpntest_definition(char **argument, long arguments);
extern RPNEXPRESSION_DEFINITION *process_new_rpnexpression_definition(char **argument, long arguments);
extern CLIP_DEFINITION *process_new_clip_definition(char **argument, long arguments);
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

long SDDS_RedefineParameter(SDDS_DATASET *SDDS_dataset, char *parameter, char **argv, long argc);
long SDDS_RedefineColumn(SDDS_DATASET *SDDS_dataset, char *column, char **argv, long argc);

extern long edit_string(char *text, char *edit);

/* rpn function for getting logical values */
extern long pop_log(long *logical);

#define is_logic_character(c) ((c)=='|' || (c)=='&' || (c)=='!')

void add_ifitem(IFITEM_LIST *ifitem, char **name, long names);

#include "scan.h"
long add_sddsfile_arguments(SCANNED_ARG **scanned, int argc);

#if defined(_WIN32)
#include <windows.h>
#define sleep(sec) Sleep(sec * 1000)
#define popen(x,y) _popen(x,y)
#define pclose(x) _pclose(x)
#endif

