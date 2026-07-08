/**
 * @file sddsgroupedenvelope.c
 * @brief Create grouped SDDS envelope files without temporary SDDS files.
 *
 * This program reads multipage SDDS files, groups pages by a selected
 * parameter, and writes one output page per group containing requested row-wise
 * statistics.  Accumulators are streaming: by default one input file is open
 * at a time, while -threads uses per-file accumulators that are merged in
 * deterministic input-file order.
 */

#include "mdb.h"
#include "SDDS.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <glob.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(linux) || (defined(_WIN32) && !defined(_MINGW))
#  include <omp.h>
#  define SDDSGROUPEDENVELOPE_USE_OPENMP 1
#else
#  define SDDSGROUPEDENVELOPE_USE_OPENMP 0
#endif

#ifndef PATH_MAX
#  define PATH_MAX 4096
#endif

typedef struct {
  char **item;
  long items;
  long allocated;
} STRING_LIST;

typedef enum {
  STAT_COPY,
  STAT_MAXIMUM,
  STAT_MINIMUM,
  STAT_MEAN,
  STAT_LARGEST,
  STAT_SIGNED_LARGEST,
  STAT_SUM,
  STAT_RMS,
  STAT_STANDARD_DEVIATION,
  STAT_SIGMA,
  STAT_WMEAN,
  STAT_WSTANDARD_DEVIATION,
  STAT_WRMS,
  STAT_WSIGMA,
  STAT_CMAXIMUM,
  STAT_CMINIMUM,
  STAT_PMAXIMUM,
  STAT_PMINIMUM,
  STAT_SLOPE,
  STAT_INTERCEPT,
  STAT_EXMM_MEAN
} STAT_CODE;

typedef struct {
  STAT_CODE code;
  STRING_LIST column;
  char *weightColumn;
  char *functionOf;
  long sumPower;
} RAW_STAT_REQUEST;

typedef struct {
  STAT_CODE code;
  char *sourceColumn;
  char *resultColumn;
  char *weightColumn;
  char *functionOf;
  long sumPower;
  int32_t sourceType;
} STAT_DEFINITION;

typedef struct {
  STRING_LIST pattern;
  STRING_LIST groupValue;
  char *groupBy;
  int32_t groupByType;
  char *output;
  char *inputDir;
  int overwrite;
  int verbose;
  int noWarnings;
  int threads;
  short columnMajorOrder;
  RAW_STAT_REQUEST *request;
  long requests;
  long requestsAllocated;
  STAT_DEFINITION *stat;
  long stats;
  long statsAllocated;
} OPTIONS;

typedef struct {
  int initialized;
  void *copyData;
  double *value1;
  double *value2;
  double *value3;
  double *value4;
  double *sumWeight;
} STAT_ACCUMULATOR;

typedef struct {
  int64_t rows;
  int64_t pages;
  STAT_ACCUMULATOR *stat;
} ROW_ACCUMULATOR;

typedef struct {
  char *name;
  void *value;
  ROW_ACCUMULATOR *rowData;
  long rowDatas;
  long rowDatasAllocated;
  int64_t zeroRowPages;
} ENVELOPE_GROUP;

typedef struct {
  ENVELOPE_GROUP *group;
  long groups;
  long groupsAllocated;
} GROUP_LIST;

typedef struct {
  GROUP_LIST groups;
  int ok;
} FILE_READ_RESULT;

enum {
  PROCESS_ERROR = -1,
  PROCESS_NO_INPUT = 0,
  PROCESS_DONE = 1
};

static char *USAGE =
  "sddsgroupedenvelope [options]\n"
  "\n"
  "Processes pages from files in -inputDir to produce -output with\n"
  "one page for each -groupBy value containing the specified quantities\n"
  "across pages for each row of the specified columns.\n"
  "\n"
  "File selection options:\n"
  "  -pattern <pattern>       Input glob pattern. May be repeated.\n"
  "                           Defaults to PSD-*.xz and PSD-*.gz.\n"
  "  -inputDir <directory>    Directory containing input files (default: current directory).\n"
  "  -groupBy <parameter>     Group pages by this parameter (default: PSName).\n"
  "  -groupValue <value>      Process only this group value. May be repeated.\n"
  "  -output <file>           Output file, relative to current directory (default: PSD-envelope.sdds).\n"
  "  -noOverwrite             Refuse to replace an existing output file.\n"
  "  -majorOrder={row|column} Output data order (default: column).\n"
  "  -threads <number>       Number of input files to read concurrently (default: 1).\n"
  "  -nowarnings              Suppress warnings about degenerate statistics.\n"
  "  -verbose                 Print per-directory and row-count details.\n"
  "  -help                    Show this message.\n"
  "\n"
  "Streaming statistic options. At least one is required:\n"
  "  -copy=<columns>\n"
  "  -maximum=<columns>       -minimum=<columns>       -mean=<columns>\n"
  "  -largest=<columns>       -signedLargest=<columns>\n"
  "  -sum=<power>,<columns>   -rms=<columns>\n"
  "  -standarddeviations=<columns>   -sigma=<columns>\n"
  "  -wmean=<weightColumn>,<columns>\n"
  "  -wstandarddeviations=<weightColumn>,<columns>\n"
  "  -wrms=<weightColumn>,<columns>  -wsigma=<weightColumn>,<columns>\n"
  "  -cmaximum=<indepColumn>,<columns>  -cminimum=<indepColumn>,<columns>\n"
  "  -pmaximum=<indepParameter>,<columns>  -pminimum=<indepParameter>,<columns>\n"
  "  -slope=<indepParameter>,<columns>  -intercept=<indepParameter>,<columns>\n"
  "  -exmmMean=<columns>\n"
  "\n"
  "Exact median, percentile, and decile-range statistics are intentionally not\n"
  "implemented because they require retaining per-row page histories.\n";

static void *xmalloc(size_t size) {
  void *ptr;
  if (size == 0)
    size = 1;
  ptr = malloc(size);
  if (!ptr) {
    fprintf(stderr, "error: memory allocation failed\n");
    exit(EXIT_FAILURE);
  }
  return ptr;
}

static void *xcalloc(size_t count, size_t size) {
  void *ptr;
  if (count == 0 || size == 0) {
    count = size = 1;
  }
  ptr = calloc(count, size);
  if (!ptr) {
    fprintf(stderr, "error: memory allocation failed\n");
    exit(EXIT_FAILURE);
  }
  return ptr;
}

static void *xrealloc(void *ptr, size_t size) {
  if (size == 0)
    size = 1;
  ptr = realloc(ptr, size);
  if (!ptr) {
    fprintf(stderr, "error: memory allocation failed\n");
    exit(EXIT_FAILURE);
  }
  return ptr;
}

static char *xstrdup(const char *s) {
  char *copy;
  size_t len;
  if (!s)
    s = "";
  len = strlen(s);
  copy = xmalloc(len + 1);
  memcpy(copy, s, len + 1);
  return copy;
}

static void string_list_append_owned(STRING_LIST *list, char *value) {
  if (list->items >= list->allocated) {
    list->allocated = list->allocated ? 2 * list->allocated : 8;
    list->item = xrealloc(list->item, sizeof(*list->item) * list->allocated);
  }
  list->item[list->items++] = value;
}

static void string_list_append(STRING_LIST *list, const char *value) {
  string_list_append_owned(list, xstrdup(value));
}

static void string_list_clear(STRING_LIST *list) {
  long i;
  for (i = 0; i < list->items; i++)
    free(list->item[i]);
  free(list->item);
  list->item = NULL;
  list->items = list->allocated = 0;
}

static int string_list_contains(const STRING_LIST *list, const char *value) {
  long i;
  for (i = 0; i < list->items; i++) {
    if (strcmp(list->item[i], value) == 0)
      return 1;
  }
  return 0;
}

static char *trimmed_copy(const char *start, size_t length) {
  const char *end = start + length;
  char *copy;

  while (start < end && isspace((unsigned char)*start))
    start++;
  while (end > start && isspace((unsigned char)*(end - 1)))
    end--;
  copy = xmalloc((size_t)(end - start) + 1);
  memcpy(copy, start, (size_t)(end - start));
  copy[end - start] = 0;
  return copy;
}

static void split_comma_list(const char *value, STRING_LIST *list) {
  const char *start, *p;
  start = value;
  for (p = value; ; p++) {
    if (*p == ',' || *p == 0) {
      char *item = trimmed_copy(start, (size_t)(p - start));
      if (!strlen(item)) {
        fprintf(stderr, "error: empty item in option value '%s'\n", value);
        free(item);
        exit(EXIT_FAILURE);
      }
      string_list_append_owned(list, item);
      if (*p == 0)
        break;
      start = p + 1;
    }
  }
}

static int is_absolute_path(const char *path) {
  if (!path || !path[0])
    return 0;
  if (path[0] == '/')
    return 1;
  if (isalpha((unsigned char)path[0]) && path[1] == ':')
    return 1;
  return 0;
}

