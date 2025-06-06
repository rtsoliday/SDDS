/**
 * @file tdms2sdds.c
 * @brief Converts National Instruments' LabVIEW TDMS files to SDDS format.
 *
 * @details
 * This program reads National Instruments' LabVIEW TDMS (Technical Data Management Streaming) files
 * and converts them into SDDS (Self Describing Data Sets) format. It supports customization of output,
 * including ASCII or binary formats, segmentation, and other advanced features for better data management.
 *
 * @section Usage
 * ```
 * tdms2sdds <inputFile> [<outputFile>]
 *           [-pipe=out]
 *           [-ascii | -binary] 
 *           [-numOfSegments] 
 *           [-segment=<integer>]
 * ```
 *
 * @section Options
 * | Optional             | Description                                                             |
 * |----------------------|-------------------------------------------------------------------------|
 * | `-pipe`              | SDDS toolkit pipe option.                                               |
 * | `-ascii`             | Requests SDDS ASCII output (default).                                   |
 * | `-binary`            | Requests SDDS binary output.                                            |
 * | `-numOfSegments`     | Prints the number of TDMS segments in the input file.                   |
 * | `-segment`           | Specifies a particular segment to convert.                              |
 *
 * @subsection Incompatibilities
 * - Only one of the following may be specified:
 *   - `-ascii`
 *   - `-binary`
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

/* Read http://www.ni.com/white-paper/5696/en/ for definitions for the header elements */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#if defined(_WIN32)
#  include <io.h>
#  include <fcntl.h>
typedef signed __int16 int16_t;
typedef signed __int8 int8_t;
typedef unsigned __int16 uint16_t;
typedef signed __int64 int64_t;
#endif

typedef struct
{
  int32_t toc;
  uint32_t version;
  uint64_t next_segment_offset;
  uint64_t raw_data_offset;
} TDMS_LEAD_IN;

typedef struct
{
  char *name;
  int32_t datatype;
  void *value;
} TDMS_META_DATA_OBJECT_PROPERTY;

typedef struct
{
  char *path;
  uint32_t raw_data_index;
  int32_t raw_data_datatype;
  uint32_t raw_data_dimensions;
  uint64_t raw_data_count;
  uint64_t raw_data_total_size;
  uint32_t n_properties;
  TDMS_META_DATA_OBJECT_PROPERTY *property;
} TDMS_META_DATA_OBJECT;

typedef struct
{
  uint32_t n_objects;
  TDMS_META_DATA_OBJECT *object;
} TDMS_META_DATA;

typedef struct
{
  uint32_t n_values;
  double *values;
  char *name;
  int32_t datatype;
} TDMS_RAW_DATA_CHANNEL;

typedef struct
{
  uint32_t n_channels;
  TDMS_RAW_DATA_CHANNEL *channel;
} TDMS_RAW_DATA;

typedef struct
{
  char *name;
  char *unit;
  long double start_time;
  long double start_offset;
  long double increment;
  int32_t samples;
  char *time_pref;
  long double range;
} TDMS_XPART;

typedef struct
{
  TDMS_LEAD_IN lead_in;
  TDMS_META_DATA meta_data;
  TDMS_RAW_DATA raw_data;
  TDMS_XPART xpart;
} TDMS_SEGMENT;

typedef struct
{
  uint32_t n_segments;
  TDMS_SEGMENT *segment; /* segments are like SDDS pages */
  long filesize;
} TDMS_FILE;

void TDMS_ReadLeadIn(FILE *fd, TDMS_SEGMENT *segment);
void TDMS_ReadMetaData(FILE *fd, TDMS_SEGMENT *segment);
void TDMS_ReadRawData(FILE *fd, TDMS_FILE *tdms, uint32_t n_segment, long filesize);
void TDMS_GetValue(FILE *fd, void **value, int32_t datatype);

/* Enumeration for TOC flags */
#define kTocMetaData (1L << 1)
#define kTocNewObjList (1L << 2)
#define kTocRawData (1L << 3)
#define kTocInterleavedData (1L << 5)
#define kTocBigEndian (1L << 6)
#define kTocDAQmxRawData (1L << 7)

/* Enumeration for data types */
#define tdsTypeVoid 0x00000000
#define tdsTypeI8 0x00000001
#define tdsTypeI16 0x00000002
#define tdsTypeI32 0x00000003
#define tdsTypeI64 0x00000004
#define tdsTypeU8 0x00000005
#define tdsTypeU16 0x00000006
#define tdsTypeU32 0x00000007
#define tdsTypeU64 0x00000008
#define tdsTypeSingleFloat 0x00000009
#define tdsTypeDoubleFloat 0x0000000A
#define tdsTypeExtendedFloat 0x0000000B
#define tdsTypeSingleFloatWithUnit 0x00000019
#define tdsTypeDoubleFloatWithUnit 0x0000001A
#define tdsTypeExtendedFloatWithUnit 0x0000001B
#define tdsTypeString 0x00000020
#define tdsTypeBoolean 0x00000021
#define tdsTypeTimeStamp 0x00000044
#define tdsTypeDAQmxRawData 0xFFFFFFFF

/* Enumeration for option types */
enum option_type {
  SET_ASCII,
  SET_BINARY,
  SET_PIPE,
  SET_SEGMENT,
  SET_NUMOFSEGMENTS,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "ascii",
  "binary",
  "pipe",
  "segment",
  "numofsegments"};

