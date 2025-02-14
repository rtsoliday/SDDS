/**
 * @file sdds2tiff.c
 * @brief Converts SDDS files to TIFF images.
 *
 * @details
 * This program reads data from an SDDS (Self Describing Data Set) file and generates TIFF images. 
 * Each page in the input file is converted to a separate TIFF image.
 * It supports options for processing pages, specifying data columns, and adjusting the output format.
 *
 * @section Usage
 * ```
 * sdds2tiff <input> <output>
 *           [-pipe[=input]]
 *           [-fromPage=<pageNumber>] 
 *           [-toPage=<pageNumber>]
 *           [-columnPrefix=<Line>]
 *           [-maxContrast]
 *           [-16bit]
 * ```
 *
 * @section Options
 * | Option                   | Description                                                     |
 * |--------------------------|-----------------------------------------------------------------|
 * | `-pipe`                  | Specifies that input should be read from a pipe.                |
 * | `-fromPage`              | Starts conversion from the specified page number.               |
 * | `-toPage`                | Stops conversion at the specified page number.                  |
 * | `-columnPrefix`          | Prefix for column names containing line data.                   |
 * | `-maxContrast`           | Adjusts the image for maximum contrast.                         |
 * | `-16bit`                 | Outputs images in 16-bit depth.                                 |
 *
 * @subsection Incompatibilities
 *   - `-columnPrefix` requires a valid column prefix; otherwise, a default prefix `Line` is used.
 *   - `-16bit` changes the output depth to 16 bits and requires the maximum value in the data to not exceed 65535.
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
 * R. Soliday, M. Borland
 */

#include "tiffio.h"
#include "SDDS.h"
#include "mdb.h"
#include "scan.h"

/* Enumeration for option types */
typedef enum {
  OPT_MAXCONTRAST,
  OPT_FROMPAGE,
  OPT_TOPAGE,
  OPT_COLUMNPREFIX,
  OPT_PIPE,
  OPT_16BIT,
  N_OPTIONS
} option_type;

/* Option names */
static char *option[N_OPTIONS] = {
  "maxcontrast", "frompage", "topage", "columnPrefix", "pipe", "16bit"};

/* Usage message */
static char *USAGE =
  "sdds2tiff [<input>] [<output>] \n"
  "          [-pipe[=input]]\n"
  "          [-fromPage=<pageNumber>] \n"
  "          [-toPage=<pageNumber>]\n"
  "          [-columnPrefix=<Line>]\n"
  "          [-maxContrast]\n"
  "          [-16bit]\n"
  "  Two styles of input files are accepted:\n"
  "  1. A single column SDDS file with Variable1Name and Variable2Name parameters,\n"
  "     as well as <Variable1Name>Dimension and <Variable2Name>Dimension parameters.\n"
  "  2. A file containing multiple columns called Line*.\n"
  "\n"
  "  Each page in the input file will be converted to a separate TIFF image.\n"
  "  The output files will be named <output>.%04ld\n\n"
  "  Program by Robert Soliday. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