static char *join_path(const char *dir, const char *name) {
  size_t dirLen, nameLen, needSlash;
  char *path;

  if (is_absolute_path(name))
    return xstrdup(name);
  if (!dir || !dir[0])
    return xstrdup(name);

  dirLen = strlen(dir);
  nameLen = strlen(name);
  needSlash = dirLen && dir[dirLen - 1] != '/';
  path = xmalloc(dirLen + needSlash + nameLen + 1);
  memcpy(path, dir, dirLen);
  if (needSlash)
    path[dirLen++] = '/';
  memcpy(path + dirLen, name, nameLen + 1);
  return path;
}

static char *current_directory(void) {
  char buffer[PATH_MAX];
  if (!getcwd(buffer, sizeof(buffer))) {
    fprintf(stderr, "error: unable to determine current directory: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  return xstrdup(buffer);
}

static int path_exists(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static int same_existing_file(const char *path1, const char *path2) {
  struct stat st1, st2;
  if (!path1 || !path2)
    return 0;
  if (strcmp(path1, path2) == 0)
    return 1;
  if (stat(path1, &st1) != 0 || stat(path2, &st2) != 0)
    return 0;
  return st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;
}

static int is_directory(const char *path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static const char *base_name(const char *path) {
  const char *slash;
  slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

static int timestamp_seconds(const char *path) {
  const char *tail, *p;
  tail = base_name(path);
  for (p = tail; *p; p++) {
    int h, m, s, n = 0;
    if (!isdigit((unsigned char)*p))
      continue;
    if (sscanf(p, "%2d:%2d:%2d%n", &h, &m, &s, &n) == 3 && n >= 7 &&
        h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60) {
      return h * 3600 + m * 60 + s;
    }
  }
  return -1;
}

static int compare_input_files(const void *a, const void *b) {
  const char *pa = *(const char * const *)a;
  const char *pb = *(const char * const *)b;
  int ta = timestamp_seconds(pa);
  int tb = timestamp_seconds(pb);
  int cmp;

  if (ta >= 0 && tb >= 0 && ta != tb)
    return ta < tb ? -1 : 1;
  cmp = strcmp(base_name(pa), base_name(pb));
  if (cmp)
    return cmp;
  return strcmp(pa, pb);
}

static int option_name_matches(const char *arg, const char *name, const char **value) {
  size_t nameLen;
  const char *body, *eq;

  if (!arg || arg[0] != '-')
    return 0;
  body = arg + 1;
  if (body[0] == '-')
    body++;
  eq = strchr(body, '=');
  nameLen = eq ? (size_t)(eq - body) : strlen(body);
  if (strlen(name) != nameLen || strncasecmp(body, name, nameLen) != 0)
    return 0;
  if (value)
    *value = eq ? eq + 1 : NULL;
  return 1;
}

static const char *option_value(int *iArg, int argc, char **argv, const char *arg, const char *name) {
  const char *value = NULL;
  if (!option_name_matches(arg, name, &value))
    return NULL;
  if (value)
    return value;
  if (*iArg + 1 >= argc) {
    fprintf(stderr, "error: missing value for -%s\n", name);
    exit(EXIT_FAILURE);
  }
  return argv[++(*iArg)];
}

static long parse_long_option(const char *name, const char *value) {
  char *endptr;
  long result;
  errno = 0;
  result = strtol(value, &endptr, 10);
  if (errno || !value[0] || *endptr) {
    fprintf(stderr, "error: -%s must be an integer\n", name);
    exit(EXIT_FAILURE);
  }
  return result;
}

static const char *stat_suffix(STAT_CODE code) {
  switch (code) {
  case STAT_COPY:
    return "";
  case STAT_MAXIMUM:
    return "Max";
  case STAT_MINIMUM:
    return "Min";
  case STAT_MEAN:
    return "Mean";
  case STAT_LARGEST:
    return "Largest";
  case STAT_SIGNED_LARGEST:
    return "SignedLargest";
  case STAT_SUM:
    return "Sum";
  case STAT_RMS:
    return "Rms";
  case STAT_STANDARD_DEVIATION:
    return "StDev";
  case STAT_SIGMA:
    return "Sigma";
  case STAT_WMEAN:
    return "WMean";
  case STAT_WSTANDARD_DEVIATION:
    return "WStDev";
  case STAT_WRMS:
    return "WRms";
  case STAT_WSIGMA:
    return "WSigma";
  case STAT_CMAXIMUM:
    return "CMaximum";
  case STAT_CMINIMUM:
    return "CMinimum";
  case STAT_PMAXIMUM:
    return "PMaximum";
  case STAT_PMINIMUM:
    return "PMinimum";
  case STAT_SLOPE:
    return "Slope";
  case STAT_INTERCEPT:
    return "Intercept";
  case STAT_EXMM_MEAN:
    return "ExmmMean";
  }
  return "";
}

static int stat_uses_weight(STAT_CODE code) {
  return code == STAT_WMEAN || code == STAT_WSTANDARD_DEVIATION ||
         code == STAT_WRMS || code == STAT_WSIGMA;
}

static int stat_uses_function_column(STAT_CODE code) {
  return code == STAT_CMAXIMUM || code == STAT_CMINIMUM;
}

static int stat_uses_function_parameter(STAT_CODE code) {
  return code == STAT_PMAXIMUM || code == STAT_PMINIMUM ||
         code == STAT_SLOPE || code == STAT_INTERCEPT;
}

static void init_options(OPTIONS *opts) {
  memset(opts, 0, sizeof(*opts));
  string_list_append(&opts->pattern, "PSD-*.xz");
  string_list_append(&opts->pattern, "PSD-*.gz");
  opts->groupBy = xstrdup("PSName");
  opts->inputDir = current_directory();
  opts->output = xstrdup("PSD-envelope.sdds");
  opts->overwrite = 1;
  opts->threads = 1;
  opts->columnMajorOrder = 1;
}

static void free_raw_request(RAW_STAT_REQUEST *request) {
  string_list_clear(&request->column);
  free(request->weightColumn);
  free(request->functionOf);
}

static void free_stat_definition(STAT_DEFINITION *stat) {
  free(stat->sourceColumn);
  free(stat->resultColumn);
  free(stat->weightColumn);
  free(stat->functionOf);
}

static void free_options(OPTIONS *opts) {
  long i;
  string_list_clear(&opts->pattern);
  string_list_clear(&opts->groupValue);
  free(opts->groupBy);
  free(opts->inputDir);
  free(opts->output);
  for (i = 0; i < opts->requests; i++)
    free_raw_request(opts->request + i);
  free(opts->request);
  for (i = 0; i < opts->stats; i++)
    free_stat_definition(opts->stat + i);
  free(opts->stat);
}

static RAW_STAT_REQUEST *add_raw_request(OPTIONS *opts, STAT_CODE code) {
  RAW_STAT_REQUEST *request;
  if (opts->requests >= opts->requestsAllocated) {
    opts->requestsAllocated = opts->requestsAllocated ? 2 * opts->requestsAllocated : 16;
    opts->request = xrealloc(opts->request, sizeof(*opts->request) * opts->requestsAllocated);
  }
  request = opts->request + opts->requests++;
  memset(request, 0, sizeof(*request));
  request->code = code;
  request->sumPower = 1;
  return request;
}

static void add_simple_stat_request(OPTIONS *opts, STAT_CODE code, const char *value) {
  RAW_STAT_REQUEST *request = add_raw_request(opts, code);
  split_comma_list(value, &request->column);
}

static void add_sum_request(OPTIONS *opts, const char *value) {
  STRING_LIST item;
  RAW_STAT_REQUEST *request;
  long i;

  memset(&item, 0, sizeof(item));
  split_comma_list(value, &item);
  if (item.items < 2) {
    fprintf(stderr, "error: -sum requires <power>,<columns>\n");
    string_list_clear(&item);
    exit(EXIT_FAILURE);
  }
  request = add_raw_request(opts, STAT_SUM);
  request->sumPower = parse_long_option("sum", item.item[0]);
  if (request->sumPower < 1) {
    fprintf(stderr, "error: -sum power must be >= 1\n");
    string_list_clear(&item);
    exit(EXIT_FAILURE);
  }
  for (i = 1; i < item.items; i++)
    string_list_append(&request->column, item.item[i]);
  string_list_clear(&item);
}

static void add_weighted_request(OPTIONS *opts, STAT_CODE code, const char *value, const char *optionName) {
  STRING_LIST item;
  RAW_STAT_REQUEST *request;
  long i;

  memset(&item, 0, sizeof(item));
  split_comma_list(value, &item);
  if (item.items < 2) {
    fprintf(stderr, "error: -%s requires <weightColumn>,<columns>\n", optionName);
    string_list_clear(&item);
    exit(EXIT_FAILURE);
  }
  request = add_raw_request(opts, code);
  request->weightColumn = xstrdup(item.item[0]);
  for (i = 1; i < item.items; i++)
    string_list_append(&request->column, item.item[i]);
  string_list_clear(&item);
}

static void add_function_request(OPTIONS *opts, STAT_CODE code, const char *value, const char *optionName) {
  STRING_LIST item;
  RAW_STAT_REQUEST *request;
  long i;

  memset(&item, 0, sizeof(item));
  split_comma_list(value, &item);
  if (item.items < 2) {
    fprintf(stderr, "error: -%s requires <independent>,<columns>\n", optionName);
    string_list_clear(&item);
    exit(EXIT_FAILURE);
  }
  request = add_raw_request(opts, code);
  request->functionOf = xstrdup(item.item[0]);
  for (i = 1; i < item.items; i++)
    string_list_append(&request->column, item.item[i]);
  string_list_clear(&item);
}

static void parse_options(OPTIONS *opts, int argc, char **argv) {
  int iArg, patternSpecified = 0;

  for (iArg = 1; iArg < argc; iArg++) {
    const char *arg = argv[iArg];
    const char *value;

    if (option_name_matches(arg, "help", NULL) || option_name_matches(arg, "h", NULL)) {
      fputs(USAGE, stdout);
      exit(EXIT_SUCCESS);
    } else if ((value = option_value(&iArg, argc, argv, arg, "pattern"))) {
      if (!patternSpecified) {
        string_list_clear(&opts->pattern);
        patternSpecified = 1;
      }
      string_list_append(&opts->pattern, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "inputDir"))) {
      if (!strlen(value)) {
        fprintf(stderr, "error: -inputDir value may not be blank\n");
        exit(EXIT_FAILURE);
      }
      free(opts->inputDir);
      opts->inputDir = xstrdup(value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "groupBy"))) {
      if (!strlen(value)) {
        fprintf(stderr, "error: -groupBy value may not be blank\n");
        exit(EXIT_FAILURE);
      }
      free(opts->groupBy);
      opts->groupBy = xstrdup(value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "groupValue"))) {
      string_list_append(&opts->groupValue, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "output"))) {
      free(opts->output);
      opts->output = xstrdup(value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "majorOrder"))) {
      if (strcasecmp(value, "column") == 0)
        opts->columnMajorOrder = 1;
      else if (strcasecmp(value, "row") == 0)
        opts->columnMajorOrder = 0;
      else {
        fprintf(stderr, "error: -majorOrder must be row or column\n");
        exit(EXIT_FAILURE);
      }
    } else if ((value = option_value(&iArg, argc, argv, arg, "threads"))) {
      long threads = parse_long_option("threads", value);
      if (threads < 1) {
        fprintf(stderr, "error: -threads must be >= 1\n");
        exit(EXIT_FAILURE);
      }
      if (threads > INT_MAX) {
        fprintf(stderr, "error: -threads value is too large\n");
        exit(EXIT_FAILURE);
      }
      opts->threads = (int)threads;
    } else if ((value = option_value(&iArg, argc, argv, arg, "copy"))) {
      add_simple_stat_request(opts, STAT_COPY, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "maximum"))) {
      add_simple_stat_request(opts, STAT_MAXIMUM, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "minimum"))) {
      add_simple_stat_request(opts, STAT_MINIMUM, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "mean"))) {
      add_simple_stat_request(opts, STAT_MEAN, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "largest"))) {
      add_simple_stat_request(opts, STAT_LARGEST, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "signedLargest"))) {
      add_simple_stat_request(opts, STAT_SIGNED_LARGEST, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "sum"))) {
      add_sum_request(opts, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "rms"))) {
      add_simple_stat_request(opts, STAT_RMS, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "standarddeviations")) ||
               (value = option_value(&iArg, argc, argv, arg, "standarddeviation"))) {
      add_simple_stat_request(opts, STAT_STANDARD_DEVIATION, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "sigmas")) ||
               (value = option_value(&iArg, argc, argv, arg, "sigma"))) {
      add_simple_stat_request(opts, STAT_SIGMA, value);
    } else if ((value = option_value(&iArg, argc, argv, arg, "wmean"))) {
      add_weighted_request(opts, STAT_WMEAN, value, "wmean");
    } else if ((value = option_value(&iArg, argc, argv, arg, "wstandarddeviations")) ||
               (value = option_value(&iArg, argc, argv, arg, "wstandarddeviation"))) {
      add_weighted_request(opts, STAT_WSTANDARD_DEVIATION, value, "wstandarddeviations");
    } else if ((value = option_value(&iArg, argc, argv, arg, "wrms"))) {
      add_weighted_request(opts, STAT_WRMS, value, "wrms");
    } else if ((value = option_value(&iArg, argc, argv, arg, "wsigma")) ||
               (value = option_value(&iArg, argc, argv, arg, "wsigmas"))) {
      add_weighted_request(opts, STAT_WSIGMA, value, "wsigma");
    } else if ((value = option_value(&iArg, argc, argv, arg, "cmaximum"))) {
      add_function_request(opts, STAT_CMAXIMUM, value, "cmaximum");
    } else if ((value = option_value(&iArg, argc, argv, arg, "cminimum"))) {
      add_function_request(opts, STAT_CMINIMUM, value, "cminimum");
    } else if ((value = option_value(&iArg, argc, argv, arg, "pmaximum"))) {
      add_function_request(opts, STAT_PMAXIMUM, value, "pmaximum");
    } else if ((value = option_value(&iArg, argc, argv, arg, "pminimum"))) {
      add_function_request(opts, STAT_PMINIMUM, value, "pminimum");
    } else if ((value = option_value(&iArg, argc, argv, arg, "slope"))) {
      add_function_request(opts, STAT_SLOPE, value, "slope");
    } else if ((value = option_value(&iArg, argc, argv, arg, "intercept"))) {
      add_function_request(opts, STAT_INTERCEPT, value, "intercept");
    } else if ((value = option_value(&iArg, argc, argv, arg, "exmmMean"))) {
      add_simple_stat_request(opts, STAT_EXMM_MEAN, value);
    } else if (option_name_matches(arg, "noOverwrite", NULL)) {
      opts->overwrite = 0;
    } else if (option_name_matches(arg, "overwrite", NULL)) {
      opts->overwrite = 1;
    } else if (option_name_matches(arg, "verbose", NULL)) {
      opts->verbose = 1;
    } else if (option_name_matches(arg, "nowarnings", NULL)) {
      opts->noWarnings = 1;
    } else if (option_name_matches(arg, "median", NULL) ||
               option_name_matches(arg, "percentile", NULL) ||
               option_name_matches(arg, "decilerange", NULL)) {
      fprintf(stderr, "error: %s is not supported because it requires retaining page histories\n", arg);
      exit(EXIT_FAILURE);
    } else {
      fprintf(stderr, "error: unknown option: %s\n", arg);
      exit(EXIT_FAILURE);
    }
  }

  if (opts->requests == 0) {
    fprintf(stderr, "error: at least one statistic option is required\n");
    exit(EXIT_FAILURE);
  }
  if (!is_directory(opts->inputDir)) {
    fprintf(stderr, "error: -inputDir is not a directory: %s\n", opts->inputDir);
    exit(EXIT_FAILURE);
  }
}