/* Improved Usage Message */
char *USAGE =
  "Usage: tdms2sdds <inputFile> [<outputFile>]\n"
  "                 [-pipe=out]\n"
  "                 [-ascii | -binary] \n"
  "                 [-numOfSegments] \n"
  "                 [-segment=<integer>]\n"
  "Options:\n"
  "  -pipe=out           SDDS toolkit pipe option.\n"
  "  -ascii              Requests SDDS ASCII output. Default is binary.\n"
  "  -binary             Requests SDDS BINARY output.\n"
  "  -numOfSegments      Print out the number of TDMS segments.\n"
  "  -segment=<integer>  Select a specific segment to convert.\n\n"
  "Converts National Instruments TDMS files to SDDS.\n"
  "Program by Robert Soliday. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  FILE *fd;
  char *input = NULL, *output = NULL;
  SDDS_DATASET SDDSout;
  SCANNED_ARG *scanned;
  long iArg;
  long ascii = 0;
  unsigned long pipeFlags = 0;
  int i = 0, n, found, ii, jj, kk, segment = 0, querySegments = 0;
  long double timeValue;
  short timeDefined = 0;
  char buffer[1024];

  TDMS_FILE tdms;
  uint64_t rows = 0, j = 0, k;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s", USAGE);
    return (EXIT_FAILURE);
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
      case SET_SEGMENT:
        if (scanned[iArg].n_items < 2) {
          fprintf(stderr, "Error (%s): invalid -segment syntax\n", argv[0]);
          return (EXIT_FAILURE);
        }
        if (sscanf(scanned[iArg].list[1], "%d", &segment) != 1 || segment <= 0) {
          fprintf(stderr, "Error (%s): invalid -segment syntax or value\n", argv[0]);
          return (EXIT_FAILURE);
        }
        break;
      case SET_NUMOFSEGMENTS:
        querySegments = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[iArg].list + 1, scanned[iArg].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "invalid -pipe syntax\n");
          return (EXIT_FAILURE);
        }
        break;
      default:
        fprintf(stderr, "invalid option seen\n");
        fprintf(stderr, "%s", USAGE);
        return (EXIT_FAILURE);
      }
    } else {
      if (!input)
        input = scanned[iArg].list[0];
      else if (!output)
        output = scanned[iArg].list[0];
      else {
        fprintf(stderr, "too many filenames\n");
        fprintf(stderr, "%s", USAGE);
        return (EXIT_FAILURE);
      }
    }
  }
  if (!querySegments) {
    processFilenames("tdms2sdds", &input, &output, pipeFlags, 0, NULL);
  }

  if (input) {
    if (!fexists(input)) {
      fprintf(stderr, "input file not found\n");
      return (EXIT_FAILURE);
    }
    if (!(fd = fopen(input, "rb"))) {
      fprintf(stderr, "problem opening input file\n");
      return (EXIT_FAILURE);
    }
  } else {
    fprintf(stderr, "tdms2sdds cannot -pipe=in tdms files\n");
    return (EXIT_FAILURE);
  }

  fseek(fd, 0L, SEEK_END);
  tdms.filesize = ftell(fd);
  fseek(fd, 0L, SEEK_SET);

  tdms.n_segments = 0;
  while (ftell(fd) < tdms.filesize) {
    tdms.n_segments++;
    if (tdms.n_segments == 1) {
      tdms.segment = malloc(sizeof(TDMS_SEGMENT));
    } else {
      tdms.segment = realloc(tdms.segment, sizeof(TDMS_SEGMENT) * tdms.n_segments);
    }
    /* Read Lead In */
    TDMS_ReadLeadIn(fd, &(tdms.segment[tdms.n_segments - 1]));
    if (tdms.segment[tdms.n_segments - 1].lead_in.toc & kTocBigEndian) {
      if (!SDDS_IsBigEndianMachine()) {
        fprintf(stderr, "tdms2sdds does not yet support reading from non-native endian TDMS files.\n");
        return (EXIT_FAILURE);
      }
    } else {
      if (SDDS_IsBigEndianMachine()) {
        fprintf(stderr, "tdms2sdds does not yet support reading from non-native endian TDMS files.\n");
        return (EXIT_FAILURE);
      }
    }
    /* Read Meta Data */
    if (tdms.segment[tdms.n_segments - 1].lead_in.toc & kTocMetaData) {
      TDMS_ReadMetaData(fd, &(tdms.segment[tdms.n_segments - 1]));
    }
    /* Read Raw Data */
    if (tdms.segment[tdms.n_segments - 1].lead_in.toc & kTocRawData) {
      TDMS_ReadRawData(fd, &tdms, tdms.n_segments - 1, tdms.filesize);
    }
  }
  fclose(fd);

  if (querySegments) {
    fprintf(stdout, "Number of segments: %u\n", tdms.n_segments);
    return (EXIT_SUCCESS);
  }
  if (!SDDS_InitializeOutput(&SDDSout, ascii ? SDDS_ASCII : SDDS_BINARY, 1, NULL, NULL, output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (EXIT_FAILURE);
  }
  if (input) {
    if (SDDS_DefineParameter(&SDDSout, "TDMSfile", NULL, NULL, NULL, NULL, SDDS_STRING, input) == -1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (EXIT_FAILURE);
    }
  }
  if (segment > tdms.n_segments) {
    fprintf(stderr, "tdms2sdds: Error: segment selected does not exist\n");
    return (EXIT_FAILURE);
  } else {
    segment--;
  }
  for (i = 0; i < tdms.n_segments; i++) {
    if ((segment != -1) && (segment != i)) {
      continue;
    }
    for (j = 0; j < tdms.segment[i].raw_data.n_channels; j++) {
      if (j == 0) {
        rows = tdms.segment[i].raw_data.channel[j].n_values;
      } else {
        if (rows != tdms.segment[i].raw_data.channel[j].n_values) {
          fprintf(stderr, "tdms2sdds: Error: channels in the same TDMS segment have different lengths which is not allowed in SDDS\n");
          return (EXIT_FAILURE);
        }
      }
    }
    k = 0;
    for (j = 0; j < tdms.segment[i].meta_data.n_objects; j++) {
      found = 0;
      for (n = 0; n < tdms.segment[i].meta_data.object[j].n_properties; n++) {
        if (strcmp(tdms.segment[i].meta_data.object[j].property[n].name, "NI_ChannelName") == 0) {
          found = 1;
          strcpy(buffer, tdms.segment[i].meta_data.object[j].property[n].value);
          break;
        }
      }
      if (!found) {
        strcpy(buffer, tdms.segment[i].meta_data.object[j].path);
      }
      if (!edit_string(buffer, "1Z/%g/ /_/%ga/a_a%g/\'//%g/(/[/%g/)/]/%g/=/_eq_/")) {
        fprintf(stderr, "tdms2sdds: Error: problem editing column label\n");
        return (EXIT_FAILURE);
      }
      /* Check for channel name that starts with a number and prepend a semicolon */
      if (!(isalpha(*buffer) || strchr(".:", *buffer))) {
        if (!edit_string(buffer, "i/:/")) {
          fprintf(stderr, "tdms2sdds: Error: problem editing column label\n");
          return (EXIT_FAILURE);
        }
      }
      strcpy(tdms.segment[i].meta_data.object[j].path, buffer);
      if (tdms.segment[i].meta_data.object[j].raw_data_index != 0xFFFFFFFF) {
        tdms.segment[i].raw_data.channel[k].name = tdms.segment[i].meta_data.object[j].path;
        tdms.segment[i].raw_data.channel[k].datatype = tdms.segment[i].meta_data.object[j].raw_data_datatype;
      }
      if (((segment == -1) && (i == 0)) || (segment != -1)) {
        if (tdms.segment[i].meta_data.object[j].raw_data_index != 0xFFFFFFFF) {
          if (timeDefined == 0) {
            if (tdms.segment[i].xpart.samples > 0) {
              if (tdms.segment[i].xpart.name == NULL) {
                tdms.segment[i].xpart.name = malloc(5);
                sprintf(tdms.segment[i].xpart.name, "Time");
              }
              if (!SDDS_DefineSimpleColumn(&SDDSout, tdms.segment[i].xpart.name, tdms.segment[i].xpart.unit, SDDS_DOUBLE)) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                return (EXIT_FAILURE);
              }
              timeDefined = 1;
            }
          }
          if ((tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeBoolean) ||
              (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeU8) ||
              (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeU16)) {
            if (!SDDS_DefineSimpleColumn(&SDDSout, tdms.segment[i].meta_data.object[j].path, NULL, SDDS_USHORT)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if ((tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeI8) ||
                     (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeI16)) {
            if (!SDDS_DefineSimpleColumn(&SDDSout, tdms.segment[i].meta_data.object[j].path, NULL, SDDS_SHORT)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeI32) {
            if (!SDDS_DefineSimpleColumn(&SDDSout, tdms.segment[i].meta_data.object[j].path, NULL, SDDS_LONG)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeU32) {
            if (!SDDS_DefineSimpleColumn(&SDDSout, tdms.segment[i].meta_data.object[j].path, NULL, SDDS_ULONG)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeI64) {
            if (!SDDS_DefineSimpleColumn(&SDDSout, tdms.segment[i].meta_data.object[j].path, NULL, SDDS_LONG64)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeU64) {
            if (!SDDS_DefineSimpleColumn(&SDDSout, tdms.segment[i].meta_data.object[j].path, NULL, SDDS_ULONG64)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if ((tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeSingleFloat) ||
                     (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeSingleFloatWithUnit)) {
            if (!SDDS_DefineSimpleColumn(&SDDSout, tdms.segment[i].meta_data.object[j].path, NULL, SDDS_FLOAT)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if ((tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeDoubleFloat) ||
                     (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeDoubleFloatWithUnit) ||
                     (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeTimeStamp)) {
            if (!SDDS_DefineSimpleColumn(&SDDSout, tdms.segment[i].meta_data.object[j].path, NULL, SDDS_DOUBLE)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeString) {
            fprintf(stderr, "tdms2sdds: string type channels are not yet supported in tdms2sdds\n");
            return (EXIT_FAILURE);
          } else if (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeVoid) {
            fprintf(stderr, "tdms2sdds: void type channels are not yet supported in tdms2sdds\n");
            return (EXIT_FAILURE);
          } else if (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeExtendedFloat) {
            fprintf(stderr, "tdms2sdds: extended float type channels are not yet supported in tdms2sdds\n");
            return (EXIT_FAILURE);
          } else if (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeExtendedFloatWithUnit) {
            fprintf(stderr, "tdms2sdds: extended float with unit type channels are not yet supported in tdms2sdds\n");
            return (EXIT_FAILURE);
          } else if (tdms.segment[i].meta_data.object[j].raw_data_datatype == tdsTypeDAQmxRawData) {
            fprintf(stderr, "tdms2sdds: DAQmx raw data channels are not yet supported in tdms2sdds\n");
            return (EXIT_FAILURE);
          } else {
            fprintf(stderr, "tdms2sdds: unknown data type\n");
            return (EXIT_FAILURE);
          }
        }
        for (n = 0; n < tdms.segment[i].meta_data.object[j].n_properties; n++) {
          if (strcmp(tdms.segment[i].meta_data.object[j].property[n].name, "name") == 0) {
            continue;
          }
          strcpy(buffer, tdms.segment[i].meta_data.object[j].property[n].name);
          if (!edit_string(buffer, "%g/ /_/%g/\'//%g/(/[/%g/)/]/%g/=/_eq_/")) {
            fprintf(stderr, "tdms2sdds: Error: problem editing column label\n");
            return (EXIT_FAILURE);
          }
          /* Check for channel name that starts with a number and prepend a semicolon */
          if (!(isalpha(*buffer) || strchr(".:", *buffer))) {
            if (!edit_string(buffer, "i/:/")) {
              fprintf(stderr, "tdms2sdds: Error: problem editing column label\n");
              return (EXIT_FAILURE);
            }
          }
          strcpy(tdms.segment[i].meta_data.object[j].property[n].name, buffer);

          /* Check if additional channels have the same property names. Only include the first */
          if (SDDS_GetParameterIndex(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name) != -1) {
            SDDS_ClearErrors();
            continue;
          }
          if ((tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeBoolean) ||
              (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeU8) ||
              (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeU16)) {
            if (!SDDS_DefineSimpleParameter(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name, NULL, SDDS_USHORT)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if ((tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeI8) ||
                     (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeI16)) {
            if (!SDDS_DefineSimpleParameter(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name, NULL, SDDS_SHORT)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeI32) {
            if (!SDDS_DefineSimpleParameter(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name, NULL, SDDS_LONG)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeU32) {
            if (!SDDS_DefineSimpleParameter(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name, NULL, SDDS_ULONG)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeI64) {
            if (!SDDS_DefineSimpleParameter(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name, NULL, SDDS_LONG64)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeU64) {
            if (!SDDS_DefineSimpleParameter(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name, NULL, SDDS_ULONG64)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if ((tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeSingleFloat) ||
                     (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeSingleFloatWithUnit)) {
            if (!SDDS_DefineSimpleParameter(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name, NULL, SDDS_FLOAT)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if ((tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeDoubleFloat) ||
                     (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeDoubleFloatWithUnit) ||
                     (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeTimeStamp)) {
            if (!SDDS_DefineSimpleParameter(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name, NULL, SDDS_DOUBLE)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeString) {
            if (!SDDS_DefineSimpleParameter(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name, NULL, SDDS_STRING)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (EXIT_FAILURE);
            }
          } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeVoid) {
            fprintf(stderr, "tdms2sdds: void type parameters are not yet supported in tdms2sdds\n");
            return (EXIT_FAILURE);
          } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeExtendedFloat) {
            fprintf(stderr, "tdms2sdds: extended float type parameters are not yet supported in tdms2sdds\n");
            return (EXIT_FAILURE);
          } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeExtendedFloatWithUnit) {
            fprintf(stderr, "tdms2sdds: extended float with unit type parameters are not yet supported in tdms2sdds\n");
            return (EXIT_FAILURE);
          } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeDAQmxRawData) {
            fprintf(stderr, "tdms2sdds: DAQmx raw data parameters are not yet supported in tdms2sdds\n");
            return (EXIT_FAILURE);
          } else {
            fprintf(stderr, "tdms2sdds: unknown data type\n");
            return (EXIT_FAILURE);
          }
        }
      }
      if (tdms.segment[i].meta_data.object[j].raw_data_index != 0xFFFFFFFF) {
        k++;
      }
    }
    if (((segment == -1) && (i == 0)) || (segment != -1)) {
      if (!SDDS_WriteLayout(&SDDSout)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (EXIT_FAILURE);
      }
    }
    if (!SDDS_StartTable(&SDDSout, rows)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (EXIT_FAILURE);
    }

    if ((tdms.segment[i].xpart.name != NULL) &&
        (tdms.segment[i].xpart.samples > 0) &&
        (tdms.segment[i].raw_data.n_channels > 0)) {
      timeValue = tdms.segment[i].xpart.start_time + tdms.segment[i].xpart.start_offset;
      for (j = 0; j < rows; j++) {
        if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, j, tdms.segment[i].xpart.name, (double)timeValue, NULL)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (EXIT_FAILURE);
        }
        timeValue += tdms.segment[i].xpart.increment;
      }
    }
    for (j = 0; j < tdms.segment[i].raw_data.n_channels; j++) {
      if ((tdms.segment[i].raw_data.channel[j].datatype == tdsTypeI16) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeU16) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeI32) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeU32) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeI64) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeU64) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeSingleFloat) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeSingleFloatWithUnit) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeDoubleFloat) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeDoubleFloatWithUnit) ||
          (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeTimeStamp)) {
        if (!SDDS_SetColumn(&SDDSout, SDDS_SET_BY_NAME, tdms.segment[i].raw_data.channel[j].values,
                            tdms.segment[i].raw_data.channel[j].n_values, tdms.segment[i].raw_data.channel[j].name)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (EXIT_FAILURE);
        }
      } else if ((tdms.segment[i].raw_data.channel[j].datatype == tdsTypeBoolean) ||
                 (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeI8)) {
        for (k = 0; k < rows; k++) {
          if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, k,
                                 tdms.segment[i].raw_data.channel[j].name,
                                 (int16_t)(((int8_t *)(tdms.segment[i].raw_data.channel[j].values))[k]), NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (EXIT_FAILURE);
          }
        }
      } else if (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeU8) {
        for (k = 0; k < rows; k++) {
          if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, k,
                                 tdms.segment[i].raw_data.channel[j].name,
                                 (uint16_t)(((uint8_t *)(tdms.segment[i].raw_data.channel[j].values))[k]), NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (EXIT_FAILURE);
          }
        }
      } else if (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeString) {
        fprintf(stderr, "tdms2sdds: string type channels are not yet supported in tdms2sdds\n");
        return (EXIT_FAILURE);
      } else if (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeVoid) {
        fprintf(stderr, "tdms2sdds: void type channels are not yet supported in tdms2sdds\n");
        return (EXIT_FAILURE);
      } else if (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeExtendedFloat) {
        fprintf(stderr, "tdms2sdds: extended float type channels are not yet supported in tdms2sdds\n");
        return (EXIT_FAILURE);
      } else if (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeExtendedFloatWithUnit) {
        fprintf(stderr, "tdms2sdds: extended float with unit type channels are not yet supported in tdms2sdds\n");
        return (EXIT_FAILURE);
      } else if (tdms.segment[i].raw_data.channel[j].datatype == tdsTypeDAQmxRawData) {
        fprintf(stderr, "tdms2sdds: DAQmx raw data channels are not yet supported in tdms2sdds\n");
        return (EXIT_FAILURE);
      } else {
        fprintf(stderr, "tdms2sdds: unknown data type\n");
        return (EXIT_FAILURE);
      }
    }

    for (j = 0; j < tdms.segment[i].meta_data.n_objects; j++) {
      for (n = 0; n < tdms.segment[i].meta_data.object[j].n_properties; n++) {
        /* Do not write parameter if it wasn't defined in the first segment */
        if (SDDS_GetParameterIndex(&SDDSout, tdms.segment[i].meta_data.object[j].property[n].name) == -1) {
          SDDS_ClearErrors();
          continue;
        }
        if ((tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeI16) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeU16) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeI32) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeU32) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeI64) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeU64) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeSingleFloat) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeSingleFloatWithUnit) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeDoubleFloat) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeDoubleFloatWithUnit) ||
            (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeTimeStamp)) {
          if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE,
                                  tdms.segment[i].meta_data.object[j].property[n].name,
                                  tdms.segment[i].meta_data.object[j].property[n].value, NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (EXIT_FAILURE);
          }
        } else if ((tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeBoolean) ||
                   (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeI8)) {
          if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                  tdms.segment[i].meta_data.object[j].property[n].name,
                                  (int16_t)(*(int8_t *)(tdms.segment[i].meta_data.object[j].property[n].value)), NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (EXIT_FAILURE);
          }
        } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeU8) {
          if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                  tdms.segment[i].meta_data.object[j].property[n].name,
                                  (uint16_t)(*(uint8_t *)(tdms.segment[i].meta_data.object[j].property[n].value)), NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (EXIT_FAILURE);
          }
        } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeString) {
          if (!SDDS_SetParameters(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                  tdms.segment[i].meta_data.object[j].property[n].name,
                                  tdms.segment[i].meta_data.object[j].property[n].value, NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (EXIT_FAILURE);
          }
        } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeVoid) {
          fprintf(stderr, "tdms2sdds: void type parameters are not yet supported in tdms2sdds\n");
          return (EXIT_FAILURE);
        } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeExtendedFloat) {
          fprintf(stderr, "tdms2sdds: extended float type parameters are not yet supported in tdms2sdds\n");
          return (EXIT_FAILURE);
        } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeExtendedFloatWithUnit) {
          fprintf(stderr, "tdms2sdds: extended float with unit type parameters are not yet supported in tdms2sdds\n");
          return (EXIT_FAILURE);
        } else if (tdms.segment[i].meta_data.object[j].property[n].datatype == tdsTypeDAQmxRawData) {
          fprintf(stderr, "tdms2sdds: DAQmx raw data parameters are not yet supported in tdms2sdds\n");
          return (EXIT_FAILURE);
        } else {
          fprintf(stderr, "tdms2sdds: unknown data type\n");
          return (EXIT_FAILURE);
        }
      }
    }
    if (!SDDS_WriteTable(&SDDSout)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (EXIT_FAILURE);
    }
  }
  if (!SDDS_Terminate(&SDDSout)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (EXIT_FAILURE);
  }

  if (tdms.n_segments > 0) {
    for (i = 0; i < tdms.n_segments; i++) {
      if (tdms.segment[i].raw_data.n_channels > 0) {
        for (j = 0; j < tdms.segment[i].raw_data.n_channels; j++) {
          if (tdms.segment[i].raw_data.channel[j].n_values > 0) {
            free(tdms.segment[i].raw_data.channel[j].values);
          }
        }
        free(tdms.segment[i].raw_data.channel);
      }
      if (tdms.segment[i].meta_data.n_objects > 0) {
        for (j = 0; j < tdms.segment[i].meta_data.n_objects; j++) {
          if (tdms.segment[i].meta_data.object[j].path) {
            free(tdms.segment[i].meta_data.object[j].path);
          }
          if (tdms.segment[i].meta_data.object[j].n_properties > 0) {
            for (k = 0; k < tdms.segment[i].meta_data.object[j].n_properties; k++) {
              if (tdms.segment[i].meta_data.object[j].property[k].name) {
                for (ii = i + 1; ii < tdms.n_segments; ii++) {
                  for (jj = 0; jj < tdms.segment[ii].meta_data.n_objects; jj++) {
                    for (kk = 0; kk < tdms.segment[ii].meta_data.object[jj].n_properties; kk++) {
                      if (tdms.segment[i].meta_data.object[j].property[k].name == tdms.segment[ii].meta_data.object[jj].property[kk].name) {
                        tdms.segment[ii].meta_data.object[jj].property[kk].name = NULL;
                        tdms.segment[ii].meta_data.object[jj].property[kk].value = NULL;
                      }
                    }
                  }
                }
                free(tdms.segment[i].meta_data.object[j].property[k].name);
                tdms.segment[i].meta_data.object[j].property[k].name = NULL;
              }
              if (tdms.segment[i].meta_data.object[j].property[k].value) {
                free(tdms.segment[i].meta_data.object[j].property[k].value);
                tdms.segment[i].meta_data.object[j].property[k].value = NULL;
              }
            }
            free(tdms.segment[i].meta_data.object[j].property);
          }
        }
        free(tdms.segment[i].meta_data.object);
      }
    }
    free(tdms.segment);
  }

  free_scanargs(&scanned, argc);

  return (EXIT_SUCCESS);
}

