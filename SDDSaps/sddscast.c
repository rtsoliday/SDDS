/**
 * @file sddscast.c
 * @brief Converts numeric columns, parameters, or arrays in SDDS files from one datatype to another.
 *
 * This program allows users to cast the data types of columns, parameters, or arrays within SDDS
 * (Self Describing Data Sets) files. It supports various numeric types and provides options for
 * handling warnings, piping input/output, and setting data ordering.
 *
 * @section Usage
 * ```
 * sddscast [<source-file>] [<target-file>]
 *          [-pipe=[<input>][,<output>]]
 *          [-noWarnings]
 *          [-majorOrder=row|column]
 *          -cast=<mode>,<columnNames>,<typeNames>,<newType>
 * ```
 *
 * @section Options
 * | Required  | Description                                                                 |
 * |-----------|-----------------------------------------------------------------------------|
 * | `-cast`              | Cast data types of specified entities.                                              |
 *
 * | Optional               | Description                                                                         |
 * |----------------------|-------------------------------------------------------------------------------------|
 * | `-pipe`              | Specify input and/or output pipes.                                                  |
 * | `-noWarnings`        | Suppress warning messages. Useful for scripting or when warnings are not desired.   |
 * | `-majorOrder`        | Set the column data ordering.                                                       |
 *
 * ### Option Constraints
 * - The `-cast` option can be used multiple times to perform multiple casting operations.
 *
 * ### Casting Details
 * - `<mode>`: Specifies the type of entity to cast. Valid values are:
 *   - `column`: Casts data types of specified columns.
 *   - `parameter`: Casts data types of specified parameters.
 *   - `array`: Casts data types of specified arrays.
 * - `<columnNames>`: Comma-separated list of entity names. Supports wildcards (`*`) for pattern matching.
 * - `<typeNames>`: Comma-separated list of current data types corresponding to the entity names. If fewer types are provided than names, the last type is reused.
 * - `<newType>`: The target data type for casting. Supported types are:
 *   - `long`, `ulong`, `long64`, `ulong64`, `short`, `ushort`, `longdouble`, `double`, `float`.
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
 * H. Shang, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

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
  SET_CAST,
  SET_NOWARNINGS,
  SET_PIPE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "cast",
  "nowarnings",
  "pipe",
  "majorOrder",
};

#define CLO_SHORT 0
#define CLO_USHORT 1
#define CLO_LONG 2
#define CLO_ULONG 3
#define CLO_LONG64 4
#define CLO_ULONG64 5
#define CLO_FLOAT 6
#define CLO_DOUBLE 7
#define CLO_LONGDOUBLE 8
#define TYPES 11

char *types[TYPES] = {
  "short",
  "ushort",
  "long",
  "ulong",
  "long64",
  "ulong64",
  "float",
  "double",
  "longdouble",
  "enum",
  "string",
};

typedef struct {
  char **name;
  char **new_type;
  long *index;
  long n;
} CAST_NAME;

typedef struct {
  char *match_string;
  char *type_string;
  char *new_type;
} CAST_REQUEST;

typedef struct {
  int32_t kind;
  char *name, *mapping, *storage;
  int used;
} ENUM_TARGET;

/* -enumValues is scanned before scanargs to keep the raw mapping (escaped commas), but
   its name is matched like other options: case-insensitive, abbreviable, '_' ignored. */
static char *enumValuesTail(char *arg) {
  char *equals, *name, *candidate[N_OPTIONS + 1];
  long i, match;
  if (arg[0] != '-' || !(equals = strchr(arg, '=')) || equals == arg + 1)
    return NULL;
  if (!(name = malloc(equals - arg)))
    SDDS_Bomb("enum option allocation failed");
  memcpy(name, arg + 1, equals - arg - 1);
  name[equals - arg - 1] = 0;
  delete_chars(name, "_");
  for (i = 0; i < N_OPTIONS; i++)
    candidate[i] = option[i];
  candidate[N_OPTIONS] = "enumValues";
  match = match_string(name, candidate, N_OPTIONS + 1, 0);
  free(name);
  return match == N_OPTIONS ? equals + 1 : NULL;
}

