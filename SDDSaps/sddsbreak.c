/**
 * @file sddsbreak.c
 * @brief Splits pages of an SDDS file into subpages based on user-defined criteria.
 *
 * @details
 * This file provides the implementation of the `sddsbreak` program, which processes
 * SDDS (Self Describing Data Set) files by dividing their pages into subpages
 * according to various user-specified conditions. Supported options include breaking
 * based on gaps, increases, or decreases in column values, matching patterns, and
 * imposing row limits. The program supports both row-major and column-major order.
 *
 * @section Usage
 * ```
 * sddsbreak [<inputfile>] [<outputfile>]
 *           [-pipe=[input][,output]]
 *           [-gapin=<column-name>[,{amount=<value>|factor=<value>}]]
 *           [-increaseof=<column-name>[,{amount=<value>}[,cumulative[,reset]]]]
 *           [-decreaseof=<column-name>[,{amount=<value>}[,cumulative[,reset]]]]
 *           [-changeof=<column-name>[,amount=<value>,base=<value>]] 
 *           [-matchto=<column-name>,<pattern>[,after]] 
 *           [-rowlimit=<integer>[,overlap=<integer>]]
 *           [-pagesPerPage=<integer>]
 *           [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Option                     | Description                                                                                     |
 * |----------------------------|-------------------------------------------------------------------------------------------------|
 * | `-pipe`                   | Use pipes for input and/or output.                                                             |
 * | `-gapin`                  | Break pages based on gaps in the specified column.                                             |
 * | `-increaseof`             | Break pages when the specified column increases by a certain amount, with optional cumulative reset. |
 * | `-decreaseof`             | Break pages when the specified column decreases by a certain amount, with optional cumulative reset. |
 * | `-changeof`               | Break pages based on changes in the specified column relative to a base value.                 |
 * | `-matchto`                | Break pages when a pattern is matched in the specified column.                                 |
 * | `-rowlimit`               | Limit the number of rows per subpage, optionally overlapping rows.                             |
 * | `-pagesPerPage`           | Break each page into the given number of roughly equal-length pages.                           |
 * | `-majorOrder`             | Specify the major order of data as row-major or column-major.                                  |
 *
 * @subsection Incompatibilities
 *   - `-gapin`, `-increaseof`, `-decreaseof`, `-changeof`, and `-matchto` are mutually exclusive; only one may be used.
 *   - `-rowlimit` cannot be combined with other break conditions.
 *   - `-pagesPerPage` cannot be combined with other break conditions.
 *   - For `-gapin`, either `amount` or `factor` must be specified, but not both.
 *   - For `-changeof`, at least one of `amount` or `base` must be specified.
 *
 * @subsection Requirements
 *   - For `-increaseof` and `-decreaseof`, the `amount` must be positive.
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
 *  M. Borland,
 *  C. Saunders,
 *  R. Soliday,
 *  H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_GAPIN,
  SET_INCREASEOF,
  SET_DECREASEOF,
  SET_CHANGEOF,
  SET_ROWLIMIT,
  SET_PIPE,
  SET_MATCHTO,
  SET_MAJOR_ORDER,
  SET_PAGES_PER_PAGE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "gapin",
  "increaseof",
  "decreaseof",
  "changeof",
  "rowlimit",
  "pipe",
  "matchto",
  "majorOrder",
  "pagesperpage"
};

char *USAGE =
  "Usage: sddsbreak [<inputfile>] [<outputfile>]\n"
  "          [-pipe=[input][,output]]\n"
  "          [-gapin=<column-name>[,{amount=<value>|factor=<value>}]]\n"
  "          [-increaseof=<column-name>[,{amount=<value>}[,cumulative[,reset]]]]\n"
  "          [-decreaseof=<column-name>[,{amount=<value>}[,cumulative[,reset]]]]\n"
  "          [-changeof=<column-name>[,amount=<value>,base=<value>]] \n"
  "          [-matchto=<column-name>,<pattern>[,after]] \n"
  "          [-rowlimit=<integer>[,overlap=<integer>]]\n"
  "          [-pagesPerPage=<integer>]\n"
  "          [-majorOrder=row|column]\n"
  "Options:\n"
  "  -pipe=[input][,output]\n"
  "      Use pipes for input and/or output.\n"
  "  -gapin=<column-name>[,{amount=<value> | factor=<value>}]\n"
  "      Break pages based on gaps in the specified column.\n"
  "  -increaseof=<column-name>[,amount=<value>[,cumulative][,reset]]\n"
  "      Break pages when the specified column increases by a certain amount.\n"
  "  -decreaseof=<column-name>[,amount=<value>[,cumulative][,reset]]\n"
  "      Break pages when the specified column decreases by a certain amount.\n"
  "  -changeof=<column-name>[,amount=<value>,base=<value>]\n"
  "      Break pages based on changes in the specified column relative to a base value.\n"
  "  -matchto=<column-name>,<pattern>[,after]\n"
  "      Break pages when a pattern is matched in the specified column.\n"
  "  -rowlimit=<integer>[,overlap=<integer>]\n"
  "      Limit the number of rows per subpage with an optional overlap.\n"
  "  -pagesPerPage=<integer>\n"
  "      Break each page into the given number of roughly equal-length pages.\n"
  "  -majorOrder=row|column\n"
  "      Specify the major order of data as row-major or column-major.\n"
  "\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define GAPIN_AMOUNT 0x0001U
#define GAPIN_FACTOR 0x0002U
char *gapinUsage = "-gapin=<column-name>[,{amount=<value> | factor=<value>}]";

#define CHANGEOF_AMOUNT 0x0001U
#define CHANGEOF_BASE 0x0002U
char *changeofUsage = "-changeof=<column-name>[,amount=<value>,base=<value>]";

#define INCREASEOF_AMOUNT 0x0001UL
#define INCREASEOF_CUMULATIVE 0x0002UL
#define INCREASEOF_RESET 0x0004UL
char *increaseOfUsage = "-increaseOf=<column-name>[,amount=<value>[,cumulative][,reset]]";

#define DECREASEOF_AMOUNT 0x0001UL
#define DECREASEOF_CUMULATIVE 0x0002UL
#define DECREASEOF_RESET 0x0004UL
char *decreaseOfUsage = "-decreaseOf=<column-name>[,amount=<value>[,cumulative[,reset]]]";

#define ROWLIMIT_OVERLAP 0x0001U

int main(int argc, char **argv) {
  SDDS_DATASET SDDSnew, SDDSold;
  long iArg;
  SCANNED_ARG *scArg;
  char *input = NULL, *output = NULL, *columnName = NULL;
  long mode = -1, matchCode, tmpfile_used;
  int64_t i, j, rows, rowLimit, pagesPerPage=0;
  long breakNext;
  double gapAmount = 0, gapFactor = 0, changeAmount, changeBase, *columnData = NULL;
  char *matchPattern = NULL;
  long matchPatternAfter = 0;
  double increaseOfAmount = -1, decreaseOfAmount = -1;
  long retval;
  int32_t dataType;
  long overlap = 0;
  unsigned long flags = 0, pipeFlags = 0, changeFlags = 0, decreaseOfFlags = 0, increaseOfFlags = 0;
  unsigned long majorOrderFlag;
  char **stringData = NULL;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scArg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  for (iArg = 1; iArg < argc; iArg++) {
    if (scArg[iArg].arg_type == OPTION) {
      switch (matchCode = match_string(scArg[iArg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scArg[iArg].n_items -= 1;
        if (scArg[iArg].n_items > 0 &&
            !scanItemList(&majorOrderFlag, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                          "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                          "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)) {
          SDDS_Bomb("invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;

      case SET_GAPIN:
        if ((scArg[iArg].n_items -= 2) < 0 ||
            !scanItemList(&flags, scArg[iArg].list + 2, &scArg[iArg].n_items, 0,
                          "amount", SDDS_DOUBLE, &gapAmount, 1, GAPIN_AMOUNT,
                          "factor", SDDS_DOUBLE, &gapFactor, 1, GAPIN_FACTOR, NULL) ||
            (flags & GAPIN_AMOUNT && gapAmount <= 0) ||
            (flags & GAPIN_FACTOR && gapFactor <= 0)) {
          fprintf(stderr, "Error: invalid -gapin syntax/values\n");
          return EXIT_FAILURE;
        }
        columnName = scArg[iArg].list[1];
        mode = matchCode;
        break;

      case SET_INCREASEOF:
        increaseOfFlags = 0;
        if (scArg[iArg].n_items < 2) {
          fprintf(stderr, "Error: invalid -increaseOf syntax\n");
          return EXIT_FAILURE;
        }
        scArg[iArg].n_items -= 2;
        if (!scanItemList(&increaseOfFlags, scArg[iArg].list + 2, &scArg[iArg].n_items, 0,
                          "amount", SDDS_DOUBLE, &increaseOfAmount, 1, INCREASEOF_AMOUNT,
                          "cumulative", -1, NULL, 0, INCREASEOF_CUMULATIVE,
                          "reset", -1, NULL, 0, INCREASEOF_RESET, NULL) ||
            ((flags & INCREASEOF_AMOUNT) && increaseOfAmount <= 0)) {
          fprintf(stderr, "Error: invalid -increaseOf syntax\n");
          return EXIT_FAILURE;
        }
        columnName = scArg[iArg].list[1];
        mode = matchCode;
        break;

      case SET_DECREASEOF:
        decreaseOfFlags = 0;
        if (scArg[iArg].n_items < 2) {
          fprintf(stderr, "Error: invalid -decreaseOf syntax\n");
          return EXIT_FAILURE;
        }
        scArg[iArg].n_items -= 2;
        if (!scanItemList(&decreaseOfFlags, scArg[iArg].list + 2, &scArg[iArg].n_items, 0,
                          "amount", SDDS_DOUBLE, &decreaseOfAmount, 1, DECREASEOF_AMOUNT,
                          "cumulative", -1, NULL, 0, DECREASEOF_CUMULATIVE,
                          "reset", -1, NULL, 0, DECREASEOF_RESET, NULL) ||
            ((flags & DECREASEOF_AMOUNT) && decreaseOfAmount <= 0)) {
          fprintf(stderr, "Error: invalid -decreaseOf syntax\n");
          return EXIT_FAILURE;
        }
        columnName = scArg[iArg].list[1];
        mode = matchCode;
        break;

      case SET_CHANGEOF:
        if ((scArg[iArg].n_items -= 2) < 0 ||
            !scanItemList(&changeFlags, scArg[iArg].list + 2, &scArg[iArg].n_items, 0,
                          "amount", SDDS_DOUBLE, &changeAmount, 1, CHANGEOF_AMOUNT,
                          "base", SDDS_DOUBLE, &changeBase, 1, CHANGEOF_BASE, NULL) ||
            (changeFlags & CHANGEOF_AMOUNT && changeAmount <= 0)) {
          fprintf(stderr, "Error: invalid -changeof syntax/values\n");
          return EXIT_FAILURE;
        }
        columnName = scArg[iArg].list[1];
        mode = matchCode;
        break;

      case SET_ROWLIMIT:
        if (scArg[iArg].n_items < 2) {
          fprintf(stderr, "Error: invalid -rowlimit syntax\n");
          return EXIT_FAILURE;
        }
        if (sscanf(scArg[iArg].list[1], "%" SCNd64, &rowLimit) != 1 || rowLimit <= 0) {
          fprintf(stderr, "Error: invalid -rowlimit syntax\n");
          return EXIT_FAILURE;
        }
        if (scArg[iArg].n_items > 2) {
          scArg[iArg].n_items -= 2;
          if (!scanItemList(&flags, scArg[iArg].list + 2, &scArg[iArg].n_items, 0,
                            "overlap", SDDS_LONG, &overlap, 1, ROWLIMIT_OVERLAP, NULL) ||
              overlap < 0) {
            fprintf(stderr, "Error: invalid overlap given in -rowlimit syntax\n");
            return EXIT_FAILURE;
          }
        }
        mode = matchCode;
        break;

      case SET_PIPE:
        if (!processPipeOption(scArg[iArg].list + 1, scArg[iArg].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "Error: invalid -pipe syntax\n");
          return EXIT_FAILURE;
        }
        break;

      case SET_MATCHTO:
        if ((scArg[iArg].n_items != 3 && scArg[iArg].n_items != 4) ||
            strlen(columnName = scArg[iArg].list[1]) == 0 ||
            strlen(matchPattern = scArg[iArg].list[2]) == 0) {
          fprintf(stderr, "Error: invalid -matchTo syntax\n");
          return EXIT_FAILURE;
        }
        if (scArg[iArg].n_items == 4) {
          if (strncmp(scArg[iArg].list[3], "after", strlen(scArg[iArg].list[3])) == 0)
            matchPatternAfter = 1;
          else {
            fprintf(stderr, "Error: invalid -matchTo syntax\n");
            return EXIT_FAILURE;
          }
        }
        mode = matchCode;
        break;

      case SET_PAGES_PER_PAGE:
        if (scArg[iArg].n_items != 2) {
          fprintf(stderr, "Error: invalid -pagesPerPage syntax\n");
          return EXIT_FAILURE;
        }
        if (sscanf(scArg[iArg].list[1], "%" SCNd64, &pagesPerPage) != 1 || pagesPerPage <= 0) {
          fprintf(stderr, "Error: invalid -pagesPerPage syntax\n");
          return EXIT_FAILURE;
        }
        mode = matchCode;
        break;
        
      default:
        fprintf(stderr, "Error: unknown switch: %s\n", scArg[iArg].list[0]);
        fprintf(stderr, "%s", USAGE);
        return EXIT_FAILURE;
      }
    } else {
      if (input == NULL)
        input = scArg[iArg].list[0];
      else if (output == NULL)
        output = scArg[iArg].list[0];
      else {
        fprintf(stderr, "Error: too many filenames given\n");
        return EXIT_FAILURE;
      }
    }
  }

  processFilenames("sddsbreak", &input, &output, pipeFlags, 0, &tmpfile_used);

  if (mode == -1) {
    fprintf(stderr, "Error: no break mode specified\n");
    return EXIT_FAILURE;
  }

  if (!SDDS_InitializeInput(&SDDSold, input) ||
      !SDDS_InitializeCopy(&SDDSnew, &SDDSold, output, "w")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  SDDSnew.layout.data_mode.no_row_counts = 0;
  if (columnMajorOrder != -1)
    SDDSnew.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSnew.layout.data_mode.column_major = SDDSold.layout.data_mode.column_major;

  if (!SDDS_WriteLayout(&SDDSnew)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (mode != SET_ROWLIMIT && mode!=SET_PAGES_PER_PAGE) {
    if (SDDS_GetColumnInformation(&SDDSold, "type", &dataType, SDDS_BY_NAME, columnName) != SDDS_LONG) {
      SDDS_SetError("Problem getting type information on given column");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
    if (mode == SET_MATCHTO) {
      if (!(dataType == SDDS_STRING)) {
        fprintf(stderr, "Error: given column does not contain string data\n");
        return EXIT_FAILURE;
      }
    } else if (!SDDS_NUMERIC_TYPE(dataType)) {
      if (!(mode == SET_CHANGEOF && !(changeFlags & CHANGEOF_AMOUNT) && !(changeFlags & CHANGEOF_BASE))) {
        fprintf(stderr, "Error: given column does not contain numeric data\n");
        return EXIT_FAILURE;
      }
    }
  }

  while ((retval = SDDS_ReadPage(&SDDSold)) > 0) {
    if ((rows = SDDS_CountRowsOfInterest(&SDDSold)) < 0) {
      SDDS_SetError("Problem getting number of rows of tabular data");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
    if (!SDDS_StartPage(&SDDSnew, rows) ||
        !SDDS_CopyParameters(&SDDSnew, &SDDSold) ||
        !SDDS_CopyArrays(&SDDSnew, &SDDSold)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
    if (rows == 0) {
      if (!SDDS_WritePage(&SDDSnew)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return EXIT_FAILURE;
      }
      continue;
    }

    switch (mode) {
    case SET_GAPIN:
      columnData = SDDS_GetColumnInDoubles(&SDDSold, columnName);
      if (!columnData) {
        SDDS_SetError("Unable to read specified column");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return EXIT_FAILURE;
      }
      if (!gapAmount && rows > 1) {
        double *gap = tmalloc(sizeof(*gap) * rows);
        for (i = 1; i < rows; i++)
          gap[i - 1] = fabs(columnData[i] - columnData[i - 1]);
        if (!compute_average(&gapAmount, gap, rows - 1)) {
          fprintf(stderr, "Error: unable to determine default gap amount--couldn't find median gap\n");
          free(gap);
          return EXIT_FAILURE;
        }
        gapAmount *= (gapFactor ? gapFactor : 2);
        free(gap);
      }
      {
        int64_t newStart = 0;
        for (i = 1; i <= rows; i++) {
          if (i != rows && fabs(columnData[i] - columnData[i - 1]) < gapAmount)
            continue;
          if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
              !SDDS_WritePage(&SDDSnew)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            free(columnData);
            return EXIT_FAILURE;
          }
          newStart = i;
        }
      }
      free(columnData);
      break;

    case SET_INCREASEOF:
      columnData = SDDS_GetColumnInDoubles(&SDDSold, columnName);
      if (!columnData) {
        SDDS_SetError("Unable to read specified column");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return EXIT_FAILURE;
      }
      {
        int64_t newStart = 0;
        if (increaseOfAmount <= 0) {
          for (i = 1; i <= rows; i++) {
            if (i != rows && columnData[i] <= columnData[i - 1])
              continue;
            if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
                !SDDS_WritePage(&SDDSnew)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              free(columnData);
              return EXIT_FAILURE;
            }
            newStart = i;
          }
        } else {
          if (increaseOfFlags & INCREASEOF_CUMULATIVE) {
            long iref = 0;
            for (i = 1; i <= rows; i++) {
              if ((increaseOfFlags & INCREASEOF_RESET) && columnData[i] < columnData[iref])
                iref = i;
              if (i != rows && (columnData[i] - columnData[iref]) < increaseOfAmount)
                continue;
              if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
                  !SDDS_WritePage(&SDDSnew)) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                free(columnData);
                return EXIT_FAILURE;
              }
              newStart = i;
              if (increaseOfFlags & INCREASEOF_CUMULATIVE)
                iref = i;
            }
          } else {
            for (i = 1; i <= rows; i++) {
              if (i != rows && (columnData[i] - columnData[i - 1]) < increaseOfAmount)
                continue;
              if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
                  !SDDS_WritePage(&SDDSnew)) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                free(columnData);
                return EXIT_FAILURE;
              }
              newStart = i;
            }
          }
        }
      }
      free(columnData);
      break;

    case SET_DECREASEOF:
      columnData = SDDS_GetColumnInDoubles(&SDDSold, columnName);
      if (!columnData) {
        SDDS_SetError("Unable to read specified column");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return EXIT_FAILURE;
      }
      {
        int64_t newStart = 0;
        if (decreaseOfAmount <= 0) {
          for (i = 1; i <= rows; i++) {
            if (i != rows && columnData[i] >= columnData[i - 1])
              continue;
            if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
                !SDDS_WritePage(&SDDSnew)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              free(columnData);
              return EXIT_FAILURE;
            }
            newStart = i;
          }
        } else {
          if (decreaseOfFlags & DECREASEOF_CUMULATIVE) {
            long iref = 0;
            for (i = 1; i <= rows; i++) {
              if ((decreaseOfFlags & DECREASEOF_RESET) && columnData[i] > columnData[iref])
                iref = i;
              if (i != rows && (columnData[iref] - columnData[i]) < decreaseOfAmount)
                continue;
              if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
                  !SDDS_WritePage(&SDDSnew)) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                free(columnData);
                return EXIT_FAILURE;
              }
              newStart = i;
              if (decreaseOfFlags & DECREASEOF_CUMULATIVE)
                iref = i;
            }
          } else {
            for (i = 1; i <= rows; i++) {
              if (i != rows && (columnData[i - 1] - columnData[i]) < decreaseOfAmount)
                continue;
              if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
                  !SDDS_WritePage(&SDDSnew)) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                free(columnData);
                return EXIT_FAILURE;
              }
              newStart = i;
            }
          }
        }
      }
      free(columnData);
      break;

    case SET_CHANGEOF:
      if (dataType != SDDS_STRING) {
        columnData = SDDS_GetColumnInDoubles(&SDDSold, columnName);
        if (!columnData) {
          SDDS_SetError("Unable to read specified column");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return EXIT_FAILURE;
        }
      } else {
        stringData = SDDS_GetColumn(&SDDSold, columnName);
        if (!stringData) {
          SDDS_SetError("Unable to read specified column");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return EXIT_FAILURE;
        }
      }
      {
        int64_t newStart = 0;
        if (dataType == SDDS_STRING || !changeAmount) {
          for (i = 1; i <= rows; i++) {
            if (i != rows &&
                ((dataType == SDDS_STRING && strcmp(stringData[i], stringData[i - 1]) == 0) ||
                 (dataType != SDDS_STRING && columnData[i] == columnData[i - 1])))
              continue;
            if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
                !SDDS_WritePage(&SDDSnew)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              if (dataType != SDDS_STRING)
                free(columnData);
              else
                SDDS_FreeStringArray(stringData, rows);
              return EXIT_FAILURE;
            }
            newStart = i;
          }
        } else {
          long region = 0, lastRegion = 0;
          if (!(changeFlags & CHANGEOF_BASE) && rows >= 1)
            changeBase = columnData[0];
          if (rows > 1)
            lastRegion = (columnData[0] - changeBase) / changeAmount;

          newStart = 0;
          for (i = 1; i <= rows; i++) {
            if (i != rows)
              region = (columnData[i] - changeBase) / changeAmount;
            if (i != rows && region == lastRegion)
              continue;
            if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
                !SDDS_WritePage(&SDDSnew)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              if (dataType != SDDS_STRING)
                free(columnData);
              else
                SDDS_FreeStringArray(stringData, rows);
              return EXIT_FAILURE;
            }
            newStart = i;
            lastRegion = region;
          }
        }
      }
      if (dataType != SDDS_STRING)
        free(columnData);
      else
        SDDS_FreeStringArray(stringData, rows);
      break;

    case SET_MATCHTO:
      stringData = SDDS_GetColumn(&SDDSold, columnName);
      if (!stringData) {
        SDDS_SetError("Unable to read specified column");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return EXIT_FAILURE;
      }
      {
        int64_t newStart = 0;
        breakNext = 0;
        for (i = 1; i <= rows; i++) {
          if (i != rows && !breakNext) {
            if (wild_match(stringData[i], matchPattern)) {
              if (matchPatternAfter) {
                breakNext = 1;
                continue;
              }
            } else {
              continue;
            }
          }
          if (!SDDS_CopyRows(&SDDSnew, &SDDSold, newStart, i - 1) ||
              !SDDS_WritePage(&SDDSnew)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            SDDS_FreeStringArray(stringData, rows);
            return EXIT_FAILURE;
          }
          breakNext = 0;
          newStart = i;
        }
      }
      SDDS_FreeStringArray(stringData, rows);
      break;

    case SET_ROWLIMIT:
      for (i = 0; i < rows; i += rowLimit - overlap) {
        if ((j = i + rowLimit - 1) >= rows)
          j = rows - 1;
        if (!SDDS_CopyRows(&SDDSnew, &SDDSold, i, j)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return EXIT_FAILURE;
        }
        if (!SDDS_WritePage(&SDDSnew)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return EXIT_FAILURE;
        }
        if (j == rows - 1)
          break;
      }
      break;

    case SET_PAGES_PER_PAGE:
      rowLimit = rows/pagesPerPage;
      for (j=0; j<pagesPerPage; j++) {
        int64_t iStart, iEnd;
        iStart = j*rowLimit;
        iEnd = j==(pagesPerPage-1)? rows-1 : (j+1)*rowLimit-1;
        if (!SDDS_CopyRows(&SDDSnew, &SDDSold, iStart, iEnd)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return EXIT_FAILURE;
        }
        if (!SDDS_WritePage(&SDDSnew)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return EXIT_FAILURE;
        }
      }
      break;

    default:
      fprintf(stderr, "Error: unknown break mode code seen---this can't happen\n");
      return EXIT_FAILURE;
    }
  }

  if (retval == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_Terminate(&SDDSold)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_Terminate(&SDDSnew)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (tmpfile_used && !replaceFileAndBackUp(input, output)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