static void collect_input_files(const char *workDir, const OPTIONS *opts, const char *finalOutput, STRING_LIST *files) {
  long i;
  for (i = 0; i < opts->pattern.items; i++) {
    char *globPattern;
    glob_t globResult;
    int status;
    size_t j;

    globPattern = is_absolute_path(opts->pattern.item[i]) ? xstrdup(opts->pattern.item[i]) : join_path(workDir, opts->pattern.item[i]);
    memset(&globResult, 0, sizeof(globResult));
    status = glob(globPattern, 0, NULL, &globResult);
    if (status == 0) {
      for (j = 0; j < globResult.gl_pathc; j++) {
        const char *path = globResult.gl_pathv[j];
        if (finalOutput && same_existing_file(path, finalOutput))
          continue;
        if (!string_list_contains(files, path))
          string_list_append(files, path);
      }
    } else if (status != GLOB_NOMATCH) {
      fprintf(stderr, "warning: glob failed for %s\n", globPattern);
    }
    globfree(&globResult);
    free(globPattern);
  }
  if (files->items > 1)
    qsort(files->item, files->items, sizeof(*files->item), compare_input_files);
}

static char *stat_column_name(const STAT_DEFINITION *stat) {
  char buffer[64];
  const char *suffix = stat_suffix(stat->code);
  const char *prefix = stat->sourceColumn;
  size_t length;
  char *name;

  if (stat->code == STAT_COPY)
    return xstrdup(stat->sourceColumn);
  if (stat->code == STAT_SUM && stat->sumPower != 1) {
    snprintf(buffer, sizeof(buffer), "%ld%s", stat->sumPower, suffix);
    suffix = buffer;
  }
  if (stat->code == STAT_CMAXIMUM || stat->code == STAT_CMINIMUM ||
      stat->code == STAT_PMAXIMUM || stat->code == STAT_PMINIMUM) {
    length = strlen(stat->functionOf) + strlen(suffix) + strlen(stat->sourceColumn) + 1;
    name = xmalloc(length);
    snprintf(name, length, "%s%s%s", stat->functionOf, suffix, stat->sourceColumn);
    return name;
  }
  length = strlen(prefix) + strlen(suffix) + 1;
  name = xmalloc(length);
  snprintf(name, length, "%s%s", prefix, suffix);
  return name;
}

static int result_name_exists(const OPTIONS *opts, const char *name) {
  long i;
  for (i = 0; i < opts->stats; i++) {
    if (strcmp(opts->stat[i].resultColumn, name) == 0)
      return 1;
  }
  return 0;
}

