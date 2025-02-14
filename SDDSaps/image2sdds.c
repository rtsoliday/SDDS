/**
 * @file image2sdds.c
 * @brief Converts raw binary image files into SDDS format.
 *
 * @details
 * This program processes raw binary image data and converts it to the SDDS (Self Describing Data Sets) format.
 * It supports both binary and ASCII outputs and includes features for data transposition, 2D array representation,
 * contour header generation, and multi-column mode. The SDDS format facilitates the storage and transfer of
 * scientific data with metadata, making it ideal for analytical and visualization applications.
 *
 * @section Usage
 * ```
 * image2sdds <IMAGE infile> <SDDS outfile>
 *            [-2d]
 *            [-ascii]
 *            [-contour]
 *            [-multicolumnmode]
 *            [-transpose]
 *            [-xdim <value>]
 *            [-ydim <value>]
 *            [-xmin <value>]
 *            [-xmax <value>]
 *            [-ymin <value>]
 *            [-ymax <value>]
 *            [-debug]
 *            [-help]
 * ```
 *
 * @section Options
 * | Optional                                | Description                                               |
 * |-----------------------------------------|-----------------------------------------------------------|
 * | `-2d`                                  | Output the SDDS file as a 2D array.                       |
 * | `-ascii`                               | Write the SDDS file in ASCII format (default is binary).  |
 * | `-contour`                             | Generate SDDS headers for compatibility with sddscontour.|
 * | `-multicolumnmode`                     | Enable multi-column mode for SDDS output.                |
 * | `-transpose`                           | Transpose the image along its diagonal axis.             |
 * | `-xdim`                                | Set the X dimension of the image (default: 482).         |
 * | `-ydim`                                | Set the Y dimension of the image (default: 512).         |
 * | `-xmin`                                | Set the minimum X coordinate value.                      |
 * | `-xmax`                                | Set the maximum X coordinate value.                      |
 * | `-ymin`                                | Set the minimum Y coordinate value.                      |
 * | `-ymax`                                | Set the maximum Y coordinate value.                      |
 * | `-debug`                               | Enable debug mode with the specified level.              |
 * | `-help`                                | Display usage information.                               |
 *
 * @subsection Incompatibilities
 *   - Only one of the following options may be specified:
 *     - `-2d`
 *     - `-multicolumnmode`
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
 * Josh Stein, J. Anderson, R. Soliday
 */

#include <stdio.h>
#include <string.h>
#include <SDDS.h> /* SDDS library header */

#define VERSION "V1.2"
#define XDIMENSION 482 /* Default beam size in X axis */
#define YDIMENSION 512 /* Default beam size in Y axis */

/* Parameter defines */
#define PARM_DEBUG 1
#define PARM_XDIM 2
#define PARM_YDIM 3
#define PARM_TRANSPOSE 4
#define PARM_ASCII 5
#define PARM_HELP 6
#define PARM_QMARK 7
#define PARM_CONTOUR 8
#define PARM_2D 9
#define PARM_XMIN 10
#define PARM_YMIN 11
#define PARM_XMAX 12
#define PARM_YMAX 13
#define PARM_MULTICOLUMNMODE 14

int asciiOutput = 0; /* Set to 1 for ASCII SDDS output, 0 for binary */
int array = 0;       /* Set to 1 for 2-dimensional array output */
int contour = 0;
int xDim = 0;
int yDim = 0;
double xMin = 0;
double yMin = 0;
double xMax = 1;
double yMax = 1;
int useXmax = 0;
int useYmax = 0;
int debug = 0;
int transpose = 0;
int multiColumnMode = 0;

struct parm_info {
  char *parm; /* Parameter string */
  int len;    /* Length of the parameter string */
  int id;     /* Parameter identifier */
  char *desc; /* Description of the parameter */
};

typedef struct parm_info PARM_INFO;

static PARM_INFO ptable[] = {
  {"-2d", 3, PARM_2D, "Output SDDS file as 2D array"},
  {"-debug", 6, PARM_DEBUG, "Enable debug mode"},
  {"-xdim", 5, PARM_XDIM, "Set X dimension of the image"},
  {"-ydim", 5, PARM_YDIM, "Set Y dimension of the image"},
  {"-transpose", 10, PARM_TRANSPOSE, "Transpose the image"},
  {"-ascii", 6, PARM_ASCII, "Output SDDS file as ASCII"},
  {"-contour", 8, PARM_CONTOUR, "Generate SDDS headers for sddscontour tool"},
  {"-help", 5, PARM_HELP, "Display usage information"},
  {"-?", 2, PARM_QMARK, "Display usage information"},
  {"-xmin", 5, PARM_XMIN, "Set minimum X value"},
  {"-ymin", 5, PARM_YMIN, "Set minimum Y value"},
  {"-xmax", 5, PARM_XMAX, "Set maximum X value"},
  {"-ymax", 5, PARM_YMAX, "Set maximum Y value"},
  {"-multicolumnmode", 16, PARM_MULTICOLUMNMODE, "Enable multi-column mode"},
  {NULL, -1, -1, NULL}};

