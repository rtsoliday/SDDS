/**
 * @file sddsmpfit.c
 * @brief Performs polynomial least-squares fitting on SDDS files.
 *
 * @details
 * This program reads SDDS (Self Describing Data Sets) files, fits the data using either ordinary or Chebyshev T polynomials,
 * and outputs the results including fitted data, residuals, and fit parameters with their uncertainties.
 * Additional features include normalization, sigma modification, and fit evaluation.
 *
 * The fitting model is:
 * \f[
 * y = \sum_{i=0}^{N-1} A[i] \cdot P(x - x_{offset}, i)
 * \f]
 * where \( P(x, i) \) is the ith basis function, typically \( x^i \), or a Chebyshev polynomial.
 *
 * @section Usage
 * ```
 * sddsmpfit [<inputfile>] [<outputfile>]
 *           [-pipe=[input][,output]]
 *            -independent=<xName>
 *            -dependent=<yname1-wildcard>[,<yname2-wildcard>...]
 *           [-sigmaIndependent=<xSigma>]
 *           [-sigmaDependent=<ySigmaFormatString>]
 *           {
 *            -terms=<number> [-symmetry={none|odd|even}] | 
 *            -orders=<number>[,<number>...] 
 *           }
 *           [-reviseOrders[=threshold=<value>][,verbose]]
 *           [-chebyshev[=convert]]
 *           [-xOffset=<value>] 
 *           [-xFactor=<value>]
 *           [-sigmas=<value>,{absolute|fractional}] 
 *           [-minimumSigma=<value>]
 *           [-modifySigmas] 
 *           [-generateSigmas={keepLargest|keepSmallest}]
 *           [-repeatFits=<integer>]
 *           [-sparse=<interval>] 
 *           [-range=<lower>,<upper>[,fitOnly]]
 *           [-normalize[=<termNumber>]] 
 *           [-verbose]
 *           [-evaluate=<filename>[,begin=<value>][,end=<value>][,number=<integer>]]
 *           [-fitLabelFormat=<sprintf-string>] 
 *           [-infoFile=<filename>]
 *           [-copyParameters]
 * ```
 *
 * @section Options
 * | Required                              | Description                                             |
 * |---------------------------------------|---------------------------------------------------------|
 * | `-independent`                      | Specify the independent data column name.                                            |
 * | `-dependent`                        | Specify dependent data columns, using wildcards if needed.                          |
 *
 * | Optional                              | Description                                                                          |
 * |-------------------------------------|--------------------------------------------------------------------------------------|
 * | `-pipe`                             | Use standard input and/or output.                                                    |
 * | `-terms`                            | Number of terms to include in the fit.                                               |
 * | `-symmetry`                         | Symmetry of the fit about x_offset.                                                  |
 * | `-orders`                           | Specify the polynomial orders to include in the fit.                                 |
 * | `-reviseOrders`                     | Revise the orders to eliminate poorly-determined coefficients.                       |
 * | `-chebyshev`                        | Use Chebyshev T polynomials. Specify `convert` to convert back to ordinary polynomials.|
 * | `-xOffset`                          | Set the x-offset for the fit.                                                        |
 * | `-xFactor`                          | Set a scaling factor for x values.                                                   |
 * | `-sigmas`                           | Set sigma values as absolute or fractional.                                          |
 * | `-minimumSigma`                     | Set a minimum sigma value; smaller values are replaced.                              |
 * | `-modifySigmas`                     | Modify y-sigmas using x-sigmas and the initial fit.                                   |
 * | `-generateSigmas`                   | Generate y-sigmas based on RMS deviation, optionally keeping the largest/smallest sigmas.|
 * | `-repeatFits`                       | Generate coefficient sigmas using repeated fits (bootstrap method)                   |
 * | `-sparse`                           | Sample data at specified intervals.                                                  |
 * | `-range`                            | Fit and evaluate within the specified range.                                         |
 * | `-normalize`                        | Normalize coefficients so that a specific term equals 1.                             |
 * | `-evaluate`                         | Evaluate the fit over a specified range.                                             |
 * | `-fitLabelFormat`                   | Specify a format string for fit labels.                                              |
 * | `-infoFile`                         | Output fit coefficients and statistics to the specified information file.            |
 * | `-copyParameters`                   | Copy parameters from the input to the output SDDS file.                              |
 * | `-verbose`                          | Enable verbose output for debugging or additional information.                       |
 *
 * @subsection Incompatibilities
 * - `-terms` is incompatible with `-orders`.
 * - `-generateSigmas` is incompatible with `-modifySigmas`.
 * - `-sigmas` cannot be used with `-sigmaDependent`.
 * - `-reviseOrders` requires `-generateSigmas`, `-sigmas`, or `-sigmaDependent`.
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
 * M. Borland, Brad Dolin, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

void print_coefs(FILE *fprec, double x_offset, double x_scale, long chebyshev, double *coef, double *s_coef,
                 int32_t *order, long n_terms, double chi, long norm_term, char *prepend);
char **makeCoefficientUnits(SDDS_DATASET *SDDSout, char *xName, char *yName, int32_t *order, long terms);
long setCoefficientData(SDDS_DATASET *SDDSout, double *coef, double *coefSigma, char **coefUnits, long *order, long terms);
char ***initializeOutputFile(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSoutInfo, char *output, char *outputInfo,
                             SDDS_DATASET *SDDSin, char *input, char *xName, char **yNames, char *xSigmaName,
                             char **ySigmaNames, long sigmasValid, int32_t *order, long terms, long isChebyshev,
                             long numCols, long copyParameters, long repeatFits);
void checkInputFile(SDDS_DATASET *SDDSin, char *xName, char **yNames, char *xSigmaName, char **ySigmaNames, long numYNames);
long coefficient_index(int32_t *order, long terms, long order_of_interest);
void makeFitLabel(char *buffer, long bufsize, char *fitLabelFormat, double *coef, double *coefSigma, int32_t *order, long terms, long colIndex);

char **ResolveColumnNames(SDDS_DATASET *SDDSin, char **wildcardList, long length, int32_t *numYNames);
char **GenerateYSigmaNames(char *controlString, char **yNames, long numYNames);
void RemoveElementFromStringArray(char **array, long index, long length);
char **RemoveNonNumericColumnsFromNameArray(SDDS_DATASET *SDDSin, char **columns, int32_t *numColumns);
void compareOriginalToFit(double *x, double *y, double **residual, int64_t points, double *rmsResidual, double *coef, int32_t *order, long terms);

void set_argument_offset(double offset);
void set_argument_scale(double scale);
double tcheby(double x, long n);
double dtcheby(double x, long n);
double ipower(double x, long n);
double dipower(double x, long n);

/* Enumeration for option types */
enum option_type {
  CLO_DEPENDENT,
  CLO_ORDERS,
  CLO_TERMS,
  CLO_SYMMETRY,
  CLO_REVISEORDERS,
  CLO_CHEBYSHEV,
  CLO_MODIFYSIGMAS,
  CLO_SIGMAS,
  CLO_GENERATESIGMAS,
  CLO_RANGE,
  CLO_SPARSE,
  CLO_NORMALIZE,
  CLO_XFACTOR,
  CLO_XOFFSET,
  CLO_VERBOSE,
  CLO_FITLABELFORMAT,
  CLO_PIPE,
  CLO_EVALUATE,
  CLO_INDEPENDENT,
  CLO_SIGMAINDEPENDENT,
  CLO_SIGMADEPENDENT,
  CLO_INFOFILE,
  CLO_COPYPARAMETERS,
  CLO_MINSIGMA,
  CLO_REPEATFITS,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "dependent",
  "orders",
  "terms",
  "symmetry",
  "reviseorders",
  "chebyshev",
  "modifysigmas",
  "sigmas",
  "generatesigmas",
  "range",
  "sparse",
  "normalize",
  "xfactor",
  "xoffset",
  "verbose",
  "fitlabelformat",
  "pipe",
  "evaluate",
  "independent",
  "sigmaindependent",
  "sigmadependent",
  "infofile",
  "copyparameters",
  "minimumsigma",
  "repeatfits"
};

