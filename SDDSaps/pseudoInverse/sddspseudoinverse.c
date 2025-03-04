/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution.
\*************************************************************************/

/* Program sddspseudoinverse
 * Reads a sddsfile with one data set.
 * Finds all numerical columns and rows, and
 * makes a matrix out of them.
 * Finds the pseudo-inverse, and make a new data set.
 * The data in the first string column becomes the names of the columns in the output file.
 * The names of the columns of the input file become data for the string column of the output file.
 *
 * Added nlargestsingularvalues 2/20/95
 * Added calls to the meschach library from netlib. 7/12/95
 * The original non-meschach version used a call to a function
 * derived from a routine in "Numerical Recipes in C". Since code
 * from this book isn't free, the original version of sddspseudoinverse
 * can't be distributed outside the lab.
 * L.Emery, ANL, 1994
 */

#if defined(__APPLE__)
#  define ACCELERATE_NEW_LAPACK
#  include <Accelerate/Accelerate.h>
#endif
#include "mdb.h"
#include "scan.h"
#include "match_string.h"

#ifdef SUNPERF
#  include <sunperf.h>
#endif

#ifdef MKL
#  include <omp.h>
#  include "mkl.h"
int dgemm_(char *transa, char *transb, const MKL_INT *m, const MKL_INT *n, const MKL_INT *k, double *alpha, double *a, const MKL_INT *lda, double *b, const MKL_INT *ldb, double *beta, double *c, const MKL_INT *ldc);
#endif

#ifdef CLAPACK
#  if !defined(_WIN32) && !defined(__APPLE__)
#    include "cblas.h"
#  endif
#  ifdef F2C
#    include "f2c.h"
#  endif
#  if !defined(__APPLE__)
#    include "clapack.h"
#  endif
#  if defined(_WIN32)
int f2c_dgemm(char *transA, char *transB, integer *M, integer *N, integer *K,
              doublereal *alpha,
              doublereal *A, integer *lda,
              doublereal *B, integer *ldb,
              doublereal *beta,
              doublereal *C, integer *ldc);
#  endif
#endif

#ifdef LAPACK
#  include "f2c.h"
#  include <lapacke.h>
int dgemm_(char *transa, char *transb, lapack_int *m, lapack_int *n, lapack_int *k, double *alpha, double *a, const lapack_int *lda, double *b, const lapack_int *ldb, double *beta, double *c, const lapack_int *ldc);
#endif

/* matrix.h requires machine.h  err.h and meminfo.h. */
#include "matrix.h"
#include "matrix2.h"
#include <setjmp.h>
#include "err.h"
/* for some reason, this function was not prototyped
   in the matrix2.h header file */
char *setformat(char *f_string);

#ifdef NUMERICAL_RECIPES
int nr_svd(MAT *a, VEC *values, MAT *v);
static void fix_nr_svd(MAT *R, VEC *values, MAT *Vt);
#endif

#include "SDDS.h"

#define CLO_MINIMUM_SINGULAR_VALUE_RATIO 0
#define CLO_VERBOSE 1
#define CLO_COLUMNROOT 2
#define CLO_SYMBOL 3
#define CLO_KEEP_SINGULAR_VALUES 4
#define CLO_ASCII 5
#define CLO_DIGITS 6
#define CLO_PIPE 7
#define CLO_OLDCOLUMNNAMES 8
#define CLO_NEWCOLUMNNAMES 9
#define CLO_UMATRIX 10
#define CLO_VMATRIX 11
#define CLO_REMOVEDCVECTORS 12
#define CLO_WEIGHT 13
#define CLO_NOWARNINGS 14
#define CLO_SMATRIX 15
#define CLO_RECONSTRUCT 16
#define CLO_DELETEVECTORS 17
#define CLO_REMOVE_SINGULAR_VALUES 18
#define CLO_ECONOMY 19
#define CLO_PRINTPACKAGE 20
#define CLO_MULTIPLY_MATRIX 21
#define CLO_MAJOR_ORDER 22
#define CLO_LAPACK_METHOD 23
#define CLO_CORRECTOR_WEIGHT 24
#define CLO_THREADS 25
#define CLO_TIKHONOV 26
#define COMMANDLINE_OPTIONS 27
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "minimumSingularValueRatio",
  "verbose",
  "root",
  "symbol",
  "largestSingularValues",
  "ascii",
  "digits",
  "pipe",
  "oldColumnNames",
  "newColumnNames",
  "uMatrix",
  "vMatrix",
  "removeDCVectors",
  "weights",
  "noWarnings",
  "sFile",
  "reconstruct",
  "deleteVectors",
  "smallestSingularValues",
  "economy",
  "printPackage",
  "multiplyMatrix",
  "majorOrder",
  "lapackMethod",
  "correctorWeight",
  "threads",
  "tikhonov",
};

static char *USAGE = "sddspseudoinverse [<input>] [<output>] [-pipe=[input][,output]]\n\
    [{-minimumSingularValueRatio=<value> | -largestSingularValues=<number>}] \n\
    [-smallestSingularValues=<number>] [-tikhonov=[svn=<number>|alpha=<value>|beta=<value>]\n\
    [-deleteVectors=<list of vectors separated by comma>] \n\
    [-economy] [-printPackage] [-threads=<number>]\n\
    [-oldColumnNames=<string>] [{-root=<string> [-digits=<integer>] | \n\
    -newColumnNames=<column>}] [-sFile=<file>[,matrix]] [-uMatrix=<file>] [-vMatrix=<file>] \n\
    [-weights=<file>,name=<columnname>,value=<columnname>] \n\
    [-correctorWeights=<file>,name=<columnname>,value=<columnname>] \n\
    [-majorOrder=row|column] [-lapackMethod={simple|divideAndConquer}] \n\
    [-reconstruct=<file>] [-symbol=<string>] [-ascii] [-verbose] [-noWarnings] [-multiplyMatrix=<file>[,invert]]\n\n";
static char *USAGE2 = "Takes the generalized inverse of a matrix in a SDDS file.\n\
pipe           reads input from and/or write output to a pipe.\n\
minimumSingularValueRatio\n\
               rejects singular values less than the largest\n\
               singular value times this ratio.\n\
largestSingularValues\n\
               retains only the first \"largestSingularValues\"\n\
               largest singularvalues.\n\
smallestSingularValues\n\
               remove the the last \"smallestSingularValues\" smallest singularvalues. \n\
tikhonov=[svn=<number>|alpha=<value>|beta=<value>]\n\
               use Tikhonov regularization to filter singular values, where InvS=Singluar/(Singular^2+alpha^2).\n\
               either svn number or alpha or beta can be provided. \
               if none are not provided, defalut alpha=0.01 will be used.\n		\
               if svn number is provided, the <svn>-th singular value will be used as alpha.\n\
               if beta is provided, then alpha=Max(SV)*beta \n\
deleteVectors  -deleteVectors=n1,n2,n3,... which will set the inverse singular values \n\
               of modes n1,n2,n3, ect to zero. \n\
               The order in which the SV removal options are processed is \n\
               minimumSingularValueRatio, largestSingularValues and then deleteVectors.\n\
economy        only the first min(m,n) columns for the U matrix are calculated or returned \n\
               where m is the number of rows and n is the number of columns. This \n\
               can potentially reduce the computation time with no loss of useful information.\n\
               economy option is highly recommended for most pratical applications since it uses\n\
               less memory and runs faster. If economy option is not give, a full m by m U matrix \n\
               will be internally computated no matter whether -uMatrix is provided. \n\
lapackMethod   give option of calling lapack svd routine, \"simple\" for dgesvd, and \"divideAndConquer\" \n\
               for dgesdd, the later is claimed to have better performance than the former.\n\
oldColumnNames\n\
               specifies a name for the output file string column created for\n\
               the input file column names.\n\
multiplyMatrix if invert is not provided,  then the output matrix is the inverse of the input\n\
               matrix multiplying by this matrix; otherwise, the output matrix is the product of \n\
               multiply matrix and the inverse of the input matrix.\n\
majorOrder     specity output file in row or column major order.\n";
static char *USAGE3 = "root           use the string specified to generate column names.\n\
               Default for column names is the first string column in\n\
               <inputfile>. If there is no string column, then the column\n\
               names are formed with the root \"Column\".\n\
digits         minimum number of digits used in the number appended to the root\n\
               part of the column names. Default is value 3.\n\
newColumnNames use the column specified as the source for new column names.\n\
sFile, uMatrix, vMatrix writes the u and v column-orthogonal matrices \n\
               and the singular values vector to files. \n\
               The SVD decomposition follows the convention A = u (SValues) v^T \n\
               The \"transformed\" x are v^T x, and the \"transformed\" y are u^T y.\n\
correctorWeights Specifies file which contains correctors weights for each of the columns\n\
               of the matrix, thus giving different weights for solving the\n\
               linear equations that the pseudoinverse problem represent.\n\
weights        Specifies file which contains BPM weights for each of the rows\n\
               of the matrix, thus giving different weights for solving the\n\
               linear equations that the pseudoinverse problem represent.\n";

static char *USAGE4 = "               The equation to solve is wAx = wy where w is the weight vector\n\
               turned into a diagonal matrix and A is the matrix. \n\
               The matrix solution returned is (wA)^I w where ^I means taking \n\
               the pseudoinverse. The u matrix now has a different interpretation:\n\
               the \"transformed\" x are v^T x, as before, but the \"transformed\" y are u^T w y.\n\
symbol         use the string specified for the symbol field for all columns definitions.\n\
reconstruct    speficy a file which will reconstruct the original matrix with only the\n\
               singular values retained in the inversion.\n\
ascii          writes the output file data in ascii mode (default is binary).\n\
verbose        prints out to stderr input and output matrices.\n\
printPackage   prints out the linear algebra package that was compiled.\n\
noWarnings     prevents printing of warning messages.\n\
Program by Louis Emery, ANL ("__DATE__
                             ")\n";

#define FL_VERBOSE 1
#define FL_VERYVERBOSE 2

MAT *m_diag(VEC *diagElements, MAT *A);

long SDDS_FreeDataPage(SDDS_DATASET *SDDS_dataset);
int m_freePointers(MAT *mat);
int t_free(MAT *mat);

#if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
void *SDDS_GetCastMatrixOfRows_SunPerf(SDDS_DATASET *SDDS_dataset, int32_t *n_rows, long sddsType);
#endif

int32_t InitializeInputAndGetColumnNames(SDDS_DATASET *SDDS_dataset, char *filename,
                                         char ***numericalColumnName, int32_t *numericalColumns,
                                         char **stringColumName, char **inputDescription, char **inputContents);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE inputPage, outputPage, uPage, vPage, sPage, reconstructPage, weightsPage, multiplyPage;

  char *inputfile, *outputfile, *uFile, *vFile, *sFile, *weightsFile, *reconstructFile, *multiplyFile, *corrWeightsFile;
  char **outputColumnName = NULL, **orthoColumnName = NULL, **actuatorName = NULL, **numericalColumnName = NULL, **multiplyColumnName = NULL;
  char *weightsNamesColumn, *weightsValuesColumn, *stringColumnName = NULL, *multiStringCol = NULL;
  char **weightsColumnName, **weightsName = NULL;
  char **corrWeightsColumnName, **corrWeightsName = NULL;
  char *corrWeightsNamesColumn, *corrWeightsValuesColumn;
  double *weights, largestS;
  int32_t rows = 0, multiplyRows = 0, outpageRows = 0, actuators = 0, outputColumns = 0;
  int32_t rowsFirstPage = 0, numericalColumns, weightsRows=0, weightsColumns, multiplyColumns, freeMultiCol = 0;
  int32_t corrWeightsRows = 0, corrWeightsColumns;
  long sFileAsMatrix;
  char *inputDescription = NULL, *inputContents = NULL;
  char *outputDescription = NULL;
  long i_arg;
  register long i, j;
#if defined(NUMERICAL_RECIPES) || defined(SUNPERF)
  register k;
#else
#  if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
#  else
  register k;