/* Prototypes */
int process_cmdline_args(int argc, char *argv[], char *infile, char *outfile);
void usage(char *name);

int main(int argc, char *argv[]) {
  short *image = NULL;
  short *rotimage = NULL;
  char infile[255];
  char outfile[255];
  char xDimStr[10];
  char yDimStr[10];
  int32_t dim[2];
  FILE *infilefp;
  int x, y, i, n, j;
  SDDS_TABLE table;
  double xInterval = 0.02;
  double yInterval = 0.02;
  double *indexes = NULL;
  char **columnNames = NULL;
  int32_t *data = NULL;

  process_cmdline_args(argc, argv, infile, outfile);

  /* If user didn't specify dimensions, use defaults */
  if (!xDim) {
    xDim = XDIMENSION;
  }
  if (!yDim) {
    yDim = YDIMENSION;
  }

  image = (short *)malloc(sizeof(short) * xDim * yDim);
  if (!image) {
    fprintf(stderr, "%s: Error allocating memory for image.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  rotimage = (short *)malloc(sizeof(short) * xDim * yDim);
  if (!rotimage) {
    fprintf(stderr, "%s: Error allocating memory for rotated image.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Open input file */
  if ((infilefp = fopen(infile, "rb")) == NULL) {
    fprintf(stderr, "%s: Error: unable to open input file: %s\n", argv[0], infile);
    exit(EXIT_FAILURE);
  }

  /* Read image into 1-dimensional array */
  for (x = 0; x < xDim * yDim; x++) {
    int byte = getc(infilefp);
    if (byte == EOF) {
      fprintf(stderr, "%s: Unexpected EOF at index %d\n", argv[0], x);
      exit(EXIT_FAILURE);
    }
    image[x] = (short)byte;
  }
  fclose(infilefp);

  /* Use transpose factor here to adjust image */
  i = 0;
  for (x = 0; x < xDim; x++) {
    for (y = 0; y < yDim; y++) {
      rotimage[y * xDim + x] = image[i];
      i++;
    }
  }

  /* Begin SDDS specifics */

  if (transpose) {                /* Reverse dimensions for transposed image */
    sprintf(yDimStr, "%d", xDim); /* Dimensions of image sent to SDDS */
    sprintf(xDimStr, "%d", yDim);
  } else {
    sprintf(xDimStr, "%d", xDim); /* Dimensions of image sent to SDDS */
    sprintf(yDimStr, "%d", yDim);
  }

  if (asciiOutput) {
    if (!SDDS_InitializeOutput(&table, SDDS_ASCII, 1, "ImageArray", "Converted image data", outfile)) {
      fprintf(stderr, "%s: SDDS error: unable to initialize output file %s\n", argv[0], outfile);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  } else {
    if (!SDDS_InitializeOutput(&table, SDDS_BINARY, 1, "ImageArray", "Converted image data", outfile)) {
      fprintf(stderr, "%s: SDDS error: unable to initialize output file %s\n", argv[0], outfile);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  if (transpose) {
    if (useXmax)
      xInterval = (xMax - xMin) / (yDim - 1);
    if (useYmax)
      yInterval = (yMax - yMin) / (xDim - 1);
  } else {
    if (useXmax)
      xInterval = (xMax - xMin) / (xDim - 1);
    if (useYmax)
      yInterval = (yMax - yMin) / (yDim - 1);
  }

  if (contour) {
    if (!multiColumnMode) {
      /* Begin setting parameters for use in sddscontour tool */
      if (SDDS_DefineParameter(&table, "Variable1Name", NULL, NULL, NULL, NULL, SDDS_STRING, "x") == -1) {
        fprintf(stderr, "%s: SDDS error: unable to define parameter 1: Variable1Name\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      if (SDDS_DefineParameter(&table, "Variable2Name", NULL, NULL, NULL, NULL, SDDS_STRING, "y") == -1) {
        fprintf(stderr, "%s: SDDS error: unable to define parameter 2: Variable2Name\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      if (SDDS_DefineParameter1(&table, "xInterval", NULL, NULL, NULL, NULL, SDDS_DOUBLE, &xInterval) == -1) {
        fprintf(stderr, "%s: SDDS error: unable to define parameter 3: xInterval\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      if (SDDS_DefineParameter1(&table, "xMinimum", NULL, NULL, NULL, NULL, SDDS_DOUBLE, &xMin) == -1) {
        fprintf(stderr, "%s: SDDS error: unable to define parameter 4: xMinimum\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      if (SDDS_DefineParameter(&table, "xDimension", NULL, NULL, NULL, NULL, SDDS_LONG, xDimStr) == -1) {
        fprintf(stderr, "%s: SDDS error: unable to define parameter 5: xDimension\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      if (SDDS_DefineParameter1(&table, "yInterval", NULL, NULL, NULL, NULL, SDDS_DOUBLE, &yInterval) == -1) {
        fprintf(stderr, "%s: SDDS error: unable to define parameter 6: yInterval\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      if (SDDS_DefineParameter1(&table, "yMinimum", NULL, NULL, NULL, NULL, SDDS_DOUBLE, &yMin) == -1) {
        fprintf(stderr, "%s: SDDS error: unable to define parameter 7: yMinimum\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }

      if (SDDS_DefineParameter(&table, "yDimension", NULL, NULL, NULL, NULL, SDDS_LONG, yDimStr) == -1) {
        fprintf(stderr, "%s: SDDS error: unable to define parameter 8: yDimension\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }
  /* Done setting parameters */

  if (array) {
    if (SDDS_DefineArray(&table, "ImageArray", NULL, NULL, "Intensity", NULL, SDDS_CHARACTER, 0, 2, NULL) == -1) {
      fprintf(stderr, "%s: SDDS error: unable to define array %s\n", argv[0], "ImageArray");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  } else {
    if (!multiColumnMode) {
      if (SDDS_DefineColumn(&table, "Image", NULL, NULL, NULL, NULL, SDDS_SHORT, 0) == -1) {
        fprintf(stderr, "%s: SDDS error: unable to define column %s\n", argv[0], "Image");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    } else {
      if (SDDS_DefineSimpleColumn(&table, "Index", NULL, SDDS_DOUBLE) < 0) {
        fprintf(stderr, "%s: SDDS error: problem defining column Index.\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      if (transpose) {
        dim[0] = xDim;
        dim[1] = yDim;
      } else {
        dim[0] = yDim;
        dim[1] = xDim;
      }
      indexes = malloc(sizeof(double) * dim[1]);
      if (!indexes) {
        fprintf(stderr, "%s: Error allocating memory for indexes.\n", argv[0]);
        exit(EXIT_FAILURE);
      }
      columnNames = malloc(sizeof(char *) * dim[0]);
      if (!columnNames) {
        fprintf(stderr, "%s: Error allocating memory for column names.\n", argv[0]);
        exit(EXIT_FAILURE);
      }
      data = malloc(sizeof(int32_t) * dim[1]);
      if (!data) {
        fprintf(stderr, "%s: Error allocating memory for data.\n", argv[0]);
        exit(EXIT_FAILURE);
      }
      for (i = 0; i < dim[0]; i++) {
        columnNames[i] = malloc(sizeof(char) * 20);
        if (!columnNames[i]) {
          fprintf(stderr, "%s: Error allocating memory for column name %d.\n", argv[0], i);
          exit(EXIT_FAILURE);
        }
        sprintf(columnNames[i], "Line%" PRId32, i);
        if (SDDS_DefineSimpleColumn(&table, columnNames[i], NULL, SDDS_SHORT) < 0) {
          fprintf(stderr, "%s: SDDS error: problem defining column %s.\n", argv[0], columnNames[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
    }
  }

  if (!SDDS_WriteLayout(&table)) {
    fprintf(stderr, "%s: SDDS error: unable to write layout\n", argv[0]);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (!multiColumnMode) {
    if (!SDDS_StartTable(&table, xDim * yDim)) {
      fprintf(stderr, "%s: SDDS error: unable to start table\n", argv[0]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  } else {
    if (!SDDS_StartTable(&table, dim[1])) {
      fprintf(stderr, "%s: SDDS error: unable to start table\n", argv[0]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  if (!array) {
    if (!multiColumnMode) {
      if (transpose) {
        for (x = 0; x < xDim * yDim; x++) {
          if (!SDDS_SetRowValues(&table, (SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE), x, "Image", rotimage[x], NULL)) {
            fprintf(stderr, "%s: SDDS error: unable to write row %d\n", argv[0], x);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        }
      } else {
        for (x = 0; x < xDim * yDim; x++) {
          if (!SDDS_SetRowValues(&table, (SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE), x, "Image", image[x], NULL)) {
            fprintf(stderr, "%s: SDDS error: unable to write row %d\n", argv[0], x);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        }
      }
    } else {
      for (i = 0; i < dim[1]; i++)
        indexes[i] = xMin + xInterval * i;
      if (!SDDS_SetColumnFromDoubles(&table, SDDS_SET_BY_NAME, indexes, dim[1], "Index")) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      n = 0;

      for (i = 0; i < dim[0]; i++) {
        for (j = 0; j < dim[1]; j++) {
          if (transpose) {
            data[j] = image[n];
          } else {
            data[j] = rotimage[n];
          }
          n++;
        }

        if (!SDDS_SetColumnFromLongs(&table, SDDS_SET_BY_NAME, data, dim[1], columnNames[i])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
    }
  } else { /* Requesting writing of 2D array */
    if (transpose) {
      dim[0] = xDim;
      dim[1] = yDim;
      if (SDDS_SetArray(&table, "ImageArray", SDDS_CONTIGUOUS_DATA, rotimage, dim) == 0) {
        fprintf(stderr, "%s: SDDS Error - unable to set array\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    } else {
      dim[0] = yDim;
      dim[1] = xDim;
      if (SDDS_SetArray(&table, "ImageArray", SDDS_CONTIGUOUS_DATA, image, dim) == 0) {
        fprintf(stderr, "%s: SDDS Error - unable to set array\n", argv[0]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }

  if (!SDDS_WriteTable(&table)) {
    fprintf(stderr, "%s: SDDS error: unable to write table\n", argv[0]);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (!SDDS_Terminate(&table)) {
    fprintf(stderr, "%s: SDDS error: unable to terminate SDDS\n", argv[0]);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  /* Free allocated memory */
  if (image)
    free(image);
  if (rotimage)
    free(rotimage);
  if (indexes)
    free(indexes);
  if (data)
    free(data);
  if (columnNames) {
    free(columnNames);
  }

  return EXIT_SUCCESS;
}

/*************************************************************************
 * FUNCTION : process_cmdline_args()
 * PURPOSE  : Get command line arguments and set flags appropriately.
 * ARGS in  : argc, argv - standard unix
 * ARGS out : infile, outfile - names of input and output files
 * GLOBAL   : nothing
 * RETURNS  : 0 on success, exits on error
 ************************************************************************/
int process_cmdline_args(int argc, char *argv[], char *infile, char *outfile) {
  int i, j;
  int cmderror = 0;
  int scan_done = 0;

  *infile = '\0';
  *outfile = '\0';

  /* Process command line options */
  if (argc < 3)
    cmderror++;
  else {
    strcpy(infile, argv[1]);
    strcpy(outfile, argv[2]);

    /* Begin parsing any optional args */
    for (i = 3; i < argc && !cmderror; i++) {
      for (j = 0; !scan_done && !cmderror && ptable[j].parm; j++) {
        if (strncmp(ptable[j].parm, argv[i], ptable[j].len) == 0) {
          switch (ptable[j].id) {

          case PARM_DEBUG:
            if (++i >= argc)
              cmderror++;
            else {
              if (argv[i][0] == '-')
                cmderror++;
              else {
                if (sscanf(argv[i], "%d", &debug) < 1)
                  cmderror++;
                else
                  scan_done = 1;
              }
            }
            break;

          case PARM_XDIM:
            if (++i >= argc)
              cmderror++;
            else {
              if (argv[i][0] == '-')
                cmderror++;
              else {
                if (sscanf(argv[i], "%d", &xDim) < 1)
                  cmderror++;
                else
                  scan_done = 1;
              }
            }
            break;

          case PARM_YDIM:
            if (++i >= argc)
              cmderror++;
            else {
              if (argv[i][0] == '-')
                cmderror++;
              else {
                if (sscanf(argv[i], "%d", &yDim) < 1)
                  cmderror++;
                else
                  scan_done = 1;
              }
            }
            break;

          case PARM_XMIN:
            if (++i >= argc)
              cmderror++;
            else {
              if (argv[i][0] == '-')
                cmderror++;
              else {
                if (sscanf(argv[i], "%lf", &xMin) < 1)
                  cmderror++;
                else
                  scan_done = 1;
              }
            }
            break;
          case PARM_XMAX:
            if (++i >= argc)
              cmderror++;
            else {
              if (argv[i][0] == '-')
                cmderror++;
              else {
                if (sscanf(argv[i], "%lf", &xMax) < 1)
                  cmderror++;
                else {
                  scan_done = 1;
                  useXmax = 1;
                }
              }
            }
            break;
          case PARM_YMIN:
            if (++i >= argc)
              cmderror++;
            else {
              if (argv[i][0] == '-')
                cmderror++;
              else {
                if (sscanf(argv[i], "%lf", &yMin) < 1)
                  cmderror++;
                else
                  scan_done = 1;
              }
            }
            break;
          case PARM_YMAX:
            if (++i >= argc)
              cmderror++;
            else {
              if (argv[i][0] == '-')
                cmderror++;
              else {
                if (sscanf(argv[i], "%lf", &yMax) < 1)
                  cmderror++;
                else {
                  scan_done = 1;
                  useYmax = 1;
                }
              }
            }
            break;

          case PARM_TRANSPOSE:
            transpose++;
            scan_done = 1;
            break;
          case PARM_MULTICOLUMNMODE:
            multiColumnMode = 1;
            scan_done = 1;
            break;

          case PARM_ASCII:
            asciiOutput++;
            scan_done = 1;
            break;

          case PARM_HELP:
          case PARM_QMARK:
            usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;

          case PARM_CONTOUR:
            contour++;
            scan_done = 1;
            break;

          case PARM_2D:
            array++;
            scan_done = 1;
            break;

          default:
            cmderror++;
            break;
          }
        }
      }
      scan_done = 0;
    }
  }
  if (cmderror) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }
  return 0;
}

/*************************************************************************
 * FUNCTION : usage()
 * PURPOSE  : Describes command line options available to client.
 * ARGS in  : char *name - name of executable program
 * ARGS out : none
 * GLOBAL   : nothing
 * RETURNS  : none
 ************************************************************************/

#define pr fprintf

void usage(char *name) {
  pr(stderr, "Image2SDDS Utility %s\n", VERSION);
  pr(stderr, "=============================\n");
  pr(stderr, "Usage:\n");
  pr(stderr, "  image2sdds <IMAGE infile> <SDDS outfile>\n");
  pr(stderr, "             [-2d]\n");
  pr(stderr, "             [-ascii]\n");
  pr(stderr, "             [-contour]\n");
  pr(stderr, "             [-multicolumnmode]\n");
  pr(stderr, "             [-transpose]\n");
  pr(stderr, "             [-xdim <value>]\n");
  pr(stderr, "             [-ydim <value>]\n");
  pr(stderr, "             [-xmin <value>]\n");
  pr(stderr, "             [-xmax <value>]\n");
  pr(stderr, "             [-ymin <value>]\n");
  pr(stderr, "             [-ymax <value>]\n");
  pr(stderr, "             [-debug]\n");
  pr(stderr, "             [-help]\n");
  pr(stderr, "Options:\n");
  pr(stderr, "  -2d                  Output SDDS file as a 2D array.\n");
  pr(stderr, "  -ascii               Write SDDS file as ASCII (default is binary).\n");
  pr(stderr, "  -contour             Generate SDDS headers for the sddscontour tool.\n");
  pr(stderr, "  -multicolumnmode     Enable multi-column mode.\n");
  pr(stderr, "  -transpose           Transpose the image about the diagonal.\n");
  pr(stderr, "  -xdim <value>        Set X dimension of the image (default = %d).\n", XDIMENSION);
  pr(stderr, "  -ydim <value>        Set Y dimension of the image (default = %d).\n", YDIMENSION);
  pr(stderr, "  -xmin <value>        Set minimum X value.\n");
  pr(stderr, "  -xmax <value>        Set maximum X value.\n");
  pr(stderr, "  -ymin <value>        Set minimum Y value.\n");
  pr(stderr, "  -ymax <value>        Set maximum Y value.\n");
  pr(stderr, "  -debug <level>       Enable debug mode with the specified level.\n");
  pr(stderr, "  -? or -help          Display this usage message.\n\n");
  pr(stderr, "Purpose:\n");
  pr(stderr, "  Reads image data from <infile> and writes SDDS data to <outfile>.\n");
  pr(stderr, "  Supports various output formats and options for data manipulation.\n");
}
