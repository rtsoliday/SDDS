/**************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution.
\**************************************************************************/

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "graph.h"
#include "matlib.h"
#include "contour.h"
#include "rpn.h"
#include "fftpackC.h"
#include "graphics.h"
#if defined(_WIN32)
#  include <windows.h>
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#define SET_QUANTITY 0
#define SET_SWAP_XY 1
#define SET_SHADE 2
#define SET_CONTOURS 3
#define SET_EQUATION 4
#define SET_SCALES 5
#define SET_LABEL_CONTOURS 6
#define SET_DEVICE 7
#define SET_OUTPUT 8
#define SET_INTERPOLATE 9
#define SET_FILTER 10
#define SET_SHAPES 11
#define SET_EQUAL_ASPECT 12
#define SET_XLABEL 13
#define SET_YLABEL 14
#define SET_TITLE 15
#define SET_TOPLINE 16
#define SET_TOPTITLE 17
#define SET_NO_LABELS 18
#define SET_NO_BORDER 19
#define SET_NO_SCALES 20
#define SET_DATE_STAMP 21
#define SET_VERBOSITY 22
#define SET_RPN_DEFNS_FILES 23
#define SET_RPN_EXPRESSIONS 24
#define SET_RPN_TRANSFORM 25
#define SET_FIXED_RANGE 26
#define SET_COLUMNMATCH 27
#define SET_LOGSCALE 28
#define SET_DELTAS 29
#define SET_YSTRINGS 30
#define SET_EDITYSTRINGS 31
#define SET_PREFERV1V2PARAMS 32
#define SET_MAPSHADE 33
#define SET_LAYOUT 34
#define SET_ARRAY 35
#define SET_SWAPARRAY 36
#define SET_THICKNESS 37
#define SET_TICKSETTINGS 38
#define SET_PIPE 39
#define SET_WATERFALL 40
#define SET_YRANGE 41
#define SET_XRANGE 42
#define SET_NO_COLOR_BAR 43
#define SET_YAXIS 44
#define SET_XAXIS 45
#define SET_XYZ 46
#define SET_DRAWLINE 47
#define SET_LEVELLIST 48
#define SET_SYMBOLS 49
#define SET_FILLSCREEN 50
#define SET_XLOG 51
#define SET_FIXFONTSIZE 52
#define SET_LIMITLEVELS 53
#define SET_CONVERTUNITS 54
#define SET_YFLIP 55
#define SET_SHOWGAPS 56
#define SET_3D 57
#define OPTIONS 58

static char *option[OPTIONS] = {
  "quantity", "swapxy", "shade", "contours", "equation", "scales",
  "labelcontours", "device", "output", "interpolate", "filter", "shapes",
  "equalaspect", "xlabel", "ylabel", "title", "topline", "toptitle",
  "nolabels", "noborder", "noscales", "datestamp", "verbosity",
  "rpndefinitionsfiles", "rpnexpressions", "rpntransform", "fixedrange",
  "columnmatch", "logscale", "deltas", "ystrings", "yeditstrings",
  "v1v2preferred", "mapshade", "layout", "array", "swaparray",
  "thickness", "ticksettings", "pipe", "waterfall", "yrange", "xrange",
  "nocolorbar", "yaxis", "xaxis", "xyz", "drawline", "levellist",
  "symbols", "fillscreen", "xlog", "fixfontsize", "limitlevels",
  "convertunits", "yflip", "showgaps", "3d"};

static long threeD = 0;

char *USAGE = "sddscontour [-pipe] [<SDDSfilename>]\n\
 [{-quantity=<column-name> | -equation=<rpn-equation>[,algebraic] |\n\
  -waterfall=parameter=<parameter>,independentColumn=<xColumn>,colorColumn=<colorColumn>[,scroll=vertical|horizontal] | \n\
  -columnmatch=<indep-column-name>,<expression> [-deltas[={fractional|normalize}]]}]]\n\
 [-array=<z-2d-array>[,<x-1d-array>,<y-id-array>]] [-swaparray]\n\
 [-xyz=<x-column>,<y-column>,<z-column>]\n\
 [-3d]\n\
 [-rpndefinitionsfiles=<filename>[,...]]\n\
 [-rpnexpressions=<setup-expression>[,...][,algebraic]]\n\
 [-rpntransform=<expression>[,algebraic]] [-fixedrange] [-showGaps]\n\
 [[-shade=<number>[,<min>,<max>,gray]] | [-contours=<number>[,<min>,<max>]]] \n\
 [-levelList=<listOfLevels>] [-limitLevels={minimum=<value>,}{maximum=<value>}]\n\
 [-mapShade=<hue0>,<hue1>] \n\
 [-scales=<xl>,<xh>,<yl>,<yh>] [-v1v2Preferred] \n\
 [-labelcontours=interval[,offset]] [-logscale[=<floor>]]\n\
 [-device={qt|motif|png|postscript}[,<device-arguments>]] [-output=<filename>]\n\
 qt device arguments: '-dashes <0|1> -linetype <filename> -movie 1 [-interval <seconds>] -keep <number> -share <name> -timeoutHours <hours> -spectrum'\n\
 motif device arguments: '-dashes 1 -linetype lineDefineFile'\n\
 png device arguments: 'rootname=<name>,template=<string>,onwhite,onblack,dashes,movie'\n\
 [-interpolate=<nx>,<ny>[,{floor|ceiling|antiripple}]] [-filter=<xcutoff>,<ycutoff>]\n\
 [-shapes=<filename>,<xColumn>,<yColumn>[,type=<lineType>][,thickness=<value>]]\n\
 [-symbols=<filename>,<xColumn>,<yColumn>[,type=<symbolType>][,fill][,thickness=<value>][,scale=<factor>]]\n\
 [-swapxy] [-yflip] [-equalaspect[={-1,1}]]\n\
 [-xlabel=<string>|@<parameter-name>[,scale=<value>][,edit=<edit-command>]] [-ylabel=<string>|@<parameter-name>[,scale=<value>][,edit=<edit-command>]] \n\
 [-title=<string>|@<parameter-name>|filename[,edit=<string>]]\n\
 [-topline=<string>|@<parameter-name>|filename[,edit=<string>][,format=string]] [-toptitle] [-nolabels]\n\
 [-yrange=minimum=<value>|@<parameter_name>,maximum=<value>|@<parameter_name>] \n\
 [-xrange=minimum=<value>|@<parameter_name>,maximum=<value>|@<parameter_name>] \n\
 [-ystrings=[edit=<editCommand>][,sparse=<integer>][,scale=<value>]]\n\
 [-noborder] [-noscales] [-fillscreen] [-datestamp] [-verbosity[=<level>]]\n\
 [-layout=<nx>,<ny>] [-thickness=<integer>] [-xlog]\n\
 [-ticksettings=[{xy}time]] [-nocolorbar] [-yaxis=scaleValue=<value>|scaleParameter=<name>[,offsetValue=<number>|offsetParameter=<name>] \n\
 [-xaxis=scaleValue=<value>|scaleParameter=<name>[,offsetValue=<number>|offsetParameter=<name>] \n\
 [-fixfontsize=[all=.02][,legend=.015][,<x|y>xlabel=<value>][,<x|y>ticks=<value>][,title=<value>][,topline=<value>]]\n\
 [-convertunits={column|parameter},<name>,<new-units-name>,<old-units-name>[,<factor>]]\n\
 [-drawLine={x0value=<value> | p0value=<value> | x0parameter=<name> | p0parameter=<name>},\n\
            {x1value=<value> | p1value=<value> | x1parameter=<name> | p1parameter=<name>},\n\
            {y0value=<value> | q0value=<value> | y0parameter=<name> | q0parameter=<name>},\n\
            {y1value=<value> | q1value=<value> | y1parameter=<name> | q1parameter=<name>}\n\
            [,linetype=<integer>][,thickness=<integer>][,clip]\n\
Program by Michael Borland. (This is version 5, December 2019)\n";

static char *drawlineUsage = "-drawLine=\n\
{x0value=<value> | p0value=<value> | x0parameter=<name> | p0parameter=<name>}, \n\
{x1value=<value> | p1value=<value> | x1parameter=<name> | p1parameter=<name>}, \n\
{y0value=<value> | q0value=<value> | y0parameter=<name> | q0parameter=<name>}, \n\
{y1value=<value> | q1value=<value> | y1parameter=<name> | q1parameter=<name>} \n\
[,linetype=<integer>][,thickness=<integer>][,clip]\n";

#define DRAW_LINE_LINETYPEGIVEN 0x000001U
#define DRAW_LINE_CLIPGIVEN 0x000002U
#define DRAW_LINE_X0GIVEN 0x000040U
#define DRAW_LINE_Y0GIVEN 0x000080U
#define DRAW_LINE_P0GIVEN 0x000100U
#define DRAW_LINE_Q0GIVEN 0x000200U
#define DRAW_LINE_X1GIVEN 0x000400U
#define DRAW_LINE_Y1GIVEN 0x000800U
#define DRAW_LINE_P1GIVEN 0x001000U
#define DRAW_LINE_Q1GIVEN 0x002000U
#define DRAW_LINE_X0PARAM 0x004000U
#define DRAW_LINE_Y0PARAM 0x008000U
#define DRAW_LINE_P0PARAM 0x010000U
#define DRAW_LINE_Q0PARAM 0x020000U
#define DRAW_LINE_X1PARAM 0x040000U
#define DRAW_LINE_Y1PARAM 0x080000U
#define DRAW_LINE_P1PARAM 0x100000U
#define DRAW_LINE_Q1PARAM 0x200000U
typedef struct
{
  /* The order of the data in this special section must not be
     changed
  */
  double x0, y0, p0, q0;
  double x1, y1, p1, q1;
  char *x0Param, *y0Param, *p0Param, *q0Param;
  char *x1Param, *y1Param, *p1Param, *q1Param;
  /* end special section */
  int32_t linetype;
  int32_t linethickness;
  unsigned long flags;
} DRAW_LINE_SPEC;

typedef struct
{
  char *name, *new_units, *old_units;
  double factor;
  long is_parameter;
} CONVERSION_DEFINITION;

double *fill_levels(double **level, double min, double max, long levels);
void getDimensionParameters(SDDS_TABLE *SDDS_table, char *name_of_name, char **variable,
                            char **variableUnits,
                            double *minimum, double *interval, int32_t *number);

double **window_2d_array(double **data_value, double *xmin, double *xmax, double *ymin, double *ymax,
                         double dx, double dy, int32_t *nx, int32_t *ny, double *limit, short xlog, int32_t *nx_offset, long swap_xy);
void PlotShapesData(SHAPE_DATA *shape, long shapes, double xmin, double xmax, double ymin, double ymax);

char *getParameterLabel(SDDS_TABLE *SDDS_table, char *parameter_name, char *edit, char *format);
void checkParameter(SDDS_TABLE *SDDS_table, char *parameter_name);
void checkLabelParameters(SDDS_TABLE *SDDS_table, char *p1, char *p2, char *p3, char *p4);
void freeParameterLabel(char *users_label, char *label);
void plot3DSurface(double **data, long nx, long ny, double xmin, double xmax, double ymin, double ymax);
void make_enumerated_yscale(char **label, double *yposition, long labels, char *editCommand, long interval, double scale, long thickness, char *ylabel, double ylableScale);
void make_enumerated_xscale(char **label, double *xposition, long labels, char *editCommand, long interval, double scale, long thickness, char *xlabel, double xlabelScale);

long plot_contour(double **data_value, long nx, long ny, long verbosity,
                  double xmin, double xmax, double ymin, double ymax,
                  double dx, double dy, double *xintervals, double *yintervals,
                  char *device, long *frameEnded, char *title, char *xlabel,
                  char *ylabel, char *topline, double min_level, double max_level, double levelLimit[2],
                  long levels, long levelLists, double *levelList, double hue0, double hue1, long *layout,
                  long *ixl, long *iyl, long thickness, unsigned long long tsetFlags,
                  SHAPE_DATA *shape, long shapes, int *pen, long *flags,
                  long pause_interval, long columnmatches, char **columnname, long columns,
                  char *yEditCommand, long ySparseInterval, double yScale, long contour_label_interval,
                  long contour_label_offset, long do_shade, long waterfall,
                  char *colorName, char *colorUnits, long swap_xy, double xlabelScale, double ylabelScale, long yRangeProvided, long xRangeProvided,
                  DRAW_LINE_SPEC *drawLineSpec, long drawlines, long fill_screen, long nx_interp,
                  long ny_interp, double *orig_limit, short xlog, long nx_offset, short show_gaps);
long get_plot_labels(SDDS_DATASET *SDDS_table, char *indeptcolumn, char **columnname, long columnnames,
                     char *allmatches, char *waterfall_par,
                     char *users_xlabel, char *users_ylabel, char *users_title,
                     char **xlabel0, char **ylabel0, char **title0, long deltas, long xRangeProvide,
                     long conversions, CONVERSION_DEFINITION **ucd);
long process_data_values(double ***data_value, long nx, long ny, long deltas);
void process_data(double ***data_value, int32_t *nx, int32_t *ny, double *xmin, double *xmax,
                  double *ymin, double *ymax, double *dx, double *dy,
                  double *limit, long logscale, double logfloor,
                  long nx_interp, long ny_interp, long x_lowpass, long y_lowpass, long interp_flags,
                  char **xyzArray, char **xyzColumn, long verbosity, short xlog, int32_t *nx_offset, long swap_xy);

int X11_args(int argc, char **argv);
char *addOuterParentheses(char *arg);
void jxyplot_string(double *x, double *y, char *s, char xmode, char ymode);

char *rearrange_by_index(char *data, long *index, long element_size, long num);
long drawline_AP(DRAW_LINE_SPEC **drawLineSpec, long *drawlines, char **item, long items);
void determine_drawline(DRAW_LINE_SPEC *drawLineSpec, long drawlines, SDDS_TABLE *table);
void plot3DSurface(double **data, long nx, long ny, double xmin, double xmax, double ymin, double ymax);
void draw_lines(DRAW_LINE_SPEC *drawLineSpec, long drawlines, long linetypeDefault, double *limit);
void get_xyaxis_value(char *xscalePar, char *xoffsetPar, char *yscalPar, char *yoffsetPar,
                      SDDS_DATASET *SDDS_table,
                      double *xscaleValue, double *xoffsetValue, double *yscaleValue, double *yoffetValue,
                      char **users_xlabel, char **users_ylabel);
void SetupFontSize(FONT_SIZE *fs);
void parseCommandlineToMotif(int argc, char **argv);
void parseCommandlineToQT(int argc, char **argv);
void passCommandlineToPNG(int argc, char **argv);
void passCommandlineToPS(int argc, char **argv);

#ifndef COMPILE_AS_SUBROUTINE
FILE *outfile;
#endif

#ifdef VAX_VMS
#  define DEFAULT_DEVICE "regis"
#  include <unixlib.h>
#endif
#define DEFAULT_DEVICE "qt"

#define DEFAULT_FONT "rowmans"

#define SHADE_GRAY 0
#define SHADE_OPTIONS 1
static char *shadeOptions[SHADE_OPTIONS] = {"gray"};

#define WATERFALL_HORIZONTAL 0
#define WATERFALL_VERTICAL 1
#define WATERFALL_OPTIONS 2
static char *waterfallScroll[WATERFALL_OPTIONS] = {
  "horizontal",
  "vertical",
};

#define DELTAS_PLAIN 0
#define DELTAS_NORMALIZED 1
#define DELTAS_FRACTIONAL 2
#define DELTAS_OPTIONS 3
static char *deltasOption[DELTAS_OPTIONS] = {
  "plain",
  "normalized",
  "fractional",
};

#define SPECTRUM_TRUE " -spectrum true"

#define COLUMN_BASED 0
#define PARAMETER_BASED 1
#define ARRAY_BASED 2
#define DATA_CLASS_KEYWORDS 3
static char *data_class_keyword[DATA_CLASS_KEYWORDS] = {
  "column",
  "parameter",
  "array",
};

void ReadShapeData(SHAPE_DATA *shape, long shapes, long swapXY);

#ifdef COMPILE_AS_SUBROUTINE

#  include "draw.h"
void sddscontour_main(char *input_line)

#else

  int main(int argc, char **argv)

