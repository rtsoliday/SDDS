/**
 * @file sddsinteg.c
 * @brief Performs numerical integration on specified columns of an SDDS dataset.
 *
 * @details
 * This program integrates specified columns of an SDDS dataset using customizable methods.
 * It supports error propagation, template customization, and column-based exclusions.
 *
 * @section Usage
 * ```
 * sddsinteg [<inputfile>] [<outputfile>]
 *           [-pipe[=input][,output]]
 *            -integrate=<column-name>[,<sigma-name>] ...
 *           [-exclude=<column-name>[,...]]
 *            -versus=<column-name>[,<sigma-name>]
 *           [-mainTemplates=<item>=<string>[,...]]
 *           [-errorTemplates=<item>=<string>[,...]]
 *           [-copycolumns=<list_of_column_names>]
 *           [-method={trapazoid|GillMiller}]
 *           [-printFinal[=bare][,stdout][,format=<string>]]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                                     |
 * |---------------------------------------|-------------------------------------------------------------------------------------------------|
 * | `-integrate`                          | Column(s) to integrate. Optional sigma names for RMS error may be provided.                    |
 * | `-versus`                             | Column to integrate against. Optionally specify sigma for RMS error.                           |
 *
 * | Optional                              | Description                                                                                     |
 * |---------------------------------------|-------------------------------------------------------------------------------------------------|
 * | `-exclude`                            | Columns to exclude from integration.                                                           |
 * | `-mainTemplates`                      | Customizes main templates for name, symbol, or description of integration columns.             |
 * | `-errorTemplates`                     | Customizes error templates for name, symbol, or description of error columns.                  |
 * | `-copycolumns`                        | List of columns to copy to the output.                                                         |
 * | `-method`                             | Integration method. `trapazoid` supports error propagation; `GillMiller` does not.             |
 * | `-printFinal`                         | Outputs the final integral value with formatting options.                                      |
 *
 * @subsection Incompatibilities
 *   - `-method=GillMiller` disables error propagation for all integrated columns.
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
#include "SDDS.h"
#include "SDDSutils.h"
#include "scan.h"
#include <ctype.h>

static char *USAGE =
  "Usage: sddsinteg [<input>] [<output>]\n"
  "                [-pipe=[input][,output]]\n"
  "                 -integrate=<column-name>[,<sigma-name>] ...\n"
  "                [-exclude=<column-name>[,...]]\n"
  "                 -versus=<column-name>[,<sigma-name>]\n"
  "                [-mainTemplates=<item>=<string>[,...]]\n"
  "                [-errorTemplates=<item>=<string>[,...]]\n"
  "                [-copycolumns=<list of column names>]\n"
  "                [-method={trapazoid|GillMiller}]\n"
  "                [-printFinal[=bare][,stdout][,format=<string>]]\n\n"
  "Options:\n"
  "  -pipe           Standard SDDS pipe option.\n"
  "  -integrate      Name of column to integrate, plus optional RMS error.\n"
  "                  Column name may include wildcards, with error name using %%s.\n"
  "  -exclude        List of column names to exclude from integration.\n"
  "  -versus         Name of column to integrate against, plus optional RMS error.\n"
  "  -mainTemplates  Customize main templates for name, symbol, or description.\n"
  "  -errorTemplates Customize error templates for name, symbol, or description.\n"
  "  -copycolumns    Comma-separated list of columns to copy to the output.\n"
  "  -method         Integration method: trapazoid (default) or GillMiller.\n"
  "  -printFinal     Print the final integral value. Options:\n"
  "                     bare      - Print only the integral value.\n"
  "                     stdout    - Print to standard output.\n"
  "                     format=<s> - Specify printf format string.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")";

/* Enumeration for option types */
enum option_type {
  CLO_INTEGRATE,
  CLO_VERSUS,
  CLO_METHOD,
  CLO_PRINTFINAL,
  CLO_MAINTEMPLATE,
  CLO_ERRORTEMPLATE,
  CLO_PIPE,
  CLO_EXCLUDE,
  CLO_MAJOR_ORDER,
  CLO_COPY,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "integrate",
  "versus",
  "method",
  "printfinal",
  "maintemplate",
  "errortemplate",
  "pipe",
  "exclude",
  "majorOrder",
  "copycolumns",
};

