/**
 * @file mcs2sdds.c
 * @brief Converts EG&G Turbo MCS (Multi-Channel Scaler) data files to SDDS format.
 *
 * @details
 * This program reads an EG&G Turbo MCS file, processes its header and data sections,
 * and outputs an equivalent SDDS (Self Describing Data Sets) file.
 * 
 * @section Usage
 * ```
 * mcs2sdds <inputfile> <outputfile> [-ascii]
 * ```
 *
 * @section Options
 * | Option     | Description                                                                    |
 * |------------|--------------------------------------------------------------------------------|
 * | `-ascii`   | Outputs the SDDS file in ASCII format. Default is binary format.              |
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
 * M. Borland, C. Saunders, R. Soliday
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_ASCII,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "ascii"};

char *USAGE =
  "Usage: mcs2sdds <inputfile> <outputfile> [-ascii]\n"
  "Options:\n"
  "  -ascii    Output the SDDS file in ASCII format. By default, binary format is used.\n"
  "\n"
  "Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

#define MCS -4

void swapshort(short *data);
void swaplong(long *data);
void swapulong(unsigned long *data);
void swapushort(unsigned short *data);
void swapfloat(float *data);

int main(int argc, char **argv) {
  char reserved[100];         /* buffer to skip reserved bytes */
  short f_type;               /* MCS file type */
  char trigger;               /* Trigger Flag */
  char dwell_flag;            /* External Dwell Flag */
  char dwell_units;           /* 0=us, 1=ms, 2=sec, 3=ns */
  char acq_mode;              /* Acquisition mode flag 0=replace, 1=sum */
  unsigned long dwell_913;    /* Dwell time in old 913 format */
  unsigned short pass_length; /* pass length in channels */
  unsigned long pass_count;
  unsigned long pass_count_preset;
  char acq_time[9];         /* buffer for acquisition time */
  char acq_date[9];         /* buffer for acquisition date */
  unsigned short mark_chan; /* first marker channel */
  char mcs_num;             /* 1-8 are valid */
  char cal_flag;            /* 0=no calibration */
  char cal_units[4];        /* calibration units */
  float cal_zero;           /* calibration zero intercept */
  float cal_slope;          /* calibration slope */
  char id_byte;             /* always 0xaa */
  char detector_len;        /* Detector description length */
  char detector[65];        /* detector description */
  char sample_len;          /* Sample description length */
  char sample[65];          /* Sample description */
  char disc_sel;            /* 0=SCA otherwise Disc */
  char disc_edge;           /* 0=falling else rising */
  float disc;               /* disc setting in volts */
  float sca_uld;            /* sca upper-level in volts */
  float sca_lld;            /* sca lower-level in volts */
  float dwell;              /* dwell time in dwell_units */
  char consistent;          /* settings consistent flag */
  char mcs_id[9];           /* MCS ID string "0914-001" */

  FILE *fpi;
  char *input, *output;
  SDDS_DATASET SDDS_dataset;
  SCANNED_ARG *scanned;
  long i, i_arg;
  char ts1[256], ts2[256], ts3[256], ts4[256];
  unsigned long *ucount;
  long *count, *channel;
  long ascii;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "Error: Insufficient arguments.\n");
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  input = output = NULL;
  ascii = 0;
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      switch (match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_ASCII:
        ascii = 1;
        break;
      default:
        fprintf(stderr, "Error: Invalid option '%s'.\n", scanned[i_arg].list[0]);
        fprintf(stderr, "%s", USAGE);
        exit(EXIT_FAILURE);
      }
    } else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else {
        fprintf(stderr, "Error: Too many filenames provided.\n");
        fprintf(stderr, "%s", USAGE);
        exit(EXIT_FAILURE);
      }
    }
  }

  if (!input) {
    SDDS_Bomb("Error: Input file not specified.");
  }
  if (!output) {
    SDDS_Bomb("Error: Output file not specified.");
  }

  fpi = fopen_e(input, "r", 0);

  /***************************************************************************/
  /*    Header Data     */
  /* Read header info from MCS file */
  /***************************************************************************/
  /* Read filetype -4 (MCS) */
  if (fread(&f_type, sizeof(short), 1, fpi) != 1) {
    SDDS_Bomb("Error: Unable to read file type.");
  }
  swapshort(&f_type);
  if (f_type != MCS) {
    fprintf(stderr, "Error: Not a valid MCS file. f_type = %hx\n", f_type);
    fclose(fpi);
    exit(EXIT_FAILURE);
  }

  fread(&trigger, sizeof(char), 1, fpi);     /* Read Trigger Flag */
  fread(&dwell_flag, sizeof(char), 1, fpi);  /* Read dwell flag */
  fread(&dwell_units, sizeof(char), 1, fpi); /* Read dwell units */
  fread(&acq_mode, sizeof(char), 1, fpi);
  fread(&dwell_913, sizeof(long), 1, fpi);
  swapulong(&dwell_913);
  fread(&pass_length, sizeof(short), 1, fpi);
  swapushort(&pass_length);
  fread(&pass_count, sizeof(long), 1, fpi);
  swapulong(&pass_count);
  fread(&pass_count_preset, sizeof(long), 1, fpi);
  swapulong(&pass_count_preset);
  fread(acq_time, sizeof(char), 8, fpi);
  fread(acq_date, sizeof(char), 8, fpi);
  fread(&mark_chan, sizeof(short), 1, fpi);
  swapushort(&mark_chan);
  fread(&mcs_num, sizeof(char), 1, fpi);
  fread(&cal_flag, sizeof(char), 1, fpi);
  fread(cal_units, sizeof(char), 4, fpi);
  fread(&cal_zero, sizeof(float), 1, fpi);
  swapfloat(&cal_zero);
  fread(&cal_slope, sizeof(float), 1, fpi);
  swapfloat(&cal_slope);
  fread(reserved, sizeof(char), 10, fpi);
  fread(&id_byte, sizeof(char), 1, fpi);
  fread(reserved, sizeof(char), 1, fpi);
  fread(&detector_len, sizeof(char), 1, fpi);
  fread(detector, sizeof(char), 63, fpi);
  fread(&sample_len, sizeof(char), 1, fpi);
  fread(sample, sizeof(char), 63, fpi);
  fread(reserved, sizeof(char), 16, fpi); /* skip view info & reserved */
  fread(&disc_sel, sizeof(char), 1, fpi);
  fread(&disc_edge, sizeof(char), 1, fpi);
  fread(&disc, sizeof(float), 1, fpi);
  swapfloat(&disc);
  fread(&sca_uld, sizeof(float), 1, fpi);
  swapfloat(&sca_uld);
  fread(&sca_lld, sizeof(float), 1, fpi);
  swapfloat(&sca_lld);
  fread(&dwell, sizeof(float), 1, fpi);
  swapfloat(&dwell);
  fread(&consistent, sizeof(char), 1, fpi);
  fread(reserved, sizeof(char), 21, fpi);
  fread(mcs_id, sizeof(char), 8, fpi);
  mcs_id[8] = '\0';

  sprintf(ts1, "%d", mcs_num + 1);
  sprintf(ts2, "%hd", pass_length);
  sprintf(ts3, "%ld", pass_count);
  sprintf(ts4, "%ld", pass_count_preset);

  if (!SDDS_InitializeOutput(&SDDS_dataset, ascii ? SDDS_ASCII : SDDS_BINARY, 1, "Turbo MCS data", "Turbo MCS data", output) ||
      SDDS_DefineParameter(&SDDS_dataset, "MCSNumber", NULL, NULL, "MCS number", NULL, SDDS_SHORT, ts1) < 0 ||
      SDDS_DefineParameter(&SDDS_dataset, "MCSID", NULL, NULL, "MCS ID", NULL, SDDS_STRING, mcs_id) < 0 ||
      SDDS_DefineParameter(&SDDS_dataset, "PassLength", NULL, NULL, "Pass length", NULL, SDDS_SHORT, ts2) < 0 ||
      SDDS_DefineParameter(&SDDS_dataset, "PassCount", NULL, NULL, "Pass count", NULL, SDDS_LONG, ts3) < 0 ||
      SDDS_DefineParameter(&SDDS_dataset, "PassCountPreset", NULL, NULL, "Pass count preset", NULL, SDDS_LONG, ts4) < 0) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  if (dwell_flag == 0) {
    if (dwell_units == 0)
      dwell *= 1e-6;
    else if (dwell_units == 1)
      dwell *= 1e-3;
    else if (dwell_units == 3)
      dwell *= 1e-9;
  } else {
    dwell = -1;
  }

  sprintf(ts1, "%15.8e", dwell);
  sprintf(ts2, "%8s %8s", acq_time, acq_date);
  if (SDDS_DefineParameter(&SDDS_dataset, "DwellTime", NULL, "s", "Dwell time", NULL, SDDS_DOUBLE, ts1) < 0 ||
      SDDS_DefineParameter(&SDDS_dataset, "TriggerMode", NULL, NULL, "Trigger mode", NULL, SDDS_STRING, trigger ? "external" : "internal") < 0 ||
      SDDS_DefineParameter(&SDDS_dataset, "AcquisitionMode", NULL, NULL, "Acquisition mode", NULL, SDDS_STRING, acq_mode ? "sum" : "replace") < 0 ||
      SDDS_DefineParameter(&SDDS_dataset, "TimeStamp", NULL, NULL, "Time at which data collection started", NULL, SDDS_STRING, ts2) < 0) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  sprintf(ts1, "%15.8e", cal_slope);
  sprintf(ts2, "%15.8e", cal_zero);
  if (cal_flag && (SDDS_DefineParameter(&SDDS_dataset, "CalibrationSlope", NULL, cal_units, "Spectrum calibration slope", NULL, SDDS_DOUBLE, ts1) < 0 ||
                   SDDS_DefineParameter(&SDDS_dataset, "CalibrationOffset", NULL, cal_units, "Spectrum calibration offset", NULL, SDDS_DOUBLE, ts2) < 0)) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  if (detector_len) {
    detector[(unsigned)detector_len] = '\0';
    if (SDDS_DefineParameter(&SDDS_dataset, "HardwareDescription", NULL, NULL, "Hardware description", NULL, SDDS_STRING, detector) < 0) {
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  }

  if (sample_len) {
    sample[(unsigned)sample_len] = '\0';
    if (SDDS_DefineParameter(&SDDS_dataset, "DataDescription", NULL, NULL, "Data description", NULL, SDDS_STRING, sample) < 0 ||
        SDDS_DefineParameter(&SDDS_dataset, "mplTitle", NULL, NULL, "MPL Title", NULL, SDDS_STRING, sample) < 0) {
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  }

  if (disc_sel == 0) {
    sprintf(ts1, "%15.8e", sca_lld);
    sprintf(ts2, "%15.8e", sca_uld);
    if (SDDS_DefineParameter(&SDDS_dataset, "SCA-LLD", NULL, "V", "SCA Lower-Level Discriminator Setting", NULL, SDDS_DOUBLE, ts1) < 0 ||
        SDDS_DefineParameter(&SDDS_dataset, "SCA-ULD", NULL, "V", "SCA Upper-Level Discriminator Setting", NULL, SDDS_DOUBLE, ts2) < 0) {
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  } else {
    sprintf(ts1, "%15.8e", disc);
    if (SDDS_DefineParameter(&SDDS_dataset, "DiscrimLevel", NULL, "V", "Discriminator Level", NULL, SDDS_DOUBLE, ts1) < 0 ||
        SDDS_DefineParameter(&SDDS_dataset, "DiscrimSlope", NULL, NULL, "Discriminator Slope", NULL, SDDS_STRING, disc_edge ? "+1" : "-1") < 0) {
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  }

  if (consistent == 0) {
    if (SDDS_DefineParameter(&SDDS_dataset, "Inconsistent", NULL, NULL, "Flag indicating whether data and settings are inconsistent", NULL, SDDS_LONG, "1") < 0) {
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    fprintf(stderr, "WARNING: Settings are not consistent with data\n");
  }

  if (SDDS_DefineColumn(&SDDS_dataset, "ChannelNumber", NULL, NULL, "Channel number", NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_dataset, "EventCount", NULL, NULL, "Number of events", NULL, SDDS_LONG, 0) < 0 ||
      !SDDS_WriteLayout(&SDDS_dataset) || !SDDS_StartPage(&SDDS_dataset, (long)pass_length)) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  /***************************************************************************/
  /*    Channel Data     */
  /* Output channel data from MCS file */
  /***************************************************************************/
  channel = tmalloc(sizeof(*channel) * pass_length);
  count = tmalloc(sizeof(*count) * pass_length);
  ucount = tmalloc(sizeof(*ucount) * pass_length);
  if (fread(ucount, sizeof(*ucount), pass_length, fpi) != pass_length) {
    SDDS_Bomb("Error: Unable to read channel data.");
  }
  for (i = 0; i < (long)pass_length; i++) {
    swapulong(&ucount[i]);
    count[i] = (long)ucount[i];
    channel[i] = i;
  }
  if (!SDDS_SetColumn(&SDDS_dataset, SDDS_SET_BY_NAME, channel, (long)pass_length, "ChannelNumber") ||
      !SDDS_SetColumn(&SDDS_dataset, SDDS_SET_BY_NAME, count, (long)pass_length, "EventCount") ||
      !SDDS_WritePage(&SDDS_dataset) || !SDDS_Terminate(&SDDS_dataset)) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  fclose(fpi);
  return EXIT_SUCCESS;
}

void swapshort(short *data) {
  unsigned char c1;
  c1 = *((char *)data);
  *((char *)data) = *(((char *)data) + 1);
  *(((char *)data) + 1) = c1;
}

void swapushort(unsigned short *data) {
  unsigned char c1;
  c1 = *((char *)data);
  *((char *)data) = *(((char *)data) + 1);
  *(((char *)data) + 1) = c1;
}

void swaplong(long *data) {
  long copy;
  copy = *data;
  *(((char *)data) + 0) = *(((char *)&copy) + 3);
  *(((char *)data) + 1) = *(((char *)&copy) + 2);
  *(((char *)data) + 2) = *(((char *)&copy) + 1);
  *(((char *)data) + 3) = *(((char *)&copy) + 0);
}

void swapulong(unsigned long *data) {
  unsigned long copy;
  copy = *data;
  *(((char *)data) + 0) = *(((char *)&copy) + 3);
  *(((char *)data) + 1) = *(((char *)&copy) + 2);
  *(((char *)data) + 2) = *(((char *)&copy) + 1);
  *(((char *)data) + 3) = *(((char *)&copy) + 0);
}

void swapfloat(float *data) {
  float copy;
  copy = *data;
  *(((char *)data) + 0) = *(((char *)&copy) + 3);
  *(((char *)data) + 1) = *(((char *)&copy) + 2);
  *(((char *)data) + 2) = *(((char *)&copy) + 1);
  *(((char *)data) + 3) = *(((char *)&copy) + 0);
}