#endif
{
#ifdef COMPILE_AS_SUBROUTINE
  int argc;
  char **argv;
#endif
  long i, j, k, i_arg, preferv1v2Parameters, thickness, yaxisScaleProvided = 0, xaxisScaleProvided = 0;
  int code;
  double yaxisOffset = 0, xaxisOffset = 0;
  SCANNED_ARG *s_arg;
  SDDS_TABLE SDDS_table;
  char s[SDDS_MAXLINE];
  char *quantity, *variable1, *variable2, **columnmatch, *allmatches, *indepcolumn, *yaxisScalePar = NULL, *yaxisOffsetPar = NULL, *xaxisScalePar = NULL, *xaxisOffsetPar = NULL;
  char *waterfall_par, *waterfall_indeptcol, *waterfall_colorcol, *waterfall_scroll;
  char *variable1Units, *variable2Units, *variable1name, *variable2name;
  long columnmatches = 0, variable1Index, variable2Index;
  char *inputfile;
  char *users_title, *users_topline, *users_xlabel, *users_ylabel, *topline_editcommand = NULL, *topline_formatcommand = NULL,
    *title_editcommand = NULL;
  char *title, *topline, *xlabel, *ylabel = NULL, *xlabel_editcommand = NULL, *ylabel_editcommand = NULL;
  double **data_value, *waterfall_parValue, ylabelScale, xlabelScale;
  int32_t nx, ny, nx_offset = 0;
  long swap_xy, swap_array, waterfall;
  double dx, dy, xmin, xmax, ymin, ymax, yaxisScale = 0, xaxisScale = 0, xmin0, xmax0, ymin0, ymax0;
  char *ymaxPar, *yminPar, *xmaxPar, *xminPar, *maxPar, *minPar;
  double *xintervals, *yintervals;
  long levels, label_contour_interval, contour_label_offset;
  long do_shade, interp_flags = 0;
  double max_level = 0, min_level = 0, hue0, hue1;
  char *device, *output;
  long x_lowpass, y_lowpass, nx_interp, ny_interp;
  long pause_interval = 1, fill_screen = 0;
  double orig_limit[4] = {0, 0, 0, 0};
  static char bufferstr[SDDS_MAXLINE];
  double levelLimit[2];

  /* double limit[4];*/
  long flags, verbosity, contour_label_interval = 0;
  SHAPE_DATA *shape;
  long shapes;
  int pen[4] = {0, 0, 0, 0};
  char **rpn_definitions_file, **rpn_expression, *rpn_equation, *rpn_transform;
  long mem1, mem2;
  long rpn_expressions, rpn_definitions_files, deltas, vertical_waterfall;
  static char *equdf_name = "SCEQ.UDF";
  static char *trudf_name = "SCTR.UDF";
  long fixed_range, readstatus, logscale;
  int32_t ySparseInterval;
  int32_t columns = 0;
  char **columnname, *devargs, *buffer = NULL, *yEditCommand;
  double logfloor, yScale;
  unsigned long dummyFlags;
  unsigned long long tsetFlags = 0;
  long rowNumberType, columnNumberType, yRangeProvided, xRangeProvided;
  char pfix[IFPF_BUF_SIZE];
  long layout[2];
  long ixl = 0, iyl = 0, frameEnded = 0;
  char *colorName = NULL, *colorUnits = NULL;
  char *xyzArray[3], *ptr2;
  char *xyzColumn[3];
  long pipe = 0;
  char **shadelist;
  long shade_items, drawlines = 0;
  DRAW_LINE_SPEC *drawLineSpec = NULL;
  long levelLists = 0;
  double *levelList = NULL;
  short xlog = 0;
  FONT_SIZE fontsize;
  CONVERSION_DEFINITION **ucd=NULL;
  long conversions = 0;
  short show_gaps = 0;

  fontsize.autosize = 1;
  xlabelScale = ylabelScale = 1.0;
  rpn_definitions_file = rpn_expression = NULL;
  rpn_equation = rpn_transform = NULL;
  rpn_expressions = rpn_definitions_files = 0;
  quantity = variable1 = variable2 = NULL;
  waterfall_parValue = NULL;
  waterfall = 0;
  waterfall_par = waterfall_indeptcol = waterfall_colorcol = waterfall_scroll = NULL;
  variable1Units = variable2Units = variable1name = variable2name = NULL;
  inputfile = users_title = users_topline = users_xlabel = users_ylabel = NULL;
  data_value = NULL;
  shape = NULL;
  x_lowpass = y_lowpass = nx_interp = ny_interp = 0;
  levels = shapes = label_contour_interval = contour_label_offset = do_shade = 0;
  flags = verbosity = swap_xy = swap_array = fixed_range = 0;
  columnmatch = NULL;
  indepcolumn = NULL;
  logscale = logfloor = 0;
  deltas = -1;
  vertical_waterfall = 0;
  columnname = NULL;
  yEditCommand = NULL;
  ySparseInterval = 1;
  yScale = 1;
  preferv1v2Parameters = 0;
  hue0 = 0;
  hue1 = 1;
  layout[0] = layout[1] = 0;
  xyzArray[0] = xyzArray[1] = xyzArray[2] = NULL;
  xyzColumn[0] = xyzColumn[1] = xyzColumn[2] = NULL;
  xintervals = yintervals = NULL;
  thickness = 1;
  dx = dy = xmin = ymin = xmax = ymax = 0;
  yRangeProvided = xRangeProvided = 0;
  ymaxPar = yminPar = xmaxPar = xminPar = maxPar = minPar = NULL;
  levelLimit[0] = -(levelLimit[1] = DBL_MAX);

#ifdef COMPILE_AS_SUBROUTINE
  make_argv(&argc, &argv, input_line);
  flags = NO_GREND + DEVICE_DEFINED;
#else
  outfile = stdout;
  if ((device = getenv("MPL_DEVICE"))) {
    cp_str(&device, device);
#  ifdef VAX_VMS
    str_tolower(device);
#  endif
  }
#endif
  set_default_font(DEFAULT_FONT);
  if (argc < 2)
    bomb(NULL, USAGE);

#ifndef COMPILE_AS_SUBROUTINE
  if (1) {
    int n;
    n = X11_args(argc, argv);
    argc -= n;
    argv += n;
  }
#endif
  parseCommandlineToMotif(argc, argv);
  parseCommandlineToQT(argc, argv);
  passCommandlineToPNG(argc, argv);
  passCommandlineToPS(argc, argv);

  argc = scanargsg(&s_arg, argc, argv);
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      /* process options here */
      switch (code = match_string(s_arg[i_arg].list[0], option, OPTIONS, 0)) {
      case SET_WATERFALL:
        if (s_arg[i_arg].n_items < 4) {
          fprintf(stderr, "Error (sddscontour): invalid -waterfall syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &waterfall_par, 1, 0,
                          "independentColumn", SDDS_STRING, &waterfall_indeptcol, 1, 0,
                          "colorColumn", SDDS_STRING, &waterfall_colorcol, 1, 0,
                          "scroll", SDDS_STRING, &waterfall_scroll, 1, 0,
                          NULL) ||
            !waterfall_par || !waterfall_indeptcol || !waterfall_colorcol) {
          fprintf(stderr, "Error (sddscontour): invalid -waterfall syntax/values\n");
          return (1);
        }
        if (waterfall_scroll && (vertical_waterfall = match_string(waterfall_scroll, waterfallScroll,
                                                                   WATERFALL_OPTIONS, 0)) < 0) {
          fprintf(stderr, "Error (sddscontour): invalid scroll given in -waterfall syntax\n");
          return (1);
        }
        waterfall = 1;
        s_arg[i_arg].n_items++;
        break;
      case SET_QUANTITY:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error (sddscontour): invalid -quantity syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&quantity, s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): invalid -quantity syntax\n");
          return (1);
        }
        break;
      case SET_YAXIS:
        /*this is only valid for columnMatch plotting */
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "scaleValue", SDDS_DOUBLE, &yaxisScale, 1, 0,
                          "scaleParameter", SDDS_STRING, &yaxisScalePar, 1, 0,
                          "offsetValue", SDDS_DOUBLE, &yaxisOffset, 1, 0,
                          "offsetParameter", SDDS_STRING, &yaxisOffsetPar, 1, 0,
                          NULL))
          SDDS_Bomb("invalid -versus syntax/values");
        s_arg[i_arg].n_items += 1;
        if (!yaxisScale && !yaxisScalePar)
          SDDS_Bomb("Invaid -yaxis systax, the yaxis scale is not provided!");
        yaxisScaleProvided = 1;
        break;
      case SET_XAXIS:
        /*this is only valid for columnMatch plotting */
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "scaleValue", SDDS_DOUBLE, &xaxisScale, 1, 0,
                          "scaleParameter", SDDS_STRING, &xaxisScalePar, 1, 0,
                          "offsetValue", SDDS_DOUBLE, &xaxisOffset, 1, 0,
                          "offsetParameter", SDDS_STRING, &xaxisOffsetPar, 1, 0,
                          NULL))
          SDDS_Bomb("invalid -versus syntax/values");
        s_arg[i_arg].n_items += 1;
        if (!xaxisScale && !xaxisScalePar)
          SDDS_Bomb("Invaid -xaxis systax, the xaxis scale is not provided!");
        xaxisScaleProvided = 1;
        break;
      case SET_COLUMNMATCH:
        if (s_arg[i_arg].n_items < 3) {
          fprintf(stderr, "Error (sddscontour): invalid -column syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&indepcolumn, s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): invalid -column syntax\n");
          return (1);
        }
        columnmatches = s_arg[i_arg].n_items - 2;
        columnmatch = tmalloc(sizeof(*columnmatch) * columnmatches);
        for (i = 0; i < columnmatches; i++) {
          if (!SDDS_CopyString(&(columnmatch[i]), s_arg[i_arg].list[2 + i])) {
            fprintf(stderr, "Error (sddscontour): invalid -column syntax\n");
            return (1);
          }
        }
        break;
      case SET_ARRAY:
        if ((s_arg[i_arg].n_items != 4) && (s_arg[i_arg].n_items != 2)) {
          fprintf(stderr, "Error (sddscontour): invalid -array syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&(xyzArray[0]), s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): invalid -array syntax\n");
          return (1);
        }
        if (s_arg[i_arg].n_items == 4) {
          if (!SDDS_CopyString(&(xyzArray[1]), s_arg[i_arg].list[2])) {
            fprintf(stderr, "Error (sddscontour): invalid -array syntax\n");
            return (1);
          }
          if (!SDDS_CopyString(&(xyzArray[2]), s_arg[i_arg].list[3])) {
            fprintf(stderr, "Error (sddscontour): invalid -array syntax\n");
            return (1);
          }
        }
        break;
      case SET_XYZ:
        if (s_arg[i_arg].n_items != 4) {
          fprintf(stderr, "Error (sddscontour): invalid -xyz syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&(xyzColumn[0]), s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): invalid -xyz syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&(xyzColumn[1]), s_arg[i_arg].list[2])) {
          fprintf(stderr, "Error (sddscontour): invalid -xyz syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&(xyzColumn[2]), s_arg[i_arg].list[3])) {
          fprintf(stderr, "Error (sddscontour): invalid -xyz syntax\n");
          return (1);
        }
        break;
      case SET_SWAPARRAY:
        swap_array = 1;
        break;
      case SET_SWAP_XY:
        swap_xy = 1;
        break;
      case SET_YFLIP:
        flags |= Y_FLIP;
        break;
      case SET_FILLSCREEN:
        fill_screen = 1;
        break;
      case SET_XLOG:
        xlog = 1;
        break;
      case SET_SHOWGAPS:
        show_gaps = 1;
        break;
      case SET_3D:
        threeD = 1;
        break;
      case SET_DEVICE:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): invalid -device syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&device, s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): invalid -device syntax\n");
          return (1);
        }
        if (s_arg[i_arg].n_items > 2) {
          gs_device_arguments(s_arg[i_arg].list[2], 0); /* for backward compatibility */
          setDeviceArgv(s_arg[i_arg].list + 2, s_arg[i_arg].n_items - 2);
        }
        break;
#ifndef COMPILE_AS_SUBROUTINE
      case SET_OUTPUT:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error (sddscontour): couldn't scan output filename\n");
          return (1);
        }
        if (!SDDS_CopyString(&output, s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): couldn't scan output filename\n");
          return (1);
        }
        /*
         * Opening an output file should be silent unless an error occurs.
         * Using FOPEN_INFORM_OF_OPEN results in diagnostic messages such as
         * "<file> opened in mode w", which is undesirable for tools that
         * create temporary files during interactive operations (e.g., when
         * replots are triggered while zooming).  Use default behavior instead
         * so no message is printed.
         */
        outfile = fopen_e(output, "w", 0);
        break;
