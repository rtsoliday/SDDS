/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution.
\*************************************************************************/

#if defined(__APPLE__)
#  define ACCELERATE_NEW_LAPACK
#  include <Accelerate/Accelerate.h>
#endif

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matrixop.h"

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
/*
  LAPACK integer-size compatibility note:

  Some systems may ship a liblapack.so soname that resolves to an ILP64
  implementation (64-bit Fortran INTEGER) rather than the traditional LP64
  implementation (32-bit Fortran INTEGER). If code passes 32-bit integer
  storage to an ILP64 runtime, routines such as DGESDD may fail with errors
  like "parameter number 5 had an illegal value" (LDA corruption).

  To remain runnable with either LP64 or ILP64 LAPACK at runtime, the LAPACK
  code path below uses 64-bit integer storage (long long) and casts pointers
  to lapack_int* when calling the Lapacke-declared Fortran entry points.
*/
#endif

/* matrix.h requires machine.h  err.h and meminfo.h. */
#include "matrix.h"
#include "matrix2.h"
#include <setjmp.h>
#include "err.h"
/* for some reason, this function was not prototyped
   in the matrix2.h header file */
char *setformat(char *f_string);

#include "SDDS.h"

#define CLO_MINIMUM_SINGULAR_VALUE_RATIO 0
#define CLO_VERBOSE 1
#define CLO_COLUMNROOT 2
#define CLO_SYMBOL 3
#define CLO_KEEP_SINGULAR_VALUES 4
#define CLO_ASCII 5
#define CLO_DIGITS 6
#define CLO_PIPE 7
#define CLO_UMATRIX 8
#define CLO_VMATRIX 9
#define CLO_REMOVEDCVECTORS 10
#define CLO_NOWARNINGS 11
#define CLO_SMATRIX 12
#define CLO_DELETEVECTORS 13
#define CLO_REMOVE_SINGULAR_VALUES 14
#define CLO_ECONOMY 15
#define CLO_PRINTPACKAGE 16
#define CLO_MAJOR_ORDER 17
#define CLO_LAPACK_METHOD 18
#define CLO_THREADS 19
#define CLO_PCA 20
#define CLO_TAU 21
#define CLO_NUMSV 22
#define CLO_TURN 23
#define CLO_COLUMN 24
#define CLO_OLD_COLUMN_NAMES 25
#define CLO_ICA_SFILE 26
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
  "uMatrix",
  "vMatrix",
  "removeDCVectors",
  "noWarnings",
  "sFile",
  "deleteVectors",
  "smallestSingularValues",
  "economy",
  "printPackage",
  "majorOrder",
  "lapackMethod",
  "threads",
  "pca",
  "tauList",
  "numberSingularValue",
  "turnNumber",
  "column",
  "oldColumnNames",
  "icaSFile"};

static char *USAGE = "sddsica [<input>] [<output>] [-pipe=[input][,output]]\n\
    [{-minimumSingularValueRatio=<value> | -largestSingularValues=<number>}] \n\
    [-smallestSingularValues=<number>] | -numberSingularValue=<number> \n\
    [-deleteVectors=<list of vectors separated by comma>] \n\
    [-economy] [-printPackage] [-threads=<number>]\n\
     [{-root=<string> [-digits=<integer>] | \n\
     [-sFile=<file>[,matrix]] [-uMatrix=<file>] [-vMatrix=<file>] \n\
    [-majorOrder=row|column] [-lapackMethod={simple|divideAndConquer}] \n\
    [-symbol=<string>] [-ascii] [-verbose] [-noWarnings] [-pca] \n\
    [-tauList=<list of integers separated by comma>] \n\
    [-turnNumber=<number>[,|<number>]] [-column]  [-oldColummNames=<string>] \n\
    [-icaSFile=<filename>] \n\\n\n";
static char *USAGE2 = "Perform singular value decomposition for a matrix in a SDDS file.\n\
pipe           reads input from and/or write output to a pipe. ICA A matrix will be the output. \n\
output         write the ICA A matrix to output file.\n\
icaSFile       provide the filename for writing ICA S matrix. \n\
minimumSingularValueRatio\n\
               rejects singular values less than the largest\n\
               singular value times this ratio.\n\
largestSingularValues\n\
               retains only the first \"largestSingularValues\"\n\
               largest singularvalues.\n\
smallestSingularValues\n\
               remove the the last \"smallestSingularValues\" smallest singularvalues. \n\
               of modes n1,n2,n3, ect to zero. \n\
numberSingularValue\n\
				keep \"numberSingularValue\" singular values. \n\
deleteVectors  -deleteVectors=n1,n2,n3,... which will set the inverse singular values \n\
               The order in which the SV removal options are processed is \n\
               numberSingularValue, minimumSingularValueRatio, largestSingularValues \n\
               and then deleteVectors.\n\
economy        only the first min(m,n) columns for the U matrix are calculated or returned \n\
               where m is the number of rows and n is the number of columns. This \n\
               can potentially reduce the computation time with no loss of useful information.\n\
               economy option is highly recommended for most pratical applications since it uses\n\
               less memory and runs faster. If economy option is not give, a full m by m U matrix \n\
               will be internally computated no matter whether -uMatrix is provided. \n\
lapackMethod   give option of calling lapack svd routine, \"simple\" for dgesvd, and \"divideAndConquer\" \n\
               for dgesdd, the later is claimed to have better performance than the former.\n\
pca            if provided, will do only PCA computation (i.e. SVD), otherwise, perfomr ICA. \n\
tauList		  -tauList=n1,n2,n3,... (up to 6) setting time lag constants, default to 0, 1, 2, 3.\n\
turnNumber	  -turnNumber=<turn1>,<nturn>, turn1 is first turn, nturn is the number of turns to use \n\
			   in the data matrix. If only one number is given, it is considered nturn, with turn1=0. \n\
column         provide list of columns to do ICA analysis, wild cards are accepted. \n\
oldColumnNames specify a name for the output file (*.S) column name to save the columns names used in ICA analysis.\n\
majorOrder     specity output file in row or column major order.\n";
static char *USAGE3 = "root           use the string specified to generate column names.\n\
               Default for column names is the first string column in\n\
               <inputfile>. If there is no string column, then the column\n\
               names are formed with the root \"Column\".\n\
digits         minimum number of digits used in the number appended to the root\n\
               part of the column names. Default is value 3.\n\
sFile, uMatrix, vMatrix writes the u and v column-orthogonal matrices \n\
               and the singular values vector to files. \n\
               The SVD decomposition follows the convention A = u (SValues) v^T \n\
               The \"transformed\" x are v^T x, and the \"transformed\" y are u^T y.\n";

static char *USAGE4 = "symbol         use the string specified for the symbol field for all columns definitions.\n\
ascii          writes the output file data in ascii mode (default is binary).\n\
verbose        prints out to stderr input and output matrices.\n\
printPackage   prints out the linear algebra package that was compiled.\n\
noWarnings     prevents printing of warning messages.\n\
Program by Xiaobiao Huang, ANL ("__DATE__")\n";

