/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: graph.h 
 * purpose: function prototypes and definitions for use with
 *          GNUPLOT-based graphics
 *
 * Michael Borland, 1991.
 $Log: not supported by cvs2svn $
 Revision 1.32  2011/03/22 23:47:10  borland
 Added thickness qualifier for arrows.

 Revision 1.31  2010/01/05 20:06:45  soliday
 Added the -intensityBar option which can be used to move the intensity bar
 and to resize the label text to the left and the unit text above.

 Revision 1.30  2007/05/17 19:40:32  soliday
 Modified to allow changing the thickness for the intensity bar labels.

 Revision 1.29  2006/10/13 22:24:16  shang
 added NO_COLOR_BAR

 Revision 1.28  2006/03/20 23:09:06  soliday
 Xuesong improved the zooming feature. I improved the color spectrum
 intensity bar when using multiple panels and spliting the data based
 on column data.

 Revision 1.27  2005/11/04 22:46:45  soliday
 Updated code to be compiled by a 64 bit processor.

 Revision 1.26  2005/08/29 21:31:25  shang
 added linetype to xlabel, ylabel, topline and title options

 Revision 1.25  2005/08/15 14:08:00  jiaox
 Added keyword "fill" for graphic option. Also added circel type(9) for symbols.

 Revision 1.24  2005/04/26 22:20:15  shang
 added labelScale argument to make_scale_with_label()

 Revision 1.23  2005/03/22 03:33:53  soliday
 Made various changes including fixing a problem with the intensity bar
 colors. Also changed png and ps default colors so they are all
 the same as motif.

 Revision 1.22  2005/03/07 22:39:06  shang
 moved make_intensity_bar() from sddscontour and added colorSymbol, colorUnits
 arguments for adding the symbol to the color bar

 Revision 1.21  2005/02/22 17:42:20  shang
 added no_log_label argument to make_scale()

 Revision 1.20  2002/08/14 17:24:51  soliday
 Added Open License

 Revision 1.19  2002/01/09 23:39:49  borland
 Added line thickness setting for enumerated scales.

 Revision 1.18  2001/08/13 20:11:51  soliday
 Added thickness options for several options.

 Revision 1.17  2001/06/04 20:03:07  soliday
 Made some changes that only affect sddscontour.

 Revision 1.16  2000/06/07 20:07:09  soliday
 Added MotifColor

 Revision 1.15  2000/06/02 20:56:05  soliday
 Added thickness option to lineTypeDefault, drawLine, axes, tick and subtick
 options.

 Revision 1.14  2000/04/27 19:25:25  soliday
 Modified prototypes so that line thickness is supported.

 Revision 1.13  1999/05/24 18:17:24  borland
 Now support the -ticksettings=[xy]scalechars qualifier for time mode ticks.

 Revision 1.12  1999/05/24 17:51:27  borland
 Fixed some label scaling bugs and reinstated the -labelsize option.

 Revision 1.11  1999/05/03 15:51:14  borland
 Fixed bugs with subticks: no linetype control, no grid control.

 Revision 1.10  1999/02/10 14:26:58  borland
 Store the string "$i$iNo data$d$d" in an array so that there are no problems
 with gnu compiler when the string is passed to jxyplot.
 Also, set the character size to the default for each panel using new
 set_default_char_size() routine.

 Revision 1.9  1998/12/22 16:02:33  borland
 New character scaling algorithm for all labels.  Considerable changes
 to psymbol routine that draws characters and returns the size of
 strings.

 Revision 1.8  1998/10/01 22:48:03  borland
 Reimplemented scale and offset qualifiers for -xlabel and -ylabel.
 Implemented {xy}scaleChars qualifier for -ticksettings.
 These options allow users to control the size of characters on scale labels
 and ticks labels.

 Revision 1.7  1998/09/02 14:09:01  borland
 Added -alignZero option.  Also added delay qualifier to -zoom option.
 Changed implementation of panel scale determination.  Also modified
 arguments to make_scale() to permit inverted scales.

 Revision 1.6  1998/08/25 15:05:24  borland
 New version with major changes to allow multiple scales on x and y axes.

 * Revision 1.5  1996/03/21  15:32:21  borland
 * Moved device argument routines from sddsplot.c to graphics.c; added prototypes.
 *
 * Revision 1.4  1996/02/29  05:42:29  borland
 * Added variable dot size using subtype qualifier of -graphic option.
 * Required adding an extra argument to the plot_dots() routine.
 *
 * Revision 1.3  1995/09/14  22:04:20  borland
 * Added support for yimpulse and ybar graphic types.
 *
 * Revision 1.2  1995/09/05  21:09:19  saunders
 * First test release of the SDDS1.5 package.
 *
 */
