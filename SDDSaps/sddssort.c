/**
 * @file sddssort.c
 * @brief Sorts an SDDS dataset by column or parameter values.
 *
 * @details
 * The `sddssort` program provides flexible sorting capabilities for SDDS datasets. Users can sort by one or more
 * columns or parameters, perform unique row elimination, and handle multi-criteria optimization through
 * non-dominated sorting. The program supports numeric sorting, absolute value sorting, and major order changes
 * for row or column storage.
 *
 * @section Usage
 * ```
 * sddssort [<inputfile>] [<outputfile>]
 *          [-pipe=[input][,output]]
 *          [-column=<name>[,{increasing|decreasing}|{minimize|maximize}][,absolute]...] 
 *          [-unique[=count]]
 *          [-nowarnings] 
 *          [-parameter=<name>[,{increasing|decreasing}]...]
 *          [-numericHigh] 
 *          [-nonDominateSort] 
 *          [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-pipe`                               | Enables piping for input and/or output.                                               |
 * | `-column`                             | Specifies columns for sorting with optional modifiers. |
 * | `-unique`                             | Removes duplicate rows. If `count` is specified, includes an `IdenticalCount` column. |
 * | `-nowarnings`                         | Suppresses warning messages.                                                         |
 * | `-parameter`                          | Specifies parameters for sorting with optional modifiers.                             |
 * | `-numericHigh`                        | Prioritizes numeric characters in string comparisons.                                 |
 * | `-nonDominateSort`                    | Enables non-dominated sorting for numeric columns.                                    |
 * | `-majorOrder`                         | Changes the data storage order to row-major or column-major.                          |
 *
 * @subsection spec_req Specific Requirements
 *   - For `-nonDominateSort`:
 *     - Requires at least two columns.
 *     - Columns must contain numeric data types.
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
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "non_dominated_sort.h"
#if defined(_WIN32)
#  include <process.h>
#  define pid_t int
#else
#  if defined(linux)
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

/* Enumeration for option types */
enum option_type {
  SET_COLUMN,
  SET_PARAMETER,
  SET_NOWARNINGS,
  SET_PIPE,
  SET_UNIQUE,
  SET_NUMERICHIGH,
  SET_NON_DOMINATE_SORT,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "column",
  "parameter",
  "nowarnings",
  "pipe",
  "unique",
  "numerichigh",
  "nonDominateSort",
  "majorOrder",
};

/* Improved Usage Message */
char *USAGE =
  "sddssort [<SDDSinput>] [<SDDSoutput>]\n"
  "         [-pipe=[input][,output]]\n"
  "         [-column=<name>[,{increasing|decreasing}|{minimize|maximize}][,absolute]...] \n"
  "         [-unique[=count]]\n"
  "         [-nowarnings] \n"
  "         [-parameter=<name>[,{increasing|decreasing}]...]\n"
  "         [-numericHigh] \n"
  "         [-nonDominateSort] \n"
  "         [-majorOrder=row|column]\n"
  "Options:\n"
  "  -pipe=[input][,output]\n"
  "      Enable piping for input and/or output.\n\n"
  "  -column=<name>[,{increasing|decreasing}|{minimize|maximize}][,absolute]...\n"
  "      Specify one or more columns to sort by.\n"
  "      - 'increasing' or 'decreasing' sets the sorting direction for regular sorting.\n"
  "      - 'minimize' or 'maximize' sets the sorting direction for non-dominated sorting.\n"
  "      - 'absolute' sorts based on absolute values.\n\n"
  "  -unique[=count]\n"
  "      Eliminate duplicate rows based on sort columns.\n"
  "      If 'count' is specified, an 'IdenticalCount' column is added to indicate the number of identical rows.\n\n"
  "  -nowarnings\n"
  "      Suppress warning messages.\n\n"
  "  -parameter=<name>[,{increasing|decreasing}]...\n"
  "      Specify parameters to sort by.\n\n"
  "  -numericHigh\n"
  "      Prioritize numeric characters over other characters in string comparisons.\n"
  "      Also ranks numeric character sets with fewer characters below those with more characters.\n\n"
  "  -nonDominateSort\n"
  "      Perform non-dominated sorting when multiple sort columns are provided.\n"
  "      Note: Non-dominated sorting only works for numeric columns.\n\n"
  "  -majorOrder=row|column\n"
  "      Set the major order for data storage, either row-major or column-major.\n\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

typedef struct
{
  char *name;
  long index, type;
  short decreasing_order, maximize_order, absolute;
  double *data;
} SORT_REQUEST;

