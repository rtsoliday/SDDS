/**
 * @file citi2sdds.c
 * @brief Converts a data file from Hewlett-Packard/Keysight Technologies' CITI format to SDDS format.
 *
 * @details
 * This program reads a waveform data file in Hewlett-Packard/Keysight Technologies' CITI format 
 * (Common Instrumentation Transfer and Interchange file) and converts it into the SDDS 
 * (Self Describing Data Sets) format. It supports various options to customize the output, 
 * including setting descriptions, MPL labels, and specifying the major order of data.
 *
 * @section Usage
 * ```
 * citi2sdds <inputfile> <outputfile>
 *           [-description=<text>,<contents>]
 *           [-mpllabels=<title>,<topline>] 
 *           [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-description`                        | Adds a description text and content to the SDDS output file.                         |
 * | `-mpllabels`                          | Sets MPL (Matplotlib) title and top-line labels for the SDDS output.                 |
 * | `-majorOrder`                         | Specifies the major order of data in the SDDS output. Defaults to row-major order.   |
 *
 * @subsection Incompatibilities
 * - Only one of the following options may be specified:
 *     - `-description`
 *     - `-mpllabels`
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
#include "match_string.h"

/* Enumeration for option types */
enum option_type {
  SET_DESCRIPTION,
  SET_MPL_LABELS,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "description",
  "mpllabels",
  "majorOrder"
};

char *USAGE =
  "Usage:\n"
  "  citi2sdds <inputfile> <outputfile>\n"
  "            [-description=<text>,<contents>]\n"
  "            [-mpllabels=<title>,<topline>] \n"
  "            [-majorOrder=row|column]\n"
  "Options:\n"
  "  -description=<text>,<contents>    Add a description to the SDDS output.\n"
  "  -mpllabels=<title>,<topline>      Set MPL title and top line labels.\n"
  "  -majorOrder=row|column             Specify the major order of data.\n\n"
  "This program converts HP CITI format waveforms to SDDS format.\n\n"
  "Program by Michael Borland (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define BUFSIZE 256

#define CITIFILE_TAG "CITIFILE"
#define CITIFILE_VERSION "A.01.00"

#define CITI_NA_KEYWORD 0
#define CITI_NAME_KEYWORD 1
#define CITI_VAR_KEYWORD 2
#define CITI_DATA_KEYWORD 3
#define CITI_VAR_LIST_KEYWORD 4
#define CITI_BEGIN_KEYWORD 5
#define CITI_SEG_LIST_BEGIN 6
#define CITI_COMMENT_KEYWORD 7
#define CITI_CONSTANT_KEYWORD 8
#define N_CITI_KEYWORDS 9
static char *citi_keyword[N_CITI_KEYWORDS] = {
  "#NA", "NAME", "VAR", "DATA", "VAR_LIST_BEGIN", "BEGIN",
  "SEG_LIST_BEGIN", "COMMENT", "CONSTANT"};

long read_CITI_var_list(FILE *fpi, double *var_list, long points);
long read_CITI_data(FILE *fpi, double *real_data, double *imag_data, long points);
long read_CITI_seg_list(FILE *fpi, double *seg_list, long points);
void alter_data_name(char *name);