char *USAGE =
    "sddsmpfit [<inputfile>] [<outputfile>]\n"
    "        [-pipe=[input][,output]]\n"
    "         -independent=<xName>\n"
    "         -dependent=<yname1-wildcard>[,<yname2-wildcard>...]\n"
    "        [-sigmaIndependent=<xSigma>]\n"
    "        [-sigmaDependent=<ySigmaFormatString>]\n"
    "        {\n"
    "         -terms=<number> [-symmetry={none|odd|even}] | \n"
    "         -orders=<number>[,<number>...] \n"
    "        }\n"
    "        [-reviseOrders[=threshold=<value>][,verbose]]\n"
    "        [-chebyshev[=convert]]\n"
    "        [-xOffset=<value>] \n"
    "        [-xFactor=<value>]\n"
    "        [-sigmas=<value>,{absolute|fractional}] \n"
    "        [-minimumSigma=<value>]\n"
    "        [-modifySigmas] \n"
    "        [-generateSigmas={keepLargest|keepSmallest}]\n"
    "        [-repeatFits=<integer>]\n"
    "        [-sparse=<interval>] \n"
    "        [-range=<lower>,<upper>[,fitOnly]]\n"
    "        [-normalize[=<termNumber>]] \n"
    "        [-verbose]\n"
    "        [-evaluate=<filename>[,begin=<value>][,end=<value>][,number=<integer>]]\n"
    "        [-fitLabelFormat=<sprintf-string>] \n"
    "        [-infoFile=<filename>]\n"
    "        [-copyParameters]\n"
    "Program by Michael Borland, revised by Brad Dolin.\n"
    "Compiled on " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

static char *additional_help = "\n\
sddsmpfit does fits to the form y = SUM(i){ A[i] *P(x-x_offset, i)}, where P(x,i) is the ith basis\n\
function evaluated at x.  sddsmpfit returns the A[i] and estimates of the errors in the values.\n\
By default P(x,i) = x^i.  One can also select Chebyshev T polynomials as the basis functions.\n\n\
-independent           specify name of independent data column to use.\n\
-dependent             specify names of dependent data columns to use, using wildcards,\n\
                       separated by commas.\n\
-sigmaIndependent      specify name of independent sigma values to use\n\
-sigmaDependent        specify names of dependent sigma values to use by specifying a printf-style control\n\
                       string to generate the names from the independent variable names (e.g., %sSigma)\n\
-terms                 number of terms desired in fit.\n\
-symmetry              symmetry of desired fit about x_offset.\n\
-orders                orders (P[i]) to use in fitting.\n\
-reviseOrders          the orders used in the fit are modified from the specified ones\n\
                       in order eliminate poorly-determined coefficients, based on fitting\n\
                       of the first data page.\n";
static char *additional_help2 = "-chebyshev             use Chebyshev T polynomials (xOffset is set automatically).\n\
                       Giving the `convert' option causes the fit to be written out in\n\
                       terms of ordinary polynomials.\n\
-xOffset               desired value of x to fit about.\n\
-xFactor               desired factor to multiply x values by before fitting.\n\
-sigmas                specify absolute or fractional sigma for all points.\n\
-minimumSigma          specify minimum sigma value. If the value is less than this\n\
                       it is replaced by this value.\n\
-modifySigmas          modify the y sigmas using the x sigmas and an initial fit.\n\
-generateSigmas        generate y sigmas from the rms deviation from an initial fit.\n\
                       optionally keep the sigmas from the data if larger/smaller than rms\n\
                       deviation.\n\
-repeatFits            Perform repeated fits to get the coefficient sigmas (bootstrap method)\n\
-sparse                specify integer interval at which to sample data.\n\
-range                 specify range of independent variable over which to perform fit and evaluation.\n\
                       If 'fitOnly' is given, then fit is compared to data over the original range.\n\
-normalize             normalize so that specified term is unity.\n\
-evaluate              specify evaluation of fit over a selected range of\n\
                       equispaced points.\n\
-fitLabelFormat        Give format string for fit labels.\n\
-infoFile              specify name of optional information file containing coefficients and fit statistics.\n\
-copyParameters        specify that parameters from input should be copied to output.\n\
-verbose               generates extra output that may be useful.\n\n";

#define NO_SYMMETRY 0
#define EVEN_SYMMETRY 1
#define ODD_SYMMETRY 2
#define N_SYMMETRY_OPTIONS 3
char *symmetry_options[N_SYMMETRY_OPTIONS] = {"none", "even", "odd"};

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
static long *iIntercept = NULL, *iInterceptO = NULL, *iInterceptSigma = NULL, *iInterceptSigmaO = NULL;
static long *iSlope = NULL, *iSlopeO = NULL, *iSlopeSigma = NULL, *iSlopeSigmaO = NULL;
static long *iCurvature = NULL, *iCurvatureO = NULL, *iCurvatureSigma = NULL, *iCurvatureSigmaO = NULL;
static long iOffset = -1, iOffsetO = -1, iFactor = -1, iFactorO = -1;
static long *iChiSq = NULL, *iChiSqO = NULL, *iRmsResidual = NULL, *iRmsResidualO = NULL, *iSigLevel = NULL, *iSigLevelO = NULL;
static long *iFitIsValid = NULL, *iFitIsValidO = NULL, *iFitLabel = NULL, *iFitLabelO = NULL, iTerms = -1, iTermsO = -1;

static long ix = -1, ixSigma = -1;
static long *iy = NULL, *iySigma = NULL;
static long *iFit = NULL, *iResidual = NULL;

static long iOrder = -1, *iCoefficient = NULL, *iCoefficientSigma = NULL, *iCoefficientUnits = NULL;

static char *xSymbol, **ySymbols;

#define EVAL_BEGIN_GIVEN 0x0001U
#define EVAL_END_GIVEN 0x0002U
#define EVAL_NUMBER_GIVEN 0x0004U

#define MAX_Y_SIGMA_NAME_SIZE 1024

typedef struct
{
  char *file;
  long initialized;
  int64_t number;
  unsigned long flags;
  double begin, end;
  SDDS_DATASET dataset;
} EVAL_PARAMETERS;
void setupEvaluationFile(EVAL_PARAMETERS *evalParameters, char *xName, char **yName, long yNames, SDDS_DATASET *SDDSin);
void makeEvaluationTable(EVAL_PARAMETERS *evalParameters, double *x, int64_t points, double *coef, int32_t *order,
                         long terms, char *xName, char **yName, long yNames, long iYName);

static double (*basis_fn)(double xa, long ordera);
static double (*basis_dfn)(double xa, long ordera);