void TDMS_ReadLeadIn(FILE *fd, TDMS_SEGMENT *segment) {
  char buffer[50];
  size_t bytesRead;

  bytesRead = fread(buffer, 1, 4, fd);
  (void)bytesRead;
  buffer[4] = '\0';
  if (strcmp(buffer, "TDSm") != 0) {
    fprintf(stderr, "tdms2sdds: Error: File does not start with TDSm\n");
    exit(EXIT_FAILURE);
  }
  bytesRead = fread(&(segment->lead_in.toc), 1, 4, fd);     /* TOC */
  (void)bytesRead;
  bytesRead = fread(&(segment->lead_in.version), 1, 4, fd); /* Version */
  (void)bytesRead;
  if (segment->lead_in.version == 4712) {
    fprintf(stderr, "tdms2sdds: Error: TDMS version 1.0 files unsupported\n");
    exit(EXIT_FAILURE);
  }
  if (segment->lead_in.version != 4713) {
    fprintf(stderr, "tdms2sdds: Error: Unknown TDMS version\n");
    exit(EXIT_FAILURE);
  }
  bytesRead = fread(&(segment->lead_in.next_segment_offset), 1, 8, fd); /* Next segment offset */
  (void)bytesRead;
  bytesRead = fread(&(segment->lead_in.raw_data_offset), 1, 8, fd);     /* Raw data offset */
  (void)bytesRead;
}