int main(int argc, char **argv) {
  SDDS_TABLE SDDS_table;
  SCANNED_ARG *scanned;
  long i, i_arg, points;
  char *input, *output, *name, *indep_var, *var_list_format;
  char *mpl_title, *mpl_topline, *descrip_text, *descrip_contents;
  FILE *fpi;
  char buffer[BUFSIZE], buffer0[BUFSIZE], buffer1[BUFSIZE];
  char *ptr, *ptr1, *ptr2;
  char **data_name, *package_name, *data_format;
  long data_names, data_sets_seen, data_sets_expected, realIndex, imagIndex;
  double **real_data, **imag_data, *var_list;
  unsigned long majorOrderFlag;
  short columnMajorOrder = 0;

  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  input = output = package_name = data_format = indep_var = var_list_format = ptr = NULL;
  mpl_title = mpl_topline = descrip_text = descrip_contents = NULL;
  data_names = realIndex = imagIndex = 0;
  data_name = NULL;
  real_data = imag_data = NULL;
  var_list = NULL;
  data_sets_expected = data_sets_seen = 0;
  points = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      delete_chars(scanned[i_arg].list[0], "_");
      /* process options here */
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[i_arg].n_items--;
        if (scanned[i_arg].n_items > 0 && (!scanItemList(&majorOrderFlag, scanned[i_arg].list + 1, &scanned[i_arg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_DESCRIPTION:
        if (scanned[i_arg].n_items != 3) {
          SDDS_Bomb("invalid -description syntax");
        }
        descrip_text = scanned[i_arg].list[1];
        descrip_contents = scanned[i_arg].list[2];
        break;
      case SET_MPL_LABELS:
        if (scanned[i_arg].n_items != 3) {
          SDDS_Bomb("invalid -mpllabels syntax");
        }
        mpl_title = scanned[i_arg].list[1];
        mpl_topline = scanned[i_arg].list[2];
        break;
      default:
        SDDS_Bomb("invalid option seen");
        break;
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else {
        SDDS_Bomb("too many filenames");
      }
    }
  }
  if (!input) {
    SDDS_Bomb("input file not seen");
  }
  if (!output) {
    SDDS_Bomb("output file not seen");
  }

  fpi = fopen_e(input, "r", 0);
  if (!fgets(buffer, BUFSIZE, fpi) || strncmp(buffer, CITIFILE_TAG, strlen(CITIFILE_TAG)) != 0 || !(ptr = strchr(buffer, ' '))) {
    SDDS_Bomb("valid CITIFILE version line not found");
  }
  *ptr++ = 0;
  ptr[strlen(ptr) - 1] = 0;
  if (strncmp(ptr, CITIFILE_VERSION, strlen(CITIFILE_VERSION)) != 0)
    fprintf(stderr, "warning: the CITIFILE version is %s--this program is only designed for version %s\n", ptr, CITIFILE_VERSION);

  if (!SDDS_InitializeOutput(&SDDS_table, SDDS_BINARY, 0, descrip_text, descrip_contents, output))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  SDDS_table.layout.data_mode.column_major = columnMajorOrder;
  if ((mpl_title && SDDS_DefineParameter(&SDDS_table, "mplTitle", NULL, NULL, NULL, NULL, SDDS_STRING, mpl_title) < 0) ||
      (mpl_topline && SDDS_DefineParameter(&SDDS_table, "mplTopline", NULL, NULL, NULL, NULL, SDDS_STRING, mpl_topline) < 0))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  while (fgets(buffer, BUFSIZE, fpi)) {
#if DEBUG
    fputs(buffer, stderr);
#endif
    buffer[strlen(buffer) - 1] = 0;
    strcpy(buffer0, buffer);
    ptr1 = NULL;
    if ((ptr1 = strchr(buffer, ' ')))
      *ptr1++ = 0;
    switch (match_string(buffer, citi_keyword, N_CITI_KEYWORDS, EXACT_MATCH)) {
    case CITI_NA_KEYWORD:
      name = buffer + 1;
      if (!*ptr1 || !(ptr2 = strchr(ptr1, ' '))) {
        fprintf(stderr, "The following line contains an apparently invalid #NA keyword:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
      *(ptr1 - 1) = '_';
      *ptr2++ = 0;
      if (SDDS_DefineParameter(&SDDS_table, name, NULL, NULL, NULL, NULL, SDDS_STRING, ptr2) < 0) {
        SDDS_SetError(buffer0);
        SDDS_SetError("Problem creating parameter for #NA keyword in the following line:");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      break;
    case CITI_NAME_KEYWORD:
      if (!ptr1 || SDDS_StringIsBlank(ptr1)) {
        fprintf(stderr, "The following line contains erroneous input:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
#if DEBUG
      fprintf(stderr, "NAME: %s\n", ptr1);
#endif
      cp_str(&package_name, ptr1);
      if (SDDS_DefineParameter(&SDDS_table, "CITIPackageName", NULL, NULL, NULL, NULL, SDDS_STRING, package_name) < 0) {
        SDDS_SetError(buffer0);
        SDDS_SetError("Problem creating parameter for NAME keyword in the following line:");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      break;
    case CITI_VAR_KEYWORD:
      if (!ptr1 || SDDS_StringIsBlank(ptr1)) {
        fprintf(stderr, "The following line contains erroneous input:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
#if DEBUG
      fprintf(stderr, "VAR: %s\n", ptr1);
#endif
      if (!(indep_var = get_token(ptr1)) || !(var_list_format = get_token(ptr1)) || !get_long(&points, ptr1) || points <= 0) {
        fprintf(stderr, "The following line contains erroneous input:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
      if (SDDS_DefineColumn(&SDDS_table, indep_var, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0) {
        SDDS_SetError(buffer0);
        SDDS_SetError("Problem creating column for data element in the following line:");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      break;
    case CITI_CONSTANT_KEYWORD:
      if (!ptr1 || !(ptr2 = strchr(ptr1, ' '))) {
        fprintf(stderr, "The following line contains erroneous input:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
#if DEBUG
      fprintf(stderr, "CONSTANT: %s\n", ptr1);
#endif
      *ptr2++ = 0;
      if (SDDS_DefineParameter(&SDDS_table, ptr1, NULL, NULL, NULL, NULL, SDDS_STRING, ptr2) < 0) {
        SDDS_SetError(buffer0);
        SDDS_SetError("Problem creating parameter for CONSTANT keyword in the following line:");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      break;
    case CITI_COMMENT_KEYWORD:
      if (!ptr1 || SDDS_StringIsBlank(ptr1)) {
        fprintf(stderr, "The following line contains erroneous input:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
#if DEBUG
      fprintf(stderr, "COMMENT: %s\n", ptr1);
#endif
      break;
    case CITI_DATA_KEYWORD:
      if (!ptr1 || !(ptr2 = strchr(ptr1, ' '))) {
        fprintf(stderr, "The following line contains erroneous input:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
#if DEBUG
      fprintf(stderr, "DATA: %s\n", ptr1);
#endif
      *ptr2++ = 0;
      cp_str(&data_format, ptr2);
      data_name = trealloc(data_name, sizeof(*data_name) * (data_names + 1));
      cp_str(data_name + data_names, ptr1);
      alter_data_name(data_name[data_names]);
      real_data = trealloc(real_data, sizeof(*real_data) * (data_names + 1));
      imag_data = trealloc(imag_data, sizeof(*imag_data) * (data_names + 1));
      sprintf(buffer, "%sReal", data_name[data_names]);
      sprintf(buffer1, "%sImag", data_name[data_names]);
      if ((realIndex = SDDS_DefineColumn(&SDDS_table, buffer, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0 ||
          (imagIndex = SDDS_DefineColumn(&SDDS_table, buffer1, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) < 0) {
        SDDS_SetError(buffer0);
        SDDS_SetError("Problem creating column for data element in the following line:");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      data_names++;
      break;
    case CITI_VAR_LIST_KEYWORD:
      if (ptr1 && !SDDS_StringIsBlank(ptr1)) {
        fprintf(stderr, "The following line contains erroneous input:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
#if DEBUG
      fprintf(stderr, "VAR_LIST_BEGIN seen\n");
#endif
      if (points == 0)
        SDDS_Bomb("VAR_LIST_BEGIN statement seen without prior VAR statement");
      var_list = tmalloc(sizeof(*var_list) * points);
      if (!read_CITI_var_list(fpi, var_list, points))
        SDDS_Bomb("unable to read VAR_LIST");
      break;
    case CITI_BEGIN_KEYWORD:
      if (ptr1 && !SDDS_StringIsBlank(ptr1)) {
        fprintf(stderr, "The following line contains erroneous input:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
#if DEBUG
      fprintf(stderr, "BEGIN seen\n");
#endif
      if (points == 0)
        SDDS_Bomb("BEGIN statement seen without prior VAR statement");
      real_data[data_sets_seen] = tmalloc(sizeof(**real_data) * points);
      imag_data[data_sets_seen] = tmalloc(sizeof(**imag_data) * points);
      if (!read_CITI_data(fpi, real_data[data_sets_seen], imag_data[data_sets_seen], points))
        SDDS_Bomb("problem reading data section");
      data_sets_seen++;
      break;
    case CITI_SEG_LIST_BEGIN:
      if (ptr1 && !SDDS_StringIsBlank(ptr1)) {
        fprintf(stderr, "The following line contains erroneous input:\n%s\n", buffer0);
        return EXIT_FAILURE;
      }
#if DEBUG
      fprintf(stderr, "SEG_LIST_BEGIN seen\n");
#endif
      if (points == 0)
        SDDS_Bomb("SEG_LIST_BEGIN statement seen without prior SEG statement");
      var_list = tmalloc(sizeof(*var_list) * points);
      if (!read_CITI_seg_list(fpi, var_list, points))
        SDDS_Bomb("unable to read SEG_LIST");
      break;
    default:
      fprintf(stderr, "Unidentifiable line in file--not CITI format:\n\"%s\"\n", buffer0);
      return EXIT_FAILURE;
      break;
    }
  }
  if (!points)
    SDDS_Bomb("no data in file");
  if (data_sets_seen != (data_sets_expected = data_names))
    SDDS_Bomb("fewer data sets than expected were actually present");
  if (!var_list) {
    fprintf(stderr, "warning: no independent variable data---supplying index\n");
    var_list = tmalloc(sizeof(*var_list) * points);
    for (i = 0; i < points; i++)
      var_list[i] = i;
  }
  if (!SDDS_WriteLayout(&SDDS_table) || !SDDS_StartTable(&SDDS_table, points))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, (void *)var_list, points, indep_var))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  for (i = 0; i < data_sets_expected; i++) {
    if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_INDEX, (void *)real_data[i], points, realIndex) ||
        !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_INDEX, (void *)imag_data[i], points, imagIndex)) {
      fprintf(stderr, "Problem setting data for column(s) %s\n", data_name[i]);
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  }
  if (!SDDS_WriteTable(&SDDS_table) || !SDDS_Terminate(&SDDS_table))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  return EXIT_SUCCESS;
}

char *process_column_definition(char **argv, long argc) {
  char buffer[SDDS_MAXLINE], *ptr;
  long i;

  if (argc < 1)
    return (NULL);
  sprintf(buffer, "&column name=%s, ", argv[0]);
  for (i = 1; i < argc; i++) {
    if (!strchr(argv[i], '='))
      return (NULL);
    strcat(buffer, argv[i]);
    strcat(buffer, ", ");
  }
  if (!strstr(buffer, "type="))
    strcat(buffer, "type=character ");
  strcat(buffer, "&end");
  cp_str(&ptr, buffer);
  return (ptr);
}

long read_CITI_var_list(FILE *fpi, double *var_list, long points) {
  long i;
  char buffer[BUFSIZE];

  i = 0;
  while (fgets(buffer, BUFSIZE, fpi) && i < points) {
    buffer[strlen(buffer) - 1] = 0;
    if (strcmp(buffer, "VAR_LIST_END") == 0)
      break;
    if (!get_double(var_list + i, buffer))
      return (0);
    i++;
  }
  if (i != points)
    return (0);
  return (1);
}

long read_CITI_data(FILE *fpi, double *real_data, double *imag_data, long points) {
  long i;
  char buffer[BUFSIZE];

  i = 0;
  while (fgets(buffer, BUFSIZE, fpi) && i < points) {
    buffer[strlen(buffer) - 1] = 0;
    if (strcmp(buffer, "END") == 0)
      break;
    if (!get_double(real_data + i, buffer) || !get_double(imag_data + i, buffer))
      return (0);
    i++;
  }
  if (i != points)
    return (0);
  return (1);
}

long read_CITI_seg_list(FILE *fpi, double *seg_list, long points) {
  char buffer[BUFSIZE];
  long i;
  double start, end, delta;

  if (!fgets(buffer, BUFSIZE, fpi) || !get_double(&start, buffer) || !get_double(&end, buffer) || !get_long(&i, buffer))
    return (0);
  if (i != points)
    return (0);
  delta = (end - start) / (points - 1);

  if (!fgets(buffer, BUFSIZE, fpi))
    return (0);
  buffer[strlen(buffer) - 1] = 0;
  if (strcmp(buffer, "SEG_LIST_END") != 0)
    return (0);

  for (i = 0; i < points; i++)
    seg_list[i] = start + i * delta;
  return (1);
}

void alter_data_name(char *name) {
  char *ptr;
  if ((ptr = strchr(name, '[')))
    strcpy_ss(ptr, ptr + 1);
  if ((ptr = strchr(name, ']')))
    strcpy_ss(ptr, ptr + 1);
  if ((ptr = strchr(name, ',')))
    strcpy_ss(ptr, ptr + 1);
}
