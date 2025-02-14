/**
 * @file agilentArb2sdds.c
 * @brief Converts Agilent Arbitrary Waveform files to SDDS format.
 *
 * @details
 * This program reads Agilent Arbitrary Waveform files and converts them into
 * SDDS (Self Describing Data Set) format. It supports various output options
 * such as ASCII or binary formats, inclusion of an index column, and output
 * precision (float or double).
 *
 * @section Usage
 * ```
 * agilentArb2sdds [<inputFile>] [<outputFile>]
 *                 [-pipe[=in][,out]]
 *                 [-ascii | -binary]
 *                 [-withIndex]
 *                 [-float | -double]
 *                 [-dumpHeader]
 * ```
 *
 * @section Options
 * | Optional | Description                                                       |
 * |----------|-------------------------------------------------------------------|
 * | `-pipe`  | Enable pipe mode with optional input and output pipes.            |
 * | `-ascii` | Request SDDS ASCII output. Default unless `-binary` is specified. |
 * | `-binary`| Request SDDS binary output.                                       |
 * | `-withIndex`| Add an Index column to the output.                             |
 * | `-float` | Output data in float format. Default is double precision.         |
 * | `-double`| Output data in double format.                                     |
 * | `-dumpHeader`| Output SDDS header information.                               |
 *
 * @subsection Incompatibilities
 *   - `-ascii` is incompatible with `-binary`.
 *   - `-float` is incompatible with `-double`.
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
 * R. Soliday
 */

#include <mdb.h>
#include <SDDS.h>
#include <scan.h>
#if defined(_WIN32)
#  include <io.h>
#  include <fcntl.h>
#endif

/* Enumeration for option types */
enum option_type {
  SET_ASCII,
  SET_BINARY,
  SET_PIPE,
  SET_WITHINDEX,
  SET_FLOAT,
  SET_DOUBLE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "ascii",
  "binary",
  "pipe",
  "withindex",
  "float",
  "double"
};

char *USAGE =
  "Usage: agilentArb2sdds [<inputFile>] [<outputFile>]\n"
  "                       [-pipe[=in][,out]]\n"
  "                       [-ascii | -binary]\n"
  "                       [-withIndex]\n"
  "                       [-float | -double]\n"
  "                       [-dumpHeader]\n"
  "Options:\n"
  "  -pipe[=in][,out]    Enable pipe mode with optional input and output pipes.\n"
  "  -ascii              Request SDDS ASCII output. Default is binary.\n"
  "  -binary             Request SDDS BINARY output.\n"
  "  -withIndex          Add an Index column to the output.\n"
  "  -float              Output data in float format. Default is double.\n"
  "  -double             Output data in double format.\n"
  "  -dumpHeader         \n\n"
  "Converts Agilent Arbitrary Waveform files to SDDS.\n"
  "Program by Robert Soliday. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define MAX_NUM_POINTS 100001