void trapizoid(double *x, double *y, double *sx, double *sy, int64_t n, double *integ, double *error);
long checkErrorNames(char **yErrorName, long nIntegrals);
void makeSubstitutions(char *buffer1, char *buffer2, char *template, char *nameRoot, char *symbolRoot, char *xName, char *xSymbol);
char *changeInformation(SDDS_DATASET *SDDSout, char *name, char *nameRoot, char *symbolRoot,
                        char *xName, char *xSymbol, char **template, char *newUnits);
long setupOutputFile(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *output,
                     char ***yOutputName, char ***yOutputErrorName, char ***yOutputUnits,
                     char *xName, char *xErrorName, char **yName, char **yErrorName,
                     long yNames, char **mainTemplate, char **errorTemplate, long methodCode,
                     short columnMajorOrder, char **colMatch, int32_t colMatches);

#define NORMAL_PRINTOUT 1
#define BARE_PRINTOUT 2
#define STDOUT_PRINTOUT 4

#define TRAPAZOID_METHOD 0
#define GILLMILLER_METHOD 1
#define N_METHODS 2

static char *methodOption[N_METHODS] = {
  "trapazoid",
  "gillmiller"
};

int main(int argc, char **argv) {
  double *xData = NULL, *yData = NULL, *xError = NULL, *yError = NULL, *integral = NULL, *integralError = NULL;
  char *input = NULL, *output = NULL, *xName = NULL, *xErrorName = NULL;
  char **yName = NULL, **yErrorName = NULL, **yOutputName = NULL, **yOutputErrorName = NULL, *ptr = NULL, **colMatch = NULL;
  char **yOutputUnits = NULL, **yExcludeName = NULL;
  char *mainTemplate[3] = {"%yNameInteg", "Integral w.r.t. %xSymbol of %ySymbol", "$sI$e %ySymbol d%xSymbol"};
  char *errorTemplate[3] = {"%yNameIntegSigma", "Sigma of integral w.r.t. %xSymbol of %ySymbol",
    "Sigma[$sI$e %ySymbol d%xSymbol]"};
  char *GMErrorTemplate[3] = {"%yNameIntegError", "Error estimate for integral w.r.t. %xSymbol of %ySymbol",
    "Error[$sI$e %ySymbol d%xSymbol]"};
  long i, iArg, yNames = 0, yExcludeNames = 0;
  int32_t colMatches = 0;
  int64_t rows;
  SDDS_DATASET SDDSin, SDDSout;
  SCANNED_ARG *scanned = NULL;
  unsigned long flags = 0, pipeFlags = 0, printFlags = 0, majorOrderFlag = 0;
  FILE *fpPrint = stderr;
  char *printFormat = "%21.15e";
  int methodCode = TRAPAZOID_METHOD;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scanned, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE);
    return EXIT_FAILURE;
  }

  colMatch = NULL;
  colMatches = 0;
  input = output = xName = xErrorName = NULL;
  yName = yErrorName = yExcludeName = NULL;
  integral = integralError = yError = yData = xData = xError = NULL;
  yNames = yExcludeNames = 0;
  pipeFlags = printFlags = 0;
  fpPrint = stderr;
  printFormat = "%21.15e";
  methodCode = TRAPAZOID_METHOD;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 &&
            !scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                          "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)) {
          SDDS_Bomb("invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_INTEGRATE:
        if (scanned[iArg].n_items != 2 && scanned[iArg].n_items != 3)
          SDDS_Bomb("invalid -integrate syntax");
        yName = SDDS_Realloc(yName, sizeof(*yName) * (yNames + 1));
        yErrorName = SDDS_Realloc(yErrorName, sizeof(*yErrorName) * (yNames + 1));
        yName[yNames] = scanned[iArg].list[1];
        if (scanned[iArg].n_items == 3)
          yErrorName[yNames] = scanned[iArg].list[2];
        else
          yErrorName[yNames] = NULL;
        yNames++;
        break;
      case CLO_EXCLUDE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -exclude syntax");
        moveToStringArray(&yExcludeName, &yExcludeNames, scanned[iArg].list + 1, scanned[iArg].n_items - 1);
        break;
      case CLO_VERSUS:
        if (xName)
          SDDS_Bomb("give -versus only once");
        if (scanned[iArg].n_items != 2 && scanned[iArg].n_items != 3)
          SDDS_Bomb("invalid -versus syntax");
        xName = scanned[iArg].list[1];
        if (scanned[iArg].n_items == 3)
          xErrorName = scanned[iArg].list[2];
        else
          xErrorName = NULL;
        break;
      case CLO_METHOD:
        if (scanned[iArg].n_items != 2 ||
            (methodCode = match_string(scanned[iArg].list[1], methodOption, N_METHODS, 0)) < 0)
          SDDS_Bomb("invalid -method syntax");
        break;
      case CLO_PRINTFINAL:
        if ((scanned[iArg].n_items -= 1) >= 1) {
          if (!scanItemList(&printFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                            "bare", -1, NULL, 0, BARE_PRINTOUT,
                            "stdout", -1, NULL, 0, STDOUT_PRINTOUT,
                            "format", SDDS_STRING, &printFormat, 1, 0, NULL)) {
            SDDS_Bomb("invalid -printFinal syntax");
          }
        }
        if (!(printFlags & BARE_PRINTOUT))
          printFlags |= NORMAL_PRINTOUT;
        break;
      case CLO_MAINTEMPLATE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -mainTemplate syntax");
        scanned[iArg].n_items--;
        if (!scanItemList(&flags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "name", SDDS_STRING, mainTemplate + 0, 1, 0,
                          "description", SDDS_STRING, mainTemplate + 1, 1, 0,
                          "symbol", SDDS_STRING, mainTemplate + 2, 1, 0, NULL)) {
          SDDS_Bomb("invalid -mainTemplate syntax");
        }
        break;
      case CLO_ERRORTEMPLATE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -errorTemplate syntax");
        scanned[iArg].n_items--;
        if (!scanItemList(&flags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "name", SDDS_STRING, errorTemplate + 0, 1, 0,
                          "description", SDDS_STRING, errorTemplate + 1, 1, 0,
                          "symbol", SDDS_STRING, errorTemplate + 2, 1, 0, NULL)) {
          SDDS_Bomb("invalid -errorTemplate syntax");
        }
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_COPY:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("Invalid copycolumns syntax provided.");
        colMatch = tmalloc(sizeof(*colMatch) * (colMatches = scanned[iArg].n_items - 1));
        for (i = 0; i < colMatches; i++)
          colMatch[i] = scanned[iArg].list[i + 1];
        break;
      default:
        fprintf(stderr, "invalid option seen: %s\n", scanned[iArg].list[0]);
        return EXIT_FAILURE;
      }
    } else {
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  processFilenames("sddsinteg", &input, &output, pipeFlags, 0, NULL);

  if (methodCode != TRAPAZOID_METHOD) {
    xErrorName = NULL;
    for (i = 0; i < yNames; i++)
      yErrorName[i] = NULL;
  }

  if (printFlags & STDOUT_PRINTOUT)
    fpPrint = stdout;

  if (yNames == 0)
    SDDS_Bomb("-integrate option must be given at least once");
  if (!checkErrorNames(yErrorName, yNames))
    SDDS_Bomb("either all -integrate quantities must have errors, or none");

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(ptr = SDDS_FindColumn(&SDDSin, FIND_NUMERIC_TYPE, xName, NULL))) {
    fprintf(stderr, "error: column %s doesn't exist\n", xName);
    return EXIT_FAILURE;
  }
  free(xName);
  xName = ptr;

  if (xErrorName) {
    if (!(ptr = SDDS_FindColumn(&SDDSin, FIND_NUMERIC_TYPE, xErrorName, NULL))) {
      fprintf(stderr, "error: column %s doesn't exist\n", xErrorName);
      return EXIT_FAILURE;
    } else {
      free(xErrorName);
      xErrorName = ptr;
    }
  }

  if (!(yNames = expandColumnPairNames(&SDDSin, &yName, &yErrorName, yNames, yExcludeName, yExcludeNames, FIND_NUMERIC_TYPE, 0))) {
    fprintf(stderr, "error: no quantities to integrate found in file\n");
    return EXIT_FAILURE;
  }

  if (!setupOutputFile(&SDDSout, &SDDSin, output, &yOutputName, &yOutputErrorName, &yOutputUnits,
                       xName, xErrorName, yName, yErrorName, yNames,
                       mainTemplate, (methodCode == GILLMILLER_METHOD) ? (char **)GMErrorTemplate : (char **)errorTemplate,
                       methodCode, columnMajorOrder, colMatch, colMatches)) {
    SDDS_Bomb("Failed to set up output file.");
  }

  while (SDDS_ReadPage(&SDDSin) > 0) {
    rows = SDDS_CountRowsOfInterest(&SDDSin);
    integral = SDDS_Realloc(integral, sizeof(*integral) * rows);
    integralError = SDDS_Realloc(integralError, sizeof(*integralError) * rows);
    if (!SDDS_StartPage(&SDDSout, rows) ||
        !SDDS_CopyPage(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    xError = NULL;
    if (!(xData = SDDS_GetColumnInDoubles(&SDDSin, xName)) ||
        (xErrorName && !(xError = SDDS_GetColumnInDoubles(&SDDSin, xErrorName))))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, xData, rows, xName) ||
        (xErrorName && !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, xError, rows, xErrorName)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    for (i = 0; i < yNames; i++) {
      yError = NULL;
      if (!(yData = SDDS_GetColumnInDoubles(&SDDSin, yName[i])) ||
          (yErrorName && yErrorName[i] &&
           !(yError = SDDS_GetColumnInDoubles(&SDDSin, yErrorName[i]))))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      switch (methodCode) {
      case TRAPAZOID_METHOD:
        trapizoid(xData, yData, xError, yError, rows, integral, integralError);
        break;
      case GILLMILLER_METHOD:
        if (GillMillerIntegration(integral, integralError, yData, xData, rows) != 0)
          SDDS_Bomb("Problem with integration: check for monotonically changing independent variable values");
        break;
      }

      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, integral, rows, yOutputName[i]) ||
          (yOutputErrorName && yOutputErrorName[i] &&
           !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_BY_NAME, integralError, rows, yOutputErrorName[i])))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      if (printFlags & BARE_PRINTOUT) {
        fprintf(fpPrint, printFormat, integral[rows - 1]);
        if (xError || yError || (yOutputErrorName && yOutputErrorName[i])) {
          fputc(' ', fpPrint);
          fprintf(fpPrint, printFormat, integralError[rows - 1]);
        }
        fputc('\n', fpPrint);
      } else if (printFlags & NORMAL_PRINTOUT) {
        fprintf(fpPrint, "%s: ", yName[i]);
        fprintf(fpPrint, printFormat, integral[rows - 1]);
        if (xError || yError || (yOutputErrorName && yOutputErrorName[i])) {
          fputs(" +/- ", fpPrint);
          fprintf(fpPrint, printFormat, integralError[rows - 1]);
          fputs(yOutputUnits[i], fpPrint);
        }
        fputc('\n', fpPrint);
      }

      if (yData)
        free(yData);
      if (yError)
        free(yError);
      yData = yError = NULL;
    }

    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (xData)
      free(xData);
    if (xError)
      free(xError);
    xData = xError = NULL;
  }

  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (integral)
    free(integral);
  if (integralError)
    free(integralError);
  if (colMatch)
    free(colMatch);

  return EXIT_SUCCESS;
}

