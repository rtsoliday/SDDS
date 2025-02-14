/**
 * @file sddsarray2column.c
 * @brief Converts SDDS arrays to SDDS columns.
 *
 * @details
 * This program reads an SDDS file, converts specified arrays into columns,
 * and writes the result to a target SDDS file. The number of elements in the
 * converted arrays must match the number of rows in existing columns or the
 * number of elements in other converted arrays.
 *
 * @section Usage
 * ```
 * sddsarray2column [<source-file>] [<target-file>]
 *                  [-pipe=[input][,output]]
 *                  [-nowarnings]
 *                  [-convert=<array-name>[,<column-name>][,d<dimension>=<indexValue>]...]
 * ```
 *
 * @section Options
 * | Required  | Description                                                                 |
 * |-----------|-----------------------------------------------------------------------------|
 * | `-convert`| Convert the specified SDDS array to a column.                               |
 *
 * | Optional  | Description                                                                 |
 * |-----------|-----------------------------------------------------------------------------|
 * | `-pipe`   | Use standard input and/or output for file operations.                       |
 * | `-nowarnings` | Suppress warning messages.                                               |
 *
 * @subsection Incompatibilities
 * - `-convert`:
 *   - Converted arrays cannot have the same `column-name` as existing columns.
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
 * R. Soliday, M. Borland
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_NOWARNINGS,
  SET_CONVERT,
  SET_PIPE,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "nowarnings",
  "convert",
  "pipe",
};

#define DIM_0 0
#define DIM_1 1
#define DIM_2 2
#define DIM_OPTIONS 3

static char *dim_option[DIM_OPTIONS] = {
  "d0",
  "d1",
  "d2",
};

static char *USAGE =
  "sddsarray2column [<source-file>] [<target-file>]\n"
  "                 [-pipe=[input][,output]]\n"
  "                 [-nowarnings]\n"
  "                  -convert=<array-name>[,<column-name>][,d<dimension>=<indexValue>]... \n\n"
  "sddsarray2column converts SDDS arrays to SDDS columns.\n"
  "The number of elements in the converted arrays must equal\n"
  "the number of rows if there are columns in the file and\n"
  "the number of elements in other converted arrays.\n\n"
  "Examples:\n"
  "  sddsarray2column in out -convert=A,A_out\n"
  "  sddsarray2column in out -convert=A,A_out,d0=0\n"
  "  sddsarray2column in out \"-convert=A,A_out,d2=(1,3)\"\n\n"
  "Program by Robert Soliday. (Compiled on " __DATE__ " at " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

typedef struct {
  char *name;
  char *new_name;
  char *d[3];
  long *dim[3];
  long dims[3];
  void *data;
  long type;
} CONVERTED_ARRAY;

int main(int argc, char **argv) {
  CONVERTED_ARRAY *ca = NULL;
  SDDS_DATASET SDDS_dataset, SDDS_orig;
  ARRAY_DEFINITION *ardef = NULL;
  long i, i_arg, j, found, k, m, n;
  SCANNED_ARG *s_arg;
  char *input = NULL, *output = NULL, *ptr = NULL, *buffer = NULL;
  char *description_text = NULL, *description_contents = NULL;
  unsigned long pipeFlags = 0;
  long noWarnings = 0, tmpfile_used = 1;
  long virtual_rows, max_size, pageNumber, vrows;
  int64_t rows;

  char **orig_column_name = NULL, **orig_parameter_name = NULL, **orig_array_name = NULL;
  int32_t orig_column_names = 0, orig_parameter_names = 0, orig_array_names = 0;
  char **new_array_name = NULL;
  long new_array_names = 0;

  long converted_array_names = 0;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    bomb(NULL, USAGE);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_CONVERT:
        if (s_arg[i_arg].n_items < 2) {
          SDDS_Bomb("Invalid -convert syntax");
        }
        ca = trealloc(ca, sizeof(*ca) * (converted_array_names + 1));

        ca[converted_array_names].name = s_arg[i_arg].list[1];

        if ((s_arg[i_arg].n_items > 2) && (strchr(s_arg[i_arg].list[2], '=') == NULL)) {
          ca[converted_array_names].new_name = s_arg[i_arg].list[2];
          i = 3;
        } else {
          ca[converted_array_names].new_name = s_arg[i_arg].list[1];
          i = 2;
        }

        ca[converted_array_names].d[0] = NULL;
        ca[converted_array_names].d[1] = NULL;
        ca[converted_array_names].d[2] = NULL;
        while (i < s_arg[i_arg].n_items) {
          if (!(ptr = strchr(s_arg[i_arg].list[i], '='))) {
            SDDS_Bomb("Invalid -convert syntax");
          }
          *ptr++ = 0;
          switch (match_string(s_arg[i_arg].list[i], dim_option, DIM_OPTIONS, 0)) {
          case DIM_0:
            ca[converted_array_names].d[0] = ptr;
            break;
          case DIM_1:
            ca[converted_array_names].d[1] = ptr;
            break;
          case DIM_2:
            ca[converted_array_names].d[2] = ptr;
            break;
          default:
            SDDS_Bomb("Invalid -convert syntax");
          }
          i++;
        }
        converted_array_names++;
        break;

      case SET_NOWARNINGS:
        if (s_arg[i_arg].n_items != 1) {
          SDDS_Bomb("Invalid -nowarnings syntax");
        }
        noWarnings = 1;
        break;

      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags)) {
          SDDS_Bomb("Invalid -pipe syntax");
        }
        break;
      }
    } else {
      if (input == NULL) {
        input = s_arg[i_arg].list[0];
      } else if (output == NULL) {
        output = s_arg[i_arg].list[0];
      } else {
        SDDS_Bomb("Too many filenames");
      }
    }
  }

  processFilenames("sddsarray2column", &input, &output, pipeFlags, noWarnings, &tmpfile_used);

  if (!SDDS_InitializeInput(&SDDS_orig, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (!description_text) {
    SDDS_GetDescription(&SDDS_orig, &description_text, &description_contents);
  }

  if (!SDDS_InitializeOutput(&SDDS_dataset, SDDS_orig.layout.data_mode.mode, 1, description_text, description_contents, output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);

  /* Read in names of parameters, columns, and arrays */
  orig_parameter_name = SDDS_GetParameterNames(&SDDS_orig, &orig_parameter_names);
  if (!orig_parameter_name) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  orig_column_name = SDDS_GetColumnNames(&SDDS_orig, &orig_column_names);
  if (!orig_column_name) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  orig_array_name = SDDS_GetArrayNames(&SDDS_orig, &orig_array_names);
  if (!orig_array_name) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  /* Check for problems with the names of the arrays that are to be converted */
  for (j = 0; j < converted_array_names; j++) {
    for (i = 0; i < orig_column_names; i++) {
      if (strcmp(orig_column_name[i], ca[j].new_name) == 0) {
        fprintf(stderr, "Error: Column '%s' already exists.\n", orig_column_name[i]);
        exit(EXIT_FAILURE);
      }
    }
    for (i = 0; i < converted_array_names; i++) {
      if ((i != j) && (strcmp(ca[i].new_name, ca[j].new_name) == 0)) {
        fprintf(stderr, "Error: Cannot convert two arrays to the same column name '%s'.\n", ca[j].new_name);
        exit(EXIT_FAILURE);
      }
    }
    found = 0;
    for (i = 0; i < orig_array_names; i++) {
      if (strcmp(orig_array_name[i], ca[j].name) == 0) {
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "Error: Array '%s' does not exist.\n", ca[j].name);
      exit(EXIT_FAILURE);
    }
  }

  /* Find array names that we will not be converting */
  for (i = 0; i < orig_array_names; i++) {
    found = 0;
    for (j = 0; j < converted_array_names; j++) {
      if (strcmp(orig_array_name[i], ca[j].name) == 0) {
        found = 1;
        break;
      }
    }
    if (!found) {
      new_array_name = trealloc(new_array_name, sizeof(*new_array_name) * (new_array_names + 1));
      new_array_name[new_array_names] = orig_array_name[i];
      new_array_names++;
    }
  }

  /* Write the header of the SDDS file */
  for (i = 0; i < orig_parameter_names; i++) {
    if (!SDDS_TransferParameterDefinition(&SDDS_dataset, &SDDS_orig, orig_parameter_name[i], orig_parameter_name[i])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  for (i = 0; i < orig_column_names; i++) {
    if (!SDDS_TransferColumnDefinition(&SDDS_dataset, &SDDS_orig, orig_column_name[i], orig_column_name[i])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  for (i = 0; i < new_array_names; i++) {
    if (!SDDS_TransferArrayDefinition(&SDDS_dataset, &SDDS_orig, new_array_name[i], new_array_name[i])) {
      fprintf(stderr, "Unable to transfer array '%s' to '%s'.\n", new_array_name[i], new_array_name[i]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  for (i = 0; i < converted_array_names; i++) {
    ardef = SDDS_GetArrayDefinition(&SDDS_orig, ca[i].name);
    if (!ardef) {
      fprintf(stderr, "Error: Unknown array named '%s'.\n", ca[i].name);
      exit(EXIT_FAILURE);
    }
    ca[i].type = ardef->type;
    if (SDDS_DefineColumn(&SDDS_dataset, ca[i].new_name, ardef->symbol, ardef->units, ardef->description, ardef->format_string, ardef->type, ardef->field_length) < 0) {
      SDDS_FreeArrayDefinition(ardef);
      fprintf(stderr, "Error: Unable to define new column '%s'.\n", ca[i].new_name);
      exit(EXIT_FAILURE);
    }
    SDDS_FreeArrayDefinition(ardef);
  }

  if (!SDDS_WriteLayout(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /* Read the data and write the new file */
  max_size = 0;
  for (i = 0; i < SDDS_NUM_TYPES; i++) {
    if (max_size < SDDS_type_size[i]) {
      max_size = SDDS_type_size[i];
    }
  }
  buffer = tmalloc(max_size * sizeof(char));

  while ((pageNumber = SDDS_ReadPage(&SDDS_orig)) >= 0) {
    if (pageNumber == 0) {
      fprintf(stderr, "Error: SDDS data garbled.\n");
      fprintf(stderr, "Warning: One or more data pages may be missing.\n");
      break;
    }
    rows = SDDS_RowCount(&SDDS_orig);
    if (rows < 0) {
      fprintf(stderr, "Error: Problem counting rows in input page.\n");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_StartPage(&SDDS_dataset, rows)) {
      fprintf(stderr, "Error: Problem starting output page.\n");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    for (i = 0; i < orig_parameter_names; i++) {
      if (!SDDS_GetParameter(&SDDS_orig, orig_parameter_name[i], buffer)) {
        fprintf(stderr, "Error: Problem getting parameter '%s'.\n", orig_parameter_name[i]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!SDDS_SetParameters(&SDDS_dataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, orig_parameter_name[i], buffer, NULL)) {
        fprintf(stderr, "Error: Problem setting parameter '%s'.\n", orig_parameter_name[i]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    if (rows) {
      for (i = 0; i < orig_column_names; i++) {
        ptr = SDDS_GetInternalColumn(&SDDS_orig, orig_column_name[i]);
        if (!ptr) {
          fprintf(stderr, "Error: Problem getting column '%s'.\n", orig_column_name[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (!SDDS_SetColumn(&SDDS_dataset, SDDS_SET_BY_NAME, ptr, rows, orig_column_name[i])) {
          fprintf(stderr, "Error: Problem setting column '%s'.\n", orig_column_name[i]);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    }

    for (i = 0; i < new_array_names; i++) {
      SDDS_ARRAY *array;
      array = SDDS_GetArray(&SDDS_orig, new_array_name[i], NULL);
      if (!array) {
        fprintf(stderr, "Error: Problem getting array '%s'.\n", new_array_name[i]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!SDDS_SetArray(&SDDS_dataset, new_array_name[i], SDDS_CONTIGUOUS_DATA, array->data, array->dimension)) {
        fprintf(stderr, "Error: Problem setting array '%s'.\n", new_array_name[i]);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      SDDS_FreeArray(array);
    }

    virtual_rows = -1;
    for (i = 0; i < converted_array_names; i++) {
      SDDS_ARRAY *array;
      array = SDDS_GetArray(&SDDS_orig, ca[i].name, NULL);
      if (!array) {
        fprintf(stderr, "Error: Problem getting array '%s'.\n", ca[i].name);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      if ((ca[i].d[0] == NULL) && (ca[i].d[1] == NULL) && (ca[i].d[2] == NULL)) {
        if ((orig_column_names) && (array->elements != rows)) {
          fprintf(stderr, "Error: Cannot convert '%s' because existing columns have a different number of rows.\n", ca[i].name);
          exit(EXIT_FAILURE);
        }
        if ((virtual_rows >= 0) && (array->elements != virtual_rows)) {
          fprintf(stderr, "Error: The number of array elements are not the same.\n");
          exit(EXIT_FAILURE);
        }
        if ((!orig_column_names) && (virtual_rows == -1)) {
          if (!SDDS_LengthenTable(&SDDS_dataset, array->elements)) {
            SDDS_SetError("Unable to lengthen table");
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
        virtual_rows = array->elements;
        if (!SDDS_SetColumn(&SDDS_dataset, SDDS_SET_BY_NAME, array->data, virtual_rows, ca[i].new_name)) {
          fprintf(stderr, "Error: Problem setting column '%s'.\n", ca[i].new_name);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      } else {
        vrows = 0;
        ca[i].dims[0] = ca[i].dims[1] = ca[i].dims[2] = 0;
        for (j = 0; j < array->definition->dimensions; j++) {
          ca[i].dim[j] = NULL;
          if (ca[i].d[j] == NULL) {
            ca[i].dims[j] = array->dimension[j];
            ca[i].dim[j] = malloc(sizeof(long) * ca[i].dims[j]);
            for (k = 0; k < ca[i].dims[j]; k++) {
              ca[i].dim[j][k] = k;
            }
          } else {
            ptr = strtok(ca[i].d[j], ",");
            while (ptr != NULL) {
              ca[i].dim[j] = realloc(ca[i].dim[j], sizeof(long) * (ca[i].dims[j] + 1));
              if (!ca[i].dim[j]) {
                fprintf(stderr, "Error: Memory allocation failed for dimension indices.\n");
                exit(EXIT_FAILURE);
              }

              if ((sscanf(ptr, "%ld", &ca[i].dim[j][ca[i].dims[j]]) != 1) ||
                  (ca[i].dim[j][ca[i].dims[j]] < 0) ||
                  (ca[i].dim[j][ca[i].dims[j]] >= array->dimension[j])) {
                fprintf(stderr, "Error: Invalid value for d%ld: '%s'.\n", j + 1, ptr);
                exit(EXIT_FAILURE);
              }
              ca[i].dims[j]++;
              ptr = strtok(NULL, ",");
            }
          }
          if (j == 0) {
            vrows = ca[i].dims[j];
          } else {
            vrows *= ca[i].dims[j];
          }
        }

        if ((orig_column_names) && (vrows != rows)) {
          fprintf(stderr, "Error: Cannot convert '%s' because existing columns have a different number of rows.\n", ca[i].name);
          exit(EXIT_FAILURE);
        }
        if ((virtual_rows >= 0) && (vrows != virtual_rows)) {
          fprintf(stderr, "Error: The number of array elements are not the same.\n");
          exit(EXIT_FAILURE);
        }
        if ((!orig_column_names) && (virtual_rows == -1)) {
          if (!SDDS_LengthenTable(&SDDS_dataset, vrows)) {
            SDDS_SetError("Unable to lengthen table");
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
        virtual_rows = vrows;

        n = 0;
        switch (ca[i].type) {
        case SDDS_SHORT:
          ca[i].data = malloc(sizeof(short) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((short *)ca[i].data)[n++] = ((short *)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((short *)ca[i].data)[n++] = ((short *)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((short *)ca[i].data)[n++] = ((short *)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                      ca[i].dim[1][k] * array->dimension[2] +
                                                                      ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        case SDDS_USHORT:
          ca[i].data = malloc(sizeof(unsigned short) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((unsigned short *)ca[i].data)[n++] = ((unsigned short *)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((unsigned short *)ca[i].data)[n++] = ((unsigned short *)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((unsigned short *)ca[i].data)[n++] = ((unsigned short *)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                                        ca[i].dim[1][k] * array->dimension[2] +
                                                                                        ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        case SDDS_LONG:
          ca[i].data = malloc(sizeof(int32_t) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((int32_t *)ca[i].data)[n++] = ((int32_t *)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((int32_t *)ca[i].data)[n++] = ((int32_t *)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((int32_t *)ca[i].data)[n++] = ((int32_t *)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                          ca[i].dim[1][k] * array->dimension[2] +
                                                                          ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        case SDDS_ULONG:
          ca[i].data = malloc(sizeof(uint32_t) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((uint32_t *)ca[i].data)[n++] = ((uint32_t *)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((uint32_t *)ca[i].data)[n++] = ((uint32_t *)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((uint32_t *)ca[i].data)[n++] = ((uint32_t *)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                            ca[i].dim[1][k] * array->dimension[2] +
                                                                            ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        case SDDS_LONG64:
          ca[i].data = malloc(sizeof(int64_t) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((int64_t *)ca[i].data)[n++] = ((int64_t *)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((int64_t *)ca[i].data)[n++] = ((int64_t *)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((int64_t *)ca[i].data)[n++] = ((int64_t *)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                          ca[i].dim[1][k] * array->dimension[2] +
                                                                          ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        case SDDS_ULONG64:
          ca[i].data = malloc(sizeof(uint64_t) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((uint64_t *)ca[i].data)[n++] = ((uint64_t *)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((uint64_t *)ca[i].data)[n++] = ((uint64_t *)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((uint64_t *)ca[i].data)[n++] = ((uint64_t *)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                            ca[i].dim[1][k] * array->dimension[2] +
                                                                            ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        case SDDS_FLOAT:
          ca[i].data = malloc(sizeof(float) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((float *)ca[i].data)[n++] = ((float *)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((float *)ca[i].data)[n++] = ((float *)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((float *)ca[i].data)[n++] = ((float *)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                      ca[i].dim[1][k] * array->dimension[2] +
                                                                      ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        case SDDS_DOUBLE:
          ca[i].data = malloc(sizeof(double) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((double *)ca[i].data)[n++] = ((double *)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((double *)ca[i].data)[n++] = ((double *)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((double *)ca[i].data)[n++] = ((double *)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                        ca[i].dim[1][k] * array->dimension[2] +
                                                                        ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        case SDDS_STRING:
          ca[i].data = malloc(sizeof(char *) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((char **)ca[i].data)[n++] = ((char **)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((char **)ca[i].data)[n++] = ((char **)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((char **)ca[i].data)[n++] = ((char **)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                      ca[i].dim[1][k] * array->dimension[2] +
                                                                      ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        case SDDS_CHARACTER:
          ca[i].data = malloc(sizeof(char) * vrows);
          if (ca[i].dims[1] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              ((char *)ca[i].data)[n++] = ((char *)array->data)[ca[i].dim[0][j]];
            }
          } else if (ca[i].dims[2] == 0) {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                ((char *)ca[i].data)[n++] = ((char *)array->data)[ca[i].dim[0][j] * array->dimension[1] + ca[i].dim[1][k]];
              }
            }
          } else {
            for (j = 0; j < ca[i].dims[0]; j++) {
              for (k = 0; k < ca[i].dims[1]; k++) {
                for (m = 0; m < ca[i].dims[2]; m++) {
                  ((char *)ca[i].data)[n++] = ((char *)array->data)[ca[i].dim[0][j] * (array->dimension[1] * array->dimension[2]) +
                                                                    ca[i].dim[1][k] * array->dimension[2] +
                                                                    ca[i].dim[2][m]];
                }
              }
            }
          }
          break;

        default:
          fprintf(stderr, "Error: Unsupported data type for array '%s'.\n", ca[i].name);
          exit(EXIT_FAILURE);
        }

        if (!SDDS_SetColumn(&SDDS_dataset, SDDS_SET_BY_NAME, ca[i].data, virtual_rows, ca[i].new_name)) {
          fprintf(stderr, "Error: Problem setting column '%s'.\n", ca[i].new_name);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }

        for (j = 0; j < array->definition->dimensions; j++) {
          if (ca[i].dim[j] != NULL) {
            free(ca[i].dim[j]);
          }
        }
        if (ca[i].data != NULL) {
          free(ca[i].data);
        }
      }
    }

    if (!SDDS_WritePage(&SDDS_dataset)) {
      fprintf(stderr, "Error: Problem writing page to file '%s'.\n", output);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (!SDDS_Terminate(&SDDS_orig) || !SDDS_Terminate(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (tmpfile_used && !replaceFileAndBackUp(input, output)) {
    exit(EXIT_FAILURE);
  }

  if (ca != NULL) {
    free(ca);
  }
  if (buffer != NULL) {
    free(buffer);
  }
  for (i = 0; i < orig_array_names; i++) {
    if (orig_array_name[i] != NULL) {
      free(orig_array_name[i]);
    }
  }
  if (orig_array_name != NULL) {
    free(orig_array_name);
  }
  for (i = 0; i < orig_parameter_names; i++) {
    if (orig_parameter_name[i] != NULL) {
      free(orig_parameter_name[i]);
    }
  }
  if (orig_parameter_name != NULL) {
    free(orig_parameter_name);
  }
  for (i = 0; i < orig_column_names; i++) {
    if (orig_column_name[i] != NULL) {
      free(orig_column_name[i]);
    }
  }
  if (orig_column_name != NULL) {
    free(orig_column_name);
  }
  if (description_text != NULL) {
    free(description_text);
  }
  if (description_contents != NULL) {
    free(description_contents);
  }

  return EXIT_SUCCESS;
}