void TDMS_ReadMetaData(FILE *fd, TDMS_SEGMENT *segment) {
  uint32_t uLong, i, j;
  size_t bytesRead;

  segment->xpart.name = NULL;
  segment->xpart.unit = NULL;
  segment->xpart.start_time = 0;
  segment->xpart.start_offset = 0;
  segment->xpart.increment = 0;
  segment->xpart.samples = 0;
  segment->xpart.time_pref = NULL;
  segment->xpart.range = 0;

  bytesRead = fread(&(segment->meta_data.n_objects), 1, 4, fd);
  (void)bytesRead;

  segment->meta_data.object = malloc(sizeof(TDMS_META_DATA_OBJECT) * segment->meta_data.n_objects);
  for (i = 0; i < segment->meta_data.n_objects; i++) {
    bytesRead = fread(&uLong, 1, 4, fd);
    (void)bytesRead;
    segment->meta_data.object[i].path = malloc(sizeof(char) * (uLong + 1));
    bytesRead = fread(segment->meta_data.object[i].path, 1, uLong, fd);
    (void)bytesRead;
    segment->meta_data.object[i].path[uLong] = '\0';
    bytesRead = fread(&(segment->meta_data.object[i].raw_data_index), 1, 4, fd);
    (void)bytesRead;
    if ((segment->meta_data.object[i].raw_data_index != 0xFFFFFFFF) && (segment->meta_data.object[i].raw_data_index != 0x00000000)) {
      bytesRead = fread(&(segment->meta_data.object[i].raw_data_datatype), 1, 4, fd);
      (void)bytesRead;
      bytesRead = fread(&(segment->meta_data.object[i].raw_data_dimensions), 1, 4, fd);
      (void)bytesRead;
      bytesRead = fread(&(segment->meta_data.object[i].raw_data_count), 1, 8, fd);
      (void)bytesRead;
      if (segment->meta_data.object[i].raw_data_datatype == tdsTypeString) {
        bytesRead = fread(&(segment->meta_data.object[i].raw_data_total_size), 1, 8, fd);
        (void)bytesRead;
      }
    }
    bytesRead = fread(&(segment->meta_data.object[i].n_properties), 1, 4, fd);
    (void)bytesRead;
    segment->meta_data.object[i].property = malloc(sizeof(TDMS_META_DATA_OBJECT_PROPERTY) * segment->meta_data.object[i].n_properties);
    for (j = 0; j < segment->meta_data.object[i].n_properties; j++) {
      bytesRead = fread(&uLong, 1, 4, fd);
      (void)bytesRead;
      segment->meta_data.object[i].property[j].name = malloc(sizeof(char) * (uLong + 1));
      bytesRead = fread(segment->meta_data.object[i].property[j].name, 1, uLong, fd);
      (void)bytesRead;
      segment->meta_data.object[i].property[j].name[uLong] = '\0';
      bytesRead = fread(&(segment->meta_data.object[i].property[j].datatype), 1, 4, fd);
      (void)bytesRead;
      if (segment->meta_data.object[i].property[j].datatype == tdsTypeString) {
        bytesRead = fread(&uLong, 1, 4, fd);
        (void)bytesRead;
        segment->meta_data.object[i].property[j].value = malloc(sizeof(char) * (uLong + 1));
        bytesRead = fread(segment->meta_data.object[i].property[j].value, 1, uLong, fd);
        (void)bytesRead;
        ((char *)(segment->meta_data.object[i].property[j].value))[uLong] = '\0';
      } else {
        TDMS_GetValue(fd, &(segment->meta_data.object[i].property[j].value), segment->meta_data.object[i].property[j].datatype);
      }
      /* check for waveform channel properties */
      if (strcmp(segment->meta_data.object[i].property[j].name, "wf_xname") == 0) {
        segment->xpart.name = segment->meta_data.object[i].property[j].value;
      }
      if (strcmp(segment->meta_data.object[i].property[j].name, "wf_xunit_string") == 0) {
        segment->xpart.unit = segment->meta_data.object[i].property[j].value;
      }
      if (strcmp(segment->meta_data.object[i].property[j].name, "wf_start_time") == 0) {
        segment->xpart.start_time = *(double *)(segment->meta_data.object[i].property[j].value);
      }
      if (strcmp(segment->meta_data.object[i].property[j].name, "wf_start_offset") == 0) {
        segment->xpart.start_offset = *(double *)(segment->meta_data.object[i].property[j].value);
      }
      if (strcmp(segment->meta_data.object[i].property[j].name, "wf_increment") == 0) {
        segment->xpart.increment = *(double *)(segment->meta_data.object[i].property[j].value);
      }
      if (strcmp(segment->meta_data.object[i].property[j].name, "wf_samples") == 0) {
        segment->xpart.samples = *(int32_t *)(segment->meta_data.object[i].property[j].value);
      }
      if (strcmp(segment->meta_data.object[i].property[j].name, "wf_time_pref") == 0) {
        segment->xpart.time_pref = segment->meta_data.object[i].property[j].value;
      }
    }
  }
}

