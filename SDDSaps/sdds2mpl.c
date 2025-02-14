/**
 * @file sdds2mpl.c
 * @brief Converts SDDS data files into Borland's older 'mpl' (Multi-Purpose Library) format.
 *
 * @details
 * This program reads data from SDDS (Self Describing Data Set) files and generates output files in 'mpl' format. It supports various options to configure input files, output formats, and page separation.
 *
 * @section Usage
 * ```
 * sdds2mpl [<SDDSfilename>] 
 *          [-pipe[=input]] 
 *          [-rootname=<string>]
 *          -output={column|parameter},<x-name>,<y-name>[,{<sy-name>|<sx-name>,<sy-name>}]...
 *          [-labelParameters=<name>[=<format>]...]
 *          [-separatePages]
 *          [-announceOpenings]
 * ```
 *
 * @section Options
 * | Required                              | Description                                                                           |
 * |---------------------------------------|---------------------------------------------------------------------------------------|
 * | `-output`           | Defines the output format and data.                                                             |
 *
 * | Option               | Description                                                                                     |
 * |----------------------|-------------------------------------------------------------------------------------------------|
 * | `-pipe`             | Reads input directly from a pipeline.                                                           |
 * | `-rootname`         | Specifies a root name for output files.                                                         |
 * | `-labelParameters`  | Adds labeled parameters to outputs.                                                             |
 * | `-separatePages`    | Separates data into multiple pages in output.                                                   |
 * | `-announceOpenings` | Prints a message when files are opened.                                                         |
 *
 * @subsection SR Specific Requirements
 *   - For `-rootname`:
 *     - If not provided, the input filename (minus extension) is used as the root name.
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
#include "table.h"
#include "scan.h"
#include "SDDS.h"
#include "SDDSaps.h"

/* Enumeration for option types */
enum option_type {
  SET_ROOTNAME,
  SET_OUTPUT,
  SET_SEPARATE_PAGES,
  SET_LABEL_PARAMETERS,
  SET_ANNOUNCE_OPENINGS,
  SET_PIPE,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "rootname",
  "output",
  "separatepages",
  "labelparameters",
  "announceopenings",
  "pipe",
};

