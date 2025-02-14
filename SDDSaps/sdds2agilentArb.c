/**
 * @file sdds2agilentArb.c
 * @brief Converts SDDS files to Agilent Arbitrary Waveform files.
 *
 * @details
 * This program reads an SDDS file, extracts the "I" and "Q" floating-point columns,
 * scales the data to fit the [-32767, 32767] range, and writes the results to an
 * output file in a binary format compatible with Agilent arbitrary waveform generators.
 * The program supports input and output via pipes, little-endian to big-endian
 * conversion, and error handling for invalid options and inputs.
 *
 * @section Usage
 * ```
 * sdds2agilentArb [<inputFile>] [<outputFile>] [-pipe[=in][,out]]
 * ```
 *
 * @section Options
 * | Option                              | Description                                                       |
 * |-------------------------------------|-------------------------------------------------------------------|
 * | `-pipe`                             | Use pipes for input and/or output.                                |
 *
 * @subsection Requirements
 *   - Input file must contain the "I" and "Q" floating-point columns.
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

#include "SDDS.h"
#include "mdb.h"
#include "scan.h"
#if defined(_WIN32)
#  include <io.h>
#  include <fcntl.h>
#endif

/* Enumeration for option types */
enum option_type {
  SET_PIPE,
  N_OPTIONS
};

char *option[N_OPTIONS] =
  {
    "pipe"
  };

char *usage =
  "sdds2agilentArb [<inputFile>] [<outputFile>] [-pipe[=in][,out]]\n\n"
  "Converts SDDS to Agilent Arbitrary Waveform files.\n"
  "Program by Robert Soliday. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char *argv[]) {
  SCANNED_ARG *scanned;
  long i_arg;
  unsigned long pipe_flags = 0;
  char *input = NULL, *output = NULL;
  FILE *fd;
  SDDS_DATASET sdds_dataset;

  long points;
  double *i_wave;
  double *q_wave;
  double max_amp = 0;
  double min_amp = 0;
  int i;
  double scale;
  short *waveform = NULL;
  char buf;
  char *p_char;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s", usage);
    return 1;
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1,
                               scanned[i_arg].n_items - 1, &pipe_flags)) {
          fprintf(stderr, "invalid -pipe syntax\n");
          return 1;
        }
        break;
      default:
        fprintf(stderr, "invalid option seen\n");
        fprintf(stderr, "%s", usage);
        return 1;
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else {
        fprintf(stderr, "too many filenames\n");
        fprintf(stderr, "%s", usage);
        return 1;
      }
    }
  }
  processFilenames("sdds2agilentArb", &input, &output, pipe_flags, 0, NULL);

  if (!SDDS_InitializeInput(&sdds_dataset, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if ((SDDS_CheckColumn(&sdds_dataset, "I", NULL, SDDS_ANY_FLOATING_TYPE,
                        NULL)) != SDDS_CHECK_OKAY) {
    fprintf(stderr, "error: Floating type column named I does not exist\n");
    return 1;
  }
  if ((SDDS_CheckColumn(&sdds_dataset, "Q", NULL, SDDS_ANY_FLOATING_TYPE,
                        NULL)) != SDDS_CHECK_OKAY) {
    fprintf(stderr, "error: Floating type column named Q does not exist\n");
    return 1;
  }

  if (SDDS_ReadTable(&sdds_dataset) != 1) {
    fprintf(stderr, "error: No data found in SDDS file\n");
    return 1;
  }
  points = SDDS_RowCount(&sdds_dataset);

  i_wave = SDDS_GetColumnInDoubles(&sdds_dataset, "I");
  q_wave = SDDS_GetColumnInDoubles(&sdds_dataset, "Q");
  if (!SDDS_Terminate(&sdds_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  max_amp = i_wave[0];
  min_amp = i_wave[0];
  for (i = 0; i < points; i++) {
    if (max_amp < i_wave[i])
      max_amp = i_wave[i];
    else if (min_amp > i_wave[i])
      min_amp = i_wave[i];
    if (max_amp < q_wave[i])
      max_amp = q_wave[i];
    else if (min_amp > q_wave[i])
      min_amp = q_wave[i];
  }
  max_amp = fabs(max_amp);
  min_amp = fabs(min_amp);
  if (min_amp > max_amp)
    max_amp = min_amp;

  scale = 32767 / max_amp;
  waveform = malloc(sizeof(short) * points * 2);
  for (i = 0; i < points; i++) {
    waveform[2 * i] = (short)floor(i_wave[i] * scale + 0.5);
    waveform[2 * i + 1] = (short)floor(q_wave[i] * scale + 0.5);
  }
  free(i_wave);
  free(q_wave);

  if (!SDDS_IsBigEndianMachine()) {
    p_char = (char *)&waveform[0];
    for (i = 0; i < 2 * points; i++) {
      buf = *p_char;
      *p_char = *(p_char + 1);
      *(p_char + 1) = buf;
      p_char += 2;
    }
  }

  if (!output) {
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
      fprintf(stderr, "error: unable to set stdout to binary mode\n");
      exit(1);
    }
#endif
    fd = stdout;
  } else {
    fd = fopen(output, "wb");
  }
  if (fd == NULL) {
    fprintf(stderr, "unable to open output file for writing\n");
    exit(1);
  }
  fwrite((void *)waveform, sizeof(short), points * 2, fd);
  fclose(fd);
  if (waveform)
    free(waveform);
  free_scanargs(&scanned, argc);

  return 0;
}
