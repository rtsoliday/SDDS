/**
 * @file sddssampledist.c
 * @brief Generates sampled distributions based on input SDDS files or direct specifications.
 *
 * @details
 * This program generates samples from specified distributions (Gaussian, Uniform, Poisson)
 * or based on cumulative distribution functions (CDF/DF) provided in input SDDS files. It
 * supports various options to customize the sampling process, including the number of samples,
 * random seed, verbosity, and output ordering.
 *
 * @section Usage
 * ```
 * sddssampledist [<inputfile>] [<outputfile>] 
 *                [-pipe=[in][,out]]
 *                 -columns=independentVariable=<name>,{cdf=<CDFName> | df=<DFName>}[,output=<name>][,units=<string>][,factor=<value>][,offset=<value>][,datafile=<filename>][,haltonRadix=<primeNumber>[,haltonOffset=<integer>][,randomize[,group=<groupID>]]]
 *                [-columns=...] 
 *                 -samples=<integer>
 *                [-seed=<integer>] 
 *                [-verbose]
 *                [-gaussian=columnName=<columnName>[,meanValue=<value>|@<parameter_name>][,sigmaValue=<value>|@<parameter_name>][,units=<string>]]
 *                [-uniform=columnName=<columnName>[,minimumValue=<value>|@<parameter_name>][,maximumValue=<value>|@<parameter_name>][,units=<string>]]
 *                [-poisson=columnName=<columnName>[,meanValue=<value>|@<parameter_name>][,units=<string>]]
 *                [-optimalHalton]
 *                [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`                            | Defines independent variable and distribution function with customization options.    |
 * | `-samples`                            | Specifies the number of samples to generate.                                         |
 * 
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard input and/or output streams.                                            |
 * | `-seed`                               | Specifies the seed for the random number generator.                                  |
 * | `-verbose`                            | Enables verbose output.                                                              |
 * | `-gaussian`                           | Samples from a Gaussian distribution.                                                |
 * | `-uniform`                            | Samples from a Uniform distribution.                                                 |
 * | `-poisson`                            | Samples from a Poisson distribution.                                                 |
 * | `-majorOrder`                         | Specifies the output file order as row-major or column-major.                        |
 *
 * @subsection Incompatibilities
 *   - `-columns`
 *     - Requires at least one `independentVariable` and exactly one of `cdf` or `df`.
 *     - If `haltonRadix` is used, the value must be a prime number.
 *     - Randomization requires valid group identifiers if `group=<groupID>` is specified.
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
 * M. Borland, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include <time.h>

/* Enumeration for option types */
enum option_type {
  CLO_PIPE,
  CLO_COLUMNS,
  CLO_SAMPLES,
  CLO_SEED,
  CLO_VERBOSE,
  CLO_GAUSSIAN,
  CLO_UNIFORM,
  CLO_POISSON,
  CLO_OPTIMAL_HALTON,
  CLO_MAJOR_ORDER,
  CLO_OPTIONS
};

static char *option[CLO_OPTIONS] = {
  "pipe",
  "columns",
  "samples",
  "seed",
  "verbose",
  "gaussian",
  "uniform",
  "poisson",
  "optimalHalton",
  "majorOrder",
};

char *USAGE1 =
  "Usage: sddssampledist [<input>] [<output>] [-pipe=[in][,out]]\n"
  "       -columns=independentVariable=<name>,{cdf=<CDFName> | df=<DFName>}"
  "[,output=<name>][,units=<string>][,factor=<value>][,offset=<value>]"
  "[,datafile=<filename>][,haltonRadix=<primeNumber>[,haltonOffset=<integer>]"
  "[,randomize[,group=<groupID>]]]\n"
  "       [-columns=...] [-samples=<integer>] [-seed=<integer>] [-verbose]\n"
  "       [-gaussian=columnName=<columnName>[,meanValue=<value>|@<parameter_name>]"
  "[,sigmaValue=<value>|@<parameter_name>][,units=<string>]]\n"
  "       [-uniform=columnName=<columnName>[,minimumValue=<value>|@<parameter_name>]"
  "[,maximumValue=<value>|@<parameter_name>][,units=<string>]]\n"
  "       [-poisson=columnName=<columnName>[,meanValue=<value>|@<parameter_name>]"
  "[,units=<string>]] [-optimalHalton] [-majorOrder=row|column]\n";

