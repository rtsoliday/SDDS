/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "SDDS.h"
#include "SDDSaps.h"

/* file: sddsplot.h
 * include file for sddsplot.c and associated files
 *
 * Michael Borland, 1994
 */

/* Description of structures defined here:
   GRAPHIC_SPEC         Set by -graphic.  Tells what kind of lines, symbols,
   etc. to use.  Includes instance of ARROW_SETTINGS.
   RANGE_SPEC           Set by -range. Specifies limits on the range of the user's
   coordinate region.
   ZOOM_SPEC            Set by -zoom.  Specifies how to zoom in/out.
   LEGEND_SPEC          Set by -legend.  Specifies if and how to make plot legends.
   OVERLAY_SPEC         Set by -overlay.  Specifies how to do an overlay.
   LABEL_SPEC           Set by -[xy]label, -title, and -topline.  Specifies how to make the
   corresponding label.
   STRING_LABEL_SPEC    Set by -string.  Specifies content and placement of text
   strings (exclusive of the x, y, title, and topline labels).
   DRAW_LINE_SPEC       Set by -drawLine.  Specifies a line to draw on the plot.
   SPLIT_SPEC           Set by -split.  Specifies how to segment a data set or data page.
   DATA_INFO            Set internally.  Holds information about a data element.
   TICK_SETTINGS        Set by -tickSettings.  Holds major tick information for x and y.
   ENUMERATE_SETTINGS   Set by -enumerate.  Specifies how to make an enumerated scale.
   POINTLABEL_SETTINGS  Set by -pointLabel.  Specifies how to label individual data points.
   REPLICATE_SETTINGS   Set by -replicate.  Specifies replication of a data set.
   GROUPBY_SETTINGS     Set by -groupby.  Specifies ordering of data sets for plotting.
   LIMIT_SETTINGS       Set by -limit. Specifies limits on users plot region.
   SCALES_GROUP_SPEC    Set by -scalesGroup.  Specifies how to figure scales for both
   planes of data in one request.
   PLOT_REQUEST         Set internally, one for each -column or -parameter option.
   Contains all the data given pertaining to that option.
   Much of this is in the above structures, but much of it is in
   simple elements of this structure.
   PLOT_DATA            Contains all the information needed to plot a single set of data
   (e.g., one array of x and y points, or enumerated values).
   Built after reading data files based on plot requests.
   PLOT_PANEL           Contains all information for a single panel of the plot.
   Includes pointers to PLOT_DATA structures that contain information
   on the data included in the panel.
   SCALES_GROUP_DATA    Contains data giving the ID and the index for the scales group.
   Used for constructing and maintaining a sorted index of the scales
   groups.
   PLOT_SPEC            All-encompassing structure that includes all the information from
   the plot requests, pointers to the data, etc.  All the highest level
   routines have only this structure as an argument.
*/

typedef struct {
  long element;
  long type, subtype, thickness, fill;
  double scale;
  long connect_linetype;
  long vary, modulus;
  ARROW_SETTINGS arrow;
  unsigned long flags;
} GRAPHIC_SPEC;
#define GRAPHIC_VARY_EACHPAGE      0x00000001
#define GRAPHIC_VARY_EACHFILE      0x00000002
#define GRAPHIC_VARY_EACHREQUEST   0x00000004
#define GRAPHIC_CONNECT            0x00000008
#define GRAPHIC_VARY_TYPE          0x00000010
#define GRAPHIC_VARY_SUBTYPE       0x00000020
#define GRAPHIC_CONNECT_EQ_TYPE    0x00000040
#define GRAPHIC_CONNECT_EQ_SUBTYPE 0x00000080
#define GRAPHIC_SUBTYPE_EQ_TYPE    0x00000100
#define GRAPHIC_VARY_FIXFORNAME    0x00000200
#define GRAPHIC_VARY_FIXFORFILE    0x00000400
#define GRAPHIC_VARY_FIXFORREQUEST 0x00000800

typedef struct {
  double maximum[2];
  double minimum[2];
  double center[2];
  unsigned long flags;
} RANGE_SPEC ;
#define RANGE_MAXIMUM 0x0001
#define XRANGE_MAXIMUM RANGE_MAXIMUM
#define YRANGE_MAXIMUM (RANGE_MAXIMUM<<1)

#define RANGE_MINIMUM (RANGE_MAXIMUM<<2)
#define XRANGE_MINIMUM RANGE_MINIMUM
#define YRANGE_MINIMUM (RANGE_MINIMUM<<1)

#define RANGE_CENTER  (RANGE_MINIMUM<<2)
#define XRANGE_CENTER RANGE_CENTER
#define YRANGE_CENTER (RANGE_CENTER<<1)

    
typedef struct {
  double magnification[2];
  double center[2];
  unsigned long flags;
} ZOOM_SPEC;
#define ZOOM_DELAY 0x0001
#define ZOOM_MAG 0x0002
#define ZOOM_XMAG ZOOM_MAG
#define ZOOM_YMAG (ZOOM_MAG<<1)
#define ZOOM_CEN  (ZOOM_MAG<<2)
#define ZOOM_XCEN ZOOM_CEN
#define ZOOM_YCEN (ZOOM_CEN<<1)
#define ZOOM_UCEN (ZOOM_CEN<<2)
#define ZOOM_PCEN ZOOM_UCEN
#define ZOOM_QCEN (ZOOM_UCEN<<1)