static int append_stat_definition(OPTIONS *opts, SDDS_DATASET *input, RAW_STAT_REQUEST *request, const char *sourceColumn) {
  STAT_DEFINITION *stat;
  char *resultName;
  int32_t sourceIndex, sourceType;

  if ((sourceIndex = SDDS_GetColumnIndex(input, (char *)sourceColumn)) < 0) {
    fprintf(stderr, "error: column %s not found\n", sourceColumn);
    return 0;
  }
  sourceType = SDDS_GetColumnType(input, sourceIndex);
  if (request->code != STAT_COPY && !SDDS_NUMERIC_TYPE(sourceType)) {
    fprintf(stderr, "error: column %s is not numeric\n", sourceColumn);
    return 0;
  }
  if (stat_uses_weight(request->code)) {
    int32_t weightIndex, weightType;
    if ((weightIndex = SDDS_GetColumnIndex(input, request->weightColumn)) < 0) {
      fprintf(stderr, "error: weight column %s not found\n", request->weightColumn);
      return 0;
    }
    weightType = SDDS_GetColumnType(input, weightIndex);
    if (!SDDS_NUMERIC_TYPE(weightType)) {
      fprintf(stderr, "error: weight column %s is not numeric\n", request->weightColumn);
      return 0;
    }
  }
  if (stat_uses_function_column(request->code)) {
    int32_t functionIndex, functionType;
    if ((functionIndex = SDDS_GetColumnIndex(input, request->functionOf)) < 0) {
      fprintf(stderr, "error: independent column %s not found\n", request->functionOf);
      return 0;
    }
    functionType = SDDS_GetColumnType(input, functionIndex);
    if (!SDDS_NUMERIC_TYPE(functionType)) {
      fprintf(stderr, "error: independent column %s is not numeric\n", request->functionOf);
      return 0;
    }
  }
  if (stat_uses_function_parameter(request->code)) {
    int32_t functionIndex, functionType;
    if ((functionIndex = SDDS_GetParameterIndex(input, request->functionOf)) < 0) {
      fprintf(stderr, "error: independent parameter %s not found\n", request->functionOf);
      return 0;
    }
    functionType = SDDS_GetParameterType(input, functionIndex);
    if (!SDDS_NUMERIC_TYPE(functionType)) {
      fprintf(stderr, "error: independent parameter %s is not numeric\n", request->functionOf);
      return 0;
    }
  }

  if (opts->stats >= opts->statsAllocated) {
    opts->statsAllocated = opts->statsAllocated ? 2 * opts->statsAllocated : 32;
    opts->stat = xrealloc(opts->stat, sizeof(*opts->stat) * opts->statsAllocated);
  }
  stat = opts->stat + opts->stats;
  memset(stat, 0, sizeof(*stat));
  stat->code = request->code;
  stat->sourceColumn = xstrdup(sourceColumn);
  stat->weightColumn = request->weightColumn ? xstrdup(request->weightColumn) : NULL;
  stat->functionOf = request->functionOf ? xstrdup(request->functionOf) : NULL;
  stat->sumPower = request->sumPower;
  stat->sourceType = sourceType;
  resultName = stat_column_name(stat);
  if (result_name_exists(opts, resultName)) {
    fprintf(stderr, "error: duplicate output column name %s\n", resultName);
    free(resultName);
    free_stat_definition(stat);
    memset(stat, 0, sizeof(*stat));
    return 0;
  }
  stat->resultColumn = resultName;
  opts->stats++;
  return 1;
}

static int compile_stat_definitions(OPTIONS *opts, SDDS_DATASET *input) {
  long iReq, iCol;
  int32_t groupIndex;

  if ((groupIndex = SDDS_GetParameterIndex(input, opts->groupBy)) < 0) {
    fprintf(stderr, "error: input file does not contain grouping parameter %s\n", opts->groupBy);
    return 0;
  }
  opts->groupByType = SDDS_GetParameterType(input, groupIndex);
  if (opts->groupByType <= 0) {
    fprintf(stderr, "error: grouping parameter %s has invalid type\n", opts->groupBy);
    return 0;
  }

  for (iReq = 0; iReq < opts->requests; iReq++) {
    RAW_STAT_REQUEST *request = opts->request + iReq;
    for (iCol = 0; iCol < request->column.items; iCol++) {
      char *columnPattern = request->column.item[iCol];
      if (has_wildcards(columnPattern)) {
        char **columnName = NULL;
        int32_t columnNames = 0;
        int32_t iName;
        SDDS_SetColumnFlags(input, 0);
        if (!SDDS_SetColumnsOfInterest(input, SDDS_MATCH_STRING, columnPattern, SDDS_OR) ||
            !(columnName = SDDS_GetColumnNames(input, &columnNames))) {
          fprintf(stderr, "error: no columns selected for wildcard %s\n", columnPattern);
          return 0;
        }
        for (iName = 0; iName < columnNames; iName++) {
          if (!append_stat_definition(opts, input, request, columnName[iName])) {
            free(columnName);
            return 0;
          }
        }
        free(columnName);
      } else if (!append_stat_definition(opts, input, request, columnPattern)) {
        return 0;
      }
    }
  }
  if (opts->stats == 0) {
    fprintf(stderr, "error: no statistic columns were selected\n");
    return 0;
  }
  return 1;
}

static void free_copy_data(void *data, int32_t type, int64_t rows) {
  int64_t i;
  if (!data)
    return;
  if (type == SDDS_STRING) {
    char **stringData = (char **)data;
    for (i = 0; i < rows; i++)
      free(stringData[i]);
  }
  free(data);
}

static void free_stat_accumulator(STAT_ACCUMULATOR *accumulator, const STAT_DEFINITION *stat, int64_t rows) {
  if (stat->code == STAT_COPY)
    free_copy_data(accumulator->copyData, stat->sourceType, rows);
  free(accumulator->value1);
  free(accumulator->value2);
  free(accumulator->value3);
  free(accumulator->value4);
  free(accumulator->sumWeight);
}

static void free_group_value(void *value, int32_t type) {
  if (!value)
    return;
  if (type == SDDS_STRING)
    free(*(char **)value);
  free(value);
}

static void *duplicate_group_value(const void *value, int32_t type) {
  void *copy;
  int32_t size;

  if (!value)
    return NULL;
  if (type == SDDS_STRING) {
    char **stringCopy = xmalloc(sizeof(*stringCopy));
    char *const *stringValue = (char *const *)value;
    *stringCopy = xstrdup(*stringValue ? *stringValue : "");
    return stringCopy;
  }
  size = SDDS_GetTypeSize(type);
  if (size <= 0) {
    fprintf(stderr, "error: invalid grouping parameter type %d\n", type);
    exit(EXIT_FAILURE);
  }
  copy = xmalloc((size_t)size);
  memcpy(copy, value, (size_t)size);
  return copy;
}

static void free_group_list(GROUP_LIST *groups, const OPTIONS *opts) {
  long i, j, iStat;
  for (i = 0; i < groups->groups; i++) {
    ENVELOPE_GROUP *group = groups->group + i;
    free(group->name);
    free_group_value(group->value, opts->groupByType);
    for (j = 0; j < group->rowDatas; j++) {
      ROW_ACCUMULATOR *rowData = group->rowData + j;
      for (iStat = 0; iStat < opts->stats; iStat++)
        free_stat_accumulator(rowData->stat + iStat, opts->stat + iStat, rowData->rows);
      free(rowData->stat);
    }
    free(group->rowData);
  }
  free(groups->group);
  groups->group = NULL;
  groups->groups = groups->groupsAllocated = 0;
}

static void copy_double_array(double *target, const double *source, int64_t rows) {
  memcpy(target, source, sizeof(*target) * (size_t)rows);
}

static ENVELOPE_GROUP *find_group(GROUP_LIST *groups, const char *groupName) {
  long i;
  for (i = 0; i < groups->groups; i++) {
    if (strcmp(groups->group[i].name, groupName) == 0)
      return groups->group + i;
  }
  return NULL;
}

static ENVELOPE_GROUP *add_group(GROUP_LIST *groups, const char *groupName, void *groupValue) {
  ENVELOPE_GROUP *group;
  if (groups->groups >= groups->groupsAllocated) {
    groups->groupsAllocated = groups->groupsAllocated ? 2 * groups->groupsAllocated : 64;
    groups->group = xrealloc(groups->group, sizeof(*groups->group) * groups->groupsAllocated);
  }
  group = groups->group + groups->groups++;
  memset(group, 0, sizeof(*group));
  group->name = xstrdup(groupName);
  group->value = groupValue;
  return group;
}