void TDMS_ReadRawData(FILE *fd, TDMS_FILE *tdms, uint32_t n_segment, long filesize) {
  uint32_t i, j, k, n, p, a;
  uint32_t channel_size;
  uint32_t chunk_size = 0;
  uint32_t total_chunks;
  uint32_t n_chunks;
  uint32_t chunk;
  int64_t LLong;
  uint64_t uLLong;
  int prevFound = 0;
  TDMS_SEGMENT *segment = &((*tdms).segment[n_segment]);
  size_t bytesRead;

  segment->raw_data.n_channels = 0;
  for (i = 0; i < segment->meta_data.n_objects; i++) {
    if (segment->meta_data.object[i].raw_data_index != 0xFFFFFFFF) {
      if (segment->meta_data.object[i].raw_data_index == 0x00000000) {
        /* An object with this name already exists in the previous segment.
           Copy over meta data */
        prevFound = 0;
        a = 0;
        while (prevFound == 0) {
          a++;
          if (a > n_segment) {
            fprintf(stderr, "tdms2sdds: Error: unable to find %s in a previous segment.\n", segment->meta_data.object[i].path);
            exit(EXIT_FAILURE);
          }
          for (n = 0; n < (*tdms).segment[n_segment - a].meta_data.n_objects; n++) {
            if (strcmp((*tdms).segment[n_segment - a].meta_data.object[n].path, segment->meta_data.object[i].path) == 0) {
              prevFound = 1;
              segment->meta_data.object[i].raw_data_datatype = (*tdms).segment[n_segment - a].meta_data.object[n].raw_data_datatype;
              segment->meta_data.object[i].raw_data_dimensions = (*tdms).segment[n_segment - a].meta_data.object[n].raw_data_dimensions;
              segment->meta_data.object[i].raw_data_count = (*tdms).segment[n_segment - a].meta_data.object[n].raw_data_count;
              if (segment->meta_data.object[i].raw_data_datatype == tdsTypeString) {
                segment->meta_data.object[i].raw_data_total_size = (*tdms).segment[n_segment - a].meta_data.object[n].raw_data_total_size;
              }

              /* Copy over previous properties excluding any that were overwritten */
              /* There will be errors if new properties are inserted after the first segment in the TDMS file */
              for (j = 0; j < (*tdms).segment[n_segment - a].meta_data.object[n].n_properties; j++) {
                for (k = 0; k < segment->meta_data.object[i].n_properties; k++) {
                  if (strcmp((*tdms).segment[n_segment - a].meta_data.object[n].property[j].name, segment->meta_data.object[i].property[k].name) == 0) {
                    break;
                  }
                }
                p = segment->meta_data.object[i].n_properties;
                segment->meta_data.object[i].n_properties++;
                segment->meta_data.object[i].property = realloc(segment->meta_data.object[i].property, sizeof(TDMS_META_DATA_OBJECT_PROPERTY) * segment->meta_data.object[i].n_properties);
                segment->meta_data.object[i].property[p].name = (*tdms).segment[n_segment - a].meta_data.object[n].property[j].name;
                segment->meta_data.object[i].property[p].datatype = (*tdms).segment[n_segment - a].meta_data.object[n].property[j].datatype;
                segment->meta_data.object[i].property[p].value = (*tdms).segment[n_segment - a].meta_data.object[n].property[j].value;
                /* check for waveform channel properties */
                if (strcmp(segment->meta_data.object[i].property[p].name, "wf_xname") == 0) {
                  segment->xpart.name = segment->meta_data.object[i].property[p].value;
                }
                if (strcmp(segment->meta_data.object[i].property[p].name, "wf_xunit_string") == 0) {
                  segment->xpart.unit = segment->meta_data.object[i].property[p].value;
                }
                if (strcmp(segment->meta_data.object[i].property[p].name, "wf_start_time") == 0) {
                  segment->xpart.start_time = *(double *)(segment->meta_data.object[i].property[p].value);
                }
                if (strcmp(segment->meta_data.object[i].property[p].name, "wf_start_offset") == 0) {
                  segment->xpart.start_offset = *(double *)(segment->meta_data.object[i].property[p].value);
                  segment->xpart.start_offset += (*tdms).segment[n_segment - a].xpart.range;
                  /*put code here */
                }
                if (strcmp(segment->meta_data.object[i].property[p].name, "wf_increment") == 0) {
                  segment->xpart.increment = *(double *)(segment->meta_data.object[i].property[p].value);
                }
                if (strcmp(segment->meta_data.object[i].property[p].name, "wf_samples") == 0) {
                  segment->xpart.samples = *(int32_t *)(segment->meta_data.object[i].property[p].value);
                }
                if (strcmp(segment->meta_data.object[i].property[p].name, "wf_time_pref") == 0) {
                  segment->xpart.time_pref = segment->meta_data.object[i].property[p].value;
                }
              }
            }
          }
        }
      }

      segment->raw_data.n_channels++;
      if (segment->meta_data.object[i].raw_data_dimensions != 1) {
        fprintf(stderr, "tdms2sdds: Error: raw data dimension is %d and should have been 1.\n", segment->meta_data.object[i].raw_data_dimensions);
        exit(EXIT_FAILURE);
      }
      if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeI8) ||
          (segment->meta_data.object[i].raw_data_datatype == tdsTypeU8) ||
          (segment->meta_data.object[i].raw_data_datatype == tdsTypeBoolean)) {
        channel_size = 1 * segment->meta_data.object[i].raw_data_dimensions * segment->meta_data.object[i].raw_data_count;
      } else if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeI16) ||
                 (segment->meta_data.object[i].raw_data_datatype == tdsTypeU16)) {
        channel_size = 2 * segment->meta_data.object[i].raw_data_dimensions * segment->meta_data.object[i].raw_data_count;
      } else if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeI32) ||
                 (segment->meta_data.object[i].raw_data_datatype == tdsTypeU32) ||
                 (segment->meta_data.object[i].raw_data_datatype == tdsTypeSingleFloat) ||
                 (segment->meta_data.object[i].raw_data_datatype == tdsTypeSingleFloatWithUnit)) {
        channel_size = 4 * segment->meta_data.object[i].raw_data_dimensions * segment->meta_data.object[i].raw_data_count;
      } else if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeI64) ||
                 (segment->meta_data.object[i].raw_data_datatype == tdsTypeU64) ||
                 (segment->meta_data.object[i].raw_data_datatype == tdsTypeDoubleFloat) ||
                 (segment->meta_data.object[i].raw_data_datatype == tdsTypeDoubleFloatWithUnit)) {
        channel_size = 8 * segment->meta_data.object[i].raw_data_dimensions * segment->meta_data.object[i].raw_data_count;
      } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeTimeStamp) {
        channel_size = 16 * segment->meta_data.object[i].raw_data_dimensions * segment->meta_data.object[i].raw_data_count;
      } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeString) {
        fprintf(stderr, "tdms2sdds: string type channels are not yet supported in tdms2sdds\n");
        exit(EXIT_FAILURE);
      } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeVoid) {
        fprintf(stderr, "tdms2sdds: void type channels are not yet supported in tdms2sdds\n");
        exit(EXIT_FAILURE);
      } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeExtendedFloat) {
        fprintf(stderr, "tdms2sdds: extended float type channels are not yet supported in tdms2sdds\n");
        exit(EXIT_FAILURE);
      } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeExtendedFloatWithUnit) {
        fprintf(stderr, "tdms2sdds: extended float type with unit channels are not yet supported in tdms2sdds\n");
        exit(EXIT_FAILURE);
      } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeDAQmxRawData) {
        fprintf(stderr, "tdms2sdds: extended DAQmx raw data channels are not yet supported in tdms2sdds\n");
        exit(EXIT_FAILURE);
      } else {
        fprintf(stderr, "tdms2sdds: unknown data type\n");
        exit(EXIT_FAILURE);
      }
      chunk_size += channel_size;
    }
  }

  if (segment->lead_in.next_segment_offset == -1) {
    if (filesize == -1) {
      fprintf(stderr, "tdms2sdds: Error: cannot calculate file size because you are using stdin\n");
      exit(EXIT_FAILURE);
    }
    segment->lead_in.next_segment_offset = filesize;
  }
  total_chunks = segment->lead_in.next_segment_offset - segment->lead_in.raw_data_offset;
  n_chunks = total_chunks / chunk_size;

  segment->raw_data.channel = malloc(sizeof(TDMS_RAW_DATA_CHANNEL) * segment->raw_data.n_channels);

  if (segment->lead_in.toc & kTocInterleavedData) {
    fprintf(stderr, "tdms2sdds does not yet support interleaved data\n");
    exit(EXIT_FAILURE);
  } else {
    j = 0;
    for (i = 0; i < segment->meta_data.n_objects; i++) {
      if (segment->meta_data.object[i].raw_data_index != 0xFFFFFFFF) {
        segment->xpart.range = segment->xpart.increment * segment->meta_data.object[i].raw_data_count * n_chunks;

        if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeI8) ||
            (segment->meta_data.object[i].raw_data_datatype == tdsTypeU8) ||
            (segment->meta_data.object[i].raw_data_datatype == tdsTypeBoolean)) {
          segment->raw_data.channel[j].values = malloc(1 * segment->meta_data.object[i].raw_data_count * n_chunks);
        } else if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeI16) ||
                   (segment->meta_data.object[i].raw_data_datatype == tdsTypeU16)) {
          segment->raw_data.channel[j].values = malloc(2 * segment->meta_data.object[i].raw_data_count * n_chunks);
        } else if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeI32) ||
                   (segment->meta_data.object[i].raw_data_datatype == tdsTypeU32) ||
                   (segment->meta_data.object[i].raw_data_datatype == tdsTypeSingleFloat) ||
                   (segment->meta_data.object[i].raw_data_datatype == tdsTypeSingleFloatWithUnit)) {
          segment->raw_data.channel[j].values = malloc(4 * segment->meta_data.object[i].raw_data_count * n_chunks);
        } else if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeI64) ||
                   (segment->meta_data.object[i].raw_data_datatype == tdsTypeU64) ||
                   (segment->meta_data.object[i].raw_data_datatype == tdsTypeDoubleFloat) ||
                   (segment->meta_data.object[i].raw_data_datatype == tdsTypeDoubleFloatWithUnit)) {
          segment->raw_data.channel[j].values = malloc(8 * segment->meta_data.object[i].raw_data_count * n_chunks);
        } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeTimeStamp) {
          /* This is converted into a double value */
          segment->raw_data.channel[j].values = malloc(8 * segment->meta_data.object[i].raw_data_count * n_chunks);
        } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeString) {
          fprintf(stderr, "tdms2sdds: string type channels are not yet supported in tdms2sdds\n");
          exit(EXIT_FAILURE);
        } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeVoid) {
          fprintf(stderr, "tdms2sdds: void type channels are not yet supported in tdms2sdds\n");
          exit(EXIT_FAILURE);
        } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeExtendedFloat) {
          fprintf(stderr, "tdms2sdds: extended float type channels are not yet supported in tdms2sdds\n");
          exit(EXIT_FAILURE);
        } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeExtendedFloatWithUnit) {
          fprintf(stderr, "tdms2sdds: extended float type with unit channels are not yet supported in tdms2sdds\n");
          exit(EXIT_FAILURE);
        } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeDAQmxRawData) {
          fprintf(stderr, "tdms2sdds: extended DAQmx raw data channels are not yet supported in tdms2sdds\n");
          exit(EXIT_FAILURE);
        } else {
          fprintf(stderr, "tdms2sdds: unknown data type\n");
          exit(EXIT_FAILURE);
        }
        segment->raw_data.channel[j].n_values = segment->meta_data.object[i].raw_data_count * n_chunks;
        j++;
      }
    }
    for (chunk = 0; chunk < n_chunks; chunk++) {
      j = 0;
      for (i = 0; i < segment->meta_data.n_objects; i++) {
        if (segment->meta_data.object[i].raw_data_index != 0xFFFFFFFF) {
          if (segment->meta_data.object[i].raw_data_datatype == tdsTypeBoolean) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((int8_t *)(segment->raw_data.channel[j].values))[k]), 1, 1, fd);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeI8) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((int8_t *)(segment->raw_data.channel[j].values))[k]), 1, 1, fd);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeU8) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((uint8_t *)(segment->raw_data.channel[j].values))[k]), 1, 1, fd);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeI16) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((int16_t *)(segment->raw_data.channel[j].values))[k]), 1, 2, fd);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeU16) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((uint16_t *)(segment->raw_data.channel[j].values))[k]), 1, 2, fd);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeI32) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((int32_t *)(segment->raw_data.channel[j].values))[k]), 1, 4, fd);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeU32) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((uint32_t *)(segment->raw_data.channel[j].values))[k]), 1, 4, fd);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeI64) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((int64_t *)(segment->raw_data.channel[j].values))[k]), 1, 8, fd);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeU64) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((uint64_t *)(segment->raw_data.channel[j].values))[k]), 1, 8, fd);
            }
          } else if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeSingleFloat) ||
                     (segment->meta_data.object[i].raw_data_datatype == tdsTypeSingleFloatWithUnit)) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((float *)(segment->raw_data.channel[j].values))[k]), 1, 4, fd);
            }
          } else if ((segment->meta_data.object[i].raw_data_datatype == tdsTypeDoubleFloat) ||
                     (segment->meta_data.object[i].raw_data_datatype == tdsTypeDoubleFloatWithUnit)) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&(((double *)(segment->raw_data.channel[j].values))[k]), 1, 8, fd);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeTimeStamp) {
            for (k = chunk * segment->meta_data.object[i].raw_data_count; k < (chunk + 1) * segment->meta_data.object[i].raw_data_count; k++) {
              bytesRead = fread(&uLLong, 1, 8, fd);
              bytesRead = fread(&LLong, 1, 8, fd);
              /*LLong = LLong - 2082844791LL; */ /* Subtract seconds between 1904 and 1970 */
              segment->raw_data.channel[j].values[k] = LLong + (uLLong * 5.42101086242752217e-20);
            }
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeString) {
            fprintf(stderr, "tdms2sdds: string type channels are not yet supported in tdms2sdds\n");
            exit(EXIT_FAILURE);
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeVoid) {
            fprintf(stderr, "tdms2sdds: void type channels are not yet supported in tdms2sdds\n");
            exit(EXIT_FAILURE);
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeExtendedFloat) {
            fprintf(stderr, "tdms2sdds: extended float type channels are not yet supported in tdms2sdds\n");
            exit(EXIT_FAILURE);
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeExtendedFloatWithUnit) {
            fprintf(stderr, "tdms2sdds: extended float with unit type channels are not yet supported in tdms2sdds\n");
            exit(EXIT_FAILURE);
          } else if (segment->meta_data.object[i].raw_data_datatype == tdsTypeDAQmxRawData) {
            fprintf(stderr, "tdms2sdds: DAQmx raw data channels are not yet supported in tdms2sdds\n");
            exit(EXIT_FAILURE);
          } else {
            fprintf(stderr, "tdms2sdds: unknown data type\n");
            exit(EXIT_FAILURE);
          }
          j++;
        }
      }
    }
  }
}

