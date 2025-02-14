/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: hist2d.c
 * purpose: 2-d histogram program with output to sddscontour program
 *
 * Michael Borland, 1988, 1995.
 */
#include "mdb.h"
#include "SDDS.h"
#include "table.h"
#include "scan.h"

#if defined(linux) || (defined(_WIN32) && !defined(_MINGW))
#  include <omp.h>
#else
#  define NOTHREADS 1
#endif

#define SET_X_PARAMETERS 0
#define SET_Y_PARAMETERS 1
#define SET_NORMALIZE 2
#define SET_SMOOTH 3
#define SET_WEIGHTS 4
#define SET_VERBOSE 5
#define SET_COLUMNS 6
#define SET_OUTPUTNAME 7
#define SET_SAMESCALE 8
#define SET_COMBINE 9
#define SET_PIPE 10
#define SET_MINIMUM_SCALE 11
#define SET_Z_COLUMN 12
#define SET_COPY_PARAMETERS 13
#define SET_INCLUDE_XY 14
#define SET_XBINSIZE 15
#define SET_YBINSIZE 16
#define SET_SPREAD 17
#define SET_THREADS 18
#define N_OPTIONS 19

FILE* outfile;
char *option[N_OPTIONS] = {
    "xparameters", "yparameters", "normalize",
    "smooth", "weights", "verbose", "columns",
    "outputname", "samescale", "combine", "pipe",
    "minimumscale", "zColumn", "copyparameters",
    "includeXY", "xBinSize", "yBinSize", "spread",
    "threads",
    } ;

char *USAGE="sddshist2d [<inputfile>] [<outputfile>] [-pipe[=input][,output]] \n\
-columns={<xname>,<yname>|<yname>} \n\
[-outputname=<string>] [-samescale] [-combine] \n\
{[-xparameters=<n_bins>[,<lower>,<upper>]] | [-xbinsize=<value>[,{<padBins>|<padBins0>,<padBins1>}]]}\n\
{[-yparameters=<n_bins>[,<lower>,<upper>]] | [-ybinsize=<value>[,{<padBins>|<padBins0>,<padBins1>}]]}\n\
[-minimumScale=<value>]\n\
[-verbose] [-normalize[=sum]] [-smooth[=<passes>]] \n\
[-spread={x|y}parameter=<parameterName>,{x|y}value=<value>,[nsigma=<value>][,fold][,unnormalized]]\n\
[-weights=<column-name>[,average]] [-zColumn=<column-name>[,bins=<n_bins>,lower=<lower>,upper=<upper>]]\n\
[-copyParameters] [-includeXY] [-threads=<integer>]\n\n\
Produces output suitable for use with contour.\n\
-columns         Names of x and y columns to histogram.\n\
-outputName      Name of output column. Defaults to \"frequency\".\n\
-sameScale       Fix scales across multiple pages.\n\
-combine         Combine data from all input pages into a single histogram.\n\
-minimumScale    Minimum range in x and y for output histogram.\n\
-normalize       Normalize the histogram to unit amplitude or sum.\n\
-smooth          Apply nearest-neighbor averaging.\n\
-spread          Apply gaussian spread function with given sigmas.\n\
                 If given, the output name defaults to \"Density\".\n\
                 If the \"fold\" qualifier is given, contributions at the \n\
                 boundaries fold back toward the interior. By default, the output\n\
                 is normalized to unit sum, unless the unnormalized qualifier is\n\
                 given.\n\
-weights         Use named column to weight the histogram. If average qualifier given,\n\
                 the output is normalized to the number of hits in each bin, so that one\n\
                 gets the average of the weights in the bin.\n\
-zColumn         Split histograms according to a third quantity.\n\
-copyParameters  Copy parameters from input to output.\n\
-includeXY       Include x and y columns in output.\n\
-threads         Number of threads to use. Unlikely to help unless -spread is used.\n\
Program by Michael Borland. (This is version 5, August 2023)\n";

typedef struct param_store {
/* array for parameter data.  The address of the data for the ith parameter
 * is parameter[i].  So *(<type-name> *)parameter[i] gives the data itself.  For type
 * SDDS_STRING the "data itself" is actually the address of the string, and the type-name
 * is "char *".
 */
void **param;
short filled;
struct param_store *next;
} PARAM_STORE;

PARAM_STORE *saveParameters(PARAM_STORE *context, SDDS_DATASET *SDDSin);
PARAM_STORE *setParameters(PARAM_STORE *context, SDDS_DATASET *SDDSout);

long define_sddscontour_parameters(SDDS_TABLE *output, SDDS_TABLE *input, char *varname, char *rootname);
long set_sddscontour_parameters(SDDS_TABLE *output, char *rootname, long dimen, double min, double delta);
long find2d_min_max(double *min, double *max, double **data, long *rows, long sets);

