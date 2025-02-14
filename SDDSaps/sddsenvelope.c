/**
 * @file sddsenvelope.c
 * @brief Combine data from SDDS pages to create a new file with computed statistics.
 *
 * @details
 * This program processes SDDS (Self Describing Data Set) files, performing statistical computations 
 * such as maximum, minimum, mean, and others across pages of data. The resulting statistics are stored 
 * in an output SDDS file for further analysis.
 *
 * @section Usage
 * ```
 * sddsenvelope [<inputfile>] [<outputfile>]
 *              [-pipe=[input][,output]]
 *              [-nowarnings]
 *              [-maximum=<column-names>]
 *              [-minimum=<column-names>]
 *              [-cmaximum=<indep-column>,<column-names>]
 *              [-cminimum=<indep-column>,<column-names>]
 *              [-pmaximum=<indep-parameter>,<column-names>]
 *              [-pminimum=<indep-parameter>,<column-names>]
 *              [-largest=<column-names>]
 *              [-signedLargest=<column-names>]
 *              [-mean=<column-names>]
 *              [-sum=<power>,<column-names>]
 *              [-median=<column-names>]
 *              [-decilerange=<column-names>]
 *              [-percentile=<percentage>,<column-names>]
 *              [-standarddeviation=<column-names>]
 *              [-rms=<column-names>]
 *              [-sigma=<column-names>]
 *              [-slope=<indep-parameter>,<column-names>]
 *              [-intercept=<indep-parameter>,<column-names>]
 *              [-wmean=<weightColumn>,<columnNames>]
 *              [-wstandarddeviation=<weightColumn>,<columnNames>]
 *              [-wrms=<weightColumn>,<columnNames>]
 *              [-wsigma=<weightColumn>,<columnNames>]
 *              [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Option                             | Description                                                              |
 * |------------------------------------|--------------------------------------------------------------------------|
 * | `-pipe`                            | Use pipe for input/output.                                               |
 * | `-nowarnings`                      | Suppress warnings.                                                       |
 * | `-maximum`                         | Compute maximum values for specified columns.                            |
 * | `-minimum`                         | Compute minimum values for specified columns.                            |
 * | `-cmaximum`                        | Compute conditional maximum based on an independent column.              |
 * | `-cminimum`                        | Compute conditional minimum based on an independent column.              |
 * | `-pmaximum`                        | Parameter-based maximum.                             |
 * | `-pminimum`                        | Parameter-based minimum.                             |
 * | `-largest`                         | Compute largest absolute values.                                         |
 * | `-signedLargest`                   | Compute largest signed values.                                           |
 * | `-mean`                            | Compute mean values.                                                     |
 * | `-sum`                             | Compute sum raised to a specified power.                                 |
 * | `-median`                          | Compute median values.                                                   |
 * | `-decilerange`                      | Compute decile range.                                |
 * | `-percentile`                      | Compute specified percentile.                                            |
 * | `-standarddeviation`               | Compute standard deviation.                                              |
 * | `-rms`                             | Compute root mean square (RMS) values.                                   |
 * | `-sigma`                            | Compute sigma values.                                |
 * | `-slope`                           | Compute slope for linear fit.                        |
 * | `-intercept`                       | Compute intercept for linear fit.                    |
 * | `-wmean`                           | Compute weighted mean.                                                   |
 * | `-wstandarddeviation`              | Compute weighted standard deviation.                                     |
 * | `-wrms`                            | Compute weighted RMS.                                |
 * | `-wsigma`                          | Compute weighted sigma.                                                  |
 * | `-majorOrder`                      | Set data ordering for output file.                                       |
 *
 * @subsection Incompatibilities
 *   - Only one of the following options may be specified:
 *     - `-mean`
 *     - `-median`
 *     - `-percentile`
 *     - `-standarddeviation`
 *
 * @subsection SR Specific Requirements
 *   - `-percentile` requires a percentage value between 0 and 100.
 *
 * @authors
 * M. Borland, C. Saunders, R. Soliday, H. Shang
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include <ctype.h>

/* Enumeration for option types */
enum option_type {
  SET_COPY,
  SET_MAXIMA,
  SET_MINIMA,
  SET_MEANS,
  SET_SDS,
  SET_RMSS,
  SET_SUMS,
  SET_SLOPE,
  SET_INTERCEPT,
  SET_PIPE,
  SET_SIGMAS,
  SET_MEDIAN,
  SET_DRANGE,
  SET_WMEANS,
  SET_WSDS,
  SET_WRMSS,
  SET_WSIGMAS,
  SET_NOWARNINGS,
  SET_LARGEST,
  SET_PERCENTILE,
  SET_SIGNEDLARGEST,
  SET_PMAXIMA,
  SET_PMINIMA,
  SET_MAJOR_ORDER,
  SET_EXMM_MEAN,
  SET_CMAXIMA,
  SET_CMINIMA,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "copy",
  "maximum",
  "minimum",
  "mean",
  "standarddeviations",
  "rms",
  "sum",
  "slope",
  "intercept",
  "pipe",
  "sigmas",
  "median",
  "decilerange",
  "wmean",
  "wstandarddeviations",
  "wrms",
  "wsigma",
  "nowarnings",
  "largest",
  "percentile",
  "signedlargest",
  "pmaximum",
  "pminimum",
  "majorOrder",
  "exmmMean",
  "cmaximum",
  "cminimum",
};

char *optionSuffix[N_OPTIONS] = {
  "",
  "Max",
  "Min",
  "Mean",
  "StDev",
  "Rms",
  "Sum",
  "Slope",
  "Intercept",
  "",
  "Sigma",
  "Median",
  "DRange",
  "WMean",
  "WStDev",
  "WRms",
  "WSigma",
  "",
  "Largest",
  "Percentile",
  "SignedLargest",
  "PMaximum",
  "PMinimum",
  "",
  "ExmmMean",
  "CMaximum",
  "CMinimum",
};