typedef struct {
  unsigned long code;
  char *value, *edit_command;
  double scale;
  int32_t thickness;
  char *format;
} LEGEND_SPEC;
#define LEGEND_YSYMBOL       0x00000001
#define LEGEND_FILENAME      0x00000002
#define LEGEND_SPECIFIED     0x00000004
#define LEGEND_ROOTNAME      0x00000008
#define LEGEND_YDESCRIPTION  0x00000010
#define LEGEND_PARAMETER     0x00000020
#define LEGEND_UNITS         0x00000040
#define LEGEND_XSYMBOL       0x00000080
#define LEGEND_XDESCRIPTION  0x00000100
#define LEGEND_EDIT          0x00000200
#define LEGEND_FIRSTFILEONLY 0x00000400
#define LEGEND_SCALEGIVEN    0x00000800
#define LEGEND_XNAME         0x00001000
#define LEGEND_YNAME         0x00002000
#define LEGEND_THICKNESS     0x00004000
#define LEGEND_FORMAT        0x00008000
#define LEGEND_NOSUBTYPE     0x00010000
#define LEGEND_CHOICES (LEGEND_YSYMBOL|LEGEND_FILENAME|LEGEND_SPECIFIED|LEGEND_ROOTNAME|LEGEND_YDESCRIPTION|LEGEND_PARAMETER|LEGEND_XSYMBOL|LEGEND_XDESCRIPTION|LEGEND_XNAME|LEGEND_YNAME|LEGEND_THICKNESS|LEGEND_NOSUBTYPE)

typedef struct {
  unsigned long flags;
  double factor[2], offset[2], unitOffset[2];
} OVERLAY_SPEC ;
#define OVERLAY_DATA   0x00000001
#define OVERLAY_FACTOR 0x00000002
#define OVERLAY_XFACTOR OVERLAY_FACTOR
#define OVERLAY_YFACTOR (OVERLAY_FACTOR<<1)
#define OVERLAY_OFFSET  (OVERLAY_FACTOR<<2)
#define OVERLAY_XOFFSET OVERLAY_OFFSET
#define OVERLAY_YOFFSET (OVERLAY_OFFSET<<1)
#define OVERLAY_NORMAL  (OVERLAY_OFFSET<<2)
#define OVERLAY_XNORMAL OVERLAY_NORMAL
#define OVERLAY_YNORMAL (OVERLAY_NORMAL<<1)
#define OVERLAY_CENTER  (OVERLAY_NORMAL<<2)
#define OVERLAY_XCENTER OVERLAY_CENTER
#define OVERLAY_YCENTER (OVERLAY_CENTER<<1)
#define OVERLAY_UNITOFFSET (OVERLAY_CENTER<<2)
#define OVERLAY_POFFSET OVERLAY_UNITOFFSET
#define OVERLAY_QOFFSET (OVERLAY_UNITOFFSET<<1)
#define OVERLAY_UNITALIGN (OVERLAY_UNITOFFSET<<2)
#define OVERLAY_PALIGN OVERLAY_UNITALIGN
#define OVERLAY_QALIGN (OVERLAY_UNITALIGN<<1)

typedef struct {
  char *label, *edit_command, *format ;
  unsigned long flags;
  double offset, scale;
  int32_t thickness, linetype;
  long vary;
} LABEL_SPEC;
typedef struct {
  char *string, *edit_command, *justify_mode, *format;
  unsigned long flags;
  char *positionParameter[2];
  double position[2], scale, angle, slant;
  int32_t linetype, thickness;
} STRING_LABEL_SPEC;
#define LABEL_PARAMETER_GIVEN   0x00001U
#define LABEL_POSITION_GIVEN    0x00002U
#define LABEL_SCALE_GIVEN       0x00004U
#define LABEL_X_GIVEN           0x00008U
#define LABEL_Y_GIVEN           0x00010U
#define LABEL_P_GIVEN           0x00020U
#define LABEL_Q_GIVEN           0x00040U
#define LABEL_EDITCOMMAND_GIVEN 0x00080U
#define LABEL_USE_DEFAULT       0x00100U
#define LABEL_JUSTIFYMODE_GIVEN 0x00200U
#define LABEL_OFFSET_GIVEN      0x00400U
#define LABEL_ANGLE_GIVEN       0x00800U
#define LABEL_LINETYPE_GIVEN    0x01000U
#define LABEL_STRING_GIVEN      0x02000U
#define LABEL_SLANT_GIVEN       0x04000U
#define LABEL_USE_NAME          0x08000U
#define LABEL_FORMAT_GIVEN      0x10000U
#define LABEL_USE_SYMBOL        (LABEL_USE_NAME<<1)
#define LABEL_USE_DESCRIPTION   (LABEL_USE_NAME<<2)
#define LABEL_INCLUDE_UNITS     (LABEL_USE_NAME<<3)
#define LABEL_THICKNESS_GIVEN   (LABEL_USE_NAME<<4)
#define LABEL_XPARAM_GIVEN      (LABEL_USE_NAME<<5)
#define LABEL_YPARAM_GIVEN      (LABEL_XPARAM_GIVEN<<1)

