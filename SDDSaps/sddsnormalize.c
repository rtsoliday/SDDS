/**
 * @file sddsnormalize.c
 * @brief A program for SDDS-format column normalization.
 *
 * @details
 * This program normalizes the specified columns of an SDDS file based on user-defined options. 
 * It provides various statistical modes for normalization, supports multithreading, and 
 * offers flexibility in data handling with features like custom suffixes and column exclusions.
 *
 * @section Usage
 * ```
 * sddsnormalize [<inputfile>] [<outputfile>]
 *               [-pipe=[input][,output]]
 *               -columns=[mode=<mode>,][suffix=<string>,][exclude=<wildcardString>,][functionOf=<columnName>,]<columnName>[,...]
 *               [-threads=<number>]
 *               [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Specifies the columns to normalize and their modes of normalization.                  |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Specifies whether the input/output is piped.                                         |
 * | `-threads`                            | Defines the number of threads for parallel normalization.                            |
 * | `-majorOrder`                         | Sets the processing order as row-major or column-major.                              |
 *
 * | Column Modes                          | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `minimum`                             | Use the minimum value as the normalization factor.                                    |
 * | `maximum`                             | Use the maximum value as the normalization factor.                                    |
 * | `largest`                             | Use the larger of \|min\| or \|max\| (default).                                           |
 * | `signedlargest`                       | Use the largest value with its sign retained.                                         |
 * | `spread`                              | Use (max - min) as the normalization factor.                                          |
 * | `rms`                                 | Use the root-mean-square of the values.                                               |
 * | `standarddeviation`                   | Use the n-1 weighted standard deviation.                                              |
 * | `sum`                                 | Use the sum of all values.                                                            |
 * | `area`                                | Use the area under the curve (requires functionOf).                                   |
 * | `average`                             | Use the average of all values.                                                        |
 *
 * @subsection spec_req Specific Requirements
 *   - For the `area` mode in `-columns`, the `functionOf` qualifier must be provided.
 *   - The `-columns` mode defaults to `largest` if not specified.
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
 * M. Borland, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  CLO_COLUMNS,
  CLO_PIPE,
  CLO_MAJOR_ORDER,
  CLO_THREADS,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "columns",
  "pipe",
  "majorOrder",
  "threads",
};

static char *USAGE =
  "Usage: sddsnormalize [<inputfile>] [<outputfile>] \n"
  "    [-pipe=[input][,output]] \n"
  "    -columns=[mode=<mode>,][suffix=<string>,][exclude=<wildcardString>,][functionOf=<columnName>,]<columnName>[,...] \n"
  "    [-threads=<number>] \n"
  "    [-majorOrder=row|column] \n\n"
  "Options:\n"
  "  <mode>       Specifies the normalization mode. Available modes are:\n"
  "               minimum, maximum, largest, signedlargest,\n"
  "               spread, rms, standarddeviation, sum, area, or average.\n"
  "               - minimum      : Use the minimum value as the normalization factor.\n"
  "               - maximum      : Use the maximum value as the normalization factor.\n"
  "               - largest      : Use the larger of |min| or |max| (default).\n"
  "               - signedlargest: Use the largest value with its sign retained.\n"
  "               - spread       : Use (max - min) as the normalization factor.\n"
  "               - rms          : Use the root-mean-square of the values.\n"
  "               - standarddeviation: Use the n-1 weighted standard deviation.\n"
  "               - sum          : Use the sum of all values.\n"
  "               - area         : Use the area under the curve (requires functionOf).\n"
  "               - average      : Use the average of all values.\n"
  "  <string>     Specifies a suffix to append to the column name for the normalized output.\n"
  "               If omitted, the original column is replaced.\n"
  "  <wildcardString> Excludes columns matching the wildcard pattern from normalization.\n"
  "  <columnName> Specifies the column(s) to normalize. Multiple columns can be separated by commas.\n"
  "  <number>     Specifies the number of threads to use for normalization.\n"
  "  row|column   Specifies the major order for data processing.\n\n"
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

/* different modes for normalizing */
#define NORM_MINIMUM 0
#define NORM_MAXIMUM 1
#define NORM_LARGEST 2
#define NORM_SLARGEST 3
#define NORM_SPREAD 4
#define NORM_RMS 5
#define NORM_STDEV 6
#define NORM_SUM 7
#define NORM_AREA 8
#define NORM_AVERAGE 9
#define NORM_OPTIONS 10
static char *normMode[NORM_OPTIONS] = {
  "minimum",
  "maximum",
  "largest",
  "signedlargest",
  "spread",
  "rms",
  "standarddeviation",
  "sum",
  "area",
  "average",
};