#  endif
#endif
  int32_t dim_ptr[1] = {1};
  long removeDCVectors, includeWeights, verbose, includeCorrWeights;
  char format[80];
  char *Symbol, *Root;
  VEC *w = NULL, *SValue, *SValueUsed, *InvSValue, *corrW = NULL;
  MAT *R, *RInvt, *RInvR, *Rnewt, *U, *Ut, *V, *Vt, *Multi = NULL, *Product = NULL;

  double ratio;
  long nlargest, nsmallest, t_svn = 0, tikhonov = 0;
  double t_alpha = 0.01, t_beta = 0;
  long NSVUsed;
  long ascii;
  long digits, foundStringColumn = 0, invertMultiply = 0, strColIndex = -1;
  unsigned long pipeFlags;
  long tmpfile_used, noWarnings;
  long ipage, row_match, mpage = 0;
  char *oldColumnNames, *newColumnNamesColumn;
  double conditionNumber, max, min;
  unsigned long flags, majorOrderFlag;
  long *deleteVector, deleteVectors, firstdelete;
  char deletedVector[1024];
  long printPackage;
  short columnMajorOrder = -1, lapackMethod = 1;
#if defined(LAPACK)
  lapack_int *iwork = NULL;
#else
  int *iwork = NULL;
#endif
  /* Flag economy changes the calculation mode of the lapack-type calls to svd
     and may later be used by a modified meschach svd routine to
     reduce the number of columns returned for the U matrix.
     The ecomony mode is already the only mode for NR and for sunperf. */
  long economy, economyRows;

#if defined(NUMERICAL_RECIPES) || defined(SUNPERF)
  register Real x;
#endif

#if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
  int info;
  /* default is standard svd calculation with square U matrix */
  char calcMode = 'A';
#endif
#if defined(CLAPACK) || defined(MKL)
  double *work;
  long lwork;
  long lda;
  double alpha = 1.0, beta = 0.0;
  int kk, ldb;

  MAT *tmpR = NULL;
#endif
#if defined(LAPACK)
  doublereal *work;
  lapack_int lwork;
  lapack_int lda;
  double alpha = 1.0, beta = 0.0;
  int kk, ldb;

  MAT *tmpR = NULL;
#endif
  long threads = 1;

  SValue = SValueUsed = InvSValue = (VEC *)NULL;
  R = RInvt = RInvR = Rnewt = U = Ut = V = Vt = (MAT *)NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s", USAGE, USAGE2, USAGE3, USAGE4);
    exit(1);
  }
  deletedVector[0] = '\0';
  firstdelete = 1;
  inputfile = outputfile = multiplyFile = NULL;
  verbose = 0;
  Symbol = Root = NULL;
  uFile = vFile = sFile = reconstructFile = NULL;
  weightsFile = corrWeightsFile = NULL;
  weightsNamesColumn = weightsValuesColumn = corrWeightsNamesColumn = corrWeightsValuesColumn = NULL;
  ratio = 0;
  nlargest = 0;
  nsmallest = 0;
  deleteVectors = 0;
  deleteVector = NULL;
  ascii = 0;
  digits = 3;
  pipeFlags = 0;
  tmpfile_used = 0;
  noWarnings = 0;
  oldColumnNames = NULL;
  newColumnNamesColumn = NULL;
  conditionNumber = 0.0;
  removeDCVectors = 0;
  includeWeights = includeCorrWeights = 0;
  sFileAsMatrix = 0;
  economy = 0;
  printPackage = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, UNIQUE_MATCH)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER,
                           NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_TIKHONOV:
        if (s_arg[i_arg].n_items > 2)
          SDDS_Bomb("Invalid -tikhonov syntax, only one of svn number and alpha value may be provided!");
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "svn", SDDS_LONG, &t_svn, 1, 0,
                           "alpha", SDDS_DOUBLE, &t_alpha, 1, 0,
                           "beta", SDDS_DOUBLE, &t_beta, 1, 0,
                           NULL)))
          SDDS_Bomb("invalid -tikhonov syntax");
        tikhonov = 1;
        break;
      case CLO_MINIMUM_SINGULAR_VALUE_RATIO:
        if (!(get_double(&ratio, s_arg[i_arg].list[1])))
          bomb("no string given for option -minimumsingularvalueratio", USAGE);
        break;
      case CLO_KEEP_SINGULAR_VALUES:
        if (!(get_long(&nlargest, s_arg[i_arg].list[1])))
          bomb("no string given for option -largestsingularvalues", USAGE);
        break;
      case CLO_REMOVE_SINGULAR_VALUES:
        if (!(get_long(&nsmallest, s_arg[i_arg].list[1])))
          bomb("no string given for option -smallestSingularvalues", USAGE);
        break;
      case CLO_THREADS:
        if (!(get_long(&threads, s_arg[i_arg].list[1])))
          bomb("no number given for option -threads", USAGE);
        break;
      case CLO_ASCII:
        ascii = 1;
        break;
      case CLO_NOWARNINGS:
        noWarnings = 1;
        break;
      case CLO_DIGITS:
        if (!(get_long(&digits, s_arg[i_arg].list[1])))
          bomb("no string given for option -digits", USAGE);
        break;
      case CLO_COLUMNROOT:
        if (!(Root = s_arg[i_arg].list[1]))
          bomb("No root string given", USAGE);
        break;
      case CLO_SYMBOL:
        if (!(Symbol = s_arg[i_arg].list[1]))
          bomb("No symbol string given", USAGE);
        break;
      case CLO_SMATRIX:
        if (s_arg[i_arg].n_items < 2 ||
            !(sFile = s_arg[i_arg].list[1]))
          bomb("No sMatrix string given", USAGE);
        if (s_arg[i_arg].n_items > 2) {
          if (s_arg[i_arg].n_items == 3 && strncmp(s_arg[i_arg].list[2], "matrix", strlen(s_arg[i_arg].list[2])) == 0) {
            sFileAsMatrix = 1;
          } else
            bomb("Invalid sMatrix syntax", USAGE);
        }
        break;
      case CLO_ECONOMY:
#if defined(NUMERICAL_RECIPES) || defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
        economy = 1;
#else
        if (!noWarnings) {
          fprintf(stderr, "Warning: -economy option not available in this compilation.\n");
          printPackage = 1;
        }
        economy = 0;
#endif
        break;
      case CLO_UMATRIX:
        if (s_arg[i_arg].n_items < 2 || !(uFile = s_arg[i_arg].list[1]))
          bomb("No uMatrix string given", USAGE);
        break;
      case CLO_VMATRIX:
        if (!(vFile = s_arg[i_arg].list[1]))
          bomb("No vMatrix string given", USAGE);
        break;
      case CLO_RECONSTRUCT:
        if (!(reconstructFile = s_arg[i_arg].list[1]))
          bomb("No reconstruct string given", USAGE);
        break;
      case CLO_WEIGHT:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -weight syntax");
        weightsFile = s_arg[i_arg].list[1];
        includeWeights = 1;
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &weightsNamesColumn, 1, 0,
                          "value", SDDS_STRING, &weightsValuesColumn, 1, 0,
                          NULL))
          SDDS_Bomb("invalid -weights syntax");
        break;
      case CLO_CORRECTOR_WEIGHT:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("invalid -correctorWeight syntax");
        corrWeightsFile = s_arg[i_arg].list[1];
        includeCorrWeights = 1;
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &corrWeightsNamesColumn, 1, 0,
                          "value", SDDS_STRING, &corrWeightsValuesColumn, 1, 0,
                          NULL))
          SDDS_Bomb("invalid -correctorWeights syntax");
        break;
      case CLO_VERBOSE:
        if (s_arg[i_arg].n_items == 1)
          verbose |= FL_VERBOSE;
        else if (s_arg[i_arg].n_items == 2 &&
                 strncmp(s_arg[i_arg].list[1], "very", strlen(s_arg[i_arg].list[1])) == 0)
          verbose |= FL_VERYVERBOSE;
        else
          SDDS_Bomb("invalid -verbose syntax");
        break;
      case CLO_REMOVEDCVECTORS:
        removeDCVectors = 1;
        break;
      case CLO_PRINTPACKAGE:
        printPackage = 1;
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          bomb("invalid -pipe syntax", NULL);
        break;
      case CLO_OLDCOLUMNNAMES:
        if (!(oldColumnNames = s_arg[i_arg].list[1]))
          bomb("No oldColumnNames string given", USAGE);
        break;
      case CLO_MULTIPLY_MATRIX:
#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
        if (!(multiplyFile = s_arg[i_arg].list[1]))
          bomb("No multiply matrix string given", USAGE);
        if (s_arg[i_arg].n_items > 2) {
          if (strncmp_case_insensitive(s_arg[i_arg].list[2], "invert", min(5, strlen(s_arg[i_arg].list[2]))) == 0)
            invertMultiply = 1;
          else
            SDDS_Bomb("Invalid -multiplyMatrix syntax provided.");
        }
#else
        if (verbose)
          fprintf(stdout, "Warning: the multiplyMatrix is not implemented for Numerical receipe or meschan or SunPerformance.\n");
#endif
        break;
      case CLO_NEWCOLUMNNAMES:
        if (s_arg[i_arg].n_items != 2 ||
            SDDS_StringIsBlank(newColumnNamesColumn = s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -newColumnNames syntax/value");
        break;
      case CLO_DELETEVECTORS:
        deleteVectors = s_arg[i_arg].n_items - 1;
        deleteVector = (long *)malloc(sizeof(*deleteVector) * deleteVectors);
        for (j = 0; j < deleteVectors; j++) {
          if (!(get_long(&deleteVector[j], s_arg[i_arg].list[j + 1])))
            bomb("non numeric value given in -deleteVectors option", USAGE);
        }
        break;
      case CLO_LAPACK_METHOD:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -lapackMethod syntax, either \"simple\" or \"divideAndConquer\" should be given.");
        if (strncmp_case_insensitive(s_arg[i_arg].list[1], "simple", MIN(strlen(s_arg[i_arg].list[1]), 6)) == 0) {
          lapackMethod = 0;
        } else if (strncmp_case_insensitive(s_arg[i_arg].list[1], "divideAndConqure", MIN(strlen(s_arg[i_arg].list[1]), 6)) == 0) {
          lapackMethod = 1;
        } else
          SDDS_Bomb("Invalid lapackMethod given, has to be \"simple\" or \"divideAndConquer\".");
        break;
      default:
        bomb("unrecognized option given", USAGE);
        break;
      }
    } else {
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        bomb("too many filenames given", USAGE);
    }
  }
#ifdef MKL
  omp_set_num_threads(threads);
