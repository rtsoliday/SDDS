/**
 * @file sddscheck.c
 * @brief Validates and checks an SDDS file for corruption or issues.
 *
 * @details
 * This program reads one or more SDDS (Self Describing Data Set) files and determines their validity.
 * It processes each file by verifying its structure, pages, and data, and outputs the status:
 * - `"ok"` if the file is valid.
 * - `"nonexistent"` if the file does not exist.
 * - `"badHeader"` if the file has an invalid header.
 * - `"corrupted"` if the file contains errors.
 * For multiple files, each output line is prefixed with the corresponding file name.
 *
 * @section Usage
 * ```
 * sddscheck [options] <filename|directory> [<filename|directory>...]
 * ```
 *
 * @section Options
 * | Option                                | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-printErrors`                        | Outputs detailed error messages to stderr.                                           |
 * | `-threads=<number>`                   | Number of input files to check concurrently.                                         |
 * | `-summary`                            | Prints aggregate counts after per-file output.                                      |
 * | `-failuresOnly`                       | Suppresses `ok` results.                                                            |
 * | `-failOnError`                        | Exits nonzero if any checked file is not `ok`.                                      |
 * | `-recursive`                          | Recursively expands directory arguments.                                            |
 * | `-pattern=<glob>`                     | Filters directory expansion by file-name wildcard.                                  |
 * | `-maxErrors=<number>`                 | Stops after the specified number of failed files.                                   |
 * | `-showPages`                          | Adds the number of pages read to each result line.                                  |
 * | `-checkDefinitionsOnly`               | Validates only the SDDS header and definitions.                                     |
 * | `-verbose`                            | Adds file size and SDDS layout metadata to each result line.                        |
 * | `-stdin`                              | Reads file or directory names from standard input.                                  |
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
 * M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if !defined(_WIN32) || defined(_MINGW)
#  include <dirent.h>
#  include <unistd.h>
#  define SDDSCHECK_USE_DIRENT 1
#else
#  define SDDSCHECK_USE_DIRENT 0
#endif

#if defined(linux) || (defined(_WIN32) && !defined(_MINGW))
#  include <omp.h>
#  define SDDSCHECK_USE_OPENMP 1
#else
#  define SDDSCHECK_USE_OPENMP 0
#endif

typedef enum {
  CLO_PRINTERRORS = 0,
  CLO_THREADS,
  CLO_SUMMARY,
  CLO_FAILURESONLY,
  CLO_FAILONERROR,
  CLO_RECURSIVE,
  CLO_PATTERN,
  CLO_MAXERRORS,
  CLO_SHOWPAGES,
  CLO_CHECKDEFINITIONSONLY,
  CLO_VERBOSE,
  CLO_STDIN
} OptionType;

#define N_OPTIONS 12

static char *option[N_OPTIONS] = {
  "printErrors",
  "threads",
  "summary",
  "failuresOnly",
  "failOnError",
  "recursive",
  "pattern",
  "maxErrors",
  "showPages",
  "checkDefinitionsOnly",
  "verbose",
  "stdin"
};

typedef enum {
  CHECK_OK = 0,
  CHECK_NONEXISTENT,
  CHECK_BADHEADER,
  CHECK_CORRUPTED
} CheckStatus;

typedef struct {
  char **item;
  long items;
  long allocated;
} StringList;

typedef struct {
  long checkDefinitionsOnly;
} CheckOptions;

typedef struct {
  int checked;
  CheckStatus status;
  long pagesRead;
  long long sizeBytes;
  int sizeKnown;
  int dataMode;
  int columns;
  int parameters;
  int arrays;
  int associates;
  int metadataKnown;
} CheckResult;

char *usage =
  "sddscheck [options] <filename|directory> [<filename|directory>...]\n\n"
  "This program allows you to determine whether SDDS files have been\n"
  "corrupted. It reads the entire file and prints a message to stdout.\n"
  "\n"
  "If the file is ok, \"ok\" is printed.\n"
  "If the file has a problem, one of the following will be printed:\n"
  "  - \"nonexistent\": The file does not exist.\n"
  "  - \"badHeader\": The file header is invalid.\n"
  "  - \"corrupted\": The file contains errors.\n"
  "If multiple files are provided, each output line is prefixed with the file name.\n"
  "\n"
  "Options:\n"
  "  -printErrors: Deliver error messages to stderr.\n"
  "  -threads=<number>: Number of input files to check concurrently (default: 1).\n"
  "  -summary: Print aggregate status counts after per-file output.\n"
  "  -failuresOnly: Suppress ok result lines.\n"
  "  -failOnError: Exit with status 1 if any checked file is not ok.\n"
  "  -recursive: Recursively expand directory arguments.\n"
  "  -pattern=<glob>: Include only matching base names during directory expansion.\n"
  "                   May be repeated. If omitted with -recursive, all files are included.\n"
  "  -maxErrors=<number>: Stop starting new checks after this many failed files.\n"
  "  -showPages: Add pagesRead=<number> to each result line.\n"
  "  -checkDefinitionsOnly: Validate only SDDS header and definitions.\n"
  "  -verbose: Add file size, page count, and SDDS layout metadata.\n"
  "  -stdin: Read additional file or directory names from standard input.\n"
  "\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

static void *checkedMalloc(size_t size) {
  void *ptr;
  if (size == 0)
    size = 1;
  ptr = malloc(size);
  if (!ptr)
    SDDS_Bomb("memory allocation failure");
  return ptr;
}

static void *checkedRealloc(void *ptr, size_t size) {
  if (size == 0)
    size = 1;
  ptr = realloc(ptr, size);
  if (!ptr)
    SDDS_Bomb("memory allocation failure");
  return ptr;
}

static char *checkedStrdup(const char *text) {
  char *copy;
  size_t length;
  if (!text)
    text = "";
  length = strlen(text) + 1;
  copy = checkedMalloc(length);
  memcpy(copy, text, length);
  return copy;
}

static void stringListAppend(StringList *list, const char *text) {
  if (list->items >= list->allocated) {
    list->allocated = list->allocated ? 2 * list->allocated : 16;
    list->item = checkedRealloc(list->item, sizeof(*list->item) * list->allocated);
  }
  list->item[list->items++] = checkedStrdup(text);
}

static void stringListFree(StringList *list) {
  long i;
  for (i = 0; i < list->items; i++)
    free(list->item[i]);
  free(list->item);
  memset(list, 0, sizeof(*list));
}

static int compareStringPointers(const void *left, const void *right) {
  const char *const *leftString = left;
  const char *const *rightString = right;
  return strcmp(*leftString, *rightString);
}

static const char *checkStatusString(CheckStatus status) {
  switch (status) {
  case CHECK_OK:
    return "ok";
  case CHECK_NONEXISTENT:
    return "nonexistent";
  case CHECK_BADHEADER:
    return "badHeader";
  case CHECK_CORRUPTED:
    return "corrupted";
  }
  return "corrupted";
}

static const char *dataModeString(int mode) {
  switch (mode) {
  case SDDS_BINARY:
    return "binary";
  case SDDS_ASCII:
    return "ascii";
  case SDDS_PARALLEL:
    return "parallel";
  default:
    return "unknown";
  }
}

static const char *baseName(const char *path) {
  const char *slash;
  const char *backslash;

  slash = strrchr(path, '/');
  backslash = strrchr(path, '\\');
  if (backslash && (!slash || backslash > slash))
    slash = backslash;
  return slash ? slash + 1 : path;
}

static int matchesPatterns(const char *path, const StringList *patterns) {
  long i;
  const char *name;

  if (!patterns->items)
    return 1;
  name = baseName(path);
  for (i = 0; i < patterns->items; i++) {
    if (wild_match((char *)name, patterns->item[i]))
      return 1;
  }
  return 0;
}

static int isDirectory(const char *path) {
  struct stat statBuffer;
  if (stat(path, &statBuffer) != 0)
    return 0;
  return S_ISDIR(statBuffer.st_mode);
}

static char *joinPath(const char *directory, const char *name) {
  char *path;
  size_t directoryLength, nameLength, length;
  int needsSlash;

  directoryLength = strlen(directory);
  nameLength = strlen(name);
  needsSlash = directoryLength && directory[directoryLength - 1] != '/' && directory[directoryLength - 1] != '\\';
  length = directoryLength + needsSlash + nameLength + 1;
  path = checkedMalloc(length);
  snprintf(path, length, "%s%s%s", directory, needsSlash ? "/" : "", name);
  return path;
}

static void collectDirectoryFiles(const char *directory, long recursive, const StringList *patterns, StringList *files) {
#if SDDSCHECK_USE_DIRENT
  DIR *dir;
  struct dirent *entry;
  StringList entries = {NULL, 0, 0};
  long i;

  dir = opendir(directory);
  if (!dir) {
    fprintf(stderr, "warning: unable to read directory %s: %s\n", directory, strerror(errno));
    return;
  }

  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    stringListAppend(&entries, entry->d_name);
  }
  closedir(dir);

  if (entries.items > 1)
    qsort(entries.item, entries.items, sizeof(*entries.item), compareStringPointers);

  for (i = 0; i < entries.items; i++) {
    char *path;
    struct stat statBuffer;

    path = joinPath(directory, entries.item[i]);
    if (lstat(path, &statBuffer) != 0) {
      free(path);
      continue;
    }
    if (S_ISDIR(statBuffer.st_mode)) {
      if (recursive)
        collectDirectoryFiles(path, recursive, patterns, files);
    } else if (S_ISREG(statBuffer.st_mode) && matchesPatterns(path, patterns)) {
      stringListAppend(files, path);
    }
    free(path);
  }
  stringListFree(&entries);
#else
  (void)directory;
  (void)recursive;
  (void)patterns;
  (void)files;
  SDDS_Bomb("-recursive and directory -pattern expansion are not supported on this platform");
#endif
}

static void readInputNamesFromStdin(StringList *input) {
  char buffer[4096];

  while (fgets(buffer, sizeof(buffer), stdin)) {
    size_t length = strlen(buffer);
    while (length && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r'))
      buffer[--length] = 0;
    if (!length)
      continue;
    stringListAppend(input, buffer);
  }
}

static CheckResult checkFile(char *input, const CheckOptions *options, long print_errors) {
  SDDS_DATASET SDDS_input;
  CheckResult result;
  struct stat statBuffer;
  long retval;

  memset(&result, 0, sizeof(result));
  result.checked = 1;
  result.status = CHECK_CORRUPTED;
  result.dataMode = 0;

  if (stat(input, &statBuffer) != 0) {
    result.status = CHECK_NONEXISTENT;
    return result;
  }
  result.sizeKnown = 1;
  result.sizeBytes = (long long)statBuffer.st_size;

  if (!SDDS_InitializeInput(&SDDS_input, input)) {
    if (print_errors)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    else
      SDDS_ClearErrors();
    result.status = CHECK_BADHEADER;
    return result;
  }

  result.metadataKnown = 1;
  result.dataMode = SDDS_input.layout.data_mode.mode;
  result.columns = SDDS_input.layout.n_columns;
  result.parameters = SDDS_input.layout.n_parameters;
  result.arrays = SDDS_input.layout.n_arrays;
  result.associates = SDDS_input.layout.n_associates;

  if (options->checkDefinitionsOnly) {
    result.status = CHECK_OK;
    SDDS_Terminate(&SDDS_input);
    return result;
  }

  while ((retval = SDDS_ReadPage(&SDDS_input)) > 0) {
    /* Loop continues until EOF or an error occurs. */
    result.pagesRead++;
  }

  if (retval == -1) {
    SDDS_Terminate(&SDDS_input);
    result.status = CHECK_OK;
    return result;
  }

  if (print_errors)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  else
    SDDS_ClearErrors();
  SDDS_Terminate(&SDDS_input);
  result.status = CHECK_CORRUPTED;
  return result;
}

