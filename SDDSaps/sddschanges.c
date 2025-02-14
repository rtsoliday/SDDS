/**
 * @file sddschanges.c
 * @brief Analyze data from columns of an SDDS file to determine changes from the first page.
 *
 * @details
 * This program processes an SDDS (Self Describing Data Set) file to compute changes
 * in specified columns relative to baseline data, which can be provided either
 * as a separate file or implicitly as the first page of the input file. The program
 * supports various options to copy columns, pass data through, and configure output
 * formatting.
 *
 * ### Key Features
 * - Compute changes in specified columns based on baseline data.
 * - Optionally copy or pass through additional columns.
 * - Support for parallel page processing and maintaining empty pages.
 * - Configurable output order (row-major or column-major).
 *
 * @section Usage
 * ```
 * sddschanges [<inputfile>] [<outputfile>]
 *             [-pipe=[input][,output]]
 *              -changesIn=[exclude=<wildcard>,][,newType=<string>]<column-names>
 *             [-copy=<column-names>]
 *             [-pass=<column-names>]
 *             [-baseline=<filename>]
 *             [-parallelPages] 
 *             [-keepEmpties] 
 *             [-majorOrder=row|column] 
 * ```
 *
 * @section Options
 * | Required  | Description                                                                 |
 * |-----------|-----------------------------------------------------------------------------|
 * | `-changesIn`                | Specify columns to compute changes for. Supports wildcards and type overrides. |
 *
 * | Optional                      | Description                                                             |
 * |-----------------------------|-------------------------------------------------------------------------|
 * | `-pipe`                     | Use standard input/output for input and/or output.                      |
 * | `-copy`                     | Specify columns to copy from the first page to all pages of the output. |
 * | `-pass`                     | Specify columns to pass through from each page of the input to the output. |
 * | `-baseline`                 | Specify a baseline SDDS file to compute changes against.                 |
 * | `-parallelPages`            | Compares input and baseline files page-by-page.                          |
 * | `-keepEmpties`              | Ensures that empty pages are emitted to the output.                      |
 * | `-majorOrder`               | Specify the major order for writing the output file (row|column).        |
 *
 * @subsection Incompatibilities
 *   - `-parallelPages` requires `-baseline`.
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
#include "match_string.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_COPY,
  SET_CHANGESIN,
  SET_PASS,
  SET_BASELINE,
  SET_PIPE,
  SET_PARALLELPAGES,
  SET_KEEPEMPTIES,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "copy",
  "changesin",
  "pass",
  "baseline",
  "pipe",
  "parallelpages",
  "keepempties",
  "majorOrder",
};

typedef struct
{
  char *columnName;
  long optionCode, typeCode;
  char *excludeName;
} CHANGE_REQUEST;

/* This structure stores data necessary for accessing/creating SDDS columns and
 * for computing a statistic.
 */
typedef struct
{
  char *sourceColumn, *resultColumn;
  long optionCode, typeCode;
  /* These store intermediate values during processing */
  double *baseline, *change;
  void *copy;
  void *pass;
  long type, newType;
} CHANGE_DEFINITION;

long addChangeRequests(CHANGE_REQUEST **request, long requests, char **item, long items, long code, char *excludeName, char *newType);
CHANGE_DEFINITION *compileChangeDefinitions(SDDS_DATASET *inSet, long *changes, CHANGE_REQUEST *request, long requests);
long setupOutputFile(SDDS_DATASET *outSet, char *output, SDDS_DATASET *inSet, CHANGE_DEFINITION *change, long changes, short columnMajorOrder);
int64_t addBaselineData(CHANGE_DEFINITION *change, long changes, char *baseline, long page, int64_t lastRows);
int64_t copyBaselineData(CHANGE_DEFINITION *change, long changes, SDDS_DATASET *dataset);
void computeChanges(CHANGE_DEFINITION *change, long changes, SDDS_DATASET *inSet, int64_t rows);
void outputChanges(CHANGE_DEFINITION *change, long changes, SDDS_DATASET *outSet, int64_t rows, SDDS_DATASET *inSet);
long transferDefinitions(SDDS_DATASET *outSet, SDDS_DATASET *inSet, CHANGE_DEFINITION *change, long changes, long optionCode);

