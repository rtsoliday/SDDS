/************************************************************************* \
* Copyright (c) 2020 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2020 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sdds2dpfit
 * purpose: do 2-dimensional polynomial least squares fitting for SDDS files.
 *
 * Michael Borland, 2020.
 */
#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "matrixop.h"
#ifdef MKL
#include <omp.h>
#endif

#define CLO_INDEPENDENT 0
#define CLO_DEPENDENT 1
#define CLO_MAXIMUM_ORDER 2
#define CLO_ADD_ORDERS 3
#define CLO_SIGMAS  4
#define CLO_PIPE 5
#define CLO_EVALUATE 6
#define CLO_COEFFICIENTS 7
#define CLO_COPY_PARAMETERS 8
#define CLO_SQUARE_ARRAY 9
#define CLO_SUM_LIMIT 10
#define CLO_THREADS 11
#define N_OPTIONS 12

char *option[N_OPTIONS] = {
  "independent", "dependent", "maximumorder", "addorders", "sigmas", "pipe", "evaluate", "coefficients",
  "copyparameters", "squarearray", "sumlimit", "threads"
};

char *USAGE="sdds2dpfit [<inputfile>] [<outputfile>] [-pipe=[input][,output]]\n\
  -independent=<x1ColumnName>,<x2ColumnName> -dependent=<yColumnName>[,<sigmaColumnName>]\n\
  {-maximumOrder=<value>,[<value>] [-squareArray] [-sumLimit=<value>] |  [-addOrders=<xOrder>,<yOrder> [-addOrders=...]]}\n \
  [-coefficients=<filename>] [-evaluate=<locationsFilename>,<x1Name>,<x2Name>,<outputFilename>]\n\
  [-copyParameters]\n\
  [-threads=<number>]\n\n\
Program by Michael Borland. ("__DATE__" "__TIME__", SVN revision: "SVN_VERSION")\n";

static char *additional_help1 = "\n\
sdds2dpfit does polynomial fits in 2 dimensions: y=P(x1,x2).\n\n\
-independent           names of independent variable data\n\
-dependent             name of dependent variable data, and optional error bar.\n\
-maximumOrder          If one value is given, requests inclusion of all terms up to x^n*y^m such that (n+m)<=order\n\
                       If two values are given, requests inclusion of all terms up to x^value1*y^value2.\n\
-squareArray           if given with -maximumOrder, include all terms up to x^n*y^m such that\n\
                       n<=order and m<=order. Equivalent to giving -maximumOrder=order,order \n\
-sumLimit              If two values (maxn,maxm) are given for -maximumOrder, by default all cross-terms are\n\
                       included up to x^maxn*y^maxm. If -sumLimit=p is given, the cross terms are limited by\n\
                       m+n<=p. Ignored if the value is <=0.\n\
-addOrders             request inclusion of of x^n*y^m in the fit.\n\
-coefficients          specify file for output of fit coefficients\n\
-evaluate              specify evaluation of fit at points given in the locations file.\n\
-copyParameters        if given, program copies all parameters from the input file\n\
                       into the main output file.  By default, no parameters are copied.\n";


int lsf2dPoly(double *x[2], double *y, double *sy, long points, int32_t *order[2], long nOrders, 
              double *coef, double *chi, double *condition, double *diff, double *xEval[2], long nEvalPoints, double *yEval);
void print_coefs(FILE *fprec, double *x_offset, double *x_scale,
                 double *coef, double *s_coef, int32_t *order, long n_terms, double chi, 
                 long norm_term, char *prepend);
char **makeCoefficientUnits(SDDS_DATASET *SDDSout, char *xName[2], char *yName, 
                            int32_t *order[2], long nOrders);
void setCoefficientData(SDDS_DATASET *SDDSout, double *coef, long nOrders);
char **initializeOutputFile(SDDS_DATASET *SDDSout, char *output, SDDS_DATASET *SDDSin, char *input,
			    char *xName[2], char *yName, char *ySigmaName,  int32_t *order[2], long nOrders,
                            long copyParameters);
