/**
 * @file sddscollect.c
 * @brief Collects data from multiple columns into new grouped columns based on specified criteria.
 *
 * @details
 * This file processes SDDS (Self Describing Data Sets) files, enabling users to organize data by
 * collecting columns with matching suffixes, prefixes, or patterns into grouped columns.
 * It supports pipelined input/output, warning control, and row/column-major order selection.
 * Additionally, it enforces strict syntax rules for mutually exclusive options and specific
 * requirements for certain parameters.
 *
 * @section Usage
 * ```
 * sddscollect [<input>] [<output>]
 *             [-pipe=[input][,output]]
 *              -collect={suffix=<string>|prefix=<string>|match=<string>}[,column=<newName>][,editCommand=<string>][,exclude=<wildcard>] 
 *             [-nowarnings] 
 *             [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required  | Description                                                                 |
 * |-----------|-----------------------------------------------------------------------------|
 * | `-collect`                           | Collects columns based on specified suffix, prefix, or matching pattern.              |
 *
 * | Option                               | Description                                                                           |
 * |--------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                              | Enables standard SDDS toolkit pipe option for input and output.                       |
 * | `-nowarnings`                        | Suppresses warning messages.                                                         |
 * | `-majorOrder`                        | Specifies major order of the output file (row-major or column-major).                 |
 *
 * @subsection Incompatibilities
 *   - `-collect`:
 *     - Options `suffix`, `prefix`, and `match` are mutually exclusive.
 *     - When `match` is used, `editCommand` and `column` must also be provided.
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
 * M. Borland, R. Soliday, L. Emery
 */

#include "mdb.h"
#include "SDDS.h"
#include "SDDSutils.h"
#include "scan.h"
#include <ctype.h>

static char *USAGE =
  "sddscollect [<input>] [<output>]\n"
  "            [-pipe=[input][,output]]\n"
  "             -collect={suffix=<string>|prefix=<string>|match=<string>}[,column=<newName>][,editCommand=<string>][,exclude=<wildcard>]\n"
  "            [-nowarnings]\n"
  "            [-majorOrder=row|column]\n"
  "Options:\n"
  "  -pipe=[input][,output]\n"
  "        Use the standard SDDS toolkit pipe option for input and output.\n"
  "  -collect={suffix=<string>|prefix=<string>|match=<string>}\n"
  "        Collects columns based on the specified suffix, prefix, or matching pattern.\n"
  "        Additional parameters:\n"
  "          column=<newName>          (Optional) Name of the new column. Defaults to suffix or prefix.\n"
  "          editCommand=<string>      (Optional) Command to edit the column names.\n"
  "          exclude=<wildcard>        (Optional) Exclude columns matching the wildcard pattern.\n"
  "  -nowarnings\n"
  "        Suppresses warning messages.\n"
  "  -majorOrder=row|column\n"
  "        Specifies the major order of the output file. Can be either row-major or column-major.\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")";

/* Enumeration for option types */
enum option_type {
  CLO_COLLECT,
  CLO_PIPE,
  CLO_NOWARNINGS,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "collect",
  "pipe",
  "nowarnings",
  "majorOrder",
};

typedef struct
{
  char *part, *newColumn, *match, *editCommand, *exclude;
  char **oldColumn;
  void **data;
  long oldColumns, targetIndex, size;
  unsigned long flags;
} COLLECTION;

typedef struct
{
  char *name;
  void *data;
  long size, targetIndex;
} NEW_PARAMETER;

#define COLLECTION_SUFFIX 0x0001U
#define COLLECTION_PREFIX 0x0002U
#define COLLECTION_COLUMN 0x0004U
#define COLLECTION_MATCH 0x0008U
#define COLLECTION_EDIT 0x0010U
#define COLLECTION_EXCLUDE 0x0020U

long InitializeOutput(SDDS_DATASET *SDDSout, char *output, SDDS_DATASET *SDDSin, COLLECTION *collection, long collections, NEW_PARAMETER **newParameter, int *newParameters, char ***rootname, char ***units, long warnings);
void CollectAndWriteData(SDDS_DATASET *SDDSout, COLLECTION *collection, long collections, NEW_PARAMETER *newParameter, int newParameters, char **rootname, char **units, long rootnames, int64_t inputRow, long origPage);
void GetAndOrganizeData(SDDS_DATASET *SDDSin, COLLECTION *collection, long collections, NEW_PARAMETER *newParameter, int newParameters);
char **ConfirmMatchingColumns(COLLECTION *collection, long collections, SDDS_DATASET *SDDSin, SDDS_DATASET *SDDSout, long *rootnames, char ***units, long warnings);