#if !defined(GRAPH_INCLUDED)
#define GRAPH_INCLUDED 

typedef struct {
    double xmag, ymag;
    double xcent, ycent;
    short auto_center;
    } ZOOM;


void edge_intersect(double ax, double ay, double bx, double by, 
    double *ex, double *ey);
int two_edge_intersect(double ax, double ay, double bx, double by,
	double *lx, double *ly);
int do_point(int x, int y, int number, double scale);
int do_point_fill(int x, int y, int number, double scale);
int line_and_point(int x, int y, int number);
int do_arrow(int sx, int sy, int ex, int ey);
int list_terms(FILE *fp);
int change_term(char *name, int length);
int init_terminal(void);
char *vms_init(void);
int UP_redirect(int caller);
int test_term(void);
long CPS_setLineParameters(double red, double green, double blue, double widthMultiplier, long index);

void set_mapping(double x_minimum, double x_maximum, double y_minimum, double y_maximum);
void get_mapping(double *xmin, double *xmax, double *ymin, double *ymax);
void set_window(double x_minimum, double x_maximum, double y_minimum, double y_maximum);
void set_pspace(double p_mini, double p_maxi, double q_mini, double q_maxi);
void get_pspace(double *p_minimum, double *p_maximum, double *q_minimum, double *q_maximum);
void set_legend_space(double p_mini, double p_maxi, double q_mini, double q_maxi);
void get_legend_space(double *p_minimum, double *p_maximum, double *q_minimum, double *q_maximum);
void set_wspace(double wp_min, double wp_max, double wq_min, double wq_max);
void get_wspace(double *wp_min, double *wp_max, double *wq_min, double *wq_max);

int change_term(char *name, int length);

char *gs_device_arguments(char *devarg, long get);
void setDeviceArgv(char **argv, long argc);
char **getDeviceArgv(long *argc);

void plot_points_fill(double *xd, double *yd, long n, long type, long subtype, double scale, int thickness,int fill);
void plot_points(double *xd, double *yd, long n, long type, long subtype, double scale, int thickness);
void plot_lines(double *xd, double *yd, long n, int line_type, int line_thickness);
void plot_lines_sever(double *xd, double *yd, long n, int line_type, int line_thickness);
void plot_lines_gap(double *xd, double *yd, double xgap, double ygap, long n, int line_type, int line_thickness);
void plot_dots(double *xd, double *yd, long n, int dot_type, int dot_subtype);
void plot_impulses(double *xd, double *yd, long n, int linetype, int line_thickness);
void plot_bars(double *xd, double *yd, long n, int line_type, int line_thickness);
void plot_yimpulses(double *xd, double *yd, long n, int linetype, int line_thickness);
void plot_ybars(double *xd, double *yd, long n, int line_type, int line_thickness);
void dplot_lines(int *xd, int *yd, long n, int line_type);
void plot_eb(double x, double y, double sx, double sy, int mode, int linetype, int line_thickness);
void plot_error_bars(double *xd, double *yd, double *sx, double *sy, long n, int mode, int line_thickness);
void graphics_on(void);
void frame_end(int hold_screen);
void graphics_off(void);
void alloc_spectrum(long num, int spec, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1);
void border(void);
void set_clipping(int points, int lines1, int lines2);
void set_aspect(double aspect);
void computeAspectAdjustedLimits(double *limitNew, double *limit, double aspect);
void check_scales(char *caller);

void get_position(double *x, double *y);
void pmove(double x, double y);
void pdraw(double x, double y, int pen_down);

#define LEFT_JUSTIFY   -1
#define CENTER_JUSTIFY  0
#define RIGHT_JUSTIFY   1
#define RETURN_LENGTH   2

extern int translate_hershey_codes(char *s);
extern void plot_string(double *x, double *y, char *s);
extern void cplot_string(double *x, double *y, char *s);
extern void jplot_string(double *x, double *y, char *s, char mode);

extern void set_char_size(double h, double v, int user_coords);
extern void fix_char_size(double h, double v, int user_coords, short mode);
extern void set_default_char_size();
extern void get_char_size(double *h, double *v, int user_coords);
extern void set_vertical_print_direction(int direction);
extern void label_character_size(int turn_on);
extern void char_angle(double rigid, double deform);
extern void char_scale(double xfact, double yfact, short mode);

extern int set_linethickness(int linethickness);
extern int set_linetype(int linetype);
extern int get_linetype(void);

extern void plot_xlabel(char *label);
extern void set_title_at_top(int mode);
int vertical_print(int turn_on);
extern void widen_window(int make_wide);
extern void plot_ylabel(char *label);