#endif
      case SET_SCALES:
        if (s_arg[i_arg].n_items != 5 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &orig_limit[0]) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%lf", &orig_limit[1]) != 1 ||
            sscanf(s_arg[i_arg].list[3], "%lf", &orig_limit[2]) != 1 ||
            sscanf(s_arg[i_arg].list[4], "%lf", &orig_limit[3]) != 1) {
          fprintf(stderr, "Error (sddscontour): incorrect -scales syntax\n");
          return (1);
        }
        break;
      case SET_LABEL_CONTOURS:
        if ((s_arg[i_arg].n_items != 2 && s_arg[i_arg].n_items != 3) ||
            sscanf(s_arg[i_arg].list[1], "%ld", &contour_label_interval) != 1) {
          fprintf(stderr, "Error (sddscontour): incorrect -label_contour syntax\n");
          return (1);
        }
        contour_label_offset = 0;
        if (s_arg[i_arg].n_items == 3 &&
            (sscanf(s_arg[i_arg].list[2], "%ld", &contour_label_offset) != 1 || contour_label_offset < 0)) {
          fprintf(stderr, "Error (sddscontour): incorrect -label_contour syntax\n");
          return (1);
        }
        break;
      case SET_CONTOURS:
        if (s_arg[i_arg].n_items == 2) {
          if (sscanf(s_arg[i_arg].list[1], "%ld", &levels) != 1) {
            fprintf(stderr, "Error (sddscontour): incorrect -contours syntax (invalid number of levels)\n");
            return (1);
          }
        } else if (s_arg[i_arg].n_items == 4) {
          if (sscanf(s_arg[i_arg].list[1], "%ld", &levels) != 1 ||
              sscanf(s_arg[i_arg].list[2], "%lf", &min_level) != 1 ||
              sscanf(s_arg[i_arg].list[3], "%lf", &max_level) != 1 ||
              min_level >= max_level) {
            fprintf(stderr, "Error (sddscontour): incorrect -contours syntax\n");
            return (1);
          }
        } else {
          fprintf(stderr, "Error (sddscontour): incorrect -contours syntax (wrong number of items)\n");
          return (1);
        }
        break;
      case SET_SHAPES:
      case SET_SYMBOLS:
        if (s_arg[i_arg].n_items < 4) {
          fprintf(stderr, "Error (sddscontour): incorrect -shapes or -symbols syntax---give filename and column names\n");
          return (1);
        }
        shape = trealloc(shape, sizeof(*shape) * (s_arg[i_arg].n_items - 1 + shapes));
        if (!SDDS_CopyString(&(shape[shapes].filename), s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): incorrect -shapes syntax--give filename(s)\n");
          return (1);
        }
        if (!SDDS_CopyString(&(shape[shapes].xColumn), s_arg[i_arg].list[2])) {
          fprintf(stderr, "Error (sddscontour): incorrect -shapes syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&(shape[shapes].yColumn), s_arg[i_arg].list[3])) {
          fprintf(stderr, "Error (sddscontour): incorrect -shapes syntax\n");
          return (1);
        }
        shape[shapes].plotSymbols = code == SET_SYMBOLS ? 1 : 0;
        shape[shapes].lineType = 0;
        shape[shapes].fill = 0;
        shape[shapes].scale = 1;
        shape[shapes].thickness = 1;
        if (s_arg[i_arg].n_items > 4) {
          s_arg[i_arg].n_items -= 4;
          dummyFlags = 0;
          if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 4, &s_arg[i_arg].n_items, 0,
                            "type", SDDS_LONG, &(shape[shapes].lineType), 1, 0,
                            "fill", -1, NULL, 0, 1,
                            "scale", SDDS_DOUBLE, &(shape[shapes].scale), 1, 0,
                            "thickness", SDDS_LONG, &(shape[shapes].thickness), 1, 0,
                            NULL)) {
            fprintf(stderr, "Error (sddscontour): invalid -shapes or -symbols sytnax\n");
            return (1);
          }
          if (dummyFlags & 1)
            shape[shapes].fill = 1;
        }
        if (!fexists(shape[shapes].filename)) {
          fprintf(stderr, "Error (sddscontour): file %s not found\n", shape[shapes].filename);
          return (1);
        }
        shapes++;
        break;
      case SET_EQUAL_ASPECT:
        switch (s_arg[i_arg].n_items) {
        case 1:
          flags |= EQUAL_ASPECT1;
          break;
        case 2:
          if (sscanf(s_arg[i_arg].list[1], "%ld", &i) != 1) {
            fprintf(stderr, "Error (sddscontour): incorrect -equal_aspect syntax\n");
            return (1);
          }
          if (i == 1)
            flags |= EQUAL_ASPECT1;
          else if (i == -1)
            flags |= EQUAL_ASPECT_1;
          else {
            fprintf(stderr, "Error (sddscontour): incorrect -equal_aspect syntax\n");
            return (1);
          }
          break;
        default:
          fprintf(stderr, "Error (sddscontour): incorrect -equal_aspect syntax\n");
          return (1);
        }
        break;
      case SET_XLABEL:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): incorrect -xlabel syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&users_xlabel, s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): incorrect -xlabel syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "scale", SDDS_DOUBLE, &xlabelScale, 1, 0,
                          "edit", SDDS_STRING, &xlabel_editcommand, 1, 0,
                          NULL) ||
            xlabelScale <= 0) {
          fprintf(stderr, "Error (sddscontour): invalid -xlabel syntax/values\n");
          return (1);
        }
        s_arg[i_arg].n_items += 2;
        break;
      case SET_YLABEL:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): incorrect -ylabel syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&users_ylabel, s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): incorrect -ylabel syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "scale", SDDS_DOUBLE, &ylabelScale, 1, 0,
                          "edit", SDDS_STRING, &ylabel_editcommand, 1, 0,
                          NULL) ||
            ylabelScale <= 0) {
          fprintf(stderr, "Error (sddscontour): invalid -ylabel syntax/values\n");
          return (1);
        }
        s_arg[i_arg].n_items += 2;
        break;
      case SET_TITLE:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): incorrect -title syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&users_title, s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): incorrect -title syntax\n");
          return (1);
        }
        if (s_arg[i_arg].n_items > 2) {
          s_arg[i_arg].n_items -= 2;
          if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                            "edit", SDDS_STRING, &title_editcommand, 1, 0, NULL)) {
            fprintf(stderr, "Error (sddscontour): invalid -title syntax/values\n");
            return (1);
          }
          s_arg[i_arg].n_items += 2;
        }
        break;
      case SET_TOPLINE:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): incorrect -topline syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&users_topline, s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): incorrect -topline syntax\n");
          return (1);
        }
        if (s_arg[i_arg].n_items > 2) {
          s_arg[i_arg].n_items -= 2;
          if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                            "edit", SDDS_STRING, &topline_editcommand, 1, 0,
                            "format", SDDS_STRING, &topline_formatcommand, 1, 0, NULL)) {
            fprintf(stderr, "Error (sddscontour): invalid -topline syntax/values\n");
            return (1);
          }
          s_arg[i_arg].n_items += 2;
        }
        break;
      case SET_NO_BORDER:
        flags |= NO_BORDER;
        break;
      case SET_NO_COLOR_BAR:
        flags |= NO_COLOR_BAR;
        break;
      case SET_NO_SCALES:
        flags |= NO_SCALES;
        break;
      case SET_NO_LABELS:
        flags |= NO_LABELS;
        break;
      case SET_DATE_STAMP:
        flags |= DATE_STAMP;
        break;
      case SET_MAPSHADE:
        if (s_arg[i_arg].n_items != 3) {
          fprintf(stderr, "Error (sddscontour): incorrect -mapshade syntax (wrong number of items)\n");
          return (1);
        }
        if (sscanf(s_arg[i_arg].list[1], "%lf", &hue0) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%lf", &hue1) != 1 ||
            hue0 < 0 || hue1 < 0 || hue0 > 1 || hue1 > 1 || hue0 == hue1) {
          fprintf(stderr, "Error (sddscontour): -incorrect -mapshade syntax.  Hues must be [0, 1].\n");
          return (1);
        }
        break;
      case SET_LAYOUT:
        if (s_arg[i_arg].n_items != 3) {
          fprintf(stderr, "Error (sddscontour): invalid -layout syntax\n");
          return (1);
        }
        if (sscanf(s_arg[i_arg].list[1], "%ld", &layout[0]) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%ld", &layout[1]) != 1 ||
            layout[0] <= 0 || layout[1] <= 0) {
          fprintf(stderr, "Error (sddscontour): invalid -layout syntax\n");
          return (1);
        }
        break;
      case SET_SHADE:
        do_shade = 1;
        levels = 100;
        min_level = max_level = 0;
        shadelist = tmalloc(sizeof(*shadelist) * s_arg[i_arg].n_items);
        shade_items = 0;
        for (i = 0; i < s_arg[i_arg].n_items; i++) {
          if (match_string(s_arg[i_arg].list[i], shadeOptions, SHADE_OPTIONS, 0) >= 0) {
            do_shade = 2;
          } else {
            shadelist[shade_items] = s_arg[i_arg].list[i];
            shade_items++;
          }
        }

        if (shade_items == 3 || shade_items > 4) {
          fprintf(stderr, "Error (sddscontour): incorrect -shade syntax (wrong number of items)\n");
          return (1);
        }
        if (shade_items >= 2) {
          if (sscanf(shadelist[1], "%ld", &levels) != 1 ||
              levels == 0) {
            fprintf(stderr, "Error (sddscontour): incorrect -shade syntax (invalid number of levels)\n");
            return (1);
          }
        }
        if (shade_items == 4) {
          if (sscanf(shadelist[2], "%lf", &min_level) != 1 ||
              sscanf(shadelist[3], "%lf", &max_level) != 1 ||
              min_level > max_level) {
            fprintf(stderr, "Error (sddscontour): incorrect -shade syntax\n");
            return (1);
          }
        }
        if (levels > 100) {
          levels = 100;
        }
        free(shadelist);
        break;
      case SET_TOPTITLE:
        flags |= TITLE_AT_TOP;
        break;
      case SET_VERBOSITY:
        verbosity = 1;
        if (s_arg[i_arg].n_items > 1) {
          if (sscanf(s_arg[i_arg].list[1], "%ld", &verbosity) != 1 ||
              verbosity < 0) {
            fprintf(stderr, "Error (sddscontour): incorrect -verbosity syntax\n");
            return (1);
          }
        }
        break;
      case SET_EQUATION:
        if ((s_arg[i_arg].n_items < 2) || (s_arg[i_arg].n_items > 3)) {
          fprintf(stderr, "Error (sddscontour): incorrect -equation syntax\n");
          return (1);
        }
        if (s_arg[i_arg].n_items == 2) {
          if (!SDDS_CopyString(&rpn_equation, s_arg[i_arg].list[1])) {
            fprintf(stderr, "Error (sddscontour): incorrect -equation syntax\n");
            return (1);
          }
          if (!strlen(rpn_equation)) {
            fprintf(stderr, "Error (sddscontour): incorrect -equation syntax\n");
            return (1);
          }
        } else if (s_arg[i_arg].n_items == 3) {
          if (strncmp(s_arg[i_arg].list[2], "algebraic", strlen(s_arg[i_arg].list[2])) == 0) {
            ptr2 = addOuterParentheses(s_arg[i_arg].list[1]);
            if2pf(pfix, ptr2, sizeof pfix);
            free(ptr2);
            if (!SDDS_CopyString(&rpn_equation, pfix)) {
              fprintf(stderr, "Error (sddscontour): problem copying argument string\n");
              return (1);
            }
          } else {
            fprintf(stderr, "Error (sddscontour): incorrect -equation syntax\n");
            return (1);
          }
        }
        break;
      case SET_RPN_DEFNS_FILES:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): invalid -rpn_definitions_files syntax\n");
          return (1);
        }
        rpn_definitions_file = trealloc(rpn_definitions_file,
                                        sizeof(*rpn_definitions_file) * (rpn_definitions_files + s_arg[i_arg].n_items - 1));
        for (i = 1; i < s_arg[i_arg].n_items; i++) {
          if (!SDDS_CopyString(&(rpn_definitions_file[rpn_definitions_files + i - 1]), s_arg[i_arg].list[i])) {
            fprintf(stderr, "Error (sddscontour): invalid -rpn_definitions_files syntax\n");
            return (1);
          }
          if (!fexists(rpn_definitions_file[rpn_definitions_files + i - 1])) {
            fprintf(stderr, "Error (sddscontour): one or more rpn definitions files do not exist\n");
            return (1);
          }
        }
        rpn_definitions_files += s_arg[i_arg].n_items - 1;
        break;
      case SET_RPN_EXPRESSIONS:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): invalid -rpn_expressions syntax\n");
          return (1);
        }
        if (strncmp(s_arg[i_arg].list[s_arg[i_arg].n_items - 1], "algebraic", strlen(s_arg[i_arg].list[s_arg[i_arg].n_items - 1])) == 0) {
          rpn_expression = trealloc(rpn_expression,
                                    sizeof(*rpn_expression) * (rpn_expressions + s_arg[i_arg].n_items - 2));
          for (i = 1; i < s_arg[i_arg].n_items - 1; i++) {
            ptr2 = addOuterParentheses(s_arg[i_arg].list[i]);
            if2pf(pfix, ptr2, sizeof pfix);
            free(ptr2);
            if (!SDDS_CopyString(&rpn_expression[rpn_expressions + i - 1], pfix)) {
              fprintf(stderr, "Error (sddscontour): invalid -rpn_expressions syntax\n");
              return (1);
            }
          }
          rpn_expressions += s_arg[i_arg].n_items - 2;
        } else {
          rpn_expression = trealloc(rpn_expression,
                                    sizeof(*rpn_expression) * (rpn_expressions + s_arg[i_arg].n_items - 1));
          for (i = 1; i < s_arg[i_arg].n_items; i++) {
            if (!SDDS_CopyString(&(rpn_expression[rpn_expressions + i - 1]), s_arg[i_arg].list[i])) {
              fprintf(stderr, "Error (sddscontour): invalid -rpn_expressions syntax\n");
              return (1);
            }
          }
          rpn_expressions += s_arg[i_arg].n_items - 1;
        }
        break;
      case SET_RPN_TRANSFORM:
        if ((s_arg[i_arg].n_items < 2) || (s_arg[i_arg].n_items > 3)) {
          fprintf(stderr, "Error (sddscontour): incorrect -rpn_transform syntax\n");
          return (1);
        }
        if (s_arg[i_arg].n_items == 2) {
          if (!SDDS_CopyString(&rpn_transform, s_arg[i_arg].list[1])) {
            fprintf(stderr, "Error (sddscontour): incorrect -rpn_transform syntax\n");
            return (1);
          }
          if (!strlen(rpn_transform)) {
            fprintf(stderr, "Error (sddscontour): incorrect -rpn_transform syntax\n");
            return (1);
          }
        } else if (s_arg[i_arg].n_items == 3) {
          if (strncmp(s_arg[i_arg].list[2], "algebraic", strlen(s_arg[i_arg].list[2])) == 0) {
            ptr2 = addOuterParentheses(s_arg[i_arg].list[1]);
            if2pf(pfix, ptr2, sizeof pfix);
            free(ptr2);
            if (!SDDS_CopyString(&rpn_transform, pfix)) {
              fprintf(stderr, "Error (sddscontour): incorrect -rpn_transform syntax\n");
              return (1);
            }
          } else {
            fprintf(stderr, "Error (sddscontour): incorrect -rpn_transform syntax\n");
            return (1);
          }
        }
        break;
      case SET_INTERPOLATE:
        if (s_arg[i_arg].n_items < 3 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &nx_interp) != 1 || nx_interp <= 0 ||
            !(nx_interp == 1 || power_of_2(nx_interp)) ||
            sscanf(s_arg[i_arg].list[2], "%ld", &ny_interp) != 1 || ny_interp <= 0 ||
            !(ny_interp == 1 || power_of_2(ny_interp))) {
          fprintf(stderr, "Error (sddscontour): invalid -interpolate syntax--integers must be 2^n\n");
          return (1);
        }
        if (s_arg[i_arg].n_items > 3) {
          char *flag_text[3] = {"floor", "ceiling", "antiripple"};
          long flag_bit[3] = {CONTOUR_FLOOR, CONTOUR_CEILING, CONTOUR_ANTIRIPPLE};
          interp_flags = 0;
          for (i = 3; i < s_arg[i_arg].n_items; i++) {
            if ((j = match_string(s_arg[i_arg].list[i], flag_text, 3, 0)) >= 0)
              interp_flags |= flag_bit[j];
            else {
              fprintf(stderr, "Error (sddscontour): unknown modifer given with -interpolate\n");
              return (1);
            }
          }
        }
        break;
      case SET_FILTER:
        if (s_arg[i_arg].n_items != 3 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &x_lowpass) != 1 || x_lowpass <= 0 ||
            sscanf(s_arg[i_arg].list[2], "%ld", &y_lowpass) != 1 || y_lowpass <= 0) {
          fprintf(stderr, "Error (sddscontour): invalid -filter syntax--integers must be > 0\n");
          return (1);
        }
        if (nx_interp == 0)
          nx_interp = 1;
        if (ny_interp == 0)
          ny_interp = 1;
        break;
      case SET_FIXED_RANGE:
        fixed_range = 1;
        break;
      case SET_LOGSCALE:
        logscale = 1;
        if ((s_arg[i_arg].n_items != 1 && s_arg[i_arg].n_items != 2) ||
            ((s_arg[i_arg].n_items == 2 && sscanf(s_arg[i_arg].list[1], "%lf", &logfloor) != 1) ||
             logfloor < 0)) {
          fprintf(stderr, "Error (sddscontour): invalid -logscale syntax\n");
          return (1);
        }
        break;
      case SET_DELTAS:
        deltas = DELTAS_PLAIN;
        if (s_arg[i_arg].n_items >= 2) {
          if ((deltas = match_string(s_arg[i_arg].list[1],
                                     deltasOption, DELTAS_OPTIONS, 0)) < 0) {
            fprintf(stderr, "Error (sddscontour): invalid -deltas syntax\n");
            return (1);
          }
        }
        break;
      case SET_YRANGE:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): invalid -yRange syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "maximum", SDDS_STRING, &maxPar, 1, 0,
                          "minimum", SDDS_STRING, &minPar, 1, 0,
                          NULL)) {
          fprintf(stderr, "Error (sddscontour): invalid -yRange syntax/values\n");
          return (1);
        }
        if (maxPar) {
          if (wild_match(maxPar, "@*"))
            SDDS_CopyString(&ymaxPar, maxPar + 1);
          else if (!get_double(&ymax, maxPar)) {
            fprintf(stderr, "Error (sddscontour): invalid -yRange syntax/values\n");
            return (1);
          }
          free(maxPar);
        }
        if (minPar) {
          if (wild_match(minPar, "@*"))
            SDDS_CopyString(&yminPar, minPar + 1);
          else if (!get_double(&ymin, minPar)) {
            fprintf(stderr, "Error (sddscontour): invalid -yRange syntax/values\n");
            return (1);
          }
          free(minPar);
        }
        s_arg[i_arg].n_items++;
        yRangeProvided = 1;
        break;
      case SET_XRANGE:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): invalid -xRange syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "maximum", SDDS_STRING, &maxPar, 1, 0,
                          "minimum", SDDS_STRING, &minPar, 1, 0,
                          NULL)) {
          fprintf(stderr, "Error (sddscontour): invalid -xRange syntax/values\n");
          return (1);
        }
        if (maxPar) {
          if (wild_match(maxPar, "@*"))
            SDDS_CopyString(&xmaxPar, maxPar + 1);
          else if (!get_double(&xmax, maxPar)) {
            fprintf(stderr, "Error (sddscontour): invalid -xRange syntax/values\n");
            return (1);
          }
          free(maxPar);
        }
        if (minPar) {
          if (wild_match(minPar, "@*"))
            SDDS_CopyString(&xminPar, minPar + 1);
          else if (!get_double(&xmin, minPar)) {
            fprintf(stderr, "Error (sddscontour): invalid -xRange syntax/values\n");
            return (1);
          }
          free(minPar);
        }
        s_arg[i_arg].n_items++;
        xRangeProvided = 1;
        break;
      case SET_YSTRINGS:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): invalid -yStrings syntax\n");
          return (1);
        }
        yEditCommand = NULL;
        ySparseInterval = 1;
        yScale = 1;
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "edit", SDDS_STRING, &yEditCommand, 1, 0,
                          "sparse", SDDS_LONG, &ySparseInterval, 1, 0,
                          "scale", SDDS_DOUBLE, &yScale, 1, 0,
                          NULL) ||
            ySparseInterval <= 0 || yScale <= 0) {
          fprintf(stderr, "Error (sddscontour): invalid -yStrings syntax/values\n");
          return (1);
        }
        s_arg[i_arg].n_items++;
        break;
      case SET_EDITYSTRINGS:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Error (sddscontour): invalid -editYstrings syntax\n");
          return (1);
        }
        if (!SDDS_CopyString(&yEditCommand, s_arg[i_arg].list[1])) {
          fprintf(stderr, "Error (sddscontour): invalid -editYstrings syntax\n");
          return (1);
        }
        break;
      case SET_PREFERV1V2PARAMS:
        preferv1v2Parameters = 1;
        break;
      case SET_THICKNESS:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &thickness) != 1 ||
            thickness <= 0 || thickness > 9) {
          fprintf(stderr, "Error (sddscontour): invalid -thickness syntax\n");
          return (1);
        }
        break;
      case SET_TICKSETTINGS:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (sddscontour): invalid -ticksettings syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items--;
        if (!scanItemListLong(&tsetFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                              "xtime", -1, NULL, 0, TICKSET_XTIME,
                              NULL)) {
          fprintf(stderr, "Error (sddscontour): invalid -ticksettings syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items++;
        break;
      case SET_PIPE:
        pipe = 1;
        break;
      case SET_DRAWLINE:
        if (!drawline_AP(&drawLineSpec, &drawlines, s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1)) {
          fprintf(stderr, "Error (sddscontour): invalid -drawline syntax\n");
          return (1);
        }
        break;
      case SET_LEVELLIST:
        if (s_arg[i_arg].n_items < 2) {
          fprintf(stderr, "Error (%s): invalid -levelList syntax\n", argv[0]);
          return (1);
        }
        if (levelLists != 0) {
          fprintf(stderr, "Error (%s): invalid syntax: specify -levelList once only\n", argv[0]);
          return (1);
        }
        levelLists = s_arg[i_arg].n_items - 1;
        levelList = trealloc(levelList, sizeof(*levelList) * (levelLists));
        for (i = 0; i < levelLists; i++) {
          if (sscanf(s_arg[i_arg].list[i + 1], "%lf", &levelList[i]) != 1) {
            fprintf(stderr, "Error (%s): invalid -levelList syntax or value\n", argv[0]);
            return (1);
          }
        }
        break;
      case SET_FIXFONTSIZE:
        fontsize.autosize = 0;
        fontsize.all = -1;
        fontsize.legend = -1;
        fontsize.xlabel = -1;
        fontsize.ylabel = -1;
        fontsize.xticks = -1;
        fontsize.yticks = -1;
        fontsize.title = -1;
        fontsize.topline = -1;
        if (s_arg[i_arg].n_items == 1) {
          fontsize.all = .02;
          SetupFontSize(&fontsize);
        } else {
          s_arg[i_arg].n_items--;
          if (!scanItemList(&dummyFlags,
                            s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                            "all", SDDS_DOUBLE, &(fontsize.all), 1, 0,
                            "legend", SDDS_DOUBLE, &(fontsize.legend), 1, 0,
                            "xlabel", SDDS_DOUBLE, &(fontsize.xlabel), 1, 0,
                            "ylabel", SDDS_DOUBLE, &(fontsize.ylabel), 1, 0,
                            "xticks", SDDS_DOUBLE, &(fontsize.xticks), 1, 0,
                            "yticks", SDDS_DOUBLE, &(fontsize.yticks), 1, 0,
                            "title", SDDS_DOUBLE, &(fontsize.title), 1, 0,
                            "topline", SDDS_DOUBLE, &(fontsize.topline), 1, 0,
                            NULL)) {
            fprintf(stderr, "Error (sddscontour): invalid -fixfontsize syntax: -fixfontsize=[all=.02][,legend=.015][,<x|y>xlabel=<value>][,<x|y>ticks=<value>][,title=<value>][,topline=<value>]\n");
            return (1);
          }
          s_arg[i_arg].n_items++;
          SetupFontSize(&fontsize);
        }
        break;
      case SET_LIMITLEVELS:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "minimum", SDDS_DOUBLE, &levelLimit[0], 1, 0,
                          "maximum", SDDS_DOUBLE, &levelLimit[1], 1, 0,
                          NULL))
          SDDS_Bomb("invalid -limitLevels syntax/values");
        break;
      case SET_CONVERTUNITS:
        if ((s_arg[i_arg].n_items != 5) && (s_arg[i_arg].n_items != 6)) {
          fprintf(stderr, "Error (sddscontour): invalid -convertunits syntax: -convertunits={column|parameter},<name>,<new-units-name>,<old-units-name>[,<factor>]\n");
          return (1);
        }
        if (conversions == 0) {
          ucd = (CONVERSION_DEFINITION **)SDDS_Malloc(sizeof(CONVERSION_DEFINITION *) * (conversions + 1));
        } else {
          ucd = (CONVERSION_DEFINITION **)SDDS_Realloc(ucd, sizeof(CONVERSION_DEFINITION *) * (conversions + 1));
        }
        ucd[conversions] = tmalloc(sizeof(CONVERSION_DEFINITION));
        switch (match_string(s_arg[i_arg].list[1], data_class_keyword, DATA_CLASS_KEYWORDS, 0)) {
        case COLUMN_BASED:
          ucd[conversions]->is_parameter = 0;
          break;
        case PARAMETER_BASED:
          ucd[conversions]->is_parameter = 1;
          break;
        default:
          fprintf(stderr, "Error (sddscontour): invalid -convertunits syntax: -convertunits={column|parameter},<name>,<new-units-name>,<old-units-name>[,<factor>]\n");
          return (1);
        }
        ucd[conversions]->name = s_arg[i_arg].list[2];
        ucd[conversions]->new_units = s_arg[i_arg].list[3];
        ucd[conversions]->old_units = s_arg[i_arg].list[4];
        if (s_arg[i_arg].n_items == 6) {
          if (sscanf(s_arg[i_arg].list[5], "%lf", &ucd[conversions]->factor) != 1) {
            fprintf(stderr, "Error (sddscontour): invalid -convertunits syntax: -convertunits={column|parameter},<name>,<new-units-name>,<old-units-name>[,<factor>]\n");
            return (1);
          }
        } else {
          ucd[conversions]->factor = 1;
        }
        conversions++;
        break;
      default:
        fprintf(stderr, "unknown option - %s given.\n", s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      /* argument is input filename */
      if (inputfile)
        bomb("only one filename accepted", NULL);
      inputfile = s_arg[i_arg].list[0];
    }
  }

  if (inputfile == NULL) {
    if (!pipe)
      bomb("no input file listed", NULL);
  }
  if (xRangeProvided && xaxisScaleProvided)
    bomb("-xrange and -xaxis options can not be provided at the same time.", NULL);
  if (yRangeProvided && yaxisScaleProvided)
    bomb("-yrange and -yaxis options can not be provided at the same time.", NULL);

  if (!device)
    device = DEFAULT_DEVICE;

  if (strncmp("motif", device, strlen(device)) == 0) {
    devargs = gs_device_arguments(NULL, 1);
    buffer = tmalloc(sizeof(*buffer) * (strlen(devargs ? devargs : "") + strlen(SPECTRUM_TRUE) + 1));
    sprintf(buffer, "%s%s", devargs ? devargs : "", SPECTRUM_TRUE);
    gs_device_arguments(buffer, 0);
  } else if (strncmp("qt", device, strlen(device)) == 0) {
    devargs = gs_device_arguments(NULL, 1);
    buffer = tmalloc(sizeof(*buffer) * (strlen(devargs ? devargs : "") + strlen(SPECTRUM_TRUE) + 1));
    sprintf(buffer, "%s%s", devargs ? devargs : "", SPECTRUM_TRUE);
    gs_device_arguments(buffer, 0);
  }

  if (rpn_definitions_files) {
    rpn(rpn_definitions_file[0]);
    if (rpn_check_error())
      return (1);
    for (i = 1; i < rpn_definitions_files; i++) {
      sprintf(s, "\"%s,s\"  @", rpn_definitions_file[i]);
      rpn(s);
      if (rpn_check_error())
        return (1);
    }
  } else {
    rpn(getenv("RPN_DEFNS"));
    if (rpn_check_error())
      return (1);
  }
  for (i = 0; i < rpn_expressions; i++) {
    rpn(rpn_expression[i]);
    if (rpn_check_error())
      return (1);
  }
  if (!SDDS_InitializeInput(&SDDS_table, inputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    return (1);
  }

  if (users_topline && strlen(users_topline) && strncmp_case_insensitive(users_topline, "filename", MIN(8, strlen(users_topline))) == 0) {
    free(users_topline);
    SDDS_CopyString(&users_topline, inputfile);
    if (topline_editcommand) {
      strcpy(bufferstr, users_topline);
      edit_string(bufferstr, topline_editcommand);
      free(users_topline);
      SDDS_CopyString(&users_topline, bufferstr);
      free(topline_editcommand);
      topline_editcommand = NULL;
    }
    if (topline_formatcommand) {
      printf("%s\n", topline_formatcommand);
      sprintf(bufferstr, topline_formatcommand, users_topline);
      SDDS_CopyString(&users_topline, bufferstr);
      free(topline_formatcommand);
      topline_formatcommand = NULL;
    }
  }

  if (users_title && strlen(users_title) && strncmp_case_insensitive(users_title, "filename", MIN(8, strlen(users_title))) == 0) {
    free(users_title);
    SDDS_CopyString(&users_title, inputfile);
    if (title_editcommand) {
      strcpy(bufferstr, users_title);
      edit_string(bufferstr, title_editcommand);
      free(users_title);
      SDDS_CopyString(&users_title, bufferstr);
      free(title_editcommand);
      title_editcommand = NULL;
    }
  }

  checkLabelParameters(&SDDS_table, users_xlabel, users_ylabel, users_title,
                       users_topline);

  if (shapes)
    ReadShapeData(shape, shapes, swap_xy);
  if (swap_xy)
    SWAP_DOUBLE(xlabelScale, ylabelScale);

  if (!quantity && !rpn_equation && !columnmatch && !xyzArray[0] && !xyzColumn[2] && !waterfall) {
    char **ptr;
    int32_t number;
    if (!(ptr = SDDS_GetColumnNames(&SDDS_table, &number)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (number > 1)
      SDDS_Bomb("no quantity specified and more than one column in file.\n");
    SDDS_CopyString(&quantity, *ptr);
    SDDS_FreeStringArray(ptr, number);
    if (verbosity > 0)
      printf("will do contour plotting for the quantity %s\n", quantity);
  }
  if (yaxisScalePar) {
    if (SDDS_GetParameterIndex(&SDDS_table, yaxisScalePar) < 0) {
      fprintf(stderr, "parameter %s does not exist in the input file.\n", yaxisScalePar);
      return (1);
    }
  }
  if (yaxisOffsetPar) {
    if (SDDS_GetParameterIndex(&SDDS_table, yaxisOffsetPar) < 0) {
      fprintf(stderr, "parameter %s does not exist in the input file.\n", yaxisOffsetPar);
      return (1);
    }
  }
  if (xaxisScalePar) {
    if (SDDS_GetParameterIndex(&SDDS_table, xaxisScalePar) < 0) {
      fprintf(stderr, "parameter %s does not exist in the input file.\n", xaxisScalePar);
      return (1);
    }
  }
  if (xaxisOffsetPar) {
    if (SDDS_GetParameterIndex(&SDDS_table, xaxisOffsetPar) < 0) {
      fprintf(stderr, "parameter %s does not exist in the input file.\n", xaxisOffsetPar);
      return (1);
    }
  }
  if (waterfall) {
    if (rpn_equation || columnmatch || quantity || xyzArray[0] || xyzColumn[2]) {
      SDDS_Bomb("waterfall option is not compatible with equation, columnmatch, xyz or array option!");
    }

    if (0 > SDDS_GetParameterIndex(&SDDS_table, waterfall_par) ||
        0 > SDDS_GetColumnIndex(&SDDS_table, waterfall_indeptcol) ||
        0 > SDDS_GetColumnIndex(&SDDS_table, waterfall_colorcol)) {
      SDDS_SetError("waterfall parameter or columns does not exist in the input file.");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return (1);
    }

    if (SDDS_GetColumnInformation(&SDDS_table, "symbol", &colorName, SDDS_GET_BY_NAME, waterfall_colorcol) != SDDS_STRING ||
        SDDS_GetColumnInformation(&SDDS_table, "units", &colorUnits, SDDS_GET_BY_NAME, waterfall_colorcol) != SDDS_STRING) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!colorName)
      colorName = waterfall_colorcol;
    for (i = 0; i < conversions; i++) {
      if (ucd[i]->is_parameter == 0) {
        if (wild_match(waterfall_colorcol, ucd[i]->name)) {
          SDDS_CopyString(&colorUnits, ucd[i]->new_units);
        }
      }
    }
  } else if (!rpn_equation && !columnmatch && !xyzArray[0] && !xyzColumn[2]) {
    if (0 > SDDS_GetColumnIndex(&SDDS_table, quantity)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    if (SDDS_GetColumnInformation(&SDDS_table, "symbol", &colorName, SDDS_GET_BY_NAME, quantity) != SDDS_STRING ||
        SDDS_GetColumnInformation(&SDDS_table, "units", &colorUnits, SDDS_GET_BY_NAME, quantity) != SDDS_STRING) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!colorName)
      colorName = quantity;
    if (rpn_transform)
      colorName = rpn_transform;
    for (i = 0; i < conversions; i++) {
      if (ucd[i]->is_parameter == 0) {
        if (wild_match(quantity, ucd[i]->name)) {
          SDDS_CopyString(&colorUnits, ucd[i]->new_units);
        }
      }
    }
  }
  if (waterfall) {
    long pages, rows, rows1;
    double *indepdata = NULL;
    double *indepdata_page = NULL;
    double *tmpptr = NULL;
    double **tmpptr2 = NULL;
    long *sorted_index = NULL;

    nx = ny = 0;
    pages = rows = 0;
    while ((readstatus = SDDS_ReadPage(&SDDS_table)) > 0) {
      waterfall_parValue = (double *)SDDS_Realloc(waterfall_parValue, sizeof(*waterfall_parValue) * (pages + 1));
      data_value = (double **)SDDS_Realloc(data_value, sizeof(*data_value) * (pages + 1));
      data_value[pages] = NULL;
      if (!SDDS_GetParameterAsDouble(&SDDS_table, waterfall_par, &waterfall_parValue[pages])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      if (pages == 0) {
        if (!(indepdata = SDDS_GetColumnInDoubles(&SDDS_table, waterfall_indeptcol)))
          bomb("unable to read independent variable data", NULL);
        rows = SDDS_CountRowsOfInterest(&SDDS_table);
        for (i = 0; i < conversions; i++) {
          if (ucd[i]->is_parameter == 0) {
            if (wild_match(waterfall_indeptcol, ucd[i]->name)) {
              for (j = 0; j < rows; j++) {
                indepdata[j] = indepdata[j] * ucd[i]->factor;
              }
            }
          }
        }

        if (!users_topline) {
          SDDS_GetDescription(&SDDS_table, &topline, NULL);
          if (!topline || !strlen(topline)) {
            sprintf(s, "Data from SDDS file %s, table %ld", inputfile, readstatus);
            SDDS_CopyString(&topline, s);
          }
        } else
          topline = users_topline;
        get_xyaxis_value(xaxisScalePar, xaxisOffsetPar, yaxisScalePar, yaxisOffsetPar,
                         &SDDS_table,
                         &xaxisScale, &xaxisOffset, &yaxisScale, &yaxisOffset, &users_xlabel, &users_ylabel);
      } else {
        rows1 = SDDS_CountRowsOfInterest(&SDDS_table);
        if (rows1 < rows) {
          fprintf(stderr, "The rows in page %d is less than that of the first page.\n", ny + 1);
          return (1);
        }
      }
      if (!(indepdata_page = SDDS_GetColumnInDoubles(&SDDS_table, waterfall_indeptcol)))
        bomb("unable to read independent variable data", NULL);
      sorted_index = sort_and_return_index(indepdata_page, SDDS_DOUBLE, rows, 1);

      if (!(tmpptr = (double *)SDDS_GetColumnInDoubles(&SDDS_table, waterfall_colorcol))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      if (drawlines && pages == 0)
        determine_drawline(drawLineSpec, drawlines, &SDDS_table);

      data_value[pages] = (double *)rearrange_by_index((char *)tmpptr, sorted_index, SDDS_type_size[SDDS_DOUBLE - 1], rows);

      free(indepdata_page);
      free(sorted_index);
      free(tmpptr);
      pages++;
    }
    nx = pages;
    ny = rows;
    sorted_index = sort_and_return_index(waterfall_parValue, SDDS_DOUBLE, pages, 1);
    tmpptr2 = (double **)rearrange_by_index((char *)(data_value), sorted_index, sizeof(double *), pages);
    free(sorted_index);
    free(data_value);
    data_value = tmpptr2;
    find_min_max(&xmin, &xmax, waterfall_parValue, nx);
    find_min_max(&ymin, &ymax, indepdata, ny);
    get_plot_labels(&SDDS_table, waterfall_par, &waterfall_colorcol, 1,
                    waterfall_colorcol, waterfall_indeptcol, users_xlabel, users_ylabel, users_title,
                    &xlabel, &ylabel, &title, deltas, xRangeProvided, conversions, ucd);
    free(waterfall_parValue);
    free(indepdata);
    dx = (xmax - xmin) / (nx - 1);
    dy = (ymax - ymin) / (ny - 1);
    /*   if (orig_limit[0]!=orig_limit[1] || orig_limit[2]!=orig_limit[3]) {
         memcpy((char*)limit, (char*)orig_limit, 4*sizeof(limit[0]));
         data_value = window_2d_array(data_value, &xmin, &xmax, &ymin, &ymax,
         dx, dy, &nx, &ny, limit);
         } */
    /*  flags |= NO_YSCALES;*/
    if (process_data_values(&data_value, nx, ny, deltas))
      return 1;
    /*above data_value is for horizontal waterfall*/
    if ((!vertical_waterfall && swap_xy) || (vertical_waterfall && !swap_xy)) {
      /*make vertical waterfall plot*/
      double **new_data;
      new_data = (double **)zarray_2d(sizeof(double), ny, nx);
      for (i = 0; i < nx; i++) {
        for (j = 0; j < ny; j++)
          new_data[j][i] = data_value[i][j];
      }
      SDDS_FreeMatrix((void **)data_value, nx);
      data_value = new_data;
      SWAP_PTR(xlabel, ylabel);
      SWAP_DOUBLE(xmin, ymin);
      SWAP_DOUBLE(dx, dy);
      SWAP_DOUBLE(xmax, ymax);
      SWAP_LONG(nx, ny);
    }
    if (xlabel[0] == '@')
      xlabel = getParameterLabel(&SDDS_table, xlabel + 1, xlabel_editcommand, NULL);

    if (ylabel[0] == '@') {
      ylabel = getParameterLabel(&SDDS_table, ylabel + 1, ylabel_editcommand, NULL);
    }
    if (title[0] == '@')
      title = getParameterLabel(&SDDS_table, title + 1, title_editcommand, NULL);
    if (topline[0] == '@')
      topline = getParameterLabel(&SDDS_table, topline + 1, topline_editcommand, topline_formatcommand);

    if (!SDDS_Terminate(&SDDS_table)) {
      SDDS_SetError("problem closing file");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    xmax = xmin + (nx - 1) * dx;
    ymax = ymin + (ny - 1) * dy;

    process_data(&data_value, &nx, &ny, &xmin, &xmax, &ymin, &ymax, &dx, &dy,
                 orig_limit, logscale, logfloor, nx_interp, ny_interp, x_lowpass,
                 y_lowpass, interp_flags, xyzArray, xyzColumn, verbosity, xlog, &nx_offset, swap_xy);
    if (yaxisScaleProvided) {
      ymin = (ymin - yaxisOffset) * yaxisScale;
      ymax = (ymax - yaxisOffset) * yaxisScale;
      dy = (ymax - ymin) / (ny - 1);
      yRangeProvided = 1;
    }
    if (xaxisScaleProvided) {
      xmin = (xmin - xaxisOffset) * xaxisScale;
      xmax = (xmax - xaxisOffset) * xaxisScale;
      dx = (xmax - xmin) / (nx - 1);
      xRangeProvided = 1;
    }
    plot_contour(data_value, nx, ny, verbosity,
                 xmin, xmax, ymin, ymax,
                 dx, dy, xintervals, yintervals,
                 device, &frameEnded, title, xlabel,
                 ylabel, topline, min_level, max_level, levelLimit,
                 levels, levelLists, levelList, hue0, hue1, layout,
                 &ixl, &iyl, thickness, tsetFlags,
                 shape, shapes, pen, &flags,
                 pause_interval, columnmatches, columnname, columns,
                 yEditCommand, ySparseInterval, yScale, contour_label_interval,
                 contour_label_offset, do_shade, 1, colorName, colorUnits, swap_xy, xlabelScale, ylabelScale, yRangeProvided, xRangeProvided,
                 drawLineSpec, drawlines, fill_screen, nx_interp, ny_interp, orig_limit, xlog, nx_offset, show_gaps);
    if (data_value)
      SDDS_FreeMatrix((void **)data_value, nx);
    data_value = NULL;
    free(waterfall_par);
    free(waterfall_indeptcol);
    free(waterfall_colorcol);
    free(title);
    free(xlabel);
    free(ylabel);
    if (!users_topline && topline)
      free(topline);
  } else {
    rowNumberType = 0;
    columnNumberType = 0;
    if (!columnmatch && !xyzArray[0] && !xyzColumn[2]) {
      if (!preferv1v2Parameters) {
        rowNumberType = SDDS_GetNamedParameterType(&SDDS_table, "NumberOfRows");
        columnNumberType = SDDS_GetNamedParameterType(&SDDS_table, "NumberOfColumns");
        SDDS_ClearErrors();
        if (rowNumberType && columnNumberType) {
          if (!SDDS_INTEGER_TYPE(rowNumberType) || !SDDS_INTEGER_TYPE(columnNumberType)) {
            fputs("NumberOfRows and NumberOfColumns parameters are present but at least one has a non-integer type--attempting alternative processing mode\n", stderr);
            rowNumberType = columnNumberType = 0;
          }
        } else
          rowNumberType = columnNumberType = 0;
        if (!rowNumberType || !columnNumberType) {
          if ((variable1Index = SDDS_GetParameterIndex(&SDDS_table, "Variable1Name")) < 0 ||
              (variable2Index = SDDS_GetParameterIndex(&SDDS_table, "Variable2Name")) < 0 ||
              SDDS_GetParameterType(&SDDS_table, variable1Index) != SDDS_STRING ||
              SDDS_GetParameterType(&SDDS_table, variable2Index) != SDDS_STRING)
            SDDS_Bomb("Can't figure out how to turn column into 2D grid!\nCheck existence and type of Variable1Name and Variable2Name");
        }
      } else {
        variable1Index = SDDS_GetParameterIndex(&SDDS_table, "Variable1Name");
        variable2Index = SDDS_GetParameterIndex(&SDDS_table, "Variable2Name");
        SDDS_ClearErrors();
        if (variable1Index >= 0 && variable2Index >= 0) {
          if (SDDS_GetParameterType(&SDDS_table, variable1Index) != SDDS_STRING ||
              SDDS_GetParameterType(&SDDS_table, variable2Index) != SDDS_STRING) {
            variable1Index = variable2Index = -1;
          }
        }
        if (variable1Index < 0 || variable2Index < 0) {
          rowNumberType = SDDS_GetNamedParameterType(&SDDS_table, "NumberOfRows");
          columnNumberType = SDDS_GetNamedParameterType(&SDDS_table, "NumberOfColumns");
          SDDS_ClearErrors();
          if (rowNumberType && columnNumberType) {
            if (!SDDS_INTEGER_TYPE(rowNumberType) || !SDDS_INTEGER_TYPE(columnNumberType)) {
              fputs("NumberOfRows and NumberOfColumns parameters are present but at least one has a non-integer type--attempting alternative processing mode\n", stderr);
              SDDS_Bomb("Can't figure out how to turn column into 2D grid!\n");
            }
          }
        }
      }
    } else if (!xyzArray[0] && !xyzColumn[2]) {
      if (!xRangeProvided) {
        if (SDDS_GetColumnIndex(&SDDS_table, indepcolumn) < 0) {
          fprintf(stderr, "error: couldn't find column %s in file\n", indepcolumn);
          return (1);
        }
      }
      SDDS_SetColumnFlags(&SDDS_table, 0);
      s[0] = 0;
      for (i = 0; i < columnmatches; i++) {
        if (strlen(s) < 256) {
          strcat(s, columnmatch[i]);
          strcat(s, " ");
        }

        if (!SDDS_SetColumnsOfInterest(&SDDS_table, SDDS_MATCH_STRING, columnmatch[i], SDDS_OR)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      }
      SDDS_CopyString(&allmatches, s);
      if ((columns = SDDS_CountColumnsOfInterest(&SDDS_table)) <= 0) {
        fprintf(stderr, "error: no columns found that match\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      SDDS_SetColumnFlags(&SDDS_table, 1);
    } else if (!xyzColumn[2]) {
      for (i = 0; i < 3; i++) {
        if (xyzArray[i]) {
          if (SDDS_GetArrayIndex(&SDDS_table, xyzArray[i]) < 0) {
            fprintf(stderr, "error: couldn't find array %s in file\n", xyzArray[i]);
            return (1);
          }
        }
      }
    } else {
      for (i = 0; i < 3; i++) {
        if (SDDS_GetColumnIndex(&SDDS_table, xyzColumn[i]) < 0) {
          fprintf(stderr, "error: couldn't find column %s in file\n", xyzColumn[i]);
          return (1);
        }
      }
    }

    if (rpn_equation)
      create_udf(equdf_name, rpn_equation);
    if (rpn_transform)
      create_udf(trudf_name, rpn_transform);
    if (fixed_range && (quantity || xyzColumn[2])) {
      double minMin, maxMax, thisMin, thisMax, *data;
      //double page1 = 0;
      long rows;
      maxMax = -(minMin = DBL_MAX);
      while (SDDS_ReadPage(&SDDS_table) > 0) {
        if ((rows = SDDS_RowCount(&SDDS_table)) <= 0)
          continue;
        if (!(data = SDDS_GetColumnInDoubles(&SDDS_table, quantity ? quantity : xyzColumn[2]))) {
          SDDS_SetError("problem reading data for fixed range determination");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }

        if (quantity) {
          for (i = 0; i < conversions; i++) {
            if (ucd[i]->is_parameter == 0) {
              if (wild_match(quantity, ucd[i]->name)) {
                for (j = 0; j < rows; j++) {
                  data[j] = data[j] * ucd[i]->factor;
                }
              }
            }
          }
        }
        if (xyzColumn[2]) {
          for (i = 0; i < conversions; i++) {
            if (ucd[i]->is_parameter == 0) {
              if (wild_match(xyzColumn[2], ucd[i]->name)) {
                for (j = 0; j < rows; j++) {
                  data[j] = data[j] * ucd[i]->factor;
                }
              }
            }
          }
        }

        find_min_max(&thisMin, &thisMax, data, rows);
        free(data);
        if (thisMin < minMin)
          minMin = thisMin;
        if (thisMax > maxMax)
          maxMax = thisMax;
        //page1++;
      }
      if (maxMax > minMin) {
        max_level = maxMax;
        min_level = minMin;
      }
      if (logscale) {
        if (max_level <= 0) {
          fprintf(stderr, "Error: can't do log scale with all data <=0\n");
          exit(1);
        }
        max_level = log10(max_level);
        if (min_level <= 0) {
          if (logfloor)
            min_level = logfloor;
          else {
            fprintf(stderr, "Error: can't do log scale with some data <=0. Try giving log floor.\n");
            exit(1);
          }
        } else
          min_level = log10(min_level);
      }
      if (!SDDS_Terminate(&SDDS_table) ||
          !SDDS_InitializeInput(&SDDS_table, inputfile)) {
        SDDS_SetError("problem closing and reopening input file");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    while ((readstatus = SDDS_ReadPage(&SDDS_table)) > 0) {
      get_xyaxis_value(xaxisScalePar, xaxisOffsetPar, yaxisScalePar, yaxisOffsetPar,
                       &SDDS_table,
                       &xaxisScale, &xaxisOffset, &yaxisScale, &yaxisOffset, &users_xlabel, &users_ylabel);
      if (drawlines)
        determine_drawline(drawLineSpec, drawlines, &SDDS_table);
      if (rowNumberType && columnNumberType) {
        if (!SDDS_GetParameter(&SDDS_table, "NumberOfRows", &nx) || !SDDS_GetParameter(&SDDS_table, "NumberOfColumns", &ny)) {
          fprintf(stderr, "error: unable to read NumberOfRows or NumberOfColumns parameter\n");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        if (!SDDS_CastValue(&nx, 0, rowNumberType, SDDS_LONG, &nx) ||
            !SDDS_CastValue(&ny, 0, columnNumberType, SDDS_LONG, &ny)) {
          fprintf(stderr, "error: unable to cast row or column numbers to type SDDS_LONG\n");
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
#ifndef COMPILE_AS_SUBROUTINE
        if (verbosity > 0)
          printf("Data has %" PRId32 " rows by %" PRId32 " columns\n", nx, ny);
#endif
        xmin = ymin = 0;
        dx = dy = 1;
        if (!users_xlabel) {
          SDDS_CopyString(&xlabel, "row");
          users_xlabel = xlabel;
        } else
          xlabel = users_xlabel;
        if (!users_ylabel) {
          SDDS_CopyString(&ylabel, "column");
          users_ylabel = ylabel;
        } else
          ylabel = users_ylabel;
        if (!users_title) {
          sprintf(s, "contours of constant %s", quantity ? quantity : rpn_equation);
          SDDS_CopyString(&title, s);
          users_title = title;
        } else
          title = users_title;
      } /*end of if (rowNumberType && columnNumberType) */
      else if (!columnmatch && !xyzArray[0] && !xyzColumn[2]) {
        getDimensionParameters(&SDDS_table, "Variable1Name",
                               &variable1, &variable1Units,
                               &xmin, &dx, &nx);
        getDimensionParameters(&SDDS_table, "Variable2Name",
                               &variable2, &variable2Units,
                               &ymin, &dy, &ny);
        if (!users_xlabel) {
          if (variable1Units) {
            sprintf(s, "%s (%s)", variable1, variable1Units);
            SDDS_CopyString(&xlabel, s);
          } else
            SDDS_CopyString(&xlabel, variable1);
        } else
          xlabel = users_xlabel;
        if (!users_ylabel) {
          if (variable2Units) {
            sprintf(s, "%s (%s)", variable2, variable2Units);
            SDDS_CopyString(&ylabel, s);
          } else
            SDDS_CopyString(&ylabel, variable2);
        } else
          ylabel = users_ylabel;
        if (!users_title) {
          if (rpn_transform)
            sprintf(s, "%s as a function of %s and %s", rpn_transform,
                    swap_xy ? variable2 : variable1, swap_xy ? variable1 : variable2);
          else
            sprintf(s, "%s as a function of %s and %s", quantity ? quantity : rpn_equation,
                    swap_xy ? variable2 : variable1, swap_xy ? variable1 : variable2);
          SDDS_CopyString(&title, s);
          users_title = title;
        } else
          title = users_title;

#ifndef COMPILE_AS_SUBROUTINE
        if (verbosity > 1) {
          /* -- print out some information about the data */
          printf("dimension 1:  name = %s, minimum = %e, interval = %e, dimension = %" PRId32 "\n",
                 variable1, xmin, dx, nx);
          printf("dimension 2:  name = %s, minimum = %e, interval = %e, dimension = %" PRId32 "\n",
                 variable2, ymin, dy, ny);
        }
#endif
      }

      if (!users_topline) {
        SDDS_GetDescription(&SDDS_table, &topline, NULL);
        if (!topline || !strlen(topline)) {
          sprintf(s, "Data from SDDS file %s, table %ld", inputfile, readstatus);
          SDDS_CopyString(&topline, s);
        }
      } else
        topline = users_topline;

      if (quantity) {
        if (data_value)
          SDDS_FreeMatrix((void **)data_value, nx);
        data_value = NULL;
        get_xyaxis_value(xaxisScalePar, xaxisOffsetPar, yaxisScalePar, yaxisOffsetPar,
                         &SDDS_table,
                         &xaxisScale, &xaxisOffset, &yaxisScale, &yaxisOffset, &users_xlabel, &users_ylabel);

        if (swap_xy) {
          SWAP_PTR(xlabel, ylabel);
          SWAP_DOUBLE(xmin, ymin);
          SWAP_DOUBLE(dx, dy);
          SWAP_LONG(nx, ny);
          if (!(data_value = SDDS_GetDoubleMatrixFromColumn(&SDDS_table,
                                                            quantity, nx, ny, SDDS_COLUMN_MAJOR_DATA))) {
            fputs("unable to get array from SDDS table\n", stderr);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        } else if (!(data_value = SDDS_GetDoubleMatrixFromColumn(&SDDS_table,
                                                                 quantity, nx, ny, SDDS_ROW_MAJOR_DATA))) {
          fputs("unable to get array from SDDS table\n", stderr);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }

        for (i = 0; i < conversions; i++) {
          if (ucd[i]->is_parameter == 0) {
            if (wild_match(quantity, ucd[i]->name)) {
              for (j = 0; j < nx; j++) {
                for (k = 0; k < ny; k++) {
                  data_value[j][k] = data_value[j][k] * ucd[i]->factor;
                }
              }
            }
          }
        }

      } /*end of if (quantity) */
      else if (columnmatch) {
        long ix, ixx, iy, flip, colID, minID, maxID, fail, logfail;
        double *indepdata;

        nx = SDDS_CountRowsOfInterest(&SDDS_table);
        flip = 0;
        if (!xRangeProvided) {
          SDDS_SetColumnFlags(&SDDS_table, 1);
          if (!(indepdata = SDDS_GetColumnInDoubles(&SDDS_table, indepcolumn)))
            bomb("unable to read independent variable data", NULL);
          if (indepdata[0] > indepdata[nx - 1])
            flip = 1;
          find_min_max(&xmin, &xmax, indepdata, nx);
          /* this is needed for uneven steps in the independent data */
          // if (do_shade && (nx_interp < 2) && (orig_limit[0]==orig_limit[1])) {
          if (do_shade && (nx_interp < 2)) {
            fail = 0;
            logfail = 0;
            for (ix = 0; ix < nx; ix++) {
              for (ixx = ix + 1; ixx < nx; ixx++) {
                if (indepdata[ix] == indepdata[ixx]) {
                  fail = 1;
                  break;
                }
              }
              if (xlog) {
                if (indepdata[ix] <= 0) {
                  logfail = 1;
                }
              }
            }
            if (fail == 0) {
              xintervals = malloc(sizeof(double) * nx);
              if (indepdata[nx - 1] < indepdata[0]) {
                for (ix = 0; ix < nx; ix++) {
                  xintervals[nx - 1 - ix] = indepdata[ix];
                }
              } else {
                for (ix = 0; ix < nx; ix++)
                  xintervals[ix] = indepdata[ix];
              }
              if ((xlog) && (logfail == 0)) {
                for (ix = 0; ix < nx; ix++)
                  xintervals[ix] = log10(xintervals[ix]);
                find_min_max(&xmin, &xmax, xintervals, nx);
              }
            } else {
              fprintf(stderr, "warning: Independent column data has duplicate values\n");
            }
          }
          /* done */
          free(indepdata);
        } else {
          if (xmaxPar && !SDDS_GetParameterAsDouble(&SDDS_table, xmaxPar, &xmax))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          if (xminPar && !SDDS_GetParameterAsDouble(&SDDS_table, xminPar, &xmin))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          if (xmax < xmin) {
            fprintf(stderr, "Invalid xrange values provided, xmax is less than xmin.\n");
            return 1;
          }
        }
        if (yRangeProvided) {
          if (ymaxPar && !SDDS_GetParameterAsDouble(&SDDS_table, ymaxPar, &ymax))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          if (yminPar && !SDDS_GetParameterAsDouble(&SDDS_table, yminPar, &ymin))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          if (ymax < ymin) {
            fprintf(stderr, "Invalid yrange values provided, ymax is less than ymin.\n");
            return 1;
          }
        }

        get_xyaxis_value(xaxisScalePar, xaxisOffsetPar, yaxisScalePar, yaxisOffsetPar, &SDDS_table,
                         &xaxisScale, &xaxisOffset, &yaxisScale, &yaxisOffset, &users_xlabel, &users_ylabel);

        if (verbosity > 1)
          printf("range of independent variable: %e to %e\n", xmin, xmax);

        SDDS_SetColumnFlags(&SDDS_table, 0);
        for (i = 0; i < columnmatches; i++) {
          if (!SDDS_SetColumnsOfInterest(&SDDS_table, SDDS_MATCH_STRING, columnmatch[i], SDDS_OR)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        }
        if (!columnname)
          columnname = SDDS_GetColumnNames(&SDDS_table, &columns);

        if (data_value)
          SDDS_FreeMatrix((void **)data_value, nx);
        data_value = SDDS_AllocateMatrix(sizeof(**data_value), nx, columns);
        ny = 0;
        minID = INT32_MAX;
        maxID = 0;

        for (iy = 0; iy < columns; iy++) {
          double *data;
          if (yaxisScaleProvided) {
            if (!get_long1(&colID, columnname[iy]))
              SDDS_Bomb("Unable to get the integer from column name.");

            if (yRangeProvided && (iy * yaxisScale < ymin || iy * yaxisScale > ymax))
              continue;
            if (colID < minID)
              minID = colID;
            if (maxID < colID)
              maxID = colID;
          }
          if (!(data = SDDS_GetColumnInDoubles(&SDDS_table, columnname[iy]))) {
            fprintf(stderr, "Unable to get column value %s\n", columnname[iy]);

            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (!flip)
            for (ix = 0; ix < nx; ix++)
              data_value[ix][ny] = data[ix];
          else
            for (ix = 0; ix < nx; ix++)
              data_value[nx - ix - 1][ny] = data[ix];
          ny++;
          free(data);
        }
        if (!ny)
          SDDS_Bomb("No data for plotting.");

        if (verbosity > 1)
          printf("%" PRId32 " rows and %" PRId32 " columns\n", nx, ny);
        dx = (xmax - xmin) / (nx - 1);
        if (yaxisScaleProvided) {
          ymin = (minID - (long)yaxisOffset) * yaxisScale;
          ymax = (maxID - (long)yaxisOffset) * yaxisScale;
          dy = (ymax - ymin) / (ny - 1);
          yRangeProvided = 1;
        } else {
          if (yRangeProvided) {
            dy = (ymax - ymin) / (ny - 1);
          } else {
            /* this is needed for uneven steps in the y axis */
            if (do_shade && (ny_interp < 2) && (orig_limit[2] == orig_limit[3])) {
              yintervals = malloc(sizeof(double) * ny);
              for (iy = 0; iy < columns; iy++) {
                if (!get_double1(&(yintervals[iy]), columnname[iy])) {
                  /* column name does not end in a number. Use column indexes instead */
                  free(yintervals);
                  yintervals = NULL;
                  break;
                }
                if (iy > 0) {
                  if (yintervals[iy] <= yintervals[iy - 1]) {
                    /* column name numbers are not incrementing as expected.  Use column indexes instead */
                    free(yintervals);
                    yintervals = NULL;
                    break;
                  }
                }
              }
              if (yintervals) {
                ymin = yintervals[0];
                ymax = yintervals[columns - 1];
                dy = (ymax - ymin) / (ny - 1);
              } else {
                ymin = 0;
                ymax = ny - 1;
                dy = 1;
              }
            } else {
              ymin = 0;
              ymax = ny - 1;
              dy = 1;
            }
          }
        }
        get_plot_labels(&SDDS_table, indepcolumn, columnname, columns,
                        allmatches, NULL, users_xlabel, users_ylabel, users_title,
                        &xlabel, &ylabel, &title, deltas, xRangeProvided, conversions, ucd);
        if (swap_xy) {
          SWAP_LONG(xRangeProvided, yRangeProvided);
        }
        if (!users_title)
          users_title = title;
        if (!swap_xy && !yRangeProvided)
          flags |= NO_YSCALES;
        else if (swap_xy && !xRangeProvided)
          flags |= NO_XSCALES;
        if (swap_xy) {
          double **new_data;
          new_data = (double **)zarray_2d(sizeof(double), ny, nx);
          for (i = 0; i < nx; i++) {
            for (j = 0; j < ny; j++)
              new_data[j][i] = data_value[i][j];
          }
          SDDS_FreeMatrix((void **)data_value, nx);
          data_value = new_data;
          SWAP_PTR(xlabel, ylabel);
          SWAP_DOUBLE(xmin, ymin);
          SWAP_DOUBLE(dx, dy);
          SWAP_DOUBLE(xmax, ymax);
          SWAP_LONG(nx, ny);
        }
        if (process_data_values(&data_value, nx, ny, deltas))
          return 1;
      } else if (xyzArray[0]) {
        char *columnunits;
        long ix, iy, type[3];
        SDDS_ARRAY *xyzData[3];
        for (i = 0; i < 3; i++) {
          if (xyzArray[i]) {
            SDDS_GetArrayInformation(&SDDS_table, "type", &type[i], SDDS_GET_BY_NAME,
                                     xyzArray[i]);
            if (!(xyzData[i] = SDDS_GetArray(&SDDS_table, xyzArray[i], NULL))) {
              fprintf(stderr, "unable to read %s array\n", xyzArray[i]);
              return (1);
            }
          }
        }
        if (xyzData[0]->definition->dimensions != 2) {
          fprintf(stderr, "array %s must be 2 dimensions\n", xyzArray[0]);
          return (1);
        }
        if (xyzArray[1]) {
          if (xyzData[1]->definition->dimensions != 1) {
            fprintf(stderr, "array %s must be 1 dimension\n", xyzArray[1]);
            return (1);
          }
          if (xyzData[1]->dimension[0] != xyzData[0]->dimension[0]) {
            fprintf(stderr, "array dimension mismatch between %s and %s\n", xyzArray[0], xyzArray[1]);
            return (1);
          }
        }
        if (xyzArray[2]) {
          if (xyzData[2]->definition->dimensions != 1) {
            fprintf(stderr, "array %s must be 1 dimension\n", xyzArray[2]);
            return (1);
          }
          if (xyzData[2]->dimension[0] != xyzData[0]->dimension[1]) {
            fprintf(stderr, "array dimension mismatch between %s and %s\n", xyzArray[0], xyzArray[2]);
            return (1);
          }
        }
        if (swap_array) {
          nx = xyzData[0]->dimension[1];
          ny = xyzData[0]->dimension[0];
        } else {
          nx = xyzData[0]->dimension[0];
          ny = xyzData[0]->dimension[1];
        }
        if (data_value)
          SDDS_FreeMatrix((void **)data_value, nx);
        data_value = SDDS_AllocateMatrix(sizeof(**data_value), nx, ny);
        switch (type[0]) {
        case SDDS_SHORT:
          for (ix = 0; ix < nx; ix++)
            for (iy = 0; iy < ny; iy++) {
              data_value[ix][iy] = (double)(((short *)xyzData[0]->data)[ix * ny + iy]);
            }
          break;
        case SDDS_USHORT:
          for (ix = 0; ix < nx; ix++)
            for (iy = 0; iy < ny; iy++) {
              data_value[ix][iy] = (double)(((unsigned short *)xyzData[0]->data)[ix * ny + iy]);
            }
          break;
        case SDDS_LONG:
          for (ix = 0; ix < nx; ix++)
            for (iy = 0; iy < ny; iy++) {
              data_value[ix][iy] = (double)(((int32_t *)xyzData[0]->data)[ix * ny + iy]);
            }
          break;
        case SDDS_ULONG:
          for (ix = 0; ix < nx; ix++)
            for (iy = 0; iy < ny; iy++) {
              data_value[ix][iy] = (double)(((uint32_t *)xyzData[0]->data)[ix * ny + iy]);
            }
          break;
        case SDDS_LONG64:
          for (ix = 0; ix < nx; ix++)
            for (iy = 0; iy < ny; iy++) {
              data_value[ix][iy] = (double)(((int64_t *)xyzData[0]->data)[ix * ny + iy]);
            }
          break;
        case SDDS_ULONG64:
          for (ix = 0; ix < nx; ix++)
            for (iy = 0; iy < ny; iy++) {
              data_value[ix][iy] = (double)(((uint64_t *)xyzData[0]->data)[ix * ny + iy]);
            }
          break;
        case SDDS_FLOAT:
          for (ix = 0; ix < nx; ix++)
            for (iy = 0; iy < ny; iy++) {
              data_value[ix][iy] = (double)(((float *)xyzData[0]->data)[ix * ny + iy]);
            }
          break;
        case SDDS_DOUBLE:
          for (ix = 0; ix < nx; ix++)
            for (iy = 0; iy < ny; iy++) {
              data_value[ix][iy] = ((double *)xyzData[0]->data)[ix * ny + iy];
            }
          break;
        }

        xintervals = malloc(sizeof(double) * nx);
        yintervals = malloc(sizeof(double) * ny);
        if (verbosity > 1)
          printf("%" PRId32 " rows and %" PRId32 " columns\n", nx, ny);

        if (swap_array) {
          j = 2;
        } else {
          j = 1;
        }
        if (xyzArray[1]) {
          switch (type[1]) {
          case SDDS_SHORT:
            for (i = 0; i < nx; i++)
              xintervals[i] = (double)(((short *)xyzData[j]->data)[i]);
            break;
          case SDDS_USHORT:
            for (i = 0; i < nx; i++)
              xintervals[i] = (double)(((unsigned short *)xyzData[j]->data)[i]);
            break;
          case SDDS_LONG:
            for (i = 0; i < nx; i++)
              xintervals[i] = (double)(((int32_t *)xyzData[j]->data)[i]);
            break;
          case SDDS_ULONG:
            for (i = 0; i < nx; i++)
              xintervals[i] = (double)(((uint32_t *)xyzData[j]->data)[i]);
            break;
          case SDDS_LONG64:
            for (i = 0; i < nx; i++)
              xintervals[i] = (double)(((int64_t *)xyzData[j]->data)[i]);
            break;
          case SDDS_ULONG64:
            for (i = 0; i < nx; i++)
              xintervals[i] = (double)(((uint64_t *)xyzData[j]->data)[i]);
            break;
          case SDDS_FLOAT:
            for (i = 0; i < nx; i++)
              xintervals[i] = (double)(((float *)xyzData[j]->data)[i]);
            break;
          case SDDS_DOUBLE:
            for (i = 0; i < nx; i++)
              xintervals[i] = ((double *)xyzData[j]->data)[i];
            break;
          }
        } else {
          for (i = 0; i < nx; i++)
            xintervals[i] = i;
        }

        if (swap_array) {
          j = 1;
        } else {
          j = 2;
        }
        if (xyzArray[2]) {
          switch (type[2]) {
          case SDDS_SHORT:
            for (i = 0; i < ny; i++)
              yintervals[i] = (double)(((short *)xyzData[j]->data)[i]);
            break;
          case SDDS_USHORT:
            for (i = 0; i < ny; i++)
              yintervals[i] = (double)(((unsigned short *)xyzData[j]->data)[i]);
            break;
          case SDDS_LONG:
            for (i = 0; i < ny; i++)
              yintervals[i] = (double)(((int32_t *)xyzData[j]->data)[i]);
            break;
          case SDDS_ULONG:
            for (i = 0; i < ny; i++)
              yintervals[i] = (double)(((uint32_t *)xyzData[j]->data)[i]);
            break;
          case SDDS_LONG64:
            for (i = 0; i < ny; i++)
              yintervals[i] = (double)(((int64_t *)xyzData[j]->data)[i]);
            break;
          case SDDS_ULONG64:
            for (i = 0; i < ny; i++)
              yintervals[i] = (double)(((uint64_t *)xyzData[j]->data)[i]);
            break;
          case SDDS_FLOAT:
            for (i = 0; i < ny; i++)
              yintervals[i] = (double)(((float *)xyzData[j]->data)[i]);
            break;
          case SDDS_DOUBLE:
            for (i = 0; i < ny; i++)
              yintervals[i] = ((double *)xyzData[j]->data)[i];
            break;
          }
        } else {
          for (i = 0; i < ny; i++)
            yintervals[i] = i;
        }

        find_min_max(&xmin, &xmax, xintervals, nx);
        find_min_max(&ymin, &ymax, yintervals, ny);
        if (xmin == xmax) {
          fprintf(stderr, "Error: invalid data range for the x-axis\n");
          return (1);
        }
        if (ymin == ymax) {
          fprintf(stderr, "Error: invalid data range for the y-axis\n");
          return (1);
        }

        dx = (xmax - xmin) / (nx - 1);
        dy = (ymax - ymin) / (ny - 1);

        if (!do_shade) {
          fprintf(stderr, "warning: dx and dy are static in a contour plot\n");
        }

        if (xyzArray[1]) {
          if (!users_xlabel) {
            SDDS_GetArrayInformation(&SDDS_table, "units", &xlabel, SDDS_GET_BY_NAME, xyzArray[1]);
            if (xlabel && xlabel[0])
              sprintf(s, "%s (%s)", xyzArray[1], xlabel);
            else
              sprintf(s, "%s", xyzArray[1]);
            free(xlabel);
            SDDS_CopyString(&xlabel, s);
          } else
            xlabel = users_xlabel;
        } else {
          if (!users_xlabel) {
            sprintf(s, "x");
            SDDS_CopyString(&xlabel, s);
          } else
            xlabel = users_xlabel;
        }

        if (xyzArray[2]) {
          if (!users_ylabel) {
            SDDS_GetArrayInformation(&SDDS_table, "units", &ylabel, SDDS_GET_BY_NAME, xyzArray[2]);
            if (ylabel && ylabel[0])
              sprintf(s, "%s (%s)", xyzArray[2], ylabel);
            else
              sprintf(s, "%s", xyzArray[2]);
            free(ylabel);
            SDDS_CopyString(&ylabel, s);
          } else
            ylabel = users_ylabel;
        } else {
          if (!users_ylabel) {
            sprintf(s, "y");
            SDDS_CopyString(&ylabel, s);
          } else
            ylabel = users_ylabel;
        }

        if (!users_title) {
          columnunits = NULL;
          SDDS_GetArrayInformation(&SDDS_table, "units", &columnunits, SDDS_GET_BY_NAME, xyzArray[0]);
          if (columnunits && columnunits[0])
            sprintf(s, "%s (%s)", xyzArray[0], columnunits);
          else
            sprintf(s, "%s", xyzArray[0]);
          if (columnunits)
            free(columnunits);
          SDDS_CopyString(&title, s);
          users_title = title;
        } else
          title = users_title;

        if (SDDS_NumberOfErrors())
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        for (i = 0; i < 3; i++) {
          if (xyzArray[i]) {
            SDDS_FreeArray(xyzData[i]);
          }
        }
      } else if (xyzColumn[2]) {
        double *xyzData[3];
        int32_t rows, nxx, nyy;
        char *columnunits;
        long ix, iy, ii;
        nx = ny = rows = SDDS_RowCount(&SDDS_table);
        for (i = 0; i < 3; i++) {
          /*Add xrange and yrange support*/
          if (!(xyzData[i] = SDDS_GetColumnInDoubles(&SDDS_table, xyzColumn[i]))) {
            fprintf(stderr, "Error: unable to read %s column\n", xyzColumn[i]);
            return (1);
          }
          for (ii = 0; ii < conversions; ii++) {
            if (ucd[ii]->is_parameter == 0) {
              if (wild_match(xyzColumn[i], ucd[ii]->name)) {
                for (j = 0; j < rows; j++) {
                  xyzData[i][j] = xyzData[i][j] * ucd[ii]->factor;
                }
              }
            }
          }
        }
        qsort((void *)(xyzData[0]), rows, sizeof(*(xyzData[0])), double_cmpasc);
        qsort((void *)(xyzData[1]), rows, sizeof(*(xyzData[1])), double_cmpasc);

        for (i = 0; i < rows - 1; i++) {
          if (xyzData[0][i] == xyzData[0][i + 1]) {
            nx--;
          }
        }
        for (i = 0; i < rows - 1; i++) {
          if (xyzData[1][i] == xyzData[1][i + 1]) {
            ny--;
          }
        }
        if (nx * ny != rows) {
          fprintf(stderr, "Error: x and y data does not appear to form a grid (nx=%" PRId32 ", ny=%" PRId32 ", rows=%" PRId32 ")\n",
                  nx, ny, rows);
          return (1);
        }
        nxx = 1;
        for (i = 0; i < rows - 1; i++) {
          if (xyzData[0][i] == xyzData[0][i + 1]) {
            nxx++;
          } else {
            if (nxx != ny) {
              fprintf(stderr, "Error: x and y data does not appear to form a grid (row=%ld, nx=%" PRId32 ", nxx=%" PRId32 ", ny=%" PRId32 ")\n",
                      i, nx, nxx, ny);
              return (1);
            }
            nxx = 1;
          }
        }
        nyy = 1;
        for (i = 0; i < rows - 1; i++) {
          if (xyzData[1][i] == xyzData[1][i + 1]) {
            nyy++;
          } else {
            if (nyy != nx) {
              fprintf(stderr, "Error: x and y data does not appear to form a grid (ny=%" PRId32 ", nyy=%" PRId32 ", nx=%" PRId32 ")\n",
                      ny, nyy, nx);
              return (1);
            }
            nyy = 1;
          }
        }
        xmin = xyzData[0][0];
        xmax = xyzData[0][rows - 1];
        ymin = xyzData[1][0];
        ymax = xyzData[1][rows - 1];
        if (xmin == xmax) {
          fprintf(stderr, "Error: invalid data range for the x-axis\n");
          return (1);
        }
        if (ymin == ymax) {
          fprintf(stderr, "Error: invalid data range for the y-axis\n");
          return (1);
        }
        dx = (xmax - xmin) / (nx - 1);
        dy = (ymax - ymin) / (ny - 1);
        xintervals = malloc(sizeof(double) * nx);
        yintervals = malloc(sizeof(double) * ny);
        for (i = 0; i < nx; i++)
          xintervals[i] = xyzData[0][i * ny];
        for (i = 0; i < ny; i++)
          yintervals[i] = xyzData[1][i * nx];

        for (i = 0; i < 2; i++) {
          free(xyzData[i]);
          if (!(xyzData[i] = SDDS_GetColumnInDoubles(&SDDS_table, xyzColumn[i]))) {
            fprintf(stderr, "Error: unable to read %s column\n", xyzColumn[i]);
            return (1);
          }
          for (ii = 0; ii < conversions; ii++) {
            if (ucd[ii]->is_parameter == 0) {
              if (wild_match(xyzColumn[i], ucd[ii]->name)) {
                for (j = 0; j < rows; j++) {
                  xyzData[i][j] = xyzData[i][j] * ucd[ii]->factor;
                }
              }
            }
          }
        }
        get_xyaxis_value(xaxisScalePar, xaxisOffsetPar, yaxisScalePar, yaxisOffsetPar, &SDDS_table,
                         &xaxisScale, &xaxisOffset, &yaxisScale, &yaxisOffset, &users_xlabel, &users_ylabel);

        if (data_value)
          SDDS_FreeMatrix((void **)data_value, nx);
        data_value = SDDS_AllocateMatrix(sizeof(**data_value), nx, ny);

        for (i = 0; i < rows; i++) {
          ix = 0;
          while (xyzData[0][i] != xintervals[ix]) {
            ix++;
          }
          iy = 0;
          while (xyzData[1][i] != yintervals[iy]) {
            iy++;
          }
          data_value[ix][iy] = xyzData[2][i];
        }

        if (xaxisScaleProvided) {
          for (ix = 0; ix < nx; ix++)
            xintervals[ix] = (xintervals[ix] - xaxisOffset) * xaxisScale;
        }
        if (yaxisScaleProvided) {
          for (iy = 0; iy < ny; iy++)
            yintervals[iy] = (yintervals[iy] - yaxisOffset) * yaxisScale;
        }
        if (!do_shade) {
          fprintf(stderr, "warning: dx and dy are static in a contour plot\n");
        }

        if (!users_xlabel) {
          SDDS_GetColumnInformation(&SDDS_table, "units", &xlabel, SDDS_GET_BY_NAME, xyzColumn[0]);
          for (i = 0; i < conversions; i++) {
            if (ucd[i]->is_parameter == 0) {
              if (wild_match(xyzColumn[0], ucd[i]->name)) {
                SDDS_CopyString(&xlabel, ucd[i]->new_units);
              }
            }
          }
          if (xlabel && xlabel[0])
            sprintf(s, "%s (%s)", xyzColumn[0], xlabel);
          else
            sprintf(s, "%s", xyzColumn[0]);
          free(xlabel);
          SDDS_CopyString(&xlabel, s);
        } else
          xlabel = users_xlabel;

        if (!users_ylabel) {
          SDDS_GetColumnInformation(&SDDS_table, "units", &ylabel, SDDS_GET_BY_NAME, xyzColumn[1]);
          for (i = 0; i < conversions; i++) {
            if (ucd[i]->is_parameter == 0) {
              if (wild_match(xyzColumn[1], ucd[i]->name)) {
                SDDS_CopyString(&ylabel, ucd[i]->new_units);
              }
            }
          }
          if (ylabel && ylabel[0])
            sprintf(s, "%s (%s)", xyzColumn[1], ylabel);
          else
            sprintf(s, "%s", xyzColumn[1]);
          free(ylabel);
          SDDS_CopyString(&ylabel, s);
        } else
          ylabel = users_ylabel;

        if (!users_title) {
          columnunits = NULL;
          SDDS_GetColumnInformation(&SDDS_table, "units", &columnunits, SDDS_GET_BY_NAME, xyzColumn[2]);
          for (i = 0; i < conversions; i++) {
            if (ucd[i]->is_parameter == 0) {
              if (wild_match(xyzColumn[2], ucd[i]->name)) {
                SDDS_CopyString(&columnunits, ucd[i]->new_units);
              }
            }
          }
          if (columnunits && columnunits[0])
            sprintf(s, "%s (%s)", xyzColumn[2], columnunits);
          else
            sprintf(s, "%s", xyzColumn[2]);
          if (columnunits)
            free(columnunits);
          SDDS_CopyString(&title, s);
          users_title = title;
        } else
          title = users_title;

        if (SDDS_NumberOfErrors())
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (swap_xy) {
          SWAP_PTR(xlabel, ylabel);
          SWAP_DOUBLE(xmin, ymin);
          SWAP_DOUBLE(dx, dy);
          SWAP_LONG(nx, ny);
        }
        data_value = SDDS_AllocateMatrix(sizeof(double), nx, ny);
      }

      if (xlabel[0] == '@')
        xlabel = getParameterLabel(&SDDS_table, xlabel + 1, xlabel_editcommand, NULL);

      if (ylabel[0] == '@')
        ylabel = getParameterLabel(&SDDS_table, ylabel + 1, ylabel_editcommand, NULL);

      if (title[0] == '@')
        title = getParameterLabel(&SDDS_table, title + 1, title_editcommand, NULL);
      if (topline[0] == '@')
        topline = getParameterLabel(&SDDS_table, topline + 1, topline_editcommand, topline_formatcommand);
      if (rpn_equation) {
        if (!swap_xy) {
          mem1 = rpn_create_mem(variable1 ? variable1 : "row", 0);
          mem2 = rpn_create_mem(variable2 ? variable2 : "column", 0);
        } else {
          mem2 = rpn_create_mem(variable1 ? variable1 : "row", 0);
          mem1 = rpn_create_mem(variable2 ? variable2 : "column", 0);
        }
        if (!SDDS_StoreParametersInRpnMemories(&SDDS_table)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        if (xyzArray[0]) {
          fprintf(stderr, "warning: rpn equations calculated using static dx and dy\n");
        }
        for (i = 0; i < nx; i++) {
          rpn_store(i * dx + xmin, NULL, mem1);
          for (j = 0; j < ny; j++) {
            rpn_store(j * dy + ymin, NULL, mem2);
            if (!SDDS_StoreRowInRpnMemories(&SDDS_table, swap_xy ? i + j * nx : i * ny + j)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (1);
            }
            data_value[i][j] = rpn(equdf_name);
            if (rpn_check_error())
              return (1);
            rpn_clear();
          }
        }
      }

      if (rpn_transform) {
        if (!swap_xy) {
          mem1 = rpn_create_mem(variable1 ? variable1 : "row", 0);
          mem2 = rpn_create_mem(variable2 ? variable2 : "column", 0);
        } else {
          mem2 = rpn_create_mem(variable1 ? variable1 : "row", 0);
          mem1 = rpn_create_mem(variable2 ? variable2 : "column", 0);
        }
        if (!SDDS_StoreParametersInRpnMemories(&SDDS_table)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        if (xyzArray[0]) {
          fprintf(stderr, "warning: rpn transform calculated using static dx and dy\n");
        }
        for (i = 0; i < nx; i++) {
          rpn_store(i * dx + xmin, NULL, mem1);
          for (j = 0; j < ny; j++) {
            rpn_store(j * dy + ymin, NULL, mem2);
            if (!SDDS_StoreRowInRpnMemories(&SDDS_table, swap_xy ? i + j * nx : i * ny + j)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (1);
            }
            /*push_num(data_value[i][j]);*/
            data_value[i][j] = rpn(trudf_name);
            if (rpn_check_error())
              return (1);
            rpn_clear();
          }
        }
      }
      xmin0 = xmin;
      xmax0 = xmax;
      ymax0 = ymax;
      ymin0 = ymin;
      /*the xmin, xmax, ymin, ymax will be changed after processing data, needs remind their values for next plot */
      process_data(&data_value, &nx, &ny, &xmin, &xmax, &ymin, &ymax, &dx, &dy,
                   orig_limit, logscale, logfloor, nx_interp, ny_interp, x_lowpass,
                   y_lowpass, interp_flags, xyzArray, xyzColumn, verbosity, xlog, &nx_offset, swap_xy);
      if (orig_limit[0] != orig_limit[1] || orig_limit[2] != orig_limit[3]) {
        // This may cause problems in the future.
        // Check example from doolings email 5/21/2019 to ensure any changes here don't mess up that example.
        // Also check Dooing email from 8/17/2020
        // Also check Emery email from 12/10/2020
        if (quantity)
          nx_offset = 0;
      }

      if (yaxisScaleProvided) {
        ymin = (ymin - yaxisOffset) * yaxisScale;
        ymax = (ymax - yaxisOffset) * yaxisScale;
        dy = (ymax - ymin) / (ny - 1);
        yRangeProvided = 1;
      }
      if (xaxisScaleProvided) {
        xmin = (xmin - xaxisOffset) * xaxisScale;
        xmax = (xmax - xaxisOffset) * xaxisScale;
        dx = (xmax - xmin) / (nx - 1);
        xRangeProvided = 1;
      }
      if (!plot_contour(data_value, nx, ny, verbosity,
                        xmin, xmax, ymin, ymax,
                        dx, dy, xintervals, yintervals,
                        device, &frameEnded, title, xlabel,
                        ylabel, topline, min_level, max_level, levelLimit,
                        levels, levelLists, levelList, hue0, hue1, layout,
                        &ixl, &iyl, thickness, tsetFlags,
                        shape, shapes, pen, &flags,
                        pause_interval, columnmatches, columnname, columns,
                        yEditCommand, ySparseInterval, yScale, contour_label_interval,
                        contour_label_offset, do_shade, 0, colorName, colorUnits, swap_xy, xlabelScale, ylabelScale, yRangeProvided, xRangeProvided,
                        drawLineSpec, drawlines, fill_screen, nx_interp, ny_interp, orig_limit, xlog, nx_offset, show_gaps))
        continue;
      /*restore the values after swap since they will be used for next plot */

      xmin = xmin0;
      xmax = xmax0;
      ymin = ymin0;
      ymax = ymax0;
      if (swap_xy && columnmatch) {
        SWAP_PTR(xlabel, ylabel);
        SWAP_DOUBLE(xmin, ymin);
        SWAP_DOUBLE(dx, dy);
        SWAP_DOUBLE(xmax, ymax);
        SWAP_LONG(xRangeProvided, yRangeProvided);
        SWAP_DOUBLE(xlabelScale, ylabelScale);
      }
      if (!layout[0] || !layout[1]) {
        if (!frameEnded) {
          frame_end(1);
          frameEnded = 1;
        }
      }
      freeParameterLabel(users_xlabel, xlabel);
      freeParameterLabel(users_ylabel, ylabel);
      freeParameterLabel(users_title, title);
      freeParameterLabel(users_topline, topline);
      if (data_value)
        SDDS_FreeMatrix((void **)data_value, nx);
      data_value = NULL;
    }
    if (buffer)
      free(buffer);
    if (!SDDS_Terminate(&SDDS_table)) {
      SDDS_SetError("problem closing file");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }

    if (SDDS_NumberOfErrors()) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
  }
  if (!frameEnded)
    frame_end(1);
  if (columnname) {
    for (i = 0; i < columns; i++) {
      free(columnname[i]);
    }
    free(columnname);
  }
  if (drawlines)
    free(drawLineSpec);
  free_scanargs(&s_arg, argc);
  return (0);
}

/* plotting of contours: */
long plot_contour(double **data_value, long nx, long ny, long verbosity,
                  double xmin, double xmax, double ymin, double ymax,
                  double dx, double dy, double *xintervals, double *yintervals,
                  char *device, long *frameEnded, char *title, char *xlabel,
                  char *ylabel, char *topline, double min_level, double max_level,
                  double levelLimit[2],
                  long levels, long levelLists, double *levelList, double hue0, double hue1, long *layout,
                  long *ixl, long *iyl, long thickness, unsigned long long tsetFlags,
                  SHAPE_DATA *shape, long shapes, int *pen, long *flags,
                  long pause_interval, long columnmatches, char **columnname, long columns,
                  char *yEditCommand, long ySparseInterval, double yScale, long contour_label_interval,
                  long contour_label_offset, long do_shade, long waterfall, char *colorName,
                  char *colorUnits, long swap_xy, double xlabelScale, double ylabelScale,
                  long yRangeProvided, long xRangeProvided,
                  DRAW_LINE_SPEC *drawLineSpec, long drawlines, long fill_screen,
                  long nx_interp, long ny_interp, double *orig_limit, short xlog, long nx_offset, short show_gaps) {
  long i, j, ix_min = 0, ix_max = 0, iy_min = 0, iy_max = 0, gray = 0;
  double max_value, min_value, *level, limit[4];
  register double value;

  level = NULL;
  max_value = -DBL_MAX;
  min_value = DBL_MAX;
  for (i = nx - 1; i >= 0; i--) {
    for (j = ny - 1; j >= 0; j--) {
      value = data_value[i][j];
      if (min_value > value && (value >= levelLimit[0])) {
        ix_min = i;
        iy_min = j;
        min_value = value;
      }
      if (max_value < value && (value <= levelLimit[1])) {
        ix_max = i;
        iy_max = j;
        max_value = value;
      }
    }
  }
#ifndef COMPILE_AS_SUBROUTINE
  if (verbosity > 1) {
    printf("maximum value is %e, at x=%e, y=%e\n",
           max_value, ix_max * dx + xmin, iy_max * dy + ymin);
    printf("minimum value is %e, at x=%e, y=%e\n",
           min_value, ix_min * dx + xmin, iy_min * dy + ymin);
    if (strncmp(device, "regis", strlen(device)) == 0) {
      fputs("hit return to continue.", stdout);
      getchar();
    }
  }
#endif
  if (!xintervals) {
    xintervals = malloc(sizeof(double) * nx);
    for (i = 0; i < nx; i++) {
      xintervals[i] = xmin + dx * i;
    }
  }
  if (!yintervals) {
    yintervals = malloc(sizeof(double) * ny);
    for (i = 0; i < ny; i++) {
      yintervals[i] = ymin + dy * i;
    }
  }
  if (threeD) {
    plot3DSurface(data_value, nx, ny, xmin, xmax, ymin, ymax);
    if (xintervals)
      free(xintervals);
    if (yintervals)
      free(yintervals);
    return 1;
  }
  set_mapping(0.0, 0.0, 0.0, 0.0);
  *frameEnded = 0;
  if (min_level == max_level) {
    min_level = min_value;
    max_level = max_value;
  }
  if (do_shade) {
    if (do_shade == 2)
      gray = 1;
    go_shade_grid(device, title, xlabel, ylabel, topline,
                  data_value, xmin, xmax, ymin, ymax, xintervals, yintervals, nx, ny,
                  min_level, max_level, levels ? levels : 100,
                  hue0, hue1, layout, *ixl, *iyl,
                  NULL, pen, *flags, pause_interval,
                  thickness, tsetFlags, colorName, colorUnits, xlabelScale, ylabelScale, gray,
                  fill_screen, xlog, nx_offset, show_gaps);
    *flags |= DEVICE_DEFINED;
    PlotShapesData(shape, shapes, xmin, xmax, ymin, ymax);
  }
  if (!do_shade || contour_label_interval) {
    if (levelLists) {
      levels = levelLists;
      level = levelList;
    } else {
      if (!(level = fill_levels(&level,
                                min_level,
                                max_level,
                                (levels ? levels : 10) + (do_shade ? 1 : 0))))
        return 0;
    }
    go_plot_contours(device, title, xlabel, ylabel, topline,
                     data_value, xmin, xmax, ymin, ymax,
                     dx, dy, nx, ny, level, levels ? levels : 10,
                     contour_label_interval, contour_label_offset,
                     layout, *ixl, *iyl, NULL, pen,
                     *flags, pause_interval,
                     shape, shapes, tsetFlags, xlabelScale, ylabelScale, do_shade, thickness, fill_screen);
    *flags |= DEVICE_DEFINED;
  }
  if (columnmatches && !(*flags & NO_SCALES)) {
    if (!swap_xy && !yRangeProvided) {
      if ((ny_interp < 2) && (orig_limit[2] == orig_limit[3])) {
        make_enumerated_yscale(columnname, yintervals, columns, yEditCommand, ySparseInterval, yScale, thickness, ylabel, ylabelScale);
      } else {
        make_enumerated_yscale(columnname, NULL, columns, yEditCommand, ySparseInterval, yScale, thickness, ylabel, ylabelScale);
      }
    } else if (swap_xy && !xRangeProvided)
      make_enumerated_xscale(columnname, NULL, columns, yEditCommand, ySparseInterval, yScale, thickness, xlabel, xlabelScale);
  }
  limit[0] = xmin;
  limit[1] = xmax;
  limit[2] = ymin;
  limit[3] = ymax;
  if (drawlines)
    draw_lines(drawLineSpec, drawlines, 0, limit);

  if (xintervals)
    free(xintervals);
  if (yintervals)
    free(yintervals);
  xintervals = NULL;
  yintervals = NULL;

  if (layout[0] && layout[1]) {
    if (++(*ixl) == layout[0]) {
      *ixl = 0;
      if (++(*iyl) == layout[1]) {
        *iyl = 0;
        frame_end(1);
        *frameEnded = 1;
      }
    }
  }
  return 1;
}

double *fill_levels(double **level, double min, double max, long levels) {
  long i;
  double delta;
  if (levels <= 0)
    return (NULL);
  *level = trealloc(*level, sizeof(**level) * levels);
  if (levels > 1)
    delta = (max - min) / (levels - 1);
  else
    delta = 0;
  for (i = 0; i < levels; i++)
    (*level)[i] = min + i * delta;
  return (*level);
}

void getDimensionParameters(SDDS_TABLE *SDDS_table, char *name_of_name, char **variable,
                            char **variableUnits,
                            double *minimum, double *interval, int32_t *number) {
  static char s[SDDS_MAXLINE], message[SDDS_MAXLINE];
  long index;
  char **ptr, *units;
  double dValue;

  if (!name_of_name)
    bomb("improper calling of get_dimension_parameters", NULL);
  if (!(ptr = SDDS_GetParameter(SDDS_table, name_of_name, NULL))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Bomb("Unable to get dimension parameters");
  }
  if (*variable)
    free(*variable);
  *variable = *ptr;
  sprintf(s, "%sInterval", *variable);
  if ((SDDS_GetParameterIndex(SDDS_table, s)) < 0 ||
      !SDDS_FLOATING_TYPE(SDDS_GetNamedParameterType(SDDS_table, s))) {
    snprintf(message, sizeof(message), "Problem with parameter %.955s---check existence.  Should be floating type", s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Bomb(message);
  }

  sprintf(s, "%sDimension", *variable);
  if ((SDDS_GetParameterIndex(SDDS_table, s)) < 0 ||
      !SDDS_INTEGER_TYPE(SDDS_GetNamedParameterType(SDDS_table, s))) {
    snprintf(message, sizeof(message), "Problem with parameter %.955s---check existence.  Should be integer type", s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Bomb(message);
  }

  sprintf(s, "%sMinimum", *variable);
  if (((index = SDDS_GetParameterIndex(SDDS_table, s))) < 0 ||
      !SDDS_FLOATING_TYPE(SDDS_GetNamedParameterType(SDDS_table, s))) {
    snprintf(message, sizeof(message), "Problem with parameter %.955s---check existence.  Should be floating type", s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Bomb(message);
  }
  if (*variableUnits)
    free(*variableUnits);
  if (SDDS_GetParameterInformation(SDDS_table, "units", &units, SDDS_GET_BY_INDEX, index) !=
      SDDS_STRING)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!units || !strlen(units)) {
    sprintf(s, "%sUnits", *variable);
    if (SDDS_GetParameterIndex(SDDS_table, s) >= 0 && !SDDS_GetParameter(SDDS_table, s, &units))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  *variableUnits = units;

  sprintf(s, "%sMinimum", *variable);
  if (!(SDDS_GetParameterAsDouble(SDDS_table, s, minimum))) {
    fprintf(stderr, "error: problem finding/casting %s\n", s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  sprintf(s, "%sInterval", *variable);
  if (!(SDDS_GetParameterAsDouble(SDDS_table, s, interval))) {
    fprintf(stderr, "error: problem finding/casting %s\n", s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  sprintf(s, "%sDimension", *variable);
  if (!(SDDS_GetParameterAsDouble(SDDS_table, s, &dValue))) {
    fprintf(stderr, "error: problem finding %s or with it's type\n", s);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  *number = dValue;
}

double **window_2d_array(double **data_value, double *xmin, double *xmax, double *ymin, double *ymax,
                         double dx, double dy, int32_t *nx, int32_t *ny, double *limit, short xlog, int32_t *nx_offset, long swap_xy) {
  double **new_data;
  long new_nx, new_ny, ix_min, iy_min, ix, iy;

  new_nx = *nx;
  new_ny = *ny;
  ix_min = 0;
  iy_min = 0;

  if (xlog == 0) {
    if (dx && limit[0] != limit[1]) {
      if (limit[0] < *xmin)
        limit[0] = *xmin;
      else
        limit[0] = (ix_min = (long)((limit[0] - *xmin) / dx)) * dx + *xmin;
      if (limit[1] > *xmax) {
        limit[1] = *xmax;
        new_nx -= ix_min;
      } else {
        new_nx = (limit[1] - limit[0]) / dx + 1.5;
        limit[1] = (new_nx - 1) * dx + limit[0];
      }
      if (limit[1] <= limit[0])
        bomb("horizontal scales are invalid", NULL);
      *xmin = limit[0];
      *xmax = limit[1];
    }
  } else {
    double xmin2, xmax2, dx2;
    xmin2 = pow(10, *xmin);
    xmax2 = pow(10, *xmax);
    dx2 = (xmax2 - xmin2) / (*nx - 1);
    if (dx2 && limit[0] != limit[1]) {
      if (limit[0] < xmin2)
        limit[0] = xmin2;
      else
        limit[0] = (ix_min = (long)((limit[0] - xmin2) / dx2)) * dx2 + xmin2;
      if (limit[1] > xmax2) {
        limit[1] = xmax2;
        new_nx -= ix_min;
      } else {
        new_nx = (limit[1] - limit[0]) / dx2 + 1.5;
        limit[1] = (new_nx - 1) * dx2 + limit[0];
      }
      if (limit[1] <= limit[0])
        bomb("horizontal scales are invalid", NULL);
      *xmin = log10(limit[0]);
      *xmax = log10(limit[1]);
    }
  }
  if (dy && limit[2] != limit[3]) {
    if (limit[2] < *ymin)
      limit[2] = *ymin;
    else
      limit[2] = (iy_min = (long)((limit[2] - *ymin) / dy)) * dy + *ymin;
    if (limit[3] > *ymax) {
      limit[3] = *ymax;
      new_ny -= iy_min;
    } else {
      new_ny = (limit[3] - limit[2]) / dy + 1.5;
      limit[3] = (new_ny - 1) * dy + limit[2];
    }
    if (limit[3] <= limit[2])
      bomb("vertical scales are invalid", NULL);
    *ymin = limit[2];
    *ymax = limit[3];
  }
  if (dx == 0 && dy == 0) {
    return data_value;
  } else {
    new_data = (double **)zarray_2d(sizeof(double), new_nx, new_ny);
    for (ix = ix_min; ix < new_nx + ix_min; ix++)
      for (iy = iy_min; iy < new_ny + iy_min; iy++)
        new_data[ix - ix_min][iy - iy_min] = data_value[ix][iy];
    free_zarray_2d((void **)data_value, *nx, *ny);
    *nx = new_nx;
    *ny = new_ny;
    if (swap_xy) {
      *nx_offset = iy_min;
    } else {
      *nx_offset = ix_min;
    }
    return (new_data);
  }
}

void checkParameter(SDDS_TABLE *SDDS_table, char *parameter_name) {
  long index;
  if (!parameter_name) {
    fprintf(stderr, "parameter name is null\n");
    exit(1);
  }
  if ((index = SDDS_GetParameterIndex(SDDS_table, parameter_name)) < 0) {
    fprintf(stderr, "error: unable to make labels from parameter %s\n",
            parameter_name);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (SDDS_GetParameterType(SDDS_table, index) != SDDS_STRING) {
    fprintf(stderr, "error: unable to make labels from parameter %s--must be string type\n",
            parameter_name);
    exit(1);
  }
}

char *getParameterLabel(SDDS_TABLE *SDDS_table, char *parameter_name, char *edit, char *format) {
  char *ptr, buffer[SDDS_MAXLINE], *dataBuffer = NULL;
  long type = 0;

  if (format && !SDDS_StringIsBlank(format)) {
    if (!(dataBuffer = malloc(sizeof(double) * 4)))
      SDDS_Bomb("Allocation failure in determin_dataset_labels");
    if (!SDDS_GetParameter(SDDS_table, parameter_name, dataBuffer))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!(type = SDDS_GetParameterType(SDDS_table, SDDS_GetParameterIndex(SDDS_table, parameter_name))))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_VerifyPrintfFormat(format, type)) {
      fprintf(stderr, "error: given format (\"%s\") for parameter %s is invalid\n",
              format, parameter_name);
      exit(1);
    }
    if (!(SDDS_SprintTypedValue((void *)dataBuffer, 0, type, format, buffer, SDDS_PRINT_NOQUOTES)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_CopyString(&ptr, buffer);
    free(dataBuffer);
  } else {
    if (!SDDS_GetParameterAsString(SDDS_table, parameter_name, &ptr)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }
  if (edit) {
    strcpy(buffer, ptr);
    edit_string(buffer, edit);
    free(ptr);
    SDDS_CopyString(&ptr, buffer);
  }
  return (ptr);
}

void checkLabelParameters(SDDS_TABLE *SDDS_table,
                          char *p1, char *p2, char *p3, char *p4) {
  if (p1 && strlen(p1) > 1 && p1[1] == '@')
    checkParameter(SDDS_table, p1 + 1);
  if (p2 && strlen(p2) > 1 && p2[1] == '@')
    checkParameter(SDDS_table, p2 + 1);
  if (p3 && strlen(p3) > 1 && p3[1] == '@')
    checkParameter(SDDS_table, p3 + 1);
  if (p4 && strlen(p4) > 1 && p4[1] == '@')
    checkParameter(SDDS_table, p4 + 1);
}

void freeParameterLabel(char *users_label, char *label) {
  if (users_label && users_label[0] == '@' && label)
    free(label);
}

void make_enumerated_yscale(char **label0, double *yposition, long labels, char *yEdit, long interval, double scale, long thickness, char *ylabel, double ylabelScale) {
  double hsave, vsave, hsize, vsize;
  double xmin, xmax, ymin, ymax;
  double x, y, yrange, xrange;
  double pmin, pmax, qmin, qmax;
  double wpmin, wpmax, wqmin, wqmax;
  long i, maxlen;
  char buffer[1024], **label;
  double tickFraction = 0.0125;

  get_mapping(&xmin, &xmax, &ymin, &ymax);
  get_pspace(&pmin, &pmax, &qmin, &qmax);
  get_wspace(&wpmin, &wpmax, &wqmin, &wqmax);

  yrange = ymax - ymin;
  xrange = xmax - xmin;
  label_character_size(1);
  get_char_size(&hsave, &vsave, 1);
  vsize = vsave;
  hsize = hsave;
  if (yrange < labels * 1.5 * vsize / interval) {
    vsize = yrange / labels / 1.5;
    hsize = hsave / vsave * vsize;
  }
  label = tmalloc(sizeof(*label) * labels);
  for (i = maxlen = 0; i < labels; i++) {
    if (yEdit) {
      strcpy(buffer, label0[i]);
      edit_string(buffer, yEdit);
      SDDS_CopyString(&label[i], buffer);
    } else
      SDDS_CopyString(&label[i], label0[i]);
    if ((long)strlen(label[i]) > maxlen)
      maxlen = strlen(label[i]);
  }
  xrange = xrange / (pmax - pmin) * 0.8 * (pmin - wpmin);
  if (xrange < maxlen * hsize) {
    hsize = xrange / maxlen;
    vsize = vsave / hsave * hsize;
  }

  hsize *= scale;
  vsize *= scale;
  set_char_size(hsize, vsize, 1);

  widen_window(1);
  i = 0;
  while (labels--) {
    if (i++ % interval != 0) {
      free(label[labels]);
      continue;
    }
    x = xmin - xrange * 0.05;
    if (yposition)
      y = yposition[labels];
    else
      y = labels;
    set_linethickness(thickness);
    jplot_string(&x, &y, label[labels], 'r');
    make_ticks('y', y, 1, 1, xmin, tickFraction * (xmax - xmin), 0, thickness);
    make_ticks('y', y, 1, 1, xmax, -tickFraction * (xmax - xmin), 0, thickness);
    free(label[labels]);
  }
  free(label);
  set_char_size(hsave, vsave, 1);
  xrange = maxlen * hsize;

  if (ylabel) {
    get_char_size(&hsize, &vsize, 1);
    hsize *= ylabelScale;
    vsize *= ylabelScale;
    maxlen = strlen(ylabel);
    if (yrange < maxlen * vsize) {
      vsize = yrange / maxlen;
      hsize = hsave / vsave * vsize;
    }
    x = x - (xmax - xmin) * 0.15;
    // y = ymin + (ymin+ymax)/2.0;
    y = (ymin + ymax) / 2.0;
    vertical_print(1);
    set_char_size(hsize, vsize, 1);
    set_linethickness(thickness);
    jxyplot_string(&x, &y, ylabel, 'c', 'c');
    set_char_size(hsave, vsave, 1);
    vertical_print(0);
  }
}

void make_enumerated_xscale(char **label0, double *xposition, long labels, char *yEdit, long interval, double scale, long thickness, char *xlabel, double xlabelScale) {
  double hsave, vsave, hsize, vsize;
  double xmin, xmax, ymin, ymax;
  double x, y, yrange, xrange;
  double pmin, pmax, qmin, qmax;
  long i, maxlen;
  char buffer[1024], **label;
  double tickFraction = 0.02;

  label_character_size(1);
  get_char_size(&hsave, &vsave, 1);
  label_character_size(0);
  hsize = hsave;
  vsize = vsave;

  get_mapping(&xmin, &xmax, &ymin, &ymax);
  get_pspace(&pmin, &pmax, &qmin, &qmax);
  yrange = ymax - ymin;
  xrange = xmax - xmin;
  if (xrange < labels * 1.5 * hsize / interval) {
    hsize = xrange / labels / 1.5;
    vsize = vsave / hsave * hsize;
  }
  label = tmalloc(sizeof(*label) * labels);
  for (i = maxlen = 0; i < labels; i++) {
    if (yEdit) {
      strcpy(buffer, label0[i]);
      edit_string(buffer, yEdit);
      SDDS_CopyString(&label[i], buffer);
    } else
      SDDS_CopyString(&label[i], label0[i]);
    if ((long)strlen(label[i]) > maxlen)
      maxlen = strlen(label[i]);
  }
  hsize *= scale;
  vsize *= scale;
  yrange = yrange * 0.1;
  if (yrange < vsize * maxlen) {
    vsize = yrange / maxlen;
    hsize = hsave / vsave * vsize;
  }
  set_char_size(hsize, vsize, 1);
  widen_window(1);
  vertical_print(1);
  i = 0;
  while (labels--) {
    if (i++ % interval != 0) {
      free(label[labels]);
      continue;
    }
    y = ymin - 0.1 * yrange;
    if (xposition)
      x = xposition[labels];
    else
      x = labels;
    if (x < xmin || x > xmax)
      continue;
    set_linethickness(thickness);
    jxyplot_string(&x, &y, label[labels], 'r', 'c');
    make_ticks('x', x, 1, 1, ymin, tickFraction * (ymax - ymin), 0, thickness);
    make_ticks('x', x, 1, 1, ymax, -tickFraction * (ymax - ymin), 0, thickness);
    free(label[labels]);
  }
  free(label);
  vertical_print(0);
  set_char_size(hsave, vsave, 1);
  if (xlabel) {
    get_char_size(&hsize, &vsize, 1);
    hsize *= xlabelScale;
    vsize *= xlabelScale;
    y -= yrange;
    maxlen = strlen(xlabel);
    if (xrange < hsize * maxlen) {
      hsize = xrange / maxlen;
      vsize = vsave / hsave * hsize;
    }
    x = xmin + xrange / 2.0;
    set_char_size(hsize, vsize, 1);
    jxyplot_string(&x, &y, xlabel, 'c', 't');
    set_char_size(hsave, vsave, 1);
  }
}

void ReadShapeData(SHAPE_DATA *shape, long shapes, long swapXY) {
  long iFile, page, code;
  SDDS_DATASET SDDSin;
  for (iFile = 0; iFile < shapes; iFile++) {
    if (!SDDS_InitializeInput(&SDDSin, shape[iFile].filename))
      SDDS_Bomb("problem reading shape file");
    shape[iFile].xData = shape[iFile].yData = NULL;
    shape[iFile].nPages = 0;
    shape[iFile].nPoints = NULL;
    page = 0;
    if (swapXY)
      SWAP_PTR(shape[iFile].xColumn, shape[iFile].yColumn);
    while ((code = SDDS_ReadPage(&SDDSin)) > 0) {
      if (!(shape[iFile].xData = SDDS_Realloc(shape[iFile].xData,
                                              sizeof(*(shape[iFile].xData)) * (page + 1))) ||
          !(shape[iFile].yData = SDDS_Realloc(shape[iFile].yData,
                                              sizeof(*(shape[iFile].yData)) * (page + 1))) ||
          !(shape[iFile].nPoints = SDDS_Realloc(shape[iFile].nPoints,
                                                sizeof(*(shape[iFile].nPoints)) * (page + 1))))
        SDDS_Bomb("Memory allocation failure reading shape file.");
      if ((shape[iFile].nPoints[page] = SDDS_RowCount(&SDDSin)) <= 0)
        continue;
      if (!(shape[iFile].xData[page] =
            SDDS_GetColumnInDoubles(&SDDSin, shape[iFile].xColumn)) ||
          !(shape[iFile].yData[page] =
            SDDS_GetColumnInDoubles(&SDDSin, shape[iFile].yColumn)))
        SDDS_Bomb("Problem getting column data from shape file.");
      page++;
    }
    shape[iFile].nPages = page;
    if (!SDDS_Terminate(&SDDSin))
      SDDS_Bomb("Problem terminating shape file.");
  }
}

char *addOuterParentheses(char *arg) {
  char *ptr;
  ptr = tmalloc(sizeof(*ptr) * (strlen(arg) + 2));
  sprintf(ptr, "(%s)", arg);
  return ptr;
}

long get_plot_labels(SDDS_DATASET *SDDS_table, char *indepcolumn, char **columnname, long columns,
                     char *allmatches, char *waterfall_par,
                     char *users_xlabel, char *users_ylabel, char *users_title,
                     char **xlabel0, char **ylabel0, char **title0, long deltas, long xRangeProvided,
                     long conversions, CONVERSION_DEFINITION **ucd) {
  char *columnunits = NULL;
  char s[1024];
  long i, is_par = 0;
  char *xlabel, *ylabel, *title, *units;

  xlabel = ylabel = title = units = NULL;
  SDDS_GetColumnInformation(SDDS_table, "units", &columnunits, SDDS_GET_BY_NAME,
                            columnname[0]);
  if (!xRangeProvided) {
    if (SDDS_GetColumnIndex(SDDS_table, indepcolumn) < 0) {
      if (SDDS_GetParameterIndex(SDDS_table, indepcolumn) < 0) {
        fprintf(stderr, "%s is neither a column or a parameter!\n", indepcolumn);
        exit(1);
      }
      is_par = 1;
    }
  }
  if (!users_ylabel) {
    if (!waterfall_par) {
      SDDS_CopyString(&ylabel, "");
    } else {
      SDDS_GetColumnInformation(SDDS_table, "units", &ylabel, SDDS_GET_BY_NAME, waterfall_par);
      for (i = 0; i < conversions; i++) {
        if (ucd[i]->is_parameter == 0) {
          if (wild_match(waterfall_par, ucd[i]->name)) {
            SDDS_CopyString(&ylabel, ucd[i]->new_units);
          }
        }
      }
      if (ylabel && ylabel[0])
        sprintf(s, "%s (%s)", waterfall_par, ylabel);
      else
        sprintf(s, "%s", waterfall_par);
      free(ylabel);
      SDDS_CopyString(&ylabel, s);
    }
  } else {
    SDDS_CopyString(&ylabel, users_ylabel);
  }
  if (!users_xlabel) {
    if (!xRangeProvided) {
      if (is_par)
        SDDS_GetParameterInformation(SDDS_table, "units", &xlabel, SDDS_GET_BY_NAME, indepcolumn);
      else
        SDDS_GetColumnInformation(SDDS_table, "units", &xlabel, SDDS_GET_BY_NAME, indepcolumn);
    }
    if (xlabel && xlabel[0])
      sprintf(s, "%s (%s)", indepcolumn, xlabel);
    else
      sprintf(s, "%s", indepcolumn);
    free(xlabel);
    SDDS_CopyString(&xlabel, s);
  } else
    SDDS_CopyString(&xlabel, users_xlabel);
  if (!users_title) {
    units = NULL;
    if (columnunits) {
      for (i = 1; i < columns; i++) {
        SDDS_GetColumnInformation(SDDS_table, "units", &units, SDDS_GET_BY_NAME,
                                  columnname[i]);
        if (!units || strcmp(columnunits, units) != 0)
          break;
        if (units)
          free(units);
      }
      if (i != columns)
        SDDS_CopyString(&units, "(V.U.)");
      else {
        units = tmalloc(sizeof(*units) * (strlen(columnunits) + 3));
        sprintf(units, "(%s)", columnunits);
      }
    } else
      SDDS_CopyString(&units, "");
    switch (deltas) {
    case DELTAS_FRACTIONAL:
      sprintf(s, "$gD$r%s (fractional) as a function of %s",
              allmatches, indepcolumn);
      break;
    case DELTAS_PLAIN:
      sprintf(s, "$gD$r%s %s as a function of %s",
              allmatches, units, indepcolumn);
      break;
    case DELTAS_NORMALIZED:
      sprintf(s, "%s (normalized)  as a function of %s",
              allmatches, indepcolumn);
      break;
    default:
      sprintf(s, "%s %s as a function of %s",
              allmatches, units, indepcolumn);
      break;
    }
    if (units != columnunits)
      free(units);
    SDDS_CopyString(&title, s);
  } else
    SDDS_CopyString(&title, users_title);
  if (SDDS_NumberOfErrors())
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (columnunits)
    free(columnunits);
  *xlabel0 = xlabel;
  *ylabel0 = ylabel;
  *title0 = title;
  return 1;
}

long process_data_values(double ***data_value0, long nx, long ny, long deltas) {
  long ix, iy;
  double average, min, max, range, factor, **data_value;
  if (deltas < 0)
    return 0;
  data_value = *data_value0;
  for (iy = 0; iy < ny; iy++) {
    average = 0;
    max = -DBL_MAX;
    min = DBL_MAX;
    for (ix = nx - 1; ix >= 0; ix--) {
      average += data_value[ix][iy];
      if (max < data_value[ix][iy])
        max = data_value[ix][iy];
      if (min > data_value[ix][iy])
        min = data_value[ix][iy];
    }
    average /= nx;
    switch (deltas) {
    case DELTAS_PLAIN:
      for (ix = nx - 1; ix >= 0; ix--)
        data_value[ix][iy] -= average;
      break;
    case DELTAS_FRACTIONAL:
      for (ix = nx - 1; ix >= 0; ix--)
        data_value[ix][iy] = (data_value[ix][iy] - average) / average;
      break;
    case DELTAS_NORMALIZED:
      if ((range = max - min))
        factor = 1 / range;
      else
        factor = 0;
      for (ix = nx - 1; ix >= 0; ix--)
        data_value[ix][iy] = (data_value[ix][iy] - average) * factor;
      break;
    default:
      fprintf(stderr, "error: invalid -deltas code %ld detected\n",
              deltas);
      return (1);
    }
  }
  return 0;
}

void process_data(double ***data_value0, int32_t *nx, int32_t *ny, double *xmin, double *xmax,
                  double *ymin, double *ymax, double *dx, double *dy,
                  double *orig_limit, long logscale, double logfloor,
                  long nx_interp, long ny_interp, long x_lowpass, long y_lowpass, long interp_flags,
                  char **xyzArray, char **xyzColumn, long verbosity, short xlog, int32_t *nx_offset, long swap_xy) {
  long i, j;
  double limit[4], **data_value;

  data_value = *data_value0;
  if (logscale) {
    for (i = 0; i < *nx; i++)
      for (j = 0; j < *ny; j++)
        data_value[i][j] = (data_value[i][j] <= 0 ? (logfloor == 0 ? -300 : log10(logfloor)) : log10(data_value[i][j]));
  }
  *xmax = *xmin + ((*nx) - 1) * (*dx);
  *ymax = *ymin + ((*ny) - 1) * (*dy);
  if (orig_limit[0] != orig_limit[1] || orig_limit[2] != orig_limit[3]) {
    memcpy((char *)limit, (char *)orig_limit, 4 * sizeof(limit[0]));
    data_value = window_2d_array(data_value, xmin, xmax, ymin, ymax,
                                 *dx, *dy, nx, ny, limit, xlog, nx_offset, swap_xy);
    *dx = (*xmax - *xmin) / (*nx - 1);
  }
  if (nx_interp != 0 || ny_interp != 0) {
    if (xyzArray[0] || xyzColumn[2]) {
      fprintf(stderr, "warning: interpolation done using static dx and dy\n");
    }
    if (nx_interp != 1 || x_lowpass > 0) {
#ifndef COMPILE_AS_SUBROUTINE
      if (verbosity > 1 && nx_interp > 1)
        printf("interpolating on %ld-times finer grid in x\n", nx_interp);
      if (verbosity > 1 && x_lowpass > 0)
        printf("low-pass filtering with cutoff at %ld steps below maximum x frequency\n", x_lowpass);
#endif
      data_value = fft_interpolation_index1(data_value, *nx, *ny, nx_interp, x_lowpass, interp_flags);
      *dx /= nx_interp;
      *nx = nx_interp * ((*nx) - 1) + 1;
    }
    if (ny_interp != 1 || y_lowpass > 0) {
#ifndef COMPILE_AS_SUBROUTINE
      if (verbosity > 1 && ny_interp > 1)
        printf("interpolating on %ld-times finer grid in y\n", ny_interp);
      if (verbosity > 1 && y_lowpass > 0)
        printf("low-pass filtering with cutoff at %ld steps below maximum y frequency\n", y_lowpass);
#endif
      data_value = fft_interpolation_index2(data_value, *nx, *ny, ny_interp, y_lowpass, interp_flags);
      (*dy) /= ny_interp;
      (*ny) = ny_interp * ((*ny) - 1) + 1;
    }
  }
  *data_value0 = data_value;
}

char *rearrange_by_index(char *data, long *index, long element_size, long num) {

  char *tmpdata = NULL;
  long i, j, offset;

  tmpdata = tmalloc(element_size * num);

  offset = element_size / sizeof(char);

  for (i = 0; i < num; i++) {
    for (j = 0; j < offset; j++)
      tmpdata[i * offset + j] = data[index[i] * offset + j];
  }

  return (tmpdata);
}

long drawline_AP(DRAW_LINE_SPEC **drawLineSpec, long *drawlines, char **item, long items) {
  long i = *drawlines;
  *drawLineSpec = SDDS_Realloc(*drawLineSpec, sizeof(**drawLineSpec) * (i + 1));
  (*drawLineSpec)[i].linethickness = 0;
  if (!scanItemList(&(*drawLineSpec)[i].flags, item, &items, 0,
                    "x0value", SDDS_DOUBLE, &(*drawLineSpec)[i].x0, 1, DRAW_LINE_X0GIVEN,
                    "y0value", SDDS_DOUBLE, &(*drawLineSpec)[i].y0, 1, DRAW_LINE_Y0GIVEN,
                    "p0value", SDDS_DOUBLE, &(*drawLineSpec)[i].p0, 1, DRAW_LINE_P0GIVEN,
                    "q0value", SDDS_DOUBLE, &(*drawLineSpec)[i].q0, 1, DRAW_LINE_Q0GIVEN,
                    "x1value", SDDS_DOUBLE, &(*drawLineSpec)[i].x1, 1, DRAW_LINE_X1GIVEN,
                    "y1value", SDDS_DOUBLE, &(*drawLineSpec)[i].y1, 1, DRAW_LINE_Y1GIVEN,
                    "p1value", SDDS_DOUBLE, &(*drawLineSpec)[i].p1, 1, DRAW_LINE_P1GIVEN,
                    "q1value", SDDS_DOUBLE, &(*drawLineSpec)[i].q1, 1, DRAW_LINE_Q1GIVEN,
                    "x0parameter", SDDS_STRING, &(*drawLineSpec)[i].x0Param, 1, DRAW_LINE_X0PARAM,
                    "y0parameter", SDDS_STRING, &(*drawLineSpec)[i].y0Param, 1, DRAW_LINE_Y0PARAM,
                    "p0parameter", SDDS_STRING, &(*drawLineSpec)[i].p0Param, 1, DRAW_LINE_P0PARAM,
                    "q0parameter", SDDS_STRING, &(*drawLineSpec)[i].q0Param, 1, DRAW_LINE_Q0PARAM,
                    "x1parameter", SDDS_STRING, &(*drawLineSpec)[i].x1Param, 1, DRAW_LINE_X1PARAM,
                    "y1parameter", SDDS_STRING, &(*drawLineSpec)[i].y1Param, 1, DRAW_LINE_Y1PARAM,
                    "p1parameter", SDDS_STRING, &(*drawLineSpec)[i].p1Param, 1, DRAW_LINE_P1PARAM,
                    "q1parameter", SDDS_STRING, &(*drawLineSpec)[i].q1Param, 1, DRAW_LINE_Q1PARAM,
                    "linetype", SDDS_LONG, &(*drawLineSpec)[i].linetype, 1, DRAW_LINE_LINETYPEGIVEN,
                    "thickness", SDDS_LONG, &(*drawLineSpec)[i].linethickness, 1, 0,
                    "clip", -1, NULL, 0, DRAW_LINE_CLIPGIVEN,
                    NULL))
    return bombre("invalid -drawline syntax", drawlineUsage, 0);

  if (bitsSet((*drawLineSpec)[i].flags &
              (DRAW_LINE_X0GIVEN + DRAW_LINE_P0GIVEN + DRAW_LINE_X0PARAM + DRAW_LINE_P0PARAM)) != 1 ||
      bitsSet((*drawLineSpec)[i].flags &
              (DRAW_LINE_Y0GIVEN + DRAW_LINE_Q0GIVEN + DRAW_LINE_Y0PARAM + DRAW_LINE_Q0PARAM)) != 1 ||
      bitsSet((*drawLineSpec)[i].flags &
              (DRAW_LINE_X1GIVEN + DRAW_LINE_P1GIVEN + DRAW_LINE_X1PARAM + DRAW_LINE_P1PARAM)) != 1 ||
      bitsSet((*drawLineSpec)[i].flags &
              (DRAW_LINE_Y1GIVEN + DRAW_LINE_Q1GIVEN + DRAW_LINE_Y1PARAM + DRAW_LINE_Q1PARAM)) != 1)
    return bombre("invalid -drawline syntax", drawlineUsage, 0);

  if ((*drawLineSpec)[i].linethickness < 0)
    (*drawLineSpec)[i].linethickness = 0;
  if ((*drawLineSpec)[i].linethickness >= 10)
    (*drawLineSpec)[i].linethickness = 9;

  *drawlines += 1;
  return 1;
}

void determine_drawline(DRAW_LINE_SPEC *drawLineSpec, long drawlines, SDDS_TABLE *table) {
  long i, j;
  double *positionPtr;
  unsigned long flagMask, flagSubs;
  char **namePtr;

  for (i = 0; i < drawlines; i++) {
    drawLineSpec[i].x0Param =
      drawLineSpec[i].y0Param =
      drawLineSpec[i].p0Param =
      drawLineSpec[i].q0Param =
      drawLineSpec[i].x1Param =
      drawLineSpec[i].y1Param =
      drawLineSpec[i].p1Param =
      drawLineSpec[i].q1Param = NULL;
    flagMask = DRAW_LINE_X0PARAM;
    flagSubs = DRAW_LINE_X0GIVEN;
    positionPtr = &(drawLineSpec[i].x0);
    namePtr = &(drawLineSpec[i].x0Param);

    for (j = 0; j < 8; j++) {
      if (!(drawLineSpec[i].flags & flagMask)) {
        flagMask = flagMask << 1;
        flagSubs = flagSubs << 1;
        positionPtr += 1;
        namePtr += 1;
        continue;
      }
      if (!SDDS_GetParameterAsDouble(table, *namePtr, positionPtr)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      drawLineSpec[i].flags |= flagSubs;
      flagMask = flagMask << 1;
      flagSubs = flagSubs << 1;
      positionPtr += 1;
      namePtr += 1;
    }
  }
}

void plot3DSurface(double **data, long nx, long ny, double xmin, double xmax, double ymin, double ymax) {
#if defined(_WIN32)
  char tmpName[L_tmpnam];
  if (!tmpnam(tmpName)) {
    fprintf(stderr, "unable to create temporary file for 3D plot\n");
    return;
  }
  FILE *fp = fopen(tmpName, "w");
  if (!fp) {
    fprintf(stderr, "unable to open temporary file for 3D plot\n");
    return;
  }
#else
  char tmpName[] = "sddscontour3dXXXXXX";
  int fd = mkstemp(tmpName);
  FILE *fp = NULL;
  if (fd == -1 || !(fp = fdopen(fd, "w"))) {
    fprintf(stderr, "unable to create temporary file for 3D plot\n");
    return;
  }
#endif
  fprintf(fp, "%ld %ld %g %g %g %g\n", nx, ny, xmin, xmax, ymin, ymax);
  for (long j = 0; j < ny; j++) {
    for (long i = 0; i < nx; i++)
      fprintf(fp, "%g ", data[i][j]);
    fprintf(fp, "\n");
  }
  fclose(fp);
  char command[1024];
  snprintf(command, sizeof(command), "mpl_qt -3d %s", tmpName);
  if (system(command) == -1)
    fprintf(stderr, "unable to run mpl_qt for 3D plot\n");
#if defined(_WIN32)
  remove(tmpName);
#else
  unlink(tmpName);
#endif
}

void draw_lines(DRAW_LINE_SPEC *drawLineSpec, long drawlines, long linetypeDefault, double *limit) {
  long iline, oldLinetype;
  double x0equiv, x1equiv, y0equiv, y1equiv;
  double x[2], y[2];
  oldLinetype = set_linetype(linetypeDefault);

  for (iline = 0; iline < drawlines; iline++) {
    if (drawLineSpec[iline].flags & DRAW_LINE_LINETYPEGIVEN) {
      set_linetype(drawLineSpec[iline].linetype);
    }
    set_linethickness(drawLineSpec[iline].linethickness);
    if (drawLineSpec[iline].flags & DRAW_LINE_X0GIVEN)
      x0equiv = drawLineSpec[iline].x0;
    else {
      x0equiv = (limit[1] - limit[0]) * drawLineSpec[iline].p0 + limit[0];
    }
    if (drawLineSpec[iline].flags & DRAW_LINE_Y0GIVEN)
      y0equiv = drawLineSpec[iline].y0;
    else {
      y0equiv = (limit[3] - limit[2]) * drawLineSpec[iline].q0 + limit[2];
    }
    if (drawLineSpec[iline].flags & DRAW_LINE_X1GIVEN)
      x1equiv = drawLineSpec[iline].x1;
    else {
      x1equiv = (limit[1] - limit[0]) * drawLineSpec[iline].p1 + limit[0];
    }
    if (drawLineSpec[iline].flags & DRAW_LINE_Y1GIVEN)
      y1equiv = drawLineSpec[iline].y1;
    else {
      y1equiv = (limit[3] - limit[2]) * drawLineSpec[iline].q1 + limit[2];
    }
    if (drawLineSpec[iline].flags & DRAW_LINE_CLIPGIVEN) {
      x[0] = x0equiv;
      y[0] = y0equiv;
      x[1] = x1equiv;
      y[1] = y1equiv;
      plot_lines(x, y, 2,
                 drawLineSpec[iline].flags & DRAW_LINE_LINETYPEGIVEN ? drawLineSpec[iline].linetype : oldLinetype, 0);
    } else {
      pdraw(x0equiv, y0equiv, 0);
      pdraw(x1equiv, y1equiv, 1);
    }
    if (drawLineSpec[iline].flags & DRAW_LINE_LINETYPEGIVEN) {
      set_linetype(linetypeDefault);
    }
  }
  set_linetype(oldLinetype);
  set_linethickness(0);
}

void get_xyaxis_value(char *xaxisScalePar, char *xaxisOffsetPar, char *yaxisScalePar, char *yaxisOffsetPar,
                      SDDS_DATASET *SDDS_table,
                      double *xaxisScale, double *xaxisOffset, double *yaxisScale, double *yaxisOffset,
                      char **users_xlabel, char **users_ylabel) {

  if (yaxisScalePar) {
    char *label, *units, s[1024];
    label = units = NULL;
    if (!SDDS_GetParameterAsDouble(SDDS_table, yaxisScalePar, yaxisScale)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    if (!(*users_ylabel)) {
      SDDS_GetParameterInformation(SDDS_table, "units", &units, SDDS_GET_BY_NAME, yaxisScalePar);
      SDDS_GetParameterInformation(SDDS_table, "symbol", &label, SDDS_GET_BY_NAME, yaxisScalePar);
      if (label) {
        if (units)
          snprintf(s, sizeof(s), "%s (%s)", label, units);
        else
          snprintf(s, sizeof(s), "%s", label);
        SDDS_CopyString(users_ylabel, s);
        free(label);
        if (units)
          free(units);
      }
    }
  }
  if (yaxisOffsetPar) {
    if (!SDDS_GetParameterAsDouble(SDDS_table, yaxisOffsetPar, yaxisOffset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
  }
  if (xaxisScalePar) {
    char *label, *units, s[1024];
    label = units = NULL;
    if (!SDDS_GetParameterAsDouble(SDDS_table, xaxisScalePar, xaxisScale)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    if (!(*users_xlabel)) {
      SDDS_GetParameterInformation(SDDS_table, "units", &units, SDDS_GET_BY_NAME, xaxisScalePar);
      SDDS_GetParameterInformation(SDDS_table, "symbol", &label, SDDS_GET_BY_NAME, xaxisScalePar);
      if (label) {
        if (units)
          snprintf(s, sizeof(s), "%s (%s)", label, units);
        else
          snprintf(s, sizeof(s), "%s", label);
        SDDS_CopyString(users_ylabel, s);
        free(label);
        if (units)
          free(units);
      }
    }
  }
  if (xaxisOffsetPar) {
    if (!SDDS_GetParameterAsDouble(SDDS_table, xaxisOffsetPar, xaxisOffset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
  }
}
