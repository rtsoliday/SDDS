/**
 * @file sddsconvolve.c
 * @brief Perform convolution, deconvolution, and correlation operations using the SDDS library.
 *
 * @details
 * This program handles discrete Fourier convolution, deconvolution, and correlation between signal and response files.
 * It assumes that the input files have uniform spacing of points and an equal number of data points.
 *
 * ### Features:
 * - Convolution: \( O = S * R \)
 * - Deconvolution: \( O = S / R \)
 * - Correlation: \( O = S * Conj(R) \)
 *
 * It also supports options for Wiener filtering and noise handling during deconvolution.
 *
 * @section Usage
 * ```
 * sddsconvolve <signal-file> <response-file> <output>
 *              [-pipe=[input][,output]]
 *              [-signalColumns=<indepColumn>,<dataName>]
 *              [-responseColumns=<indepColumn>,<dataName>]
 *              [-outputColumns=<indepColumn>,<dataName>]
 *              [-reuse]
 *              [-majorOrder=row|column]
 *              [-deconvolve]
 *              [-noiseFraction=value]
 *              [-wienerFilter=value]
 *              [-correlate]
 * ```
 *
 * @section Options
 * | Option                                | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Use standard input/output in place of the signal file and output file.                |
 * | `-signalColumns`                      | Specify the independent column and data name for the signal file.                   |
 * | `-responseColumns`                    | Specify the independent column and data name for the response file.               |
 * | `-outputColumns`                      | Specify the independent column and data name for the output file.                 |
 * | `-reuse`                              | Reuse the first page of the response file for each page of the signal file.          |
 * | `-majorOrder`                         | Set data ordering in the output file.                                                |
 * | `-deconvolve`                         | Perform deconvolution instead of convolution.                                        |
 * | `-noiseFraction`                      | Specify noise fraction to prevent divide-by-zero errors during deconvolution.        |
 * | `-wienerFilter`                       | Apply a Wiener filter with the specified fraction during deconvolution.              |
 * | `-correlate`                          | Perform correlation instead of convolution.                                          |
 *
 * @subsection Incompatibilities
 *   - `-deconvolve` is incompatible with:
 *     - `-correlate`
 *   - For `-deconvolve`:
 *     - At least one of `-noiseFraction` or `-wienerFilter` must be specified.
 *   - Only one of the following may be specified:
 *     - `-noiseFraction=<value>`
 *     - `-wienerFilter=<value>`
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
 * Michael Borland, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "fftpackC.h"

/* Enumeration for option types */
enum option_type {
  CLO_DECONVOLVE,
  CLO_PIPE,
  CLO_NOISE_FRACTION,
  CLO_CORRELATE,
  CLO_INDEPENDENTCOLUMN,
  CLO_SIGNALCOLUMN,
  CLO_RESPONSECOLUMN,
  CLO_OUTPUTCOLUMN,
  CLO_WIENER_FILTER,
  CLO_MAJOR_ORDER,
  CLO_REUSE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "deconvolve",
  "pipe",
  "noisefraction",
  "correlate",
  "independentcolumn",
  "signalcolumn",
  "responsecolumn",
  "outputcolumn",
  "wienerfilter",
  "majorOrder",
  "reuse",
};

#define USAGE "sddsconvolve <signal-file> <response-file> <output>\n\
 [-pipe=[input][,output]]\n\
  -signalColumns=<indepColumn>,<dataName>\n\
  -responseColumns=<indepColumn>,<dataName>\n\
  -outputColumns=<indepColumn>,<dataName>\n\
  [-reuse] [-majorOrder=row|column]\n\
  [{-deconvolve [{-noiseFraction=<value> | -wienerFilter=<value>}] | -correlate}]\n\n\
Description:\n\
  Performs discrete Fourier convolution, deconvolution, or correlation between signal and response files.\n\
  Assumes uniform spacing of points in both input files and that both files contain the same number of data points.\n\
\n\
Mathematical Operations:\n\
  - Convolution:      O = S * R\n\
  - Deconvolution:    O = S / R\n\
  - Correlation:      O = S * Conj(R)\n\
\n\
Options:\n\
  -signalColumns=<indepColumn>,<dataName>     Specify the independent column and data name for the signal file.\n\
  -responseColumns=<indepColumn>,<dataName>   Specify the independent column and data name for the response file.\n\
  -outputColumns=<indepColumn>,<dataName>     Specify the independent column and data name for the output file.\n\
  -reuse                                      Reuse the first page of the response file for each page of the signal file.\n\
  -majorOrder=row|column                      Set data ordering in the output file.\n\
  -deconvolve                                 Perform deconvolution instead of convolution.\n\
    -noiseFraction=<value>                    Specify noise fraction to prevent divide-by-zero.\n\
    -wienerFilter=<value>                     Apply a Wiener filter with the specified fraction.\n\
  -correlate                                  Perform correlation instead of convolution.\n\
  -pipe=[input][,output]                      Use standard input/output in place of the signal file and output file.\n\
