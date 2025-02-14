/**
 * @file sddssplinefit.c
 * @brief Performs nth-order spline least squares fitting for SDDS files.
 *
 * @details
 * This program fits splines to data contained in SDDS (Self Describing Data Sets) files. It allows for
 * various configurations such as specifying the order of the spline, the number of coefficients or breakpoints,
 * handling of sigma values, and more. The program processes SDDS files and outputs the fitted results
 * along with optional diagnostics or evaluation.
 *
 * @section Usage
 * ```
 * sddssplinefit <inputfile> <outputfile>
 *               [-pipe=[input][,output]]
 *                -independent=<xName>
 *                -dependent=<yName1-wildcard>[,<yName2-wildcard>...]
 *               [-sigmaIndependent=<xSigma>]
 *               [-sigmaDependent=<ySigmaFormatString>]
 *               [-order=<number>]
 *               [-coefficients=<number>]
 *               [-breakpoints=<number>]
 *               [-xOffset=<value>]
 *               [-xFactor=<value>]
 *               [-sigmas=<value>,{absolute|fractional}]
 *               [-modifySigmas]
 *               [-generateSigmas[={keepLargest,keepSmallest}]]
 *               [-sparse=<interval>]
 *               [-range=<lower>,<upper>[,fitOnly]]
 *               [-normalize[=<termNumber>]]
 *               [-verbose]
 *               [-evaluate=<filename>[,begin=<value>][,end=<value>][,number=<integer>][,derivatives=<order>][,basis]]
 *               [-infoFile=<filename>]
 *               [-copyParameters]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-independent`                        | Specifies the name of the independent data column to use.                            |
 * | `-dependent`                          | Specifies the names of dependent data columns to use, supporting wildcards.          |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Uses pipes for input/output data flow.                                      |
 * | `-sigmaIndependent`                   | Specifies the name of the independent sigma column.                                   |
 * | `-sigmaDependent`                     | Specifies a format string for dependent sigma column names (e.g., `%sSigma`).         |
 * | `-order`                              | Specifies the order of the spline. Default is 4.                                     |
 * | `-coefficients`                       | Defines the number of coefficients.                                                  |
 * | `-breakpoints`                        | Sets the number of breakpoints.                                                      |
 * | `-xOffset`                            | Specifies the desired value of x to fit about.                                       |
 * | `-xFactor`                            | Specifies the factor by which to multiply x values before fitting.                   |
 * | `-sigmas`                             | Specifies absolute or fractional sigma for all points.                               |
 * | `-modifySigmas`                       | Modifies the y sigmas using the x sigmas and an initial fit.                         |
 * | `-generateSigmas`                     | Generates y sigmas from the RMS deviation of an initial fit.                         |
 * | `-sparse`                             | Specifies an interval at which to sample data.                                       |
 * | `-range`                              | Defines the range of the independent variable for fitting and evaluation.            |
 * | `-normalize`                          | Normalizes so that the specified term is unity.                                      |
 * | `-verbose`                            | Enables verbose output for additional information.                                   |
 * | `-evaluate`                           | Evaluates the spline fit and optionally computes derivatives or basis functions.     |
 * | `-infoFile`                           | Specifies a file to output fit information.                                          |
 * | `-copyParameters`                     | Copies parameters from the input file to the output file.                            |
 *
 * @subsection Incompatibilities
 *   - `-coefficients` is incompatible with:
 *     - `-breakpoints`
 *   - For `-modifySigmas`:
 *     - Requires `-sigmaIndependent`
 *   - For `-evaluate`:
 *     - Parameters must include valid range and number of evaluation points.
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
 * L. Emery, M. Borland, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "SDDSutils.h"
#include "scan.h"

#include <gsl/gsl_bspline.h>
#include <gsl/gsl_multifit.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_version.h>

void makeSubstitutions(char *buffer1, char *buffer2, char *template, char *nameRoot, char *symbolRoot, char *xName, char *xSymbol);
char *changeInformation(SDDS_DATASET *SDDSout, char *name, char *nameRoot, char *symbolRoot, char *xName, char *xSymbol, char **template, char *newUnits);
char **makeCoefficientUnits(SDDS_DATASET *SDDSout, char *xName, char *yName, long int order, long coeffs);
void initializeOutputFile(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSoutInfo, char *output, char *outputInfo,
                          SDDS_DATASET *SDDSin, char *input, char *xName, char **yNames, char *xSigmaName,
                          char **ySigmaNames, long sigmasValid, long int order, long coeffs, long breaks,
                          long numCols, long copyParameters);
void checkInputFile(SDDS_DATASET *SDDSin, char *xName, char **yNames, char *xSigmaName, char **ySigmaNames, long numYNames);
long coefficient_index(long int order, long coeffs, long order_of_interest);

char **ResolveColumnNames(SDDS_DATASET *SDDSin, char **wildcardList, long length, int32_t *numYNames);
char **GenerateYSigmaNames(char *controlString, char **yNames, long numYNames);
void RemoveElementFromStringArray(char **array, long index, long length);
char **RemoveNonNumericColumnsFromNameArray(SDDS_DATASET *SDDSin, char **columns, int32_t *numColumns);

int print_matrix(FILE *f, const gsl_matrix *m);

/* Enumeration for option types */
enum option_type {
  CLO_DEPENDENT,
  CLO_ORDER,
  CLO_COEFFICIENTS,
  CLO_BREAKPOINTS,
  CLO_REVISEORDERS,
  CLO_XXXXX,
  CLO_MODIFYSIGMAS,
  CLO_SIGMAS,
  CLO_GENERATESIGMAS,
  CLO_RANGE,
  CLO_SPARSE,
  CLO_NORMALIZE,
  CLO_XFACTOR,
  CLO_XOFFSET,
  CLO_VERBOSE,
  CLO_PIPE,
  CLO_EVALUATE,
  CLO_INDEPENDENT,
  CLO_SIGMAINDEPENDENT,
  CLO_SIGMADEPENDENT,
  CLO_INFOFILE,
  CLO_COPYPARAMETERS,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "dependent",
  "order",
  "coefficients",
  "breakpoints",
  "reviseorders",
  "splinebasis",
  "modifysigmas",
  "sigmas",
  "generatesigmas",
  "range",
  "sparse",
  "normalize",
  "xfactor",
  "xoffset",
  "verbose",
  "pipe",
  "evaluate",
  "independent",
  "sigmaindependent",
  "sigmadependent",
  "infofile",
  "copyparameters",
};

char *USAGE = 
  "sddssplinefit [<inputfile>] [<outputfile>]\n"
  "              [-pipe=[input][,output]]\n"
  "               -independent=<xName>\n"
  "               -dependent=<yName1-wildcard>[,<yName2-wildcard>...]\n"
  "              [-sigmaIndependent=<xSigma>]\n"
  "              [-sigmaDependent=<ySigmaFormatString>]\n"
  "              [-order=<number>]\n"
  "              [-coefficients=<number>]\n"
  "              [-breakpoints=<number>]\n"
  "              [-xOffset=<value>]\n"
  "              [-xFactor=<value>]\n"
  "              [-sigmas=<value>,{absolute|fractional}]\n"
  "              [-modifySigmas]\n"
  "              [-generateSigmas[={keepLargest,keepSmallest}]]\n"
  "              [-sparse=<interval>]\n"
  "              [-range=<lower>,<upper>[,fitOnly]]\n"
  "              [-normalize[=<termNumber>]]\n"
  "              [-verbose]\n"
  "              [-evaluate=<filename>[,begin=<value>][,end=<value>][,number=<integer>][,derivatives=<order>][,basis]]\n"
  "              [-infoFile=<filename>]\n"
  "              [-copyParameters]\n"
  "Program by Louis Emery, started with Michael Borland polynomial fit program (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static char *additional_help = "\n\
sddssplinefit performs spline fits of the form y = SUM(i){ A[i] * B(x-x_offset, i)}, where B(x,i) is the ith basis\n\
spline function evaluated at x. Internally, sddssplinefit computes the A[i] coefficients, writes the fitted y values to the output file,\n\
and estimates the errors in these values.\n";