char *USAGE2 =
  "Options:\n"
  "  -columns        Specifies the independent variable and its distribution.\n"
  "                  Usage:\n"
  "                    -columns=independentVariable=<name>,{cdf=<CDFName> | df=<DFName>}\n"
  "                    [,output=<name>][,units=<string>][,factor=<value>][,offset=<value>]\n"
  "                    [,datafile=<filename>][,haltonRadix=<primeNumber>]\n"
  "                    [,haltonOffset=<integer>][,randomize[,group=<groupID>]]\n"
  "                  Description:\n"
  "                    Defines the independent variable and its distribution function (CDF or DF).\n"
  "                    Additional qualifiers allow for customization of output names, units,\n"
  "                    scaling factors, offsets, data sources, Halton sequence parameters,\n"
  "                    and randomization groups.\n\n"
  "  -gaussian       Samples from a Gaussian distribution.\n"
  "                  Usage:\n"
  "                    -gaussian=columnName=<columnName>[,meanValue=<value>|@<parameter_name>]\n"
  "                              [,sigmaValue=<value>|@<parameter_name>][,units=<string>]\n"
  "                  Description:\n"
  "                    Generates Gaussian-distributed samples with specified mean and sigma.\n"
  "                    Parameters can be directly provided or referenced from input file parameters.\n\n";

char *USAGE3 =
  "  -uniform        Samples from a Uniform distribution.\n"
  "                  Usage:\n"
  "                    -uniform=columnName=<columnName>[,minimumValue=<value>|@<parameter_name>]\n"
  "                             [,maximumValue=<value>|@<parameter_name>][,units=<string>]\n"
  "                  Description:\n"
  "                    Generates uniformly distributed samples within specified minimum and maximum values.\n"
  "                    Parameters can be directly provided or referenced from input file parameters.\n\n"
  "  -poisson        Samples from a Poisson distribution.\n"
  "                  Usage:\n"
  "                    -poisson=columnName=<columnName>[,meanValue=<value>|@<parameter_name>]\n"
  "                             [,units=<string>]\n"
  "                  Description:\n"
  "                    Generates Poisson-distributed samples with a specified mean.\n"
  "                    The mean can be directly provided or referenced from an input file parameter.\n\n"
  "  -samples        Specifies the number of samples to generate.\n"
  "  -seed           Specifies the seed for the random number generator.\n"
  "                  If not provided or non-positive, the seed is derived from the system clock.\n"
  "  -optimalHalton  Uses an improved Halton sequence for generating random numbers.\n"
  "  -majorOrder     Specifies the output file order as row-major or column-major.\n"
  "                  Usage:\n"
  "                    -majorOrder=row|column\n"
  "  -verbose        Enables verbose output, printing information to stderr during execution.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define SEQ_DATAFILE 0x0001UL
#define SEQ_INDEPNAME 0x0002UL
#define SEQ_CDFNAME 0x0004UL
#define SEQ_DFNAME 0x0008UL
#define SEQ_OUTPUTNAME 0x0010UL
#define SEQ_HALTONRADIX 0x0020UL
#define SEQ_RANDOMIZE 0x0040UL
#define SEQ_RANDOMGROUP 0x0080UL
#define SEQ_UNITSGIVEN 0x0100UL
#define SEQ_HALTONOFFSET 0x0200UL
#define SEQ_DIRECT_GAUSSIAN 0x0400UL
#define SEQ_DIRECT_UNIFORM 0x0800UL
#define SEQ_DIRECT_POISSON 0x1000UL

typedef struct
{
  unsigned long flags;
  char *dataFileName, *indepName, *CDFName, *DFName, *outputName, *units, *meanPar, *sigmaPar, *minPar, *maxPar;
  SDDS_DATASET SDDSin;
  int32_t haltonRadix, randomizationGroup, haltonOffset;
  double factor, offset, mean, min, max, sigma;
} SEQ_REQUEST;

typedef struct
{
  long group;
  long *order;
} RANDOMIZED_ORDER;

long CreatePoissonDistributionTable(double **x, double **pos_CDF, double mean);