long setupOutputFile(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *output,
                     char ***yOutputName, char ***yOutputErrorName, char ***yOutputUnits,
                     char *xName, char *xErrorName, char **yName, char **yErrorName,
                     long yNames, char **mainTemplate, char **errorTemplate, long methodCode,
                     short columnMajorOrder, char **colMatch, int32_t colMatches) {
  long i;
  char *xSymbol = NULL, *ySymbol = NULL, **col = NULL;
  int32_t cols = 0;

  *yOutputName = tmalloc(sizeof(**yOutputName) * yNames);
  *yOutputErrorName = tmalloc(sizeof(**yOutputErrorName) * yNames);
  *yOutputUnits = tmalloc(sizeof(**yOutputUnits) * yNames);

  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddsinteg output", output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, xName, NULL) ||
      (xErrorName && !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xErrorName, NULL)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_GetColumnInformation(SDDSout, "symbol", &xSymbol, SDDS_GET_BY_NAME, xName) != SDDS_STRING) {
    fprintf(stderr, "error: problem getting symbol for column %s\n", xName);
    return EXIT_FAILURE;
  }

  if (columnMajorOrder != -1)
    SDDSout->layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout->layout.data_mode.column_major = SDDSin->layout.data_mode.column_major;

  if (!xSymbol)
    SDDS_CopyString(&xSymbol, xName);

  for (i = 0; i < yNames; i++) {
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName[i], NULL)) {
      fprintf(stderr, "error: problem transferring definition for column %s\n", yName[i]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (SDDS_GetColumnInformation(SDDSout, "symbol", &ySymbol, SDDS_GET_BY_NAME, yName[i]) != SDDS_STRING) {
      fprintf(stderr, "error: problem getting symbol for column %s\n", yName[i]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (!ySymbol || SDDS_StringIsBlank(ySymbol))
      SDDS_CopyString(&ySymbol, yName[i]);

    (*yOutputUnits)[i] = multiplyColumnUnits(SDDSout, yName[i], xName);
    (*yOutputName)[i] = changeInformation(SDDSout, yName[i], yName[i], ySymbol, xName, xSymbol, mainTemplate, (*yOutputUnits)[i]);

    if (yErrorName || xErrorName || methodCode == GILLMILLER_METHOD) {
      if (yErrorName && yErrorName[i]) {
        if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yErrorName[i], NULL)) {
          fprintf(stderr, "error: problem transferring definition for column %s\n", yErrorName[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        (*yOutputErrorName)[i] = changeInformation(SDDSout, yErrorName[i], yName[i], ySymbol, xName, xSymbol, errorTemplate, (*yOutputUnits)[i]);
      } else {
        if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName[i], NULL)) {
          fprintf(stderr, "error: problem transferring error definition for column %s\n", yName[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        (*yOutputErrorName)[i] = changeInformation(SDDSout, yName[i], yName[i], ySymbol, xName, xSymbol, errorTemplate, (*yOutputUnits)[i]);
      }
    } else {
      (*yOutputErrorName)[i] = NULL;
    }
  }

  if (colMatches) {
    col = getMatchingSDDSNames(SDDSin, colMatch, colMatches, &cols, SDDS_MATCH_COLUMN);
    for (i = 0; i < cols; i++) {
      if (SDDS_GetColumnIndex(SDDSout, col[i]) < 0 && !SDDS_TransferColumnDefinition(SDDSout, SDDSin, col[i], NULL))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    SDDS_FreeStringArray(col, cols);
    free(col);
  }

  if (!SDDS_TransferAllParameterDefinitions(SDDSout, SDDSin, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  return 1;
}

char *changeInformation(SDDS_DATASET *SDDSout, char *name, char *nameRoot, char *symbolRoot,
                        char *xName, char *xSymbol, char **template, char *newUnits) {
  char buffer1[SDDS_MAXLINE], buffer2[SDDS_MAXLINE], *ptr = NULL;

  if (!SDDS_ChangeColumnInformation(SDDSout, "units", newUnits, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, name))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  makeSubstitutions(buffer1, buffer2, template[2], nameRoot, symbolRoot, xName, xSymbol);
  if (!SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer2, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, name))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  makeSubstitutions(buffer1, buffer2, template[1], nameRoot, symbolRoot, xName, xSymbol);
  if (!SDDS_ChangeColumnInformation(SDDSout, "description", buffer2, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, name))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  makeSubstitutions(buffer1, buffer2, template[0], nameRoot, symbolRoot, xName, xSymbol);
  if (!SDDS_ChangeColumnInformation(SDDSout, "name", buffer2, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, name))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  SDDS_CopyString(&ptr, buffer2);
  return ptr;
}

void makeSubstitutions(char *buffer1, char *buffer2, char *template, char *nameRoot, char *symbolRoot,
                       char *xName, char *xSymbol) {
  strcpy(buffer2, template);
  replace_string(buffer1, buffer2, "%ySymbol", symbolRoot);
  replace_string(buffer2, buffer1, "%xSymbol", xSymbol);
  replace_string(buffer1, buffer2, "%yName", nameRoot);
  replace_string(buffer2, buffer1, "%xName", xName);
  strcpy(buffer1, buffer2);
}

void trapizoid(double *x, double *y, double *sx, double *sy, int64_t n, double *integ, double *error) {
  double dx, dy;
  int64_t i;

  integ[0] = error[0] = 0;
  for (i = 1; i < n; i++) {
    dy = y[i] + y[i - 1];
    dx = x[i] - x[i - 1];
    integ[i] = integ[i - 1] + dy * dx;
    if (sx && sy)
      error[i] = error[i - 1] + sqr(dy) * (sqr(sx[i - 1]) + sqr(sx[i])) + sqr(dx) * (sqr(sy[i - 1]) + sqr(sy[i]));
    else if (sx)
      error[i] = error[i - 1] + sqr(dy) * (sqr(sx[i - 1]) + sqr(sx[i]));
    else if (sy)
      error[i] = error[i - 1] + sqr(dx) * (sqr(sy[i - 1]) + sqr(sy[i]));
    else
      error[i] = 0;
  }
  for (i = 0; i < n; i++) {
    error[i] = sqrt(error[i]) / 2;
    integ[i] /= 2;
  }
}

long checkErrorNames(char **yErrorName, long yNames) {
  long i;
  if (yErrorName[0]) {
    for (i = 1; i < yNames; i++)
      if (!yErrorName[i])
        return 0;
  } else {
    for (i = 1; i < yNames; i++)
      if (yErrorName[i])
        return 0;
  }
  return 1;
}
