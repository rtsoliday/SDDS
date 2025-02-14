/**
 * @file tiff2sdds.c
 * @brief Converts a TIFF image to an SDDS file format.
 *
 * @details
 * This program reads a TIFF image file and converts its pixel data into the SDDS (Self Describing Data Set) format.
 * It supports extracting specific color channels and arranging the data in a single column or multiple columns
 * based on user options. The input is a TIFF image in RGBA format, and the output is an SDDS binary file.
 *
 * @section Usage
 * ```
 * tiff2sdds <inputfile> <outputfile>
 *           [-redOnly] 
 *           [-greenOnly] 
 *           [-blueOnly] 
 *           [-singleColumnMode]
 * ```
 *
 * @section Options
 * | Optional          | Description                                              |
 * |-------------------|----------------------------------------------------------|
 * | `-redOnly`        | Extracts the red channel only.                           |
 * | `-greenOnly`      | Extracts the green channel only.                         |
 * | `-blueOnly`       | Extracts the blue channel only.                          |
 * | `-singleColumnMode` | Outputs pixel data in a single column.                  |
 *
 * @subsection Incompatibilities
 *   - Only one of `-redOnly`, `-greenOnly`, or `-blueOnly` may be specified.
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

#include "tiffio.h"
#include "SDDS.h"
#include "mdb.h"
#include "scan.h"

/*
 * Enumeration for supported command-line options.
 */
typedef enum {
  OPT_REDONLY,          /* Option for extracting red channel only. */
  OPT_GREENONLY,        /* Option for extracting green channel only. */
  OPT_BLUEONLY,         /* Option for extracting blue channel only. */
  OPT_SINGLECOLUMNMODE, /* Option for single column mode in output. */
  N_OPTIONS             /* Total number of options. */
} OptionType;

/*
 * Array of option strings corresponding to the enumeration.
 */
char *option[N_OPTIONS] =
  {
    "redOnly",
    "greenOnly",
    "blueOnly",
    "singleColumnMode",
  };

/*
 * Usage message displayed for invalid or missing arguments.
 */
char *usage =
  "tiff2sdds <input> <output>\n"
  "  [-redOnly] [-greenOnly] [-blueOnly]\n"
  "  [-singleColumnMode]\n"
  "Program by Robert Soliday. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n\n";

