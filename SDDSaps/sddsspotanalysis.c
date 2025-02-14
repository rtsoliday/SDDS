/**
 * @file sddsspotanalysis.c
 * @brief Analysis of spot images from SDDS files.
 *
 * @details
 * This program analyzes spot images provided in the SDDS (Self Describing Data Sets) file format.
 * It allows users to define regions of interest, apply filters, compute statistical parameters,
 * and save analysis results to new SDDS files. The program also supports options for outputting 
 * visualizations of the processed spots.
 *
 * @section Usage
 * ```
 * sddsspotanalysis <inputfile> <outputfile>
 *                  [-pipe[=in][,out]] 
 *                  [-ROI=[{xy}{01}value=<value>][,{xy}{01}parameter=<name>]]
 *                  [-spotROIsize=[{xy}value=<value>][,{xy}parameter=<name>]]
 *                  [-centerOn={{xy}centroid | {xy}peak} | {xy}Parameter=<name>}]
 *                  [-imageColumns=<listOfNames>]
 *                  [-xyz=<ix>,<iy>,<Intensity>]
 *                  [-singleSpot]
 *                  [-levels=[intensity=<integer>][,saturation=<integer>]]
 *                  [-blankOut=[{xy}{01}value=<value>][,{xy}{01}parameter=<name>]]
 *                  [-sizeLines=[{xy}value=<value>][,{xy}parameter=<name>]]
 *                  [-background=[halfwidth=<value>][,symmetric][,antihalo][,antiloner[,lonerThreshold=<value>]]
 *                  [-despike=[neighbors=<integer>][,passes=<integer>][,averageOf=<integer>][,threshold=<value>][,keep]]
 *                  [-hotpixelFilter=level=<fraction>,distance=<integer>[,passes=<integer>]]
 *                  [-clipNegative] 
 *                  [-spotImage=<filename>] 
 *                  [-majorOrder=row|column] 
 * ```
 *
 * @section Options
 * | Optional                             | Description                                                                            |
 * |--------------------------------------|----------------------------------------------------------------------------------------|
 * | `-pipe`                              | Use standard SDDS Toolkit pipe option.                                                 |
 * | `-ROI`                               | Define a region of interest in pixel units.                                            |
 * | `-spotROIsize`                       | Specify the ROI size for analyzing spot properties.                                    |
 * | `-centerOn`                          | Center analysis on peak value, centroid, or specified parameter for both axes.         |
 * | `-imageColumns`                      | List column names containing image data.                                               |
 * | `-xyz`                               | Define columns for x, y indices, and intensity.                                        |
 * | `-singleSpot`                        | Retain only the most intense connected pixels, eliminating multiple spots.             |
 * | `-levels`                            | Set intensity levels and saturation thresholds.                                        |
 * | `-blankOut`                          | Define regions to blank out.                                                          |
 * | `-sizeLines`                         | Specify number of lines for beam size analysis.                                        |
 * | `-background`                        | Configure background subtraction settings.                                             |
 * | `-despike`                           | Apply noise reduction with despiking options.                                          |
 * | `-hotpixelFilter`                    | Apply a hot-pixel filter to smooth intensity values.                                   |
 * | `-clipNegative`                      | Set negative intensity values to zero.                                                |
 * | `-spotImage`                         | Save images of spots for visualization.                                               |
 * | `-majorOrder`                        | Specify the output file order (row or column major).                                   |
 *
 * @subsection Incompatibilities
 *   - `-imageColumns` and `-xyz` are mutually exclusive options.
 *   - For `-despike`:
 *     - `averageOf` must not exceed the number of neighbors.
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
 * L. Emery, M. Borland, R. Soliday
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "SDDSutils.h"
#include "fftpackC.h"

/* Enumeration for option types */
enum option_type {
  SET_ROI,
  SET_SPOTROISIZE,
  SET_IMAGECOLUMNS,
  SET_DESPIKE,
  SET_PIPE,
  SET_SIZELINES,
  SET_LEVELS,
  SET_BLANKOUT,
  SET_BACKGROUND,
  SET_SPOTIMAGE,
  SET_SINGLESPOT,
  SET_CENTERON,
  SET_MAJOR_ORDER,
  SET_CLIP_NEGATIVE,
  SET_XYZ,
  SET_HOTPIXELS,
  N_OPTIONS
};

static char *option[N_OPTIONS] = {
  "roi", 
  "spotroi", 
  "imagecolumns", 
  "despike", 
  "pipe", 
  "sizeLines",
  "levels", 
  "blankout", 
  "background", 
  "spotimage", 
  "singlespot", 
  "centeron", 
  "majorOrder",
  "clipnegative", 
  "xyz", 
  "hotpixelfilter"
};

char *USAGE1 = "Usage: sddsspotanalysis [<input>] [<output>] [-pipe[=in][,out]] \n\
[-ROI=[{xy}{01}value=<value>][,{xy}{01}parameter=<name>]]\n\
[-spotROIsize=[{xy}value=<value>][,{xy}parameter=<name>]]\n\
[-centerOn={{xy}centroid | {xy}peak} | {xy}Parameter=<name>}]\n\
{-imageColumns=<listOfNames> | -xyz=<ix>,<iy>,<Intensity>} [-singleSpot]\n\
[-levels=[intensityLevels=<integer>][,saturationLevel=<integer>]]\n\
[-blankOut=[{xy}{01}value=<value>][,{xy}{01}parameter=<name>]]\n\
[-sizeLines=[{xy}value=<value>][,{xy}parameter=<name>]]\n\
[-background=[halfwidth=<value>][,symmetric][,antihalo][,antiloner[,lonerThreshold=<value>]]\n\
[-despike=[neighbors=<integer>][,passes=<integer>][,averageOf=<integer>][,threshold=<value>][,keep]]\n\
[-hotpixelFilter=level=<fraction>,distance=<integer>[,passes=<integer>]]\n\
[-clipNegative] [-spotImage=<filename>] [-majorOrder=row|column] \n\n";
char *USAGE2 = "Options:\n"
               "  -pipe[=in][,out]            Use the standard SDDS Toolkit pipe option.\n"
               "  -ROI                        Define the region of interest in pixel units.\n"
               "                              All data outside this region is ignored.\n"
               "  -spotROIsize                Specify the size in pixels of the ROI around the spot.\n"
               "                              This ROI is used for computing spot properties.\n"
               "  -imagecolumns <list>        List names of columns containing image data.\n"
               "                              Wildcards are supported.\n"
               "  -xyz <ix>,<iy>,<Intensity>  Specify columns for x and y pixel indices and intensity.\n"
               "  -singleSpot                 Eliminate multiple spots by retaining only the most intense connected pixels.\n"
               "  -centerOn <method>          Center the analysis on the peak value, centroid, or a specified parameter for both x and y axes.\n"
               "  -levels intensityLevels=<int>, saturationLevel=<int>\n"
               "                              Set intensity levels and saturation level.\n"
               "  -blankOut <parameters>      Define regions to blank out based on pixel values or parameters.\n"
               "  -sizeLines <parameters>     Number of lines to use for analyzing the beam size. Default is 3.\n"
               "  -background <parameters>    Configure background subtraction with options like halfwidth, symmetric,\n"
               "                              antihalo, antiloner, and lonerThreshold.\n"
               "  -despike <parameters>       Apply despiking to smooth the data with options for neighbors, passes,\n"
               "                              averageOf, threshold, and keep.\n"
               "  -hotpixelFilter <parameters> Apply a hot pixel filter with level, distance, and passes parameters.\n"
               "  -clipNegative               Set negative pixel values to zero.\n"
               "  -spotImage <filename>       Specify a file to save images of the spots for plotting with sddscontour.\n"
               "  -majorOrder <row|column>    Define the output file order as either row or column.\n\n";

char *USAGE3 = "Program by Michael Borland. (" __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ")\n";

#define DESPIKE_AVERAGEOF 0x0001U
#define DESPIKE_KEEP (DESPIKE_AVERAGEOF << 1)

#define X0VALUE 0x0001U
#define X1VALUE (X0VALUE << 1)
#define Y0VALUE (X1VALUE << 1)
#define Y1VALUE (Y0VALUE << 1)
#define X0PARAM (Y1VALUE << 1)
#define X1PARAM (X0PARAM << 1)
#define Y0PARAM (X1PARAM << 1)
#define Y1PARAM (Y0PARAM << 1)
#define OPTGIVEN (Y1PARAM << 1)

typedef struct
{
  double backgroundLevel, integratedSpotIntensity, peakSpotIntensity;
  double saturationCount, backgroundKilledPositive, backgroundKilledNegative;
  int32_t ROI[4], spotROI[4], spotCenter[2];
  double spotSigma[2], spotRange50[2], spotRange65[2], spotRange80[2], spotCentroid[2];
  double S11, S33, rmsCrossProduct, phi, majorAxis, minorAxis;
} IMAGE_ANALYSIS;

