/**
 * @file sddsdistest.c
 * @brief Statistical distribution testing tool for SDDS datasets.
 *
 * @details
 * This program performs statistical tests on columns of data from an SDDS file
 * against specified distributions (Gaussian, Poisson, Student's t, or Chi-Squared).
 * It supports both Kolmogorov-Smirnov (KS) and Chi-Squared tests, along with the
 * ability to use user-defined distributions provided via external files.
 *
 * @section Usage
 * ```
 * sddsdistest [<inputfile>] [<outputfile>]
 *             [-pipe=[in][,out]]
 *              -column=<name>[,sigma=<name>]...
 *             [-exclude=<name>[,...]]
 *             [-degreesOfFreedom={<value>|@<parameterName>}]
 *             [-test={ks|chisquared}]
 *             [-fileDistribution=<filename>,<indepName>,<depenName>]
 *             [-gaussian]
 *             [-poisson]
 *             [-student]
 *             [-chisquared]
 *             [-majorOrder=row|column]
 *             [-threads=<number>]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-column`                             | Specifies the columns to test, with optional sigma columns for error handling.        |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Enables piping for input and/or output data streams.                                  |
 * | `-exclude`                            | Excludes specified columns from testing.                                             |
 * | `-degreesOfFreedom`                   | Specifies degrees of freedom as a fixed value or a parameter.                        |
 * | `-test`                               | Selects the test to perform: Kolmogorov-Smirnov or Chi-Squared.                      |
 * | `-fileDistribution`                   | Uses a user-defined distribution from a file.                                         |
 * | `-gaussian`                           | Tests against a Gaussian distribution.                                               |
 * | `-poisson`                            | Tests against a Poisson distribution.                                                |
 * | `-student`                            | Tests against a Student's t distribution.                                            |
 * | `-chisquared`                         | Tests against a Chi-Squared distribution.                                            |
 * | `-majorOrder`                         | Specifies the data ordering: 'row' or 'column'.                                      |
 * | `-threads`                            | Number of threads for parallel computations.                                         |
 *
 * @subsection Incompatibilities
 *   - The `-test` option is incompatible with specifying multiple distributions (e.g., `-gaussian` and `-poisson`).
 *   - Only one of the following may be specified:
 *     - `-fileDistribution`
 *     - `-gaussian`
 *     - `-poisson`
 *     - `-student`
 *     - `-chisquared`
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
 * M. Borland, R. Soliday, Xuesong Jiao, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "SDDSutils.h"
#include "scan.h"

#if defined(_WIN32)
#  if _MSC_VER <= 1600
#    include "fdlibm.h"
#  endif
#endif

void compareToFileDistribution(char *output, long testCode, SDDS_DATASET *SDDSin, char **columnName, long columnNames, char *distFile, char *distFileIndep, char *distFileDepen);
void compareToDistribution(char *output, long testCode, SDDS_DATASET *SDDSin, char **columnName, char **sigmaName, long columnNames, long distCode, long degreesFree, char *dofParameter, short columnMajorOrder, int threads);
void ksTestWithFunction(double *data, int64_t rows, double (*CDF)(double x), double *statReturn, double *sigLevelReturn);
void chiSquaredTestWithFunction(double *data, int64_t rows, double (*PDF)(double x), double *statReturn, double *sigLevelReturn);

static char *USAGE =
  "sddsdistest [<input>] [<output>]\n"
  "            [-pipe=[in][,out]]\n"
  "             -column=<name>[,sigma=<name>]...\n"
  "             -exclude=<name>[,...]\n"
  "            [-degreesOfFreedom={<value>|@<parameterName>}]\n"
  "            [-test={ks|chisquared}]\n"
  "            [{\n"
  "              -fileDistribution=<filename>,<indepName>,<depenName> |\n"
  "              -gaussian |\n"
  "              -poisson |\n"
  "              -student |\n"
  "              -chisquared\n"
  "            }]\n"
  "            [-majorOrder=row|column]\n"
  "            [-threads=<number>]\n\n"
  "Description:\n"
  "  Tests data columns against specified statistical distributions using the\n"
  "  Kolmogorov-Smirnov or Chi-Squared tests.\n\n"
  "Options:\n"
  "  <input>                   Input SDDS file. If omitted, standard input is used.\n"
  "  <output>                  Output SDDS file. If omitted, standard output is used.\n"
  "  -pipe=[in][,out]          Use pipe for input and/or output.\n"
  "  -column=<name>[,sigma=<name>]...\n"
  "                            Specify one or more columns to test, optionally with\n"
  "                            corresponding sigma (error) columns.\n"
  "  -exclude=<name>[,...]     Exclude specified columns from testing.\n"
  "  -degreesOfFreedom={<value>|@<parameterName>}\n"
  "                            Specify degrees of freedom as a fixed value or reference\n"
  "                            a parameter in the input SDDS file.\n"
  "  -test={ks|chisquared}     Choose the statistical test to perform: 'ks' for\n"
  "                            Kolmogorov-Smirnov or 'chisquared' for Chi-Squared.\n"
  "  -fileDistribution=<filename>,<indepName>,<depenName>\n"
  "                            Use a user-defined distribution from a file.\n"
  "  -gaussian                 Test against a Gaussian distribution.\n"
  "  -poisson                  Test against a Poisson distribution.\n"
  "  -student                  Test against a Student's t distribution.\n"
  "  -chisquared               Test against a Chi-Squared distribution.\n"
  "  -majorOrder=row|column    Specify data ordering: 'row' for row-major or 'column'\n"
  "                            for column-major.\n"
  "  -threads=<number>         Number of threads to use for computations.\n\n"
  "Program Information:\n"
  "  Program by M. Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

/* Enumeration for option types */
enum option_type {
  CLO_PIPE,
  CLO_COLUMN,
  CLO_TEST,
  CLO_FILEDIST,
  CLO_GAUSSIAN,
  CLO_POISSON,
  CLO_STUDENT,
  CLO_CHISQUARED,
  CLO_DOF,
  CLO_EXCLUDE,
  CLO_MAJOR_ORDER,
  CLO_THREADS,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "pipe",
  "column",
  "test",
  "filedistribution",
  "gaussian",
  "poisson",
  "student",
  "chisquared",
  "degreesoffreedom",
  "exclude",
  "majorOrder",
  "threads"
};