int main(int argc, char *argv[]) {
  SCANNED_ARG *scanned;
  long iArg;
  int ascii = 0, withIndex = 0, floatValues = 0;
  unsigned long pipeFlags = 0;
  char *input = NULL, *output = NULL;
  FILE *fd;
  SDDS_DATASET SDDSout;

  long points;
  short waveform[2 * MAX_NUM_POINTS];
  char *pChar;
  char buf;
  int i;
  double *IwaveIn;
  double *QwaveIn;
  int32_t *index = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  for (iArg = 1; iArg < argc; iArg++) {
    if (scanned[iArg].arg_type == OPTION) {
      switch (match_string(scanned[iArg].list[0], option, N_OPTIONS, 0)) {
      case SET_ASCII:
        ascii = 1;
        break;
      case SET_BINARY:
        ascii = 0;
        break;
      case SET_WITHINDEX:
        withIndex = 1;
        break;
      case SET_FLOAT:
        floatValues = 1;
        break;
      case SET_DOUBLE:
        floatValues = 0;
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "Error: Invalid -pipe syntax.\n");
          fprintf(stderr, "%s", USAGE);
          return EXIT_FAILURE;
        }
        break;
      default:
        fprintf(stderr, "Error: Invalid option '%s'.\n", scanned[iArg].list[0]);
        fprintf(stderr, "%s", USAGE);
        return EXIT_FAILURE;
      }
    } else {
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else {
        fprintf(stderr, "Error: Too many filenames provided.\n");
        fprintf(stderr, "%s", USAGE);
        return EXIT_FAILURE;
      }
    }
  }

  processFilenames("agilentArb2sdds", &input, &output, pipeFlags, 0, NULL);

  if (input) {
    if (!fexists(input)) {
      fprintf(stderr, "Error: Input file '%s' not found.\n", input);
      return EXIT_FAILURE;
    }
    if (!(fd = fopen(input, "rb"))) {
      fprintf(stderr, "Error: Unable to open input file '%s'.\n", input);
      return EXIT_FAILURE;
    }
  } else {
#if defined(_WIN32)
    if (_setmode(_fileno(stdin), _O_BINARY) == -1) {
      fprintf(stderr, "Error: Unable to set stdin to binary mode.\n");
      return EXIT_FAILURE;
    }
#endif
    fd = stdin;
  }

  points = fread((void *)waveform, sizeof(short), MAX_NUM_POINTS * 2, fd);
  if (points == MAX_NUM_POINTS * 2) {
    fprintf(stderr, "Error: Number of points in the waveform exceeds the maximum (%d).\n", MAX_NUM_POINTS);
    return EXIT_FAILURE;
  }
  fclose(fd);

  if (!SDDS_IsBigEndianMachine()) {
    pChar = (char *)&waveform[0];
    for (i = 0; i < points; i++) {
      buf = *pChar;
      *pChar = *(pChar + 1);
      *(pChar + 1) = buf;
      pChar += 2;
    }
  }

  points = points / 2;
  IwaveIn = malloc(sizeof(double) * points);
  QwaveIn = malloc(sizeof(double) * points);
  if (!IwaveIn || !QwaveIn) {
    fprintf(stderr, "Error: Memory allocation failed for waveform data.\n");
    return EXIT_FAILURE;
  }

  for (i = 0; i < points; i++) {
    IwaveIn[i] = waveform[2 * i] / 32767.0;
    QwaveIn[i] = waveform[2 * i + 1] / 32767.0;
  }

  if (withIndex) {
    index = malloc(sizeof(int32_t) * points);
    if (!index) {
      fprintf(stderr, "Error: Memory allocation failed for index data.\n");
      return EXIT_FAILURE;
    }
    for (i = 0; i < points; i++) {
      index[i] = i;
    }
  }

  if (!SDDS_InitializeOutput(&SDDSout, ascii ? SDDS_ASCII : SDDS_BINARY, 1, NULL, NULL, output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (withIndex) {
    if (!SDDS_DefineSimpleColumn(&SDDSout, "Index", NULL, SDDS_LONG)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
  }

  if (!SDDS_DefineSimpleColumn(&SDDSout, "I", NULL, floatValues ? SDDS_FLOAT : SDDS_DOUBLE)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_DefineSimpleColumn(&SDDSout, "Q", NULL, floatValues ? SDDS_FLOAT : SDDS_DOUBLE)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_WriteLayout(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_StartTable(&SDDSout, points)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (withIndex) {
    if (!SDDS_SetColumnFromLongs(&SDDSout, SDDS_SET_BY_NAME, index, points, "Index")) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
  }

  if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, IwaveIn, points, "I")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_SetColumnFromDoubles(&SDDSout, SDDS_SET_BY_NAME, QwaveIn, points, "Q")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_WriteTable(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (withIndex)
    free(index);
  free_scanargs(&scanned, argc);
  free(IwaveIn);
  free(QwaveIn);

  return EXIT_SUCCESS;
}