int main(int argc, char **argv) {
  long iArg, ic;
  SDDS_DATASET SDDSin, SDDSout;
  SCANNED_ARG *scanned;
  unsigned long pipeFlags, flags, majorOrderFlag;
  COLLECTION *collection;
  NEW_PARAMETER *newParameter;
  char **rootname, **units;
  long collections, rootnames, code;
  int newParameters = 0;
  int64_t rows, row;
  char *input, *output;
  long warnings;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scanned, argc, argv);
  if (argc == 1) {
    bomb(NULL, USAGE);
    return EXIT_FAILURE;
  }

  input = output = NULL;
  collection = NULL;
  collections = 0;
  pipeFlags = 0;
  warnings = 1;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("Invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_COLLECT:
        if (!(collection = SDDS_Realloc(collection, sizeof(*collection) * (collections + 1)))) {
          SDDS_Bomb("Memory allocation failure");
        }
        ic = collections;
        collection[ic].newColumn = collection[ic].part = collection[ic].match = collection[ic].editCommand = NULL;
        collection[ic].exclude = NULL;
        if (--scanned[iArg].n_items == 0 ||
            !scanItemList(&flags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "suffix", SDDS_STRING, &collection[ic].part, 1, COLLECTION_SUFFIX,
                          "prefix", SDDS_STRING, &collection[ic].part, 1, COLLECTION_PREFIX,
                          "column", SDDS_STRING, &collection[ic].newColumn, 1, COLLECTION_COLUMN,
                          "match", SDDS_STRING, &collection[ic].match, 1, COLLECTION_MATCH,
                          "editcommand", SDDS_STRING, &collection[ic].editCommand, 1, COLLECTION_EDIT,
                          "exclude", SDDS_STRING, &collection[ic].exclude, 1, COLLECTION_EXCLUDE, NULL) ||
            ((flags & COLLECTION_SUFFIX && flags & COLLECTION_PREFIX) ||
             (flags & COLLECTION_SUFFIX && flags & COLLECTION_MATCH) ||
             (flags & COLLECTION_PREFIX && flags & COLLECTION_MATCH))) {
          SDDS_Bomb("Invalid -collect syntax");
        }
        if (flags & COLLECTION_MATCH &&
            (!(flags & COLLECTION_EDIT) || !(flags & COLLECTION_COLUMN))) {
          SDDS_Bomb("Invalid -collect syntax: must give editCommand and column with match");
        }
        collection[ic].flags = flags;
        collections++;
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags)) {
          SDDS_Bomb("Invalid -pipe syntax");
        }
        break;
      case CLO_NOWARNINGS:
        warnings = 0;
        break;
      default:
        fprintf(stderr, "Invalid option seen: %s\n", scanned[iArg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else
        SDDS_Bomb("Too many filenames");
    }
  }

  if (!collections) {
    SDDS_Bomb("At least one -collect option must be given");
  }

  processFilenames("sddscollect", &input, &output, pipeFlags, !warnings, NULL);

  if (!SDDS_InitializeInput(&SDDSin, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  rootnames = InitializeOutput(&SDDSout, output, &SDDSin, collection, collections,
                               &newParameter, &newParameters, &rootname, &units, warnings);
  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;

  while ((code = SDDS_ReadPage(&SDDSin)) > 0) {
    if (!SDDS_StartPage(&SDDSout, rootnames) ||
        !SDDS_CopyParameters(&SDDSout, &SDDSin) ||
        !SDDS_CopyArrays(&SDDSout, &SDDSin)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if ((rows = SDDS_CountRowsOfInterest(&SDDSin)) > 0) {
      GetAndOrganizeData(&SDDSin, collection, collections, newParameter, newParameters);
      for (row = 0; row < rows; row++) {
        CollectAndWriteData(&SDDSout, collection, collections, newParameter, newParameters,
                            rootname, units, rootnames, row, code);
        if (row != rows - 1 && !SDDS_StartPage(&SDDSout, rootnames)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    } else if (!SDDS_WritePage(&SDDSout)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!code || !SDDS_Terminate(&SDDSout) || !SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  return EXIT_SUCCESS;
}

void CollectAndWriteData(SDDS_DATASET *SDDSout, COLLECTION *collection, long collections,
                         NEW_PARAMETER *newParameter, int newParameters, char **rootname,
                         char **units, long rootnames, int64_t inputRow, long origPage) {
  long ic, ip, ir;
  if (rootnames) {
    if (!SDDS_SetColumn(SDDSout, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, rootname, rootnames, "Rootname")) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_SetColumn(SDDSout, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, units, rootnames, "Units")) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    for (ic = 0; ic < collections; ic++) {
      for (ir = 0; ir < rootnames; ir++) {
        if (!SDDS_SetRowValues(SDDSout, SDDS_PASS_BY_REFERENCE | SDDS_SET_BY_INDEX,
                               ir, collection[ic].targetIndex,
                               ((char *)(collection[ic].data[ir])) + inputRow * collection[ic].size, -1)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    }
  }

  for (ip = 0; ip < newParameters; ip++) {
    if (!SDDS_SetParameters(SDDSout, SDDS_PASS_BY_REFERENCE | SDDS_SET_BY_NAME,
                            newParameter[ip].name,
                            ((char *)(newParameter[ip].data) + inputRow * newParameter[ip].size), NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!SDDS_SetParameters(SDDSout, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME,
                          "OriginalPage", origPage, NULL) ||
      !SDDS_WritePage(SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}

void GetAndOrganizeData(SDDS_DATASET *SDDSin, COLLECTION *collection, long collections,
                        NEW_PARAMETER *newParameter, int newParameters) {
  long ic, ip, ii;
  for (ic = 0; ic < collections; ic++) {
    for (ii = 0; ii < collection[ic].oldColumns; ii++) {
      if (!(collection[ic].data[ii] = SDDS_GetInternalColumn(SDDSin, collection[ic].oldColumn[ii]))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }
  for (ip = 0; ip < newParameters; ip++) {
    if (!(newParameter[ip].data = SDDS_GetColumn(SDDSin, newParameter[ip].name))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
}

long InitializeOutput(SDDS_DATASET *SDDSout, char *output, SDDS_DATASET *SDDSin,
                      COLLECTION *collection, long collections, NEW_PARAMETER **newParameter,
                      int *newParameters, char ***rootname, char ***units, long warnings) {
  char **inputColumn, *partString;
  long partLength, *inputLength;
  int32_t inputColumns;
  short *inputUsed;
  long ic, ii, ip, inputsUsed, rootnames;
  char *matchString, *excludeString;

  partLength = 0;
  inputLength = NULL;

  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddscollect output", output) ||
      !SDDS_TransferAllParameterDefinitions(SDDSout, SDDSin, 0) ||
      !SDDS_TransferAllArrayDefinitions(SDDSout, SDDSin, 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!(inputColumn = SDDS_GetColumnNames(SDDSin, &inputColumns)) || inputColumns == 0) {
    SDDS_Bomb("No columns in input file");
  }
  if (!(inputUsed = (short *)calloc(inputColumns, sizeof(*inputUsed))) ||
      !(inputLength = (long *)calloc(inputColumns, sizeof(*inputLength)))) {
    SDDS_Bomb("Memory allocation failure");
  }
  for (ii = 0; ii < inputColumns; ii++)
    inputLength[ii] = strlen(inputColumn[ii]);

  inputsUsed = 0;
  matchString = NULL;
  excludeString = NULL;
  for (ic = 0; ic < collections; ic++) {
    if (!collection[ic].newColumn)
      SDDS_CopyString(&collection[ic].newColumn, collection[ic].part);
    if ((partString = collection[ic].part))
      partLength = strlen(partString);
    if (collection[ic].match) {
      matchString = collection[ic].match;
      partLength = -1;
    }
    if (collection[ic].exclude) {
      excludeString = collection[ic].exclude;
    }
    collection[ic].oldColumn = NULL;
    collection[ic].oldColumns = 0;
    for (ii = 0; ii < inputColumns; ii++) {
      if (inputUsed[ii])
        continue;
      if (partLength >= inputLength[ii])
        continue;
      if (matchString) {
        if (wild_match(inputColumn[ii], matchString)) {
          if ((excludeString == NULL) || (!wild_match(inputColumn[ii], excludeString))) {
            if (!(collection[ic].oldColumn = SDDS_Realloc(collection[ic].oldColumn,
                                                          sizeof(*collection[ic].oldColumn) * (collection[ic].oldColumns + 1)))) {
              SDDS_Bomb("Memory allocation failure");
            }
            collection[ic].oldColumn[collection[ic].oldColumns] = inputColumn[ii];
            inputUsed[ii] = 1;
            inputsUsed++;
            collection[ic].oldColumns++;
          }
        }
      } else if (collection[ic].flags & COLLECTION_PREFIX) {
        if (strncmp(partString, inputColumn[ii], partLength) == 0) {
          if (!(collection[ic].oldColumn = SDDS_Realloc(collection[ic].oldColumn,
                                                        sizeof(*collection[ic].oldColumn) * (collection[ic].oldColumns + 1)))) {
            SDDS_Bomb("Memory allocation failure");
          }
          collection[ic].oldColumn[collection[ic].oldColumns] = inputColumn[ii];
          inputUsed[ii] = 1;
          inputsUsed++;
          collection[ic].oldColumns++;
        }
      } else {
        if (strcmp(partString, inputColumn[ii] + inputLength[ii] - partLength) == 0) {
          if (!(collection[ic].oldColumn = SDDS_Realloc(collection[ic].oldColumn,
                                                        sizeof(*collection[ic].oldColumn) * (collection[ic].oldColumns + 1)))) {
            SDDS_Bomb("Memory allocation failure");
          }
          collection[ic].oldColumn[collection[ic].oldColumns] = inputColumn[ii];
          inputUsed[ii] = 1;
          inputsUsed++;
          collection[ic].oldColumns++;
        }
      }
    }
    if (!collection[ic].oldColumns && warnings) {
      fprintf(stderr, "Warning (sddscollect): No columns in input for %s %s\n",
              collection[ic].flags & COLLECTION_PREFIX ? "prefix" : "suffix", collection[ic].part);
    }
    if (!(collection[ic].data = (void **)calloc(collection[ic].oldColumns, sizeof(*collection[ic].data)))) {
      SDDS_Bomb("Memory allocation failure");
    }
  }

  if ((*newParameters = inputColumns - inputsUsed)) {
    *newParameter = (NEW_PARAMETER *)malloc(sizeof(**newParameter) * (*newParameters));
    for (ii = ip = 0; ii < inputColumns; ii++) {
      if (inputUsed[ii])
        continue;
      if (!SDDS_CopyString(&(*newParameter)[ip].name, inputColumn[ii]) ||
          !SDDS_DefineParameterLikeColumn(SDDSout, SDDSin, inputColumn[ii], NULL) ||
          ((*newParameter)[ip].targetIndex = SDDS_GetParameterIndex(SDDSout, inputColumn[ii])) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      (*newParameter)[ip].size = SDDS_GetTypeSize(SDDS_GetParameterType(SDDSout, (*newParameter)[ip].targetIndex));
      ip++;
    }
  }

  *rootname = ConfirmMatchingColumns(collection, collections, SDDSin, SDDSout, &rootnames, units, warnings);

  if (SDDS_DefineParameter(SDDSout, "OriginalPage", NULL, NULL, NULL, NULL, SDDS_LONG, NULL) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (rootnames &&
      (SDDS_DefineColumn(SDDSout, "Rootname", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0 ||
       SDDS_DefineColumn(SDDSout, "Units", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_WriteLayout(SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  return rootnames;
}

char **ConfirmMatchingColumns(COLLECTION *collection, long collections, SDDS_DATASET *SDDSin,
                              SDDS_DATASET *SDDSout, long *rootnames, char ***units, long warnings) {
  long ic, ip, ii, partLength;
  char **rootname, editBuffer[1024];
  char saveChar;

  partLength = 0;
  rootname = NULL;
  *units = NULL;
  *rootnames = 0;
  if (!collections)
    return NULL;
  for (ic = 0; ic < collections; ic++) {
    if (!collection[ic].oldColumns)
      continue;
    if (collection[ic].part)
      partLength = strlen(collection[ic].part);
    if (collection[ic].part && collection[ic].flags & COLLECTION_SUFFIX) {
      /* sort collection according to truncated string */
      saveChar = collection[ic].part[0];
      for (ip = 0; ip < collection[ic].oldColumns; ip++)
        collection[ic].oldColumn[ip][strlen(collection[ic].oldColumn[ip]) - partLength] = 0;
      qsort(collection[ic].oldColumn, collection[ic].oldColumns, sizeof(*collection[ic].oldColumn), string_cmpasc);
      /* now restore the strings (column names) to original form */
      for (ip = 0; ip < collection[ic].oldColumns; ip++)
        collection[ic].oldColumn[ip][strlen(collection[ic].oldColumn[ip])] = saveChar;
    } else {
      qsort(collection[ic].oldColumn, collection[ic].oldColumns, sizeof(*collection[ic].oldColumn), string_cmpasc);
    }
    if (rootname)
      continue;
    *rootnames = collection[ic].oldColumns;
    if (!(rootname = (char **)malloc(sizeof(*rootname) * (*rootnames)))) {
      SDDS_Bomb("Memory allocation failure");
    }
    if (!(*units = (char **)malloc(sizeof(**units) * (*rootnames)))) {
      SDDS_Bomb("Memory allocation failure");
    }
    for (ip = 0; ip < (*rootnames); ip++) {
      if (SDDS_GetColumnInformation(SDDSin, "units", *units + ip, SDDS_GET_BY_NAME, collection[ic].oldColumn[ip]) != SDDS_STRING) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (collection[ic].flags & COLLECTION_EDIT) {
        strcpy(editBuffer, collection[ic].oldColumn[ip]);
        if (!edit_string(editBuffer, collection[ic].editCommand)) {
          SDDS_Bomb("Problem editing column name.");
        }
        SDDS_CopyString(rootname + ip, editBuffer);
      } else if (collection[ic].flags & COLLECTION_PREFIX) {
        SDDS_CopyString(rootname + ip, collection[ic].oldColumn[ip] + partLength);
      } else {
        SDDS_CopyString(rootname + ip, collection[ic].oldColumn[ip]);
        rootname[ip][strlen(rootname[ip]) - partLength] = 0;
      }
    }
  }
  if (!(*rootnames))
    return NULL;

  for (ic = 0; ic < collections; ic++) {
    if (!collection[ic].oldColumns)
      continue;
    if (collection[ic].oldColumns != (*rootnames)) {
      fprintf(stderr, "Error (sddscollect): Groups have different numbers of members\n");
      for (ip = 0; ip < collections; ip++) {
        fprintf(stderr, "%ld in %s\n", collection[ip].oldColumns,
                collection[ip].part ? collection[ip].part : collection[ip].match);
      }
      exit(EXIT_FAILURE);
    }
    if (collection[ic].flags & COLLECTION_MATCH)
      continue;
    for (ip = 0; ip < collection[ic].oldColumns; ip++)
      if (strstr(collection[ic].oldColumn[ip], rootname[ip]) == NULL) {
        fprintf(stderr, "Error (sddscollect): Mismatch with rootname %s for column %s in group %s\n",
                rootname[ip], collection[ic].oldColumn[ip],
                collection[ic].part ? collection[ic].part : collection[ic].match);
        /* about to bomb here, so it's okay to reuse these indices */
        for (ic = 0; ic < collections; ic++) {
          fprintf(stderr, "Group %s (%ld):\n",
                  collection[ic].part ? collection[ic].part : collection[ic].match, ic);
          for (ip = 0; ip < collection[ic].oldColumns; ip++)
            fprintf(stderr, "  old column[%ld] = %s\n", ip, collection[ic].oldColumn[ip]);
        }
        exit(EXIT_FAILURE);
      }
  }

  for (ic = 0; ic < collections; ic++) {
    char *units;
    int32_t type = 0;
    long unitsMismatch;
    if (!collection[ic].oldColumns)
      continue;
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, collection[ic].oldColumn[0], collection[ic].newColumn) ||
        (collection[ic].targetIndex = SDDS_GetColumnIndex(SDDSout, collection[ic].newColumn)) < 0 ||
        SDDS_GetColumnInformation(SDDSout, "units", &units, SDDS_GET_BY_NAME, collection[ic].newColumn) != SDDS_STRING ||
        SDDS_GetColumnInformation(SDDSout, "type", &type, SDDS_GET_BY_NAME, collection[ic].newColumn) != SDDS_LONG) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    collection[ic].size = SDDS_GetTypeSize(type);
    unitsMismatch = 0;
    for (ii = 1; ii < collection[ic].oldColumns; ii++) {
      if (SDDS_CheckColumn(SDDSin, collection[ic].oldColumn[ii], NULL, type, stderr) == SDDS_CHECK_WRONGTYPE) {
        fprintf(stderr, "Error (sddscollect): Inconsistent data types for suffix/prefix/match %s\n",
                collection[ic].part ? collection[ic].part : collection[ic].match);
        exit(EXIT_FAILURE);
      }
      if (SDDS_CheckColumn(SDDSin, collection[ic].oldColumn[ii], units, type, NULL) == SDDS_CHECK_WRONGUNITS)
        unitsMismatch = 1;
    }
    if (unitsMismatch) {
      if (warnings)
        fprintf(stderr, "Warning (sddscollect): Inconsistent units for suffix/prefix %s\n", collection[ic].part);
      if (!SDDS_ChangeColumnInformation(SDDSout, "units", "?", SDDS_BY_NAME, collection[ic].newColumn)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }

  return rootname;
}
