/**
 * @file sddsfft.c
 * @brief Performs FFT (Fast Fourier Transform) on SDDS-formatted data files.
 *
 * @details
 * This program processes SDDS data files, performing Fast Fourier Transforms (FFT) with extensive
 * options for data manipulation, including normalization, windowing, PSD computation, and inverse
 * transformations. It supports both complex and real input data and allows flexible output formats
 * based on user-defined parameters.
 *
 * @section Usage
 * ```
 * sddsfft [<inputfile>] [<outputfile>]
 *         [-pipe=[input][,output]]
 *         [-columns=<indep-variable>[,<depen-quantity>[,...]]]
 *         [-complexInput[=unfolded|folded]]
 *         [-exclude=<depen-quantity>[,...]]
 *         [-window[={hanning|welch|parzen|hamming|flattop|gaussian|none}[,correct]]]
 *         [-sampleInterval=<number>]
 *         [-normalize]
 *         [-fullOutput[=unfolded|folded],unwrapLimit=<value>]
 *         [-psdOutput[=plain][,{integrated|rintegrated[=<cutoff>]}]]
 *         [-inverse]
 *         [-padwithzeroes[=exponent] | -truncate]
 *         [-suppressaverage]
 *         [-noWarnings]
 *         [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required           | Description                                                                |
 * |--------------------|----------------------------------------------------------------------------|
 * | `-columns`           | Specify the independent variable and dependent quantities for FFT analysis. |
 *
 * | Optional                                | Description                                                                 |
 * |---------------------------------------|-----------------------------------------------------------------------------|
 * | `-pipe`              | Utilize the standard SDDS Toolkit pipe option for input and/or output.      |
 * | `-complexInput`      | Specify complex input column handling.                                      |
 * | `-exclude`           | List of wildcard patterns to exclude specific quantities from analysis.     |
 * | `-window`            | Apply a windowing function before analysis.                                 |
 * | `-sampleInterval`    | Define the interval for sampling input data points.                         |
 * | `-normalize`         | Normalize output to have a peak magnitude of 1.                             |
 * | `-fullOutput`        | Output the real and imaginary parts of the FFT.                             |
 * | `-psdOutput`         | Output Power Spectral Density (PSD) in various formats.                     |
 * | `-inverse`           | Perform an inverse Fourier transform.                                       |
 * | `-padwithzeroes`     | Pad data with zeroes for FFT optimization.                                  |
 * | `-truncate`          | Truncate data to nearest product of small primes for efficiency.            |
 * | `-suppressaverage`   | Remove average value from data before FFT.                                  |
 * | `-noWarnings`        | Suppress warning messages.                                                  |
 * | `-majorOrder`        | Specify row or column major order for output.                               |
 *
 * @subsection Incompatibilities
 *   - `-truncate` is incompatible with:
 *     - `-padwithzeroes`
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
 *   - M. Borland
 *   - C. Saunders
 *   - R. Soliday
 *   - L. Emery
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "fftpackC.h"
#include "SDDSutils.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_WINDOW,
  SET_NORMALIZE,
  SET_PADWITHZEROES,
  SET_TRUNCATE,
  SET_SUPPRESSAVERAGE,
  SET_SAMPLEINTERVAL,
  SET_COLUMNS,
  SET_FULLOUTPUT,
  SET_PIPE,
  SET_PSDOUTPUT,
  SET_EXCLUDE,
  SET_NOWARNINGS,
  SET_COMPLEXINPUT,
  SET_INVERSE,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "window",
  "normalize",
  "padwithzeroes",
  "truncate",
  "suppressaverage",
  "sampleinterval",
  "columns",
  "fulloutput",
  "pipe",
  "psdoutput",
  "exclude",
  "nowarnings",
  "complexinput",
  "inverse",
  "majorOrder",
};

#define FL_TRUNCATE 0x0001
#define FL_PADWITHZEROES 0x0002
#define FL_NORMALIZE 0x0004
#define FL_SUPPRESSAVERAGE 0x0008
#define FL_FULLOUTPUT 0x0010
#define FL_MAKEFREQDATA 0x0020
#define FL_PSDOUTPUT 0x0040
#define FL_PSDINTEGOUTPUT 0x0080
#define FL_PSDRINTEGOUTPUT 0x0100
#define FL_FULLOUTPUT_FOLDED 0x0200
#define FL_FULLOUTPUT_UNFOLDED 0x0400
#define FL_COMPLEXINPUT_FOLDED 0x0800
#define FL_COMPLEXINPUT_UNFOLDED 0x1000
#define FL_UNWRAP_PHASE 0x2000

#define WINDOW_HANNING 0
#define WINDOW_WELCH 1
#define WINDOW_PARZEN 2
#define WINDOW_HAMMING 3
#define WINDOW_FLATTOP 4
#define WINDOW_GAUSSIAN 5
#define WINDOW_NONE 6
#define N_WINDOW_TYPES 7
char *window_type[N_WINDOW_TYPES] = {
  "hanning", "welch", "parzen", "hamming", "flattop", "gaussian", "none"};

/* Improved and more readable usage message */
static char *USAGE1 =
  "Usage:\n"
  "  sddsfft [<inputfile>] [<outputfile>]\n"
  "          [-pipe=[input][,output]]\n"
  "          [-columns=<indep-variable>[,<depen-quantity>[,...]]]\n"
  "          [-complexInput[=unfolded|folded]]\n"
  "          [-exclude=<depen-quantity>[,...]]\n"
  "          [-window[={hanning|welch|parzen|hamming|flattop|gaussian|none}[,correct]]]\n"
  "          [-sampleInterval=<number>]\n"
  "          [-normalize]\n"
  "          [-fullOutput[=unfolded|folded],unwrapLimit=<value>]\n"
  "          [-psdOutput[=plain][,{integrated|rintegrated[=<cutoff>]}]]\n"
  "          [-inverse]\n"
  "          [-padwithzeroes[=exponent] | -truncate]\n"
  "          [-suppressaverage]\n"
  "          [-noWarnings]\n"
  "          [-majorOrder=row|column]\n\n";

