/**
 * @file sdds2dfft.c
 * @brief SDDS-format 2D FFT program.
 *
 * @details
 * This program performs a two-dimensional Fast Fourier Transform (FFT) on data formatted in the
 * Self Describing Data Set (SDDS) format. It provides various options for normalization, padding, truncation,
 * suppressing averages, and more to customize the FFT process.
 *
 * @section Usage
 * ```
 * sdds2dfft [<inputfile>] [<outputfile>]
 *           [-pipe=[input][,output]]
 *            -columns=<indep-variable>[,<depen-quantity>[,...]]
 *           [-complexInput[=unfolded|folded]]
 *           [-exclude=<depen-quantity>[,...]]
 *           [-sampleInterval=<number>]
 *           [-normalize]
 *           [-fullOutput[=unfolded|folded],unwrapLimit=<value>]
 *           [-psdOutput[=plain][,{integrated|rintegrated[=<cutoff>]}]]
 *           [-inverse]
 *           [-padwithzeroes[=exponent]]
 *           [-truncate]
 *           [-suppressaverage]
 *           [-noWarnings]
 *           [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-columns`          | Specify independent and dependent variables for FFT analysis.  |
 *
 * | Optional            | Description                                                     |
 * |---------------------|-----------------------------------------------------------------|
 * | `-pipe`             | Standard SDDS Toolkit pipe option.                             |
 * | `-complexInput`     | Indicates the input columns are in complex form.               |
 * | `-exclude`          | Exclude quantities from analysis using wildcards.              |
 * | `-sampleInterval`   | Request sampling of input data points at specified intervals.  |
 * | `-normalize`        | Normalize output to a peak magnitude of 1.                     |
 * | `-fullOutput`       | Request real and imaginary parts of the FFT.                   |
 * | `-psdOutput`        | Request Power Spectral Density (PSD) output.                   |
 * | `-inverse`          | Perform inverse Fourier transform.                             |
 * | `-padwithzeroes`    | Pad data with zeroes to match required data points.            |
 * | `-truncate`         | Truncate data to match required data points.                   |
 * | `-suppressaverage`  | Suppress the average value before FFT.                         |
 * | `-noWarnings`       | Suppress warning messages.                                     |
 * | `-majorOrder`       | Specify output file's data order (row or column).              |
 *
 * @subsection Incompatibilities
 * - `-inverse` is incompatible with:
 *   - `-complexInput=folded`
 * - `-padwithzeroes` is incompatible with `-truncate`.
 * - For `-complexInput`:
 *   - Requires `-columns` specifying dependent quantities in pairs (real, imaginary).
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
 * H. Shang, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "fftpackC.h"
#include "SDDSutils.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
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

static char *USAGE1 =
  "Usage: sdds2dfft [<inputfile>] [<outputfile>]\n"
  "          [-pipe=[input][,output]]\n"
  "           -columns=<indep-variable>[,<depen-quantity>[,...]]\n"
  "          [-complexInput[=unfolded|folded]]\n"
  "          [-exclude=<depen-quantity>[,...]]\n"
  "          [-sampleInterval=<number>]\n"
  "          [-normalize]\n"
  "          [-fullOutput[=unfolded|folded],unwrapLimit=<value>]\n"
  "          [-psdOutput[=plain][,{integrated|rintegrated[=<cutoff>]}]]\n"
  "          [-inverse]\n"
  "          [-padwithzeroes[=exponent]]\n"
  "          [-truncate]\n"
  "          [-suppressaverage]\n"
  "          [-noWarnings]\n"
  "          [-majorOrder=row|column]\n"
  "Options:\n"
  "  -pipe=[input][,output]\n"
  "    The standard SDDS Toolkit pipe option.\n"
  "  -columns=<indep-variable>[,<depen-quantity>[,...]]\n"
  "    Specifies the independent variable and dependent quantities to Fourier analyze.\n"
  "    <depen-quantity> entries may contain wildcards.\n"
  "  -complexInput[=unfolded|folded]\n"
  "    Indicates that the input columns are in complex form.\n"
  "    Options:\n"
  "      unfolded - The input frequency space is unfolded and must include negative frequencies.\n"
  "      folded   - The input frequency space is folded (default).\n"
  "  -inverse\n"
  "    Produces the inverse Fourier transform. The output is always an unfolded spectrum.\n"
  "    If combined with -fullOutput=folded, it will be changed to -fullOutput=unfolded.\n"
  "  -exclude=<depen-quantity>[,...]\n"
  "    Specifies a list of wild-card patterns to exclude certain quantities from analysis.\n";

static char *USAGE2 =
  "  -sampleInterval=<number>\n"
  "    Requests sampling of the input data points with the given interval.\n"
  "  -normalize\n"
  "    Normalizes the output to a peak magnitude of 1.\n"
  "  -fullOutput[=unfolded|folded],unwrapLimit=<value>\n"
  "    Requests output of the real and imaginary parts of the FFT.\n"
  "    Options:\n"
  "      unfolded - Outputs the unfolded frequency-space (full FFT).\n"
  "      folded   - Outputs the folded frequency-space (half FFT) (default).\n"
  "    Additional parameter:\n"
  "      unwrapLimit=<value> - Unwraps the phase where the relative magnitude exceeds this limit.\n"
  "  -psdOutput[=plain][,{integrated|rintegrated[=<cutoff>]}]\n"
  "    Requests output of the Power Spectral Density (PSD).\n"
  "    Qualifiers:\n"
  "      plain       - Includes plain PSD output.\n"
  "      integrated  - Includes integrated PSD.\n"
  "      rintegrated - Includes reverse-integrated PSD with an optional cutoff frequency.\n"
  "  -padwithzeroes[=exponent] | -truncate\n"
  "    -padwithzeroes: Pads the data with zeroes if the number of data points is not a product of small primes.\n"
  "      Optionally specify an exponent to determine the padding factor.\n"
  "    -truncate: Truncates the data if the number of data points is not a product of small primes.\n"
  "  -suppressaverage\n"
  "    Removes the average value of the data before performing the FFT.\n"
  "  -noWarnings\n"
  "    Suppresses warning messages.\n"
  "  -majorOrder=row|column\n"
  "    Specifies the output file's data order.\n"
  "      row    - Row-major order.\n"
  "      column - Column-major order.\n\n"
  "Program by Hairong Shang. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int64_t greatestProductOfSmallPrimes(int64_t rows);
long create_fft_frequency_column(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *timeName, char *freqUnits, long inverse);

long create_fft_columns(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *origName, char *indepName,
                        char *freqUnits, long full_output, unsigned long psd_output, long complexInput,
                        long inverse, long unwrap_phase);
long create_fft_parameters(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *indepName, char *freqUnits);
char *makeFrequencyUnits(SDDS_DATASET *SDDSin, char *indepName);
long expandComplexColumnPairNames(SDDS_DATASET *SDDSin, char **name, char ***realName, char ***imagName,
                                  long names, char **excludeName, long excludeNames, long typeMode, long typeValue);
void moveToStringArrayComplex(char ***targetReal, char ***targetImag, long *targets, char **sourceReal, char **sourceImag, long sources);

int main(int argc, char **argv) {
  int iArg, j;
  char *freqUnits;
  char *indepQuantity, **depenQuantity, **exclude, **realQuan = NULL, **imagQuan = NULL;
  long depenQuantities, excludes;
  char *input, *output;
  long sampleInterval, readCode, noWarnings, complexInput, inverse, spectrumFoldParExist = 0, colsToUse;
  int64_t i, rows, rowsToUse, primeRows, pow2Rows, n_freq, fftrows;
  int32_t spectrumFolded = 0, page = 0, index;
  unsigned long flags, pipeFlags, complexInputFlags = 0, fullOutputFlags = 0, majorOrderFlag;
  long primeCols, pow2Cols;
  SCANNED_ARG *scanned;
  SDDS_DATASET SDDSin, SDDSout;
  double *tdata, rintegCutOffFreq, unwrapLimit = 0;
  long padFactor;
  short columnMajorOrder = -1;
  double length, *real_imag = NULL, **real = NULL, **imag = NULL, *real_imag1 = NULL, *fdata = NULL, df, t0, factor;
  double dtf_real, dtf_imag, *arg = NULL, *magData = NULL;
  char str[256], *tempStr = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3 || argc > (3 + N_OPTIONS)) {
    fprintf(stderr, "%s%s", USAGE1, USAGE2);
    exit(EXIT_FAILURE);
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
        if (scanned[iArg].n_items != 2 || sscanf(scanned[iArg].list[1], "%ld", &sampleInterval) != 1 || sampleInterval <= 0)
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
            SDDS_CopyString(&depenQuantity[i], scanned[iArg].list[i + 2]);
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
        if (scanned[iArg].n_items > 1) {
          unsigned long tmpFlags;
          if (strchr(scanned[iArg].list[1], '=') == NULL) {
            if (!scanItemList(&tmpFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "integrated", -1, NULL, 0, FL_PSDINTEGOUTPUT, "rintegrated", -1, NULL, 0, FL_PSDRINTEGOUTPUT, "plain", -1, NULL, 0, FL_PSDOUTPUT, NULL))
              SDDS_Bomb("invalid -psdOutput syntax");
          } else {
            if (!scanItemList(&tmpFlags, scanned[iArg].list + 1, &scanned[iArg].n_items, 0, "integrated", -1, NULL, 0, FL_PSDINTEGOUTPUT, "rintegrated", SDDS_DOUBLE, &rintegCutOffFreq, 0, FL_PSDRINTEGOUTPUT, "plain", -1, NULL, 0, FL_PSDOUTPUT, NULL))
              SDDS_Bomb("invalid -psdOutput syntax");
          }
          flags |= tmpFlags;
        } else {
          flags |= FL_PSDOUTPUT;
        }
        if ((flags & FL_PSDINTEGOUTPUT) && (flags & FL_PSDRINTEGOUTPUT))
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
      fprintf(stderr, "Warning: The inverse option is ignored since it only works with -complexInput.\n");
    inverse = 0;
  }
  if (!noWarnings && inverse && (flags & FL_FULLOUTPUT_FOLDED))
    fprintf(stderr, "Warning: The combination of -inverse and -fullOutput=folded will be changed to -inverse -fullOutput=unfolded.\n");

  processFilenames("sdds2dfft", &input, &output, pipeFlags, 0, NULL);

  if (!indepQuantity)
    SDDS_Bomb("Supply the independent quantity name with the -columns option");

  if ((flags & FL_TRUNCATE) && (flags & FL_PADWITHZEROES))
    SDDS_Bomb("Specify only one of -padwithzeroes and -truncate");
  if (!inverse) {
    /* For 2D FFT, always use full output unfolded */
    flags |= FL_FULLOUTPUT;
    flags |= FL_FULLOUTPUT_UNFOLDED;
  }
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
      SDDS_Bomb("No quantities selected to FFT");
    }
  } else {
    if ((depenQuantities = expandComplexColumnPairNames(&SDDSin, depenQuantity, &realQuan, &imagQuan, depenQuantities, exclude, excludes, FIND_NUMERIC_TYPE, 0)) <= 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_Bomb("No quantities selected to FFT");
    }
  }

