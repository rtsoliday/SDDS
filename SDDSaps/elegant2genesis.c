/**
 * @file elegant2genesis.c
 * @brief Converts the output file from Elegant into the beamline file format used by Genesis.
 *
 * @section Introduction
 * This program processes beam data generated by Elegant, applies various transformations
 * and filters, and outputs the result in a format compatible with Genesis. Features include
 * momentum tail removal, steering beam centroids, and customizable slicing based on specified
 * wavelengths or slices.
 *
 * @section Usage
 * ```
 * elegant2genesis [<input>] [<output>]
 *                 [-pipe=[in][,out]] 
 *                 [-textOutput]
 *                 [-totalCharge=<charge-in-Coulombs>]
 *                 [-chargeParameter=<name>]
 *                 [-wavelength=<meters>]
 *                 [-slices=<integer>]
 *                 [-steer] 
 *                 [-removePTails=deltaLimit=<value>[,fit][,beamOutput=<filename>]]
 *                 [-reverseOrder] 
 *                 [-localFit]
 * ```
 *
 * @section Options
 * | Option                                 | Description                                                                 |
 * |----------------------------------------|-----------------------------------------------------------------------------|
 * | `-pipe`                                | Set up pipe communication for input and/or output.                         |
 * | `-textOutput`                          | Make the output file a text file instead of an SDDS file.                  |
 * | `-totalCharge`                         | Specify the total charge in Coulombs.                                       |
 * | `-chargeParameter`                     | Specify the name of a parameter in the input file that gives the total charge. |
 * | `-wavelength`                          | Specify the wavelength of light in meters.                                  |
 * | `-slices`                              | Specify the number of slices to divide the beam into.                       |
 * | `-steer`                               | Force the x, x', y, and y' centroids for the whole beam to zero.            |
 * | `-removePTails`                        | Remove the momentum tails from the beam |
 * | `-reverseOrder`                        | Output the data for the tail of the beam first instead of the head.         |
 * | `-localFit`                            | Perform a local linear fit of momentum vs time for each slice, removing a contribution to the energy spread. |
 *
 * @section IC Incompatible Options
 * The following options are mutually exclusive:
 * - `-wavelength` and `-slices` cannot be used together.
 *
 * @section DD Detailed Description
 * This program reads beam data from an Elegant output file, processes it according to the specified options, 
 * and generates an output file in the Genesis-compatible beamline format. The processing includes:
 * - **Momentum Tail Removal:** Using the `-removePTails` option to eliminate particles with momentum deviations beyond a specified limit.
 * - **Steering Centroids:** Adjusting the centroids of the beam to zero using the `-steer` option.
 * - **Slicing:** Dividing the beam into slices based on wavelength or a specified number of slices.
 * - **Local Linear Fit:** Applying a local linear fit to momentum vs time data for each slice with the `-localFit` option.
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
 * R. Soliday, M. Borland
 */

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"

/* Enumeration for option types */
enum option_type {
  SET_WAVELENGTH,
  SET_SLICES,
  SET_TOTALCHARGE,
  SET_TEXTOUTPUT,
  SET_STEER,
  SET_CHARGEPARAMETER,
  SET_PIPE,
  SET_REMPTAILS,
  SET_REVERSEORDER,
  SET_LOCAL_FIT,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "wavelength",
  "slices",
  "totalcharge",
  "textoutput",
  "steer",
  "chargeparameter",
  "pipe",
  "removePTails",
  "reverseorder",
  "localfit"};

