/**
 * @file sddsgenericfit.c
 * @brief Performs fitting using a generic form supplied by the user.
 *
 * @details
 * The `sddsgenericfit` program uses the Simplex optimization method to fit user-defined equations
to a dataset. The equation is provided in Reverse Polish Notation (RPN) or optionally algebraic
form. The program allows detailed configuration of the optimization process and outputs
results in SDDS format.
 *
 * @section Usage
 * ```
 * sddsgenericfit [<inputfile>] [<outputfile>]
 *                [-pipe=[input][,output]]
 *                 -equation=<rpnString>[,algebraic]
 *                [-expression=<string>[,<columnName>]]
 *                [-expression=...]
 *                [-target=<value>]
 *                [-tolerance=<value>]
 *                [-simplex=[restarts=<nRestarts>][,cycles=<nCycles>,][evaluations=<nEvals>][,no1DScans]]
 *                 -variable=name=<name>,lowerLimit=<value|parameter-name>,upperLimit=<value|parameter-name>,stepsize=<value|parameter-name>,startingValue=<value|parameter-name>[,units=<string>][,heat=<value|parameter-name>]
 *                [-variable=...]
 *                [-verbosity=<integer>]
 *                [-startFromPrevious]
 *                [-majorOrder=row|column] 
 *                [-copy=<list of column names>]
 *                [-ycolumn=<ycolumn_name>[,ySigma=<sy-name>]]
 *                [-logFile=<filename>[,<flushInterval(500)>]]
 * ```
 *
 * @section Options
 * | Required Option                        | Description                                                                 |
 * |----------------------------------------|-----------------------------------------------------------------------------|
 * | `-equation`                            | The equation to fit, in RPN or optionally algebraic format.                |
 * | `-variable`                            | Specifies fitting variables with limits, step size, and initial values.    |
 * | `-ycolumn`                             | The dependent variable column name, optionally with uncertainty column.    |
 *
 * | Optional Options                       | Description                                                                 |
 * |----------------------------------------|-----------------------------------------------------------------------------|
 * | `-pipe`                                | Use pipes for input and/or output.                                         |
 * | `-expression`                          | RPN expression(s) for pre-processing or data preparation.                  |
 * | `-target`                              | Target RMS residual for an acceptable fit.                                 |
 * | `-tolerance`                           | Minimum significant change in RMS residual to continue optimization.       |
 * | `-simplex`                             | Parameters for simplex optimization.                                       |
 * | `-verbosity`                           | Sets verbosity of output during optimization.                              |
 * | `-startFromPrevious`                   | Initializes variables from the previous fit.                               |
 * | `-majorOrder`                          | Specifies row-major or column-major output order.                          |
 * | `-copy`                                | Copies specific columns from the input file to the output file.            |
 * | `-logFile`                             | Logs intermediate fitting results.                                         |
 *
 * @subsection Incompatibilities
 * - The following options cannot be used together:
 *   - `-simplex=no1DScans` with simplex cycles or evaluations specified.
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
 * M. Borland, L. Emery, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "SDDSaps.h"
#include "scan.h"
#include "rpn.h"
#include <time.h>
#include <signal.h>

/* Enumeration for option types */
enum option_type {
  SET_VARIABLE,
  SET_PIPE,
  SET_EQUATION,
  SET_COLUMNS,
  SET_TARGET,
  SET_TOLERANCE,
  SET_SIMPLEX,
  SET_VERBOSITY,
  SET_STARTFROMPREVIOUS,
  SET_EXPRESSION,
  SET_COPYCOLUMNS,
  SET_YCOLUMN,
  SET_LOGFILE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "variable",
  "pipe",
  "equation",
  "columns",
  "target",
  "tolerance",
  "simplex",
  "verbosity",
  "startfromprevious",
  "expression",
  "copycolumns",
  "ycolumn",
  "logFile",
  "majorOrder",
};