#if 0
  fprintf(stderr, "%ld dependent quantities:\n", depenQuantities);
  for (i = 0; i < depenQuantities; i++)
    fprintf(stderr, "  %s\n", depenQuantity[i]);
#endif

  if (!(freqUnits = makeFrequencyUnits(&SDDSin, indepQuantity)) ||
      !SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 0, NULL, "sdds2dfft output", output) ||
      !create_fft_frequency_column(&SDDSout, &SDDSin, indepQuantity, freqUnits, inverse) ||
      SDDS_DefineParameter(&SDDSout, "fftFrequencies", NULL, NULL, NULL, NULL, SDDS_LONG, NULL) < 0 ||
      SDDS_DefineParameter(&SDDSout, "fftFrequencySpacing", "$gD$rf", freqUnits, NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDSin.layout.data_mode.column_major;

  if ((flags & FL_FULLOUTPUT) && SDDS_DefineParameter(&SDDSout, "SpectrumFolded", NULL, NULL, NULL, NULL, SDDS_LONG, NULL) < 0)
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
                         flags & FL_FULLOUTPUT, flags & (FL_PSDOUTPUT + FL_PSDINTEGOUTPUT + FL_PSDRINTEGOUTPUT),
                         0, inverse, flags & FL_UNWRAP_PHASE);
    else
      create_fft_columns(&SDDSout, &SDDSin, realQuan[i], indepQuantity, freqUnits,
                         flags & FL_FULLOUTPUT, flags & (FL_PSDOUTPUT + FL_PSDINTEGOUTPUT + FL_PSDRINTEGOUTPUT),
                         1, inverse, flags & FL_UNWRAP_PHASE);
  }

  if (!SDDS_TransferAllParameterDefinitions(&SDDSout, &SDDSin, SDDS_TRANSFER_KEEPOLD) || !SDDS_WriteLayout(&SDDSout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  colsToUse = depenQuantities;
  primeCols = greatestProductOfSmallPrimes(depenQuantities);
  if (depenQuantities != primeCols || padFactor) {
    if (flags & FL_PADWITHZEROES) {
      pow2Cols = ipow(2., ((long)(log((double)depenQuantities) / log(2.0F))) + (padFactor ? padFactor : 1));
      if ((primeCols = greatestProductOfSmallPrimes(pow2Cols)) > depenQuantities)
        colsToUse = primeCols;
      else
        colsToUse = pow2Cols;
      fprintf(stdout, "Using %ld columns\n", colsToUse);
    } else if (flags & FL_TRUNCATE)
      colsToUse = greatestProductOfSmallPrimes(depenQuantities);
    else if (largest_prime_factor(depenQuantities) > 100 && !noWarnings)
      fputs("Warning: Number of dependent columns has large prime factors.\nThis could take a very long time.\nConsider using the -truncate option.\n", stderr);
  }
  real_imag = tmalloc(sizeof(*real_imag) * (2 * colsToUse + 2));
  real = malloc(sizeof(*real) * colsToUse);
  imag = malloc(sizeof(*imag) * colsToUse);

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
      rowsToUse = rows;
      primeRows = greatestProductOfSmallPrimes(rows);
      if (rows != primeRows || padFactor) {
        if (flags & FL_PADWITHZEROES) {
          pow2Rows = ipow(2., ((long)(log((double)rows) / log(2.0F))) + (padFactor ? padFactor : 1));
          if ((primeRows = greatestProductOfSmallPrimes(pow2Rows)) > rows)
            rowsToUse = primeRows;
          else
            rowsToUse = pow2Rows;
          fprintf(stdout, "Using %" PRId64 " rows\n", rowsToUse);
        } else if (flags & FL_TRUNCATE)
          rowsToUse = greatestProductOfSmallPrimes(rows);
        else if (largest_prime_factor(rows) > 100 && !noWarnings)
          fputs("Warning: Number of points has large prime factors.\nThis could take a very long time.\nConsider using the -truncate option.\n", stderr);
      }
      if (!(tdata = SDDS_GetColumnInDoubles(&SDDSin, indepQuantity)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

      for (j = 0; j < colsToUse; j++) {
        real[j] = imag[j] = NULL;
        if (j < depenQuantities) {
          if (complexInput) {
            if (!(real[j] = (double *)SDDS_GetColumnInDoubles(&SDDSin, realQuan[j])) ||
                !(imag[j] = (double *)SDDS_GetColumnInDoubles(&SDDSin, imagQuan[j])))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          } else {
            if (!(real[j] = (double *)SDDS_GetColumnInDoubles(&SDDSin, depenQuantity[j])))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            imag[j] = calloc(sizeof(**imag), rowsToUse);
          }
          if (rows < rowsToUse) {
            real[j] = SDDS_Realloc(real[j], sizeof(**real) * rowsToUse);
            imag[j] = SDDS_Realloc(imag[j], sizeof(**imag) * rowsToUse);
          }
        } else {
          real[j] = calloc(sizeof(**real), rowsToUse);
          imag[j] = calloc(sizeof(**imag), rowsToUse);
        }
      }
      fdata = malloc(sizeof(*fdata) * rowsToUse);
      if (rows < rowsToUse) {
        length = ((double)rows) * (tdata[rows - 1] - tdata[0]) / ((double)rows - 1.0);
      } else
        length = tdata[rows - 1] - tdata[0];
      t0 = tdata[0];
      df = factor = 1.0 / length;
      free(tdata);
      for (i = 0; i < rows; i++)
        fdata[i] = i * df;
      for (i = rows; i < rowsToUse; i++) {
        fdata[i] = i * df;
      }
      /* First perform FFT per row */
      for (i = 0; i < rows; i++) {
        for (j = 0; j < colsToUse; j++) {
          real_imag[2 * j] = real_imag[2 * j + 1] = 0;
          if (j < depenQuantities) {
            real_imag[2 * j] = real[j][i];
            if (imag[j])
              real_imag[2 * j + 1] = imag[j][i];
            else
              real_imag[2 * j + 1] = 0;
          }
        }
        complexFFT(real_imag, colsToUse, inverse);
        for (j = 0; j < colsToUse; j++) {
          real[j][i] = real_imag[2 * j];
          imag[j][i] = real_imag[2 * j + 1];
        }
      }
      /* Then perform FFT by column */
      n_freq = rowsToUse;
      fftrows = rowsToUse;
      arg = malloc(sizeof(*arg) * rowsToUse);
      magData = malloc(sizeof(*magData) * rowsToUse);
      real_imag1 = calloc(sizeof(*real_imag1), 2 * fftrows + 2);

      for (j = 0; j < depenQuantities; j++) {
        for (i = 0; i < rowsToUse; i++) {
          if (i < rows) {
            real_imag1[2 * i] = real[j][i];
            real_imag1[2 * i + 1] = imag[j][i];
          } else {
            real_imag1[2 * i] = 0;
            real_imag1[2 * i + 1] = 0;
          }
        }
        complexFFT(real_imag1, rowsToUse, inverse);
        for (i = 0; i < n_freq; i++) {
          dtf_real = cos(-2 * PI * fdata[i] * t0);
          dtf_imag = sin(-2 * PI * fdata[i] * t0);
          real[j][i] = real_imag1[2 * i] * dtf_real - real_imag1[2 * i + 1] * dtf_imag;
          imag[j][i] = real_imag1[2 * i + 1] * dtf_real + real_imag1[2 * i] * dtf_imag;
          magData[i] = sqrt(sqr(real[j][i]) + sqr(imag[j][i]));
          if (real[j][i] || imag[j][i])
            arg[i] = 180.0 / PI * atan2(imag[j][i], real[j][i]);
          else
            arg[i] = 0;
        }
        if (flags & FL_NORMALIZE) {
          factor = -DBL_MAX;
          for (i = 0; i < n_freq; i++)
            if (magData[i] > factor)
              factor = magData[i];
          if (factor != -DBL_MAX)
            for (i = 0; i < n_freq; i++) {
              real[j][i] /= factor;
              imag[j][i] /= factor;
              magData[i] /= factor;
            }
        }
        if (!inverse)
          sprintf(str, "FFT%s", depenQuantity[j] + (imagQuan ? 4 : 0));
        else {
          if (complexInput)
            tempStr = realQuan[j];
          else
            tempStr = depenQuantity[j];

          if (strncmp(tempStr, "FFT", 3) == 0)
            sprintf(str, "%s", tempStr + 3);
          else if (strncmp(tempStr, "RealFFT", 7) == 0)
            sprintf(str, "%s", tempStr + 7);
          else
            sprintf(str, "%s", tempStr);
        }
        if ((index = SDDS_GetColumnIndex(&SDDSout, str)) < 0)
          exit(EXIT_FAILURE);
        if (!SDDS_StartPage(&SDDSout, rowsToUse) ||
            !SDDS_CopyParameters(&SDDSout, &SDDSin) ||
            !SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "fftFrequencies", n_freq, "fftFrequencySpacing", df, NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (flags & FL_FULLOUTPUT) {
          if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, magData, n_freq, index) ||
              !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, real[j], n_freq, index + 1) ||
              !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, imag[j], n_freq, index + 2) ||
              !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, arg, n_freq, index + 3))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        } else {
          if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, real[j], n_freq, index))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
      if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX, fdata, n_freq, 0))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      free(fdata);
      free(arg);
      free(magData);
      for (j = 0; j < colsToUse; j++) {
        if (real[j])
          free(real[j]);
        if (imag[j])
          free(imag[j]);
      }
      free(real_imag1);
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
  if (excludes) {
    SDDS_FreeStringArray(exclude, excludes);
    free(exclude);
  }
  free(real);
  free(imag);
  if (realQuan) {
    SDDS_FreeStringArray(realQuan, depenQuantities);
    SDDS_FreeStringArray(imagQuan, depenQuantities);
    free(realQuan);
    free(imagQuan);
  } else {
    SDDS_FreeStringArray(depenQuantity, depenQuantities);
    free(depenQuantity);
  }
  free(real_imag);
  free_scanargs(&scanned, argc);
  return EXIT_SUCCESS;
}

