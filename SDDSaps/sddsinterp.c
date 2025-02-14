/**
 * @file sddsinterp.c
 * @brief Performs interpolation on SDDS formatted data.
 *
 * @details
 * The program reads SDDS data files, performs interpolation based on user-specified options,
 * and writes the results back in SDDS format. It supports various interpolation methods, 
 * handles monotonicity enforcement, and manages out-of-range conditions.
 *
 * @section Usage
 * ```
 * sddsinterp [<inputfile>] [<outputfile>]
 *            [-pipe=[input][,output]]
 *             -columns=<independent-quantity>,<dependent-name>[,...]
 *            [-exclude=<name>[,...]]
 *            {
 *             -atValues=<values-list> | 
 *             -sequence=<points>[,<start>,<end>] |
 *             -equispaced=<spacing>[,<start>,<end>] | 
 *             -fillIn |
 *             -fileValues=<valuesfile>[,column=<column-name>][,parallelPages]]
 *            [-interpShort=-1|-2] 
 *            [-order=<number>]
 *            [-printout[=bare][,stdout]]
 *            [-forceMonotonic[={increasing|decreasing}]]
 *            [-belowRange={value=<value>|skip|saturate|extrapolate|wrap}[,{abort|warn}]]
 *            [-aboveRange={value=<value>|skip|saturate|extrapolate|wrap}[,{abort|warn}]]
 *            [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                                     |
 * |---------------------------------------|-------------------------------------------------------------------------------------------------|
 * | `-columns`                     | Specify the independent and dependent columns.                             |
 *
 * | Option                         | Description                                                                |
 * |--------------------------------|----------------------------------------------------------------------------|
 * | `-pipe`                        | Use pipe for input and/or output.                                          |
 * | `-exclude`                     | Exclude specified columns from processing.                                 |
 * | `-atValues`                    | Interpolate at the specified list of values.                               |
 * | `-sequence`                    | Generate a sequence of interpolation points.                               |
 * | `-equispaced`                  | Generate equispaced interpolation points.                                  |
 * | `-fillIn`                      | Automatically fill in interpolation points based on data.                  |
 * | `-fileValues`                  | Use values from a file for interpolation.                                  |
 * | `-interpShort`                 | Interpolate short columns with specified order (-1 or -2).                 |
 * | `-order`                       | Set the interpolation order (default is 1).                                |
 * | `-printout`                    | Output interpolated data in bare format and/or to stdout.                  |
 * | `-forceMonotonic`              | Enforce monotonicity in the data.                                          |
 * | `-belowRange`                  | Handle values below the interpolation range.                               |
 * | `-aboveRange`                  | Handle values above the interpolation range.                               |
 * | `-majorOrder`                  | Set the major order of data storage (row or column).                       |
 *
 * @subsection Incompatibilities
 * - One and only one these options must be specified:
 *   - `-atValues`, `-sequence`, `-equispaced`, `-fillIn`, or `-fileValues`
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
#include "SDDS.h"
#include "scan.h"
#include "SDDSutils.h"

/* Enumeration for option types */
enum option_type {
  CLO_ORDER,
  CLO_ATVALUES,
  CLO_SEQUENCE,
  CLO_COLUMNS,
  CLO_PRINTOUT,
  CLO_FILEVALUES,
  CLO_COMBINEDUPLICATES,
  CLO_BRANCH,
  CLO_BELOWRANGE,
  CLO_ABOVERANGE,
  CLO_PIPE,
  CLO_EXCLUDE,
  CLO_FORCEMONOTONIC,
  CLO_FILLIN,
  CLO_EQUISPACED,
  CLO_INTERP_SHORT,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "order",
  "atvalues",
  "sequence",
  "columns",
  "printout",
  "filevalues",
  "combineduplicates",
  "branch",
  "belowrange",
  "aboverange",
  "pipe",
  "exclude",
  "forcemonotonic",
  "fillin",
  "equispaced",
  "interpShort",
  "majorOrder",
};

