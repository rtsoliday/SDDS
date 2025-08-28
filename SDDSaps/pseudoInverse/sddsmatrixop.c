/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution.
\*************************************************************************/

#include "mdb.h"
#include "scan.h"
#include "rpn.h"
#include "matrixop.h"
#ifdef MKL
#  include <omp.h>
#endif

#define CLO_PUSH 0
#define CLO_ADD 1
#define CLO_SUBTRACT 2
#define CLO_SCALARMULTIPLY 3
#define CLO_SCALARDIVIDE 4
#define CLO_TRANSPOSE 5
#define CLO_INVERT 6
#define CLO_MULTIPLY 7
#define CLO_IDENTITY 8
#define CLO_PIPE 9
#define CLO_NEGATION 10
#define CLO_VERBOSE 11
#define CLO_DIVIDE 12
#define CLO_SWAP 13
#define CLO_COLUMNNAMES 14
#define CLO_DETERMINANT 15
#define CLO_ROOT 16
#define CLO_DIGITS 17
#define CLO_PRINTPACKAGE 18
#define CLO_MAJOR_ORDER 19
#define CLO_THREADS 20
#define N_OPTIONS 21

/*note: only -pipe=out is valid */
char *option[N_OPTIONS] = {"push", "add", "subtract", "scalarmultiply", "scalardivide",
  "transpose", "invert", "multiply", "identity", "pipe", "negation", "verbose", "divide", "swap", "columnNames", "determinant", "root", "digits", "printPackage", "majorOrder", "threads"};

#define BINARYOPTIONS 5
char *binaryOption[BINARYOPTIONS] = {
  "add",
  "subtract",
  "multiply",
  "divide",
  "swap",
};

/*here, the unary option just mean that only one matrix is needed to be poped from the stack */
#define UNARYOPTIONS 7
char *unaryOption[UNARYOPTIONS] = {
  "scalarmultiply",
  "scalardivide",
  "transpose",
  "invert",
  "identity",
  "negation",
  "determinant",
};

char *USAGE1 = "sddsmatrixop [input_matrix] [output_matrix] [-pipe=in|out] [-verbose] \n\
{[-push=<matrix>] [-multiply] | [-add] | [-subtract] | [-invert] | [-transpose] ...} [-root=<string>] [-digits=<number>] \n\
[-columnNames=filename=<file>[,column=<column>]] [-determinant]\n\
[-threads=<num>] [-printPackage] [-majorOrder=row|column] \n               \
<input_matrix>       SDDS file which contains the input matrix -- the first element \n\
                     in the stack.\n\
<output_matrix>      The result matrix is written into SDDS file named by <output_matrix>.\n\
-push=<matrix>       the matrix (file) that will pushed into stack. \n\
-add                 addition operator, other operator are also named by their meanings. \n\
                     such as multiply, invert, transpose, negation, etc. \n\
-invert[=largestSingularValues=<integer>]|[-smallestSingularValues=<integer>]|[minimumSingularValueRatio=<value>]]  \n\
                     matrix inversion.\n\
-identity            means push a unit matrix on the stack which has the same demension as \n\
                     top top matrix in the statck. \n\
-identity=<number>   push a square unit matrix with dimension of <number> into stack.\n\
-swap                swap the top two matrices in the stack. \n";
char *USAGE2 = "-multiply            normal matrix-matrix multiplication. \n\
-multiply=hadamard   element-by-element multiplication, similar to addition but \n\
                     with * operator. \n\
-divide=hadamard     element-by-element division. \n\
-columnNames=filename=<filename>[,column=<column>] \n\
                     This takes column <column> from <filename>. The default for <column> is \n\
                     ColumnName. This <column> supplies the output column names. \n\
-scalarmultiply=<value> multiply the matrix by a constant value. \n\
-scalardivide=<value>   divide the matrix by a constact value. \n\
-determinant         compute the determinant of final matrix and write to the output file \n\
                     \"Determinant\" parameter.\n\
-root                 use the string specified to generate column names.\n\
-digits               minimum number of digits used in the number appended to the root\n\
                      part of the column names. Default is value 2.\n\
-printPackage         prints out the linear algebra package that was compiled.\n\
-majorOrder      Specifies the output file written in row or column major order\n\
The -push and operator can be repeated many times. \n\n\
sddsmatrixop provides a generic method for matrix operations.\n\
Program by Hairong Shang. (This is version 1.0, June 2003)\n";