static void printCheckResult(const char *input, const CheckResult *result, long singleResult, long showPages, long verbose) {
  if (singleResult && !showPages && !verbose) {
    puts(checkStatusString(result->status));
    return;
  }

  printf("%s: %s", input, checkStatusString(result->status));
  if (showPages || verbose)
    printf(" pagesRead=%ld", result->pagesRead);
  if (verbose) {
    if (result->sizeKnown)
      printf(" sizeBytes=%lld", result->sizeBytes);
    if (result->metadataKnown) {
      printf(" dataMode=%s columns=%d parameters=%d arrays=%d associates=%d",
             dataModeString(result->dataMode), result->columns, result->parameters, result->arrays, result->associates);
    }
  }
  putchar('\n');
}

static void printSummary(long checked, long *statusCounts) {
  printf("checked: %ld ok: %ld nonexistent: %ld badHeader: %ld corrupted: %ld\n",
         checked, statusCounts[CHECK_OK], statusCounts[CHECK_NONEXISTENT], statusCounts[CHECK_BADHEADER], statusCounts[CHECK_CORRUPTED]);
}

int main(int argc, char **argv) {
  StringList rawInput = {NULL, 0, 0};
  StringList input = {NULL, 0, 0};
  StringList patterns = {NULL, 0, 0};
  CheckOptions checkOptions;
  CheckResult *result;
  long i_arg, print_errors, threads, summary, failuresOnly, failOnError, recursive, useStdin, maxErrors, showPages, verbose;
  long checked, failures, statusCounts[4];
  SCANNED_ARG *s_arg;

  /* Register the program name for error reporting. */
  SDDS_RegisterProgramName(argv[0]);

  /* Parse command-line arguments. */
  argc = scanargs(&s_arg, argc, argv);
  if (!s_arg || argc < 2) {
    bomb(NULL, usage); /* Display usage and exit if arguments are insufficient. */
  }

  memset(&checkOptions, 0, sizeof(checkOptions));
  memset(statusCounts, 0, sizeof(statusCounts));
  print_errors = 0;
  threads = 1;
  summary = 0;
  failuresOnly = 0;
  failOnError = 0;
  recursive = 0;
  useStdin = 0;
  maxErrors = 0;
  showPages = 0;
  verbose = 0;

  /* Process each command-line argument. */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      /* Match recognized options. */
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_PRINTERRORS:
        print_errors = 1;
        break;
      case CLO_THREADS:
        if (s_arg[i_arg].n_items != 2 ||
            !sscanf(s_arg[i_arg].list[1], "%ld", &threads) ||
            threads < 1 ||
            threads > INT_MAX)
          SDDS_Bomb("invalid -threads syntax");
        break;
      case CLO_SUMMARY:
        summary = 1;
        break;
      case CLO_FAILURESONLY:
        failuresOnly = 1;
        break;
      case CLO_FAILONERROR:
        failOnError = 1;
        break;
      case CLO_RECURSIVE:
        recursive = 1;
        break;
      case CLO_PATTERN:
        if (s_arg[i_arg].n_items != 2 || !strlen(s_arg[i_arg].list[1]))
          SDDS_Bomb("invalid -pattern syntax");
        stringListAppend(&patterns, s_arg[i_arg].list[1]);
        break;
      case CLO_MAXERRORS:
        if (s_arg[i_arg].n_items != 2 ||
            !sscanf(s_arg[i_arg].list[1], "%ld", &maxErrors) ||
            maxErrors < 1)
          SDDS_Bomb("invalid -maxErrors syntax");
        break;
      case CLO_SHOWPAGES:
        showPages = 1;
        break;
      case CLO_CHECKDEFINITIONSONLY:
        checkOptions.checkDefinitionsOnly = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_STDIN:
        useStdin = 1;
        break;
      default:
        SDDS_Bomb("unknown option given"); /* Handle unrecognized options. */
        break;
      }
    } else {
      stringListAppend(&rawInput, s_arg[i_arg].list[0]);
    }
  }

  if (useStdin)
    readInputNamesFromStdin(&rawInput);

  if (rawInput.items < 1)
    bomb(NULL, usage);

  for (i_arg = 0; i_arg < rawInput.items; i_arg++) {
    if ((recursive || patterns.items) && isDirectory(rawInput.item[i_arg]))
      collectDirectoryFiles(rawInput.item[i_arg], recursive, &patterns, &input);
    else
      stringListAppend(&input, rawInput.item[i_arg]);
  }

  result = checkedMalloc(sizeof(*result) * input.items);
  memset(result, 0, sizeof(*result) * input.items);
  checked = 0;
  failures = 0;

  if (input.items == 1 && !summary && !failuresOnly && !showPages && !verbose) {
    result[0] = checkFile(input.item[0], &checkOptions, 0);
    checked = 1;
    statusCounts[result[0].status]++;
    failures = result[0].status != CHECK_OK;
    if (print_errors && result[0].status == CHECK_CORRUPTED)
      checkFile(input.item[0], &checkOptions, 1);
    printCheckResult(input.item[0], &result[0], 1, showPages, verbose);
    if (print_errors && result[0].status == CHECK_BADHEADER)
      checkFile(input.item[0], &checkOptions, 1);
    free(result);
    stringListFree(&patterns);
    stringListFree(&rawInput);
    stringListFree(&input);
    return (failOnError && failures) ? 1 : 0;
  }

  if (threads <= 1 || input.items <= 1) {
    for (i_arg = 0; i_arg < input.items; i_arg++) {
      result[i_arg] = checkFile(input.item[i_arg], &checkOptions, print_errors);
      checked++;
      statusCounts[result[i_arg].status]++;
      if (result[i_arg].status != CHECK_OK) {
        failures++;
        if (maxErrors && failures >= maxErrors)
          break;
      }
    }
  } else {
    long sharedFailures = 0;
    if (threads > input.items)
      threads = input.items;
#if SDDSCHECK_USE_OPENMP
    omp_set_num_threads((int)threads);
#pragma omp parallel for schedule(dynamic)
    for (i_arg = 0; i_arg < input.items; i_arg++) {
      long failuresSeen = 0;
      if (maxErrors) {
#pragma omp atomic read
        failuresSeen = sharedFailures;
        if (failuresSeen >= maxErrors)
          continue;
      }
      result[i_arg] = checkFile(input.item[i_arg], &checkOptions, 0);
      if (maxErrors && result[i_arg].status != CHECK_OK) {
#pragma omp atomic update
        sharedFailures++;
      }
    }
#else
    for (i_arg = 0; i_arg < input.items; i_arg++) {
      if (maxErrors && sharedFailures >= maxErrors)
        break;
      result[i_arg] = checkFile(input.item[i_arg], &checkOptions, 0);
      if (result[i_arg].status != CHECK_OK)
        sharedFailures++;
    }
#endif
    if (print_errors) {
      for (i_arg = 0; i_arg < input.items; i_arg++) {
        if (result[i_arg].checked && result[i_arg].status != CHECK_OK && result[i_arg].status != CHECK_NONEXISTENT)
          checkFile(input.item[i_arg], &checkOptions, 1);
      }
    }
    for (i_arg = 0; i_arg < input.items; i_arg++) {
      if (!result[i_arg].checked)
        continue;
      checked++;
      statusCounts[result[i_arg].status]++;
      if (result[i_arg].status != CHECK_OK)
        failures++;
    }
  }

  for (i_arg = 0; i_arg < input.items; i_arg++) {
    if (!result[i_arg].checked)
      continue;
    if (failuresOnly && result[i_arg].status == CHECK_OK)
      continue;
    printCheckResult(input.item[i_arg], &result[i_arg], input.items == 1, showPages, verbose);
  }
  if (summary)
    printSummary(checked, statusCounts);

  free(result);
  stringListFree(&patterns);
  stringListFree(&rawInput);
  stringListFree(&input);
  return (failOnError && failures) ? 1 : 0;
}
