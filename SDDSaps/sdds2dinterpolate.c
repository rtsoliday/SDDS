/**
 * @file sdds2dinterpolate.c
 * @brief Interpolates scalar 2D data at specified points using various algorithms like Natural Neighbours or Cubic Spline Approximation.
 *
 * @details
 * This program reads 2D scalar data from an SDDS file, interpolates at specified output points, 
 * and writes the interpolated data to an output SDDS file. It supports multiple interpolation algorithms 
 * and various options to control the interpolation process, scaling, output grid, and more.
 *
 * @section Usage
 * ```
 * sdds2dinterpolate <input> <output>
 *                   [-pipe=[input][,output]]
 *                   [-independentColumn=xcolumn=<string>,ycolumn=<string>[,errorColumn=<string>]]
 *                   [-dependentColumn=<list of z column names separated by commas>]
 *                   [-scale=circle|square]
 *                   [-outDimension=xdimension=<nx>,ydimension=<ny>]
 *                   [-range=xminimum=<value>,xmaximum=<value>,yminimum=<value>,ymaximum=<value>]
 *                   [-zoom=<value>]
 *                   [-dimensionThin=xdimension=<nx>,ydimension=<ny>]
 *                   [-clusterThin=<value>]
 *                   [-preprocess]
 *                   [-algorithm=linear|sibson|nonsibson[,average=<value>][,sensitivity=<value>]]
 *                   [-weight=<value>]
 *                   [-vertex=<id>]
 *                   [-npoints=<integer>]
 *                   [-verbose]
 *                   [-merge]
 *                   [-file=<output points file>[,<xName>,<yName>]]
 *                   [-majorOrder=row|column]
 * ```
 *
 * @section Options
 * | Optional                              | Description                                                           |
 * |---------------------------------------|-----------------------------------------------------------------------|
 * | `-pipe`                               | Specifies input/output pipes for data streaming.                      |
 * | `-independentColumn`                  | Specifies x and y independent columns with an optional error column.  |
 * | `-dependentColumn`                    | Specifies dependent z columns for interpolation.                      |
 * | `-scale`                              | Selects scaling mode (`circle` or `square`).                          |
 * | `-outDimension`                       | Sets dimensions of the output grid.                                   |
 * | `-range`                              | Sets range for x and y values in the output grid.                     |
 * | `-zoom`                               | Scales the output grid by a zoom factor.                              |
 * | `-dimensionThin`                      | Reduces input data using average within specified dimensions.         |
 * | `-clusterThin`                        | Reduces input data by clustering points within a maximum distance.    |
 * | `-preprocess`                         | Outputs preprocessed data without interpolation.                      |
 * | `-algorithm`                          | Selects interpolation algorithm with optional parameters.             |
 * | `-weight`                             | Sets minimum interpolation weight.                                    |
 * | `-vertex`                             | Outputs detailed information for a specified vertex.                  |
 * | `-npoints`                            | Specifies the number of output points.                                |
 * | `-verbose`                            | Enables verbose logging for debugging.                                |
 * | `-merge`                              | Merges all input pages before processing.                             |
 * | `-file`                               | Specifies an output file for grid points with optional column names.  |
 * | `-majorOrder`                         | Sets the major order for output data (row or column).                 |
 *
 * @subsection Incompatibilities
 * - `-preprocess` is incompatible with:
 *   - `-algorithm`
 *   - `-merge`
 * - Only one of the following options can be specified:
 *   - `-zoom`
 *   - `-range`
 * - `-algorithm=sibson` requires:
 *   - `-weight`
 *   - `-npoints`
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
 * Hairong Shang, R. Soliday, L. Emery, M. Borland
 */

#include "SDDS.h"
#include "mdb.h"
#include "scan.h"
#include "nn_2d_interpolate.h"
#include "csa.h"

/* Enumeration for option types */
enum option_type {
  CLO_PIPE,
  CLO_INDEPENDENT_COLUMN,
  CLO_DEPENDENT_COLUMN,
  CLO_SCALE,
  CLO_OUT_DIMENSION,
  CLO_RANGE,
  CLO_ZOOM,
  CLO_DIMENSION_THIN,
  CLO_CLUSTER_THIN,
  CLO_PREPROCESS,
  CLO_ALGORITHM,
  CLO_WEIGHT,
  CLO_VERTEX,
  CLO_NPOINTS,
  CLO_VERBOSE,
  CLO_MERGE,
  CLO_FILE,
  CLO_MAJOR_ORDER,
  N_OPTIONS
};

char *option[N_OPTIONS] = {
  "pipe", "independentColumn", "dependentColumn", "scale",
  "outDimension", "range", "zoom", "dimensionThin", "clusterThin",
  "preprocess", "algorithm", "weight", "vertex", "npoints", "verbose", "merge", "file", "majorOrder"};

