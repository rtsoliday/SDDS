/**
 * @file sddspfit.c
 * @brief Performs nth order polynomial least squares fitting for SDDS files.
 *
 * @details
 * sddspfit fits data to the form:
 *
 * \f[
 * y = \sum_{i=0}^{N-1} A[i] \cdot P(x - x_{offset}, i)
 * \f]
 *
 * where \( P(x, i) \) is the ith basis function evaluated at \( x \).
 * By default, \( P(x, i) = x^i \), but Chebyshev T polynomials can also be used as the basis functions.
 *
 * The program outputs the coefficients \( A[i] \) and their estimated errors.
 *
 * @section Usage
 * ```
 * sddspfit [<inputfile>] [<outputfile>] 
 *          [-pipe=[input][,output]]
 *           -columns=<xname>,<yname>[,xSigma=<name>][,ySigma=<name>]
 *           -terms=<number>
 *          [-symmetry={none|odd|even}]
 *          [-orders=<number>[,...]}]
 *          [-reviseOrders[=threshold=<chiValue>][,verbose][,complete=<chiThreshold>][,goodEnough=<chiValue>]]
 *          [-chebyshev[=convert]]
 *          [-xOffset=<value>] 
 *          [-autoOffset] 
 *          [-xFactor=<value>]
 *          [-sigmas=<value>,{absolute|fractional}]
 *          [-modifySigmas] 
 *          [-generateSigmas[={keepLargest|keepSmallest}]]
 *          [-sparse=<interval>] 
 *          [-range=<lower>,<upper>[,fitOnly]]
 *          [-normalize[=<termNumber>]] 
 *          [-verbose]
 *          [-evaluate=<filename>[,begin=<value>][,end=<value>][,number=<integer>][,valuesFile=<filename>,valuesColumn=<string>[,reusePage]]]
 *          [-fitLabelFormat=<sprintf-string>] 
 *          [-copyParameters] 
 *          [-majorOrder={row|column}]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Specifies the column names for x and y data, and optionally their sigmas.            |
 * | `-terms`                              | Number of terms desired in the fit.                                                  |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-symmetry`                           | Specifies symmetry of the fit about xOffset (none, odd, or even).                    |
 * | `-orders`                             | Specifies the orders to use in the fit.                                              |
 * | `-reviseOrders`                       | Revises orders to optimize the fit.                                                  |
 * | `-chebyshev`                          | Uses Chebyshev T polynomials for the basis functions.                                |
 * | `-xOffset`, `-autoOffset`, `-xFactor` | Modifies x values for fitting.                                                       |
 * | `-sigmas`                             | Specifies absolute or fractional sigmas for the points.                              |
 * | `-modifySigmas`, `-generateSigmas`    | Modifies or generates y sigmas based on data deviations.                             |
 * | `-sparse`                             | Specify integer interval at which to sample data.                                    |
 * | `-range`                              | Restricts fitting to a specified x range.                                            |
 * | `-normalize`                          | Normalizes coefficients so a specified term equals unity.                            |
 * | `-verbose`                            | Outputs additional details during execution.                                         |
 * | `-evaluate`                           | Outputs evaluated fit at specified points or from a file.                            |
 * | `-fitLabelFormat`                     | Format string for labeling the fit.                                                  |
 * | `-repeatFits'                         | Repeats the fit <number> times with resampling to estimate errors in fit coefficients. |
 * | `-copyParameters`                     | Copies parameters from the input file to the output file.                            |
 * | `-majorOrder`                         | Specifies output order (row or column).                                              |
 *
 * @subsection Incompatibilities
 *   - `-symmetry` is incompatible with:
 *     - `-orders`
 *   - `-chebyshev` is incompatible with:
 *     - `-orders`
 *     - `-symmetry`
 *   - Only one of the following may be specified:
 *     - `-generateSigmas`
 *     - `-modifySigmas`
 *   - For `-normalize`:
 *     - The term specified must exist in the fit.
 *   - For `-reviseOrders`:
 *     - Requires y sigmas or generated sigmas.
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
#include "scan.h"
#include "../../2d_interpolate/nn/nan.h"

void print_coefs(FILE *fprec, double x_offset, double x_scale, long chebyshev,
                 double *coef, double *s_coef, int32_t *order, long n_terms,
                 double chi, long norm_term, char *prepend);
char **makeCoefficientUnits(SDDS_DATASET *SDDSout, char *xName, char *yName,
                            int32_t *order, long terms);
long setCoefficientData(SDDS_DATASET *SDDSout, double *coef, double *coefSigma,
                        char **coefUnits, int32_t *order, long terms, long chebyshev,
                        char *fitLabelFormat, char *rpnSeqBuffer);
char **initializeOutputFile(SDDS_DATASET *SDDSout, char *output,
                            SDDS_DATASET *SDDSin, char *input, char *xName,
                            char *yName, char *xSigmaName, char *ySigmaName,
                            long sigmasValid, int32_t *order, long terms,
                            long chebyshev, long copyParameters, long repeatFits);
void checkInputFile(SDDS_DATASET *SDDSin, char *xName, char *yName,
                    char *xSigmaName, char *ySigmaName);
long coefficient_index(int32_t *order, long terms, long order_of_interest);
void makeFitLabel(char *buffer, long bufsize, char *fitLabelFormat,
                  double *coef, double *coefSigma, int32_t *order, long terms, long chebyshev);
void createRpnSequence(char *buffer, long bufsize, double *coef, int32_t *order,
                       long terms);

double tcheby(double x, long n);
double dtcheby(double x, long n);
double ipower(double x, long n);
double dipower(double x, long n);
long reviseFitOrders(double *x, double *y, double *sy, int64_t points,
                     long terms, int32_t *order, double *coef,
                     double *coefSigma, double *diff,
                     double (*basis_fn)(double xa, long ordera),
                     unsigned long reviseOrders, double xOffset,
                     double xScaleFactor, long normTerm, long ySigmasValid,
                     long chebyshev, double revpowThreshold,
                     double revpowCompleteThres, double goodEnoughChi);
long reviseFitOrders1(double *x, double *y, double *sy, int64_t points,
                      long terms, int32_t *order, double *coef,
                      double *coefSigma, double *diff,
                      double (*basis_fn)(double xa, long ordera),
                      unsigned long reviseOrders, double xOffset,
                      double xScaleFactor, long normTerm, long ySigmasValid,
                      long chebyshev, double revpowThreshold,
                      double goodEnoughChi);
void compareOriginalToFit(double *x, double *y, double **residual,
                          int64_t points, double *rmsResidual, double *coef,
                          int32_t *order, long terms);

typedef struct {
  long nTerms;  /* Maximum power is nTerms-1 */
  double *coef; /* Coefficients for Chebyshev T polynomials */
} CHEBYSHEV_COEF;

CHEBYSHEV_COEF *makeChebyshevCoefficients(long maxOrder, long *nPoly);
void convertFromChebyshev(long termsT, int32_t *orderT, double *coefT, double *coefSigmaT,
                          long *termsOrdinaryRet, int32_t **orderOrdinaryRet, double **coefOrdinaryRet, double **coefSigmaOrdinaryRet);

/* Enumeration for option types */
enum option_type {
  CLO_COLUMNS,
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
  CLO_AUTOOFFSET,
  CLO_COPY_PARAMETERS,
  CLO_MAJOR_ORDER,
  CLO_REPEATFITS,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "columns",
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
  "autooffset",
  "copyparameters",
  "majorOrder",
  "repeatfits",
};

char *USAGE =
  "sddspfit [<inputfile>] [<outputfile>] [-pipe=[input][,output]]\n\
  -columns=<xname>,<yname>[,xSigma=<name>][,ySigma=<name>]\n\
  [ {-terms=<number> [-symmetry={none|odd|even}] | -orders=<number>[,...]} ]\n\
  [-reviseOrders [=threshold=<chiValue>] [,verbose] [,complete=<chiThreshold>] [,goodEnough=<chiValue>]]\n\
  [-chebyshev [=convert]]\n\
  [-xOffset=<value>] [-autoOffset] [-xFactor=<value>]\n\
  [-sigmas=<value>,{absolute|fractional}] \n\
  [-modifySigmas] [-generateSigmas[={keepLargest|keepSmallest}]]\n\
  [-sparse=<interval>] [-range=<lower>,<upper>[,fitOnly]]\n\
  [-normalize[=<termNumber>]] [-verbose]\n\
  [-evaluate=<filename>[,begin=<value>] [,end=<value>] [,number=<integer>] \n\
            [,valuesFile=<filename>,valuesColumn=<string>[,reusePage]]]\n\
  [-fitLabelFormat=<sprintf-string>] [-copyParameters] [-majorOrder={row|column}]\n\n\
Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static char *additional_help1 = "\n\
sddspfit fits data to the form y = SUM(i){ A[i] * P(x - x_offset, i) }, where P(x, i) is the ith basis\n\
function evaluated at x. By default, P(x, i) = x^i. Chebyshev T polynomials can also be selected as the basis functions.\n\n\
 -columns               Specify names of data columns to use.\n\
 -terms                 Number of terms desired in fit.\n\
 -symmetry              Symmetry of desired fit about x_offset.\n\
 -orders                Orders (P[i]) to use in fitting.\n\
 -reviseOrders          Modify the orders used in the fit to eliminate poorly-determined coefficients based on fitting\n\
                             of the first data page. The algorithm adds one order at a time, terminating when the reduced\n\
                             chi-squared is less than the 'goodEnough' value (default: 1) or when the new term does not improve\n\
                             the reduced chi-squared by more than the threshold value (default: 0.1). It next tries removing terms one at a time.\n\
                             Finally, if the resulting best reduced chi-squared is greater than the threshold given with the 'complete' option,\n\
                             it also tries all possible combinations of allowed terms.\n\
 -chebyshev             Use Chebyshev T polynomials (xOffset is set automatically).\n\
                             Giving the `convert` option causes the fit to be written out in terms of ordinary polynomials.\n\
 -majorOrder            Specify output file in row or column major order.\n\
 -xOffset               Desired value of x to fit about.\n";

