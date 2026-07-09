/**
 * @file sddscombinelogfiles.c
 * @brief Combines multiple SDDS log files into a single file, retaining only common timestamps.
 *
 * @details
 * This program processes SDDS log files in the one-PV-per-file format, merging them into a single SDDS file. 
 * It retains only the timestamps common across all input files and supports flexible input-output configurations 
 * such as pipe-based I/O and overwriting existing output files.
 *
 * @section Usage
 * ```
 * sddscombinelogfiles [<SDDSinputfilelist>] [<SDDSoutputfile>] 
 *                     [-pipe=[output]] 
 *                     [-overwrite]
 *                     [-threads=<number>]
 * ```
 *
 * @section Options
 * | Option             | Description                                                     |
 * |--------------------|-----------------------------------------------------------------|
 * | `-pipe`            | Use pipe output for the SDDS file instead of writing to a file. |
 * | `-overwrite`       | Overwrite the output file if it already exists.                 |
 * | `-threads`         | Number of input files to load concurrently.                     |
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
 * R. Soliday, L. Emery
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"

/* Enumeration for option types */
enum option_type {
  SET_PIPE,
  SET_OVERWRITE,
  SET_THREADS,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "pipe",
  "overwrite",
  "threads"
};

const char *USAGE =
  "Usage: sddscombinelogfiles [<SDDSinputfilelist>] [<SDDSoutputfile>]\n"
  "       [-pipe=[output]] [-overwrite] [-threads=<number>]\n\n"
  "This program combines data logger output files that are in the one-PV-per-file format.\n"
  "Only the timestamps present in all input files are retained in the output file.\n\n"
  "Options:\n"
  "  -pipe=[output]    Specify the pipe output.\n"
  "  -overwrite        Overwrite the output file if it already exists.\n"
  "  -threads=<number> Number of input files to load concurrently. The default is 1.\n\n"
  "Example:\n"
  "  sddscombinelogfiles input1.sdds input2.sdds output.sdds -overwrite\n\n"
  "Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

typedef struct {
  double *timeValues;
  double *dataValues;
  char *dataName;
  int64_t rows;
} LOG_PAGE_DATA;

typedef struct {
  LOG_PAGE_DATA *page;
  int pages;
  int status;
  char error[1024];
} LOG_FILE_DATA;