static void allocate_stat_accumulator(STAT_ACCUMULATOR *accumulator, const STAT_DEFINITION *stat, int64_t rows) {
  switch (stat->code) {
  case STAT_COPY:
    break;
  case STAT_STANDARD_DEVIATION:
  case STAT_SIGMA:
  case STAT_CMAXIMUM:
  case STAT_CMINIMUM:
  case STAT_PMAXIMUM:
  case STAT_PMINIMUM:
  case STAT_WSTANDARD_DEVIATION:
  case STAT_WSIGMA:
    accumulator->value1 = xcalloc(rows, sizeof(*accumulator->value1));
    accumulator->value2 = xcalloc(rows, sizeof(*accumulator->value2));
    if (stat->code == STAT_WSTANDARD_DEVIATION || stat->code == STAT_WSIGMA)
      accumulator->sumWeight = xcalloc(rows, sizeof(*accumulator->sumWeight));
    break;
  case STAT_SLOPE:
  case STAT_INTERCEPT:
  case STAT_EXMM_MEAN:
    accumulator->value1 = xcalloc(rows, sizeof(*accumulator->value1));
    accumulator->value2 = xcalloc(rows, sizeof(*accumulator->value2));
    accumulator->value3 = xcalloc(rows, sizeof(*accumulator->value3));
    accumulator->value4 = xcalloc(rows, sizeof(*accumulator->value4));
    if (stat->code == STAT_EXMM_MEAN)
      accumulator->sumWeight = xcalloc(rows, sizeof(*accumulator->sumWeight));
    break;
  case STAT_WMEAN:
  case STAT_WRMS:
    accumulator->value1 = xcalloc(rows, sizeof(*accumulator->value1));
    accumulator->sumWeight = xcalloc(rows, sizeof(*accumulator->sumWeight));
    break;
  default:
    accumulator->value1 = xcalloc(rows, sizeof(*accumulator->value1));
    break;
  }
}

static ROW_ACCUMULATOR *find_row_accumulator(ENVELOPE_GROUP *group, int64_t rows) {
  long i;
  for (i = 0; i < group->rowDatas; i++) {
    if (group->rowData[i].rows == rows)
      return group->rowData + i;
  }
  return NULL;
}

static ROW_ACCUMULATOR *add_row_accumulator(ENVELOPE_GROUP *group, const OPTIONS *opts, int64_t rows) {
  ROW_ACCUMULATOR *rowData;
  long iStat;

  if (group->rowDatas >= group->rowDatasAllocated) {
    group->rowDatasAllocated = group->rowDatasAllocated ? 2 * group->rowDatasAllocated : 2;
    group->rowData = xrealloc(group->rowData, sizeof(*group->rowData) * group->rowDatasAllocated);
  }
  rowData = group->rowData + group->rowDatas++;
  memset(rowData, 0, sizeof(*rowData));
  rowData->rows = rows;
  rowData->stat = xcalloc(opts->stats, sizeof(*rowData->stat));
  for (iStat = 0; iStat < opts->stats; iStat++)
    allocate_stat_accumulator(rowData->stat + iStat, opts->stat + iStat, rows);
  return rowData;
}

static ROW_ACCUMULATOR *get_row_accumulator(ENVELOPE_GROUP *group, const OPTIONS *opts, int64_t rows) {
  ROW_ACCUMULATOR *rowData = find_row_accumulator(group, rows);
  if (rowData)
    return rowData;
  return add_row_accumulator(group, opts, rows);
}

static ROW_ACCUMULATOR *best_row_accumulator(ENVELOPE_GROUP *group) {
  ROW_ACCUMULATOR *best = NULL;
  long i;
  for (i = 0; i < group->rowDatas; i++) {
    if (!best || group->rowData[i].pages > best->pages)
      best = group->rowData + i;
  }
  return best;
}

