/**
 * @file sddsdigfilter.c
 * @brief General series/cascade time-domain filtering utility.
 *
 * @details
 * This program applies series/cascade time-domain filtering to input data using proportional, 
 * low-pass, high-pass, digital, and analog filters. It supports cascading of filters and 
 * customization for various filter configurations. Data can be supplied via files or pipes.
 *
 * @section Usage
 * ```
 * sddsdigfilter [<inputfile>] [<outputfile>]
 *               [-pipe=[input][,output]]
 *                -columns=<xcol>,<ycol>
 *               [-proportional=<gain>]
 *               [-lowpass=<gain>,<cutoff_freq>]
 *               [-highpass=<gain>,<cutoff_freq>]
 *               [-digitalfilter=<sddsfile>,<a_coeff_col>,<b_coeff_col>]
 *               [-digitalfilter=[A,<a0>,<a1>[,...]][,B,<b0>,<b1>[,...]]]
 *               [-analogfilter=<sddsfile>,<c_coeff_col>,<d_coeff_col>]
 *               [-analogfilter=[C,<c0>,<c1>[,...]][,D,<d0>,<d1>[,...]]]
 *               [-cascade]
 *               [-verbose]
 *               [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Required                            | Description                                                 |
 * |-------------------------------------|-------------------------------------------------------------|
 * | `-columns`                          | Specifies the x and y columns to process.                   |
 *
 * | Optional                            | Description                                                 |
 * |-------------------------------------|-------------------------------------------------------------|
 * | `-pipe`                             | Uses standard input/output.                                 |
 * | `-proportional`                     | Applies a proportional gain to the input data.              |
 * | `-lowpass`                          | Applies a low-pass filter with specified gain and cutoff.   |
 * | `-highpass`                         | Applies a high-pass filter with specified gain and cutoff.  |
 * | `-digitalfilter`                    | Apply a digital filter with given coefficients.           |
 * | `-analogfilter`                     | Apply an analog filter with given coefficients.           |
 * | `-cascade`                          | Enables cascading of filters.                               |
 * | `-verbose`                          | Outputs detailed processing information.                    |
 * | `-majorOrder`                       | Specifies the major order of data (row or column).          |
 *
 * @subsection Incompatibilities
 * - `-cascade` is incompatible with:
 *   - Using no filters.
 *
 * @subsection MEO Mutually Exclusive Options
 * - Only one of the following may be specified:
 *   - `-digitalfilter=<sddsfile>,<a_col>,<b_col>`
 *   - `-digitalfilter=[A,<a0>,<a1>,...][,B,<b0>,<b1>,...]`
 * - Only one of the following may be specified:
 *   - `-analogfilter=<sddsfile>,<c_col>,<d_col>`
 *   - `-analogfilter=[C,<c0>,<c1>,...][,D,<d0>,<d1>,...]`
 *
 * @note Digital filter form:
 * @verbatim
 *             b0 + b1.z^-1 + ... + bn.z^-n
 *     H(z) = --------------------------------
 *             a0 + a1.z^-1 + ... + an.z^-n
 * @endverbatim
 *
 * @note Analog filter form:
 * @verbatim
 *             d0 + d1.s^1 + ... + dn.s^n
 *     H(s) = ------------------------------
 *             c0 + c1.s^1 + ... + cn.s^n
 * @endverbatim
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
 * J. Carwardine, C. Saunders, R. Soliday, Xuesong Jiao, H. Shang, L. Emery
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "SDDSutils.h"

#define TRUE 1
#define FALSE 0

/* Enumeration for option types */
enum option_type {
  CLO_LOWPASS,
  CLO_HIGHPASS,
  CLO_PROPORTIONAL,
  CLO_ANALOGFILTER,
  CLO_DIGITALFILTER,
  CLO_CASCADE,
  CLO_COLUMN,
  CLO_PIPE,
  CLO_VERBOSE,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "lowpass",
  "highpass",
  "proportional",
  "analogfilter",
  "digitalfilter",
  "cascade",
  "columns",
  "pipe",
  "verbose", 
  "majorOrder"
};

char *usage =
  "\nsddsdigfilter [<inputfile>] [<outputfile>] [-pipe=[input][,output]]\n"
  "  -columns=<xcol>,<ycol>[,...]\n"
  "  [options]:\n"
  "    -proportional=<gain>\n"
  "    -lowpass=<gain>,<cutoff_freq>\n"
  "    -highpass=<gain>,<cutoff_freq>\n"
  "    -digitalfilter=<sddsfile>,<a_coeff_col>,<b_coeff_col>\n"
  "       or -digitalfilter=[A,<a0>,<a1>,...][,B,<b0>,<b1>,...]\n"
  "    -analogfilter=<sddsfile>,<c_coeff_col>,<d_coeff_col>\n"
  "       or -analogfilter=[C,<c0>,<c1>,...][,D,<d0>,<d1>,...]\n"
  "    -cascade\n"
  "    -verbose\n"
  "    -majorOrder=row|column\n"
  "\nDigital filter form:\n"
  "      b0 + b1.z^-1 + ... + bn.z^-n\n"
  "    --------------------------------\n"
  "      a0 + a1.z^-1 + ... + an.z^-n\n"
  "\nAnalog filter form:\n"
  "      d0 + d1.s^1 + ... + dn.s^n\n"
  "    ------------------------------\n"
  "      c0 + c1.s^1 + ... + cn.s^n\n\n";

/**************************/

/* structure definitions */

typedef struct
{
  double gain, cutoff;
} LOWHIGH;

typedef struct
{
  double gain;
} GAIN;

typedef struct
{
  double *ACcoeff;
  double *BDcoeff;
  int64_t ncoeffs;
} ANADIG;

typedef struct
{
  void **filter;
  long *type, numFilters;
  double *time, *input, *output;
} STAGE;