void checkInputFile(SDDS_DATASET *SDDSin, char *xName[2], char *yName, char *ySigmaName);
long coefficient_index(int32_t *order, long terms, long order_of_interest);
void makeFitLabel(char *buffer, long bufsize, char *fitLabelFormat, double *coef, int32_t *order, long terms);
long readEvaluationPoints(char *evalLocationFile, char *xName0, char *xName1, double **xData0, double **xData1);
void initializeEvaluationFile(SDDS_DATASET *SDDSeval, char *evalOutputFile, SDDS_DATASET *SDDSin,
                              char *xName[2], char *xEvalName[2], char *yName,  int32_t *order[2], long nOrders, long copyParameters);
void writeEvaluationData(SDDS_DATASET *SDDSeval, double *xEval[2], long nEvalPoints, double *yEval, 
                         char *xEvalName[2], char *yName, long copyParameters, SDDS_DATASET *SDDSin);
void initializeCoefficientsFile(SDDS_DATASET *SDDScoef, char *coefficientsFile, SDDS_DATASET *SDDSin, 
                                char **coefUnits, char *xName[2], long nOrders, long copyParameters);
void writeCoefficientData(SDDS_DATASET *SDDScoef, double *coef, int32_t *order[2], long nOrders,
                          long copyParameters, SDDS_DATASET *SDDSin);

long ix[2], iy, iySigma, iFit, iResidual;
long iRmsResidual, iChiSqr, iFitIsValid, iTerms, *iTerm, iConditionNumber;