static const char *usage =
  "sdds2mpl [<SDDSfilename>]\n"
  "         [-pipe[=input]] \n"
  "         [-rootname=<string>]\n"
  "          -output={column|parameter},<x-name>,<y-name>[,{<sy-name>|<sx-name>,<sy-name>}]...\n"
  "         [-labelParameters=<name>[=<format>]...]\n"
  "         [-separatePages]\n"
  "         [-announceOpenings]\n"
  "Any number of -output specifications may be given.\n\n"
  "sdds2mpl extracts data from an SDDS file into MPL-format files.\n"
  "Program by Michael Borland.  ("__DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SDDS_DATASET sdds_dataset;
  LABEL_PARAMETER *label_parameter = NULL;
  long label_parameters = 0;
  COLUMN_DEFINITION **coldef;
  PARAMETER_DEFINITION **pardef;
  char *inputfile = NULL;
  char *rootname = NULL;
  char filename[200];
  long outputs = 0;
  OUTPUT_REQUEST **output = NULL;
  long i, i_arg;
  int64_t n_rows;
  long page_number, separate_pages = 0;
  long announce_openings = 0;
  SCANNED_ARG *s_arg;
  char *ptr;
  void *data[4];
  double param[4];
  long data_present = 0;
  unsigned long pipe_flags = 0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fputs(usage, stderr);
    exit(EXIT_FAILURE);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_ROOTNAME:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -rootname syntax");
        rootname = s_arg[i_arg].list[1];
        break;
      case SET_OUTPUT:
        if (s_arg[i_arg].n_items < 4 || s_arg[i_arg].n_items > 6)
          SDDS_Bomb("Invalid -output syntax");
        output = trealloc(output, sizeof(*output) * (outputs + 1));
        if (!(output[outputs] = process_output_request(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, outputs ? output[outputs - 1] : NULL)))
          SDDS_Bomb("Invalid -output syntax");
        outputs++;
        break;
      case SET_SEPARATE_PAGES:
        separate_pages = 1;
        break;
      case SET_LABEL_PARAMETERS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -labelparameters syntax");
        label_parameter = trealloc(label_parameter, sizeof(*label_parameter) * (label_parameters + s_arg[i_arg].n_items));
        for (i = 1; i < s_arg[i_arg].n_items; i++)
          scan_label_parameter(label_parameter + i - 1, s_arg[i_arg].list[i]);
        label_parameters += s_arg[i_arg].n_items - 1;
        break;
      case SET_ANNOUNCE_OPENINGS:
        announce_openings = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;
      default:
        SDDS_Bomb("Unknown switch");
        break;
      }
    } else {
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many filenames");
    }
  }

  if (!inputfile && !(pipe_flags & USE_STDIN))
    SDDS_Bomb("No input source given");

  if (!rootname) {
    if (!inputfile)
      SDDS_Bomb("You must give a rootname if you don't give an input filename");
    SDDS_CopyString(&rootname, inputfile);
    if ((ptr = strrchr(rootname, '.')))
      *ptr = '\0';
  }

  if (outputs <= 0)
    SDDS_Bomb("No output specifications given");

  if (!SDDS_InitializeInput(&sdds_dataset, inputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < outputs; i++) {
    if (!output[i]->parameter_output) {
      if (SDDS_GetColumnIndex(&sdds_dataset, output[i]->item[0]) < 0 ||
          SDDS_GetColumnIndex(&sdds_dataset, output[i]->item[1]) < 0 ||
          (output[i]->item[2] && SDDS_GetColumnIndex(&sdds_dataset, output[i]->item[2]) < 0) ||
          (output[i]->item[3] && SDDS_GetColumnIndex(&sdds_dataset, output[i]->item[3]) < 0)) {
        fprintf(stderr, "Error: unrecognized column name given\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    } else {
      if (SDDS_GetParameterIndex(&sdds_dataset, output[i]->item[0]) < 0 ||
          SDDS_GetParameterIndex(&sdds_dataset, output[i]->item[1]) < 0 ||
          (output[i]->item[2] && SDDS_GetParameterIndex(&sdds_dataset, output[i]->item[2]) < 0) ||
          (output[i]->item[3] && SDDS_GetParameterIndex(&sdds_dataset, output[i]->item[3]) < 0)) {
        fprintf(stderr, "Error: unrecognized parameter name given\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
    }
  }

  while ((page_number = SDDS_ReadPage(&sdds_dataset)) > 0) {
    data_present = 1;
    if (!SDDS_SetRowFlags(&sdds_dataset, 1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if ((n_rows = SDDS_CountRowsOfInterest(&sdds_dataset)) <= 0) {
      fprintf(stderr, "Warning: no rows selected for page %" PRId32 "\n", sdds_dataset.page_number);
      if (SDDS_NumberOfErrors())
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    for (i = 0; i < outputs; i++) {
      if (!output[i]->fp) {
        if (separate_pages && !output[i]->parameter_output)
          snprintf(filename, sizeof(filename), "%s_%03ld_%s_%s.out", rootname, output[i]->counter++, output[i]->item[0], output[i]->item[1]);
        else
          snprintf(filename, sizeof(filename), "%s_%s_%s.out", rootname, output[i]->item[0], output[i]->item[1]);
        set_up_output(filename, output[i], label_parameter, label_parameters, separate_pages, announce_openings, &sdds_dataset);
      }
      if (!output[i]->parameter_output) {
        coldef = (COLUMN_DEFINITION **)output[i]->definitions;
        data[2] = data[3] = NULL;
        if (!(data[0] = SDDS_GetColumn(&sdds_dataset, output[i]->item[0])) ||
            !(data[1] = SDDS_GetColumn(&sdds_dataset, output[i]->item[1])) ||
            (output[i]->columns > 2 && !(data[2] = SDDS_GetColumn(&sdds_dataset, output[i]->item[2]))) ||
            (output[i]->columns > 3 && !(data[3] = SDDS_GetColumn(&sdds_dataset, output[i]->item[3])))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
        for (int64_t j = 0; j < n_rows; j++) {
          for (long k = 0; k < output[i]->columns; k++) {
            SDDS_PrintTypedValue((char *)data[k], j, coldef[k]->type, coldef[k]->format_string, output[i]->fp, 0);
            if (k < output[i]->columns - 1)
              fputc(' ', output[i]->fp);
          }
          fputc('\n', output[i]->fp);
        }
        output[i]->points += n_rows;
        for (long k = 0; k < output[i]->columns; k++) {
          free(data[k]);
        }
      } else {
        pardef = (PARAMETER_DEFINITION **)output[i]->definitions;
        for (long k = 0; k < output[i]->columns; k++) {
          if (!SDDS_GetParameter(&sdds_dataset, output[i]->item[k], &param[k])) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(EXIT_FAILURE);
          }
        }
        output[i]->points += 1;
        for (long k = 0; k < output[i]->columns; k++) {
          SDDS_PrintTypedValue((char *)&param[k], 0, pardef[k]->type, pardef[k]->format_string, output[i]->fp, 0);
          if (k < output[i]->columns - 1)
            fputc(' ', output[i]->fp);
        }
        fputc('\n', output[i]->fp);
      }
      if (separate_pages && !output[i]->parameter_output) {
        fclose(output[i]->fp);
        output[i]->fp = NULL;
        output[i]->points = 0;
      }
    }
  }

  if (page_number == 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (page_number == -1 && !data_present) {
    if (SDDS_NumberOfErrors()) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Error: input data file is empty\n");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < outputs; i++) {
    if (output[i]->fp)
      fclose(output[i]->fp);
  }

  for (i = 0; i < outputs; i++) {
    if (!separate_pages || output[i]->parameter_output) {
      snprintf(filename, sizeof(filename), "%s_%s_%s.out", rootname, output[i]->item[0], output[i]->item[1]);
      fixcount(filename, output[i]->points);
    }
  }
  return 0;
}