#endif

  if (printPackage) {
#if defined(NUMERICAL_RECIPES)
    fprintf(stderr, "Compiled with package NUMERICAL_RECIPES\n");
#elif defined(SUNPERF)
    fprintf(stderr, "Compiled with package SUNPERF\n");
#elif defined(CLAPACK)
    fprintf(stderr, "Compiled with package CLAPACK\n");
#elif defined(LAPACK)
    fprintf(stderr, "Compiled with package LAPACK\n");
#elif defined(MKL)
    fprintf(stderr, "Compiled with package Intel MKL\n");
#else
    fprintf(stderr, "Compiled with package MESCHACH\n");
#endif
  }

  processFilenames("sddspseudoinverse", &inputfile, &outputfile, pipeFlags, noWarnings, &tmpfile_used);

  if ((nlargest && ratio) || (nlargest && nsmallest) || (nsmallest && ratio))
    SDDS_Bomb("Can only specify one of minimumSingularValueRatio, largestSingularValues and smallestSingularValues options.\n");
  if (newColumnNamesColumn && Root)
    SDDS_Bomb("-root and -newColumnNames are incompatible");
  if (verbose & FL_VERBOSE) {
    report_stats(stderr, "\nBefore initializing SDDS input.\n");
  }
  InitializeInputAndGetColumnNames(&inputPage, inputfile, &numericalColumnName, &numericalColumns,
                                   &stringColumnName, &inputDescription, &inputContents);
  if (verbose & FL_VERBOSE) {
    report_stats(stderr, "\nAfter initializing SDDS input.\n");
  }
  if (multiplyFile)
    InitializeInputAndGetColumnNames(&multiplyPage, multiplyFile, &multiplyColumnName, &multiplyColumns,
                                     &multiStringCol, NULL, NULL);

  while (0 < (ipage = SDDS_ReadTable(&inputPage))) {
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter reading page.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
    if (!SDDS_SetColumnFlags(&inputPage, 0))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    if (!SDDS_SetColumnsOfInterest(&inputPage, SDDS_NAME_ARRAY, numericalColumns, numericalColumnName))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (ipage == 1) {
      if (!(rows = SDDS_CountRowsOfInterest(&inputPage))) {
        SDDS_Bomb("No rows in dataset.");
      }
      if (verbose & FL_VERBOSE)
        fprintf(stderr, "Page %ld has %" PRId32 " rows.\n", ipage, rows);
      rowsFirstPage = rows;
      /* read string column or create names for output file columns*/
      if (!Root && stringColumnName) {
        /* use specified string column, or first string column encountered */
        if (!newColumnNamesColumn) {
          /* first string column encountered */
          outputColumnName = (char **)SDDS_GetColumn(&inputPage, stringColumnName);
          foundStringColumn = 1;
        } else {
          /* use specified string column */
          if (SDDS_CheckColumn(&inputPage, newColumnNamesColumn, NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY)
            SDDS_Bomb("column named with -newColumnNames does not exist in input");
          outputColumnName = (char **)SDDS_GetColumn(&inputPage, newColumnNamesColumn);
          foundStringColumn = 1;
        }
      } else {
        /* use command line options to produce column names in the output file */
        foundStringColumn = 0;
        outputColumnName = (char **)malloc(rows * sizeof(char *));
        digits = MAX(digits, log10(rows) + 1);
        if (!Root) {
          Root = (char *)malloc(sizeof(char) * (strlen("Column") + 1));
          strcpy(Root, "Column");
        }
        for (i = 0; i < rows; i++) {
          outputColumnName[i] = (char *)malloc(sizeof(char) * (strlen(Root) + digits + 1));
          sprintf(format, "%s%%0%ldld", Root, digits);
          sprintf(outputColumnName[i], format, i);
        }
      }
      outputColumns = rows;
    } else {
      if (!(rows = SDDS_CountRowsOfInterest(&inputPage))) {
        SDDS_Bomb("No rows in dataset.");
      }
      if (verbose & FL_VERBOSE)
        fprintf(stderr, "Page %ld has %" PRId32 " rows.\n", ipage, rows);
      if (rows != rowsFirstPage) {
        SDDS_Bomb("Datasets don't have the same number of rows.\nProcessing stopped before reaching the end of the input file.");
      }
    }
    /* ***** read multiply matrix file */
    if (multiplyFile) {
#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
      if ((mpage = SDDS_ReadPage(&multiplyPage)) > 0) {
        multiplyRows = SDDS_CountRowsOfInterest(&multiplyPage);
        if (!SDDS_SetColumnFlags(&multiplyPage, 0))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if (!SDDS_SetColumnsOfInterest(&multiplyPage, SDDS_NAME_ARRAY, multiplyColumns, multiplyColumnName))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if (mpage == 1) {
          if (!invertMultiply) {
            if (outputColumnName) {
              SDDS_FreeStringArray(outputColumnName, rows);
              free(outputColumnName);
            }
            actuatorName = numericalColumnName;
            outputColumns = multiplyColumns;
            if (!Root)
              outputColumnName = multiplyColumnName;
            else {
              freeMultiCol = 1;
              outputColumnName = (char **)malloc(outputColumns * sizeof(char *));
              for (i = 0; i < outputColumns; i++) {
                outputColumnName[i] = (char *)malloc(sizeof(char) * (strlen(Root) + digits + 1));
                sprintf(format, "%s%%0%ldld", Root, digits);
                sprintf(outputColumnName[i], format, i);
              }
            }
          } else {
            /*get the actutaor name from multiply matrix */
            if (multiStringCol) {
              actuatorName = (char **)SDDS_GetColumn(&multiplyPage, multiStringCol);
              actuators = multiplyRows;
            } else if (newColumnNamesColumn) {
              if (SDDS_CheckColumn(&multiplyPage, newColumnNamesColumn, NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
                actuatorName = (char **)SDDS_GetColumn(&multiplyPage, newColumnNamesColumn);
                actuators = multiplyRows;
              }
            }
            if (multiplyColumnName)
              freeMultiCol = 1;
            outputColumns = rows;
            if (Root) {
              SDDS_FreeStringArray(outputColumnName, outputColumns);
              free(outputColumnName);
              outputColumnName = (char **)malloc(outputColumns * sizeof(char *));
              for (i = 0; i < outputColumns; i++) {
                outputColumnName[i] = (char *)malloc(sizeof(char) * (strlen(Root) + digits + 1));
                sprintf(format, "%s%%0%ldld", Root, digits);
                sprintf(outputColumnName[i], format, i);
              }
            }
          }
        }
        if (!Multi) {
          Multi = NEW(MAT);
          if (mem_info_is_on()) {
            mem_bytes(TYPE_MAT, 0, sizeof(MAT));
            mem_numvar(TYPE_MAT, 1);
          }
        }
        Multi->m = Multi->max_m = multiplyRows;
        Multi->n = Multi->max_n = multiplyColumns;

        if (!(Multi->base = (Real *)SDDS_GetCastMatrixOfRows_SunPerf(&multiplyPage, &multiplyRows, SDDS_DOUBLE)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        Multi->me = (Real **)calloc(Multi->n, sizeof(Real *));
        if (mem_info_is_on()) {
          mem_bytes(TYPE_MAT, 0, Multi->n * sizeof(Real *));
        }
        /* Calculates the correct pointer address for the contiguous
           memory of the matrix. However the data is ordered wrong.
           So at this point if the function m_foutput is called, the
           matrix elements will be jumbled. */
        for (i = 0; i < Multi->n; i++)
          Multi->me[i] = &(Multi->base[i * Multi->m]);
        if (mem_info_is_on()) {
          /* because memory has been allocated outside of the meschach
             library we need to run these commands */
          mem_bytes(TYPE_MAT, 0, multiplyRows * multiplyColumns * sizeof(Real));
          mem_bytes(TYPE_MAT, 0, multiplyRows * sizeof(Real *));
        }
        /* free data page memory as soon as possible */
        SDDS_FreeDataPage(&multiplyPage);
      }
#endif
    }

    /* ***************
       read BPM  weights file
       *************** */
    if (includeWeights && ipage == 1) {
      if (verbose & FL_VERBOSE)
        fprintf(stderr, "Reading file %s...\n", weightsFile);
      if (!SDDS_InitializeInput(&weightsPage, weightsFile) ||
          !(weightsColumnName = (char **)SDDS_GetColumnNames(&weightsPage, &weightsColumns)))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (0 < SDDS_ReadTable(&weightsPage))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(weightsRows = SDDS_CountRowsOfInterest(&weightsPage)))
        SDDS_Bomb("No rows in weights dataset.");
      /* check for the presence of the two columns */
      if (!(weightsName = SDDS_GetColumn(&weightsPage, weightsNamesColumn)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(weights = SDDS_GetColumnInDoubles(&weightsPage, weightsValuesColumn)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_Terminate(&weightsPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      /* sort through the names. The rows of the weights file must be matched with
         the rows of the input matrix */

      w = v_get(rows);
      for (i = 0; i < rows; i++) {
        if (0 > (row_match = match_string(outputColumnName[i],
                                          weightsName,
                                          weightsRows, EXACT_MATCH))) {
          if (!noWarnings)
            fprintf(stderr, "Warning: Name %s doesn't exist in file %s.\n",
                    outputColumnName[i], weightsFile);
          w->ve[i] = 1.0;
        } else {
          w->ve[i] = weights[row_match];
        }
      }
      free(weights);
      weights = NULL;
    }
    /* ***************
       read corrector  weights file
       *************** */
    if (includeCorrWeights && ipage == 1) {
      if (verbose & FL_VERBOSE)
        fprintf(stderr, "Reading file %s...\n", corrWeightsFile);
      if (!SDDS_InitializeInput(&weightsPage, corrWeightsFile) ||
          !(corrWeightsColumnName = (char **)SDDS_GetColumnNames(&weightsPage, &corrWeightsColumns)))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (0 < SDDS_ReadTable(&weightsPage))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(corrWeightsRows = SDDS_CountRowsOfInterest(&weightsPage)))
        SDDS_Bomb("No rows in weights dataset.");
      /* check for the presence of the two columns */
      if (!(corrWeightsName = SDDS_GetColumn(&weightsPage, corrWeightsNamesColumn)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(weights = SDDS_GetColumnInDoubles(&weightsPage, corrWeightsValuesColumn)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_Terminate(&weightsPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      /* sort through the names. The rows of the weights file must be matched with
         the rows of the input matrix */

      corrW = v_get(numericalColumns);
      for (i = 0; i < numericalColumns; i++) {
        if (0 > (row_match = match_string(numericalColumnName[i],
                                          corrWeightsName,
                                          corrWeightsRows, EXACT_MATCH))) {
          if (!noWarnings)
            fprintf(stderr, "Warning: Name %s doesn't exist in file %s.\n",
                    numericalColumnName[i], weightsFile);
          corrW->ve[i] = 1.0;
        } else {
          corrW->ve[i] = weights[row_match];
        }
      }
      free(weights);
      weights = NULL;
    }
    /*Turning this off because it is crashes when the memory goes above INT_MAX bytes. Plus I don't trust it anyways becuase it does not match what the system is reporting.*/
    mem_info_on(0);
    /*    R = m_get( rows, numericalColumns); */
    /* allocate R but without the data memory */
    if (!R) {
      R = NEW(MAT);
      R->m = R->max_m = rows;
      R->n = R->max_n = numericalColumns;
      if (mem_info_is_on()) {
        mem_bytes(TYPE_MAT, 0, sizeof(MAT));
        mem_numvar(TYPE_MAT, 1);
      }
    }
    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter partial R allocation (if first loop).\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
    /* it is assumed that the non-numerical columns are filtered out */
    /* On subsequent loops, the memory pointed to by R->me is supposed to be freed */
#if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
    /* read in data as a matrix with contiguously allocated memory */
    /* The data will be ordered along columns (for fortran) and not along rows as in C.
       So the call to m_foutput will give the wrong layout */
    if (!(R->base = (Real *)SDDS_GetCastMatrixOfRows_SunPerf(&inputPage, &rows, SDDS_DOUBLE)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    R->me = (Real **)calloc(R->n, sizeof(Real *));
    if (mem_info_is_on()) {
      mem_bytes(TYPE_MAT, 0, R->m * sizeof(Real *));
    }
    /* Calculates the correct pointer address for the contiguous
       memory of the matrix. However the data is ordered wrong.
       So at this point if the function m_foutput is called, the
       matrix elements will be jumbled. */
    for (i = 0; i < R->n; i++)
      R->me[i] = &(R->base[i * R->m]);
#else
    /* for Meschach or numerical recipes in C */
    /* Obviously the data is ordered correctly for this case */
    if (!(R->me = (Real **)SDDS_GetCastMatrixOfRows(&inputPage, &rows, SDDS_DOUBLE)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#endif

    if (mem_info_is_on()) {
      /* because memory has been allocated outside of the meschach
         library we need to run these commands */
      mem_bytes(TYPE_MAT, 0, rows * numericalColumns * sizeof(Real));
      mem_bytes(TYPE_MAT, 0, rows * sizeof(Real *));
    }
    /* free data page memory as soon as possible */
    SDDS_FreeDataPage(&inputPage);

    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter filling R matrix with data.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }

    if (verbose & FL_VERYVERBOSE) {
      setformat("%9.6le ");
#if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
      fprintf(stderr, "Because a fortran routine is used (SunPerf, LAPACK or CLAPACK) the following Input matrix elements are jumbled but in the correct order for calling dgesvd \n");
#endif
      fprintf(stderr, "Input ");
      m_foutput(stderr, R);
    }
    /*here there was an important bug by in multiplying weight, R->m=rows (corresponse to bpms), weight numbers = rows (bpm weights); so that the correct
      way should be i<R->m (instead of i<R->n, which was before), and j<R->n (instead of R->m, which was before) */
    if (includeWeights) {
#if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
      /*for lapack, the R matrix is transposed (in column major order */
      for (j = 0; j < R->n; j++)
        for (i = 0; i < R->m; i++)
          R->me[j][i] *= w->ve[i];
#else
      for (i = 0; i < R->m; i++)
        for (j = 0; j < R->n; j++)
          R->me[i][j] *= w->ve[i];
#endif
    }
    if (includeCorrWeights) {
#if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
      /*for lapack, the R matrix is transposed (in column major order */
      for (j = 0; j < R->n; j++)
        for (i = 0; i < R->m; i++)
          R->me[j][i] *= corrW->ve[j];
#else
      for (i = 0; i < R->m; i++)
        for (j = 0; j < R->n; j++)
          R->me[i][j] *= corrW->ve[j];
#endif
    }
    /* On first passs, allocate the memory. On subsequent passes,
       the memory may have been free, and the pointer made NULL */
    if (!SValue)
      SValue = v_get(numericalColumns);
    if (!SValueUsed)
      SValueUsed = v_get(numericalColumns);
    if (!InvSValue)
      InvSValue = v_get(numericalColumns);

    /* Summary of which subroutine is called
       Method           Num. Rec.           SunPerf      LAPACK    CLAPACK    Meschach
       Flag             NUMERICAL_RECIPES   SUNPERF      LAPACK    CLAPACK    NONE
       -----------------------------------------------------------------------------
       Function call
       nr_svd           Yes                 No           No         No         No
       dgesvd           No                  Yes          No         No         No
       dgesvd_          No                  No           Yes        Yes        No
       svd              No                  No           No         No         Yes

       Allocated matrices for variables
       (note some memories are freed as soon as possible after svd calculation.)
       U or Ut          U uses R memory     U            Ut uses U  Ut uses U  Ut
       V or Vt          V (freed) and Vt    Vt           Vt         Vt         Vt
       Numerical recipes uses the same memory for R and U and saves memory.
       SunPerf is an alternative to Num. Rec. and Meschach on solaris.
       SunPerf library is an implementation of LAPACK. It has C-interface
       library calls. That's why we don't see the _ postfix on dgesvd below.
       CLAPACK is a C version of fortran LAPACK.
       CLAPACK may use ATLAS, an automatically tuned version of basic algebra functions.
       LAPACK is plain lapack now installed in linux systems.
    */

#ifdef NUMERICAL_RECIPES
    if (!V)
      V = m_get(numericalColumns, numericalColumns);
    /* R is replaced by values for U in situ */
    nr_svd(R, SValue, V);
    if (!Vt)
      Vt = m_get(numericalColumns, numericalColumns);
    m_transp(V, Vt);
    m_free(V);
    V = (MAT *)NULL;
    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter Vt allocation.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
    /* singular values have to be sorted now */
    fix_nr_svd(R, SValue, Vt);
    /* we can use pointer variable U because the memory in
       R is replaced with the left-hand eigenvectors. So U
       doesn't need allocation */
    U = R;
    if (verbose & FL_VERYVERBOSE) {
      fprintf(stderr, "U after svd call ");
      m_foutput(stderr, U);
    }

#else
#  if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
    /* These fortran calling routines have a option to
       run with standard svd (square U) and economy SVD (smaller U) */
    if (!Vt)
      Vt = m_get(R->n, R->n);
    if (!U) {
      if (!economy) {
        U = m_get(R->m, R->m);
      } else {
        economyRows = MIN(R->n, R->m);
        U = m_get(R->m, economyRows);
      }
    }
    if (!economy)
      calcMode = 'A';
    else
      calcMode = 'S';

#    if defined(SUNPERF)
    dgesvd(calcMode, calcMode, (int)R->m, (int)R->n,
           R->base, (int)(MAX(1, R->m)),
           SValue->ve,
           U->base, (int)R->m,
           Vt->base, (int)R->n,
           &info);

    dtrans('I', 1.0, U->base, U->m, U->n, NULL);
    dtrans('I', 1.0, Vt->base, Vt->m, Vt->n, NULL);
    if (verbose & FL_VERYVERBOSE) {
      fprintf(stderr, "U after svd calculation ");
      m_foutput(stderr, U);
    }
    if (verbose & FL_VERYVERBOSE) {
      fprintf(stderr, "Vt after svd calculation ");
      m_foutput(stderr, Vt);
    }
#    endif
#    if defined(CLAPACK)
#if defined(ACCELERATE_NEW_LAPACK)
    work = (double *)malloc(sizeof(double) * 1);
    lwork = -1;
    lda = MAX(1, R->m);
    if (lapackMethod == 1) {
      iwork = malloc(sizeof(*iwork) * 8 * MIN(R->m, R->n));
      dgesdd_((char *)&calcMode, (__LAPACK_int *)&R->m, (__LAPACK_int *)&R->n,
              (double *)R->base, (__LAPACK_int *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (__LAPACK_int *)&R->m,
              (double *)Vt->base, (__LAPACK_int *)&R->n,
              (double *)work, (__LAPACK_int *)&lwork, iwork,
              (__LAPACK_int *)&info);
    } else
      dgesvd_((char *)&calcMode, (char *)&calcMode, (__LAPACK_int *)&R->m, (__LAPACK_int *)&R->n,
              (double *)R->base, (__LAPACK_int *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (__LAPACK_int *)&R->m,
              (double *)Vt->base, (__LAPACK_int *)&R->n,
              (double *)work, (__LAPACK_int *)&lwork,
              (__LAPACK_int *)&info);

    lwork = work[0];
    if (verbose & FL_VERYVERBOSE)
      fprintf(stderr, "Work space size returned from dgesvd_ is %ld.\n", lwork);
    work = (double *)realloc(work, sizeof(double) * lwork);
    if (lapackMethod == 1) {
      dgesdd_((char *)&calcMode, (__LAPACK_int *)&R->m, (__LAPACK_int *)&R->n,
              (double *)R->base, (__LAPACK_int *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (__LAPACK_int *)&R->m,
              (double *)Vt->base, (__LAPACK_int *)&R->n,
              (double *)work, (__LAPACK_int *)&lwork, iwork,
              (__LAPACK_int *)&info);
      free(iwork);
    } else
      dgesvd_((char *)&calcMode, (char *)&calcMode, (__LAPACK_int *)&R->m, (__LAPACK_int *)&R->n,
              (double *)R->base, (__LAPACK_int *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (__LAPACK_int *)&R->m,
              (double *)Vt->base, (__LAPACK_int *)&R->n,
              (double *)work, (__LAPACK_int *)&lwork,
              (__LAPACK_int *)&info);

    free(work);
    /* do not need R now can free it*/
    t_free(R);
    R = (MAT *)NULL;
    /*removed the previous codes here; do not tranpose matrix, but take the advantage of
      the column order of U and Vt matrices */
#else
    work = (double *)malloc(sizeof(double) * 1);
    lwork = -1;
    lda = MAX(1, R->m);
    if (lapackMethod == 1) {
      iwork = malloc(sizeof(*iwork) * 8 * MIN(R->m, R->n));
      dgesdd_((char *)&calcMode, (long *)&R->m, (long *)&R->n,
              (double *)R->base, (long *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (long *)&R->m,
              (double *)Vt->base, (long *)&R->n,
              (double *)work, (long *)&lwork, iwork,
              (long *)&info);
    } else
      dgesvd_((char *)&calcMode, (char *)&calcMode, (long *)&R->m, (long *)&R->n,
              (double *)R->base, (long *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (long *)&R->m,
              (double *)Vt->base, (long *)&R->n,
              (double *)work, (long *)&lwork,
              (long *)&info);

    lwork = work[0];
    if (verbose & FL_VERYVERBOSE)
      fprintf(stderr, "Work space size returned from dgesvd_ is %ld.\n", lwork);
    work = (double *)realloc(work, sizeof(double) * lwork);
    if (lapackMethod == 1) {
      dgesdd_((char *)&calcMode, (long *)&R->m, (long *)&R->n,
              (double *)R->base, (long *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (long *)&R->m,
              (double *)Vt->base, (long *)&R->n,
              (double *)work, (long *)&lwork, iwork,
              (long *)&info);
      free(iwork);
    } else
      dgesvd_((char *)&calcMode, (char *)&calcMode, (long *)&R->m, (long *)&R->n,
              (double *)R->base, (long *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (long *)&R->m,
              (double *)Vt->base, (long *)&R->n,
              (double *)work, (long *)&lwork,
              (long *)&info);

    free(work);
    /* do not need R now can free it*/
    t_free(R);
    R = (MAT *)NULL;
    /*removed the previous codes here; do not tranpose matrix, but take the advantage of
      the column order of U and Vt matrices */
#endif
#    endif
#    if defined(MKL)
    work = (double *)malloc(sizeof(double) * 1);
    lwork = -1;
    lda = MAX(1, R->m);
    if (lapackMethod == 1) {
      iwork = malloc(sizeof(*iwork) * 8 * MIN(R->m, R->n));
      dgesdd_((char *)&calcMode, (MKL_INT *)&R->m, (MKL_INT *)&R->n,
              (double *)R->base, (MKL_INT *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (MKL_INT *)&R->m,
              (double *)Vt->base, (MKL_INT *)&R->n,
              (double *)work, (MKL_INT *)&lwork, iwork,
              (MKL_INT *)&info);
    } else
      dgesvd_((char *)&calcMode, (char *)&calcMode, (MKL_INT *)&R->m, (MKL_INT *)&R->n,
              (double *)R->base, (MKL_INT *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (MKL_INT *)&R->m,
              (double *)Vt->base, (MKL_INT *)&R->n,
              (double *)work, (MKL_INT *)&lwork,
              (MKL_INT *)&info);

    lwork = work[0];
    if (verbose & FL_VERYVERBOSE)
      fprintf(stderr, "Work space size returned from dgesvd_ is %ld.\n", lwork);
    work = (double *)realloc(work, sizeof(double) * lwork);
    if (lapackMethod == 1) {
      dgesdd_((char *)&calcMode, (MKL_INT *)&R->m, (MKL_INT *)&R->n,
              (double *)R->base, (MKL_INT *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (MKL_INT *)&R->m,
              (double *)Vt->base, (MKL_INT *)&R->n,
              (double *)work, (MKL_INT *)&lwork, iwork,
              (MKL_INT *)&info);
      free(iwork);
    } else
      dgesvd_((char *)&calcMode, (char *)&calcMode, (MKL_INT *)&R->m, (MKL_INT *)&R->n,
              (double *)R->base, (MKL_INT *)&lda,
              (double *)SValue->ve,
              (double *)U->base, (MKL_INT *)&R->m,
              (double *)Vt->base, (MKL_INT *)&R->n,
              (double *)work, (MKL_INT *)&lwork,
              (MKL_INT *)&info);

    free(work);
    /* do not need R now can free it*/
    t_free(R);
    R = (MAT *)NULL;
    /*removed the previous codes here; do not tranpose matrix, but take the advantage of
      the column order of U and Vt matrices */
#    endif
#    if defined(LAPACK)
    work = (doublereal *)malloc(sizeof(doublereal) * 1);
    lwork = -1;
    lda = MAX(1, R->m);
    if (lapackMethod == 1) {
      iwork = malloc(sizeof(*iwork) * 8 * MIN(R->m, R->n));
      LAPACK_dgesdd((char *)&calcMode, (lapack_int *)&R->m, (lapack_int *)&R->n,
                    (double *)R->base, (lapack_int *)&lda,
                    (double *)SValue->ve,
                    (double *)U->base, (lapack_int *)&R->m,
                    (double *)Vt->base, (lapack_int *)&R->n,
                    (double *)work, (lapack_int *)&lwork, iwork,
                    (lapack_int *)&info);
    } else
      LAPACK_dgesvd((char *)&calcMode, (char *)&calcMode, (lapack_int *)&R->m, (lapack_int *)&R->n,
                    (doublereal *)R->base, (lapack_int *)&lda,
                    (doublereal *)SValue->ve,
                    (doublereal *)U->base, (lapack_int *)&R->m,
                    (doublereal *)Vt->base, (lapack_int *)&R->n,
                    (doublereal *)work, (lapack_int *)&lwork,
                    (lapack_int *)&info);

    lwork = work[0];
    if (verbose & FL_VERYVERBOSE)
      fprintf(stderr, "Work space size returned from dgesvd_ is %d.\n", lwork);
    work = (doublereal *)realloc(work, sizeof(doublereal) * lwork);
    if (lapackMethod == 1) {
      LAPACK_dgesdd((char *)&calcMode, (lapack_int *)&R->m, (lapack_int *)&R->n,
                    (double *)R->base, (lapack_int *)&lda,
                    (double *)SValue->ve,
                    (double *)U->base, (lapack_int *)&R->m,
                    (double *)Vt->base, (lapack_int *)&R->n,
                    (double *)work, (lapack_int *)&lwork, iwork,
                    (lapack_int *)&info);
      free(iwork);
    } else
      LAPACK_dgesvd((char *)&calcMode, (char *)&calcMode, (lapack_int *)&R->m, (lapack_int *)&R->n,
                    (doublereal *)R->base, (lapack_int *)&lda,
                    (doublereal *)SValue->ve,
                    (doublereal *)U->base, (lapack_int *)&R->m,
                    (doublereal *)Vt->base, (lapack_int *)&R->n,
                    (doublereal *)work, (lapack_int *)&lwork,
                    (lapack_int *)&info);
    free(work);
    /* do not need R now can free it*/

    t_free(R);
    R = (MAT *)NULL;
    /*removed the previous codes here; do not tranpose matrix, but take the advantage of
      the column order of U and Vt matrices */
#    endif
#  else
    /* Meschach method. */
    /* This can be a large matrix!
       The memory might have been freed on a previous loop if
       -umatrix option is used. */
    /* Economy option is not available for Meschach. So
       the problem can get very large if the number of rows is large */
    if (!Ut)
      Ut = m_get(rows, rows);

    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter Ut allocation.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
    /* Note that meschach's svd decomposes A = (arg1)^T w (arg2) which
       is a different convention as in num. recipes in C: A = (arg1) w
       (arg2)^T, which we use in this program. That's why the
       arguments are Ut, Vt instead of U, V as shown in the function
       code.*/
    if (!Vt)
      Vt = m_get(numericalColumns, numericalColumns);
    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter Vt allocation.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
    svd(R, Ut, Vt, SValue);
    m_freePointers(R);
    R = (MAT *)NULL;
    if (verbose & FL_VERYVERBOSE) {
      fprintf(stderr, "Ut after svd calculation ");
      m_foutput(stderr, Ut);
    }
    if (verbose & FL_VERYVERBOSE) {
      fprintf(stderr, "Vt after svd calculation ");
      m_foutput(stderr, Vt);
    }
#  endif
#endif

    /* This part is common to all svd methods */
    /* remove DC vectors in V matrix */
    if (removeDCVectors) {
      double sum;
      for (i = 0; i < numericalColumns; i++) {
        sum = 0.0;
        for (j = 0; j < numericalColumns; j++) {
          /* note here for CLAPACK, Vt is transposed*/
#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
          sum += Vt->me[j][i];
#else
          sum += Vt->me[i][j];
#endif
        }
        if (fabs(sum) > 0.1 * sqrt(numericalColumns)) {
          SValue->ve[i] = 0.0;
        }
      }
    }
    max = 0;
    min = HUGE;
    /*find the largest S value, and use it as reference for ratio instead of the first Svalue because the first Svalue could be removed
      (set to zero) with -removeDCVectors option*/
    largestS = 0;
    for (i = 0; i < numericalColumns; i++) {
      if (SValue->ve[i]) {
        largestS = SValue->ve[i];
        break;
      }
    }
    if (!largestS)
      SDDS_Bomb("Error: no non-zero singular values found, unable to find the inverse response matrix.");
    /*
      1) first remove SVs that are exactly zero
      2) remove SV according to ratio option
      3) remove SV according to largests option
      4) remove SV of user-selected vectors
    */
    NSVUsed = 0;
    if (tikhonov && t_svn && t_svn <= numericalColumns)
      t_alpha = SValue->ve[t_svn - 1];
    if (tikhonov && t_beta) {
      find_min_max(&min, &max, SValue->ve, numericalColumns);
      t_alpha = max * t_beta;
    }
    /*    fprintf(stderr, "alpha=%f\n", t_alpha); */
    for (i = 0; i < numericalColumns; i++) {
      if (!SValue->ve[i]) {
        InvSValue->ve[i] = 0;
      } else if ((SValue->ve[i] / largestS) < ratio) {
        InvSValue->ve[i] = 0;
        SValueUsed->ve[i] = 0;
      } else if (nlargest && i >= nlargest) {
        InvSValue->ve[i] = 0;
        SValueUsed->ve[i] = 0;
      } else if (nsmallest && i >= (numericalColumns - nsmallest)) {
        InvSValue->ve[i] = 0;
        SValueUsed->ve[i] = 0;
      } else {
        if (tikhonov)
          InvSValue->ve[i] = SValue->ve[i] / (SValue->ve[i] * SValue->ve[i] + t_alpha * t_alpha);
        else
          InvSValue->ve[i] = 1 / SValue->ve[i];
        SValueUsed->ve[i] = SValue->ve[i];
        max = MAX(SValueUsed->ve[i], max);
        min = MIN(SValueUsed->ve[i], min);
        NSVUsed++;
      }
    }
    /*4) remove SV of user-selected vectors -
      delete vector given in the -deleteVectors option
      by setting the inverse singular values to 0*/
    for (i = 0; i < deleteVectors; i++) {
      if (0 <= deleteVector[i] && deleteVector[i] < numericalColumns) {
        if (firstdelete)
          sprintf(deletedVector, "%ld", deleteVector[i]);
        else
          sprintf(deletedVector + strlen(deletedVector), " %ld", deleteVector[i]);
        firstdelete = 0;
        InvSValue->ve[deleteVector[i]] = 0;
        SValueUsed->ve[deleteVector[i]] = 0;
        if (nlargest && deleteVector[i] >= nlargest)
          break;
        NSVUsed--;
      }
    }
    conditionNumber = max / min;
    if (verbose & FL_VERYVERBOSE) {
      setformat("%9.6le ");
      fprintf(stderr, "Inverse singular value ");
      v_foutput(stderr, InvSValue);
    }

    /* R = U S Vt and Rinv = V SInv Ut */
    if (!RInvt) {
#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
      if (!multiplyFile)
        RInvt = m_get(numericalColumns, rows);
      else {
        /* the inverse matrix should be numericalColumns X rows, RInvt is the transpose of inverse, therefore
           it is rows X numericalColumns */
        RInvt = NEW(MAT);
        RInvt->m = RInvt->max_m = rows;
        RInvt->n = RInvt->max_n = numericalColumns;
        RInvt->base = (Real *)calloc(rows * numericalColumns, sizeof(Real));
        RInvt->me = (Real **)calloc(RInvt->n, sizeof(Real *));
        for (i = 0; i < RInvt->n; i++)
          RInvt->me[i] = &(RInvt->base[i * RInvt->m]);
        /* the transpose step after dgemm is not needed */
      }
#else
      RInvt = m_get(rows, numericalColumns);
#endif
    }

    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter RInvt allocation.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }

#if defined(NUMERICAL_RECIPES) || defined(SUNPERF)
    /* Only U (with original R memory) and Vt are available.  Make
       multiplication as sum of singular triplets RInv_ij = (V_ik InvS_k
       Ut_kj) becomes (Vt_ki InvS_k U_jk) */
    for (i = 0; i < Vt->n; i++)
      for (j = 0; j < U->m; j++) {
        x = 0.0;
        for (k = 0; k < numericalColumns; k++)
          x += Vt->me[k][i] * InvSValue->ve[k] * U->me[j][k];
        RInvt->me[j][i] = x;
      }
#else
#  if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
    if (!V)
      V = m_get(Vt->m, Vt->n);
    /* U ant Vt matrix (in column order) are available */
    for (i = 0; i < Vt->n; i++) {
      for (kk = 0; kk < numericalColumns; kk++)
        V->base[i * V->m + kk] = Vt->base[i * V->m + kk] * InvSValue->ve[kk];
    }
    /* Rinvt = U Sinv Vt = U V  this is only applicable for economy option,
       otherwise, dgemm does not work since U has R->m columns, while V (Vt) has R->n rows */
    /* note that, the arguments of dgemm should be
       U->m -- should be the row number of product matrix, which is U x V
       V->n -- the column number of the product matrix
       kk   -- should be the column number of U and row number of V, whichever is smaller (otherwise, memory access error)
       lda  -- should be the row number of U matrix
       ldb  -- should be the row number of V matrix */
    kk = MIN(U->n, V->m);
    lda = MAX(1, U->m);
    ldb = MAX(1, V->m);
#    if defined(SOLARIS)
    f2c_dgemm("N", "N",
              &U->m, &V->n, &kk, &alpha, U->base,
              &lda, V->base, &ldb, &beta, RInvt->base, &U->m);
#    else
#      if defined(CLAPACK)
    /* Could use cblas_dgemm here in the future. See cblas.h
       void cblas_dgemm(const enum CBLAS_ORDER Order, const enum CBLAS_TRANSPOSE TransA,
       const enum CBLAS_TRANSPOSE TransB, const int M, const int N,
       const int K, const double alpha, const double *A,
       const int lda, const double *B, const int ldb,
       const double beta, double *C, const int ldc);
    */
    /* Note that dgemm_ is not prototyped in any header file (I looked),
       so you'll get a warning message during compilation */
#if defined(ACCELERATE_NEW_LAPACK)
    dgemm_("N", "N",
           (__LAPACK_int *)&U->m, (__LAPACK_int *)&V->n, &kk, &alpha, U->base,
           (__LAPACK_int *)&lda, V->base, &ldb, &beta, RInvt->base, (__LAPACK_int *)&U->m);
#else
    dgemm_("N", "N",
           &U->m, &V->n, &kk, &alpha, U->base,
           &lda, V->base, &ldb, &beta, RInvt->base, &U->m);
#endif
#      endif
#      if defined(MKL)
    dgemm_("N", "N",
           (MKL_INT *)&U->m, (MKL_INT *)&V->n, (MKL_INT *)&kk, &alpha, U->base,
           (MKL_INT *)&lda, V->base, (MKL_INT *)&ldb, &beta, RInvt->base, (MKL_INT *)&U->m);
#      endif
#      if defined(LAPACK)
    dgemm_("N", "N",
           (lapack_int *)&U->m, (lapack_int *)&V->n, &kk, &alpha, U->base,
           &lda, V->base, &ldb, &beta, RInvt->base, (lapack_int *)&U->m);
#      endif
#    endif
    m_free(V);
    V = (MAT *)NULL;
    /*move the adding weights from inside else statement (was a bug) to outside,*/
    /*added "&& multiplyFiles" in following statement on 12/7/2022, otherwise, for multiplyFiles=0, the weight will be applied 3 times */
    /*here RInvt->m is the number of correctors, and RInvt->n is number of bpms */
    if (includeWeights && multiplyFile)
      for (i = 0; i < RInvt->n; i++)
        for (j = 0; j < RInvt->m; j++)
          RInvt->base[i * RInvt->m + j] *= w->ve[i];
    if (includeCorrWeights && multiplyFile)
      for (i = 0; i < RInvt->n; i++)
        for (j = 0; j < RInvt->m; j++)
          RInvt->base[i * RInvt->m + j] *= corrW->ve[j];
    if (!multiplyFile) {
      /* here RInvt->base is in column order, however, RInvt->me take it
         as row order, need transpose RInvt->me
         to output column by RInvt->me[i] */
      tmpR = m_get(rows, numericalColumns);
      for (i = 0; i < rows; i++) {
        for (j = 0; j < numericalColumns; j++)
          tmpR->me[i][j] = RInvt->me[j][i];
      }
      m_free(RInvt);
      RInvt = tmpR;
    } else {
      /* do not transpose the RInvt, since the RInvt->base is in column order already */

      if (!Product) {
        Product = NEW(MAT);
        if (!invertMultiply) {
          if (RInvt->m != Multi->m)
            SDDS_Bomb("Unable to multiply inverse by multiply matrix, their column and row number do not match.");
          ldb = MAX(1, Multi->m);
          Product->m = Product->max_m = RInvt->n;
          Product->n = Product->max_n = Multi->n;
          kk = MAX(1, Multi->m);
          ldb = MAX(1, kk);
        } else {
          if (Multi->n != RInvt->n) {
            SDDS_Bomb("Unable to multiply \"multiply matrix\" by inverse, their column and row number do not match.");
          }

          Product->m = Product->max_m = Multi->m;
          Product->n = Product->max_n = RInvt->m;
          kk = MAX(1, Multi->n);
          lda = MAX(1, Multi->m);
          ldb = MAX(1, Product->n);
        }
        Product->base = (Real *)calloc(Product->m * Product->n, sizeof(Real));
        if (mem_info_is_on())
          mem_bytes(TYPE_MAT, 0, Product->m * Product->n * sizeof(Real));
        Product->me = (Real **)calloc(Product->n, sizeof(Real *));
        if (mem_info_is_on())
          mem_bytes(TYPE_MAT, 0, Product->m * sizeof(Real *));
        for (i = 0; i < Product->n; i++)
          Product->me[i] = &(Product->base[i * Product->m]);
      }
#    if defined(SOLARIS)
      if (!invertMultiply)
        f2c_dgemm("T", "N",
                  &Product->m, &Product->n, &kk, &alpha, RInvt->base, &lda,
                  Multi->base, &ldb, &beta, Product->base, &Product->m);
      else
        f2c_dgemm("N", "T",
                  &Product->m, &Product->n, &kk, &alpha, Multi->base, &lda,
                  RInvt->base, &ldb, &beta, Product->base, &Product->m);
#    else
#      if defined(MKL)
      if (!invertMultiply)
        dgemm_("T", "N",
               (MKL_INT *)&Product->m, (MKL_INT *)&Product->n, (MKL_INT *)&kk, &alpha, RInvt->base, (MKL_INT *)&lda,
               Multi->base, (MKL_INT *)&ldb, &beta, Product->base, (MKL_INT *)&Product->m);
      else
        dgemm_("N", "T",
               (MKL_INT *)&Product->m, (MKL_INT *)&Product->n, (MKL_INT *)&kk, &alpha, Multi->base, (MKL_INT *)&lda,
               RInvt->base, (MKL_INT *)&ldb, &beta, Product->base, (MKL_INT *)&Product->m);
#      else
#if defined(ACCELERATE_NEW_LAPACK)
      if (!invertMultiply)
        dgemm_("T", "N",
               (__LAPACK_int *)&Product->m, (__LAPACK_int *)&Product->n, &kk, &alpha, RInvt->base, (__LAPACK_int *)&lda,
               Multi->base, &ldb, &beta, Product->base, (__LAPACK_int *)&Product->m);
      else
        dgemm_("N", "T",
               (__LAPACK_int *)&Product->m, (__LAPACK_int *)&Product->n, &kk, &alpha, Multi->base, (__LAPACK_int *)&lda,
               RInvt->base, &ldb, &beta, Product->base, (__LAPACK_int *)&Product->m);
#else
      if (!invertMultiply)
        dgemm_("T", "N",
          (lapack_int *)&Product->m, (lapack_int *)&Product->n, &kk, &alpha, RInvt->base, (lapack_int *)&lda,
               Multi->base, (lapack_int *)&ldb, &beta, Product->base, (lapack_int *)&Product->m);
      else
        dgemm_("N", "T",
               (lapack_int *)&Product->m, (lapack_int *)&Product->n, &kk, &alpha, Multi->base, (lapack_int *)&lda,
               RInvt->base, (lapack_int *)&ldb, &beta, Product->base, (lapack_int *)&Product->m);
#endif
#      endif
#    endif
      t_free(Multi);
      Multi = (MAT *)NULL;
      m_free(RInvt);
      RInvt = (MAT *)NULL;
    }
#  else
    /* Only Ut and Vt are available for Meschach.
       Make multiplication as sum of singular triplets RInv_ij = (V_ik
       InvS_k Ut_kj) becomes (Vt_ki InvS_k Ut_kj) */
    if (verbose & FL_VERYVERBOSE) {
      fprintf(stderr, "Ut for Rinvt calculation ");
      m_foutput(stderr, Ut);
    }
    if (verbose & FL_VERYVERBOSE) {
      fprintf(stderr, "Vt for Rinvt calculation ");
      m_foutput(stderr, Vt);
    }
    for (i = 0; i < Vt->n; i++)
      for (j = 0; j < Ut->n; j++) {
        RInvt->me[j][i] = 0.0;
        for (k = 0; k < MIN(rows, numericalColumns); k++) {
          RInvt->me[j][i] += Vt->me[k][i] * InvSValue->ve[k] * Ut->me[k][j];
        }
      }
#  endif
#endif

    /*here RInvt->n is the number of correctors, and RInvt->m is number of bpms because RInvt was transposed now. */
    if (includeWeights && !multiplyFile)
      for (i = 0; i < RInvt->m; i++)
        for (j = 0; j < RInvt->n; j++)
          /* Normally weights multiply columns of RInv */
          /* since we print RInvt, weights multiply rows. */
          RInvt->me[i][j] *= w->ve[i];
    if (includeCorrWeights && !multiplyFile)
      /*correct weights is the oposite of bpm weights in above, multiply by columns */
      for (i = 0; i < RInvt->m; i++)
        for (j = 0; j < RInvt->n; j++)
          RInvt->me[i][j] *= corrW->ve[j];
    if (verbose & FL_VERYVERBOSE) {
      setformat("%9.6le ");
      fprintf(stderr, "Pseudo-inverse (transpose) ");
      m_foutput(stderr, RInvt);
    }

    /*********************\
     * define output page *
     \*********************/
    if (ipage == 1) {
      if (inputDescription) {
        outputDescription = (char *)malloc(sizeof(char) * (strlen("Pseudo-inverse of ") + strlen(inputDescription) + 1));
        strcat(strcpy(outputDescription, "Pseudo-inverse of "), inputDescription);
        if (!SDDS_InitializeOutput(&outputPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, outputDescription, inputContents, outputfile))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (!SDDS_InitializeOutput(&outputPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, "Pseudoinverse", "Pseudoinverse", outputfile))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (columnMajorOrder != -1)
        outputPage.layout.data_mode.column_major = columnMajorOrder;
      else
        outputPage.layout.data_mode.column_major = inputPage.layout.data_mode.column_major;
      if (verbose & FL_VERBOSE) {
        report_stats(stderr, "\nAfter SDDS_InitializeOutput.\n");
        if (mem_info_is_on())
          mem_info_file(stderr, 0);
      }
      if (sFile) {
        if (!SDDS_InitializeOutput(&sPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, "Singular values", NULL, sFile))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (columnMajorOrder != -1)
          sPage.layout.data_mode.column_major = columnMajorOrder;
        else
          sPage.layout.data_mode.column_major = inputPage.layout.data_mode.column_major;
      }
      if (uFile) {
        if (!SDDS_InitializeOutput(&uPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, "U column-orthogonal matrix", "Orthogonal Matrix", uFile))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (columnMajorOrder != -1)
          uPage.layout.data_mode.column_major = columnMajorOrder;
        else
          uPage.layout.data_mode.column_major = inputPage.layout.data_mode.column_major;
      }
      if (vFile) {
        if (!SDDS_InitializeOutput(&vPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, "V column-orthogonal matrix", "Orthogonal Matrix", vFile))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (columnMajorOrder != -1)
          vPage.layout.data_mode.column_major = columnMajorOrder;
        else
          vPage.layout.data_mode.column_major = inputPage.layout.data_mode.column_major;
      }
      /* define singular value arrays */
      if (0 > SDDS_DefineArray(&outputPage, "SingularValues", "SingularValues", NULL, "Singular Values",
                               NULL, SDDS_DOUBLE, 0, 1, NULL) ||
          0 > SDDS_DefineArray(&outputPage, "SingularValuesUsed", "SingularValuesUsed", NULL,
                               "Singular Values Used",
                               NULL, SDDS_DOUBLE, 0, 1, NULL) ||
          0 > SDDS_DefineArray(&outputPage, "InverseSingularValues", "InverseSingularValues", NULL,
                               "Inserse Singular Values",
                               NULL, SDDS_DOUBLE, 0, 1, NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      /***********************************\
       * define names for double columns *
       \***********************************/
      if (!multiplyFile) {
        if (0 > SDDS_DefineColumn(&outputPage, oldColumnNames ? oldColumnNames : "OldColumnNames",
                                  NULL, NULL, NULL, NULL, SDDS_STRING, 0))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (Symbol) {
          for (i = 0; i < rows; i++)
            if (0 > SDDS_DefineColumn(&outputPage, outputColumnName[i], Symbol, NULL, NULL,
                                      NULL, SDDS_DOUBLE, 0))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        } else {
          if (0 > SDDS_DefineSimpleColumns(&outputPage, rows, outputColumnName, NULL, SDDS_DOUBLE))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      } else {
        if (!invertMultiply) {
          if (!SDDS_DefineSimpleColumn(&outputPage, "OldColumnNames", NULL, SDDS_STRING))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          strColIndex = 0;
        }
        if (invertMultiply && (newColumnNamesColumn || multiStringCol)) {
          if (!SDDS_DefineSimpleColumn(&outputPage, newColumnNamesColumn ? newColumnNamesColumn : multiStringCol, NULL, SDDS_STRING))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          strColIndex = 0;
        }
        if (0 > SDDS_DefineSimpleColumns(&outputPage, Product->n, outputColumnName, NULL, SDDS_DOUBLE))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (verbose & FL_VERBOSE) {
        report_stats(stderr, "\nAfter defining columns.\n");
        if (mem_info_is_on())
          mem_info_file(stderr, 0);
      }
      if (uFile || vFile || (sFile && sFileAsMatrix)) {
        orthoColumnName = (char **)malloc(MAX(rows, numericalColumns) * sizeof(char *));
        for (i = 0; i < MAX(rows, numericalColumns); i++) {
          orthoColumnName[i] = (char *)malloc(sizeof(char) * (strlen("SV") + digits + 1));
          sprintf(format, "SV%%0%ldld", digits);
          sprintf(orthoColumnName[i], format, i);
        }
      }

      if (uFile) {
        if (0 > SDDS_DefineColumn(&uPage,
                                  newColumnNamesColumn ? newColumnNamesColumn : "OriginalRows",
                                  NULL, NULL, NULL, NULL, SDDS_STRING, 0))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

        /* the number of rows of U depends on the method used and the
           economy mode flag.  The clearest way for determining the number of
           columns for the page is to use the matrix data itself. For
           NUMERICAL_RECIPES and SUNPERF only U is available, for other methods
           Ut is available.*/
#if defined(NUMERICAL_RECIPES) || defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
        if (0 > SDDS_DefineSimpleColumns(&uPage,
                                         U->n,
                                         orthoColumnName, NULL, SDDS_DOUBLE))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#else
        if (0 > SDDS_DefineSimpleColumns(&uPage,
                                         Ut->m,
                                         orthoColumnName, NULL, SDDS_DOUBLE))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#endif
        if (!SDDS_WriteLayout(&uPage))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      if (vFile)
        if (0 > SDDS_DefineSimpleColumn(&vPage,
                                        oldColumnNames ? oldColumnNames : "OldColumnNames",
                                        NULL, SDDS_STRING) ||
            0 > SDDS_DefineSimpleColumns(&vPage, numericalColumns,
                                         orthoColumnName, NULL, SDDS_DOUBLE) ||
            !SDDS_WriteLayout(&vPage))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      if (sFile) {
        if (!sFileAsMatrix) {
          if (0 > SDDS_DefineSimpleColumn(&sPage, "Index",
                                          NULL, SDDS_LONG) ||
              0 > SDDS_DefineSimpleColumn(&sPage, "SingularValues",
                                          NULL, SDDS_DOUBLE) ||
              !SDDS_WriteLayout(&sPage))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        } else {
          if (0 > SDDS_DefineSimpleColumns(&sPage,
                                           numericalColumns,
                                           orthoColumnName, NULL, SDDS_DOUBLE) ||
              !SDDS_WriteLayout(&sPage))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }

      if (0 > SDDS_DefineParameter(&outputPage, "MinimumSingularValueRatio", "MinimumSingularValueRatio", NULL,
                                   "Minimum singular value ratio", NULL, SDDS_DOUBLE, NULL) ||
          0 > SDDS_DefineParameter(&outputPage, "NumberOfSingularValuesUsed", "NumberOfSingularValuesUsed", NULL,
                                   "largest singular value used", NULL, SDDS_LONG, NULL) ||
          0 > SDDS_DefineParameter(&outputPage, "DeletedVectors", "DeletedVectors", NULL,
                                   "list of vectors that were deleted", NULL, SDDS_STRING, NULL) ||
          0 > SDDS_DefineParameter(&outputPage, "InputFile", "InputFile", NULL,
                                   "InputFile", NULL, SDDS_STRING, 0) ||
          0 > SDDS_DefineParameter(&outputPage, "ConditionNumber", "ConditionNumber", NULL,
                                   "Condition Number", NULL, SDDS_DOUBLE, 0) ||
          0 > SDDS_DefineParameter(&outputPage, "TikhonovFilterUsed", "TikhonovFilterUsed", NULL,
                                   "TikhonovFilterUsed", NULL, SDDS_LONG, 0) ||
          0 > SDDS_DefineParameter(&outputPage, "TikhonovSVNNumber", "TikhonovSVNNumber", NULL,
                                   "TikhonovSVNNumber", NULL, SDDS_LONG, 0) ||
          0 > SDDS_DefineParameter(&outputPage, "TikhonovAlpha", "TikhonovAlpha", NULL,
                                   "TikhonovAlpha", NULL, SDDS_DOUBLE, 0) ||
          0 > SDDS_DefineParameter(&outputPage, "TikhonovBeta", "TikhonovBeta", NULL,
                                   "TikhonovBeta", NULL, SDDS_DOUBLE, 0) ||
          !SDDS_WriteLayout(&outputPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      if (reconstructFile) {
        if (!SDDS_InitializeCopy(&reconstructPage, &inputPage, reconstructFile, "w"))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (newColumnNamesColumn && !SDDS_DefineSimpleColumn(&reconstructPage, newColumnNamesColumn, NULL, SDDS_STRING))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!SDDS_FindParameter(&inputPage, FIND_NUMERIC_TYPE, "NumberOfSingularValuesUsed", NULL))
          if (0 > SDDS_DefineParameter(&reconstructPage, "NumberOfSingularValuesUsed",
                                       "NumberOfSingularValuesUsed", NULL,
                                       "largest singular value used", NULL, SDDS_LONG, NULL))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!SDDS_FindParameter(&inputPage, FIND_ANY_TYPE, "DeletedVectors", NULL))
          if (0 > SDDS_DefineParameter(&reconstructPage, "DeletedVectors", "DeletedVectors", NULL,
                                       "list of vectors that were deleted", NULL, SDDS_STRING, NULL))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!SDDS_WriteLayout(&reconstructPage))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    /* Write to each table in turn, and deallocate page data as soon as possible
       to make memory usage emore efficient. */

    if (!multiplyFile)
      outpageRows = numericalColumns;
    else
      outpageRows = Product->m;
    if (!SDDS_StartTable(&outputPage, outpageRows) ||
        !SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "MinimumSingularValueRatio", ratio,
                            "NumberOfSingularValuesUsed", NSVUsed,
                            "DeletedVectors", deletedVector,
                            "ConditionNumber", conditionNumber,
                            "TikhonovFilterUsed", tikhonov,
                            "TikhonovSVNNumber", t_svn,
                            "TikhonovAlpha", t_alpha,
                            "TikhonovBeta", t_beta,
                            "InputFile", inputfile ? inputfile : "pipe", NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter output SDDS_StartTable.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }

    dim_ptr[0] = numericalColumns;
    if (!SDDS_SetArray(&outputPage, "SingularValues", SDDS_POINTER_ARRAY, SValue->ve, dim_ptr))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    dim_ptr[0] = NSVUsed;
    if (!SDDS_SetArray(&outputPage, "SingularValuesUsed", SDDS_POINTER_ARRAY, SValueUsed->ve, dim_ptr))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_SetArray(&outputPage, "InverseSingularValues", SDDS_POINTER_ARRAY, InvSValue->ve, dim_ptr))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!multiplyFile) {
      if (!SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                          numericalColumnName, numericalColumns, oldColumnNames ? oldColumnNames : "OldColumnNames"))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i = 0; i < rows; i++)
        if (!SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                            RInvt->me[i], numericalColumns, outputColumnName[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      m_free(RInvt);
      RInvt = (MAT *)NULL;
    } else {
      if (strColIndex >= 0)
        if (SDDS_SetColumn(&outputPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE,
                           actuatorName, Product->m, strColIndex))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i = 0; i < Product->n; i++)
        if (!SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                            Product->me[i], Product->m, outputColumnName[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      t_free(Product);
      Product = (MAT *)NULL;
    }
    if (!SDDS_WriteTable(&outputPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    SDDS_FreeDataPage(&outputPage);
    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter freeing RInvt.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
    if (reconstructFile) {
      /* R = U S Vt is our convention.  Make multiplcation as sum of
         singular triplets Rnew_ij = (U_ik Sk Vt_kj).  Need to
         multiply all the columns of Ut. Even though some
         singular values may be zero, they may not be at the end of the
         list. */
      if (!Rnewt) {
#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
        Rnewt = m_get(rows, numericalColumns);
#else
        Rnewt = m_get(numericalColumns, rows);
#endif
      }
#if defined(NUMERICAL_RECIPES) || defined(SUNPERF)
      /* only U (with original R memory) and Vt are available. */
      for (i = 0; i < U->m; i++)
        for (j = 0; j < Vt->n; j++) {
          Rnewt->me[j][i] = 0.0;
          for (k = 0; k < numericalColumns; k++)
            Rnewt->me[j][i] += U->me[i][k] * SValueUsed->ve[k] * Vt->me[k][j];
        }
#else
#  if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
      /* U and Vt are available and their base are in column order*/

      /* R = U S Vt */
      if (!V)
        V = m_get(Vt->m, Vt->n);
      for (i = 0; i < Vt->n; i++) {
        for (kk = 0; kk < numericalColumns; kk++)
          V->base[i * Vt->m + kk] = Vt->base[i * Vt->m + kk] * SValueUsed->ve[kk];
      }
      kk = MIN(U->n, V->m);
      lda = MAX(1, U->m);
      ldb = MAX(1, V->m);

#    if defined(SOLARIS)
      f2c_dgemm("N", "N",
                &U->m, &V->n, &kk, &alpha, U->base,
                &lda, V->base, &ldb, &beta, Rnewt->base, &U->m);
#    else
#      if defined(CLAPACK)
      /* Could use cblas_dgemm here in the future. See cblas.h
         void cblas_dgemm(const enum CBLAS_ORDER Order, const enum CBLAS_TRANSPOSE TransA,
         const enum CBLAS_TRANSPOSE TransB, const int M, const int N,
         const int K, const double alpha, const double *A,
         const int lda, const double *B, const int ldb,
         const double beta, double *C, const int ldc);
      */
#if defined(ACCELERATE_NEW_LAPACK)
      dgemm_("N", "N",
             (__LAPACK_int *)&U->m, (__LAPACK_int *)&V->n, &kk, &alpha, U->base,
             (__LAPACK_int *)&lda, V->base, &ldb, &beta, Rnewt->base, (__LAPACK_int *)&U->m);
#else
      dgemm_("N", "N",
             &U->m, &V->n, &kk, &alpha, U->base,
             &lda, V->base, &ldb, &beta, Rnewt->base, &U->m);
#endif
#      endif
#      if defined(MKL)
      dgemm_("N", "N",
             (MKL_INT *)&U->m, (MKL_INT *)&V->n, (MKL_INT *)&kk, &alpha, U->base,
             (MKL_INT *)&lda, V->base, (MKL_INT *)&ldb, &beta, Rnewt->base, (MKL_INT *)&U->m);
#      endif
#      if defined(LAPACK)
      dgemm_("N", "N",
        (lapack_int *)&U->m, (lapack_int *)&V->n, &kk, &alpha, U->base,
             &lda, V->base, &ldb, &beta, Rnewt->base, (lapack_int *)&U->m);
#      endif
#    endif
      m_free(V);
      V = (MAT *)NULL;
      /* here Rnewt->base is in column order */
#  else
      /* only Ut and Vt are available for Meschach,
         Rnew_ij = (U_ik Sk Vt_kj) = (Ut_ki Sk Vt_kj) */
      for (i = 0; i < Ut->n; i++)
        for (j = 0; j < Vt->n; j++) {
          Rnewt->me[j][i] = 0.0;
          for (k = 0; k < numericalColumns; k++)
            Rnewt->me[j][i] += Ut->me[k][i] * SValueUsed->ve[k] * Vt->me[k][j];
        }
#  endif
#endif
      if (verbose & FL_VERYVERBOSE) {
        setformat("%9.6le ");
        fprintf(stderr, "Reconstructed (tranposed)");
        m_foutput(stderr, Rnewt);
      }
      if (!SDDS_StartPage(&reconstructPage, rows) ||
          !SDDS_CopyParameters(&reconstructPage, &inputPage) ||
          !SDDS_CopyArrays(&reconstructPage, &inputPage) ||
          !SDDS_SetParameters(&reconstructPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                              "NumberOfSingularValuesUsed", NSVUsed, NULL) ||
          !SDDS_SetParameters(&reconstructPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                              "DeletedVectors", deletedVector, NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (foundStringColumn) {
        if (!SDDS_SetColumn(&reconstructPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                            outputColumnName, rows,
                            newColumnNamesColumn ? newColumnNamesColumn : stringColumnName))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      for (i = 0; i < numericalColumns; i++) {
        /* to output a matrix one has to write columns of the transpose! */
#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
        if (!SDDS_SetColumnFromDoubles(&reconstructPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                                       (double *)&(Rnewt->base[i * rows]), rows,
                                       numericalColumnName[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#else
        if (!SDDS_SetColumnFromDoubles(&reconstructPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                                       Rnewt->me[i], rows,
                                       numericalColumnName[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#endif
      }
      if (!SDDS_WriteTable(&reconstructPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      m_free(Rnewt);
      Rnewt = (MAT *)NULL;
      SDDS_FreeDataPage(&reconstructPage);
    }

    if (uFile) {
      if (!SDDS_StartTable(&uPage, rows))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_SetColumn(&uPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                          outputColumnName,
                          rows,
                          newColumnNamesColumn ? newColumnNamesColumn : "OriginalRows"))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

#if defined(NUMERICAL_RECIPES)
      /* only U and Vt are available, and U is rectangular */
      for (i = 0; i < rows; i++)
        for (j = 0; j < numericalColumns; j++)
          if (!SDDS_SetRowValues(&uPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                 i, j + 1, U->me[i][j], -1))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_WriteTable(&uPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      m_free(U);
      U = (MAT *)NULL;
      R = (MAT *)NULL;
#else
#  if defined(SUNPERF)
      /* only U and Vt are available, and U is rectangular and U may be smaller when -economy is used. */
      for (i = 0; i < rows; i++)
        for (j = 0; j < U->n; j++)
          if (!SDDS_SetRowValues(&uPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                 i, j + 1, U->me[i][j], -1))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_WriteTable(&uPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      m_free(U);
      U = (MAT *)NULL;
      R = (MAT *)NULL;
#  else
#    if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
      /* U and Vt matrix are available and their base are in column order */
      for (i = 0; i < U->n; i++)
        if (!SDDS_SetColumn(&uPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                            (double *)&(U->base[i * U->m]), rows, orthoColumnName[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_WriteTable(&uPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      m_free(U);
      U = (MAT *)NULL;
#    else
      /* Only Ut and Vt are available for Meschah */
      /* Whether economy mode or not, Ut->m (rows of Ut) is the required number of
         columns of U to write. */
      for (i = 0; i < Ut->m; i++)
        if (!SDDS_SetColumn(&uPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                            Ut->me[i], rows, orthoColumnName[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_WriteTable(&uPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      m_free(Ut);
      Ut = (MAT *)NULL;
#    endif
#  endif
#endif
      SDDS_FreeDataPage(&uPage);
    } else {
      /* free pointers in U or Ut anyhow with -vmatrix not specified.*/
#if defined(NUMERICAL_RECIPES) || defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
      m_free(U);
      U = (MAT *)NULL;
      R = (MAT *)NULL;
#else
      m_free(Ut);
      Ut = (MAT *)NULL;
#endif
    }

    if (vFile) {
      if (!SDDS_StartTable(&vPage, numericalColumns))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      for (i = 0; i < numericalColumns; i++) {
#if defined(CLAPACK) || defined(LAPACK) || defined(MKL)
        /* Vt->base is in column order, that is, Vt->me = V->me */
        for (j = 0; j < numericalColumns; j++)
          if (!SDDS_SetRowValues(&vPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                                 orthoColumnName[j], Vt->me[i][j], NULL))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#else
        if (!SDDS_SetColumn(&vPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                            Vt->me[i], numericalColumns, orthoColumnName[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#endif
        if (!SDDS_SetRowValues(&vPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                               oldColumnNames ? oldColumnNames : "OldColumnNames",
                               numericalColumnName[i], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!SDDS_WriteTable(&vPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      m_free(Vt);
      Vt = (MAT *)NULL;
      SDDS_FreeDataPage(&vPage);
    } else {
      /* free pointers in Vt anyhow with -vmatrix not specified.*/
      m_free(Vt);
      Vt = (MAT *)NULL;
    }

    if (sFile) {
      if (!sFileAsMatrix) {
        if (!SDDS_StartTable(&sPage, numericalColumns))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (i = 0; i < numericalColumns; i++)
          if (!SDDS_SetRowValues(&sPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                 i, "Index", i, NULL))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!SDDS_SetColumn(&sPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                            SValue->ve, numericalColumns, "SingularValues"))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (!SDDS_StartTable(&sPage, economy ? economyRows : rows))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        /*long index; */
        for (i = 0; i < numericalColumns; i++)
          for (j = 0; j < (economy ? economyRows : rows); j++)
            if (!SDDS_SetRowValues(&sPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                   j, i, i == j ? SValue->ve[i] : 0.0, -1)) {
              fprintf(stderr, "Problem setting S[%ld][%ld] of %d x %d matrix\n", i, j,
                      numericalColumns, rows);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
      }
      if (!SDDS_WriteTable(&sPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_FreeDataPage(&sPage);
    }
    /* end of page reading loop */
    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAt the end of the loop.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
  }

  if (!SDDS_Terminate(&inputPage) || !SDDS_Terminate(&outputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (multiplyFile && !SDDS_Terminate(&multiplyPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (uFile) {
    if (!SDDS_Terminate(&uPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (vFile) {
    if (!SDDS_Terminate(&vPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (sFile) {
    if (!SDDS_Terminate(&sPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (reconstructFile) {
    if (!SDDS_Terminate(&reconstructPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (verbose & FL_VERBOSE) {
    report_stats(stderr, "\nAfter terminating SDDS pages.\n");
    if (mem_info_is_on())
      mem_info_file(stderr, 0);
  }
  v_free(SValue);
  v_free(SValueUsed);
  v_free(InvSValue);
  if (deleteVector)
    free(deleteVector);
  if (orthoColumnName) {
    SDDS_FreeStringArray(orthoColumnName, MAX(rows, numericalColumns));
    free(orthoColumnName);
  }
  if (freeMultiCol) {
    SDDS_FreeStringArray(multiplyColumnName, multiplyColumns);
    free(multiplyColumnName);
  }
  if (multiStringCol)
    free(multiStringCol);
  if (inputDescription)
    free(inputDescription);
  if (inputContents)
    free(inputContents);
  if (outputColumnName) {
    SDDS_FreeStringArray(outputColumnName, outputColumns);
    free(outputColumnName);
  }
  if (stringColumnName)
    free(stringColumnName);
  if (numericalColumnName)
    SDDS_FreeStringArray(numericalColumnName, numericalColumns);
  free(numericalColumnName);
  if (actuators) {
    SDDS_FreeStringArray(actuatorName, actuators);
    free(actuatorName);
  }
  if (outputDescription)
    free(outputDescription);

  if (weightsName) {
    SDDS_FreeStringArray(weightsName, weightsRows);
    free(weightsName);
    /*  free(weights);*/
  }

  if (corrWeightsName) {
    SDDS_FreeStringArray(corrWeightsName, corrWeightsRows);
    free(corrWeightsName);
  }

  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile))
    exit(1);
  free_scanargs(&s_arg, argc);
  return (0);
}

/* this routine written by L. Emery */
MAT *m_diag(VEC *diagElements, MAT *A) {
  long i;

  if (!A || !A->me)
    bomb("Problem with allocation of matrix.\n", NULL);
  if (!diagElements)
    bomb("Problem with allocation of vector of diagonal elements.\n", NULL);
  m_zero(A);
  for (i = 0; i < MIN(A->n, A->m); i++)
    A->me[i][i] = diagElements->ve[i];
  return A;
}

int32_t InitializeInputAndGetColumnNames(SDDS_DATASET *SDDS_dataset, char *filename,
                                         char ***numericalColumnName, int32_t *numericalColumns,
                                         char **stringColumnName, char **inputDescription, char **inputContents) {
  char **columnName = NULL, **numColName = NULL;
  int32_t columns = 0, i, numCol, stringCol, columnType;

  if (!SDDS_InitializeInput(SDDS_dataset, filename) ||
      !(columnName = (char **)SDDS_GetColumnNames(SDDS_dataset, &columns)))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (inputDescription && inputContents) {
    if (!SDDS_GetDescription(SDDS_dataset, inputDescription, inputContents))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  numCol = 0;
  stringCol = -1;

  for (i = 0; i < columns; i++) {
    if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(SDDS_dataset, i))) {
      numColName = SDDS_Realloc(numColName, sizeof(*numColName) * (numCol + 1));
      SDDS_CopyString(&numColName[numCol], columnName[i]);
      numCol++;
    } else if (columnType == SDDS_STRING) {
      if (stringCol == -1) {
        stringCol = i;
        SDDS_CopyString(stringColumnName, columnName[i]);
      }
    }
  }
  if (!numCol)
    SDDS_Bomb("No numerical columns provided in the input.");
  *numericalColumns = numCol;
  *numericalColumnName = numColName;
  SDDS_FreeStringArray(columnName, columns);
  free(columnName);
  columnName = NULL;
  return 1;
}

long SDDS_FreeDataPage(SDDS_DATASET *SDDS_dataset) {
  SDDS_LAYOUT *layout;
  char **ptr;
  long i, j;
  layout = &SDDS_dataset->original_layout;
  if (SDDS_dataset->data) {
    for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
      if (SDDS_dataset->data[i]) {
        if (layout->column_definition[i].type == SDDS_STRING) {
          ptr = (char **)SDDS_dataset->data[i];
          for (j = 0; j < SDDS_dataset->n_rows_allocated; j++, ptr++)
            if (*ptr)
              free(*ptr);
        }
        free(SDDS_dataset->data[i]);
        SDDS_dataset->data[i] = NULL;
      }
    }
    SDDS_dataset->n_rows_allocated = 0;
    SDDS_dataset->n_rows = 0;
  }
  return (1);
}

/*free transposed matrix */
int t_free(MAT *mat) {
  if (mat->me)
    free(mat->me);
  free(mat->base);
  free(mat);
  mat = NULL;
  return 1;
}

int m_freePointers(mat)
     MAT *mat;
{
  int i;
  if (mat == (MAT *)NULL || (int)(mat->m) < 0 ||
      (int)(mat->n) < 0)
    /* don't trust it */
    return (-1);

  for (i = 0; i < mat->max_m; i++)
    if (mat->me[i] != (Real *)NULL) {
      if (mem_info_is_on()) {
        mem_bytes(TYPE_MAT, mat->max_n * sizeof(Real), 0);
      }
      free((char *)(mat->me[i]));
    }
  if (mat->me != (Real **)NULL) {
    if (mem_info_is_on()) {
      mem_bytes(TYPE_MAT, mat->max_m * sizeof(Real *), 0);
    }
    free((char *)(mat->me));
  }

  if (mem_info_is_on()) {
    mem_bytes(TYPE_MAT, sizeof(MAT), 0);
    mem_numvar(TYPE_MAT, -1);
  }
  free((char *)mat);

  return (0);
}

#if defined(NUMERICAL_RECIPES)

#  define MAX_STACK 100
/* fix_nr_svd -- fix minor details about SVD from numerical recipes in C
   This is based on a meschach library routine to fix svd.
   -- make singular values non-negative
   -- sort singular values in decreasing order
   -- variables as for bisvd()
   -- no argument checking */
/* Note that meschach's svd decomposes A = (arg1)^T w (arg2) which
   is a different convention as in num. recipes in C: A = (arg1) w
   (arg2)^T, which we use in this program. That's why the
   arguments here appeared to be the tranpose of the calling
   arguments.
   Ut is rectangular with the same number of columns as the
   V. This function will differ from fixsvd in that Ut
   is treated in a "transposed" way relative to U. */
static void fix_nr_svd(Ut, d, V)
     VEC *d;
MAT *Ut, *V;
{
  int i, j, k, l, r, stack[MAX_STACK], sp;
  Real tmp, v;

  /* make singular values non-negative */
  for (i = 0; i < d->dim; i++)
    if (d->ve[i] < 0.0) {
      d->ve[i] = -d->ve[i];
      if (Ut != MNULL)
        for (j = 0; j < Ut->m; j++)
          Ut->me[j][i] = -Ut->me[j][i];
    }

  /* sort singular values */
  /* nonrecursive implementation of quicksort due to R.Sedgewick,
     "Algorithms in C", p. 122 (1990) */
  sp = -1;
  l = 0;
  r = d->dim - 1;
  for (;;) {
    while (r > l) {
      /* i = partition(d->ve,l,r) */
      v = d->ve[r];

      i = l - 1;
      j = r;
      for (;;) { /* inequalities have been set "backwards" for **decreasing** order */
        while (d->ve[++i] > v)
          ;
        while (d->ve[--j] < v)
          ;
        if (i >= j)
          break;
        /* swap entries in d->ve */
        tmp = d->ve[i];
        d->ve[i] = d->ve[j];
        d->ve[j] = tmp;
        /* swap columns of Ut & rows of V as well */
        if (Ut != MNULL)
          for (k = 0; k < Ut->m; k++) {
            tmp = Ut->me[k][i];
            Ut->me[k][i] = Ut->me[k][j];
            Ut->me[k][j] = tmp;
          }
        if (V != MNULL)
          /* can't be clever and swap pointer here, since the memory
             may have been allocated as one long array */
          for (k = 0; k < V->n; k++) {
            tmp = V->me[i][k];
            V->me[i][k] = V->me[j][k];
            V->me[j][k] = tmp;
          }
      }
      tmp = d->ve[i];
      d->ve[i] = d->ve[r];
      d->ve[r] = tmp;
      if (Ut != MNULL)
        for (k = 0; k < Ut->m; k++) {
          tmp = Ut->me[k][i];
          Ut->me[k][i] = Ut->me[k][r];
          Ut->me[k][r] = tmp;
        }
      if (V != MNULL)
        for (k = 0; k < V->n; k++) {
          tmp = V->me[i][k];
          V->me[i][k] = V->me[r][k];
          V->me[r][k] = tmp;
        }
      /* end i = partition(...) */
      if (i - l > r - i) {
        stack[++sp] = l;
        stack[++sp] = i - 1;
        l = i + 1;
      } else {
        stack[++sp] = i + 1;
        stack[++sp] = r;
        r = i - 1;
      }
    }
    if (sp < 0)
      break;
    r = stack[sp--];
    l = stack[sp--];
  }
}
#endif

#if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
void *SDDS_GetCastMatrixOfRows_SunPerf(SDDS_DATASET *SDDS_dataset, int32_t *n_rows, long sddsType) {
  void *data;
  long i, j, k, size;
  /*  long type;*/
  if (!SDDS_CheckDataset(SDDS_dataset, "SDDS_GetCastMatrixOfRows_SunPerf"))
    return (NULL);
  if (!SDDS_NUMERIC_TYPE(sddsType)) {
    SDDS_SetError("Unable to get matrix of rows--no columns selected (SDDS_GetCastMatrixOfRows_SunPerf) (1)");
    return NULL;
  }
  if (SDDS_dataset->n_of_interest <= 0) {
    SDDS_SetError("Unable to get matrix of rows--no columns selected (SDDS_GetCastMatrixOfRows_SunPerf) (2)");
    return (NULL);
  }
  if (!SDDS_CheckTabularData(SDDS_dataset, "SDDS_GetCastMatrixOfRows_SunPerf"))
    return (NULL);
  size = SDDS_type_size[sddsType - 1];
  if ((*n_rows = SDDS_CountRowsOfInterest(SDDS_dataset)) <= 0) {
    SDDS_SetError("Unable to get matrix of rows--no rows of interest (SDDS_GetCastMatrixOfRows_SunPerf) (3)");
    return (NULL);
  }
  for (i = 0; i < SDDS_dataset->n_of_interest; i++) {
    if (!SDDS_NUMERIC_TYPE(SDDS_dataset->layout.column_definition[SDDS_dataset->column_order[i]].type)) {
      SDDS_SetError("Unable to get matrix of rows--not all columns are numeric (SDDS_GetCastMatrixOfRows_SunPerf) (4)");
      return NULL;
    }
  }
  if (!(data = (void *)SDDS_Malloc(size * (*n_rows) * SDDS_dataset->n_of_interest))) {
    SDDS_SetError("Unable to get matrix of rows--memory allocation failure (SDDS_GetCastMatrixOfRows_SunPerf) (5)");
    return (NULL);
  }
  for (j = k = 0; j < SDDS_dataset->n_rows; j++) {
    if (SDDS_dataset->row_flag[j]) {
      for (i = 0; i < SDDS_dataset->n_of_interest; i++)
        SDDS_CastValue(SDDS_dataset->data[SDDS_dataset->column_order[i]], j,
                       SDDS_dataset->layout.column_definition[SDDS_dataset->column_order[i]].type,
                       sddsType, (char *)data + (k + i * SDDS_dataset->n_rows) * sizeof(Real));
      k++;
    }
  }
  return (data);
}
#endif