static char *additional_help2 = "\n\
  -independent           Specify the name of the independent data column to use.\n\
  -dependent             Specify the names of dependent data columns to use, supporting wildcards and separated by commas.\n\
  -sigmaIndependent      Specify the name of the independent sigma values column.\n\
  -sigmaDependent        Specify a printf-style control string to generate dependent sigma column names from independent variable names (e.g., %sSigma).\n\
  -order                 Define the order of the spline. Default is 4.\n\
  -coefficients          Set the number of coefficients. Specify either coefficients or breakpoints, not both.\n\
  -breakpoints           Set the number of breakpoints. Condition enforced: breakpoints = coefficients + 2 - order.\n\
  -xOffset               Define the desired value of x to fit about.\n\
  -xFactor               Define the factor to multiply x values by before fitting.\n\
  -sigmas                Specify absolute or fractional sigma for all points.\n\
  -modifySigmas          Modify the y sigmas using the x sigmas and an initial fit.\n\
  -generateSigmas        Generate y sigmas from the RMS deviation of an initial fit.\n\
                         Optionally keep the sigmas from the data if larger/smaller than the RMS deviation.\n\
  -sparse                Specify an integer interval at which to sample data.\n\
  -range                 Define the range of the independent variable over which to perform the fit and evaluation.\n\
                         If 'fitOnly' is given, the fit is compared to data over the original range.\n\
  -normalize             Normalize so that the specified term is unity.\n\
  -verbose               Enable verbose output for additional information.\n\
  -evaluate              Evaluate the spline fit and optionally compute derivatives and provide basis functions.\n\
  -infoFile              Specify a file to output fit information.\n\
  -copyParameters        Copy parameters from the input file to the output file.\n\n";

#define ABSOLUTE_SIGMAS 0
#define FRACTIONAL_SIGMAS 1
#define N_SIGMAS_OPTIONS 2
char *sigmas_options[N_SIGMAS_OPTIONS] = {"absolute", "fractional"};

#define FLGS_GENERATESIGMAS 1
#define FLGS_KEEPLARGEST 2
#define FLGS_KEEPSMALLEST 4

#define REVPOW_ACTIVE 0x0001
#define REVPOW_VERBOSE 0x0002
/* SDDS indices for output page */
static long iOffset = -1, iOffsetO = -1, iFactor = -1, iFactorO = -1;
static long *iChiSq = NULL, *iChiSqO = NULL, *iRmsResidual = NULL, *iRmsResidualO = NULL, *iSigLevel = NULL, *iSigLevelO = NULL;
static long *iFitIsValid = NULL, *iFitIsValidO = NULL;

/* These column indices are globals, so we don't have to pass them to functions */
static long ix = -1, ixSigma = -1;
static long *iy = NULL, *iySigma = NULL;
static long *iFit = NULL, *iResidual = NULL;

static long *iCoefficient = NULL, *iCoefficientSigma = NULL, *iCoefficientUnits = NULL;

static char *xSymbol, **ySymbols;

#define EVAL_BEGIN_GIVEN 0x0001U
#define EVAL_END_GIVEN 0x0002U
#define EVAL_NUMBER_GIVEN 0x0004U
#define EVAL_DERIVATIVES 0x0008U
#define EVAL_PROVIDEBASIS 0x0010U

#define MAX_Y_SIGMA_NAME_SIZE 1024

typedef struct
{
  char *file;
  long initialized;
  int64_t number, nderiv;
  unsigned long flags;
  double begin, end;
  SDDS_DATASET dataset;
  char ***yDerivName, ***yDerivUnits; /* a matrix of names */
  long *iSpline;                      /* index of spline columns for dataset */
  gsl_bspline_workspace *bw;          /* eases the passing of parameters in function arguments */
} EVAL_PARAMETERS;
void setupEvaluationFile(EVAL_PARAMETERS *evalParameters, char *xName, char **yName, long yNames, SDDS_DATASET *SDDSin);
void makeEvaluationTable(EVAL_PARAMETERS *evalParameters, double *x, int64_t points, gsl_vector *B,
                         gsl_matrix *cov, gsl_vector *c, char *xName, char **yName, long yNames, long iYName, long int order);

