/**
 * @file tek2sdds.c
 * @brief Converts waveforms from Tektronix ASCII format to SDDS format.
 *
 * @details
 * This program processes Tektronix ASCII files containing waveform data and converts them into 
 * SDDS format for improved interoperability and analysis. It supports defining signal names, 
 * setting descriptions, and specifying data layouts using various options.
 *
 * @section Usage
 * ```
 * tek2sdds <inputfile> <outputfile>
 *          -signalname=<name>
 *         [-description=<text>,<contents>]
 *         [-mpllabels=<title>,<topline>]
 *         [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-signalname`                         | Specifies the name of the signal to use in the SDDS file.                             |
 *
 * | Optional                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-description`                        | Adds a description with specified text and contents to the SDDS output.               |
 * | `-mpllabels`                          | Sets MPL labels such as the title and topline for graphing purposes.                  |
 * | `-majorOrder`                         | Defines the major order for data layout in the SDDS file (row or column).             |
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

#include <stdlib.h>

/* Enumeration for option types */
enum option_type {
  SET_SIGNAL_NAME,
  SET_DESCRIPTION,
  SET_MPL_LABELS,
  SET_MAJOR_ORDER,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "signalname",
  "description",
  "mpllabels",
  "majorOrder",
};

char *USAGE =
  "tek2sdds <inputfile> <outputfile>\n"
  "         -signalname=<name>\n"
  "        [-description=<text>,<contents>]\n"
  "        [-mpllabels=<title>,<topline>]\n"
  "        [-majorOrder=row|column]\n"
  "Options:\n"
  "  -signalname=<name>              (required) Name of the signal\n"
  "  -description=<text>,<contents>  (optional) Description text and contents\n"
  "  -mpllabels=<title>,<topline>    (optional) MPL labels: title and topline\n"
  "  -majorOrder=row|column          (optional) Major order of data\n"
  "\n"
  "This program converts Tektronix ASCII format waveforms to SDDS format.\n"
  "\n"
  "Program by Michael Borland (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

typedef struct
{
  char *TEK_name, *SDDS_name, *value_string;
  long type;
} TEK_PARAMETER;

#define TEK_PARAMETERS 23
TEK_PARAMETER TEK_parameter[TEK_PARAMETERS] = {
  {"ACSTATE", "ACStart", NULL, SDDS_STRING},
  {"NR.PT", "Points", NULL, SDDS_LONG},
  {"WFID", "WaveformID", NULL, SDDS_STRING},
  {"XMULT", "XMultiplier", NULL, SDDS_DOUBLE},
  {"LABEL", "TekLabel", NULL, SDDS_STRING},
  {"TIME", "TimeStamp", NULL, SDDS_STRING},
  {"DATE", "DateStamp", NULL, SDDS_STRING},
  {"TSTIME", "TSTime", NULL, SDDS_DOUBLE},
  {"XINCR", "XIncrement", NULL, SDDS_DOUBLE},
  {"XZERO", "XZero", NULL, SDDS_DOUBLE},
  {"XUNIT", "XUnits", NULL, SDDS_STRING},
  {"YUNIT", "YUnits", NULL, SDDS_STRING},
  {"YZERO", "YZero", NULL, SDDS_DOUBLE},
  {"YMULT", "YMultiplier", NULL, SDDS_DOUBLE},
  {"ENCDG", "Encoding", NULL, SDDS_STRING},
  {"BYT/NR", "TEKBytesPerNumber", NULL, SDDS_LONG},
  {"BYT.OR", "TEKByteOrder", NULL, SDDS_STRING},
  {NULL, NULL, NULL, 0},
};

char *TEK_PreambleString = "WFMPRE ";
char *TEK_DataMarker = "CURVE ";
char *TEK_PointsName = "NR.PT";
char *TEK_XIncrementName = "XINCR";
char *TEK_XZeroName = "XZERO";
char *TEK_XUnitsName = "XUNIT";
char *TEK_YUnitsName = "YUNIT";
char *TEK_YZeroName = "YZERO";
char *TEK_YMultiplierName = "YMULT";
char *TEK_EncodingName = "ENCDG";
char *TEK_BytesPerNumberName = "BYT/NR";
char *TEK_ByteOrderName = "BYT.OR";

#define BUFSIZE 256

long GetNextItem(char *buffer, long bufsize, FILE *fpi);