/* this structure stores a command-line request for statistics computation */
/* columnName may contain wildcards */
typedef struct
{
  char *columnName;
  char *weightColumnName;
  long optionCode, sumPower;
  double percentile;
  char *percentileString;
  char *functionOf;
} STAT_REQUEST;

/* this structure stores data necessary for accessing/creating SDDS columns and
 * for computing a statistic
 */
typedef struct
{
  char *sourceColumn, *weightColumn, *resultColumn, *functionOf;
  long optionCode, resultIndex, sumPower;
  double percentile;
  char *percentileString;
  /* these store intermediate values during processing */
  void *copy;
  double *value1, *value2, *value3, *value4;
  double **array;
  double *sumWeight;
} STAT_DEFINITION;

long addStatRequests(STAT_REQUEST **statRequest, long requests, char **item, long items, long code, double percentile, long power, char *functionOf, long weighted, char *percentileString);
/*weighted=0, no weighted column; else, weighted statistic, the weight factor is given by
  weightedColumn*/
STAT_DEFINITION *compileStatDefinitions(SDDS_DATASET *inTable, long *stats, STAT_REQUEST *request, long requests);
long setupOutputFile(SDDS_DATASET *outTable, char *output, SDDS_DATASET *inTable, STAT_DEFINITION *stat, long stats, int64_t rows, short columnMajorOrder);
int compute_mean_exclude_min_max(double *value, double *data, long n);

static char *USAGE = "sddsenvelope [<input>] [<output>] [options]\n"
  "             [-pipe=[input][,output]]\n"
  "             [-nowarnings]\n"
  "             [-maximum=<column-names>]\n"
  "             [-minimum=<column-names>]\n"
  "             [-cmaximum=<indep-column>,<column-names>]\n"
  "             [-cminimum=<indep-column>,<column-names>]\n"
  "             [-pmaximum=<indep-parameter>,<column-names>]\n"
  "             [-pminimum=<indep-parameter>,<column-names>]\n"
  "             [-largest=<column-names>]\n"
  "             [-signedLargest=<column-names>]\n"
  "             [-mean=<column-names>]\n"
  "             [-sum=<power>,<column-names>]\n"
  "             [-median=<column-names>]\n"
  "             [-decilerange=<column-names>]\n"
  "             [-percentile=<percentage>,<column-names>]\n"
  "             [-standarddeviation=<column-names>]\n"
  "             [-rms=<column-names>]\n"
  "             [-sigma=<column-names>]\n"
  "             [-slope=<indep-parameter>,<column-names>]\n"
  "             [-intercept=<indep-parameter>,<column-names>]\n"
  "             [-wmean=<weightColumn>,<columnNames>]\n"
  "             [-wstandarddeviation=<weightColumn>,<columnNames>]\n"
  "             [-wrms=<weightColumn>,<columnNames>]\n"
  "             [-wsigma=<weightColumn>,<columnNames>]\n"
  "             [-majorOrder=row|column]\n"
  "Options:\n"
  "  -copy=<column-names>                         Copy specified columns.\n"
  "  -pipe=[input][,output]                       Use pipe for input/output.\n"
  "  -nowarnings                                  Suppress warnings.\n"
  "  -maximum=<column-names>                      Compute maximum values.\n"
  "  -minimum=<column-names>                      Compute minimum values.\n"
  "  -cmaximum=<indep-column>,<column-names>      Conditional maximum based on an independent column.\n"
  "  -cminimum=<indep-column>,<column-names>      Conditional minimum based on an independent column.\n"
  "  -pmaximum=<indep-parameter>,<column-names>   Parameter-based maximum.\n"
  "  -pminimum=<indep-parameter>,<column-names>   Parameter-based minimum.\n"
  "  -largest=<column-names>                      Compute the largest absolute values.\n"
  "  -signedLargest=<column-names>                Compute the largest signed values.\n"
  "  -mean=<column-names>                         Compute mean values.\n"
  "  -sum=<power>,<column-names>                  Compute sum with power.\n"
  "  -median=<column-names>                       Compute median values.\n"
  "  -decilerange=<column-names>                  Compute decile range.\n"
  "  -percentile=<percentage>,<column-names>      Compute specified percentile.\n"
  "  -standarddeviation=<column-names>            Compute standard deviations.\n"
  "  -rms=<column-names>                          Compute RMS values.\n"
  "  -sigma=<column-names>                        Compute sigma values.\n"
  "  -slope=<indep-parameter>,<column-names>      Compute slope for linear fit.\n"
  "  -intercept=<indep-parameter>,<column-names>  Compute intercept for linear fit.\n"
  "  -wmean=<weightColumn>,<columnNames>          Compute weighted mean.\n"
  "  -wstandarddeviation=<weightColumn>,<columnNames> Compute weighted standard deviation.\n"
  "  -wrms=<weightColumn>,<columnNames>           Compute weighted RMS.\n"
  "  -wsigma=<weightColumn>,<columnNames>         Compute weighted sigma.\n"
  "  -majorOrder=row|column                       Set major order.\n\n"
  "Processes pages from <input> to produce <output> with\n"
  "one page containing the specified quantities across pages\n"
  "for each row of the specified columns.\n"
  "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")";