#define MAX_PAGES 1024 /*the number of pages in a file, each page is an element of an array stack */
long matrixStack[MAX_PAGES][STACKSIZE];
long matrixStackPtr[MAX_PAGES];
/*each page of files is a stack, each stack has a maximum number size of STACKSIZE */
/*the array size of stack is number of pages in the input file */
static long input_pages; /*number of pages in the input file */
int32_t InitializeInputAndGetColumnNames(SDDS_DATASET *SDDS_dataset, char *filename,
                                         char ***numericalColumnName, int32_t *numericalColumns);

void push_matrix(char *fileName, char ***doubleColumnName, int32_t *doubleColumnNames, long isInput);
long push_address(long address, long index);
long pop_address(long index);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE outputPage, columnPage;
  char *inputFile, *outputFile, *matrixA, *matrixB, **doubleColumnName, *columnNameFile, **givenColumnName, *columnNameFileColumn;
  char *root = NULL, **outputColumnName = NULL;
  unsigned long pipeFlags, dummyFlags, majorOrderFlag;
  int32_t currentOption, i, j, i_arg, n, keepDefinition, doubleColumns, givenColumnNames, do_determinant, outputColumns;
  long digits = 0, threads = 1, identityN, tmpfile_used;
  double scalarValue, det;
  MAT **A, **B, **temp;
  short isBinary, isUnary, verbose = 0;
  char format[100], name[1024];
  int32_t operationsSupplied = 0, largestSValue = 0, smallestSValue = 0, inputRead = 0;
  double minRatio = 0;
  short columnMajorOrder = 0;

  do_determinant = 0;
  input_pages = 0;
  columnNameFileColumn = NULL;
  doubleColumnName = NULL;
  doubleColumns = 0;
  inputFile = outputFile = NULL;
  A = B = temp = NULL;
  pipeFlags = 0;
  tmpfile_used = 0;
  dstackptr = 0;
  sstackptr = 0;
  keepDefinition = 1;
  matrixA = matrixB = NULL;
  columnNameFile = NULL;
  givenColumnName = NULL;
  givenColumnNames = 0;

  for (i = 0; i < MAX_PAGES - 1; i++)
    matrixStackPtr[i] = 0;
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s%s\n", USAGE1, USAGE2);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    isBinary = isUnary = 0;
    if (s_arg[i_arg].arg_type == OPTION) {
      if (input_pages && !A && !B) {
        A = (MAT **)malloc(sizeof(*A) * input_pages);
        B = (MAT **)malloc(sizeof(*B) * input_pages);
        temp = (MAT **)malloc(sizeof(*temp) * input_pages);
      }
      delete_chars(s_arg[i_arg].list[0], "_");
      currentOption = match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0);
      if (match_string(s_arg[i_arg].list[0], binaryOption, BINARYOPTIONS, 0) >= 0) {
        isBinary = 1;
        if (sstackptr < 2) {
          fprintf(stderr, "Too few values on string stack.\n");
          exit(1);
        }
        matrixB = pop_string();
        matrixA = pop_string();
        for (i = 0; i < input_pages; i++) {
          if (!(B[i] = (MAT *)pop_address(i)) || !(A[i] = (MAT *)pop_address(i)))
            exit(1);
        }
      } else if (match_string(s_arg[i_arg].list[0], unaryOption, UNARYOPTIONS, 0) >= 0) {
        isUnary = 1;
        if (sstackptr < 1) {
          fprintf(stderr, "Too few values on string stack.\n");
          exit(1);
        }
        matrixB = pop_string();
        for (i = 0; i < input_pages; i++)
          if (!(B[i] = (MAT *)pop_address(i)))
            exit(1);
      }
      switch (currentOption) {
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
      case CLO_PRINTPACKAGE:
#if defined(CLAPACK)
        fprintf(stderr, "Compiled with package CLAPACK\n");
#elif defined(LAPACK)
        fprintf(stderr, "Compiled with package LAPACK\n");
#elif defined(MKL)
        fprintf(stderr, "Compiled with package MKL\n");
#else
        fprintf(stderr, "Compiled with oag new matrix library\n");
#endif
        if (argc < 3)
          SDDS_Bomb("Two few arguments.");
        break;
      case CLO_ROOT:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -root option!");
        root = s_arg[i_arg].list[1];
        break;
      case CLO_DIGITS:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -digits option!");
        if (!get_long(&digits, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -digits value provided!");
        break;
      case CLO_THREADS:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -threads option!");
        if (!get_long(&threads, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -threads value provided!");
        break;
      case CLO_COLUMNNAMES:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -columnNames option!");
        s_arg[i_arg].list += 1;
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items > 0 &&
            !scanItemList(&dummyFlags, s_arg[i_arg].list, &s_arg[i_arg].n_items, 0,
                          "filename", SDDS_STRING, &columnNameFile, 1, 0,
                          "column", SDDS_STRING, &columnNameFileColumn, 1, 0, NULL))
          SDDS_Bomb("invalid -parameter syntax");
        if (!columnNameFileColumn)
          SDDS_CopyString(&columnNameFileColumn, "ColumnName");
        s_arg[i_arg].list -= 1;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_PUSH:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -push option!");
        push_matrix(s_arg[i_arg].list[1], &doubleColumnName, &doubleColumns, 0);
        push_string(s_arg[i_arg].list[1]);
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        switch (pipeFlags) {
        case USE_STDOUT:
          break;
        default:
          push_matrix(NULL, &doubleColumnName, &doubleColumns, 1);
          push_string("stdin");
          inputRead = 1;
          break;
        }
        break;
      case CLO_ADD:
        for (i = 0; i < input_pages; i++) {
          if (!matrix_add_sm(A[i], B[i])) {
            fprintf(stderr, "The dimension of file %s and %s at page %d does not match for addtion!\n",
                    matrixA, matrixB, i + 1);
            exit(1);
          }
          matrix_free(B[i]);
          temp[i] = A[i];
        }
        operationsSupplied++;
        break;
      case CLO_MULTIPLY:
        if (s_arg[i_arg].n_items == 1) {
          for (i = 0; i < input_pages; i++) {
            temp[i] = matrix_mult(A[i], B[i]);
            matrix_free(A[i]);
            matrix_free(B[i]);
          }
          keepDefinition = 0;
        } else if (s_arg[i_arg].n_items == 2) {
          n = strlen(s_arg[i_arg].list[1]);
          if (strncmp(s_arg[i_arg].list[1], "hadamard", n))
            SDDS_Bomb("Invalid -multiply=<value> option!");
          for (i = 0; i < input_pages; i++) {
            if (!matrix_h_mult_sm(A[i], B[i])) {
              fprintf(stderr, "The dimension of file %s and %s at page %d should have same dimensions for element-by-element multiplication!\n",
                      matrixA, matrixB, i + 1);
              exit(1);
            }
            matrix_free(B[i]);
            temp[i] = A[i];
          }
        } else
          SDDS_Bomb("Invald -multiply option, should be -multiply or -multiply=hadamard!");
        operationsSupplied++;

        break;
      case CLO_DIVIDE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -divide option!");
        n = strlen(s_arg[i_arg].list[1]);
        if (strncmp(s_arg[i_arg].list[1], "hadamard", n))
          SDDS_Bomb("Invalid -divide option!");
        for (i = 0; i < input_pages; i++) {
          if (!matrix_h_divide_sm(A[i], B[i])) {
            fprintf(stderr, "The dimension of file %s and %s at page %d should have same dimensions for element-by-element division!\n",
                    matrixA, matrixB, i + 1);
            exit(1);
          }
          temp[i] = A[i];
        }
        operationsSupplied++;
        break;
      case CLO_SUBTRACT:
        for (i = 0; i < input_pages; i++) {
          if (!matrix_sub_sm(A[i], B[i])) {
            fprintf(stderr, "The dimension of file %s and %s at page %d does not match for subtraction!\n",
                    matrixA, matrixB, i + 1);
            exit(1);
          }
          temp[i] = A[i];
        }
        operationsSupplied++;
        break;
      case CLO_SWAP:
        for (i = 0; i < input_pages; i++) {
          push_address((long)B[i], i);
          push_address((long)A[i], i);
        }
        push_string(matrixB);
        push_string(matrixA);
        keepDefinition = 0;
        operationsSupplied++;
        break;
      case CLO_SCALARMULTIPLY:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -scalarmultiply option!");
        if (!(get_double(&scalarValue, s_arg[i_arg].list[1])))
          SDDS_Bomb("no value given for option -scalarmultiply");
        for (i = 0; i < input_pages; i++) {
          for (j = 0; j < B[i]->m * B[i]->n; j++)
            B[i]->base[j] *= scalarValue;
          temp[i] = B[i];
        }
        operationsSupplied++;
        break;
      case CLO_SCALARDIVIDE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -scalardivide option!");
        if (!(get_double(&scalarValue, s_arg[i_arg].list[1])))
          SDDS_Bomb("no value given for option -scalardivide");
        if (!scalarValue)
          SDDS_Bomb("Matrix is divided by zero!");
        for (i = 0; i < input_pages; i++) {
          for (j = 0; j < B[i]->m * B[i]->n; j++)
            B[i]->base[j] /= scalarValue;
          temp[i] = B[i];
        }
        operationsSupplied++;
        break;
      case CLO_TRANSPOSE:
        for (i = 0; i < input_pages; i++) {
          temp[i] = matrix_transpose(B[i]);
          matrix_free(B[i]);
        }
        keepDefinition = 0;
        operationsSupplied++;
        break;
      case CLO_INVERT:
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items > 0 &&
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "largestSingluarValuesr", SDDS_LONG, &largestSValue, 1, 0,
                          "smallestSingularValues", SDDS_LONG, &smallestSValue, 1, 0,
                          "minimumSingularValueRatio", SDDS_DOUBLE, &minRatio, 1, 0,
                          NULL))
          SDDS_Bomb("invalid -parameter syntax");
        s_arg[i_arg].n_items += 1;
        if (largestSValue * smallestSValue * minRatio != 0)
          SDDS_Bomb("Only one of largestSValue, smallestSValue and ratio can be specified!");
        for (i = 0; i < input_pages; i++) {
          temp[i] = matrix_invert(B[i], largestSValue, smallestSValue, minRatio,
                                  0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
          matrix_free(B[i]);
        }
        operationsSupplied++;
        keepDefinition = 0;
        break;
      case CLO_NEGATION:
        for (i = 0; i < input_pages; i++) {
          for (j = 0; j < B[i]->m * B[i]->n; j++)
            B[i]->base[i] *= -1;
          temp[i] = B[i];
        }
        operationsSupplied++;
        break;
      case CLO_IDENTITY:
        if (s_arg[i_arg].n_items == 1) {
          /*the indentity matrix has the same dimension as the top matrix */
          for (i = 0; i < input_pages; i++)
            temp[i] = matrix_identity(B[i]->m, B[i]->n);
        } else if (s_arg[i_arg].n_items == 2) {
          if (!(get_long(&identityN, s_arg[i_arg].list[1])))
            SDDS_Bomb("invalid -identity option");
          for (i = 0; i < input_pages; i++)
            temp[i] = matrix_identity(identityN, identityN);
        } else
          SDDS_Bomb("invalid -identity option");
        /*push back the poped matrix */
        for (i = 0; i < input_pages; i++)
          push_address((long)B[i], i);
        push_string(matrixB);
        operationsSupplied++;
        break;
      case CLO_DETERMINANT:
        do_determinant = 1;
        for (i = 0; i < input_pages; i++)
          temp[i] = B[i];
        operationsSupplied++;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      default:
        fprintf(stderr, "option %s is invalid!\n", s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
      if ((isBinary || isUnary) && currentOption != CLO_SWAP) {
        for (i = 0; i < input_pages; i++)
          push_address((long)temp[i], i);
        push_string("Intermediate Matrix");
      }
      if (isUnary)
        free(matrixB);
      if (isBinary) {
        free(matrixA);
        free(matrixB);
      }
    } else {
      if (inputFile == NULL && !inputRead) {
        inputFile = s_arg[i_arg].list[0];
        push_matrix(inputFile, &doubleColumnName, &doubleColumns, 1);
        push_string(inputFile);
      } else if (outputFile == NULL)
        outputFile = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "argument %s is invalid: too many filenames (sddsmatrixop)\n",
                s_arg[i_arg].list[0]);
        exit(1);
      }
    }
  }