static char *order_mode[5] = {
  "increasing",
  "decreasing",
  "minimize",
  "maximize",
  "absolute",
};

long SDDS_SortRows(SDDS_DATASET *SDDS_dataset, SORT_REQUEST *xsort_request, long xsort_requests, long non_dominate_sort);
long SDDS_UnsetDuplicateRows(SDDS_DATASET *SDDS_dataset, SORT_REQUEST *xsort_request, long xsort_requests, long provideIdenticalCount);

long SDDS_SortAll(SDDS_DATASET *SDDS_input, SDDS_DATASET *SDDS_output, SORT_REQUEST *xsort_request, long xsort_requests,
                  SORT_REQUEST *xsort_parameter, long xsort_parameters, long uniqueRows, long provideIdenticalCount,
                  long pipeFlags, long non_dominate_sort);

double *read_constr_violation(SDDS_DATASET *SDDS_dataset);

long numericHigh = 0, constDefined = 0;

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_input, SDDS_output, SDDS_tmp;
  char *input, *output;
  long i_arg, non_dominate_sort = 0;
  SCANNED_ARG *s_arg;

  long tmpfile_used, sort_requests, noWarnings, uniqueRows, provideIdenticalCount, tmpfileForInternalPipe;
  long sort_parameters;
  SORT_REQUEST *sort_request, *sort_parameter;
  unsigned long pipeFlags, majorOrderFlag;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    bomb(NULL, USAGE);
  }

  input = output = NULL;
  tmpfile_used = sort_requests = noWarnings = sort_parameters = tmpfileForInternalPipe = 0;
  sort_request = sort_parameter = NULL;
  pipeFlags = 0;
  uniqueRows = provideIdenticalCount = 0;
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_NON_DOMINATE_SORT:
        non_dominate_sort = 1;
        break;
      case SET_COLUMN:
        if (s_arg[i_arg].n_items < 2 || s_arg[i_arg].n_items > 4)
          SDDS_Bomb("invalid -column syntax");
        sort_request = trealloc(sort_request, sizeof(*sort_request) * (sort_requests + 1));
        sort_request[sort_requests].name = s_arg[i_arg].list[1];
        sort_request[sort_requests].maximize_order = 0;
        sort_request[sort_requests].decreasing_order = 0;
        sort_request[sort_requests].absolute = 0;
        if (s_arg[i_arg].n_items >= 3) {
          int j;
          for (j = 2; j < s_arg[i_arg].n_items; j++) {
            switch (match_string(s_arg[i_arg].list[j], order_mode, 5, 0)) {
            case 0:
              break;
            case 1:
              sort_request[sort_requests].decreasing_order = 1;
              break;
            case 2:
              break;
            case 3:
              sort_request[sort_requests].maximize_order = 1;
              break;
            case 4:
              sort_request[sort_requests].absolute = 1;
              break;
            default:
              fprintf(stderr, "unknown sort order specified--give 'increasing' or 'decreasing' for dominated sorting\n or'maximize' or 'minimize' for non-dominated-sorting.\n");
              exit(EXIT_FAILURE);
              break;
            }
          }
        }
        sort_requests++;
        break;
      case SET_PARAMETER:
        if (s_arg[i_arg].n_items < 2 || s_arg[i_arg].n_items > 3)
          SDDS_Bomb("invalid -parameter syntax");
        sort_parameter = trealloc(sort_parameter, sizeof(*sort_parameter) * (sort_parameters + 1));
        sort_parameter[sort_parameters].name = s_arg[i_arg].list[1];
        if (s_arg[i_arg].n_items == 3) {
          if ((sort_parameter[sort_parameters].decreasing_order = match_string(s_arg[i_arg].list[2], order_mode, 2, 0)) < 0)
            SDDS_Bomb("unknown sort order specified--give 'increasing' or 'decreasing'");
        } else
          sort_parameter[sort_parameters].decreasing_order = 0;
        sort_parameters++;
        break;
      case SET_NOWARNINGS:
        noWarnings = 1;
        break;
      case SET_NUMERICHIGH:
        numericHigh = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_UNIQUE:
        uniqueRows = 1;
        if (s_arg[i_arg].n_items > 1) {
          str_tolower(s_arg[i_arg].list[1]);
          if (s_arg[i_arg].n_items > 2 ||
              strncmp("count", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) != 0)
            SDDS_Bomb("invalid -unique syntax");
          provideIdenticalCount = 1;
        }
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (input == NULL)
        input = s_arg[i_arg].list[0];
      else if (output == NULL)
        output = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  if (!sort_requests && !sort_parameters)
    SDDS_Bomb("No sorting requests!");
  processFilenames("sddssort", &input, &output, pipeFlags, noWarnings, &tmpfile_used);
  if (!SDDS_InitializeInput(&SDDS_input, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (sort_requests <= 1)
    non_dominate_sort = 0;

  if (SDDS_input.layout.popenUsed) {
    /* SDDS library has opened the file using a command on a pipe, usually for
     * decompression in the absence of the zlib library.
     */
    pid_t pid;
    char tmpfileName[1024];
    pid = getpid();
    sprintf(tmpfileName, "/tmp/sddssort.%ld", (long)pid);
    tmpfileForInternalPipe = 1;
    if (!SDDS_InitializeCopy(&SDDS_tmp, &SDDS_input, tmpfileName, "w")) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (columnMajorOrder != -1)
      SDDS_tmp.layout.data_mode.column_major = columnMajorOrder;
    else
      SDDS_tmp.layout.data_mode.column_major = SDDS_input.layout.data_mode.column_major;
    if (non_dominate_sort) {
      if (!SDDS_DefineSimpleColumn(&SDDS_output, "Rank", NULL, SDDS_LONG) ||
          !SDDS_DefineSimpleColumn(&SDDS_output, "CrowdingDistance", NULL, SDDS_DOUBLE)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      if (SDDS_CheckColumn(&SDDS_input, "ConstraintsViolation", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) != SDDS_CHECK_OK) {
        if (!SDDS_DefineSimpleColumn(&SDDS_output, "ConstraintsViolation", NULL, SDDS_DOUBLE))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        constDefined = 1;
      }
    }

    if (!SDDS_WriteLayout(&SDDS_tmp)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    while (SDDS_ReadPage(&SDDS_input) > 0) {
      if (!SDDS_CopyPage(&SDDS_tmp, &SDDS_input) || !SDDS_WritePage(&SDDS_tmp)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
    if (!SDDS_Terminate(&SDDS_tmp)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (!SDDS_InitializeInput(&SDDS_input, tmpfileName)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
  if (!SDDS_InitializeCopy(&SDDS_output, &SDDS_input, output, "w")) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (columnMajorOrder != -1)
    SDDS_output.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDS_output.layout.data_mode.column_major = SDDS_input.layout.data_mode.column_major;
  if (provideIdenticalCount &&
      !SDDS_DefineSimpleColumn(&SDDS_output, "IdenticalCount", NULL, SDDS_LONG64)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (non_dominate_sort) {
    if (!SDDS_DefineSimpleColumn(&SDDS_output, "Rank", NULL, SDDS_LONG) ||
        !SDDS_DefineSimpleColumn(&SDDS_output, "CrowdingDistance", NULL, SDDS_DOUBLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (SDDS_CheckColumn(&SDDS_input, "ConstraintsViolation", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) != SDDS_CHECK_OK) {
      if (!SDDS_DefineSimpleColumn(&SDDS_output, "ConstraintsViolation", NULL, SDDS_DOUBLE))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      constDefined = 1;
    }
  }
  if (!SDDS_WriteLayout(&SDDS_output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_SortAll(&SDDS_input, &SDDS_output, sort_request, sort_requests, sort_parameter, sort_parameters,
                    uniqueRows, provideIdenticalCount, pipeFlags, non_dominate_sort)) {
    SDDS_SetError("Problem sorting data");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&SDDS_input) || !SDDS_Terminate(&SDDS_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (tmpfile_used && !replaceFileAndBackUp(input, output))
    exit(EXIT_FAILURE);
  free_scanargs(&s_arg, argc);
  /*  if (tmpfileForInternalPipe)
      system("rm /tmp/tmpfile"); */
  return EXIT_SUCCESS;
}

static SDDS_DATASET *SDDS_sort;
static SORT_REQUEST *sort_request;
static long sort_requests;
static int64_t *sort_row_index;

int SDDS_CompareData(SDDS_DATASET *SDDS_dataset, short type, short absolute, void *data1, void *data2) {
  double ldouble_diff;
  double double_diff;
  float float_diff;
  int32_t long_diff;
  int64_t long64_diff;
  short short_diff;
  int char_diff;

  if (!absolute) {
    switch (type) {
    case SDDS_LONGDOUBLE:
      if ((ldouble_diff = *(long double *)data1 - *(long double *)data2) > 0)
        return (1);
      else if (ldouble_diff < 0)
        return (-1);
      return (0);
    case SDDS_DOUBLE:
      if ((double_diff = *(double *)data1 - *(double *)data2) > 0)
        return (1);
      else if (double_diff < 0)
        return (-1);
      return (0);
    case SDDS_FLOAT:
      if ((float_diff = *(float *)data1 - *(float *)data2) > 0)
        return (1);
      else if (float_diff < 0)
        return (-1);
      return (0);
    case SDDS_LONG64:
      if ((long64_diff = *(int64_t *)data1 - *(int64_t *)data2) > 0)
        return (1);
      else if (long64_diff < 0)
        return (-1);
      return (0);
    case SDDS_LONG:
      if ((long_diff = *(int32_t *)data1 - *(int32_t *)data2) > 0)
        return (1);
      else if (long_diff < 0)
        return (-1);
      return (0);
    case SDDS_SHORT:
      if ((short_diff = *(short *)data1 - *(short *)data2) > 0)
        return (1);
      else if (short_diff < 0)
        return (-1);
      return (0);
    case SDDS_ULONG64:
      if (*(uint64_t *)data1 > *(uint64_t *)data2)
        return 1;
      if (*(uint64_t *)data1 < *(uint64_t *)data2)
        return -1;
      return 0;
    case SDDS_ULONG:
      if (*(uint32_t *)data1 > *(uint32_t *)data2)
        return 1;
      if (*(uint32_t *)data1 < *(uint32_t *)data2)
        return -1;
      return 0;
    case SDDS_USHORT:
      if (*(unsigned short *)data1 > *(unsigned short *)data2)
        return 1;
      if (*(unsigned short *)data1 < *(unsigned short *)data2)
        return -1;
      return 0;
    case SDDS_CHARACTER:
      if ((char_diff = *(char *)data1 - *(char *)data2) > 0)
        return (1);
      else if (char_diff < 0)
        return (-1);
      return (0);
    case SDDS_STRING:
      if (numericHigh)
        return (strcmp_nh(*(char **)data1, *(char **)data2));
      else
        return (strcmp(*(char **)data1, *(char **)data2));
    default:
      SDDS_SetError("Problem doing data comparison--invalid data type (SDDS_CompareData)");
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  } else {
    switch (type) {
    case SDDS_LONGDOUBLE:
      if ((ldouble_diff = fabsl(*(long double *)data1) - fabsl(*(long double *)data2)) > 0)
        return (1);
      else if (ldouble_diff < 0)
        return (-1);
      return (0);
    case SDDS_DOUBLE:
      if ((double_diff = fabs(*(double *)data1) - fabs(*(double *)data2)) > 0)
        return (1);
      else if (double_diff < 0)
        return (-1);
      return (0);
    case SDDS_FLOAT:
      if ((float_diff = fabsf(*(float *)data1) - fabsf(*(float *)data2)) > 0)
        return (1);
      else if (float_diff < 0)
        return (-1);
      return (0);
    case SDDS_LONG64:
      if ((long64_diff = llabs(*(int64_t *)data1) - llabs(*(int64_t *)data2)) > 0)
        return (1);
      else if (long64_diff < 0)
        return (-1);
      return (0);
    case SDDS_LONG:
      if ((long_diff = labs(*(int32_t *)data1) - labs(*(int32_t *)data2)) > 0)
        return (1);
      else if (long_diff < 0)
        return (-1);
      return (0);
    case SDDS_SHORT:
      if ((short_diff = abs(*(short *)data1) - abs(*(short *)data2)) > 0)
        return (1);
      else if (short_diff < 0)
        return (-1);
      return (0);
    case SDDS_ULONG64:
      if (*(uint64_t *)data1 > *(uint64_t *)data2)
        return (1);
      if (*(uint64_t *)data1 < *(uint64_t *)data2)
        return (-1);
      return (0);
    case SDDS_ULONG:
      if (*(uint32_t *)data1 > *(uint32_t *)data2)
        return (1);
      if (*(uint32_t *)data1 < *(uint32_t *)data2)
        return (-1);
      return (0);
    case SDDS_USHORT:
      if (*(unsigned short *)data1 > *(unsigned short *)data2)
        return (1);
      if (*(unsigned short *)data1 < *(unsigned short *)data2)
        return (-1);
      return (0);
    case SDDS_CHARACTER:
      if ((char_diff = *(char *)data1 - *(char *)data2) > 0)
        return (1);
      else if (char_diff < 0)
        return (-1);
      return (0);
    case SDDS_STRING:
      if (numericHigh)
        return (strcmp_nh(*(char **)data1, *(char **)data2));
      else
        return (strcmp(*(char **)data1, *(char **)data2));
    default:
      SDDS_SetError("Problem doing data comparison--invalid data type (SDDS_CompareData)");
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }
}

int SDDS_CompareRows(const void *vrow1, const void *vrow2) {
  /* I'm assuming that a double is large enough to store any of the data types,
   * including a pointer
   */
  static double data1, data2;
  static int64_t row1, row2;
  long i;
  int comparison;
  void *p1, *p2;
  row1 = *(int64_t *)vrow1;
  row2 = *(int64_t *)vrow2;

  for (i = 0; i < sort_requests; i++) {
    if (!SDDS_GetValueByAbsIndex(SDDS_sort, sort_request[i].index, row1, &data1) ||
        !SDDS_GetValueByAbsIndex(SDDS_sort, sort_request[i].index, row2, &data2)) {
      SDDS_SetError("Problem getting value for sort (SDDS_CompareRows)");
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if ((comparison = SDDS_CompareData(SDDS_sort, sort_request[i].type, sort_request[i].absolute,
                                       (void *)&data1, (void *)&data2))) {
      if (sort_request[i].type == SDDS_STRING) {
        p1 = &data1;
        p2 = &data2;
        free(*((char **)p1));
        free(*((char **)p2));
        // free(*((char**)&data1));
        // free(*((char**)&data2));
      }
      return (sort_request[i].decreasing_order ? -comparison : comparison);
    }
    if (sort_request[i].type == SDDS_STRING) {
      p1 = &data1;
      p2 = &data2;
      free(*((char **)p1));
      free(*((char **)p2));
      // free(*((char**)&data1));
      // free(*((char**)&data2));
    }
  }
  return (0);
}

long SDDS_SwapRows(SDDS_DATASET *SDDS_dataset, int64_t row1, int64_t row2) {
#define SWAP_BUFFER_SIZE 16
  static char buffer[SWAP_BUFFER_SIZE];
  void **data;
  long i, size;
  data = SDDS_dataset->data;
#if defined(DEBUG)
  fprintf(stderr, "swapping row %" PRId64 " with row %" PRId64 "\n", row1, row2);
#endif
  for (i = 0; i < SDDS_dataset->layout.n_columns; i++) {
    if ((size = SDDS_GetTypeSize(SDDS_dataset->layout.column_definition[i].type)) > SWAP_BUFFER_SIZE) {
      SDDS_SetError("Unable to swap rows--swap buffer is too small (SDDS_SwapRows)");
      return (0);
    }
#if defined(DEBUG)
    if (SDDS_dataset->layout.column_definition[i].type == SDDS_STRING)
      fprintf(stderr, "    %s  <-->  %s\n", *(char **)((char *)(data[i]) + row1 * size), *(char **)((char *)(data[i]) + row2 * size));
#endif
    memcpy((char *)buffer, (char *)(data[i]) + row1 * size, size);
    memcpy((char *)(data[i]) + row1 * size, (char *)(data[i]) + row2 * size, size);
    memcpy((char *)(data[i]) + row2 * size, (char *)buffer, size);
  }
  return (1);
}

long SDDS_SortRows(SDDS_DATASET *SDDS_dataset, SORT_REQUEST *xsort_request, long xsort_requests, long non_dominate_sort) {
  int64_t i, j, k, rows;
  int64_t *row_location;
  char s[1024];
  double **data = NULL, *dist = NULL, *const_violation = NULL;
  int32_t *rank = NULL;
  population pop;
  long *maximize = NULL;

  SDDS_sort = SDDS_dataset;
  sort_request = xsort_request;
  sort_requests = xsort_requests;
  if ((rows = SDDS_CountRowsOfInterest(SDDS_sort)) < 0)
    return (0);

  for (i = 0; i < sort_requests; i++) {
    if ((sort_request[i].index = SDDS_GetColumnIndex(SDDS_sort, sort_request[i].name)) < 0) {
      sprintf(s, "column name \"%s\" is not recognized(SDDS_GetColumnIndex)", sort_request[i].name);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return (0);
    }
    sort_request[i].type = SDDS_GetColumnType(SDDS_sort, sort_request[i].index);
  }
  if (non_dominate_sort) {
    data = (double **)malloc(sizeof(*data) * sort_requests);
    maximize = (long *)malloc(sizeof(*maximize) * sort_requests);
    for (i = 0; i < sort_requests; i++) {
      if (sort_request[i].type == SDDS_STRING) {
        fprintf(stderr, "Non-dominated sort is not available for string column.\n");
        exit(EXIT_FAILURE);
      }
      if (!(data[i] = (double *)SDDS_GetColumnInDoubles(SDDS_sort, sort_request[i].name))) {
        SDDS_SetError("Problem performing sort");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        exit(EXIT_FAILURE);
      }
      maximize[i] = sort_request[i].maximize_order;
    }
    const_violation = read_constr_violation(SDDS_sort);
    fill_population(&pop, rows, sort_requests, data, maximize, const_violation);
    sort_row_index = non_dominated_sort(&pop);
    rank = (int32_t *)malloc(sizeof(*rank) * rows);
    dist = (double *)malloc(sizeof(*dist) * rows);
    if (!const_violation)
      const_violation = calloc(rows, sizeof(*const_violation));
    for (i = 0; i < rows; i++) {
      rank[i] = pop.ind[sort_row_index[i]].rank;
      dist[i] = pop.ind[sort_row_index[i]].crowd_dist;
      const_violation[i] = pop.ind[sort_row_index[i]].constr_violation;
    }
    free_pop_mem(&pop);
    for (i = 0; i < sort_requests; i++)
      free(data[i]);
    free(data);
    free(maximize);
  } else {
    sort_row_index = tmalloc(sizeof(*sort_row_index) * rows);
    for (i = 0; i < rows; i++)
      sort_row_index[i] = i;
    /* After this sort, sort_row_index will contain the indices of the rows
     * in sorted order
     */
    qsort((void *)sort_row_index, rows, sizeof(*sort_row_index), SDDS_CompareRows);

    /* create an array to give the location in the sort_row_index array of
     * a particular row
     */
#if defined(DEBUG)
    fprintf(stderr, "new row order:\n");
    for (i = 0; i < rows; i++)
      fprintf(stderr, "%" PRId64 " %" PRId64 "\n", i, sort_row_index[i]);
    fprintf(stderr, "\n");
#endif
  }
  row_location = tmalloc(sizeof(*sort_row_index) * rows);
  for (i = 0; i < rows; i++)
    row_location[sort_row_index[i]] = i;
  for (i = 0; i < rows; i++) {
    if ((j = sort_row_index[i]) != i) {
      /* move this row from position i to position j, where it belongs */
      if (!SDDS_SwapRows(SDDS_sort, i, j)) {
        SDDS_SetError("Problem swapping rows after index sort (SDDS_SortRows");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      /* adjust the indices to reflect the swap */
      sort_row_index[i] = i;
      k = row_location[i];
      row_location[i] = i;
      sort_row_index[k] = j;
      row_location[j] = k;
    }
#if defined(DEBUG)
    fprintf(stderr, "new row order:\n");
    for (j = 0; j < rows; j++)
      fprintf(stderr, "%" PRId64 " %" PRId64 "\n", j, sort_row_index[j]);
    fprintf(stderr, "\n");
#endif
  }
  if (non_dominate_sort) {
    if (SDDS_SetColumn(SDDS_sort, SDDS_SET_BY_NAME, rank, rows, "Rank", NULL) != 1 ||
        SDDS_SetColumn(SDDS_sort, SDDS_SET_BY_NAME, dist, rows, "CrowdingDistance", NULL) != 1 ||
        SDDS_SetColumn(SDDS_sort, SDDS_SET_BY_NAME, const_violation, rows, "ConstraintsViolation", NULL) != 1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    free(rank);
    free(dist);
    free(const_violation);
  }
  free(sort_row_index);
  free(row_location);
  return 1;
}

long SDDS_UnsetDuplicateRows(SDDS_DATASET *SDDS_dataset, SORT_REQUEST *xsort_request, long xsort_requests, long provideIdenticalCount) {
  int64_t rows, i, j;
  int64_t *identicalCount;
  int32_t *rowFlag;
  char s[1024];

  SDDS_sort = SDDS_dataset;
  sort_request = xsort_request;
  sort_requests = xsort_requests;

  for (i = 0; i < sort_requests; i++) {
    if ((sort_request[i].index = SDDS_GetColumnIndex(SDDS_sort, sort_request[i].name)) < 0) {
      sprintf(s, "column name \"%s\" is not recognized(SDDS_GetColumnIndex)", sort_request[i].name);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return 0;
    }
    sort_request[i].type = SDDS_GetColumnType(SDDS_sort, sort_request[i].index);
  }

  if ((rows = SDDS_CountRowsOfInterest(SDDS_sort)) < 0)
    return (0);

  rowFlag = tmalloc(sizeof(*rowFlag) * rows);
  identicalCount = tmalloc(sizeof(*identicalCount) * rows);
  for (i = 0; i < rows; i++)
    rowFlag[i] = identicalCount[i] = 1;

  for (i = 0; i < rows - 1; i++) {
    if (!rowFlag[i])
      continue;
    for (j = i + 1; j < rows; j++) {
      if (rowFlag[j]) {
        if (SDDS_CompareRows(&i, &j) == 0) {
          identicalCount[i] += 1;
          rowFlag[j] = 0;
        } else
          break;
      }
    }
  }
  if (!SDDS_AssertRowFlags(SDDS_sort, SDDS_FLAG_ARRAY, rowFlag, rows))
    return 0;
  if (provideIdenticalCount && !SDDS_SetColumn(SDDS_sort, SDDS_SET_BY_NAME, identicalCount, rows, "IdenticalCount"))
    return 0;
  free(rowFlag);
  return 1;
}

static SORT_REQUEST *sort_parameter;
static long sort_parameters;
static SDDS_DATASET *SDDS_sortpage;

int SDDS_ComparePages(const void *vpage1, const void *vpage2) {
  /* I'm assuming that a double is large enough to store any of the data types,
   * including a pointer
   */
  static int32_t page1, page2;
  long i;
  int comparison;

  page1 = *(int32_t *)vpage1;
  page2 = *(int32_t *)vpage2;

  for (i = 0; i < sort_parameters; i++) {
    if ((comparison = SDDS_CompareData(SDDS_sortpage, sort_parameter[i].type, sort_parameter[i].absolute,
                                       (void *)&(sort_parameter[i].data[page1]), (void *)&(sort_parameter[i].data[page2])))) {
      /* if (sort_parameter[i].type==SDDS_STRING) {
       *   free(*((char**)&(sort_parameter[i].data[page1])));
       *   free(*((char**)&(sort_parameter[i].data[page2])));
       * } */
      return (sort_parameter[i].decreasing_order ? -comparison : comparison);
    }
    /* if (sort_parameter[i].type==SDDS_STRING) {
     *   free(*((char**)&(sort_parameter[i].data[page1])));
     *   free(*((char**)&(sort_parameter[i].data[page2])));
     * } */
  }
  return (0);
}

long SDDS_SortAll(SDDS_DATASET *SDDS_inputx, SDDS_DATASET *SDDS_outputx, SORT_REQUEST *xsort_request,
                  long xsort_requests, SORT_REQUEST *xsort_parameter, long xsort_parameters, long uniqueRows,
                  long provideIdenticalCount, long xpipeFlags, long non_dominate_sort) {
  long i, j, k, pages;
  int32_t *xsort_page_index;
  char s[1024];
  SDDS_DATASET *tmp_datasets;

  tmp_datasets = NULL;
  SDDS_sortpage = SDDS_inputx;
  sort_parameter = xsort_parameter;
  sort_parameters = xsort_parameters;
  sort_request = xsort_request;
  sort_requests = xsort_requests;
  xsort_page_index = NULL;
  pages = i = j = k = 0;
  for (i = 0; i < sort_parameters; i++) {
    if ((sort_parameter[i].index = SDDS_GetParameterIndex(SDDS_sortpage, sort_parameter[i].name)) < 0) {
      sprintf(s, "Unable to get parameter value--parameter name \"%s\" is not recognized(SDDS_GetParameterIndex)", sort_parameter[i].name);
      SDDS_SetError(s);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return (0);
    }
    sort_parameter[i].type = SDDS_GetParameterType(SDDS_sortpage, sort_parameter[i].index);
  }
  if (sort_parameters) {
    if (xpipeFlags == 0 && !SDDS_sortpage->layout.gzipFile)
      SDDS_SetDefaultIOBufferSize(0);
    for (i = 0; i < sort_parameters; i++)
      sort_parameter[i].data = NULL;
    while (SDDS_ReadPage(SDDS_sortpage) > 0) {
      if (xpipeFlags || SDDS_sortpage->layout.gzipFile) {
        /* copy each page of sortpage into tmp_datasets indexed by the pages, i.e.
         * tmp_datasets[pages] contains only one page */
        tmp_datasets = realloc(tmp_datasets, sizeof(*tmp_datasets) * (pages + 1));
        if (!SDDS_InitializeCopy(&tmp_datasets[pages], SDDS_sortpage, NULL, "m")) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          return (0);
        }
        if (!SDDS_CopyPage(&tmp_datasets[pages], SDDS_sortpage)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          return (0);
        }
      }
      for (i = 0; i < sort_parameters; i++) {
        sort_parameter[i].data = realloc(sort_parameter[i].data, sizeof(*sort_parameter[i].data) * (pages + 1));

        if (!SDDS_GetParameterByIndex(SDDS_sortpage, sort_parameter[i].index, &(sort_parameter[i].data[pages]))) {
          SDDS_SetError("Problem getting parameter value for sort (SDDS_SortAll)");
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
      pages++;
    }
    /* sort pages by parameter */
    xsort_page_index = tmalloc(sizeof(*xsort_page_index) * pages);
    for (k = 0; k < pages; k++)
      xsort_page_index[k] = k;
    if (pages > 1)
      qsort((void *)xsort_page_index, pages, sizeof(*xsort_page_index), SDDS_ComparePages);

    /* sort_page_index=xsort_page_index; */
    for (i = 0; i < pages; i++) {
      j = xsort_page_index[i];
      if (xpipeFlags || SDDS_sortpage->layout.gzipFile) {
        if (!SDDS_CopyPage(SDDS_outputx, &tmp_datasets[j])) {
          SDDS_SetError("Problem copying data from memory");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
        if (!SDDS_Terminate(&tmp_datasets[j])) {
          SDDS_SetError("Problem terminate datasets");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else {
        if (!SDDS_GotoPage(SDDS_sortpage, j + 1)) {
          SDDS_SetError("Problem goto page");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
        if (SDDS_ReadPage(SDDS_sortpage) < 1) {
          SDDS_SetError("Problem read page");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
        if (!SDDS_CopyPage(SDDS_outputx, SDDS_sortpage)) {
          SDDS_SetError("Problem copying data");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
      if (sort_requests) {
        if (SDDS_CountRowsOfInterest(SDDS_outputx) > 0) {
          if (!SDDS_SortRows(SDDS_outputx, sort_request, sort_requests, non_dominate_sort)) {
            SDDS_SetError("Problem performing sort");
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            exit(EXIT_FAILURE);
          }
          if (uniqueRows && !SDDS_UnsetDuplicateRows(SDDS_outputx, sort_request, sort_requests, provideIdenticalCount)) {
            SDDS_SetError("Problem marking duplicate rows.");
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            exit(EXIT_FAILURE);
          }
        }
      }
      if (!SDDS_WritePage(SDDS_outputx)) {
        SDDS_SetError("Problem writing data to output file");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  } else {
    while (SDDS_ReadPage(SDDS_inputx) > 0) {
      if (!SDDS_CopyPage(SDDS_outputx, SDDS_inputx)) {
        SDDS_SetError("Problem copying data for output file");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        exit(EXIT_FAILURE);
      }
      if (SDDS_CountRowsOfInterest(SDDS_outputx) > 0) {
        if (!SDDS_SortRows(SDDS_outputx, sort_request, sort_requests, non_dominate_sort)) {
          SDDS_SetError("Problem performing sort");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
        if (uniqueRows && !SDDS_UnsetDuplicateRows(SDDS_outputx, sort_request, sort_requests, provideIdenticalCount)) {
          SDDS_SetError("Problem marking duplicate rows.");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
      if (!SDDS_WritePage(SDDS_outputx)) {
        SDDS_SetError("Problem writing data to output file");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }
  if (tmp_datasets)
    free(tmp_datasets);
  if (xsort_page_index)
    free(xsort_page_index);
  return 1;
}

double *read_constr_violation(SDDS_DATASET *SDDS_dataset) {
  double *const_violation = NULL, **data = NULL;
  int32_t columns, constraints, j;
  int64_t i, rows;
  char **columnName = NULL;

  columns = constraints = 0;
  rows = SDDS_CountRowsOfInterest(SDDS_dataset);
  if (!constDefined && !(const_violation = (double *)SDDS_GetColumnInDoubles(SDDS_dataset, "ConstraintsViolation")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  else {
    if (!(columnName = (char **)SDDS_GetColumnNames(SDDS_dataset, &columns)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < columns; i++) {
      if (wild_match(columnName[i], "*Constraints*") && strcmp(columnName[i], "ConstraintsViolation") != 0) {
        data = SDDS_Realloc(data, sizeof(*data) * (constraints + 1));
        data[constraints] = NULL;
        if (!(data[constraints] = (double *)SDDS_GetColumnInDoubles(SDDS_dataset, columnName[i])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        constraints++;
      }
    }
    if (constraints) {
      const_violation = calloc(rows, sizeof(*const_violation));
      for (i = 0; i < rows; i++) {
        for (j = 0; j < constraints; j++) {
          if (data[j][i] < 0)
            const_violation[i] += data[j][i];
        }
      }
      for (j = 0; j < constraints; j++)
        free(data[j]);
      free(data);
    }
  }
  return const_violation;
}