int main(int argc, char **argv) {
  SDDS_TABLE SDDS_table;
  SCANNED_ARG *scanned;
  long i, i_arg, index, points, bytes_per_number;
  // long lsb_first;
  char *input, *output, buffer[BUFSIZE];
  char *signal_name, *ptr, *parameter_name;
  char *mpl_title, *mpl_topline, *descrip_text, *descrip_contents;
  FILE *fpi;
  long code, binary;
  double xIncrement, xZero, yMultiplier, yZero;
  char *xUnits, *yUnits;
  double *time, *data;
  short columnMajorOrder = 0;
  unsigned long majorOrderFlag;
  size_t bytesRead;

  xUnits = yUnits = NULL;

  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    bomb(NULL, USAGE);
  }

  input = output = signal_name = NULL;
  mpl_title = mpl_topline = descrip_text = descrip_contents = NULL;
  binary = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      delete_chars(scanned[i_arg].list[0], "_");
      /* process options here */
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scanned[i_arg].n_items--;
        if (scanned[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scanned[i_arg].list + 1, &scanned[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_SIGNAL_NAME:
        if (scanned[i_arg].n_items != 2) {
          bomb("invalid -signalname syntax", USAGE);
        }
        signal_name = scanned[i_arg].list[1];
        break;
      case SET_DESCRIPTION:
        if (scanned[i_arg].n_items != 3) {
          bomb("invalid -description syntax", USAGE);
        }
        descrip_text = scanned[i_arg].list[1];
        descrip_contents = scanned[i_arg].list[2];
        break;
      case SET_MPL_LABELS:
        if (scanned[i_arg].n_items != 3) {
          bomb("invalid -mpllabels syntax", USAGE);
        }
        mpl_title = scanned[i_arg].list[1];
        mpl_topline = scanned[i_arg].list[2];
        break;
      default:
        bomb("invalid option seen", USAGE);
        break;
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else
        bomb("too many filenames", USAGE);
    }
  }
  if (!input)
    SDDS_Bomb("input file not seen");
  if (!output)
    SDDS_Bomb("output file not seen");
  if (!signal_name)
    signal_name = "V";

  fpi = fopen_e(input, "r", 0);
  if (fread(buffer, 1, strlen(TEK_PreambleString), fpi) != strlen(TEK_PreambleString) ||
      strncmp(TEK_PreambleString, buffer, strlen(TEK_PreambleString)) != 0)
    SDDS_Bomb("file does not appear to be in Tektronix format");

  parameter_name = buffer;
  while ((code = GetNextItem(buffer, BUFSIZE, fpi)) < 3) {
    if (!(ptr = strchr(buffer, ':')))
      SDDS_Bomb("error parsing input file--missing colon on parameter tag");
    *ptr++ = 0;
    if (strcmp(TEK_DataMarker, parameter_name) == 0)
      break;
    index = 0;
    while (TEK_parameter[index].TEK_name) {
      if (strcmp(TEK_parameter[index].TEK_name, parameter_name) == 0)
        break;
      index++;
    }
    if (!TEK_parameter[index].TEK_name) {
      fprintf(stderr, "warning: parameter %s is not recognized\n", parameter_name);
      continue;
    }
    if (TEK_parameter[index].value_string) {
      fprintf(stderr, "error: duplicate entries for parameter %s\n", parameter_name);
      exit(EXIT_FAILURE);
    }
    SDDS_RemovePadding(ptr);
    SDDS_CopyString(&TEK_parameter[index].value_string, ptr);
    if (code == 2 || code == 3)
      break;
  }
  if (code != 2)
    SDDS_Bomb("unexpected end of file");

  if (fread(buffer, 1, strlen(TEK_DataMarker), fpi) != strlen(TEK_DataMarker) ||
      strncmp(TEK_DataMarker, buffer, strlen(TEK_DataMarker)) != 0)
    SDDS_Bomb("CURVE item missing or not in right place");

  if (!SDDS_InitializeOutput(&SDDS_table, SDDS_BINARY, 0, descrip_text, descrip_contents, output))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  SDDS_table.layout.data_mode.column_major = columnMajorOrder;

  index = 0;
  while (TEK_parameter[index].TEK_name) {
    if (!TEK_parameter[index].value_string) {
      index++;
      continue;
    }
    if (strcmp(TEK_parameter[index].TEK_name, TEK_XIncrementName) == 0) {
      if (sscanf(TEK_parameter[index].value_string, "%lf", &xIncrement) != 1)
        SDDS_Bomb("unable to scan value for x increment");
    } else if (strcmp(TEK_parameter[index].TEK_name, TEK_XZeroName) == 0) {
      if (sscanf(TEK_parameter[index].value_string, "%lf", &xZero) != 1)
        SDDS_Bomb("unable to scan value for x zero");
    } else if (strcmp(TEK_parameter[index].TEK_name, TEK_YZeroName) == 0) {
      if (sscanf(TEK_parameter[index].value_string, "%lf", &yZero) != 1)
        SDDS_Bomb("unable to scan value for y zero");
    } else if (strcmp(TEK_parameter[index].TEK_name, TEK_YMultiplierName) == 0) {
      if (sscanf(TEK_parameter[index].value_string, "%lf", &yMultiplier) != 1)
        SDDS_Bomb("unable to scan value for y multiplier");
    } else if (strcmp(TEK_parameter[index].TEK_name, TEK_XUnitsName) == 0) {
      xUnits = TEK_parameter[index].value_string;
      str_tolower(xUnits);
    } else if (strcmp(TEK_parameter[index].TEK_name, TEK_YUnitsName) == 0) {
      yUnits = TEK_parameter[index].value_string;
      str_tolower(yUnits);
    } else if (strcmp(TEK_parameter[index].TEK_name, TEK_PointsName) == 0) {
      if (sscanf(TEK_parameter[index].value_string, "%ld", &points) != 1)
        SDDS_Bomb("unable to scan value for number of points");
    } else if (strcmp(TEK_parameter[index].TEK_name, TEK_EncodingName) == 0) {
      if (strcmp(TEK_parameter[index].value_string, "ASCII") == 0)
        binary = 0;
      else if (strcmp(TEK_parameter[index].value_string, "BINARY") == 0)
        binary = 1;
      else
        SDDS_Bomb("data encoding is neither ASCII nor BINARY");
    } else if (strcmp(TEK_parameter[index].TEK_name, TEK_BytesPerNumberName) == 0) {
      if (sscanf(TEK_parameter[index].value_string, "%ld", &bytes_per_number) != 1)
        SDDS_Bomb("unable to scan value bytes per number");
    } else if (strcmp(TEK_parameter[index].value_string, TEK_ByteOrderName) == 0) {
      // lsb_first = 1;
      if (strcmp(TEK_parameter[index].value_string, "LSB") != 0) {
        // lsb_first = 0;
      }
    }
    if (SDDS_DefineParameter(&SDDS_table, TEK_parameter[index].SDDS_name, NULL, NULL, TEK_parameter[index].TEK_name,
                             NULL, TEK_parameter[index].type, TEK_parameter[index].value_string) < 0)
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    index++;
  }

  if (mpl_title &&
      (SDDS_DefineParameter(&SDDS_table, "mplTitle", NULL, NULL, NULL, NULL, SDDS_STRING, mpl_title) < 0 ||
       SDDS_DefineParameter(&SDDS_table, "mplTopline", NULL, NULL, NULL, NULL, SDDS_STRING, mpl_topline) < 0))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (SDDS_DefineColumn(&SDDS_table, "t", NULL, xUnits, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_table, signal_name, NULL, yUnits, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      !SDDS_WriteLayout(&SDDS_table) || !SDDS_StartTable(&SDDS_table, points))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  data = tmalloc(sizeof(*data) * points);
  time = tmalloc(sizeof(*time) * points);
  if (!binary) {
    for (i = 0; i < points; i++) {

      if (!(code = GetNextItem(buffer, BUFSIZE, fpi)))
        SDDS_Bomb("insufficient data in input file");
      if (code == 4) {
        points = i;
        break;
      }
      time[i] = xZero + i * xIncrement;
      if (sscanf(buffer, "%lf", data + i) != 1)
        SDDS_Bomb("invalid data in input file");
      data[i] = yZero + data[i] * yMultiplier;
    }
  } else {
    short sdata;
    bytesRead = fread(buffer, sizeof(char), 4, fpi);
    (void)bytesRead;
    for (i = 0; i < points; i++) {
      if (fread(&sdata, sizeof(sdata), 1, fpi) != 1) {
        fprintf(stderr, "file ends unexpectedly\n");
        points = i;
        break;
      }
      time[i] = xZero + i * xIncrement;
      data[i] = sdata;
      data[i] = yZero + data[i] * yMultiplier;
    }
  }

  if (!SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, time, points, "t") ||
      !SDDS_SetColumn(&SDDS_table, SDDS_SET_BY_NAME, data, points, signal_name) ||
      !SDDS_WriteTable(&SDDS_table) ||
      !SDDS_Terminate(&SDDS_table))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  return EXIT_SUCCESS;
}

long GetNextItem(char *buffer, long bufsize, FILE *fpi) {
  long i;
  char c;

  i = c = 0;
  while (i < bufsize && (c = getc(fpi)) != EOF) {
    if (c == ',' || c == ';' || c == '%')
      break;
    buffer[i++] = c;
  }
  if (c == EOF && i == 0)
    return 0;
  buffer[i] = '\0';
  if (c == ',')
    return 1;
  if (c == ';')
    return 2;
  if (c == EOF)
    return 3;
  fprintf(stderr, "warning: invalid data seen--ignoring\n");
  return 4;
}