#ifdef MKL
  omp_set_num_threads(threads);
#endif

  processFilenames("sddsmatrixop", &inputFile, &outputFile, pipeFlags, 1, &tmpfile_used);
  if (inputFile && tmpfile_used && verbose)
    fprintf(stderr, "Warning: input file %s will be overwritten.\n", inputFile);
  if (!operationsSupplied) {
    fprintf(stderr, "No operations performed on matrices!\n");
    exit(1);
  }
  if (columnNameFile) {
    if (!SDDS_InitializeInput(&columnPage, columnNameFile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (0 > SDDS_ReadPage(&columnPage))
      SDDS_Bomb("Unable to read column file!");
    givenColumnNames = SDDS_CountRowsOfInterest(&columnPage);
    if (givenColumnNames) {
      if (!(givenColumnName = (char **)SDDS_GetColumn(&columnPage, columnNameFileColumn)))
        SDDS_Bomb("Unable to get the ColumnName from column name file!");
    }
    if (!SDDS_Terminate(&columnPage))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_InitializeOutput(&outputPage, SDDS_BINARY, 1, NULL, NULL, outputFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  outputPage.layout.data_mode.column_major = columnMajorOrder;

  if (do_determinant && !SDDS_DefineSimpleParameter(&outputPage, "Determinant", NULL,
                                                    SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (givenColumnNames) {
    SDDS_FreeStringArray(doubleColumnName, doubleColumns);
    free(doubleColumnName);
    outputColumns = temp[0]->n;
    if (outputColumns <= givenColumnNames)
      outputColumnName = givenColumnName;
    else {
      outputColumnName = (char **)malloc(sizeof(*outputColumnName) * temp[0]->n);
      for (i = 0; i < outputColumns; i++) {
        if (i < givenColumnNames) {
          outputColumnName[i] = givenColumnName[i];
        } else {
          sprintf(name, "doubleColumnName%d", i);
          SDDS_CopyString(&outputColumnName[i], name);
        }
      }
    }
  } else if (!keepDefinition) {
    SDDS_FreeStringArray(doubleColumnName, doubleColumns);
    free(doubleColumnName);
    outputColumns = temp[0]->n;
    if (!digits)
      digits = log10(temp[0]->n) + 1;
    if (!root)
      sprintf(format, "doubleColumn%%0%ldld", digits);
    else
      sprintf(format, "%s%%0%ldld", root, digits);
    outputColumnName = (char **)malloc(sizeof(*outputColumnName) * outputColumns);
    for (i = 0; i < outputColumns; i++) {
      sprintf(name, format, i);
      SDDS_CopyString(&outputColumnName[i], name);
    }
  } else {
    if (doubleColumns != temp[0]->n)
      SDDS_Bomb("The columns of output matrix and double columns do not match!");
    outputColumns = doubleColumns;
    outputColumnName = doubleColumnName;
  }
  if (!SDDS_DefineSimpleColumns(&outputPage, outputColumns, outputColumnName, NULL, SDDS_DOUBLE))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(&outputPage))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (j = 0; j < input_pages; j++) {
    if (!SDDS_StartPage(&outputPage, temp[j]->m))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (do_determinant) {
      det = matrix_det(temp[j]);
      if (!SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                              "Determinant", det, NULL))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    for (i = 0; i < temp[j]->n; i++) {
      if (i < outputColumns &&
          !SDDS_SetColumn(&outputPage, SDDS_SET_BY_NAME, temp[j]->me[i], temp[j]->m, outputColumnName[i]))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (!SDDS_WritePage(&outputPage))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    matrix_free(temp[j]);
  }
  if (!!SDDS_Terminate(&outputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (tmpfile_used && !replaceFileAndBackUp(inputFile, outputFile))
    exit(1);

  free_scanargs(&s_arg, argc);
  free(A);
  free(B);
  free(temp);
  if (columnNameFileColumn)
    free(columnNameFileColumn);
  if (columnNameFile)
    free(columnNameFile);
  if (outputColumns <= givenColumnNames) {
    SDDS_FreeStringArray(givenColumnName, givenColumnNames);
    free(givenColumnName);
  } else {
    SDDS_FreeStringArray(outputColumnName, outputColumns);
    free(outputColumnName);
    if (givenColumnName)
      free(givenColumnName);
  }
  return 0;
}

void push_matrix(char *fileName, char ***doubleColumnName, int32_t *doubleColumns, long isInput) {
  SDDS_DATASET inputPage;
  char **numericalColumnNames = NULL;
  int32_t numericalColumns, rows;
  long i, ipage, pages;
  MAT *matrix = NULL, *matrix1;

  /* note that the matrix loaded from input file is actually the transpose matrix of it should be
     to save time. The operations are done between tranposed matrix;
     the benefit is that: no transpose need to be done when write to output file;
     while in previose version, readfrom input -->transpose -->operation --> transpose --> output
     now, readfrom input-->operation-->output, should be much more efficient */
  /* for subtraction, addition, hadamard multiplya and division, the operation order are the same
     however, for C=A*B, Ct = Bt*At */
  InitializeInputAndGetColumnNames(&inputPage, fileName, &numericalColumnNames, &numericalColumns);

  /* count the numerical columns in input */
  pages = 0;
  while ((ipage = SDDS_ReadTable(&inputPage)) > 0) {
    if (!SDDS_SetColumnFlags(&inputPage, 0))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (!SDDS_SetColumnsOfInterest(&inputPage, SDDS_NAME_ARRAY, numericalColumns, numericalColumnNames))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (!(rows = SDDS_CountRowsOfInterest(&inputPage))) {
      fprintf(stderr, "No date found in file %s\n", fileName ? fileName : "stdin");
      exit(1);
    }
    matrix = NEW(MAT);
    matrix->n = matrix->max_n = numericalColumns;
    matrix->m = matrix->max_m = rows;

    if (!(matrix->base = (Real *)SDDS_GetCastMatrixOfRowsByColumn(&inputPage, &rows, SDDS_DOUBLE)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    matrix->me = (Real **)calloc(matrix->n, sizeof(Real *));
    for (i = 0; i < matrix->n; i++)
      matrix->me[i] = &(matrix->base[i * matrix->m]);
    push_address((long)matrix, pages);
    pages++;
    if (pages > MAX_PAGES) {
      fprintf(stderr, "The number of pages in %s file exceeds 256!\n", fileName ? fileName : "stdin");
      exit(1);
    }
  }

  if (input_pages == 0)
    input_pages = pages;
  else if (pages > input_pages) {
    fprintf(stderr, "Warning: file %s has more pages than the input file, the extra pages will be ignored.\n", fileName);
  } else if (pages < input_pages) {
    fprintf(stderr, "Warning: file %s has less pages than the input, its last page will be reused!\n", fileName);
    for (ipage = pages; ipage < input_pages; ipage++) {
      matrix1 = matrix_copy(matrix);
      push_address((long)matrix1, ipage);
    }
  }
  if (!SDDS_Terminate(&inputPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (isInput) {
    *doubleColumns = numericalColumns;
    *doubleColumnName = numericalColumnNames;
  } else {
    SDDS_FreeStringArray(numericalColumnNames, numericalColumns);
    free(numericalColumnNames);
  }
}

long push_address(long address, long index) {
  if (index >= MAX_PAGES) {
    fputs("The index of stack exceeds the MAX_PAGES\n", stderr);
    return (0);
  }
  if (matrixStackPtr[index] >= STACKSIZE) {
    fputs("stack overflow--numeric stack size exceeded (push_address)\n", stderr);
    return (0);
  }
  matrixStack[index][matrixStackPtr[index]++] = address;
  return 1;
}

long pop_address(long index) {
  if (index >= MAX_PAGES) {
    fputs("The index of stack exceeds the MAX_PAGES\n", stderr);
    return (0);
  }
  if (matrixStackPtr[index] < 1) {
    fprintf(stderr, "too few items on matrix stack %ld\n", index);
    return (0);
  }
  return (matrixStack[index][--matrixStackPtr[index]]);
}

int32_t InitializeInputAndGetColumnNames(SDDS_DATASET *SDDS_dataset, char *filename,
                                         char ***numericalColumnName, int32_t *numericalColumns) {
  char **columnName = NULL, **numColName = NULL;
  int32_t columns = 0, i, numCol, columnType;

  if (!SDDS_InitializeInput(SDDS_dataset, filename) ||
      !(columnName = (char **)SDDS_GetColumnNames(SDDS_dataset, &columns)))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  numCol = 0;
  for (i = 0; i < columns; i++) {
    if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(SDDS_dataset, i))) {
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