typedef struct
{
  unsigned long flags;
  double fraction;
  long passes, distance;
} HOTPIXEL_SETTINGS;

void replaceWithNearNeighbors(double **image, long iy0, long iy1, long ix0, long ix1, long iyc, long ixc, long distance);

typedef struct
{
  unsigned long flags;
  int32_t neighbors, passes, averageOf;
  double threshold;
} DESPIKE_SETTINGS;

long SetUpOutputFile(SDDS_DATASET *SDDSout, char *outputFile, SDDS_DATASET *SDDSin, char ***copyParamName,
                     long *copyParamNames, short columnMajorOrder);
long DetermineQuadLongValues(int32_t *ROI, unsigned long flags, char **parameter, SDDS_DATASET *SDDSin, long nx, long ny, long isROI);
long DetermineDualLongValues(int32_t *spotROISize, unsigned long flags, char **parameter, SDDS_DATASET *SDDSin, long defaultValue);
void BlankOutImageData(double **image, long nx, long ny, int32_t *region);
long GetImageData(long *nx, double ***image, char **imageColumn, long ny, SDDS_DATASET *SDDSin);
long GetXYZImageData(long *nx, long *ny, double ***image, char *ixName, char *iyName, char *intensityName, SDDS_DATASET *SDDSin);

#define SYMMETRIC_BGREMOVAL 0x0001U
#define ANTIHALO_BGREMOVAL 0x0002U
#define REMOVE_LONERS 0x0004U
#define SINGLE_SPOT 0x0008U
#define XCENTER_PEAK 0x0010U
#define YCENTER_PEAK 0x0020U
#define XCENTER_CENTROID 0x0040U
#define YCENTER_CENTROID 0x0080U
#define CLIP_NEGATIVE 0x0100U
#define XCENTER_PARAM 0x0200U
#define YCENTER_PARAM 0x0400U

long AnalyzeImageData(double **image, long nx, long ny, int32_t *ROI, int32_t *spotROISize,
                      int32_t *sizeLines, DESPIKE_SETTINGS *despikeSettings, HOTPIXEL_SETTINGS *hotpixelSettings,
                      long intensityLevels, long saturationLevel, long backgroundHalfWidth, long lonerThreshold,
                      long lonerPasses, unsigned long flags, IMAGE_ANALYSIS *analysisResults, SDDS_DATASET *SDDSspim,
                      double *centerValue);

void DetermineBeamSizes(double *sigma, double *spotRange50, double *spotRange65, double *spotRange80,
                        double *lineBuffer, long i0, long i1);
void DetermineBeamParameters(double **image, long *spotROI, long nx, long ny, double *S11, double *S33,
                             double *rmsCrossProduct, double *phi, double *majorAxis, double *minorAxis);