static char *additional_help2 =
" -autoOffset           Automatically offset x values by the mean x value for fitting.\n\
                           Helpful if x values are very large in magnitude.\n\
 -xFactor               Desired factor to multiply x values by before fitting.\n\
 -sigmas                Specify absolute or fractional sigma for all points.\n\
 -modifySigmas          Modify the y sigmas using the x sigmas and an initial fit.\n\
 -generateSigmas        Generate y sigmas from the RMS deviation from an initial fit.\n\
                            Optionally keep the sigmas from the data if larger/smaller than RMS deviation.\n\
 -repeatFits            Repeats the fit <number> times with resampling (bootstrap) to estimate RMS errors in fit coefficients.\n\
 -sparse                Specify integer interval at which to sample data.\n\
 -range                 Specify range of independent variable over which to perform fit and evaluation.\n\
                             If 'fitOnly' is given, then fit is compared to data over the original range.\n\
 -normalize             Normalize so that the specified term is unity.\n\
 -verbose               Generates extra output that may be useful.\n\
 -evaluate              Specify evaluation of fit over a selected range of equispaced points,\n\
                             or at values listed in a file.\n\
 -copyParameters        If given, program copies all parameters from the input file into the main output file.\n\
                             By default, no parameters are copied.\n\n";

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
#define REVPOW_COMPLETE 0x0004

/* SDDS indices for output page */
static long iIntercept = -1, iInterceptSigma = -1;
static long iSlope = -1, iSlopeSigma = -1;
static long iCurvature = -1, iCurvatureSigma = -1;
static long *iTerm = NULL, *iTermSig = NULL;
static long iOffset = -1, iFactor = -1;
static long iChiSq = -1, iRmsResidual = -1, iSigLevel = -1;
static long iFitIsValid = -1, iFitLabel = -1, iTerms = -1;
static long iRpnSequence;

static long ix = -1, iy = -1, ixSigma = -1, iySigma = -1;
static long iFit = -1, iResidual = -1;

static char *xSymbol, *ySymbol;

#define EVAL_BEGIN_GIVEN 0x0001U
#define EVAL_END_GIVEN 0x0002U
#define EVAL_NUMBER_GIVEN 0x0004U
#define EVAL_VALUESFILE_GIVEN 0x0008U
#define EVAL_VALUESCOLUMN_GIVEN 0x0010U
#define EVAL_REUSE_PAGE_GIVEN 0x0020U

typedef struct {
  char *file;
  short initialized;
  int64_t number;
  unsigned long flags;
  double begin, end;
  SDDS_DATASET dataset;
  short inputInitialized;
  char *valuesFile, *valuesColumn;
  SDDS_DATASET valuesDataset;
} EVAL_PARAMETERS;
void makeEvaluationTable(EVAL_PARAMETERS *evalParameters, double *x, int64_t n,
                         double *coef, int32_t *order, long terms,
                         SDDS_DATASET *SDDSin, char *xName, char *yName);

static double (*basis_fn)(double xa, long ordera);
static double (*basis_dfn)(double xa, long ordera);