#define KS_TEST 0
#define CHI_TEST 1
#define N_TESTS 2
static char *testChoice[N_TESTS] = {
  "ks",
  "chisquared",
};

int main(int argc, char **argv) {
  int iArg;
  unsigned long dummyFlags, pipeFlags, majorOrderFlag;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin;
  char *input = NULL, *output = NULL, *distFile = NULL, **columnName = NULL, **sigmaName = NULL, **excludeName = NULL, *distFileIndep = NULL, *distFileDepen = NULL;
  long testCode = 0, distCode = -1, code, degreesFree = -1, columnNames = 0, excludeNames = 0;
  char *dofParameter = NULL;
  short columnMajorOrder = -1;
  int threads = 1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3)
    bomb(NULL, USAGE);

  pipeFlags = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      code = match_string(scanned[iArg].list[0], option, N_OPTIONS, 0);
      switch (code) {
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
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax/values");
        break;
      case CLO_COLUMN:
        if ((scanned[iArg].n_items != 2 && scanned[iArg].n_items != 3) ||
            SDDS_StringIsBlank(scanned[iArg].list[1])) {
          SDDS_Bomb("invalid -column syntax/values");
        }
        moveToStringArray(&columnName, &columnNames, scanned[iArg].list + 1, 1);
        sigmaName = SDDS_Realloc(sigmaName, sizeof(*sigmaName) * columnNames);
        if (scanned[iArg].n_items == 3) {
          scanned[iArg].n_items -= 2;
          if (!scan_item_list(&dummyFlags, scanned[iArg].list + 2, &scanned[iArg].n_items, "sigma",
                              SDDS_STRING, sigmaName + columnNames - 1, 1, 1, NULL) ||
              dummyFlags != 1 ||
              SDDS_StringIsBlank(sigmaName[columnNames - 1])) {
            SDDS_Bomb("invalid -column syntax/values");
          }
        } else {
          sigmaName[columnNames - 1] = NULL;
        }
        break;
      case CLO_TEST:
        if (scanned[iArg].n_items != 2 ||
            (testCode = match_string(scanned[iArg].list[1], testChoice, N_TESTS, 0)) < 0) {
          SDDS_Bomb("invalid -test syntax/values");
        }
        break;
      case CLO_FILEDIST:
        if (scanned[iArg].n_items != 4) {
          SDDS_Bomb("too few qualifiers for -fileDistribution");
        }
        if (SDDS_StringIsBlank(distFile = scanned[iArg].list[1]) ||
            SDDS_StringIsBlank(distFileIndep = scanned[iArg].list[2]) ||
            SDDS_StringIsBlank(distFileDepen = scanned[iArg].list[3])) {
          SDDS_Bomb("invalid -fileDistribution values");
        }
        break;
      case CLO_GAUSSIAN:
      case CLO_POISSON:
      case CLO_STUDENT:
      case CLO_CHISQUARED:
        distCode = code;
        break;
      case CLO_DOF:
        if (scanned[iArg].n_items != 2) {
          SDDS_Bomb("too few qualifiers for -degreesOfFreedom");
        }
        if (scanned[iArg].list[1][0] == '@') {
          if (!SDDS_CopyString(&dofParameter, scanned[iArg].list[1] + 1)) {
            SDDS_Bomb("memory allocation failure");
          }
        } else if (sscanf(scanned[iArg].list[1], "%ld", &degreesFree) != 1 || degreesFree <= 1) {
          SDDS_Bomb("invalid degrees-of-freedom given for -student/-chiSquared");
        }
        break;
      case CLO_EXCLUDE:
        if (scanned[iArg].n_items < 2 || SDDS_StringIsBlank(scanned[iArg].list[1])) {
          SDDS_Bomb("invalid -exclude syntax/values");
        }
        moveToStringArray(&excludeName, &excludeNames, scanned[iArg].list + 1, scanned[iArg].n_items - 1);
        break;
      case CLO_THREADS:
        if (scanned[iArg].n_items != 2 ||
            !sscanf(scanned[iArg].list[1], "%d", &threads) ||
            threads < 1) {
          SDDS_Bomb("invalid -threads syntax");
        }
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s (%s)\n", scanned[iArg].list[0], argv[0]);
        exit(EXIT_FAILURE);
      }
    } else {
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  processFilenames("sddsdistest", &input, &output, pipeFlags, 0, NULL);

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!columnNames)
    SDDS_Bomb("-column option must be supplied");
  if (!(columnNames = expandColumnPairNames(&SDDSin, &columnName, &sigmaName, columnNames, excludeName, excludeNames, FIND_NUMERIC_TYPE, 0))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_Bomb("named columns nonexistent or nonnumerical");
  }
  if (dofParameter && SDDS_CheckParameter(&SDDSin, dofParameter, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY)
    SDDS_Bomb("degrees-of-freedom parameter not found");

  if (distFile)
    compareToFileDistribution(output, testCode, &SDDSin, columnName, columnNames, distFile, distFileIndep, distFileDepen);
  else
    compareToDistribution(output, testCode, &SDDSin, columnName, sigmaName, columnNames, distCode, degreesFree, dofParameter, columnMajorOrder, threads);

  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

void compareToFileDistribution(char *output, long testCode, SDDS_DATASET *SDDSin, char **columnName, long columnNames, char *distFile, char *distFileIndep, char *distFileDepen) {
  SDDS_DATASET SDDSdist;
  if (!SDDS_InitializeInput(&SDDSdist, distFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_CheckColumn(&SDDSdist, distFileIndep, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY ||
      SDDS_CheckColumn(&SDDSdist, distFileDepen, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY)
    exit(EXIT_FAILURE);
  if (!SDDS_Terminate(&SDDSdist)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  SDDS_Bomb("-fileDistribution option not implemented yet");
}

static double sampleMean, sampleStDev;
static int32_t DOF;

double gaussianPDF(double x) {
  double z;
  z = (x - sampleMean) / sampleStDev;
  return exp(-z * z / 2) / sqrt(PIx2);
}

double gaussianCDF(double x) {
  long zSign;
  double z;
  z = (x - sampleMean) / sampleStDev;
  zSign = 1;
  if (z < 0) {
    zSign = -1;
    z = -z;
  }
  return (1 + zSign * erf(z / SQRT2)) / 2.0;
}

#define POISSON_ACCURACY (1e-8)

double poissonPDF(double xd) {
  long x;

  if ((x = xd) < 0)
    return 0;
  return exp(-sampleMean + x * log(sampleMean) - lgamma(x + 1.0));
}

double poissonCDF(double xd) {
  double CDF, term, accuracy;
  long x, n;

  if (xd < 0)
    xd = 0;
  x = xd;
  xd = x;
  term = 1;
  CDF = 1;
  accuracy = POISSON_ACCURACY / exp(-sampleMean);
  for (n = 1; n <= x; n++) {
    term *= sampleMean / n;
    CDF += term;
    if (term < accuracy)
      break;
  }
  return CDF * exp(-sampleMean);
}

double studentPDF(double t) {
  return exp(-0.5 * (DOF + 1) * log(1 + t * t / DOF) + lgamma((DOF + 1.0) / 2.0) - lgamma(DOF / 2.0)) / sqrt(PI * DOF);
}

double studentCDF(double t) {
  if (t > 0)
    return 1 - betaInc(DOF / 2.0, 0.5, DOF / (DOF + t * t)) / 2;
  else
    return betaInc(DOF / 2.0, 0.5, DOF / (DOF + t * t)) / 2;
}

#define LOG2 0.693147180559945

double chiSquaredPDF(double x) {
  double chiSqr, DOFover2;

  if (x < 0)
    return 0;
  chiSqr = x * DOF / sampleMean;
  DOFover2 = DOF / 2.0;
  return exp((DOFover2 - 1.0) * log(chiSqr) - chiSqr / 2 - DOFover2 * LOG2 - lgamma(DOFover2));
}

double chiSquaredCDF(double x) {
  double chiSqr;

  if (x < 0)
    x = 0;
  chiSqr = x * DOF / sampleMean;
  return 1 - gammaQ(DOF / 2.0, chiSqr / 2.0);
}

void compareToDistribution(char *output, long testCode, SDDS_DATASET *SDDSin, char **columnName, char **sigmaName, long columnNames, long distCode, long degreesFree, char *dofParameter, short columnMajorOrder, int threads) {
  SDDS_DATASET SDDSout;
  double *data, *data1, stat, sigLevel;
  long iStat, iSigLevel, iCount, iColumnName;
  long icol;
  int64_t rows, row;
  iStat = iSigLevel = iCount = iColumnName = 0;
  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 0, NULL, "sddsdistest output", output) ||
      0 > SDDS_DefineParameter(&SDDSout, "distestDistribution", NULL, NULL, "sddsdistest distribution name", NULL,
                               SDDS_STRING, option[distCode]) ||
      0 > SDDS_DefineParameter(&SDDSout, "distestTest", NULL, NULL, "sddsdistest test name", NULL, SDDS_STRING, testChoice[testCode]) ||
      0 > (iCount = SDDS_DefineParameter(&SDDSout, "Count", NULL, NULL, "Number of data points", NULL, SDDS_LONG, 0))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin->layout.data_mode.column_major;
  switch (testCode) {
  case KS_TEST:
    if (0 > (iColumnName = SDDS_DefineColumn(&SDDSout, "ColumnName", NULL, NULL, "Column analysed by sddsdistest",
                                             NULL, SDDS_STRING, 0)) ||
        0 > (iStat = SDDS_DefineColumn(&SDDSout, "D", NULL, NULL, "Kolmogorov-Smirnov D statistic", NULL, SDDS_DOUBLE, 0)) ||
        0 > (iSigLevel = SDDS_DefineColumn(&SDDSout, "distestSigLevel", "P(D$ba$n>D)", NULL, "Probability of exceeding D", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    break;
  case CHI_TEST:
    if (0 > (iColumnName = SDDS_DefineColumn(&SDDSout, "ColumnName", NULL, NULL, "Column analysed by sddsdistest",
                                             NULL, SDDS_STRING, 0)) ||
        0 > (iStat = SDDS_DefineParameter(&SDDSout, "ChiSquared", "$gh$r$a2$n", NULL, "Chi-squared statistic", NULL,
                                          SDDS_DOUBLE, 0)) ||
        0 > (iSigLevel = SDDS_DefineParameter(&SDDSout, "distestSigLevel", "P($gh$r$a2$n$ba$n>$gh$r$a2$n)", NULL, "Probability of exceeding $gh$r$a2$n", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    break;
  default:
    SDDS_Bomb("Invalid testCode seen in compareToDistribution--this shouldn't happen.");
    break;
  }
  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  while (SDDS_ReadPage(SDDSin) > 0) {
    if (!SDDS_StartPage(&SDDSout, columnNames))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    rows = SDDS_CountRowsOfInterest(SDDSin);
    for (icol = 0; icol < columnNames; icol++) {
      stat = 0;
      sigLevel = 1;
      if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, columnName, columnNames, iColumnName) ||
          !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, iCount, rows, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (rows >= 2) {
        if (!(data = SDDS_GetColumnInDoubles(SDDSin, columnName[icol])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (dofParameter)
          SDDS_GetParameterAsLong(SDDSin, dofParameter, &DOF);
        else
          DOF = degreesFree;
        switch (distCode) {
        case CLO_GAUSSIAN:
          computeMomentsThreaded(&sampleMean, NULL, &sampleStDev, NULL, data, rows, threads);
          if (testCode == KS_TEST)
            ksTestWithFunction(data, rows, gaussianCDF, &stat, &sigLevel);
          else
            chiSquaredTestWithFunction(data, rows, gaussianPDF, &stat, &sigLevel);
          break;
        case CLO_POISSON:
          computeMomentsThreaded(&sampleMean, NULL, NULL, NULL, data, rows, threads);
          if (testCode == KS_TEST)
            ksTestWithFunction(data, rows, poissonCDF, &stat, &sigLevel);
          else
            chiSquaredTestWithFunction(data, rows, poissonPDF, &stat, &sigLevel);
          break;
        case CLO_STUDENT:
          if (DOF < 1)
            SDDS_Bomb("must have at least one degree of freedom for Student distribution tests");
          computeMomentsThreaded(&sampleMean, NULL, NULL, NULL, data, rows, threads);
          if (sigmaName && sigmaName[icol]) {
            if (!(data1 = SDDS_GetColumnInDoubles(SDDSin, sigmaName[icol])))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            for (row = 0; row < rows; row++)
              /* compute t = (x-mu)/sigma */
              data[row] = (data[row] - sampleMean) / data1[row];
            free(data1);
          } else {
            for (row = 0; row < rows; row++)
              data[row] -= sampleMean;
          }
          if (testCode == KS_TEST)
            ksTestWithFunction(data, rows, studentCDF, &stat, &sigLevel);
          else
            chiSquaredTestWithFunction(data, rows, studentPDF, &stat, &sigLevel);
          break;
        case CLO_CHISQUARED:
          computeMomentsThreaded(&sampleMean, NULL, NULL, NULL, data, rows, threads);
          if (DOF < 1)
            SDDS_Bomb("must have at least one degree of freedom for chi-squared distribution tests");
          if (testCode == KS_TEST)
            ksTestWithFunction(data, rows, chiSquaredCDF, &stat, &sigLevel);
          else
            chiSquaredTestWithFunction(data, rows, chiSquaredPDF, &stat, &sigLevel);
          break;
        default:
          SDDS_Bomb("Invalid distCode in compareToDistribution--this shouldn't happen");
          break;
        }
      }
      if (!SDDS_SetRowValues(&SDDSout, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, icol, iStat, stat, iSigLevel, sigLevel, -1))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
}

void chiSquaredTestWithFunction(double *data, int64_t rows, double (*PDF)(double x), double *statReturn, double *sigLevelReturn) {
  SDDS_Bomb("Chi-squared distribution test not implemented yet---wouldn't you really like a nice K-S test instead?");
}

void ksTestWithFunction(double *data, int64_t rows, double (*CDF)(double x), double *statReturn, double *sigLevelReturn) {
  double CDF1, CDF2, dCDF1, dCDF2, CDF0, dCDFmaximum;
  int64_t row;

  qsort((void *)data, rows, sizeof(*data), double_cmpasc);
  dCDFmaximum = CDF1 = 0;
  for (row = 0; row < rows; row++) {
    CDF0 = (*CDF)(data[row]);
    CDF2 = (row + 1.0) / rows;
    dCDF1 = CDF0 - CDF1;
    if (dCDF1 < 0)
      dCDF1 = -dCDF1;
    dCDF2 = CDF0 - CDF2;
    if (dCDF2 < 0)
      dCDF2 = -dCDF2;
    CDF1 = CDF2;
    if (dCDF1 > dCDFmaximum)
      dCDFmaximum = dCDF1;
    if (dCDF2 > dCDFmaximum)
      dCDFmaximum = dCDF2;
  }
  *statReturn = dCDFmaximum;
  *sigLevelReturn = KS_Qfunction(sqrt((double)rows) * dCDFmaximum);
}