int main(int argc, char **argv) {
  double **y = NULL, **yFit = NULL, **sy = NULL, **diff = NULL;
  double *x = NULL, *sx = NULL;
  double xOffset, xScaleFactor;
  double *xOrig = NULL, **yOrig = NULL, **yFitOrig = NULL, *sxOrig = NULL, **syOrig = NULL, **sy0 = NULL;
  long coeffs, breaks, normTerm, ySigmasValid;
  long orderGiven, coeffsGiven, breaksGiven;
  int64_t i, j, points, pointsOrig;
  double sigmas;
  long sigmasMode, sparseInterval;
  double *chi = NULL, xLow, xHigh, *rmsResidual = NULL;
  char *xName = NULL, *yName = NULL, **yNames = NULL, *xSigmaName = NULL;
  char **ySigmaNames = NULL, *ySigmaControlString = NULL;
  char *input = NULL, *output = NULL;
  SDDS_DATASET SDDSin, SDDSout, SDDSoutInfo;
  long *isFit = NULL, iArg, modifySigmas;
  long generateSigmas, verbose, ignoreSigmas;
  long outputInitialized, copyParameters = 0;
  long int order;
  SCANNED_ARG *s_arg;
  double xMin, xMax, revpowThreshold;
  double rms_average(double *d_x, int64_t d_n);
  char *infoFile = NULL;
  unsigned long pipeFlags, reviseOrders;
  EVAL_PARAMETERS evalParameters;
  long rangeFitOnly = 0;

  long colIndex;
  long cloDependentIndex = -1, numDependentItems;
  int32_t numYNames;

  /* spline memory for working on a single column */
  gsl_bspline_workspace *bw;
  gsl_vector *B;
  gsl_vector *c, *yGsl, *wGsl;
  gsl_matrix *X, *cov;
  gsl_multifit_linear_workspace *mw;
  long degreesOfFreedom;
  double totalSumSquare, Rsq;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2 || argc > (3 + N_OPTIONS)) {
    fprintf(stderr, "usage: %s\n", USAGE);
    fprintf(stderr, "%s%s", additional_help, additional_help2);
    exit(EXIT_FAILURE);
  }

  input = output = NULL;
  xName = yName = xSigmaName = ySigmaControlString = NULL;
  yNames = ySigmaNames = NULL;
  numDependentItems = 0;
  modifySigmas = reviseOrders = 0;
  /* 8 independent coefficients, a default that I think is reasonable */
  coeffs = 8;
  xMin = xMax = 0;
  generateSigmas = 0;
  sigmasMode = -1;
  sigmas = 1;
  sparseInterval = 1;
  verbose = ignoreSigmas = 0;
  normTerm = -1;
  xOffset = 0;
  xScaleFactor = 1;
  pipeFlags = 0;
  evalParameters.file = NULL;
  infoFile = NULL;
  orderGiven = 0;
  coeffsGiven = 0;
  breaksGiven = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (s_arg[iArg].arg_type == OPTION) {
      switch (match_string(s_arg[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MODIFYSIGMAS:
        modifySigmas = 1;
        break;
      case CLO_ORDER:
        if (s_arg[iArg].n_items != 2 || sscanf(s_arg[iArg].list[1], "%ld", &order) != 1)
          SDDS_Bomb("invalid -order syntax");
        orderGiven = 1;
        break;
      case CLO_COEFFICIENTS:
        if (s_arg[iArg].n_items != 2 || sscanf(s_arg[iArg].list[1], "%ld", &coeffs) != 1)
          SDDS_Bomb("invalid -coefficients syntax");
        coeffsGiven = 1;
        break;
      case CLO_BREAKPOINTS:
        if (s_arg[iArg].n_items != 2 || sscanf(s_arg[iArg].list[1], "%ld", &breaks) != 1)
          SDDS_Bomb("invalid -breakpoints syntax");
        breaksGiven = 1;
        break;
      case CLO_RANGE:
        rangeFitOnly = 0;
        if ((s_arg[iArg].n_items != 3 && s_arg[iArg].n_items != 4) ||
            1 != sscanf(s_arg[iArg].list[1], "%lf", &xMin) ||
            1 != sscanf(s_arg[iArg].list[2], "%lf", &xMax) ||
            xMin >= xMax)
          SDDS_Bomb("incorrect -range syntax");
        if (s_arg[iArg].n_items == 4) {
          if (strncmp(str_tolower(s_arg[iArg].list[3]), "fitonly", strlen(s_arg[iArg].list[3])) == 0) {
            rangeFitOnly = 1;
          } else
            SDDS_Bomb("incorrect -range syntax");
        }
        break;
      case CLO_GENERATESIGMAS:
        generateSigmas = FLGS_GENERATESIGMAS;
        if (s_arg[iArg].n_items > 1) {
          if (s_arg[iArg].n_items != 2)
            SDDS_Bomb("incorrect -generateSigmas syntax");
          if (strncmp(s_arg[iArg].list[1], "keepsmallest", strlen(s_arg[iArg].list[1])) == 0)
            generateSigmas |= FLGS_KEEPSMALLEST;
          if (strncmp(s_arg[iArg].list[1], "keeplargest", strlen(s_arg[iArg].list[1])) == 0)
            generateSigmas |= FLGS_KEEPLARGEST;
          if ((generateSigmas & FLGS_KEEPSMALLEST) && (generateSigmas & FLGS_KEEPLARGEST))
            SDDS_Bomb("ambiguous -generateSigmas syntax");
        }
        break;
      case CLO_XOFFSET:
        if (s_arg[iArg].n_items != 2 || sscanf(s_arg[iArg].list[1], "%lf", &xOffset) != 1)
          SDDS_Bomb("invalid -xOffset syntax");
        break;
      case CLO_SIGMAS:
        if (s_arg[iArg].n_items != 3)
          SDDS_Bomb("incorrect -sigmas syntax");
        if (sscanf(s_arg[iArg].list[1], "%lf", &sigmas) != 1)
          SDDS_Bomb("couldn't scan value for -sigmas");
        if ((sigmasMode = match_string(s_arg[iArg].list[2], sigmas_options, N_SIGMAS_OPTIONS, 0)) < 0)
          SDDS_Bomb("unrecognized -sigmas mode");
        break;
      case CLO_SPARSE:
        if (s_arg[iArg].n_items != 2)
          SDDS_Bomb("incorrect -sparse syntax");
        if (sscanf(s_arg[iArg].list[1], "%ld", &sparseInterval) != 1)
          SDDS_Bomb("couldn't scan value for -sparse");
        if (sparseInterval < 1)
          SDDS_Bomb("invalid -sparse value");
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_NORMALIZE:
        normTerm = 0;
        if (s_arg[iArg].n_items > 2 ||
            (s_arg[iArg].n_items == 2 && sscanf(s_arg[iArg].list[1], "%ld", &normTerm) != 1) ||
            normTerm < 0)
          SDDS_Bomb("invalid -normalize syntax");
        break;
      case CLO_REVISEORDERS:
        revpowThreshold = 0.1;
        s_arg[iArg].n_items -= 1;
        if (!scanItemList(&reviseOrders, s_arg[iArg].list + 1, &s_arg[iArg].n_items, 0,
                          "threshold", SDDS_DOUBLE, &revpowThreshold, 1, 0,
                          "verbose", -1, NULL, 1, REVPOW_VERBOSE, NULL))
          SDDS_Bomb("invalid -reviseOrders syntax");
        s_arg[iArg].n_items += 1;
        reviseOrders |= REVPOW_ACTIVE;
        revpowThreshold = fabs(revpowThreshold);
        break;
      case CLO_XFACTOR:
        if (s_arg[iArg].n_items != 2 ||
            sscanf(s_arg[iArg].list[1], "%lf", &xScaleFactor) != 1 || xScaleFactor == 0)
          SDDS_Bomb("invalid -xFactor syntax");
        break;
      case CLO_INDEPENDENT:
        if (s_arg[iArg].n_items != 2)
          SDDS_Bomb("invalid -independent syntax");
        xName = s_arg[iArg].list[1];
        break;
      case CLO_DEPENDENT:
        numDependentItems = s_arg[iArg].n_items - 1;
        cloDependentIndex = iArg;
        if (numDependentItems < 1)
          SDDS_Bomb("invalid -dependent syntax");
        break;
      case CLO_SIGMAINDEPENDENT:
        if (s_arg[iArg].n_items != 2)
          SDDS_Bomb("invalid -sigmaIndependent syntax");
        xSigmaName = s_arg[iArg].list[1];
        break;
      case CLO_SIGMADEPENDENT:
        if (s_arg[iArg].n_items != 2)
          SDDS_Bomb("invalid -sigmaDependent syntax");
        ySigmaControlString = s_arg[iArg].list[1];
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[iArg].list + 1, s_arg[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_INFOFILE:
        if (s_arg[iArg].n_items != 2)
          SDDS_Bomb("invalid -infoFile syntax");
        infoFile = s_arg[iArg].list[1];
        break;
      case CLO_EVALUATE:
        if (s_arg[iArg].n_items < 2)
          SDDS_Bomb("invalid -evaluate syntax");
        evalParameters.file = s_arg[iArg].list[1];
        s_arg[iArg].n_items -= 2;
        s_arg[iArg].list += 2;
        evalParameters.begin = 0;
        evalParameters.end = 0;
        evalParameters.nderiv = 0;
        evalParameters.number = 0;
        if (!scanItemList(&evalParameters.flags, s_arg[iArg].list, &s_arg[iArg].n_items, 0,
                          "begin", SDDS_DOUBLE, &evalParameters.begin, 1, EVAL_BEGIN_GIVEN,
                          "end", SDDS_DOUBLE, &evalParameters.end, 1, EVAL_END_GIVEN,
                          "derivatives", SDDS_LONG64, &evalParameters.nderiv, 1, EVAL_DERIVATIVES,
                          "basis", -1, NULL, 0, EVAL_PROVIDEBASIS,
                          "number", SDDS_LONG64, &evalParameters.number, 1, EVAL_NUMBER_GIVEN, NULL))
          SDDS_Bomb("invalid -evaluate syntax");
        s_arg[iArg].n_items += 2;
        s_arg[iArg].list -= 2;
        break;
      case CLO_COPYPARAMETERS:
        copyParameters = 1;
        break;
      default:
        bomb("unknown switch", USAGE);
        break;
      }
    } else {
      if (input == NULL)
        input = s_arg[iArg].list[0];
      else if (output == NULL)
        output = s_arg[iArg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }
  /* your basic spline is order 4 (continuous second derivative) */
  if (!orderGiven)
    order = 4;
  if (!breaksGiven)
    breaks = coeffs + 2 - order;
  if (!coeffsGiven)
    coeffs = breaks - 2 + order;
  if (breaksGiven && coeffsGiven)
    SDDS_Bomb("You must specify only one of breakpoints or coefficients");

  processFilenames("sddssplinefit", &input, &output, pipeFlags, 0, NULL);

  if (!xName || !numDependentItems)
    SDDS_Bomb("you must specify a column name for x and y");
  if (modifySigmas && !xSigmaName)
    SDDS_Bomb("you must specify x sigmas with -modifySigmas");
  if (generateSigmas) {
    if (modifySigmas)
      SDDS_Bomb("you can't specify both -generateSigmas and -modifySigmas");
  }
  if (ySigmaControlString) {
    if (sigmasMode != -1)
      SDDS_Bomb("you can't specify both -sigmas and a y sigma name");
  }
  ySigmasValid = 0;
  if (sigmasMode != -1 || generateSigmas || ySigmaControlString || modifySigmas)
    ySigmasValid = 1;

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  outputInitialized = 0;
  yNames = ResolveColumnNames(&SDDSin, s_arg[cloDependentIndex].list + 1, numDependentItems, &numYNames);
  if (ySigmaControlString != NULL)
    ySigmaNames = GenerateYSigmaNames(ySigmaControlString, yNames, numYNames);

  checkInputFile(&SDDSin, xName, yNames, xSigmaName, ySigmaNames, numYNames);
  sy0 = tmalloc(sizeof(double *) * numYNames);
  y = tmalloc(sizeof(double *) * numYNames);
  yFit = tmalloc(sizeof(double *) * numYNames); /* this array of arrays is not in sddsmpfit */
  sy = tmalloc(sizeof(double *) * numYNames);
  isFit = tmalloc(sizeof(long) * numYNames);
  chi = tmalloc(sizeof(double) * numYNames);
  iCoefficient = tmalloc(sizeof(long) * numYNames);
  iCoefficientSigma = tmalloc(sizeof(long) * numYNames);
  iCoefficientUnits = tmalloc(sizeof(long) * numYNames);

  while (SDDS_ReadPage(&SDDSin) > 0) {
    if ((points = SDDS_CountRowsOfInterest(&SDDSin)) < coeffs) {
      /* probably should emit an empty page here */
      continue;
    }
    if (verbose)
      fprintf(stdout, "number of points %" PRId64 "\n", points);
    if (!(x = SDDS_GetColumnInDoubles(&SDDSin, xName))) {
      fprintf(stderr, "error: unable to read column %s\n", xName);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    for (i = 0; i < numYNames; i++) {
      if (!(y[i] = SDDS_GetColumnInDoubles(&SDDSin, yNames[i]))) {
        fprintf(stderr, "error: unable to read column %s\n", yNames[i]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    sx = NULL;
    if (xSigmaName && !(sx = SDDS_GetColumnInDoubles(&SDDSin, xSigmaName))) {
      fprintf(stderr, "error: unable to read column %s\n", xSigmaName);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    for (colIndex = 0; colIndex < numYNames; colIndex++) {
      sy0[colIndex] = tmalloc(sizeof(double) * points);
      yFit[colIndex] = tmalloc(sizeof(double) * points);
    }

    if (ySigmaNames) {
      for (i = 0; i < numYNames; i++) {
        if (!(sy0[i] = SDDS_GetColumnInDoubles(&SDDSin, ySigmaNames[i]))) {
          fprintf(stderr, "error: unable to read column %s\n", ySigmaNames[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    }

    if (xMin != xMax || sparseInterval != 1) {
      xOrig = tmalloc(sizeof(*xOrig) * points);
      yOrig = tmalloc(sizeof(*yOrig) * numYNames);
      yFitOrig = tmalloc(sizeof(*yFitOrig) * numYNames);
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        if (verbose)
          fprintf(stdout, "Setting up a separate array for range or sparsing for column %s because of range option ...\n", yNames[colIndex]);
        yOrig[colIndex] = tmalloc(sizeof(double) * points);
        yFitOrig[colIndex] = tmalloc(sizeof(double) * points);
      }
      if (sx)
        sxOrig = tmalloc(sizeof(*sxOrig) * points);
      if (ySigmasValid) {
        syOrig = tmalloc(sizeof(*syOrig) * numYNames);
        for (colIndex = 0; colIndex < numYNames; colIndex++)
          syOrig[colIndex] = tmalloc(sizeof(double) * points);
      }
      pointsOrig = points;
      for (i = j = 0; i < points; i++) {
        xOrig[i] = x[i];
        if (sx)
          sxOrig[i] = sx[i];
        for (colIndex = 0; colIndex < numYNames; colIndex++) {
          yOrig[colIndex][i] = y[colIndex][i];
          if (ySigmasValid)
            syOrig[colIndex][i] = sy0[colIndex][i];
        }
      }
      if (xMin != xMax) {
        for (i = j = 0; i < points; i++) {
          if (xOrig[i] <= xMax && xOrig[i] >= xMin) {
            x[j] = xOrig[i];
            for (colIndex = 0; colIndex < numYNames; colIndex++) {
              y[colIndex][j] = yOrig[colIndex][i];
              if (ySigmasValid)
                sy0[colIndex][j] = syOrig[colIndex][i];
            }
            if (sx)
              sx[j] = sxOrig[i];
            j++;
          }
        }
        points = j;
      }
      if (sparseInterval != 1) {
        for (i = j = 0; i < points; i++) {
          if (i % sparseInterval == 0) {
            x[j] = x[i];
            for (colIndex = 0; colIndex < numYNames; colIndex++) {
              y[colIndex][j] = y[colIndex][i];
              if (ySigmasValid)
                sy0[colIndex][j] = sy0[colIndex][i];
            }
            if (sx)
              sx[j] = sx[i];
            j++;
          }
        }
        points = j;
      }
    } else {
      /* normal processing, no ranges or sparsing */
      xOrig = x;
      yOrig = y;
      sxOrig = sx;
      syOrig = sy0;
      pointsOrig = points;
    }

    find_min_max(&xLow, &xHigh, x, points);
    if (verbose)
      fprintf(stdout, "Range: xLow %lf; xHigh %lf; points %" PRId64 "\n", xLow, xHigh, points);
    if (sigmasMode == ABSOLUTE_SIGMAS) {
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        for (i = 0; i < points; i++)
          sy0[colIndex][i] = sigmas;
        if (sy0[colIndex] != syOrig[colIndex])
          for (i = 0; i < pointsOrig; i++)
            syOrig[colIndex][i] = sigmas;
      }
    } else if (sigmasMode == FRACTIONAL_SIGMAS) {
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        for (i = 0; i < points; i++)
          sy0[colIndex][i] = sigmas * fabs(y[colIndex][i]);
        if (sy0[colIndex] != syOrig[colIndex])
          for (i = 0; i < pointsOrig; i++)
            syOrig[colIndex][i] = fabs(yOrig[colIndex][i]) * sigmas;
      }
    }

    if (!ySigmasValid || generateSigmas)
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        for (i = 0; i < points; i++)
          sy0[colIndex][i] = 1;
      }
    else
      for (i = 0; i < points; i++)
        for (colIndex = 0; colIndex < numYNames; colIndex++) {
          if (sy0[colIndex][i] == 0)
            SDDS_Bomb("y sigma = 0 for one or more points.");
        }

    diff = tmalloc(sizeof(*diff) * numYNames);
    sy = tmalloc(sizeof(*sy) * numYNames);
    for (colIndex = 0; colIndex < numYNames; colIndex++) {
      diff[colIndex] = tmalloc(sizeof(double) * points);
      sy[colIndex] = tmalloc(sizeof(double) * points);
    }

    for (i = 0; i < points; i++) {
      for (colIndex = 0; colIndex < numYNames; colIndex++)
        sy[colIndex][i] = sy0[colIndex][i];
    }

    /* this seems the places when things really start */

    /* allocate a cubic bspline workspace (k = 4) */
    /* k is order of spline; cubic infers k=4; breaks are number of splines */
    /* each will be reused for each of the columns. No need to make them an array */
    bw = gsl_bspline_alloc(order, breaks);
    B = gsl_vector_alloc(coeffs); /* coeffs are the number of linear,
                                     quadratic and cubic coeff on the
                                     splines, say for k=4 */
    X = gsl_matrix_alloc(points, coeffs);
    c = gsl_vector_alloc(coeffs);
    yGsl = gsl_vector_alloc(points);
    wGsl = gsl_vector_alloc(points);
    cov = gsl_matrix_alloc(coeffs, coeffs);
    mw = gsl_multifit_linear_alloc(points, coeffs);
    degreesOfFreedom = points - coeffs;
    if (verbose)
      fprintf(stdout, "Order %ld\ncoefficients %ld\nbreak points  %ld\n", order, coeffs, breaks);
    if (generateSigmas || modifySigmas)
      fprintf(stderr, "generate sigmas or modify sigmas are not a feature in spline fitting.\n");

    if (reviseOrders & REVPOW_ACTIVE)
      fprintf(stderr, "revise orders is not a feature in spline fitting.\n");

    if (!outputInitialized) {
      initializeOutputFile(&SDDSout, &SDDSoutInfo, output, infoFile, &SDDSin, input, xName, yNames,
                           xSigmaName, ySigmaNames, ySigmasValid, order, coeffs, breaks, numYNames, copyParameters);
      // free(output);
      outputInitialized = 1;
      /* we also want to setup the evaluation file only once */
      if (evalParameters.file) {
        /* check nderiv against order, repair if necessary */
        if (evalParameters.nderiv >= order) {
          evalParameters.nderiv = order - 1;
          if (verbose)
            fprintf(stderr, "Spline derivative order reduced to %" PRId64 " (i.e. order - 1)\n", evalParameters.nderiv);
        }
        evalParameters.bw = bw;
        setupEvaluationFile(&evalParameters, xName, yNames, numYNames, &SDDSin);
      }
    }

    rmsResidual = tmalloc(sizeof(double) * numYNames);

    if (outputInitialized) {
      if (!SDDS_StartPage(&SDDSout, rangeFitOnly ? pointsOrig : points) ||
          (infoFile && !SDDS_StartPage(&SDDSoutInfo, coeffs)))
        bomb("A", NULL);
      if (copyParameters) {
        if (!SDDS_CopyParameters(&SDDSout, &SDDSin))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (infoFile && !SDDS_CopyParameters(&SDDSoutInfo, &SDDSin))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      /* info file writing has been removed below for now. See sddsmpfit.c to retrieve the original */
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, rangeFitOnly ? xOrig : x, rangeFitOnly ? pointsOrig : points, ix))
        bomb("B", NULL);
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        /* do fit now for each column */
        double Bj;
        for (i = 0; i < points; i++) {
          gsl_vector_set(yGsl, i, y[colIndex][i]);
          /* if there was no sigmaY data given then sy = 1 will be used */
          gsl_vector_set(wGsl, i, 1.0 / ipower(sy[colIndex][i], 2));
        }
        /* use uniform breakpoints on [low, high] */
        /* what if we don't want uniform breakpoints? */
        gsl_bspline_knots_uniform(xLow, xHigh, bw);
        /* alternative is gsl_bspline_knots ( breakpts, bw) where
           breakpts is the gsl vector of breakpoints that is
           supplied by the user */

        /* construct the fit matrix X */
        for (i = 0; i < points; ++i) {
          /* compute B_j(xi) for all j */
          /* B is a vector */
          gsl_bspline_eval(x[i], B, bw);
          /* fill in row i of X */
          for (j = 0; j < coeffs; ++j) {
            Bj = gsl_vector_get(B, j);
            /* X is some sort of large matrix points x coeffs, e.g. 100 x 8 */
            gsl_matrix_set(X, i, j, Bj);
          }
        }
        /* show the matrix X */
        if (verbose == 2) {
          fprintf(stderr, "X matrix %s:\n", yNames[colIndex]);
          print_matrix(stderr, X);
        }
        /* do the fit */
        gsl_multifit_wlinear(X, wGsl, yGsl, c, cov, &chi[colIndex], mw);
        if (verbose)
          fprintf(stdout, "conventionally-defined chi = sum( sqr(diff) * weight):  %e\n", chi[colIndex]);
        /*   c is the answer */
        if (verbose == 2) {
          fprintf(stderr, "Covariance matrix for %s:\n", yNames[colIndex]);
          print_matrix(stderr, cov);
        }
        /* weighted total sum of squares */
        totalSumSquare = gsl_stats_wtss(wGsl->data, 1, yGsl->data, 1, yGsl->size);
        Rsq = 1.0 - chi[colIndex] / totalSumSquare;
        if (verbose)
          fprintf(stdout, "(reduced) chisq/dof = %e, Rsq = %f\n", chi[colIndex] / degreesOfFreedom, Rsq);

        double y_err; /* standard deviation of model at x[i]. Is it useful? */
        for (i = 0; i < points; i++) {
          gsl_bspline_eval(x[i], B, bw);
          /* y = B c is the evaluated function. B must be interesting looking.  */
          gsl_multifit_linear_est(B, c, cov, &yFit[colIndex][i], &y_err);
        }
        if (rangeFitOnly) {
          for (i = 0; i < pointsOrig; i++) {
            diff[colIndex][i] = yOrig[colIndex][i] - yFitOrig[colIndex][i];
          }
          rmsResidual[colIndex] = rms_average(diff[colIndex], points);
          /* the index iFit refers to the fit data column corresponding to colIndex */
          if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, yOrig[colIndex], pointsOrig, iy[colIndex]) ||
              !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, yFitOrig[colIndex], pointsOrig, iFit[colIndex]) ||
              !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, diff[colIndex], pointsOrig, iResidual[colIndex]))
            bomb("C", NULL);
        } else {
          for (i = 0; i < points; i++) {
            diff[colIndex][i] = y[colIndex][i] - yFit[colIndex][i];
          }
          rmsResidual[colIndex] = rms_average(diff[colIndex], points);
          /* the index iFit refers to the fit data column corresponding to colIndex */
          if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, y[colIndex], points, iy[colIndex]) ||
              !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, yFit[colIndex], points, iFit[colIndex]) ||
              !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, diff[colIndex], points, iResidual[colIndex]))
            bomb("C", NULL);
        }
        if (infoFile)
          if (!SDDS_SetColumnFromDoubles(&SDDSoutInfo, SDDS_SET_BY_INDEX, c->data, coeffs, iCoefficient[colIndex]))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if (evalParameters.file) {
          evalParameters.bw = bw;
          makeEvaluationTable(&evalParameters, x, points, B, cov, c, xName, yNames, numYNames, colIndex, order);
        }
      }

      if (ixSigma != -1 &&
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, rangeFitOnly ? sxOrig : sx, rangeFitOnly ? pointsOrig : points, ixSigma))
        bomb("E", NULL);
      if (infoFile) {
        {
          int32_t *Indices;
          Indices = malloc(sizeof(*Indices) * coeffs);
          for (i = 0; i < coeffs; i++)
            Indices[i] = i;
          if (!SDDS_SetColumn(&SDDSoutInfo, SDDS_SET_BY_NAME, Indices, coeffs, "Index"))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
      }
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        if (ySigmasValid && iySigma[colIndex] != -1 &&
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, rangeFitOnly ? syOrig[colIndex] : sy[colIndex],
                                       rangeFitOnly ? pointsOrig : points, iySigma[colIndex]))
          bomb("F", NULL);

        /* info file has been removed for now. Splines have orders
           but not used the same way as for regular least
           squares of polynomials */
        if (infoFile) {
          if (!SDDS_SetParameters(&SDDSoutInfo, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  iRmsResidual[colIndex], rmsResidual[colIndex],
                                  iChiSq[colIndex], (chi[colIndex] / degreesOfFreedom),
                                  iSigLevel[colIndex], ChiSqrSigLevel(chi[colIndex], points - coeffs),
                                  iOffset, xOffset, iFactor, xScaleFactor, iFitIsValid[colIndex], isFit[colIndex] ? 'y' : 'n', -1))
            bomb("O", NULL);
        }

        /* writing the results for each page */
        if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                iRmsResidualO[colIndex], rmsResidual[colIndex],
                                iChiSqO[colIndex], (chi[colIndex] / degreesOfFreedom),
                                iSigLevelO[colIndex], ChiSqrSigLevel(chi[colIndex], points - coeffs),
                                iOffsetO, xOffset, iFactorO, xScaleFactor, iFitIsValidO[colIndex], isFit[colIndex] ? 'y' : 'n', -1))
          bomb("O", NULL);
      }
      if (!SDDS_WritePage(&SDDSout) || (infoFile && !SDDS_WritePage(&SDDSoutInfo)))
        bomb("O", NULL);
    }

    /* this is the end of the page, the wrap-up before going to the next page. */
    if (xOrig != x)
      free(xOrig);
    if (sxOrig != sx)
      free(sxOrig);
    free(x);
    free(sx);
    for (colIndex = 0; colIndex < numYNames; colIndex++) {
      free(diff[colIndex]);
      free(sy[colIndex]);
      if (yOrig[colIndex] != y[colIndex])
        free(yOrig[colIndex]);
      if (syOrig && sy0 && syOrig[colIndex] != sy0[colIndex])
        free(syOrig[colIndex]);
      free(y[colIndex]);
      if (sy0 && sy0[colIndex])
        free(sy0[colIndex]);
      if (yFit && yFit[colIndex])
        free(yFit[colIndex]);
    }
    gsl_bspline_free(bw);
    gsl_vector_free(B);
    gsl_matrix_free(X);
    gsl_vector_free(yGsl);
    gsl_vector_free(wGsl);
    // gsl_vector_alloc(c);
    gsl_matrix_free(cov);
    gsl_multifit_linear_free(mw);
  }

  if (yFit)
    free(yFit);
  if (outputInitialized) {
    if (!SDDS_Terminate(&SDDSout)) {
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (infoFile) {
      if (!SDDS_Terminate(&SDDSoutInfo)) {
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
    if (evalParameters.file) {
      if (!SDDS_Terminate(&evalParameters.dataset)) {
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
  }
  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  free_scanargs(&s_arg, argc);

  return EXIT_SUCCESS;
}

void RemoveElementFromStringArray(char **array, long index, long length) {
  long lh;

  for (lh = index; lh < length - 1; lh++)
    array[lh] = array[lh + 1];
}

char **RemoveNonNumericColumnsFromNameArray(SDDS_DATASET *SDDSin, char **columns, int32_t *numColumns) {
  long i, numNumericColumns = *numColumns;

  for (i = 0; i < *numColumns; i++) {
    if (SDDS_CheckColumn(SDDSin, columns[i], NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
      printf("Removing %s because not a numeric type.", columns[i]);
      RemoveElementFromStringArray(columns, i, *numColumns);
      numNumericColumns--;
    }
  }

  *numColumns = numNumericColumns;
  return (columns);
}

char **ResolveColumnNames(SDDS_DATASET *SDDSin, char **wildcardList, long length, int32_t *numYNames) {
  char **result;
  long i;

  /* initially set the columns of interest to none, to make SDDS_OR work below */
  SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, "", SDDS_AND);
  for (i = 0; i < length; i++) {
    SDDS_SetColumnsOfInterest(SDDSin, SDDS_MATCH_STRING, wildcardList[i], SDDS_OR);
  }

  if (!(result = SDDS_GetColumnNames(SDDSin, numYNames)) || *numYNames == 0)
    bomb("Error matching columns in ResolveColumnNames: No matches.", NULL);

  result = RemoveNonNumericColumnsFromNameArray(SDDSin, result, numYNames);
  return (result);
}

char **GenerateYSigmaNames(char *controlString, char **yNames, long numYNames) {
  long i, nameLength;
  char **result, sigmaName[MAX_Y_SIGMA_NAME_SIZE];

  result = tmalloc(sizeof(char *) * numYNames);
  for (i = 0; i < numYNames; i++) {
    sprintf(sigmaName, controlString, yNames[i]);
    nameLength = strlen(sigmaName);
    result[i] = tmalloc(sizeof(char) * (nameLength + 1));
    strcpy(result[i], sigmaName);
  }
  return (result);
}

double rms_average(double *x, int64_t n) {
  double sum2;
  int64_t i;

  for (i = sum2 = 0; i < n; i++)
    sum2 += sqr(x[i]);

  return (sqrt(sum2 / n));
}

void checkInputFile(SDDS_DATASET *SDDSin, char *xName, char **yNames, char *xSigmaName, char **ySigmaNames, long numYNames) {
  char *ptr = NULL;
  long i;

  if (!(ptr = SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, xName, NULL)))
    SDDS_Bomb("x column doesn't exist or is nonnumeric");
  free(ptr);

  /* y columns don't need to be checked because located using SDDS_SetColumnsOfInterest */

  ptr = NULL;
  if (xSigmaName && !(ptr = SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, xSigmaName, NULL)))
    SDDS_Bomb("x sigma column doesn't exist or is nonnumeric");
  if (ptr)
    free(ptr);

  if (ySigmaNames) {
    for (i = 0; i < numYNames; i++) {
      ptr = NULL;
      if (!(ptr = SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, ySigmaNames[i], NULL)))
        SDDS_Bomb("y sigma column doesn't exist or is nonnumeric");
      if (ptr)
        free(ptr);
    }
  }
}

void initializeOutputFile(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSoutInfo, char *output, char *outputInfo,
                          SDDS_DATASET *SDDSin, char *input, char *xName, char **yNames, char *xSigmaName,
                          char **ySigmaNames, long sigmasValid, long int order, long coeffs, long breakpoints,
                          long numCols, long copyParameters) {
  char buffer[SDDS_MAXLINE], buffer1[SDDS_MAXLINE], buffer2[SDDS_MAXLINE], buffer3[SDDS_MAXLINE];
  char *xUnits, *yUnits;
  // char ***coefUnits;
  long colIndex;

  /* all array names followed by an 'O' contain the index of the parameter in the main output file; others refer to
     parameters in the infoFile */
  // coefUnits = tmalloc(sizeof(char **) * numCols);
  ySymbols = tmalloc(sizeof(char *) * numCols);
  iChiSq = tmalloc(sizeof(long) * numCols);
  iChiSqO = tmalloc(sizeof(long) * numCols);
  iRmsResidual = tmalloc(sizeof(long) * numCols);
  iRmsResidualO = tmalloc(sizeof(long) * numCols);
  iSigLevel = tmalloc(sizeof(long) * numCols);
  iSigLevelO = tmalloc(sizeof(long) * numCols);
  iFitIsValid = tmalloc(sizeof(long) * numCols);
  iFitIsValidO = tmalloc(sizeof(long) * numCols);
  iy = tmalloc(sizeof(long) * numCols);
  iySigma = tmalloc(sizeof(long) * numCols);
  iFit = tmalloc(sizeof(long) * numCols);
  iResidual = tmalloc(sizeof(long) * numCols);

  for (colIndex = 0; colIndex < numCols; colIndex++) {
    ySymbols[colIndex] = NULL;
    // coefUnits[colIndex] = tmalloc(sizeof(char *) * coeffs);
    iChiSq[colIndex] = -1;
    iChiSqO[colIndex] = -1;
    iRmsResidual[colIndex] = -1;
    iRmsResidualO[colIndex] = -1;
    iSigLevel[colIndex] = -1;
    iSigLevelO[colIndex] = -1;
    iFitIsValid[colIndex] = -1;
    iFitIsValidO[colIndex] = -1;
    iy[colIndex] = -1;
    iySigma[colIndex] = -1;
    iFit[colIndex] = -1;
    iResidual[colIndex] = -1;
  }

  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddssplinefit output: fitted data", output) ||
      !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xName, NULL) ||
      SDDS_GetColumnInformation(SDDSout, "symbol", &xSymbol, SDDS_GET_BY_NAME, xName) != SDDS_STRING ||
      (xSigmaName && !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xSigmaName, NULL)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  for (colIndex = 0; colIndex < numCols; colIndex++) {
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yNames[colIndex], NULL) ||
        SDDS_GetColumnInformation(SDDSout, "symbol", &ySymbols[colIndex], SDDS_GET_BY_NAME, yNames[colIndex]) != SDDS_STRING ||
        (ySigmaNames && !SDDS_TransferColumnDefinition(SDDSout, SDDSin, ySigmaNames[colIndex], NULL)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!xSymbol || SDDS_StringIsBlank(xSymbol))
    xSymbol = xName;
  for (colIndex = 0; colIndex < numCols; colIndex++)
    if (!ySymbols[colIndex] || SDDS_StringIsBlank(ySymbols[colIndex]))
      ySymbols[colIndex] = yNames[colIndex];
  ix = SDDS_GetColumnIndex(SDDSout, xName);
  for (colIndex = 0; colIndex < numCols; colIndex++) {
    iy[colIndex] = SDDS_GetColumnIndex(SDDSout, yNames[colIndex]);
    if (ySigmaNames)
      iySigma[colIndex] = SDDS_GetColumnIndex(SDDSout, ySigmaNames[colIndex]);
  }
  if (xSigmaName)
    ixSigma = SDDS_GetColumnIndex(SDDSout, xSigmaName);
  if (SDDS_NumberOfErrors())
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  for (colIndex = 0; colIndex < numCols; colIndex++) {
    sprintf(buffer, "%sFit", yNames[colIndex]);
    sprintf(buffer1, "Fit[%s]", ySymbols[colIndex]);
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yNames[colIndex], buffer) ||
        !SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer1, SDDS_SET_BY_NAME, buffer))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((iFit[colIndex] = SDDS_GetColumnIndex(SDDSout, buffer)) < 0)
      SDDS_Bomb("unable to get index of just-defined fit output column");

    sprintf(buffer, "%sResidual", yNames[colIndex]);
    sprintf(buffer1, "Residual[%s]", ySymbols[colIndex]);
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yNames[colIndex], buffer) ||
        !SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer1, SDDS_SET_BY_NAME, buffer))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!(iResidual[colIndex] = SDDS_GetColumnIndex(SDDSout, buffer)))
      SDDS_Bomb("unable to get index of just-defined residual output column");

    if (sigmasValid && !ySigmaNames) {
      sprintf(buffer, "%sSigma", yNames[colIndex]);
      if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yNames[colIndex], buffer))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      iySigma[colIndex] = SDDS_GetColumnIndex(SDDSout, buffer);
      if (ySymbols[colIndex] && !SDDS_StringIsBlank(ySymbols[colIndex])) {
        sprintf(buffer1, "Sigma[%s]", ySymbols[colIndex]);
        if (!SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer1, SDDS_SET_BY_NAME, buffer))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }

  if (outputInfo && !SDDS_InitializeOutput(SDDSoutInfo, SDDS_BINARY, 0, NULL, "sddsspline output: fit information", outputInfo))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (outputInfo) {

    if (SDDS_DefineParameter(SDDSoutInfo, "Order", NULL, NULL, "Order of term in fit", NULL, SDDS_LONG, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (SDDS_DefineParameter(SDDSoutInfo, "Coefficients", NULL, NULL, "Number of Coefficients", NULL, SDDS_LONG, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (SDDS_DefineParameter(SDDSoutInfo, "Breakpoints", NULL, NULL, "Number of breakpoints", NULL, SDDS_LONG, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (SDDS_GetColumnInformation(SDDSout, "units", &xUnits, SDDS_GET_BY_NAME, xName) != SDDS_STRING)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    sprintf(buffer, "%sOffset", xName);
    sprintf(buffer1, "Offset of %s for fit", xName);
    if ((iOffset = SDDS_DefineParameter(SDDSoutInfo, buffer, NULL, xUnits, buffer1, NULL, SDDS_DOUBLE, NULL)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    sprintf(buffer, "%sScale", xName);
    sprintf(buffer1, "Scale factor of %s for fit", xName);
    if ((iFactor = SDDS_DefineParameter(SDDSoutInfo, buffer, NULL, xUnits, buffer1, NULL, SDDS_DOUBLE, NULL)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (SDDS_DefineColumn(SDDSoutInfo, "Index", NULL, NULL, "Index of spline coefficients", NULL, SDDS_LONG, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (colIndex = 0; colIndex < numCols; colIndex++) {
      if (SDDS_GetColumnInformation(SDDSout, "units", &yUnits, SDDS_GET_BY_NAME, yNames[colIndex]) != SDDS_STRING)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      sprintf(buffer1, "%sCoefficient", yNames[colIndex]);
      sprintf(buffer2, "%sCoefficientSigma", yNames[colIndex]);

      if (SDDS_DefineColumn(SDDSoutInfo, buffer1, NULL, yUnits, "Coefficient of spline fit", NULL, SDDS_DOUBLE, 0) < 0 ||
          (sigmasValid && SDDS_DefineColumn(SDDSoutInfo, buffer2, "$gs$r$ba$n", "[CoefficientUnits]", "sigma of coefficient of term in fit", NULL, SDDS_DOUBLE, 0) < 0))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      iCoefficient[colIndex] = SDDS_GetColumnIndex(SDDSoutInfo, buffer1); /* this index is used for setting values of that column */
      iCoefficientSigma[colIndex] = SDDS_GetColumnIndex(SDDSoutInfo, buffer2);

      sprintf(buffer1, "%sReducedChiSquared", yNames[colIndex]);
      sprintf(buffer2, "%sRmsResidual", yNames[colIndex]);
      sprintf(buffer3, "%sSignificanceLevel", yNames[colIndex]);

      if ((iChiSq[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer1, "$gh$r$a2$n/(N-M)", NULL,
                                                   "Reduced chi-squared of fit",
                                                   NULL, SDDS_DOUBLE, NULL)) < 0 ||
          SDDS_GetColumnInformation(SDDSout, "units", &yUnits, SDDS_GET_BY_NAME, yNames[colIndex]) != SDDS_STRING ||
          (iRmsResidual[colIndex] =
           SDDS_DefineParameter(SDDSoutInfo, buffer2, "$gs$r$bres$n", yUnits, "RMS residual of fit", NULL, SDDS_DOUBLE, NULL)) < 0 ||
          (iSigLevel[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer3, NULL, NULL, "Probability that data is from fit function", NULL, SDDS_DOUBLE, NULL)) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (yUnits)
        free(yUnits);

      sprintf(buffer, "%sFitIsValid", yNames[colIndex]);
      if ((iFitIsValid[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer, NULL, NULL, NULL, NULL, SDDS_CHARACTER, NULL)) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (SDDS_DefineParameter1(SDDSout, "Order", NULL, NULL, "Order of splines", NULL, SDDS_LONG, &order) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_DefineParameter1(SDDSout, "Coefficients", NULL, NULL, "Number of coeffs in fit", NULL, SDDS_LONG, &coeffs) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_DefineParameter1(SDDSout, "Breakpoints", NULL, NULL, "Number of break points in fit", NULL, SDDS_LONG, &breakpoints) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_GetColumnInformation(SDDSout, "units", &xUnits, SDDS_GET_BY_NAME, xName) != SDDS_STRING)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(buffer, "%sOffset", xName);
  sprintf(buffer1, "Offset of %s for fit", xName);
  if ((iOffsetO = SDDS_DefineParameter(SDDSout, buffer, NULL, xUnits, buffer1, NULL, SDDS_DOUBLE, NULL)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(buffer, "%sScale", xName);
  sprintf(buffer1, "Scale factor of %s for fit", xName);
  if ((iFactorO = SDDS_DefineParameter(SDDSout, buffer, NULL, xUnits, buffer1, NULL, SDDS_DOUBLE, NULL)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  for (colIndex = 0; colIndex < numCols; colIndex++) {

    sprintf(buffer1, "%sReducedChiSquared", yNames[colIndex]);
    sprintf(buffer2, "%sRmsResidual", yNames[colIndex]);
    sprintf(buffer3, "%sSignificanceLevel", yNames[colIndex]);

    if ((iChiSqO[colIndex] = SDDS_DefineParameter(SDDSout, buffer1, "$gh$r$a2$n/(N-M)", NULL,
                                                  "Reduced chi-squared of fit",
                                                  NULL, SDDS_DOUBLE, NULL)) < 0 ||
        SDDS_GetColumnInformation(SDDSout, "units", &yUnits, SDDS_GET_BY_NAME, yNames[colIndex]) != SDDS_STRING ||
        (iRmsResidualO[colIndex] =
         SDDS_DefineParameter(SDDSout, buffer2, "$gs$r$bres$n", yUnits, "RMS residual of fit", NULL, SDDS_DOUBLE, NULL)) < 0 ||
        (iSigLevelO[colIndex] = SDDS_DefineParameter(SDDSout, buffer3, NULL, NULL, "Probability that data is from fit function", NULL, SDDS_DOUBLE, NULL)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (yUnits)
      free(yUnits);

    sprintf(buffer, "%sFitIsValid", yNames[colIndex]);
    if ((iFitIsValidO[colIndex] = SDDS_DefineParameter(SDDSout, buffer, NULL, NULL, NULL, NULL, SDDS_CHARACTER, NULL)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (copyParameters) {
    if (!SDDS_TransferAllParameterDefinitions(SDDSout, SDDSin, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (outputInfo && !SDDS_TransferAllParameterDefinitions(SDDSoutInfo, SDDSin, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if ((outputInfo && !SDDS_WriteLayout(SDDSoutInfo)) || !SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  // return coefUnits;
  return;
}

void setupEvaluationFile(EVAL_PARAMETERS * evalParameters, char *xName, char **yName, long yNames, SDDS_DATASET *SDDSin) {
  long iYName, i, iCoeff, coeffs;
  char *xSymbol, *ySymbol;
  char *mainTemplateFirstDeriv[3] = {"%yNameDeriv", "Derivative w.r.t. %xSymbol of %ySymbol", "d[%ySymbol]/d[%xSymbol]"};
  char *mainTemplate[3];
  char buffer[1024];
  char ***yDerivName, ***yDerivUnits;
  long nderiv, derivOrder;
  long *iSpline;
  gsl_bspline_workspace *bw;
  SDDS_DATASET *SDDSout;
#if GSL_MAJOR_VERSION == 1
  gsl_bspline_deriv_workspace *bdw;
#endif
  SDDSout = &evalParameters->dataset;
  bw = evalParameters->bw;
  coeffs = gsl_bspline_ncoeffs(bw);

  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddsspline output: evaluation of spline fits", evalParameters->file) ||
      !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xName, NULL))
    SDDS_Bomb("Problem setting up evaluation file");
  if (SDDS_GetColumnInformation(SDDSout, "symbol", &xSymbol, SDDS_GET_BY_NAME, xName) != SDDS_STRING) {
    /*      fprintf(stderr, "error: problem getting symbol for column %s\n", xName);*/
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!xSymbol)
    SDDS_CopyString(&xSymbol, xName);

  if (evalParameters->flags & EVAL_PROVIDEBASIS) {
    iSpline = tmalloc(sizeof(*iSpline) * coeffs); /* dataset index of spline column names */
    evalParameters->iSpline = iSpline;
    for (iCoeff = 0; iCoeff < coeffs; iCoeff++) {
      sprintf(buffer, "B%04ld", iCoeff);
      if ((iSpline[iCoeff] = SDDS_DefineColumn(SDDSout, buffer, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (evalParameters->flags & EVAL_DERIVATIVES) {
    nderiv = evalParameters->nderiv;
    yDerivName = tmalloc(sizeof(*yDerivName) * (nderiv + 1));
    evalParameters->yDerivName = yDerivName;
    yDerivUnits = tmalloc(sizeof(*yDerivUnits) * (nderiv + 1));
    evalParameters->yDerivUnits = yDerivUnits;
    for (derivOrder = 0; derivOrder <= nderiv; derivOrder++) {
      yDerivName[derivOrder] = tmalloc(sizeof(**yDerivName) * yNames);
      yDerivUnits[derivOrder] = tmalloc(sizeof(**yDerivUnits) * yNames);
    }
    for (iYName = 0; iYName < yNames; iYName++) {
      if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName[iYName], NULL))
        SDDS_Bomb("Problem setting up evaluation file");
      yDerivName[0][iYName] = yName[iYName];
      /* first derivative is the first one */
      for (derivOrder = 1; derivOrder <= nderiv; derivOrder++) {
        for (i = 0; i < 3; i++) {
          if (derivOrder != 1) {
            switch (i) {
            case 0:
              /* name */
              sprintf(buffer, "%%yNameDeriv%ld", derivOrder);
              break;
            case 1:
              /* description */
              sprintf(buffer, "Derivative %ld w.r.t. %%xSymbol of %%ySymbol", derivOrder);
              break;
            case 2:
              /* symbol */
              sprintf(buffer, "d$a%ld$n[%%ySymbol]/d[%%xSymbol]$a%ld$n", derivOrder, derivOrder);
              break;
            }
            cp_str(&mainTemplate[i], buffer);
          } else {
            mainTemplate[i] = mainTemplateFirstDeriv[i];
          }
        }
        if (SDDS_GetColumnInformation(SDDSout, "symbol", &ySymbol, SDDS_GET_BY_NAME, yName[iYName]) != SDDS_STRING) {
          fprintf(stderr, "error: problem getting symbol for column %s\n", yName[iYName]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!ySymbol || SDDS_StringIsBlank(ySymbol))
          SDDS_CopyString(&ySymbol, yName[iYName]);
        /* I'm using the function changeInformation from sddsderiv which requires an existing column of some kind*/
        if (SDDS_DefineColumn(SDDSout, "placeholderName", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        yDerivUnits[derivOrder][iYName] = (char *)divideColumnUnits(SDDSout, yDerivName[derivOrder - 1][iYName], xName);
        yDerivName[derivOrder][iYName] = (char *)changeInformation(SDDSout, "placeholderName", yDerivName[0][iYName],
                                                                   ySymbol, xName, xSymbol, mainTemplate, yDerivUnits[derivOrder][iYName]);
      }
    }
    if (!SDDS_WriteLayout(SDDSout))
      SDDS_Bomb("Problem setting up evaluation file with derivatives");
  } else {
    for (iYName = 0; iYName < yNames; iYName++) {
      if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName[iYName], NULL))
        SDDS_Bomb("Problem setting up evaluation file");
    }
    if (!SDDS_WriteLayout(SDDSout))
      SDDS_Bomb("Problem setting up evaluation file");
  }
}

/* called for each column, for each page */
void makeEvaluationTable(EVAL_PARAMETERS * evalParameters, double *x, int64_t points,
                         gsl_vector *B, gsl_matrix *cov, gsl_vector *c,
                         char *xName, char **yName, long yNames, long iYName, long int order) {
  static double *xEval = NULL, *yEval = NULL;
  static int64_t maxEvals = 0;
  double delta;
  double yerr;
  int64_t i;
  char ***yDerivName;
  double xi;
  gsl_matrix *dB = NULL;
  long nderiv, coeffs, iCoeff, derivOrder;
  double **yDeriv = NULL;
  double **Bspline;
  long *iSpline;
  gsl_bspline_workspace *bw;
  SDDS_DATASET *SDDSout;
#if GSL_MAJOR_VERSION == 1
  gsl_bspline_deriv_workspace *bdw;
#endif
  SDDSout = &evalParameters->dataset;
  yDerivName = evalParameters->yDerivName;
  iSpline = evalParameters->iSpline;
  bw = evalParameters->bw;
  coeffs = gsl_bspline_ncoeffs(bw);

  if (!(evalParameters->flags & EVAL_BEGIN_GIVEN) || !(evalParameters->flags & EVAL_END_GIVEN)) {
    double min, max;
    find_min_max(&min, &max, x, points);
    if (!(evalParameters->flags & EVAL_BEGIN_GIVEN))
      evalParameters->begin = min;
    if (!(evalParameters->flags & EVAL_END_GIVEN))
      evalParameters->end = max;
  }
  if (!(evalParameters->flags & EVAL_NUMBER_GIVEN))
    evalParameters->number = points;
  if (evalParameters->number > 1)
    delta = (evalParameters->end - evalParameters->begin) / (evalParameters->number - 1);
  else
    delta = 0;

  if (!xEval || maxEvals < evalParameters->number) {
    if (!(xEval = (double *)SDDS_Realloc(xEval, sizeof(*xEval) * evalParameters->number)) ||
        !(yEval = (double *)SDDS_Realloc(yEval, sizeof(*yEval) * evalParameters->number)))
      SDDS_Bomb("allocation failure");
    maxEvals = evalParameters->number;
  }

  Bspline = tmalloc(sizeof(*Bspline) * coeffs);
  /* the allocation and calculation of Bspline is done only on the first column */
  if ((iYName == 0) && (evalParameters->flags & EVAL_PROVIDEBASIS)) {
    for (iCoeff = 0; iCoeff < coeffs; iCoeff++) {
      Bspline[iCoeff] = tmalloc(sizeof(**Bspline) * evalParameters->number);
    }
  }

  if (!(evalParameters->flags & EVAL_DERIVATIVES)) {
    for (i = 0; i < evalParameters->number; i++) {
      xi = evalParameters->begin + i * delta;
      xEval[i] = xi;
      gsl_bspline_eval(xi, B, bw);
      gsl_multifit_linear_est(B, c, cov, &yEval[i], &yerr);
      if ((iYName == 0) && (evalParameters->flags & EVAL_PROVIDEBASIS)) {
        for (iCoeff = 0; iCoeff < coeffs; iCoeff++) {
          Bspline[iCoeff][i] = gsl_vector_get(B, iCoeff);
        }
      }
    }
    /* Oh, this filters so that StartPage and WritePage are run only once. Tricksty */
    /* On the other hand the x values is repeatedly assigned the values until the calls stop */
    if ((iYName == 0 &&
         !SDDS_StartPage(SDDSout, evalParameters->number)) ||
        !SDDS_SetColumnFromDoubles(SDDSout, SDDS_SET_BY_NAME, xEval, evalParameters->number, xName) ||
        !SDDS_SetColumnFromDoubles(SDDSout, SDDS_SET_BY_NAME, yEval, evalParameters->number, yName[iYName]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((iYName == 0) && (evalParameters->flags & EVAL_PROVIDEBASIS)) {
      for (iCoeff = 0; iCoeff < coeffs; iCoeff++)
        if (!SDDS_SetColumnFromDoubles(SDDSout, SDDS_SET_BY_INDEX, Bspline[iCoeff], evalParameters->number, iSpline[iCoeff]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if ((iYName == yNames - 1 && !SDDS_WritePage(SDDSout)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    nderiv = evalParameters->nderiv;
    yDeriv = tmalloc(sizeof(double *) * (nderiv + 1));
    for (derivOrder = 0; derivOrder <= nderiv; derivOrder++) {
      yDeriv[derivOrder] = tmalloc(sizeof(double) * evalParameters->number);
    }
    dB = gsl_matrix_alloc(coeffs, nderiv + 1);
    for (i = 0; i < evalParameters->number; i++) {
      xi = evalParameters->begin + i * delta;
      xEval[i] = xi;
      gsl_bspline_eval(xi, B, bw);
      gsl_multifit_linear_est(B, c, cov, &yEval[i], &yerr);
#if GSL_MAJOR_VERSION == 1
      bdw = gsl_bspline_deriv_alloc(order);
      gsl_bspline_deriv_eval(xi, nderiv, dB, bw, bdw);
      gsl_bspline_deriv_free(bdw);
#else
      gsl_bspline_deriv_eval(xi, nderiv, dB, bw);
#endif
      for (derivOrder = 0; derivOrder <= nderiv; derivOrder++) {
        yDeriv[derivOrder][i] = 0;
        for (iCoeff = 0; iCoeff < coeffs; iCoeff++) {
          yDeriv[derivOrder][i] += gsl_vector_get(c, iCoeff) * gsl_matrix_get(dB, iCoeff, derivOrder);
        }
      }
      if ((iYName == 0) && (evalParameters->flags & EVAL_PROVIDEBASIS)) {
        for (iCoeff = 0; iCoeff < coeffs; iCoeff++) {
          Bspline[iCoeff][i] = gsl_vector_get(B, iCoeff);
        }
      }
    }
    gsl_matrix_free(dB);
    /* Oh, this filters so that StartPage and WritePage are run only once. Tricksty */
    /* On the other hand the x values is repeatedly assigned the values until the calls stop */
    if ((iYName == 0 &&
         !SDDS_StartPage(SDDSout, evalParameters->number)) ||
        !SDDS_SetColumnFromDoubles(SDDSout, SDDS_SET_BY_NAME, xEval, evalParameters->number, xName) ||
        !SDDS_SetColumnFromDoubles(SDDSout, SDDS_SET_BY_NAME, yEval, evalParameters->number, yName[iYName]))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    /* set derivatives */ /* don't need the 0th derivative */
    for (derivOrder = 1; derivOrder <= nderiv; derivOrder++) {
      if (!SDDS_SetColumnFromDoubles(SDDSout, SDDS_SET_BY_NAME, yDeriv[derivOrder], evalParameters->number, yDerivName[derivOrder][iYName]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if ((iYName == 0) && (evalParameters->flags & EVAL_PROVIDEBASIS)) {
      for (iCoeff = 0; iCoeff < coeffs; iCoeff++)
        if (!SDDS_SetColumnFromDoubles(SDDSout, SDDS_SET_BY_INDEX, Bspline[iCoeff], evalParameters->number, iSpline[iCoeff]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if ((iYName == yNames - 1 && !SDDS_WritePage(SDDSout)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (yDeriv) {
      for (derivOrder = 0; derivOrder <= nderiv; derivOrder++) {
        free(yDeriv[derivOrder]);
      }
      free(yDeriv);
    }
  }
  if ((iYName == 0) && (evalParameters->flags & EVAL_PROVIDEBASIS)) {
    for (iCoeff = 0; iCoeff < coeffs; iCoeff++) {
      free(Bspline[iCoeff]);
    }
  }
  if (iYName == 0)
    free(Bspline);
}

char *changeInformation(SDDS_DATASET * SDDSout, char *name, char *nameRoot, char *symbolRoot, char *xName, char *xSymbol, char **template, char *newUnits) {
  char buffer1[SDDS_MAXLINE], buffer2[SDDS_MAXLINE], *ptr;

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

void makeSubstitutions(char *buffer1, char *buffer2, char *template, char *nameRoot, char *symbolRoot, char *xName, char *xSymbol) {
  strcpy(buffer2, template);
  replace_string(buffer1, buffer2, "%ySymbol", symbolRoot);
  replace_string(buffer2, buffer1, "%xSymbol", xSymbol);
  replace_string(buffer1, buffer2, "%yName", nameRoot);
  replace_string(buffer2, buffer1, "%xName", xName);
  strcpy(buffer1, buffer2);
}

int print_matrix(FILE * f, const gsl_matrix *m) {
  int status, n = 0;
  size_t i, j;
  for (i = 0; i < m->size1; i++) {
    for (j = 0; j < m->size2; j++) {
      if ((status = fprintf(f, "%10.6lf ", gsl_matrix_get(m, i, j))) < 0)
        return -1;
      n += status;
    }

    if ((status = fprintf(f, "\n")) < 0)
      return -1;
    n += status;
  }

  return n;
}