int main(int argc, char **argv) {
  double *x = NULL, *y = NULL, *sy = NULL, *sx = NULL, *diff = NULL, xOffset,
    xScaleFactor;
  double *xOrig = NULL, *yOrig = NULL, *sxOrig, *syOrig, *sy0 = NULL;
  long terms, normTerm, ySigmasValid;
  int64_t i, j, points, pointsOrig;
  long symmetry, chebyshev, autoOffset, copyParameters = 0;
  double sigmas;
  long sigmasMode, sparseInterval;
  unsigned long flags;
  double *coef, *coefSigma;
  double chi, xLow, xHigh, rmsResidual;
  char *xName, *yName, *xSigmaName, *ySigmaName;
  char *input, *output, **coefUnits;
  SDDS_DATASET SDDSin, SDDSout;
  long isFit, iArg, modifySigmas;
  long generateSigmas, verbose, ignoreSigmas;
  //long npages = 0;
  long invalid = 0;
  int32_t *order;
  SCANNED_ARG *s_arg;
  double xMin, xMax, revpowThreshold, revpowCompleteThres, goodEnoughChi;
  long rangeFitOnly = 0;
  double rms_average(double *d_x, int64_t d_n);
  char *fitLabelFormat = "%g";
  static char rpnSeqBuffer[SDDS_MAXLINE];
  unsigned long pipeFlags, reviseOrders, majorOrderFlag;
  EVAL_PARAMETERS evalParameters;
  short columnMajorOrder = -1;
  long repeatFits = 0;

  sxOrig = syOrig = NULL;
  rmsResidual = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2 || argc > (3 + N_OPTIONS)) {
    fprintf(stderr, "usage: %s%s%s\n", USAGE, additional_help1,
            additional_help2);
    exit(EXIT_FAILURE);
  }

  input = output = NULL;
  xName = yName = xSigmaName = ySigmaName = NULL;
  modifySigmas = reviseOrders = chebyshev = 0;
  order = NULL;
  symmetry = NO_SYMMETRY;
  xMin = xMax = 0;
  autoOffset = 0;
  generateSigmas = 0;
  sigmasMode = -1;
  sigmas = 1;
  sparseInterval = 1;
  terms = 2;
  verbose = ignoreSigmas = 0;
  normTerm = -1;
  xOffset = 0;
  xScaleFactor = 1;
  coefUnits = NULL;
  basis_fn = ipower;
  basis_dfn = dipower;
  pipeFlags = 0;
  evalParameters.file = evalParameters.valuesFile = evalParameters.valuesColumn = NULL;
  evalParameters.initialized = evalParameters.inputInitialized = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (s_arg[iArg].arg_type == OPTION) {
  switch (match_string(s_arg[iArg].list[0], option, N_OPTIONS, 0)) {
      case CLO_REPEATFITS:
        if (s_arg[iArg].n_items != 2 || sscanf(s_arg[iArg].list[1], "%ld", &repeatFits) != 1 || repeatFits < 1)
          SDDS_Bomb("invalid -repeatFits syntax");
	if (repeatFits<10)
	  SDDS_Bomb("The number of repeats should be at least 10");
        break;
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[iArg].n_items--;
        if (s_arg[iArg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[iArg].list + 1,
                           &s_arg[iArg].n_items, 0, "row", -1, NULL, 0,
                           SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0,
                           SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_MODIFYSIGMAS:
        modifySigmas = 1;
        break;
      case CLO_AUTOOFFSET:
        autoOffset = 1;
        break;
      case CLO_ORDERS:
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
        if ((s_arg[iArg].n_items != 3 && s_arg[iArg].n_items != 4) ||
            1 != sscanf(s_arg[iArg].list[1], "%lf", &xMin) ||
            1 != sscanf(s_arg[iArg].list[2], "%lf", &xMax) || xMin >= xMax)
          SDDS_Bomb("incorrect -range syntax");
        if (s_arg[iArg].n_items == 4) {
          if (strncmp(str_tolower(s_arg[iArg].list[3]), "fitonly",
                      strlen(s_arg[iArg].list[3])) == 0) {
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
          if (strncmp(s_arg[iArg].list[1], "keepsmallest",
                      strlen(s_arg[iArg].list[1])) == 0)
            generateSigmas |= FLGS_KEEPSMALLEST;
          if (strncmp(s_arg[iArg].list[1], "keeplargest",
                      strlen(s_arg[iArg].list[1])) == 0)
            generateSigmas |= FLGS_KEEPLARGEST;
          if ((generateSigmas & FLGS_KEEPSMALLEST) &&
              (generateSigmas & FLGS_KEEPLARGEST))
            SDDS_Bomb("ambiguous -generateSigmas syntax");
        }
        break;
      case CLO_TERMS:
        if (s_arg[iArg].n_items != 2 ||
            sscanf(s_arg[iArg].list[1], "%ld", &terms) != 1)
          SDDS_Bomb("invalid -terms syntax");
        break;
      case CLO_XOFFSET:
        if (s_arg[iArg].n_items != 2 ||
            sscanf(s_arg[iArg].list[1], "%lf", &xOffset) != 1)
          SDDS_Bomb("invalid -xOffset syntax");
        break;
      case CLO_SYMMETRY:
        if (s_arg[iArg].n_items == 2) {
          if ((symmetry = match_string(s_arg[iArg].list[1], symmetry_options,
                                       N_SYMMETRY_OPTIONS, 0)) < 0)
            SDDS_Bomb("unknown option used with -symmetry");
        } else
          SDDS_Bomb("incorrect -symmetry syntax");
        break;
      case CLO_SIGMAS:
        if (s_arg[iArg].n_items != 3)
          SDDS_Bomb("incorrect -sigmas syntax");
        if (sscanf(s_arg[iArg].list[1], "%lf", &sigmas) != 1)
          SDDS_Bomb("couldn't scan value for -sigmas");
        if ((sigmasMode = match_string(s_arg[iArg].list[2], sigmas_options,
                                       N_SIGMAS_OPTIONS, 0)) < 0)
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
            (s_arg[iArg].n_items == 2 &&
             sscanf(s_arg[iArg].list[1], "%ld", &normTerm) != 1) ||
            normTerm < 0)
          SDDS_Bomb("invalid -normalize syntax");
        break;
      case CLO_REVISEORDERS:
        revpowThreshold = 0.1;
        revpowCompleteThres = 10;
        goodEnoughChi = 1;
        s_arg[iArg].n_items -= 1;
        if (!scanItemList(&reviseOrders, s_arg[iArg].list + 1,
                          &s_arg[iArg].n_items, 0,
                          "threshold", SDDS_DOUBLE, &revpowThreshold, 1, 0,
                          "complete", SDDS_DOUBLE, &revpowCompleteThres, 1, REVPOW_COMPLETE,
                          "goodenough", SDDS_DOUBLE, &goodEnoughChi, 1, 0,
                          "verbose", -1, NULL, 1, REVPOW_VERBOSE, NULL) ||
            revpowThreshold < 0 || revpowCompleteThres < 0 || goodEnoughChi < 0)
          SDDS_Bomb("invalid -reviseOrders syntax");
        reviseOrders |= REVPOW_ACTIVE;
        break;
      case CLO_CHEBYSHEV:
        if (s_arg[iArg].n_items > 2 ||
            (s_arg[iArg].n_items == 2 &&
             strncmp(s_arg[iArg].list[1], "convert",
                     strlen(s_arg[iArg].list[1])) != 0))
          SDDS_Bomb("invalid -chebyshev syntax");
        chebyshev = s_arg[iArg].n_items; /* 1: use chebyshev polynomials; 2: also convert to ordinary form */
        basis_fn = tcheby;
        basis_dfn = dtcheby;
        break;
      case CLO_XFACTOR:
        if (s_arg[iArg].n_items != 2 ||
            sscanf(s_arg[iArg].list[1], "%lf", &xScaleFactor) != 1 ||
            xScaleFactor == 0)
          SDDS_Bomb("invalid -xFactor syntax");
        break;
      case CLO_COLUMNS:
        if (s_arg[iArg].n_items < 3 || s_arg[iArg].n_items > 5)
          SDDS_Bomb("invalid -columns syntax");
        xName = s_arg[iArg].list[1];
        yName = s_arg[iArg].list[2];
        s_arg[iArg].n_items -= 3;
        if (!scanItemList(&flags, s_arg[iArg].list + 3, &s_arg[iArg].n_items, 0,
                          "xsigma", SDDS_STRING, &xSigmaName, 1, 0, "ysigma",
                          SDDS_STRING, &ySigmaName, 1, 0, NULL))
          SDDS_Bomb("invalid -columns syntax");
        break;
      case CLO_FITLABELFORMAT:
        if (s_arg[iArg].n_items != 2)
          SDDS_Bomb("invalid -fitLabelFormat syntax");
        fitLabelFormat = s_arg[iArg].list[1];
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[iArg].list + 1, s_arg[iArg].n_items - 1,
                               &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_EVALUATE:
        if (s_arg[iArg].n_items < 2)
          SDDS_Bomb("invalid -evaluate syntax");
        evalParameters.file = s_arg[iArg].list[1];
        s_arg[iArg].n_items -= 2;
        s_arg[iArg].list += 2;
        if (!scanItemList(&evalParameters.flags, s_arg[iArg].list,
                          &s_arg[iArg].n_items, 0,
                          "begin", SDDS_DOUBLE, &evalParameters.begin, 1, EVAL_BEGIN_GIVEN,
                          "end", SDDS_DOUBLE, &evalParameters.end, 1, EVAL_END_GIVEN,
                          "number", SDDS_LONG64, &evalParameters.number, 1, EVAL_NUMBER_GIVEN,
                          "valuesfile", SDDS_STRING, &evalParameters.valuesFile, 1, EVAL_VALUESFILE_GIVEN,
                          "valuescolumn", SDDS_STRING, &evalParameters.valuesColumn, 1, EVAL_VALUESCOLUMN_GIVEN,
                          "reusepage", 0, NULL, 0, EVAL_REUSE_PAGE_GIVEN,
                          NULL))
          SDDS_Bomb("invalid -evaluate syntax");
        if (evalParameters.flags & EVAL_VALUESFILE_GIVEN || evalParameters.flags & EVAL_VALUESCOLUMN_GIVEN) {
          if (evalParameters.flags & (EVAL_BEGIN_GIVEN | EVAL_END_GIVEN | EVAL_NUMBER_GIVEN))
            SDDS_Bomb("invalid -evaluate syntax: given begin/end/number or valuesFile/valuesColumn, not a mixture.");
          if (!(evalParameters.flags & EVAL_VALUESFILE_GIVEN && evalParameters.flags & EVAL_VALUESCOLUMN_GIVEN))
            SDDS_Bomb("invalid -evaluate syntax: give both valuesFile and valuesColumn, not just one");
        }
        evalParameters.initialized = 0;
        break;
      case CLO_COPY_PARAMETERS:
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

  processFilenames("sddspfit", &input, &output, pipeFlags, 0, NULL);

  if (symmetry && order)
    SDDS_Bomb("can't specify both -symmetry and -orders");
  if (chebyshev && order)
    SDDS_Bomb("can't specify both -chebyshev and -orders");
  if (chebyshev && symmetry)
    SDDS_Bomb("can't specify both -chebyshev and -symmetry");
  if (!xName || !yName)
    SDDS_Bomb("you must specify a column name for x and y");

  if (modifySigmas && !xSigmaName)
    SDDS_Bomb("you must specify x sigmas with -modifySigmas");
  if (generateSigmas) {
    if (modifySigmas)
      SDDS_Bomb("you can't specify both -generateSigmas and -modifySigmas");
  }
  if (ySigmaName) {
    if (sigmasMode != -1)
      SDDS_Bomb("you can't specify both -sigmas and a y sigma name");
  }
  ySigmasValid = 0;
  if (sigmasMode != -1 || generateSigmas || ySigmaName || modifySigmas)
    ySigmasValid = 1;

  if (normTerm >= 0 && normTerm >= terms)
    SDDS_Bomb("can't normalize to that term--not that many terms");
  if (reviseOrders && !(sigmasMode != -1 || generateSigmas || ySigmaName))
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
  coef = tmalloc(sizeof(*coef) * terms);
  coefSigma = tmalloc(sizeof(*coefSigma) * terms);
  iTerm = tmalloc(sizeof(*iTerm) * terms);
  iTermSig = tmalloc(sizeof(*iTermSig) * terms);

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  checkInputFile(&SDDSin, xName, yName, xSigmaName, ySigmaName);
  coefUnits = initializeOutputFile(&SDDSout, output, &SDDSin, input, xName,
                                   yName, xSigmaName, ySigmaName, ySigmasValid,
                                   order, terms, chebyshev, copyParameters,  repeatFits);
  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major =
      SDDSin.layout.data_mode.column_major;
  while (SDDS_ReadPage(&SDDSin) > 0) {
    //npages++;
    invalid = 0;
    if ((points = SDDS_CountRowsOfInterest(&SDDSin)) < terms) {
      pointsOrig = 0;
      invalid = 1;
      isFit = 0;
    } else {
      if (!(x = SDDS_GetColumnInDoubles(&SDDSin, xName))) {
        fprintf(stderr, "error: unable to read column %s\n", xName);
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!(y = SDDS_GetColumnInDoubles(&SDDSin, yName))) {
        fprintf(stderr, "error: unable to read column %s\n", yName);
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      sx = NULL;
      if (xSigmaName && !(sx = SDDS_GetColumnInDoubles(&SDDSin, xSigmaName))) {
        fprintf(stderr, "error: unable to read column %s\n", xSigmaName);
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      sy0 = NULL;
      if (ySigmaName && !(sy0 = SDDS_GetColumnInDoubles(&SDDSin, ySigmaName))) {
        fprintf(stderr, "error: unable to read column %s\n", ySigmaName);
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!sy0)
        sy0 = tmalloc(sizeof(*sy0) * points);

      if (xMin != xMax || sparseInterval != 1) {
        xOrig = tmalloc(sizeof(*xOrig) * points);
        yOrig = tmalloc(sizeof(*yOrig) * points);
        if (sx)
          sxOrig = tmalloc(sizeof(*sxOrig) * points);
        if (ySigmasValid)
          syOrig = tmalloc(sizeof(*syOrig) * points);
        pointsOrig = points;
        for (i = j = 0; i < points; i++) {
          xOrig[i] = x[i];
          yOrig[i] = y[i];
          if (ySigmasValid)
            syOrig[i] = sy0[i];
          if (sx)
            sxOrig[i] = sx[i];
        }
        if (xMin != xMax) {
          for (i = j = 0; i < points; i++) {
            if (xOrig[i] <= xMax && xOrig[i] >= xMin) {
              x[j] = xOrig[i];
              y[j] = yOrig[i];
              if (ySigmasValid)
                sy0[j] = syOrig[i];
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
              y[j] = y[i];
              if (ySigmasValid)
                sy0[j] = sy0[i];
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
        for (i = 0; i < points; i++)
          sy0[i] = sigmas;
        if (sy0 != syOrig)
          for (i = 0; i < pointsOrig; i++)
            syOrig[i] = sigmas;
      } else if (sigmasMode == FRACTIONAL_SIGMAS) {
        for (i = 0; i < points; i++)
          sy0[i] = sigmas * fabs(y[i]);
        if (sy0 != syOrig)
          for (i = 0; i < pointsOrig; i++)
            syOrig[i] = fabs(yOrig[i]) * sigmas;
      }

      if (!ySigmasValid || generateSigmas)
        for (i = 0; i < points; i++)
          sy0[i] = 1;
      else
        for (i = 0; i < points; i++)
          if (sy0[i] == 0)
            SDDS_Bomb("y sigma = 0 for one or more points.");

      diff = tmalloc(sizeof(*x) * points);
      sy = tmalloc(sizeof(*sy) * points);
      for (i = 0; i < points; i++)
        sy[i] = sy0[i];

      if (autoOffset && !compute_average(&xOffset, x, points))
        xOffset = 0;

      set_argument_offset(xOffset);
      set_argument_scale(xScaleFactor);
      if (chebyshev) {
        if (xOffset) {
          /* User has provided an offset, adjust scale factor to match range of data */
          xScaleFactor = MAX(fabs(xHigh - xOffset), fabs(xLow - xOffset));
        } else {
          /* Compute the offset and scale factor to match range of data */
          xOffset = (xHigh + xLow) / 2;
          xScaleFactor = (xHigh - xLow) / 2;
        }
        set_argument_offset(xOffset);
        set_argument_scale(xScaleFactor);
      }

      if (generateSigmas || modifySigmas) {
        /* do an initial fit */
        isFit = lsfg(x, y, sy, points, terms, order, coef, coefSigma, &chi,
                     diff, basis_fn);
        if (!isFit)
          SDDS_Bomb("initial fit failed.");
        if (verbose) {
          fputs("initial_fit:", stdout);
          print_coefs(stdout, xOffset, xScaleFactor, chebyshev, coef, NULL,
                      order, terms, chi, normTerm, "");
          fprintf(stdout, "unweighted rms deviation from fit: %21.15e\n",
                  rms_average(diff, points));
        }
        if (modifySigmas) {
          if (!ySigmasValid) {
            for (i = 0; i < points; i++)
              sy[i] =
                fabs(eval_sum(basis_dfn, coef, order, terms, x[i]) * sx[i]);
          } else
            for (i = 0; i < points; i++) {
              sy[i] = sqrt(
                           sqr(sy0[i]) +
                           sqr(eval_sum(basis_dfn, coef, order, terms, x[i]) * sx[i]));
            }
        }
        if (generateSigmas) {
          double sigma;
          for (i = sigma = 0; i < points; i++) {
            sigma += sqr(diff[i]);
          }
          sigma = sqrt(sigma / (points - terms));
          for (i = 0; i < points; i++) {
            if (generateSigmas & FLGS_KEEPSMALLEST) {
              if (sigma < sy[i])
                sy[i] = sigma;
            } else if (generateSigmas & FLGS_KEEPLARGEST) {
              if (sigma > sy[i])
                sy[i] = sigma;
            } else {
              sy[i] = sigma;
            }
          }
          for (i = 0; i < pointsOrig; i++) {
            if (generateSigmas & FLGS_KEEPSMALLEST) {
              if (sigma < sy0[i])
                sy0[i] = sigma;
            } else if (generateSigmas & FLGS_KEEPLARGEST) {
              if (sigma > sy0[i])
                sy0[i] = sigma;
            } else {
              sy0[i] = sigma;
            }
          }
        }
      }

      if (reviseOrders & REVPOW_ACTIVE) {
        terms = reviseFitOrders(
                                x, y, sy, points, terms, order, coef, coefSigma, diff, basis_fn,
                                reviseOrders, xOffset, xScaleFactor, normTerm, ySigmasValid,
                                chebyshev, revpowThreshold, revpowCompleteThres, goodEnoughChi);
        reviseOrders = 0;
      }

      if (repeatFits <= 1) {
        isFit = lsfg(x, y, sy, points, terms, order, coef, coefSigma, &chi, diff,
                     basis_fn);
      } else {
        double *coefRepeat = tmalloc(sizeof(*coefRepeat) * terms * repeatFits);
        double *coefSigmaRepeat = tmalloc(sizeof(*coefSigmaRepeat) * terms * repeatFits);
        long fitIdx;
        isFit = 1;
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
            ySample[i] = y[indices[i]];
            sySample[i] = sy[i];
          }
          double chiTmp;
          double *diffTmp = tmalloc(sizeof(*diffTmp) * points);
          int fitOk = lsfg(xSample, ySample, sySample, points, terms, order, coefRepeat + fitIdx * terms, coefSigmaRepeat + fitIdx * terms, &chiTmp, diffTmp, basis_fn);
          free(indices);
          free(xSample);
          free(ySample);
          free(sySample);
          free(diffTmp);
	  isFit *= fitOk;
        }
        // Compute mean and rms for each coefficient
        for (i = 0; i < terms; i++) {
          double sum = 0, sum2 = 0;
          for (j = 0; j < repeatFits; j++) {
            double v = coefRepeat[j * terms + i];
            sum += v;
            sum2 += v * v;
          }
          coef[i] = sum / repeatFits;
          coefSigma[i] = sqrt(sum2 / repeatFits - (coef[i] * coef[i]));
        }
        free(coefRepeat);
        free(coefSigmaRepeat);
        // Evaluate the fit for the mean coefficients and populate the diff array (residuals)
	chi = 0;
        for (i = 0; i < points; i++) {
          double fitValue = eval_sum(basis_fn, coef, order, terms, x[i]);
          diff[i] = fitValue- y[i];
	  chi += sqr(diff[i]);
        }
	chi /= points - terms;
      }
      if (isFit) {
        rmsResidual = rms_average(diff, points);
        if (verbose) {
          print_coefs(stdout, xOffset, xScaleFactor, chebyshev, coef,
                      (ySigmasValid ? coefSigma : NULL), order, terms, chi,
                      normTerm, "");
          fprintf(stdout, "unweighted rms deviation from fit: %21.15e\n",
                  rmsResidual);
        }
      } else if (verbose)
        fprintf(stdout, "fit failed.\n");

      if (evalParameters.file)
        makeEvaluationTable(&evalParameters, x, points, coef, order, terms,
                            &SDDSin, xName, yName);
    }

    if (!SDDS_StartPage(&SDDSout, rangeFitOnly ? pointsOrig : points))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    rpnSeqBuffer[0] = 0;
    if (!invalid) {
      setCoefficientData(&SDDSout, coef, ((repeatFits || ySigmasValid) ? coefSigma : NULL),
                         coefUnits, order, terms, chebyshev, fitLabelFormat,
                         rpnSeqBuffer);
      if (rangeFitOnly) {
        double *residual;
        compareOriginalToFit(xOrig, yOrig, &residual, pointsOrig, &rmsResidual,
                             coef, order, terms);

        if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, xOrig,
                                       pointsOrig, ix) ||
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, yOrig,
                                       pointsOrig, iy) ||
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, residual,
                                       pointsOrig, iResidual))
          SDDS_PrintErrors(stderr,
                           SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (i = 0; i < pointsOrig; i++)
          residual[i] = yOrig[i] - residual[i]; /* computes fit values from
                                                   residual and y */
        if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, residual,
                                       pointsOrig, iFit))
          SDDS_PrintErrors(stderr,
                           SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

        if (ixSigma != -1 &&
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, sxOrig,
                                       pointsOrig, ixSigma))
          SDDS_PrintErrors(stderr,
                           SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (ySigmasValid && iySigma != -1 &&
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, syOrig,
                                       pointsOrig, iySigma))
          SDDS_PrintErrors(stderr,
                           SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        free(residual);
      } else {
        for (i = 0; i < points; i++)
          diff[i] =
            -diff[i]; /* convert from (Fit-y) to (y-Fit) to get residual */
        if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, x, points,
                                       ix) ||
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, y, points,
                                       iy) ||
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, diff,
                                       points, iResidual))
          SDDS_PrintErrors(stderr,
                           SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (i = 0; i < points; i++)
          diff[i] =
            y[i] - diff[i]; /* computes fit values from residual and y */
        if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, diff,
                                       points, iFit))
          SDDS_PrintErrors(stderr,
                           SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

        if (ixSigma != -1 &&
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, sx, points,
                                       ixSigma))
          SDDS_PrintErrors(stderr,
                           SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (ySigmasValid && iySigma != -1 &&
            !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_INDEX, sy, points,
                                       iySigma))
          SDDS_PrintErrors(stderr,
                           SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    if (copyParameters && !SDDS_CopyParameters(&SDDSout, &SDDSin))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_SetParameters(
                            &SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iRpnSequence,
                            invalid ? "" : rpnSeqBuffer, iRmsResidual,
                            invalid ? NaN : rmsResidual, iChiSq, invalid ? NaN : chi, iTerms,
                            terms, iSigLevel,
                            invalid ? NaN : ChiSqrSigLevel(chi, points - terms), iOffset,
                            invalid ? NaN : xOffset, iFactor, invalid ? NaN : xScaleFactor,
                            iFitIsValid, isFit ? 'y' : 'n', -1) ||
        !SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!invalid) {
      free(diff);
      free(sy);
      if (xOrig != x)
        free(xOrig);
      if (yOrig != y)
        free(yOrig);
      if (syOrig != sy0)
        free(syOrig);
      if (sxOrig != sx)
        free(sxOrig);
      free(x);
      free(y);
      free(sx);
      free(sy0);
    }
  }
  if (!SDDS_Terminate(&SDDSin) || !SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (evalParameters.initialized && !SDDS_Terminate(&(evalParameters.dataset)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  /* free_scanargs(&s_arg, argc); */
  free(coef);
  free(coefSigma);
  if (coefUnits)
    free(coefUnits);
  if (order)
    free(order);

  return (EXIT_SUCCESS);
}

void print_coefs(FILE *fpo, double xOffset, double xScaleFactor, long chebyshev,
                 double *coef, double *coefSigma, int32_t *order, long terms,
                 double chi, long normTerm, char *prepend) {
  long i;

  if (chebyshev)
    fprintf(fpo, "%s%ld-term Chebyshev T polynomial least-squares fit about "
            "x=%21.15e, scaled by %21.15e:\n",
            prepend, terms, xOffset, xScaleFactor);
  else
    fprintf(fpo, "%s%ld-term polynomial least-squares fit about x=%21.15e:\n",
            prepend, terms, xOffset);
  if (normTerm >= 0 && terms > normTerm) {
    if (coef[normTerm] != 0)
      fprintf(fpo, "%s  coefficients are normalized with factor %21.15e to "
              "make a[%ld]==1\n",
              prepend, coef[normTerm], (order ? order[normTerm] : normTerm));
    else {
      fprintf(fpo, "%s can't normalize coefficients as requested: a[%ld]==0\n",
              prepend, (order ? order[normTerm] : normTerm));
      normTerm = -1;
    }
  } else
    normTerm = -1;

  for (i = 0; i < terms; i++) {
    fprintf(fpo, "%sa[%ld] = %21.15e ", prepend, (order ? order[i] : i),
            (normTerm < 0 ? coef[i] : coef[i] / coef[normTerm]));
    if (coefSigma)
      fprintf(
              fpo, "+/- %21.15e\n",
              (normTerm < 0 ? coefSigma[i] : coefSigma[i] / fabs(coef[normTerm])));
    else
      fputc('\n', fpo);
  }
  if (coefSigma)
    fprintf(fpo, "%sreduced chi-squared = %21.15e\n", prepend, chi);
}

void makeFitLabel(char *buffer, long bufsize, char *fitLabelFormat,
                  double *coef, double *coefSigma, int32_t *order, long terms, long chebyshev) {
  long i;
  static char buffer1[SDDS_MAXLINE], buffer2[SDDS_MAXLINE], buffer3[SDDS_MAXLINE];

  sprintf(buffer, "%s = ", ySymbol);
  for (i = 0; i < terms; i++) {
    buffer1[0] = 0;
    if (coef[i] == 0)
      continue;
    if (order[i] == 0) {
      if (coef[i] > 0) {
        strcat(buffer1, "+");
        sprintf(buffer1 + 1, fitLabelFormat, coef[i]);
      } else
        sprintf(buffer1, fitLabelFormat, coef[i]);
      if (coefSigma) {
	strcat(buffer1, "($sa$e");
	sprintf(buffer3, fitLabelFormat, coefSigma[i]);
	strcat(buffer1, buffer3);
	strcat(buffer1, ")");
      }
    } else {
      if (coef[i] > 0) {
        strcat(buffer1, "+");
        sprintf(buffer1 + 1, fitLabelFormat, coef[i]);
      } else
        sprintf(buffer1, fitLabelFormat, coef[i]);
      if (coefSigma) {
	strcat(buffer1, "($sa$e");
	sprintf(buffer3, fitLabelFormat, coefSigma[i]);
	strcat(buffer1, buffer3);
	strcat(buffer1, ")");
      }
      if (order[i] >= 1) {
        strcat(buffer1, "*");
        if (chebyshev != 1) {
          strcat(buffer1, xSymbol);
          if (order[i] > 1) {
            sprintf(buffer2, "$a%d$n", order[i]);
            strcat(buffer1, buffer2);
          }
        } else {
          sprintf(buffer2, "T$b%d$n(%s)", order[i], xSymbol);
          strcat(buffer1, buffer2);
        }
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

long coefficient_index(int32_t *order, long terms, long order_of_interest) {
  long i;
  for (i = 0; i < terms; i++)
    if (order[i] == order_of_interest)
      return (i);
  return (-1);
}

void checkInputFile(SDDS_DATASET *SDDSin, char *xName, char *yName,
                    char *xSigmaName, char *ySigmaName) {
  char *ptr = NULL;
  if (!(ptr = SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, xName, NULL)))
    SDDS_Bomb("x column doesn't exist or is nonnumeric");
  free(ptr);
  ptr = NULL;
  if (!(ptr = SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, yName, NULL)))
    SDDS_Bomb("y column doesn't exist or is nonnumeric");
  free(ptr);
  ptr = NULL;
  if (xSigmaName &&
      !(ptr = SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, xSigmaName, NULL)))
    SDDS_Bomb("x sigma column doesn't exist or is nonnumeric");
  if (ptr)
    free(ptr);
  ptr = NULL;
  if (ySigmaName &&
      !(ptr = SDDS_FindColumn(SDDSin, FIND_NUMERIC_TYPE, ySigmaName, NULL)))
    SDDS_Bomb("y sigma column doesn't exist or is nonnumeric");
  if (ptr)
    free(ptr);
  ptr = NULL;
}

char **initializeOutputFile(SDDS_DATASET *SDDSout, char *output,
                            SDDS_DATASET *SDDSin, char *input, char *xName,
                            char *yName, char *xSigmaName, char *ySigmaName,
                            long sigmasValid, int32_t *order, long terms,
                            long chebyshev, long copyParameters, long repeatFits) {
  char buffer[SDDS_MAXLINE], buffer1[SDDS_MAXLINE], *xUnits, *yUnits,
    **coefUnits;
  long i;
  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddspfit output",
                             output) ||
      !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xName, NULL) ||
      !SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName, NULL) ||
      SDDS_GetColumnInformation(SDDSout, "symbol", &xSymbol, SDDS_GET_BY_NAME,
                                xName) != SDDS_STRING ||
      SDDS_GetColumnInformation(SDDSout, "symbol", &ySymbol, SDDS_GET_BY_NAME,
                                yName) != SDDS_STRING ||
      (xSigmaName &&
       !SDDS_TransferColumnDefinition(SDDSout, SDDSin, xSigmaName, NULL)) ||
      (ySigmaName &&
       !SDDS_TransferColumnDefinition(SDDSout, SDDSin, ySigmaName, NULL)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!xSymbol || SDDS_StringIsBlank(xSymbol))
    xSymbol = xName;
  if (!ySymbol || SDDS_StringIsBlank(ySymbol))
    ySymbol = yName;
  ix = SDDS_GetColumnIndex(SDDSout, xName);
  iy = SDDS_GetColumnIndex(SDDSout, yName);
  if (ySigmaName)
    iySigma = SDDS_GetColumnIndex(SDDSout, ySigmaName);
  if (xSigmaName)
    ixSigma = SDDS_GetColumnIndex(SDDSout, xSigmaName);
  if (SDDS_NumberOfErrors())
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  sprintf(buffer, "%sFit", yName);
  sprintf(buffer1, "Fit[%s]", ySymbol);
  if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName, buffer) ||
      !SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer1,
                                    SDDS_SET_BY_NAME, buffer))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if ((iFit = SDDS_GetColumnIndex(SDDSout, buffer)) < 0)
    SDDS_Bomb("unable to get index of just-defined fit output column");

  sprintf(buffer, "%sResidual", yName);
  sprintf(buffer1, "Residual[%s]", ySymbol);
  if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName, buffer) ||
      !SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer1,
                                    SDDS_SET_BY_NAME, buffer))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((iResidual = SDDS_GetColumnIndex(SDDSout, buffer)) < 0)
    SDDS_Bomb("unable to get index of just-defined residual output column");

  if (sigmasValid && !ySigmaName) {
    sprintf(buffer, "%sSigma", yName);
    if (!SDDS_TransferColumnDefinition(SDDSout, SDDSin, yName, buffer))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    iySigma = SDDS_GetColumnIndex(SDDSout, buffer);
    if (ySymbol && !SDDS_StringIsBlank(ySymbol)) {
      sprintf(buffer1, "Sigma[%s]", ySymbol);
      if (!SDDS_ChangeColumnInformation(SDDSout, "symbol", buffer1,
                                        SDDS_SET_BY_NAME, buffer))
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!(coefUnits = makeCoefficientUnits(SDDSout, xName, yName, order, terms)))
    SDDS_Bomb("unable to make coefficient units");

  if (SDDS_DefineArray(SDDSout, "Order", NULL, NULL, "Order of term in fit",
                       NULL, SDDS_LONG, 0, 1, "FitResults") < 0 ||
      SDDS_DefineArray(SDDSout, "Coefficient", "a", "[CoefficientUnits]",
                       "Coefficient of term in fit", NULL, SDDS_DOUBLE, 0, 1,
                       "FitResults") < 0 ||
      ((sigmasValid || repeatFits) &&
       SDDS_DefineArray(SDDSout, "CoefficientSigma", "$gs$r$ba$n",
                        "[CoefficientUnits]",
                        "Sigma of coefficient of term in fit", NULL,
                        SDDS_DOUBLE, 0, 1, "FitResults") < 0) ||
      SDDS_DefineArray(SDDSout, "CoefficientUnits", NULL, NULL, NULL, NULL,
                       SDDS_STRING, 0, 1, "FitResults") < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_DefineParameter(SDDSout, "Basis", NULL, NULL,
                           "Function basis for fit", NULL, SDDS_STRING,
                           chebyshev ? (chebyshev == 1 ? "Chebyshev T polynomials" : "Converted Chebyshev T polynomials")
                           : "ordinary polynomials") < 0 ||
      (iChiSq = SDDS_DefineParameter(
                                     SDDSout, "ReducedChiSquared", "$gh$r$a2$n/(N-M)", NULL,
                                     "Reduced chi-squared of fit", NULL, SDDS_DOUBLE, NULL)) < 0 ||
      SDDS_GetColumnInformation(SDDSout, "units", &yUnits, SDDS_GET_BY_NAME,
                                yName) != SDDS_STRING ||
      (iRmsResidual = SDDS_DefineParameter(
                                           SDDSout, "RmsResidual", "$gs$r$bres$n", yUnits,
                                           "RMS residual of fit", NULL, SDDS_DOUBLE, NULL)) < 0 ||
      (iSigLevel =
       SDDS_DefineParameter(SDDSout, "SignificanceLevel", NULL, NULL,
                            "Probability that data is from fit function",
                            NULL, SDDS_DOUBLE, NULL)) < 0 ||
      (iRpnSequence = SDDS_DefineParameter(SDDSout, "RpnSequence", NULL, NULL,
                                           "Rpn sequence to evaluate the fit",
                                           NULL, SDDS_STRING, NULL)) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (yUnits)
    free(yUnits);

  if (SDDS_GetColumnInformation(SDDSout, "units", &xUnits, SDDS_GET_BY_NAME,
                                xName) != SDDS_STRING)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(buffer, "%sOffset", xName);
  sprintf(buffer1, "Offset of %s for fit", xName);
  if ((iOffset = SDDS_DefineParameter(SDDSout, buffer, NULL, xUnits, buffer1,
                                      NULL, SDDS_DOUBLE, NULL)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(buffer, "%sScale", xName);
  sprintf(buffer1, "Scale factor of %s for fit", xName);
  if ((iFactor = SDDS_DefineParameter(SDDSout, buffer, NULL, xUnits, buffer1,
                                      NULL, SDDS_DOUBLE, NULL)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((iFitIsValid = SDDS_DefineParameter(SDDSout, "FitIsValid", NULL, NULL,
                                          NULL, NULL, SDDS_CHARACTER, NULL)) <
      0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((iTerms = SDDS_DefineParameter(SDDSout, "Terms", NULL, NULL,
                                     "Number of terms in fit", NULL, SDDS_LONG,
                                     NULL)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  iFitLabel = SDDS_DefineParameter(SDDSout, "sddspfitLabel", NULL, NULL, NULL,
                                   NULL, SDDS_STRING, NULL);
  if (!chebyshev) {
    if ((i = coefficient_index(order, terms, 0)) >= 0) {
      iIntercept =
        SDDS_DefineParameter(SDDSout, "Intercept", "Intercept", coefUnits[i],
                             "Intercept of fit", NULL, SDDS_DOUBLE, NULL);
      if (sigmasValid || repeatFits)
        iInterceptSigma = SDDS_DefineParameter(
                                               SDDSout, "InterceptSigma", "InterceptSigma", coefUnits[i],
                                               "Sigma of intercept of fit", NULL, SDDS_DOUBLE, NULL);
    }
    if ((i = coefficient_index(order, terms, 1)) >= 0) {
      iSlope = SDDS_DefineParameter(SDDSout, "Slope", "Slope", coefUnits[i],
                                    "Slope of fit", NULL, SDDS_DOUBLE, NULL);
      if (sigmasValid || repeatFits)
        iSlopeSigma = SDDS_DefineParameter(SDDSout, "SlopeSigma", "SlopeSigma",
                                           coefUnits[i], "Sigma of slope of fit",
                                           NULL, SDDS_DOUBLE, NULL);
    }
    if ((i = coefficient_index(order, terms, 2)) >= 0) {
      iCurvature =
        SDDS_DefineParameter(SDDSout, "Curvature", "Curvature", coefUnits[i],
                             "Curvature of fit", NULL, SDDS_DOUBLE, NULL);
      if (sigmasValid || repeatFits)
        iCurvatureSigma = SDDS_DefineParameter(
                                               SDDSout, "CurvatureSigma", "CurvatureSigma", coefUnits[i],
                                               "Sigma of curvature of fit", NULL, SDDS_DOUBLE, NULL);
    }
  }

  for (i = 0; i < terms; i++) {
    char s[100];
    sprintf(s, "Coefficient%02ld", (long)order[i]);
    iTerm[i] = SDDS_DefineParameter(SDDSout, s, s, coefUnits[i], NULL, NULL,
                                    SDDS_DOUBLE, NULL);
  }
  for (i = 0; i < terms; i++) {
    char s[100];
    if (sigmasValid || repeatFits) {
      sprintf(s, "Coefficient%02ldSigma", (long)order[i]);
      iTermSig[i] = SDDS_DefineParameter(SDDSout, s, s, coefUnits[i], NULL,
                                         NULL, SDDS_DOUBLE, NULL);
    } else {
      iTermSig[i] = -1;
    }
  }

  if (SDDS_NumberOfErrors())
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (copyParameters &&
      !SDDS_TransferAllParameterDefinitions(SDDSout, SDDSin, 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  return coefUnits;
}

long setCoefficientData(SDDS_DATASET *SDDSout, double *coef, double *coefSigma,
                        char **coefUnits, int32_t *order, long terms, long chebyshev,
                        char *fitLabelFormat, char *rpnSeqBuffer) {
  long termIndex, i;
  long invalid = 0;
  static char fitLabelBuffer[SDDS_MAXLINE];

  if (chebyshev != 2) {
    createRpnSequence(rpnSeqBuffer, SDDS_MAXLINE, coef, order, terms);
    if (!SDDS_SetArrayVararg(SDDSout, "Order", SDDS_POINTER_ARRAY, order,
                             terms) ||
        !SDDS_SetArrayVararg(SDDSout, "Coefficient", SDDS_POINTER_ARRAY, coef,
                             terms) ||
        (coefSigma &&
         !SDDS_SetArrayVararg(SDDSout, "CoefficientSigma", SDDS_POINTER_ARRAY,
                              coefSigma, terms)) ||
        !SDDS_SetArrayVararg(SDDSout, "CoefficientUnits", SDDS_POINTER_ARRAY,
                             coefUnits, terms))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    termIndex = coefficient_index(order, terms, 0);

    if (iIntercept != -1 &&
        !SDDS_SetParameters(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                            iIntercept, invalid ? NaN : coef[termIndex], -1))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (iInterceptSigma != -1 &&
        !SDDS_SetParameters(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                            iInterceptSigma,
                            invalid ? NaN : coefSigma[termIndex], -1))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!invalid)
      termIndex = coefficient_index(order, terms, 1);
    if (iSlope != -1 &&
        !SDDS_SetParameters(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                            iSlope, invalid ? NaN : coef[termIndex], -1))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (iSlopeSigma != -1 &&
        !SDDS_SetParameters(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                            iSlopeSigma, invalid ? NaN : coefSigma[termIndex],
                            -1))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!invalid)
      termIndex = coefficient_index(order, terms, 2);
    if (iCurvature != -1 &&
        !SDDS_SetParameters(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                            iCurvature, invalid ? NaN : coef[termIndex], -1))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (iCurvatureSigma != -1 &&
        !SDDS_SetParameters(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                            iCurvatureSigma,
                            invalid ? NaN : coefSigma[termIndex], -1))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (iFitLabel != -1 && !invalid) {
      makeFitLabel(fitLabelBuffer, SDDS_MAXLINE, fitLabelFormat, coef, coefSigma, order,
                   terms, chebyshev);
      if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                              iFitLabel, fitLabelBuffer, -1))
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    for (i = 0; i < terms; i++) {
      if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                              iTerm[i], coef[i], -1))
        SDDS_PrintErrors(stderr,
                         SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (iTermSig[i] != -1)
        if (!SDDS_SetParameters(SDDSout,
                                SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                iTermSig[i], coefSigma[i], -1))
          SDDS_PrintErrors(stderr,
                           SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  } else {
    long termsC;
    int32_t *orderC;
    double *coefC, *coefSigmaC;
    convertFromChebyshev(terms, order, coef, coefSigma, &termsC, &orderC, &coefC, &coefSigmaC);
    setCoefficientData(SDDSout, coefC, coefSigmaC, coefUnits, orderC, termsC, 0, fitLabelFormat,
                       rpnSeqBuffer);
  }

  return 1;
}

char **makeCoefficientUnits(SDDS_DATASET *SDDSout, char *xName, char *yName,
                            int32_t *order, long terms) {
  char *xUnits, *yUnits, buffer[SDDS_MAXLINE];
  char **coefUnits = NULL;
  long i;

  if (!SDDS_GetColumnInformation(SDDSout, "units", &xUnits, SDDS_GET_BY_NAME,
                                 xName) ||
      !SDDS_GetColumnInformation(SDDSout, "units", &yUnits, SDDS_GET_BY_NAME,
                                 yName))
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
          sprintf(buffer, "1/%s$a%d$n", xUnits, order[i] - 1);
        else
          strcpy(buffer, "");
        SDDS_CopyString(coefUnits + i, buffer);
      } else {
        if (order[i] > 1)
          sprintf(buffer, "%s/%s$a%d$n", yUnits, xUnits, order[i]);
        else
          sprintf(buffer, "%s/%s", yUnits, xUnits);
        SDDS_CopyString(coefUnits + i, buffer);
      }
    }
  }
  return coefUnits;
}

void compareOriginalToFit(double *x, double *y, double **residual,
                          int64_t points, double *rmsResidual, double *coef,
                          int32_t *order, long terms) {
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

void makeEvaluationTable(EVAL_PARAMETERS *evalParameters, double *x,
                         int64_t points, double *coef, int32_t *order,
                         long terms, SDDS_DATASET *SDDSin, char *xName,
                         char *yName) {
  double *xEval, *yEval, delta;
  int64_t i;
  yEval = NULL;
  if (!evalParameters->initialized) {
    if (!SDDS_InitializeOutput(&evalParameters->dataset, SDDS_BINARY, 0, NULL,
                               "sddspfit evaluation table",
                               evalParameters->file) ||
        !SDDS_TransferColumnDefinition(&evalParameters->dataset, SDDSin, xName,
                                       NULL) ||
        !SDDS_TransferColumnDefinition(&evalParameters->dataset, SDDSin, yName,
                                       NULL) ||
        !SDDS_WriteLayout(&evalParameters->dataset))
      SDDS_PrintErrors(stderr,
                       SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    evalParameters->initialized = 1;
  }

  if (evalParameters->flags & EVAL_VALUESFILE_GIVEN) {
    if (!evalParameters->inputInitialized) {
      if (!SDDS_InitializeInput(&(evalParameters->valuesDataset), evalParameters->valuesFile)) {
        fprintf(stderr, "error: unable to initialize %s\n", evalParameters->valuesFile);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!SDDS_ReadPage(&(evalParameters->valuesDataset))) {
        fprintf(stderr, "error: unable to read page from %s\n", evalParameters->valuesFile);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      evalParameters->inputInitialized = 1;
    } else {
      if (!(evalParameters->flags & EVAL_REUSE_PAGE_GIVEN) &&
          !SDDS_ReadPage(&(evalParameters->valuesDataset))) {
        fprintf(stderr, "error: unable to read page from %s\n", evalParameters->valuesFile);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    if (!(xEval = SDDS_GetColumnInDoubles(&(evalParameters->valuesDataset), evalParameters->valuesColumn))) {
      fprintf(stderr, "error: unable to read column %s from %s\n", evalParameters->valuesColumn, evalParameters->valuesFile);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    evalParameters->number = SDDS_CountRowsOfInterest(&(evalParameters->valuesDataset));
  } else {
    if (!(evalParameters->flags & EVAL_BEGIN_GIVEN) ||
        !(evalParameters->flags & EVAL_END_GIVEN)) {
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
      delta = (evalParameters->end - evalParameters->begin) /
        (evalParameters->number - 1);
    else
      delta = 0;

    if (!(xEval = (double *)malloc(sizeof(*xEval) * evalParameters->number)))
      SDDS_Bomb("allocation failure");

    for (i = 0; i < evalParameters->number; i++)
      xEval[i] = evalParameters->begin + i * delta;
  }

  if (!(yEval = (double *)malloc(sizeof(*yEval) * evalParameters->number)))
    SDDS_Bomb("allocation failure");
  for (i = 0; i < evalParameters->number; i++)
    yEval[i] = eval_sum(basis_fn, coef, order, terms, xEval[i]);

  if (!SDDS_StartPage(&evalParameters->dataset, evalParameters->number) ||
      !SDDS_SetColumnFromDoubles(&evalParameters->dataset, SDDS_SET_BY_NAME,
                                 xEval, evalParameters->number, xName) ||
      !SDDS_SetColumnFromDoubles(&evalParameters->dataset, SDDS_SET_BY_NAME,
                                 yEval, evalParameters->number, yName) ||
      !SDDS_WritePage(&evalParameters->dataset))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(xEval);
  free(yEval);
}

long reviseFitOrders(double *x, double *y, double *sy, int64_t points,
                     long terms, int32_t *order, double *coef,
                     double *coefSigma, double *diff,
                     double (*basis_fn)(double xa, long ordera),
                     unsigned long reviseOrders, double xOffset,
                     double xScaleFactor, long normTerm, long ySigmasValid,
                     long chebyshev, double revpowThreshold,
                     double revpowCompleteThreshold, double goodEnoughChi) {
  double bestChi, chi;
  long bestTerms, newTerms, newBest, *termUsed;
  int32_t *newOrder, *bestOrder;
  long i, ip, j;
  long origTerms, *origOrder;

  bestOrder = tmalloc(sizeof(*bestOrder) * terms);
  newOrder = tmalloc(sizeof(*newOrder) * terms);
  termUsed = tmalloc(sizeof(*termUsed) * terms);
  origOrder = tmalloc(sizeof(*origOrder) * terms);
  origTerms = terms;
  for (i = 0; i < terms; i++)
    origOrder[i] = order[i];
  qsort((void *)order, terms, sizeof(*order), long_cmpasc);
  bestOrder[0] = newOrder[0] = order[0];
  termUsed[0] = 1;
  newTerms = bestTerms = 1;
  /* do a fit */
  if (!lsfg(x, y, sy, points, newTerms, newOrder, coef, coefSigma, &bestChi,
            diff, basis_fn))
    SDDS_Bomb("revise-orders fit failed.");
  if (reviseOrders & REVPOW_VERBOSE) {
    fputs("fit to revise orders:", stdout);
    print_coefs(stdout, xOffset, xScaleFactor, chebyshev, coef,
                (ySigmasValid ? coefSigma : NULL), bestOrder, bestTerms,
                bestChi, normTerm, "");
    fprintf(stdout, "unweighted rms deviation from fit: %21.15e\n",
            rms_average(diff, points));
  }

  do {
    newBest = 0;
    newTerms = newTerms + 1;
    for (ip = 1; ip < terms; ip++) {
      if (termUsed[ip])
        continue;
      newOrder[newTerms - 1] = order[ip];
      if (!lsfg(x, y, sy, points, newTerms, newOrder, coef, coefSigma, &chi,
                diff, basis_fn))
        SDDS_Bomb("revise-orders fit failed.");
      if (reviseOrders & REVPOW_VERBOSE) {
        fputs("trial fit:", stdout);
        print_coefs(stdout, xOffset, xScaleFactor, chebyshev, coef,
                    (ySigmasValid ? coefSigma : NULL), newOrder, newTerms,
                    chi, normTerm, "");
        fprintf(stdout, "unweighted rms deviation from fit: %21.15e\n",
                rms_average(diff, points));
      }
      if ((bestChi > goodEnoughChi && chi < bestChi) ||
          (chi + revpowThreshold < bestChi && newTerms < bestTerms)) {
        bestChi = chi;
        bestTerms = newTerms;
        newBest = 1;
        termUsed[ip] = 1;
        for (i = 0; i < newTerms; i++)
          bestOrder[i] = newOrder[i];
        if (reviseOrders & REVPOW_VERBOSE) {
          fputs("new best fit:", stdout);
          print_coefs(stdout, xOffset, xScaleFactor, chebyshev, coef,
                      (ySigmasValid ? coefSigma : NULL), bestOrder, bestTerms,
                      bestChi, normTerm, "");
          fprintf(stdout, "unweighted rms deviation from fit: %21.15e\n",
                  rms_average(diff, points));
        }
        break;
      }
    }
  } while (newBest && bestChi > goodEnoughChi);

  terms = bestTerms;
  for (ip = 0; ip < terms; ip++)
    order[ip] = bestOrder[ip];

  if (newBest) {
    do {
      newBest = 0;
      for (ip = 0; ip < terms; ip++) {
        for (i = j = 0; i < terms; i++) {
          if (i != ip)
            newOrder[j++] = order[i];
        }
        newTerms = terms - 1;
        if (!lsfg(x, y, sy, points, newTerms, newOrder, coef, coefSigma, &chi, diff,
                  basis_fn))
          SDDS_Bomb("revise-orders fit failed.");
        if ((bestChi > goodEnoughChi && chi < goodEnoughChi) ||
            (chi + revpowThreshold < bestChi && newTerms < terms)) {
          bestChi = chi;
          terms = newTerms;
          newBest = 1;
          for (i = 0; i < newTerms; i++)
            order[i] = newOrder[i];
          if (reviseOrders & REVPOW_VERBOSE) {
            fputs("new best fit:", stdout);
            print_coefs(stdout, xOffset, xScaleFactor, chebyshev, coef,
                        (ySigmasValid ? coefSigma : NULL), order, terms,
                        bestChi, normTerm, "");
            fprintf(stdout, "unweighted rms deviation from fit: %21.15e\n",
                    rms_average(diff, points));
          }
          break;
        }
      }
    } while (newBest && terms > 1 && bestChi > goodEnoughChi);
  }

  free(bestOrder);
  free(termUsed);
  free(newOrder);

  if ((reviseOrders & REVPOW_COMPLETE) && bestChi > revpowCompleteThreshold) {
    terms = origTerms;
    for (i = 0; i < origTerms; i++)
      order[i] = origOrder[i];
    if (reviseOrders & REVPOW_VERBOSE)
      fprintf(stdout, "Result unsatisfactory---attempting complete trials\n");
    return reviseFitOrders1(x, y, sy, points, terms, order, coef, coefSigma,
                            diff, basis_fn, reviseOrders, xOffset, xScaleFactor,
                            normTerm, ySigmasValid, chebyshev, revpowThreshold,
                            goodEnoughChi);
  }

  free(origOrder);
  return terms;
}

long reviseFitOrders1(double *x, double *y, double *sy, int64_t points,
                      long terms, int32_t *order, double *coef,
                      double *coefSigma, double *diff,
                      double (*basis_fn)(double xa, long ordera),
                      unsigned long reviseOrders, double xOffset,
                      double xScaleFactor, long normTerm, long ySigmasValid,
                      long chebyshev, double revpowThreshold,
                      double goodEnoughChi) {
  double bestChi, chi;
  long bestTerms, newTerms;
  int32_t *newOrder = NULL, *bestOrder;
  long i, ip, j;
  long *counter = NULL, *counterLim = NULL;

  if (!(bestOrder = malloc(sizeof(*bestOrder) * terms)) ||
      !(newOrder = malloc(sizeof(*newOrder) * terms)) ||
      !(counter = calloc(sizeof(*counter), terms)) ||
      !(counterLim = calloc(sizeof(*counterLim), terms))) {
    fprintf(stderr, "Error: memory allocation failure (%ld terms)\n", terms);
    SDDS_Bomb(NULL);
  }
  for (i = 0; i < terms; i++)
    counterLim[i] = 2;
  qsort((void *)order, terms, sizeof(*order), long_cmpasc);
  /* do a fit */
  if (!lsfg(x, y, sy, points, 2, order, coef, coefSigma, &bestChi, diff,
            basis_fn))
    SDDS_Bomb("revise-orders fit failed.");
  for (i = 0; i < 2; i++)
    bestOrder[i] = order[i];
  bestTerms = 2;
  if (reviseOrders & REVPOW_VERBOSE) {
    fputs("starting fit to revise orders:", stdout);
    print_coefs(stdout, xOffset, xScaleFactor, chebyshev, coef,
                (ySigmasValid ? coefSigma : NULL), order, 1, bestChi, normTerm,
                "");
    fprintf(stdout, "unweighted rms deviation from fit: %21.15e\n",
            rms_average(diff, points));
  }

  newTerms = 1;
  while (advance_counter(counter, counterLim, terms) >= 0) {
    for (i = j = 0; i < terms; i++) {
      if (counter[i])
        newOrder[j++] = order[i];
    }
    newTerms = j;
    if (!lsfg(x, y, sy, points, newTerms, newOrder, coef, coefSigma, &chi, diff,
              basis_fn))
      SDDS_Bomb("revise-orders fit failed.");
    if ((chi < goodEnoughChi && newTerms < bestTerms) ||
        (bestChi > goodEnoughChi && chi < bestChi)) {
      bestChi = chi;
      bestTerms = newTerms;
      for (i = 0; i < newTerms; i++)
        bestOrder[i] = newOrder[i];
      if (reviseOrders & REVPOW_VERBOSE) {
        fputs("new best fit:", stdout);
        print_coefs(stdout, xOffset, xScaleFactor, chebyshev, coef,
                    (ySigmasValid ? coefSigma : NULL), bestOrder, bestTerms,
                    bestChi, normTerm, "");
        fprintf(stdout, "unweighted rms deviation from fit: %21.15e\n",
                rms_average(diff, points));
      }
    }
  }

  terms = bestTerms;
  for (ip = 0; ip < terms; ip++)
    order[ip] = bestOrder[ip];

  free(bestOrder);
  free(newOrder);
  free(counter);
  free(counterLim);
  return terms;
}

void createRpnSequence(char *buffer, long bufsize, double *coef, int32_t *order,
                       long terms) {
  long i, j, maxOrder;
  static char buffer1[SDDS_MAXLINE];
  double coef1;
  double offset;

  buffer[0] = 0;
  maxOrder = 0;
  for (i = 0; i < terms; i++) {
    if (maxOrder < order[i])
      maxOrder = order[i];
  }
  if (maxOrder == 0) {
    snprintf(buffer, SDDS_MAXLINE, "%.15e", coef[0]);
    return;
  }
  offset = get_argument_offset();
  if (offset != 0)
    snprintf(buffer, SDDS_MAXLINE, "%le - ", offset);
  for (i = 2; i <= maxOrder; i++) {
    strcat(buffer, "= ");
    if ((strlen(buffer) + 2) > bufsize) {
      fprintf(stderr, "buffer overflow making rpn expression!\n");
      return;
    }
  }
  for (i = maxOrder; i >= 0; i--) {
    for (j = 0; j < terms; j++) {
      if (order[j] == i)
        break;
    }
    if (j == terms)
      coef1 = 0;
    else
      coef1 = coef[j];
    if (i == maxOrder)
      snprintf(buffer1, SDDS_MAXLINE, "%.15g * ", coef1);
    else if (i == 0 && order[j] == 0) {
      if (coef1 != 0)
        snprintf(buffer1, SDDS_MAXLINE, "%.15g + ", coef1);
      else
        continue;
    } else {
      if (coef1 == 0)
        strcpy(buffer1, "* ");
      else
        snprintf(buffer1, SDDS_MAXLINE, "%.15g + * ", coef1);
    }
    if ((strlen(buffer) + strlen(buffer1)) >= bufsize) {
      fprintf(stderr, "buffer overflow making rpn expression!\n");
      return;
    }
    strcat(buffer, buffer1);
  }
}

/* Make array of structures giving coefficients for Chebyshev T polynomials,
 * which allows converting to ordinary polynomials.
 */

CHEBYSHEV_COEF *makeChebyshevCoefficients(long maxOrder, long *nPoly) {
  CHEBYSHEV_COEF *coef;
  long i, j;

  if (maxOrder < 2)
    *nPoly = 2;
  else
    *nPoly = maxOrder + 1;

  coef = tmalloc(sizeof(*coef) * (*nPoly));

  coef[0].nTerms = 1;
  coef[0].coef = tmalloc(sizeof(*(coef[0].coef)) * coef[0].nTerms);
  coef[0].coef[0] = 1;

  coef[1].nTerms = 2;
  coef[1].coef = tmalloc(sizeof(*(coef[1].coef)) * coef[1].nTerms);
  coef[1].coef[0] = 0;
  coef[1].coef[1] = 1;

  for (i = 2; i < *nPoly; i++) {
    coef[i].nTerms = coef[i - 1].nTerms + 1;
    coef[i].coef = calloc(coef[i].nTerms, sizeof(*(coef[i].coef)));
    for (j = 0; j < coef[i - 2].nTerms; j++)
      coef[i].coef[j] = -coef[i - 2].coef[j];
    for (j = 0; j < coef[i - 1].nTerms; j++)
      coef[i].coef[j + 1] += 2 * coef[i - 1].coef[j];
  }
  /*
    for (i = 0; i < *nPoly; i++) {
    printf("T%ld: ", i);
    for (j = 0; j < coef[i].nTerms; j++) {
    if (coef[i].coef[j])
    printf("%c%lg*x^%ld ", coef[i].coef[j] < 0 ? '-' : '+', fabs(coef[i].coef[j]), j);
    }
    printf("\n");
    }
  */
  return coef;
}

void convertFromChebyshev(long termsT, int32_t *orderT, double *coefT, double *coefSigmaT,
                          long *termsOrdinaryRet, int32_t **orderOrdinaryRet, double **coefOrdinaryRet, double **coefSigmaOrdinaryRet) {
  long i, maxOrder;
  long termsOrdinary;
  int32_t *orderOrdinary;
  double *coefOrdinary, *coefSigmaOrdinary, scale;
  static CHEBYSHEV_COEF *chebyCoef = NULL;
  static long nChebyCoef = 0, chebyMaxOrder = 0;

  maxOrder = 0;
  for (i = 0; i < termsT; i++)
    if (orderT[i] > maxOrder)
      maxOrder = orderT[i];

  termsOrdinary = maxOrder + 1;
  orderOrdinary = tmalloc(sizeof(*orderOrdinary) * termsOrdinary);
  coefOrdinary = calloc(termsOrdinary, sizeof(*coefOrdinary));
  if (coefSigmaT)
    coefSigmaOrdinary = calloc(termsOrdinary, sizeof(*coefSigmaOrdinary));
  else
    coefSigmaOrdinary = NULL;

  if (chebyCoef == NULL || maxOrder > chebyMaxOrder) {
    if (chebyCoef) {
      for (i = 0; i < nChebyCoef; i++)
        free(chebyCoef[i].coef);
      free(chebyCoef);
    }
    chebyCoef = makeChebyshevCoefficients(chebyMaxOrder = maxOrder, &nChebyCoef);
  }

  for (i = 0; i < termsT; i++) {
    long j;
    for (j = 0; j < chebyCoef[orderT[i]].nTerms; j++) {
      coefOrdinary[j] += coefT[i] * chebyCoef[i].coef[j];
      if (coefSigmaT)
        coefSigmaOrdinary[j] += sqr(coefSigmaT[i] * chebyCoef[i].coef[j]);
    }
  }
  scale = get_argument_scale();
  for (i = 0; i < termsOrdinary; i++) {
    if (coefSigmaT)
      coefSigmaOrdinary[i] = sqrt(coefSigmaOrdinary[i]) / ipow(scale, i);
    orderOrdinary[i] = i;
    coefOrdinary[i] /= ipow(scale, i);
  }
  *termsOrdinaryRet = termsOrdinary;
  *orderOrdinaryRet = orderOrdinary;
  *coefOrdinaryRet = coefOrdinary;
  *coefSigmaOrdinaryRet = coefSigmaOrdinary;
}