int main(int argc, char **argv) {
  double **y = NULL, **sy = NULL, **diff = NULL;
  double *x = NULL, *sx = NULL;
  double xOffset, xScaleFactor;
  double *xOrig = NULL, **yOrig = NULL, *sxOrig = NULL, **syOrig = NULL, **sy0 = NULL;
  long terms, normTerm, ip, ySigmasValid;
  int64_t i, j, points, pointsOrig;
  long symmetry, chebyshev, termsGiven;
  double sigmas, minimumSigma;
  long sigmasMode, sparseInterval;
  double **coef = NULL, **coefSigma = NULL;
  double *chi = NULL, xLow, xHigh, *rmsResidual = NULL;
  char *xName = NULL, *yName = NULL, **yNames = NULL, *xSigmaName = NULL;
  char **ySigmaNames = NULL, *ySigmaControlString = NULL;
  char *input = NULL, *output = NULL;
  SDDS_DATASET SDDSin, SDDSout, SDDSoutInfo;
  long *isFit = NULL, iArg, modifySigmas, termIndex;
  long generateSigmas, verbose, ignoreSigmas;
  long outputInitialized, copyParameters = 0;
  int32_t *order = NULL;
  SCANNED_ARG *s_arg;
  double xMin, xMax, revpowThreshold;
  double rms_average(double *d_x, int64_t d_n);
  char *infoFile = NULL;
  char *fitLabelFormat = "%g";
  static char fitLabelBuffer[SDDS_MAXLINE];
  unsigned long pipeFlags, reviseOrders;
  EVAL_PARAMETERS evalParameters;
  long rangeFitOnly = 0;
  long repeatFits = 0;

  long colIndex;
  long cloDependentIndex = -1, numDependentItems;
  int32_t numYNames;

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
  modifySigmas = reviseOrders = chebyshev = 0;
  order = NULL;
  symmetry = NO_SYMMETRY;
  xMin = xMax = 0;
  generateSigmas = 0;
  sigmasMode = -1;
  sigmas = 1;
  minimumSigma = 0;
  sparseInterval = 1;
  terms = 2;
  verbose = ignoreSigmas = 0;
  normTerm = -1;
  xOffset = 0;
  xScaleFactor = 1;
  basis_fn = ipower;
  basis_dfn = dipower;
  pipeFlags = 0;
  evalParameters.file = NULL;
  infoFile = NULL;
  termsGiven = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (s_arg[iArg].arg_type == OPTION) {
      switch (match_string(s_arg[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_REPEATFITS:
        if (s_arg[iArg].n_items != 2 || sscanf(s_arg[iArg].list[1], "%ld", &repeatFits) != 1 || repeatFits < 1)
          SDDS_Bomb("invalid -repeatFits syntax");
	if (repeatFits<10)
	  SDDS_Bomb("The number of repeats should be at least 10");
        break;
      case CLO_MODIFYSIGMAS:
        modifySigmas = 1;
        break;
      case CLO_ORDERS:
        if (termsGiven)
          SDDS_Bomb("give -order or -terms, not both");
        if (s_arg[iArg].n_items < 2)
          SDDS_Bomb("invalid -orders syntax");
        order = tmalloc(sizeof(*order) * (terms = s_arg[iArg].n_items - 1));
        for (i = 1; i < s_arg[iArg].n_items; i++) {
          if (sscanf(s_arg[iArg].list[i], "%" SCNd32, order + i - 1) != 1)
            SDDS_Bomb("unable to scan order from -orders list");
        }
        break;
      case CLO_RANGE:
        rangeFitOnly = 0;
        if ((s_arg[iArg].n_items != 3 && s_arg[iArg].n_items != 4) || 1 != sscanf(s_arg[iArg].list[1], "%lf", &xMin) || 1 != sscanf(s_arg[iArg].list[2], "%lf", &xMax) || xMin >= xMax)
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
            SDDS_Bomb("incorrect -generateSigmas synax");
          if (strncmp(s_arg[iArg].list[1], "keepsmallest", strlen(s_arg[iArg].list[1])) == 0)
            generateSigmas |= FLGS_KEEPSMALLEST;
          if (strncmp(s_arg[iArg].list[1], "keeplargest", strlen(s_arg[iArg].list[1])) == 0)
            generateSigmas |= FLGS_KEEPLARGEST;
          if ((generateSigmas & FLGS_KEEPSMALLEST) && (generateSigmas & FLGS_KEEPLARGEST))
            SDDS_Bomb("ambiguous -generateSigmas synax");
        }
        break;
      case CLO_TERMS:
        if (order)
          SDDS_Bomb("give -order or -terms, not both");
        if (s_arg[iArg].n_items != 2 || sscanf(s_arg[iArg].list[1], "%ld", &terms) != 1)
          SDDS_Bomb("invalid -terms syntax");
        termsGiven = 1;
        break;
      case CLO_XOFFSET:
        if (s_arg[iArg].n_items != 2 || sscanf(s_arg[iArg].list[1], "%lf", &xOffset) != 1)
          SDDS_Bomb("invalid -xOffset syntax");
        break;
      case CLO_SYMMETRY:
        if (s_arg[iArg].n_items == 2) {
          if ((symmetry = match_string(s_arg[iArg].list[1], symmetry_options, N_SYMMETRY_OPTIONS, 0)) < 0)
            SDDS_Bomb("unknown option used with -symmetry");
        } else
          SDDS_Bomb("incorrect -symmetry syntax");
        break;
      case CLO_SIGMAS:
        if (s_arg[iArg].n_items != 3)
          SDDS_Bomb("incorrect -sigmas syntax");
        if (sscanf(s_arg[iArg].list[1], "%lf", &sigmas) != 1)
          SDDS_Bomb("couldn't scan value for -sigmas");
        if ((sigmasMode = match_string(s_arg[iArg].list[2], sigmas_options, N_SIGMAS_OPTIONS, 0)) < 0)
          SDDS_Bomb("unrecognized -sigmas mode");
        break;
      case CLO_MINSIGMA:
        if (s_arg[iArg].n_items != 2)
          SDDS_Bomb("incorrect -minimumSigma syntax");
        if (sscanf(s_arg[iArg].list[1], "%lf", &minimumSigma) != 1)
          SDDS_Bomb("couldn't scan value for -minimumSigma");
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
        reviseOrders |= REVPOW_ACTIVE;
        revpowThreshold = fabs(revpowThreshold);
        break;
      case CLO_CHEBYSHEV:
        if (s_arg[iArg].n_items > 2 ||
            (s_arg[iArg].n_items == 2 && strncmp(s_arg[iArg].list[1], "convert", strlen(s_arg[iArg].list[1])) != 0))
          SDDS_Bomb("invalid -chebyshev syntax");
        chebyshev = s_arg[iArg].n_items;
        basis_fn = tcheby;
        basis_dfn = dtcheby;
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
      case CLO_FITLABELFORMAT:
        if (s_arg[iArg].n_items != 2)
          SDDS_Bomb("invalid -fitLabelFormat syntax");
        fitLabelFormat = s_arg[iArg].list[1];
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
        if (!scanItemList(&evalParameters.flags, s_arg[iArg].list, &s_arg[iArg].n_items, 0,
                          "begin", SDDS_DOUBLE, &evalParameters.begin, 1, EVAL_BEGIN_GIVEN,
                          "end", SDDS_DOUBLE, &evalParameters.end, 1, EVAL_END_GIVEN,
                          "number", SDDS_LONG64, &evalParameters.number, 1, EVAL_NUMBER_GIVEN, NULL))
          SDDS_Bomb("invalid -evaluate syntax");
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

  processFilenames("sddsmpfit", &input, &output, pipeFlags, 0, NULL);

  if (symmetry && order)
    SDDS_Bomb("can't specify both -symmetry and -orders");
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

  if (normTerm >= 0 && normTerm >= terms)
    SDDS_Bomb("can't normalize to that term--not that many terms");
  if (reviseOrders && !(sigmasMode != -1 || generateSigmas || ySigmaNames))
    SDDS_Bomb("can't use -reviseOrders unless a y sigma or -generateSigmas is given");

  if (symmetry == EVEN_SYMMETRY) {
    order = tmalloc(sizeof(*order) * terms);
    for (i = 0; i < terms; i++)
      order[i] = 2 * i;
  } else if (symmetry == ODD_SYMMETRY) {
    order = tmalloc(sizeof(*order) * terms);
    for (i = 0; i < terms; i++)
      order[i] = 2 * i + 1;
  } else if (!order) {
    order = tmalloc(sizeof(*order) * terms);
    for (i = 0; i < terms; i++)
      order[i] = i;
  }

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  outputInitialized = 0;
  yNames = ResolveColumnNames(&SDDSin, s_arg[cloDependentIndex].list + 1, numDependentItems, &numYNames);
  if (ySigmaControlString != NULL)
    ySigmaNames = GenerateYSigmaNames(ySigmaControlString, yNames, numYNames);

  checkInputFile(&SDDSin, xName, yNames, xSigmaName, ySigmaNames, numYNames);
  sy0 = tmalloc(sizeof(double *) * numYNames);
  y = tmalloc(sizeof(double *) * numYNames);
  sy = tmalloc(sizeof(double *) * numYNames);
  isFit = tmalloc(sizeof(long) * numYNames);
  chi = tmalloc(sizeof(double) * numYNames);
  coef = tmalloc(sizeof(double *) * numYNames);
  coefSigma = tmalloc(sizeof(double *) * numYNames);
  for (colIndex = 0; colIndex < numYNames; colIndex++) {
    coef[colIndex] = tmalloc(sizeof(double) * terms);
    coefSigma[colIndex] = tmalloc(sizeof(double) * terms);
  }
  iCoefficient = tmalloc(sizeof(long) * numYNames);
  iCoefficientSigma = tmalloc(sizeof(long) * numYNames);
  iCoefficientUnits = tmalloc(sizeof(long) * numYNames);

  while (SDDS_ReadPage(&SDDSin) > 0) {
    if ((points = SDDS_CountRowsOfInterest(&SDDSin)) < terms) {
      /* probably should emit an empty page here */
      continue;
    }
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
    for (colIndex = 0; colIndex < numYNames; colIndex++)
      sy0[colIndex] = tmalloc(sizeof(double) * points);
    if (ySigmaNames) {
      for (i = 0; i < numYNames; i++) {
        if (!(sy0[i] = SDDS_GetColumnInDoubles(&SDDSin, ySigmaNames[i]))) {
          fprintf(stderr, "error: unable to read column %s\n", ySigmaNames[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    }

    if (minimumSigma > 0) {
      int64_t j;
      for (i = 0; i < numYNames; i++) {
        for (j = 0; j < points; j++)
          if (sy0[i][j] < minimumSigma)
            sy0[i][j] = minimumSigma;
      }
    }

    if (xMin != xMax || sparseInterval != 1) {
      xOrig = tmalloc(sizeof(*xOrig) * points);
      yOrig = tmalloc(sizeof(*yOrig) * numYNames);
      for (colIndex = 0; colIndex < numYNames; colIndex++)
        yOrig[colIndex] = tmalloc(sizeof(double) * points);
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
      xOrig = x;
      yOrig = y;
      sxOrig = sx;
      syOrig = sy0;
      pointsOrig = points;
    }

    find_min_max(&xLow, &xHigh, x, points);

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

    for (i = 0; i < numYNames; i++) {
      if (minimumSigma > 0) {
        int64_t j;
        for (j = 0; j < points; j++)
          if (sy0[i][j] < minimumSigma)
            sy0[i][j] = minimumSigma;
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

    set_argument_offset(xOffset);
    set_argument_scale(xScaleFactor);
    if (chebyshev) {
      xOffset = (xHigh + xLow) / 2;
      set_argument_offset(xOffset);
      set_argument_scale(xScaleFactor = (xHigh - xLow) / 2);
    }

    if (generateSigmas || modifySigmas) {
      /* do an initial fit */
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        isFit[colIndex] = lsfg(x, y[colIndex], sy[colIndex], points, terms, order, coef[colIndex], coefSigma[colIndex], &chi[colIndex], diff[colIndex], basis_fn);
        if (!isFit[colIndex]) {
          fprintf(stderr, "Column %s: ", yNames[colIndex]);
          SDDS_Bomb("initial fit failed.");
        }
        if (verbose) {
          fprintf(stderr, "Column %s: ", yNames[colIndex]);
          fputs("initial_fit:", stderr);
          print_coefs(stderr, xOffset, xScaleFactor, chebyshev, coef[colIndex], NULL, order, terms, chi[colIndex], normTerm, "");
          fprintf(stderr, "unweighted rms deviation from fit: %21.15le\n", rms_average(diff[colIndex], points));
        }
        if (modifySigmas) {
          if (!ySigmasValid) {
            for (i = 0; i < points; i++)
              sy[colIndex][i] = fabs(eval_sum(basis_dfn, coef[colIndex], order, terms, x[i]) * sx[i]);
          } else
            for (i = 0; i < points; i++) {
              sy[colIndex][i] = sqrt(sqr(sy0[colIndex][i]) + sqr(eval_sum(basis_dfn, coef[colIndex], order, terms, x[i]) * sx[i]));
            }
        }
        if (generateSigmas) {
          double sigma;
          for (i = sigma = 0; i < points; i++) {
            sigma += sqr(diff[colIndex][i]);
          }
          sigma = sqrt(sigma / (points - terms));
          for (i = 0; i < points; i++) {
            if (generateSigmas & FLGS_KEEPSMALLEST) {
              if (sigma < sy[colIndex][i])
                sy[colIndex][i] = sigma;
            } else if (generateSigmas & FLGS_KEEPLARGEST) {
              if (sigma > sy[colIndex][i])
                sy[colIndex][i] = sigma;
            } else {
              sy[colIndex][i] = sigma;
            }
          }
          for (i = 0; i < pointsOrig; i++) {
            if (generateSigmas & FLGS_KEEPSMALLEST) {
              if (sigma < sy0[colIndex][i])
                sy0[colIndex][i] = sigma;
            } else if (generateSigmas & FLGS_KEEPLARGEST) {
              if (sigma > sy0[colIndex][i])
                sy0[colIndex][i] = sigma;
            } else {
              sy0[colIndex][i] = sigma;
            }
          }
        }
      }
    }

    if (reviseOrders & REVPOW_ACTIVE) {
      double bestChi;
      long bestTerms, newBest;
      int32_t *bestOrder;

      bestTerms = terms;
      bestOrder = tmalloc(sizeof(*bestOrder) * bestTerms);
      for (ip = 0; ip < terms; ip++)
        bestOrder[ip] = order[ip];
      /* do a fit */
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        isFit[colIndex] = lsfg(x, y[colIndex], sy[colIndex], points, bestTerms, bestOrder, coef[colIndex], coefSigma[colIndex], &bestChi, diff[colIndex], basis_fn);
        if (!isFit[colIndex]) {
          fprintf(stderr, "Column %s: ", yNames[colIndex]);
          SDDS_Bomb("revise-orders fit failed.");
          if (reviseOrders & REVPOW_VERBOSE) {
            fprintf(stderr, "Column %s: ", yNames[colIndex]);
            fputs("fit to revise orders:", stderr);
            print_coefs(stderr, xOffset, xScaleFactor, chebyshev, coef[colIndex], (ySigmasValid ? coefSigma[colIndex] : NULL), bestOrder, bestTerms, bestChi, normTerm, "");
            fprintf(stderr, "unweighted rms deviation from fit: %21.15le\n", rms_average(diff[colIndex], points));
          }
        }

        do {
          newBest = 0;
          terms = bestTerms - 1;
          for (ip = bestTerms - 1; ip >= 0; ip--) {
            for (i = j = 0; i < bestTerms; i++)
              if (i != ip)
                order[j++] = bestOrder[i];
            isFit[colIndex] = lsfg(x, y[colIndex], sy[colIndex], points, terms, order, coef[colIndex], coefSigma[colIndex], &chi[colIndex], diff[colIndex], basis_fn);
            if (!isFit[colIndex]) {
              fprintf(stderr, "Column %s: ", yNames[colIndex]);
              SDDS_Bomb("revise-orders fit failed.");
            }
            if (reviseOrders & REVPOW_VERBOSE) {
              fprintf(stderr, "Column %s: ", yNames[colIndex]);
              fputs("new trial fit:", stderr);
              print_coefs(stderr, xOffset, xScaleFactor, chebyshev, coef[colIndex], (ySigmasValid ? coefSigma[colIndex] : NULL), order, terms, chi[colIndex], normTerm, "");
              fprintf(stderr, "unweighted rms deviation from fit: %21.15le\n", rms_average(diff[colIndex], points));
            }
            if (chi[colIndex] - bestChi < revpowThreshold) {
              bestChi = chi[colIndex];
              bestTerms = terms;
              newBest = 1;
              for (i = 0; i < terms; i++)
                bestOrder[i] = order[i];
              if (reviseOrders & REVPOW_VERBOSE) {
                fputs("new best fit:", stderr);
                print_coefs(stderr, xOffset, xScaleFactor, chebyshev, coef[colIndex], (ySigmasValid ? coefSigma[colIndex] : NULL), bestOrder, bestTerms, bestChi, normTerm, "");
                fprintf(stderr, "unweighted rms deviation from fit: %21.15le\n", rms_average(diff[colIndex], points));
              }
              break;
            }
          }
          if (bestTerms == 1)
            break;
        } while (newBest);
        terms = bestTerms;
        for (ip = 0; ip < terms; ip++)
          order[ip] = bestOrder[ip];
        free(bestOrder);
        reviseOrders = 0;
      }
    }

    if (!outputInitialized) {
      initializeOutputFile(&SDDSout, &SDDSoutInfo, output, infoFile, &SDDSin, input, xName, yNames, xSigmaName, ySigmaNames, ySigmasValid, order, terms, chebyshev, numYNames, copyParameters, repeatFits);
      free(output);
      outputInitialized = 1;
    }
    if (evalParameters.file)
      setupEvaluationFile(&evalParameters, xName, yNames, numYNames, &SDDSin);

    rmsResidual = tmalloc(sizeof(double) * numYNames);
    for (colIndex = 0; colIndex < numYNames; colIndex++) {
      if (!repeatFits) {
	isFit[colIndex] = lsfg(x, y[colIndex], sy[colIndex], points, terms, order, coef[colIndex], coefSigma[colIndex], &chi[colIndex], diff[colIndex], basis_fn);
      } else {
        double *coefRepeat = tmalloc(sizeof(*coefRepeat) * terms * repeatFits);
        double *coefSigmaRepeat = tmalloc(sizeof(*coefSigmaRepeat) * terms * repeatFits);
        long fitIdx;
        isFit[colIndex] = 1;
	srand(1);
        for (fitIdx = 0; fitIdx < repeatFits; fitIdx++) {
          // Resample indices with replacement (bootstrap)
          int64_t *indices = tmalloc(sizeof(*indices) * points);
          for (i = 0; i < points; i++) indices[i] = rand() % points;
          double *xSample = tmalloc(sizeof(*xSample) * points);
          double *ySample = tmalloc(sizeof(*ySample) * points);
          double *sySample = tmalloc(sizeof(*sySample) * points);
          for (i = 0; i < points; i++) {
            xSample[i] = x[indices[i]];
            ySample[i] = y[colIndex][indices[i]];
            sySample[i] = sy[colIndex][i];
          }
          double chiTmp;
          double *diffTmp = tmalloc(sizeof(*diffTmp) * points);
          int fitOk = lsfg(xSample, ySample, sySample, points, terms, order, coefRepeat + fitIdx * terms, coefSigmaRepeat + fitIdx * terms, &chiTmp, diffTmp, basis_fn);
          free(indices);
          free(xSample);
          free(ySample);
          free(sySample);
          free(diffTmp);
	  isFit[colIndex] *= fitOk;
        }
        // Compute mean and rms for each coefficient
        for (i = 0; i < terms; i++) {
          double sum = 0, sum2 = 0;
          for (j = 0; j < repeatFits; j++) {
            double v = coefRepeat[j * terms + i];
            sum += v;
            sum2 += v * v;
          }
          coef[colIndex][i] = sum / repeatFits;
          coefSigma[colIndex][i] = sqrt(sum2 / repeatFits - (coef[colIndex][i] * coef[colIndex][i]));
        }
        free(coefRepeat);
        free(coefSigmaRepeat);
        // Evaluate the fit for the mean coefficients and populate the diff array (residuals)
	chi[colIndex] = 0;
        for (i = 0; i < points; i++) {
          double fitValue = eval_sum(basis_fn, coef[colIndex], order, terms, x[i]);
          diff[colIndex][i] = fitValue - y[colIndex][i];
	  chi[colIndex] += sqr(diff[colIndex][i]);
        }
	chi[colIndex] /= (points-terms);
      }
      if (isFit[colIndex]) {
        rmsResidual[colIndex] = rms_average(diff[colIndex], points);
        if (verbose) {
          fprintf(stderr, "Column: %s\n", yNames[colIndex]);
          print_coefs(stderr, xOffset, xScaleFactor, chebyshev, coef[colIndex], (ySigmasValid ? coefSigma[colIndex] : NULL), order, terms, chi[colIndex], normTerm, "");
          fprintf(stderr, "unweighted rms deviation from fit: %21.15le\n", rmsResidual[colIndex]);
        }
      } else if (verbose)
        fprintf(stderr, "fit failed for %s.\n", yNames[colIndex]);

      if (evalParameters.file)
        makeEvaluationTable(&evalParameters, x, points, coef[colIndex], order, terms, xName, yNames, numYNames, colIndex);
    }

    if (outputInitialized) {
      if (!SDDS_StartPage(&SDDSout, rangeFitOnly ? pointsOrig : points) ||
          (infoFile && !SDDS_StartPage(&SDDSoutInfo, terms)))
        bomb("A", NULL);
      if (copyParameters) {
        if (!SDDS_CopyParameters(&SDDSout, &SDDSin))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (infoFile && !SDDS_CopyParameters(&SDDSoutInfo, &SDDSin))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, rangeFitOnly ? xOrig : x, rangeFitOnly ? pointsOrig : points, ix) ||
          (infoFile && !SDDS_SetColumnFromLongs(&SDDSoutInfo, SDDS_SET_BY_INDEX, order, terms, iOrder)))
        bomb("B", NULL);
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        if (rangeFitOnly) {
          double *residual, rmsResidual0;
          compareOriginalToFit(xOrig, yOrig[colIndex], &residual, pointsOrig, &rmsResidual0, coef[colIndex], order, terms);
          if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, yOrig[colIndex], pointsOrig, iy[colIndex]) ||
              !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, residual, pointsOrig, iResidual[colIndex]))
            bomb("C", NULL);
          for (i = 0; i < pointsOrig; i++)
            residual[i] = yOrig[colIndex][i] - residual[i]; /* computes fit values from residual and y */
          if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, residual, pointsOrig, iFit[colIndex]))
            bomb("D", NULL);
          free(residual);
        } else {
          for (i = 0; i < points; i++)
            diff[colIndex][i] = -diff[colIndex][i]; /* convert from (Fit-y) to (y-Fit) to get residual */
          if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, y[colIndex], points, iy[colIndex]) ||
              !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, diff[colIndex], points, iResidual[colIndex]))
            bomb("C", NULL);
          for (i = 0; i < points; i++)
            diff[colIndex][i] = y[colIndex][i] - diff[colIndex][i]; /* computes fit values from residual and y */
          if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, diff[colIndex], points, iFit[colIndex]))
            bomb("D", NULL);
        }
      }
      if (ixSigma != -1 &&
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, rangeFitOnly ? sxOrig : sx, rangeFitOnly ? pointsOrig : points, ixSigma))
        bomb("E", NULL);
      for (colIndex = 0; colIndex < numYNames; colIndex++) {
        if (ySigmasValid && iySigma[colIndex] != -1 &&
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, rangeFitOnly ? syOrig[colIndex] : sy[colIndex], rangeFitOnly ? pointsOrig : points, iySigma[colIndex]))
          bomb("F", NULL);

        if (infoFile) {
          termIndex = coefficient_index(order, terms, 0);
          if (iIntercept[colIndex] != -1 &&
              !SDDS_SetParameters(&SDDSoutInfo, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iIntercept[colIndex], coef[colIndex][termIndex], -1))
            bomb("G", NULL);
          if (iInterceptSigma[colIndex] != -1 &&
              !SDDS_SetParameters(&SDDSoutInfo, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iInterceptSigma[colIndex], coefSigma[colIndex][termIndex], -1))
            bomb("H", NULL);

          termIndex = coefficient_index(order, terms, 1);
          if (iSlope[colIndex] != -1 &&
              !SDDS_SetParameters(&SDDSoutInfo, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iSlope[colIndex], coef[colIndex][termIndex], -1))
            bomb("I", NULL);
          if (iSlopeSigma[colIndex] != -1 &&
              !SDDS_SetParameters(&SDDSoutInfo, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iSlopeSigma[colIndex], coefSigma[colIndex][termIndex], -1))
            bomb("J", NULL);

          termIndex = coefficient_index(order, terms, 2);
          if (iCurvature[colIndex] != -1 &&
              !SDDS_SetParameters(&SDDSoutInfo, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iCurvature[colIndex], coef[colIndex][termIndex], -1))
            bomb("K", NULL);
          if (iCurvatureSigma[colIndex] != -1 &&
              !SDDS_SetParameters(&SDDSoutInfo, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iCurvatureSigma[colIndex], coefSigma[colIndex][termIndex], -1))
            bomb("L", NULL);
          if (iFitLabel[colIndex] != -1) {
            makeFitLabel(fitLabelBuffer, SDDS_MAXLINE, fitLabelFormat, coef[colIndex], (ySigmasValid || repeatFits)?coefSigma[colIndex]:NULL, order, terms, colIndex);
            if (!SDDS_SetParameters(&SDDSoutInfo, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iFitLabel[colIndex], fitLabelBuffer, -1))
              bomb("M", NULL);
          }
          if (!SDDS_SetColumnFromDoubles(&SDDSoutInfo, SDDS_SET_BY_INDEX, coef[colIndex], terms, iCoefficient[colIndex]) ||
              (ySigmasValid &&
               !SDDS_SetColumnFromDoubles(&SDDSoutInfo, SDDS_SET_BY_INDEX, coefSigma[colIndex], terms, iCoefficientSigma[colIndex])))
            bomb("N", NULL);
          if (!SDDS_SetParameters(&SDDSoutInfo, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  iRmsResidual[colIndex], rmsResidual[colIndex], iChiSq[colIndex], chi[colIndex],
                                  iTerms, terms, iSigLevel[colIndex], ChiSqrSigLevel(chi[colIndex], points - terms),
                                  iOffset, xOffset, iFactor, xScaleFactor, iFitIsValid[colIndex], isFit[colIndex] ? 'y' : 'n', -1))
            bomb("O", NULL);
        }

        termIndex = coefficient_index(order, terms, 0);
        if (iInterceptO[colIndex] != -1 &&
            !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iInterceptO[colIndex], coef[colIndex][termIndex], -1))
          bomb("G", NULL);
        if (iInterceptSigmaO[colIndex] != -1 &&
            !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iInterceptSigmaO[colIndex], coefSigma[colIndex][termIndex], -1))
          bomb("H", NULL);

        termIndex = coefficient_index(order, terms, 1);
        if (iSlopeO[colIndex] != -1 &&
            !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iSlopeO[colIndex], coef[colIndex][termIndex], -1))
          bomb("I", NULL);
        if (iSlopeSigmaO[colIndex] != -1 &&
            !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iSlopeSigmaO[colIndex], coefSigma[colIndex][termIndex], -1))
          bomb("J", NULL);

        termIndex = coefficient_index(order, terms, 2);
        if (iCurvatureO[colIndex] != -1 &&
            !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iCurvatureO[colIndex], coef[colIndex][termIndex], -1))
          bomb("K", NULL);
        if (iCurvatureSigmaO[colIndex] != -1 &&
            !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iCurvatureSigmaO[colIndex], coefSigma[colIndex][termIndex], -1))
          bomb("L", NULL);
        if (iFitLabelO[colIndex] != -1) {
          makeFitLabel(fitLabelBuffer, SDDS_MAXLINE, fitLabelFormat, coef[colIndex], (ySigmasValid || repeatFits)?coefSigma[colIndex]:NULL, order, terms, colIndex);
          if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iFitLabelO[colIndex], fitLabelBuffer, -1))
            bomb("M", NULL);
        }
        if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                iRmsResidualO[colIndex], rmsResidual[colIndex], iChiSqO[colIndex], chi[colIndex],
                                iTermsO, terms, iSigLevelO[colIndex], ChiSqrSigLevel(chi[colIndex], points - terms),
                                iOffsetO, xOffset, iFactorO, xScaleFactor, iFitIsValidO[colIndex], isFit[colIndex] ? 'y' : 'n', -1))
          bomb("O", NULL);
      }
      if (!SDDS_WritePage(&SDDSout) || (infoFile && !SDDS_WritePage(&SDDSoutInfo)))
        bomb("O", NULL);
    }
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
    }
  }
  return (EXIT_SUCCESS);
}