char *USAGE =
  "Usage:\n"
  "  elegant2genesis [<input>] [<output>]\n"
  "                  [-pipe=[in][,out]] \n"
  "                  [-textOutput]\n"
  "                  [-totalCharge=<charge-in-Coulombs>]\n"
  "                  [-chargeParameter=<name>]\n"
  "                  [-wavelength=<meters>]\n"
  "                  [-slices=<integer>]\n"
  "                  [-steer] \n"
  "                  [-removePTails=deltaLimit=<value>[,fit][,beamOutput=<filename>]]\n"
  "                  [-reverseOrder] \n"
  "                  [-localFit]\n"
  "Options:\n"
  "  -pipe=[in][,out]                                 Set up pipe communication for input and/or output.\n"
  "  -textOutput                                      Make the output file a text file instead of an SDDS file.\n"
  "  -totalCharge=<charge-in-Coulombs>                Specify the total charge in Coulombs.\n"
  "  -chargeParameter=<name>                          Specify the name of a parameter in the input file that gives the total charge in Coulombs.\n"
  "  -wavelength=<meters>                             Specify the wavelength of light in meters.\n"
  "  -slices=<integer>                                Specify the number of slices to divide the beam into.\n"
  "  -steer                                           Force the x, x', y, and y' centroids for the whole beam to zero.\n"
  "                                                   Slices may still have nonzero centroids.\n"
  "  -removePTails=deltaLimit=<value>[,fit][,beamOutput=<filename>]\n"
  "                                                   Remove the momentum tails from the beam.\n"
  "                                                   deltaLimit specifies the maximum |p - <p>|/<p> to keep.\n"
  "                                                   'fit' enables a linear fit to (t, p) for tail removal based on residuals.\n"
  "                                                   'beamOutput' writes the filtered beam to the specified file for review.\n"
  "  -reverseOrder                                    Output the data for the tail of the beam first instead of the head.\n"
  "  -localFit                                        Perform a local linear fit of momentum vs time for each slice and subtract it from the momentum data,\n"
  "                                                   removing a contribution to the energy spread.\n\n"
  "Program by Robert Soliday and Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define PTAILS_DELTALIMIT 0x0001U
#define PTAILS_FITMODE 0x0002U
#define PTAILS_OUTPUT 0x0004U

int64_t removeMomentumTails(double *x, double *xp, double *y, double *yp, double *s, double *p, double *t, int64_t rows, double limit, unsigned long flags);
void removeLocalFit(double *p, double *s, short *selected, double pAverage, int64_t rows);

/* ********** */