static char *USAGE2 =
  "Options:\n"
  "  -pipe\n"
  "        Utilize the standard SDDS Toolkit pipe option for input and/or output.\n\n"
  "  -columns\n"
  "        Specify the independent variable and dependent quantities to Fourier analyze.\n"
  "        <depen-quantity> entries may include wildcards.\n\n"
  "  -complexInput\n"
  "        Indicate that input columns are complex, with names prefixed by Real and Imag.\n"
  "        Options:\n"
  "          folded   (default): Input frequency space is folded.\n"
  "          unfolded : Input frequency space is unfolded and must include negative frequencies.\n"
  "        If omitted, the program checks the SpectrumFolded parameter in the input file.\n\n"
  "  -exclude\n"
  "        Provide a list of wildcard patterns to exclude specific quantities from analysis.\n\n"
  "  -window\n"
  "        Apply a windowing function to the data before analysis.\n"
  "        Available types:\n"
  "          hanning, welch, parzen, hamming, flattop, gaussian, none\n"
  "        Adding ',correct' applies a correction factor to preserve the integrated PSD.\n"
  "        Default: hanning.\n\n"
  "  -sampleInterval\n"
  "        Sample the input data points at the specified interval.\n\n"
  "  -normalize\n"
  "        Normalize the output to have a peak magnitude of 1.\n\n"
  "  -fullOutput\n"
  "        Output the real and imaginary parts of the FFT.\n"
  "        Options:\n"
  "          folded   (default): Outputs the half FFT spectrum.\n"
  "          unfolded : Outputs the full FFT spectrum.\n"
  "        Adding ',unwrapLimit=<value>' computes and outputs the unwrapped phase where the relative magnitude exceeds the limit.\n\n"
  "  -psdOutput\n"
  "        Output the Power Spectral Density (PSD).\n"
  "        Options:\n"
  "          plain          : Outputs the standard PSD.\n"
  "          integrated     : Outputs the integrated PSD.\n"
  "          rintegrated=<cutoff> : Outputs the reverse-integrated PSD with an optional cutoff frequency.\n"
  "        Multiple options can be combined using commas.\n\n"
  "  -inverse\n"
  "        Perform an inverse Fourier transform. The output will always be an unfolded spectrum.\n"
  "        If combined with -fullOutput=folded, it overrides to -fullOutput=unfolded.\n\n"
  "  -padwithzeroes\n"
  "        Pad data with zeroes to optimize FFT performance.\n"
  "        Optionally specify an exponent to determine the padding size as 2^(original points * exponent).\n"
  "  -truncate\n"
  "        Truncate data to the nearest product of small prime numbers to reduce runtime.\n"
  "        Note: Only one of -padwithzeroes or -truncate can be used.\n\n"
  "  -suppressaverage\n"
  "        Remove the average value from the data before performing the FFT.\n\n"
  "  -noWarnings\n"
  "        Suppress all warning messages.\n\n"
  "  -majorOrder\n"
  "        Specify the output file's data order:\n"
  "          row     : Row-major order.\n"
  "          column  : Column-major order.\n\n"
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

/* Rest of the code remains unchanged */

/* ... [The rest of your code continues here without changes] ... */

int64_t greatestProductOfSmallPrimes(int64_t rows);
long process_data(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, double *tdata, int64_t rows, int64_t rowsToUse,
                  char *depenQuantity, char *depenQuantity2, unsigned long flags, long windowType,
                  int64_t sampleInterval, long correctWindowEffects, long inverse, double rintegCutOffFreq, double unwrapLimit);
long create_fft_frequency_column(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *timeName, char *freqUnits, long inverse);

long create_fft_columns(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *origName, char *indepName,
                        char *freqUnits, long full_output, unsigned long psd_output, long complexInput, long inverse, long unwrap_phase);
long create_fft_parameters(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *indepName, char *freqUnits);
char *makeFrequencyUnits(SDDS_DATASET *SDDSin, char *indepName);
long expandComplexColumnPairNames(SDDS_DATASET *SDDSin, char **name, char ***realName, char ***imagName,
                                  long names, char **excludeName, long excludeNames, long typeMode, long typeValue);