long processFilterParams(char **args, long items, char *fistStr, char *nextStr, ANADIG *ADptr);
long processFilterParamsFromFile(char *filename, char *ACColumn, char *BDColumn, ANADIG *ADptr);

/* filter functions */
long lowpass_function(STAGE *stage, long filterNum, int64_t npoints);
long highpass_function(STAGE *stage, long filterNum, int64_t npoints);
long proportional_function(STAGE *stage, long filterNum, int64_t npoints);
long analog_function(STAGE *stage, long filterNum, int64_t npoints);
long digital_function(STAGE *stage, long filterNum, int64_t npoints);

/* signal processing functions */
long response(double *input, double *output, int64_t nInput, int64_t nOutput, double *Bcoeff, double *Acoeff, int64_t nBcoeffs, int64_t nAcoeffs);

void dspbiln(double *d, double *c, int64_t ln, double *b, double *a, double *work, long *error);
void dspfblt(double *d, double *c, long ln, long iband, double fln, double fhn, double *b, double *a, double *work, long *error);
double dspbfct(long *i1, long *i2);
double dpow_ri(double *ap, long *bp);

long (*filter_funct[])(STAGE *stage, long filterNum, int64_t npoints) =
  {
    lowpass_function, highpass_function, proportional_function, analog_function, digital_function};

/**************************/
long verbose;