static char *USAGE =
  "sddschanges [<input>] [<output>]\n"
  "            [-pipe=[input][,output]]\n"
  "             -changesIn=[exclude=<wildcard>,][,newType=<string>]<column-names>\n"
  "            [-copy=<column-names>]\n"
  "            [-pass=<column-names>]\n"
  "            [-baseline=<filename>]\n"
  "            [-parallelPages] \n"
  "            [-keepEmpties] \n"
  "            [-majorOrder=row|column] \n"
  "Options:\n"
  "  -pipe=[input][,output]\n"
  "      Use standard input/output for input and/or output.\n"
  "  -changesIn=[exclude=<wildcard>,][,newType=<string>,]<column-names>\n"
  "      Specify columns to compute changes for. Optionally exclude certain columns\n"
  "      using wildcards and set a new data type for the resulting change columns.\n"
  "  -copy=<column-names>\n"
  "      Specify columns to copy from the first page of the input to all pages of the output.\n"
  "      By default, only requested changes appear in the output.\n"
  "  -pass=<column-names>\n"
  "      Specify columns to pass through from each page of the input to each page of the output.\n"
  "      By default, only requested changes appear in the output.\n"
  "  -baseline=<filename>\n"
  "      Specify a baseline SDDS file to compute changes against. If not provided,\n"
  "      the first page of the input file is used as the baseline.\n"
  "  -parallelPages\n"
  "      When used with -baseline, compares the input and baseline files page-by-page.\n"
  "      Otherwise, compares all input pages to the first page of the baseline data.\n"
  "  -keepEmpties\n"
  "      By default, empty pages in the input do not appear in the output.\n"
  "      This option ensures that empty pages are emitted to the output.\n"
  "  -majorOrder=row|column\n"
  "      Specify the major order for writing the output file, either row-major or column-major.\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  CHANGE_DEFINITION *change;
  long changes, requests;
  CHANGE_REQUEST *request;
  SCANNED_ARG *scanned; /* structure for scanned arguments */
  SDDS_DATASET inSet, outSet;
  long i_arg, code, parallelPages, keepEmpties, i;
  int64_t baselineRows, rows, lastRows;
  char *input, *baseline, *output;
  unsigned long pipeFlags, majorOrderFlag;
  char *excludeName = NULL;
  char *ptr = NULL;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    bomb(NULL, USAGE);
  }

  input = output = baseline = NULL;
  change = NULL;
  request = NULL;
  changes = requests = baselineRows = 0;
  pipeFlags = 0;
  parallelPages = keepEmpties = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* Process options here */
      switch (code = match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[i_arg].n_items -= 1;
        if (scanned[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[i_arg].list + 1, &scanned[i_arg].n_items,
                           0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("Invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_COPY:
      case SET_CHANGESIN:
      case SET_PASS:
        if (scanned[i_arg].n_items < 2) {
          fprintf(stderr, "Error: Invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        if (code == SET_CHANGESIN) {
          long offset;
          char *newTypeName;
          char *changeOption[2] = {"exclude", "newtype"};
#define CHANGE_EXCLUDE 0
#define CHANGE_NEWTYPE 1
#define N_CHANGE_OPTIONS 2
          excludeName = newTypeName = NULL;
          for (offset = 1; offset < scanned[i_arg].n_items; offset++) {
            if ((ptr = strchr(scanned[i_arg].list[offset], '='))) {
              *ptr = '\0';
              switch (match_string(scanned[i_arg].list[offset], changeOption, N_CHANGE_OPTIONS, 0)) {
              case CHANGE_EXCLUDE:
                SDDS_CopyString(&excludeName, ++ptr);
                break;
              case CHANGE_NEWTYPE:
                SDDS_CopyString(&newTypeName, ++ptr);
                break;
              default:
                break;
              }
            } else {
              break;
            }
          }
          requests = addChangeRequests(&request, requests, scanned[i_arg].list + offset,
                                       scanned[i_arg].n_items - offset, code, excludeName, newTypeName);
        } else {
          requests = addChangeRequests(&request, requests, scanned[i_arg].list + 1,
                                       scanned[i_arg].n_items - 1, code, NULL, NULL);
        }
        break;
      case SET_BASELINE:
        if (scanned[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -baseline syntax");
        SDDS_CopyString(&baseline, scanned[i_arg].list[1]);
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;
      case SET_PARALLELPAGES:
        parallelPages = 1;
        break;
      case SET_KEEPEMPTIES:
        keepEmpties = 1;
        break;
      default:
        fprintf(stderr, "Error: Unknown option '%s' given\n", scanned[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      /* Argument is filename */
      if (!input)
        SDDS_CopyString(&input, scanned[i_arg].list[0]);
      else if (!output)
        SDDS_CopyString(&output, scanned[i_arg].list[0]);
      else
        SDDS_Bomb("Too many filenames provided");
    }
  }

  if (parallelPages && !baseline)
    SDDS_Bomb("-parallelPages only makes sense with -baseline");

  processFilenames("sddschanges", &input, &output, pipeFlags, 0, NULL);

  if (!requests)
    SDDS_Bomb("No changes requested");

  if (!SDDS_InitializeInput(&inSet, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(change = compileChangeDefinitions(&inSet, &changes, request, requests)))
    SDDS_Bomb("Unable to compile definitions");

  if (!setupOutputFile(&outSet, output, &inSet, change, changes, columnMajorOrder)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Bomb("Unable to setup output file");
  }

  if (baseline && !parallelPages)
    baselineRows = addBaselineData(change, changes, baseline, 0, 0);

  lastRows = 0;
  while ((code = SDDS_ReadPage(&inSet)) > 0) {
    rows = SDDS_CountRowsOfInterest(&inSet);
    if (baseline && parallelPages)
      baselineRows = addBaselineData(change, changes, baseline, code, lastRows);
    if (!baseline && code == 1) {
      baselineRows = copyBaselineData(change, changes, &inSet);
      continue;
    }
    if (rows != baselineRows)
      SDDS_Bomb("Number of rows in file changed");
    if (rows)
      computeChanges(change, changes, &inSet, rows);
    if (rows || keepEmpties)
      outputChanges(change, changes, &outSet, rows, &inSet);
    lastRows = rows;
  }
  if (!SDDS_Terminate(&inSet) || !SDDS_Terminate(&outSet)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (change) {
    for (i = 0; i < changes; i++) {
      if (change[i].baseline)
        free(change[i].baseline);
      if (change[i].change)
        free(change[i].change);
    }
    free(change);
  }
  if (input)
    free(input);
  if (output)
    free(output);
  if (baseline)
    free(baseline);
  if (request)
    free(request);
  free_scanargs(&scanned, argc);
  return EXIT_SUCCESS;
}

int64_t addBaselineData(CHANGE_DEFINITION *change, long changes, char *baseline, long page, int64_t lastRows) {
  static SDDS_DATASET dataset;
  long i, code;
  int64_t rows;
  rows = 0;

  if (page == 0 || page == 1) {
    if (!SDDS_InitializeInput(&dataset, baseline))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if ((code = SDDS_ReadPage(&dataset)) <= 0 || (rows = SDDS_CountRowsOfInterest(&dataset)) < 0) {
    SDDS_SetError("Problem reading (next) page of baseline data file");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (page && code != page)
    SDDS_Bomb("Page mixup in baseline file");
  for (i = 0; i < changes; i++) {
    if (change[i].optionCode == SET_CHANGESIN) {
      if (change[i].baseline) {
        if (page > 1)
          free(change[i].baseline);
        change[i].baseline = NULL;
      }
      if (rows && !(change[i].baseline = SDDS_GetColumnInDoubles(&dataset, change[i].sourceColumn))) {
        fprintf(stderr, "Problem reading baseline data\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    } else if (change[i].optionCode == SET_COPY) {
      if (change[i].copy) {
        if (page > 1) {
          if (change[i].type == SDDS_STRING && lastRows)
            SDDS_FreeStringArray(change[i].copy, lastRows);
        }
        free(change[i].copy);
        change[i].copy = NULL;
      }
      if (rows && !(change[i].copy = SDDS_GetColumn(&dataset, change[i].sourceColumn))) {
        fprintf(stderr, "Problem reading baseline data\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }
  return rows;
}

int64_t copyBaselineData(CHANGE_DEFINITION *change, long changes, SDDS_DATASET *dataset) {
  long i;
  int64_t rows;

  if (!(rows = SDDS_CountRowsOfInterest(dataset)))
    SDDS_Bomb("No data in first page of input file");
  for (i = 0; i < changes; i++) {
    if (change[i].optionCode == SET_CHANGESIN) {
      if (!(change[i].baseline = SDDS_GetColumnInDoubles(dataset, change[i].sourceColumn))) {
        fprintf(stderr, "Problem reading baseline data\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    } else if (change[i].optionCode == SET_COPY) {
      if (!(change[i].copy = SDDS_GetColumn(dataset, change[i].sourceColumn))) {
        fprintf(stderr, "Problem reading baseline data\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }
  return rows;
}

void computeChanges(CHANGE_DEFINITION *change, long changes, SDDS_DATASET *inSet, int64_t rows) {
  double *data;
  long i;
  int64_t j;

  for (i = 0; i < changes; i++) {
    switch (change[i].optionCode) {
    case SET_COPY:
      break;
    case SET_PASS:
      break;
    case SET_CHANGESIN:
      if (!(data = SDDS_GetColumnInDoubles(inSet, change[i].sourceColumn))) {
        fprintf(stderr, "Problem reading input data\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (change[i].change)
        free(change[i].change);
      change[i].change = (double *)tmalloc(sizeof(*(change[i].change)) * rows);
      for (j = 0; j < rows; j++) {
        change[i].change[j] = data[j] - change[i].baseline[j];
      }
      free(data);
      break;
    }
  }
}

void outputChanges(CHANGE_DEFINITION *change, long changes, SDDS_DATASET *outSet, int64_t rows, SDDS_DATASET *inSet) {
  long i;
  void *data;

  if (!SDDS_StartPage(outSet, rows))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_CopyParameters(outSet, inSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (rows) {
    for (i = 0; i < changes; i++) {
      switch (change[i].optionCode) {
      case SET_CHANGESIN:
        if (!SDDS_SetColumnFromDoubles(outSet, SDDS_SET_BY_NAME, change[i].change, rows, change[i].resultColumn))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SET_COPY:
        if (!SDDS_SetColumn(outSet, SDDS_SET_BY_NAME, change[i].copy, rows, change[i].resultColumn))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      case SET_PASS:
        if (!(data = SDDS_GetInternalColumn(inSet, change[i].resultColumn)) ||
            !SDDS_SetColumn(outSet, SDDS_SET_BY_NAME, data, rows, change[i].resultColumn))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        break;
      }
    }
  }
  if (!SDDS_WritePage(outSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

long addChangeRequests(CHANGE_REQUEST **request, long requests, char **item, long items, long code, char *excludeName, char *newTypeName) {
  long i;
  *request = SDDS_Realloc(*request, sizeof(**request) * (requests + items));
  for (i = 0; i < items; i++) {
    (*request)[i + requests].columnName = item[i];
    (*request)[i + requests].optionCode = code;
    (*request)[i + requests].excludeName = excludeName;
    if (newTypeName) {
      if (((*request)[i + requests].typeCode = SDDS_IdentifyType(newTypeName)) == 0) {
        char buffer[16384];
        snprintf(buffer, sizeof(buffer), "Unknown type given: %s", newTypeName);
        SDDS_Bomb(buffer);
      }
    } else {
      (*request)[i + requests].typeCode = -1;
    }
  }
  return items + requests;
}

CHANGE_DEFINITION *compileChangeDefinitions(SDDS_DATASET *inSet, long *changes, CHANGE_REQUEST *request, long requests) {
  CHANGE_DEFINITION *change;
  long iReq, iChange, iName, index;
  int32_t columnNames;
  char s[SDDS_MAXLINE];
  char **columnName;

  change = tmalloc(sizeof(*change) * requests);
  *changes = iChange = 0;
  for (iReq = 0; iReq < requests; iReq++) {
    if (iChange >= *changes)
      change = SDDS_Realloc(change, sizeof(*change) * (*changes += 10));
    if (!has_wildcards(request[iReq].columnName)) {
      if ((index = SDDS_GetColumnIndex(inSet, request[iReq].columnName)) < 0) {
        sprintf(s, "Error: Column '%s' not found in input file", request[iReq].columnName);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      change[iChange].sourceColumn = request[iReq].columnName;
      change[iChange].optionCode = request[iReq].optionCode;
      change[iChange].change = change[iChange].baseline = NULL;
      change[iChange].type = SDDS_GetColumnType(inSet, index);
      if (change[iChange].optionCode == SET_CHANGESIN && !SDDS_NUMERIC_TYPE(SDDS_GetColumnType(inSet, index))) {
        fprintf(stderr, "Error: Column '%s' is non-numeric. Cannot compute changes.\n", change[iChange].sourceColumn);
        exit(EXIT_FAILURE);
      }
      change[iChange].copy = change[iChange].pass = NULL;
      change[iChange].baseline = change[iChange].change = NULL;
      change[iChange].newType = request[iReq].typeCode;
      iChange++;
    } else {
      SDDS_SetColumnFlags(inSet, 0);
      if (!SDDS_SetColumnsOfInterest(inSet, SDDS_MATCH_STRING, request[iReq].columnName, SDDS_OR))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (request[iReq].excludeName) {
        if (!SDDS_SetColumnsOfInterest(inSet, SDDS_MATCH_STRING, request[iReq].excludeName, SDDS_NEGATE_MATCH | SDDS_AND))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!(columnName = SDDS_GetColumnNames(inSet, &columnNames))) {
        sprintf(s, "No columns selected for wildcard sequence '%s'", request[iReq].columnName);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (iChange + columnNames > *changes)
        change = SDDS_Realloc(change, sizeof(*change) * (*changes = iChange + columnNames + 10));
      for (iName = 0; iName < columnNames; iName++) {
        change[iChange + iName].sourceColumn = columnName[iName];
        change[iChange + iName].optionCode = request[iReq].optionCode;
        change[iChange + iName].change = change[iChange + iName].baseline = NULL;
        change[iChange + iName].type = SDDS_GetNamedColumnType(inSet, change[iChange].sourceColumn);
        if (change[iChange].optionCode == SET_CHANGESIN && !SDDS_NUMERIC_TYPE(SDDS_GetNamedColumnType(inSet, change[iChange].sourceColumn))) {
          fprintf(stderr, "Error: Column '%s' is non-numeric. Cannot compute changes.\n", change[iChange].sourceColumn);
          exit(EXIT_FAILURE);
        }
        change[iChange].copy = change[iChange].pass = NULL;
        change[iChange].baseline = change[iChange].change = NULL;
        change[iChange].newType = request[iReq].typeCode;
      }
      iChange += columnNames;
      free(columnName);
    }
  }

  *changes = iChange;
  for (iChange = 0; iChange < *changes; iChange++) {
    switch (change[iChange].optionCode) {
    case SET_COPY:
    case SET_PASS:
      strcpy(s, change[iChange].sourceColumn);
      break;
    case SET_CHANGESIN:
      sprintf(s, "ChangeIn%s", change[iChange].sourceColumn);
      break;
    }
    if (!SDDS_CopyString(&change[iChange].resultColumn, s))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  return change;
}

long setupOutputFile(SDDS_DATASET *outSet, char *output, SDDS_DATASET *inSet, CHANGE_DEFINITION *change, long changes, short columnMajorOrder) {
  long i;
  int32_t parameters;
  char **parameter = NULL, s[SDDS_MAXLINE];

  if (!SDDS_InitializeOutput(outSet, SDDS_BINARY, 0, NULL, "sddschanges output", output))
    return 0;
  if (columnMajorOrder != -1)
    outSet->layout.data_mode.column_major = columnMajorOrder;
  else
    outSet->layout.data_mode.column_major = inSet->layout.data_mode.column_major;
  if (!transferDefinitions(outSet, inSet, change, changes, SET_CHANGESIN))
    return 0;
  if (!transferDefinitions(outSet, inSet, change, changes, SET_COPY))
    return 0;
  if (!transferDefinitions(outSet, inSet, change, changes, SET_PASS))
    return 0;
  if ((parameter = SDDS_GetParameterNames(inSet, &parameters)) && parameters) {
    for (i = 0; i < parameters; i++)
      if (!SDDS_TransferParameterDefinition(outSet, inSet, parameter[i], NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < parameters; i++)
      free(parameter[i]);
    free(parameter);
  }
  if (!SDDS_WriteLayout(outSet)) {
    sprintf(s, "Unable to complete setup of output file");
    SDDS_SetError(s);
    return 0;
  }
  return 1;
}

long transferDefinitions(SDDS_DATASET *outSet, SDDS_DATASET *inSet, CHANGE_DEFINITION *change, long changes, long optionCode) {
  long column;
  char s[SDDS_MAXLINE], *symbol;

  for (column = 0; column < changes; column++) {
    if (optionCode != change[column].optionCode)
      continue;
    if (!SDDS_TransferColumnDefinition(outSet, inSet, change[column].sourceColumn, change[column].resultColumn)) {
      sprintf(s, "Problem transferring definition of column '%s'", change[column].sourceColumn);
      SDDS_SetError(s);
      return 0;
    }
    /* THIS CODE IS BROKEN
       if (change[column].newType > 0 && change[column].newType != change[column].type)
       {
       fprintf(stderr, "change[%d].newType=%d change[%d].type=%d\n", column, change[column].newType, column, change[column].type);
       if (!SDDS_ChangeColumnInformation(outSet, "type", SDDS_GetTypeName(change[column].newType), SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, change[column].resultColumn))
       {
       sprintf(s, "Problem changing type of column %s to make %s\n", change[column].sourceColumn, change[column].resultColumn);
       SDDS_SetError(s);
       return 0;
       }
       }
    */
    if (!SDDS_ChangeColumnInformation(outSet, "description", NULL, SDDS_SET_BY_NAME, change[column].resultColumn) ||
        SDDS_GetColumnInformation(outSet, "symbol", &symbol, SDDS_BY_NAME, change[column].resultColumn) != SDDS_STRING) {
      sprintf(s, "Unable to get/modify column '%s' information", change[column].sourceColumn);
      SDDS_SetError(s);
      return 0;
    }
    if (!symbol)
      SDDS_CopyString(&symbol, change[column].sourceColumn);
    switch (change[column].optionCode) {
    case SET_COPY:
    case SET_PASS:
      strcpy(s, symbol);
      break;
    case SET_CHANGESIN:
      sprintf(s, "ChangeIn[%s]", symbol);
      break;
    default:
      SDDS_Bomb("Invalid option code in transferDefinitions");
      break;
    }
    if (!SDDS_ChangeColumnInformation(outSet, "symbol", s, SDDS_BY_NAME, change[column].resultColumn))
      return 0;
  }
  return 1;
}
