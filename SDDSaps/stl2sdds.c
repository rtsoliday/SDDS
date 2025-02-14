/**
 * @file stl2sdds.c
 * @brief Converts STL (STereo-Lithography) files to SDDS format.
 *
 * @details
 * This program reads an STL (STereo-Lithography) file and converts it
 * into the Self Describing Data Sets (SDDS) format. It supports both ASCII and binary
 * STL files and can output in ASCII or binary SDDS formats. Additionally, it
 * supports piping options for input and output.
 *
 * @section Usage
 * ```
 * stl2sdds [<inputFile>] [<outputFile>] 
 *          [-pipe[=in][,out]]
 *          [-ascii | -binary]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                              | Enables piping for input and/or output using the SDDS toolkit pipe option.           |
 * | `-ascii`                             | Requests SDDS ASCII output. Default is binary.                                       |
 * | `-binary`                            | Requests SDDS binary output.                                                         |
 *
 * @subsection Incompatibilities
 *   - Only one of the following may be specified:
 *     - `-ascii`
 *     - `-binary`
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

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#define LINE_MAX_LEN 256
#define FALSE 0
#define TRUE 1

float float_reverse_bytes(float x);

int leqi(char *string1, char *string2);

/* Enumeration for option types */
enum option_type {
  SET_ASCII,
  SET_BINARY,
  SET_PIPE,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "ascii", "binary", "pipe"};

/* Improved and formatted usage message */
char *USAGE =
  "Usage: stl2sdds [<inputFile>] [<outputFile>] [-pipe[=in][,out]]\n"
  "                [-ascii | -binary]\n\n"
  "Options:\n"
  "  -pipe[=in][,out]   Enable piping for input and/or output using SDDS toolkit.\n"
  "  -ascii             Output SDDS in ASCII format. Default is binary.\n"
  "  -binary            Output SDDS in binary format.\n\n"
  "Converts STL files to SDDS format.\n"
  "Author: Robert Soliday.\n"
  "Compiled on " __DATE__ " at " __TIME__ ", SVN revision: " SVN_VERSION "\n";

