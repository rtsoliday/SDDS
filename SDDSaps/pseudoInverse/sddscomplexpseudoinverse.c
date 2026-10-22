/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* Program sddspcomplexseudoinverse
 * simliar to sddspseudoinverse except that the input and output are complex matrices.
 * to simplify, only use MKL library for complex svd and complex matrix multiplication
 * Input file columns should have Real%s and Imag%s pair for each element of complex matrix.
 * output file columns are written as Real%s and Imag%s too.
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"

#ifndef SDDS_COMPLEX_WEIGHTS_TEST
/** @brief Validate an explicitly selected categorical naming column before reading pages. */
static int32_t checkComplexNameColumn(SDDS_DATASET *input, char *column) {
  int32_t type = SDDS_GetNamedColumnType(input, column);
  if (type == SDDS_STRING || type == SDDS_ENUM)
    return 1;
  SDDS_SetError("-newColumnNames column must exist and have string or enum type");
  return 0;
}

/** @brief Widen the minimum digit count so every index below count has the same width (legacy rule). */
static long widenComplexNameDigits(long digits, int64_t count) {
  return count > 0 ? MAX(digits, (long)(log10((double)count) + 1)) : digits;
}

/** @brief Allocate legacy root%0<digits>ld index names, sizing each buffer from its formatted length. */
static char **makeComplexIndexNames(const char *root, int64_t count, long digits) {
  char **names;
  int64_t i;
  int length;

  if (count <= 0 || (uint64_t)count > SIZE_MAX / sizeof(*names) || digits > INT_MAX || digits < -INT_MAX) {
    SDDS_SetError("Invalid count or digit width for complex matrix names");
    return NULL;
  }
  if (!(names = SDDS_Calloc(count, sizeof(*names))))
    return NULL;
  for (i = 0; i < count; i++) {
    if ((length = snprintf(NULL, 0, "%s%0*" PRId64, root, (int)digits, i)) < 0 ||
        !(names[i] = SDDS_Malloc((size_t)length + 1))) {
      SDDS_FreeStringArray(names, count);
      free(names);
      SDDS_SetError("Unable to allocate complex matrix names");
      return NULL;
    }
    snprintf(names[i], (size_t)length + 1, "%s%0*" PRId64, root, (int)digits, i);
  }
  return names;
}

/** @brief Return owned row names, giving an explicit column precedence over automatic discovery. */
static char **getComplexOutputNames(SDDS_DATASET *input, char *root, char *automaticColumn,
                                    char *requestedColumn, long digits) {
  char *column = requestedColumn ? requestedColumn : automaticColumn;
  int64_t rows = SDDS_CountRowsOfInterest(input);

  if (requestedColumn && !checkComplexNameColumn(input, requestedColumn))
    return NULL;
  if (!root && column) {
    if (!checkComplexNameColumn(input, column))
      return NULL;
    return SDDS_GetColumnInString(input, column);
  }
  return makeComplexIndexNames(root ? root : "Column", rows, widenComplexNameDigits(digits, rows));
}

/** @brief Define real/imaginary column pairs without limiting categorical label lengths. */
static int32_t defineComplexNamedColumns(SDDS_DATASET *output, char **names, int32_t count, char *symbol) {
  int32_t i;
  for (i = 0; i < count; i++) {
    size_t length = strlen(names[i]);
    char *name;
    int32_t realIndex, imagIndex;
    if (length > SIZE_MAX - 5 || !(name = SDDS_Malloc(length + 5)))
      return 0;
    snprintf(name, length + 5, "Real%s", names[i]);
    realIndex = SDDS_DefineColumn(output, name, symbol, NULL, NULL, NULL, SDDS_DOUBLE, 0);
    snprintf(name, length + 5, "Imag%s", names[i]);
    imagIndex = SDDS_DefineColumn(output, name, symbol, NULL, NULL, NULL, SDDS_DOUBLE, 0);
    free(name);
    if (realIndex < 0 || imagIndex < 0)
      return 0;
  }
  return 1;
}

/** @brief Write a named real/imaginary pair using the complete categorical label. */
static int32_t setComplexNamedColumn(SDDS_DATASET *output, char *label, int64_t rows,
                                    double *real, double *imag) {
  size_t length = strlen(label);
  char *name;
  int32_t realSet, imagSet;
  if (length > SIZE_MAX - 5 || !(name = SDDS_Malloc(length + 5)))
    return 0;
  snprintf(name, length + 5, "Real%s", label);
  realSet = SDDS_SetColumn(output, SDDS_SET_BY_NAME, real, rows, name);
  snprintf(name, length + 5, "Imag%s", label);
  imagSet = SDDS_SetColumn(output, SDDS_SET_BY_NAME, imag, rows, name);
  free(name);
  return realSet && imagSet;
}

/** @brief Retain original categorical definitions when preparing a reconstruction file. */
static int32_t initializeComplexReconstruction(SDDS_DATASET *output, SDDS_DATASET *input, char *filename) {
  if (!SDDS_InitializeCopy(output, input, filename, "w"))
    return 0;
  if (!SDDS_FindParameter(input, FIND_NUMERIC_TYPE, "NumberOfSingularValuesUsed", NULL) &&
      SDDS_DefineParameter(output, "NumberOfSingularValuesUsed", NULL, NULL,
                           "largest singular value used", NULL, SDDS_LONG, NULL) < 0)
    return 0;
  if (!SDDS_FindParameter(input, FIND_ANY_TYPE, "DeletedVectors", NULL) &&
      SDDS_DefineParameter(output, "DeletedVectors", "DeletedVectors", NULL,
                           "list of vectors that were deleted", NULL, SDDS_STRING, NULL) < 0)
    return 0;
  return SDDS_WriteLayout(output);
}

/** @brief Copy original codes and dictionaries before replacing reconstructed numeric values. */
static int32_t copyComplexReconstructionPage(SDDS_DATASET *output, SDDS_DATASET *input,
                                            int32_t used, char *deleted) {
  return SDDS_StartPage(output, SDDS_CountRowsOfInterest(input)) &&
    SDDS_CopyParameters(output, input) && SDDS_CopyArrays(output, input) && SDDS_CopyColumns(output, input) &&
    SDDS_SetParameters(output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                        "NumberOfSingularValuesUsed", used, "DeletedVectors", deleted, NULL);
}
#endif

#ifndef SDDS_COMPLEX_NAMES_TEST
/**
 * @brief Read owned STRING or ENUM labels and match weights to matrix names.
 * @return An allocated weight vector, using 1 for names absent from the file.
 */