\n\
Program Information:\n\
  Michael Borland (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n"

void complex_multiply(double *r0, double *i0, double r1, double i1, double r2, double i2);
void complex_divide(double *r0, double *i0, double r1, double i1, double r2, double i2, double threshold);
void wrap_around_order(double *response1, double *t, double *response, int64_t nres, int64_t nsig);

#define MODE_CONVOLVE 1
#define MODE_DECONVOLVE 2
#define MODE_CORRELATE 3

int main(int argc, char **argv) {
  SDDS_DATASET SDDS1, SDDS2, SDDSout;
  int i_arg;
  SCANNED_ARG *scanned;
  char *input1, *input2, *output;
  long mode, doWiener;
  double *fft_sig, *fft_res, noise, mag2, threshold, range;
  double *signal1, *signal2, *indep1, *indep2;
  double WienerFraction, *WienerFilter = NULL;
  unsigned long pipeFlags, majorOrderFlag;
  char *input1Column[2], *input2Column[2], *outputColumn[2];
  char description[1024];
  long tmpfile_used, code1, code2;
  int64_t i, rows1, rows2, nfreq;
  int32_t parameters = 0;
  char **parameterName = NULL;
  short columnMajorOrder = -1, reuse = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 4 || argc > (4 + N_OPTIONS))
    bomb(NULL, USAGE);

  input1 = input2 = output = NULL;
  mode = MODE_CONVOLVE;
  noise = 1e-14;
  input1Column[0] = input1Column[1] = NULL;
  input2Column[0] = input2Column[1] = NULL;
  outputColumn[0] = outputColumn[1] = NULL;
  pipeFlags = 0;
  doWiener = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* process options here */
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[i_arg].n_items--;
        if (scanned[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[i_arg].list + 1, &scanned[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("Invalid -majorOrder syntax or values.");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_DECONVOLVE:
        mode = MODE_DECONVOLVE;
        break;
      case CLO_CORRELATE:
        mode = MODE_CORRELATE;
        break;
      case CLO_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax.");
        break;
      case CLO_NOISE_FRACTION:
        if (scanned[i_arg].n_items != 2 || sscanf(scanned[i_arg].list[1], "%lf", &noise) != 1 || noise <= 0)
          SDDS_Bomb("Invalid -noisefraction syntax or value.");
        break;
      case CLO_WIENER_FILTER:
        if (scanned[i_arg].n_items != 2 || sscanf(scanned[i_arg].list[1], "%lf", &WienerFraction) != 1 ||
            WienerFraction <= 0 || WienerFraction >= 1)
          SDDS_Bomb("Invalid -wienerfilter syntax or value.");
        doWiener = 1;
        break;
      case CLO_SIGNALCOLUMN:
        if (scanned[i_arg].n_items != 3 ||
            !strlen(input1Column[0] = scanned[i_arg].list[1]) ||
            !strlen(input1Column[1] = scanned[i_arg].list[2]))
          SDDS_Bomb("Invalid -signalColumns syntax.");
        break;
      case CLO_RESPONSECOLUMN:
        if (scanned[i_arg].n_items != 3 ||
            !strlen(input2Column[0] = scanned[i_arg].list[1]) ||
            !strlen(input2Column[1] = scanned[i_arg].list[2]))
          SDDS_Bomb("Invalid -responseColumns syntax.");
        break;
      case CLO_OUTPUTCOLUMN:
        if (scanned[i_arg].n_items != 3 ||
            !strlen(outputColumn[0] = scanned[i_arg].list[1]) ||
            !strlen(outputColumn[1] = scanned[i_arg].list[2]))
          SDDS_Bomb("Invalid -outputColumns syntax.");
        break;
      case CLO_REUSE:
        reuse = 1;
        break;
      default:
        SDDS_Bomb("Unknown option provided.");
        break;
      }
    } else {
      if (!input1)
        input1 = scanned[i_arg].list[0];
      else if (!input2)
        input2 = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else
        SDDS_Bomb("Too many filenames provided.");
    }
  }

  if (pipeFlags & USE_STDIN && input1) {
    if (output)
      SDDS_Bomb("Too many filenames provided.");
    output = input2;
    input2 = input1;
    input1 = NULL;
  }
  if (!input1Column[0] || !input1Column[1] || !strlen(input1Column[0]) || !strlen(input1Column[1]))
    SDDS_Bomb("SignalColumns not provided.");
  if (!input2Column[0] || !input2Column[1] || !strlen(input2Column[0]) || !strlen(input2Column[1]))
    SDDS_Bomb("ResponseColumns not provided.");
  if (!outputColumn[0] || !outputColumn[1] || !strlen(outputColumn[0]) || !strlen(outputColumn[1]))
    SDDS_Bomb("OutputColumns not provided.");

  processFilenames("sddsconvolve", &input1, &output, pipeFlags, 1, &tmpfile_used);
  if (!input2)
    SDDS_Bomb("Second input file not specified.");

  if (!SDDS_InitializeInput(&SDDS1, input1) || !SDDS_InitializeInput(&SDDS2, input2)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  switch (mode) {
  case MODE_CONVOLVE:
    sprintf(description, "Convolution of signal '%s' with response '%s'", input1Column[1], input2Column[1]);
    break;
  case MODE_DECONVOLVE:
    sprintf(description, "Deconvolution of signal '%s' with response '%s'", input1Column[1], input2Column[1]);
    break;
  case MODE_CORRELATE:
    sprintf(description, "Correlation of signal '%s' with response '%s'", input1Column[1], input2Column[1]);
    break;
  }

  parameterName = SDDS_GetParameterNames(&SDDS1, &parameters);
  if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 1, NULL, NULL, output) ||
      !SDDS_TransferColumnDefinition(&SDDSout, &SDDS1, input1Column[0], outputColumn[0]) ||
      0 > SDDS_DefineColumn(&SDDSout, outputColumn[1], NULL, NULL, description, NULL, SDDS_DOUBLE, 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (columnMajorOrder != -1)
    SDDSout.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout.layout.data_mode.column_major = SDDS1.layout.data_mode.column_major;

  if (parameters) {
    for (i = 0; i < parameters; i++)
      if (!SDDS_TransferParameterDefinition(&SDDSout, &SDDS1, parameterName[i], parameterName[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_WriteLayout(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  code2 = -1;
  while ((code1 = SDDS_ReadPage(&SDDS1)) > 0) {
    if ((rows1 = SDDS_RowCount(&SDDS1)) <= 0) {
      fprintf(stderr, "Warning (sddsconvolve): Skipping page due to no signal rows.\n");
      continue;
    }
    if (reuse) {
      if (code2 == -1) {
        if ((code2 = SDDS_ReadPage(&SDDS2)) <= 0) {
          fprintf(stderr, "Error (sddsconvolve): Couldn't read data from response file.\n");
          exit(EXIT_FAILURE);
        }
        if ((rows2 = SDDS_RowCount(&SDDS2)) < 0) {
          fprintf(stderr, "Error (sddsconvolve): Response file has zero rows on first page.\n");
          exit(EXIT_FAILURE);
        }
      }
    } else {
      if ((code2 = SDDS_ReadPage(&SDDS2)) <= 0)
        break;
      rows2 = SDDS_RowCount(&SDDS2);
    }
    if (rows1 != rows2)
      SDDS_Bomb("Different numbers of points for signal and response.");

    if (!(signal1 = SDDS_GetColumnInDoubles(&SDDS1, input1Column[1])) ||
        !(indep1 = SDDS_GetColumnInDoubles(&SDDS1, input1Column[0])) ||
        !(signal2 = SDDS_GetColumnInDoubles(&SDDS2, input2Column[1])) ||
        !(indep2 = SDDS_GetColumnInDoubles(&SDDS2, input2Column[0]))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }

    if (!(fft_sig = SDDS_Calloc(sizeof(*fft_sig), (2 * rows1 + 2))) ||
        !(fft_res = SDDS_Calloc(sizeof(*fft_res), (2 * rows1 + 2)))) {
      SDDS_SetError("Memory allocation failure.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }

    /* Rearrange response for FFT */
    wrap_around_order(fft_res, indep2, signal2, rows2, rows1);
    for (i = 0; i < rows1; i++)
      fft_sig[i] = signal1[i];

    /* Perform FFT on signal and response */
    realFFT2(fft_sig, fft_sig, 2 * rows1, 0);
    realFFT2(fft_res, fft_res, 2 * rows1, 0);
    nfreq = rows1 + 1;
    range = 2 * rows1 * (indep1[rows1 - 1] - indep1[0]) / (rows1 - 1);

    if (mode == MODE_CONVOLVE || mode == MODE_CORRELATE) {
      if (mode == MODE_CORRELATE)
        /* Take complex conjugate of response FFT */
        for (i = 0; i < nfreq; i++)
          fft_res[2 * i + 1] = -fft_res[2 * i + 1];

      /* Multiply FFTs */
      for (i = 0; i < nfreq; i++) {
        complex_multiply(&fft_sig[2 * i], &fft_sig[2 * i + 1],
                         fft_sig[2 * i], fft_sig[2 * i + 1],
                         fft_res[2 * i], fft_res[2 * i + 1]);
      }

      /* Inverse FFT */
      realFFT2(fft_sig, fft_sig, 2 * rows1, INVERSE_FFT);

      /* Apply normalization factor */
      for (i = 0; i < rows1; i++)
        fft_sig[i] *= range;

      /* Write output */
      if (!SDDS_StartPage(&SDDSout, rows1) ||
          !SDDS_CopyParameters(&SDDSout, &SDDS1) ||
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, fft_sig, rows1, outputColumn[1]) ||
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, indep1, rows1, outputColumn[0]) ||
          !SDDS_WritePage(&SDDSout)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    } else if (mode == MODE_DECONVOLVE) {
      double maxMag2;
      /* Calculate maximum magnitude squared */
      for (i = threshold = 0; i < nfreq; i++) {
        if ((mag2 = sqr(fft_res[2 * i]) + sqr(fft_res[2 * i + 1])) > threshold)
          threshold = mag2;
      }
      maxMag2 = threshold;
      threshold = threshold * noise;

      if (doWiener) {
        /* Compute and apply Wiener filter */
        double *S = NULL, *N = NULL, wThreshold;
        if (!(WienerFilter = malloc(sizeof(*WienerFilter) * nfreq)) ||
            !(S = malloc(sizeof(*S) * nfreq)) ||
            !(N = malloc(sizeof(*N) * nfreq))) {
          SDDS_Bomb("Memory allocation failure.");
        }
        wThreshold = maxMag2 * sqr(WienerFraction);
        for (i = 0; i < nfreq; i++) {
          S[i] = sqrt(sqr(fft_res[2 * i]) + sqr(fft_res[2 * i + 1]));
          if (sqr(S[i]) < wThreshold) {
            N[i] = S[i];
            S[i] = 0;
          } else {
            S[i] = S[i] - sqrt(wThreshold);
            N[i] = sqrt(wThreshold);
          }
        }
        for (i = 0; i < nfreq; i++)
          WienerFilter[i] = sqr(S[i]) / (sqr(S[i]) + sqr(N[i]) + threshold);
        free(N);
        free(S);
      }

      /* Perform division in frequency domain */
      for (i = 0; i < nfreq; i++) {
        complex_divide(&fft_sig[2 * i], &fft_sig[2 * i + 1],
                       fft_sig[2 * i], fft_sig[2 * i + 1],
                       fft_res[2 * i], fft_res[2 * i + 1],
                       threshold);
      }

      if (doWiener) {
        for (i = 0; i < nfreq; i++) {
          fft_sig[2 * i] *= WienerFilter[i];
          fft_sig[2 * i + 1] *= WienerFilter[i];
        }
        free(WienerFilter);
      }

      /* Inverse FFT */
      realFFT2(fft_sig, fft_sig, 2 * rows1, INVERSE_FFT);

      /* Apply normalization factor */
      for (i = 0; i < rows1; i++)
        fft_sig[i] = fft_sig[i] / range;

      /* Write output */
      if (!SDDS_StartPage(&SDDSout, rows1) ||
          !SDDS_CopyParameters(&SDDSout, &SDDS1) ||
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, fft_sig, rows1, outputColumn[1]) ||
          !SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, indep1, rows1, outputColumn[0]) ||
          !SDDS_WritePage(&SDDSout)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    } else {
      SDDS_Bomb("Unexpected processing mode encountered.");
    }

    /* Free allocated memory for this iteration */
    free(fft_sig);
    fft_sig = NULL;
    free(fft_res);
    fft_res = NULL;
    free(signal1);
    free(indep1);
    free(signal2);
    free(indep2);
    signal1 = indep1 = signal2 = indep2 = NULL;
  }

  if (!SDDS_Terminate(&SDDS1) || !SDDS_Terminate(&SDDS2) || !SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (tmpfile_used && !replaceFileAndBackUp(input1, output))
    exit(EXIT_FAILURE);

  free(parameterName);
  free_scanargs(&scanned, argc);

  return EXIT_SUCCESS;
}

void wrap_around_order(double *response1, double *t, double *response, int64_t nres, int64_t nsig) {
  int64_t i;
  int64_t iz;

  for (iz = 0; iz < nres; iz++)
    if (t[iz] >= 0)
      break;
  if (iz == nres)
    bomb("Response function is acausal.", NULL);

  fill_double_array(response1, 2 * nsig + 2, 0.0L);
  for (i = iz; i < nres; i++)
    response1[i - iz] = response[i];
  for (i = 0; i < iz; i++)
    response1[2 * nsig - (iz - i)] = response[i];
}