static long psdOffset, argOffset, realOffset, imagOffset, fftOffset = -1, psdIntOffset, unwrappedArgOffset = -1;

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

long create_fft_columns(SDDS_DATASET *SDDSout, SDDS_DATASET *SDDSin, char *origName, char *indepName,
                        char *freqUnits, long full_output, unsigned long psd_output, long complexInput,
                        long inverse, long unwrap_phase) {
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

  if (SDDS_NumberOfErrors() ||
      (index0 = SDDS_DefineColumn(SDDSout, name, symbol, origUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
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

    if (SDDS_NumberOfErrors() ||
        (index1 = SDDS_DefineColumn(SDDSout, name, symbol, units, description, NULL, SDDS_DOUBLE, 0)) < 0)
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

    if (SDDS_NumberOfErrors() ||
        (index1 = SDDS_DefineColumn(SDDSout, name, symbol, units, description, NULL, SDDS_DOUBLE, 0)) < 0)
      return 0;
    psdIntOffset = index1 - index0;
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

    if (SDDS_NumberOfErrors() ||
        (index1 = SDDS_DefineColumn(SDDSout, name, symbol, origUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
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

    if (SDDS_NumberOfErrors() ||
        (index1 = SDDS_DefineColumn(SDDSout, name, symbol, origUnits, description, NULL, SDDS_DOUBLE, 0)) < 0)
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

    if (SDDS_NumberOfErrors() ||
        (index1 = SDDS_DefineColumn(SDDSout, name, symbol, "degrees", description, NULL, SDDS_DOUBLE, 0)) < 0)
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

      if (SDDS_NumberOfErrors() ||
          (index1 = SDDS_DefineColumn(SDDSout, name, symbol, "degrees", description, NULL, SDDS_DOUBLE, 0)) < 0)
        return 0;
      unwrappedArgOffset = index1 - index0;
      free(name);
      free(symbol);
      free(description);
    }
  }

  free(origSymbol);
  return 1;
}

long expandComplexColumnPairNames(SDDS_DATASET *SDDSin, char **name, char ***realName, char ***imagName, long names,
                                  char **excludeName, long excludeNames, long typeMode, long typeValue) {
  long i, j, k, realNames, imagNames, names2;
  char **realName1, **imagName1, **realName2, **imagName2;
  char *realPattern, *imagPattern = NULL;
  long longest;

  if (!names || !name)
    return 0;
  realName1 = imagName1 = realName2 = imagName2 = NULL;
  realNames = imagNames = names2 = 0;
  for (i = longest = 0; i < names; i++) {
    if (strlen(name[i]) > longest)
      longest = strlen(name[i]);
  }
  longest += 10;
  if (!(realPattern = SDDS_Malloc(sizeof(*realPattern) * longest)) ||
      !(imagPattern = SDDS_Malloc(sizeof(*imagPattern) * longest)))
    SDDS_Bomb("Memory allocation failure");

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
          SDDS_Bomb("Invalid type value in expandColumnPairNames");
        realNames = SDDS_MatchColumns(SDDSin, &realName1, SDDS_MATCH_STRING, typeMode, typeValue, realPattern, SDDS_0_PREVIOUS | SDDS_OR);
        imagNames = SDDS_MatchColumns(SDDSin, &imagName1, SDDS_MATCH_STRING, typeMode, typeValue, imagPattern, SDDS_0_PREVIOUS | SDDS_OR);
        break;
      default:
        SDDS_Bomb("Invalid typeMode in expandColumnPairNames");
        exit(EXIT_FAILURE);
        break;
      }
      if (realNames == 0)
        continue;
      if (realNames == -1 || imagNames == -1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Bomb("Unable to perform column name match in expandColumnPairNames");
      }
      if (realNames != imagNames)
        SDDS_Bomb("Found different number of real and imaginary columns");
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
    SDDS_Bomb("Memory allocation failure");
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