static double *readComplexWeightFile(char *filename, char *nameColumn, char *valueColumn,
                                     char **targetNames, int32_t targetCount,
                                     long verbose, long noWarnings) {
  SDDS_DATASET weightsPage;
  char **names = NULL;
  double *values = NULL, *weights = NULL;
  int64_t rows;
  int32_t type, i;
  long match;

  if (!filename || !nameColumn || !valueColumn)
    SDDS_Bomb("Weights require a filename, name column and value column");
  if (verbose)
    fprintf(stderr, "Reading file %s...\n", filename);
  if (!SDDS_InitializeInput(&weightsPage, filename))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  type = SDDS_GetNamedColumnType(&weightsPage, nameColumn);
  if (type != SDDS_STRING && type != SDDS_ENUM) {
    SDDS_SetError("Weight name column must exist and have string or enum type");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (SDDS_ReadPage(&weightsPage) <= 0) {
    SDDS_SetError("Unable to read weights dataset");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if ((rows = SDDS_CountRowsOfInterest(&weightsPage)) <= 0 || rows > LONG_MAX)
    SDDS_Bomb("Weights dataset must have between 1 and LONG_MAX rows");
  if (!(names = SDDS_GetColumnInString(&weightsPage, nameColumn)) ||
      !(values = SDDS_GetColumnInDoubles(&weightsPage, valueColumn)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  /* Label copies remain valid after their dictionary and dataset are released. */
  if (!SDDS_Terminate(&weightsPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (targetCount < 0 || (uint64_t)targetCount > SIZE_MAX / sizeof(*weights) ||
      !(weights = SDDS_Malloc(sizeof(*weights) * targetCount)))
    SDDS_Bomb("Unable to allocate weights");
  for (i = 0; i < targetCount; i++) {
    match = match_string(targetNames[i], names, (long)rows, EXACT_MATCH);
    if (match < 0) {
      if (!noWarnings)
        fprintf(stderr, "Warning: Name %s doesn't exist in file %s.\n", targetNames[i], filename);
      weights[i] = 1;
    } else
      weights[i] = values[match];
  }
  SDDS_FreeStringArray(names, rows);
  free(names);
  free(values);
  return weights;
}

/** @brief Load the first-page BPM and corrector weight inputs without SVD dependencies. */
static void readComplexWeightInputs(long ipage, long verbose, long noWarnings,
                                     long includeWeights, char *weightsFile,
                                     char *weightsNamesColumn, char *weightsValuesColumn,
                                     long includeCorrWeights, char *corrWeightsFile,
                                     char *corrWeightsNamesColumn, char *corrWeightsValuesColumn,
                                     int32_t rows, char **rowNames,
                                     int32_t columns, char **columnNames,
                                     double **weights, double **corrWeights) {
  if (ipage != 1)
    return;
  if (includeWeights)
    *weights = readComplexWeightFile(weightsFile, weightsNamesColumn, weightsValuesColumn,
                                    rowNames, rows, verbose, noWarnings);
  if (includeCorrWeights)
    *corrWeights = readComplexWeightFile(corrWeightsFile, corrWeightsNamesColumn, corrWeightsValuesColumn,
                                        columnNames, columns, verbose, noWarnings);
}
#endif

/* The named regression driver compiles the production readers without MKL. */
#if !defined(SDDS_COMPLEX_WEIGHTS_TEST) && !defined(SDDS_COMPLEX_NAMES_TEST)
#include "mkl_types.h"
#include "mkl.h"
/*#include "mkl_lapack.h" */
/*extern void zgesdd_(char* jobz, int *m, int *n, MKL_COMPLEX16* a, int *lda, double *s,
                    MKL_COMPLEX16* u, int* ldu, MKL_COMPLEX16* vt, int* ldvt,
                    MKL_COMPLEX16* work, int* lwork, double* rwork, int* iwork, int* info); */
extern void zgesdd( const char* jobz, const MKL_INT* m, const MKL_INT* n, 
             MKL_Complex16* a, const MKL_INT* lda, double* s, MKL_Complex16* u,
             const MKL_INT* ldu, MKL_Complex16* vt, const MKL_INT* ldvt,
             MKL_Complex16* work, const MKL_INT* lwork, double* rwork,
             MKL_INT* iwork, MKL_INT* info ); 

extern void zgesvd( const char* jobu, const char* jobvt, const MKL_INT* m, 
             const MKL_INT* n, MKL_Complex16* a, const MKL_INT* lda, double* s,
             MKL_Complex16* u, const MKL_INT* ldu, MKL_Complex16* vt,
             const MKL_INT* ldvt, MKL_Complex16* work, const MKL_INT* lwork,
             double* rwork, MKL_INT* info );

extern void dzgemm_(char* transa, char* transb, const MKL_INT* m, const MKL_INT* n,
                    const MKL_INT* k, MKL_Complex16* alpha, MKL_Complex16* a, const MKL_INT* lda, 
                    MKL_Complex16* b, const MKL_INT* ldb, MKL_Complex16* beta, MKL_Complex16* c, const MKL_INT* ldc); 

int dgemm_(char* transa, char* transb, const MKL_INT* m, const MKL_INT* n, const MKL_INT* k, double* alpha, double* a, const MKL_INT* lda, double* b, const MKL_INT* ldb, double* beta, double* c, const MKL_INT* ldc);
void zscal_(const MKL_INT *n, const MKL_Complex16 *a, MKL_Complex16 *x, const MKL_INT *incx);
void zgemm_(const char *transa, const char *transb, const MKL_INT *m, const MKL_INT *n, const MKL_INT *k,
           const MKL_Complex16 *alpha, const MKL_Complex16 *a, const MKL_INT *lda,
           const MKL_Complex16 *b, const MKL_INT *ldb, const MKL_Complex16 *beta,
           MKL_Complex16 *c, const MKL_INT *ldc);

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
#define CLO_WEIGHT 12
#define CLO_NOWARNINGS 13
#define CLO_SMATRIX 14
#define CLO_RECONSTRUCT 15
#define CLO_DELETEVECTORS 16
#define CLO_REMOVE_SINGULAR_VALUES 17
#define CLO_ECONOMY 18
#define CLO_PRINTPACKAGE 19
#define CLO_MULTIPLY_MATRIX 20
#define CLO_MAJOR_ORDER 21
#define CLO_LAPACK_METHOD 22
#define CLO_CORRECTOR_WEIGHT 23
#define COMMANDLINE_OPTIONS 24
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
  };

static char *USAGE="sddspseudoinverse [<input>] [<output>] [-pipe=[input][,output]]\n\
    [{-minimumSingularValueRatio=<value> | -largestSingularValues=<number>}] \n\
    [-smallestSingularValues=<number>] \n\
    [-deleteVectors=<list of vectors separated by comma>] \n\
    [-economy] \n\
    [-oldColumnNames=<string>] [{-root=<string> [-digits=<integer>] | \n\
    -newColumnNames=<column>}] [-sFile=<file>[,matrix]] [-uMatrix=<file>] [-vMatrix=<file>] \n\
    [-weights=<file>,name=<columnname>,value=<columnname>] \n\
    [-correctorWeights=<file>,name=<columnname>,value=<columnname>] \n\
    [-majorOrder=row|column] [-lapackMethod={simple|divideAndConquer}] \n\
    [-reconstruct=<file>] [-symbol=<string>] [-ascii] [-verbose] [-noWarnings] [-multiplyMatrix=<file>[,invert]]\n\n";
static char *USAGE2="Takes the generalized inverse of a complex matrix in a SDDS file. \n\
                     All matrix files (input our output) should have Real* and Imag* pair of columns\n\
               input file should contain pairs of Real%s and Imag%s columns for defining columns of complex matrix.\n\
               where Real%s column defines the real element of the complex number, \n\
               and Imag%s column defines the imaginary element of the complex number.\n\
               therefore, the input file must have at least 2*n columns for a n column complex matrix.\n\
pipe           reads input from and/or write output to a pipe.\n\
minimumSingularValueRatio\n\
               rejects singular values less than the largest\n\
               singular value times this ratio.\n\
largestSingularValues\n\
               retains only the first \"largestSingularValues\"\n\
               largest singularvalues.\n\
smallestSingularValues\n\
               remove the the last \"smallestSingularValues\" smallest singularvalues. \n\
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
oldColumnNames\n\
               specifies a name for the output file string column created for\n\
               the input file column names.\n\
multiplyMatrix if invert is not provided,  then the output matrix is the inverse of the input\n\
               matrix multiplying by this matrix; otherwise, the output matrix is the product of \n\
               multiply matrix and the inverse of the input matrix.\n\
majorOrder     specity output file in row or column major order.\n";
static char *USAGE3="root           use the string specified to generate column names.\n\
               Default for column names is the first string column in\n\
               <inputfile>. If there is no string column, then the column\n\
               names are formed with the root \"Real\" and \"Imag\".\n\
digits         minimum number of digits used in the number appended to the root\n\
               part of the column names. Default is value 3.\n\
newColumnNames use the specified STRING or ENUM column as the source for new column names.\n\
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

static char *USAGE4="               The equation to solve is wAx = wy where w is the weight vector\n\
               turned into a diagonal matrix and A is the matrix. \n\
               The matrix solution returned is (wA)^I w where ^I means taking \n\
               the pseudoinverse. The u matrix now has a different interpretation:\n\
               the \"transformed\" x are v^T x, as before, but the \"transformed\" y are u^T w y.\n\
symbol         use the string specified for the symbol field for all columns definitions.\n\
reconstruct    speficy a file which will reconstruct the original matrix with only the\n\
               singular values retained in the inversion.\n\
ascii          writes the output file data in ascii mode (default is binary).\n\
verbose        prints out to stderr input and output matrices.\n\
noWarnings     prevents printing of warning messages.\n\
Program by Hairong Shang, ANL ("__DATE__")\n";

#define FL_VERBOSE 1
#define FL_VERYVERBOSE 2


long SDDS_FreeDataPage(SDDS_DATASET *SDDS_dataset);

int32_t InitializeInputAndGetColumnNames(SDDS_DATASET *SDDS_dataset, char *filename, 
                                         char ***numericalColumnName, int32_t *numericalColumns,
                                         char **stringColumName, char **inputDescription, char **inputContents);


void ReadOtherInputData(long verbose, long nowarnings, long ipage, SDDS_DATASET *inputPage, char *Root, char *stringColumnName, char *newColumnNamesColumn,
                        long digits, SDDS_DATASET *multiplyPage, long invertMultiply, char **multiplyColumnName, int32_t multiplyColumns, char *multiplyFile,
                        int32_t numericalColumns, char **numericalColumnName, char *multiStringCol, long includeWeights, char *weightsFile, char *weightsNamesColumn,
                        long includeCorrWeights, char *corrWeightsFile, char *corrWeightsNamesColumn, char *weightsValuesColumn, char *corrWeightsValuesColumn,
                        int32_t *rows, int32_t *rowsFirstPage, char ***outputColumnName, int32_t *outputColumns, 
                        int32_t *multiplyRows, int32_t *freeMultiCol, char ***actuatorName, int32_t *actuators, MKL_Complex16 **Multi,
                        double **w, double **corrW);

void SetupOutputFiles(char *inputDescription, char *inputContents, char *outputfile,
                      SDDS_DATASET *outputPage, SDDS_DATASET *inputPage, long ascii, short columnMajorOrder, long verbose,
                      char *sFile, char *vFile, char *uFile, SDDS_DATASET *sPage, SDDS_DATASET *vPage, SDDS_DATASET *uPage, char *multiplyFile, long invertMultiply,
                      char *oldColumnNames, char *Symbol, char **outputColumnName, int32_t rows, int32_t numericalColumns, int32_t outputColumns, int32_t productCols,
                      char *newColumnNamesColumn, char *multiStringCol, long *strColIndex, long sFileAsMatrix, char ***orthoColumnName, 
                      long digits,  long ucols, char *reconstructFile, SDDS_DATASET *reconstructPage);

/* Auxiliary routines prototypes */
void print_rmatrix( char* desc, MKL_INT m, MKL_INT n, double* a, MKL_INT lda );
int cprintmatrix( char *matname, int m, int n, MKL_Complex16 *A);

int main(int argc, char **argv)
{
  SCANNED_ARG *s_arg;
  SDDS_TABLE inputPage, outputPage, uPage, vPage, sPage, reconstructPage, multiplyPage;
  
  char *inputfile, *outputfile, *uFile, *vFile, *sFile, *weightsFile, *reconstructFile, *multiplyFile, *corrWeightsFile;
  char **outputColumnName=NULL, **orthoColumnName=NULL, **actuatorName=NULL, **numericalColumnName=NULL, **multiplyColumnName=NULL;
  char *weightsNamesColumn, *weightsValuesColumn, *stringColumnName=NULL, *multiStringCol=NULL;
  char **weightsName=NULL;
  char  **corrWeightsName=NULL;
  char *corrWeightsNamesColumn, *corrWeightsValuesColumn;
  double largestS;
  int32_t rows, multiplyRows=0, actuators=0, outputColumns=0, urows, ucols, productRows, productCols, outputRows;
  int32_t rowsFirstPage=0, numericalColumns, weightsRows, multiplyColumns, freeMultiCol=0;
  int32_t corrWeightsRows;
  long sFileAsMatrix; 
  char *inputDescription=NULL, *inputContents=NULL;
  char *outputDescription=NULL;
  long i_arg;
  double *real=NULL, *imag=NULL;
  register long i, j, k;
  
  int32_t dim_ptr[1]={1};
  long includeWeights, verbose, includeCorrWeights;
  char realcol[256], imagcol[256];
  char *Symbol, *Root;
  double  *w=NULL, *SValue, *SValueUsed, *InvSValue, *corrW=NULL, **realData=NULL, **imagData=NULL;
  MKL_Complex16 *R, *RInv, *RInvR, *Rnewt, *U, *Ut, *V, *Vt, *Multi=NULL, *Product=NULL;
  
  double ratio;
  long nlargest,nsmallest;
  long NSVUsed;
  long ascii;
  long digits, invertMultiply=0, strColIndex=-1;
  unsigned long pipeFlags;
  long tmpfile_used, noWarnings;
  long ipage;
  char *oldColumnNames, *newColumnNamesColumn;
  double conditionNumber, max, min;
  unsigned long flags, majorOrderFlag;
  long *deleteVector,deleteVectors,firstdelete;
  char deletedVector[1024];
  long printPackage;
  short columnMajorOrder=-1, lapackMethod = 1;
  
  
  /* Flag economy changes the calculation mode of the lapack-type calls to svd
     and may later be used by a modified meschach svd routine to
     reduce the number of columns returned for the U matrix.
     The ecomony mode is already the only mode for NR and for sunperf. */
  long economy, economyRows;
  MKL_INT info;
  /* default is standard svd calculation with square U matrix */
  char calcMode='A'; /* 'A': all M columns of U and all N rows of V**H are returned in the arrays U and VT;
                        'S': the first min(M,N) columns of U and the first min(M,N) rows of V**H are returned in the arrays U and VT;
                        'O': if M>=N, the first N columns of U are overwritten in the array A and all rows of V**H are turned in the array VT;
                        'N': no columns of U or rows of V**H are computed. */ 
  MKL_INT lda, ldu, ldvt, kk, ldb, lwork, lrwork, m, n, vrows;
  MKL_Complex16 wkopt, *work, alpha, beta;
  MKL_INT *iwork=NULL;
  double  *rwork=NULL;
  
  alpha.real=1.0; alpha.imag=beta.imag=beta.real=0;
  
  SValue = SValueUsed = InvSValue = NULL;
  R = RInv = RInvR = Rnewt = U = Ut = V = Vt =  NULL;
  
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg,  argc,  argv);
  if (argc==1) {
    fprintf(stderr, "%s%s%s%s", USAGE, USAGE2, USAGE3, USAGE4);
    exit(1);
  }
  deletedVector[0]='\0';
  firstdelete=1;
  inputfile = outputfile = multiplyFile = NULL;
  verbose=0;
  Symbol=Root=NULL;
  uFile=vFile=sFile=reconstructFile=NULL;
  weightsFile= corrWeightsFile = NULL;
  weightsNamesColumn=weightsValuesColumn= corrWeightsNamesColumn= corrWeightsValuesColumn =NULL;
  ratio=0;
  nlargest=0;
  nsmallest=0;
  deleteVectors=0;
  deleteVector=NULL;
  ascii=0;
  digits=3;
  pipeFlags = 0;
  tmpfile_used=0;
  noWarnings=0;
  oldColumnNames = NULL;
  newColumnNamesColumn = NULL;
  conditionNumber = 0.0;
  includeWeights = includeCorrWeights = 0;
  sFileAsMatrix = 0;
  economy = 1; /*make economy as default */
  printPackage = 0;
  
  for (i_arg=1; i_arg<argc; i_arg++) {
    if (s_arg[i_arg].arg_type==OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0],  commandline_option,  COMMANDLINE_OPTIONS,  UNIQUE_MATCH)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag=0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items>0 &&
	    (!scanItemList(&majorOrderFlag, s_arg[i_arg].list+1, &s_arg[i_arg].n_items, 0,
			   "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
			   "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER,
			   NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag&SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder=1;
        else if (majorOrderFlag&SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder=0;
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
      case CLO_ASCII:
        ascii=1;
        break;
      case CLO_NOWARNINGS:
        noWarnings=1;
        break;
      case CLO_DIGITS:
        if (!(get_long(&digits, s_arg[i_arg].list[1])))
          bomb("no string given for option -digits", USAGE);
        break;
      case CLO_COLUMNROOT:
        if (!(Root=s_arg[i_arg].list[1]))
          bomb("No root string given", USAGE);
        break;
      case CLO_SYMBOL:
        if (!(Symbol=s_arg[i_arg].list[1]))
          bomb("No symbol string given", USAGE);
        break;
      case CLO_SMATRIX:
        if (s_arg[i_arg].n_items<2 ||
            !(sFile=s_arg[i_arg].list[1]))
          bomb("No sMatrix string given", USAGE);
        if (s_arg[i_arg].n_items>2) {
          if (s_arg[i_arg].n_items==3 
              && strncmp(s_arg[i_arg].list[2], "matrix", strlen(s_arg[i_arg].list[2]))==0) {
            sFileAsMatrix = 1;
          } else
            bomb("Invalid sMatrix syntax", USAGE);
        }
        break;
      case CLO_ECONOMY:
        economy=1;
        break;
      case CLO_UMATRIX:
        if (s_arg[i_arg].n_items<2 || !(uFile=s_arg[i_arg].list[1]))
          bomb("No uMatrix string given", USAGE);
        break;
      case CLO_VMATRIX:
        if (!(vFile=s_arg[i_arg].list[1]))
          bomb("No vMatrix string given", USAGE);
        break;
      case CLO_RECONSTRUCT:
        if (!(reconstructFile=s_arg[i_arg].list[1]))
          bomb("No reconstruct string given", USAGE);
        break;
      case CLO_WEIGHT:
        if (s_arg[i_arg].n_items<3)
          SDDS_Bomb("invalid -weight syntax");
        weightsFile = s_arg[i_arg].list[1];
        includeWeights = 1;
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&flags, s_arg[i_arg].list+2, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &weightsNamesColumn, 1, 0,
                          "value", SDDS_STRING, &weightsValuesColumn, 1, 0,
                          NULL))
          SDDS_Bomb("invalid -weights syntax");
        break;
      case CLO_CORRECTOR_WEIGHT:
        if (s_arg[i_arg].n_items<3)
          SDDS_Bomb("invalid -correctorWeight syntax");
        corrWeightsFile = s_arg[i_arg].list[1];
        includeCorrWeights = 1;
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&flags, s_arg[i_arg].list+2, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &corrWeightsNamesColumn, 1, 0,
                          "value", SDDS_STRING, &corrWeightsValuesColumn, 1, 0,
                          NULL))
          SDDS_Bomb("invalid -correctorWeights syntax");
        break;
      case CLO_VERBOSE:
        if (s_arg[i_arg].n_items==1)
          verbose |= FL_VERBOSE;
        else if (s_arg[i_arg].n_items==2 && 
		 strncmp(s_arg[i_arg].list[1], "very", strlen(s_arg[i_arg].list[1]))==0)
          verbose |= FL_VERYVERBOSE; 
        else
          SDDS_Bomb("invalid -verbose syntax");
        break;
      case CLO_PRINTPACKAGE:
        printPackage=1;
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list+1, s_arg[i_arg].n_items-1, &pipeFlags))
          bomb("invalid -pipe syntax", NULL);
        break;
      case CLO_OLDCOLUMNNAMES:
        if (!(oldColumnNames=s_arg[i_arg].list[1]))
          bomb("No oldColumnNames string given", USAGE);
        break;
      case CLO_MULTIPLY_MATRIX:
        if (!(multiplyFile=s_arg[i_arg].list[1]))
          bomb("No multiply matrix string given", USAGE);
        if (s_arg[i_arg].n_items>2) {
          if (strncmp_case_insensitive(s_arg[i_arg].list[2], "invert", MIN(5, strlen(s_arg[i_arg].list[2])))==0)
            invertMultiply = 1;
          else
            SDDS_Bomb("Invalid -multiplyMatrix syntax provided.");
        }
        break;
      case CLO_NEWCOLUMNNAMES:
        if (s_arg[i_arg].n_items!=2 ||
            SDDS_StringIsBlank(newColumnNamesColumn = s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -newColumnNames syntax/value");
        break;
      case CLO_DELETEVECTORS:
        deleteVectors=s_arg[i_arg].n_items-1;
        deleteVector=(long*)malloc(sizeof(*deleteVector)*deleteVectors);
        for (j=0;j<deleteVectors;j++) {
          if (!(get_long(&deleteVector[j], s_arg[i_arg].list[j+1])))
            bomb("non numeric value given in -deleteVectors option", USAGE);
        }
        break;
      case CLO_LAPACK_METHOD:
        if (s_arg[i_arg].n_items!=2)
          SDDS_Bomb("Invalid -lapackMethod syntax, either \"simple\" or \"divideAndConquer\" should be given.");
        if (strncmp_case_insensitive(s_arg[i_arg].list[1], "simple", MIN(strlen(s_arg[i_arg].list[1]), 6))==0) {
          lapackMethod = 0;
        } else if (strncmp_case_insensitive(s_arg[i_arg].list[1], "divideAndConqure", MIN(strlen(s_arg[i_arg].list[1]), 6))==0) {
          lapackMethod = 1;
        } else 
          SDDS_Bomb("Invalid lapackMethod given, has to be \"simple\" or \"divideAndConquer\".");
        break;
      default: 
        bomb("unrecognized option given",  USAGE);
        break;
      }
    }
    else {
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        bomb("too many filenames given",  USAGE);
    }
  }
  
  processFilenames("sddscomplexpseudoinverse", &inputfile, &outputfile, pipeFlags, noWarnings, &tmpfile_used);
  
  if ((nlargest&&ratio) || (nlargest&&nsmallest) || (nsmallest&&ratio))
    SDDS_Bomb("Can only specify one of minimumSingularValueRatio, largestSingularValues and smallestSingularValues options.\n");
  if (newColumnNamesColumn && Root) 
    SDDS_Bomb("-root and -newColumnNames are incompatible");
  if (verbose&FL_VERBOSE) {
    report_stats(stderr,"\nBefore initializing SDDS input.\n");
  }
  InitializeInputAndGetColumnNames(&inputPage, inputfile, &numericalColumnName, &numericalColumns,
                                   &stringColumnName, &inputDescription, &inputContents);
  if (newColumnNamesColumn && !checkComplexNameColumn(&inputPage, newColumnNamesColumn))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (verbose&FL_VERBOSE) {
    report_stats(stderr,"\nAfter initializing SDDS input.\n");
  }
  if (multiplyFile)
    InitializeInputAndGetColumnNames(&multiplyPage, multiplyFile,  &multiplyColumnName, &multiplyColumns,
                                     &multiStringCol, NULL, NULL);
  while (0<(ipage=SDDS_ReadTable(&inputPage))) {
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (verbose&FL_VERBOSE) {
      report_stats(stderr,"\nAfter reading page.\n");
    }
    ReadOtherInputData(verbose, noWarnings, ipage, &inputPage, Root, stringColumnName, newColumnNamesColumn,
                       digits, &multiplyPage, invertMultiply, multiplyColumnName,  multiplyColumns, multiplyFile,
                       numericalColumns, numericalColumnName, multiStringCol, includeWeights, weightsFile, weightsNamesColumn,
                       includeCorrWeights, corrWeightsFile, corrWeightsNamesColumn, weightsValuesColumn, corrWeightsValuesColumn,
                       &rows, &rowsFirstPage, &outputColumnName, &outputColumns, 
                       &multiplyRows, &freeMultiCol, &actuatorName, &actuators, &Multi,
                       &w, &corrW);
    
    if (!R) 
      R = malloc(sizeof(*R) * rows * numericalColumns);
    if (verbose&FL_VERBOSE) 
      report_stats(stderr,"\nAfter partial R allocation (if first loop).\n");
    /* it is assumed that the non-numerical columns are filtered out */
    /* On subsequent loops, the memory pointed to by R->me is supposed to be freed */
    /*read R complex matrix from input */
    for (i=0; i<numericalColumns; i++) {
      sprintf(realcol, "Real%s", numericalColumnName[i]);
      sprintf(imagcol, "Imag%s", numericalColumnName[i]);
      if (!(real = SDDS_GetColumnInDoubles(&inputPage, realcol)) ||
          !(imag = SDDS_GetColumnInDoubles(&inputPage, imagcol)))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors); 
      for (j=0; j<rows; j++) {
        R[i*rows+j].real = real[j];
        R[i*rows+j].imag = imag[j];
      }
      free(real); real=NULL;
      free(imag); imag=NULL;
    }
    /* free data page memory as soon as possible */
    if (!reconstructFile)
      SDDS_FreeDataPage(&inputPage);
    if (verbose&FL_VERBOSE) 
      report_stats(stderr,"\nAfter filling R matrix with data.\n");
    if (includeWeights) {
      for (j=0;j<numericalColumns;j++) {
        for (i=0;i<rows;i++) {
          R[j * rows +i].real *= w[i];
          R[j * rows +i].imag *= w[i];
        }
      }
    }
    if (includeCorrWeights) {
      for (j=0;j<numericalColumns;j++) {
        for (i=0;i<rows;i++) {
          R[j * rows +i].real *= corrW[j];
          R[j * rows +i].imag *= corrW[j];
        }
      }
    }
    
    m = (MKL_INT)rows;
    n = (MKL_INT)numericalColumns;
    /* On first passs, allocate the memory. On subsequent passes,
       the memory may have been free, and the pointer made NULL */
    if (!SValue) {
      if (!(SValue = malloc(sizeof(*SValue)*n)))
        SDDS_Bomb("Error allocating memory for SValue.");
    }
    if (!SValueUsed)
      if (!(SValueUsed =  malloc(sizeof(*SValue)*n)))
        SDDS_Bomb("Error allocating memory for SValueUsed.");
    if (!InvSValue)
      if (!(InvSValue =  malloc(sizeof(*SValue)*n)))
        SDDS_Bomb("Error allocating memory for InvSValue");
    if (!U) {
      urows = rows;
      ucols = MIN(numericalColumns, rows);
      if (!(U = malloc(sizeof(*U) * urows * ucols)))
        SDDS_Bomb("Error allocating memory for U.");
    }
    economyRows = MIN(numericalColumns, rows);
    vrows = MIN(numericalColumns, rows);
    if (!Vt) {
      if (!(Vt = malloc(sizeof(*Vt)* vrows * numericalColumns)))
        SDDS_Bomb("Error allocating memory for Vt.");
    }
    calcMode = 'S';
    if (!(iwork = malloc(sizeof(*iwork)*8*rows)))
      SDDS_Bomb("Error allocate memory for iwork."); 
    lrwork = 5*rows*rows+7*rows;
    if (!(rwork = malloc(sizeof(*rwork)*lrwork)))
      SDDS_Bomb("Error allocate memory for rwork"); 
    
    lda = m;
    ldu = m;
    ldvt = vrows;
    lwork = -1;
    zgesdd(&calcMode, &m, &n, R, &lda, SValue, 
           U, &ldu, Vt, &ldvt, &wkopt, &lwork, rwork, iwork, &info);
    lwork = (MKL_INT)wkopt.real;
    work = malloc(sizeof(*work)*lwork); 
    /*compute complex SVD */
    zgesdd(&calcMode, &m, &n, R, &lda, SValue, 
           U, &ldu, Vt, &ldvt, work, &lwork, rwork, iwork, &info);
    if (info>0) {
      fprintf(stderr, "The alogirthim computing complex SVD failed to converge.\n");
      return(1);
    }
    free(work); work=NULL;
    free(R); R=NULL;
    free(iwork); iwork=NULL;
    free(rwork); rwork=NULL;
    
    max = 0;
    min = DBL_MAX;
    /*find the largest S value, and use it as reference for ratio instead of the first Svalue because the first Svalue could be removed 
      (set to zero) with -removeDCVectors option*/
    largestS = 0;
    for (i=0; i<numericalColumns; i++) {
      if (SValue[i]) {
	largestS = SValue[i];
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
    for(i=0;i<numericalColumns;i++) {
      if (!SValue[i]) {
        InvSValue[i] = 0;
      }
      else if ((SValue[i]/largestS)<ratio ) {
        InvSValue[i] = 0;
        SValueUsed[i] = 0;
      }
      else if (nlargest && i>=nlargest) {
        InvSValue[i] = 0;
        SValueUsed[i] = 0;
      } else if (nsmallest && i>=(numericalColumns-nsmallest)) {
        InvSValue[i] = 0;
        SValueUsed[i] = 0;
      }
      else {
        InvSValue[i] = 1/SValue[i];
        SValueUsed[i] = SValue[i];
        max = MAX(SValueUsed[i],max);
        min = MIN(SValueUsed[i],min);
        NSVUsed++;
      }
    }
    /*4) remove SV of user-selected vectors -
      delete vector given in the -deleteVectors option
      by setting the inverse singular values to 0*/
    for (i=0;i<deleteVectors;i++) {
      if (0<=deleteVector[i] && deleteVector[i]<numericalColumns) {
        size_t used = strlen(deletedVector);
        /* Append in place; the old "%s %ld" form read from its own destination. */
        snprintf(deletedVector + used, sizeof(deletedVector) - used, firstdelete ? "%ld" : " %ld", deleteVector[i]);
        firstdelete=0;
        InvSValue[deleteVector[i]] = 0;
        SValueUsed[deleteVector[i]] = 0;
        if (nlargest && deleteVector[i]>=nlargest)
          break;
        NSVUsed--;
      }
    }
    
    conditionNumber = max/min;
    if (verbose&FL_VERYVERBOSE) {
      fprintf(stderr, "Inverse singular value ");
      for (i=0; i<numericalColumns; i++)
        fprintf(stderr, "%9.6le ", InvSValue[i]);
      fprintf(stderr, "\n");
    }
    if (!RInv) 
      RInv = malloc(sizeof(*RInv)* rows *numericalColumns);
    if (!V)
      V = malloc(sizeof(*V)*vrows * numericalColumns);
    /*note that Vt is in row order, not column order */
    for (i=0; i<numericalColumns; i++) {
      for (k=0; k<vrows; k++) {
        V[i * vrows + k].real = Vt[i * vrows + k].real * InvSValue[k];
        V[i * vrows + k].imag = Vt[i * vrows + k].imag * InvSValue[k];
      }
    }
    
    /* Rinv = Vt+ Sinv U+ = (VtS)+ U+  this is only applicable for economy option, RInv is nxm matrix 
       otherwise, dgemm does not work since U has R->m columns, while V (Vt) has R->n rows */
    /* note that, the arguments of dgemm should be
       U->m -- should be the row number of product matrix, which is U x V
       V->n -- the column number of the product matrix
       kk   -- should be the column number of U and row number of V, whichever is smaller (otherwise, memory access error)
       lda  -- should be the row number of U matrix
       ldb  -- should be the row number of V matrix */
    
    if  (verbose&FL_VERBOSE) {
      cprintmatrix("U", urows, ucols, U);
      cprintmatrix("Vt", vrows, n, Vt);
      cprintmatrix("V", vrows, n, V);
    }
    
    kk = MIN(m, n);
    ldvt = kk;
    ldu = m;
    zgemm_("C", "C", &n, &m, &kk, &alpha, V, &ldvt, U, &m, &beta, RInv, &n);
    if  (verbose&FL_VERBOSE)
      cprintmatrix("Rinv", n, m, RInv);
    
    free(V); 
    V = NULL;
    /*rows is the column number of RInv, numerical columns is the column number of RInv */
    if (includeWeights)
      for (i=0; i<rows; i++)
        for (j=0; j<numericalColumns; j++) {
          RInv[i*numericalColumns + j].real *=w[i]; 
          RInv[i*numericalColumns + j].imag *=w[i];
        }
    if (includeCorrWeights)
      for (i=0; i<rows; i++)
        for (j=0; j<numericalColumns; j++) {
          RInv[i*numericalColumns + j].real *=corrW[j];
          RInv[i*numericalColumns + j].imag *=corrW[j];
        }
    realData = imagData = NULL;
    if (!multiplyFile) {
      realData = malloc(sizeof(*realData)*rows);
      imagData = malloc(sizeof(*imagData)*rows);
      for (i=0; i<rows; i++) {
        realData[i] = malloc(sizeof(**realData)*numericalColumns);
        imagData[i] = malloc(sizeof(**imagData)*numericalColumns);
      }
      /*RInvt is the transpose of RInv, Real and Imag belong to RInv */
      for (i=0; i<rows; i++) {
        for (j=0; j<numericalColumns; j++) {
          realData[i][j] = RInv[i*numericalColumns+j].real;
          imagData[i][j] = RInv[i*numericalColumns+j].imag;
        }
      }
      outputRows = numericalColumns;
      outputColumns = rows;
    } else {
      /* the inverse matrix should be numericalColumns X rows, RInvt is the transpose of inverse, therefore 
         it is rows X numericalColumns */  
      if (!Product) {
        if (!invertMultiply) { /*Product = RInv * Multi*/
          if (rows!=multiplyRows) /*rows is the column number of RInv, multiplyRows is the row of Multi */
            SDDS_Bomb("Unable to multiply inverse by multiply matrix, their column and row number do not match.");
          lda = numericalColumns; /*row number of RInv */
          Product = malloc(sizeof(*Product) * numericalColumns * multiplyColumns);
          kk = multiplyRows;
          ldb = kk; /*row number of multi matrix */
          productRows = numericalColumns;
          productCols = multiplyColumns;
        } else { /*Product = Multi * RInv */
          if ( multiplyColumns!=numericalColumns) /*numericalColumns is the row number of RInv  */
            SDDS_Bomb("Unable to multiply \"multiply matrix\" by inverse, their column and row number do not match.");
          Product = malloc(sizeof(*Product)*  multiplyRows * rows);
          kk = multiplyColumns; 
          lda = multiplyRows; /*row number of multi matrix */
          ldb = multiplyColumns; /* row number of RInv */
          productRows = multiplyRows;
          productCols = rows;
        } /*end of invertMultiply*/
      }
      if (!invertMultiply)
        zgemm_("N", "N", &productRows, &productCols, &kk, &alpha, RInv, &lda, Multi, &ldb, &beta, Product, &productRows);
      else 
        zgemm_("N", "N", &productRows, &productCols, &kk, &alpha, Multi, &lda, RInv, &ldb, &beta, Product, &productRows);
      /* One output column per product column, each holding productRows values. */
      realData = malloc(sizeof(*realData)*productCols);
      imagData = malloc(sizeof(*imagData)*productCols);
      for (i=0; i<productCols; i++) {
        realData[i] = malloc(sizeof(**realData)*productRows);
        imagData[i] = malloc(sizeof(**imagData)*productRows);
      }
      for (i=0; i<productCols; i++) {
        for (j=0; j<productRows; j++) {
          realData[i][j] = Product[i*productRows + j].real;
          imagData[i][j] = Product[i*productRows + j].imag;
        }
      }
      outputRows = productRows;
      outputColumns = productCols;
      free(Product); Product = NULL;
      /* Multi is kept so a shorter multiply file reuses its last page. */
    }
    free(RInv); RInv=NULL;
    
    /*  define output page */
    if (ipage==1) {
      SetupOutputFiles(inputDescription, inputContents, outputfile,
                       &outputPage, &inputPage, ascii, columnMajorOrder, verbose,
                       sFile, vFile, uFile, &sPage, &vPage, &uPage, multiplyFile, invertMultiply,
                       oldColumnNames, Symbol, outputColumnName, rows, numericalColumns, outputColumns, productCols,
                       newColumnNamesColumn, multiStringCol, &strColIndex, sFileAsMatrix, &orthoColumnName, 
                       digits, ucols, reconstructFile, &reconstructPage);
    }
    if (!SDDS_StartTable(&outputPage, outputRows) ||
        !SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE,
                            "MinimumSingularValueRatio", ratio, 
                            "NumberOfSingularValuesUsed",NSVUsed,
                            "DeletedVectors",deletedVector,
                            "ConditionNumber", conditionNumber,
                            "InputFile", inputfile?inputfile:"pipe", NULL))
      SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (verbose&FL_VERBOSE) {
      report_stats(stderr,"\nAfter output SDDS_StartTable.\n");
    }
    dim_ptr[0]=numericalColumns;
    if (!SDDS_SetArray(&outputPage, "SingularValues", SDDS_POINTER_ARRAY, SValue, dim_ptr))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    dim_ptr[0]=NSVUsed;
    if (!SDDS_SetArray(&outputPage, "SingularValuesUsed", SDDS_POINTER_ARRAY, SValueUsed, dim_ptr))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors); 
    if (!multiplyFile) {
      if (!SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, 
                          numericalColumnName, numericalColumns, oldColumnNames?oldColumnNames:"OldColumnNames"))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else {
      if (strColIndex>=0)
        if (!SDDS_SetColumn(&outputPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE,
                            actuatorName, productRows, strColIndex))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    for(i=0;i<outputColumns;i++) {
      if (!setComplexNamedColumn(&outputPage, outputColumnName[i], outputRows, realData[i], imagData[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      free(realData[i]);
      free(imagData[i]);
    }
    free(realData); realData=NULL;
    free(imagData); imagData=NULL;
    if (!SDDS_WriteTable(&outputPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_FreeDataPage( &outputPage );
    if (reconstructFile) {
      /* R = U S Vt is our convention.  Make multiplcation as sum of
         singular triplets Rnew_ij = (U_ik Sk Vt_kj).  Need to
         multiply all the columns of Ut. Even though some
         singular values may be zero, they may not be at the end of the
         list. */
      /* U and Vt are available and their base are in column order*/
      /* R = U S Vt */
      if (!Rnewt)
        Rnewt = malloc(sizeof(*Rnewt)* rows * numericalColumns);
      if (!V)
        V = malloc(sizeof(*V)*vrows*numericalColumns);
      for (i=0; i<numericalColumns; i++) {
        for (kk=0; kk<vrows; kk++) {
          V[i * vrows + kk].real = Vt[i*vrows + kk].real * SValueUsed[kk];
          V[i * vrows + kk].imag = Vt[i*vrows + kk].imag * SValueUsed[kk];
        }
      }
      kk = MIN(urows, numericalColumns);
      lda = MAX(1, urows);
      ldb = vrows;
      zgemm_("N", "N", &urows, &numericalColumns, &kk, &alpha, U,  &lda, V, &ldb, &beta, Rnewt, &urows);
      realData = imagData = NULL;
      realData = malloc(sizeof(*realData)*numericalColumns);
      imagData = malloc(sizeof(*imagData)*numericalColumns);
      for (i=0; i<numericalColumns; i++) {
        realData[i] = malloc(sizeof(**realData)*rows);
        imagData[i] = malloc(sizeof(**imagData)*rows);
      }
      for (i=0; i<numericalColumns; i++) {
        for (j=0; j<rows; j++) {
          realData[i][j] = Rnewt[i*rows + j].real;
          imagData[i][j] = Rnewt[i*rows + j].imag;
        }
      }
      free(Rnewt); Rnewt = NULL;
      if (verbose&FL_VERYVERBOSE) {
        fprintf(stderr, "Reconstructed (tranposed)");
      }
      if (!copyComplexReconstructionPage(&reconstructPage, &inputPage, NSVUsed, deletedVector))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      SDDS_FreeDataPage(&inputPage);
      for(i=0;i<numericalColumns;i++) {
        sprintf(realcol,"Real%s", numericalColumnName[i]);
        sprintf(imagcol,"Imag%s", numericalColumnName[i]);
        if (!SDDS_SetColumn(&reconstructPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, 
                          realData[i], rows, realcol ) ||
          !SDDS_SetColumn(&reconstructPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, 
                          imagData[i], rows, imagcol ))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        free(realData[i]);
        free(imagData[i]);
      }
      free(realData); realData=NULL;
      free(imagData); imagData=NULL;
      if (!SDDS_WriteTable(&reconstructPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_FreeDataPage( &reconstructPage );
    }
    if (uFile) {
      if (!SDDS_StartTable(&uPage, rows)) 
        SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      if (!SDDS_SetColumn(&uPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                          outputColumnName,
                          rows,
                          newColumnNamesColumn?newColumnNamesColumn:"OriginalRows"))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i=0; i<ucols; i++) {
        sprintf(realcol, "Real%s", orthoColumnName[i]);
        sprintf(imagcol, "Imag%s", orthoColumnName[i]);
        for (j=0; j<rows; j++) {
          if (!SDDS_SetRowValues(&uPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                 j, realcol, U[i*rows+j].real, NULL) ||
              !SDDS_SetRowValues(&uPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                 j, imagcol, U[i*rows+j].imag, NULL))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      if (!SDDS_WriteTable(&uPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_FreeDataPage( &uPage );
    } /*end of writing date to U file */
    free(U); U=NULL;
    if (vFile) {
      /*Vt is in row order, so it is V */
      if (!SDDS_StartTable(&vPage, numericalColumns)) 
        SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      for (i=0; i<numericalColumns; i++) {
        for (j=0; j<vrows; j++) {
          sprintf(realcol, "Real%s", orthoColumnName[j]);
          sprintf(imagcol, "Imag%s", orthoColumnName[j]);
          if (!SDDS_SetRowValues(&vPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                 i, realcol, Vt[i*vrows + j].real, NULL) ||
              !SDDS_SetRowValues(&vPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                 i, imagcol, Vt[i*vrows + j].imag, NULL))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      if (!SDDS_WriteTable(&vPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_FreeDataPage( &vPage );
    } /*end of write vFile */
    free(Vt); Vt=NULL;
    if (sFile) {
      if (!sFileAsMatrix) {
        if (!SDDS_StartTable(&sPage, numericalColumns))
          SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        for ( i=0 ; i < numericalColumns ; i++)
          if (!SDDS_SetRowValues(&sPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, 
                                 i, "Index" , i, NULL) )
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (!SDDS_SetColumn(&sPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, 
                            SValue, numericalColumns, "SingularValues") )
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (!SDDS_StartTable(&sPage, economy?economyRows:rows))
          SDDS_PrintErrors(stderr,SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
        /*long index; */
        for (i=0; i<numericalColumns ; i++)
          for (j=0; j<(economy?economyRows:rows); j++) 
            if (!SDDS_SetRowValues(&sPage, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                   j, i, i==j?SValue[i]:0.0, -1)) {
              fprintf(stderr, "Problem setting S[%ld][%ld] of %d x %d matrix\n", i, j,
                      numericalColumns, rows);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
      }
      if (!SDDS_WriteTable(&sPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_FreeDataPage( &sPage );
    } 
    
    if (verbose&FL_VERBOSE) {
      report_stats(stderr,"\nAt the end of the loop.\n");
    }
  } /*end of page reading loop */
  if (ipage == 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
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

  if (verbose&FL_VERBOSE) {
    report_stats(stderr,"\nAfter terminating SDDS pages.\n");
  }
  free(SValue);
  free(SValueUsed);
  free(InvSValue);
  if (deleteVector) free(deleteVector);
   if (orthoColumnName) {
    SDDS_FreeStringArray(orthoColumnName, MAX(rows,numericalColumns));
    free(orthoColumnName);
  }
  if (freeMultiCol) {
    SDDS_FreeStringArray(multiplyColumnName, multiplyColumns);
    free(multiplyColumnName);
  }
  if (multiStringCol) free(multiStringCol);
  if (inputDescription) free(inputDescription);
  if (inputContents) free(inputContents);
  if (outputColumnName) {
    SDDS_FreeStringArray(outputColumnName, outputColumns);
    free(outputColumnName);
  } 
  if (stringColumnName) free(stringColumnName);
  if (numericalColumnName) SDDS_FreeStringArray(numericalColumnName, numericalColumns);
  free(numericalColumnName);
  if (actuators) {
    SDDS_FreeStringArray(actuatorName, actuators);
    free(actuatorName);
  }
  if (outputDescription) free(outputDescription);
  
  if (weightsName) {
    SDDS_FreeStringArray(weightsName, weightsRows);
    free(weightsName);
    /*  free(weights);*/
  } 
  
  if (corrWeightsName) {
    SDDS_FreeStringArray(corrWeightsName, corrWeightsRows);
    free(corrWeightsName);
  }
  free(Multi);
  if (tmpfile_used && !replaceFileAndBackUp(inputfile, outputfile)) 
    exit(1);
  free_scanargs(&s_arg, argc); 
  return(0);
}



int32_t InitializeInputAndGetColumnNames(SDDS_DATASET *SDDS_dataset, char *filename, 
                                    char ***numericalColumnName, int32_t *numericalColumns,
                                    char **stringColumnName, char **inputDescription, char **inputContents)
{
  char **columnName=NULL, **numColName=NULL, **imagColumnName=NULL;
  int32_t columns=0, i, numCol, stringCol, columnType, realCol, imagCol, error=0;
  
  if (!SDDS_InitializeInput(SDDS_dataset, filename) ||
      !(columnName=(char**)SDDS_GetColumnNames(SDDS_dataset, &columns)))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  if (inputDescription && inputContents) {
    if (!SDDS_GetDescription(SDDS_dataset, inputDescription, inputContents))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  }
  numCol = realCol = imagCol = 0;
  stringCol = -1;
 
  for(i=0;i<columns;i++) {
    if ( SDDS_NUMERIC_TYPE( columnType = SDDS_GetColumnType(SDDS_dataset, i))) {
      if (wild_match(columnName[i], "Real*")) {
        numColName = SDDS_Realloc(numColName, sizeof(*numColName)*(numCol+1));
        SDDS_CopyString(&numColName[numCol], columnName[i]+4);
        numCol ++;
      } else if (wild_match(columnName[i], "Imag*")) {
        imagColumnName = SDDS_Realloc(imagColumnName, sizeof(*imagColumnName)*(imagCol+1));
        SDDS_CopyString(&imagColumnName[imagCol], columnName[i]+4);
        imagCol ++;
      }
      /*numColName = SDDS_Realloc(numColName, sizeof(*numColName)*(numCol+1));
      SDDS_CopyString(&numColName[numCol], columnName[i]);
      numCol++; */
    } else if ( columnType == SDDS_STRING ) {
      if (stringCol==-1) {
        stringCol=i;
        SDDS_CopyString(stringColumnName, columnName[i]);
      }
    }
  }
  realCol = numCol;
  if (realCol!=imagCol) {
    fprintf(stderr, "Error: the real and imaginary columns are not the same.\n");
    exit(1);
  }
  for (i=0; i<numCol; i++) {
    if (match_string(numColName[i], imagColumnName, imagCol, EXACT_MATCH)<0) {
      fprintf(stderr, "Error: real column Real%s has no imaginary column match (i.e., Imag%s column does not exist).\n", numColName[i], numColName[i]);
      error++;
    }
  }
  SDDS_FreeStringArray(imagColumnName, imagCol);
  imagColumnName  = NULL;
  if (error)
    exit(1);
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
    for (i=0; i<SDDS_dataset->layout.n_columns; i++) {
      if (SDDS_dataset->data[i]) {
        if (layout->column_definition[i].type==SDDS_STRING) {
          ptr = (char**)SDDS_dataset->data[i];
          for (j=0; j<SDDS_dataset->n_rows_allocated; j++, ptr++)
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
  return(1);
}

void SetupOutputFiles(char *inputDescription, char *inputContents, char *outputfile,
                      SDDS_DATASET *outputPage, SDDS_DATASET *inputPage, long ascii, short columnMajorOrder, long verbose,
                      char *sFile, char *vFile, char *uFile, SDDS_DATASET *sPage, SDDS_DATASET *vPage, SDDS_DATASET *uPage, char *multiplyFile, long invertMultiply,
                      char *oldColumnNames, char *Symbol, char **outputColumnName, int32_t rows, int32_t numericalColumns, int32_t outputColumns, int32_t productCols,
                      char *newColumnNamesColumn, char *multiStringCol, long *strColIndex, long sFileAsMatrix, char ***orthoColumnName, 
                      long digits, long ucols, char *reconstructFile, SDDS_DATASET *reconstructPage)
{
  char *outputDescription=NULL;
  long i;
  char realcol[256], imagcol[256];
  
  if (inputDescription) {
    outputDescription = (char*) malloc( sizeof(char) * (strlen("Pseudo-inverse of ") + strlen(inputDescription) + 1));
    strcat(strcpy(outputDescription, "Pseudo-inverse of "), inputDescription); 
    if (!SDDS_InitializeOutput(outputPage, ascii?SDDS_ASCII:SDDS_BINARY, 1, outputDescription, inputContents, outputfile))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  } else {
    if (!SDDS_InitializeOutput(outputPage, ascii?SDDS_ASCII:SDDS_BINARY, 1, "Pseudoinverse", "Pseudoinverse", outputfile))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  }
  if (columnMajorOrder!=-1)
    outputPage->layout.data_mode.column_major = columnMajorOrder;
  else
    outputPage->layout.data_mode.column_major = inputPage->layout.data_mode.column_major;
  if (verbose&FL_VERBOSE) {
    report_stats(stderr,"\nAfter SDDS_InitializeOutput.\n");
  }
  
  if (sFile) {
    if (!SDDS_InitializeOutput(sPage, ascii?SDDS_ASCII:SDDS_BINARY, 1, "Singular values", NULL, sFile))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (columnMajorOrder!=-1)
      sPage->layout.data_mode.column_major = columnMajorOrder;
    else
      sPage->layout.data_mode.column_major = inputPage->layout.data_mode.column_major;
  }
  if (uFile) {
    if (!SDDS_InitializeOutput(uPage, ascii?SDDS_ASCII:SDDS_BINARY, 1, "U column-orthogonal matrix", "Orthogonal Matrix", uFile))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (columnMajorOrder!=-1)
      uPage->layout.data_mode.column_major = columnMajorOrder;
    else
      uPage->layout.data_mode.column_major = inputPage->layout.data_mode.column_major;
  }
  if (vFile) {
    if (!SDDS_InitializeOutput(vPage, ascii?SDDS_ASCII:SDDS_BINARY, 1, "V column-orthogonal matrix", "Orthogonal Matrix", vFile))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
    if (columnMajorOrder!=-1)
      vPage->layout.data_mode.column_major = columnMajorOrder;
    else
      vPage->layout.data_mode.column_major = inputPage->layout.data_mode.column_major;
  }
  /* define singular value arrays */
  if(0>SDDS_DefineArray(outputPage, "SingularValues", "SingularValues", NULL, "Singular Values", 
                        NULL, SDDS_DOUBLE, 0, 1, NULL) ||
     0>SDDS_DefineArray(outputPage, "SingularValuesUsed", "SingularValuesUsed", NULL, 
                        "Singular Values Used", 
                        NULL, SDDS_DOUBLE, 0, 1, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  
  /* define names for double columns */
  
  if (!multiplyFile) {
    if ( 0 > SDDS_DefineColumn(outputPage, oldColumnNames?oldColumnNames:"OldColumnNames",
                               NULL, NULL, NULL, NULL, SDDS_STRING, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!defineComplexNamedColumns(outputPage, outputColumnName, rows, Symbol))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!invertMultiply) {
      if (!SDDS_DefineSimpleColumn(outputPage, "OldColumnNames", NULL, SDDS_STRING))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      *strColIndex = 0; 
    }
    if (invertMultiply && (newColumnNamesColumn || multiStringCol)) {
      if (!SDDS_DefineSimpleColumn(outputPage, newColumnNamesColumn ? newColumnNamesColumn : multiStringCol, NULL, SDDS_STRING))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      *strColIndex = 0;
    }
    if (!defineComplexNamedColumns(outputPage, outputColumnName, productCols, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (verbose&FL_VERBOSE) {
    report_stats(stderr,"\nAfter defining columns.\n");
  }
  if (uFile || vFile || (sFile && sFileAsMatrix)) {
    if (!(*orthoColumnName = makeComplexIndexNames("SV", MAX(rows, numericalColumns), digits)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (uFile) {
    if ( 0 > SDDS_DefineColumn(uPage, 
                               newColumnNamesColumn?newColumnNamesColumn:"OriginalRows",
                                   NULL, NULL, NULL, NULL, SDDS_STRING, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    /* the number of rows of U depends on the method used and the
       economy mode flag.*/
    for (i=0; i<ucols; i++) {
      sprintf(realcol, "Real%s", (*orthoColumnName)[i]);
      sprintf(imagcol, "Imag%s", (*orthoColumnName)[i]);
      if (!SDDS_DefineSimpleColumn(uPage, realcol, NULL, SDDS_DOUBLE) ||
          !SDDS_DefineSimpleColumn(uPage, imagcol, NULL, SDDS_DOUBLE)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    if (!SDDS_WriteLayout(uPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (vFile) {
    if ( 0 > SDDS_DefineSimpleColumn(vPage,
                                     oldColumnNames?oldColumnNames:"OldColumnNames",
                                     NULL, SDDS_STRING))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i=0; i<numericalColumns; i++) {
      sprintf(realcol, "Real%s", (*orthoColumnName)[i]);
      sprintf(imagcol, "Imag%s", (*orthoColumnName)[i]);
      if (!SDDS_DefineSimpleColumn(vPage, realcol, NULL, SDDS_DOUBLE) ||
          !SDDS_DefineSimpleColumn(vPage, imagcol, NULL, SDDS_DOUBLE)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    if (!SDDS_WriteLayout(vPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (sFile) {
    if (!sFileAsMatrix) {
      if ( 0 > SDDS_DefineSimpleColumn(sPage, "Index", 
                                           NULL, SDDS_LONG) ||
           0 > SDDS_DefineSimpleColumn(sPage, "SingularValues",
                                       NULL, SDDS_DOUBLE) ||
           !SDDS_WriteLayout(sPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else {
      if ( 0 > SDDS_DefineSimpleColumns(sPage, 
                                        numericalColumns, 
                                        *orthoColumnName, NULL, SDDS_DOUBLE) ||
           !SDDS_WriteLayout(sPage))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (0>SDDS_DefineParameter(outputPage, "MinimumSingularValueRatio", "MinimumSingularValueRatio", NULL, 
                             "Minimum singular value ratio", NULL, SDDS_DOUBLE, NULL)||
      0>SDDS_DefineParameter(outputPage, "NumberOfSingularValuesUsed", "NumberOfSingularValuesUsed", NULL, 
                             "largest singular value used", NULL, SDDS_LONG, NULL)||
      0>SDDS_DefineParameter(outputPage, "DeletedVectors", "DeletedVectors", NULL, 
                             "list of vectors that were deleted", NULL, SDDS_STRING, NULL)||
      0>SDDS_DefineParameter(outputPage, "InputFile", "InputFile", NULL,
                             "InputFile", NULL, SDDS_STRING, 0) ||
      0>SDDS_DefineParameter(outputPage, "ConditionNumber", "ConditionNumber", NULL,
                             "Condition Number", NULL, SDDS_DOUBLE, 0) ||
      !SDDS_WriteLayout(outputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  
  
  if (reconstructFile) {
    if (!initializeComplexReconstruction(reconstructPage, inputPage, reconstructFile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}

void ReadOtherInputData(long verbose, long noWarnings, long ipage, SDDS_DATASET *inputPage, char *Root, char *stringColumnName, char *newColumnNamesColumn,
                        long digits, SDDS_DATASET *multiplyPage, long invertMultiply, char **multiplyColumnName, int32_t multiplyColumns, char *multiplyFile,
                        int32_t numericalColumns, char **numericalColumnName, char *multiStringCol, long includeWeights, char *weightsFile, char *weightsNamesColumn,
                        long includeCorrWeights, char *corrWeightsFile, char *corrWeightsNamesColumn, char *weightsValuesColumn, char *corrWeightsValuesColumn,
                        int32_t *rows, int32_t *rowsFirstPage, char ***outputColumnName, int32_t *outputColumns, 
                        int32_t *multiplyRows, int32_t *freeMultiCol, char ***actuatorName, int32_t *actuators, MKL_Complex16 **Multi,
                        double **w, double **corrW)
{
  char realcol[256], imagcol[256];
  long i, j;
  int32_t mpage;
  static int32_t lastMultiplyPage = 0;
  double *real=NULL, *imag=NULL;

  if (verbose&FL_VERBOSE) {
    report_stats(stderr,"\nAfter reading page.\n");
  }
  if (ipage==1) {
    if ( !(*rows = SDDS_CountRowsOfInterest(inputPage))) {
      SDDS_Bomb("No rows in dataset.");
    }
    if (verbose&FL_VERBOSE)
      fprintf(stderr, "Page %ld has %" PRId32 " rows.\n", ipage, *rows);
    *rowsFirstPage = *rows;
    if (!Root && !newColumnNamesColumn && !stringColumnName)
      Root = "Column";
    /* Generated names share one width, which the -root names built below reuse. */
    if (Root)
      digits = widenComplexNameDigits(digits, *rows);
    if (!(*outputColumnName = getComplexOutputNames(inputPage, Root, stringColumnName, newColumnNamesColumn, digits)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    *outputColumns = *rows;
  } else {
    if ( !(*rows = SDDS_CountRowsOfInterest(inputPage)) ) {
      SDDS_Bomb("No rows in dataset.");
    }
    if (verbose&FL_VERBOSE)
      fprintf(stderr, "Page %ld has %" PRId32 " rows.\n", ipage, *rows);
    if (*rows != *rowsFirstPage) {
      SDDS_Bomb("Datasets don't have the same number of rows.\nProcessing stopped before reaching the end of the input file.");
    }
  }
  /* ***** read multiply matrix file */
  if (multiplyFile) {
    mpage = SDDS_ReadPage(multiplyPage);
    if (mpage == 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (mpage < 0) {
      /* A single multiply page applies to every input page; otherwise pages must pair up. */
      if (lastMultiplyPage == 0)
        SDDS_Bomb("The multiply matrix file has no data pages.");
      if (lastMultiplyPage > 1)
        SDDS_Bomb("The multiply matrix file has fewer pages than the input file.");
    }
    if (mpage>0) {
      lastMultiplyPage = mpage;
      *multiplyRows = SDDS_CountRowsOfInterest(multiplyPage);
      if (mpage==1) {
        if (!invertMultiply) {
          if (*outputColumnName) {
            SDDS_FreeStringArray(*outputColumnName, *rows);
            free(*outputColumnName);
          }
          *actuatorName = numericalColumnName;  
          *outputColumns = multiplyColumns;
          if (!Root)
            *outputColumnName = multiplyColumnName;
          else {
            *freeMultiCol = 1;
            if (!(*outputColumnName = makeComplexIndexNames(Root, *outputColumns, digits)))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        } else {
          /*get the actutaor name from multiply matrix */
          if (multiStringCol || newColumnNamesColumn) {
            if (!(*actuatorName = getComplexOutputNames(multiplyPage, NULL, multiStringCol,
                                                        multiStringCol ? NULL : newColumnNamesColumn, digits)))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            *actuators = *multiplyRows;
          }
          if (multiplyColumnName) 
            *freeMultiCol = 1;
          *outputColumns = *rows;
          if (Root) {
            SDDS_FreeStringArray(*outputColumnName, *outputColumns);
            free(*outputColumnName);
            if (!(*outputColumnName = makeComplexIndexNames(Root, *outputColumns, digits)))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        } 
      }
      /* Each multiply page may have a different row count. */
      free(*Multi);
      if (!(*Multi = malloc(sizeof(**Multi)* (*multiplyRows) * multiplyColumns)))
        SDDS_Bomb("Error allocating memory for the multiply matrix.");
      for (i=0; i< multiplyColumns; i++) {
        sprintf(realcol, "Real%s", multiplyColumnName[i]);
        sprintf(imagcol, "Imag%s", multiplyColumnName[i]);
        if (!(real = SDDS_GetColumnInDoubles(multiplyPage, realcol)) ||
            !(imag = SDDS_GetColumnInDoubles(multiplyPage, imagcol)))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors); 
        for (j=0; j< *multiplyRows; j++) {
          (*Multi)[i* (*multiplyRows) + j].real= real[j];
          (*Multi)[i* (*multiplyRows) + j].imag = imag[j];
        }
        free(real); real=NULL;
        free(imag); imag=NULL;
      }
      /* free data page memory as soon as possible */
      SDDS_FreeDataPage(multiplyPage);
    }
  } /*end if multiplyFile */
  
  readComplexWeightInputs(ipage, verbose & FL_VERBOSE, noWarnings,
                          includeWeights, weightsFile, weightsNamesColumn, weightsValuesColumn,
                          includeCorrWeights, corrWeightsFile, corrWeightsNamesColumn, corrWeightsValuesColumn,
                          *rows, *outputColumnName, numericalColumns, numericalColumnName, w, corrW);

}

/* Auxiliary routine: printing a matrix */
void print_matrix( char* desc, MKL_INT m, MKL_INT n, MKL_Complex16* a, MKL_INT lda ) {
        MKL_INT i, j;
        printf( "\n %s\n", desc );
        for( j = 0; j < n; j++ ) {
          printf("column %d:  ", j);
          for( i = 0; i < m; i++ )
            printf( " (%8.5f,%8.5f)", a[j*lda+i].real, a[j*lda+i].imag );
          printf( "\n" );
        }
}

/* Auxiliary routine: printing a real matrix */
void print_rmatrix( char* desc, MKL_INT m, MKL_INT n, double* a, MKL_INT lda ) {
  MKL_INT i, j;
  printf( "\n %s\n", desc );
  for( i = 0; i < n; i++ ) {
    printf("column %d:  ", i);
    for( j = 0; j < m; j++ ) printf( " %8.5f", a[i*lda+j] );
    printf( "\n" );
  }
}

int cprintmatrix( char *matname, int m, int n, MKL_Complex16 *A){
  int i,j;
  
  printf("%s = [\n", matname);
  for( i = 0; i < m; i++){
    for( j = 0; j < n; j++ )
      printf("%1.5e + %1.5ei ",A[i+j*m].real,A[i+j*m].imag);
    printf("\n");
  }
  printf("]; \n");

  return 0;
}
#endif