static int LoadLogFile(const char *filename, LOG_FILE_DATA *logFile);
static void FreeLogFileData(LOG_FILE_DATA *logFile);

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_output;
  SCANNED_ARG *s_arg;
  KEYED_EQUIVALENT **keyGroup = NULL;
  long keyGroups = 0;
  char **inputfile = NULL;
  int inputfiles = 0;
  char *outputfile = NULL;
  int i_arg, n, row, z;
  int threads = 1;
  int64_t i, j, m, r, s;
  unsigned long pipeFlags = 0;
  int overwrite = 0;
  double **timeValues = NULL;
  double **dataValues = NULL;
  short **flag = NULL;
  int64_t *rows = NULL;
  char **dataNames = NULL;
  char **uniqueDataName = NULL;
  int uniqueDataNames = 0;
  int page = 0;
  int pages;
  int found;
  double *outputTimeValues = NULL;
  double **outputDataValues = NULL;
  int64_t allocated_rows = 0;
  int **array = NULL;
  int *arrayCount;
  LOG_FILE_DATA *logFile = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);

  if (argc < 3) {
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_OVERWRITE:
        overwrite = 1;
        break;
      case SET_THREADS:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%d", &threads) != 1 ||
            threads < 1) {
          fprintf(stderr, "Error: Invalid -threads option syntax.\n");
          return EXIT_FAILURE;
        }
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1,
                               s_arg[i_arg].n_items - 1,
                               &pipeFlags)) {
          fprintf(stderr, "Error: Invalid -pipe option syntax.\n");
          return EXIT_FAILURE;
        }
        if (pipeFlags & USE_STDIN) {
          fprintf(stderr, "Error: -pipe=in is not supported.\n");
          return EXIT_FAILURE;
        }
        break;
      default:
        fprintf(stderr, "Error: Unrecognized option.\n%s", USAGE);
        return EXIT_FAILURE;
      }
    } else {
      inputfile = trealloc(inputfile, sizeof(*inputfile) * (inputfiles + 1));
      inputfile[inputfiles++] = s_arg[i_arg].list[0];
    }
  }

  if (inputfiles > 1) {
    if (!(pipeFlags & USE_STDOUT)) {
      outputfile = inputfile[--inputfiles];
      if (fexists(outputfile) && !overwrite) {
        fprintf(stderr, "Error: Output file '%s' already exists. Use -overwrite to replace it.\n", outputfile);
        return EXIT_FAILURE;
      }
    }
  } else if (inputfiles == 1) {
    if ((pipeFlags & USE_STDOUT) && outputfile) {
      fprintf(stderr, "Error: Too many filenames provided with -pipe=output.\n");
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Error: No input filenames provided.\n%s", USAGE);
    return EXIT_FAILURE;
  }

  if (threads > inputfiles)
    threads = inputfiles;

  logFile = calloc(inputfiles, sizeof(*logFile));
  if (!logFile) {
    fprintf(stderr, "Error: Memory allocation failure.\n");
    return EXIT_FAILURE;
  }

#pragma omp parallel for if (threads > 1 && inputfiles > 1) num_threads(threads) schedule(dynamic)
  for (i = 0; i < inputfiles; i++) {
    LoadLogFile(inputfile[i], &logFile[i]);
  }

  pages = 0;
  for (i = 0; i < inputfiles; i++) {
    if (!logFile[i].status) {
      fprintf(stderr, "%s\n", logFile[i].error);
      for (j = 0; j < inputfiles; j++)
        FreeLogFileData(&logFile[j]);
      free(logFile);
      return EXIT_FAILURE;
    }
    pages += logFile[i].pages;
  }

  timeValues = malloc(sizeof(*timeValues) * pages);
  dataValues = malloc(sizeof(*dataValues) * pages);
  dataNames = malloc(sizeof(*dataNames) * pages);
  rows = malloc(sizeof(*rows) * pages);
  if (!timeValues || !dataValues || !dataNames || !rows) {
    fprintf(stderr, "Error: Memory allocation failure.\n");
    for (i = 0; i < inputfiles; i++)
      FreeLogFileData(&logFile[i]);
    free(logFile);
    return EXIT_FAILURE;
  }

  page = 0;
  for (i = 0; i < inputfiles; i++) {
    int filePage;
    for (filePage = 0; filePage < logFile[i].pages; filePage++) {
      timeValues[page] = logFile[i].page[filePage].timeValues;
      dataValues[page] = logFile[i].page[filePage].dataValues;
      dataNames[page] = logFile[i].page[filePage].dataName;
      rows[page] = logFile[i].page[filePage].rows;
      logFile[i].page[filePage].timeValues = NULL;
      logFile[i].page[filePage].dataValues = NULL;
      logFile[i].page[filePage].dataName = NULL;
      page++;
    }
    FreeLogFileData(&logFile[i]);
  }
  free(logFile);

  pages = page;

  /* Identify unique data names */
  for (page = 0; page < pages; page++) {
    found = 0;
    for (i = 0; i < uniqueDataNames; i++) {
      if (strcmp(dataNames[page], uniqueDataName[i]) == 0) {
        found = 1;
        break;
      }
    }
    if (!found) {
      uniqueDataName = realloc(uniqueDataName, sizeof(*uniqueDataName) * (uniqueDataNames + 1));
      SDDS_CopyString(&uniqueDataName[uniqueDataNames], dataNames[page]);
      uniqueDataNames++;
    }
  }

  /* Initialize output SDDS file */
  if (!SDDS_InitializeOutput(&SDDS_output, SDDS_BINARY, 0, NULL, NULL, outputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_DefineSimpleColumn(&SDDS_output, "Time", "s", SDDS_DOUBLE)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  for (i = 0; i < uniqueDataNames; i++) {
    if (!SDDS_DefineSimpleColumn(&SDDS_output, uniqueDataName[i], NULL, SDDS_DOUBLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
  }

  outputDataValues = malloc(sizeof(*outputDataValues) * uniqueDataNames);
  if (uniqueDataNames == 1) {
    /* Single PV: Concatenate all data */
    for (page = 0; page < pages; page++) {
      allocated_rows += rows[page];
    }

    outputTimeValues = malloc(sizeof(*outputTimeValues) * allocated_rows);
    outputDataValues[0] = malloc(sizeof(*(outputDataValues[0])) * allocated_rows);

    i = 0;
    for (page = 0; page < pages; page++) {
      for (j = 0; j < rows[page]; j++) {
        outputTimeValues[i] = timeValues[page][j];
        outputDataValues[0][i] = dataValues[page][j];
        i++;
      }
    }
  } else {
    /* Multiple PVs: Retain only common timestamps */
    flag = malloc(sizeof(*flag) * pages);
    for (page = 0; page < pages; page++) {
      flag[page] = calloc(rows[page], sizeof(*(flag[page])));
    }

    array = malloc(sizeof(*array) * uniqueDataNames);
    arrayCount = calloc(uniqueDataNames, sizeof(*arrayCount));

    for (i = 0; i < uniqueDataNames; i++) {
      for (page = 0; page < pages; page++) {
        if (strcmp(dataNames[page], uniqueDataName[i]) == 0) {
          arrayCount[i]++;
          if (arrayCount[i] == 1) {
            array[i] = malloc(sizeof(*(array[i])));
          } else {
            array[i] = realloc(array[i], sizeof(*(array[i])) * arrayCount[i]);
          }
          array[i][arrayCount[i] - 1] = page;
        }
      }
    }

    for (i = 0; i < arrayCount[0]; i++) {
      keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_DOUBLE, timeValues[array[0][i]], rows[array[0][i]]);
      for (n = 1; n < uniqueDataNames; n++) {
        for (m = 0; m < arrayCount[n]; m++) {
          if ((i == m) && (rows[array[0][i]] == rows[array[n][m]]) && (rows[array[0][i]] > 10)) {
            if ((timeValues[array[0][i]][0] == timeValues[array[n][m]][0]) &&
                (timeValues[array[0][i]][1] == timeValues[array[n][m]][1]) &&
                (timeValues[array[0][i]][rows[array[0][i]] - 2] == timeValues[array[n][m]][rows[array[n][m]] - 2]) &&
                (timeValues[array[0][i]][rows[array[0][i]] - 1] == timeValues[array[n][m]][rows[array[n][m]] - 1])) {
              /* Assume the entire page matches because it has the same number of rows and key timestamps match */
              for (r = 0; r < rows[array[n][m]]; r++) {
                if (flag[array[n][m]][r]) {
                  continue;
                }
                flag[array[0][i]][r] += 1;
                flag[array[n][m]][r] = 1;
              }
            }
          }

          for (r = 0; r < rows[array[n][m]]; r++) {
            if (flag[array[n][m]][r]) {
              continue;
            }
            row = FindMatchingKeyGroup(keyGroup, keyGroups, SDDS_DOUBLE, &(timeValues[array[n][m]][r]), 1);
            if (row >= 0) {
              flag[array[0][i]][row] += 1;
              flag[array[n][m]][r] = 1;
            }
          }
        }
      }

      for (j = 0; j < keyGroups; j++) {
        free(keyGroup[j]->equivalent);
        free(keyGroup[j]);
      }
      free(keyGroup);
    }

    z = uniqueDataNames - 1;
    for (n = 0; n < arrayCount[0]; n++) {
      for (m = 0; m < rows[array[0][n]]; m++) {
        if (flag[array[0][n]][m] >= z) {
          allocated_rows++;
        }
      }
    }

    outputTimeValues = malloc(sizeof(*outputTimeValues) * allocated_rows);
    for (i = 0; i < uniqueDataNames; i++) {
      outputDataValues[i] = malloc(sizeof(*(outputDataValues[i])) * allocated_rows);
    }

    s = 0;
    for (i = 0; i < arrayCount[0]; i++) {
      for (j = 0; j < rows[array[0][i]]; j++) {
        if (flag[array[0][i]][j] >= z) {
          outputTimeValues[s] = timeValues[array[0][i]][j];
          outputDataValues[0][s] = dataValues[array[0][i]][j];
          s++;
        }
      }
    }

    if (s == 0) {
      fprintf(stderr, "Error: No matching 'Time' rows found in input files.\n");
      return EXIT_FAILURE;
    }

    keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_DOUBLE, outputTimeValues, s);

    for (n = 1; n < uniqueDataNames; n++) {
      for (m = 0; m < arrayCount[n]; m++) {
        for (r = 0; r < rows[array[n][m]]; r++) {
          if (flag[array[n][m]][r]) {
            row = FindMatchingKeyGroup(keyGroup, keyGroups, SDDS_DOUBLE, &(timeValues[array[n][m]][r]), 1);
            if (row >= 0) {
              outputDataValues[n][row] = dataValues[array[n][m]][r];
            }
          }
        }
      }
    }

    for (i = 0; i < uniqueDataNames; i++) {
      free(array[i]);
    }

    for (j = 0; j < keyGroups; j++) {
      if (keyGroup[j]->equivalent)
        free(keyGroup[j]->equivalent);
      if (keyGroup[j])
        free(keyGroup[j]);
    }

    for (page = 0; page < pages; page++) {
      free(flag[page]);
    }

    free(array);
    free(keyGroup);
    free(arrayCount);
    free(flag);
  }

  /* Free allocated memory for input data */
  for (page = 0; page < pages; page++) {
    if (timeValues[page])
      free(timeValues[page]);
    if (dataValues[page])
      free(dataValues[page]);
    free(dataNames[page]);
  }
  free(timeValues);
  free(dataValues);
  free(dataNames);

  /* Write the output SDDS file */
  if (!SDDS_WriteLayout(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_StartPage(&SDDS_output, allocated_rows)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_SetColumnFromDoubles(&SDDS_output, SDDS_SET_BY_NAME, outputTimeValues, allocated_rows, "Time")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  for (i = 0; i < uniqueDataNames; i++) {
    if (!SDDS_SetColumnFromDoubles(&SDDS_output, SDDS_SET_BY_NAME, outputDataValues[i], allocated_rows, uniqueDataName[i])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return EXIT_FAILURE;
    }
  }

  if (!SDDS_WriteTable(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  if (!SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return EXIT_FAILURE;
  }

  /* Free allocated memory for output data */
  for (i = 0; i < uniqueDataNames; i++) {
    free(uniqueDataName[i]);
    free(outputDataValues[i]);
  }
  free(outputTimeValues);
  free(outputDataValues);
  free(uniqueDataName);
  free(rows);

  if (inputfiles > 0) {
    free(inputfile);
  }

  free_scanargs(&s_arg, argc);

  return EXIT_SUCCESS;
}

static int LoadLogFile(const char *filename, LOG_FILE_DATA *logFile) {
  SDDS_DATASET SDDS_input;
  char **columnname = NULL;
  int32_t columnnames = 0;
  int dataIndex = -1;
  int32_t readCode;
  int64_t j;
  int initialized = 0;

  logFile->page = NULL;
  logFile->pages = 0;
  logFile->status = 0;
  logFile->error[0] = 0;

  if (!SDDS_InitializeInput(&SDDS_input, (char *)filename)) {
    snprintf(logFile->error, sizeof(logFile->error), "Error: Unable to open '%s'.", filename);
    return 0;
  }
  initialized = 1;

  columnname = SDDS_GetColumnNames(&SDDS_input, &columnnames);
  if (columnname == NULL) {
    snprintf(logFile->error, sizeof(logFile->error), "Error: Unable to read column names from '%s'.", filename);
    goto fail;
  }

  if (columnnames > 3 || columnnames < 2) {
    snprintf(logFile->error, sizeof(logFile->error), "Error: Unexpected number of columns in '%s'.", filename);
    goto fail;
  }

  if (columnnames == 2) {
    if (strcmp("Time", columnname[0]) == 0) {
      dataIndex = 1;
    } else if (strcmp("Time", columnname[1]) == 0) {
      dataIndex = 0;
    } else {
      snprintf(logFile->error, sizeof(logFile->error), "Error: 'Time' column is missing in '%s'.", filename);
      goto fail;
    }
  }

  if (columnnames == 3) {
    if (strcmp("CAerrors", columnname[0]) == 0) {
      if (strcmp("Time", columnname[1]) == 0) {
        dataIndex = 2;
      } else if (strcmp("Time", columnname[2]) == 0) {
        dataIndex = 1;
      } else {
        snprintf(logFile->error, sizeof(logFile->error), "Error: 'Time' column is missing in '%s'.", filename);
        goto fail;
      }
    } else if (strcmp("CAerrors", columnname[1]) == 0) {
      if (strcmp("Time", columnname[0]) == 0) {
        dataIndex = 2;
      } else if (strcmp("Time", columnname[2]) == 0) {
        dataIndex = 0;
      } else {
        snprintf(logFile->error, sizeof(logFile->error), "Error: 'Time' column is missing in '%s'.", filename);
        goto fail;
      }
    } else if (strcmp("CAerrors", columnname[2]) == 0) {
      if (strcmp("Time", columnname[0]) == 0) {
        dataIndex = 1;
      } else if (strcmp("Time", columnname[1]) == 0) {
        dataIndex = 0;
      } else {
        snprintf(logFile->error, sizeof(logFile->error), "Error: 'Time' column is missing in '%s'.", filename);
        goto fail;
      }
    } else {
      snprintf(logFile->error, sizeof(logFile->error), "Error: 'CAerrors' column is missing in '%s'.", filename);
      goto fail;
    }
  }

  while ((readCode = SDDS_ReadTable(&SDDS_input)) > 0) {
    int page = logFile->pages;
    LOG_PAGE_DATA *pageData;
    pageData = realloc(logFile->page, sizeof(*logFile->page) * (page + 1));
    if (!pageData) {
      snprintf(logFile->error, sizeof(logFile->error), "Error: Memory allocation failure.");
      goto fail;
    }
    logFile->page = pageData;
    logFile->page[page].timeValues = NULL;
    logFile->page[page].dataValues = NULL;
    logFile->page[page].dataName = NULL;
    logFile->page[page].rows = SDDS_RowCount(&SDDS_input);
    logFile->pages++;

    if (!SDDS_CopyString(&logFile->page[page].dataName, columnname[dataIndex])) {
      snprintf(logFile->error, sizeof(logFile->error), "Error: Memory allocation failure.");
      goto fail;
    }
    if (logFile->page[page].rows > 0) {
      logFile->page[page].timeValues = SDDS_GetColumnInDoubles(&SDDS_input, "Time");
      if (logFile->page[page].timeValues == NULL) {
        snprintf(logFile->error, sizeof(logFile->error), "Error: Unable to read 'Time' data from '%s'.", filename);
        goto fail;
      }

      logFile->page[page].dataValues = SDDS_GetColumnInDoubles(&SDDS_input, columnname[dataIndex]);
      if (logFile->page[page].dataValues == NULL) {
        snprintf(logFile->error, sizeof(logFile->error), "Error: Unable to read '%s' data from '%s'.", columnname[dataIndex], filename);
        goto fail;
      }
    }
  }
  if (readCode == 0) {
    snprintf(logFile->error, sizeof(logFile->error), "Error: Unable to read data from '%s'.", filename);
    goto fail;
  }

  for (j = 0; j < columnnames; j++)
    free(columnname[j]);
  free(columnname);
  columnname = NULL;

  if (!SDDS_Terminate(&SDDS_input)) {
    initialized = 0;
    snprintf(logFile->error, sizeof(logFile->error), "Error: Unable to close '%s'.", filename);
    goto fail;
  }
  initialized = 0;

  logFile->status = 1;
  return 1;

fail:
  if (columnname) {
    for (j = 0; j < columnnames; j++)
      free(columnname[j]);
    free(columnname);
  }
  if (initialized)
    SDDS_Terminate(&SDDS_input);
  FreeLogFileData(logFile);
  return 0;
}

static void FreeLogFileData(LOG_FILE_DATA *logFile) {
  int page;
  if (!logFile)
    return;
  for (page = 0; page < logFile->pages; page++) {
    free(logFile->page[page].timeValues);
    free(logFile->page[page].dataValues);
    free(logFile->page[page].dataName);
  }
  free(logFile->page);
  logFile->page = NULL;
  logFile->pages = 0;
}