void TDMS_GetValue(FILE *fd, void **value, int32_t datatype) {
  uint64_t uLLong;
  int64_t LLong;

  if (datatype == tdsTypeI8) {
    *value = malloc(sizeof(int8_t));
    bytesRead = fread(((int8_t *)(*value)), 1, 1, fd);
  } else if (datatype == tdsTypeU8) {
    *value = malloc(sizeof(uint8_t));
    bytesRead = fread(((uint8_t *)(*value)), 1, 1, fd);
  } else if (datatype == tdsTypeI16) {
    *value = malloc(sizeof(int16_t));
    bytesRead = fread(((int16_t *)(*value)), 1, 2, fd);
  } else if (datatype == tdsTypeU16) {
    *value = malloc(sizeof(uint16_t));
    bytesRead = fread(((uint16_t *)(*value)), 1, 2, fd);
  } else if (datatype == tdsTypeI32) {
    *value = malloc(sizeof(int32_t));
    bytesRead = fread(((int32_t *)(*value)), 1, 4, fd);
  } else if (datatype == tdsTypeU32) {
    *value = malloc(sizeof(uint32_t));
    bytesRead = fread(((uint32_t *)(*value)), 1, 4, fd);
  } else if (datatype == tdsTypeI64) {
    *value = malloc(sizeof(int64_t));
    bytesRead = fread(((int64_t *)(*value)), 1, 8, fd);
  } else if (datatype == tdsTypeU64) {
    *value = malloc(sizeof(uint64_t));
    bytesRead = fread(((uint64_t *)(*value)), 1, 8, fd);
  } else if ((datatype == tdsTypeSingleFloat) || (datatype == tdsTypeSingleFloatWithUnit)) {
    *value = malloc(sizeof(float));
    bytesRead = fread(((float *)(*value)), 1, 4, fd);
  } else if ((datatype == tdsTypeDoubleFloat) || (datatype == tdsTypeDoubleFloatWithUnit)) {
    *value = malloc(sizeof(double));
    bytesRead = fread(((double *)(*value)), 1, 8, fd);
  } else if (datatype == tdsTypeBoolean) {
    *value = malloc(sizeof(int8_t));
    bytesRead = fread(((int8_t *)(*value)), 1, 1, fd);
  } else if (datatype == tdsTypeTimeStamp) {
    *value = malloc(sizeof(double));
    bytesRead = fread(&uLLong, 1, 8, fd);
    bytesRead = fread(&LLong, 1, 8, fd);
    /*LLong = LLong - 2082844791LL; */ /* Subtract seconds between 1904 and 1970 */
    *((double *)(*value)) = LLong + (uLLong * 5.42101086242752217e-20);
  } else if (datatype == tdsTypeVoid) {
    fprintf(stderr, "tdms2sdds: datatype Void is not yet supported in tdms2sdds\n");
    exit(EXIT_FAILURE);
  } else if (datatype == tdsTypeExtendedFloat) {
    fprintf(stderr, "tdms2sdds: datatype ExtendedFloat is not yet supported in tdms2sdds\n");
    exit(EXIT_FAILURE);
  } else if (datatype == tdsTypeExtendedFloatWithUnit) {
    fprintf(stderr, "tdms2sdds: datatype ExtendedFloatWithUnit is not yet supported in tdms2sdds\n");
    exit(EXIT_FAILURE);
  } else if (datatype == tdsTypeDAQmxRawData) {
    fprintf(stderr, "tdms2sdds: datatype DAQmxRawData is not yet supported in tdms2sdds\n");
    exit(EXIT_FAILURE);
  } else {
    fprintf(stderr, "tdms2sdds: unknown data type\n");
    exit(EXIT_FAILURE);
  }
}