int main(int argc, char **argv)
{
  char **coefUnits;
  double *x[2], *y, *sy;
  double *diff, *coef, rmsResidual, chi, condition;
  char *xName[2], *yName, *ySigmaName;
  long copyParameters=0;
  char *input, *output;
  SDDS_DATASET SDDSin, SDDSout, SDDSeval, SDDScoef;
  int32_t *order[2];
  long i, j, k, nOrders, maximumOrder[2], sumLimit, squareArray, invalid, points, isFit;
  SCANNED_ARG *s_arg;
  char *evalLocationFile, *xEvalName[2],  *evalOutputFile, *coefficientsFile;
  double *xEval[2], *yEval;
  long nEvalPoints, iArg;
  unsigned long pipeFlags;
  int threads = 1;
  
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc<2) {
    fprintf(stderr, "usage: %s%s\n", USAGE, additional_help1);
    exit(1);
  }

  rmsResidual = chi = condition = -1;
  input = output = NULL;
  x[0] = x[1] = NULL;
  y = sy = NULL;
  xName[0] = xName[1] = NULL;
  yName = NULL;
  ySigmaName = NULL;
  input = output = NULL;
  evalLocationFile = evalOutputFile = NULL;
  xEvalName[0] = xEvalName[1] = NULL;
  coefficientsFile = NULL;
  order[0] = order[1] = NULL;
  maximumOrder[0] = maximumOrder[1] = -1;
  squareArray = 0;
  sumLimit = -1;
  nOrders = -1;
  pipeFlags = 0;

  for (iArg=1; iArg<argc; iArg++) {
    if (s_arg[iArg].arg_type==OPTION) {
      switch (match_string(s_arg[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_ADD_ORDERS:
	if (s_arg[iArg].n_items<3)
	  SDDS_Bomb("invalid -addOrders syntax");
        if (nOrders==-1)
          nOrders = 0;
	order[0] = SDDS_Realloc(order[0], sizeof(int32_t)*(nOrders+1));
	order[1] = SDDS_Realloc(order[1], sizeof(int32_t)*(nOrders+1));
        if (sscanf(s_arg[iArg].list[1], "%" SCNd32, order[0]+nOrders)!=1 ||
            sscanf(s_arg[iArg].list[2], "%" SCNd32, order[1]+nOrders)!=1)
          SDDS_Bomb("unable to scan order from -addOrders list");
        nOrders++;
	break;
      case CLO_MAXIMUM_ORDER:
        if (s_arg[iArg].n_items!=2 && s_arg[iArg].n_items!=3)
	  SDDS_Bomb("invalid -maximumOrder syntax");
        if (sscanf(s_arg[iArg].list[1], "%ld", &maximumOrder[0])!=1 || maximumOrder[0]<0)
          SDDS_Bomb("invalid -maximumOrder syntax");
        if (s_arg[iArg].n_items==3 && 
            (sscanf(s_arg[iArg].list[2], "%ld", &maximumOrder[1])!=1 || maximumOrder[1]<0))
          SDDS_Bomb("invalid -maximumOrder syntax");
	break;
      case CLO_SQUARE_ARRAY:
	if (s_arg[iArg].n_items!=1)
	  SDDS_Bomb("invalid -squareArray syntax");
        squareArray = 1;
	break;
      case CLO_SUM_LIMIT:
        if (s_arg[iArg].n_items!=2)
	  SDDS_Bomb("invalid -sumLimit syntax");
        if (sscanf(s_arg[iArg].list[1], "%ld", &sumLimit)!=1)
          SDDS_Bomb("invalid -sumLimit syntax");
	break;
      case CLO_INDEPENDENT:
	if (s_arg[iArg].n_items!=3)
	  SDDS_Bomb("invalid -independent syntax");
        xName[0] = s_arg[iArg].list[1];
        xName[1] = s_arg[iArg].list[2];
	break;
      case CLO_DEPENDENT:
	if (s_arg[iArg].n_items!=2 && s_arg[iArg].n_items!=3)
	  SDDS_Bomb("invalid -dependent syntax");
        yName = s_arg[iArg].list[1];
        if (s_arg[iArg].n_items==3)
          ySigmaName = s_arg[iArg].list[2];
	break;
      case CLO_PIPE:
	if (!processPipeOption(s_arg[iArg].list+1, s_arg[iArg].n_items-1, &pipeFlags))
	  SDDS_Bomb("invalid -pipe syntax");
	break;
      case CLO_COEFFICIENTS:
        if (s_arg[iArg].n_items!=2) 
          SDDS_Bomb("invalid -coefficients syntax");
        coefficientsFile = s_arg[iArg].list[1];
        break;
      case CLO_EVALUATE:
	if (s_arg[iArg].n_items!=5)
	  SDDS_Bomb("invalid -evaluate syntax");
        evalLocationFile = s_arg[iArg].list[1];
        xEvalName[0] = s_arg[iArg].list[2];
        xEvalName[1] = s_arg[iArg].list[3];
        evalOutputFile = s_arg[iArg].list[4];
	break;
      case CLO_COPY_PARAMETERS:
        copyParameters = 1;
        break;
      case CLO_THREADS:
        if (s_arg[iArg].n_items != 2 ||
            !sscanf(s_arg[iArg].list[1], "%d", &threads) || threads < 1)
          SDDS_Bomb("invalid -threads syntax");
        break;
      default:
	bomb("unknown switch", USAGE);
	break;
      }
    }
    else {
      if (input==NULL)
	input = s_arg[iArg].list[0];
      else if (output==NULL)
	output = s_arg[iArg].list[0];
      else
	SDDS_Bomb("too many filenames");
    }
  }
  
  processFilenames("sdds2dpfit", &input, &output, pipeFlags, 0, NULL);

  if (maximumOrder[0]>0 && nOrders>0)
    SDDS_Bomb("can't specify both -maximumOrder and -addOrders");
  if (nOrders>0 && squareArray)
    SDDS_Bomb("can't specify both -squareArray and -addOrders");
  if (squareArray && maximumOrder[1]>0)
    SDDS_Bomb("can't specify both two values to maximumOrder and -squareArray");
  if (maximumOrder[0]<0 && nOrders<=0)
    SDDS_Bomb("specify either -maximumOrder and -addOrders");
  if (!yName)
    SDDS_Bomb("you must specify a column name for dependent variable");
  if (!xName[0] || !xName[1])
    SDDS_Bomb("you must specify column names for both independent variables");

  if (maximumOrder[0]>0) {
    if (maximumOrder[1]<0) {
      if (squareArray)
        nOrders = (maximumOrder[0]+1)*(maximumOrder[0]+1);
      else
        nOrders = (maximumOrder[0]+1)*(maximumOrder[0]+2)/2;
      order[0] = tmalloc(sizeof(long)*nOrders);
      order[1] = tmalloc(sizeof(long)*nOrders);
      for (i=k=0; i<=maximumOrder[0]; i++) {
        for (j=0; j<=(maximumOrder[0]-(squareArray?0:i)); j++) {
          if (sumLimit<=0 || (i+j)<=sumLimit) {
            order[0][k] = i;
            order[1][k] = j;
            k++;
          }
        }
      }
    } else {
      nOrders = (maximumOrder[0]+1)*(maximumOrder[1]+1);
      order[0] = tmalloc(sizeof(long)*nOrders);
      order[1] = tmalloc(sizeof(long)*nOrders);
      for (i=k=0; i<=maximumOrder[0]; i++) {
        for (j=0; j<=maximumOrder[1]; j++) {
          if (sumLimit<=0 || (i+j)<=sumLimit) {
            order[0][k] = i;
            order[1][k] = j;
            k++;
          }
        }
      }
    }
    nOrders = k;
  }

  coef = tmalloc(sizeof(*coef)*nOrders);

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  checkInputFile(&SDDSin, xName, yName, ySigmaName); 
  
  coefUnits = initializeOutputFile(&SDDSout, output, &SDDSin, input, 
                                   xName, yName, ySigmaName, order, nOrders, copyParameters);

  nEvalPoints = 0;
  yEval = NULL;
  if (evalLocationFile) {
    nEvalPoints = readEvaluationPoints(evalLocationFile, xEvalName[0], xEvalName[1], &xEval[0], &xEval[1]);
    yEval = tmalloc(sizeof(double)*nEvalPoints);
    initializeEvaluationFile(&SDDSeval, evalOutputFile, &SDDSin, xName, xEvalName, yName, order, nOrders, copyParameters);
  }
  if (coefficientsFile)
    initializeCoefficientsFile(&SDDScoef, coefficientsFile, &SDDSin, coefUnits, xName, nOrders, copyParameters);

  while (SDDS_ReadPage(&SDDSin)>0) {
    invalid = 0;
    isFit = 0;     
    x[0] = x[1] = y = diff = NULL;
    if ((points = SDDS_CountRowsOfInterest(&SDDSin))<nOrders) {
      invalid=1;
    } else {
      for (i=0; i<2; i++)
        if (!(x[i]=SDDS_GetColumnInDoubles(&SDDSin, xName[i]))) {
          fprintf(stderr, "error: unable to read column %s\n", xName[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        }
      if (!(y=SDDS_GetColumnInDoubles(&SDDSin, yName))) {
        fprintf(stderr, "error: unable to read column %s\n", yName);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      }
      sy = NULL;
      if (ySigmaName && !(sy=SDDS_GetColumnInDoubles(&SDDSin, ySigmaName))) {
        fprintf(stderr, "error: unable to read column %s\n", ySigmaName);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      }
      
      diff   = tmalloc(sizeof(*diff)*points);
      if (lsf2dPoly(x, y, sy, points, order, nOrders, coef, &chi, &condition, diff, xEval, nEvalPoints, yEval)) {
        rmsResidual = rmsValueThreaded(diff, points, threads);
        isFit = 1;
      } else {
        invalid = 1;
        isFit = 0;
      }
    }

    if (!SDDS_StartPage(&SDDSout, points)) 
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (!invalid) {
      setCoefficientData(&SDDSout, coef, nOrders);
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, x[0], points, ix[0]) ||
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, x[1], points, ix[1]) ||
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, y, points, iy) ||
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, diff, points, iResidual))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      for (i=0; i<points; i++)
        diff[i] = y[i] - diff[i]; /* computes fit values from residual and y */
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, diff, points, iFit))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      
      if (iySigma!=-1 &&
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, sy, points, iySigma))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

      if (evalLocationFile) 
        writeEvaluationData(&SDDSeval, xEval, nEvalPoints, yEval, xEvalName, yName, copyParameters, &SDDSin);
      if (coefficientsFile)
        writeCoefficientData(&SDDScoef, coef, order, nOrders, copyParameters, &SDDSin);

    }
    if (copyParameters && !SDDS_CopyParameters(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX|SDDS_PASS_BY_VALUE,
                            iRmsResidual, rmsResidual,
                            iChiSqr, chi, iConditionNumber, condition,
                            iFitIsValid,  isFit?'y':'n',
                            iTerms, nOrders,
                            -1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    for (i=0; i<nOrders; i++)
      if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX|SDDS_PASS_BY_VALUE,
                              iTerm[i], coef[i], -1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (!invalid) {
      free(diff);
      free(sy);
      free(x[0]);
      free(x[1]);
      free(y);
    }
  }
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout)  ||
      (evalLocationFile && !SDDS_Terminate(&SDDSeval)) ||
      (coefficientsFile && !SDDS_Terminate(&SDDScoef))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  return(0);
}