int main(int argc, char **argv)
{
  long i_arg, copyParameters=0;
  SDDS_TABLE SDDS_in, SDDS_out;
  SCANNED_ARG *scanned;
  char *input, *output;
  double **histogram, **weightSum;
  long **count;
  long i, j, nx, ny, ix, iy, do_normalize, n_smooth_passes, *rows, nz, iz, includeXY;
  long n_binned, n_pages, verbose, one_page_per_line, weights_average, spreadUnnormalized;
  double **xdata, **ydata, **weight, **zdata, z_center;
  double dx, dy, dz=0;
  long xPad[2]={0,0}, yPad[2]={0,0};
  double users_xmin, users_xmax, users_ymin, users_ymax, users_zmin, users_zmax;
  double xmin, xmax, ymin, ymax, zmin, zmax;
  double minimumScale;
  double hrange, middle;
  char *weight_column, *output_name, *output_units;
  long tablecount, samescale, combine;
  char *xname, *yname, *zname;
  unsigned long pipeFlags, dummyFlags=0;
  PARAM_STORE *paramContext, paramStore;
  short doSpread = 0, doFoldSpread = 0;
  char *spreadParameter[2] = {NULL, NULL};
  double spreadValue[2] = {-1,-1}, spreadSigmas=5;
  double *spreadValuePerPage[2] = {NULL, NULL};
  int threads=1;
  
  if (argc<3)
    bomb(NULL, USAGE);

  input = output = NULL;
  users_xmin = users_xmax = 0;
  users_ymin = users_ymax = 0;
  users_zmin = users_zmax = 0;
  nx = ny = 21;
  dx = dy = -1;
  nz = 1;
  do_normalize = n_smooth_passes = verbose = 0;
  weight_column = NULL;
  weights_average = 0;
  spreadUnnormalized = 0;
  samescale = combine = 0;
  yname = xname = zname = NULL;
  output_name = output_units = NULL;
  one_page_per_line = 0;
  pipeFlags = 0;
  minimumScale = 0;
  includeXY = 0;

  scanargs(&scanned, argc, argv); 
  for (i_arg=1; i_arg<argc; i_arg++) {
    if (scanned[i_arg].arg_type==OPTION) {
      /* process options here */
      switch (match_string(scanned[i_arg].list[0],
                           option, N_OPTIONS, 0)) {
      case SET_Z_COLUMN:
	zname = scanned[i_arg].list[1];
	if (scanned[i_arg].n_items>2) {
	  scanned[i_arg].n_items -=2;
	  if (!scanItemList(&dummyFlags, scanned[i_arg].list+2, &scanned[i_arg].n_items, 0,
                            "bins", SDDS_LONG, &nz, 1, 0,
                            "lower", SDDS_DOUBLE, &users_zmin, 1, 0,
			    "upper", SDDS_DOUBLE, &users_zmax, 1, 0,
                            NULL))
            SDDS_Bomb("Invalid -zColumn syntax");
	  scanned[i_arg].n_items +=2;
	}
	if (nz<1)
	  nz = 21;
	break;
      case SET_X_PARAMETERS:
        if (scanned[i_arg].n_items==2) {
          if (1!=sscanf(scanned[i_arg].list[1], "%ld", &nx) ||
              nx<=2)
            SDDS_Bomb("invalid number of bins for x");
        }
        else if (scanned[i_arg].n_items==4) {
          if (1!=sscanf(scanned[i_arg].list[1], "%ld", &nx) ||
              1!=sscanf(scanned[i_arg].list[2], "%lf", &users_xmin) ||
              1!=sscanf(scanned[i_arg].list[3], "%lf", &users_xmax) ||
              nx<=2 || users_xmin>=users_xmax )
            SDDS_Bomb("invalid -x_parameters values");
        }
        else
          SDDS_Bomb("wrong number of items for -x_parameters");
        break;
      case SET_Y_PARAMETERS:
        if (scanned[i_arg].n_items==2) {
          if (1!=sscanf(scanned[i_arg].list[1], "%ld", &ny) ||
              ny<=2)
            SDDS_Bomb("invalid number of bins for y");
        }
        else if (scanned[i_arg].n_items==4) {
          if (1!=sscanf(scanned[i_arg].list[1], "%ld", &ny) ||
              1!=sscanf(scanned[i_arg].list[2], "%lf", &users_ymin) ||
              1!=sscanf(scanned[i_arg].list[3], "%lf", &users_ymax) ||
              ny<=2 || users_ymin>=users_ymax )
            SDDS_Bomb("invalid -y_parameters values");
        }
        else
          SDDS_Bomb("wrong number of items for -y_parameters");
        break;
      case SET_XBINSIZE:
        if (scanned[i_arg].n_items<2 || scanned[i_arg].n_items>4)
          SDDS_Bomb("invalid -xBinSize syntax");
        if (1!=sscanf(scanned[i_arg].list[1], "%le", &dx) || dx<=0)
            SDDS_Bomb("invalid bin size given for x");
	if (scanned[i_arg].n_items==3) {
	  long pad;
	  if (1!=sscanf(scanned[i_arg].list[2], "%ld", &pad) || pad<0)
	    SDDS_Bomb("invalid padding given for x");
	  xPad[0] = (pad/2+0.5);
	  xPad[1] = pad-xPad[0];
	}
	if (scanned[i_arg].n_items==4) {
	  if ((1!=sscanf(scanned[i_arg].list[2], "%ld", &xPad[0]) || xPad[0]<0) ||
	      (1!=sscanf(scanned[i_arg].list[3], "%ld", &xPad[1]) || xPad[1]<0))
	    SDDS_Bomb("invalid padding given for x");
	}
        break;
      case SET_YBINSIZE:
        if (scanned[i_arg].n_items<2 || scanned[i_arg].n_items>4)
          SDDS_Bomb("invalid -yBinSize syntax");
        if (1!=sscanf(scanned[i_arg].list[1], "%le", &dy) || dy<=0)
            SDDS_Bomb("invalid bin size given for y");
	if (scanned[i_arg].n_items==3) {
	  long pad;
	  if (1!=sscanf(scanned[i_arg].list[2], "%ld", &pad) || pad<0)
	    SDDS_Bomb("invalid padding given for y");
	  yPad[0] = (pad/2+0.5);
	  yPad[1] = pad-yPad[0];
	}
	if (scanned[i_arg].n_items==4) {
	  if ((1!=sscanf(scanned[i_arg].list[2], "%ld", &yPad[0]) || yPad[0]<0) ||
	      (1!=sscanf(scanned[i_arg].list[3], "%ld", &yPad[1]) || yPad[1]<0))
	    SDDS_Bomb("invalid padding given for y");
	}
        break;
      case SET_MINIMUM_SCALE:
        if (scanned[i_arg].n_items!=2 || sscanf(scanned[i_arg].list[1], "%lf", &minimumScale)!=1 ||
              minimumScale<=0)
          SDDS_Bomb("invalid -minimumScale syntax/value");
        break;
      case SET_NORMALIZE:
        if (scanned[i_arg].n_items==2) {
          if (strncmp(scanned[i_arg].list[1], "sum", strlen(scanned[i_arg].list[1]))==0)
            do_normalize = 2;
          else
            SDDS_Bomb("invalid -normalize syntax");
        }
        else
          do_normalize = 1;
        break;
      case SET_SMOOTH:
        if (scanned[i_arg].n_items==1)
          n_smooth_passes = 1;
        else if (scanned[i_arg].n_items==2) {
          if (sscanf(scanned[i_arg].list[1], "%ld", &n_smooth_passes)!=1 ||
              n_smooth_passes<1)
            SDDS_Bomb("invalid -smooth syntax");
        }
        else
          SDDS_Bomb("invalid -smooth syntax");
        break;
      case SET_WEIGHTS:
        if (scanned[i_arg].n_items!=2 && scanned[i_arg].n_items!=3)
          SDDS_Bomb("invalid -weights syntax");
        weight_column = scanned[i_arg].list[1];
        weights_average = 0;
        if (scanned[i_arg].n_items==3) {
          if (strncmp(scanned[i_arg].list[2], "average", strlen(scanned[i_arg].list[2]))==0)
            weights_average = 1;
          else
            SDDS_Bomb("invalid -weights syntax");
        }
        break;
      case SET_VERBOSE:
        verbose = 1;
        break;
      case SET_COLUMNS:
        if (scanned[i_arg].n_items!=3 && scanned[i_arg].n_items!=2)
          SDDS_Bomb("invalid -columns syntax");
        if (scanned[i_arg].n_items==2) {
          yname = scanned[i_arg].list[1];
          one_page_per_line = 1;
        }
        else {
          xname = scanned[i_arg].list[1];
          yname = scanned[i_arg].list[2];
        }
        break;
      case SET_OUTPUTNAME:
        if (scanned[i_arg].n_items!=2)
          SDDS_Bomb("invalid -outputname syntax");
        output_name = scanned[i_arg].list[1];
        break;
      case SET_SAMESCALE:
        samescale = 1;
        break;
      case SET_COMBINE:
        combine = 1;
        break;
      case SET_PIPE:
        if (!processPipeOption(scanned[i_arg].list+1, scanned[i_arg].n_items-1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case SET_COPY_PARAMETERS:
        copyParameters = 1;
        paramContext = &paramStore;
        paramStore.filled = -1;  /* indicates root node */
        break;
      case SET_INCLUDE_XY:
        includeXY = 1;
        break;
      case SET_SPREAD:
	spreadParameter[0] = spreadParameter[1] = NULL;
	spreadValue[0] = spreadValue[1] = -1;
	scanned[i_arg].n_items -= 1;
	spreadSigmas = 5;
	doSpread = 1;
	doFoldSpread = 0;
        spreadUnnormalized = 0;
	if (!scanItemList(&dummyFlags, scanned[i_arg].list+1, &scanned[i_arg].n_items, 0,
			  "xparameter", SDDS_STRING, &spreadParameter[0], 1, 0,
			  "yparameter", SDDS_STRING, &spreadParameter[1], 1, 0,
			  "xvalue", SDDS_DOUBLE, &spreadValue[0], 1, 0,
			  "yvalue", SDDS_DOUBLE, &spreadValue[1], 1, 0,
			  "nsigma", SDDS_DOUBLE, &spreadSigmas, 1, 0,
			  "fold", -1, NULL, 0, 1,
			  "unnormalized", -1, NULL, 0, 2,
			  NULL) || spreadSigmas<=0)
	  SDDS_Bomb("Invalid -spread syntax");
	if (dummyFlags&1)
	  doFoldSpread = 1;
        if (dummyFlags&2)
          spreadUnnormalized = 1;
	if (!spreadParameter[0] && spreadValue[0]<0)
	  SDDS_Bomb("Invalid -spread syntax: give x parameter or value");
	if (spreadParameter[0] && spreadValue[0]>=0)
	  SDDS_Bomb("Invalid -spread syntax: give only one of x parameter or value");
	if (!spreadParameter[1] && spreadValue[1]<0)
	  SDDS_Bomb("Invalid -spread syntax: give y parameter or value");
	if (spreadParameter[1] && spreadValue[1]>=0)
	  SDDS_Bomb("Invalid -spread syntax: give only one of y parameter or value");
	break;
      case SET_THREADS:
        if (scanned[i_arg].n_items!=2 || !sscanf(scanned[i_arg].list[1], "%d", &threads) ||
            threads<=1)
          SDDS_Bomb("invalid -threads syntax");
        break;
      default:
        bomb("unknown option given", USAGE);
        break;
      }
    }
    else {
      if (!input)
        input = scanned[i_arg].list[0];
      else if (!output)
        output = scanned[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames seen");
    }
  }

  processFilenames("sddshist2d", &input, &output, pipeFlags, 0, NULL);

  if (!yname || SDDS_StringIsBlank(yname) || (xname && SDDS_StringIsBlank(xname)))
    SDDS_Bomb("invalid/missing -columns option");

  if (!SDDS_InitializeInput(&SDDS_in, input)) {
    fprintf(stderr, "problem initializing file %s\n", input);
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  }
  SDDS_SetColumnMemoryMode(&SDDS_in, DONT_TRACK_COLUMN_MEMORY_AFTER_ACCESS);
  if (!output_name) {
    if (doSpread) 
      output_name = "Density";
    else if (!weights_average)
      output_name = "frequency";
    else
      SDDS_CopyString(&output_name, weight_column);
  }

  output_units = NULL;
  if (weights_average && !SDDS_GetColumnInformation(&SDDS_in, "units", &output_units, SDDS_GET_BY_NAME, weight_column))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);

  if (!SDDS_InitializeOutput(&SDDS_out, SDDS_BINARY, 0, NULL, "sddshist2d output", output) ||
      !define_sddscontour_parameters(&SDDS_out, &SDDS_in, "Variable1Name", xname) ||
      !define_sddscontour_parameters(&SDDS_out, &SDDS_in, "Variable2Name", yname) ||
      SDDS_DefineColumn(&SDDS_out, output_name, NULL, output_units, NULL, NULL, SDDS_DOUBLE, 0)<0)
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  if (includeXY && 
      (!SDDS_TransferColumnDefinition(&SDDS_out, &SDDS_in, xname, NULL) ||
       !SDDS_TransferColumnDefinition(&SDDS_out, &SDDS_in, yname, NULL)))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  if (zname && (!define_sddscontour_parameters(&SDDS_out, &SDDS_in, "ZColumnName", zname) || 
                SDDS_DefineParameter(&SDDS_out,"zCenter",NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)<0))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  if (copyParameters && !SDDS_TransferAllParameterDefinitions(&SDDS_out, &SDDS_in, 0))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  if (!SDDS_WriteLayout(&SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  
  xdata = ydata = weight = zdata = NULL;
  rows = NULL;
  n_pages = 0;
  while ((tablecount=SDDS_ReadTable(&SDDS_in))>0) {
    xdata  = SDDS_Realloc(xdata, sizeof(*xdata)*(n_pages+1));
    ydata  = SDDS_Realloc(ydata, sizeof(*ydata)*(n_pages+1));
    if (doSpread) {
      long is;
      for (is=0; is<2; is++) {
	spreadValuePerPage[is] = SDDS_Realloc(spreadValuePerPage[is], sizeof(**spreadValuePerPage)*(n_pages+1));
	if (spreadParameter[is]) {
	  if (!SDDS_GetParameterAsDouble(&SDDS_in, spreadParameter[is], &spreadValuePerPage[is][n_pages]))
	    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
	} else
	  spreadValuePerPage[is][n_pages] = spreadValue[is];
      }
    }
    if (zname)
      zdata = SDDS_Realloc(zdata, sizeof(*zdata)*(n_pages+1));
    if (weight_column)
      weight = SDDS_Realloc(weight, sizeof(*xdata)*(n_pages+1));
    rows   = SDDS_Realloc(rows, sizeof(*xdata)*(n_pages+1));
    if (xname && !(xdata[n_pages] = SDDS_GetColumnInDoubles(&SDDS_in, xname))) {
      fprintf(stderr, "problem getting data for x quantity (%s)\n", xname);
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
    }
    if (!(ydata[n_pages] = SDDS_GetColumnInDoubles(&SDDS_in, yname))) {
      fprintf(stderr, "problem getting data for y quantity (%s)\n", yname);
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
    }
    if (zname && !(zdata[n_pages] = SDDS_GetColumnInDoubles(&SDDS_in, zname))) {
      fprintf(stderr, "problem getting data for z column (%s)\n", zname);
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
    }
    if (weight_column && !(weight[n_pages]=SDDS_GetColumnInDoubles(&SDDS_in, weight_column))) {
      fprintf(stderr, "problem getting data for weight quantity (%s)\n", weight_column);
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
    }
    if (copyParameters)
      paramContext = saveParameters(paramContext, &SDDS_in);
    if ((rows[n_pages] = SDDS_CountRowsOfInterest(&SDDS_in))<threads)
      threads = 1;
    n_pages++;
  }
  if (!SDDS_Terminate(&SDDS_in))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  if (copyParameters)
    paramContext = &paramStore;

  if (one_page_per_line) {
    users_xmin = 0;
    users_xmax = (n_pages-1)*(1+1e-7);
    dx = 1;
    nx = n_pages;
    combine = 1;
    for (j=0; j<n_pages; j++) {
      xdata[j] = tmalloc(sizeof(**xdata)*rows[j]);
      for (i=0; i<rows[j]; i++)
        xdata[j][i] = j;
    }
  } else if (dx>0) {
    find2d_min_max(&xmin, &xmax, xdata, rows, n_pages);
    nx = (xmax-xmin)/dx + 1;
    if (verbose) 
      fprintf(stderr, "Using %ld bins in x, range is [%le, %le]\n",
	      nx, xmin, xmax);
    if (xPad[0] || xPad[1]) {
      nx += xPad[0] + xPad[1];
      xmin -= (dx*xPad[0]);
      xmax += (dx*xPad[1]);
      if (verbose) 
	fprintf(stderr, "Expanding by %ld, %ld bins in x, new range is [%le, %le] with %ld bins\n",
		xPad[0], xPad[1], xmin, xmax, nx);
    }
    users_xmin = xmin;
    users_xmax = xmax;
  }
  if (dy>0) {
    find2d_min_max(&ymin, &ymax, ydata, rows, n_pages);
    ny = (ymax-ymin)/dy + 1;
    if (yPad[0] || yPad[1]) {
      ny += yPad[0] + yPad[1];
      ymin -= (dy*yPad[0]);
      ymax += (dy*yPad[1]);
      if (verbose) 
	fprintf(stderr, "Expanding by %ld, %ld bins in y, new range is [%le, %le] with %ld bins\n",
		yPad[0], yPad[1], ymin, ymax, ny);
    }
    if (verbose) 
      fprintf(stderr, "Using %ld bins in y, range is [%le, %le]\n",
	      ny, ymin, ymax);
    users_ymin = ymin;
    users_ymax = ymax;
  }
  histogram = tmalloc(sizeof(*histogram)*nz);
  weightSum = tmalloc(sizeof(*weightSum)*nz);
  count = tmalloc(sizeof(*count)*nz);
  for (i=0; i<nz; i++) {
    histogram[i] = tmalloc(sizeof(**histogram)*nx*ny);
    weightSum[i] = tmalloc(sizeof(**weightSum)*nx*ny);
    count[i] = tmalloc(sizeof(**count)*nx*ny);
  }
  if (samescale || combine) {
    if (users_xmin==users_xmax) {
      /* auto-scale in x */
      find2d_min_max(&xmin, &xmax, xdata, rows, n_pages);
      if (xmin==xmax) {
        if (!minimumScale)
          SDDS_Bomb("can't auto-scale in x--no spread to data.  Try using the -minimumScale option.");
        xmin -= minimumScale/2;
        xmax += minimumScale/2;
      }
      hrange = 1.0001*(xmax-xmin)/2.;
      middle = (xmax+xmin)/2.;
      xmax = middle+hrange;
      xmin = middle-hrange;
      users_xmin = xmin;
      users_xmax = xmax;
    }

    if (users_ymin==users_ymax) {
      /* auto-scale in y */
      find2d_min_max(&ymin, &ymax, ydata, rows, n_pages);
      if (ymin==ymax) {
        if (!minimumScale)
          SDDS_Bomb("can't auto-scale in y--no spread to data.  Try using the -minimumScale option.");
        ymin -= minimumScale/2;
        ymax += minimumScale/2;
      }
      hrange = 1.0001*(ymax-ymin)/2.;
      middle = (ymax+ymin)/2.;
      ymax = middle+hrange;
      ymin = middle-hrange;
      users_ymin = ymin;
      users_ymax = ymax;
    }
    if (zname && users_zmin==users_zmax) {
      /* auto-scale in y */
      find2d_min_max(&zmin, &zmax, zdata, rows, n_pages);
      if (zmin==zmax) {
        if (!minimumScale)
          SDDS_Bomb("can't auto-scale in y--no spread to data.  Try using the -minimumScale option.");
        zmin -= minimumScale/2;
        zmax += minimumScale/2;
      }
      hrange = 1.0001*(zmax-zmin)/2.;
      middle = (zmax+zmin)/2.;
      zmax = middle+hrange;
      zmin = middle-hrange;
      users_zmin = zmin;
      users_zmax = zmax;
    }
  }
  
  for (j=0; j<n_pages; j++) {
    if (j==0 || !combine)
      for (iz=0; iz<nz; iz++)
        for (ix=0; ix<nx; ix++)
          for (iy=0; iy<ny; iy++) {
            histogram[iz][ix*ny+iy] = 0.;
            weightSum[iz][ix*ny+iy] = 0;
            count[iz][ix*ny+iy] = 0;
          }
    if (users_xmin==users_xmax) {
      /* auto-scale in x */
      find_min_max(&xmin, &xmax, xdata[j], rows[j]);
      if (xmin==xmax) {
        if (!minimumScale)
          SDDS_Bomb("can't auto-scale in x--no spread to data.  Try using the -minimumScale option.");
        xmin -= minimumScale/2;
        xmax += minimumScale/2;
      }
      hrange = 1.0001*(xmax-xmin)/2.;
      middle = (xmax+xmin)/2.;
      xmax = middle+hrange;
      xmin = middle-hrange;
    }
    else {
      xmin = users_xmin;
      xmax = users_xmax;
    }
    dx = (xmax-xmin)/(nx-1);
    
    if (users_ymin==users_ymax) {
      /* auto-scale in y */
      find_min_max(&ymin, &ymax, ydata[j], rows[j]);
      if (ymin==ymax) {
        if (!minimumScale)
          SDDS_Bomb("can't auto-scale in y--no spread to data.  Try using the -minimumScale option.");
        ymin -= minimumScale/2;
        ymax += minimumScale/2;
      }
      hrange = 1.0001*(ymax-ymin)/2.;
      middle = (ymax+ymin)/2.;
      ymax = middle+hrange;
      ymin = middle-hrange;
    }
    else {
      ymin = users_ymin;
      ymax = users_ymax;
    }
    dy = (ymax-ymin)/(ny-1);
    
    if (zname) {
      if (users_zmin==users_zmax) {
        /* auto-scale in y */
        find_min_max(&zmin, &zmax, zdata[j], rows[j]);
        if (zmin==zmax) {
          if (!minimumScale)
            SDDS_Bomb("can't auto-scale in y--no spread to data.  Try using the -minimumScale option.");
          zmin -= minimumScale/2;
          zmax += minimumScale/2;
        }
        hrange = 1.0001*(zmax-zmin)/2.;
        middle = (zmax+zmin)/2.;
        zmax = middle+hrange;
        zmin = middle-hrange;
      }
      else {
        zmin = users_zmin;
        zmax = users_zmax;
      }
      dz = (zmax-zmin)/(nz-1);
    }
#if !defined(NOTHREADS)
    omp_set_num_threads(threads);
#endif
    n_binned = 0;
#pragma omp parallel
    {
    long i, i1, i2, iz, ix, iy;
    int myid;
    double w = 1, x, y, z;
#if !defined(NOTHREADS)
    myid = omp_get_thread_num();
#else
    myid = 0;
#endif
    if (rows[j]<threads)
      SDDS_Bomb("fewer rows than threads!");
    i1 = myid*(rows[j]/threads);
    i2 = i1 + rows[j]/threads;
    if (myid==(threads-1))
      i2 = rows[j];
    for (i=i1; i<i2; i++) {
      x = xdata[j][i];
      y = ydata[j][i];
      if (x<xmin || x>=xmax || y<ymin || y>=ymax) 
        continue;
      iz = 0;
      if (zname) {
        z = zdata[j][i];
        if (z<zmin || z>=zmax)
          continue;
        iz = (z-zmin)/dz + 0.5;
      }
      if (weight)
        w = weight[j][i];
      ix = (x-xmin)/dx + 0.5;
      iy = (y-ymin)/dy + 0.5;
#pragma omp atomic
      weightSum[iz][ix*ny+iy] += w;
#pragma omp atomic
      count[iz][ix*ny+iy] ++;
      if (doSpread) {
	long ix0, ix1, iy0, iy1, ixu, iyu;
        double sum;
	if ((ix0 = ix - (spreadSigmas*spreadValuePerPage[0][j]/dx + 1))<0 && !doFoldSpread)
	  ix0 = 0;
	if ((ix1 = ix + (spreadSigmas*spreadValuePerPage[0][j]/dx + 1))>=nx && !doFoldSpread)
	  ix1 = nx-1;
	if ((iy0 = iy - (spreadSigmas*spreadValuePerPage[1][j]/dy + 1))<0 && !doFoldSpread)
	  iy0 = 0;
	if ((iy1 = iy + (spreadSigmas*spreadValuePerPage[1][j]/dy + 1))>=ny && !doFoldSpread)
	  iy1 = ny-1;
        sum = 0;
        /* first compute a normalization factor for the kernel */
	for (ix=ix0; ix<=ix1; ix++) {
	  for (iy=iy0; iy<=iy1; iy++) {
	    ixu = ix;
	    iyu = iy;
	    if (ixu<0)
	      ixu = -ixu;
	    if (iyu<0)
	      iyu = -iyu;
	    if (ixu>=nx)
	      ixu = nx - (ixu - (nx-2));
	    if (iyu>=ny)
	      iyu = ny - (iyu - (ny-2));
	    if (ixu>=0 && iyu>=0 && ixu<=(nx-1) && iyu<=(ny-1)) {
              sum += exp(-sqr((ixu*dx + xmin - x)/spreadValuePerPage[0][j])/2 
                         -sqr((iyu*dy + ymin - y)/spreadValuePerPage[1][j])/2);
            }
          }
        }
        /* sum up the contributions across the grid, with kernel sum normalized to 1 */
	for (ix=ix0; ix<=ix1; ix++) {
	  for (iy=iy0; iy<=iy1; iy++) {
	    ixu = ix;
	    iyu = iy;
	    if (ixu<0)
	      ixu = -ixu;
	    if (iyu<0)
	      iyu = -iyu;
	    if (ixu>=nx)
	      ixu = nx - (ixu - (nx-2));
	    if (iyu>=ny)
	      iyu = ny - (iyu - (ny-2));
	    if (ixu>=0 && iyu>=0 && ixu<=(nx-1) && iyu<=(ny-1))
#pragma omp atomic
	      histogram[iz][ixu*ny+iyu] += w*
                exp(-sqr((ixu*dx + xmin - x)/spreadValuePerPage[0][j])/2 
                    -sqr((iyu*dy + ymin - y)/spreadValuePerPage[1][j])/2)/sum;
          }
        }
      } else {
#pragma omp atomic
	histogram[iz][ix*ny+iy] += w;
      }
#pragma omp atomic
      n_binned++;
    }
    }
    if (doSpread) {
      double sum, factor;
      for (iz=0; iz<nz; iz++) {
	sum = 0;
	for (ix=0; ix<nx; ix++) {
	  for (iy=0; iy<ny; iy++)
	    sum += histogram[iz][ix*ny+iy];
	}
	if (sum>0) {
          if (spreadUnnormalized)
            /* the weights are absolute */
            factor = 1.;
          else
            /* the weights are relative, so we normalize out the sum over the histogram */
            factor = 1./sum;
          factor /= dx*dy; /* to get density instead of count */
	  for (ix=0; ix<nx; ix++) {
	    for (iy=0; iy<ny; iy++) {
	      histogram[iz][ix*ny+iy] *= factor;
	    }
	  }
	}
      }
    }
    if (verbose)
      fprintf(stderr, "page %ld: %ld of %ld points binned\n", j, n_binned, rows[j]);
    free(xdata[j]);
    free(ydata[j]);
    if (zname)
      free(zdata[j]);
    if (weight)
      free(weight[j]);

    if (combine && j!=n_pages-1)
      continue;
    
    if (weights_average) {
      for (iz=0; iz<nz; iz++)
        for (ix=0; ix<nx; ix++)
          for (iy=0; iy<ny; iy++)
            if (count[iz][ix*ny+iy])
              histogram[iz][ix*ny+iy] /= count[iz][ix*ny+iy];
    }
    
    if (n_smooth_passes) {
      double *new_hist, *tmp, sum;
      long jx, jy, nsum, ip, dix;
      
      new_hist = tmalloc(sizeof(*new_hist)*nx*ny);
      dix = one_page_per_line?0:1; 
      for (iz=0; iz<nz; iz++) {
        for (ip=0; ip<n_smooth_passes; ip++) { 
          for (ix=0; ix<nx; ix++) { 
            for (iy=0; iy<ny; iy++) {
              sum = nsum = 0;
              for (jx=ix-dix; jx<=ix+dix; jx++) {
                if (jx<0 || jx>=nx)
                  continue;
                for (jy=iy-1; jy<=iy+1; jy++) {
                  if (jy<0 || jy>=ny)
                    continue;
                  sum += histogram[iz][jx*ny+jy];
                  nsum++;
                }
              }
              if (nsum)
                new_hist[ix*ny+iy] = sum/nsum;
              else
                new_hist[ix*ny+iy] = histogram[iz][ix*ny+iy];
            }
          }
          tmp = histogram[iz];
          histogram[iz] = new_hist;
          new_hist = tmp;
        }
      }
      free(new_hist);
    }
    if (do_normalize) {
      double max_count = 0;
      for (iz=0; iz<nz; iz++) {
        max_count = 0;
        if (do_normalize==1) 
          /* peak normalization */
          for (ix=0; ix<nx; ix++) {
            for (iy=0; iy<ny; iy++) {
              if (histogram[iz][ix*ny+iy]>max_count)
                max_count = histogram[iz][ix*ny+iy];
            }
          }
        else
          /* sum normalization */
          for (ix=0; ix<nx; ix++) {
            for (iy=0; iy<ny; iy++) {
              max_count += histogram[iz][ix*ny+iy];
            }
          }
        if (!max_count)
          SDDS_Bomb("can't normalize histogram--no points histogrammed");
        for (ix=0; ix<nx; ix++) {
          for (iy=0; iy<ny; iy++) {
            histogram[iz][ix*ny+iy] /= max_count;
          }
        }
      }
    }
    for (iz=0; iz<nz; iz++) {
      if (!SDDS_StartPage(&SDDS_out, nx*ny) ||
          !set_sddscontour_parameters(&SDDS_out, xname, nx, xmin, dx) ||
          !set_sddscontour_parameters(&SDDS_out, yname, ny, ymin, dy) ||
          !SDDS_SetColumn(&SDDS_out, SDDS_SET_BY_NAME, histogram[iz], nx*ny, output_name))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
      if (includeXY) {
        long ix, iy, row;
        for (ix=row=0; ix<nx; ix++) 
          for (iy=0; iy<ny; iy++) {
            if (!SDDS_SetRowValues(&SDDS_out, SDDS_SET_BY_INDEX|SDDS_PASS_BY_VALUE, row,
                                   1, xmin+ix*dx,
                                   2, ymin+iy*dy,
                                   -1)) {
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
            }
            row++;
          }
      }
      if (zname) {
        if (iz==0)
          z_center = zmin + dz/2.0;
        else
          z_center += dz;
        if (!set_sddscontour_parameters(&SDDS_out, zname, nz, zmin, dz) ||
            !SDDS_SetParameters(&SDDS_out, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, "zCenter", z_center, NULL))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
      }
      if (copyParameters)
        paramContext = setParameters(paramContext, &SDDS_out);
      if (!SDDS_WritePage(&SDDS_out))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
    }
  }
  if (!SDDS_Terminate(&SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
  for (iz=0; iz<nz; iz++) {
    free(histogram[iz]);
    free(weightSum[iz]);
  }
  if (xdata) free(xdata);
  if (ydata) free(ydata);
  if (zdata) free(zdata);
  free(rows);
  if (weight) free(weight);
  if (output_units) free(output_units);
  if (doSpread) {
    free(spreadValuePerPage[0]);
    free(spreadValuePerPage[1]);
  }
  free(histogram);
  free(weightSum);
  free_scanargs(&scanned,argc);
  return(0);
}

long define_sddscontour_parameters(SDDS_TABLE *output, SDDS_TABLE *input, char *varname, char *rootname)
{
  char name[SDDS_MAXLINE];
  int32_t double_type = SDDS_DOUBLE;
  if (!rootname) {
    rootname = "Page";
    if (SDDS_DefineParameter(output, varname, NULL, NULL, NULL, NULL, SDDS_STRING, rootname)<0) 
      return 0;
    sprintf(name, "%sDimension", rootname);
    if (SDDS_DefineParameter(output, name, NULL, NULL, NULL, NULL, SDDS_LONG, 0)<0)
      return 0;
    sprintf(name, "%sInterval", rootname);
    if (SDDS_DefineParameter(output, name, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)<0)
      return 0;
    sprintf(name, "%sMinimum", rootname);
    if (SDDS_DefineParameter(output, name, NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)<0)
      return 0;
    return 1;
  }
  if (SDDS_DefineParameter(output, varname, NULL, NULL, NULL, NULL, SDDS_STRING, rootname)<0) 
    return 0;
  sprintf(name, "%sDimension", rootname);
  if (SDDS_DefineParameter(output, name, NULL, NULL, NULL, NULL, SDDS_LONG, 0)<0)
    return 0;
  sprintf(name, "%sInterval", rootname);
  if (SDDS_DefineParameterLikeColumn(output, input, rootname, name)<0)
    return 0;
  if(!SDDS_ChangeParameterInformation(output,"type",&double_type,0,name) )
    return 0;
  sprintf(name, "%sMinimum", rootname);
  if (SDDS_DefineParameterLikeColumn(output, input, rootname, name)<0)
    return 0;
  if(!SDDS_ChangeParameterInformation(output,"type",&double_type,0,name) )
      return 0;
  return 1;
}

long set_sddscontour_parameters(SDDS_TABLE *output, char *rootname, long dimen, double min, double delta)
{
  char name[SDDS_MAXLINE];
  if (!rootname)
    rootname = "Page";
  sprintf(name, "%sDimension", rootname);
  if (!SDDS_SetParameters(output, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, name, dimen, NULL))
    return 0;
  sprintf(name, "%sMinimum", rootname);
  if (!SDDS_SetParameters(output, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, name, min, NULL))
    return 0;
  sprintf(name, "%sInterval", rootname);
  if (!SDDS_SetParameters(output, SDDS_SET_BY_NAME|SDDS_PASS_BY_VALUE, name, delta, NULL))
    return 0;
  return 1;
}

long find2d_min_max(double *min, double *max, double **data, long *rows, long sets)
{
  long i;
  double min1, max1;
  *min = DBL_MAX;
  *max = -DBL_MAX;
  for (i=0; i<sets; i++) {
    find_min_max(&min1, &max1, data[i], rows[i]);
    if (*min>min1)
      *min = min1;
    if (*max<max1)
      *max = max1;
  }
  return 0;
}

static int32_t nParameters = -1;
static char **parameterName = NULL;

PARAM_STORE *saveParameters(PARAM_STORE *context, SDDS_DATASET *SDDSin)
{
  long i;
  
  if (nParameters==-1 && !(parameterName = SDDS_GetParameterNames(SDDSin, &nParameters)))
    SDDS_Bomb("problem getting parameter names");
  if (nParameters==0)
    return NULL;

  if (context->filled==1) {
    context->next = tmalloc(sizeof(PARAM_STORE));
    context->next->filled = 0;
    context->next->next = NULL;
    context->next->param = NULL;
    context = context->next;
  }
  
  context->param = tmalloc(sizeof(*(context->param))*nParameters);

  for (i=0; i<nParameters; i++)
    if (!(context->param[i] = SDDS_GetParameterByIndex(SDDSin, i, NULL)))
      SDDS_Bomb("Error storing parameters");
  
  context->filled = 1;

  return context;
}


PARAM_STORE *setParameters(PARAM_STORE *context, SDDS_DATASET *SDDSout)
{
  long i;
  
  if (nParameters==0)
    return NULL;

  for (i=0; i<nParameters; i++)
    if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME|SDDS_PASS_BY_REFERENCE,
                           parameterName[i], context->param[i], NULL))
      SDDS_Bomb("Error setting parameters");
  
  context = context->next;
  return context;
}

