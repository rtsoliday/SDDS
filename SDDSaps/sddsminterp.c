/**
 * @file sddsminterp.c
 * @brief Multiplicative renormalized model interpolation utility for SDDS data sets.
 *
 * @details
 * This program performs interpolation of data using another data set as a model function.
 * It utilizes abscissa values from either a values file or the model function file to generate
 * interpolated results. Multiple options allow for customization of interpolation parameters,
 * output formats, and verbosity. The program supports SDDS files for both input and output.
 *
 * @section Usage
 * ```
 * sddsminterp <input-file> <output-file>
 *            [-pipe=[input],[output]]
 *             -columns=<independent-quantity>,<name>
 *            [-interpOrder=<order>]
 *             -model=<modelFile>,abscissa=<column>,ordinate=<column>[,interp=<order>]
 *            [-fileValues=<valuesFile>[,abscissa=<column>]]
 *            [-majorOrder=row|column]
 *            [-verbose]
 *            [-ascii]
 * ```
 *
 * @section Options
 * | Required                              | Description                                             |
 * |---------------------------------------|---------------------------------------------------------|
 * | -columns                            | Specify the columns for interpolation.                                      |
 * | -model                              | Define the model dataset with required abscissa and ordinate columns.       |
 *
 * | Optional                              | Description                                                                 |
 * |-------------------------------------|-----------------------------------------------------------------------------|
 * | -pipe                               | Use pipes for input and output.                                             |
 * | -interpOrder                        | Set the interpolation order (default is 1).                                 |
 * | -fileValues                         | Provide abscissa values for interpolation from a file.                      |
 * | -majorOrder                         | Set output data order as row or column.                                     |
 * | -verbose                            | Enable detailed output logging.                                             |
 * | -ascii                              | Produce ASCII-formatted output.                                             |
 *
 * @subsection Incompatibilities
 *   - `-fileValues` is not operational yet
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"

/* Enumeration for option types */
enum option_type {
  CLO_COLUMNS,
  CLO_ORDER,
  CLO_MODEL,
  CLO_VALUES,
  CLO_VERBOSE,
  CLO_ASCII,
  CLO_PIPE,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

char *commandline_option[N_OPTIONS] = {
  "columns",
  "order",
  "model",
  "fileValues",
  "verbose",
  "ascii",
  "pipe",
  "majorOrder",
};

static char *USAGE =
  "sddsminterp [<input-file>] [<output-file>]\n"
  "            [-pipe=[input],[output]]\n"
  "             -columns=<independent-quantity>,<name>\n"
  "            [-interpOrder=<order>]\n"
  "             -model=<modelFile>,abscissa=<column>,ordinate=<column>[,interp=<order>]\n"
  "            [-fileValues=<valuesFile>[,abscissa=<column>]]\n"
  "            [-majorOrder=row|column]\n"
  "            [-verbose]\n"
  "            [-ascii]\n"
  "\nOptions:\n"
  "  -pipe=<input>,<output>                      Use pipes for input and output.\n"
  "  -columns=<independent-quantity>,<name>      Specify the columns to interpolate.\n"
  "  -interpOrder=<order>                        Set the interpolation order (default: 1).\n"
  "  -model=<modelFile>,abscissa=<column>,\n"
  "          ordinate=<column>[,interp=<order>]  Define the model data set.\n"
  "  -fileValues=<valuesFile>[,abscissa=<column>]\n"
  "                                             Specify abscissa values for interpolation.\n"
  "  -majorOrder=row|column                      Set output data order.\n"
  "  -verbose                                    Enable verbose mode.\n"
  "  -ascii                                      Output in ASCII format.\n"
  "\nDescription:\n"
  "  Multiplicative renormalized model interpolation of a data set using another\n"
  "  data set as a model function.\n"
  "\n"
  "Program by Louis Emery, ANL (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define MOD_ABSCISSA 0x0001U
#define MOD_ORDINATE 0x0002U
#define MOD_ORDER 0x0004U
static char *modUsage =
  "-model=<file>,abscissa=<column>,ordinate=<column>[,interp=<order>]\n";

#define VAL_ABSCISSA 0x0001U
static char *valUsage =
  "-fileValues=<file>,abscissa=<column>\n";

#define DEFAULT_ORDER 1

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET modDataSet, valDataSet, dataDataSet, outDataSet;
  unsigned long modFlags, valFlags, majorOrderFlag;
  char *modAbscissaName, *modOrdinateName;
  char *valAbscissaName;
  char *dataAbscissaName, *dataOrdinateName;
  char *outAbscissaName, *outOrdinateName;
  char *modFile, *dataFile, *valFile, *outFile;
  long dataOrder;
  int32_t modOrder;
  double *modAbscissa, *modOrdinate;
  double *valAbscissa;
  double *dataAbscissa, *dataOrdinate, *dataScale, *dataOrdinateInterp;
  double *outScale, *outAbscissa, *outOrdinate;
  int64_t i, modRows, valRows, dataRows, outRows;