int main(int argc, char *argv[]) {
  SDDS_DATASET SDDS_dataset;
  SCANNED_ARG *s_arg;
  TIFF *tif;
  char *input = NULL, *output = NULL;
  char **column_names = NULL;
  int32_t **data;
  int32_t *indexes = NULL;
  long rgb[3];
  long i, j, n = 0, single_column_mode = 0;

  SDDS_RegisterProgramName(argv[0]);

  rgb[0] = rgb[1] = rgb[2] = 1; /* Enable all color channels by default. */

  argc = scanargs(&s_arg, argc, argv); /* Parse command-line arguments. */
  if (argc < 3) {
    fprintf(stderr, "%s", usage);
    return 1;
  }

  for (i = 1; i < argc; i++) {
    if (s_arg[i].arg_type == OPTION) {
      switch (match_string(s_arg[i].list[0], option, N_OPTIONS, 0)) {
      case OPT_REDONLY:
        rgb[0] = 1;
        rgb[1] = rgb[2] = 0; /* Enable red channel only. */
        break;
      case OPT_GREENONLY:
        rgb[1] = 1;
        rgb[0] = rgb[2] = 0; /* Enable green channel only. */
        break;
      case OPT_BLUEONLY:
        rgb[2] = 1;
        rgb[0] = rgb[1] = 0; /* Enable blue channel only. */
        break;
      case OPT_SINGLECOLUMNMODE:
        single_column_mode = 1; /* Enable single column mode. */
        break;
      default:
        fprintf(stderr, "invalid option seen\n%s", usage);
        return 1;
      }
    } else {
      if (!input)
        input = s_arg[i].list[0]; /* Assign first non-option argument as input file. */
      else if (!output)
        output = s_arg[i].list[0]; /* Assign second non-option argument as output file. */
      else {
        fprintf(stderr, "too many filenames\n%s", usage);
        return 1;
      }
    }
  }

  tif = TIFFOpen(input, "r"); /* Open TIFF file for reading. */
  if (tif) {
    uint32_t w, h;
    size_t npixels;
    uint32_t *raster;
    double tmp;
    int32_t tmp2;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);                /* Retrieve image width. */
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);               /* Retrieve image height. */
    npixels = w * h;                                          /* Calculate total pixels. */
    raster = (uint32_t *)_TIFFmalloc(npixels * sizeof(uint32_t)); /* Allocate memory for pixel data. */
    if (raster != NULL) {
      if (TIFFReadRGBAImage(tif, w, h, raster, 0)) {

        if (!SDDS_InitializeOutput(&SDDS_dataset, SDDS_BINARY, 1, NULL, NULL, output))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (single_column_mode) {
          /* Define parameters for single column mode. */
          if (SDDS_DefineParameter(&SDDS_dataset, "Variable1Name", NULL, NULL, NULL, NULL, SDDS_STRING, "x") < 0) {
            fprintf(stderr, "Problem defining parameter Variable1Name.\n");
            return 1;
          }
          if (SDDS_DefineParameter(&SDDS_dataset, "Variable2Name", NULL, NULL, NULL, NULL, SDDS_STRING, "y") < 0) {
            fprintf(stderr, "Problem defining parameter Variable2Name.\n");
            return 1;
          }
          tmp = 1;
          if (SDDS_DefineParameter1(&SDDS_dataset, "xInterval", NULL, NULL, NULL, NULL, SDDS_DOUBLE, &tmp) < 0) {
            fprintf(stderr, "Problem defining parameter xInterval.\n");
            return 1;
          }
          if (SDDS_DefineParameter1(&SDDS_dataset, "yInterval", NULL, NULL, NULL, NULL, SDDS_DOUBLE, &tmp) < 0) {
            fprintf(stderr, "Problem defining parameter yInterval.\n");
            return 1;
          }
          tmp2 = w;
          if (SDDS_DefineParameter1(&SDDS_dataset, "xDimension", NULL, NULL, NULL, NULL, SDDS_LONG, &tmp2) < 0) {
            fprintf(stderr, "Problem defining parameter xDimension.\n");
            return 1;
          }
          tmp2 = h;
          if (SDDS_DefineParameter1(&SDDS_dataset, "yDimension", NULL, NULL, NULL, NULL, SDDS_LONG, &tmp2) < 0) {
            fprintf(stderr, "Problem defining parameter yDimension.\n");
            return 1;
          }
          tmp = 0;
          if (SDDS_DefineParameter1(&SDDS_dataset, "xMinimum", NULL, NULL, NULL, NULL, SDDS_DOUBLE, &tmp) < 0) {
            fprintf(stderr, "Problem defining parameter xMinimum.\n");
            return 1;
          }
          if (SDDS_DefineParameter1(&SDDS_dataset, "yMinimum", NULL, NULL, NULL, NULL, SDDS_DOUBLE, &tmp) < 0) {
            fprintf(stderr, "Problem defining parameter yMinimum.\n");
            return 1;
          }
          if (SDDS_DefineSimpleColumn(&SDDS_dataset, "z", NULL, SDDS_LONG) < 0) {
            fprintf(stderr, "Problem defining column z.\n");
            return 1;
          }
          data = malloc(sizeof(*data) * 1);
          data[0] = malloc(sizeof(*(data[0])) * npixels);
        } else {
          /* Define parameters for multi-column mode. */
          if (SDDS_DefineSimpleColumn(&SDDS_dataset, "Index", NULL, SDDS_LONG) < 0) {
            fprintf(stderr, "Problem defining column Index.\n");
            return 1;
          }
          indexes = malloc(sizeof(*indexes) * w);
          column_names = malloc(sizeof(char **) * h);
          data = malloc(sizeof(*data) * h);
          for (i = 0; i < h; i++) {
            column_names[i] = malloc(sizeof(char *) * 15);
            data[i] = malloc(sizeof(*(data[i])) * w);
            sprintf(column_names[i], "Line%05ld", i);
            if (SDDS_DefineSimpleColumn(&SDDS_dataset, column_names[i], NULL, SDDS_LONG) < 0) {
              fprintf(stderr, "Problem defining column %s.\n", column_names[i]);
              return 1;
            }
          }
        }

        if (!SDDS_WriteLayout(&SDDS_dataset))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (single_column_mode) {
          if (!SDDS_StartPage(&SDDS_dataset, npixels))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
              /* Process pixel values for single column mode. */
              data[0][j * h + i] = TIFFGetR(raster[n]) * rgb[0] + TIFFGetG(raster[n]) * rgb[1] + TIFFGetB(raster[n]) * rgb[2];
              n++;
            }
          }

          if (!SDDS_SetColumnFromLongs(&SDDS_dataset, SDDS_SET_BY_NAME, data[0], npixels, "z"))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        } else {
          if (!SDDS_StartPage(&SDDS_dataset, w))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          for (i = 0; i < w; i++)
            indexes[i] = i; /* Initialize index values. */
          for (i = 0; i < h; i++) {
            for (j = 0; j < w; j++) {
              /* Process pixel values for multi-column mode. */
              data[i][j] = TIFFGetR(raster[n]) * rgb[0] + TIFFGetG(raster[n]) * rgb[1] + TIFFGetB(raster[n]) * rgb[2];
              n++;
            }
          }
          if (!SDDS_SetColumnFromLongs(&SDDS_dataset, SDDS_SET_BY_NAME, indexes, w, "Index"))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          for (i = 0; i < h; i++) {
            if (!SDDS_SetColumnFromLongs(&SDDS_dataset, SDDS_SET_BY_NAME, data[i], w, column_names[i]))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }

        if (!SDDS_WritePage(&SDDS_dataset) || !SDDS_Terminate(&SDDS_dataset))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

        if (single_column_mode) {
          free(data[0]);
          free(data);
        } else {
          for (i = 0; i < h; i++) {
            free(column_names[i]);
            free(data[i]);
          }
          free(column_names);
          free(data);
        }
      }
      _TIFFfree(raster); /* Free allocated memory for pixel data. */
    }
    TIFFClose(tif); /* Close TIFF file. */
  }
  if (indexes)
    free(indexes);             /* Free allocated memory for indexes. */
  free_scanargs(&s_arg, argc); /* Free scanned arguments. */
  return 0;
}