static const char *USAGE =
  "Usage: sdds2dinterpolate [<input>] [<output>]\n"
  "                         [-pipe=[input][,output]]\n"
  "                         [-independentColumn=xcolumn=<string>,ycolumn=<string>[,errorColumn=<string>]]\n"
  "                         [-dependentColumn=<list of z column names separated by commas>]\n"
  "                         [-scale=circle|square]\n"
  "                         [-outDimension=xdimension=<nx>,ydimension=<ny>]\n"
  "                         [-range=xminimum=<value>,xmaximum=<value>,yminimum=<value>,ymaximum=<value>]\n"
  "                         [-zoom=<value>]\n"
  "                         [-dimensionThin=xdimension=<nx>,ydimension=<ny>]\n"
  "                         [-clusterThin=<value>]\n"
  "                         [-preprocess]\n"
  "                         [-algorithm=linear|sibson|nonsibson[,average=<value>][,sensitivity=<value>]]\n"
  "                         [-weight=<value>]\n"
  "                         [-vertex=<id>]\n"
  "                         [-npoints=<integer>]\n"
  "                         [-verbose]\n"
  "                         [-merge]\n"
  "                         [-file=<output points file>[,<xName>,<yName>]]\n"
  "                         [-majorOrder=row|column]\n"
  "Detailed option descriptions are as follows:\n"
  "  -independentColumn: Specifies the independent columns for X, Y, and optional error.\n"
  "  -dependentColumn: Specifies the dependent Z columns to interpolate.\n"
  "  -scale: Choose between 'circle' or 'square' scaling methods.\n"
  "  -outDimension: Define the output grid dimensions in X and Y.\n"
  "  -range: Set the minimum and maximum values for X and Y in the output grid.\n"
  "  -zoom: Zoom in or out on the output grid.\n"
  "  -dimensionThin: Thin input data by averaging within specified grid dimensions.\n"
  "  -clusterThin: Thin input data by clustering points based on a maximum distance.\n"
  "  -preprocess: Output data without performing interpolation.\n"
  "  -algorithm: Select the interpolation algorithm and its parameters.\n"
  "  -weight: Set the minimal allowed weight for a vertex.\n"
  "  -vertex: Enable verbose output for a specific vertex.\n"
  "  -npoints: Define the number of output points.\n"
  "  -verbose: Enable verbose output during processing.\n"
  "  -merge: Merge data from all input pages before interpolation.\n"
  "  -file: Specify an SDDS file with output points.\n"
  "  -majorOrder: Set the output file's data order to 'row' or 'column'.\n"
  "Program by Hairong Shang. " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION "\n";

/* Struct for output points */
typedef struct
{
  double xmin, xmax, ymin, ymax, dx, dy;
  int nx, ny, nout;
  point *pout;
} OUT_POINT;

#define ALGORITHMS 2
#define LINEAR_NN 0x0001UL
#define SIBSON_NN 0x0002UL
#define NONSIBSON_NN 0x0004UL

char *algorithm_option[ALGORITHMS] = {"nn", "csa"};

#define SCALE_OPTIONS 2
char *scale_option[SCALE_OPTIONS] = {"circle", "square"};

/* Function prototypes */
void ReadInputFile(char *inputFile, char *xCol, char *yCol, char **zColMatch, long zColMatches, char ***zCol, int32_t *zCols,
                   char *stdCol, long *pages, long **rows, double ***x, double ***y, double ****z, double ***std, SDDS_DATASET *SDDS_in);
void SetupOutputFile(SDDS_DATASET *SDDS_out, SDDS_DATASET *SDDS_in, char *outputFile, char *xCol, char *yCol,
                     char *xName, char *yName, char **zCol, long zCols, short columnMajorOrder);
void WriteOutputPage(SDDS_DATASET *SDDS_out, char *xCol, char *yCol, char *zCol, specs *spec, int nout, point *pout, long writepage);
void ReadPointFile(char *pointsFile, char *xCol, char *yCol, long *point_pages, OUT_POINT **out_point);
int interpolate_output_points(int nin, point **pin, double *std, char *xCol, char *yCol, char *xName, char *yName,
                              char **zCol, long zCols, specs *spec, long pages, OUT_POINT *out_point, SDDS_DATASET *SDDS_out);

static char *INFINITY_OPTION[1] = {"infinity"};