  long i_arg;
  /*long verbose; */
  long warning;
  long ascii;
  long returnCode;
  unsigned long pipeFlags;
  long tmpfile_used, noWarnings;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1)
    bomb(NULL, USAGE);

  /* Initialize variables */
  /*verbose=0; */
  dataFile = dataAbscissaName = dataOrdinateName = modFile = valFile = NULL;
  modOrdinate = dataOrdinate = NULL;
  outFile = NULL;
  warning = 0;
  modOrder = DEFAULT_ORDER;
  dataOrder = DEFAULT_ORDER;
  ascii = 0;
  modFlags = 0x0UL;
  valFlags = 0x0UL;
  pipeFlags = 0;
  tmpfile_used = 0;
  noWarnings = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], commandline_option, N_OPTIONS, UNIQUE_MATCH)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            !scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                          "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER,
                          NULL))
          SDDS_Bomb("Invalid -majorOrder syntax or values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_VERBOSE:
        /*verbose=1; */
        break;
      case CLO_ASCII:
        ascii = 1;
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;
      case CLO_MODEL:
        if ((s_arg[i_arg].n_items -= 2) < 0 ||
            !scanItemList(&modFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "abscissa", SDDS_STRING, &modAbscissaName, 1, MOD_ABSCISSA,
                          "ordinate", SDDS_STRING, &modOrdinateName, 1, MOD_ORDINATE,
                          "interp", SDDS_LONG, &modOrder, 1, MOD_ORDER,
                          NULL) ||
            !(modFlags & MOD_ABSCISSA) || !(modFlags & MOD_ORDINATE))
          bomb("Invalid -model syntax", modUsage);
        if (!strlen(modFile = s_arg[i_arg].list[1]))
          bomb("Invalid -model syntax", modUsage);
        break;
      case CLO_VALUES:
        if ((s_arg[i_arg].n_items -= 2) < 0 ||
            !scanItemList(&valFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "abscissa", SDDS_STRING, &valAbscissaName, 1, VAL_ABSCISSA,
                          NULL) ||
            !(valFlags & VAL_ABSCISSA))
          bomb("Invalid -fileValues syntax", valUsage);
        if (!strlen(valFile = s_arg[i_arg].list[1]))
          bomb("Invalid -fileValues syntax", valUsage);
        break;
      case CLO_ORDER:
        if (!get_long(&dataOrder, s_arg[i_arg].list[1]))
          bomb("No value provided for option -order", USAGE);
        break;
      case CLO_COLUMNS:
        /* The input file and the -order and -columns options could be combined more
           compactly to resemble the syntax of the -model option.
           However, we adopt the command line options of the
           command sddsinterp since it allows an input pipe */
        if (s_arg[i_arg].n_items != 3 ||
            !strlen(dataAbscissaName = s_arg[i_arg].list[1]) ||
            !strlen(dataOrdinateName = s_arg[i_arg].list[2]))
          SDDS_Bomb("Invalid -columns syntax");
        break;
      default:
        SDDS_Bomb("Unrecognized option provided");
      }
    } else {
      if (!dataFile)
        dataFile = s_arg[i_arg].list[0];
      else if (!outFile)
        outFile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many filenames provided");
    }
  }

  processFilenames("sddsminterp", &dataFile, &outFile, pipeFlags, noWarnings, &tmpfile_used);

  if (valFlags) {
    fprintf(stderr, "Warning: Option -fileValues is not operational yet. Using model abscissa values.\n");
  }

  if (!SDDS_InitializeInput(&modDataSet, modFile))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (SDDS_ReadPage(&modDataSet) < 0)
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (valFlags) {
    if (!SDDS_InitializeInput(&valDataSet, valFile))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (SDDS_ReadPage(&valDataSet) < 0)
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  /* Check that the selected columns exist */
  switch (SDDS_CheckColumn(&modDataSet, modAbscissaName, NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_NONEXISTENT:
    fprintf(stderr, "Error: Column '%s' does not exist in file '%s'.\n", modAbscissaName, modFile);
    exit(EXIT_FAILURE);
  }
  switch (SDDS_CheckColumn(&modDataSet, modOrdinateName, NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_NONEXISTENT:
    fprintf(stderr, "Error: Column '%s' does not exist in file '%s'.\n", modOrdinateName, modFile);
    exit(EXIT_FAILURE);
  }

  if (valFlags) {
    switch (SDDS_CheckColumn(&valDataSet, valAbscissaName, NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_NONEXISTENT:
      fprintf(stderr, "Error: Column '%s' does not exist in file '%s'.\n", valAbscissaName, valFile);
      exit(EXIT_FAILURE);
    }
  }

  modRows = SDDS_CountRowsOfInterest(&modDataSet);
  modAbscissa = (double *)SDDS_GetColumnInDoubles(&modDataSet, modAbscissaName);
  modOrdinate = (double *)SDDS_GetColumnInDoubles(&modDataSet, modOrdinateName);
  if (!modAbscissa || !modOrdinate)
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (valFlags) {
    valRows = SDDS_CountRowsOfInterest(&valDataSet);
    valAbscissa = (double *)SDDS_GetColumnInDoubles(&valDataSet, valAbscissaName);
    if (!valAbscissa)
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    outAbscissa = valAbscissa;
    outRows = valRows;
  } else {
    outAbscissa = modAbscissa;
    outRows = modRows;
  }

  if (!SDDS_InitializeInput(&dataDataSet, dataFile))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  switch (SDDS_CheckColumn(&dataDataSet, dataAbscissaName, NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_NONEXISTENT:
    fprintf(stderr, "Error: Column '%s' does not exist in file '%s'.\n", dataAbscissaName, dataFile);
    exit(EXIT_FAILURE);
  }
  switch (SDDS_CheckColumn(&dataDataSet, dataOrdinateName, NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_NONEXISTENT:
    fprintf(stderr, "Error: Column '%s' does not exist in file '%s'.\n", dataOrdinateName, dataFile);
    exit(EXIT_FAILURE);
  }

  if (!SDDS_InitializeOutput(&outDataSet, ascii ? SDDS_ASCII : SDDS_BINARY, 1,
                             "Interpolation data from model file", "Interpolated data", outFile) ||
      !SDDS_InitializeCopy(&outDataSet, &dataDataSet, outFile, "w"))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (columnMajorOrder != -1)
    outDataSet.layout.data_mode.column_major = columnMajorOrder;
  else
    outDataSet.layout.data_mode.column_major = dataDataSet.layout.data_mode.column_major;

  if (!SDDS_WriteLayout(&outDataSet))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (valFlags) {
    SDDS_CopyString(&outAbscissaName, valAbscissaName);
  } else {
    SDDS_CopyString(&outAbscissaName, modAbscissaName);
  }
  SDDS_CopyString(&outOrdinateName, dataOrdinateName);

  while (SDDS_ReadPage(&dataDataSet) > 0) {
    dataAbscissa = (double *)SDDS_GetColumnInDoubles(&dataDataSet, dataAbscissaName);
    dataOrdinate = (double *)SDDS_GetColumnInDoubles(&dataDataSet, dataOrdinateName);
    if (!dataAbscissa || !dataOrdinate)
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    dataRows = SDDS_CountRowsOfInterest(&dataDataSet);
    dataOrdinateInterp = (double *)malloc(sizeof(double) * dataRows);
    dataScale = (double *)malloc(sizeof(double) * dataRows);

    outScale = (double *)malloc(sizeof(double) * outRows);
    outOrdinate = (double *)malloc(sizeof(double) * outRows);

    /* There are normally more rows in the model file or value file than in the data file. */
    for (i = 0; i < dataRows; i++) {
      dataOrdinateInterp[i] = interp(modOrdinate, modAbscissa, modRows, dataAbscissa[i], warning, modOrder, &returnCode);
      dataScale[i] = dataOrdinate[i] / dataOrdinateInterp[i]; /* dataScale is a numerator */
    }

    for (i = 0; i < outRows; i++) {
      outScale[i] = interp(dataScale, dataAbscissa, dataRows, outAbscissa[i], warning, dataOrder, &returnCode);
      outOrdinate[i] = modOrdinate[i] * outScale[i];
    }

    if (SDDS_StartPage(&outDataSet, outRows) < 0)
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    if (!SDDS_CopyParameters(&outDataSet, &dataDataSet) ||
        !SDDS_CopyArrays(&outDataSet, &dataDataSet) ||
        !SDDS_SetColumn(&outDataSet, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, modAbscissa, outRows, outAbscissaName) ||
        !SDDS_SetColumn(&outDataSet, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, outOrdinate, outRows, outOrdinateName) ||
        SDDS_WritePage(&outDataSet) < 0)
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    free(dataAbscissa);
    free(dataOrdinate);
    free(dataOrdinateInterp);
    free(dataScale);
    free(outScale);
    free(outOrdinate);
  }

  if (!SDDS_Terminate(&modDataSet) ||
      !SDDS_Terminate(&outDataSet) ||
      !SDDS_Terminate(&dataDataSet))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (tmpfile_used && !replaceFileAndBackUp(dataFile, outFile))
    exit(EXIT_FAILURE);

  return EXIT_SUCCESS;
}