static char *USAGE =
    "sddsinterp [<inputfile>] [<outputfile>]\n"
    "           [-pipe=[input][,output]]\n"
    "            -columns=<independent-quantity>,<dependent-name>[,...]\n"
    "           [-exclude=<name>[,...]]\n"
    "           {\n"
    "            -atValues=<values-list> | \n"
    "            -sequence=<points>[,<start>,<end>] |\n"
    "            -equispaced=<spacing>[,<start>,<end>] | \n"
    "            -fillIn |\n"
    "            -fileValues=<valuesfile>[,column=<column-name>][,parallelPages]]\n"
    "           [-interpShort=-1|-2] \n"
    "           [-order=<number>]\n"
    "           [-printout[=bare][,stdout]]\n"
    "           [-forceMonotonic[={increasing|decreasing}]]\n"
    "           [-belowRange={value=<value>|skip|saturate|extrapolate|wrap}[,{abort|warn}]]\n"
    "           [-aboveRange={value=<value>|skip|saturate|extrapolate|wrap}[,{abort|warn}]]\n"
    "           [-majorOrder=row|column]\n"
    "  Options:\n"
    "    -pipe=[input][,output]                         Use pipe for input and/or output.\n"
    "    -columns=<independent>,<dependent1>[,...]      Specify the independent and dependent columns.\n"
    "    -exclude=<name>[,...]                          Exclude specified columns from processing.\n"
    "    -atValues=<values-list>                        Interpolate at the specified list of values.\n"
    "    -sequence=<points>[,<start>,<end>]             Generate a sequence of interpolation points.\n"
    "    -equispaced=<spacing>[,<start>,<end>]          Generate equispaced interpolation points.\n"
    "    -fillIn                                        Automatically fill in interpolation points based on data.\n"
    "    -fileValues=<valuesfile>[,column=<name>][,parallelPages]\n"
    "                                                   Use values from a file for interpolation.\n"
    "    -interpShort=-1|-2                             Interpolate short columns with order -1 or -2.\n"
    "                                                   Order -1 inherits value from the previous point;\n"
    "                                                   Order -2 inherits value from the next point.\n"
    "    -order=<number>                                Set the interpolation order (default is 1).\n"
    "    -printout[=bare][,stdout]                      Output interpolated data in bare format and/or to stdout.\n"
    "    -forceMonotonic[={increasing|decreasing}]      Enforce monotonicity in the data.\n"
    "    -belowRange={value=<v>|skip|saturate|extrapolate|wrap}[,{abort|warn}]\n"
    "                                                   Handle values below the interpolation range.\n"
    "    -aboveRange={value=<v>|skip|saturate|extrapolate|wrap}[,{abort|warn}]\n"
    "                                                   Handle values above the interpolation range.\n"
    "    -majorOrder=row|column                         Set the major order of data storage.\n\n"
    "  Example:\n"
    "    sddsinterp input.sdds output.sdds -columns=energy,flux -atValues=1.0,2.0,3.0\n\n"
    "  Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

double *makeSequence(int64_t *points, double start, double end, double spacing, double *data, int64_t dataRows);
double *makeFillInSequence(double *x, int64_t dataRows, int64_t *nPointsReturn);
long checkMonotonicity(double *indepValue, int64_t rows);
int64_t forceMonotonicity(double *indepValue, double **y, long columns, int64_t rows, unsigned long direction);
int64_t combineDuplicatePoints(double *x, double **y, long columns, int64_t rows, double tolerance);

#define FILEVALUES_PARALLEL_PAGES 0x0001U

#define NORMAL_PRINTOUT 1
#define BARE_PRINTOUT 2
#define STDOUT_PRINTOUT 4

#define FORCE_MONOTONIC 0x0001U
#define FORCE_INCREASING 0x0002U
#define FORCE_DECREASING 0x0004U