int main(int argc, char *argv[]) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET SDDS_input, SDDS_output;
  char *inputfile, *outputfile, *xcolName, **ycolName, **ycolMatch;
  long columnsupplied, totalFilterCount, totalStages, tmpfileUsed;
  int64_t npoints;
  int32_t yColumns, ycolMatches;
  long i, j, i_arg, filterNum, stageNum, currentFilter, currentStage, filterType, error;
  unsigned long pipeFlags, majorOrderFlag;
  char outColName[256], outColDesc[256];
  double *xcol, *ycol;
  short columnMajorOrder = -1;

  LOWHIGH *LHptr;
  GAIN *Gptr;
  ANADIG *ADptr;
  STAGE *stage;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, usage);

  /* initialize flags */
  tmpfileUsed = 0;
  pipeFlags = 0;
  verbose = 0;
  columnsupplied = 0;
  inputfile = outputfile = xcolName = NULL;
  ycolName = ycolMatch = NULL;
  yColumns = ycolMatches = 0;

  /* set up stage counters etc */
  totalFilterCount = 0;
  filterNum = 0;
  stageNum = 0;

  /* allocate memory for first stage */
  stage = (STAGE *)malloc(sizeof(*stage));
  stage[0].filter = (void *)malloc(sizeof(*stage[0].filter));
  stage[0].type = (long *)malloc(sizeof(*stage[0].type));

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER, "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_LOWPASS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -lowpass option.");
        LHptr = (LOWHIGH *)malloc(sizeof(*LHptr));
        if (!get_double(&(LHptr->gain), s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -lowpass value provided.");
        if (s_arg[i_arg].n_items > 2 && !get_double(&(LHptr->cutoff), s_arg[i_arg].list[2]))
          SDDS_Bomb("Invalid -lowpass value provided.");
        stage[stageNum].filter[filterNum] = LHptr;
        stage[stageNum].type[filterNum] = CLO_LOWPASS;
        stage[stageNum].numFilters = ++filterNum;
        totalFilterCount++;
        /* allocate memory for next filter */
        stage[stageNum].filter = (void *)realloc(stage[stageNum].filter, sizeof(*stage[stageNum].filter) * (filterNum + 1));
        stage[stageNum].type = (long *)realloc(stage[stageNum].type, sizeof(*stage[stageNum].type) * (filterNum + 1));
        break;
      case CLO_HIGHPASS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -highpass option.");
        LHptr = (LOWHIGH *)malloc(sizeof(*LHptr));
        if (!get_double(&(LHptr->gain), s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -highpass value provided.");
        if (s_arg[i_arg].n_items > 2 && !get_double(&(LHptr->cutoff), s_arg[i_arg].list[2]))
          SDDS_Bomb("Invalid -highpass value provided.");
        stage[stageNum].filter[filterNum] = LHptr;
        stage[stageNum].type[filterNum] = CLO_HIGHPASS;
        stage[stageNum].numFilters = ++filterNum;
        totalFilterCount++;
        /* allocate memory for next filter */
        stage[stageNum].filter = (void *)realloc(stage[stageNum].filter, sizeof(*stage[stageNum].filter) * (filterNum + 1));
        stage[stageNum].type = (long *)realloc(stage[stageNum].type, sizeof(*stage[stageNum].type) * (filterNum + 1));
        break;
      case CLO_PROPORTIONAL:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -proportional option.");
        Gptr = (GAIN *)malloc(sizeof(*Gptr));
        if (!get_double(&(Gptr->gain), s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -proportional value provided.");
        stage[stageNum].filter[filterNum] = Gptr;
        stage[stageNum].type[filterNum] = CLO_PROPORTIONAL;
        stage[stageNum].numFilters = ++filterNum;
        totalFilterCount++;
        /* allocate memory for next filter */
        stage[stageNum].filter = (void *)realloc(stage[stageNum].filter, sizeof(*stage[stageNum].filter) * (filterNum + 1));
        stage[stageNum].type = (long *)realloc(stage[stageNum].type, sizeof(*stage[stageNum].type) * (filterNum + 1));
        break;
      case CLO_ANALOGFILTER:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -analogfilter option.");
        ADptr = (ANADIG *)malloc(sizeof(*ADptr));
        ADptr->ACcoeff = ADptr->BDcoeff = NULL;
        if (strcasecmp(s_arg[i_arg].list[1], "C") == 0 || strcasecmp(s_arg[i_arg].list[1], "D") == 0) {
          processFilterParams(s_arg[i_arg].list, s_arg[i_arg].n_items, "C", "D", ADptr);
        } else {
          if (s_arg[i_arg].n_items != 4)
            SDDS_Bomb("Invalid -analogfilter option for providing coefficient file and coefficient column names.");
          processFilterParamsFromFile(s_arg[i_arg].list[1], s_arg[i_arg].list[2], s_arg[i_arg].list[3], ADptr);
        }
        stage[stageNum].filter[filterNum] = ADptr;
        stage[stageNum].type[filterNum] = CLO_ANALOGFILTER;
        stage[stageNum].numFilters = ++filterNum;
        totalFilterCount++;
        /* allocate memory for next filter */
        stage[stageNum].filter = (void *)realloc(stage[stageNum].filter, sizeof(*stage[stageNum].filter) * (filterNum + 1));
        stage[stageNum].type = (long *)realloc(stage[stageNum].type, sizeof(*stage[stageNum].type) * (filterNum + 1));
        break;
      case CLO_DIGITALFILTER:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -digitalfilter option.");
        ADptr = (ANADIG *)malloc(sizeof(*ADptr));
        ADptr->ACcoeff = ADptr->BDcoeff = NULL;
        if (strcasecmp(s_arg[i_arg].list[1], "A") == 0 || strcasecmp(s_arg[i_arg].list[1], "B") == 0) {
          processFilterParams(s_arg[i_arg].list, s_arg[i_arg].n_items, "A", "B", ADptr);
        } else {
          if (s_arg[i_arg].n_items != 4)
            SDDS_Bomb("Invalid -digitalfilter option for providing coefficient file and coefficient column names.");
          processFilterParamsFromFile(s_arg[i_arg].list[1], s_arg[i_arg].list[2], s_arg[i_arg].list[3], ADptr);
        }
        stage[stageNum].filter[filterNum] = ADptr;
        stage[stageNum].type[filterNum] = CLO_DIGITALFILTER;
        stage[stageNum].numFilters = ++filterNum;
        totalFilterCount++;
        /* allocate memory for next filter */
        stage[stageNum].filter = (void *)realloc(stage[stageNum].filter, sizeof(*stage[stageNum].filter) * (filterNum + 1));
        stage[stageNum].type = (long *)realloc(stage[stageNum].type, sizeof(*stage[stageNum].type) * (filterNum + 1));
        break;
      case CLO_CASCADE:
        stageNum++;
        /* allocate memory for next stage */
        stage = (STAGE *)realloc(stage, sizeof(*stage) * (stageNum + 1));
        stage[stageNum].filter = (void *)malloc(sizeof(*stage[stageNum].filter));
        stage[stageNum].type = (long *)malloc(sizeof(*stage[stageNum].type));
        filterNum = 0;
        stage[stageNum].numFilters = filterNum;
        break;
      case CLO_COLUMN:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("Invalid -column option.");
        xcolName = s_arg[i_arg].list[1];
        ycolMatches = s_arg[i_arg].n_items - 2;
        ycolMatch = malloc(sizeof(*ycolMatch) * ycolMatches);
        for (i = 2; i < s_arg[i_arg].n_items; i++)
          ycolMatch[i - 2] = s_arg[i_arg].list[i];
        columnsupplied = 1;
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      default:
        fprintf(stderr, "Error (%s): unknown switch: %s\n", argv[0], s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (inputfile == NULL)
        inputfile = s_arg[i_arg].list[0];
      else if (outputfile == NULL)
        outputfile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames.");
    }
  }

  processFilenames("sddsdigfilter", &inputfile, &outputfile, pipeFlags, 1, &tmpfileUsed);
  /* check options */
  if (totalFilterCount == 0 || columnsupplied == 0) {
    fprintf(stderr, "no filter or no columns supplied.\n");
    exit(EXIT_FAILURE);
  }
  totalStages = stageNum + 1;
  if (!SDDS_InitializeInput(&SDDS_input, inputfile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_CheckColumn(&SDDS_input, xcolName, NULL, 0, stderr) != SDDS_CHECK_OKAY) {
    fprintf(stderr, "xColumn %s does not exist.\n", xcolName);
    exit(EXIT_FAILURE);
  }
  ycolName = getMatchingSDDSNames(&SDDS_input, ycolMatch, ycolMatches, &yColumns, SDDS_MATCH_COLUMN);
  for (i = 0; i < yColumns; i++) {
    if (SDDS_CheckColumn(&SDDS_input, ycolName[i], NULL, 0, stderr) != SDDS_CHECK_OKAY) {
      fprintf(stderr, "yColumn %s does not exist.\n", ycolName[i]);
      exit(EXIT_FAILURE);
    }
  }
  if (!SDDS_InitializeCopy(&SDDS_output, &SDDS_input, outputfile, "w"))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    SDDS_output.layout.data_mode.column_major = columnMajorOrder;
  else
    SDDS_output.layout.data_mode.column_major = SDDS_input.layout.data_mode.column_major;
  for (i = 0; i < yColumns; i++) {
    sprintf(outColName, "DigFiltered%s", ycolName[i]);
    sprintf(outColDesc, "Digital Filtered %s", ycolName[i]);
    if (SDDS_DefineColumn(&SDDS_output, outColName, NULL, NULL, outColDesc, NULL, SDDS_DOUBLE, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_WriteLayout(&SDDS_output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  while ((SDDS_ReadPage(&SDDS_input)) > 0) {
    if (!SDDS_CopyPage(&SDDS_output, &SDDS_input))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if ((npoints = SDDS_CountRowsOfInterest(&SDDS_input)) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (npoints) {
      /**** get input data ****/
      if (!(xcol = SDDS_GetColumnInDoubles(&SDDS_input, xcolName)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i = 0; i < yColumns; i++) {
        sprintf(outColName, "DigFiltered%s", ycolName[i]);
        if (!(ycol = SDDS_GetColumnInDoubles(&SDDS_input, ycolName[i])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        /**** PROCESS EACH STAGE ****/
        for (currentStage = 0; currentStage < totalStages; currentStage++) {
          if (verbose == 1) {
            fprintf(stdout, "STAGE %ld\n", currentStage);
          }
          /* set up time array */
          stage[currentStage].time = xcol;
          /* set up input data for current stage */
          if (currentStage == 0) {
            stage[currentStage].input = ycol;
          } else {
            stage[currentStage].input = stage[currentStage - 1].output;
          }
          /* allocate memory for output array */
          stage[currentStage].output = (double *)calloc(npoints, sizeof(*stage[currentStage].output));
          /**** PROCESS EACH FILTER IN CURRENT STAGE ****/
          for (currentFilter = 0; currentFilter < stage[currentStage].numFilters; currentFilter++) {
            if (verbose == 1) {
              fprintf(stdout, "filter %ld\n", currentFilter);
            }
            /* send data to relevant filter function */
            filterType = stage[currentStage].type[currentFilter];
            error = (*filter_funct[filterType])(&(stage[currentStage]), currentFilter, npoints);
            if (error != 0) {
              fprintf(stderr, "nyquist violation.\n");
              exit(EXIT_FAILURE);
            }
            /* end of filter loop */
          }
          /* end of stage loop */
        }
        /**** End of Processing ****/
        /* produce outputfile */
        if (!SDDS_SetColumnFromDoubles(&SDDS_output, SDDS_BY_NAME, stage[totalStages - 1].output, npoints, outColName))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        /* clean up */
        free(ycol);
        for (currentStage = 0; currentStage < totalStages; currentStage++)
          free(stage[currentStage].output);
      }
      free(xcol);
    }
    if (!SDDS_WritePage(&SDDS_output))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&SDDS_input) || !SDDS_Terminate(&SDDS_output))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < totalStages; i++) {
    for (j = 0; j < stage[i].numFilters; j++) {
      switch (stage[i].type[j]) {
      case CLO_ANALOGFILTER:
      case CLO_DIGITALFILTER:
        free(((ANADIG *)(stage[i].filter[j]))->ACcoeff);
        free(((ANADIG *)(stage[i].filter[j]))->BDcoeff);
        free((ANADIG *)(stage[i].filter[j]));
        break;
      case CLO_PROPORTIONAL:
        free((GAIN *)(stage[i].filter[j]));
        break;
      case CLO_HIGHPASS:
      case CLO_LOWPASS:
        free((LOWHIGH *)(stage[i].filter[j]));
        break;
      }
    }
    free(stage[i].filter);
    free(stage[i].type);
  }
  free(stage);
  SDDS_FreeStringArray(ycolName, yColumns);
  free(ycolName);
  free(ycolMatch);
  free_scanargs(&s_arg, argc);
  return (EXIT_SUCCESS);
  /**** end of main ****/
}

/********************************************/
long lowpass_function(STAGE *stage, long filterNum, int64_t npoints) {

  double C_coeff[2] = {1.0, 1.0};
  double D_coeff[2] = {1.0, 0.0};

  double A_coeff[2], B_coeff[2];
  double timePerPoint, normFrequency;
  double *work, *tmpoutput;

  LOWHIGH *filter_ptr;

  long error;
  int64_t i;

  filter_ptr = stage->filter[filterNum];

  timePerPoint = (stage->time[1]) - (stage->time[0]);
  if (timePerPoint < 0)
    timePerPoint *= -1.0;

  if (filter_ptr->gain == 0.0) {
    for (i = 0; i < npoints; i++) {
      stage->output[i] = 0.0;
    }
    return 0;
  }

  work = (double *)calloc(npoints, sizeof(*work));
  tmpoutput = (double *)calloc(npoints, sizeof(*tmpoutput));

  normFrequency = timePerPoint * (filter_ptr->cutoff);

  /* create digital filter coeffs */
  dspfblt(D_coeff, C_coeff, 1, 1, normFrequency, 0.0, B_coeff, A_coeff, work, &error);
  if (error != 0) {
    free(work);
    free(tmpoutput);
    return error;
  }
  /* shift A coeffs by one */
  A_coeff[1] = A_coeff[0];
  A_coeff[0] = 1.0;

  if (verbose == 1) {
    fprintf(stdout, "a0: %f\tb0: %f\n", A_coeff[0], B_coeff[0]);
    fprintf(stdout, "a1: %f\tb1: %f\n\n", A_coeff[1], B_coeff[1]);
  }

  /* apply digital filter to data */
  if (response(stage->input, tmpoutput, npoints, npoints, B_coeff, A_coeff, 2, 2)) {
    fprintf(stderr, "Invalid coeffs in lowpass filter.\n");
    free(work);
    free(tmpoutput);
    exit(EXIT_FAILURE);
  }

  /* sum this output with the existing stage output */
  for (i = 0; i < npoints; i++) {
    stage->output[i] += (filter_ptr->gain) * tmpoutput[i];
  }

  /* clean up */
  free(work);
  free(tmpoutput);

  return 0;
  /* end of lowpass_function() */
}

/************************************************/
long highpass_function(STAGE *stage, long filterNum, int64_t npoints) {

  double C_coeff[] = {1.0, 1.0};
  double D_coeff[] = {1.0, 0.0};

  double A_coeff[2], B_coeff[2];
  double timePerPoint, normFrequency;
  double *work, *tmpoutput;

  LOWHIGH *filter_ptr;

  long error;
  int64_t i;

  filter_ptr = stage->filter[filterNum];

  timePerPoint = (stage->time[1]) - (stage->time[0]);
  if (timePerPoint < 0)
    timePerPoint *= -1.0;

  if (filter_ptr->gain == 0.0) {
    for (i = 0; i < npoints; i++) {
      stage->output[i] = 0.0;
    }
    return 0;
  }

  work = (double *)calloc(npoints, sizeof(*work));
  tmpoutput = (double *)calloc(npoints, sizeof(*tmpoutput));

  normFrequency = timePerPoint * (filter_ptr->cutoff);

  /* create digital filter coeffs */
  dspfblt(D_coeff, C_coeff, 1, 2, normFrequency, 0.0, B_coeff, A_coeff, work, &error);
  if (error != 0) {
    free(work);
    free(tmpoutput);
    return error;
  }
  /* shift A coeffs by one */
  A_coeff[1] = A_coeff[0];
  A_coeff[0] = 1.0;

  if (verbose == 1) {
    fprintf(stdout, "a0: %6.6g\tb0: %6.6g\n", A_coeff[0], B_coeff[0]);
    fprintf(stdout, "a1: %6.6g\tb1: %6.6g\n\n", A_coeff[1], B_coeff[1]);
  }

  /* apply digital filter to data */
  if (response(stage->input, tmpoutput, npoints, npoints, B_coeff, A_coeff, 2, 2)) {
    fprintf(stderr, "invalid coeffs in highpass filter.\n");
    free(work);
    free(tmpoutput);
    exit(EXIT_FAILURE);
  }

  /* sum this output with the existing stage output */
  for (i = 0; i < npoints; i++) {
    stage->output[i] += (filter_ptr->gain) * tmpoutput[i];
  }

  /* clean up */
  free(work);
  free(tmpoutput);

  return 0;
  /* end of highpass_function() */
}

/*****************************************/

long digital_function(STAGE *stage, long filterNum, int64_t npoints) {

  double *tmpoutput;

  ANADIG *filter_ptr;

  int64_t i;

  filter_ptr = stage->filter[filterNum];

  tmpoutput = (double *)calloc(npoints, sizeof(*tmpoutput));

  if (verbose == 1) {
    for (i = 0; i < filter_ptr->ncoeffs; i++) {
      fprintf(stdout, "a%" PRId64 ": %6.6g\tb%" PRId64 " %6.6g\n", i, filter_ptr->ACcoeff[i], i, filter_ptr->BDcoeff[i]);
    }
  }

  /* apply digital filter to data */
  if (response(stage->input, tmpoutput, npoints, npoints, filter_ptr->BDcoeff, filter_ptr->ACcoeff, (filter_ptr->ncoeffs), (filter_ptr->ncoeffs))) {
    fprintf(stderr, "Invalid coefficients in digitalfilter.\n");
    free(tmpoutput);
    exit(EXIT_FAILURE);
  }

  /* sum this output with the existing stage output */
  for (i = 0; i < npoints; i++) {
    stage->output[i] += tmpoutput[i];
  }

  /* clean up */
  free(tmpoutput);

  return 0;
  /* end of digital_function() */
}

/*****************************************/

long analog_function(STAGE *stage, long filterNum, int64_t npoints) {
  double sampleFrequency, sampleTime;
  double *tmpoutput, *work;
  double *Bcoeff, *Acoeff;
  ANADIG *filter_ptr;

  long error;
  int64_t i;

  filter_ptr = stage->filter[filterNum];
  work = (double *)calloc(npoints, sizeof(*work));
  tmpoutput = (double *)calloc(npoints, sizeof(*tmpoutput));
  Bcoeff = (double *)calloc(filter_ptr->ncoeffs, sizeof(*Bcoeff));
  Acoeff = (double *)calloc(filter_ptr->ncoeffs, sizeof(*Acoeff));

  /* normalize analog frequency components */
  /* this is needed because the bilinear transform routine
     does not take into account the T/2 in the transformation */
  sampleTime = (stage->time[1] - stage->time[0]);
  sampleFrequency = 2.0 / sampleTime;
  if (sampleFrequency < 0.0)
    sampleFrequency *= -1.0;

  for (i = 0; i < filter_ptr->ncoeffs; i++) {
    filter_ptr->BDcoeff[i] *= pow(sampleFrequency, i);
    filter_ptr->ACcoeff[i] *= pow(sampleFrequency, i);
  }

  /* do bilinear transform */
  dspbiln(filter_ptr->BDcoeff, filter_ptr->ACcoeff, (filter_ptr->ncoeffs) - 1, Bcoeff, Acoeff, work, &error);
  if (error) {
    fprintf(stderr, "Error(%ld): invalid or all-zero analog transfer function\n", error);
    free(work);
    free(tmpoutput);
    free(Bcoeff);
    free(Acoeff);
    exit(EXIT_FAILURE);
  }

  /* shift A coeffs back by one (dspbiln assumes A0 is actually A1) */
  for (i = filter_ptr->ncoeffs - 1; i > 0; i--) {
    Acoeff[i] = Acoeff[i - 1];
  }
  Acoeff[0] = 1.0;

  if (verbose == 1) {
    for (i = 0; i < filter_ptr->ncoeffs; i++) {
      fprintf(stdout, "c%" PRId64 ": %6.6g\td%" PRId64 ": %6.6g\ta%" PRId64 ": %6.6g\tb%" PRId64 ": %6.6g\n",
              i, filter_ptr->ACcoeff[i],
              i, filter_ptr->BDcoeff[i],
              i, Acoeff[i],
              i, Bcoeff[i]);
    }
  }

  /* apply digital filter to data */
  if (response(stage->input, tmpoutput, npoints, npoints, Bcoeff, Acoeff, (filter_ptr->ncoeffs), (filter_ptr->ncoeffs))) {
    fprintf(stderr, "Invalid coefficients in analogfilter.\n");
    free(work);
    free(tmpoutput);
    free(Bcoeff);
    free(Acoeff);
    exit(EXIT_FAILURE);
  }

  /* sum this output with the existing stage output */
  for (i = 0; i < npoints; i++) {
    stage->output[i] += tmpoutput[i];
  }

  /* clean up */
  free(tmpoutput);
  free(Bcoeff);
  free(Acoeff);
  free(work);

  return 0;

  /* end of analog_function() */
}

/*****************************************/

long proportional_function(STAGE *stage, long filterNum, int64_t npoints) {

  GAIN *filter_ptr;
  int64_t i;
  double *tmpoutput;

  tmpoutput = (double *)calloc(npoints, sizeof(*tmpoutput));
  filter_ptr = stage->filter[filterNum];

  for (i = 0; i < npoints; i++) {
    tmpoutput[i] = (stage->input[i]) * (filter_ptr->gain);
  }

  /* sum this output with the existing stage output */
  for (i = 0; i < npoints; i++) {
    stage->output[i] += tmpoutput[i];
  }

  free(tmpoutput);

  return 0;
  /* end of proportional_function() */
}

/**************************************/

long response(double *input, double *output, int64_t nInput, int64_t nOutput, double *Bcoeff, double *Acoeff, int64_t nBcoeffs, int64_t nAcoeffs) {

  /* This routine adapted from Stearns & David, "Digital Signal Processing
     Algorithms in Fortran & C, Prentice Hall, 1993  */

  /* realizes the equation:  */
  /*  Acoeff[0] * output[k] =  SUM( Bcoeff[n] * input[k] ) - SUM( Acoeff[m] * output[m] ) */
  /*                            n                             m                           */
  /* where  0 < n < nBcoeffs, and 1 < m < nAcoeffs */
  /* normal situation is where Acoeff[0] = 1.0     */
  /* if there are fewer points in the input than the output, */
  /* the last point of input is continued on */

  int64_t k, n, m, max, inputPoint;

  if (Acoeff[0] == 0.0 || nAcoeffs < 1 || nBcoeffs < 1) {
    return 1;
  }

  for (k = 0; k < nOutput; k++) {
    output[k] = 0.0;

    max = (k < nBcoeffs) ? k : nBcoeffs;
    for (n = 0; n < max; n++) {
      if ((k - n) >= 0) {
        /* take the last supplied input point or earlier */
        inputPoint = ((k - n) < nInput) ? (k - n) : (nInput - 1);
        output[k] += Bcoeff[n] * input[inputPoint];
      }
    }

    max = (k < nAcoeffs) ? k : nAcoeffs;
    for (m = 1; m < max; m++) {
      if ((k - m) >= 0) {
        output[k] -= Acoeff[m] * output[k - m];
      }
    }

    output[k] /= Acoeff[0];
  }

  return 0;

  /* end of response */
}

/****************************************/

/* DSPBILN     11/13/85 */
/* This routine from Stearns & David, "Digital Signal Processing Algorithms
   in Fortran & C, Prentice Hall, 1993  */
/* CONVERTS ANALOG H(S) TO DIGITAL H(Z) VIA BILINEAR TRANSFORM */
/*      ANALOG TRANSFER FUNCTION         DIGITAL TRANSFER FUNCTION */
/*          D(L)*S**L+.....+D(0)             B(0)+......+B(L)*Z**-L */
/*    H(S)=---------------------        H(Z)=---------------------- */
/*          C(L)*S**L+.....+C(0)               1+.......+A(L)*Z**-L */
/* H(S) IS ASSUMED TO BE PRE-SCALED AND PRE-WARPED */
/* LN DSPECIFIES THE LENGTH OF THE COEFFICIENT ARRAYS */
/* FILTER ORDER L IS COMPUTED INTERNALLY */
/* WORK IS AN INTERNAL ARRAY (2D) SIZED TO MATCH COEF ARRAYS */
/* IERROR=0    NO ERRORS DETECTED IN TRANSFORMATION */
/*        1    ALL ZERO TRANSFER FUNCTION */
/*        2    INVALID TRANSFER FUNCTION; Y(K) COEF=0 */

void dspbiln(double *d, double *c, int64_t ln, double *b, double *a, double *work, long *error) {
  /* local variables */
  long zero_func;
  int64_t i, j, l, work_dim1;
  double scale, tmp;
  double atmp;

  scale = 0;

  zero_func = TRUE;
  for (i = ln; i >= 0 && zero_func; --i) {
    if (c[i] != 0.0 || d[i] != 0.0) {
      zero_func = FALSE;
    }
  }

  if (zero_func) {
    *error = 1;
    return;
  }

  work_dim1 = ln + 1;

  l = i + 1;
  for (j = 0; j <= l; ++j) {
    work[j * work_dim1] = 1.0;
  }

  tmp = 1.0;
  for (i = 1; i <= l; ++i) {
    tmp = tmp * (double)(l - i + 1) / (double)i;
    work[i] = tmp;
  }

  for (i = 1; i <= l; ++i) {
    for (j = 1; j <= l; ++j) {
      work[i + j * work_dim1] = work[i + (j - 1) * work_dim1] - work[i - 1 + j * work_dim1] - work[i - 1 + (j - 1) * work_dim1];
    }
  }

  for (i = l; i >= 0; --i) {
    b[i] = 0.0;
    atmp = 0.0;

    for (j = 0; j <= l; ++j) {
      b[i] += work[i + j * work_dim1] * d[j];
      atmp += (double)work[i + j * work_dim1] * c[j];
    }

    scale = (double)atmp;

    if (i != 0) {
      a[i - 1] = (double)atmp;
    }
  }

  if (scale == 0.0) {
    *error = 2;
    return;
  }

  b[0] /= scale;
  for (i = 1; i <= l; ++i) {
    b[i] /= scale;
    a[i - 1] /= scale;
  }

  if (l < ln) {
    for (i = l + 1; i <= ln; ++i) {
      b[i] = 0.0;
      a[i - 1] = 0.0;
    }
  }

  *error = 0;
  return;
} /* dspbiln */

/*********************************************/

/* DSPFBLT     10/26/90 */
/* This routine from Stearns & David, "Digital Signal Processing Algorithms
   in Fortran & C, Prentice Hall, 1993  */
/* CONVERTS NORMALIZED LP ANALOG H(S) TO DIGITAL H(Z) */
/*       ANALOG TRANSFER FUNCTION           DIGITAL TRANSFER FUNCTION */
/*        D(M)*S**M+.....+D(0)               B(0)+.....+B(L)*Z**-L */
/*   H(S)=--------------------          H(Z)=-------------------- */
/*        C(M)*S**M+.....+C(0)                 1+......+A(L)*Z**-L */
/* FILTER ORDER L IS COMPUTED INTERNALLY */
/* IBAND=1    LOWPASS            FLN=NORMALIZED CUTOFF IN HZ-SEC */
/*       2    HIGHPASS           FLN=NORMALIZED CUTOFF IN HZ-SEC */
/*       3    BANDPASS           FLN=LOW CUTOFF; FHN=HIGH CUTOFF */
/*       4    BANDSTOP           FLN=LOW CUTOFF; FHN=HIGH CUTOFF */
/* LN DSPECIFIES COEFFICIENT ARRAY SIZE */
/* WORK(0:LN,0:LN) IS A WORK ARRAY USED INTERNALLY */
/* RETURN IERROR=0    NO ERRORS DETECTED */
/*               1    ALL ZERO TRANSFER FUNCTION */
/*               2    DSPBILN: INVALID TRANSFER FUNCTION */
/*               3    FILTER ORDER EXCEEDS ARRAY SIZE */
/*               4    INVALID FILTER TYPE PARAMETER (IBAND) */
/*               5    INVALID CUTOFF FREQUENCY */

void dspfblt(double *d, double *c, long ln, long iband, double fln, double fhn, double *b, double *a, double *work, long *error) {

  long work_dim1, tmp_int, i, k, l, m, zero_func, ll, mm, ls;
  double tmpc, tmpd, w = 0, w1, w2, w02 = 0, tmp;

  if (iband < 1 || iband > 4) {
    *error = 4;
    return;
  }
  if ((fln <= 0.0 || fln > 0.5) || (iband >= 3 && (fln >= fhn || fhn > 0.5))) {
    *error = 5;
    return;
  }

  zero_func = TRUE;
  for (i = ln; i >= 0 && zero_func; --i) {
    if (c[i] != 0.0 || d[i] != 0.0) {
      zero_func = FALSE;
    }
  }

  if (zero_func) {
    *error = 1;
    return;
  }

  work_dim1 = ln + 1;

  m = i + 1;
  w1 = (double)tan(PI * fln);
  l = m;
  if (iband > 2) {
    l = m * 2;
    w2 = (double)tan(PI * fhn);
    w = w2 - w1;
    w02 = w1 * w2;
  }

  if (l > ln) {
    *error = 3;
    return;
  }

  switch (iband) {
  case 1:

    /* SCALING S/W1 FOR LOWPASS,HIGHPASS */

    for (mm = 0; mm <= m; ++mm) {
      d[mm] /= dpow_ri(&w1, &mm);
      c[mm] /= dpow_ri(&w1, &mm);
    }

    break;

  case 2:

    /* SUBSTITUTION OF 1/S TO GENERATE HIGHPASS (HP,BS) */

    for (mm = 0; mm <= (m / 2); ++mm) {
      tmp = d[mm];
      d[mm] = d[m - mm];
      d[m - mm] = tmp;
      tmp = c[mm];
      c[mm] = c[m - mm];
      c[m - mm] = tmp;
    }

    /* SCALING S/W1 FOR LOWPASS,HIGHPASS */

    for (mm = 0; mm <= m; ++mm) {
      d[mm] /= dpow_ri(&w1, &mm);
      c[mm] /= dpow_ri(&w1, &mm);
    }

    break;

  case 3:

    /* SUBSTITUTION OF (S**2+W0**2)/(W*S)  BANDPASS,BANDSTOP */

    for (ll = 0; ll <= l; ++ll) {
      work[ll] = 0.0;
      work[ll + work_dim1] = 0.0;
    }

    for (mm = 0; mm <= m; ++mm) {
      tmp_int = m - mm;
      tmpd = d[mm] * dpow_ri(&w, &tmp_int);
      tmpc = c[mm] * dpow_ri(&w, &tmp_int);

      for (k = 0; k <= mm; ++k) {
        ls = m + mm - (k * 2);
        tmp_int = mm - k;
        tmp = dspbfct(&mm, &mm) / (dspbfct(&k, &k) * dspbfct(&tmp_int, &tmp_int));
        work[ls] += tmpd * dpow_ri(&w02, &k) * tmp;
        work[ls + work_dim1] += tmpc * dpow_ri(&w02, &k) * tmp;
      }
    }

    for (ll = 0; ll <= l; ++ll) {
      d[ll] = work[ll];
      c[ll] = work[ll + work_dim1];
    }

    break;

  case 4:

    /* SUBSTITUTION OF 1/S TO GENERATE HIGHPASS (HP,BS) */

    for (mm = 0; mm <= (m / 2); ++mm) {
      tmp = d[mm];
      d[mm] = d[m - mm];
      d[m - mm] = tmp;
      tmp = c[mm];
      c[mm] = c[m - mm];
      c[m - mm] = tmp;
    }

    /* SUBSTITUTION OF (S**2+W0**2)/(W*S)  BANDPASS,BANDSTOP */

    for (ll = 0; ll <= l; ++ll) {
      work[ll] = 0.0;
      work[ll + work_dim1] = 0.0;
    }

    for (mm = 0; mm <= m; ++mm) {
      tmp_int = m - mm;
      tmpd = d[mm] * dpow_ri(&w, &tmp_int);
      tmpc = c[mm] * dpow_ri(&w, &tmp_int);

      for (k = 0; k <= mm; ++k) {
        ls = m + mm - (k * 2);
        tmp_int = mm - k;
        tmp = dspbfct(&mm, &mm) / (dspbfct(&k, &k) * dspbfct(&tmp_int, &tmp_int));
        work[ls] += tmpd * dpow_ri(&w02, &k) * tmp;
        work[ls + work_dim1] += tmpc * dpow_ri(&w02, &k) * tmp;
      }
    }

    for (ll = 0; ll <= l; ++ll) {
      d[ll] = work[ll];
      c[ll] = work[ll + work_dim1];
    }

    break;
  }

  dspbiln(d, c, ln, b, a, work, error);

  return;
} /* dspfblt */

/******************************************/

/* DSPBFCT     11/14/85 */
/* This routine from Stearns & David, "Digital Signal Processing Algorithms
   in Fortran & C, Prentice Hall, 1993  */
/* GENERATES (I1)!/(I1-I2)!=I1*(I1-1)*...*(I1-I2+1). */
/* NOTE: 0!=1 AND DSPBFCT(I,I)=DSPBFCT(I,I-1)=I!. */

double dspbfct(long *i1, long *i2) {
  /* Local variables */
  long i;
  double ret_val;

  ret_val = 0.0;
  if (*i1 < 0 || *i2 < 0 || *i2 > *i1) {
    return (ret_val);
  }

  ret_val = 1.0;
  for (i = *i1; i >= (*i1 - *i2 + 1); --i) {
    ret_val *= (double)i;
  }

  return (ret_val);

} /* DSPBFCT */

/********************************************/

double dpow_ri(double *ap, long *bp) {
  /* This routine from Stearns & David, "Digital Signal Processing Algorithms
     in Fortran & C, Prentice Hall, 1993  */

  double pow, x;
  long n;

  pow = 1;
  x = *ap;
  n = *bp;

  if (n != 0) {
    if (n < 0) {
      if (x == 0) {
        return (pow);
      }
      n = -n;
      x = 1 / x;
    }

    for (;;) {
      if (n & 01)
        pow *= x;

      if (n >>= 1)
        x *= x;
      else
        break;
    }
  }

  return (pow);
}

/********************************************/

long processFilterParams(char **args, long items, char *firstStr, char *nextStr, ANADIG *ADptr) {
  long a, b, aFlag, bFlag, i;

  a = b = aFlag = bFlag = 0;
  ADptr->ACcoeff = ADptr->BDcoeff = NULL;
  for (i = 1; i < items; i++) {
    if (strcasecmp(args[i], firstStr) == 0) {
      aFlag = 1;
      bFlag = 0;
    } else if (strcasecmp(args[i], nextStr) == 0) {
      bFlag = 1;
      aFlag = 0;
    } else {
      if (aFlag) {
        ADptr->ACcoeff = SDDS_Realloc(ADptr->ACcoeff, sizeof(*(ADptr->ACcoeff)) * (a + 1));
        if (!get_double(&(ADptr->ACcoeff[a]), args[i]))
          SDDS_Bomb("Invalid filter coefficient provided.");
        a++;
      }
      if (bFlag) {
        ADptr->BDcoeff = SDDS_Realloc(ADptr->BDcoeff, sizeof(*(ADptr->BDcoeff)) * (b + 1));
        if (!get_double(&(ADptr->BDcoeff[b]), args[i]))
          SDDS_Bomb("Invalid filter coefficient provided.");
        b++;
      }
    }
  }
  if (a == 0 && b == 0)
    SDDS_Bomb("No coefficients provided.");
  if (a == 0) {
    ADptr->ACcoeff = SDDS_Realloc(ADptr->ACcoeff, sizeof(*(ADptr->ACcoeff)) * (a + 1));
    ADptr->ACcoeff[0] = 1.0;
    a = 1;
  }
  if (b == 0) {
    ADptr->BDcoeff = SDDS_Realloc(ADptr->BDcoeff, sizeof(*(ADptr->BDcoeff)) * (b + 1));
    ADptr->BDcoeff[0] = 1.0;
    b = 1;
  }
  ADptr->ncoeffs = a > b ? a : b;
  if (a < ADptr->ncoeffs) {
    ADptr->ACcoeff = SDDS_Realloc(ADptr->ACcoeff, sizeof(*(ADptr->ACcoeff)) * ADptr->ncoeffs);
    for (i = a; i < ADptr->ncoeffs; i++)
      ADptr->ACcoeff[i] = 0.0;
  }
  if (b < ADptr->ncoeffs) {
    ADptr->BDcoeff = SDDS_Realloc(ADptr->BDcoeff, sizeof(*(ADptr->BDcoeff)) * ADptr->ncoeffs);
    for (i = b; i < ADptr->ncoeffs; i++)
      ADptr->BDcoeff[i] = 0.0;
  }
  return 1;
}

long processFilterParamsFromFile(char *filename, char *ACColumn, char *BDColumn, ANADIG *ADptr) {
  int64_t rows = 0;
  SDDS_DATASET SDDSdata;
  if (!SDDS_InitializeInput(&SDDSdata, filename)) {
    fprintf(stderr, "error: problem reading %s\n", filename);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (SDDS_ReadPage(&SDDSdata) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!(rows = SDDS_CountRowsOfInterest(&SDDSdata))) {
    fprintf(stderr, "error: problem counting rows in file %s\n", filename);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  ADptr->ncoeffs = rows;
  ADptr->ACcoeff = ADptr->BDcoeff = NULL;
  if (!(ADptr->ACcoeff = SDDS_GetColumnInDoubles(&SDDSdata, ACColumn))) {
    fprintf(stderr, "error: unable to read AC coefficient column.\n");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!(ADptr->BDcoeff = SDDS_GetColumnInDoubles(&SDDSdata, BDColumn))) {
    fprintf(stderr, "error: unable to read BD coefficient column.\n");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_Terminate(&SDDSdata))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 1;
}

/************************************/