static int read_parameter_double(SDDS_DATASET *input, const char *name, double *value) {
  if (!SDDS_GetParameterAsDouble(input, (char *)name, value)) {
    fprintf(stderr, "error: unable to read parameter %s as double\n", name);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  return 1;
}

static double integer_power(double value, long power) {
  double result = 1;
  long i;
  for (i = 0; i < power; i++)
    result *= value;
  return result;
}

static void accumulate_exmm(STAT_ACCUMULATOR *accumulator, double *data, int64_t rows) {
  int64_t i;
  if (!accumulator->initialized) {
    for (i = 0; i < rows; i++) {
      accumulator->value1[i] = data[i];
      accumulator->value2[i] = data[i];
      accumulator->value3[i] = data[i];
      accumulator->value4[i] = 1;
      accumulator->sumWeight[i] = 1;
    }
    accumulator->initialized = 1;
    return;
  }
  for (i = 0; i < rows; i++) {
    accumulator->value1[i] += data[i];
    if (data[i] < accumulator->value2[i]) {
      accumulator->value2[i] = data[i];
      accumulator->value4[i] = 1;
    } else if (data[i] == accumulator->value2[i]) {
      accumulator->value4[i] += 1;
    }
    if (data[i] > accumulator->value3[i]) {
      accumulator->value3[i] = data[i];
      accumulator->sumWeight[i] = 1;
    } else if (data[i] == accumulator->value3[i]) {
      accumulator->sumWeight[i] += 1;
    }
  }
}

static int accumulate_stat(SDDS_DATASET *input, ROW_ACCUMULATOR *rowData, const OPTIONS *opts, long iStat) {
  STAT_DEFINITION *stat = opts->stat + iStat;
  STAT_ACCUMULATOR *accumulator = rowData->stat + iStat;
  double *data = NULL, *weight = NULL, *otherData = NULL;
  double parameterValue = 0;
  int64_t i, rows = rowData->rows;

  if (stat->code == STAT_COPY) {
    if (!accumulator->initialized) {
      if (!(accumulator->copyData = SDDS_GetColumn(input, stat->sourceColumn))) {
        fprintf(stderr, "error: unable to read copy column %s\n", stat->sourceColumn);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 0;
      }
      accumulator->initialized = 1;
    }
    return 1;
  }

  if (!(data = SDDS_GetColumnInDoubles(input, stat->sourceColumn))) {
    fprintf(stderr, "error: unable to read column %s\n", stat->sourceColumn);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (stat_uses_weight(stat->code)) {
    if (!(weight = SDDS_GetColumnInDoubles(input, stat->weightColumn))) {
      fprintf(stderr, "error: unable to read weight column %s\n", stat->weightColumn);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      free(data);
      return 0;
    }
  }
  if (stat_uses_function_column(stat->code)) {
    if (!(otherData = SDDS_GetColumnInDoubles(input, stat->functionOf))) {
      fprintf(stderr, "error: unable to read independent column %s\n", stat->functionOf);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      free(data);
      free(weight);
      return 0;
    }
  }
  if (stat_uses_function_parameter(stat->code) && !read_parameter_double(input, stat->functionOf, &parameterValue)) {
    free(data);
    free(weight);
    free(otherData);
    return 0;
  }

  switch (stat->code) {
  case STAT_MAXIMUM:
    if (!accumulator->initialized) {
      for (i = 0; i < rows; i++)
        accumulator->value1[i] = data[i];
      accumulator->initialized = 1;
    } else {
      for (i = 0; i < rows; i++)
        if (accumulator->value1[i] < data[i])
          accumulator->value1[i] = data[i];
    }
    break;
  case STAT_MINIMUM:
    if (!accumulator->initialized) {
      for (i = 0; i < rows; i++)
        accumulator->value1[i] = data[i];
      accumulator->initialized = 1;
    } else {
      for (i = 0; i < rows; i++)
        if (accumulator->value1[i] > data[i])
          accumulator->value1[i] = data[i];
    }
    break;
  case STAT_MEAN:
  case STAT_SUM:
    for (i = 0; i < rows; i++)
      accumulator->value1[i] += stat->code == STAT_SUM ? integer_power(data[i], stat->sumPower) : data[i];
    accumulator->initialized = 1;
    break;
  case STAT_LARGEST:
    if (!accumulator->initialized) {
      for (i = 0; i < rows; i++)
        accumulator->value1[i] = fabs(data[i]);
      accumulator->initialized = 1;
    } else {
      for (i = 0; i < rows; i++)
        if (accumulator->value1[i] < fabs(data[i]))
          accumulator->value1[i] = fabs(data[i]);
    }
    break;
  case STAT_SIGNED_LARGEST:
    if (!accumulator->initialized) {
      for (i = 0; i < rows; i++)
        accumulator->value1[i] = data[i];
      accumulator->initialized = 1;
    } else {
      for (i = 0; i < rows; i++)
        if (fabs(accumulator->value1[i]) < fabs(data[i]))
          accumulator->value1[i] = data[i];
    }
    break;
  case STAT_RMS:
    for (i = 0; i < rows; i++)
      accumulator->value1[i] += data[i] * data[i];
    accumulator->initialized = 1;
    break;
  case STAT_STANDARD_DEVIATION:
  case STAT_SIGMA:
    for (i = 0; i < rows; i++) {
      accumulator->value1[i] += data[i];
      accumulator->value2[i] += data[i] * data[i];
    }
    accumulator->initialized = 1;
    break;
  case STAT_WMEAN:
    for (i = 0; i < rows; i++) {
      accumulator->sumWeight[i] += weight[i];
      accumulator->value1[i] += data[i] * weight[i];
    }
    accumulator->initialized = 1;
    break;
  case STAT_WSTANDARD_DEVIATION:
  case STAT_WSIGMA:
    for (i = 0; i < rows; i++) {
      accumulator->sumWeight[i] += weight[i];
      accumulator->value1[i] += data[i] * weight[i];
      accumulator->value2[i] += data[i] * data[i] * weight[i];
    }
    accumulator->initialized = 1;
    break;
  case STAT_WRMS:
    for (i = 0; i < rows; i++) {
      accumulator->sumWeight[i] += weight[i];
      accumulator->value1[i] += data[i] * data[i] * weight[i];
    }
    accumulator->initialized = 1;
    break;
  case STAT_CMAXIMUM:
  case STAT_CMINIMUM:
    if (!accumulator->initialized) {
      for (i = 0; i < rows; i++) {
        accumulator->value1[i] = otherData[i];
        accumulator->value2[i] = data[i];
      }
      accumulator->initialized = 1;
    } else {
      for (i = 0; i < rows; i++) {
        if ((stat->code == STAT_CMAXIMUM && accumulator->value2[i] < data[i]) ||
            (stat->code == STAT_CMINIMUM && accumulator->value2[i] > data[i])) {
          accumulator->value1[i] = otherData[i];
          accumulator->value2[i] = data[i];
        }
      }
    }
    break;
  case STAT_PMAXIMUM:
  case STAT_PMINIMUM:
    if (!accumulator->initialized) {
      for (i = 0; i < rows; i++) {
        accumulator->value1[i] = parameterValue;
        accumulator->value2[i] = data[i];
      }
      accumulator->initialized = 1;
    } else {
      for (i = 0; i < rows; i++) {
        if ((stat->code == STAT_PMAXIMUM && accumulator->value2[i] < data[i]) ||
            (stat->code == STAT_PMINIMUM && accumulator->value2[i] > data[i])) {
          accumulator->value1[i] = parameterValue;
          accumulator->value2[i] = data[i];
        }
      }
    }
    break;
  case STAT_SLOPE:
  case STAT_INTERCEPT:
    for (i = 0; i < rows; i++) {
      accumulator->value1[i] += parameterValue;
      accumulator->value2[i] += parameterValue * parameterValue;
      accumulator->value3[i] += data[i];
      accumulator->value4[i] += parameterValue * data[i];
    }
    accumulator->initialized = 1;
    break;
  case STAT_EXMM_MEAN:
    accumulate_exmm(accumulator, data, rows);
    break;
  case STAT_COPY:
    break;
  }

  free(data);
  free(weight);
  free(otherData);
  return 1;
}

static void merge_stat_accumulator(STAT_ACCUMULATOR *target, STAT_ACCUMULATOR *source,
                                   const STAT_DEFINITION *stat, int64_t rows) {
  int64_t i;

  if (!source->initialized)
    return;

  if (stat->code == STAT_COPY) {
    if (!target->initialized) {
      target->copyData = source->copyData;
      source->copyData = NULL;
      target->initialized = 1;
    }
    return;
  }

  if (!target->initialized) {
    if (source->value1)
      copy_double_array(target->value1, source->value1, rows);
    if (source->value2)
      copy_double_array(target->value2, source->value2, rows);
    if (source->value3)
      copy_double_array(target->value3, source->value3, rows);
    if (source->value4)
      copy_double_array(target->value4, source->value4, rows);
    if (source->sumWeight)
      copy_double_array(target->sumWeight, source->sumWeight, rows);
    target->initialized = 1;
    return;
  }

  switch (stat->code) {
  case STAT_MAXIMUM:
  case STAT_LARGEST:
    for (i = 0; i < rows; i++)
      if (target->value1[i] < source->value1[i])
        target->value1[i] = source->value1[i];
    break;
  case STAT_MINIMUM:
    for (i = 0; i < rows; i++)
      if (target->value1[i] > source->value1[i])
        target->value1[i] = source->value1[i];
    break;
  case STAT_SIGNED_LARGEST:
    for (i = 0; i < rows; i++)
      if (fabs(target->value1[i]) < fabs(source->value1[i]))
        target->value1[i] = source->value1[i];
    break;
  case STAT_CMAXIMUM:
  case STAT_PMAXIMUM:
    for (i = 0; i < rows; i++) {
      if (target->value2[i] < source->value2[i]) {
        target->value1[i] = source->value1[i];
        target->value2[i] = source->value2[i];
      }
    }
    break;
  case STAT_CMINIMUM:
  case STAT_PMINIMUM:
    for (i = 0; i < rows; i++) {
      if (target->value2[i] > source->value2[i]) {
        target->value1[i] = source->value1[i];
        target->value2[i] = source->value2[i];
      }
    }
    break;
  case STAT_MEAN:
  case STAT_SUM:
  case STAT_RMS:
    for (i = 0; i < rows; i++)
      target->value1[i] += source->value1[i];
    break;
  case STAT_STANDARD_DEVIATION:
  case STAT_SIGMA:
    for (i = 0; i < rows; i++) {
      target->value1[i] += source->value1[i];
      target->value2[i] += source->value2[i];
    }
    break;
  case STAT_WMEAN:
  case STAT_WRMS:
    for (i = 0; i < rows; i++) {
      target->value1[i] += source->value1[i];
      target->sumWeight[i] += source->sumWeight[i];
    }
    break;
  case STAT_WSTANDARD_DEVIATION:
  case STAT_WSIGMA:
    for (i = 0; i < rows; i++) {
      target->value1[i] += source->value1[i];
      target->value2[i] += source->value2[i];
      target->sumWeight[i] += source->sumWeight[i];
    }
    break;
  case STAT_SLOPE:
  case STAT_INTERCEPT:
    for (i = 0; i < rows; i++) {
      target->value1[i] += source->value1[i];
      target->value2[i] += source->value2[i];
      target->value3[i] += source->value3[i];
      target->value4[i] += source->value4[i];
    }
    break;
  case STAT_EXMM_MEAN:
    for (i = 0; i < rows; i++) {
      target->value1[i] += source->value1[i];
      if (source->value2[i] < target->value2[i]) {
        target->value2[i] = source->value2[i];
        target->value4[i] = source->value4[i];
      } else if (source->value2[i] == target->value2[i]) {
        target->value4[i] += source->value4[i];
      }
      if (source->value3[i] > target->value3[i]) {
        target->value3[i] = source->value3[i];
        target->sumWeight[i] = source->sumWeight[i];
      } else if (source->value3[i] == target->value3[i]) {
        target->sumWeight[i] += source->sumWeight[i];
      }
    }
    break;
  case STAT_COPY:
    break;
  }
}

static void merge_row_accumulator(ROW_ACCUMULATOR *target, ROW_ACCUMULATOR *source, const OPTIONS *opts) {
  long iStat;

  for (iStat = 0; iStat < opts->stats; iStat++)
    merge_stat_accumulator(target->stat + iStat, source->stat + iStat, opts->stat + iStat, source->rows);
  target->pages += source->pages;
}

static void merge_group_list(GROUP_LIST *target, GROUP_LIST *source, const OPTIONS *opts) {
  long iGroup, iRowData;

  for (iGroup = 0; iGroup < source->groups; iGroup++) {
    ENVELOPE_GROUP *sourceGroup = source->group + iGroup;
    ENVELOPE_GROUP *targetGroup = find_group(target, sourceGroup->name);
    if (!targetGroup) {
      targetGroup = add_group(target, sourceGroup->name, duplicate_group_value(sourceGroup->value, opts->groupByType));
    }
    targetGroup->zeroRowPages += sourceGroup->zeroRowPages;
    for (iRowData = 0; iRowData < sourceGroup->rowDatas; iRowData++) {
      ROW_ACCUMULATOR *sourceRowData = sourceGroup->rowData + iRowData;
      ROW_ACCUMULATOR *targetRowData = get_row_accumulator(targetGroup, opts, sourceRowData->rows);
      merge_row_accumulator(targetRowData, sourceRowData, opts);
    }
  }
}

static int define_stat_column(SDDS_DATASET *output, SDDS_DATASET *input, const STAT_DEFINITION *stat) {
  char *symbol = NULL;
  char *newSymbol;
  size_t needed;
  const char *definitionColumn = stat->sourceColumn;

  if (stat->code == STAT_CMAXIMUM || stat->code == STAT_CMINIMUM)
    definitionColumn = stat->functionOf;

  if (stat->code == STAT_COPY) {
    if (!SDDS_TransferColumnDefinition(output, input, stat->sourceColumn, stat->resultColumn))
      return 0;
    return 1;
  }
  if (!SDDS_TransferColumnDefinition(output, input, (char *)definitionColumn, stat->resultColumn))
    return 0;
  if (SDDS_ChangeColumnInformation(output, "description", NULL, SDDS_SET_BY_NAME, stat->resultColumn) != SDDS_STRING) {
    fprintf(stderr, "error: unable to clear description for output column %s\n", stat->resultColumn);
    return 0;
  }
  if (SDDS_ChangeColumnInformation(output, "type", "double", SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, stat->resultColumn) != SDDS_LONG) {
    fprintf(stderr, "error: unable to set output column %s type to double\n", stat->resultColumn);
    return 0;
  }
  if (SDDS_GetColumnInformation(output, "symbol", &symbol, SDDS_BY_NAME, stat->resultColumn) != SDDS_STRING) {
    fprintf(stderr, "error: unable to read symbol for output column %s\n", stat->resultColumn);
    return 0;
  }
  if (!symbol || !strlen(symbol)) {
    free(symbol);
    symbol = xstrdup(stat->sourceColumn);
  }

  needed = strlen(stat_suffix(stat->code)) + strlen(symbol) + 3;
  newSymbol = xmalloc(needed);
  snprintf(newSymbol, needed, "%s[%s]", stat_suffix(stat->code), symbol);
  free(symbol);

  if (SDDS_ChangeColumnInformation(output, "symbol", newSymbol, SDDS_BY_NAME, stat->resultColumn) != SDDS_STRING) {
    fprintf(stderr, "error: unable to set symbol for output column %s\n", stat->resultColumn);
    free(newSymbol);
    return 0;
  }
  free(newSymbol);
  return 1;
}

static int setup_output_file(SDDS_DATASET *output, const char *outputFile, SDDS_DATASET *input, const OPTIONS *opts) {
  long iStat;

  if (!SDDS_InitializeOutput(output, SDDS_BINARY, 0, NULL, "sddsgroupedenvelope output", (char *)outputFile)) {
    fprintf(stderr, "error: unable to initialize output file %s\n", outputFile);
    return 0;
  }
  output->layout.data_mode.column_major = opts->columnMajorOrder;

  if (!SDDS_TransferParameterDefinition(output, input, opts->groupBy, opts->groupBy)) {
    fprintf(stderr, "error: unable to define grouping parameter %s in output\n", opts->groupBy);
    return 0;
  }
  for (iStat = 0; iStat < opts->stats; iStat++) {
    if (!define_stat_column(output, input, opts->stat + iStat)) {
      fprintf(stderr, "error: unable to define output column %s\n", opts->stat[iStat].resultColumn);
      return 0;
    }
  }
  if (!SDDS_WriteLayout(output)) {
    fprintf(stderr, "error: unable to write output layout for %s\n", outputFile);
    return 0;
  }
  return 1;
}

static int read_input_file(const char *filename, const OPTIONS *opts, GROUP_LIST *groups) {
  SDDS_DATASET input;
  int pageCode;
  STRING_LIST seen;

  memset(&seen, 0, sizeof(seen));
  if (opts->verbose)
    fprintf(stderr, "Reading %s\n", filename);
  if (!SDDS_InitializeInput(&input, (char *)filename)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    string_list_clear(&seen);
    return 0;
  }

  while ((pageCode = SDDS_ReadPage(&input)) > 0) {
    char *groupName = NULL;
    ENVELOPE_GROUP *group;
    ROW_ACCUMULATOR *rowData;
    int64_t rows;
    int wantPage;
    long iStat;

    groupName = SDDS_GetParameterAsString(&input, opts->groupBy, NULL);
    if (!groupName || !strlen(groupName)) {
      fprintf(stderr, "error: blank or missing %s in %s page %d\n", opts->groupBy, filename, pageCode);
      free(groupName);
      SDDS_Terminate(&input);
      string_list_clear(&seen);
      return 0;
    }
    if (string_list_contains(&seen, groupName)) {
      fprintf(stderr, "error: duplicate %s %s in %s\n", opts->groupBy, groupName, filename);
      free(groupName);
      SDDS_Terminate(&input);
      string_list_clear(&seen);
      return 0;
    }
    string_list_append(&seen, groupName);

    wantPage = opts->groupValue.items == 0 || string_list_contains(&opts->groupValue, groupName);
    if (!wantPage) {
      free(groupName);
      continue;
    }

    if (!(group = find_group(groups, groupName))) {
      void *groupValue = SDDS_GetParameter(&input, opts->groupBy, NULL);
      if (!groupValue) {
        fprintf(stderr, "error: unable to read grouping parameter %s\n", opts->groupBy);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        free(groupName);
        SDDS_Terminate(&input);
        string_list_clear(&seen);
        return 0;
      }
      group = add_group(groups, groupName, groupValue);
    }
    rows = SDDS_CountRowsOfInterest(&input);
    if (rows <= 0) {
      group->zeroRowPages++;
      free(groupName);
      continue;
    }

    rowData = get_row_accumulator(group, opts, rows);
    for (iStat = 0; iStat < opts->stats; iStat++) {
      if (!accumulate_stat(&input, rowData, opts, iStat)) {
        free(groupName);
        SDDS_Terminate(&input);
        string_list_clear(&seen);
        return 0;
      }
    }
    rowData->pages++;
    free(groupName);
  }

  if (pageCode == 0) {
    fprintf(stderr, "error: failed while reading pages from %s\n", filename);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&input);
    string_list_clear(&seen);
    return 0;
  }
  if (!SDDS_Terminate(&input)) {
    fprintf(stderr, "error: failed to terminate input file %s\n", filename);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    string_list_clear(&seen);
    return 0;
  }
  string_list_clear(&seen);
  return 1;
}

static int read_input_files_serial(const STRING_LIST *files, const OPTIONS *opts, GROUP_LIST *groups) {
  long iFile;

  for (iFile = 0; iFile < files->items; iFile++) {
    if (!read_input_file(files->item[iFile], opts, groups))
      return 0;
  }
  return 1;
}

static int read_input_files_threaded(const STRING_LIST *files, const OPTIONS *opts, GROUP_LIST *groups) {
#if SDDSGROUPEDENVELOPE_USE_OPENMP
  FILE_READ_RESULT *result;
  long batchStart, iBatch, batchSize;
  int ok = 1;
  int threads = opts->threads;

  if (threads > files->items)
    threads = (int)files->items;
  if (threads <= 1)
    return read_input_files_serial(files, opts, groups);

  result = xcalloc((size_t)threads, sizeof(*result));
  omp_set_num_threads(threads);

  for (batchStart = 0; ok && batchStart < files->items; batchStart += threads) {
    batchSize = files->items - batchStart;
    if (batchSize > threads)
      batchSize = threads;

#pragma omp parallel for schedule(dynamic)
    for (iBatch = 0; iBatch < batchSize; iBatch++)
      result[iBatch].ok = read_input_file(files->item[batchStart + iBatch], opts, &result[iBatch].groups);

    for (iBatch = 0; iBatch < batchSize; iBatch++) {
      if (!result[iBatch].ok)
        ok = 0;
    }
    if (ok) {
      for (iBatch = 0; iBatch < batchSize; iBatch++)
        merge_group_list(groups, &result[iBatch].groups, opts);
    }
    for (iBatch = 0; iBatch < batchSize; iBatch++) {
      free_group_list(&result[iBatch].groups, opts);
      memset(&result[iBatch], 0, sizeof(*result));
    }
  }
  free(result);
  return ok;
#else
  return read_input_files_serial(files, opts, groups);
#endif
}

static int read_input_files(const STRING_LIST *files, const OPTIONS *opts, GROUP_LIST *groups) {
  if (opts->threads <= 1)
    return read_input_files_serial(files, opts, groups);
  return read_input_files_threaded(files, opts, groups);
}

static void warn_zero_weight(const OPTIONS *opts, const STAT_DEFINITION *stat, int64_t row) {
  if (!opts->noWarnings)
    fprintf(stderr, "warning: the total weight for row %" PRId64 " of %s is zero\n", row + 1, stat->sourceColumn);
}

static void finalize_stat_values(ROW_ACCUMULATOR *rowData, const OPTIONS *opts, long iStat) {
  STAT_DEFINITION *stat = opts->stat + iStat;
  STAT_ACCUMULATOR *accumulator = rowData->stat + iStat;
  int64_t i, pages = rowData->pages;

  switch (stat->code) {
  case STAT_COPY:
  case STAT_MAXIMUM:
  case STAT_MINIMUM:
  case STAT_LARGEST:
  case STAT_SIGNED_LARGEST:
  case STAT_SUM:
  case STAT_CMAXIMUM:
  case STAT_CMINIMUM:
  case STAT_PMAXIMUM:
  case STAT_PMINIMUM:
    break;
  case STAT_MEAN:
    for (i = 0; i < rowData->rows; i++)
      accumulator->value1[i] /= pages;
    break;
  case STAT_RMS:
    for (i = 0; i < rowData->rows; i++)
      accumulator->value1[i] = sqrt(accumulator->value1[i] / pages);
    break;
  case STAT_STANDARD_DEVIATION:
    if (pages < 2) {
      for (i = 0; i < rowData->rows; i++)
        accumulator->value1[i] = DBL_MAX;
    } else {
      for (i = 0; i < rowData->rows; i++) {
        double variance = accumulator->value2[i] / pages - sqr(accumulator->value1[i] / pages);
        accumulator->value1[i] = variance <= 0 ? 0 : sqrt(variance * pages / (pages - 1.0));
      }
    }
    break;
  case STAT_SIGMA:
    if (pages < 2) {
      for (i = 0; i < rowData->rows; i++)
        accumulator->value1[i] = DBL_MAX;
    } else {
      for (i = 0; i < rowData->rows; i++) {
        double variance = accumulator->value2[i] / pages - sqr(accumulator->value1[i] / pages);
        accumulator->value1[i] = variance <= 0 ? 0 : sqrt(variance / (pages - 1.0));
      }
    }
    break;
  case STAT_WMEAN:
    for (i = 0; i < rowData->rows; i++) {
      if (accumulator->sumWeight[i])
        accumulator->value1[i] /= accumulator->sumWeight[i];
      else {
        warn_zero_weight(opts, stat, i);
        accumulator->value1[i] = DBL_MAX;
      }
    }
    break;
  case STAT_WSTANDARD_DEVIATION:
    if (pages < 2) {
      for (i = 0; i < rowData->rows; i++)
        accumulator->value1[i] = DBL_MAX;
    } else {
      for (i = 0; i < rowData->rows; i++) {
        if (accumulator->sumWeight[i]) {
          double mean = accumulator->value1[i] / accumulator->sumWeight[i];
          double variance = accumulator->value2[i] / accumulator->sumWeight[i] - mean * mean;
          accumulator->value1[i] = variance <= 0 ? 0 : sqrt(variance * pages / (pages - 1.0));
        } else {
          warn_zero_weight(opts, stat, i);
          accumulator->value1[i] = DBL_MAX;
        }
      }
    }
    break;
  case STAT_WRMS:
    for (i = 0; i < rowData->rows; i++) {
      if (accumulator->sumWeight[i])
        accumulator->value1[i] = sqrt(accumulator->value1[i] / accumulator->sumWeight[i]);
      else {
        warn_zero_weight(opts, stat, i);
        accumulator->value1[i] = DBL_MAX;
      }
    }
    break;
  case STAT_WSIGMA:
    if (pages < 2) {
      for (i = 0; i < rowData->rows; i++)
        accumulator->value1[i] = DBL_MAX;
    } else {
      for (i = 0; i < rowData->rows; i++) {
        if (accumulator->sumWeight[i]) {
          double mean = accumulator->value1[i] / accumulator->sumWeight[i];
          double variance = accumulator->value2[i] / accumulator->sumWeight[i] - mean * mean;
          accumulator->value1[i] = variance <= 0 ? 0 : sqrt(variance / (pages - 1.0));
        } else {
          warn_zero_weight(opts, stat, i);
          accumulator->value1[i] = DBL_MAX;
        }
      }
    }
    break;
  case STAT_SLOPE:
    for (i = 0; i < rowData->rows; i++) {
      double D = pages * accumulator->value2[i] - accumulator->value1[i] * accumulator->value1[i];
      accumulator->value1[i] = D ? (pages * accumulator->value4[i] - accumulator->value1[i] * accumulator->value3[i]) / D : DBL_MAX;
    }
    break;
  case STAT_INTERCEPT:
    for (i = 0; i < rowData->rows; i++) {
      double D = pages * accumulator->value2[i] - accumulator->value1[i] * accumulator->value1[i];
      accumulator->value1[i] = D ? (accumulator->value2[i] * accumulator->value3[i] - accumulator->value1[i] * accumulator->value4[i]) / D : DBL_MAX;
    }
    break;
  case STAT_EXMM_MEAN:
    for (i = 0; i < rowData->rows; i++) {
      double min = accumulator->value2[i];
      double max = accumulator->value3[i];
      double excluded = min == max ? (double)pages : accumulator->value4[i] + accumulator->sumWeight[i];
      double kept = (double)pages - excluded;
      if (kept <= 0)
        accumulator->value1[i] = min;
      else
        accumulator->value1[i] = (accumulator->value1[i] - min * accumulator->value4[i] - max * accumulator->sumWeight[i]) / kept;
    }
    break;
  }
}

static int write_output_pages(const char *outputFile, SDDS_DATASET *templateInput, const OPTIONS *opts, GROUP_LIST *groups) {
  SDDS_DATASET output;
  long iGroup;
  int pagesWritten = 0;

  if (!setup_output_file(&output, outputFile, templateInput, opts))
    goto error;

  for (iGroup = 0; iGroup < groups->groups; iGroup++) {
    ENVELOPE_GROUP *group = groups->group + iGroup;
    ROW_ACCUMULATOR *rowData = best_row_accumulator(group);
    int64_t ignoredRows = 0;
    long iRowData, iStat;

    if (!rowData) {
      fprintf(stderr, "error: all pages have zero rows for %s\n", group->name);
      goto error_with_output;
    }
    for (iRowData = 0; iRowData < group->rowDatas; iRowData++) {
      if (group->rowData + iRowData != rowData)
        ignoredRows += group->rowData[iRowData].pages;
    }
    if (opts->verbose && (group->zeroRowPages || ignoredRows)) {
      fprintf(stderr, "Row-count selection for %s: keeping %" PRId64 " pages with %" PRId64
                      " rows; ignored %" PRId64 " zero-row pages and %" PRId64 " nonmatching pages\n",
              group->name, rowData->pages, rowData->rows, group->zeroRowPages, ignoredRows);
    }

    if (!SDDS_StartPage(&output, rowData->rows) ||
        !SDDS_SetParameters(&output, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, opts->groupBy, group->value, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      goto error_with_output;
    }

    for (iStat = 0; iStat < opts->stats; iStat++) {
      STAT_ACCUMULATOR *accumulator = rowData->stat + iStat;
      STAT_DEFINITION *stat = opts->stat + iStat;
      finalize_stat_values(rowData, opts, iStat);
      if (stat->code == STAT_COPY) {
        if (!SDDS_SetColumn(&output, SDDS_SET_BY_NAME, accumulator->copyData, rowData->rows, stat->resultColumn)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          goto error_with_output;
        }
      } else if (!SDDS_SetColumnFromDoubles(&output, SDDS_SET_BY_NAME, accumulator->value1, rowData->rows, stat->resultColumn)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        goto error_with_output;
      }
    }

    if (!SDDS_WritePage(&output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      goto error_with_output;
    }
    pagesWritten++;
  }

  if (!SDDS_Terminate(&output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    goto error;
  }

  if (pagesWritten == 0) {
    fprintf(stderr, "error: no envelope pages were created\n");
    return 0;
  }
  if (opts->verbose)
    fprintf(stderr, "Wrote %s with %d pages\n", outputFile, pagesWritten);
  return 1;

error_with_output:
  SDDS_Terminate(&output);
error:
  return 0;
}

static void warn_for_missing_requested_group_values(const OPTIONS *opts, const GROUP_LIST *groups) {
  long i;
  for (i = 0; i < opts->groupValue.items; i++) {
    long j;
    int found = 0;
    for (j = 0; j < groups->groups; j++) {
      if (strcmp(opts->groupValue.item[i], groups->group[j].name) == 0) {
        found = 1;
        break;
      }
    }
    if (!found)
      fprintf(stderr, "warning: requested %s value not found: %s\n", opts->groupBy, opts->groupValue.item[i]);
  }
}

static int process_directory(const char *workDir, OPTIONS *opts, int requireInputs) {
  STRING_LIST files;
  GROUP_LIST groups;
  char *finalOutput;
  SDDS_DATASET templateInput;
  int templateOpen = 0;
  int status = PROCESS_ERROR;

  memset(&files, 0, sizeof(files));
  memset(&groups, 0, sizeof(groups));
  finalOutput = xstrdup(opts->output);

  collect_input_files(workDir, opts, finalOutput, &files);
  if (files.items == 0) {
    if (requireInputs)
      fprintf(stderr, "error: no input files matched patterns in %s\n", workDir);
    status = requireInputs ? PROCESS_ERROR : PROCESS_NO_INPUT;
    goto finish;
  }

  if (path_exists(finalOutput)) {
    if (!opts->overwrite) {
      fprintf(stderr, "error: output file already exists: %s\n", finalOutput);
      goto finish;
    }
    if (remove(finalOutput) != 0) {
      fprintf(stderr, "error: unable to remove existing output file %s: %s\n", finalOutput, strerror(errno));
      goto finish;
    }
  }

  if (opts->verbose) {
    long i;
    fprintf(stderr, "Using %ld input files in %s\n", files.items, workDir);
    for (i = 0; i < files.items; i++)
      fprintf(stderr, "  %s\n", base_name(files.item[i]));
  }

  if (!SDDS_InitializeInput(&templateInput, files.item[0])) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    goto finish;
  }
  templateOpen = 1;
  if (!compile_stat_definitions(opts, &templateInput))
    goto finish;

  if (!read_input_files(&files, opts, &groups))
    goto finish;
  warn_for_missing_requested_group_values(opts, &groups);
  if (groups.groups == 0) {
    fprintf(stderr, "error: no %s values selected in %s\n", opts->groupBy, workDir);
    goto finish;
  }
  if (!write_output_pages(finalOutput, &templateInput, opts, &groups))
    goto finish;
  status = PROCESS_DONE;

finish:
  free_group_list(&groups, opts);
  if (templateOpen && !SDDS_Terminate(&templateInput))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  string_list_clear(&files);
  free(finalOutput);
  return status;
}

int main(int argc, char **argv) {
  OPTIONS opts;
  int ok;

  SDDS_RegisterProgramName(argv[0]);
  if (argc == 1) {
    fputs(USAGE, stderr);
    return EXIT_FAILURE;
  }

  init_options(&opts);
  parse_options(&opts, argc, argv);

  ok = process_directory(opts.inputDir, &opts, 1) == PROCESS_DONE;

  free_options(&opts);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
