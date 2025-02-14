/**
 * @file sddsduplicate.c
 * @brief A program for duplicating rows in an SDDS file based on a weight column or fixed duplication factors.
 *
 * @details
 * This program duplicates rows in an SDDS file and creates a new output file. The number of duplicates can be
 * determined either by a weight column or by a fixed duplication factor. Users can specify minimum and maximum
 * duplication factors, enable probabilistic duplication, and control verbosity settings. The program also supports
 * input/output through pipes for integration into data processing pipelines.
 *
 * @section Usage
 * ```
 * sddsduplicate [<input>] [<output>]
 *               [-pipe=[input][,output]]
 *               [-weight=<columnName>]
 *               [-minFactor=<integer>]
 *               [-maxFactor=<integer>]
 *               [-factor=<integer>]
 *               [-probabilistic]
 *               [-seed=<integer>]
 *               [-verbosity[=<level>]]
 * ```
 *
 * @section Options
 * | Option            | Description                                                                      |
 * |-------------------|----------------------------------------------------------------------------------|
 * | `-pipe`           | Use pipes for input and/or output.                                               |
 * | `-weight`         | Specify the column to use for weighting the number of duplicates.                |
 * | `-minFactor`      | Set the minimum number of rows to emit, scaling weights accordingly.             |
 * | `-maxFactor`      | Set the maximum number of rows to emit, scaling weights accordingly.             |
 * | `-factor`         | Specify a fixed number of duplicates to create. Mutually exclusive with `-weight`|
 * | `-probabilistic`  | Treat fractional duplication counts as probabilities.                            |
 * | `-seed`           | Set the random number generator seed (default: system clock).                    |
 * | `-verbosity`      | Set verbosity level for detailed output.                                         |
 *
 * @subsection Incompatibilities
 *   - `-weight` is incompatible with:
 *     - `-factor`
 *   - `-minFactor` and `-maxFactor` only work with:
 *     - `-weight`
 *   - Only one of the following may be specified:
 *     - `-minFactor`
 *     - `-maxFactor`
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
 * M. Borland, R. Soliday
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"

// Enumeration of available options for parsing input arguments
typedef enum {
  SET_WEIGHT,
  SET_PIPE,
  SET_MAXFACTOR,
  SET_MINFACTOR,
  SET_FACTOR,
  SET_VERBOSITY,
  SET_SEED,
  SET_PROBABILISTIC,
  N_OPTIONS
} OptionType;

// String representations of each option for command-line parsing
static char *option[N_OPTIONS] = {
  "weight",
  "pipe",
  "maxfactor",
  "minfactor",
  "factor",
  "verbosity",
  "seed",
  "probabilistic",
};

// Usage message displayed when the program is invoked incorrectly
static char *usage =
  "sddsduplicate [<input>] [<output>]\n"
  "              [-pipe=[input][,output]]\n"
  "              [-weight=<columnName>]\n"
  "              [-minFactor=<integer>]\n"
  "              [-maxFactor=<integer>]\n"
  "              [-factor=<integer>]\n"
  "              [-probabilistic]\n"
  "              [-seed=<integer>]\n"
  "              [-verbosity[=<level>]]\n"
  "Options:\n"
  "  -pipe=[input][,output]\n"
  "      Use pipes for input and/or output.\n\n"
  "  -weight=<columnName>\n"
  "      Name of a column to use for weighting the number of duplicates.\n\n"
  "  -minFactor=<integer>\n"
  "      Minimum number of rows to emit. Results in scaling of weights.\n\n"
  "  -maxFactor=<integer>\n"
  "      Maximum number of rows to emit. Results in scaling of weights.\n"
  "      In some cases, input rows will not appear in the output file because\n"
  "      the weight is less than 1.\n\n"
  "  -factor=<integer>\n"
  "      Number of duplicates to create. Incompatible with -weight.\n\n"
  "  -probabilistic\n"
  "      Treat fractional duplication counts as probabilities.\n\n"
  "  -seed=<integer>\n"
  "      Set the seed for random number generation. By default, the\n"
  "      system clock is used.\n\n"
  "  -verbosity[=<level>]\n"
  "      Set verbosity level.\n\n"
  "This program duplicates rows in the input file and creates a new file.\n"
  "The number of duplicates is determined either by a weight column or\n"
  "by a fixed value.\n\n"
  "Program by Michael Borland.  (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

int main(int argc, char **argv) {
  SDDS_DATASET sdds_input, sdds_output;
  char *inputfile = NULL, *outputfile = NULL;
  long i_arg, verbosity = 0;
  SCANNED_ARG *s_arg;
  unsigned long pipe_flags = 0;
  char *weight_column_name = NULL;
  double *weight_data = NULL, min_weight, max_weight;
  double *dup_value = NULL;
  long max_factor = 0, min_factor = 0, dup_rows = 0;
  long random_number_seed = 0;
  int64_t i, j, input_rows, stored_rows;
  short probabilistic = 0;

  // Register the program name for error messages
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3)
    bomb(NULL, usage); // Ensure sufficient arguments

  // Parse command-line arguments
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      // Match the option and process accordingly
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_PIPE:
        // Process pipe-related options
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipe_flags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_WEIGHT:
        // Specify the column name for weighting
        if (s_arg[i_arg].n_items != 2 || !(weight_column_name = s_arg[i_arg].list[1]))
          bomb("invalid -weight syntax", usage);
        break;
      case SET_FACTOR:
        // Specify a fixed number of duplicates
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &dup_rows) != 1 || dup_rows <= 0)
          bomb("invalid -rows syntax", usage);
        break;
      case SET_MINFACTOR:
        // Specify the minimum duplication factor
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &min_factor) != 1 || min_factor <= 0)
          bomb("invalid -minFactor syntax", usage);
        break;
      case SET_MAXFACTOR:
        // Specify the maximum duplication factor
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &max_factor) != 1 || max_factor <= 0)
          bomb("invalid -maxFactor syntax", usage);
        break;
      case SET_VERBOSITY:
        // Set the verbosity level
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &verbosity) != 1 || verbosity < 0)
          bomb("invalid -verbosity syntax", usage);
        break;
      case SET_PROBABILISTIC:
        // Enable probabilistic duplication
        probabilistic = 1;
        if (s_arg[i_arg].n_items != 1)
          bomb("invalid -probabilistic syntax", usage);
        break;
      case SET_SEED:
        // Set the random number generator seed
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%ld", &random_number_seed) != 1 || random_number_seed < 0)
          bomb("invalid -seed syntax", usage);
        break;
      default:
        // Unrecognized option
        bomb("unrecognized option", usage);
        break;
      }
    } else {
      // Process input and output file arguments
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames");
    }
  }

  // Ensure only one of minFactor or maxFactor is set
  if (min_factor && max_factor)
    SDDS_Bomb("give only one of -minFactor and -maxFactor");

  // Process file names and pipe configurations
  processFilenames("sddsduplicate", &inputfile, &outputfile, pipe_flags, 0, NULL);

  // Initialize random number generator
  if (random_number_seed == 0) {
    random_number_seed = (long)time(NULL);                 // Use system clock if no seed provided
    random_number_seed = 2 * (random_number_seed / 2) + 1; // Ensure odd seed
#if defined(_WIN32) || defined(__APPLE__)
    random_1(-labs(random_number_seed));
#else
    random_1(-FABS(random_number_seed));
#endif
  } else {
    random_1(-random_number_seed); // Use specified seed
  }

  // Initialize SDDS datasets
  if (!SDDS_InitializeInput(&sdds_input, inputfile) ||
      !SDDS_InitializeCopy(&sdds_output, &sdds_input, outputfile, "w") ||
      !SDDS_WriteLayout(&sdds_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  // Main loop for reading and duplicating rows
  while (SDDS_ReadPage(&sdds_input) > 0) {
    input_rows = SDDS_RowCount(&sdds_input); // Get number of rows in current page
    if (input_rows > 0) {
      dup_value = tmalloc(sizeof(*dup_value) * input_rows); // Allocate duplication array

      // Handle weighting logic if a weight column is specified
      if (weight_column_name) {
        if (!(weight_data = SDDS_GetColumnInDoubles(&sdds_input, weight_column_name))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }

        // Scale weights based on minFactor or maxFactor
        if (min_factor) {
          find_min_max(&min_weight, &max_weight, weight_data, input_rows);
          if (min_weight <= 0)
            SDDS_Bomb("Minimum weight value is nonpositive. Can't use -minFactor.");
          for (i = 0; i < input_rows; i++)
            dup_value[i] = weight_data[i] * min_factor / min_weight;
        } else if (max_factor) {
          find_min_max(&min_weight, &max_weight, weight_data, input_rows);
          if (max_weight <= 0)
            SDDS_Bomb("Maximum weight value is nonpositive. Can't use -maxFactor.");
          for (i = 0; i < input_rows; i++)
            dup_value[i] = weight_data[i] * max_factor / max_weight;
        } else {
          for (i = 0; i < input_rows; i++)
            dup_value[i] = weight_data[i];
        }

        // Apply probabilistic logic for fractional duplication counts
        if (probabilistic) {
          double fraction;
          for (i = 0; i < input_rows; i++) {
            fraction = dup_value[i] - ((long)dup_value[i]);
            dup_value[i] = (long)dup_value[i];
            if (fraction > random_1(0))
              dup_value[i] += 1;
          }
        } else {
          for (i = 0; i < input_rows; i++)
            dup_value[i] = (long)dup_value[i];
        }
      } else {
        // Use fixed duplication factor if no weight column is provided
        for (i = 0; i < input_rows; i++)
          dup_value[i] = dup_rows;
      }

      // Count total rows to be stored after duplication
      stored_rows = 0;
      for (i = 0; i < input_rows; i++)
        stored_rows += (int64_t)dup_value[i];

      // Print duplication summary if verbosity is enabled
      if (verbosity) {
        int64_t max_dup = 0, min_dup = INT64_MAX;
        for (i = 0; i < input_rows; i++) {
          if (max_dup < dup_value[i])
            max_dup = dup_value[i];
          if (min_dup > dup_value[i])
            min_dup = dup_value[i];
        }
        fprintf(stderr, "%" PRId64 " output rows, minimum and maximum duplication factor: %" PRId64 ", %" PRId64 "\n",
                stored_rows, min_dup, max_dup);
      }

      // Start a new SDDS page and copy data
      if (!SDDS_StartPage(&sdds_output, stored_rows) ||
          !SDDS_CopyParameters(&sdds_output, &sdds_input) ||
          !SDDS_CopyArrays(&sdds_output, &sdds_input)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      // Duplicate rows into the output dataset
      stored_rows = 0;
      for (i = 0; i < input_rows; i++) {
        for (j = 0; j < dup_value[i]; j++) {
          if (SDDS_CopyRowDirect(&sdds_output, stored_rows++, &sdds_input, i)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
      }

      // Write the completed page to the output file
      if (!SDDS_WritePage(&sdds_output)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }

      free(dup_value); // Free allocated memory for duplication array
      dup_value = NULL;
    }
  }

  // Terminate SDDS datasets and close files
  if (!SDDS_Terminate(&sdds_input) || !SDDS_Terminate(&sdds_output)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  return 0; // Exit successfully
}