void print_coefs(FILE * fpo, double xOffset, double xScaleFactor, long chebyshev, double *coef, double *coefSigma,
                 int32_t *order, long terms, double chi, long normTerm, char *prepend) {
  long i;

  if (chebyshev)
    fprintf(fpo, "%s%ld-term Chebyshev T polynomial least-squares fit about x=%21.15le, scaled by %21.15le:\n", prepend, terms, xOffset, xScaleFactor);
  else
    fprintf(fpo, "%s%ld-term polynomial least-squares fit about x=%21.15le:\n", prepend, terms, xOffset);
  if (normTerm >= 0 && terms > normTerm) {
    if (coef[normTerm] != 0)
      fprintf(fpo, "%s  coefficients are normalized with factor %21.15le to make a[%ld]==1\n", prepend, coef[normTerm], (order ? order[normTerm] : normTerm));
    else {
      fprintf(fpo, "%s can't normalize coefficients as requested: a[%ld]==0\n", prepend, (order ? order[normTerm] : normTerm));
      normTerm = -1;
    }
  } else
    normTerm = -1;

  for (i = 0; i < terms; i++) {
    fprintf(fpo, "%sa[%ld] = %21.15le ", prepend, (order ? order[i] : i), (normTerm < 0 ? coef[i] : coef[i] / coef[normTerm]));
    if (coefSigma)
      fprintf(fpo, "+/- %21.15le\n", (normTerm < 0 ? coefSigma[i] : coefSigma[i] / fabs(coef[normTerm])));
    else
      fputc('\n', fpo);
  }
  if (coefSigma)
    fprintf(fpo, "%sreduced chi-squared = %21.15le\n", prepend, chi);
}