void checkInputFile(SDDS_DATASET *SDDSin, char *xName[2], char *yName, char *ySigmaName)
{
  char *ptr=NULL;
  long i;
  for (i=0; i<2; i++) {
    if (!(ptr=SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, xName[i], NULL)))
      SDDS_Bomb("At least one x column doesn't exist or is nonnumeric");
    free(ptr);
    ptr = NULL;
  }
  if (!(ptr=SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, yName, NULL)))
    SDDS_Bomb("y column doesn't exist or is nonnumeric");
  free(ptr);
  ptr = NULL;
  if (ySigmaName && !(ptr=SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, ySigmaName, NULL)))
    SDDS_Bomb("y sigma column doesn't exist or is nonnumeric");
  if (ptr)
    free(ptr);
  ptr = NULL;
}

char **initializeOutputFile(SDDS_DATASET *SDDSout, char *output, SDDS_DATASET *SDDSin, char *input,
                            char *xName[2], char *yName, char *ySigmaName,
                            int32_t *order[2], long nOrders, long copyParameters)
{
  char buffer[SDDS_MAXLINE], buffer1[SDDS_MAXLINE];
  char *ySymbol, *yUnits, **coefUnits;
  long i;

  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sdds2dpfit output", output) ||
      !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xName[0], NULL) ||
      !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xName[1], NULL) ||
      !SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName, NULL) ||
      SDDS_GetColumnInformation(SDDSout, "symbol", &ySymbol, SDDS_GET_BY_NAME, yName)!=SDDS_STRING ||
      SDDS_GetColumnInformation(SDDSout, "units", &yUnits, SDDS_GET_BY_NAME, yName)!=SDDS_STRING ||
      (ySigmaName && !SDDS_TransferColumnDefinition(SDDSout, SDDSin, ySigmaName, NULL))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (!ySymbol || SDDS_StringIsBlank(ySymbol))
    ySymbol = yName;
  if (SDDS_NumberOfErrors()) 
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

  ix[0] = SDDS_GetColumnIndex(SDDSout, xName[0]);
  ix[1] = SDDS_GetColumnIndex(SDDSout, xName[1]);
  iy = SDDS_GetColumnIndex(SDDSout, yName);
  iySigma = -1;
  if (ySigmaName)
    iySigma = SDDS_GetColumnIndex(SDDSout, ySigmaName);

  sprintf(buffer, "%sFit", yName);
  sprintf(buffer1, "Fit[%s]", ySymbol);
  if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName, buffer) ||
      !SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer1, SDDS_SET_BY_NAME, buffer))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  if ((iFit = SDDS_GetColumnIndex(SDDSout, buffer))<0)
    SDDS_Bomb("unable to get index of just-defined fit output column");

  sprintf(buffer, "%sResidual", yName);
  sprintf(buffer1, "Residual[%s]", ySymbol);
  if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName, buffer) ||
      !SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer1, SDDS_SET_BY_NAME, buffer))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  if (!(iResidual = SDDS_GetColumnIndex(SDDSout, buffer)))
    SDDS_Bomb("unable to get index of just-defined residual output column");

  if (!(coefUnits=makeCoefficientUnits(SDDSout, xName, yName, order, nOrders)))
    SDDS_Bomb("unable to make coefficient units");

  if ((iRmsResidual=SDDS_DefineParameter(SDDSout, "RmsResidual", NULL, yUnits, NULL, NULL, SDDS_DOUBLE, NULL))<0 ||
      (iChiSqr=SDDS_DefineParameter(SDDSout, "ReducedChiSquared", NULL, NULL, NULL, NULL, SDDS_DOUBLE, NULL))<0 ||
      (iConditionNumber=SDDS_DefineParameter(SDDSout, "ConditionNumber", NULL, NULL, NULL, NULL, SDDS_DOUBLE, NULL))<0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

  if ((iFitIsValid=SDDS_DefineParameter(SDDSout, "FitIsValid", NULL, NULL, NULL, NULL,
                                        SDDS_CHARACTER, NULL))<0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

  if ((iTerms=SDDS_DefineParameter(SDDSout, "Terms", NULL, NULL, "Number of terms in fit", NULL,
                                   SDDS_LONG, NULL))<0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

  iTerm = tmalloc(sizeof(*iTerm)*nOrders);
  for (i=0; i<nOrders; i++) {
    char s[16384];
    snprintf(s, 16384, "Coefficient_%02ld_%02ld", (long)order[0][i], (long)order[1][i]);
    iTerm[i] = SDDS_DefineParameter(SDDSout, s, s, coefUnits[i], NULL, NULL, SDDS_DOUBLE, NULL);
  }
  if (SDDS_NumberOfErrors()) 
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

  if (copyParameters && !SDDS_TransferAllParameterDefinitions(SDDSout, SDDSin, 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  
  if (!SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

  return coefUnits;
}

char **makeCoefficientUnits(SDDS_DATASET *SDDSout, char *xName[2], char *yName, 
                            int32_t *order[2], long nOrders)
{
  char *xUnits[2], *yUnits;
  char **coefUnits = NULL;
  long i;
  
  if (!SDDS_GetColumnInformation(SDDSout, "units", &xUnits[0], SDDS_GET_BY_NAME, xName[0]) ||
      !SDDS_GetColumnInformation(SDDSout, "units", &xUnits[1], SDDS_GET_BY_NAME, xName[1]) ||
      !SDDS_GetColumnInformation(SDDSout, "units", &yUnits, SDDS_GET_BY_NAME, yName) )
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  
  coefUnits = tmalloc(sizeof(*coefUnits)*nOrders);
  if ((!xUnits[0] || SDDS_StringIsBlank(xUnits[0])) &&
      (!xUnits[1] || SDDS_StringIsBlank(xUnits[1]))) {
    if (!yUnits || SDDS_StringIsBlank(yUnits))
      SDDS_CopyString(&yUnits, "");
    for (i=0; i<nOrders; i++) 
      coefUnits[i] = yUnits;
  } else {
    char buffer[SDDS_MAXLINE];
    if (!yUnits || SDDS_StringIsBlank(yUnits))
      SDDS_CopyString(&yUnits, "1");
    for (i=0; i<nOrders; i++) {
      buffer[0] = 0;
      if ((order[0][i] && xUnits[0] && !SDDS_StringIsBlank(xUnits[0])) &&
          (order[1][i] && xUnits[1] && !SDDS_StringIsBlank(xUnits[1])))
        snprintf(buffer, SDDS_MAXLINE, "%s/(%s^%ld*%s^%ld)", yUnits, xUnits[0], (long)order[0][i], xUnits[1], (long)order[1][i]);
      else if (order[0][i] && xUnits[0] && !SDDS_StringIsBlank(xUnits[0]))
        snprintf(buffer, SDDS_MAXLINE, "%s/%s^%ld", yUnits, xUnits[0], (long)order[0][i]);
      else if (order[1][i] && xUnits[1] && !SDDS_StringIsBlank(xUnits[1]))
        snprintf(buffer, SDDS_MAXLINE, "%s/%s^%ld", yUnits, xUnits[1], (long)order[1][i]);
      else 
        snprintf(buffer, SDDS_MAXLINE, "%s", yUnits);
      SDDS_CopyString(coefUnits+i, buffer);
    }
  }
  return coefUnits;
}

int lsf2dPoly
(
 /* input */
 double *x[2],   /* independent variable values */
 double *y,      /* dependent variable values */
 double *sy,     /* uncertainty values for dependent variable values */
 long points,    /* number of points */
 int32_t *order[2], /* pairs of orders for terms */
 long nOrders,   /* number of pairs of orders for terms */
 /* output  */
 /* main */
 double *coef,   /* polynomial coefficients */
 double *chi,    /* reduced chi squared */
 double *condition, /* condition number from SVD matrix inversion */
 double *diff,   /* array of differences between fit and y values */
 /* evaluation */
 double *xEval[2],
 long nEvalPoints,
 double *yEval
 )
{
  MAT *X, *A, *Y, *K, *Xc;
  double *weight;
  long i, j;
  
  X = matrix_get(points, nOrders);
  Y = matrix_get(points, 1);
  weight = tmalloc(sizeof(*weight)*points);

  for (i=0; i<points; i++) {
    if (sy)
      weight[i] = 1/sqr(sy[i]);
    else
      weight[i] = 1;
    Mij(Y, i, 0) = y[i];
    for (j=0; j<nOrders; j++) 
      Mij(X, i, j) = ipow(x[0][i], order[0][j])*ipow(x[1][i], order[1][j]);
  }

  /* Equation is Y=X*K 
   * A = Inv(X)
   * K = A*Y 
   */
  Xc = matrix_copy(X);
  A = matrix_invert_weight(X, weight, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, condition);
  K = matrix_mult(A, Y);
    
  for (i=0; i<nOrders; i++) 
    coef[i] = Mij(K, i, 0);

  /* evaluate the polynomial at the fit points */
  matrix_free(Y);
  Y = matrix_mult(Xc, K);
  *chi = 0;
  for (i=0; i<points; i++) {
    diff[i] = y[i] - Mij(Y, i, 0);
    *chi += sqr(diff[i])*weight[i];
  }
  if (nOrders<points)
    *chi /= (points-nOrders);
  else
    *chi = -1;

  matrix_free(X);
  matrix_free(Y);
  matrix_free(A);
  
  if (nEvalPoints) {
    X = matrix_get(nEvalPoints, nOrders);
    for (i=0; i<nEvalPoints; i++)
      for (j=0; j<nOrders; j++)
        Mij(X, i, j) = ipow(xEval[0][i], order[0][j])*ipow(xEval[1][i], order[1][j]);
    Y = matrix_mult(X, K);
    for (i=0; i<nEvalPoints; i++)
      yEval[i] = Mij(Y, i, 0);
    matrix_free(X);
    matrix_free(Y);
  }

  matrix_free(K);

  return 1;
}

void setCoefficientData(SDDS_DATASET *SDDSout, double *coef, long nOrders)
{
  long i;
  for (i=0; i<nOrders; i++) {
    if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_INDEX|SDDS_PASS_BY_VALUE,
                                iTerm[i], coef[i], -1))
      SDDS_Bomb("Unable to set coefficient values in main output file");
  }
}

void initializeEvaluationFile(SDDS_DATASET *SDDSeval, char *evalOutputFile, SDDS_DATASET *SDDSin,
                              char *xName[2], char *xEvalName[2], char *yName,  int32_t *order[2], 
                              long nOrders, long copyParameters)
{
  if (!SDDS_InitializeOutput(SDDSeval, SDDS_BINARY, 0, NULL, "sdds2dpfit evaluation output", evalOutputFile) ||
      !SDDS_TransferColumnDefinition(SDDSeval, SDDSin, xName[0], xEvalName[0]) ||
      !SDDS_TransferColumnDefinition(SDDSeval, SDDSin, xName[1], xEvalName[1]) ||
      !SDDS_TransferColumnDefinition(SDDSeval, SDDSin, yName, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  if (copyParameters && !SDDS_TransferAllParameterDefinitions(SDDSeval, SDDSin, 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(SDDSeval))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
}


void writeEvaluationData(SDDS_DATASET *SDDSeval, double *xEval[2], long nEvalPoints, double *yEval, 
                         char *xEvalName[2], char *yName, long copyParameters, SDDS_DATASET *SDDSin)
{
  if (!SDDS_StartPage(SDDSeval, nEvalPoints) ||
      !SDDS_SetColumnFromDoubles(SDDSeval, SDDS_SET_BY_NAME, xEval[0], nEvalPoints, xEvalName[0]) ||
      !SDDS_SetColumnFromDoubles(SDDSeval, SDDS_SET_BY_NAME, xEval[1], nEvalPoints, xEvalName[1]) ||
      !SDDS_SetColumnFromDoubles(SDDSeval, SDDS_SET_BY_NAME, yEval, nEvalPoints, yName) ||
      (copyParameters && !SDDS_CopyParameters(SDDSeval, SDDSin)) ||
      !SDDS_WritePage(SDDSeval))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
}


void initializeCoefficientsFile(SDDS_DATASET *SDDScoef, char *coefficientsFile, SDDS_DATASET *SDDSin, 
                                char **coefUnits, char *xName[2], long nOrders, long copyParameters)
{
  char buffer[SDDS_MAXLINE];
  if (!SDDS_InitializeOutput(SDDScoef, SDDS_BINARY, 0, NULL, "sdds2dpfit coefficient output", coefficientsFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  if (SDDS_DefineColumn(SDDScoef, "CoefficientValue", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)<0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

  snprintf(buffer, SDDS_MAXLINE, "%sPower", xName[0]);
  if (SDDS_DefineColumn(SDDScoef, buffer, NULL, NULL, NULL, NULL, SDDS_LONG, 0)<0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

  snprintf(buffer, SDDS_MAXLINE, "%sPower", xName[1]);
  if (SDDS_DefineColumn(SDDScoef, buffer, NULL, NULL, NULL, NULL, SDDS_LONG, 0)<0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);

  if (copyParameters && !SDDS_TransferAllParameterDefinitions(SDDScoef, SDDSin, 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(SDDScoef))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
}
  
void writeCoefficientData(SDDS_DATASET *SDDScoef, double *coef, int32_t *order[2], long nOrders,
                          long copyParameters, SDDS_DATASET *SDDSin)
{
  if (!SDDS_StartPage(SDDScoef, nOrders) ||
      !SDDS_SetColumnFromDoubles(SDDScoef, SDDS_SET_BY_NAME, coef, nOrders, "CoefficientValue") ||
      !SDDS_SetColumnFromLongs(SDDScoef, SDDS_SET_BY_INDEX, order[0], nOrders, 1) ||
      !SDDS_SetColumnFromLongs(SDDScoef, SDDS_SET_BY_INDEX, order[1], nOrders, 2) ||
      (copyParameters && !SDDS_CopyParameters(SDDScoef, SDDSin)) ||
      !SDDS_WritePage(SDDScoef))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
}

long readEvaluationPoints(char *evalLocationFile, char *xName0, char *xName1, double **xData0, double **xData1)
{
  SDDS_DATASET SDDSin;
  long rows = 0;
  if (!SDDS_InitializeInput(&SDDSin, evalLocationFile) || SDDS_ReadPage(&SDDSin)!=1 || 
      (rows=SDDS_CountRowsOfInterest(&SDDSin))<1)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  if (!(*xData0=SDDS_GetColumnInDoubles(&SDDSin, xName0)) ||
      !(*xData1=SDDS_GetColumnInDoubles(&SDDSin, xName1)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  return rows;
}
