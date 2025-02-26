/**
 * @file plaindata2sdds.c
 * @brief Converts plain data files to SDDS format.
 *
 * This program reads plain data files in ASCII or binary format and converts them 
 * to the structured SDDS (Self Describing Data Set) format. It supports extensive 
 * configuration options for handling input and output modes, customizing separators, 
 * skipping lines, defining parameters and columns, and more.
 *
 * @section Usage
 * ```
 * plaindata2sdds <input> <output>
 *                [-pipe=[input][,output]] 
 *                [-inputMode=<ascii|binary>] 
 *                [-outputMode=<ascii|binary>] 
 *                [-separator=<char>] 
 *                [-commentCharacters=<chars>] 
 *                [-noRowCount] 
 *                [-binaryRows=<rowcount>] 
 *                [-order=<rowMajor|columnMajor>] 
 *                [-parameter=<name>,<type>[,units=<string>][,description=<string>][,symbol=<string>][,count=<integer>]...] 
 *                [-column=<name>,<type>[,units=<string>][,description=<string>][,symbol=<string>][,count=<integer>]...] 
 *                [-skipcolumn=<type>] 
 *                [-nowarnings] 
 *                [-fillin] 
 *                [-skiplines=<integer>] 
 *                [-eofSequence=<string>] 
 *                [-majorOrder=row|column] 
 * ```
 *
 * @section Options
 * | Option                | Description                                                                                     |
 * |-----------------------|-------------------------------------------------------------------------------------------------|
 * | `-pipe`               | Enables piping for input and/or output. Use `-pipe=input,output` for both.                      |
 * | `-inputMode`          | Specifies the input file format: ASCII or binary.                                               |
 * | `-outputMode`         | Specifies the output SDDS file format: ASCII or binary.                                         |
 * | `-separator`          | Defines the column separator in ASCII mode. Defaults to whitespace.                             |
 * | `-commentCharacters`  | Specifies characters that indicate comments in the input file.                                  |
 * | `-noRowCount`         | Indicates that the input file does not include a row count.                                     |
 * | `-binaryRows`         | Specifies the number of rows for binary input files without a row count.                        |
 * | `-order`              | Sets the data storage order in the input file: rowMajor or columnMajor.                         |
 * | `-parameter`          | Defines parameters with attributes such as name, type, units, description, and symbol.          |
 * | `-column`             | Defines columns with attributes such as name, type, units, description, and symbol.             |
 * | `-skipcolumn`         | Skips a specific type of column during parsing.                                                 |
 * | `-nowarnings`         | Suppresses warning messages during execution.                                                   |
 * | `-fillin`             | Fills blank entries with default values (0 for numeric, empty string for string columns).       |
 * | `-skiplines`          | Skips the first `n` header lines in the input file.                                             |
 * | `-eofSequence`        | Stops parsing when the specified sequence is found at the start of a line.                      |
 * | `-majorOrder`         | Specifies the major order for writing the output file: row or column.                           |
 *
 * @subsection Incompatibilities
 * - `-binaryRows` is incompatible with:
 *   - `-noRowCount`
 * - `-skiplines` does not work with binary input files.
 *
 * @subsection SR Specific Requirements
 * - `-separator` requires a single character.
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
 *  R. Soliday, M. Borland, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include <ctype.h>
#include <signal.h>
#if defined(_WIN32)
#  include <io.h>
#  include <fcntl.h>
#endif

#define ASCII_MODE 0
#define BINARY_MODE 1
#define MODES 2
static char *mode_name[MODES] = {
  "ascii",
  "binary",
};

#define TYPE_SHORT 0
#define TYPE_LONG 1
#define TYPE_LONG64 2
#define TYPE_FLOAT 3
#define TYPE_LONGDOUBLE 4
#define TYPE_DOUBLE 5
#define TYPE_STRING 6
#define TYPE_CHARACTER 7
#define DATATYPES 8
static char *type_name[DATATYPES] = {
  "short",
  "long",
  "long64",
  "float",
  "longdouble",
  "double",
  "string",
  "character",
};

#define HEADER_UNITS 0
#define HEADER_DESCRIPTION 1
#define HEADER_SYMBOL 2
#define HEADER_COUNT 3
#define HEADERELEMENTS 4
static char *header_elements[HEADERELEMENTS] = {
  "units", "description", "symbol", "count"
};

typedef struct
{
  void *values;
  long elements;
  char **stringValues;
  char *units;
  char *description;
  char *symbol;
  char *name;
  long type;
  short skip;
} COLUMN_DATA_STRUCTURES;

typedef struct
{
  char *units;
  char *description;
  char *symbol;
  char *name;
  long type;
} PARAMETER_DATA_STRUCTURES;

/* Enumeration for option types */
enum option_type {
  SET_INPUTMODE,
  SET_OUTPUTMODE,
  SET_SEPARATOR,
  SET_NOROWCOUNT,
  SET_PARAMETER,
  SET_COLUMN,
  SET_PIPE,
  SET_NOWARNINGS,
  SET_ORDER,
  SET_FILLIN,
  SET_SKIPLINES,
  SET_SKIPCOLUMN,
  SET_COMMENT,
  SET_MAJOR_ORDER,
  SET_BINARY_ROWS,
  SET_EOF_SEQUENCE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "inputMode",
  "outputMode",
  "separator",
  "noRowCount",
  "parameter",
  "column",
  "pipe",
  "nowarnings",
  "order",
  "fillin",
  "skiplines",
  "skipcolumn",
  "commentCharacters",
  "majorOrder",
  "binaryRows",
  "eofsequence"
};

#define ROW_ORDER 0
#define COLUMN_ORDER 1
#define ORDERS 2
static char *order_names[ORDERS] = {
  "rowMajor",
  "columnMajor",
};