void RemoveElementFromStringArray(char **array, long index, long length) {
  long lh;

  for (lh = index; lh < length - 1; lh++)
    array[lh] = array[lh + 1];
}

char **RemoveNonNumericColumnsFromNameArray(SDDS_DATASET * SDDSin, char **columns, int32_t *numColumns) {
  long i, numNumericColumns = *numColumns;

  for (i = 0; i < *numColumns; i++) {
    if (SDDS_CheckColumn(SDDSin, columns[i], NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
      printf("Removing %s because not a numeric type.\n", columns[i]);
      RemoveElementFromStringArray(columns, i, *numColumns);
      numNumericColumns--;
    }
  }

  *numColumns = numNumericColumns;
  return (columns);
}

char **ResolveColumnNames(SDDS_DATASET * SDDSin, char **wildcardList, long length, int32_t *numYNames) {
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

void makeFitLabel(char *buffer, long bufsize, char *fitLabelFormat, double *coef, double *coefSigma, int32_t *order, long terms, long colIndex) {
  long i;
  static char buffer1[SDDS_MAXLINE], buffer2[SDDS_MAXLINE], buffer3[SDDS_MAXLINE];

  sprintf(buffer, "%s = ", ySymbols[colIndex]);
  for (i = 0; i < terms; i++) {
    if (order[i] == 0) {
      sprintf(buffer1, fitLabelFormat, coef[i]);
      if (coefSigma) {
	strcat(buffer1, "($sa$e");
	sprintf(buffer3, fitLabelFormat, coefSigma[i]);
	strcat(buffer1, buffer3);
	strcat(buffer1, ")");
      }
    } else {
      if (coef[i] >= 0) {
        strcpy(buffer1, " +");
        sprintf(buffer1 + 2, fitLabelFormat, coef[i]);
      } else
        sprintf(buffer1, fitLabelFormat, coef[i]);
      if (coefSigma) {
	strcat(buffer1, "($sa$e");
	sprintf(buffer3, fitLabelFormat, coefSigma[i]);
	strcat(buffer1, buffer3);
	strcat(buffer1, ")");
      }
      strcat(buffer1, "*");
      strcat(buffer1, xSymbol);
      if (order[i] > 1) {
        sprintf(buffer2, "$a%" PRId32 "$n", order[i]);
        strcat(buffer1, buffer2);
      }
    }
    if ((long)(strlen(buffer) + strlen(buffer1)) > (long)(0.95 * bufsize)) {
      fprintf(stderr, "buffer overflow making fit label!\n");
      return;
    }
    strcat(buffer, buffer1);
  }
}

double rms_average(double *x, int64_t n) {
  double sum2;
  int64_t i;

  for (i = sum2 = 0; i < n; i++)
    sum2 += sqr(x[i]);

  return (sqrt(sum2 / n));
}

long coefficient_index(int32_t * order, long terms, long order_of_interest) {
  long i;
  for (i = 0; i < terms; i++)
    if (order[i] == order_of_interest)
      return (i);
  return (-1);
}

void checkInputFile(SDDS_DATASET * SDDSin, char *xName, char **yNames, char *xSigmaName, char **ySigmaNames, long numYNames) {
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

char ***initializeOutputFile(SDDS_DATASET * SDDSout, SDDS_DATASET * SDDSoutInfo, char *output, char *outputInfo,
                             SDDS_DATASET *SDDSin, char *input, char *xName, char **yNames, char *xSigmaName,
                             char **ySigmaNames, long sigmasValid, int32_t *order, long terms, long isChebyshev,
                             long numCols, long copyParameters, long repeatFits) {
  char buffer[SDDS_MAXLINE], buffer1[SDDS_MAXLINE], buffer2[SDDS_MAXLINE], buffer3[SDDS_MAXLINE];
  char *xUnits, *yUnits, ***coefUnits;
  long i, colIndex;

  /* all array names followed by an 'O' contain the index of the parameter in the main output file; others refer to
     parameters in the infoFile */
  coefUnits = tmalloc(sizeof(char **) * numCols);
  ySymbols = tmalloc(sizeof(char *) * numCols);
  iIntercept = tmalloc(sizeof(long) * numCols);
  iInterceptO = tmalloc(sizeof(long) * numCols);
  iInterceptSigma = tmalloc(sizeof(long) * numCols);
  iInterceptSigmaO = tmalloc(sizeof(long) * numCols);
  iSlope = tmalloc(sizeof(long) * numCols);
  iSlopeO = tmalloc(sizeof(long) * numCols);
  iSlopeSigma = tmalloc(sizeof(long) * numCols);
  iSlopeSigmaO = tmalloc(sizeof(long) * numCols);
  iCurvature = tmalloc(sizeof(long) * numCols);
  iCurvatureO = tmalloc(sizeof(long) * numCols);
  iCurvatureSigma = tmalloc(sizeof(long) * numCols);
  iCurvatureSigmaO = tmalloc(sizeof(long) * numCols);
  iChiSq = tmalloc(sizeof(long) * numCols);
  iChiSqO = tmalloc(sizeof(long) * numCols);
  iRmsResidual = tmalloc(sizeof(long) * numCols);
  iRmsResidualO = tmalloc(sizeof(long) * numCols);
  iSigLevel = tmalloc(sizeof(long) * numCols);
  iSigLevelO = tmalloc(sizeof(long) * numCols);
  iFitIsValid = tmalloc(sizeof(long) * numCols);
  iFitIsValidO = tmalloc(sizeof(long) * numCols);
  iFitLabel = tmalloc(sizeof(long) * numCols);
  iFitLabelO = tmalloc(sizeof(long) * numCols);
  iy = tmalloc(sizeof(long) * numCols);
  iySigma = tmalloc(sizeof(long) * numCols);
  iFit = tmalloc(sizeof(long) * numCols);
  iResidual = tmalloc(sizeof(long) * numCols);

  for (colIndex = 0; colIndex < numCols; colIndex++) {
    ySymbols[colIndex] = NULL;
    coefUnits[colIndex] = tmalloc(sizeof(char *) * terms);
    iInterceptSigma[colIndex] = -1;
    iInterceptSigmaO[colIndex] = -1;
    iIntercept[colIndex] = -1;
    iInterceptO[colIndex] = -1;
    iInterceptSigma[colIndex] = -1;
    iInterceptSigmaO[colIndex] = -1;
    iSlope[colIndex] = -1;
    iSlopeO[colIndex] = -1;
    iSlopeSigma[colIndex] = -1;
    iSlopeSigmaO[colIndex] = -1;
    iCurvature[colIndex] = -1;
    iCurvatureO[colIndex] = -1;
    iCurvatureSigma[colIndex] = -1;
    iCurvatureSigmaO[colIndex] = -1;
    iChiSq[colIndex] = -1;
    iChiSqO[colIndex] = -1;
    iRmsResidual[colIndex] = -1;
    iRmsResidualO[colIndex] = -1;
    iSigLevel[colIndex] = -1;
    iSigLevelO[colIndex] = -1;
    iFitIsValid[colIndex] = -1;
    iFitIsValidO[colIndex] = -1;
    iFitLabel[colIndex] = -1;
    iFitLabelO[colIndex] = -1;
    iy[colIndex] = -1;
    iySigma[colIndex] = -1;
    iFit[colIndex] = -1;
    iResidual[colIndex] = -1;
  }

  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddsmpfit output: fitted data", output) ||
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

    if (!(coefUnits[colIndex] = makeCoefficientUnits(SDDSout, xName, yNames[colIndex], order, terms)))
      SDDS_Bomb("unable to make coefficient units");
  }

  if (outputInfo && !SDDS_InitializeOutput(SDDSoutInfo, SDDS_BINARY, 0, NULL, "sddsmpfit output: fit information", outputInfo))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (outputInfo) {
    if ((SDDS_DefineColumn(SDDSoutInfo, "Order", NULL, NULL, "Order of term in fit", NULL, SDDS_LONG, 0) < 0) ||
        SDDS_DefineParameter(SDDSoutInfo, "Basis", NULL, NULL, "Function basis for fit", NULL, SDDS_STRING, isChebyshev ? "Chebyshev T polynomials" : "ordinary polynomials") < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if ((iTerms = SDDS_DefineParameter(SDDSoutInfo, "Terms", NULL, NULL, "Number of terms in fit", NULL, SDDS_LONG, NULL)) < 0)
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

    for (colIndex = 0; colIndex < numCols; colIndex++) {

      sprintf(buffer1, "%sCoefficient", yNames[colIndex]);
      sprintf(buffer2, "%sCoefficientSigma", yNames[colIndex]);
      sprintf(buffer3, "%sCoefficientUnits", yNames[colIndex]);

      if (SDDS_DefineColumn(SDDSoutInfo, buffer1, NULL, "[CoefficientUnits]", "Coefficient of term in fit", NULL, SDDS_DOUBLE, 0) < 0 ||
          ((sigmasValid || repeatFits) && SDDS_DefineColumn(SDDSoutInfo, buffer2, "$gs$r$ba$n", "[CoefficientUnits]", "sigma of coefficient of term in fit", NULL, SDDS_DOUBLE, 0) < 0))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      iOrder = SDDS_GetColumnIndex(SDDSoutInfo, "Order");
      iCoefficient[colIndex] = SDDS_GetColumnIndex(SDDSoutInfo, buffer1);
      iCoefficientSigma[colIndex] = SDDS_GetColumnIndex(SDDSoutInfo, buffer2);
      iCoefficientUnits[colIndex] = SDDS_GetColumnIndex(SDDSoutInfo, buffer3);

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

      if (!isChebyshev) {

        sprintf(buffer, "%sSddsmpfitlabel", yNames[colIndex]);
        iFitLabel[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer, NULL, NULL, NULL, NULL, SDDS_STRING, NULL);
        if ((i = coefficient_index(order, terms, 0)) >= 0) {
          sprintf(buffer, "%sIntercept", yNames[colIndex]);
          iIntercept[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer, buffer, coefUnits[colIndex][i], "Intercept of fit", NULL, SDDS_DOUBLE, NULL);
          sprintf(buffer, "%sInterceptSigma", yNames[colIndex]);
          if (sigmasValid || repeatFits)
            iInterceptSigma[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer, buffer, coefUnits[colIndex][i], "Sigma of intercept of fit", NULL, SDDS_DOUBLE, NULL);
        }
        sprintf(buffer, "%sSlope", yNames[colIndex]);
        if ((i = coefficient_index(order, terms, 1)) >= 0) {
          iSlope[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer, buffer, coefUnits[colIndex][i], "Slope of fit", NULL, SDDS_DOUBLE, NULL);
          if (sigmasValid || repeatFits) {
            sprintf(buffer, "%sSlopeSigma", yNames[colIndex]);
            iSlopeSigma[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer, buffer, coefUnits[colIndex][i], "Sigma of slope of fit", NULL, SDDS_DOUBLE, NULL);
          }
        }
        if ((i = coefficient_index(order, terms, 2)) >= 0) {
          sprintf(buffer, "%sCurvature", yNames[colIndex]);
          iCurvature[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer, buffer, coefUnits[colIndex][i], "Curvature of fit", NULL, SDDS_DOUBLE, NULL);
          if (sigmasValid || repeatFits) {
            sprintf(buffer, "%sCurvatureSigma", yNames[colIndex]);
            iCurvatureSigma[colIndex] = SDDS_DefineParameter(SDDSoutInfo, buffer, buffer, coefUnits[colIndex][i], "Sigma of curvature of fit", NULL, SDDS_DOUBLE, NULL);
          }
        }
        if (SDDS_NumberOfErrors())
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }

  if (SDDS_DefineParameter(SDDSout, "Basis", NULL, NULL, "Function basis for fit", NULL, SDDS_STRING, isChebyshev ? "Chebyshev T polynomials" : "ordinary polynomials") < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((iTermsO = SDDS_DefineParameter(SDDSout, "Terms", NULL, NULL, "Number of terms in fit", NULL, SDDS_LONG, NULL)) < 0)
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

    if (!isChebyshev) {
      sprintf(buffer, "%sSddsmpfitlabel", yNames[colIndex]);
      iFitLabelO[colIndex] = SDDS_DefineParameter(SDDSout, buffer, NULL, NULL, NULL, NULL, SDDS_STRING, NULL);
      if ((i = coefficient_index(order, terms, 0)) >= 0) {
        sprintf(buffer, "%sIntercept", yNames[colIndex]);
        iInterceptO[colIndex] = SDDS_DefineParameter(SDDSout, buffer, buffer, coefUnits[colIndex][i], "Intercept of fit", NULL, SDDS_DOUBLE, NULL);
        sprintf(buffer, "%sInterceptSigma", yNames[colIndex]);
        if (sigmasValid || repeatFits)
          iInterceptSigmaO[colIndex] = SDDS_DefineParameter(SDDSout, buffer, buffer, coefUnits[colIndex][i], "Sigma of intercept of fit", NULL, SDDS_DOUBLE, NULL);
      }
      sprintf(buffer, "%sSlope", yNames[colIndex]);
      if ((i = coefficient_index(order, terms, 1)) >= 0) {
        iSlopeO[colIndex] = SDDS_DefineParameter(SDDSout, buffer, buffer, coefUnits[colIndex][i], "Slope of fit", NULL, SDDS_DOUBLE, NULL);
        if (sigmasValid || repeatFits) {
          sprintf(buffer, "%sSlopeSigma", yNames[colIndex]);
          iSlopeSigmaO[colIndex] = SDDS_DefineParameter(SDDSout, buffer, buffer, coefUnits[colIndex][i], "Sigma of slope of fit", NULL, SDDS_DOUBLE, NULL);
        }
      }
      if ((i = coefficient_index(order, terms, 2)) >= 0) {
        sprintf(buffer, "%sCurvature", yNames[colIndex]);
        iCurvatureO[colIndex] = SDDS_DefineParameter(SDDSout, buffer, buffer, coefUnits[colIndex][i], "Curvature of fit", NULL, SDDS_DOUBLE, NULL);
        if (sigmasValid || repeatFits) {
          sprintf(buffer, "%sCurvatureSigma", yNames[colIndex]);
          iCurvatureSigmaO[colIndex] = SDDS_DefineParameter(SDDSout, buffer, buffer, coefUnits[colIndex][i], "Sigma of curvature of fit", NULL, SDDS_DOUBLE, NULL);
        }
      }
      if (SDDS_NumberOfErrors())
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (copyParameters) {
    if (!SDDS_TransferAllParameterDefinitions(SDDSout, SDDSin, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (outputInfo && !SDDS_TransferAllParameterDefinitions(SDDSoutInfo, SDDSin, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if ((outputInfo && !SDDS_WriteLayout(SDDSoutInfo)) || !SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  return coefUnits;
}

char **makeCoefficientUnits(SDDS_DATASET * SDDSout, char *xName, char *yName, int32_t *order, long terms) {
  char *xUnits, *yUnits, buffer[SDDS_MAXLINE];
  char **coefUnits = NULL;
  long i;

  if (!SDDS_GetColumnInformation(SDDSout, "units", &xUnits, SDDS_GET_BY_NAME, xName) ||
      !SDDS_GetColumnInformation(SDDSout, "units", &yUnits, SDDS_GET_BY_NAME, yName))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  coefUnits = tmalloc(sizeof(*coefUnits) * terms);
  if (!xUnits || SDDS_StringIsBlank(xUnits)) {
    if (!yUnits || SDDS_StringIsBlank(yUnits))
      SDDS_CopyString(&yUnits, "");
    for (i = 0; i < terms; i++)
      coefUnits[i] = yUnits;
  } else {
    if (!yUnits || SDDS_StringIsBlank(yUnits))
      SDDS_CopyString(&yUnits, "1");
    for (i = 0; i < terms; i++) {
      if (order[i] == 0) {
        if (strcmp(yUnits, "1") != 0)
          SDDS_CopyString(coefUnits + i, yUnits);
        else
          SDDS_CopyString(coefUnits + i, "");
      } else if (strcmp(xUnits, yUnits) == 0) {
        if (order[i] > 1)
          sprintf(buffer, "1/%s$a%" PRId32 "$n", xUnits, order[i] - 1);
        else
          strcpy(buffer, "");
        SDDS_CopyString(coefUnits + i, buffer);
      } else {
        if (order[i] > 1)
          sprintf(buffer, "%s/%s$a%" PRId32 "$n", yUnits, xUnits, order[i]);
        else
          sprintf(buffer, "%s/%s", yUnits, xUnits);
        SDDS_CopyString(coefUnits + i, buffer);
      }
    }
  }
  return coefUnits;
}

void setupEvaluationFile(EVAL_PARAMETERS * evalParameters, char *xName, char **yName, long yNames, SDDS_DATASET *SDDSin) {
  long i;
  SDDS_DATASET *SDDSout;
  SDDSout = &evalParameters->dataset;
  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddsmpfit output: evaluation of fits", evalParameters->file) ||
      !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xName, NULL))
    SDDS_Bomb("Problem setting up evaluation file");
  for (i = 0; i < yNames; i++)
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName[i], NULL))
      SDDS_Bomb("Problem setting up evaluation file");
  if (!SDDS_WriteLayout(SDDSout))
    SDDS_Bomb("Problem setting up evaluation file");
}

void makeEvaluationTable(EVAL_PARAMETERS * evalParameters, double *x, int64_t points, double *coef, int32_t *order, long terms,
                         char *xName, char **yName, long yNames, long iYName) {
  static double *xEval = NULL, *yEval = NULL;
  static int64_t maxEvals = 0;
  double delta;
  int64_t i;

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

  for (i = 0; i < evalParameters->number; i++) {
    xEval[i] = evalParameters->begin + i * delta;
    yEval[i] = eval_sum(basis_fn, coef, order, terms, xEval[i]);
  }

  if ((iYName == 0 &&
       !SDDS_StartPage(&evalParameters->dataset, evalParameters->number)) ||
      !SDDS_SetColumnFromDoubles(&evalParameters->dataset, SDDS_SET_BY_NAME, xEval, evalParameters->number, xName) ||
      !SDDS_SetColumnFromDoubles(&evalParameters->dataset, SDDS_SET_BY_NAME, yEval, evalParameters->number, yName[iYName]) ||
      (iYName == yNames - 1 && !SDDS_WritePage(&evalParameters->dataset)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void compareOriginalToFit(double *x, double *y, double **residual, int64_t points, double *rmsResidual, double *coef, int32_t *order, long terms) {
  int64_t i;
  double residualSum2, fit;

  *residual = tmalloc(sizeof(**residual) * points);

  residualSum2 = 0;
  for (i = 0; i < points; i++) {
    fit = eval_sum(basis_fn, coef, order, terms, x[i]);
    (*residual)[i] = y[i] - fit;
    residualSum2 += sqr((*residual)[i]);
  }
  *rmsResidual = sqrt(residualSum2 / points);
}