int main(int argc, char **argv) {
  SDDS_DATASET SDDS_input, SDDS_output, SDDS_pTails;
  FILE *fileID = NULL;
  SCANNED_ARG *s_arg;
  long i_arg;
  char *input, *output;
  unsigned long pipeFlags = 0;
  long noWarnings = 0, tmpfile_used = 0;
  long retval, sddsOutput = 1, steer = 0, firstOne, sliceOffset = 0;
  int64_t i, j, rows, origRows;
  long reverseOrder = 0;
  char *chargeParameter;
  unsigned long removePTailsFlags = 0;
  double pTailsDeltaLimit, term;
  char *pTailsOutputFile = NULL;
  long localFit = 0;
  short *selected;

  char column[7][15] = {"x", "xp", "y", "yp", "t", "p", "particleID"};
  long columnIndex[7];

  double *tValues, *sValues, *xValues, *xpValues, *yValues, *ypValues, *pValues;
  double sAverage, pAverage, pStDev;
  double xAverage, xpAverage, xRMS, x2Average, xp2Average, xxpAverage, xEmittance;
  double yAverage, ypAverage, yRMS, y2Average, yp2Average, yypAverage, yEmittance;
  double sMin = 1, sMax = -1, s1, s2;
  /*
    double pMin, pMax;
  */
  double alphax, alphay;

  double wavelength = 0.0001;
  double totalCharge = 0;
  double current;
  long slices = 4;
  long tmp = 0;
  long N = 0;
  double Ne = 0;

  input = output = NULL;
  sValues = NULL;
  chargeParameter = NULL;
  selected = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "Error: Insufficient arguments.\n");
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_FAILURE);
  }

  /* Parse the command line */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_TOTALCHARGE:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error: Invalid -totalCharge syntax.\n");
          exit(EXIT_FAILURE);
        }
        if (sscanf(s_arg[i_arg].list[1], "%lf", &totalCharge) != 1) {
          fprintf(stderr, "Error: Invalid -totalCharge value.\n");
          exit(EXIT_FAILURE);
        }
        break;
      case SET_WAVELENGTH:
        if (tmp == 2) {
          fprintf(stderr, "Error: -wavelength and -slices cannot be used together.\n");
          exit(EXIT_FAILURE);
        }
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error: Invalid -wavelength syntax.\n");
          exit(EXIT_FAILURE);
        }
        if (sscanf(s_arg[i_arg].list[1], "%lf", &wavelength) != 1 || wavelength <= 0) {
          fprintf(stderr, "Error: Invalid -wavelength value.\n");
          exit(EXIT_FAILURE);
        }
        tmp = 1;
        break;
      case SET_SLICES:
        if (tmp == 1) {
          fprintf(stderr, "Error: -wavelength and -slices cannot be used together.\n");
          exit(EXIT_FAILURE);
        }
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error: Invalid -slices syntax.\n");
          exit(EXIT_FAILURE);
        }
        if (sscanf(s_arg[i_arg].list[1], "%ld", &slices) != 1 || slices <= 0) {
          fprintf(stderr, "Error: Invalid -slices value.\n");
          exit(EXIT_FAILURE);
        }
        tmp = 2;
        break;
      case SET_TEXTOUTPUT:
        sddsOutput = 0;
        break;
      case SET_STEER:
        steer = 1;
        break;
      case SET_CHARGEPARAMETER:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error: Invalid -chargeParameter syntax.\n");
          exit(EXIT_FAILURE);
        }
        chargeParameter = s_arg[i_arg].list[1];
        break;
      case SET_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags)) {
          SDDS_Bomb("Invalid -pipe syntax.");
        }
        break;
      case SET_REMPTAILS:
        if (s_arg[i_arg].n_items > 1) {
          if (!scanItemList(&removePTailsFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                            "deltalimit", SDDS_DOUBLE, &pTailsDeltaLimit, 1, PTAILS_DELTALIMIT,
                            "fit", -1, NULL, 0, PTAILS_FITMODE,
                            "beamoutput", SDDS_STRING, &pTailsOutputFile, 1, PTAILS_OUTPUT,
                            NULL) ||
              !(removePTailsFlags & PTAILS_DELTALIMIT) || pTailsDeltaLimit <= 0) {
            SDDS_Bomb("Invalid -removePTails syntax or values.");
          }
        }
        break;
      case SET_REVERSEORDER:
        reverseOrder = 1;
        break;
      case SET_LOCAL_FIT:
        localFit = 1;
        break;
      default:
        fprintf(stderr, "Error: Unknown option '%s'.\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
      }
    } else {
      if (input == NULL) {
        input = s_arg[i_arg].list[0];
      } else if (output == NULL) {
        output = s_arg[i_arg].list[0];
      } else {
        fprintf(stderr, "Error: Too many filenames provided.\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  processFilenames("elegant2genesis", &input, &output, pipeFlags, noWarnings, &tmpfile_used);

  /* Open the input file */
  if (!SDDS_InitializeInput(&SDDS_input, input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  /* Check for needed columns */
  for (i = 0; i < 7; i++) {
    if ((columnIndex[i] = SDDS_GetColumnIndex(&SDDS_input, column[i])) < 0) {
      fprintf(stderr, "Error: Column '%s' does not exist.\n", column[i]);
      exit(EXIT_FAILURE);
    }
  }

  if (removePTailsFlags & PTAILS_OUTPUT) {
    if (!SDDS_InitializeOutput(&SDDS_pTails, SDDS_BINARY, 0, NULL, NULL, pTailsOutputFile) ||
        !SDDS_DefineSimpleColumn(&SDDS_pTails, "t", "s", SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&SDDS_pTails, "p", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&SDDS_pTails, "x", "m", SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&SDDS_pTails, "xp", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&SDDS_pTails, "y", "m", SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&SDDS_pTails, "yp", NULL, SDDS_DOUBLE) ||
        !SDDS_WriteLayout(&SDDS_pTails)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  }

  retval = -1;
  firstOne = 1;
  while ((retval = SDDS_ReadPage(&SDDS_input)) >= 1) {
    rows = origRows = SDDS_RowCount(&SDDS_input);
    if (rows < 1) {
      fprintf(stderr, "Error: No rows found for page %ld.\n", retval);
      exit(EXIT_FAILURE);
    }
    /* Get all the needed column data */
    tValues = SDDS_GetNumericColumn(&SDDS_input, "t", SDDS_DOUBLE);
    xValues = SDDS_GetNumericColumn(&SDDS_input, "x", SDDS_DOUBLE);
    xpValues = SDDS_GetNumericColumn(&SDDS_input, "xp", SDDS_DOUBLE);
    yValues = SDDS_GetNumericColumn(&SDDS_input, "y", SDDS_DOUBLE);
    ypValues = SDDS_GetNumericColumn(&SDDS_input, "yp", SDDS_DOUBLE);
    pValues = SDDS_GetNumericColumn(&SDDS_input, "p", SDDS_DOUBLE);

    if (chargeParameter && !SDDS_GetParameterAsDouble(&SDDS_input, chargeParameter, &totalCharge)) {
      SDDS_Bomb("Unable to read charge from file.");
    }
    if (!(tValues && xValues && xpValues && yValues && ypValues && pValues)) {
      fprintf(stderr, "Error: Invalid data for page %ld.\n", retval);
      exit(EXIT_FAILURE);
    }
    selected = realloc(selected, sizeof(*selected) * rows);
    sValues = realloc(sValues, sizeof(*sValues) * rows);
    if (!selected || !sValues) {
      SDDS_Bomb("Memory allocation failure.");
    }

    sAverage = xAverage = xpAverage = yAverage = ypAverage = 0;
    for (i = 0; i < rows; i++) { /* Calculate s values and sAverage */
      sValues[i] = tValues[i] * 2.99792458e8;
      sAverage += tValues[i];
      xAverage += xValues[i];
      xpAverage += xpValues[i];
      yAverage += yValues[i];
      ypAverage += ypValues[i];
    }
    sAverage = sAverage * 2.99792458e8 / rows;
    xAverage /= rows;
    xpAverage /= rows;
    yAverage /= rows;
    ypAverage /= rows;

    for (i = 0; i < rows; i++) {
      /* Move origin of s values to center at sAverage and flip the sign */
      sValues[i] = sAverage - sValues[i];
    }

    /* Remove the momentum tails, if requested */
    if (removePTailsFlags) {
      rows = removeMomentumTails(xValues, xpValues, yValues, ypValues, sValues, pValues, tValues, rows, pTailsDeltaLimit, removePTailsFlags);
      if (rows == 0) {
        SDDS_Bomb("All data removed by P-tails filter.");
      }
      if (removePTailsFlags & PTAILS_OUTPUT) {
        if (!SDDS_StartPage(&SDDS_pTails, rows) ||
            !SDDS_SetColumn(&SDDS_pTails, SDDS_SET_BY_NAME, xValues, rows, "x") ||
            !SDDS_SetColumn(&SDDS_pTails, SDDS_SET_BY_NAME, xpValues, rows, "xp") ||
            !SDDS_SetColumn(&SDDS_pTails, SDDS_SET_BY_NAME, yValues, rows, "y") ||
            !SDDS_SetColumn(&SDDS_pTails, SDDS_SET_BY_NAME, ypValues, rows, "yp") ||
            !SDDS_SetColumn(&SDDS_pTails, SDDS_SET_BY_NAME, tValues, rows, "t") ||
            !SDDS_SetColumn(&SDDS_pTails, SDDS_SET_BY_NAME, pValues, rows, "p") ||
            !SDDS_WritePage(&SDDS_pTails)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      }
    }

    if (steer) {
      /* Set the centroids for the whole beam to zero */
      for (i = 0; i < rows; i++) {
        xValues[i] -= xAverage;
        xpValues[i] -= xpAverage;
        yValues[i] -= yAverage;
        ypValues[i] -= ypAverage;
      }
    }

    /* Find limits of distribution in s */
    find_min_max(&sMin, &sMax, sValues, rows);
    if ((tmp == 1) || (tmp == 0)) { /* Calculate the number of slices from the wavelength */
      slices = (sMax - sMin) / wavelength + 1;
    }
    if (tmp == 2) { /* Calculate the wavelength from the number of slices */
      wavelength = (sMax - sMin) / slices;
    }

    if (firstOne) {
      /* Open the output file */
      if (sddsOutput) {
        if (!SDDS_InitializeOutput(&SDDS_output, SDDS_ASCII, 1, NULL, NULL, output)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
        if ((SDDS_DefineColumn(&SDDS_output, "s", "Location", "m", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "t", "Time position", "s", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "gamma", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "dgamma", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "Sdelta", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "xemit", "NormalizedEmittance-x", "m", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "yemit", "NormalizedEmittance-y", "m", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "xrms", "Beam Size-x", "m", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "yrms", "Beam Size-y", "m", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "xavg", "Position-x", "m", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "yavg", "Position-y", "m", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "pxavg", "Average x'", "rad", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "pyavg", "Average y'", "rad", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "alphax", "Alpha-x", NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "alphay", "Alpha-y", NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "current", "Current", "Amp", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "wakez", "Wake Loss", "eV/m", NULL, NULL, SDDS_DOUBLE, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "N", "Number of macroparticles", NULL, NULL, NULL, SDDS_LONG, 0) == -1) ||
            (SDDS_DefineColumn(&SDDS_output, "Ne", "Number of electrons", NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
        if ((SDDS_WriteLayout(&SDDS_output) != 1) || (SDDS_StartPage(&SDDS_output, slices) != 1)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else {
        fileID = fopen(output, "w");
        if (fileID == NULL) {
          fprintf(stderr, "Error: Unable to open output file for writing.\n");
          exit(EXIT_FAILURE);
        }
        fprintf(fileID, "%ld", slices);
      }
      firstOne = 0;
    } else {
      if (sddsOutput && !SDDS_LengthenTable(&SDDS_output, slices)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      sliceOffset += slices;
    }

    /* For each slice, calculate all the needed values */
    for (i = 0; i < slices; i++) {
      N = 0;
      xAverage = xpAverage = xRMS = x2Average = xp2Average = xxpAverage = 0;
      yAverage = ypAverage = yRMS = y2Average = yp2Average = yypAverage = 0;
      pAverage = alphax = alphay = 0;

      if (!reverseOrder) {
        s1 = sMin + (wavelength * i);
        s2 = sMin + (wavelength * (i + 1));
      } else {
        s1 = sMin + (wavelength * (slices - i - 1));
        s2 = sMin + (wavelength * (slices - i));
      }

      for (j = 0; j < rows; j++) {
        selected[j] = 0;
        if ((s1 <= sValues[j]) && (s2 > sValues[j])) {
          selected[j] = 1;
          N++;
          xAverage += xValues[j];
          xpAverage += xpValues[j];
          yAverage += yValues[j];
          ypAverage += ypValues[j];
          pAverage += pValues[j];
        }
      }

      if (N > 2) {
        current = N * 2.99792458e8 * (totalCharge / (origRows * wavelength));
        Ne = totalCharge * N / (e_mks * origRows);
        xAverage /= N;
        yAverage /= N;
        xpAverage /= N;
        ypAverage /= N;
        pAverage /= N;

        pStDev = 0;
        if (localFit) {
          removeLocalFit(pValues, sValues, selected, pAverage, rows);
        }
        for (j = 0; j < rows; j++) {
          if (selected[j]) {
            pStDev += (pValues[j] - pAverage) * (pValues[j] - pAverage);
            x2Average += (xValues[j] - xAverage) * (xValues[j] - xAverage);
            y2Average += (yValues[j] - yAverage) * (yValues[j] - yAverage);
            xp2Average += (xpValues[j] - xpAverage) * (xpValues[j] - xpAverage);
            yp2Average += (ypValues[j] - ypAverage) * (ypValues[j] - ypAverage);
            xxpAverage += (xValues[j] - xAverage) * (xpValues[j] - xpAverage);
            yypAverage += (yValues[j] - yAverage) * (ypValues[j] - ypAverage);
          }
        }
        pStDev = sqrt(pStDev / (N - 1.0));
        x2Average /= N;
        y2Average /= N;
        xp2Average /= N;
        yp2Average /= N;
        xxpAverage /= N;
        yypAverage /= N;
        xRMS = sqrt(x2Average);
        yRMS = sqrt(y2Average);

        if ((term = (x2Average * xp2Average) - (xxpAverage * xxpAverage)) > 0) {
          xEmittance = sqrt(term) * pAverage;
        } else {
          xEmittance = 0;
        }

        if ((term = (y2Average * yp2Average) - (yypAverage * yypAverage)) > 0) {
          yEmittance = sqrt(term) * pAverage;
        } else {
          yEmittance = 0;
        }

        alphax = -xxpAverage / ((xEmittance > 0) ? (xEmittance / pAverage) : 1);
        alphay = -yypAverage / ((yEmittance > 0) ? (yEmittance / pAverage) : 1);
      } else {
        pAverage = pStDev = xEmittance = yEmittance = xRMS = yRMS = xAverage = yAverage = xpAverage = ypAverage = alphax = alphay = current = Ne = 0;
      }

      if (sddsOutput) {
        if (SDDS_SetRowValues(&SDDS_output, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i + sliceOffset,
                              "s", s1,
                              "t", -s1 / c_mks,
                              "gamma", pAverage,
                              "dgamma", pStDev,
                              "Sdelta", (pAverage > 0) ? (pStDev / pAverage) : 0,
                              "xemit", xEmittance,
                              "yemit", yEmittance,
                              "xrms", xRMS,
                              "yrms", yRMS,
                              "xavg", xAverage,
                              "yavg", yAverage,
                              "pxavg", xpAverage,
                              "pyavg", ypAverage,
                              "alphax", alphax,
                              "alphay", alphay,
                              "current", current,
                              "wakez", 0.0,
                              "N", N,
                              "Ne", Ne,
                              NULL) != 1) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          exit(EXIT_FAILURE);
        }
      } else {
        fprintf(fileID, "\n%.6E %.6E %.6E %.6E %.6E %.6E %.6E %.6E %.6E %.6E %.6E %.6E %.6E %.6E 0.000000E+00",
                s1, pAverage, pStDev, xEmittance, yEmittance, xRMS, yRMS,
                xAverage, yAverage, xpAverage, ypAverage, alphax, alphay, current);
      }
    }

    free(tValues);
    free(xValues);
    free(xpValues);
    free(yValues);
    free(ypValues);
    free(pValues);
  }
  free(sValues);

  /* Close the output file */
  if (sddsOutput) {
    if (!SDDS_WritePage(&SDDS_output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
    if (!SDDS_Terminate(&SDDS_output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(EXIT_FAILURE);
    }
  } else {
    fclose(fileID);
  }

  /* Close the input file */
  if (!SDDS_Terminate(&SDDS_input)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if ((removePTailsFlags & PTAILS_OUTPUT) && !SDDS_Terminate(&SDDS_pTails)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}

int64_t removeMomentumTails(double *x, double *xp, double *y, double *yp, double *s, double *p, double *t, int64_t rows, double limit, unsigned long flags) {
  double *delta, pAve;
  int64_t ip, jp;

#if defined(DEBUG)
  static FILE *fp = NULL;
#endif

  if (rows == 0)
    return rows;

  delta = malloc(sizeof(*delta) * rows);
  if (!delta)
    SDDS_Bomb("Memory allocation failure.");

  for (ip = pAve = 0; ip < rows; ip++)
    pAve += p[ip];
  pAve /= rows;

  for (ip = 0; ip < rows; ip++)
    delta[ip] = (p[ip] - pAve) / pAve;

  if (flags & PTAILS_FITMODE) {
    double slope, intercept, variance;
    if (!unweightedLinearFit(s, delta, rows, &slope, &intercept, &variance))
      SDDS_Bomb("Fit failed in momentum tail removal.");
    for (ip = 0; ip < rows; ip++)
      delta[ip] -= s[ip] * slope + intercept;
  }

  jp = rows - 1;
  for (ip = 0; ip < rows; ip++) {
    if (fabs(delta[ip]) > limit) {
      if (jp > ip) {
        x[ip] = x[jp];
        xp[ip] = xp[jp];
        y[ip] = y[jp];
        yp[ip] = yp[jp];
        s[ip] = s[jp];
        p[ip] = p[jp];
        t[ip] = t[jp];
        delta[ip] = delta[jp];
        jp--;
        ip--;
      }
      rows--;
    }
  }
  free(delta);

#if defined(DEBUG)
  if (!fp) {
    fp = fopen("e2g.sdds", "w");
    if (fp) {
      fprintf(fp, "SDDS1\n&column name=x type=double &end\n");
      fprintf(fp, "&column name=xp type=double &end\n");
      fprintf(fp, "&column name=y type=double &end\n");
      fprintf(fp, "&column name=yp type=double &end\n");
      fprintf(fp, "&column name=s type=double &end\n");
      fprintf(fp, "&column name=p type=double &end\n");
      fprintf(fp, "&data mode=ascii &end\n");
      fprintf(fp, "%ld\n", rows);
    }
  }
  if (fp) {
    for (ip = 0; ip < rows; ip++)
      fprintf(fp, "%le %le %le %le %le %le\n", x[ip], xp[ip], y[ip], yp[ip], s[ip], p[ip]);
  }
#endif

  return rows;
}

void removeLocalFit(double *p, double *s, short *selected, double pAverage, int64_t rows) {
  double slope, intercept, variance;
  int64_t i;

  if (unweightedLinearFitSelect(s, p, selected, rows, &slope, &intercept, &variance)) {
    for (i = 0; i < rows; i++) {
      if (selected[i])
        p[i] -= intercept + slope * s[i] - pAverage;
    }
  }
}