int main(int argc, char **argv) {
  int iArg;
  char *freqUnits;
  char *indepQuantity, **depenQuantity, **exclude, **realQuan = NULL, **imagQuan = NULL;
  long depenQuantities, excludes;
  char *input, *output;
  long i, readCode, noWarnings, complexInput, inverse, spectrumFoldParExist = 0;
  int64_t rows, rowsToUse, sampleInterval;
  int32_t spectrumFolded = 0, page = 0;
  unsigned long flags, pipeFlags, complexInputFlags = 0, fullOutputFlags = 0, majorOrderFlag;
  long windowType = -1;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  double *tdata, rintegCutOffFreq, unwrapLimit = 0;
  long padFactor, correctWindowEffects = 0;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3 || argc > (3 + N_OPTIONS)) {
    fprintf(stderr, "%s%s", USAGE1, USAGE2);
    exit(EXIT_FAILURE);
    /* bomb(NULL, USAGE); */
  }
  rintegCutOffFreq = 0;
  output = input = NULL;
  flags = pipeFlags = excludes = complexInput = inverse = 0;
  sampleInterval = 1;
  indepQuantity = NULL;
  depenQuantity = exclude = NULL;
  depenQuantities = 0;
  noWarnings = 0;
  padFactor = 0;
  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case SET_NORMALIZE:
        flags |= FL_NORMALIZE;
        break;
      case SET_WINDOW:
        if (scanned[iArg].n_items != 1) {
          if ((i = match_string(scanned[iArg].list[1], window_type, N_WINDOW_TYPES, 0)) < 0)
            SDDS_Bomb("unknown window type");
          windowType = i;
          if (scanned[iArg].n_items > 2) {
            if (strncmp(scanned[iArg].list[2], "correct", strlen(scanned[iArg].list[2])) == 0)
              correctWindowEffects = 1;
            else
              SDDS_Bomb("invalid -window syntax");
          }
        } else
          windowType = 0;
        break;
      case SET_PADWITHZEROES:
        flags |= FL_PADWITHZEROES;
        if (scanned[iArg].n_items != 1) {
          if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%ld", &padFactor) != 1 || padFactor < 1)
            SDDS_Bomb("invalid -padwithzeroes syntax");
        }
        break;
      case SET_TRUNCATE:
        flags |= FL_TRUNCATE;
        break;
      case SET_SUPPRESSAVERAGE:
        flags |= FL_SUPPRESSAVERAGE;
        break;
      case SET_SAMPLEINTERVAL:
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%" SCNd64, &sampleInterval) != 1 || sampleInterval <= 0)
          SDDS_Bomb("invalid -sampleinterval syntax");
        break;
      case SET_COLUMNS:
        if (indepQuantity)
          SDDS_Bomb("only one -columns option may be given");
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -columns syntax");
        indepQuantity = scanned[iArg].list[1];
        if (scanned[iArg].n_items >= 2) {
          depenQuantity = tmalloc(sizeof(*depenQuantity) * (depenQuantities = scanned[iArg].n_items - 2));
          for (i = 0; i < depenQuantities; i++)
            depenQuantity[i] = scanned[iArg].list[i + 2];
        }
        break;
      case SET_FULLOUTPUT:
        flags |= FL_FULLOUTPUT;
        if (scanned[iArg].n_items >= 2) {
          scanned[iArg].n_items--;
          if (!scanItemList(&fullOutputFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "folded", -1, NULL, 0, FL_FULLOUTPUT_FOLDED, "unfolded", -1, NULL, 0, FL_FULLOUTPUT_UNFOLDED, "unwrapLimit", SDDS_DOUBLE, &unwrapLimit, 0, FL_UNWRAP_PHASE, NULL))
            SDDS_Bomb("Invalid -fullOutput syntax");
          scanned[iArg].n_items++;
          if (fullOutputFlags & FL_FULLOUTPUT_UNFOLDED)
            flags |= FL_FULLOUTPUT_UNFOLDED;
          else
            flags |= FL_FULLOUTPUT_FOLDED;
          if (fullOutputFlags & FL_UNWRAP_PHASE)
            flags |= FL_UNWRAP_PHASE;
        }
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_PSDOUTPUT:
        if (scanned[iArg].n_items -= 1) {
          unsigned long tmpFlags;
          if (strchr(scanned[iArg].list[1], '=') <= 0) {
            if (!scanItemList(&tmpFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "integrated", -1, NULL, 0, FL_PSDINTEGOUTPUT, "rintegrated", -1, NULL, 0, FL_PSDRINTEGOUTPUT, "plain", -1, NULL, 0, FL_PSDOUTPUT, NULL))
              SDDS_Bomb("invalid -psdOutput syntax");
          } else {
            if (!scanItemList(&tmpFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "integrated", -1, NULL, 0, FL_PSDINTEGOUTPUT, "rintegrated", SDDS_DOUBLE, &rintegCutOffFreq, 0, FL_PSDRINTEGOUTPUT, "plain", -1, NULL, 0, FL_PSDOUTPUT, NULL))
              SDDS_Bomb("invalid -psdOutput syntax");
          }
          flags |= tmpFlags;
        } else
          flags |= FL_PSDOUTPUT;
        if (flags & FL_PSDINTEGOUTPUT && flags & FL_PSDRINTEGOUTPUT)
          SDDS_Bomb("invalid -psdOutput syntax: give only one of integrated or rintegrated");
        break;
      case SET_EXCLUDE:
        if (scanned[iArg].n_items < 2)
          SDDS_Bomb("invalid -exclude syntax");
        moveToStringArray(&exclude, &excludes, scanned[iArg].list + 1, scanned[iArg].n_items - 1);
        break;
      case SET_NOWARNINGS:
        noWarnings = 1;
        break;
      case SET_COMPLEXINPUT:
        complexInput = 1;
        if (scanned[iArg].n_items == 2) {
          scanned[iArg].n_items--;
          if (!scanItemList(&complexInputFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "folded", -1, NULL, 0, FL_COMPLEXINPUT_FOLDED, "unfolded", -1, NULL, 0, FL_COMPLEXINPUT_UNFOLDED, NULL))
            SDDS_Bomb("Invalid -complexInput syntax");
          scanned[iArg].n_items++;
        }
        break;
      case SET_INVERSE:
        inverse = 1;
        break;
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[iArg].n_items--;
        if (scanned[iArg].n_items > 0 && (!scanItemList(&majorOrderFlag, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
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
  if (!complexInput) {
    if (!noWarnings && inverse)
      fprintf(stderr, "Warning: the inverse option is ignored since it only works with -complexInput.\n");
    inverse = 0;
  }
  if (!noWarnings && inverse && flags & FL_FULLOUTPUT_FOLDED)
    fprintf(stderr, "Warning: the -inverse -fullOutput=folded will be changed to -inverse -fullOutput=unfolded.\n");

  processFilenames("sddsfft", &input, &output, pipeFlags, 0, NULL);

  if (!indepQuantity)
    SDDS_Bomb("Supply the independent quantity name with the -columns option.");

  if (flags & FL_TRUNCATE && flags & FL_PADWITHZEROES)
    SDDS_Bomb("Specify only one of -padwithzeroes and -truncate.");

  if (!SDDS_InitializeInput(&SDDSin, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_CheckColumn(&SDDSin, indepQuantity, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY)
    exit(EXIT_FAILURE);

  excludes = appendToStringArray(&exclude, excludes, indepQuantity);
  if (!depenQuantities)
    depenQuantities = appendToStringArray(&depenQuantity, depenQuantities, "*");

  if (!complexInput) {
    if ((depenQuantities = expandColumnPairNames(&SDDSin, &depenQuantity, NULL, depenQuantities, exclude, excludes, FIND_NUMERIC_TYPE, 0)) <= 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_Bomb("No quantities selected to FFT.");
    }
  } else {
    if ((depenQuantities = expandComplexColumnPairNames(&SDDSin, depenQuantity, &realQuan, &imagQuan, depenQuantities, exclude, excludes, FIND_NUMERIC_TYPE, 0)) <= 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_Bomb("No quantities selected to FFT.");
    }
  }

#if 0
  fprintf(stderr, "%ld dependent quantities:\n", depenQuantities);
  for (i = 0; i < depenQuantities; i++)
    fprintf(stderr, "  %s\n", depenQuantity[i]);
#endif

  if (!(freqUnits = makeFrequencyUnits(&SDDSin, indepQuantity)) ||
      !SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 0, NULL, "sddsfft output", output) ||
      !create_fft_frequency_column(&SDDSout, &SDDSin, indepQuantity, freqUnits, inverse) ||
      SDDS_DefineParameter(&SDDSout, "fftFrequencies", NULL, NULL, NULL, NULL, SDDS_LONG, NULL) < 0 ||
      SDDS_DefineParameter(&SDDSout, "fftFrequencySpacing", "$gD$rf", freqUnits, NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;

  if (flags & FL_FULLOUTPUT && SDDS_DefineParameter(&SDDSout, "SpectrumFolded", NULL, NULL, NULL, NULL, SDDS_LONG, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (complexInput) {
    if (!complexInputFlags) {
      if (SDDS_CheckParameter(&SDDSin, "SpectrumFolded", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK)
        spectrumFoldParExist = 1;
    } else if (complexInputFlags & FL_COMPLEXINPUT_UNFOLDED)
      flags |= FL_COMPLEXINPUT_UNFOLDED;
    else
      flags |= FL_COMPLEXINPUT_FOLDED;
  }
  for (i = 0; i < depenQuantities; i++) {
    if (!complexInput)
      create_fft_columns(&SDDSout, &SDDSin, depenQuantity[i], indepQuantity, freqUnits,
                         flags & FL_FULLOUTPUT,
                         flags & (FL_PSDOUTPUT + FL_PSDINTEGOUTPUT + FL_PSDRINTEGOUTPUT),
                         0, inverse, flags & FL_UNWRAP_PHASE);
    else
      create_fft_columns(&SDDSout, &SDDSin, realQuan[i], indepQuantity, freqUnits,
                         flags & FL_FULLOUTPUT,
                         flags & (FL_PSDOUTPUT + FL_PSDINTEGOUTPUT + FL_PSDRINTEGOUTPUT),
                         1, inverse, flags & FL_UNWRAP_PHASE);
  }

  if (!SDDS_TransferAllParameterDefinitions(&SDDSout, &SDDSin, SDDS_TRANSFER_KEEPOLD) ||
      !SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  while ((readCode = SDDS_ReadPage(&SDDSin)) > 0) {
    page++;
    if ((rows = SDDS_CountRowsOfInterest(&SDDSin)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (page == 1 && spectrumFoldParExist) {
      if (!SDDS_GetParameterAsLong(&SDDSin, "SpectrumFolded", &spectrumFolded))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (spectrumFolded)
        flags |= FL_COMPLEXINPUT_FOLDED;
      else
        flags |= FL_COMPLEXINPUT_UNFOLDED;
    }
    if (rows) {
      int64_t primeRows, pow2Rows;
      rowsToUse = rows;
      primeRows = greatestProductOfSmallPrimes(rows);
      if (rows != primeRows || padFactor) {
        if (flags & FL_PADWITHZEROES) {
          pow2Rows = ipow(2., ((int64_t)(log((double)rows) / log(2.0F))) + (padFactor ? padFactor : 1.0));
          if ((primeRows = greatestProductOfSmallPrimes(pow2Rows)) > rows)
            rowsToUse = primeRows;
          else
            rowsToUse = pow2Rows;
        } else if (flags & FL_TRUNCATE)
          rowsToUse = greatestProductOfSmallPrimes(rows);
        else if (largest_prime_factor(rows) > 1000 && !noWarnings)
          fputs("Warning: number of points has large prime factors.\nThis could take a very long time.\nConsider using the -truncate option.\n", stderr);
      }
      if (!SDDS_StartPage(&SDDSout, 2 * rowsToUse + 2) || !SDDS_CopyParameters(&SDDSout, &SDDSin))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(tdata = SDDS_GetColumnInDoubles(&SDDSin, indepQuantity)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i = 0; i < depenQuantities; i++)
        if (!process_data(&SDDSout, &SDDSin, tdata, rows, rowsToUse,
                          complexInput ? realQuan[i] : depenQuantity[i],
                          complexInput ? imagQuan[i] : NULL,
                          flags | (i == 0 ? FL_MAKEFREQDATA : 0),
                          windowType, sampleInterval, correctWindowEffects, inverse,
                          rintegCutOffFreq, unwrapLimit)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      free(tdata);
    } else {
      if (!SDDS_StartPage(&SDDSout, 0) || !SDDS_CopyParameters(&SDDSout, &SDDSin))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_WritePage(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (!SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}

static long psdOffset, argOffset, realOffset, imagOffset, fftOffset = -1, psdIntOffset, psdIntPowerOffset, unwrappedArgOffset = -1;

long process_data(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, double *tdata, int64_t rows,
                  int64_t rowsToUse, char *depenQuantity, char *imagQuantity, unsigned long flags, long windowType,
                  int64_t sampleInterval, long correctWindowEffects, long inverse, double rintegCutOffFreq, double unwrapLimit) {
  long offset, index, unfold = 0;
  int64_t n_freq, i, fftrows = 0;
  double r, r1, r2, length, factor, df, min, max, delta;
  double *real, *imag, *magData, *arg = NULL, *real_imag, *data, *psd = NULL, *psdInteg = NULL, *psdIntegPower = NULL, *unwrapArg = NULL, phase_correction = 0;
  double dtf_real, dtf_imag, t0;
  char s[256];
  double *fdata, *imagData = NULL;
  double *tDataStore = NULL;
  double windowCorrectionFactor = 0;

  if (!(data = SDDS_GetColumnInDoubles(SDDSin, depenQuantity)))
    return 0;
  if (imagQuantity && !(imagData = SDDS_GetColumnInDoubles(SDDSin, imagQuantity)))
    return 0;
  if (flags & FL_SUPPRESSAVERAGE) {
    compute_average(&r, data, rows);
    for (i = 0; i < rows; i++)
      data[i] -= r;
    if (imagData) {
      compute_average(&r, imagData, rows);
      for (i = 0; i < rows; i++)
        imagData[i] -= r;
    }
  }
  if (rows < rowsToUse) {
    /* pad with zeroes */
    tDataStore = tmalloc(sizeof(*tDataStore) * rowsToUse);
    memcpy((char *)tDataStore, (char *)tdata, rows * sizeof(*tdata));
    if (!(data = SDDS_Realloc(data, sizeof(*data) * rowsToUse)))
      SDDS_Bomb("memory allocation failure");
    if (imagData && !(imagData = SDDS_Realloc(imagData, sizeof(*imagData) * rowsToUse)))
      length = ((double)rows) * (tdata[rows - 1] - tdata[0]) / ((double)rows - 1.0);
    else
      length = tdata[rows - 1] - tdata[0];
    for (i = rows; i < rowsToUse; i++) {
      tDataStore[i] = tDataStore[i - 1] + length / ((double)rows - 1);
      data[i] = 0;
    }
    if (imagData)
      for (i = rows; i < rowsToUse; i++)
        imagData[i] = 0;
    tdata = tDataStore;
  }
  rows = rowsToUse; /* results in truncation if rows>rowsToUse */
  windowCorrectionFactor = 0;
  switch (windowType) {
  case WINDOW_HANNING:
    r = PIx2 / (rows - 1);
    for (i = 0; i < rows; i++) {
      factor = (1 - cos(i * r)) / 2;
      data[i] *= factor;
      windowCorrectionFactor += sqr(factor);
      if (imagData)
        imagData[i] *= factor;
    }
    break;
  case WINDOW_HAMMING:
    r = PIx2 / (rows - 1);
    for (i = 0; i < rows; i++) {
      factor = 0.54 - 0.46 * cos(i * r);
      data[i] *= factor;
      windowCorrectionFactor += sqr(factor);
      if (imagData)
        imagData[i] *= factor;
    }
    if (imagData)
      for (i = 0; i < rows; i++)
        imagData[i] *= (1 - cos(i * r)) / 2;
    break;
  case WINDOW_WELCH:
    r1 = (rows - 1) / 2.0;
    r2 = sqr((rows + 1) / 2.0);
    for (i = 0; i < rows; i++) {
      factor = 1 - sqr(i - r1) / r2;
      data[i] *= factor;
      windowCorrectionFactor += sqr(factor);
      if (imagData)
        imagData[i] *= factor;
    }
    break;
  case WINDOW_PARZEN:
    r = (rows - 1) / 2.0;
    for (i = 0; i < rows; i++) {
      factor = 1 - FABS((i - r) / r);
      data[i] *= factor;
      windowCorrectionFactor += sqr(factor);
      if (imagData)
        imagData[i] *= factor;
    }
    break;
  case WINDOW_FLATTOP:
    for (i = 0; i < rows; i++) {
      r = i * PIx2 / (rows - 1);
      factor = 1 - 1.93 * cos(r) + 1.29 * cos(2 * r) - 0.388 * cos(3 * r) + 0.032 * cos(4 * r);
      data[i] *= factor;
      windowCorrectionFactor += sqr(factor);
      if (imagData)
        imagData[i] *= factor;
    }
    break;
  case WINDOW_GAUSSIAN:
    for (i = 0; i < rows; i++) {
      r = sqr((i - (rows - 1) / 2.) / (0.4 * (rows - 1) / 2.)) / 2;
      factor = exp(-r);
      data[i] *= factor;
      windowCorrectionFactor += sqr(factor);
      if (imagData)
        imagData[i] *= factor;
    }
    break;
  case WINDOW_NONE:
  default:
    windowCorrectionFactor = 1;
    break;
  }

  if (correctWindowEffects) {
    /* Add correction factor to make the integrated PSD come out right. */
    windowCorrectionFactor = 1 / sqrt(windowCorrectionFactor / rows);
    for (i = 0; i < rows; i++)
      data[i] *= windowCorrectionFactor;
    if (imagData)
      imagData[i] *= windowCorrectionFactor;
  }
  if (imagData && flags & FL_COMPLEXINPUT_FOLDED) {
    double min, max, max1;
    data = SDDS_Realloc(data, sizeof(*data) * rows * 2);
    imagData = SDDS_Realloc(imagData, sizeof(*data) * rows * 2);
    find_min_max(&min, &max, data, rows);
    if (fabs(min) > fabs(max))
      max1 = fabs(min);
    else
      max1 = fabs(max);
    find_min_max(&min, &max, imagData, rows);
    if (fabs(min) > max1)
      max1 = fabs(min);
    if (fabs(max) > max1)
      max1 = fabs(max);
    if (fabs(imagData[rows - 1]) / max1 < 1.0e-15) {
      fftrows = 2 * (rows - 1);
      for (i = 1; i < rows - 1; i++) {
        data[i] = data[i] / 2.0;
        imagData[i] = imagData[i] / 2.0;
      }
      for (i = 1; i < rows - 1; i++) {
        data[rows - 1 + i] = data[rows - 1 - i];
        imagData[rows - 1 + i] = -imagData[rows - 1 - i];
      }
      length = (tdata[rows - 1] - tdata[0]) * 2.0;
    } else {
      fftrows = 2 * (rows - 1) + 1;
      for (i = 1; i < rows; i++) {
        data[i] = data[i] / 2.0;
        imagData[i] = imagData[i] / 2.0;
      }
      for (i = 0; i < rows - 1; i++) {
        data[rows + i] = data[rows - 1 - i];
        imagData[rows + i] = -imagData[rows - 1 - i];
      }
      length = ((double)fftrows) * (tdata[rows - 1] - tdata[0]) / ((double)fftrows - 1.0) * 2;
    }
  } else {
    fftrows = rows;
    length = ((double)rows) * (tdata[rows - 1] - tdata[0]) / ((double)rows - 1.0);
  }
  /* compute FFT */

  real_imag = tmalloc(sizeof(double) * (2 * fftrows + 2));
  for (i = 0; i < fftrows; i++) {
    real_imag[2 * i] = data[i];
    if (imagData)
      real_imag[2 * i + 1] = imagData[i];
    else
      real_imag[2 * i + 1] = 0;
  }
  if (!inverse) {
    complexFFT(real_imag, fftrows, 0);
    if (flags & FL_FULLOUTPUT_UNFOLDED) {
      n_freq = fftrows;
      unfold = 1;
    } else if (flags & FL_FULLOUTPUT_FOLDED)
      n_freq = fftrows / 2 + 1;
    else if (!imagData)
      n_freq = fftrows / 2 + 1;
    else
      n_freq = fftrows + 1;
  } else {
    complexFFT(real_imag, fftrows, INVERSE_FFT);
    n_freq = fftrows;
  }
  /* calculate factor for converting k to f or omega */
  /* length is assumed the length of period, not the total length of the data file */

  /* length = ((double)rows)*(tdata[rows-1]-tdata[0])/((double)rows-1.0); */
  t0 = tdata[0];
  df = factor = 1.0 / length;

  /* convert into amplitudes and frequencies, adding phase factor for t[0]!=0 */
  real = tmalloc(sizeof(double) * n_freq);
  imag = tmalloc(sizeof(double) * n_freq);
  fdata = tmalloc(sizeof(double) * n_freq);
  magData = tmalloc(sizeof(double) * n_freq);
  if (flags & FL_PSDOUTPUT || flags & (FL_PSDINTEGOUTPUT + FL_PSDRINTEGOUTPUT)) {
    psd = tmalloc(sizeof(*psd) * n_freq);
    if (flags & (FL_PSDINTEGOUTPUT + FL_PSDRINTEGOUTPUT)) {
      psdInteg = tmalloc(sizeof(*psdInteg) * n_freq);
      psdIntegPower = tmalloc(sizeof(*psdIntegPower) * n_freq);
    }
  }

  for (i = 0; i < n_freq; i++) {
    fdata[i] = i * df;
    dtf_real = cos(-2 * PI * fdata[i] * t0);
    dtf_imag = sin(-2 * PI * fdata[i] * t0);
    if (psd)
      psd[i] = (sqr(real_imag[2 * i]) + sqr(real_imag[2 * i + 1])) / df;
    if (!imagData && i != 0 && !(i == (n_freq - 1) && rows % 2 == 0)) {
      /* This is not the DC or Nyquist term, so
         multiply by 2 to account for amplitude in
         negative frequencies
      */
      if (!unfold) {
        real_imag[2 * i] *= 2;
        real_imag[2 * i + 1] *= 2;
      }
      if (psd)
        psd[i] *= 2; /* 2 really is what I want--not 4 */
    }
    real[i] = real_imag[2 * i] * dtf_real - real_imag[2 * i + 1] * dtf_imag;
    imag[i] = real_imag[2 * i + 1] * dtf_real + real_imag[2 * i] * dtf_imag;
    magData[i] = sqrt(sqr(real[i]) + sqr(imag[i]));
  }

  if (psdInteg) {
    if (flags & FL_PSDINTEGOUTPUT) {
      psdIntegPower[0] = 0;
      for (i = 1; i < n_freq; i++)
        psdIntegPower[i] = psdIntegPower[i - 1] + (psd[i - 1] + psd[i]) * df / 2.;
      for (i = 0; i < n_freq; i++)
        psdInteg[i] = sqrt(psdIntegPower[i]);
    } else {
      psdIntegPower[n_freq - 1] = 0;
      for (i = n_freq - 2; i >= 0; i--) {
        if (rintegCutOffFreq == 0 || fdata[i] <= rintegCutOffFreq)
          psdIntegPower[i] = psdIntegPower[i + 1] + (psd[i + 1] + psd[i]) * df / 2.;
      }
      for (i = 0; i < n_freq; i++)
        psdInteg[i] = sqrt(psdIntegPower[i]);
    }
  }

  if (flags & FL_FULLOUTPUT) {
    arg = tmalloc(sizeof(*arg) * n_freq);
    for (i = 0; i < n_freq; i++) {
      if (real[i] || imag[i])
        arg[i] = 180.0 / PI * atan2(imag[i], real[i]);
      else
        arg[i] = 0;
    }
  }
  if (flags & FL_UNWRAP_PHASE) {
    find_min_max(&min, &max, magData, n_freq);
    unwrapArg = tmalloc(sizeof(*unwrapArg) * n_freq);
    phase_correction = 0;
    for (i = 0; i < n_freq; i++) {
      if (i && magData[i] / max > unwrapLimit) {
        delta = arg[i] - arg[i - 1];
        if (delta < -180.0)
          phase_correction += 360.0;
        else if (delta > 180.0)
          phase_correction -= 360.0;
      }
      unwrapArg[i] = arg[i] + phase_correction;
    }
  }

  if (flags & FL_NORMALIZE) {
    factor = -DBL_MAX;
    for (i = 0; i < n_freq; i++)
      if (magData[i] > factor)
        factor = magData[i];
    if (factor != -DBL_MAX)
      for (i = 0; i < n_freq; i++) {
        real[i] /= factor;
        imag[i] /= factor;
        magData[i] /= factor;
      }
  }
  if (!inverse)
    sprintf(s, "FFT%s", depenQuantity + (imagData ? 4 : 0));
  else {
    if (strncmp(depenQuantity, "FFT", 3) == 0)
      sprintf(s, "%s", depenQuantity + 3);
    else if (strncmp(depenQuantity, "RealFFT", 7) == 0)
      sprintf(s, "%s", depenQuantity + 7);
    else
      sprintf(s, "%s", depenQuantity);
  }

  if ((index = SDDS_GetColumnIndex(SDDSout, s)) < 0)
    return 0;

  if (flags & FL_SUPPRESSAVERAGE) {
    n_freq -= 1;
    offset = 1;
  } else
    offset = 0;

  if ((flags & FL_MAKEFREQDATA &&
       !SDDS_SetColumn(SDDSout, SDDS_SET_BY_INDEX, fdata + offset, n_freq, 0)) ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_INDEX, magData + offset, n_freq, index + fftOffset) ||
      (flags & FL_FULLOUTPUT &&
       (!SDDS_SetColumn(SDDSout, SDDS_SET_BY_INDEX, real + offset, n_freq, index + realOffset) ||
        !SDDS_SetColumn(SDDSout, SDDS_SET_BY_INDEX, imag + offset, n_freq, index + imagOffset) ||
        !SDDS_SetColumn(SDDSout, SDDS_SET_BY_INDEX, arg + offset, n_freq, index + argOffset))) ||
      (flags & FL_PSDOUTPUT &&
       !SDDS_SetColumn(SDDSout, SDDS_SET_BY_INDEX, psd + offset, n_freq, index + psdOffset)) ||
      (flags & (FL_PSDINTEGOUTPUT + FL_PSDRINTEGOUTPUT) && (!SDDS_SetColumn(SDDSout, SDDS_SET_BY_INDEX, psdInteg + offset, n_freq, index + psdIntOffset) ||
                                                            !SDDS_SetColumn(SDDSout, SDDS_SET_BY_INDEX, psdIntegPower + offset, n_freq, index + psdIntPowerOffset))) ||
      (flags & FL_UNWRAP_PHASE && !SDDS_SetColumn(SDDSout, SDDS_SET_BY_INDEX, unwrapArg + offset, n_freq, index + unwrappedArgOffset)))
    return 0;
  if (sampleInterval > 0) {
    int32_t *sample_row_flag;
    sample_row_flag = calloc(sizeof(*sample_row_flag), n_freq);
    for (i = 0; i < n_freq; i += sampleInterval)
      sample_row_flag[i] = 1;
    if (!SDDS_AssertRowFlags(SDDSout, SDDS_FLAG_ARRAY, sample_row_flag, n_freq)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
    free(sample_row_flag);
  }
  if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "fftFrequencies", n_freq, "fftFrequencySpacing", df, NULL))
    return 0;
  if (flags & FL_FULLOUTPUT && !SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "SpectrumFolded", flags & FL_FULLOUTPUT_UNFOLDED ? 0 : 1, NULL))
    return 0;
  free(data);
  free(magData);
  if (imagData)
    free(imagData);
  if (tDataStore)
    free(tDataStore);
  if (arg)
    free(arg);
  free(real_imag);
  free(real);
  free(imag);
  free(fdata);
  if (psd)
    free(psd);
  if (psdInteg)
    free(psdInteg);
  if (psdIntegPower)
    free(psdIntegPower);
  if (unwrapArg)
    free(unwrapArg);
  return 1;
}

long create_fft_frequency_column(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *timeName, char *freqUnits, long inverse) {
  char s[SDDS_MAXLINE];
  char *timeSymbol;
  char *description;

  if (SDDS_GetColumnInformation(SDDSin, "symbol", &timeSymbol, SDDS_GET_BY_NAME, timeName) != SDDS_STRING)
    return 0;
  if (!timeSymbol || SDDS_StringIsBlank(timeSymbol))
    SDDS_CopyString(&timeSymbol, timeName);

  sprintf(s, "Frequency for %s", timeSymbol);
  SDDS_CopyString(&description, s);
  if (!inverse) {
    if (SDDS_DefineColumn(SDDSout, "f", NULL, freqUnits, description, NULL, SDDS_DOUBLE, 0) < 0) {
      free(timeSymbol);
      free(description);
      return 0;
    }
  } else {
    sprintf(s, "inverse for %s", timeSymbol);
    SDDS_CopyString(&description, s);
    if (SDDS_DefineColumn(SDDSout, "t", NULL, freqUnits, description, NULL, SDDS_DOUBLE, 0) < 0) {
      free(timeSymbol);
      free(description);
      return 0;
    }
  }
  free(timeSymbol);
  free(description);
  return 1;
}

long create_fft_columns(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *origName, char *indepName, char *freqUnits, long full_output, unsigned long psd_output, long complexInput, long inverse, long unwrap_phase) {
  char s[SDDS_MAXLINE];
  char *origUnits, *origSymbol;
  char *description, *name, *symbol, *units;
  long index0, index1;
  long offset = 0;

  if (complexInput)
    offset = 4;
  if (SDDS_GetColumnInformation(SDDSin, "units", &origUnits, SDDS_GET_BY_NAME, origName) != SDDS_STRING ||
      SDDS_GetColumnInformation(SDDSin, "symbol", &origSymbol, SDDS_GET_BY_NAME, origName) != SDDS_STRING)
    return 0;
  if (!inverse)
    sprintf(s, "FFT%s", origName + offset);
  else {
    if (strncmp(origName, "FFT", 3) == 0)
      offset = 3;
    else if (strncmp(origName, "RealFFT", 7) == 0)
      offset = 7;
    else
      offset = 0;
    sprintf(s, "%s", origName + offset);
  }
  SDDS_CopyString(&name, s);
  if (!origSymbol)
    SDDS_CopyString(&origSymbol, origName + offset);
  sprintf(s, "FFT %s", origSymbol);
  SDDS_CopyString(&symbol, s);

  sprintf(s, "Amplitude of FFT of %s", origSymbol);
  SDDS_CopyString(&description, s);

  if (SDDS_NumberOfErrors() || (index0 = SDDS_DefineColumn(SDDSout, name, symbol, origUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
    return 0;
  free(name);
  free(symbol);
  free(description);

  if (fftOffset == -1)
    fftOffset = 0;

  if (psd_output & FL_PSDOUTPUT) {
    if (origUnits && !SDDS_StringIsBlank(origUnits)) {
      if (freqUnits && !SDDS_StringIsBlank(freqUnits)) {
        sprintf(s, "(%s)$a2$n/(%s)", origUnits, freqUnits);
      } else
        sprintf(s, "(%s)$a2$n", origUnits);
      SDDS_CopyString(&units, s);
    } else
      units = NULL;

    sprintf(s, "PSD%s", origName + offset);
    SDDS_CopyString(&name, s);

    if (!origSymbol)
      SDDS_CopyString(&origSymbol, origName + offset);
    sprintf(s, "PSD %s", origSymbol);
    SDDS_CopyString(&symbol, s);

    sprintf(s, "PSD of %s", origSymbol);
    SDDS_CopyString(&description, s);

    if (SDDS_NumberOfErrors() || (index1 = SDDS_DefineColumn(SDDSout, name, symbol, units, description, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    psdOffset = index1 - index0;
    free(name);
    if (units)
      free(units);
    free(symbol);
    free(description);
  }

  if (psd_output & (FL_PSDINTEGOUTPUT + FL_PSDRINTEGOUTPUT)) {
    if (origUnits && !SDDS_StringIsBlank(origUnits)) {
      SDDS_CopyString(&units, origUnits);
    } else
      units = NULL;

    sprintf(s, "SqrtIntegPSD%s", origName + offset);
    SDDS_CopyString(&name, s);

    if (!origSymbol)
      SDDS_CopyString(&origSymbol, origName + offset);
    sprintf(s, "Sqrt Integ PSD %s", origSymbol);
    SDDS_CopyString(&symbol, s);

    sprintf(s, "Sqrt Integ PSD of %s", origSymbol);
    SDDS_CopyString(&description, s);

    if (SDDS_NumberOfErrors() || (index1 = SDDS_DefineColumn(SDDSout, name, symbol, units, description, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    psdIntOffset = index1 - index0;

    sprintf(s, "IntegPSD%s", origName + offset);
    SDDS_CopyString(&name, s);

    if (!origSymbol)
      SDDS_CopyString(&origSymbol, origName + offset);
    sprintf(s, "Integ PSD %s", origSymbol);
    SDDS_CopyString(&symbol, s);

    sprintf(s, "Integ PSD of %s", origSymbol);
    SDDS_CopyString(&description, s);

    if (origUnits && !SDDS_StringIsBlank(origUnits)) {
      sprintf(s, "%sPower", origUnits);
      SDDS_CopyString(&units, origUnits);
    } else
      units = NULL;

    if (SDDS_NumberOfErrors() || (index1 = SDDS_DefineColumn(SDDSout, name, symbol, units, description, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    psdIntPowerOffset = index1 - index0;
    free(name);
    if (units)
      free(units);
    free(symbol);
    free(description);
  }

  if (full_output) {
    if (!inverse)
      sprintf(s, "RealFFT%s", origName + offset);
    else
      sprintf(s, "Real%s", origName + offset);
    SDDS_CopyString(&name, s);

    if (!origSymbol)
      SDDS_CopyString(&origSymbol, origName + offset);
    if (!inverse)
      sprintf(s, "Re[FFT %s]", origSymbol);
    else
      sprintf(s, "Re[%s]", origSymbol);
    SDDS_CopyString(&symbol, s);

    if (!inverse)
      sprintf(s, "Real part of FFT of %s", origSymbol);
    else
      sprintf(s, "Real part of %s", origSymbol);
    SDDS_CopyString(&description, s);

    if (SDDS_NumberOfErrors() || (index1 = SDDS_DefineColumn(SDDSout, name, symbol, origUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    realOffset = index1 - index0;
    free(name);
    free(symbol);
    free(description);

    if (!inverse)
      sprintf(s, "ImagFFT%s", origName + offset);
    else
      sprintf(s, "Imag%s", origName + offset);
    SDDS_CopyString(&name, s);

    if (!origSymbol)
      SDDS_CopyString(&origSymbol, origName + offset);
    if (!inverse)
      sprintf(s, "Im[FFT %s]", origSymbol);
    else
      sprintf(s, "Im[%s]", origSymbol);
    SDDS_CopyString(&symbol, s);

    if (!inverse)
      sprintf(s, "Imaginary part of FFT of %s", origSymbol);
    else
      sprintf(s, "Imaginary part of %s", origSymbol);
    SDDS_CopyString(&description, s);

    if (SDDS_NumberOfErrors() || (index1 = SDDS_DefineColumn(SDDSout, name, symbol, origUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    imagOffset = index1 - index0;
    free(name);
    free(symbol);
    free(description);

    if (!inverse)
      sprintf(s, "ArgFFT%s", origName + offset);
    else
      sprintf(s, "Arg%s", origName + offset);
    SDDS_CopyString(&name, s);

    if (!origSymbol)
      SDDS_CopyString(&origSymbol, origName + offset);
    if (!inverse)
      sprintf(s, "Arg[FFT %s]", origSymbol);
    else
      sprintf(s, "Arg[%s]", origSymbol);
    SDDS_CopyString(&symbol, s);

    if (!inverse)
      sprintf(s, "Phase of FFT of %s", origSymbol);
    else
      sprintf(s, "Phase of %s", origSymbol);
    SDDS_CopyString(&description, s);

    if (SDDS_NumberOfErrors() || (index1 = SDDS_DefineColumn(SDDSout, name, symbol, "degrees", description, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    argOffset = index1 - index0;
    free(name);
    free(symbol);
    free(description);
    if (unwrap_phase) {
      if (!inverse)
        sprintf(s, "UnwrapArgFFT%s", origName + offset);
      else
        sprintf(s, "UnwrapArg%s", origName + offset);
      SDDS_CopyString(&name, s);

      if (!origSymbol)
        SDDS_CopyString(&origSymbol, origName + offset);
      if (!inverse)
        sprintf(s, "UnwrapArg[FFT %s]", origSymbol);
      else
        sprintf(s, "UnwrapArg[%s]", origSymbol);
      SDDS_CopyString(&symbol, s);

      if (!inverse)
        sprintf(s, "Unwrapped Phase of FFT of %s", origSymbol);
      else
        sprintf(s, "Unwrapped Phase of %s", origSymbol);
      SDDS_CopyString(&description, s);

      if (SDDS_NumberOfErrors() || (index1 = SDDS_DefineColumn(SDDSout, name, symbol, "degrees", description, NULL, SDDS_DOUBLE, 0)) < 0)
        return 0;
      unwrappedArgOffset = index1 - index0;
      free(name);
      free(symbol);
      free(description);
    }
  }

  return 1;
}

void moveToStringArrayComplex(char ***targetReal, char ***targetImag, long *targets, char **sourceReal, char **sourceImag, long sources);

long expandComplexColumnPairNames(SDDS_DATASET *SDDSin, char **name, char ***realName, char ***imagName, long names, char **excludeName, long excludeNames, long typeMode, long typeValue) {
  long i, j, k, realNames, imagNames, names2;
  char **realName1, **imagName1, **realName2, **imagName2;
  char *realPattern, *imagPattern = NULL;
  long longest=0;

  if (!names || !name)
    return 0;
  realName1 = imagName1 = realName2 = imagName2 = NULL;
  realNames = imagNames = names2 = 0;
  for (i = 0; i < names; i++) {
    if (strlen(name[i]) > longest)
      longest = strlen(name[i]);
  }
  longest += 10;
  if (!(realPattern = SDDS_Malloc(sizeof(*realPattern) * longest)) || !(imagPattern = SDDS_Malloc(sizeof(*imagPattern) * longest)))
    SDDS_Bomb("memory allocation failure");

  for (i = 0; i < names; i++) {
    for (j = 0; j < 2; j++) {
      if (j == 0) {
        sprintf(realPattern, "Real%s", name[i]);
        sprintf(imagPattern, "Imag%s", name[i]);
      } else {
        sprintf(realPattern, "%sReal", name[i]);
        sprintf(imagPattern, "%sImag", name[i]);
      }
      switch (typeMode) {
      case FIND_ANY_TYPE:
      case FIND_NUMERIC_TYPE:
      case FIND_INTEGER_TYPE:
      case FIND_FLOATING_TYPE:
        realNames = SDDS_MatchColumns(SDDSin, &realName1, SDDS_MATCH_STRING, typeMode, realPattern, SDDS_0_PREVIOUS | SDDS_OR);
        imagNames = SDDS_MatchColumns(SDDSin, &imagName1, SDDS_MATCH_STRING, typeMode, imagPattern, SDDS_0_PREVIOUS | SDDS_OR);
        break;
      case FIND_SPECIFIED_TYPE:
        if (!SDDS_VALID_TYPE(typeValue))
          SDDS_Bomb("invalid type value in expandColumnPairNames");
        realNames = SDDS_MatchColumns(SDDSin, &realName1, SDDS_MATCH_STRING, typeMode, typeValue, realPattern, SDDS_0_PREVIOUS | SDDS_OR);
        imagNames = SDDS_MatchColumns(SDDSin, &imagName1, SDDS_MATCH_STRING, typeMode, typeValue, imagPattern, SDDS_0_PREVIOUS | SDDS_OR);
        break;
      default:
        SDDS_Bomb("invalid typeMode in expandColumnPairNames");
        exit(EXIT_FAILURE);
        break;
      }
      if (realNames == 0)
        continue;
      if (realNames == -1 || imagNames == -1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Bomb("unable to perform column name match in expandColumnPairNames");
      }
      if (realNames != imagNames)
        SDDS_Bomb("found different number of real and imaginary columns");
      if (excludeNames) {
        for (j = 0; j < excludeNames; j++)
          for (k = 0; k < realNames; k++)
            if (wild_match(realName1[k], excludeName[j])) {
              free(realName1[k]);
              free(imagName1[k]);
              imagName1[k] = realName1[k] = NULL;
            }
      }
      moveToStringArrayComplex(&realName2, &imagName2, &names2, realName1, imagName1, realNames);
      free(realName1);
      free(imagName1);
    }
  }
  free(realPattern);
  free(imagPattern);
  if (names2 == 0)
    return 0;
  *realName = realName2;
  *imagName = imagName2;
  return names2;
}

void moveToStringArrayComplex(char ***targetReal, char ***targetImag, long *targets, char **sourceReal, char **sourceImag, long sources) {
  long i, j;
  if (!sources)
    return;
  if (!(*targetReal = SDDS_Realloc(*targetReal, sizeof(**targetReal) * (*targets + sources))) ||
      !(*targetImag = SDDS_Realloc(*targetImag, sizeof(**targetImag) * (*targets + sources))))
    SDDS_Bomb("memory allocation failure");
  for (i = 0; i < sources; i++) {
    if (sourceReal[i] == NULL || sourceImag[i] == NULL)
      continue;
    for (j = 0; j < *targets; j++)
      if (strcmp(sourceReal[i], (*targetReal)[j]) == 0)
        break;
    if (j == *targets) {
      (*targetReal)[j] = sourceReal[i];
      (*targetImag)[j] = sourceImag[i];
      *targets += 1;
    }
  }
}