int main(int argc, char **argv) {
  int iArg;
  char *indepQuantity, **depenQuantity, *fileValuesQuantity, *fileValuesFile, **exclude;
  long depenQuantities, monotonicity, excludes;
  char *input, *output;
  long i, readCode, order, valuesReadCode, fillIn;
  int64_t rows, row, interpPoints, j;
  long sequencePoints, combineDuplicates, branch;
  int32_t *rowFlag;
  double sequenceStart, sequenceEnd;
  double sequenceSpacing;
  unsigned long flags, interpCode, printFlags, forceMonotonic;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout, SDDSvalues;
  OUTRANGE_CONTROL aboveRange, belowRange;
  double *atValue;
  long atValues, doNotRead, parallelPages;
  double *indepValue, **depenValue, *interpPoint, **outputData;
  unsigned long pipeFlags, majorOrderFlag;
  FILE *fpPrint;
  short interpShort = 0, interpShortOrder = -1, *shortValue = NULL, columnMajorOrder = -1;
  long nextPos;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3 || argc > (3 + N_OPTIONS))
    bomb(NULL, USAGE);

  atValue = NULL;
  atValues = fillIn = 0;
  output = input = NULL;
  combineDuplicates = branch = sequencePoints = parallelPages = 0;
  indepQuantity = NULL;
  depenQuantity = exclude = NULL;
  depenQuantities = excludes = 0;
  aboveRange.flags = belowRange.flags = OUTRANGE_SATURATE;
  order = 1;
  readCode = interpPoints = 0;
  fileValuesFile = fileValuesQuantity = NULL;
  sequenceStart = sequenceEnd = sequenceSpacing = 0;
  printFlags = pipeFlags = 0;
  forceMonotonic = 0;
  indepValue = interpPoint = NULL;
  depenValue = outputData = NULL;

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
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_ORDER:
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%ld", &order) != 1 || order < 0)
          SDDS_Bomb("invalid -order syntax/value");
        break;
      case CLO_ATVALUES:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -atValues syntax");
        if (atValue)
          SDDS_Bomb("give -atValues only once");
        atValue = tmalloc(sizeof(*atValue) * (atValues = scanned[iArg].n_items - 1));
        for (i = 0; i < atValues; i++)
          if (sscanf(scanned[iArg].list[i + 1], "%lf", &atValue[i]) != 1)
            SDDS_Bomb("invalid -atValues value");
        break;
      case CLO_INTERP_SHORT:
        if (scanned[iArg].n_items == 2) {
          if (sscanf(scanned[iArg].list[1], "%hd", &interpShortOrder) != 1 ||
              (interpShortOrder != -1 && interpShortOrder != -2))
            SDDS_Bomb("invalid -interpShort value; must be -1 or -2");
        }
        interpShort = 1;
        break;
      case CLO_SEQUENCE:
        if ((scanned[iArg].n_items != 2 && scanned[iArg].n_items != 4) ||
            sscanf(scanned[iArg].list[1], "%ld", &sequencePoints) != 1 ||
            sequencePoints < 2)
          SDDS_Bomb("invalid -sequence syntax/value");
        if (scanned[iArg].n_items == 4 &&
            (sscanf(scanned[iArg].list[2], "%lf", &sequenceStart) != 1 ||
             sscanf(scanned[iArg].list[3], "%lf", &sequenceEnd) != 1))
          SDDS_Bomb("invalid -sequence syntax/value");
        if (sequenceSpacing)
          SDDS_Bomb("give only one of -sequence and -equispaced");
        break;
      case CLO_EQUISPACED:
        if ((scanned[iArg].n_items != 2 && scanned[iArg].n_items != 4) ||
            sscanf(scanned[iArg].list[1], "%lf", &sequenceSpacing) != 1 ||
            sequenceSpacing <= 0)
          SDDS_Bomb("invalid -equispaced syntax/value");
        if (scanned[iArg].n_items == 4 &&
            (sscanf(scanned[iArg].list[2], "%lf", &sequenceStart) != 1 ||
             sscanf(scanned[iArg].list[3], "%lf", &sequenceEnd) != 1))
          SDDS_Bomb("invalid -equispaced syntax/values");
        if (sequencePoints)
          SDDS_Bomb("give only one of -sequence and -equispaced");
        break;
      case CLO_COLUMNS:
        if (indepQuantity)
          SDDS_Bomb("only one -columns option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        indepQuantity = scanned[iArg].list[1];
        if (scanned[iArg].n_items >= 2) {
          depenQuantity = tmalloc(sizeof(*depenQuantity) * (depenQuantities = scanned[iArg].n_items - 2));
          for (i = 0; i < depenQuantities; i++)
            depenQuantity[i] = scanned[iArg].list[i + 2];
        }
        break;
      case CLO_PRINTOUT:
        if ((scanned[iArg].n_items -= 1) >= 1) {
          if (!scanItemList(&printFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                            "bare", -1, NULL, 0, BARE_PRINTOUT,
                            "stdout", -1, NULL, 0, STDOUT_PRINTOUT, NULL))
            SDDS_Bomb("invalid -printout syntax");
        }
        if (!(printFlags & BARE_PRINTOUT))
          printFlags |= NORMAL_PRINTOUT;
        break;
      case CLO_FILEVALUES:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -fileValues syntax");
        fileValuesFile = scanned[iArg].list[1];
        scanned[iArg].n_items -= 2;
        if (!scanItemList(&flags, scanned[iArg].list + 2, &scanned[iArg].n_items, 0,
                          "column", SDDS_STRING, &fileValuesQuantity, 1, 0,
                          "parallelpages", -1, NULL, 0, FILEVALUES_PARALLEL_PAGES, NULL))
          SDDS_Bomb("invalid -fileValues syntax");
        if (flags & FILEVALUES_PARALLEL_PAGES)
          parallelPages = 1;
        break;
      case CLO_COMBINEDUPLICATES:
        SDDS_Bomb("-combineDuplicates option not implemented yet--send email to borland@aps.anl.gov");
        combineDuplicates = 1;
        break;
      case CLO_BRANCH:
        SDDS_Bomb("-branch option not implemented yet--send email to borland@aps.anl.gov");
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%ld", &branch) != 1 || branch < 1)
          SDDS_Bomb("invalid -branch syntax/value");
        break;
      case CLO_BELOWRANGE:
        if ((scanned[iArg].n_items -= 1) < 1 ||
            !scanItemList(&belowRange.flags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "value", SDDS_DOUBLE, &belowRange.value, 1, OUTRANGE_VALUE,
                          "skip", -1, NULL, 0, OUTRANGE_SKIP,
                          "saturate", -1, NULL, 0, OUTRANGE_SATURATE,
                          "extrapolate", -1, NULL, 0, OUTRANGE_EXTRAPOLATE,
                          "wrap", -1, NULL, 0, OUTRANGE_WRAP,
                          "abort", -1, NULL, 0, OUTRANGE_ABORT,
                          "warn", -1, NULL, 0, OUTRANGE_WARN, NULL))
          SDDS_Bomb("invalid -belowRange syntax/value");
        if ((i = bitsSet(belowRange.flags & (OUTRANGE_VALUE | OUTRANGE_SKIP | OUTRANGE_SATURATE | OUTRANGE_EXTRAPOLATE | OUTRANGE_WRAP | OUTRANGE_ABORT))) > 1)
          SDDS_Bomb("incompatible keywords given for -belowRange");
        if (i != 1)
          belowRange.flags |= OUTRANGE_SATURATE;
        break;
      case CLO_ABOVERANGE:
        if ((scanned[iArg].n_items -= 1) < 1 ||
            !scanItemList(&aboveRange.flags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "value", SDDS_DOUBLE, &aboveRange.value, 1, OUTRANGE_VALUE,
                          "skip", -1, NULL, 0, OUTRANGE_SKIP,
                          "saturate", -1, NULL, 0, OUTRANGE_SATURATE,
                          "extrapolate", -1, NULL, 0, OUTRANGE_EXTRAPOLATE,
                          "wrap", -1, NULL, 0, OUTRANGE_WRAP,
                          "abort", -1, NULL, 0, OUTRANGE_ABORT,
                          "warn", -1, NULL, 0, OUTRANGE_WARN, NULL))
          SDDS_Bomb("invalid -aboveRange syntax/value");
        if ((i = bitsSet(aboveRange.flags & (OUTRANGE_VALUE | OUTRANGE_SKIP | OUTRANGE_SATURATE | OUTRANGE_EXTRAPOLATE | OUTRANGE_WRAP | OUTRANGE_ABORT))) > 1)
          SDDS_Bomb("incompatible keywords given for -aboveRange");
        if (i != 1)
          aboveRange.flags |= OUTRANGE_SATURATE;
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_EXCLUDE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -exclude syntax");
        moveToStringArray(&exclude, &excludes, scanned[iArg].list + 1, scanned[iArg].n_items - 1);
        break;
      case CLO_FORCEMONOTONIC:
        if ((scanned[iArg].n_items -= 1) > 0) {
          if (!scanItemList(&forceMonotonic, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                            "increasing", -1, NULL, 0, FORCE_INCREASING,
                            "decreasing", -1, NULL, 0, FORCE_DECREASING, NULL) ||
              bitsSet(forceMonotonic) != 1)
            SDDS_Bomb("invalid -forceMonotonic syntax/value");
        } else
          forceMonotonic = FORCE_MONOTONIC;
        break;
      case CLO_FILLIN:
        fillIn = 1;
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

  processFilenames("sddsinterp", &input, &output, pipeFlags, 0, NULL);

  fpPrint = stderr;
  if (printFlags & STDOUT_PRINTOUT)
    fpPrint = stdout;

  if (!indepQuantity)
    SDDS_Bomb("supply the independent quantity name with the -columns option");

  if ((atValues ? 1 : 0) + (fileValuesFile ? 1 : 0) + (sequencePoints ? 1 : 0) + fillIn + (sequenceSpacing > 0 ? 1 : 0) != 1)
    SDDS_Bomb("you must give one and only one of -atValues, -fileValues, -sequence, -equispaced, and -fillIn");

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  excludes = appendToStringArray(&exclude, excludes, indepQuantity);
  if (!depenQuantities)
    depenQuantities = appendToStringArray(&depenQuantity, depenQuantities, "*");

  if ((depenQuantities = expandColumnPairNames(&SDDSin, &depenQuantity, NULL, depenQuantities, exclude, excludes, FIND_NUMERIC_TYPE, 0)) <= 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_Bomb("no dependent quantities selected for interpolation");
  }

  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 0, NULL, "sddsinterp output", output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, indepQuantity, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;
  if (fileValuesFile && !SDDS_InitializeInput(&SDDSvalues, fileValuesFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_DefineParameter(&SDDSout, "InterpDataPage", NULL, NULL,
                           "Page of interpolation data file used to create this page", NULL, SDDS_LONG, NULL) < 0 ||
      SDDS_DefineParameter(&SDDSout, "InterpPointsPage", NULL, NULL, "Page of interpolation points file used to create this page", NULL, SDDS_LONG, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < depenQuantities; i++)
    if (!SDDS_TransferColumnDefinition(&SDDSout, &SDDSin, depenQuantity[i], NULL)) {
      fprintf(stderr, "problem creating interpolated-output column %s\n", depenQuantity[i]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  if (!SDDS_TransferAllParameterDefinitions(&SDDSout, &SDDSin, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  doNotRead = 0;
  interpPoint = NULL;
  outputData = tmalloc(sizeof(*outputData) * (depenQuantities));
  depenValue = tmalloc(sizeof(*depenValue) * (depenQuantities));
  rowFlag = NULL;
  valuesReadCode = 0;

  while (doNotRead || (readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    rows = SDDS_CountRowsOfInterest(&SDDSin);
    if (!(indepValue = SDDS_GetColumnInDoubles(&SDDSin, indepQuantity)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (atValues) {
      interpPoint = atValue;
      interpPoints = atValues;
    } else if (fileValuesFile) {
      if (interpPoint)
        free(interpPoint);
      if ((valuesReadCode = SDDS_ReadPage(&SDDSvalues)) == 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      else if (valuesReadCode == -1) {
        if (parallelPages) {
          fprintf(stderr, "warning: file %s ends before file %s\n", fileValuesFile, input);
          break;
        } else {
          /* "rewind" the values file */
          if (!SDDS_Terminate(&SDDSvalues) ||
              !SDDS_InitializeInput(&SDDSvalues, fileValuesFile))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          if ((valuesReadCode = SDDS_ReadPage(&SDDSvalues)) < 1) {
            fprintf(stderr, "error: unable to (re)read file %s\n", fileValuesFile);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
          /* read the next page of the interpolation data file */
          if ((readCode = SDDS_ReadPage(&SDDSin)) < 1) {
            if (readCode == -1)
              break;
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
          rows = SDDS_CountRowsOfInterest(&SDDSin);
          if (indepValue)
            free(indepValue);
          if (!(indepValue = SDDS_GetColumnInDoubles(&SDDSin, indepQuantity)))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }

      if (!parallelPages)
        doNotRead = 1;
      interpPoints = SDDS_CountRowsOfInterest(&SDDSvalues);
      interpPoint = SDDS_GetColumnInDoubles(&SDDSvalues, fileValuesQuantity);
      if (SDDS_NumberOfErrors())
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (sequencePoints || sequenceSpacing) {
      if (interpPoint)
        free(interpPoint);
      interpPoints = sequencePoints;
      if (!(interpPoint = makeSequence(&interpPoints, sequenceStart, sequenceEnd, sequenceSpacing, indepValue, rows)))
        exit(EXIT_FAILURE);
    } else {
      /* fillIn interpolation */
      if (interpPoint)
        free(interpPoint);
      if (!(interpPoint = makeFillInSequence(indepValue, rows, &interpPoints)))
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < depenQuantities; i++)
      outputData[i] = tmalloc(sizeof(*outputData[i]) * interpPoints);
    rowFlag = trealloc(rowFlag, sizeof(*rowFlag) * interpPoints);
    for (j = 0; j < interpPoints; j++)
      rowFlag[j] = 1;
    for (i = 0; i < depenQuantities; i++) {
      if (!(depenValue[i] = SDDS_GetColumnInDoubles(&SDDSin, depenQuantity[i])))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (forceMonotonic)
      rows = forceMonotonicity(indepValue, depenValue, depenQuantities, rows, forceMonotonic);
    else if (combineDuplicates)
      rows = combineDuplicatePoints(indepValue, depenValue, depenQuantities, rows, 0.0);
    if ((monotonicity = checkMonotonicity(indepValue, rows)) == 0)
      SDDS_Bomb("independent data values do not change monotonically or repeated independent values exist");
    if (interpShort)
      shortValue = tmalloc(sizeof(*shortValue) * rows);

    for (i = 0; i < depenQuantities; i++) {
      if (interpShort) {
        for (row = 0; row < rows; row++) {
          shortValue[row] = (short)depenValue[i][row];
        }
      }
      for (j = 0; j < interpPoints; j++) {
        if (!interpShort) {
          outputData[i][j] = interpolate(depenValue[i], indepValue, rows, interpPoint[j], &belowRange, &aboveRange, order, &interpCode, monotonicity);
        } else {
          outputData[i][j] = (double)interp_short(shortValue, indepValue, rows, interpPoint[j], 0, interpShortOrder, &interpCode, &nextPos);
        }
        if (interpCode) {
          if (interpCode & OUTRANGE_ABORT) {
            fprintf(stderr, "error: value %e is out of range for column %s\n", interpPoint[j], depenQuantity[i]);
            exit(EXIT_FAILURE);
          }
          if (interpCode & OUTRANGE_WARN)
            fprintf(stderr, "warning: value %e is out of range for column %s\n", interpPoint[j], depenQuantity[i]);
          if (interpCode & OUTRANGE_SKIP)
            rowFlag[j] = 0;
        }
      }
    }
    if (interpShort)
      free(shortValue);
    if (!SDDS_StartPage(&SDDSout, interpPoints) ||
        !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, interpPoint, interpPoints, indepQuantity))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_SetParameters(&SDDSout, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, "InterpDataPage", readCode, "InterpPointsPage", valuesReadCode, NULL) ||
        !SDDS_CopyParameters(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < depenQuantities; i++)
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, outputData[i], interpPoints, depenQuantity[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_AssertRowFlags(&SDDSout, SDDS_FLAG_ARRAY, rowFlag, rows) || !SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (printFlags & BARE_PRINTOUT) {
      for (j = 0; j < interpPoints; j++)
        if (rowFlag[j]) {
          fprintf(fpPrint, "%21.15e ", interpPoint[j]);
          for (i = 0; i < depenQuantities; i++)
            fprintf(fpPrint, "%21.15e ", outputData[i][j]);
          fputc('\n', fpPrint);
        }
    } else if (printFlags & NORMAL_PRINTOUT) {
      for (j = 0; j < interpPoints; j++)
        if (rowFlag[j]) {
          fprintf(fpPrint, "%s=%21.15e ", indepQuantity, interpPoint[j]);
          for (i = 0; i < depenQuantities; i++)
            fprintf(fpPrint, "%s=%21.15e ", depenQuantity[i], outputData[i][j]);
          fputc('\n', fpPrint);
        }
    }
    if (indepValue)
      free(indepValue);
    indepValue = NULL;
    for (i = 0; i < depenQuantities; i++) {
      if (outputData[i])
        free(outputData[i]);
      outputData[i] = NULL;
      if (depenValue[i])
        free(depenValue[i]);
      depenValue[i] = NULL;
    }
    if (fileValuesFile) {
      if (interpPoint)
        free(interpPoint);
      interpPoint = NULL;
    }
    if (rowFlag)
      free(rowFlag);
    rowFlag = NULL;
  }

  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (fileValuesFile) {
    if (!SDDS_Terminate(&SDDSvalues)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  return EXIT_SUCCESS;
}

double *makeFillInSequence(double *x, int64_t dataRows, int64_t *nPointsReturn) {
  int64_t i;
  double dxMin, dx, xMin, xMax;

  dxMin = DBL_MAX;
  xMin = xMax = x[0];
  for (i = 1; i < dataRows; i++) {
    if ((dx = fabs(x[i] - x[i - 1])) < dxMin && dx > 0)
      dxMin = dx;
    if (x[i] < xMin)
      xMin = x[i];
    if (x[i] > xMax)
      xMax = x[i];
  }
  *nPointsReturn = (int64_t)((xMax - xMin) / dxMin + 1);
  return makeSequence(nPointsReturn, xMin, xMax, 0.0, x, dataRows);
}

double *makeSequence(int64_t *points, double start, double end, double spacing, double *data, int64_t dataRows) {
  int64_t i;
  double delta, *sequence;
  if (start == end) {
    if (!find_min_max(&start, &end, data, dataRows))
      return NULL;
  }
  if (*points > 1) {
    delta = (end - start) / (*points - 1);
  } else if (spacing >= 0) {
    delta = spacing;
    if (delta == 0)
      return NULL;
    *points = (int64_t)((end - start) / delta + 1.5);
  } else {
    *points = 1;
    delta = 0;
  }
  sequence = tmalloc(sizeof(*sequence) * (*points));
  for (i = 0; i < *points; i++)
    sequence[i] = start + delta * i;
  return sequence;
}

long checkMonotonicity(double *indepValue, int64_t rows) {
  if (rows == 1)
    return 1;
  if (indepValue[rows - 1] > indepValue[0]) {
    while (--rows > 0)
      if (indepValue[rows] <= indepValue[rows - 1])
        return 0;
    return 1;
  } else {
    while (--rows > 0)
      if (indepValue[rows] >= indepValue[rows - 1])
        return 0;
    return -1;
  }
}

int64_t combineDuplicatePoints(double *x, double **y, long columns, int64_t rows, double tolerance) {
  double xmin, xmax;
  long column;
  int64_t i, j;

  find_min_max(&xmin, &xmax, x, rows);
  if (xmin == xmax)
    SDDS_Bomb("interpolation data is invalid--no range in independent variable");
  tolerance *= xmax - xmin;
  for (i = 1; i < rows; i++) {
    if (fabs(x[i] - x[i - 1]) <= tolerance) {
      x[i - 1] = (x[i] + x[i - 1]) / 2;
      for (column = 0; column < columns; column++)
        y[column][i - 1] = (y[column][i] + y[column][i - 1]) / 2;
      for (j = i + 1; j < rows; j++) {
        x[j - 1] = x[j];
        for (column = 0; column < columns; column++)
          y[column][j - 1] = y[column][j];
      }
      rows--;
      i--;
    }
  }
  return rows;
}

int64_t forceMonotonicity(double *x, double **y, long columns, int64_t rows, unsigned long mode) {
  int64_t i, j;
  long column, direction;
  char *keep;
  double reference;

  if (rows < 2)
    return rows;

  if (mode & FORCE_INCREASING)
    direction = 1;
  else if (mode & FORCE_DECREASING)
    direction = -1;
  else
    direction = x[1] > x[0] ? 1 : -1;

  keep = tmalloc(sizeof(*keep) * rows);
  reference = x[0];
  keep[0] = 1;  // Always keep the first point
  for (i = 1; i < rows; i++) {
    if (direction * (x[i] - reference) > 0) {
      reference = x[i];
      keep[i] = 1;
    } else {
      keep[i] = 0;
    }
  }
  for (i = j = 1; i < rows; i++) {
    if (keep[i]) {
      if (i != j) {
        for (column = 0; column < columns; column++)
          y[column][j] = y[column][i];
        x[j] = x[i];
      }
      j++;
    }
  }
  rows = j;
  free(keep);
  return rows;
}