int main(int argc, char **argv) {
  SCANNED_ARG *scArg;
  long iArg, imageColumns = 0, i, ip;
  char **imageColumn = NULL;
  char *ixName, *iyName, *IntensityName;
  short xyzMode = 0;
  unsigned long pipeFlags = 0, ROIFlags = 0, spotROIFlags = 0, blankOutFlags = 0;
  int32_t intensityLevels = 256, saturationLevel = 254, backgroundHalfWidth = 3, lonerThreshold = 1, lonerPasses = 1;
  long despike = 0;
  DESPIKE_SETTINGS despikeSettings;
  HOTPIXEL_SETTINGS hotpixelSettings;
  short hotpixelFilter = 0;
  char *inputFile = NULL, *outputFile = NULL;
  int32_t ROI[4] = {-1, -1, -1, -1};
  int32_t spotROISize[2] = {-1, -1};
  char *ROIParameter[4] = {NULL, NULL, NULL, NULL};
  int32_t blankOut[4] = {-1, -1, -1, -1};
  char *blankOutParameter[4] = {NULL, NULL, NULL, NULL};
  char *spotROISizeParameter[2] = {NULL, NULL};
  SDDS_DATASET SDDSin, SDDSout;
  long readStatus, nx, ny, outputRow, maxPages = 0;
  double **image;
  IMAGE_ANALYSIS analysisResults;
  unsigned long sizeLinesFlags = 0, dummyFlags = 0, backgroundFlags = 0, centerFlags = 0, majorOrderFlag;
  int32_t sizeLines[2] = {-1, -1};
  char *sizeLinesParameter[2] = {NULL, NULL};
  char *centerParameter[2] = {NULL, NULL};
  double centerValue[2];
  char **copyParamName;
  long copyParamNames, copyBuffer[32];
  char *spotImageFile = NULL;
  short columnMajorOrder = -1;
  SDDS_DATASET SDDSspim;

  if (argc < 2) {
    fprintf(stderr, "%s%s%s", USAGE1, USAGE2, USAGE3);
    exit(EXIT_FAILURE);
  }

  ixName = iyName = IntensityName = NULL;

  argc = scanargsg(&scArg, argc, argv);
  for (iArg = 1; iArg < argc; iArg++) {
    if (scArg[iArg].arg_type == OPTION) {
      delete_chars(scArg[iArg].list[0], "_");
      /* process options here */
      switch (match_string(scArg[iArg].list[0], option, N_OPTIONS, 0)) {
      case SET_MAJOR_ORDER:
        majorOrderFlag = 0;
        scArg[iArg].n_items--;
        if (scArg[iArg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL)))
          SDDS_Bomb("invalid -majorOrder syntax/values");
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case SET_IMAGECOLUMNS:
        if (scArg[iArg].n_items < 2)
          SDDS_Bomb("invalid -imagecolumns syntax");
        if (imageColumns)
          SDDS_Bomb("give one and only one -imagecolumns option");
        imageColumns = scArg[iArg].n_items - 1;
        imageColumn = tmalloc(sizeof(*imageColumn) * imageColumns);
        for (i = 0; i < imageColumns; i++)
          imageColumn[i] = scArg[iArg].list[i + 1];
        xyzMode = 0;
        break;
      case SET_ROI:
        if (ROIFlags & OPTGIVEN)
          SDDS_Bomb("give -ROI only once");
        ROIFlags = OPTGIVEN;
        scArg[iArg].n_items--;
        if (scArg[iArg].n_items < 1 ||
            (!scanItemList(&ROIFlags, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                           "x0value", SDDS_LONG, ROI + 0, 1, X0VALUE,
                           "x1value", SDDS_LONG, ROI + 1, 1, X1VALUE,
                           "y0value", SDDS_LONG, ROI + 2, 1, Y0VALUE,
                           "y1value", SDDS_LONG, ROI + 3, 1, Y1VALUE,
                           "x0parameter", SDDS_STRING, ROIParameter + 0, 1, X0PARAM,
                           "x1parameter", SDDS_STRING, ROIParameter + 1, 1, X1PARAM,
                           "y0parameter", SDDS_STRING, ROIParameter + 2, 1, Y0PARAM,
                           "y1parameter", SDDS_STRING, ROIParameter + 3, 1, Y1PARAM,
                           NULL) ||
             ((ROIFlags & X0VALUE) && ROI[0] < 0) || ((ROIFlags & X1VALUE) && ROI[1] < 0) ||
             ((ROIFlags & Y0VALUE) && ROI[2] < 0) || ((ROIFlags & Y1VALUE) && ROI[3] < 0) ||
             ((ROIFlags & X0PARAM) && (!ROIParameter[0] || !strlen(ROIParameter[0]))) ||
             ((ROIFlags & X1PARAM) && (!ROIParameter[1] || !strlen(ROIParameter[1]))) ||
             ((ROIFlags & Y0PARAM) && (!ROIParameter[2] || !strlen(ROIParameter[2]))) ||
             ((ROIFlags & Y1PARAM) && (!ROIParameter[3] || !strlen(ROIParameter[3])))))
          SDDS_Bomb("invalid -ROI syntax/values");
        break;
      case SET_BLANKOUT:
        if (blankOutFlags & OPTGIVEN)
          SDDS_Bomb("give -blankout only once");
        blankOutFlags = OPTGIVEN;
        scArg[iArg].n_items--;
        if (scArg[iArg].n_items < 1 ||
            (!scanItemList(&blankOutFlags, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                           "x0value", SDDS_LONG, blankOut + 0, 1, X0VALUE,
                           "x1value", SDDS_LONG, blankOut + 1, 1, X1VALUE,
                           "y0value", SDDS_LONG, blankOut + 2, 1, Y0VALUE,
                           "y1value", SDDS_LONG, blankOut + 3, 1, Y1VALUE,
                           "x0parameter", SDDS_STRING, blankOutParameter + 0, 1, X0PARAM,
                           "x1parameter", SDDS_STRING, blankOutParameter + 1, 1, X1PARAM,
                           "y0parameter", SDDS_STRING, blankOutParameter + 2, 1, Y0PARAM,
                           "y1parameter", SDDS_STRING, blankOutParameter + 3, 1, Y1PARAM,
                           NULL) ||
             ((blankOutFlags & X0VALUE) && blankOut[0] < 0) || ((blankOutFlags & X1VALUE) && blankOut[1] < 0) ||
             ((blankOutFlags & Y0VALUE) && blankOut[2] < 0) || ((blankOutFlags & Y1VALUE) && blankOut[3] < 0) ||
             ((blankOutFlags & X0PARAM) && (!blankOutParameter[0] || !strlen(blankOutParameter[0]))) ||
             ((blankOutFlags & X1PARAM) && (!blankOutParameter[1] || !strlen(blankOutParameter[1]))) ||
             ((blankOutFlags & Y0PARAM) && (!blankOutParameter[2] || !strlen(blankOutParameter[2]))) ||
             ((blankOutFlags & Y1PARAM) && (!blankOutParameter[3] || !strlen(blankOutParameter[3])))))
          SDDS_Bomb("invalid -blankOut syntax/values");
        break;
      case SET_SPOTROISIZE:
        if (spotROIFlags & OPTGIVEN)
          SDDS_Bomb("give -spotROIsize only once");
        spotROIFlags = OPTGIVEN;
        scArg[iArg].n_items--;
        if (scArg[iArg].n_items < 1 ||
            (!scanItemList(&spotROIFlags, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                           "xvalue", SDDS_LONG, spotROISize + 0, 1, X0VALUE,
                           "yvalue", SDDS_LONG, spotROISize + 1, 1, Y0VALUE,
                           "xparameter", SDDS_STRING, spotROISizeParameter + 0, 1, X0PARAM,
                           "yparameter", SDDS_STRING, spotROISizeParameter + 1, 1, Y0PARAM,
                           NULL) ||
             ((spotROIFlags & X0VALUE) && spotROISize[0] < 0) ||
             ((spotROIFlags & Y0VALUE) && spotROISize[1] < 0) ||
             ((spotROIFlags & X0PARAM) && (!spotROISizeParameter[0] || !strlen(spotROISizeParameter[0]))) ||
             ((spotROIFlags & Y0PARAM) && (!spotROISizeParameter[1] || !strlen(spotROISizeParameter[1])))))
          SDDS_Bomb("invalid -spotROIsize syntax/values");
        break;
      case SET_SIZELINES:
        if (sizeLinesFlags & OPTGIVEN)
          SDDS_Bomb("give -sizeLines only once");
        sizeLinesFlags = OPTGIVEN;
        scArg[iArg].n_items--;
        if (scArg[iArg].n_items < 1 ||
            (!scanItemList(&sizeLinesFlags, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                           "xvalue", SDDS_LONG, sizeLines + 0, 1, X0VALUE,
                           "yvalue", SDDS_LONG, sizeLines + 1, 1, Y0VALUE,
                           "xparameter", SDDS_STRING, sizeLinesParameter + 0, 1, X0PARAM,
                           "yparameter", SDDS_STRING, sizeLinesParameter + 1, 1, Y0PARAM,
                           NULL) ||
             ((sizeLinesFlags & X0VALUE) && sizeLines[0] < 0) ||
             ((sizeLinesFlags & Y0VALUE) && sizeLines[1] < 0) ||
             ((sizeLinesFlags & X0PARAM) && (!sizeLinesParameter[0] || !strlen(sizeLinesParameter[0]))) ||
             ((sizeLinesFlags & Y0PARAM) && (!sizeLinesParameter[1] || !strlen(sizeLinesParameter[1])))))
          SDDS_Bomb("invalid -sizeLines syntax/values");
        break;
      case SET_DESPIKE:
        scArg[iArg].n_items--;
        despikeSettings.neighbors = 4;
        despikeSettings.passes = 1;
        despikeSettings.threshold = 0;
        despikeSettings.averageOf = 2;
        despikeSettings.flags = 0;
        if (scArg[iArg].n_items > 0 &&
            (!scanItemList(&despikeSettings.flags, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                           "neighbors", SDDS_LONG, &despikeSettings.neighbors, 1, 0,
                           "passes", SDDS_LONG, &despikeSettings.passes, 1, 0,
                           "averageOf", SDDS_LONG, &despikeSettings.averageOf, 1, DESPIKE_AVERAGEOF,
                           "threshold", SDDS_DOUBLE, &despikeSettings.threshold, 1, 0,
                           "keep", -1, NULL, 0, DESPIKE_KEEP, NULL) ||
             despikeSettings.neighbors < 2 || despikeSettings.passes < 1 ||
             despikeSettings.averageOf < 2 || despikeSettings.threshold < 0))
          SDDS_Bomb("invalid -despike syntax/values");
        if (!(despikeSettings.flags & DESPIKE_AVERAGEOF))
          despikeSettings.averageOf = despikeSettings.neighbors;
        if (despikeSettings.averageOf > despikeSettings.neighbors)
          SDDS_Bomb("invalid -despike syntax/values: averageOf>neighbors");
        despike = 1;
        break;
      case SET_HOTPIXELS:
        scArg[iArg].n_items--;
        hotpixelSettings.passes = 1;
        hotpixelSettings.distance = 1;
        hotpixelSettings.fraction = -1;
        if (scArg[iArg].n_items > 0 &&
            (!scanItemList(&hotpixelSettings.flags, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                           "fraction", SDDS_DOUBLE, &hotpixelSettings.fraction, 1, 0,
                           "passes", SDDS_LONG, &hotpixelSettings.passes, 1, 0,
                           "distance", SDDS_LONG, &hotpixelSettings.distance, 1, 0, NULL) ||
             hotpixelSettings.fraction <= 0 || hotpixelSettings.passes < 1 || hotpixelSettings.distance < 1))
          SDDS_Bomb("invalid -hotpixelFilter syntax/values");
        hotpixelFilter = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(scArg[iArg].list + 1, scArg[iArg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_LEVELS:
        scArg[iArg].n_items--;
        if (scArg[iArg].n_items < 1 ||
            !scanItemList(&dummyFlags, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                          "intensityLevels", SDDS_LONG, &intensityLevels, 1, 0,
                          "saturationLevel", SDDS_LONG, &saturationLevel, 1, 0, NULL) ||
            intensityLevels <= 10 || saturationLevel <= 0)
          SDDS_Bomb("invalid -levels syntax/values");
        break;
      case SET_BACKGROUND:
        scArg[iArg].n_items--;
        if (scArg[iArg].n_items < 1 ||
            !scanItemList(&backgroundFlags, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                          "halfwidth", SDDS_LONG, &backgroundHalfWidth, 1, 0,
                          "symmetric", -1, NULL, 0, SYMMETRIC_BGREMOVAL,
                          "antihalo", -1, NULL, 0, ANTIHALO_BGREMOVAL,
                          "antiloner", -1, NULL, 0, REMOVE_LONERS,
                          "lonerthreshold", SDDS_LONG, &lonerThreshold, 1, 0,
                          "lonerpasses", SDDS_LONG, &lonerPasses, 1, 0, NULL) ||
            backgroundHalfWidth < 0)
          SDDS_Bomb("invalid -background syntax/values");
        break;
      case SET_SINGLESPOT:
        if (scArg[iArg].n_items != 1)
          SDDS_Bomb("invalid -singleSpot syntax/values");
        backgroundFlags |= SINGLE_SPOT;
        break;
      case SET_SPOTIMAGE:
        if (scArg[iArg].n_items != 2 || !(spotImageFile = scArg[iArg].list[1]))
          SDDS_Bomb("invalid -spotImage syntax/values");
        break;
      case SET_CLIP_NEGATIVE:
        backgroundFlags |= CLIP_NEGATIVE;
        break;
      case SET_CENTERON:
        scArg[iArg].n_items--;

        if (scArg[iArg].n_items < 1 ||
            !scanItemList(&centerFlags, scArg[iArg].list + 1, &scArg[iArg].n_items, 0,
                          "xcentroid", -1, NULL, 0, XCENTER_CENTROID,
                          "ycentroid", -1, NULL, 0, YCENTER_CENTROID,
                          "xpeak", -1, NULL, 0, XCENTER_PEAK,
                          "ypeak", -1, NULL, 0, YCENTER_PEAK,
                          "xparameter", SDDS_STRING, centerParameter + 0, 1, XCENTER_PARAM,
                          "yparameter", SDDS_STRING, centerParameter + 1, 1, YCENTER_PARAM, NULL) ||
            bitsSet(centerFlags & (XCENTER_CENTROID + XCENTER_PEAK + XCENTER_PARAM)) != 1 ||
            bitsSet(centerFlags & (YCENTER_CENTROID + YCENTER_PEAK + YCENTER_PARAM)) != 1)
          SDDS_Bomb("invalid -centerOn syntax");
        break;
      case SET_XYZ:
        if (scArg[iArg].n_items != 4 ||
            !strlen(ixName = scArg[iArg].list[1]) ||
            !strlen(iyName = scArg[iArg].list[2]) ||
            !strlen(IntensityName = scArg[iArg].list[3]))
          SDDS_Bomb("invalid -xyz syntax");
        xyzMode = 1;
        break;
      default:
        fprintf(stderr, "unknown option %s given\n", scArg[iArg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      /* argument is input filename */
      if (!inputFile) {
        inputFile = scArg[iArg].list[0];
      } else if (!outputFile) {
        outputFile = scArg[iArg].list[0];
      } else
        SDDS_Bomb("too many filenames");
    }
  }

  processFilenames("sddsspotanalysis", &inputFile, &outputFile, pipeFlags, 0, NULL);

  if (!imageColumns && !IntensityName)
    SDDS_Bomb("you must give either the -imageColumns or -xyz option");
  if (imageColumns && IntensityName)
    SDDS_Bomb("you must give either the -imageColumns or -xyz option, not both");

  if (!SDDS_InitializeInput(&SDDSin, inputFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (spotImageFile &&
      (!SDDS_InitializeOutput(&SDDSspim, SDDS_BINARY, 0, NULL, "sddsspotanalysis spot image", spotImageFile) ||
       SDDS_DefineColumn(&SDDSspim, "ix", NULL, NULL, NULL, NULL, SDDS_SHORT, 0) < 0 ||
       SDDS_DefineColumn(&SDDSspim, "iy", NULL, NULL, NULL, NULL, SDDS_SHORT, 0) < 0 ||
       SDDS_DefineColumn(&SDDSspim, "Image", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
       SDDS_DefineParameter(&SDDSspim, "nx", NULL, NULL, NULL, NULL, SDDS_SHORT, 0) < 0 ||
       SDDS_DefineParameter(&SDDSspim, "ny", NULL, NULL, NULL, NULL, SDDS_SHORT, 0) < 0 ||
       !SDDS_WriteLayout(&SDDSspim))) {
    SDDS_SetError("Problem setting up spot image output file.");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }

  if (!xyzMode) {
    if ((imageColumns = expandColumnPairNames(&SDDSin, &imageColumn, NULL, imageColumns, NULL, 0, FIND_NUMERIC_TYPE, 0)) <= 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Bomb("No quantities selected to analyze");
    }
    ny = imageColumns;
    outputRow = 0;
    if (!(image = calloc(ny, sizeof(*image))))
      SDDS_Bomb("Memory allocation failure");
    nx = 0;
  } else
    ny = nx = 0;

  if (!SetUpOutputFile(&SDDSout, outputFile, &SDDSin, &copyParamName, &copyParamNames, columnMajorOrder) ||
      !SDDS_StartPage(&SDDSout, maxPages = 10))
    SDDS_Bomb("Problem setting up output file.");

  while ((readStatus = SDDS_ReadPage(&SDDSin)) > 0) {
    if (readStatus > maxPages) {
      if (!SDDS_LengthenTable(&SDDSout, 10))
        SDDS_Bomb("Problem lengthening output file.");
      maxPages += 10;
    }
    /* Get image into array.
     * N.B.: pixel (ix, iy) is accessed as image[iy][ix].
     */
    if (!xyzMode) {
      if (!GetImageData(&nx, &image, imageColumn, ny, &SDDSin))
        SDDS_Bomb("Problem getting image data.");
      if (!nx)
        continue;
    } else {
      if (!GetXYZImageData(&nx, &ny, &image, ixName, iyName, IntensityName, &SDDSin))
        SDDS_Bomb("Problem getting image data.");
      if (!nx || !ny)
        continue;
    }
    if (!DetermineQuadLongValues(ROI, ROIFlags, ROIParameter, &SDDSin, nx, ny, 1)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Bomb("Problem determining region of interest---verify that you gave sufficient information.");
    }
    if (blankOutFlags && !DetermineQuadLongValues(blankOut, blankOutFlags, blankOutParameter, &SDDSin, nx, ny, 0))
      SDDS_Bomb("Problem determining blankout region---verify that you gave sufficient information.");
    if (!DetermineDualLongValues(spotROISize, spotROIFlags, spotROISizeParameter, &SDDSin, 150))
      SDDS_Bomb("Problem determine size of spot region of interest---verify that you gave sufficient information.");
    if (!DetermineDualLongValues(sizeLines, sizeLinesFlags, sizeLinesParameter, &SDDSin, 3))
      SDDS_Bomb("Problem determine size of number of lines to use for spot size computation---verify that you gave sufficient information.");
    centerValue[0] = centerValue[1] = -1;
    if (centerFlags & XCENTER_PARAM && !SDDS_GetParameterAsDouble(&SDDSin, centerParameter[0], centerValue + 0))
      SDDS_Bomb("Problem getting center parameter value for x.");
    if (centerFlags & YCENTER_PARAM && !SDDS_GetParameterAsDouble(&SDDSin, centerParameter[1], centerValue + 1))
      SDDS_Bomb("Problem getting center parameter value for y.");
    if (blankOutFlags)
      BlankOutImageData(image, nx, ny, blankOut);
    if (!AnalyzeImageData(image, nx, ny, ROI, spotROISize, sizeLines,
                          despike ? &despikeSettings : NULL, hotpixelFilter ? &hotpixelSettings : NULL, intensityLevels,
                          saturationLevel, backgroundHalfWidth, lonerThreshold, lonerPasses, backgroundFlags | centerFlags,
                          &analysisResults, spotImageFile ? &SDDSspim : NULL, centerValue))
      continue;
    for (ip = 0; ip < copyParamNames; ip++) {
      if (!SDDS_GetParameter(&SDDSin, copyParamName[ip], copyBuffer))
        SDDS_Bomb("Problem reading parameter data from input file.");
      if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_REFERENCE, outputRow, copyParamName[ip], copyBuffer, NULL)) {
        SDDS_Bomb("Problem copying parameter data from input file.");
      }
    }
    if (!SDDS_SetRowValues(&SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, outputRow++,
                           "ImageIndex", readStatus - 1,
                           "xROILower", analysisResults.ROI[0],
                           "xROIUpper", analysisResults.ROI[1],
                           "yROILower", analysisResults.ROI[2],
                           "yROIUpper", analysisResults.ROI[3],
                           "xSpotROILower", analysisResults.spotROI[0],
                           "xSpotROIUpper", analysisResults.spotROI[1],
                           "ySpotROILower", analysisResults.spotROI[2],
                           "ySpotROIUpper", analysisResults.spotROI[3],
                           "xSpotCenter", analysisResults.spotCenter[0],
                           "ySpotCenter", analysisResults.spotCenter[1],
                           "xSpotCentroid", analysisResults.spotCentroid[0],
                           "ySpotCentroid", analysisResults.spotCentroid[1],
                           "xSpotSigma", analysisResults.spotSigma[0],
                           "xSpotRange50", analysisResults.spotRange50[0],
                           "xSpotRange65", analysisResults.spotRange65[0],
                           "xSpotRange80", analysisResults.spotRange80[0],
                           "ySpotSigma", analysisResults.spotSigma[1],
                           "ySpotRange50", analysisResults.spotRange50[1],
                           "ySpotRange65", analysisResults.spotRange65[1],
                           "ySpotRange80", analysisResults.spotRange80[1],
                           "BackgroundLevel", analysisResults.backgroundLevel,
                           "BackgroundKilledPositive", analysisResults.backgroundKilledPositive,
                           "BackgroundKilledNegative", analysisResults.backgroundKilledNegative,
                           "IntegratedSpotIntensity", analysisResults.integratedSpotIntensity,
                           "PeakSpotIntensity", analysisResults.peakSpotIntensity,
                           "SaturationCount", analysisResults.saturationCount,
                           "phi", analysisResults.phi,
                           "rmsCrossProduct", analysisResults.rmsCrossProduct,
                           "majorAxis", analysisResults.majorAxis,
                           "minorAxis", analysisResults.minorAxis,
                           "S11", analysisResults.S11,
                           "S33", analysisResults.S33,
                           "S13", analysisResults.rmsCrossProduct, NULL)) {
      SDDS_SetError("Problem setting values into output file.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!SDDS_WritePage(&SDDSout) || !SDDS_Terminate(&SDDSout) || !SDDS_Terminate(&SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (spotImageFile && !SDDS_Terminate(&SDDSspim))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < ny; i++)
    free(image[i]);
  if (!xyzMode) {
    for (i = 0; i < ny; i++)
      free(imageColumn[i]);
    free(imageColumn);
  }
  for (i = 0; i < copyParamNames; i++)
    free(copyParamName[i]);
  free(copyParamName);
  free(image);
  return EXIT_SUCCESS;
}

long SetUpOutputFile(SDDS_DATASET *SDDSout, char *outputFile, SDDS_DATASET *SDDSin, char ***copyParamName,
                     long *copyParamNames, short columnMajorOrder) {
  char **paramName;
  int32_t paramNames;

  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, "sddsspotanalysis output", outputFile) ||
      SDDS_DefineColumn(SDDSout, "xROILower", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "xROIUpper", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "xSpotROILower", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "xSpotROIUpper", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "xSpotCenter", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "xSpotCentroid", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "xSpotSigma", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "xSpotRange50", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "xSpotRange65", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "xSpotRange80", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "yROILower", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "yROIUpper", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "ySpotROILower", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "ySpotROIUpper", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "ySpotCenter", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "ySpotCentroid", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "ySpotSigma", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "ySpotRange50", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "ySpotRange65", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "ySpotRange80", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "ImageIndex", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "BackgroundLevel", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "BackgroundKilledNegative", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "BackgroundKilledPositive", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "IntegratedSpotIntensity", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "PeakSpotIntensity", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "SaturationCount", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "phi", NULL, "degree", NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "rmsCrossProduct", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "majorAxis", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "minorAxis", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "S11", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "S33", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(SDDSout, "S13", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0) {
    SDDS_SetError("Problem setting up output file.");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  if (columnMajorOrder != -1)
    SDDSout->layout.data_mode.column_major = columnMajorOrder;
  else
    SDDSout->layout.data_mode.column_major = SDDSin->layout.data_mode.column_major;

  *copyParamNames = 0;
  *copyParamName = NULL;
  if ((paramName = SDDS_GetParameterNames(SDDSin, &paramNames)) && paramNames > 0) {
    long ip;
    if (!(*copyParamName = calloc(sizeof(**copyParamName), paramNames)))
      SDDS_Bomb("memory allocation failure");
    for (ip = 0; ip < paramNames; ip++) {
      if (SDDS_GetColumnIndex(SDDSout, paramName[ip]) >= 0) {
        free(paramName[ip]);
        continue;
      }
      if (!SDDS_DefineColumnLikeParameter(SDDSout, SDDSin, paramName[ip], NULL)) {
        SDDS_SetError("Problem setting up output file.");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(EXIT_FAILURE);
      }
      (*copyParamName)[*copyParamNames] = paramName[ip];
      (*copyParamNames)++;
    }
  }
  free(paramName);
  if (!SDDS_WriteLayout(SDDSout)) {
    SDDS_SetError("Problem setting up output file.");
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(EXIT_FAILURE);
  }
  return 1;
}

long DetermineQuadLongValues(int32_t *ROI, unsigned long flags, char **parameter, SDDS_DATASET *SDDSin, long nx, long ny, long isROI) {
  long i;
  double value;
  double defaultROI[4];
  if (isROI) {
    defaultROI[0] = defaultROI[2] = 0;
    defaultROI[1] = nx - 1;
    defaultROI[3] = ny - 1;
  } else
    defaultROI[0] = defaultROI[2] = defaultROI[1] = defaultROI[3] = -1;
  for (i = 0; i < 4; i++) {
    if (flags & (X0VALUE << i))
      continue;
    if (flags & (X0PARAM << i)) {
      if (!SDDS_GetParameterAsDouble(SDDSin, parameter[i], &value)) {
        SDDS_SetError("parameter is nonexistent or nonnumeric");
        return 0;
      }
      ROI[i] = value + 0.5;
    } else
      ROI[i] = defaultROI[i];
  }
  if (ROI[0] < 0 || ROI[2] < 0) {
    SDDS_SetError("lower bound of region less than zero");
    return 0;
  }
  if (ROI[1] > nx - 1 || ROI[3] > ny - 1) {
    SDDS_SetError("upper bound of region too large");
    return 0;
  }
  if (ROI[0] >= ROI[1]) {
    SDDS_SetError("x region has zero or negative width");
    return 0;
  }
  if (ROI[2] >= ROI[3]) {
    SDDS_SetError("y region has zero or negative width");
    return 0;
  }
  return 1;
}

long DetermineDualLongValues(int32_t *returnValue, unsigned long flags, char **parameter, SDDS_DATASET *SDDSin, long defaultValue) {
  long i;
  double value;

  for (i = 0; i < 2; i++) {
    if (flags & (X0VALUE << (2 * i)))
      continue;
    if (flags & (X0PARAM << (2 * i))) {
      if (!SDDS_GetParameterAsDouble(SDDSin, parameter[i], &value)) {
        SDDS_SetError("parameter is nonexistent or nonnumeric");
        return 0;
      }
      returnValue[i] = (int32_t)value;
    } else
      returnValue[i] = defaultValue;
  }
  if (returnValue[0] <= 0 || returnValue[1] <= 0) {
    SDDS_SetError("determined value is nonpositive");
    return 0;
  }
  return 1;
}

long GetImageData(long *nx, double ***image, char **imageColumn, long ny, SDDS_DATASET *SDDSin) {
  /* Note that because each column is a horizontal line, the pixels are
   * accessed as image[iy][ix].  I could swap it, but that would mean
   * doubling the amount of memory used.
   */
  long i;
  *nx = 0;
  for (i = 0; i < ny; i++) {
    if ((*image)[i]) {
      free((*image)[i]);
      (*image)[i] = NULL;
    }
    if (!((*image)[i] = SDDS_GetColumnInDoubles(SDDSin, imageColumn[i]))) {
      SDDS_Bomb("Unable to get data from columns");
    }
  }
  *nx = SDDS_RowCount(SDDSin);
  return 1;
}

long GetXYZImageData(long *nx, long *ny, double ***image, char *ixName, char *iyName, char *intensityName, SDDS_DATASET *SDDSin) {
  int32_t *ixData, *iyData;
  double *intensityData;
  long ixMin, iyMin, ixMax, iyMax, i, ix, iy, nRows;
  char *ixIndexCheck, *iyIndexCheck;

  if ((nRows = SDDS_RowCount(SDDSin)) <= 0 ||
      !(ixData = SDDS_GetColumnInLong(SDDSin, ixName)) ||
      !(iyData = SDDS_GetColumnInLong(SDDSin, iyName)) ||
      !(intensityData = SDDS_GetColumnInDoubles(SDDSin, intensityName)))
    return 0;

  ixMax = iyMax = -(ixMin = iyMin = LONG_MAX);

  for (i = 0; i < nRows; i++) {
    if (ixMax < ixData[i])
      ixMax = ixData[i];
    if (ixMin > ixData[i])
      ixMin = ixData[i];
    if (iyMax < iyData[i])
      iyMax = iyData[i];
    if (iyMin > iyData[i])
      iyMin = iyData[i];
  }
  if (ixMax == ixMin)
    return 0;
  if (iyMax == iyMin)
    return 0;

  ixIndexCheck = calloc((*nx = ixMax - ixMin + 1), sizeof(*ixIndexCheck));
  iyIndexCheck = calloc((*ny = iyMax - iyMin + 1), sizeof(*iyIndexCheck));

  *image = calloc(*ny, sizeof(**image));
  for (iy = 0; iy < *ny; iy++)
    (*image)[iy] = calloc(*nx, sizeof(***image));

  for (i = 0; i < nRows; i++) {
    ix = ixData[i] - ixMin;
    iy = iyData[i] - iyMin;
    ixIndexCheck[ix] = 1;
    iyIndexCheck[iy] = 1;
    (*image)[iy][ix] = intensityData[i];
  }

  for (i = 0; i <= (ixMax - ixMin); i++) { // Changed from < to <=
    if (!ixIndexCheck[i]) {
      fprintf(stderr, "Error: image file is missing some x index values\n");
      /* Free allocated memory before returning */
      for (long y = 0; y < *ny; y++) {
        free((*image)[y]);
      }
      free(*image);
      free(ixIndexCheck);
      free(iyIndexCheck);
      free(ixData);
      free(iyData);
      free(intensityData);
      return 0;
    }
  }
  for (i = 0; i <= (iyMax - iyMin); i++) { // Changed from < to <=
    if (!iyIndexCheck[i]) {
      fprintf(stderr, "Error: image file is missing some y index values\n");
      /* Free allocated memory before returning */
      for (long y = 0; y < *ny; y++) {
        free((*image)[y]);
      }
      free(*image);
      free(ixIndexCheck);
      free(iyIndexCheck);
      free(ixData);
      free(iyData);
      free(intensityData);
      return 0;
    }
  }

  free(ixIndexCheck);
  free(iyIndexCheck);
  free(ixData);
  free(iyData);
  free(intensityData);

  return 1;
}

long AnalyzeImageData(double **image, long nx, long ny, int32_t *ROI, int32_t *spotROISize, int32_t *sizeLines,
                      DESPIKE_SETTINGS *despikeSettings, HOTPIXEL_SETTINGS *hotpixelSettings,
                      long intensityLevels, long saturationLevel, long backgroundHalfWidth, long lonerThreshold,
                      long lonerPasses, unsigned long flags, IMAGE_ANALYSIS *analysisResults,
                      SDDS_DATASET *SDDSspim, double *centerValue) {
  long ix, iy, iy0, iy1, ix0, ix1, nxROI, nyROI;
  double maxValue, minValue, background, value, sum1, sum2;
  long ixCenter, iyCenter;
  int64_t ixMax, ixMin;
  int32_t i;
  double *intensityHistogram, *lineBuffer = NULL, *x = NULL, *y = NULL;
  long spotROI[4];
  double mean, sum;

  /* Steps in image analysis:
   * . "Apply" the ROI
   * . Remove hot pixels, if requested
   * . Find the spot:
   *    optionally despike the image to remove noise
   *    scan each line for maximum until the maximum maximum is found
   * . Determine and subtract the background:
   *    accumulate histogram of pixel intensities.
   *    find the mode
   * . If requested, run "single-spot" filter.
   * . Check that ixCenter and iyCenter are consistent with having the
   *    spot ROI inside the pixel map.  If not, adjust.
   * . Sum over the spot ROI and subtract background*nx*ny.
   */

  /* set the ROI */
  if (ROI[0] >= 0 && ROI[1] >= 0 && ROI[0] < ROI[1]) {
    ix0 = ROI[0];
    ix1 = ROI[1];
    if (ix1 >= nx)
      ix1 = nx - 1;
  } else {
    ix0 = 0;
    ix1 = nx - 1;
  }
  nxROI = ix1 - ix0 + 1;

  if (ROI[2] >= 0 && ROI[3] >= 0 && ROI[2] < ROI[3]) {
    iy0 = ROI[2];
    iy1 = ROI[3];
    if (iy1 >= ny)
      iy1 = ny - 1;
  } else {
    iy0 = 0;
    iy1 = ny - 1;
  }
  nyROI = iy1 - iy0 + 1;

  if ((nyROI) < spotROISize[1] || (nxROI) < spotROISize[0])
    SDDS_Bomb("spot ROI is larger than ROI");

  /* Find the spot  */
  if (despikeSettings)
    if (!(lineBuffer = SDDS_Malloc(sizeof(*lineBuffer) * (nx > ny ? nx : ny))))
      SDDS_Bomb("memory allocation failure");
  minValue = DBL_MAX;
  maxValue = -DBL_MAX;
  ixCenter = iyCenter = -1;

  for (ix = ix0; ix <= ix1; ix++) {
    for (iy = iy0; iy <= iy1; iy++) {
      if ((flags & CLIP_NEGATIVE) && image[iy][ix] < 0)
        image[iy][ix] = 0;
      if (image[iy][ix] < 0 || image[iy][ix] >= intensityLevels)
        SDDS_Bomb("image intensity outside intensity level range");
    }
  }

  if (despikeSettings && (despikeSettings->flags & DESPIKE_KEEP)) {
    /* despike vertical lines */
    for (ix = ix0; ix <= ix1; ix++) {
      for (iy = iy0; iy <= iy1; iy++)
        lineBuffer[iy - iy0] = image[iy][ix];
      despikeData(lineBuffer, nyROI, despikeSettings->neighbors, despikeSettings->passes, despikeSettings->averageOf,
                  despikeSettings->threshold, 0);
      for (iy = iy0; iy <= iy1; iy++)
        image[iy][ix] = lineBuffer[iy - iy0];
    }
  }

  /* despike horizontal lines, also find point of max intensity */
  for (iy = iy0; iy <= iy1; iy++) {
    if (despikeSettings) {
      memcpy(lineBuffer, image[iy] + ix0, nxROI * sizeof(*lineBuffer));
      despikeData(lineBuffer, nxROI, despikeSettings->neighbors, despikeSettings->passes, despikeSettings->averageOf,
                  despikeSettings->threshold, 0);
      if (despikeSettings->flags & DESPIKE_KEEP)
        memcpy(image[iy] + ix0, lineBuffer, nxROI * sizeof(*lineBuffer));
    } else
      lineBuffer = image[iy] + ix0;
    index_min_max(&ixMin, &ixMax, lineBuffer, nxROI);
    if (lineBuffer[ixMax] > maxValue) {
      maxValue = lineBuffer[ixMax];
      ixCenter = ixMax + ix0; /* since lineBuffer copy starts at ix=ix0 */
      iyCenter = iy;
    }
    if (lineBuffer[ixMin] < minValue)
      minValue = lineBuffer[ixMin];
  }
  if (despikeSettings) {
    free(lineBuffer);
    lineBuffer = NULL;
  }
  if (flags & XCENTER_PARAM)
    ixCenter = centerValue[0];
  else
    centerValue[0] = ixCenter;
  if (flags & YCENTER_PARAM)
    iyCenter = centerValue[1];
  else
    centerValue[1] = iyCenter;
  if (ixCenter == -1 || iyCenter == -1)
    return 0;

  /* Determine the background */
  if (!(intensityHistogram = calloc(intensityLevels, sizeof(*intensityHistogram))))
    SDDS_Bomb("memory allocation failure");
  for (iy = iy0; iy <= iy1; iy++)
    make_histogram(intensityHistogram, intensityLevels, -0.5, intensityLevels + 0.5, image[iy] + ix0, nxROI, iy == iy0);
  index_min_max(&ixMin, &ixMax, intensityHistogram, intensityLevels);
  sum1 = sum2 = 0;
  for (i = ixMax - backgroundHalfWidth; i <= ixMax + backgroundHalfWidth; i++) {
    if (i < 0 || i >= intensityLevels)
      continue;
    sum1 += intensityHistogram[i];
    sum2 += intensityHistogram[i] * i;
  }
  if (sum1)
    background = sum2 / sum1;
  else
    background = ixMax;
  if (background < 0)
    background = 0;
  free(intensityHistogram);

  if (hotpixelSettings) {
    long min, max, pass;
    /* remove hot pixels:
     * 1. Histogram the intensity levels
     * 2. Identify pixels with levels that are >defined fraction below max
     * 3. Replace identified intensities with average of near neighbors
     */
    pass = hotpixelSettings->passes;
    while (pass--) {
      max = -(min = LONG_MAX);
      for (iy = iy0; iy <= iy1; iy++) {
        for (ix = ix0; ix <= ix1; ix++) {
          if (image[iy][ix] > max)
            max = image[iy][ix];
          if (image[iy][ix] < min)
            min = image[iy][ix];
        }
      }
      if (min >= max)
        SDDS_Bomb("Can't apply hotpixel filter (min=max). Are data non-integer?");
      for (iy = iy0; iy <= iy1; iy++)
        for (ix = ix0; ix <= ix1; ix++)
          if ((image[iy][ix] - min) > (max - min) * hotpixelSettings->fraction)
            replaceWithNearNeighbors(image, iy0, iy1, ix0, ix1, iy, ix, hotpixelSettings->distance);
    }
  }

  /* Compute new ROI for spot only */
  if (flags & XCENTER_CENTROID) {
    mean = sum = 0;
    for (ix = ix0; ix <= ix1; ix++)
      for (iy = iy0; iy <= iy1; iy++) {
        sum += image[iy][ix];
        mean += image[iy][ix] * ix;
      }
    mean /= sum;
    spotROI[0] = mean - spotROISize[0] / 2;
  } else {
    spotROI[0] = ixCenter - spotROISize[0] / 2;
  }
  spotROI[1] = spotROI[0] + spotROISize[0] - 1;
  if (spotROI[0] < ix0) {
    spotROI[0] = ix0;
    spotROI[1] = ix0 + spotROISize[0] - 1;
  } else if (spotROI[1] > ix1) {
    spotROI[1] = ix1;
    spotROI[0] = ix1 - spotROISize[0] + 1;
  }
  if (spotROI[0] < ix0 || spotROI[1] > ix1)
    SDDS_Bomb("spot ROI is larger than ROI for x"); /* shouldn't ever see this */

  if (flags & XCENTER_CENTROID) {
    mean = sum = 0;
    for (ix = ix0; ix <= ix1; ix++)
      for (iy = iy0; iy <= iy1; iy++) {
        sum += image[iy][ix];
        mean += image[iy][ix] * iy;
      }
    mean /= sum;
    spotROI[2] = mean - spotROISize[1] / 2;
  } else {
    spotROI[2] = iyCenter - spotROISize[1] / 2;
  }

  spotROI[3] = spotROI[2] + spotROISize[1] - 1;
  if (spotROI[2] < iy0) {
    spotROI[2] = iy0;
    spotROI[3] = iy0 + spotROISize[1] - 1;
  } else if (spotROI[3] > iy1) {
    spotROI[3] = iy1;
    spotROI[2] = iy1 - spotROISize[1] + 1;
  }
  if (spotROI[2] < iy0 || spotROI[3] > iy1)
    SDDS_Bomb("spot ROI is larger than ROI for y"); /* shouldn't ever see this */

  /* perform background removal */
  analysisResults->saturationCount = 0;
  analysisResults->integratedSpotIntensity = 0;
  analysisResults->backgroundKilledNegative = 0;
  analysisResults->backgroundKilledPositive = 0;
  saturationLevel -= (long)background; // Cast to long to avoid implicit conversion warning
  for (ix = spotROI[0]; ix <= spotROI[1]; ix++) {
    for (iy = spotROI[2]; iy <= spotROI[3]; iy++) {
      if ((value = image[iy][ix] - background) <= 0) {
        analysisResults->backgroundKilledNegative += 1;
        value = 0;
      }
      image[iy][ix] = value;
    }
  }
  if (flags & SYMMETRIC_BGREMOVAL) {
    for (ix = spotROI[0]; ix <= spotROI[1]; ix++) {
      for (iy = spotROI[2]; iy <= spotROI[3]; iy++) {
        long ox, oy, goodPixels;
        long ox0, ox1, oy0, oy1;
        if (image[iy][ix] > 0 && image[iy][ix] <= backgroundHalfWidth) {
          /* if no more than one of the pixels around this pixel are >backgroundHalfWidth, then
           * set this pixel to zero
           */
          ox0 = (ix == spotROI[0] ? 0 : -1);
          ox1 = (ix == spotROI[1] ? 0 : 1);
          oy0 = (iy == spotROI[2] ? 0 : -1);
          oy1 = (iy == spotROI[3] ? 0 : 1);
          /* the pixel itself is counted but guaranteed to be subtracted off, too */
          /* this avoids an extra test in the loop */
          goodPixels = (ox1 - ox0 + 1) * (oy1 - oy0 + 1);
          for (ox = ox0; ox <= ox1; ox++) {
            for (oy = oy0; oy <= oy1; oy++) {
              if (image[iy + oy][ix + ox] <= backgroundHalfWidth)
                goodPixels--;
            }
          }
          if (goodPixels <= 1) {
            analysisResults->backgroundKilledPositive += 1;
            image[iy][ix] = 0;
          }
        }
      }
    }
  }
  if (flags & REMOVE_LONERS) {
    while (lonerPasses-- > 0) {
      for (ix = spotROI[0]; ix <= spotROI[1]; ix++) {
        for (iy = spotROI[2]; iy <= spotROI[3]; iy++) {
          long ox, oy, company;
          long ox0, ox1, oy0, oy1;
          if (image[iy][ix] > 0) {
            /* if none of the pixels around this pixel are nonzero, then set it to zero.
             */
            ox0 = (ix == spotROI[0] ? 0 : -1);
            ox1 = (ix == spotROI[1] ? 0 : 1);
            oy0 = (iy == spotROI[2] ? 0 : -1);
            oy1 = (iy == spotROI[3] ? 0 : 1);
            /* the pixel itself is counted but guaranteed to be subtracted off, too */
            /* this avoids an extra test in the loop */
            company = (ox1 - ox0 + 1) * (oy1 - oy0 + 1);
            for (ox = ox0; ox <= ox1; ox++) {
              for (oy = oy0; oy <= oy1; oy++) {
                if (image[iy + oy][ix + ox] == 0)
                  company--;
              }
            }
            if (company <= lonerThreshold) {
              analysisResults->backgroundKilledPositive += 1;
              image[iy][ix] = 0;
            }
          }
        }
      }
    }
  }
  if (flags & ANTIHALO_BGREMOVAL) {
    long tryCount; // Renamed from 'try' to avoid confusion with C++ keyword
    for (tryCount = 0; tryCount < 2; tryCount++) {
      for (ix = spotROI[0]; ix <= spotROI[1]; ix++) {
        for (iy = spotROI[2]; iy < spotROI[3]; iy++) {
          if (image[iy][ix] > backgroundHalfWidth || image[iy + 1][ix] > backgroundHalfWidth)
            break;
          if (image[iy][ix]) {
            image[iy][ix] = 0;
            analysisResults->backgroundKilledPositive += 1;
          }
        }
        if (iy != spotROI[3])
          for (iy = spotROI[3]; iy > spotROI[2]; iy--) {
            if (image[iy][ix] > backgroundHalfWidth || image[iy - 1][ix] > backgroundHalfWidth)
              break;
            if (image[iy][ix]) {
              image[iy][ix] = 0;
              analysisResults->backgroundKilledPositive += 1;
            }
          }
      }
      for (iy = spotROI[2]; iy <= spotROI[3]; iy++) {
        for (ix = spotROI[0]; ix < spotROI[1]; ix++) {
          if (image[iy][ix] > backgroundHalfWidth || image[iy][ix + 1] > backgroundHalfWidth)
            break;
          if (image[iy][ix]) {
            image[iy][ix] = 0;
            analysisResults->backgroundKilledPositive += 1;
          }
        }
        if (ix != spotROI[1])
          for (ix = spotROI[1]; ix > spotROI[0]; ix--) {
            if (image[iy][ix] > backgroundHalfWidth || image[iy][ix - 1] > backgroundHalfWidth)
              break;
            if (image[iy][ix]) {
              image[iy][ix] = 0;
              analysisResults->backgroundKilledPositive += 1;
            }
          }
      }
    }
  }
  if (flags & SINGLE_SPOT) {
    short **connected, changed;
    double maxVal;
    long ix_max = 0, iy_max = 0;

    connected = tmalloc(sizeof(*connected) * nx);
    for (ix = 0; ix < nx; ix++)
      connected[ix] = tmalloc(sizeof(**connected) * ny);
    maxVal = -HUGE_VAL;
    for (ix = spotROI[0]; ix <= spotROI[1]; ix++)
      for (iy = spotROI[2]; iy <= spotROI[3]; iy++) {
        connected[ix][iy] = 0;
        if (image[iy][ix] > maxVal) { // Changed from image[ix][iy] to image[iy][ix]
          ix_max = ix;
          iy_max = iy;
          maxVal = image[iy][ix];
        }
      }
    connected[ix_max][iy_max] = 1;

    do {
      changed = 0;
      for (ix = spotROI[0]; ix <= spotROI[1]; ix++)
        for (iy = spotROI[2]; iy <= spotROI[3]; iy++) {
          if (!image[iy][ix] || connected[ix][iy])
            continue;
          if (ix > spotROI[0] && connected[ix - 1][iy]) {
            changed += (connected[ix][iy] = 1);
            continue;
          }
          if (ix < spotROI[1] && connected[ix + 1][iy]) {
            changed += (connected[ix][iy] = 1);
            continue;
          }
          if (iy > spotROI[2] && connected[ix][iy - 1]) {
            changed += (connected[ix][iy] = 1);
            continue;
          }
          if (iy < spotROI[3] && connected[ix][iy + 1]) {
            changed += (connected[ix][iy] = 1);
            continue;
          }
        }
      for (ix = spotROI[1]; ix >= spotROI[0]; ix--)
        for (iy = spotROI[3]; iy >= spotROI[2]; iy--) {
          if (!image[iy][ix] || connected[ix][iy])
            continue;
          if (ix > spotROI[0] && connected[ix - 1][iy]) {
            changed += (connected[ix][iy] = 1);
            continue;
          }
          if (ix < spotROI[1] && connected[ix + 1][iy]) {
            changed += (connected[ix][iy] = 1);
            continue;
          }
          if (iy > spotROI[2] && connected[ix][iy - 1]) {
            changed += (connected[ix][iy] = 1);
            continue;
          }
          if (iy < spotROI[3] && connected[ix][iy + 1]) {
            changed += (connected[ix][iy] = 1);
            continue;
          }
        }
    } while (changed);

    for (ix = spotROI[0]; ix <= spotROI[1]; ix++)
      for (iy = spotROI[2]; iy <= spotROI[3]; iy++)
        if (!connected[ix][iy])
          image[iy][ix] = 0;

    /* Free connected memory */
    for (ix = 0; ix < nx; ix++) {
      free(connected[ix]);
    }
    free(connected);
  }

  /* check for saturation */
  for (ix = spotROI[0]; ix <= spotROI[1]; ix++)
    for (iy = spotROI[2]; iy <= spotROI[3]; iy++)
      if (image[iy][ix] > saturationLevel)
        analysisResults->saturationCount += 1;

  /* find the spot intensity and centroids */
  analysisResults->spotCentroid[0] = analysisResults->spotCentroid[1] = 0;
  for (ix = spotROI[0]; ix <= spotROI[1]; ix++)
    for (iy = spotROI[2]; iy <= spotROI[3]; iy++) {
      analysisResults->integratedSpotIntensity += image[iy][ix];
      analysisResults->spotCentroid[0] += image[iy][ix] * ix;
      analysisResults->spotCentroid[1] += image[iy][ix] * iy;
    }
  if (analysisResults->integratedSpotIntensity)
    for (i = 0; i < 2; i++)
      analysisResults->spotCentroid[i] /= analysisResults->integratedSpotIntensity;

  /* find the spot size in y using central lines around the peak */
  if (!(lineBuffer = calloc(ny, sizeof(*lineBuffer))))
    SDDS_Bomb("memory allocation failure");
  if (!(x = calloc(ny, sizeof(*x))))
    SDDS_Bomb("memory allocation failure");
  if (!(y = calloc(ny, sizeof(*y))))
    SDDS_Bomb("memory allocation failure");
  for (ix = ixCenter - sizeLines[0] / 2; ix <= ixCenter + sizeLines[0] / 2; ix++) {
    if (ix < ix0 || ix > ix1)
      continue;
    for (iy = spotROI[2]; iy <= spotROI[3]; iy++)
      lineBuffer[iy] += image[iy][ix];
  }
  for (iy = spotROI[2]; iy <= spotROI[3]; iy++)
    y[iy] = lineBuffer[iy];

  DetermineBeamSizes(&analysisResults->spotSigma[1], &analysisResults->spotRange50[1], &analysisResults->spotRange65[1],
                     &analysisResults->spotRange80[1], lineBuffer, spotROI[2], spotROI[3]);
  free(lineBuffer);
  /* find the spot size in x using central lines around the peak */
  if (!(lineBuffer = calloc(nx, sizeof(*lineBuffer))))
    SDDS_Bomb("memory allocation failure");
  for (iy = iyCenter - sizeLines[1] / 2; iy <= iyCenter + sizeLines[1] / 2; iy++) {
    if (iy < iy0 || iy > iy1)
      continue;
    for (ix = spotROI[0]; ix <= spotROI[1]; ix++)
      lineBuffer[ix] += image[iy][ix];
  }
  DetermineBeamSizes(&analysisResults->spotSigma[0], &analysisResults->spotRange50[0], &analysisResults->spotRange65[0],
                     &analysisResults->spotRange80[0], lineBuffer, spotROI[0], spotROI[1]);
  free(lineBuffer);
  DetermineBeamParameters(image, spotROI, nx, ny, &analysisResults->S11, &analysisResults->S33, &analysisResults->rmsCrossProduct,
                          &analysisResults->phi, &analysisResults->majorAxis, &analysisResults->minorAxis);
  /* put results in the structure for return */
  analysisResults->peakSpotIntensity = maxValue - background;
  analysisResults->spotCenter[0] = ixCenter;
  analysisResults->spotCenter[1] = iyCenter;
  analysisResults->backgroundLevel = background;
  analysisResults->ROI[0] = ix0;
  analysisResults->ROI[1] = ix1;
  analysisResults->ROI[2] = iy0;
  analysisResults->ROI[3] = iy1;
  for (i = 0; i < 4; i++)
    analysisResults->spotROI[i] = spotROI[i];
  if (x)
    free(x);
  if (y)
    free(y);

  if (SDDSspim) {
    long i_row;
    if (!SDDS_StartPage(SDDSspim, (spotROI[1] - spotROI[0] + 1) * (spotROI[3] - spotROI[2] + 1)))
      SDDS_Bomb("Problem starting page for spot image output file.");
    if (!SDDS_SetParameters(SDDSspim, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, "nx", (short)(spotROI[1] - spotROI[0] + 1),
                            "ny", (short)(spotROI[3] - spotROI[2] + 1), NULL))
      SDDS_Bomb("Problem setting parameter values for spot image output file.");
    i_row = 0;
    for (iy = spotROI[2]; iy <= spotROI[3]; iy++) {
      for (ix = spotROI[0]; ix <= spotROI[1]; ix++) {
        if (!SDDS_SetRowValues(SDDSspim, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME, i_row++, "ix", (short)ix,
                               "iy", (short)iy, "Image", image[iy][ix], NULL)) {
          SDDS_Bomb("Problem setting row values for spot image output file.");
        }
      }
    }
    if (!SDDS_WritePage(SDDSspim)) {
      SDDS_Bomb("Problem writing page for spot image output file.");
    }
  }
  return 1;
}

void DetermineBeamSizes(double *sigma, double *Range50, double *Range65, double *Range80, double *lineBuffer, long i0, long i1) {
  double centroid, sum;
  long i, j;
  double pLevel[6] = {.10, .175, .25, .75, .825, .90};
  double pValue[6];

  centroid = sum = 0;
  *sigma = *Range80 = *Range65 = *Range50 = 0;
  for (i = i0; i <= i1; i++) {
    sum += lineBuffer[i];
    centroid += lineBuffer[i] * i;
  }
  if (sum) {
    centroid = centroid / sum;
    for (i = i0; i <= i1; i++)
      *sigma += lineBuffer[i] * sqr(i - centroid);
    *sigma = sqrt(*sigma / sum);

    /* integrate the intensity */
    for (i = i0 + 1; i <= i1; i++)
      lineBuffer[i] += lineBuffer[i - 1];
    if (lineBuffer[i1]) {
      for (i = i0; i <= i1; i++)
        lineBuffer[i] /= lineBuffer[i1];
      i = i0 + 1;
      for (j = 0; j < 6; j++) {
        pValue[j] = 0;
        while (i <= i1 && lineBuffer[i] < pLevel[j])
          i++;
        if (i > i1) { // Added check to prevent out-of-bounds access
          pValue[j] = i1;
        } else if (lineBuffer[i] == lineBuffer[i - 1])
          pValue[j] = i - 0.5;
        else
          pValue[j] = i - (lineBuffer[i] - pLevel[j]) / (lineBuffer[i] - lineBuffer[i - 1]);
      }
      *Range80 = pValue[5] - pValue[0];
      *Range65 = pValue[4] - pValue[1];
      *Range50 = pValue[3] - pValue[2];
    }
  }
}

void BlankOutImageData(double **image, long nx, long ny, int32_t *region) {
  long ix, iy, count = 0;
  for (ix = region[0]; ix <= region[1]; ix++)
    for (iy = region[2]; iy <= region[3]; iy++, count++)
      image[iy][ix] = 0;
}

void DetermineBeamParameters(double **image, long *spotROI, long nx, long ny, double *S11, double *S33,
                             double *rmsCrossProduct, double *phi, double *majorAxis, double *minorAxis) {
  long i, j;
  long x1, x2, y1, y2;
  double imageArea = 0, x2Ave = 0, y2Ave = 0, xyAve = 0, dominator = 0, xcentroid = 0, ycentroid = 0;

  x1 = spotROI[0];
  x2 = spotROI[1];
  y1 = spotROI[2];
  y2 = spotROI[3];

  /*calcuate the area of image(x,y) in a square defined by (x1,y1), (x1,y2), (x2,y2) and (x2,y1) */
  for (i = y1; i <= y2; i++)
    for (j = x1; j <= x2; j++) {
      imageArea += image[i][j];
      xcentroid += image[i][j] * j;
      ycentroid += image[i][j] * i;
    }
  if (imageArea == 0) {
    *rmsCrossProduct = *majorAxis = *minorAxis = DBL_MAX;
  } else {
    xcentroid = xcentroid / imageArea;
    ycentroid = ycentroid / imageArea;
    for (i = y1; i <= y2; i++)
      for (j = x1; j <= x2; j++) {
        x2Ave += sqr(j - xcentroid) * image[i][j];
        y2Ave += sqr(i - ycentroid) * image[i][j];
        xyAve += (i - ycentroid) * (j - xcentroid) * image[i][j];
      }
    x2Ave = x2Ave / imageArea;
    y2Ave = y2Ave / imageArea;
    xyAve = xyAve / imageArea;
    dominator = x2Ave * y2Ave - xyAve * xyAve;
    *S11 = x2Ave;
    *S33 = y2Ave;
    *rmsCrossProduct = xyAve;
    *phi = 0.5 * atan2(2 * xyAve, x2Ave - y2Ave) / PI * 180;
    if ((x2Ave + y2Ave - sqrt(sqr(x2Ave - y2Ave) + 4 * sqr(xyAve))) != 0) {
      *majorAxis = sqrt(2 * dominator / (x2Ave + y2Ave - sqrt(sqr(x2Ave - y2Ave) + 4 * sqr(xyAve))));
    } else {
      *majorAxis = DBL_MAX;
    }
    if ((x2Ave + y2Ave + sqrt(sqr(x2Ave - y2Ave) + 4 * sqr(xyAve))) != 0) {
      *minorAxis = sqrt(2 * dominator / (x2Ave + y2Ave + sqrt(sqr(x2Ave - y2Ave) + 4 * sqr(xyAve))));
    } else {
      *minorAxis = DBL_MAX;
    }
  }
}

void replaceWithNearNeighbors(double **image, long iy0, long iy1, long ix0, long ix1, long iyc, long ixc, long distance) {
  long ix, iy, nnn;
  double sum;

  if ((iyc - distance) > iy0)
    iy0 = iyc - distance;
  if ((iyc + distance) < iy1)
    iy1 = iyc + distance;

  if ((ixc - distance) > ix0)
    ix0 = ixc - distance;
  if ((ixc + distance) < ix1)
    ix1 = ixc + distance;

  sum = 0;
  nnn = 0;
  for (iy = iy0; iy <= iy1; iy++)
    for (ix = ix0; ix <= ix1; ix++) {
      if (ix == ixc && iy == iyc)
        continue;
      sum += image[iy][ix];
      nnn++;
    }

  if (nnn > 0)
    image[iyc][ixc] = sum / nnn;
}