/** @brief Apply an explicit enum conversion only to a new output layout. */
static int configureCast(SDDS_DATASET *output, CAST_NAME *casts, int32_t kind, ENUM_TARGET *targets, long count) {
  long i, j;
  int converted = 0;
  for (i = 0; i < casts->n; i++) {
    int32_t index = (int32_t)casts->index[i];
    int32_t oldType = kind == COLUMN_MODE ? output->layout.column_definition[index].type :
      kind == PARAMETER_MODE ? output->layout.parameter_definition[index].type : output->layout.array_definition[index].type;
    int32_t newType = SDDS_IdentifyType(casts->new_type[i]);
    const char *mapping = NULL;
    for (j = 0; j < count; j++)
      if (targets[j].kind == kind && !strcmp(targets[j].name, casts->name[i])) {
        if (mapping) SDDS_Bomb("duplicate -enumValues definition");
        mapping = targets[j].mapping; targets[j].used = 1;
      }
    if (oldType == SDDS_ENUM || newType == SDDS_ENUM) {
      if (!SDDS_ConfigureEnumConversion(output, kind, index, newType, mapping))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      converted = 1;
    } else {
      int32_t ok = kind == COLUMN_MODE ? SDDS_ChangeColumnInformation(output, "type", casts->new_type[i], SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, casts->name[i]) :
        kind == PARAMETER_MODE ? SDDS_ChangeParameterInformation(output, "type", casts->new_type[i], SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, casts->name[i]) :
        SDDS_ChangeArrayInformation(output, "type", casts->new_type[i], SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, casts->name[i]);
      if (!ok) SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  return converted;
}

char *USAGE =
  "sddscast [<source-file>] [<target-file>] \n"
  "         [-pipe=[<input>][,<output>]] \n"
  "         [-noWarnings] \n"
  "          -cast=<mode>,<columnNames>,<typeNames>,<newType>\n"
  "         [-majorOrder=row|column]\n"
  "         [-enumValues=<column|parameter|array>,<name>,<code:label,...>]\n"
  "Options:\n"
  "  -pipe=[<input>][,<output>]  Specify input and/or output pipes.\n"
  "  -noWarnings                 Suppress warning messages.\n"
  "  -cast=<mode>,<names>,<types>,<newType>\n"
  "                              Cast datatypes of specified columns, parameters, or arrays.\n"
  "                              <mode>: one of 'column', 'parameter', or 'array'.\n"
  "                              <names>: Comma-separated list of names (supports wildcards).\n"
  "                              <types>: Comma-separated list of current types.\n"
  "                              <newType>: Target datatype (long, ulong, long64, ulong64, short, ushort,\n"
  "                                         longdouble, double, float, enum, string).\n"
  "  Enums convert explicitly to/from long codes or string labels; enum targets require -enumValues.\n\n"
  "Description:\n"
  "  sddscast converts the numeric columns, parameters, or arrays from one datatype to another within an SDDS file.\n\n"
  "Author: Hairong Shang\n"
  "Compiled on " __DATE__ " at " __TIME__ ", SVN revision: " SVN_VERSION "\n";

void process_cast_columns(SDDS_DATASET *SDDS_input, CAST_NAME *column_cast, CAST_REQUEST *col_request, long col_requests, long noWarnings);
void process_cast_parameters(SDDS_DATASET *SDDS_input, CAST_NAME *parameter_cast, CAST_REQUEST *par_request, long par_requests, long noWarnings);
void process_cast_arrays(SDDS_DATASET *SDDS_input, CAST_NAME *array_cast, CAST_REQUEST *array_request, long array_requests, long noWarnings);
long add_casts(char *old_type, long sdds_type, CAST_NAME *cast_name, char *add_name, char *new_type, long index, long noWarnings, long exactName);
void cleanUpCast(CAST_NAME *cast_name);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_input, SDDS_output;
  SCANNED_ARG *s_arg;
  char *inputfile, *outputfile;
  long tmpfile_used, noWarnings, i, i_arg;
  CAST_NAME column_cast, parameter_cast, array_cast;
  CAST_REQUEST *col_request, *par_request, *array_request;
  long col_requests, par_requests, array_requests, rows;
  unsigned long pipeFlags, majorOrderFlag;
  short columnMajorOrder = -1;
  ENUM_TARGET *enumTargets = NULL;
  long enumTargetCount = 0, writeArg = 1;
  int enumConversions = 0;
  int32_t readResult;
  char **expandedArgs = NULL;
  int scannedCount;

  column_cast.n = parameter_cast.n = array_cast.n = 0;
  column_cast.name = column_cast.new_type = NULL;
  column_cast.index = parameter_cast.index = array_cast.index = 0;
  parameter_cast.name = parameter_cast.new_type = NULL;
  array_cast.name = array_cast.new_type = NULL;
  col_request = par_request = array_request = NULL;
  col_requests = par_requests = array_requests = rows = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = add_file_arguments(argc, argv, &expandedArgs);
  argv = expandedArgs;
  /* Consume exactly two structural commas; leave the logical mapping untouched. */
  for (i = 1; i < argc; i++) {
    char *tail = enumValuesTail(argv[i]);
    if (tail) {
      char *text = NULL, *first, *second;
      int32_t kind;
      if (!SDDS_CopyString(&text, tail)) SDDS_Bomb("enum option allocation failed");
      first = strchr(text, ',');
      second = first ? strchr(first + 1, ',') : NULL;
      if (!first || !second || !second[1]) SDDS_Bomb("invalid -enumValues syntax");
      *first++ = 0; *second++ = 0;
      kind = match_string(text, mode_name, MODES, 0);
      if (kind < 0 || !*first) SDDS_Bomb("invalid -enumValues element kind/name");
      enumTargets = SDDS_Realloc(enumTargets, sizeof(*enumTargets) * (enumTargetCount + 1));
      enumTargets[enumTargetCount++] = (ENUM_TARGET){kind, first, second, text, 0};
      free(argv[i]);
    } else argv[writeArg++] = argv[i];
  }
  argc = writeArg;
  scannedCount = scanargs(&s_arg, argc, argv);
  for (i = 0; i < writeArg; i++) free(argv[i]);
  free(expandedArgs);
  argc = scannedCount;
  if (argc < 3)
    bomb(NULL, USAGE);

  inputfile = outputfile = NULL;
  noWarnings = tmpfile_used = pipeFlags = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
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
      case SET_NOWARNINGS:
        noWarnings = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_CAST:
        if (s_arg[i_arg].n_items != 5)
          SDDS_Bomb("invalid -cast syntax");
        if (match_string(s_arg[i_arg].list[4], types, TYPES, MATCH_WHOLE_STRING) < 0) {
          SDDS_Bomb("Invalid target type; enum conversions support enum, long and string.");
        }
        switch (match_string(s_arg[i_arg].list[1], mode_name, MODES, 0)) {
        case COLUMN_MODE:
          col_request = SDDS_Realloc(col_request, sizeof(*col_request) * (col_requests + 1));
          col_request[col_requests].match_string = s_arg[i_arg].list[2];
          col_request[col_requests].type_string = s_arg[i_arg].list[3];
          col_request[col_requests].new_type = s_arg[i_arg].list[4];
          col_requests++;
          break;
        case PARAMETER_MODE:
          par_request = SDDS_Realloc(par_request, sizeof(*par_request) * (par_requests + 1));
          par_request[par_requests].match_string = s_arg[i_arg].list[2];
          par_request[par_requests].type_string = s_arg[i_arg].list[3];
          par_request[par_requests].new_type = s_arg[i_arg].list[4];
          par_requests++;
          break;
        case ARRAY_MODE:
          array_request = SDDS_Realloc(array_request, sizeof(*array_request) * (array_requests + 1));
          array_request[array_requests].match_string = s_arg[i_arg].list[2];
          array_request[array_requests].type_string = s_arg[i_arg].list[3];
          array_request[array_requests].new_type = s_arg[i_arg].list[4];
          array_requests++;
          break;
        default:
          SDDS_Bomb("unknown cast mode.");
          break;
        }
        break;
      default:
        SDDS_Bomb("unknown option syntax.");
        break;
      }
    } else {
      if (inputfile == NULL)
        inputfile = s_arg[i_arg].list[0];
      else if (outputfile == NULL)
        outputfile = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "Error: Too many filenames provided (sddscast).\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  processFilenames("sddscast", &inputfile, &outputfile, pipeFlags, noWarnings, &tmpfile_used);

  if (!SDDS_InitializeInput(&SDDS_input, inputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!SDDS_InitializeCopy(&SDDS_output, &SDDS_input, outputfile, "w")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (columnMajorOrder != -1)
    SDDS_output.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDS_output.layout.data_mode.column_major = SDDS_input.layout.data_mode.column_major;

  process_cast_columns(&SDDS_input, &column_cast, col_request, col_requests, noWarnings);
  process_cast_parameters(&SDDS_input, &parameter_cast, par_request, par_requests, noWarnings);
  process_cast_arrays(&SDDS_input, &array_cast, array_request, array_requests, noWarnings);

  enumConversions |= configureCast(&SDDS_output, &column_cast, COLUMN_MODE, enumTargets, enumTargetCount);
  enumConversions |= configureCast(&SDDS_output, &parameter_cast, PARAMETER_MODE, enumTargets, enumTargetCount);
  enumConversions |= configureCast(&SDDS_output, &array_cast, ARRAY_MODE, enumTargets, enumTargetCount);
  for (i = 0; i < enumTargetCount; i++)
    if (!enumTargets[i].used) SDDS_Bomb("-enumValues did not match a cast target");

  if (!SDDS_WriteLayout(&SDDS_output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  while ((readResult = SDDS_ReadPage(&SDDS_input)) > 0) {
    if (!(enumConversions ? SDDS_CopyPageWithEnumConversions(&SDDS_output, &SDDS_input) : SDDS_CopyPage(&SDDS_output, &SDDS_input)) || !SDDS_WritePage(&SDDS_output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  if (!readResult) {
    /* Keep the pages already written readable, but never replace an in-place input. */
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&SDDS_input);
    if (!SDDS_Terminate(&SDDS_output))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    if (tmpfile_used)
      remove(outputfile);
    exit(EXIT_FAILURE);
  }
  for (i = 0; i < enumTargetCount; i++) free(enumTargets[i].storage);
  free(enumTargets);
  if (!SDDS_Terminate(&SDDS_input) || !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile))
    exit(EXIT_FAILURE);

  /* Free allocated memory */
  cleanUpCast(&column_cast);
  cleanUpCast(&parameter_cast);
  cleanUpCast(&array_cast);
  if (array_request)
    free(array_request);
  if (par_request)
    free(par_request);
  if (col_request)
    free(col_request);

  free_scanargs(&s_arg, argc);
  return EXIT_SUCCESS;
}

void cleanUpCast(CAST_NAME *cast_name) {
  long i;
  for (i = 0; i < cast_name->n; i++) {
    free(cast_name->name[i]);
    free(cast_name->new_type[i]);
    cast_name->name[i] = NULL;
    cast_name->new_type[i] = NULL;
  }
  free(cast_name->name);
  free(cast_name->new_type);
  free(cast_name->index);
  cast_name->name = NULL;
  cast_name->new_type = NULL;
  cast_name->index = NULL;
}

void process_cast_columns(SDDS_DATASET *SDDS_input, CAST_NAME *column_cast, CAST_REQUEST *col_request, long col_requests, long noWarnings) {
  char *ptr, **Names, **Match_string, **type_string;
  long i, index, j, k, type, s1, s2;
  int32_t names;

  if (!col_requests)
    return;
  ptr = NULL;
  names = s1 = s2 = 0;
  Names = NULL;
  Match_string = type_string = NULL;

  for (i = 0; i < col_requests; i++) {
    s1 = s2 = 0;
    ptr = strtok(col_request[i].match_string, ",");
    while (ptr != NULL) {
      Match_string = SDDS_Realloc(Match_string, sizeof(char *) * (s1 + 1));
      SDDS_CopyString(&Match_string[s1], ptr);
      s1++;
      ptr = strtok(NULL, ",");
    }
    ptr = strtok(col_request[i].type_string, ",");
    while (ptr != NULL) {
      type_string = SDDS_Realloc(type_string, sizeof(char *) * (s2 + 1));
      SDDS_CopyString(&type_string[s2], ptr);
      s2++;
      ptr = strtok(NULL, ",");
    }
    if (s2 < s1) {
      for (j = s2; j < s1; j++) {
        type_string = SDDS_Realloc(type_string, sizeof(char *) * (j + 1));
        SDDS_CopyString(&type_string[j], type_string[s2 - 1]);
        s2++;
      }
    }
    for (k = 0; k < s1; k++) {
      if (!has_wildcards(Match_string[k])) {
        index = SDDS_GetColumnIndex(SDDS_input, Match_string[k]);
        if (index < 0) {
          if (!noWarnings)
            fprintf(stderr, "Warning: Column '%s' does not exist.\n", Match_string[k]);
          continue;
        }
        type = SDDS_GetColumnType(SDDS_input, index);
        add_casts(type_string[k], type, column_cast, Match_string[k], col_request[i].new_type, index, noWarnings, 1);
      } else {
        SDDS_SetColumnFlags(SDDS_input, 0);
        if (!SDDS_SetColumnsOfInterest(SDDS_input, SDDS_MATCH_STRING, Match_string[k], SDDS_OR))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!(Names = (char **)SDDS_GetColumnNames(SDDS_input, &names))) {
          if (!noWarnings)
            fprintf(stderr, "Warning: No columns match the pattern '%s'.\n", Match_string[k]);
        } else {
          for (j = 0; j < names; j++) {
            index = SDDS_GetColumnIndex(SDDS_input, Names[j]);
            type = SDDS_GetColumnType(SDDS_input, index);
            add_casts(type_string[k], type, column_cast, Names[j], col_request[i].new_type, index, noWarnings, 0);
            free(Names[j]);
          }
          free(Names);
          Names = NULL;
        }
      }
    }
    if (Match_string)
      SDDS_FreeStringArray(Match_string, s1);
    if (type_string)
      SDDS_FreeStringArray(type_string, s2);
    Match_string = type_string = NULL;
  }
}

void process_cast_parameters(SDDS_DATASET *SDDS_input, CAST_NAME *parameter_cast, CAST_REQUEST *par_request, long par_requests, long noWarnings) {
  char *ptr, **Names, **Match_string, **type_string;
  long i, index, j, k, type, s1, s2;
  int32_t names;

  if (!par_requests)
    return;
  ptr = NULL;
  names = s1 = s2 = 0;
  Names = NULL;
  Match_string = type_string = NULL;

  if (!(Names = (char **)SDDS_GetParameterNames(SDDS_input, &names))) {
    if (!noWarnings)
      fprintf(stderr, "Warning: No parameters found in the input file.\n");
    return;
  }
  for (i = 0; i < par_requests; i++) {
    s1 = s2 = 0;
    ptr = strtok(par_request[i].match_string, ",");
    while (ptr != NULL) {
      Match_string = SDDS_Realloc(Match_string, sizeof(char *) * (s1 + 1));
      SDDS_CopyString(&Match_string[s1], ptr);
      s1++;
      ptr = strtok(NULL, ",");
    }
    ptr = strtok(par_request[i].type_string, ",");
    while (ptr != NULL) {
      type_string = SDDS_Realloc(type_string, sizeof(char *) * (s2 + 1));
      SDDS_CopyString(&type_string[s2], ptr);
      s2++;
      ptr = strtok(NULL, ",");
    }
    if (s2 < s1) {
      for (j = s2; j < s1; j++) {
        type_string = SDDS_Realloc(type_string, sizeof(char *) * (j + 1));
        SDDS_CopyString(&type_string[j], type_string[s2 - 1]);
        s2++;
      }
    }
    for (k = 0; k < s1; k++) {
      if (!has_wildcards(Match_string[k])) {
        index = SDDS_GetParameterIndex(SDDS_input, Match_string[k]);
        if (index < 0) {
          if (!noWarnings)
            fprintf(stderr, "Warning: Parameter '%s' does not exist in the input file.\n", Match_string[k]);
          continue;
        }
        type = SDDS_GetParameterType(SDDS_input, index);
        add_casts(type_string[k], type, parameter_cast, Match_string[k], par_request[i].new_type, index, noWarnings, 1);
      } else {
        for (j = 0; j < names; j++) {
          if (wild_match(Names[j], Match_string[k])) {
            index = SDDS_GetParameterIndex(SDDS_input, Names[j]);
            type = SDDS_GetParameterType(SDDS_input, index);
            add_casts(type_string[k], type, parameter_cast, Names[j], par_request[i].new_type, index, noWarnings, 0);
          }
        }
      }
    }
    if (Match_string) {
      for (j = 0; j < s1; j++)
        free(Match_string[j]);
      free(Match_string);
    }
    if (type_string) {
      for (j = 0; j < s2; j++)
        free(type_string[j]);
      free(type_string);
    }
    Match_string = type_string = NULL;
  }
  for (i = 0; i < names; i++)
    free(Names[i]);
  free(Names);
}

void process_cast_arrays(SDDS_DATASET *SDDS_input, CAST_NAME *array_cast, CAST_REQUEST *array_request, long array_requests, long noWarnings) {
  char *ptr, **Names, **Match_string, **type_string;
  long i, index, j, k, type, s1, s2;
  int32_t names;

  if (!array_requests)
    return;
  ptr = NULL;
  names = s1 = s2 = 0;
  Names = NULL;
  Match_string = type_string = NULL;

  if (!(Names = (char **)SDDS_GetArrayNames(SDDS_input, &names))) {
    if (!noWarnings)
      fprintf(stderr, "Warning: Unable to retrieve array names from the input file.\n");
    return;
  }
  for (i = 0; i < array_requests; i++) {
    s1 = s2 = 0;
    ptr = strtok(array_request[i].match_string, ",");
    while (ptr != NULL) {
      Match_string = SDDS_Realloc(Match_string, sizeof(char *) * (s1 + 1));
      SDDS_CopyString(&Match_string[s1], ptr);
      s1++;
      ptr = strtok(NULL, ",");
    }
    ptr = strtok(array_request[i].type_string, ",");
    while (ptr != NULL) {
      type_string = SDDS_Realloc(type_string, sizeof(char *) * (s2 + 1));
      SDDS_CopyString(&type_string[s2], ptr);
      s2++;
      ptr = strtok(NULL, ",");
    }
    if (s2 < s1) {
      for (j = s2; j < s1; j++) {
        type_string = SDDS_Realloc(type_string, sizeof(char *) * (j + 1));
        SDDS_CopyString(&type_string[j], type_string[s2 - 1]);
        s2++;
      }
    }
    for (k = 0; k < s1; k++) {
      if (!has_wildcards(Match_string[k])) {
        index = SDDS_GetArrayIndex(SDDS_input, Match_string[k]);
        if (index < 0) {
          if (!noWarnings)
            fprintf(stderr, "Warning: Array '%s' does not exist in the input file.\n", Match_string[k]);
          continue;
        }
        type = SDDS_GetArrayType(SDDS_input, index);
        add_casts(type_string[k], type, array_cast, Match_string[k], array_request[i].new_type, index, noWarnings, 1);
      } else {
        for (j = 0; j < names; j++) {
          if (wild_match(Names[j], Match_string[k])) {
            index = SDDS_GetArrayIndex(SDDS_input, Names[j]);
            type = SDDS_GetArrayType(SDDS_input, index);
            add_casts(type_string[k], type, array_cast, Names[j], array_request[i].new_type, index, noWarnings, 0);
          }
        }
      }
    }
    if (Match_string)
      SDDS_FreeStringArray(Match_string, s1);
    if (type_string)
      SDDS_FreeStringArray(type_string, s2);
    Match_string = type_string = NULL;
  }
  for (i = 0; i < names; i++)
    free(Names[i]);
  free(Names);
}

long add_casts(char *old_type, long sdds_type, CAST_NAME *cast_name, char *add_name, char *new_type, long index, long noWarnings, long exactName) {
  long add = 0;

  if (strcmp(old_type, "*") == 0)
    add = 1;
  else {
    switch (sdds_type) {
    case SDDS_ENUM:
      if (!strcmp(old_type, "enum")) add = 1;
      break;
    case SDDS_STRING:
      if (!strcmp(old_type, "string")) add = 1;
      break;
    case SDDS_LONGDOUBLE:
      if (strcmp(old_type, "longdouble") == 0)
        add = 1;
      break;
    case SDDS_DOUBLE:
      if (strcmp(old_type, "double") == 0)
        add = 1;
      break;
    case SDDS_FLOAT:
      if (strcmp(old_type, "float") == 0)
        add = 1;
      break;
    case SDDS_LONG64:
      if (strcmp(old_type, "long64") == 0)
        add = 1;
      break;
    case SDDS_ULONG64:
      if (strcmp(old_type, "ulong64") == 0)
        add = 1;
      break;
    case SDDS_LONG:
      if (strcmp(old_type, "long") == 0)
        add = 1;
      break;
    case SDDS_ULONG:
      if (strcmp(old_type, "ulong") == 0)
        add = 1;
      break;
    case SDDS_SHORT:
      if (strcmp(old_type, "short") == 0)
        add = 1;
      break;
    case SDDS_USHORT:
      if (strcmp(old_type, "ushort") == 0)
        add = 1;
      break;
    default:
      break;
    }
  }
  if (!add) {
    if (!noWarnings)
      fprintf(stderr, "Warning: The type of '%s' does not match '%s'.\n", add_name, old_type);
    return 0;
  }
  /* A name wildcard can include fields excluded by the source-type filter.  An unsupported
     cast is an error only when the field was requested by exact name or explicit source type. */
  {
    int32_t target = SDDS_IdentifyType(new_type);
    const char *problem = NULL;
    if (sdds_type == SDDS_ENUM || target == SDDS_ENUM) {
      if (!((sdds_type == SDDS_ENUM && (target == SDDS_ENUM || target == SDDS_LONG || target == SDDS_STRING)) ||
            (target == SDDS_ENUM && (sdds_type == SDDS_LONG || sdds_type == SDDS_STRING))))
        problem = "enum conversions require enum, long codes, or string labels";
    } else if (target == SDDS_STRING) {
      problem = "The new type to cast has to be a numeric type (string is allowed only for enum sources)!";
    } else if (!SDDS_NUMERIC_TYPE(sdds_type) || !SDDS_NUMERIC_TYPE(target)) {
      if (!noWarnings) fprintf(stderr, "Warning: unsupported nonnumeric cast for %s\n", add_name);
      return 0;
    }
    if (problem) {
      if (exactName || strcmp(old_type, "*"))
        SDDS_Bomb((char *)problem);
      if (!noWarnings)
        fprintf(stderr, "Warning: unsupported cast of '%s' to %s skipped.\n", add_name, new_type);
      return 0;
    }
  }
  cast_name->name = SDDS_Realloc(cast_name->name, sizeof(char *) * (cast_name->n + 1));
  cast_name->new_type = SDDS_Realloc(cast_name->new_type, sizeof(char *) * (cast_name->n + 1));
  SDDS_CopyString(&cast_name->name[cast_name->n], add_name);
  SDDS_CopyString(&cast_name->new_type[cast_name->n], new_type);
  cast_name->index = SDDS_Realloc(cast_name->index, sizeof(long) * (cast_name->n + 1));
  cast_name->index[cast_name->n] = index;
  cast_name->n++;
  return 1;
}