#define COMPPLOTTEDSIZE_DEVICEUNITS  0x0001UL
#define COMPPLOTTEDSIZE_NOROTATE     0x0002UL
long computePlottedStringSize(char *label, double *xlen, double *ylen, 
                              double *xcen, double *ycen, unsigned long mode);
double computePlottedStringLength(char *label, unsigned long mode);
void plotTitle(char *label, long which, long topTitle,
               double scale, double offset, long thickness, long linetype);
void plotStringInBox(char *s, double xc, double yc, double dx, double dy,
                     unsigned long mode, short lockSize);
void time_date_stamp(void);

typedef struct {
    unsigned long flags;
    double scale, barbLength, barbAngle;
    int32_t linetype, thickness;
    } ARROW_SETTINGS;
#define ARROW_CENTERED         0x0001
#define ARROW_SCALE_GIVEN      0x0002
#define ARROW_BARBLENGTH_GIVEN 0x0004
#define ARROW_BARBANGLE_GIVEN  0x0008
#define ARROW_CARTESIAN_DATA   0x0010
#define ARROW_POLAR_DATA       0x0020
#define ARROW_SCALAR_DATA      0x0040
#define ARROW_LINETYPE_GIVEN   0x0080
#define ARROW_SINGLEBARB       0x0100
#define ARROW_AUTOSCALE        0x0200
#define ARROW_ENDPOINTS        0x0400
#define ARROW_THICKNESS_GIVEN  0x0800

void plot_arrows(double *x, double *y, double *x1, double *y1, long n, 
                 ARROW_SETTINGS *arrow);
void plot_arrow(double x, double y, double length, double angle,
    double barb_length, double barb_angle, long arrow_type, long arrow_flags, long thickness);
void plot_arrows_old(double *x, double *y, double *length, double *angle, long n,
    double arrow_type, double arrow_code);
void plot_arrow_old(double x, double y, double length, double angle,
    double barb_length, double barb_angle,
    long arrow_type, long arrow_flags
    );
void plot_scalar(double x, double y, double length, long type, unsigned long flags);
void set_arrow_scale_factor(double *arsf);
void set_arrow_barb_length(double *arsf);
void set_arrow_barb_angle(double *arsf);

void scales(void);
void make_scales(int plane, int do_log,  int do_labels, double label_modulus, 
                 double tick_fraction, double subtick_fraction, 
                 double spacing, double factor, int n_subticks,
		 int linetype, int linethickness, int sublinetype, int sublinethickness);
void make_scales_with_label(int plane, int do_log,  int do_labels, double label_modulus, 
			    double tick_fraction, double subtick_fraction, 
			    double spacing, double factor, int n_subticks, int linetype, 
			    int linethickness, long tickLabelThickness, int sublinetype, int sublinethickness,
			    char *label, long labelLineType, long labelThickness, double labelScale);
void make_scale
  (int plane, int do_log, int do_labels, double tick_fraction, double spacing, int linetype, int linethickness,
   long tickLabelThickness, double factor, long hideFactor,  long invertScale,
   double offset, double modulus, double tickLabelScale, int n_subticks, double subtick_fraction,
   int sublinetype, int sublinethickness, int side, double range_min, double range_max, double position, double allowedSpace,
   long adjustForTicks, char *label_string, double labelScale, double labelOffset, long labelLineType, long labelThickness, int no_log_label);
void make_time_scales(int plane, double tick_fraction, int linetype);
void makeTimeScales
  (int plane, double tick_fraction, int linetype,
   double position1, double position2, char *scaleLabel, long scaleLabelLineType, double allowedSize,
   long adjustForTicks, double labelCharSize, double tickCharSize,
   int linethickness, int tickLabelThickness, int labelThickness);
void adjustLabelCharSize(char *label, double allowedSpace, long plane);
void adjustTickCharSize(long labelLength, double plottedLength, char *sampleLable, double spacing, 
                        long ticks, double allowedSpace, long plane);
void makeTickLabel(char *label, char *format, double value, 
                   double factor, double offset, double modulus,
                   double scale, long doExpon, long doLog, long power);
void adjustPositionAndSpaceForTicks(double *position, double *allowedSpace,
                                    double tickFraction, long plane, long side);

void find_format(char *format, double xl, double xh, double dx, int do_exponential);
void find_tick_interval(double *start, double *delta, int *number,
    int *do_exponential, double lower, double upper);
void make_ticks(char dimension, double start, double spacing, int number, double tick_base, double tick_height, int pen, int thickness);
void make_subticks(char dimension, double lower, double upper, double spacing, double tick_base, double tick_height, int pen, int thickness);
typedef struct {
    char *filename;
    double *x, *y;
    int n_pts;
    double hmag, vmag;
    double hoff, voff;
    long plotcode;
    long flags;
/* the first two provide interpretation of hoff and voff */
#define OVL_USERC_OFFSET 1
#define OVL_UNITC_OFFSET 2
/* if set, then pointer can be incremented to find another overlay */
#define OVL_ANOTHER_FOLLOWS 4
    } *OVERLAY;

