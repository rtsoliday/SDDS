/**
 * @file sddssequence.c
 * @brief Generates an SDDS file with equispaced indices in a column.
 *
 * @details
 * This program creates an SDDS file containing a single page with one or more columns of data. The columns can be defined, and sequences of values can be generated using user-specified parameters. Multiple sequences, repeats, and page breaks are supported.
 *
 * @section Usage
 * ```
 * sddssequence [<outputfile>]
 *              [-pipe=<output>]
 *               -define=<columnName>[,<definitionEntries>]
 *              [-repeat=<number>]
 *              [-break]
 *               -sequence=begin=<value>[,number=<integer>][,end=<value>][,delta=<value>][,interval=<integer>]
 *              [-sequence=...] 
 *              [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-define`                             | Define a column with the given name and its entries.                                  |
 * | `-sequence`                           | Specify a sequence with flexible parameters (start, end, delta, interval).            |
 * 
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Define pipe output options.                                                          |
 * | `-repeat`                             | Repeat the sequence a specified number of times.                                      |
 * | `-break`                              | Insert a page break between repeats.                                                 |
 * | `-majorOrder`                         | Specify the major order for data storage (row or column).                             |
 *
 * @subsection Incompatibilities
 *   - `-break` requires `-repeat=<number>`.
 *   - `-sequence` parameters must conform to one of these combinations:
 *     - `end` and `delta`
 *     - `end` and `number`
 *     - `delta` and `number`
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
 *   - M. Borland
 *   - C. Saunders
 *   - L. Emery
 *   - R. Soliday
 *   - H. Shang
 */

#include <stdlib.h>
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_PIPE,
  SET_DEFINE,
  SET_SEQUENCE,
  SET_REPEAT,
  SET_MAJOR_ORDER,
  SET_BREAK,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "pipe",
  "define",
  "sequence",
  "repeat",
  "majorOrder",
  "break",
};

/* Improved usage message with enhanced readability */
static char *USAGE =
  "sddssequence [<outputfile>] \\\n"
  "    [-pipe=<output>] \\\n"
  "     -define=<columnName>[,<definitionEntries>] \\\n"
  "    [-repeat=<number>] \\\n"
  "    [-break] \\\n"
  "     -sequence=begin=<value>[,number=<integer>][,end=<value>][,delta=<value>][,interval=<integer>] \\\n"
  "    [-sequence=begin=<value>[,number=<integer>][,end=<value>][,delta=<value>][,interval=<integer>] ...] \\\n"
  "    [-majorOrder=row|column]\n"
  "Generates an SDDS file with a single page and several columns of data.\n"
  "Options:\n"
  "  <outputfile>                      Specify the output SDDS file. If omitted, standard output is used.\n"
  "  -pipe=<output>                    Define pipe output options.\n"
  "  -define=<columnName>,<entries>    Define a column with the given name and entries.\n"
  "  -repeat=<number>                  Repeat the sequence the specified number of times.\n"
  "  -break                            Insert a page break between repeats.\n"
  "  -sequence=begin=<val>,number=<n>,end=<val>,delta=<val>,interval=<n>\n"
  "                                    Define a sequence with specified parameters. Multiple -sequence options can be used.\n"
  "  -majorOrder=row|column            Set the major order of data storage.\n\n"
  "Notes:\n"
  "  - The default data type is double. To specify a different type, use type=<typeName> in -define.\n"
  "  - Each column is specified with a -define option followed by any number of -sequence options.\n"
  "  - The default value of delta is 1.\n"
  "  - The default beginning value is the ending value of the last sequence plus the delta of the last sequence.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define SEQ_END_GIVEN 0x0001
#define SEQ_BEGIN_GIVEN 0x0002
#define SEQ_NUMBER_GIVEN 0x0004
#define SEQ_DELTA_GIVEN 0x0008
#define SEQ_INTERVAL_GIVEN 0x0010

/* Valid combinations of end, number, and delta: */
#define SEQ_ENDplusDELTA (SEQ_END_GIVEN + SEQ_DELTA_GIVEN)
#define SEQ_ENDplusNUMBER (SEQ_END_GIVEN + SEQ_NUMBER_GIVEN)
#define SEQ_DELTAplusNUMBER (SEQ_DELTA_GIVEN + SEQ_NUMBER_GIVEN)

typedef struct {
  unsigned long flags;
  double begin, end, delta;
  int64_t number, interval;
} SEQUENCE;

typedef struct {
  char *columnName;
  char **item;
  SEQUENCE *sequence;
  long items;
  long sequences;
  long rows;
  long repeats;
  double *data;
} DEFINITION;

void addSequence(char **item, long items, DEFINITION *definition);
void addDefinition(char **item, long items, DEFINITION **definition, long *definitions);
void generateOutput(SDDS_DATASET *outputTable, DEFINITION *definition, long definitions, long doBreak);
void setupOutputFile(SDDS_DATASET *outputTable, char *output, DEFINITION *definition, long definitions, short columnMajorOrder);
void createColumn(SDDS_DATASET *outputTable, DEFINITION *definition);