int main(int argc, char **argv) {
  char *inputFile = NULL, *outputFile = NULL;
  SDDS_DATASET SDDSout;
  SCANNED_ARG *scanned;
  long iArg;
  long ascii = 0;
  unsigned long pipeFlags = 0;

  int i, n = 0, solid = 0;
  char init[7];
  short attribute = 0;
  int32_t face_num = 0, iface;
  int32_t bigEndianMachine = 0;
  int64_t bytes_num = 0, text_num = 0;
  float *normalVector[3];
  float *vertex1[3];
  float *vertex2[3];
  float *vertex3[3];
  FILE *fd;
  char input[LINE_MAX_LEN];
  char *next;
  char token[LINE_MAX_LEN];
  int width;

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
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "Error: Invalid -pipe syntax.\n");
          fprintf(stderr, "%s", USAGE);
          return EXIT_FAILURE;
        }
        break;
      default:
        fprintf(stderr, "Error: Invalid option detected.\n%s", USAGE);
        return EXIT_FAILURE;
      }
    } else {
      if (!inputFile)
        SDDS_CopyString(&inputFile, scanned[iArg].list[0]);
      else if (!outputFile)
        SDDS_CopyString(&outputFile, scanned[iArg].list[0]);
      else {
        fprintf(stderr, "Error: Too many filenames provided.\n%s", USAGE);
        return EXIT_FAILURE;
      }
    }
  }

  processFilenames("stl2sdds", &inputFile, &outputFile, pipeFlags, 0, NULL);

  if (inputFile) {
    if (!fexists(inputFile)) {
      fprintf(stderr, "Error: Input file '%s' not found.\n", inputFile);
      return EXIT_FAILURE;
    }
    if (!(fd = fopen(inputFile, "rb"))) {
      fprintf(stderr, "Error: Unable to open input file '%s'.\n", inputFile);
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

  bigEndianMachine = SDDS_IsBigEndianMachine();

  /* Check for ASCII or BINARY STL type */
  bytes_num += fread(&init, 1, 6, fd);
  init[6] = '\0';
  int stlascii = (strcasecmp("solid ", init) == 0) ? TRUE : FALSE;

  if (!stlascii) {
    /* 80 byte Header. (74 bytes remaining) */
    for (i = 0; i < 74; i++) {
      if (fgetc(fd) == EOF)
        break;
      bytes_num++;
    }

    /* Number of faces */
    bytes_num += fread(&face_num, 1, sizeof(int32_t), fd);
    if (bigEndianMachine) {
      SDDS_SwapLong((int32_t *)&face_num);
    }

    for (i = 0; i < 3; i++) {
      normalVector[i] = malloc(sizeof(*(normalVector[i])) * face_num);
      vertex1[i] = malloc(sizeof(*(vertex1[i])) * face_num);
      vertex2[i] = malloc(sizeof(*(vertex2[i])) * face_num);
      vertex3[i] = malloc(sizeof(*(vertex3[i])) * face_num);
      if (!normalVector[i] || !vertex1[i] || !vertex2[i] || !vertex3[i]) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return EXIT_FAILURE;
      }
    }

    /* Read each face's data */
    for (iface = 0; iface < face_num; iface++) {
      for (i = 0; i < 3; i++) {
        if (fread(&(normalVector[i][iface]), 1, sizeof(float), fd) != sizeof(float)) {
          fprintf(stderr, "Error: Unexpected end of file while reading normals.\n");
          return EXIT_FAILURE;
        }
        bytes_num += sizeof(float);
        if (bigEndianMachine) {
          normalVector[i][iface] = float_reverse_bytes(normalVector[i][iface]);
        }
      }
      for (i = 0; i < 3; i++) {
        if (fread(&(vertex1[i][iface]), 1, sizeof(float), fd) != sizeof(float)) {
          fprintf(stderr, "Error: Unexpected end of file while reading vertex1.\n");
          return EXIT_FAILURE;
        }
        bytes_num += sizeof(float);
        if (bigEndianMachine) {
          vertex1[i][iface] = float_reverse_bytes(vertex1[i][iface]);
        }
      }
      for (i = 0; i < 3; i++) {
        if (fread(&(vertex2[i][iface]), 1, sizeof(float), fd) != sizeof(float)) {
          fprintf(stderr, "Error: Unexpected end of file while reading vertex2.\n");
          return EXIT_FAILURE;
        }
        bytes_num += sizeof(float);
        if (bigEndianMachine) {
          vertex2[i][iface] = float_reverse_bytes(vertex2[i][iface]);
        }
      }
      for (i = 0; i < 3; i++) {
        if (fread(&(vertex3[i][iface]), 1, sizeof(float), fd) != sizeof(float)) {
          fprintf(stderr, "Error: Unexpected end of file while reading vertex3.\n");
          return EXIT_FAILURE;
        }
        bytes_num += sizeof(float);
        if (bigEndianMachine) {
          vertex3[i][iface] = float_reverse_bytes(vertex3[i][iface]);
        }
      }
      if (fread(&(attribute), 1, sizeof(short int), fd) != sizeof(short int)) {
        fprintf(stderr, "Error: Unexpected end of file while reading attribute.\n");
        return EXIT_FAILURE;
      }
      bytes_num += sizeof(short int);
    }
  } else {
    for (i = 0; i < 3; i++) {
      normalVector[i] = NULL;
      vertex1[i] = NULL;
      vertex2[i] = NULL;
      vertex3[i] = NULL;
    }
    fseek(fd, 0, SEEK_SET);
    while (fgets(input, LINE_MAX_LEN, fd) != NULL) {
      text_num++;
      for (next = input; *next != '\0' && isspace((unsigned char)*next); next++) {
      }
      if (*next == '\0' || *next == '#' || *next == '!' || *next == '$') {
        continue;
      }
      sscanf(next, "%s%n", token, &width);
      next += width;

      if (leqi(token, "facet")) {
        if (n == 0) {
          n++;
          for (i = 0; i < 3; i++) {
            normalVector[i] = malloc(sizeof(*(normalVector[i])) * n);
            vertex1[i] = malloc(sizeof(*(vertex1[i])) * n);
            vertex2[i] = malloc(sizeof(*(vertex2[i])) * n);
            vertex3[i] = malloc(sizeof(*(vertex3[i])) * n);
            if (!normalVector[i] || !vertex1[i] || !vertex2[i] || !vertex3[i]) {
              fprintf(stderr, "Error: Memory allocation failed.\n");
              return EXIT_FAILURE;
            }
          }
        } else {
          n++;
          for (i = 0; i < 3; i++) {
            normalVector[i] = realloc(normalVector[i], sizeof(*(normalVector[i])) * n);
            vertex1[i] = realloc(vertex1[i], sizeof(*(vertex1[i])) * n);
            vertex2[i] = realloc(vertex2[i], sizeof(*(vertex2[i])) * n);
            vertex3[i] = realloc(vertex3[i], sizeof(*(vertex3[i])) * n);
            if (!normalVector[i] || !vertex1[i] || !vertex2[i] || !vertex3[i]) {
              fprintf(stderr, "Error: Memory allocation failed.\n");
              return EXIT_FAILURE;
            }
          }
        }

        /* Read normal vector */
        sscanf(next, "%*s %e %e %e",
               &(normalVector[0][n - 1]),
               &(normalVector[1][n - 1]),
               &(normalVector[2][n - 1]));

        /* Skip "outer loop" */
        if (!fgets(input, LINE_MAX_LEN, fd) || !fgets(input, LINE_MAX_LEN, fd)) {
          fprintf(stderr, "Error: Unexpected end of file while reading outer loop.\n");
          return EXIT_FAILURE;
        }
        text_num += 2;

        /* Read vertex1 */
        sscanf(next, "%*s %e %e %e",
               &(vertex1[0][n - 1]),
               &(vertex1[1][n - 1]),
               &(vertex1[2][n - 1]));

        /* Skip "vertex" */
        if (!fgets(input, LINE_MAX_LEN, fd)) {
          fprintf(stderr, "Error: Unexpected end of file while reading vertex2.\n");
          return EXIT_FAILURE;
        }
        text_num++;

        /* Read vertex2 */
        sscanf(next, "%*s %e %e %e",
               &(vertex2[0][n - 1]),
               &(vertex2[1][n - 1]),
               &(vertex2[2][n - 1]));

        /* Skip "vertex" */
        if (!fgets(input, LINE_MAX_LEN, fd)) {
          fprintf(stderr, "Error: Unexpected end of file while reading vertex3.\n");
          return EXIT_FAILURE;
        }
        text_num++;

        /* Read vertex3 */
        sscanf(next, "%*s %e %e %e",
               &(vertex3[0][n - 1]),
               &(vertex3[1][n - 1]),
               &(vertex3[2][n - 1]));

        /* Skip "endloop" and "endfacet" */
        if (!fgets(input, LINE_MAX_LEN, fd) || !fgets(input, LINE_MAX_LEN, fd)) {
          fprintf(stderr, "Error: Unexpected end of file while reading endloop/endfacet.\n");
          return EXIT_FAILURE;
        }
        text_num += 2;
      } else if (leqi(token, "color")) {
        fprintf(stderr, "Warning: Color field seen in STL file ignored.\n");
      } else if (leqi(token, "solid")) {
        solid++;
        if (solid > 1) {
          fprintf(stderr, "Error: More than one solid field seen in STL file.\n");
          return EXIT_FAILURE;
        }
      } else if (leqi(token, "endsolid")) {
        /* Do nothing */
      } else {
        fprintf(stderr, "Error: Unrecognized keyword '%s' in STL file.\n", token);
        return EXIT_FAILURE;
      }
    }
    face_num = n;
  }

  /* Initialize SDDS output */
  if (!SDDS_InitializeOutput(&SDDSout, ascii ? SDDS_ASCII : SDDS_BINARY, 1, NULL, NULL, outputFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!ascii) {
    SDDSout.layout.data_mode.column_major = 1;
  }

  /* Define SDDS columns */
  const char *columns[] = {
    "NormalVectorX", "NormalVectorY", "NormalVectorZ",
    "Vertex1X", "Vertex1Y", "Vertex1Z",
    "Vertex2X", "Vertex2Y", "Vertex2Z",
    "Vertex3X", "Vertex3Y", "Vertex3Z"};
  for (i = 0; i < 12; i++) {
    if (!SDDS_DefineSimpleColumn(&SDDSout, columns[i], NULL, SDDS_FLOAT)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
  }

  /* Write SDDS layout */
  if (!SDDS_WriteLayout(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  /* Start SDDS table */
  if (!SDDS_StartTable(&SDDSout, face_num)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  /* Set SDDS columns from data */
  const char *column_names[] = {
    "NormalVectorX", "NormalVectorY", "NormalVectorZ",
    "Vertex1X", "Vertex1Y", "Vertex1Z",
    "Vertex2X", "Vertex2Y", "Vertex2Z",
    "Vertex3X", "Vertex3Y", "Vertex3Z"};
  float *data_columns[12] = {
    normalVector[0], normalVector[1], normalVector[2],
    vertex1[0], vertex1[1], vertex1[2],
    vertex2[0], vertex2[1], vertex2[2],
    vertex3[0], vertex3[1], vertex3[2]};

  for (i = 0; i < 12; i++) {
    if (!SDDS_SetColumnFromFloats(&SDDSout, SDDS_SET_BY_NAME, data_columns[i], face_num, column_names[i])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
  }

  /* Write SDDS table */
  if (!SDDS_WriteTable(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    free_scanargs(&scanned, argc);
    return EXIT_FAILURE;
  }

  /* Terminate SDDS output */
  if (!SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  /* Clean up */
  free_scanargs(&scanned, argc);
  for (i = 0; i < 3; i++) {
    free(normalVector[i]);
    free(vertex1[i]);
    free(vertex2[i]);
    free(vertex3[i]);
  }

  return EXIT_SUCCESS;
}

float float_reverse_bytes(float x) {
  char c;
  union {
    float yfloat;
    char ychar[4];
  } y;

  y.yfloat = x;

  /* Swap bytes 0 and 3 */
  c = y.ychar[0];
  y.ychar[0] = y.ychar[3];
  y.ychar[3] = c;

  /* Swap bytes 1 and 2 */
  c = y.ychar[1];
  y.ychar[1] = y.ychar[2];
  y.ychar[2] = c;

  return y.yfloat;
}

int leqi(char *string1, char *string2) {
  int i;
  int nchar;
  int nchar1 = strlen(string1);
  int nchar2 = strlen(string2);

  nchar = (nchar1 < nchar2) ? nchar1 : nchar2;

  /* Compare characters case-insensitively */
  for (i = 0; i < nchar; i++) {
    if (toupper((unsigned char)string1[i]) != toupper((unsigned char)string2[i])) {
      return FALSE;
    }
  }

  /* Check for trailing spaces in the longer string */
  if (nchar1 > nchar) {
    for (i = nchar; i < nchar1; i++) {
      if (string1[i] != ' ') {
        return FALSE;
      }
    }
  } else if (nchar2 > nchar) {
    for (i = nchar; i < nchar2; i++) {
      if (string2[i] != ' ') {
        return FALSE;
      }
    }
  }

  return TRUE;
}