static char *USAGE =
  "Usage: sddsgenericfit [OPTIONS] [<inputfile>] [<outputfile>]\n"
  "\nOptions:\n"
  "  -pipe=[input][,output]\n"
  "  -equation=<rpnString>[,algebraic]\n"
  "  -expression=<string>[,<columnName>] [-expression=...]\n"
  "  -target=<value>\n"
  "  -tolerance=<value>\n"
  "  -simplex=[restarts=<nRestarts>][,cycles=<nCycles>][,evaluations=<nEvals>][,no1DScans]\n"
  "  -variable=name=<name>,lowerLimit=<value|parameter-name>,upperLimit=<value|parameter-name>,\n"
  "            stepsize=<value|parameter-name>,startingValue=<value|parametername>[,units=<string>][,heat=<value|parameter-name>]\n"
  "  -verbosity=<integer>\n"
  "  -startFromPrevious\n"
  "  -majorOrder=row|column\n"
  "  -copy=<list of column names>\n"
  "  -ycolumn=ycolumn_name[,ySigma=<sy-name>]\n"
  "  -logFile=<filename>[,<flushInterval(500)>]\n"
  "\nDescription:\n"
  "  Uses the Simplex method to find a fit to <y-name> as a function of <x-name> by varying the given\n"
  "  variables, which are assumed to appear in the <rpnString>.\n"
  "\nDetailed Options:\n"
  "  -ycolumn\n"
  "      Specify the name of the dependent data column and optionally <sy-name> to weight the fit.\n"
  "      This option replaces the old -columns option.\n"
  "  -copycolumns\n"
  "      Provide a list of column names to copy from the input file to the output file.\n"
  "  -logFile\n"
  "      If provided, the intermediate fitting results will be written to the specified log file.\n"
  "  -equation\n"
  "      Specify an RPN expression for the equation used in fitting. This equation can use the names\n"
  "      of any of the columns or parameters in the file, just as in sddsprocess. It is expected\n"
  "      to return a value that will be compared to the data in column <y-name>.\n"
  "  -expression\n"
  "      Specify an RPN expression to evaluate before the main equation is evaluated. Can be used\n"
  "      to prepare quantities on the stack or in variables when the equation is complicated.\n"
  "      If the <columnName> is given, values of the expression are stored in the output file\n"
  "      under the given name.\n"
  "  -target\n"
  "      Specify the value of the (weighted) RMS residual that is acceptably small to consider the\n"
  "      fit \"good\".\n"
  "  -tolerance\n"
  "      Specify the minimum change in the (weighted) RMS residual that is considered significant\n"
  "      enough to justify continuing optimization.\n"
  "  -simplex\n"
  "      Configure simplex optimization parameters such as restarts, cycles, evaluations, and disabling 1D scans.\n"
  "      Defaults are 10 restarts, 10 cycles, and 5000 evaluations.\n"
  "  -variable\n"
  "      Define a fitting variable with its name, lower limit, upper limit, step size, starting value,\n"
  "      units, and an optional heat parameter. The variable name must not match any existing column\n"
  "      or parameter in the input file.\n"
  "  -verbosity\n"
  "      Set the verbosity level of output during optimization. Higher values result in more detailed output.\n"
  "  -startFromPrevious\n"
  "      Use the final values from the previous fit as starting values for the next fit.\n"
  "  -majorOrder\n"
  "      Specify the output file's data order as row-major or column-major.\n"
  "\nProgram Information:\n"
  "  Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

void report(double res, double *a, long pass, long n_eval, long n_dimen);
void setupOutputFile(SDDS_DATASET *OutputTable, long *fitIndex, long *residualIndex, char *output,
                     SDDS_DATASET *InputTable, char *xName, char *yName, char *syName, char **variableName,
                     char **variableUnits, long variables, char **colMatch, int32_t colMatches,
                     char **expression, char **expressionColumn, long nExpressions,
                     SDDS_DATASET *logData, char *logFile, short columnMajorOrder);

double fitFunction(double *a, long *invalid);

static SDDS_DATASET InputTable;
static double *xData = NULL, *yData = NULL, *syData = NULL, *yFit = NULL, *yResidual = NULL;
static int64_t nData = 0;
static char *equation;
static long *variableMem, nVariables, verbosity;
static char **expression = NULL;
static char **expressionColumn = NULL;
static long nExpressions = 0;
static double **expressionValue = NULL;
static char **variableName = NULL;
static int64_t step = 0;
static char *logFile = NULL;
static SDDS_DATASET logData;
static int32_t maxLogRows = 500;

#define VARNAME_GIVEN 0x0001U
#define LOWER_GIVEN 0x0002U
#define UPPER_GIVEN 0x0004U
#define STEP_GIVEN 0x0008U
#define START_GIVEN 0x0010U
#define VARUNITS_GIVEN 0x0020U

static short abortRequested = 0;

void optimizationInterruptHandler(int signal) {
  simplexMinAbort(1);
  abortRequested = 1;
  fprintf(stderr, "Aborting minimization\n");
}