/* structure for users requests to normalize */
#define FL_SUFFIX_GIVEN 0x0001U
#define FL_MODE_GIVEN 0x0002U
#define FL_FUNCOF_GIVEN 0x0004U
typedef struct
{
  unsigned long flags;
  char *suffix, **source, *exclude, *functionOf;
  long sources, mode;
} NORM_REQUEST;

/* individual specifications for one column, made from
 * users request after expanding wildcards and lists
 */
typedef struct
{
  unsigned long flags;
  char *source, *target, *functionOf;
  long mode;
} NORM_SPEC;

long resolveColumnNames(SDDS_DATASET *SDDSin, NORM_REQUEST *normRequest, long normRequests, NORM_SPEC **normSpecRet, long *normSpecsRet);

int main(int argc, char **argv) {
  int iArg;
  NORM_REQUEST *normRequest;
  NORM_SPEC *normSpec;
  long normRequests, normSpecs, i, readCode;
  int64_t j, rows;
  char *input, *output, *modeString;
  unsigned long pipeFlags, majorOrderFlag;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  double *data, *funcOfData, factor, min, max;
  short columnMajorOrder = -1;
  int threads = 1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  output = input = NULL;
  pipeFlags = 0;
  normRequest = NULL;
  normSpec = NULL;
  normRequests = normSpecs = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 && (!scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_COLUMNS:
        if (!(normRequest = SDDS_Realloc(normRequest, sizeof(*normRequest) * (normRequests + 1))))
          SDDS_Bomb("memory allocation failure");
        normRequest[normRequests].exclude = normRequest[normRequests].suffix = NULL;
        if (!scanItemList(&normRequest[normRequests].flags,
                          scanned[iArg].list, &scanned[iArg].n_items,
                          SCANITEMLIST_UNKNOWN_VALUE_OK | SCANITEMLIST_REMOVE_USED_ITEMS |
                          SCANITEMLIST_IGNORE_VALUELESS,
                          "mode", SDDS_STRING, &modeString, 1, FL_MODE_GIVEN,
                          "suffix", SDDS_STRING, &normRequest[normRequests].suffix, 1, FL_SUFFIX_GIVEN,
                          "functionof", SDDS_STRING, &normRequest[normRequests].functionOf, 1, FL_FUNCOF_GIVEN,
                          "exclude", SDDS_STRING, &normRequest[normRequests].exclude, 1, 0, NULL))
          SDDS_Bomb("invalid -columns syntax");
        if (normRequest[normRequests].flags & FL_MODE_GIVEN) {
          if ((normRequest[normRequests].mode = match_string(modeString, normMode, NORM_OPTIONS, 0)) < 0)
            SDDS_Bomb("invalid -columns syntax: unknown mode");
        } else
          normRequest[normRequests].mode = NORM_LARGEST;
        if (scanned[iArg].n_items < 1)
          SDDS_Bomb("invalid -columns syntax: no columns listed");
        normRequest[normRequests].source = scanned[iArg].list + 1;
        normRequest[normRequests].sources = scanned[iArg].n_items - 1;
        normRequests++;
        break;
      case CLO_THREADS:
        if (scanned[iArg].n_items != 2 ||
            !sscanf(scanned[iArg].list[1], "%d", &threads) || threads < 1)
          SDDS_Bomb("invalid -threads syntax");
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s\n", scanned[iArg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  processFilenames("sddsnormalize", &input, &output, pipeFlags, 0, NULL);

  if (!normRequests)
    SDDS_Bomb("supply the names of columns to normalize with the -columns option");

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!resolveColumnNames(&SDDSin, normRequest, normRequests, &normSpec, &normSpecs))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!normSpecs)
    SDDS_Bomb("no columns selected for normalization");

  if (!SDDS_InitializeCopy(&SDDSout, &SDDSin, output, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;
  for (i = 0; i < normSpecs; i++) {
    if (normSpec[i].flags & FL_SUFFIX_GIVEN) {
      if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, normSpec[i].source, normSpec[i].target) ||
          !SDDS_ChangeColumnInformation(&SDDSout, "units", "", SDDS_BY_NAME, normSpec[i].target))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (!SDDS_ChangeColumnInformation(&SDDSout, "units", "Normalized", SDDS_BY_NAME, normSpec[i].target))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    if (!SDDS_CopyPage(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((rows = SDDS_RowCount(&SDDSin))) {
      for (i = 0; i < normSpecs; i++) {
        if (!(data = SDDS_GetColumnInDoubles(&SDDSin, normSpec[i].source)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        funcOfData = NULL;
        if (normSpec[i].functionOf &&
            !(funcOfData = SDDS_GetColumnInDoubles(&SDDSin, normSpec[i].functionOf)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!find_min_max(&min, &max, data, rows))
          min = max = 1;
        switch (normSpec[i].mode) {
        case NORM_RMS:
          factor = rmsValueThreaded(data, rows, threads);
          break;
        case NORM_STDEV:
          factor = standardDeviationThreaded(data, rows, threads);
          break;
        case NORM_MINIMUM:
          factor = min;
          break;
        case NORM_MAXIMUM:
          factor = max;
          break;
        case NORM_LARGEST:
          min = fabs(min);
          max = fabs(max);
          factor = MAX(min, max);
          break;
        case NORM_SLARGEST:
          if (fabs(min) > fabs(max))
            factor = min;
          else
            factor = max;
          break;
        case NORM_SPREAD:
          factor = max - min;
          break;
        case NORM_SUM:
          for (j = factor = 0; j < rows; j++)
            factor += data[j];
          break;
        case NORM_AREA:
          if (!funcOfData)
            SDDS_Bomb("functionOf qualifier must be given for area normalization");
          trapazoidIntegration(funcOfData, data, rows, &factor);
          break;
        case NORM_AVERAGE:
          for (j = factor = 0; j < rows; j++)
            factor += data[j];
          factor /= rows;
          break;
        default:
          SDDS_Bomb("Invalid normalization mode---programming error");
          break;
        }
        if (funcOfData)
          free(funcOfData);
        if (factor)
          for (j = 0; j < rows; j++)
            data[j] /= factor;
        if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, data, rows, normSpec[i].target))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        free(data);
      }
    }
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }
  if (!SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

long resolveColumnNames(SDDS_DATASET *SDDSin, NORM_REQUEST *normRequest, long normRequests, NORM_SPEC **normSpecRet, long *normSpecsRet) {
  long i, j;
  int32_t columns;
  char **column, buffer[1024];
  long normSpecs = 0;
  NORM_SPEC *normSpec = NULL;

  for (i = 0; i < normRequests; i++) {
    SDDS_SetColumnFlags(SDDSin, 0);
    if (normRequest[i].flags & FL_SUFFIX_GIVEN) {
      if (!normRequest[i].suffix || !strlen(normRequest[i].suffix)) {
        SDDS_SetError("resolveColumnNames: missing or blank suffix");
        return 0;
      }
    }
    for (j = 0; j < normRequest[i].sources; j++) {
      if (!SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, normRequest[i].source[j], SDDS_OR)) {
        SDDS_SetError("resolveColumnNames: SDDS_SetColumnsOfInterest error");
        return 0;
      }
    }
    if (normRequest[i].exclude &&
        !SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, normRequest[i].exclude, SDDS_NEGATE_MATCH | SDDS_AND)) {
      SDDS_SetError("resolveColumnNames: SDDS_SetColumnsOfInterest error");
      return 0;
    }
    if (!(column = SDDS_GetColumnNames(SDDSin, &columns)) || columns == 0) {
      sprintf(buffer, "No match for column list: ");
      for (j = 0; j < normRequest[i].sources; j++) {
        strcat(buffer, normRequest[i].source[j]);
        if (j != normRequest[i].sources - 1)
          strcat(buffer, ", ");
      }
      SDDS_SetError(buffer);
      return 0;
    }
    if (!(normSpec = SDDS_Realloc(normSpec, sizeof(*normSpec) * (normSpecs + columns)))) {
      SDDS_SetError("resolveColumnNames: Memory allocation failure");
      return 0;
    }
    for (j = 0; j < columns; j++) {
      normSpec[j + normSpecs].source = column[j];
      normSpec[j + normSpecs].mode = normRequest[i].mode;
      normSpec[j + normSpecs].flags = normRequest[i].flags;
      normSpec[j + normSpecs].functionOf = NULL;
      if (normRequest[i].flags & FL_FUNCOF_GIVEN) {
        if (!SDDS_CopyString(&normSpec[j + normSpecs].functionOf, normRequest[i].functionOf)) {
          SDDS_SetError("resolveColumnNames: Memory allocation failure");
          return 0;
        }
      }
      normSpec[j + normSpecs].target = NULL;
      if (normRequest[i].flags & FL_SUFFIX_GIVEN) {
        sprintf(buffer, "%s%s", normSpec[j + normSpecs].source, normRequest[i].suffix);
        if (!SDDS_CopyString(&normSpec[j + normSpecs].target, buffer)) {
          SDDS_SetError("resolveColumnNames: Memory allocation failure");
          return 0;
        }
      } else
        normSpec[j + normSpecs].target = normSpec[j + normSpecs].source;
    }
    normSpecs += columns;
  }
  *normSpecRet = normSpec;
  *normSpecsRet = normSpecs;
  return 1;
}