/* Improved Usage Message */
char *USAGE =
  "plaindata2sdds [<input>] [<output>]\n"
  "               [-pipe=[input][,output]]\n"
  "               [-inputMode=<ascii|binary>]\n"
  "               [-outputMode=<ascii|binary>]\n"
  "               [-separator=<char>]\n"
  "               [-commentCharacters=<chars>]\n"
  "               [-noRowCount]\n"
  "               [-binaryRows=<rowcount>]\n"
  "               [-order=<rowMajor|columnMajor>]\n"
  "               [-parameter=<name>,<type>[,units=<string>][,description=<string>][,symbol=<string>][,count=<integer>]...]\n"
  "               [-column=<name>,<type>[,units=<string>][,description=<string>][,symbol=<string>][,count=<integer>]...]\n"
  "               [-skipcolumn=<type>]\n"
  "               [-skiplines=<integer>]\n"
  "               [-eofSequence=<string>]\n"
  "               [-majorOrder=<row|column>]\n"
  "               [-fillin]\n"
  "               [-nowarnings]\n\n"
  "Options:\n"
  "  -inputMode        The plain data file can be read in ascii or binary format.\n"
  "  -outputMode       The SDDS data file can be written in ascii or binary format.\n"
  "  -separator        In ascii mode, columns of the plain data file are separated by the given character.\n"
  "                    By default, any combination of whitespace characters is used.\n"
  "  -commentCharacters Characters that denote comments. Lines starting with these are ignored.\n"
  "  -noRowCount       The number of rows is not included in the plain data file.\n"
  "                    If the plain data file is binary, the row count must be set using -binaryRows.\n"
  "  -binaryRows       The number of rows in a binary file without an explicit row count.\n"
  "  -order            Specifies the order of data storage in the input file.\n"
  "                    - rowMajor (default): Each row consists of one element from each column.\n"
  "                    - columnMajor: Each column is located entirely on one row.\n"
  "  -parameter        Add this option for each parameter in the plain data file.\n"
  "  -column           Add this option for each column in the plain data file.\n"
  "  -skipcolumn       Add this option to skip over a column in the plain data file.\n"
  "  -skiplines        Add this option to skip a specified number of header lines.\n"
  "  -eofSequence      Stop parsing the file when this sequence is found at the start of a line.\n"
  "  -majorOrder       Specifies the major order for writing the output file (row or column).\n"
  "  -fillin           Fill in blanks with default values (0 for numeric columns, empty string for string columns).\n"
  "  -nowarnings       Suppress warning messages during execution.\n\n"
  "Program by Robert Soliday. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

/* Function Prototypes */
void SetColumnData(long type, SDDS_DATASET *dataset, void *values, int32_t rows, int32_t index);
void *AllocateColumnData(long type, void *values, int32_t rows);
char **AllocateColumnStringData(char **values, int32_t rows, int32_t previous_rows);
long getToken(char *s, char *buffer, long buflen, char separator, long whitespace);
void ConvertDNotationToENotation(char *line);
void interrupt_handler(int sig);

void interrupt_handler(int sig) {
  fprintf(stderr, "Segmentation fault. Ensure that your input file matches the -inputMode given.\n");
  exit(EXIT_FAILURE);
}

/* ********** */