/* Main function */
int main(int argc, char *argv[]) {
  SDDS_DATASET SDDS_dataset;
  SCANNED_ARG *s_arg;
  TIFF *tif;
  char **columnNames = NULL;
  char *input = NULL, *output = NULL;
  char *columnPrefix = NULL;
  char *buffer = NULL, *outputName = NULL;
  uint16_t *buffer16 = NULL;
  int32_t **data = NULL;
  long j;
  long bit16 = 0;
  long linesFound = 0;
  int64_t i, k;
  int64_t rows = 0;
  int32_t nColumns = 0;
  long maxvalue = 0;
  long page = 1;
  long index = 1;
  long fromPage = 0;
  long toPage = 0;
  int maxContrast = 0;
  double div = 1;
  char *xVar = NULL, *yVar = NULL;
  char zColumnName[40];
  char xDimName[40], yDimName[40];
  int64_t xDim;
  int32_t yDim;
  long style = 1;
  unsigned long pipeFlags = 0;
  long maxPossibleLong = 255;
  double maxPossible = maxPossibleLong;

  /* Register program name */
  SDDS_RegisterProgramName(argv[0]);

  /* Process arguments */
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s", USAGE);
    return 1;
  }

  /* Parse options */
  for (i = 1; i < argc; i++) {
    if (s_arg[i].arg_type == OPTION) {
      switch (match_string(s_arg[i].list[0], option, N_OPTIONS, 0)) {
      case OPT_MAXCONTRAST:
        maxContrast = 1;
        break;
      case OPT_FROMPAGE:
        if (s_arg[i].n_items < 2) {
          SDDS_Bomb("invalid -fromPage syntax");
        }
        if (sscanf(s_arg[i].list[1], "%ld", &fromPage) != 1 || fromPage <= 0) {
          SDDS_Bomb("invalid -fromPage syntax or value");
        }
        break;
      case OPT_TOPAGE:
        if (s_arg[i].n_items < 2) {
          SDDS_Bomb("invalid -toPage syntax");
        }
        if (sscanf(s_arg[i].list[1], "%ld", &toPage) != 1 || toPage <= 0) {
          SDDS_Bomb("invalid -toPage syntax or value");
        }
        break;
      case OPT_COLUMNPREFIX:
        if (s_arg[i].n_items < 2) {
          SDDS_Bomb("invalid -columnPrefix syntax");
        }
        SDDS_CopyString(&columnPrefix, s_arg[i].list[1]);
        break;
      case OPT_PIPE:
        if (!processPipeOption(s_arg[i].list + 1, s_arg[i].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "invalid -pipe syntax\n");
          return 1;
        }
        break;
      case OPT_16BIT:
        bit16 = 1;
        maxPossible = maxPossibleLong = 65535;
        break;
      default:
        fprintf(stderr, "sdds2tiff: invalid option seen\n%s", USAGE);
        return 1;
      }
    } else {
      if (!input) {
        SDDS_CopyString(&input, s_arg[i].list[0]);
      } else if (!output) {
        SDDS_CopyString(&output, s_arg[i].list[0]);
      } else {
        fprintf(stderr, "sdds2tiff: too many filenames\n%s", USAGE);
        return 1;
      }
    }
  }

  if (fromPage && toPage && fromPage > toPage) {
    SDDS_Bomb("invalid -fromPage and -toPage");
  }

  if (!columnPrefix) {
    columnPrefix = malloc(5 * sizeof(char));
    sprintf(columnPrefix, "Line");
  }

  if (pipeFlags & USE_STDIN) {
    processFilenames("sdds2tiff", &input, &output, USE_STDIN, 1, NULL);
  }
  if (!SDDS_InitializeInput(&SDDS_dataset, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  outputName = malloc((strlen(output) + 10) * sizeof(char));

  /* Check required parameters */
  if (SDDS_CheckParameter(&SDDS_dataset, "Variable1Name", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OKAY) {
    style = 2;
  }
  if (SDDS_CheckParameter(&SDDS_dataset, "Variable2Name", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OKAY) {
    style = 2;
  }

  columnNames = SDDS_GetColumnNames(&SDDS_dataset, &nColumns);
  if (!columnNames) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /* Handle different styles */
  if (style == 1) {
    if (nColumns != 1) {
      fprintf(stderr, "sdds2tiff: Expected one column but found more than one\n");
      return 1;
    }
    sprintf(zColumnName, "%s", columnNames[0]);
  } else if (style == 2) {
    for (i = 0; i < nColumns; i++) {
      if (strncmp(columnPrefix, columnNames[i], strlen(columnPrefix)) == 0) {
        linesFound++;
      }
    }
    if (linesFound == 0) {
      fprintf(stderr, "sdds2tiff: No columns found named %s*\n", columnPrefix);
      return 1;
    }
    data = malloc(linesFound * sizeof(*data));
  }

  /* Process each page */
  while (SDDS_ReadTable(&SDDS_dataset) > 0) {
    if ((fromPage > 0 && fromPage > page) || (toPage > 0 && toPage < page)) {
      continue;
    }

    rows = SDDS_RowCount(&SDDS_dataset);

    if (style == 1) {
      if (!SDDS_GetParameter(&SDDS_dataset, "Variable1Name", &xVar)) {
        fprintf(stderr, "sdds2tiff: problem getting parameter Variable1Name\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!SDDS_GetParameter(&SDDS_dataset, "Variable2Name", &yVar)) {
        fprintf(stderr, "sdds2tiff: problem getting parameter Variable2Name\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      sprintf(xDimName, "%sDimension", xVar);
      sprintf(yDimName, "%sDimension", yVar);
      if (!SDDS_GetParameterAsLong64(&SDDS_dataset, xDimName, &xDim)) {
        fprintf(stderr, "sdds2tiff: problem getting parameter %s\n", xDimName);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!SDDS_GetParameterAsLong(&SDDS_dataset, yDimName, &yDim)) {
        fprintf(stderr, "sdds2tiff: problem getting parameter %s\n", yDimName);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      fprintf(stderr, "%s %s\n", xVar, yVar);
      fprintf(stderr, "%" PRId64 " %" PRId32 "\n", xDim, yDim);
      free(xVar);
      free(yVar);

      if (xDim * yDim != rows) {
        fprintf(stderr, "sdds2tiff: %s * %s does not equal the number of rows in the page\n", xDimName, yDimName);
        return 1;
      }

      data = malloc(sizeof(*data));
      data[0] = SDDS_GetColumnInLong(&SDDS_dataset, zColumnName);
      for (i = 0; i < rows; i++) {
        if (data[0][i] > maxvalue) {
          maxvalue = data[0][i];
        }
      }

      if (!bit16) {
        buffer = malloc(rows * sizeof(char));
      } else {
        buffer16 = malloc(rows * sizeof(int16_t));
      }
    } else if (style == 2) {
      j = 0;
      for (i = 0; i < nColumns; i++) {
        if (strncmp(columnPrefix, columnNames[i], strlen(columnPrefix)) == 0) {
          data[j] = SDDS_GetColumnInLong(&SDDS_dataset, columnNames[i]);
          for (k = 0; k < rows; k++) {
            if (data[j][k] > maxvalue) {
              maxvalue = data[j][k];
            }
          }
          j++;
        }
      }

      if (!bit16) {
        buffer = malloc(rows * linesFound * sizeof(char));
      } else {
        buffer16 = malloc(rows * linesFound * sizeof(uint16_t));
      }

      xDim = rows;
      yDim = linesFound;
    }

    if (maxContrast) {
      div = maxvalue / maxPossible;
    } else if (maxvalue <= maxPossibleLong) {
      div = 1;
    } else if (maxvalue <= 3 * maxPossibleLong) {
      div = 3;
    } else {
      div = maxvalue / maxPossible;
    }

    sprintf(outputName, "%s.%04ld", output, index);
    tif = TIFFOpen(outputName, "w");
    if (tif) {
      TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, xDim);
      TIFFSetField(tif, TIFFTAG_IMAGELENGTH, yDim);
      TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bit16 ? 16 : 8);
      TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
      TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, yDim);
      TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
      TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
      TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
      TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
      TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);

      if (style == 1) {
        k = 0;
        for (i = 0; i < xDim; i++) {
          for (j = 0; j < yDim; j++) {
            if (!bit16) {
              buffer[xDim * (yDim - (j + 1)) + i] = (unsigned int)(round(data[0][k] / div));
            } else {
              buffer16[xDim * (yDim - (j + 1)) + i] = (uint16_t)(round(data[0][k] / div));
            }
            k++;
          }
        }
        TIFFWriteEncodedStrip(tif, 0, bit16 ? (char *)buffer16 : buffer, rows * (bit16 ? 2 : 1));
      } else if (style == 2) {
        for (j = 0; j < yDim; j++) {
          for (i = 0; i < xDim; i++) {
            if (!bit16) {
              buffer[j * xDim + i] = (unsigned int)(round(data[(yDim - j) - 1][i] / div));
            } else {
              buffer16[j * xDim + i] = (uint16_t)(round(data[(yDim - j) - 1][i] / div));
            }
          }
        }
        TIFFWriteEncodedStrip(tif, 0, bit16 ? (char *)buffer16 : buffer, xDim * yDim * (bit16 ? 2 : 1));
      }
      TIFFClose(tif);
    }

    if (buffer) {
      free(buffer);
    }
    if (buffer16) {
      free(buffer16);
    }
    if (style == 1) {
      free(data[0]);
      free(data);
    } else if (style == 2) {
      for (j = 0; j < yDim; j++) {
        free(data[j]);
      }
    }

    index++;
  }

  if (!SDDS_Terminate(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (input) {
    free(input);
  }
  if (output) {
    free(output);
  }
  if (outputName) {
    free(outputName);
  }

  for (i = 0; i < nColumns; i++) {
    if (columnNames[i]) {
      free(columnNames[i]);
    }
  }

  if (columnNames) {
    free(columnNames);
  }

  if (style == 2 && data) {
    free(data);
  }

  free_scanargs(&s_arg, argc);
  return 0;
}