#define MAX_TAU 6
#define FL_VERBOSE 1
#define FL_VERYVERBOSE 2

MAT *m_diag(VEC *diagElements, MAT *A);

long SDDS_FreeDataPage(SDDS_DATASET *SDDS_dataset);
int m_freePointers(MAT *mat);
int t_free(MAT *mat);

void *SDDS_GetCastMatrixOfRows_SunPerf(SDDS_DATASET *SDDS_dataset, int32_t *n_rows, long sddsType);

int32_t InitializeInputAndGetColumnNames(SDDS_DATASET *SDDS_dataset, char *filename, int32_t inputColumnNames0, char **inputColumnName,
                                         char ***numericalColumnName, int32_t *numericalColumns,
                                         char **stringColumName, char **inputDescription, char **inputContents);

void SetupOutputFile(SDDS_TABLE *outputPage, SDDS_TABLE *inputPage, long ascii, u_int cols, long type, char *outputfile,
                     char *root, char *symbol, long digits, char *stringColName, short columnMajorOrder);
void WriteOutputPage(SDDS_TABLE *outputPage, long type, MAT *mat, double ratio, long NSVUsed, char *deletedVectgor, double conditionNumber, char *inputFile, VEC *SValue, long sValues, VEC *SValueUsed, int32_t stringNames, char **stringName, long *betaPair);

MAT *m_covmat(MAT *X, int tau);
void m_jointdiag(MAT **A, MAT *V, int ntau, double el);
void mmtr_mult(MAT *A, MAT *B, MAT *out);
void mvmtr_mult(MAT *A, VEC *Lamb, MAT *B, MAT *out);
void mv_mult(MAT *A, VEC *Lamb, MAT *out);
MAT *mcopy_Nrow(MAT *A, long row);
MAT *mcopy_Nrow2(MAT *A, long row1, long nrow);
long findICAModePairs(MAT **covR, long ntau, long *pair);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE inputPage, outputPageA, outputPageS, uPage, vPage, sPage;

  char *inputfile, *outputfile, *uFile, *vFile, *sFile, *icaSFile;
  char **outputColumnName = NULL, **orthoColumnName = NULL, **numericalColumnName = NULL;

  char **inputColumnName0, *oldColumnNames, *stringColumnName;
  int32_t inputColumnNames0;

  double largestS;
  int32_t numericalColumns;
  long sFileAsMatrix;
  char *inputDescription = NULL, *inputContents = NULL;
  long i_arg;
  register long i, j, k;

  long removeDCVectors, verbose;
  int32_t rows, rowsPrevPage, outputColumns;
  char format[80];
  char *Symbol, *Root;
  VEC *SValue, *SValueUsed, *InvSValue;
  MAT *R, *RInvt, *RInvR, *Rnewt, *U, *Ut, *V, *Vt, *newU = NULL, *newV = NULL;
  MAT *A = NULL, *s = NULL, *W = NULL, *Wt = NULL;
  double el = 1e-8;

  double ratio;
  long nlargest, nsmallest;
  long NSVUsed;
  long ascii;
  long digits;
  unsigned long pipeFlags;
  long tmpfile_used, noWarnings;
  long ipage;
  char *newColumnNamesColumn;
  double conditionNumber, max, min;
  unsigned long majorOrderFlag;
  long *deleteVector, deleteVectors, firstdelete;
  char deletedVector[1024];
  long printPackage;
  short columnMajorOrder = -1, lapackMethod = 1;
  /* Flag economy changes the calculation mode of the lapack-type calls to svd
     and may later be used by a modified meschach svd routine to
     reduce the number of columns returned for the U matrix.
     The ecomony mode is already the only mode for NR and for sunperf. */
  long economy, economyRows = 0, ica = 1;
  long threads = 1;
#if defined(CLAPACK)
  int *iwork = NULL;
#endif
  MAT **covR;
  long tauList[MAX_TAU] = {0, 1, 2, 3}, tauListN;
  long ntau = 4;
  long numSV = 0;
  long turn1 = 0, turnNum = 0;
  long npair = 0, betaPair[4];

#if defined(NUMERICAL_RECIPES) || defined(SUNPERF)
  register Real x;
#endif

#if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
  /* default is standard svd calculation with square U matrix */
  char calcMode = 'A';
#endif
#if defined(CLAPACK)
  double *work;
  long lwork;
  long lda;
  int info;
#endif
#if defined(MKL)
  double *work;
#endif
#if defined(LAPACK)
  doublereal *work;
  long long lwork;
  long long lda;
  int info;