int main(int argc, char **argv) {
  int iArg;
  char *input, *output, *meanPar, *sigmaPar, *maxPar, *minPar;
  long i, mainInputOpened, haltonID = 0, requireInput = 0;
  unsigned long pipeFlags, majorOrderFlag;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout, *SDDSptr;
  long randomNumberSeed = 0;
  SEQ_REQUEST *seqRequest;
  long samples, seqRequests, randomizationGroups = 0;
  int64_t j, values;
  double *sample, *IVValue, *CDFValue;
  char msgBuffer[1000];
  RANDOMIZED_ORDER *randomizationData = NULL;
  long verbose, optimalHalton = 0;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s%s%s\n", USAGE1, USAGE2, USAGE3);
    return EXIT_FAILURE;
  }
  seqRequest = NULL;
  seqRequests = 0;
  output = input = NULL;
  pipeFlags = 0;
  samples = values = 0;
  sample = IVValue = CDFValue = NULL;
  verbose = 0;
  maxPar = minPar = meanPar = sigmaPar = NULL;

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, CLO_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_COLUMNS:
        if (scanned[iArg].n_items < 3)
          SDDS_Bomb("invalid -columns syntax");
        if (!(seqRequest = SDDS_Realloc(seqRequest, sizeof(*seqRequest) * (seqRequests + 1))))
          SDDS_Bomb("memory allocation failure");
        scanned[iArg].n_items -= 1;
        memset(seqRequest + seqRequests, 0, sizeof(*seqRequest));
        /* remove following pointer initialization because memset already initializes them */
        seqRequest[seqRequests].randomizationGroup = -1;
        seqRequest[seqRequests].factor = 1;
        seqRequest[seqRequests].offset = 0;
        if (!scanItemList(&seqRequest[seqRequests].flags,
                          scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "datafile", SDDS_STRING, &seqRequest[seqRequests].dataFileName, 1, SEQ_DATAFILE,
                          "independentvariable", SDDS_STRING, &seqRequest[seqRequests].indepName, 1, SEQ_INDEPNAME,
                          "cdf", SDDS_STRING, &seqRequest[seqRequests].CDFName, 1, SEQ_CDFNAME,
                          "df", SDDS_STRING, &seqRequest[seqRequests].DFName, 1, SEQ_DFNAME,
                          "output", SDDS_STRING, &seqRequest[seqRequests].outputName, 1, SEQ_OUTPUTNAME,
                          "units", SDDS_STRING, &seqRequest[seqRequests].units, 1, SEQ_UNITSGIVEN,
                          "haltonradix", SDDS_LONG, &seqRequest[seqRequests].haltonRadix, 1, SEQ_HALTONRADIX,
                          "haltonoffset", SDDS_LONG, &seqRequest[seqRequests].haltonOffset, 1, SEQ_HALTONOFFSET,
                          "randomize", -1, NULL, 0, SEQ_RANDOMIZE,
                          "group", SDDS_LONG, &seqRequest[seqRequests].randomizationGroup, 1, SEQ_RANDOMGROUP,
                          "factor", SDDS_DOUBLE, &seqRequest[seqRequests].factor, 1, 0,
                          "offset", SDDS_DOUBLE, &seqRequest[seqRequests].offset, 1, 0, NULL) ||
            bitsSet(seqRequest[seqRequests].flags & (SEQ_INDEPNAME + SEQ_CDFNAME + SEQ_DFNAME)) != 2)
          SDDS_Bomb("invalid -columns syntax");
        if (seqRequest[seqRequests].flags & SEQ_RANDOMGROUP && seqRequest[seqRequests].randomizationGroup <= 0)
          SDDS_Bomb("use a positive integer for the randomization group ID");
        if (seqRequest[seqRequests].flags & SEQ_CDFNAME && seqRequest[seqRequests].flags & SEQ_DFNAME)
          SDDS_Bomb("give df or cdf for -columns, not both");
        if (seqRequest[seqRequests].flags & SEQ_HALTONRADIX && !is_prime(seqRequest[seqRequests].haltonRadix))
          SDDS_Bomb("halton radix must be a prime number");
        seqRequests++;
        scanned[iArg].n_items += 1;
        break;
      case CLO_GAUSSIAN:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -gaussian syntax");
        if (!(seqRequest = SDDS_Realloc(seqRequest, sizeof(*seqRequest) * (seqRequests + 1))))
          SDDS_Bomb("memory allocation failure");
        memset(seqRequest + seqRequests, 0, sizeof(*seqRequest));
        scanned[iArg].n_items -= 1;
        seqRequest[seqRequests].randomizationGroup = -1;
        seqRequest[seqRequests].mean = 0;
        seqRequest[seqRequests].sigma = 1;
        if (!scanItemList(&seqRequest[seqRequests].flags,
                          scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "columnName", SDDS_STRING, &seqRequest[seqRequests].outputName, 1, SEQ_OUTPUTNAME,
                          "meanValue", SDDS_STRING, &meanPar, 1, 0,
                          "sigmaValue", SDDS_STRING, &sigmaPar, 1, 0,
                          "units", SDDS_STRING, &seqRequest[seqRequests].units, 1, SEQ_UNITSGIVEN, NULL))
          SDDS_Bomb("invalid -gaussian syntax");
        seqRequest[seqRequests].flags |= SEQ_DIRECT_GAUSSIAN;
        if (!(seqRequest[seqRequests].flags & SEQ_OUTPUTNAME) || !(seqRequest[seqRequests].outputName))
          SDDS_Bomb("columnName is not provided for gaussian distribution/");
        if (meanPar) {
          if (wild_match(meanPar, "@*"))
            SDDS_CopyString(&seqRequest[seqRequests].meanPar, meanPar + 1);
          else if (!get_double(&seqRequest[seqRequests].mean, meanPar))
            SDDS_Bomb("Invalid value given for mean value of -gaussian distribution.");
          free(meanPar);
          meanPar = NULL;
        }
        if (sigmaPar) {
          if (wild_match(sigmaPar, "@*"))
            SDDS_CopyString(&seqRequest[seqRequests].sigmaPar, sigmaPar + 1);
          else if (!get_double(&seqRequest[seqRequests].sigma, sigmaPar))
            SDDS_Bomb("Invalid value given for sigma value of -gaussian distribution.");
          free(sigmaPar);
          sigmaPar = NULL;
        }
        seqRequests++;
        scanned[iArg].n_items += 1;
        break;
      case CLO_UNIFORM:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -uniform syntax");
        if (!(seqRequest = SDDS_Realloc(seqRequest, sizeof(*seqRequest) * (seqRequests + 1))))
          SDDS_Bomb("memory allocation failure");
        memset(seqRequest + seqRequests, 0, sizeof(*seqRequest));
        scanned[iArg].n_items -= 1;
        memset(seqRequest + seqRequests, 0, sizeof(*seqRequest));
        seqRequest[seqRequests].randomizationGroup = -1;
        seqRequest[seqRequests].min = 0;
        seqRequest[seqRequests].max = 1;
        if (!scanItemList(&seqRequest[seqRequests].flags,
                          scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "columnName", SDDS_STRING, &seqRequest[seqRequests].outputName, 1, SEQ_OUTPUTNAME,
                          "minimumValue", SDDS_STRING, &minPar, 1, 0,
                          "maximumValue", SDDS_STRING, &maxPar, 1, 0,
                          "units", SDDS_STRING, &seqRequest[seqRequests].units, 1, SEQ_UNITSGIVEN, NULL))
          SDDS_Bomb("invalid -uniform syntax");
        seqRequest[seqRequests].flags |= SEQ_DIRECT_UNIFORM;
        if (!(seqRequest[seqRequests].flags & SEQ_OUTPUTNAME) ||
            !(seqRequest[seqRequests].outputName))
          SDDS_Bomb("columnName is not provided for uniform distribution/");
        if (minPar) {
          if (wild_match(minPar, "@*"))
            SDDS_CopyString(&seqRequest[seqRequests].minPar, minPar + 1);
          else if (!get_double(&seqRequest[seqRequests].min, minPar))
            SDDS_Bomb("Invalid value given for minimum value of -uniform distribution.");
          free(minPar);
          minPar = NULL;
        }
        if (maxPar) {
          if (wild_match(maxPar, "@*"))
            SDDS_CopyString(&seqRequest[seqRequests].maxPar, maxPar + 1);
          else if (!get_double(&seqRequest[seqRequests].max, maxPar))
            SDDS_Bomb("Invalid value given for maximum value of -uniform distribution.");
          free(maxPar);
          maxPar = NULL;
        }
        seqRequests++;
        scanned[iArg].n_items += 1;
        break;
      case CLO_POISSON:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -poisson syntax");
        if (!(seqRequest = SDDS_Realloc(seqRequest, sizeof(*seqRequest) * (seqRequests + 1))))
          SDDS_Bomb("memory allocation failure");
        memset(seqRequest + seqRequests, 0, sizeof(*seqRequest));
        scanned[iArg].n_items -= 1;
        memset(seqRequest + seqRequests, 0, sizeof(*seqRequest));
        seqRequest[seqRequests].randomizationGroup = -1;
        seqRequest[seqRequests].mean = 1;
        if (!scanItemList(&seqRequest[seqRequests].flags,
                          scanned[iArg].list + 1, &scanned[iArg].n_items, 0,
                          "columnName", SDDS_STRING, &seqRequest[seqRequests].outputName, 1, SEQ_OUTPUTNAME,
                          "meanValue", SDDS_STRING, &meanPar, 1, 0,
                          "units", SDDS_STRING, &seqRequest[seqRequests].units, 1, SEQ_UNITSGIVEN, NULL))
          SDDS_Bomb("invalid -poisson syntax");
        seqRequest[seqRequests].flags |= SEQ_DIRECT_POISSON;
        if (!(seqRequest[seqRequests].flags & SEQ_OUTPUTNAME) || !(seqRequest[seqRequests].outputName))
          SDDS_Bomb("columnName is not provided for poisson distribution/");
        if (meanPar) {
          if (wild_match(meanPar, "@*"))
            SDDS_CopyString(&seqRequest[seqRequests].meanPar, meanPar + 1);
          else if (!get_double(&seqRequest[seqRequests].mean, meanPar))
            SDDS_Bomb("Invalid value given for mean value of -poisson distribution.");
          free(meanPar);
          meanPar = NULL;
        }
        seqRequests++;
        scanned[iArg].n_items += 1;
        break;
      case CLO_SAMPLES:
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%ld", &samples) != 1 ||
            samples <= 0)
          SDDS_Bomb("invalid -samples syntax");
        break;
      case CLO_SEED:
        if (scanned[iArg].n_items != 2 ||
            sscanf(scanned[iArg].list[1], "%ld", &randomNumberSeed) != 1)
          SDDS_Bomb("invalid -seed syntax");
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_OPTIMAL_HALTON:
        optimalHalton = 1;
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous option: %s\n", scanned[iArg].list[0]);
        exit(EXIT_FAILURE);
        break;
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

  if (!seqRequests)
    SDDS_Bomb("give one or more -columns options");
  if (samples < 1)
    SDDS_Bomb("-samples option not given");

  for (i = 0; i < seqRequests; i++) {
    if (!(seqRequest[i].flags & (SEQ_DATAFILE | SEQ_DIRECT_GAUSSIAN | SEQ_DIRECT_UNIFORM | SEQ_DIRECT_POISSON)))
      break;
  }
  if (i == seqRequests) {
    /* all columns options have either their own input files or else use
     * one of the "direct" distributions.  Hence, we don't expect an input
     * file.
     */
    if (!input)
      pipeFlags |= USE_STDIN; /* not really, but fakes out processFilenames */
    if (input && !output) {
      output = input;
      input = NULL;
      pipeFlags |= USE_STDIN;
      if (fexists(output)) {
        sprintf(msgBuffer, "%s exists already (sddssampledist)", output);
        SDDS_Bomb(msgBuffer);
      }
    }
  }

  processFilenames("sddssampledist", &input, &output, pipeFlags, 0, NULL);

  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 0, NULL, NULL, output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (verbose)
    fprintf(stderr, "Initialized output file %s\n", output);

  /* open and check input files */
  for (i = mainInputOpened = 0; i < seqRequests; i++) {
    if (seqRequest[i].flags & SEQ_DIRECT_GAUSSIAN) {
      if (seqRequest[i].meanPar || seqRequest[i].sigmaPar) {
        if (!mainInputOpened) {
          if (!SDDS_InitializeInput(&SDDSin, input) ||
              !SDDS_TransferAllParameterDefinitions(&SDDSout, &SDDSin, 0))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          mainInputOpened = 1;
        }
        requireInput = 1;
        SDDSptr = &SDDSin;
        if ((seqRequest[i].meanPar &&
             SDDS_CheckParameter(SDDSptr, seqRequest[i].meanPar, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
            (seqRequest[i].sigmaPar &&
             SDDS_CheckParameter(SDDSptr, seqRequest[i].sigmaPar, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
      if (!SDDS_DefineSimpleColumn(&SDDSout, seqRequest[i].outputName, NULL, SDDS_DOUBLE))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (seqRequest[i].flags & SEQ_DIRECT_UNIFORM) {
      if (seqRequest[i].minPar || seqRequest[i].maxPar) {
        if (!mainInputOpened) {
          if (!SDDS_InitializeInput(&SDDSin, input) ||
              !SDDS_TransferAllParameterDefinitions(&SDDSout, &SDDSin, 0))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          mainInputOpened = 1;
        }
        requireInput = 1;
        SDDSptr = &SDDSin;
        if ((seqRequest[i].minPar &&
             SDDS_CheckParameter(SDDSptr, seqRequest[i].minPar, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
            (seqRequest[i].maxPar &&
             SDDS_CheckParameter(SDDSptr, seqRequest[i].maxPar, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
      if (!SDDS_DefineSimpleColumn(&SDDSout, seqRequest[i].outputName, NULL, SDDS_DOUBLE))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (seqRequest[i].flags & SEQ_DIRECT_POISSON) {
      if (seqRequest[i].meanPar) {
        if (!mainInputOpened) {
          if (!SDDS_InitializeInput(&SDDSin, input) ||
              !SDDS_TransferAllParameterDefinitions(&SDDSout, &SDDSin, 0))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          mainInputOpened = 1;
        }
        requireInput = 1;
        SDDSptr = &SDDSin;
        if (SDDS_CheckParameter(SDDSptr, seqRequest[i].meanPar, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
      if (!SDDS_DefineSimpleColumn(&SDDSout, seqRequest[i].outputName, NULL, SDDS_LONG))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else {
      if (seqRequest[i].flags & SEQ_RANDOMIZE) {
        long newGroupID = 0;
        /* define randomization groups */
        if (seqRequest[i].flags & SEQ_RANDOMGROUP) {
          newGroupID = seqRequest[i].randomizationGroup;
          for (j = 0; j < randomizationGroups; j++)
            if (randomizationData[j].group == newGroupID) {
              newGroupID = 0;
              break;
            }
        } else {
          seqRequest[i].randomizationGroup = newGroupID = -(i + 1);
        }
        if (newGroupID != 0) {
          if (!(randomizationData = SDDS_Realloc(randomizationData, sizeof(*randomizationData) * (randomizationGroups + 1))))
            SDDS_Bomb("memory allocation failure");
          randomizationData[randomizationGroups].group = newGroupID;
          randomizationData[randomizationGroups].order = NULL;
          randomizationGroups++;
        }
      }
      if (seqRequest[i].flags & SEQ_DATAFILE) {
        if (!SDDS_InitializeInput(&seqRequest[i].SDDSin, seqRequest[i].dataFileName))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        SDDSptr = &seqRequest[i].SDDSin;
      } else {
        if (!mainInputOpened) {
          if (!SDDS_InitializeInput(&SDDSin, input) ||
              !SDDS_TransferAllParameterDefinitions(&SDDSout, &SDDSin, 0))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          mainInputOpened = 1;
        }
        requireInput = 1;
        SDDSptr = &SDDSin;
      }
      if (SDDS_CheckColumn(SDDSptr, seqRequest[i].indepName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK ||
          ((seqRequest[i].flags & SEQ_CDFNAME) &&
           SDDS_CheckColumn(SDDSptr, seqRequest[i].CDFName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
          ((seqRequest[i].flags & SEQ_DFNAME) &&
           SDDS_CheckColumn(SDDSptr, seqRequest[i].DFName, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) ||
          !SDDS_TransferColumnDefinition(&SDDSout, SDDSptr, seqRequest[i].indepName, seqRequest[i].outputName)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }

    if (seqRequest[i].flags & SEQ_UNITSGIVEN &&
        !SDDS_ChangeColumnInformation(&SDDSout, "units", seqRequest[i].units, SDDS_SET_BY_NAME, seqRequest[i].outputName))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (verbose)
    fprintf(stderr, "Initialized input files\n");

  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = 0;

  if (!SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (randomNumberSeed <= 0) {
    randomNumberSeed = (long)time((time_t *)NULL);
    randomNumberSeed = 2 * (randomNumberSeed / 2) + 1;
#if defined(_WIN32) || defined(__APPLE__)
    random_1(-labs(randomNumberSeed));
#else
    random_1(-fabs(randomNumberSeed));
#endif
  } else
    random_1(-randomNumberSeed);

  if (!(sample = calloc(samples, sizeof(*sample))))
    SDDS_Bomb("memory allocation failure");
  while (1) {
    if (verbose)
      fprintf(stderr, "Beginning page loop\n");
    if (input && SDDS_ReadPage(&SDDSin) <= 0)
      break;
    for (i = 0; i < seqRequests; i++) {
      if (seqRequest[i].flags & SEQ_DATAFILE && SDDS_ReadPage(&seqRequest[i].SDDSin) <= 0)
        break;
    }
    if (i != seqRequests)
      break;
    if (!SDDS_StartPage(&SDDSout, samples) || (input && !SDDS_CopyParameters(&SDDSout, &SDDSin)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (verbose)
      fprintf(stderr, "Defining randomization tables\n");
    /* define randomization tables */
    for (i = 0; i < randomizationGroups; i++) {
      if (!(randomizationData[i].order = SDDS_Malloc(sizeof(*randomizationData[i].order) * samples)))
        SDDS_Bomb("memory allocation failure");
      for (j = 0; j < samples; j++)
        randomizationData[i].order[j] = j;
      randomizeOrder((char *)randomizationData[i].order, sizeof(*randomizationData[i].order), samples, 0, random_1);
    }
    if (verbose)
      fprintf(stderr, "Beginning loop over sequence requests\n");
    for (i = 0; i < seqRequests; i++) {
      if (verbose)
        fprintf(stderr, "Processing sequence request %ld\n", i);
      if (seqRequest[i].flags & SEQ_DIRECT_GAUSSIAN) {
        if ((seqRequest[i].meanPar &&
             !SDDS_GetParameterAsDouble(&SDDSin, seqRequest[i].meanPar, &seqRequest[i].mean)) ||
            (seqRequest[i].sigmaPar &&
             !SDDS_GetParameterAsDouble(&SDDSin, seqRequest[i].sigmaPar, &seqRequest[i].sigma)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (j = 0; j < samples; j++)
          sample[j] = gauss_rn_lim(seqRequest[i].mean, seqRequest[i].sigma, -1, random_1);
      } else if (seqRequest[i].flags & SEQ_DIRECT_UNIFORM) {
        if ((seqRequest[i].minPar &&
             !SDDS_GetParameterAsDouble(&SDDSin, seqRequest[i].minPar, &seqRequest[i].min)) ||
            (seqRequest[i].maxPar &&
             !SDDS_GetParameterAsDouble(&SDDSin, seqRequest[i].maxPar, &seqRequest[i].max)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (j = 0; j < samples; j++)
          sample[j] = seqRequest[i].min + (seqRequest[i].max - seqRequest[i].min) * random_1(1);
      } else if (seqRequest[i].flags & SEQ_DIRECT_POISSON) {
        double *pos_x, *pos_cdf, CDF;
        long pos_points, code;
        pos_x = pos_cdf = NULL;
        if ((seqRequest[i].meanPar &&
             !SDDS_GetParameterAsDouble(&SDDSin, seqRequest[i].meanPar, &seqRequest[i].mean)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        pos_points = CreatePoissonDistributionTable(&pos_x, &pos_cdf, seqRequest[i].mean);

        for (j = 0; j < samples; j++) {
          CDF = random_1(1);
          sample[j] = (int)(interp(pos_x, pos_cdf, pos_points, CDF, 0, 1, &code));
          /*  fprintf(stderr, "%ld, cdf=%f, sample=%f\n", j, CDF, sample[j]); */
        }
        free(pos_x);
        free(pos_cdf);
      } else {
        if (input && !(seqRequest[i].flags & SEQ_DATAFILE))
          SDDSptr = &SDDSin;
        else
          SDDSptr = &seqRequest[i].SDDSin;
        if ((values = SDDS_CountRowsOfInterest(SDDSptr))) {
          if (!(IVValue = SDDS_GetColumnInDoubles(SDDSptr, seqRequest[i].indepName)) ||
              (seqRequest[i].flags & SEQ_CDFNAME &&
               !(CDFValue = SDDS_GetColumnInDoubles(SDDSptr, seqRequest[i].CDFName))) ||
              (seqRequest[i].flags & SEQ_DFNAME &&
               !(CDFValue = SDDS_GetColumnInDoubles(SDDSptr, seqRequest[i].DFName))))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        } else {
          sprintf(msgBuffer, "empty page for file %s\n",
                  (seqRequest[i].flags & SEQ_DATAFILE) ? seqRequest[i].dataFileName : input);
          SDDS_Bomb(msgBuffer);
        }
        if (verbose)
          fprintf(stderr, "Checking and converting CDF/DF values\n");
        /* check/convert CDF/DF values */
        for (j = 1; j < values; j++) {
          if (IVValue[j - 1] > IVValue[j]) {
            sprintf(msgBuffer, "random variate values not monotonically increasing for %s",
                    (seqRequest[i].flags & SEQ_DATAFILE) ? seqRequest[i].dataFileName : input);
            SDDS_Bomb(msgBuffer);
          }
          if (seqRequest[i].flags & SEQ_DFNAME)
            /* convert DF to CDF */
            CDFValue[j] += CDFValue[j - 1];
          if (CDFValue[j] < CDFValue[j - 1]) {
            sprintf(msgBuffer, "CDF values decreasing for %s",
                    (seqRequest[i].flags & SEQ_DATAFILE) ? seqRequest[i].dataFileName : input);
            SDDS_Bomb(msgBuffer);
          }
        }
        if (verbose)
          fprintf(stderr, "Normalizing CDF\n");
        /* normalize the CDF */
        if (CDFValue[values - 1] <= 0) {
          sprintf(msgBuffer, "CDF not valid for %s\n", seqRequest[i].dataFileName);
          SDDS_Bomb(msgBuffer);
        }
        for (j = 0; j < values; j++)
          CDFValue[j] /= CDFValue[values - 1];
        if (seqRequest[i].flags & SEQ_HALTONRADIX) {
          if (verbose)
            fprintf(stderr, "Starting halton sequence, offset=%" PRId32 "\n", seqRequest[i].haltonOffset);
          if (!optimalHalton)
            haltonID = startHaltonSequence(&seqRequest[i].haltonRadix, 0.5);
          else
            haltonID = startModHaltonSequence(&seqRequest[i].haltonRadix, 0);
          while (seqRequest[i].haltonOffset-- > 0) {
            if (!optimalHalton)
              nextHaltonSequencePoint(haltonID);
            else
              nextModHaltonSequencePoint(haltonID);
          }
        }
        if (verbose)
          fprintf(stderr, "Generating samples\n");
        for (j = 0; j < samples; j++) {
          double CDF;
          long code;
          while (1) {
            if (seqRequest[i].flags & SEQ_HALTONRADIX) {
              if (!optimalHalton)
                CDF = nextHaltonSequencePoint(haltonID);
              else
                CDF = nextModHaltonSequencePoint(haltonID);
            } else
              CDF = random_1(1);
            if (CDF <= CDFValue[values - 1] && CDF >= CDFValue[0])
              break;
          }
          sample[j] = seqRequest[i].factor * interp(IVValue, CDFValue, values, CDF, 0, 1, &code) + seqRequest[i].offset;
        }
        if (seqRequest[i].flags & SEQ_RANDOMIZE) {
          long k, l;
          double *sample1;
          if (verbose)
            fprintf(stderr, "Randomizing order of values\n");
          if (!(sample1 = malloc(sizeof(*sample1) * samples)))
            SDDS_Bomb("memory allocation failure");
          for (l = 0; l < randomizationGroups; l++)
            if (randomizationData[l].group == seqRequest[i].randomizationGroup)
              break;
          if (l == randomizationGroups)
            SDDS_Bomb("problem with construction of randomization groups!");
          for (k = 0; k < samples; k++)
            sample1[k] = sample[randomizationData[l].order[k]];
          free(sample);
          sample = sample1;
        }
        free(IVValue);
        free(CDFValue);
      }
      if (verbose)
        fprintf(stderr, "Setting SDDS column values\n");
      if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, sample, samples,
                                     seqRequest[i].outputName ? seqRequest[i].outputName : seqRequest[i].indepName))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (verbose)
      fprintf(stderr, "Writing data page\n");
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!requireInput)
      break;
  }
  if (verbose)
    fprintf(stderr, "Exited read loop\n");
  free(sample);
  if ((input && !SDDS_Terminate(&SDDSin)) || !SDDS_Terminate(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < seqRequests; i++) {
    if (seqRequest[i].dataFileName)
      free(seqRequest[i].dataFileName);
    if (seqRequest[i].indepName)
      free(seqRequest[i].indepName);
    if (seqRequest[i].outputName)
      free(seqRequest[i].outputName);
    if (seqRequest[i].DFName)
      free(seqRequest[i].DFName);
    if (seqRequest[i].CDFName)
      free(seqRequest[i].CDFName);
    if (seqRequest[i].units)
      free(seqRequest[i].units);
    if (seqRequest[i].meanPar)
      free(seqRequest[i].meanPar);
    if (seqRequest[i].sigmaPar)
      free(seqRequest[i].sigmaPar);
    if (seqRequest[i].minPar)
      free(seqRequest[i].minPar);
    if (seqRequest[i].maxPar)
      free(seqRequest[i].maxPar);
    if (seqRequest[i].flags & SEQ_DATAFILE && !SDDS_Terminate(&(seqRequest[i].SDDSin)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  free(seqRequest);
  for (i = 0; i < randomizationGroups; i++)
    free(randomizationData[i].order);
  if (randomizationData)
    free(randomizationData);

  free_scanargs(&scanned, argc);

  return EXIT_SUCCESS;
}

long CreatePoissonDistributionTable(double **x, double **pos_CDF, double mean) {
  long i, npoints = 20, count = 0;
  double *pos = NULL;
  /* SDDS_DATASET pos_out; */

  *x = *pos_CDF = NULL;
  if (!(*x = malloc(sizeof(**x) * npoints)) ||
      !(pos = malloc(sizeof(*pos) * npoints)) ||
      !(*pos_CDF = malloc(sizeof(**pos_CDF) * npoints)))
    SDDS_Bomb("memory allocation failure.");
  i = count = 0;
  while (1) {
    if (count + 2 >= npoints) {
      npoints += 20;
      *x = SDDS_Realloc(*x, sizeof(**x) * npoints);
      *pos_CDF = SDDS_Realloc(*pos_CDF, sizeof(**pos_CDF) * npoints);
      pos = SDDS_Realloc(pos, sizeof(*pos) * npoints);
    }

    (*x)[count] = i;
    if (!i) {
      pos[i] = exp(-mean);
      (*pos_CDF)[count] = pos[i];
      count++;
    } else {
      pos[i] = pos[i - 1] * mean / i;
      (*pos_CDF)[count] = (*pos_CDF)[count - 1];
      (*pos_CDF)[count + 1] = (*pos_CDF)[count - 1] + pos[i];
      (*x)[count + 1] = i;
      if (1.0 - (*pos_CDF)[count + 1] <= 1.0e-15)
        break;
      count += 2;
    }
    i++;
  }
  /* fprintf(stderr,"lamda=%f\n", mean);
     if (!SDDS_InitializeOutput(&pos_out, SDDS_BINARY, 0, NULL, NULL, "pos_dist.sdds"))
     SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
     if (!SDDS_DefineSimpleColumn(&pos_out, "Count", NULL, SDDS_DOUBLE) ||
     !SDDS_DefineSimpleColumn(&pos_out, "P", NULL, SDDS_DOUBLE))
     SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
     if (!SDDS_SaveLayout(&pos_out) || !SDDS_WriteLayout(&pos_out))
     SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
     if (!SDDS_StartPage(&pos_out, count) ||
     !SDDS_SetColumnFromDoubles(&pos_out, SDDS_SET_BY_NAME, *x, count, "Count") ||
     !SDDS_SetColumnFromDoubles(&pos_out, SDDS_SET_BY_NAME, *pos_CDF, count, "P"))
     SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
     if (!SDDS_WritePage(&pos_out) || !SDDS_Terminate(&pos_out))
     SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
  */
  free(pos);
  return count;
}