int main(int argc, char *argv[]) {
  SDDS_DATASET SDDS_out, SDDS_in;
  char *inputFile, *outputFile, *xCol = "x", *yCol = "y", **zCol = NULL, *stdCol = "StdError", *pointsFile = NULL, **zColMatch = NULL;
  char *pointsFileXName = NULL, *pointsFileYName = NULL;
  long i, j, i_arg, k, scale, *rows, pages, merge = 0, point_pages = 0, writepage = 0, zColMatches = 0;
  int32_t zCols = 1;
  unsigned long dummyFlags = 0, pipeFlags = 0, majorOrderFlag;
  SCANNED_ARG *s_arg;
  double **x = NULL, **y = NULL, ***z = NULL, xmin = 0, xmax = 0, ymin = 0, ymax = 0, **std = NULL, *std_all = NULL;
  specs *spec = specs_create();
  int nin = 0;
  point **pin = NULL;
  int nout = 0;
  point *pout = NULL;
  OUT_POINT *out_point = NULL;
  short columnMajorOrder = -1;

  /* Turn off the extrapolation by default */
  spec->wmin = 0;
  inputFile = outputFile = NULL;
  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s", USAGE);
    return EXIT_FAILURE;
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_MAJOR_ORDER:
        majorOrderFlag = 0;
        s_arg[i_arg].n_items -= 1;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&majorOrderFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "row", -1, NULL, 0, SDDS_ROW_MAJOR_ORDER,
                           "column", -1, NULL, 0, SDDS_COLUMN_MAJOR_ORDER, NULL))) {
          SDDS_Bomb("Invalid -majorOrder syntax/values");
        }
        if (majorOrderFlag & SDDS_COLUMN_MAJOR_ORDER)
          columnMajorOrder = 1;
        else if (majorOrderFlag & SDDS_ROW_MAJOR_ORDER)
          columnMajorOrder = 0;
        break;
      case CLO_FILE:
        if (s_arg[i_arg].n_items != 2 && s_arg[i_arg].n_items != 4)
          SDDS_Bomb("Invalid -file syntax.");
        pointsFile = s_arg[i_arg].list[1];
        if (s_arg[i_arg].n_items == 4) {
          pointsFileXName = s_arg[i_arg].list[2];
          pointsFileYName = s_arg[i_arg].list[3];
        }
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("Invalid -pipe syntax");
        break;
      case CLO_INDEPENDENT_COLUMN:
        if (s_arg[i_arg].n_items < 3)
          SDDS_Bomb("Invalid -independentColumn syntax.");
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "xcolumn", SDDS_STRING, &xCol, 1, 0,
                          "ycolumn", SDDS_STRING, &yCol, 1, 0,
                          "errorcolumn", SDDS_STRING, &stdCol, 1, 0, NULL))
          SDDS_Bomb("Invalid -independentColumn syntax");
        s_arg[i_arg].n_items++;
        break;
      case CLO_DEPENDENT_COLUMN:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -dependentColumn syntax.");
        zColMatches = s_arg[i_arg].n_items - 1;
        zColMatch = tmalloc(sizeof(*zColMatch) * zColMatches);
        for (i = 0; i < zColMatches; i++)
          zColMatch[i] = s_arg[i_arg].list[i + 1];
        break;
      case CLO_SCALE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -scale syntax.");
        if ((scale = match_string(s_arg[i_arg].list[1], scale_option, SCALE_OPTIONS, 0)) == -1) {
          fprintf(stderr, "Invalid scale option - %s provided.\n", s_arg[i_arg].list[1]);
          exit(EXIT_FAILURE);
        }
        spec->square = !scale;
        spec->invariant = scale;
        break;
      case CLO_OUT_DIMENSION:
        if (s_arg[i_arg].n_items != 3)
          SDDS_Bomb("Invalid -outDimension syntax.");
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "xdimension", SDDS_LONG, &spec->nx, 1, 0,
                          "ydimension", SDDS_LONG, &spec->ny, 1, 0, NULL))
          SDDS_Bomb("Invalid -outDimension syntax");
        s_arg[i_arg].n_items++;
        if (spec->nx <= 0 || spec->nx > NMAX || spec->ny <= 0 || spec->ny > NMAX)
          SDDS_Bomb("Invalid size for output grid.");
        spec->generate_points = 1;
        break;
      case CLO_RANGE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -range syntax.");
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "xminimum", SDDS_DOUBLE, &spec->xmin, 1, 0,
                          "xmaximum", SDDS_DOUBLE, &spec->xmax, 1, 0,
                          "yminimum", SDDS_DOUBLE, &spec->ymin, 1, 0,
                          "ymaximum", SDDS_DOUBLE, &spec->ymax, 1, 0, NULL))
          SDDS_Bomb("Invalid -range syntax");
        s_arg[i_arg].n_items++;
        if (spec->xmin > spec->xmax || spec->ymin > spec->ymax ||
            isnan(spec->xmin) || isnan(spec->xmax) || isnan(spec->ymin) || isnan(spec->ymax))
          SDDS_Bomb("Invalid -range provided.");
        spec->range = 1;
        break;
      case CLO_ZOOM:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -zoom syntax.");
        if (!get_double(&spec->zoom, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -zoom value provided.");
        break;
      case CLO_DIMENSION_THIN:
        if (s_arg[i_arg].n_items != 3)
          SDDS_Bomb("Invalid -dimensionThin syntax.");
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "xdimension", SDDS_LONG, &spec->nxd, 1, 0,
                          "ydimension", SDDS_LONG, &spec->nyd, 1, 0, NULL))
          SDDS_Bomb("Invalid -dimensionThin syntax");
        s_arg[i_arg].n_items++;
        spec->thin = 1;
        break;
      case CLO_CLUSTER_THIN:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -clusterThin syntax.");
        if (!get_double(&spec->rmax, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid -clusterThin value provided.");
        spec->thin = 2;
        break;
      case CLO_PREPROCESS:
        spec->nointerp = 1;
        break;
      case CLO_ALGORITHM:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -algorithm syntax!");
        if ((spec->method = match_string(s_arg[i_arg].list[1], algorithm_option, ALGORITHMS, 0)) == -1) {
          fprintf(stderr, "Invalid algorithm - %s provided, has to be nn or csa.\n", s_arg[i_arg].list[1]);
          exit(EXIT_FAILURE);
        }
        dummyFlags = 0;
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "linear", -1, NULL, 0, LINEAR_NN,
                          "sibson", -1, NULL, 0, SIBSON_NN,
                          "nonSibson", -1, NULL, 0, NONSIBSON_NN,
                          "average", SDDS_LONG, &spec->nppc, 1, 0,
                          "sensitivity", SDDS_DOUBLE, &spec->k, 1, 0, NULL))
          SDDS_Bomb("Invalid -algorithm syntax!");
        spec->linear = 0;
        if (!dummyFlags || dummyFlags & LINEAR_NN) {
          spec->linear = 1;
        } else if (dummyFlags & SIBSON_NN) {
          nn_rule = SIBSON;
        } else if (dummyFlags & NONSIBSON_NN) {
          nn_rule = NON_SIBSONIAN;
        }
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_WEIGHT:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -weight syntax.");
        if (match_string(s_arg[i_arg].list[1], INFINITY_OPTION, 1, 0) == 0)
          spec->wmin = -DBL_MAX;
        else {
          if (!get_double(&spec->wmin, s_arg[i_arg].list[1]))
            SDDS_Bomb("Invalid weight value provided.");
        }
        break;
      case CLO_VERTEX:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -vertex syntax.");
        if (!get_int(&nn_test_vertice, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid vertex value provided.");
        nn_verbose = 1;
        break;
      case CLO_NPOINTS:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -npoints syntax.");
        if (!get_int(&spec->npoints, s_arg[i_arg].list[1]))
          SDDS_Bomb("Invalid npoints value provided.");
        break;
      case CLO_VERBOSE:
        nn_verbose = 2;
        break;
      case CLO_MERGE:
        merge = 1;
        break;
      default:
        fprintf(stderr, "Unknown option - %s provided.\n", s_arg[i_arg].list[0]);
        exit(EXIT_FAILURE);
        break;
      }
    } else {
      if (!inputFile)
        inputFile = s_arg[i_arg].list[0];
      else if (!outputFile)
        outputFile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("Too many files given!");
    }
  }

  processFilenames("sdds2dinterpolate", &inputFile, &outputFile, pipeFlags, 0, NULL);
  if (!spec->generate_points && !spec->nointerp && !pointsFile) {
    fprintf(stderr, "No output grid specified.\n");
    exit(EXIT_FAILURE);
  }
  if (spec->thin) {
    if (spec->nxd == -1)
      spec->nxd = spec->nx;
    if (spec->nyd == -1)
      spec->nyd = spec->ny;
    if (spec->nxd <= 0 || spec->nyd <= 0) {
      fprintf(stderr, "Invalid grid size for thinning.\n");
      exit(EXIT_FAILURE);
    }
  }
  if (spec->npoints == 1) {
    if (spec->nx <= 0)
      spec->npoints = 0;
    else
      spec->npoints = spec->nx * spec->ny;
  }
  ReadInputFile(inputFile, xCol, yCol, zColMatch, zColMatches, &zCol, &zCols, stdCol, &pages, &rows, &x, &y, &z, &std, &SDDS_in);
  if (!spec->nointerp)
    SetupOutputFile(&SDDS_out, &SDDS_in, outputFile, xCol, yCol, pointsFileXName, pointsFileYName,
                    zCol, zCols, columnMajorOrder);
  if (!SDDS_Terminate(&SDDS_in))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (pointsFile)
    ReadPointFile(pointsFile, pointsFileXName ? pointsFileXName : xCol, pointsFileYName ? pointsFileYName : yCol,
                  &point_pages, &out_point);
  pin = tmalloc(sizeof(*pin) * zCols);
  if (merge) {
    for (i = 0; i < pages; i++) {
      if (!i) {
        xmin = xmax = x[0][0];
        ymin = ymax = y[0][0];
      }
      for (k = 0; k < zCols; k++)
        pin[k] = SDDS_Realloc(pin[k], sizeof(**pin) * (nin + rows[i]));
      if (std)
        std_all = SDDS_Realloc(std_all, sizeof(*std_all) * (nin + rows[i]));
      for (j = 0; j < rows[i]; j++) {
        if (x[i][j] < xmin)
          xmin = x[i][j];
        else if (x[i][j] > xmax)
          xmax = x[i][j];
        if (y[i][j] < ymin)
          ymin = y[i][j];
        else if (y[i][j] > ymax)
          ymax = y[i][j];
        for (k = 0; k < zCols; k++) {
          pin[k][nin + j].x = x[i][j];
          pin[k][nin + j].y = y[i][j];
          pin[k][nin + j].z = z[k][i][j];
        }
        if (std)
          std_all[nin + j] = std[i][j];
      }
      nin += rows[i];
      free(x[i]);
      free(y[i]);
      for (k = 0; k < zCols; k++) {
        free(z[k][i]);
        z[k][i] = NULL;
      }
      if (std && std[i]) {
        free(std[i]);
        std[i] = NULL;
      }
      x[i] = y[i] = NULL;
    }
    free(x);
    free(y);
    x = y = NULL;
    for (k = 0; k < zCols; k++) {
      free(z[k]);
      z[k] = NULL;
    }
    free(z);
    z = NULL;
    if (std)
      free(std);
    std = NULL;
    if (!spec->range) {
      spec->xmin = xmin;
      spec->ymin = ymin;
      spec->xmax = xmax;
      spec->ymax = ymax;
    }
    if (pointsFile) {
      if (!interpolate_output_points(nin, pin, std_all, xCol, yCol, pointsFileXName, pointsFileYName,
                                     zCol, zCols, spec, pages, out_point, &SDDS_out))
        exit(EXIT_FAILURE);
      writepage = 1;
    } else {
      for (k = 0; k < zCols; k++) {
        if (spec->method == NN) {
          do_nn_2d_interpolate(spec, &nin, &pin[k], &nout, &pout);
        } else if (spec->method == CSA) {
          do_csa_2d_interpolate(spec, nin, pin[k], &nout, &pout, std_all);
        }
        if (!spec->nointerp) {
          if (!writepage) {
            WriteOutputPage(&SDDS_out, pointsFileXName ? pointsFileXName : xCol,
                            pointsFileYName ? pointsFileYName : yCol,
                            zCol[k], spec, nout, pout, 0);
            writepage = 1;
          } else {
            for (i = 0; i < nout; i++) {
              if (!SDDS_SetRowValues(&SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                                     zCol[k], pout[i].z, NULL))
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            }
          }
        }
      }
      if (writepage && !SDDS_WritePage(&SDDS_out))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  } else {
    for (i = 0; i < pages; i++) {
      xmin = xmax = x[i][0];
      ymin = ymax = y[i][0];
      nin = rows[i];
      for (j = 0; j < nin; j++) {
        if (x[i][j] < xmin)
          xmin = x[i][j];
        else if (x[i][j] > xmax)
          xmax = x[i][j];
        if (y[i][j] < ymin)
          ymin = y[i][j];
        else if (y[i][j] > ymax)
          ymax = y[i][j];
      }
      if (!spec->range) {
        spec->xmin = xmin;
        spec->ymin = ymin;
        spec->xmax = xmax;
        spec->ymax = ymax;
      }
      for (k = 0; k < zCols; k++) {
        pin[k] = malloc(sizeof(**pin) * nin);
        for (j = 0; j < rows[i]; j++) {
          pin[k][j].x = x[i][j];
          pin[k][j].y = y[i][j];
          pin[k][j].z = z[k][i][j];
        }
        free(z[k][i]);
      }
      free(x[i]);
      free(y[i]);
      x[i] = y[i] = NULL;
      if (std && std[i])
        std_all = std[i];
      else
        std_all = NULL;
      if (pointsFile) {
        if (!interpolate_output_points(nin, pin, std_all, xCol, yCol, pointsFileXName, pointsFileYName,
                                       zCol, zCols, spec, point_pages, out_point, &SDDS_out))
          exit(EXIT_FAILURE);
        writepage = 1;
      } else {
        for (k = 0; k < zCols; k++) {
          if (spec->method == NN) {
            do_nn_2d_interpolate(spec, &nin, &pin[k], &nout, &pout);
          } else if (spec->method == CSA) {
            do_csa_2d_interpolate(spec, nin, pin[k], &nout, &pout, std_all);
          }
          if (!spec->nointerp) {
            if (!writepage) {
              WriteOutputPage(&SDDS_out, pointsFileXName ? pointsFileXName : xCol,
                              pointsFileYName ? pointsFileYName : yCol,
                              zCol[k], spec, nout, pout, 0);
              writepage = 1;
            } else {
              for (j = 0; j < nout; j++) {
                if (!SDDS_SetRowValues(&SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, j,
                                       zCol[k], pout[j].z, NULL))
                  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              }
            }
          }
        }
        if (std && std[i]) {
          free(std[i]);
          std[i] = NULL;
        }
        if (writepage && !SDDS_WritePage(&SDDS_out))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    free(x);
    free(y);
    for (k = 0; k < zCols; k++)
      free(z[k]);
    free(z);
    x = y = NULL;
  }
  if (std)
    free(std);
  for (k = 0; k < zCols; k++)
    free(pin[k]);
  free(pin);
  if (writepage && !SDDS_Terminate(&SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (point_pages && out_point) {
    for (i = 0; i < point_pages; i++) {
      free(out_point[i].pout);
    }
    free(out_point);
  }
  if (pout)
    free(pout);
  free(spec);
  free(rows);
  for (i = 0; i < zCols; i++)
    free(zCol[i]);
  free(zCol);
  free_scanargs(&s_arg, argc);
  return EXIT_SUCCESS;
}

void ReadInputFile(char *inputFile, char *xCol, char *yCol, char **zColMatch, long zColMatches, char ***zCol, int32_t *zCols,
                   char *stdCol, long *pages, long **rows, double ***x, double ***y, double ****z, double ***std, SDDS_DATASET *SDDS_in) {
  int64_t rows1 = 0;
  long i, pages0 = 0, std_exist = 0;

  if (!SDDS_InitializeInput(SDDS_in, inputFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!zColMatches) {
    *zCol = tmalloc(sizeof(**zCol));
    SDDS_CopyString(zCol[0], "z");
    *zCols = 1;
  } else {
    *zCol = getMatchingSDDSNames(SDDS_in, zColMatch, zColMatches, zCols, SDDS_MATCH_COLUMN);
    free(zColMatch);
    if (!(*zCols)) {
      fprintf(stderr, "No dependent columns found in input file.\n");
      exit(EXIT_FAILURE);
    }
  }
  *z = tmalloc(sizeof(**z) * (*zCols));
  *rows = NULL;
  if (SDDS_CheckColumn(SDDS_in, xCol, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) {
    fprintf(stderr, "X column - %s does not exist!\n", xCol);
    exit(EXIT_FAILURE);
  }
  if (SDDS_CheckColumn(SDDS_in, yCol, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) {
    fprintf(stderr, "Y column - %s does not exist!\n", yCol);
    exit(EXIT_FAILURE);
  }
  for (i = 0; i < *zCols; i++) {
    if (SDDS_CheckColumn(SDDS_in, (*zCol)[i], NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) {
      fprintf(stderr, "Z column - %s does not exist!\n", (*zCol)[i]);
      exit(EXIT_FAILURE);
    }
  }
  if (SDDS_CheckColumn(SDDS_in, stdCol, NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK)
    std_exist = 1;

  while (SDDS_ReadPage(SDDS_in) > 0) {
    rows1 = SDDS_CountRowsOfInterest(SDDS_in);
    if (!rows1)
      continue;
    if (rows1 > INT32_MAX) {
      fprintf(stderr, "Too many input rows for 2D interpolate library routines.\n");
      exit(EXIT_FAILURE);
    }
    *rows = SDDS_Realloc(*rows, sizeof(**rows) * (pages0 + 1));
    (*rows)[pages0] = rows1;
    if (!(*x = SDDS_Realloc(*x, sizeof(**x) * (pages0 + 1))) ||
        !(*y = SDDS_Realloc(*y, sizeof(**y) * (pages0 + 1)))) {
      fprintf(stderr, "Memory allocation error for X and Y data.\n");
      exit(EXIT_FAILURE);
    }
    for (i = 0; i < *zCols; i++) {
      if (!((*z)[i] = SDDS_Realloc((*z)[i], sizeof(***z) * (pages0 + 1)))) {
        fprintf(stderr, "Memory allocation error for Z data.\n");
        exit(EXIT_FAILURE);
      }
    }
    if (std_exist) {
      if (!(*std = SDDS_Realloc(*std, sizeof(**std) * (pages0 + 1)))) {
        fprintf(stderr, "Memory allocation error for standard error.\n");
        exit(EXIT_FAILURE);
      }
      (*std)[pages0] = NULL;
    }

    (*x)[pages0] = (*y)[pages0] = NULL;
    if (!((*x)[pages0] = (double *)SDDS_GetColumnInDoubles(SDDS_in, xCol)) ||
        !((*y)[pages0] = (double *)SDDS_GetColumnInDoubles(SDDS_in, yCol)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < *zCols; i++) {
      (*z)[i][pages0] = NULL;
      if (!((*z)[i][pages0] = (double *)SDDS_GetColumnInDoubles(SDDS_in, (*zCol)[i])))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (std_exist && !((*std)[pages0] = (double *)SDDS_GetColumnInDoubles(SDDS_in, stdCol)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    pages0++;
  }

  if (!pages0) {
    fprintf(stderr, "No data found in the input file.\n");
    exit(EXIT_FAILURE);
  }
  *pages = pages0;
  return;
}

void SetupOutputFile(SDDS_DATASET *SDDS_out, SDDS_DATASET *SDDS_in, char *outputFile, char *xCol, char *yCol,
                     char *xName, char *yName, char **zCol, long zCols, short columnMajorOrder) {
  char tmpstr[256], *xUnits = NULL, *yUnits = NULL;
  long i;

  if (!SDDS_InitializeOutput(SDDS_out, SDDS_BINARY, 1, NULL, NULL, outputFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_DefineSimpleParameter(SDDS_out, "Variable1Name", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDS_out, "Variable2Name", NULL, SDDS_STRING) ||
      !SDDS_TransferColumnDefinition(SDDS_out, SDDS_in, xCol, xName) ||
      !SDDS_TransferColumnDefinition(SDDS_out, SDDS_in, yCol, yName))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnMajorOrder != -1)
    SDDS_out->layout.data_mode.column_major = columnMajorOrder;
  else
    SDDS_out->layout.data_mode.column_major = SDDS_in->layout.data_mode.column_major;
  for (i = 0; i < zCols; i++) {
    if (!SDDS_TransferColumnDefinition(SDDS_out, SDDS_in, zCol[i], NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (SDDS_GetColumnInformation(SDDS_in, "units", &xUnits, SDDS_GET_BY_NAME, xCol) != SDDS_STRING ||
      SDDS_GetColumnInformation(SDDS_in, "units", &yUnits, SDDS_GET_BY_NAME, yCol) != SDDS_STRING)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sDimension", xName ? xName : xCol);
  if (!SDDS_DefineSimpleParameter(SDDS_out, tmpstr, NULL, SDDS_LONG))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sDimension", yName ? yName : yCol);
  if (!SDDS_DefineSimpleParameter(SDDS_out, tmpstr, NULL, SDDS_LONG))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sInterval", xName ? xName : xCol);
  if (!SDDS_DefineSimpleParameter(SDDS_out, tmpstr, xUnits, SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sInterval", yName ? yName : yCol);
  if (!SDDS_DefineSimpleParameter(SDDS_out, tmpstr, yUnits, SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sMinimum", xName ? xName : xCol);
  if (!SDDS_DefineSimpleParameter(SDDS_out, tmpstr, xUnits, SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sMinimum", yName ? yName : yCol);
  if (!SDDS_DefineSimpleParameter(SDDS_out, tmpstr, yUnits, SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sMaximum", xName ? xName : xCol);
  if (!SDDS_DefineSimpleParameter(SDDS_out, tmpstr, xUnits, SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sMaximum", yName ? yName : yCol);
  if (!SDDS_DefineSimpleParameter(SDDS_out, tmpstr, yUnits, SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (xUnits)
    free(xUnits);
  if (yUnits)
    free(yUnits);
}

void WriteOutputPage(SDDS_DATASET *SDDS_out, char *xCol, char *yCol, char *zCol, specs *spec, int nout, point *pout, long writepage) {
  char tmpstr[256];
  int i;

  if (!SDDS_StartPage(SDDS_out, nout))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_SetParameters(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "Variable1Name", xCol,
                          "Variable2Name", yCol, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sDimension", xCol);
  if (!SDDS_SetParameters(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpstr, spec->nx, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sDimension", yCol);
  if (!SDDS_SetParameters(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpstr, spec->ny, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sInterval", xCol);
  if (!SDDS_SetParameters(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpstr, spec->dx, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sInterval", yCol);
  if (!SDDS_SetParameters(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpstr, spec->dy, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sMinimum", xCol);
  if (!SDDS_SetParameters(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpstr, spec->xmin, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sMinimum", yCol);
  if (!SDDS_SetParameters(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpstr, spec->ymin, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sMaximum", xCol);
  if (!SDDS_SetParameters(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpstr, spec->xmax, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  sprintf(tmpstr, "%sMaximum", yCol);
  if (!SDDS_SetParameters(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpstr, spec->ymax, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < nout; i++) {
    if (!SDDS_SetRowValues(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                           xCol, pout[i].x,
                           yCol, pout[i].y,
                           zCol, pout[i].z, NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (writepage && !SDDS_WritePage(SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void ReadPointFile(char *inputFile, char *xCol, char *yCol, long *pages, OUT_POINT **out_point) {
  SDDS_DATASET SDDS_in;
  int64_t rows1 = 0;
  long pages0 = 0;
  double *x, *y, xmin, xmax, firstx, firsty, ymin, ymax;
  int nx, ny, i;

  if (!SDDS_InitializeInput(&SDDS_in, inputFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_CheckColumn(&SDDS_in, xCol, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) {
    fprintf(stderr, "X column - %s does not exist!\n", xCol);
    exit(EXIT_FAILURE);
  }
  if (SDDS_CheckColumn(&SDDS_in, yCol, NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OK) {
    fprintf(stderr, "Y column - %s does not exist!\n", yCol);
    exit(EXIT_FAILURE);
  }
  while (SDDS_ReadPage(&SDDS_in) > 0) {
    rows1 = SDDS_CountRowsOfInterest(&SDDS_in);
    if (!rows1)
      continue;
    if (rows1 > INT32_MAX) {
      fprintf(stderr, "Too many input rows for 2D interpolate library routines.\n");
      exit(EXIT_FAILURE);
    }
    *out_point = SDDS_Realloc(*out_point, sizeof(**out_point) * (pages0 + 1));
    x = y = NULL;
    (*out_point)[pages0].pout = malloc(sizeof(point) * rows1);
    (*out_point)[pages0].nout = rows1;
    if (!(x = (double *)SDDS_GetColumnInDoubles(&SDDS_in, xCol)) ||
        !(y = (double *)SDDS_GetColumnInDoubles(&SDDS_in, yCol)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    xmin = xmax = firstx = x[0];
    ymin = ymax = firsty = y[0];
    nx = ny = 0;

    for (i = 0; i < rows1; i++) {
      (*out_point)[pages0].pout[i].x = x[i];
      (*out_point)[pages0].pout[i].y = y[i];
      if (x[i] < xmin)
        xmin = x[i];
      else if (x[i] > xmax)
        xmax = x[i];
      if (x[i] == firstx)
        ny++;
      if (y[i] < ymin)
        ymin = y[i];
      else if (y[i] > ymax)
        ymax = y[i];
      if (y[i] == firsty)
        nx++;
    }
    free(x);
    free(y);
    (*out_point)[pages0].xmin = xmin;
    (*out_point)[pages0].xmax = xmax;
    (*out_point)[pages0].ymin = ymin;
    (*out_point)[pages0].ymax = ymax;
    (*out_point)[pages0].nx = nx;
    (*out_point)[pages0].ny = ny;
    if (nx > 1)
      (*out_point)[pages0].dx = (xmax - xmin) / (nx - 1);
    else
      (*out_point)[pages0].dx = 0;
    if (ny > 1)
      (*out_point)[pages0].dy = (ymax - ymin) / (ny - 1);
    else
      (*out_point)[pages0].dy = 0;
    pages0++;
  }
  if (!SDDS_Terminate(&SDDS_in))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!pages0) {
    fprintf(stderr, "No data found in the points file.\n");
    exit(EXIT_FAILURE);
  }
  *pages = pages0;
  return;
}

int interpolate_output_points(int nin, point **pin, double *std, char *xCol, char *yCol, char *xName, char *yName,
                              char **zCol, long zCols, specs *spec, long pages, OUT_POINT *out_point, SDDS_DATASET *SDDS_out) {
  int nout, page, k, writepage = 0, i;
  point *pout = NULL;

  for (page = 0; page < pages; page++) {
    nout = out_point[page].nout;
    pout = out_point[page].pout;
    spec->nx = out_point[page].nx;
    spec->ny = out_point[page].ny;
    spec->xmin = out_point[page].xmin;
    spec->xmax = out_point[page].xmax;
    spec->dx = out_point[page].dx;
    spec->ymin = out_point[page].ymin;
    spec->ymax = out_point[page].ymax;
    spec->dy = out_point[page].dy;
    for (k = 0; k < zCols; k++) {
      if (spec->method == NN) {
        do_nn_2d_interpolate(spec, &nin, &pin[k], &nout, &pout);
      } else {
        do_csa_2d_interpolate(spec, nin, pin[k], &nout, &pout, std);
      }
      if (!spec->nointerp) {
        if (!writepage) {
          WriteOutputPage(SDDS_out, xName ? xName : xCol, yName ? yName : yCol, zCol[k], spec, nout, pout, 0);
          writepage = 1;
        } else {
          for (i = 0; i < nout; i++) {
            if (!SDDS_SetRowValues(SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, i,
                                   zCol[k], pout[i].z, NULL))
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          }
        }
      }
    }
  }
  if (writepage && !SDDS_WritePage(SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 1;
}