#endif

  betaPair[0] = betaPair[1] = betaPair[2] = betaPair[3] = 0;
  inputColumnName0 = NULL;
  oldColumnNames = NULL;
  inputColumnNames0 = 0;
  outputColumns = 0;
  rowsPrevPage = 0;
  SValue = SValueUsed = InvSValue = (VEC *)NULL;
  R = RInvt = RInvR = Rnewt = U = Ut = V = Vt = (MAT *)NULL;
  covR = (MAT **)malloc(sizeof(*covR) * MAX_TAU);

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s", USAGE, USAGE2, USAGE3, USAGE4);
    exit(1);
  }
  deletedVector[0] = '\0';
  firstdelete = 1;
  inputfile = outputfile = icaSFile = NULL;
  verbose = 0;
  Symbol = Root = NULL;
  uFile = vFile = sFile = NULL;

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
  newColumnNamesColumn = NULL;
  conditionNumber = 0.0;
  removeDCVectors = 0;

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
      case CLO_PCA:
        ica = 0;
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
        economy = 1;
        break;
      case CLO_UMATRIX:
        if (s_arg[i_arg].n_items < 2 || !(uFile = s_arg[i_arg].list[1]))
          bomb("No uMatrix string given", USAGE);
        break;
      case CLO_VMATRIX:
        if (!(vFile = s_arg[i_arg].list[1]))
          bomb("No vMatrix string given", USAGE);
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
      case CLO_DELETEVECTORS:
        deleteVectors = s_arg[i_arg].n_items - 1;
        deleteVector = (long *)malloc(sizeof(*deleteVector) * deleteVectors);
        for (j = 0; j < deleteVectors; j++) {
          if (!(get_long(&deleteVector[j], s_arg[i_arg].list[j + 1])))
            bomb("non numeric value given in -deleteVectors option", USAGE);
        }
        break;
      case CLO_TAU:
        tauListN = s_arg[i_arg].n_items - 1;
        // deleteVector=(long*)malloc(sizeof(*deleteVector)*deleteVectors);
        for (j = 0; j < MIN(MAX_TAU, tauListN); j++) {
          if (!(get_long(&tauList[j], s_arg[i_arg].list[j + 1])))
            bomb("non numeric value given in -tauList option", USAGE);
        }
        ntau = MIN(MAX_TAU, tauListN);
        break;
      case CLO_NUMSV: /*choose the number of singular value to keep */
        if (!(get_long(&numSV, s_arg[i_arg].list[1]))) {
          bomb("non numeric value given in -numberSingularValue option", USAGE);
        }
        if (numSV < 0) {
          bomb("zero or negative was value given in -numberSingularValue option", USAGE);
        }
        break;
      case CLO_TURN: /*choose the number of turns to keep */
        if (s_arg[i_arg].n_items - 1 >= 2) {
          if (!(get_long(&turn1, s_arg[i_arg].list[1]))) {
            bomb("non numeric value given in -turnNumber option", USAGE);
          }
          if (!(get_long(&turnNum, s_arg[i_arg].list[2]))) {
            bomb("non numeric value given in -turnNumber option", USAGE);
          }
        } else {
          if (!(get_long(&turnNum, s_arg[i_arg].list[1]))) {
            bomb("non numeric value given in -turnNumber option", USAGE);
          }
          if (numSV < 0) {
            bomb("zero or negative was value given in -turnNumber option", USAGE);
          }
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
      case CLO_COLUMN:
        inputColumnName0 = trealloc(inputColumnName0, sizeof(*inputColumnName0) * (inputColumnNames0 + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          inputColumnName0[i - 1 + inputColumnNames0] = s_arg[i_arg].list[i];
        inputColumnNames0 += s_arg[i_arg].n_items - 1;
        break;
      case CLO_OLD_COLUMN_NAMES:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -oldColummNames syntax.");
        if (!(oldColumnNames = s_arg[i_arg].list[1]))
          bomb("No oldColumnNames string given", USAGE);
        break;
      case CLO_ICA_SFILE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -icaSFile syntax.");
        if (!s_arg[i_arg].list[1])
          bomb("No icaSFile string given", USAGE);
        SDDS_CopyString(&icaSFile, s_arg[i_arg].list[1]);
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
    fprintf(stderr, "MESCHACH not available\n");
    exit(1);
#endif
  }

#if defined(MESCHACH)
  fprintf(stderr, "MESCHACH not available\n");
  exit(1);
#endif

  /*if given output file name but icaSFile matrix not provided, use <outputfile>.S for s matrix */
  if (!icaSFile && outputfile) {
    char tempstr[1024];
    sprintf(tempstr, "%s.S", outputfile);
    SDDS_CopyString(&icaSFile, tempstr);
  }
  processFilenames("sddspseudoinverse", &inputfile, &outputfile, pipeFlags, noWarnings, &tmpfile_used);
  if (inputfile && tmpfile_used && verbose & FL_VERBOSE)
    fprintf(stderr, "Warning: input file %s will be overwritten.\n", inputfile);

  if ((nlargest && ratio) || (nlargest && nsmallest) || (nsmallest && ratio) || (numSV && ratio) || (numSV && nlargest) || (numSV && nsmallest))
    SDDS_Bomb("Can only specify one of minimumSingularValueRatio, largestSingularValues and smallestSingularValues options.\n");
  if (newColumnNamesColumn && Root)
    SDDS_Bomb("-root and -newColumnNames are incompatible");
  if (verbose & FL_VERBOSE) {
    report_stats(stderr, "\nBefore initializing SDDS input.\n");
  }
  InitializeInputAndGetColumnNames(&inputPage, inputfile, inputColumnNames0, inputColumnName0, &numericalColumnName, &numericalColumns,
                                   &stringColumnName, &inputDescription, &inputContents);

  if (inputColumnNames0)
    free(inputColumnName0);

  if (verbose & FL_VERBOSE) {
    report_stats(stderr, "\nAfter initializing SDDS input.\n");
  }
  ipage = 0;
  while (0 < SDDS_ReadTable(&inputPage)) {
    ipage++;
    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter reading page.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
    if (!SDDS_SetColumnFlags(&inputPage, 0))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    if (!SDDS_SetColumnsOfInterest(&inputPage, SDDS_NAME_ARRAY, numericalColumns, numericalColumnName))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    if (!(rows = SDDS_CountRowsOfInterest(&inputPage))) {
      SDDS_Bomb("No rows in dataset.");
    }
    if (uFile) {
      if (ipage == 1)
        rowsPrevPage = outputColumns = rows;
      if (ipage == 1 || rows != rowsPrevPage) {
        if (outputColumnName) {
          SDDS_FreeStringArray(outputColumnName, outputColumns);
          free(outputColumnName);
          outputColumnName = NULL;
        }
        if (verbose & FL_VERBOSE)
          fprintf(stderr, "Page %ld has %" PRId32 " rows.\n", ipage, rows);
        if (!Root && stringColumnName) {
          /* use specified string column, or first string column encountered */
          if (!newColumnNamesColumn) {
            /* first string column encountered */
            outputColumnName = (char **)SDDS_GetColumn(&inputPage, stringColumnName);
          } else {
            /* use specified string column */
            if (SDDS_CheckColumn(&inputPage, newColumnNamesColumn, NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY)
              SDDS_Bomb("column named with -newColumnNames does not exist in input");
            outputColumnName = (char **)SDDS_GetColumn(&inputPage, newColumnNamesColumn);
          }
        } else {
          /* use command line options to produce column names in the output file */
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
        outputColumns = rowsPrevPage = rows;
      }
    }
    /*Turning this off because it is crashes when the memory goes above INT_MAX bytes. Plus I don't trust it anyways becuase it does not match what the system is reporting.*/
    mem_info_on(0);
    /*    R = m_get( rows, numericalColumns); */
    /* allocate R but without the data memory */
    /*fprintf(stderr, "allocat memory for R\n");*/
    if (!R) {
      R = NEW(MAT);
      R->m = R->max_m = rows;
      R->n = R->max_n = numericalColumns;
      if (mem_info_is_on()) {
        mem_bytes(TYPE_MAT, 0, sizeof(MAT));
        mem_numvar(TYPE_MAT, 1);
      }
    }
    if (verbose & FL_VERBOSE)
      fprintf(stderr, "R->m %d R->n %d\n", R->m, R->n);

    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter partial R allocation (if first loop).\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
    /* it is assumed that the non-numerical columns are filtered out */
    /* On subsequent loops, the memory pointed to by R->me is supposed to be freed */

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

    /*
     * reduce the number of turns if turnNUM is given
     */
    if (verbose & FL_VERBOSE)
      fprintf(stdout, "turn 1=%ld, turn number %ld\n", turn1, turnNum);
    if (turnNum > 0) {
      if (turnNum > R->m)
        turnNum = R->m;
      else {
        MAT *tmp = R;
        R = mcopy_Nrow2(R, turn1, turnNum);
        m_free(tmp);
      }
    }

    if (mem_info_is_on()) {
      /* because memory has been allocated outside of the meschach
         library we need to run these commands */
      mem_bytes(TYPE_MAT, 0, rows * numericalColumns * sizeof(Real));
      mem_bytes(TYPE_MAT, 0, rows * sizeof(Real *));
    }
    /* free data page memory as soon as possible */
    SDDS_FreeDataPage(&inputPage);
    /*fprintf(stderr, "R data assigned.\n");*/
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
    // debugging
    // fprintf(stderr, "Input R: ");
    //  matrix_output( stdout, R);

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

    /* ICA additions: remove offsets in each column
     * //R->[i][k]: the k'th column, i'th row, where  X has m rows, n columns,
     * 1/15/2021
     */
    // matrix_output(stdout, R);
    if (1) {
      double sum;
      for (i = 0; i < R->n; i++) {
        sum = 0.0;
        for (j = 0; j < R->m; j++) {
          // fprintf(stdout,"%f \t", R->me[i][j]);
          sum += R->me[i][j]; /* column i, row j */
        }
        // fprintf(stdout,"\n");

        sum /= R->m;
        for (j = 0; j < R->m; j++) {
          R->me[i][j] -= sum;
        }
      }
    }
    //    matrix_output(stdout, R);
    /* End of ICA additions: remove offsets in each column
     * 1/15/2021
     */

#if defined(SUNPERF) || defined(CLAPACK) || defined(LAPACK) || defined(MKL)
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
#endif
#if defined(CLAPACK)
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
#endif
#if defined(MKL)
    /*
       IMPORTANT (Windows + MKL):
       Avoid casting pointers like &R->m, &lda, &lwork to MKL_INT*. Under
       MKL ILP64, MKL_INT is 64-bit and such casts corrupt arguments.
    */
    {
      MKL_INT mMkl, nMkl, ldaMkl, lduMkl, ldvtMkl, lworkMkl, infoMkl;
      MKL_INT *iworkMkl = NULL;
      size_t lworkAlloc;

      work = (double *)malloc(sizeof(double) * 1);
      mMkl = (MKL_INT)R->m;
      nMkl = (MKL_INT)R->n;
      ldaMkl = (MKL_INT)MAX(1, R->m);
      lduMkl = (MKL_INT)R->m;
      ldvtMkl = (MKL_INT)R->n;
      lworkMkl = -1;
      infoMkl = 0;

      if (lapackMethod == 1) {
        MKL_INT minMN = mMkl < nMkl ? mMkl : nMkl;
        iworkMkl = (MKL_INT *)malloc(sizeof(*iworkMkl) * (size_t)(8 * (size_t)minMN));
        dgesdd_((char *)&calcMode, &mMkl, &nMkl,
                (double *)R->base, &ldaMkl,
                (double *)SValue->ve,
                (double *)U->base, &lduMkl,
                (double *)Vt->base, &ldvtMkl,
                (double *)work, &lworkMkl, iworkMkl,
                &infoMkl);
      } else {
        dgesvd_((char *)&calcMode, (char *)&calcMode, &mMkl, &nMkl,
                (double *)R->base, &ldaMkl,
                (double *)SValue->ve,
                (double *)U->base, &lduMkl,
                (double *)Vt->base, &ldvtMkl,
                (double *)work, &lworkMkl,
                &infoMkl);
      }

      lworkMkl = (MKL_INT)work[0];
      if (lworkMkl <= 0)
        SDDS_Bomb("Error: invalid workspace size returned by MKL LAPACK SVD call.");
      if (verbose & FL_VERYVERBOSE)
        fprintf(stderr, "Work space size returned from dgesvd_ is %lld.\n", (long long)lworkMkl);

      lworkAlloc = (size_t)lworkMkl;
      work = (double *)realloc(work, sizeof(double) * lworkAlloc);
      if (lapackMethod == 1) {
        dgesdd_((char *)&calcMode, &mMkl, &nMkl,
                (double *)R->base, &ldaMkl,
                (double *)SValue->ve,
                (double *)U->base, &lduMkl,
                (double *)Vt->base, &ldvtMkl,
                (double *)work, &lworkMkl, iworkMkl,
                &infoMkl);
      } else {
        dgesvd_((char *)&calcMode, (char *)&calcMode, &mMkl, &nMkl,
                (double *)R->base, &ldaMkl,
                (double *)SValue->ve,
                (double *)U->base, &lduMkl,
                (double *)Vt->base, &ldvtMkl,
                (double *)work, &lworkMkl,
                &infoMkl);
      }

      if (iworkMkl)
        free(iworkMkl);
      free(work);
    }
    /* do not need R now can free it*/
    t_free(R);
    R = (MAT *)NULL;
    /*removed the previous codes here; do not tranpose matrix, but take the advantage of
      the column order of U and Vt matrices */
#endif
#if defined(LAPACK)
    {
      long long m64, n64, ldu64, ldvt64;
      long long info64 = 0;
      long long minMN;
      size_t lworkAlloc;
      void *iwork64 = NULL;

      work = (doublereal *)malloc(sizeof(doublereal) * 1);
      lwork = -1;

      m64 = (long long)R->m;
      n64 = (long long)R->n;
      lda = (long long)MAX(1, R->m);
      ldu64 = (long long)R->m;
      ldvt64 = (long long)R->n;

      if (lapackMethod == 1) {
        minMN = (m64 < n64) ? m64 : n64;
        iwork64 = malloc(sizeof(long long) * (size_t)(8 * (size_t)minMN));
        LAPACK_dgesdd((char *)&calcMode, (lapack_int *)&m64, (lapack_int *)&n64,
                      (double *)R->base, (lapack_int *)&lda,
                      (double *)SValue->ve,
                      (double *)U->base, (lapack_int *)&ldu64,
                      (double *)Vt->base, (lapack_int *)&ldvt64,
                      (double *)work, (lapack_int *)&lwork, (lapack_int *)iwork64,
                      (lapack_int *)&info64);
      } else {
        LAPACK_dgesvd((char *)&calcMode, (char *)&calcMode, (lapack_int *)&m64, (lapack_int *)&n64,
                      (doublereal *)R->base, (lapack_int *)&lda,
                      (doublereal *)SValue->ve,
                      (doublereal *)U->base, (lapack_int *)&ldu64,
                      (doublereal *)Vt->base, (lapack_int *)&ldvt64,
                      (doublereal *)work, (lapack_int *)&lwork,
                      (lapack_int *)&info64);
      }

      lwork = (long long)work[0];
      if (lwork <= 0)
        SDDS_Bomb("Error: invalid workspace size returned by LAPACK SVD call.");
      if (verbose & FL_VERYVERBOSE)
        fprintf(stderr, "Work space size returned from dgesvd_ is %lld.\n", lwork);

      lworkAlloc = (size_t)lwork;
      work = (doublereal *)realloc(work, sizeof(doublereal) * lworkAlloc);

      if (lapackMethod == 1) {
        LAPACK_dgesdd((char *)&calcMode, (lapack_int *)&m64, (lapack_int *)&n64,
                      (double *)R->base, (lapack_int *)&lda,
                      (double *)SValue->ve,
                      (double *)U->base, (lapack_int *)&ldu64,
                      (double *)Vt->base, (lapack_int *)&ldvt64,
                      (double *)work, (lapack_int *)&lwork, (lapack_int *)iwork64,
                      (lapack_int *)&info64);
      } else {
        LAPACK_dgesvd((char *)&calcMode, (char *)&calcMode, (lapack_int *)&m64, (lapack_int *)&n64,
                      (doublereal *)R->base, (lapack_int *)&lda,
                      (doublereal *)SValue->ve,
                      (doublereal *)U->base, (lapack_int *)&ldu64,
                      (doublereal *)Vt->base, (lapack_int *)&ldvt64,
                      (doublereal *)work, (lapack_int *)&lwork,
                      (lapack_int *)&info64);
      }

      if (iwork64)
        free(iwork64);
      free(work);
      work = NULL;

      info = (int)info64;
      if (info) {
        if (info < 0) {
          fprintf(stderr, "** LAPACK error: illegal value in argument %d to %s\n",
                  -info, lapackMethod == 1 ? "DGESDD" : "DGESVD");
        } else {
          fprintf(stderr, "** LAPACK error: %s failed to converge (info=%d)\n",
                  lapackMethod == 1 ? "DGESDD" : "DGESVD", info);
        }
        exit(1);
      }
    }
    /* do not need R now can free it*/

    t_free(R);
    R = (MAT *)NULL;
    /*removed the previous codes here; do not tranpose matrix, but take the advantage of
      the column order of U and Vt matrices */
#endif

    /*fprintf(stderr, "svd done\n");*/

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
    /*SVD done here */
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
        InvSValue->ve[i] = 1 / SValue->ve[i];
        SValueUsed->ve[i] = SValue->ve[i];
        max = MAX(SValueUsed->ve[i], max);
        min = MIN(SValueUsed->ve[i], min);
        NSVUsed++;
        if (numSV > 0) {
          if (NSVUsed >= numSV)
            break;
        }
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

    /*
     * Start ica code here
     *
     * */
    /*newV is the transpose of Vt */
    V = (MAT *)matrix_transpose(Vt);
    if (verbose & FL_VERBOSE)
      fprintf(stderr, "V m=%d, n=%d\n", V->m, V->n);

    newU = matrix_get(U->m, NSVUsed);
    newV = matrix_get(Vt->m, NSVUsed);
    k = 0;
    for (i = 0; i < numericalColumns; i++) {
      if (k + 1 > NSVUsed)
        break;
      if (SValueUsed->ve[i]) {
        // fprintf(stderr, "i=%d k=%d\n", i, k);
        /* newU->me[k] = U->me[i]; */
        memcpy((char *)newU->me[k], (char *)U->me[k], sizeof(*newU->base) * newU->m);
        memcpy((char *)newV->me[k], (char *)V->me[k], sizeof(*newV->base) * newV->m);

        k++;
      }
    }
    if (verbose & FL_VERBOSE) {
      fprintf(stderr, "newU m=%d, n=%d\n", newU->m, newU->n);
      fprintf(stderr, "newV m=%d, n=%d\n", newV->m, newV->n);
    }
    /*column order matrix can not use m_foutput to print */
    /*
      fprintf(stdout, "newU matrix\n");
      matrix_output(stdout, newU);
      fprintf(stdout, "newV matrix\n");
      matrix_output(stdout, newV);
    */
    if (ica) {

      for (i = 0; i < ntau; i++) {
        /* fprintf(stderr, "tau=%d\n", tauList[i]); */
        /*newU should remove the removed singular values */
        covR[i] = m_covmat(newU, tauList[i]);

        /*for debugging purpose*/
        /*      fprintf(stdout,"tau=%d m=%d n=%d\n\n",tauList[i], covR[i]->m, covR[i]->n);
                matrix_output(stdout,covR[i]); */
      }
      /* joint diagonalization */

      el = 1.0e-8;
      if (!W || !W->me)
        W = m_get(covR[0]->m, covR[0]->m);

      m_jointdiag(covR, W, ntau, el);
      /* fprintf(stdout,"Decoupling matrix:\n");
         matrix_output(stdout,W); */
      Wt = matrix_transpose(W);
      /* fprintf(stdout,"W transpose:\n");
         matrix_output(stdout,Wt); */

      s = matrix_get(U->m, NSVUsed);
      mmtr_mult(newU, Wt, s);
      /*    fprintf(stdout,"s-matrix:\n");
            matrix_output(stdout,s);*/

      A = matrix_get(newV->m, NSVUsed);
      // mmtr_mult(newV,Wt,A);
      mvmtr_mult(newV, SValueUsed, W, A);

      /*    fprintf(stdout,"A-matrix:\n");
            matrix_output(stdout,A);   */

      /*for debugging purpose*/
      /*    fprintf(stdout,"after JD\n");
            for( i=0;i<ntau;i++) {
            fprintf(stdout, "tau=%d\n", tauList[i]);
            matrix_output(stdout,covR[i]);
            } */

      /*    fprintf(stderr,"looking for betatron mode pair\n");*/
      npair = findICAModePairs(covR, ntau, betaPair);
      if (verbose & FL_VERBOSE) {
        if (npair > 0)
          fprintf(stdout, "betatron mode pair %d: [%ld, %ld]\n", 1, betaPair[1], betaPair[0]);
        if (npair > 1)
          fprintf(stdout, "betatron mode pair %d: [%ld, %ld]\n", 2, betaPair[3], betaPair[2]);
      }
    } else { /*PCA */
      A = matrix_get(newV->m, NSVUsed);
      mv_mult(newV, SValueUsed, A);
    }
    /*ica is done here */
    /*********************                      \
     * define output page *
     \*********************/
    /*newU is for S (source signal) matrix, newV is for A (mixing) matrix */
    digits = MAX(digits, log10(newU->n) + 1);
    digits = MAX(digits, log10(newV->n) + 1);
    if (ipage == 1) {
      if (!ica) {
        if (icaSFile)
          SetupOutputFile(&outputPageS, &inputPage, ascii, newU->n, 1, icaSFile, Root, Symbol, digits, oldColumnNames, columnMajorOrder);
        SetupOutputFile(&outputPageA, &inputPage, ascii, A->n, 0, outputfile, Root, Symbol, digits, oldColumnNames, columnMajorOrder);
      } else {
        if (icaSFile)
          SetupOutputFile(&outputPageS, &inputPage, ascii, s->n, 1, icaSFile, Root, Symbol, digits, oldColumnNames, columnMajorOrder);
        SetupOutputFile(&outputPageA, &inputPage, ascii, A->n, 0, outputfile, Root, Symbol, digits, oldColumnNames, columnMajorOrder);
      }

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
    }
    /* Write to each table in turn, and deallocate page data as soon as possible
       to make memory usage emore efficient. */
    if (!ica) {
      if (icaSFile)
        WriteOutputPage(&outputPageS, 1, newU, ratio, NSVUsed, deletedVector, conditionNumber, inputfile, SValue, numericalColumns, SValueUsed, numericalColumns, numericalColumnName, betaPair);
      WriteOutputPage(&outputPageA, 0, A, ratio, NSVUsed, deletedVector, conditionNumber, inputfile, SValue, numericalColumns, SValueUsed, numericalColumns, numericalColumnName, betaPair);
      m_free(A);
    } else {
      if (icaSFile)
        WriteOutputPage(&outputPageS, 1, s, ratio, NSVUsed, deletedVector, conditionNumber, inputfile, SValue, numericalColumns, SValueUsed, numericalColumns, numericalColumnName, betaPair);
      WriteOutputPage(&outputPageA, 0, A, ratio, NSVUsed, deletedVector, conditionNumber, inputfile, SValue, numericalColumns, SValueUsed, numericalColumns, numericalColumnName, betaPair);
      m_free(s);
      s = NULL;
      m_free(A);
      A = NULL;
      m_free(W);
      W = NULL;
      m_free(Wt);
      Wt = NULL;
      for (i = 0; i < ntau; i++)
        matrix_free(covR[i]);
    }
    matrix_free(V);
    V = NULL;
    matrix_free(newV);
    newV = NULL;
    matrix_free(newU);
    newU = NULL;

    if (verbose & FL_VERBOSE) {
      report_stats(stderr, "\nAfter output SDDS_StartTable.\n");
      if (mem_info_is_on())
        mem_info_file(stderr, 0);
    }
    if (mem_info_is_on())
      mem_info_file(stderr, 0);

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

  if (!SDDS_Terminate(&inputPage) || !SDDS_Terminate(&outputPageA))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile))
    exit(1);
  if (icaSFile && !SDDS_Terminate(&outputPageS))
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

  if (verbose & FL_VERBOSE) {
    report_stats(stderr, "\nAfter terminating SDDS pages.\n");
    if (mem_info_is_on())
      mem_info_file(stderr, 0);
  }
  free(covR);
  v_free(SValue);
  v_free(SValueUsed);
  v_free(InvSValue);
  if (deleteVector)
    free(deleteVector);
  if (icaSFile)
    free(icaSFile);
  if (numericalColumnName)
    SDDS_FreeStringArray(numericalColumnName, numericalColumns);
  free(numericalColumnName);
  if (outputColumns)
    SDDS_FreeStringArray(outputColumnName, outputColumns);
  free(outputColumnName);
#ifdef MKL
  mkl_free_buffers();
#endif
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
/*
 * extract diagonal elements and turn into a vector
 * written by X. Huang, 2/18/2021
 *  */
VEC *v_diag(MAT *A, VEC *diag) {
  long i;

  if (!A || !A->me)
    bomb("Data matrix not allocated.\n", NULL);
  if (!diag)
    bomb("Diagonal vector not allocated.\n", NULL);

  for (i = 0; i < MIN(A->n, A->m); i++)
    diag->ve[i] = A->me[i][i];
  return diag;
}

int32_t InitializeInputAndGetColumnNames(SDDS_DATASET *SDDS_dataset, char *filename, int32_t inputColumnNames0, char **inputColumnName0,
                                         char ***numericalColumnName, int32_t *numericalColumns,
                                         char **stringColumnName, char **inputDescription, char **inputContents) {
  char **columnName = NULL, **numColName = NULL;
  int32_t columns = 0, i, numCol, stringCol, columnType, found;

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
    columnType = SDDS_GetColumnType(SDDS_dataset, i);
    if (columnType == SDDS_STRING) {
      if (stringCol == -1) {
        stringCol = i;
        SDDS_CopyString(stringColumnName, columnName[i]);
      }
      continue;
    }
    if (inputColumnNames0) {
      /*only use given columns */
      if ((found = match_string(columnName[i], inputColumnName0, inputColumnNames0, WILDCARD_MATCH)) < 0)
        continue;
    }
    if (SDDS_NUMERIC_TYPE(columnType)) {
      numColName = SDDS_Realloc(numColName, sizeof(*numColName) * (numCol + 1));
      SDDS_CopyString(&numColName[numCol], columnName[i]);
      numCol++;
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

void SetupOutputFile(SDDS_TABLE *outputPage, SDDS_TABLE *inputPage, long ascii, u_int cols, long type,
                     char *outputfile, char *root, char *symbol, long digits, char *stringColName, short columnMajorOrder) {
  char outCol[80], format[80];
  int32_t i;
  /*type=0 for A matrix, type=1 for S matrix */

  if (!SDDS_InitializeOutput(outputPage, ascii ? SDDS_ASCII : SDDS_BINARY, 1, NULL, NULL, outputfile))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    outputPage->layout.data_mode.column_major = columnMajorOrder;
  else
    outputPage->layout.data_mode.column_major = inputPage->layout.data_mode.column_major;
  /* define singular value arrays */
  if (0 > SDDS_DefineArray(outputPage, "SingularValues", "SingularValues", NULL, "Singular Values",
                           NULL, SDDS_DOUBLE, 0, 1, NULL) ||
      0 > SDDS_DefineArray(outputPage, "SingularValuesUsed", "SingularValuesUsed", NULL,
                           "Singular Values Used",
                           NULL, SDDS_DOUBLE, 0, 1, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (0 > SDDS_DefineSimpleColumn(outputPage, "Index", NULL, SDDS_LONG))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (type == 0) {
    /*define string column for A matrix*/
    if (0 > SDDS_DefineSimpleColumn(outputPage, stringColName ? stringColName : "OldColumnNames",
                                    NULL, SDDS_STRING))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  /***********************************
   * define names for double columns *
   ***********************************/
  for (i = 0; i < cols; i++) {
    if (root)
      sprintf(format, "%s%%0%ldld", root, digits);
    else
      sprintf(format, "Mode%%0%ldld", digits);
    sprintf(outCol, format, i);

    if (0 > SDDS_DefineSimpleColumn(outputPage, outCol, symbol, SDDS_DOUBLE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (0 > SDDS_DefineParameter(outputPage, "MinimumSingularValueRatio", "MinimumSingularValueRatio", NULL,
                               "Minimum singular value ratio", NULL, SDDS_DOUBLE, NULL) ||
      0 > SDDS_DefineParameter(outputPage, "NumberOfSingularValuesUsed", "NumberOfSingularValuesUsed", NULL,
                               "largest singular value used", NULL, SDDS_LONG, NULL) ||
      0 > SDDS_DefineParameter(outputPage, "DeletedVectors", "DeletedVectors", NULL,
                               "list of vectors that were deleted", NULL, SDDS_STRING, NULL) ||
      0 > SDDS_DefineParameter(outputPage, "InputFile", "InputFile", NULL,
                               "InputFile", NULL, SDDS_STRING, 0) ||
      0 > SDDS_DefineParameter(outputPage, "ConditionNumber", "ConditionNumber", NULL,
                               "Condition Number", NULL, SDDS_DOUBLE, 0) ||
      !SDDS_DefineSimpleParameter(outputPage, "betaPair1A", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(outputPage, "betaPair1B", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(outputPage, "betaPair2A", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(outputPage, "betaPair2B", NULL, SDDS_LONG) ||
      !SDDS_WriteLayout(outputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void WriteOutputPage(SDDS_TABLE *outputPage, long type, MAT *mat, double ratio, long NSVUsed, char *deletedVector, double conditionNumber, char *inputfile, VEC *SValue, long sValues, VEC *SValueUsed, int32_t stringNames, char **stringName, long *betaPair) {
  int32_t i, outputRows, *index = NULL, startIndex;
  int32_t dim_ptr[1] = {1};

  outputRows = mat->m;
  index = malloc(sizeof(*index) * outputRows);
  for (i = 0; i < outputRows; i++)
    index[i] = i;
  if (!SDDS_StartTable(outputPage, outputRows) ||
      !SDDS_SetParameters(outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "MinimumSingularValueRatio", ratio,
                          "NumberOfSingularValuesUsed", NSVUsed,
                          "DeletedVectors", deletedVector,
                          "ConditionNumber", conditionNumber,
                          "betaPair1A", betaPair[0],
                          "betaPair1B", betaPair[1],
                          "betaPair2A", betaPair[2],
                          "betaPair2B", betaPair[3],
                          "InputFile", inputfile ? inputfile : "pipe", NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  dim_ptr[0] = sValues;
  if (!SDDS_SetArray(outputPage, "SingularValues", SDDS_POINTER_ARRAY, SValue->ve, dim_ptr))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  dim_ptr[0] = NSVUsed;
  if (!SDDS_SetArray(outputPage, "SingularValuesUsed", SDDS_POINTER_ARRAY, SValueUsed->ve, dim_ptr))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_SetColumn(outputPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, index, outputRows, 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  startIndex = 1;
  if (type == 0) {
    /*write string column*/
    if (stringNames != outputRows)
      fprintf(stderr, "Warning,  string namess != outputRows \n");
    if (!SDDS_SetColumn(outputPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, stringName, stringNames, 1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    startIndex = 2;
  }
  for (i = 0; i < mat->n; i++) {
    if (!SDDS_SetColumn(outputPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, mat->me[i], mat->m, i + startIndex))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_WriteTable(outputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(index);
  SDDS_FreeDataPage(outputPage);
}

/* calculate time lagged covariance matrix
 * written by X. Huang, 1/16/2021
 * Note: cov matrix needs to defined and allocated of memory
 * X, m row, n col;
 * cov, ndim*ndim, square, ndim<=n
 * tau, small integer, e.g., 0,1,2,3
 * */
MAT *m_covmat(MAT *X, int tau) {
  long i, j, k;
  double sum;
  MAT *cov;
#if 0 /*comment out Hairong's code for covariance matrix calculation */
  MAT *temp1, *temp2;
  
  /*covMat = X(1:end-tau, 1:NSVUsed)'*X(tau+1:end, 1:NSVUsed)   */
  temp1 = matrix_transpose(X);
  /*remove last tau columns of temp1 */
  temp1->n -= tau;
  fprintf(stderr, "tmp1  m %d n %d\n", temp1->m, temp1->n);
  fprintf(stdout, "temp1 matrix\n");
  matrix_output(stdout, temp1);
  
  
  temp2 = matrix_copy(X);
  
  
  /*remove first tau rows of temp2 */
  
  if (tau>0) {
    temp2 ->m -= tau;
    if (tau>0) {
      for (i=0; i<temp2->n; i++) {
        for (j=0; j<temp2->m; j++) {
          if (j+tau<X->m)
            temp2->me[i][j] = X->me[i][j+tau];
          else
            temp2->me[i][j] = X->me[i][j + tau - X->m];
        }
      }
    }
  } 
  
  fprintf(stdout, "temp2 matrix\n");
  matrix_output(stdout, temp2);
  cov = matrix_mult(temp1, temp2);

  fprintf(stderr,"cov m %d n %d\n", cov->m, cov->n);
  matrix_free(temp1);
  matrix_free(temp2);

  return cov;

#endif

  cov = matrix_get(X->n, X->n);
  /*fprintf(stderr,"cov m %d n %d\n", cov->m, cov->n); */

  /*if(!cov||!cov->me)
    bomb("Covariance matrix not allocated.\n",NULL); */
  if (!X || !X->me)
    bomb("Data matrix not allocated.\n", NULL);
  /* if(X->n<cov->n)
     bomb("Data matrix has fewer columns than covariance matrix.\n",NULL); */
  /*X is in column order, cov is in column order */

  /*
   * X->[col<n][row<m]:   X has m rows, n columns,
   */

  for (i = 0; i < cov->m; i++) {
    sum = 0;
    for (k = 0; k < X->m - tau; k++)
      sum += X->me[i][k] * X->me[i][k + tau];
    cov->me[i][i] = sum;

    for (j = 0; j < i; j++) {
      sum = 0;
      for (k = 0; k < X->m - tau; k++)
        sum += (X->me[i][k] * X->me[j][k + tau] + X->me[j][k] * X->me[i][k + tau]) / 2.0;
      cov->me[i][j] = cov->me[j][i] = sum;
    }
  }
  return cov;
}

/* joint diagonalization of a few square matrices
 * written by X. Huang, 1/16/2021
 * A, 1D array of MAT pointers
 * ntau, integer, number of MAT in A
 * V, Matrix that diagonalizes the matrices in A, allocated before calling
 * D, 1D array MAT pointers, matrices after diagonalization
 * matA = V*matD*V'
 * */
void m_jointdiag(MAT **A, MAT *V, int ntau, double el) {
  int i, j, m, p, q;
  MAT *g = NULL, *g2 = NULL;
  double s, c, theta, ton, toff;

  m = A[0]->m;

  g = matrix_get(2, ntau);
  g2 = matrix_get(2, 2);
  /*
    g->m=2;
    g->n=ntau;
    g=matrix_get(g->m,g->n);
    g2->m=g2->n=2;
    g2=matrix_get(g2->m,g2->n); */

  V = m_ident(V);

  s = 1;
  int cnt = 0;
  while (fabs(s) > el) {
    /*fprintf(stderr,"iteration %d, s=%f\n", cnt++, s);
     */
    if (cnt > 100)
      break;

    for (p = 0; p < m - 1; p++)
      for (q = p + 1; q < m; q++) {
        for (i = 0; i < ntau; i++) {
          g->me[i][0] = A[i]->me[p][p] - A[i]->me[q][q];
          g->me[i][1] = A[i]->me[q][p] + A[i]->me[p][q];
        }

        // mmtr_mlt(g,g,g2);
        mmtr_mult(g, g, g2);

        ton = m_entry(g2, 0, 0) - m_entry(g2, 1, 1);
        toff = m_entry(g2, 0, 1) + m_entry(g2, 1, 0);
        theta = 0.5 * atan2(toff, ton + sqrt(ton * ton + toff * toff));
        c = cos(theta);
        s = sin(theta);

        if (fabs(s) < el)
          break;
        else {
          for (i = 0; i < ntau; i++)
            for (j = 0; j < m; j++) {
              double Mp, Mq;
              Mp = m_entry(A[i], p, j);
              Mq = m_entry(A[i], q, j);
              A[i]->me[p][j] = c * Mp + s * Mq;
              A[i]->me[q][j] = c * Mq - s * Mp;
            }
          for (i = 0; i < ntau; i++)
            for (j = 0; j < m; j++) {
              double rp, rq;
              rp = m_entry(A[i], j, p);
              rq = m_entry(A[i], j, q);
              A[i]->me[j][p] = c * rp + s * rq;
              A[i]->me[j][q] = c * rq - s * rp;
            }
          for (j = 0; j < m; j++) {
            double temp;
            temp = m_entry(V, p, j);
            V->me[p][j] = c * m_entry(V, p, j) + s * m_entry(V, q, j);
            V->me[q][j] = c * m_entry(V, q, j) - s * temp;
          }
        }
      }

  } /* while */
  matrix_free(g);
  matrix_free(g2);
}

/*
 * out = A*B'.
 * Somehow mmtr_mlt in 'matrix.h' does not do this as advocated
 * Note: A->me[k][i] is $A_{ik}$
 * X. Huang, 1/21/2021
 */
void mmtr_mult(MAT *A, MAT *B, MAT *out) {
  if (!out || !out->me)
    bomb("output matrix not allocated\n", NULL);

  if (A->n != B->n)
    bomb("A and B sizes do not match\n", NULL);
  if ((out->m != A->m) || (out->n != B->m))
    bomb("output matrix does not match\n", NULL);

  int i, j, k;
  double sum;
  for (i = 0; i < out->m; i++)
    for (j = 0; j < out->n; j++) {
      sum = 0;
      for (k = 0; k < A->n; k++)
        sum += A->me[k][i] * B->me[k][j];
      out->me[j][i] = sum;
    }
}

/*
 * out = A*diag(Lambda)*B'.
 *
 * Note: A->me[k][i] is $A_{ik}$
 * X. Huang, 1/21/2021
 */
void mvmtr_mult(MAT *A, VEC *Lamb, MAT *B, MAT *out) {
  if (!out || !out->me)
    bomb("output matrix not allocated\n", NULL);
  if (!Lamb)
    bomb("vector not allocated\n", NULL);
  if (A->n != B->n)
    bomb("A and B sizes do not match\n", NULL);
  if ((out->m != A->m) || (out->n != B->m))
    bomb("output matrix does not match\n", NULL);

  int i, j, k;
  double sum;
  for (i = 0; i < out->m; i++)
    for (j = 0; j < out->n; j++) {
      sum = 0;
      for (k = 0; k < A->n; k++)
        sum += A->me[k][i] * B->me[k][j] * Lamb->ve[j];
      out->me[j][i] = sum;
    }
}

/*
 * out = A*diag(Lambda).
 *
 * Note: A->me[k][i] is $A_{ik}$
 * X. Huang, 1/21/2021
 */
void mv_mult(MAT *A, VEC *Lamb, MAT *out) {
  if (!out || !out->me)
    bomb("output matrix not allocated\n", NULL);
  if (!Lamb)
    bomb("vector not allocated\n", NULL);
  if (A->n > Lamb->dim) {
    fprintf(stdout, "A: %d, %d; vec: %d\n", A->m, A->n, Lamb->dim);
    bomb("A-matrix and vec sizes do not match\n", NULL);
  }
  if ((out->m != A->m) || (out->n != A->n))
    bomb("output matrix does not match\n", NULL);

  int i, j;
  for (i = 0; i < out->m; i++)
    for (j = 0; j < out->n; j++) {
      out->me[j][i] = A->me[j][i] * Lamb->ve[j];
    }
}

/*
 * copy a matrix A to B but using only the first N rows.
 * B is allocated here
 * X. Huang, 2/12/2021
 */
MAT *mcopy_Nrow(MAT *A, long row) {
  MAT *B;

  if (row < 0)
    bomb("negative row for output matrix\n", NULL);

  if ((A->m < row))
    bomb("Not enough rows to copy\n", NULL);

  B = matrix_get(row, A->n);
  int i, j;
  for (i = 0; i < B->m; i++)
    for (j = 0; j < B->n; j++) {
      B->me[j][i] = A->me[j][i];
    }
  return B;
}
MAT *mcopy_Nrow2(MAT *A, long row1, long nrow) {
  MAT *B;

  if (row1 < 0)
    bomb("negative value for row1\n", NULL);

  if ((A->m < row1 + nrow))
    bomb("Not enough rows to copy\n", NULL);

  B = matrix_get(nrow, A->n);
  int i, j;
  for (i = 0; i < B->m; i++)
    for (j = 0; j < B->n; j++) {
      B->me[j][i] = A->me[j][i + row1];
    }
  return B;
}

/*
 * look for pairs in ICA modes, using correlation coefficients of diagnal elements
 * of the time-lagged covariance matrices, up to two pairs
 * X. Huang, 2/15/2021
 */
#define MIN_VAL_COV_LOW_FREQ SQR(cos(2 * 3.1415926 * 0.1))
#define CORR_BETATRON_PAIR (0.99)
long findICAModePairs(MAT **covR, long ntau, long *pair) {
  long i, j, k, m;
  double sum, s1, s2, corr;
  long *cand, ncand;
  long npair = 0;

  m = covR[0]->m;
  cand = (long *)malloc(sizeof(long) * m);

  /* for(i=0;i<m;i++)
     {
     fprintf(stderr,"mode %d:  ", i);
     for(k=0;k<ntau;k++)
     {
     fprintf(stderr,"\t%f", covR[k]->me[i][i]);

     }
     fprintf(stderr,"\n");
     }*/

  ncand = -1;
  for (i = 0; i < m; i++) {
    for (k = 0; k < ntau; k++) {

      if (fabs(covR[k]->me[i][i]) < MIN_VAL_COV_LOW_FREQ) {
        ncand++;
        cand[ncand] = i;
        // fprintf(stderr,"mode: %d, val=%f\n", i,covR[k]->me[i][i]);
        break;
      }
    }
  }
  /*	 for(i=0;i<ncand;i++)
         fprintf(stderr,"mode: %d\n", cand[i]);*/
  if (ncand <= 1) {
    free(cand);
    return npair;
  }

  for (i = 0; i < ncand; i++)
    for (j = 0; j < i; j++) {
      sum = 0;
      s1 = s2 = 0;
      for (k = 0; k < ntau; k++) {
        s1 = s1 + SQR(covR[k]->me[cand[i]][cand[i]]);
        s2 = s2 + SQR(covR[k]->me[cand[j]][cand[j]]);
        sum = sum + covR[k]->me[cand[i]][cand[i]] * covR[k]->me[cand[j]][cand[j]];
      }
      corr = sum / sqrt(s1) / sqrt(s2);
      /*fprintf(stderr,"[i,j]=[%d, %d],%f\n", cand[i],cand[j],corr); */
      if (corr > CORR_BETATRON_PAIR) {
        pair[npair * 2 + 0] = cand[i];
        pair[npair * 2 + 1] = cand[j];
        npair++;
        if (npair >= 2) {
          free(cand);
          return npair;
        }
      }
    }
  free(cand);
  return npair;
}