int main(int argc, char **argv) {
  SDDS_DATASET outputTable;
  SCANNED_ARG *s_arg;
  long i_arg, i;
  DEFINITION *definition = NULL;
  long definitions = 0;
  long doBreak = 0;

  char *output = NULL;
  unsigned long pipeFlags = 0, majorOrderFlag;
  short columnMajorOrder = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    bomb(NULL, USAGE);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            !scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                          "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)) {
          SDDS_Bomb("invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER) {
          columnMajorOrder = 1;
        } else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER) {
          columnMajorOrder = 0;
        }
        break;

      case SET_DEFINE:
        addDefinition(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &definition, &definitions);
        break;

      case SET_REPEAT:
        if (definitions == 0) {
          SDDS_Bomb("can't give a repeat specifier prior to a definition");
        }
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &definition[definitions - 1].repeats) != 1 ||
            definition[definitions - 1].repeats <= 0) {
          SDDS_Bomb("invalid -repeat syntax/value");
        }
        break;

      case SET_BREAK:
        doBreak = 1;
        break;

      case SET_SEQUENCE:
        if (definitions == 0) {
          SDDS_Bomb("can't create a sequence prior to defining the variable");
        }
        addSequence(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, definition + definitions - 1);
        break;

      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags)) {
          SDDS_Bomb("invalid -pipe syntax");
        }
        break;

      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (output == NULL) {
        output = s_arg[i_arg].list[0];
      } else {
        SDDS_Bomb("too many filenames");
      }
    }
  }

  if (output == NULL && !(pipeFlags & USE_STDOUT)) {
    SDDS_Bomb("no output specified");
  }

  if (!definitions) {
    SDDS_Bomb("no sequences defined");
  }

  setupOutputFile(&outputTable, output, definition, definitions, columnMajorOrder);
  generateOutput(&outputTable, definition, definitions, doBreak);

  if (!SDDS_Terminate(&outputTable)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < definitions; i++) {
    free(definition[i].sequence);
    free(definition[i].data);
  }
  free(definition);
  free_scanargs(&s_arg, argc);

  return EXIT_SUCCESS;
}

void addDefinition(char **item, long items, DEFINITION **definition, long *definitions) {
  if (items < 1) {
    SDDS_Bomb("unable to add definition--supply column name");
  }
  *definition = SDDS_Realloc(*definition, sizeof(**definition) * (*definitions + 1));
  if (!(*definition)) {
    SDDS_Bomb("unable to add definition---allocation failure");
  }

  (*definition)[*definitions].columnName = item[0];
  (*definition)[*definitions].item = item + 1;
  (*definition)[*definitions].items = items - 1;
  (*definition)[*definitions].sequence = NULL;
  (*definition)[*definitions].sequences = 0;
  (*definition)[*definitions].rows = 0;
  (*definition)[*definitions].data = NULL;
  (*definition)[*definitions].repeats = 1;
  (*definitions)++;
}

void addSequence(char **item, long items, DEFINITION *definition) {
  SEQUENCE *sequence;
  long i;

  definition->sequence = SDDS_Realloc(definition->sequence, sizeof(*definition->sequence) * (definition->sequences + 1));
  if (!definition->sequence) {
    SDDS_Bomb("unable to add sequence--memory allocation failure");
  }

  sequence = &definition->sequence[definition->sequences];
  sequence->interval = 1;

  if (!scanItemList(&sequence->flags, item, &items, 0,
                    "begin", SDDS_DOUBLE, &sequence->begin, 1, SEQ_BEGIN_GIVEN,
                    "end", SDDS_DOUBLE, &sequence->end, 1, SEQ_END_GIVEN,
                    "number", SDDS_LONG64, &sequence->number, 1, SEQ_NUMBER_GIVEN,
                    "delta", SDDS_DOUBLE, &sequence->delta, 1, SEQ_DELTA_GIVEN,
                    "interval", SDDS_LONG64, &sequence->interval, 1, SEQ_INTERVAL_GIVEN,
                    NULL)) {
    SDDS_Bomb("invalid -sequence syntax");
  }

  if ((sequence->flags & SEQ_NUMBER_GIVEN) && sequence->number <= 0) {
    SDDS_Bomb("number <= 0 is not valid for -sequence");
  }

  if ((sequence->flags & SEQ_DELTA_GIVEN) && sequence->delta == 0) {
    SDDS_Bomb("delta == 0 is not valid for -sequence");
  }

  if (!(sequence->flags & SEQ_BEGIN_GIVEN)) {
    if (definition->sequences == 0) {
      SDDS_Bomb("you must give begin point for the first sequence of a definition");
    }
    if (!(sequence->flags & SEQ_DELTA_GIVEN)) {
      SDDS_Bomb("you must give delta with implied begin point");
    }
    sequence->begin = definition->sequence[definition->sequences - 1].end + sequence->delta;
  }

  if ((sequence->flags & SEQ_INTERVAL_GIVEN) && sequence->interval <= 0) {
    SDDS_Bomb("interval for sequence must be > 0");
  }

  if ((sequence->flags & SEQ_ENDplusDELTA) == SEQ_ENDplusDELTA) {
    sequence->number = ((long)((sequence->end - sequence->begin) / sequence->delta + 1.5)) * sequence->interval;
    if (sequence->number <= 0) {
      SDDS_Bomb("given (start, end, delta) implies number of points <= 0");
    }
  } else if ((sequence->flags & SEQ_ENDplusNUMBER) == SEQ_ENDplusNUMBER) {
    if (sequence->number == 1) {
      sequence->delta = 0;
    } else {
      sequence->delta = (sequence->end - sequence->begin) / (sequence->number - 1) * sequence->interval;
    }
  } else if ((sequence->flags & SEQ_DELTAplusNUMBER) == SEQ_DELTAplusNUMBER) {
    sequence->end = (sequence->delta / sequence->interval) * (sequence->number - 1) + sequence->begin;
  } else {
    SDDS_Bomb("you must supply (end, delta), (end, number), or (delta, number)");
  }

  definition->data = SDDS_Realloc(definition->data, sizeof(*definition->data) * (definition->rows + sequence->number));
  if (!definition->data) {
    SDDS_Bomb("unable to generate sequence data--allocation failure");
  }

  for (i = 0; i < sequence->number; i++) {
    definition->data[definition->rows + i] = sequence->begin + (i / sequence->interval) * sequence->delta;
  }

  definition->rows += sequence->number;
  definition->sequences++;
}