int main(int argc, char **argv) {
  FILE *fileID;
  COLUMN_DATA_STRUCTURES *columnValues;
  PARAMETER_DATA_STRUCTURES *parameterValues;

  SDDS_DATASET SDDS_dataset;
  SCANNED_ARG *s_arg;
  long i, j, k, n = 0, i_arg;
  int32_t rows, binaryRows = -1;
  int32_t maxRows = 10000, initRows = 10000, row;
  long par, col, page, size, readline = 1, fillin = 0;
  int32_t ptrSize = 0;
  char *input, *output, s[1024], *ptr, *ptr2, data[10240], temp[10240], *eofSequence;
  unsigned long pipeFlags = 0, majorOrderFlag;
  long noWarnings = 0, tmpfile_used = 0, columnOrder = 0, whitespace = 1;
  short shortValue, stop;
  int32_t longValue;
  int64_t long64Value;
  float floatValue;
  long double ldoubleValue;
  double doubleValue;
  char stringValue[SDDS_MAXLINE];
  char characterValue;
  char buffer[124], buffer2[200];
  long *parameterIndex, *columnIndex;

  long binary = 0, noRowCount = 0, inputBinary = 0, count = 0;
  char separator;
  char commentCharacters[20];
  short checkComment = 0;
  short commentFound;
  long parameters = 0, columns = 0;
  long skiplines = 0;
  short abort_flag = 0, recover = 1, columnMajorOrder = 0;

  input = output = NULL;
  separator = ' ';
  eofSequence = NULL;
  columnValues = NULL;
  parameterValues = NULL;

  parameterIndex = columnIndex = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  signal(SIGSEGV, interrupt_handler);

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
      case SET_OUTPUTMODE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -outputMode syntax");
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case ASCII_MODE:
          binary = 0;
          break;
        case BINARY_MODE:
          binary = 1;
          break;
        default:
          SDDS_Bomb("invalid -outputMode syntax");
          break;
        }
        break;
      case SET_INPUTMODE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -inputMode syntax");
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case ASCII_MODE:
          inputBinary = 0;
          break;
        case BINARY_MODE:
          inputBinary = 1;
          break;
        default:
          SDDS_Bomb("invalid -inputMode syntax");
          break;
        }
        break;
      case SET_SEPARATOR:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -separator syntax");
        separator = s_arg[i_arg].list[1][0];
        whitespace = 0;
        break;
      case SET_COMMENT:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -commentCharacters syntax");
        strncpy(commentCharacters, s_arg[i_arg].list[1], sizeof(commentCharacters) - 1);
        commentCharacters[sizeof(commentCharacters) - 1] = '\0';
        checkComment = 1;
        break;
      case SET_FILLIN:
        fillin = 1;
        break;
      case SET_NOROWCOUNT:
        if (s_arg[i_arg].n_items != 1)
          SDDS_Bomb("invalid -noRowCount syntax");
        noRowCount = 1;
        break;
      case SET_ORDER:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -order syntax");
        switch (match_string(s_arg[i_arg].list[1], order_names, ORDERS, 0)) {
        case ROW_ORDER:
          columnOrder = 0;
          break;
        case COLUMN_ORDER:
          columnOrder = 1;
          break;
        default:
          SDDS_Bomb("invalid -order syntax");
          break;
        }
        break;
      case SET_PARAMETER:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -parameter syntax");
        count = 1;
        parameters++;
        parameterValues = trealloc(parameterValues, sizeof(*parameterValues) * (parameters));
        SDDS_CopyString(&parameterValues[parameters - 1].name, s_arg[i_arg].list[1]);
        parameterValues[parameters - 1].units = NULL;
        parameterValues[parameters - 1].description = NULL;
        parameterValues[parameters - 1].symbol = NULL;
        switch (match_string(s_arg[i_arg].list[2], type_name, DATATYPES, MATCH_WHOLE_STRING)) {
        case TYPE_SHORT:
          parameterValues[parameters - 1].type = SDDS_SHORT;
          break;
        case TYPE_LONG:
          parameterValues[parameters - 1].type = SDDS_LONG;
          break;
        case TYPE_LONG64:
          parameterValues[parameters - 1].type = SDDS_LONG64;
          break;
        case TYPE_FLOAT:
          parameterValues[parameters - 1].type = SDDS_FLOAT;
          break;
        case TYPE_LONGDOUBLE:
          parameterValues[parameters - 1].type = SDDS_LONGDOUBLE;
          break;
        case TYPE_DOUBLE:
          parameterValues[parameters - 1].type = SDDS_DOUBLE;
          break;
        case TYPE_STRING:
          parameterValues[parameters - 1].type = SDDS_STRING;
          break;
        case TYPE_CHARACTER:
          parameterValues[parameters - 1].type = SDDS_CHARACTER;
          break;
        default:
          SDDS_Bomb("invalid -parameter type");
          break;
        }
        for (i = 3; i < s_arg[i_arg].n_items; i++) {
          if (!(ptr = strchr(s_arg[i_arg].list[i], '=')))
            SDDS_Bomb("invalid -parameter syntax");
          *ptr++ = 0;
          switch (match_string(s_arg[i_arg].list[i], header_elements, HEADERELEMENTS, 0)) {
          case HEADER_UNITS:
            SDDS_CopyString(&parameterValues[parameters - 1].units, ptr);
            break;
          case HEADER_DESCRIPTION:
            SDDS_CopyString(&parameterValues[parameters - 1].description, ptr);
            break;
          case HEADER_SYMBOL:
            SDDS_CopyString(&parameterValues[parameters - 1].symbol, ptr);
            break;
          case HEADER_COUNT:
            if (sscanf(ptr, "%ld", &count) != 1 || count <= 0)
              SDDS_Bomb("invalid parameter count value");
            snprintf(buffer, sizeof(buffer), "%s", parameterValues[parameters - 1].name);
            snprintf(buffer2, sizeof(buffer2), "%s1", buffer);
            free(parameterValues[parameters - 1].name);
            SDDS_CopyString(&parameterValues[parameters - 1].name, buffer2);
            break;
          default:
            SDDS_Bomb("invalid -parameter syntax");
            break;
          }
        }

        for (i = 2; i <= count; i++) {
          parameters++;
          parameterValues = trealloc(parameterValues, sizeof(*parameterValues) * (parameters));
          snprintf(buffer2, sizeof(buffer2), "%s%ld", buffer, i);
          SDDS_CopyString(&parameterValues[parameters - 1].name, buffer2);
          parameterValues[parameters - 1].units = NULL;
          parameterValues[parameters - 1].description = NULL;
          parameterValues[parameters - 1].symbol = NULL;
          parameterValues[parameters - 1].type = parameterValues[parameters - 2].type;
          if (parameterValues[parameters - 2].units)
            SDDS_CopyString(&parameterValues[parameters - 1].units, parameterValues[parameters - 2].units);
          if (parameterValues[parameters - 2].description)
            SDDS_CopyString(&parameterValues[parameters - 1].description, parameterValues[parameters - 2].description);
          if (parameterValues[parameters - 2].symbol)
            SDDS_CopyString(&parameterValues[parameters - 1].symbol, parameterValues[parameters - 2].symbol);
        }

        break;
      case SET_COLUMN:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -column syntax");
        count = 1;
        columns++;
        columnValues = trealloc(columnValues, sizeof(*columnValues) * (columns));
        SDDS_CopyString(&columnValues[columns - 1].name, s_arg[i_arg].list[1]);
        columnValues[columns - 1].elements = 0;
        columnValues[columns - 1].values = NULL;
        columnValues[columns - 1].stringValues = NULL;
        columnValues[columns - 1].units = NULL;
        columnValues[columns - 1].description = NULL;
        columnValues[columns - 1].symbol = NULL;
        columnValues[columns - 1].skip = 0;

        switch (match_string(s_arg[i_arg].list[2], type_name, DATATYPES, MATCH_WHOLE_STRING)) {
        case TYPE_SHORT:
          columnValues[columns - 1].type = SDDS_SHORT;
          break;
        case TYPE_LONG:
          columnValues[columns - 1].type = SDDS_LONG;
          break;
        case TYPE_LONG64:
          columnValues[columns - 1].type = SDDS_LONG64;
          break;
        case TYPE_FLOAT:
          columnValues[columns - 1].type = SDDS_FLOAT;
          break;
        case TYPE_LONGDOUBLE:
          columnValues[columns - 1].type = SDDS_LONGDOUBLE;
          break;
        case TYPE_DOUBLE:
          columnValues[columns - 1].type = SDDS_DOUBLE;
          break;
        case TYPE_STRING:
          columnValues[columns - 1].type = SDDS_STRING;
          break;
        case TYPE_CHARACTER:
          columnValues[columns - 1].type = SDDS_CHARACTER;
          break;
        default:
          SDDS_Bomb("invalid -column type");
          break;
        }
        for (i = 3; i < s_arg[i_arg].n_items; i++) {
          if (!(ptr = strchr(s_arg[i_arg].list[i], '=')))
            SDDS_Bomb("invalid -column syntax");
          *ptr++ = 0;
          switch (match_string(s_arg[i_arg].list[i], header_elements, HEADERELEMENTS, 0)) {
          case HEADER_UNITS:
            SDDS_CopyString(&columnValues[columns - 1].units, ptr);
            break;
          case HEADER_DESCRIPTION:
            SDDS_CopyString(&columnValues[columns - 1].description, ptr);
            break;
          case HEADER_SYMBOL:
            SDDS_CopyString(&columnValues[columns - 1].symbol, ptr);
            break;
          case HEADER_COUNT:
            if (sscanf(ptr, "%ld", &count) != 1 || count <= 0)
              SDDS_Bomb("invalid column count value");
            snprintf(buffer, sizeof(buffer), "%s", columnValues[columns - 1].name);
            snprintf(buffer2, sizeof(buffer2), "%s1", buffer);
            free(columnValues[columns - 1].name);
            SDDS_CopyString(&columnValues[columns - 1].name, buffer2);
            break;
          default:
            SDDS_Bomb("invalid -column syntax");
            break;
          }
        }

        for (i = 2; i <= count; i++) {
          columns++;
          columnValues = trealloc(columnValues, sizeof(*columnValues) * (columns));
          snprintf(buffer2, sizeof(buffer2), "%s%ld", buffer, i);
          SDDS_CopyString(&columnValues[columns - 1].name, buffer2);
          columnValues[columns - 1].elements = 0;
          columnValues[columns - 1].values = NULL;
          columnValues[columns - 1].stringValues = NULL;
          columnValues[columns - 1].units = NULL;
          columnValues[columns - 1].description = NULL;
          columnValues[columns - 1].symbol = NULL;
          columnValues[columns - 1].type = columnValues[columns - 2].type;
          if (columnValues[columns - 2].units)
            SDDS_CopyString(&columnValues[columns - 1].units, columnValues[columns - 2].units);
          if (columnValues[columns - 2].description)
            SDDS_CopyString(&columnValues[columns - 1].description, columnValues[columns - 2].description);
          if (columnValues[columns - 2].symbol)
            SDDS_CopyString(&columnValues[columns - 1].symbol, columnValues[columns - 2].symbol);
        }

        break;
      case SET_SKIPCOLUMN:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -skipcolumn syntax");
        count = 1;
        columns++;
        columnValues = trealloc(columnValues, sizeof(*columnValues) * (columns));
        columnValues[columns - 1].name = NULL;
        columnValues[columns - 1].elements = 0;
        columnValues[columns - 1].values = NULL;
        columnValues[columns - 1].stringValues = NULL;
        columnValues[columns - 1].units = NULL;
        columnValues[columns - 1].description = NULL;
        columnValues[columns - 1].symbol = NULL;
        columnValues[columns - 1].skip = 1;

        switch (match_string(s_arg[i_arg].list[1], type_name, DATATYPES, MATCH_WHOLE_STRING)) {
        case TYPE_SHORT:
          columnValues[columns - 1].type = SDDS_SHORT;
          break;
        case TYPE_LONG:
          columnValues[columns - 1].type = SDDS_LONG;
          break;
        case TYPE_LONG64:
          columnValues[columns - 1].type = SDDS_LONG64;
          break;
        case TYPE_FLOAT:
          columnValues[columns - 1].type = SDDS_FLOAT;
          break;
        case TYPE_LONGDOUBLE:
          columnValues[columns - 1].type = SDDS_LONGDOUBLE;
          break;
        case TYPE_DOUBLE:
          columnValues[columns - 1].type = SDDS_DOUBLE;
          break;
        case TYPE_STRING:
          columnValues[columns - 1].type = SDDS_STRING;
          break;
        case TYPE_CHARACTER:
          columnValues[columns - 1].type = SDDS_CHARACTER;
          break;
        default:
          SDDS_Bomb("invalid -skipcolumn type");
          break;
        }
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_NOWARNINGS:
        if (s_arg[i_arg].n_items != 1)
          SDDS_Bomb("invalid -nowarnings syntax");
        noWarnings = 1;
        break;
      case SET_SKIPLINES:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &skiplines) != 1 || skiplines <= 0)
          SDDS_Bomb("invalid -skiplines syntax");
        break;
      case SET_BINARY_ROWS:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%" SCNd32, &binaryRows) != 1 || binaryRows < 0)
          SDDS_Bomb("invalid -binaryRows syntax");
        break;
      case SET_EOF_SEQUENCE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -eofSequence syntax");
        eofSequence = s_arg[i_arg].list[1];
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (input == NULL) {
        input = s_arg[i_arg].list[0];
      } else if (output == NULL) {
        output = s_arg[i_arg].list[0];
      } else {
        fprintf(stderr, "too many filenames\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  processFilenames("plaindata2sdds", &input, &output, pipeFlags, noWarnings, &tmpfile_used);

  if (!columns && !parameters)
    SDDS_Bomb("you must specify one of the -column or the -parameter options");

  if (skiplines && inputBinary)
    SDDS_Bomb("-skiplines does not work with binary input files");

  if (!input) {
    if (inputBinary) {
#if defined(_WIN32)
      if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
        fprintf(stderr, "error: unable to set stdin to binary mode\n");
        exit(EXIT_FAILURE);
      }
#endif
    }
    fileID = stdin;
  } else {
    if (!fexists(input)) {
      fprintf(stderr, "input file not found\n");
      exit(EXIT_FAILURE);
    }
    if (inputBinary) {
      fileID = fopen(input, "rb");
    } else {
      fileID = fopen(input, "r");
    }
  }
  if (fileID == NULL) {
    fprintf(stderr, "unable to open input file for reading\n");
    exit(EXIT_FAILURE);
  }

  if (!SDDS_InitializeOutput(&SDDS_dataset, binary ? SDDS_BINARY : SDDS_ASCII, 1, NULL, NULL, output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  SDDS_dataset.layout.data_mode.column_major = columnMajorOrder;

  if (parameters) {
    parameterIndex = tmalloc(sizeof(*parameterIndex) * parameters);
  }
  if (columns) {
    columnIndex = tmalloc(sizeof(*columnIndex) * columns);
  }

  for (i = 0; i < parameters; i++) {
    if ((parameterIndex[i] = SDDS_DefineParameter(&SDDS_dataset, parameterValues[i].name, parameterValues[i].symbol, parameterValues[i].units, parameterValues[i].description, NULL, parameterValues[i].type, 0)) < 0) {
      snprintf(s, sizeof(s), "Problem defining parameter %s.", parameterValues[i].name);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  for (i = 0; i < columns; i++) {
    if (columnValues[i].skip)
      continue;
    if ((columnIndex[i] = SDDS_DefineColumn(&SDDS_dataset, columnValues[i].name, columnValues[i].symbol, columnValues[i].units, columnValues[i].description, NULL, columnValues[i].type, 0)) < 0) {
      snprintf(s, sizeof(s), "Problem defining column %s.", columnValues[i].name);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_WriteLayout(&SDDS_dataset))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_StartPage(&SDDS_dataset, initRows))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  row = par = col = page = 0;
  while (inputBinary) {
    row = par = col = 0;

    if (binaryRows == -1) {
      if (fread(&rows, sizeof(rows), 1, fileID) != 1) {
        if (page == 0) {
          SDDS_SetError("Unable to read number of rows");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        } else {
          if (!SDDS_Terminate(&SDDS_dataset))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          for (k = 0; k < columns; k++) {
            if (columnValues[k].type == SDDS_STRING) {
              SDDS_FreeStringArray(columnValues[k].stringValues, columnValues[k].elements);
            } else {
              free(columnValues[k].values);
            }
          }
          return EXIT_SUCCESS;
        }
      }
    } else {
      if (page == 0) {
        rows = binaryRows;
      } else {
        if (!SDDS_Terminate(&SDDS_dataset))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (k = 0; k < columns; k++) {
          if (columnValues[k].type == SDDS_STRING) {
            SDDS_FreeStringArray(columnValues[k].stringValues, columnValues[k].elements);
          } else {
            free(columnValues[k].values);
          }
        }
        return EXIT_SUCCESS;
      }
    }
    page++;

    for (par = 0; par < parameters; par++) {
      switch (parameterValues[par].type) {
      case SDDS_SHORT:
        if (fread(&shortValue, sizeof(shortValue), 1, fileID) != 1) {
          SDDS_SetError("Unable to read short parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, shortValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_LONG:
        if (fread(&longValue, sizeof(longValue), 1, fileID) != 1) {
          SDDS_SetError("Unable to read long parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, longValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_LONG64:
        if (fread(&long64Value, sizeof(long64Value), 1, fileID) != 1) {
          SDDS_SetError("Unable to read long64 parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, long64Value, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_FLOAT:
        if (fread(&floatValue, sizeof(floatValue), 1, fileID) != 1) {
          SDDS_SetError("Unable to read float parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, floatValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_DOUBLE:
        if (fread(&doubleValue, sizeof(doubleValue), 1, fileID) != 1) {
          SDDS_SetError("Unable to read double parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, doubleValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_LONGDOUBLE:
        if (fread(&ldoubleValue, sizeof(ldoubleValue), 1, fileID) != 1) {
          SDDS_SetError("Unable to read long double parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, ldoubleValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_STRING:
        if (fread(&size, sizeof(size), 1, fileID) != 1) {
          SDDS_SetError("Unable to read string parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (size > SDDS_MAXLINE - 1)
          SDDS_Bomb("String is too long");
        if (size > 0) {
          if (fread(&stringValue, size, 1, fileID) != 1) {
            SDDS_SetError("Unable to read string parameter");
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          stringValue[size] = '\0';
        } else {
          strcpy(stringValue, "");
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, stringValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_CHARACTER:
        if (fread(&characterValue, sizeof(characterValue), 1, fileID) != 1) {
          SDDS_SetError("Unable to read character parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, characterValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      }
    }
    for (i = 0; i < columns; i++) {
      if (columnValues[i].skip)
        continue;
      if (columnValues[i].type == SDDS_STRING) {
        columnValues[i].stringValues = AllocateColumnStringData(columnValues[i].stringValues, rows, columnValues[i].elements);
      } else {
        columnValues[i].values = AllocateColumnData(columnValues[i].type, columnValues[i].values, rows);
      }
      columnValues[i].elements = rows;
    }
    if (columnOrder) {
      for (col = 0; col < columns; col++) {
        switch (columnValues[col].type) {
        case SDDS_SHORT:
          for (i = 0; i < rows; i++) {
            if (fread((short *)(columnValues[col].values) + i, sizeof(short), 1, fileID) != 1) {
              SDDS_SetError("Unable to read short column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_LONG:
          for (i = 0; i < rows; i++) {
            if (fread((int32_t *)(columnValues[col].values) + i, sizeof(int32_t), 1, fileID) != 1) {
              SDDS_SetError("Unable to read long column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_LONG64:
          for (i = 0; i < rows; i++) {
            if (fread((int64_t *)(columnValues[col].values) + i, sizeof(int64_t), 1, fileID) != 1) {
              SDDS_SetError("Unable to read long64 column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_FLOAT:
          for (i = 0; i < rows; i++) {
            if (fread((float *)(columnValues[col].values) + i, sizeof(float), 1, fileID) != 1) {
              SDDS_SetError("Unable to read float column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_DOUBLE:
          for (i = 0; i < rows; i++) {
            if (fread((double *)(columnValues[col].values) + i, sizeof(double), 1, fileID) != 1) {
              SDDS_SetError("Unable to read double column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_LONGDOUBLE:
          for (i = 0; i < rows; i++) {
            if (fread((long double *)(columnValues[col].values) + i, sizeof(long double), 1, fileID) != 1) {
              SDDS_SetError("Unable to read long double column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_STRING:
          for (i = 0; i < rows; i++) {
            if (fread(&size, sizeof(size), 1, fileID) != 1) {
              SDDS_SetError("Unable to read string column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            if (size > SDDS_MAXLINE - 1)
              SDDS_Bomb("String is too long");
            columnValues[col].stringValues[i] = malloc(size + 1);
            if (!columnValues[col].stringValues[i]) {
              SDDS_Bomb("Memory allocation failed for string column");
            }
            if (size > 0) {
              if (fread(columnValues[col].stringValues[i], size, 1, fileID) != 1) {
                SDDS_SetError("Unable to read string column");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
              columnValues[col].stringValues[i][size] = '\0';
            } else {
              strcpy(columnValues[col].stringValues[i], "");
            }
          }
          break;
        case SDDS_CHARACTER:
          for (i = 0; i < rows; i++) {
            if (fread((char *)(columnValues[col].values) + i, sizeof(char), 1, fileID) != 1) {
              SDDS_SetError("Unable to read character column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        }
      }
    } else {
      for (i = 0; i < rows; i++) {
        for (col = 0; col < columns; col++) {
          switch (columnValues[col].type) {
          case SDDS_SHORT:
            if (fread((short *)(columnValues[col].values) + i, sizeof(short), 1, fileID) != 1) {
              SDDS_SetError("Unable to read short column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            break;
          case SDDS_LONG:
            if (fread((int32_t *)(columnValues[col].values) + i, sizeof(int32_t), 1, fileID) != 1) {
              SDDS_SetError("Unable to read long column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            break;
          case SDDS_LONG64:
            if (fread((int64_t *)(columnValues[col].values) + i, sizeof(int64_t), 1, fileID) != 1) {
              SDDS_SetError("Unable to read long64 column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            break;
          case SDDS_FLOAT:
            if (fread((float *)(columnValues[col].values) + i, sizeof(float), 1, fileID) != 1) {
              SDDS_SetError("Unable to read float column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            break;
          case SDDS_DOUBLE:
            if (fread((double *)(columnValues[col].values) + i, sizeof(double), 1, fileID) != 1) {
              SDDS_SetError("Unable to read double column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            break;
          case SDDS_LONGDOUBLE:
            if (fread((long double *)(columnValues[col].values) + i, sizeof(long double), 1, fileID) != 1) {
              SDDS_SetError("Unable to read long double column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            break;
          case SDDS_STRING:
            if (fread(&size, sizeof(size), 1, fileID) != 1) {
              SDDS_SetError("Unable to read string column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            if (size > SDDS_MAXLINE - 1)
              SDDS_Bomb("String is too long");
            columnValues[col].stringValues[i] = malloc(size + 1);
            if (!columnValues[col].stringValues[i]) {
              SDDS_Bomb("Memory allocation failed for string column");
            }
            if (size > 0) {
              if (fread(columnValues[col].stringValues[i], size, 1, fileID) != 1) {
                SDDS_SetError("Unable to read string column");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
              columnValues[col].stringValues[i][size] = '\0';
            } else {
              strcpy(columnValues[col].stringValues[i], "");
            }
            break;
          case SDDS_CHARACTER:
            if (fread((char *)(columnValues[col].values) + i, sizeof(char), 1, fileID) != 1) {
              SDDS_SetError("Unable to read character column");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            break;
          }
        }
      }
    }
    if (rows > maxRows) {
      if (!SDDS_LengthenTable(&SDDS_dataset, rows - maxRows)) {
        SDDS_SetError("Unable to lengthen table");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      maxRows = rows;
    }
    j = n;
    for (i = 0; i < columns; i++) {
      if (columnValues[i].skip)
        continue;
      if (columnValues[i].type == SDDS_STRING) {
        SetColumnData(columnValues[i].type, &SDDS_dataset, columnValues[i].stringValues, rows, n);
      } else {
        SetColumnData(columnValues[i].type, &SDDS_dataset, columnValues[i].values, rows, n);
      }
      n++;
    }

    if (!SDDS_WritePage(&SDDS_dataset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    maxRows = 10000;
    if (!SDDS_StartPage(&SDDS_dataset, initRows))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  row = par = col = n = 0;
  rows = -1;
  ptr = NULL;
  ptr = SDDS_Malloc(sizeof(*ptr) * (ptrSize = 2048));
  ptr2 = SDDS_Malloc(sizeof(*ptr2) * (ptrSize = 2048));
  if (!ptr2) {
    SDDS_Bomb("Memory allocation failed for ptr2");
  }
  ptr[0] = 0;
  stop = 0;
  while (!stop) {
    if (readline) {
      while (skiplines > 0) {
        if (!fgets(ptr, ptrSize, fileID))
          break;
        skiplines--;
      }
      if (!fgetsSkipCommentsResize(NULL, &ptr, &ptrSize, fileID, '!'))
        break;
      commentFound = 0;
      if (checkComment) {
        for (i = 0; i < strlen(commentCharacters); i++) {
          if (ptr[0] == commentCharacters[i]) {
            commentFound = 1;
            break;
          }
        }
      }
      if (commentFound == 1) {
        continue;
      }
      if (ptr[strlen(ptr) - 1] == '\n')
        ptr[strlen(ptr) - 1] = '\0';
      strcpy(temp, ptr);
      /*skip empty lines */
      if (getToken(temp, data, 10240, separator, whitespace) < 0)
        continue;
    } else {
      readline = 1;
    }
    if (eofSequence && strncmp(eofSequence, ptr, strlen(eofSequence)) == 0) {
      stop = 1;
      continue;
    }
    if (par < parameters) {
      switch (parameterValues[par].type) {
      case SDDS_SHORT:
        if (sscanf(ptr, "%hd", &shortValue) != 1) {
          SDDS_SetError("Invalid short parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, shortValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_LONG:
        if (sscanf(ptr, "%" SCNd32, &longValue) != 1) {
          SDDS_SetError("Invalid long parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, longValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_LONG64:
        if (sscanf(ptr, "%" SCNd64, &long64Value) != 1) {
          SDDS_SetError("Invalid long64 parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, long64Value, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_FLOAT:
        ConvertDNotationToENotation(ptr);
        if (sscanf(ptr, "%f", &floatValue) != 1) {
          SDDS_SetError("Invalid float parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, floatValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_DOUBLE:
        ConvertDNotationToENotation(ptr);
        if (sscanf(ptr, "%lf", &doubleValue) != 1) {
          SDDS_SetError("Invalid double parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, doubleValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_LONGDOUBLE:
        ConvertDNotationToENotation(ptr);
        if (sscanf(ptr, "%Lf", &ldoubleValue) != 1) {
          SDDS_SetError("Invalid long double parameter");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, ldoubleValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_STRING:
        SDDS_GetToken(ptr, stringValue, SDDS_MAXLINE);
        SDDS_InterpretEscapes(stringValue);
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, stringValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SDDS_CHARACTER:
        SDDS_InterpretEscapes(ptr);
        characterValue = ptr[0];
        if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, par, characterValue, -1))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      }
      par++;
    } else if ((rows == -1) && (!noRowCount)) {
      if (sscanf(ptr, "%" SCNd32, &rows) != 1) {
        SDDS_SetError("Invalid row count");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    } else if ((columns > 0) && ((row < rows) || (noRowCount))) {

      if (columnOrder) {

        if (noRowCount) {
          cp_str(&ptr2, ptr);
          rows = 0;
          while (getToken(ptr2, data, 10240, separator, whitespace) >= 0) {
            rows++;
          }
          free(ptr2);
          ptr2 = NULL;
        }

        if (rows > columnValues[col].elements) {
          if (columnValues[col].type == SDDS_STRING) {
            columnValues[col].stringValues = AllocateColumnStringData(columnValues[col].stringValues, rows, columnValues[col].elements);
          } else {
            columnValues[col].values = AllocateColumnData(columnValues[col].type, columnValues[col].values, rows);
          }
          columnValues[col].elements = rows;
        }
        switch (columnValues[col].type) {
        case SDDS_SHORT:
          for (row = 0; row < rows; row++) {
            if (getToken(ptr, data, 10240, separator, whitespace) < 0) {
              SDDS_SetError("Invalid short column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            if (sscanf(data, "%hd", ((short *)(columnValues[col].values) + row)) != 1) {
              SDDS_SetError("Invalid short column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_LONG:
          for (row = 0; row < rows; row++) {
            if (getToken(ptr, data, 10240, separator, whitespace) < 0) {
              SDDS_SetError("Invalid long column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            if (sscanf(data, "%" SCNd32, ((int32_t *)(columnValues[col].values) + row)) != 1) {
              SDDS_SetError("Invalid long column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_LONG64:
          for (row = 0; row < rows; row++) {
            if (getToken(ptr, data, 10240, separator, whitespace) < 0) {
              SDDS_SetError("Invalid long64 column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            if (sscanf(data, "%" SCNd64, ((int64_t *)(columnValues[col].values) + row)) != 1) {
              SDDS_SetError("Invalid long64 column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_FLOAT:
          for (row = 0; row < rows; row++) {
            if (getToken(ptr, data, 10240, separator, whitespace) < 0) {
              SDDS_SetError("Invalid float column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            ConvertDNotationToENotation(data);
            if (sscanf(data, "%f", ((float *)(columnValues[col].values) + row)) != 1) {
              SDDS_SetError("Invalid float column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_DOUBLE:
          for (row = 0; row < rows; row++) {
            if (getToken(ptr, data, 10240, separator, whitespace) < 0) {
              SDDS_SetError("Invalid double column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            ConvertDNotationToENotation(data);
            if (sscanf(data, "%lf", ((double *)(columnValues[col].values) + row)) != 1) {
              SDDS_SetError("Invalid double column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_LONGDOUBLE:
          for (row = 0; row < rows; row++) {
            if (getToken(ptr, data, 10240, separator, whitespace) < 0) {
              SDDS_SetError("Invalid long double column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            ConvertDNotationToENotation(data);
            if (sscanf(data, "%Lf", ((long double *)(columnValues[col].values) + row)) != 1) {
              SDDS_SetError("Invalid long double column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
          break;
        case SDDS_STRING:
          for (row = 0; row < rows; row++) {
            if (getToken(ptr, data, 10240, separator, whitespace) < 0) {
              SDDS_SetError("Invalid string column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            SDDS_InterpretEscapes(data);
            columnValues[col].stringValues[row] = malloc(strlen(data) + 1);
            if (!columnValues[col].stringValues[row]) {
              SDDS_Bomb("Memory allocation failed for string column element");
            }
            strcpy(columnValues[col].stringValues[row], data);
          }
          break;
        case SDDS_CHARACTER:
          for (row = 0; row < rows; row++) {
            if (getToken(ptr, data, 10240, separator, whitespace) < 0) {
              SDDS_SetError("Invalid character column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            SDDS_InterpretEscapes(data);
            *((char *)(columnValues[col].values) + row) = data[0];
          }
          break;
        }
        if (rows > maxRows) {
          if (!SDDS_LengthenTable(&SDDS_dataset, rows - maxRows)) {
            SDDS_SetError("Unable to lengthen table");
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          maxRows = rows;
        }
        if (columnValues[col].skip == 0) {
          if (columnValues[col].type == SDDS_STRING) {
            SetColumnData(columnValues[col].type, &SDDS_dataset, columnValues[col].stringValues, rows, col);
          } else {
            SetColumnData(columnValues[col].type, &SDDS_dataset, columnValues[col].values, rows, col);
          }
          n++;
        }
        col++;
        row = 0;
      } else {
        if (noRowCount) {
          if (row == 0) {
            rows = 3;
          } else if (row == rows - 1) {
            rows = rows + 3;
            for (i = 0; i < columns; i++) {
              if (rows > columnValues[i].elements) {
                if (columnValues[i].type == SDDS_STRING) {
                  columnValues[i].stringValues = AllocateColumnStringData(columnValues[i].stringValues, rows, columnValues[i].elements);
                } else {
                  columnValues[i].values = AllocateColumnData(columnValues[i].type, columnValues[i].values, rows);
                }
              }
              columnValues[i].elements = rows;
            }
          }
        }
        if (row == 0)
          for (i = 0; i < columns; i++) {
            if (rows > columnValues[i].elements) {
              if (columnValues[i].type == SDDS_STRING) {
                columnValues[i].stringValues = AllocateColumnStringData(columnValues[i].stringValues, rows, columnValues[i].elements);
              } else {
                columnValues[i].values = AllocateColumnData(columnValues[i].type, columnValues[i].values, rows);
              }
            }
            columnValues[i].elements = rows;
          }

        if (noRowCount) {
          cp_str(&ptr2, ptr);
          i = 0;
          while (getToken(ptr2, data, 10240, separator, whitespace) >= 0) {
            i++;
          }
          free(ptr2);
          ptr2 = NULL;
          if ((i != columns) && (parameters > 0 && i == 1)) {
            if (row > 0) {
              if (row > maxRows) {
                if (!SDDS_LengthenTable(&SDDS_dataset, row - maxRows)) {
                  SDDS_SetError("Unable to lengthen table");
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
                }
                maxRows = row;
              }
              n = 0;
              for (j = 0; j < columns; j++) {
                if (columnValues[j].skip)
                  continue;
                if (columnValues[j].type == SDDS_STRING) {
                  SetColumnData(columnValues[j].type, &SDDS_dataset, columnValues[j].stringValues, row, n);
                } else {
                  SetColumnData(columnValues[j].type, &SDDS_dataset, columnValues[j].values, row, n);
                }
                n++;
              }
              if (!SDDS_WritePage(&SDDS_dataset))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              maxRows = 10000;
              if (!SDDS_StartPage(&SDDS_dataset, initRows))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              row = par = col = 0;
              rows = -1;
            }
            readline = 0;
            continue;
          }
        }

        for (i = 0; i < columns; i++) {
          if (getToken(ptr, data, 10240, separator, whitespace) < 0) {
            if (!fillin) {
              fprintf(stderr, "Problem with column data: %s\n", data);
              SDDS_SetError("Invalid column element");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            } else {
              switch (columnValues[i].type) {
              case SDDS_SHORT:
              case SDDS_LONG:
              case SDDS_LONG64:
              case SDDS_FLOAT:
              case SDDS_DOUBLE:
              case SDDS_LONGDOUBLE:
                snprintf(data, sizeof(data), "0");
                break;
              case SDDS_STRING:
              case SDDS_CHARACTER:
                data[0] = '\0';
                break;
              }
            }
          }

          switch (columnValues[i].type) {
          case SDDS_SHORT:
            if (sscanf(data, "%hd", ((short *)(columnValues[i].values) + row)) != 1) {
              if (recover) {
                abort_flag = 1;
                row--;
              } else {
                SDDS_SetError("Invalid short column element");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            break;
          case SDDS_LONG:
            if (sscanf(data, "%" SCNd32, ((int32_t *)(columnValues[i].values) + row)) != 1) {
              if (recover) {
                abort_flag = 1;
                row--;
              } else {
                SDDS_SetError("Invalid long column element");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            break;
          case SDDS_LONG64:
            if (sscanf(data, "%" SCNd64, ((int64_t *)(columnValues[i].values) + row)) != 1) {
              if (recover) {
                abort_flag = 1;
                row--;
              } else {
                SDDS_SetError("Invalid long64 column element");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            break;
          case SDDS_FLOAT:
            ConvertDNotationToENotation(data);
            if (sscanf(data, "%f", ((float *)(columnValues[i].values) + row)) != 1) {
              if (recover) {
                abort_flag = 1;
                row--;
              } else {
                SDDS_SetError("Invalid float column element");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            break;
          case SDDS_DOUBLE:
            ConvertDNotationToENotation(data);
            if (sscanf(data, "%lf", ((double *)(columnValues[i].values) + row)) != 1) {
              if (recover) {
                abort_flag = 1;
                row--;
              } else {
                SDDS_SetError("Invalid double column element");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            break;
          case SDDS_LONGDOUBLE:
            ConvertDNotationToENotation(data);
            if (sscanf(data, "%Lf", ((long double *)(columnValues[i].values) + row)) != 1) {
              if (recover) {
                abort_flag = 1;
                row--;
              } else {
                SDDS_SetError("Invalid long double column element");
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
            break;
          case SDDS_STRING:
            SDDS_InterpretEscapes(data);
            columnValues[i].stringValues[row] = malloc(strlen(data) + 1);
            if (!columnValues[i].stringValues[row]) {
              SDDS_Bomb("Memory allocation failed for string column element");
            }
            strcpy(columnValues[i].stringValues[row], data);
            break;
          case SDDS_CHARACTER:
            SDDS_InterpretEscapes(data);
            *((char *)(columnValues[i].values) + row) = data[0];
            break;
          }

          if (recover && abort_flag) {
            abort_flag = 0;
            break;
          }
        }
        row++;
        if ((row == rows) && (!noRowCount)) {
          if (rows > maxRows) {
            if (!SDDS_LengthenTable(&SDDS_dataset, rows - maxRows)) {
              SDDS_SetError("Unable to lengthen table");
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
            maxRows = rows;
          }
          n = 0;
          for (i = 0; i < columns; i++) {
            if (columnValues[i].skip)
              continue;
            if (columnValues[i].type == SDDS_STRING) {
              SetColumnData(columnValues[i].type, &SDDS_dataset, columnValues[i].stringValues, rows, n);
            } else {
              SetColumnData(columnValues[i].type, &SDDS_dataset, columnValues[i].values, rows, n);
            }
            n++;
          }
        }
      }
    }
    if ((par == parameters) &&
        (((!noRowCount) &&
          (rows != -1)) ||
         (noRowCount)) &&
        (((columnOrder) &&
          (col == columns)) ||
         ((columns > 0) &&
          (row == rows)) ||
         (columns == 0))) {
      if (!SDDS_WritePage(&SDDS_dataset)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      maxRows = 10000;
      if (!SDDS_StartPage(&SDDS_dataset, initRows))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      row = par = col = 0;
      rows = -1;
    }
    ptr[0] = '\0';
  }

  if (noRowCount) {
    if (row > 0) {
      if (row > maxRows) {
        if (!SDDS_LengthenTable(&SDDS_dataset, row - maxRows)) {
          SDDS_SetError("Unable to lengthen table");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        maxRows = row;
      }
      n = 0;
      for (j = 0; j < columns; j++) {
        if (columnValues[j].skip)
          continue;
        if (columnValues[j].type == SDDS_STRING) {
          SetColumnData(columnValues[j].type, &SDDS_dataset, columnValues[j].stringValues, row, n);
        } else {
          SetColumnData(columnValues[j].type, &SDDS_dataset, columnValues[j].values, row, n);
        }
        n++;
      }
      if (!SDDS_WritePage(&SDDS_dataset)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      maxRows = 10000;
    }
  }
  for (i = 0; i < columns; i++) {
    if (columnValues[i].type == SDDS_STRING) {
      for (j = 0; j < columnValues[i].elements; j++) {
        free(columnValues[i].stringValues[j]);
      }
      free(columnValues[i].stringValues);
    } else {
      free(columnValues[i].values);
    }
    if (columnValues[i].name)
      free(columnValues[i].name);
    if (columnValues[i].units)
      free(columnValues[i].units);
    if (columnValues[i].description)
      free(columnValues[i].description);
    if (columnValues[i].symbol)
      free(columnValues[i].symbol);
  }
  for (i = 0; i < parameters; i++) {
    free(parameterValues[i].name);
    if (parameterValues[i].units)
      free(parameterValues[i].units);
    if (parameterValues[i].description)
      free(parameterValues[i].description);
    if (parameterValues[i].symbol)
      free(parameterValues[i].symbol);
  }
  if (columnValues)
    free(columnValues);
  if (parameterValues)
    free(parameterValues);
  if (columnIndex)
    free(columnIndex);
  if (parameterIndex)
    free(parameterIndex);
  if (ptr)
    free(ptr);
  if (ptr2)
    free(ptr2);
  if (!SDDS_Terminate(&SDDS_dataset))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free_scanargs(&s_arg, argc);
  return EXIT_SUCCESS;
}

void SetColumnData(long type, SDDS_DATASET *dataset, void *values, int32_t rows, int32_t index) {
  switch (type) {
  case SDDS_SHORT:
    if (!SDDS_SetColumn(dataset, SDDS_SET_BY_INDEX, (short *)values, rows, index, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  case SDDS_LONG:
    if (!SDDS_SetColumn(dataset, SDDS_SET_BY_INDEX, (int32_t *)values, rows, index, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  case SDDS_LONG64:
    if (!SDDS_SetColumn(dataset, SDDS_SET_BY_INDEX, (int64_t *)values, rows, index, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  case SDDS_FLOAT:
    if (!SDDS_SetColumn(dataset, SDDS_SET_BY_INDEX, (float *)values, rows, index, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  case SDDS_DOUBLE:
    if (!SDDS_SetColumn(dataset, SDDS_SET_BY_INDEX, (double *)values, rows, index, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  case SDDS_LONGDOUBLE:
    if (!SDDS_SetColumn(dataset, SDDS_SET_BY_INDEX, (long double *)values, rows, index, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  case SDDS_STRING:
    if (!SDDS_SetColumn(dataset, SDDS_SET_BY_INDEX, (char **)values, rows, index, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  case SDDS_CHARACTER:
    if (!SDDS_SetColumn(dataset, SDDS_SET_BY_INDEX, (char *)values, rows, index, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  }
}

void *AllocateColumnData(long type, void *values, int32_t rows) {
  switch (type) {
  case SDDS_SHORT:
    return SDDS_Realloc(values, rows * sizeof(short));
  case SDDS_LONG:
    return SDDS_Realloc(values, rows * sizeof(int32_t));
  case SDDS_LONG64:
    return SDDS_Realloc(values, rows * sizeof(int64_t));
  case SDDS_FLOAT:
    return SDDS_Realloc(values, rows * sizeof(float));
  case SDDS_DOUBLE:
    return SDDS_Realloc(values, rows * sizeof(double));
  case SDDS_LONGDOUBLE:
    return SDDS_Realloc(values, rows * sizeof(long double));
  case SDDS_CHARACTER:
    return SDDS_Realloc(values, rows * sizeof(char));
  }
  return values;
}

char **AllocateColumnStringData(char **values, int32_t rows, int32_t previous_rows) {
  long i;
  values = SDDS_Realloc(values, rows * sizeof(char *));
  for (i = previous_rows; i < rows; i++) {
    values[i] = NULL;
    /*    values[i] = malloc(SDDS_MAXLINE); */
  }
  return values;
}

long getToken(char *s, char *buffer, long buflen, char separator, long whitespace) {
  char *ptr0, *ptr1, *escptr;
  long n;
  /* save the pointer to the head of the string */
  ptr0 = s;

  /* skip leading white-space */
  while (isspace(*s))
    s++;
  if (*s == 0)
    return (-1);
  ptr1 = s;

  if (*s == '"') {
    /* if quoted string, skip to next quotation mark */
    ptr1 = s + 1; /* beginning of actual token */
    do {
      s++;
      escptr = NULL;
      if (*s == '\\' && *(s + 1) == '\\') {
        /* skip and remember literal \ (indicated by \\ in the string) */
        escptr = s + 1;
        s += 2;
      }
    } while (*s && (*s != '"' || (*(s - 1) == '\\' && (s - 1) != escptr)));
    /* replace trailing quotation mark with a space */
    if (*s && *s == '"')
      *s = ' ';
    n = (long)(s - ptr1);
    if (whitespace) {
      while (*s && (!isspace(*s))) {
        s++;
      }
      if (*s && (isspace(*s)))
        *s = ' ';
    } else {
      while (*s && (*s != separator)) {
        s++;
      }
      if (*s && (*s == separator))
        *s = ' ';
    }
  } else {
    /* skip to first separator following token */
    if (whitespace) {
      do {
        s++;
        /* imbedded quotation marks are handled here */
        if (*s == '"' && *(s - 1) != '\\') {
          while (*++s && !(*s == '"' && *(s - 1) != '\\'))
            ;
        }
      } while (*s && (!isspace(*s)));
      if (*s && (isspace(*s)))
        *s = ' ';
    } else {
      if (*s != separator) {
        do {
          s++;
          /* imbedded quotation marks are handled here */
          if (*s == '"' && *(s - 1) != '\\') {
            while (*++s && !(*s == '"' && *(s - 1) != '\\'))
              ;
          }
        } while (*s && (*s != separator));
      }
      if (*s && (*s == separator))
        *s = ' ';
    }
    n = (long)(s - ptr1);
  }
  if (n >= buflen)
    return (-1);
  strncpy(buffer, ptr1, n);
  buffer[n] = '\0';

  /* update the original string to delete the token */
  strcpy_ss(ptr0, s);

  /* return the string length */
  return (n);
}

/* Description: Converts Fortran D notation to C++ e notation */
void ConvertDNotationToENotation(char *line) {
  char *ptr = line;

  while (*ptr && (ptr = strstr(ptr, "D+"))) {
    *ptr = 'e';
    ptr++;
    *ptr = '+';
    ptr++;
  }

  ptr = line;
  while (*ptr && (ptr = strstr(ptr, "D-"))) {
    *ptr = 'e';
    ptr++;
    *ptr = '-';
    ptr++;
  }
}