typedef struct {
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

/* The order of the *[01]GIVEN items must not be changed. */
/* They must correspond in order to the *[01]PARAM items */
#define DRAW_LINE_LINETYPEGIVEN   0x000001U
#define DRAW_LINE_CLIPGIVEN       0x000002U
#define DRAW_LINE_X0GIVEN         0x000040U
#define DRAW_LINE_Y0GIVEN         0x000080U
#define DRAW_LINE_P0GIVEN         0x000100U
#define DRAW_LINE_Q0GIVEN         0x000200U
#define DRAW_LINE_X1GIVEN         0x000400U
#define DRAW_LINE_Y1GIVEN         0x000800U
#define DRAW_LINE_P1GIVEN         0x001000U
#define DRAW_LINE_Q1GIVEN         0x002000U
/* The order of the *[01]PARAM items must not be changed. */
/* They must correspond in order to the *[01]GIVEN items */
#define DRAW_LINE_X0PARAM         0x004000U
#define DRAW_LINE_Y0PARAM         0x008000U
#define DRAW_LINE_P0PARAM         0x010000U
#define DRAW_LINE_Q0PARAM         0x020000U
#define DRAW_LINE_X1PARAM         0x040000U
#define DRAW_LINE_Y1PARAM         0x080000U
#define DRAW_LINE_P1PARAM         0x100000U
#define DRAW_LINE_Q1PARAM         0x200000U

typedef struct {
  unsigned long flags;
  int32_t interval;
  double width, start, end, min, max;
  char *name, *symbol, *units;
  long data_type, value_valid;
  double ref_value;
  char *ref_string;
} SPLIT_SPEC;
#define SPLIT_PARAMETERCHANGE 0x00000001
#define SPLIT_COLUMNBIN       0x00000002
#define SPLIT_CHANGE_WIDTH    0x00000004
#define SPLIT_CHANGE_START    0x00000008
#define SPLIT_PAGES           0x00000010
#define SPLIT_PAGES_INTERVAL  0x00000020
#define SPLIT_POINTS          0x00000040
#define SPLIT_COMPLETELY      0x00000080
#define SPLIT_NOCOLORBAR      0x00000100
#define SPLIT_REVERSE_ORDER   0x00000200

typedef struct {
  char *symbol, *units, *description;
} DATA_INFO;

# if __WORDSIZE == 64
#define TICKSET_GIVEN          0x000000001UL
#define TICKSET_XGRID          0x000000002UL
#define TICKSET_YGRID          0x000000004UL
#define TICKSET_XLINETYPE      0x000000008UL
#define TICKSET_YLINETYPE      0x000000010UL
#define TICKSET_XFRACTION      0x000000020UL
#define TICKSET_YFRACTION      0x000000040UL
#define TICKSET_XDIVISIONS     0x000000080UL
#define TICKSET_YDIVISIONS     0x000000100UL
#define TICKSET_XSPACING       0x000000200UL
#define TICKSET_YSPACING       0x000000400UL
#define TICKSET_XLOGARITHMIC   0x000000800UL
#define TICKSET_YLOGARITHMIC   0x000001000UL
#define TICKSET_LINETYPE       0x000002000UL
#define TICKSET_FRACTION       0x000004000UL
#define TICKSET_XMODULUS       0x000008000UL
#define TICKSET_YMODULUS       0x000010000UL
#define TICKSET_XFACTOR        0x000020000UL
#define TICKSET_YFACTOR        0x000040000UL
#define TICKSET_XTIME          0x000080000UL
#define TICKSET_YTIME          0x000100000UL
#define TICKSET_XNONEXPLABELS  0x000200000UL
#define TICKSET_YNONEXPLABELS  0x000400000UL
#define TICKSET_XOFFSET        0x000800000UL
#define TICKSET_YOFFSET        0x001000000UL
#define TICKSET_XINVERT        0x002000000UL
#define TICKSET_YINVERT        0x004000000UL
#define TICKSET_XSCALECHAR     0x008000000UL
#define TICKSET_YSCALECHAR     0x010000000UL
#define TICKSET_XTHICKNESS     0x020000000UL
#define TICKSET_YTHICKNESS     0x040000000UL
#define TICKSET_THICKNESS      0x080000000UL
#define TICKSET_XNOLOGLABEL    0x100000000UL
#define TICKSET_YNOLOGLABEL    0x200000000UL
# else
#define TICKSET_GIVEN          0x000000001ULL
#define TICKSET_XGRID          0x000000002ULL
#define TICKSET_YGRID          0x000000004ULL
#define TICKSET_XLINETYPE      0x000000008ULL
#define TICKSET_YLINETYPE      0x000000010ULL
#define TICKSET_XFRACTION      0x000000020ULL
#define TICKSET_YFRACTION      0x000000040ULL
#define TICKSET_XDIVISIONS     0x000000080ULL
#define TICKSET_YDIVISIONS     0x000000100ULL
#define TICKSET_XSPACING       0x000000200ULL
#define TICKSET_YSPACING       0x000000400ULL
#define TICKSET_XLOGARITHMIC   0x000000800ULL
#define TICKSET_YLOGARITHMIC   0x000001000ULL
#define TICKSET_LINETYPE       0x000002000ULL
#define TICKSET_FRACTION       0x000004000ULL
#define TICKSET_XMODULUS       0x000008000ULL
#define TICKSET_YMODULUS       0x000010000ULL
#define TICKSET_XFACTOR        0x000020000ULL
#define TICKSET_YFACTOR        0x000040000ULL
#define TICKSET_XTIME          0x000080000ULL
#define TICKSET_YTIME          0x000100000ULL
#define TICKSET_XNONEXPLABELS  0x000200000ULL
#define TICKSET_YNONEXPLABELS  0x000400000ULL
#define TICKSET_XOFFSET        0x000800000ULL
#define TICKSET_YOFFSET        0x001000000ULL
#define TICKSET_XINVERT        0x002000000ULL
#define TICKSET_YINVERT        0x004000000ULL
#define TICKSET_XSCALECHAR     0x008000000ULL
#define TICKSET_YSCALECHAR     0x010000000ULL
#define TICKSET_XTHICKNESS     0x020000000ULL
#define TICKSET_YTHICKNESS     0x040000000ULL
#define TICKSET_THICKNESS      0x080000000ULL
#define TICKSET_XNOLOGLABEL    0x100000000ULL
#define TICKSET_YNOLOGLABEL    0x200000000ULL
# endif

#define TICKSET_XOPTS (TICKSET_XGRID+TICKSET_XLINETYPE+TICKSET_XTHICKNESS+TICKSET_XFRACTION+TICKSET_XDIVISIONS+TICKSET_XSPACING+TICKSET_XLOGARITHMIC+TICKSET_LINETYPE+TICKSET_THICKNESS+TICKSET_FRACTION+TICKSET_XMODULUS+TICKSET_XFACTOR+TICKSET_XTIME+TICKSET_XNONEXPLABELS+TICKSET_XOFFSET+TICKSET_XINVERT+TICKSET_XSCALECHAR+TICKSET_XNOLOGLABEL)
#define TICKSET_YOPTS (TICKSET_YGRID+TICKSET_YLINETYPE+TICKSET_YTHICKNESS+TICKSET_YFRACTION+TICKSET_YDIVISIONS+TICKSET_YSPACING+TICKSET_YLOGARITHMIC+TICKSET_LINETYPE+TICKSET_THICKNESS+TICKSET_FRACTION+TICKSET_YMODULUS+TICKSET_YFACTOR+TICKSET_YTIME+TICKSET_YNONEXPLABELS+TICKSET_YOFFSET+TICKSET_YINVERT+TICKSET_YSCALECHAR+TICKSET_YNOLOGLABEL)

typedef struct {
  double spacing[2], modulus[2], fraction[2], factor[2], offset[2], scaleChar[2];
  int32_t divisions[2], linetype[2], thickness[2], labelThickness[2];
  unsigned long long flags;
} TICK_SETTINGS;

#define COLORSET_START         0x00000001U
#define COLORSET_INCREMENT     0x00000002U
#define COLORSET_FINISH        0x00000004U
#define COLORSET_SPECTRAL      0x00000008U
#define COLORSET_RSPECTRAL     0x00000010U
#define COLORSET_USERDEFINED   0x00000020U
#define COLORSET_TEMPERATURE   0x00000040U
#define COLORSET_RTEMPERATURE  0x00000080U
typedef struct {
  unsigned short red[2], green[2], blue[2];
  double increment[3];
  unsigned long flags;
  long num;
} COLOR_SETTINGS;

typedef struct {
  char *text;
  double labelsize;
  double unitsize;
  double xadjust;
  unsigned long flags;
} INTENSITYBAR_SETTINGS;
#define INTENSITYBAR_LABELSIZE_GIVEN  0x0001U
#define INTENSITYBAR_UNITSIZE_GIVEN   0x0002U
#define INTENSITYBAR_XADJUST_GIVEN    0x0004U
#define INTENSITYBAR_TEXT_GIVEN       0x0008U

typedef struct {
  char **enumerate, *editcommand;
  long *enumerateIndex;
  long enumerates;
  int32_t interval, limit;
  double scale;
  unsigned long flags;
} ENUMERATE_SETTINGS ;
#define ENUM_GIVEN             0x00000001U 
#define ENUM_ROTATE            0x00000002U
#define ENUM_SCALEGIVEN        0x00000004U
#define ENUM_INTERVALGIVEN     0x00000008U
#define ENUM_EDITCOMMANDGIVEN  0x00000010U
#define ENUM_LIMITGIVEN        0x00000020U
#define ENUM_ALLTICKS          0x00000040U
/* the following is set internally if -swap is given */
#define ENUM_YSCALES           0x80000000U

#define POINTLABEL_LINETYPE_GIVEN 0x0001U
#define POINTLABEL_VERTICAL 0x0002U
typedef struct {
  unsigned long flags;
  char *name, *editCommand, *justifyMode;
  double scale;
  int32_t linetype;
  int32_t thickness;
} POINTLABEL_SETTINGS;

#define REPLICATE_NUMBER         0x0001U
#define REPLICATE_MATCH          0x0002U
#define REPLICATE_MATCH_NAMES    0x0004U
#define REPLICATE_MATCH_PAGES    0x0008U
#define REPLICATE_MATCH_REQUESTS 0x0010U
#define REPLICATE_MATCH_FILES    0x0020U
#define REPLICATE_SCROLL_MODE    0x0040U

typedef struct {
  unsigned long flags;
  int32_t number;
} REPLICATE_SETTINGS;

#define GROUPBY_FILEINDEX 0
#define GROUPBY_PAGE 1
#define GROUPBY_NAMEINDEX 2
#define GROUPBY_REQUEST 3
#define GROUPBY_TAG 4
#define GROUPBY_FILESTRING 5
#define GROUPBY_NAMESTRING 6
#define GROUPBY_SUBPAGE 7
#define GROUPBY_INAMESTRING 8
#define GROUPING_NAMES 9
typedef struct {
  long rank[GROUPING_NAMES];
  long ranksChanged, groupbyGiven;
} GROUPBY_SETTINGS;


typedef struct {
  double xmin, xmax, ymin, ymax;
  double cxmin, cxmax, cymin, cymax;  /* converted values for autoscale limiting */
  unsigned long flags;
} LIMIT_SETTINGS;
#define LIMIT_XMIN_GIVEN  0x0001U
#define LIMIT_XMAX_GIVEN  0x0002U
#define LIMIT_YMIN_GIVEN  0x0004U
#define LIMIT_YMAX_GIVEN  0x0008U
#define LIMIT_AUTOSCALING 0x0010U

typedef struct {
  double pinUnitSpace[2];
  unsigned long flags;
} ALIGN_SETTINGS;  
#define ALIGNZERO_XCENTER_GIVEN 0x0001
#define ALIGNZERO_YCENTER_GIVEN (ALIGNZERO_XCENTER_GIVEN<<1)
#define ALIGNZERO_XFACTOR_GIVEN (ALIGNZERO_XCENTER_GIVEN<<2)
#define ALIGNZERO_YFACTOR_GIVEN (ALIGNZERO_XFACTOR_GIVEN<<1)
#define ALIGNZERO_PPIN_GIVEN (ALIGNZERO_XFACTOR_GIVEN<<2)
#define ALIGNZERO_QPIN_GIVEN (ALIGNZERO_PPIN_GIVEN<<1)
#define ALIGNZERO_XOPTS (ALIGNZERO_XCENTER_GIVEN|ALIGNZERO_XFACTOR_GIVEN|ALIGNZERO_PPIN_GIVEN)
#define ALIGNZERO_YOPTS (ALIGNZERO_YCENTER_GIVEN|ALIGNZERO_YFACTOR_GIVEN|ALIGNZERO_QPIN_GIVEN)

/* user's specification of scales groups */
typedef struct {
  unsigned long flags;
  char *ID;       /* scales group ID, possibly derived from other data or enter by user */
} SCALES_GROUP_SPEC;
#define SCALESGROUP_ID_GIVEN        0x0001U
#define SCALESGROUP_USE_FILEINDEX   0x0002U
#define SCALESGROUP_USE_FILESTRING  0x0004U
#define SCALESGROUP_USE_NAMEINDEX   0x0008U
#define SCALESGROUP_USE_NAMESTRING  0x0010U
#define SCALESGROUP_USE_PAGE        0x0020U
#define SCALESGROUP_USE_REQUEST     0x0040U
#define SCALESGROUP_USE_INAMESTRING 0x0080U
#define SCALESGROUP_USE_UNITS       0x0100U
#define SCALESGROUP_OTHER_SIDE      0x0200U
#define RESERVED_SCALESGROUP_ID "\001\002\003\004\005\006\007\010"

/* used for collecting and sorting scales group data for each ID */
typedef struct {
  long index;              /* used as a unique identifier of the group */
  long panel;              /* panel to which this group applies */
  char *ID;                /* user's ID.  Combined with panel, has 1-to-1 correspondence with index */
  /* These are used as pointers to data in the plot request structure.  For a given
   * scales group, each will point to the data from the first plot request that
   * has something specified.  I store the indices because it makes it possible
   * to determine the first request as I construct the scales groups.
   */
  long tickSettingsRequestIndex, subtickSettingsRequestIndex;
  long enumSettingsRequestIndex, labelSpecRequestIndex, alignZeroRequestIndex;
  unsigned long flags;  /* or'ing of all plot request flags for this group */
  unsigned long mode;   /* or'ing of all -mode flags for this group */
  double limit[2];      /* limit[0(1)] is lower (upper) limit for plane */
  unsigned long limitFlags;
} SCALES_GROUP_DATA;
#define LIMIT_COUNTED  0x0001UL
#define LIMIT0_SET     0x0002UL
#define LIMIT1_SET     0x0004UL

#define X_EXCLUDE 0
#define Y_EXCLUDE 1

typedef struct {
  char **excludeName;
  short *was_wildExclude;
  long excludeNames;
} EXCLUDE_SPEC;

/* values for data_class in plot request structure */
#define COLUMN_DATA 0
#define PARAMETER_DATA 1
#define ARRAY_DATA 2

/* Flags for -usePages option */
#define USEPAGES_START_GIVEN  0x0001U
#define USEPAGES_END_GIVEN    0x0002U
#define USEPAGES_INTERVAL_GIVEN 0x0004U

typedef struct {
  char **filename, **description_text;
  char **xname, **yname, **x1name, **y1name;
  short *was_wildname;
  long datanames, filenames, data_class,frompage,topage;
  /* -usePages selection */
  unsigned long usePagesFlags;
  long usePagesStart, usePagesEnd, usePagesInterval;
  EXCLUDE_SPEC x_exclude_spec;
  EXCLUDE_SPEC y_exclude_spec;
  GRAPHIC_SPEC graphic;
  long linetype_default, linethickness_default;
  int32_t axesLinetype, axesLinethickness;
  long global_thickness_default;
  ZOOM_SPEC zoom;
  RANGE_SPEC range;
  LABEL_SPEC label[4];
  STRING_LABEL_SPEC *string_label;
  long string_labels;
  DRAW_LINE_SPEC *drawLineSpec;
  long drawLineSpecs;
  LEGEND_SPEC legend;
  OVERLAY_SPEC overlay;
  unsigned long flags, mode, mplflags, nameScanFlags, joinScaleFlags;
  char *filenamesOnToplineEditCmd;
  SPLIT_SPEC split;
  FILTER_DEFINITION **filter;
  TIME_FILTER_DEFINITION **time_filter;
  MATCH_DEFINITION **match;
  LIMIT_SETTINGS limit;
  ALIGN_SETTINGS alignSettings;
  long filters, matches,time_filters;
  long sparse_interval, sparse_offset, clip_head, clip_tail;
  long presparse_interval, presparse_offset;
  int32_t separate_group_size;
  unsigned long separate_flags;
  /*double user_tag, xgap, ygap;*/
  char *user_tag; /*change the type of user_tag from double to string (shang) */
  double xgap, ygap;
  SCALES_GROUP_SPEC scalesGroupSpec[2];
  char *tag_parameter, *offset_parameter[2], *factor_parameter[2];
  double sample_fraction, stagger_amount[2], factor[2], offset[2];
  unsigned long stagger_flags, factor_flags, offset_flags;
  double mapping[4], pspace[4], lspace[4], aspect_ratio;
  double labelsize_fraction, xorig, yorig;
  double dither[2];
  TICK_SETTINGS tick_settings, subtick_settings;
  ENUMERATE_SETTINGS enumerate_settings;
  POINTLABEL_SETTINGS pointLabelSettings;
  REPLICATE_SETTINGS replicateSettings;
  COLOR_SETTINGS color_settings;
  INTENSITYBAR_SETTINGS intensityBar_settings;
} PLOT_REQUEST;
/* These flags are used with the 'flags' member of PLOT_REQUEST */
#define PLREQ_OMNIPRESENT     0x00000001
#define PLREQ_NEWPANEL        0x00000002
#define PLREQ_SEVER           0x00000004
#define PLREQ_INVERTCLIP      0x00000008
#define PLREQ_FNONTOPLINE     0x00000010
#define PLREQ_VPRINTDOWN      0x00000020
#define PLREQ_TOPTITLE        0x00000040
#define PLREQ_DATESTAMP       0x00000080
#define PLREQ_NOLABELS        0x00000100
#define PLREQ_NOBORDER        0x00000200
#define PLREQ_SEPARATE        0x00000400
#define PLREQ_SWAP            0x00000800
#define PLREQ_TRANSPOSE       0x00001000
#define PLREQ_ENDPANEL        0x00002000
#define PLREQ_YLONTOPLINE     0x00004000
#define PLREQ_SAMESCALEGLOBAL 0x00008000
#define PLREQ_LSPACE_GIVEN    0x00010000
#define PLREQ_XGAP            0x00020000
#define PLREQ_YGAP            0x00040000
#define PLREQ_SAMESCALEX      0x00080000
#define PLREQ_SAMESCALEY      0x00100000
#define PLREQ_UNSUPPRESSX     0x00200000
#define PLREQ_UNSUPPRESSY     0x00400000
#define PLREQ_NOSCALESX       0x00800000
#define PLREQ_NOSCALESY       0x01000000
#define PLREQ_GRIDX           0x02000000
#define PLREQ_GRIDY           0x04000000
#define PLREQ_AXESX           0x08000000
#define PLREQ_AXESY           0x10000000
#define PLREQ_NEXTPAGE        0x20000000
#define PLREQ_THICKNESS       0x40000000    
#define PLREQ_OMNIFIRST       0x80000000

#define PLREQ_XFLAGS (PLREQ_XGAP+PLREQ_SAMESCALEX+PLREQ_UNSUPPRESSX+PLREQ_NOSCALESX+PLREQ_GRIDX+PLREQ_AXESX)
#define PLREQ_YFLAGS (PLREQ_YGAP+PLREQ_SAMESCALEY+PLREQ_UNSUPPRESSY+PLREQ_NOSCALESY+PLREQ_GRIDY+PLREQ_AXESY)
#if !((PLREQ_X_BITS<<1)==PLREQ_Y_BITS) 
#error "PLREQ_X_BITS and PLREQ_Y_BITS are inconsistent!"
#endif
#if PLREQ_AXESY<PLREQ_XGAP
#error "PLREQ flags are using too many bits!"
#endif

/* These are used with the 'mplflags' member of PLOT_REQUEST */
#define MPLFILE           0x001
#define MPLFILE_NOTITLE   0x002
#define MPLFILE_NOTOPLINE 0x004

/* These are used with the 'mode' member of PLOT_REQUEST */
#define MODE_LOG           0x00000001UL
#define MODE_SPECIALSCALES 0x00000002UL
#define MODE_NORMALIZE     0x00000004UL
#define MODE_OFFSET        0x00000008UL
#define MODE_MEANCENTER    0x00000010UL
#define MODE_CENTER        0x00000020UL
#define MODE_COFFSET       0x00000040UL
#define MODE_EOFFSET       0x00000080UL
#define MODE_FRACDEV       0x00000100UL
#define MODE_AUTOLOG       0x00000200UL
#define MODE_ABSOLUTE      0x00000400UL
#define MODE_X_LOG MODE_LOG
#define MODE_X_SPECIALSCALES  MODE_SPECIALSCALES
#define MODE_X_NORMALIZE MODE_NORMALIZE
#define MODE_X_OFFSET MODE_OFFSET
#define MODE_X_COFFSET MODE_COFFSET
#define MODE_X_EOFFSET MODE_EOFFSET
#define MODE_X_MEANCENTER MODE_MEANCENTER
#define MODE_X_CENTER MODE_CENTER
#define MODE_X_FRACDEV MODE_FRACDEV
#define MODE_X_AUTOLOG MODE_AUTOLOG
#define MODE_X_ABSOLUTE MODE_ABSOLUTE
#define MODE_X_BITS 0x0000ffff
#define MODE_Y_LOG (MODE_LOG<<16)
#define MODE_Y_SPECIALSCALES (MODE_X_SPECIALSCALES<<16)
#define MODE_Y_NORMALIZE (MODE_X_NORMALIZE<<16)
#define MODE_Y_OFFSET (MODE_X_OFFSET<<16)
#define MODE_Y_COFFSET (MODE_X_COFFSET<<16)
#define MODE_Y_EOFFSET (MODE_X_EOFFSET<<16)
#define MODE_Y_MEANCENTER (MODE_X_MEANCENTER<<16)
#define MODE_Y_CENTER (MODE_X_CENTER<<16)
#define MODE_Y_FRACDEV (MODE_FRACDEV<<16)
#define MODE_Y_AUTOLOG (MODE_AUTOLOG<<16)
#define MODE_Y_ABSOLUTE (MODE_ABSOLUTE<<16)
#define MODE_Y_BITS (MODE_X_BITS<<16)

#define NAMESCAN_ALL   0x00000001U
#define NAMESCAN_FIRST 0x00000002U

#define SEPARATE_GROUPSOF      0x00000001
#define SEPARATE_FILEINDEX     0x00000002
#define SEPARATE_NAMEINDEX     0x00000004
#define SEPARATE_FILESTRING    0x00000008
#define SEPARATE_NAMESTRING    0x00000010
#define SEPARATE_PAGE          0x00000020
#define SEPARATE_REQUEST       0x00000040
#define SEPARATE_TAG           0x00000080
#define SEPARATE_SUBPAGE       0x00000100
#define SEPARATE_INAMESTRING   0x00000200

#define STAGGER_XINC_GIVEN     0x00000001
#define STAGGER_YINC_GIVEN     0x00000002
#define STAGGER_FILES          0x00000004
#define STAGGER_DATANAMES      0x00000008

#define FACTOR_XMULT_GIVEN      0x00000001
#define FACTOR_YMULT_GIVEN      0x00000002
#define FACTOR_XPARAMETER_GIVEN 0x00000004
#define FACTOR_YPARAMETER_GIVEN 0x00000008
#define FACTOR_XINVERT_GIVEN    0x00000010
#define FACTOR_YINVERT_GIVEN    0x00000020
#define FACTOR_XBEFORELOG_GIVEN 0x00000040
#define FACTOR_YBEFORELOG_GIVEN 0x00000080

#define OFFSET_XCHANGE_GIVEN     0x00000001
#define OFFSET_YCHANGE_GIVEN     0x00000002
#define OFFSET_XPARAMETER_GIVEN  0x00000004
#define OFFSET_YPARAMETER_GIVEN  0x00000008
#define OFFSET_XINVERT_GIVEN     0x00000010
#define OFFSET_YINVERT_GIVEN     0x00000020
#define OFFSET_XBEFORELOG_GIVEN  0x00000040
#define OFFSET_YBEFORELOG_GIVEN  0x00000080

#define PLREQ_JOINSCALE_X     0x0001U
#define PLREQ_JOINSCALE_Y     0x0002U
#define JOINSCALE_NOTOP 0x0004U
#define JOINSCALE_NOBOT 0x0008U

typedef struct {
  short replicant, scrollParent, enumPlane;
  double *x, *y, *x1, *y1;
  char **enumerate, **pointLabel;
  double *split_data;
  double split_min,split_max;
  DATA_INFO info[2]; /* x, y */
  long points, pointsStored;   /* If scrollParent is non-zero, then points and pointsStored may be different */
  GRAPHIC_SPEC graphic;
  char *legend, *label[4];
  STRING_LABEL_SPEC *string_label;
  long string_labels;
  DRAW_LINE_SPEC *drawLineSpec;
  long drawLineSpecs;
  double limit[4];     /* (effective) data limits (xmin, xmax, ymin, ymax) */
  double offset[2], factor[2];
  char *tag; /*change the type of tag from double to string (shang) */
  long datapage, subpage, plotpanel;
  long request_index, file_index, dataname_index;
  long scalesGroupIndex[2], globalScalesGroupIndex[2];
  char *scalesGroupID[2];
  /* The virtual indices differ from the plain ones when datasets are replicated. 
   * They are used for grouping functions and panel assignments.  In those contexts,
   * the replicated data set looks as if it originated in a different request,
   * dataname, or file than it really did. */
  long virtual_request_index, virtual_dataname_index, virtual_file_index;
  short fromOmni;
  long omniSourceDataset;
  double xOrig0, yOrig0;
  double xOrig1, yOrig1;
} PLOT_DATA;

/* This holds label and scale information for a single scale of
 * a panel.
 */
typedef struct {
  unsigned long flags;
  char *label;
  long scaleNumber;
  double scale, offset;
  long thickness, linetype;
} SCALE_LABEL_INFO;
#define SCALE_LABEL_DONE       0x01UL
#define SCALE_LABEL_USED       0x02UL
#define SCALE_LABEL_SCALEGIVEN 0x04UL
#define SCALE_LABEL_OFFSETGIVEN 0x08UL
#define SCALE_LABEL_THICKNESSGIVEN 0x10UL
#define SCALE_LABEL_LINETYPEGIVEN 0x20UL

typedef struct {
  /* This arrays has a slot for every scales group that is defined, even those that aren't used on the panel.
   */
  long *scalesGroupIndex[2];  /* indices of scale groups used by this panel for each plane */
  long scalesUsed[2];  /* actual number of scales on this panel */
  long scalesOtherSide[2]; /* of which, number on the "other side" */
  double pspace[4];    /* the 'traditional' pspace, which maps to user's plot region */
  double workSpace[4];  /* the boundary around this plot, including labels and
                         * legends, in same coordinates as the pspace.  Is 0, 1, 0, 1
                         * if there is only one panel per page.
                         */
  char *title[2];  /* generated title, topline */
  long ix, iy, last_panel_on_page, last_panel_in_row, last_panel_in_column;
  PLOT_DATA **dataset;
  long datasets;
  /* These are derived from the requests that the applicable datasets
   * belong to: 
   */
  unsigned long flags, mode, joinScaleFlags;  /* or'ing of all applicable request flags */
  long linetypeDefault, linethicknessDefault, axesLinetype, axesLinethickness;
  double lspace[4], labelSize;
  LEGEND_SPEC legend;
  LABEL_SPEC titleSpec[2]; /* user's requested title, topline */
} PLOT_PANEL ;

typedef struct {
  short autosize;
  double all;
  double legend;
  double xlabel;
  double ylabel;
  double xticks;
  double yticks;
  double title;
  double topline;
} FONT_SIZE;

typedef struct {
  char *outputfile, *device, **deviceArgv, *font;
  FONT_SIZE fontsize[1];
  long deviceArgc;
  GROUPBY_SETTINGS groupBy;
  /* unsigned long flags; */
  long layout[2];
  int32_t maxPanelsPerPage;
  PLOT_PANEL *panel;
  long panels;
  PLOT_DATA *dataset;
  long datasets;
  PLOT_REQUEST *plot_request;
  long plot_requests;
  long scalesGroups[2], globalScalesGroups[2];
  SCALES_GROUP_DATA *scalesGroupData[2], *globalScalesGroupData[2];
  SCALE_LABEL_INFO *scaleLabelInfo[2];  /* x and y info for each scales group */
  CONVERSION_DEFINITION **conversion;
  long conversions;
} PLOT_SPEC;

void determine_labels(PLOT_SPEC *plspec);
void determine_panel_labels(PLOT_SPEC *plspec, long panel_index);
void determine_dataset_labels(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset);
void determine_dataset_tag(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset);
void determine_dataset_drawlines(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset);
void determine_dataset_scalesGroup(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset);

void record_column_information(DATA_INFO *info, SDDS_TABLE *table, char *name);
void record_parameter_information(DATA_INFO *info, SDDS_TABLE *table, char *name);
void record_array_information(DATA_INFO *info, SDDS_TABLE *table, char *name);

long transferNameToLists(char ***yname, char ***xname, char ***x1name, char ***y1name, short **was_wildname, 
                         long names, char *yname1, char *xname1, char *x1name1, char *y1name1, short wildcode);
long transferNamesToLists(char ***yname, char ***xname, char ***x1name, char ***y1name, short **was_wildname, long names,
                          char **newname, long newnames, char *xname1, char *x1name1, char *y1name1, short wildcode);
long transferNamesToLists1(char ***yname, char ***xname, char ***x1name, char ***y1name, 
                           short **was_wildname, long names,
                           char **newname, long newnames, char *yname1, char *x1name1, 
			   char *y1name1, short wildcode);


void determine_panel_pspaces(PLOT_SPEC *plspec);
void determine_limits(PLOT_SPEC *plspec);
void recalculate_small_limits(double *map0, double *map1);

long determine_zoomed_limits(double *mapping, ZOOM_SPEC *zoom, long plane);
void restore_fixed_limits(long offset, PLOT_SPEC *plspec, long request);
void equalize_dataset_ranges(PLOT_SPEC *plspec, long request, long dimension);
void determine_panel_limits(PLOT_SPEC *plspec, long panel);
void impose_range_limits(double *mapping, RANGE_SPEC *range, unsigned long logMode);

void expand_interval(double *lower, double *upper, double fraction);
void determine_dataset_range(double *mapping, double *limit, PLOT_DATA *dataset, LIMIT_SETTINGS *limitSettings);
void unsuppress_zero_limits(double *lo, double *hi);

long graphic_AP1(GRAPHIC_SPEC *graphic_spec, long element, char **item, long items);
long plotnames_AP1(PLOT_SPEC *plotspec, char **item, long items, char *plotnames_usage, long class);
void add_plot_request(PLOT_SPEC *plspec);
long plotlabel_AP(PLOT_SPEC *plotspec, long label_index, char **item, long items);
long keepnames_AP1(PLOT_SPEC *plotspec);
long keepfilenames_AP1(PLOT_SPEC *plotspec);
long add_filename(PLOT_SPEC *plotspec, char *filename);
long count_chars(char *string, char c);
void show_plot_request(PLOT_REQUEST *plreq, long index, FILE *fp);
void show_plot_spec(PLOT_SPEC *plspec, FILE *fp);


void resolve_omnipresent_references(PLOT_SPEC *plspec);

int plot_data_arrays(double **x, double **y, double **sigma_x, double **sigma_y, double *xgap,
                     double *ygap, long *n,
                     GRAPHIC_SPEC **graphic, long linetype_default,
                     char **legend, double legendScale,
                     OVERLAY_SPEC **overlay, long np, unsigned long flags, unsigned long mode,
                     char *xlab, char *ylab, char *title, char *top_line, double *limit,
                     double *pspace, double *lspace, double aspect, 
                     TICK_SETTINGS *tick_settings, TICK_SETTINGS *subtick_settings, 
                     double labelsize_fraction, ENUMERATE_SETTINGS *enum_settings,
                     long axesLinetype, long axesLinethickness
                     );
void sddsplotDataArrays(double *xn, double *yn, double *sxn, double *syn, long npts,
                        double xgap, double ygap, GRAPHIC_SPEC *graphic, unsigned long flags,
                        double *pspace);
void draw_sddsplot_strings(PLOT_DATA *dataset, long linetypeDefault, double *limit);
void drawSddsplotLines(PLOT_DATA *dataset, long linetypeDefault, 
                       double *limit);
void make_legend(char **legend, GRAPHIC_SPEC **graphic, long n, double legendScale, long legendThickness, unsigned long *legendFlags, long do_box);
void labelDataPoints(double *x, double *y, long n, char **pointLabel, POINTLABEL_SETTINGS *settings, long linetypeDefault);
long countNonblankStrings(char **string, long strings);
void plot_sddsplot_data(PLOT_SPEC *plspec, short initDevice);
void read_sddsplot_data(PLOT_SPEC *plspec);
void assign_enumerate_values(PLOT_SPEC *plspec);
void columnbin_sddsplot_data(PLOT_SPEC *plspec);
PLOT_DATA *add_dataset_slots(PLOT_DATA *dataset, long datasets, long datanames);
void append_to_dataset(PLOT_DATA *dataset, double *x, char **enumerate, double *y, double *x1, double *y1, 
                       double *split, char **pointLabel, long points);
void determine_panel_assignments(PLOT_SPEC *plspec);
long datanameCompare(PLOT_REQUEST *plreq1, long index1, PLOT_REQUEST *plreq2, long index2, long invertString);
void perform_dataset_sort(PLOT_SPEC *plspec);
void assign_scales_groups(PLOT_SPEC *plspec);
void determine_graphic_assignments(PLOT_SPEC *plspec);
unsigned long translate_to_plotcode(GRAPHIC_SPEC graphic);
void perform_dataset_conversions(PLOT_SPEC *plspec);
double perform_normalization(double *data, double *data1, long points);
void perform_dataset_transpositions(PLOT_SPEC *plspec);
long perform_sddsplot_filtering(SDDS_TABLE *table, FILTER_DEFINITION **filter, long filters);
long perform_sddsplot_time_filtering(SDDS_TABLE *table, TIME_FILTER_DEFINITION **time_filter, long time_filters);
long perform_sddsplot_matching(SDDS_TABLE *table, MATCH_DEFINITION **match, long matches);
long remove_nonpositive_data(double *data1, double *data2, double *data3, double *data4, double *split, 
                             char **enumerate, char **pointLabel, long n);
long remove_invalid_data(double *data1, double *data2, double *data3, double *data4, double *data5, 
                         char **enumerate, char **pointLabel, long n);
void resolve_wildcard_datanames(PLOT_SPEC *plspec);
void perform_dataname_swapping(PLOT_SPEC *plspec);
void perform_arrow_autoscaling(PLOT_SPEC *plspec);
void replicate_datasets(PLOT_SPEC *plspec);
void ReplicateDataset(PLOT_DATA *target, PLOT_DATA *source, long offset);

long find_datanames_absent(SDDS_TABLE *table, short **dataname_absent, long *datanames_absent,
                           char **yname, char **y1name, char **xname, char **x1name,
                           long candidate_names, long data_class,
                           char **xExcludeName, short *was_wildexExclude,long xExcludeNames,
                           char **yExcludeName, short *was_wildeyExclude,long yExcludeNames);
void determine_dataset_legends(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset, long first_file);
void determine_dataset_strings(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset);
void determine_dataset_offsets(PLOT_SPEC *plspec, SDDS_TABLE *table, PLOT_DATA *dataset);
void read_mpl_dataset(PLOT_DATA *dataset, char *file, long sample_interval, long ireq, long ifile, long flags);
long extract_name_and_units(DATA_INFO *info, char *label);
long adjust_sddsplot_overlay_data(double *x, double *u, long n_pts, double range, double min, 
                                  OVERLAY_SPEC *overlay, long shift);
void sparse_sample_clip(PLOT_SPEC *plspec);
long check_for_split(SDDS_TABLE *table, SPLIT_SPEC *split, long datapage,long dataclass);
void make_enumerated_xscale(ENUMERATE_SETTINGS *enum_settings);
void make_enumerated_yscale(ENUMERATE_SETTINGS *enum_settings);

void setDeviceArgv(char **argv, long argc);
char **getDeviceArgv(long *argc);
int countArguments(char *s);
void shorten_filename(char *s);