int main(int argc, char **argv) {
  STAT_DEFINITION *stat;
  long stats;
  STAT_REQUEST *request;
  long requests;
  SCANNED_ARG *scanned; /* structure for scanned arguments */
  SDDS_DATASET inTable, outTable;
  long i_arg, code, power, iStat, pages, nowarnings = 0;
  int64_t i, rows, firstRows;
  char *input, *output;
  double *inputData, *otherData, indepData, *weight;
  unsigned long pipeFlags, majorOrderFlag;
  double decilePoint[2] = {10.0, 90.0}, decileResult[2];
  double percentilePoint, percentileResult;
  double percentile;
  short columnMajorOrder = -1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&scanned, argc, argv);
  if (argc < 2) {
    bomb("too few arguments", USAGE);
    exit(EXIT_FAILURE);
  }
  weight = NULL;
  input = output = NULL;
  stat = NULL;
  request = NULL;
  stats = requests = pipeFlags = 0;
  rows = firstRows = i = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scanned[i_arg].arg_type == OPTION) {
      /* process options here */
      switch (code = match_string(scanned[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_COPY:
      case SET_MINIMA:
      case SET_MAXIMA:
      case SET_LARGEST:
      case SET_SIGNEDLARGEST:
      case SET_MEANS:
      case SET_SDS:
      case SET_SIGMAS:
      case SET_RMSS:
      case SET_MEDIAN:
      case SET_DRANGE:
      case SET_EXMM_MEAN:
        if (scanned[i_arg].n_items < 2) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        requests = addStatRequests(&request, requests, scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, code, 0, 0, NULL, 0, NULL);
        break;
      case SET_WMEANS:
      case SET_WSDS:
      case SET_WRMSS:
      case SET_WSIGMAS:
        if (scanned[i_arg].n_items < 3) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        /*note here, items=scanned[i_arg].n_items-2, because the weightedColumn should be excluded */
        requests = addStatRequests(&request, requests, scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, code, 0, 0, NULL, 1, NULL);
        break;
      case SET_SUMS:
        if (scanned[i_arg].n_items < 3) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        if (sscanf(scanned[i_arg].list[1], "%ld", &power) != 1 || power < 1) {
          fprintf(stderr, "error: invalid -%s syntax--bad power in field %s\n", option[code], scanned[i_arg].list[2]);
          exit(EXIT_FAILURE);
        }
        requests = addStatRequests(&request, requests, scanned[i_arg].list + 2, scanned[i_arg].n_items - 2, code, 0, power, NULL, 0, NULL);
        break;
      case SET_PERCENTILE:
        if (scanned[i_arg].n_items < 3) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        if (sscanf(scanned[i_arg].list[1], "%lf", &percentile) != 1 || percentile < 0 || percentile > 100) {
          fprintf(stderr, "error: invalid -%s syntax--bad percentage in field %s\n", option[code], scanned[i_arg].list[1]);
          exit(EXIT_FAILURE);
        }
        requests = addStatRequests(&request, requests, scanned[i_arg].list + 2, scanned[i_arg].n_items - 2, code, percentile, 0, NULL, 0, scanned[i_arg].list[1]);
        break;
      case SET_SLOPE:
      case SET_INTERCEPT:
      case SET_PMINIMA:
      case SET_PMAXIMA:
      case SET_CMINIMA:
      case SET_CMAXIMA:
        if (scanned[i_arg].n_items < 3) {
          fprintf(stderr, "error: invalid -%s syntax\n", option[code]);
          exit(EXIT_FAILURE);
        }
        requests = addStatRequests(&request, requests, scanned[i_arg].list + 2, scanned[i_arg].n_items - 2, code, 0, 0, scanned[i_arg].list[1], 0, NULL);
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[i_arg].list + 1, scanned[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_NOWARNINGS:
        nowarnings = 1;
        break;
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
      default:
        fprintf(stderr, "error: unknown option '%s' given\n", scanned[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      /* argument is filename */
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  processFilenames("sddsenvelope", &input, &output, pipeFlags, 0, NULL);

  if (!requests)
    SDDS_Bomb("no statistics requested");

  if (!SDDS_InitializeInput(&inTable, input))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  pages = 0;
  while ((code = SDDS_ReadPage(&inTable)) > 0) {
    pages++;
    if (!(rows = SDDS_CountRowsOfInterest(&inTable)))
      SDDS_Bomb("empty data page in input file");
    if (code == 1) {
      firstRows = rows;
      if (!(stat = compileStatDefinitions(&inTable, &stats, request, requests))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      if (!setupOutputFile(&outTable, output, &inTable, stat, stats, rows, columnMajorOrder)) {
        if (SDDS_NumberOfErrors())
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        else
          fprintf(stderr, "Error setting up output file.\n");
        exit(EXIT_FAILURE);
      }
    } else if (firstRows != rows)
      SDDS_Bomb("inconsistent number of rows in input file");
    for (iStat = 0; iStat < stats; iStat++) {
      if (stat[iStat].optionCode == SET_COPY) {
        if (code == 1 && !(stat[iStat].copy = SDDS_GetColumn(&inTable, stat[iStat].sourceColumn)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        continue;
      }
      stat[iStat].copy = NULL;
      if (!(inputData = SDDS_GetColumnInDoubles(&inTable, stat[iStat].sourceColumn)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      switch (stat[iStat].optionCode) {
      case SET_MINIMA:
        if (code == 1)
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] = inputData[i];
        else
          for (i = 0; i < rows; i++)
            if (stat[iStat].value1[i] > inputData[i])
              stat[iStat].value1[i] = inputData[i];
        break;
      case SET_MAXIMA:
        if (code == 1)
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] = inputData[i];
        else
          for (i = 0; i < rows; i++)
            if (stat[iStat].value1[i] < inputData[i])
              stat[iStat].value1[i] = inputData[i];
        break;
      case SET_CMINIMA:
        /* Value from another column corresponding to minimum value in main column */
        if (!(otherData = SDDS_GetColumnInDoubles(&inTable, stat[iStat].functionOf)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (code == 1)
          for (i = 0; i < rows; i++) {
            stat[iStat].value2[i] = inputData[i];
            stat[iStat].value1[i] = otherData[i];
          }
        else
          for (i = 0; i < rows; i++)
            if (stat[iStat].value2[i] > inputData[i]) {
              stat[iStat].value2[i] = inputData[i];
              stat[iStat].value1[i] = otherData[i];
            }
        free(otherData);
        break;
      case SET_CMAXIMA:
        /* Value from another column corresponding to maximum value in main column */
        if (!(otherData = SDDS_GetColumnInDoubles(&inTable, stat[iStat].functionOf)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        if (code == 1)
          for (i = 0; i < rows; i++) {
            stat[iStat].value2[i] = inputData[i];
            stat[iStat].value1[i] = otherData[i];
          }
        else
          for (i = 0; i < rows; i++)
            if (stat[iStat].value2[i] < inputData[i]) {
              stat[iStat].value2[i] = inputData[i];
              stat[iStat].value1[i] = otherData[i];
            }
        free(otherData);
        break;
      case SET_LARGEST:
        if (code == 1)
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] = fabs(inputData[i]);
        else
          for (i = 0; i < rows; i++)
            if (stat[iStat].value1[i] < fabs(inputData[i]))
              stat[iStat].value1[i] = fabs(inputData[i]);
        break;
      case SET_SIGNEDLARGEST:
        if (code == 1)
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] = inputData[i];
        else
          for (i = 0; i < rows; i++)
            if (fabs(stat[iStat].value1[i]) < fabs(inputData[i]))
              stat[iStat].value1[i] = inputData[i];
        break;
      case SET_MEANS:
        if (code == 1)
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] = inputData[i];
        else
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] += inputData[i];
        break;
      case SET_WMEANS:
        if (!(weight = SDDS_GetColumnInDoubles(&inTable, stat[iStat].weightColumn)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (i = 0; i < rows; i++) {
          stat[iStat].sumWeight[i] += weight[i];
          stat[iStat].value1[i] += inputData[i] * weight[i];
        }
        free(weight);
        break;
      case SET_SDS:
      case SET_SIGMAS:
        if (code == 1)
          for (i = 0; i < rows; i++) {
            stat[iStat].value1[i] = inputData[i];
            stat[iStat].value2[i] = inputData[i] * inputData[i];
          }
        else
          for (i = 0; i < rows; i++) {
            stat[iStat].value1[i] += inputData[i];
            stat[iStat].value2[i] += inputData[i] * inputData[i];
          }
        break;
      case SET_WSDS:
      case SET_WSIGMAS:
        if (!(weight = SDDS_GetColumnInDoubles(&inTable, stat[iStat].weightColumn)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (i = 0; i < rows; i++) {
          stat[iStat].sumWeight[i] += weight[i];
          stat[iStat].value1[i] += inputData[i] * weight[i];
          stat[iStat].value2[i] += inputData[i] * inputData[i] * weight[i];
        }
        free(weight);
        break;
      case SET_RMSS:
        if (code == 1)
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] = inputData[i] * inputData[i];
        else
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] += inputData[i] * inputData[i];
        break;
      case SET_WRMSS:
        if (!(weight = SDDS_GetColumnInDoubles(&inTable, stat[iStat].weightColumn)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (i = 0; i < rows; i++) {
          stat[iStat].sumWeight[i] += weight[i];
          stat[iStat].value1[i] += inputData[i] * inputData[i] * weight[i];
        }
        free(weight);
        break;
      case SET_SUMS:
        if (code == 1)
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] = ipow(inputData[i], stat[iStat].sumPower);
        else
          for (i = 0; i < rows; i++)
            stat[iStat].value1[i] += ipow(inputData[i], stat[iStat].sumPower);
        break;
      case SET_PMINIMA:
        if (!SDDS_GetParameterAsDouble(&inTable, stat[iStat].functionOf, &indepData)) {
          fprintf(stderr, "error:  unable to get value of parameter %s\n", stat[iStat].functionOf);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (code == 1)
          for (i = 0; i < rows; i++) {
            stat[iStat].value2[i] = inputData[i];
            stat[iStat].value1[i] = indepData;
          }
        else
          for (i = 0; i < rows; i++) {
            if (stat[iStat].value2[i] > inputData[i]) {
              stat[iStat].value2[i] = inputData[i];
              stat[iStat].value1[i] = indepData;
            }
          }
        break;
      case SET_PMAXIMA:
        if (!SDDS_GetParameterAsDouble(&inTable, stat[iStat].functionOf, &indepData)) {
          fprintf(stderr, "error:  unable to get value of parameter %s\n", stat[iStat].functionOf);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        if (code == 1)
          for (i = 0; i < rows; i++) {
            stat[iStat].value2[i] = inputData[i];
            stat[iStat].value1[i] = indepData;
          }
        else
          for (i = 0; i < rows; i++) {
            if (stat[iStat].value2[i] < inputData[i]) {
              stat[iStat].value2[i] = inputData[i];
              stat[iStat].value1[i] = indepData;
            }
          }
        break;
      case SET_SLOPE:
      case SET_INTERCEPT:
        if (!SDDS_GetParameterAsDouble(&inTable, stat[iStat].functionOf, &indepData)) {
          fprintf(stderr, "error:  unable to get value of parameter %s\n", stat[iStat].functionOf);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
        /* linear fit:
           y = a + bx
           a = (S x^2 Sy - S x S xy)/D
           b = (N S xy  - Sx Sy)/D
           D = N S x^2 - (S x)^2
        */
        if (code == 1)
          for (i = 0; i < rows; i++) {
            stat[iStat].value1[i] = indepData;                /* Sum x   */
            stat[iStat].value2[i] = indepData * indepData;    /* Sum x^2 */
            stat[iStat].value3[i] = inputData[i];             /* Sum y   */
            stat[iStat].value4[i] = indepData * inputData[i]; /* Sum xy  */
          }
        else
          for (i = 0; i < rows; i++) {
            stat[iStat].value1[i] += indepData;
            stat[iStat].value2[i] += indepData * indepData;
            stat[iStat].value3[i] += inputData[i];
            stat[iStat].value4[i] += indepData * inputData[i];
          }
        break;
      case SET_MEDIAN:
      case SET_DRANGE:
      case SET_PERCENTILE:
      case SET_EXMM_MEAN:
        if (code == 1)
          for (i = 0; i < rows; i++) {
            stat[iStat].array[i] = tmalloc(sizeof(*stat[iStat].array[i]));
            stat[iStat].array[i][pages - 1] = inputData[i];
          }
        else {
          for (i = 0; i < rows; i++) {
            stat[iStat].array[i] = SDDS_Realloc(stat[iStat].array[i], sizeof(*stat[iStat].array[i]) * pages);
            stat[iStat].array[i][pages - 1] = inputData[i];
          }
        }
        break;
      default:
        SDDS_Bomb("invalid statistic code (accumulation loop)");
        break;
      }
      free(inputData);
    }
  }
  if (pages == 0)
    SDDS_Bomb("no pages in input");
  for (iStat = 0; iStat < stats; iStat++) {
    switch (stat[iStat].optionCode) {
    case SET_COPY:
    case SET_MINIMA:
    case SET_MAXIMA:
    case SET_PMINIMA:
    case SET_PMAXIMA:
    case SET_CMINIMA:
    case SET_CMAXIMA:
    case SET_LARGEST:
    case SET_SIGNEDLARGEST:
    case SET_SUMS:
      break;
    case SET_MEANS:
      for (i = 0; i < rows; i++)
        stat[iStat].value1[i] /= pages;
      break;
    case SET_WMEANS:
      for (i = 0; i < rows; i++)
        if (stat[iStat].sumWeight[i])
          stat[iStat].value1[i] /= stat[iStat].sumWeight[i];
        else {
          if (!nowarnings)
            fprintf(stderr, "warning: the total weight for the %" PRId64 "th row of %s is zero.\n", i + 1, stat[iStat].sourceColumn);
          stat[iStat].value1[i] = DBL_MAX;
        }
      break;
    case SET_SDS:
      if (pages < 2)
        stat[iStat].value1[i] = DBL_MAX;
      else
        for (i = 0; i < rows; i++) {
          double tmp1;
          if ((tmp1 = stat[iStat].value2[i] / pages - sqr(stat[iStat].value1[i] / pages)) <= 0)
            stat[iStat].value1[i] = 0;
          else
            stat[iStat].value1[i] = sqrt(tmp1 * pages / (pages - 1.0));
        }
      break;
    case SET_WSDS:
      if (pages < 2)
        stat[iStat].value1[i] = DBL_MAX;
      else
        for (i = 0; i < rows; i++) {
          double tmp1;
          if (stat[iStat].sumWeight[i]) {
            if ((tmp1 = stat[iStat].value2[i] / stat[iStat].sumWeight[i] - sqr(stat[iStat].value1[i] / stat[iStat].sumWeight[i])) <= 0)
              stat[iStat].value1[i] = 0;
            else
              stat[iStat].value1[i] = sqrt(tmp1 * pages / (pages - 1.0));
          } else {
            if (!nowarnings)
              fprintf(stderr, "Warning, the total weight for the %" PRId64 "th row of %s is zero.\n", i + 1, stat[iStat].sourceColumn);
            stat[iStat].value1[i] = DBL_MAX;
          }
        }
      break;
    case SET_SIGMAS:
      if (pages < 2)
        stat[iStat].value1[i] = DBL_MAX;
      else
        for (i = 0; i < rows; i++) {
          double tmp1;
          if ((tmp1 = stat[iStat].value2[i] / pages - sqr(stat[iStat].value1[i] / pages)) <= 0)
            stat[iStat].value1[i] = 0;
          else
            stat[iStat].value1[i] = sqrt(tmp1 / (pages - 1.0));
        }
      break;
    case SET_WSIGMAS:
      if (pages < 2)
        stat[iStat].value1[i] = DBL_MAX;
      else
        for (i = 0; i < rows; i++) {
          double tmp1;
          if (stat[iStat].sumWeight[i]) {
            if ((tmp1 = stat[iStat].value2[i] / stat[iStat].sumWeight[i] - sqr(stat[iStat].value1[i] / stat[iStat].sumWeight[i])) <= 0)
              stat[iStat].value1[i] = 0;
            else
              stat[iStat].value1[i] = sqrt(tmp1 / (pages - 1.0));
          } else {
            if (!nowarnings)
              fprintf(stderr, "Warning, the total weight for the %" PRId64 "th row of %s is zero.\n", i + 1, stat[iStat].sourceColumn);
            stat[iStat].value1[i] = DBL_MAX;
          }
        }
      break;
    case SET_RMSS:
      for (i = 0; i < rows; i++)
        stat[iStat].value1[i] = sqrt(stat[iStat].value1[i] / pages);
      break;
    case SET_WRMSS:
      for (i = 0; i < rows; i++) {
        if (stat[iStat].sumWeight[i])
          stat[iStat].value1[i] = sqrt(stat[iStat].value1[i] / stat[iStat].sumWeight[i]);
        else {
          if (!nowarnings)
            fprintf(stderr, "Warning, the total weight for the %" PRId64 "th row of %s is zero.\n", i + 1, stat[iStat].sourceColumn);
          stat[iStat].value1[i] = DBL_MAX;
        }
      }
      break;
    case SET_SLOPE:
      for (i = 0; i < rows; i++) {
        double D;
        D = pages * stat[iStat].value2[i] - stat[iStat].value1[i] * stat[iStat].value1[i];
        stat[iStat].value1[i] = (pages * stat[iStat].value4[i] - stat[iStat].value1[i] * stat[iStat].value3[i]) / D;
      }
      break;
    case SET_INTERCEPT:
      for (i = 0; i < rows; i++) {
        double D;
        D = pages * stat[iStat].value2[i] - stat[iStat].value1[i] * stat[iStat].value1[i];
        stat[iStat].value1[i] = (stat[iStat].value2[i] * stat[iStat].value3[i] - stat[iStat].value1[i] * stat[iStat].value4[i]) / D;
      }
      break;
    case SET_MEDIAN:
      for (i = 0; i < rows; i++) {
        compute_median(&stat[iStat].value1[i], stat[iStat].array[i], pages);
      }
      break;
    case SET_DRANGE:
      for (i = 0; i < rows; i++) {
        if (!compute_percentiles(decileResult, decilePoint, 2, stat[iStat].array[i], pages))
          stat[iStat].value1[i] = 0;
        else
          stat[iStat].value1[i] = decileResult[1] - decileResult[0];
      }
      break;
    case SET_PERCENTILE:
      percentilePoint = stat[iStat].percentile;
      for (i = 0; i < rows; i++) {
        if (!compute_percentiles(&percentileResult, &percentilePoint, 1, stat[iStat].array[i], pages))
          stat[iStat].value1[i] = 0;
        else
          stat[iStat].value1[i] = percentileResult;
      }
      break;
    case SET_EXMM_MEAN:
      for (i = 0; i < rows; i++)
        if (!compute_mean_exclude_min_max(&(stat[iStat].value1[i]), stat[iStat].array[i], pages))
          stat[iStat].value1[i] = 0;
      break;
    default:
      SDDS_Bomb("invalid statistic code (final loop)");
      break;
    }
    if (stat[iStat].optionCode == SET_COPY) {
      if (!SDDS_SetColumn(&outTable, SDDS_SET_BY_NAME, stat[iStat].copy, rows, stat[iStat].resultColumn)) {
        fprintf(stderr, "error setting column values for column %s\n", stat[iStat].resultColumn);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    } else if (!SDDS_SetColumnFromDoubles(&outTable, SDDS_SET_BY_NAME, stat[iStat].value1, rows, stat[iStat].resultColumn)) {
      fprintf(stderr, "error setting column values for column %s\n", stat[iStat].resultColumn);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (stat[iStat].value1)
      free(stat[iStat].value1);
    if (stat[iStat].value2)
      free(stat[iStat].value2);
    if (stat[iStat].value3)
      free(stat[iStat].value3);
    if (stat[iStat].value4)
      free(stat[iStat].value4);
    if (stat[iStat].copy)
      free(stat[iStat].copy);
    if (stat[iStat].array) {
      for (i = 0; i < rows; i++) {
        free(stat[iStat].array[i]);
      }
      free(stat[iStat].array);
    }
    if (stat[iStat].sumWeight)
      free(stat[iStat].sumWeight);
    free(stat[iStat].sourceColumn);
    free(stat[iStat].resultColumn);
    stat[iStat].value1 = stat[iStat].value2 = stat[iStat].value3 = stat[iStat].value4 = NULL;
    stat[iStat].copy = NULL;
    stat[iStat].array = NULL;
  }
  free(stat);
  if (!SDDS_WritePage(&outTable) || !SDDS_Terminate(&inTable) || !SDDS_Terminate(&outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return EXIT_SUCCESS;
}

long addStatRequests(STAT_REQUEST **statRequest, long requests, char **item, long items, long code, double percentile, long power, char *functionOf, long weighted, char *percentileString) {
  long i;
  /*weighted factor should be either 0 or 1 */
  if (weighted != 0 && weighted != 1)
    SDDS_Bomb("addStatRequests: weighted parameter should be either 0 or 1");
  if (code == SET_PERCENTILE && (!percentileString || !strlen(percentileString))) {
    fprintf(stderr, "Percentile specification is incorrect: percentile=%e, percentileString=%s\n", percentile, percentileString ? percentileString : "NULL");
    exit(EXIT_FAILURE);
  }
  *statRequest = SDDS_Realloc(*statRequest, sizeof(**statRequest) * (requests + items - weighted));
  for (i = 0; i < items - weighted; i++) {
    if (weighted)
      (*statRequest)[requests + i].weightColumnName = item[0];
    else
      (*statRequest)[requests + i].weightColumnName = NULL;
    (*statRequest)[i + requests].columnName = item[i + weighted];
    (*statRequest)[i + requests].optionCode = code;
    (*statRequest)[i + requests].sumPower = power;
    (*statRequest)[i + requests].percentile = percentile;
    (*statRequest)[i + requests].percentileString = percentileString;
    (*statRequest)[i + requests].functionOf = functionOf;
  }

  return items + requests - weighted;
}

STAT_DEFINITION *compileStatDefinitions(SDDS_DATASET *inTable, long *stats, STAT_REQUEST *request, long requests) {
  STAT_DEFINITION *stat;
  long iReq, iStat, iName;
  int32_t columnNames;
  char s[SDDS_MAXLINE];
  char **columnName;

  *stats = iStat = 0;
  stat = tmalloc(sizeof(*stat) * requests);
  for (iReq = 0; iReq < requests; iReq++) {
    if (iStat >= *stats)
      stat = SDDS_Realloc(stat, sizeof(*stat) * (*stats += 10));
    if (!has_wildcards(request[iReq].columnName)) {
      if (SDDS_GetColumnIndex(inTable, request[iReq].columnName) < 0) {
        sprintf(s, "error: column %s not found input file", request[iReq].columnName);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      stat[iStat].weightColumn = request[iReq].weightColumnName;
      stat[iStat].sourceColumn = request[iReq].columnName;
      stat[iStat].optionCode = request[iReq].optionCode;
      stat[iStat].percentile = request[iReq].percentile;
      stat[iStat].percentileString = request[iReq].percentileString;
      stat[iStat].sumPower = request[iReq].sumPower;
      stat[iStat].value1 = stat[iStat].value2 = stat[iStat].value3 = stat[iStat].value4 = NULL;
      stat[iStat].array = NULL;
      stat[iStat].copy = NULL;
      stat[iStat].sumWeight = NULL;
      if ((stat[iStat].functionOf = request[iReq].functionOf)) {
        if (stat[iStat].optionCode != SET_CMAXIMA && stat[iStat].optionCode != SET_CMINIMA) {
          if (SDDS_GetParameterIndex(inTable, request[iReq].functionOf) < 0) {
            sprintf(s, "error: parameter %s not found input file (1)", request[iReq].functionOf);
            SDDS_SetError(s);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        } else {
          if (SDDS_GetColumnIndex(inTable, request[iReq].functionOf) < 0) {
            sprintf(s, "error: column %s not found input file (1)", request[iReq].functionOf);
            SDDS_SetError(s);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
      }
      iStat++;
    } else {
      SDDS_SetColumnFlags(inTable, 0);
      if (!SDDS_SetColumnsOfInterest(inTable, SDDS_MATCH_STRING, request[iReq].columnName, SDDS_OR))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(columnName = SDDS_GetColumnNames(inTable, &columnNames))) {
        sprintf(s, "no columns selected for wildcard sequence %s", request[iReq].columnName);
        SDDS_SetError(s);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (iStat + columnNames > *stats)
        stat = SDDS_Realloc(stat, sizeof(*stat) * (*stats = iStat + columnNames + 10));
      for (iName = 0; iName < columnNames; iName++) {
        stat[iStat + iName].weightColumn = request[iReq].weightColumnName;
        stat[iStat + iName].sourceColumn = columnName[iName];
        stat[iStat + iName].optionCode = request[iReq].optionCode;
        stat[iStat + iName].sumPower = request[iReq].sumPower;
        stat[iStat + iName].percentile = request[iReq].percentile;
        stat[iStat + iName].percentileString = request[iReq].percentileString;
        stat[iStat + iName].value1 = stat[iStat + iName].value2 = stat[iStat + iName].value3 = stat[iStat + iName].value4 = NULL;
        stat[iStat + iName].array = NULL;
        stat[iStat + iName].copy = NULL;
        stat[iStat + iName].sumWeight = NULL;
        if ((stat[iStat + iName].functionOf = request[iReq].functionOf) && iName == 0) {
          if (stat[iStat + iName].optionCode != SET_CMAXIMA && stat[iStat + iName].optionCode != SET_CMINIMA) {
            if (SDDS_GetParameterIndex(inTable, request[iReq].functionOf) < 0) {
              sprintf(s, "error: parameter %s not found input file (2)", request[iReq].functionOf);
              SDDS_SetError(s);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          } else {
            if (SDDS_GetColumnIndex(inTable, request[iReq].functionOf) < 0) {
              sprintf(s, "error: column %s not found input file (2)", request[iReq].functionOf);
              SDDS_SetError(s);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
        }
      }
      iStat += columnNames;
      free(columnName);
    }
  }

  *stats = iStat;
  for (iStat = 0; iStat < *stats; iStat++) {
    switch (stat[iStat].optionCode) {
    case SET_COPY:
      strcpy(s, stat[iStat].sourceColumn);
      break;
    case SET_SUMS:
      if (stat[iStat].sumPower == 1)
        sprintf(s, "%s%s", stat[iStat].sourceColumn, optionSuffix[stat[iStat].optionCode]);
      else
        sprintf(s, "%s%ld%s", stat[iStat].sourceColumn, stat[iStat].sumPower, optionSuffix[stat[iStat].optionCode]);
      break;
    case SET_PERCENTILE:
      sprintf(s, "%s%s%s", stat[iStat].sourceColumn, stat[iStat].percentileString, optionSuffix[stat[iStat].optionCode]);
      break;
    case SET_PMAXIMA:
    case SET_PMINIMA:
      sprintf(s, "%s%s%s", stat[iStat].functionOf, optionSuffix[stat[iStat].optionCode], stat[iStat].sourceColumn);
      break;
    case SET_CMAXIMA:
    case SET_CMINIMA:
      sprintf(s, "%s%s%s", stat[iStat].functionOf, optionSuffix[stat[iStat].optionCode], stat[iStat].sourceColumn);
      break;
    default:
      sprintf(s, "%s%s", stat[iStat].sourceColumn, optionSuffix[stat[iStat].optionCode]);
      break;
    }
    if (!SDDS_CopyString(&stat[iStat].resultColumn, s))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  return stat;
}

long setupOutputFile(SDDS_DATASET *outTable, char *output, SDDS_DATASET *inTable, STAT_DEFINITION *stat, long stats, int64_t rows, short columnMajorOrder) {
  long column;
  char s[SDDS_MAXLINE], *symbol, *symbol1, *units1;

  if (!SDDS_InitializeOutput(outTable, SDDS_BINARY, 0, NULL, "sddsenvelope output", output))
    return 0;
  if (columnMajorOrder != -1)
    outTable->layout.data_mode.column_major = columnMajorOrder;
  else
    outTable->layout.data_mode.column_major = inTable->layout.data_mode.column_major;
  for (column = 0; column < stats; column++) {
    stat[column].value1 = calloc(sizeof(*stat[column].value1), rows);
    stat[column].value2 = stat[column].value3 = stat[column].value4 = NULL;
    if (stat[column].optionCode == SET_SDS || stat[column].optionCode == SET_SIGMAS ||
        stat[column].optionCode == SET_WSDS || stat[column].optionCode == SET_WSIGMAS ||
        stat[column].optionCode == SET_PMINIMA || stat[column].optionCode == SET_PMAXIMA ||
        stat[column].optionCode == SET_CMINIMA || stat[column].optionCode == SET_CMAXIMA)
      stat[column].value2 = calloc(sizeof(*stat[column].value2), rows);
    if (stat[column].optionCode == SET_INTERCEPT || stat[column].optionCode == SET_SLOPE) {
      stat[column].value2 = malloc(sizeof(*stat[column].value2) * rows);
      stat[column].value3 = malloc(sizeof(*stat[column].value3) * rows);
      stat[column].value4 = malloc(sizeof(*stat[column].value4) * rows);
    }
    if (stat[column].optionCode == SET_WSDS || stat[column].optionCode == SET_WSIGMAS ||
        stat[column].optionCode == SET_WRMSS || stat[column].optionCode == SET_WMEANS)
      stat[column].sumWeight = calloc(sizeof(*stat[column].sumWeight), rows);
    if (stat[column].optionCode == SET_MEDIAN || stat[column].optionCode == SET_DRANGE ||
        stat[column].optionCode == SET_PERCENTILE || stat[column].optionCode == SET_EXMM_MEAN) {
      stat[column].array = tmalloc(sizeof(*stat[column].array) * rows);
    }
    if (!SDDS_TransferColumnDefinition(outTable, inTable, stat[column].sourceColumn, stat[column].resultColumn)) {
      sprintf(s, "Problem transferring definition of column %s to %s\n", stat[column].sourceColumn, stat[column].resultColumn);
      SDDS_SetError(s);
      return 0;
    }
    if (SDDS_ChangeColumnInformation(outTable, "description", NULL, SDDS_SET_BY_NAME, stat[column].resultColumn) != SDDS_STRING ||
        SDDS_GetColumnInformation(outTable, "symbol", &symbol, SDDS_BY_NAME, stat[column].resultColumn) != SDDS_STRING) {
      fprintf(stderr, "Error: problem setting description for column %s\n", stat[column].resultColumn);
      return 0;
    }
    if (stat[column].optionCode > 0) {
      if (SDDS_ChangeColumnInformation(outTable, "type", "double", SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, stat[column].resultColumn) != SDDS_LONG) {
        fprintf(stderr, "Error: problem setting type for column %s\n", stat[column].resultColumn);
        return 0;
      }
    }
    if (!symbol)
      SDDS_CopyString(&symbol, stat[column].sourceColumn);
    switch (stat[column].optionCode) {
    case SET_COPY:
      strcpy(s, symbol);
      break;
    case SET_SUMS:
      if (stat[column].sumPower == 1)
        sprintf(s, "%s[%s]", optionSuffix[stat[column].optionCode], symbol);
      else
        sprintf(s, "%s[%s$a%ld$n]", optionSuffix[stat[column].optionCode], symbol, stat[column].sumPower);
      break;
    case SET_PERCENTILE:
      sprintf(s, "%s[%s,%g]", optionSuffix[stat[column].optionCode], symbol, stat[column].percentile);
      break;
    case SET_PMINIMA:
    case SET_PMAXIMA:
      if (SDDS_GetParameterInformation(inTable, "symbol", &symbol1, SDDS_BY_NAME, stat[column].functionOf) != SDDS_STRING ||
          !symbol1 ||
          !strlen(symbol1))
        symbol1 = stat[column].functionOf;
      sprintf(s, "%s[%s:%s]", optionSuffix[stat[column].optionCode], symbol, symbol1);
      if (SDDS_GetParameterInformation(inTable, "units", &units1, SDDS_BY_NAME, stat[column].functionOf) != SDDS_STRING)
        return 0;
      if (units1) {
        if (SDDS_ChangeColumnInformation(outTable, "units", units1, SDDS_BY_NAME, stat[column].resultColumn) != SDDS_STRING) {
          fprintf(stderr, "Error: problem setting units for column %s (1)\n", stat[column].resultColumn);
          return 0;
        }
      } else if (SDDS_ChangeColumnInformation(outTable, "units", "", SDDS_BY_NAME, stat[column].resultColumn) != SDDS_STRING) {
        fprintf(stderr, "Error: problem setting units for column %s (2)\n", stat[column].resultColumn);
        return 0;
      }
      break;
    case SET_CMINIMA:
    case SET_CMAXIMA:
      if (SDDS_GetColumnInformation(inTable, "symbol", &symbol1, SDDS_BY_NAME, stat[column].functionOf) != SDDS_STRING ||
          !symbol1 ||
          !strlen(symbol1))
        symbol1 = stat[column].functionOf;
      sprintf(s, "%s[%s:%s]", optionSuffix[stat[column].optionCode], symbol, symbol1);
      if (SDDS_GetColumnInformation(inTable, "units", &units1, SDDS_BY_NAME, stat[column].resultColumn) != SDDS_STRING)
        return 0;
      if (units1) {
        if (SDDS_ChangeColumnInformation(outTable, "units", units1, SDDS_BY_NAME, stat[column].resultColumn) != SDDS_STRING) {
          fprintf(stderr, "Error: problem setting units for column %s\n", stat[column].resultColumn);
          return 0;
        }
      } else if (SDDS_ChangeColumnInformation(outTable, "units", "", SDDS_BY_NAME, stat[column].resultColumn) != SDDS_STRING) {
        fprintf(stderr, "Error: problem setting units for column %s\n", stat[column].resultColumn);
        return 0;
      }
      break;
    case SET_INTERCEPT:
    case SET_SLOPE:
      if (SDDS_GetParameterInformation(inTable, "symbol", &symbol1, SDDS_BY_NAME, stat[column].functionOf) != SDDS_STRING ||
          !symbol1 ||
          !strlen(symbol1))
        symbol1 = stat[column].functionOf;
      sprintf(s, "%s[%s:%s]", optionSuffix[stat[column].optionCode], symbol, symbol1);
      break;
    default:
      sprintf(s, "%s[%s]", optionSuffix[stat[column].optionCode], symbol);
      break;
    }
    free(symbol);
    if (SDDS_ChangeColumnInformation(outTable, "symbol", s, SDDS_BY_NAME, stat[column].resultColumn) != SDDS_STRING) {
      fprintf(stderr, "Error: problem setting symbol for column %s\n", stat[column].resultColumn);
      return 0;
    }
  }
  if (!SDDS_WriteLayout(outTable) || !SDDS_StartPage(outTable, rows))
    return 0;
  return 1;
}

int compute_mean_exclude_min_max(double *value, double *data, long n) {
  double sum = 0;
  long i, count = 0;
  double min, max;
  max = -(min = DBL_MAX);
  if (n <= 0 || !data || !value)
    return 0;
  for (i = 0; i < n; i++) {
    if (min > data[i])
      min = data[i];
    if (max < data[i])
      max = data[i];
  }
  for (i = 0; i < n; i++) {
    if (data[i] == min || data[i] == max)
      continue;
    count++;
    sum += data[i];
  }
  if (count == 0)
    *value = min;
  else
    *value = sum / count;
  return 1;
}