void generateOutput(SDDS_DATASET *outputTable, DEFINITION *definition, long definitions, long doBreak) {
  long idef, row, rows = 0, totalRows = 0;

  if (!doBreak) {
    for (idef = 0; idef < definitions; idef++) {
      totalRows = definition[idef].rows * definition[idef].repeats;
      if (idef && totalRows != rows) {
        fputs("warning: sequences are of different length (sddssequence)\n", stderr);
      }
      if (totalRows > rows) {
        rows = totalRows;
      }
    }

    if (rows == 0) {
      SDDS_Bomb("total number of points in sequence is zero");
    }

    if (!SDDS_StartPage(outputTable, rows)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    for (idef = 0; idef < definitions; idef++) {
      if (definition[idef].rows != totalRows) {
        /* repeats are carried out here */
        definition[idef].data = SDDS_Realloc(definition[idef].data, sizeof(*definition[idef].data) * rows);
        if (!definition[idef].data) {
          SDDS_Bomb("unable to generate output--allocation failure");
        }
        for (row = definition[idef].rows; row < rows; row++) {
          definition[idef].data[row] = definition[idef].data[row % definition[idef].rows];
        }
      }
      if (!SDDS_SetColumnFromDoubles(outputTable, SDDS_BY_NAME, definition[idef].data, rows, definition[idef].columnName)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    if (!SDDS_WritePage(outputTable)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  } else {
    long irep;
    rows = definition[0].rows;
    if (rows == 0) {
      SDDS_Bomb("number of points in sequence is zero");
    }
    for (idef = 1; idef < definitions; idef++) {
      if (rows != definition[idef].rows) {
        fputs("warning: sequences are of different length (sddssequence)\n", stderr);
      }
    }
    for (irep = 0; irep < definition[0].repeats; irep++) {
      if (!SDDS_StartPage(outputTable, rows)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      for (idef = 0; idef < definitions; idef++) {
        if (!SDDS_SetColumnFromDoubles(outputTable, SDDS_BY_NAME, definition[idef].data, rows, definition[idef].columnName)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      if (!SDDS_WritePage(outputTable)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }
}

void setupOutputFile(SDDS_DATASET *outputTable, char *output, DEFINITION *definition, long definitions, short columnMajorOrder) {
  long i;

  if (!SDDS_InitializeOutput(outputTable, SDDS_BINARY, 0, NULL, "sddssequence output", output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  for (i = 0; i < definitions; i++) {
    createColumn(outputTable, &definition[i]);
  }

  outputTable->layout.data_mode.column_major = columnMajorOrder;
  if (!SDDS_WriteLayout(outputTable)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}

void createColumn(SDDS_DATASET *outputTable, DEFINITION *definition) {
  char s[SDDS_MAXLINE];
  char *ptr;
  long i;

  if (!definition->columnName) {
    SDDS_Bomb("column name is null");
  }
  if (SDDS_GetColumnIndex(outputTable, definition->columnName) >= 0) {
    SDDS_Bomb("column name already exists (sddssequence)");
  }

  /* Initialize the column definition string */
  snprintf(s, sizeof(s), "&column name=%s, ", definition->columnName);

  for (i = 0; i < definition->items; i++) {
    ptr = strchr(definition->item[i], '=');
    if (!ptr) {
      fprintf(stderr, "error: invalid definition-entry: %s\n", definition->item[i]);
      exit(EXIT_FAILURE);
    }
    *ptr = '\0';
    strcat(s, definition->item[i]);
    strcat(s, "=\"");
    strcat(s, ptr + 1);
    strcat(s, "\", ");
  }

  /* Ensure type is specified, default to double if not */
  if (!strstr(s, ", type=")) {
    strcat(s, " type=\"double\", ");
  }

  strcat(s, "&end");

  if (!SDDS_ProcessColumnString(outputTable, s, 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}