void time_date_stamp();
float psymbol(int x, int y, char *ktext, float size, float aspect, 
              float daspect, float angle, float tilt, int text_length, int mode);
float psymbol1(int x, int y, char *ktext, float size, float aspect, 
               float daspect, float angle, float tilt, int text_length, int mode,
               double *extent);


void shade_box(long shade, double xl, double xh, double yl, double yh);
int X11_fill_box(int shade, int xl, int xh, int yl, int yh);
void make_intensity_bar(long n_shades, long shadeOffset, long reverse, 
                        double min_value, double max_value, double hue0, 
                        double hue1, char *colorSymbol, char *colorUnits,
			long tickLabelThickness, 
                        double labelsize, double unitsize, double xadjust);

void RGB_values(double *r, double *g, double *b, double hue);
void Spectral_RGB_values(double *r, double *g, double *b, double hue);
void Spectral_BGR_values(double *r, double *g, double *b, double hue);
void Spectral_RGB_NoMagenta_values(double *r, double *g, double *b, double hue);
void Spectral_BGR_NoMagenta_values(double *r, double *g, double *b, double hue);
void Custom_RGB_values(double *red, double *green, double *blue, unsigned short red0, unsigned short green0, unsigned short blue0, unsigned short red1, unsigned short green1, unsigned short blue1, double hue);
void RGB_short_values(unsigned short *r, unsigned short *g, unsigned short *b, double hue);
void Spectral_RGB_short_values(unsigned short *r, unsigned short *g, unsigned short *b, double hue);
void Spectral_BGR_short_values(unsigned short *r, unsigned short *g, unsigned short *b, double hue);
void Spectral_RGB_NoMagenta_short_values(unsigned short *r, unsigned short *g, unsigned short *b, double hue);
void Spectral_BGR_NoMagenta_short_values(unsigned short *r, unsigned short *g, unsigned short *b, double hue);
void Custom_RGB_short_values(unsigned short *r, unsigned short *g, unsigned short *b, unsigned short r0, unsigned short g0, unsigned short b0, unsigned short r1, unsigned short g1, unsigned short b1, double hue);


typedef struct {
    unsigned short red, green, blue;
} MotifColor;

/* Flags for use with multi_plot etc. */
#define EQUAL_ASPECT1    0x000000001
#define EQUAL_ASPECT_1   0x000000002
#define UNSUPPRESS_X_ZERO 0x000000004
#define UNSUPPRESS_Y_ZERO 0x000000008
#define SWAP_X_Y         0x000000010
#define MULTIPEN         0x000000020
#define DO_CONNECT       0x000000040
#define NO_LABELS        0x000000080
#define NO_BORDER        0x000000100
#define NO_SCALES        0x000000200
#define DATE_STAMP       0x000000400
#define NUMBER_GRAPHS    0x000000800
#define DO_WINDOW        0x000001000
#define TITLE_AT_TOP     0x000002000
#define DEVICE_DEFINED   0x000004000
#define NO_GREND         0x000008000
#define DO_SEVER         0x000010000
#define GETCHAR_WAIT     0x000020000
#define SLEEP_WAIT       0x000040000
#define X_GRID           0x000080000
#define Y_GRID           0x000100000
#define DO_X_AXIS        0x000200000
#define DO_Y_AXIS        0x000400000
#define RAPID_FIRE       0x000800000
#define X_MARGINS_ADDED  0x001000000
#define Y_MARGINS_ADDED  0x002000000
#define BOX_LEGEND       0x004000000
#define CENTER_ARROWS    0x008000000
#define UNMULTI_LAST     0x010000000
#define NO_YSCALES       0x020000000
#define NO_XSCALES       0x040000000
#define NO_COLOR_BAR     0x080000000
#define Y_FLIP           0x100000000

#define PLOT_LINE     0x00100
#define PLOT_SYMBOL   0x00200
#define PLOT_CSYMBOL  0x00400
#define PLOT_ERRORBAR 0x00800
#define PLOT_DOTS     0x01000
#define PLOT_IMPULSE  0x02000
#define PLOT_ARROW    0x04000
#define PLOT_BAR      0x08000
#define PLOT_YIMPULSE 0x10000
#define PLOT_YBAR     0x20000
#define PLOT_CODE_MASK 0x000f
#define PLOT_SIZE_MASK 0x00f0

#define PRESET_LINETYPE 0xffff

#endif