int main(int argc, char **argv) {
  int64_t i;
  SDDS_DATASET OutputTable;
  SCANNED_ARG *s_arg;
  long i_arg, ii;
  char *input, *output, *xName, *yName, *syName, **colMatch;
  long fitIndex, residualIndex, retval;
  int32_t colMatches;
  double rmsResidual, chiSqr, sigLevel;
  unsigned long pipeFlags, dummyFlags, majorOrderFlag;
  int32_t nEvalMax = 5000, nPassMax = 10, nRestartMax = 10;
  long nEval, iRestart;
  double tolerance, target;
  char **variableUnits;
  double *lowerLimit, *upperLimit, *stepSize, *startingValue, *paramValue, *paramDelta = NULL, *paramDelta0 = NULL;
  char **startingPar = NULL, **lowerLimitPar = NULL, **upperLimitPar = NULL, **heatPar = NULL, **stepPar = NULL;
  double *heat, *bestParamValue, bestResult = 0;
  long iVariable, startFromPrevious = 0;
  double result, lastResult = 0;
  char pfix[IFPF_BUF_SIZE];
  unsigned long simplexFlags = 0;
  char *ptr;
  short columnMajorOrder = -1;

  signal(SIGINT, optimizationInterruptHandler);

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  colMatches = 0;
  colMatch = NULL;
  if (argc <= 1) {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }
  logFile = NULL;
  input = output = equation = NULL;
  variableName = variableUnits = NULL;
  lowerLimit = upperLimit = stepSize = startingValue = heat = bestParamValue = NULL;
  nVariables = 0;
  pipeFlags = 0;
  verbosity = startFromPrevious = 0;
  xName = yName = syName = NULL;
  tolerance = target = 1e-14;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("Invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_TOLERANCE:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &tolerance) != 1 || tolerance <= 0)
          SDDS_Bomb("Incorrect -tolerance syntax");
        break;
      case SET_TARGET:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &target) != 1 || target <= 0)
          SDDS_Bomb("Incorrect -target syntax");
        break;
      case SET_VERBOSITY:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &verbosity) != 1)
          SDDS_Bomb("Incorrect -verbosity syntax");
        break;
      case SET_COLUMNS:
        if (s_arg[i_arg].n_items != 3 && s_arg[i_arg].n_items != 4)
          SDDS_Bomb("Invalid -columns syntax");
        xName = s_arg[i_arg].list[1];
        yName = s_arg[i_arg].list[2];
        s_arg[i_arg].n_items -= 3;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 3, &s_arg[i_arg].n_items, 0, "ysigma", SDDS_STRING, &syName, 1, 0, NULL))
          SDDS_Bomb("Invalid -columns syntax");
        break;
      case SET_YCOLUMN:
        if (s_arg[i_arg].n_items != 2 && s_arg[i_arg].n_items != 3)
          SDDS_Bomb("Invalid -ycolumn syntax");
        yName = s_arg[i_arg].list[1];
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0, "ysigma", SDDS_STRING, &syName, 1, 0, NULL))
          SDDS_Bomb("Invalid -ycolumn syntax");
        break;
      case SET_COPYCOLUMNS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -copycolumns syntax provided.");
        colMatch = tmalloc(sizeof(*colMatch) * (colMatches = s_arg[i_arg].n_items - 1));
        for (i = 0; i < colMatches; i++)
          colMatch[i] = s_arg[i_arg].list[i + 1];
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;
      case SET_LOGFILE:
        if (s_arg[i_arg].n_items != 2 && s_arg[i_arg].n_items != 3)
          SDDS_Bomb("Invalid -logFile syntax");
        logFile = s_arg[i_arg].list[1];
        if (s_arg[i_arg].n_items == 3 && (sscanf(s_arg[i_arg].list[2], "%" SCNd32, &maxLogRows) != 1 || maxLogRows <= 0))
          SDDS_Bomb("Invalid -logFile syntax");
        break;
      case SET_VARIABLE:
        if (!(variableName = SDDS_Realloc(variableName, sizeof(*variableName) * (nVariables + 1))) ||
            !(lowerLimit = SDDS_Realloc(lowerLimit, sizeof(*lowerLimit) * (nVariables + 1))) ||
            !(upperLimit = SDDS_Realloc(upperLimit, sizeof(*upperLimit) * (nVariables + 1))) ||
            !(stepSize = SDDS_Realloc(stepSize, sizeof(*stepSize) * (nVariables + 1))) ||
            !(heat = SDDS_Realloc(heat, sizeof(*heat) * (nVariables + 1))) ||
            !(bestParamValue = SDDS_Realloc(bestParamValue, sizeof(*bestParamValue) * (nVariables + 1))) ||
            !(startingValue = SDDS_Realloc(startingValue, sizeof(*startingValue) * (nVariables + 1))) ||
            !(startingPar = SDDS_Realloc(startingPar, sizeof(*startingPar) * (nVariables + 1))) ||
            !(lowerLimitPar = SDDS_Realloc(lowerLimitPar, sizeof(*lowerLimitPar) * (nVariables + 1))) ||
            !(upperLimitPar = SDDS_Realloc(upperLimitPar, sizeof(*upperLimitPar) * (nVariables + 1))) ||
            !(heatPar = SDDS_Realloc(heatPar, sizeof(*heatPar) * (nVariables + 1))) ||
            !(stepPar = SDDS_Realloc(stepPar, sizeof(*stepPar) * (nVariables + 1))) ||
            !(variableUnits = SDDS_Realloc(variableUnits, sizeof(*variableUnits) * (nVariables + 1))))
          SDDS_Bomb("Memory allocation failure");
        variableUnits[nVariables] = NULL;
        heat[nVariables] = 0;
        startingPar[nVariables] = heatPar[nVariables] = lowerLimitPar[nVariables] = upperLimitPar[nVariables] = stepPar[nVariables] = NULL;
        if ((s_arg[i_arg].n_items -= 1) < 5 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, variableName + nVariables, 1, VARNAME_GIVEN,
                          "lowerlimit", SDDS_STRING, lowerLimitPar + nVariables, 1, LOWER_GIVEN,
                          "upperlimit", SDDS_STRING, upperLimitPar + nVariables, 1, UPPER_GIVEN,
                          "stepsize", SDDS_STRING, &(stepPar[nVariables]), 1, STEP_GIVEN,
                          "startingvalue", SDDS_STRING, startingPar + nVariables, 1, START_GIVEN,
                          "heat", SDDS_STRING, heatPar + nVariables, 1, 0,
                          "units", SDDS_STRING, variableUnits + nVariables, 1, VARUNITS_GIVEN, NULL))
          SDDS_Bomb("Invalid -variable syntax or values");
        if (startingPar[nVariables] && sscanf(startingPar[nVariables], "%lf", startingValue + nVariables) == 1) {
          free(startingPar[nVariables]);
          startingPar[nVariables] = NULL;
        }
        if (lowerLimitPar[nVariables] && sscanf(lowerLimitPar[nVariables], "%lf", lowerLimit + nVariables) == 1) {
          free(lowerLimitPar[nVariables]);
          lowerLimitPar[nVariables] = NULL;
        }
        if (upperLimitPar[nVariables] && sscanf(upperLimitPar[nVariables], "%lf", upperLimit + nVariables) == 1) {
          free(upperLimitPar[nVariables]);
          upperLimitPar[nVariables] = NULL;
        }
        if (heatPar[nVariables] && sscanf(heatPar[nVariables], "%lf", heat + nVariables) == 1) {
          free(heatPar[nVariables]);
          heatPar[nVariables] = NULL;
        }
        if (stepPar[nVariables] && sscanf(stepPar[nVariables], "%lf", stepSize + nVariables) == 1) {
          free(stepPar[nVariables]);
          stepPar[nVariables] = NULL;
        }
        if ((dummyFlags & (VARNAME_GIVEN | LOWER_GIVEN | UPPER_GIVEN | STEP_GIVEN | START_GIVEN)) != (VARNAME_GIVEN | LOWER_GIVEN | UPPER_GIVEN | STEP_GIVEN | START_GIVEN))
          SDDS_Bomb("Insufficient information given for -variable");
        if (!strlen(variableName[nVariables]))
          SDDS_Bomb("Invalid blank variable name");
        if (!lowerLimitPar[nVariables] && !upperLimitPar[nVariables] && lowerLimit[nVariables] >= upperLimit[nVariables])
          SDDS_Bomb("Invalid limits value for variable");

        if (!lowerLimitPar[nVariables] && !upperLimitPar[nVariables] && !startingPar[nVariables] &&
            (startingValue[nVariables] <= lowerLimit[nVariables] || startingValue[nVariables] >= upperLimit[nVariables]))
          SDDS_Bomb("Invalid limits or starting value for variable");
        if (!stepPar[nVariables] && stepSize[nVariables] <= 0)
          SDDS_Bomb("Invalid step size for variable");
        nVariables++;
        break;
      case SET_SIMPLEX:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&simplexFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "restarts", SDDS_LONG, &nRestartMax, 1, 0,
                          "cycles", SDDS_LONG, &nPassMax, 1, 0,
                          "evaluations", SDDS_LONG, &nEvalMax, 1, 0,
                          "no1dscans", -1, NULL, 0, SIMPLEX_NO_1D_SCANS, NULL) ||
            nRestartMax < 0 || nPassMax <= 0 || nEvalMax <= 0)
          SDDS_Bomb("Invalid -simplex syntax/values");
        break;
      case SET_EQUATION:
        if ((s_arg[i_arg].n_items < 2) || (s_arg[i_arg].n_items > 3))
          SDDS_Bomb("Invalid -equation syntax");
        if (s_arg[i_arg].n_items == 2) {
          if (!strlen(equation = s_arg[i_arg].list[1])) {
            SDDS_Bomb("Invalid -equation syntax");
          }
        } else if (s_arg[i_arg].n_items == 3) {
          if (strncmp(s_arg[i_arg].list[2], "algebraic", strlen(s_arg[i_arg].list[2])) == 0) {
            ptr = addOuterParentheses(s_arg[i_arg].list[1]);
            if2pf(pfix, ptr, sizeof pfix);
            free(ptr);
            if (!SDDS_CopyString(&equation, pfix)) {
              fprintf(stderr, "Error: Problem copying equation string\n");
              exit(EXIT_FAILURE);
            }
          } else {
            SDDS_Bomb("Invalid -equation syntax");
          }
        }
        break;
      case SET_EXPRESSION:
        if (s_arg[i_arg].n_items != 2 && s_arg[i_arg].n_items != 3)
          SDDS_Bomb("Invalid -expression syntax");
        expression = trealloc(expression, sizeof(*expression) * (nExpressions + 1));
        expression[nExpressions] = s_arg[i_arg].list[1];
        expressionColumn = trealloc(expressionColumn, sizeof(*expressionColumn) * (nExpressions + 1));
        if (s_arg[i_arg].n_items == 3)
          expressionColumn[nExpressions] = s_arg[i_arg].list[2];
        else
          expressionColumn[nExpressions] = NULL;
        nExpressions++;
        break;
      case SET_STARTFROMPREVIOUS:
        startFromPrevious = 1;
        break;
      default:
        fprintf(stderr, "Error: Unknown or ambiguous option: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many filenames provided");
    }
  }

  processFilenames("sddsgenericfit", &input, &output, pipeFlags, 0, NULL);

  if (!yName)
    SDDS_Bomb("-ycolumn option must be given");
  if (nVariables == 0)
    SDDS_Bomb("You must specify at least one -variable option");
  if (equation == NULL)
    SDDS_Bomb("You must specify an equation string");

  rpn(getenv("RPN_DEFNS"));
  if (rpn_check_error())
    exit(EXIT_FAILURE);

  if (!SDDS_InitializeInput(&InputTable, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if ((xName && !SDDS_FindColumn(&InputTable, FIND_NUMERIC_TYPE, xName, NULL)) ||
      !SDDS_FindColumn(&InputTable, FIND_NUMERIC_TYPE, yName, NULL) ||
      (syName && !SDDS_FindColumn(&InputTable, FIND_NUMERIC_TYPE, syName, NULL)))
    SDDS_Bomb("One or more of the specified data columns does not exist or is non-numeric");

  setupOutputFile(&OutputTable, &fitIndex, &residualIndex, output, &InputTable, xName, yName, syName,
                  variableName, variableUnits, nVariables, colMatch, colMatches,
                  expression, expressionColumn, nExpressions,
                  &logData, logFile, columnMajorOrder);

  if (!(paramValue = SDDS_Malloc(sizeof(*paramValue) * nVariables)) ||
      !(paramDelta = SDDS_Malloc(sizeof(*paramDelta) * nVariables)) ||
      !(paramDelta0 = SDDS_Malloc(sizeof(*paramDelta0) * nVariables)) ||
      !(variableMem = SDDS_Malloc(sizeof(*variableMem) * nVariables)))
    SDDS_Bomb("Memory allocation failure");
  for (iVariable = 0; iVariable < nVariables; iVariable++)
    variableMem[iVariable] = rpn_create_mem(variableName[iVariable], 0);

  while ((retval = SDDS_ReadPage(&InputTable)) > 0) {
    if ((xName && !(xData = SDDS_GetColumnInDoubles(&InputTable, xName))) ||
        !(yData = SDDS_GetColumnInDoubles(&InputTable, yName)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (syName && !(syData = SDDS_GetColumnInDoubles(&InputTable, syName)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if ((nData = SDDS_CountRowsOfInterest(&InputTable)) <= nVariables)
      continue;

    for (iVariable = 0; iVariable < nVariables; iVariable++) {
      if (startingPar[iVariable] && !SDDS_GetParameterAsDouble(&InputTable, startingPar[iVariable], &startingValue[iVariable]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (lowerLimitPar[iVariable] && !SDDS_GetParameterAsDouble(&InputTable, lowerLimitPar[iVariable], &lowerLimit[iVariable]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (upperLimitPar[iVariable] && !SDDS_GetParameterAsDouble(&InputTable, upperLimitPar[iVariable], &upperLimit[iVariable]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (heatPar[iVariable] && !SDDS_GetParameterAsDouble(&InputTable, heatPar[iVariable], &heat[iVariable]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (stepPar[iVariable] && !SDDS_GetParameterAsDouble(&InputTable, stepPar[iVariable], &stepSize[iVariable]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (SDDS_GetParameterIndex(&InputTable, variableName[iVariable]) >= 0) {
        if (!SDDS_GetParameterAsDouble(&InputTable, variableName[iVariable], &paramValue[iVariable]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else if (retval == 1 || !startFromPrevious)
        paramValue[iVariable] = startingValue[iVariable];
      paramDelta[iVariable] = stepSize[iVariable];
    }
    if (verbosity > 2) {
      /* Show starting values */
      fprintf(stderr, "Starting values and step sizes:\n");
      for (iVariable = 0; iVariable < nVariables; iVariable++)
        fprintf(stderr, "  %s = %le  %le\n", variableName[iVariable], paramValue[iVariable], paramDelta[iVariable]);
    }

    if (!(yFit = SDDS_Realloc(yFit, sizeof(*yFit) * nData)) ||
        !(yResidual = SDDS_Realloc(yResidual, sizeof(*yResidual) * nData)))
      SDDS_Bomb("Memory allocation failure");
    if (nExpressions) {
      long j;
      expressionValue = SDDS_Realloc(expressionValue, sizeof(*expressionValue) * nExpressions);
      for (j = 0; j < nExpressions; j++)
        expressionValue[j] = malloc(sizeof(**expressionValue) * nData);
    }
    SDDS_StoreParametersInRpnMemories(&InputTable);
    memcpy(paramDelta0, paramDelta, sizeof(*paramDelta) * nVariables);
    for (iRestart = nEval = 0; iRestart <= nRestartMax; iRestart++) {
      memcpy(paramDelta, paramDelta0, sizeof(*paramDelta) * nVariables);
      if (iRestart) {
        if (iRestart == 1)
          random_1(-labs(2 * ((long)time(NULL) / 2) + 1));
        for (iVariable = 0; iVariable < nVariables; iVariable++) {
          paramValue[iVariable] += gauss_rn_lim(0.0, heat[iVariable], 2, random_1);
          if (paramValue[iVariable] < lowerLimit[iVariable])
            paramValue[iVariable] = lowerLimit[iVariable] + paramDelta[iVariable];
          if (paramValue[iVariable] > upperLimit[iVariable])
            paramValue[iVariable] = upperLimit[iVariable] - paramDelta[iVariable];
        }
      }
      nEval += simplexMin(&result, paramValue, paramDelta, lowerLimit, upperLimit, NULL, nVariables, target, tolerance, fitFunction, (verbosity > 0 ? report : NULL), nEvalMax, nPassMax, 12, 3, 1.0, simplexFlags);
      if (iRestart != 0 && result > bestResult) {
        result = bestResult;
        for (iVariable = 0; iVariable < nVariables; iVariable++)
          paramValue[iVariable] = bestParamValue[iVariable];
      } else {
        bestResult = result;
        for (iVariable = 0; iVariable < nVariables; iVariable++)
          bestParamValue[iVariable] = paramValue[iVariable];
      }
      if (verbosity > 0)
        fprintf(stderr, "Result of simplex minimization: %le\n", result);
      if (result < target || (iRestart != 0 && fabs(lastResult - result) < tolerance))
        break;
      lastResult = result;
      if (abortRequested)
        break;
      if (verbosity > 0)
        fprintf(stderr, "Performing restart %ld\n", iRestart + 1);
    }

    for (iVariable = 0; iVariable < nVariables; iVariable++)
      paramValue[iVariable] = bestParamValue[iVariable];
    result = fitFunction(paramValue, &ii);

    if (verbosity > 3)
      fprintf(stderr, "%ld evaluations of fit function required, giving result %e\n", nEval, result);
    for (i = result = 0; i < nData; i++)
      result += sqr(yResidual[i]);
    rmsResidual = sqrt(result / nData);
    if (syData) {
      for (i = chiSqr = 0; i < nData; i++)
        chiSqr += sqr(yResidual[i] / syData[i]);
    } else {
      double sy2;
      sy2 = result / (nData - nVariables);
      for (i = chiSqr = 0; i < nData; i++)
        chiSqr += sqr(yResidual[i]) / sy2;
    }
    sigLevel = ChiSqrSigLevel(chiSqr, nData - nVariables);
    if (verbosity > 1) {
      if (syData)
        fprintf(stderr, "Significance level: %.5e\n", sigLevel);
      fprintf(stderr, "RMS deviation: %.15e\n", rmsResidual);
    }

    if (!SDDS_StartPage(&OutputTable, nData) ||
        !SDDS_CopyPage(&OutputTable, &InputTable) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, yFit, nData, fitIndex) ||
        !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_INDEX, yResidual, nData, residualIndex))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < nExpressions; i++)
      if (expressionColumn[i] &&
          !SDDS_SetColumn(&OutputTable, SDDS_SET_BY_NAME, expressionValue[i], nData, expressionColumn[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (iVariable = 0; iVariable < nVariables; iVariable++)
      if (!SDDS_SetParameters(&OutputTable, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, variableName[iVariable], paramValue[iVariable], NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_WritePage(&OutputTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (logFile && !SDDS_WritePage(&logData))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    step = 0;
    free(xData);
    free(yData);
    if (syData)
      free(syData);
  }
  if (!SDDS_Terminate(&InputTable) || !SDDS_Terminate(&OutputTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (logFile && !SDDS_Terminate(&logData))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (colMatches)
    free(colMatch);
  if (syName)
    free(syName);
  SDDS_FreeStringArray(variableName, nVariables);
  free_scanargs(&s_arg, argc);
  free(lowerLimit);
  free(upperLimit);
  free(stepSize);
  free(heat);
  free(bestParamValue);
  free(startingValue);
  free(startingPar);
  free(lowerLimitPar);
  free(upperLimitPar);
  free(heatPar);
  free(stepPar);
  free(variableUnits);

  return EXIT_SUCCESS;
}

void setupOutputFile(SDDS_DATASET *OutputTable, long *fitIndex, long *residualIndex, char *output,
                     SDDS_DATASET *InputTable, char *xName, char *yName, char *syName, char **variableName,
                     char **variableUnits, long variables, char **colMatch, int32_t colMatches,
                     char **expression, char **expressionColumn, long nExpressions,
                     SDDS_DATASET *logData, char *logFile, short columnMajorOrder) {
  char *name, *yUnits = NULL, **col = NULL;
  int32_t typeValue = SDDS_DOUBLE;
  static char *residualNamePart = "Residual";
  long i;
  int32_t cols = 0;

  if (!SDDS_InitializeOutput(OutputTable, SDDS_BINARY, 0, NULL, "sddsgfit output", output) ||
      !SDDS_TransferColumnDefinition(OutputTable, InputTable, yName, NULL) ||
      !SDDS_ChangeColumnInformation(OutputTable, "type", &typeValue, SDDS_BY_NAME, yName))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    OutputTable->layout.data_mode.column_major = columnMajorOrder;
  else
    OutputTable->layout.data_mode.column_major = InputTable->layout.data_mode.column_major;
  if (logFile && !!SDDS_InitializeOutput(logData, SDDS_BINARY, 0, NULL, "sddsgenericfit log", logFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (syName && (!SDDS_TransferColumnDefinition(OutputTable, InputTable, syName, NULL) ||
                 !SDDS_ChangeColumnInformation(OutputTable, "type", &typeValue, SDDS_BY_NAME, syName)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (xName && !SDDS_TransferColumnDefinition(OutputTable, InputTable, xName, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (colMatches) {
    col = getMatchingSDDSNames(InputTable, colMatch, colMatches, &cols, SDDS_MATCH_COLUMN);
    for (i = 0; i < cols; i++) {
      if (SDDS_GetColumnIndex(OutputTable, col[i]) < 0 &&
          !SDDS_TransferColumnDefinition(OutputTable, InputTable, col[i], NULL))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    SDDS_FreeStringArray(col, cols);
  }
  name = tmalloc(sizeof(*name) * (strlen(yName) + strlen(residualNamePart) + 1));
  sprintf(name, "%s%s", yName, residualNamePart);
  if ((*residualIndex = SDDS_DefineColumn(OutputTable, name, NULL, yUnits, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(name, "%sFit", yName);
  if ((*fitIndex = SDDS_DefineColumn(OutputTable, name, NULL, yUnits, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(name);

  for (i = 0; i < variables; i++) {
    if (SDDS_DefineParameter(OutputTable, variableName[i], NULL, variableUnits[i], NULL, NULL, SDDS_DOUBLE, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (logFile && SDDS_DefineColumn(logData, variableName[i], NULL, variableUnits[i], NULL, NULL, SDDS_DOUBLE, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  for (i = 0; i < nExpressions; i++) {
    if (expressionColumn[i]) {
      if (SDDS_DefineColumn(OutputTable, expressionColumn[i], NULL, NULL, expression[i], NULL, SDDS_DOUBLE, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!SDDS_TransferAllParameterDefinitions(OutputTable, InputTable, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(OutputTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (logFile && (!SDDS_DefineSimpleColumn(logData, "Step", NULL, SDDS_LONG) ||
                  !SDDS_DefineSimpleColumn(logData, "Chi", NULL, SDDS_DOUBLE) ||
                  !SDDS_WriteLayout(logData)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

double fitFunction(double *a, long *invalid) {
  double sum, tmp;
  int64_t i;
  long j;

  rpn_clear();
  *invalid = 0;

  for (i = 0; i < nVariables; i++)
    rpn_store(a[i], NULL, variableMem[i]);

  if (verbosity > 10)
    fprintf(stderr, "Running fit function:\n");
  if (!syData) {
    for (i = sum = 0; i < nData; i++) {
      if (!SDDS_StoreRowInRpnMemories(&InputTable, i))
        SDDS_Bomb("Problem storing data in RPN memories");
      rpn_clear();
      for (j = 0; j < nExpressions; j++) {
        expressionValue[j][i] = rpn(expression[j]);
        if (verbosity > 10)
          fprintf(stderr, "Expression %ld: %le\n", j, expressionValue[j][i]);
      }
      yFit[i] = rpn(equation);
      if (rpn_check_error()) {
        *invalid = 1;
        return 0;
      }
      yResidual[i] = tmp = (yFit[i] - yData[i]);
      if (verbosity > 10) {
        if (xData)
          fprintf(stderr, "i=%" PRId64 " x=%le y=%le fit=%le\n", i, xData[i], yData[i], yFit[i]);
        else
          fprintf(stderr, "i=%" PRId64 " y=%le fit=%le\n", i, yData[i], yFit[i]);
      }
      sum += sqr(tmp);
    }
  } else {
    for (i = sum = 0; i < nData; i++) {
      if (!SDDS_StoreRowInRpnMemories(&InputTable, i))
        SDDS_Bomb("Problem storing data in RPN memories");
      rpn_clear();
      for (j = 0; j < nExpressions; j++)
        expressionValue[j][i] = rpn(expression[j]);
      yFit[i] = rpn(equation);
      if (rpn_check_error()) {
        *invalid = 1;
        return 0;
      }
      yResidual[i] = tmp = (yFit[i] - yData[i]);
      sum += sqr(tmp / syData[i]);
    }
  }
  if (logFile) {
    if (!step && !SDDS_StartPage(&logData, maxLogRows))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    step++;
    if (step && (step % maxLogRows) == 0) {
      if (!SDDS_UpdatePage(&logData, FLUSH_TABLE))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_SetRowValues(&logData, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step - 1, "Step", step, "Chi", sum / nData, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < nVariables; i++) {
      if (!SDDS_SetRowValues(&logData, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step - 1, variableName[i], a[i], NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  return (sum / nData);
}

void report(double y, double *x, long pass, long n_eval, long n_dimen) {
  long i;

  fprintf(stderr, "pass %ld, after %ld evaluations: result = %.16e\na = ", pass, n_eval, y);
  for (i = 0; i < n_dimen; i++)
    fprintf(stderr, "%.8e ", x[i]);
  fputc('\n', stderr);
}
